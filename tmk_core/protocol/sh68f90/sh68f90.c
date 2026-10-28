/* SPDX-License-Identifier: GPL-2.0-or-later */

/* QMK host driver and protocol hooks for the SH68F90A USB device (usb_device.c). The keyboard, extra keys and the
 * other shared reports go to EP1 IN (16-byte packets), raw HID uses EP2 IN and OUT (64-byte packets). */

#include <string.h>
#include "report.h"
#include "host.h"
#include "host_driver.h"
#include "keyboard.h"
#include "action.h"
#include "suspend.h"
#include "timer.h"
#include "wait.h"
#include "usb_device_state.h"
#include "usb_descriptor.h"
#include "usb_device.h"
#ifdef BOOTLOADER_SINOWEALTH_ISP
#    include "bootloader.h"
#endif
#ifdef RAW_ENABLE
#    include "raw_hid.h"
#endif

#ifndef KEYBOARD_SHARED_EP
#    error "The SH68F90 protocol needs KEYBOARD_SHARED_EP: the keyboard shares EP1 with the other reports"
#endif
#if defined(CONSOLE_ENABLE) || defined(MIDI_ENABLE) || defined(VIRTSER_ENABLE) || defined(PLOVER_HID_ENABLE)
#    error "CONSOLE_ENABLE, MIDI_ENABLE, VIRTSER_ENABLE and PLOVER_HID_ENABLE are not implemented in the SH68F90 protocol"
#endif
#if (defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)) || (defined(JOYSTICK_ENABLE) && !defined(JOYSTICK_SHARED_EP)) || (defined(DIGITIZER_ENABLE) && !defined(DIGITIZER_SHARED_EP))
#    error "The SH68F90 protocol sends mouse, joystick and digitizer reports only on the shared endpoint"
#endif
#if defined(NKRO_ENABLE) && (NKRO_REPORT_BITS + 2) > SHARED_EPSIZE
#    error "The NKRO report does not fit the 16-byte packets of the shared endpoint"
#endif
#if defined(BOOTLOADER_SINOWEALTH_ISP) && defined(PROGRAMMABLE_BUTTON_ENABLE)
#    error "REPORT_ID_PROGRAMMABLE_BUTTON and REPORT_ID_SINOWEALTH_ISP are the same report ID"
#endif

/* The current keyboard report: the report ID, then the 8 bytes of the boot report. It is the last report loaded
 * into EP1, or one kept until EP1 is free; GET_REPORT (USB interrupt) answers it, and with a nonzero SET_IDLE
 * duration it is sent again when that time has passed since the last report (HID 1.11 7.2.4). */
uint8_t         kbd_report[KEYBOARD_REPORT_SIZE];
static bool     kbd_report_pending = false;
static uint16_t kbd_report_time;

#ifdef EXTRAKEY_ENABLE
/* Extra key reports that found EP1 busy, the latest one per report ID: [0] REPORT_ID_SYSTEM, [1] REPORT_ID_CONSUMER */
static report_extra_t extra_pending[2];
static bool           extra_pending_set[2] = {false, false};
#endif

static uint8_t keyboard_leds(void);
static void    send_keyboard(report_keyboard_t *report);
static void    send_nkro(report_nkro_t *report);
static void    send_mouse(report_mouse_t *report);
static void    send_extra(report_extra_t *report);
#ifdef RAW_ENABLE
static void send_raw_hid(uint8_t *data, uint8_t length) __reentrant;
#endif

static host_driver_t sh68f90_driver = {
    .keyboard_leds = keyboard_leds,
    .send_keyboard = send_keyboard,
    .send_nkro     = send_nkro,
    .send_mouse    = send_mouse,
    .send_extra    = send_extra,
#ifdef RAW_ENABLE
    .send_raw_hid = send_raw_hid,
#endif
};

/*******************************************************************************
 * Endpoints
 ******************************************************************************/
/* Remote wake-up, if the host enabled it and the bus is suspended: one resume signal per suspend. Until the host
 * resumes the bus (usb_wakeup_sent), later reports wait without signaling again. */
static void wake_host(void) {
    if (usb_suspended && usb_remote_wakeup && !usb_wakeup_sent) {
        usb_send_resume();
    }
}

/* IEPnRDY stays set until the host has read the packet in the endpoint's buffer (datasheet 8.4.4). */

/* Waits up to about 10 ms, as LUFA does, for the host to read the packet that EP1 holds, if the device is configured
 * and the bus is active. Without the wait, reports that come faster than the host polls replace each other in the
 * kept report, and a press with its release can be lost. */
