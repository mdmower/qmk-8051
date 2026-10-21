/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* usb.max_power (keyboard.json) is 500 mA: the keyboard draws up to 420 mA with all LEDs at full white. */

/* Roller (encoder in keyboard.json, P5.5/P5.6, 2 transitions per detent): 10 ms between the press and the
 * release of its keycode (encoder map) */
#define ENCODER_MAP_KEY_DELAY 10

/* MCU sleep in USB suspend; the keys wake it (keyboards/redragon/common/led_dma.c) */
#define CH555_SUSPEND_SLEEP

/* Indicator LEDs enabled by P5.4, active low (stock 1.04 @0xdc4b); Win Lock LED on P5.0
 * (keyboards/redragon/common/redragon_ch555.h) */
#define LED_INDICATOR_ENABLE_PIN PORT5_4
#define LED_WIN_LOCK_PIN PORT5_0
/* Saved with the lighting settings: the roller mode and the Rec LED (k580.c) */
#define REDRAGON_KB_SETTINGS 2

/* Disabled for flash space: tap keys (LT, MT, TT, *_T, tap-toggle) and one-shot keys (OSM, OSL) do nothing;
 * MO() still works. */
#define NO_ACTION_TAPPING
#define NO_ACTION_ONESHOT

/* Lighting effects (keyboards/redragon/common/led_fx.h): key LEDs and side lights; the Rec LED through
 * led_fx_mono_val() (k580.c) */
#define LED_FX_ZONES 2
#define LED_FX_MONO_HOOK
