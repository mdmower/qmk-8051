#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Replace the Evision bootloader of a CH555 keyboard with WCH's, and flash a QMK image through it.

QMK images for the Evision bootloader start at 0x2800, because that bootloader keeps 0x0000-0x27FF for itself. The
chip's own bootloader from WCH, at 0xF400, can write all of 0x0000-0xEFFF, which gives an image 10 KB more space. This
script takes a keyboard with stock firmware through the four steps of the switch and stops at the first step that
fails:

  1. evision_dump.py reads the whole flash into DUMP. Flashing this dump back to the keyboard is the only way to
     restore the Evision bootloader. The script does not proceed without this backup.
  2. evision_flash.py enter starts the Evision bootloader.
  3. evision_flash.py flashes the stepping stone, evision_wch_stone.bin, in place of the stock firmware. The stone is
     a small program whose purpose is to call the WCH bootloader with P1.5 low.
  4. wch_isp.py flashes IMAGE through the WCH bootloader. IMAGE must be built with BOOTLOADER = wch-isp. On some
     keyboards, you may be asked to re-plug the USB cable; the script says when.

  python3 -I util/evision_wch_switch.py IMAGE DUMP [--expect VID:PID] [--send]

Without --send, the script only reports the keyboard's state and the commands it would run. If a step fails, run the
same command again: the script checks whether the keyboard is in the stock firmware, in the Evision bootloader, or in
the WCH bootloader, and continues from there so long as DUMP exists from an earlier run. The script never overwrites a
DUMP that exists. While the keyboard still runs the stock firmware, the script reads the flash again and continues
only if the two dumps are identical in 0x0000-0xEFFF, the range that a restore writes. It refuses a DUMP that is not
64 KB, has no boot marker at 0x27C0, or does not contain the Evision bootloader.

Later flashes do not need this script. Fn+Esc, or Esc held while plugging in, starts the WCH bootloader, and
wch_isp.py flash writes the next image. To return to the Evision bootloader and the stock firmware, flash DUMP with
wch_isp.py.

