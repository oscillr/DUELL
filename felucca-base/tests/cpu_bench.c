/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.23 LOAD BENCH: how the processor takes the engines and the effects, one reproducible table (report-only).
 *   build/host/cpu_bench              (host_tests.sh "bench"; docs/TESTING.md "Load bench")
 * Host instructions per sample of the steady state (proc_pid_rusage, as cpu_scenarios_test.c: -O1 -fno-builtin, the device's memset / memcpy), in the loadcost
 * scale: 420 k instructions = one half buffer (128 frames) = 100 %, so 3281 a sample = 100 % and 33 a sample = 1 % (the device reads ~0.75-0.9 x of it).
 * Every figure is a difference against its own reference scene (the same scene with the thing off), each scene from a fresh power-on:
 *   VOICES   one held note, each factory sound of each engine (sends, INSERT, SAT, EQ, DUCK off, POLY): the engine's dearest sound and the mean of its sounds;
 *   PART     on one ANALOG voice: every INSERT type (A B C at 64, MIX full), SAT (the part's drive), EQ (three bands moved), DUCK (PUMP, depth 90);
 *   BUSES    both parts one ANALOG voice each, sending 100 to one effect (chorus, delay, reverb) in STEREO (the shared buses: the stereo chorus) and in DUEL
 *            (a bus per part, all mono): CHORUS, DELAY DIGITAL / PING-PONG / RANDOM / OCT UP, REVERB ROOM / SPRING / HALL / SHIM / RESO / PLATE, SHIM's
 *            PITCH DOWN and MOD on top of SHIM UP (0.2.25: BOTH removed);
 *   MASTER   STEREO (none of them runs in DUEL): COMP (4:1), WIDTH (150 %), BASS MONO (100 Hz) (0.2.25: the SATURATOR is removed);
 *   SCENE    the worst case per MODE (LAYER, STEREO, DUEL): both parts on the dearest sound of the dearest engine with INSERT (the dearest type) + SAT + EQ + DUCK,
 *            every send at 100 (DELAY RANDOM, REVERB SHIM with MOD 50 (0.2.25: PITCH BOTH removed), the chorus), COMP + WIDTH on (not in DUEL: they do not run there),
 *            the ARP running on both parts (CHORD, 1/16, the held notes again every step); k notes held on each part, k = 1 and 4: the fixed part F and the
 *            cost a voice V (both parts alike), the estimated polyphony per part before 100 %: (3281 - F) / (2 V). (The unit's pool is 8 voices: 4 a part with
 *            two parts; the VOICE CAP is not applied here.)
 * The figures go to tests/bench_baseline.txt: a run fails only when one of them is more than 10 % (and 5 instructions) above it; BENCH_UPDATE=1 rewrites it.
 * Needs macOS (the instruction counter); elsewhere it reports and passes. Runs in about 2 s. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

#define FULL 3281.25                                    /* host instructions a sample = 100 % (420 k a half of 128 frames) */
#define SETTLE 60u                                      /* blocks of CTL before the count (attacks, fades, WAVE tables) */
#define COUNT 400u                                      /* blocks counted (0.29 s) */
static uint64_t ovh;                                    /* the counter's own cost (a pair of reads) */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static int32_t ob[2 * CTL];
static void mixblocks(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        mix_block(ob, CTL);
        while (wave_service())
            ;
    }
}
static double g_voices;                                 /* the voices sounding in the last measure, averaged over its blocks (counted outside the count) */
static double measure(void)                             /* instructions a sample of the scene as it stands */
{
    uint64_t a, sum = 0, vs = 0;
    uint32_t b, p, i;
    mixblocks(SETTLE);
    for (b = 0; b < COUNT; b++) {
        a = instr_now();
        mix_block(ob, CTL);
        sum += instr_now() - a;
        while (wave_service())
            ;
        for (p = 0; p < NTRK; p++)
            for (i = 0; i < NVOICE; i++)
                vs += trk[p].v[i].active != 0;
    }
    g_voices = (double)vs / COUNT;
    return (double)(sum - (uint64_t)COUNT * ovh) / (COUNT * CTL);
}

/* ------------------------------------------------------------------ scenes --- */
typedef struct {
    int mode;                                           /* DM_* */
    int e[2], k[2], n[2];                               /* engine (-1: none), sound, notes held */
    int ins, sat, eq, duck;                             /* part stages on both sounding parts (ins: the INSERT type) */
    int cho, dly, rev;                                  /* send levels (both parts) */
    int dmode, rtype, shp, shm;                         /* G_DMODE, G_RTYPE, SHIM's PITCH and MOD (-1: as the power-on has them) */
    int comp, width, bmono;                             /* master: RATIO index, WIDTH %, BASS MONO (0 off) */
    int arp;                                            /* the ARP: CHORD at 1/16 on the sounding parts */
    int sendsoff;                                       /* GLO > MODULES SENDS OFF (the buses not run: the reference of the bus rows) */
} scn_t;
static scn_t blank(int mode)
{
    scn_t s;
    memset(&s, 0, sizeof s);
    s.mode = mode;
    s.e[0] = s.e[1] = -1;
    s.dmode = s.rtype = s.shp = s.shm = -1;
    s.width = 100;
    return s;
}
static uint32_t eng_named(const char *n) { uint32_t e; for (e = 0; e < NENGINES; e++) if (str_eq(ENGINES[e]->name, n)) return e; return 0; }
static const uint8_t N8[8] = {48, 52, 55, 59, 62, 65, 69, 72};
static void setpart(track_t *t, const scn_t *s, uint32_t p)
{
    uint32_t i;
    if (s->e[p] < 0)
        return;
    t->eng_req = (uint8_t)s->e[p];
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = ENGINES[s->e[p]]->edit[i].def;
    apply_preset_to(t, (uint32_t)s->k[p]);
    t->p[P_SUS] = 127;
    t->p[P_CHRD] = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_AMODE] = 0;
    t->p[P_CHOR] = (int16_t)s->cho, t->p[P_DLY] = (int16_t)s->dly, t->p[P_REV] = (int16_t)s->rev;
    t->p[P_ITYPE] = (int16_t)s->ins;
    if (s->ins)
        t->p[P_IA] = t->p[P_IB] = t->p[P_IC] = 64, t->p[P_IMIX] = 127;
    t->p[P_DIST] = (int16_t)s->sat;
    t->p[P_EQLG] = t->p[P_EQMG] = t->p[P_EQHG] = 0;
    if (s->eq)
        t->p[P_EQLG] = 5, t->p[P_EQLF] = -4, t->p[P_EQMG] = -4, t->p[P_EQMF] = 5, t->p[P_EQMQ] = 3, t->p[P_EQHG] = 6, t->p[P_EQHF] = 2;
    t->p[P_DKDEP] = 0;
    if (s->duck)
        t->p[P_DKDEP] = 90, t->p[P_DKATK] = 20, t->p[P_DKRLS] = 70, t->p[P_DKSRC] = 0;
    if (s->arp)
        t->p[P_AMODE] = AM_CHORD, t->p[P_ARATE] = 2, t->p[P_AGATE] = 127, t->p[P_AOCT] = 1;
}
static double run(const scn_t *s)
{
    uint32_t p, i;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    for (p = 0; p < NTRK; p++)
        memset(fm6_patch[p], 0, sizeof fm6_patch[p]);
    flash_ok = 1;
    multi_boot();
    frame();
    song.master_q12 = 2048;                             /* (MASTER at its default: the stages see what they see on a unit) */
    duell_mode_force((uint32_t)s->mode);
    if (s->sendsoff)
        mods_put(MODS_SENDS, 1u);                       /* (its fade runs out in the settle) */
    for (p = 0; p < 2u; p++)
        setpart(&trk[p], s, p);
    if (s->dmode >= 0) song.g[G_DMODE] = (int16_t)s->dmode;
    if (s->rtype >= 0) song.g[G_RTYPE] = (int16_t)s->rtype;
    if (s->shp >= 0) song.g[G_SHPITCH] = (int16_t)s->shp;
    if (s->shm >= 0) song.g[G_SHMOD] = (int16_t)s->shm;
    song.g[G_RLOCUT] = 0;
    cmp_cfg = (uint32_t)CMP_THR_DEF | (uint32_t)s->comp << 8 | (uint32_t)CMP_ATK_DEF << 16 | (uint32_t)CMP_REL_DEF << 24;   /* (the audio side's words: no frame() runs after this) */
    wd_sel = (uint8_t)s->width;
    bm_sel = (uint8_t)s->bmono;
    mixblocks(40);                                         /* (the engine switch's fade, the mode, the tables) */
    if (s->arp)
        transport_req = 1;
    for (p = 0; p < 2u; p++)
        for (i = 0; s->e[p] >= 0 && i < (uint32_t)s->n[p]; i++)
            trk_note_on(&trk[p], (uint8_t)(N8[i] + 7u * p), 110);
    return measure();
}

