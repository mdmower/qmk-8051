/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* MCU sleep in USB suspend; the keys wake it (keyboards/redragon/common/led_dma.c) */
#define CH555_SUSPEND_SLEEP

/* Lighting effects (keyboards/redragon/common/led_fx.h): the key LEDs, one zone; the Caps Lock and Scroll Lock
 * indicators are LEDs of the LED matrix that k552.c drives (LED_FX_SKIP in led_fx_pos.h). */
#define LED_FX_SKIP_ENABLE

/* Disabled for flash space, as on the K580: tap keys (LT, MT, TT, *_T, tap-toggle) and one-shot keys (OSM, OSL)
 * do nothing; MO() still works. */
#define NO_ACTION_TAPPING
#define NO_ACTION_ONESHOT
