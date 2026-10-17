// Copyright 2024 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * quantum/nvm/eeprom/nvm_via.c with byte addresses: SDCC takes the element size of void as 0, so the upstream
 * file's void-pointer arithmetic reads and writes all three magic bytes at one address (sdcc_check.query).
 * Same EEPROM layout.
 */

#include "eeprom.h"
#include "util.h"
#include "via.h"
#include "nvm_via.h"
#include "nvm_eeprom_eeconfig_internal.h"
#include "nvm_eeprom_via_internal.h"

#define EEPROM_BYTE(addr) ((uint8_t *)(uint16_t)(addr))

void nvm_via_erase(void) {
    // No-op, nvm_eeconfig_erase() will have already erased EEPROM if necessary.
}

void nvm_via_read_magic(uint8_t *magic0, uint8_t *magic1, uint8_t *magic2) {
    if (magic0) {
        *magic0 = eeprom_read_byte(EEPROM_BYTE(VIA_EEPROM_MAGIC_ADDR + 0));
    }

    if (magic1) {
        *magic1 = eeprom_read_byte(EEPROM_BYTE(VIA_EEPROM_MAGIC_ADDR + 1));
    }

    if (magic2) {
        *magic2 = eeprom_read_byte(EEPROM_BYTE(VIA_EEPROM_MAGIC_ADDR + 2));
    }
}

void nvm_via_update_magic(uint8_t magic0, uint8_t magic1, uint8_t magic2) {
    eeprom_update_byte(EEPROM_BYTE(VIA_EEPROM_MAGIC_ADDR + 0), magic0);
    eeprom_update_byte(EEPROM_BYTE(VIA_EEPROM_MAGIC_ADDR + 1), magic1);
    eeprom_update_byte(EEPROM_BYTE(VIA_EEPROM_MAGIC_ADDR + 2), magic2);
}

// Layout options: most significant byte first
uint32_t nvm_via_read_layout_options(void) {
    uint8_t  bytes[VIA_EEPROM_LAYOUT_OPTIONS_SIZE];
    uint32_t value = 0;
    eeprom_read_block(bytes, EEPROM_BYTE(VIA_EEPROM_LAYOUT_OPTIONS_ADDR), sizeof(bytes));
    for (uint8_t i = 0; i < sizeof(bytes); i++) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

void nvm_via_update_layout_options(uint32_t val) {
    uint8_t bytes[VIA_EEPROM_LAYOUT_OPTIONS_SIZE];
    for (uint8_t i = sizeof(bytes); i > 0; i--) {
        bytes[i - 1] = val & 0xFF;
        val          = val >> 8;
    }
    eeprom_update_block(bytes, EEPROM_BYTE(VIA_EEPROM_LAYOUT_OPTIONS_ADDR), sizeof(bytes));
}

uint32_t nvm_via_read_custom_config(void *buf, uint32_t offset, uint32_t length) {
#if VIA_EEPROM_CUSTOM_CONFIG_SIZE > 0
    if (offset >= VIA_EEPROM_CUSTOM_CONFIG_SIZE) {
        return 0;
    }
    uint16_t len = MIN(VIA_EEPROM_CUSTOM_CONFIG_SIZE - offset, length);
    eeprom_read_block(buf, EEPROM_BYTE(VIA_EEPROM_CUSTOM_CONFIG_ADDR + offset), len);
    return len;
#else
    return 0;
#endif
}

uint32_t nvm_via_update_custom_config(const void *buf, uint32_t offset, uint32_t length) {
#if VIA_EEPROM_CUSTOM_CONFIG_SIZE > 0
    if (offset >= VIA_EEPROM_CUSTOM_CONFIG_SIZE) {
        return 0;
    }
    uint16_t len = MIN(VIA_EEPROM_CUSTOM_CONFIG_SIZE - offset, length);
    eeprom_update_block(buf, EEPROM_BYTE(VIA_EEPROM_CUSTOM_CONFIG_ADDR + offset), len);
    return len;
#else
    return 0;
#endif
}
