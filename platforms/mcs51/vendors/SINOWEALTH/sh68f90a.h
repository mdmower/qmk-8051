/* SPDX-License-Identifier: GPL-2.0-or-later */

/* SH68F90A registers, from the datasheet (DS000064E V1.0, chapter 6 and the PWM chapter 8.2). Bit masks: b<bit>, and
 * MASK_<field> for fields of more than one bit. */
#pragma once

/* CPU core (table 6.1) */
__sfr __at(0xE0) ACC;
__sfr __at(0xF0) B;
__sfr __at(0xF1) AUXC;                  // C register (16-bit MUL and DIV)
__sfr __at(0xD0) PSW;
__sbit __at(0xD7) CY;                   // PSW.7
__sbit __at(0xD6) AC;                   // PSW.6
__sbit __at(0xD5) F0;                   // PSW.5
__sbit __at(0xD4) RS1;                  // PSW.4
__sbit __at(0xD3) RS0;                  // PSW.3
__sbit __at(0xD2) OV;                   // PSW.2
__sbit __at(0xD1) F1;                   // PSW.1
__sbit __at(0xD0) P;                    // PSW.0
__sfr __at(0x81) SP;
__sfr __at(0x82) DPL;
__sfr __at(0x83) DPH;
__sfr __at(0x84) DPL1;
__sfr __at(0x85) DPH1;
__sfr __at(0x86) INSCON;                // data pointer select, 16-bit MUL and DIV
#define bDIV 0x08
#define bMUL 0x04
#define bDPS 0x01                       // DPTR1 selected

/* Power and clock (tables 6.2 and 6.5) */
__sfr __at(0x87) PCON;
#define bSMOD 0x80
#define bSSTAT 0x40
#define bSSTAT1 0x20
#define bGF1 0x08
#define bGF0 0x04
#define bPD 0x02                        // power-down
#define bIDL 0x01                       // idle
__sfr __at(0x8E) SUSLO;                 // suspend mode control
__sfr __at(0xB2) CLKCON;                // system clock control
#define MASK_CLKS 0x60                  // SYSCLK = OSCSCLK / 1, 2, 4, 12
#define bHFON 0x08                      // 24 MHz RC oscillator on
#define bFS 0x04                        // SYSCLK = OSCSCLK, not OSC1CLK
__sfr __at(0xBC) PLLCON;                // PLL control
#define bPLLSTA 0x04                    // PLL locked
#define bPLLON 0x02                     // PLL on
#define bPLLFS 0x01                     // OSCSCLK = PLL / 2
__sfr __at(0xBD) CLKLO;                 // RC calibration control
#define bCLKRCEN 0x80
#define MASK_CLKLO 0x0F
__sfr __at(0xC6) CLKRC0H;
__sfr __at(0xBE) CLKRC0L;
__sfr __at(0xC7) CLKRC1H;
__sfr __at(0xBF) CLKRC1L;

/* Flash and self-programming (table 6.3) */
__sfr __at(0xFB) IB_OFFSET;
__sfr __at(0xFC) IB_DATA;
__sfr __at(0xF2) IB_CON1;
__sfr __at(0xF3) IB_CON2;
__sfr __at(0xF4) IB_CON3;
__sfr __at(0xF5) IB_CON4;
__sfr __at(0xF6) IB_CON5;
__sfr __at(0xF7) XPAGE;                 // MOVX @Ri page, SSP sector
/* SDCC's start-up code pages MOVX @Ri through the register named _XPAGE. SDCC's library defines it as 0xA0 (P2 of the
 * original 8051, P3 on this MCU) unless the program defines it. */
__sfr __at(0xF7) _XPAGE;
__sfr __at(0xA7) FLASHCON;
#define bFAC 0x01                       // MOVC and SSP access the information area

