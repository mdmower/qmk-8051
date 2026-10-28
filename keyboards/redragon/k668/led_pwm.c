/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "led_pwm.h"
#include "keyboards/redragon/common/led_fx.h"
#include "mcs51_isr.h"
#include "usb_device.h"

/* Matrix rows (LED columns) per port: P6.0-P6.7, P5.0-P5.2, P5.7, P4.0, P4.2, P4.3, P4.5, P4.6, P7.4, P0.5-P0.7 */
#define ROWS_P6 0xFF
#define ROWS_P5 0x87
#define ROWS_P4 0x6D
#define ROWS_P7 0x10
#define ROWS_P0 0xE0
/* Key inputs with pull-up: matrix columns 0-3 P7.0-P7.3, columns 4 and 5 P5.3 and P5.4 */
#define KEYS_P7 0x0F
#define KEYS_P5 0x18
/* LED channel pins: P1.0-P1.5, P2.0-P2.5, P3.0-P3.5, P7.5-P7.7 */
#define CHANNELS_P123 0x3F
#define CHANNELS_P7 0xE0

/* Bit of each matrix row in its port, in row order */
static __code const uint8_t row_mask[LED_PWM_COLUMNS] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, // P6.0-P6.7
    0x01, 0x02, 0x04, 0x80,                         // P5.0-P5.2, P5.7
    0x01, 0x04, 0x08, 0x20, 0x40,                   // P4.0, P4.2, P4.3, P4.5, P4.6
    0x10,                                           // P7.4
    0x20, 0x40, 0x80                                // P0.5-P0.7
};

__xdata uint8_t          led_pwm_buf[LED_PWM_COLUMNS * LED_PWM_ROWS * 3];
volatile __xdata uint8_t led_pwm_keys[LED_PWM_COLUMNS];
/* LED data of a column while the LEDs are off: all zero */
static __xdata uint8_t         led_pwm_off[LED_PWM_ROWS * 3];
static volatile __data uint8_t led_phase;
/* led_phase * (LED_PWM_ROWS * 3), the offset of the phase's column in led_pwm_buf */
static volatile __data uint16_t led_offset;

/*
 * PWM clock SYSCLK/4 (6 MHz), period 1080 counts (180 us), as in the stock firmware; a phase is that period plus
 * the time the interrupt handler keeps the banks stopped. The pulse of the channel of buffer byte i of a column
 * starts at count PULSE_START + i (DUTY1), so that no two channels switch on at the same count, and ends 4 * byte i
 * counts later (DUTY2): 0 to 1020 counts. With DUTY2 = DUTY1 the output stays low.
 */
#define PWM_PERIOD 1080
#define PULSE_START 0x26
#define BANK_CK 0x02
#define BANK_ON (bPWM_EN | bPWM_SS | BANK_CK)
#define CHANNEL_ON bPWM_SS

/* Input bits of the current phase: bit n set = the key in matrix column n is pressed */
#define READ_KEYS() ((uint8_t)~((P7 & KEYS_P7) | ((P5 & KEYS_P5) << 1)) & 0x3F)

/* DUTY2 of channel PWM<ch> from byte i of the column at p; the high byte is written first (datasheet 8.2.1) */
#define SET_DUTY(ch, i)                                              \
    do {                                                             \
        uint16_t d      = ((uint16_t)p[i] << 2) + PULSE_START + (i); \
        PWM##ch##DUTY2H = d >> 8;                                    \
        PWM##ch##DUTY2L = d & 0xFF;                                  \
    } while (0)

/*
 * Runs at the end of each PWM0 period. With PWMxEN = 0 the four banks stop and their outputs are low (datasheet
 * 8.2.1, note 5); the LEDs stay off while the row of the next phase is selected, the channels get that column's
 * duties and its keys are read; then PWMxEN starts the four banks again, as in the stock firmware. The keys are read
 * after the duty writes, so that an input that a key of the previous row pulled low has returned high through its
 * pull-up. The LEDs stay off while the device is not configured or the bus is suspended: USB 2.0 allows 100 mA before
 * SET_CONFIGURATION and 2.5 mA in suspend.
 * While FLASHCON.FAC is set, MOVC reads the information area instead of the program; the handler reads row_mask
 * with MOVC, so it clears FLASHCON and restores it at the end.
 */
