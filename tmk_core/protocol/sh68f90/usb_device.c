/* SPDX-License-Identifier: GPL-2.0-or-later */

/* USB device of the SH68F90A (datasheet 8.4): control transfers on EP0 in 8-byte packets, the standard and HID class
 * requests, bus reset, suspend and resume. The descriptors come from usb_descriptor.c. */

#include "usb_device.h"
#include "mcs51_mcu.h"
/* Standard request codes, request type fields and feature selectors */
#include "lib/lufa/LUFA/Drivers/USB/Core/StdRequestType.h"
#include "report.h"
#include "wait.h"
#include "mcs51_isr.h"

volatile bool    usb_configured;
volatile bool    usb_suspended;
volatile bool    usb_remote_wakeup;
volatile bool    usb_wakeup_sent;
volatile uint8_t usb_keyboard_leds;
volatile uint8_t usb_keyboard_protocol;
volatile uint8_t usb_keyboard_idle;
#ifdef RAW_ENABLE
volatile bool usb_raw_out_waiting;
#endif
#ifdef BOOTLOADER_SINOWEALTH_ISP
volatile bool usb_isp_requested;
#endif
#ifdef SH68F90_SUSPEND_SLEEP
/* usb_sleep() has lowered the clock: the USB interrupt leaves GOSUSP to it, except at a bus reset, whose SWRST clears
 * GOSUSP */
static volatile bool usb_sleeping;
#endif

/* HID 1.11 7.2.4: the recommended idle rate of a keyboard at initialization, 500 ms (4 ms units) */
#define KEYBOARD_IDLE_DEFAULT 125

#if !defined(SERIAL_NUMBER) && (defined(SERIAL_NUMBER_USE_HARDWARE_ID) && SERIAL_NUMBER_USE_HARDWARE_ID == TRUE)
/* String descriptor 3 (serial number) in xdata, built from the chip ID (usb_descriptor.c). It is built once in
 * usb_device_init(): reading the chip ID switches MOVC to the information area. */
void set_serial_number_descriptor(void);
#endif

/* The control transfer on EP0 */
enum {
    EP0_IDLE,      /* no transfer, or one that needs no further action */
    EP0_IN_DATA,   /* control read: the data stage */
    EP0_OUT_DATA,  /* control write: waiting for the data stage */
    EP0_IN_STATUS, /* control write: waiting for the end of the status stage */
};

static uint8_t        setup[8];      /* the SETUP packet of the transfer */
static uint8_t        ep0_state;
static const uint8_t *ep0_data;      /* the rest of a control read's data: code memory, or xdata (serial number) */
static uint16_t       ep0_left;      /* bytes of it not loaded yet */
static bool           ep0_zlp;       /* a zero-length packet ends the data stage */
static uint8_t        ep0_reply[2];  /* data of the short replies */
static uint8_t        usb_address;   /* SET_ADDRESS: written to USBADDR at the end of the status stage */
#ifdef BOOTLOADER_SINOWEALTH_ISP
static bool ep0_isp; /* the SET_REPORT data stage was the ISP request: usb_isp_requested after the status stage */
#endif

/* Report types in the high byte of wValue of GET_REPORT and SET_REPORT (HID 1.11 7.2.1) */
#define HID_REPORT_TYPE_INPUT 1
#define HID_REPORT_TYPE_OUTPUT 2
#define HID_REPORT_TYPE_FEATURE 3

#define SETUP_TYPE setup[0]
#define SETUP_REQUEST setup[1]
#define SETUP_VALUE_L setup[2]
#define SETUP_VALUE_H setup[3]
#define SETUP_INDEX_L setup[4]
#define SETUP_VALUE ((uint16_t)setup[3] << 8 | setup[2])
#define SETUP_INDEX ((uint16_t)setup[5] << 8 | setup[4])
#define SETUP_LENGTH ((uint16_t)setup[7] << 8 | setup[6])

