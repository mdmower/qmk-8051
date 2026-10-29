/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/*
 * QMK's EEPROM in SH68F90A code flash (EEPROM_DRIVER = vendor).
 *
 * The EEPROM is a copy of EEPROM_SIZE bytes in xRAM. Reads and writes use the copy; the copy is saved to
 * code flash SH68F90_EEPROM_SAVE_DELAY ms after the last write. The flash store holds two slots of whole
 * 512-byte sectors; a slot holds EEPROM_SIZE data bytes, followed by a 16-byte header at the end of its last
 * sector. A save erases the slot that does not hold the newest copy, header sector first, programs the data,
 * compares it with the copy, and programs the header last: a power loss during a save leaves the previous slot
 * as the newest valid one. At startup the valid slot with the higher sequence number is loaded; without one,
 * the EEPROM reads 0x00 and QMK writes its defaults. A slot whose header holds another SH68F90_EEPROM_LAYOUT
 * (config.h, default 0) or EEPROM_SIZE is not valid.
 *
 * Flash is erased and programmed through the MCU's sector self-programming (SSP), which can write any sector
 * of code flash. Only eeprom_sh68f90_code_flash.c starts SSP commands, each for one byte or one sector of the
 * store; the address is checked against the store bounds after it is loaded into XPAGE and IB_OFFSET, right
 * before the command. Code is linked below the store (CODE_SIZE).
 */

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
