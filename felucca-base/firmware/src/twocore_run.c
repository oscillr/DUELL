/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL TWO-CORE, stage 1 (DUELL_TWOCORE=1 only; docs/TWOCORE_STAGE1.md "Job 5"): the job core 1 plays, and the runner core 0 calls every block.
 * Included by fx.c after mix_part (it needs the stages, mixpart_t and blockstate_t) and before mix_gather and mix_block (they read tcr.skip2 and call the
 * runner). Stage 1 = core 1 plays part 2 (voices, part effects, part mix into its own pscr / pacc / track); core 0 plays part 1, then waits for part 2 or
 * takes it over, then gathers, sends, master. Bus B (DUEL) stays on core 0.
 *
 * THE JOB (tc_job_run, core 1, called by tc_c1_step after its claim): the stages of mix_part / tests/twocore_sim.c run_part in their order. Its inputs are a
 * static context core 0 fills before tc_post (tc_ctx: the block's snapshot), not a pointer into core 0's stack; n and a 16-bit hash of the snapshot are in
 * arg. A snapshot that does not fit its hash (read wrong) runs nothing and is refused as a wrong check value is: core 1 writes bad_job / bad (its own
 * words), still answers done, and core 0 sees the refusal at the done word (tcr_part2).
 *
 * THE RUNNER (core 0, in mix_block): tcr_begin after the block's snapshot, part 1's stages, tcr_part2, then gather. The sequence of docs/TWOCORE_STAGE1.md
 * section 2.4: post when core 1 is up, not paused, not parked and the last job is closed (else core 0 plays part 2 itself, in the one-core order: the same
 * samples); wait for done until the DEADLINE; not done: take it back, and by core 1's answer
 *   yld      core 0 plays part 2 itself, no click                              "late"       (a run of TCR_LATE_RUN late blocks pauses the jobs)
 *   claim    wait for done until the WAIT LIMIT tcr.lim (BENCHT4; up to BENCHT3 the end of the half): the latest moment at which core 1's done word still leaves core 0
 *            the globals of this block and every later block of the half at the whole one-core cost (= the half's end less those = the deadline's cap + part 2's own cost)
 *     done   fine                                                              "very late"  (BENCHT4: never a pause by itself, however many in a row or in a window; counted as
 *                                                                               "very late without a pause", tcs.vlate_ok. The BENCHT3 run / window rules are off by default and
 *                                                                               can be switched on for a diagnosis: bench vrun N, bench vwin K [MS])
 *     not    part 2 silent this block (core 0 touches nothing of part 2: tcr.skip2, mix_gather skips it); the jobs pause (a fault); part 2 stays silent
 *            until the open job is done, then core 0 plays it (the late job's sums are thrown away). This is also the answer to a job that would make core 0 miss its half
 *   refused  a strike (BADJOB), core 0 plays part 2
 *   nothing  (no answer within TCR_ANSWER_US) part 2 silent, the jobs pause; tc_watch stops core 1 after TC_HANG_MS without a beat
 * A real fault (refused job, a word ahead, a refused snapshot): tc_fail, a strike, core 0 plays part 2 from then on; if core 1 never claimed the open job
 * part 2 continues untouched, else its voices fade out first (tcr.fade2: voice_kill) and it starts cleanly. Lateness is never a strike.
 * Pauses by reason (tcs.pause_*): a real fault (wrong answer, refused, no answer, a silent block, a job not finished by its wait limit), a run of very late blocks and K very
 * late blocks inside a time window (both OFF by default since BENCHT4), a run of late blocks (4 in a row: core 1 does not pick up jobs; never seen on the unit).
 * THE FLOOR (BENCHT3, TC_MEAS builds, switchable): the deadline is not earlier than now + core 1's recent maximum run time (two buckets of 128 blocks) + the slack, and not
 * later than core 0's own latest moment (the table's deadline plus the slack); if now + that run time is already past the latest moment, no job is posted ("nofit": own
 * because of the floor, not a fault, core 0 plays part 2 now, the output is the one-core output).
 * Pausing: core 1 stays alive in its loop (a stopped core 1 cannot be started again before the next power-on); the jobs resume after TCR_CALM_MS.
 * Nothing here runs per sample: a few tests and, at the post, a hash of 40 bytes and the voice count of the two parts. */

/* ---------------------------------------------------------------- the constants --- */
#define TCR_FRAME_TICKS 544u            /* TIMER4 ticks per I2S frame (audio.c DAC_TICKS: 24 MHz / 44,117.6 Hz) */
#define TCR_BLOCK_TICKS (CTL * TCR_FRAME_TICKS)                  /* one block: 17,408 ticks (0.725 ms) */
#define TCR_HALF_TICKS (HALF_FRAMES * TCR_FRAME_TICKS)           /* one half buffer: 69,632 ticks (2.9 ms), the time the ISR has to render it */
#define TCR_BLOCKS (HALF_FRAMES / CTL)                           /* blocks in a half: 4 */
#define TCR_SLACK_TICKS (TCR_HALF_TICKS / 20u)                   /* kept back from the deadline for what the cost table does not hold (events, note starts, the ISR's entry): 5 % of a half, 3,481 ticks */
#define TCR_NEED_MAX 200000u            /* the cost sums (1/100 %) are clamped to this: a tick count of it fits 32 bits */
#define TCR_ANSWER_US 100u              /* after the take, core 1 answers a job within this (an idle core 1 does so in microseconds); no answer: it is stuck */
#define TCR_ANSWER_TICKS (TCR_ANSWER_US * TC_TICKS_PER_US)
#define TCR_LATE_RUN 4u                 /* this many late blocks in a row (one half's worth) pause the jobs */
#define TCR_VLATE_RUN 0u                /* BENCHT4: 0 = OFF. A very late block whose job finished inside its wait limit never pauses, however many in a row (BENCHT3: 4; BENCHT2: 1). `bench vrun N` switches the run rule on for a diagnosis */
#define TCR_VWIN_K 0u                   /* BENCHT4: 0 = OFF. The window rule: this many very late blocks (not in a row) within TCR_VWIN_MS pause the jobs (BENCHT3: 8). `bench vwin K [MS]` switches it on */
#define TCR_VWIN_MS 2000u
#define TCR_VWIN_TICKS (TCR_VWIN_MS * TC_TICKS_PER_US * 1000u)
#define TCR_VWIN_MAX 16u                /* the ring of times for the window: K can be set up to this */
#define TCR_FLOOR_BLOCKS 128u           /* the deadline's floor: the run times of core 1 are kept in two buckets of this many blocks, so the maximum covers the last 128 to 256 blocks */
#define TCR_CALM_MS 10000u              /* a pause lasts this long, then the jobs resume */
#define TCR_CALM_TICKS (TCR_CALM_MS * 1000u * TC_TICKS_PER_US)   /* 240 million ticks: fits 32 bits (the timer wraps every 179 s) */

/* BENCHT2 (TC_MEAS: developer bench builds only): the margin settings can be changed at run time by the bench console (bench slack / calm / pause / lrun /
 * vrun / vwin / floor, bench.c), so one build compares settings. Without TC_MEAS they are the constants above and the compiler sees no variable.
 * BENCHT3: vwin = K very late blocks within vwin_ticks pause the jobs (K 0: off); floor 1 = the deadline has the floor of core 1's measured run time (needs the
 * measuring, so a build without TC_MEAS has no floor).
 * BENCHT4: the defaults are vrun 0 (off), vwin 0 (off), floor 0 (off): only a real fault, a job not finished inside its wait limit (tcr.lim) or a run of late blocks pauses. */
#if TC_MEAS
typedef struct { uint32_t slack, calm_ticks, lrun, vrun, vwin_k, vwin_ticks, floor; } tcm_cfg_t;
static tcm_cfg_t tcm_cfg = { TCR_SLACK_TICKS, TCR_CALM_TICKS, TCR_LATE_RUN, TCR_VLATE_RUN, TCR_VWIN_K, TCR_VWIN_TICKS, 0u };
#define TCR_SLACK_NOW tcm_cfg.slack
#define TCR_CALM_NOW tcm_cfg.calm_ticks
#define TCR_LATE_NOW tcm_cfg.lrun
#define TCR_VLATE_NOW tcm_cfg.vrun
#define TCR_VWINK_NOW tcm_cfg.vwin_k
#define TCR_VWINT_NOW tcm_cfg.vwin_ticks
#define TCR_FLOOR_NOW tcm_cfg.floor
#else
#define TCR_SLACK_NOW TCR_SLACK_TICKS
#define TCR_CALM_NOW TCR_CALM_TICKS
#define TCR_LATE_NOW TCR_LATE_RUN
#define TCR_VLATE_NOW TCR_VLATE_RUN
#define TCR_VWINK_NOW TCR_VWIN_K
#define TCR_VWINT_NOW TCR_VWIN_TICKS
#define TCR_FLOOR_NOW 0u
#endif
_Static_assert(TCR_VWIN_K <= TCR_VWIN_MAX && (TCR_VWIN_MAX & (TCR_VWIN_MAX - 1u)) == 0, "twocore_run.c: the window's ring");

_Static_assert(HALF_FRAMES % CTL == 0 && TCR_BLOCKS >= 1, "twocore_run.c: a half is whole blocks");
_Static_assert(NLIVE == 2, "twocore_run.c: stage 1 plays part 1 on core 0 and part 2 on core 1");
_Static_assert(sizeof(blockstate_t) % 4u == 0, "twocore_run.c: the snapshot's hash reads whole words");

/* the budget's cost table (budget.c, included after fx.c; budget.h declares the functions): the deadline reads part 2's cost at its current settings */
#include "budget.h"

/* ---------------------------------------------------------------- the counters (the developer page shows these) --- */
typedef struct {
    uint32_t blocks;            /* blocks through the runner */
    uint32_t in_time;           /* core 1 was done by the deadline */
    uint32_t late;              /* core 1 yielded (had not started): core 0 played part 2, no click */
    uint32_t vlate;             /* core 1 had claimed and was not done by the deadline (done within the half, or not: silent) */
    uint32_t silent;            /* blocks whose part 2 was silent: the very late ones that did not finish, no answer, and the blocks waiting for an open job */
    uint32_t wait;              /* of those: no new job, core 1 still held the old one */
    uint32_t own;               /* core 0 played part 2 with no job (core 1 down, paused, parked, the half too far gone) */
    uint32_t refused;           /* a refused job or snapshot, or a word ahead: a strike, core 0 played part 2 */
    uint32_t noanswer;          /* no answer to the take within TCR_ANSWER_US */
    uint32_t pauses, resumes;   /* the jobs paused (any reason below), and resumed after the calm period */
    uint32_t trims;             /* notes faded at once because the jobs paused or core 1 stopped (voice_trim_now) */
    uint32_t vlate_ok;          /* BENCHT3: very late blocks whose job finished inside its half and that did NOT cause a pause (they are counted in vlate too) */
    uint32_t pause_fault, pause_run, pause_win, pause_late;   /* BENCHT3: pauses by reason: a real fault (wrong answer, refused, no answer, silent block, a job not finished in its half) /
                                 * a run of very late blocks / K very late blocks within the window / a run of late blocks; they add up to pauses */
} tc_stat_t;                    /* (the flash parks are tc.parks) */
static tc_stat_t tcs TC_BSS;

typedef struct {
    uint32_t t0;                /* the half's start (audio.c: the tick at the ISR's entry) */
    uint32_t dl;                /* this block's deadline */
    uint32_t lim;               /* BENCHT4: the wait limit of this block's job: the latest moment core 1's done word still leaves core 0 its own half (see tcr_deadline); core 0 never waits past it */
    uint32_t job;               /* the job posted this block (0: none) */
    uint32_t open;              /* the last job posted that is not known to be closed (core 1 may still write part 2) */
    uint32_t pause_t;           /* when the jobs paused */
    uint32_t c1f, c1v, c2f, c2v, g;   /* the cost table's items (1/100 %): fixed and a voice, part 1 and part 2, the globals; read at a half's first block */
    uint8_t blk;                /* the block in the half */
    uint8_t t0_ok, cost_ok;
    uint8_t skip2;              /* part 2 is silent this block and core 1 may still write it: mix_gather must not read it */
    uint8_t paused;
    uint16_t late_run, vlate_run;   /* late / very late blocks in a row (the runtime settings go up to 1000) */
    uint8_t vw_i, vw_n;         /* BENCHT3: the ring of the times of the last very late blocks (the window rule): the next slot, and how many are valid */
    uint32_t vw_t[TCR_VWIN_MAX];
    uint8_t fade2;              /* part 2 may be half written (core 1 stopped in a claimed job): fade its voices out before core 0 plays it */
    uint8_t trim;               /* bit per part: notes above the part's number to fade at once (tc_trim_poll asks, the next block does it) */
    uint8_t trim_on;            /* the poll saw the condition (so it asks once) */
#if TC_MEAS
    uint8_t own_why;            /* why this block has no job: TCM_OWN_* (the own counter's split) */
    uint16_t fl_n;              /* BENCHT3, the deadline's floor: blocks in the current bucket */
    uint32_t cap;               /* the latest moment core 0 can still play part 2 itself (this block; the floor's ceiling) */
    uint32_t fl_cur, fl_prev;   /* the largest run time (ticks) core 1 showed in the current and in the last bucket of TCR_FLOOR_BLOCKS blocks */
#endif
} tcr_t;
static tcr_t tcr TC_BSS;

#if TC_MEAS
/* ---------------------------------------------------------------- BENCHT2: what the jobs measure (core 0 collects; developer bench builds only) ---
 * Per job core 1 ran: START DELAY = core 1's clock when it saw the job word less core 0's clock just before it posted; RUN TIME = core 1's clock at the return of the
 * job less its clock at the sight (the claim and the job; the done word is written after); FINISH MARGIN = the block's deadline less the return (negative: after it).
 * All in TIMER4 ticks (24 a us), shown in us. Collected when the job is seen done (in time, or very late but finished) or, for a very late one that finished after
 * its half, when the runner settles it at a later block. A job core 0 took back (late) or that was refused has no figures.
 * Eight bins each, edges in us:  delay <2 <5 <10 <25 <50 <100 <250 >=250;  run <100 <150 <200 <300 <400 <600 <900 >=900;  margin <-500 <-100 <0 <100 <250 <500 <1000 >=1000.
 * Cleared by the audio interrupt itself at the next block after the main loop sets tcm.clr (bench run does), so the figures are those of one test. */
enum { TCM_OWN_DOWN, TCM_OWN_PARK, TCM_OWN_PAUSE, TCM_OWN_DL, TCM_OWN_FL, TCM_OWN_N };   /* no job because core 1 is down / a flash park / paused / the deadline was already past / (BENCHT3) the floor said core 1's measured run time cannot fit before the latest deadline */
#define TCM_NB 8u
#define TCM_NEV 16u
typedef struct { uint32_t blk, at, a1, a2; int32_t sd, rt, mg; } tcm_ev_t;     /* a very late job: the block in the half, the block count since the test's start, voices sounding (part 1, part 2), ticks */
typedef struct {
    uint32_t clr;                       /* the main loop asks for a clear (written there, taken here) */
    uint32_t b0;                        /* tcs.blocks at the clear */
    uint32_t posted, own[TCM_OWN_N];
    uint32_t n, badclk;                 /* jobs measured; jobs whose clock figures were impossible (not counted in the bins) */
    uint32_t sd_max, rt_max;
    int32_t mg_min;
    uint64_t sd_sum, rt_sum;
    int64_t mg_sum;
    uint32_t hsd[TCM_NB], hrt[TCM_NB], hmg[TCM_NB];
    uint32_t nv, vsd_max, vrt_max, vblk[TCR_BLOCKS];   /* the very late jobs that were measured: their delay, run and the block they were in */
    uint64_t vrt_sum;
    uint64_t st_sum[TCR_BLOCKS], dl_sum[TCR_BLOCKS];   /* per block of the half: where core 1 started the job and where its deadline was, ticks after the half's start, summed */
    uint32_t nb[TCR_BLOCKS], st_max;    /* jobs measured per block of the half; the latest start after the half's start */
    uint32_t hacc, hpk;                 /* core 1's run ticks in this half, and the largest half so far */
    uint32_t ev_n;                      /* very late jobs seen (the ring holds the newest TCM_NEV) */
    tcm_ev_t ev[TCM_NEV];
} tcm_t;
static tcm_t tcm TC_BSS;
static struct { uint32_t t_post, t0, blk, at, a1, a2; uint8_t vl; } tcj TC_BSS;   /* the job in flight (not cleared with the figures) */
static const int32_t TCM_E_SD[TCM_NB - 1u] = { 2 * 24, 5 * 24, 10 * 24, 25 * 24, 50 * 24, 100 * 24, 250 * 24 };
static const int32_t TCM_E_RT[TCM_NB - 1u] = { 100 * 24, 150 * 24, 200 * 24, 300 * 24, 400 * 24, 600 * 24, 900 * 24 };
static const int32_t TCM_E_MG[TCM_NB - 1u] = { -500 * 24, -100 * 24, 0, 100 * 24, 250 * 24, 500 * 24, 1000 * 24 };
static uint32_t tcm_bin(int32_t v, const int32_t *e)
{
    uint32_t i = 0;
    while (i < TCM_NB - 1u && v >= e[i])
        i++;
    return i;
}
static void tcm_half(void)              /* a half begins: its run ticks closed */
{
    if (tcm.hacc > tcm.hpk)
        tcm.hpk = tcm.hacc;
    tcm.hacc = 0;
}
/* job j is closed and core 1 ran it: take its figures (the words core 1 wrote before done) */
static void tcm_collect(uint32_t j)
{
    int32_t sd, rt, mg;
    uint32_t ts, te;
    if (TC_LD(&tc_mb.mj) != j || TC_LD(&tc_mb.bad_job) == j)
        return;
    ts = TC_LD(&tc_mb.t_seen);
    te = TC_LD(&tc_mb.t_end);
    sd = (int32_t)(ts - tcj.t_post);
    rt = (int32_t)(te - ts);
    mg = (int32_t)(tcr.dl - te);
    if (sd < 0 || rt < 0 || sd > (int32_t)(4u * TCR_HALF_TICKS) || rt > (int32_t)(4u * TCR_HALF_TICKS)) {   /* (a clock that does not fit: counted, not binned) */
        tcm.badclk++;
        return;
    }
    tcm.n++;
    {   /* where in the half: the start of this job (core 1's clock) and its deadline, after the half's start */
        uint32_t b = tcj.blk % TCR_BLOCKS, st = ts - tcj.t0;
        tcm.nb[b]++;
        tcm.st_sum[b] += st;
        tcm.dl_sum[b] += tcr.dl - tcj.t0;
        if (st > tcm.st_max && st < 4u * TCR_HALF_TICKS)
            tcm.st_max = st;
    }
    tcm.hsd[tcm_bin(sd, TCM_E_SD)]++;
    tcm.hrt[tcm_bin(rt, TCM_E_RT)]++;
    tcm.hmg[tcm_bin(mg, TCM_E_MG)]++;
    if ((uint32_t)sd > tcm.sd_max)
        tcm.sd_max = (uint32_t)sd;
    if ((uint32_t)rt > tcm.rt_max)
        tcm.rt_max = (uint32_t)rt;
    if (tcm.n == 1u || mg < tcm.mg_min)
        tcm.mg_min = mg;
    tcm.sd_sum += (uint32_t)sd;
    tcm.rt_sum += (uint32_t)rt;
    tcm.mg_sum += mg;
    tcm.hacc += (uint32_t)rt;
    if ((uint32_t)rt > tcr.fl_cur)
        tcr.fl_cur = (uint32_t)rt;                      /* (the deadline's floor: the recent maximum) */
    if (tcj.vl) {
        tcm_ev_t *e = &tcm.ev[tcm.ev_n++ % TCM_NEV];
        tcm.nv++;
        tcm.vblk[tcj.blk % TCR_BLOCKS]++;
        tcm.vrt_sum += (uint32_t)rt;
        if ((uint32_t)rt > tcm.vrt_max)
            tcm.vrt_max = (uint32_t)rt;
        if ((uint32_t)sd > tcm.vsd_max)
            tcm.vsd_max = (uint32_t)sd;
        e->blk = tcj.blk, e->at = tcj.at - tcm.b0, e->a1 = tcj.a1, e->a2 = tcj.a2, e->sd = sd, e->rt = rt, e->mg = mg;
    }
}
static void tcm_clear(void)
{
    uint32_t b0 = tcs.blocks - 1u;                      /* (this block is counted already and is the test's first) */
    memset(&tcm, 0, sizeof tcm);
    tcm.b0 = b0;
    tcr.fl_n = 0, tcr.fl_cur = tcr.fl_prev = 0;        /* (the floor starts again with the test) */
}
#endif

/* ---------------------------------------------------------------- the job (core 1) --- */
#ifndef TC_CTX_SECT
#define TC_CTX_SECT                     /* (the unit: the pool's tail beside tc_mb, hal/fm1_core1.h) */
#endif
typedef struct { blockstate_t bs; } __attribute__((aligned(64))) tc_ctx_t;   /* (rounded up to whole 64-byte lines: nothing else shares them) */
static tc_ctx_t tc_ctx TC_CTX_SECT;     /* the block's snapshot: core 0 writes it before tc_post, core 1 reads it after its claim, nobody in between */

/* 16 bits of the snapshot: a misread byte changes it (FNV over the words) */
static uint32_t tc_ctx_hash(const blockstate_t *bs)
{
    const uint32_t *w = (const uint32_t *)bs;
    uint32_t i, h = 0x811C9DC5u;
    for (i = 0; i < sizeof *bs / 4u; i++)
        h = (h ^ w[i]) * 0x01000193u;
    return (h ^ h >> 16) & 0xFFFFu;
}

/* refused like a wrong check value: core 1's own words (bad_job = this job, bad + 1), nothing run. tc_c1_step still answers done; core 0 reads bad */
static void tc_job_refuse(void)
{
    TC_ST(&tc_mb.bad_job, TC_LD(&tc_mb.job));
    TC_SYNC();
    TC_ST(&tc_mb.bad, TC_LD(&tc_mb.bad) + 1u);
}

/* core 1: part 2 for one block, the stages of mix_part with the morph window around them (the order of tests/twocore_sim.c run_part). arg: n | hash << 16 */
static void tc_job_run(uint32_t arg)
{
    track_t *t = &trk[1];
    uint32_t n = arg & 0xFFFFu;
    mixpart_t m;
    int mo;
    if (tc_ctx_hash(&tc_ctx.bs) != arg >> 16 || n != CTL) {   /* (a block is always CTL frames; the stages get the constant CTL, not n: with a variable n the compiler stops folding the block length in the hot filter code (uf_run, uf_voice) and the hot block grows by 68 B) */
        tc_job_refuse();
        return;
    }
    mo = morph_begin(t, 1);
    if (part_voice(&m, t, CTL, &tc_ctx.bs)) {
        part_fx(&m);
        part_mix(&m);
        part_end(&m);
    } else {
        part_rest(&m);
    }
    if (mo)
        morph_end(t);
    /* STAGE 2: bus B */
}

/* ---------------------------------------------------------------- the deadline (core 0) --- */
static uint32_t tcr_active(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active != 0;
    return n;
}
/* The latest tick at which core 0 can still play part 2 itself and finish the half in time: the half's end, less
 *   part 2's cost at its current settings (its fixed stages and its sounding voices),
 *   the rest of this block (gather, sends, master: the globals),
 *   the blocks still to come, each at the whole one-core cost (both parts and the globals: the worst case, core 1 late again),
 *   and TCR_SLACK_TICKS.
 * The costs are the NOTES table of budget.c (1/100 % of a half's time for a block's worth), BD_U's 15 % margin in them. The table's items are read at a
 * half's first block; the sounding voices at every block (a count over 8 voices each). Core 1 is idle (the last job is closed) when this runs, so
 * reading part 2's settings is safe */
static uint32_t tcr_deadline(uint32_t now, int *nofit)
{
    uint32_t k = tcr.blk, c1, c2, need, dl, cap;
    if (k == 0 || !tcr.cost_ok) {
        tcr.c1f = part_fixed(&trk[0]);
        tcr.c1v = part_cost(&trk[0]);
        tcr.c2f = part_fixed(&trk[1]);
        tcr.c2v = part_cost(&trk[1]);
        tcr.g = global_cost();
        tcr.cost_ok = 1;
    }
#if TC_MEAS
    tcj.a1 = tcr_active(&trk[0]);
    tcj.a2 = tcr_active(&trk[1]);
    c1 = tcr.c1f + tcj.a1 * tcr.c1v;
    c2 = tcr.c2f + tcj.a2 * tcr.c2v;
#else
    c1 = tcr.c1f + tcr_active(&trk[0]) * tcr.c1v;
    c2 = tcr.c2f + tcr_active(&trk[1]) * tcr.c2v;
#endif
    need = c2 + tcr.g + (TCR_BLOCKS - 1u - k) * (c1 + c2 + tcr.g);
    if (need > TCR_NEED_MAX)
        need = TCR_NEED_MAX;
    cap = tcr.t0 + TCR_HALF_TICKS - need * TCR_BLOCK_TICKS / 10000u;   /* the latest moment core 0 can still play part 2 itself and finish the half (no slack) */
    dl = cap - TCR_SLACK_NOW;                           /* the table's deadline */
    {   /* the wait limit: with part 2 done by core 1 core 0 still has to do the globals of this block and every later block at the whole one-core cost, so core 1's done word
         * is useful until the half's end less exactly that (= cap + part 2's own cost). Never later than the half's end (the globals are >= 0) */
        uint32_t rest = need > c2 ? need - c2 : 0u;
        tcr.lim = tcr.t0 + TCR_HALF_TICKS - rest * TCR_BLOCK_TICKS / 10000u;
    }
    *nofit = 0;
#if TC_MEAS
    tcr.cap = cap;
    /* BENCHT3, the floor: the deadline is not earlier than core 1's own measurement says it needs (now + its recent maximum run time + the slack), but never later than
     * cap, so core 0 can still play part 2 itself without a miss. If now + the recent maximum is already past cap, the job would be very late whatever the deadline:
     * no job (nofit), core 0 plays part 2 now; only when the table's deadline itself was still ahead (else it is the plain "deadline past") */
    if (TCR_FLOOR_NOW) {
        uint32_t rm = tcr.fl_cur > tcr.fl_prev ? tcr.fl_cur : tcr.fl_prev;
        if (rm && (int32_t)(dl - now) > 0) {
            uint32_t tgt = now + rm + TCR_SLACK_NOW;
            if ((int32_t)(now + rm - cap) > 0)
                *nofit = 1;
            else if ((int32_t)(tgt - dl) > 0)
                dl = (int32_t)(tgt - cap) > 0 ? cap : tgt;
        }
    }
#else
    (void)now;
#endif
    return dl;
}

/* ---------------------------------------------------------------- the runner (core 0) --- */
/* the start of a half (audio.c, before its first block): the clock the deadlines are measured from */
static void tc_half_begin(uint32_t t0)
{
    tcr.t0 = t0;
    tcr.t0_ok = 1;
    tcr.blk = 0;
#if TC_MEAS
    tcm_half();
#endif
}

/* main loop (main.c, beside tc_watch): the jobs paused or core 1 stopped: notes above a part's number fade out at once (the next block does it, in the
 * audio interrupt: the voices are not touched from here). Asked once for each pause or stop */
static void tc_trim_poll(void)
{
    uint8_t on = tcr.paused || tc.failed;
    if (on && !tcr.trim_on)
        tcr.trim = 3u;
    tcr.trim_on = on;
}

enum { TCR_PW_FAULT, TCR_PW_RUN, TCR_PW_WIN, TCR_PW_LATE };   /* why the jobs pause (BENCHT3): a real fault / a run of very late blocks / K very late blocks in the window / a run of late blocks */
static void tcr_pause(uint32_t now, uint32_t why)
{
    if (!tcr.paused) {
        tcr.paused = 1;
        tcr.pause_t = now;
        tcs.pauses++;
        if (why == TCR_PW_FAULT)
            tcs.pause_fault++;
        else if (why == TCR_PW_RUN)
            tcs.pause_run++;
        else if (why == TCR_PW_WIN)
            tcs.pause_win++;
        else
            tcs.pause_late++;
    }
}

/* a very late block whose job finished inside its half (nothing silent, refused or wrong): it pauses the jobs only when it is the vrun-th in a row (TCR_VLATE_NOW), or
 * the vwin_k-th within the window (the ring of the last times; a gap longer than the window starts the chain again, so a stale slot never counts). Else it is counted
 * (vlate_ok) and core 1 goes on */
static void tcr_vlate_done(uint32_t now)
{
    uint32_t k = TCR_VWINK_NOW, win = 0;
    if (k > TCR_VWIN_MAX)
        k = TCR_VWIN_MAX;
    if (k) {
        if (tcr.vw_n && (uint32_t)(now - tcr.vw_t[(tcr.vw_i - 1u) % TCR_VWIN_MAX]) > TCR_VWINT_NOW)
            tcr.vw_n = 0;
        tcr.vw_t[tcr.vw_i % TCR_VWIN_MAX] = now;
        tcr.vw_i = (uint8_t)((tcr.vw_i + 1u) % TCR_VWIN_MAX);
        if (tcr.vw_n < TCR_VWIN_MAX)
            tcr.vw_n++;
        win = tcr.vw_n >= k && (uint32_t)(now - tcr.vw_t[(tcr.vw_i - k) % TCR_VWIN_MAX]) <= TCR_VWINT_NOW;
    }
    if (TCR_VLATE_NOW && ++tcr.vlate_run >= TCR_VLATE_NOW)   /* (BENCHT4: the run rule is off at 0; then the run is not even counted) */
        tcr_pause(now, TCR_PW_RUN);
    else if (win)
        tcr_pause(now, TCR_PW_WIN);
    else
        tcs.vlate_ok++;
}

/* an old job: core 1 stopped (its words are final: a claimed job may have left part 2 half written, else part 2 is untouched), or it is closed now */
static void tcr_settle(void)
{
    if (!tcr.open)
        return;
    if (!tc.up) {
        uint32_t a = tc_answer(tcr.open);
        if (a == TCA_CLAIMED || a == TCA_BAD)
            tcr.fade2 = 1;
        tcr.open = 0;
    } else if (tc_closed()) {
#if TC_MEAS
        tcm_collect(tcr.open);                          /* (a very late job that finished after its half) */
#endif
        tcr.open = 0;
    }
}

/* core 0 plays part 2 itself: the one-core order. After a core 1 stopped in the middle of it, its voices fade out first (voice_kill, the one-block fade) */
static void tcr_play2(const blockstate_t *bs, uint32_t n)
{
    track_t *t = &trk[1];
    int mo;
    if (tcr.fade2) {
        uint32_t i;
        tcr.fade2 = 0;
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].stage != 4u)
                voice_kill(&t->v[i]);
    }
    if (tcr.trim & 2u) {
        tcr.trim &= (uint8_t)~2u;
        tcs.trims += voice_trim_now(t);
    }
    mo = morph_begin(t, 1);
    mix_part(t, n, bs);
    if (mo)
        morph_end(t);
}

