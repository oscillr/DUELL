/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL NOTES budget (firmware/src/budget.c; docs/CPU_PLAN.md section 5; host_tests.sh "budget"). No instruction counter is needed: the model is
 * checked against the figures of tests/bench_baseline.txt (the bench's Mac run), so it runs on any host.
 *   (a) MODEL    the cost table reproduces the bench rows within 5 %: one voice of each engine, each INSERT type, SAT, EQ, DUCK, each bus effect in STEREO and
 *                DUEL, the master stages; the worst-case scene of each MODE: its fixed part within 5 %, the totals at 1 + 1 and 4 + 4 notes never below the bench
 *                (the model errs high: a first voice costs more than the following ones) and within 12 % over it
 *   (b) FACTORY  the 16 factory multis as loaded: each part's NOTES, what limits it, the VOICE CAP of 0.2.11 beside it (printed; the old cap must not be
 *                stricter in a way that matters: reported, the stricter one holds in voice.c)
 *   (c) MONOTONE switching on an INSERT, a 24 dB slope, SAT, an EQ band, the ducker or a heavier send effect never raises that part's number (the 16 multis and
 *                5000 random settings)
 *   (d) STEAL    12 note-ons on a part whose budget is 4: at most 4 voices at any block, and the largest sample step at a steal within 1.5 x the calm before it
 *                (the limit of regress.c's steal-fade check); also with the budget worked out from settings (budget_poll)
 *   (e) PURE     the same settings give the same numbers in any order of calls, after any history (a setting switched on and off again), twice; the reasons
 *   the numbers are then printed as the tables of the report */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
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

static int fails;
static void bchk(const char *what, int ok)
{
    printf("budget: %-4s %s\n", ok ? "ok" : "FAIL", what);
    fails += !ok;
}

static void power(void)
{
    uint32_t k;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
}
static uint32_t eng_named(const char *n) { uint32_t e; for (e = 0; e < NENGINES; e++) if (str_eq(ENGINES[e]->name, n)) return e; return 0; }
static uint32_t sound_named(uint32_t e, const char *n)
{
    uint32_t k, m = ds_count(e);
    for (k = 0; k < m; k++)
        if (str_eq(ds_name(e, k), n))
            return k;
    return 0;
}
static const char *const MODE_NAME[3] = {"DUEL", "STEREO", "LAYER"};

/* the model's figures back in the bench's units: 1/100 % with the margin -> host instructions a sample */
static double raw(uint32_t u) { return (double)u * 2100.0 / (64.0 * BUDGET_MARGIN); }

/* ---- the bench baseline ---- */
static double base_get(const char *name)
{
    FILE *f = fopen(getenv("BENCH_FILE") ? getenv("BENCH_FILE") : "tests/bench_baseline.txt", "r");
    char line[160], nm[96];
    double v;
    if (!f)
        return -1.0;
    while (fgets(line, sizeof line, f))
        if (line[0] != '#' && sscanf(line, "%95s %lf", nm, &v) == 2 && !strcmp(nm, name)) {
            fclose(f);
            return v;
        }
    fclose(f);
    return -1.0;
}
static void slug(char *d, const char *s, uint32_t n)       /* cpu_bench.c slug */
{
    uint32_t i;
    for (i = 0; s[i] && i + 1u < n; i++)
        d[i] = s[i] == ' ' || s[i] == '/' ? '_' : s[i];
    d[i] = 0;
}
static uint32_t within(const char *what, double model, double bench, double tol)
{
    double d = (model - bench) / bench;
    int ok = d >= -tol && d <= tol;
    if (!ok || getenv("VERBOSE"))
        printf("budget:   %-44s model %7.1f bench %7.1f  %+5.1f %% %s\n", what, model, bench, d * 100.0, ok ? "" : "<-- OUT");
    return ok;
}

/* a neutral part: the engine's voice only (filter bypassed, nothing else on) */
static void neutral(track_t *t, uint32_t e)
{
    memset(t->p, 0, sizeof t->p);
    t->eng_req = (uint8_t)e;
    t->p[P_FREEA] = (int16_t)UF_STEPS;
    t->p[P_FREED] = 1;
}
static void clean_globals(uint32_t mode)
{
    uint32_t p;
    duell_mode_force(mode);
    duell_mode_req = (uint8_t)mode;
    mods_off = 0;
    cmp_cfg = 0;
    wd_sel = 100;
    bm_sel = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    song.g[G_RTYPE] = 2;
    song.g[G_SHPITCH] = SHP_UP;
    song.g[G_SHMOD] = 0;
    for (p = 0; p < NLIVE; p++) {
        neutral(&trk[p], eng_named("ANALOG"));
        mph[p].nid = mph[p].fnid = 0;
    }
}

/* the model's table, as the report prints it: % of the time budget with the margin (one voice, one part stage, one global item) */
static void t_table(void)
{
    uint32_t i, m;
    printf("budget: --- the cost table (%% of the time budget with the margin %u %%; host instructions a sample / 32.8 = %%)\n", BUDGET_MARGIN);
    printf("budget: voice a note: ");
    for (i = 0; i < NELEM(BD_ENG); i++)
        printf("%s %.1f  ", BD_ENG[i].e->name, BD_U(BD_ENG[i].c) / 100.0);
    printf("\nbudget: + multimode filter a voice: 12 dB %.1f, 24 dB %.1f; MORPH %.1f, engine fade %.1f\n", BD_U(BD_F12) / 100.0, BD_U(BD_F24) / 100.0, BD_U(BD_MORPH) / 100.0, BD_U(BD_FADE) / 100.0);
    printf("budget: part stages: INSERT");
    for (i = 1; i < NELEM(BD_INS); i++)
        printf(" %s %.1f", N_ITYPE[i], BD_U(BD_INS[i]) / 100.0);
    printf(", SAT %.1f, EQ %.1f + %.1f a band, DUCK %.1f, KEY TRK %.2f, matrix slot %.1f, LFO %.1f, ARP %.1f\n", BD_U(BD_SAT) / 100.0, BD_U(BD_EQ_BASE) / 100.0, BD_U(BD_EQ_BAND) / 100.0,
           BD_U(BD_DUCK) / 100.0, BD_U(BD_KTRK) / 100.0, BD_U(BD_MX_SLOT) / 100.0, BD_U(BD_LFO) / 100.0, BD_U(BD_ARP) / 100.0);
    for (m = 0; m < 2u; m++) {
        printf("budget: %s: idle mix (sends off) %.1f; sends on, nothing fed (HALL + DIGITAL) +%.1f; chorus +%.1f; delay DIGITAL %+.1f PING-PONG %+.1f RANDOM %+.1f OCT UP %+.1f; reverb ROOM %+.1f SPRING %+.1f HALL %+.1f SHIM %+.1f RESO %+.1f PLATE %+.1f, SHIM DOWN %+.1f, SHIM + MOD %+.1f\n",
               m ? "DUEL" : "STEREO/LAYER", BD_U(BD_IDLE[m]) / 100.0, BD_U(BD_BUS[m]) / 100.0, (BD_U(BD_CHO[m]) - BD_U(BD_BUS[m])) / 100.0,
               ((int)BD_U(BD_DLY[m][0]) - (int)BD_U(BD_BUS[m])) / 100.0, ((int)BD_U(BD_DLY[m][1]) - (int)BD_U(BD_BUS[m])) / 100.0, ((int)BD_U(BD_DLY[m][2]) - (int)BD_U(BD_BUS[m])) / 100.0,
               ((int)BD_U(BD_DLY[m][3]) - (int)BD_U(BD_BUS[m])) / 100.0, ((int)BD_U(BD_REV[m][0]) - (int)BD_U(BD_BUS[m])) / 100.0, ((int)BD_U(BD_REV[m][1]) - (int)BD_U(BD_BUS[m])) / 100.0,
               ((int)BD_U(BD_REV[m][2]) - (int)BD_U(BD_BUS[m])) / 100.0, ((int)BD_U(BD_REV[m][3]) - (int)BD_U(BD_BUS[m])) / 100.0, ((int)BD_U(BD_REV[m][4]) - (int)BD_U(BD_BUS[m])) / 100.0,
               ((int)BD_U(BD_REV[m][5]) - (int)BD_U(BD_BUS[m])) / 100.0, ((int)BD_U(BD_SHD[m]) - (int)BD_U(BD_BUS[m])) / 100.0, ((int)BD_U(BD_SHM[m]) - (int)BD_U(BD_BUS[m])) / 100.0);
    }
    printf("budget: master (STEREO/LAYER only): COMP %.1f, WIDTH %.1f, BASS MONO %.1f\n", BD_U(BD_COMP) / 100.0, BD_U(BD_WIDTH) / 100.0, BD_U(BD_BMONO) / 100.0);
}

static void t_model(void)
{
    static const char *const ENG[10] = {"ANALOG", "FM6", "PHASE", "LOFI", "VOICE", "TRIO", "WHEEL", "PHYS", "NOISE", "WAVE"};
    static const char *const BN[18] = {"nothing fed (HALL, DIGITAL: power-on)", "CHORUS (STEREO: the stereo chorus)", "DIGITAL", "PING-PONG", "RANDOM", "OCT UP",
                                       "ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE", "SHIM PITCH DOWN", "SHIM PITCH UP + MOD 50"};
    static const struct { int cho, dly, rev, dmode, rtype, shp, shm; } BS[14] = {
        {0, 0, 0, -1, -1, -1, -1}, {100, 0, 0, -1, -1, -1, -1},
        {0, 100, 0, 0, -1, -1, -1}, {0, 100, 0, 1, -1, -1, -1}, {0, 100, 0, 2, -1, -1, -1}, {0, 100, 0, 3, -1, -1, -1},
        {0, 0, 100, -1, 0, SHP_UP, 0}, {0, 0, 100, -1, 1, SHP_UP, 0}, {0, 0, 100, -1, 2, SHP_UP, 0}, {0, 0, 100, -1, 3, SHP_UP, 0},
        {0, 0, 100, -1, 4, SHP_UP, 0}, {0, 0, 100, -1, 5, SHP_UP, 0}, {0, 0, 100, -1, 3, SHP_DOWN, 0}, {0, 0, 100, -1, 3, SHP_UP, 50}};
    uint32_t i, ok = 1, n = 0;
    char nm[96], s1[40];
    double b;
    power();
    clean_globals(DM_STEREO);
    for (i = 0; i < NELEM(ENG); i++) {                              /* one voice of each engine */
        neutral(&trk[0], eng_named(ENG[i]));
        trk[0].p[P_KTRK] = 32;                                      /* (the factory default: the 12 dB filter runs, as in the bench's sounds) */
        snprintf(nm, sizeof nm, "voice/%s", ENG[i]);
        b = base_get(nm);
        if (b > 0) {                                                /* (cloud job J: the bench rows are of the engines as they were: ANALOG, TRIO, NOISE less their own filter (est.), every engine plus the chain path CHAIN_ALL puts a note with filter 1 on (est.)) */
            b += (CHAIN_ALL || !strcmp(ENG[i], "ANALOG") || !strcmp(ENG[i], "TRIO") || !strcmp(ENG[i], "NOISE")) ? BD_TW_CHAIN : 0;
            b -= !strcmp(ENG[i], "ANALOG") ? BD_RM_ANALOG : !strcmp(ENG[i], "TRIO") ? BD_RM_TRIO : !strcmp(ENG[i], "NOISE") ? BD_RM_NOISE : 0;
        }
        ok &= b > 0 && within(nm, raw(part_cost(&trk[0])), b, 0.05);
        n++;
    }
    bchk("model: one voice of each of the 10 engines within 5 % of the bench", ok);
    ok = 1;
    for (i = 1; i < NELEM(N_ITYPE); i++) {                          /* the INSERT types */
        neutral(&trk[0], eng_named("ANALOG"));
        trk[0].p[P_ITYPE] = (int16_t)i;
        snprintf(nm, sizeof nm, "insert/%s", N_ITYPE[i]);
        b = base_get(nm);
        ok &= b > 0 && within(nm, raw(part_fixed(&trk[0])), b, 0.05);
    }
    bchk("model: every INSERT type within 5 % of the bench", ok);
    ok = 1;
    neutral(&trk[0], eng_named("ANALOG"));
    trk[0].p[P_DIST] = 64;
    ok &= within("part/SAT", raw(part_fixed(&trk[0])), base_get("part/SAT"), 0.05);
    neutral(&trk[0], eng_named("ANALOG"));
    trk[0].p[P_EQLG] = 5;
    trk[0].p[P_EQMG] = -4;
    trk[0].p[P_EQHG] = 6;
    ok &= within("part/EQ", raw(part_fixed(&trk[0])), base_get("part/EQ"), 0.05);
    neutral(&trk[0], eng_named("ANALOG"));
    trk[0].p[P_DKDEP] = 90;
    ok &= within("part/DUCK", raw(part_fixed(&trk[0])), base_get("part/DUCK"), 0.05);
    bchk("model: SAT, the 3-band EQ and the ducker within 5 % of the bench", ok);
    {   /* the buses: the whole bus section against SENDS OFF, STEREO and DUEL */
        uint32_t m;
        ok = 1;
        for (m = 0; m < 2u; m++)
            for (i = 0; i < NELEM(BS); i++) {
                double idle_off, v;
                clean_globals(m ? DM_SPLIT : DM_STEREO);
                trk[0].p[P_CHOR] = trk[1].p[P_CHOR] = (int16_t)BS[i].cho;
                trk[0].p[P_DLY] = trk[1].p[P_DLY] = (int16_t)BS[i].dly;
                trk[0].p[P_REV] = trk[1].p[P_REV] = (int16_t)BS[i].rev;
                if (BS[i].dmode >= 0) song.g[G_DMODE] = (int16_t)BS[i].dmode;
                if (BS[i].rtype >= 0) song.g[G_RTYPE] = (int16_t)BS[i].rtype;
                if (BS[i].shp >= 0) song.g[G_SHPITCH] = (int16_t)BS[i].shp;
                if (BS[i].shm >= 0) song.g[G_SHMOD] = (int16_t)BS[i].shm;
                mods_off = MODS_SENDS;
                idle_off = raw(global_cost());
                mods_off = 0;
                v = raw(global_cost()) - idle_off;
                slug(s1, BN[i], 32);
                snprintf(nm, sizeof nm, "bus/%s/%s", m ? "DUEL" : "STEREO", s1);
                b = base_get(nm);
                ok &= b > 0 && within(nm, v, b, 0.05);
            }
        bchk("model: every bus row (chorus, 4 delay modes, 6 reverbs, SHIM DOWN / MOD) in STEREO and DUEL within 5 % of the bench", ok);
    }
    {   /* the master stages (STEREO) */
        double g0, v;
        clean_globals(DM_STEREO);
        g0 = raw(global_cost());
        ok = 1;
        cmp_cfg = 4u << 8;
        v = raw(global_cost()) - g0;
        ok &= within("master/COMP", v, base_get("master/COMP"), 0.05);
        cmp_cfg = 0;
        wd_sel = 150;
        v = raw(global_cost()) - g0;
        ok &= within("master/WIDTH", v, base_get("master/WIDTH"), 0.05);
        wd_sel = 100;
        bm_sel = 5;
        v = raw(global_cost()) - g0;
        ok &= within("master/BASSMONO", v, base_get("master/BASSMONO"), 0.05);
        bm_sel = 0;
        clean_globals(DM_SPLIT);
        cmp_cfg = 4u << 8;
        wd_sel = 150;
        bm_sel = 5;
        bchk("model: COMP, WIDTH and BASS MONO within 5 % of the bench", ok);
        {   /* none of them runs in DUEL */
            uint32_t a = global_cost();
            cmp_cfg = 0;
            wd_sel = 100;
            bm_sel = 0;
            bchk("model: the master stages cost nothing in DUEL", a == global_cost());
        }
    }
    {   /* the worst-case scene of each MODE (cpu_bench.c SCENE) */
        static const int MODES[3] = {DM_LAYER, DM_STEREO, DM_SPLIT};
        uint32_t m, e = eng_named("PHYS"), k = sound_named(e, "PAN WHISTLE"), p, okf = 1, okt = 1;
        for (m = 0; m < 3u; m++) {
            double F, V, c1, c4, f0, v0;
            clean_globals((uint32_t)MODES[m]);
            for (p = 0; p < 2u; p++) {
                track_t *t = &trk[p];
                t->eng_req = (uint8_t)e;
                apply_preset_to(t, k);
                t->p[P_ITYPE] = 6;                                  /* PHASR, the dearest */
                t->p[P_IA] = t->p[P_IB] = t->p[P_IC] = 64;
                t->p[P_DIST] = 64;
                t->p[P_EQLG] = 5, t->p[P_EQMG] = -4, t->p[P_EQHG] = 6;
                t->p[P_DKDEP] = 90;
                t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 100;
                t->p[P_AMODE] = AM_CHORD;
                t->p[P_MUTE] = 0;
                mph[p].nid = mph[p].fnid = 0;
            }
            song.g[G_DMODE] = DMD_RAND, song.g[G_RTYPE] = 3, song.g[G_SHPITCH] = SHP_UP, song.g[G_SHMOD] = 50;
            cmp_cfg = 4u << 8;
            wd_sel = 150;
            F = raw(global_cost() + part_fixed(&trk[0]) + part_fixed(&trk[1]));
            V = raw(part_cost(&trk[0]));
            c1 = F + 2.0 * V;
            c4 = F + 6.0 * V;                                       /* (PHYS plays 3 a part: the bench's 4 + 4 sound 6 voices) */
            snprintf(nm, sizeof nm, "scene/%s/fixed", MODE_NAME[MODES[m]]);
            f0 = base_get(nm);
            okf &= within(nm, F, f0, 0.05);
            snprintf(nm, sizeof nm, "scene/%s/1+1", MODE_NAME[MODES[m]]);
            v0 = base_get(nm) + (CHAIN_ALL ? 2.0 * BD_TW_CHAIN : 0.0);   /* (cloud job J: the bench is of the code before CHAIN_ALL; each voice, whose filter runs, takes the chain path now: est. BD_TW_CHAIN) */
            okt &= within(nm, c1, v0, 0.12) && c1 >= v0 * 0.995;
            snprintf(nm, sizeof nm, "scene/%s/4+4", MODE_NAME[MODES[m]]);
            v0 = base_get(nm) + (CHAIN_ALL ? 6.0 * BD_TW_CHAIN : 0.0);
            okt &= within(nm, c4, v0, 0.12) && c4 >= v0 * 0.995;
            printf("budget:   scene %-6s fixed model %6.0f bench %6.0f | 1+1 model %6.0f | 4+4 (6 voices) model %6.0f bench %6.0f | a voice %4.0f (scene bench %4.0f)\n",
                   MODE_NAME[MODES[m]], F, f0, c1, c4, v0, V, base_get("scene/LAYER/voice"));
        }
        bchk("model: the worst-case scene of each MODE, fixed part within 5 % of the bench", okf);
        bchk("model: .. and its totals at 1 + 1 and 4 + 4 notes never below the bench, within 12 % over it", okt);
    }
    (void)n;
}

/* ---- the factory multis ---- */
static void load_multi(uint32_t i)
{
    power();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(i);
    frame();
}
static const char *why_name(uint32_t r)
{
    static const char *const W[NR_N] = {"-", "ENGINE", "24 DB", "INSERT", "SAT", "EQ", "EFFECTS", "OTHER PART", "STAGES"};
    return r < NR_N ? W[r] : "?";
}
typedef struct {
    uint32_t n[2], w[2], oc[2], glob, fix[2], cost[2], mode, nid[2], eng[2];
} frow_t;
static void frow_get(frow_t *r)
{
    uint32_t p;
    vcap_poll();
    for (p = 0; p < 2u; p++) {
        r->n[p] = part_notes((int)p);
        r->w[p] = part_notes_reason((int)p);
        r->oc[p] = vcap_shown(p);
        r->fix[p] = part_fixed(&trk[p]);
        r->cost[p] = part_cost(&trk[p]);
        r->nid[p] = mph[p].nid + 100u * (mph[p].fnid != 0);
        r->eng[p] = eng_idx(trk[p].eng_req);
        if (part_notes_shown((int)p) != r->n[p])
            r->w[p] = NR_N + 1u;
    }
    r->glob = global_cost();
    r->mode = duell_mode_req;
}
/* a multi in a child: [0] as loaded (frame() after the load), [1] in cpuscen's state (40 halves settled, MORPH target B, the fader in the middle) */
static void frow_child(uint32_t i, frow_t *rows)
{
    int32_t o[2 * CTL];
    uint32_t b;
    load_multi(i);
    frow_get(&rows[0]);
    for (b = 0; b < 40u * HALF_FRAMES / CTL; b++) {
        mix_block(o, CTL);
        while (wave_service())
            ;
    }
    morph_set_target(1);
    morph_set_fader(64);
    frow_get(&rows[1]);
}
static void frow_hash(uint32_t i, frow_t *rows)
{
    int fd[2];
    pid_t pid;
    int st;
    memset(rows, 0, 2u * sizeof *rows);
    if (pipe(fd))
        return;
    pid = fork();
    if (!pid) {
        frow_t r[2];
        frow_child(i, r);
        if (write(fd[1], r, sizeof r) != (ssize_t)sizeof r)
            _exit(1);
        _exit(0);
    }
    if (read(fd[0], rows, 2u * sizeof *rows) != (ssize_t)(2u * sizeof *rows))
        memset(rows, 0, 2u * sizeof *rows);
    waitpid(pid, &st, 0);
    close(fd[0]);
    close(fd[1]);
}
static void t_factory(void)
{
    uint32_t i, p, st, below4[2] = {0, 0}, old_stricter[2] = {0, 0}, bad = 0;
    printf("budget: --- the 16 factory multis (MODE RECALL): NOTES a part (budget.c, one core, %u %% of the time, margin %u %%), what limits it, the VOICE CAP of 0.2.11 beside it.\n"
           "budget:     'loaded' = as loaded; 'MORPH' = cpuscen's state (settled, MORPH on End B, fader in the middle): the morph adds a little to each voice\n", BUDGET_LIMIT, BUDGET_MARGIN);
    for (st = 0; st < 2u; st++) {
        printf("budget: [%s] %-3s %-14s %-6s %-7s %-7s %8s %8s %8s %7s %7s  %-12s %-12s %s\n", st ? "MORPH " : "loaded", "no", "name", "mode", "part 1", "part 2", "global %", "fixed 1", "fixed 2",
               "voice 1", "voice 2", "NOTES 1", "NOTES 2", "old cap");
        for (i = 0; i < MU_SLOTS; i++) {
            frow_t rows[2];
            const frow_t *r;
            char a[24], b[24];
            frow_hash(i, rows);
            r = &rows[st];
            snprintf(a, sizeof a, "%u %s", r->n[0], why_name(r->w[0]));
            snprintf(b, sizeof b, "%u %s", r->n[1], why_name(r->w[1]));
            printf("budget: [%s] M%02u %-14s %-6s %-7s %-7s %8.1f %8.1f %8.1f %7.1f %7.1f  %-12s %-12s %u / %u%s\n", st ? "MORPH " : "loaded", i + 1, mu_names[i], MODE_NAME[r->mode % 3u],
                   ENGINES[r->eng[0]]->name, ENGINES[r->eng[1]]->name, r->glob / 100.0, r->fix[0] / 100.0, r->fix[1] / 100.0, r->cost[0] / 100.0, r->cost[1] / 100.0, a, b, r->oc[0], r->oc[1],
                   r->oc[0] < r->n[0] || r->oc[1] < r->n[1] ? "   (old cap stricter)" : "");
            for (p = 0; p < 2u; p++) {
                bad += r->n[p] < 1u || r->n[p] > NOTES_MAX || r->n[p] > NVOICE || r->w[p] >= NR_N;
                below4[st] += r->n[p] < 4u;
                old_stricter[st] += r->oc[p] < r->n[p];
            }
        }
        printf("budget: [%s] %u of 32 parts get fewer than 4 notes (the old cap never went below 4); %u parts where the old cap is stricter than the budget\n", st ? "MORPH " : "loaded",
               below4[st], old_stricter[st]);
    }
    bchk("factory: every part gets 1 .. min(NOTES_MAX, pool) notes, a valid reason, shown == notes (both states)", !bad);
}

/* ---- monotone ---- */
static uint32_t rnd_state = 0x9E3779B9u;
static uint32_t rnd(void) { rnd_state ^= rnd_state << 13; rnd_state ^= rnd_state >> 17; rnd_state ^= rnd_state << 5; return rnd_state; }
typedef struct { const char *what; void (*on)(track_t *t); } bstep_t;
static void s_ins(track_t *t) { t->p[P_ITYPE] = 6; }
static void s_f24(track_t *t) { t->p[P_FREEA] = 52; t->p[P_FREEB] = 20; t->p[P_FREEC] = 0; t->p[P_FREED] = 2; }
static void s_sat(track_t *t) { t->p[P_DIST] = 64; }
static void s_eq(track_t *t) { t->p[P_EQMG] = -4; }
static void s_duck(track_t *t) { t->p[P_DKDEP] = 90; }
static void s_mx(track_t *t) { t->p[P_M1SRC] = MS_LFO; t->p[P_M1DST] = MD_CUT; t->p[P_M1AMT] = 30; }
static void s_twin(track_t *t) { t->p[P_TWTYPE] = TT_LADDER; t->p[P_DRV2] = 64; XV(t, X_ROUTE) = 1; XV(t, X_LFO1) = 20; }
static const bstep_t STEPS[] = {{"INSERT", s_ins}, {"24 dB slope", s_f24}, {"SAT", s_sat}, {"EQ band", s_eq}, {"ducker", s_duck}, {"matrix slot", s_mx}, {"TWIN filter 2", s_twin}};
static void g_rand(void) { song.g[G_DMODE] = DMD_RAND; }
static void g_shim(void) { song.g[G_RTYPE] = 3; song.g[G_SHPITCH] = SHP_DOWN; song.g[G_SHMOD] = 50; }
static void g_comp(void) { cmp_cfg = 4u << 8; }
static void g_width(void) { wd_sel = 150; }
static void g_cho(void) { trk[0].p[P_CHOR] = trk[1].p[P_CHOR] = 100; }
static const struct { const char *what; void (*on)(void); } GSTEPS[] = {{"RANDOM delay", g_rand}, {"SHIM DOWN", g_shim}, {"COMP", g_comp}, {"WIDTH", g_width}, {"chorus send", g_cho}};

static uint32_t t_mono_viol, t_mono_other, t_mono_n;
static void mono_step(uint32_t p, void (*part_on)(track_t *), void (*glob_on)(void))
{
    int16_t keep[2][sizeof trk[0].p / sizeof(int16_t)];
    uint32_t q, a[2], b[2];
    uint32_t cmp0 = cmp_cfg, wd0 = wd_sel;
    int16_t g0[G_COUNT];
    memcpy(keep[0], trk[0].p, sizeof keep[0]);
    memcpy(keep[1], trk[1].p, sizeof keep[1]);
    memcpy(g0, song.g, sizeof g0);
    a[0] = part_notes(0);
    a[1] = part_notes(1);
    if (part_on)
        part_on(&trk[p]);
    if (glob_on)
        glob_on();
    b[0] = part_notes(0);
    b[1] = part_notes(1);
    t_mono_n++;
    for (q = 0; q < 2u; q++)
        if (b[q] > a[q]) {
            if (glob_on || q == p) {
                t_mono_viol++;
                printf("budget:   RAISED part %u: %u -> %u\n", q + 1u, a[q], b[q]);
            } else {
                t_mono_other++;                         /* the other part gets the notes the changed one gave up: allowed */
            }
        }
    memcpy(trk[0].p, keep[0], sizeof keep[0]);
    memcpy(trk[1].p, keep[1], sizeof keep[1]);
    memcpy(song.g, g0, sizeof g0);
    cmp_cfg = cmp0;
    wd_sel = (uint8_t)wd0;
}
static void t_monotone(void)
{
    uint32_t i, p, k, it;
    static const char *const ENG[10] = {"ANALOG", "FM6", "PHASE", "LOFI", "VOICE", "TRIO", "WHEEL", "PHYS", "NOISE", "WAVE"};
    t_mono_viol = t_mono_other = t_mono_n = 0;
    for (i = 0; i < MU_SLOTS; i++) {
        load_multi(i);
        for (p = 0; p < 2u; p++) {
            for (k = 0; k < NELEM(STEPS); k++)
                mono_step(p, STEPS[k].on, 0);
            for (k = 0; k < NELEM(GSTEPS); k++)
                mono_step(p, 0, GSTEPS[k].on);
        }
    }
    printf("budget: factory multis: %u steps (7 part items x 2 parts and 5 effects x 2) x 16: raises of the changed part %u, of the other part %u\n", t_mono_n, t_mono_viol, t_mono_other);
    bchk("monotone: on the 16 multis, switching on an INSERT, 24 dB, SAT, an EQ band, the ducker, a matrix slot, filter 2 (TWIN), RANDOM, SHIM DOWN, COMP, WIDTH or the chorus never raises the changed part's number", !t_mono_viol);
    t_mono_viol = t_mono_other = t_mono_n = 0;
    power();
    for (it = 0; it < 5000u; it++) {
        clean_globals(rnd() % 3u == 0 ? DM_SPLIT : rnd() % 2u ? DM_STEREO : DM_LAYER);
        for (p = 0; p < 2u; p++) {
            track_t *t = &trk[p];
            neutral(t, eng_named(ENG[rnd() % 10u]));
            if (rnd() % 4u == 0) t->p[P_MUTE] = 1;
            if (rnd() % 3u == 0) s_ins(t), t->p[P_ITYPE] = (int16_t)(1 + rnd() % 10u);
            if (rnd() % 3u == 0) s_f24(t), t->p[P_FREED] = (int16_t)(1 + rnd() % 2u);
            if (rnd() % 3u == 0) s_sat(t);
            if (rnd() % 3u == 0) t->p[P_EQLG] = 3;
            if (rnd() % 3u == 0) s_eq(t);
            if (rnd() % 4u == 0) s_duck(t);
            if (rnd() % 4u == 0) s_mx(t);
            t->p[P_CHOR] = (int16_t)(rnd() % 2u * 60);
        }
        if (rnd() % 3u == 0) g_rand();
        if (rnd() % 3u == 0) g_comp();
        if (rnd() % 3u == 0) song.g[G_RTYPE] = (int16_t)(rnd() % 6u);
        if (rnd() % 5u == 0) mods_off = MODS_SENDS;
        p = rnd() % 2u;
        mono_step(p, STEPS[rnd() % NELEM(STEPS)].on, 0);
        mono_step(p, 0, GSTEPS[rnd() % NELEM(GSTEPS)].on);
    }
    printf("budget: 5000 random settings: %u steps, raises of the changed part %u, of the other part %u\n", t_mono_n, t_mono_viol, t_mono_other);
    bchk("monotone: the same on 5000 random settings (two parts, any engine, mutes, DUEL / STEREO / LAYER, SENDS OFF)", !t_mono_viol);
    mods_off = 0;
}

/* ---- steal ---- */
static void (*const ST_ITEMS[])(track_t *) = {s_ins, s_f24, s_sat, s_eq, s_duck, s_mx};
static uint32_t g_items;                                           /* the settings run: how many of ST_ITEMS are on, both parts */
static uint32_t held_voices(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active != 0;
    return n;
}
/* part 1 a plain sine pad (regress.c's steal-fade rig), one new note every 0.15 s up to 12 notes, none released; the budget of part 1 as bnotes[0] holds it */
static void steal_run(const char *label, uint32_t want, int settings)
{
    const uint32_t frames = 3u * FS, gap = (uint32_t)(0.15 * FS) / CTL * CTL;
    int32_t *L = calloc(frames + CTL, sizeof *L);
    uint32_t f, i, k, nn = 0, vmax = 0, kills0 = voice_kills, take[16], nt = 0, bad = 0, steals = 0;
    double worst = 0;
    int32_t o[2 * CTL], wat = 0, wcalm = 0;
    power();
    host_tracks_init();                                             /* (regress.c's rig: the defaults, no sends, a plain sine) */
    xfade_sine(&trk[0]);
    xfade_sine(&trk[1]);
    trk[1].p[P_MUTE] = 1;
    mods_off = 0;
    if (settings) {                                                 /* the budget from the settings, handed over as main.c does */
        for (i = 0; i < g_items; i++)
            ST_ITEMS[i](&trk[0]), ST_ITEMS[i](&trk[1]);
        want = part_notes(0);
        budget_poll();
    } else {
        bnotes[0] = (uint8_t)want;
    }
    for (f = 0; f < frames; f += CTL) {
        if (f % gap == 0 && nn < 12u) {
            if (nn >= want && f >= FS / 2u && nt < NELEM(take))
                take[nt++] = f;
            trk_note_on(&trk[0], 48u + 3u * nn, 100);
            nn++;
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            L[f + i] = o[2 * i];
        k = held_voices(&trk[0]);
        vmax = k > vmax ? k : vmax;
    }
    for (k = 0; k < nt; k++) {
        int32_t calm = 1, a = 0;
        for (i = take[k] - FS / 4u; i < take[k] - 2u * CTL; i++)
            calm = abs(L[i] - L[i - 1]) > calm ? abs(L[i] - L[i - 1]) : calm;
        for (i = take[k]; i < take[k] + 2u * CTL; i++)
            a = abs(L[i] - L[i - 1]) > a ? abs(L[i] - L[i - 1]) : a;
        if ((double)a / calm > worst) {
            worst = (double)a / calm;
            wat = a;
            wcalm = calm;
        }
        bad += a * 2 > calm * 3;
    }
    steals = nt;
    {
        char b[200];
        snprintf(b, sizeof b, "steal (%s): 12 note-ons on a part with %u notes: at most %u voices at any block, %u steals (%u fast kills), largest step at a steal %d vs %d before it (%.2f x, limit 1.50 x)",
                 label, want, vmax, steals, voice_kills - kills0, wat, wcalm, worst);
        bchk(b, vmax <= want && steals >= 4u && !bad);
    }
    free(L);
    bnotes[0] = bnotes[1] = 0;
}
/* a part holds 8 notes, then its number drops to 3 (an item switched on): its next note-on fades the voices above 3 out in one block, and no more than 3 sound after */
static void trim_run(void)
{
    const uint32_t frames = 2u * FS;
    int32_t *L = calloc(frames + CTL, sizeof *L), o[2 * CTL], calm = 1, a = 0;
    uint32_t f, i, at = 0, k0, after = 99u, held0 = 0, kills = 0, sounding_after = 0, kept = 0;
    power();
    host_tracks_init();
    xfade_sine(&trk[0]);
    xfade_sine(&trk[1]);
    trk[1].p[P_MUTE] = 1;
    bnotes[0] = bnotes[1] = 0;
    vcap[0] = vcap[1] = 0;                                      /* (DUELL 1.0: the worst-case voice cap (vcap_poll) was made for the power-on sounds, now twin-filter and SUB sounds of the owner's bank: it gave part 1 7 voices; this run is about the NOTES budget, so no cap) */
    for (f = 0; f < frames; f += CTL) {
        if (f == 0)
            for (i = 0; i < 8u; i++)
                trk_note_on(&trk[0], 48u + 4u * i, 100);
        if (f == (uint32_t)(0.8 * FS) / CTL * CTL) {
            held0 = held_voices(&trk[0]);
            bnotes[0] = 3;                                          /* the budget drops: nothing is cut yet */
        }
        if (f == (uint32_t)(1.0 * FS) / CTL * CTL) {
            at = f;
            k0 = voice_kills;
            trk_note_on(&trk[0], 90, 100);
            kept = trk[0].v[0].active;
            kills = voice_kills - k0;
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            L[f + i] = o[2 * i];
        if (at && f == at + 8u * CTL) {
            after = 0;
            for (i = 0; i < NVOICE; i++)
                after += trk[0].v[i].active && trk[0].v[i].stage != 4u;
            sounding_after = after;
        }
    }
    for (i = at - FS / 4u; i < at - 2u * CTL; i++)
        calm = abs(L[i] - L[i - 1]) > calm ? abs(L[i] - L[i - 1]) : calm;
    for (i = at; i < at + 3u * CTL; i++)
        a = abs(L[i] - L[i - 1]) > a ? abs(L[i] - L[i - 1]) : a;
    {
        char b[200];
        snprintf(b, sizeof b, "trim: 8 notes held, the number drops to 3, the next note-on fades %u voices out (%u held before it, %u sound 8 blocks later), largest step %d vs %d before (%.2f x, limit 1.50 x)",
                 kills, held0, sounding_after, a, calm, (double)a / calm);
        bchk(b, held0 == 8u && kills == 5u && sounding_after <= 3u && a * 2 <= calm * 3 && kept);
    }
    free(L);
    bnotes[0] = bnotes[1] = 0;
}
static void t_steal(void)
{
    uint32_t p, k, n, found = 0;
    void (*const *ST)(track_t *) = ST_ITEMS;
    steal_run("budget 4 given", 4u, 0);
    steal_run("budget 1 given", 1u, 0);
    trim_run();
    /* the budget from the settings: ANALOG on both parts, items switched on one after the other (both parts) until part 1 has 4 notes or fewer */
    power();
    clean_globals(DM_STEREO);
    for (k = 0; k < NELEM(ST) + 4u && !found; k++) {
        n = part_notes(0);
        printf("budget:   steal setup: step %u: part 1 gets %u notes\n", k, n);
        if (n <= 4u && n >= 2u) {
            found = 1;
            break;
        }
        for (p = 0; p < 2u; p++)
            if (k < NELEM(ST))
                ST[k](&trk[p]);
        if (k == NELEM(ST))
            g_rand();
        if (k == NELEM(ST) + 1u)
            g_shim();
        if (k == NELEM(ST) + 2u)
            g_comp();
    }
    n = part_notes(0);
    printf("budget: steal from settings: ANALOG on both parts with %u items on: part 1 gets %u notes (%s)\n", k, n, why_name(part_notes_reason(0)));
    g_items = k;
    steal_run("budget from the settings", 0, 1);
}

/* ---- bit for bit ---- */
static const uint8_t N8[8] = {48, 52, 55, 59, 62, 65, 69, 72};
/* the multi i loaded, the notes held on its parts (which: 0 the budget's numbers, 1 four on each), 1.2 s of audio hashed; the budget handed over (as main.c does) or not.
 * The firmware keeps generator states (the noise, the random draws) that a load does not reset, so every render runs in a child process of its own: the same
 * start state, the same bits (the parent never renders) */
static uint64_t render_child(uint32_t i, int which, int budget, uint32_t *kills, uint32_t *peak, uint32_t *nout)
{
    uint64_t h = 1469598103934665603ull;
    int32_t o[2 * CTL];
    uint32_t b, p, k, k0, n[2];
    load_multi(i);
    song.master_q12 = 2048;                                         /* (the MASTER at its default, as cpu_bench.c) */
    for (b = 0; b < 40u * HALF_FRAMES / CTL; b++) {                /* (cpuscen's way: the engine switch and the mode settle first) */
        mix_block(o, CTL);
        while (wave_service())
            ;
    }
    morph_set_target(1);
    morph_set_fader(64);
    n[0] = which ? 4u : part_notes(0);                              /* (in cpuscen's state: the MORPH on, the fader in the middle) */
    n[1] = which ? 4u : part_notes(1);
    nout[0] = part_notes(0), nout[1] = part_notes(1);
    bnotes[0] = bnotes[1] = 0;
    if (budget)
        budget_poll();
    for (p = 0; p < 2u; p++)
        for (k = 0; k < n[p]; k++)
            trk_note_on(&trk[p], (uint8_t)(N8[k] + 7u * p), 110);
    k0 = voice_kills;
    for (b = 0; b < 160u * HALF_FRAMES / CTL; b++) {
        mix_block(o, CTL);
        while (wave_service())
            ;
        for (p = 0; p < 2u; p++) {                                  /* (the voices that sound, the ones fading out for another part not counted) */
            uint32_t v, c = 0;
            for (v = 0; v < NVOICE; v++)
                c += trk[p].v[v].active && trk[p].v[v].stage != 4u;
            peak[p] = c > peak[p] ? c : peak[p];
        }
        if (b >= 80u * HALF_FRAMES / CTL)
            for (k = 0; k < 2u * CTL; k++)
                h = (h ^ (uint32_t)o[k]) * 1099511628211ull;
    }
    *kills = voice_kills - k0;
    return h;
}
static uint64_t render_hash(uint32_t i, int which, int budget, uint32_t *kills, uint32_t *peak, uint32_t *nn)
{
    int fd[2];
    uint64_t r[6] = {0, 0, 0, 0, 0, 0};
    pid_t pid;
    int st;
    if (pipe(fd))
        return 0;
    pid = fork();
    if (!pid) {
        uint32_t pk[2] = {0, 0}, kl = 0, nb[2] = {0, 0};
        r[0] = render_child(i, which, budget, &kl, pk, nb);
        r[1] = kl, r[2] = pk[0], r[3] = pk[1], r[4] = nb[0], r[5] = nb[1];
        if (write(fd[1], r, sizeof r) != (ssize_t)sizeof r)
            _exit(1);
        _exit(0);
    }
    if (read(fd[0], r, sizeof r) != (ssize_t)sizeof r)
        r[0] = 0;
    waitpid(pid, &st, 0);
    close(fd[0]);
    close(fd[1]);
    *kills = (uint32_t)r[1];
    peak[0] = (uint32_t)r[2];
    peak[1] = (uint32_t)r[3];
    nn[0] = (uint32_t)r[4];
    nn[1] = (uint32_t)r[5];
    return r[0];
}
static void t_bits(void)
{
    uint32_t i, bad = 0, diff44 = 0, nrep = 0, nover = 0;
    printf("budget: --- bit for bit: each multi with the notes its budget allows held, and with 4 + 4 notes (cpuscen's scene), the budget on and off (peak = the most voices\n"
           "budget:     that sounded without the budget: an arp's release tails hold voices beyond the held notes)\n");
    for (i = 0; i < MU_SLOTS; i++) {
        uint32_t k, k4, pa[2], pb[2], pc[2], pd[2], n[2], over;
        uint64_t a, a2, b, c, d;
        a = render_hash(i, 0, 0, &k, pa, n);
        a2 = render_hash(i, 0, 0, &k, pb, n);                         /* (the render itself must repeat) */
        b = render_hash(i, 0, 1, &k, pb, n);
        c = render_hash(i, 1, 0, &k, pc, n);
        d = render_hash(i, 1, 1, &k4, pd, n);
        over = pa[0] > n[0] || pa[1] > n[1];
        nrep += a != a2;
        diff44 += c != d;
        nover += over;
        bad += a != b && !over;
        printf("budget:   M%02u %-14s the budget's %u + %u notes (peak %u + %u): %s | 4 + 4 notes (peak %u + %u): %s\n", i + 1, mu_names[i], n[0], n[1], pa[0], pa[1],
               a == b ? "bit for bit" : over ? "changes (the voices that sound are over the budget)" : "DIFFERENT", pc[0], pc[1], c == d ? "bit for bit" : "changes");
    }
    printf("budget: %u of 16 multis keep 4 + 4 notes bit for bit, %u change (a part has fewer than 4 notes there); %u sound more voices than held notes allow for\n", 16u - diff44, diff44, nover);
    bchk("bit for bit: a render repeats exactly (each in a child process)", !nrep);
    bchk("bit for bit: where no more voices sound than the budget allows, the render is the same with the budget on or off (all 16 multis)", !bad);
}

/* ---- pure ---- */
static void t_pure(void)
{
    uint32_t i, p, bad = 0, it;
    static const char *const ENG[10] = {"ANALOG", "FM6", "PHASE", "LOFI", "VOICE", "TRIO", "WHEEL", "PHYS", "NOISE", "WAVE"};
    for (i = 0; i < MU_SLOTS; i++) {
        uint32_t a0, a1, r0, r1, b0, b1, s0, s1;
        load_multi(i);
        a0 = part_notes(0), a1 = part_notes(1), r0 = part_notes_reason(0), r1 = part_notes_reason(1);
        b1 = part_notes(1), b0 = part_notes(0);                  /* the other order */
        (void)part_cost(&trk[0]), (void)part_fixed(&trk[1]), (void)global_cost();
        s0 = part_notes(0), s1 = part_notes(1);
        bad += a0 != b0 || a1 != b1 || a0 != s0 || a1 != s1;
        bad += r0 != part_notes_reason(0) || r1 != part_notes_reason(1);
        {   /* the history: INSERT, 24 dB, SAT on and off again; the effects changed and put back */
            int16_t keep[2][sizeof trk[0].p / sizeof(int16_t)];
            int16_t g0[G_COUNT];
            memcpy(keep[0], trk[0].p, sizeof keep[0]);
            memcpy(keep[1], trk[1].p, sizeof keep[1]);
            memcpy(g0, song.g, sizeof g0);
            s_ins(&trk[0]), s_f24(&trk[1]), s_sat(&trk[0]), g_rand(), g_shim();
            (void)part_notes(1), (void)part_notes(0);
            memcpy(trk[0].p, keep[0], sizeof keep[0]);
            memcpy(trk[1].p, keep[1], sizeof keep[1]);
            memcpy(song.g, g0, sizeof g0);
            bad += part_notes(0) != a0 || part_notes(1) != a1 || part_notes_reason(0) != r0 || part_notes_reason(1) != r1;
        }
        budget_poll();
        bad += bnotes[0] != a0 || bnotes[1] != a1;
        budget_poll();
        bad += bnotes[0] != a0 || bnotes[1] != a1;
        bnotes[0] = bnotes[1] = 0;
    }
    bchk("pure: the 16 multis give the same numbers and reasons in either order of calls, after other calls, after a setting is switched on and off again, and in budget_poll", !bad);
    bad = 0;
    for (it = 0; it < 2000u; it++) {                             /* random settings: asked in four orders */
        uint32_t x[4], y[4];
        clean_globals(rnd() % 3u == 0 ? DM_SPLIT : DM_STEREO);
        for (p = 0; p < 2u; p++) {
            neutral(&trk[p], eng_named(ENG[rnd() % 10u]));
            if (rnd() % 4u == 0) trk[p].p[P_MUTE] = 1;
            if (rnd() % 2u) s_ins(&trk[p]);
            if (rnd() % 2u) s_f24(&trk[p]);
            if (rnd() % 2u) s_sat(&trk[p]);
            if (rnd() % 3u == 0) g_rand();
        }
        x[0] = part_notes(0), x[1] = part_notes(1);
        y[1] = part_notes(1), y[0] = part_notes(0);
        x[2] = part_notes_shown(1), x[3] = part_notes_shown(0);
        y[2] = part_notes_reason(0), y[3] = part_notes_reason(1);
        (void)y[2], (void)y[3];
        bad += x[0] != y[0] || x[1] != y[1] || x[2] != x[1] || x[3] != x[0];
        bad += part_notes_reason(1) != y[3] || part_notes_reason(0) != y[2];
    }
    bchk("pure: 2000 random settings, asked in four orders, the same", !bad);
    {   /* sanity of the numbers: monotone in the pool of settings and the extremes */
        uint32_t a;
        clean_globals(DM_STEREO);
        trk[1].p[P_MUTE] = 1;                                    /* the other part muted: part 1 alone, LOFI-like cheap -> the pool */
        neutral(&trk[0], eng_named("LOFI"));
        a = part_notes(0);
        bchk("sanity: a plain LOFI part beside a muted part gets the whole pool", a == NVOICE && part_notes_reason(0) == NR_NONE);
        neutral(&trk[0], eng_named("PHYS"));
        a = part_notes(0);
        bchk("sanity: PHYS gets its own limit of 3 and the reason is the engine", a == 3u && part_notes_reason(0) == NR_ENGINE);
        neutral(&trk[0], eng_named("ANALOG"));
        trk[0].p[P_MUTE] = 1;
        trk[1].p[P_MUTE] = 1;
        bchk("sanity: both parts muted: nothing is limited", part_notes(0) == NVOICE && part_notes(1) == NVOICE);
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("budget: NOTES budget: limit %u %% of the time budget, margin %u %%, NOTES_MAX %u, voice pool %u\n", BUDGET_LIMIT, BUDGET_MARGIN, NOTES_MAX, (unsigned)NVOICE);
    t_table();
    t_model();
    t_factory();
    t_monotone();
    t_steal();
    t_bits();
    t_pure();
    printf("budget: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
