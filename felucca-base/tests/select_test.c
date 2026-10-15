/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL (SPEC 1.1 / 1.2 / 1.3 / 5): the SELECT knob is the select knob and never the tempo.
 *   SELECT     turned left / right on every screen (every page the UI has, for every engine the selected part can have,
 *              stopped and playing; HOME; the menu; ABOUT and INFO; a dialog; NAME; with a layer button held): the tempo
 *              (song.g[G_BPM]) never changes. On a page of a family with sub-pages it steps to the previous / next
 *              sub-page (wrapping), the one the page button pressed again goes to; with one page it stays; HOME has
 *              its own round (HOME, its pages); in the menu it moves the row (continuously across the tabs, clamped, sweep_menu); in a dialog, NAME and a layer: nothing.
 *   TEMPO      GLO's TEMPO page: KNOB 1 is TEMPO (G_BPM) and sets it; with an external clock (CLK USB / TRS, set on the same page's
 *              KNOB 4) it is inert and shows EXT; KNOB 2 TAP: a detent either way is one tap, the BPM is the mean of the last taps.
 *   no BPM LOCK  the MENU has no such row (the tempo has no lock to keep).
 * Built and run by DUELL/host_tests.sh (name: select): the UI sources against ui_test.c's stub display and knobs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static int ncheck;

static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("select: FAIL %s\n", what);
        bad++;
    }
    return ok;
}

static uint32_t fam_count(uint32_t fam)             /* the visible pages of a family */
{
    uint32_t i, n = 0;
    for (i = 0; i < NPAGES; i++)
        n += PAGES[i].fam == fam && page_visible(i);
    return n;
}
static uint32_t fam_step(uint32_t from, int dir)    /* the table walk the page button's taps make (open_family) */
{
    uint32_t i = from, n;
    for (n = 0; n < NPAGES; n++) {
        i = dir > 0 ? (i + 1u) % NPAGES : (i + NPAGES - 1u) % NPAGES;
        if (PAGES[i].fam == PAGES[from].fam && page_visible(i))
            break;
    }
    return i;
}
/* HOME's round (SPEC 3.5): HOME's pages in table order (MORPH, MORPH SET, MODE, MIXER); 0.2.14: no OVERVIEW after them */
static uint32_t home_round(uint8_t *r)
{
    uint32_t i, m = 0;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_TRK && page_visible(i))
            r[m++] = (uint8_t)i;
    return m;
}
static uint32_t where(void) { return ui.page; }
static void enter(uint32_t i)
{
    ui.menu = 0; ui.confirm = 0; ui.layer = 0;
    ui.page = (uint8_t)i; page_entered();
    frames(48);
}
static void settle(void) { fm1_in.buttons = 0; frames(64); }

/* every page, every engine, stopped and playing */
static void sweep_pages(void)
{
    uint32_t e, i, pass, pages = 0, moved = 0, kept = 0;
    for (pass = 0; pass < 2u; pass++)
        for (e = 0; e < NENGINES; e++) {
            if (!eng_ok(e))
                continue;
            for (i = 0; i < NPAGES; i++) {
                uint32_t n, nxt, prv;
                char w[96];
                ui_power_on();
                set_engine_of(TSEL, e);
                TSEL->engine = TSEL->eng_req;
                song.g[G_BPM] = 117;
                if (pass) song.playing = 1;
                if (!page_visible(i))
                    continue;
                enter(i);
                n = fam_count(PAGES[i].fam);
                nxt = fam_step(i, 1);
                prv = fam_step(i, -1);
                pages++;
                snprintf(w, sizeof w, "%s/%s%s", ENGINES[e]->name, PAGES[i].title, pass ? " (playing)" : "");
                if (PAGES[i].fam == FAM_TRK) {                   /* HOME's pages: the round with HOME (sweep_home) */
                    uint8_t r[8];
                    uint32_t m = home_round(r), at = 0;
                    while (at < m && r[at] != i)
                        at++;
                    turn(EN_SELECT, 1);
                    expect(w, song.g[G_BPM] == 117 && where() == r[(at + 1u) % m]);
                    enter(i);
                    turn(EN_SELECT, -1);
                    expect(w, song.g[G_BPM] == 117 && where() == r[(at + m - 1u) % m]);
                    kept++;
                    continue;
                }

                turn(EN_SELECT, 1);
                expect(w, song.g[G_BPM] == 117 && !ui.menu);
                if (n > 1) {
                    expect("SELECT right: the next sub-page", ui.page == nxt);
                    moved++;
                } else {
                    expect("SELECT right: one page only, it stays", ui.page == i);
                    kept++;
                }
                turn(EN_SELECT, -1);
                expect("SELECT left: back", ui.page == i && song.g[G_BPM] == 117);
                turn(EN_SELECT, -1);
                expect("SELECT left: the previous sub-page", ui.page == (n > 1 ? prv : i) && song.g[G_BPM] == 117);
                turn(EN_SELECT, 3);                       /* several detents in a frame: still the tempo untouched */
                expect("SELECT 3 detents: tempo untouched", song.g[G_BPM] == 117);
                if (n > 1 && pass == 0u && ui.page != i) {   /* round the whole family: back where it began */
                    uint32_t k;
                    enter(i);
                    for (k = 0; k < n; k++)
                        turn(EN_SELECT, 1);
                    expect("SELECT right x pages: round the family", ui.page == i);
                    for (k = 0; k < n; k++)
                        turn(EN_SELECT, -1);
                    expect("SELECT left x pages: round the family", ui.page == i);
                }
            }
        }
    printf("select: %u page screens (all engines, stopped and playing); %u stepped, %u single-page or HOME's\n", pages, moved, kept);
}

/* SELECT does what the page button pressed again does (taps of the button), family by family */
static void sweep_buttons(void)
{
    static const uint8_t BTN[] = {B_ENV, B_LFO, B_FX, B_SCL, B_EDIT, B_GLO, B_SAVE, B_ARP, B_SEQ};
    uint32_t b, i;
    for (b = 0; b < sizeof BTN; b++)
        for (i = 0; i < NPAGES; i++) {
            uint32_t sel_page;
            char w[64];
            ui_power_on();
            song.g[G_BPM] = 117;
            if (!page_visible(i) || FAM_BTN[PAGES[i].fam] != BTN[b] || fam_count(PAGES[i].fam) < 2u)
                continue;
            enter(i);
            turn(EN_SELECT, 1);
            sel_page = ui.page;
            enter(i);
            press(BTN[b]);
            snprintf(w, sizeof w, "SELECT right = the button again (%s)", PAGES[i].title);
            expect(w, ui.page == sel_page && song.g[G_BPM] == 117);
            settle();
        }
}

/* HOME has its own round (SPEC 3.5): MORPH, MORPH SET, MODE, MIXER, MORPH again (0.2.14: OVERVIEW, the old HOME screen, is no
 * longer on it); SELECT right and HOME's taps go round it the same way, SELECT left backwards */
static void sweep_home(void)
{
    static const char *const ORDER[4] = {"MORPH", "MORPH SET", "MODE", "MIXER"};
    uint8_t r[8];
    uint32_t m, k, ok, c;
    ui_power_on();
    song.g[G_BPM] = 117;
    m = home_round(r);
    c = 0;
    ok = m == 4u;
    for (k = 0; ok && k < m; k++)
        ok &= str_eq(PAGES[r[k]].title, ORDER[k]);
    expect("HOME's round: MORPH, MORPH SET, MODE, MIXER (0.2.14: no OVERVIEW)", ok);
    {   /* ENV's last two pages are the matrix pages again (MOD 1 / MOD 2): the same slots and graph as LFO's */
        uint32_t e = 0, l = 0, n = 0, i, j;
        for (i = 0; i < NPAGES; i++) {
            n += PAGES[i].fam == FAM_ENV;
            if (PAGES[i].fam == FAM_ENV && PAGES[i].graph == GR_MOD) {
                for (j = 0; j < NPAGES; j++)
                    if (PAGES[j].fam == FAM_LFO && str_eq(PAGES[j].title, PAGES[i].title)) l = j;
                e = i;
                ok = PAGES[l].graph == GR_MOD && !memcmp(PAGES[e].id, PAGES[l].id, 4) &&
                     (PAGES[e].id[1] == P_M1SRC || PAGES[e].id[1] == P_M5SRC);
                expect(PAGES[e].id[1] == P_M1SRC ? "ENV's MOD 1 edits LFO's MOD 1 slots" : "ENV's MOD 2 edits LFO's MOD 2 slots", ok);
            }
        }
        expect("ENV family: AMP ENV, AMP DEST, ENV 2, ENV 2 DEST, MOD 1, MOD 2 (the last two MOD pages)", n == 6u && str_eq(PAGES[fam_step(e, -1)].title, "MATRIX 1-4"));
    }
    {   /* the classic arrangement (SPEC 2.4): each family's pages in this order (index order, the button's round) */
        static const char *const EF[] = {"AMP ENV", "AMP DEST", "ENV 2", "ENV 2 DEST", "MATRIX 1-4", "MATRIX 5-8"};
        static const char *const LF[] = {"LFO 1", "LFO 1 TRIG", "LFO 1 DEST", "LFO 2", "LFO 2 TRIG", "LFO 2 DEST", "MATRIX 1-4", "MATRIX 5-8"};
        uint32_t i, ne = 0, nl = 0, e2 = 0, l2 = 0;
        ok = 1;
        for (i = 0; i < NPAGES; i++) {
            e2 = str_eq(PAGES[i].title, "ENV 2 DEST") ? i : e2;
            l2 = str_eq(PAGES[i].title, "LFO 2 DEST") ? i : l2;
            if (PAGES[i].fam == FAM_ENV)
                ok &= ne < 6u && str_eq(PAGES[i].title, EF[ne++]);
            if (PAGES[i].fam == FAM_LFO)
                ok &= nl < 8u && str_eq(PAGES[i].title, LF[nl++]);
        }
        ok &= ne == 6u && nl == 8u && PAGES[e2].id[0] == P_E2FLT && PAGES[e2].id[1] == P_E2SHP &&
              PAGES[l2].id[0] == P_L2PIT && PAGES[l2].id[1] == P_L2FLT && PAGES[l2].id[2] == P_L2SHP;
        expect("ENV: AMP ENV, AMP DEST, ENV 2, ENV 2 DEST, MOD 1, MOD 2; LFO: LFO 1, 1 TRIG, 1 DEST, LFO 2, 2 TRIG, 2 DEST, MOD 1, MOD 2", ok);
    }
    expect("the harness starts on HOME OPENS' page: MORPH, the round's first", where() == r[0] && at_home());
    ok = 1;
    for (k = 1; k <= m; k++) {
        turn(EN_SELECT, 1);
        ok &= where() == r[(c + k) % m];
    }
    expect("SELECT right: MORPH SET, MODE, MIXER, MORPH", ok);
    for (ok = 1, k = 1; k <= m; k++) {
        turn(EN_SELECT, -1);
        ok &= where() == r[(c + m * 2u - k) % m];
    }
    expect("SELECT left: the round backwards, back to MORPH", ok && where() == r[c]);
    for (ok = 1, k = 1; k <= m; k++) {
        press(B_HOME);
        frames(64);
        ok &= where() == r[(c + k) % m];
    }
    expect("HOME tapped: the same round (MIXER, then MORPH)", ok);
    expect("HOME: tempo untouched", song.g[G_BPM] == 117);
}

