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
typedef struct { uint32_t slack, calm_ticks, lrun, vrun, vwin_k, vwin_ticks, floor, nocore1; } tcm_cfg_t;
static tcm_cfg_t tcm_cfg = { TCR_SLACK_TICKS, TCR_CALM_TICKS, TCR_LATE_RUN, TCR_VLATE_RUN, TCR_VWIN_K, TCR_VWIN_TICKS, 0u, 0u };
#define TCR_SLACK_NOW tcm_cfg.slack
#define TCR_CALM_NOW tcm_cfg.calm_ticks
#define TCR_LATE_NOW tcm_cfg.lrun
#define TCR_VLATE_NOW tcm_cfg.vrun
#define TCR_VWINK_NOW tcm_cfg.vwin_k
#define TCR_VWINT_NOW tcm_cfg.vwin_ticks
#define TCR_FLOOR_NOW tcm_cfg.floor
#define TCR_NOCORE1_NOW tcm_cfg.nocore1                 /* HANDOFF_V2 item 112 (bench word `core1 off`): 1 = core 1 is treated as down, with no power-cycle: core 0 plays part 2, the budget has the one-core numbers */
#define TCR_OR_NOCORE1 || tcm_cfg.nocore1               /* (the extra term of the conditions, written as tokens so that outside the bench the source text is exactly the old one) */
#define TCR_AND_NOT_NOCORE1 && !tcm_cfg.nocore1
#else
#define TCR_SLACK_NOW TCR_SLACK_TICKS
#define TCR_CALM_NOW TCR_CALM_TICKS
#define TCR_LATE_NOW TCR_LATE_RUN
#define TCR_VLATE_NOW TCR_VLATE_RUN
#define TCR_VWINK_NOW TCR_VWIN_K
#define TCR_VWINT_NOW TCR_VWIN_TICKS
#define TCR_FLOOR_NOW 0u
#define TCR_OR_NOCORE1                                  /* (no such switch outside the developer bench: these expand to nothing, the normal build is byte for byte the old one) */
#define TCR_AND_NOT_NOCORE1
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
    uint32_t sh_posts;          /* VOICE SHARE B2: share jobs posted */
    uint32_t sh_done;           /* .. of those done by core 1 and merged (in time or very late) */
    uint32_t sh_late;           /* .. core 1 had not claimed: core 0 took the job back and rendered the set itself (bit-identical sound) */
    uint32_t sh_freeze;         /* .. core 1 claimed and was not done by the wait limit: the set made no sound this block (frozen until the job closes; a click) */
    uint32_t sh_drop;           /* .. not claimed, but core 0 had no time to render the set (nott): the set made no sound this block */
    uint32_t sh_skip;           /* blocks that could share but were not posted (the wait limit was past): core 0 rendered every voice */
    uint32_t sh_none;           /* blocks prepared for a share with no post (fewer than two voices): core 0 played part 2 */
    uint32_t sh_fault;          /* a refused / wrong share job (a strike) */
    uint32_t sh_hold;           /* share failures that started a hold-off (TCR_SH_HOLD blocks without a new share, no pause) */
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
    uint32_t t1f, t1v, t2f, t2v, tg;  /* DUELL 1.1 item 142: the same items at their true cost (ceil(x * 100 / BUDGET_DL_MARGIN), as tcr_deadline_true), for the wait limit */
    uint32_t f0, f1, f0t, f1t;        /* THE DEAL's running numbers of this block (tcr_deal; 1/100 %): core 0's load (the sends and the master, its voices) and core 1's (the parts' own stages, its
                                       * voices), from the 1.15 table and at true cost: what tcr_deadline counts with */
    uint8_t blk;                /* the block in the half */
    uint8_t t0_ok, cost_ok;
    uint8_t skip;               /* the parts are silent this block and core 1 may still write them (its job is open, or it stopped inside one): mix_gather must not read them */
    uint8_t paused;
    uint16_t late_run, vlate_run;   /* late / very late blocks in a row (the runtime settings go up to 1000) */
    uint8_t vw_i, vw_n;         /* BENCHT3: the ring of the times of the last very late blocks (the window rule): the next slot, and how many are valid */
    uint32_t vw_t[TCR_VWIN_MAX];
    uint8_t fade;               /* the parts may be half written (core 1 stopped in a claimed job): their voices fade out before the next block plays */
    uint8_t trim;               /* bit per part: notes above the part's number to fade at once (tc_trim_poll asks, the next block does it) */
    uint8_t trim_on;            /* the poll saw the condition (so it asks once) */
    uint8_t two;                /* DUELL 1.1 item 90: the two-core NOTES rule is in force (read at a half's first block with the costs): the later blocks count core 0's own share only */
    uint8_t nott;               /* this block's job may not be taken back (the take-over no longer fits the half): core 1 finishes it, or the jobs pause */
    uint8_t c0v;                /* the parts whose own set core 0 has rendered this block (bit per part: the word core 1 waits for, tc_mb.c0v) */
    uint8_t ph;                 /* tcr_parts_a ran this block's control and sets: tcr_parts_b has a block to finish (0: a block waiting for an open job) */
    uint32_t pj;                /* .. and the job it posted (0: none, core 0 plays everything) */
    int32_t *out;               /* THIS block's output, PERFORM's flag and the master level, set in every block (tcr_post, before anything can skip): what core 0 writes or copies the globals into */
    int32_t mg;
    int perf;
    uint32_t t0r;               /* core 0's clock when its own sets were in (tcr_parts_a; the balance, 2026-11-09) */
    int32_t bias;               /* THE BALANCE (2026-11-07; STRESS-LADDER: with everything on core 0 waited 70 to 140 us of each 725 us block for core 1 while core 1 stood idle at its start: the table's
                                 * fixed costs do not say which core is really the fuller one). The deal splits the voices by their costs alone, core 1's running number starting at this correction (the
                                 * table's units, 1/100 % of a block; positive = core 1 gets fewer voices). After every finished job: half of (core 1's finish stamp - the moment core 0 came to its wait),
                                 * so the two cores finish their block together within a voice. Never in the deadline's numbers (tcr.f0 / f1 keep the table's view). Which core plays a voice never changes
                                 * its samples */
#if TC_MEAS
    uint32_t vm_t[3];           /* the bench's timers of this block: before the control pass, after it, after core 0's own sets */
