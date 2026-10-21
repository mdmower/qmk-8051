/* SPDX-License-Identifier: GPL-2.0-or-later */

#include QMK_KEYBOARD_H
#include "eeprom.h"
#include "nvm_eeprom_eeconfig_internal.h" // EECONFIG_SIZE

/*
 * Macro recorder on G1-G5 and Rec. Rec, then a G key, then the keys of the macro, then Rec again: the G key plays
 * the macro back. Rec, a G key, Rec clears that G key's macro. Rec or Esc while choosing the G key cancels. A G
 * key without a macro sends nothing.
 * A macro is the sequence of presses and releases of basic keycodes (keys, modifiers, media keys, up to 0xFF) after
 * the layer lookup; other keycodes (MO, lighting, QK_BOOT) are not recorded. A macro that fills its slot ends the
 * recording. Playback sends one press or release every MACRO_DELAY ms and then releases the keys that were held
 * when the recording ended.
 * The macros are kept in the EEPROM (code flash, rules.mk) after QMK's settings, one slot per G key: a count byte,
 * then 2 bytes per event, the keycode and 1 for a press or 0 for a release.
 * LEDs: Rec is lit while choosing the G key and blinks while recording; the G keys blink while choosing, and only
 * the chosen G key is lit while recording. Otherwise Rec is dark and the G keys light at the key LEDs' brightness.
 */
enum macro_keycodes {
    MC_G1 = QK_USER,
    MC_G2,
    MC_G3,
    MC_G4,
    MC_G5,
    MC_REC,
};
#define MACRO_COUNT 5
#define MACRO_SLOT_SIZE ((EEPROM_SIZE - EECONFIG_SIZE) / MACRO_COUNT)
#define MACRO_EVENTS ((MACRO_SLOT_SIZE - 1) / 2)
_Static_assert(MACRO_EVENTS >= 1 && MACRO_EVENTS <= 255, "macro slots: 1-255 events");
#define MACRO_DELAY 10  // ms between the events of a playback
#define MACRO_BLINK 250 // ms per half period of the blinking LEDs
#define MACRO_LED_VAL 144
/* LED buffer slots (COM * 8 + group) of G1 and Rec: matrix row 6, columns 6 and 11; G2-G5 between them */
#define MACRO_LED_G1 (6 * 8 + 6)
#define MACRO_LED_REC (11 * 8 + 6)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
	[0] = LAYOUT_default(
        KC_ESC,   KC_F1,   	KC_F2,   	KC_F3,   	KC_F4,   	KC_F5,   	KC_F6,   	KC_F7,   	KC_F8,    KC_F9,    KC_F10,   KC_F11,  	KC_F12,  	KC_PSCR, 	KC_SCRL, 	KC_PAUS,    KC_NUM,  KC_PSLS,
        KC_GRV,   KC_1,    	KC_2,    	KC_3,    	KC_4,    	KC_5,    	KC_6,    	KC_7,    	KC_8,     KC_9,    	KC_0,     KC_MINS, 	KC_EQL,  	KC_BSPC, 	KC_INS,  	KC_HOME,    KC_PGUP, KC_PAST,
        KC_TAB,   KC_Q,    	KC_W,    	KC_E,    	KC_R,    	KC_T,    	KC_Y,    	KC_U,    	KC_I,     KC_O,    	KC_P,     KC_LBRC, 	KC_RBRC, 	KC_BSLS, 	KC_DEL,  	KC_END,     KC_PGDN, KC_PMNS,
        KC_CAPS,  KC_A,   	KC_S,    	KC_D,    	KC_F,    	KC_G,    	KC_H,    	KC_J,    	KC_K,     KC_L,    	KC_SCLN,  KC_QUOT, 	          KC_ENT,   KC_P7,    KC_P8,      KC_P9,   KC_PPLS,
        KC_LSFT,  KC_Z,     KC_X,    	KC_C,    	KC_V,    	KC_B,    	KC_N,    	KC_M,    	KC_COMM,  KC_DOT,  	KC_SLSH,             	        KC_RSFT,  KC_UP,    KC_P4,      KC_P5,   KC_P6,
        KC_LCTL,  KC_LGUI,  KC_LALT, 	KC_SPC,  	KC_RALT, 	MO(1),   	KC_APP,   	KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RIGHT, KC_P0, 	  KC_PDOT, 	KC_P1,  	KC_P2,    KC_P3,      KC_PENT,
        /* row 6 — G1-G5 play their macros, Rec records one; roller toggle, media keys */
                                                                    MC_G1,    MC_G2,    MC_G3,     MC_G4,   MC_G5,    MC_REC,   K580_ROLL,   KC_MPRV, KC_MPLY, KC_MNXT),
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

