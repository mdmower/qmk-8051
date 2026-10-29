/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "k668.h"
#include "led_pwm.h"
#ifdef EEPROM_SH68F90_CODE_FLASH
#    include "eeprom_sh68f90_code_flash.h"
#endif

_Static_assert(EECONFIG_KB_DATA_SIZE == LED_FX_SAVE_SIZE, "EECONFIG_KB_DATA_SIZE: LED_FX_SAVE_SIZE bytes");

/* Writes the lighting settings to the EEPROM data block. */
static void save_settings(void) {
    __xdata uint8_t buf[LED_FX_SAVE_SIZE];

    led_fx_save(buf);
    eeconfig_update_kb_datablock(buf, 0, sizeof(buf));
}

/* Called at startup, before keyboard_post_init_kb(), when the EEPROM has no valid eeconfig or data block: the
 * lighting settings are still the defaults of led_fx.c. */
void eeconfig_init_kb_datablock(void) {
    save_settings();
}

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

void keyboard_post_init_kb(void) {
    __xdata uint8_t buf[LED_FX_SAVE_SIZE];

    eeconfig_read_kb_datablock(buf, 0, sizeof(buf));
    led_fx_load(buf);

    keyboard_post_init_user();
}

/* Key presses for the lighting effects: the LED of the key at matrix row r, column c is in row c of LED column r.
 * Events from outside the matrix have no LED. */
bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed && record->event.key.row < MATRIX_ROWS) {
        led_fx_key_pressed(record->event.key.row * LED_FX_ROWS + record->event.key.col);
    }
    return pre_process_record_user(keycode, record);
}

/* QK_CLEAR_EEPROM was pressed with Ctrl held */
static bool clear_armed = false;

/* The lighting keycodes, each change written to the EEPROM data block, QK_CLEAR_EEPROM, and the keycodes with a GUI
 * modifier. */
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }
    if (keycode >= RD_KMODE && keycode <= RD_KVALD) {
        if (record->event.pressed) {
            bool reverse = get_mods() & MOD_MASK_SHIFT;
            if (keycode == RD_KVALD) {
                keycode = RD_KVAL;
                reverse = !reverse;
            }
            led_fx_adjust(keycode - RD_KMODE, reverse);
            save_settings();
        }
        return false;
    }
    /* Only with Ctrl held at the press. On release, so that the key is up when the keyboard starts again. QMK's own
     * clear writes only the EEPROM copy in xRAM, and its soft reset comes before the delayed save. */
    if (keycode == QK_CLEAR_EEPROM) {
        if (record->event.pressed) {
            clear_armed = (get_mods() & MOD_MASK_CTRL) != 0;
        } else if (clear_armed) {
#ifdef EEPROM_SH68F90_CODE_FLASH
            eeprom_code_flash_clear();
#endif
            soft_reset_keyboard();
        }
        return false;
    }
    /* A keycode with a GUI modifier, such as LGUI(KC_L), sends it also while Win Lock is on: register_code16() adds
     * the keycode's modifiers without mod_config(), which removes GUI while keymap_config.no_gui is set. */
    if (IS_QK_MODS(keycode) && (QK_MODS_GET_MODS(keycode) & MOD_LGUI)) {
        if (record->event.pressed) {
            register_code16(keycode);
        } else {
            unregister_code16(keycode);
        }
        return false;
    }
    return true;
}

/* The LEDs are off in the bootloader; the PWM banks would otherwise keep running with the last column's duties. */
bool shutdown_kb(bool jump_to_bootloader) {
    if (!shutdown_user(jump_to_bootloader)) {
        return false;
    }
    led_pwm_stop();
    return true;
}

static bool suspended = false;

/* The Win Lock LED is lit while the GUI keys are off (keymap_config.no_gui, toggled with GU_TOGG), except while the
 * host is suspended. The lighting effects are rendered here. */
void housekeeping_task_kb(void) {
    gpio_write_pin(LED_WIN_LOCK_PIN, !keymap_config.no_gui || suspended);
    led_fx_task();

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
