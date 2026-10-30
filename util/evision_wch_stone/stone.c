/* Stepping stone from the Evision bootloader to the WCH bootloader of the CH555.
 *
 * evision_wch_switch.py flashes it through the Evision bootloader, in place of the stock firmware. Its purpose is
 * to call the WCH bootloader at 0xF400 with P1.5 low; P1.5 low keeps that bootloader in USB mode (4348:55e0)
 * without a timeout. It does so at every start, until the WCH bootloader has erased the flash. evision_oem_image.py
 * adds the Esc check of the QMK images in front of it, so that Esc held at plug-in starts the Evision bootloader
 * instead.
 */
#include <stdint.h>
#include "ch555.h"

/* No global variables: they would make SDCC's start-up code write P2 (a row of the LED controller). */

void main(void) {
    uint8_t  i;
    uint16_t j;

    EA       = 0;
    LED_CTRL = 0;
    USB_CTRL = 0;

    /* COM lines on P0 and P3 push-pull high: a low COM switches on the external PMOS of its LED column, and with
     * P1.5 low the LEDs of that column on the P1.5 row would conduct. The latches are 1 from the reset, so the mode
     * change drives no pin low. P7.0 and P7.1 (COMs on some boards) stay inputs with pull-up, as after a reset. */
    P0        = 0xFF;
    P3        = 0xFF;
    P0_MOD_OC = 0;
    P0_DIR_PU = 0xFF;
    P3_MOD_OC = 0;
    P3_DIR_PU = 0xFF;

    /* USB off for at least 100 ms at any system clock (about 0.7 s at 12 MHz, the clock after a reset), so that
     * the host sees the device leave the bus */
    for (i = 0; i < 10; i++)
        for (j = 0; j < 62500; j++)
            __asm__("nop");

    P1_MOD_OC &= (uint8_t)~(1 << 5);
    P1_DIR_PU |= 1 << 5;
    P1_5 = 0;
    __asm__("lcall #0xF400");

    while (1)
        ;
}