/* The functions below run in the USB interrupt (usb_device_init() calls usb_bus_reset() before the interrupt is
 * enabled). Functions that an interrupt handler calls must not share overlay space with the main loop's functions. */
#pragma save
#pragma nooverlay

/* Called at a bus reset and at the start. A USB software reset (SWRST) returns the controller's state machine, the
 * endpoint registers (also the ready bits, which a bus reset leaves as they are), the address and the interrupt flags
 * to their reset values and clears GOSUSP (datasheet 8.4.4). The datasheet gives no width for the SWRST pulse; it is six cycles
 * here (250 ns). SWRST also clears most interrupt enable bits, so they are written after it. */
static void usb_bus_reset(void) {
    USBCON |= bSWRST;
    __asm__("nop\n nop\n nop\n nop\n nop\n nop");
    USBCON &= ~bSWRST;

    /* OEP0RDY is set before the first SETUP arrives. OW is set when a SETUP arrives while OEP0RDY is clear. While OW
     * is set, writes to OEP0RDY have no effect, and the status stage of a control read is answered with NAK. */
    EP0CON |= bOEP_RDY;
    USBADDR = 0;
    USBIE1  = bSETUPIE | bRESMIE | bSUSPIE | bPBRSTIE;
#ifdef RAW_ENABLE
    USBIE2 = bIEP0IE | bOEP0IE | bOEP2IE;
#else
    USBIE2 = bIEP0IE | bOEP0IE;
#endif

    ep0_state             = EP0_IDLE;
    usb_configured        = false;
    usb_suspended         = false;
    usb_remote_wakeup     = false;
    usb_wakeup_sent       = false;
    usb_keyboard_protocol = 1; /* report protocol, the default (HID 1.11 7.2.6) */
    usb_keyboard_idle     = KEYBOARD_IDLE_DEFAULT;
#ifdef RAW_ENABLE
    usb_raw_out_waiting = false;
#endif
}

/* Loads the next packet of a control read's data stage into EP0. A packet shorter than 8 bytes ends the stage. */
static void ep0_load(void) {
    uint8_t n = (ep0_left > USB_EP0_SIZE) ? USB_EP0_SIZE : (uint8_t)ep0_left;
    uint8_t i;

    for (i = 0; i < n; i++) {
        USB_EP0_IN_BUF[i] = ep0_data[i];
    }
    ep0_data += n;
    ep0_left -= n;
    if (n < USB_EP0_SIZE) {
        ep0_zlp = false;
    }
    IEP0CNT = n;
    EP0CON |= bIEP_RDY;
}

/* Starts a control read with len bytes at data, at most wLength. The data stage ends with a zero-length packet if
 * it is shorter than wLength and a multiple of 8 bytes (USB 2.0 5.5.3). */
static void ep0_in(const uint8_t *data, uint16_t len) {
    uint16_t length = SETUP_LENGTH;

    if (len > length) {
        len = length;
    }
    ep0_data  = data;
    ep0_left  = len;
    ep0_zlp   = len < length && (len % USB_EP0_SIZE) == 0;
    ep0_state = EP0_IN_DATA;
    ep0_load();
}

/* The status stage of a control write: a zero-length IN packet */
static void ep0_status(void) {
    IEP0CNT   = 0;
    EP0CON   |= bIEP_RDY;
    ep0_state = EP0_IN_STATUS;
}

/* A request error: EP0 answers STALL in both directions until the next SETUP, which clears both stall bits
 * (datasheet 8.4.4, EP0CON). */
static void ep0_stall(void) {
    EP0CON |= bIEP_STL | bOEP_STL;
    ep0_state = EP0_IDLE;
}

/* SET_FEATURE (halt) and CLEAR_FEATURE (not halt) of ENDPOINT_HALT for the endpoint address ep. Clearing it also
 * restarts the data toggle at DATA0 (USB 2.0 9.4.5). EP0 is not halted by a request: its stall ends with the next
 * SETUP. Returns false for an endpoint that the device does not have. */
