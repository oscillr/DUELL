/* SPDX-License-Identifier: GPL-3.0-only */
/* Cloud job H, the pages of the TWIN FILTER, SUB OSC, CYCLE ENV and the QUICK MULTI layer (docs/design/FILTER_PAGES_PROPOSAL.md Option B with the owner's answers, SUB_AND_CYCLE_DESIGN.md A4 / B4 / C3).
 *   (a) fit      every label and value of every new card in every state against the unit's font table (labels 50 px in the small font, values 49 px: medium, then small), and the page titles in the
 *                header (about 150 px): the worst widths are printed
 *   (b) pages    visibility: TWIN MOD and TWIN CHAIN skipped while TYPE is OFF (page buttons, SELECT), the footer's page count, the fall back when the page under the cursor vanishes (TYPE set to OFF
 *                by a direct write as MIDI and the editor do, by a multi load, by INIT SOUND, by the knob); COMB is not offered on WAVE and PHYS; LINK and TYPE go through twin_link_set / twin_type_set
 *   (c) cards    which cards show, in which column (hide-inert and the move to the left), for every TYPE, LINK on and off, COMB; the knob addresses the card that shows; filter 1's cards never hide;
 *                the centre-filled gauge of OFFSET, TUNE, FDBK and BLEND (pixels); DRIVE OFF; BLEND's ends; the STEPS card of WAVE
 *   (d) quick    QUICK MULTI's four cards for filter 2 OFF, ON and LINKED (and COMB), ON and UNLINKED
 *   (e) names    the matrix destination names of the new rows (names only: job B made the rows)
 *   (f) graphs   the three new graph kinds draw something, the TWIN pages' pictures follow the state, the signature follows the extension slots
 * Built and run by DUELL/host_tests.sh (name: pages): the UI sources against ui_test.c's stub display and knobs. */
#define UI_TEST_NO_MAIN 1
#include <stdlib.h>
#include "ui_test.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("pages: FAIL %s\n", what);
        bad++;
    }
    return ok;
}
static void settle(void) { fm1_in.buttons = 0; frames(64); }
static uint32_t page_of(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (str_eq(PAGES[i].title, title))
            return i;
    return 0xFFFFu;
}
static void goto_page(const char *title)
{
    go_title(title);
    frames(64);
}
static void fresh(uint32_t eng)
{
    ui_power_on();
    song.sel = 0;
    set_engine_of(TSEL, eng);
    {   /* the pages are tested from the plain state: since the 0.2.31 refinement pass the engine's first factory sound (what a power-on part holds) runs filter 2 and the SUB layer */
        static const uint8_t TWP[] = {P_TWTYPE, P_CUT2, P_RES2, P_DRV2, P_BLEND};
        uint32_t k;
        for (k = 0; k < sizeof TWP; k++)
            TSEL->p[TWP[k]] = 0;
        XV(TSEL, X_LINK) = 1;
        XV(TSEL, X_ROUTE) = XV(TSEL, X_KTRK2) = XV(TSEL, X_ENV2) = XV(TSEL, X_LFO1) = XV(TSEL, X_LFO2) = XV(TSEL, X_SUBLVL) = 0;
    }
    go_main_to(HP_MORPH);
    frames(64);
}
static int hidden_col(uint32_t c) { return ui.col[c][0] == '\x7f'; }
static int card_is(uint32_t c, const char *s) { return !strncmp(ui.col[c], s, strlen(s)); }
static int blank_col(uint32_t c) { return hidden_col(c) || ui.col[c][0] == '|'; }   /* not drawn, or a blank slot */
static int cards_are(const char *a, const char *b, const char *c, const char *d)   /* "" : the column is empty (not drawn, or a blank slot) */
{
    const char *w[4] = {a, b, c, d};
    uint32_t k;
    for (k = 0; k < 4u; k++)
        if (w[k][0] ? !card_is(k, w[k]) : !blank_col(k))
            return 0;
    return 1;
}
static void set_twin(int type, int link)
{
    TSEL->p[P_TWTYPE] = (int16_t)type;
    XV(TSEL, X_LINK) = (int16_t)link;
}

