/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL (SPEC 1.1 / 1.2 / 1.3 / 5): the SELECT knob is the select knob and never the tempo.
 *   SELECT     turned left / right on every screen (every page the UI has, for every engine the selected part can have,
 *              stopped and playing; HOME; the menu; ABOUT and INFO; a dialog; NAME; with a layer button held): the tempo
 *              (song.g[G_BPM]) never changes. On a page of a family with sub-pages it steps to the previous / next
 *              sub-page (wrapping), the one the page button pressed again goes to; with one page it stays; HOME has
 *              its own round (HOME, its pages); in the menu it moves the row (continuously across the tabs, clamped, sweep_menu); in a dialog, NAME and a layer: nothing.
 *   TEMPO      GLO page 1 (the first tap of GLO) KNOB 1 is TEMPO (G_BPM) and sets it; with an external clock (CLK USB /
 *              TRS) it is inert and shows EXT.
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
/* HOME's round (SPEC 3.5): HOME's pages in table order (MORPH, MORPH SET, MODE, MIXER), then OVERVIEW (ui.home, r[m - 1] = 0xFF) */
static uint32_t home_round(uint8_t *r)
{
    uint32_t i, m = 0;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_TRK && page_visible(i))
            r[m++] = (uint8_t)i;
    r[m++] = 0xFFu;
    return m;
}
static uint32_t where(void) { return ui.home ? 0xFFu : ui.page; }
static void enter(uint32_t i)
{
    ui.menu = 0; ui.confirm = 0; ui.layer = 0;
    ui.home = 0; ui.page = (uint8_t)i; page_entered();
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
                expect(w, song.g[G_BPM] == 117 && !ui.home && !ui.menu);
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

/* HOME has its own round (SPEC 3.5): MORPH, MORPH SET, MODE, MIXER, OVERVIEW (the old HOME screen), MORPH again; SELECT
 * right and HOME's taps go round it the same way, SELECT left backwards */
static void sweep_home(void)
{
    static const char *const ORDER[4] = {"MORPH", "MORPH SET", "MODE", "MIXER"};
    uint8_t r[8];
    uint32_t m, k, ok, c;
    ui_power_on();
    song.g[G_BPM] = 117;
    m = home_round(r);
    c = m - 1u;
    ok = m == 5u && r[c] == 0xFFu;
    for (k = 0; ok && k < c; k++)
        ok &= str_eq(PAGES[r[k]].title, ORDER[k]);
    expect("HOME's round: MORPH, MORPH SET, MODE, MIXER, OVERVIEW", ok);
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
        expect("ENV family: AMP ENV, AMP DEST, ENV 2, ENV 2 DEST, MOD 1, MOD 2 (the last two MOD pages)", n == 6u && str_eq(PAGES[fam_step(e, -1)].title, "MOD 1"));
    }
    {   /* the classic arrangement (SPEC 2.4): each family's pages in this order (index order, the button's round) */
        static const char *const EF[] = {"AMP ENV", "AMP DEST", "ENV 2", "ENV 2 DEST", "MOD 1", "MOD 2"};
        static const char *const LF[] = {"LFO 1", "LFO 1 SYNC", "LFO 1 DEST", "LFO 2", "LFO 2 SYNC", "LFO 2 DEST", "MOD 1", "MOD 2"};
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
        expect("ENV: AMP ENV, AMP DEST, ENV 2, ENV 2 DEST, MOD 1, MOD 2; LFO: LFO 1, 1 SYNC, 1 DEST, LFO 2, 2 SYNC, 2 DEST, MOD 1, MOD 2", ok);
    }
    expect("the harness starts on OVERVIEW (ui.home)", ui.home);
    for (ok = 1, k = 1; k <= m; k++) {
        turn(EN_SELECT, 1);
        ok &= where() == r[(c + k) % m];
    }
    expect("SELECT right: MORPH, MORPH SET, MODE, MIXER, OVERVIEW, MORPH", ok);
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
    expect("HOME tapped: the same round", ok);
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
    expect("ABOUT: SELECT scrolls, tempo untouched, stays in ABOUT", ui.menu == 2u && song.g[G_BPM] == 117);
    ui.menu = 3; frames(48);
    turn(EN_SELECT, 1); turn(EN_SELECT, -1);
    expect("INFO: tempo untouched, stays in INFO", ui.menu == 3u && song.g[G_BPM] == 117);
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
        static const uint8_t AT[4] = {0, MI_HOLD, MI_INFO, MI_COUNT - 1u};
        uint32_t sel = AT[i], prefs = ui_prefs, sk = song.sel;
        ui.menu_sel = (uint8_t)sel; frames(4);
        turn(EN_ALGO, 1); turn(EN_ALGO, -1); turn(EN_ALGO, 3);
        turn(EN_PRESET, 1); turn(EN_PRESET, -1); turn(EN_PRESET, 4);
        expect("menu: ALGORITHM and PRESETS are inert (no row, no tab, no value, no part, no sound)",
               ui.menu_sel == sel && ui.menu == 1u && ui_prefs == prefs && song.sel == sk && song.g[G_BPM] == 117);
    }
    /* KNOB 1 changes the selected row's value, and only that row's */
    for (i = 0; i < sizeof VR; i++) {
        uint32_t v, w[MI_VALUES], j, other = 1;
        ui.menu_sel = VR[i]; frames(4);
        for (j = 0; j < MI_VALUES; j++)
            w[j] = menu_get(j);
        v = menu_get(VR[i]);
        turn(EN_K1, 1);
        if (menu_get(VR[i]) == v)                              /* at its top: the other way */
            turn(EN_K1, -1);
        for (j = 0; j < MI_VALUES; j++)
            other &= j == VR[i] || menu_get(j) == w[j];
        expect(MI_NAME[VR[i]], menu_get(VR[i]) != v && other && ui.menu_sel == VR[i] && ui.menu == 1u);
    }
    ui.menu = 0; frames(16);
}

