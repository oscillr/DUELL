/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* Tests of KEY SENSE, after Velo's method (Charles Vestal, github.com/charlesvestal/fm1-velocity, GPL-3.0-only; see firmware/src/sense.c). */
/* hal/fm1_adc.h: the non-blocking converter (fm1_adc_start / done / take / abort, DUELL_VELO) writes the registers in the order and with the
 * values of the blocking fm1_adc_read for the same channel and CON word, with the polling loop between them left out. The registers are
 * variables; every access to one logs the value the previous write left in it, so the log is the sequence of states the converter saw.
 * What the real converter does with them (its timing, the divider, the start delay) cannot be tested here. */
#define DUELL_VELO 1
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
static volatile uint32_t r_con, r_res = 0x2A5, r_wla = 1u << 14;
static uint32_t log_[256][2], nlog, polls;
static volatile uint32_t *reg(int which)
{
    static uint32_t last[3] = {0, 0, 0};
    volatile uint32_t *p = which == 0 ? &r_con : which == 1 ? &r_res : &r_wla;
    if (which != 1 && nlog < 256u && *p != last[which]) {                   /* a write happened since the last access (the result register is only read) */
        log_[nlog][0] = (uint32_t)which;
        log_[nlog][1] = *p;
        nlog++;
    }
    last[which] = *p;
    if (which == 0 && (r_con & 0x50u) == 0x50u && ++polls > 3u)   /* started: done after a few polls */
        r_con |= 0x80u;
    return p;
}
#define FM1_ADC_CON (*reg(0))
#define FM1_ADC_RES (*reg(1))
#define FM1_WLA_CON0 (*reg(2))
#include "../firmware/hal/fm1_adc.h"

int main(void)
{
    uint32_t a[256][2], na, i, ok = 1, v1, v2, fails = 0;
    const uint32_t con = 0x204Eu;
    r_con = 0;
    r_wla = 1u << 14;
    nlog = polls = 0;
    memset(log_, 0, sizeof log_);
    /* blocking, with the same CON word as the non-blocking one will be given */
    v1 = (uint32_t)fm1_adc_read(FM1_ADC_MASTER);
    (void)reg(0);
    (void)reg(2);
    na = nlog;
    memcpy(a, log_, sizeof a);
    r_con = 0;
    r_wla = 1u << 14;
    nlog = polls = 0;
    memset(log_, 0, sizeof log_);
    fm1_adc_start(FM1_ADC_MASTER, 0xF04Eu);
    while (!fm1_adc_done())
        ;
    v2 = fm1_adc_take();
    (void)reg(0);
    (void)reg(2);
    if (getenv("VELO_DEBUG")) {
        for (i = 0; i < na; i++)
            printf("blocking %u: reg %u = %05x\n", i, a[i][0], a[i][1]);
        for (i = 0; i < nlog; i++)
            printf("nonblock %u: reg %u = %05x\n", i, log_[i][0], log_[i][1]);
    }
    if (na != nlog)
        ok = 0;
    for (i = 0; i < na && i < nlog; i++)
        if (a[i][0] != log_[i][0] || a[i][1] != log_[i][1])
            ok = 0;
    printf("%-86s %s\n", "start / done / take writes the registers as the blocking read does (same CON word)", ok ? "ok" : "FAIL");
    fails += !ok;
    printf("%-86s %s\n", "  .. and returns the same value", v1 == v2 && v2 == 0x2A5u ? "ok" : "FAIL");
    fails += !(v1 == v2 && v2 == 0x2A5u);
    r_con = 0;
    nlog = polls = 0;
    fm1_adc_start(4, con);
    ok = ((r_con & 0xF00u) >> 8) == 4u && (r_con & 0xF000u) == (con & 0xF000u) && (r_con & 0x50u) == 0x50u;
    printf("%-86s %s\n", "the channel goes in bits 8..11, the CON word's start delay in 12..15, enable and start set", ok ? "ok" : "FAIL");
    fails += !ok;
    fm1_adc_abort();
    ok = r_con == 0u;
    printf("%-86s %s\n", "abort leaves the converter off", ok ? "ok" : "FAIL");
    fails += !ok;
    printf("velo adc: %u failed\n", fails);
    return fails != 0;
}
