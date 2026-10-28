/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* LED positions of the K552 for the lighting effects (led_fx.c includes this file). */

/*
 * Position of each LED, 0-191, by slot (COM * 8 + group): key center x on the
 * 18.25-unit-wide TKL layout (keyboard.json), so position grows from left to right. The key at matrix position
 * (row, col) has its LED at slot col * 8 + row (stock V0101 tables @0x3027 and @0x31a9).
 * Also: slot 0x57 (COM 10, group 7), the small LED right of Esc (stock effect grid @0x30a7), at x 1.25; the ISO
 * key left of Z (slot 12) and ISO # (slot 99) at their ISO positions, the ABNT2 key left of Right Shift (slot 100)
 * at its ABNT2 position. LED_FX_SKIP: the Caps Lock (0x4F) and Scroll Lock (0x47) indicators, driven by
 * redragon_ch555.c.
 */
static __code const uint8_t led_pos[LED_FX_COLUMNS * LED_FX_ROWS] = {
    // clang-format off
      5,   5,   8,   9,  12,   7,   0,   0, // COM 0
     26,  16,  21,  24,  18,  20,   0,   0, // COM 1
     37,  26,  32,  34,  29,  33,   0,   0, // COM 2
     47,  37,  42,  45,  39,   0,   0,   0, // COM 3
     58,  47,  53,  55,  50,   0,   0,   0, // COM 4
     74,  58,  63,  66,  60,  72,   0,   0, // COM 5
     84,  68,  74,  76,  71,   0,   0,   0, // COM 6
     95,  79,  84,  87,  82,   0,   0,   0, // COM 7
    105,  89,  95,  97,  92,   0,   0, LED_FX_SKIP, // COM 8
    121, 100, 105, 108, 103, 112,   0, LED_FX_SKIP, // COM 9
    132, 110, 116, 118, 113, 125,   0,  13, // COM 10
    142, 121, 126, 129, 124, 138,   0,   0, // COM 11
    153, 132, 137, 139, 134, 151,   0,   0, // COM 12
    166, 147, 150, 146, 143, 166,   0,   0, // COM 13
    176, 166, 166, 187, 176, 176,   0,   0, // COM 14
    187, 176, 176, 187,   0, 187,   0,   0, // COM 15
    // clang-format on
};
