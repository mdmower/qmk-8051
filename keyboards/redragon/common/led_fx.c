/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "led_fx.h"
#include "timer.h"

/*
 * Colors are HSV. Hue runs 0-191 around the color wheel (6 sectors of 32); the hue setting has 24
 * steps of 8, so red, yellow, green, cyan, blue and magenta are every 4th step. Saturation 0 is
 * white at any hue.
 */
#define HUE_RANGE 192
#define HUE_STEPS 24

/* Modes before MODES_SIDE: all zones; from MODES_SIDE on: the key LEDs (zone 0) only */
enum {
    MODE_STATIC,
    MODE_RAINBOW,
    MODE_RAINBOW_REV,
    MODE_WAVE,
    MODE_WAVE_REV,
    MODE_CYCLE,
    MODE_BREATHE,
    MODES_SIDE,
    MODE_RAINBOW_ROWS = MODES_SIDE,
    MODE_WAVE_ROWS,
    MODE_CHEVRON,
    MODE_OUT_IN,
    MODE_RAINDROPS,
    MODE_REACTIVE,
    MODE_HEATMAP,
    MODES
};
#define F_POS 0x01     // hue changes with the LED position (led_pos)
#define F_REV 0x02     // ... in the other direction
#define F_MOVE 0x04    // hue changes with time
#define F_BREATHE 0x08 // brightness changes with time
#define F_KEYS 0x10    // color from key_level[]: pressed keys or raindrops; changes with time
#define F_ROWS 0x20    // with F_POS: the position is the key row, not led_pos
#define F_CHEVRON 0x40 // with F_POS: led_pos + distance from the middle row
#define F_DIST 0x80    // with F_POS: distance from the center of the keys

static __code const uint8_t mode_flags[MODES] = {
    0, F_POS, F_POS | F_REV, F_POS | F_MOVE, F_POS | F_REV | F_MOVE, F_MOVE, F_BREATHE,
    F_POS | F_ROWS, F_POS | F_ROWS | F_MOVE, F_POS | F_CHEVRON | F_MOVE, F_POS | F_DIST | F_REV | F_MOVE,
    F_KEYS, F_KEYS, F_KEYS,
};
/* Steps of each setting, by zone: mode, hue, sat, val, speed, on */
static __code const uint8_t setting_steps[LED_FX_ZONES * LED_FX_SETTINGS] = {
    MODES, HUE_STEPS, 5, 8, 5, 2,
#if LED_FX_ZONES > 1
    MODES_SIDE, HUE_STEPS, 5, 8, 5, 2,
#endif
};
static __code const uint8_t sat_table[5]        = {0, 96, 160, 208, 255};
static __code const uint8_t val_table[8]        = {8, 16, 32, 64, 96, 144, 200, 255};
/* Phase advance in 1/131072 of a cycle per ms: one cycle takes 26, 13, 6.5, 3.3 or 1.6 s */
static __code const uint8_t  speed_table[5]     = {5, 10, 20, 40, 80};
/* Time between the starts of two passes of an animated effect, in ms */
#define FX_INTERVAL 16
/* Longest time a pass advances the phase by, in ms, so that the advance fits 16 bits */
#define FX_MAX_ELAPSED 250

/* led_pos[]: the keyboard's LED positions (led_fx.h) */
#include "led_fx_pos.h"

/* Settings, indexed by LED_FX_MODE...LED_FX_ON; zone 0 = keys, zone 1 = side lights */
static __xdata uint8_t settings[LED_FX_ZONES][LED_FX_SETTINGS] = {
    {MODE_WAVE, 0, 4, 4, 2, 1},
#if LED_FX_ZONES > 1
    {MODE_RAINBOW, 0, 4, 4, 2, 1},
#endif
};
static __xdata uint16_t phase[LED_FX_ZONES];

/* Per-zone values for the current pass */
typedef struct {
    uint8_t mode;
    uint8_t flags;
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    uint8_t step; // phase advance of this pass in 1/256 of a cycle (whole units)
} zone_t;
static __xdata zone_t zone[LED_FX_ZONES];

_Static_assert(LED_FX_ROWS <= 8, "LED_FX_ROWS: at most 8");
_Static_assert(LED_FX_COLUMNS * LED_FX_ROWS <= 256, "LED slots: at most 256");

/* Per key LED, by slot: brightness (reactive), heat (heatmap) or hue offset (raindrops). Cleared when the key LEDs'
 * mode changes. */
static __xdata uint8_t key_level[LED_FX_COLUMNS * LED_FX_ROWS];
#define HEAT_STEP 32 // heat added per key press

/* By key row (KEY_ROW): position for F_ROWS (3 * y), and distance from the middle of the 6 key rows for
 * F_CHEVRON and F_DIST, with y = 12 position units per row (a key is about 8.5 units wide). X_MID: the middle of
 * led_pos. */
