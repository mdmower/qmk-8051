# Redragon K582 Surara

[![Redragon K582 Surara](https://i.rtings.com/assets/products/eYjOTm39/redragon-k582-surara-rgb/backlight-large.jpg?format=auto "Redragon K582 SURARA RGB Review - RTINGS.com")](https://www.rtings.com/keyboard/reviews/redragon/k582-surara-rgb)

A 104-key keyboard with RGB lighting. This definition is for units with a Sonix MCU, which report USB ID `0c45:5004` on the stock firmware. Units with a CH555 MCU report `320f:5000` and cannot run this firmware.

* Keyboard Maintainer: [Robert S.](https://github.com/Wallvon)
* Based on Redragon K556 by: [Alexandre Oliveira](https://github.com/Xinayder)
* Hardware Supported: K582RGB, SN32F248BF
* Hardware Availability: [Redragon product page](https://www.redragonzone.com/detail.html?id=715)

Make example for this keyboard (after setting up your build environment):

    make redragon/k582:default

Flashing example for this keyboard, with [SonixFlasherC](https://github.com/SonixQMK/SonixFlasherC) installed and the keyboard in the bootloader:

    make redragon/k582:default:flash

To replace the stock firmware, build first and run this instead. It reboots the keyboard into the bootloader and flashes the new image:

    sonixflasher -v 0c45/5004 -r evision -f redragon_k582_default.bin

See the [build environment setup](https://docs.qmk.fm/newbs_getting_started) and the [make instructions](https://docs.qmk.fm/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/newbs).

## Bootloader

Press Fn+Esc to enter the bootloader.