/* Reset and watchdog (table 6.4) */
__sfr __at(0xB1) RSTSTAT;
#define bWDOF 0x80                      // watchdog or program overflow reset
#define bPORF 0x20                      // power-on reset
#define bLVRF 0x10                      // low-voltage reset
#define bCLRF 0x08                      // reset pin
#define MASK_WDT 0x07                   // watchdog period

/* Interrupts (table 6.6) */
__sfr __at(0xA8) IEN0;
__sbit __at(0xAF) EA;                   // IEN0.7
__sbit __at(0xAE) ESPI;                 // IEN0.6
__sbit __at(0xAD) ELPD;                 // IEN0.5
__sbit __at(0xAC) ESCM;                 // IEN0.4
__sbit __at(0xAB) EX2;                  // IEN0.3
__sbit __at(0xAA) EX3;                  // IEN0.2
__sbit __at(0xA9) EX4;                  // IEN0.1
__sbit __at(0xA8) ET2;                  // IEN0.0
__sfr __at(0xA9) IEN1;
#define bES0 0x40
#define bEPWM4 0x20
#define bEPWM3 0x10
#define bEPWM2 0x08
#define bEPWM1 0x04
#define bEPWM0 0x02
#define bEUSB 0x01
__sfr __at(0xBA) IENC;                  // INT40-INT47 channel enable, bit n = INT4n
__sfr __at(0xB4) IPH0;
#define bPSPIH 0x40
#define bPLPDH 0x20
#define bPSCMH 0x10
#define bPX2H 0x08
#define bPX3H 0x04
#define bPX4H 0x02
#define bPT2H 0x01
__sfr __at(0xB8) IPL0;
__sbit __at(0xBE) PSPIL;                // IPL0.6
__sbit __at(0xBD) PLPDL;                // IPL0.5
__sbit __at(0xBC) PSCML;                // IPL0.4
__sbit __at(0xBB) PX2L;                 // IPL0.3
__sbit __at(0xBA) PX3L;                 // IPL0.2
__sbit __at(0xB9) PX4L;                 // IPL0.1
__sbit __at(0xB8) PT2L;                 // IPL0.0
__sfr __at(0xB5) IPH1;
#define bPS0H 0x40
#define bPPWM4H 0x20
#define bPPWM3H 0x10
#define bPPWM2H 0x08
#define bPPWM1H 0x04
#define bPPWM0H 0x02
#define bPUSBH 0x01
__sfr __at(0xB9) IPL1;
#define bPS0L 0x40
#define bPPWM4L 0x20
#define bPPWM3L 0x10
#define bPPWM2L 0x08
#define bPPWM1L 0x04
#define bPPWM0L 0x02
#define bPUSBL 0x01
__sfr __at(0xB6) EXF0;
#define MASK_IT4 0xC0
#define MASK_IT3 0x30
#define MASK_IT2 0x0C
#define bIE3 0x02
#define bIE2 0x01
__sfr __at(0xE8) EXF1;
__sbit __at(0xEF) IF47;                 // EXF1.7
__sbit __at(0xEE) IF46;                 // EXF1.6
__sbit __at(0xED) IF45;                 // EXF1.5
__sbit __at(0xEC) IF44;                 // EXF1.4
__sbit __at(0xEB) IF43;                 // EXF1.3
__sbit __at(0xEA) IF42;                 // EXF1.2
__sbit __at(0xE9) IF41;                 // EXF1.1
__sbit __at(0xE8) IF40;                 // EXF1.0
__sfr __at(0x8B) EXCON;                 // external interrupt sampling
#define MASK_I1PS 0xC0
#define MASK_I1SN 0x30
#define MASK_I0PS 0x0C
#define MASK_I0SN 0x03

/* Interrupt numbers (vector 8n + 3; datasheet 7.8.9) */
#define INT_NO_TIMER2 0
#define INT_NO_INT4 1
#define INT_NO_INT3 2
#define INT_NO_INT2 3
#define INT_NO_LPD 5
#define INT_NO_SPI 6
#define INT_NO_USB 7
#define INT_NO_PWM0 8
#define INT_NO_PWM1 9
#define INT_NO_PWM2 10
#define INT_NO_PWM3 11
#define INT_NO_PWM4 12
#define INT_NO_EUART0 13

