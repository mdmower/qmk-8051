#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build an MCS-51 keyboard of this tree (e.g. redragon/k580) in Docker (util/mcs51_build.Dockerfile: SDCC, clang and
the QMK CLI; SDCC version = its ARG SDCC_VER). Linux and Windows (Docker Desktop). Python standard library only.

  python3 -I util/mcs51_build.py KEYBOARD [KEYMAP] [--serial SERIAL]     (KEYMAP default: default)

--serial sets the USB serial number (optional; without it the serial is the chip ID).

Output: qmk compile writes the image to the top directory of the tree as <keyboard>_<keymap>.bin or .hex, in the
format of the keyboard's bootloader (e.g. redragon_k580_default.bin), after the bootloader's flash tool has checked it.
"""
import sys, os, re, io, shutil, tarfile, argparse, subprocess

TREE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCKERFILE = os.path.join(TREE, "util", "mcs51_build.Dockerfile")

def run(cmd, **kw):
    r = subprocess.run(cmd, **kw)
    if r.returncode:
        sys.exit(r.returncode)

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("keyboard", help="keyboard under keyboards/, e.g. redragon/k580")
    ap.add_argument("keymap", nargs="?", default="default", help="keymap (default: default)")
    ap.add_argument("--serial", help="USB serial number: letters, digits and . _ : -, at most 126 characters")
    a = ap.parse_args()
    if not (re.fullmatch(r"[a-z0-9_/]+", a.keyboard)
            and os.path.isfile(os.path.join(TREE, "keyboards", a.keyboard, "keyboard.json"))):
        ap.error("KEYBOARD must be a directory under keyboards/ with a keyboard.json, e.g. redragon/k580")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", a.keymap):
        ap.error("KEYMAP: letters, digits, _ and - only")
    if a.serial is not None and not re.fullmatch(r"[A-Za-z0-9._:-]{1,126}", a.serial):
        ap.error("--serial: letters, digits and . _ : - only, at most 126 characters")
    with open(DOCKERFILE) as f:
        sdcc_ver = re.search(r"^ARG SDCC_VER=(\S+)", f.read(), re.M).group(1)
    image = "qmk-8051-mcs51:sdcc-%s-clang" % sdcc_ver

    if subprocess.run(["docker", "image", "inspect", image], stdout=subprocess.DEVNULL,
                      stderr=subprocess.DEVNULL).returncode:
        # The build context: the Dockerfile and the tree's requirements.txt, nothing else. USTAR: docker reads stdin
        # as a tar archive only if its first 512-byte header is a file's (PAX puts an extended header first).
        ctx = io.BytesIO()
        with tarfile.open(fileobj=ctx, mode="w", format=tarfile.USTAR_FORMAT) as t:
            t.add(DOCKERFILE, "mcs51_build.Dockerfile")
            t.add(os.path.join(TREE, "requirements.txt"), "requirements.txt")
        run(["docker", "build", "-t", image, "-f", "mcs51_build.Dockerfile", "-"], input=ctx.getvalue())
    shutil.rmtree(os.path.join(TREE, ".build"), ignore_errors=True)
    # Linux: the build runs as the calling user. Docker Desktop on Windows maps file ownership itself.
    user = ["-u", "%d:%d" % (os.getuid(), os.getgid())] if hasattr(os, "getuid") else []
    # The image has no git: SKIP_GIT=yes makes qmk generate version.h without it (QMK_VERSION "NA").
    run(["docker", "run", "--rm"] + user + ["-e", "HOME=/tmp", "-v", TREE + ":/qmk", "-w", "/qmk", image,
         "qmk", "compile", "-kb", a.keyboard, "-km", a.keymap, "-e", "SKIP_GIT=yes"]
        + (["-e", "SERIAL_NUMBER=" + a.serial] if a.serial else []))

if __name__ == "__main__":
    main()