/* the screens that are not pages */
static void sweep_other(void)
{
    uint32_t r, k;
    /* the menu: SELECT moves the row */
    ui_power_on();
    song.g[G_BPM] = 117;
    ui.menu = 1; ui.menu_sel = 0; frames(48);
    for (r = 0; r < MI_COUNT; r++)
        expect("the menu has no BPM LOCK row", strcmp(MI_NAME[r], "BPM LOCK") != 0);
    r = ui.menu_sel;
    turn(EN_SELECT, 1);
    expect("menu: SELECT right moves the row down", ui.menu_sel != r && ui.menu == 1u && song.g[G_BPM] == 117);
    turn(EN_SELECT, -1);
    expect("menu: SELECT left moves it back", ui.menu_sel == r && song.g[G_BPM] == 117);
    for (k = 0; k < MI_COUNT * 2u; k++)
        turn(EN_SELECT, k & 1u ? -2 : 3);
    expect("menu: any number of turns, tempo untouched", song.g[G_BPM] == 117 && ui.menu == 1u);
    ui.menu = 2; ui.menu_scroll = 0; frames(48);
    turn(EN_SELECT, 1);
    expect("ABOUT: SELECT goes to the next section, tempo untouched, stays in ABOUT", ui.menu == 2u && ui.menu_scroll > 0u && song.g[G_BPM] == 117);
    ui.menu = 3; frames(48);
    turn(EN_SELECT, 1); turn(EN_SELECT, -1);
    expect("TEST: tempo untouched, stays in TEST", ui.menu == 3u && song.g[G_BPM] == 117);
    ui.menu = 0; frames(16);
    /* a dialog: nothing, the dialog stays */
    ui_power_on();
    song.g[G_BPM] = 117;
    enter(0);
    confirm_open(CF_CLEAR_SEQ, 0); frames(16);
    turn(EN_SELECT, 1); turn(EN_SELECT, -1);
    expect("dialog: SELECT does nothing (it stays, tempo untouched)", ui.confirm == CF_CLEAR_SEQ && song.g[G_BPM] == 117);
    ui.confirm = 0; frames(16);
    /* NAME */
    ui_power_on();
    song.g[G_BPM] = 117;
    name_open(NK_PROJ_SAVE, 0); frames(16);
    expect("NAME opened", name_on());
    turn(EN_SELECT, 1); turn(EN_SELECT, -1);
    expect("NAME: SELECT does nothing (it stays, tempo untouched)", name_on() && song.g[G_BPM] == 117);
}

/* SPEC 1.1a, the MENU: SELECT is the one way round it, continuously across the tabs, clamped at both ends; ALGORITHM and
 * PRESETS do nothing there; KNOB 1 changes the selected row's value */
static void sweep_menu(void)
{
    uint32_t r, t, i, ok;
    static const uint8_t VR[] = {MI_ANIM, MI_HOLD, MI_ACCEL, MI_TUNE, MI_USB};
    ui_power_on();
    song.g[G_BPM] = 117;
    ui.menu = 1; ui.menu_sel = 0; frames(48);
    expect("menu: opened at the very first row", ui.menu_sel == 0u && menu_tab() == 0u);
    for (ok = 1, r = 0; r + 1u < MI_COUNT; r++) {            /* forward, a detent at a time, every row and every tab */
        t = menu_tab();
        turn(EN_SELECT, 1);
        ok &= ui.menu_sel == r + 1u && ui.menu == 1u;
        if (menu_tab() != t)                                   /* the tab's end: on to the next tab's first row */
            ok &= menu_tab() == t + 1u && ui.menu_sel == mtab_first(t + 1u) && r + 1u == mtab_first(t) + mtab_rows(t);
        else                                                   /* never back to the first row of the same tab */
            ok &= ui.menu_sel != mtab_first(t);
    }
    expect("menu: SELECT right visits every row of every tab in order, on across the tab ends", ok && ui.menu_sel == MI_COUNT - 1u && menu_tab() == MTAB_COUNT - 1u);
    turn(EN_SELECT, 1); turn(EN_SELECT, 5);
    expect("menu: SELECT right at the last row stays there (no wrap)", ui.menu_sel == MI_COUNT - 1u && menu_tab() == MTAB_COUNT - 1u);
    for (ok = 1, r = MI_COUNT - 1u; r > 0; r--) {              /* and back */
        t = menu_tab();
        turn(EN_SELECT, -1);
        ok &= ui.menu_sel == r - 1u && ui.menu == 1u;
        if (menu_tab() != t)
            ok &= menu_tab() + 1u == t && r == mtab_first(t) && ui.menu_sel == mtab_first(t) - 1u;   /* the last row of the tab before */
        else
            ok &= ui.menu_sel != mtab_first(t) + mtab_rows(t) - 1u;
    }
    expect("menu: SELECT left visits every row backwards, on across the tab ends", ok && ui.menu_sel == 0u && menu_tab() == 0u);
    turn(EN_SELECT, -1); turn(EN_SELECT, -7);
    expect("menu: SELECT left at the first row stays there (no jump to the last)", ui.menu_sel == 0u && menu_tab() == 0u);
    turn(EN_SELECT, 3);
    expect("menu: 3 detents in a frame move 3 rows", ui.menu_sel == 3u);
    for (t = 0; t < MTAB_COUNT; t++) {                         /* a tab's last row, one more: the next tab, not this one's top */
        ui.menu_sel = (uint8_t)(mtab_first(t) + mtab_rows(t) - 1u);
        turn(EN_SELECT, 1);
        expect("menu: after a tab's last row SELECT goes to the next tab's first row",
               t + 1u == MTAB_COUNT ? ui.menu_sel == MI_COUNT - 1u : (ui.menu_sel == mtab_first(t + 1u) && menu_tab() == t + 1u));
    }
    /* ALGORITHM and PRESETS: nothing in the menu, wherever the selection is */
    for (i = 0; i < 4u; i++) {
        static const uint8_t AT[4] = {0, MI_HOLD, MI_TEST, MI_COUNT - 1u};
        uint32_t sel = AT[i], prefs = ui_prefs, sk = song.sel;
        ui.menu_sel = (uint8_t)sel; frames(4);
        turn(EN_ALGO, 1); turn(EN_ALGO, -1); turn(EN_ALGO, 3);
        turn(EN_PRESET, 1); turn(EN_PRESET, -1); turn(EN_PRESET, 4);
        expect("menu: ALGORITHM and PRESETS are inert (no row, no tab, no value, no part, no sound)",
               ui.menu_sel == sel && ui.menu == 1u && ui_prefs == prefs && song.sel == sk && song.g[G_BPM] == 117);
    }
    /* KNOB 1 changes the selected row's value, and only that row's */
    for (i = 0; i < sizeof VR; i++) {
        uint32_t v, w[MI_COUNT], j, other = 1;
        ui.menu_sel = VR[i]; frames(4);
        for (j = 0; j < MI_COUNT; j++)
            w[j] = menu_valued(j) ? menu_get(j) : 0u;
        v = menu_get(VR[i]);
        turn(EN_K1, 1);
        if (menu_get(VR[i]) == v)                              /* at its top: the other way */
            turn(EN_K1, -1);
        for (j = 0; j < MI_COUNT; j++)
            other &= j == VR[i] || !menu_valued(j) || menu_get(j) == w[j];
        expect(MI_NAME[VR[i]], menu_get(VR[i]) != v && other && ui.menu_sel == VR[i] && ui.menu == 1u);
    }
    ui.menu = 0; frames(16);
}

/* a layer button held: SELECT is nobody's (not the tempo, not the page) */
static void sweep_layers(void)
{
    static const uint8_t LB[] = {B_SCL, B_EDIT, B_SEQ, B_REC, B_LFO, B_ENV, B_ARP};   /* (GLO is a page, not a held layer, since step 6; FX neither,
                                                                                         * since 0.2.2; LFO ENV ARP: 0.2.9's QUICK EDIT layers) */
    uint32_t l;
    for (l = 0; l < sizeof LB; l++) {
        uint32_t pg;
        ui_power_on();
        song.g[G_BPM] = 117;
        enter(page_first(FAM_ENV));
        pg = ui.page;
        btn_down(LB[l]);
        frames(1200);                                    /* held past the threshold: the layer is open */
        turn(EN_SELECT, 1); turn(EN_SELECT, -2);
        expect("layer held: tempo untouched", song.g[G_BPM] == 117);
        expect("layer held: SELECT turns no page", ui.page == pg);
        btn_up(LB[l]);
        frames(64);
        turn(EN_SELECT, 0);
        expect("layer let go: the held-back turns do not land later", song.g[G_BPM] == 117 && ui.page == pg);
        settle();
    }
}

