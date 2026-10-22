/* SPDX-License-Identifier: GPL-3.0-only */
/* Job 5: HOST-THREAD SIMULATION of the two-core stage 1, written to the UNIT'S protocol (firmware/src/twocore.{h,c}: separate 32-bit words, a sequence number, core 1
 * decides who plays a job) with the real code on both sides. A model of the PROTOCOL, the DATA SEPARATION and the runner's decisions, not of the device's timing.
 *   core 1 (a pthread)        the real tc_c1_step loop (what hal/fm1_core1.h runs in RAM), with the computer hal (tests/twocore_hal_host.h: acquire / release atomics,
 *                             load and store hooks for the fault injection, a simulated clock, a start hook); its job is the real tc_job_run (twocore_run.c: part 2's
 *                             stages into part 2's own pscr / pacc / track), wrapped by sim_job (delays, the bus B check)
 *   core 0 (the main thread)  the firmware's own mix_block (fx.c) with DUELL_TWOCORE on: the real runner (tcr_begin: the deadline, tc_post; part 1; tcr_part2: the wait, the
 *                             take, the three cases, the pause), gather, sends, master. A half is 4 blocks of 32 (as audio.c calls it: tc_half_begin before the first);
 *                             the main-loop passes (tc_watch, tc_trim_poll) are called at the start of each half
 * The clock is simulated: it stands still unless a delay is running (then every 4th read of core 0 moves it a tick), so a healthy core 1 is never late because the machine was
 * busy; the harness sets it to the half's start at every half. A delay of core 1 is "until the clock passes T" (T from the runner's deadline of that job), so every late case happens
 * in the block it is aimed at.
 * Each run is one scene (the nine of partindep_scn.h) in a child process with ONE injected fault:
 *   none          the hand-over at full speed                                                       output = the old combined path (mixsplit_ref.h)
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
 * Built and run by DUELL/host_tests.sh (name: twocore) */
#define DUELL_TWOCORE 1
#define TC_AUTO_ARM 0                                   /* (1.0: every TWOCORE build arms itself; the 'unarmed' scenes of this simulation test the explicit arm (tc_arm): tests/twocore_bench_test.c, tcbench, tests the automatic one) */
#define TC_JOB_RUN sim_job
#include <stdint.h>
static void sim_job(uint32_t arg);
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
       F_UNARMED, F_NOANSWER, F_SHARED, F_BUSB, F_TCMCLR, F_VRUN, F_VWIN, F_FLOOR, F_FLOORX, F_NOFLOOR, F_VOFF, F_NOANS, F_VLONG, F_N };
#define F_TRIMSTOP 100                                  /* (the fade run that stops core 1: not one of the scene runs) */
static const char *const FNAME[F_N] = {"none", "torn1", "tornp", "late_pre", "late_post", "late_run", "badchk", "stale", "ahead", "badsnap", "hang_pre", "hang_post", "park",
                                       "park_stuck", "silent_late", "unarmed", "noanswer", "shared", "busb", "tcmclr", "vrun", "vwin", "floor", "floorx", "nofloor", "voff", "noans", "vlong"};

