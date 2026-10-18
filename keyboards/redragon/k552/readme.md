# Redragon K552RGB-2

QMK for the Redragon K552 Kumara RGB, board K552RGB-2, with an Evision VS11K28A MCU (an 8051 in the WCH CH555
family). The firmware is flashed through the keyboard's own Evision bootloader, which stays in place, as on the
[K580](../k580/readme.md).

**Not run on a keyboard yet.** The key matrix, the LED layout and the indicators come from the stock firmware
V0101 (Redragon's updater for the K552RGB-1/-2); the firmware is tested only in a simulator. The LED controller,
key matrix and lighting code are shared with the K580 ([common](../common/)), where they are tested.

Two keymaps: `default` and `via` (at your own risk, as on the K580).

## Check the keyboard first

- With the stock firmware, the keyboard enumerates as `320f:5000` (`lsusb`), the same ID as the K580. The
  updater's firmware reports bcdDevice 0x0101 (V0101).
- The K552RGB-1 has a different MCU (Evision VS11K09A, a Sonix SN32; `0c45:5004`). This firmware does not apply
  to it.
- The backup in step 1 prints whether the boot marker `AA 55 33 CC` is at 0x27C0. If it is not, the flash layout
  does not apply: do not flash.
- The bootloader does not identify the keyboard (the K580 has the same `0c45:7687`): flash only images built for
  `redragon/k552`.

## Backup, build, flash, restore

As for the K580 ([readme](../k580/readme.md)), with these names:

```
python3 -I util/evision_dump.py --out k552_stock_1.bin --send         # backup (stock firmware; same read command)
python3 -I util/evision_flash.py extract k552_stock_1.bin k552_stock_app.bin
util/ch555_build.sh redragon/k552                                      # or: qmk compile -kb redragon/k552 -km default
python3 -I util/evision_flash.py enter --send                          # stock firmware -> bootloader
python3 -I util/evision_flash.py flash redragon_k552_default.bin --expect 5244:0552 --send
```

Over QMK, enter the bootloader with Fn+Esc, or hold Esc while plugging in the keyboard (Esc is at matrix
position (0, 0), as on the K580). To restore the stock firmware, flash `k552_stock_app.bin` from the bootloader.

## Default keymap

- Layer 0: the printed ANSI layout (QMK layout `tkl_ansi`).
- Layer 1 (Fn): Fn+Esc enters the bootloader (`QK_BOOT`). Fn+F1–F12 send the functions printed on the keycaps:
  media player, volume down, volume up, mute, stop, previous, play/pause, next, mail, browser home, calculator,
  search. Fn+Win toggles Win Lock, which blocks the Win key. Win Lock is off after power-up.
- Lighting ([led_fx.c](../common/led_fx.c)): one zone, the key LEDs, with the K580's keys. Fn plus a key below
  steps a setting; with Shift held it steps back. The settings are saved in the data flash 3 s after the last
  change.

  | Setting | Keys |
  |---|---|
  | Mode | Fn+Ins |
  | Hue (24 steps) | Fn+Home |
  | Saturation (5 steps; the lowest is white) | Fn+PgUp |
  | Brightness (8 steps) | Fn+Del |
  | Speed (5 steps) | Fn+End |
  | On/off | Fn+PgDn |

  Modes: static colour, rainbow, reversed rainbow, wave, reversed wave, colour cycle, breathing. Default: wave.
- Caps Lock and Scroll Lock indicators (right of the logo): red and green, lit by the host's lock state also
  while the key LEDs are off.

The other lighting legends on the keycaps (Ins–PgDn 1–6, −/= speed, arrows brightness, direction and colour) are
not used, as on the K580: Fn+Ins–PgDn set everything. Not implemented either: per-key colour editing (Fn+\`) and
factory reset (Fn+PrtSc).

## VIA keymap (at your own risk)

The same as on the K580 (see its readme for the risk and how the store works). VIA definition:
[keymaps/via/redragon_k552_via.json](keymaps/via/redragon_k552_via.json); the keyboard is `5244:0552`.

## Hardware

From the stock firmware V0101 (static analysis):

- Key matrix: 6 rows × 16 columns. Rows P4.0–P4.5; columns P0.0–P0.7, P3.0–P3.7. The stock key table also has
  ISO, ABNT2 and JIS positions (left of Z (4, 1), ISO # (3, 12), ABNT2 /? (4, 12), JIS keys in row 5); only the
  ANSI layout is defined here.
- LEDs: the CH555 LED controller, with red, green and blue rows on P4, P2 and P1 and common lines on P0 and P3,
  shared with the key matrix. LED row n on common line c is the LED of the key in matrix row n, column c. LED row
  7: Caps Lock (common line 9, red) and Scroll Lock (common line 8, green); common line 10 is in the stock
  lighting effects, between Esc and F1.
- No GPIO indicators, roller or side lights; nothing on P5 or P7.
- The stock firmware runs at 48 MHz, which the CH555 datasheet lists for tests only; this firmware runs at
  32 MHz, as the K580 and the K589 (also a VS11K28A).
