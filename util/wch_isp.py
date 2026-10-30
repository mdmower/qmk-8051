#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Flash a CH555 through the bootloader that WCH puts in the chip.

The WCH bootloader is at 0xF400, above the application and the data flash, and appears on USB as 4348:55e0. This
script writes 0x0000-0xEFFF through it, everything below the data flash. Nothing in the range is protected: a
keyboard's own bootloader at 0x0000 is overwritten together with the firmware. Keep a full dump of the flash from
before the first flash through the WCH bootloader. Flashing that dump is the only way to restore the original
bootloader.

Three things start the WCH bootloader. The firmware can call it, as QMK does for Fn+Esc and for Esc held while the
keyboard is plugged in. Or, the chip is powered up with P1.5 held low. Or, the chip starts the bootloader at every
power-up while the first two bytes of its flash read 00 00, as they do after an erase of the first page. In the last
case, a flash that stopped halfway can be repeated: plug the keyboard in again and run the same flash command.

  python3 -I util/wch_isp.py status                          # the 4348:55e0 devices that are connected
  python3 -I util/wch_isp.py check IMAGE                     # the image checks alone, no device needed
  python3 -I util/wch_isp.py flash IMAGE [--poll] [--send]   # erase, write, verify, then start the firmware
  python3 -I util/wch_isp.py verify FILE                     # compare FILE with the flash; writes nothing
  python3 -I util/wch_isp.py config                          # the chip's UID and bootloader version

IMAGE is a firmware image linked at 0x0000 and is at most 0xF000 bytes long, or a full 64 KB dump of the chip, of
which 0x0000-0xEFFF is written back. Without --send, a flash is a dry run that prints what it would do. With --poll,
the script waits up to 15 s for the bootloader, so you can start the command and then restart the keyboard to the WCH
bootloader.

A flash erases 0x0000-0xEFFF, writes and verifies the image from its second page on, then writes and verifies the
first page, and resets the chip. The first page comes last so that the bootloader keeps starting at power-up until the
whole image is in place. After the reset the script waits for the firmware to enumerate at the same USB port and
prints its ID. --expect VID:PID makes it wait for that ID in particular.

On some chips, the bootloader cannot read flash after a call from the firmware. The reply to IDENTIFY signals this
limitation. In these cases, the script erases the flash, asks you to unplug the keyboard and plug it in again, and
then writes and verifies in the session that follows the power-up.

The script refuses an image if: 1) it does not start with a jump, 2) it has an interrupt vector that is neither an
LJMP into the image nor RETI, or 3) it has no clr P1.5, lcall 0xF400 sequence. --force flashes such an image anyway.
The bootloader does not identify the keyboard model, so make sure the image you intend to flash is for the correct
board.

The script refuses to run if multiple 4348:55e0 devices are connected.