/* after the block's snapshot, before part 1: settle the old job, resume after the calm period, and post this block's job if the hand-over is on */
static void tcr_begin(const blockstate_t *bs, uint32_t n)
{
    uint32_t now = TC_TICKS(), dl;
    int nofit;
    tcs.blocks++;
    tcr.job = 0;
    tcr.skip2 = 0;
#if TC_MEAS
    if (tcm.clr)
        tcm_clear();
#endif
    if (!tcr.t0_ok) {
        tcr.t0 = now;
        tcr.t0_ok = 1;
    }
#if TC_MEAS
    if (++tcr.fl_n >= TCR_FLOOR_BLOCKS)                 /* (the floor's buckets turn over by blocks, not by jobs, so a maximum that stopped the jobs fades too) */
        tcr.fl_n = 0, tcr.fl_prev = tcr.fl_cur, tcr.fl_cur = 0;
#endif
    tcr_settle();
    if (tcr.trim & 1u) {
        tcr.trim &= (uint8_t)~1u;
        tcs.trims += voice_trim_now(&trk[0]);
    }
    if (tcr.paused && (int32_t)(now - tcr.pause_t - TCR_CALM_NOW) >= 0) {
        tcr.paused = 0;
        tcr.late_run = tcr.vlate_run = 0;
        tcr.vw_n = 0;
        tcs.resumes++;
    }
    if (!tc.up || tc.park || tcr.paused || tcr.open) {
#if TC_MEAS
        tcr.own_why = !tc.up ? TCM_OWN_DOWN : tc.park ? TCM_OWN_PARK : TCM_OWN_PAUSE;
#endif
        return;                                         /* core 0 plays part 2 itself, or waits for the open job (tcr_part2) */
    }
    dl = tcr_deadline(now, &nofit);
    if ((int32_t)(dl - now) <= 0 || nofit) {
#if TC_MEAS
        tcr.own_why = TCM_OWN_DL;
        if (nofit)
            tcr.own_why = TCM_OWN_FL;
#endif
        return;                                         /* the half is too far gone to wait for core 1 (or core 1's measured run time does not fit before the latest deadline: not a fault) */
    }
    tc_ctx.bs = *bs;
    tcr.dl = dl;
#if TC_MEAS
    tcj.t_post = TC_TICKS();                            /* (before the post, so core 1 cannot see the job before this tick) */
    tcj.blk = tcr.blk;
    tcj.t0 = tcr.t0;
    tcj.at = tcs.blocks;
    tcj.vl = 0;
    tcm.posted++;
#endif
    tcr.job = tc_post(n | (uint32_t)tc_ctx_hash(&tc_ctx.bs) << 16);
    tcr.open = tcr.job;
}