/* TEMPO (0.2.18): the TEMPO page KNOB 1 CLK, KNOB 2 TEMPO, KNOB 3 TAP, KNOB 4 SWG */
static void test_tempo(void)
{
    char val[8];
    const char *unit;
    int16_t b0;
    uint32_t c, glo1 = page_first(FAM_GLO), ck = 0, i, g0;
    ui_power_on();
    for (i = 0; i < NPAGES; i++)
        ck = PAGES[i].fam == FAM_GLO && str_eq(PAGES[i].title, "TEMPO") ? i : ck;
    expect("GLO's pages in order: GLO, TEMPO, SIDECHAIN, BASS MONO (0.2.19, the master-stage page), MIDI, MODULES (0.2.14), MIDI LEARN (TEMPO, 0.2.18: KNOB 1 is G_CLOCK, KNOB 2 G_BPM, KNOB 3 TAP, KNOB 4 G_SWING), no TEST page (it is MENU > SYSTEM > TEST)",
           PAGES[glo1].graph == GR_GLO && str_eq(PAGES[glo1].title, "GLO") && str_eq(PAGES[glo1 + 1u].title, "TEMPO") &&
           str_eq(PAGES[glo1 + 2u].title, "SIDECHAIN") && str_eq(PAGES[glo1 + 3u].title, MASTER_PAGE_TITLE) && PAGES[glo1 + 3u].scope == SC_GLOBAL &&
           str_eq(PAGES[glo1 + 4u].title, "MIDI") && PAGES[glo1 + 4u].graph == GR_MIDI &&
           str_eq(PAGES[glo1 + 5u].title, "MODULES") && PAGES[glo1 + 5u].scope == SC_MODS && PAGES[glo1 + 5u].graph == GR_MODS &&
           str_eq(PAGES[glo1 + 6u].title, "MIDI LEARN") &&
           ck == glo1 + 1u && PAGES[ck].id[0] == G_CLOCK && PAGES[ck].id[1] == G_BPM && PAGES[ck].id[TEMPO_TAP_SLOT] == 0xFFu && TEMPO_TAP_SLOT == 2u &&
           PAGES[ck].id[3] == G_SWING &&
           PAGES[ck].scope == SC_GLOBAL && PAGES[ck].graph == GR_TEMPO && PAGES[glo1 + 7u].fam != FAM_GLO);
    for (i = 0, g0 = 0; i < NPAGES; i++)
        g0 += PAGES[i].scope == SC_TEST || PAGES[i].graph == GR_TEST || str_eq(PAGES[i].title, "TEST");
    expect("  .. no page named TEST or of its kind is in the table at all", g0 == 0u);
    expect("the label is TEMPO", str_eq(GP[G_BPM].label, "TEMPO"));
    {
        uint32_t i, c, n = 0;
        for (i = 0; i < NPAGES; i++)
            for (c = 0; c < 4u; c++)
                n += PAGES[i].scope == SC_GLOBAL && PAGES[i].id[c] == G_BPM;
        expect("no other page holds G_BPM", n == 1u);
    }
    press(B_GLO);                                    /* the first tap of GLO */
    expect("GLO tapped from HOME: the main GLO page", ui.page == glo1);
    settle();
    enter(ck);                                       /* the clock page */
    b0 = song.g[G_BPM];
    expect("CLK AUTO (the default since 0.2.14), no clock arriving: the internal clock", song.g[G_CLOCK] == CLK_AUTO && clock_src() == 0u);
    turn(EN_K2, 3);
    expect("KNOB 2 right x3: TEMPO +3", song.g[G_BPM] == b0 + 3);
    turn(EN_K2, -5);
    expect("KNOB 2 left x5: TEMPO -2 from the start", song.g[G_BPM] == b0 - 2);
    turn(EN_K2, 1000);
    expect("KNOB 2 clamps at 240", song.g[G_BPM] == 240);
    turn(EN_K2, -1000);
    expect("KNOB 2 clamps at 40", song.g[G_BPM] == 40);
    song.g[G_BPM] = 100;
    turn(EN_K4, 1); turn(EN_K3, 0);
    expect("KNOB 3, 3 are not the tempo", song.g[G_BPM] == 100);
    param_format(&GP[G_BPM], 100, val, &unit);
    expect("INT clock: the card shows 100 BPM", str_eq(val, "100") && str_eq(unit, "BPM"));
    for (c = 1; c <= 3u; c++) {              /* USB, TRS, AUTO locked on USB */
        song.g[G_CLOCK] = (int16_t)c;
        midi_clock.lock = (uint8_t)(c == CLK_AUTO);
        song.g[G_BPM] = 100;
        turn(EN_K2, 7); turn(EN_K2, -3);
        expect("external clock: KNOB 2 is inert", song.g[G_BPM] == 100);
        param_format(&GP[G_BPM], 100, val, &unit);
        expect("external clock: the card shows EXT", str_eq(val, "EXT") && str_eq(unit, ""));
        turn(EN_SELECT, 1); turn(EN_SELECT, -1);
        expect("external clock: SELECT leaves it", song.g[G_BPM] == 100);
        draw_columns();
    }
    midi_clock.lock = 0;
    param_format(&GP[G_BPM], 100, val, &unit);
    expect("AUTO, the clock gone: numbers again", str_eq(val, "100"));
    song.g[G_CLOCK] = 0;
    param_format(&GP[G_BPM], 100, val, &unit);
    expect("INT again: numbers", str_eq(val, "100"));
    turn(EN_K2, 1);
    expect("INT again: KNOB 2 edits", song.g[G_BPM] == 101);
    /* KNOB 3 TAP: each detent, either way, one tap; 2 s of rest start over; the tempo is the mean of the last 4 taps */
    song.g[G_BPM] = 90; song.g[G_SWING] = 0;
    frames(2200);
    turn(EN_K3, 1);
    expect("TAP: the first tap alone changes nothing", song.g[G_BPM] == 90 && ui.tap_n == 1u);
    fm1_ms += 484; turn(EN_K3, -1);                  /* 500 ms after the first, turned the other way */
    expect("TAP: a second tap (the other direction) 500 ms later: 120 BPM", song.g[G_BPM] == 120 && ui.tap_n == 2u);
    fm1_ms += 484; turn(EN_K3, 1);
    fm1_ms += 484; turn(EN_K3, 1);
    expect("TAP: four taps 500 ms apart: still 120", song.g[G_BPM] == 120 && ui.tap_n == 4u);
    fm1_ms += 384; turn(EN_K3, 1);                   /* 400 ms: the last four taps are 500, 500, 400 apart */
    expect("TAP: the fifth tap, 400 ms after the fourth: the mean of the last 4 taps (3 intervals in 1400 ms = 129 BPM)", song.g[G_BPM] == 129 && ui.tap_n == 4u);
    expect("  .. the card shows the BPM while tapping", tempo_tapping() == 4u);
    draw_columns();
    fm1_ms += 2100;
    expect("  .. 2 s later the card is TAP again", tempo_tapping() == 0u);
    b0 = song.g[G_BPM];
    turn(EN_K3, 1);
    expect("TAP: after the 2 s a new run starts, one tap changes nothing", song.g[G_BPM] == b0 && ui.tap_n == 1u);
    fm1_ms += 584; turn(EN_K3, 1);
    expect("  .. the next 600 ms later: 100 BPM", song.g[G_BPM] == 100 && ui.tap_n == 2u);
    turn(EN_K3, 40);
    expect("  .. a fast twist is one tap per frame, not one per detent", ui.tap_n == 3u);
    song.g[G_BPM] = 100; frames(2200);
    song.g[G_CLOCK] = 1;
    ui.msg_t = 0;
    turn(EN_K3, 1); fm1_ms += 484; turn(EN_K3, 1);
    draw_columns();
    expect("TAP with an external clock: the TEMPO and TAP cards are not drawn; the knobs do nothing and say nothing", song.g[G_BPM] == 100 && !ui.msg_t &&
           ui.col[1][0] == 0x7f && ui.col[2][0] == 0x7f && ui.col[0][0] != 0x7f);
    song.g[G_CLOCK] = 0;
    g0 = (uint32_t)song.g[G_SWING];
    turn(EN_K4, 4);
    expect("TEMPO KNOB 4 is SWING", (uint32_t)song.g[G_SWING] != g0);
    /* KNOB 1 CLK (moved here from the MIDI page): INT / USB / TRS, a detent a step, clamped; the tempo and the MIDI settings untouched */
    song.g[G_BPM] = 100; song.g[G_ROUTE] = 0; song.g[G_CLOCK] = 0;
    turn(EN_K1, 1);
    expect("TEMPO KNOB 1 CLK right: USB", song.g[G_CLOCK] == 1 && msg_is("CLOCK USB"));
    turn(EN_K1, 1);
    expect("  .. right again: TRS", song.g[G_CLOCK] == 2);
    turn(EN_K1, 3);
    expect("  .. clamps at TRS", song.g[G_CLOCK] == 2);
    draw_columns();
    turn(EN_K1, -1);
    expect("  .. left: USB", song.g[G_CLOCK] == 1);
    turn(EN_K1, -4);
    expect("  .. left: INT (clamped), the tempo and MIDI IN untouched", song.g[G_CLOCK] == 0 && song.g[G_ROUTE] == 0 && song.g[G_BPM] == 100);
    /* the keys play notes on the TEMPO page (the page is not a layer) */
    frames(48);
    expect("TEMPO: the keys are notes (no layer, no kb_lock)", kb_lock == 0u && !ui.layer);
    key_down(3);
    expect("  .. a key held plays a note and is no layer key", !((kb_layer >> 3) & 1u) && kb_note[3] != KB_SILENT && gates() > 0u);
    key_up(3);
}