enum { MC_IDLE, MC_CHOOSE, MC_RECORD };
static uint8_t  macro_state = MC_IDLE;
static uint8_t  macro_slot; // G key being recorded, 0-4
static bool     macro_blink;
static uint16_t macro_blink_time;
/* The macro being recorded or played back: the count, then the events */
static uint8_t macro_buf[1 + 2 * MACRO_EVENTS];

static void *macro_addr(uint8_t n) {
    return (void *)(uint16_t)(EECONFIG_SIZE + (uint16_t)n * MACRO_SLOT_SIZE);
}

static void macro_set_state(uint8_t state) {
    macro_state      = state;
    macro_blink      = true;
    macro_blink_time = timer_read();
    led_fx_refresh();
}

static void macro_play(uint8_t n) {
    uint8_t *e = macro_buf + 1;
    uint8_t  count, i, j;

    eeprom_read_block(macro_buf, macro_addr(n), sizeof(macro_buf));
    count = macro_buf[0];
    if (count > MACRO_EVENTS) {
        return;
    }
    for (i = 0; i < count; i++) {
        if (e[2 * i + 1]) {
            register_code(e[2 * i]);
        } else {
            unregister_code(e[2 * i]);
        }
        wait_ms(MACRO_DELAY);
    }
    /* Release each key whose last event is a press */
    for (i = 0; i < count; i++) {
        if (!e[2 * i + 1]) {
            continue;
        }
        for (j = i + 1; j < count && e[2 * j] != e[2 * i]; j++) {
        }
        if (j == count) {
            unregister_code(e[2 * i]);
        }
    }
}

static void macro_stop(void) {
    eeprom_update_block(macro_buf, macro_addr(macro_slot), 1 + 2 * macro_buf[0]);
    macro_set_state(MC_IDLE);
}

/* G1-G5 (0-4) and Rec (MACRO_COUNT) pressed */
static void macro_key(uint8_t k) {
    switch (macro_state) {
    case MC_IDLE:
        if (k == MACRO_COUNT) {
            macro_set_state(MC_CHOOSE);
        } else {
            macro_play(k);
        }
        break;
    case MC_CHOOSE:
        if (k == MACRO_COUNT) {
            macro_set_state(MC_IDLE);
        } else {
            macro_slot   = k;
            macro_buf[0] = 0;
            macro_set_state(MC_RECORD);
        }
        break;
    case MC_RECORD:
        if (k == MACRO_COUNT) {
            macro_stop();
        }
        break;
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= MC_G1 && keycode <= MC_REC) {
        if (record->event.pressed) {
            macro_key(keycode - MC_G1);
        }
        return false;
    }
    if (macro_state == MC_CHOOSE && keycode == KC_ESC && record->event.pressed) {
        macro_set_state(MC_IDLE);
        return false;
    }
    if (macro_state == MC_RECORD && keycode > KC_TRANSPARENT && keycode <= 0xFF) {
        uint8_t n = macro_buf[0];

        macro_buf[1 + 2 * n] = keycode;
        macro_buf[2 + 2 * n] = record->event.pressed;
        macro_buf[0]         = n + 1;
        if (n + 1 == MACRO_EVENTS) {
            macro_stop();
        }
    }
    return true;
}

void housekeeping_task_user(void) {
    if (macro_state != MC_IDLE && timer_elapsed(macro_blink_time) >= MACRO_BLINK) {
        macro_blink      = !macro_blink;
        macro_blink_time = timer_read();
        led_fx_refresh();
    }
}

uint8_t led_fx_mono_val(uint8_t slot, uint8_t val) {
    if (slot == MACRO_LED_REC) {
        return (macro_state == MC_CHOOSE || (macro_state == MC_RECORD && macro_blink)) ? MACRO_LED_VAL : 0;
    }
    if (slot < MACRO_LED_G1 || slot > MACRO_LED_REC) {
        return val; // media keys
    }
    switch (macro_state) {
    case MC_CHOOSE:
        return macro_blink ? MACRO_LED_VAL : 0;
    case MC_RECORD:
        return slot == MACRO_LED_G1 + macro_slot * 8 ? MACRO_LED_VAL : 0;
    }
    return val;
}
