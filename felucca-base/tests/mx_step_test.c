/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.25 (owner, the matrix round; release_drafts/MATRIX_DESTINATIONS.md): matrix moves without a step.
 *   SLEW   mod.c mx_slew: each slot's source value moves at most MX_SLEW a block. A MODW 0 -> 127 jump at a block boundary on a held sine (PHASE, DCW 0)
 *          through the multimode filter (CUT, RES 40): the largest 2nd difference after the jump, in 1/1000 of the peak, at most 60 (the study: 377 before,
 *          21 with its 6-block slew; 5 blocks here). An LFO 1 sine at the top rate (index 127) and an ENV 2 with a 10 ms attack pass the slew unchanged (the per-block value with
 *          and without it are the same: tested on mx_slew with the values the LFO / envelope produce)
 *   RAMP   fx.c mix_ramp: the same jump on PAN (the left channel), PATCH LEVEL (TRIM, from -12 dB) and the DELAY and CHORUS sends (the wet bus): at most 40
 *          (the study: 613, 575, 385, 221 before; 19, 17, 11, 6 with the ramp); an 8 Hz LFO on PAN: at most 10 (47 before)
 *   BACK   the route taken away while the value is up (SRC set to OFF, an edit): the way back to the stored value is a line over one block (PAN at most 40,
 *          PATCH LEVEL from +7.5 dB to -12 dB at most 80; as a step it was 575: the source slew does not apply, the slot is gone)
 *   SAT    fx.c track_dist_ramp: SAT 0 + MODW -> SAT, jump into the blend zone (0..12): at most 40 (the study: 103 before, 23 after)
 *   MORPH  (hunt1, release_drafts/CLICK_HUNT1.md) the MORPH fader moves PAN / the sends / SAT by whole steps: the open morph window counts as a route for the mix ramp
 *          (mod.c mod_mixmv): the fader 0 -> 127 at a block boundary on the CHORUS and DELAY sends, at most 40 (241 before)
 *   SAT 0  (hunt1) SAT's first block out of 0 (and the last into 0): the followed SAT is under half a step while the integer is still 0, and the blend ran from nothing at
 *          the first block with the integer 1 (a cold wet at a 4 % blend): SAT 0 -> +2 by the matrix and by the MORPH fader (0 -> 1) on a sine, and the way back into 0 (a route switched off at SAT +3), at most 10 (17, 29 and 15 before)
 *   SPREAD (hunt1) the same jumps with SPREAD 38 (the voices spread around PAN; mix_spread, hot code, mixed a block at one set of gains and ran SAT at its integer): fx.c
 *          spread_run / spread_post: PAN, PATCH LEVEL, the sends, SAT, an 8 Hz LFO on PAN and PATCH LEVEL, the route taken away while up, the MORPH fader on the sends,
 *          SAT and PAN, at most as above
 *   REST   a part with no matrix route on PAN / LEVEL / a send mixes as before: the ramp is not asked for (mod_mixmv); the hashes of pan_test, mode_test and
 *          mrender hold the bit-for-bit side
 *   PHASE  (hunt4, release_drafts/HUNT4.md) #31 BRAZEN's whistle: PHASE builds its bend from DCW once a block, so the ENV (the amp envelope) sweeping DCW in the attack stepped the
 *          bend at every block seam (a buzz at 1,378 Hz and its harmonics, -62 .. -77 dBFS above 3 kHz). eng_phase.c phase_render_0 carries the step as an offset that dies away: the
 *          mean |2nd difference| at the block seams over the attack, against the mean elsewhere, at most 2.5 (3.9 at C4 and 11.7 at C3 before); a steady note (ENV 0) is bit for bit as before
 * Built and run by DUELL/host_tests.sh (name: mxstep) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("mxstep: %-110s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}
#define NB 400u
#define NS (NB * CTL)
#define JB 200u
static int32_t ra[NS], rb[NS];
static int l2_ov;                                       /* (hunt1: the VOICE engine with LFO 2 on its own SHAPE depth (127) with this WAVE (1 TRI, 2 SAW, 3 SQR), ~8 Hz: the direct route, not the matrix's) */
static int sprd_ov;                                     /* (hunt1: SPREAD for the render, 0 = off; >0 also makes the part POLY, so SPREAD's mix runs) */
static int mv_id = -1, mv_a, mv_b;                      /* (hunt1: a MORPH between two values of one parameter: its id, End A, End B; the fader jumps 0 -> 127 at JB) */
static int tw_on, tw_cut = -5;                                       /* cloud job E: the TWIN FILTER on (TYPE LP, filter 1 CUT 30): the matrix's TWIN destinations */
static int sus_ov = -1;                                 /* (T0229: the AMP ENV DECAY check: a sustain below full, so the decay is heard) */
static void clean(void)
{
    uint32_t i;
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
    memset(sp_cur, 0, sizeof sp_cur);
    memset(sp_prv, 0, sizeof sp_prv);
    memset(sp_go, 0, sizeof sp_go);
    memset(sp_ok, 0, sizeof sp_ok);
    memset(dist_x, 0, sizeof dist_x);
    memset(dist_live, 0, sizeof dist_live);
    memset(duck_st, 0, sizeof duck_st);
    memset(mod_goff, 0, sizeof mod_goff);
    for (i = 0; i < NLIVE; i++)
        mx_sl[i].ok = 0, mx_ok[i] = 0;
    memset(mx_was, 0, sizeof mx_was); memset(mod_mixmv, 0, sizeof mod_mixmv);
    trk[0].mgon = trk[1].mgon = trk[0].mdk = trk[1].mdk = 0, mod_gw = 0;
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
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    memset(&bm, 0, sizeof bm);
    host_tracks_init();
    song.g[G_RTYPE] = 0;
    song.g[G_RLOCUT] = 0;
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}

enum { O_MIX, O_L, O_WET, O_SC, O_SD, O_SIDE };   /* (O_SC, O_SD: the chorus and delay sends, as the plain mix hands them to the buses) */
enum { S_JUMP, S_LFO8, S_BACK, S_CYC };
static int sub_wv = -1;                                 /* (cloud job F: >= 0 plays the SUB LAYER of that WAVE, OCTAVE 0, from LEVEL 0 and the route's) */
/* a held sine (PHASE, DCW 0) on part 1, one matrix route src -> dst at amt; base: the destination's stored value; S_JUMP: MODW 0 -> 127 at JB, S_LFO8 LFO 1
 * sine at ~8 Hz, S_BACK: MODW 127 all along and the route's SRC set to OFF at JB. send: the sends at 100 (else 0) */
static void render(int32_t *out, int dst, int amt, int src, int base, int send, int o, int filt)
{
    static int32_t ob[2 * CTL];
    track_t *t = &trk[0];
    uint32_t b, i;
    clean();
    host_preset(t, scn_eng(l2_ov ? "VOICE" : "PHASE"), 0);
    for (i = 0; i < 7u; i++)
        t->p[P_E0 + i] = 0;                             /* WAVE 1 SAW, WAVE 2 -, DCW 0, ENV 0, DTN 0, MIX, SUB 0: a sine */
    t->p[P_ED_FLT] = t->p[P_ED_SHP] = t->p[P_ED_PIT] = 0;
    t->p[P_LD_PIT] = t->p[P_LD_FLT] = t->p[P_LD_SHP] = t->p[P_LD_AMP] = 0;
    t->p[P_VOICE] = (int16_t)(sprd_ov ? V_POLY : V_MONO);
    t->p[P_SPRD] = (int16_t)sprd_ov;
    t->p[P_PAN] = 0;
    t->p[P_DIST] = 0;
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = (int16_t)(send ? 100 : 0);
    t->p[P_TRIM] = 100;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = (int16_t)(sus_ov >= 0 ? sus_ov : 127);
    t->p[P_GLIDE] = 0;
    t->p[P_DKSRC] = 3;
    t->p[P_ITYPE] = 0;
    if (filt) {                                         /* the multimode filter: CUT low, RES 40, LP 12 dB */
        t->p[P_FREEA] = 30;
        t->p[P_FREEB] = 40;
        t->p[P_FREEC] = 0;
        t->p[P_FREED] = 1;
    }
    if (tw_on) {
        t->p[P_TWTYPE] = (int16_t)tw_on;                /* filter 2 on filter 1's CUT 30: RES 40, OFFSET tw_cut, SERIES, BLEND 0, DRIVE 20 */
        t->p[P_RES2] = 40;
        t->p[P_CUT2] = (int16_t)tw_cut;
        t->p[P_DRV2] = 20;
        XV(t, X_LINK) = 1;
    }
    if (dst > MD_AMP && dst != MD_CUT)
        t->p[mod_param((uint32_t)dst)] = (int16_t)base;
    if (sub_wv >= 0) {
        XV(t, X_SUBWAVE) = (int16_t)sub_wv;
        XV(t, X_SUBOCT) = 2;
        XV(t, X_SUBLVL) = (int16_t)base;
    }
    if (src == S_CYC) {                                 /* CYCLE LOOP, RISE / FALL 2 blocks and 5 */
        XV(t, X_CYMODE) = 0;
        XV(t, X_CYRISE) = 0;
        XV(t, X_CYFALL) = 20;
    }
    t->p[P_M1SRC] = (int16_t)(src == S_LFO8 ? MS_LFO : src == S_CYC ? MS_CYCLE : MS_MODW);
    t->p[P_M1DST] = (int16_t)dst;
    t->p[P_M1AMT] = (int16_t)amt;
    if (l2_ov) {
        uint32_t k, best = 0;
        int32_t bd = 1 << 30;
        for (k = 0; k < 128u; k++) {
            int32_t dd = (int32_t)LFO_HZ_X100[k] - 800;
            dd = dd < 0 ? -dd : dd;
            if (dd < bd)
                bd = dd, best = k;
        }
        t->p[P_E0] = t->p[P_E0 + 1] = 40, t->p[P_E0 + 4] = 64, t->p[P_E0 + 6] = 64;   /* the VOICE engine (formants): VOWL 1 / 2, BUZZ, Q; SHAPE is its BUZZ */
        t->p[P_L2WAVE] = (int16_t)l2_ov, t->p[P_L2RATE] = (int16_t)best, t->p[P_L2SHP] = 127, t->p[P_L2FADE] = 0, t->p[P_L2SYNC] = 0, t->p[P_L2TRIG] = 1, t->p[P_L2POL] = 0;
    }
    if (src == S_LFO8) {
        uint32_t k, best = 0;
        int32_t bd = 1 << 30;
        for (k = 0; k < 128u; k++) {
            int32_t dd = (int32_t)LFO_HZ_X100[k] - 800;
            dd = dd < 0 ? -dd : dd;
            if (dd < bd)
                bd = dd, best = k;
        }
        t->p[P_LRATE] = (int16_t)best;
        t->p[P_LWAVE] = 0, t->p[P_LFADE] = 0, t->p[P_LSYNC] = 0, t->p[P_LPOL] = 0, t->p[P_LTRIG] = 1;
    }
    if (mv_id >= 0) {                                   /* (hunt1) a MORPH of one value: End A = the Original, End B = the Original with mv_id at mv_b */
        int8_t *ea, *eb;
        t->p[mv_id] = (int16_t)mv_a;
        morph_set_amount(100);
        morph_roll(0);
        morph_roll(1);
        ea = mph[0].h[0][mph[0].top[0]];
        eb = mph[0].h[1][mph[0].top[1]];
        memcpy(ea, mph[0].base, P_COUNT);
        memcpy(eb, mph[0].base, P_COUNT);
        eb[mv_id] = (int8_t)mv_b;
        mph[0].sm_r = 32768;
        morph_build(&mph[0]);
    }
    for (b = 0; b < NB; b++) {
        if (b == 4u)
            trk_note_on(t, 57, 110);
        if (mv_id >= 0)
            mph[0].fader = (uint8_t)(b >= JB ? 127 : 0);
        t->mw = (uint8_t)(src == S_JUMP ? (b >= JB ? 127 : 0) : src == S_BACK ? 127 : 0);
        if (src == S_BACK && b == JB)
            t->p[P_M1SRC] = 0;
        mix_block(ob, CTL);
        for (i = 0; i < CTL; i++)
            out[b * CTL + i] = o == O_WET ? wet[i] : o == O_SC ? send_c[i] : o == O_SD ? send_d[i] : o == O_SIDE ? mix_l[i] - mix_r[i] : o == O_L ? mix_l[i] : mix_l[i] + mix_r[i];
    }
}
static int32_t d2(const int32_t *x, uint32_t i) { int32_t v = x[i] - 2 * x[i - 1] + x[i - 2]; return v < 0 ? -v : v; }
/* the largest |2nd difference| of x in blocks k0..k1, in 1/1000 of x's peak there */
static int32_t d2max(const int32_t *x, uint32_t k0, uint32_t k1)
{
    int32_t m = 0, pk = 1;
    uint32_t i;
    for (i = k0 * CTL; i < k1 * CTL; i++) {
        int32_t a = x[i] < 0 ? -x[i] : x[i];
        pk = a > pk ? a : pk;
        if (i >= 2u) {
            int32_t v = d2(x, i);
            m = v > m ? v : m;
        }
    }
    return (int32_t)((int64_t)m * 1000 / pk);
}
/* the recorder's click rule on x[lo..hi): hits merged within 16 samples are one click */
static int32_t clicks(const int32_t *x, uint32_t lo, uint32_t hi)
{
    uint32_t i, j, last = 0;
    int32_t n = 0, w = 1764;
    for (i = lo > (uint32_t)w ? lo : (uint32_t)w; i + (uint32_t)w < hi; i++) {
        int32_t a = d2(x, i), m = 0;
        if (a <= 16)
            continue;
        for (j = i - (uint32_t)w; j < i - 16u; j++)
            m = d2(x, j) > m ? d2(x, j) : m;
        for (j = i + 17u; j <= i + (uint32_t)w; j++)
            m = d2(x, j) > m ? d2(x, j) : m;
        if (a > 3 * m) {
            if (!last || i - last > 16u)
                n++;
            last = i;
        }
    }
    return n;
}
/* the largest 2nd difference at a block boundary (the first two samples of a block) in x[lo..hi), in 1/1000 of the mean |2nd difference| in the block (the other samples) around it */
static int32_t seams(const int32_t *x, uint32_t lo, uint32_t hi)
{
    uint32_t k, i;
    int64_t best = 0;
    for (k = lo / CTL + 1u; k + 1u < hi / CTL; k++) {
        int64_t sum = 0, sm = d2(x, k * CTL) > d2(x, k * CTL + 1u) ? d2(x, k * CTL) : d2(x, k * CTL + 1u);
        for (i = (k - 1u) * CTL + 2u; i < (k + 1u) * CTL; i++)
            if (i != k * CTL && i != k * CTL + 1u)
                sum += d2(x, i);
        sum = sum / (2 * CTL - 4) + 1;
        if (sm * 1000 / sum > best)
            best = sm * 1000 / sum;
    }
    return (int32_t)best;
}
static int same(const int32_t *a, const int32_t *b)
{
    uint32_t i;
    for (i = 0; i < NS; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

/* PHASE: #31 BRAZEN's values (SAW, DCW 28, ENV 88, DTN 7, ATK 43), a held C4, the attack and decay (blocks 4..70): mean |2nd difference| at the block seams / elsewhere */
static double phase_seam(int env, int dcw, int dtn, int note)
{
    static int32_t ob[2 * CTL], x[NS];
    track_t *t = &trk[0];
    uint32_t b, i, n0 = 0, n1 = 0;
    double a0 = 0, a1 = 0;
    clean();
    host_preset(t, scn_eng("PHASE"), 0);
    t->p[P_E0] = 0, t->p[P_E0 + 1] = 0, t->p[P_E0 + 2] = (int16_t)dcw, t->p[P_E0 + 3] = (int16_t)env, t->p[P_E0 + 4] = (int16_t)dtn, t->p[P_E0 + 5] = 0, t->p[P_E0 + 6] = 0;
    t->p[P_ED_FLT] = t->p[P_ED_SHP] = t->p[P_ED_PIT] = 0;
    t->p[P_LD_PIT] = t->p[P_LD_FLT] = t->p[P_LD_SHP] = t->p[P_LD_AMP] = 0;
    t->p[P_VOICE] = V_MONO;
    t->p[P_PAN] = 0, t->p[P_DIST] = 0, t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0, t->p[P_TRIM] = 100;
    t->p[P_ATK] = 43, t->p[P_DEC] = 79, t->p[P_SUS] = 108, t->p[P_REL] = 66;
    t->p[P_GLIDE] = 0, t->p[P_DKSRC] = 3, t->p[P_ITYPE] = 0;
    for (b = 0; b < 120u; b++) {
        if (b == 4u)
            trk_note_on(t, (uint32_t)note, 90);
        mix_block(ob, CTL);
        for (i = 0; i < CTL; i++)
            x[b * CTL + i] = mix_l[i];
    }
    for (i = 4u * CTL + 2u; i < 70u * CTL; i++) {
        double d = (double)d2(x, i);
        if (i % CTL == 0 || i % CTL == 1) a0 += d, n0++;        /* the 2nd difference that includes the seam: samples 0 and 1 of a block */
        else a1 += d, n1++;
    }
    return (a0 / n0) / (a1 / n1);
}
int main(void)
{
    char line[160];
    int32_t v;
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    /* SLEW on the filter's cutoff (a per-voice destination) */
    render(rb, MD_CUT, 63, S_JUMP, 0, 0, O_MIX, 1);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "SLEW: MODW 0 -> 127 at a block boundary -> CUT +63 (RES 40): 2nd difference %d / 1000 of the peak (<= 60; 377 before)", v);
    expect(line, v <= 60);
    {   /* the slew passes an LFO and an envelope: the values a full sine at the top rate and a 10 ms attack take a block move less than MX_SLEW */
        uint32_t k, ok = 1;
        int32_t x, y, prev = 0;
        double inc = 2.0 * 3.14159265358979 * (LFO_HZ_X100[127] / 100.0) * CTL / FS;
        clean();
        for (k = 0; k < 2000u; k++) {
            x = (int32_t)(32767.0 * sin(inc * k));
            y = mx_slew(&trk[0], 0, x);
            ok &= y == x;
        }
        snprintf(line, sizeof line, "SLEW: a full sine LFO at the top rate (%.1f Hz) passes unchanged, 2000 blocks", LFO_HZ_X100[127] / 100.0);
        expect(line, ok);
        clean();
        for (k = 0, ok = 1, prev = 0; k < 40u; k++) {    /* a 10 ms attack: 0 -> 32767 over ~14 blocks */
            x = clamp((int32_t)(32767.0 * k * CTL * 1000.0 / FS / 10.0), 0, 32767);
            y = mx_slew(&trk[0], 1, x);
            ok &= y == x;
            prev = x;
        }
        expect("SLEW: an envelope with a 10 ms attack passes unchanged", ok && prev == 32767);
        y = mx_slew(&trk[0], 1, 0);
        expect("SLEW: a jump of a whole swing glides: MX_SLEW a block (5 blocks for 0..1)", y == 32767 - MX_SLEW);
    }
    /* RAMP: PAN, PATCH LEVEL, the sends */
    render(rb, MD_PAN, 63, S_JUMP, 0, 0, O_L, 0);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "RAMP: the jump -> PAN +63 (left channel): %d (<= 40; 613 before)", v);
    expect(line, v <= 40);
    render(rb, MD_PAN, 63, S_LFO8, 0, 0, O_L, 0);
    v = d2max(rb, 100u, NB);
    snprintf(line, sizeof line, "RAMP: an 8 Hz LFO -> PAN +63: %d (<= 10; 47 before)", v);
    expect(line, v <= 10);
    render(rb, MD_LEVEL, 63, S_JUMP, 88, 0, O_MIX, 0);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "RAMP: the jump -> PATCH LEVEL +63 (from -12 dB): %d (<= 40; 575 before)", v);
    expect(line, v <= 40);
    render(rb, MD_DLY, 63, S_JUMP, 0, 1, O_WET, 0);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "RAMP: the jump -> DELAY send +63 (the wet bus): %d (<= 40; 385 before)", v);
    expect(line, v <= 40);
    render(rb, MD_CHO, 63, S_JUMP, 0, 1, O_WET, 0);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "RAMP: the jump -> CHORUS send +63 (the wet bus): %d (<= 40; 221 before)", v);
    expect(line, v <= 40);
    /* BACK: the route taken away while it is up */
    render(rb, MD_PAN, 63, S_BACK, 0, 0, O_L, 0);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "BACK: PAN's route switched off while up: the way back %d (<= 40)", v);
    expect(line, v <= 40);
    render(rb, MD_LEVEL, 63, S_BACK, 88, 0, O_MIX, 0);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "BACK: PATCH LEVEL's route switched off while up (+7.5 -> -12 dB in one block's line): %d (<= 80; 575 as a step)", v);
    expect(line, v <= 80);
    /* SAT's blend zone */
    render(rb, MD_DIST, 5, S_JUMP, 0, 0, O_MIX, 0);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "SAT: SAT 0, the jump -> SAT +5 (into the blend zone 0..12): %d (<= 40; 103 before)", v);
    expect(line, v <= 40);
    /* T0229 (owner, MATRIX_LIST_PREVIEW.md): the AMP ENV times as destinations (MD_AATK .. MD_AREL: P_ATK .. P_REL per block, voice.c esync_coef): the
     * envelope stays continuous, only its slope or its sustain target moves. Measured as the study did: a held sine, an 8 Hz LFO and the MODW jump,
     * against the same render without the route (the floor) */
    {
        int32_t fl, lf, jm;
        render(ra, MD_ASUS, 0, S_LFO8, 64, 0, O_MIX, 0);
        fl = d2max(ra, 100u, NB);
        render(rb, MD_ASUS, 63, S_LFO8, 64, 0, O_MIX, 0);
        lf = d2max(rb, 100u, NB);
        render(rb, MD_ASUS, 63, S_JUMP, 64, 0, O_MIX, 0);
        jm = d2max(rb, JB, JB + 60u);
        snprintf(line, sizeof line, "AMP ENV SUSTAIN: an 8 Hz LFO %d (floor %d, <= floor + 5), the jump %d (<= 40)", lf, fl, jm);
        expect(line, lf <= fl + 5 && jm <= 40);
        render(ra, MD_AATK, 0, S_LFO8, 90, 0, O_MIX, 0);
        fl = d2max(ra, 5u, 100u);
        render(rb, MD_AATK, 63, S_LFO8, 90, 0, O_MIX, 0);
        lf = d2max(rb, 5u, 100u);
        snprintf(line, sizeof line, "AMP ENV ATTACK: an 8 Hz LFO during a slow attack %d (floor %d, <= floor + 5)", lf, fl);
        expect(line, lf <= fl + 5);
        sus_ov = 40;
        render(ra, MD_ADEC, 0, S_LFO8, 90, 0, O_MIX, 0);
        fl = d2max(ra, 5u, NB);
        render(rb, MD_ADEC, 63, S_LFO8, 90, 0, O_MIX, 0);
        lf = d2max(rb, 5u, NB);
        sus_ov = -1;
        snprintf(line, sizeof line, "AMP ENV DECAY (SUSTAIN 40): an 8 Hz LFO %d (floor %d, <= floor + 5)", lf, fl);
        expect(line, lf <= fl + 5);
        {
            int an, fm;
            clean();
            host_preset(&trk[0], scn_eng("ANALOG"), 0);
            an = mod_dst_offered(&trk[0], MD_AATK) && mod_dst_offered(&trk[0], MD_AREL);
            host_preset(&trk[0], scn_eng("FM6"), 0);
            fm = !mod_dst_offered(&trk[0], MD_AATK) && !mod_dst_offered(&trk[0], MD_AREL);
            expect("AMP ENV times: the four ids are P_ATK .. P_REL, named AMP / ATTACK .. on the unit, offered on ANALOG, not on FM6 (OWN ENV)",
                   mod_param(MD_AATK) == P_ATK && mod_param(MD_AREL) == P_REL && str_eq(mod_dst_name(&trk[0], MD_ASUS), "AMP\nSUSTAIN") && an && fm);
        }
    }
    /* hunt1: the LFOs' own depths (VIB, CUT, SHP of LFO 1 and 2) move at most MX_SLEW a block like a matrix source: LFO 2 as a SAW (the reset every cycle) or a SQUARE on the VOICE
     * engine's SHAPE depth (BUZZ: the glottal pulse's shape; 127 = the whole range at the edge), ~8 Hz. Measured at the block boundaries: the largest 2nd difference there in
     * thousandths of the mean around it (the VOICE engine's own seams, with LFO 2 off: the floor) */
    {
        int32_t fl, c1, c2;
        l2_ov = 1;
        render(rb, MD_PITCH, 0, S_LFO8, 0, 0, O_MIX, 0);
        fl = seams(rb, 100u * CTL, NS);
        l2_ov = 2;
        render(rb, MD_PITCH, 0, S_LFO8, 0, 0, O_MIX, 0);
        c1 = seams(rb, 100u * CTL, NS);
        l2_ov = 3;
        render(rb, MD_PITCH, 0, S_LFO8, 0, 0, O_MIX, 0);
        c2 = seams(rb, 100u * CTL, NS);
        l2_ov = 0;
        snprintf(line, sizeof line, "LFO 2 on SHAPE (VOICE, ~8 Hz), the seam ratio: SAW %d, SQUARE %d (<= 30,000; 76,000 / 75,800 before), a sine LFO 2 %d", c1, c2, fl);
        expect(line, c1 <= 30000 && c2 <= 30000);
    }
    /* hunt1: SAT out of 0, the first blocks of the follower (integer 0, then 1) */
    render(rb, MD_DIST, 2, S_JUMP, 0, 0, O_MIX, 0);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "SAT 0: the jump -> SAT +2 (the follower leaves 0 through the integer 0): %d (<= 10; 17 before)", v);
    expect(line, v <= 10);
    mv_id = P_DIST, mv_a = 0, mv_b = 1;
    render(rb, MD_PITCH, 0, S_JUMP, 0, 0, O_MIX, 0);
    mv_id = -1;
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "SAT 0: the MORPH fader 0 -> 127 moves SAT 0 -> 1: %d (<= 10; 29 before)", v);
    expect(line, v <= 10);
    render(rb, MD_DIST, 3, S_BACK, 0, 0, O_MIX, 0);
    v = d2max(rb, JB, JB + 60u);
    snprintf(line, sizeof line, "SAT 0: the route to SAT +3 switched off while up: the follower goes back into 0 through the integer 0: %d (<= 10; 15 before)", v);
    expect(line, v <= 10);
    /* hunt1: SPREAD's mix (spread_run, spread_post): the jumps of the matrix and the fader, the LFO, the way back */
    {
        static const struct { const char *nm; int dst, amt, src, base, send, o, before; } SP[] = {
            {"PAN +63 (left channel)", MD_PAN, 63, S_JUMP, 0, 0, O_L, 436}, {"PATCH LEVEL +63 (from -12 dB)", MD_LEVEL, 63, S_JUMP, 88, 0, O_MIX, 524},
            {"DELAY send +63 (wet bus)", MD_DLY, 63, S_JUMP, 0, 1, O_WET, 2}, {"CHORUS send +63 (wet bus)", MD_CHO, 63, S_JUMP, 0, 1, O_WET, 105},
            {"SAT +5 (blend zone)", MD_DIST, 5, S_JUMP, 0, 0, O_MIX, 85}};
        uint32_t k;
        sprd_ov = 38;
        for (k = 0; k < NELEM(SP); k++) {
            render(rb, SP[k].dst, SP[k].amt, SP[k].src, SP[k].base, SP[k].send, SP[k].o, 0);
            v = d2max(rb, JB, JB + 60u);
            snprintf(line, sizeof line, "SPREAD 38: the jump -> %s: %d (<= 40; %d before)", SP[k].nm, v, SP[k].before);
            expect(line, v <= 40);
        }
        render(rb, MD_PAN, 63, S_LFO8, 0, 0, O_L, 0);
        v = d2max(rb, 100u, NB);
        snprintf(line, sizeof line, "SPREAD 38: an 8 Hz LFO -> PAN +63: %d (<= 10; 31 before)", v);
        expect(line, v <= 10);
        render(rb, MD_LEVEL, 63, S_LFO8, 88, 0, O_MIX, 0);
        v = d2max(rb, 100u, NB);
        snprintf(line, sizeof line, "SPREAD 38: an 8 Hz LFO -> PATCH LEVEL +63: %d (<= 10; 90 before)", v);
        expect(line, v <= 10);
        render(rb, MD_LEVEL, 63, S_BACK, 88, 0, O_MIX, 0);
        v = d2max(rb, JB, JB + 60u);
        snprintf(line, sizeof line, "SPREAD 38: PATCH LEVEL's route switched off while up: the way back %d (<= 80; 2042 before)", v);
        expect(line, v <= 80);
        sprd_ov = 0;
    }
    {
        static const struct { const char *nm; int id, a, b, o, before; } MS[] = {
            {"CHORUS send 0 -> 100", P_CHOR, 0, 100, O_SC, 241}, {"DELAY send 0 -> 100", P_DLY, 0, 100, O_SD, 241}, {"DELAY send 1 -> 2", P_DLY, 1, 2, O_SD, 119},
            {"SAT 0 -> 5", P_DIST, 0, 5, O_MIX, 48}, {"SAT 1 -> 2", P_DIST, 1, 2, O_MIX, 31}, {"PAN 0 -> 40", P_PAN, 0, 40, O_L, 35}};
        uint32_t k;
        sprd_ov = 38;
        for (k = 0; k < NELEM(MS); k++) {
            mv_id = MS[k].id, mv_a = MS[k].a, mv_b = MS[k].b;
            render(rb, MD_PITCH, 0, S_JUMP, 0, 0, MS[k].o, 0);
            mv_id = -1;
            v = d2max(rb, JB, JB + 60u);
            snprintf(line, sizeof line, "SPREAD 38: the MORPH fader 0 -> 127 at a block boundary moves %s: %d (<= 40; %d before)", MS[k].nm, v, MS[k].before);
            expect(line, v <= 40);
        }
        mv_id = P_DIST, mv_a = 0, mv_b = 1;
        render(rb, MD_PITCH, 0, S_JUMP, 0, 0, O_MIX, 0);
        mv_id = -1;
        v = d2max(rb, JB, JB + 60u);
        snprintf(line, sizeof line, "SPREAD 38: the MORPH fader 0 -> 127 moves SAT 0 -> 1: %d (<= 10; 65 before)", v);
        expect(line, v <= 10);
        sprd_ov = 0;
    }
    /* hunt1: the MORPH fader moves the sends by whole steps (a morph B end with CHORUS or DELAY at 100): the open morph window is a route for the mix ramp */
    {
        static const struct { const char *nm; int id, a, b, o; } MV[] = {
            {"CHORUS send 0 -> 100", P_CHOR, 0, 100, O_SC}, {"DELAY send 0 -> 100", P_DLY, 0, 100, O_SD}, {"DELAY send 1 -> 2", P_DLY, 1, 2, O_SD},
            {"SAT 0 -> 5", P_DIST, 0, 5, O_MIX}};
        uint32_t k;
        for (k = 0; k < NELEM(MV); k++) {
            mv_id = MV[k].id, mv_a = MV[k].a, mv_b = MV[k].b;
            render(rb, MD_PITCH, 0, S_JUMP, 0, 0, MV[k].o, 0);
            mv_id = -1;
            v = d2max(rb, JB, JB + 60u);
            snprintf(line, sizeof line, "MORPH: the fader 0 -> 127 at a block boundary moves %s: %d (<= 40)", MV[k].nm, v);
            expect(line, v <= 40);
        }
    }
    /* hunt1: spread_run's SAT pass works on the mid before mix_spread forms the side: SAT moves in its blend zone (the jump to +5), and the side (L - R) is the side of the render
     * without SAT to within 2 LSB (mix_spread takes 2 d - b from the mid after SAT: the pass gives d half of SAT's change in advance, one LSB off at most); the mid (L + R) is the
     * plain mix's, scaled by SPREAD's gains (3 LSB: the plain and the SPREAD mix round differently) */
    {
        int32_t md = 0, sd = 0, m0, m1;
        uint32_t i;
        sprd_ov = 38;
        render(ra, MD_DIST, 0, S_JUMP, 0, 0, O_SIDE, 0);
        render(rb, MD_DIST, 5, S_JUMP, 0, 0, O_SIDE, 0);
        for (i = JB * CTL; i < NS; i++) {
            int32_t e = rb[i] - ra[i];
            sd = (e < 0 ? -e : e) > sd ? (e < 0 ? -e : e) : sd;
        }
        render(ra, MD_DIST, 5, S_JUMP, 0, 0, O_MIX, 0);
        sprd_ov = 0;
        render(rb, MD_DIST, 5, S_JUMP, 0, 0, O_MIX, 0);
        m0 = 0;
        for (i = JB * CTL; i < NS; i++) {   /* PAN 0, SPREAD 38: the mid gain of both channels is (4096 + 2880) / 2 = 3488 */
            int32_t e = ra[i] - (int32_t)(((int64_t)rb[i] * 3488) >> 12);
            md = (e < 0 ? -e : e) > md ? (e < 0 ? -e : e) : md;
            m1 = rb[i] < 0 ? -rb[i] : rb[i];
            m0 = m1 > m0 ? m1 : m0;
        }
        snprintf(line, sizeof line, "SPREAD 38: SAT 0 -> +5 in the blend zone: the side differs from the side without SAT by %d LSB (<= 2), the mid from the plain mix's by %d of %d (<= 3)", sd, md, m0);
        expect(line, sd <= 2 && md <= 3);
    }
    /* hunt1: the ramp spread_post makes is the plain mix's: PATCH LEVEL jumping by the matrix (from -12 dB to +7.5) and the DELAY send going up, the mid (L + R) of the SPREAD note
     * is the plain mix's times SPREAD 38's mid gain 3488 / 4096 at every sample of the ramp (3 LSB: two different roundings), and the delay send (O_SD) is the plain mix's to 1 LSB */
    {
        int32_t md = 0, ms = 0;
        uint32_t i;
        sprd_ov = 38;
        render(ra, MD_LEVEL, 63, S_JUMP, 88, 0, O_MIX, 0);
        render(rb, MD_DLY, 63, S_JUMP, 0, 1, O_SD, 0);
        sprd_ov = 0;
        {
            static int32_t pa_[NS], pb_[NS];
            memcpy(pa_, ra, sizeof pa_);
            memcpy(pb_, rb, sizeof pb_);
            render(ra, MD_LEVEL, 63, S_JUMP, 88, 0, O_MIX, 0);
            render(rb, MD_DLY, 63, S_JUMP, 0, 1, O_SD, 0);
            for (i = JB * CTL - 64u; i < NS; i++) {
                int32_t e = pa_[i] - (int32_t)(((int64_t)ra[i] * 3488) >> 12), f = pb_[i] - rb[i];
                md = (e < 0 ? -e : e) > md ? (e < 0 ? -e : e) : md;
                ms = (f < 0 ? -f : f) > ms ? (f < 0 ? -f : f) : ms;
            }
        }
        snprintf(line, sizeof line, "SPREAD 38: the jump -> PATCH LEVEL +63 and -> DELAY send +63 are the plain mix's: the mid differs by %d LSB (<= 3), the send by %d (<= 1)", md, ms);
        expect(line, md <= 3 && ms <= 1);
    }
    /* hunt1: REST, SPREAD: a steady SPREAD note (sends 100, PATCH LEVEL, SAT in the blend zone / above it, PAN) with the MORPH's window closed and a route that moves nothing:
     * the hashes of the render with the code before spread_run / spread_post (git 4e7699a) */
    {
        static const struct { const char *nm; int dst, base, send; uint32_t h; } RS[] = {
            {"sends 100, SAT 3", MD_DIST, 3, 1, 0x513b8b35u}, {"sends 100, SAT 40", MD_DIST, 40, 1, 0x5e00824fu}, {"sends 100, PAN 20", MD_PAN, 20, 1, 0xa25d5585u},
            {"no sends, PATCH LEVEL 88", MD_LEVEL, 88, 0, 0xccbf108bu}};
        uint32_t k, i, h;
        sprd_ov = 38;
        for (k = 0; k < NELEM(RS); k++) {
            render(rb, RS[k].dst, 0, S_JUMP, RS[k].base, RS[k].send, O_MIX, 0);
            for (i = 0, h = 2166136261u; i < NS; i++)
                h = (h ^ (uint32_t)rb[i]) * 16777619u;
            snprintf(line, sizeof line, "REST: a steady SPREAD 38 note, %s: the render is bit for bit the one before spread_run (%08x)", RS[k].nm, (unsigned)RS[k].h);
            expect(line, h == RS[k].h);
        }
        sprd_ov = 0;
    }
    /* TWIN FILTER (cloud job E): the matrix's CUTOFF 2, RES 2, DRIVE and BLEND jumped by MODW 0 -> 127 with filter 2 on (the cutoff through the second list and the source slew, the others per block as RES) */
    {
        static const struct { int dst, amt, base, end; const char *nm; } TW[] = {
            {MD_CUT2, 63, -5, 58, "CUTOFF 2"}, {MD_RES2, 63, 0, 125, "RES 2"}, {MD_DRIVE, 63, 0, 63, "DRIVE"}, {MD_BLEND, 63, -64, -1, "BLEND"}};   /* (end: where the route takes the stored value) */
        static const uint8_t TYS[5] = {1, 2, 5, 6, 3};   /* LP BP LADDER COMB, then HP: a high-pass of a driven sine is a train of edges (a high 2nd difference of its own) and its k x band-pass term steps with RES: reported, not a gate. COMB's TUNE is a pitch: a jump of 83 semitones clicks by nature (reported) */
        uint32_t k, tyi, ty;
        for (tyi = 0; tyi < 5u; tyi++)
            for (ty = TYS[tyi], k = 0; k < sizeof TW / sizeof TW[0]; k++) {
                int32_t fl, fl2;
                tw_on = (int)ty;
                tw_cut = TW[k].dst == MD_CUT2 ? -5 : 20;      /* (the cutoff above the sine: a resonance at 220 Hz would clip the test's 16-bit capture, not click) */
                render(ra, TW[k].dst, 0, S_JUMP, TW[k].base, 0, O_MIX, 1);   /* the floor: the same render with the route at 0, at the value before the jump and at the one after (a high-pass leaves only the harmonics: a high 2nd difference of its own) */
                fl = d2max(ra, JB, JB + 60u);
                render(ra, TW[k].dst, 0, S_JUMP, TW[k].end, 0, O_MIX, 1);
                fl2 = d2max(ra, JB, JB + 60u);
                fl = fl2 > fl ? fl2 : fl;
                render(rb, TW[k].dst, TW[k].amt, S_JUMP, TW[k].base, 0, O_MIX, 1);
                v = d2max(rb, JB, JB + 60u);
                tw_on = 0;
                tw_cut = -5;
                {
                    int info = ty == 3u || (ty == 6u && TW[k].dst == MD_CUT2);
                    snprintf(line, sizeof line, "TWIN %s, TYPE %u%s: the jump -> %s +%d: %d (steady renders' %d; <= 60 or <= 1.6 x that + 10)", TW[k].nm, ty, info ? " (reported)" : "", TW[k].nm, TW[k].amt, v, fl);
                    expect(line, info || v <= 60 || v <= (fl * 8) / 5 + 10);
                }
            }
        tw_on = 1;
        render(rb, MD_DRIVE, 63, S_LFO8, 0, 0, O_MIX, 1);
        v = d2max(rb, 100u, NB);
        tw_on = 0;
        snprintf(line, sizeof line, "TWIN DRIVE: an 8 Hz LFO -> DRIVE +63 on a held sine: %d (<= 30)", v);
        expect(line, v <= 30);
    }
    /* cloud job F: the SUB LEVEL route (job B's destination 55) moves the sub's gain per sample (chain_sub.c glides it from the block before's), and CYCLE is a source like another */
    {
        int32_t fl, w, sq, tr;
        sub_wv = 0;
        render(ra, MD_SUBLVL, 0, S_LFO8, 0, 0, O_MIX, 0);
        render(rb, MD_SUBLVL, 63, S_JUMP, 0, 0, O_MIX, 0);
        v = d2max(rb, JB, JB + 60u);
        snprintf(line, sizeof line, "SUB LEVEL: the jump MODW 0 -> 127 -> SUB LEVEL +63 (a SINE sub under the sine, from 0): %d (<= 40; the gain glides per sample)", v);
        expect(line, v <= 40 && !same(ra, rb));
        render(rb, MD_SUBLVL, 63, S_LFO8, 40, 0, O_MIX, 0);
        w = d2max(rb, 100u, NB);
        sub_wv = -1;
        render(ra, MD_SUBLVL, 0, S_LFO8, 40, 0, O_MIX, 0);
        fl = d2max(ra, 100u, NB);
        sub_wv = 0;
        snprintf(line, sizeof line, "SUB LEVEL: an 8 Hz LFO around LEVEL 40, AMT 63: %d (the sine alone %d: <= 2 x + 10)", w, fl);
        expect(line, w <= 2 * fl + 10);
        sub_wv = 2;                                     /* a SQUARE sub: its edges are steep by nature, the route may not make them steeper (against the same sub held) */
        render(ra, MD_SUBLVL, 0, S_LFO8, 90, 0, O_MIX, 0);
        sq = d2max(ra, 100u, NB);
        render(rb, MD_SUBLVL, 63, S_LFO8, 40, 0, O_MIX, 0);
        tr = d2max(rb, 100u, NB);
        snprintf(line, sizeof line, "SUB LEVEL: the same LFO on a SQUARE sub: %d (the held SQUARE at LEVEL 90: %d; <= 1.25 x + 10)", tr, sq);
        expect(line, tr <= sq * 5 / 4 + 10);
        sub_wv = -1;
        render(rb, MD_PAN, 63, S_CYC, 0, 0, O_L, 0);
        v = d2max(rb, 20u, NB);
        snprintf(line, sizeof line, "CYCLE -> PAN +63 (LOOP, a rise of 2 blocks, a fall of 53): %d (<= 40: the mix ramp makes the per-block value a line)", v);
        expect(line, v <= 40);
    }
    /* REST: a route that moves none of the six leaves the plain mix alone (mod_mixmv stays 0: the old loop, bit for bit; pan_test's and mrender's hashes) */
    render(rb, MD_PITCH, 63, S_LFO8, 0, 1, O_MIX, 0);
    expect("REST: a route to PITCH (not PAN, PATCH LEVEL or a send): the mix ramp is never asked for (mod_mixmv 0)", !mod_mixmv[0] && !mod_mixmv[1] && !mx_was[0] && !mx_was[1]);
    {   /* PHASE (hunt4): the seam of the DCW sweep */
        double r1 = phase_seam(88, 28, 7, 60), r0 = phase_seam(0, 28, 7, 60), r3 = phase_seam(88, 28, 7, 48), r5 = phase_seam(88, 28, 0, 72);
        snprintf(line, sizeof line, "PHASE: BRAZEN's attack (SAW, DCW 28, ENV 88) at C4: |2nd difference| at the block seams / elsewhere %.2f (<= 2.5; 3.9 before), C3 %.2f (11.7 before), C5 %.2f (1.7 before), ENV 0: %.2f", r1, r3, r5, r0);
        expect(line, r1 <= 2.5 && r3 <= 2.5 && r5 <= 2.5 && r0 <= 2.5);
    }
    printf("mxstep: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
