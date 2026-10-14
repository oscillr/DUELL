/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL step 3.6 (SPEC 1.2, the EDIT row): EDIT's first sub-page is the ENGINE page of the selected part. Engine
 * selection is never hidden behind a hold.
 *   EDIT         opens on ENGINE, for every engine (a first visit); tapped again: the engine's first parameter page; the
 *                pages go round in PAGES order (ENGINE first) and back to ENGINE; SELECT turns the same round
 *   KNOB 1       ENGINE: the next / previous engine in PRESETS order (engines.c ENGINE_ORDER), only the ones shown
 *                (NENG_SHOWN: never DRUM, never hidden DIGITAL); wraps at both ends
 *   KNOB 2       SOUND: the next / previous sound of the engine (wraps); the engine stays
 *   KNOB 3       FAV: right marks, left unmarks the sound as a favourite
 *   KNOB 4       INIT: a turn right asks (the dialog INITIALIZE SOUND?), OCT+ does it, OCT- leaves it; left nothing
 *   ALGORITHM    the other part keeps its own engine, the page stays ENGINE and shows that part's engine
 * Built and run by DUELL/host_tests.sh (name: engine): the UI sources against ui_test.c's stub display and knobs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static int ncheck;

static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("engine: FAIL %s\n", what);
        bad++;
    }
    return ok;
}
static int on_engine_page(void) { return !ui.home && str_eq(cur_page()->title, "ENGINE"); }
static void fresh(uint32_t e)                       /* power on, part A on engine e, HOME */
{
    ui_power_on();
    set_engine_of(TSEL, e);
    TSEL->engine = TSEL->eng_req;
    memset(ui.fam_last, 0, sizeof ui.fam_last);
    frames(320);
    go_home(); frame();
}
static void tap_edit(void) { press(B_EDIT); frames(320); }   /* (taps further apart than a double tap: #83 locks) */
static uint32_t snd(void) { return TSEL->preset | (uint32_t)TSEL->user << 8 | (uint32_t)TSEL->eng_req << 16; }   /* (DUELL: the PRESETS list runs across the engines, by category: the engine is part of "the sound") */

/* the visible EDIT family in table order */
static uint32_t edit_pages(const char **t, uint32_t max)
{
    uint32_t i, n = 0;
    for (i = 0; i < NPAGES && n < max; i++)
        if (PAGES[i].fam == FAM_EDIT && page_visible(i))
            t[n++] = PAGES[i].title;
    return n;
}