/* ------------------------------------------------------------------------------------------ (a) fit --- */
static int w_lbl_max, w_val_m_max, w_val_s_max, n_small;
static int fits_card(const page_t *pg, uint32_t slot, const char *tag)
{
    int16_t *vp;
    const param_desc_t *d = page_desc(pg, slot, &vp);
    char val[16], l1[16];
    const char *unit = "", *l2, *lbl;
    int32_t v, uw, lines, ok = 1;
    if (!d || !vp || !d->label || d->label[0] == '-')
        return 1;
    lbl = desc_shown(d);
    l2 = label_split(lbl, l1);
    lines = l2 != 0;
    if (text_w(&AF_S, lines ? l1 : lbl) > LBL_W || (l2 && text_w(&AF_S, l2) > LBL_W)) {
        printf("pages: label of %s does not fit: '%s' %d px\n", tag, lbl, text_w(&AF_S, lines ? l1 : lbl));
        ok = 0;
    }
    if (text_w(&AF_S, lines ? l1 : lbl) > w_lbl_max)
        w_lbl_max = text_w(&AF_S, lines ? l1 : lbl);
    for (v = d->min; v <= d->max; v++) {
        int32_t room = COL_W - 8, wm;
        const int16_t save = *vp;
        param_format(d, (int16_t)v, val, &unit);
        *vp = (int16_t)v;
        (void)twin_card(TSEL, vp, d, v, val, &unit);      /* (the text it may change: OFF, F1 ONLY, a number) */
        *vp = save;
        card_ctr_next = 0;
        uw = unit[0] ? text_w(&AF_S, unit) + 3 : 0;
        wm = text_w(&AF_M, val) + uw;
        if (!lines && wm <= room) {
            if (wm > w_val_m_max)
                w_val_m_max = wm;
        } else {
            int ws, wl = 0;
            char *nl = strchr(val, '\n');
            if (nl) {                                      /* a value of two lines (1.0: BLEND's MULTI / 40%): each line in S, the longer one counts */
                *nl = 0;
                wl = text_w(&AF_S, nl + 1);
            }
            ws = text_w(&AF_S, val);
            if (wl > ws)
                ws = wl;
            ws += uw;
            n_small++;
            if (ws > w_val_s_max)
                w_val_s_max = ws;
            if (ws > room) {
                printf("pages: value of %s does not fit: '%s %s' %d px (room %d)\n", tag, val, unit, ws, room);
                ok = 0;
            }
        }
    }
    return ok;
}
static void test_fit(void)
{
    static const char *const PG[] = {"TWIN FILTER", "TWIN MOD", "TWIN CHAIN", "SUB OSC", "CYCLE ENV 1/2", "CYCLE ENV 2/2"};
    uint32_t p, ty, lk, s, e;
    int ok = 1, tw;
    static const uint8_t ENG[2] = {0, ENGI_WAVE};
    fresh(0);
    for (e = 0; e < 2u; e++) {
        set_engine_of(TSEL, ENG[e]);
        for (p = 0; p < NELEM(PG); p++) {
            const page_t *pg = &PAGES[page_of(PG[p])];
            for (ty = 0; ty <= TT_COMB; ty++)
                for (lk = 0; lk < 2u; lk++)
                    for (tw = 0; tw < (p == 4u ? 2 : 1); tw++) {      /* (CYCLE ENV 1/2: SYNC off and on: RISE and FALL read as note values) */
                        char tag[48];
                        set_twin((int)ty, (int)lk);
                        XV(TSEL, X_CYSYNC) = (int16_t)tw;
                        for (s = 0; s < 4u; s++) {
                            snprintf(tag, sizeof tag, "%s K%u TYPE %u LINK %u", PG[p], s + 1u, ty, lk);
                            ok &= fits_card(pg, s, tag);
                        }
                    }
        }
    }
    set_twin(0, 1);
    XV(TSEL, X_CYSYNC) = 0;
    expect("(a) every label and value of every new card fits its card in every state (labels 50 px, values 49 px, medium then small)", ok);
    printf("pages: widest label %d px (limit %d); widest value in the medium font %d px; %d values in the small font, the widest %d px (limit %d)\n", w_lbl_max, (int)LBL_W, w_val_m_max, n_small, w_val_s_max, COL_W - 8);
    {   /* the names the owner named: PARAL, FDBK, OFFSET fit; PARALLEL, FEEDBACK do not */
        expect("(a) PARAL, FDBK, OFFSET fit; PARALLEL (70 px medium, 55 small) and FEEDBACK (58 px) do not", text_w(&AF_M, "PARAL") <= COL_W - 8 && text_w(&AF_S, "FDBK") <= (int)LBL_W && text_w(&AF_S, "OFFSET") <= (int)LBL_W &&
                text_w(&AF_S, "PARALLEL") > COL_W - 8 && text_w(&AF_S, "FEEDBACK") > (int)LBL_W);
    }
    {   /* the page titles in the header (about 150 px of room, medium font) */
        static const char *const T[] = {"TWIN FILTER", "TWIN MOD", "TWIN CHAIN", "SUB OSC", "CYCLE ENV 1/2", "CYCLE ENV 2/2"};
        int tok = 1;
        for (p = 0; p < NELEM(T); p++) {
            int w = text_w(&AF_M, title_shown(T[p]));
            printf("pages: title %-14s %3d px (header room about 150)\n", T[p], w);
            tok &= w <= 150;
        }
        expect("(a) the new page titles fit the header", tok);
    }
}

