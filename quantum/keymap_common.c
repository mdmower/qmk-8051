/*
Copyright 2012-2017 Jun Wako <wakojun@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "keymap_common.h"
#include "keymap_introspection.h"
#include "report.h"
#include "keycode.h"
#include "action_layer.h"
#include "action.h"
#include "debug.h"
#include "keycode_config.h"
#include "quantum_keycodes.h"

#ifdef ENCODER_MAP_ENABLE
#    include "encoder.h"
#endif

#ifdef DIP_SWITCH_MAP_ENABLE
#    include "dip_switch.h"
#endif

#ifdef BACKLIGHT_ENABLE
#    include "backlight.h"
#endif

#ifdef MIDI_ENABLE
#    include "process_midi.h"
#endif

extern keymap_config_t keymap_config;

/* converts key to action */
action_t action_for_key(uint8_t layer, keypos_t key) {
    // 16bit keycodes - important
    uint16_t keycode = keymap_key_to_keycode(layer, key);
#ifdef __SDCC
    // SDCC returns a struct correctly only from a variable (https://sourceforge.net/p/sdcc/bugs/4157/)
    action_t action = action_for_keycode(keycode);
    return action;
#else
    return action_for_keycode(keycode);
#endif
}

action_t action_for_keycode(uint16_t keycode) {
    // keycode remapping
    keycode = keycode_config(keycode);

    action_t action = {};
    uint8_t  action_layer, mod;

    (void)action_layer;
    (void)mod;

    if (IS_BASIC_KEYCODE(keycode) || IS_MODIFIER_KEYCODE(keycode)) {
        action.code = ACTION_KEY(keycode);
#ifdef EXTRAKEY_ENABLE
    } else if (IS_SYSTEM_KEYCODE(keycode)) {
        action.code = ACTION_USAGE_SYSTEM(KEYCODE2SYSTEM(keycode));
    } else if (IS_CONSUMER_KEYCODE(keycode)) {
        action.code = ACTION_USAGE_CONSUMER(KEYCODE2CONSUMER(keycode));
#endif
    } else if (IS_MOUSE_KEYCODE(keycode)) {
        action.code = ACTION_MOUSEKEY(keycode);
    } else if (keycode == KC_TRANSPARENT) {
        action.code = ACTION_TRANSPARENT;
    } else if (IS_QK_MODS(keycode)) {
        // Has a modifier
        // Split it up
#ifdef LEGACY_MAGIC_HANDLING
        action.code = ACTION_MODS_KEY(QK_MODS_GET_MODS(keycode), QK_MODS_GET_BASIC_KEYCODE(keycode)); // adds modifier to key
#else                                                                                                     // LEGACY_MAGIC_HANDLING
        action.code = ACTION_MODS_KEY(mod_config(QK_MODS_GET_MODS(keycode)), keycode_config(QK_MODS_GET_BASIC_KEYCODE(keycode))); // adds modifier to key
#endif                                                                                                    // LEGACY_MAGIC_HANDLING
    } else if ((keycode >= QK_LAYER_TAP && keycode <= QK_LAYER_TAP_MAX)) {
#if !defined(NO_ACTION_LAYER) && !defined(NO_ACTION_TAPPING)
#    ifdef LEGACY_MAGIC_HANDLING
        action.code = ACTION_LAYER_TAP_KEY(QK_LAYER_TAP_GET_LAYER(keycode), QK_LAYER_TAP_GET_TAP_KEYCODE(keycode));
#    else  // LEGACY_MAGIC_HANDLING
        action.code = ACTION_LAYER_TAP_KEY(QK_LAYER_TAP_GET_LAYER(keycode), keycode_config(QK_LAYER_TAP_GET_TAP_KEYCODE(keycode)));
#    endif // LEGACY_MAGIC_HANDLING
#else
        // pass through keycode_config again, since it previously missed it
        // and then only send as ACTION_KEY to bypass most of action.c handling
        action.code = ACTION_KEY(keycode_config(QK_LAYER_TAP_GET_TAP_KEYCODE(keycode)));
#endif
#ifndef NO_ACTION_LAYER
    } else if ((keycode >= QK_TO && keycode <= QK_TO_MAX)) {
        // Layer set "GOTO"
        action_layer = QK_TO_GET_LAYER(keycode);
        action.code  = ACTION_LAYER_GOTO(action_layer);
    } else if ((keycode >= QK_MOMENTARY && keycode <= QK_MOMENTARY_MAX)) {
        // Momentary action_layer
        action_layer = QK_MOMENTARY_GET_LAYER(keycode);
        action.code  = ACTION_LAYER_MOMENTARY(action_layer);
    } else if ((keycode >= QK_DEF_LAYER && keycode <= QK_DEF_LAYER_MAX)) {
        // Set default action_layer
        action_layer = QK_DEF_LAYER_GET_LAYER(keycode);
        action.code  = ACTION_DEFAULT_LAYER_SET(action_layer);
    } else if ((keycode >= QK_TOGGLE_LAYER && keycode <= QK_TOGGLE_LAYER_MAX)) {
        // Set toggle
        action_layer = QK_TOGGLE_LAYER_GET_LAYER(keycode);
        action.code  = ACTION_LAYER_TOGGLE(action_layer);
#endif
#ifndef NO_ACTION_ONESHOT
    } else if ((keycode >= QK_ONE_SHOT_LAYER && keycode <= QK_ONE_SHOT_LAYER_MAX)) {
        // OSL(action_layer) - One-shot action_layer
        action_layer = QK_ONE_SHOT_LAYER_GET_LAYER(keycode);
        action.code  = ACTION_LAYER_ONESHOT(action_layer);
#endif // NO_ACTION_ONESHOT
    } else if ((keycode >= QK_ONE_SHOT_MOD && keycode <= QK_ONE_SHOT_MOD_MAX)) {
        // OSM(mod) - One-shot mod
        mod = mod_config(QK_ONE_SHOT_MOD_GET_MODS(keycode));
#if defined(NO_ACTION_TAPPING) || defined(NO_ACTION_ONESHOT)
        action.code = ACTION_MODS(mod);
#else  // defined(NO_ACTION_TAPPING) || defined(NO_ACTION_ONESHOT)
        action.code = ACTION_MODS_ONESHOT(mod);
#endif // defined(NO_ACTION_TAPPING) || defined(NO_ACTION_ONESHOT)
#ifndef NO_ACTION_LAYER
    } else if ((keycode >= QK_LAYER_TAP_TOGGLE && keycode <= QK_LAYER_TAP_TOGGLE_MAX)) {
#    ifndef NO_ACTION_TAPPING
        action.code = ACTION_LAYER_TAP_TOGGLE(QK_LAYER_TAP_TOGGLE_GET_LAYER(keycode));
#    else // NO_ACTION_TAPPING
#        ifdef NO_ACTION_TAPPING_TAP_TOGGLE_MO
        action.code = ACTION_LAYER_MOMENTARY(QK_LAYER_TAP_TOGGLE_GET_LAYER(keycode));
#        else  // NO_ACTION_TAPPING_TAP_TOGGLE_MO
        action.code = ACTION_LAYER_TOGGLE(QK_LAYER_TAP_TOGGLE_GET_LAYER(keycode));
#        endif // NO_ACTION_TAPPING_TAP_TOGGLE_MO
#    endif     // NO_ACTION_TAPPING
    } else if ((keycode >= QK_LAYER_MOD && keycode <= QK_LAYER_MOD_MAX)) {
        mod          = mod_config(QK_LAYER_MOD_GET_MODS(keycode));
        action_layer = QK_LAYER_MOD_GET_LAYER(keycode);
        action.code  = ACTION_LAYER_MODS(action_layer, (mod & 0x10) ? (mod & 0xF) << 4 : mod);
#endif // NO_ACTION_LAYER
    } else if ((keycode >= QK_MOD_TAP && keycode <= QK_MOD_TAP_MAX)) {
#ifndef NO_ACTION_TAPPING
        mod = mod_config(QK_MOD_TAP_GET_MODS(keycode));
#    ifdef LEGACY_MAGIC_HANDLING
        action.code = ACTION_MODS_TAP_KEY(mod, QK_MOD_TAP_GET_TAP_KEYCODE(keycode));
#    else  // LEGACY_MAGIC_HANDLING
        action.code = ACTION_MODS_TAP_KEY(mod, keycode_config(QK_MOD_TAP_GET_TAP_KEYCODE(keycode)));
#    endif // LEGACY_MAGIC_HANDLING
#else      // NO_ACTION_TAPPING
#    ifdef NO_ACTION_TAPPING_MODTAP_MODS
        // pass through mod_config again, since it previously missed it
        // and then only send as ACTION_KEY to bypass most of action.c handling
        action.code = ACTION_MODS(mod_config(QK_MOD_TAP_GET_MODS(keycode)));
#    else  // NO_ACTION_TAPPING_MODTAP_MODS
           // pass through keycode_config again, since it previously missed it
           // and then only send as ACTION_KEY to bypass most of action.c handling
        action.code = ACTION_KEY(keycode_config(QK_MOD_TAP_GET_TAP_KEYCODE(keycode)));
#    endif // NO_ACTION_TAPPING_MODTAP_MODS
#endif     // NO_ACTION_TAPPING
#ifdef SWAP_HANDS_ENABLE
    } else if ((keycode >= QK_SWAP_HANDS && keycode <= QK_SWAP_HANDS_MAX)) {
#    ifdef LEGACY_MAGIC_HANDLING
        action.code = ACTION(ACT_SWAP_HANDS, QK_SWAP_HANDS_GET_TAP_KEYCODE(keycode));
#    else  // LEGACY_MAGIC_HANDLING
        action.code = ACTION(ACT_SWAP_HANDS, keycode_config(QK_SWAP_HANDS_GET_TAP_KEYCODE(keycode)));
#    endif // LEGACY_MAGIC_HANDLING
#endif

    } else {
        action.code = ACTION_NO;
    }

    return action;
}

// translates key to keycode
__attribute__((weak)) uint16_t keymap_key_to_keycode(uint8_t layer, keypos_t key) {
    if (key.row < MATRIX_ROWS && key.col < MATRIX_COLS) {
        return keycode_at_keymap_location(layer, key.row, key.col);
    }
#ifdef ENCODER_MAP_ENABLE
    else if (key.row == KEYLOC_ENCODER_CW && key.col < NUM_ENCODERS) {
        return keycode_at_encodermap_location(layer, key.col, true);
    } else if (key.row == KEYLOC_ENCODER_CCW && key.col < NUM_ENCODERS) {
        return keycode_at_encodermap_location(layer, key.col, false);
    }
#endif // ENCODER_MAP_ENABLE
#ifdef DIP_SWITCH_MAP_ENABLE
    else if (key.row == KEYLOC_DIP_SWITCH_ON && key.col < NUM_DIP_SWITCHES) {
        return keycode_at_dip_switch_map_location(key.col, true);
    } else if (key.row == KEYLOC_DIP_SWITCH_OFF && key.col < NUM_DIP_SWITCHES) {
        return keycode_at_dip_switch_map_location(key.col, false);
    }
#endif // DIP_SWITCH_MAP_ENABLE

    return KC_NO;
}
