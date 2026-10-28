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

    gpio_set_pin_output(LED_WIN_LOCK_PIN);
    gpio_write_pin_high(LED_WIN_LOCK_PIN);

    keyboard_pre_init_user();
}

static bool suspended = false;

/* The Win Lock LED is lit while the GUI keys are off (keymap_config.no_gui, toggled with GU_TOGG), except while the
 * host is suspended. */
void housekeeping_task_kb(void) {
    gpio_write_pin(LED_WIN_LOCK_PIN, !keymap_config.no_gui || suspended);

    housekeeping_task_user();
}

void suspend_power_down_kb(void) {
    suspended = true;
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
    suspended = false;
    suspend_wakeup_init_user();
}