/* ---------------------------------------------------------------------------------------- (b) pages --- */
static uint32_t edit_count(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < NPAGES; i++)
        n += PAGES[i].fam == FAM_EDIT && page_visible(i);
    return n;
}
static void foot_count(uint32_t *k, uint32_t *n)
{
    char nm[20], ct[8];
    page_parts(nm, ct);
    *k = (uint32_t)atoi(ct);
    *n = (uint32_t)atoi(strchr(ct, '/') ? strchr(ct, '/') + 1 : "0");
}
static void test_pages(void)
{
    uint32_t k, n, n0, ti = page_of("TWIN FILTER"), mi = page_of("TWIN MOD"), ci = page_of("TWIN CHAIN");
    int ok;
    fresh(0);
    set_twin(0, 1);
    n0 = edit_count();
    expect("(b) TYPE OFF: TWIN FILTER shows, TWIN MOD and TWIN CHAIN are skipped", page_visible(ti) && !page_visible(mi) && !page_visible(ci));
    set_twin(TT_LP, 1);
    expect("(b) TYPE LP: all three show; the EDIT round is two pages longer", page_visible(ti) && page_visible(mi) && page_visible(ci) && edit_count() == n0 + 2u);
    for (k = 1, ok = 1; k <= TT_COMB; k++) {
        set_twin((int)k, 1);
        ok &= page_visible(mi) && page_visible(ci);
    }
    expect("(b) every other TYPE (BP HP NOTCH LADDER COMB) shows them", ok);
    set_twin(0, 1);
    goto_page("TWIN FILTER");
    foot_count(&k, &n);
    expect("(b) the footer's counter counts visible pages only (OFF: TWIN FILTER is k of n, n = the visible EDIT pages)", n == n0 && k >= 1u && k < n && PAGES[ui.page].fam == FAM_EDIT);
    set_twin(TT_LP, 1);
    frame();
    foot_count(&k, &n);
    expect("(b) .. and grows by two when TYPE leaves OFF", n == n0 + 2u);
    {   /* the page buttons and SELECT step over the skipped pages */
        const char *nxt;
        set_twin(0, 1);
        goto_page("TWIN FILTER");
        ui.fam_last[FAM_EDIT] = ui.page;
        press(B_EDIT); frames(320);
        nxt = cur_page()->title;
        expect("(b) TYPE OFF: the EDIT button from TWIN FILTER goes to VOICE 1/2", str_eq(nxt, "VOICE 1/2"));
        goto_page("TWIN FILTER");
        turn(EN_SELECT, 1); frame();
        expect("(b) TYPE OFF: SELECT right from TWIN FILTER goes to VOICE 1/2", str_eq(cur_page()->title, "VOICE 1/2"));
        turn(EN_SELECT, -1); frame();
        expect("(b) TYPE OFF: SELECT left from VOICE 1/2 goes to TWIN FILTER", str_eq(cur_page()->title, "TWIN FILTER"));
        set_twin(TT_LP, 1);
        goto_page("TWIN FILTER");
        turn(EN_SELECT, 1); frame();
        ok = str_eq(cur_page()->title, "TWIN MOD");
        turn(EN_SELECT, 1); frame();
        ok &= str_eq(cur_page()->title, "TWIN CHAIN");
        turn(EN_SELECT, 1); frame();
        ok &= str_eq(cur_page()->title, "VOICE 1/2");
        turn(EN_SELECT, -1); frame();
        ok &= str_eq(cur_page()->title, "TWIN CHAIN");
        expect("(b) TYPE LP: SELECT goes TWIN FILTER, TWIN MOD, TWIN CHAIN, VOICE 1/2 and back", ok);
    }
    {   /* the page vanishes under the cursor: TYPE set to OFF by a direct write (MIDI, the editor), a multi load, INIT SOUND, the knob */
        static const char *const HOW[4] = {"a direct write (MIDI, the editor)", "a multi load", "INIT SOUND", "the TYPE knob"};
        uint32_t how, pg;
        for (pg = 0; pg < 2u; pg++)
            for (how = 0; how < 4u; how++) {
                char w[120];
                fresh(0);
                set_twin(TT_LADDER, 1);
                goto_page(pg ? "TWIN CHAIN" : "TWIN MOD");
                if (how == 0u) {
                    TSEL->p[P_TWTYPE] = 0;
                } else if (how == 1u) {
                    project_load(0);
                    TSEL->p[P_TWTYPE] = 0;                    /* (the factory multis hold no filter 2) */
                } else if (how == 2u) {
                    init_sound();
                    TSEL->p[P_TWTYPE] = 0;                    /* (INIT SOUND loads the engine's first factory sound; since the 0.2.31 refinement pass ANALOG's has filter 2 on: the page rule is the test, not that sound) */
                } else {
                    goto_page("TWIN FILTER");
                    turn(EN_K1, -9);
                    frames(64);
                    goto_page(pg ? "TWIN CHAIN" : "TWIN MOD");
                }
                frames(64);
                snprintf(w, sizeof w, "(b) on %s, TYPE to OFF by %s: the page before it, TWIN FILTER", PAGES[page_of(pg ? "TWIN CHAIN" : "TWIN MOD")].title, HOW[how]);
                expect(w, TSEL->p[P_TWTYPE] == 0 && str_eq(cur_page()->title, "TWIN FILTER"));
            }
    }
    {   /* a page that vanishes for another reason keeps going to the family's first */
        fresh(ENGI_FM6);
        goto_page("OP TUNE");
        set_engine_of(TSEL, ENGI_WAVE);
        frames(64);
        expect("(b) OP TUNE gone with the engine: ENGINE as before", str_eq(cur_page()->title, "ENGINE"));
    }
    {   /* TYPE through the knob: LINK and COMB */
        fresh(0);
        goto_page("TWIN FILTER");
        turn(EN_K1, 9);
        frames(64);
        expect("(b) the TYPE knob reaches COMB on ANALOG", TSEL->p[P_TWTYPE] == TT_COMB);
        fresh(ENGI_WAVE);
        goto_page("TWIN FILTER");
        turn(EN_K1, 9);
        frames(64);
        expect("(b) COMB is not in the TYPE list on WAVE (the knob stops at LADDER)", TSEL->p[P_TWTYPE] == TT_LADDER);
        fresh(ENGI_PHYS);
        goto_page("TWIN FILTER");
        turn(EN_K1, 9);
        frames(64);
        expect("(b) .. and on PHYS", TSEL->p[P_TWTYPE] == TT_LADDER);
        fresh(0);
        goto_page("TWIN FILTER");
        set_twin(TT_LP, 1);
        TSEL->p[P_FREEA] = 50;
        TSEL->p[P_CUT2] = 7;                         /* OFFSET +7 semitones above filter 1's 50 (a dial step is 1.324 st: 5.3 steps) */
        frames(64);
        turn(EN_K2, -1);                             /* LINK to OFF: the sound stays (twin_link_set): CUTOFF 55 */
        frames(64);
        expect("(b) the LINK knob keeps the sound: OFFSET +7 st over CUTOFF 50 becomes CUTOFF 55 (twin_link_set)", XV(TSEL, X_LINK) == 0 && TSEL->p[P_CUT2] == 55);
        turn(EN_K2, 1);
        frames(64);
        expect("(b) .. and back: OFFSET +7", XV(TSEL, X_LINK) == 1 && TSEL->p[P_CUT2] == 7);
    }
}

