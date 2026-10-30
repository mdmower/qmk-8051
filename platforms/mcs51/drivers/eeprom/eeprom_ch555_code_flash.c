/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>
#include "ch555.h"
#include "eeprom_driver.h"
#include "timer.h"
#include "eeprom_ch555_code_flash.h"

/* The MCU's build rules set the store's bounds: it occupies [CH555_EEPROM_CODE_FLASH_ADDR,
 * CH555_EEPROM_CODE_FLASH_END), and code is linked below it (CODE_SIZE). */
#if !defined(EEPROM_SIZE) || !defined(CH555_EEPROM_CODE_FLASH_ADDR) || !defined(CH555_EEPROM_CODE_FLASH_END) || !defined(CH555_EEPROM_CODE_FLASH_XRAM)
#    error "EEPROM_SIZE, CH555_EEPROM_CODE_FLASH_ADDR, CH555_EEPROM_CODE_FLASH_END or CH555_EEPROM_CODE_FLASH_XRAM is not defined"
#endif

#ifndef CH555_EEPROM_SAVE_DELAY
#    define CH555_EEPROM_SAVE_DELAY 2000
#endif

/* The store holds two slots of EEPROM_SIZE data bytes and a 64-byte header page each. */
#define STORE_ADDR ((uint16_t)(CH555_EEPROM_CODE_FLASH_ADDR))
#define STORE_END ((uint16_t)(CH555_EEPROM_CODE_FLASH_END))
#define DATA_PAGES (EEPROM_SIZE / ROM_PAGE_SIZE)
#define SLOT_PAGES (DATA_PAGES + 1) // data pages, then the header page
#define STORE_PAGES (2 * SLOT_PAGES)

_Static_assert(EEPROM_SIZE >= ROM_PAGE_SIZE && EEPROM_SIZE % ROM_PAGE_SIZE == 0, "EEPROM_SIZE: a multiple of 64");
_Static_assert(STORE_PAGES <= 255, "EEPROM_SIZE too large for 8-bit page numbers");
_Static_assert(STORE_ADDR % ROM_PAGE_SIZE == 0 && STORE_END % ROM_PAGE_SIZE == 0, "store bounds: 64-byte pages");
_Static_assert(STORE_END - STORE_ADDR == STORE_PAGES * ROM_PAGE_SIZE, "store size: two slots");
/* Never the first 10 KB (an OEM bootloader and its boot marker on Evision boards), the data flash or the
 * WCH bootloader */
_Static_assert(STORE_ADDR >= 0x2800 && STORE_END <= DATA_FLASH_ADDR, "store outside 0x2800-0xEFFF");
_Static_assert(CH555_EEPROM_CODE_FLASH_XRAM % ROM_PAGE_SIZE == 0, "xRAM copy: 64-byte aligned");
_Static_assert(CH555_EEPROM_CODE_FLASH_XRAM >= 0x1000 && CH555_EEPROM_CODE_FLASH_XRAM + EEPROM_SIZE + ROM_PAGE_SIZE <= 0x2000, "xRAM copy outside 0x1000-0x1FFF");

#define IN_STORE(a) ((a) >= STORE_ADDR && (a) < STORE_END)

/* The EEPROM copy and the header buffer, outside the linker's xRAM and 64-byte aligned: the block program
 * reads its data from xRAM at the same offset in a 64-byte block as the flash address (datasheet 6.5). */
static __xdata __at(CH555_EEPROM_CODE_FLASH_XRAM) uint8_t ee_copy[EEPROM_SIZE];
static __xdata __at(CH555_EEPROM_CODE_FLASH_XRAM + EEPROM_SIZE) uint8_t header_buf[ROM_PAGE_SIZE];

/* Header page of a slot. Erased flash reads 0x00; the block program writes in address order, so a header
 * cut short lacks its last byte, HEADER_VALID. */
#define HEADER_MAGIC0 'Q'
#define HEADER_MAGIC1 'E'
#define HEADER_VALID 0xA5
#define H_MAGIC0 0
#define H_MAGIC1 1
#define H_SEQ_LO 2
#define H_SEQ_HI 3
#define H_SUM_LO 4
#define H_SUM_HI 5
#define H_CHECK 6 // 0xFF minus the sum of bytes 0-62 except this one (bytes 10-62 are 0)
#define H_LAYOUT_LO 7
#define H_LAYOUT_HI 8
#define H_DATA_PAGES 9
#define H_VALID (ROM_PAGE_SIZE - 1)

#define NO_SLOT 0xFF