#endif
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
    int32_t lm_min, ld_last;            /* the very late jobs: the least wait limit less core 1's end, and the latest wait limit less the take-back point (BENCH-HEAVY S2 freezes, 2026-11-05) */
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
        {   int32_t lm = (int32_t)(tcr.lim - te);
            if (tcm.nv == 1u || lm < tcm.lm_min)
                tcm.lm_min = lm;
            tcm.ld_last = (int32_t)(tcr.lim - tcr.dl);
        }
        tcm.vrt_sum += (uint32_t)rt;
        if ((uint32_t)rt > tcm.vrt_max)
            tcm.vrt_max = (uint32_t)rt;
        if ((uint32_t)sd > tcm.vsd_max)
            tcm.vsd_max = (uint32_t)sd;
        e->blk = tcj.blk, e->at = tcj.at - tcm.b0, e->a1 = tcj.a1, e->a2 = tcj.a2, e->sd = sd, e->rt = rt, e->mg = mg;
    }
}
/* BENCHVD (2026-11-05, owner: "let's find out why do we have late blocks"): where core 0's time goes inside a block, split by the block's place in the half (the first block; the three after it).
 * Marks through fx.c mix_block and the runner (TSG): the events, the snapshot and the runner's begin, the gather of the block before, the control pass, the cost table (tcr_costs: the first block of a
 * half only), the deal with the record's hash and the post, core 0's own sets, the sends, the master, the wait for core 1, the rest (core 1's share when core 0 plays it, the close). Kept: per place
 * the largest and the sum of each piece, and the pieces of the ONE longest block of the test. Ticks (24 a us); `bench late` prints them in us (bench.c). Cleared with the other figures */
enum { SG_EV, SG_SNAP, SG_GATHER, SG_CTL, SG_COST, SG_POST, SG_OWN, SG_SENDS, SG_MASTER, SG_WAIT, SG_REST, SG_TAIL, SG_MORPH, SG_OPEN, SG_NS, SG_NSN, SG_EVTRK, SG_EVKEY, SG_EVMIDI, SG_EVCC, SG_EVCCN, SG_N };   /* (2026-11-08, the WAVE crackle: SG_NS the block's note starts in all (inside SG_EV), SG_NSMO their MORPH windows, SG_NSN how many (stored x ticks a us)) */
/* (SG_TAIL, 2026-11-07: the ISR's work after the mix: audio.c audio_block; SG_MORPH / SG_OPEN: the control pass split: morph_begin, part_open (the matrix, the LFOs), then the voices' control steps stay in SG_CTL) */
typedef struct { uint32_t n[2], max[2][SG_N], sum[2][SG_N]; } tsg_t;   /* (2026-11-07: the longest block's pieces and the note-start timers went: the flash is full) */
static tsg_t tsg TC_BSS;
static uint32_t tsg_last, tsg_blk, tsg_cur[SG_N];
static uint32_t tsg_h[4][SG_N];                       /* (2026-11-08, the WAVE crackle: this half's blocks, piece by piece: bench.c bench_late keeps them for a missed half) */
#define TSG(k) do { uint32_t t_ = TC_TICKS(); tsg_cur[k] += t_ - tsg_last; tsg_last = t_; } while (0)
static void tsg_begin(void)
{
    uint32_t i;
    for (i = 0; i < SG_N; i++)
        tsg_cur[i] = 0;
    tsg_blk = tcr.blk;
    ns_ticks = ns_n = ns_mo = 0;
    memset(ev_t, 0, sizeof ev_t);
    tsg_last = TC_TICKS();
}
static void tsg_end(void)                               /* (after the ISR's tail: audio.c audio_block; the mix's own rest was marked at the end of mix_block) */
{
    uint32_t i, b = tsg_blk ? 1u : 0u;
    TSG(SG_TAIL);
    tsg_cur[SG_NS] = ns_ticks, tsg_cur[SG_NSN] = ns_n * TC_TICKS_PER_US;
    memcpy(&tsg_cur[SG_EVTRK], ev_t, sizeof ev_t);
    for (i = 0; i < SG_N; i++) {
        tsg.sum[b][i] += tsg_cur[i];
        if (tsg_cur[i] > tsg.max[b][i])
            tsg.max[b][i] = tsg_cur[i];
    }
    tsg.n[b]++;
    if (tsg_blk < 4u)
        memcpy(tsg_h[tsg_blk], tsg_cur, sizeof tsg_cur);
}
#define TSG_BEGIN() tsg_begin()
#define TSG_END() tsg_end()
static void tcm_clear(void)
{
    uint32_t b0 = tcs.blocks - 1u;                      /* (this block is counted already and is the test's first) */
    memset(&tcm, 0, sizeof tcm);
    memset(&tsg, 0, sizeof tsg);
    memset(&vsm, 0, sizeof vsm);                        /* (voice.c: the share path's timing) */
    tcm.b0 = b0;
    tcr.fl_n = 0, tcr.fl_cur = tcr.fl_prev = 0;        /* (the floor starts again with the test) */
}
#endif

#if !TC_MEAS
#define TSG(k) ((void)0)                                /* (the block's segment timers: developer bench builds only) */
#define TSG_BEGIN() ((void)0)
#define TSG_END() ((void)0)
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

/* THE VOICE SPLIT (owner 2026-11-05; working docs/design studies/voice-split-equal-parts-size_2026-11-05.md): no part owns a core. Core 0 runs the control pass of every part (fx.c part_ctl), deals
 * each listed voice to the core that is less full (tcr_deal_post) and posts ONE job: core 1 renders its sets and then runs each part's own stages (fx.c part_stages); core 0 renders its own sets
 * meanwhile. tc_blk is the block's record: core 0 writes it before the post, core 1 reads it after its claim, core 0 only reads it until the done word. The voices' control results are in vr_m.
 * The check value covers the snapshot, the record and the vr_m records of core 1's ranks: a misread or stale record is refused like a wrong snapshot (tc_job_refuse). After the post core 1 writes its
 * own words, pscr_c1, the voices of its ranks and, once core 0 has said a part's own set is in (tc_mb.c0v), that part's buffers, accumulators and effect states */
