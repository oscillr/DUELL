/* SPDX-License-Identifier: GPL-3.0-only */
/* Job 5: HOST-THREAD SIMULATION of the two-core stage 1, written to the UNIT'S protocol (firmware/src/twocore.{h,c}: separate 32-bit words, a sequence number, core 1
 * decides who plays a job) with the real code on both sides. A model of the PROTOCOL, the DATA SEPARATION and the runner's decisions, not of the device's timing.
 *   core 1 (a pthread)        the real tc_c1_step loop (what hal/fm1_core1.h runs in RAM), with the computer hal (tests/twocore_hal_host.h: acquire / release atomics,
 *                             load and store hooks for the fault injection, a simulated clock, a start hook); its job is the real tc_job_run (twocore_run.c: part 2's
 *                             stages into part 2's own pscr / pacc / track), wrapped by sim_job (delays, the bus B check)
 *   core 0 (the main thread)  the firmware's own mix_block (fx.c) with DUELL_TWOCORE on: the real runner (tcr_begin, tcr_parts: the control pass, the deal, tc_post, core 0's own sets; tcr_join: the wait, the
 *                             take, the three cases, the pause), gather, sends, master. A half is 4 blocks of 32 (as audio.c calls it: tc_half_begin before the first);
 *                             the main-loop passes (tc_watch, tc_trim_poll) are called at the start of each half
 * The clock is simulated: it stands still unless a delay is running (then every 4th read of core 0 moves it a tick), so a healthy core 1 is never late because the machine was
 * busy; the harness sets it to the half's start at every half. A delay of core 1 is "until the clock passes T" (T from the runner's deadline of that job), so every late case happens
 * in the block it is aimed at.
 * Each run is one scene (the nine of partindep_scn.h) in a child process with ONE injected fault:
 *   none          the hand-over at full speed                                                       output = the ONE-CORE RUN of the same build (the run 'unarmed': every block through the same runner on core 0)
 *                 (2026-11-05: it was the old combined path of mixsplit_ref.h, which no longer equals the firmware even on one core: 216 failures that said nothing about the two cores; the old path against
 *                 the new order is tests/regress.c's and mixsplit's job)
 *   torn1         core 1 reads a new job with the OLD arg / chk once: re-read, run, counted      same
 *   tornp         ... persistently: refused (BADJOB), a strike, core 0 plays part 2 from then on  same
 *   late_pre      core 1 sleeps before it looks: take, yld, core 0 plays it ("late")              same
 *   late_post     core 1 sleeps after its claim, done within its wait limit ("very late", single blocks: nothing pauses, counted as very late without a pause)   same
 *   voff          many very late blocks, 4 in a row and 8 within 2 s among them, with the BENCHT4 defaults: nothing pauses, all counted as very late without a pause   same
 *   vrun / vwin   (TC_MEAS builds, which switch the BENCHT3 rules on as `bench vrun 4` / `bench vwin 8` do) 4 very late blocks in a row (twice) and 3 in a row (once): the pause
 *                 by a run (the first two); 8 very late blocks, never 2 in a row, within 2 s: the pause by the window; 7 of them later: no pause                      same
 *   noans         core 1 sleeps before it looks until past the 100 us answer time of the take, then yields: "no answer", a fault pause, core 0 plays part 2          same
 *   vlong         core 1 sleeps after its claim until 60 ticks past the WAIT LIMIT: core 0 stops waiting AT the limit (checked in every run: it never leaves the wait after the
 *                 limit nor after the half's end), part 2 silent, a fault pause                                                                                        counters only
 *   floor / floorx / nofloor  (TC_MEAS builds) core 1's job in block 0 of every half takes 2,500 ticks longer than the table's first deadline: the floor raises the deadline
 *                 (very late once, then in time); takes 6,000 more (does not fit): own because of the floor, no job; with the floor off: very late in most halves    same
 *   late_run      a run of late blocks pauses the jobs, they resume                                  same
 *   badchk        the store hook corrupts the check value: refused, a strike                         same
 *   stale         the load hook returns the older number for done: not done, the deadline path      same
 *   ahead         a done word ahead (BADWORD)                                                        same
 *   badsnap       the snapshot core 1 reads does not fit its hash: refused like a wrong check       same
 *   hang_pre      core 1 hangs before it looks at a job: part 2 silent until tc_watch stops it (HANG, 100 ms), then core 0 plays it    = a one-core run that skips exactly those blocks
 *   hang_post     core 1 hangs after its claim: the same, its voices fade out when core 0 takes over  = the same with the fade
 *   park          a flash park in the middle of play: no click, output equal, counted
 *   park_stuck    a flash park with core 1 stuck: FLASH after 20 ms simulated                      = as hang_post
 *   silent_late   core 1 finishes a job after its half: part 2 silent, then core 0 plays it       counters only (the late job renders later state)
 *   unarmed / noanswer   the firmware stays on one core: output = the old combined path; the record says why
 * Self-checks (this binary, not with -nomut): core 1 writing part 1's REVERB send once changes the output; core 1 writing bus B's delay half is seen by the bus B check.
 * host_tests.sh twocore also builds mutated copies of the firmware (twocore_mut) that must fail. ThreadSanitizer: host_tests.sh twocore_tsan (-tsan: the runs without a
 * silent window).
 * THE VOICE SPLIT (owner 2026-11-05, twocore_run.c tcr_parts_a / tcr_parts_b): no part owns a core. Core 0 runs the control pass of BOTH parts, deals every listed voice to the core that is less full, posts ONE job a block
 * (core 1: its sets of both parts into pscr_c1, the merge, then both parts' own stages) and renders its own sets meanwhile. What this file checks changed with it:
 *   the voice audit (the run without a fault, every job): core 1's job writes no voice that was dealt to core 0 (voice_t and the per-slot arrays, both parts); core 0 writes none of core 1's voices between the
 *                 post and the claim; a job with a set does move its voices. Core 1 now writes both parts' buffers, accumulators and effect states by design, so the old "nothing of part 1" audit is gone
 *   a silent block is silent for BOTH parts (tcr.skip). The first one of a run is the FROZEN block: its control pass ran and core 0's own sets rendered, the parts' stages did not; the blocks after it wait
 *                 (nothing of the parts is touched) until the job closes or core 1 is stopped; then the parts' windows close and, if core 1 had claimed, every voice fades (tcr.fade). The one-core reference
 *                 of the hang runs (skip_block) does exactly that, from the block kinds and the dealt sets the run recorded
 *   ahead         a done word ahead stops core 1 inside the block: the block is frozen and closed at once, the voices fade in the next: compared with that reference now (it was "same" on the old runner)
 * THE ONE-BLOCK STEP (owner 2026-11-05, twocore_run.c tcr_parts_a / tcr_parts_b): core 0 gathers the parts' sums of the block BEFORE, runs this block's control pass, deals and posts, renders its own sets,
 * then runs the sends and the master of the block before WHILE core 1 renders this block's voices and the parts' stages, and only then waits. So the output is one block late, and a silent block of the
 * parts is heard in the NEXT block's output. The one-core run has the same order, so the compare of the two is also the proof that the sends and the master read nothing the parts' stages write (on one
 * core they run before the stages, on two beside them). The voice audit's gate holds core 1 back until core 0 is in its wait, so the run 'none' has no such overlap: the fault runs and twocore_tsan have it
 * THE CHORD SCENES (built with -DTC_SIM_CHORD=1, host_tests.sh twocoreshare; the scenes of the old voice share kept for the new runner): instead of the twelve scenes part 1 plays an 8-note chord (a re-strike,
 * a release), three sounds (ANALOG SPREAD 60, ANALOG SPREAD off, PHASE), no MORPH, CH_NBLK blocks each: many voices in every block, so the deal gives both cores voices. Faults: none / torn1 / tornp / badchk /
 * badsnap / stale / late_pre / late_post / noans / park / unarmed / noanswer (output = the one-core path bit for bit), hang_pre / hang_post / park_stuck (the frozen reference), and
 *   p2chord       part 2 plays a chord in blocks 500..699 too (output = the one-core path of the same scene)
 *   freeze        core 1 claims the job in the LAST block of a half and is not done by the wait limit: both parts silent for exactly that block, a fault pause; the output up to and including that block equals
 *                 the frozen reference; no voice of either part is touched between the freeze and the late job
 * Every run, both builds: the job counters add up (sh_posts = sh_done + sh_late + sh_freeze + sh_fault, plus at most the no-answer blocks that ended in a yield), every block is counted once, core 0 leaves
 * its wait no later than the wait limit.
 * NOT covered here: the per-slot arrays of the engines that live in ENG_MEM (FM6 / PHYS / COMB), a note event while a job is open, ThreadSanitizer on the chord scenes.
 * Built and run by DUELL/host_tests.sh (names: twocore, twocoreshare) */
#if defined(TC_SIM_CHORD) && (TC_SIM_CHORD + 0)
#define CHORD_SIM 1                                     /* the chord scenes (see above) */
#else
#define CHORD_SIM 0                                     /* the scenes of partindep_scn.h, as ever */
#endif
#define DUELL_TWOCORE 1
#define TC_AUTO_ARM 0                                   /* (1.0: every TWOCORE build arms itself; the 'unarmed' scenes of this simulation test the explicit arm (tc_arm): tests/twocore_bench_test.c, tcbench, tests the automatic one) */
#define TC_JOB_RUN sim_job
#include <stdint.h>
static void sim_job(uint32_t arg, uint32_t j);
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"                                   /* (scn_eng) */
#include "mixsplit_ref.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include "partindep_scn.h"

#define MAXB 4300u                                      /* blocks of the longest scene (3 s) */
#define BASE_TICKS 24000u                             /* the simulated clock's start */
#define MS_TICKS (TC_TICKS_PER_US * 1000u)

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("twocore: FAIL %s\n", what);
        bad++;
    }
    return ok;
}

enum { F_NONE, F_TORN1, F_TORNP, F_LATE_PRE, F_LATE_POST, F_LATE_RUN, F_BADCHK, F_STALE, F_AHEAD, F_BADSNAP, F_HANG_PRE, F_HANG_POST, F_PARK, F_PARK_STUCK, F_SILENT_LATE,
       F_UNARMED, F_NOANSWER, F_SHARED, F_BUSB, F_TCMCLR, F_VRUN, F_VWIN, F_FLOOR, F_FLOORX, F_NOFLOOR, F_VOFF, F_NOANS, F_VLONG, F_FREEZE, F_P2, F_N };   /* (the last two: chord scenes only) */
#define F_TRIMSTOP 100                                  /* (the fade run that stops core 1: not one of the scene runs) */
static const char *const FNAME[F_N] = {"none", "torn1", "tornp", "late_pre", "late_post", "late_run", "badchk", "stale", "ahead", "badsnap", "hang_pre", "hang_post", "park",
                                       "park_stuck", "silent_late", "unarmed", "noanswer", "shared", "busb", "tcmclr", "vrun", "vwin", "floor", "floorx", "nofloor", "voff", "noans", "vlong", "freeze", "p2chord"};
/* the faults a chord run goes through (CHORD_SIM) */
static int sh_listed(int f)
{
    return f == F_NONE || f == F_TORN1 || f == F_TORNP || f == F_LATE_PRE || f == F_LATE_POST || f == F_BADCHK || f == F_STALE || f == F_BADSNAP || f == F_HANG_PRE || f == F_HANG_POST || f == F_PARK ||
           f == F_PARK_STUCK || f == F_UNARMED || f == F_NOANSWER || f == F_SHARED || f == F_BUSB || f == F_NOANS || f == F_FREEZE || f == F_P2;
}
#define SH_J(x) ((x) + (CHORD_SIM ? 250u : 0u))         /* the hang's job number: later in a chord run, so the held chord has settled to its sustain when the block freezes */

