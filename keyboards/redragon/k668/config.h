/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* LEDs for the lighting effects: one column per matrix row, 7 LEDs per column (led_pwm.h); the key LEDs and the
 * side lights (LED row 6) have separate settings. */
#define LED_FX_COLUMNS MATRIX_ROWS
#define LED_FX_ROWS 7
#define LED_FX_ZONES 2
/* The lighting settings are saved in the keyboard's EEPROM data block: LED_FX_SAVE_SIZE bytes (led_fx.h). */
#define EECONFIG_KB_DATA_SIZE 12

/* Win Lock LED, active low */
#define LED_WIN_LOCK_PIN PORT4_7

/* The NKRO report has a 14-byte key bitmap (usages 0x00-0x6F) so that it fits a 16-byte packet of the shared
 * endpoint. The keymap has no keycode above 0x6F. */
#define NKRO_REPORT_BITS 14

/* Tap keys (LT, MT, TT, *_T, tap-toggle) and one-shot keys (OSM, OSL) are off: action_tapping.c does not compile
 * with SDCC (compound literal assignments, case ranges). MO() works. */
#define NO_ACTION_TAPPING
#define NO_ACTION_ONESHOT
