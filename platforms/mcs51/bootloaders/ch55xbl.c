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
 * bootloader_jump()
 * -----------------
 * Disables USB and all interrupts, waits for the host to detect the
 * disconnect, then jumps to the WCH factory bootloader at BOOT_LOAD_ADDR
 * (0xF400 on the CH555).
 */
void bootloader_jump(void) {
    USB_CTRL = 0;
    EA       = 0; // Disabling all interrupts is required.
    wait_us(50000);
    wait_us(50000);

    __asm__("lcall #0xF400"); // Jump to bootloader code

    while (1)
        ;
}