/* DPTR as one 16-bit SFR: the block program reads its data from xRAM at DPTR. */
__sfr16 __at(0x8382) CF_DPTR;

/* GLOBAL_CFG is writable for 13-23 cycles after 0x55, 0xAA to SAFE_MOD (datasheet, SAFE_MOD). */
#define CODE_WE_ON()            \
    do {                        \
        SAFE_MOD = 0x55;        \
        SAFE_MOD = 0xAA;        \
        GLOBAL_CFG |= bCODE_WE; \
        SAFE_MOD = 0;           \
    } while (0)
#define CODE_WE_OFF()            \
    do {                         \
        SAFE_MOD = 0x55;         \
        SAFE_MOD = 0xAA;         \
        GLOBAL_CFG &= ~bCODE_WE; \
        SAFE_MOD = 0;            \
    } while (0)

static uint8_t  newest      = NO_SLOT; // slot of the newest valid copy
static uint16_t newest_seq  = 0;
static bool     dirty       = false; // the xRAM copy changed since the newest slot or since the save began
static uint16_t last_write  = 0;
static uint8_t  save_slot   = NO_SLOT; // slot being written, NO_SLOT when no save runs
static uint8_t  save_page   = 0;       // next page of the save: erase 0..SLOT_PAGES-1, program SLOT_PAGES..
static uint8_t  save_errors = 0;

/*
 * Flash operations on store page n (0..STORE_PAGES-1). Each returns 0 on success. Interrupts are off from
 * write enable to write disable; the CPU stops during the command.
 * Code-flash writes need bCODE_WE, which enables writes to all of code flash, the bootloader included. Only these
 * functions set it, for one erase or program of one store page; the page number is checked against the store
 * size, and the address in ROM_ADDR against the store bounds again after write enable, right before the command.
 */

static uint16_t page_addr(uint8_t n) {
    return STORE_ADDR + (uint16_t)n * ROM_PAGE_SIZE;
}

/* Erases store page n. */
static uint8_t store_erase(uint8_t n) {
    uint8_t  status = 0xFF;
    uint16_t addr;
    __bit    ea = EA;

    if (n >= STORE_PAGES) return status;
    addr = page_addr(n);
    if (!IN_STORE(addr)) return status;

    EA = 0;
    CODE_WE_ON();
    ROM_ADDR    = addr;
    ROM_BUF_MOD = bROM_BUF_BYTE; // 64-byte page erase
    ROM_DAT_BUF = 0;
    if (IN_STORE(ROM_ADDR) && (ROM_STATUS & bROM_ADDR_OK)) {
        ROM_CTRL = ROM_CMD_ERASE;
        status   = ROM_STATUS & bROM_CMD_ERR;
    }
    CODE_WE_OFF();
    EA = ea;
    return status;
}

/* Programs all 64 bytes of store page n from the 64-byte aligned xRAM block at buf. */
static uint8_t store_program(uint8_t n, __xdata const uint8_t *buf) {
    uint8_t  status = 0xFF;
    uint16_t addr;
    __bit    ea = EA;

    if (n >= STORE_PAGES || ((uint8_t)(uint16_t)buf & MASK_ROM_ADDR)) return status;
    addr = page_addr(n);
    if (!IN_STORE(addr)) return status;

    EA = 0;
    CODE_WE_ON();
    ROM_ADDR    = addr;
    ROM_BUF_MOD = MASK_ROM_ADDR; // block program: low 6 bits of the last address
    if (IN_STORE(ROM_ADDR) && (ROM_STATUS & bROM_ADDR_OK)) {
        /* Nothing between these two may use DPTR (checked in the compiler output). */
        CF_DPTR  = (uint16_t)buf;
        ROM_CTRL = ROM_CMD_PROG;
        status   = ROM_STATUS & bROM_CMD_ERR;
    }
    CODE_WE_OFF();
    EA = ea;
    return status;
}

/* Code memory at addr (a host test maps it to a simulated flash) */
#ifndef FLASH_BYTES
#    define FLASH_BYTES(addr) ((__code const uint8_t *)(addr))
#endif

static __code const uint8_t *slot_page(uint8_t slot, uint8_t page) {
    return FLASH_BYTES(page_addr(slot * SLOT_PAGES + page));
}

