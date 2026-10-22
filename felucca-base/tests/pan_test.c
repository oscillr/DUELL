/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.24: PAN without a zipper (fx.c pan_follow, mix_part, mix_spread). The owner heard a crackle turning a part's PAN from the editor: the
 * pan gains were read once a block and jumped. Now the stored pan is followed (Q8, 5/16 of what is left a block, snapped under 1/16 of a step)
 * and the plain mix ramps its gains over the block.
 *   STEP      a sustained ANALOG chord, PAN 0 -> +40 at block 60: the gain (this render over the same render at PAN 0, where it is loud) moves by no more
 *             than 1 % from one sample to the next (the un-smoothed step jumped 62 %); the same for -30 -> 0 and 63 -> -64. The SPREAD mix (hot code: no ramp
 *             in it) steps once a block, 1/8 of what is left (pan_follow): it moves and settles, no sample metric (its channels are no one gain each)
 *   SETTLES   60 ms after the step the render is bit for bit the render that had PAN +40 all along (the pan lands exactly on the value)
 *   REST      a render whose pan never moves is bit for bit mode_scn.h's STEREO scenario hash (which modulates PAN from the matrix): the matrix's
 *             per-block steps are as they were; a silent part takes a new pan at once (its first sounding block is at the pan: no glide from centre)
 * Built and run by DUELL/host_tests.sh (name: pan) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"

#define MODE_STEREO_HASH 0x9C13C6ADFCF3BC87ull   /* 0.2.32: re-based on purpose (the direct depth cards reach 100 % at +63, the merged line); was 0xC04494F53D46F0F0 */ /* older: */   /* 0.2.30: merged line (cloud wave 6 + click fixes), re-based on purpose, was 0x1CC6C8241C84C0DA; before:  0.2.29: re-based on purpose, the scenario runs the HALL and its quiet lines now round to the nearest (fx.c hall_end; 0.2.21..0.2.28: 0xC970174114DFD9BA) */        /* mode_test.c's (0.2.21) */
static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("pan: FAIL %s\n", what);
        bad++;
    }
    return ok;
}
#define NB 160u                                         /* blocks: 232 ms */
#define NS (NB * CTL)
static int32_t ra[2][NS], rb[2][NS], rz[2][NS];
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
/* a sustained chord on part 1 (ANALOG, POLY, no sends), its PAN p0, then p1 from block `at`; spread: SPREAD 100 (the spread mix) */
static void render(int32_t (*out)[NS], int32_t p0, int32_t p1, uint32_t at, int spread)
{
    static const uint8_t C1[4] = {48, 55, 60, 64};
    static int32_t o[2 * CTL];
    uint32_t b, i;
    clean();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SPRD] = (int16_t)(spread ? 100 : 0);
    trk[0].p[P_PAN] = (int16_t)p0;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_TRIM] = 120;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_ATK] = 0;
    for (b = 0; b < NB; b++) {
        if (b == at)
            trk[0].p[P_PAN] = (int16_t)p1;
        if (b == 4u)
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], C1[i], 120);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {                     /* the mix before the master (its limiter and DC blocker remember the history: they would differ for long) */
            out[0][b * CTL + i] = mix_l[i];
            out[1][b * CTL + i] = mix_r[i];
        }
    }
}
static int32_t peak_of(int32_t (*r)[NS])
{
    int32_t m = 0;
    uint32_t i, k;
    for (k = 0; k < 2u; k++)
        for (i = 0; i < NS; i++) {
            int32_t a = r[k][i] < 0 ? -r[k][i] : r[k][i];
            m = a > m ? a : m;
        }
    return m;
}
/* the largest jump of the GAIN from one sample to the next, in 1/1000 of unity: g[i] = b[i] / a[i] where a is loud (|a| > 1/4 of the peak: a quiet sample
 * says nothing); either channel. blocks: across a block boundary only (the spread mix steps there) */