Linux only and only the Python standard library is needed. The script reaches the bootloader through usbfs; install
the necessary rules: util/udev/70-wch-isp.rules
"""
import sys
if sys.platform == "win32":
    raise SystemExit("wch_isp.py runs on Linux only: it reaches the bootloader through usbfs and has no Windows "
                     "backend")
import argparse
import ctypes
import errno
import fcntl
import glob
import hashlib
import os
import struct
import time

ISP_ID = (0x4348, 0x55e0)
EP_OUT = 0x02
BULK = struct.Struct("@IIIP")  # struct usbdevfs_bulktransfer
USBDEVFS_BULK = (3 << 30) | (BULK.size << 16) | (ord("U") << 8) | 2
USBDEVFS_CLAIMINTERFACE = (2 << 30) | (4 << 16) | (ord("U") << 8) | 15
USBDEVFS_RELEASEINTERFACE = (2 << 30) | (4 << 16) | (ord("U") << 8) | 16
MCU_ISP = b"MCU ISP & WCH.CN"

CHIP_ID = 0x55
CODE_SIZE = 0xf000                       # 0x0000-0xEFFF; the data flash starts at 0xF000, the bootloader at 0xF400
FLASH_SIZE = 0x10000
PAGE = 64
CHUNK = 56                               # data bytes per packet: 64 - 3 (header) - 4 (address) - 1
ERASE_KB = CODE_SIZE // 1024             # 60
VECTORS = range(0x0003, 0x005c, 8)       # the CH555's interrupt vectors
LJMP, AJMP_MASK, AJMP, SJMP, RETI = 0x02, 0x1f, 0x01, 0x80, 0x32
ISP_CALL = bytes.fromhex("c29512f400")   # clr P1.5; lcall 0xF400: the bootloader stays in USB mode only with P1.5 low
BOOT_LOAD = 0x20                         # bBOOT_LOAD in config byte 4 of the READ_CONFIG reply

CMD_TIMEOUT_MS = 2000
ERASE_TIMEOUT_MS = 30000
POLL_TIMEOUT = 15                        # seconds to wait for the bootloader with --poll
REPLUG_TIMEOUT = 120                     # seconds to wait for the replug after the erase
APP_TIMEOUT = 30                         # seconds to wait for the firmware after the reset


class IspError(Exception):
    pass


# --- images ---------------------------------------------------------------------------------------------------
def read_file(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError as e:
        raise SystemExit("%s: %s" % (path, e.strerror or e))


def is_jump(op):
    return op == LJMP or op == SJMP or op & AJMP_MASK == AJMP


def check_image(img):
    """The problems of a firmware image linked at 0x0000, as a list of strings."""
    if not PAGE <= len(img) <= CODE_SIZE:
        return ["%d bytes: an image holds 64 to %d bytes (0x0000-0x%04X)" % (len(img), CODE_SIZE, CODE_SIZE - 1)]
    bad = []
    if not is_jump(img[0]):
        bad.append("0x0000 holds %s, not a jump" % img[:3].hex(" "))
    for v in VECTORS:
        if img[v] == RETI:
            continue
        if img[v] != LJMP or struct.unpack_from(">H", img, v + 1)[0] >= len(img):
            bad.append("vector 0x%04X holds %s, neither an LJMP into the image nor RETI (an image for a "
                       "bootloader at 0x0000?)" % (v, img[v:v + 3].hex(" ")))
            break
    if ISP_CALL not in img:
        bad.append("no call of the bootloader (%s: clr P1.5; lcall 0xF400): once this image runs, only P1.5 held "
                   "low at power-up starts the bootloader" % ISP_CALL.hex(" "))
    return bad


def load(path, force=False):
    """The bytes to write (whole pages) and a description of the file. Exit when the file fails the checks."""
    img = read_file(path)
    if len(img) == FLASH_SIZE:
        kind = "a 64 KB dump"
        bad = [] if is_jump(img[0]) else ["0x0000 holds %s, not a jump%s" % (
            img[:3].hex(" "), ": with 00 00 there the chip starts the bootloader at every power-up"
            if img[:2] == b"\x00\x00" else "")]
    else:
        kind = "a firmware image"
        bad = check_image(img)
    if bad and not force:
        raise SystemExit("%s: not accepted:\n  %s\n--force flashes it anyway." % (path, "\n  ".join(bad)))
    if bad:
        print("WARNING, flashing with --force:\n  %s" % "\n  ".join(bad))
    data = img[:CODE_SIZE]
    data += bytes(-len(data) % PAGE)                     # erased flash reads 00
    print("file    : %s, %d bytes, sha256 %s" % (path, len(img), hashlib.sha256(img).hexdigest()))
    print("          %s; writes 0x0000-0x%04X" % (kind, len(data) - 1))
    return data


def usb_id_in(img):
    """(vid, pid) of the first USB device descriptor in img, or None."""
    i = img.find(b"\x12\x01")
    while i >= 0 and i + 18 <= len(img):
        if img[i + 3] in (1, 2) and img[i + 7] in (8, 16, 32, 64):     # bcdUSB 1.x or 2.x, bMaxPacketSize0
            return struct.unpack_from("<HH", img, i + 8)
        i = img.find(b"\x12\x01", i + 1)
    return None


# --- devices --------------------------------------------------------------------------------------------------
def usb_devices(ids=None):
    """[(vid, pid, port, sysfs dir)] of the USB devices with an ID in ids, or of all of them."""
    hits = []
    for d in glob.glob("/sys/bus/usb/devices/*"):
        try:
            vidpid = (int(open(d + "/idVendor").read(), 16), int(open(d + "/idProduct").read(), 16))
        except (OSError, ValueError):
            continue
        if ids is None or vidpid in ids:
            hits.append((vidpid[0], vidpid[1], os.path.basename(d), d))
    return sorted(hits, key=lambda h: h[2])


def the_isp():
    """sysfs dir of the one 4348:55e0 device, or None. Exit if there are several."""
    hits = usb_devices({ISP_ID})
    if len(hits) > 1:
        raise SystemExit("%d WCH bootloaders found (USB %s); refusing. Unplug all but the one to use."
                         % (len(hits), ", ".join(h[2] for h in hits)))
    return hits[0][3] if hits else None


def wait_at_port(port, ids, timeout):
    """(vid, pid, port, sysfs dir) of the device at port once it has an ID in ids, or any ID but the
    bootloader's if ids is None; None after timeout seconds."""
    deadline = time.monotonic() + timeout
    while True:
        hits = [h for h in usb_devices(ids) if h[2] == port and (ids or (h[0], h[1]) != ISP_ID)]
        if hits:
            return hits[0]
        if time.monotonic() >= deadline:
            return None
        time.sleep(0.25)


