# QMK for CH555-family 8051 keyboards

This is a fork of [QMK](https://github.com/qmk/qmk_firmware) for keyboards using 8051 microcontrollers. Currently, a handful of Redragon keyboards using Evision MCUs from the WCH CH555 family are supported. Additional 8051 keyboards could potentially be supported but the amount of overlap in reusable platform code (if different than CH555), not to mention backup and flash scripts, is uncertain. The [platform documentation](docs/platformdev_mcs51.md#adding-an-mcu) lists what a port to another MCU involves.

_This is an unofficial QMK fork. Use at your own risk. You assume all risk for damages that may occur, including but not limited to loss of data, damages to hardware, or loss of business profits._

## Supported keyboards

| Keyboard                                       | MCU              | Stock USB VID:PID | QMK USB VID:PID | State                          |
| ---------------------------------------------- | ---------------- | ----------------- | --------------- | ------------------------------ |
| [Redragon K551RGB-1](keyboards/redragon/k551/) | Evision VS11K28A | `320f:5000`       | `5244:0551`     | Untested                       |
| [Redragon K552RGB-2](keyboards/redragon/k552/) | Evision VS11K28A | `320f:5000`       | `5244:0552`     | Untested                       |
| [Redragon K580RGB](keyboards/redragon/k580/)   | Evision VS11K33A | `320f:5000`       | `5244:0580`     | Tested on Linux and Windows 11 |
| [Redragon K582RGB](keyboards/redragon/k582/)   | Evision VS11K28A | `320f:5000`       | `5244:0582`     | Tested on Linux and Windows 11 |
| [Redragon K589RGB](keyboards/redragon/k589/)   | Evision VS11K28A | `320f:5000`       | `5244:0589`     | Untested                       |

This root readme covers information common to all of the above keyboards. See the individual keyboard readmes for information specific to each (bootloader checks, keymaps, lighting, and hardware).

## Before you flash

- **Verify your keyboard is supported**: If the stock USB VID:PID does not match what's shown in the table above, your keyboard is not supported by this fork.
- Back up your keyboard's stock firmware, see [step 1](#1-back-up-the-stock-firmware) below.
- Notice the same stock USB VID:PID is used for several keyboard models. The stock bootloader does not validate a firmware is written for the correct keyboard model before flashing it. Make sure you only flash firmware images designed for your keyboard model.
- Entering the bootloader erases the boot marker. The keyboard stays in the bootloader until a flash completes, even if you replug the USB cable. Make sure you are ready to flash a firmware image before booting to the bootloader.
- The CH555 has no write protection for the Evision bootloader region of flash storage. A single "enable" bit allows writes to all of code flash. Be careful when editing addresses in flash storage code; if you accidentally corrupt the bootloader, there is no tested means of restoring it via the WCH ISP.

## Requirements

**To back up and flash,** you need Python 3 on Linux or Windows.

- On Linux, the scripts need access to the keyboard's hidraw nodes. Install udev rules and make sure your user is in the `plugdev` group. For example, on Debian/Ubuntu distributions:

    ```sh
    groups # Verify output includes: plugdev

    sudo cp util/udev/70-redragon.rules /etc/udev/rules.d/
    sudo udevadm control --reload && sudo udevadm trigger
    ```

    The udev rules grant access to the stock firmware's vendor interface, the bootloader, and QMK's raw HID interface, which VIA uses.

- On Windows, install Python from [python.org](https://www.python.org/downloads/windows/) or the Microsoft Store. No additional drivers are needed. Windows blocks some of the operations needed when using an unsigned version of Python, for example those installed by `uv`. Make sure no Redragon software is running when you back up and flash, as it uses the same interface as the scripts.

**To build,** you need Docker or the full QMK toolchain described in [Development: Build](#build). The `lib/lufa` submodule must be available; either clone this repository with `git clone --recurse-submodules ...`, or after cloning run `git submodule update --init lib/lufa`.

## Getting started

This section covers the steps to back up, build, flash, and restore firmware common to all supported keyboards.

**Tips**

- Commands should be run from the project root.
- On Windows, use PowerShell, not Command Prompt.
- `python` is assumed to be v3. If your installation of Python does not alias `python`, write `python3` instead.
    - On Debian/Ubuntu, you can install the alias with `sudo apt install python-is-python3`.
- The `util/evision_dump.py` and `util/evision_flash.py` scripts are "dry run" by default. After confirming the output is what you'd expect, you can enable real interaction with your keyboard by passing argument `--send`.

### 0. Preliminary setup

The commands in the following sections expect two shell variables to be set, identifying your keyboard: `KB` and `QMK_ID`. `KB` is the keyboard's directory under `keyboards/redragon/`. `QMK_ID` is the USB ID that QMK enumerates as on that keyboard. The values for each keyboard are:

| Keyboard  | KB   | QMK_ID    |
| --------- | ---- | --------- |
| K551RGB-1 | k551 | 5244:0551 |
| K552RGB-2 | k552 | 5244:0552 |
| K580RGB   | k580 | 5244:0580 |
| K582RGB   | k582 | 5244:0582 |
| K589RGB   | k589 | 5244:0589 |

For example, when flashing the K580RGB from Linux, set environment variables:

```
export KB=k580 QMK_ID=5244:0580
```

Or, when flashing the K552RGB-2 from Windows, set PowerShell variables:

```
$KB = "k552"; $QMK_ID = "5244:0552"
```

_Tip: For all of the sections below, you can check the mode your keyboard is in with: `python -I util/evision_flash.py status`. The script will return one of: `bootloader`, `app` (stock firmware), or `QMK` if the keyboard is in a recognized state._

### 1. Back up the stock firmware

Read the firmware from your keyboard to a file:

```sh
python -I util/evision_dump.py --out ${KB}_stock.bin --send
```

The dump prints whether the boot marker `AA 55 33 CC` is at the expected position: `0x27C0`. **If it is not, the flash layout below does not apply to your keyboard and you should stop here; this fork is not compatible with your keyboard!**

Extract the app portion of the firmware (i.e. remove the bootloader) from the dumped file:

```sh
python -I util/evision_flash.py extract ${KB}_stock.bin ${KB}_stock_app.bin
```

### 2. Build with Docker

`util/mcs51_build.py KEYBOARD [KEYMAP] [--serial SERIAL]` builds the firmware inside a Docker container so that you do not need to install the build toolchain on your system (SDCC, clang, and the QMK CLI). The first build creates the Docker image, which takes a few minutes. Subsequent builds are faster. If you would rather install the build toolchain on your system, see section [Development: Build](#build).

#### Examples

Example 1: Build the `default` keymap for your device and use the microcontroller chip ID for the USB serial number:

```sh
python -I util/mcs51_build.py redragon/$KB
# Output file: redragon_${KB}_default.bin
```

Example 2: Build the `via` keymap for your device and manually provide the serial number from the sticker on the back of your keyboard:

```sh
python -I util/mcs51_build.py redragon/$KB via --serial RDK580RGB00000000000
# Output file: redragon_${KB}_via.bin
```

### 3. Flash QMK

_Make sure you have a second keyboard available before flashing. Once the keyboard reboots to the bootloader, you cannot type on it, nor can it be replugged to boot like normal. You **must** flash a firmware image before it will boot like normal._

If your keyboard is running stock firmware, enter the bootloader with:

```sh
python -I util/evision_flash.py enter --send
```

Otherwise, if your keyboard is running QMK, type Fn+Esc or hold Esc when plugging in the keyboard to enter the bootloader.

Verify your keyboard is in the expected mode with `python -I util/evision_flash.py status`, and then flash QMK:

```sh
python -I util/evision_flash.py flash redragon_${KB}_default.bin --expect $QMK_ID --send
```

`flash` checks the image before it erases anything, writes only the application region 0x2800–0xEFBF (i.e. it does not overwrite the bootloader), and then waits for the `--expect` ID from the same USB port. If a flash fails or is interrupted, the keyboard remains at the bootloader and you can try running the command again.

_Tip: Include argument `--poll` at the end of the above flash command to make the script wait for your keyboard to reach the bootloader before flashing. This lets you execute the command and then press Fn+Esc afterwards._

### Restore stock firmware (if needed)

The `evision_flash.py` script only accepts stock images, which it recognizes by the stock firmware's command to enter the bootloader, and images that have the feature "hold Esc when plugging in to reach the bootloader". All QMK images have this feature. If the firmware backup you made earlier is unrecognized by the script, you can modify your stock firmware to include the feature:

```sh
# Notice _app in filename
python -I util/evision_oem_image.py stock ${KB}_stock_app.bin ${KB}_stock_esc.bin
```

Reboot your keyboard to the bootloader and flash the backup image with:

```sh
# Use _app for the unmodified stock backup image or _esc for the modified backup
python -I util/evision_flash.py flash ${KB}_stock_app.bin --send
```

To compare the image flashed on your keyboard with a backup image:

```sh
python -I util/evision_dump.py --start 0x2800 --end 0xEFC0 --out readback.bin --send
# Use _app for the unmodified stock backup image or _esc for the modified backup
python -I util/evision_flash.py verify readback.bin ${KB}_stock_app.bin
```

_Tip: The data flash region (0xF000–0xF3FF) is unused by stock and is not erased when stock is restored. QMK stores some settings there (e.g. lighting). To clear it, press Fn+Ctrl+Backspace (see [Clearing saved settings](#clearing-saved-settings)) before you restore the stock firmware._

## Using the keyboard

### Keymaps: `default` and `via`

Each keyboard has a `default` and a `via` keymap with the same layout, Fn layer, and lighting.

|                                    | `default`                                     | `via`                                                                                                          |
| ---------------------------------- | --------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| Keymap changes                     | Edit source, build, and flash again           | Live, in [VIA](https://usevia.app): 3 layers, macros                                                           |
| Kept at power-off                  | The keyboard's own settings (in data flash)   | The keyboard's own settings (in data flash), as well as QMK's settings, the keymap, and macros (in code flash) |
| Code flash written by the firmware | The boot marker page, to enter the bootloader | The boot marker page, to enter the bootloader, as well as the store at 0xDF40–0xEFBF, 2 s after each change    |

To use VIA:

1. Build and flash the `via` image (`redragon_${KB}_via.bin`).
2. Open [usevia.app](https://usevia.app) in Chrome or Chromium (which support WebHID), and in Settings turn on "Show Design tab".
3. In the Design tab, load the keyboard's VIA definition: `keyboards/redragon/$KB/keymaps/via/redragon_${KB}_via.json`.
4. In the Configure tab, authorize the keyboard (`$QMK_ID`).

Notes about the code flash store:

- QMK's EEPROM is 2 KB. It holds QMK's settings, the three layers, the encoders (e.g. the K580's roller mode), and the macros (the space available for macros depends on the keyboard). Layers 0 and 1 start as the default keymap; layer 2 starts as transparent keys.
- Changes are saved 2 s after the last edit. When writing changes, old settings are erased only after the entire update has been written. In the event of a power loss during save, the last working copy of settings remains available.
- The build script has a check that ensures app code does not overlap with the store.
- Every QMK flash erases the entire application region in code flash, including the store. Save your layout in VIA and load it again after the flash. The keyboard's own settings in data flash are retained and do not need backup/restore.

### Clearing saved settings

Fn+Ctrl+Backspace clears all stored settings (data flash and code flash) and then restarts the keyboard. In VIA builds, key `EE_CLR` can be mapped to do the same, but Ctrl must additionally be held to avoid accidental resets.

## Known limitations

- **`default` keymap:** QMK's settings (eeconfig) are kept in RAM only, using the transient EEPROM driver. Those settings are lost when the keyboard power cycles. The keyboard's own settings (e.g. lighting) use the data flash store and are retained.
- **Some features are turned off to save space:** See each keyboard's `config.h` and `rules.mk` to identify features that are disabled. Some are small enough to enable, but they would still require QMK core source edits to be compatible with SDCC.
- **VIA lists keycodes for unsupported features:** Disabled features, and others such as mouse keys, may still be assignable in VIA. Most won't do anything when pressed. A few do: `LT(layer, key)` and `MT(mod, key)` send only their key, `OSM(mod)` acts as a plain modifier key, and `TT(layer)` acts as `TG(layer)`.
- **No `rgb_matrix`:** QMK's `rgb_matrix` does not compile with SDCC. The keyboards have their own lighting code and VIA offers no lighting controls for them.
- **Scripts run on Linux and Windows only:** The backup and flash scripts (`util/evision_dump.py`, `util/evision_flash.py`) do not support macOS.

## Development

### Flash layout

| Range         | Contents                                                                                                                                                                             |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 0x0000–0x27FF | Evision bootloader (USB VID:PID `0c45:7687`). `util/evision_flash.py` never writes to this region.                                                                                   |
| 0x27C0        | Boot marker `AA 55 33 CC`, in the last page of the bootloader's region. The bootloader starts the application only if the marker is present. It is written after a successful flash. |
| 0x2800–0xEFBF | Application: stock firmware and QMK. The bootloader erases and writes this range on every flash. The `via` keymap (and the K580's `macro` keymap) reserve a store at 0xDF40–0xEFBF.  |
| 0xF000–0xF3FF | Data flash. QMK stores the keyboard's own settings here (e.g. lighting). Flashing does not erase this region. Stock firmware does not use this region.                               |
| 0xF400–       | WCH factory ISP bootloader (`4348:55e0`). Flashing through the ISP bootloader is untested.                                                                                           |

QMK images start with a check for Esc at 0x2803 and contain the interrupt table that the bootloader jumps to at 0x3000; both are added by `util/evision_oem_image.py`. The Esc check runs before QMK starts, so holding Esc (which is column P0.0 and row P4.0 for all supported keyboards) while plugging in the keyboard enters the bootloader even when a QMK app image fails.

### Build

Whether building with Docker (as described in [Getting started](#getting-started)) or with the toolchain installed to your system, the firmware image is output to `<keyboard>_<keymap>.bin`. The `default` keymap of `redragon/k580`, for example, outputs `redragon_k580_default.bin`. The file is the application image for the Evision bootloader, based at 0x2800.

The USB serial number is the microcontroller chip ID unless you set one at build time. A custom serial may contain letters, digits, and `. _ : -`, and is at most 126 characters.

#### Build with the QMK toolchain

The toolchain is SDCC 4.6.0, clang with clang-query and llvm-nm, binutils, and the QMK CLI. SDCC 4.6.0 is required. The build stops at constructs that SDCC 4.6.0 compiles incorrectly. The [platform documentation](docs/platformdev_mcs51.md#writing-code-for-sdcc) lists them, along with the corresponding SDCC tickets.

```
# System packages
sudo apt install binutils bzip2 clang clang-tools curl libstdc++6 llvm make python3 python3-venv python-is-python3 wget

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

### Platform documentation

[docs/platformdev_mcs51.md](docs/platformdev_mcs51.md) describes the MCS-51 platform for developers: the compiler driver and its build options, the limits of SDCC that affect keyboard and keymap code, the SDCC bugs that have workarounds in this tree, the source layout, and what a port to another 8051 MCU involves.

### Adding a Redragon keyboard

The `rules.mk` of a Redragon keyboard with the same LED controller can include [keyboards/redragon/common/redragon_ch555.mk](keyboards/redragon/common/redragon_ch555.mk) at the top. The K552 is the smaller example to start from.

### History and credits

The original series of commits and their authors is at [mdmower/vial-qmk-8051](https://github.com/mdmower/vial-qmk-8051) in branch `k580-ch55x`. Rebasing on newer QMK required enough rework that a smaller series of squashed and edited commits was prepared; the commits in this repository have `Co-authored-by` lines when they include changes that originated from other authors.

- **vuhuycan**, [vuhuycan/qmk-8051](https://github.com/vuhuycan/qmk-8051): the port of vial-qmk to the 8051 and SDCC, including the MCS-51 platform code and the CH555 USB stack.
- **TechnoClasher**, [RakeRaven/qmk-8051](https://github.com/RakeRaven/qmk-8051): the Redragon K580 keyboard definition, changes to the CH555 platform, and fixes to the CH555 USB stack, NKRO, and the lock LEDs.
- **Glitch752**, [Glitch752/qmk-8051-vs11k28a](https://github.com/Glitch752/qmk-8051-vs11k28a): reverse engineering of the Evision OEM bootloader's flash protocol.