static void wait_ep1(void) {
    uint8_t timeout = 255;
    if (!usb_configured || usb_suspended || usb_wakeup_sent) {
        return;
    }
    while (timeout-- && (EP1CON & bIEP_RDY)) {
        wait_us(40);
    }
}

/* Loads a report into EP1. Returns false if the endpoint still holds the previous report or is halted. */
static bool load_ep1(const uint8_t *report, uint8_t size) {
    uint8_t i;

    if (EP1CON & (bIEP_RDY | bIEP_STL)) {
        return false;
    }
    for (i = 0; i < size; i++) {
        USB_EP1_IN_BUF[i] = report[i];
    }
    IEP1CNT = size;
    EP1CON |= bIEP_RDY;
    return true;
}

/* Shared endpoint. Does not wait: returns false if the endpoint still holds the previous report or is halted, or if
 * the device is not configured. */
static bool send_ep1(const void *report, uint8_t size) {
    if (!usb_configured) {
        return false;
    }
    wake_host();
    return load_ep1(report, size);
}

/*******************************************************************************
 * Raw HID
 ******************************************************************************/
#ifdef RAW_ENABLE
/* Called through the host driver (raw_hid_send()): reentrant, so that SDCC passes the arguments on the stack.
 * Waits up to about 10 ms for EP2 to be free and drops the report if it is not. */
static void send_raw_hid(uint8_t *data, uint8_t length) __reentrant {
    uint8_t timeout = 255;
    uint8_t i;

    if (length != RAW_EPSIZE || !usb_configured) {
        return;
    }
    wake_host();
    while (timeout-- && (EP2CON & bIEP_RDY)) {
        wait_us(40);
    }
    if (EP2CON & (bIEP_RDY | bIEP_STL)) {
        return;
    }
    for (i = 0; i < RAW_EPSIZE; i++) {
        USB_EP2_IN_BUF[i] = data[i];
    }
    IEP2CNT = RAW_EPSIZE;
    EP2CON |= bIEP_RDY;
}

/* Called from the main loop. A packet of another length than RAW_EPSIZE is dropped. EP2 answers NAK from the
 * packet's arrival until OEP2RDY is set here. */
void raw_hid_task(void) {
    uint8_t data[RAW_EPSIZE];
    uint8_t i;
    bool    received;

    if (!usb_raw_out_waiting) {
        return;
    }
    received = OEP2CNT == RAW_EPSIZE;
    if (received) {
        for (i = 0; i < RAW_EPSIZE; i++) {
            data[i] = USB_EP2_OUT_BUF[i];
        }
    }
    usb_raw_out_waiting = false;
    EP2CON |= bOEP_RDY;
    if (received) {
        raw_hid_receive(data, RAW_EPSIZE);
    }
}
#endif

/*******************************************************************************
 * Host driver
 ******************************************************************************/
/* The LED report of the last SET_REPORT (USB interrupt); QMK core's led_task() calls led_set() when it changes. */
static uint8_t keyboard_leds(void) {
    return usb_keyboard_leds;
}

/* Sends kbd_report on EP1 (without the report ID in the boot protocol), or keeps it for protocol_post_task() if EP1
 * still holds the previous report. */
static void send_kbd_report(void) {
    if (usb_keyboard_protocol == 0) {
        kbd_report_pending = !send_ep1(&kbd_report[1], KEYBOARD_REPORT_SIZE - 1);
    } else {
        kbd_report_pending = !send_ep1(kbd_report, KEYBOARD_REPORT_SIZE);
    }
    if (!kbd_report_pending) {
        kbd_report_time = timer_read();
    }
}

static void send_keyboard(report_keyboard_t *report) {
    memcpy(kbd_report, report, KEYBOARD_REPORT_SIZE);
    wait_ep1();
    send_kbd_report();
}

static void send_nkro(report_nkro_t *report) {
#ifdef NKRO_ENABLE
    wait_ep1();
    send_ep1(report, sizeof(report_nkro_t));
#endif
}

static void send_mouse(report_mouse_t *report) {
#ifdef MOUSE_ENABLE
    wait_ep1();
    send_ep1(report, sizeof(report_mouse_t));
#endif
}

