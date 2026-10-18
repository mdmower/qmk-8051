/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "k580.h"

/* [0]: roller mode, saved with the lighting settings; 1 = key LED brightness, 0 = volume */
__xdata uint8_t redragon_kb_settings[REDRAGON_KB_SETTINGS];
#define roller_light redragon_kb_settings[0]

void keyboard_post_init_kb(void) {
    __code const uint8_t *p = redragon_init();

    if (p) {
        roller_light = p[0] == 1;
    }
    keyboard_post_init_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record) || !redragon_process_record(keycode, record)) {
        return false;
    }
    switch (keycode) {
    case K580_ROLL:
        if (record->event.pressed) {
            roller_light = !roller_light;
            redragon_settings_changed();
        }
        return false;
    case K580_RLDN:
    case K580_RLUP:
        if (roller_light) {
            if (record->event.pressed) {
                led_fx_adjust(LED_FX_VAL, keycode == K580_RLDN);
                redragon_settings_changed();
            }
        } else if (record->event.pressed) {
            register_code(keycode == K580_RLDN ? KC_VOLD : KC_VOLU);
        } else {
            unregister_code(keycode == K580_RLDN ? KC_VOLD : KC_VOLU);
        }
        return false;
    }
    return true;
}

void housekeeping_task_kb(void) {
    redragon_task();
}

/* Indicator LEDs (P5.4) and key LEDs off while the host is suspended (stock 1.04 @0x794f) */
void suspend_power_down_kb(void) {
    redragon_suspend(true);
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
    redragon_suspend(false);
    suspend_wakeup_init_user();
}
