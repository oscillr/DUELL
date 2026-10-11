/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL effects step 2 (SPEC 5c.4): the per-part MIDI-triggered sidechain (fx.c duck_*). On mode_test.c's rig (the DSP, MIDI in).
 *   build/host/duck_test   (DUELL/host_tests.sh "duck")
 *   CURVE    part 1 SRC CH10, NOTE ANY, DEPTH 100, ATK 5 ms, HOLD 100 ms, RLS 242 ms; a held tone; a note-on on channel 10:
 *            the level is about 0 in the hold, about half in the middle of the release, full after it
 *   PER PART part 2 (DEPTH 0) is not touched; then part 2's own DUCK (another channel) leaves part 1 alone
 *   SWALLOW  a trigger note plays nothing on a channel that plays no part
 *   PUMP     SRC PUMP, DEPTH 100, RATE 1/4 at 120 BPM: three dips in 1.48 s, at 0, 500 and 1000 ms
 *   PART     SRC PART: part 1 ducks while part 2 plays loud (ratio), comes back after; part 2 silent: part 1 untouched */
#define MODE_TEST_NO_MAIN 1
#include "mode_test.c"

static double rms_win(const int32_t *x, uint32_t at, uint32_t n)
{
    double s = 0;
    for (uint32_t i = 0; i < n; i++)
        s += (double)x[at + i] * x[at + i];
    return sqrt(s / n);
}
static int32_t ref_l[MAXB * CTL], ref_r[MAXB * CTL];
/* the scene: two held tones, part 1 on DUCK CH ch1, part 2 on ch2 (0 = OFF); the trigger note-on(s) at 500 ms; 1200 ms in all */
static uint32_t scene(uint32_t ch1, uint32_t ch2, uint32_t trig_ch)
{
    uint32_t e = eng_of("ANALOG"), ms = FS / 1000u, i, v;
    dsp_clean(DM_SPLIT);
    memset(duck_st, 0, sizeof duck_st);
    for (i = 0; i < 2u; i++) {
        host_preset(&trk[i], e, 0);
        trk[i].p[P_LEVEL] = 100;
        trk[i].p[P_SUS] = 127;
        trk[i].p[P_AMODE] = 0;
        trk[i].p[P_CHOR] = trk[i].p[P_DLY] = trk[i].p[P_REV] = 0;   /* (the sends' tails are not ducked) */
        trk[i].p[P_DKNOTE] = 0;
        trk[i].p[P_DKDEP] = 100;
        trk[i].p[P_DKATK] = 5;
        trk[i].p[P_DKHLD] = 20;                          /* 100 ms */
        trk[i].p[P_DKRLS] = 36;                          /* 242 ms */
    }
    trk[0].p[P_DKSRC] = (int16_t)(ch1 ? ch1 + 1u : 0u);   /* SRC 2..17 = CH1..16 (0 = off here: PUMP with DEPTH 0) */
    trk[1].p[P_DKSRC] = (int16_t)(ch2 ? ch2 + 1u : 0u);
    if (!ch1)
        trk[0].p[P_DKDEP] = 0;
    if (!ch2)
        trk[1].p[P_DKDEP] = 0;
    trk_note_on(&trk[0], 57, 100);
    trk_note_on(&trk[1], 64, 100);
    rec_n = 0;
    mblk(500u * ms / CTL);
    if (trig_ch)
        midi(0x90, trig_ch - 1u, 36, 100);               /* the trigger: plays nothing */
    mblk(700u * ms / CTL);
    for (v = i = 0; i < NVOICE; i++)
        v += trk[0].v[i].active + trk[1].v[i].active;
    return v;
}
static double ratio(const int32_t *on, const int32_t *off, uint32_t at_ms, uint32_t len_ms)
{
    uint32_t ms = FS / 1000u;
    return rms_win(on, 500u * ms + at_ms * ms, len_ms * ms) / rms_win(off, 500u * ms + at_ms * ms, len_ms * ms);
}
/* PUMP: part 1 (left) a held tone, SRC PUMP from 200 ms on; the left channel's 10 ms levels for 1.48 s; the dips' starts */
static uint32_t pump_dips(double *at)
{
    uint32_t ms = FS / 1000u, i, w, nd = 0, low = 0;
    double mx = 0, r[200];
    dsp_clean(DM_SPLIT);
    memset(duck_st, 0, sizeof duck_st);
    host_preset(&trk[0], eng_of("ANALOG"), 0);
    trk[0].p[P_LEVEL] = 100;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_AMODE] = 0;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_DKDEP] = 100;
    trk[0].p[P_DKATK] = 5;
    trk[0].p[P_DKHLD] = 0;
    trk[0].p[P_DKRLS] = 25;                              /* 113 ms */
    trk[0].p[P_DKRATE] = 0;                              /* 1/4 */
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    trk[0].p[P_DKSRC] = 2;                               /* (CH1, no note: idle) */
    trk_note_on(&trk[0], 57, 100);
    mblk(200u * ms / CTL);
    trk[0].p[P_DKSRC] = 0;                               /* PUMP */
    rec_n = 0;
    mblk(1480u * ms / CTL);
    for (w = 0; w < 148u; w++) {
        r[w] = rms_win(rec_l, w * 10u * ms, 10u * ms);
        mx = r[w] > mx ? r[w] : mx;
    }
    for (w = 0; w < 148u; w++) {
        if (r[w] < 0.5 * mx && !low && nd < 8u)
            at[nd++] = w * 10.0;
        low = r[w] < 0.5 * mx;
    }
    (void)i;
    return nd;
}
/* PART: part 1 (left) a held tone keyed by part 2's loud notes, 400..800 ms (p2 0: none); returns part 1's levels before, in
 * the middle of and after the notes (rms) */
