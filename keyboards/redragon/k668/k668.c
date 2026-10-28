/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "quantum.h"

/* Esc held at power-up starts the bootloader, before USB starts. Esc is on the strobe P6.0 and the input P7.0. */
void keyboard_pre_init_kb(void) {
    gpio_set_pin_input_high(PORT7_0);
    gpio_set_pin_output(PORT6_0);
    gpio_write_pin_low(PORT6_0);
    wait_us(10);
    if (!gpio_read_pin(PORT7_0)) {
        bootloader_jump();
    }
    gpio_set_pin_input_high(PORT6_0);

    keyboard_pre_init_user();
}