typedef struct {
    mixpart_t m[NLIVE];
    vpass_t vp[NLIVE];
    uint32_t sel[NLIVE];        /* the ranks of each part's render list that core 1 renders */
    uint32_t n;                 /* the block length: always CTL (checked) */
    int32_t *out;               /* THE GLOBALS (2026-11-07): the block's output, PERFORM's flag and the master level: what core 1's first phase needs (out: for the record; core 1 writes outc1, never the DMA buffer) */
    int32_t mg;
    int perf;
    uint8_t mo[NLIVE];          /* the part's morph window is open (core 0 closes it when the block's job is over) */
} __attribute__((aligned(64))) tc_blk_t;
static tc_blk_t tc_blk TC_CTX_SECT;
static int32_t tc_outc1[2 * CTL] TC_CTX_SECT;          /* core 1's output block (the master of the block before, interleaved L R): core 0 copies it into the DMA buffer after the done word, so a late core 1
                                                         * never touches that buffer. Outside the record: core 1 writes it while core 0 works out the record's check value */
static part_scr_c1_t pscr_c1b TC_CTX_SECT;              /* (2026-11-09) core 1's set of PART 2 (pscr_c1 holds part 1's): core 1 renders both its sets before its first wait for core 0, so it no longer stands idle
                                                         * between them while core 0 renders part 1. 256 B in the pool's tail */
_Static_assert(sizeof(tc_blk_t) % 4u == 0, "twocore_run.c: the record is hashed in whole words");
_Static_assert(sizeof(vmod_t) % 4u == 0, "twocore_run.c: vmod_t is hashed in whole words");
_Static_assert(NVOICE <= 16u, "twocore_run.c: a set is 16 bits of ranks");
_Static_assert(VR_RECS >= NVOICE && VR_RECS <= 2 * NVOICE, "twocore_run.c: the hash covers the VR_RECS records");

/* 16 bits over the snapshot, the record and the vr_m records of core 1's ranks (FNV over the words) */
static uint32_t tc_blk_hash(const blockstate_t *bs)
{
    const uint32_t *w = (const uint32_t *)bs;
    uint32_t i, p, r, h = 0x811C9DC5u;
    for (i = 0; i < sizeof *bs / 4u; i++)
        h = (h ^ w[i]) * 0x01000193u;
    w = (const uint32_t *)&tc_blk;
    for (i = 0; i < sizeof tc_blk / 4u; i++)
        h = (h ^ w[i]) * 0x01000193u;
    for (p = 0; p < NLIVE; p++) {
        const vpass_t *vp = &tc_blk.vp[p];
        for (r = 0; r < vp->nl && r < NVOICE && vp->r0 + r < VR_RECS; r++)   /* (2026-11-07: every record, both parts') */
            if ((tc_blk.sel[p] >> r) & 1u) {
                w = (const uint32_t *)&vr_m[vp->r0 + r];
                for (i = 0; i < sizeof(vmod_t) / 4u; i++)
                    h = (h ^ w[i]) * 0x01000193u;
            }
    }
    return (h ^ h >> 16) & 0xFFFFu;
}

/* a part's set of ranks rendered into pscr_c1 (cleared first) and added to the part's own buffers: core 1's share of the part, on whichever core runs it */
static void tc_set_run(const vpass_t *vp, uint32_t sel, part_scr_c1_t *c1)
{
    vrctx_t rc = vp->rc;
    uint32_t k;
    rc.out = c1->buf, rc.sbuf = c1->sbuf;
    for (k = 0; k < CTL; k++)
        c1->buf[k] = c1->sbuf[k] = 0;
    vs_render_set_c1(&rc, vp->rl, vp->nl, sel, vr_m + vp->r0);
}

/* core 1: the block's job, IN TWO PHASES (2026-11-07; owner: core 0 waited 70 to 140 us of every 725 us block for core 1, and core 1 stood idle for the first 140 us of each, while core 0 ran the sends
 * and the master itself: STRESS-LADDER). The job is posted right after the gather, before the control pass (tcr_post). Phase 1, at once: the globals of the block before (fx.c mix_globals: the sends,
 * the meters, the master into tc_blk.out), then the gdone word. Phase 2: the deal word (tc_mb.deal == this job: core 0's control pass is over, the sets are dealt, tc_blk and the vr_m records are final,
 * checked by dealchk), then its sets of both parts (pscr_c1, pscr_c1b), then per part (core 0's own set of the part is in: tc_mb.c0v) the merge and the part's own stages. arg: n | the snapshot's hash << 16 */
static void tc_job_run(uint32_t arg, uint32_t j)
{
    uint32_t n = arg & 0xFFFFu, p;                      /* (j: the number tc_c1_step verified against the check value) */
    if (tc_ctx_hash(&tc_ctx.bs) != arg >> 16 || n != CTL || tc_blk.n != CTL) {   /* (a block is always CTL frames; the stages get the constant CTL, not n: with a variable n the compiler stops folding the block length in the hot filter code) */
        tc_job_refuse();
        return;
    }
    mix_globals(tc_outc1, CTL, tc_blk.perf, tc_blk.mg);   /* phase 1, into core 1's own block */
    TC_SYNC();                                          /* (out and the buses' states before the word) */
    TC_ST(&tc_mb.gdone, j);
    while (TC_LD(&tc_mb.deal) != j)                     /* phase 2: the deal (core 0 writes it for every posted job, with every set empty when the half is too far gone) */
        ;
    TC_SYNC();
    if (TC_LD(&tc_mb.dealchk) != TC_CHECK(j, tc_blk_hash(&tc_ctx.bs))) {   /* a misread or stale record: refused; the globals are done (gdone), core 0 plays the sets and the stages */
        tc_job_refuse();
        return;
    }
    for (p = 0; p < NLIVE; p++)                         /* (2026-11-09) both its sets first, each into its own pair: no wait between them */
        if (tc_blk.sel[p])
            tc_set_run(&tc_blk.vp[p], tc_blk.sel[p], p ? &pscr_c1b : &pscr_c1);
    TC_ST(&tc_mb.trdy, TC_TICKS());                     /* (the balance's stamp, 2026-11-09: its voices done; what follows, the merges and the stages, waits for core 0's sets whatever the split) */
    for (p = 0; p < NLIVE; p++) {
        const vpass_t *vp = &tc_blk.vp[p];
        while (!((TC_LD(&tc_mb.c0v) >> p) & 1u))       /* (core 0 renders its own sets right after the post, before anything else) */
            ;
        TC_SYNC();
        if (tc_blk.sel[p])
            vs_merge(vp->rc.out, vp->rc.sbuf, vp->rc.spread_on, p ? &pscr_c1b : &pscr_c1);
        part_stages(&tc_blk.m[p], vp);
    }
}

