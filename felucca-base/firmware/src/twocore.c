/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL TWO-CORE, stage 1: core 0's side (DUELL_TWOCORE=1 only; src/twocore.h has the words and core 1's side; docs/TWOCORE_STAGE1.md).
 * From the bench that ran on the owner's unit (wip/bench0226 src/bench.c, BENCH-B3): the record across a reset, the armed start, the
 * supply raise before the start, the heartbeat watch, the read check, the flash park; rewritten against the stage-1 words and without the
 * bench's screen. What is NOT here (cloud job 5): the runner (the hand-over every block, the deadline, the three cases, pausing the jobs),
 * the job itself (TC_JOB_RUN: part 2's stages), the strike counter's storage, the notice, the menu line.
 *
 * Who calls what (all core 0):
 *   power-on, interrupts off, before audio_init     tc_boot (main.c)
 *   the audio interrupt (the runner, job 5)         tc_post, tc_answer, tc_take, tc_closed, tc_fail; it must honour tc.park
 *   every main-loop pass                            tc_watch (main.c)
 *   before every flash read, erase or program       tc_flash_park (storage_hw.c)
 *   before an intended reset (an update)            tc_hold_clean (main.c)
 *   the developer page (local, stage 1)             tc_arm, then a restart */

#include "twocore.h"

static tc_t tc TC_BSS;          /* (tc_nv, the record across a reset: declared by the hal, twocore.h) */

/* ---------------------------------------------------------------- the hand-over's primitives (the runner's policy is job 5's) --- */
/* hand core 1 the next job (only when tc_closed(): the words of the job before are no longer read). Returns its number */
static TC_FN uint32_t tc_post(uint32_t arg)
{
    uint32_t j = tc.job + 1u;
    TC_ST(&tc_mb.arg, arg);
    TC_ST(&tc_mb.chk, TC_CHECK(j, arg));
    TC_SYNC();
    TC_ST(&tc_mb.job, j);                               /* (last: core 1 reads the other words after it) */
    TC_SYNC();
    tc.job = j;
    return j;
}
/* ask for job j back (core 1 answers yld if it had not claimed it, else it finishes it) */
static TC_FN void tc_take(uint32_t j)
{
    TC_ST(&tc_mb.take, j);
    TC_SYNC();
}
/* core 1's answer to job j so far. TCA_BAD: a word ahead of the last job posted (read or written wrong: a strike, TCR_BADWORD) */
static TC_FN uint32_t tc_answer(uint32_t j)
{
    uint32_t d, y, c, b;
    TC_SYNC();
    d = TC_LD(&tc_mb.done);
    y = TC_LD(&tc_mb.yld);
    c = TC_LD(&tc_mb.claim);
    b = TC_LD(&tc_mb.bad_job);
    if ((int32_t)(d - tc.job) > 0 || (int32_t)(y - tc.job) > 0 || (int32_t)(c - tc.job) > 0)
        return TCA_BAD;
    if (d == j)
        return TCA_DONE;
    if (y == j)
        return TCA_YIELD;
    if (b == j)
        return TCA_REFUSED;
    if (c == j)
        return TCA_CLAIMED;
    return TCA_NONE;
}
/* the last job posted is answered and over: core 1 is back in its waiting loop (in RAM) and reads nothing of part 2 */
static TC_FN int tc_closed(void)
{
    uint32_t a;
    if (!tc.up || !tc.job)
        return 1;
    a = tc_answer(tc.job);
    return a == TCA_DONE || a == TCA_YIELD || a == TCA_REFUSED;
}

/* ---------------------------------------------------------------- the safety net --- */
/* core 1 held for the rest of the session (it cannot be started again before the next power-on), the reason kept in the record */
static TC_FN void tc_fail(uint32_t why)
{
    TC_C1_HALT();
    tc.up = 0;
    tc.park = 0;
    tc.failed = (uint8_t)why;
    tc.strike = (uint8_t)tc_is_strike(why);
    tc_nv.magic = TCNV_MAGIC;
    tc_nv.state = 0;
    tc_nv.last = why;
}
/* an intended end (an update, a restart the owner asked for): held, the record says OK */
static TC_FN void tc_hold_clean(void)
{
    if (tc.up) {
        TC_C1_HALT();
        tc.up = 0;
        tc_nv.magic = TCNV_MAGIC;
        tc_nv.state = 0;
        tc_nv.last = TCR_OK;
    }
}
/* every main-loop pass: a refused job, a misread, no heartbeat for TC_HANG_MS: a strike, core 1 stopped. A flash park of this pass ends here */
static TC_FN void tc_watch(uint32_t now_ms)
{
    uint32_t b;
    if (!tc.up)
        return;
    TC_SYNC();
    if (TC_LD(&tc_mb.bad)) {
        tc_fail(TCR_BADJOB);
        return;
    }
    if (TC_LD(&tc_mb.misread)) {
        tc_fail(TCR_MISREAD);
        return;
    }
    if (tc.job && tc_answer(tc.job) == TCA_BAD) {
        tc_fail(TCR_BADWORD);
        return;
    }
    b = TC_LD(&tc_mb.beat);
    if (b != tc.beat) {
        tc.beat = b;
        tc.beat_t = now_ms;
    } else if (now_ms - tc.beat_t > TC_HANG_MS) {
        tc_fail(TCR_HANG);
        return;
    }
    tc.park = 0;                                        /* (the flash accesses of the pass before are over: jobs may go again) */
}
/* before every flash read, erase or program through the driver (storage_hw.c; the XIP map is off meanwhile, and part 2's job reads its
 * tables from the flash): the runner posts no job from now on (tc.park; it plays part 2 itself) and the last job must be over within
 * TC_PARK_US, else core 1 is stopped (TCR_FLASH). A park within the time is no fault (counted: tc.parks). The audio interrupt keeps running */
static TC_FN void tc_flash_park(void)
{
    uint32_t t0;
    if (!tc.up)
        return;
    if (!tc.park)
        tc.parks++;
    tc.park = 1;
    t0 = TC_TICKS();
    while (!tc_closed() && TC_TICKS() - t0 < TC_PARK_US * TC_TICKS_PER_US)
        TC_SYNC();
    if (!tc_closed())
        tc_fail(TCR_FLASH);
}

/* ---------------------------------------------------------------- power-on --- */
/* the last session's outcome from the record and the reset's cause (rst: fm1_boot.p3_rst, bit 0 power-on, bit 2 watchdog; crashed: a crash
 * record newer than the last boot saw). Pure: the computer's test walks every case */
static TC_FN uint32_t tc_boot_reason(const tc_nv_t *nv, uint32_t rst, int crashed)
{
    if (nv->magic != TCNV_MAGIC || nv->state != TCNV_RAN)
        return nv->magic == TCNV_MAGIC ? nv->last : TCR_NONE;
    if (rst & 4u)
        return TCR_WDT;                                 /* a hang while core 1 ran */
    if (crashed)
        return TCR_CRASH;
    if (rst & 1u)
        return TCR_OK;                                  /* a power-off: the normal end */
    return TCR_RESET;                                   /* another reset (not counted as a fault) */
}
/* power-on, interrupts off, before audio_init (main.c). armed: a start is wanted (stage 1: the record's one-shot ARMED, as BENCH-B3; from
 * stage 5 the 2ND CORE setting and fewer than 3 strikes: job 5 leaves this as it is). boot_failed: the boot guard saw the last start die
 * in its first 30 s (one core this time, no strike: ChoralRoot's rule). Returns 1 when core 1 runs */
static TC_FN int tc_boot(uint32_t rst, int crashed, int boot_failed)
{
    uint32_t i, t0, armed;
    if (tc_nv.magic != TCNV_MAGIC) {
        tc_nv.magic = TCNV_MAGIC;
        tc_nv.state = 0;
        tc_nv.last = TCR_NONE;
    }
    tc_nv.last = tc_boot_reason(&tc_nv, rst, crashed);
    tc.boot_last = (uint8_t)tc_nv.last;
    armed = tc_nv.state == TCNV_ARMED;
    tc.armed = armed ? TCW_ONESHOT : TCW_NOT;
#if TC_AUTO_ARM
    if (!armed && tc_auto_arm_ok(tc_nv.last)) {
        armed = 1;
        tc.armed = TCW_AUTO;
    } else if (!armed)
        tc_nv.last = TCR_NONE;                          /* a strike last time: one core now, forgotten, the next power-on tries again */
#endif
    if (armed && boot_failed) {
        tc_nv.last = TCR_SKIPBOOT;
        tc.boot_last = TCR_SKIPBOOT;
    }
    tc_nv.state = 0;
    if (!armed || boot_failed)
        return 0;
    tc_nv.state = TCNV_RAN;                             /* (first: a crash in the start itself is found at the next power-on) */
    tc_nv.last = TCR_NONE;
    for (i = 0; i < TC_PAT_N; i++)
        tc_pat[i] = TC_PAT(i);
    tc.supply = TC_SUPPLY_STOCK();                      /* M-VAVE's stock levels before core 1 (hal/fm1_sys.h) */
    t0 = TC_TICKS();
    tc.up = (uint8_t)(TC_C1_START() == 0);
    tc.start_us = (TC_TICKS() - t0) / TC_TICKS_PER_US;
    tc.job = 0;
    tc.park = 0;
    tc.failed = 0;
    tc.strike = 0;
    TC_SYNC();
    tc.beat = TC_LD(&tc_mb.beat);
    tc.beat_t = 0;
    if (!tc.up) {
        tc_nv.state = 0;
        tc_nv.last = TCR_NOANSWER;
        tc.failed = TCR_NOANSWER;
        tc.strike = 1;
    }
    return tc.up;
}
/* arm a start for the next power-on (stage 1's developer page; the caller then restarts the unit) */
static TC_FN void tc_arm(void)
{
    tc_nv.magic = TCNV_MAGIC;
    tc_nv.state = TCNV_ARMED;
}
