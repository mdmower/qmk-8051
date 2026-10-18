/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "k552.h"
#include "led_dma.h"

/* Caps Lock and Scroll Lock indicators (right of the logo): LEDs in group 7 of COMs 9 and 8, Caps Lock on the red
 * line and Scroll Lock on the green line (stock V0101 @0xd988). Buffer offsets: slot * 4 + colour byte. */
#define CAPS_LOCK_LED ((9 * LED_DMA_GROUPS + 7) * 4 + 1)
#define SCROLL_LOCK_LED ((8 * LED_DMA_GROUPS + 7) * 4 + 2)

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
        led_dma_buf[CAPS_LOCK_LED]   = led_state.caps_lock ? 0xFF : 0;
        led_dma_buf[SCROLL_LOCK_LED] = led_state.scroll_lock ? 0xFF : 0;
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
