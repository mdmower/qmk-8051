/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/*
 * The CH555 part of the MCS-51 platform. The platform includes this header, not ch555.h or hal.h. Each MCU has a
 * header of this name in its directory, vendors/$(MCU_FAMILY), that provides:
 *   - the standard 8051 SFRs and bits (TMOD, TH0, TL0, TR0, ET0, EA) and INT_NO_TMR0;
 *   - mcs51_clock_init(): sets Fsys to F_CPU (platform_setup());
 *   - mcs51_print_init(): initializes the print output that putchar() writes to (platform_setup(), unless NO_PRINT);
 *   - MCS51_TIMER0_INIT(): selects the Timer0 clock; MCS51_TIMER0_HZ: the Timer0 count rate;
 *   - mcs51_delay_us(n), mcs51_delay_ms(n): waits of at least n us or n ms, n up to 65535.
 * The directory's $(MCU_SERIES).mk compiles the MCU's gpio.c (the functions of gpio.h), get_hardware_id(),
 * mcu_reset() and putchar(); its config.h holds the MCU's defaults, such as SERIAL_NUMBER_LENGTH.
 */

#include "hal.h"

/* Fsys = F_CPU: 12, 16, 24 or 32 MHz */
#define mcs51_clock_init() CfgFsys()

/* The UART that putchar() writes to */
#define mcs51_print_init() mInitSTDIO()

/* Timer0 at Fsys/12 (bT0_CLK = 0): 2000 counts per ms at 24 MHz, 2666.67 at 32 MHz */
#define MCS51_TIMER0_INIT() (T2MOD &= ~bT0_CLK)
#define MCS51_TIMER0_HZ (F_CPU / 12)

#define mcs51_delay_us(n) mDelayuS(n)
#define mcs51_delay_ms(n) mDelaymS(n)