static void part_levels(uint32_t src, uint32_t p2, double *bef, double *dur, double *aft)
{
    uint32_t ms = FS / 1000u;
    dsp_clean(DM_SPLIT);
    memset(duck_st, 0, sizeof duck_st);
    for (uint32_t i = 0; i < 2u; i++) {
        host_preset(&trk[i], eng_of("ANALOG"), 0);
        trk[i].p[P_LEVEL] = i ? 127 : 100;
        trk[i].p[P_SUS] = 127;
        trk[i].p[P_AMODE] = 0;
        trk[i].p[P_CHOR] = trk[i].p[P_DLY] = trk[i].p[P_REV] = 0;
    }
    trk[0].p[P_DKSRC] = (int16_t)src;
    trk[0].p[P_DKDEP] = 100;
    trk[0].p[P_DKTHR] = 30;
    trk[0].p[P_DKATK] = 5;
    trk[0].p[P_DKHLD] = 0;
    trk[0].p[P_DKRLS] = 25;
    trk_note_on(&trk[0], 57, 100);
    rec_n = 0;
    mblk(400u * ms / CTL);
    if (p2)
        trk_note_on(&trk[1], 64, 127);
    mblk(400u * ms / CTL);
    if (p2)
        trk_note_off(&trk[1], 64);
    mblk(600u * ms / CTL);
    *bef = rms_win(rec_l, 300u * ms, 80u * ms);
    *dur = rms_win(rec_l, 500u * ms, 200u * ms);
    *aft = rms_win(rec_l, 1100u * ms, 80u * ms);
}
/* PING-PONG SPREAD (G_PINGSPREAD): a short note into the delay (PING ON, a mono part): the largest |L - R| of the render */
static double ping_lr(int32_t spread)
{
    uint32_t ms = FS / 1000u, i;
    double m = 0;
    dsp_clean(DM_STEREO);
    memset(duck_st, 0, sizeof duck_st);
    host_preset(&trk[0], eng_of("ANALOG"), 0);
    trk[0].p[P_LEVEL] = 100;
    trk[0].p[P_PAN] = 0;
    trk[0].p[P_SPRD] = 0;
    trk[0].p[P_AMODE] = 0;
    trk[0].p[P_CHOR] = trk[0].p[P_REV] = 0;
    trk[0].p[P_DLY] = 127;
    song.g[G_DTIME] = 40;
    song.g[G_DFDBK] = 80;
    song.g[G_DMIX] = 127;
    song.g[G_PINGPONG] = 1;
    song.g[G_PINGSPREAD] = (int16_t)spread;
    trk_note_on(&trk[0], 57, 100);
    rec_n = 0;
    mblk(100u * ms / CTL);
    trk_note_off(&trk[0], 57);
    mblk(1400u * ms / CTL);
    for (i = 0; i < rec_n; i++) {
        double d = (double)rec_l[i] - rec_r[i];
        m = fabs(d) > m ? fabs(d) : m;
    }
    return m;
}
int main(void)
{
    uint32_t v_ref = scene(10, 0, 0), v_on, i;
    memcpy(ref_l, rec_l, sizeof ref_l);
    memcpy(ref_r, rec_r, sizeof ref_r);
    v_on = scene(10, 0, 10);
    double hold = ratio(rec_l, ref_l, 40, 20), mid = ratio(rec_l, ref_l, 225, 10), back = ratio(rec_l, ref_l, 600, 20);
    double lo = 9, hi = 0;
    for (i = 0; i < 680u; i += 10u) {
        double r = ratio(rec_r, ref_r, i, 10);
        lo = r < lo ? r : lo;
        hi = r > hi ? r : hi;
    }
    printf("duck: part 1 of its undisturbed level: hold %.3f, mid-release %.3f, after %.3f; part 2 %.3f..%.3f\n", hold, mid, back, lo, hi);
    expect("CURVE: ~0 in the hold (< 0.05), 0.25..0.75 mid release (linear: 0.5), >= 0.97 after the release", hold < 0.05 && mid > 0.25 && mid < 0.75 && back >= 0.97);
    expect("PER PART: part 2 (CH OFF) identical all through (0.99 .. 1.01)", lo > 0.99 && hi < 1.01);
    expect("SWALLOW: the trigger note started no voice", v_on == v_ref);
    scene(0, 11, 11);                                    /* part 2 on channel 11, part 1 OFF */
    double r2 = ratio(rec_r, ref_r, 40, 20), l1 = ratio(rec_l, ref_l, 40, 20);
    printf("duck: part 2 in its hold %.3f, part 1 %.3f\n", r2, l1);
    expect("PER PART: part 2's trigger ducks part 2 (< 0.05), part 1 not (0.99 .. 1.01)", r2 < 0.05 && l1 > 0.99 && l1 < 1.01);
    double dip[8], b0, d0, a0, b1, d1, a1, b2, d2, a2;
    uint32_t nd = pump_dips(dip);
    printf("duck: PUMP %u dips at %.0f %.0f %.0f ms\n", nd, dip[0], dip[1], dip[2]);
    expect("PUMP: 3 dips in 1.48 s, at 0, 500, 1000 ms (+- 20)", nd == 3u && dip[0] <= 20 && dip[1] > 480 && dip[1] < 520 && dip[2] > 980 && dip[2] < 1020);
    part_levels(2, 1, &b0, &d0, &a0);                    /* the reference: SRC CH1 (no key), part 2 plays */
    part_levels(1, 1, &b1, &d1, &a1);
    part_levels(1, 0, &b2, &d2, &a2);
    printf("duck: PART keyed part 1 of its level: before %.3f, during %.3f, after %.3f; part 2 silent %.3f\n", b1 / b0, d1 / d0, a1 / a0, d2 / d0);
    expect("PART: part 2 loud: part 1 < 0.2 while it sounds, >= 0.97 before and after", b1 / b0 > 0.97 && d1 / d0 < 0.2 && a1 / a0 > 0.97);
    expect("PART: part 2 silent: part 1 untouched (>= 0.99)", d2 / d0 > 0.99 && b2 / b0 > 0.99 && a2 / a0 > 0.99);
    double sp0 = ping_lr(0), sp50 = ping_lr(50), sp100 = ping_lr(100);
    printf("duck: PING SPREAD max |L-R|: 0 %% %.0f, 50 %% %.0f, 100 %% %.0f\n", sp0, sp50, sp100);
    expect("PING SPREAD 0: the repeats centred (L == R), 100: left / right (|L-R| large), 50 between", sp0 < 1.0 && sp100 > 1000.0 && sp50 > 0.3 * sp100 && sp50 < 0.7 * sp100);
    printf("duck: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
