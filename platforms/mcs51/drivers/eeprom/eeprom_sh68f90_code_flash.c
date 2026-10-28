/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>
#include "sh68f90a.h"
#include "eeprom_driver.h"
#include "timer.h"
#include "eeprom_sh68f90_code_flash.h"

/* SH68F90.mk sets the store's bounds: it occupies [SH68F90_EEPROM_CODE_FLASH_ADDR, SH68F90_EEPROM_CODE_FLASH_END). */
#if !defined(EEPROM_SIZE) || !defined(SH68F90_EEPROM_CODE_FLASH_ADDR) || !defined(SH68F90_EEPROM_CODE_FLASH_END)
#    error "EEPROM_SIZE, SH68F90_EEPROM_CODE_FLASH_ADDR and SH68F90_EEPROM_CODE_FLASH_END are not defined"
#endif

#ifndef SH68F90_EEPROM_SAVE_DELAY
#    define SH68F90_EEPROM_SAVE_DELAY 2000
#endif

#define SECTOR_SIZE 512
#define HEADER_SIZE 16
#define SLOT_SECTORS ((EEPROM_SIZE + HEADER_SIZE) / SECTOR_SIZE)
#define SLOT_SIZE ((uint16_t)SLOT_SECTORS * SECTOR_SIZE)
#define STORE_ADDR ((uint16_t)(SH68F90_EEPROM_CODE_FLASH_ADDR))
#define STORE_LAST ((uint16_t)((SH68F90_EEPROM_CODE_FLASH_END) - 1))
#define PROGRAM_STEP 64 // bytes programmed per call of the task
#define PROGRAM_STEPS ((EEPROM_SIZE + PROGRAM_STEP - 1) / PROGRAM_STEP)

_Static_assert(EEPROM_SIZE > 0 && (EEPROM_SIZE + HEADER_SIZE) % SECTOR_SIZE == 0, "EEPROM_SIZE: 512 * n - 16");
_Static_assert((SH68F90_EEPROM_CODE_FLASH_ADDR) % SECTOR_SIZE == 0 && (SH68F90_EEPROM_CODE_FLASH_END) % SECTOR_SIZE == 0, "store bounds: 512-byte sectors");
_Static_assert((SH68F90_EEPROM_CODE_FLASH_END) - (SH68F90_EEPROM_CODE_FLASH_ADDR) == 2L * SLOT_SECTORS * SECTOR_SIZE, "store size: two slots");
_Static_assert((SH68F90_EEPROM_CODE_FLASH_END) <= 0x10000L, "store above 64 KB");

#define IN_STORE(a) ((a) >= STORE_ADDR && (a) <= STORE_LAST)

/* SSP commands (IB_CON1) and the keys that start them (IB_CON2-5, datasheet 7.4.1) */
#define SSP_ERASE 0xE6
#define SSP_PROGRAM 0x6E
#define SSP_KEY2 0x05
#define SSP_KEY3 0x0A
#define SSP_KEY4 0x09
#define SSP_KEY5 0x06

static __xdata uint8_t ee_copy[EEPROM_SIZE];
static __xdata uint8_t header_buf[HEADER_SIZE];

/* Header at the end of a slot's last sector. Bytes are programmed in address order, so a header cut short lacks
 * its last byte, HEADER_VALID. */
#define HEADER_MAGIC0 'Q'
#define HEADER_MAGIC1 'E'
#define HEADER_VALID 0xA5
#define H_MAGIC0 0
#define H_MAGIC1 1
#define H_SEQ_LO 2
#define H_SEQ_HI 3
#define H_SUM_LO 4
#define H_SUM_HI 5
#define H_CHECK 6 // 0xFF minus the sum of bytes 0-14 except this one (bytes 10-14 are 0)
#define H_LAYOUT_LO 7
#define H_LAYOUT_HI 8
#define H_SECTORS 9
#define H_VALID (HEADER_SIZE - 1)

#define NO_SLOT 0xFF

static uint8_t  newest      = NO_SLOT; // slot of the newest valid copy
static uint16_t newest_seq  = 0;
static bool     dirty       = false; // the xRAM copy changed since the newest slot or since the save began
static uint16_t last_write  = 0;
static uint8_t  save_slot   = NO_SLOT; // slot being written, NO_SLOT when no save runs
static uint8_t  save_step   = 0;       // next step of the save: erase 0..SLOT_SECTORS-1, then program
static uint8_t  save_errors = 0;

