/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "hardware_id.h"
#include <string.h>
#include "sh68f90a.h"

/* Datasheet 7.4.4: the 5-byte identification code at 0x127B-0x127F of the information area, which MOVC reads while
 * FLASHCON.FAC is set. While FAC is set every MOVC reads the information area, so interrupts are off. The code
 * goes to the first 5 bytes of id. */
hardware_id_t get_hardware_id(void) {
    hardware_id_t        id;
    uint8_t              code[5];
    __code const uint8_t *info = (__code const uint8_t *)0x127B;

    __critical {
        FLASHCON = bFAC;
        for (uint8_t i = 0; i < sizeof(code); i++) {
            code[i] = info[i];
        }
        FLASHCON = 0;
    }
    memset(&id, 0, sizeof(hardware_id_t));
    memcpy(&id, code, sizeof(code));
    return id;
}
