/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "redragon_ch555.h"
#include "led_dma.h"
#include "flash_store.h"
#ifdef CH555_EEPROM_CODE_FLASH
#    include "eeprom_code_flash.h"
#endif

_Static_assert(LED_FX_SAVE_SIZE + REDRAGON_KB_SETTINGS <= FLASH_STORE_SIZE, "saved settings: at most FLASH_STORE_SIZE bytes");

/* Win Lock: while it is on, LGUI/RGUI presses are blocked and the Win Lock LED (if any) is lit. Releases pass, so a
 * GUI key held when Win Lock turns on is released on the host. */
static bool win_lock = false;

/* QK_CLEAR_EEPROM was pressed with Ctrl held */
static bool clear_armed = false;

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
#ifdef LED_WIN_LOCK_BYTE
            led_dma_buf[LED_WIN_LOCK_BYTE] = win_lock ? 0xFF : 0;
#endif
        }
        return false;
    case QK_CLEAR_EEPROM:
        /* Only with Ctrl held at the press. On release, so that the key is up when the keyboard starts again. */
        if (record->event.pressed) {
            clear_armed = (get_mods() & MOD_MASK_CTRL) != 0;
        } else if (clear_armed) {
            flash_store_clear();
#ifdef CH555_EEPROM_CODE_FLASH
            eeprom_code_flash_clear();
#endif
            soft_reset_keyboard();
        }
        return false;
    case KC_LGUI:
    case KC_RGUI:
        return !win_lock || !record->event.pressed;
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

#if defined(LED_NUM_LOCK_BYTE) || defined(LED_CAPS_LOCK_BYTE) || defined(LED_SCROLL_LOCK_BYTE)
void redragon_led_update(led_t led_state) {
#    ifdef LED_NUM_LOCK_BYTE
    led_dma_buf[LED_NUM_LOCK_BYTE] = led_state.num_lock ? 0xFF : 0;
#    endif
#    ifdef LED_CAPS_LOCK_BYTE
    led_dma_buf[LED_CAPS_LOCK_BYTE] = led_state.caps_lock ? 0xFF : 0;
#    endif
#    ifdef LED_SCROLL_LOCK_BYTE
    led_dma_buf[LED_SCROLL_LOCK_BYTE] = led_state.scroll_lock ? 0xFF : 0;
#    endif
}
#endif

void redragon_suspend(bool suspended) {
#ifdef LED_INDICATOR_ENABLE_PIN
    gpio_write_pin(LED_INDICATOR_ENABLE_PIN, suspended);
#endif
    led_dma_output(!suspended);
}
