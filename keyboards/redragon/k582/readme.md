# Redragon K582RGB

QMK for the Redragon K582 Surara RGB, board K582RGB, with an Evision VS11K28A MCU, an 8051 in the WCH CH555 family.
Firmware is flashed through the keyboard's own Evision bootloader, which is left in place.

* Keyboard Maintainer: [mdmower](https://github.com/mdmower)
* Hardware Supported: Redragon K582RGB and K582RGB SE with an Evision VS11K28A MCU
* State: **not run on a keyboard yet.** The key matrix, LED layout and indicators come from the stock firmware V0107
  (Redragon's updater for the K582RGB), and the firmware has been tested only in a simulator.

Backup, flashing, restore, the `via` keymap and the risks of flashing are all covered in the
[top-level readme](../../../readme.md#before-you-flash). Make the backup before your first flash: it reads the flash
through the stock firmware, which flashing erases.

## Check the keyboard first

- With the stock firmware, the keyboard enumerates as `320f:5000`. You can confirm this with
  `python3 -I util/evision_flash.py status`. The updater's firmware reports bcdDevice 0x0107 (V0107). Other Redragon
  keyboards report the same ID, so also check that yours is a K582RGB.
- The K582RGB-PRO (optical switches) uses a different MCU (Evision VS11K09A-1, a Sonix SN32), and this firmware does
  not apply to it.
- The backup prints whether the boot marker `AA 55 33 CC` is at 0x27C0. If it is not, do not flash.

## Build and flash

```
python3 -I util/ch555_build.py redragon/k582      # or: qmk compile -kb redragon/k582 -km default
python3 -I util/evision_flash.py enter --send     # from the stock firmware; from QMK: Fn+Esc
python3 -I util/evision_flash.py flash redragon_k582_default.bin --expect 5244:0582 --send
```

## Entering the bootloader

You can enter the Evision bootloader (`0c45:7687`) in any of these ways:

* Press Fn+Esc (`QK_BOOT` on layer 1).
* Hold Esc while plugging in the keyboard. This check runs before QMK starts, so it works even when a QMK build does
  not start.
* With the stock firmware, run `python3 -I util/evision_flash.py enter --send`.

The keyboard stays in the bootloader until a flash completes, even across a replug.

## Default keymap

- Layer 0: the printed ANSI layout (QMK layout `fullsize_ansi`).
- Layer 1 (Fn):
  - Fn+Esc enters the bootloader (`QK_BOOT`).
  - Fn+F1–F12 send the functions printed on the keycaps: media player, volume down, volume up, mute, stop, previous,
    play/pause, next, mail, browser home, calculator and search.
  - Fn+Win toggles Win Lock, which blocks the Win key; Win Lock is off after power-up.
  - Fn+Ctrl+Backspace clears the saved settings and then restarts the keyboard (see
    [Clearing the saved settings](../../../readme.md#clearing-the-saved-settings)).
- Indicators above the numpad: Num Lock, Caps Lock, Scroll Lock and Win Lock, lit by the host's lock state and by
  Win Lock, even while the key LEDs are off.

## VIA keymap

The default keymap running under VIA ([top-level readme](../../../readme.md#keymaps-default-and-via)). VIA definition:
[keymaps/via/redragon_k582_via.json](keymaps/via/redragon_k582_via.json). About 310 bytes of the EEPROM are left for
macros.

## Lighting

Lighting changes are saved 3 seconds after the last change and survive power loss; the LEDs blink briefly when a
save happens. The controls and modes are listed below.

| Setting | Next step | Previous step |
|---|---|---|
| Mode | Fn+Ins | Fn+Shift+Ins |
| Color | Fn+Home | Fn+Shift+Home |
| Saturation | Fn+PgUp | Fn+Shift+PgUp |
| Brightness | Fn+Del | Fn+Shift+Del |
| Speed | Fn+End | Fn+Shift+End |
| On/off | Fn+PgDn | |

| # | Mode | What it does |
|---|---|---|
| 1 | Static | One color on all keys |
| 2 | Rainbow | A static rainbow from left to right |
| 3 | Reversed rainbow | A static rainbow from right to left |
| 4 | Wave | A rainbow that moves from left to right |
| 5 | Reversed wave | A rainbow that moves from right to left |
| 6 | Color cycle | All keys cycle through colors together |
| 7 | Breathing | All keys fade in and out together |
| 8 | Rainbow rows | A static color for each row of keys |
| 9 | Wave rows | A rainbow of one color per row of keys that moves from top to bottom |
| 10 | Chevron | A rainbow that moves from left to right in a chevron (>) shape |
| 11 | Out-in | A rainbow that moves from the left and right edges toward the middle |
| 12 | Raindrops | One color on all keys; random keys change to a nearby color and keep it |
| 13 | Reactive | Keys illuminate when pressed and then fade |
| 14 | Heatmap | Keys illuminate when pressed and the color temperature indicates frequency |

## Flash space

The application region (0x2800–0xEFBF) has about 23.5 KB free with the `default` keymap, and about 12.7 KB below the
store with the `via` keymap. The features turned off for flash space are listed in [config.h](config.h) and
[rules.mk](rules.mk).

## Hardware

From the stock firmware V0107 (static analysis):

- Key matrix: 7 rows × 16 columns. Rows P4.0–P4.6; columns P0.0–P0.7, P3.0–P3.7. The numpad is in row 6, except
  numpad 1 at (4, 15). The stock key table also has ISO, ABNT2 and JIS positions (left of Z (4, 1), ISO # (3, 12),
  ABNT2 /? (4, 12), numpad comma (7, 15), JIS keys in rows 5 and 7); only the ANSI layout is defined here.
- LEDs: the CH555 LED controller, with red, green and blue rows on P4, P2 and P1 and common lines on P0 and P3,
  shared with the key matrix. LED row n on common line c is the LED of the key in matrix row n, column c. LED row 7
  is Num Lock (common line 8, green) plus Caps Lock, Win Lock and Scroll Lock (common line 9: red, green and blue).
- No GPIO indicators, roller or side lights; nothing on P5 or P7.
- The stock firmware runs at 32 MHz, as this firmware does.
