/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.25 layout round (owner): the part's EQ has NO ON / OFF switch any more (0.2.24 / 0.2.25 had one on the retired DUCK NOTE id 106, core.h P_DKNOTE):
 *   STORE     the id is a free id again ("-", 0 .. 0): a stored value reads 0 (a test record of 0.2.24 with EQ OFF plays its EQ: disposable before the alpha)
 *   SOUND     every band flat: nothing of the EQ runs (eq_live clear, EQ_ON false); bands set: the EQ works; a 1 in the free id changes nothing (bit for bit)
 *   PAGES     EQ 1/2: K1 LOW FREQ, K2 LOW GAIN, K3 MID FREQ, K4 MID GAIN; EQ 2/2: K1 MID Q, K2 HIGH FREQ, K3 HIGH GAIN, K4 none; every band card drawn (no hide rule), the labels
 *             of two lines (LOW / FREQ ...: the value on the third line), the panel the EQ curve (GR_EQ); DUCK 1/2's K2 is no card for MIDI / OFF
 *   PICTURE   ui_graph.c graph_eq's curve (the audio's own eq_design coefficients, |H|^2 in the phi = sin^2(w / 2) form) against the exact response of the same
 *             coefficients (double, z = e^jw) at every point of the axis, every gain, frequency step and Q: within 0.25 dB
 * Built and run by DUELL/host_tests.sh (name: eqon) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"
#include <math.h>

static int bad, ncheck;
static int card_is(uint32_t c, const char *s) { return !strncmp(ui.col[c], s, strlen(s)); }   /* the card c as drawn last (ui_draw.c draw_column's cache key) */
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok)
        printf("eqon: FAIL %s\n", what), bad++;
    return ok;
}
#define NB 120u
#define NS (NB * CTL)
static int32_t ra[2][NS], rb[2][NS];
static void clean(void)
{
    memset(trk, 0, sizeof trk);
    memset(&trk[0].mdw, 0, sizeof trk[0].mdw);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(ins, 0, sizeof ins);
    memset(eqs, 0, sizeof eqs);
    trk[0].eqlv = trk[1].eqlv = 0;
    memset(pan_x, 0, sizeof pan_x);
    memset(pan_gv, 0, sizeof pan_gv);
    mix_bal = 0;
    vage = 0;
    mod_seed = 0x2545F491u;
    duell_mode_force(DM_SPLIT);
    duell_mode_force(DM_STEREO);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    host_tracks_init();
    song.g[G_RTYPE] = 0;
}
/* a held ANALOG chord with the EQ bands set (LOW +6, MID -6, HIGH +4); id106: what the free id holds; flat: every band 0 */
static void render(int32_t (*out)[NS], int id106, int flat)
{
    static const uint8_t C1[4] = {48, 55, 60, 64};
    static int32_t o[2 * CTL];
    uint32_t b, i;
    clean();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_EQLG] = (int16_t)(flat ? 0 : 6);
    trk[0].p[P_EQMG] = (int16_t)(flat ? 0 : -6);
    trk[0].p[P_EQHG] = (int16_t)(flat ? 0 : 4);
    trk[0].p[P_EQLF] = 4; trk[0].p[P_EQMF] = -3; trk[0].p[P_EQMQ] = 2; trk[0].p[P_EQHF] = -2;
    trk[0].p[P_CUT2] = (int16_t)param_fit(&TP[P_CUT2], id106);   /* (as every load path puts it: a record, a project, the editor's SET clamp to the id's range; id 106 is the TWIN FILTER's CUTOFF 2 now: the EQ must not read it) */
    for (b = 0; b < NB; b++) {
        if (b == 4u)
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], C1[i], 120);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            out[0][b * CTL + i] = mix_l[i];
            out[1][b * CTL + i] = mix_r[i];
        }
    }
}
static int same(int32_t (*a)[NS], int32_t (*b)[NS], uint32_t from)
{
    uint32_t i, k;
    for (k = 0; k < 2u; k++)
        for (i = from; i < NS; i++)
            if (a[k][i] != b[k][i])
                return 0;
    return 1;
}
static int32_t biggest_step(int32_t (*a)[NS], uint32_t b0, uint32_t b1)
{
    int32_t m = 0;
    uint32_t i, k;
    for (k = 0; k < 2u; k++)
        for (i = b0 * CTL + 1; i < b1 * CTL; i++) {
            int32_t d = a[k][i] - a[k][i - 1];
            d = d < 0 ? -d : d;
            m = d > m ? d : m;
        }
    return m;
}

