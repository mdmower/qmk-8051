/*
Copyright 2011 Jun Wako <wakojun@gmail.com>

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

#include "timer.h"
#include "mcs51_isr.h"

// counter resolution 1ms
volatile uint32_t timer_count;

/** \brief timer initialization */
void timer_init(void) {
    // A 1 ms interrupt from the MCU's timer (mcs51_mcu.h)
    MCS51_TIMER_INIT();

    EA = 1; // global interrupt enable
}

/** \brief timer clear */
inline void timer_clear(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        timer_count = 0;
    }
}

/** \brief timer read */
inline uint16_t timer_read(void) {
    uint32_t t;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        t = timer_count;
    }

    return (t & 0xFFFF);
}

/** \brief timer read32 */
inline uint32_t timer_read32(void) {
    uint32_t t;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        t = timer_count;
    }

    return t;
}

// Once per ms
#pragma save
#pragma nooverlay
MCS51_ISR(timer_isr, INT_NO_TIMER) {
    MCS51_TIMER_RELOAD();
    timer_count++;
}
#pragma restore
