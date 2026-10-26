/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* MCU sleep in USB suspend; the keys wake it (led_dma.c) */
#define CH555_SUSPEND_SLEEP

/* Caps Lock and Scroll Lock indicators (right of the logo): LEDs in group 7 of COMs 9 and 8 of the LED matrix, Caps
 * Lock on the red line and Scroll Lock on the green line (stock V0101 @0xd988), set by
 * redragon_ch555.c. */
#define LED_CAPS_LOCK_BYTE LED_DMA_BYTE(9, 7, 1)
#define LED_SCROLL_LOCK_BYTE LED_DMA_BYTE(8, 7, 2)

/* Lighting effects (led_fx.h): the key LEDs, one zone; the indicators are skipped
 * (LED_FX_SKIP in led_fx_pos.h). */
#define LED_FX_SKIP_ENABLE

/* Disabled for flash space, as on the K580: tap keys (LT, MT, TT, *_T, tap-toggle) and one-shot keys (OSM, OSL)
 * do nothing; MO() still works. */
#define NO_ACTION_TAPPING
#define NO_ACTION_ONESHOT