/* what a run reports (shared memory: the runs are child processes) */
typedef struct {
    tc_stat_t s;
    uint32_t parks, up, failed, strike, nv_last, reread, bad_w, misread, c1_jobs, busb_c1, busb_checks, nblk, park_us, park_ok;
    uint32_t cap_viol;                                  /* posted jobs whose deadline was later than core 0's own latest moment (TC_MEAS: the floor's ceiling) */
    uint32_t lim_over, end_over, lim_blocks, lim_max_over;   /* blocks with a posted job where core 0 left its block later than the wait limit (+8 ticks) / later than the half's end; blocks looked at; the largest overrun (ticks) */
    uint32_t own_checks, own_writes, own_timeouts;     /* the voice audit (sim_job): jobs audited, jobs that changed a voice dealt to core 0 (or not listed), gate timeouts */
    uint32_t look_chk, look_writes;                     /* jobs whose sets were hashed at core 1's first look and at its claim / of those, the ones core 0 wrote in between */
    uint32_t set_jobs, set_moved, both_jobs;            /* audited jobs that had a voice for core 1 / of those, the ones that changed their voices / audited jobs in which both cores had a voice */
    uint32_t fz_ok, fz_blk, fz_chk, fz_diff;            /* freeze: the first frozen block seen, its number; the late job's check that no voice of the parts was touched since the freeze / the times one was */
    uint64_t hash;
    uint64_t bh[MAXB];                                  /* the hash of each block's output */
    uint8_t skip[MAXB];                                 /* the parts were silent in this block (tcr.skip): K_FROZEN the job's own block, K_WAIT a block waiting for the open job; 0 played */
    uint16_t sel[MAXB][NLIVE];                          /* the ranks of each part's render list dealt to core 1 in this block (tc_blk.sel) */
    uint64_t dl_sum[TCR_BLOCKS];                        /* the deadlines of the posted jobs, ticks after the half's start, summed per block of the half */
    uint32_t dl_n[TCR_BLOCKS];
    uint32_t nott;                                      /* posted jobs of blocks where core 0 could no longer take part 2 back in time (item 90, tcr.nott: the job is posted with the wait limit as its deadline, no take-over) */
    uint64_t lim_sum[TCR_BLOCKS];                       /* the wait limits of the posted jobs, ticks after the half's start, summed per block of the half */
#if TC_MEAS
    tcm_t m;                                            /* BENCHT2's timing figures at the end (tcm: twocore_run.c) */
#endif
} res_t;
static res_t *R;                                        /* the child writes here */

static int fault;                                       /* the run's injected fault (the child's) */
static uint32_t nblk;                                   /* blocks so far */
static uint32_t clk_jump;                               /* ticks added to the half starts (the calm periods skipped) */
static uint32_t jumps;                                  /* pauses already followed by a jump */
static int parked;
static volatile uint32_t quit, c1_ready;
static pthread_t thr;
static int thr_on;

static inline uint32_t raw_ticks(void) { return __atomic_load_n(&tc_host_ticks, __ATOMIC_RELAXED); }
static inline int c1_live(void) { return __atomic_load_n(&tc_host_live, __ATOMIC_ACQUIRE) && !__atomic_load_n(&quit, __ATOMIC_ACQUIRE); }
static void cpu_relax(void)
{
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#else
    sched_yield();
#endif
}
/* The clock stands still unless someone wants time to pass: a delay core 1 was told to take, a hung core 1, a stale word being held back. Then core 0's wait loops move
 * it (every divsel-th read). So a healthy core 1 is never "late" because the machine was busy, and every late case is the one aimed at. */
static pthread_mutex_t tmx = PTHREAD_MUTEX_INITIALIZER;
static int tusers;
static uint32_t divsel = 4u;
static void time_adj(int d)
{
    pthread_mutex_lock(&tmx);
    tusers += d;
    __atomic_store_n(&tc_host_tick_div, tusers > 0 ? divsel : 0u, __ATOMIC_RELAXED);
    pthread_mutex_unlock(&tmx);
}
/* core 1's delay: until the simulated clock reaches t (core 0's wait loops move it; it does not pass t until core 1 is awake, so the case aimed at is the one that
 * happens) or core 1 is held / the run is over */
static void wait_clock(uint32_t t)
{
    __atomic_store_n(&tc_host_stop, t, __ATOMIC_RELEASE);
    time_adj(1);
    while (c1_live() && (int32_t)(raw_ticks() - t) < 0)
        cpu_relax();
    time_adj(-1);                                       /* (time stands still first, then the barrier goes: no tick slips by) */
    __atomic_store_n(&tc_host_stop, 0u, __ATOMIC_RELEASE);
}
static volatile uint32_t release_silent;                /* the harness lets the silent_late core 1 go on at the next half */
static void wait_release(void)
{
    time_adj(1);
    while (c1_live() && !__atomic_load_n(&release_silent, __ATOMIC_ACQUIRE))
        cpu_relax();
    time_adj(-1);
}
static void hang(void)                                  /* hung: until core 0 stops it (or the run ends) */
{
    time_adj(1);
    while (c1_live())
        cpu_relax();
    time_adj(-1);
}

/* ------------------------------------------------------------ the fault plan: which job gets what --- */
static __thread uint32_t c1_job, c1_seen, torn_left, prev_arg, prev_chk;   /* core 1's: the job it is looking at, the torn reads left */
static uint32_t trim_job, ahead_left;
static __thread uint32_t c1_dealt;                      /* (core 1's: the last job whose deal word it saw, so the delays of the deal word run once a job) */
static volatile uint32_t c0_wait;                       /* the ownership audit's gate: the job core 0 is waiting for (set when it loads the done word) */
static int stale_on;                  /* (the fade runs name their own job) */
static int pl_torn1(uint32_t j) { return fault == F_TORN1 && j % 7u == 3u; }
static int pl_tornp(uint32_t j) { return fault == F_TORNP && j == 150u; }
static int pl_late_pre(uint32_t j)
{
    return (fault == F_LATE_PRE && j % 31u == 2u) || (fault == F_LATE_RUN && ((j >= 150u && j < 154u) || (j >= 500u && j < 505u))) || (fault == F_AHEAD && j == 100u);
}
static int pl_late_post(uint32_t j)
{
    return (fault == F_LATE_POST && j % 401u == 5u) ||
           (fault == F_VOFF && (((j >= 150u && j < 154u) || (j >= 300u && j < 304u) || (j >= 450u && j < 453u)) || (j >= 700u && j < 716u && !(j & 1u)) || j % 401u == 5u)) ||
           (fault == F_VRUN && ((j >= 150u && j < 154u) || (j >= 300u && j < 304u) || (j >= 450u && j < 453u))) ||
           (fault == F_VWIN && ((j >= 150u && j < 166u && !(j & 1u)) || (j >= 700u && j < 714u && !(j & 1u))));
}
static volatile uint32_t sim_blk;                       /* the block in the half core 0 is in (set before mix_block; core 0 does not leave the block before core 1's job is done or taken back) */
static uint32_t fl_d;                                   /* the floor runs: how long core 1's slow job takes, ticks (set by the first slow job) */
static int is_floor(void) { return fault == F_FLOOR || fault == F_FLOORX || fault == F_NOFLOOR; }
static int pl_noans(uint32_t j) { return (fault == F_NOANS && j % 53u == 9u) || (fault == F_NONE && trim_job && j == trim_job); }
static int pl_vlong(uint32_t j) { return fault == F_VLONG && j % 61u == 7u; }
static int pl_stale(uint32_t j) { return fault == F_STALE && j % 29u == 4u; }
static int pl_hang_pre(uint32_t j) { return fault == F_HANG_PRE && j == SH_J(150u); }
static int pl_hang_post(uint32_t j) { return (fault == F_HANG_POST || fault == F_PARK_STUCK) && j == SH_J(150u); }
static int pl_silent(uint32_t j) { return fault == F_SILENT_LATE && j == 150u; }
/* freeze: the first job from number 400 on that core 0 posts in the LAST block of a half is claimed and then slow (like vlong: past the wait limit), so the parts are silent for exactly that block: the late job is
 * let go at the next half's start (sim_block), before the next block settles it. Planned once per run (the job's own thread sets fz_planned) */
static int fz_planned;
static int pl_freeze(uint32_t j)
{
    if (fault != F_FREEZE || fz_planned || j < 400u || __atomic_load_n(&sim_blk, __ATOMIC_ACQUIRE) != TCR_BLOCKS - 1u)
        return 0;
    fz_planned = 1;
    return 1;
}

/* ------------------------------------------------------------ the voice audit: whose voices are whose --- */
static uint64_t hbytes(uint64_t h, const void *p, size_t len)
{
    const uint8_t *q = (const uint8_t *)p;
    size_t i;
    for (i = 0; i < len; i++)
        h = (h ^ q[i]) * 0x100000001B3ull;
    return h;
}
/* the state of the voices of part p named by the slot mask (bit i = slot i): the voice_t and its per-slot arrays that the render writes (SUB, the TWIN filter's state, WHEEL's partials). Not in it: the slots in ENG_MEM
 * (FM6, PHYS, COMB) and the engines' other globals: an engine that keeps state there is audited by the audio only. The padding of the structs is hashed too: fine, nobody writes it between two hashes of one run */
static uint64_t vh_part(uint64_t h, uint32_t p, uint32_t mask)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if ((mask >> i) & 1u) {
            h = hbytes(h, &trk[p].v[i], sizeof trk[p].v[i]);
            h = hbytes(h, &sub_ph[p][i], sizeof sub_ph[p][i]);
            h = hbytes(h, &sub_g[p][i], sizeof sub_g[p][i]);
            h = hbytes(h, &sub_cfg[p][i], sizeof sub_cfg[p][i]);
            h = hbytes(h, &f2s[p][i], sizeof f2s[p][i]);
            h = hbytes(h, &drw_v[p][i], sizeof drw_v[p][i]);
        }
    return h;
}
#define ALL_SLOTS ((1u << NVOICE) - 1u)
/* the block's record (twocore_run.c tc_blk; core 0 writes it before the post): the slots of part p dealt to core 1 (tc_blk.sel names RANKS of the render list), and every slot the control pass listed */
static uint32_t c1_slots(uint32_t p)
{
    const vpass_t *vp = &tc_blk.vp[p];
    uint32_t r, m = 0;
    for (r = 0; r < vp->nl && r < NVOICE; r++)
        if ((tc_blk.sel[p] >> r) & 1u)
            m |= 1u << vp->rl[r];
    return m;
}
static uint32_t listed_slots(uint32_t p)
{
    const vpass_t *vp = &tc_blk.vp[p];
    uint32_t r, m = 0;
    for (r = 0; r < vp->nl && r < NVOICE; r++)
        m |= 1u << vp->rl[r];
    return m;
}
/* core 1's voices (the masks m, a part each), and every other voice of the parts (core 0's own sets and the voices that are not listed) */
static uint64_t set_hash(const uint32_t *m)
{
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t p;
    for (p = 0; p < NLIVE; p++)
        h = vh_part(h, p, m[p]);
    return h;
}
static uint64_t rest_hash(const uint32_t *m)
{
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t p;
    for (p = 0; p < NLIVE; p++)
        h = vh_part(h, p, ~m[p] & ALL_SLOTS);
    return h;
}
static uint64_t parts_hash(void)                        /* every voice of the parts */
{
    static const uint32_t none[NLIVE];
    return rest_hash(none);
}
static uint64_t look_h;                                 /* THE TWO-PHASE JOB (2026-11-07): core 1's sets as they stood when core 0 wrote the deal word (st_hook, core 0's thread; the sets are final then), and which slots
                                                         * those were; read by core 1 behind the gate (ordered by c0_wait) */
static uint32_t look_m[NLIVE];
static uint32_t look_chk, look_writes, set_jobs, set_moved, both_jobs;
static uint32_t fz_ok, fz_blk, fz_chk, fz_diff, fz_seen;   /* the first frozen block (core 0 notes it after the block); the late job's check */
static volatile uint64_t fz_h0;
static volatile uint32_t fz_ready;
static volatile uint32_t snap_bad;                      /* badsnap: the snapshot's bit is flipped (core 1 sets it at its first look, core 0 puts the bit back when it reads the refusal) */

