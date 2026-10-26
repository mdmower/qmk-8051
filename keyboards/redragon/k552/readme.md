# Redragon K552RGB-2

Some revisions of the Redragon K552RGB-2 (K552 Kumara RGB) use an Evision VS11K28A MCU, an 8051 microcontroller in the WCH CH555 family. QMK is flashed through the stock bootloader, which is left in place.

- Maintainer: [mdmower](https://github.com/mdmower)
- Status: Untested

Procedures for backing up the stock firmware, building and flashing QMK, and restoring the stock firmware (if needed) are as described in the [top-level readme](../../../readme.md#getting-started).

## Keyboard verification

- This fork of QMK only supports the Evision VS11K28A revision of the Redragon K552. Other revisions of the K552 exist with a Sonix MCU; those revisions are not supported by this fork. Make sure your K552RGB-2 has USB VID:PID `320f:5000` before continuing:
    ```sh
    python -I util/evision_flash.py status
    ```
- The backup prints whether the boot marker `AA 55 33 CC` is at 0x27C0. If it is not, do not flash.

## Quick start

See the [top-level readme](../../../readme.md#getting-started) for complete instructions. The short version is:

```sh
# Build default image
python -I util/ch555_build.py redragon/k552

# Reboot from stock to bootloader
python -I util/evision_flash.py enter --send

# Flash QMK
python -I util/evision_flash.py flash redragon_k552_default.bin --expect 5244:0552 --send
```

## Getting to the bootloader

You can reach the Evision bootloader (USB VID:PID `0c45:7687`) by:

- Pressing Fn+Esc while QMK is running
- Holding Esc while plugging in the keyboard
- Running `python -I util/evision_flash.py enter --send` while the stock firmware is running

The keyboard remains at the bootloader until a flash completes, even if you replug the USB cable.

## Default keymap

- Layer 0: Matches the printed ANSI layout on stock keycaps (QMK layout `tkl_ansi`).
- Layer 1 (Fn):
    - Fn+Esc enters the bootloader (`QK_BOOT`).
    - Fn+F1–F12 match the functions printed on the keycaps: Fn+F1→media player, Fn+F2→volume down, Fn+F3→volume up, Fn+F4→mute, Fn+F5→stop, Fn+F6→previous, Fn+F7→play/pause, Fn+F8→next, Fn+F9→mail, Fn+F10→browser home, Fn+F11→calculator, and Fn+F12→search.
    - Fn+Win toggles Win Lock, which blocks the Win key.
    - Fn+Ctrl+Backspace clears saved settings and then restarts the keyboard, see [Clearing saved settings](../../../readme.md#clearing-saved-settings).
- Caps Lock and Scroll Lock LEDs (red and green) to the right of the logo. They light even while the key LEDs are off. All LEDs turn off while the host is suspended. The K552 has no Num Lock or Win Lock LED.

## VIA keymap (`via`)

The VIA keymap uses the same layers as the default keymap. About 1420 bytes of the EEPROM are available for macros. The LEDs blink briefly when settings are saved.

VIA definition: [keymaps/via/redragon_k552_via.json](keymaps/via/redragon_k552_via.json).

**VIA settings are stored in code flash. They are overwritten (reset) whenever you flash a new image. Be sure to save your customizations using VIA's save/load feature before flashing a new image!**

## Lighting

Lighting changes are saved 3 seconds after the last change and are stored in a dedicated data flash region that survives (is not erased) when flashing new images.

| Setting    | Next step | Previous step |
| ---------- | --------- | ------------- |
| Mode       | Fn+Ins    | Fn+Shift+Ins  |
| Color      | Fn+Home   | Fn+Shift+Home |
| Saturation | Fn+PgUp   | Fn+Shift+PgUp |
| Brightness | Fn+Del    | Fn+Shift+Del  |
| Speed      | Fn+End    | Fn+Shift+End  |
| On/off     | Fn+PgDn   |               |

| #   | Mode             | What it does                                                               |
| --- | ---------------- | -------------------------------------------------------------------------- |
| 1   | Static           | One color on all keys                                                      |
| 2   | Rainbow          | A static rainbow from left to right                                        |
| 3   | Reversed rainbow | A static rainbow from right to left                                        |
| 4   | Wave             | A rainbow that moves from left to right                                    |
| 5   | Reversed wave    | A rainbow that moves from right to left                                    |
| 6   | Color cycle      | All keys cycle through colors together                                     |
| 7   | Breathing        | All keys fade in and out together                                          |
| 8   | Rainbow rows     | A static color for each row of keys                                        |
| 9   | Wave rows        | A rainbow of one color per row of keys that moves from top to bottom       |
| 10  | Chevron          | A rainbow that moves from left to right in a chevron (>) shape             |
| 11  | Out-in           | A rainbow that moves from the left and right edges toward the middle       |
| 12  | Raindrops        | One color on all keys; random keys change to a nearby color and keep it    |
| 13  | Reactive         | Keys illuminate when pressed and then fade                                 |
| 14  | Heatmap          | Keys illuminate when pressed and the color temperature indicates frequency |

## Flash space

Remaining free space in the application region (0x2800–0xEFBF) for each keymap:

- `default`: 23.7 KB
- `via`: 10.2 KB before the code flash store

The code flash store starts at 0xDF40 in the `via` keymap. Features that have been turned off to save space are listed in [config.h](config.h) and [rules.mk](rules.mk).

## Hardware

From static analysis of the stock firmware V0101:

- Key matrix: 6 rows × 16 columns. Rows P4.0–P4.5; columns P0.0–P0.7, P3.0–P3.7.
    - The stock key table also has ISO, ABNT2, and JIS positions (left of Z (4, 1), ISO # (3, 12), ABNT2 /? (4, 12), JIS keys in row 5); only the ANSI layout is defined here.
- LEDs: the CH555 LED controller.
    - Red, green, and blue rows on P4, P2, and P1; common lines on P0 and P3, shared with the key matrix.
    - LED row n on common line c is the LED of the key in matrix row n, column c.
    - LED row 7 is Caps Lock (common line 9, red) and Scroll Lock (common line 8, green).
    - Common line 10 is in the stock lighting effects, between Esc and F1.
    - The keys are read in the LED controller's interrupt, on each common line ([led_dma.c](../common/led_dma.c)).
- No GPIO indicators, roller, or side lights; nothing on P5 or P7.
- The stock firmware runs at 48 MHz, a clock the CH555 datasheet lists for tests only. QMK runs at 32 MHz.