/* Rotate-and-add sum of a slot's data as stored in flash */
static uint16_t data_sum(uint8_t slot) {
    __code const uint8_t *p   = slot_page(slot, 0);
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
    __code const uint8_t *h = slot_page(slot, DATA_PAGES);
    if (h[H_VALID] != HEADER_VALID || h[H_MAGIC0] != HEADER_MAGIC0 || h[H_MAGIC1] != HEADER_MAGIC1 || h[H_CHECK] != header_check(h)) {
        return false;
    }
    if (h[H_LAYOUT_LO] != (uint8_t)(CH555_EEPROM_LAYOUT) || h[H_LAYOUT_HI] != (uint8_t)((CH555_EEPROM_LAYOUT) >> 8) || h[H_DATA_PAGES] != DATA_PAGES) {
        return false;
    }
    if (data_sum(slot) != (h[H_SUM_LO] | (uint16_t)h[H_SUM_HI] << 8)) {
        return false;
    }
    *seq = h[H_SEQ_LO] | (uint16_t)h[H_SEQ_HI] << 8;
    return true;
}

/* Loads the valid slot with the higher sequence number; without one, the EEPROM reads 0x00 and QMK writes its
 * defaults. */
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
        memcpy(ee_copy, slot_page(newest, 0), EEPROM_SIZE);
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

uint8_t eeprom_code_flash_clear(void) {
    memset(ee_copy, 0, EEPROM_SIZE);
    dirty     = false;
    save_slot = NO_SLOT;
    newest    = NO_SLOT;
    return store_erase(DATA_PAGES) | store_erase(SLOT_PAGES + DATA_PAGES);
}

static void save_stop(bool failed) {
    save_slot = NO_SLOT;
    if (failed) {
        save_errors++;
        mark_dirty(); // again after the delay
    }
}

static bool page_matches(uint8_t slot, uint8_t page, __xdata const uint8_t *buf) {
    return memcmp(slot_page(slot, page), buf, ROM_PAGE_SIZE) == 0;
}

/* Steps of a save into the slot that does not hold the newest copy: erase the header page, then the data pages;
 * program the data pages, each compared with the copy; program the header last, with the sum of the data in flash,
 * so that a power loss during a save leaves the previous slot as the newest valid one. A write to the copy during a
 * save ends it before its header, and a new save starts after the delay. */
void eeprom_code_flash_task(void) {
    if (save_slot == NO_SLOT) {
        if (!dirty || timer_elapsed(last_write) < CH555_EEPROM_SAVE_DELAY || save_errors >= 3) return;
        dirty     = false;
        save_slot = (newest == 0) ? 1 : 0;
        save_page = 0;
        return;
    }
    if (dirty) {
        save_stop(false);
        return;
    }

    if (save_page < SLOT_PAGES) {
        /* Erase: the header page first */
        uint8_t page = (save_page == 0) ? DATA_PAGES : save_page - 1;
        if (store_erase(save_slot * SLOT_PAGES + page) != 0) {
            save_stop(true);
            return;
        }
    } else if (save_page < SLOT_PAGES + DATA_PAGES) {
        uint8_t                page = save_page - SLOT_PAGES;
        __xdata const uint8_t *data = &ee_copy[(uint16_t)page * ROM_PAGE_SIZE];
        if (store_program(save_slot * SLOT_PAGES + page, data) != 0 || !page_matches(save_slot, page, data)) {
            save_stop(true);
            return;
        }
    } else {
        uint16_t seq = newest_seq + 1;
        uint16_t sum = data_sum(save_slot);
        memset(header_buf, 0, ROM_PAGE_SIZE);
        header_buf[H_MAGIC0]     = HEADER_MAGIC0;
        header_buf[H_MAGIC1]     = HEADER_MAGIC1;
        header_buf[H_SEQ_LO]     = seq & 0xFF;
        header_buf[H_SEQ_HI]     = seq >> 8;
        header_buf[H_SUM_LO]     = sum & 0xFF;
        header_buf[H_SUM_HI]     = sum >> 8;
        header_buf[H_LAYOUT_LO]  = (uint8_t)(CH555_EEPROM_LAYOUT);
        header_buf[H_LAYOUT_HI]  = (uint8_t)((CH555_EEPROM_LAYOUT) >> 8);
        header_buf[H_DATA_PAGES] = DATA_PAGES;
        header_buf[H_CHECK]      = header_check(header_buf);
        header_buf[H_VALID]      = HEADER_VALID;
        if (store_program(save_slot * SLOT_PAGES + DATA_PAGES, header_buf) != 0 || !page_matches(save_slot, DATA_PAGES, header_buf)) {
            save_stop(true);
            return;
        }
        newest      = save_slot;
        newest_seq  = seq;
        save_errors = 0;
        save_stop(false);
        return;
    }
    save_page++;
}

bool eeprom_code_flash_busy(void) {
    return save_slot != NO_SLOT || (dirty && save_errors < 3);
}
