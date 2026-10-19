# Redragon K582RGB

QMK for the Redragon K582 Surara RGB, board K582RGB, with an Evision VS11K28A MCU, an 8051 in the WCH CH555
family. The firmware is flashed through the keyboard's own Evision bootloader, which stays in place.

* Keyboard maintainer: [mdmower](https://github.com/mdmower)
* Hardware supported: Redragon K582RGB and K582RGB SE with an Evision VS11K28A MCU
* State: **not run on a keyboard yet.** The key matrix, the LED layout and the indicators come from the stock
  firmware V0107 (Redragon's updater for the K582RGB, which also serves the K551RGB-1 and the K589RGB); the
  firmware is tested only in a simulator. The LED controller, key matrix and lighting code are shared with the
  [K580](../k580/), where they are tested.

Backup, flashing, restore, the `via` keymap and the risks of flashing:
[top-level readme](../../../readme.md#before-you-flash). Make the backup before the first flash: it reads the
flash through the stock firmware, which the flash erases.

## Check the keyboard first

- With the stock firmware, the keyboard enumerates as `320f:5000` (`lsusb`). The updater's firmware reports
  bcdDevice 0x0107 (V0107). Other Redragon keyboards, the K580 among them, report the same ID: check that the
  keyboard is a K582RGB.
- The K582RGB-PRO (optical switches) has a different MCU (Evision VS11K09A-1, a Sonix SN32). This firmware does
  not apply to it.
- The backup prints whether the boot marker `AA 55 33 CC` is at 0x27C0. If it is not, do not flash.

## Build and flash

```
util/ch555_build.sh redragon/k582                 # or: qmk compile -kb redragon/k582 -km default
python3 -I util/evision_flash.py enter --send     # from the stock firmware; from QMK: Fn+Esc
python3 -I util/evision_flash.py flash redragon_k582_default.bin --expect 5244:0582 --send
```

## Bootloader

Enter the Evision bootloader (`0c45:7687`) in one of these ways:

* Fn+Esc (`QK_BOOT` on layer 1).
* Hold Esc while plugging in the keyboard. This check runs before QMK starts, so it also works when a QMK build
  does not start.
* With the stock firmware: `python3 -I util/evision_flash.py enter --send`.

The keyboard stays in the bootloader, also after a replug, until a flash completes.

## Default keymap

- Layer 0: the printed ANSI layout (QMK layout `fullsize_ansi`).
- Layer 1 (Fn): Fn+Esc enters the bootloader (`QK_BOOT`). Fn+F1–F12 send the functions printed on the keycaps:
  media player, volume down, volume up, mute, stop, previous, play/pause, next, mail, browser home, calculator,
  search. Fn+Win toggles Win Lock, which blocks the Win key. Win Lock is off after power-up.
- Indicators above the numpad: Num Lock, Caps Lock, Scroll Lock and Win Lock, lit by the host's lock state and by
  Win Lock also while the key LEDs are off.
- Lighting ([led_fx.c](../common/led_fx.c)): one zone, the key LEDs, set with the same keys as on the K580. Fn
  plus a key below steps a setting; with Shift held it steps back. The settings are saved in the data flash 3 s
  after the last change and kept at power-off ([flash_store.c](../common/flash_store.c)); the LEDs flicker briefly
  while they are saved.

  | Setting | Keys |
  |---|---|
  | Mode | Fn+Ins |
  | Hue (24 steps) | Fn+Home |
  | Saturation (5 steps; the lowest is white) | Fn+PgUp |
  | Brightness (8 steps) | Fn+Del |
  | Speed (5 steps) | Fn+End |
  | On/off | Fn+PgDn |

  Modes: static colour, rainbow, reversed rainbow, wave, reversed wave, colour cycle, breathing. Default: wave.

The other lighting legends on the keycaps (Ins–PgDn 1–6, −/= speed, arrows brightness, direction and colour) are
not used, as on the K580: Fn+Ins–PgDn set everything. Not implemented either: per-key colour editing (Fn+\`),
factory reset (Fn+PrtSc) and LEDs off (Fn+Caps Lock).

## VIA keymap

The default keymap with VIA ([top-level readme](../../../readme.md#keymaps-default-and-via)). VIA definition:
[keymaps/via/redragon_k582_via.json](keymaps/via/redragon_k582_via.json). About 310 bytes of the EEPROM are left
for macros.

## Flash space

The application region (0x2800–0xEFBF) has about 17.5 KB free with the `default` keymap and about 6 KB below the
store with the `via` keymap. The features turned off for flash space are listed in [config.h](config.h) and
[rules.mk](rules.mk).

## Hardware

From the stock firmware V0107 (static analysis):

- Key matrix: 7 rows × 16 columns. Rows P4.0–P4.6; columns P0.0–P0.7, P3.0–P3.7. The numpad is in row 6, except
  numpad 1 at (4, 15). The stock key table also has ISO, ABNT2 and JIS positions (left of Z (4, 1), ISO # (3, 12),
  ABNT2 /? (4, 12), numpad comma (7, 15), JIS keys in rows 5 and 7); only the ANSI layout is defined here.
- LEDs: the CH555 LED controller, with red, green and blue rows on P4, P2 and P1 and common lines on P0 and P3,
  shared with the key matrix. LED row n on common line c is the LED of the key in matrix row n, column c. LED row
  7: Num Lock (common line 8, green), Caps Lock, Win Lock and Scroll Lock (common line 9: red, green, blue).
- No GPIO indicators, roller or side lights; nothing on P5 or P7.
- The stock firmware runs at 32 MHz, as this firmware does.
