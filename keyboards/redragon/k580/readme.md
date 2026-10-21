# Redragon K580 RGB

QMK for the Redragon K580 RGB (K580 Vata) with an Evision VS11K33A MCU, an 8051 in the WCH CH555 family. The
firmware is flashed through the keyboard's own Evision bootloader, which stays in place.

* Keyboard maintainer: [mdmower](https://github.com/mdmower)
* Hardware supported: Redragon K580 RGB with an Evision VS11K33A MCU
* State: works; tested on one keyboard (stock firmware 1.04), on Linux and Windows 11

Backup, flashing, restore, the `via` keymap and the risks of flashing:
[top-level readme](../../../readme.md#before-you-flash). Make the backup before the first flash: it reads the
flash through the stock firmware, which the flash erases.

## Check the keyboard first

- With the stock firmware, the keyboard enumerates as `320f:5000` (`python3 -I util/evision_flash.py status`). The
  tested keyboard reports bcdDevice 0x0104 (firmware 1.04). Other Redragon keyboards report the same ID: check
  that the keyboard is a K580.
- Other K580 revisions may use a different MCU; one keyboard database lists the K580 with a Sonix SN32F248B. This
  firmware does not apply to them.
- The backup prints whether the boot marker `AA 55 33 CC` is at 0x27C0. If it is not, do not flash.

## Build and flash

```
python3 -I util/ch555_build.py redragon/k580      # or: qmk compile -kb redragon/k580 -km default
python3 -I util/evision_flash.py enter --send     # from the stock firmware; from QMK: Fn+Esc
python3 -I util/evision_flash.py flash redragon_k580_default.bin --expect 5244:0580 --send
```

## Bootloader

Enter the Evision bootloader (`0c45:7687`) in one of these ways:

* Fn+Esc (`QK_BOOT` on layer 1).
* Hold Esc while plugging in the keyboard. This check runs before QMK starts, so it also works when a QMK build
  does not start.
* With the stock firmware: `python3 -I util/evision_flash.py enter --send`.

The keyboard stays in the bootloader, also after a replug, until a flash completes.

## Default keymap

- Layer 0: the printed layout. Media keys: previous, play/pause, next. Roller: volume; the roller toggle key
  switches it to key LED brightness and back. The roller mode is saved like the lighting settings.
- Layer 1 (Fn): Fn+Esc enters the bootloader (`QK_BOOT`). Fn+F1–F12 send the functions printed on the keycaps:
  media player, volume down, volume up, mute, stop, previous, play/pause, next, mail, browser home, calculator,
  search. Fn+Win toggles Win Lock, which blocks the Win key and lights the Win Lock LED. Win Lock is off after
  power-up.
- G1–G5 and Rec: F13–F17 and F18, for shortcuts set in the operating system.
- Num Lock, Caps Lock, Scroll Lock and Win Lock LEDs. They and the key LEDs turn off while the host is suspended.

## VIA keymap

The default keymap with VIA ([top-level readme](../../../readme.md#keymaps-default-and-via)). VIA definition:
[keymaps/via/redragon_k580_via.json](keymaps/via/redragon_k580_via.json). The roller is encoder 0, with its own
action on each layer. VIA's encoder panel shows the roller's default actions (custom keys) as `CUSTOM(n)`; the key
panels show their names. About 215 bytes of the EEPROM are left for macros.

## Macro keymap

The default keymap with a macro recorder on G1–G5 and Rec, in place of F13–F18
([keymaps/macro/keymap.c](keymaps/macro/keymap.c)). Build it with `python3 -I util/ch555_build.py redragon/k580 macro`.

- Record: Rec, then a G key, then the keys of the macro, then Rec. While you choose the G key, Rec is lit and the G
  keys blink; Rec or Esc cancels. While you record, Rec blinks and only the chosen G key is lit. The keys are sent as
  you type them.
- Play: the G key. Playback sends one key press or release every 10 ms. A G key without a macro sends nothing.
- Clear: Rec, the G key, Rec.
- A macro holds up to 98 presses and releases (49 keystrokes) of keys, modifiers and media keys (also Fn+F1–F12).
  The lighting keys, Win Lock, Fn+Esc and the roller are not recorded. A full macro ends the recording.
- The macros are kept at power-off in QMK's EEPROM, which this keymap keeps in code flash (0xE740–0xEFBF), as the
  `via` keymap does ([top-level readme](../../../readme.md#before-you-flash)). Every flash erases them. The EEPROM
  is saved 2 s after a recording ends; the LEDs flicker while it is saved.
- Outside a recording, Rec is dark and G1–G5 light at the key LEDs' brightness.

## Lighting

Lighting controls and modes are listed below. Key and side lights are controlled independently. Lighting updates
are saved 3 seconds after the last change and survive power loss. LEDs blink briefly when settings are saved.

The G1–G5, Rec, and media keys have fixed colors (no RGB). Their brightness matches the RGB key brightness. In the
`macro` keymap, G1–G5 and Rec show the macro recorder's state while recording, and Rec is dark otherwise.

### Key lighting

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

### Side lighting

| Setting | Next step | Previous step |
|---|---|---|
| Mode | Fn+G1 | Fn+Shift+G1 |
| Color | Fn+G2 | Fn+Shift+G2 |
| Saturation | Fn+G3 | Fn+Shift+G3 |
| Brightness | Fn+G4 | Fn+Shift+G4 |
| Speed | Fn+G5 | Fn+Shift+G5 |
| On/off | Fn+Rec | |

| # | Mode | What it does |
|---|---|---|
| 1 | Static | One color for the whole strip |
| 2 | Rainbow | A static rainbow from back to front |
| 3 | Reversed rainbow | A static rainbow from front to back |
| 4 | Wave | A rainbow that moves from back to front |
| 5 | Reversed wave | A rainbow that moves from front to back |
| 6 | Color cycle | The whole strip cycles through colors |
| 7 | Breathing | The whole strip fades in and out |

## Flash space

The application region (0x2800–0xEFBF) has about 19.5 KB free with the `default` keymap. Below the store at 0xE740,
it has about 7.7 KB free with the `via` keymap and about 14.5 KB with the `macro` keymap. The features turned off
for flash space are listed in [config.h](config.h) and [rules.mk](rules.mk).

## Hardware

- Key matrix: 7 rows × 18 columns. Rows P4.0–P4.6; columns P0.0–P0.7, P3.0–P3.7, P7.0–P7.1.
- Port 5: P5.5/P5.6 volume roller; P5.4 indicator LED enable (active low); P5.3, P5.2, P5.1, P5.0 Num Lock,
  Caps Lock, Scroll Lock, Win Lock LEDs (high = on).
- Key LEDs and side lights: the CH555 LED controller, with red, green and blue rows on P4, P2 and P1 and common
  lines on P0, P3 and P7, shared with the key matrix. LED row n on common line c is the LED of the key in matrix
  row n, column c (rows 0–6); LED row 7 on common lines 0–15 drives the side lights. G1–G5 (white), Rec and
  the media keys (red) have one LED each, on the red row. The keys are read in the LED controller's interrupt,
  on each common line ([led_dma.c](../common/led_dma.c)).
