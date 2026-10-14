/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL factory sound audit (host_tests.sh sounds): every factory sound (firmware/src/sounds.c, tools/duell_sounds.py) loaded
 * on part 1 as ui.c apply_preset_to loads it (INIT: TP's and the engine's defaults, then its pairs, its arp too), C3 at
 * velocity 100 held 4 s, 1 s of release, in STEREO (the power-on mode). The measure is tests/loudness.h's: ear-weighted
 * (1-pole HP 100 Hz, LP 6 kHz), over the loudest 1.5 s window of a sound that sustains and the loudest 0.5 s window of one
 * that does not (a pluck, a stab), on the mix before MASTER and the master limiter, so a slow attack (a pad, a riser) reads
 * its body, not its onset. Every sound is measured with the one note (owner, 2026-10-11: a pad measured as a three-note
 * chord was trimmed ~5 dB under the others and heard "very quiet"); PAD and KEYS also show the peak of C3 E3 G3 (CHORD PK,
 * information: a chord is ~5 dB over its note on any polysynth, the master limiter takes its peaks).
 * End B: the factory sound's own (morph.c morph_rebase_part: its 'bseed' and 'bamt', SMART), the fader at B, rendered the
 * same way in STEREO and in SPLIT (the roll differs between them: PAN / SPREAD draw only in STEREO) and compared with A in
 * the same mode (SPLIT's part sits differently in the mix): its loudness change (dB) and its brightness against A's (a
 * factor, tests/loudness.h).
 * Time-resolved (owner, 2026-10-11: BACKWARDS, a slow attack, was "super quiet" after seconds): the note is held 10 s and
 * the columns show the level at 1 s and 3 s (the 0.5 s window centred there), MED (the median of the ten 0.5 s windows of
 * seconds 1..6: what a held note sounds like), T-3 (the seconds until a 0.5 s window is within 3 dB of the plateau, the
 * loudest 1.5 s), TAP (a second render: 1 s hold + release, its loudest 0.5 s: a played-short note) and SPLIT (MED of the
 * active side in SPLIT, per-channel RMS: the other channel is silent there, so the two-channel mean would read it 3 dB low).
 * FAIL: silent (A below -60 dBFS), clipping (a peak >= -0.5 dBFS), a value that is not a number; an End B outside its
 * category's window (BASS KEYS PLUCK LEAD OTHER: +-2.5 dB, brightness x0.5..x2; PAD FX: +-3.5 dB, x0.4..x2.5) or with a
 * peak above -1 dBFS (STEREO and SPLIT both); and, in STEREO, for a sound that is not a hit:
 *   (1) it sustains (still sounding at 9 s: the weighted level there above -60 dBFS) and MED is under -21 dBFS  (quiet body)
 *   (2) T-3 above 2 s, unless it is on the allow-list of risers below                               (slow rise)
 *   (3) it sustains and the level at 1 s is more than 12 dB under the plateau (a pluck's natural decay is not a slow start)                                      (slow start)
 *   (4) not PERC / PLUCK / KEYS and MED is more than 10 dB under the loudest 1.5 s window            (falls away)
 *   (5) TAP under -24 dBFS                                                                           (quiet tap)
 *   (6) it sustains, is not a riser and its MED is more than 3 dB under the median MED of all the sounds that sustain
 *       (owner, 2026-10-12: a sound 5..8 dB under the rest was heard "very low" while still above rule 1)  (quiet vs others)
 * Keep-as-designed (exempt from (1), (4) and (6), because the owner loves them as they are): none since 0.2.9 (BACKSPIN, a reversed
 * half-beat that fell away to a quiet body by design, went with the SLICE engine).
 * A HIT (PERC, and an OTHER that is not sounding at 9 s: a one-shot) is judged by its PEAK alone: between -6 and -3 dBFS
 * (hit level), no weighted-loudness rule. Risers allowed to take longer than 2 s to reach the plateau: WHITE OUT, MOOR WIND,
 * LESLIE STORM (a swell is their sound); nothing else.
 * Every sound must set LFO 1's and LFO 2's TRIG to FREE itself (DUELL 0.2.2; the pair is in the sound, not the default): else FAIL.
 *   build/host/sound_audit                   the table; exit 1 on a FAIL
 *   SOUND_TRIM=1 build/host/sound_audit      also the LEVEL each sound would need for AUDIT_TARGET_DB (weighted, in its
 *                                            window), its peak at most AUDIT_PEAK_DB (LEVEL: 0.5 dB a step,
 *                                            tools/gen_tables.py), to copy into tools/duell_sounds.py ("LEVEL": n); a sound
 *                                            that sustains is trimmed on MED (its held body), a hit on its peak (-4.5)
 *   SOUND_BSEED=1 build/host/sound_audit     the End B seed search: per sound AUDIT_NCAND candidate seeds (the hash of
 *                                            (engine, idx) first) rolled at its bamt, each rendered in STEREO and SPLIT;
 *                                            the seeds that pass both (the windows above), the first one's rank, the best
 *                                            (smallest loudness change): copy one into tools/duell_sounds.py ("bseed": n)
 *   SOUND_BALL=1                             with SOUND_BSEED: also the candidates that fail, in [ ]
 *   SOUND_ONLY=NAME                          only that sound
 *   SOUND_WIN=1                              also the windows (loudest 1.5 s, 0.5 s, the 1.5 s before key-off) and when the
 *                                            peak came */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/category.c"   /* CAT_* and their names */
#include "loudness.h"

#define AUDIT_TARGET_DB (-18.0)
#define AUDIT_PEAK_DB (-3.0)           /* SOUND_TRIM keeps the note's peak this far under full scale */
#define AUDIT_BPEAK_DB (-1.0)          /* an End B's peak at most */
#ifndef AUDIT_NCAND
#define AUDIT_NCAND 64u
#endif
#define AUDIT_JOBS 12u                 /* renders at once */
#define AUDIT_HOLD 10.0
#define AUDIT_SOUNDING_DB (-60.0)      /* still sounding at 9 s: the level there is above this */
#define AUDIT_MED_MIN (-21.0)
#define AUDIT_TAP_MIN (-24.0)
#define AUDIT_HIT_LO (-6.0)
#define AUDIT_HIT_HI (-3.0)
#define AUDIT_HIT_TRIM (-4.5)
#define AUDIT_REL_MARGIN 3.0           /* rule (6): a sustaining sound's MED at most this far under the median MED of the sustaining ones */
static const char *const AUDIT_RISERS[] = {"WHITE OUT", "MOOR WIND", "LESLIE STORM", "RAIN CHIMES"};   /* RAIN CHIMES: its arp is one LFO phrase now (TRIG NOTE no longer restarts per step), so its 0.25 Hz LFO 2 filter sweep runs its 4 s cycle as the recipe says */
static const char *const AUDIT_KEEP[] = {0};   /* keep-as-designed: exempt from quiet body, falls away, quiet vs others (BACKSPIN until
                                                * 0.2.8: retired with SLICE; none now) */

typedef struct { uint32_t e, k, mode, b, seed, amt, chord, tap; } au_t;   /* b: 0 End A (the sound), 1 its factory End B, 2 seed / amt */
static void au_setup(void *v)
{
    const au_t *a = v;
    host_tracks_init();
    song.g[G_RTYPE] = GP[G_RTYPE].def;        /* (the product's reverb, not the goldens' ROOM) */
    duell_mode_force(a->mode);
    host_ds_load(&trk[0], a->e, a->k);        /* (hostsim.c: as ui.c apply_preset_to) */
    if (!a->chord)
        trk[0].p[P_CHRD] = 0;                 /* (chord keys of a pad: the audit's one note stays one note, as it was trimmed) */
    if (a->b) {
        morph_ready(0);                       /* (first use: the load's rebase rolls the factory End B) */
        if (a->b == 2u)
            morph_roll_cfg(0, 1u, a->seed, a->amt, a->mode != DM_SPLIT ? MC_STEREO : 0u);
        mph[0].fader = 127;
    }
}
static const uint8_t CH[3] = {48, 52, 55};
static int au_chordy(const ds_sound_t *s) { return s->cat == CAT_PAD || s->cat == CAT_KEYS; }
static int au_wide(const ds_sound_t *s) { return s->cat == CAT_PAD || s->cat == CAT_FX; }
/* a hit: judged by its peak (PERC, and an OTHER that is not sounding at 9 s) */
static int au_hit(const ds_sound_t *s, const lz_t *A) { return s->cat == CAT_PERC || (s->cat == CAT_OTHER && A->l9 <= AUDIT_SOUNDING_DB); }
static int au_riser(const ds_sound_t *s)
{
    uint32_t i;
    for (i = 0; i < sizeof AUDIT_RISERS / sizeof *AUDIT_RISERS; i++)
        if (str_eq(s->name, AUDIT_RISERS[i]))
            return 1;
    return 0;
}
static int au_keep(const ds_sound_t *s)
{
    uint32_t i;
    for (i = 0; i < sizeof AUDIT_KEEP / sizeof *AUDIT_KEEP; i++)
        if (AUDIT_KEEP[i] && str_eq(s->name, AUDIT_KEEP[i]))
            return 1;
    return 0;
}
static int au_dcmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
/* the level the trim reads: a sound that sustains (judged on A), its held body (MED, the median of seconds 1-6); any other, its
 * loudest window. (End B is compared on the loudest window, LOUD, as before) */
static double au_lvl(const ds_sound_t *s, const lz_t *A, const lz_t *X) { return !au_hit(s, A) && A->l9 > AUDIT_SOUNDING_DB ? X->med16 : X->loud; }

/* End B against A in its category's window */
static int au_b_ok(const ds_sound_t *s, const lz_t *A, const lz_t *B)
{
    double dl = B->loud - A->loud, br = A->bright > 0 ? B->bright / A->bright : 0, w = au_wide(s) ? 3.5 : 2.5;
    double blo = au_wide(s) ? 0.4 : 0.5, bhi = au_wide(s) ? 2.5 : 2.0;
    return !B->nan && fabs(dl) <= w && br >= blo && br <= bhi && B->peak <= AUDIT_BPEAK_DB && B->loud > -60;
}
/* render the jobs a[0..n) into L, AUDIT_JOBS at a time (skip: not rendered, L NaN) */
static void au_run(au_t *a, lz_t *L, uint32_t n, const char *only)
{
    uint32_t i, j;
    for (i = 0; i < n; i += AUDIT_JOBS) {
        lz_job_t jb[AUDIT_JOBS];
        for (j = i; j < n && j < i + AUDIT_JOBS; j++)
        {
            lz_hold = a[j].tap ? 1.0 : AUDIT_HOLD;
            jb[j - i] = only && !str_eq(ds_get(a[j].e, a[j].k)->name, only) ? (lz_job_t){-1, -1} :
                        lz_spawn(au_setup, &a[j], CH, a[j].chord ? 3u : 1u);
        }
        for (j = i; j < n && j < i + AUDIT_JOBS; j++)
            L[j] = lz_collect(jb[j - i]);
    }
}

static uint32_t cand_first[CAT_N], cand_n[CAT_N], cand_late[CAT_N];
static void bseed_search(uint32_t e, uint32_t k, const ds_sound_t *s, const lz_t *A, const lz_t *Ap)
{
    static au_t a[2u * AUDIT_NCAND];
    static lz_t L[2u * AUDIT_NCAND];
    uint32_t c, seed = ds_hash(e, k), first = 0, npass = 0, best = 0;
    double bestd = 1e9;
    for (c = 0; c < AUDIT_NCAND; c++) {
        a[2u * c] = (au_t){e, k, DM_STEREO, 2u, seed, s->bamt, 0u, 0u};
        a[2u * c + 1u] = (au_t){e, k, DM_SPLIT, 2u, seed, s->bamt, 0u, 0u};
        seed = (seed * 1103515245u + 12345u) % 65535u + 1u;
    }
    au_run(a, L, 2u * AUDIT_NCAND, 0);
    printf("  bseed %-12s %-6s bamt %2u:", s->name, CAT_NAME[s->cat % CAT_N], (unsigned)s->bamt);
    for (c = 0; c < AUDIT_NCAND; c++) {
        const lz_t *st = &L[2u * c], *sp = &L[2u * c + 1u];
        double d = fabs(st->loud - A->loud) > fabs(sp->loud - Ap->loud) ? fabs(st->loud - A->loud) : fabs(sp->loud - Ap->loud);
        if (!au_b_ok(s, A, st) || !au_b_ok(s, Ap, sp)) {
            if (getenv("SOUND_BALL"))             /* (the failing candidates too: dB, brightness, peak, STEREO / SPLIT) */
                printf(" [%u: %+.1f/%+.1f x%.2f/x%.2f pk %.1f/%.1f]", (unsigned)a[2u * c].seed, st->loud - A->loud, sp->loud - Ap->loud,
                       st->bright / A->bright, sp->bright / Ap->bright, st->peak, sp->peak);
            continue;
        }
        if (!npass++)
            first = c + 1u;
        printf(" %u(%+.1f/%+.1f x%.2f/x%.2f)", (unsigned)a[2u * c].seed, st->loud - A->loud, sp->loud - Ap->loud,
               st->bright / A->bright, sp->bright / Ap->bright);
        if (d < bestd) {
            bestd = d;
            best = a[2u * c].seed;
        }
    }
    printf("\n  bseed %-12s %u of %u pass, the first at candidate %u, the best %u (max |dB| %.1f)\n", s->name, (unsigned)npass,
           (unsigned)AUDIT_NCAND, (unsigned)first, (unsigned)best, npass ? bestd : 0.0);
    cand_n[s->cat % CAT_N]++;
    cand_late[s->cat % CAT_N] += !first || first > 16u;
    cand_first[s->cat % CAT_N] += first;
}

int main(void)
{
    double med_all = -200.0;
    int trim = getenv("SOUND_TRIM") != 0, bsrch = getenv("SOUND_BSEED") != 0, fails = 0;
    const char *only = getenv("SOUND_ONLY");
    uint32_t n, N = ds_list_n();
    static au_t a[6u * 256u];
    static lz_t L[6u * 256u];
    printf("sounds: DUELL factory sounds, part 1, C3 velocity 100 held %.0f s + %.0f s release, STEREO; dBFS before MASTER; "
           "LOUD = weighted, in the loudest 1.5 s (S) / 0.5 s (D: not sustained) window; L1 L3 = level at 1 s / 3 s, MED = median "
           "of seconds 1-6, T-3 = s to within 3 dB of the plateau, TAP = loudest 0.5 s of a 1 s note, SPLIT = MED of the active "
           "side; End B in STEREO / SPLIT against A\n", AUDIT_HOLD, LZ_TAIL);
    printf("  %-3s %-12s %-6s %-7s %6s %6s %5s %5s %5s %4s %6s %6s %8s  %-13s %-17s %-8s %s%s\n", "#", "NAME", "CAT", "ENGINE", "PEAK",
           "LOUD", "L1", "L3", "MED", "T-3", "TAP", "SPLIT", "CHORD PK", "END B dB", "BRIGHT x", "SEED/AMT", "VERDICT",
           trim ? "              LEVEL -> TRIM" : "");
    for (n = 0; n < N; n++) {             /* every sound: A, B STEREO, A and B SPLIT, the chord, the tap: all at once */
        uint32_t k, e = ds_list_at(n, &k);
        a[6u * n] = (au_t){e, k, DM_STEREO, 0u, 0u, 0u, 0u, 0u};
        a[6u * n + 1u] = (au_t){e, k, DM_STEREO, 1u, 0u, 0u, 0u, 0u};
        a[6u * n + 2u] = (au_t){e, k, DM_SPLIT, 0u, 0u, 0u, 0u, 0u};
        a[6u * n + 3u] = (au_t){e, k, DM_SPLIT, 1u, 0u, 0u, 0u, 0u};
        a[6u * n + 4u] = (au_t){e, k, DM_STEREO, 0u, 0u, 0u, 1u, 0u};
        a[6u * n + 5u] = (au_t){e, k, DM_STEREO, 0u, 0u, 0u, 0u, 1u};
    }
    au_run(a, L, 6u * N, only);
    {   /* rule (6): the median MED of the sounds that sustain (all of them, a riser too) */
        static double meds[256];
        uint32_t m = 0;
        for (n = 0; n < N; n++) {
            uint32_t k, e = ds_list_at(n, &k);
            const ds_sound_t *s = ds_get(e, k);
            const lz_t *A = &L[6u * n];
            if (!A->nan && !au_hit(s, A) && A->l9 > AUDIT_SOUNDING_DB && A->med16 > -150.0)
                meds[m++] = A->med16;
        }
        if (m) {
            qsort(meds, m, sizeof *meds, au_dcmp);
            med_all = (m & 1u) ? meds[m / 2u] : 0.5 * (meds[m / 2u - 1u] + meds[m / 2u]);
        }
        if (!only)
            printf("sounds: median MED of the %u sounds that sustain %.1f dBFS; rule (6) floor %.1f\n", (unsigned)m, med_all,
                   med_all - AUDIT_REL_MARGIN);
    }
    for (n = 0; n < N; n++) {
        uint32_t k, e = ds_list_at(n, &k);
        const ds_sound_t *s = ds_get(e, k);
        const lz_t *A = &L[6u * n], *Bt = &L[6u * n + 1u], *Ap = &L[6u * n + 2u], *Bp = &L[6u * n + 3u], *C = &L[6u * n + 4u],
                   *T = &L[6u * n + 5u];
        int bok = au_b_ok(s, A, Bt) && au_b_ok(s, Ap, Bp), hit = au_hit(s, A), sound9 = A->l9 > AUDIT_SOUNDING_DB;
        const char *why = "ok";
        int lv = ds_value(s, P_LEVEL, TP[P_LEVEL].def);
        char cpk[16];
        if (only && !str_eq(s->name, only))
            continue;
        if (ds_value(s, P_LTRIG, 0) != 1 || ds_value(s, P_L2TRIG, 0) != 1)   /* (def 0: a sound that leaves it out fails) */
            why = "FAIL (LFO TRIG not FREE)";   /* DUELL 0.2.2: every factory sound sets both LFOs FREE itself */
        else if (A->nan || T->nan)
            why = "FAIL (NaN / crash)";
        else if (A->loud < -60)
            why = "FAIL (silent)";
        else if (A->peak >= -0.5)
            why = "FAIL (clips)";
        else if (hit && (A->peak < AUDIT_HIT_LO || A->peak > AUDIT_HIT_HI))
            why = "FAIL (hit level)";
        else if (!hit && sound9 && A->med16 < AUDIT_MED_MIN && !au_keep(s))
            why = "FAIL (quiet body)";
        else if (!hit && A->t3 > 2.0 && !au_riser(s))
            why = "FAIL (slow rise)";
        else if (!hit && sound9 && A->l1 < A->w15 - 12.0)
            why = "FAIL (slow start)";
        else if (!hit && s->cat != CAT_PLUCK && s->cat != CAT_KEYS && s->cat != CAT_PERC && A->med16 < A->w15 - 10.0 && !au_keep(s))
            why = "FAIL (falls away)";
        else if (!hit && sound9 && !au_riser(s) && !au_keep(s) && A->med16 < med_all - AUDIT_REL_MARGIN)
            why = "FAIL (quiet vs others)";
        else if (!hit && T->w05 < AUDIT_TAP_MIN)
            why = "FAIL (quiet tap)";
        else if (!bok)
            why = "FAIL (End B)";
        fails += why[0] == 'F';
        if (au_chordy(s))
            snprintf(cpk, sizeof cpk, "%8.1f", C->peak);
        else
            snprintf(cpk, sizeof cpk, "%8s", "");
        printf("  %-3u %-12s %-6s %-7s %6.1f %6.1f %5.1f %5.1f %5.1f %4.1f %6.1f %6.1f %s  %+5.1f / %+5.1f  x%.2f / x%.2f   %5u/%2u %-17s %c%c",
               n + 1, s->name, CAT_NAME[s->cat % CAT_N], ENGINES[e]->name, A->peak, A->loud, A->l1, A->l3, A->med16, A->t3,
               T->w05, Ap->cmed, cpk, Bt->loud - A->loud, Bp->loud - Ap->loud, Bt->bright / A->bright, Bp->bright / Ap->bright,
               (unsigned)ds_seed(e, k), (unsigned)ds_amount(e, k), why, A->sus ? 'S' : 'D', hit ? 'H' : ' ');
        if (getenv("SOUND_WIN"))
            printf("  w1.5 %.1f w0.5 %.1f last1.5 %.1f peak@%.2fs l9 %.1f", A->w15, A->w05, A->last15, A->peak_t, A->l9);
        if (trim) {
            int to, cap, pk = (int)floor(lv + 2.0 * (AUDIT_PEAK_DB - A->peak));
            if (hit)
                to = (int)lround(lv + 2.0 * (AUDIT_HIT_TRIM - A->peak)), cap = 0;
            else {
                to = (int)lround(lv + 2.0 * (AUDIT_TARGET_DB - au_lvl(s, A, A)));
                cap = pk < to;
                to = cap ? pk : to;               /* (its peak sets the trim, not its loudness) */
            }
            to = to < 1 ? 1 : to > 127 ? 127 : to;
            printf("  %3d -> %3d%s%s", lv, to, cap ? " (peak)" : hit ? " (hit)" : "",
                   !hit && lv + 2.0 * (AUDIT_TARGET_DB - au_lvl(s, A, A)) > 127.5 ? " (needs more than LEVEL 127)" : "");
        }
        printf("\n");
        if (bsrch)
            bseed_search(e, k, s, A, Ap);
    }
    if (bsrch) {
        printf("sounds: bseed search, per category: sounds, those whose first passing seed was beyond candidate 16 (or none), "
               "mean first rank:");
        for (n = 0; n < CAT_N; n++)
            if (cand_n[n])
                printf(" %s %u/%u (%.1f)", CAT_NAME[n], (unsigned)cand_late[n], (unsigned)cand_n[n], (double)cand_first[n] / cand_n[n]);
        printf("\n");
    }
    printf("sounds: %u sounds, %d FAIL\n", (unsigned)N, fails);
    return fails ? 1 : 0;
}