/* a layer button held: SELECT is nobody's (not the tempo, not the page) */
static void sweep_layers(void)
{
    static const uint8_t LB[] = {B_FX, B_SCL, B_EDIT, B_SEQ, B_REC};   /* (GLO is a page, not a held layer, since step 6) */
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
        expect("layer held: SELECT turns no page", ui.page == pg && !ui.home);
        btn_up(LB[l]);
        frames(64);
        turn(EN_SELECT, 0);
        expect("layer let go: the held-back turns do not land later", song.g[G_BPM] == 117 && ui.page == pg);
        settle();
    }
}

/* TEMPO: GLO page 1 KNOB 1 */
static void test_tempo(void)
{
    char val[8];
    const char *unit;
    int16_t b0;
    uint32_t c, glo1 = page_first(FAM_GLO), ck = 0, i;
    ui_power_on();
    for (i = 0; i < NPAGES; i++)
        ck = PAGES[i].fam == FAM_GLO && str_eq(PAGES[i].title, "CLOCK") ? i : ck;
    expect("GLO's first page is the main GLO page (a tap of GLO opens it), then the MIDI pages CHANNELS, SIDECHAIN, MIDI LEARN; the clock page after them: KNOB 1 is G_BPM",
           PAGES[glo1].graph == GR_GLO && str_eq(PAGES[glo1].title, "GLO") && str_eq(PAGES[glo1 + 1u].title, "CHANNELS") &&
           str_eq(PAGES[glo1 + 2u].title, "SIDECHAIN") && str_eq(PAGES[glo1 + 3u].title, "MIDI LEARN") && ck == glo1 + 4u &&
           PAGES[ck].id[0] == G_BPM && PAGES[ck].scope == SC_GLOBAL && str_eq(PAGES[glo1 + 5u].title, "TEST") && PAGES[glo1 + 6u].fam != FAM_GLO);
    expect("the label is TEMPO", str_eq(GP[G_BPM].label, "TEMPO"));
    {
        uint32_t i, c, n = 0;
        for (i = 0; i < NPAGES; i++)
            for (c = 0; c < 4u; c++)
                n += PAGES[i].scope == SC_GLOBAL && PAGES[i].id[c] == G_BPM;
        expect("no other page holds G_BPM", n == 1u);
    }
    press(B_GLO);                                    /* the first tap of GLO */
    expect("GLO tapped from HOME: the main GLO page", !ui.home && ui.page == glo1);
    settle();
    enter(ck);                                       /* the clock page */
    b0 = song.g[G_BPM];
    expect("INT clock", song.g[G_CLOCK] == 0);
    turn(EN_K1, 3);
    expect("KNOB 1 right x3: TEMPO +3", song.g[G_BPM] == b0 + 3);
    turn(EN_K1, -5);
    expect("KNOB 1 left x5: TEMPO -2 from the start", song.g[G_BPM] == b0 - 2);
    turn(EN_K1, 1000);
    expect("KNOB 1 clamps at 240", song.g[G_BPM] == 240);
    turn(EN_K1, -1000);
    expect("KNOB 1 clamps at 40", song.g[G_BPM] == 40);
    song.g[G_BPM] = 100;
    turn(EN_K2, 1); turn(EN_K3, 0);
    expect("KNOB 2 is not the tempo", song.g[G_BPM] == 100);
    param_format(&GP[G_BPM], 100, val, &unit);
    expect("INT clock: the card shows 100 BPM", str_eq(val, "100") && str_eq(unit, "BPM"));
    for (c = 1; c <= 2u; c++) {              /* USB, TRS */
        song.g[G_CLOCK] = (int16_t)c;
        song.g[G_BPM] = 100;
        turn(EN_K1, 7); turn(EN_K1, -3);
        expect("external clock: KNOB 1 is inert", song.g[G_BPM] == 100);
        param_format(&GP[G_BPM], 100, val, &unit);
        expect("external clock: the card shows EXT", str_eq(val, "EXT") && str_eq(unit, ""));
        turn(EN_SELECT, 1); turn(EN_SELECT, -1);
        expect("external clock: SELECT leaves it", song.g[G_BPM] == 100);
        draw_columns();
    }
    song.g[G_CLOCK] = 0;
    param_format(&GP[G_BPM], 100, val, &unit);
    expect("INT again: numbers", str_eq(val, "100"));
    turn(EN_K1, 1);
    expect("INT again: KNOB 1 edits", song.g[G_BPM] == 101);
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
    expect("GLO tapped: its main page (graph GR_GLO), no layer held, the keys are the page's (kb_lock)",
           !ui.home && ui.page == glo1 && !ui.layer && kb_lock == 1u);
    key_down(1);                                         /* F#3: MUTE part 1 */
    frames(48);
    expect("  .. F#3 held: it plays no note (a layer key)", (kb_layer >> 1) & 1u && kb_note[1] == KB_SILENT);
    key_up(1);
    frames(48);
    expect("  .. F#3: MUTE part 1 (latched)", trk[0].p[P_MUTE] == 1);
    key_down(1); frames(48); key_up(1); frames(48);
    expect("  .. F#3 again: unmuted", trk[0].p[P_MUTE] == 0);
    key_down(2);                                         /* G3: SOLO part 2 while held */
    frames(48);
    expect("  .. G3 held: SOLO part 2", perf_solo == 2u);
    key_up(2);
    frames(48);
    expect("  .. G3 let go: no solo", perf_solo == 0u);
    lvl = trk[0].p[P_LEVEL];
    turn(EN_K1, 5);
    expect("  .. KNOB 1: part 1's LEVEL", trk[0].p[P_LEVEL] != lvl);
    lvl = trk[1].p[P_LEVEL];
    turn(EN_K2, -5);
    expect("  .. KNOB 2: part 2's LEVEL", trk[1].p[P_LEVEL] != lvl);
    turn(EN_K3, 5); turn(EN_K4, 5);
    expect("  .. KNOB 3, 4: nothing, the tempo untouched", song.g[G_BPM] == 117);
    key_down(10);                                        /* D4 was MIDI LEARN: no more */
    frames(48); key_up(10); frames(48);
    expect("  .. D4: no MIDI LEARN here any more", !ui.ml.on);
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. SELECT right: the CHANNELS page, the keys play notes again", str_eq(cur_page()->title, "CHANNELS") && kb_lock == 0u && song.g[G_BPM] == 117);
    turn(EN_K1, 4);
    turn(EN_K2, 5);
    expect("CHANNELS KNOB 1, 2: P1 CH 5, P2 CH 7 (MIDI IN untouched: OWN CH)", midi_pch[0] == 4u && midi_pch[1] == 6u && song.g[G_ROUTE] == 0);
    turn(EN_K3, 1);
    expect("  .. KNOB 3 right: MIDI IN SELECTED", song.g[G_ROUTE] == 1);
    turn(EN_K3, -1);
    expect("  .. KNOB 3 left: MIDI IN OWN CH", song.g[G_ROUTE] == 0 && song.g[G_BPM] == 117);
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
            ok |= str_eq(PAGES[i].title, "DUCK 1") && PAGES[i].id[0] == P_DKSRC && PAGES[i].id[1] == P_DKRATE;
        expect("  .. DUCK 1 holds the same parameters (SRC, KNOB 2 by SRC)", ok && duck_ctx_id(TSEL) == P_DKNOTE);
    }
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
    expect("  .. SELECT right: the clock page; then the TEST page (the last), then round, the main GLO page", str_eq(cur_page()->title, "CLOCK"));
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. SELECT right: TEST, the last page of GLO (no parameter; keys play: not locked)", str_eq(cur_page()->title, "TEST") && cur_page()->graph == GR_TEST &&
           cur_page()->fam == FAM_GLO && kb_lock == 0u);
    turn(EN_SELECT, 1);
    frames(48);
    expect("  .. back on GLO: its keys again", ui.page == glo1 && kb_lock == 1u && song.g[G_BPM] == 117);
    ui.menu = 1;
    frames(48);
    expect("  .. the menu over it: the keys play", kb_lock == 0u);
    ui.menu = 0;
    frames(48);
}