static void send_extra(report_extra_t *report) {
#ifdef EXTRAKEY_ENABLE
    uint8_t i = (report->report_id == REPORT_ID_CONSUMER) ? 1 : 0;
    wait_ep1();
    extra_pending_set[i] = !send_ep1(report, sizeof(report_extra_t));
    if (extra_pending_set[i]) {
        memcpy(&extra_pending[i], report, sizeof(report_extra_t));
    }
#endif
}

void send_joystick(report_joystick_t *report) {
#ifdef JOYSTICK_ENABLE
    wait_ep1();
    send_ep1(report, sizeof(report_joystick_t));
#endif
}

void send_programmable_button(report_programmable_button_t *report) {
#ifdef PROGRAMMABLE_BUTTON_ENABLE
    wait_ep1();
    send_ep1(report, sizeof(report_programmable_button_t));
#endif
}

void send_digitizer(report_digitizer_t *report) {
#ifdef DIGITIZER_ENABLE
    wait_ep1();
    send_ep1(report, sizeof(report_digitizer_t));
#endif
}

/*******************************************************************************
 * main
 ******************************************************************************/
void protocol_setup(void) {
    usb_device_state_init();
}

void protocol_pre_init(void) {
    usb_device_init();
}

/* Until SET_CONFIGURATION the main loop does not wait for the host: the latest keyboard report and the latest extra
 * key report per report ID are kept, the other reports are dropped. */
void protocol_post_init(void) {
    host_set_driver(&sh68f90_driver);
}

#ifndef USB_RESUME_DELAY_MS
/* Bus active this long before the wakeup hooks run; the bus switches between suspend and active several times while
 * a PC resumes from standby. */
#    define USB_RESUME_DELAY_MS 500
#endif

void protocol_pre_task(void) {
    /* The watchdog restarts its count when RSTSTAT is read or written (datasheet 8.8); its period after a reset is
     * 1024 ms. */
    (void)RSTSTAT;

#ifdef BOOTLOADER_SINOWEALTH_ISP
    /* Called here, not in the USB interrupt: a jump out of an interrupt handler, without RETI, leaves the interrupt
     * in service, and interrupts of the same and lower priority stay blocked. */
    if (usb_isp_requested) {
        bootloader_jump();
    }
#endif

#if !defined(NO_USB_STARTUP_CHECK)
    /* Run the QMK suspend hooks when the bus enters or leaves suspend. Unlike LUFA and ChibiOS this does not wait in
     * a loop: the matrix is still scanned while suspended, and a key press wakes the host through wake_host() (if
     * the host enabled remote wake-up). After that signal the bus counts as suspended until the host resumes it. */
    static bool     suspended = false;
    static uint16_t suspend_time;

    if (usb_suspended || usb_wakeup_sent) {
        suspend_time = timer_read();
        if (!suspended) {
            suspended = true;
            suspend_power_down();
        }
    } else if (suspended && timer_elapsed(suspend_time) >= USB_RESUME_DELAY_MS) {
        suspended = false;
        suspend_wakeup_init();
    }
#endif
}

void protocol_post_task(void) {
    uint8_t idle;
#ifdef EXTRAKEY_ENABLE
    uint8_t i;
#endif

    /* SET_PROTOCOL (USB interrupt) takes effect here. QMK core builds boot reports in the boot protocol and, with
     * NKRO, NKRO reports in the report protocol; clear_keyboard() empties the report and sends it in the new form, as
     * LUFA and ChibiOS do. */
    if (usb_keyboard_protocol != usb_device_state_get_protocol()) {
        usb_device_state_set_protocol(usb_keyboard_protocol);
        clear_keyboard();
    }

    /* A kept keyboard report; otherwise, with a nonzero SET_IDLE duration (4 ms units), the current report again
     * once that time has passed since the last report. The repeat does not signal remote wake-up. */
    if (kbd_report_pending) {
        send_kbd_report();
    } else {
        idle = usb_keyboard_idle;
        if (idle && usb_configured && timer_elapsed(kbd_report_time) >= (uint16_t)idle * 4) {
            if (usb_keyboard_protocol == 0 ? load_ep1(&kbd_report[1], KEYBOARD_REPORT_SIZE - 1) : load_ep1(kbd_report, KEYBOARD_REPORT_SIZE)) {
                kbd_report_time = timer_read();
            }
        }
    }

#ifdef EXTRAKEY_ENABLE
    /* One kept extra key report per call, when EP1 is free */
    for (i = 0; i < 2; i++) {
        if (extra_pending_set[i]) {
            extra_pending_set[i] = !send_ep1(&extra_pending[i], sizeof(report_extra_t));
            break;
        }
    }
#endif
}