def bulk(fd, ep, data=None, length=64, timeout=CMD_TIMEOUT_MS):
    buf = ctypes.create_string_buffer(data, len(data)) if data is not None else ctypes.create_string_buffer(length)
    n = len(data) if data is not None else length
    # mutable: with an immutable buffer, ioctl() returns the copied buffer instead of the byte count
    r = fcntl.ioctl(fd, USBDEVFS_BULK, bytearray(BULK.pack(ep, n, timeout, ctypes.addressof(buf))))
    return buf.raw[:r]


class Isp:
    """Interface 0 of the 4348:55e0 device through usbfs.

    A request is the command byte, the payload length as 16 bits little-endian, and the payload. A reply is the
    command byte, one byte without meaning, the length as 16 bits, a status byte, and data. Requests go to bulk
    endpoint 2; the bulk IN endpoint is read from sysfs."""

    def __init__(self, d):
        deadline = time.monotonic() + 5                  # the interface appears after the device
        while True:
            ep_in = [int(os.path.basename(e)[3:], 16) for e in glob.glob(d + "/%s:1.0/ep_*" % os.path.basename(d))]
            ep_in = [e for e in ep_in if e & 0x80]
            if len(ep_in) == 1:
                break
            if time.monotonic() >= deadline:
                raise IspError("expected one IN endpoint on interface 0 of the bootloader, found %s" % ep_in)
            time.sleep(0.1)
        self.ep_in = ep_in[0]
        self.port = os.path.basename(d)
        node = "/dev/bus/usb/%03d/%03d" % (int(open(d + "/busnum").read()), int(open(d + "/devnum").read()))
        deadline = time.monotonic() + 10                 # udev sets the permissions of a new node
        while not os.access(node, os.R_OK | os.W_OK):
            if time.monotonic() >= deadline:
                raise IspError("no read/write access to %s (is util/udev/70-wch-isp.rules installed?)" % node)
            time.sleep(0.1)
        print("device  : WCH bootloader at USB %s (%s), bcdDevice %s, bulk OUT 0x%02x, IN 0x%02x" % (
            self.port, node, open(d + "/bcdDevice").read().strip(), EP_OUT, self.ep_in), flush=True)
        self.fd = os.open(node, os.O_RDWR)
        try:
            fcntl.ioctl(self.fd, USBDEVFS_CLAIMINTERFACE, bytearray(struct.pack("I", 0)))
        except OSError:
            os.close(self.fd)
            raise
        self.key = None

    def close(self):
        if self.fd is None:
            return
        try:
            fcntl.ioctl(self.fd, USBDEVFS_RELEASEINTERFACE, bytearray(struct.pack("I", 0)))
        except OSError:                                  # the device has left the bus
            pass
        os.close(self.fd)
        self.fd = None

    def xfer(self, cmd, payload, timeout=CMD_TIMEOUT_MS, show=False):
        req = bytes([cmd]) + struct.pack("<H", len(payload)) + payload
        if show:
            print("=> %s  %s" % (req[:3].hex(" "), req[3:].hex(" ")))
        try:
            bulk(self.fd, EP_OUT, req, timeout=timeout)
            rep = bulk(self.fd, self.ep_in, timeout=timeout)
        except OSError as e:
            if e.errno == errno.ETIMEDOUT:
                raise IspError("no reply to %02X within %d ms" % (cmd, timeout))
            raise
        if show:
            print("<= %s  %s" % (rep[:4].hex(" "), rep[4:].hex(" ")))
        if len(rep) < 5 or rep[0] != cmd:
            raise IspError("reply %s to %02X" % (rep.hex(" "), cmd))
        return rep

    def read_config(self):
        """READ_CONFIG (A7) with mask 1f: (bBOOT_LOAD, BTVER, UID). bBOOT_LOAD is set after a power-up entry and
        clear after a call from the firmware. The command also sets bCODE_WE, which the writes need."""
        rep = self.xfer(0xA7, bytes([0x1f, 0]))
        if rep[4] != 0x1f or len(rep) < 30:
            raise IspError("READ_CONFIG reply %s" % rep.hex(" "))
        return bool(rep[10] & BOOT_LOAD), rep[18:22], rep[22:30]

    def identify(self):
        """IDENTIFY (A1). The bootloader compares the string with a copy in its own code by MOVC, so the command
        fails (f1) in a session in which the bootloader reads no flash, and VERIFY would fail as well."""
        rep = self.xfer(0xA1, bytes([CHIP_ID, 0x11]) + MCU_ISP)
        return rep[4] == CHIP_ID and rep[5] == 0x11

    def set_key(self, uid):
        """ISP_KEY (A3) with a seed of 30 equal bytes, the sum of the 8 UID bytes. The key that follows from it
        is 00 x 7, 55, and the reply is the sum of the key bytes."""
        rep = self.xfer(0xA3, bytes([sum(uid) & 0xff]) * 30)
        if rep[4] != CHIP_ID:
            raise IspError("ISP_KEY reply %02X, not the key sum %02X" % (rep[4], CHIP_ID))
        self.key = bytes(7) + bytes([CHIP_ID])

    def erase(self):
        """ERASE (A4) of ERASE_KB kilobytes in 64-byte pages from 0x0000 upward. The bootloader accepts 8 KB or
        more and has no upper bound; 60 KB ends at 0xEFFF. ERASE is the only command that clears the write lock
        the bootloader sets at its start, so every session that writes begins with it."""
        rep = self.xfer(0xA4, bytes([ERASE_KB]), timeout=ERASE_TIMEOUT_MS)
        if rep[4] != 0:
            raise IspError("ERASE status %02X" % rep[4])

    def _packets(self, cmd, addr, data, what):
        """PROGRAM or VERIFY of data at addr in packets of CHUNK bytes, each the 32-bit address, one unused byte
        and the data XOR the key; return the first failing address or None."""
        for off in range(0, len(data), CHUNK):
            part = data[off:off + CHUNK]
            enc = bytes(x ^ self.key[i % 8] for i, x in enumerate(part))
            rep = self.xfer(cmd, struct.pack("<I", addr + off) + b"\x00" + enc)
            if rep[4] != 0:
                print("  %-8s 0x%04X: status %02X" % (what, addr + off, rep[4]), flush=True)
                return addr + off
        return None

    def program(self, addr, data):
        """PROGRAM (A5). The bootloader collects the data of a 64-byte page and writes the page when it is
        complete, so the packets must run from a page boundary without gaps."""
        assert addr % PAGE == 0 and len(data) % PAGE == 0
        return self._packets(0xA5, addr, data, "program")

    def verify(self, addr, data):
        """VERIFY (A6) compares by MOVC: f5 on a mismatch, then fe for every VERIFY until the next ERASE."""
        return self._packets(0xA6, addr, data, "verify")

    def end(self):
        """ISP_END (A2) with payload 01: the bootloader resets the chip at once, before the reply is read."""
        bulk(self.fd, EP_OUT, b"\xa2\x01\x00\x01")
        try:
            bulk(self.fd, self.ep_in, timeout=500)
        except OSError:
            pass


