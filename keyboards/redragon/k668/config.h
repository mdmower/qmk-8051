/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* Tap keys (LT, MT, TT, *_T, tap-toggle) and one-shot keys (OSM, OSL) are off: action_tapping.c does not compile
 * with SDCC (compound literal assignments, case ranges). MO() works. */
#define NO_ACTION_TAPPING
#define NO_ACTION_ONESHOT