#pragma save
#pragma nooverlay
MCS51_ISR(led_pwm_isr, INT_NO_PWM0) {
    uint8_t          flashcon = FLASHCON;
    uint8_t          phase    = led_phase;
    uint8_t          mask;
    __xdata uint8_t *p;

    FLASHCON = 0;

    P6 = ROWS_P6;
    P5 |= ROWS_P5;
    P4 |= ROWS_P4;
    P7 |= ROWS_P7;
    P0 |= ROWS_P0;
    /* Also clears PWM0IF */
    PWM00CON = BANK_CK;
    PWM10CON = BANK_CK;
    PWM20CON = BANK_CK;
    PWM30CON = BANK_CK;
    mask     = row_mask[phase];
    if (phase < 8) {
        P6 &= ~mask;
    } else if (phase < 12) {
        P5 &= ~mask;
    } else if (phase < 17) {
        P4 &= ~mask;
    } else if (phase < 18) {
        P7 &= ~mask;
    } else {
        P0 &= ~mask;
    }

    p = (usb_configured && !usb_suspended) ? led_pwm_buf + led_offset : led_pwm_off;
    SET_DUTY(20, 0);
    SET_DUTY(21, 1);
    SET_DUTY(22, 2);
    SET_DUTY(23, 3);
    SET_DUTY(24, 4);
    SET_DUTY(25, 5);
    SET_DUTY(10, 6);
    SET_DUTY(11, 7);
    SET_DUTY(12, 8);
    SET_DUTY(13, 9);
    SET_DUTY(14, 10);
    SET_DUTY(15, 11);
    SET_DUTY(03, 12);
    SET_DUTY(04, 13);
    SET_DUTY(05, 14);
    SET_DUTY(00, 15);
    SET_DUTY(01, 16);
    SET_DUTY(02, 17);
    SET_DUTY(30, 18);
    SET_DUTY(31, 19);
    SET_DUTY(32, 20);

    led_pwm_keys[phase] = READ_KEYS();

    PWM00CON = BANK_ON | bPWM_IE;
    PWM10CON = BANK_ON;
    PWM20CON = BANK_ON;
    PWM30CON = BANK_ON;

    if (phase == LED_PWM_COLUMNS - 1) {
        led_phase  = 0;
        led_offset = 0;
    } else {
        led_phase = phase + 1;
        led_offset += LED_PWM_ROWS * 3;
    }

    FLASHCON = flashcon;
}
#pragma restore

/* DUTY1 and DUTY2 of channel PWM<ch>, buffer byte i: both at the pulse start, so that the output stays low */
#define INIT_DUTY(ch, i)                     \
    do {                                     \
        PWM##ch##DUTY1H = 0;                 \
        PWM##ch##DUTY1L = PULSE_START + (i); \
        PWM##ch##DUTY2H = 0;                 \
        PWM##ch##DUTY2L = PULSE_START + (i); \
    } while (0)

/*
 * Matrix rows: outputs, high (the low row is the selected one). Key inputs: pull-ups. Channel pins: outputs, low.
 * The PWM0 interrupt is at priority 2 of 0-3, as in the stock firmware.
 */
