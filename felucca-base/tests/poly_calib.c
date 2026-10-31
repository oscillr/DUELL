/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 POLYPHONY CALIBRATION (HANDOFF_V2 item 90): the cost of ONE MORE note, per engine, with and without the MULTI and TWIN filters, measured with the load bench's
 * counter (tests/cpu_bench.c: host instructions a sample, -O1 -fno-builtin, 3281.25 = 100 % of a half; the unit read 0.90 .. 1.08 x of the same model in the one-core bench B2 of
 * 2026-10-18, release_drafts/bench_results/bench_B2_run1_core1off_2026-10-18.csv). Every factory sound of the engine, cleaned (no TWIN, no SUB, no matrix slot, no CYCLE, POLY,
 * INSERT / SAT / EQ / DUCK / sends off), then the variant: MULTI off (bypassed), MULTI 12 dB, MULTI 24 dB, TWIN LP / LADDER / COMB after a 12 dB MULTI, TWIN with DRIVE + PARALLEL.
 * A note's cost = (cost with n notes - cost with 1 note) / (n - 1), n = 5 or the engine's own limit: the part's own work that runs once whatever plays is not charged per
 * note (it is in "first", the first note's extra). Prints the dearest and the mean sound per engine and variant, and the budget's figure for the same settings (budget.c
 * part_cost, margin taken out) beside it. Report only; needs macOS (the instruction counter).   build/host/poly_calib  (host_tests.sh polycalib) */
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
static void (*tweak)(track_t *t);
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
    for (p = 0; p < 2u; p++)
        if (s->e[p] >= 0 && tweak)
            tweak(&trk[p]);
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


/* ------------------------------------------------------------------ the variants --- */
enum { VA_OFF, VA_M12, VA_M24, VA_TLP, VA_TLAD, VA_TCOMB, VA_TOFF, VA_TFULL, VA_SUB, VA_N };
static const char *const VA_NAME[VA_N] = {"MULTI off", "MULTI 12", "MULTI 24", "M12+TWIN LP", "M12+TW LADDER", "M12+TW COMB", "TWIN LP only", "TWIN LP+DRV+PAR+ENV", "M12+SUB SQR"};
static const uint8_t VA_REF[VA_N] = {VA_OFF, VA_OFF, VA_OFF, VA_M12, VA_M12, VA_M12, VA_OFF, VA_TLP, VA_M12};   /* the paired difference: against this variant, the same sound */
static int g_va;
static void clean(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NMSLOT; i++)
        t->p[MSLOT_ID(i)] = t->p[MSLOT_ID(i) + 1] = t->p[MSLOT_ID(i) + 2] = 0;
    t->p[P_TWTYPE] = TT_OFF, t->p[P_DRV2] = 0, t->p[P_RES2] = 0;
    for (i = 0; i < NX; i++)
        if (i != X_LFO1LVL && i != X_LFO2LVL && i != X_LINK)
            XV(t, i) = 0;
    t->p[P_KTRK] = 0;
    t->p[P_FREEB] = 40;
    t->p[P_LD_FLT] = t->p[P_E2FLT] = t->p[P_ED_FLT] = t->p[P_L2FLT] = 0;   /* (no cutoff modulation: the bypass is a true bypass) */
    switch (g_va) {
    case VA_OFF: case VA_TOFF:
        t->p[P_FREEA] = UF_STEPS, t->p[P_FREEC] = 0, t->p[P_FREED] = 1;
        break;
    default:
        t->p[P_FREEA] = 50, t->p[P_FREEC] = 0, t->p[P_FREED] = g_va == VA_M24 ? 2 : 1;
        break;
    }
    if (g_va >= VA_TLP && g_va <= VA_TFULL) {
        t->p[P_TWTYPE] = g_va == VA_TLAD ? TT_LADDER : g_va == VA_TCOMB ? TT_COMB : TT_LP;
        t->p[P_CUT2] = g_va == VA_TCOMB ? 0 : 10;
        t->p[P_RES2] = 30;
        if (g_va == VA_TFULL)
            t->p[P_DRV2] = 64, XV(t, X_ROUTE) = 1, XV(t, X_ENV2) = 20;
    }
    if (g_va == VA_SUB)
        XV(t, X_SUBLVL) = 90, XV(t, X_SUBWAVE) = 2;
}
#define MAXK 64
static double note_cost(uint32_t e, uint32_t k, double *first)
{
    uint32_t cap = vc_eng_cap(ENGINES[e]), nh = cap < 5u ? cap : 5u;
    double c0, c1, cn, v;
    scn_t s = blank(DM_STEREO);
    s.e[0] = (int)e, s.k[0] = (int)k, s.n[0] = 0;
    c0 = run(&s);
    s.n[0] = 1;
    c1 = run(&s);
    s.n[0] = (int)nh;
    cn = run(&s);
    v = (cn - c1) / (double)(nh - 1u);
    if (first)
        *first = c1 - c0 - v;
    return v;
}