/* DUELL step 6: GLO's main page (the former hold-GLO layer, now what a tap opens) and the MIDI pages */
static void test_glo_page(void)
{
    uint32_t glo1 = page_first(FAM_GLO);
    int16_t lvl;
    ui_power_on();
    song.g[G_BPM] = 117;
    go_title("AMP ENV");
    frames(48);
    press(B_GLO);
    frames(48);
    expect("GLO tapped: its main page (graph GR_GLO), no layer held, the keys are the page's (kb_lock 0)",
           ui.page == glo1 && !ui.layer && kb_lock == 0u && cur_page()->graph == GR_GLO);
    trk[0].p[P_MUTE] = trk[1].p[P_MUTE] = 0;
    key_down(1);                                         /* F#3 plays a note: it was MUTE part 1 */
    frames(48);
    expect("  .. F#3 held: it plays a note (no layer key)", !((kb_layer >> 1) & 1u) && kb_note[1] != KB_SILENT && gates() > 0u);
    key_up(1);
    frames(48);
    key_down(2); frames(48); key_up(2); frames(48);
    expect("  .. F#3 and G3 changed nothing (no MUTE on keys)", trk[0].p[P_MUTE] == 0 && trk[1].p[P_MUTE] == 0);
    key_down(7); frames(48); key_up(7); frames(48);
    key_down(0); frames(48); key_up(0); frames(48);
    expect("  .. F4, F3 neither: no ALL, no TAP (the tempo untouched)", song.g[G_BPM] == 117 && ui.tap_n == 0u);
    /* KNOB 1 MUTE 1 / KNOB 2 MUTE 2: each its own part; one click either way flips the mute, once per twist */
    turn(EN_K1, 1);
    expect("KNOB 1 MUTE 1: ONE detent right mutes at once", trk[0].p[P_MUTE] == 1 && trk[1].p[P_MUTE] == 0 && !glo_sounding(0) && glo_sounding(1));
    turn(EN_K1, 1); turn(EN_K1, 1);
    expect("  .. the same twist going on: still one action", trk[0].p[P_MUTE] == 1);
    frames(600);
    turn(EN_K1, 1);
    expect("  .. another click (same way) unmutes", trk[0].p[P_MUTE] == 0 && glo_sounding(0));
    frames(600);
    turn(EN_K1, -1);
    expect("  .. a click left mutes too (either way flips)", trk[0].p[P_MUTE] == 1);
    draw_layer();
    frames(600);
    turn(EN_K1, -1);
    expect("  .. and a click left again unmutes", trk[0].p[P_MUTE] == 0);
    frames(600);
    turn(EN_K2, 1);
    expect("KNOB 2 MUTE 2: a click mutes part 2 only", trk[0].p[P_MUTE] == 0 && trk[1].p[P_MUTE] == 1 && !glo_sounding(1));
    frames(600);
    turn(EN_K2, -1);
    expect("  .. a click the other way unmutes it", trk[1].p[P_MUTE] == 0 && glo_sounding(1));
    frames(600);
    turn(EN_K2, 1);
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. a mute stays when the page is left (latched)", trk[1].p[P_MUTE] == 1 && str_eq(cur_page()->title, "TEMPO"));
    turn(EN_SELECT, -1);
    frames(600);
    turn(EN_K2, 1);
    expect("  .. and back on the page a click unmutes", trk[1].p[P_MUTE] == 0 && ui.page == glo1);
    lvl = trk[0].p[P_LEVEL];
    turn(EN_K3, 5);
    expect("KNOB 3: LEVEL 1 (part 1)", trk[0].p[P_LEVEL] != lvl);
    lvl = trk[1].p[P_LEVEL];
    turn(EN_K4, -5);
    expect("KNOB 4: LEVEL 2 (part 2)", trk[1].p[P_LEVEL] != lvl);
    expect("  .. the tempo untouched, the keys still notes", song.g[G_BPM] == 117 && kb_lock == 0u);
    draw_layer();
    frames(48);
    key_down(10);                                        /* D4 was MIDI LEARN: no more */
    frames(48); key_up(10); frames(48);
    expect("  .. D4: no MIDI LEARN here", !ui.ml.on);
    {   /* GLO held + PLAY: RESTART (kept) */
        btn_down(B_GLO);
        frame();
        song.playing = 0;
        press(B_PLAY);
        btn_up(B_GLO);
        frames(48);
        expect("  .. GLO held + PLAY still starts the transport (RESTART)", transport_req != 0u || song.playing);
        transport_req = 0; song.playing = 0;
        frames(600);                                     /* (the layer GLO's hold opened: the knobs are quiet a moment after it) */
        ui.page = (uint8_t)glo1; page_entered();
    }
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. SELECT right: the TEMPO page (keys play: not locked)", str_eq(cur_page()->title, "TEMPO") && kb_lock == 0u && song.g[G_BPM] == 117);
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. SELECT right: SIDECHAIN", str_eq(cur_page()->title, "SIDECHAIN"));
    turn(EN_K1, 3);
    expect("SIDECHAIN KNOB 1: the selected part's DUCK SRC (CH2), the value DUCK 1's KNOB 1 edits", TSEL->p[P_DKSRC] == 3);
    turn(EN_K2, 2);
    expect("  .. KNOB 2: its DUCK NOTE", TSEL->p[P_DKNOTE] == 2);
    turn(EN_K3, 5); turn(EN_K4, 5);
    expect("  .. KNOB 3, 4: nothing", TSEL->p[P_DKSRC] == 3 && song.g[G_ROUTE] == 0 && song.g[G_BPM] == 117);
    {
        uint32_t i, ok = 0;
        for (i = 0; i < NPAGES; i++)
            ok |= str_eq(PAGES[i].title, "DUCK 1/2") && PAGES[i].id[0] == P_DKSRC && PAGES[i].id[1] == P_DKRATE;
        expect("  .. DUCK 1 holds the same parameters (SRC, KNOB 2 by SRC)", ok && duck_ctx_id(TSEL) == P_DKNOTE);
    }
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. SELECT right: BASS MONO (0.2.19; one global card, K1 BMONO: hidden in DUEL, where ui_power_on leaves the mode), the keys play notes", str_eq(cur_page()->title, MASTER_PAGE_TITLE) && kb_lock == 0u && song.g[G_BPM] == 117);
    turn(EN_K1, 3); turn(EN_K2, 3); turn(EN_K4, 3);
    expect("  .. its knobs change nothing here (DUEL: no BASS MONO card; K2..K4 are empty)", song.bm == 0 && song.g[G_BPM] == 117);
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. SELECT right: the MIDI page, the keys play notes", str_eq(cur_page()->title, "MIDI") && kb_lock == 0u && song.g[G_BPM] == 117);
    turn(EN_K1, 4);
    turn(EN_K2, 5);
    expect("MIDI KNOB 1, 2: P1 CH 5, P2 CH 7 (MIDI IN untouched: OWN CH)", midi_pch[0] == 4u && midi_pch[1] == 6u && song.g[G_ROUTE] == 0);
    turn(EN_K3, 1);
    expect("  .. KNOB 3 right: MIDI IN SELECTED", song.g[G_ROUTE] == 1);
    turn(EN_K3, -1);
    expect("  .. KNOB 3 left: MIDI IN OWN CH", song.g[G_ROUTE] == 0 && song.g[G_BPM] == 117);
    song.g[G_CLOCK] = 0;
    turn(EN_K4, 3);
    expect("  .. KNOB 4: nothing (CLK is on TEMPO)", song.g[G_CLOCK] == 0 && midi_pch[0] == 4u && song.g[G_ROUTE] == 0 && song.g[G_BPM] == 117);
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. SELECT right: MODULES (0.2.14), the keys play notes, all four ON", str_eq(cur_page()->title, "MODULES") && kb_lock == 0u && mods_get() == 0u);
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. SELECT right: MIDI LEARN", str_eq(cur_page()->title, "MIDI LEARN"));
    turn(EN_K1, 1);
    expect("MIDI LEARN KNOB 1 right: ON", ui.ml.on == 1u);
    turn(EN_K1, 1);
    expect("  .. right again: stays ON", ui.ml.on == 1u);
    turn(EN_K1, -1);
    expect("  .. left: OFF", ui.ml.on == 0u);
    ml_put(0, 1u << 9 | 1u);                             /* one CC learned (code 1, CC 1) */
    expect("  .. a CC is learned", ml_count() == 1u);
    turn(EN_K2, -1);
    expect("  .. KNOB 2 left: nothing", ml_count() == 1u);
    turn(EN_K2, 1);
    expect("  .. KNOB 2 right: CLEAR, every learned CC gone", ml_count() == 0u && msg_is("LEARN CLEARED"));
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. SELECT right: round to the main GLO page (no TEST page any more); the keys still play notes", ui.page == glo1 && kb_lock == 0u && song.g[G_BPM] == 117);
    ui.menu = 1;
    frames(48);
    expect("  .. the menu over it: the keys play", kb_lock == 0u);
    ui.menu = 0;
    frames(48);
}

/* DUELL 0.1: TEST (was INFO) and ABOUT open with KNOB 1 turned right as well as OCT+; OCT- closes them (on TEST KNOB 1 is MORPH, both ways; in ABOUT, 0.2.9,
 * KNOB 1 scrolls and SELECT goes between its sections); CALIBRATION (a setup) still takes OCT+ only */
static void sweep_readonly(void)
{
    ui_power_on();
    ui.menu = 1; ui.menu_sel = MI_TEST; frames(8);
    expect("the item is called TEST", str_eq(MI_NAME[MI_TEST], "TEST") && MI_TAB[MI_TEST] == MTAB_SYSTEM);
    turn(EN_K1, 1);
    expect("TEST: KNOB 1 right opens it", ui.menu == 3u);
    turn(EN_K1, 2);
    expect("  .. KNOB 1 right again: stays (it is MORPH there)", ui.menu == 3u);
    turn(EN_K1, -1);
    expect("  .. KNOB 1 left: stays too (MORPH goes both ways); OCT- is the way back", ui.menu == 3u);
    press(B_OCTDN);
    expect("  .. OCT-: back to the tabs, TEST still the row", ui.menu == 1u && ui.menu_sel == MI_TEST);
    press(B_OCTUP);
    expect("  .. OCT+ still opens it", ui.menu == 3u);
    press(B_OCTDN);
    expect("  .. OCT- closes it", ui.menu == 1u);
    ui.menu_sel = MI_ABOUT; frames(4);
    turn(EN_K1, 1);
    expect("ABOUT: KNOB 1 right opens it", ui.menu == 2u && ui.menu_scroll == 0u);
    {   /* 0.2.9 (the owner): KNOB 1 scrolls down and up, SELECT right / left the next / previous section, PRESETS and KNOB 2..4 nothing */
        uint32_t s1, s2, s3, s4, max = (uint32_t)menu_scroll_max(), ok = 1;
        turn(EN_K1, 2);
        s1 = ui.menu_scroll;
        turn(EN_K1, -1);
        ok &= ui.menu == 2u && s1 == 36u && ui.menu_scroll == 18u;
        turn(EN_K1, -5);
        ok &= ui.menu == 2u && ui.menu_scroll == 0u;     /* (at the top: the animation; KNOB 1 left stays in ABOUT) */
        turn(EN_K1, 2000);
        ok &= ui.menu_scroll == max;                      /* (bounded) */
        expect("  .. KNOB 1 scrolls: right down 18 px a detent, left up, bounded at the top (the animation) and the end; it never leaves ABOUT", ok);
        ui.menu_scroll = 0;
        turn(EN_SELECT, 1); s1 = ui.menu_scroll;
        turn(EN_SELECT, 1); s2 = ui.menu_scroll;
        turn(EN_SELECT, 1); s3 = ui.menu_scroll;
        turn(EN_SELECT, 1); s4 = ui.menu_scroll;
        ok = s1 == 1u && (int32_t)s2 == (about_sec[0] < (int32_t)max ? about_sec[0] : (int32_t)max) &&
             (int32_t)s3 == (about_sec[1] < (int32_t)max ? about_sec[1] : (int32_t)max) && s4 == s3 && s3 > s2;
        turn(EN_SELECT, -1);
        ok &= ui.menu_scroll == s2;
        turn(EN_SELECT, -3);
        ok &= ui.menu_scroll == 0u && ui.menu == 2u;
        expect("  .. SELECT right: the information, CREDITS, LICENSES (then stays); left back to the animation, no wrap", ok);
        ui.menu_scroll = s2;
        turn(EN_PRESET, 3); turn(EN_K2, 2); turn(EN_K3, -2); turn(EN_K4, 1); turn(EN_ALGO, 1);
        expect("  .. PRESETS, ALGORITHM, KNOB 2..4: nothing (no scroll, no way back)", ui.menu == 2u && ui.menu_scroll == s2);
    }
    press(B_OCTDN);
    expect("  .. OCT-: back to the tabs", ui.menu == 1u && ui.menu_sel == MI_ABOUT);
    press(B_OCTUP);
    expect("  .. OCT+ still opens it", ui.menu == 2u && ui.menu_scroll == 0u);
    press(B_OCTDN);
    ui.menu_sel = MI_PANEL; frames(4);
    turn(EN_K1, 1);
    expect("BUTTON MAP (was CALIBRATION): KNOB 1 right does not start it (OCT+ only)", ui.menu == 1u && ui.menu_sel == MI_PANEL);
    ui.menu = 0; frames(8);
}