/* every load of a shared word, core 1's and core 0's (the hal's hook) */
static uint32_t ld_hook(const uint32_t *p, uint32_t v)
{
    if (tc_host_is_c1) {
        if (p == &tc_mb.job && v != c1_seen) {          /* core 1 looks at a new job: what happens first (delays before it looks, a hang, torn reads, a misread snapshot) */
            c1_seen = c1_job = v;
            if (pl_noans(v) || pl_late_pre(v)) {        /* (the two-phase job, 2026-11-07: the job is posted before the control pass, so this block's deadline (tcr.dl) exists only once core 0 has written the deal word: wait for it first) */
                while (c1_live() && __atomic_load_n(&tc_mb.deal, __ATOMIC_ACQUIRE) != v)
                    cpu_relax();
            }
            if (pl_noans(v))
                wait_clock(tcr.dl + 2600u);             /* later than the 100 us (2,400 ticks) core 0 waits for the answer to its take, earlier than the wait limit: "no answer", then yld */
            else if (pl_late_pre(v))
                wait_clock(tcr.dl + 1000u);             /* late to wake: core 0 takes the job back before core 1 looks */
            if (pl_hang_pre(v))
                hang();                                 /* hung before it looks: it never answers (held at the end: its stores are dropped) */
            torn_left = pl_torn1(v) ? 1u : pl_tornp(v) ? 100u : 0u;
            if (fault == F_BADSNAP && v == 100u) {
                *(volatile uint32_t *)&tc_ctx ^= 0x10u;   /* the snapshot as core 1 reads it: one bit wrong */
                __atomic_store_n(&snap_bad, 1u, __ATOMIC_RELEASE);
            }
            return v;
        }
        if (p == &tc_mb.deal && v == c1_job && v != c1_dealt) {   /* THE TWO-PHASE JOB (2026-11-07): core 1 has run the globals (gdone) and sees the deal word: a core 1 that is slow in its sets and stages is slow from here
                                                         * (before, these delays sat at the start of the job; the globals of the block are done, as the one-core reference has them, and tcr.dl / tcr.lim are this block's) */
            const uint32_t j = v;
            const int frz = pl_freeze(j);
            c1_dealt = v;
            if (pl_vlong(j) || frz)
                wait_clock(tcr.lim + 60u);              /* slower than core 0 may wait: it stops at the wait limit, the parts are silent for the block */
            else if (pl_late_post(j))
                wait_clock(tcr.dl + 1000u);             /* slow in the middle: core 0 takes it back, then waits for done */
            if (is_floor() && __atomic_load_n(&sim_blk, __ATOMIC_ACQUIRE) == 0u) {   /* the floor runs: the job of a half's block 0 takes a fixed time, 2,500 ticks past the table's first deadline (floorx: 6,000 more) */
                uint32_t now1 = raw_ticks();
                if (!fl_d)
                    fl_d = (uint32_t)(tcr.dl - now1) + 2500u + (fault == F_FLOORX ? 6000u : 0u);
                {
                    uint32_t t_end = now1 + fl_d, lim_ok = tcr.lim - 100u;
                    wait_clock((int32_t)(t_end - lim_ok) > 0 ? lim_ok : t_end);
                }
            }
            if (pl_silent(j))
                wait_release();                         /* not done in the half: core 0 gives the parts up for the block; it goes on at the next half */
            if (pl_hang_post(j)) {
                hang();                                 /* hung after its claim (and after the globals): nothing of the parts is touched */
                pthread_exit(0);                        /* (2026-11-07: stopped by core 0 = dead, as on the unit: this thread must not go on into the job and write its voices; before the two-phase job the hang sat where nothing was left to write) */
            }
            if (frz && __atomic_load_n(&fz_ready, __ATOMIC_ACQUIRE)) {   /* the freeze's check: no voice of the parts has been touched since core 0 gave the block up (it hashed them after the block) */
                fz_chk++;
                if (parts_hash() != fz_h0)
                    fz_diff++;
            }
            return v;
        }
        if (torn_left && p == &tc_mb.arg)
            return prev_arg;                            /* the old job's words */
        if (torn_left && p == &tc_mb.chk) {
            torn_left--;
            return prev_chk;
        }
        return v;
    }
    if (p == &tc_mb.bad_job && v == tc.job && __atomic_load_n(&snap_bad, __ATOMIC_ACQUIRE)) {   /* badsnap: core 1 has refused the job (its hash is over: the refusal is written after it). The fault is a
                                                         * MISREAD of core 1's, the memory itself is right: the bit goes back before core 0 plays the block with that snapshot (the parts' stages read tc_ctx) */
        *(volatile uint32_t *)&tc_ctx ^= 0x10u;
        __atomic_store_n(&snap_bad, 0u, __ATOMIC_RELEASE);
    }
    if (p == &tc_mb.done) {
        if (fault == F_NONE)
            __atomic_store_n(&c0_wait, tc.job, __ATOMIC_RELEASE);   /* core 0 is in its wait for this job (tcr_join): its own sets are rendered, it touches no part state until done */
        int hide = pl_stale(tc.job) && v == tc.job && (int32_t)(raw_ticks() - (tcr.dl + 500u)) < 0;
        if (hide != stale_on) {                         /* (time passes while the word is held back) */
            stale_on = hide;
            time_adj(hide ? 1 : -1);
        }
        if (hide)
            return v - 1u;                              /* the older number: not done yet */
        if (fault == F_AHEAD && tc.job == 100u && ahead_left) {
            ahead_left--;
            return v + 5u;                              /* a word ahead of the last job posted */
        }
    }
    return v;
}
/* every store of a shared word */
static uint32_t st_hook(uint32_t *p, uint32_t v)
{
    if (!tc_host_is_c1 && p == &tc_mb.chk && ((fault == F_BADCHK && tc.job + 1u == 200u) || (fault == F_TRIMSTOP && tc.job + 1u == trim_job)))
        return v ^ 0x40u;                               /* core 0's check value written wrong */
    if (!tc_host_is_c1 && p == &tc_mb.deal && v == tc.job && fault == F_NONE) {   /* the voice audit, first half: core 1's voices as they are the moment core 0 hands the sets over (the deal word) */
        uint32_t q;
        for (q = 0; q < NLIVE; q++)
            look_m[q] = c1_slots(q);
        look_h = set_hash(look_m);
    }
    return v;
}

/* ------------------------------------------------------------ bus B: nothing of it may be written by core 1 in stage 1 --- */
static uint64_t busb_sum(void)
{
    uint64_t h = 0xCBF29CE484222325ull;
    const uint32_t *w;
    uint32_t i;
    w = (const uint32_t *)&fx_b;
    for (i = 0; i < sizeof fx_b / 4u; i++)
        h = (h ^ w[i]) * 0x100000001B3ull;
    w = (const uint32_t *)&hl_b;
    for (i = 0; i < sizeof hl_b / 4u; i++)
        h = (h ^ w[i]) * 0x100000001B3ull;
    for (i = 0; i < CTL; i++)
        h = (h ^ (uint32_t)(sa_c[i] + 3 * sa_d[i] + 5 * sa_r[i] + 7 * sp_fc_a[i] + 11 * sp_fd_a[i] + 13 * sp_fr_a[i])) * 0x100000001B3ull;
    w = (const uint32_t *)(dly_buf + DLY_HALF);          /* the delay's half B (BUS_DLY of bus B: fx.c) */
    for (i = 0; i < DLY_HALF / 2u; i++)
        h = (h ^ w[i]) * 0x100000001B3ull;
    return h;
}


/* ------------------------------------------------------------ the voice audit's gate --- */
/* A write into a voice of the other core is a race in the threaded run: it shows only if both cores are at that voice at the same moment. This makes it show every time: in the run without a fault core 1 waits
 * until core 0 is in its wait for done (its own sets are rendered and it touches no part state until it reads done), hashes every voice that is NOT its own (rest_hash) and its own (set_hash), runs the real
 * job, and hashes again. The job may write its own voices only (and the parts' buffers, accumulators and effect states, which are not in the hashes). No timing, no race. */
static uint32_t own_checks, own_writes, own_timeouts;
static int own_gate(uint32_t j)                         /* wait for core 0 to be in its wait for job j */
{
    uint64_t n = 0;
    while (c1_live() && __atomic_load_n(&c0_wait, __ATOMIC_ACQUIRE) != j) {
        cpu_relax();
        if (++n > 400000000ull)
            return 0;
    }
    return 1;
}

/* ------------------------------------------------------------ core 1 --- */
/* the job as core 1 runs it: delays, the bus B check around the real job */
static uint32_t c1_jobs, c1_busb, c1_checks;
static void sim_job(uint32_t arg, uint32_t j)
{
    uint64_t s0, s1;
    /* (2026-11-07: the delays of a slow core 1 and the freeze's check sit at its read of the deal word now: ld_hook) */
    if (!c1_live())
        return;
    s0 = fault == F_BUSB ? busb_sum() : 0u;             /* (the one-block step: core 0 runs the sends beside this job, and bus B is its to write: the sum is taken behind the gate, below, where core 0 stands still) */
    if (fault == F_NONE) {
        uint64_t a0 = 0, v0 = 0;
        uint32_t cm[NLIVE], p, any = 0, c0any = 0;
        int gated = own_gate(j);
        s0 = busb_sum();                                /* (core 0 is in its wait; the globals are this job's now, so the sum changes in every job that has sends: counted, not a fault) */
        if (gated) {                                    /* the voice audit, second half: the sets as they stand now equal the sets at the deal word: core 0 wrote none of those voices between the deal and its wait */
            look_chk++;
            if (set_hash(look_m) != look_h)
                look_writes++;
        }
        for (p = 0; p < NLIVE; p++) {                   /* (core 0 is in its wait for done: the record is stable) */
            cm[p] = c1_slots(p);
            any |= cm[p];
            c0any |= listed_slots(p) & ~cm[p];
        }
        if (gated) {
            a0 = rest_hash(cm);
            v0 = set_hash(cm);
        } else
            own_timeouts++;
        tc_job_run(arg, j);
        if (gated) {
            own_checks++;
            if (rest_hash(cm) != a0)
                own_writes++;
            if (any) {
                set_jobs++;
                if (set_hash(cm) != v0)
                    set_moved++;                        /* the job did write its own voices (a job that moved nothing would be a hollow audit) */
            }
            both_jobs += any && c0any;
        }
    } else
        tc_job_run(arg, j);
    if (fault == F_SHARED && c1_jobs == 300u)
        trk[0].p[P_REV] = (int16_t)(trk[0].p[P_REV] + 9);   /* the deliberate stray write: core 1 changes a part's parameter (part 1's REVERB send) after its job */
    if (fault == F_BUSB && c1_jobs == 300u)
        dly_buf[DLY_HALF + 3u] = (int16_t)(dly_buf[DLY_HALF + 3u] + 1);   /* a write into bus B's half of the delay */
    s1 = fault == F_NONE || fault == F_BUSB ? busb_sum() : 0u;
    c1_checks++;
    if (s0 != s1)
        c1_busb++;
    c1_jobs++;
}
static void *core1(void *arg)
{
    tc_c1_t c;
    (void)arg;
    tc_host_is_c1 = 1;
    tc_c1_init(&c);
    __atomic_store_n(&c1_ready, 1u, __ATOMIC_RELEASE);
    while (c1_live()) {
        uint32_t before = c.last;
        tc_c1_step(&c);
        if (c.last != before) {                         /* (the words of the job it just answered: the "old" ones of the next job's torn read) */
            prev_arg = __atomic_load_n(&tc_mb.arg, __ATOMIC_RELAXED);
            prev_chk = __atomic_load_n(&tc_mb.chk, __ATOMIC_RELAXED);
        }
        cpu_relax();
    }
    return 0;
}
static void start_core1(void)
{
    thr_on = 1;
    pthread_create(&thr, 0, core1, 0);
}

/* ------------------------------------------------------------ core 0: the blocks --- */
static uint64_t blk_hash(const int32_t *o, uint32_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t i;
    for (i = 0; i < n; i++)
        h = (h ^ (uint32_t)o[i]) * 0x100000001B3ull;
    return h;
}
static void record(const int32_t *out, uint32_t skip)
{
    if (nblk < MAXB) {
        R->bh[nblk] = blk_hash(out, 2u * CTL);
        R->skip[nblk] = (uint8_t)skip;
    }
    nblk++;
}