static void test_open_and_order(void)
{
    uint32_t r, e;
    for (r = 0; r < NENG_SHOWN; r++) {
        const char *want[16];
        uint32_t n, k, ok = 1;
        char w[64];
        e = eng_vis(r);
        fresh(e);
        snprintf(w, sizeof w, "%s: EDIT opens on ENGINE", ENGINES[e]->name);
        tap_edit();
        expect(w, on_engine_page() && TSEL->eng_req == e);
        n = edit_pages(want, 16);
        expect("ENGINE is the family's first page", n > 1u && str_eq(want[0], "ENGINE"));
        for (k = 1; k <= n; k++) {                    /* the taps go round: each page in table order, then ENGINE again */
            tap_edit();
            ok &= str_eq(cur_page()->title, want[k % n]);
        }
        snprintf(w, sizeof w, "%s: EDIT again goes through %u pages in order and back to ENGINE", ENGINES[e]->name, n);
        expect(w, ok && on_engine_page());
        tap_edit();
        snprintf(w, sizeof w, "%s: EDIT again after ENGINE is the engine's first parameter page", ENGINES[e]->name);
        expect(w, str_eq(cur_page()->title, want[1]) && PAGES[ui.page].scope == SC_ENGINE);
        turn(EN_SELECT, -1);
        snprintf(w, sizeof w, "%s: SELECT left from the first parameter page is ENGINE", ENGINES[e]->name);
        expect(w, on_engine_page());
        turn(EN_SELECT, 1);
        expect("SELECT right from ENGINE: the engine's first parameter page", str_eq(cur_page()->title, want[1]));
        turn(EN_SELECT, -1);
        turn(EN_SELECT, -1);
        snprintf(w, sizeof w, "%s: SELECT left from ENGINE: the last page", ENGINES[e]->name);
        expect(w, str_eq(cur_page()->title, want[n - 1u]));
        turn(EN_SELECT, 1);
        expect("SELECT right: ENGINE again", on_engine_page());
    }
    {   /* the order spelled out for two engines (ANALOG: FILTER; FM6: OPERATOR pages) */
        static const char *const A[] = {"ENGINE", "EDIT 1", "EDIT 2", "FILTER", "VCF", "VOICE 1/2", "VOICE 2/2"};
        static const char *const F[] = {"ENGINE", "EDIT 1", "EDIT 2", "FILTER", "OP TUNE", "OP ENV", "OP MODE", "VOICE 1/2", "VOICE 2/2"};
        const char *got[16];
        uint32_t n, k, ok = 1;
        fresh(0);
        n = edit_pages(got, 16);
        for (k = 0; k < n && k < NELEM(A); k++) ok &= str_eq(got[k], A[k]);
        expect("ANALOG: ENGINE EDIT 1 EDIT 2 FILTER (multimode) VCF (its own) VOICE 1/2 VOICE 2/2", ok && n == NELEM(A));
        fresh(ENGI_FM6);
        n = edit_pages(got, 16);
        for (k = 0, ok = 1; k < n && k < NELEM(F); k++) ok &= str_eq(got[k], F[k]);
        expect("FM6: ENGINE EDIT 1 EDIT 2 OPERATOR OP ENV OPERATOR 2 VOICE 1/2 VOICE 2/2", ok && n == NELEM(F));
    }
    /* the engine changes on the ENGINE page: it stays there; on a parameter page that the new engine lacks: ENGINE */
    fresh(0);
    tap_edit(); tap_edit(); tap_edit(); tap_edit(); tap_edit();   /* ENGINE, EDIT 1, EDIT 2, FILTER (multimode), VCF (ANALOG's own) */
    expect("on VCF", str_eq(cur_page()->title, "VCF"));
    set_engine_of(TSEL, ENGI_FM6);
    frames(64);
    expect("VCF of a part that is no longer ANALOG: the family's first page, ENGINE", on_engine_page());
}