def open_isp(poll):
    d = the_isp()
    if not d and poll:
        print("waiting up to %d s for the WCH bootloader (4348:55e0) ..." % POLL_TIMEOUT, flush=True)
        deadline = time.monotonic() + POLL_TIMEOUT
        while not d and time.monotonic() < deadline:
            time.sleep(0.25)
            d = the_isp()
    if not d:
        raise SystemExit("no WCH bootloader (4348:55e0) found. Start it from the firmware (QMK: Fn+Esc, or Esc "
                         "held while plugging in), or hold P1.5 low at power-up.")
    try:
        return Isp(d)
    except IspError as e:
        raise SystemExit("%s; nothing was sent" % e)


# --- flash ----------------------------------------------------------------------------------------------------
def start_session(isp):
    """READ_CONFIG, IDENTIFY, ISP_KEY; return (bBOOT_LOAD, readable, UID)."""
    boot_load, btver, uid = isp.read_config()
    readable = isp.identify()
    isp.set_key(uid)
    print("  session  UID %s, BTVER %s, bBOOT_LOAD %d (%s), IDENTIFY %s" % (
        uid.hex(" "), btver.hex(" "), boot_load, "power-up entry" if boot_load else "called from the firmware",
        "ok: the bootloader reads the flash" if readable else "f1: the bootloader reads no flash in this session"),
        flush=True)
    return boot_load, readable, uid


