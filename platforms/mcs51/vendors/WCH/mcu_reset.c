/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "bootloader.h"
#include "ch555.h"

/* Software reset: safe mode, then bSW_RESET. The CPU starts at 0x0000 with bBOOT_LOAD = 0 (datasheet 7.3.3).
 * Does not return. */
void mcu_reset(void) {
    EA         = 0;    /* disable all interrupts first */
    SAFE_MOD   = 0x55; /* enter safe mode step 1 */
    SAFE_MOD   = 0xAA; /* enter safe mode step 2 */
    GLOBAL_CFG |= bSW_RESET; /* trigger software reset - does not return */
    while (1)
        ; /* should never reach here */
}
