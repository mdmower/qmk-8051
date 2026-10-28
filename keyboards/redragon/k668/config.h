/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* The matrix rows are also the sinks of the LED columns: an unselected row is driven high, not left to the pull-up. */
#define MATRIX_UNSELECT_DRIVE_HIGH

/* Tap keys (LT, MT, TT, *_T, tap-toggle) and one-shot keys (OSM, OSL) are off: action_tapping.c does not compile
 * with SDCC (compound literal assignments, case ranges). MO() works. */
#define NO_ACTION_TAPPING
#define NO_ACTION_ONESHOT
