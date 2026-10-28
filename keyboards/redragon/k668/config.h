/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* The matrix rows are also the sinks of the LED columns: an unselected row is driven high, not left to the pull-up. */
#define MATRIX_UNSELECT_DRIVE_HIGH

/* Win Lock LED, active low */
#define LED_WIN_LOCK_PIN PORT4_7

/* The NKRO report has a 14-byte key bitmap (usages 0x00-0x6F) so that it fits a 16-byte packet of the shared
 * endpoint. The keymap has no keycode above 0x6F. */
#define NKRO_REPORT_BITS 14

/* Tap keys (LT, MT, TT, *_T, tap-toggle) and one-shot keys (OSM, OSL) are off: action_tapping.c does not compile
 * with SDCC (compound literal assignments, case ranges). MO() works. */
#define NO_ACTION_TAPPING
#define NO_ACTION_ONESHOT