/* Runs one SSP command on the byte or the sector at addr (datasheet 7.4.3). XPAGE holds bits 8-15 of the address
 * (the sector number and bit 8 of the offset in the sector), IB_OFFSET bits 0-7; FLASHCON.FAC clear selects the
 * main block. The CPU idles until the command ends and continues after the NOPs. Interrupts are off throughout.
 * Returns 0 if the command was started. */
static uint8_t ssp_command(uint16_t addr, uint8_t command, uint8_t data) {
    uint8_t status = 0xFF;

    if (!IN_STORE(addr)) return status;

    __critical {
        FLASHCON  = 0;
        XPAGE     = addr >> 8;
        IB_OFFSET = addr & 0xFF;
        IB_DATA   = data;
        if (IN_STORE((uint16_t)XPAGE << 8 | IB_OFFSET)) {
            IB_CON1 = command;
            IB_CON2 = SSP_KEY2;
            IB_CON3 = SSP_KEY3;
            IB_CON4 = SSP_KEY4;
            IB_CON5 = SSP_KEY5;
            __asm__("nop");
            __asm__("nop");
            __asm__("nop");
            __asm__("nop");
            IB_CON1 = 0;
            IB_CON2 = 0;
            IB_CON3 = 0;
            IB_CON4 = 0;
            IB_CON5 = 0;
            status  = 0;
        }
        XPAGE = 0;
    }
    return status;
}

static uint16_t slot_addr(uint8_t slot) {
    return STORE_ADDR + slot * SLOT_SIZE;
}

/* Programs len bytes at addr from src and compares them with src. Returns 0 on success. */
static uint8_t program(uint16_t addr, __xdata const uint8_t *src, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (ssp_command(addr + i, SSP_PROGRAM, src[i]) != 0) return 1;
    }
    return memcmp((__code const uint8_t *)addr, src, len) != 0;
}

/* Rotate-and-add sum of a slot's data as stored in flash */
static uint16_t data_sum(uint8_t slot) {
    __code const uint8_t *p   = (__code const uint8_t *)slot_addr(slot);
    uint16_t              sum = 0;
    for (uint16_t i = 0; i < EEPROM_SIZE; i++) {
        sum = ((sum << 1) | (sum >> 15)) + p[i];
    }
    return sum;
}

static uint8_t header_check(const uint8_t *h) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < H_VALID; i++) {
        if (i != H_CHECK) sum += h[i];
    }
    return 0xFF - sum;
}

/* A slot is valid with a complete header of this layout whose sum matches its data. */
static bool slot_valid(uint8_t slot, uint16_t *seq) {
    __code const uint8_t *h = (__code const uint8_t *)(slot_addr(slot) + EEPROM_SIZE);
    if (h[H_VALID] != HEADER_VALID || h[H_MAGIC0] != HEADER_MAGIC0 || h[H_MAGIC1] != HEADER_MAGIC1 || h[H_CHECK] != header_check(h)) {
        return false;
    }
    if (h[H_LAYOUT_LO] != (uint8_t)(SH68F90_EEPROM_LAYOUT) || h[H_LAYOUT_HI] != (uint8_t)((SH68F90_EEPROM_LAYOUT) >> 8) || h[H_SECTORS] != SLOT_SECTORS) {
        return false;
    }
    if (data_sum(slot) != (h[H_SUM_LO] | (uint16_t)h[H_SUM_HI] << 8)) {
        return false;
    }
    *seq = h[H_SEQ_LO] | (uint16_t)h[H_SEQ_HI] << 8;
    return true;
}

void eeprom_driver_init(void) {
    uint16_t seq0, seq1;
    bool     valid0 = slot_valid(0, &seq0);
    bool     valid1 = slot_valid(1, &seq1);

    if (valid0 && (!valid1 || (int16_t)(seq0 - seq1) > 0)) {
        newest     = 0;
        newest_seq = seq0;
    } else if (valid1) {
        newest     = 1;
        newest_seq = seq1;
    }
    if (newest != NO_SLOT) {
        memcpy(ee_copy, (__code const uint8_t *)slot_addr(newest), EEPROM_SIZE);
    } else {
        memset(ee_copy, 0, EEPROM_SIZE);
    }
}