/* ----------------------------------------------------------------------------------------- (c) cards --- */
static int gauge_filled(uint32_t c, int32_t dx)       /* the pixel of card c's gauge row, dx px from the track's centre: not the track's colour */
{
    int32_t x = CARD_X(c) + 5 + (CARD_W - 10) / 2 + dx, y = (int32_t)Y_LABEL + 39;
    return host_screen[y * 240 + x] != swap16(T_BG) && host_screen[y * 240 + x] != swap16(T_SURF);
}
static void test_cards(void)
{
    uint32_t ty, k;
    int ok;
    fresh(0);
    goto_page("TWIN FILTER");
    set_twin(0, 1);
    frames(64);
    expect("(c) TWIN FILTER, TYPE OFF: the TYPE card alone", cards_are("TYPE|OFF", "", "", ""));
    set_twin(TT_LP, 1);
    frames(64);
    expect("(c) LP, LINK on: TYPE, LINK, OFFSET, RES", cards_are("TYPE|LP", "LINK|ON", "OFFSET|0|st", "RES|0|%"));
    set_twin(TT_LP, 0);
    TSEL->p[P_CUT2] = UF_STEPS;
    frames(64);
    expect("(c) LP, LINK off: the third card is CUTOFF (a plain cutoff)", cards_are("TYPE|LP", "LINK|OFF", "CUTOFF|16.0|kHz", "RES|0|%"));
    for (ty = TT_BP; ty <= TT_LADDER; ty++) {
        char w[80];
        set_twin((int)ty, 1);
        TSEL->p[P_CUT2] = 0;
        frames(64);
        snprintf(w, sizeof w, "(c) TYPE %u (BP HP NOTCH LADDER) shows all four cards", ty);
        expect(w, !hidden_col(0) && !hidden_col(1) && !hidden_col(2) && !hidden_col(3));
    }
    set_twin(TT_COMB, 1);
    TSEL->p[P_CUT2] = 0;
    TSEL->p[P_RES2] = 0;
    frames(64);
    expect("(c) COMB: LINK hidden, the others move left: TYPE, TUNE, FDBK, empty", cards_are("TYPE|COMB", "TUNE|0|st", "FDBK|0|%", ""));
    turn(EN_K2, 2);
    frames(64);
    expect("(c) COMB: K2 is TUNE (not the hidden LINK)", TSEL->p[P_CUT2] > 0 && XV(TSEL, X_LINK) == 1);
    turn(EN_K3, -2);
    frames(64);
    expect("(c) COMB: K3 is FDBK", TSEL->p[P_RES2] < 0);
    turn(EN_K4, 3);
    frames(64);
    expect("(c) COMB: K4 does nothing (no card)", TSEL->p[P_BLEND] == 0 && TSEL->p[P_DRV2] == 0);

    /* TWIN MOD */
    goto_page("TWIN MOD");
    set_twin(TT_LP, 1);
    frames(64);
    expect("(c) TWIN MOD (LP): KEY TRACK, FILT ENV, LFO 1, LFO 2", cards_are("KEY", "FILT ENV|", "LFO 1|", "LFO 2|"));
    set_twin(TT_COMB, 1);
    frames(64);
    expect("(c) TWIN MOD on COMB: KEY TRACK hidden, FILT ENV, LFO 1, LFO 2 move left", cards_are("FILT ENV|", "LFO 1|", "LFO 2|", ""));
    turn(EN_K1, 5);
    frames(64);
    expect("(c) .. K1 is FILT ENV", XV(TSEL, X_ENV2) > 0 && XV(TSEL, X_KTRK2) == 0);

    /* TWIN CHAIN */
    goto_page("TWIN CHAIN");
    set_twin(TT_LP, 1);
    XV(TSEL, X_ROUTE) = 0;
    TSEL->p[P_DRV2] = 0;
    TSEL->p[P_BLEND] = 0;
    frames(64);
    expect("(c) TWIN CHAIN: ROUTE SERIES, DRIVE OFF, BLEND BOTH, K4 empty", cards_are("ROUTE|SERIES", "DRIVE|OFF", "BLEND|BOTH", ""));
    XV(TSEL, X_ROUTE) = 1;
    TSEL->p[P_DRV2] = 40;
    TSEL->p[P_BLEND] = -64;
    frames(64);
    expect("(c) .. PARAL, DRIVE 40, BLEND MULTI at -64", cards_are("ROUTE|PARAL", "DRIVE|40", "BLEND|MULTI|", ""));
    TSEL->p[P_BLEND] = 63;
    frames(64);
    expect("(c) .. BLEND TWIN at +63", card_is(2, "BLEND|TWIN|"));
    TSEL->p[P_BLEND] = 20;
    frames(64);
    expect("(c) .. BLEND between: the name and the percent of the way from BOTH, TWIN 31% at +20 (20 * 100 / 63)", card_is(2, "BLEND|TWIN\n31%"));
    TSEL->p[P_BLEND] = -20;
    frames(64);
    expect("(c) .. and MULTI 31% at -20 (20 * 100 / 64)", card_is(2, "BLEND|MULTI\n31%"));
    turn(EN_K4, 3);
    frames(64);
    expect("(c) .. K4 does nothing", TSEL->p[P_BLEND] == -20 && TSEL->p[P_DRV2] == 40);

    /* filter 1's cards never hide, whatever filter 2 is */
    fresh(ENGI_WAVE);                                    /* (an engine with the multimode filter and its three amount cards; ANALOG, TRIO, NOISE are job J's) */
    for (ty = 0, ok = 1; ty <= TT_LADDER; ty++) {
        set_twin((int)ty, (int)(ty & 1u));
        goto_page("MULTI FILTER 1/2");
        frames(64);
        for (k = 0; k < 4u; k++)
            ok &= !hidden_col(k);
        goto_page("MULTI FILTER 2/2");
        frames(64);
        for (k = 0; k < 4u; k++)
            ok &= !hidden_col(k);
    }
    expect("(c) filter 1's pages show all four cards for every filter 2 state", ok);

    /* the centre-filled gauge */
    goto_page("TWIN FILTER");
    set_twin(TT_LP, 1);
    TSEL->p[P_CUT2] = 40;
    ui.hot_t = 0;
    frames(64);
    ok = gauge_filled(2, 10) && !gauge_filled(2, -10);
    TSEL->p[P_CUT2] = -40;
    frames(64);
    ok &= !gauge_filled(2, 10) && gauge_filled(2, -10);
    TSEL->p[P_CUT2] = 0;
    frames(64);
    ok &= !gauge_filled(2, 10) && !gauge_filled(2, -10) && (gauge_filled(2, 0) || gauge_filled(2, -1));
    expect("(c) OFFSET's gauge fills from the centre: right for +, left for -, a tick at 0", ok);
    set_twin(TT_LP, 0);
    TSEL->p[P_CUT2] = 60;
    frames(64);
    ok = gauge_filled(2, -10) && !gauge_filled(2, 20);
    expect("(c) CUTOFF (LINK off) fills from the left, as every unipolar card", ok);
    goto_page("TWIN CHAIN");
    set_twin(TT_LP, 1);
    TSEL->p[P_BLEND] = 63;                               /* (1.0: between the ends the card is the two small lines MULTI / 40%, which have no gauge; the ends keep it) */
    frames(64);
    ok = gauge_filled(2, 6) && !gauge_filled(2, -6);
    TSEL->p[P_BLEND] = -64;
    frames(64);
    ok &= !gauge_filled(2, 6) && gauge_filled(2, -6);
    expect("(c) BLEND's gauge fills from the centre", ok);
    goto_page("TWIN FILTER");
    frames(64);
    expect("(c) TYPE shows no gauge (a list)", !gauge_filled(0, -15) && !gauge_filled(0, 15) && !gauge_filled(0, 0));

    /* WAVE's STEPS card (job F: the engine's own edit slot E7 = P_E6): on EDIT 1's K4, EDIT 2 (the empty E8) is skipped */
    fresh(ENGI_WAVE);
    goto_page("EDIT 1");
    TSEL->p[P_E6] = 0;
    frames(64);
    expect("(c) WAVE EDIT 1: SYNC, DTN, STEPS (OFF), the fourth card empty (its own SUB went: cloud job J; DTN and STEPS moved one place left)", cards_are("SYNC|", "DETUNE|", "STEPS|OFF", ""));
    TSEL->p[P_E6] = 2;
    frames(64);
    expect("(c) .. STEPS 3 places is stored 2 and shows 3", card_is(2, "STEPS|3"));
    expect("(c) WAVE's EDIT 2 (E8, unused) is not in the round", !page_visible(page_of("EDIT 2")));
    fresh(0);
    goto_page("EDIT 1");
    frames(64);
    expect("(c) another engine (ANALOG): no STEPS card anywhere on EDIT 1", !card_is(0, "STEPS") && !card_is(1, "STEPS") && !card_is(2, "STEPS") && !card_is(3, "STEPS"));
}