/* DUELL 0.1: MENU > CONTROL > HOME OPENS: the page HOME opens from another page, at power-on and when the menu closes.
 * 0.2.14: MORPH, MODE, MIXER (OVERVIEW is gone with its page; a stored 3, OVERVIEW up to 0.2.13, reads as MORPH) */
static void sweep_homepg(void)
{
    static const char *const T[3] = {"MORPH", "MODE", "MIXER"};
    uint32_t v, ok;
    expect("HOME OPENS: three values, MORPH MODE MIXER", HP_COUNT == 3 && menu_n(MI_HOMEPG) == 3u && str_eq(menu_vname(MI_HOMEPG, 2), "MIXER"));
    for (v = 0; v < HP_COUNT; v++) {
        ui_power_on();
        menu_put(MI_HOMEPG, v);
        go_main(); frames(32);
        ok = str_eq(cur_page()->title, T[v]);
        go_title("AMP ENV"); frames(16);
        press(B_HOME); frames(32);
        ok &= str_eq(cur_page()->title, T[v]);
        hold(B_HOME); frames(16);                         /* the menu, then HOME closes it: the same page */
        press(B_HOME); frames(32);
        ok &= !ui.menu && str_eq(cur_page()->title, T[v]);
        expect(T[v], ok);
    }
    ui_power_on();
    ui_home_pg = 3;                                       /* OVERVIEW stored by 0.2.13: MORPH, never the old page */
    go_main(); frames(16);
    ok = str_eq(cur_page()->title, "MORPH") && menu_get(MI_HOMEPG) == 0u && ui_home_pg == 3u;
    go_title("AMP ENV"); frames(16);
    press(B_HOME); frames(16);
    expect("HOME OPENS stored OVERVIEW (0.2.13): MORPH, the byte kept as stored", ok && str_eq(cur_page()->title, "MORPH"));
    ui_home_pg = 77;
    expect("  an unknown stored code reads as MORPH", menu_get(MI_HOMEPG) == 0u);
    ui_power_on();
}

/* 0.2.9 (the owner: "hide the step strip except on sequencer pages"): the footer's step bars only on SEQ's pages (STEP, AUTOMATION) and the SEQ TOOLS
 * layer; elsewhere the footer is not redrawn for a changed step or the playhead */
static void sweep_footstrip(void)
{
    uint32_t pg, ok = 1, seq = 0;
    ui_power_on();
    for (pg = 0; pg < NPAGES; pg++) {
        ui.page = (uint8_t)pg;
        ok &= foot_steps() == (PAGES[pg].fam == FAM_SEQ);
        seq += foot_steps();
    }
    go_main_to(HP_MORPH);
    ok &= !foot_steps();
    ui.layer = LAYER_SEQ;
    ok &= foot_steps() != 0;
    ui.layer = 0;
    expect("footer: the step strip on SEQ's pages only (STEP, AUTOMATION) and SEQ TOOLS; not on HOME, not on any other page", ok && seq == 2u);
    {   /* 0.2.9: the ARP pages show it too, while the part's arp MODE is SEQUENCER (AM_SEQ); the playhead is the arp's own step */
        uint32_t arp = 0, none = 1, five = 1;
        TSEL->p[P_AMODE] = AM_SEQ;
        TSEL->p[P_SLEN] = 16;
        for (pg = 0; pg < NPAGES; pg++) {
            ui.page = (uint8_t)pg;
            ok &= foot_steps() == (PAGES[pg].fam == FAM_SEQ || PAGES[pg].fam == FAM_ARP);
            arp += PAGES[pg].fam == FAM_ARP && foot_steps();
        }
        go_main_to(HP_MORPH);
        ok &= !foot_steps();
        TSEL->p[P_AMODE] = AM_UP;
        for (pg = 0; pg < NPAGES; pg++) {
            ui.page = (uint8_t)pg;
            ok &= foot_steps() == (PAGES[pg].fam == FAM_SEQ);
        }
        go_title("ARP 1/3");
        TSEL->p[P_AMODE] = AM_SEQ;
        TSEL->nheld = 0; TSEL->arp_idx = 0xFFFFFFFFu; song.playing = 0; TSEL->seq_idx = 9;
        none &= foot_playhead(TSEL) == 0xFFu;                          /* no key held: no playhead (the sequencer's seq_idx is not it) */
        TSEL->nheld = 1; TSEL->arp_idx = 21;
        five &= foot_playhead(TSEL) == 5u;                              /* 21 % 16: the arp's own step, with PLAY stopped */
        song.playing = 1;
        five &= foot_playhead(TSEL) == 5u;                              /* (seq_idx 9 is not what the arp plays) */
        go_title("STEP");
        none &= foot_playhead(TSEL) == 9u;                              /* SEQ's pages: the sequencer's step, as before */
        song.playing = 0;
        none &= foot_playhead(TSEL) == 0xFFu;
        TSEL->nheld = 0; TSEL->p[P_AMODE] = AM_OFF;
        expect("footer: the ARP pages show the strip only with MODE SEQUENCER, and mark the arp's own step (arp_idx % LEN)", ok && arp == 3u && none && five);
    }
    go_title("AMP ENV");
    frame();
    {
        uint32_t s0 = ui.foot_sig;
        TSEL->step[3].time = TSEL->step[3].time == ST_REST ? ST_NOTE : ST_REST;   /* (a step changed) */
        ui.force = 0;
        draw_foot();
        expect("  AMP ENV: a changed step does not redraw the footer (its signature has no steps there)", ui.foot_sig == s0);
        go_title("STEP");
        frame();
        s0 = ui.foot_sig;
        TSEL->step[3].time = TSEL->step[3].time == ST_REST ? ST_NOTE : ST_REST;
        ui.force = 0;
        draw_foot();
        expect("  STEP: a changed step redraws it", ui.foot_sig != s0);
    }
}

/* 0.2.11: REC as HOLD acts on every part the keys play. M01-like LAYER: part 1 arp UP, part 2 arp off; DUEL (SPLIT): the selected part only */
static uint32_t sr_gated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].gate != 0;
    return n;
}
static void sr_keys(uint32_t mask)
{
    fm1_in.notes = mask;
    keyboard_block();
}
static void sweep_rec_hold_layer(void)
{
    uint32_t i;
    ui_power_on();
    song.rec = 0;
    go_title("AMP ENV");
    duell_mode_force(DM_LAYER); ui_mode_set = ui.mode_seen = DM_LAYER;
    trk[0].p[P_AMODE] = AM_UP; trk[1].p[P_AMODE] = AM_OFF;
    trk[0].p[P_AHOLD] = trk[1].p[P_AHOLD] = 0;
    song.sel = 0;
    press(B_REC);
    frames(600);
    expect("LAYER, REC: HOLD ARP on part 1 (arp on) and HOLD ALL on part 2 (arp off), one message", trk[0].p[P_AHOLD] == 1 && trk[1].p[P_AHOLD] == 2 && msg_is("HOLD ON") && song.rec == 0u && hold_any());
    kb_prev = 0;
    sr_keys((1u << 0) | (1u << 4) | (1u << 7));
    sr_keys(0);
    for (i = 0; i < 4; i++)
        events_block(CTL);
    expect("  .. chord down and up: part 2 still sounds all 3 notes, part 1 holds it for its arp", sr_gated(&trk[1]) == 3 && trk[1].nheld == 3 && trk[0].nheld == 3);
    arp_tick(&trk[0], div_samples((uint32_t)trk[0].p[P_ARATE]));
    expect("  .. part 1 arpeggiates the latched chord", trk[0].arp_note != 0u && sr_gated(&trk[0]) >= 1u);
    press(B_REC);
    frames(600);
    for (i = 0; i < 4; i++)
        events_block(CTL);
    expect("  .. REC again: HOLD OFF, both parts release", trk[0].p[P_AHOLD] == 0 && trk[1].p[P_AHOLD] == 0 && msg_is("HOLD OFF") && !hold_any() && trk[0].nheld == 0 && trk[1].nheld == 0 && sr_gated(&trk[1]) == 0);
    trk[1].p[P_AHOLD] = 2;                              /* mixed: any on -> all off (not inverted) */
    press(B_REC);
    frames(600);
    expect("  .. one part on, one off: REC turns both OFF", trk[0].p[P_AHOLD] == 0 && trk[1].p[P_AHOLD] == 0 && msg_is("HOLD OFF"));
    duell_mode_force(DM_SPLIT); ui_mode_set = ui.mode_seen = DM_SPLIT;
    press(B_REC);
    frames(600);
    expect("DUEL: REC acts on the selected part only", trk[0].p[P_AHOLD] == 1 && trk[1].p[P_AHOLD] == 0);
    press(B_REC);
    frames(600);
    song.sel = 1;
    press(B_REC);
    frames(600);
    expect("  .. part 2 selected: part 2 only (arp off: ALL)", trk[1].p[P_AHOLD] == 2 && trk[0].p[P_AHOLD] == 0);
    press(B_REC);
    frames(600);
    song.sel = 0;
}

