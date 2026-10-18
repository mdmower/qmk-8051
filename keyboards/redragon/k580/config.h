/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* usb.max_power (keyboard.json) is 500 mA: the keyboard draws up to 420 mA with all LEDs at full white. */

/* Roller (encoder in keyboard.json, P5.5/P5.6, 2 transitions per detent): 10 ms between the press and the
 * release of its keycode (encoder map) */
#define ENCODER_MAP_KEY_DELAY 10

/* MCU sleep in USB suspend; the keys wake it (keyboards/redragon/common/led_dma.c) */
#define CH555_SUSPEND_SLEEP

/* Win Lock LED (Fn+Win in the default keymap) */
#define LED_WIN_LOCK_PIN PORT5_0

/* Disabled for flash space: tap keys (LT, MT, TT, *_T, tap-toggle) and one-shot keys (OSM, OSL) do nothing;
 * MO() still works. */
#define NO_ACTION_TAPPING
#define NO_ACTION_ONESHOT

/* Lighting effects (keyboards/redragon/common/led_fx.h): key LEDs and side lights */
#define LED_FX_ZONES 2