/* ----------------------------------------------------------------------------------------- (d) quick --- */
static void open_quick(void)
{
    btn_down(B_EDIT);
    frames(480);
}
static void close_quick(void)
{
    btn_up(B_EDIT);
    frames(64);
}
static void test_quick(void)
{
    const page_t *qp;
    int ok;
    fresh(0);
    go_main_to(HP_MORPH);
    set_twin(0, 1);
    open_quick();
    qp = ly_page(LAYER_EDIT);
    ok = ui.layer == LAYER_EDIT && qp->id[0] == P_FREEA && qp->id[1] == P_FREEB && qp->id[2] == P_FREEC && qp->id[3] == P_REL;
    expect("(d) QUICK MULTI, filter 2 OFF: CUT, RES, MODE, AMP REL (as today)", ok);
    expect("(d) .. its cards", cards_are("CUTOFF|", "RES|", "MODE|", "RELEASE|") || cards_are("CUTOFF|", "RES|", "MODE|", "REL"));
    close_quick();

    set_twin(TT_LP, 1);
    open_quick();
    qp = ly_page(LAYER_EDIT);
    ok = ui.layer == LAYER_EDIT && qp->id[0] == P_FREEA && qp->id[1] == P_FREEB && qp->id[2] == P_DRV2 && qp->id[3] == P_REL;
    expect("(d) filter 2 on and LINKED: CUT, RES, DRIVE, AMP REL", ok);
    expect("(d) .. its cards (DRIVE OFF)", card_is(2, "DRIVE|OFF"));
    TSEL->p[P_DRV2] = 0;
    turn(EN_K3, 5);
    frames(64);
    expect("(d) .. K3 turns DRIVE", TSEL->p[P_DRV2] > 0);
    close_quick();

    set_twin(TT_LP, 0);
    TSEL->p[P_CUT2] = 30;
    TSEL->p[P_RES2] = 10;
    open_quick();
    qp = ly_page(LAYER_EDIT);
    ok = ui.layer == LAYER_EDIT && qp->id[0] == P_FREEA && qp->id[1] == P_FREEB && qp->id[2] == P_CUT2 && qp->id[3] == P_RES2;
    expect("(d) filter 2 on and UNLINKED: CUT 1, RES 1, CUT 2, RES 2", ok);
    expect("(d) .. the cards are CUTOFF, RES, CUTOFF, RES under the group cells MULTI FILTER and TWIN FILTER", cards_are("CUTOFF|", "RES|", "CUTOFF|", "RES|"));
    {
        int16_t c2 = TSEL->p[P_CUT2], r2 = TSEL->p[P_RES2], c1 = TSEL->p[P_FREEA];
        turn(EN_K3, -2);
        turn(EN_K4, 3);
        turn(EN_K1, -2);
        frames(64);
        ok = TSEL->p[P_CUT2] != c2 && TSEL->p[P_RES2] != r2 && TSEL->p[P_FREEA] != c1 && XV(TSEL, X_LINK) == 0 && TSEL->p[P_TWTYPE] == TT_LP;
        expect("(d) .. K1 K3 K4 turn CUTOFF 1, the absolute CUTOFF 2 and RES 2", ok);
        oct_back();
        frames(64);
        expect("(d) .. OCT- puts the four back as the layer opened", TSEL->p[P_CUT2] == c2 && TSEL->p[P_RES2] == r2 && TSEL->p[P_FREEA] == c1);
    }
    close_quick();

    set_twin(TT_COMB, 1);
    open_quick();
    qp = ly_page(LAYER_EDIT);
    expect("(d) COMB follows the key by itself: the LINKED layout (CUT, RES, DRIVE, AMP REL)", qp->id[2] == P_DRV2 && qp->id[3] == P_REL);
    close_quick();

    set_twin(TT_LP, 0);
    open_quick();
    expect("(d) the other QUICK layers are unchanged (LFO, ENV)", ly_page(LAYER_LFO)->id[1] == P_LD_FLT && ly_page(LAYER_ENV)->id[2] == P_E2FLT);
    close_quick();
    /* the filter 2 state changes while the layer is shown: its cards follow */
    set_twin(TT_LP, 1);
    open_quick();
    expect("(d) LINK on again: DRIVE is back on K3", card_is(2, "DRIVE|"));
    close_quick();
}

