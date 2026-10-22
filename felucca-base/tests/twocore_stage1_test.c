/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL TWO-CORE stage 1: the protocol pieces of firmware/src/twocore.{h,c} on the computer, ONE thread (core 1's step is called by hand,
 * so every interleaving below is exact). It does not render audio and does not include the firmware: the runner and the two-thread
 * simulation with part 2's real stages are cloud job 5 (tests/twocore_sim.c). docs/TWOCORE_STAGE1.md.
 *   1 a job: claim, run, done, a beat; closed
 *   2 take before core 1 looked: yld, the job not run; take after it claimed: done, run once
 *   3 a check value that does not fit (stays wrong): refused, never run, a strike (BADJOB) at the next watch; one wrong read that fits at
 *     the re-read: run, counted as reread, no strike
 *   4 a stale done word (an older number): not done; a done word ahead of the last job: TCA_BAD, a strike (BADWORD)
 *   5 the heartbeat: idle beats keep it alive; no beat for more than 100 ms: HANG
 *   6 the read check: a pattern word read wrong: MISREAD
 *   7 the flash park: an idle core 1 parks at once (counted, no fault); a job left open: stopped after 20 ms (FLASH)
 *   8 the record across a reset: every case of tc_boot_reason and tc_boot (armed, not armed, a failed boot, no answer), which are strikes
 *   9 the stack mark
 * Built and run by DUELL/host_tests.sh (name: tcstage1) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define TC_JOB_RUN host_job
static void host_job(uint32_t arg);
#include "twocore_hal_host.h"
#include "../firmware/src/twocore.c"

static int bad, ncheck;
static void expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("tcstage1: FAIL %s\n", what);
        bad++;
    }
}
static uint32_t runs, last_arg;
static int take_in_job, claimed_seen;
static void host_job(uint32_t arg)                      /* (runs as "core 1"; take_in_job: core 0 writes take meanwhile) */
{
    runs++;
    last_arg = arg;
    if (take_in_job) {
        take_in_job = 0;
        claimed_seen = tc_answer(tc.job) == TCA_CLAIMED && !tc_closed();
        tc_take(tc.job);
    }
}
static tc_c1_t c1;
static void boot_up(void)                               /* a fresh power-on with an armed start */
{
    memset(&tc, 0, sizeof tc);
    memset(&tc_nv, 0, sizeof tc_nv);
    tc_host_ld_hook = 0;
    tc_host_start_ok = 1;
    tc_arm();
    tc_boot(1u, 0, 0);
    tc_c1_init(&c1);
    runs = 0;
}
/* fault injection through the hal's load hook */
static const uint32_t *f_word;
static uint32_t f_xor, f_count, f_val, f_set;
static uint32_t hook(const uint32_t *p, uint32_t v)
{
    if (p == f_word && f_count) {
        f_count--;
        return f_set ? f_val : v ^ f_xor;
    }
    return v;
}
static void inject(const uint32_t *w, uint32_t x, uint32_t n)
{
    f_word = w, f_xor = x, f_count = n, f_set = 0;
    tc_host_ld_hook = hook;
}
static void inject_val(const uint32_t *w, uint32_t v, uint32_t n)
{
    f_word = w, f_val = v, f_count = n, f_set = 1;
    tc_host_ld_hook = hook;
}