/* Ports (table 6.7). P1, P2 and P3 have six pins (bits 0-5). PxCR: direction, 1 = output; PxPCR: pull-up of an
 * input, 1 = on. */
__sfr __at(0x80) P0;
__sfr __at(0x90) P1;
__sfr __at(0x98) P2;
__sfr __at(0xA0) P3;
__sfr __at(0xB0) P4;
__sfr __at(0x88) P5;
__sfr __at(0xC0) P6;
__sfr __at(0xF8) P7;
__sfr __at(0xE1) P0CR;
__sfr __at(0xE2) P1CR;
__sfr __at(0xE3) P2CR;
__sfr __at(0xE4) P3CR;
__sfr __at(0xE5) P4CR;
__sfr __at(0xE6) P5CR;
__sfr __at(0xE7) P6CR;
__sfr __at(0xD1) P7CR;
__sfr __at(0xE9) P0PCR;
__sfr __at(0xEA) P1PCR;
__sfr __at(0xEB) P2PCR;
__sfr __at(0xEC) P3PCR;
__sfr __at(0xED) P4PCR;
__sfr __at(0xEE) P5PCR;
__sfr __at(0xEF) P6PCR;
__sfr __at(0xD9) P7PCR;
__sbit __at(0x87) P0_7;
__sbit __at(0x86) P0_6;
__sbit __at(0x85) P0_5;
__sbit __at(0x84) P0_4;
__sbit __at(0x83) P0_3;
__sbit __at(0x82) P0_2;
__sbit __at(0x81) P0_1;
__sbit __at(0x80) P0_0;
__sbit __at(0x95) P1_5;
__sbit __at(0x94) P1_4;
__sbit __at(0x93) P1_3;
__sbit __at(0x92) P1_2;
__sbit __at(0x91) P1_1;
__sbit __at(0x90) P1_0;
__sbit __at(0x9D) P2_5;
__sbit __at(0x9C) P2_4;
__sbit __at(0x9B) P2_3;
__sbit __at(0x9A) P2_2;
__sbit __at(0x99) P2_1;
__sbit __at(0x98) P2_0;
__sbit __at(0xA5) P3_5;
__sbit __at(0xA4) P3_4;
__sbit __at(0xA3) P3_3;
__sbit __at(0xA2) P3_2;
__sbit __at(0xA1) P3_1;
__sbit __at(0xA0) P3_0;
__sbit __at(0xB7) P4_7;
__sbit __at(0xB6) P4_6;
__sbit __at(0xB5) P4_5;
__sbit __at(0xB4) P4_4;
__sbit __at(0xB3) P4_3;
__sbit __at(0xB2) P4_2;
__sbit __at(0xB1) P4_1;
__sbit __at(0xB0) P4_0;
__sbit __at(0x8F) P5_7;
__sbit __at(0x8E) P5_6;
__sbit __at(0x8D) P5_5;
__sbit __at(0x8C) P5_4;
__sbit __at(0x8B) P5_3;
__sbit __at(0x8A) P5_2;
__sbit __at(0x89) P5_1;
__sbit __at(0x88) P5_0;
__sbit __at(0xC7) P6_7;
__sbit __at(0xC6) P6_6;
__sbit __at(0xC5) P6_5;
__sbit __at(0xC4) P6_4;
__sbit __at(0xC3) P6_3;
__sbit __at(0xC2) P6_2;
__sbit __at(0xC1) P6_1;
__sbit __at(0xC0) P6_0;
__sbit __at(0xFF) P7_7;
__sbit __at(0xFE) P7_6;
__sbit __at(0xFD) P7_5;
__sbit __at(0xFC) P7_4;
__sbit __at(0xFB) P7_3;
__sbit __at(0xFA) P7_2;
__sbit __at(0xF9) P7_1;
__sbit __at(0xF8) P7_0;
__sfr __at(0x8C) DRVCON;                // port drive change: port select and lock
#define MASK_DRVEN 0xC0
#define MASK_DRVCON 0x0F
__sfr __at(0xA5) P1DRV;
__sfr __at(0xA6) P2DRV;
__sfr __at(0xBB) P3DRV;
__sfr __at(0x8D) P5DRV;
__sfr __at(0x8A) MAPPING;               // 0x5A: EUART0 on P3.3/P3.4, 0xA5: SPI on P3.3-P3.5

