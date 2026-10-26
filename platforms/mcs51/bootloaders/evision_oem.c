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
 * The Evision OEM bootloader (0c45:7687, app at 0x2800) starts the app only while the marker AA 55 33 CC at
 * 0x27C0 is present. This erases that 64-byte page and resets, as the Redragon K580 stock firmware does for its
 * "04 EE EE" command; the bootloader writes the marker again after a successful flash. Nothing else below 0x2800
 * is written. mcu_reset() starts the CPU at 0x0000, the bootloader, which has no timeout: the board stays in it,
 * across replugs, until a flash completes. If the erase fails, the reset starts the app again.
 */
void bootloader_jump(void) {
    USB_CTRL = 0;
    EA       = 0; // Disabling all interrupts is required.
    wait_us(50000);
    wait_us(50000);

    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    GLOBAL_CFG |= bCODE_WE;
    ROM_ADDR_H  = 0x27;
    ROM_ADDR_L  = 0xC0;
    ROM_DATA_HH = 0x80;
    ROM_DATA_HL = 0x00;
    if (ROM_CTRL & bROM_ADDR_OK) {
        ROM_CTRL = ROM_CMD_ERASE;
    }
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    GLOBAL_CFG &= ~bCODE_WE;

    mcu_reset();
}
