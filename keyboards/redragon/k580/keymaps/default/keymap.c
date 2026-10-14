#include QMK_KEYBOARD_H
#include "led_dma.h"
#include "led_fx.h"
#include "flash_store.h"

// Custom keycodes: Win Lock; roller toggle key and roller turns; lighting settings, keys then side lights in led_fx.h order
enum {
  U_WINLK = SAFE_RANGE,
  U_ROLL, U_RLDN, U_RLUP,
  U_KMODE, U_KHUE, U_KSAT, U_KVAL, U_KSPD, U_KTOG,
  U_SMODE, U_SHUE, U_SSAT, U_SVAL, U_SSPD, U_STOG
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
//COM is Row, KeyPu is Column
	[0] = LAYOUT_default(
		/*COM16*/	  /*COM17*/	  /*COM18*/	/*COM19*/	/*COM20*/	/*COM21*/	/*COM22*/	/*COM23*/	/*COM24*/	/*COM25*/ /*COM25*/	/*COM27*/	/*COM28*/	/*COM29*/	/*COM30*/	/*COM31*/   /*COM32*/	/*COM33*/
        KC_ESC,   KC_F1,   	KC_F2,   	KC_F3,   	KC_F4,   	KC_F5,   	KC_F6,   	KC_F7,   	KC_F8,    KC_F9,    KC_F10,   KC_F11,  	KC_F12,  	KC_PSCR, 	KC_SCRL, 	KC_PAUS,    KC_NUM,  KC_PSLS,
        KC_GRV,   KC_1,    	KC_2,    	KC_3,    	KC_4,    	KC_5,    	KC_6,    	KC_7,    	KC_8,     KC_9,    	KC_0,     KC_MINS, 	KC_EQL,  	KC_BSPC, 	KC_INS,  	KC_HOME,    KC_PGUP, KC_PAST,
        KC_TAB,   KC_Q,    	KC_W,    	KC_E,    	KC_R,    	KC_T,    	KC_Y,    	KC_U,    	KC_I,     KC_O,    	KC_P,     KC_LBRC, 	KC_RBRC, 	KC_BSLS, 	KC_DEL,  	KC_END,     KC_PGDN, KC_PMNS,
        KC_CAPS,  KC_A,   	KC_S,    	KC_D,    	KC_F,    	KC_G,    	KC_H,    	KC_J,    	KC_K,     KC_L,    	KC_SCLN,  KC_QUOT, 	          KC_ENT,   KC_P7,    KC_P8,      KC_P9,   KC_PPLS,
        KC_LSFT,  KC_Z,     KC_X,    	KC_C,    	KC_V,    	KC_B,    	KC_N,    	KC_M,    	KC_COMM,  KC_DOT,  	KC_SLSH,             	        KC_RSFT,  KC_UP,    KC_P4,      KC_P5,   KC_P6,
        KC_LCTL,  KC_LGUI,  KC_LALT, 	KC_SPC,  	KC_RALT, 	MO(1),   	KC_APP,   	KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RIGHT, KC_P0, 	  KC_PDOT, 	KC_P1,  	KC_P2,    KC_P3,      KC_PENT,
        /* row 6 — G1-G5 and Rec send F13-F18 (bind them in the OS), roller toggle, media keys */
                                                                    KC_F13,   KC_F14,   KC_F15,    KC_F16,  KC_F17,   KC_F18,   U_ROLL,   KC_MPRV, KC_MPLY, KC_MNXT),
	/* Fn layer: Fn+Esc = bootloader, Fn+F1-F12 = the functions printed on the keycaps (media player,
	 * volume down/up, mute, stop, previous, play/pause, next, mail, browser home, calculator, search),
	 * Fn+Win = Win Lock. Lighting (Shift reverses): Ins/G1 mode, Home/G2 hue, PgUp/G3 saturation,
	 * Del/G4 brightness, End/G5 speed, PgDn/Rec on/off; Ins-PgDn for the keys, G1-Rec for the side
	 * lights. */
	[1] = LAYOUT_default(
        QK_BOOT, KC_MSEL, KC_VOLD, KC_VOLU, KC_MUTE, KC_MSTP, KC_MPRV, KC_MPLY, KC_MNXT, KC_MAIL, KC_WHOM, KC_CALC, KC_WSCH, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, U_KMODE, U_KHUE,  U_KSAT,  _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, U_KVAL,  U_KSPD,  U_KTOG,  _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, U_WINLK, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        U_SMODE, U_SHUE,  U_SSAT,  U_SVAL,  U_SSPD,  U_STOG,  _______, _______, _______, _______),
};

