# Redragon K580 RGB

QMK for the Redragon K580 RGB with an Evision VS11K33A MCU (an 8051 in the WCH CH555 family). The firmware is
flashed through the keyboard's own Evision bootloader, which stays in place, so the stock firmware can be
restored from a backup.

Two keymaps: `default` (recommended) and `via`, which can be changed in VIA and keeps QMK's EEPROM in code flash,
at your own risk ([VIA keymap](#via-keymap-at-your-own-risk)).

Tested on one keyboard (stock firmware 1.04). The firmware is built with SDCC
([platforms/mcs51](../../../platforms/mcs51)).

## Check the keyboard first

- With the stock firmware, the keyboard enumerates as `320f:5000` (`lsusb`). The tested keyboard reports
  bcdDevice 0x0104 (firmware 1.04).
- The backup in step 1 prints whether the boot marker `AA 55 33 CC` is at 0x27C0. If it is not, the flash layout
  below does not apply: do not flash.

Other K580 revisions may use a different MCU. This firmware does not apply to them.

## Requirements

- Linux, Python 3 (standard library only), Docker (for the build).
- Access to the keyboard's hidraw nodes: install the udev rule and be a member of the `plugdev` group. The rule
  also grants the raw HID interface of QMK, which VIA uses.
  ```
  sudo cp util/udev/70-redragon-k580.rules /etc/udev/rules.d/
  sudo udevadm control --reload && sudo udevadm trigger
  ```

Run the commands below from the top directory of this repository. `util/k580_dump.py` and
`util/evision_flash.py` send nothing to the keyboard unless `--send` is given; without it they print what they
would do.

## Flash layout and bootloader

| Range | Contents |
|---|---|
| 0x0000–0x27FF | Evision OEM bootloader (USB `0c45:7687`). The tools here never write it. |
| 0x27C0 | Boot marker `AA 55 33 CC`. The bootloader starts the application only while it is present. |
| 0x2800–0xEFBF | Application: stock firmware or QMK. The bootloader erases and programs this range. The `via` build keeps QMK's EEPROM at 0xE740–0xEFBF. |
| 0xF000–0xF3FF | Data flash. The keyboard saves its lighting settings and roller mode here. Flashing does not erase it, and the stock firmware 1.04 does not use it. |
| 0xF400– | WCH factory ISP bootloader (`4348:55e0`). Not used: flashing through it overwrites the OEM bootloader. |

Entering the bootloader erases the boot marker. The keyboard then stays in the bootloader, also after a replug,
until a flash completes. If a flash fails or is interrupted, the keyboard stays in the bootloader and the flash
command can be run again. `util/evision_flash.py` writes only 0x2800–0xEFBF, and checks the image before it
erases anything.

`python3 -I util/evision_flash.py status` shows which mode the keyboard is in: stock firmware, bootloader or QMK.

## 1. Back up the stock firmware

QMK does not implement the read command that the backup uses. Make the backup before flashing QMK.

```
python3 -I util/k580_dump.py --out k580_stock_1.bin --send
python3 -I util/k580_dump.py --out k580_stock_2.bin --send
cmp k580_stock_1.bin k580_stock_2.bin
python3 -I util/evision_flash.py extract k580_stock_1.bin k580_stock_app.bin
```

`k580_stock_1.bin` is the full 64 KB code flash, including the OEM bootloader. `k580_stock_app.bin` is its
application region, the image that restores the stock firmware. Keep both.

## 2. Build

```
util/k580_build.sh            # keymap "default", in Docker (SDCC, clang and the QMK CLI)
util/k580_build.sh via        # keymap "via"
```

or, with SDCC 4.6.0 (4.4.0 also works; 4.5.0 does not), clang (with clang-query and llvm-nm) and binutils
installed, `qmk compile -kb redragon/k580 -km default`. The output is `redragon_k580_default.bin` (or
`redragon_k580_via.bin`), the application image based at 0x2800. It contains a
check for Esc at plug-in at 0x2803 and the interrupt table that the bootloader jumps to at 0x3000 (made by
`util/evision_oem_image.py`).

The USB serial number is the chip ID unless one is set at build time (letters, digits and `. _ : -`, at
most 126 characters):

```
SERIAL_NUMBER=RDK580RGB00000000000 util/k580_build.sh
```

## 3. Flash QMK

From the stock firmware:

```
python3 -I util/evision_flash.py enter --send
python3 -I util/evision_flash.py flash redragon_k580_default.bin --expect 5244:0580 --send
```

QMK enumerates as one USB device (`5244:0580`) with three HID interfaces: boot keyboard, raw HID, and NKRO keyboard
with system and media keys.

To flash a new build over QMK, enter the bootloader with Fn+Esc, or hold Esc while plugging in the keyboard, then
run the `flash` command. The Esc check runs before QMK starts, so it also works if a QMK build does not start.
With `--poll`, the `flash` command waits up to 15 s for the bootloader: start it first, then press
Fn+Esc. Use it only with one keyboard connected that has this bootloader.

## Restore the stock firmware

Enter the bootloader (Fn+Esc in QMK), then:

```
python3 -I util/evision_flash.py flash k580_stock_app.bin --send
```

To compare the flash with the backup while the stock firmware runs:

```
python3 -I util/k580_dump.py --start 0x2800 --end 0xEFC0 --out readback.bin --send
python3 -I util/evision_flash.py verify readback.bin k580_stock_app.bin
```

On the tested keyboard, a restore of firmware 1.04 gave a full 64 KB read-back identical to the original backup.

## Default keymap

- Layer 0: the printed layout. Media keys: previous, play/pause, next. Roller: volume; the roller toggle key
  switches it to key LED brightness and back. The roller mode is saved like the lighting settings.
- Layer 1 (Fn): Fn+Esc enters the bootloader (`QK_BOOT`). Fn+F1–F12 send the functions printed on the keycaps:
  media player, volume down, volume up, mute, stop, previous, play/pause, next, mail, browser home, calculator,
  search. Fn+Win toggles Win Lock, which blocks both Win keys and lights the Win Lock LED. Win Lock is off after
  power-up.
- G1–G5 and Rec: F13–F17 and F18, for shortcuts set in the operating system.
- Lighting ([led_fx.c](../common/led_fx.c)): two zones with separate settings, the keys and the side lights. Fn
  plus the keys below steps a setting; with Shift held it steps back. The settings are saved in the data flash 3 s
  after the last change and kept at power-off ([flash_store.c](../common/flash_store.c)); the LEDs flicker briefly
  while they are saved.

  | Setting | Keys | Side lights |
  |---|---|---|
  | Mode | Fn+Ins | Fn+G1 |
  | Hue (24 steps) | Fn+Home | Fn+G2 |
  | Saturation (5 steps; the lowest is white) | Fn+PgUp | Fn+G3 |
  | Brightness (8 steps) | Fn+Del | Fn+G4 |
  | Speed (5 steps) | Fn+End | Fn+G5 |
  | On/off | Fn+PgDn | Fn+Rec |

  Modes: static colour, rainbow, reversed rainbow, wave, reversed wave, colour cycle, breathing. The rainbow
  runs from left to right on the keys and from back to front on the side lights; the wave moves the same way
  (the reversed modes run the other way). G1–G5, Rec and the media keys have one colour each and light at the
  zone's brightness whatever the hue. Defaults: wave on the keys, rainbow on the side lights.

## VIA keymap (at your own risk)

The `via` keymap is the default keymap with [VIA](https://usevia.app): keys, the roller and macros can be changed
from a browser while the keyboard runs, and the changes are kept at power-off.

**Risk.** QMK's EEPROM (its settings, the keymap and the macros) is stored in code flash at 0xE740–0xEFBF, written
by the firmware while the keyboard runs. The CH555 enables writes to all of code flash at once, including the
Evision bootloader at 0x0000–0x27FF. The store code ([lib/ch555/eeprom_code_flash.c](../../../lib/ch555/eeprom_code_flash.c))
writes only its own pages and checks the address before and after it enables writes, and the build keeps code
below the store. A fault in this code, or in the firmware around it, could still erase or damage the bootloader.
No tested way exists to restore the bootloader: the keyboard could then no longer be flashed or used.

Build and flash `redragon_k580_via.bin` as above. Then:

1. Open [usevia.app](https://usevia.app) in Chrome or Chromium (WebHID). In Settings, turn on "Show Design tab".
2. In the Design tab, load the VIA definition [keymaps/via/redragon_k580_via.json](keymaps/via/redragon_k580_via.json).
3. In the Configure tab, authorize the keyboard (`5244:0580`).

How the store works:
- The EEPROM is 1 KB: QMK's settings, 3 layers, the roller on each layer and about 215 bytes of macros. Layer 0
  and 1 start as the default keymap, layer 2 as transparent keys.
- A change is saved 2 s after the last one. The store has two copies; a save writes the older copy and marks it
  valid last, so a power loss during a save keeps the previous state.
- Every flash erases the store: after a flash the keymap is the default again. Save the layout first in VIA
  (Save + Load) and load it again after the flash. The lighting settings and the roller mode are in the data
  flash and are kept.
- The custom keys of the default keymap (Win Lock, roller mode, lighting settings) have names in VIA. VIA's
  roller panel shows them as `CUSTOM(n)`.
- Tap keys (`LT`, `MT`, …) and one-shot keys do nothing ([config.h](config.h)). Macros are untested.

To go back, flash the `default` build. It writes no code flash except the boot marker page.

## Limits

- In the `default` keymap, QMK's settings (eeconfig) are kept in RAM only (transient EEPROM driver): after
  power-up they are QMK's defaults. The keyboard's own settings (lighting, roller mode) use the data flash
  instead. The `via` keymap keeps QMK's settings in code flash.
- To save flash, the K580 build turns off tap and one-shot keys ([config.h](config.h)), `send_string` (on in
  the `via` keymap, for macros), Grave Escape, Magic keycodes and Space Cadet ([rules.mk](rules.mk)).
- The backup and flash tools (`util/k580_dump.py`, `util/evision_flash.py`) run on Linux only.
- Vial is not supported: it needs the vial-qmk fork. The `via` keymap supports VIA.

## Hardware

- Key matrix: 7 rows × 18 columns. Rows P4.0–P4.6; columns P0.0–P0.7, P3.0–P3.7, P7.0–P7.1.
- Port 5: P5.5/P5.6 volume roller; P5.4 indicator LED enable (active low); P5.3, P5.2, P5.1, P5.0 Num Lock,
  Caps Lock, Scroll Lock, Win Lock LEDs (high = on).
- Key LEDs and side lights: the CH555 LED controller, with red, green and blue rows on P4, P2 and P1 and common
  lines on P0, P3 and P7, shared with the key matrix. LED row n on common line c is the LED of the key in matrix
  row n, column c (rows 0–6); LED row 7 on common lines 0–15 drives the side lights. G1–G5 (white), Rec and
  the media keys (red) have one LED each, on the red row. The keys are read in the LED controller's interrupt,
  on each common line ([led_dma.c](../common/led_dma.c)).
