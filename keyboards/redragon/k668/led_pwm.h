/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sh68f90a.h"

/* The LED matrix and key scan. The 21 key matrix rows (MATRIX_ROWS) are the LED columns: in each phase of the PWM0
 * interrupt one row is driven low and sinks the current of that column's LEDs, the six key inputs are read, and the
 * 21 PWM channels light the column. */

/* The PWM channels are the LED rows: rows 0-5 (the key rows, top row 0) and row 6 (the side lights, in columns
 * 0-16), with one channel per color: rows 0 and 1 PWM20-PWM25 (P1.0-P1.5), rows 2 and 3 PWM10-PWM15 (P2.0-P2.5),
 * row 4 PWM03-PWM05 (P3.3-P3.5), row 5 PWM00-PWM02 (P3.0-P3.2), row 6 PWM30-PWM32 (P7.5-P7.7).
 * Buffer: one byte per color, 21 bytes per column: LED_PWM_BYTE(column, row, color), color 0 red, 1 green, 2 blue. */
#define LED_PWM_COLUMNS 21
#define LED_PWM_ROWS 7
#define LED_PWM_BYTE(c, r, color) ((c) * (LED_PWM_ROWS * 3) + (r) * 3 + (color))
extern __xdata uint8_t led_pwm_buf[LED_PWM_COLUMNS * LED_PWM_ROWS * 3];

/* Key inputs read in each phase: bit n set = the key at matrix row (phase), column n is pressed. */
extern volatile __xdata uint8_t led_pwm_keys[LED_PWM_COLUMNS];

void led_pwm_init(void);
/* Turns the LEDs off and stops the key scan. */
void led_pwm_stop(void);

/* Declared here so that main.c, which includes this (MCS51_ISR_HEADERS in rules.mk), puts them in the interrupt
 * vector table. */
void led_pwm_isr(void) __interrupt(INT_NO_PWM0);
void led_pwm_key_wake_isr(void) __interrupt(INT_NO_INT4);
