/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "redragon_ch555.h"
#include "led_dma.h"
#include "flash_store.h"

_Static_assert(LED_FX_SAVE_SIZE + REDRAGON_KB_SETTINGS <= FLASH_STORE_SIZE, "saved settings: at most FLASH_STORE_SIZE bytes");

/* Win Lock: while it is on, LGUI/RGUI are blocked and the Win Lock LED (if any) is lit. */
static bool win_lock = false;

/* Settings saved in the data flash SAVE_DELAY after the last change: the lighting settings (led_fx.c), then
 * redragon_kb_settings[]. */
#define SAVE_DELAY 3000 // ms
static uint16_t save_time;
static bool     save_pending = false;

void redragon_settings_changed(void) {
    save_pending = true;
    save_time    = timer_read();
}

__code const uint8_t *redragon_init(void) {
    __code const uint8_t *p;

#ifdef LED_INDICATOR_ENABLE_PIN
    gpio_set_pin_output(LED_INDICATOR_ENABLE_PIN);
    gpio_write_pin_low(LED_INDICATOR_ENABLE_PIN);
#endif
#ifdef LED_WIN_LOCK_PIN
    gpio_set_pin_output(LED_WIN_LOCK_PIN);
    gpio_write_pin_low(LED_WIN_LOCK_PIN);
#endif

    p = flash_store_find();
    if (!p) {
        return 0;
    }
    led_fx_load(p);
    return p + LED_FX_SAVE_SIZE;
}

bool redragon_process_record(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= RD_KMODE && keycode < RD_KB_SAFE_RANGE) {
        if (record->event.pressed) {
            led_fx_adjust(keycode - RD_KMODE, get_mods() & MOD_MASK_SHIFT);
            redragon_settings_changed();
        }
        return false;
    }
    switch (keycode) {
    case RD_WINLK:
        if (record->event.pressed) {
            win_lock = !win_lock;
#ifdef LED_WIN_LOCK_PIN
            gpio_write_pin(LED_WIN_LOCK_PIN, win_lock);
#endif
        }
        return false;
    case KC_LGUI:
    case KC_RGUI:
        return !win_lock;
    }
    return true;
}

void redragon_task(void) {
    __xdata uint8_t rec[LED_FX_SAVE_SIZE + REDRAGON_KB_SETTINGS];

    led_fx_task();
    if (save_pending && timer_elapsed(save_time) >= SAVE_DELAY) {
        save_pending = false;
        led_fx_save(rec);
#if REDRAGON_KB_SETTINGS > 0
        {
            uint8_t i;
            for (i = 0; i < REDRAGON_KB_SETTINGS; i++) {
                rec[LED_FX_SAVE_SIZE + i] = redragon_kb_settings[i];
            }
        }
#endif
        flash_store_save(rec, sizeof(rec));
    }
}

void redragon_suspend(bool suspended) {
#ifdef LED_INDICATOR_ENABLE_PIN
    gpio_write_pin(LED_INDICATOR_ENABLE_PIN, suspended);
#endif
    led_dma_output(!suspended);
}
