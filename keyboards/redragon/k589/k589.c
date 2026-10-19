/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "k589.h"

void keyboard_post_init_kb(void) {
    redragon_init();
    keyboard_post_init_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    return process_record_user(keycode, record) && redragon_process_record(keycode, record);
}

bool led_update_kb(led_t led_state) {
    bool res = led_update_user(led_state);

    if (res) {
        redragon_led_update(led_state);
    }
    return res;
}

void housekeeping_task_kb(void) {
    redragon_task();
}

/* All LEDs off while the host is suspended */
void suspend_power_down_kb(void) {
    redragon_suspend(true);
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
    redragon_suspend(false);
    suspend_wakeup_init_user();
}