/* the one-core path with the parts silent in the blocks of ref_skip, as the runner leaves them (twocore_run.c tcr_parts_a / tcr_parts_b / tcr_join / tcr_settle / tcr_close):
 *   K_FROZEN  the block whose job core 1 did not finish: the control pass of both parts ran and core 0's own sets rendered (the ranks NOT in ref_sel), core 1's sets and the parts' stages did not; the parts'
 *             windows (MORPH's, the matrix's) stay open
 *   K_WAIT    a block waiting for that job: nothing of the parts is touched
 *   (THE ONE-BLOCK STEP: every block first gathers the parts of the block BEFORE, silent when that one was frozen or waiting, and runs the sends and the master on them between this block's sets and
 *   its stages: the firmware's order, see the header)
 *   the next played block first closes the windows as tcr_close(1) does (ref_close_same: in the frozen block itself, a core 1 stopped inside the block), and at block ref_fade every voice that is not
 *   fading already is killed (a core 1 that had claimed: tcr.fade)
 * The gather of a silent block runs with tcr.skip set, as in the run: it reads nothing of the parts and does not run SPREAD's post step (fx.c spread_post), whose state (sp_ok, the gains of the block
 * before) therefore stays as the last played block left it. (A reference that only cleared pacc_on ran that step and lost the gains' ramp of the first block after the silence: 9 failures, 2026-11-05) */
enum { K_PLAY, K_FROZEN, K_WAIT };
static const uint8_t *ref_skip;
static const uint16_t (*ref_sel)[NLIVE];
static uint32_t ref_fade, ref_close_same;
static void ref_close_x(int *mo, int dead)             /* (twocore_run.c tcr_close) */
{
    uint32_t p;
    for (p = 0; p < NLIVE; p++) {
        if (dead) {
            if (MODW(&trk[p]).on)
                mod_end(&trk[p]);
            pacc_on[p] = 0;
        }
        if (mo[p])
            morph_end(&trk[p]);
        mo[p] = 0;
    }
}
static void ref_close(int *mo) { ref_close_x(mo, 1); }
static void skip_block(int32_t *out, uint32_t n)
{
    static mixpart_t m[NLIVE];
    static vpass_t vp[NLIVE];
    static blockstate_t pbs;                            /* the snapshot of the last block whose parts ran (the runner's tc_ctx.bs): the next gather's */
    static int mo[NLIVE], held;                         /* (held: the parts' windows are open since the frozen block) */
    static uint32_t pkind;                              /* the block before's kind: its parts are what this block gathers */
    uint32_t i, k, b = nblk, nrec = 0;
    const uint32_t kind = b < MAXB ? ref_skip[b] : K_PLAY;
    int perf;
    blockstate_t bs;
    int32_t mg = MIX_MG();
    mix_events(n);
    if (kind != K_WAIT && held) {                       /* (tcr_begin, tcr_settle: the old job is over; before the bus-offset copy, as the runner has it) */
        ref_close(mo);
        held = 0;
    }
    if (!held)                                          /* (2026-11-07: the send stage's copy of the matrix's bus offsets, fx.c mix_block: not while a job is open) */
        mod_bus_snap();
    sp_tap = snd_st != SND_OFF && (perf_space + mod_space_off() > 0 || sv.spc > 0);
    if (perf_idle()) {
        perf_tick(n);
        perf = 0;
    } else {
        perf = perf_begin(n);
    }
    blk_snap(&bs, sp_tap);
    tcr.skip = pkind != K_PLAY;                         /* (this child does not run the runner: the word is this function's alone) */
    mix_gather(n, &pbs);                                /* the parts of the block before */
    tcr.skip = 0;
    if (kind != K_WAIT) {                               /* tcr_parts_a */
        if (b == ref_fade)
            for (i = 0; i < NLIVE; i++)
                for (k = 0; k < NVOICE; k++)
                    if (trk[i].v[k].active && trk[i].v[k].stage != 4u)
                        voice_kill(&trk[i].v[k]);
        pbs = bs;
        for (i = 0; i < NLIVE; i++) {
            mo[i] = morph_begin(&trk[i], 1) != 0;
            part_ctl(&m[i], &vp[i], &nrec, &trk[i], n, &pbs);
        }
        if (kind == K_PLAY)
            for (i = 0; i < NLIVE; i++)
                track_voices(&vp[i]);
        else
            for (i = 0; i < NLIVE; i++)                 /* core 0's own sets; the rest of the block never happens */
                vs_render_set(&vp[i].rc, vp[i].rl, vp[i].nl, ~(uint32_t)ref_sel[b][i] & ((1u << vp[i].nl) - 1u), vr_m + vp[i].r0);
    }
    mix_sends(n);                                       /* the globals of the block before */
    if (meter_on) {
        meter_on--;
        if (dm_split)
            meter_scan(n);
    }
    if (scope_mix)
        scope_take_mix(n);
    mix_master(out, n, perf, mg);
    if (kind == K_PLAY) {                               /* tcr_parts_b */
        for (i = 0; i < NLIVE; i++)
            part_stages(&m[i], &vp[i]);
        ref_close_x(mo, 0);                             /* (tcr_close(0): the MORPH windows; the stages closed the matrix's and wrote the sums) */
    } else if (kind == K_FROZEN) {
        held = 1;
        if (ref_close_same) {
            ref_close(mo);
            held = 0;
        }
    }
    pkind = kind;
    record(out, kind);
}

/* the main loop's passes and the audio interrupt's half start, then the firmware's own mix_block */
static uint32_t park_t0, park_t1, beat_seen, beat_ms;
static void sim_block(int32_t *out, uint32_t n)
{
    uint32_t b = nblk, sp0, w0, p;
    if (b % TCR_BLOCKS == 0) {
        uint32_t h = b / TCR_BLOCKS, t = BASE_TICKS + h * TCR_HALF_TICKS + clk_jump;
        if ((fault == F_VLONG || fault == F_FREEZE) && tcr.open) {   /* (owner rule G21: a late job pauses nothing; it is simply still open) */   /* the late job is let go before the calm period passes (its clock figures stay those of one job) */
            uint32_t want = tcr.lim + 100u;
            if ((int32_t)(want - raw_ticks()) > 0)
                __atomic_store_n(&tc_host_ticks, want, __ATOMIC_RELAXED);
            while (c1_live() && !tc_closed())
                cpu_relax();
        }
        if ((fault == F_LATE_POST || fault == F_LATE_RUN || fault == F_LATE_PRE || fault == F_STALE || fault == F_VRUN || fault == F_VWIN || fault == F_NOANS || fault == F_VLONG || fault == F_FREEZE) && tcs.pauses > jumps) {
            jumps = tcs.pauses;
            clk_jump += TCR_CALM_TICKS;                 /* the calm period passes */
            t += TCR_CALM_TICKS;
        }
        if ((int32_t)(t - raw_ticks()) > 0)
            __atomic_store_n(&tc_host_ticks, t, __ATOMIC_RELAXED);
        if (fault == F_SILENT_LATE && thr_on && tcs.silent) {   /* the late core 1 goes on now, and the next half starts with it idle */
            __atomic_store_n(&release_silent, 1u, __ATOMIC_RELEASE);
            while (c1_live() && !tc_closed())
                cpu_relax();
        }
        if (thr_on && tc.up && fault != F_HANG_PRE && fault != F_HANG_POST && fault != F_PARK_STUCK) {   /* the heartbeat watch reads this clock as real time: a healthy core 1 has beaten by now */
            uint32_t ms = raw_ticks() / MS_TICKS, bt = __atomic_load_n(&tc_mb.beat, __ATOMIC_RELAXED);
            if (bt != beat_seen) {
                beat_seen = bt;
                beat_ms = ms;
            } else if (ms - beat_ms > 50u) {
                while (c1_live() && __atomic_load_n(&tc_mb.beat, __ATOMIC_RELAXED) == beat_seen)
                    cpu_relax();
                beat_seen = __atomic_load_n(&tc_mb.beat, __ATOMIC_RELAXED);
                beat_ms = ms;
            }
        }
        tc_watch(raw_ticks() / MS_TICKS);               /* a main-loop pass: the safety net; a flash park of the pass before ends */
        tc_trim_poll();
        tc_half_begin(raw_ticks());
#if TC_MEAS
        if (fault == F_TCMCLR && h == 40u)
            tcm.clr = 1;                                /* the main loop asks for a clear (bench run does): the audio interrupt takes it at the next block */
#endif
        if (fault == F_PARK && h == 60u)
            tc_flash_park();                            /* the flash driver: this half plays everything on core 0 */
        if (fault == F_PARK_STUCK && !parked && tcs.silent) {
            parked = 1;
            divsel = 1u;                                /* (20 ms of ticks in single steps) */
            time_adj(0);
            park_t0 = raw_ticks();
            tc_flash_park();
            park_t1 = raw_ticks();
        }
    }
    __atomic_store_n(&sim_blk, b % TCR_BLOCKS, __ATOMIC_RELEASE);
    sp0 = tcs.sh_posts;
    w0 = tcs.wait;
    mix_block(out, n);
    if (fault == F_FREEZE && tcs.sh_freeze != fz_seen) {   /* the parts froze in this block (core 1 is held in its delay until the next half's start): note the first one and hash every voice, for the late job's check (nobody may touch them from here on) */
        fz_seen = tcs.sh_freeze;
        if (!fz_ok) {
            fz_ok = 1;
            fz_blk = b;
        }
        fz_h0 = parts_hash();
        __atomic_store_n(&fz_ready, 1u, __ATOMIC_RELEASE);
    }
    if (tcs.sh_posts != sp0) {                          /* (the wait clears tcr.job: the post counter says there was a job) */
        int32_t lo = (int32_t)(raw_ticks() - tcr.lim), eo = (int32_t)(raw_ticks() - (tcr.t0 + TCR_HALF_TICKS));   /* where core 0 is when it leaves the block's wait: never later than the limit, so never later than the half's end */
        R->lim_blocks++;
        R->lim_over += lo > 8;
        R->end_over += eo > 0;
        if (lo > (int32_t)R->lim_max_over)
            R->lim_max_over = (uint32_t)lo;
#if TC_MEAS
        R->cap_viol += (int32_t)(tcr.dl - tcr.cap) > 0;
#endif
        R->nott += tcr.nott != 0u;
        R->dl_sum[b % TCR_BLOCKS] += tcr.dl - tcr.t0;
        R->dl_n[b % TCR_BLOCKS]++;
        R->lim_sum[b % TCR_BLOCKS] += tcr.lim - tcr.t0;
    }
    if (b < MAXB)
        for (p = 0; p < NLIVE; p++)
            R->sel[b][p] = (uint16_t)tc_blk.sel[p];
    record(out, tcr.skip ? (tcs.wait != w0 ? K_WAIT : K_FROZEN) : K_PLAY);
}

/* ------------------------------------------------------------ a run --- */
/* ------------------------------------------------------------ the chord scenes (CHORD_SIM) --- */
/* Scene 4 of partindep_scn.h without its MORPH and its timeline: LAYER, part 1 alone (part 2 silent). The sound is set in block 0 (ANALOG with SPREAD 60 / ANALOG with SPREAD off / PHASE with SPREAD 60), the DEC
 * at its fastest so the held chord is at its sustain long before a fault freezes the set. Block 2: an 8-note chord (one voice a block starts), 60 / 61: a note off and the same note on again (the voice_step
 * re-strike path), 800: the release. With sh_ev, part 2 plays a 3-note chord in blocks 500..699. Eight voices in every block for a long time: the deal has voices for both cores */
