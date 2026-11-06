// Copyright 2026 Matt Mower (@mdmower)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// With ROW2COL PWM, the R, G, and B channels of a column turn on together and white shows red.
// With COL2ROW PWM, one channel turns on at a time.
#define SN32F2XX_PWM_DIRECTION COL2ROW