/* DUELL 0.1: INFO and ABOUT (read only) open with KNOB 1 turned right as well as OCT+; KNOB 1 left or OCT- close them; CALIBRATION
 * (a setup, not read only) still takes OCT+ only */
static void sweep_readonly(void)
{
    ui_power_on();
    ui.menu = 1; ui.menu_sel = MI_INFO; frames(8);
    turn(EN_K1, 1);
    expect("INFO: KNOB 1 right opens it", ui.menu == 3u);
    turn(EN_K1, 2);
    expect("  .. KNOB 1 right again: stays", ui.menu == 3u);
    turn(EN_K1, -1);
    expect("  .. KNOB 1 left: back to the tabs, INFO still the row", ui.menu == 1u && ui.menu_sel == MI_INFO);
    press(B_OCTUP);
    expect("  .. OCT+ still opens it", ui.menu == 3u);
    press(B_OCTDN);
    expect("  .. OCT- closes it", ui.menu == 1u);
    ui.menu_sel = MI_ABOUT; frames(4);
    turn(EN_K1, 1);
    expect("ABOUT: KNOB 1 right opens it", ui.menu == 2u && ui.menu_scroll == 0u);
    turn(EN_SELECT, 2);
    expect("  .. SELECT scrolls", ui.menu == 2u && ui.menu_scroll > 0u);
    turn(EN_K1, -1);
    expect("  .. KNOB 1 left: back to the tabs", ui.menu == 1u && ui.menu_sel == MI_ABOUT);
    press(B_OCTUP);
    expect("  .. OCT+ still opens it", ui.menu == 2u);
    press(B_OCTDN);
    ui.menu_sel = MI_PANEL; frames(4);
    turn(EN_K1, 1);
    expect("CALIBRATION: KNOB 1 right does not start it (OCT+ only)", ui.menu == 1u && ui.menu_sel == MI_PANEL);
    ui.menu = 0; frames(8);
}

