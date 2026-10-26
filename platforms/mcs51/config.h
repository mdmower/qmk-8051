/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* usb_descriptor.c compares SERIAL_NUMBER_USE_HARDWARE_ID with TRUE. ChibiOS defines TRUE and FALSE; SDCC does not. */
#ifndef TRUE
#    define TRUE 1
#endif
#ifndef FALSE
#    define FALSE 0
#endif

/* The USB serial number is the chip ID unless the keyboard defines SERIAL_NUMBER, or defines this FALSE for no
 * serial number. */
#ifndef SERIAL_NUMBER_USE_HARDWARE_ID
#    define SERIAL_NUMBER_USE_HARDWARE_ID TRUE
#endif
