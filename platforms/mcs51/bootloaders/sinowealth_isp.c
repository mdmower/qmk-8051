/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "bootloader.h"
#include "sh68f90a.h"

/* Starts the Sinowealth ISP bootloader with the sequence of the vendor firmware: interrupts off, B = 0xA5,
 * A = 0x5A, and a jump to 0xFF00 in the bootloader. The bootloader enumerates as 0603:1020. Its entry at 0xFF00 sets
 * up neither the clock nor the USB module's 3.3 V regulator (REGCON), so the regulator is turned on here for a jump
 * before USB has started. The bootloader enables USB at least 16,000 cycles (0.67 ms) after the entry, later than the
 * 500 us the regulator needs. */
void bootloader_jump(void) {
    REGCON = bREGEN;
    __asm__("clr _EA\n mov b,#0xa5\n mov a,#0x5a\n ljmp 0xff00");
}