static bool ep_set_halt(uint8_t ep, bool halt) {
    switch (ep) {
        case 0x00:
        case 0x80:
            return true;
        case 0x80 | SHARED_IN_EPNUM:
            if (halt) {
                EP1CON |= bIEP_STL;
            } else {
                EP1CON &= ~(bIEP_STL | bIEP_DTG);
            }
            return true;
#ifdef RAW_ENABLE
        case 0x80 | RAW_IN_EPNUM:
            if (halt) {
                EP2CON |= bIEP_STL;
            } else {
                EP2CON &= ~(bIEP_STL | bIEP_DTG);
            }
            return true;
        case RAW_OUT_EPNUM:
            if (halt) {
                EP2CON |= bOEP_STL;
            } else {
                EP2CON &= ~(bOEP_STL | bOEP_DTG);
            }
            return true;
#endif
    }
    return false;
}

/* GET_STATUS of an endpoint: 1 if it is halted. Returns 0xFF for an endpoint that the device does not have. */
static uint8_t ep_halted(uint8_t ep) {
    switch (ep) {
        case 0x00:
        case 0x80:
            return 0;
        case 0x80 | SHARED_IN_EPNUM:
            return (EP1CON & bIEP_STL) ? 1 : 0;
#ifdef RAW_ENABLE
        case 0x80 | RAW_IN_EPNUM:
            return (EP2CON & bIEP_STL) ? 1 : 0;
        case RAW_OUT_EPNUM:
            return (EP2CON & bOEP_STL) ? 1 : 0;
#endif
    }
    return 0xFF;
}

/* Standard requests (USB 2.0 9.4). Returns false for a request error. */
static bool ep0_standard_request(void) {
    uint8_t recipient = SETUP_TYPE & CONTROL_REQTYPE_RECIPIENT;

    switch (SETUP_REQUEST) {
        case REQ_GetDescriptor: {
            const void *addr;
            uint16_t    len = get_usb_descriptor(SETUP_VALUE, SETUP_INDEX, SETUP_LENGTH, &addr);
            if (len == NO_DESCRIPTOR) {
                return false;
            }
            ep0_in(addr, len);
            return true;
        }

        case REQ_SetAddress:
            usb_address = SETUP_VALUE_L & 0x7F;
            ep0_status();
            return true;

        case REQ_GetConfiguration:
            ep0_reply[0] = usb_configured ? 1 : 0;
            ep0_in(ep0_reply, 1);
            return true;

        case REQ_SetConfiguration:
            /* 0, or the bConfigurationValue of the one configuration (1) */
            if (SETUP_VALUE_L > 1) {
                return false;
            }
            usb_configured = SETUP_VALUE_L != 0;
            /* Every endpoint restarts at DATA0 and its halt ends (USB 2.0 9.4.5). A written 0 leaves a ready bit as
             * it is (datasheet 8.4.4), so a plain write clears the toggle, buffer select and stall bits. */
            EP1CON = 0;
#ifdef RAW_ENABLE
            EP2CON = usb_raw_out_waiting ? 0 : bOEP_RDY;
#endif
            ep0_status();
            return true;

        case REQ_GetInterface:
            if (!usb_configured || SETUP_INDEX_L >= TOTAL_INTERFACES) {
                return false;
            }
            ep0_reply[0] = 0;
            ep0_in(ep0_reply, 1);
            return true;

        case REQ_SetInterface:
            /* Each interface has only the alternate setting 0. The request restarts the data toggles of the
             * interface's endpoints and ends their halts (USB 2.0 9.4.5). */
            if (!usb_configured || SETUP_INDEX_L >= TOTAL_INTERFACES || SETUP_VALUE != 0) {
                return false;
            }
            if (SETUP_INDEX_L == SHARED_INTERFACE) {
                ep_set_halt(0x80 | SHARED_IN_EPNUM, false);
            }
#ifdef RAW_ENABLE
            if (SETUP_INDEX_L == RAW_INTERFACE) {
                ep_set_halt(0x80 | RAW_IN_EPNUM, false);
                ep_set_halt(RAW_OUT_EPNUM, false);
            }
#endif
            ep0_status();
            return true;

        case REQ_SetFeature:
        case REQ_ClearFeature:
            if (recipient == REQREC_DEVICE && SETUP_VALUE == FEATURE_SEL_DeviceRemoteWakeup) {
                usb_remote_wakeup = SETUP_REQUEST == REQ_SetFeature;
            } else if (recipient != REQREC_ENDPOINT || SETUP_VALUE != FEATURE_SEL_EndpointHalt || !ep_set_halt(SETUP_INDEX_L, SETUP_REQUEST == REQ_SetFeature)) {
                return false;
            }
            ep0_status();
            return true;

        case REQ_GetStatus:
            ep0_reply[0] = 0;
            ep0_reply[1] = 0;
            if (recipient == REQREC_DEVICE) {
                /* bit 1: remote wake-up enabled; bit 0 (self-powered) stays 0 */
                if (usb_remote_wakeup) {
                    ep0_reply[0] = FEATURE_REMOTE_WAKEUP_ENABLED;
                }
            } else if (recipient == REQREC_INTERFACE) {
                if (!usb_configured || SETUP_INDEX_L >= TOTAL_INTERFACES) {
                    return false;
                }
            } else if (recipient == REQREC_ENDPOINT) {
                uint8_t halted = ep_halted(SETUP_INDEX_L);
                if (halted == 0xFF) {
                    return false;
                }
                ep0_reply[0] = halted;
            } else {
                return false;
            }
            ep0_in(ep0_reply, 2);
            return true;
    }
    return false;
}

