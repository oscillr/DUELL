/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.5 (SPEC 5c.2): the delay's MODULATION (fx_bus.c dly_mod; G_DMDEP DEPTH, G_DMRATE RATE). On mode_test.c's rig.
 *   build/host/dlymod_test   (DUELL/host_tests.sh "dlymod")
 *   OFF      DEPTH 0: RATE changes nothing and L == R (the plain delay) in STEREO; the render's hash is printed (the same as the firmware
 *            before the modulation: the hash is checked against it by building this file on that commit with -DDLYMOD_BASE=1)
 *   STEREO   DEPTH > 0: L and R differ (the image widens), the output stays bounded, no click (largest step against the plain delay's),
 *            also while DEPTH is turned up and down in jumps; LAYER the same; PING-PONG ON: bounded, no click
 *   DUEL     part 1 alone: the other channel is digital silence (mode_test's leak map has the same case); the modulation changes the sound
 *   COST     instructions per sample of the buses with DEPTH 0 and 60 (STEREO, PING-PONG, DUEL: two buses), the kernel's counter */
#define MODE_TEST_NO_MAIN 1
#include "mode_test.c"

#ifdef DLYMOD_BASE
#define SET_DM(d, r) ((void)0)
#else
#define SET_DM(d, r) do { song.g[G_DMDEP] = (int16_t)(d); song.g[G_DMRATE] = (int16_t)(r); } while (0)
#endif

static uint64_t dm_hash(const int32_t *x, uint32_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < n; i++) {
        h ^= (uint32_t)x[i];
        h *= 1099511628211ull;
    }
    return h;
}
/* a held chord into the delay (send 127, MIX 127, FDBK 60, 1/8 at 120 BPM), 3 s; steps: DEPTH changes at 1.0 s (->d2) and 2.0 s (->0) when d2 >= 0 */
static void scene(uint32_t mode, uint32_t pp, int d1, int r1, int d2)
{
    uint32_t k, ms = FS / 1000u;
    dsp_clean(mode);
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    song.g[G_DTIME] = 1;
    song.g[G_DFDBK] = 60;
    song.g[G_DMIX] = 127;
    song.g[G_DCOLOR] = 127;
    song.g[G_DMODE] = (int16_t)(pp ? DMD_PING : DMD_DIGITAL);
    song.g[G_PINGSPREAD] = 100;
    for (k = 0; k < 2u; k++) {
        host_preset(&trk[k], eng_of("ANALOG"), 0);
        trk[k].p[P_TRIM] = 100;
        trk[k].p[P_PAN] = 0;
        trk[k].p[P_SUS] = 127;
        trk[k].p[P_AMODE] = 0;
        trk[k].p[P_CHOR] = trk[k].p[P_REV] = 0;
        trk[k].p[P_DLY] = 127;
    }
    SET_DM(d1, r1);
    trk_note_on(&trk[0], 52, 100);
    if (mode != DM_SPLIT)
        trk_note_on(&trk[0], 59, 100);
    rec_n = 0;
    mblk(1000u * ms / CTL);
    if (d2 >= 0)
        SET_DM(d2, r1);
    mblk(1000u * ms / CTL);
    if (d2 >= 0)
        SET_DM(0, r1);
    mblk(1000u * ms / CTL);
}
static double max_lr(void)
{
    double m = 0;
    for (uint32_t i = 0; i < rec_n; i++) {
        double d = fabs((double)rec_l[i] - rec_r[i]);
        m = d > m ? d : m;
    }
    return m;
}
static int32_t dm_step_of(const int32_t *x)
{
    int32_t m = 0;
    for (uint32_t i = 1; i < rec_n; i++) {
        int32_t d = x[i] - x[i - 1];
        m = (d < 0 ? -d : d) > m ? (d < 0 ? -d : d) : m;
    }
    return m;
}
static double diff_rms(const int32_t *a, const int32_t *b, uint32_t n)
{
    double s = 0;
    for (uint32_t i = 0; i < n; i++)
        s += ((double)a[i] - b[i]) * ((double)a[i] - b[i]);
    return sqrt(s / n);
}
static int32_t keep_l[MAXB * CTL], keep_r[MAXB * CTL];
/* instructions per sample of mix_block, 2 s after 0.5 s to settle, a held chord (the parts' own cost is the same for both settings) */
static double cost(uint32_t mode, uint32_t pp, int d)
{
    uint64_t i0;
    uint32_t nb = 2u * FS / CTL;
    scene(mode, pp, d, 44, -1);
    rec_n = 0;
    i0 = instr_now();
    for (uint32_t b = 0; b < nb; b++)
        mix_block(ob, CTL);
    return i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
}
int main(void)
{
    uint32_t n;
    double off_lr, on_lr, ref_pk, on_pk, pk2;
    int32_t ref_step, on_step, jump_step, ref_step_l;
    scene(DM_STEREO, 0, 0, 44, -1);
    n = rec_n;
    memcpy(keep_l, rec_l, sizeof(int32_t) * n);
    memcpy(keep_r, rec_r, sizeof(int32_t) * n);
    off_lr = max_lr();
    ref_pk = peak_of(rec_l, n);
    ref_step = dm_step_of(rec_l);
    printf("dlymod: DEPTH 0 STEREO hash %016llx (L %d, peak %.0f, largest step %d)\n", (unsigned long long)dm_hash(rec_l, n), (int)(n), ref_pk, ref_step);
#ifdef DLYMOD_BASE
    printf("dlymod: BASE cost: STEREO %.1f, PING-PONG %.1f, DUEL %.1f\n", cost(DM_STEREO, 0, 0), cost(DM_STEREO, 1, 0), cost(DM_SPLIT, 0, 0));
    return 0;
#endif
    scene(DM_STEREO, 0, 0, 80, -1);
    expect("OFF: DEPTH 0, RATE 80 instead of 44: the same render bit for bit (RATE is inert at DEPTH 0)",
           rec_n == n && !memcmp(keep_l, rec_l, sizeof(int32_t) * n) && !memcmp(keep_r, rec_r, sizeof(int32_t) * n));
    expect("OFF: the plain delay is stereo-equal (L == R) in STEREO", off_lr == 0.0);
    scene(DM_STEREO, 0, 60, 44, -1);
    on_lr = max_lr();
    on_pk = peak_of(rec_l, rec_n);
    on_step = dm_step_of(rec_l);
    ref_step_l = ref_step;
    printf("dlymod: DEPTH 60 STEREO: max |L-R| %.0f, peak %.0f (plain %.0f), largest step %d (plain %d), difference to plain %.0f rms\n", on_lr, on_pk, ref_pk,
           on_step, ref_step_l, diff_rms(rec_l, keep_l, n));
    expect("STEREO DEPTH 60: left and right differ (the image widens)", on_lr > 500.0);
    expect("STEREO DEPTH 60: the sound moves against the plain delay and stays bounded (peak under 1.3 x the plain, under full scale)",
           diff_rms(rec_l, keep_l, n) > 100.0 && on_pk < 1.3 * ref_pk && on_pk < 32767);
    expect("STEREO DEPTH 60: no click (the largest sample step under 1.3 x the plain delay's)", on_step < 1.3 * ref_step_l + 50);
    scene(DM_STEREO, 0, 100, 44, 20);                    /* DEPTH 100, then 20 at 1 s, then 0 at 2 s: jumps of the knob */
    jump_step = dm_step_of(rec_l);
    pk2 = peak_of(rec_l, rec_n);
    printf("dlymod: DEPTH 100 -> 20 -> 0 in jumps: largest step %d, peak %.0f\n", jump_step, pk2);
    expect("STEREO, DEPTH turned 100 -> 20 -> 0 in jumps: no click, bounded", jump_step < 1.3 * ref_step_l + 50 && pk2 < 1.3 * ref_pk);
    scene(DM_LAYER, 0, 60, 44, -1);
    expect("LAYER DEPTH 60: left and right differ, bounded", max_lr() > 500.0 && peak_of(rec_l, rec_n) < 1.3 * ref_pk);
    scene(DM_STEREO, 1, 0, 44, -1);
    ref_pk = peak_of(rec_l, rec_n);
    ref_step_l = dm_step_of(rec_l);
    memcpy(keep_l, rec_l, sizeof(int32_t) * rec_n);
    scene(DM_STEREO, 1, 60, 44, -1);
    printf("dlymod: PING-PONG, DEPTH 60: peak %.0f (plain %.0f), largest step %d (plain %d)\n", (double)peak_of(rec_l, rec_n), ref_pk, dm_step_of(rec_l), ref_step_l);
    expect("PING-PONG ON, DEPTH 60: the sound moves, bounded, no click",
           diff_rms(rec_l, keep_l, rec_n) > 100.0 && peak_of(rec_l, rec_n) < 1.3 * ref_pk && dm_step_of(rec_l) < 1.3 * ref_step_l + 50);
    scene(DM_SPLIT, 0, 0, 44, -1);
    memcpy(keep_l, rec_l, sizeof(int32_t) * rec_n);
    scene(DM_SPLIT, 0, 60, 44, -1);
    printf("dlymod: DUEL part 1 alone, DEPTH 60: the right channel's peak %d\n", peak_of(rec_r, rec_n));
    expect("DUEL, part 1 alone: the right channel stays digital silence (no coupling of the buses), the left moves against the plain delay",
           peak_of(rec_r, rec_n) == 0 && diff_rms(rec_l, keep_l, rec_n) > 100.0);
    {
        double c0 = cost(DM_STEREO, 0, 0), c1 = cost(DM_STEREO, 0, 60), p0 = cost(DM_STEREO, 1, 0), p1 = cost(DM_STEREO, 1, 60);
        double d0 = cost(DM_SPLIT, 0, 0), d1 = cost(DM_SPLIT, 0, 60);
        printf("dlymod: COST instructions / sample (whole mix, a held chord): STEREO off %.1f, on %.1f (+%.1f); PING-PONG off %.1f, on %.1f (+%.1f); DUEL off %.1f, on %.1f (+%.1f for two buses)\n",
               c0, c1, c1 - c0, p0, p1, p1 - p0, d0, d1, d1 - d0);
        expect("COST: the modulation adds under 65 instructions per sample (STEREO: two taps and the side; PING-PONG: under 35; DUEL: under 45 for its two buses)",
               c0 == 0.0 || (c1 - c0 < 65.0 && p1 - p0 < 35.0 && d1 - d0 < 45.0));
    }
    printf("dlymod: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
