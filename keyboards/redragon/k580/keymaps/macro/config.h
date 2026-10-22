/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* G1-G5 and Rec LEDs show the macro recorder's state (led_fx_mono_val() in keymap.c) */
#define LED_FX_MONO_HOOK

/* Layout of the macros in the EEPROM (lib/ch555/eeprom_code_flash.c): format 1 in the high byte, EECONFIG_SIZE
 * (where the macros start) in the low byte; keymap.c checks the low byte. */
#define CH555_EEPROM_LAYOUT 0x0125