/* 0.2.9: REC is the HOLD switch on every page but the sequencer's (arp on: OFF <-> ARP; arp off: OFF <-> ALL); on SEQ's pages (STEP, AUTOMATION) it records as ever */
static void sweep_rec_hold(void)
{
    ui_power_on();
    song.rec = 0;
    go_title("AMP ENV");
    TSEL->p[P_AMODE] = AM_OFF;
    TSEL->p[P_AHOLD] = 0;
    press(B_REC);
    frames(600);
    expect("REC, arp off, an ordinary page: HOLD switch OFF -> ALL, nothing armed", rec_is_hold() && TSEL->p[P_AHOLD] == 2 && msg_is("HOLD ON") && song.rec == 0u);
    press(B_REC);
    frames(600);
    expect("  .. ALL -> OFF", TSEL->p[P_AHOLD] == 0 && msg_is("HOLD OFF"));
    song.rec = 1u << song.sel;
    press(B_REC);
    frames(600);
    expect("  .. an armed recording stays armed while REC switches HOLD", song.rec == (1u << song.sel) && TSEL->p[P_AHOLD] == 2);
    press(B_REC);
    frames(600);
    song.rec = 0; song.playing = 0; transport_req = 0;
    TSEL->p[P_AHOLD] = 0;
    TSEL->p[P_AMODE] = AM_UP;
    press(B_REC);
    frames(600);
    expect("REC, arp on: HOLD ARP, the message says it, no recording armed", TSEL->p[P_AHOLD] == 1 && msg_is("HOLD ON") && song.rec == 0u && rec_is_hold());
    press(B_REC);
    frames(600);
    expect("  .. again: HOLD OFF", TSEL->p[P_AHOLD] == 0 && msg_is("HOLD OFF"));
    TSEL->p[P_AHOLD] = 2;
    press(B_REC);
    frames(600);
    expect("  .. HOLD ALL with the arp on: off", TSEL->p[P_AHOLD] == 0);
    go_title("STEP");
    press(B_REC);
    frames(600);
    expect("  .. the sequencer's page (STEP), arp on: REC records, HOLD untouched", song.rec != 0u && TSEL->p[P_AHOLD] == 0 && !rec_is_hold());
    press(B_REC);
    frames(600);
    song.rec = 0; song.playing = 0; transport_req = 0;
    TSEL->p[P_AMODE] = AM_OFF;
    ui.perf = 1;
    expect("PERFORM, arp off: REC is the HOLD switch", rec_is_hold());
    TSEL->p[P_AHOLD] = 0;
    press(B_REC);
    frames(600);
    expect("  .. OFF -> ALL", TSEL->p[P_AHOLD] == 2 && msg_is("HOLD ON") && song.rec == 0u);
    press(B_REC);
    frames(600);
    expect("  .. ALL -> OFF", TSEL->p[P_AHOLD] == 0 && msg_is("HOLD OFF"));
    ui.perf = 0;
    {   /* 0.2.9: in PERFORM SELECT is BALANCE (no page to turn to), PRESETS the sound */
        int8_t b0 = mix_bal;
        uint8_t pg = ui.page;
        ui.perf = 1;
        mix_bal = 0;
        turn(EN_SELECT, 3);
        expect("PERFORM: SELECT turns BALANCE (+3), the page stays", mix_bal == 3 && ui.page == pg && ui.perf);
        ui.perf = 0;
        mix_bal = b0;
    }
    ui_power_on();
    {   /* VOICE 2/2 (0.2.19): the glide, SPREAD (step-lockable) and the sound's TRIM last (a patch setting: no step lock); PAN (a mixer setting) and MUTE are on no VOICE page */
        uint32_t p, vp = 0, mute = 0, ok = 0;
        for (p = 0; p < NPAGES; p++) {
            vp += str_eq(PAGES[p].title, "VOICE 2/2");
            mute += PAGES[p].fam == FAM_EDIT && (PAGES[p].id[0] == P_MUTE || PAGES[p].id[1] == P_MUTE || PAGES[p].id[2] == P_MUTE || PAGES[p].id[3] == P_MUTE);
            if (str_eq(PAGES[p].title, "VOICE 2/2")) {
                go_title("VOICE 2/2");
                ok = PAGES[p].id[3] == P_TRIM && lock_page_ok(p) && ui.plk_src == p + 1u && lock_id(2) == P_SPRD && lock_id(3) == 0xFFu;
            }
        }
        for (p = 0; p < NPAGES; p++)
            mute += PAGES[p].fam == FAM_EDIT && (PAGES[p].id[0] == P_PAN || PAGES[p].id[1] == P_PAN || PAGES[p].id[2] == P_PAN || PAGES[p].id[3] == P_PAN);
        expect("VOICE: two pages, TRIM on VOICE 2/2 K4 (no step lock), SPRD step-lockable (lock_id), PAN and MUTE on no EDIT page", vp == 1u && mute == 0u && ok);
    }
}


/* DUELL 0.2.9, the QUICK EDIT layers (SPEC 1.2 "Quick edit layers"): EDIT, LFO, ENV, ARP held past HOLD open their layer over the page shown,
 * the selected part's four (ly_page); a tap opens the page as before (two quick taps: the next page, no lock, for LFO ENV ARP); the keys
 * play notes in all four (none is the layer's, a key is no combo; 0.2.11: QUICK FILTER has no INIT key and no engine actions); the knobs edit the ids
 * with motion; QUICK GLO (0.2.11, hold GLO: the GLO page's MUTE / LEVEL cards over any page, the keys play); in a quick layer the header lights the
 * selected part alone (GLO's and SCL's: both); every layer's header is "QUICK <PAGE>" with the bolt, in the accent, in M */
