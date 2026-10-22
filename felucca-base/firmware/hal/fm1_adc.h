/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM-1 SARADC: PB6 = MASTER pot (ch4),
 * PB1 = battery divider (ch3). Polled, 10 bit, from the main loop.
 *
 *   fm1_adc_init()        both pins analog inputs, no pulls
 *   fm1_adc_read(ch)      0..1023, -1 on timeout
 */
#pragma once
#include <stdint.h>
#include "fm1_gpio.h"

#ifndef FM1_ADC_CON
#define FM1_ADC_CON (*(volatile uint32_t *)0x13100u)
#endif
#ifndef FM1_ADC_RES
#define FM1_ADC_RES (*(volatile uint32_t *)0x13104u)
#endif
#ifndef FM1_WLA_CON0
#define FM1_WLA_CON0 (*(volatile uint32_t *)0x11900u)
#endif
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };

/* Both are only ever analog inputs: PB1 held low for 8 s would reset the
 * chip (isd_config). */
static void fm1_adc_init(void)
{
    uint32_t m = (1u << 6) | (1u << 1);
    FM1_PR(FM1_PB, FM1_DIE) &= ~m;
    FM1_PR(FM1_PB, FM1_PU) &= ~m;
    FM1_PR(FM1_PB, FM1_PD) &= ~m;
    FM1_PR(FM1_PB, FM1_DIR) |= m;
}

static int32_t fm1_adc_read(uint32_t ch)
{
    uint32_t t, v;
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = 0xF04Eu | ((ch & 0xFu) << 8);
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
    for (t = 0; t < 20000u; t++)
        if (FM1_ADC_CON & 0x80u)
            break;
    v = FM1_ADC_RES & 0x3FFu;
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    return t == 20000u ? -1 : (int32_t)v;
}

#if DUELL_VELO && !defined(FM1_ADC_NB_EXT)
/* After Velo's method (Charles Vestal, github.com/charlesvestal/fm1-velocity, GPL-3.0-only): the converter started in one scan tick and
 * taken in the next, nothing spins and waits. There is ONE converter: while the scan tick uses it (hal/fm1_input.h, DUELL_VELO) nothing
 * else may call fm1_adc_read (the pot and the battery are read in the tick's slots then).
 *   fm1_adc_start(ch, con)  con = the CON word without the channel (sense_cal.h SC_ADC_CON_KEY / _SLOW: clock divider, start delay)
 *   fm1_adc_done()          1 when the conversion has finished
 *   fm1_adc_take()          0..1023, and the converter off
 *   fm1_adc_abort()         the converter off, the reading dropped
 * The register sequence is that of fm1_adc_read (tests/velo_adc_test.c compares them). */
static inline void fm1_adc_start(uint32_t ch, uint32_t con)
{
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = con | ((ch & 0xFu) << 8);
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
}
static inline uint32_t fm1_adc_done(void) { return (FM1_ADC_CON & 0x80u) != 0u; }
static inline uint32_t fm1_adc_take(void)
{
    uint32_t v = FM1_ADC_RES & 0x3FFu;
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    return v;
}
static inline void fm1_adc_abort(void)
{
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
}
#endif