/* HID class requests (HID 1.11 7.2). Returns false for a request error. Only the keyboard interface (the shared
 * interface) has a boot protocol, an idle rate and reports that GET_REPORT returns. */
static bool ep0_hid_request(void) {
    uint8_t interface = SETUP_INDEX_L;

    if (interface >= TOTAL_INTERFACES) {
        return false;
    }
    switch (SETUP_REQUEST) {
        case HID_REQ_GetReport:
            /* The input report of the keyboard: 8 bytes in the boot protocol, the report ID and 8 bytes in the
             * report protocol. The main loop writes kbd_report while this interrupt is enabled, so an answer during
             * a key change can mix two consecutive reports; the new report follows on EP1. */
            if (interface != KEYBOARD_INTERFACE || SETUP_VALUE_H != HID_REPORT_TYPE_INPUT) {
                return false;
            }
            if (usb_keyboard_protocol == 0) {
                ep0_in(&kbd_report[1], KEYBOARD_REPORT_SIZE - 1);
            } else if (SETUP_VALUE_L == REPORT_ID_KEYBOARD) {
                ep0_in(kbd_report, KEYBOARD_REPORT_SIZE);
            } else {
                return false;
            }
            return true;

        case HID_REQ_SetReport:
            /* The data stage, at most one packet, is handled in ep0_out() */
            if (SETUP_LENGTH == 0 || SETUP_LENGTH > USB_EP0_SIZE) {
                return false;
            }
            ep0_state = EP0_OUT_DATA;
            return true;

        case HID_REQ_SetIdle:
            /* The duration is in the high byte of wValue. The keyboard report is repeated (protocol_post_task());
             * the other reports are sent only when they change. */
            if (interface == KEYBOARD_INTERFACE) {
                usb_keyboard_idle = SETUP_VALUE_H;
            }
            ep0_status();
            return true;

        case HID_REQ_GetIdle:
            ep0_reply[0] = (interface == KEYBOARD_INTERFACE) ? usb_keyboard_idle : 0;
            ep0_in(ep0_reply, 1);
            return true;

        case HID_REQ_SetProtocol:
            /* Any value but 0 selects the report protocol, as in usb_device_state_set_protocol(); protocol_post_task()
             * applies it */
            if (interface != KEYBOARD_INTERFACE) {
                return false;
            }
            usb_keyboard_protocol = (SETUP_VALUE_L != 0);
            ep0_status();
            return true;

        case HID_REQ_GetProtocol:
            if (interface != KEYBOARD_INTERFACE) {
                return false;
            }
            ep0_reply[0] = usb_keyboard_protocol;
            ep0_in(ep0_reply, 1);
            return true;
    }
    return false;
}