static int head_has(uint16_t c)                      /* a pixel of colour c in the header's rows */
{
    uint32_t k;
    uint16_t w = (uint16_t)((c >> 8) | (c << 8));
    for (k = 0; k < 240u * 24u; k++)
        if (host_screen[k] == w)
            return 1;
    return 0;
}
static void sweep_quick(void)
{
    static const uint8_t QB[4] = {B_EDIT, B_LFO, B_ENV, B_ARP}, QL[4] = {LAYER_EDIT, LAYER_LFO, LAYER_ENV, LAYER_ARP};
    static const uint8_t QF[4] = {FAM_EDIT, FAM_LFO, FAM_ENV, FAM_ARP};
    static const char *const QH[4] = {"QUICK MULTI", "QUICK LFO", "QUICK ENV", "QUICK ARP"};
    static const uint8_t QID[4][4] = {{P_FREEA, P_FREEB, P_FREEC, P_REL}, {P_LRATE, P_LD_FLT, P_L2RATE, P_L2FLT},
                                      {P_ATK, P_REL, P_E2FLT, P_E2DEC}, {P_AMODE, P_ARATE, P_AGATE, P_AOCT}};
    uint32_t q, k, l;
    char w[96];
    for (q = 0; q < 4u; q++) {
        const page_t *qp;
        uint32_t pg, first;
        ui_power_on();
        set_engine_of(TSEL, 2u);                         /* PHASE: the multimode filter (ufilt) */
        go_title("MIXER");
        pg = ui.page;
        /* a tap: the family's page, as before; the second tap soon after: its next page (no lock) */
        press(QB[q]);
        first = ui.page;
        snprintf(w, sizeof w, "%s tap: its page when let go, no layer", QH[q]);
        expect(w, cur_page()->fam == QF[q] && !ui.layer && !ui.lock);
        if (q) {
            frames(96);                                  /* (within the 300 ms of a double tap) */
            press(QB[q]);
            snprintf(w, sizeof w, "  %s: a second quick tap, the next page as before (no lock)", QH[q]);
            expect(w, ui.page != first && cur_page()->fam == QF[q] && !ui.lock && !ui.layer);
        }
        settle(); frames(400);
        /* held past HOLD: the layer over the page, the page stays */
        go_title("MIXER");
        btn_down(QB[q]);
        frames(240);
        expect("  held 0.24 s: no layer yet", !ui.layer);
        frames(400);
        snprintf(w, sizeof w, "  %s held past HOLD: its layer, the page under it unchanged", QH[q]);
        expect(w, ui.layer == QL[q] && ui.page == pg);
        snprintf(w, sizeof w, "  header \"%s\"", QH[q]);
        expect(w, str_eq(layer_head(), QH[q]));
        frame();
        expect("  .. drawn in the accent (bolt and word)", head_has(vt(T_ACCENT, T_BG)));
        qp = ly_page(QL[q]);
        for (k = 0; k < 4u; k++) {
            snprintf(w, sizeof w, "  %s KNOB %u: id %u", QH[q], k + 1u, QID[q][k]);
            expect(w, qp->id[k] == QID[q][k]);
        }
        btn_up(QB[q]); frames(64);
        expect("  let go: no page, no layer", !ui.layer && ui.page == pg);
        settle(); frames(400);
        /* a knob with the button held: the layer at once (a combo), the id edited and recorded as a knob move; let go: no tap */
        {
            uint32_t id = QID[q][1], mc;
            int16_t v0;
            go_title("MIXER");
            song.rec = 1u << song.sel; song.playing = 1;
            mc = motion_count(TSEL);
            v0 = TSEL->p[id];
            btn_down(QB[q]); frame();
            turn(EN_K2, -3);
            turn(EN_K2, 5);
            snprintf(w, sizeof w, "  %s + KNOB 2: the layer at once, its id turned (and recorded)", QH[q]);
            expect(w, ui.layer == QL[q] && TSEL->p[id] != v0 && (!motion_param(id) || motion_count(TSEL) > mc));
            btn_up(QB[q]); frames(64);
            expect("  .. let go: no page (a combo)", ui.page == pg);
            song.rec = 0; song.playing = 0; transport_req = 0;
            settle(); frames(400);
        }
        /* the keys: notes in all four (no combo: a quick tap is still a tap) */
        {
            ui_power_on();
            usb.config = 1;
            go_title("MIXER");
            btn_down(QB[q]); key_down(white(3));
            frame();
            snprintf(w, sizeof w, "  %s held + a key: the note plays, the key is not the layer's", QH[q]);
            expect(w, gates() > 0 && !((kb_layer >> white(3)) & 1u) && !ui.layer);
            frames(400);
            expect("  .. held on: the layer opens over the note", ui.layer == QL[q] && gates() > 0 && !(key_leds(0) == 0u));
            key_up(white(3)); btn_up(QB[q]); frames(64);
            settle(); frames(400);
            btn_down(QB[q]); key_down(white(5)); frame(); key_up(white(5)); frame(); btn_up(QB[q]); frame();
            snprintf(w, sizeof w, "  %s + a quick key, let go soon: still a tap (its page)", QH[q]);
            expect(w, cur_page()->fam == QF[q]);
            usb.config = 0;
        }
    }
    /* QUICK FILTER: F3 plays, it does not INIT (0.2.11, owner: playing an F erased the sound), and no black key does an engine action */
    ui_power_on();
    usb.config = 1;
    go_title("MIXER");
    {
        int16_t e0 = TSEL->p[P_E0];
        uint32_t kk, ok = 1;
        btn_down(B_EDIT);
        for (kk = 0; kk < 27u; kk++) {
            key_down(kk); frame();
            ok &= gates() > 0 && !ui.confirm && !((kb_layer >> kk) & 1u);
            key_up(kk); frame(); frames(8);
        }
        btn_up(B_EDIT); frames(64);
        expect("QUICK FILTER (EDIT held): all 27 keys play notes, none opens INIT's dialog or is the layer's", ok && TSEL->p[P_E0] == e0);
    }
    /* QUICK GLO (0.2.11): hold GLO over the PERFORM page, the page stays under it and comes back; a tap is GLO's page as before */
    ui_power_on();
    usb.config = 1;
    perf_enter(); frames(48);
    {
        uint8_t pg = ui.page, pm = ui.perf;
        int16_t l0 = trk[0].p[P_LEVEL], l1 = trk[1].p[P_LEVEL];
        btn_down(B_GLO); frames(240);
        expect("QUICK GLO: held 0.24 s, no layer yet", !ui.layer);
        frames(400);
        expect("QUICK GLO: held past HOLD, the layer over PERFORM", ui.layer == LAYER_GLO && ui.perf && ui.page == pg);
        expect("  .. header \"QUICK GLO\"", str_eq(layer_head(), "QUICK GLO"));
        frame();
        expect("  .. drawn in the accent (bolt and word)", head_has(vt(T_ACCENT, T_BG)));
        expect("  .. the header lights both parts (K1 K3 part 1, K2 K4 part 2)", badge_lit() == 3u);
        turn(EN_K3, 5); turn(EN_K4, -5);
        expect("  .. K3 LEVEL 1, K4 LEVEL 2", trk[0].p[P_LEVEL] != l0 && trk[1].p[P_LEVEL] != l1);
        turn(EN_K1, 1);
        expect("  .. K1 MUTE 1", trk[0].p[P_MUTE] == 1);
        frames(600);
        turn(EN_K1, -1);
        expect("  .. a twist back unmutes", trk[0].p[P_MUTE] == 0);
        key_down(white(3)); frame();
        expect("  .. the keys are not the layer's", !((kb_layer >> white(3)) & 1u) && ui.layer == LAYER_GLO);
        key_up(white(3)); frame();
        btn_up(B_GLO); frames(64);
        expect("  .. let go: closed, PERFORM and its page back, no GLO page", !ui.layer && ui.perf == pm && ui.page == pg &&
               cur_page()->graph != GR_GLO);
        settle(); frames(400);
    }
    perf_leave(); frames(48);
    usb.config = 0;
    ui_power_on();
    go_title("MIXER");
    press(B_GLO);
    expect("GLO tap: still the GLO page (MUTE / LEVEL, then its sub-pages)", cur_page()->graph == GR_GLO && !ui.layer);
    press(B_GLO);
    expect("  .. a second tap: the next GLO page", cur_page()->graph != GR_GLO && cur_page()->fam == FAM_GLO);
    /* the header in a quick layer: the selected part alone, even with ALGORITHM at BOTH on PERFORM / MORPH */
    ui_power_on();
    morph.target = MT_BOTH; song.sel = 1;
    perf_enter(); frames(48);
    expect("PERFORM, ALGORITHM BOTH: both badges", badge_lit() == 3u);
    for (q = 0; q < 4u; q++) {
        btn_down(QB[q]); frames(640);
        snprintf(w, sizeof w, "  %s open over PERFORM (BOTH): the badge of the selected part alone", QH[q]);
        expect(w, ui.layer == QL[q] && badge_lit() == 2u);
        btn_up(QB[q]); frames(64); settle(); frames(400);
    }
    btn_down(B_GLO); frames(640);
    expect("  QUICK GLO over PERFORM (BOTH): both badges (its cards are per part)", ui.layer == LAYER_GLO && badge_lit() == 3u);
    btn_up(B_GLO); frames(64);
    perf_leave(); frames(48);
    morph.target = MT_SELECTED; song.sel = 0;
    /* OCT- in QUICK FILTER puts its four back; in QUICK LFO it is the octave */
    ui_power_on();
    set_engine_of(TSEL, 2u);
    go_title("MIXER");
    {
        int16_t c0 = TSEL->p[P_FREEA];
        btn_down(B_EDIT); frames(640);
        turn(EN_K1, -4);
        k = TSEL->p[P_FREEA] != c0;
        press(B_OCTDN);
        expect("QUICK FILTER: OCT- puts CUT back, no octave", k && TSEL->p[P_FREEA] == c0 && song.octave == 0 && msg_is("VALUES PUT BACK"));
        btn_up(B_EDIT); frames(64);
    }
    settle(); frames(400);
    btn_down(B_LFO); frames(640);
    press(B_OCTUP);
    expect("QUICK LFO: OCT+ is the octave (no put back)", ui.layer == LAYER_LFO && song.octave != 0);
    btn_up(B_LFO); frames(64);
    song.octave = 0;
    /* 0.2.14 (owner): QUICK FILTER is the MULTIMODE filter's CUT RES MODE and REL on every engine that can be picked (ANALOG TRIO NOISE
     * too: their own filters stay on their EDIT pages); a synced LFO's SYNC card */
    ui_power_on();
    {
        uint32_t ok = 1, n = 0;
        for (k = 0; k < NENGINES; k++) {
            const page_t *qp;
            if (!eng_ok(k))
                continue;
            set_engine_of(TSEL, k);
            qp = ly_page(LAYER_EDIT);
            n++;
            ok &= qp->id[0] == P_FREEA && qp->id[1] == P_FREEB && qp->id[2] == P_FREEC && qp->id[3] == P_REL && ENGINES[k]->ufilt;
        }
        expect("QUICK FILTER: CUT RES MODE (ids 45..47) + REL on all ten engines, one mapping", ok && n == 10u);
        set_engine_of(TSEL, 0u);                         /* ANALOG: K3 is the multimode MODE, its TYPE untouched */
        {
            int16_t ft = TSEL->p[P_FTYPE], m0 = TSEL->p[P_FREEC];
            go_title("MIXER");
            btn_down(B_EDIT); frames(640);
            turn(EN_K3, 6);
            btn_up(B_EDIT); frames(64);
            expect("  ANALOG: QUICK FILTER K3 turns the multimode MODE, not the VCF's TYPE", TSEL->p[P_FREEC] != m0 && TSEL->p[P_FTYPE] == ft);
            settle(); frames(400);
        }
    }
    TSEL->p[P_LSYNC] = 5;
    expect("QUICK LFO: LFO 1 synced, KNOB 1 is its SYNC; LFO 2 free, KNOB 3 RATE2", ly_page(LAYER_LFO)->id[0] == P_LSYNC &&
           ly_page(LAYER_LFO)->id[2] == P_L2RATE);
    TSEL->p[P_LSYNC] = 0; TSEL->p[P_L2SYNC] = 3;
    expect("  LFO 2 synced: KNOB 3 its SYNC2, KNOB 1 RATE", ly_page(LAYER_LFO)->id[0] == P_LRATE && ly_page(LAYER_LFO)->id[2] == P_L2SYNC);
    /* every quick layer's header fits in M with the bolt (and the lock) */
    for (l = LAYER_SCL; l < LAYER_N; l++) {
        int32_t b[4];
        icon_ink(12, ICON_X_QUICK, b);
        snprintf(w, sizeof w, "header \"%s\": QUICK, fits in M with the bolt and the lock", LAYERS[l].head);
        expect(w, !strncmp(LAYERS[l].head, "QUICK ", 6) && text_w(&AF_M, LAYERS[l].head) + b[2] - b[0] + KH_GAP + 15 <= HEAD_MSG_W);
    }
}

/* DUELL 0.2.14, HOME > MIXER: both parts side by side (ui_layer.c mixer_panel, GLO's layout and meters), the selected part's cell outlined
 * 2 px in its colour, the other 1 px RAISE; the cards edit the selected part (K1 LEVEL, K2 PAN, K3 SPACE, K4 MUTE) and ALGORITHM moves
 * the selection and the outline; nothing is drawn again while nothing changes (the panel's signature, the meters' own strip) */
static uint16_t mx_px(int32_t x, int32_t y) { return swap16(host_screen[(uint32_t)y * 240u + (uint32_t)x]); }
static void sweep_mixer(void)
{
    uint32_t k, ok = 1;
    int16_t l0, l1;
    ui_power_on();
    trk[0].peak = trk[1].peak = 0;
    go_title("MIXER");
    frames(8);
    expect("MIXER: the page is HOME's GR_TRK, cards MUTE PAN SPACE LEVEL", cur_page()->graph == GR_TRK && cur_page()->scope == SC_TRK);
    for (k = 0; k < 2u; k++) {                           /* the outline: 2 px in the part's colour on the selected cell, RAISE on the other */
        int32_t x = GC_X(k), y = Y_GRAPH + MC_Y + MC_H / 2;
        uint16_t c0 = mx_px(x, y), c1 = mx_px(x + 1, y);
        ok &= k == song.sel ? c0 == T_PTHEME(k) && c1 == T_PTHEME(k) : c0 == T_RAISE && c1 == T_SURF;
    }
    expect("MIXER: two cells side by side, part 1's (selected) outlined 2 px in its colour, part 2's 1 px RAISE", ok && song.sel == 0u);
    l0 = trk[0].p[P_LEVEL]; l1 = trk[1].p[P_LEVEL];
    turn(EN_K4, -3); frame();
    expect("MIXER: K4 turns the selected part's LEVEL only", trk[0].p[P_LEVEL] != l0 && trk[1].p[P_LEVEL] == l1);
    turn(EN_ALGO, 1); frames(4);
    ok = song.sel == 1u && mx_px(GC_X(1), Y_GRAPH + MC_Y + MC_H / 2) == T_PTHEME(1) && mx_px(GC_X(1) + 1, Y_GRAPH + MC_Y + MC_H / 2) == T_PTHEME(1) &&
         mx_px(GC_X(0), Y_GRAPH + MC_Y + MC_H / 2) == T_RAISE;
    expect("MIXER: ALGORITHM selects part 2, its cell gets the outline", ok);
    l0 = trk[0].p[P_LEVEL]; l1 = trk[1].p[P_LEVEL];
    turn(EN_K1, 2); turn(EN_K4, 1); frame();
    expect("  .. K1 LEVEL and K4 MUTE now part 2's", trk[1].p[P_LEVEL] != l1 && trk[0].p[P_LEVEL] == l0 && trk[1].p[P_MUTE] && !trk[0].p[P_MUTE]);
    frames(600);                                         /* (past the knob's accent) */
    trk[0].peak = trk[1].peak = 0;
    frames(80);                                          /* (the meters fall to rest) */
    host_blit_rows = 0;
    for (k = 0; k < 8u; k++) { trk[0].peak = trk[1].peak = 0; frame(); }
    expect("MIXER: nothing changes, nothing is drawn (8 frames, no blit)", host_blit_rows == 0u);
    trk[0].p[P_REV] = (int16_t)(trk[0].p[P_REV] == 64 ? 65 : 64);
    host_blit_rows = 0; frame();
    expect("  .. part 1's REV send changed (not the selected part's): the panel is drawn again", host_blit_rows >= (uint32_t)H_GRAPH);
    turn(EN_ALGO, -1); frames(4);
}