static int32_t jump_of(int32_t (*a)[NS], int32_t (*b)[NS], int blocks)
{
    int32_t m = 0, pk = peak_of(a) / 4;
    uint32_t i, k;
    for (k = 0; k < 2u; k++) {
        double gp = -1.0;
        for (i = 0; i < NS; i++) {
            int32_t x = a[k][i] < 0 ? -a[k][i] : a[k][i];
            double g;
            if (x < pk) {
                gp = -1.0;
                continue;
            }
            g = (double)b[k][i] / (double)a[k][i];
            if (gp >= 0.0 && (!blocks || !(i % CTL))) {
                int32_t d = (int32_t)(fabs(g - gp) * 1000.0 + 0.5);
                m = d > m ? d : m;
            }
            gp = g;
        }
    }
    return m;
}
static int same_from(int32_t (*a)[NS], int32_t (*b)[NS], uint32_t from)
{
    uint32_t i, k;
    for (k = 0; k < 2u; k++)
        for (i = from; i < NS; i++)
            if (a[k][i] != b[k][i])
                return 0;
    return 1;
}
static int differ_in(int32_t (*a)[NS], int32_t (*b)[NS], uint32_t b0, uint32_t b1)
{
    uint32_t i, k;
    for (k = 0; k < 2u; k++)
        for (i = b0 * CTL; i < b1 * CTL; i++)
            if (a[k][i] != b[k][i])
                return 1;
    return 0;
}
static void step_case(const char *name, int32_t p0, int32_t p1, int spread)
{
    char what[160];
    int32_t pk, j, jb;
    uint32_t settle = spread ? 130u : 100u;             /* blocks: the plain mix lands within ~26 ms, the spread mix (1/8 a block) within ~70 ms */
    render(ra, p0, p0, 60u, spread);
    render(rb, p0, p1, 60u, spread);
    render(rz, p1, p1, 60u, spread);
    pk = peak_of(ra);
    j = jump_of(ra, rb, 0);
    jb = jump_of(ra, rb, 1);
    if (!spread) {                                      /* (the spread mix is no one gain a channel: L = x glm + side gld; its steps are pan_follow's 1/8 of what is left a block, checked by the settle time) */
        snprintf(what, sizeof what, "%s: PAN %d -> %d at block 60: the largest gain jump from one sample to the next is %d/1000 (the un-smoothed step: %d/1000)", name, p0, p1, j,
                 (p1 > p0 ? p1 - p0 : p0 - p1) * 1000 / 64);
        expect(what, j <= 10);
    }
    snprintf(what, sizeof what, "%s: the pan moves (blocks 60..63 differ from both the start pan and the end pan)", name);
    expect(what, differ_in(rb, ra, 60u, 64u) && differ_in(rb, rz, 60u, 64u));
    snprintf(what, sizeof what, "%s: settled: from block %u on bit for bit the render at PAN %d all along", name, settle, p1);
    expect(what, same_from(rb, rz, settle * CTL));
    printf("pan: %s %d -> %d%s: peak %d, largest gain jump %d/1000 a sample, %d/1000 at block boundaries\n", name, p0, p1, spread ? " (spread)" : "", pk, j, jb);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* REST first (the scenario must be the process's first render): the matrix-modulated STEREO scenario bit for bit as before (the stored pan never moves there; the parts start sounding at their pans) */
        uint64_t h = mode_stereo_scenario();
        expect("REST: mode_scn.h's STEREO scenario (LFO -> PAN from the matrix, PAN -30 and +40 from the start) is bit for bit the 0.2.21 hash", h == MODE_STEREO_HASH);
        printf("pan: STEREO scenario hash %016llX (%s)\n", (unsigned long long)h, h == MODE_STEREO_HASH ? "as before" : "CHANGED");
    }
    step_case("plain", 0, 40, 0);
    step_case("plain", -30, 0, 0);
    step_case("plain", 63, -64, 0);
    step_case("spread", 0, 40, 1);
    step_case("spread", -30, 0, 1);
    {   /* a silent part takes a new pan at once: its first sounding block is at the pan (no glide from the centre: the render at +40 from the start equals one where +40 was set a block before the notes) */
        render(ra, 40, 40, 60u, 0);
        render(rb, 0, 40, 2u, 0);
        expect("a pan set while the part is silent: the notes start at it, bit for bit", same_from(ra, rb, 0u));
    }
    printf("pan: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