/* ----------------------------------------------------------------------------------------- (e) names --- */
static void test_names(void)
{
    char b[40];
    const char *more;
    fresh(0);
    set_twin(0, 1);
    expect("(e) the TWIN destinations are not offered while TYPE is OFF", !mod_dst_offered(TSEL, MD_CUT2) && !mod_dst_offered(TSEL, MD_RES2) && !mod_dst_offered(TSEL, MD_DRIVE) && !mod_dst_offered(TSEL, MD_BLEND));
    set_twin(TT_LP, 1);
    expect("(e) .. and are while it is not", mod_dst_offered(TSEL, MD_CUT2) && mod_dst_offered(TSEL, MD_RES2) && mod_dst_offered(TSEL, MD_DRIVE) && mod_dst_offered(TSEL, MD_BLEND));
    expect("(e) card names: TWIN / CUTOFF, TWIN / RES, FILTER / DRIVE, FILTER / BLEND, SUB / LEVEL",
           str_eq(mod_dst_name(TSEL, MD_CUT2), "TWIN\nCUTOFF") && str_eq(mod_dst_name(TSEL, MD_RES2), "TWIN\nRES") && str_eq(mod_dst_name(TSEL, MD_DRIVE), "FILTER\nDRIVE") &&
           str_eq(mod_dst_name(TSEL, MD_BLEND), "FILTER\nBLEND") && str_eq(mod_dst_name(TSEL, MD_SUBLVL), "SUB\nLEVEL"));
    mod_dst_line(TSEL, MD_CUT2, b, &more);
    expect("(e) full line TWIN FILTER CUTOFF", str_eq(b, "DEST: TWIN FILTER CUTOFF"));
    set_twin(TT_COMB, 1);
    mod_dst_line(TSEL, MD_CUT2, b, &more);
    expect("(e) TWIN CUTOFF reads TUNE on COMB (card and full line)", str_eq(mod_dst_name(TSEL, MD_CUT2), "TWIN\nTUNE") && str_eq(b, "DEST: TWIN FILTER TUNE"));
    mod_dst_line(TSEL, MD_RES2, b, &more);
    expect("(e) TWIN / RES reads FEEDBACK on COMB", str_eq(mod_dst_name(TSEL, MD_RES2), "TWIN\nFDBK") && str_eq(b, "DEST: TWIN FILTER FEEDBACK"));
    expect("(e) the card names fit (every line of the two-line names in the small font, 49 px; the matrix row names MULTI CUT / TWIN CUT / MULTI RES / TWIN RES in 64 px; the longest full line in the 212 px of the DEST line)", text_w(&AF_S, "MULTI") <= COL_W - 8 && text_w(&AF_S, "TWIN") <= COL_W - 8 && text_w(&AF_S, "CUTOFF") <= COL_W - 8 && text_w(&AF_S, "FILTER") <= (int)LBL_W && text_w(&AF_S, "MULTI CUT") <= 64 && text_w(&AF_S, "TWIN CUT") <= 64 && text_w(&AF_S, "MULTI RES") <= 64 && text_w(&AF_S, "TWIN RES") <= 64 && text_w(&AF_M, "DEST: TWIN FILTER FEEDBACK") <= 212 && text_w(&AF_M, "DEST: MULTI FILTER CUTOFF") <= 212);
}

