// Copyright 2024 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * quantum/nvm/eeprom/nvm_dynamic_keymap.c with byte addresses and 16-bit offsets. SDCC takes the element size of
 * void as 0, so the upstream file's void-pointer arithmetic gives every key the same EEPROM address
 * (sdcc_check.query); its 32-bit counters also take internal RAM for spill locations. Same EEPROM layout; buffers
 * are read and written as blocks.
 */

#include "compiler_support.h"
#include "keycodes.h"
#include "eeprom.h"
#include "dynamic_keymap.h"
#include "nvm_dynamic_keymap.h"
#include "nvm_eeprom_eeconfig_internal.h"
#include "nvm_eeprom_via_internal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ENCODER_ENABLE
#    include "encoder.h"
#endif

#ifdef VIA_ENABLE
#    include "via.h"
#    define DYNAMIC_KEYMAP_EEPROM_START (VIA_EEPROM_CONFIG_END)
#else
#    define DYNAMIC_KEYMAP_EEPROM_START (EECONFIG_SIZE)
#endif

#ifndef DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#    define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR (TOTAL_EEPROM_BYTE_COUNT - 1)
#endif

STATIC_ASSERT(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR <= (TOTAL_EEPROM_BYTE_COUNT - 1), "DYNAMIC_KEYMAP_EEPROM_MAX_ADDR is configured to use more space than what is available for the selected EEPROM driver");

// Offsets are 16-bit
STATIC_ASSERT(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR <= 65535, "DYNAMIC_KEYMAP_EEPROM_MAX_ADDR must be less than 65536");

// If DYNAMIC_KEYMAP_EEPROM_ADDR not explicitly defined in config.h,
#ifndef DYNAMIC_KEYMAP_EEPROM_ADDR
#    define DYNAMIC_KEYMAP_EEPROM_ADDR DYNAMIC_KEYMAP_EEPROM_START
#endif

// Dynamic encoders starts after dynamic keymaps
#ifndef DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR
#    define DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR (DYNAMIC_KEYMAP_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2))
#endif

// Dynamic macro starts after dynamic encoders, but only when using ENCODER_MAP
#ifdef ENCODER_MAP_ENABLE
#    ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#        define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * NUM_ENCODERS * 2 * 2))
#    endif // DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#else      // ENCODER_MAP_ENABLE
#    ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#        define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR)
#    endif // DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#endif     // ENCODER_MAP_ENABLE

// Sanity check that dynamic keymaps fit in available EEPROM
// If there's not 100 bytes available for macros, then something is wrong.
// The keyboard should override DYNAMIC_KEYMAP_LAYER_COUNT to reduce it,
// or DYNAMIC_KEYMAP_EEPROM_MAX_ADDR to increase it, *only if* the microcontroller has
// more than the default.
STATIC_ASSERT((int32_t)(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR) - (int32_t)(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR) >= 100, "Dynamic keymaps are configured to use more EEPROM than is available.");

#ifndef TOTAL_EEPROM_BYTE_COUNT
#    error Unknown total EEPROM size. Cannot derive maximum for dynamic keymaps.
#endif
// Dynamic macros are stored after the keymaps and use what is available
// up to and including DYNAMIC_KEYMAP_EEPROM_MAX_ADDR.
#ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE
#    define DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR - DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + 1)
#endif

#define DYNAMIC_KEYMAP_KEYMAP_SIZE (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2)

#define EEPROM_BYTE(addr) ((uint8_t *)(uint16_t)(addr))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void nvm_dynamic_keymap_erase(void) {
    // No-op, nvm_eeconfig_erase() will have already erased EEPROM if necessary.
}

void nvm_dynamic_keymap_macro_erase(void) {
    // No-op, nvm_eeconfig_erase() will have already erased EEPROM if necessary.
}

// Big endian, so we can read/write EEPROM directly from host if we want
static uint16_t read_keycode_at(uint16_t addr) {
    uint16_t keycode = (uint16_t)eeprom_read_byte(EEPROM_BYTE(addr)) << 8;
    return keycode | eeprom_read_byte(EEPROM_BYTE(addr + 1));
}

static void update_keycode_at(uint16_t addr, uint16_t keycode) {
    eeprom_update_byte(EEPROM_BYTE(addr), (uint8_t)(keycode >> 8));
    eeprom_update_byte(EEPROM_BYTE(addr + 1), (uint8_t)(keycode & 0xFF));
}

