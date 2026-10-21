/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* LED positions of the K551 for the lighting effects (keyboards/redragon/common/led_fx.c, which includes this). */

/*
 * Position of each LED, 0-191, by buffer slot (COM * 8 + group), from tools/led_fx_pos.py: key center x on the
 * 22.5-unit-wide full-size layout (keyboard.json), so position grows from left to right. The key at matrix
 * position (row, col) has its LED at slot col * 8 + row (stock V0107 tables @0x30a7 and @0x3241).
 * Also: the ISO key left of Z (slot 12) and ISO # (slot 99) at their ISO positions, the ABNT2 key left of Right
 * Shift (slot 100) and numpad comma (slot 127) at their ABNT2 positions. LED_FX_SKIP: the lock indicators (slots
 * 0x47 and 0x4F), driven by keyboards/redragon/common/redragon_ch555.c.
 */
static __code const uint8_t led_pos[LED_DMA_COMS * LED_DMA_GROUPS] = {
    // clang-format off
      4,   4,   6,   7,  10,   5, 162,   0, // COM 0
     21,  13,  17,  19,  15,  16, 171,   0, // COM 1
     30,  21,  26,  28,  23,  27, 179,   0, // COM 2
     38,  30,  34,  36,  32,   0, 188,   0, // COM 3
     47,  38,  43,  45,  41,   0, 162,   0, // COM 4
     60,  47,  51,  53,  49,  59, 171,   0, // COM 5
     68,  55,  60,  62,  58,   0, 179,   0, // COM 6
     77,  64,  68,  70,  66,   0, 162,   0, // COM 7
     85,  73,  77,  79,  75,   0, 171, LED_FX_SKIP, // COM 8
     98,  81,  85,  87,  83,  91, 179, LED_FX_SKIP, // COM 9
    107,  90,  94,  96,  92, 101, 188,   0, // COM 10
    115,  98, 102, 105, 100, 112, 171,   0, // COM 11
    124, 107, 111, 113, 109, 123, 179,   0, // COM 12
    134, 119, 122, 118, 116, 134, 166,   0, // COM 13
    143, 134, 134, 151, 143, 143, 179,   0, // COM 14
    151, 143, 143, 151, 162, 151, 188, 188, // COM 15
    // clang-format on
};

/*
 * Key row of each LED, 0-5 (0: the Esc row), by buffer slot, from tools/led_fx_pos.py --rows: the row of the key's
 * top edge in the layout (keyboard.json); numpad comma (slot 127) in the row below numpad +, as on ABNT2 boards.
 * Slots without a key: their group.
 */
static __code const uint8_t led_row[LED_DMA_COMS * LED_DMA_GROUPS] = {
    // clang-format off
      0,   1,   2,   3,   4,   5,   1,   7, // COM 0
      0,   1,   2,   3,   4,   5,   1,   7, // COM 1
      0,   1,   2,   3,   4,   5,   1,   7, // COM 2
      0,   1,   2,   3,   4,   5,   1,   7, // COM 3
      0,   1,   2,   3,   4,   5,   2,   7, // COM 4
      0,   1,   2,   3,   4,   5,   2,   7, // COM 5
      0,   1,   2,   3,   4,   5,   2,   7, // COM 6
      0,   1,   2,   3,   4,   5,   3,   7, // COM 7
      0,   1,   2,   3,   4,   5,   3,   7, // COM 8
      0,   1,   2,   3,   4,   5,   3,   7, // COM 9
      0,   1,   2,   3,   4,   5,   2,   7, // COM 10
      0,   1,   2,   3,   4,   5,   4,   7, // COM 11
      0,   1,   2,   3,   4,   5,   4,   7, // COM 12
      0,   1,   2,   3,   4,   5,   5,   7, // COM 13
      0,   1,   2,   1,   4,   5,   5,   7, // COM 14
      0,   1,   2,   2,   4,   5,   4,   3, // COM 15
    // clang-format on
};
