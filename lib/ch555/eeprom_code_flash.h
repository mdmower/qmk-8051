/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/*
 * QMK's EEPROM in CH555 code flash (CH555_EEPROM_CODE_FLASH = yes in rules.mk, which selects
 * EEPROM_DRIVER = custom; lib/ch555/mcs51_mcu.mk).
 *
 * The EEPROM is a copy of EEPROM_SIZE bytes in xRAM. Reads and writes use the copy; the copy is saved to
 * code flash CH555_EEPROM_SAVE_DELAY ms after the last write. The flash store at the top of the app's
 * code space holds two slots of EEPROM_SIZE data bytes and a 64-byte header page each. A save erases the
 * slot that does not hold the newest copy, programs the data, compares it with the copy, and programs
 * the header last: a power loss during a save leaves the previous slot as the newest valid one. At
 * startup the valid slot with the higher sequence number is loaded; without one, the EEPROM reads 0x00
 * and QMK writes its defaults. A slot whose header names another CH555_EEPROM_LAYOUT (config.h, default 0)
 * or EEPROM_SIZE is not valid. A flash image written by the bootloader erases the store.
 *
 * Code-flash writes need bCODE_WE, which enables writes to all of code flash, the bootloader included.
 * Only eeprom_code_flash.c sets it, for one erase or program of one store page; the page number is
 * checked against the store size, and the address in ROM_ADDR against the store bounds again after
 * write enable, right before the command. The build keeps code below the store (CODE_SIZE).
 */

/* Layout of the EEPROM contents, kept in each slot's header: a slot written with another value (or another
 * EEPROM_SIZE) is not loaded. A keymap that keeps its own data in the EEPROM sets it in config.h. */
#ifndef CH555_EEPROM_LAYOUT
#    define CH555_EEPROM_LAYOUT 0
#endif

/* Runs at most one flash operation of a pending save (2-8 ms, the CPU stops; datasheet 19.5). Called from
 * the main loop (protocol_post_task()). */
void eeprom_code_flash_task(void);
/* Clears the EEPROM: the copy reads 0x00, a running save ends, and the header pages of both slots are erased, so
 * that the next start finds no valid slot, as after a flash. Returns 0 on success. */
uint8_t eeprom_code_flash_clear(void);
