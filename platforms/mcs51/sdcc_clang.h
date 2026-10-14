// Copyright 2026 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * SDCC's keywords for clang, which sdcc_cc.py runs on the preprocessed source to find the weak definitions and the
 * constructs of sdcc_check.query. Never part of a firmware build.
 */
#pragma once

#define __xdata
#define __data
#define __idata
#define __pdata
#define __code
#define __near
#define __far
#define __at(x)
#define __sfr volatile unsigned char
#define __sfr16 volatile unsigned short
#define __sfr32 volatile unsigned long
#define __sbit volatile _Bool
#define __bit _Bool
#define __reentrant
#define __interrupt(x)
#define __naked
#define __critical
#define __using(x)
#define __banked
#define __nonbanked
