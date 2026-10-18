/* SPDX-License-Identifier: GPL-2.0-or-later */

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
	[0] = LAYOUT_default(
        KC_ESC,   KC_F1,   	KC_F2,   	KC_F3,   	KC_F4,   	KC_F5,   	KC_F6,   	KC_F7,   	KC_F8,    KC_F9,    KC_F10,   KC_F11,  	KC_F12,  	KC_PSCR, 	KC_SCRL, 	KC_PAUS,    KC_NUM,  KC_PSLS,
        KC_GRV,   KC_1,    	KC_2,    	KC_3,    	KC_4,    	KC_5,    	KC_6,    	KC_7,    	KC_8,     KC_9,    	KC_0,     KC_MINS, 	KC_EQL,  	KC_BSPC, 	KC_INS,  	KC_HOME,    KC_PGUP, KC_PAST,
        KC_TAB,   KC_Q,    	KC_W,    	KC_E,    	KC_R,    	KC_T,    	KC_Y,    	KC_U,    	KC_I,     KC_O,    	KC_P,     KC_LBRC, 	KC_RBRC, 	KC_BSLS, 	KC_DEL,  	KC_END,     KC_PGDN, KC_PMNS,
        KC_CAPS,  KC_A,   	KC_S,    	KC_D,    	KC_F,    	KC_G,    	KC_H,    	KC_J,    	KC_K,     KC_L,    	KC_SCLN,  KC_QUOT, 	          KC_ENT,   KC_P7,    KC_P8,      KC_P9,   KC_PPLS,
        KC_LSFT,  KC_Z,     KC_X,    	KC_C,    	KC_V,    	KC_B,    	KC_N,    	KC_M,    	KC_COMM,  KC_DOT,  	KC_SLSH,             	        KC_RSFT,  KC_UP,    KC_P4,      KC_P5,   KC_P6,
        KC_LCTL,  KC_LGUI,  KC_LALT, 	KC_SPC,  	KC_RALT, 	MO(1),   	KC_APP,   	KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RIGHT, KC_P0, 	  KC_PDOT, 	KC_P1,  	KC_P2,    KC_P3,      KC_PENT,
        /* row 6 — G1-G5 and Rec send F13-F18 (bind them in the OS), roller toggle, media keys */
                                                                    KC_F13,   KC_F14,   KC_F15,    KC_F16,  KC_F17,   KC_F18,   K580_ROLL,   KC_MPRV, KC_MPLY, KC_MNXT),
	/* Fn layer: Fn+Esc = bootloader, Fn+F1-F12 = the functions printed on the keycaps (media player,
	 * volume down/up, mute, stop, previous, play/pause, next, mail, browser home, calculator, search),
	 * Fn+Win = Win Lock. Lighting (Shift reverses): Ins/G1 mode, Home/G2 hue, PgUp/G3 saturation,
	 * Del/G4 brightness, End/G5 speed, PgDn/Rec on/off; Ins-PgDn for the keys, G1-Rec for the side
	 * lights. */
	[1] = LAYOUT_default(
        QK_BOOT, KC_MSEL, KC_VOLD, KC_VOLU, KC_MUTE, KC_MSTP, KC_MPRV, KC_MPLY, KC_MNXT, KC_MAIL, KC_WHOM, KC_CALC, KC_WSCH, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, RD_KMODE, RD_KHUE, RD_KSAT, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, RD_KVAL, RD_KSPD, RD_KTOG, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, RD_WINLK, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        RD_SMODE, RD_SHUE, RD_SSAT, RD_SVAL, RD_SSPD, RD_STOG, _______, _______, _______, _______),
};

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = { ENCODER_CCW_CW(K580_RLDN, K580_RLUP) }, // volume or key LED brightness (K580_ROLL)
    [1] = { ENCODER_CCW_CW(_______, _______) },
};
#endif
