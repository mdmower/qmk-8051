/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/* The SH68F90A part of the MCS-51 platform: registers, clock, the 1 ms timer and delays. */

#include <stdint.h>
#include "sh68f90a.h"

#ifndef NO_PRINT
#    error "NO_PRINT is not defined, and there is no print output for the SH68F90A"
#endif

/* SYSCLK = F_CPU = 24 MHz, the PLL's 48 MHz divided by 2 */
void mcs51_clock_init(void);

/* The 1 ms tick: Timer 2 in 16-bit auto-reload mode at SYSCLK / 12 (T2MOD.TCLKP2 = 0). TF2 is cleared by software
 * only: the handler clears it. */
#define MCS51_TIMER_COUNTS (F_CPU / 12 / 1000)
#define MCS51_TIMER_RELOAD() (TF2 = 0)
#define MCS51_TIMER_INIT()                                       \
    do {                                                         \
        T2MOD  = 0;                                              \
        T2CON  = 0;                                              \
        RCAP2L = (0x10000 - MCS51_TIMER_COUNTS) & 0xff;          \
        RCAP2H = ((0x10000 - MCS51_TIMER_COUNTS) >> 8) & 0xff;   \
        TL2    = RCAP2L;                                         \
        TH2    = RCAP2H;                                         \
        TR2    = 1;                                              \
        ET2    = 1;                                              \
    } while (0)
#define INT_NO_TIMER INT_NO_TIMER2

void mcs51_delay_us(uint16_t n);
void mcs51_delay_ms(uint16_t n);
