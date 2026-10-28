/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

// CH555 ports. P6 (P6.0/P6.1 are the USB D-/D+ pins) is not implemented in gpio.c. P7 has only P7.0 and P7.1; its
// register holds their DIR, OUT_PU and IN bits.
#define PORT0 0x0
#define PORT1 0x1
#define PORT2 0x2
#define PORT3 0x3
#define PORT4 0x4
#define PORT5 0x5
#define PORT7 0x7
