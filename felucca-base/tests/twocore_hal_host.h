/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL TWO-CORE: the computer half of the hal of firmware/src/twocore.h (the unit's half: firmware/hal/fm1_core1.h). Include this, then
 * firmware/src/twocore.c. docs/TWOCORE_STAGE1.md.
 *   TC_LD / TC_ST   C11 atomics, acquire loads and release stores (stronger than the unit needs: ThreadSanitizer does not model a fence on
 *                   its own, so the edges must sit on the words for part 2's plain data to be seen as ordered); every load passes tc_host_ld_hook
 *                   if one is set (the fault injection: a word read wrong, a stale word, a torn job), every store tc_host_st_hook (it returns the value
 *                   stored: a word written wrong)
 *   TC_SYNC         a full fence (stronger than the unit's csync is known to be: a test that needs less must say so)
 *   TC_TICKS        a simulated clock: tc_host_ticks, which the test advances (core 1's reads never move it) (tc_host_tick_step is added at every read, so a wait loop ends)
 *                   tc_host_tick_div (job 5): 1 every read moves the clock; n > 1 only every n-th read (a wait loop of core 0 then needs n times the reads to
 *                   pass a given time); 0 the clock stands still. tc_host_stop != 0: the clock does not pass that tick. The simulation moves the clock only while a delay it injected is running, so a job is
 *                   "in time" whatever the machine's speed and the scheduler
 *   tc_host_is_c1   (thread-local, the thread test sets it on core 1's thread) after a halt core 1's stores are dropped, as a halted core writes nothing
 *   TC_C1_START / TC_C1_HALT / TC_SUPPLY_STOCK   flags: tc_host_start_ok decides the start's answer; halts are counted. The start does what
 *                   the unit's fm1_cpu1_start does to the words (all 0), sets alive, and calls tc_host_c1_started if set (a thread test starts
 *                   its core-1 thread there) */
#pragma once
#include <stdint.h>
#include <string.h>

static uint32_t (*tc_host_ld_hook)(const uint32_t *p, uint32_t v);   /* returns the value core "reads" (v: the true one) */
static uint32_t (*tc_host_st_hook)(uint32_t *p, uint32_t v);
static __thread int tc_host_is_c1;
static int tc_host_live;
static inline uint32_t tc_host_ld(const uint32_t *p)
{
    uint32_t v = __atomic_load_n(p, __ATOMIC_ACQUIRE);
    return tc_host_ld_hook ? tc_host_ld_hook(p, v) : v;
}
static inline void tc_host_st(uint32_t *p, uint32_t v)
{
    if (tc_host_is_c1 && !__atomic_load_n(&tc_host_live, __ATOMIC_RELAXED))
        return;                                         /* (a halted core 1 writes nothing) */
    if (tc_host_st_hook)
        v = tc_host_st_hook(p, v);
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}
static uint32_t tc_host_ticks, tc_host_tick_step = 1u, tc_host_tick_div = 1u, tc_host_reads, tc_host_stop;
static inline uint32_t tc_host_now(void)                /* (core 0's thread is the only writer of the clock) */
{
    uint32_t d = __atomic_load_n(&tc_host_tick_div, __ATOMIC_RELAXED), t = __atomic_load_n(&tc_host_ticks, __ATOMIC_RELAXED), stop;
    if (tc_host_is_c1)
        return t;                                       /* (core 1 only reads the clock, as it does on the unit: BENCHT2's job timestamps) */
    if (d != 1u && (!d || ++tc_host_reads % d))
        return t;
    t += tc_host_tick_step;
    stop = __atomic_load_n(&tc_host_stop, __ATOMIC_ACQUIRE);
    if (stop && (int32_t)(t - stop) > 0)
        t = stop;                                       /* (the clock does not pass tc_host_stop: a thread test holds time at the tick it is waiting for) */
    __atomic_store_n(&tc_host_ticks, t, __ATOMIC_RELAXED);
    return t;
}
static int tc_host_start_ok = 1;
static uint32_t tc_host_halts, tc_host_starts;
static void (*tc_host_c1_started)(void);
static int tc_host_start(void);
static inline void tc_host_halt(void) { __atomic_store_n(&tc_host_live, 0, __ATOMIC_RELEASE); tc_host_halts++; }
#define TC_LD(p) tc_host_ld((const uint32_t *)(p))
#define TC_ST(p, v) tc_host_st((uint32_t *)(p), (uint32_t)(v))
#define TC_SYNC() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define TC_TICKS() tc_host_now()
#define TC_TICKS_PER_US 24u
#define TC_C1_START() tc_host_start()
#define TC_C1_HALT() tc_host_halt()
#define TC_SUPPLY_STOCK() (7u | 15u << 8)
#include "../firmware/src/twocore.h"
static tc_nv_t tc_nv;                                   /* (the unit's is in .noinit: hal/fm1_core1.h) */
static int tc_host_start(void)
{
    uint32_t i, *w = (uint32_t *)&tc_mb;
    tc_host_starts++;
    for (i = 0; i < sizeof tc_mb / 4u; i++)
        __atomic_store_n(&w[i], 0u, __ATOMIC_RELAXED);
    if (!tc_host_start_ok)
        return -1;
    __atomic_store_n(&tc_host_live, 1, __ATOMIC_RELEASE);
    __atomic_store_n(&tc_mb.alive, 1u, __ATOMIC_RELAXED);
    if (tc_host_c1_started)
        tc_host_c1_started();
    return 0;
}
