#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Sinowealth ISP bootloader (0603:1020) backup and flash tool for Redragon keyboards with a Sinowealth SH68F90A.

The firmware occupies 0x0000-0xEFFF, and the ISP bootloader 0xF000-0xFFFF. The bootloader keeps its own jump at 0x0000
and starts the firmware only if 0xEFFB holds 02 (LJMP), followed by the firmware's reset vector at 0xEFFC-0xEFFD. It
reads and writes 0x0000-0xEFFF in "ISP form": bytes 0x0000-0x0002 are the firmware's own LJMP, and 0xEFFB-0xEFFF read as
zeros. QMK images and backups made by this tool are in this form.

The firmware (stock or QMK) jumps to the ISP bootloader when it receives the feature report 05 75. QMK also jumps to it
when Esc is held while plugging in, or when Fn+Esc is pressed. A replug in ISP mode starts the firmware again if it is
enabled (02 at 0xEFFB); after an erase, the board stays in ISP mode across replugs until a flash completes.

Bootloader commands (SET_FEATURE, report ID 5, 6 bytes: 05 cmd lo hi 00 00) and transfers (report ID 6, 2050 bytes:
06 72|77 + one 2048-byte page):
  55        enable firmware: programs 02 at 0xEFFB if it is not there; also enables reads below 0xF000
  52 lo hi  set the read address; then each GET_FEATURE of report 6 returns 06 72 + the next page
            (before 55 or 45 every read returns the bootloader's first page, 0xF000-0xF7FF)
  57 lo hi  set the write address; then each SET_FEATURE of report 6 (06 77 + page) programs the next page
  45        erase 0x0000-0xEFFF; the bootloader puts its jump at 0x0000
  5A        reboot (a watchdog reset)

Safety:
  * Dry run by default: nothing is sent without --send.
  * Never writes the bootloader: only 30 pages from 0x0000 (0x0000-0xEFFF) are written, and the bootloader itself does
    not write above 0xEFFF.
  * Image checks: an LJMP at 0x0000 to an address below 0xEFFB, zeros at 0xEFFB-0xEFFF, nothing at or above 0xF000 (the
    firmware region of a full 64 KB backup is used), and the jump into the ISP bootloader (clr EA, mov B,#0xA5,
    mov A,#0x5A, ljmp 0xFF00) somewhere in the image, so that a flashed image that starts can enter ISP mode again.
  * Before the erase, reads the bootloader's first page and checks its first instructions: the transfer report, its page
    size, and the bootloader are checked before anything is written.
  * After the write, reads 0x0000-0xEFFF back and compares it with the image; 55 (enable) and 5A (reboot) are sent only
    if they are identical. A failed flash leaves the board in ISP mode; flashing can be retried.
  * backup sends 55 before it reads. 55 enables whatever is at 0x0000-0xEFFF, so a board that is in ISP mode after a
    failed flash must be flashed, not backed up: backup from ISP mode needs --from-isp.
  * Refuses when more than one board in these modes is connected.

Linux (hidraw, usbfs, and sysfs) and Windows (hid.dll and SetupAPI, in hid_win.py). Python standard library only. On
Linux, the bootloader has no hidraw node: its interface descriptor declares no endpoint (bNumEndpoints 0, although an
interrupt endpoint descriptor follows), so the kernel's HID driver does not bind to it. The tool sends its reports as
HID class requests on endpoint 0 through usbfs (/dev/bus/usb), which needs the udev rule for the USB device 0603:1020.

Usage:
  python3 -I util/sinowealth_flash.py status                         # current keyboard mode
  python3 -I util/sinowealth_flash.py check IMAGE                    # validate an image (no device access)
  python3 -I util/sinowealth_flash.py backup OUT.bin [--send]        # read the 64 KB flash twice, save if equal
  python3 -I util/sinowealth_flash.py flash IMAGE [--send]           # erase, write, verify, enable, reboot
IMAGE is Intel HEX (.hex, as qmk compile writes it) or binary: the firmware region (at most 61,440 B) or a full 64 KB
backup, of which only the firmware region is written.
"""
import sys, os, re, glob, argparse, hashlib, struct, time, ctypes, importlib.util
if sys.platform != "win32":
    import fcntl

def _ioc(d, t, nr, size): return (d << 30) | (size << 16) | (t << 8) | nr
def HIDIOCSFEATURE(n): return _ioc(3, ord('H'), 0x06, n)
def HIDIOCGFEATURE(n): return _ioc(3, ord('H'), 0x07, n)
CTRL = struct.Struct("@BBHHHIP")         # struct usbdevfs_ctrltransfer
USBDEVFS_CONTROL = _ioc(3, ord('U'), 0, CTRL.size)
USBDEVFS_CLAIMINTERFACE = _ioc(2, ord('U'), 15, 4)
USBDEVFS_RELEASEINTERFACE = _ioc(2, ord('U'), 16, 4)
CONTROL_TIMEOUT_MS = 5000

STOCK_IDS = {(0x258a, 0x0049): "stock firmware"}
QMK_IDS = {(0x5244, 0x0668): "QMK K668"}
ISP_IDS = {(0x0603, 0x1020): "ISP bootloader"}
APP_IDS = {**STOCK_IDS, **QMK_IDS}
ALL_IDS = {**APP_IDS, **ISP_IDS}
VENDOR_PAGE = 0xff00                     # usage page of the collections of reports 5 and 6
REPORT_CMD, CMD_LEN = 5, 6               # lengths with the report ID byte
REPORT_XFER, XFER_LEN = 6, 2050
PAGE = 2048
FW_SIZE = 0xf000                         # firmware region 0x0000-0xEFFF
FLASH_SIZE = 0x10000
FW_PAGES, FLASH_PAGES = FW_SIZE // PAGE, FLASH_SIZE // PAGE   # 30, 32
MARKER = FW_SIZE - 5                     # 0xEFFB: 02 + the firmware's reset vector, kept by the bootloader
CMD_ENTER_ISP = 0x75                     # to the running firmware
CMD_ENABLE, CMD_READ, CMD_WRITE, CMD_ERASE, CMD_REBOOT = 0x55, 0x52, 0x57, 0x45, 0x5a
XFER_READ, XFER_WRITE = 0x72, 0x77
ISP_JUMP = bytes.fromhex("c2af75f0a5745a02ff00")   # clr EA; mov B,#0xA5; mov A,#0x5A; ljmp 0xFF00
BOOT_PROLOGUE = bytes.fromhex("c2af758170")         # clr EA; mov SP,#0x70
BOOT_ISP_REPORT = bytes.fromhex(                    # the collection of report 6 in the bootloader's descriptor
    "0600ff0901a1018506150025ff190129027508960108b102c0")
ISP_TIMEOUT = 10                         # seconds to wait for ISP mode after 05 75
APP_TIMEOUT = 30                         # seconds to wait for the firmware after the reboot
ERASE_SETTLE = 2.0                       # seconds after the erase before the next command

KNOWN_IMAGES = {   # sha256 of the 61,440-byte firmware region in ISP form -> label
    "ed1599c7d89e3f89770c5c5835ca0d983e72fe772eaeaf2db35238555affdd2a":
        "K668 stock V1.6.6 (bcdDevice 0x0105) firmware region, as read from one K668",
}
KNOWN_BOOTLOADERS = {   # MD5 of the 4 KB bootloader -> label
    "3e0ebd0c440af5236d7ff8872343f85d": "SH68F90 ISP bootloader 3e0ebd0c (on the K668)",
    "2d169670eae0d36eae8188562c1f66e8": "SH68F90 ISP bootloader 2d169670",
    "cfc8661da8c9d7e351b36c0a763426aa": "SH68F90 ISP bootloader cfc8661d",
}

class IspError(Exception):
    pass

# --- images ---------------------------------------------------------------------------------------------------
def read_file(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError as e:
        raise SystemExit("%s: %s" % (path, e.strerror or e))

def read_ihex(path):
    """Intel HEX -> {address: byte}. Checks every record's checksum."""
    data, base = {}, 0
    for n, line in enumerate(read_file(path).splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        if not line.startswith(b":"):
            raise SystemExit("%s:%d: not an Intel HEX record" % (path, n))
        try:
            rec = bytes.fromhex(line[1:].decode("ascii"))
        except ValueError:
            raise SystemExit("%s:%d: not an Intel HEX record (an odd number of digits or a character that is not a "
                             "hex digit)" % (path, n))
        if len(rec) < 5 or len(rec) != rec[0] + 5 or sum(rec) & 0xff:
            raise SystemExit("%s:%d: bad record length or checksum" % (path, n))
        addr, typ, payload = rec[1] << 8 | rec[2], rec[3], rec[4:-1]
        if typ == 0:
            for i, b in enumerate(payload):
                data[base + addr + i] = b
        elif typ == 1:
            break
        elif typ == 2:
            base = int.from_bytes(payload, "big") << 4
        elif typ == 4:
            base = int.from_bytes(payload, "big") << 16
        elif typ not in (3, 5):
            raise SystemExit("%s:%d: unknown record type %d" % (path, n, typ))
    return data

def load_image(path):
    """Return (firmware region of FW_SIZE bytes, padded with 0x00, info). Exit if the image fails a check."""
    if path.lower().endswith((".hex", ".ihx")):
        data = read_ihex(path)
        if not data:
            raise SystemExit("%s: no data" % path)
        top = max(data) + 1
        if top > FLASH_SIZE:
            raise SystemExit("%s: data up to 0x%X, beyond the 64 KB flash" % (path, top - 1))
        raw = bytearray(FLASH_SIZE if top > FW_SIZE else FW_SIZE)
        for a, b in data.items():
            raw[a] = b
    else:
        raw = bytearray(read_file(path))
        if len(raw) > FW_SIZE and len(raw) != FLASH_SIZE:
            raise SystemExit("%s: %d bytes; expected the firmware region (at most %d) or a full 64 KB backup"
                             % (path, len(raw), FW_SIZE))
    full = len(raw) > FW_SIZE
    if full and not raw[FW_SIZE:].startswith(BOOT_PROLOGUE):
        raise SystemExit("%s: data at 0x%X and above, but no ISP bootloader there: not a backup of this flash"
                         % (path, FW_SIZE))
    img = bytes(raw[:FW_SIZE]) + bytes(FW_SIZE - min(len(raw), FW_SIZE))
    used = len(img.rstrip(b"\x00"))
    if img[0] != 0x02:
        raise SystemExit("%s: no LJMP at 0x0000 (byte %02X)" % (path, img[0]))
    entry = img[1] << 8 | img[2]
    if entry == FW_SIZE and img[MARKER] == 0x02:
        raise SystemExit("%s: the image has the bootloader's jump at 0x0000 and the firmware's at 0x%04X (the layout "
                         "in flash, as an ICP programmer reads it); this tool takes the ISP form, with the firmware's "
                         "LJMP at 0x0000" % (path, MARKER))
    if entry >= MARKER:
        raise SystemExit("%s: LJMP at 0x0000 leads to 0x%04X, outside the firmware" % (path, entry))
    if any(img[MARKER:]):
        raise SystemExit("%s: 0x%04X-0x%04X are not zero (%s); the bootloader keeps its marker and the firmware's "
                         "reset vector there" % (path, MARKER, FW_SIZE - 1, img[MARKER:].hex(" ")))
    jump = img.find(ISP_JUMP)
    if jump < 0:
        raise SystemExit("%s: no jump into the ISP bootloader (%s) in the image; after a flash the board could not "
                         "enter ISP mode again" % (path, ISP_JUMP.hex(" ")))
    sha = hashlib.sha256(img).hexdigest()
    info = {"path": path, "full": full, "used": used, "entry": entry, "jump": jump, "sha256": sha,
            "label": KNOWN_IMAGES.get(sha)}
    if full:
        info["bootloader"] = bytes(raw[FW_SIZE:])
    return img, info

def print_image(info):
    print("image   : %s" % info["path"])
    print("          %d B used of %d (last non-zero byte at 0x%04X), reset LJMP to 0x%04X, ISP jump at 0x%04X"
          % (info["used"], FW_SIZE, info["used"] - 1, info["entry"], info["jump"]))
    print("          sha256 %s (firmware region)%s" % (info["sha256"], " = " + info["label"] if info["label"] else ""))
    if info["full"]:
        print("          full 64 KB backup: only 0x0000-0x%04X is written" % (FW_SIZE - 1))

def bootloader_text(bl):
    md5 = hashlib.md5(bl).hexdigest()
    return "MD5 %s (%s)" % (md5, KNOWN_BOOTLOADERS.get(md5, "not a known one"))

# --- device discovery: Linux (sysfs and hidraw) ---------------------------------------------------------------
def feature_reports(desc):
    """{report ID: length with the report ID byte} of the feature reports of a HID report descriptor."""
    bits, size, count, rid, stack, i = {}, 0, 0, 0, [], 0
    while i < len(desc):
        b = desc[i]
        if b == 0xfe:                                     # long item
            i += 3 + (desc[i + 1] if i + 1 < len(desc) else 0)
            continue
        n = (0, 1, 2, 4)[b & 3]
        v = int.from_bytes(desc[i + 1:i + 1 + n], "little")
        tag = b & 0xfc
        if tag == 0x74:
            size = v
        elif tag == 0x94:
            count = v
        elif tag == 0x84:
            rid = v
        elif tag == 0xa4:
            stack.append((size, count, rid))
        elif tag == 0xb4 and stack:
            size, count, rid = stack.pop()
        elif tag == 0xb0:
            bits[rid] = bits.get(rid, 0) + size * count
        i += 1 + n
    return {r: (nb + 7) // 8 + (1 if r else 0) for r, nb in bits.items()}

def collections(ids):
    """Return [(node, vid, pid, feature reports, port, usb)] for the HID nodes of the (vid, pid) pairs in ids; port is
    the sysfs name of the USB port (the same when the board enumerates again with another ID), usb its sysfs dir."""
    hits = []
    for ue in glob.glob("/sys/class/hidraw/hidraw*/device/uevent"):
        try:
            m = re.search(r"HID_ID=0003:0000([0-9A-F]{4}):0000([0-9A-F]{4})", open(ue).read().upper())
            if not m or (int(m.group(1), 16), int(m.group(2), 16)) not in ids:
                continue
            hiddir = os.path.realpath(os.path.dirname(ue))     # .../1-9.2/1-9.2:1.0/0003:5244:0668.0042
            desc = open(os.path.join(hiddir, "report_descriptor"), "rb").read()
        except OSError:                                     # node removed since the glob
            continue
        usb = os.path.dirname(os.path.dirname(hiddir))
        hits.append(("/dev/" + ue.split("/")[4], int(m.group(1), 16), int(m.group(2), 16), feature_reports(desc),
                     os.path.basename(usb), usb))
    return sorted(hits)

def usb_text(usb):
    out = []
    for k in ("bcdDevice", "manufacturer", "product", "serial"):
        try:
            out.append("%s %r" % (k, open(os.path.join(usb, k)).read().strip()))
        except OSError:
            pass
    return ", ".join(out)

def usbfs_devices(ids):
    """Return [(usbfs node, vid, pid, port, usb)] for the USB devices of the (vid, pid) pairs in ids."""
    hits = []
    for usb in glob.glob("/sys/bus/usb/devices/*"):
        try:
            vidpid = (int(open(usb + "/idVendor").read(), 16), int(open(usb + "/idProduct").read(), 16))
            if vidpid not in ids:
                continue
            node = "/dev/bus/usb/%03d/%03d" % (int(open(usb + "/busnum").read()), int(open(usb + "/devnum").read()))
        except (OSError, ValueError):
            continue
        hits.append((node, vidpid[0], vidpid[1], os.path.basename(usb), usb))
    return sorted(hits)

def accessible(node, timeout):
    """Wait until udev has set the permissions of a new hidraw or usbfs node."""
    deadline = time.monotonic() + timeout
    while not os.access(node, os.R_OK | os.W_OK):
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.1)
    return True

class Usbfs:
    """Interface 0 of a USB device through usbfs: HID class requests on endpoint 0."""
    def __init__(self, node):
        self.fd = os.open(node, os.O_RDWR)
        try:
            fcntl.ioctl(self.fd, USBDEVFS_CLAIMINTERFACE, bytearray(struct.pack("I", 0)))
        except OSError:
            os.close(self.fd)
            raise

    def close(self):
        try:
            fcntl.ioctl(self.fd, USBDEVFS_RELEASEINTERFACE, bytearray(struct.pack("I", 0)))
        except OSError:                                  # the device has left the bus
            pass
        os.close(self.fd)

    def control(self, rtype, req, value, data=None, length=0):
        """Control transfer to interface 0: data out, or up to length bytes in. Return the bytes read."""
        if data is not None:
            buf = ctypes.create_string_buffer(bytes(data), len(data))
        else:
            buf = ctypes.create_string_buffer(length)
        n =fcntl.ioctl(self.fd, USBDEVFS_CONTROL,
                        bytearray(CTRL.pack(rtype, req, value, 0, len(buf), CONTROL_TIMEOUT_MS, ctypes.addressof(buf))))
        return buf.raw[:n]

    def report_descriptor(self):
        return self.control(0x81, 0x06, 0x2200, length=1024)        # GET_DESCRIPTOR (HID report) of interface 0

def open_node(node):
    return Usbfs(node) if node.startswith("/dev/bus/usb/") else os.open(node, os.O_RDWR)

def close_node(fd):
    fd.close() if isinstance(fd, Usbfs) else os.close(fd)

def set_feature(fd, data):
    """SET_FEATURE; data starts with the report ID."""
    if isinstance(fd, Usbfs):
        fd.control(0x21, 0x09, 0x0300 | data[0], data=data)          # SET_REPORT (feature)
        return
    buf = bytearray(data)
    fcntl.ioctl(fd, HIDIOCSFEATURE(len(buf)), buf, True)

def get_feature(fd, report_id, length):
    """GET_FEATURE of up to length bytes; the reply starts with the report ID."""
    if isinstance(fd, Usbfs):
        return fd.control(0xa1, 0x01, 0x0300 | report_id, length=length)   # GET_REPORT (feature)
    buf = bytearray(length)
    buf[0] = report_id
    n = fcntl.ioctl(fd, HIDIOCGFEATURE(length), buf, True)
    return bytes(buf[:n])

# --- device discovery: Windows (hid_win.py) -------------------------------------------------------------------
if sys.platform == "win32":
    _spec = importlib.util.spec_from_file_location(
        "hid_win", os.path.join(os.path.dirname(os.path.abspath(__file__)), "hid_win.py"))
    _win = importlib.util.module_from_spec(_spec)
    _spec.loader.exec_module(_win)

    def collections(ids):
        """As on Linux. Windows gives a collection's longest feature report, not its report IDs: the collections of
        reports 5 and 6 are the vendor collections whose feature reports have their lengths."""
        hits = []
        for c in _win.hid_collections(set(ids)):
            feats = {}
            if c.page == VENDOR_PAGE and c.feat in (CMD_LEN, XFER_LEN):
                feats[REPORT_CMD if c.feat == CMD_LEN else REPORT_XFER] = c.feat
            hits.append((c.path, c.dev.vid, c.dev.pid, feats, c.dev.location, c.dev))
        return sorted(hits, key=lambda h: h[0])

    def usb_text(dev):
        a = _win.usb_attrs(dev)
        return ", ".join("%s %r" % (k, a[k]) for k in ("bcdDevice", "manufacturer", "product", "serial") if a[k])

    usbfs_devices = lambda ids: []
    accessible = _win.wait_access
    open_node, close_node = _win.open_node, _win.close_node
    set_feature = _win.set_feature_id
    get_feature = lambda node, report_id, length: _win.get_feature_id(node, report_id)[:length]

# --- boards ---------------------------------------------------------------------------------------------------
class Board:
    """A keyboard in one of the modes of ALL_IDS: its USB port and the HID nodes of reports 5 and 6."""
    def __init__(self, vid, pid, port, usb):
        self.vid, self.pid, self.port, self.usb = vid, pid, port, usb
        self.cmd = self.xfer = None

    @property
    def isp(self):
        return (self.vid, self.pid) in ISP_IDS

    def __str__(self):
        return "%04x:%04x %s at USB %s" % (self.vid, self.pid, ALL_IDS[(self.vid, self.pid)], self.port)

def boards(ids=ALL_IDS):
    found = {}
    for node, vid, pid, feats, port, usb in collections(ids):
        b = found.setdefault(port, Board(vid, pid, port, usb))
        if feats.get(REPORT_CMD) == CMD_LEN:
            b.cmd = node
        if feats.get(REPORT_XFER) == XFER_LEN:
            b.xfer = node
    for node, vid, pid, port, usb in usbfs_devices({i: 1 for i in ids if i in ISP_IDS}):
        b = found.setdefault(port, Board(vid, pid, port, usb))
        if not b.cmd:                                    # no hidraw node: the reports go through usbfs
            b.cmd = b.xfer = node
    return [found[p] for p in sorted(found)]

def the_board():
    """The one connected board, or None. Exit if there are several: the bootloader does not identify a board."""
    bs = boards()
    if len(bs) > 1:
        raise SystemExit("%d boards found (%s); refusing. Unplug all but the one to use."
                         % (len(bs), "; ".join(str(b) for b in bs)))
    return bs[0] if bs else None

def wait_board(port, ids, timeout):
    """Wait until a board with one of ids is at port, with the nodes it needs (report 5; in ISP mode also 6)."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for b in boards(ids):
            if b.port == port and b.cmd and (b.xfer or not b.isp):
                return b
        time.sleep(0.25)
    return None

def need_access(b):
    for node in {b.cmd, b.xfer} - {None}:
        if not accessible(node, 10):
            raise SystemExit("no read/write access to %s of %04x:%04x (Linux: is the udev rule for this ID "
                             "installed?)" % (node, b.vid, b.pid))

def enter_isp(b):
    """Send 05 75 to the firmware of board b; return the board in ISP mode."""
    if not b.cmd:
        raise SystemExit("%s: no HID node with the feature report 5 (%d B), so its firmware cannot be told to enter "
                         "ISP mode; nothing was sent. Another keyboard with this ID, or a firmware without this "
                         "report: start the bootloader with Fn+Esc or with Esc held while plugging in, if the "
                         "firmware has them."
                         % (b, CMD_LEN))
    need_access(b)
    fd = open_node(b.cmd)
    try:
        set_feature(fd, bytes([REPORT_CMD, CMD_ENTER_ISP, 0, 0, 0, 0]))
    except OSError:                                      # the firmware may leave the bus before the status stage
        pass
    finally:
        close_node(fd)
    print("sent 05 75; waiting up to %d s for the ISP bootloader at USB %s ..." % (ISP_TIMEOUT, b.port), flush=True)
    isp = wait_board(b.port, ISP_IDS, ISP_TIMEOUT)
    if not isp:
        raise SystemExit("the ISP bootloader did not appear; nothing was written. Run 'status'.")
    need_access(isp)
    print("device  : %s" % isp)
    return isp

# --- ISP transport --------------------------------------------------------------------------------------------
class Isp:
    def __init__(self, b):
        if not (b.cmd and b.xfer):
            raise SystemExit("%s: no HID node with the feature reports 5 (%d B) and 6 (%d B); not this bootloader"
                             % (b, CMD_LEN, XFER_LEN))
        self.cmd = open_node(b.cmd)
        self.xfer = self.cmd if b.xfer == b.cmd else open_node(b.xfer)
        if isinstance(self.cmd, Usbfs):
            feats = feature_reports(self.cmd.report_descriptor())
            if feats.get(REPORT_CMD) != CMD_LEN or feats.get(REPORT_XFER) != XFER_LEN:
                self.close()
                raise SystemExit("%s: feature reports %s, expected 5 (%d B) and 6 (%d B); not this bootloader"
                                 % (b, feats, CMD_LEN, XFER_LEN))

    def close(self):
        if self.xfer is not self.cmd:
            close_node(self.xfer)
        close_node(self.cmd)

    def command(self, cmd, addr=0):
        set_feature(self.cmd, bytes([REPORT_CMD, cmd, addr & 0xff, addr >> 8, 0, 0]))

    def read(self, addr, pages, what):
        self.command(CMD_READ, addr)
        out = bytearray()
        for i in range(pages):
            r = get_feature(self.xfer, REPORT_XFER, XFER_LEN)
            if len(r) != XFER_LEN or r[0] != REPORT_XFER or r[1] != XFER_READ:
                raise IspError("page %d of %s: reply of %d B starting %s, expected %d B starting %02x %02x"
                               % (i, what, len(r), r[:2].hex(" "), XFER_LEN, REPORT_XFER, XFER_READ))
            out += r[2:]
            progress(what, i + 1, pages)
        return bytes(out)

    def write(self, img):
        self.command(CMD_WRITE, 0)
        for i in range(FW_PAGES):
            set_feature(self.xfer, bytes([REPORT_XFER, XFER_WRITE]) + img[i * PAGE:(i + 1) * PAGE])
            progress("write", i + 1, FW_PAGES)

    def reboot(self):
        try:
            self.command(CMD_REBOOT)
        except OSError:                                  # the bootloader resets during the request
            pass

def error_text(e):
    return "interrupted" if isinstance(e, KeyboardInterrupt) else str(e)

def progress(what, done, total):
    if done == total or done % 8 == 0:
        print("  %-8s %2d/%d pages" % (what, done, total), flush=True)

def check_first_page(isp):
    """Read the bootloader's first page (0xF000-0xF7FF): a read that writes nothing, before the erase."""
    page = isp.read(FW_SIZE, 1, "check")
    if not page.startswith(BOOT_PROLOGUE):
        raise IspError("0x%04X reads %s, not the ISP bootloader's first instructions %s"
                       % (FW_SIZE, page[:5].hex(" "), BOOT_PROLOGUE.hex(" ")))
    print("  check    bootloader at 0x%04X starts %s" % (FW_SIZE, BOOT_PROLOGUE.hex(" ")), flush=True)

def wait_app(port, ids):
    names = " or ".join("%04x:%04x" % i for i in ids)
    print("waiting up to %d s for %s at USB %s ..." % (APP_TIMEOUT, names, port), flush=True)
    b = wait_board(port, ids, APP_TIMEOUT)
    if not b:
        raise SystemExit("%s did not appear at USB %s; run 'status'" % (names, port))
    print("%s enumerated: %s" % (b, usb_text(b.usb)))

# --- subcommands ----------------------------------------------------------------------------------------------
def do_status(a):
    bs = boards()
    for b in bs:
        print("%-40s %s" % (b, usb_text(b.usb)))
    if not bs:
        print("no board found (%s)" % ", ".join("%04x:%04x" % i for i in ALL_IDS))

def do_check(a):
    print_image(load_image(a.image)[1])
    print("ok")

def find_board():
    """The one connected board; exit if there is none."""
    b = the_board()
    if not b:
        raise SystemExit("no board found: connect the keyboard (stock firmware, QMK or ISP mode)")
    print("device  : %s" % b)
    if (b.vid, b.pid) in STOCK_IDS:
        print("          258a:0049 is the ID of several Redragon keyboards: check the model on the label")
    return b

def do_flash(a):
    img, info = load_image(a.image)
    print_image(info)
    ids = [a.expect] if a.expect else list(APP_IDS)
    b = find_board()
    print("plan    : %s52 00 F0, read 1 page (check) -> 45 erase -> 57 00 00, write %d pages -> 52 00 00, read %d "
          "pages, compare\n          -> 55 enable, 5A reboot (only if identical) -> %s"
          % ("" if b.isp else "05 75 (ISP mode) -> ", FW_PAGES, FW_PAGES,
             " or ".join("%04x:%04x" % i for i in ids)))
    if not a.send:
        print("\nDRY-RUN. Nothing sent. Re-run with --send to flash."); return
    was_isp = b.isp
    if not b.isp:
        b = enter_isp(b)
    need_access(b)
    port = b.port
    # The bootloader stores bytes 1-2 of page 0 at 0xEFFC-0xEFFD; the last page carries them too.
    wimg = bytearray(img)
    wimg[MARKER + 1:MARKER + 3] = img[1:3]
    isp = Isp(b)
    erased = False
    try:
        try:
            check_first_page(isp)
            erased = True                                # an error of the erase request may come after the erase
            isp.command(CMD_ERASE)
            print("  erase    0x0000-0x%04X" % (FW_SIZE - 1), flush=True)
            time.sleep(ERASE_SETTLE)
            isp.write(bytes(wimg))
            back = isp.read(0, FW_PAGES, "verify")
        except (OSError, IspError, KeyboardInterrupt) as e:
            if erased:
                raise SystemExit("ABORT: %s. The flash may be erased: the board stays in ISP mode, also across "
                                 "replugs; run the flash again." % error_text(e))
            if was_isp:
                raise SystemExit("ABORT before the erase: %s. Nothing was written; the board is in ISP mode as before."
                                 % error_text(e))
            raise SystemExit("ABORT before the erase: %s. Nothing was written; a replug starts the firmware again."
                             % error_text(e))
        diff = [i for i in range(FW_SIZE) if back[i] != img[i]]
        if diff:
            raise SystemExit("ABORT: the read-back differs from the image in %d bytes, first at 0x%04X (%02X, image "
                             "%02X). The firmware was not enabled: the board stays in ISP mode; run the flash again."
                             % (len(diff), diff[0], back[diff[0]], img[diff[0]]))
        print("  verify   0x0000-0x%04X identical" % (FW_SIZE - 1), flush=True)
        try:
            isp.command(CMD_ENABLE)
        except (OSError, KeyboardInterrupt) as e:
            raise SystemExit("ABORT: verified, but 55 (enable) failed: %s. The board stays in ISP mode; run the flash "
                             "again." % error_text(e))
        print("  enable   ok", flush=True)
        isp.reboot()
    finally:
        isp.close()
    print("reboot sent")
    wait_app(port, ids)

def do_backup(a):
    if os.path.exists(a.out):
        raise SystemExit("%s exists; refusing to overwrite it" % a.out)
    b = find_board()
    if b.isp and not a.from_isp:
        raise SystemExit("the board is in ISP mode already. A backup sends 55, which enables whatever is at "
                         "0x0000-0x%04X: after a failed flash that is a partial image, and the board would no longer "
                         "start the bootloader. If the firmware put the board into ISP mode (Fn+Esc, or Esc held at "
                         "plug-in), run the backup again with --from-isp; after a failed flash, flash instead."
                         % (FW_SIZE - 1))
    print("plan    : %s55 enable -> 52 00 00, read %d pages, twice -> 5A reboot%s -> write %s if the reads are "
          "identical" % ("" if b.isp else "05 75 (ISP mode) -> ", FLASH_PAGES,
                         " (skipped: --no-reboot)" if a.no_reboot else "", a.out))
    print("          (55 programs 02 at 0x%04X only if it is not there; on a board whose firmware runs it is)" % MARKER)
    if not a.send:
        print("\nDRY-RUN. Nothing sent. Re-run with --send to read."); return
    try:                                                 # before anything is sent, so the reads cannot be lost
        out = open(a.out, "xb")
    except OSError as e:
        raise SystemExit("%s: %s; nothing was sent" % (a.out, e.strerror or e))
    saved = False
    try:
        if not b.isp:
            b = enter_isp(b)
        need_access(b)
        port = b.port
        isp = Isp(b)
        try:
            try:
                isp.command(CMD_ENABLE)
                reads = [isp.read(0, FLASH_PAGES, "read %d" % n) for n in (1, 2)]
            except (OSError, IspError, KeyboardInterrupt) as e:
                raise SystemExit("ABORT: %s. Nothing was saved. The board is in ISP mode; a replug starts the "
                                 "firmware." % error_text(e))
            if not a.no_reboot:
                isp.reboot()
        finally:
            isp.close()
        if reads[0] != reads[1]:
            n = sum(x != y for x, y in zip(*reads))
            raise SystemExit("the two reads differ in %d bytes; nothing was saved" % n)
        data = reads[0]
        bl = data[FW_SIZE:]
        if not bl.startswith(BOOT_PROLOGUE) or BOOT_ISP_REPORT not in bl:
            raise SystemExit("0x%04X-0x%04X is not the ISP bootloader (it starts %s); nothing was saved"
                             % (FW_SIZE, FLASH_SIZE - 1, bl[:5].hex(" ")))
        out.write(data)
        saved = True
    finally:
        out.close()
        if not saved:
            os.remove(a.out)
    fw_sha = hashlib.sha256(data[:FW_SIZE]).hexdigest()
    print("saved   : %s (%d B, two identical reads)" % (a.out, len(data)))
    print("          sha256 %s, MD5 %s (64 KB)" % (hashlib.sha256(data).hexdigest(), hashlib.md5(data).hexdigest()))
    print("          firmware region sha256 %s%s" % (fw_sha, " = " + KNOWN_IMAGES[fw_sha] if fw_sha in KNOWN_IMAGES
                                                     else ""))
    print("          bootloader %s" % bootloader_text(bl))
    if not a.no_reboot:
        wait_app(port, list(APP_IDS))

def usb_id(s):
    """argparse type for --expect: VID:PID in hex."""
    try:
        vid, pid = (int(x, 16) for x in s.split(":"))
        if 0 <= vid <= 0xffff and 0 <= pid <= 0xffff:
            return vid, pid
    except ValueError:
        pass
    raise argparse.ArgumentTypeError("%r is not VID:PID in hex (e.g. 5244:0668)" % s)

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("status"); p.set_defaults(fn=do_status)
    p = sub.add_parser("check"); p.add_argument("image"); p.set_defaults(fn=do_check)
    p = sub.add_parser("backup"); p.add_argument("out"); p.add_argument("--send", action="store_true")
    p.add_argument("--from-isp", action="store_true",
                   help="the board's firmware put it into ISP mode (Fn+Esc, Esc held at plug-in); never after a "
                        "failed flash")
    p.add_argument("--no-reboot", action="store_true", help="leave the board in ISP mode after the reads")
    p.set_defaults(fn=do_backup)
    p = sub.add_parser("flash"); p.add_argument("image"); p.add_argument("--send", action="store_true")
    p.add_argument("--expect", type=usb_id,
                   help="VID:PID the flashed firmware enumerates as (default: %s)"
                        % " or ".join("%04x:%04x" % i for i in APP_IDS))
    p.set_defaults(fn=do_flash)
    a = ap.parse_args()
    try:
        a.fn(a)
    except KeyboardInterrupt:
        raise SystemExit("interrupted")

if __name__ == "__main__":
    main()
