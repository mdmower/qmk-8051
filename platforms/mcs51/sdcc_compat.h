// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Included before every source file of an SDCC build (platform.mk). SDCC has no GCC built-ins; the definitions
 * here take the place of those in QMK headers that check `#if !defined(...)` first.
 */
#pragma once

/* quantum/util.h: without __builtin_choose_expr(), no check that the argument is an array */
#define ARRAY_SIZE(array) (sizeof((array)) / sizeof((array)[0]))

/* wchar_t of 16 bits, as GCC's -fshort-wchar (QMK's USB string descriptors); USBSTR() gives char16_t literals
 * under SDCC to match (usb_descriptor_common.h). SDCC's headers define wchar_t only without __WCHAR_T_DEFINED. */
#define __WCHAR_T_DEFINED
typedef unsigned int wchar_t;

/* GCC attributes: SDCC has none. sdcc_cc.py keeps them (QMK_SDCC_ATTRIBUTES) in the text that clang reads to find
 * the weak definitions. */
#ifndef QMK_SDCC_ATTRIBUTES
#    define __attribute__(x)
#endif

/* quantum/eeconfig.h, quantum/nvm/nvm_eeconfig.h: SDCC gives error 141 for a repeated typedef of a union tag
 * (https://sourceforge.net/p/sdcc/bugs/4159/) but accepts a forward typedef followed by the defining one. Under SDCC
 * the two headers leave out their forward typedefs and use these, which come before every definition. */
typedef union debug_config_t      debug_config_t;
typedef union keymap_config_t     keymap_config_t;
typedef union audio_config_t      audio_config_t;
typedef union unicode_config_t    unicode_config_t;
typedef union backlight_config_t  backlight_config_t;
typedef union rgb_config_t        rgb_config_t;
typedef union led_eeconfig_t      led_eeconfig_t;
typedef union rgblight_config_t   rgblight_config_t;
typedef union haptic_config_t     haptic_config_t;
typedef union connection_config_t connection_config_t;
