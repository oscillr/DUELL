/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL TWO-CORE: the reasons core 1 stopped or did not start, and which count as strikes. Plain C with no hal, so that twocore.h, the
 * bench's log (bench.c) and the Mac twin (tests/bench_sim.c) share one list. */
#ifndef TWOCORE_REASONS_H
#define TWOCORE_REASONS_H
#include <stdint.h>
/* why core 1 stopped or did not start (the record across a reset keeps the last one; tc_is_strike says which count as a fault of core 1:
 * docs/TWOCORE_STAGE1.md section 3) */
enum { TCR_NONE, TCR_OK, TCR_NOANSWER, TCR_HANG, TCR_MISREAD, TCR_BADJOB, TCR_BADWORD, TCR_CRASH, TCR_WDT, TCR_RESET, TCR_SKIPBOOT, TCR_FLASH, TCR_N };
static inline int tc_is_strike(uint32_t r)
{
    return r == TCR_NOANSWER || r == TCR_HANG || r == TCR_MISREAD || r == TCR_BADJOB || r == TCR_BADWORD || r == TCR_CRASH || r == TCR_WDT || r == TCR_FLASH;
}

/* names of the reasons, for the bench's log (same order as the enum) */
#define TCR_NAMES { "none", "ok", "NOANSWER", "HANG", "MISREAD", "BADJOB", "BADWORD", "CRASH", "WDT", "RESET", "SKIPBOOT", "FLASH" }

/* TC_AUTO_ARM (DUELL 1.0: set in every DUELL_TWOCORE build, the release included, and in a DUELL_BENCH developer build): tc_boot arms the start itself at every power-on, so the owner needs
 * no developer page and the Mac needs no restart. It does NOT arm when the last session ended in a strike (tc_is_strike: core 1 crashed,
 * hung or answered wrong): that power-on runs on one core, the record forgets the strike, and the next power-on tries again. The boot
 * guard (a start that died within 30 s) still wins: one core that time (SKIPBOOT). A core 1 that does not answer is NOANSWER: one core,
 * the unit boots and plays as a normal one-core DUELL. */
#ifndef TC_AUTO_ARM
#if (defined(DUELL_BENCH) && DUELL_BENCH) || (defined(DUELL_TWOCORE) && DUELL_TWOCORE)
#define TC_AUTO_ARM 1
#else
#define TC_AUTO_ARM 0
#endif
#endif
enum { TCW_NOT, TCW_ONESHOT, TCW_AUTO };               /* how the start was armed this power-on (tc.armed) */
static inline int tc_auto_arm_ok(uint32_t last) { return !tc_is_strike(last); }

#endif