/* A SETUP packet: it ends the previous control transfer (USB 2.0 8.5.3). EP0 accepts the next OUT packet (the data
 * stage of a control write, or the status stage of a control read) once the packet has been read.
 * Facts not in the datasheet: OW is set by a SETUP that finds OEP0RDY clear, OVERIF by an IN or OUT data packet that
 * arrives while the SETUP is unread, and while either is set the SIE takes no write to the EP0 ready and stall bits
 * (OW: a readout of the registers; OVERIF: Sinowealth's demo code). Both are cleared by writing them 0, although the
 * datasheet lists OW as read-only; a read-modify-write that writes OW back as 1 does not clear it. A data packet
 * between the clear and the write of OEP0RDY sets OVERIF again, so the write is checked and repeated once, as the
 * demo code does. The request handlers write the ready and stall bits after OEP0RDY is set, when no packet sets
 * OVERIF. */
static void ep0_setup(void) {
    uint8_t i;
    bool    ok;

    for (i = 0; i < sizeof(setup); i++) {
        setup[i] = USB_EP0_OUT_BUF[i];
    }
    USBIF1 &= ~(bOVERIF | bOW);
    EP0CON |= bOEP_RDY;
    if (!(EP0CON & bOEP_RDY)) {
        USBIF1 &= ~(bOVERIF | bOW);
        EP0CON |= bOEP_RDY;
    }
    ep0_state = EP0_IDLE;
#ifdef BOOTLOADER_SINOWEALTH_ISP
    ep0_isp = false;
#endif

    switch (SETUP_TYPE & CONTROL_REQTYPE_TYPE) {
        case REQTYPE_STANDARD:
            ok = ep0_standard_request();
            break;
        case REQTYPE_CLASS:
            ok = (SETUP_TYPE & CONTROL_REQTYPE_RECIPIENT) == REQREC_INTERFACE && ep0_hid_request();
            break;
        default:
            ok = false;
            break;
    }
    if (!ok) {
        ep0_stall();
    }
}

/* The data stage of SET_REPORT: the LED output report of the keyboard (1 byte in the boot protocol, the report ID
 * and 1 byte in the report protocol), and the feature report REPORT_ID_SINOWEALTH_ISP. Other reports are accepted
 * and ignored. */
static void ep0_set_report(void) {
    uint8_t n = OEP0CNT;

    if (SETUP_INDEX_L != KEYBOARD_INTERFACE) {
        return;
    }
    if (SETUP_VALUE_H == HID_REPORT_TYPE_OUTPUT) {
        if (n == 1) {
            usb_keyboard_leds = USB_EP0_OUT_BUF[0];
        } else if (n == 2 && (USB_EP0_OUT_BUF[0] == REPORT_ID_KEYBOARD || USB_EP0_OUT_BUF[0] == REPORT_ID_NKRO)) {
            usb_keyboard_leds = USB_EP0_OUT_BUF[1];
        }
    }
#ifdef BOOTLOADER_SINOWEALTH_ISP
    ep0_isp = SETUP_VALUE_H == HID_REPORT_TYPE_FEATURE && SETUP_VALUE_L == REPORT_ID_SINOWEALTH_ISP && n >= 2 && USB_EP0_OUT_BUF[0] == REPORT_ID_SINOWEALTH_ISP && USB_EP0_OUT_BUF[1] == 0x75;
#endif
}