def write_all(isp, data, state):
    """ERASE, then PROGRAM and VERIFY from the second page to the end, then the first page. Raise IspError on a
    failure."""
    isp.erase()
    print("  erase    0x0000-0x%04X" % (CODE_SIZE - 1), flush=True)
    for addr, part, what in ((PAGE, data[PAGE:], "0x%04X-0x%04X" % (PAGE, len(data) - 1)),
                             (0, data[:PAGE], "0x0000-0x%04X (last)" % (PAGE - 1))):
        if not part:
            continue
        state["page0"] = addr == 0
        a = isp.program(addr, part)
        if a is not None:
            raise IspError("PROGRAM failed in the page 0x%04X" % (a & ~(PAGE - 1)))
        print("  program  %s" % what, flush=True)
        a = isp.verify(addr, part)
        if a is not None:
            raise IspError("VERIFY failed at 0x%04X-0x%04X" % (a, min(a + CHUNK, addr + len(part)) - 1))
        print("  verify   %s identical" % what, flush=True)


def replug(port):
    """Wait for the bootloader at port to leave the bus and for a bootloader to come back; return its sysfs
    dir. Bootloaders that were connected before do not count."""
    print("\nErased. Unplug the keyboard, then plug it in again. The bootloader starts at power-up now.",
          flush=True)
    others = {h[2] for h in usb_devices({ISP_ID}) if h[2] != port}
    deadline = time.monotonic() + REPLUG_TIMEOUT
    while any(h[2] == port for h in usb_devices({ISP_ID})):
        if time.monotonic() >= deadline:
            raise SystemExit("the bootloader is still connected after %d s. The flash is erased, so the chip "
                             "starts the bootloader at every power-up; run the flash again." % REPLUG_TIMEOUT)
        time.sleep(0.25)
    while time.monotonic() < deadline:
        back = [h for h in usb_devices({ISP_ID}) if h[2] not in others]
        if len(back) == 1:
            time.sleep(0.5)                              # the interface appears after the device
            return back[0][3]
        if len(back) > 1:
            raise SystemExit("%d bootloaders came back (USB %s); refusing. The flash is erased, so the chip starts "
                             "the bootloader at every power-up; run the flash again with only this keyboard "
                             "connected." % (len(back), ", ".join(h[2] for h in back)))
        other = [h for h in usb_devices() if h[2] == port]
        if other:
            raise SystemExit("%04x:%04x came back at USB %s, not the bootloader: the erase did not reach the first "
                             "page although the bootloader reported success. Nothing else was written."
                             % (other[0][0], other[0][1], port))
        time.sleep(0.25)
    raise SystemExit("nothing came back within %d s. If the erase took effect, the chip starts the bootloader at "
                     "every power-up; run the flash again." % REPLUG_TIMEOUT)


