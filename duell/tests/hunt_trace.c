/* SPDX-License-Identifier: GPL-3.0-only
 * Click hunt probe (not part of the test list; tools/hunt_probe.py builds it): the bench twin (tests/bench_sim.c) with a log of what changes from block to block,
 * for the sample-exact trace of an engine click (release_drafts/CLICK_HUNT1.md). The hooks are inserted into a scratch copy of fx.c / eng_fm6.c / eng_formant.c /
 * bench_sim.c by the tool; the firmware sources are not changed, and the audio is the twin's, bit for bit. HUNT_LOG=path: one line per block of a recorded test:
 *   B<frame> p<part> P<id>: old -> new      a track parameter t->p[id] after morph_begin and mod_begin (what the block's engines read)
 *   B<frame> p<part> F<byte>: old -> new    FM6: a byte of fm6_eff (the patch as the engines read it after the macros and the morph)
 *   B<frame> p0 SAT followed Q8 x (was y), integer z    SAT as the followed value the block mixes at (dist_x), and the integer track_dist gets
 *   B<frame> p0 DIST in/out first/last      the plain mix's SAT stage: the block's first and last sample before and after
 *   B<frame> S1..S4 maxd2 m at i seam s     (HUNT_WIN) the largest 2nd difference inside the stage's block, at which sample, and at the seam to the block before
 *   B<frame> FMT note ...                   (HUNT_WIN) the VOICE engine's block: formants, pitch, amplitude ramp, SHAPE, aftertouch, the morph fader
 *   B<frame> out <peak> <first> <last>      the stereo output block (left)
 * B<frame> is the recording's frame count at the block's start (the same number as the WAV's frame index). HUNT_FREEZE / HUNT_SET / HUNT_FMFREEZE: tools/hunt_probe.py */
#include "bench_sim.c"
#include <stdio.h>
static FILE *hunt_f;
static int16_t hunt_prev[2][P_COUNT];
static uint8_t hunt_fprev[2][FP_SIZE];
static int hunt_init;
static int16_t hunt_frz[2][P_COUNT];
static uint8_t hunt_frzv[2][P_COUNT], hunt_fm_have;
static uint8_t hunt_fm[2][FP_SIZE];
static uint32_t hunt_fat;
static int hunt_setid = -1, hunt_setv;                  /* HUNT_SET="id=value": this id of the parts is forced every block */
static int hunt_fmfreeze;                              /* HUNT_FMFREEZE=1: the FM6 patch bytes stay as at the first note-on (the morph's moves of them off) */
static void hunt_open(void)
{
    const char *p = getenv("HUNT_LOG"), *z = getenv("HUNT_FREEZE");   /* HUNT_FREEZE="33,39": these ids of the parts stay as at the first block */
    hunt_init = 1;
    if (p)
        hunt_f = fopen(p, "w");
    while (z && *z) {
        unsigned id = (unsigned)strtoul(z, (char **)&z, 10);
        if (id < P_COUNT)
            hunt_frzv[0][id] = hunt_frzv[1][id] = 1;
        if (*z == ',')
            z++;
        else
            break;
    }
    hunt_fmfreeze = getenv("HUNT_FMFREEZE") != 0;
    if (getenv("HUNT_SET"))
        sscanf(getenv("HUNT_SET"), "%d=%d", &hunt_setid, &hunt_setv);
    hunt_fat = getenv("HUNT_FAT") ? (uint32_t)strtoul(getenv("HUNT_FAT"), 0, 10) : 0u;
    if (getenv("HUNT_FREEZE") && !strcmp(getenv("HUNT_FREEZE"), "all")) {
        unsigned id;
        for (id = 0; id < P_COUNT; id++)
            hunt_frzv[0][id] = hunt_frzv[1][id] = 1;
    }
}
static void hunt_blk(track_t *t)
{
    uint32_t pi = (uint32_t)(t - trk), i;
    if (!hunt_init)
        hunt_open();
    if (hunt_frame == 0xFFFFFFFFu || pi > 1u)
        return;
    for (i = 0; i < P_COUNT; i++)
        if (hunt_frzv[pi][i] && hunt_frame >= hunt_fat) {
            if (hunt_frzv[pi][i] == 1)
                hunt_frz[pi][i] = t->p[i], hunt_frzv[pi][i] = 2;
            t->p[i] = hunt_frz[pi][i];
        }
    if (hunt_setid >= 0 && hunt_frame >= hunt_fat)
        t->p[hunt_setid] = (int16_t)hunt_setv;
    if (!hunt_f)
        return;
    for (i = 0; i < P_COUNT; i++)
        if (t->p[i] != hunt_prev[pi][i]) {
            fprintf(hunt_f, "B%u p%u P%u: %d -> %d\n", hunt_frame, pi, i, hunt_prev[pi][i], t->p[i]);
            hunt_prev[pi][i] = t->p[i];
        }
}
static void hunt_fm6(uint32_t tr, const uint8_t *p, fm6_note_t *n, voice_t *v)
{
    uint32_t i;
    if (hunt_frame == 0xFFFFFFFFu || tr > 1u)
        return;
    if (hunt_fmfreeze && hunt_frame >= hunt_fat) {
        if (!(hunt_fm_have & (1u << tr)))
            memcpy(hunt_fm[tr], p, FP_SIZE), hunt_fm_have |= 1u << tr;
        memcpy((uint8_t *)p, hunt_fm[tr], FP_SIZE);
    }
    if (!hunt_f)
        return;
    for (i = 0; i < FP_SIZE; i++)
        if (p[i] != hunt_fprev[tr][i]) {
            fprintf(hunt_f, "B%u p%u F%u: %d -> %d\n", hunt_frame, tr, i, hunt_fprev[tr][i], p[i]);
            hunt_fprev[tr][i] = p[i];
        }
}
static void hunt_after(const int32_t *o, uint32_t n)
{
    uint32_t i;
    int32_t pk = 0;
    if (!hunt_f || hunt_frame == 0xFFFFFFFFu)
        return;
    for (i = 0; i < n; i++)
        if (o[2 * i] > pk || -o[2 * i] > pk)
            pk = o[2 * i] < 0 ? -o[2 * i] : o[2 * i];
    fprintf(hunt_f, "B%u out %d %d %d\n", hunt_frame, pk, o[0], o[2 * (n - 1)]);
}
static void hunt_dist(uint32_t pp, int32_t dx, int32_t di, int32_t w0)
{
    if (hunt_f && pp == 0 && hunt_frame != 0xFFFFFFFFu && (w0 != dx || di != 0))
        fprintf(hunt_f, "B%u p0 SAT followed Q8 %d (was %d), integer %d\n", hunt_frame, dx, w0, di);
}
static void hunt_io(uint32_t pp, const int32_t *a, const int32_t *b, uint32_t n)
{
    if (hunt_f && pp == 0 && hunt_frame != 0xFFFFFFFFu)
        fprintf(hunt_f, "B%u p0 DIST in first %d last %d  out first %d last %d\n", hunt_frame, a[0], a[n - 1], b[0], b[n - 1]);
}
/* stage taps: S1 the engine's block (after track_render), S2 after SAT, S3 after the INSERT, S4 after the EQ. Per block the largest |second difference|
 * inside (and its index) and at the seam to the block before; printed when the frame is in HUNT_WIN="lo,hi" */