/* ---------------------------------------------------------------------------------------- (f) graphs --- */
static uint32_t graph_hash(void)
{
    uint32_t h = 2166136261u, y, x;
    for (y = Y_GRAPH; y < Y_GRAPH + H_GRAPH; y++)
        for (x = 0; x < 240u; x++)
            h = (h ^ host_screen[y * 240u + x]) * 16777619u;
    return h;
}
static uint32_t ink_rows(uint32_t y0, uint32_t y1)    /* the pixels in the panel's rows that are neither the panel nor the background */
{
    uint32_t n = 0, y, x;
    for (y = y0; y < y1; y++)
        for (x = 12; x < 228u; x++)
            n += host_screen[y * 240u + x] != swap16(T_SURF) && host_screen[y * 240u + x] != swap16(T_BG);
    return n;
}
static void test_graphs(void)
{
    uint32_t h0, h1, h2;
    int ok;
    fresh(0);
    set_twin(0, 1);
    goto_page("TWIN FILTER");
    frames(64);
    h0 = graph_hash();
    expect("(f) TWIN FILTER, filter 2 OFF: filter 1's curve and the caption FILTER 2 OFF", ink_rows(Y_GRAPH + 20, Y_GRAPH + 110) > 150 && ink_rows(Y_GRAPH + 108, Y_GRAPH + 120) > 20);
    set_twin(TT_LP, 1);
    TSEL->p[P_CUT2] = 8;
    frames(64);
    h1 = graph_hash();
    set_twin(TT_LP, 0);
    TSEL->p[P_CUT2] = 20;
    frames(64);
    h2 = graph_hash();
    expect("(f) the picture follows TYPE, LINK and the cutoff (three different pictures)", h0 != h1 && h1 != h2 && h0 != h2);
    set_twin(TT_COMB, 1);
    TSEL->p[P_RES2] = 30;
    frames(64);
    h0 = graph_hash();
    TSEL->p[P_RES2] = 50;
    frames(64);
    expect("(f) COMB draws its teeth and FDBK moves them", h0 != graph_hash());
    goto_page("TWIN CHAIN");
    set_twin(TT_LP, 1);
    XV(TSEL, X_ROUTE) = 0;
    TSEL->p[P_DRV2] = 40;
    frames(64);
    h0 = graph_hash();
    XV(TSEL, X_ROUTE) = 1;
    frames(64);
    h1 = graph_hash();
    TSEL->p[P_DRV2] = 0;
    frames(64);
    h2 = graph_hash();
    expect("(f) TWIN CHAIN: SERIES and PARAL differ (the one-line strip and the two branches), the word DRIVE is dim while DRIVE is OFF", h0 != h1 && h1 != h2);
    goto_page("MULTI FILTER 1/2");
    set_twin(0, 1);
    frames(64);
    h0 = graph_hash();
    set_twin(TT_LP, 1);
    frames(64);
    h1 = graph_hash();
    expect("(f) MULTI FILTER 1/2 draws filter 2 dim and the caption FILTER 2 FOLLOWS FILTER 1 only while filter 2 runs", h0 != h1);

    goto_page("SUB OSC");
    XV(TSEL, X_SUBLVL) = 0;
    XV(TSEL, X_SUBHARM) = 0;
    frames(64);
    h0 = graph_hash();
    expect("(f) SUB OSC at LEVEL 0: the flat line and 'NOT RUN AT LEVEL 0'", ink_rows(Y_GRAPH + 108, Y_GRAPH + 120) > 20);
    XV(TSEL, X_SUBLVL) = 100;
    frames(64);
    h1 = graph_hash();
    XV(TSEL, X_SUBHARM) = 100;
    frames(64);
    h2 = graph_hash();
    ok = h0 != h1 && h1 == h2;                  /* (HARM is disabled since 0.2.31: a stored value draws no octave copies) */
    XV(TSEL, X_SUBWAVE) = 2;
    frames(64);
    ok &= graph_hash() != h2;
    XV(TSEL, X_SUBOCT) = 0;
    frames(64);
    expect("(f) SUB OSC: LEVEL, WAVE and OCTAVE each change the picture; a stored HARM changes nothing (disabled, 0.2.31)", ok);

    goto_page("CYCLE ENV 1/2");
    XV(TSEL, X_CYMODE) = 0;
    XV(TSEL, X_CYRISE) = 10;
    XV(TSEL, X_CYFALL) = 70;
    frames(64);
    h0 = graph_hash();
    XV(TSEL, X_CYMODE) = 1;
    frames(64);
    h1 = graph_hash();
    XV(TSEL, X_CYMODE) = 0;
    XV(TSEL, X_CYRISE) = 80;
    frames(64);
    h2 = graph_hash();
    expect("(f) CYCLE ENV 1/2: LOOP (three shapes), ONCE (one) and RISE each change the picture", h0 != h1 && h0 != h2);
    goto_page("CYCLE ENV 2/2");
    XV(TSEL, X_CYCURVE) = 0;
    frames(64);
    h0 = graph_hash();
    XV(TSEL, X_CYCURVE) = 40;
    frames(64);
    h1 = graph_hash();
    XV(TSEL, X_CYCURVE) = -40;
    frames(64);
    h2 = graph_hash();
    expect("(f) CYCLE ENV 2/2: CURVE bends the shape both ways", h0 != h1 && h0 != h2 && h1 != h2);
}
static void test_sub_cycle_cards(void)
{
    fresh(0);
    goto_page("SUB OSC");
    XV(TSEL, X_SUBWAVE) = 0;
    XV(TSEL, X_SUBOCT) = 1;
    XV(TSEL, X_SUBHARM) = 0;
    XV(TSEL, X_SUBLVL) = 0;
    frames(64);
    expect("(g) SUB OSC: WAVE SINE, OCTAVE -1, LEVEL, no fourth card (HARM is disabled since 0.2.31; none hides: a value of 0 is a value)", card_is(0, "WAVE|SINE") && card_is(1, "OCTAVE|-1") && card_is(2, "LEVEL|0") && blank_col(3));
    turn(EN_K1, 2);
    turn(EN_K2, -1);
    turn(EN_K3, 5);
    frames(64);
    expect("(g) .. the knobs turn the extension values", XV(TSEL, X_SUBWAVE) == 2 && XV(TSEL, X_SUBOCT) == 0 && XV(TSEL, X_SUBHARM) == 0 && XV(TSEL, X_SUBLVL) > 0);
    goto_page("CYCLE ENV 1/2");
    XV(TSEL, X_CYMODE) = 0;
    XV(TSEL, X_CYSYNC) = 0;
    frames(64);
    expect("(g) CYCLE ENV 1/2: MODE LOOP, SYNC OFF, RISE, FALL", card_is(0, "MODE|LOOP") && card_is(1, "SYNC|OFF") && card_is(2, "RISE|") && card_is(3, "FALL|"));
    XV(TSEL, X_CYSYNC) = 1;
    frames(64);
    expect("(g) .. with SYNC on RISE and FALL read note values", card_is(1, "SYNC|ON") && !card_is(2, "RISE|0 ms"));
    goto_page("CYCLE ENV 2/2");
    frames(64);
    expect("(g) CYCLE ENV 2/2: CURVE on K1 alone", cards_are("CURVE|", "", "", ""));
}

int main(void)
{
    test_fit();
    test_pages();
    test_cards();
    test_quick();
    test_names();
    test_graphs();
    test_sub_cycle_cards();
    printf("pages: %d checks, %d failed\n", ncheck, bad);
    if (bad)
        printf("PAGES TEST FAILED\n");
    return bad != 0;
}
