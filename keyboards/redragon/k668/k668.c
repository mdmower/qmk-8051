/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "k668.h"
#include "led_pwm.h"

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

/* Key presses for the lighting effects: the LED of the key at matrix row r, column c is in row c of LED column r.
 * Events from outside the matrix have no LED. */
bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed && record->event.key.row < MATRIX_ROWS) {
        led_fx_key_pressed(record->event.key.row * LED_FX_ROWS + record->event.key.col);
    }
    return pre_process_record_user(keycode, record);
}

/* The lighting keycodes; the settings are not saved. */
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }
    if (keycode >= RD_KMODE && keycode <= RD_STOG) {
        if (record->event.pressed) {
            led_fx_adjust(keycode - RD_KMODE, get_mods() & MOD_MASK_SHIFT);
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
