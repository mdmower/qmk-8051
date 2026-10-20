#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Read the code flash of a Redragon keyboard with an Evision CH555-family 8051 (K580: VS11K33A; K552RGB-2,
K551RGB-1, K582RGB, K589RGB: VS11K28A) running its stock firmware.

The stock firmware (USB 320f:5000) answers vendor command 0x10 on USB interface 1 with up to 0x36 bytes read
from code flash at 0xA700 + offset (mod 0x10000), so the whole 64 KB, including the OEM bootloader, can be
read. The read handler contains no flash write or erase. Tested with stock firmware 1.04. The K552RGB-2 stock
firmware V0101 and the K551RGB-1/K582RGB/K589RGB stock firmware V0107 (VS11K28A, also 320f:5000) have the same
handler (static analysis; not run on these keyboards). QMK does not implement this command: make the backup
before flashing QMK.

The replies contain no checksum of the data: the range is read twice, and the file is written only if both reads
are identical.

Read-only by construction:
  * Sends only command 0x10 (read). Refuses any report that matches the bootloader-entry pattern
    (bytes 1-3 = EE EE 00).
  * Opens only the vendor collection of 320f:5000 on USB interface 1, and refuses when more than one keyboard
    has that ID (these keyboards share it; the stock firmware does not identify the board).
  * DRY-RUN by default. Nothing is sent without --send.

Linux (hidraw and sysfs) and Windows (hid.dll and SetupAPI, in util/evision_win.py). Python standard library only.

Usage:
  python3 -I util/evision_dump.py --out k580_full64k.bin [--send]
  python3 -I util/evision_dump.py --start 0x2800 --end 0xEFC0 --out readback.bin [--send]
