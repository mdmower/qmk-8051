/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpio.h"
#include <stdbool.h>
#include <stdint.h>
#include "sh68f90a.h"

__code const uint8_t pin_to_pinmask[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

/* Datasheet 7.6: PxCR is the direction (0 input, 1 output), PxPCR the pull-up of an input (1 on). */

/* The pull-up is switched on before the pin becomes an input, so that the input does not float meanwhile. */
void pinModeInputHighQMK(uint8_t pin) {
    uint8_t pinmask = PIN_MASK(pin);
    switch (PORTx(pin)) {
        case PORT0:
            P0PCR |= pinmask;
            P0CR &= ~pinmask;
            break;
        case PORT1:
            P1PCR |= pinmask;
            P1CR &= ~pinmask;
            break;
        case PORT2:
            P2PCR |= pinmask;
            P2CR &= ~pinmask;
            break;
        case PORT3:
            P3PCR |= pinmask;
            P3CR &= ~pinmask;
            break;
        case PORT4:
            P4PCR |= pinmask;
            P4CR &= ~pinmask;
            break;
        case PORT5:
            P5PCR |= pinmask;
            P5CR &= ~pinmask;
            break;
        case PORT6:
            P6PCR |= pinmask;
            P6CR &= ~pinmask;
            break;
        case PORT7:
            P7PCR |= pinmask;
            P7CR &= ~pinmask;
            break;
    }
}

void pinModeInputZ(uint8_t pin) {
    uint8_t pinmask = PIN_MASK(pin);
    switch (PORTx(pin)) {
        case PORT0:
            P0CR &= ~pinmask;
            P0PCR &= ~pinmask;
            break;
        case PORT1:
            P1CR &= ~pinmask;
            P1PCR &= ~pinmask;
            break;
        case PORT2:
            P2CR &= ~pinmask;
            P2PCR &= ~pinmask;
            break;
        case PORT3:
            P3CR &= ~pinmask;
            P3PCR &= ~pinmask;
            break;
        case PORT4:
            P4CR &= ~pinmask;
            P4PCR &= ~pinmask;
            break;
        case PORT5:
            P5CR &= ~pinmask;
            P5PCR &= ~pinmask;
            break;
        case PORT6:
            P6CR &= ~pinmask;
            P6PCR &= ~pinmask;
            break;
        case PORT7:
            P7CR &= ~pinmask;
            P7PCR &= ~pinmask;
            break;
    }
}

void pinModeOutputPushPull(uint8_t pin) {
    uint8_t pinmask = PIN_MASK(pin);
    switch (PORTx(pin)) {
        case PORT0:
            P0CR |= pinmask;
            break;
        case PORT1:
            P1CR |= pinmask;
            break;
        case PORT2:
            P2CR |= pinmask;
            break;
        case PORT3:
            P3CR |= pinmask;
            break;
        case PORT4:
            P4CR |= pinmask;
            break;
        case PORT5:
            P5CR |= pinmask;
            break;
        case PORT6:
            P6CR |= pinmask;
            break;
        case PORT7:
            P7CR |= pinmask;
            break;
    }
}

void writePinHigh(uint8_t pin) {
    uint8_t pinmask = PIN_MASK(pin);
    switch (PORTx(pin)) {
        case PORT0:
            P0 |= pinmask;
            break;
        case PORT1:
            P1 |= pinmask;
            break;
        case PORT2:
            P2 |= pinmask;
            break;
        case PORT3:
            P3 |= pinmask;
            break;
        case PORT4:
            P4 |= pinmask;
            break;
        case PORT5:
            P5 |= pinmask;
            break;
        case PORT6:
            P6 |= pinmask;
            break;
        case PORT7:
            P7 |= pinmask;
            break;
    }
}

void writePinLow(uint8_t pin) {
    uint8_t pinmask = PIN_MASK(pin);
    switch (PORTx(pin)) {
        case PORT0:
            P0 &= ~pinmask;
            break;
        case PORT1:
            P1 &= ~pinmask;
            break;
        case PORT2:
            P2 &= ~pinmask;
            break;
        case PORT3:
            P3 &= ~pinmask;
            break;
        case PORT4:
            P4 &= ~pinmask;
            break;
        case PORT5:
            P5 &= ~pinmask;
            break;
        case PORT6:
            P6 &= ~pinmask;
            break;
        case PORT7:
            P7 &= ~pinmask;
            break;
    }
}

uint8_t readPin(uint8_t pin) {
    uint8_t pinmask = PIN_MASK(pin);
    switch (PORTx(pin)) {
        case PORT0:
            return (bool)(P0 & pinmask);
        case PORT1:
            return (bool)(P1 & pinmask);
        case PORT2:
            return (bool)(P2 & pinmask);
        case PORT3:
            return (bool)(P3 & pinmask);
        case PORT4:
            return (bool)(P4 & pinmask);
        case PORT5:
            return (bool)(P5 & pinmask);
        case PORT6:
            return (bool)(P6 & pinmask);
        case PORT7:
            return (bool)(P7 & pinmask);
        default:
            return 0;
    }
}
