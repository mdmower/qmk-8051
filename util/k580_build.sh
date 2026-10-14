#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Build redragon/k580 in Docker (util/k580_build.Dockerfile: SDCC, clang and the QMK CLI; SDCC version = its ARG
# SDCC_VER).
#
#   util/k580_build.sh [KEYMAP]     (default: default)
#   SERIAL_NUMBER=<serial> before the command sets the USB serial number (optional).
#
# Output: redragon_k580_<keymap>.bin in the top directory of the tree, the app image for the Evision OEM
# bootloader, based at 0x2800 (util/evision_flash.py flash).
set -euo pipefail
tree=$(cd "$(dirname "$0")/.." && pwd)
km=${1:-default}
serial=${SERIAL_NUMBER:-}
if [ -n "$serial" ] && ! [[ $serial =~ ^[A-Za-z0-9._:-]{1,126}$ ]]; then
    echo "SERIAL_NUMBER: letters, digits and . _ : - only, at most 126 characters" >&2; exit 1
fi
sdcc_ver=$(sed -n 's/^ARG SDCC_VER=//p' "$tree/util/k580_build.Dockerfile")
image=qmk-8051-k580:sdcc-${sdcc_ver:?}-clang

if ! docker image inspect "$image" >/dev/null 2>&1; then
    tar -cf - -C "$tree/util" k580_build.Dockerfile -C "$tree" requirements.txt |
        docker build -t "$image" -f k580_build.Dockerfile -
fi
rm -rf "$tree/.build" "$tree/redragon_k580_${km}.bin"
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$tree":/qmk -w /qmk "$image" \
    qmk compile -kb redragon/k580 -km "$km" ${serial:+-e SERIAL_NUMBER=$serial}
python3 -I "$tree/util/evision_flash.py" check "$tree/redragon_k580_${km}.bin"