/* ---------------------------------------------------------------- the deadline (core 0) --- */
static uint32_t tcr_active(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active != 0;
    return n;
}
/* THE DEADLINE AND THE WAIT LIMIT, for the voice split's one-block order (2026-11-05; the first unit run with the old formula, BENCHVB, dropped one block at every chord start: it still took the
 * sends and the master of this block off both numbers, although core 0 now runs them BEFORE its wait; working docs/reports/voice-split-bench_2026-11-05.md).
 * When core 0 comes to its wait (tcr_join) this block's control pass, its own sets and the sends and the master are done. What is left of the half for core 0:
 *   if core 1 finishes its job     the blocks still to come
 *   if core 0 must do the job      core 1's share of this block (tcr.f1: the parts' own stages and the voices dealt to core 1), then the blocks still to come
 * A block still to come costs core 0 its own load of this block's deal (tcr.f0: the sends and the master and its voices) under the two-core NOTES rule (tcr.two; DUELL 1.1 item 90: the parts may
 * hold more notes than one core can play, core 1 plays its share there again), and everything (tcr.f0 + tcr.f1) under the one-core rule (the worst case: core 1 late again). So
 *   the take-back deadline = the half's end - (f1 + later blocks) - TCR_SLACK      a job core 1 has not STARTED by then is played by core 0, no click
 *   the wait limit         = the half's end - later blocks                         core 0 never waits past it; the later of this (the 1.15 table) and the same at TRUE cost less TCR_SLACK
 *                                                                                  (item 142), never after the half's end
 * The costs are the NOTES table of budget.c (1/100 % of a half's time for a block's worth; part_dl, global_cost_dl: a 15 % margin in them), read at a half's first block (tcr_costs); the deal's numbers
 * are this block's (tcr_deal runs before this). The voice items of the table hold a voice's control step too, which core 0 always runs: a small overcount of core 1's share, on the safe side for the
 * deadline. Not yet refitted to costs measured on the unit with this runner (owner 2026-11-05: measure, then set the voice limit) */
#define TCR_TRUE(x) (((x) * 100u + BUDGET_DL_MARGIN - 1u) / BUDGET_DL_MARGIN)   /* a 1.15 item back at its true cost, rounded up (>= the exact 1.00 item: item 137) */
/* the table's items, taken at a half's first block: the copy the main loop handed over (budget.h bd_dl_publish; a few words to read: working them out here cost 171 to 188 us in every half's first
 * block on the unit, BENCHVD); before the main loop's first pass, and in host tests without one, worked out here as before */
static void tcr_costs(void)
{
    if (tcr.blk == 0 || !tcr.cost_ok) {
        if (bd_dl_ok) {
            const bd_dl_t *d = &bd_dl_buf[bd_dl_i];
            tcr.c1f = d->c1f, tcr.c1v = d->c1v, tcr.c2f = d->c2f, tcr.c2v = d->c2v, tcr.g = d->g;
        } else {
            part_dl(&trk[0], &tcr.c1f, &tcr.c1v);       /* (item 142: one bd_part a part, was two) */
            part_dl(&trk[1], &tcr.c2f, &tcr.c2v);
            tcr.g = global_cost_dl();
        }
        tcr.two = (uint8_t)bd_two_now();
        tcr.t1f = TCR_TRUE(tcr.c1f), tcr.t1v = TCR_TRUE(tcr.c1v), tcr.t2f = TCR_TRUE(tcr.c2f), tcr.t2v = TCR_TRUE(tcr.c2v), tcr.tg = TCR_TRUE(tcr.g);
        tcr.cost_ok = 1;
    }
}
/* THE DEAL (owner 2026-11-05): each listed voice, in voice order over the parts, goes to the core that is less full at that moment. One running number a core, in the budget's units: core 0 starts with
 * the sends and the master (tcr.g), core 1 with the parts' own stages (tcr.c1f + tcr.c2f); a voice adds its part's voice cost. The placement measures nothing of the voice itself. Writes the block's
 * sets (tc_blk.sel) and the running numbers the deadline counts with */
static void tcr_deal(void)
{
    /* THE SPLIT (2026-11-07): the voices by their costs alone, core 0's number from 0 and core 1's from the measured balance (tcr.bias): the table's fixed parts (core 0: nothing it charges for the control
     * pass; core 1: the globals and the parts' stages) are not what decides, the measurement is. The deadline's numbers (tcr.f0 / f1) are the table's: core 1's share is the globals, the stages and its voices */
    uint32_t p, r, f0 = 0, f1 = tcr.g + tcr.c1f + tcr.c2f, f0t = 0, f1t = tcr.tg + tcr.t1f + tcr.t2f;
    int32_t v0 = 0, v1 = tcr.bias;
    /* (2026-11-10: the block's own extra of 2026-11-08 is gone. It gave core 1 the voices of a block whose control pass ran long, but core 1 can start them only at the deal, as core 0 does:
     * in the knob test's late halves core 0 rendered 4 us of voices and then waited 300 to 530 us for core 1. Both cores start at the deal, so the deal splits what follows by the balance alone) */
    for (p = 0; p < NLIVE; p++) {
        const uint32_t cv = p ? tcr.c2v : tcr.c1v, cvt = p ? tcr.t2v : tcr.t1v;
        tc_blk.sel[p] = 0;
        for (r = 0; r < tc_blk.vp[p].nl; r++)
            if (v1 <= v0)
                tc_blk.sel[p] |= 1u << r, v1 += (int32_t)cv, f1 += cv, f1t += cvt;
            else
                v0 += (int32_t)cv, f0 += cv, f0t += cvt;
    }
    tcr.f0 = f0, tcr.f1 = f1, tcr.f0t = f0t, tcr.f1t = f1t;
}
/* THE BALANCE after a finished job (tcr_parts_b): d = core 1's finish stamp less the moment core 0 came to its wait, in ticks (positive: core 0 waited that long; negative: core 1 was done that much
 * earlier). Half of it goes onto the correction, in 1/100 % of a block, so the error halves each block and the split settles within a voice of the balance. Held within half a block each way */
#define TCR_BIAS_LIM 5000
#define TCR_BIAS_STEP 1500                              /* (a block's step at most 15 % of a block, about one voice: one odd block cannot throw the split) */
#define TCR_BIAS_AIM 0                                  /* the aim: core 1 is to finish this many ticks before core 0 comes to its wait. 0 (2026-11-07, STRESS-RAMP on bench B1107G): an aim of 50 us changed nothing
                                                         * at 3 voices (the wait there is the best split: core 1's fixed work, the globals and the stages, is longer than core 0's work after the post, and a
                                                         * voice moved to core 0 would cost it more than the wait), and at many voices it would make core 0 take half a voice more than the balance */
