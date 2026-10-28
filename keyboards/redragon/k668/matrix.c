/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "matrix.h"
#include "led_pwm.h"

_Static_assert(MATRIX_ROWS == LED_PWM_COLUMNS, "one LED column per matrix row");

/* The PWM0 interrupt handler drives the matrix rows (the LED columns), so it also reads the keys. */
void matrix_init_custom(void) {
    led_pwm_init();
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool    changed = false;
    uint8_t row;

    for (row = 0; row < MATRIX_ROWS; row++) {
        matrix_row_t bits = led_pwm_keys[row];
        if (current_matrix[row] != bits) {
            current_matrix[row] = bits;
            changed             = true;
        }
    }
    return changed;
}
