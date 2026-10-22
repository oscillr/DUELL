/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.25: the part SAT (P_DIST, fx.c track_dist) without a zipper. The owner heard a crackle changing a part's SAT on an FM part: the drive
 * (gain, tone, make-up, wet blend) was read once a block and stepped at the block boundary. Now the stored SAT is followed a block at a time
 * (fx.c dist_follow, like pan_follow; not hot code) and track_dist gets the followed value.
 *   STEP    a held note (ANALOG, PHASE, FM6; SAT 0 -> 100, 20 -> 127, 127 -> 20, 100 -> 0, 5 -> 11, 0 -> 127 at block 60): the largest sample-to-sample step of the
 *           output around it is within 1.5 x the largest step of the steady renders (SAT at either end all along), and at every block boundary the step is within
 *           1.5 x the steady renders' step at that very sample + 8 % of the peak (before the fix: up to 45 %; the oscillator does not depend on SAT, so the renders are in phase)
 *   SETTLES the followed SAT lands exactly: 115 blocks after the step (the whole range takes ~95) the render is bit for bit the render that had the new SAT all along
 *   RETRIG  (0.2.25, owner: a click on some notes) WAVE and ANALOG, SAT 2, matrix LFO 2 (sine, ~1.118 Hz, TRIG NOTE) -> SAT depth 98: a second note-on (the first note released) restarts the LFO
 *           at a high or low phase and the MODULATED SAT jumps (up to ~95 steps). No block boundary in the 20 blocks after the second note-on steps more than 1.5 x the step of
 *           the control render there (the same notes, the LFO restarted at the phase it had: no jump) + 5 % of the peak (before the fix: 7 to 11 %, ANALOG 9 and 12 %, now 2 to 3 %)
 *   SLOW    the same LFO free-running (TRIG FREE) on a held note: the SAT the block ran at stays within 2 steps of the modulated SAT (mod_begin's), over 3 s
 *   REST    a SAT that never moves is bit for bit as before: tests/golden.txt (regress), multi_render_golden.txt (mrender) and mode_scn.h's hashes (mode, pan) are unchanged
 * Built and run by DUELL/host_tests.sh (name: sat) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("sat: FAIL %s\n", what);
        bad++;
    }
    return ok;
}
#define NB 220u
#define NS (NB * CTL)
static int32_t ra[NS], rb[NS], rz[NS];
static void clean(void)                                 /* the DSP as at power-on (bassmono_test.c dsp_clean), STEREO */
{
    memset(trk, 0, sizeof trk);
    memset(&trk[0].mdw, 0, sizeof trk[0].mdw);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(&trk[0].mxw, 0, sizeof trk[0].mxw);
    memset(ins, 0, sizeof ins);
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_owners, 0, sizeof midi_owners);
    memset(mchord, 0, sizeof mchord);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(kb_chn, 0, sizeof kb_chn);
    memset(pan_x, 0, sizeof pan_x);
    memset(pan_g, 0, sizeof pan_g);
    memset(pan_gv, 0, sizeof pan_gv);
    mix_bal = 0;
    mix_xf = mix_split = 0;
    vage = 0;
    mod_seed = 0x2545F491u;
    fm1_in.notes = 0;
    kb_prev = 0;
    mi_r = mi_w = 0;
    duell_mode_force(DM_SPLIT);
    duell_mode_force(DM_STEREO);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    memset(&bm, 0, sizeof bm);
    host_tracks_init();
    song.g[G_RTYPE] = 0;
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}

/* a held note on part 1 (the engine, no sends, no insert), its SAT s0, then s1 from block `at`; the mono sum before the master */
static void render(int32_t *out, const char *eng, int32_t s0, int32_t s1, uint32_t at)
{
    static int32_t o[2 * CTL];
    uint32_t b, i;
    clean();
    host_preset(&trk[0], scn_eng(eng), 1);
    trk[0].p[P_VOICE] = V_MONO;
    trk[0].p[P_SPRD] = 0;
    trk[0].p[P_PAN] = 0;
    trk[0].p[P_DIST] = (int16_t)s0;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_TRIM] = 100;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_ATK] = 0;
    for (b = 0; b < NB; b++) {
        if (b == at)
            trk[0].p[P_DIST] = (int16_t)s1;
        if (b == 4u)
            trk_note_on(&trk[0], 57, 120);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = mix_l[i] + mix_r[i];
    }
}
static int32_t max_step(const int32_t *x, uint32_t from, uint32_t to)
{
    int32_t m = 0;
    uint32_t i;
    for (i = from + 1u; i < to; i++) {
        int32_t d = x[i] - x[i - 1u];
        d = d < 0 ? -d : d;
        m = d > m ? d : m;
    }
    return m;
}
/* the block boundaries: the step into the first sample of each block 60..110 against the same step of the two steady renders (the oscillator does not depend on SAT: the three renders
 * are in phase sample for sample). The worst excess over 1.5 x the larger steady step there, in 1/1000 of the steady render's peak: a drive that jumps at the boundary is a click
 * on a smooth stretch that the steady corner (the global largest step) hides */