/* a fault of core 1 found by the runner: stopped for the session, a strike, core 0 plays part 2 (faded out first if core 1 had claimed the job) */
static void tcr_fault(uint32_t why, const blockstate_t *bs, uint32_t n)
{
    tc_fail(why);
    tcs.refused++;
    tcr_settle();
    tcr_play2(bs, n);
}

/* after part 1: part 2 of this block is on core 1 (wait for it, the late path), or core 0 plays it, or it stays silent */
static void tcr_part2(const blockstate_t *bs, uint32_t n)
{
    uint32_t j = tcr.job, a, lim = tcr.lim, t1;
    int vl = 0;                                         /* very late: counted already */
    int noans = 0;                                      /* no answer to the take: a fault even if core 1 finishes the job later */
    if (tcr.blk < TCR_BLOCKS - 1u)
        tcr.blk++;
    if (!j) {
        if (tcr.open) {                                 /* core 1 still holds an older job: part 2 is its until it is done */
            tcs.wait++;
            tcs.silent++;
            tcr.skip2 = 1;
        } else {
            tcs.own++;
#if TC_MEAS
            tcm.own[tcr.own_why]++;
#endif
            tcr_play2(bs, n);
        }
        return;
    }
    while ((a = tc_answer(j)) == TCA_NONE || a == TCA_CLAIMED)
        if ((int32_t)(TC_TICKS() - tcr.dl) >= 0)
            break;
    if (a == TCA_NONE || a == TCA_CLAIMED) {            /* not done by the deadline: take it back and see what core 1 says */
        tc_take(j);
        t1 = TC_TICKS() + TCR_ANSWER_TICKS;
        if ((int32_t)(t1 - lim) > 0)
            t1 = lim;                                   /* (the wait for the answer is inside the limit too) */
        while ((a = tc_answer(j)) == TCA_NONE && (int32_t)(TC_TICKS() - t1) < 0)
            ;
        if (a == TCA_YIELD) {                           /* it had not started: core 0 plays part 2, no click */
            tcs.late++;
            tcr.open = 0;
            tcr.vlate_run = 0;
            if (++tcr.late_run >= TCR_LATE_NOW)
                tcr_pause(TC_TICKS(), TCR_PW_LATE);
            tcr_play2(bs, n);
            return;
        }
        if (a == TCA_CLAIMED || a == TCA_NONE) {        /* it is on it (or stuck): wait for done, but not past the wait limit (BENCHT4; up to BENCHT3: the end of the half) */
            if (a == TCA_NONE)
                tcs.noanswer++, noans = 1;
            while ((a = tc_answer(j)) == TCA_CLAIMED || a == TCA_NONE)
                if ((int32_t)(TC_TICKS() - lim) >= 0)
                    break;
            vl = 1;
            tcs.vlate++;
#if TC_MEAS
            tcj.vl = 1;
#endif
            tcr.late_run = 0;
            if (a == TCA_YIELD) {                       /* (no answer in time, then yld: it never touched part 2) */
                tcr_pause(TC_TICKS(), TCR_PW_FAULT); /* [no answer] */
                tcr.open = 0;
                tcr_play2(bs, n);
                return;
            }
            if (a == TCA_CLAIMED || a == TCA_NONE) {    /* not done by the wait limit: part 2 silent for this block, core 0 touches nothing of it */
                tcr_pause(TC_TICKS(), TCR_PW_FAULT); /* [silent] */
                tcs.silent++;
                tcr.skip2 = 1;
                return;
            }
            /* done inside the half: a real fault only if there was no answer to the take or the answer is wrong (below); else a very late block that did no harm */
            if (noans || a != TCA_DONE || TC_LD(&tc_mb.bad))
                tcr_pause(TC_TICKS(), TCR_PW_FAULT);
            else
                tcr_vlate_done(TC_TICKS());
        }
    }
    if (a == TCA_DONE && !TC_LD(&tc_mb.bad)) {
        if (!vl) {                                      /* (done by the deadline, or between it and the take) */
            tcs.in_time++;
            tcr.late_run = tcr.vlate_run = 0;
        }
#if TC_MEAS
        tcm_collect(j);
#endif
        tcr.open = 0;                                   /* (a very late one that is done: its sums are good) */
        return;
    }
    if (vl)
        tcs.vlate--;                                    /* (counted as refused, once) */
    tcr_fault(a == TCA_BAD ? TCR_BADWORD : TCR_BADJOB, bs, n);   /* refused (a check value, a snapshot), or a word ahead */
}
