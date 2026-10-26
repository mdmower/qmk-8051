# Redragon K580RGB

Some revisions of the Redragon K580RGB (K580 Vata) use an Evision VS11K33A MCU, an 8051 microcontroller in the WCH CH555 family. QMK is flashed through the stock bootloader, which is left in place.

- Maintainer: [mdmower](https://github.com/mdmower)
- Status: Tested on Linux and Windows 11

Procedures for backing up the stock firmware, building and flashing QMK, and restoring the stock firmware (if needed) are as described in the [top-level readme](../../../readme.md#getting-started).

## Keyboard verification

- This fork of QMK only supports the Evision VS11K33A revision of the Redragon K580. Other revisions of the K580 exist with a Sonix MCU; those revisions are not supported by this fork. Make sure your K580RGB has USB VID:PID `320f:5000` before continuing:
    ```sh
    python -I util/evision_flash.py status
    ```
- The backup prints whether the boot marker `AA 55 33 CC` is at 0x27C0. If it is not, do not flash.

## Quick start

See the [top-level readme](../../../readme.md#getting-started) for complete instructions. The short version is:

```sh
# Build default image
python -I util/ch555_build.py redragon/k580

# Reboot from stock to bootloader
python -I util/evision_flash.py enter --send

# Flash QMK
python -I util/evision_flash.py flash redragon_k580_default.bin --expect 5244:0580 --send
```

## Getting to the bootloader

You can reach the Evision bootloader (USB VID:PID `0c45:7687`) by:

- Pressing Fn+Esc while QMK is running
- Holding Esc while plugging in the keyboard
- Running `python -I util/evision_flash.py enter --send` while the stock firmware is running

The keyboard remains at the bootloader until a flash completes, even if you replug the USB cable.

## Default keymap

- Layer 0: Matches the printed layout on stock keycaps with the exception of G1–G5 and Rec.
    - G1→F13, G2→F14, G3→F15, G4→F16, G5→F17, and Rec→F18
    - The volume/brightness toggle works like stock, alternating the roller between volume adjustments and key brightness adjustments. The keyboard remembers how you last toggled it.
    - The previous, play/pause, and next media keys work like stock.
- Layer 1 (Fn):
    - Fn+Esc enters the bootloader (`QK_BOOT`).
    - Fn+F1–F12 match the functions printed on the keycaps: Fn+F1→media player, Fn+F2→volume down, Fn+F3→volume up, Fn+F4→mute, Fn+F5→stop, Fn+F6→previous, Fn+F7→play/pause, Fn+F8→next, Fn+F9→mail, Fn+F10→browser home, Fn+F11→calculator, and Fn+F12→search.
    - Fn+Win toggles Win Lock, which blocks the Win key and lights the Win Lock LED.
    - Fn+R turns the Rec LED on and off, see [Lighting](#lighting).
    - Fn+Ctrl+Backspace clears saved settings and then restarts the keyboard, see [Clearing saved settings](../../../readme.md#clearing-saved-settings).
- Num Lock, Caps Lock, Scroll Lock, and Win Lock LEDs. They and the key LEDs turn off while the host is suspended.

## VIA keymap (`via`)

The VIA keymap uses the same layers as the default keymap. About 1240 bytes of the EEPROM are available for macros. The LEDs blink briefly when settings are saved.

VIA definition: [keymaps/via/redragon_k580_via.json](keymaps/via/redragon_k580_via.json).

**VIA settings are stored in code flash. They are overwritten (reset) whenever you flash a new image. Be sure to save your customizations using VIA's save/load feature before flashing a new image!**

The roller is encoder 0 and can be customized per layer. VIA's encoder panel shows the roller's default actions (custom keys) as `CUSTOM(n)`; the key panels show their names.

## Macro keymap (`macro`)

The macro keymap is the most similar to stock. The difference from the default layout is that G1–G5 and Rec have built-in functionality to record and play macros, without VIA.

Each macro can hold up to 200 presses and releases (100 keystrokes) of keys, modifiers, and media keys (including Fn+F1–F12). Lighting keys, Win Lock, Fn+Esc, G keys, and the roller are not recorded. The LEDs blink briefly when macros are saved.

**Macros are stored in code flash. They are overwritten (reset) whenever you flash a new image. Be sure to save your macros using the backup procedure below before flashing a new image!**

**Record a macro**

1. Press Rec. The Rec key lights up and the G keys start blinking.
1. Press the G key where you want to record the macro. That G key stays lit, the others go dark, and the Rec key starts blinking.
1. Type your macro.
    - If your macro reaches the maximum length allowed, recording ends automatically.
1. Press Rec to stop recording and save changes. The G and Rec keys return to their default lighting.

**Play a macro**

Press the G key. Playback sends one key press or release every 10 ms. A G key without a macro sends nothing.

**Clear a macro**

Follow the same procedure as for recording a macro, but skip step 3 (i.e. don't type anything during recording).

### Back up and restore macros

Before flashing a new image, back up the EEPROM from your current `macro` keymap:

```sh
python -I util/evision_flash.py backup-settings k580_macros.bin --send
```

So long as the new `macro` image you are flashing uses the same store layout, you can specify the backup file with the `--settings` argument:

```sh
python -I util/evision_flash.py flash redragon_k580_macro.bin --settings k580_macros.bin --expect 5244:0580 --send
```

## Lighting

Key lights and side lights are controlled independently. Lighting changes are saved 3 seconds after the last change and are stored in a dedicated data flash region that survives (is not erased) when flashing new images.

The G1–G5, Rec, and media keys have fixed colors (no RGB) with a brightness that matches the RGB keys' brightness. In the `default` and `via` keymaps, the Rec key is not lit by default; it can be toggled on with Fn+R to follow the lighting of its neighbor G1–G5 keys. In the `macro` keymap, G1–G5 and Rec show the recorder's state while you choose a G key and while recording; Rec is dark otherwise (no Fn+R toggle).

### Key lighting

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

### Side lighting

| Setting    | Next step | Previous step |
| ---------- | --------- | ------------- |
| Mode       | Fn+G1     | Fn+Shift+G1   |
| Color      | Fn+G2     | Fn+Shift+G2   |
| Saturation | Fn+G3     | Fn+Shift+G3   |
| Brightness | Fn+G4     | Fn+Shift+G4   |
| Speed      | Fn+G5     | Fn+Shift+G5   |
| On/off     | Fn+Rec    |               |

| #   | Mode             | What it does                            |
| --- | ---------------- | --------------------------------------- |
| 1   | Static           | One color for the whole strip           |
| 2   | Rainbow          | A static rainbow from back to front     |
| 3   | Reversed rainbow | A static rainbow from front to back     |
| 4   | Wave             | A rainbow that moves from back to front |
| 5   | Reversed wave    | A rainbow that moves from front to back |
| 6   | Color cycle      | The whole strip cycles through colors   |
| 7   | Breathing        | The whole strip fades in and out        |

## Flash space

Remaining free space in the application region (0x2800–0xEFBF) for each keymap:

- `default`: 19.2 KB
- `via`: 4.5 KB before the code flash store
- `macro`: 11.7 KB before the code flash store

The code flash store starts at 0xDF40 in the `via` and `macro` keymaps. Features that have been turned off to save space are listed in [config.h](config.h) and [rules.mk](rules.mk).

## Hardware

- Key matrix: 7 rows × 18 columns. Rows P4.0–P4.6; columns P0.0–P0.7, P3.0–P3.7, P7.0–P7.1.
- Port 5: P5.5/P5.6 volume roller; P5.4 indicator LED enable (active low); P5.3, P5.2, P5.1, and P5.0 are the Num Lock, Caps Lock, Scroll Lock, and Win Lock LEDs (high = on).
- Key LEDs and side lights: the CH555 LED controller.
    - Red, green, and blue rows on P4, P2, and P1; common lines on P0, P3, and P7, shared with the key matrix.
    - LED row n on common line c is the LED of the key in matrix row n, column c (rows 0–6).
    - LED row 7 on common lines 0–15 drives the side lights.
    - G1–G5 (white), Rec and the media keys (red) have one LED each, on the red row.
    - The keys are read in the LED controller's interrupt, on each common line ([led_dma.c](../common/led_dma.c)).
- The stock firmware 1.04 runs at 32 MHz.
