/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL: the cost of a CHANGE, not of the steady state. On the real FM-1 the INFO/TEST meter counted a few missed blocks at the
 * moment a patch or a multi was loaded although the steady load was fine: one half buffer (128 frames, 4 mix blocks of CTL) took
 * longer than its 2.9 ms because a change did a lump of work in the audio ISR (a 16 KB delay slice and the buses' lines cleared byte
 * by byte, coefficients designed with 64-bit shift-and-subtract divides ...) or kept the interrupts off (a project load: 74 k
 * instructions). This test applies each kind of change on top of a steady render of M15 TEST 3 HEAVY (FM6 + PHASE, SPLIT, SHIM,
 * PING-PONG, FLANGER inserts, EQ, ducker; a chord of 4 notes on each part, pressed again every 24 halves) and counts the host
 * instructions (proc_pid_rusage, as mode_test.c) of every half (4 blocks) afterwards.
 *   extra of an operation = its worst half - the busiest half of the steady state before / after it (the lump, not the new sound's price)
 *   half = the worst half as a whole, steady load included;  irq-off = the longest interrupts-off window of the main loop (the audio
 *   interrupt waits for it, so it adds to the half it falls in); all as % of HALF_BUDGET (420 k instructions: 33 host instructions
 *   a sample = 1 % of the device's half, audio.c). The scene's steady load is ~65-75 % (the owner's meter: ~60 %).
 *   The host's memset / memcpy are vectorised; here they are the device's (memfast.h, libc.c) so a 16 KB clear costs what it costs there.
 *   Build: -O1 -fno-builtin (host_tests.sh does).
 * Default run (host_tests.sh "loadcost"): memset / memcpy and 64-bit division checked against the plain versions, then guards on a SHIM
 * reverb switch, the M15 load and the mode changes (each: extra, half and irq-off under a limit). LOADCOST_FULL=1 prints the whole
 * table (all sounds of all 12 engines, M03..M06 (M01..M04 before 0.2.13) M13..M16, 5 reverbs, 8 inserts, EQ, ducker, delay / chorus, morph, modes);
 * `loadcost_test trace mode 0 1 | multi 13 14 | roll 1 | rev 2 3 | patch 9 2` prints every block of one case.
 * Needs macOS (proc_pid_rusage); elsewhere it reports and passes.
 *   build/host/loadcost_test         (DUELL/host_tests.sh "loadcost") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1                 /* morph.c: MORPH_LOCK counts as an interrupts-off window too */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
/* the device's memset / memcpy are firmware/src/memfast.h's loops (libc.c); the host's are vectorised and would count a 16 KB clear as
 * a handful of instructions. These replace them in this executable (build with -fno-builtin -O1: no loop idioms either), so the lumps
 * are counted as the device runs them (set LOADCOST_BYTELOOP for the old byte loops, to see what they cost) */
#include "../firmware/src/memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);                  /* (ui_test.c defines them: the hook counts the windows; morph.c needs them early) */
static void fm1_irq_on(void);
#include "ui_test.c"

static uint8_t nor[0x100000];
static long cut_bytes = -1;
static int dead, watch;
static uint32_t stray;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

#define HALF_BUDGET 420000.0            /* the device's half in host instructions (audio.c calibration) */

static int bad, full;
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static uint64_t overhead;                               /* the two proc_pid_rusage calls around a block */
static int trace;
static int irq_depth;
static uint64_t irq_t0, irq_max;                        /* the longest interrupts-off window seen (instructions) */
static void irq_hook(int on)
{
    if (!on) {
        if (!irq_depth++)
            irq_t0 = instr_now();
    } else if (irq_depth && !--irq_depth) {
        uint64_t w = instr_now() - irq_t0 - overhead;
        if (w > irq_max)
            irq_max = w;
        if (trace && w > 2000)
            printf("   irq-off window: %llu instr\n", (unsigned long long)w);
    }
}
static int32_t ob[2 * CTL];

static uint64_t block_cost(void)
{
    uint64_t i0 = instr_now();
    mix_block(ob, CTL);
    return instr_now() - i0 - overhead;
}
static uint64_t half_run(uint64_t *blk)                 /* one half: its blocks (each into blk[]), the sum */
{
    uint32_t b;
    uint64_t s = 0;
    for (b = 0; b < HALF_FRAMES / CTL; b++) {
        uint64_t c = block_cost();
        if (blk)
            blk[b] = c;
        s += c;
    }
    return s;
}
static uint64_t median(uint64_t *v, uint32_t n)
{
    uint64_t t[64];
    uint32_t i, j;
    for (i = 0; i < n; i++)
        t[i] = v[i];
    for (i = 1; i < n; i++)
        for (j = i; j && t[j - 1] > t[j]; j--) {
            uint64_t x = t[j];
            t[j] = t[j - 1], t[j - 1] = x;
        }
    return t[n / 2u];
}

static void power_cycle(void)
{
    uint32_t k;
    memset(nor, 0xFF, sizeof nor);
    dead = 0;
    cut_bytes = -1;
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
}

static const uint8_t NTA[4] = {48, 55, 60, 64}, NTB[4] = {67, 71, 74, 79};
static void notes_on(uint32_t p)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        trk_note_on(&trk[p], p ? NTB[i] : NTA[i], 110);
}
static void notes_off(uint32_t p)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        trk_note_off(&trk[p], p ? NTB[i] : NTA[i]);
}

/* one operation: what it does (op), and whether the player then presses the keys of part 1 (notes: after 1 half, again after 12) */
typedef struct {
    const char *name;
    uint32_t base;                                      /* the multi the steady state is (0-based slot) */
    void (*pre)(uint32_t a);                            /* before the steady state is measured (a prior state) */
    uint32_t a;
    void (*op)(uint32_t b);
    uint32_t b;
    uint32_t notes;                                     /* 1: chord on part 1 and 2 after the change, and again later */
} case_t;
typedef struct {
    double extra_half, extra_blk, st_old, st_new, irq, op_main, peak;
    uint32_t at_half;
} res_t;

#define NH_EVT 36u                      /* halves after the change that count as its moment */
#define NH_TAIL 48u                     /* the halves after that: the new steady state (two note cycles) */
#define NH_CYCLE 24u                    /* the player presses a chord on part 1 at cycle 1, on part 2 at cycle 13 (the voices decay) */
static void play_tick(uint32_t h)
{
    uint32_t c = h % NH_CYCLE;
    if (c == 1u)
        notes_off(0), notes_on(0);
    if (c == 13u)
        notes_off(1), notes_on(1);
}
static res_t run_case(const case_t *c)
{
    uint64_t hv[NH_EVT + NH_TAIL], bv[4], pre[48], blk_pre = 0, blk_evt = 0, peak_pre = 0, peak_tail = 0, peak_evt = 0, blk_tail = 0;
    uint32_t i, at = 0, nb = HALF_FRAMES / CTL;
    res_t r;
    power_cycle();
    project_load(c->base);
    song.sel = 0;
    if (c->pre)
        c->pre(c->a);
    for (i = 0; i < 168u; i++) {                        /* settle: the mode's dip, the morph ramp, the tails; the chords pressed */
        play_tick(i);
        half_run(0);
    }
    for (i = 0; i < 48u; i++) {                         /* the steady state before */
        uint32_t b;
        play_tick(i);
        pre[i] = half_run(bv);
        peak_pre = pre[i] > peak_pre ? pre[i] : peak_pre;
        for (b = 0; b < nb; b++)
            blk_pre = bv[b] > blk_pre ? bv[b] : blk_pre;
    }
    {
        uint64_t m0;
        irq_max = 0;
        host_irq_hook = irq_hook;
        m0 = instr_now();
        c->op(c->b);
        r.op_main = (double)(instr_now() - m0);
        host_irq_hook = 0;
        r.irq = (double)irq_max;
    }
    for (i = 0; i < NH_EVT + NH_TAIL; i++) {
        uint32_t b;
        play_tick(i);
        hv[i] = half_run(bv);
        if (trace && i < NH_EVT)
            printf("   h%02u  %6llu %6llu %6llu %6llu  = %6llu (%.0f %%)\n", i, (unsigned long long)bv[0], (unsigned long long)bv[1],
                   (unsigned long long)bv[2], (unsigned long long)bv[3], (unsigned long long)hv[i], hv[i] * 100.0 / HALF_BUDGET);
        if (i < NH_EVT) {
            for (b = 0; b < nb; b++)
                blk_evt = bv[b] > blk_evt ? bv[b] : blk_evt;
            if (hv[i] > peak_evt)
                peak_evt = hv[i], at = i;
        } else {
            for (b = 0; b < nb; b++)
                blk_tail = bv[b] > blk_tail ? bv[b] : blk_tail;
            peak_tail = hv[i] > peak_tail ? hv[i] : peak_tail;
        }
    }
    r.st_old = (double)median(pre, 48);
    r.st_new = (double)median(hv + NH_EVT, NH_TAIL);
    {
        double ref = (double)(peak_pre > peak_tail ? peak_pre : peak_tail);   /* the busiest half of the steady state, before or after */
        double bref = (double)(blk_pre > blk_tail ? blk_pre : blk_tail);
        r.peak = (double)peak_evt;
        r.extra_half = (double)peak_evt - ref;
        r.extra_blk = (double)blk_evt - bref;
        if (r.extra_half < 0)
            r.extra_half = 0;
        if (r.extra_blk < 0)
            r.extra_blk = 0;
    }
    r.at_half = at;
    return r;
}

static void print_res(const char *name, const char *tag, const res_t *r)
{
    printf("loadcost: %-50s %-7s +%7.0f = %5.1f %% | peak %5.1f %% | blk +%6.0f | irq-off %6.0f = %4.1f %% | steady %3.0f -> %3.0f %% | h%u\n",
           name, tag, r->extra_half, r->extra_half * 100.0 / HALF_BUDGET, r->peak * 100.0 / HALF_BUDGET, r->extra_blk, r->irq,
           r->irq * 100.0 / HALF_BUDGET, r->st_old * 100.0 / HALF_BUDGET, r->st_new * 100.0 / HALF_BUDGET, r->at_half);
}
static void show(const case_t *c, const char *tag)
{
    res_t r = run_case(c);
    print_res(c->name, tag, &r);
}
/* one guard: the worst half's extra (% of the half budget) under lim, the whole half (the steady load of the scene included) under
 * peak_lim, the longest interrupts-off window (the audio interrupt waits for it) under irq_lim */
static void check_case(const char *what, const case_t *c, double lim, double peak_lim, double irq_lim)
{
    res_t r = run_case(c);
    double pct = r.extra_half * 100.0 / HALF_BUDGET, pk = r.peak * 100.0 / HALF_BUDGET, iw = r.irq * 100.0 / HALF_BUDGET;
    char b[200];
    int ok = pct < lim && pk < peak_lim && iw < irq_lim;
    snprintf(b, sizeof b, "%s: extra +%.1f %% (< %.0f), half %.0f %% (< %.0f), irq-off %.1f %% (< %.0f)", what, pct, lim, pk, peak_lim, iw,
             irq_lim);
    printf("loadcost: %-120s %s\n", b, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* the word-at-a-time memset / memcpy (memfast.h) against byte loops: every alignment, lengths 0 .. 70, guard bytes round the target */
static void t_mem(void)
{
    static uint8_t a[160], b[160], ref[160];
    uint32_t off_d, off_s, n, i, ok = 1;
    for (off_d = 0; off_d < 8u; off_d++)
        for (n = 0; n < 71u; n++) {
            for (i = 0; i < sizeof a; i++)
                a[i] = ref[i] = (uint8_t)(0xA0u + i);
            mf_memset(a + 16 + off_d, 0x5C, n);
            for (i = 0; i < n; i++)
                ref[16 + off_d + i] = 0x5C;
            ok &= !memcmp(a, ref, sizeof a);
        }
    for (off_d = 0; off_d < 8u; off_d++)
        for (off_s = 0; off_s < 8u; off_s++)
            for (n = 0; n < 71u; n++) {
                for (i = 0; i < sizeof a; i++)
                    a[i] = ref[i] = (uint8_t)(0xA0u + i), b[i] = (uint8_t)(i * 7u + 3u);
                mf_memcpy(a + 16 + off_d, b + 16 + off_s, n);
                for (i = 0; i < n; i++)
                    ref[16 + off_d + i] = b[16 + off_s + i];
                ok &= !memcmp(a, ref, sizeof a);
            }
    printf("loadcost: %-120s %s\n", "memset / memcpy (memfast.h): every alignment pair and length 0..70 equals the byte loops", ok ? "ok" : "FAIL");
    bad += !ok;
}

/* __udivdi3 / __divdi3 (libc.c, the EQ coefficients) against the compiler's 64-bit division: random operands of every bit length */
static uint64_t xr = 0x9E3779B97F4A7C15ull;
static uint64_t xrand(void)
{
    xr ^= xr << 13;
    xr ^= xr >> 7;
    xr ^= xr << 17;
    return xr;
}
static void t_div(void)
{
    uint64_t i, n, d, ok = 1, cnt = 0;
    static const uint64_t E[] = {0, 1, 2, 3, 0xFFFFull, 0x10000ull, 0xFFFFFFFFull, 0x100000000ull, 0x100000001ull, 0x7FFFFFFFFFFFFFFFull,
                                 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0x480000000ull, 0x10000000ull, 0xFFFFFFFF00000000ull};
    for (n = 0; n < sizeof E / sizeof E[0]; n++)
        for (d = 1; d < sizeof E / sizeof E[0]; d++) {
            ok &= __udivdi3(E[n], E[d]) == E[n] / E[d];
            cnt++;
        }
    for (i = 0; i < 1500000u; i++) {
        n = xrand() >> (xrand() & 63u);
        d = (xrand() >> (xrand() & 63u)) | ((i & 7u) == 0u ? 1u : 0u);
        if (!d)
            continue;
        ok &= __udivdi3(n, d) == n / d;
        ok &= __divdi3((long long)n, (long long)d) == (long long)n / (long long)d;
        ok &= __divdi3(-(long long)(n >> 1), (long long)(d >> 1 | 1u)) == -(long long)(n >> 1) / (long long)(d >> 1 | 1u);
        cnt++;
    }
    {
        char b[160];
        snprintf(b, sizeof b, "__udivdi3 / __divdi3 (libc.c) equal the compiler's 64-bit division on %llu operand pairs", (unsigned long long)cnt);
        printf("loadcost: %-120s %s\n", b, ok ? "ok" : "FAIL");
        bad += !ok;
    }
}

/* ------------------------------------------------------------------ the operations --- */
static uint32_t eng_named(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static void load_patch(track_t *t, uint32_t e, uint32_t k)   /* set_engine_of, then sound k (ui.c) */
{
    uint32_t i;
    t->eng_req = (uint8_t)e;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = ENGINES[e]->edit[i].def;
    apply_preset_to(t, k);
}
static void op_patch(uint32_t a)                        /* part 1 loads sound (a & 255) of engine (a >> 8) */
{
    song.sel = 0;
    load_patch(&trk[0], a >> 8, a & 255u);
}
static void op_engine(uint32_t a) { song.sel = 0; set_engine_of(&trk[0], a); }
static void op_multi(uint32_t a) { project_load(a); }
static void op_rtype(uint32_t a) { song.g[G_RTYPE] = (int16_t)a; }
static void pre_rtype(uint32_t a) { song.g[G_RTYPE] = (int16_t)a; }
static void op_itype(uint32_t a)                        /* a: type | 16 both parts */
{
    trk[0].p[P_ITYPE] = (int16_t)(a & 15u);
    if (a & 16u)
        trk[1].p[P_ITYPE] = (int16_t)(a & 15u);
}
static void pre_ioff(uint32_t a) { (void)a; trk[0].p[P_ITYPE] = trk[1].p[P_ITYPE] = 0; }
static void pre_eqoff(uint32_t a)
{
    uint32_t p;
    (void)a;
    for (p = 0; p < 2u; p++)
        trk[p].p[P_EQLG] = trk[p].p[P_EQMG] = trk[p].p[P_EQHG] = 0;
}
static void op_eq(uint32_t a)                           /* a bit 0 part 1, bit 1 part 2; bit 2: one band only (LOW gain) */
{
    uint32_t p;
    for (p = 0; p < 2u; p++)
        if ((a >> p) & 1u) {
            if (a & 4u) {
                trk[p].p[P_EQLG] = (int16_t)(trk[p].p[P_EQLG] + 3);
            } else {
                trk[p].p[P_EQLG] = 5, trk[p].p[P_EQLF] = -4;
                trk[p].p[P_EQMG] = -4, trk[p].p[P_EQMF] = 5, trk[p].p[P_EQMQ] = 3;
                trk[p].p[P_EQHG] = 6, trk[p].p[P_EQHF] = 2;
            }
        }
}
static void op_duck(uint32_t a)
{
    uint32_t p;
    for (p = 0; p < 2u; p++) {
        trk[p].p[P_DKDEP] = (int16_t)(a ? 90 : 0);
        trk[p].p[P_DKATK] = (int16_t)(a ? 20 : 0);
        trk[p].p[P_DKRLS] = (int16_t)(a ? 70 : 0);
    }
}
static void op_global(uint32_t a)                       /* a: which global jumps */
{
    switch (a) {
    case 0: song.g[G_DTIME] = (int16_t)(song.g[G_DTIME] ^ 5); break;
    case 1: song.g[G_DFDBK] = 20; break;
    case 2: song.g[G_CRATE] = 100; song.g[G_CDEPTH] = 10; break;
    case 3: song.g[G_DMODE] = (int16_t)(song.g[G_DMODE] == DMD_PING ? DMD_DIGITAL : DMD_PING); break;
    case 4: song.g[G_RSIZE] = 20; song.g[G_RDAMP] = 100; break;
    default: song.g[G_BPM] = 90; break;
    }
}
static void op_roll(uint32_t a)                         /* a 0: ROLL B; 1: ROLL A (at A: heard); */
{
    morph_roll(a);
}
static void pre_morph(uint32_t a)
{
    (void)a;
    morph_set_target(1);
    morph_roll(1);
    morph_set_amount(100);
}
static void pre_morph_sel(uint32_t a)
{
    (void)a;
    morph_set_target(0);
    morph_roll(1);
    morph_set_amount(100);
}
static void op_fader(uint32_t a)
{
    morph_set_fader(a);
}
static void op_mode(uint32_t a) { duell_mode_req = (uint8_t)a; }
static void pre_mode(uint32_t a) { duell_mode_force(a); }
/* DUELL 0.2.14 GLO > MODULES: a switch (a: MODS_* bit, | 0x100 for OFF); pre: that module OFF before the change */
static void op_mods(uint32_t a) { mods_put(a & 0xFFu, (a >> 8) & 1u); }
static void pre_mods(uint32_t a) { mods_put(a, 1u); }

/* ------------------------------------------------------------------ the table --- */
static const char *const ENGN[12] = {"ANALOG", "FM6", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "WHEEL", "GRAIN", "PHYS", "NOISE", "SLICE"};
static const char *const RN[6] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};
static const char *const IN[9] = {"OFF", "SOFT", "HARD", "FOLD", "FUZZ", "CRUSH", "PHASER", "FLANGER", "CHORUS"};
static char cname[112];

static void guards(void)
{
    case_t c;
    t_mem();
    t_div();
    c = (case_t){"", 14, pre_rtype, 0, op_rtype, 3, 0};      /* SHIM switched to from ROOM (the M15 scene: SPLIT, both buses) */
    check_case("SHIM reverb switch in M15 (ROOM -> SHIM)", &c, 8, 100, 1);
    c = (case_t){"", 14, pre_rtype, 2, op_rtype, 3, 0};
    check_case("SHIM reverb switch in M15 (HALL -> SHIM)", &c, 8, 100, 1);
    c = (case_t){"", 13, 0, 0, op_multi, 14, 1};
    check_case("M15 TEST 3 HEAVY load over M14", &c, 10, 90, 10);
    c = (case_t){"", 12, 0, 0, op_multi, 14, 1};
    check_case("M15 TEST 3 HEAVY load over M13", &c, 10, 95, 10);
    c = (case_t){"", 14, pre_mode, DM_SPLIT, op_mode, DM_STEREO, 0};
    check_case("mode change SPLIT -> STEREO in M15", &c, 10, 90, 1);
    c = (case_t){"", 14, pre_mode, DM_STEREO, op_mode, DM_SPLIT, 0};
    check_case("mode change STEREO -> SPLIT in M15", &c, 10, 90, 1);
    c = (case_t){"", 14, pre_mode, DM_LAYER, op_mode, DM_SPLIT, 0};
    check_case("mode change LAYER -> SPLIT in M15", &c, 10, 90, 1);
    c = (case_t){"", 14, 0, 0, op_mods, MODS_SENDS | 0x100u, 0};   /* (0.2.14: the fade, then the lines cleared a chunk a block, as the dip) */
    check_case("MODULES SENDS OFF in M15", &c, 10, 90, 1);
    c = (case_t){"", 14, pre_mods, MODS_SENDS, op_mods, MODS_SENDS, 0};
    check_case("MODULES SENDS ON again in M15", &c, 10, 90, 1);
    c = (case_t){"", 14, 0, 0, op_mods, MODS_PFX | 0x100u, 0};
    check_case("MODULES PART FX OFF in M15", &c, 10, 90, 1);
    c = (case_t){"", 14, pre_mods, MODS_PFX, op_mods, MODS_PFX, 0};
    check_case("MODULES PART FX ON again in M15", &c, 10, 90, 1);
}

/* what eq_design's 64-bit divisions cost: the old shift-and-subtract __udivdi3 (kept here for the comparison) against libc.c's, on the
 * operands the design divides (a numerator of ~2^58, a divisor of ~2^28..2^31). On the device eq_design calls the library 7 times a band
 * (the disassembly: 2 __udivdi3, 5 __divdi3), 21 times when one value of a part's EQ moves (all three bands are redesigned), 42 when
 * both parts' do; a MORPH fader sweep with WILD on moves them every block */
static unsigned long long old_udivdi3(unsigned long long n, unsigned long long d)
{
    unsigned long long q = 0, r = 0;
    int i;
    if (!d)
        return 0;
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d)
            r -= d, q |= 1ull << i;
    }
    return q;
}
static void divcost(void)
{
    volatile unsigned long long sink = 0;
    uint64_t t0, t1, t2, i;
    const uint32_t N = 20000;
    t0 = instr_now();
    for (i = 0; i < N; i++)
        sink += old_udivdi3(((uint64_t)(i * 2654435761u) << 26) | 12345u, 0x30000000ull + (i * 40503u & 0xFFFFFFFu));
    t1 = instr_now();
    for (i = 0; i < N; i++)
        sink += __udivdi3(((uint64_t)(i * 2654435761u) << 26) | 12345u, 0x30000000ull + (i * 40503u & 0xFFFFFFFu));
    t2 = instr_now();
    (void)sink;
    printf("loadcost: 64-bit divide as eq_design uses it: old %llu instr, now %llu instr a divide (host count, loop overhead included); "
           "21 per part: %llu -> %llu, 42 for both parts: %.1f %% -> %.1f %% of the half\n",
           (unsigned long long)((t1 - t0 - overhead) / N), (unsigned long long)((t2 - t1 - overhead) / N),
           (unsigned long long)(21 * (t1 - t0 - overhead) / N), (unsigned long long)(21 * (t2 - t1 - overhead) / N),
           42.0 * (double)(t1 - t0 - overhead) / N * 100.0 / HALF_BUDGET, 42.0 * (double)(t2 - t1 - overhead) / N * 100.0 / HALF_BUDGET);
}

static void table(void)
{
    divcost();
    static const uint8_t MS[8] = {2, 3, 4, 5, 12, 13, 14, 15};   /* (the four emulator-made factory multis: M03..M06 since 0.2.13, M01..M04 before) */
    uint32_t e, k, from;
    printf("loadcost: steady scene = M15 TEST 3 HEAVY (SPLIT) with 4 + 4 notes held; extra = worst half (4 blocks) minus the steady half, "
           "of %.0f instr\n", HALF_BUDGET);
    for (e = 0; e < 12u; e++) {                          /* patch loads: every sound of the engine over M15's FM6 / PHASE */
        uint32_t ei = eng_named(ENGN[e]), n = ds_count(ei), wk = 0;
        res_t w = {0};
        char tag[24];
        w.extra_half = -1;
        for (k = 0; k < (n ? n : 1u); k++) {
            case_t c = {"patch", 14, 0, 0, op_patch, ei << 8 | k, 1};
            res_t r = run_case(&c);
            if (r.extra_half > w.extra_half)
                w = r, wk = k;
        }
        snprintf(tag, sizeof tag, "(of %u)", n);
        snprintf(cname, sizeof cname, "patch load part 1 -> %s #%u %s", ENGN[e], wk, ds_name(ei, wk));
        print_res(cname, tag, &w);
    }
    for (e = 0; e < 12u; e++) {
        case_t c = {0, 14, 0, 0, op_engine, eng_named(ENGN[e]), 1};
        snprintf(cname, sizeof cname, "ENGINE knob (set_engine_of) -> %s", ENGN[e]);
        c.name = cname;
        show(&c, "");
    }
    for (k = 0; k < 8u; k++) {
        case_t c = {0, MS[(k + 1u) % 8u], 0, 0, op_multi, MS[k], 1};
        snprintf(cname, sizeof cname, "multi load M%02u over M%02u", MS[k] + 1u, MS[(k + 1u) % 8u] + 1u);
        c.name = cname;
        show(&c, "");
    }
    {
        case_t c = {"multi load M15 over M01", 0, 0, 0, op_multi, 14, 1};
        show(&c, "");
        c = (case_t){"multi load M15 over M16", 15, 0, 0, op_multi, 14, 1};
        show(&c, "");
    }
    for (k = 0; k < 6u; k++) {                           /* reverb type: worst over the 5 types it can come from */
        res_t w = {0};
        w.extra_half = -1;
        for (from = 0; from < 6u; from++) {
            case_t c = {0, 14, pre_rtype, from, op_rtype, k, 0};
            res_t r;
            if (from == k)
                continue;
            r = run_case(&c);
            if (r.extra_half > w.extra_half)
                w = r;
        }
        snprintf(cname, sizeof cname, "reverb type -> %s (worst source)", RN[k]);
        print_res(cname, "", &w);
    }
    for (k = 1; k < 9u; k++) {                           /* insert OFF -> type (part 1; and both parts) */
        case_t c = {0, 14, pre_ioff, 0, op_itype, k, 0};
        snprintf(cname, sizeof cname, "insert OFF -> %s (part 1)", IN[k]);
        c.name = cname;
        show(&c, "");
        c.b = k | 16u;
        snprintf(cname, sizeof cname, "insert OFF -> %s (both parts)", IN[k]);
        c.name = cname;
        show(&c, "");
    }
    {
        case_t c = {"EQ: one band's gain +3 dB (part 1)", 14, 0, 0, op_eq, 1u | 4u, 0};
        show(&c, "");
        c = (case_t){"EQ: all 7 values (part 1), bands on already", 14, 0, 0, op_eq, 1, 0};
        show(&c, "");
        c = (case_t){"EQ: all 7 values (both parts)", 14, 0, 0, op_eq, 3, 0};
        show(&c, "");
        c = (case_t){"EQ: off -> all 3 bands on (both parts)", 14, pre_eqoff, 0, op_eq, 3, 0};
        show(&c, "");
        c = (case_t){"ducker depth/attack/release (both parts)", 14, 0, 0, op_duck, 1, 0};
        show(&c, "");
        c = (case_t){"delay TIME", 14, 0, 0, op_global, 0, 0};
        show(&c, "");
        c = (case_t){"delay FEEDBACK", 14, 0, 0, op_global, 1, 0};
        show(&c, "");
        c = (case_t){"chorus RATE/DEPTH", 14, 0, 0, op_global, 2, 0};
        show(&c, "");
        c = (case_t){"ping-pong toggle", 14, 0, 0, op_global, 3, 0};
        show(&c, "");
        c = (case_t){"reverb SIZE/DAMP jump", 14, 0, 0, op_global, 4, 0};
        show(&c, "");
        c = (case_t){"morph ROLL B (fader at A, B not heard)", 14, pre_morph, 0, op_roll, 1, 0};
        show(&c, "");
        c = (case_t){"morph ROLL A (fader at A: heard)", 14, pre_morph, 0, op_roll, 0, 0};
        show(&c, "");
        c = (case_t){"morph ROLL A, TARGET SELECTED (one part)", 14, pre_morph_sel, 0, op_roll, 0, 0};
        show(&c, "");
        c = (case_t){"MORPH fader jump 0 -> 127", 14, pre_morph, 0, op_fader, 127, 0};
        show(&c, "");
    }
    for (from = 0; from < 3u; from++)
        for (k = 0; k < 3u; k++) {
            static const char *const MD[3] = {"DUEL", "STEREO", "LAYER"};
            case_t c = {0, 14, pre_mode, from, op_mode, k, 0};
            if (from == k)
                continue;
            snprintf(cname, sizeof cname, "mode %s -> %s", MD[from], MD[k]);
            c.name = cname;
            show(&c, "");
        }
}

int main(int argc, char **argv)
{
    uint64_t a, b;
    int i;
    (void)argv;
    if (!instr_now()) {
        printf("loadcost: no instruction counter on this host: skipped\n");
        return 0;
    }
    a = instr_now();
    b = instr_now();
    overhead = b - a;
    for (i = 0; i < 5; i++) {
        a = instr_now();
        b = instr_now();
        if (b - a < overhead)
            overhead = b - a;
    }
    printf("loadcost: counter overhead %llu instr a read\n", (unsigned long long)overhead);
    full = (argc > 1 && !strcmp(argv[1], "full")) || getenv("LOADCOST_FULL") != 0;
    if (argc > 1 && !strcmp(argv[1], "trace")) {
        uint32_t from = argc > 3 ? (uint32_t)atoi(argv[3]) : 0, to = argc > 4 ? (uint32_t)atoi(argv[4]) : 0;
        case_t c = {0, 14, 0, 0, 0, 0, 0};
        trace = 1;
        c.name = argv[2];
        if (!strcmp(argv[2], "mode")) c.pre = pre_mode, c.a = from, c.op = op_mode, c.b = to;
        else if (!strcmp(argv[2], "multi")) c.base = from, c.op = op_multi, c.b = to;
        else if (!strcmp(argv[2], "roll")) c.pre = pre_morph, c.op = op_roll, c.b = from;
        else if (!strcmp(argv[2], "rev")) c.pre = pre_rtype, c.a = from, c.op = op_rtype, c.b = to;
        else if (!strcmp(argv[2], "patch")) c.op = op_patch, c.b = from << 8 | to;
        else if (!strcmp(argv[2], "eq")) c.pre = pre_eqoff, c.op = op_eq, c.b = 3;
        else return 2;
        show(&c, "");
        return 0;
    }
    if (full)
        table();
    else
        guards();
    printf("loadcost: %s\n", bad ? "FAIL" : "all ok");
    return bad != 0;
}
