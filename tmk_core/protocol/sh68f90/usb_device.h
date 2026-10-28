/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/* USB device of the SH68F90A: control transfers on EP0, the report endpoints, suspend and remote wake-up. */

#include <stdbool.h>
#include <stdint.h>
#include "sh68f90a.h"
#include "report.h"
#include "usb_descriptor.h"

/* Endpoint buffers in USB RAM (datasheet 8.4.1). EP0 receives SETUP and OUT data packets in the same buffer. */
#define USB_EP0_OUT_BUF ((__xdata volatile uint8_t *)0x1100)
#define USB_EP0_IN_BUF ((__xdata volatile uint8_t *)0x1108)
#define USB_EP1_IN_BUF ((__xdata volatile uint8_t *)0x1120)
#define USB_EP2_OUT_BUF ((__xdata volatile uint8_t *)0x1140)
#define USB_EP2_IN_BUF ((__xdata volatile uint8_t *)0x1180)

/* Packet sizes of the endpoints (datasheet 8.4.1) */
#define USB_EP0_SIZE 8
#define USB_EP1_SIZE 16
#define USB_EP2_SIZE 64

/* Set by the USB interrupt, read by the main loop */
extern volatile bool    usb_configured;       /* SET_CONFIGURATION with a nonzero value */
extern volatile bool    usb_suspended;        /* SUSPIF until RESMIF or a bus reset */
extern volatile bool    usb_remote_wakeup;    /* the host enabled remote wake-up */
extern volatile bool    usb_wakeup_sent;      /* a resume signal was sent and the bus is not active yet */
extern volatile uint8_t usb_keyboard_leds;    /* the last LED output report */
extern volatile uint8_t usb_keyboard_protocol; /* of the keyboard interface: 0 boot, 1 report */
extern volatile uint8_t usb_keyboard_idle;    /* SET_IDLE duration of the keyboard interface, in 4 ms units */
#ifdef RAW_ENABLE
extern volatile bool usb_raw_out_waiting; /* EP2 holds an OUT packet that the main loop has not read */
#endif
#ifdef BOOTLOADER_SINOWEALTH_ISP
extern volatile bool usb_isp_requested; /* feature report REPORT_ID_SINOWEALTH_ISP with the bytes 05 75 */
#endif

/* The current report of the keyboard (sh68f90.c): report ID, then the 8 bytes of the boot report. GET_REPORT
 * answers it. */
extern uint8_t kbd_report[KEYBOARD_REPORT_SIZE];

void usb_device_init(void);
void usb_send_resume(void);

#pragma save
#pragma nooverlay
void usb_isr(void) __interrupt(INT_NO_USB);
#pragma restore