/* An OUT packet on EP0: the data stage of a control write, or the status stage of a control read */
static void ep0_out(void) {
    if (ep0_state == EP0_OUT_DATA) {
        ep0_set_report();
        ep0_status();
    } else {
        ep0_state = EP0_IDLE;
    }
    EP0CON |= bOEP_RDY;
}

/* An IN packet on EP0 was acknowledged: the next packet of a control read, or the end of a status stage. OEP0RDY is
 * set after every IN packet, so that EP0 OUT is ready for the status stage of a control read and for the next
 * SETUP. */
static void ep0_in_done(void) {
    if (ep0_state == EP0_IN_DATA) {
        if (ep0_left != 0 || ep0_zlp) {
            ep0_load();
        } else {
            ep0_state = EP0_IDLE;
        }
    } else if (ep0_state == EP0_IN_STATUS) {
        if (SETUP_REQUEST == REQ_SetAddress) {
            USBADDR = usb_address;
        }
#ifdef BOOTLOADER_SINOWEALTH_ISP
        /* The main loop calls reset_keyboard() (protocol_pre_task()) */
        if (SETUP_REQUEST == HID_REQ_SetReport && ep0_isp) {
            usb_isp_requested = true;
        }
#endif
        ep0_state = EP0_IDLE;
    }
    EP0CON |= bOEP_RDY;
}

/* The USB interrupt. While FLASHCON.FAC is set, MOVC reads the information area instead of the program; the handler
 * reads the descriptors with MOVC, so it clears FLASHCON and restores it at the end. */
MCS51_ISR(usb_isr, INT_NO_USB) {
    uint8_t flashcon = FLASHCON;
    uint8_t if1;
    uint8_t if2;

    FLASHCON = 0;

    if1 = USBIF1;
    if2 = USBIF2;
    /* SOFIF is set at every frame; it is handled only while SOFIE is set, which is during a suspend */
    if (!(USBIE1 & bSOFIE)) {
        if1 &= ~bSOFIF;
    }

    if (if1 & bUSBRSTIF) {
        /* SWRST clears the other flags (datasheet 8.4.4) */
        usb_bus_reset();
        USBIF1 &= ~bUSBRSTIF;
    } else {
        if (if1 & bSUSPIF) {
            /* The controller must be put into suspend within 7 ms of SUSPIF (datasheet 8.4.3). RESMIF is set only when
             * the host resumes the bus while GOSUSP is set; a remote wake-up clears GOSUSP, so after it the first SOF
             * ends the suspend. */
            USBIF1 &= ~(bSUSPIF | bSOFIF);
            USBCON |= bGOSUSP;
            USBIE1 |= bSOFIE;
            usb_suspended = true;
        }
        if (if1 & (bRESMIF | bSOFIF)) {
            USBIF1 &= ~(bRESMIF | bSOFIF);
            USBIE1 &= ~bSOFIE;
#ifdef SH68F90_SUSPEND_SLEEP
            /* GOSUSP is cleared after the high-frequency clock has started again (datasheet 8.4.3) */
            if (!usb_sleeping) {
                USBCON &= ~bGOSUSP;
            }
#else
            USBCON &= ~bGOSUSP;
#endif
            usb_suspended   = false;
            usb_wakeup_sent = false;
        }
        if (if1 & bSETUPIF) {
            /* EP0 flags that are still set belong to the transfer that the SETUP ended. SETUPIF is cleared before
             * OVERIF (datasheet 8.4.4), which ep0_setup() clears with OW. */
            USBIF1 &= ~bSETUPIF;
            USBIF2 &= ~(bIEP0IF | bOEP0IF);
            ep0_setup();
        } else {
            if (if2 & bOEP0IF) {
                USBIF2 &= ~bOEP0IF;
                ep0_out();
            }
            if (if2 & bIEP0IF) {
                USBIF2 &= ~bIEP0IF;
                ep0_in_done();
            }
        }
#ifdef RAW_ENABLE
        if (if2 & bOEP2IF) {
            /* EP2 answers NAK until the main loop has read the packet and set OEP2RDY */
            USBIF2 &= ~bOEP2IF;
            usb_raw_out_waiting = true;
        }
#endif
    }

    FLASHCON = flashcon;
}