/* Win Lock: while it is on, LGUI/RGUI are blocked and the Win Lock LED is lit. */
static bool win_lock = false;
/* The roller toggle key switches the roller between volume and key LED brightness. */
static bool roller_light = false;

/* Settings saved in the data flash SAVE_DELAY after the last change: the lighting settings (led_fx.c),
 * then the roller mode (1 = key LED brightness). */
#define SAVE_DELAY 3000 // ms
static uint16_t save_time;
static bool     save_pending = false;

static void settings_changed(void) {
    save_pending = true;
    save_time    = timer_read();
}

static void settings_task(void) {
    __xdata uint8_t rec[LED_FX_SAVE_SIZE + 1];

    if (save_pending && timer_elapsed(save_time) >= SAVE_DELAY) {
        save_pending = false;
        led_fx_save(rec);
        rec[LED_FX_SAVE_SIZE] = roller_light;
        flash_store_save(rec, sizeof(rec));
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= U_KMODE && keycode <= U_STOG) {
        if (record->event.pressed) {
            led_fx_adjust(keycode - U_KMODE, get_mods() & MOD_MASK_SHIFT);
            settings_changed();
        }
        return false;
    }
    switch (keycode) {
    case U_ROLL:
        if (record->event.pressed) {
            roller_light = !roller_light;
            settings_changed();
        }
        return false;
    case U_RLDN:
    case U_RLUP:
        if (roller_light) {
            if (record->event.pressed) {
                led_fx_adjust(LED_FX_VAL, keycode == U_RLDN);
                settings_changed();
            }
        } else if (record->event.pressed) {
            register_code(keycode == U_RLDN ? KC_VOLD : KC_VOLU);
        } else {
            unregister_code(keycode == U_RLDN ? KC_VOLD : KC_VOLU);
        }
        return false;
    case U_WINLK:
        if (record->event.pressed) {
            win_lock = !win_lock;
            gpio_write_pin(LED_WIN_LOCK_PIN, win_lock);
        }
        return false;
    case KC_LGUI:
    case KC_RGUI:
        return !win_lock;
    }
    return true;
}

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = { ENCODER_CCW_CW(U_RLDN, U_RLUP) }, // volume or key LED brightness (U_ROLL)
    [1] = { ENCODER_CCW_CW(_______, _______) },
};
#endif

void keyboard_post_init_user(void) {
    /* P5.4 enables the indicator LEDs, active low (stock 1.04 @0xdc4b) */
    gpio_set_pin_output(PORT5_4);
    gpio_write_pin_low(PORT5_4);
    gpio_set_pin_output(LED_WIN_LOCK_PIN);
    gpio_write_pin_low(LED_WIN_LOCK_PIN);

    __code const uint8_t *p = flash_store_find();
    if (p) {
        led_fx_load(p);
        roller_light = p[LED_FX_SAVE_SIZE] == 1;
    }
}
/* Indicator LEDs and key LEDs off while the host is suspended (stock 1.04 @0x794f) */
void suspend_power_down_user(void) {
    gpio_write_pin_high(PORT5_4);
    led_dma_output(false);
}
void suspend_wakeup_init_user(void) {
    gpio_write_pin_low(PORT5_4);
    led_dma_output(true);
}
void housekeeping_task_user(void) {
    led_fx_task();
    settings_task();
}