def do_flash(a):
    data = load(a.image, a.force)
    expect = {a.expect} if a.expect else None
    names = "%04x:%04x" % a.expect if a.expect else "the firmware"
    if not a.expect:
        vidpid = usb_id_in(data)
        if vidpid:
            print("          USB device descriptor %04x:%04x found in the file" % vidpid)
    print("plan    : A7 READ_CONFIG, A1 IDENTIFY, A3 ISP_KEY; if the bootloader reads no flash in this session:\n"
          "          A4 erase, replug, A7, A1, A3\n"
          "          A4 erase 0x0000-0x%04X, A5/A6 0x%04X-0x%04X, A5/A6 0x0000-0x%04X, A2 reset, wait for %s"
          % (CODE_SIZE - 1, PAGE, len(data) - 1, PAGE - 1, names))
    print("          The bootloader does not say which keyboard it is in: make sure the image is for this board.")
    if not a.send:
        print("\nDRY RUN. Nothing sent. Run again with --send to flash.")
        return
    isp = open_isp(a.poll)
    erased = False
    state = {"page0": False}
    try:
        try:
            boot_load, readable, uid = start_session(isp)
            if not readable:
                if boot_load:
                    raise SystemExit("IDENTIFY fails after a power-up entry, so a write could not be verified. "
                                     "Nothing was written.")
                erased = True                            # an error of the erase request may come after the erase
                isp.erase()
                print("  erase    0x0000-0x%04X (no VERIFY in this session)" % (CODE_SIZE - 1), flush=True)
                isp.close()
                isp = Isp(replug(isp.port))
                boot_load, readable, uid2 = start_session(isp)
                if uid2 != uid:
                    uid2 = isp.read_config()[2]          # one first reply of a session has shown other bytes
                if uid2 != uid:
                    raise SystemExit("the chip that came back has UID %s, not %s: another keyboard. Nothing was "
                                     "written to it. The erased keyboard starts the bootloader at every power-up; "
                                     "plug in only that one and run the flash again." % (uid2.hex(" "), uid.hex(" ")))
                if not readable:
                    raise SystemExit("IDENTIFY fails after the power-up entry too, so a write could not be "
                                     "verified. Nothing was written. The flash is erased: the chip starts the "
                                     "bootloader at every power-up.")
            erased = True
            write_all(isp, data, state)
        except (OSError, IspError, KeyboardInterrupt) as e:
            msg = "interrupted" if isinstance(e, KeyboardInterrupt) else str(e)
            if state["page0"]:
                raise SystemExit("ABORT: %s, while the first page was being written. Do not unplug the keyboard: "
                                 "run the flash again now, in this session. After a replug the chip may start "
                                 "neither the firmware nor the bootloader; then only P1.5 held low at power-up "
                                 "starts the bootloader." % msg)
            if erased:
                raise SystemExit("ABORT: %s. The flash may be erased or partly written. Run the flash again; if "
                                 "the bootloader is gone, replug first (with the first page erased it starts at "
                                 "power-up)." % msg)
            raise SystemExit("ABORT before the erase: %s. Nothing was written." % msg)
        try:
            isp.end()
        except OSError as e:
            raise SystemExit("ISP_END failed: %s. The flash is written and verified; a replug starts it." % e)
    finally:
        isp.close()
    print("reset sent; waiting up to %d s for %s at USB %s ..." % (APP_TIMEOUT, names, isp.port), flush=True)
    time.sleep(0.5)                                      # the bootloader leaves the bus first
    h = wait_at_port(isp.port, expect, APP_TIMEOUT)
    if not h:
        raise SystemExit("%s did not appear at USB %s within %d s. The image is written and verified; run 'status' "
                         "or 'lsusb' to see what the keyboard enumerates as." % (names, isp.port, APP_TIMEOUT))
    print("%04x:%04x enumerated at USB %s" % (h[0], h[1], h[2]))


