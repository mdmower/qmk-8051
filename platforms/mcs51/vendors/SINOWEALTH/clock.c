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

/* Before power-down in USB suspend (datasheet 8.4.3): SYSCLK = OSC1CLK, the 128 kHz RC oscillator, then the PLL and
 * the 24 MHz RC oscillator off. The PLL must be off before power-down (datasheet 8.9.3). */
void mcs51_clock_stop(void) {
    CLKCON &= ~bFS;
    PLLCON &= ~bPLLFS;
    PLLCON &= ~bPLLON;
    CLKCON &= ~bHFON;
}

/* After power-down: the sequence of mcs51_clock_init() from SYSCLK = OSC1CLK, with a wait of about 5 ms after PLLSTA
 * as a margin before SYSCLK changes to the PLL clock. At 128 kHz, mcs51_delay_us(27) takes 27 passes of 24 cycles,
 * 5.1 ms. */
void mcs51_clock_restart(void) {
    CLKCON |= bHFON;
    PLLCON |= bPLLON;
    while (!(PLLCON & bPLLSTA))
        ;
    mcs51_delay_us(27);
    PLLCON |= bPLLFS;
    CLKCON |= bFS;
}
