/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "quantum.h"
#include "led_fx.h"

/*
 * Functions shared by the Redragon CH555 keyboards: Win Lock, the lighting settings (led_fx.h) and their storage
 * in the data flash (flash_store.h). The keyboard's own code calls them from its *_kb functions.
 *
 * Optional, in the keyboard's config.h:
 *   LED_INDICATOR_ENABLE_PIN  pin that enables the indicator LEDs, active low; off while the host is suspended
 *   LED_WIN_LOCK_PIN          Win Lock LED, active high
 *   LED_NUM_LOCK_BYTE, LED_CAPS_LOCK_BYTE, LED_SCROLL_LOCK_BYTE, LED_WIN_LOCK_BYTE
 *                             indicator LEDs in the LED matrix: index of the LED's color byte in led_dma_buf
 *                             (LED_DMA_BYTE in led_dma.h), 0xFF while the lock is on; the slot needs LED_FX_SKIP in
 *                             led_fx_pos.h. The lock indicators are set by redragon_led_update().
 *   REDRAGON_KB_SETTINGS      number of bytes in redragon_kb_settings[], saved after the lighting settings
 *
 * Optional, in a keymap's config.h:
 *   REDRAGON_STORE_COMMANDS   raw HID commands that read QMK's EEPROM in code flash (CH555_EEPROM_CODE_FLASH), for
 *                             util/evision_flash.py backup-settings; not with VIA, which has its own raw HID
 *                             commands. redragon_ch555.c defines raw_hid_receive().
 */

/* Keycodes from QK_KB_0: Win Lock, then the lighting settings in led_fx.h order (key LEDs, then side lights with
 * 2 zones); Shift steps a setting back. A keyboard's own keycodes start at RD_KB_SAFE_RANGE. */
enum redragon_keycodes {
    RD_WINLK = QK_KB_0,
    RD_KMODE,
    RD_KHUE,
    RD_KSAT,
    RD_KVAL,
    RD_KSPD,
    RD_KTOG,
#if LED_FX_ZONES > 1
    RD_SMODE,
    RD_SHUE,
    RD_SSAT,
    RD_SVAL,
    RD_SSPD,
    RD_STOG,
#endif
    RD_KB_SAFE_RANGE
};

#ifndef REDRAGON_KB_SETTINGS
#    define REDRAGON_KB_SETTINGS 0
#endif
#if REDRAGON_KB_SETTINGS > 0
/* Defined by the keyboard; saved with the lighting settings SAVE_DELAY after redragon_settings_changed(). */
extern __xdata uint8_t redragon_kb_settings[REDRAGON_KB_SETTINGS];
#endif

/* From keyboard_post_init_kb: sets up the LED pins and loads the saved settings. Returns the keyboard's saved
 * bytes (REDRAGON_KB_SETTINGS, in code space), or 0 if nothing has been saved. */
__code const uint8_t *redragon_init(void);
/* From process_record_kb: Win Lock, the lighting keycodes and QK_CLEAR_EEPROM (pressed with Ctrl held, on release:
 * erases the data flash and, with CH555_EEPROM_CODE_FLASH, QMK's EEPROM, then restarts the keyboard). Returns false for a key it has handled or
 * blocked. */
bool redragon_process_record(uint16_t keycode, keyrecord_t *record);
/* From housekeeping_task_kb: lighting effects and the delayed save. */
void redragon_task(void);
#if defined(LED_NUM_LOCK_BYTE) || defined(LED_CAPS_LOCK_BYTE) || defined(LED_SCROLL_LOCK_BYTE)
/* From led_update_kb: the lock indicators in the LED matrix (LED_*_LOCK_BYTE). */
void redragon_led_update(led_t led_state);
#endif
/* From suspend_power_down_kb (true) and suspend_wakeup_init_kb (false): LEDs off while the host is suspended. */
void redragon_suspend(bool suspended);
/* Saves the settings SAVE_DELAY after the last call. */
void redragon_settings_changed(void);
