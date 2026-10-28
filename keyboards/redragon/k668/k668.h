/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "quantum.h"
#include "keyboards/redragon/common/led_fx.h"

/* Keycodes from QK_KB_0: the lighting settings in led_fx.h order, key LEDs, then side lights; Shift steps a setting
 * back. */
enum k668_keycodes {
    RD_KMODE = QK_KB_0,
    RD_KHUE,
    RD_KSAT,
    RD_KVAL,
    RD_KSPD,
    RD_KTOG,
    RD_SMODE,
    RD_SHUE,
    RD_SSAT,
    RD_SVAL,
    RD_SSPD,
    RD_STOG,
};