static __code const uint8_t row_pos[8]  = {0, 36, 72, 108, 144, 180, 0, 0};
static __code const uint8_t row_dist[8] = {30, 18, 6, 6, 18, 30, 0, 0};
/* Key row of row n of the column being rendered: led_row[] with LED_FX_KEY_ROWS, else n */
#ifdef LED_FX_KEY_ROWS
#    define KEY_ROW(n) (rows[n])
#else
#    define KEY_ROW(n) (n)
#endif
#define X_MID (HUE_RANGE / 2)

/* 16-bit linear congruential generator, for raindrops */
static __xdata uint16_t rnd;

static __xdata uint8_t  fx_column = LED_FX_COLUMNS; // next column to render; LED_FX_COLUMNS: no pass running
static __xdata uint16_t fx_time;                    // timer_read() at the start of the last pass
static __bit            fx_valid;                   // the last pass used the current settings
static __bit            fx_animated;                // a zone changes with time

/* a * b / 255, rounded down; b = 255 gives a */
#define SCALE8(a, b) ((uint8_t)(((uint16_t)(uint8_t)(a) * (uint8_t)(b) + (uint8_t)(a)) >> 8))

/* hsv_to_rgb input and output */
static __data uint8_t hsv_h, hsv_s, hsv_v;
static __data uint8_t rgb[3];

/* Per hue sector: which of v, p, q, t (hsv_c[0..3]) are red, green and blue */
static __code const uint8_t hsv_order[6][3] = {{0, 3, 1}, {2, 0, 1}, {1, 0, 3}, {1, 2, 0}, {3, 1, 0}, {0, 1, 2}};
static __data uint8_t       hsv_c[4];

static void hsv_to_rgb(void) {
    uint8_t               rem = (hsv_h & 31) << 3;
    __code const uint8_t *o   = hsv_order[hsv_h >> 5];

    hsv_c[0] = hsv_v;
    hsv_c[1] = SCALE8(hsv_v, 255 - hsv_s);
    hsv_c[2] = SCALE8(hsv_v, 255 - SCALE8(hsv_s, rem));
    hsv_c[3] = SCALE8(hsv_v, 255 - SCALE8(hsv_s, 255 - rem));
    rgb[0]   = hsv_c[o[0]];
    rgb[1]   = hsv_c[o[1]];
    rgb[2]   = hsv_c[o[2]];
}

/* hsv_h = (hsv_h + b) mod HUE_RANGE, for b <= HUE_RANGE */
static void hue_add(uint8_t b) {
    uint8_t a = hsv_h;

    hsv_h += b;
    if (hsv_h < a || hsv_h >= HUE_RANGE) {
        hsv_h -= HUE_RANGE;
    }
}

static uint8_t rand8(void) {
    rnd = rnd * 25173 + 13849;
    return rnd >> 8;
}

/* Sets zone[] from the settings; the phase advances by the time since the last pass. */
static void start_pass(void) {
    __xdata uint8_t  *set     = settings[0];
    __xdata uint16_t *ph      = phase;
    __xdata zone_t   *zs      = zone;
    uint16_t          elapsed = timer_elapsed(fx_time);
    uint8_t           z;

    fx_time += elapsed;
    if (elapsed > FX_MAX_ELAPSED) {
        elapsed = FX_MAX_ELAPSED;
    }
    fx_animated = 0;
    for (z = 0; z < LED_FX_ZONES; z++, set += LED_FX_SETTINGS, ph++, zs++) {
        uint8_t flags = mode_flags[set[LED_FX_MODE]];
        uint8_t p     = *ph >> 8;
        uint8_t v     = 0;

        *ph += (elapsed * speed_table[set[LED_FX_SPEED]]) >> 1;
        zs->step = (uint8_t)(*ph >> 8) - p;
        p        = *ph >> 8;
        hsv_h    = set[LED_FX_HUE] << 3;
        if (set[LED_FX_ON]) {
            v = val_table[set[LED_FX_VAL]];
            if (flags & (F_MOVE | F_BREATHE | F_KEYS)) {
                fx_animated = 1;
            }
        }
        if (flags & F_MOVE) {
            hue_add(HUE_RANGE - ((p >> 1) + (p >> 2))); // hue - p * 3/4
        }
        if (flags & F_BREATHE) {
            uint8_t t = p << 1; // triangle wave 0-254-1
            if (p & 0x80) {
                t = ~t;
            }
            v = SCALE8(v, SCALE8(t, t));
        }
        zs->mode  = set[LED_FX_MODE];
        zs->flags = flags;
        zs->hue   = hsv_h;
        zs->sat   = sat_table[set[LED_FX_SAT]];
        zs->val   = v;
    }
    /* raindrops: at most one per pass, on a random key LED, with a hue up to 1/3 of the wheel from the zone's */
    if (zone[0].mode == MODE_RAINDROPS && zone[0].step) {
        uint8_t slot = rand8();
        if (slot < sizeof(key_level)) {
            key_level[slot] = rand8() & 0x3F;
        }
    }
}

