/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "redragon_ch555.h"
#include "led_dma.h"
#include "flash_store.h"
#ifdef CH555_EEPROM_CODE_FLASH
#    include "eeprom_ch555_code_flash.h"
#endif
#ifdef REDRAGON_STORE_COMMANDS
#    if !defined(CH555_EEPROM_CODE_FLASH) || !defined(RAW_ENABLE) || defined(VIA_ENABLE)
#        error "REDRAGON_STORE_COMMANDS needs CH555_EEPROM_CODE_FLASH and RAW_ENABLE, and not VIA_ENABLE"
#    endif
#    include "eeprom.h"
#    include "raw_hid.h"
#    include "usb_descriptor.h" // RAW_EPSIZE
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

#ifdef REDRAGON_STORE_COMMANDS
/*
 * Raw HID: read-only access to the EEPROM, for a backup (util/evision_flash.py backup-settings).
 * Replies are 32 bytes, byte 0 the command.
 * STORE_INFO: reply bytes 1 format (1), 2-3 EEPROM_SIZE, 4-5 the store's code-flash address, 6-7
 * CH555_EEPROM_LAYOUT (16-bit values little-endian), the rest 0.
 * STORE_READ, bytes 1-2 offset (little-endian), 3 count (1-28): reply bytes 1-3 as sent, from byte 4 the count
 * EEPROM bytes at offset (the copy in xRAM, with changes not yet saved), the rest 0.
 * Any other command or a read outside the EEPROM: reply byte 0 0xFF, the rest 0.
 */
#    define STORE_INFO 0x80
#    define STORE_READ 0x81

void raw_hid_receive(uint8_t *data, uint8_t length) {
    static uint8_t reply[RAW_EPSIZE];
    uint8_t        cmd    = data[0];
    uint16_t       offset = data[1] | (uint16_t)data[2] << 8;
    uint8_t        count  = data[3];

    (void)length;
    memset(reply, 0, sizeof(reply));
    reply[0] = cmd;
    if (cmd == STORE_INFO) {
        reply[1] = 1;
        reply[2] = (uint8_t)(EEPROM_SIZE);
        reply[3] = (uint8_t)((EEPROM_SIZE) >> 8);
        reply[4] = (uint8_t)(CH555_EEPROM_CODE_FLASH_ADDR);
        reply[5] = (uint8_t)((CH555_EEPROM_CODE_FLASH_ADDR) >> 8);
        reply[6] = (uint8_t)(CH555_EEPROM_LAYOUT);
        reply[7] = (uint8_t)((CH555_EEPROM_LAYOUT) >> 8);
    } else if (cmd == STORE_READ && count >= 1 && count <= sizeof(reply) - 4 && offset <= EEPROM_SIZE - count) {
        reply[1] = (uint8_t)offset;
        reply[2] = offset >> 8;
        reply[3] = count;
        eeprom_read_block(reply + 4, (void *)offset, count);
    } else {
        reply[0] = 0xFF;
    }
    raw_hid_send(reply, sizeof(reply));
}
#endif

void redragon_suspend(bool suspended) {
#ifdef LED_INDICATOR_ENABLE_PIN
    gpio_write_pin(LED_INDICATOR_ENABLE_PIN, suspended);
#endif
    led_dma_output(!suspended);
}
