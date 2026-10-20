# QMK for CH555-family 8051 keyboards

A branch of [QMK](https://github.com/qmk/qmk_firmware) (`develop`) that builds QMK with the SDCC compiler for
8051 microcontrollers in the WCH CH555 family. It supports Redragon keyboards with Evision MCUs of that
family. The firmware is flashed through the keyboard's own Evision bootloader, which stays in place, so the stock
firmware can be restored from a backup.

| Keyboard | MCU | USB ID: stock / QMK | State |
|---|---|---|---|
| [Redragon K580 RGB](keyboards/redragon/k580/) | Evision VS11K33A | `320f:5000` / `5244:0580` | Works; tested on one keyboard (Linux, Windows 11) |
| [Redragon K552RGB-2](keyboards/redragon/k552/) | Evision VS11K28A | `320f:5000` / `5244:0552` | Not run on a keyboard yet |
| [Redragon K551RGB-1](keyboards/redragon/k551/) | Evision VS11K28A | `320f:5000` / `5244:0551` | Not run on a keyboard yet |
| [Redragon K582RGB](keyboards/redragon/k582/) | Evision VS11K28A | `320f:5000` / `5244:0582` | Not run on a keyboard yet |
| [Redragon K589RGB](keyboards/redragon/k589/) | Evision VS11K28A | `320f:5000` / `5244:0589` | Not run on a keyboard yet |

The keyboards have the same Evision bootloader (`0c45:7687`) and share their LED controller, key matrix and
lighting code ([keyboards/redragon/common](keyboards/redragon/common/)). This readme describes what is common to
them: the risks, backup, flashing, restore, the `default` and `via` keymaps, and the build. The keyboard readmes
describe each keyboard's checks, keymap, lighting keys and hardware.

## Before you flash

- The firmware is experimental. It has been tested on one K580 with stock firmware 1.04. Other revisions of these
  keyboards and macOS have not been tested.
- Back up the stock firmware before the first flash ([step 1](#1-back-up-the-stock-firmware)). The backup can be
  made only while the stock firmware runs, because it reads the flash through a command of the stock firmware.
- The stock firmware of these keyboards (and of other Redragon keyboards) enumerates as `320f:5000`, and their
  bootloaders as `0c45:7687`. The bootloader does not identify the keyboard and accepts an image built for another
  one: check the keyboard as its readme describes, and flash only images built for it.
- Entering the bootloader erases the boot marker. The keyboard then stays in the bootloader, also after a replug,
  until a flash completes.
- The CH555 has no write protection for the Evision bootloader: one enable bit allows writes to all of code flash.
  Every firmware for these keyboards uses it. The stock firmware and QMK erase the boot marker page, the last page
  of the bootloader's region, to enter the bootloader (QMK: Fn+Esc, `QK_BOOT`, Esc held at plug-in). The K580's
  stock firmware 1.04 also keeps its settings in code flash, and so does the `via` keymap
  ([Keymaps](#keymaps-default-and-via)). A fault in one of these writers could damage the bootloader. The
  keyboard could then no longer be flashed, and no tested way exists to restore the bootloader.

## Requirements

- Python 3 (standard library only) for the backup and flash tools, on Linux or Windows.
- For the build: Docker (Linux) or Docker Desktop (Windows), or the toolchain in [Build with QMK](#build-with-qmk).
- The submodule `lib/lufa`: clone with `git clone --recurse-submodules`, or run
  `git submodule update --init lib/lufa` in an existing clone.
- Linux: access to the keyboard's hidraw nodes: install the udev rule and be a member of the `plugdev` group. The
  rule grants the stock firmware's vendor interface, the bootloader, and QMK's raw HID interface, which VIA uses.
  ```
  sudo cp util/udev/70-redragon-ch555.rules /etc/udev/rules.d/
  sudo udevadm control --reload && sudo udevadm trigger
  ```
- Windows: Python from [python.org](https://www.python.org/downloads/windows/) or the Microsoft Store. No driver
  and no setting is needed: any user can open the HID interfaces that the tools use. Smart App Control
  can block a DLL of an unsigned Python build; the tools then say so. Quit the Redragon software before a backup
  or a flash: it uses the same interface of the keyboard.

Run the commands below from the top directory of this repository: in a shell on Linux, in PowerShell on Windows.
On Windows, type `python` where the commands say `python3`: the python.org installer does not add a `python3`
command.

`util/evision_dump.py` and `util/evision_flash.py` send nothing to the keyboard unless `--send` is given; without it
they print what they would do. `python3 -I util/evision_flash.py status` shows which mode the keyboard is in: stock
firmware, bootloader or QMK.

## Flash layout

| Range | Contents |
|---|---|
| 0x0000–0x27FF | Evision bootloader (USB `0c45:7687`). `util/evision_flash.py` never writes it. |
| 0x27C0 | Boot marker `AA 55 33 CC`, in the last page of the bootloader's region. The bootloader starts the application only while the marker is present, and writes it after a successful flash. |
| 0x2800–0xEFBF | Application: stock firmware or QMK. The bootloader erases and programs this range on every flash. The `via` keymap keeps its store at 0xE740–0xEFBF. |
| 0xF000–0xF3FF | Data flash. QMK keeps the keyboard's own settings here (lighting; K580: roller mode). Flashing does not erase it. The K580's stock firmware 1.04 does not use it. |
| 0xF400– | WCH factory ISP bootloader (`4348:55e0`). Not used: flashing through it overwrites the Evision bootloader. |

A QMK image starts with a check for Esc at 0x2803 and contains the interrupt table that the bootloader jumps to at
0x3000; both are added by `util/evision_oem_image.py`. The Esc check runs before QMK starts, so holding Esc while
plugging in the keyboard enters the bootloader also when a QMK build does not start. It reads column P0.0 and row
P4.0, the Esc key on all of these keyboards.

## Back up, build, flash, restore

The commands use two shell variables for the keyboard. Set the variables relevant to your keyboard, for example:

```
KB=k580 QMK_ID=5244:0580    # K580 RGB
# OR
KB=k552 QMK_ID=5244:0552    # K552RGB-2
# OR
KB=k551 QMK_ID=5244:0551    # K551RGB-1
# OR
KB=k582 QMK_ID=5244:0582    # K582RGB
# OR
KB=k589 QMK_ID=5244:0589    # K589RGB
```

In PowerShell, set them as `$KB = "k580"; $QMK_ID = "5244:0580"`. The commands below are the same in both shells.

`KB` is the keyboard's directory under `keyboards/redragon/`, used in the build command and the file names below.
`QMK_ID` is the USB ID that QMK enumerates as on that keyboard; `flash --expect` waits for it.

### 1. Back up the stock firmware

```
python3 -I util/evision_dump.py --out ${KB}_stock.bin --send
python3 -I util/evision_flash.py extract ${KB}_stock.bin ${KB}_stock_app.bin
```

The stock firmware's replies to the read command contain no checksum of the data, so `evision_dump.py` reads the
flash twice and writes the file only if both reads are identical. If they differ, it says so; run it again.

The dump prints whether the boot marker `AA 55 33 CC` is at 0x27C0. If it is not, the flash layout above does not
apply: do not flash. `${KB}_stock.bin` is the full 64 KB code flash, including the bootloader.
`${KB}_stock_app.bin` is its application region, the image that restores the stock firmware. Keep both.

### 2. Build

```
python3 -I util/ch555_build.py redragon/$KB          # keymap default: redragon_${KB}_default.bin
python3 -I util/ch555_build.py redragon/$KB via      # keymap via: redragon_${KB}_via.bin
```

[Building](#building) describes the build without Docker and the USB serial number.

### 3. Flash QMK

From the stock firmware:

```
python3 -I util/evision_flash.py enter --send
python3 -I util/evision_flash.py flash redragon_${KB}_default.bin --expect $QMK_ID --send
```

`flash` checks the image before it erases anything, writes only 0x2800–0xEFBF, and then waits for the `--expect`
ID at the same USB port. If a flash fails or is interrupted, the keyboard stays in the bootloader and the command
can be run again.

Other Redragon keyboards use the same stock and bootloader IDs, which do not identify the keyboard: `enter`,
`flash` and `evision_dump.py` refuse to run when more than one keyboard has the ID they need, and print the USB
port of the keyboard they use.

QMK enumerates as one USB device (`$QMK_ID`) with three HID interfaces: boot keyboard, raw HID (used by VIA), and
NKRO keyboard with system and media keys.

To flash a new build over QMK, enter the bootloader with Fn+Esc, or hold Esc while plugging in the keyboard, then
run the `flash` command. With `--poll --send`, `flash` waits up to 15 s for the bootloader: start it first, then
press Fn+Esc.

### Restore the stock firmware

Enter the bootloader (Fn+Esc in QMK), then:

```
python3 -I util/evision_flash.py flash ${KB}_stock_app.bin --send
```

To compare the flash with the backup while the stock firmware runs:

```
python3 -I util/evision_dump.py --start 0x2800 --end 0xEFC0 --out readback.bin --send
python3 -I util/evision_flash.py verify readback.bin ${KB}_stock_app.bin
```

On the tested K580, a restore of firmware 1.04 gave a full 64 KB read-back identical to the original backup.

## Keymaps: `default` and `via`

Each keyboard has two keymaps with the same layout, Fn layer and lighting.

| | `default` | `via` |
|---|---|---|
| Keymap changes | In the source; build and flash again | Live, in [VIA](https://usevia.app): 3 layers, macros |
| Kept at power-off | The keyboard's own settings (data flash) | The same, and QMK's settings, the keymap and the macros (code flash, 0xE740–0xEFBF) |
| Code flash written by the firmware | The boot marker page, to enter the bootloader | The same, and the store at 0xE740–0xEFBF, 2 s after each change |

To use VIA:

1. Build and flash the `via` image (`redragon_${KB}_via.bin`).
2. Open [usevia.app](https://usevia.app) in Chrome or Chromium (WebHID). In Settings, turn on "Show Design tab".
3. In the Design tab, load the keyboard's VIA definition,
   `keyboards/redragon/$KB/keymaps/via/redragon_${KB}_via.json`.
4. In the Configure tab, authorize the keyboard (`$QMK_ID`).

The store ([lib/ch555/eeprom_code_flash.c](lib/ch555/eeprom_code_flash.c)):
- QMK's EEPROM is 1 KB: QMK's settings, 3 layers, the encoders (the K580's roller) and the macros (the keyboard
  readmes give the size). Layers 0 and 1 start as the default keymap, layer 2 as transparent keys.
- A change is saved 2 s after the last one. The store has two copies: a save erases and writes the older copy and
  marks it valid last, so a power loss during a save keeps the previous state.
- The store writes only its own pages. It checks the page address before it enables writes and again after, and
  the build stops if the code reaches the store.
- Every flash erases the store: after a flash the keymap is the default again. Save the layout in VIA first
  (Save + Load) and load it again after the flash. The keyboard's own settings in the data flash are kept.
- VIA's "Reset" key (`QK_BOOT`) enters the bootloader, as Fn+Esc does. The keyboard then stays in the bootloader
  until a flash, and the flash erases the store.
- The custom keys of the default keymap (Win Lock, lighting settings; K580: roller mode) have names in VIA.

To go back, flash the `default` image; the flash erases the store.

## Limits

- `default` keymap: QMK's settings (eeconfig) are kept in RAM only (transient EEPROM driver) and are QMK's
  defaults after power-up. The keyboard's own settings use the data flash instead.
- Turned off for flash space (each keyboard's `config.h` and `rules.mk`): tap and one-shot keys, Grave Escape
  (`QK_GESC`), Space Cadet, Magic keycodes (among them the modifier swaps and the NKRO keys `NK_TOGG`, `NK_ON`,
  `NK_OFF`) and, in the `default` keymap, `send_string` (the `via` keymap needs it for macros). The `default`
  keymaps use none of these features. A keymap changed in the source that uses one must turn it on again, if the
  flash space allows (the keyboard readmes give the free space).
- VIA lists keycodes of features that these builds do not contain: the features above and others, such as mouse
  keys. Assigned in VIA, these keycodes do nothing, with two exceptions: `LT(layer, key)` sends only its key, and
  `OSL(layer)` works as `MO(layer)`.
- QMK's `rgb_matrix` does not compile with SDCC: the keyboards have their own lighting code, and VIA has no
  lighting controls for them.
- Vial is not supported: it would need the vial-qmk fork.
- The backup and flash tools (`util/evision_dump.py`, `util/evision_flash.py`) run on Linux and Windows only.

## Building

Both builds write `<keyboard>_<keymap>.bin` in the top directory: the `default` keymap of `redragon/k580` gives
`redragon_k580_default.bin`. The file is the application image for the Evision bootloader, based at 0x2800.

The USB serial number is the chip ID unless one is set at build time (letters, digits and `. _ : -`, at most 126
characters).

### Build with Docker

`util/ch555_build.py KEYBOARD [KEYMAP] [--serial SERIAL]` builds in Docker (SDCC, clang and the QMK CLI;
[util/ch555_build.Dockerfile](util/ch555_build.Dockerfile)), on Linux, and with Docker Desktop on Windows. The
first build makes the Docker image, which takes a few minutes.

```
# Option 1: Use chip ID for serial number
python3 -I util/ch555_build.py redragon/k580

# Option 2: Provide serial number (from sticker on keyboard)
python3 -I util/ch555_build.py redragon/k580 --serial RDK580RGB00000000000
```

### Build with QMK

The toolchain is SDCC 4.6.0, clang with clang-query and llvm-nm, binutils and the QMK CLI. The build stops at
constructs that SDCC 4.6.0 compiles to wrong code
([platforms/mcs51/sdcc_check.query](platforms/mcs51/sdcc_check.query); tickets in
[SDCC bug reports and feature requests](#sdcc-bug-reports-and-feature-requests)). SDCC 4.6.0 is required.

```
# System packages
sudo apt install binutils bzip2 clang clang-tools curl libstdc++6 llvm make python3 python3-venv wget

# QMK CLI installs to ~/.local/bin
curl -fsSL https://install.qmk.fm | sh

# Install SDCC to ~/.local/sdcc-4.6.0
wget -O /tmp/sdcc-4.6.0.tar.bz2 https://sourceforge.net/projects/sdcc/files/sdcc-linux-amd64/4.6.0/sdcc-4.6.0-amd64-unknown-linux2.5.tar.bz2/download
tar -xjf /tmp/sdcc-4.6.0.tar.bz2 -C ~/.local
echo 'export PATH="$HOME/.local/sdcc-4.6.0/bin:$HOME/.local/bin:$PATH"' >> ~/.bashrc
```

Then, in a new shell, from the project directory:

```
# Option 1: Use chip ID for serial number
qmk compile -kb redragon/k580 -km default

# Option 2: Provide serial number (from sticker on keyboard)
qmk compile -kb redragon/k580 -km default -e SERIAL_NUMBER=RDK580RGB00000000000
```

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
  arrays). The QMK core is edited in the files that the K580 and K552 builds compile; other features may need the
  same edits.
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
- A Redragon keyboard with the same LED controller wiring includes
  [keyboards/redragon/common/redragon_ch555.mk](keyboards/redragon/common/redragon_ch555.mk) in its `rules.mk`;
  the K552 is the smaller example.

### SDCC bug reports and feature requests

The following bugs and feature requests have workarounds applied to QMK, most guarded by `#ifdef __SDCC`, or in
the compiler driver (weak symbols, the checks of
[platforms/mcs51/sdcc_check.query](platforms/mcs51/sdcc_check.query)):

- [Feature Request 595: Weak linkage](https://sourceforge.net/p/sdcc/feature-requests/595/)
- [Feature Request 1038: Compile case ranges as ranges instead of one case per value](https://sourceforge.net/p/sdcc/feature-requests/1038/)
- [Bug 1900: calling inline function](https://sourceforge.net/p/sdcc/bugs/1900/)
- [Bug 3857: Ternary operator in struct assignment using a function call fails](https://sourceforge.net/p/sdcc/bugs/3857/)
  (checked by the driver; no site in the build)
- [Bug 3886: Compiler don't create constant for compound literal](https://sourceforge.net/p/sdcc/bugs/3886/)
  (fixed after 4.6.0, in r16747)
- [Bug 3979: error 147: excess elements in struct initializer](https://sourceforge.net/p/sdcc/bugs/3979/)
  (designated initializers into an anonymous struct:
  [comment 5099](https://sourceforge.net/p/sdcc/bugs/3979/#5099))
- [Bug 4044: Initializer loses const qualifier](https://sourceforge.net/p/sdcc/bugs/4044/) (fixed after 4.6.0,
  in r16763)
- [Bug 4157: Struct returned from a function call is not copied to the caller](https://sourceforge.net/p/sdcc/bugs/4157/)
- [Bug 4158: mcs51: second return of the same struct variable gives wrong values (large model)](https://sourceforge.net/p/sdcc/bugs/4158/)
- [Bug 4159: Repeated typedef of a struct or union tag gives error 141](https://sourceforge.net/p/sdcc/bugs/4159/)

## History and credits

The code comes from [mdmower/vial-qmk-8051](https://github.com/mdmower/vial-qmk-8051) (branch `k580-ch55x`), a
fork of vial-qmk that keeps the full commit history of the original 8051 port and of the K580 work. Here that
code is a new series of commits on QMK `develop`; their `Co-authored-by` lines name the authors of the original
code.

- **vuhuycan**, [vuhuycan/qmk-8051](https://github.com/vuhuycan/qmk-8051): the port of vial-qmk to the 8051 and
  SDCC, including the MCS-51 platform code and the CH555 USB stack.
- **TechnoClasher**, [RakeRaven/qmk-8051](https://github.com/RakeRaven/qmk-8051): the Redragon K580 keyboard
  definition, changes to the CH555 platform, and fixes to the CH555 USB stack, NKRO and the lock LEDs.
- **Glitch752**, [Glitch752/qmk-8051-vs11k28a](https://github.com/Glitch752/qmk-8051-vs11k28a): reverse
  engineering of the Evision OEM bootloader's flash protocol.