#define SH_NBLK 1000u
#define SH_NSCENE 3u
static const char *const SHN[SH_NSCENE] = {"ANALOG, SPREAD 60, an 8-note chord", "ANALOG, SPREAD off, an 8-note chord", "PHASE, SPREAD 60, an 8-note chord"};
static uint32_t sh_ev;                                  /* 1: part 2 plays in blocks 500..699 (set by the harness before the fork: the reference and the run have the same scene) */
static uint64_t sh_scene(uint32_t id, blk_t blk)
{
    static const uint8_t N8[8] = {40, 43, 47, 50, 53, 57, 62, 67}, P2[3] = {67, 72, 76};
    static int32_t o[2 * CTL];
    uint32_t k, i, b;
    hsh = 0xCBF29CE484222325ull;
    peakmax = 0;
    host_tracks_init();
    host_preset(&trk[0], scn_eng(id == 2u ? "PHASE" : "ANALOG"), id == 2u ? 2 : 1);
    host_preset(&trk[1], scn_eng("PHASE"), 2);
    song.g[G_RTYPE] = 2;
    song.g[G_RLOCUT] = 0;
    song.g[G_DFDBK] = 90;
    song.g[G_DMIX] = 110;
    song.g[G_CDEPTH] = 100;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SPRD] = (int16_t)(id == 1u ? 0 : 60);
    trk[0].p[P_PAN] = -30;
    trk[0].p[P_DIST] = 40;
    trk[0].p[P_LTRIG] = 0;
    trk[0].p[P_M1SRC] = MS_LFO;
    trk[0].p[P_M1DST] = MD_PAN;
    trk[0].p[P_M1AMT] = 50;
    trk[1].p[P_VOICE] = V_POLY;
    trk[1].p[P_PAN] = 40;
    trk[1].p[P_ITYPE] = IT_CHORUS;
    trk[1].p[P_IA] = 60;
    trk[1].p[P_IB] = 90;
    trk[1].p[P_IMIX] = 127;
    for (k = 0; k < 2u; k++) {
        trk[k].p[P_TRIM] = 120;
        trk[k].p[P_CHOR] = 100;
        trk[k].p[P_DLY] = 90;
        trk[k].p[P_REV] = 110;
        trk[k].p[P_SUS] = 110;
        trk[k].p[P_REL] = 70;
    }
    trk[0].p[P_ATK] = 4;
    trk[0].p[P_DEC] = 0;
    duell_mode_force(DM_LAYER);
    trk[1].p[P_SPRD] = 0;
    trk[0].p[P_EQLG] = 12; trk[0].p[P_EQMG] = -12; trk[0].p[P_EQHG] = 12;
    trk[1].p[P_EQLG] = 12; trk[1].p[P_EQMG] = -12; trk[1].p[P_EQHG] = 12;
    for (b = 0; b < SH_NBLK; b++) {
        if (b == 2u)
            for (i = 0; i < 8u; i++)
                trk_note_on(&trk[0], N8[i], 110);
        if (b == 60u)
            trk_note_off(&trk[0], N8[0]);
        if (b == 61u)
            trk_note_on(&trk[0], N8[0], 100);
        if (sh_ev && b == 500u)
            for (i = 0; i < 3u; i++)
                trk_note_on(&trk[1], P2[i], 100);
        if (sh_ev && b == 700u)
            for (i = 0; i < 3u; i++)
                trk_note_off(&trk[1], P2[i]);
        if (b == 800u)
            for (i = 0; i < 8u; i++)
                trk_note_off(&trk[0], N8[i]);
        blk(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t a = o[i] < 0 ? -o[i] : o[i];
            peakmax = a > peakmax ? a : peakmax;
            hfeed((uint32_t)o[i]);
        }
        hfeed((uint32_t)trk[0].peak);
        hfeed((uint32_t)trk[1].peak);
    }
    return peakmax > 1000 ? hsh : 0;
}

static uint64_t scene_hash(uint32_t id, blk_t blk)
{
    return CHORD_SIM ? sh_scene(id, blk) : scene(id, blk);
}
static void fill_res(uint64_t h)
{
    R->hash = h;
    R->s = tcs;
#if TC_MEAS
    R->m = tcm;
#endif
    R->parks = tc.parks;
    R->up = tc.up;
    R->failed = tc.failed;
    R->strike = tc.strike;
    R->nv_last = tc_nv.last;
    R->reread = TC_LD(&tc_mb.reread);
    R->bad_w = TC_LD(&tc_mb.bad);
    R->misread = TC_LD(&tc_mb.misread);
    R->c1_jobs = c1_jobs;
    R->busb_c1 = c1_busb;
    R->busb_checks = c1_checks;
    R->own_checks = own_checks;
    R->own_writes = own_writes;
    R->own_timeouts = own_timeouts;
    R->look_chk = look_chk;
    R->look_writes = look_writes;
    R->set_jobs = set_jobs;
    R->set_moved = set_moved;
    R->both_jobs = both_jobs;
    R->fz_ok = fz_ok;
    R->fz_blk = fz_blk;
    R->fz_chk = fz_chk;
    R->fz_diff = fz_diff;
    R->nblk = nblk;
    R->park_us = (park_t1 - park_t0) / TC_TICKS_PER_US;
}
/* one run in a child process: kind -2 the one-core path with the blocks of ref_skip silent (skip_block), else the injected fault */
static void child_run(uint32_t id, int kind)
{
    uint64_t h;
    if (kind == -2) {
        h = scene_hash(id, skip_block);
        fill_res(h);
        return;
    }
    fault = kind;
#if TC_MEAS
    if (kind == F_VRUN || kind == F_VWIN)
        tcm_cfg.vrun = 4u, tcm_cfg.vwin_k = 8u;         /* (BENCHT3's two rules switched on, as `bench vrun 4` and `bench vwin 8` do; the BENCHT4 defaults have them off) */
    if (kind == F_FLOOR || kind == F_FLOORX)
        tcm_cfg.floor = 1;                              /* (the floor is off by default; these runs are about it) */
#endif
    tc_host_ld_hook = ld_hook;
    tc_host_st_hook = st_hook;
    time_adj(0);
    __atomic_store_n(&tc_host_ticks, BASE_TICKS, __ATOMIC_RELAXED);
    ahead_left = 1;
    tc_host_c1_started = start_core1;
    if (fault == F_NOANSWER)
        tc_host_start_ok = 0;
    if (fault != F_UNARMED)
        tc_arm();
    tc_boot(1u, 0, 0);
    while (thr_on && !__atomic_load_n(&c1_ready, __ATOMIC_ACQUIRE))
        cpu_relax();                                    /* (core 1 is in its loop before the first block) */
    h = scene_hash(id, sim_block);
    __atomic_store_n(&quit, 1u, __ATOMIC_RELEASE);
    if (thr_on)
        pthread_join(thr, 0);
    fill_res(h);
}
static void run(uint32_t id, int kind, res_t *out)
{
    pid_t pid;
    int st = 0;
    memset(R, 0, sizeof *R);
    fflush(stdout);
    pid = fork();
    if (pid == 0) {
        child_run(id, kind);
        _exit(0);
    }
    waitpid(pid, &st, 0);
    if (!(WIFEXITED(st) && WEXITSTATUS(st) == 0))
        R->hash = 0;
    *out = *R;
}

/* ------------------------------------------------------------ the fallback fade (item 11) --- */
/* part 2 holds 8 notes, its number drops to 3 (the budget), then the jobs pause (a very late block) or core 1 stops (a wrong check value): the notes above the number fade
 * out at once, in the next block, the oldest first, with no step in the output over 1.5 x the calm before it (budget_test.c (d)'s rule). Reports through R */
static int trim_child(int how)
{
    const uint32_t frames = 2u * FS;
    int32_t *L = calloc(frames + CTL, sizeof *L), o[2 * CTL], calm = 1, a = 0;
    uint32_t f, i, at = 0, after = 99, young = 0;
    host_tracks_init();
    xfade_sine(&trk[0]);
    xfade_sine(&trk[1]);
    trk[0].p[P_MUTE] = 1;
    bnotes[0] = bnotes[1] = bnotes1[0] = bnotes1[1] = 0;
    fault = how ? F_TRIMSTOP : F_NONE;
    tc_host_ld_hook = ld_hook;
    tc_host_st_hook = st_hook;
    time_adj(0);
    __atomic_store_n(&tc_host_ticks, BASE_TICKS, __ATOMIC_RELAXED);
    tc_host_c1_started = start_core1;
    tc_arm();
    tc_boot(1u, 0, 0);
    while (thr_on && !__atomic_load_n(&c1_ready, __ATOMIC_ACQUIRE))
        cpu_relax();
    for (f = 0; f < frames; f += CTL) {
        if (f == 0)
            for (i = 0; i < 8u; i++)
                trk_note_on(&trk[1], 48u + 4u * i, 100);
        if (f == (uint32_t)(0.8 * FS) / CTL * CTL)
            bnotes[1] = bnotes1[1] = 3;                 /* the budget drops (both rules' numbers: item 90): nothing is cut yet */
        if (f == (uint32_t)(1.0 * FS) / CTL * CTL)
            trim_job = tc.job + 3u;                     /* the third job from now is the late one (or the one written wrong) */
        sim_block(o, CTL);
        for (i = 0; i < CTL; i++)
            L[f + i] = o[2 * i];
        if (!at && tcs.trims)
            at = f;
        if (at && f == at + 8u * CTL) {
            after = young = 0;
            for (i = 0; i < NVOICE_PLAY; i++) {
                after += trk[1].v[i].active && trk[1].v[i].stage != 4u;
                young += trk[1].v[i].active && trk[1].v[i].stage != 4u && trk[1].v[i].note >= 48u + 4u * 5u;
            }
        }
    }
    __atomic_store_n(&quit, 1u, __ATOMIC_RELEASE);
    if (thr_on)
        pthread_join(thr, 0);
    if (at) {
        for (i = at - FS / 4u; i < at - 2u * CTL; i++)
            calm = abs(L[i] - L[i - 1]) > calm ? abs(L[i] - L[i - 1]) : calm;
        for (i = at; i < at + 3u * CTL; i++)
            a = abs(L[i] - L[i - 1]) > a ? abs(L[i] - L[i - 1]) : a;
    }
    free(L);
    R->hash = at ? 1u : 0u;
    R->s = tcs;
    R->failed = tc.failed;
    R->c1_jobs = (uint32_t)a;
    R->park_us = (uint32_t)calm;
    R->nblk = after;
    R->busb_c1 = young;
    return 1;
}

