/* Copyright 2017 Fred Sundvik
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include "bootloader.h"
#include "wait.h"
#include "ch555.h"

/*
 * The WCH factory bootloader at 0xF400 (4348:55e0). Called from the app, its start-up code stays in USB mode,
 * without a timeout, if P1.5 reads low when it starts (or code 0x0000-0x0001 reads 00 00); otherwise it resets into
 * the app.
 * P1.5 is also an output of the RGB LED controller, which is stopped first. The bootloader leaves only through a
 * reset, so the call does not return.
 */
void bootloader_jump(void) {
    EA       = 0; // Disabling all interrupts is required.
    LED_CTRL = 0;
    USB_CTRL = 0;
    wait_us(50000);
    wait_us(50000);

    P1_MOD_OC &= ~(uint8_t)(1 << 5);
    P1_DIR_PU |= 1 << 5;
    P1_5 = 0;
    __asm__("lcall #0xF400");

    while (1)
        ;
}
