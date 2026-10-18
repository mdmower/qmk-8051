/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "redragon_ch555.h"

/* Roller: the mode key switches the roller between volume and key LED brightness; the encoder map sends the
 * roller turns as K580_RLDN and K580_RLUP. */
enum k580_keycodes {
    K580_ROLL = RD_KB_SAFE_RANGE,
    K580_RLDN,
    K580_RLUP,
};