static void mark_dirty(void) {
    dirty      = true;
    last_write = timer_read();
}

void eeprom_driver_erase(void) {
    memset(ee_copy, 0, EEPROM_SIZE);
    mark_dirty();
}

void eeprom_driver_format(bool erase) {
    if (erase) {
        eeprom_driver_erase();
    }
}

void eeprom_read_block(void *buf, const void *addr, size_t len) {
    uint16_t offset = (uint16_t)addr;
    if (offset >= EEPROM_SIZE) return;
    if (len > EEPROM_SIZE - offset) len = EEPROM_SIZE - offset;
    memcpy(buf, &ee_copy[offset], len);
}

void eeprom_write_block(const void *buf, void *addr, size_t len) {
    uint16_t offset = (uint16_t)addr;
    if (offset >= EEPROM_SIZE) return;
    if (len > EEPROM_SIZE - offset) len = EEPROM_SIZE - offset;
    if (len && memcmp(&ee_copy[offset], buf, len) != 0) {
        memcpy(&ee_copy[offset], buf, len);
        mark_dirty();
    }
}

static void save_stop(bool failed) {
    save_slot = NO_SLOT;
    if (failed) {
        save_errors++;
        mark_dirty(); // again after the delay
    }
}

/* Steps of a save: erase the sector with the header, then the other sectors of the slot; program the data in
 * steps of PROGRAM_STEP bytes, each compared with the copy; program the header with the sum of the data in flash.
 * A write to the copy during a save ends it before its header, and a new save starts after the delay. */
void eeprom_code_flash_task(void) {
    if (save_slot == NO_SLOT) {
        if (!dirty || timer_elapsed(last_write) < SH68F90_EEPROM_SAVE_DELAY || save_errors >= 3) return;
        dirty     = false;
        save_slot = (newest == 0) ? 1 : 0;
        save_step = 0;
        return;
    }
    if (dirty) {
        save_stop(false);
        return;
    }

    if (save_step < SLOT_SECTORS) {
        uint16_t addr = slot_addr(save_slot) + (uint16_t)(SLOT_SECTORS - 1 - save_step) * SECTOR_SIZE;
        if (ssp_command(addr, SSP_ERASE, 0) != 0) {
            save_stop(true);
            return;
        }
    } else if (save_step < SLOT_SECTORS + PROGRAM_STEPS) {
        uint16_t offset = (uint16_t)(save_step - SLOT_SECTORS) * PROGRAM_STEP;
        uint8_t  len    = (EEPROM_SIZE - offset < PROGRAM_STEP) ? EEPROM_SIZE - offset : PROGRAM_STEP;
        if (program(slot_addr(save_slot) + offset, &ee_copy[offset], len) != 0) {
            save_stop(true);
            return;
        }
    } else {
        uint16_t seq = newest_seq + 1;
        uint16_t sum = data_sum(save_slot);
        memset(header_buf, 0, HEADER_SIZE);
        header_buf[H_MAGIC0]    = HEADER_MAGIC0;
        header_buf[H_MAGIC1]    = HEADER_MAGIC1;
        header_buf[H_SEQ_LO]    = seq & 0xFF;
        header_buf[H_SEQ_HI]    = seq >> 8;
        header_buf[H_SUM_LO]    = sum & 0xFF;
        header_buf[H_SUM_HI]    = sum >> 8;
        header_buf[H_LAYOUT_LO] = (uint8_t)(SH68F90_EEPROM_LAYOUT);
        header_buf[H_LAYOUT_HI] = (uint8_t)((SH68F90_EEPROM_LAYOUT) >> 8);
        header_buf[H_SECTORS]   = SLOT_SECTORS;
        header_buf[H_CHECK]     = header_check(header_buf);
        header_buf[H_VALID]     = HEADER_VALID;
        if (program(slot_addr(save_slot) + EEPROM_SIZE, header_buf, HEADER_SIZE) != 0) {
            save_stop(true);
            return;
        }
        newest      = save_slot;
        newest_seq  = seq;
        save_errors = 0;
        save_stop(false);
        return;
    }
    save_step++;
}
