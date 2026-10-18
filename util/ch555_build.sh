#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Build a CH555 keyboard of this tree (e.g. redragon/k580) in Docker (util/ch555_build.Dockerfile: SDCC, clang and
# the QMK CLI; SDCC version = its ARG SDCC_VER).
#
#   util/ch555_build.sh KEYBOARD [KEYMAP]     (KEYMAP default: default)
#   SERIAL_NUMBER=<serial> before the command sets the USB serial number (optional).
#
# Output: <keyboard>_<keymap>.bin in the top directory of the tree (e.g. redragon_k580_default.bin), the app image
# for the Evision OEM bootloader, based at 0x2800 (util/evision_flash.py flash).
set -euo pipefail
tree=$(cd "$(dirname "$0")/.." && pwd)
if [ $# -lt 1 ] || ! [[ $1 =~ ^[a-z0-9_/]+$ ]] || ! [ -f "$tree/keyboards/$1/keyboard.json" ]; then
    echo "usage: util/ch555_build.sh KEYBOARD [KEYMAP]   (KEYBOARD under keyboards/, e.g. redragon/k580)" >&2
    exit 1
fi
kb=$1
km=${2:-default}
out=${kb//\//_}_${km}.bin
serial=${SERIAL_NUMBER:-}
if [ -n "$serial" ] && ! [[ $serial =~ ^[A-Za-z0-9._:-]{1,126}$ ]]; then
    echo "SERIAL_NUMBER: letters, digits and . _ : - only, at most 126 characters" >&2; exit 1
fi
sdcc_ver=$(sed -n 's/^ARG SDCC_VER=//p' "$tree/util/ch555_build.Dockerfile")
image=qmk-8051-ch555:sdcc-${sdcc_ver:?}-clang

if ! docker image inspect "$image" >/dev/null 2>&1; then
    tar -cf - -C "$tree/util" ch555_build.Dockerfile -C "$tree" requirements.txt |
        docker build -t "$image" -f ch555_build.Dockerfile -
fi
rm -rf "$tree/.build" "$tree/$out"
# The image has no git: SKIP_GIT=yes makes qmk generate version.h without it (QMK_VERSION "NA").
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$tree":/qmk -w /qmk "$image" \
    qmk compile -kb "$kb" -km "$km" -e SKIP_GIT=yes ${serial:+-e SERIAL_NUMBER=$serial}
python3 -I "$tree/util/evision_flash.py" check "$tree/$out"