int main(int argc, char **argv)
{
    uint32_t t, e, k, va;
    scn_t s;
    double idle, gmax[VA_N] = {0}, fmax = 0;
    static double c[VA_N][MAXK];
    FILE *csv = argc > 1 ? fopen(argv[1], "w") : NULL;
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!instr_now()) {
        printf("polycalib: no instruction counter on this host, skipped\n");
        return 0;
    }
    {
        uint64_t a0, a1;
        uint32_t i;
        ovh = ~0ull;
        for (i = 0; i < 20u; i++) { a0 = instr_now(); a1 = instr_now(); if (a1 - a0 < ovh) ovh = a1 - a0; }
    }
    tweak = clean;
    s = blank(DM_STEREO);
    idle = run(&s);
    printf("polycalib: host instructions a sample (3281 = 100 %%), one more note on one part (marginal), every factory sound cleaned; the dearest sound per engine\n");
    printf("polycalib: idle (STEREO, power-on sends, nothing playing) %.0f = %.1f %%\n", idle, idle * 100.0 / FULL);
    {
        double a, b, c;
        s = blank(DM_STEREO); s.sendsoff = 1; a = run(&s);
        s = blank(DM_SPLIT); s.sendsoff = 1; b = run(&s);
        s = blank(DM_SPLIT); c = run(&s);
        printf("polycalib: idle SENDS OFF: STEREO %.0f, DUEL %.0f; DUEL with power-on sends %.0f\n", a, b, c);
        if (getenv("POLYCALIB_IDLE_ONLY"))
            return 0;
    }
    if (getenv("POLYCALIB_MULTIS")) {                   /* the factory multis as loaded: one more note on each part alone (the other muted), against the budget's figure */
        uint32_t m, p, n, i;
        for (m = 1; m <= 16u; m++)
            for (p = 0; p < 2u; p++) {
                double c[2], bv;
                for (n = 0; n < 2u; n++) {
                    memset(nor, 0xFF, sizeof nor);
                    ui_power_on();
                    flash_ok = 1;
                    multi_boot();
                    frame();
                    ui_prefs2 |= PREF_MODE_RECALL >> 16;
                    project_load(m);
                    frame();
                    trk[!p].p[P_MUTE] = 1;
                    trk[p].p[P_VOICE] = V_POLY;
                    trk[p].p[P_AMODE] = 0;
                    mixblocks(40);
                    for (i = 0; i < (n ? 4u : 1u) && i < vc_eng_cap(ENGINES[eng_idx(trk[p].eng_req)]); i++)
                        trk_note_on(&trk[p], (uint8_t)(N8[i] + 7u * p), 100);
                    c[n] = measure();
                    c[n] = c[n] / (g_voices > 0 ? 1.0 : 1.0);
                    if (n)
                        c[n] = (c[n] - c[0]) / (g_voices - 1.0 > 0 ? g_voices - 1.0 : 1.0);
                }
                bv = part_cost(&trk[p]) * 2100.0 / (64.0 * BUDGET_MARGIN);
                printf("polycalib: M%02u part %u %-7s one more note measured %4.0f, budget (margin out) %4.0f (%+.0f %%)\n", m, p + 1, ENGINES[eng_idx(trk[p].eng_req)]->name, c[1], bv, (bv / c[1] - 1.0) * 100.0);
            }
        return 0;
    }
    printf("polycalib: %-8s %8s %8s | paired extra over the reference variant, dearest sound: %s\n", "engine", "OFF", "M12", "M12-OFF M24-OFF TLP TLAD TCOMB TOFF-OFF FULL-TLP SUB");
    if (csv)
        fprintf(csv, "engine,note_off_dear,note_m12_dear,first_dear,x_m12,x_m24,x_tlp,x_tlad,x_tcomb,x_toff,x_full,x_sub\n");
    for (t = 0; t < NENG_SHOWN; t++) {
        double x[VA_N] = {0}, off = 0, m12 = 0, fst = 0;
        uint32_t nk;
        e = ENGINE_ORDER[t];
        nk = ds_count(e) < MAXK ? ds_count(e) : MAXK;
        for (va = 0; va < VA_N; va++) {
            g_va = (int)va;
            for (k = 0; k < nk; k++) {
                double f = 0;
                c[va][k] = note_cost(e, k, va == VA_M12 ? &f : NULL);
                if (va == VA_M12 && f > fst)
                    fst = f;
            }
        }
        for (k = 0; k < nk; k++) {
            off = c[VA_OFF][k] > off ? c[VA_OFF][k] : off;
            m12 = c[VA_M12][k] > m12 ? c[VA_M12][k] : m12;
            for (va = 1; va < VA_N; va++) {
                double d = c[va][k] - c[VA_REF[va]][k];
                x[va] = d > x[va] ? d : x[va];
            }
        }
        for (va = 1; va < VA_N; va++)
            gmax[va] = x[va] > gmax[va] ? x[va] : gmax[va];
        fmax = fst > fmax ? fst : fmax;
        printf("polycalib: %-8s %8.0f %8.0f | first %3.0f |", ENGINES[e]->name, off, m12, fst);
        for (va = 1; va < VA_N; va++)
            printf(" %4.0f", x[va]);
        printf("\n");
        if (csv)
            fprintf(csv, "%s,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n", ENGINES[e]->name, off, m12, fst, x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[8]);
    }
    printf("polycalib: all engines, the dearest extra: first %.0f |", fmax);
    for (va = 1; va < VA_N; va++)
        printf(" %s %.0f |", VA_NAME[va], gmax[va]);
    printf("\n");
    if (csv)
        fclose(csv);
    return 0;
}