"""
import sys, os, glob, argparse, hashlib, select, importlib.util

VID, PID = 0x320f, 0x5000
READ_OP = 0x10
BASE_ADDR = 0xA700         # the read handler adds this to the offset (addr = BASE + offset, mod 2^16)
REPORT_ID = 0x04
REPORT_LEN = 64            # 1 report-id byte + 63 data bytes
MAX_CHUNK = 0x36
BOOT_MARKER = 0x27C0       # AA 55 33 CC here makes the OEM bootloader start the app at 0x2800

def usb_port(devpath):
    """USB port path (sysfs name, e.g. 1-2.2) and serial of the USB device of a hidraw devpath."""
    d = os.path.dirname(os.path.dirname(devpath))   # .../1-2.2/1-2.2:1.1/0003:VVVV:PPPP.NNNN
    try:
        serial = open(os.path.join(d, "serial")).read().strip()
    except OSError:
        serial = None
    return "USB %s%s" % (os.path.basename(d), ", serial %s" % serial if serial else "")

def find_nodes(vid, pid):
    """Return [(node, iface1, devpath)] for the hidraw nodes of vid:pid."""
    hits = []
    for ue in glob.glob("/sys/class/hidraw/hidraw*/device/uevent"):
        try:
            txt = open(ue).read()
        except OSError:                             # node removed since the glob
            continue
        if ("HID_ID=0003:0000%04X:0000%04X" % (vid, pid)).upper() not in txt.upper():
            continue
        devpath = os.path.realpath(os.path.dirname(ue))
        hits.append(("/dev/" + ue.split("/")[4], ":1.1/" in devpath, devpath))
    return sorted(hits)

def resolve_node():
    """Find the node for VID:PID on USB interface 1 (the vendor collection). Return (node, USB port)."""
    hits = find_nodes(VID, PID)
    iface1 = [(node, usb_port(devpath)) for node, i1, devpath in hits if i1]
    if len(iface1) > 1:
        raise SystemExit("%d keyboards with %04x:%04x found (%s); refusing. Unplug all but the one to read."
                         % (len(iface1), VID, PID, "; ".join("%s at %s" % hit for hit in iface1)))
    if iface1:
        return iface1[0]
    if hits:
        raise SystemExit("found %s but not on interface 1; refusing" % hits[0][0])
    raise SystemExit("no HID device for %04x:%04x found (is the keyboard connected and running stock firmware?)"
                     % (VID, PID))

def open_node(node):
    return os.open(node, os.O_RDWR)

def close_node(fd):
    os.close(fd)

def write_report(fd, data):
    os.write(fd, data)

def read_report(fd, timeout):
    """One input report, or None if none arrives within timeout seconds."""
    rl, _, _ = select.select([fd], [], [], timeout)
    return os.read(fd, REPORT_LEN) if rl else None

def build_report(offset, length):
    if length > MAX_CHUNK:
        raise SystemExit("length %d exceeds max packet payload 0x%x" % (length, MAX_CHUNK))
    r = bytearray(REPORT_LEN)
    r[0] = REPORT_ID
    r[3] = READ_OP
    r[4] = length & 0xff
    r[5] = offset & 0xff
    r[6] = (offset >> 8) & 0xff
    # checksum = sum of bytes 3..63, low byte in [1], high byte in [2]
    cks = sum(r[3:64]) & 0xffff
    r[1] = cks & 0xff
    r[2] = (cks >> 8) & 0xff
    if r[1] == 0xEE and r[2] == 0xEE and r[3] == 0x00:
        raise SystemExit("refusing: report matches bootloader-entry pattern")
    return bytes(r)

def read_chunk(fd, addr, n):
    """One 0x10 read of n bytes at absolute flash addr. Returns bytes or None."""
    offset = (addr - BASE_ADDR) & 0xffff
    report = build_report(offset, n)
    for _ in range(3):
        write_report(fd, report)
        resp = read_report(fd, 1.0)
        if resp is None:
            continue
        # the reply echoes the request header; a mismatch means a stray or rejected report
        if resp[3] == READ_OP and resp[4] == n and resp[5] == (offset & 0xff) and resp[6] == ((offset >> 8) & 0xff):
            return bytes(resp[8:8 + n])
    return None

# --- Windows: the same functions on hid.dll and SetupAPI ------------------------------------------------------
if sys.platform == "win32":
    _spec = importlib.util.spec_from_file_location(
        "evision_win", os.path.join(os.path.dirname(os.path.abspath(__file__)), "evision_win.py"))
    _win = importlib.util.module_from_spec(_spec)
    _spec.loader.exec_module(_win)
    for _name in ("find_nodes", "usb_port", "open_node", "close_node", "write_report", "read_report"):
        globals()[_name] = getattr(_win, _name)

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True, help="output file")
    ap.add_argument("--start", default="0x0000", help="first address (default 0x0000)")
    ap.add_argument("--end", default="0x10000", help="end address, exclusive (default 0x10000)")
    ap.add_argument("--chunk", default="0x30", help="bytes per read (<= 0x36)")
    ap.add_argument("--send", action="store_true", help="read from the keyboard (default: dry-run)")
    a = ap.parse_args()
    start, end, chunk = int(a.start, 0), int(a.end, 0), int(a.chunk, 0)
    if not (0 <= start < end <= 0x10000):
        raise SystemExit("need 0 <= start < end <= 0x10000")
    if not (0 < chunk <= MAX_CHUNK):
        raise SystemExit("chunk must be 1..0x%x" % MAX_CHUNK)
    node, port = resolve_node()
    print("device : %04x:%04x  node %s (interface 1, %s)" % (VID, PID, node, port))
    print("read   : 0x%04x-0x%04x (%d bytes) twice, in 0x%02x-byte reads, command 0x10 (read-only)"
          % (start, end - 1, end - start, chunk))
    if not a.send:
        print("\nDRY-RUN. Nothing sent. Re-run with --send to read."); return
    fd = open_node(node)
    reads = []
    try:
        for k in (1, 2):
            print("read %d of 2" % k, flush=True)
            out = bytearray()
            addr = start
            while addr < end:
                n = min(chunk, end - addr)
                data = read_chunk(fd, addr, n)
                if data is None or len(data) != n:
                    raise SystemExit("read failed at 0x%04x" % addr)
                out += data
                addr += n
                if (addr - start) % 0x1000 == 0:
                    print("  0x%04x  (%d/%d)" % (addr, addr - start, end - start), flush=True)
            reads.append(out)
    finally:
        close_node(fd)
    if reads[0] != reads[1]:
        bad = next(i for i in range(len(out)) if reads[0][i] != reads[1][i])
        raise SystemExit("the two reads differ (first at 0x%04x); nothing written. Run again." % (start + bad))
    open(a.out, "wb").write(out)
    print("wrote %d bytes to %s" % (len(out), a.out))
    print("sha256 : %s" % hashlib.sha256(out).hexdigest())
    if start <= BOOT_MARKER and BOOT_MARKER + 4 <= end:
        m = out[BOOT_MARKER - start:BOOT_MARKER - start + 4]
        print("0x%04x : %s (%s)" % (BOOT_MARKER, m.hex(" "), "boot marker present" if m == b"\xaa\x55\x33\xcc"
                                    else "NOT the expected boot marker AA 55 33 CC"))

if __name__ == "__main__":
    main()