def do_status(a):
    hits = usb_devices({ISP_ID})
    for vid, pid, port, d in hits:
        try:
            bcd = open(d + "/bcdDevice").read().strip()
        except OSError:
            bcd = "?"
        print("%04x:%04x WCH bootloader at USB %s, bcdDevice %s" % (vid, pid, port, bcd))
    if not hits:
        print("no WCH bootloader (4348:55e0) found")


def do_check(a):
    data = load(a.image, a.force)
    vidpid = usb_id_in(data)
    if vidpid:
        print("          USB device descriptor %04x:%04x found in the file" % vidpid)
    print("ok")


def do_config(a):
    isp = open_isp(a.poll)
    try:
        boot_load, btver, uid = isp.read_config()
    finally:
        isp.close()
    print("UID %s, BTVER %s, bBOOT_LOAD %d (%s)" % (uid.hex(" "), btver.hex(" "), boot_load,
                                                   "power-up entry" if boot_load else "called from the firmware"))


def do_verify(a):
    data = read_file(a.file)
    if len(data) == FLASH_SIZE:
        data = data[:CODE_SIZE]
    elif len(data) > CODE_SIZE:
        raise SystemExit("%s: longer than the code flash (0x%04X bytes) and not a 64 KB dump" % (a.file, CODE_SIZE))
    data += bytes(-len(data) % 8)                        # VERIFY compares multiples of 8 bytes
    isp = open_isp(a.poll)
    try:
        boot_load, readable, uid = start_session(isp)
        if not readable:
            raise SystemExit("the bootloader reads no flash in this session, so VERIFY cannot work. Unplug the "
                             "keyboard and plug it in again with P1.5 held low, or compare after a flash.")
        bad = isp.verify(0, data)
    finally:
        isp.close()
    if bad is not None:
        raise SystemExit("MISMATCH in 0x%04X-0x%04X" % (bad, min(bad + CHUNK, len(data)) - 1))
    print("identical: 0x0000-0x%04X" % (len(data) - 1))


def usb_id(s):
    """argparse type for --expect: VID:PID in hex."""
    try:
        vid, pid = (int(x, 16) for x in s.split(":"))
        if 0 <= vid <= 0xffff and 0 <= pid <= 0xffff:
            return vid, pid
    except ValueError:
        pass
    raise argparse.ArgumentTypeError("%r is not VID:PID in hex (e.g. 5244:0580)" % s)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("status"); p.set_defaults(fn=do_status)
    p = sub.add_parser("check"); p.add_argument("image")
    p.add_argument("--force", action="store_true", help="accept an image that fails the checks")
    p.set_defaults(fn=do_check)
    p = sub.add_parser("flash"); p.add_argument("image"); p.add_argument("--send", action="store_true")
    p.add_argument("--poll", action="store_true", help="wait up to %d s for the bootloader" % POLL_TIMEOUT)
    p.add_argument("--force", action="store_true", help="flash an image that fails the checks")
    p.add_argument("--expect", type=usb_id, help="VID:PID to wait for after the reset (default: any device at "
                                                 "the same USB port)")
    p.set_defaults(fn=do_flash)
    p = sub.add_parser("verify"); p.add_argument("file")
    p.add_argument("--poll", action="store_true", help="wait up to %d s for the bootloader" % POLL_TIMEOUT)
    p.set_defaults(fn=do_verify)
    p = sub.add_parser("config")
    p.add_argument("--poll", action="store_true", help="wait up to %d s for the bootloader" % POLL_TIMEOUT)
    p.set_defaults(fn=do_config)
    a = ap.parse_args()
    try:
        a.fn(a)
    except KeyboardInterrupt:
        raise SystemExit("interrupted")


if __name__ == "__main__":
    main()