int main(void)
{
    uint32_t i, ok;
    setvbuf(stdout, NULL, _IONBF, 0);
    /* the store */
    expect("id 106 (the retired DUCK NOTE, 0.2.24's EQ switch) is the TWIN FILTER's CUTOFF 2 now: -63 .. 82, default 0, a stored 1 reads 1; the EQ's old switch name reads the empty entry 127",
           P_CUT2 == 106 && str_eq(TP[P_CUT2].label, "CUTOFF") && TP[P_CUT2].min == -63 && TP[P_CUT2].max == 82 && param_fit(&TP[P_CUT2], 1) == 1 && up_def(0, P_CUT2) == 0 && P_DKNOTE == P_COUNT);
    {   /* the pages */
        uint32_t e1 = NPAGES, e2 = NPAGES;
        for (i = 0; i < NPAGES; i++) {
            if (str_eq(PAGES[i].title, "EQ 1/2")) e1 = i;
            if (str_eq(PAGES[i].title, "EQ 2/2")) e2 = i;
        }
        expect("EQ 1/2: K1 LOW FREQ, K2 LOW GAIN, K3 MID FREQ, K4 MID GAIN; EQ 2/2: K1 MID Q, K2 HIGH FREQ, K3 HIGH GAIN, K4 none; both draw the EQ curve",
               e1 < NPAGES && e2 < NPAGES && PAGES[e1].id[0] == P_EQLF && PAGES[e1].id[1] == P_EQLG && PAGES[e1].id[2] == P_EQMF && PAGES[e1].id[3] == P_EQMG &&
               PAGES[e2].id[0] == P_EQMQ && PAGES[e2].id[1] == P_EQHF && PAGES[e2].id[2] == P_EQHG && PAGES[e2].id[3] == 0xFFu && PAGES[e1].graph == GR_EQ && PAGES[e2].graph == GR_EQ);
        ui_power_on();
        duell_mode_req = DM_STEREO;
        init_engine_of(TSEL, 0u);
        go_title("EQ 1/2");
        frames(48);
        ok = card_is(0, "LOW\nFREQ|") && card_is(1, "LOW\nGAIN|") && card_is(2, "MID\nFREQ|") && card_is(3, "MID\nGAIN|");
        expect("EQ 1/2's cards: LOW / FREQ, LOW / GAIN, MID / FREQ, MID / GAIN (two lines each)", ok);
        TSEL->p[P_CUT2] = 1;
        frames(48);
        ok = !card_hidden_vp(&TSEL->p[P_EQLG]) && !card_hidden_vp(&TSEL->p[P_EQHF]) && !str_eq(ui.col[1], "\x7f");
        expect("  .. no hide rule: a 1 in id 106 (CUTOFF 2) hides nothing", ok);
        TSEL->p[P_CUT2] = 0;
        turn(EN_K2, 3);
        expect("  .. K2 turns LOW GAIN (+3)", TSEL->p[P_EQLG] == 3);
        go_title("EQ 2/2");
        frames(48);
        expect("EQ 2/2's cards: MID Q, HIGH / FREQ, HIGH / GAIN, none", card_is(0, "MID Q|") && card_is(1, "HIGH\nFREQ|") && card_is(2, "HIGH\nGAIN|"));
        TSEL->p[P_DKSRC] = 2;
        go_title("DUCK 1/2");
        frames(48);
        expect("DUCK 1/2 with SRC MIDI: no RATE / THRESH card, DEPTH and SETUP move left (T0227), K4 empty", card_is(1, "DEPTH|") && card_is(2, "SETUP|") && str_eq(ui.col[3], "\x7f") && duck_ctx_id(TSEL) == 0xFFu);
        TSEL->p[P_DKSRC] = 3;
        frames(48);
        expect("  .. SRC OFF: the same", str_eq(ui.col[1], "\x7f"));
    }
    /* the sound */
    render(ra, 0, 1);
    expect("every band flat: nothing of the EQ runs (eq_live clear, EQ_ON false)", !trk[0].eqlv && !EQ_ON(&trk[0]));
    render(rb, 0, 0);
    expect("bands set: a different sound (the EQ works)", !same(ra, rb, 0) && EQ_ON(&trk[0]));
    render(ra, 1, 0);
    expect("  .. a 1 in id 106 (a 0.2.24 record's EQ OFF, CUTOFF 2 +1 now): kept, and bit for bit the same sound (the EQ follows its gains only)", same(ra, rb, 0) && trk[0].p[P_CUT2] == 1);
    {   /* the picture: graph_eq's arithmetic against the exact response of the same coefficients */
        int bd, g, fr, q;
        double worst = 0;
        for (q = -12; q <= 12; q += 4)
            for (bd = 0; bd < 3; bd++)
                for (g = -12; g <= 12; g++)
                    for (fr = -24; fr <= (bd == 2 ? 12 : 24); fr += 3) {
                        eqc_t c;
                        gq_t n, d;
                        uint32_t f = GF_F0, k;
                        if (!g || (bd != 1 && q))
                            continue;
                        eq_design(&c, (uint32_t)bd, g, fr, bd == 1 ? q : 0);
                        eq_quad(&n, &d, &c);
                        for (k = 0; k < GF_N; k++, f = (uint32_t)(((uint64_t)f * GF_STEP) >> 16)) {
                            double w = 2 * M_PI * (f / 256.0) / FS, b0 = c.b0 / 268435456.0, b1 = c.b1 / 268435456.0, b2 = c.b2 / 268435456.0, a1 = c.a1 / 268435456.0, a2 = c.a2 / 268435456.0;
                            double re = b0 + b1 * cos(w) + b2 * cos(2 * w), im = -(b1 * sin(w) + b2 * sin(2 * w)), dr = 1 + a1 * cos(w) + a2 * cos(2 * w), di = -(a1 * sin(w) + a2 * sin(2 * w));
                            double ex = 10 * log10((re * re + im * im) / (dr * dr + di * di));
                            int64_t ph = gr_phi30(f);
                            double got = gr_db10(gr_lg64(gq_at(&n, ph)), gr_lg64(gq_at(&d, ph))) / 10.0;
                            worst = fabs(got - ex) > worst ? fabs(got - ex) : worst;
                        }
                    }
        printf("eqon: the EQ picture against the exact response: worst %.2f dB\n", worst);
        expect("the EQ picture (graph_eq) is the response of the audio's own coefficients within 0.25 dB at every point, gain, frequency and Q", worst <= 0.25);
    }
    printf("eqon: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