void led_pwm_init(void) {
    led_phase  = 0;
    led_offset = 0;

    P6 = ROWS_P6;
    P5 |= ROWS_P5;
    P4 |= ROWS_P4;
    P7 |= ROWS_P7;
    P0 |= ROWS_P0;
    P6CR = ROWS_P6;
    P5CR |= ROWS_P5;
    P4CR |= ROWS_P4;
    P7CR |= ROWS_P7;
    P0CR |= ROWS_P0;

    P7PCR |= KEYS_P7;
    P5PCR |= KEYS_P5;
    P7CR &= ~KEYS_P7;
    P5CR &= ~KEYS_P5;

    P1 &= ~CHANNELS_P123;
    P2 &= ~CHANNELS_P123;
    P3 &= ~CHANNELS_P123;
    P7 &= ~CHANNELS_P7;
    P1CR |= CHANNELS_P123;
    P2CR |= CHANNELS_P123;
    P3CR |= CHANNELS_P123;
    P7CR |= CHANNELS_P7;

    PWM0PERDH = PWM_PERIOD >> 8;
    PWM0PERDL = PWM_PERIOD & 0xFF;
    PWM1PERDH = PWM_PERIOD >> 8;
    PWM1PERDL = PWM_PERIOD & 0xFF;
    PWM2PERDH = PWM_PERIOD >> 8;
    PWM2PERDL = PWM_PERIOD & 0xFF;
    PWM3PERDH = PWM_PERIOD >> 8;
    PWM3PERDL = PWM_PERIOD & 0xFF;
    INIT_DUTY(20, 0);
    INIT_DUTY(21, 1);
    INIT_DUTY(22, 2);
    INIT_DUTY(23, 3);
    INIT_DUTY(24, 4);
    INIT_DUTY(25, 5);
    INIT_DUTY(10, 6);
    INIT_DUTY(11, 7);
    INIT_DUTY(12, 8);
    INIT_DUTY(13, 9);
    INIT_DUTY(14, 10);
    INIT_DUTY(15, 11);
    INIT_DUTY(03, 12);
    INIT_DUTY(04, 13);
    INIT_DUTY(05, 14);
    INIT_DUTY(00, 15);
    INIT_DUTY(01, 16);
    INIT_DUTY(02, 17);
    INIT_DUTY(30, 18);
    INIT_DUTY(31, 19);
    INIT_DUTY(32, 20);
    PWM01CON = CHANNEL_ON;
    PWM02CON = CHANNEL_ON;
    PWM03CON = CHANNEL_ON;
    PWM04CON = CHANNEL_ON;
    PWM05CON = CHANNEL_ON;
    PWM11CON = CHANNEL_ON;
    PWM12CON = CHANNEL_ON;
    PWM13CON = CHANNEL_ON;
    PWM14CON = CHANNEL_ON;
    PWM15CON = CHANNEL_ON;
    PWM21CON = CHANNEL_ON;
    PWM22CON = CHANNEL_ON;
    PWM23CON = CHANNEL_ON;
    PWM24CON = CHANNEL_ON;
    PWM25CON = CHANNEL_ON;
    PWM31CON = CHANNEL_ON;
    PWM32CON = CHANNEL_ON;

    IPH1 |= bPPWM0H;
    IPL1 &= ~bPPWM0L;
    IEN1 |= bEPWM0;
    PWM10CON = BANK_ON;
    PWM20CON = BANK_ON;
    PWM30CON = BANK_ON;
    PWM00CON = BANK_ON | bPWM_IE;
}

/* Stops the PWM0 interrupt and the four banks and turns off the channel outputs: the channel pins are then GPIO, low,
 * and all matrix rows are high. The PWMxyCON registers are at 0xFF80-0xFF95. */
void led_pwm_stop(void) {
    volatile __xdata uint8_t *con = &PWM00CON;
    uint8_t                   i;

    IEN1 &= ~bEPWM0;
    for (i = 0; i < 0x16; i++) {
        con[i] = 0;
    }
    P6 = ROWS_P6;
    P5 |= ROWS_P5;
    P4 |= ROWS_P4;
    P7 |= ROWS_P7;
    P0 |= ROWS_P0;
}

/* Slot c * 7 + r is the LED in row r of column c. */
void led_fx_set_color(uint8_t slot, uint8_t r, uint8_t g, uint8_t b) {
    __xdata uint8_t *p = led_pwm_buf + (uint16_t)slot * 3;

    p[0] = r;
    p[1] = g;
    p[2] = b;
}
