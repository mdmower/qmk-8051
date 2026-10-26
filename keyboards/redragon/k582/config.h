/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* MCU sleep in USB suspend; the keys wake it (led_dma.c) */
#define CH555_SUSPEND_SLEEP

/* Lock indicators above the numpad: single-color LEDs in group 7 of COMs 8 and 9 of the LED matrix (stock V0107
 * @0xdca8), set by redragon_ch555.c. */
#define LED_NUM_LOCK_BYTE LED_DMA_BYTE(8, 7, 2)
#define LED_CAPS_LOCK_BYTE LED_DMA_BYTE(9, 7, 1)
#define LED_SCROLL_LOCK_BYTE LED_DMA_BYTE(9, 7, 3)
#define LED_WIN_LOCK_BYTE LED_DMA_BYTE(9, 7, 2)

/* Lighting effects (led_fx.h): the key LEDs, one zone; the indicators are skipped
 * (LED_FX_SKIP in led_fx_pos.h). */
#define LED_FX_SKIP_ENABLE
/* The numpad (except 1), Page Up and Page Down are not wired in the matrix row of their key row: the row modes
 * take the key row from led_row[] in led_fx_pos.h. */
#define LED_FX_KEY_ROWS

/* Disabled for flash space, as on the K580: tap keys (LT, MT, TT, *_T, tap-toggle) and one-shot keys (OSM, OSL)
 * do nothing; MO() still works. */
#define NO_ACTION_TAPPING
#define NO_ACTION_ONESHOT
