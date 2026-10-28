/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "bootloader.h"
#include "sh68f90a.h"

/* Program overflow (OVL) reset: the opcode 0xA5, which the 8051 instruction set does not have, resets the chip and
 * sets RSTSTAT.WDOF (datasheet 8.8). Does not return. */
void mcu_reset(void) {
    EA = 0;
    __asm__(".db 0xa5");
    while (1)
        ;
}