/* ALGORITHM (0.2.9, owner 2026-10-14): on the MORPH page (HOME 1/5) PART 1, BOTH, PART 2 from left to right, clamped, BOTH the
 * morph's target (both badges); on every other page PART 1 / PART 2 only, BOTH never set there and kept for MORPH / PERFORM, the
 * badge the selected part's alone (both only on a GLOBAL page); OVERVIEW too */
static void sweep_algo(void)
{
    uint32_t i, ok = 1, ok2 = 1, mp = NPAGES;
    for (i = 0; i < NPAGES; i++) {
        if (!page_visible(i) || PAGES[i].graph == GR_PERF)
            continue;
        if (PAGES[i].graph == GR_MORPH) {
            mp = i;
            continue;
        }
        morph.target = MT_SELECTED;
        song.sel = 0;
        enter(i);
        turn(EN_ALGO, 1); turn(EN_ALGO, 1);
        ok &= song.sel == 1u && morph.target == MT_SELECTED;
        turn(EN_ALGO, -1);
        ok &= song.sel == 0u && morph.target == MT_SELECTED;
        morph.target = MT_BOTH;                         /* (BOTH left on the MORPH page) */
        turn(EN_ALGO, 1);
        frame();
        ok2 &= song.sel == 1u && morph.target == MT_BOTH && badge_lit() == (page_global(&PAGES[i]) ? 3u : 2u);
        turn(EN_ALGO, -1);
        ok2 &= song.sel == 0u && morph.target == MT_BOTH;
        if (!ok || !ok2) {
            printf("select: ALGORITHM on page %u (%s)\n", (unsigned)i, PAGES[i].title);
            break;
        }
    }
    expect("ALGORITHM on every page but MORPH: PART 1 / PART 2 only, clamped, never BOTH", ok);
    expect("  .. BOTH (set on MORPH) kept, the page's badge the selected part's alone (a GLOBAL page: both)", ok2);
    morph.target = MT_SELECTED;
    song.sel = 0;
    enter(mp);
    ok = mp < NPAGES && badge_lit() == 1u;
    turn(EN_ALGO, -1);
    ok &= song.sel == 0u && morph.target == MT_SELECTED;
    turn(EN_ALGO, 1);
    ok &= song.sel == 0u && morph.target == MT_BOTH && badge_lit() == 3u;
    turn(EN_ALGO, 1);
    ok &= song.sel == 1u && morph.target == MT_SELECTED && badge_lit() == 2u;
    turn(EN_ALGO, 1);
    ok &= song.sel == 1u && morph.target == MT_SELECTED;
    turn(EN_ALGO, -1);
    ok &= song.sel == 1u && morph.target == MT_BOTH && badge_lit() == 3u;
    expect("MORPH page: ALGORITHM PART 1 | BOTH | PART 2, clamped; BOTH keeps the selected part, both badges", ok);
    morph.target = MT_SELECTED;
    song.sel = 0;
    frames(48);
}

/* DUELL 0.2.10: PLAY's two LEDs (stopped: green; playing or counting in: red) and REC's (outside the sequencer: HOLD, lit when on, the idle level
 * when off; on the sequencer's pages: armed). The armed mark is in the footer on every page; the header has none */
static void sweep_leds_play_rec(void)
{
    uint8_t qp, qr;
    ui_power_on();
    settings_leds = LEDS_DIM;
    ui_leds();
    memset(led_pos, 0xFF, sizeof led_pos);              /* (the host's stub key map has none: PLAY and REC in column 0) */
    led_pos[panel.btn[B_PLAY]] = 0u << 3 | 3u; led_pos[panel.btn[B_REC]] = 0u << 3 | 4u;
    song.rec = 0; song.playing = 0;
    go_title("AMP ENV");
    TSEL->p[P_AMODE] = AM_OFF; TSEL->p[P_AHOLD] = 0;
    ui_leds();
    qp = led_pos[panel.btn[B_PLAY]]; qr = led_pos[panel.btn[B_REC]];
    expect("PLAY stopped: green lit, its red LED not", (fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u) & 1u) && !(fm1_led[qp >> 3] >> (qp & 7u) & 1u));
    expect("REC outside the sequencer, HOLD off: not lit", !(fm1_led[qr >> 3] >> (qr & 7u) & 1u));
    TSEL->p[P_AHOLD] = 2;
    ui_leds();
    expect("  .. HOLD on: lit (the idle level when off is 1 frame in 4)", fm1_led[qr >> 3] >> (qr & 7u) & 1u);
    song.rec = 1u << song.sel;
    TSEL->p[P_AHOLD] = 0;
    ui_leds();
    expect("  .. armed recording is not shown on REC's LED here (HOLD is)", !(fm1_led[qr >> 3] >> (qr & 7u) & 1u));
    song.playing = 1;
    ui_leds();
    expect("PLAY running: red lit, green dark", !(fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u) & 1u) && (fm1_led[qp >> 3] >> (qp & 7u) & 1u));
    song.playing = 0; song.rec = 0;
    open_family(FAM_SEQ);
    song.rec = 1u << song.sel;
    ui_leds();
    expect("STEP / AUTOMATION: REC's LED shows the armed recording (lit)", !rec_is_hold() && (fm1_led[qr >> 3] >> (qr & 7u) & 1u));
    song.rec = 0;
    ui_power_on();
}

/* 0.2.18, the page-order audit (docs/PAGE_ORDER_AUDIT.md), the UI-only reorders: top-down (a mode or type first, an amount next, a level or mix last). The stored ids never move */
static const page_t *pg_titled(const char *t)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (str_eq(PAGES[i].title, t))
            return &PAGES[i];
    return &PAGES[0];
}
static void test_page_order(void)
{
    const page_t *a1 = pg_titled("ARP 1/3"), *a2 = pg_titled("ARP 2/3"), *a3 = pg_titled("ARP 3/3"), *v1 = pg_titled("VOICE 1/2"), *v2 = pg_titled("VOICE 2/2");
    const page_t *d2 = pg_titled("DUCK 2/2"), *mf = pg_titled("MULTI FILTER");
    uint32_t k, ok = 1;
    expect("ARP: 1/3 MODE HOLD ORD OCT, 2/3 RATE GATE SWG PROB, 3/3 TRGT",
           a1->id[0] == P_AMODE && a1->id[1] == P_AHOLD && a1->id[2] == P_AORDER && a1->id[3] == P_AOCT &&
           a2->id[0] == P_ARATE && a2->id[1] == P_AGATE && a2->id[2] == P_ASWING && a2->id[3] == P_APROB && a3->id[0] == P_ATGT);
    expect("VOICE: 1/2 VCE PRIO ALLOC DTUNE, 2/2 GLMOD GLD SPRD TRIM",
           v1->id[0] == P_VOICE && v1->id[1] == P_PRIO && v1->id[2] == P_ALLOC && v1->id[3] == P_DETUNE &&
           v2->id[0] == P_GLMODE && v2->id[1] == P_GLIDE && v2->id[2] == P_SPRD && v2->id[3] == P_TRIM);
    expect("DUCK 2/2: CURVE ATK RLS", d2->id[0] == P_DKCRV && d2->id[1] == P_DKATK && d2->id[2] == P_DKRLS);
    expect("MULTI FILTER: CUT RES MODE SLOPE", mf->id[0] == P_FREEA && mf->id[1] == P_FREEB && mf->id[2] == P_FREEC && mf->id[3] == P_FREED);
    /* every id of a reordered page is still on exactly that page set (nothing lost or doubled) */
    {
        static const uint8_t ARPIDS[9] = {P_AMODE, P_AHOLD, P_AORDER, P_AOCT, P_ARATE, P_AGATE, P_ASWING, P_APROB, P_ATGT};
        uint32_t n[9] = {0}, i, c;
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == FAM_ARP)
                for (c = 0; c < 4u; c++)
                    for (k = 0; k < 9u; k++)
                        n[k] += PAGES[i].id[c] == ARPIDS[k];
        for (k = 0; k < 9u; k++)
            ok &= n[k] == 1u;
        expect("ARP: each of the nine cards is on exactly one page", ok);
    }
    /* FM6: SLOT first on EDIT 1 (stored ids unchanged: the card shows P_E7, K2 ALG is P_E0 ...) */
    ui_power_on();
    set_engine_of(TSEL, ENGI_FM6);
    {
        const page_t *e1 = pg_titled("EDIT 1"), *e2 = pg_titled("EDIT 2");
        int16_t *vp;
        static const uint8_t WANT1[4] = {7, 0, 1, 2}, WANT2[4] = {3, 4, 5, 6};
        ok = 1;
        for (k = 0; k < 4u; k++) {
            ok &= page_desc(e1, k, &vp) && vp == &TSEL->p[P_E0 + WANT1[k]];
            ok &= page_desc(e2, k, &vp) && vp == &TSEL->p[P_E0 + WANT2[k]];
        }
        page_desc(e1, 0, &vp);
        expect("FM6 EDIT 1: SLOT ALG FB MLVL, EDIT 2: MRAT MEG VMOD DTUN (the cards move, the stored E slots do not)", ok && !strcmp(page_desc(e1, 0, &vp)->label, "SLOT") &&
               !strcmp(page_desc(e2, 0, &vp)->label, "MRAT"));
        set_engine_of(TSEL, 0u);
        ok = page_desc(e1, 0, &vp) && vp == &TSEL->p[P_E0] && page_desc(e2, 3, &vp) && vp == &TSEL->p[P_E7];
        expect("every other engine: EDIT 1 / EDIT 2 in the engine's own order, E1..E8", ok);
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    sweep_homepg();
    sweep_readonly();
    sweep_pages();
    sweep_buttons();
    sweep_home();
    sweep_other();
    sweep_menu();
    sweep_layers();
    test_tempo();
    test_glo_page();
    sweep_footstrip();
    sweep_rec_hold();
    sweep_rec_hold_layer();
    sweep_quick();
    sweep_mixer();
    test_page_order();
    sweep_algo();
    sweep_leds_play_rec();
    printf("select: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
