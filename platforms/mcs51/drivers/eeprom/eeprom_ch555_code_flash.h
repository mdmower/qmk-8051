/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <stdbool.h>

/* QMK's EEPROM in CH555 code flash (EEPROM_DRIVER = vendor): a copy of EEPROM_SIZE bytes in xRAM, which reads and
 * writes use, saved to code flash CH555_EEPROM_SAVE_DELAY ms after the last write. */

/* Layout of the EEPROM contents, kept in each slot's header: a slot written with another value (or another
 * EEPROM_SIZE) is not loaded. A keymap that keeps its own data in the EEPROM sets it in config.h. */
#ifndef CH555_EEPROM_LAYOUT
#    define CH555_EEPROM_LAYOUT 0
#endif

/* Runs at most one flash operation of a pending save (2-8 ms, the CPU stops; datasheet 19.5). Called from
 * the main loop (protocol_post_task()). */
void eeprom_code_flash_task(void);
/* True while changes wait for their save (also during the delay before it) or a save runs; false after three failed
 * saves, when no further save starts. */
bool eeprom_code_flash_busy(void);
/* Clears the EEPROM: the copy reads 0x00, a running save ends, and the header pages of both slots are erased, so
 * that the next start finds no valid slot, as after a flash. Returns 0 on success. */
uint8_t eeprom_code_flash_clear(void);
