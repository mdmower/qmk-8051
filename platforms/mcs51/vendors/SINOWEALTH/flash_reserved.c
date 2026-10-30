/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>

/* Code flash that holds no code: an instruction that accesses external RAM fails when it is at 0x10FE-0x1107 or at
 * the same offset of another 8 KB block (Sinowealth; not in the datasheet). As in Sinowealth's demo code, the 24 bytes
 * from 0x10F0 of each block hold 0xA5, the OVL reset opcode, and the linker places the code areas around them
 * (SH68F90.mk). The range of the block at 0xE000, 0xF0F0-0xF107, is above the linked code. */
#define A5_8 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5
#define RESERVED(addr) __code __at(addr) const uint8_t sh68f90_reserved_##addr[24] = {A5_8, A5_8, A5_8}

RESERVED(0x10F0);
RESERVED(0x30F0);
RESERVED(0x50F0);
RESERVED(0x70F0);
RESERVED(0x90F0);
RESERVED(0xB0F0);
RESERVED(0xD0F0);