/* ------------------------------------------------------------------ the table and the baseline --- */
typedef struct { char name[48]; double v; } fig_t;
static fig_t figs[512];
static uint32_t nfig;
static void fig(const char *name, double v)
{
    if (nfig < NELEM(figs)) {
        snprintf(figs[nfig].name, sizeof figs[nfig].name, "%s", name);
        figs[nfig++].v = v;
    }
}
static void row(const char *what, double v, const char *note)
{
    printf("bench: %-44s %7.1f  %5.1f %%  %s\n", what, v, v * 100.0 / FULL, note ? note : "");
}
static void slug(char *d, const char *s, uint32_t n)
{
    uint32_t i;
    for (i = 0; s[i] && i + 1u < n; i++)
        d[i] = s[i] == ' ' || s[i] == '/' ? '_' : s[i];
    d[i] = 0;
}

int main(void)
{
    static const char *const MN[3] = {"DUEL", "STEREO", "LAYER"};
    const char *bpath = getenv("BENCH_FILE") ? getenv("BENCH_FILE") : "tests/bench_baseline.txt";
    uint32_t e, k, t, m, heavy_e = 0, heavy_k = 0, heavy_ins = 1;
    double idle2, ref1, v, worst = 0, wins = 0, ref;
    char nm[64], s1[32];
    int bad = 0;
    scn_t s;
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!instr_now()) {
        printf("bench: no instruction counter on this host, skipped\n");
        return 0;
    }
    {
        uint64_t a0, a1;
        uint32_t i;
        ovh = ~0ull;
        for (i = 0; i < 20u; i++) { a0 = instr_now(); a1 = instr_now(); if (a1 - a0 < ovh) ovh = a1 - a0; }
    }
    printf("bench: DUELL load bench, host instructions a sample (-O1, the device's memset); 100 %% = %.0f (33 a sample = 1 %%; the device reads ~0.75-0.9 x)\n", FULL);

    /* VOICES */
    printf("bench: --- VOICES: one held note (sends, INSERT, SAT, EQ, DUCK off), the engine's dearest factory sound and the mean of its sounds\n");
    s = blank(DM_STEREO);
    idle2 = run(&s);
    row("idle mix (STEREO, nothing playing)", idle2, "");
    fig("idle", idle2);
    for (t = 0; t < NENG_SHOWN; t++) {
        double mx = 0, sum = 0;
        uint32_t nk = 0, mk = 0;
        e = ENGINE_ORDER[t];
        for (k = 0; k < ds_count(e); k++) {
            s = blank(DM_STEREO);
            s.e[0] = (int)e, s.k[0] = (int)k, s.n[0] = 0;
            ref = run(&s);
            s.n[0] = 1;
            v = run(&s) - ref;
            sum += v, nk++;
            if (v > mx)
                mx = v, mk = k;
        }
        if (!nk)
            continue;
        snprintf(nm, sizeof nm, "voice %s (dearest: %s)", ENGINES[e]->name, ds_name(e, mk));
        snprintf(s1, sizeof s1, "mean %.0f", sum / nk);
        row(nm, mx, s1);
        snprintf(nm, sizeof nm, "voice/%s", ENGINES[e]->name);
        fig(nm, mx);
        if (mx > worst)
            worst = mx, heavy_e = e, heavy_k = mk;
    }

    /* PART */
    printf("bench: --- PART stages: one ANALOG voice, the stage on that part\n");
    s = blank(DM_STEREO);
    s.e[0] = (int)eng_named("ANALOG"), s.k[0] = 0, s.n[0] = 1;
    ref1 = run(&s);
    for (t = 1; t < NELEM(N_ITYPE); t++) {
        scn_t q = s;
        q.ins = (int)t;
        v = run(&q) - ref1;
        snprintf(nm, sizeof nm, "INSERT %s", N_ITYPE[t]);
        row(nm, v, "");
        snprintf(nm, sizeof nm, "insert/%s", N_ITYPE[t]);
        fig(nm, v);
        if (v > wins)
            wins = v, heavy_ins = t;
    }
    {
        scn_t q = s;
        q.sat = 64;
        v = run(&q) - ref1;
        row("SAT (the part's drive, 64)", v, "");
        fig("part/SAT", v);
        q = s, q.eq = 1;
        v = run(&q) - ref1;
        row("EQ (three bands)", v, "");
        fig("part/EQ", v);
        q = s, q.duck = 1;
        v = run(&q) - ref1;
        row("DUCK (PUMP, depth 90)", v, "");
        fig("part/DUCK", v);
    }

    /* BUSES */
    printf("bench: --- BUS effects: both parts one ANALOG voice each; 'all' = the bus section against SENDS OFF with only this effect fed (the delay and the reverb run whenever\n"
           "bench:     SENDS is ON, fed or not; the chorus is skipped while nothing feeds it), 'fed' = this effect at send 100 against the same with every send at 0\n");
    printf("bench: %-44s %15s %15s %15s %15s\n", "", "STEREO all", "STEREO fed", "DUEL all", "DUEL fed");
    {
        static const char *const DM[4] = {"DIGITAL", "PING-PONG", "RANDOM", "OCT UP"};
        static const char *const RT[6] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};
        double off[2];
        uint32_t c;
        struct { const char *name; int cho, dly, rev, dmode, rtype, shp, shm; } B[18];
        uint32_t nb = 0;
        B[nb].name = "nothing fed (HALL, DIGITAL: power-on)", B[nb].cho = B[nb].dly = B[nb].rev = 0, B[nb].dmode = B[nb].rtype = B[nb].shp = B[nb].shm = -1, nb++;
        B[nb].name = "CHORUS (STEREO: the stereo chorus)", B[nb].cho = 100, B[nb].dly = B[nb].rev = 0, B[nb].dmode = B[nb].rtype = B[nb].shp = B[nb].shm = -1, nb++;
        for (c = 0; c < 4u; c++)
            B[nb].name = DM[c], B[nb].cho = 0, B[nb].dly = 100, B[nb].rev = 0, B[nb].dmode = (int)c, B[nb].rtype = B[nb].shp = B[nb].shm = -1, nb++;
        for (c = 0; c < 6u; c++)
            B[nb].name = RT[c], B[nb].cho = B[nb].dly = 0, B[nb].rev = 100, B[nb].dmode = -1, B[nb].rtype = (int)c, B[nb].shp = SHP_UP, B[nb].shm = 0, nb++;
        B[nb].name = "SHIM PITCH DOWN", B[nb].cho = B[nb].dly = 0, B[nb].rev = 100, B[nb].dmode = -1, B[nb].rtype = 3, B[nb].shp = SHP_DOWN, B[nb].shm = 0, nb++;
        B[nb].name = "SHIM PITCH UP + MOD 50", B[nb].cho = B[nb].dly = 0, B[nb].rev = 100, B[nb].dmode = -1, B[nb].rtype = 3, B[nb].shp = SHP_UP, B[nb].shm = 50, nb++;   /* (0.2.25: PITCH BOTH removed, its two rows with it) */
        for (m = 0; m < 2u; m++) {
            s = blank(m ? DM_SPLIT : DM_STEREO);
            s.e[0] = s.e[1] = (int)eng_named("ANALOG"), s.n[0] = s.n[1] = 1;
            s.sendsoff = 1;
            off[m] = run(&s);
        }
        for (c = 0; c < nb; c++) {
            double w[2], f[2];
            for (m = 0; m < 2u; m++) {
                s = blank(m ? DM_SPLIT : DM_STEREO);
                s.e[0] = s.e[1] = (int)eng_named("ANALOG"), s.n[0] = s.n[1] = 1;
                s.dmode = B[c].dmode, s.rtype = B[c].rtype, s.shp = B[c].shp, s.shm = B[c].shm;
                ref = run(&s);                          /* (this setting, nothing fed) */
                s.cho = B[c].cho, s.dly = B[c].dly, s.rev = B[c].rev;
                v = c ? run(&s) : ref;
                w[m] = v - off[m];
                f[m] = v - ref;
                slug(s1, B[c].name, sizeof s1);
                snprintf(nm, sizeof nm, "bus/%s/%s", m ? "DUEL" : "STEREO", s1);
                fig(nm, w[m]);
            }
            snprintf(nm, sizeof nm, "%s%s", c >= 2u && c <= 5u ? "DELAY " : c >= 6u && c <= 11u ? "REVERB " : "", B[c].name);
            printf("bench: %-44s %7.1f %5.1f %% %7.1f %5.1f %% %7.1f %5.1f %% %7.1f %5.1f %%\n", nm, w[0], w[0] * 100.0 / FULL, f[0], f[0] * 100.0 / FULL,
                   w[1], w[1] * 100.0 / FULL, f[1], f[1] * 100.0 / FULL);
        }
    }

    /* MASTER */
    printf("bench: --- MASTER stages (STEREO; none runs in DUEL): both parts one ANALOG voice\n");
    {
        s = blank(DM_STEREO);
        s.e[0] = s.e[1] = (int)eng_named("ANALOG"), s.n[0] = s.n[1] = 1;
        ref = run(&s);
        for (t = 0; t < 3u; t++) {
            scn_t q = s;
            const char *n2 = t == 0u ? "COMP (4:1)" : t == 1u ? "WIDTH (150 %)" : "BASS MONO (100 Hz)";
            if (t == 0u) q.comp = 4;
            if (t == 1u) q.width = 150;
            if (t == 2u) q.bmono = 5;
            v = run(&q) - ref;
            row(n2, v, "");
            snprintf(nm, sizeof nm, "master/%s", t == 0u ? "COMP" : t == 1u ? "WIDTH" : "BASSMONO");
            fig(nm, v);
        }
    }

    /* SCENE */
    printf("bench: --- WORST-CASE SCENE: both parts %s #%u %s, INSERT %s + SAT + EQ + DUCK, CHORUS + DELAY RANDOM + REVERB SHIM (MOD 50) at 100, COMP + WIDTH, ARP CHORD 1/16\n",
           ENGINES[heavy_e]->name, heavy_k, ds_name(heavy_e, heavy_k), N_ITYPE[heavy_ins]);
    for (m = 0; m < 3u; m++) {
        static const int MODES[3] = {DM_LAYER, DM_STEREO, DM_SPLIT};
        double c1, c4, V, F, est, v1, v4;
        s = blank(MODES[m]);
        s.e[0] = s.e[1] = (int)heavy_e, s.k[0] = s.k[1] = (int)heavy_k;
        s.ins = (int)heavy_ins, s.sat = 64, s.eq = 1, s.duck = 1;
        s.cho = s.dly = s.rev = 100, s.dmode = DMD_RAND, s.rtype = 3, s.shp = SHP_UP, s.shm = 50;   /* (0.2.25: BOTH removed: UP + MOD 50, SHIM's dearest now) */
        s.comp = 4, s.width = 150;
        s.arp = 1;
        s.n[0] = s.n[1] = 1;
        c1 = run(&s);
        v1 = g_voices;
        s.n[0] = s.n[1] = 4;
        c4 = run(&s);
        v4 = g_voices;
        V = v4 > v1 ? (c4 - c1) / (v4 - v1) : 0;       /* (a voice, from the voices that sounded: the arp's retriggers fade the old ones, an engine may play fewer) */
        F = c1 - v1 * V;
        est = V > 0 ? (FULL - F) / (2.0 * V) : 99.0;
        printf("bench: %-6s 1 + 1 notes %6.0f (%5.1f %%, %.1f voices), 4 + 4 notes %6.0f (%5.1f %%, %.1f voices): fixed %5.0f (%4.1f %%), a voice %4.0f (%4.1f %%)\n",
               MN[MODES[m]], c1, c1 * 100.0 / FULL, v1, c4, c4 * 100.0 / FULL, v4, F, F * 100.0 / FULL, V, V * 100.0 / FULL);
        printf("bench: %-6s   est. max %4.1f voices a part before 100 %% (the device at 0.9 x .. 0.75 x: %4.1f .. %4.1f)\n", MN[MODES[m]], est,
               V > 0 ? (FULL - 0.9 * F) / (2.0 * 0.9 * V) : 99.0, V > 0 ? (FULL - 0.75 * F) / (2.0 * 0.75 * V) : 99.0);
        snprintf(nm, sizeof nm, "scene/%s/fixed", MN[MODES[m]]);
        fig(nm, F);
        snprintf(nm, sizeof nm, "scene/%s/voice", MN[MODES[m]]);
        fig(nm, V);
        snprintf(nm, sizeof nm, "scene/%s/1+1", MN[MODES[m]]);
        fig(nm, c1);
        snprintf(nm, sizeof nm, "scene/%s/4+4", MN[MODES[m]]);
        fig(nm, c4);
    }

    /* the baseline */
    if (getenv("BENCH_UPDATE")) {
        FILE *f = fopen(bpath, "w");
        uint32_t i;
        if (!f) {
            printf("bench: cannot write %s\n", bpath);
            return 1;
        }
        fprintf(f, "# DUELL load bench (tests/cpu_bench.c): host instructions a sample, -O1 -fno-builtin on the Mac (kernel-counted, ~1 %% run to run). A run fails when a figure\n"
                   "# is more than 10 %% (and 5 instructions) above its line here. Rewritten by BENCH_UPDATE=1 (on purpose: review the diff).\n");
        for (i = 0; i < nfig; i++)
            fprintf(f, "%s %.0f\n", figs[i].name, figs[i].v);
        fclose(f);
        printf("bench: %u figures written to %s\n", nfig, bpath);
        return 0;
    }
    {
        FILE *f = fopen(bpath, "r");
        char line[160], name[64];
        double b;
        uint32_t i, seen = 0;
        if (!f) {
            printf("bench: no baseline %s (BENCH_UPDATE=1 writes it)\n", bpath);
            return 0;
        }
        while (fgets(line, sizeof line, f)) {
            if (line[0] == '#' || sscanf(line, "%63s %lf", name, &b) != 2)
                continue;
            for (i = 0; i < nfig; i++)
                if (!strcmp(figs[i].name, name)) {
                    seen++;
                    if (figs[i].v > b * 1.10 && figs[i].v > b + 5.0) {
                        printf("bench: FAIL %s: %.0f, the baseline %.0f (+%.0f %%)\n", name, figs[i].v, b, (figs[i].v / b - 1.0) * 100.0);
                        bad = 1;
                    }
                }
        }
        fclose(f);
        printf("bench: %u figures against the baseline (%u in it): %s\n", nfig, seen, bad ? "FAIL (a figure grew more than 10 %; BENCH_UPDATE=1 if that is meant)" : "none grew more than 10 %");
    }
    return bad;
}
