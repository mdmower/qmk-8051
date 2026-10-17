/* SPDX-License-Identifier: GPL-2.0-or-later */

/* The default keymap, editable from VIA (rules.mk). Its custom keycodes take the keyboard range from QK_KB_0,
 * the range that the VIA definition (redragon_k580_via.json) names. */
#include QMK_KEYBOARD_H
#undef SAFE_RANGE
#define SAFE_RANGE QK_KB_0
#include "../default/keymap.c"
