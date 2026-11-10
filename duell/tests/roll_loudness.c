/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL: the loudness of the player's own rolls (host_tests.sh rolls; by hand, not in the default list: ~1 min).
 * ROLL B at AMOUNT ROLL_AMOUNT (20) with the player's default settings (SMART, ARP and PITCH off, STEREO), 300 seeds per
 * engine over its sounds (ANALOG: the DUELL factory sounds; PHASE FM6 TRIO WHEEL GRAIN: the engine's presets), each End B
 * (fader at B) and its Original rendered as tests/loudness.h measures (C3 held 4 s, the weighted loudness of the sound's
 * body). The ratio B / A in dB: its distribution per engine and in all. Targets (owner, 2026-10-11): 90 % of the rolls
 * within +-4.5 dB, 99 % within +-8 dB; the test fails outside them.
 *   ROLL_WHY=1   for every roll beyond +-4.5 dB: which parameter group of End B moved the loudness most (End B rendered
 *                with that group back at the Original: the group whose revert brings it closest to A)
 *   ROLL_AMOUNT=n, ROLL_N=n   another AMOUNT / number of rolls per engine */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/category.c"
#include "loudness.h"

#define RL_JOBS 12u
enum { G_FILTER, G_ENV, G_SOURCE, G_FX, G_MATRIX, G_LFO, G_FMB, G_N };
static const char *const GNAME[G_N] = {"filter", "envelopes", "source", "fx/sends", "matrix", "lfo", "fm6 bytes"};

typedef struct { uint32_t e, k, ds, seed, amt; int keep; } rl_t;   /* keep: -1 the whole End B, else that group reverted */

static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
/* the group of id for engine e */
static uint32_t rl_group(const engine_t *e, uint32_t id)
{
    if (id >= P_E0) {
        uint32_t k = id - P_E0;
        if (morph_cutlike(e, k) || (e == &ENG_ANALOG && (k == 5u || k == 7u)) || (e == &ENG_TRIO && (k == 4u || k == 6u)))
            return G_FILTER;
        return G_SOURCE;
    }
    if (id == P_ED_FLT || id == P_E2FLT || id == P_LD_FLT || id == P_L2FLT)
        return G_FILTER;
    if ((id >= P_ATK && id <= P_REL) || (id >= P_E2ATK && id <= P_E2REL) || id == P_ED_SHP || id == P_E2SHP || id == P_ED_PIT)
        return G_ENV;
    if (id >= P_M1SRC && id < P_M1SRC + 3u * NMSLOT)
        return G_MATRIX;
    if ((id >= P_DIST && id <= P_REV) || id == P_PAN || id == P_SPRD || (id >= P_IA && id <= P_IMIX))
        return G_FX;
    return G_LFO;
}
static void rl_setup(void *v)
{
    const rl_t *a = v;
    track_t *t = &trk[0];
    host_tracks_init();
    song.g[G_RTYPE] = GP[G_RTYPE].def;
    duell_mode_force(DM_STEREO);
    if (a->ds)
        host_ds_load(t, a->e, a->k);
    else
        host_preset(t, a->e, a->k);
    t->p[P_VOICE] = t->p[P_VOICE] == V_POLY ? V_POLY : t->p[P_VOICE];
    if (!a->seed)
        return;                                         /* End A: the Original */
    morph_ready(0);
    morph_roll_cfg(0, 1u, a->seed, a->amt, MC_STEREO);   /* (the player's default: SMART, no ARP / PITCH) */
    if (a->keep >= 0) {
        morph_part_t *mp = &mph[0];
        int8_t *b = mp->h[1][mp->top[1]];
        uint32_t id;
        const engine_t *en = ENGINES[eng_idx(mp->eng)];
        for (id = 0; id < P_COUNT; id++)
            if (rl_group(en, id) == (uint32_t)a->keep)
                b[id] = mp->base[id];
        if (a->keep == G_FMB)
            memcpy(mp->fh[1][mp->top[1]], mp->fb, NFM);
        morph_build(mp);
    }
    mph[0].fader = 127;
}
static void rl_run(rl_t *a, lz_t *L, uint32_t n)
{
    static const uint8_t C3 = 48;
    uint32_t i, j;
    for (i = 0; i < n; i += RL_JOBS) {
        lz_job_t jb[RL_JOBS];
        for (j = i; j < n && j < i + RL_JOBS; j++)
            jb[j - i] = lz_spawn(rl_setup, &a[j], &C3, 1u);
        for (j = i; j < n && j < i + RL_JOBS; j++)
            L[j] = lz_collect(jb[j - i]);
    }
}
static int cmpd(const void *x, const void *y) { return *(const double *)x < *(const double *)y ? -1 : *(const double *)x > *(const double *)y; }

int main(void)
{
    static const char *const ENG[] = {"ANALOG", "PHASE", "FM6", "TRIO", "WHEEL", "GRAIN"};
    static rl_t a[4096];
    static lz_t L[4096];
    static double all[4096];
    uint32_t amt = getenv("ROLL_AMOUNT") ? (uint32_t)atoi(getenv("ROLL_AMOUNT")) : 20u;
    uint32_t nr = getenv("ROLL_N") ? (uint32_t)atoi(getenv("ROLL_N")) : 300u, why = getenv("ROLL_WHY") != 0;
    uint32_t q, nall = 0, in45 = 0, in8 = 0, gcount[G_N] = {0};
    nr = nr > 600u ? 600u : nr;
    printf("rolls: ROLL B at AMOUNT %u, SMART (no ARP / PITCH), STEREO, %u seeds per engine; End B against the Original, "
           "weighted loudness of the body (tests/loudness.h), C3\n", (unsigned)amt, (unsigned)nr);
    for (q = 0; q < NELEM(ENG); q++) {
        uint32_t e = eng_by_name(ENG[q]), ds = ds_count(e) > 0, ns = ds ? ds_count(e) : ENGINES[e]->npresets, i, n45 = 0, n8 = 0;
        static lz_t LA[64];
        static rl_t aa[64];
        double d[600];
        ns = ns > 64u ? 64u : ns;
        for (i = 0; i < ns; i++)
            aa[i] = (rl_t){e, i, ds, 0u, 0u, -1};
        rl_run(aa, LA, ns);
        for (i = 0; i < nr; i++)
            a[i] = (rl_t){e, i % ns, ds, 1u + (i * 7919u + q * 104729u) % 65535u, amt, -1};
        rl_run(a, L, nr);
        for (i = 0; i < nr; i++) {
            d[i] = L[i].loud - LA[i % ns].loud;
            all[nall++] = d[i];
            n45 += fabs(d[i]) <= 4.5;
            n8 += fabs(d[i]) <= 8.0;
            if (why && fabs(d[i]) > 4.5) {
                rl_t g[G_N];
                lz_t lg[G_N];
                uint32_t k, best = 0;
                for (k = 0; k < G_N; k++) {
                    g[k] = a[i];
                    g[k].keep = (int)k;
                }
                rl_run(g, lg, G_N);
                for (k = 1; k < G_N; k++)
                    if (fabs(lg[k].loud - LA[i % ns].loud) < fabs(lg[best].loud - LA[i % ns].loud))
                        best = k;
                gcount[best]++;
                printf("  %-6s %-12s seed %5u: %+5.1f dB; without:", ENG[q], ds ? ds_get(e, i % ns)->name : ENGINES[e]->presets[i % ns].name,
                       (unsigned)a[i].seed, d[i]);
                for (k = 0; k < G_N; k++)
                    printf(" %s %+.1f", GNAME[k], lg[k].loud - LA[i % ns].loud);
                printf("\n");
            }
        }
        in45 += n45;
        in8 += n8;
        qsort(d, nr, sizeof d[0], cmpd);
        printf("rolls: %-6s %2u sounds: within +-4.5 dB %5.1f %%, +-8 dB %5.1f %%; B/A dB p1 %+.1f p5 %+.1f p50 %+.1f p95 %+.1f p99 %+.1f "
               "(min %+.1f, max %+.1f)\n", ENG[q], (unsigned)ns, 100.0 * n45 / nr, 100.0 * n8 / nr, d[nr / 100], d[nr / 20],
               d[nr / 2], d[nr * 19 / 20], d[nr * 99 / 100], d[0], d[nr - 1]);
    }
    qsort(all, nall, sizeof all[0], cmpd);
    printf("rolls: ALL    %u rolls: within +-4.5 dB %5.1f %%, +-8 dB %5.1f %%; p1 %+.1f p5 %+.1f p50 %+.1f p95 %+.1f p99 %+.1f\n",
           (unsigned)nall, 100.0 * in45 / nall, 100.0 * in8 / nall, all[nall / 100], all[nall / 20], all[nall / 2],
           all[nall * 19 / 20], all[nall * 99 / 100]);
    if (why) {
        printf("rolls: the group that moved the loudness most, over the rolls beyond +-4.5 dB:");
        for (q = 0; q < G_N; q++)
            printf(" %s %u", GNAME[q], (unsigned)gcount[q]);
        printf("\n");
    }
    {
        int ok = in45 * 100u >= 90u * nall && in8 * 100u >= 99u * nall;
        printf("rolls: %s (targets: 90 %% within +-4.5 dB, 99 %% within +-8 dB)\n", ok ? "ok" : "FAIL");
        return ok ? 0 : 1;
    }
}