#pragma restore

/* The USB module's 3.3 V regulator is turned on 500 us before the module (datasheet 8.1). The USB interrupt is at the
 * highest of the four priority levels, so that the other interrupt handlers do not delay it. The 1.5 kOhm pull-up
 * on D+ is connected last: from then on the host sees the device. */
void usb_device_init(void) {
#if !defined(SERIAL_NUMBER) && (defined(SERIAL_NUMBER_USE_HARDWARE_ID) && SERIAL_NUMBER_USE_HARDWARE_ID == TRUE)
    set_serial_number_descriptor();
#endif
    REGCON = bREGEN;
    wait_us(500);
    USBCON = bENUSB;
    usb_bus_reset();
    IPH1 |= bPUSBH;
    IPL1 |= bPUSBL;
    IEN1 |= bEUSB;
    USBCON |= bSW1CON;
}

/* Remote wake-up (USB 2.0 7.1.7.7): resume signaling only after the bus has been idle for 5 ms. A device detects
 * suspend after 3 ms of idle bus (USB 2.0 7.1.7.6), so usb_suspended still set 2 ms later means at least 5 ms. The
 * controller drives the resume signal for 5 ms once WKUP is set and then clears the bit (datasheet 8.4.4). */
void usb_send_resume(void) {
    wait_ms(2);
    IEN1 &= ~bEUSB;
    if (usb_suspended) {
        USBCON = (USBCON & ~bGOSUSP) | bWKUP;
        usb_wakeup_sent = true;
    }
    IEN1 |= bEUSB;
}

#ifdef SH68F90_SUSPEND_SLEEP
/*
 * Power-down in USB suspend (datasheet 8.4.3 and 8.9.3). Only the USB interrupt, and the keyboard's key interrupt if
 * the host enabled remote wake-up, stay enabled; resume, bus reset and key presses wake the MCU, and the interrupt
 * handler runs before the code after PCON.PD, at SYSCLK = 128 kHz. The 3.3 V regulator stays on: datasheet 8.4.3
 * (design spec 4) keeps it on in power-down with a USB wake-up, although the REGCON description (8.1) asks for it to
 * be off before power-down. Not after a remote wake-up signal, which has cleared GOSUSP: the host then resumes the
 * bus (USB 2.0 7.1.7.7).
 * SUSLO = 0x55 and the write of PCON.PD must be consecutive instructions, followed by at least three NOPs. On the
 * 8051 an instruction that writes IE delays interrupts by one more instruction: an interrupt that is pending at SETB
 * EA runs between the SUSLO and the PCON write, and the MCU does not power down.
 */
void usb_sleep(void) {
    uint8_t ien0;
    uint8_t ien1;

    EA = 0;
    if (!usb_suspended || usb_wakeup_sent) {
        EA = 1;
        return;
    }
    ien0 = IEN0;
    ien1 = IEN1;
    sh68f90_sleep_enter_kb(usb_remote_wakeup);
    ET2  = 0;
    IEN1 = bEUSB;
    mcs51_clock_stop();
    usb_sleeping = true;
    (void)RSTSTAT;
    __asm__("setb _EA\n mov _SUSLO,#0x55\n orl _PCON,#0x02\n nop\n nop\n nop\n nop\n nop\n nop");
    EA = 0;
    mcs51_clock_restart();
    (void)RSTSTAT;
    usb_sleeping = false;
    if (!usb_suspended) {
        USBCON &= ~bGOSUSP;
    }
    sh68f90_sleep_exit_kb();
    IEN1 = ien1;
    IEN0 = ien0;
    EA   = 1;
}
#endif