static void test_knob1(void)
{
    uint32_t r, k, i;
    int ok, hidden;
    for (r = 0; r < NENG_SHOWN; r++) {              /* from every engine: one step right / left is the neighbour in the order */
        fresh(eng_vis(r));
        tap_edit();
        turn(EN_K1, 1);
        expect("KNOB 1 right: the next engine of the order",
               TSEL->eng_req == eng_vis((r + 1u) % NENG_SHOWN) && on_engine_page());
        turn(EN_K1, -1);
        expect("KNOB 1 left: back", TSEL->eng_req == eng_vis(r));
        turn(EN_K1, -1);
        expect("KNOB 1 left: the previous engine of the order (wraps below the first)",
               TSEL->eng_req == eng_vis((r + NENG_SHOWN - 1u) % NENG_SHOWN));
    }
    fresh(eng_vis(0));
    tap_edit();
    for (i = 0, ok = 1, hidden = 0; i < NENG_SHOWN * 2u + 1u; i++) {   /* right round twice: every visible engine in order, wrapping */
        ok &= TSEL->eng_req == eng_vis(i % NENG_SHOWN);
        hidden |= TSEL->eng_req == ENGI_DRUM || !eng_ok(TSEL->eng_req);
        turn(EN_K1, 1);
    }
    expect("KNOB 1 right x (2 rounds): every shown engine in PRESETS order, wrapping from the last to the first", ok);
    expect("KNOB 1 right: never DRUM, never an engine the order hides", !hidden);
    for (i = 0, ok = 1, hidden = 0; i < NENG_SHOWN * 2u + 1u; i++) {   /* and left round twice, the order backwards */
        ok &= TSEL->eng_req == eng_vis((NENG_SHOWN * 3u + 1u - i) % NENG_SHOWN);
        turn(EN_K1, -1);
        hidden |= TSEL->eng_req == ENGI_DRUM || !eng_ok(TSEL->eng_req);
    }
    expect("KNOB 1 left x (2 rounds): the order backwards, wrapping from the first to the last", ok);
    expect("KNOB 1 left: never DRUM", !hidden);
    for (k = 0, ok = 1; k < NENG_SHOWN; k++)
        ok &= eng_vis(k) != ENGI_DRUM;
    expect("DRUM is not in the order at all", ok);
    /* the engine page of the engine KNOB 1 reached: its sound is that engine's first, the engine's page list follows it */
    fresh(0);
    tap_edit();
    turn(EN_K1, 1);
    expect("KNOB 1: the new engine plays its first sound", TSEL->eng_req == eng_vis(1) && user_of(TSEL) >= UP_SLOTS);
    turn(EN_K1, 1);                                  /* (the click-free swap is set_engine's own, select_engine; the audio fades) */
    expect("KNOB 1: eng_req moves at once, the voices follow through set_engine's fade", TSEL->eng_req == eng_vis(2));
}

static void test_knob2(void)
{
    uint32_t r;
    for (r = 0; r < NENG_SHOWN; r++) {
        uint32_t e = eng_vis(r), total, i, s0;
        int ok = 1;
        char w[64];
        fresh(e);
        tap_edit();
        eng_list_pos(&total);
        s0 = snd();
        for (i = 0; i < total; i++) {
            uint32_t before = snd();
            turn(EN_K2, 1);
            ok &= TSEL->eng_req == e && (total < 2u || snd() != before);
        }
        snprintf(w, sizeof w, "%s: KNOB 2 right x %u: the engine stays, every step is another sound", ENGINES[e]->name, total);
        expect(w, ok);
        snprintf(w, sizeof w, "%s: KNOB 2 right x all sounds: round to the first (wraps)", ENGINES[e]->name);
        expect(w, snd() == s0);
        if (total > 1u) {
            turn(EN_K2, -1);
            expect("KNOB 2 left: the previous sound (wraps below the first), the engine stays",
                   TSEL->eng_req == e && snd() != s0);
            turn(EN_K2, 1);
            expect("KNOB 2 right: back", snd() == s0 && on_engine_page());
        }
    }
}

static void test_knob3(void)
{
    uint32_t r;
    for (r = 0; r < NENG_SHOWN; r++) {
        uint32_t e = eng_vis(r);
        char w[64];
        fresh(e);
        tap_edit();
        turn(EN_K3, -1);
        snprintf(w, sizeof w, "%s: KNOB 3 left: not a favourite", ENGINES[e]->name);
        expect(w, !preset_favorite());
        turn(EN_K3, 1);
        snprintf(w, sizeof w, "%s: KNOB 3 right: FAV on", ENGINES[e]->name);
        expect(w, preset_favorite() && TSEL->eng_req == e);
        turn(EN_K3, 1);
        expect("KNOB 3 right again: stays on", preset_favorite());
        turn(EN_K3, -1);
        snprintf(w, sizeof w, "%s: KNOB 3 left: FAV off", ENGINES[e]->name);
        expect(w, !preset_favorite());
    }
    fresh(0);
    tap_edit();
    turn(EN_K3, 1);
    turn(EN_K2, 1);
    expect("FAV marks the sound shown, not the next one", !preset_favorite());
    turn(EN_K2, -1);
    expect("... and it is still marked", preset_favorite());
}

