# QMK for CH555-family 8051 keyboards

A branch of [QMK](https://github.com/qmk/qmk_firmware) (`develop`) that builds QMK for 8051 microcontrollers in
the WCH CH555 family with the SDCC compiler. It supports the Redragon K580 RGB (Evision VS11K33A), flashed
through the keyboard's own Evision bootloader.

The code comes from [mdmower/vial-qmk-8051](https://github.com/mdmower/vial-qmk-8051) (branch `k580-ch55x`), a
fork of vial-qmk that keeps the full commit history of the original 8051 port and of the K580 work. Here that
code is a new series of commits on QMK `develop`; their `Co-authored-by` lines name the authors of the original
code.

## Credits

- **vuhuycan**, [vuhuycan/qmk-8051](https://github.com/vuhuycan/qmk-8051): the port of vial-qmk to the 8051 and
  SDCC, including the MCS-51 platform code and the CH555 USB stack.
- **TechnoClasher**, [RakeRaven/qmk-8051](https://github.com/RakeRaven/qmk-8051): the Redragon K580 keyboard
  definition, changes to the CH555 platform, and fixes to the CH555 USB stack, NKRO and the lock LEDs.
- **Glitch752**, [Glitch752/qmk-8051-vs11k28a](https://github.com/Glitch752/qmk-8051-vs11k28a): reverse
  engineering of the Evision OEM bootloader's flash protocol.

## Status

Experimental. The firmware has been tested on **one** keyboard, a Redragon K580 RGB with an Evision VS11K33A MCU
and stock firmware 1.04. Other keyboards, other K580 revisions and macOS have not been tested.

| Keyboard | MCU | Flashed through | State |
|---|---|---|---|
| [Redragon K580 RGB](keyboards/redragon/k580/) | Evision VS11K33A | Evision OEM bootloader (`0c45:7687`) | Works; tested on one keyboard |

## Firmware options for the K580

The K580 has two keymaps. Both contain the same layout, Fn layer and lighting.

| | `default` (recommended) | `via` (at your own risk) |
|---|---|---|
| Keymap changes | In the source; build and flash again | Live, in [VIA](https://usevia.app) (3 layers, macros) |
| Saved at power-off | Lighting settings and roller mode (data flash) | The same, and QMK's settings, the keymap and the macros (code flash, 0xE740–0xEFBF) |
| Code flash written by the firmware | Only the boot marker page, to enter the bootloader | Also the EEPROM store at 0xE740–0xEFBF |
| Tested with | Linux and Windows 11 | Linux |

**The `via` keymap writes to code flash while the keyboard runs.** The CH555 enables writes to all of code
flash at once, including the Evision bootloader at 0x0000–0x27FF. The store code writes only its own pages and
checks the address before and after it enables writes, but a fault in this code, or in the firmware around it,
could erase or damage the bootloader. No tested way exists to restore the bootloader: the keyboard could then no
longer be flashed or used. Use the `default` keymap unless you accept this risk.

Details, build and flash steps for both: [keyboards/redragon/k580/readme.md](keyboards/redragon/k580/readme.md).

### Redragon K580 RGB

Works:
- All keys of the main block, the navigation cluster and the numpad; NKRO.
- Fn layer: Fn+Esc enters the bootloader, Fn+F1–F12 send the media and application keys printed on the keycaps,
  Fn+Win toggles Win Lock.
- Media keys and the volume roller. The roller toggle key switches the roller between volume and key LED
  brightness.
- G1–G5 and Rec send F13–F18, for shortcuts set in the operating system.
- Num Lock, Caps Lock, Scroll Lock and Win Lock LEDs. They turn off while the host is suspended.
- Key LEDs (RGB; one colour on G1–G5, Rec and the media keys) and side lights, driven by the CH555 LED
  controller, with separate effect settings for the keys and the side lights, set on the Fn layer. The LEDs turn
  off while the host is suspended.
- The lighting settings and the roller mode are saved in the CH555 data flash and kept at power-off.
- Holding Esc while plugging in the keyboard enters the bootloader.
- USB serial number: the chip ID, or a serial number set at build time.
- USB: one device (`5244:0580`) with three HID interfaces: boot keyboard, raw HID, and NKRO keyboard with system
  and media keys.
- `via` keymap: key and roller remapping in VIA, kept at power-off. VIA macros are untested.

Not implemented:
- QMK's `rgb_matrix` and the VIA lighting controls: the K580 has its own lighting code.
- Vial: it needs the vial-qmk fork.
- In the `default` keymap, QMK's EEPROM: QMK settings kept in EEPROM (eeconfig) last until power-off.

Other limits:
- In the `default` build about 14 KB of the application region (0x2800–0xEFBF, 51,136 bytes) is free; the `via`
  build leaves about 2 KB below its EEPROM store.
- To save flash, the K580 build turns off tap and one-shot keys, Grave Escape, Magic keycodes, Space Cadet and,
  in the `default` keymap, `send_string`; the lists are in
  [keyboards/redragon/k580/config.h](keyboards/redragon/k580/config.h) and
  [keyboards/redragon/k580/rules.mk](keyboards/redragon/k580/rules.mk). VIA offers tap keys (`LT`, `MT`); they do
  nothing on this build.
- The backup and flash tools (`util/k580_dump.py`, `util/evision_flash.py`) run on Linux only.
- Other K580 revisions may use a different MCU; one keyboard database lists the K580 with a Sonix SN32F248B.
  [keyboards/redragon/k580/readme.md](keyboards/redragon/k580/readme.md) describes how to check a keyboard
  before flashing.

## Building

The toolchain is SDCC 4.6.0 or 4.4.0, clang with clang-query and llvm-nm, binutils and the QMK CLI. SDCC 4.5.0
stops with an internal error on the CH555 USB code (SDCC bug #3894).

- `util/k580_build.sh [KEYMAP]` runs the K580 build in Docker and writes `redragon_k580_<keymap>.bin`, the image
  for the Evision bootloader.
- With the toolchain installed, `qmk compile -kb redragon/k580 -km default` (or `-km via`) makes the same image.

## Toolchain limits

These limits of SDCC and of this port affect keyboard and keymap code:

- `$(CC)` is [platforms/mcs51/sdcc_cc.py](platforms/mcs51/sdcc_cc.py), a GCC-style driver for SDCC. It uses
  clang to find weak definitions, which SDCC does not have, and resolves them at the link.
- SDCC 4.6.0 compiles some C to wrong code without a diagnostic. The driver stops the build at these constructs
  ([platforms/mcs51/sdcc_check.query](platforms/mcs51/sdcc_check.query)): a struct or union returned from
  anything but a variable, one struct variable returned by two return statements, a struct-valued `?:` with a
  call operand, the address of a compound literal at file scope, a designated initializer that names a member of
  an anonymous struct or union, and arithmetic on a void pointer.
- SDCC rejects some GNU C that GCC accepts (case ranges, casts to union types, empty initializers, variable-length
  arrays). The QMK core is edited in the files the K580 build compiles; other features may need the same edits.
- SDCC compiles a copy of every `static inline` function of a header into each file that includes the header,
  used or not. Use a macro, or define the function once in a `.c` file.
- SDCC keeps values that do not fit in registers in direct RAM (0x08–0x7F), which the K580 build fills almost
  completely. In VIA builds some files are compiled with fewer optimizations to fit (`MCS51_FEWER_SPILLS_SRC` in
  [platforms/mcs51/platform.mk](platforms/mcs51/platform.mk)).
- Pins are named `PORT<port>_<pin>` (for example `PORT0_1`). `keyboard.json` uses `"processor": "ch555"` and
  `"bootloader": "evision-oem"` (an Evision OEM bootloader at 0x0000–0x27FF) or `"ch55xbl"` (the WCH factory ISP
  bootloader; untested).
- SDCC puts an interrupt handler into its vector table only if `main.c` sees its declaration: a keyboard adds the
  header that declares its handler with `MCS51_ISR_HEADERS += <header>` in `rules.mk`.
