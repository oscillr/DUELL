/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL TWO-CORE in a bench developer build (TC_AUTO_ARM=1, as DUELL_BENCH=1 sets it): the start arms itself at every power-on.
 * What must hold, so that an unproven second core cannot leave the owner with a dead unit:
 *   1 a power-on with nothing armed and no record: core 1 starts (armed by the build, tc.armed == TCW_AUTO), the record says RAN
 *   2 the next power-on after a power-off (RAN + power-on reset: OK): starts again
 *   3 the last session ended in a strike (WDT, CRASH): this power-on is ONE core (boot_last names it), the record forgets it, the next starts again
 *   4 the boot guard (the last start died within 30 s): one core, SKIPBOOT, no strike; the next power-on starts
 *   5 core 1 does not answer: tc_boot RETURNS (bounded), tc.up == 0, failed == NOANSWER, a strike; the unit goes on; the next power-on is one core
 *     (the strike forgotten), the one after tries again
 *   6 the one-shot record (tc_arm) still works and is not reported as AUTO
 *   7 the reasons list: names and strikes agree (the bench's log words)
 * Built and run by DUELL/host_tests.sh (name: tcbench) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TC_AUTO_ARM 1
#define TC_JOB_RUN host_job
static void host_job(uint32_t arg) { (void)arg; }
#include "twocore_hal_host.h"
#include "../firmware/src/twocore.c"

static int bad, ncheck;
static void expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("tcbench: FAIL %s\n", what);
        bad++;
    }
}
static void reset_world(void)
{
    memset(&tc, 0, sizeof tc);
    memset(&tc_nv, 0, sizeof tc_nv);
    tc_host_ld_hook = 0;
    tc_host_start_ok = 1;
}
static void next_boot(void)                             /* a new power-on: the volatile state is gone, the record (tc_nv) stays */
{
    memset(&tc, 0, sizeof tc);
    tc_host_start_ok = 1;
}
int main(void)
{
    uint32_t n;
    static const char *const nm[] = TCR_NAMES;

    reset_world();
    n = tc_host_starts;
    expect("1 nothing armed, no record: started by the build", tc_boot(1u, 0, 0) == 1 && tc.up && tc_host_starts == n + 1u && tc.armed == TCW_AUTO);
    expect("1 the record says RAN", tc_nv.state == TCNV_RAN);
    next_boot();
    expect("2 after a power-off (RAN + power-on reset): started again, no strike", tc_boot(1u, 0, 0) == 1 && tc.up && tc.boot_last == TCR_OK && tc.armed == TCW_AUTO);

    next_boot();
    expect("3 after a watchdog reset while it ran: ONE core, boot_last WDT", tc_boot(4u, 0, 0) == 0 && !tc.up && tc.boot_last == TCR_WDT && !tc.failed);
    expect("3 ..the strike is forgotten in the record", tc_nv.last == TCR_NONE && tc_nv.state == 0);
    next_boot();
    expect("3 ..and the next power-on starts again", tc_boot(1u, 0, 0) == 1 && tc.up);
    next_boot();
    expect("3 after a crash record while it ran: ONE core, boot_last CRASH", tc_boot(0u, 1, 0) == 0 && !tc.up && tc.boot_last == TCR_CRASH);
    next_boot();
    expect("3 ..the next starts", tc_boot(1u, 0, 0) == 1);

    next_boot();
    expect("4 the boot guard saw the last start die: ONE core, SKIPBOOT, not a strike",
           tc_boot(1u, 0, 1) == 0 && !tc.up && tc.boot_last == TCR_SKIPBOOT && !tc.failed && !tc.strike && tc_nv.last == TCR_SKIPBOOT && !tc_is_strike(tc_nv.last));
    next_boot();
    expect("4 ..the next power-on starts", tc_boot(1u, 0, 0) == 1 && tc.up);

    reset_world();
    tc_host_start_ok = 0;
    expect("5 core 1 does not answer: tc_boot returns 0 (no hang), core 1 not up", tc_boot(1u, 0, 0) == 0 && !tc.up);
    expect("5 ..failed NOANSWER, a strike, the record says so and is not RAN", tc.failed == TCR_NOANSWER && tc.strike && tc_nv.last == TCR_NOANSWER && tc_nv.state == 0);
    expect("5 ..core 1 is held and the watch has nothing to do (the unit goes on: tc_watch returns at once)", (tc_watch(1000u), tc_watch(5000u), !tc.up && tc.failed == TCR_NOANSWER));
    next_boot();
    expect("5 the next power-on: ONE core (the strike), boot_last NOANSWER, forgotten", tc_boot(1u, 0, 0) == 0 && tc.boot_last == TCR_NOANSWER && tc_nv.last == TCR_NONE);
    next_boot();
    expect("5 the one after tries again", tc_boot(1u, 0, 0) == 1 && tc.up);

    reset_world();
    tc_arm();
    expect("6 the one-shot record: started, reported as ONESHOT", tc_boot(1u, 0, 0) == 1 && tc.armed == TCW_ONESHOT);

    expect("7 the reason names cover the enum and the strikes are the ones of the design",
           sizeof nm / sizeof nm[0] == TCR_N && !strcmp(nm[TCR_NOANSWER], "NOANSWER") && !strcmp(nm[TCR_HANG], "HANG") && !strcmp(nm[TCR_FLASH], "FLASH")
           && tc_is_strike(TCR_HANG) && tc_is_strike(TCR_NOANSWER) && !tc_is_strike(TCR_OK) && !tc_is_strike(TCR_SKIPBOOT) && !tc_is_strike(TCR_RESET));

    printf("tcbench: %d checks, %d failed\n", ncheck, bad);
    return bad != 0;
}
