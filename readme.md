# QMK for 8051 keyboards

This is a fork of [QMK](https://github.com/qmk/qmk_firmware) for keyboards using 8051 microcontrollers. Currently, a handful of Redragon keyboards using Evision MCUs from the WCH CH555 family or the Sinowealth SH68F90A are supported. Additional 8051 keyboards could potentially be supported but the amount of overlap in reusable platform code, not to mention backup and flash scripts, is uncertain. The [platform documentation](docs/platformdev_mcs51.md#adding-an-mcu) lists what a port to another MCU involves.

_This is an unofficial QMK fork. Use at your own risk. You assume all risk for damages that may occur, including but not limited to loss of data, damages to hardware, or loss of business profits._

## Supported keyboards

| Keyboard                                       | MCU                 | Stock USB VID:PID | QMK USB VID:PID | State                          |
| ---------------------------------------------- | ------------------- | ----------------- | --------------- | ------------------------------ |
| [Redragon K551RGB-1](keyboards/redragon/k551/) | Evision VS11K28A    | `320f:5000`       | `5244:0551`     | Untested                       |
| [Redragon K552RGB-2](keyboards/redragon/k552/) | Evision VS11K28A    | `320f:5000`       | `5244:0552`     | Untested                       |
| [Redragon K580RGB](keyboards/redragon/k580/)   | Evision VS11K33A    | `320f:5000`       | `5244:0580`     | Tested on Linux and Windows 11 |
| [Redragon K582RGB](keyboards/redragon/k582/)   | Evision VS11K28A    | `320f:5000`       | `5244:0582`     | Tested on Linux and Windows 11 |
| [Redragon K589RGB](keyboards/redragon/k589/)   | Evision VS11K28A    | `320f:5000`       | `5244:0589`     | Untested                       |
| [Redragon K668](keyboards/redragon/k668/)      | Sinowealth SH68F90A | `258a:0049`       | `5244:0668`     | Tested on Linux and Windows 11 |

This root readme covers information common to all of the above keyboards. See the individual keyboard readmes for information specific to each (bootloader checks, keymaps, lighting, and hardware). Keyboards with a CH555-family MCU (the Evision VS11K28A and VS11K33A) and keyboards with a Sinowealth SH68F90A have different bootloaders and use different scripts to back up and flash. Where the steps differ, each has its own subsection.

## Before you flash

- **Verify your keyboard is supported**: If the stock USB VID:PID does not match what's shown in the table above, your keyboard is not supported by this fork.
- Back up your keyboard's stock firmware, see [step 1](#1-back-up-the-stock-firmware) below.
- Notice the same stock USB VID:PID is used for several keyboard models. The stock bootloaders do not check that an image was written for the correct keyboard model before flashing it. Make sure you only flash firmware images designed for your keyboard model.

**CH555 keyboards**

- Entering the bootloader erases the boot marker. The keyboard stays in the bootloader until a flash completes, even if you replug the USB cable. Make sure you are ready to flash a firmware image before booting to the bootloader.
- The CH555 has no write protection for the Evision bootloader region of flash storage. A single "enable" bit allows writes to all of code flash. Be careful when editing addresses in flash storage code; if you accidentally corrupt the bootloader's code, the keyboard will no longer start. The recovery option (which is unproven on these keyboards) requires a hardware modification: hold pin P1.5 of the chip low while you plug in the keyboard. From there, it is possible to [flash through the WCH bootloader](#flashing-through-the-wch-bootloader-ch555-keyboards).

**SH68F90A keyboards**

- Entering the bootloader does not erase anything. If you replug the keyboard, the firmware starts again. Once `util/sinowealth_flash.py` has erased the firmware, the keyboard stays in the bootloader until a flash completes, even if you replug the USB cable.
- The ISP bootloader does not have a built-in held key check to decide whether to stay at the bootloader or start the firmware. When stock firmware is running, `util/sinowealth_flash.py` reboots the keyboard to the bootloader. Once QMK is flashed, you can reach the bootloader by holding Esc while plugging in the keyboard, or by pressing Fn+Esc while QMK is running. If you flash an image that does not start, none of these options work, and the keyboard can only be recovered with a hardware programmer on the MCU's debug pins.
- `sinowealth_flash.py` refuses to flash an image that is missing bootloader jump support, but it cannot check whether or not QMK will reach that point.

## Requirements

**To back up and flash,** you need Python 3 on Linux or Windows.

- On Linux, the scripts need access to the keyboard's hidraw and USB device nodes. Install udev rules and make sure your user is in the `plugdev` group. For example, on Debian/Ubuntu distributions:

    ```sh
    groups # Verify output includes: plugdev

    sudo cp util/udev/70-redragon.rules /etc/udev/rules.d/
    sudo udevadm control --reload && sudo udevadm trigger
    ```

    The udev rules grant access to the stock firmware's vendor interface, the bootloader, and QMK's raw HID interface, which VIA uses. For SH68F90A keyboards, they also grant access to QMK's keyboard interface (interface 0), through which `util/sinowealth_flash.py` asks QMK to reboot to the bootloader.

- On Windows, install Python from [python.org](https://www.python.org/downloads/windows/) or the Microsoft Store. No additional drivers are needed. Windows blocks some of the operations needed when using an unsigned version of Python, for example those installed by `uv`. Make sure no Redragon software is running when you back up and flash, as it uses the same interface as the scripts.

**To build,** you need Docker or the full QMK toolchain described in [Development: Build](#build). The `lib/lufa` submodule must be available; either clone this repository with `git clone --recurse-submodules ...`, or after cloning run `git submodule update --init lib/lufa`.

## Getting started

This section covers the steps to back up, build, flash, and restore firmware common to all supported keyboards.

**Tips**

- Commands should be run from the project root.
- On Windows, use PowerShell, not Command Prompt.
- `python` is assumed to be v3. If your installation of Python does not alias `python`, write `python3` instead.
    - On Debian/Ubuntu, you can install the alias with `sudo apt install python-is-python3`.
- The `util/evision_dump.py`, `util/evision_flash.py`, and `util/sinowealth_flash.py` scripts are "dry run" by default. After confirming the output is what you'd expect, you can enable real interaction with your keyboard by passing argument `--send`.

### 0. Preliminary setup

The commands in the following sections expect two shell variables to be set, identifying your keyboard: `KB` and `QMK_ID`. `KB` is the keyboard's directory under `keyboards/redragon/`. `QMK_ID` is the USB ID that QMK enumerates as on that keyboard. The values for each keyboard are:

| Keyboard  | KB   | QMK_ID    |
| --------- | ---- | --------- |
| K551RGB-1 | k551 | 5244:0551 |
| K552RGB-2 | k552 | 5244:0552 |
| K580RGB   | k580 | 5244:0580 |
| K582RGB   | k582 | 5244:0582 |
| K589RGB   | k589 | 5244:0589 |
| K668      | k668 | 5244:0668 |

For example, when flashing the K580RGB from Linux, set environment variables:

```
export KB=k580 QMK_ID=5244:0580
```

Or, when flashing the K552RGB-2 from Windows, set PowerShell variables:

```
$KB = "k552"; $QMK_ID = "5244:0552"
```

_Tip: For all of the sections below, you can check the mode your keyboard is in:_

- _CH555 keyboards: `python -I util/evision_flash.py status` returns `bootloader`, `app` (stock firmware), or `QMK`._
- _SH68F90A keyboards: `python -I util/sinowealth_flash.py status` lists the keyboard as `ISP bootloader`, `stock firmware`, or `QMK` with the keyboard's name._

### 1. Back up the stock firmware

#### CH555 keyboards

Read the 64 KB of flash, including the bootloader, to a file:

```sh
python -I util/evision_dump.py --out ${KB}_stock.bin --send
```

The dump prints whether the boot marker `AA 55 33 CC` is at the expected position: `0x27C0`. **If it is not, the flash layout below does not apply to your keyboard and you should stop here; this fork is not compatible with your keyboard!**

_Note: In some dumps, 0xF400–0xFBFF (part of the WCH factory bootloader) reads as zeros, while in others the whole factory bootloader is present. This does not affect the backup, since only the application region is flashed back._

Extract the app portion of the firmware (i.e. remove the bootloader) from the dumped file:

```sh
python -I util/evision_flash.py extract ${KB}_stock.bin ${KB}_stock_app.bin
```

#### SH68F90A keyboards

Read the 64 KB of flash, including the bootloader, to a file:

```sh
python -I util/sinowealth_flash.py backup ${KB}_stock.bin --send
```

The script reboots the keyboard to the bootloader, reads the flash twice, saves the file only if both reads are the same, and then restarts the stock firmware. The backup can be flashed without changes. There is no extract step.

### 2. Build with Docker

`util/mcs51_build.py KEYBOARD [KEYMAP] [--serial SERIAL]` builds the firmware inside a Docker container so that you do not need to install the build toolchain on your system (SDCC, clang, and the QMK CLI). The first build creates the Docker image, which takes a few minutes. Subsequent builds are faster. If you would rather install the build toolchain on your system, see section [Development: Build](#build).

#### Examples

Example 1: Build the `default` keymap for your device and use the microcontroller chip ID for the USB serial number:

```sh
python -I util/mcs51_build.py redragon/$KB
# Output file: redragon_${KB}_default.bin (CH555) or redragon_${KB}_default.hex (SH68F90A)
```

Example 2: Build the `via` keymap for your device and manually provide the serial number from the sticker on the back of your keyboard:

```sh
python -I util/mcs51_build.py redragon/$KB via --serial RDK580RGB00000000000
# Output file: redragon_${KB}_via.bin (CH555) or redragon_${KB}_via.hex (SH68F90A)
```

### 3. Flash QMK

#### CH555 keyboards

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

#### SH68F90A keyboards

_Make sure you have a second keyboard available before flashing. Once the flash script has erased the firmware, you cannot type on the keyboard until a flash completes._

Flash QMK. The same command works from the stock firmware and from QMK:

```sh
python -I util/sinowealth_flash.py flash redragon_${KB}_default.hex --expect $QMK_ID --send
```

The script first reboots the keyboard to the bootloader if not already there and then checks that the bootloader is recognized before proceeding. Once the bootloader is verified, the script erases and writes 0x0000–0xEFFF, which excludes the bootloader region. After flashing, the write is verified and the keyboard reboots only if successful. Finally, the script waits for the `--expect` ID. If a flash fails or is interrupted, the keyboard stays in the bootloader, and you can retry flashing.

### Restore stock firmware (if needed)

#### CH555 keyboards

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

#### SH68F90A keyboards

Flash the backup made in step 1:

```sh
python -I util/sinowealth_flash.py flash ${KB}_stock.bin --send
```

The script writes only the firmware region of the backup (0x0000–0xEFFF). The write is subsequently verified.

## Flashing through the WCH bootloader (CH555 keyboards)

The CH555 has a second bootloader, from WCH, the maker of the chip. It is at 0xF400, outside the range that the Evision bootloader erases, and it can write all of 0x0000–0xEFFF. An image you flash through it starts at 0x0000 instead of 0x2800, so it has about 10 KB more room. The first flash through the WCH bootloader erases the Evision bootloader together with the stock firmware. From then on, a dump of your keyboard's original flash is the only copy of the Evision bootloader you have, and flashing that dump is the only way to restore it.

This is for people who want more space for their own keymap and accept a keyboard with only an ISP bootloader. The switch and the return to stock have been tested on a K580RGB and a K582RGB. The scripts run on Linux only: they reach the WCH bootloader through usbfs, which has no Windows counterpart. Additional udev rules (beyond those described in [Requirements](#requirements)) must be installed:

```sh
sudo cp util/udev/70-wch-isp.rules /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger
```

### Build an image for the WCH bootloader

Set the bootloader in your keymap's `rules.mk`:

```make
BOOTLOADER = wch-isp
```

and build as usual, for example with `python -I util/mcs51_build.py redragon/$KB mykeymap`. For a one-off build without a keymap of your own, pass `-e BOOTLOADER=wch-isp` to `qmk compile`. The build process checks the generated image with `util/wch_isp.py check`. Flash scripts only accept images they are designed to flash; `util/evision_flash.py` refuses an image built for the WCH bootloader and `util/wch_isp.py` refuses one built for the Evision bootloader.

### Switch

_Make sure you have a second keyboard available. From the moment the Evision bootloader starts until the switch is complete, this keyboard does not type._

While the keyboard is running stock firmware, one command performs the switch:

```sh
python -I util/evision_wch_switch.py redragon_${KB}_mykeymap.bin ${KB}_full.bin --expect $QMK_ID --send
# redragon_${KB}_mykeymap.bin is the QMK firmware built with BOOTLOADER=wch-isp
# ${KB}_full.bin is the (recommended) filename of the stock flash dump to be output
```

The script reads a 64 KB dump of the flash into `${KB}_full.bin` and refuses to continue without it. It then starts the Evision bootloader and flashes a stepping stone image through it. The stepping stone is a small program whose purpose is to call the WCH bootloader. The script then flashes your image through the WCH bootloader. Run the command without `--send` first to see the keyboard's state and the steps the script would take.

On a K580RGB, the WCH bootloader cannot read flash in the session that the stepping stone starts, so the script erases the flash, asks you to unplug the keyboard and plug it in again, and writes after that. On a K582RGB the script writes in the same session because the WCH bootloader on that chip can read flash in the same session.

If a step fails, run the same command again. The script checks whether the keyboard is in the stock firmware, in the Evision bootloader, or in the WCH bootloader and continues from there (so long as the stock dump from the first run exists).

### Flash the next image

Fn+Esc, or Esc held while plugging in, now starts the WCH bootloader (USB VID:PID `4348:55e0`) instead of the Evision bootloader. Flash with `qmk flash`, or with:

```sh
python -I util/wch_isp.py flash redragon_${KB}_mykeymap.bin --expect $QMK_ID --send
```

Add `--poll` to start the command before you press Fn+Esc. On a K580RGB, the script asks you at every flash to unplug the keyboard and plug it in again once, as during the switch. If a flash fails after the erase, the keyboard starts the WCH bootloader every time you plug it in until a flash completes, so you can run the command again.

### Restore the Evision bootloader and the stock firmware

Flash the dump from the switch through the WCH bootloader:

```sh
python -I util/wch_isp.py flash ${KB}_full.bin --expect 320f:5000 --send
```

This writes 0x0000–0xEFFF of the dump: the Evision bootloader, the boot marker, and the stock firmware. The keyboard then starts the stock firmware, and the Evision bootloader can be used again as before. The data flash (0xF000–0xF3FF, which is not used by stock) is not written. If you want QMK's settings cleared from it, press Fn+Ctrl+Backspace before you restore.

## Using the keyboard

### Keymaps: `default` and `via`

Each keyboard has a `default` and a `via` keymap with the same layout, Fn layer, and lighting.

|                                              | `default`                                                      | `via`                                                                                                          |
| -------------------------------------------- | -------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| Keymap changes                               | Edit source, build, and flash again                            | Live, in [VIA](https://usevia.app): 3 layers, macros                                                           |
| Kept at power-off, CH555 keyboards           | The keyboard's own settings (in data flash)                    | The keyboard's own settings (in data flash), as well as QMK's settings, the keymap, and macros (in code flash) |
| Kept at power-off, SH68F90A keyboards        | The keyboard's own settings and QMK's settings (in code flash) | The keyboard's own settings, QMK's settings, the keymap, and macros (in code flash)                            |
| Code flash written by the firmware, CH555    | The boot marker page, to enter the bootloader                  | The boot marker page, to enter the bootloader, as well as the store at 0xDF40–0xEFBF                           |
| Code flash written by the firmware, SH68F90A | The store (its address is in the keyboard's readme)            | The store (its address is in the keyboard's readme)                                                            |

To use VIA:

1. Build and flash the `via` image (`redragon_${KB}_via.bin` or `redragon_${KB}_via.hex`).
2. Open [usevia.app](https://usevia.app) in Chrome or Chromium (which support WebHID), and in Settings turn on "Show Design tab".
3. In the Design tab, load the keyboard's VIA definition: `keyboards/redragon/$KB/keymaps/via/redragon_${KB}_via.json`.
4. In the Configure tab, authorize the keyboard (`$QMK_ID`).

Notes about the code flash store:

- QMK's EEPROM is about 2 KB. It holds QMK's settings, the three layers, the encoders (e.g. the K580's roller mode), and the macros (the space available for macros depends on the keyboard). Layers 0 and 1 start as the default keymap; layer 2 starts as transparent keys.
- Changes are saved 2 s after the last edit. When writing changes, old settings are erased only after the entire update has been written. In the event of a power loss during save, the last working copy of settings remains available.
- The build script has a check that ensures app code does not overlap with the store.
- Every QMK flash erases the entire application region in code flash, including the store. Save your layout in VIA and load it again after the flash.
- CH555 keyboards keep their own settings (e.g. lighting) in data flash, which flashing does not erase. SH68F90A keyboards do not have a separate data flash region; their own settings are in the code flash store, which flashing erases.

### Clearing saved settings

Fn+Ctrl+Backspace clears all stored settings and then restarts the keyboard. In VIA builds, key `EE_CLR` can be mapped to do the same, but Ctrl must additionally be held to avoid accidental resets.

## Known limitations

- **`default` keymap of the CH555 keyboards:** QMK's settings (eeconfig) are kept in RAM only, using the transient EEPROM driver. Those settings are lost when the keyboard power cycles. The keyboard's own settings (e.g. lighting) use the data flash store and are retained.
- **Some features are turned off to save space:** See each keyboard's `config.h` and `rules.mk` to identify features that are disabled. Some are small enough to enable, but they would still require QMK core source edits to be compatible with SDCC.
- **VIA lists keycodes for unsupported features:** Disabled features, and others such as mouse keys, may still be assignable in VIA. Most won't do anything when pressed. A few do: `LT(layer, key)` and `MT(mod, key)` send only their key, `OSM(mod)` acts as a plain modifier key, and `TT(layer)` acts as `TG(layer)`.
- **No `rgb_matrix`:** QMK's `rgb_matrix` does not compile with SDCC. The keyboards have their own lighting code and VIA offers no lighting controls for them.
- **Scripts run on Linux and Windows only:** The backup and flash scripts (`util/evision_dump.py`, `util/evision_flash.py`, `util/sinowealth_flash.py`) do not support macOS. The scripts for the WCH bootloader (`util/wch_isp.py`, `util/evision_wch_switch.py`) run on Linux only.

## Development

### Flash layout

#### CH555 keyboards

| Range         | Contents                                                                                                                                                                             |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 0x0000–0x27FF | Evision bootloader (USB VID:PID `0c45:7687`). `util/evision_flash.py` never writes to this region.                                                                                   |
| 0x27C0        | Boot marker `AA 55 33 CC`, in the last page of the bootloader's region. The bootloader starts the application only if the marker is present. It is written after a successful flash. |
| 0x2800–0xEFBF | Application: stock firmware and QMK. The bootloader erases and writes this range on every flash. The `via` keymap (and the K580's `macro` keymap) reserve a store at 0xDF40–0xEFBF.  |
| 0xF000–0xF3FF | Data flash. QMK stores the keyboard's own settings here (e.g. lighting). Flashing does not erase this region. Stock firmware does not use this region.                               |
| 0xF400–       | WCH's bootloader (`4348:55e0`). `util/wch_isp.py` flashes through it, see [Flashing through the WCH bootloader](#flashing-through-the-wch-bootloader-ch555-keyboards).               |

QMK images start with a check for Esc at 0x2803 and contain the interrupt table that the bootloader jumps to at 0x3000; both are added by `util/evision_oem_image.py`. The Esc check runs before QMK starts, so holding Esc (which is column P0.0 and row P4.0 on all CH555 keyboards) while plugging in the keyboard enters the bootloader even when a QMK app image fails.

#### SH68F90A keyboards

| Range         | Contents                                                                                                                                                                       |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 0x0000–0xEFFA | Application (stock firmware or QMK). QMK's EEPROM store ends at 0xEDFF; its start depends on the keymap and is in each keyboard's readme.                                      |
| 0xEFFB–0xEFFF | Start marker `02` and the image's reset vector, written by the bootloader. The bootloader starts the application only if the marker is present. Images leave this range empty. |
| 0xF000–0xFFFF | Sinowealth ISP bootloader (USB VID:PID `0603:1020`). `util/sinowealth_flash.py` never writes to this region.                                                                   |

QMK images for SH68F90A keyboards are linked at 0x0000. Every flash, the bootloader erases 0x0000–0xEFFF and writes the image. It keeps a jump to itself at 0x0000 and moves the image's reset vector to 0xEFFC–0xEFFD. The check for Esc is in each keyboard's `keyboard_pre_init_kb()`, so it runs after QMK's startup code but before USB starts. The 512-byte sector 0xEE00–0xEFFF holds the start marker, so QMK never erases it.

### Build

Whether building with Docker (as described in [Getting started](#getting-started)) or with the toolchain installed to your system, the firmware image is output in the format of the keyboard's bootloader:

- CH555 keyboards: `<keyboard>_<keymap>.bin`, the application image based at 0x2800. The `default` keymap of `redragon/k580`, for example, outputs `redragon_k580_default.bin`.
- SH68F90A keyboards: `<keyboard>_<keymap>.hex`, an Intel HEX file linked at 0x0000. The `default` keymap of `redragon/k668`, for example, outputs `redragon_k668_default.hex`.

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

- CH555 keyboards: the `rules.mk` of a keyboard with the CH555's LED controller can include [keyboards/redragon/common/redragon_ch555.mk](keyboards/redragon/common/redragon_ch555.mk) at the top. The K552 is the smaller example to start from.
- SH68F90A keyboards: the [K668](keyboards/redragon/k668/) is the example to start from. Its LED driver also scans the key matrix, so a keyboard with different wiring would need its own scanner and LED driver.

### History and credits

The original series of commits and their authors is at [mdmower/vial-qmk-8051](https://github.com/mdmower/vial-qmk-8051) in branch `k580-ch55x`. Rebasing on newer QMK required enough rework that a smaller series of squashed and edited commits was prepared; the commits in this repository have `Co-authored-by` lines when they include changes that originated from other authors.

- **vuhuycan**, [vuhuycan/qmk-8051](https://github.com/vuhuycan/qmk-8051): the port of vial-qmk to the 8051 and SDCC, including the MCS-51 platform code and the CH555 USB stack.
- **TechnoClasher**, [RakeRaven/qmk-8051](https://github.com/RakeRaven/qmk-8051): the Redragon K580 keyboard definition, changes to the CH555 platform, and fixes to the CH555 USB stack, NKRO, and the lock LEDs.
- **Glitch752**, [Glitch752/qmk-8051-vs11k28a](https://github.com/Glitch752/qmk-8051-vs11k28a): reverse engineering of the Evision OEM bootloader's flash protocol.
- **karstx**, [karstx/sinowisp](https://github.com/karstx/sinowisp) (MIT): the Sinowealth ISP bootloader's protocol, which `util/sinowealth_flash.py` implements.
- **gashtaan**, [gashtaan/sinowealth-8051-dumper](https://github.com/gashtaan/sinowealth-8051-dumper): analysis of the Sinowealth ISP bootloaders, including the command that lets the bootloader start the firmware and read it back.