int main(void)
{
    uint32_t j, i, ms;
    tc_nv_t nv;

    /* 1 */
    boot_up();
    expect("1 armed boot: core 1 up, the record says RAN", tc.up && tc_nv.state == TCNV_RAN && tc.supply == (7u | 15u << 8));
    expect("1 nothing posted: closed", tc_closed());
    j = tc_post(0x21u);
    expect("1 posted: no answer yet, not closed", tc_answer(j) == TCA_NONE && !tc_closed());
    tc_c1_step(&c1);
    expect("1 core 1 ran it once with its argument", runs == 1u && last_arg == 0x21u);
    expect("1 done, closed, a beat", tc_answer(j) == TCA_DONE && tc_closed() && TC_LD(&tc_mb.beat) == 1u);
    tc_c1_step(&c1);
    expect("1 a second step runs nothing", runs == 1u);

    /* 2 */
    j = tc_post(0x22u);
    tc_take(j);
    tc_c1_step(&c1);
    expect("2 take before core 1 looked: yld, not run", tc_answer(j) == TCA_YIELD && runs == 1u && tc_closed());
    j = tc_post(0x23u);
    take_in_job = 1;                                    /* core 0 asks for it back while core 1 runs it (host_job) */
    tc_c1_step(&c1);
    expect("2 take after the claim: CLAIMED and not closed meanwhile, then done, run once", claimed_seen && tc_answer(j) == TCA_DONE && runs == 2u && tc_closed());

    /* 3 */
    j = tc_post(0x24u);
    inject(&tc_mb.chk, 1u, 1u);                         /* one wrong read: fits at the re-read */
    tc_c1_step(&c1);
    expect("3 one wrong read of the check: run, reread counted, not refused", runs == 3u && TC_LD(&tc_mb.reread) == 1u && !TC_LD(&tc_mb.bad));
    tc_watch(10u);
    expect("3 ... no strike", tc.up);
    j = tc_post(0x25u);
    inject(&tc_mb.chk, 0x100u, 100u);                   /* stays wrong */
    tc_c1_step(&c1);
    tc_host_ld_hook = 0;
    expect("3 a check value that stays wrong: refused, not run", runs == 3u && tc_answer(j) == TCA_REFUSED && TC_LD(&tc_mb.bad) == 1u && tc_closed());
    tc_watch(20u);
    expect("3 ... BADJOB at the watch, core 1 held, a strike", !tc.up && tc.failed == TCR_BADJOB && tc.strike && tc_host_live == 0 && tc_nv.last == TCR_BADJOB && tc_nv.state == 0);
    boot_up();
    j = tc_post(0x26u);
    inject(&tc_mb.job, 0x40u, 100u);                    /* the job word itself read wrong (not the next number) */
    tc_c1_step(&c1);
    tc_host_ld_hook = 0;
    expect("3 a job number read wrong: refused, not run", runs == 0u && TC_LD(&tc_mb.bad) == 1u);

    /* 4 */
    boot_up();
    j = tc_post(1u);
    tc_c1_step(&c1);
    j = tc_post(2u);
    expect("4 the done word of the job before (stale): not done", tc_answer(j) == TCA_NONE);
    inject_val(&tc_mb.done, j + 5u, 1000u);
    expect("4 a done word ahead of the last job: TCA_BAD", tc_answer(j) == TCA_BAD);
    tc_watch(5u);
    tc_host_ld_hook = 0;
    expect("4 ... BADWORD, a strike", !tc.up && tc.failed == TCR_BADWORD && tc.strike);

    /* 5 */
    boot_up();
    for (ms = 0; ms < 1000u; ms += 10u) {
        for (i = 0; i < 2u * TC_IDLE_BEAT; i++)
            tc_c1_step(&c1);
        tc_watch(ms);
    }
    expect("5 an idle core 1 that beats stays up for 1 s", tc.up);
    tc_watch(ms + 90u);
    expect("5 90 ms without a beat: still up", tc.up);
    tc_watch(ms + 120u);
    expect("5 more than 100 ms without a beat: HANG, a strike", !tc.up && tc.failed == TCR_HANG && tc.strike);

    /* 6 */
    boot_up();
    tc_post(1u);
    tc_c1_step(&c1);
    tc_watch(1u);
    expect("6 the pattern block read right: up", tc.up && !TC_LD(&tc_mb.misread));
    inject(&tc_pat[16], 4u, 1u);
    tc_post(2u);
    tc_c1_step(&c1);
    tc_host_ld_hook = 0;
    tc_watch(2u);
    expect("6 one pattern word read wrong: MISREAD, a strike", !tc.up && tc.failed == TCR_MISREAD && tc.strike);

    /* 7 */
    boot_up();
    tc_post(1u);
    tc_c1_step(&c1);
    tc_host_ticks = 0;
    tc_flash_park();
    expect("7 an idle core 1: parked at once, counted, no fault", tc.up && tc.park && tc.parks == 1u && tc_host_ticks < 100u);
    tc_flash_park();
    expect("7 a second access in the same pass: the same park", tc.parks == 1u);
    tc_watch(3u);
    expect("7 the watch ends the park", tc.up && !tc.park);
    tc_post(2u);                                        /* posted, core 1 never answers */
    tc_host_ticks = 0;
    tc_host_tick_step = 50u;
    tc_flash_park();
    tc_host_tick_step = 1u;
    expect("7 a job left open: stopped after 20 ms (FLASH), a strike", !tc.up && tc.failed == TCR_FLASH && tc.strike && tc_host_ticks >= TC_PARK_US * TC_TICKS_PER_US);

    /* 8 */
    nv.magic = TCNV_MAGIC, nv.state = TCNV_RAN, nv.last = 0;
    expect("8 RAN + watchdog: WDT", tc_boot_reason(&nv, 4u, 0) == TCR_WDT);
    expect("8 RAN + a crash record: CRASH", tc_boot_reason(&nv, 0u, 1) == TCR_CRASH);
    expect("8 RAN + a power-on: OK (power-off is the normal end)", tc_boot_reason(&nv, 1u, 0) == TCR_OK);
    expect("8 RAN + another reset: RESET", tc_boot_reason(&nv, 0x40u, 0) == TCR_RESET);
    nv.state = 0, nv.last = TCR_HANG;
    expect("8 not RAN: the last reason kept", tc_boot_reason(&nv, 1u, 0) == TCR_HANG);
    nv.magic = 0;
    expect("8 no record: NONE", tc_boot_reason(&nv, 1u, 0) == TCR_NONE);
    expect("8 strikes: WDT CRASH HANG MISREAD BADJOB BADWORD FLASH NOANSWER", tc_is_strike(TCR_WDT) && tc_is_strike(TCR_CRASH) && tc_is_strike(TCR_HANG) && tc_is_strike(TCR_MISREAD) &&
           tc_is_strike(TCR_BADJOB) && tc_is_strike(TCR_BADWORD) && tc_is_strike(TCR_FLASH) && tc_is_strike(TCR_NOANSWER));
    expect("8 not strikes: OK RESET SKIPBOOT NONE", !tc_is_strike(TCR_OK) && !tc_is_strike(TCR_RESET) && !tc_is_strike(TCR_SKIPBOOT) && !tc_is_strike(TCR_NONE));
    memset(&tc, 0, sizeof tc);
    memset(&tc_nv, 0, sizeof tc_nv);
    i = tc_host_starts;
    expect("8 not armed: no start", !tc_boot(1u, 0, 0) && tc_host_starts == i && !tc.up);
    tc_arm();
    expect("8 armed: started, RAN", tc_boot(1u, 0, 0) && tc_host_starts == i + 1u && tc_nv.state == TCNV_RAN);
    expect("8 the next power-on after a power-off: OK, not armed again (one-shot)", !tc_boot(1u, 0, 0) && tc_nv.last == TCR_OK && tc_host_starts == i + 1u);
    tc_arm();
    expect("8 armed but the last boot failed: not started, SKIPBOOT (no strike)", !tc_boot(1u, 0, 1) && tc_nv.last == TCR_SKIPBOOT && tc_host_starts == i + 1u);
    tc_arm();
    tc_host_start_ok = 0;
    expect("8 armed, no answer at start: NOANSWER, a strike", !tc_boot(1u, 0, 0) && tc.failed == TCR_NOANSWER && tc.strike && tc_nv.last == TCR_NOANSWER && tc_nv.state == 0);
    tc_host_start_ok = 1;
    tc_arm();
    tc_boot(1u, 0, 0);
    tc_hold_clean();
    expect("8 a clean hold: OK in the record, held", !tc.up && tc_nv.last == TCR_OK && tc_nv.state == 0);

    /* 9 */
    {
        static uint32_t stk[1088];
        tc_stack_fill(stk, 1088u);
        expect("9 a filled stack: peak 0", tc_stack_peak(stk, 1088u) == 0u);
        for (i = 1088u - 300u; i < 1088u; i++)
            stk[i] = i;
        expect("9 300 words used: 1200 bytes", tc_stack_peak(stk, 1088u) == 1200u);
    }

    printf("tcstage1: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
