/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* QMK's EEPROM in SH68F90A code flash (EEPROM_DRIVER = vendor): a copy of EEPROM_SIZE bytes in xRAM, which reads
 * and writes use, saved to code flash SH68F90_EEPROM_SAVE_DELAY ms after the last write. */

/* Layout of the EEPROM contents, kept in each slot's header: a slot written with another value (or another
 * EEPROM_SIZE) is not loaded. A keymap that keeps its own data in the EEPROM sets it in config.h. */
#ifndef SH68F90_EEPROM_LAYOUT
#    define SH68F90_EEPROM_LAYOUT 0
#endif

/* Runs one step of a pending save: one sector erase, or the program and compare of up to 64 bytes. The CPU
 * idles during each SSP command, with interrupts off. Called from the main loop (protocol_post_task()). */
void eeprom_code_flash_task(void);
/* Clears the EEPROM: the copy reads 0x00, a running save ends, and the header sectors of both slots are erased, so
 * that the next start finds no valid slot. Returns 0 if both erase commands were started. */
uint8_t eeprom_code_flash_clear(void);