static void tcr_bias(int32_t d)
{
    int32_t s, b;
    d += TCR_BIAS_AIM;
    d = d < -(int32_t)TCR_BLOCK_TICKS ? -(int32_t)TCR_BLOCK_TICKS : d > (int32_t)TCR_BLOCK_TICKS ? (int32_t)TCR_BLOCK_TICKS : d;   /* (a block each way at most: 32-bit arithmetic, no 64-bit divide in the flash) */
    s = d * 5000 / (int32_t)TCR_BLOCK_TICKS;
    s = s < -TCR_BIAS_STEP ? -TCR_BIAS_STEP : s > TCR_BIAS_STEP ? TCR_BIAS_STEP : s;
    b = tcr.bias + s;
    tcr.bias = b < -TCR_BIAS_LIM ? -TCR_BIAS_LIM : b > TCR_BIAS_LIM ? TCR_BIAS_LIM : b;
}
static uint32_t tcr_deadline(uint32_t now, int *nofit)
{
    const uint32_t left = TCR_BLOCKS - 1u - tcr.blk;    /* the blocks of the half still to come */
    uint32_t later = left * (tcr.two ? tcr.f0 : tcr.f0 + tcr.f1), need, dl, cap;
    if (later > TCR_NEED_MAX)
        later = TCR_NEED_MAX;
    need = tcr.f1 + later;
    if (need > TCR_NEED_MAX)
        need = TCR_NEED_MAX;
    cap = tcr.t0 + TCR_HALF_TICKS - need * TCR_BLOCK_TICKS / 10000u;   /* the latest moment core 0 can still play core 1's share itself and finish the half (no slack) */
    dl = cap - TCR_SLACK_NOW;                           /* the take-back deadline */
    {   /* the wait limit: the later of the 1.15 table's and the true-cost one less TCR_SLACK (item 142), never after the half's end */
        uint32_t rt = left * (tcr.two ? tcr.f0t : tcr.f0t + tcr.f1t);
        int32_t lo, lt;
        rt = rt > TCR_NEED_MAX ? TCR_NEED_MAX : rt;
        lo = (int32_t)TCR_HALF_TICKS - (int32_t)(later * TCR_BLOCK_TICKS / 10000u);        /* (ticks after the half's start; negative: before it) */
        lt = (int32_t)TCR_HALF_TICKS - (int32_t)(rt * TCR_BLOCK_TICKS / 10000u) - (int32_t)TCR_SLACK_NOW;
        lo = lt > lo ? lt : lo;
        lo = lo < (int32_t)TCR_HALF_TICKS ? lo : (int32_t)TCR_HALF_TICKS;
        tcr.lim = tcr.t0 + (uint32_t)lo;
    }
    *nofit = 0;
#if TC_MEAS
    tcr.cap = cap;
    /* BENCHT3, the floor: the deadline is not earlier than core 1's own measurement says it needs (now + its recent maximum run time + the slack), but never later than
     * cap, so core 0 can still play the job itself without a miss. If now + the recent maximum is already past cap, the job would be very late whatever the deadline:
     * no job (nofit), core 0 plays everything now; only when the table's deadline itself was still ahead (else it is the plain "deadline past") */
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

/* DUELL 1.1 item 137: the take-over at true cost. Called only under the two-core rule when tcr_deadline's deadline (the 1.15 table) is already past at the post, i.e. when there would be no take-over
 * at all (tcr_deal_post's "nott" path: a core 1 that has not started by the wait limit = the parts silent + a pause). The same sum as tcr_deadline with the deal's numbers at true cost, less the same
 * slack. If that moment is still ahead of now and before the wait limit, it is the take-over deadline: a job core 1 has not started by then is played by core 0 (no click); a job core 1 is running is
 * waited for up to the wait limit, exactly as before. Otherwise dl comes back unchanged (the "nott" path). The wait limit is not touched. */
static uint32_t tcr_deadline_true(uint32_t now, uint32_t dl)
{
    uint32_t need = tcr.f1t + (TCR_BLOCKS - 1u - tcr.blk) * tcr.f0t, cap, dt;   /* (two-core rule only: the later blocks at core 0's own load, as tcr_deadline) */
    if (need > TCR_NEED_MAX)
        need = TCR_NEED_MAX;
    cap = tcr.t0 + TCR_HALF_TICKS - need * TCR_BLOCK_TICKS / 10000u;
    dt = cap - TCR_SLACK_NOW;
#if TC_MEAS
    if (TCR_FLOOR_NOW) {   /* item 142: BENCHT3's floor (a diagnosis switch, off by default) on this deadline too, as tcr_deadline has it */
        uint32_t rm = tcr.fl_cur > tcr.fl_prev ? tcr.fl_cur : tcr.fl_prev;
        if (rm) {
            if ((int32_t)(now + rm - cap) > 0)
                return dl;
            if ((int32_t)(now + rm + TCR_SLACK_NOW - dt) > 0)
                dt = (int32_t)(now + rm + TCR_SLACK_NOW - cap) > 0 ? cap : now + rm + TCR_SLACK_NOW;
        }
    }
#endif
    if ((int32_t)(dt - now) > 0 && (int32_t)(dt - tcr.lim) < 0)
        return dt;
    return dl;
}
/* the deadline a job is posted with (tcr_deal_post; tests/deadline_test.c checks it for every setup): tcr_deadline, and under the two-core rule, when that is already past, the true-cost one */
static uint32_t tcr_post_deadline(uint32_t now, int *nofit)
{
    uint32_t dl = tcr_deadline(now, nofit);
    if (tcr.two && (int32_t)(dl - now) <= 0 && !*nofit)
        dl = tcr_deadline_true(now, dl);
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
static uint8_t tc_say;          /* DUELL 1.1 (owner 2026-10-25): 1 = the unit plays on one core and the screen has not said it yet (ui_input.c ui_notices says it once, then clears this) */
static uint8_t tc_boot_looked;  /* the power-on state of core 1 was looked at (tc_trim_poll) */
static void tc_trim_poll(void)
{
    uint8_t on = tcr.paused || tc.failed;
    if (on && !tcr.trim_on) {
        tcr.trim = 3u;
        tc_say = 1;                                     /* (one core now, for a pause or for the rest of the session) */
    }
    tcr.trim_on = on;
    if (!tc_boot_looked) {                              /* the first pass: core 1 did not start at this power-on (the last session ended in a fault, or the boot guard skipped it) */
        tc_boot_looked = 1;
        if (TC_AUTO_ARM && !tc.up && !tc.failed)
            tc_say = 1;
    }
}

enum { TCR_PW_FAULT, TCR_PW_RUN, TCR_PW_WIN, TCR_PW_LATE };   /* why the jobs pause (BENCHT3): a real fault / a run of very late blocks / K very late blocks in the window / a run of late blocks */
static void tcr_pause(uint32_t now, uint32_t why)
{
    if (!tcr.paused) {
        tcr.paused = 1;
        tcr.bias = 0;                                   /* (the balance starts afresh after a pause) */
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

/* the parts' block is over on this core's side: their morph windows closed. dead: core 1 stopped inside the job, so the matrix's window may still be open and the block's sums are not to be read */
static void tcr_close(uint32_t dead)
{
    uint32_t p;
    for (p = 0; p < NLIVE; p++) {
        if (dead) {
            if (MODW(&trk[p]).on)
                mod_end(&trk[p]);
            pacc_on[p] = 0;
        }
        if (tc_blk.mo[p])
            morph_end(&trk[p]);
        tc_blk.mo[p] = 0;
    }
}

/* an old job: core 1 stopped (its words are final: a claimed job may have left the parts half written), or it is closed now (a block that finished after its wait limit: the parts are whole again, its
 * sums are thrown away) */
static void tcr_settle(void)
{
    if (!tcr.open)
        return;
    if (!tc.up) {
        uint32_t a = tc_answer(tcr.open);
        if (a == TCA_CLAIMED || a == TCA_BAD)
            tcr.fade = 1;
    } else if (!tc_closed()) {
        return;
    } else {
#if TC_MEAS
        tcm_collect(tcr.open);
#endif
    }
    tcr.open = 0;
    tcr_close(1);
}

/* after the block's snapshot, before the parts: settle the old job, the one-core rule, resume after the calm period */
static void tcr_begin(void)
{
    uint32_t now = TC_TICKS();
    tcs.blocks++;
    tcr.job = 0;                                        /* (tcr.skip stays: it is the block before's, for this block's gather; tcr_parts_a starts it again) */
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
    {   /* DUELL 1.1 item 90: every voice on core 0 from this block (core 1 down, paused, or a flash park): the one-core numbers hold at once (trk_nvoice reads bnotes1), and the
         * notes above them fade now (voice_trim_now: the one-block fade), not at the main loop's next pass */
        uint8_t one = (uint8_t)(!tc.up || tc.park || tcr.paused TCR_OR_NOCORE1);
        if (one && !bd_one_now)
            tcr.trim = 3u;
        bd_one_now = one;
    }
    if (tcr.paused && (int32_t)(now - tcr.pause_t - TCR_CALM_NOW) >= 0) {
        tcr.paused = 0;
        tcr.late_run = tcr.vlate_run = 0;
        tcr.vw_n = 0;
        tcs.resumes++;
    }
}

/* THE POST (2026-11-07, the two-phase job), right after the gather and before the control pass: the block's snapshot and the globals' inputs into the record, then the job, so core 1 starts the sends
 * and the master of the block before while core 0 runs the control pass. No job (tcr.pj 0) when core 1 is down, paused or parked, or when an older job is still open (then the parts of this block are
 * silent: tcr.skip, as before); core 0 then runs the globals itself in tcr_parts_b */
static void tcr_post(const blockstate_t *bs, int32_t *out, int perf, int32_t mg)
{
    tcr.out = out, tcr.perf = perf, tcr.mg = mg;
    tcr.skip = 0;
    tcr.ph = 0;
    tcr.pj = 0;
    if (tcr.open) {                                     /* core 1 still holds the parts of an older block: nothing of them is touched, they are silent */
        tcs.wait++;
        tcs.silent++;
        tcr.skip = 1;
        return;
    }
    tc_ctx.bs = *bs;                                    /* (the snapshot both cores read: core 0 writes it only here, before the post; the next block's gather reads it too) */
    tc_blk.n = CTL;
    tc_blk.out = out;
    tc_blk.perf = perf;
    tc_blk.mg = mg;
    if (!tc.up || tc.park || tcr.paused TCR_OR_NOCORE1) {
#if TC_MEAS
        tcr.own_why = !tc.up || TCR_NOCORE1_NOW ? TCM_OWN_DOWN : tc.park ? TCM_OWN_PARK : TCM_OWN_PAUSE;
#endif
        return;
    }
#if TC_MEAS
    if (TCR_FLOOR_NOW) {                                /* BENCHT3's floor (a diagnosis switch, off by default), before the post (2026-11-07: the job is posted before the deal now, so the "does not fit"
                                                         * decision is made here, with the last block's deal numbers as the estimate of core 1's share): core 1's recent longest run would end after the
                                                         * latest moment core 0 could still play the job itself: no job, core 0 plays everything */
        uint32_t rm = tcr.fl_cur > tcr.fl_prev ? tcr.fl_cur : tcr.fl_prev, left = TCR_BLOCKS - 1u - tcr.blk, need, cap;
        need = tcr.f1 + left * (tcr.two ? tcr.f0 : tcr.f0 + tcr.f1);
        need = need > TCR_NEED_MAX ? TCR_NEED_MAX : need;
        cap = tcr.t0 + TCR_HALF_TICKS - need * TCR_BLOCK_TICKS / 10000u;
        if (rm && (int32_t)(TC_TICKS() + rm - cap) > 0) {
            tcr.own_why = TCM_OWN_FL;
            return;
        }
    }
#endif
    tcr.c0v = 0;
    TC_ST(&tc_mb.c0v, 0u);
#if TC_MEAS
    tcj.t_post = TC_TICKS();                            /* (before the post, so core 1 cannot see the job before this tick) */
    tcj.blk = tcr.blk;
    tcj.t0 = tcr.t0;
    tcj.at = tcs.blocks;
    tcj.vl = 0;
    tcm.posted++;
#endif
    tcr.job = tc_post(CTL | (uint32_t)tc_ctx_hash(&tc_ctx.bs) << 16);
    tcr.open = tcr.job;
    tcr.pj = tcr.job;
    tcs.sh_posts++;
}
/* THE DEAL, after every part's control pass (the deal: tcr_deal above), and the deal word core 1 waits for. Every posted job gets the word; when the half is too far gone for a take-back, every set is
 * core 0's and core 1 is waited for up to the wait limit (it is in the globals already, or will yield). The deadline the wait uses (tcr.dl) is this block's */
static void tcr_deal_word(void)
{
    uint32_t now, dl, p, j = tcr.pj;
    int nofit;
    for (p = 0; p < NLIVE; p++)
        tc_blk.sel[p] = 0;
    if (!j)
        return;
    now = TC_TICKS();
    tcr_costs();
    TSG(SG_COST);
    tcr_deal();
#if TC_MEAS
    tcj.a1 = tcr_active(&trk[0]);                       /* (the voices sounding, for the bench's list of very late jobs) */
    tcj.a2 = tcr_active(&trk[1]);
#endif
    dl = tcr_post_deadline(now, &nofit);
    tcr.nott = 0;
    if (tcr.two && ((int32_t)(dl - now) <= 0 || nofit)) {   /* item 90: two-core numbers sound and core 0 can no longer take the job back in time: no take-over */
        tcr.nott = 1;
        dl = tcr.lim;
        nofit = 0;
    }
    if ((int32_t)(dl - now) <= 0 || nofit) {            /* the half is too far gone to wait for core 1's voices: every voice is core 0's; the job is posted already (core 1 is in the globals): the wait takes it
                                                         * back at once (a core 1 that has not claimed yields; one that has is waited for to the limit, as any late job). Not an "own" block: the job goes on */
        for (p = 0; p < NLIVE; p++)
            tc_blk.sel[p] = 0;
    }
    tcr.dl = dl;
    TC_ST(&tc_mb.dealchk, TC_CHECK(j, tc_blk_hash(&tc_ctx.bs)));
    TC_SYNC();                                          /* (the record, the vr_m records and the check value before the word) */
    TC_ST(&tc_mb.deal, j);
    TC_SYNC();
}

/* the wait for the block's job, after core 0's own sets. Returns 0: core 1 is done; 1: core 1 never touched the job (it had not started, or refused it unread): core 0 does that share itself, the
 * same samples; 2: the parts are silent this block (core 1 holds them past the wait limit: the job stays open; or it stopped inside the job) */
static uint32_t tcr_join(uint32_t j)
{
    uint32_t a, lim = tcr.lim, t1;
    int vl = 0, noans = 0;
    tcr.job = 0;
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
        if (a == TCA_YIELD) {                           /* it had not started: core 0 does it, no click */
            tcs.late++;
            tcs.sh_late++;
            tcr.open = 0;
            tcr.vlate_run = 0;                          /* (owner rule G21, 2026-11-05: NO REST AFTER A LATE BLOCK. A job core 1 had not started is played by core 0 and that is all: the pause after a run of
                                                         * late blocks, and the one for a take-back with no time left, are gone. A late block is a defect to find at its cause) */
            return 1u;
        }
        if (a == TCA_CLAIMED || a == TCA_NONE) {        /* it is on it (or stuck): wait for done, not past the wait limit */
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
            if (a == TCA_YIELD) {                       /* (no answer in time, then yld: it never touched the parts) */
                tcr_pause(TC_TICKS(), TCR_PW_FAULT);
                tcr.open = 0;
                return 1u;
            }
            if (a == TCA_CLAIMED || a == TCA_NONE) {    /* not done by the wait limit: the parts are core 1's until tcr_settle sees the job closed */
                if (a == TCA_NONE)                      /* (G21: a job core 1 is WORKING on and only late pauses nothing: the next block's job is posted as soon as this one closes. A core 1 that
                                                         * gave no answer at all is not late but stuck: that stays a fault) */
                    tcr_pause(TC_TICKS(), TCR_PW_FAULT);
                tcs.silent++;
                tcs.sh_freeze++;
                return 2u;
            }
            /* done inside the wait limit: a real fault only if there was no answer to the take or the answer is wrong (below); else a very late block that did no harm */
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
        tcs.sh_done++;
#if TC_MEAS
        tcm_collect(j);
#endif
        tcr.open = 0;
        return 0;
    }
    if (vl)
        tcs.vlate--;                                    /* (counted as refused, once) */
    {   /* refused (a check value, a snapshot): core 1 ran nothing of it (bad_job = this job, or no done word). A word ahead, or a bad flag of another cause: core 1 may have run part of it; it is
         * stopped now (tc_fail), the parts are silent this block and their voices fade before the next one */
        const int ran = !(a == TCA_REFUSED || (a == TCA_DONE && TC_LD(&tc_mb.bad_job) == j));
        tc_fail(a == TCA_BAD ? TCR_BADWORD : TCR_BADJOB);
        tcs.refused++;
        tcs.sh_fault++;
        tcr.open = 0;
        if (!ran)
            return 1u;
        tcr.fade = 1;
        return 2u;
    }
}

/* THE PARTS of a block, in two calls with core 0's globals between them (fx.c mix_block). THE ONE-BLOCK STEP (owner 2026-11-05): the sends and the master work on the parts' sums of the block
 * BEFORE (mix_gather took them before tcr_parts_a, with that block's snapshot, which is still in tc_ctx), so core 0 runs them while core 1 renders this block's voices and the parts' own stages.
 * Everything sounds one block (0.7 ms) later, dry and wet alike; with no core 1 the order is the same, so the delay never changes.
 *   tcr_parts_a   the control pass of every part, the deal and the post, core 0's own sets
 *   (fx.c: mix_sends, the meters, mix_master of the block before)
 *   tcr_parts_b   the wait, whatever core 1 did not do, the parts' windows closed
 * tcr.skip: the parts of THIS block are silent (the next block's gather reads it) */
static void tcr_parts_a(const blockstate_t *bs, uint32_t n)
{
    uint32_t p, i, nrec = 0, j = tcr.pj;
    (void)bs;
    if (tcr.skip)                                       /* (tcr_post: an older job still holds the parts) */
        return;
    for (p = 0; p < NLIVE; p++) {
        track_t *t = &trk[p];
        if (tcr.fade)                                   /* (after a core 1 that stopped in the middle of the parts) */
            for (i = 0; i < NVOICE; i++)
                if (t->v[i].active && t->v[i].stage != 4u)
                    voice_kill(&t->v[i]);
        if ((tcr.trim >> p) & 1u) {
            tcr.trim &= (uint8_t)~(1u << p);
            tcs.trims += voice_trim_now(t);
        }
    }
    tcr.fade = 0;
#if TC_MEAS
    tcr.vm_t[0] = TC_TICKS();
#endif
    for (p = 0; p < NLIVE; p++) {
        tc_blk.mo[p] = (uint8_t)(morph_begin(&trk[p], 1) != 0);
        TSG(SG_MORPH);
        part_open(&tc_blk.m[p], &trk[p], n, &tc_ctx.bs);   /* (part_ctl's two halves, so the block's timing sees the part's own work (the matrix, the LFOs) apart from the voices' control steps) */
        TSG(SG_OPEN);
        track_ctl(&trk[p], tc_blk.m[p].b, &tc_blk.vp[p], &nrec);
    }
#if TC_MEAS
    tcr.vm_t[1] = TC_TICKS();
#endif
    TSG(SG_CTL);
    tcr_deal_word();
    TSG(SG_POST);
    for (p = 0; p < NLIVE; p++) {                       /* core 0's own sets, into the parts' own buffers */
        const vpass_t *vp = &tc_blk.vp[p];
        vs_render_set(&vp->rc, vp->rl, vp->nl, ~tc_blk.sel[p] & ((1u << vp->nl) - 1u), vr_m + vp->r0);
        if (j) {
            TC_SYNC();                                  /* (the set's samples before the word core 1 waits for) */
            tcr.c0v |= (uint8_t)(1u << p);
            TC_ST(&tc_mb.c0v, tcr.c0v);
        }
    }
    tcr.t0r = TC_TICKS();                               /* (the balance: core 0's own sets are in) */
#if TC_MEAS
    tcr.vm_t[2] = TC_TICKS();
#endif
    TSG(SG_OWN);
    tcr.pj = j;
    tcr.ph = 1;
}
/* the globals on this core (no job, or core 1 did not reach them): the same samples */
static void tcr_globals_c0(void)
{
    mix_globals(tcr.out, CTL, tcr.perf, tcr.mg);
    TSG(SG_SENDS);
}
/* core 1 may be inside job j's globals right now: it is alive, it claimed the job, it has neither finished nor refused it, and it has not written gdone. Only then does this core keep off the globals' state */
static uint32_t tcr_c1_in_globals(uint32_t j)
{
    uint32_t a;
    if (!j || !tc.up || TC_LD(&tc_mb.gdone) == j)
        return 0;
    a = tc_answer(j);
    return a == TCA_CLAIMED;
}
static void tcr_out_copy(void)                          /* core 1's block into the DMA buffer */
{
    memcpy(tcr.out, tc_outc1, sizeof tc_outc1);
}
static void tcr_out_zero(void)                          /* this block of the output silent (core 1 may still be in the globals: nothing of theirs is touched) */
{
    memset(tcr.out, 0, sizeof tc_outc1);
}
static void tcr_parts_b(void)
{
    uint32_t p, j = tcr.pj, w, gd;
    if (!tcr.ph) {                                      /* the parts skipped (an older job open): the output still needs the globals of the block before */
        if (tcr.skip) {
            if (tcr_c1_in_globals(tcr.open))
                tcr_out_zero();                         /* core 1 may still be in the old job's globals: silent, nothing of the globals touched (the gather left the arrays alone too) */
            else
                tcr_globals_c0();                       /* core 1 is past them (it writes the parts only now), never claimed, or is stopped: this core runs this block's */
        }
    } else {
#if TC_MEAS
        const uint32_t vm_t3 = TC_TICKS();
#endif
        w = j ? tcr_join(j) : 1u;
        if (j && w == 0u)                               /* THE BALANCE (2026-11-09, COST-CAL / bench B1109G: measured at core 1's END it could never reach 0, since core 1 runs the last part's merge and
                                                         * stages after core 0's set of it whatever the split; so it sat at +TCR_BIAS_LIM and core 0 rendered about 70 % of the voices, its time with two cores
                                                         * nearly its one-core time). Now: the moment core 1's voices were done against core 0's own sets in. Both can be equal; the merges and stages follow on core 1 */
            tcr_bias((int32_t)(TC_LD(&tc_mb.trdy) - tcr.t0r));
        TSG(SG_WAIT);
        gd = j && TC_LD(&tc_mb.gdone) == j;             /* core 1 wrote the globals (phase 1) */
#if TC_MEAS
        if (j) {                                        /* the bench's timers: the control pass, core 0's own sets, the wait (after the sends and the master) */
            vsm.n++;
            VSM_ADD(0, tcr.vm_t[1] - tcr.vm_t[0]);
            VSM_ADD(1, tcr.vm_t[2] - tcr.vm_t[1]);
            VSM_ADD(2, TC_TICKS() - vm_t3);
        } else {
            tcm.own[tcr.own_why]++;
        }
#endif
        if (gd) {                                       /* core 1 wrote the globals: its block into the output */
            tcr_out_copy();
        } else if (w == 1u || !tcr_c1_in_globals(j)) {  /* no job, or core 1 never touched it, refused it, or is stopped: it is not in the globals; this core runs them, so the output goes on with the parts
                                                         * silent, as the one-core path with those blocks silent */
            tcr_globals_c0();
        } else {                                        /* alive, claimed and not past the globals by the wait limit (a hang: tc_watch stops it): this block silent, nothing of the globals touched */
            tcr_out_zero();
        }
        if (w == 1u) {                                  /* core 1's share on this core, in its order: the same samples */
            if (!j)
                tcs.own++;
            for (p = 0; p < NLIVE; p++) {
                const vpass_t *vp = &tc_blk.vp[p];
                if (tc_blk.sel[p]) {
                    tc_set_run(vp, tc_blk.sel[p], &pscr_c1);
                    vs_merge(vp->rc.out, vp->rc.sbuf, vp->rc.spread_on, &pscr_c1);
                }
                part_stages(&tc_blk.m[p], vp);
            }
        }
        if (w == 2u)
            tcr.skip = 1;
        if (w != 2u || !tcr.open)                       /* (core 1 still holds them: tcr_settle closes the block) */
            tcr_close(w == 2u);
    }
    if (tcr.blk < TCR_BLOCKS - 1u)                      /* (the block in the half, for the next block's deadline) */
        tcr.blk++;
}