Linux only and only the Python standard library is used.
"""
import sys
if sys.platform == "win32":
    raise SystemExit("evision_wch_switch.py runs on Linux only, because wch_isp.py does")
import argparse
import glob
import os
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
STONE = os.path.join(HERE, "evision_wch_stone.bin")
STOCK_ID = (0x320f, 0x5000)
EVISION_ID = (0x0c45, 0x7687)
WCH_ID = (0x4348, 0x55e0)
NAMES = {STOCK_ID: "stock firmware", EVISION_ID: "Evision bootloader", WCH_ID: "WCH bootloader"}
FLASH_SIZE = 0x10000
CODE_SIZE = 0xf000                               # wch_isp.py writes 0x0000-0xEFFF of a dump; the data flash follows
MARKER, MARKER_ADDR = bytes.fromhex("aa5533cc"), 0x27c0
EVISION_END = 0x2800
EVISION_DESCRIPTOR = bytes.fromhex("450c8776")   # 0c45:7687 in the Evision bootloader's device descriptor


def tool(name, *args):
    return [sys.executable, "-I", os.path.join(HERE, name)] + list(args)


def show(cmd):
    """The command as one line, with the files of util/ relative to the working directory."""
    return " ".join([os.path.basename(cmd[0]), cmd[1]]
                    + [os.path.relpath(c) if c.startswith(HERE + os.sep) else c for c in cmd[2:]])


def run(cmd):
    print("\n$ %s" % show(cmd), flush=True)
    return subprocess.run(cmd).returncode == 0


def usb_devices():
    """{(vid, pid): [USB port]} of the USB devices that have one of the three IDs."""
    found = {}
    for d in glob.glob("/sys/bus/usb/devices/*"):
        try:
            vidpid = (int(open(d + "/idVendor").read(), 16), int(open(d + "/idProduct").read(), 16))
        except (OSError, ValueError):
            continue
        if vidpid in NAMES:
            found.setdefault(vidpid, []).append(os.path.basename(d))
    return found


def keyboard_state():
    """The one (vid, pid) of the keyboard, or None. Exit if more than one keyboard could be meant."""
    found = usb_devices()
    if len(found) > 1 or any(len(ports) > 1 for ports in found.values()):
        raise SystemExit("more than one keyboard found (%s); refusing. Unplug all but the one to switch."
                         % ", ".join("%04x:%04x at USB %s" % (vid, pid, p) for (vid, pid), ps in found.items()
                                     for p in ps))
    return next(iter(found), None)


def read_dump(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError as e:
        raise SystemExit("%s: %s" % (path, e.strerror or e))


def dump_problems(d):
    """Why the bytes d are not a dump of a keyboard with the Evision bootloader, as a list of strings."""
    if len(d) != FLASH_SIZE:
        return ["%d bytes, not a 64 KB dump" % len(d)]
    bad = []
    if d[MARKER_ADDR:MARKER_ADDR + 4] != MARKER:
        bad.append("no boot marker AA 55 33 CC at 0x27C0 (found %s)" % d[MARKER_ADDR:MARKER_ADDR + 4].hex(" "))
    if EVISION_DESCRIPTOR not in d[:EVISION_END]:
        bad.append("0x0000-0x27FF does not hold the Evision bootloader (no USB ID 0c45:7687 in it)")
    return bad


def accept_dump(path):
    """Exit unless the file at path is a dump the script accepts."""
    bad = dump_problems(read_dump(path))
    if bad:
        raise SystemExit("%s is not a dump the script accepts:\n  %s\nNothing has been changed on the keyboard."
                         % (path, "\n  ".join(bad)))


def compare_dump(fresh, existing):
    """Exit unless the dump just read into fresh equals the file at existing in 0x0000-0xEFFF. Remove fresh either
    way. The data flash above may differ: QMK keeps its settings there, and a restore does not write it."""
    a, b = read_dump(fresh), read_dump(existing)
    os.remove(fresh)
    if a[:CODE_SIZE] != b[:CODE_SIZE]:
        first = next(i for i in range(CODE_SIZE) if a[i] != b[i])
        raise SystemExit("%s exists and differs from this keyboard's flash, first at 0x%04X. The script does not "
                         "overwrite it: name a new file. Nothing has been changed on the keyboard." % (existing, first))
    print("\ndump    : %s is identical to the flash read now%s" % (
        existing, " in 0x0000-0xEFFF; the data flash differs, which holds QMK's settings" if a != b else ""))


def usb_id(s):
    """argparse type for --expect: VID:PID in hex."""
    try:
        vid, pid = (int(x, 16) for x in s.split(":"))
        if 0 <= vid <= 0xffff and 0 <= pid <= 0xffff:
            return "%04x:%04x" % (vid, pid)
    except ValueError:
        pass
    raise argparse.ArgumentTypeError("%r is not VID:PID in hex (e.g. 5244:0580)" % s)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("image", help="QMK image built with BOOTLOADER = wch-isp")
    ap.add_argument("dump", help="file for the 64 KB dump of the keyboard's flash")
    ap.add_argument("--expect", type=usb_id, help="VID:PID that QMK enumerates as, passed on to wch_isp.py")
    ap.add_argument("--send", action="store_true")
    a = ap.parse_args()

    if os.path.exists(a.image) and os.path.getsize(a.image) == FLASH_SIZE:
        raise SystemExit("%s is a 64 KB dump, not a QMK image" % a.image)
    if not os.path.isdir(os.path.dirname(os.path.abspath(a.dump))):
        raise SystemExit("%s: the directory does not exist" % a.dump)
    if not run(tool("wch_isp.py", "check", a.image)):
        raise SystemExit("%s: wch_isp.py does not accept the image" % a.image)

    state = keyboard_state()
    if state is None:
        raise SystemExit("\nno keyboard in the stock firmware (320f:5000), the Evision bootloader (0c45:7687), or "
                         "the WCH bootloader (4348:55e0) found. The dump can only be read from the stock firmware. "
                         "A keyboard on QMK needs the stock firmware flashed back first. If you have the dump from "
                         "an earlier run of this script, Fn+Esc on QMK starts the Evision bootloader instead, and "
                         "the script continues from there.")
    print("\nkeyboard: %s (%04x:%04x)" % (NAMES[state], *state))

    steps = []                                           # (command, what to do after it succeeded)
    temp = None                                          # the file of the second read, while it exists
    if state == STOCK_ID:
        if os.path.exists(a.dump):
            bad = dump_problems(read_dump(a.dump))
            if bad:
                raise SystemExit("%s exists and is not a dump the script accepts:\n  %s\nThe script does not "
                                 "overwrite it: name a new file." % (a.dump, "\n  ".join(bad)))
            print("dump    : %s exists; the flash is read again, and the script continues only if both are identical"
                  % a.dump)
            fd, fresh = tempfile.mkstemp(dir=os.path.dirname(os.path.abspath(a.dump)),
                                         prefix=os.path.basename(a.dump) + ".", suffix=".new")
            os.close(fd)
            temp = fresh
            steps.append((tool("evision_dump.py", "--out", fresh, "--send"), lambda: compare_dump(fresh, a.dump)))
        else:
            steps.append((tool("evision_dump.py", "--out", a.dump, "--send"), lambda: accept_dump(a.dump)))
        steps.append((tool("evision_flash.py", "enter", "--send"), None))
    else:
        bad = dump_problems(read_dump(a.dump)) if os.path.exists(a.dump) else ["the file does not exist"]
        if bad:
            raise SystemExit("the keyboard has left the stock firmware, but %s is not its dump:\n  %s\nWithout the "
                             "dump there is no way back to the Evision bootloader. If it is in another file, name "
                             "that file." % (a.dump, "\n  ".join(bad)))
        print("dump    : %s holds the Evision bootloader and the boot marker" % a.dump)
    if state != WCH_ID:
        steps.append((tool("evision_flash.py", "flash", STONE, "--expect", "4348:55e0", "--send"), None))
    steps.append((tool("wch_isp.py", "flash", a.image, "--poll", "--send")
                  + (["--expect", a.expect] if a.expect else []), None))

    print("steps   :")
    for cmd, _ in steps:
        print("  " + show(cmd))
    if not a.send:
        if temp:
            os.remove(temp)
        print("\nDRY RUN. Nothing sent. Run again with --send to switch.")
        return
    for cmd, after in steps:
        if not run(cmd):
            if temp and os.path.exists(temp):
                os.remove(temp)
            raise SystemExit("\nstopped at this step. Run the same evision_wch_switch.py command again to "
                             "continue from the keyboard's current state.")
        if after:
            after()
    print("\nswitched: the keyboard runs QMK with the WCH bootloader. Keep %s: flashing it with wch_isp.py "
          "returns the keyboard to the Evision bootloader and the stock firmware." % a.dump)


if __name__ == "__main__":
    main()
