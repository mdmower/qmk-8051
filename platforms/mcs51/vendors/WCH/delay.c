/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "mcs51_mcu.h"

/* CH555 core cycles: NOP 1; DJNZ Rn 6 when it branches, 2 when it does not. One pass of the loop below, DELAY_NOPS NOPs
 * and a DJNZ r6 that branches, takes F_CPU / 1000000 cycles = 1 us. Every 256th pass takes 2 cycles more (DJNZ r6
 * does not branch, DJNZ r7 does). Interrupts that run during the loop lengthen the delay. */
#define DELAY_NOP2 "nop\n nop\n"
#define DELAY_NOP8 DELAY_NOP2 DELAY_NOP2 DELAY_NOP2 DELAY_NOP2
#if F_CPU == 32000000
#    define DELAY_NOPS DELAY_NOP8 DELAY_NOP8 DELAY_NOP8 DELAY_NOP2 /* 26 */
#elif F_CPU == 24000000
#    define DELAY_NOPS DELAY_NOP8 DELAY_NOP8 DELAY_NOP2 /* 18 */
#elif F_CPU == 16000000
#    define DELAY_NOPS DELAY_NOP8 DELAY_NOP2 /* 10 */
#elif F_CPU == 12000000
#    define DELAY_NOPS DELAY_NOP2 DELAY_NOP2 DELAY_NOP2 /* 6 */
#else
#    error "F_CPU is not 12, 16, 24 or 32 MHz, the clocks of the delay loop"
#endif

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

void mcs51_delay_ms(uint16_t n) {
    while (n) {
        mcs51_delay_us(1000);
        --n;
    }
}
