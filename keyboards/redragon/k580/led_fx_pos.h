/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* LED positions of the K580 for the lighting effects (keyboards/redragon/common/led_fx.c, which includes this). */

/*
 * Position of each LED along its zone, 0-191, by buffer slot (COM * 8 + group). F_POS modes add it
 * to the hue (F_REV: subtract it), and F_MOVE modes shift the hue so that the colors move towards
 * higher positions.
 * Keys: key center x on a 22.5-unit-wide board (standard 104-key positions), so position grows from
 * left to right.
 * Side lights: pair index * 24; stock table @0x30c9 pairs COM c with COM 15 - c, one on each side.
 * Pair 0 (COMs 0 and 15) is at the back of the keyboard (the edge away from the user), pair 7 (COMs 7
 * and 8) at the front, so position grows from back to front.
 * LED_FX_MONO: one LED on the red line, white for G1-G5, red for Rec and the media keys.
 */
static __code const uint8_t led_pos[LED_DMA_COMS * LED_DMA_GROUPS] = {
    // clang-format off
      4,   4,   6,   7,  10,   5,   0,   0, // COM 0
     21,  13,  17,  19,   0,  16,   0,  24, // COM 1
     30,  21,  26,  28,  23,  27,   0,  48, // COM 2
     38,  30,  34,  36,  32,  59,   0,  72, // COM 3
     47,  38,  43,  45,  41,  91,   0,  96, // COM 4
     60,  47,  51,  53,  49, 101,   0, 120, // COM 5
     68,  55,  60,  62,  58, 112, LED_FX_MONO, 144, // COM 6
     77,  64,  68,  70,  66, 123, LED_FX_MONO, 168, // COM 7
     85,  73,  77,  79,  75, 134, LED_FX_MONO, 168, // COM 8
     98,  81,  85,  87,  83, 143, LED_FX_MONO, 144, // COM 9
    107,  90,  94,  96,  92, 151, LED_FX_MONO, 120, // COM 10
    115,  98, 102, 105, 100, 166, LED_FX_MONO,  96, // COM 11
    124, 107, 111,   0,   0, 179, LED_FX_MONO,  72, // COM 12
    134, 119, 122, 118, 116, 162, LED_FX_MONO,  48, // COM 13
    143, 134, 134, 162, 143, 171, LED_FX_MONO,  24, // COM 14
    151, 143, 143, 171, 162, 179, LED_FX_MONO,   0, // COM 15
    162, 151, 151, 179, 171, 188,   0,   0, // COM 16
    171, 179, 188, 188, 179,   0,   0,   0, // COM 17
    // clang-format on
};
