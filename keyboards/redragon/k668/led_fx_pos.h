/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* LED positions of the K668 for the lighting effects (led_fx.c includes this file). */

/*
 * Position of each LED along its zone, 0-191, by slot (matrix row * 7 + matrix column; LED rows 0-5 are the key
 * rows, row 6 the side lights). F_POS modes add it to the hue (F_REV: subtract it), and F_MOVE modes shift the hue so
 * that the colors move toward higher positions.
 * Keys: key center x on a 22.75-unit-wide board, so position grows from left to right. 0 where there is no key.
 * Side lights: LED row 6 of matrix rows 0-16 is one path of 17 LEDs around the case: the left side from back to
 * front (rows 0-2), the front strip from left to right (rows 3-13), the right side from front to back (rows 14-16).
 * Position grows along the path by 192 / 17 per LED. Row 6 of matrix rows 17-20 has no LED.
 */
static __code const uint8_t led_pos[LED_FX_COLUMNS * LED_FX_ROWS] = {
    // clang-format off
      4,   4,   6,   7,   9,   5,   0, // matrix row 0
      0,  13,  17,  19,  23,  16,  11, // matrix row 1
     21,  21,  25,  27,  32,  26,  22, // matrix row 2
     30,  30,  34,  36,  40,   0,  33, // matrix row 3
     38,  38,  42,  44,  49,   0,  45, // matrix row 4
     46,  46,  51,  53,  57,  58,  56, // matrix row 5
     59,  55,  59,  61,  65,   0,  67, // matrix row 6
     68,  63,  68,  70,  74,   0,  79, // matrix row 7
     76,  72,  76,  78,  82,  90,  90, // matrix row 8
     84,  80,  84,  87,  91, 100, 101, // matrix row 9
     97,  89,  93,  95,  99, 111, 112, // matrix row 10
    105,  97, 101, 103,   0,   0, 124, // matrix row 11
    114, 105, 110,   0,   0,   0, 135, // matrix row 12
    122, 120, 120, 117, 115, 121, 146, // matrix row 13
    135, 135, 135,   0,   0, 135, 158, // matrix row 14
    143, 143, 143,   0, 143, 143, 169, // matrix row 15
    152, 152, 152,   0,   0, 152, 180, // matrix row 16
    162, 162, 162, 162, 162, 167,   0, // matrix row 17
    171, 171, 171, 171, 171,   0,   0, // matrix row 18
    179, 179, 179, 179, 179, 179,   0, // matrix row 19
    188, 188, 188,   0, 188,   0,   0, // matrix row 20
    // clang-format on
};
