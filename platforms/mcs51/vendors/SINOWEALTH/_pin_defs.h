/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

// SH68F90A ports. P1, P2 and P3 have six pins (Px.0-Px.5); P0.0 and P0.1 are the USB D- and D+ pins.
#define PORT0 0x0
#define PORT1 0x1
#define PORT2 0x2
#define PORT3 0x3
#define PORT4 0x4
#define PORT5 0x5
#define PORT6 0x6
#define PORT7 0x7

// The ports have no open-drain output mode
#define MCS51_NO_OPEN_DRAIN
