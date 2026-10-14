/* Copyright 2021 QMK
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "pin_defs.h"

typedef uint8_t pin_t;

/* Operation of GPIO by pin. */

void pinModeInputZ(uint8_t pin) ;
void pinModeOutputPushPull(uint8_t pin) ;
void pinModeOutputOpen(uint8_t pin) ;
void pinModeBiDi(uint8_t pin) ;

void pinModeInputHighQMK(uint8_t pin) ;

void writePinLow(uint8_t pin) ;
void writePinHigh(uint8_t pin) ;
uint8_t readPin(uint8_t pin) ;

#define gpio_set_pin_input(pin) pinModeInputZ(pin)
#define gpio_set_pin_input_high(pin) pinModeInputHighQMK(pin)
#define gpio_set_pin_input_low(pin) _Static_assert(0, "MCS51 processors cannot implement an input as pull low")
#define gpio_set_pin_output_push_pull(pin) pinModeOutputPushPull(pin)
#define gpio_set_pin_output_open_drain(pin) pinModeOutputOpen(pin)
#define gpio_set_pin_output(pin) gpio_set_pin_output_push_pull(pin)

#define gpio_write_pin_high(pin) writePinHigh(pin)
#define gpio_write_pin_low(pin) writePinLow(pin)
#define gpio_write_pin(pin, level) ((level) ? gpio_write_pin_high(pin) : gpio_write_pin_low(pin))

#define gpio_read_pin(pin) readPin(pin)

#define gpio_toggle_pin(pin) gpio_write_pin(pin, !gpio_read_pin(pin))