static void test_knob4(void)
{
    fresh(0);
    tap_edit();
    TSEL->p[P_E0] = 77;
    turn(EN_K4, -1);
    expect("KNOB 4 left: nothing", !ui.confirm && TSEL->p[P_E0] == 77);
    turn(EN_K4, 1);
    expect("KNOB 4 right: the dialog INITIALIZE SOUND?", ui.confirm == CF_INIT_SOUND && TSEL->p[P_E0] == 77);
    press(B_OCTDN);
    expect("OCT- leaves the dialog, nothing initialised", !ui.confirm && TSEL->p[P_E0] == 77 && on_engine_page());
    turn(EN_K4, 1);
    press(B_OCTUP);
    expect("OCT+ does it: the engine's defaults, the engine kept", !ui.confirm && TSEL->p[P_E0] != 77 && TSEL->eng_req == eng_vis(0));
    expect("... the page stays ENGINE", on_engine_page());
    /* (the song chain and its 'STOP TO EDIT' lock were removed with the song mode) */
    /* the hold-EDIT layer (0.2.9: QUICK FILTER, K1..K4 CUT RES MODE REL; the engine is the ENGINE page's alone) */
    fresh(0);
    {
        const page_t *qp;
        int16_t c0;
        uint32_t e0 = TSEL->eng_req;
        btn_down(B_EDIT); frames(900);
        expect("EDIT held: the layer opens", ui.layer == LAYER_EDIT || layer_open() == LAYER_EDIT);
        qp = ly_page(LAYER_EDIT);
        c0 = qp->id[0] != 0xFFu ? TSEL->p[qp->id[0]] : 0;
        turn(EN_K1, -2);
        btn_up(B_EDIT); frames(64);
        expect("... its KNOB 1 is the filter's CUT, the engine stays", TSEL->eng_req == e0 && qp->id[0] != 0xFFu &&
               TSEL->p[qp->id[0]] < c0);
    }
}

static void test_parts(void)
{
    uint32_t a = eng_vis(2), b = eng_vis(5);
    ui_power_on();
    song.sel = 0; set_engine_of(TSEL, a);
    song.sel = 1; set_engine_of(TSEL, b);
    song.sel = 0;
    memset(ui.fam_last, 0, sizeof ui.fam_last);
    frames(320);
    go_home(); frame();
    tap_edit();
    expect("part A: ENGINE page, A's engine", on_engine_page() && song.sel == 0u && TSEL->eng_req == a);
    turn(EN_ALGO, 1);
    frames(64);
    expect("ALGORITHM: part B, the page stays ENGINE with B's own engine", on_engine_page() && song.sel == 1u && TSEL->eng_req == b);
    turn(EN_K1, 1);
    expect("KNOB 1 on part B: B's engine steps, A's is untouched", TSEL->eng_req == eng_vis(6) && trk[0].eng_req == a);
    turn(EN_ALGO, -1);
    frames(64);
    expect("back on A: A's engine, still the ENGINE page", on_engine_page() && song.sel == 0u && TSEL->eng_req == a);
    tap_edit();
    turn(EN_ALGO, 1);
    frames(64);
    expect("a parameter page of A, then B: the page stays (that part's engine pages)", !on_engine_page() && PAGES[ui.page].fam == FAM_EDIT);
    turn(EN_SELECT, -1);
    expect("... SELECT left: B's ENGINE page", on_engine_page() && TSEL->eng_req == eng_vis(6));
}

static void test_draw(void)
{
    fresh(ENGI_FM6);
    tap_edit();
    expect("ENGINE page drawn without trouble", on_engine_page());
    draw_columns();
    {
        uint32_t s = snd();
        turn(EN_PRESET, 1);
        expect("the PRESETS knob steps the sound on the ENGINE page too", snd() != s && on_engine_page());
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    test_open_and_order();
    test_knob1();
    test_knob2();
    test_knob3();
    test_knob4();
    test_parts();
    test_draw();
    printf("engine: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
