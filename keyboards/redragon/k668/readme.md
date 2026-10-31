# Redragon K668 Trundle

The wired Redragon K668 RGB (K668 Trundle) uses a Sinowealth SH68F90A MCU, an 8051 microcontroller. QMK is flashed through the stock ISP bootloader, which is left in place.

- Maintainer: [mdmower](https://github.com/mdmower)
- Status: Tested on Linux and Windows 11

Procedures for backing up the stock firmware, building and flashing QMK, and restoring the stock firmware (if needed) are as described in the [top-level readme](../../../readme.md#getting-started). The K668 is backed up and flashed with `util/sinowealth_flash.py`.

## Keyboard verification

- Make sure your K668 has USB VID:PID `258a:0049` before continuing:
    ```sh
    python -I util/sinowealth_flash.py status
    ```
    Several other Redragon keyboards use the same ID, so also check that the label on the back of your keyboard says K668.
- Only the wired K668 has been tested. Wireless versions of the K668 are not supported.

## Quick start

See the [top-level readme](../../../readme.md#getting-started) for complete instructions. The short version is:

```sh
# Build default image
python -I util/mcs51_build.py redragon/k668

# Flash QMK (the script reboots the keyboard to the bootloader)
python -I util/sinowealth_flash.py flash redragon_k668_default.hex --expect 5244:0668 --send
```

## Getting to the bootloader

You can reach the Sinowealth ISP bootloader (USB VID:PID `0603:1020`) by:

- Pressing Fn+Esc while QMK is running
- Holding Esc while plugging in the keyboard (QMK only)
- Running `python -I util/sinowealth_flash.py` with the `backup --send ...` or `flash --send ...` arguments (QMK and stock)

## Default keymap

- Layer 0: Matches the printed ANSI layout on stock keycaps (QMK layout `fullsize_ansi`).
    - The four keys above the numpad are mute, calculator, lock screen (Win+L), and show desktop (Win+D). The lock screen and show desktop keys work even while Win Lock is on.
- Layer 1 (Fn):
    - Fn+Esc enters the bootloader (`QK_BOOT`).
    - Fn+F1 and Fn+F2 make the key lights darker and brighter.
    - Fn+F3–F12 match the functions printed on the keycaps: Fn+F3→mail, Fn+F4→calculator, Fn+F5→my computer, Fn+F6→media player, Fn+F7→previous, Fn+F8→play/pause, Fn+F9→next, Fn+F10→mute, Fn+F11→volume down, and Fn+F12→volume up.
    - Fn+Win toggles Win Lock, which blocks the Win keys and lights the Win Lock LED.
    - Fn+Ctrl+Backspace clears saved settings and then restarts the keyboard, see [Clearing saved settings](../../../readme.md#clearing-saved-settings).
    - The lighting keys are described in [Lighting](#lighting).
- Num Lock, Caps Lock, Scroll Lock, and Win Lock LEDs. They and the key/side LEDs turn off while the host is suspended.

The `default` keymap stores QMK's settings, such as whether Win Lock is on, and lighting settings (see [Lighting](#lighting)) in code flash.

## VIA keymap (`via`)

The VIA keymap uses the same layers as the default keymap, plus a third layer whose keys are all transparent until you change them. About 1220 bytes of the EEPROM are available for macros.

VIA definition: [keymaps/via/redragon_k668_via.json](keymaps/via/redragon_k668_via.json).

**VIA settings are stored in code flash. They are overwritten (reset) whenever you flash a new image. Be sure to save your customizations using VIA's save/load feature before flashing a new image!**

## Lighting

Key lights and side lights are controlled independently. Lighting changes are saved 2 seconds after the last change. They are stored in code flash, which every flash erases. See [Back up and restore settings](#back-up-and-restore-settings) for an option on how to retain them.

### Key lighting

| Setting    | Next step       | Previous step         |
| ---------- | --------------- | --------------------- |
| Mode       | Fn+Ins          | Fn+Shift+Ins          |
| Color      | Fn+Home         | Fn+Shift+Home         |
| Saturation | Fn+PgUp         | Fn+Shift+PgUp         |
| Brightness | Fn+Del or Fn+F2 | Fn+Shift+Del or Fn+F1 |
| Speed      | Fn+End          | Fn+Shift+End          |
| On/off     | Fn+PgDn         |                       |

| #   | Mode             | What it does                                                               |
| --- | ---------------- | -------------------------------------------------------------------------- |
| 1   | Static           | One color on all keys                                                      |
| 2   | Rainbow          | A static rainbow from left to right                                        |
| 3   | Reversed rainbow | A static rainbow from right to left                                        |
| 4   | Wave (default)   | A rainbow that moves from left to right                                    |
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

### Side lighting

The side lights run along the left side, the front edge, and the right side of the case. In the mode table, the directions run from the back left corner, across the front, to the back right corner.

| Setting    | Next step | Previous step |
| ---------- | --------- | ------------- |
| Mode       | Fn+Num7   | Fn+Shift+Num7 |
| Color      | Fn+Num8   | Fn+Shift+Num8 |
| Saturation | Fn+Num9   | Fn+Shift+Num9 |
| Brightness | Fn+Num4   | Fn+Shift+Num4 |
| Speed      | Fn+Num5   | Fn+Shift+Num5 |
| On/off     | Fn+Num6   |               |

| #   | Mode              | What it does                                              |
| --- | ----------------- | --------------------------------------------------------- |
| 1   | Static            | One color on all side lights                              |
| 2   | Rainbow (default) | A static rainbow from the back left to the back right     |
| 3   | Reversed rainbow  | A static rainbow from the back right to the back left     |
| 4   | Wave              | A rainbow that moves from the back left to the back right |
| 5   | Reversed wave     | A rainbow that moves from the back right to the back left |
| 6   | Color cycle       | All side lights cycle through colors together             |
| 7   | Breathing         | All side lights fade in and out together                  |

## Back up and restore settings

The code flash store holds the lighting settings, whether Win Lock is on, and QMK's other settings. In the `via` keymap, it also holds the keymap and the macros. A flash erases the store, but you can copy it from a backup into the new image.

Back up the keyboard while QMK is running. Wait 2 seconds after your last change, so that it is saved before the backup:

```sh
python -I util/sinowealth_flash.py backup k668_backup.bin --send
```

The script reboots the keyboard to the bootloader, reads the whole flash, and then restarts QMK. Then flash the new image, and give the backup with `--settings`:

```sh
python -I util/sinowealth_flash.py flash redragon_k668_default.hex --settings k668_backup.bin --expect 5244:0668 --send
```

The script copies the store from the backup into the image before it writes the image. **The backup and the new image must be of the same keymap.** If the backup was made from another keymap, you could end up with multiple stores, or worse, a store that overlaps application code.

Be aware that QMK's VIA code resets the keymap and the macros if the firmware was built on a different day than the firmware under which they were saved. It does this so that a new build never reads VIA data that an older build may have stored in a different format. Save and restore VIA settings via usevia.app.

## Known limitations

These are in addition to the limitations listed in the [top-level readme](../../../readme.md#known-limitations).

- **NKRO does not send F21–F24 or the international and language keys:** The NKRO report has room for every key of the K668. Keycodes above F20, such as F21–F24 and the international and language keys, are sent only while NKRO is off.
- **No console:** `print` and the QMK console are not available.

## Flash space

Remaining free space in the application region for each keymap:

- `default`: 27.3 KB before the code flash store
- `via`: 16.6 KB before the code flash store

The code flash store is at 0xEA00–0xEDFF in the `default` keymap and at 0xDE00–0xEDFF in the `via` keymap. Seven 24-byte ranges of the flash (0x10F0–0x1107 and the same offset in every 8 KB block up to 0xD0F0) hold no code: an instruction that accesses external RAM fails on this MCU at the last ten bytes of each range (0x10FE–0x1107 and the like), so the build keeps code out of the ranges and `sinowealth_flash.py check` refuses an image that has anything but 0xA5 there. Features that have been turned off to save space are listed in [config.h](config.h) and [rules.mk](rules.mk).

## Hardware

- MCU: Sinowealth SH68F90A at 24 MHz, 64 KB of code flash in 512-byte sectors (rated for 10,000 erase cycles per sector), 4 KB of xRAM. The stock firmware V1.6.6 reports USB bcdDevice 0x0105.
- Key matrix: 21 rows × 6 columns in QMK. Rows (driven low one at a time) P6.0–P6.7, P5.0–P5.2, P5.7, P4.0, P4.2, P4.3, P4.5, P4.6, P7.4, P0.5–P0.7; columns (inputs with pull-ups) P7.0–P7.3, P5.3, P5.4. Esc is row P6.0, column P7.0.
- Key LEDs and side lights: an LED matrix on the PWM channels.
    - The matrix rows are also the LED columns: an LED is lit while its matrix row is driven low.
    - The six key rows are driven by red, green, and blue PWM channels on P1.0–P1.5, P2.0–P2.5, and P3.0–P3.5. LED row r on matrix row n is the LED of the key in matrix row n, column r.
    - The 17 side lights are a seventh LED row on P7.5–P7.7, on matrix rows 0–16.
    - The keys are read in the PWM0 interrupt, once per matrix row ([led_pwm.c](led_pwm.c)).
- Indicator LEDs, active low: Num Lock P0.4, Caps Lock P0.3, Scroll Lock P0.2, and Win Lock P4.7.
- P5.5 and P5.6 are not used.
