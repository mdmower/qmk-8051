/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "mcs51_mcu.h"

#if F_CPU != 24000000
#    error "F_CPU is not 24000000, the only SYSCLK with USB"
#endif

/* Datasheet 7.5.4, note (1): the 24 MHz RC oscillator on, the PLL on and locked, OSCSCLK = PLL / 2 = 24 MHz, then
 * SYSCLK = OSCSCLK (CLKS = 00: not divided). */
void mcs51_clock_init(void) {
    CLKCON = bHFON;
    PLLCON = bPLLON;
    while (!(PLLCON & bPLLSTA))
        ;
    PLLCON = bPLLON | bPLLFS;
    CLKCON = bHFON | bFS;
}