/* Sets the colors of the LEDs of column c (with 2 zones, the last row = side lights, zone 1) */
static void render_column(uint8_t c) {
    uint8_t               slot = c * LED_FX_ROWS;
    __code const uint8_t *pos  = led_pos + slot;
#ifdef LED_FX_KEY_ROWS
    __code const uint8_t *rows = led_row + slot;
#endif
    __xdata zone_t       *zs   = zone;
    __xdata uint8_t      *kp   = key_level + slot;
    uint8_t               n;

    for (n = 0; n < LED_FX_ROWS; n++, slot++, kp++) {
        uint8_t x = *pos++;

#if LED_FX_ZONES > 1
        if (n == LED_FX_ROWS - 1) {
            zs++;
        }
#endif
#ifdef LED_FX_SKIP_ENABLE
        if (x == LED_FX_SKIP) {
            continue;
        }
#endif
        if (x == LED_FX_MONO) {
            /* single-color LED: lit at the zone's brightness whatever the hue */
#ifdef LED_FX_MONO_HOOK
            rgb[0] = led_fx_mono_val(slot, zs->val);
#else
            rgb[0] = zs->val;
#endif
            rgb[1] = 0;
            rgb[2] = 0;
        } else {
            uint8_t flags = zs->flags;

            hsv_h = zs->hue;
            hsv_s = zs->sat;
            hsv_v = zs->val;
            if (flags & F_KEYS) {
                uint8_t k = *kp;

                if (zs->mode == MODE_RAINDROPS) {
                    hue_add(k);
                } else {
                    uint8_t d = zs->step, b;

                    if (zs->mode == MODE_REACTIVE) {
                        /* lit at a press (led_fx_key_pressed), fades out */
                        b = k;
                        d <<= 2;
                    } else {
                        /* heatmap: dark at heat 0, blue at 85, then toward red at 255 */
                        uint8_t h = k > 85 ? k - 85 : 0;

                        hsv_h = 128 - (h >> 1) - (h >> 2);
                        b     = k < 85 ? k * 3 : 0xFF;
                    }
                    hsv_v = SCALE8(hsv_v, b);
                    *kp   = k > d ? k - d : 0;
                }
            } else if (flags & F_POS) {
                if (flags & F_ROWS) {
                    x = row_pos[KEY_ROW(n)];
                } else if (flags & (F_CHEVRON | F_DIST)) {
                    if (flags & F_DIST) {
                        x = x < X_MID ? X_MID - x : x - X_MID;
                    }
                    x += row_dist[KEY_ROW(n)];
                    if (x >= HUE_RANGE) {
                        x -= HUE_RANGE;
                    }
                }
                hue_add((flags & F_REV) ? HUE_RANGE - x : x);
            }
            hsv_to_rgb();
        }
        led_fx_set_color(slot, rgb[0], rgb[1], rgb[2]);
    }
}

void led_fx_load(const uint8_t *p) {
    __xdata uint8_t *v = &settings[0][0];
    uint8_t          i;

    for (i = 0; i < sizeof(settings); i++) {
        if (p[i] < setting_steps[i]) {
            v[i] = p[i];
        }
    }
    fx_valid = 0;
}

void led_fx_save(__xdata uint8_t *dst) {
    __xdata uint8_t *v = &settings[0][0];
    uint8_t          i;

    for (i = 0; i < sizeof(settings); i++) {
        dst[i] = v[i];
    }
}

#ifdef LED_FX_MONO_HOOK
void led_fx_refresh(void) {
    fx_valid = 0;
}
#endif

void led_fx_task(void) {
    if (fx_column >= LED_FX_COLUMNS) {
        if (fx_valid && (!fx_animated || timer_elapsed(fx_time) < FX_INTERVAL)) {
            return;
        }
        fx_valid  = 1;
        fx_column = 0;
        start_pass();
    }
    render_column(fx_column++);
}

void led_fx_key_pressed(uint8_t slot) {
    __xdata uint8_t *k = key_level + slot;

    if (slot >= sizeof(key_level)) {
        return;
    }
    if (settings[0][LED_FX_MODE] == MODE_REACTIVE) {
        *k = 0xFF;
    } else if (settings[0][LED_FX_MODE] == MODE_HEATMAP) {
        *k = *k < 0xFF - HEAT_STEP ? *k + HEAT_STEP : 0xFF;
    }
}

void led_fx_adjust(uint8_t action, bool reverse) {
    __xdata uint8_t *v = &settings[0][0] + action;
    uint8_t          s = action < LED_FX_SETTINGS ? action : action - LED_FX_SETTINGS;

    if (s == LED_FX_ON) {
        *v = !*v;
    } else {
        uint8_t last = setting_steps[action] - 1;
        if (reverse) {
            if (*v) {
                (*v)--;
            } else if (s <= LED_FX_HUE) {
                *v = last; // mode and hue wrap around
            }
        } else if (*v < last) {
            (*v)++;
        } else if (s <= LED_FX_HUE) {
            *v = 0;
        }
    }
    if (action == LED_FX_MODE) {
        for (s = 0; s < sizeof(key_level); s++) {
            key_level[s] = 0;
        }
    }
    fx_valid = 0;
}