/* what a run reports (shared memory: the runs are child processes) */
typedef struct {
    tc_stat_t s;
    uint32_t parks, up, failed, strike, nv_last, reread, bad_w, misread, c1_jobs, busb_c1, busb_checks, nblk, park_us, park_ok;
    uint32_t cap_viol;                                  /* posted jobs whose deadline was later than core 0's own latest moment (TC_MEAS: the floor's ceiling) */
    uint32_t lim_over, end_over, lim_blocks, lim_max_over;   /* blocks with a posted job where core 0 left its block later than the wait limit (+8 ticks) / later than the half's end; blocks looked at; the largest overrun (ticks) */
    uint32_t own_checks, own_writes, own_timeouts;     /* the ownership audit (sim_job): jobs audited, jobs that changed part 1's state, gate timeouts */
    uint64_t hash;
    uint64_t bh[MAXB];                                  /* the hash of each block's output */
    uint8_t skip[MAXB];                                 /* part 2 was silent in this block (tcr.skip2) */
    uint64_t dl_sum[TCR_BLOCKS];                        /* the deadlines of the posted jobs, ticks after the half's start, summed per block of the half */
    uint32_t dl_n[TCR_BLOCKS];
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
static int pl_hang_pre(uint32_t j) { return fault == F_HANG_PRE && j == 150u; }
static int pl_hang_post(uint32_t j) { return (fault == F_HANG_POST || fault == F_PARK_STUCK) && j == 150u; }
static int pl_silent(uint32_t j) { return fault == F_SILENT_LATE && j == 150u; }

/* every load of a shared word, core 1's and core 0's (the hal's hook) */
static uint32_t ld_hook(const uint32_t *p, uint32_t v)
{
    if (tc_host_is_c1) {
        if (p == &tc_mb.job && v != c1_seen) {          /* core 1 looks at a new job: what happens first (delays before it looks, a hang, torn reads, a misread snapshot) */
            c1_seen = c1_job = v;
            if (pl_noans(v))
                wait_clock(tcr.dl + 2600u);             /* later than the 100 us (2,400 ticks) core 0 waits for the answer to its take, earlier than the wait limit: "no answer", then yld */
            else if (pl_late_pre(v))
                wait_clock(tcr.dl + 1000u);             /* late to wake: core 0 takes the job back before core 1 looks */
            if (pl_hang_pre(v))
                hang();                                 /* hung before it looks: it never answers (held at the end: its stores are dropped) */
            torn_left = pl_torn1(v) ? 1u : pl_tornp(v) ? 100u : 0u;
            if (fault == F_BADSNAP && v == 100u)
                *(volatile uint32_t *)&tc_ctx ^= 0x10u;   /* the snapshot as core 1 reads it: one bit wrong */
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
    if (p == &tc_mb.done) {
        if (fault == F_NONE)
            __atomic_store_n(&c0_wait, tc.job, __ATOMIC_RELEASE);   /* core 0 is in its wait for this job: part 1's stages are over, it touches no part state until done */
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


/* ------------------------------------------------------------ the ownership audit: part 2's job writes nothing of part 1 --- */
/* A shared write put back by mistake (the matrix window, the ENV SYNC steps, the block scratch) is a race in the threaded run: it shows only if both parts run that spot at
 * the same moment, so it is caught one run in a few. This makes it show every time: in the run without a fault core 1 waits until core 0 is in its wait for done (part 1's
 * stages are over and core 0 touches no part state until it reads done), hashes part 1's whole state (its track, its block scratch, its accumulators), runs the real job, and
 * hashes again. Part 2's job may write only part 2's own: any change is a write into part 1. No timing, no race. */
static uint64_t own_hash(void)
{
    uint64_t h = 0xCBF29CE484222325ull;
    const uint8_t *q;
    uint32_t i, k;
    const void *obj[3] = {&trk[0], &pscr[0], &pacc[0]};
    const uint32_t len[3] = {(uint32_t)sizeof trk[0], (uint32_t)sizeof pscr[0], (uint32_t)sizeof pacc[0]};
    for (k = 0; k < 3u; k++)
        for (q = (const uint8_t *)obj[k], i = 0; i < len[k]; i++)
            h = (h ^ q[i]) * 0x100000001B3ull;
    return h;
}
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
static void sim_job(uint32_t arg)
{
    uint32_t j = c1_job;
    uint64_t s0, s1;
    if (pl_vlong(j))
        wait_clock(tcr.lim + 60u);                      /* slower than core 0 may wait: it stops at the wait limit, part 2 silent for the block */
    else if (pl_late_post(j))
        wait_clock(tcr.dl + 1000u);                     /* slow in the middle: core 0 takes it back, then waits for done */
    if (is_floor() && __atomic_load_n(&sim_blk, __ATOMIC_ACQUIRE) == 0u) {   /* the floor runs: the job of a half's block 0 takes a fixed time, 2,500 ticks past the table's first deadline (floorx: 6,000 more) */
        uint32_t now1 = raw_ticks();
        if (!fl_d)
            fl_d = (uint32_t)(tcr.dl - now1) + 2500u + (fault == F_FLOORX ? 6000u : 0u);
        {   /* (not past the wait limit: a job slower than that is a silent block and a fault pause now, which is not what these runs are about; after the first half the deadline of block 0 can be earlier than at the first job) */
            uint32_t t_end = now1 + fl_d, lim_ok = tcr.lim - 100u;
            wait_clock((int32_t)(t_end - lim_ok) > 0 ? lim_ok : t_end);
        }
    }
    if (pl_silent(j))
        wait_release();                                 /* not done in the half: core 0 gives part 2 up for the block; it goes on at the next half */
    if (pl_hang_post(j)) {
        hang();                                         /* hung after its claim: nothing of part 2 is touched */
        return;
    }
    if (!c1_live())
        return;
    s0 = busb_sum();
    if (fault == F_NONE) {
        uint64_t a0 = 0;
        int gated = own_gate(j);
        if (gated)
            a0 = own_hash();
        else
            own_timeouts++;
        tc_job_run(arg);
        if (gated) {
            own_checks++;
            if (own_hash() != a0)
                own_writes++;
        }
    } else
        tc_job_run(arg);
    if (fault == F_SHARED && c1_jobs == 300u)
        trk[0].p[P_REV] = (int16_t)(trk[0].p[P_REV] + 9);   /* the deliberate shared write: part 2's stage touches part 1's parameter (its REVERB send) */
    if (fault == F_BUSB && c1_jobs == 300u)
        dly_buf[DLY_HALF + 3u] = (int16_t)(dly_buf[DLY_HALF + 3u] + 1);   /* a write into bus B's half of the delay */
    s1 = busb_sum();
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

/* the reference paths record their blocks too */
static void ref_block(int32_t *out, uint32_t n)
{
    ref_mix_block(out, n);
    record(out, 0);
}
/* the one-core staged path with part 2 skipped in the blocks of ref_skip (the hung core 1's silent blocks), part 2's voices faded out at block ref_fade */
static const uint8_t *ref_skip;
static uint32_t ref_fade;
static void skip_block(int32_t *out, uint32_t n)
{
    uint32_t i, b = nblk;
    int perf;
    blockstate_t bs;
    int32_t mg = MIX_MG();
    mix_events(n);
    sp_tap = snd_st != SND_OFF && (perf_space > 0 || sv.spc > 0);
    if (perf_idle()) {
        perf_tick(n);
        perf = 0;
    } else {
        perf = perf_begin(n);
    }
    blk_snap(&bs, sp_tap);
    for (i = 0; i < NLIVE; i++) {
        int mo;
        if (i == 1u && ref_skip[b]) {
            pacc_on[1] = 0;                             /* (a part that wrote nothing this block: what the skip in mix_gather must be equal to) */
            continue;
        }
        if (i == 1u && b == ref_fade) {
            uint32_t k;
            for (k = 0; k < NVOICE; k++)
                if (trk[1].v[k].active && trk[1].v[k].stage != 4u)
                    voice_kill(&trk[1].v[k]);
        }
        mo = morph_begin(&trk[i], 1);
        mix_part(&trk[i], n, &bs);
        if (mo)
            morph_end(&trk[i]);
    }
    mix_gather(n, &bs);
    mix_sends(n);
    if (meter_on) {
        meter_on--;
        if (dm_split)
            meter_scan(n);
    }
    if (scope_mix)
        scope_take_mix(n);
    mix_master(out, n, perf, mg);
    record(out, ref_skip[b]);
}

/* the main loop's passes and the audio interrupt's half start, then the firmware's own mix_block */
static uint32_t park_t0, park_t1, beat_seen, beat_ms;
static void sim_block(int32_t *out, uint32_t n)
{
    uint32_t b = nblk;
    if (b % TCR_BLOCKS == 0) {
        uint32_t h = b / TCR_BLOCKS, t = BASE_TICKS + h * TCR_HALF_TICKS + clk_jump;
        if (fault == F_VLONG && tcs.pauses > jumps && tcr.open) {   /* the late job is let go before the calm period passes (its clock figures stay those of one job) */
            uint32_t want = tcr.lim + 100u;
            if ((int32_t)(want - raw_ticks()) > 0)
                __atomic_store_n(&tc_host_ticks, want, __ATOMIC_RELAXED);
            while (c1_live() && !tc_closed())
                cpu_relax();
        }
        if ((fault == F_LATE_POST || fault == F_LATE_RUN || fault == F_LATE_PRE || fault == F_STALE || fault == F_VRUN || fault == F_VWIN || fault == F_NOANS || fault == F_VLONG) && tcs.pauses > jumps) {
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
            tc_flash_park();                            /* the flash driver: this half plays part 2 on core 0 */
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
    mix_block(out, n);
    if (tcr.job) {
        int32_t lo = (int32_t)(raw_ticks() - tcr.lim), eo = (int32_t)(raw_ticks() - (tcr.t0 + TCR_HALF_TICKS));   /* where core 0 is when it leaves the block's wait: never later than the limit, so never later than the half's end */
        R->lim_blocks++;
        R->lim_over += lo > 8;
        R->end_over += eo > 0;
        if (lo > (int32_t)R->lim_max_over)
            R->lim_max_over = (uint32_t)lo;
#if TC_MEAS
        R->cap_viol += (int32_t)(tcr.dl - tcr.cap) > 0;
#endif
        R->dl_sum[b % TCR_BLOCKS] += tcr.dl - tcr.t0;
        R->dl_n[b % TCR_BLOCKS]++;
        R->lim_sum[b % TCR_BLOCKS] += tcr.lim - tcr.t0;
    }
    record(out, tcr.skip2);
}

/* ------------------------------------------------------------ a run --- */
static uint64_t scene_hash(uint32_t id, blk_t blk)
{
    return scene(id, blk);
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
    R->nblk = nblk;
    R->park_us = (park_t1 - park_t0) / TC_TICKS_PER_US;
}
/* one run in a child process: kind -1 the old combined path, -2 the one-core path with the blocks of ref_skip skipped, else the injected fault; div the clock's divider */
static void child_run(uint32_t id, int kind)
{
    uint64_t h;
    if (kind == -1) {
        h = scene_hash(id, ref_block);
        fill_res(h);
        return;
    }
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
    bnotes[0] = bnotes[1] = 0;
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
            bnotes[1] = 3;                              /* the budget drops: nothing is cut yet */
        if (f == (uint32_t)(1.0 * FS) / CTL * CTL)
            trim_job = tc.job + 3u;                     /* the third job from now is the late one (or the one written wrong) */
        sim_block(o, CTL);
        for (i = 0; i < CTL; i++)
            L[f + i] = o[2 * i];
        if (!at && tcs.trims)
            at = f;
        if (at && f == at + 8u * CTL) {
            after = young = 0;
            for (i = 0; i < NVOICE; i++) {
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
    uint32_t s, s0 = 0, s1 = NSCENE, f;
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
        uint64_t hr;
        run(s, -1, &ref);
        hr = ref.hash;
        snprintf(what, sizeof what, "scene %u (%s) is not silent", s, SCN[s]);
        expect(what, hr != 0);
        printf("twocore: scene %u (%s): reference %016llx, %u blocks\n", s, SCN[s], (unsigned long long)hr, ref.nblk);
        for (f = 0; f < F_N; f++) {
            int equal_ref = f != F_HANG_PRE && f != F_HANG_POST && f != F_PARK_STUCK && f != F_SILENT_LATE && f != F_SHARED && f != F_BUSB && f != F_VLONG;
            const tc_stat_t *c;
            uint32_t sum;
            if (audit && f != F_NONE)
                continue;
            if (f == F_SHARED && (nomut || s != 0))
                continue;
            if (f == F_BUSB && nomut)
                continue;
            if ((f == F_TCMCLR || f == F_FLOOR || f == F_FLOORX || f == F_NOFLOOR || f == F_VRUN || f == F_VWIN) && !TC_MEAS)
                continue;
            if (tsan && (f == F_HANG_PRE || f == F_HANG_POST || f == F_PARK_STUCK || f == F_SILENT_LATE))
                continue;                               /* (a window in which core 0 runs the sends while core 1 still holds part 2 is not race free: docs "Job 5") */
            run(s, (int)f, &r);
            c = &r.s;
            printf("twocore:   %-11s blocks %4u in time %4u late %3u very late %3u silent %3u (waiting %3u) own %4u refused %u no answer %u | pauses %u (fault %u run %u window %u late %u) very late without pause %u resumes %u | rereads %u parks %u | "
                   "core 1 jobs %4u bus B writes %u of %u | up %u strike %u record %u failed %u bad %u | %016llx\n",
                   FNAME[f], c->blocks, c->in_time, c->late, c->vlate, c->silent, c->wait, c->own, c->refused, c->noanswer, c->pauses, c->pause_fault, c->pause_run, c->pause_win, c->pause_late,
                   c->vlate_ok, c->resumes, r.reread, r.parks, r.c1_jobs,
                   r.busb_c1, r.busb_checks, r.up, r.strike, r.nv_last, r.failed, r.bad_w, (unsigned long long)r.hash);
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
                snprintf(what, sizeof what, "scene %u %s: the output equals the old combined path bit for bit", s, FNAME[f]);
                expect(what, r.hash == hr);
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
                snprintf(what, sizeof what, "scene %u %s: no deadline was later than core 0's own latest moment to play part 2 (%u posted jobs over it)", s, FNAME[f], r.cap_viol);
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
                    snprintf(what, sizeof what, "scene %u late_run: the pause shows as 'paused' own blocks (%u)", s, m->own[TCM_OWN_PAUSE]);
                    expect(what, m->own[TCM_OWN_PAUSE] > 0);
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
                snprintf(what, sizeof what, "scene %u none: core 1 played part 2 in time in most blocks (%u of %u)", s, c->in_time, c->blocks);
                expect(what, c->in_time > c->blocks / 2u && r.c1_jobs >= c->in_time);
                snprintf(what, sizeof what, "scene %u none: core 1 wrote no bus B state in stage 1 (%u of %u jobs)", s, r.busb_c1, r.busb_checks);
                expect(what, r.busb_c1 == 0 && r.busb_checks >= c->in_time);
                snprintf(what, sizeof what, "scene %u none: core 1's job changed nothing of part 1's state (track, block scratch, accumulators): %u of %u jobs wrote, %u gate timeouts", s, r.own_writes, r.own_checks, r.own_timeouts);
                expect(what, r.own_writes == 0 && r.own_timeouts == 0 && r.own_checks >= c->in_time);
                break;
            case F_TORN1:
                snprintf(what, sizeof what, "scene %u torn1: re-read and run, counted (%u), no refusal", s, r.reread);
                expect(what, r.reread > 0 && c->refused == 0 && !r.strike && r.bad_w == 0);
                break;
            case F_TORNP:
                snprintf(what, sizeof what, "scene %u tornp: refused, a strike (BADJOB), core 1 held, core 0 plays part 2 from then on", s);
                expect(what, c->refused == 1 && r.failed == TCR_BADJOB && r.strike && !r.up && c->own > 0 && r.nv_last == TCR_BADJOB);
                break;
            case F_LATE_PRE:
                snprintf(what, sizeof what, "scene %u late_pre: the late path (take, yld) was taken and counted (%u), no strike", s, c->late);
                expect(what, c->late > 0 && !r.strike && r.up && c->refused == 0 && c->vlate == 0);
                break;
            case F_LATE_POST:
                snprintf(what, sizeof what, "scene %u late_post: single very late blocks that finished in their half are counted (%u) and pause nothing (pauses %u, without a pause %u), no silent block, no strike", s, c->vlate, c->pauses, c->vlate_ok);
                expect(what, c->vlate > 0 && c->silent == 0 && c->pauses == 0 && c->vlate_ok == c->vlate && !r.strike && r.up);
                break;
            case F_VOFF:
                snprintf(what, sizeof what, "scene %u voff: with the BENCHT4 defaults %u very late blocks (4 in a row, 8 within the window among them) paused nothing (pauses %u, without a pause %u), no silent block, no strike", s, c->vlate, c->pauses, c->vlate_ok);
                expect(what, c->vlate >= 24u && c->pauses == 0 && c->vlate_ok == c->vlate && c->silent == 0 && !r.strike && r.up);
                break;
            case F_NOANS:
                snprintf(what, sizeof what, "scene %u noans: a core 1 that answers the take later than 100 us is a real fault: no answer %u, then yld, a fault pause (%u of %u pauses), core 0 played part 2 (output equal), no strike, nothing counted as without a pause (%u)", s, c->noanswer, c->pause_fault, c->pauses, c->vlate_ok);
                expect(what, c->noanswer > 0 && c->pauses > 0 && c->pause_fault == c->pauses && c->vlate_ok == 0 && c->silent == 0 && !r.strike && r.up);
                break;
            case F_VLONG:
                snprintf(what, sizeof what, "scene %u vlong: a job that is not done by the wait limit makes part 2 silent for the block (silent %u) and pauses the jobs as a fault (%u of %u pauses); core 0 left every wait at the limit (largest overrun %u ticks, %u of %u blocks over), none past the half's end (%u); no strike", s, c->silent, c->pause_fault, c->pauses, r.lim_max_over, r.lim_over, r.lim_blocks, r.end_over);
                expect(what, c->silent > 0 && c->pauses > 0 && c->pause_fault == c->pauses && c->vlate_ok == 0 && r.lim_over == 0 && r.end_over == 0 && !r.strike);
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
                snprintf(what, sizeof what, "scene %u late_run: a run of late blocks paused the jobs (%u) and they resumed (%u), no strike", s, c->pauses, c->resumes);
                expect(what, c->late >= 4u && c->pauses > 0 && c->resumes + 1u >= c->pauses && !r.strike && r.up);
                break;
            case F_BADCHK:
                snprintf(what, sizeof what, "scene %u badchk: refused, a strike (BADJOB), core 0 plays part 2", s);
                expect(what, c->refused == 1 && r.failed == TCR_BADJOB && r.strike && !r.up);
                break;
            case F_STALE:
                snprintf(what, sizeof what, "scene %u stale: a stale done word took the deadline path (very late %u), done after, no strike", s, c->vlate);
                expect(what, c->vlate > 0 && c->refused == 0 && !r.strike && r.up);
                break;
            case F_AHEAD:
                snprintf(what, sizeof what, "scene %u ahead: a done word ahead is BADWORD, a strike, core 1 held", s);
                expect(what, r.failed == TCR_BADWORD && r.strike && !r.up && c->refused == 1);
                break;
            case F_BADSNAP:
                snprintf(what, sizeof what, "scene %u badsnap: the snapshot that fits no hash is refused, a strike (BADJOB), core 0 plays part 2", s);
                expect(what, c->refused == 1 && r.failed == TCR_BADJOB && r.strike && !r.up);   /* (core 0 may see bad_job before core 1 wrote bad: it is held at once) */
                break;
            case F_HANG_PRE:
            case F_HANG_POST:
            case F_PARK_STUCK: {
                uint32_t first = 0, last = 0, nsk = 0, i, pre_ok = 1;
                static uint8_t sk[MAXB];
                res_t *ps = &rs;
                for (i = 0; i < r.nblk && i < MAXB; i++)
                    if (r.skip[i]) {
                        if (!nsk)
                            first = i;
                        last = i;
                        nsk++;
                    }
                snprintf(what, sizeof what, "scene %u %s: core 1 stopped (%s), a strike, part 2 silent in %u blocks (%u counted), then core 0 played it", s, FNAME[f],
                         f == F_PARK_STUCK ? "FLASH" : "HANG", nsk, c->silent);
                expect(what, nsk > 0 && nsk == c->silent && r.failed == (f == F_PARK_STUCK ? TCR_FLASH : TCR_HANG) && r.strike && !r.up && last + 1u < r.nblk && c->own > 0 &&
                                 nsk == last - first + 1u);
                for (i = 0; i < first; i++)
                    pre_ok &= r.bh[i] == ref.bh[i];
                snprintf(what, sizeof what, "scene %u %s: the output equals the reference up to the hang (block %u)", s, FNAME[f], first);
                expect(what, pre_ok);
                memcpy(sk, r.skip, sizeof sk);
                ref_skip = sk;
                ref_fade = f == F_HANG_PRE ? 0xFFFFFFFFu : last + 1u;
                run(s, -2, ps);
                snprintf(what, sizeof what, "scene %u %s: the whole output equals the one-core path with exactly those blocks silent%s", s, FNAME[f], f == F_HANG_PRE ? "" : " (and the fade)");
                expect(what, ps->hash == r.hash);
                if (f == F_PARK_STUCK) {
                    snprintf(what, sizeof what, "scene %u park_stuck: FLASH after 20 ms of simulated time (%u us)", s, r.park_us);
                    expect(what, r.park_us >= 20000u && r.park_us < 20100u);
                } else {
                    snprintf(what, sizeof what, "scene %u %s: the stop came from the 100 ms heartbeat watch (noanswer %u)", s, FNAME[f], c->noanswer);
                    expect(what, f == F_HANG_PRE ? c->noanswer > 0 : 1);
                }
                break;
            }
            case F_PARK:
                snprintf(what, sizeof what, "scene %u park: a flash park in the middle of play, counted (%u), part 2 played by core 0 meanwhile (own %u), core 1 still up", s, r.parks, c->own);
                expect(what, r.parks == 1u && c->own >= 3u && r.up && !r.strike);
                break;
            case F_SILENT_LATE:
                snprintf(what, sizeof what, "scene %u silent_late: part 2 silent in %u blocks, the jobs paused (%u) and resumed (%u), no strike", s, c->silent, c->pauses, c->resumes);
                expect(what, c->vlate > 0 && c->silent > 0 && c->pauses > 0 && c->pause_fault == c->pauses && !r.strike);
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
                snprintf(what, sizeof what, "self-check: core 1 writing part 1's REVERB send changes the output (the test sees a shared write)");
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
    if (!tsan && !audit) {
        /* the fallback fade (item 11) */
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