/* Timer 2 (table 6.8) */
__sfr __at(0xC8) T2CON;
__sbit __at(0xCF) TF2;                  // T2CON.7
__sbit __at(0xCE) EXF2;                 // T2CON.6
__sbit __at(0xCB) EXEN2;                // T2CON.3
__sbit __at(0xCA) TR2;                  // T2CON.2
__sbit __at(0xC9) C_T2;                 // T2CON.1
__sbit __at(0xC8) CP_RL2;               // T2CON.0
__sfr __at(0xC9) T2MOD;
#define bTCLKP2 0x80                    // Timer 2 clock SYSCLK, not SYSCLK / 12
#define bT2OE 0x02
#define bDCEN 0x01
__sfr __at(0xCA) RCAP2L;
__sfr __at(0xCB) RCAP2H;
__sfr __at(0xCC) TL2;
__sfr __at(0xCD) TH2;

/* Voltage regulator (table 6.9) */
__sfr __at(0x8F) REGCON;
#define bREGEN 0x01                     // 3.3 V regulator of the USB module on

/* SPI (table 6.10) */
__sfr __at(0xA2) SPCON;
#define bSPI_DIR 0x80
#define bMSTR 0x40
#define bCPHA 0x20
#define bCPOL 0x10
#define bSSDIS 0x08
#define MASK_SPR 0x07
__sfr __at(0xA1) SPSTA;
#define bSPEN 0x80
#define bSPIF 0x40
#define bMODF 0x20
#define bWCOL 0x10
#define bRXOV 0x08
__sfr __at(0xA3) SPDAT;

/* Low-voltage detection (table 6.12) */
__sfr __at(0xB3) LPDCON;
#define bLPDEN 0x80
#define bLPDF 0x40
#define bLPDV 0x20
#define bLPDIF 0x10
#define bLPDMD 0x08
__sfr __at(0x89) LPDSEL;
#define MASK_LPDS 0x0F

/* USB (table 6.13) */
__sfr __at(0x91) USBCON;
#define bENUSB 0x80
#define bSW1CON 0x40                    // D+ pull-up
#define bSWRST 0x20
#define bDPSTA 0x10
#define bDMSTA 0x08
#define bSW2CON 0x04
#define bWKUP 0x02                      // remote wake-up
#define bGOSUSP 0x01
__sfr __at(0x92) USBIF1;
#define bPUPIF 0x80
#define bOVERIF 0x40
#define bOW 0x20
#define bSETUPIF 0x10
#define bSOFIF 0x08
#define bRESMIF 0x04
#define bSUSPIF 0x02
#define bUSBRSTIF 0x01
__sfr __at(0x93) USBIF2;
#define bOEP2IF 0x40
#define bOEP1IF 0x20
#define bOEP0IF 0x10
#define bIEP2IF 0x04
#define bIEP1IF 0x02
#define bIEP0IF 0x01
__sfr __at(0x94) USBIE1;
#define bPUPIE 0x80
#define bOVERIE 0x40
#define bSETUPIE 0x10
#define bSOFIE 0x08
#define bRESMIE 0x04
#define bSUSPIE 0x02
#define bPBRSTIE 0x01
__sfr __at(0x95) USBIE2;
#define bOEP2IE 0x40
#define bOEP1IE 0x20
#define bOEP0IE 0x10
#define bIEP2IE 0x04
#define bIEP1IE 0x02
#define bIEP0IE 0x01
__sfr __at(0x96) USBADDR;
__sfr __at(0x97) EP0CON;
__sfr __at(0x99) EP1CON;
__sfr __at(0x9A) EP2CON;
/* Bits of EP0CON, EP1CON and EP2CON (EP0CON has no BUFSEL bits) */
#define bIEP_DTG 0x80
#define bOEP_DTG 0x40
#define bIEP_BUFSEL 0x20
#define bOEP_BUFSEL 0x10
#define bIEP_STL 0x08
#define bIEP_RDY 0x04
#define bOEP_STL 0x02
#define bOEP_RDY 0x01
__sfr __at(0x9B) IEP0CNT;
__sfr __at(0x9C) IEP1CNT;
__sfr __at(0x9D) IEP2CNT;
__sfr __at(0x9E) OEP0CNT;
__sfr __at(0x9F) OEP1CNT;
__sfr __at(0xA4) OEP2CNT;

