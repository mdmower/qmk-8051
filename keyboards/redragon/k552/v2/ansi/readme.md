# Redragon K552 Kumara ANSI

An 87-key keyboard with RGB lighting. This definition is for the ANSI version of the K552RGB-2.

* Keyboard Maintainer: [Matt Mower](https://github.com/mdmower)
* Hardware Supported: K552RGB-2, SN32F248BF
* Hardware Availability: [Redragon product page](https://www.redragonzone.com/detail.html?id=1119)

Make example for this keyboard (after setting up your build environment):

    make redragon/k552/v2/ansi:default

Flashing example for this keyboard, with [SonixFlasherC](https://github.com/SonixQMK/SonixFlasherC) installed and the keyboard in the bootloader:

    make redragon/k552/v2/ansi:default:flash

To replace the stock firmware, build first and run this instead. It reboots the keyboard into the bootloader and flashes the new image:

    sonixflasher -v 0c45/5004 -r evision -f redragon_k552_v2_ansi_default.bin

See the [build environment setup](https://docs.qmk.fm/newbs_getting_started) and the [make instructions](https://docs.qmk.fm/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/newbs).

## Bootloader

Press Fn+Esc to enter the bootloader.