static uint16_t key_address(uint8_t layer, uint8_t row, uint8_t column) {
    return DYNAMIC_KEYMAP_EEPROM_ADDR + ((((uint16_t)layer * MATRIX_ROWS) + row) * MATRIX_COLS + column) * 2;
}

uint16_t nvm_dynamic_keymap_read_keycode(uint8_t layer, uint8_t row, uint8_t column) {
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT || row >= MATRIX_ROWS || column >= MATRIX_COLS) return KC_NO;
    return read_keycode_at(key_address(layer, row, column));
}

void nvm_dynamic_keymap_update_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode) {
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT || row >= MATRIX_ROWS || column >= MATRIX_COLS) return;
    update_keycode_at(key_address(layer, row, column), keycode);
}

#ifdef ENCODER_MAP_ENABLE
static uint16_t encoder_address(uint8_t layer, uint8_t encoder_id, bool clockwise) {
    return DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR + (((uint16_t)layer * NUM_ENCODERS) + encoder_id) * 2 * 2 + (clockwise ? 0 : 2);
}

uint16_t nvm_dynamic_keymap_read_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise) {
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT || encoder_id >= NUM_ENCODERS) return KC_NO;
    return read_keycode_at(encoder_address(layer, encoder_id, clockwise));
}

void nvm_dynamic_keymap_update_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode) {
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT || encoder_id >= NUM_ENCODERS) return;
    update_keycode_at(encoder_address(layer, encoder_id, clockwise), keycode);
}
#endif // ENCODER_MAP_ENABLE

// Bytes past the end of a region read as 0
static void read_region(uint16_t base, uint16_t region_size, uint16_t offset, uint16_t size, uint8_t *data) {
    uint16_t len = 0;
    if (offset < region_size) {
        len = region_size - offset;
        if (len > size) {
            len = size;
        }
        eeprom_read_block(data, EEPROM_BYTE(base + offset), len);
    }
    for (; len < size; len++) {
        data[len] = 0x00;
    }
}

// Bytes past the end of a region are not written
static void update_region(uint16_t base, uint16_t region_size, uint16_t offset, uint16_t size, uint8_t *data) {
    if (offset < region_size) {
        uint16_t len = region_size - offset;
        if (len > size) {
            len = size;
        }
        eeprom_update_block(data, EEPROM_BYTE(base + offset), len);
    }
}

// dynamic_keymap.c passes 16-bit offsets and sizes
#define FITS_16_BITS(offset, size) ((offset) <= 0xFFFF && (size) <= 0xFFFF)

void nvm_dynamic_keymap_read_buffer(uint32_t offset, uint32_t size, uint8_t *data) {
    if (FITS_16_BITS(offset, size)) {
        read_region(DYNAMIC_KEYMAP_EEPROM_ADDR, DYNAMIC_KEYMAP_KEYMAP_SIZE, offset, size, data);
    }
}

void nvm_dynamic_keymap_update_buffer(uint32_t offset, uint32_t size, uint8_t *data) {
    if (FITS_16_BITS(offset, size)) {
        update_region(DYNAMIC_KEYMAP_EEPROM_ADDR, DYNAMIC_KEYMAP_KEYMAP_SIZE, offset, size, data);
    }
}

uint32_t nvm_dynamic_keymap_macro_size(void) {
    return DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE;
}

void nvm_dynamic_keymap_macro_read_buffer(uint32_t offset, uint32_t size, uint8_t *data) {
    if (FITS_16_BITS(offset, size)) {
        read_region(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR, DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE, offset, size, data);
    }
}

void nvm_dynamic_keymap_macro_update_buffer(uint32_t offset, uint32_t size, uint8_t *data) {
    if (FITS_16_BITS(offset, size)) {
        update_region(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR, DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE, offset, size, data);
    }
}

void nvm_dynamic_keymap_macro_reset(void) {
    static const uint8_t zeros[16] = {0};
    for (uint16_t done = 0; done < DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE; done += sizeof(zeros)) {
        uint16_t len = DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE - done;
        if (len > sizeof(zeros)) {
            len = sizeof(zeros);
        }
        eeprom_update_block(zeros, EEPROM_BYTE(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + done), len);
    }
}