/* EUART0 (table 6.14) */
__sfr __at(0xD8) SCON;
__sbit __at(0xDF) SM0;                  // SCON.7
__sbit __at(0xDE) SM1;                  // SCON.6
__sbit __at(0xDD) SM2;                  // SCON.5
__sbit __at(0xDC) REN;                  // SCON.4
__sbit __at(0xDB) TB8;                  // SCON.3
__sbit __at(0xDA) RB8;                  // SCON.2
__sbit __at(0xD9) TI;                   // SCON.1
__sbit __at(0xD8) RI;                   // SCON.0
__sfr __at(0xAA) SBUF;
__sfr __at(0xAB) SADDR;
__sfr __at(0xAC) SADEN;
__sfr __at(0xAD) SBRTH;
__sfr __at(0xAE) SBRTL;
__sfr __at(0xAF) SFINE;
#define bSBRTEN 0x80                    // SBRTH: baud rate generator on

/* PWM (chapter 8.2). Banks 0-3 are external SFRs at 0xFF80-0xFFFF (MOVX), bank 4 is in the SFR space. */
#define bPWM_EN 0x80                    // PWMx0CON only
#define bPWM_IE 0x40                    // PWMx0CON only
#define bPWM_IF 0x20                    // PWMx0CON only
#define bPWM_MOD 0x10                   // output level for duty 0
#define bPWM_SS 0x08                    // pin output on
#define MASK_PWM_CK 0x07                // PWMx0CON only: clock
__sfr __at(0xDA) PWM40CON;
__sfr __at(0xDB) PWM41CON;
__sfr __at(0xDC) PWM42CON;
__sfr __at(0xDD) PWM4PERDL;
__sfr __at(0xDE) PWM4PERDH;
__sfr __at(0xD2) PWM40DUTY1L;
__sfr __at(0xD3) PWM41DUTY1L;
__sfr __at(0xD4) PWM42DUTY1L;
__sfr __at(0xD5) PWM40DUTY1H;
__sfr __at(0xD6) PWM41DUTY1H;
__sfr __at(0xD7) PWM42DUTY1H;
__sfr __at(0xCE) PWM40DUTY2L;
__sfr __at(0xC1) PWM41DUTY2L;
__sfr __at(0xC2) PWM42DUTY2L;
__sfr __at(0xC3) PWM40DUTY2H;
__sfr __at(0xC4) PWM41DUTY2H;
__sfr __at(0xC5) PWM42DUTY2H;
#define PWM_XSFR(addr) (*(volatile __xdata unsigned char *)(addr))
#define PWM00CON PWM_XSFR(0xFF80)
#define PWM01CON PWM_XSFR(0xFF81)
#define PWM02CON PWM_XSFR(0xFF82)
#define PWM03CON PWM_XSFR(0xFF83)
#define PWM04CON PWM_XSFR(0xFF84)
#define PWM05CON PWM_XSFR(0xFF85)
#define PWM10CON PWM_XSFR(0xFF86)
#define PWM11CON PWM_XSFR(0xFF87)
#define PWM12CON PWM_XSFR(0xFF88)
#define PWM13CON PWM_XSFR(0xFF89)
#define PWM14CON PWM_XSFR(0xFF8A)
#define PWM15CON PWM_XSFR(0xFF8B)
#define PWM20CON PWM_XSFR(0xFF8C)
#define PWM21CON PWM_XSFR(0xFF8D)
#define PWM22CON PWM_XSFR(0xFF8E)
#define PWM23CON PWM_XSFR(0xFF8F)
#define PWM24CON PWM_XSFR(0xFF90)
#define PWM25CON PWM_XSFR(0xFF91)
#define PWM30CON PWM_XSFR(0xFF92)
#define PWM31CON PWM_XSFR(0xFF93)
#define PWM32CON PWM_XSFR(0xFF94)
#define PWM33CON PWM_XSFR(0xFF95)
#define PWM0PERDL PWM_XSFR(0xFF98)
#define PWM1PERDL PWM_XSFR(0xFF99)
#define PWM2PERDL PWM_XSFR(0xFF9A)
#define PWM3PERDL PWM_XSFR(0xFF9B)
#define PWM0PERDH PWM_XSFR(0xFF9C)
#define PWM1PERDH PWM_XSFR(0xFF9D)
#define PWM2PERDH PWM_XSFR(0xFF9E)
#define PWM3PERDH PWM_XSFR(0xFF9F)
#define PWM00DUTY1L PWM_XSFR(0xFFA0)
#define PWM01DUTY1L PWM_XSFR(0xFFA1)
#define PWM02DUTY1L PWM_XSFR(0xFFA2)
#define PWM03DUTY1L PWM_XSFR(0xFFA3)
#define PWM04DUTY1L PWM_XSFR(0xFFA4)
#define PWM05DUTY1L PWM_XSFR(0xFFA5)
#define PWM10DUTY1L PWM_XSFR(0xFFA6)
#define PWM11DUTY1L PWM_XSFR(0xFFA7)
#define PWM12DUTY1L PWM_XSFR(0xFFA8)
#define PWM13DUTY1L PWM_XSFR(0xFFA9)
#define PWM14DUTY1L PWM_XSFR(0xFFAA)
#define PWM15DUTY1L PWM_XSFR(0xFFAB)
#define PWM20DUTY1L PWM_XSFR(0xFFAC)
#define PWM21DUTY1L PWM_XSFR(0xFFAD)
#define PWM22DUTY1L PWM_XSFR(0xFFAE)
#define PWM23DUTY1L PWM_XSFR(0xFFAF)
#define PWM24DUTY1L PWM_XSFR(0xFFB0)
#define PWM25DUTY1L PWM_XSFR(0xFFB1)
#define PWM30DUTY1L PWM_XSFR(0xFFB2)
#define PWM31DUTY1L PWM_XSFR(0xFFB3)
#define PWM32DUTY1L PWM_XSFR(0xFFB4)
#define PWM33DUTY1L PWM_XSFR(0xFFB5)
#define PWM00DUTY1H PWM_XSFR(0xFFB8)
#define PWM01DUTY1H PWM_XSFR(0xFFB9)
#define PWM02DUTY1H PWM_XSFR(0xFFBA)
#define PWM03DUTY1H PWM_XSFR(0xFFBB)
#define PWM04DUTY1H PWM_XSFR(0xFFBC)
#define PWM05DUTY1H PWM_XSFR(0xFFBD)
#define PWM10DUTY1H PWM_XSFR(0xFFBE)
#define PWM11DUTY1H PWM_XSFR(0xFFBF)
#define PWM12DUTY1H PWM_XSFR(0xFFC0)
#define PWM13DUTY1H PWM_XSFR(0xFFC1)
#define PWM14DUTY1H PWM_XSFR(0xFFC2)
#define PWM15DUTY1H PWM_XSFR(0xFFC3)
#define PWM20DUTY1H PWM_XSFR(0xFFC4)
#define PWM21DUTY1H PWM_XSFR(0xFFC5)
#define PWM22DUTY1H PWM_XSFR(0xFFC6)
#define PWM23DUTY1H PWM_XSFR(0xFFC7)
#define PWM24DUTY1H PWM_XSFR(0xFFC8)
#define PWM25DUTY1H PWM_XSFR(0xFFC9)
#define PWM30DUTY1H PWM_XSFR(0xFFCA)
#define PWM31DUTY1H PWM_XSFR(0xFFCB)
#define PWM32DUTY1H PWM_XSFR(0xFFCC)
#define PWM33DUTY1H PWM_XSFR(0xFFCD)
#define PWM00DUTY2L PWM_XSFR(0xFFD0)
#define PWM01DUTY2L PWM_XSFR(0xFFD1)
#define PWM02DUTY2L PWM_XSFR(0xFFD2)
#define PWM03DUTY2L PWM_XSFR(0xFFD3)
#define PWM04DUTY2L PWM_XSFR(0xFFD4)
#define PWM05DUTY2L PWM_XSFR(0xFFD5)
#define PWM10DUTY2L PWM_XSFR(0xFFD6)
#define PWM11DUTY2L PWM_XSFR(0xFFD7)
#define PWM12DUTY2L PWM_XSFR(0xFFD8)
#define PWM13DUTY2L PWM_XSFR(0xFFD9)
#define PWM14DUTY2L PWM_XSFR(0xFFDA)
#define PWM15DUTY2L PWM_XSFR(0xFFDB)
#define PWM20DUTY2L PWM_XSFR(0xFFDC)
#define PWM21DUTY2L PWM_XSFR(0xFFDD)
#define PWM22DUTY2L PWM_XSFR(0xFFDE)
#define PWM23DUTY2L PWM_XSFR(0xFFDF)
#define PWM24DUTY2L PWM_XSFR(0xFFE0)
#define PWM25DUTY2L PWM_XSFR(0xFFE1)
#define PWM30DUTY2L PWM_XSFR(0xFFE2)
#define PWM31DUTY2L PWM_XSFR(0xFFE3)
#define PWM32DUTY2L PWM_XSFR(0xFFE4)
#define PWM33DUTY2L PWM_XSFR(0xFFE5)
#define PWM00DUTY2H PWM_XSFR(0xFFE8)
#define PWM01DUTY2H PWM_XSFR(0xFFE9)
#define PWM02DUTY2H PWM_XSFR(0xFFEA)
#define PWM03DUTY2H PWM_XSFR(0xFFEB)
#define PWM04DUTY2H PWM_XSFR(0xFFEC)
#define PWM05DUTY2H PWM_XSFR(0xFFED)
#define PWM10DUTY2H PWM_XSFR(0xFFEE)
#define PWM11DUTY2H PWM_XSFR(0xFFEF)
#define PWM12DUTY2H PWM_XSFR(0xFFF0)
#define PWM13DUTY2H PWM_XSFR(0xFFF1)
#define PWM14DUTY2H PWM_XSFR(0xFFF2)
#define PWM15DUTY2H PWM_XSFR(0xFFF3)
#define PWM20DUTY2H PWM_XSFR(0xFFF4)
#define PWM21DUTY2H PWM_XSFR(0xFFF5)
#define PWM22DUTY2H PWM_XSFR(0xFFF6)
#define PWM23DUTY2H PWM_XSFR(0xFFF7)
#define PWM24DUTY2H PWM_XSFR(0xFFF8)
#define PWM25DUTY2H PWM_XSFR(0xFFF9)
#define PWM30DUTY2H PWM_XSFR(0xFFFA)
#define PWM31DUTY2H PWM_XSFR(0xFFFB)
#define PWM32DUTY2H PWM_XSFR(0xFFFC)
#define PWM33DUTY2H PWM_XSFR(0xFFFD)
