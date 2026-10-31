/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* Item 92: the unit started on ONE core after the installer's ERASE. The chain, run here with the firmware's own lines (bootguard.h = main.c's
 * boot guard, twocore.c tc_boot = the start of core 1): installer flash -> first start (pending set) -> ERASE DATA inside 30 s -> restart
 * (main.c erase_restart_due) -> next start. Core 1 must run at that start.
 *   -DOLD_ERASE   the restart as test4 did it (pending left set): the test must FAIL (this is the proof that it catches the cause)
 * Built and run by DUELL/host_tests.sh (name: bootguard) */
#define DUELL_TWOCORE 1                                 /* TC_AUTO_ARM, as in the release build */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TC_JOB_RUN host_job
static void host_job(uint32_t arg) { (void)arg; }
#include "twocore_hal_host.h"
#include "../firmware/src/twocore.c"
#include "../firmware/src/bootguard.h"
#define BOOTGUARD_MAGIC 0x42475244u
static struct { uint32_t magic, failed, pending; } bootguard;
static int bad, ncheck;
static void expect(const char *w, int ok) { ncheck++; if (!ok) { printf("bootguard: FAIL %s\n", w); bad++; } }
/* one start: the boot guard, then tc_boot as main.c calls it; rst bit 0 = power-on. Returns 1 when core 1 runs */
static int start(uint32_t rst, int *uboot)
{
    int enter;
    BG_START(bootguard, BOOTGUARD_MAGIC, enter);
    *uboot = enter;
    memset(&tc, 0, sizeof tc);
    tc_host_start_ok = 1;
    return tc_boot(rst, 0, bootguard.failed != 0) && tc.up;
}
static void erase_restart(void)
{
#ifdef OLD_ERASE
    /* test4: fm1_reboot() with pending still set */
#else
    BG_ERASE_ALL(bootguard);                            /* main.c's erase_restart_due lines, whole */
    tc_forget();
#endif
}
int main(void)
{
    int ub;
    /* power-on, nothing in RAM */
    memset(&tc_nv, 0, sizeof tc_nv);
    memset(&bootguard, 0, sizeof bootguard);
    expect("first power-on: core 1 runs", start(1u, &ub) && !ub);
    /* ERASE DATA 10 s later: the chip resets (RAM kept) */
    erase_restart();
    expect("after the ERASE restart: core 1 runs (two cores)", start(0u, &ub) && !ub);
    expect("tc_boot did not say SKIPBOOT", tc.boot_last != TCR_SKIPBOOT);
    /* a real failed start still counts: a reset without saying so, twice, goes to the update mode and the first one skips core 1 */
    memset(&bootguard, 0, sizeof bootguard);
    memset(&tc_nv, 0, sizeof tc_nv);
    start(1u, &ub);                                     /* start, dies inside 30 s (no meant reset) */
    expect("a start after a died start: one core (the guard still works)", !start(0u, &ub));
    expect("... SKIPBOOT recorded", tc.boot_last == TCR_SKIPBOOT);
    expect("two died starts: update mode", (start(0u, &ub), ub));
    /* a session that lived 30 s clears pending (main.c): the next start runs two cores again */
    memset(&bootguard, 0, sizeof bootguard);
    start(1u, &ub);
    bootguard.pending = 0; bootguard.failed = 0;
    expect("after a 30 s session: two cores", start(1u, &ub));
    /* addendum 2: a strike recorded, the update-mode counter at 1, then ERASE -> restart: both cores start, everything wiped */
    memset(&bootguard, 0, sizeof bootguard);
    memset(&tc_nv, 0, sizeof tc_nv);
    start(1u, &ub);
    tc_nv.magic = TCNV_MAGIC; tc_nv.state = 0; tc_nv.last = TCR_HANG;   /* core 1 hung in that session: a strike, one core at the next start */
    bootguard.failed = 1; bootguard.pending = 1;
    expect("a strike (HANG) no longer skips the try: the next power-on starts both cores, the reason stays in boot_last", start(1u, &ub) && tc.boot_last == TCR_HANG);
    tc_nv.magic = TCNV_MAGIC; tc_nv.state = 0; tc_nv.last = TCR_NOANSWER;
    bootguard.failed = 1; bootguard.pending = 1;
    erase_restart();
#ifndef OLD_ERASE
    expect("ERASE clears the second-core record", tc_nv.magic == 0 && tc_nv.state == 0 && tc_nv.last == TCR_NONE);
    expect("ERASE clears the boot guard and its update-mode counter", bootguard.magic == 0 && bootguard.pending == 0 && bootguard.failed == 0);
#endif
    expect("strike recorded -> ERASE -> restart: both cores start", start(0u, &ub) && !ub && tc.boot_last == TCR_NONE);
    expect("... no update mode, counter at 0", bootguard.failed == 0);
    /* addendum 3: every strike kind, the next power-on tries two cores; strike + two died starts: update mode as before */
    {
        uint32_t k, strikes[] = {TCR_NOANSWER, TCR_HANG, TCR_MISREAD, TCR_BADJOB, TCR_BADWORD, TCR_FLASH};
        int all = 1;
        for (k = 0; k < sizeof strikes / sizeof *strikes; k++) {
            memset(&bootguard, 0, sizeof bootguard);
            tc_nv.magic = TCNV_MAGIC; tc_nv.state = 0; tc_nv.last = strikes[k];
            all &= start(1u, &ub) && !ub && tc.boot_last == strikes[k];
        }
        expect("every strike kind: the next power-on starts both cores", all);
        tc_nv.magic = TCNV_MAGIC; tc_nv.state = TCNV_RAN; tc_nv.last = TCR_NONE;
        memset(&bootguard, 0, sizeof bootguard);
        expect("WDT strike (hang on core 0 while core 1 ran): both cores tried", start(4u, &ub) && tc.boot_last == TCR_WDT);
        memset(&bootguard, 0, sizeof bootguard);
        tc_nv.magic = TCNV_MAGIC; tc_nv.state = 0; tc_nv.last = TCR_HANG;
        start(1u, &ub);                                 /* the start after the strike dies inside 30 s */
        expect("strike + a died start: the guard skips core 1 (SKIPBOOT, no crash loop)", !start(0u, &ub) && tc.boot_last == TCR_SKIPBOOT);
        expect("strike + two died starts: update mode as before", (start(0u, &ub), ub));
    }
    printf("bootguard: %d checks, %d failed\n", ncheck, bad);
    return bad != 0;
}
