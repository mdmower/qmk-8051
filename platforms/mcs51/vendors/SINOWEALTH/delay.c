/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "mcs51_mcu.h"

/* Core cycles (datasheet, instruction set): NOP 1; DJNZ Rn 5 when it branches, 3 when it does not. One pass of the
 * loop below, 19 NOPs and a DJNZ r6 that branches, takes 24 cycles = 1 us at 24 MHz. Every 256th pass takes 3 cycles
 * more (DJNZ r6 does not branch, DJNZ r7 does). Interrupts that run during the loop lengthen the delay. */
#if F_CPU != 24000000
#    error "F_CPU is not 24000000, the only clock of the delay loop"
#endif
#define DELAY_NOP2 "nop\n nop\n"
#define DELAY_NOP8 DELAY_NOP2 DELAY_NOP2 DELAY_NOP2 DELAY_NOP2
#define DELAY_NOPS DELAY_NOP8 DELAY_NOP8 DELAY_NOP2 "nop\n" /* 19 */

void mcs51_delay_us(uint16_t n) __naked {
    (void)n; /* in DPL (low byte) and DPH */
    /* SDCC saves no registers around a call of a naked function defined in the same file (mcs51_delay_ms) */
    __asm__("push ar6");
    __asm__("push ar7");
    __asm__("mov  a,dpl");
    __asm__("mov  r6,a");
    __asm__("orl  a,dph");
    __asm__("jz   00002$"); /* n = 0 */
    __asm__("mov  r7,dph");
    __asm__("mov  a,r6");
    __asm__("jz   00001$");
    __asm__("inc  r7"); /* low byte not 0: the first r6 round has that many passes, the others 256 */
    __asm__("00001$:");
    __asm__(DELAY_NOPS);
    __asm__("djnz r6,00001$");
    __asm__("djnz r7,00001$");
    __asm__("00002$:");
    __asm__("pop  ar7");
    __asm__("pop  ar6");
    __asm__("ret");
}

/* A read of RSTSTAT restarts the watchdog count (datasheet 8.8), so a wait longer than the watchdog period does not
 * reset the chip. */
void mcs51_delay_ms(uint16_t n) {
    while (n) {
        (void)RSTSTAT;
        mcs51_delay_us(1000);
        --n;
    }
}
