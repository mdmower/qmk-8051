/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/*
 * The CH555 part of the MCS-51 platform. The platform includes this header, not ch555.h or hal.h. Each MCU has a
 * header of this name in its directory, vendors/$(MCU_FAMILY), that provides:
 *   - the SFR bit EA;
 *   - mcs51_clock_init(): sets Fsys to F_CPU (platform_setup());
 *   - mcs51_print_init(): initializes the print output that putchar() writes to (platform_setup(), unless NO_PRINT);
 *   - MCS51_TIMER_INIT(): starts a timer whose interrupt INT_NO_TIMER comes once per ms and enables that interrupt;
 *     MCS51_TIMER_RELOAD(): what its handler does before it counts the tick;
 *   - mcs51_delay_us(n), mcs51_delay_ms(n): waits of at least n us or n ms, n up to 65535.
 * The directory's $(MCU_SERIES).mk compiles the MCU's gpio.c (the functions of gpio.h), get_hardware_id(),
 * mcu_reset() and putchar(); its config.h holds the MCU's defaults, such as SERIAL_NUMBER_LENGTH.
 */

#include "hal.h"

/* Fsys = F_CPU: 12, 16, 24 or 32 MHz */
#define mcs51_clock_init() CfgFsys()

/* The UART that putchar() writes to */
#define mcs51_print_init() mInitSTDIO()

/* The 1 ms tick: Timer0 at Fsys/12 (bT0_CLK = 0) in mode 1, 16 bits without reload. The handler reloads it, so the
 * counts from the overflow to the reload are lost and a tick is slightly longer than 1 ms. Counts per tick
 * truncated: 2666 for 2666.67 at 32 MHz. */
#define MCS51_TIMER_COUNTS (F_CPU / 12 / 1000)
#define MCS51_TIMER_RELOAD()                                \
    do {                                                    \
        TL0 = (0x10000 - MCS51_TIMER_COUNTS) & 0xff;        \
        TH0 = ((0x10000 - MCS51_TIMER_COUNTS) >> 8) & 0xff; \
    } while (0)
#define MCS51_TIMER_INIT()                                  \
    do {                                                    \
        T2MOD &= ~bT0_CLK;                                  \
        TMOD = TMOD & 0xf0 | 0x01;                          \
        MCS51_TIMER_RELOAD();                               \
        TR0 = 1;                                            \
        ET0 = 1;                                            \
    } while (0)
#define INT_NO_TIMER INT_NO_TMR0

#define mcs51_delay_us(n) mDelayuS(n)
#define mcs51_delay_ms(n) mDelaymS(n)