static int32_t boundary_excess(const int32_t *b, const int32_t *a, const int32_t *z)
{
    int32_t pk = 1, w = 0;
    uint32_t i, k;
    for (i = 0; i < NS; i++) {
        int32_t x = a[i] < 0 ? -a[i] : a[i], y = z[i] < 0 ? -z[i] : z[i];
        pk = x > pk ? x : pk;
        pk = y > pk ? y : pk;
    }
    for (k = 60u; k < 110u; k++) {
        int32_t i2 = (int32_t)(k * CTL), db = b[i2] - b[i2 - 1], da = a[i2] - a[i2 - 1], dz = z[i2] - z[i2 - 1], m, e;
        db = db < 0 ? -db : db;
        da = da < 0 ? -da : da;
        dz = dz < 0 ? -dz : dz;
        m = da > dz ? da : dz;
        e = (int32_t)((int64_t)(db - m * 3 / 2) * 1000 / pk);
        w = e > w ? e : w;
    }
    return w;
}
static int same_from(const int32_t *a, const int32_t *b, uint32_t from)
{
    uint32_t i;
    for (i = from; i < NS; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}
static void step_case(const char *eng, int32_t s0, int32_t s1)
{
    char what[200];
    int32_t st, ss0, ss1, steady, ex;
    render(ra, eng, s0, s0, 60u);
    render(rb, eng, s0, s1, 60u);
    render(rz, eng, s1, s1, 60u);
    ss0 = max_step(ra, 50u * CTL, 150u * CTL);
    ss1 = max_step(rz, 50u * CTL, 150u * CTL);
    steady = ss0 > ss1 ? ss0 : ss1;
    st = max_step(rb, 50u * CTL, 150u * CTL);
    snprintf(what, sizeof what, "%s: SAT %d -> %d at block 60: largest sample step %d, steady %d / %d (limit 1.5 x %d)", eng, s0, s1, st, ss0, ss1, steady);
    expect(what, st * 2 <= steady * 3);
    ex = boundary_excess(rb, ra, rz);
    snprintf(what, sizeof what, "%s: SAT %d -> %d: no block boundary steps more than 1.5 x the steady renders' step there + 8 %% of the peak (worst excess %d/1000)", eng, s0, s1, ex);
    expect(what, ex <= 80);
    snprintf(what, sizeof what, "%s: SAT %d -> %d: settled, from block 175 on bit for bit the render with SAT %d all along", eng, s0, s1, s1);
    expect(what, same_from(rb, rz, 175u * CTL));
    printf("sat: %-6s %3d -> %3d: largest step %6d, steady %6d / %6d  (%d %%), boundary excess %d/1000\n", eng, s0, s1, st, ss0, ss1, steady ? st * 100 / steady : 0, ex);
}
/* two notes on part 1 (poly, ENGINE eng, SAT 2, LFO 2 -> SAT 98, the sine at ~1.118 Hz, TRIG NOTE): note 57 at block 4 with the LFO started at PHS ph1 (32 = the sine's top), released at block 30,
 * note 64 at block 60 with PHS ph2 (the LFO restarts there: 0 = the sine's zero, a jump of the modulated SAT from ~95 down to 2). With free = 1 the LFO runs free (TRIG FREE) */
static uint32_t lfo_idx(void)                          /* the LFO RATE nearest 1.118 Hz */
{
    uint32_t i, best = 0;
    int32_t bd = 1 << 30;
    for (i = 0; i < 128u; i++) {
        int32_t d = (int32_t)LFO_HZ_X100[i] - 112;
        d = d < 0 ? -d : d;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}
static void render_retrig(int32_t *out, const char *eng, int ph1, int ph2, int free)
{
    static int32_t o[2 * CTL];
    uint32_t b, i;
    clean();
    host_preset(&trk[0], scn_eng(eng), 1);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SPRD] = 0;
    trk[0].p[P_PAN] = 0;
    trk[0].p[P_DIST] = 2;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_TRIM] = 100;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_REL] = 100;
    trk[0].p[P_L2RATE] = (int16_t)lfo_idx();
    trk[0].p[P_L2WAVE] = 0;
    trk[0].p[P_L2FADE] = 0;
    trk[0].p[P_L2SYNC] = 0;
    trk[0].p[P_L2POL] = 0;
    trk[0].p[P_L2TRIG] = (int16_t)free;
    trk[0].p[P_L2PHASE] = (int16_t)ph1;
    trk[0].p[P_M1SRC] = MS_LFO2;
    trk[0].p[P_M1DST] = MD_DIST;
    trk[0].p[P_M1AMT] = 98;
    for (b = 0; b < NB; b++) {
        if (b == 4u)
            trk_note_on(&trk[0], 57, 120);
        if (b == 30u)
            trk_note_off(&trk[0], 57);
        if (b == 60u) {
            trk[0].p[P_L2PHASE] = (int16_t)ph2;
            trk_note_on(&trk[0], 64, 120);
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = mix_l[i] + mix_r[i];
    }
}
static void retrig_case(const char *eng, int ph2)
{
    char what[200];
    int32_t pk = 1, w;
    uint32_t i, k;
    render_retrig(ra, eng, 32, ph2, 0);
    render_retrig(rz, eng, 32, 32, 0);                  /* control: the LFO restarts at the top again */
    for (i = 0; i < NS; i++) {
        int32_t x = ra[i] < 0 ? -ra[i] : ra[i], y = rz[i] < 0 ? -rz[i] : rz[i];
        pk = x > pk ? x : pk;
        pk = y > pk ? y : pk;
    }
    w = 0;
    for (k = 60u; k < 80u; k++) {                       /* every block boundary: the step against the control's step at that very sample (the oscillators are in phase) */
        int32_t i2 = (int32_t)(k * CTL), da = ra[i2] - ra[i2 - 1], dc = rz[i2] - rz[i2 - 1], e;
        da = da < 0 ? -da : da;
        dc = dc < 0 ? -dc : dc;
        e = (int32_t)((int64_t)(da - dc * 3 / 2) * 1000 / pk);
        w = e > w ? e : w;
    }
    snprintf(what, sizeof what, "%s: LFO 2 -> SAT 98 restarted at PHS %d: no block boundary steps more than 1.5 x the control's step there + 8 %% of the peak (worst excess %d/1000, limit 50)", eng, ph2, w);
    expect(what, w <= 50);
    printf("sat: %-6s retrigger PHS 32 -> %3d: worst boundary excess %d/1000 of the peak %d\n", eng, ph2, w, pk);
}
static void slow_case(const char *eng)
{
    static int32_t o[2 * CTL];
    char what[200];
    uint32_t b;
    int32_t dev = 0, lo = 999, hi = -1;
    clean();
    host_preset(&trk[0], scn_eng(eng), 1);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SPRD] = 0;
    trk[0].p[P_DIST] = 2;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_L2RATE] = (int16_t)lfo_idx();
    trk[0].p[P_L2TRIG] = 1;
    trk[0].p[P_M1SRC] = MS_LFO2;
    trk[0].p[P_M1DST] = MD_DIST;
    trk[0].p[P_M1AMT] = 98;
    for (b = 0; b < 3u * FS / CTL; b++) {
        int32_t want, got;
        if (b == 4u)
            trk_note_on(&trk[0], 57, 120);
        mod_begin(&trk[0]);                             /* (the modulated SAT this block will run at; mod_end puts the stored back) */
        want = trk[0].p[P_DIST];
        mod_end(&trk[0]);
        mix_block(o, CTL);
        got = (dist_x[0] + 128) >> 8;
        if (b > 8u) {
            int32_t d = got - want;
            d = d < 0 ? -d : d;
            dev = d > dev ? d : dev;
            lo = want < lo ? want : lo;
            hi = want > hi ? want : hi;
        }
    }
    snprintf(what, sizeof what, "%s: LFO 2 -> SAT 98 free-running at ~1.118 Hz: the followed SAT is within 2 steps of the modulated one (largest deviation %d, SAT swings %d..%d)", eng, dev, lo, hi);
    expect(what, dev <= 2 && hi - lo > 60);
    printf("sat: %-6s free LFO 2 -> SAT: largest deviation %d step(s), SAT %d..%d\n", eng, dev, lo, hi);
}
int main(void)
{
    static const char *const E[] = {"ANALOG", "PHASE", "FM6"};
    static const int16_t S[][2] = {{0, 100}, {20, 127}, {127, 20}, {100, 0}, {5, 11}, {0, 127}};
    uint32_t e, k;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (e = 0; e < 3u; e++)
        for (k = 0; k < sizeof S / sizeof S[0]; k++)
            step_case(E[e], S[k][0], S[k][1]);
    retrig_case("WAVE", 0);
    retrig_case("WAVE", 96);
    retrig_case("ANALOG", 0);
    retrig_case("ANALOG", 96);
    slow_case("WAVE");
    slow_case("ANALOG");
    printf("sat: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
