/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Lighting effects for LED_FX_ZONES zones with separate settings (keyboard config.h, default 1): zone 0 = the key
 * LEDs (with 2 zones: groups 0-6), zone 1 = the side lights (group 7).
 * Settings of a zone, in this order; an action is zone * LED_FX_SETTINGS + setting.
 */
#ifndef LED_FX_ZONES
#    define LED_FX_ZONES 1
#endif
enum { LED_FX_MODE, LED_FX_HUE, LED_FX_SAT, LED_FX_VAL, LED_FX_SPEED, LED_FX_ON, LED_FX_SETTINGS };

/* Saved form of the settings: LED_FX_SAVE_SIZE bytes, zone 0 first, each in the order above. */
#define LED_FX_SAVE_SIZE (LED_FX_ZONES * LED_FX_SETTINGS)
/* Sets the settings from their saved form (in code space); a value out of range keeps its default. */
void led_fx_load(__code const uint8_t *p);
/* Writes the saved form of the settings to dst. */
void led_fx_save(__xdata uint8_t *dst);
/* Next step of a setting (previous step with reverse); LED_FX_ON toggles. */
void led_fx_adjust(uint8_t action, bool reverse);
/* Call from the main loop: renders the effects into the LED buffer, one COM per call. */
void led_fx_task(void);

/*
 * The keyboard's led_fx_pos.h defines led_pos[LED_DMA_COMS * LED_DMA_GROUPS]: the position of each LED along
 * its zone, 0-191, by buffer slot (COM * 8 + group); LED_FX_MONO for a single-color LED on the red line;
 * LED_FX_SKIP (with LED_FX_SKIP_ENABLE in config.h) for an LED that the keyboard drives itself, such as a lock
 * indicator: the effects do not write it.
 * With LED_FX_KEY_ROWS in config.h, it also defines led_row[LED_DMA_COMS * LED_DMA_GROUPS]: the key row (0-5, top
 * row 0) of each LED, by buffer slot, for the row modes. Without it, a key's row is its LED group, i.e. its matrix
 * row.
 */
#define LED_FX_MONO 0xFF
#define LED_FX_SKIP 0xFE

#ifdef LED_FX_MONO_HOOK
/* With LED_FX_MONO_HOOK in config.h, defined by the keyboard or keymap: brightness of the single-color LED in
 * buffer slot slot (COM * 8 + group), given its zone's brightness val. */
uint8_t led_fx_mono_val(uint8_t slot, uint8_t val);
/* Renders all LEDs again, for a change in what led_fx_mono_val() returns. */
void led_fx_refresh(void);
#endif