/* DUELL 0.1: MENU > CONTROL > HOME OPENS: the page HOME opens from another page, at power-on and when the menu closes */
static void sweep_homepg(void)
{
    static const char *const T[4] = {"MORPH", "MODE", "MIXER", "OVERVIEW"};
    uint32_t v, ok;
    for (v = 0; v < HP_COUNT; v++) {
        ui_power_on();
        menu_put(MI_HOMEPG, v);
        go_main(); frames(32);
        ok = v == HP_OVERVIEW ? ui.home == 1u : (!ui.home && str_eq(cur_page()->title, T[v]));
        go_title("AMP ENV"); frames(16);
        press(B_HOME); frames(32);
        ok &= v == HP_OVERVIEW ? ui.home == 1u : (!ui.home && str_eq(cur_page()->title, T[v]));
        hold(B_HOME); frames(16);                         /* the menu, then HOME closes it: the same page */
        press(B_HOME); frames(32);
        ok &= !ui.menu && (v == HP_OVERVIEW ? ui.home == 1u : (!ui.home && str_eq(cur_page()->title, T[v])));
        expect(T[v], ok);
    }
    ui_power_on();
    menu_put(MI_HOMEPG, HP_OVERVIEW);
    go_main(); frames(16);
    press(B_HOME); frames(16);
    expect("HOME OPENS OVERVIEW: HOME on OVERVIEW goes round to MORPH, not stuck", !ui.home && str_eq(cur_page()->title, "MORPH"));
    ui_home_pg = 77;
    expect("  an unknown stored code reads as MORPH", menu_get(MI_HOMEPG) == 0u);
    ui_power_on();
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
    printf("select: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