static int32_t hunt_sl[4][5][2];
static void hunt_stg(uint32_t pp, uint32_t id, const int32_t *b, uint32_t n)
{
    static uint32_t lo = 1, hi = 0, got;
    int32_t m = 0, mi = 0, seam = 0, i;
    if (!got) {
        const char *w = getenv("HUNT_WIN");
        got = 1;
        if (w)
            lo = (uint32_t)strtoul(w, (char **)&w, 10), hi = (uint32_t)strtoul(w + 1, 0, 10);
    }
    if (!hunt_f || pp > 0u || hunt_frame == 0xFFFFFFFFu || id > 4u)
        return;
    if (hunt_frame >= lo && hunt_frame <= hi) {
        int32_t x0 = hunt_sl[pp][id][0], x1 = hunt_sl[pp][id][1];
        for (i = 0; i < (int32_t)n; i++) {
            int32_t a = i == 0 ? x0 : (i == 1 ? x1 : b[i - 2]), c = i == 0 ? x1 : b[i - 1], d2 = b[i] - 2 * c + a;
            d2 = d2 < 0 ? -d2 : d2;
            if (i == 0 || i == 1)
                seam = d2 > seam ? d2 : seam;
            if (d2 > m)
                m = d2, mi = i;
        }
        fprintf(hunt_f, "B%u S%u maxd2 %d at %d seam %d\n", hunt_frame, id, m, mi, seam);
    }
    hunt_sl[pp][id][0] = b[n - 2];
    hunt_sl[pp][id][1] = b[n - 1];
}
static void hunt_fmt(track_t *t, voice_t *v, int32_t *fa, int32_t *ba, int32_t k, uint32_t ratio, uint32_t bws, uint32_t f0, const vmod_t *m)
{
    static uint32_t lo = 1, hi = 0, got;
    if (!got) {
        const char *w = getenv("HUNT_WIN");
        got = 1;
        if (w)
            lo = (uint32_t)strtoul(w, (char **)&w, 10), hi = (uint32_t)strtoul(w + 1, 0, 10);
    }
    if (hunt_f && t == &trk[0] && hunt_frame >= lo && hunt_frame <= hi)
        fprintf(hunt_f, "B%u FMT note %u F %d %d %d  k %d f0 %u inc %u pitch16 %d amp %d -> %d cut %d shape %d envout %d stage %d at %d mf %d\n", hunt_frame, v->note, fa[0], fa[1], fa[2], k, f0, m->inc, m->pitch16, m->amp0, m->amp1, m->cutoff, m->shape, v->env_out, v->stage, t->at, t->mf);
}