int main(int argc, char **argv)
{
    uint32_t s, s0 = 0, s1 = CHORD_SIM ? SH_NSCENE : NSCENE, f;
    int nomut = 0, tsan = 0, audit = 0, k;
    static res_t ref, r, rs;
    tc_stat_t tot[F_N];
    uint32_t ntot[F_N];
    for (k = 1; k < argc; k++) {
        if (argv[k][0] != '-')
            s0 = (uint32_t)atoi(argv[k]), s1 = s0 + 1u;
        else if (!strcmp(argv[k], "-nomut"))
            nomut = 1;
        else if (!strcmp(argv[k], "-audit"))
            audit = nomut = 1;                          /* (all scenes, the run without a fault only: the ownership audit of twocore_mut) */
        else if (!strcmp(argv[k], "-tsan"))
            tsan = nomut = 1;
    }
    R = mmap(0, sizeof *R, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(tot, 0, sizeof tot);
    memset(ntot, 0, sizeof ntot);
    for (s = s0; s < s1; s++) {
        char what[240];
        uint64_t hr, hf;
        const char *sname = CHORD_SIM ? SHN[s] : SCN[s];
        sh_ev = 0;
        run(s, F_UNARMED, &ref);                      /* the reference: one core, the same build */
        hr = ref.hash;
        snprintf(what, sizeof what, "scene %u (%s) is not silent", s, sname);
        expect(what, hr != 0);
        printf("twocore: scene %u (%s): one-core reference %016llx, %u blocks\n", s, sname, (unsigned long long)hr, ref.nblk);
        for (f = 0; f < F_N; f++) {
            int equal_ref = f != F_HANG_PRE && f != F_HANG_POST && f != F_PARK_STUCK && f != F_SILENT_LATE && f != F_SHARED && f != F_BUSB && f != F_VLONG && f != F_FREEZE && f != F_AHEAD;
            const tc_stat_t *c;
            uint32_t sum;
            if (CHORD_SIM ? !sh_listed((int)f) : f >= F_FREEZE)
                continue;
            if (audit && f != F_NONE)
                continue;
            if (f == F_SHARED && (nomut || s != 0))
                continue;
            if (f == F_BUSB && nomut)
                continue;
            if ((f == F_TCMCLR || f == F_FLOOR || f == F_FLOORX || f == F_NOFLOOR || f == F_VRUN || f == F_VWIN) && !TC_MEAS)
                continue;
            if (tsan && (f == F_HANG_PRE || f == F_HANG_POST || f == F_PARK_STUCK || f == F_SILENT_LATE))
                continue;                               /* (a window in which core 0 runs the sends while core 1 still holds the parts is not race free: docs "Job 5") */
            hf = hr;
            if (f == F_P2) {                            /* part 2's chord is in this scene: its reference is the one-core path of the same scene */
                static res_t ref2;
                sh_ev = 1;
                run(s, F_UNARMED, &ref2);
                hf = ref2.hash;
            }
            run(s, (int)f, &r);
            sh_ev = 0;
            c = &r.s;
            printf("twocore:   %-11s blocks %4u in time %4u late %3u very late %3u silent %3u (waiting %3u) own %4u refused %u no answer %u | pauses %u (fault %u run %u window %u late %u) very late without pause %u resumes %u | rereads %u parks %u | "
                   "core 1 jobs %4u bus B writes %u of %u | no take-over %u | up %u strike %u record %u failed %u bad %u | %016llx\n",
                   FNAME[f], c->blocks, c->in_time, c->late, c->vlate, c->silent, c->wait, c->own, c->refused, c->noanswer, c->pauses, c->pause_fault, c->pause_run, c->pause_win, c->pause_late,
                   c->vlate_ok, c->resumes, r.reread, r.parks, r.c1_jobs,
                   r.busb_c1, r.busb_checks, r.nott, r.up, r.strike, r.nv_last, r.failed, r.bad_w, (unsigned long long)r.hash);
            if (f == F_NONE) {
                uint32_t k2;
                printf("twocore:   deadlines (mean, microseconds after the half's start, of the posted jobs; the half is %u):", TCR_HALF_TICKS / TC_TICKS_PER_US);
                for (k2 = 0; k2 < TCR_BLOCKS; k2++)
                    printf(" block %u: %u (%u jobs)", k2, r.dl_n[k2] ? (uint32_t)(r.dl_sum[k2] / r.dl_n[k2]) / TC_TICKS_PER_US : 0u, r.dl_n[k2]);
                printf("\n");
                printf("twocore:   wait limits (mean, microseconds after the half's start; the spare time core 0 gives a very late job = limit - deadline):");
                for (k2 = 0; k2 < TCR_BLOCKS; k2++)
                    printf(" block %u: %u (+%u)", k2, r.dl_n[k2] ? (uint32_t)(r.lim_sum[k2] / r.dl_n[k2]) / TC_TICKS_PER_US : 0u, r.dl_n[k2] ? (uint32_t)((r.lim_sum[k2] - r.dl_sum[k2]) / r.dl_n[k2]) / TC_TICKS_PER_US : 0u);
                printf("\n");
            }
            sum = c->in_time + c->late + c->vlate + c->refused + c->own + c->wait;
            snprintf(what, sizeof what, "scene %u %s: every block is counted once (blocks %u = %u)", s, FNAME[f], c->blocks, sum);
            expect(what, c->blocks == r.nblk && sum == c->blocks);
            snprintf(what, sizeof what, "scene %u %s: the pauses (%u) add up by reason (fault %u + run %u + window %u + late %u), and every very late block (%u) either paused, was a fault (silent %u at most) or is counted without a pause (%u)", s, FNAME[f], c->pauses,
                     c->pause_fault, c->pause_run, c->pause_win, c->pause_late, c->vlate, c->silent, c->vlate_ok);
            expect(what, c->pauses == c->pause_fault + c->pause_run + c->pause_win + c->pause_late && c->vlate_ok <= c->vlate && c->vlate_ok + c->pause_run + c->pause_win + c->silent + c->refused + c->noanswer >= c->vlate);
            snprintf(what, sizeof what, "scene %u %s: core 0 never waited past its wait limit nor past the half's end (%u of %u blocks with a job left the wait later than the limit, %u later than the half's end, the largest overrun %u ticks)", s, FNAME[f],
                     r.lim_over, r.lim_blocks, r.end_over, r.lim_max_over);
            expect(what, r.lim_over == 0 && r.end_over == 0);
            if (equal_ref) {
                snprintf(what, sizeof what, "scene %u %s: the output equals the one-core run bit for bit", s, FNAME[f]);
                expect(what, r.hash == hf);
            }
            {   /* the job counters, every run */
                uint32_t named = c->sh_done + c->sh_late + c->sh_freeze + c->sh_fault, extra = c->sh_posts - named;
                printf("twocore:     jobs: posts %u = done %u + late %u + freeze %u + refused %u (+ %u unnamed) | audit: jobs %u, with a voice for core 1 %u (moved %u), voices on both cores %u, sets hashed at the first look %u (written by core 0 before the claim %u)\n",
                       c->sh_posts, c->sh_done, c->sh_late, c->sh_freeze, c->sh_fault, extra, r.own_checks, r.set_jobs, r.set_moved, r.both_jobs, r.look_chk, r.look_writes);
                snprintf(what, sizeof what, "scene %u %s: every job posted (%u) is counted once: done %u + late %u + freeze %u + refused %u, and %u more (only blocks whose take got no answer and then a yield, which have no counter of their own: no answer %u)", s, FNAME[f],
                         c->sh_posts, c->sh_done, c->sh_late, c->sh_freeze, c->sh_fault, extra, c->noanswer);
                expect(what, c->sh_posts >= named && extra <= c->noanswer);
                if (CHORD_SIM && f != F_UNARMED && f != F_NOANSWER && f != F_TORNP && f != F_BADCHK) {
                    snprintf(what, sizeof what, "scene %u %s: the chord went through the hand-over (%u jobs posted in %u blocks)", s, FNAME[f], c->sh_posts, c->blocks);
                    expect(what, c->sh_posts >= 100u);
                }
                if (CHORD_SIM) {
                    snprintf(what, sizeof what, "scene %u %s: no block where core 0 could not take the job back in time (no take-over %u)", s, FNAME[f], r.nott);
                    expect(what, r.nott == 0);
                }
            }
            tot[f].blocks += c->blocks, tot[f].in_time += c->in_time, tot[f].late += c->late, tot[f].vlate += c->vlate, tot[f].silent += c->silent, tot[f].own += c->own;
            tot[f].refused += c->refused, tot[f].noanswer += c->noanswer, tot[f].pauses += c->pauses, tot[f].resumes += c->resumes, tot[f].wait += c->wait;
            ntot[f] += r.reread;
#if TC_MEAS
            {   /* BENCHT2: the timing figures. Every run: the own split adds up to own, the histograms to the jobs measured, the very late blocks to the very late jobs; then what each fault must show */
                const tcm_t *m = &r.m;
                uint32_t i, hs = 0, hr2 = 0, hm = 0, vb = 0, ow = 0;
                for (i = 0; i < TCM_NB; i++)
                    hs += m->hsd[i], hr2 += m->hrt[i], hm += m->hmg[i];
                for (i = 0; i < TCR_BLOCKS; i++)
                    vb += m->vblk[i];
                for (i = 0; i < TCM_OWN_N; i++)
                    ow += m->own[i];
                if (f == F_TCMCLR) {
                    snprintf(what, sizeof what, "scene %u tcmclr: the figures were cleared by the audio interrupt at the block after the request (b0 %u = 160 blocks), only the later jobs are in them (%u of %u blocks)", s, m->b0, m->posted, c->blocks);
                    expect(what, m->b0 == 160u && m->posted > 0 && m->posted <= c->blocks - 160u && m->n <= m->posted && !m->clr);
                } else {
                    snprintf(what, sizeof what, "scene %u %s: own split adds up to own (%u = %u), histograms to the jobs measured (%u %u %u = %u), very late jobs by block (%u) = very late jobs measured (%u), no impossible clock (%u)",
                             s, FNAME[f], ow, c->own, hs, hr2, hm, m->n, vb, m->nv, m->badclk);
                    expect(what, ow == c->own && hs == m->n && hr2 == m->n && hm == m->n && vb == m->nv && m->badclk == 0 && m->nv <= c->vlate && m->n <= m->posted);
                }
                snprintf(what, sizeof what, "scene %u %s: no deadline was later than core 0's own latest moment to play the job itself (%u posted jobs over it)", s, FNAME[f], r.cap_viol);
                expect(what, r.cap_viol == 0);
                if (f == F_NONE) {
                    snprintf(what, sizeof what, "scene %u none: every job posted was measured (%u of %u = in time %u), the own blocks were all 'deadline already past' (%u of %u), no very late job, the margin never negative (%d), the run times and delays small (%u, %u ticks)",
                             s, m->n, m->posted, c->in_time, m->own[TCM_OWN_DL], c->own, m->mg_min, m->rt_max, m->sd_max);
                    expect(what, m->n == m->posted && m->n == c->in_time && m->own[TCM_OWN_DL] == c->own && m->nv == 0 && m->mg_min >= 0 && m->rt_max < TCR_HALF_TICKS && m->sd_max < TCR_HALF_TICKS && m->ev_n == 0);
                }
                if (f == F_NONE) {
                    uint32_t nbs = 0, same = 1;
                    for (i = 0; i < TCR_BLOCKS; i++)
                        nbs += m->nb[i], same &= m->dl_sum[i] == r.dl_sum[i] && m->nb[i] == r.dl_n[i] && m->st_sum[i] <= m->dl_sum[i];
                    snprintf(what, sizeof what, "scene %u none: per block of the half the measured deadlines and job counts are the ones the simulation recorded at the post (%u jobs), and no job started after its deadline", s, nbs);
                    expect(what, nbs == m->n && same);
                }
                if (f == F_LATE_PRE) {
                    snprintf(what, sizeof what, "scene %u late_pre: the jobs core 0 took back (late %u) have no figures: posted %u = measured %u + late %u", s, c->late, m->posted, m->n, c->late);
                    expect(what, m->posted == m->n + c->late);
                }
                if (f == F_LATE_POST) {
                    snprintf(what, sizeof what, "scene %u late_post: every very late job (%u) was measured with its block and run time, finished after the deadline (margin %d <= -1000 ticks), ring entries %u", s, c->vlate, m->mg_min, m->ev_n);
                    expect(what, m->nv == c->vlate && c->vlate > 0 && m->mg_min <= -1000 && m->vrt_max >= 1000u && m->ev_n == m->nv && m->own[TCM_OWN_PAUSE] == 0 && m->hmg[0] + m->hmg[1] + m->hmg[2] >= m->nv);
                }
                if (f == F_LATE_RUN) {
                    snprintf(what, sizeof what, "scene %u late_run: no 'paused' own blocks (%u; owner rule G21: late blocks pause nothing)", s, m->own[TCM_OWN_PAUSE]);
                    expect(what, m->own[TCM_OWN_PAUSE] == 0);
                }
                if (f == F_PARK) {
                    snprintf(what, sizeof what, "scene %u park: the blocks of the flash park show as 'flash park' own blocks (%u)", s, m->own[TCM_OWN_PARK]);
                    expect(what, m->own[TCM_OWN_PARK] > 0);
                }
                if (f == F_UNARMED || f == F_NOANSWER) {
                    snprintf(what, sizeof what, "scene %u %s: every block is 'core 1 down' (%u of %u), nothing posted (%u)", s, FNAME[f], m->own[TCM_OWN_DOWN], c->blocks, m->posted);
                    expect(what, m->own[TCM_OWN_DOWN] == c->blocks && m->posted == 0 && m->n == 0);
                }
                if (f == F_TORNP || f == F_BADCHK) {
                    snprintf(what, sizeof what, "scene %u %s: after the strike the blocks are 'core 1 down' (%u)", s, FNAME[f], m->own[TCM_OWN_DOWN]);
                    expect(what, m->own[TCM_OWN_DOWN] > 0);
                }
            }
#endif
            switch (f) {
            case F_NONE:
                snprintf(what, sizeof what, "scene %u none: no fault, no late block, no refusal, no strike", s);
                expect(what, c->late == 0 && c->vlate == 0 && c->refused == 0 && !r.strike && r.up && !r.failed && c->pauses == 0);
                /* (DUELL 1.1 item 142: the wait limit at true cost (twocore_run.c tcr_deadline) is later, so scene 11's blocks of no take-over (305 a run before) now get the true-cost
                 * take-over deadline (tcr_deadline_true) and no scene has such blocks; the decision itself is checked for every setup in tests/deadline_test.c, KEPT / INSIDE) */
                snprintf(what, sizeof what, "scene %u none: jobs posted with no take-over (item 90: two-core numbers sound and core 0 can no longer take the job back in time): %u of %u; none in any scene since item 142's wait limit at true cost (scene 11 had 305)", s, r.nott, r.lim_blocks);
                expect(what, r.nott == 0);
                snprintf(what, sizeof what, "scene %u none: core 1's job was done in time in most blocks (%u of %u)", s, c->in_time, c->blocks);
                expect(what, c->in_time > c->blocks / 2u && r.c1_jobs >= c->in_time);
                /* (2026-11-07: the globals run in core 1's job, so bus B is core 1's to write now; the old "core 1 wrote no bus B state" check is gone, the sum is only counted) */
                snprintf(what, sizeof what, "scene %u none: core 1's job changed no voice dealt to core 0 and no voice that was not listed: %u of %u jobs wrote one, %u gate timeouts", s, r.own_writes, r.own_checks, r.own_timeouts);
                expect(what, r.own_writes == 0 && r.own_timeouts == 0 && r.own_checks >= c->in_time);
                snprintf(what, sizeof what, "scene %u none: every job done (%u of %u posted, none late, frozen or refused), core 0 wrote none of core 1's voices between the deal word and its wait (%u of %u jobs), every job with a voice for core 1 moved its voices (%u of %u)", s,
                         c->sh_done, c->sh_posts, r.look_writes, r.look_chk, r.set_moved, r.set_jobs);
                expect(what, c->sh_done == c->sh_posts && c->sh_late == 0 && c->sh_freeze == 0 && c->sh_fault == 0 && r.look_chk == r.own_checks && r.look_writes == 0 && r.set_moved == r.set_jobs);
                if (CHORD_SIM) {
                    snprintf(what, sizeof what, "scene %u none: the deal gave core 1 voices of the chord (%u of %u jobs had a voice for core 1; in %u both cores had one)", s, r.set_jobs, r.own_checks, r.both_jobs);
                    expect(what, r.set_jobs >= 100u);
                }
                break;
            case F_TORN1:
                snprintf(what, sizeof what, "scene %u torn1: re-read and run, counted (%u), no refusal", s, r.reread);
                expect(what, r.reread > 0 && c->refused == 0 && !r.strike && r.bad_w == 0);
                break;
            case F_TORNP:
                snprintf(what, sizeof what, "scene %u tornp: refused, a strike (BADJOB), core 1 held, core 0 plays everything from then on", s);
                expect(what, c->refused == 1 && r.failed == TCR_BADJOB && r.strike && !r.up && c->own > 0 && r.nv_last == TCR_BADJOB);
                break;
            case F_LATE_PRE:
                /* (2026-11-07, the two-phase job: core 1's share now holds the globals, so the take-back deadline of a half's LAST block is past at the post more often (a block with no
                 * take-over, r.nott) while the simulated clock runs during the late jobs. The no-take-over expectations below describe what happens when a LATE job falls in such a block
                 * (a silent block, a fault pause); when none did (silent 0, pauses 0) the plain late-path expectations hold, with the blocks of no take-over just counted) */
                if (!r.nott || (c->silent == 0 && c->pauses == 0)) {
                    snprintf(what, sizeof what, "scene %u late_pre: the late path (take, yld) was taken and counted (%u), no strike", s, c->late);
                    expect(what, c->late > 0 && !r.strike && r.up && c->refused == 0 && c->vlate == 0);
                } else {   /* item 90 (tcr_join, tcr.nott): in a block where core 0 cannot take part 2 back there is no take-over; a job not started at the wait limit is a silent block and a fault pause, a take that gets no answer is a very late fault */
                    snprintf(what, sizeof what, "scene %u late_pre (blocks with no take-over %u): the late path (take, yld) was taken where core 0 had time (late %u); where it had none a silent block and a fault pause (silent %u, pauses %u of which fault %u), every very late block an unanswered take (%u of %u), no strike", s, r.nott, c->late, c->silent, c->pauses, c->pause_fault, c->noanswer, c->vlate);
                    expect(what, c->late > 0 && c->silent > 0 && c->pauses > 0 && c->pause_fault == c->pauses && c->vlate == c->noanswer && c->vlate_ok == 0 && !r.strike && r.up && c->refused == 0);
                }
                break;
            case F_LATE_POST:
                if (!r.nott || (c->silent == 0 && c->pauses == 0)) {   /* (as late_pre: no late job fell in a block of no take-over) */
                    snprintf(what, sizeof what, "scene %u late_post: single very late blocks that finished in their half are counted (%u) and pause nothing (pauses %u, without a pause %u), no silent block, no strike", s, c->vlate, c->pauses, c->vlate_ok);
                    expect(what, c->vlate > 0 && c->silent == 0 && c->pauses == 0 && c->vlate_ok == c->vlate && !r.strike && r.up);
                } else {   /* item 90: with no take-over the wait limit is the job's deadline, so a claimed job that is not done by it is a silent block and a fault pause; the others are single very late blocks that pause nothing */
                    snprintf(what, sizeof what, "scene %u late_post (blocks with no take-over %u): single very late blocks that finished in their half are counted without a pause (%u of %u), those not done by the limit are a silent block and a fault pause (silent %u, pauses %u, fault %u), no strike", s, r.nott, c->vlate_ok, c->vlate, c->silent, c->pauses, c->pause_fault);
                    expect(what, c->vlate > 0 && c->vlate_ok > 0 && c->silent > 0 && c->pauses > 0 && c->pause_fault == c->pauses && c->vlate_ok + c->pauses >= c->vlate && !r.strike && r.up);
                }
                break;
            case F_VOFF:
                if (!r.nott) {
                    snprintf(what, sizeof what, "scene %u voff: with the BENCHT4 defaults %u very late blocks (4 in a row, 8 within the window among them) paused nothing (pauses %u, without a pause %u), no silent block, no strike", s, c->vlate, c->pauses, c->vlate_ok);
                    expect(what, c->vlate >= 24u && c->pauses == 0 && c->vlate_ok == c->vlate && c->silent == 0 && !r.strike && r.up);
                } else {   /* item 90: the run and window rules stay off (no pause by them, however many very late blocks); a job not done by the limit of a block with no take-over is the one fault pause */
                    snprintf(what, sizeof what, "scene %u voff (blocks with no take-over %u): with the BENCHT4 defaults %u very late blocks (4 in a row, 8 within the window among them) never paused by a run or the window (run %u, window %u, late %u); only a job not done by its limit paused (fault %u of %u pauses), every other very late block counted without a pause (%u), no strike", s, r.nott, c->vlate, c->pause_run, c->pause_win, c->pause_late, c->pause_fault, c->pauses, c->vlate_ok);
                    expect(what, c->vlate >= 24u && c->pause_run == 0 && c->pause_win == 0 && c->pause_late == 0 && c->pause_fault == c->pauses && c->vlate_ok + c->pauses >= c->vlate && c->vlate_ok > 0 && !r.strike && r.up);
                }
                break;
            case F_NOANS:
                snprintf(what, sizeof what, "scene %u noans: a core 1 that answers the take later than 100 us is a real fault: no answer %u, then yld, a fault pause (%u of %u pauses), core 0 played the job itself (output equal), no strike, nothing counted as without a pause (%u)", s, c->noanswer, c->pause_fault, c->pauses, c->vlate_ok);
                if (r.nott) {   /* item 90: where core 0 had no time to take part 2 back, the unanswered take left part 2 silent for the block (a fault pause as well): silent blocks are expected, besides the blocks waiting for the open job */
                    snprintf(what, sizeof what, "scene %u noans (blocks with no take-over %u): no answer %u, a fault pause for each (%u of %u pauses), core 0 played part 2 where it could (own %u), part 2 silent where it could not (silent %u), no strike, nothing counted as without a pause (%u)", s, r.nott, c->noanswer, c->pause_fault, c->pauses, c->own, c->silent, c->vlate_ok);
                    expect(what, c->noanswer > 0 && c->pauses > 0 && c->pause_fault == c->pauses && c->vlate_ok == 0 && c->own > 0 && c->silent > 0 && !r.strike && r.up);
                } else {
                    expect(what, c->noanswer > 0 && c->pauses > 0 && c->pause_fault == c->pauses && c->vlate_ok == 0 && c->silent == 0 && !r.strike && r.up);
                }
                break;
            case F_VLONG:
                snprintf(what, sizeof what, "scene %u vlong: a job that is not done by the wait limit makes the parts silent until it closes (silent %u) and pauses NOTHING (G21; it was (%u of %u pauses); core 0 left every wait at the limit (largest overrun %u ticks, %u of %u blocks over), none past the half's end (%u); no strike", s, c->silent, c->pause_fault, c->pauses, r.lim_max_over, r.lim_over, r.lim_blocks, r.end_over);
                expect(what, c->silent > 0 && c->pauses == 0 && c->vlate_ok == 0 && r.lim_over == 0 && r.end_over == 0 && !r.strike && r.up);   /* (owner rule G21, 2026-11-05: NO REST after a late block: the jobs go on when the late one closes) */
                break;
            case F_VRUN:
                snprintf(what, sizeof what, "scene %u vrun: 4 very late blocks in a row paused the jobs twice (run %u of %u pauses), 3 in a row did not; resumed (%u); the other very late blocks (%u of %u) paused nothing; no silent block, no strike", s, c->pause_run, c->pauses, c->resumes, c->vlate_ok, c->vlate);
                expect(what, c->vlate == 11u && c->pauses == 2u && c->pause_run == 2u && c->resumes + 1u >= c->pauses && c->vlate_ok == 9u && c->silent == 0 && !r.strike && r.up);
                break;
            case F_VWIN:
                snprintf(what, sizeof what, "scene %u vwin: 8 very late blocks within the window (never 2 in a row) paused the jobs once (window %u of %u pauses), 7 later did not (%u very late, %u without a pause); no silent block, no strike", s, c->pause_win, c->pauses, c->vlate, c->vlate_ok);
                expect(what, c->vlate == 15u && c->pauses == 1u && c->pause_win == 1u && c->vlate_ok == 14u && c->silent == 0 && !r.strike && r.up);
                break;
#if TC_MEAS
            case F_FLOOR:
                snprintf(what, sizeof what, "scene %u floor: the floor lifted the deadline of block 0 so the slow job finishes in time (core 1 learns it in the first half, and again after a deadline that moved too early left it unused for 128 to 256 blocks): very late %u in %u halves, no pause, no silent block", s, c->vlate, c->blocks / 4u);
                expect(what, c->vlate <= 2u + c->blocks / 150u && c->pauses == 0 && c->silent == 0 && !r.strike && r.up);
                break;
            case F_FLOORX:
                snprintf(what, sizeof what, "scene %u floorx: a job that cannot fit before the latest deadline is not given: own because of the floor %u blocks, very late %u in %u halves (core 1 is tried again when the measured time has faded out), no pause, no silent block, no strike", s,
                         r.m.own[TCM_OWN_FL], c->vlate, c->blocks / 4u);
                expect(what, r.m.own[TCM_OWN_FL] >= c->blocks / 16u && c->vlate <= 2u + c->blocks / 150u && c->pauses == 0 && c->silent == 0 && !r.strike && r.up);
                break;
            case F_NOFLOOR:
                snprintf(what, sizeof what, "scene %u nofloor: with the floor off the same slow job is very late again and again (%u in %u halves), nothing pauses, no own because of the floor (%u)", s, c->vlate, c->blocks / 4u, r.m.own[TCM_OWN_FL]);
                expect(what, c->vlate >= 10u && c->pauses == 0 && c->silent == 0 && r.m.own[TCM_OWN_FL] == 0 && !r.strike && r.up);
                break;
#endif
            case F_LATE_RUN:
                snprintf(what, sizeof what, "scene %u late_run: runs of late blocks (%u late: core 0 played them, the same samples) pause nothing (pauses %u; owner rule G21), no strike", s, c->late, c->pauses);
                expect(what, c->late >= 9u && c->pauses == 0 && !r.strike && r.up);
                break;
            case F_BADCHK:
                snprintf(what, sizeof what, "scene %u badchk: refused, a strike (BADJOB), core 0 plays everything", s);
                expect(what, c->refused == 1 && r.failed == TCR_BADJOB && r.strike && !r.up);
                break;
            case F_STALE:
                snprintf(what, sizeof what, "scene %u stale: a stale done word took the deadline path (very late %u), done after, no strike", s, c->vlate);
                expect(what, c->vlate > 0 && c->refused == 0 && !r.strike && r.up);
                break;
            case F_BADSNAP:
                snprintf(what, sizeof what, "scene %u badsnap: the snapshot that fits no hash is refused, a strike (BADJOB), core 0 plays the block itself with the right snapshot", s);
                expect(what, c->refused == 1 && r.failed == TCR_BADJOB && r.strike && !r.up);   /* (core 0 may see bad_job before core 1 wrote bad: it is held at once) */
                break;
            case F_AHEAD:
            case F_HANG_PRE:
            case F_HANG_POST:
            case F_PARK_STUCK: {
                uint32_t first = 0, last = 0, nsk = 0, i, pre_ok = 1, kinds_ok = 1;
                static uint8_t sk[MAXB];
                static uint16_t sl[MAXB][NLIVE];
                res_t *ps = &rs;
                for (i = 0; i < r.nblk && i < MAXB; i++)
                    if (r.skip[i]) {
                        if (!nsk)
                            first = i;
                        last = i;
                        kinds_ok &= r.skip[i] == (nsk ? K_WAIT : K_FROZEN);
                        nsk++;
                    }
                if (f == F_AHEAD) {
                    snprintf(what, sizeof what, "scene %u ahead: a done word ahead is BADWORD, a strike, core 1 held; the parts silent in that one block (%u), then core 0 played on", s, nsk);
                    expect(what, r.failed == TCR_BADWORD && r.strike && !r.up && c->refused == 1 && nsk == 1u && last + 1u < r.nblk && c->own > 0);
                } else {
                    snprintf(what, sizeof what, "scene %u %s: core 1 stopped (%s), a strike, the parts silent in %u blocks (%u counted), then core 0 played on", s, FNAME[f],
                             f == F_PARK_STUCK ? "FLASH" : "HANG", nsk, c->silent);
                    expect(what, nsk > 0 && nsk == c->silent && r.failed == (f == F_PARK_STUCK ? TCR_FLASH : TCR_HANG) && r.strike && !r.up && last + 1u < r.nblk && c->own > 0);
                }
                snprintf(what, sizeof what, "scene %u %s: the silent blocks are one run (%u..%u): the frozen block of the job, then blocks waiting for it", s, FNAME[f], first, last);
                expect(what, nsk == last - first + 1u && kinds_ok);
                for (i = 0; i < first; i++)
                    pre_ok &= r.bh[i] == ref.bh[i];
                snprintf(what, sizeof what, "scene %u %s: the output equals the reference up to the first silent block (block %u)", s, FNAME[f], first);
                expect(what, pre_ok);
                memcpy(sk, r.skip, sizeof sk);
                memcpy(sl, r.sel, sizeof sl);
                ref_skip = sk;
                ref_sel = (const uint16_t (*)[NLIVE])sl;
                ref_fade = f == F_HANG_PRE ? 0xFFFFFFFFu : last + 1u;   /* (hung before its claim: the parts are whole, nothing fades) */
                ref_close_same = f == F_AHEAD;
                run(s, -2, ps);
                ref_close_same = 0;
                snprintf(what, sizeof what, "scene %u %s: the whole output equals the one-core path with exactly those blocks silent (the frozen block's control pass and core 0's own sets run, nothing else of it)%s", s, FNAME[f], f == F_HANG_PRE ? "" : ", and the fade");
                if (!expect(what, ps->hash == r.hash)) {   /* (2026-11-07: where it differs, for the hunt) */
                    uint32_t d;
                    for (d = 0; d < r.nblk && d < MAXB && d < ps->nblk; d++)
                        if (r.bh[d] != ps->bh[d])
                            break;
                    printf("twocore:     first differing block %u (kind %u; frozen %u..%u, fade at %u; blocks %u / %u)\n", d, d < MAXB ? r.skip[d] : 9u, first, last, last + 1u, r.nblk, ps->nblk);
                }
                if (f == F_PARK_STUCK) {
                    snprintf(what, sizeof what, "scene %u park_stuck: FLASH after 20 ms of simulated time (%u us)", s, r.park_us);
                    expect(what, r.park_us >= 20000u && r.park_us < 20100u);
                } else if (f != F_AHEAD) {
                    snprintf(what, sizeof what, "scene %u %s: the stop came from the 100 ms heartbeat watch (noanswer %u)", s, FNAME[f], c->noanswer);
                    expect(what, f == F_HANG_PRE ? c->noanswer > 0 : 1);
                }
                break;
            }
            case F_FREEZE: {   /* claimed and late in the last block of a half: the parts are silent for exactly that block */
                uint32_t i, last = r.fz_blk, ok = 1, nz = 0;
                static uint8_t sk2[MAXB];
                static uint16_t sl2[MAXB][NLIVE];
                snprintf(what, sizeof what, "scene %u freeze: the parts froze once, in block %u (the last of a half, %u), as the only silent block (%u), NO pause (%u fault of %u pauses; owner rule G21), nothing counted as without a pause (%u), no strike, core 1 still up", s, r.fz_blk,
                         r.fz_blk % TCR_BLOCKS, c->silent, c->pause_fault, c->pauses, c->vlate_ok);
                expect(what, r.fz_ok && r.fz_blk % TCR_BLOCKS == TCR_BLOCKS - 1u && c->sh_freeze == 1u && c->silent == 1u && c->pauses == 0u && c->vlate_ok == 0u && !r.strike && r.up);
                snprintf(what, sizeof what, "scene %u freeze: no voice of the parts was touched between the freeze and the late job (checked %u, changed %u), the jobs went on right after it (done %u of %u posts)", s, r.fz_chk, r.fz_diff, c->sh_done, c->sh_posts);
                expect(what, r.fz_chk == 1u && r.fz_diff == 0u && c->sh_done > c->sh_freeze + 50u);
                for (i = 0; i < r.nblk && i < MAXB; i++)
                    nz += r.skip[i] != 0;
                memcpy(sk2, r.skip, sizeof sk2);
                memcpy(sl2, r.sel, sizeof sl2);
                ref_skip = sk2;
                ref_sel = (const uint16_t (*)[NLIVE])sl2;
                ref_fade = 0xFFFFFFFFu;
                run(s, -2, &rs);
                for (i = 0; i <= last + 1u && i < MAXB; i++)   /* (the one-block step: the frozen block's silence is in the next block's output) */
                    ok &= rs.bh[i] == r.bh[i];
                snprintf(what, sizeof what, "scene %u freeze: the output up to and including the block after the frozen block %u (where its silence is heard; %u silent) equals the one-core path with that block frozen (what comes after is not compared: the late job renders that block later)", s, last, nz);
                expect(what, ok && nz == 1u && r.skip[last < MAXB ? last : 0] == K_FROZEN);
                break;
            }
            case F_P2:
                snprintf(what, sizeof what, "scene %u p2chord: part 2 played a chord beside part 1's: the jobs went on (in time %u of %u blocks), nothing late, refused or paused", s, c->in_time, c->blocks);
                expect(what, c->in_time > c->blocks / 2u && c->sh_late == 0u && c->sh_freeze == 0u && c->late == 0u && c->vlate == 0u && c->refused == 0u && c->pauses == 0u && !r.strike && r.up);
                break;
            case F_PARK:
                snprintf(what, sizeof what, "scene %u park: a flash park in the middle of play, counted (%u), everything played by core 0 meanwhile (own %u), core 1 still up", s, r.parks, c->own);
                expect(what, r.parks == 1u && c->own >= 3u && r.up && !r.strike);
                break;
            case F_SILENT_LATE:
                snprintf(what, sizeof what, "scene %u silent_late: the parts silent in %u blocks until the late job closed, nothing paused (pauses %u; owner rule G21), no strike", s, c->silent, c->pauses);
                expect(what, c->vlate > 0 && c->silent > 0 && c->pauses == 0 && !r.strike && r.up);
                break;
            case F_UNARMED:
                snprintf(what, sizeof what, "scene %u unarmed: one core, every block on core 0 (own %u), the record says none (%u)", s, c->own, r.nv_last);
                expect(what, !r.up && c->own == c->blocks && r.nv_last == TCR_NONE && !r.failed && r.c1_jobs == 0);
                break;
            case F_NOANSWER:
                snprintf(what, sizeof what, "scene %u noanswer: the start failed, one core, the record says NOANSWER (%u), a strike", s, r.nv_last);
                expect(what, !r.up && c->own == c->blocks && r.nv_last == TCR_NOANSWER && r.failed == TCR_NOANSWER && r.strike);
                break;
            case F_SHARED:
                snprintf(what, sizeof what, "self-check: core 1 writing part 1's REVERB send after its job changes the output (the test sees a stray write)");
                expect(what, r.hash != hr);
                break;
            case F_BUSB:
                snprintf(what, sizeof what, "self-check: core 1 writing bus B's delay half is seen by the bus B check (%u)", r.busb_c1);
                expect(what, r.busb_c1 >= 1u);
                break;
            default:
                break;
            }
        }
    }
    printf("twocore: totals over %u scenes (blocks, in time, late, very late, silent, own, refused, pauses, resumes):\n", s1 - s0);
    for (f = 0; f < F_N; f++)
        if (tot[f].blocks)
            printf("twocore:   %-11s %6u %6u %5u %5u %5u %6u %3u %4u %4u  rereads %u\n", FNAME[f], tot[f].blocks, tot[f].in_time, tot[f].late, tot[f].vlate, tot[f].silent, tot[f].own,
                   tot[f].refused, tot[f].pauses, tot[f].resumes, ntot[f]);
    if (!audit)
        expect("the late path was taken and counted in some blocks (late_pre)", tot[F_LATE_PRE].late > 0);
    expect("several hundred blocks per scene went through the hand-over (none)", tot[F_NONE].in_time >= 300u * (s1 - s0));
    if (!tsan && !audit && !CHORD_SIM) {
        /* the fallback fade (item 11) (part 2's notes: not in the chord scenes, where part 2 is silent) */
        int how;
        for (how = 0; how < 2; how++) {
            pid_t pid;
            int st = 0;
            res_t t;
            memset(R, 0, sizeof *R);
            fflush(stdout);
            pid = fork();
            if (pid == 0)
                _exit(trim_child(how) ? 0 : 1);
            waitpid(pid, &st, 0);
            t = *R;
            {
                char what[300];
                int32_t a = (int32_t)t.c1_jobs, calm = (int32_t)t.park_us;
                snprintf(what, sizeof what, "fade at once (%s): 8 notes held, the number drops to 3, then %s: %u fade out in the next block, %u sound 8 blocks later (the %u youngest kept), largest step %d vs %d before (%.2f x, limit 1.50 x)",
                         how ? "core 1 stops" : "the jobs pause", how ? "a wrong check value stops core 1" : "a very late block pauses the jobs", t.s.trims, t.nblk, t.busb_c1, a, calm,
                         calm ? (double)a / calm : 0.0);
                expect(what, WIFEXITED(st) && WEXITSTATUS(st) == 0 && t.hash == 1u && t.s.trims == 5u && t.nblk <= 3u && t.busb_c1 == 3u && a * 2 <= calm * 3 &&
                                 (how ? t.failed == TCR_BADJOB : t.s.pauses > 0));
            }
        }
    }
    printf("twocore: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
