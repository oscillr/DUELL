/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL TWO-CORE, stage 1 (DUELL_TWOCORE=1 only; the normal build has none of it): the hand-over between core 0 and core 1 as plain C that
 * compiles on the unit and on a computer. docs/TWOCORE_STAGE1.md is the description; this comment is the short form.
 *
 * THE WORDS (tc_mb, one object, two 64-byte lines; each word has ONE writer; every access is one aligned 32-bit load or store; no 64-bit
 * word, no read-modify-write of a shared word, no atomic swap between the cores is assumed: whether the chip has one is not known):
 *   core 0 writes   job    the job number (1, 2, 3 ..: +1 for every job posted, never reused within a power-on), written LAST
 *                   arg    what to do (the runner's encoding: the block, the part)          written before job
 *                   chk    TC_CHECK(job, arg)                                               written before job
 *                   take   "core 0 asks for job N back": N
 *   core 1 writes   alive  1 once it runs its waiting loop
 *                   claim  N: "I play job N"  (core 1 decided to run it)
 *                   yld    N: "you play job N" (core 1 saw take == N before it claimed it: it never touches job N)
 *                   done   N: job N finished (part 2's sums for that block are written)
 *                   beat   the heartbeat: +1 after every job and every TC_IDLE_BEAT polls of the waiting loop
 *                   bad, bad_job   jobs refused (a check value or a number that does not fit) and the last refused number
 *                   misread        words of the pattern block (tc_pat) read wrong (16 a job)
 *                   reread         jobs whose words fitted only at the second or third read (counted, not refused: see tc_c1_step)
 * Core 1 is the ONLY one that decides who plays a job: it answers every job exactly once, with claim (then done) or yld, or refuses it
 * (bad_job). Core 0 never concludes "core 1 has not started" from a missing claim: it writes take and waits for the answer. So no
 * store-to-load order between the cores is needed, only that each core's stores arrive in the order made by TC_SYNC (message passing).
 * The sequence number makes every word compare by equality with the job number: a stale word (an older number) is simply "not yet".
 *
 * THE HAL (defined before this file is included; device: hal/fm1_core1.h; computer: tests/twocore_hal_host.h)
 *   TC_LD(p) TC_ST(p, v)   one 32-bit load / store of a shared word (volatile on the unit; atomics and fault hooks on the computer)
 *   TC_SYNC()              orders this core's loads and stores (csync on the unit)
 *   TC_TICKS() TC_TICKS_PER_US   core 0's free-running clock (TIMER4, 24 a us)
 *   TC_C1_START() TC_C1_HALT() TC_SUPPLY_STOCK()   start core 1 (power-on only; 0 = it answered), hold it, raise the supplies
 *   TC_C1_INLINE           how core 1's step is compiled (always inlined into the waiting loop in RAM on the unit)
 *   TC_FN TC_MB_ATTR TC_BSS   placement of core 0's code, the words, core 0's state
 *   tc_nv                  the record across a reset (a tc_nv_t; on the unit in .noinit, declared by hal/fm1_core1.h by name, so that
 *                          tools/elfcheck.py's source scan sees it) */
#pragma once
#include <stdint.h>

#ifndef TC_FN
#define TC_FN
#endif
#ifndef TC_BSS
#define TC_BSS
#endif
#ifndef TC_MB_ATTR
#define TC_MB_ATTR __attribute__((aligned(64)))
#endif
#ifndef TC_C1_INLINE
#define TC_C1_INLINE static inline
#endif

/* BENCHT2 (docs/BENCH_TWOCORE2.md): core 1 timestamps every job it runs and core 0 turns them into the diagnosis counters (twocore_run.c tcm_*). Developer
 * bench builds only (DUELL_BENCH); a normal build, and every TWOCORE build without the bench, have none of it. A test may define TC_MEAS itself. */
#ifndef TC_MEAS
#if defined(DUELL_BENCH) && DUELL_BENCH
#define TC_MEAS 1
#else
#define TC_MEAS 0
#endif
#endif

#define TC_MAGIC 0x5EA1C0DEu
#define TC_CHECK(job, arg) ((uint32_t)(job) * 0x9E3779B1u ^ (uint32_t)(arg) * 0x85EBCA6Bu ^ TC_MAGIC)   /* one bit wrong in job, arg or chk: no fit */
#define TC_IDLE_BEAT 4096u          /* waiting-loop polls a beat (a power of 2) */
#define TC_HANG_MS 100u             /* no beat for this long: a hang (a strike) */
#define TC_PARK_US 20000u           /* a flash access: core 1 must be idle within this, else it is stopped (a strike) */
#define TC_REREAD 2u                /* a job whose words do not fit is read again this many times before it is refused */
#define TC_PAT_N 64u                /* the pattern block core 1 re-reads (16 words a job; a power of 2). Memory trim 1: 64 words = 256 B (was 256 words = 1,024 B): still four different
                                      * blocks of 16 words, so a job reads a different quarter each time and every word is read again about every fourth job */
#define TC_PAT(i) ((uint32_t)(i) * 0x9E3779B1u ^ 0x5A17C3E1u)

typedef struct {
    uint32_t job, arg, chk, take;                       /* line 0: core 0 writes */
    uint32_t pad0[12];
    uint32_t alive, claim, yld, done;                   /* line 1: core 1 writes */
    uint32_t beat, bad, bad_job, misread;
    uint32_t reread;
#if TC_MEAS
    uint32_t t_seen, t_end, mj;                         /* core 1's clock when it saw job mj, and when its job returned (written before done) */
    uint32_t pad1[4];
#else
    uint32_t pad1[7];
#endif
} tc_mb_t;
_Static_assert(sizeof(tc_mb_t) == 128, "twocore.h: two 64-byte lines");
static tc_mb_t tc_mb TC_MB_ATTR;
static uint32_t tc_pat[TC_PAT_N] TC_BSS;               /* written once by core 0 at power-on (tc_boot), then only read */

#include "twocore_reasons.h"

/* core 1's job: part 2's stages for one block (the runner defines it: docs/TWOCORE_STAGE1.md, cloud job 5). The computer's tests may name
 * another function with TC_JOB_RUN */
#ifndef TC_JOB_RUN
#define TC_JOB_RUN tc_job_run
static void tc_job_run(uint32_t arg);
#endif

/* ---------------------------------------------------------------- core 1's side --- */
typedef struct {
    uint32_t last;              /* the last job number it answered (or refused) */
    uint32_t polls, beat, bad, mis, rr, k;
} tc_c1_t;                      /* core 1's own loop state: on core 1's stack (hal/fm1_core1.h fm1_c1_main), never shared */

static inline void tc_c1_init(tc_c1_t *c)
{
    c->last = TC_LD(&tc_mb.job);
    c->polls = c->beat = c->bad = c->mis = c->rr = c->k = 0;
}

/* one pass of core 1's waiting loop: nothing new (a beat now and then), or a new job answered exactly once:
 *   refused  (the number is not the next one, or the check value does not fit after TC_REREAD re-reads): bad_job = N, bad + 1; not run
 *   yld = N  (core 0 asked for it back before core 1 claimed it)
 *   claim = N, the job, done = N, a beat, 16 words of the pattern block checked
 * Calls nothing but TC_JOB_RUN (on the unit everything here is inlined into the RAM loop) */
TC_C1_INLINE void tc_c1_step(tc_c1_t *c)
{
    uint32_t j = TC_LD(&tc_mb.job), arg, chk, r, i;
#if TC_MEAS
    uint32_t ts, te;
#endif
    if (j == c->last) {
        if (!(++c->polls & (TC_IDLE_BEAT - 1u))) {
            c->beat++;
            TC_ST(&tc_mb.beat, c->beat);
        }
        return;
    }
#if TC_MEAS
    ts = TC_TICKS();                                    /* (the moment core 1 saw the new job word) */
#endif
    for (r = 0;; r++) {
        TC_SYNC();                                      /* (the words core 0 wrote before the job word) */
        arg = TC_LD(&tc_mb.arg);
        chk = TC_LD(&tc_mb.chk);
        if (j == c->last + 1u && chk == TC_CHECK(j, arg))
            break;
        if (r >= TC_REREAD) {                           /* refused: never run */
            c->last = j;
            TC_ST(&tc_mb.bad_job, j);
            TC_SYNC();
            c->bad++;
            TC_ST(&tc_mb.bad, c->bad);
            TC_SYNC();
            return;
        }
        j = TC_LD(&tc_mb.job);                          /* (the job word read again too: a number read wrong) */
    }
    if (r) {
        c->rr++;
        TC_ST(&tc_mb.reread, c->rr);
    }
    c->last = j;
    if (TC_LD(&tc_mb.take) == j) {                      /* core 0 wants it back and core 1 has not touched it: it is core 0's */
        TC_ST(&tc_mb.yld, j);
        TC_SYNC();
        return;
    }
    TC_ST(&tc_mb.claim, j);
    TC_SYNC();
    TC_JOB_RUN(arg);
#if TC_MEAS
    te = TC_TICKS();
    TC_ST(&tc_mb.t_seen, ts);
    TC_ST(&tc_mb.t_end, te);
    TC_ST(&tc_mb.mj, j);
#endif
    TC_SYNC();                                          /* (part 2's writes before the done word) */
    TC_ST(&tc_mb.done, j);
    c->beat++;
    TC_ST(&tc_mb.beat, c->beat);
    for (i = 0; i < 16u; i++, c->k++)                   /* the read check: known words, read again every job */
        if (TC_LD(&tc_pat[c->k & (TC_PAT_N - 1u)]) != TC_PAT(c->k & (TC_PAT_N - 1u)))
            c->mis++;
    if (c->mis)
        TC_ST(&tc_mb.misread, c->mis);
    TC_SYNC();
}

/* ---------------------------------------------------------------- core 0's side (src/twocore.c) --- */
enum { TCA_NONE, TCA_CLAIMED, TCA_DONE, TCA_YIELD, TCA_REFUSED, TCA_BAD };   /* core 1's answer to a job (tc_answer) */
typedef struct { uint32_t magic, state, last; } tc_nv_t;   /* the record across a reset (.noinit on the unit) */
#define TCNV_MAGIC 0x54434E56u                             /* "TCNV" */
#define TCNV_ARMED 0xA4ED17C2u                             /* a start is armed for the next power-on (used once: stage 1, as BENCH-B3) */
#define TCNV_RAN 0x2B17E5C2u                               /* core 1 was started and not held cleanly since */
typedef struct {
    uint8_t up;                 /* core 1 answered at this power-on and is not held */
    uint8_t failed;             /* TCR_* that stopped it this session (0: none) */
    volatile uint8_t park;      /* a flash access: the runner posts no job (and plays part 2 itself) until tc_watch's next pass */
    uint8_t strike;             /* the stop of this session counts as a strike (tc_is_strike) */
    uint32_t job;               /* the last job number posted (core 0's own copy of tc_mb.job) */
    uint32_t beat, beat_t;      /* the beat last seen, and when (ms) */
    uint32_t parks;             /* flash parks this session */
    uint32_t supply;            /* VDC14 | SYSVDD << 8 read back after the raise (0: not raised) */
    uint32_t start_us;          /* how long core 1 took to answer at its start */
    uint8_t armed;              /* TCW_*: no start armed / the one-shot record / armed by the build (TC_AUTO_ARM) */
    uint8_t boot_last;          /* TCR_*: the last session's outcome as found at this power-on (TCR_SKIPBOOT when the boot guard skipped the start) */
} tc_t;

/* ---------------------------------------------------------------- core 1's stack mark (after Felucca 1.6.1's; core 0 fills it before the start) --- */
#define TC_STK_FILL 0xC1C1C1C1u
static inline void tc_stack_fill(uint32_t *lo, uint32_t words)
{
    uint32_t i;
    for (i = 0; i < words; i++)
        lo[i] = TC_STK_FILL;
}
/* the deepest use so far in bytes (the stack grows down from lo + words: the untouched words are at the bottom) */
static inline uint32_t tc_stack_peak(const uint32_t *lo, uint32_t words)
{
    uint32_t i = 0;
    while (i < words && lo[i] == TC_STK_FILL)
        i++;
    return (words - i) * 4u;
}
