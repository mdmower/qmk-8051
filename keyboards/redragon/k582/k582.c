// Copyright 2026 Matt Mower (@mdmower)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"

// The Win Lock LED is on when its pin is low, as the other indicators are.

void keyboard_pre_init_kb(void) {
    gpio_set_pin_output(LED_WIN_LOCK_PIN);
    gpio_write_pin_high(LED_WIN_LOCK_PIN);
    keyboard_pre_init_user();
}

void housekeeping_task_kb(void) {
    gpio_write_pin(LED_WIN_LOCK_PIN, !keymap_config.no_gui);
}

// housekeeping_task_kb does not run during a suspend, so this function turns the LED off.
void suspend_power_down_kb(void) {
    gpio_write_pin_high(LED_WIN_LOCK_PIN);
    suspend_power_down_user();
}
