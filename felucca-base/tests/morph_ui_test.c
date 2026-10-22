/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL build step 4b: the MORPH's page, its MIDI CCs, the matrix source, its settings (SPEC 3.4, 3.4a, 3.5, 4.4).
 *   ROUND      MORPH, MORPH SET, MODE, MIXER (v2, SPEC 3.5; 0.2.14: no OVERVIEW): HOME's taps and SELECT; MORPH is the main page
 *   KNOBS      MORPH (v3): KNOB 1 the fader, 2 ROLL A (left) / B (right), 3 UNDO A (left) / B (right) (no KEEP), 4 AMOUNT;
 *              MORPH SET: KNOB 1 SMART, 2 ARP, 3 PITCH, 4 empty (0.2.9: TARGET is ALGORITHM's BOTH on the MORPH page). Each drives
 *              its morph call on the parts TARGET says (SELECTED: the part ALGORITHM picked; BOTH: both)
 *   GESTURE    a nudge below the detent never acts (rested, reversed); one twist past it acts exactly once, however far
 *   UNDO END   the UNDO card shows A<>B (<A / B> while a twist builds); left undoes A, right B, whatever the fader
 *   MIDI       CC 20-23, 25, 111 and 113-115 (CC 24 unassigned) through the real input path (USB packets and TRS bytes into the queue, events_block,
 *              then the main loop): each part's channel, SELECTED and BOTH, the >= 64 triggers and their re-arm, a hidden
 *              part's channel ignored, ROUT SEL, a learned CC the user's
 *   MATRIX     the MORPH source (fader / 127), no feedback (a roll never makes a route from MORPH nor changes one)
 *   LOAD       a sound load: the fader at A, End A the sound, End B a fresh roll
 *   SAVE HELD  SAVE held (any page): the SAVE OVER dialog, OCT+ saves the patch with End B (a load gives it back); a press\n *              opens the SAVE pages; a factory preset saved over: a copy shadows it in the browser, erased it comes back
 *   SETTINGS   AMOUNT, SMART, ARP, PITCH kept in the settings bytes (TARGET's bit reserved, not read or written: 0.2.9), back after a power cycle; the default writes nothing
 * Built and run by DUELL/host_tests.sh (name: morphui): the UI sources against ui_test.c's stub display and knobs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("morphui: %-100s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}

static void settle(void) { frames(600); }               /* the knobs at rest (a gesture is over) */
static uint32_t where(void) { return ui.page; }
static uint32_t page_of(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, title); i++)
        ;
    return i;
}
static void morph_page(void) { ui.menu = 0; go_title("MORPH"); frames(32); }
static uint32_t rolls(uint32_t pi, uint32_t e) { return mph[pi].n[e]; }   /* the end's entries (1: the Original only) */
static uint32_t seed_now(uint32_t pi, uint32_t e) { return mph[pi].seed[e][mph[pi].top[e]]; }
static const int8_t *endp(uint32_t pi, uint32_t e) { return mph[pi].h[e][mph[pi].top[e]]; }
static int end_is_base(uint32_t pi, uint32_t e) { return !memcmp(mph[pi].h[e][mph[pi].top[e]], mph[pi].base, P_COUNT); }
static int card_is(uint32_t c, const char *label, const char *val)   /* the card's cache key: "LABEL|VALUE|UNIT..." */
{
    char want[40];
    snprintf(want, sizeof want, "%s|%s|", label, val);
    return !strncmp(ui.col[c], want, strlen(want));
}
static void fresh(void)
{
    ui_power_on();
    frame();                                            /* (morph_settings: the defaults read) */
}

/* ------------------------------------------------------------------ ROUND --- */
static void t_round(void)
{
    static const char *const T[3] = {"MORPH", "MORPH SET", "MODE"};   /* (0.2.25 layout round: the MIXER is GLO's first page, not HOME's) */
    uint32_t k, ok = 1;
    fresh();
    go_title("AMP ENV");                                 /* (from another page HOME opens MORPH) */
    for (k = 0; k < 3u; k++) {
        press(B_HOME);
        frames(64);
        ok &= str_eq(PAGES[ui.page].title, T[k]);
    }
    press(B_HOME);
    frames(64);
    expect("HOME tapped: MORPH, MORPH SET, MODE, then MORPH again (0.2.14: no OVERVIEW; 0.2.25: no MIXER)", ok && str_eq(PAGES[ui.page].title, "MORPH"));
    press(B_HOME);
    frames(64);
    expect("  .. and MORPH SET; from another page HOME opens MORPH", str_eq(PAGES[ui.page].title, "MORPH SET"));
    go_title("AMP ENV");
    press(B_HOME);
    frames(64);
    ok = str_eq(PAGES[ui.page].title, "MORPH");
    for (k = 1; k <= 3u; k++) {
        turn(EN_SELECT, 1);
        ok &= where() == page_of(T[k % 3u]);
    }
    expect("SELECT right: the same round (MODE, then MORPH)", ok);
    turn(EN_SELECT, -1);
    expect("SELECT left from MORPH: MODE (the round backwards)", where() == page_of("MODE"));
    morph_page();
    {
        char nm[20], ct[12];
        page_parts(nm, ct);
        expect("footer title: MORPH 1/3", str_eq(nm, "MORPH") && str_eq(ct, "1/3"));
    }
}

/* ---------------------------------------------------------------- EDITABLE B (cloud job G) --- */
static uint32_t ink_cols(int32_t x0, int32_t x1)       /* header columns x0..x1 with anything not BG */
{
    uint32_t n = 0;
    int32_t x, y;
    for (x = x0; x <= x1; x++)
        for (y = 2; y < 22; y++)
            if (swap16(host_screen[(uint32_t)y * 240u + (uint32_t)x]) != T_BG) { n++; break; }
    return n;
}
static uint32_t knob_of(uint32_t id)                    /* the page that has id on one of its four cards: the page is opened, the knob (0..3) returned; 9: none */
{
    uint32_t i, k;
    for (i = 0; i < NPAGES; i++)
        for (k = 0; k < 4u; k++)
            if (PAGES[i].id[k] == id && PAGES[i].scope == SC_TRACK && PAGES[i].graph != GR_BROWSE) {
                ui.menu = 0;
                go_title(PAGES[i].title);
                frames(32);
                return k;
            }
    return 9u;
}
static void live_b(void)                                /* a morph that moves things: B rolled at AMOUNT 40, the fader at the B end */
{
    morph_set_amount(40);
    morph_roll(1);
    morph_set_fader(127);
    frames(32);
}
static void t_editb(void)
{
    int16_t pa[EXT_P_LEN];
    xb_edit_t ed[XB_MAX];
    int8_t b0[EXT_P_LEN];
    uint32_t i, k, ok, n, sl, pos, total;
    char want[24], un[40];
    fresh();
    live_b();
    memcpy(pa, trk[0].p, sizeof pa);
    morph_set_fader(126);
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 3);
    expect("EDITABLE B: the fader at 126: a knob turn edits A (the stored ATK moves), nothing is marked", k < 4u && trk[0].p[P_ATK] != pa[P_ATK] && morph_edits(0, ed) == 0u);
    trk[0].p[P_ATK] = pa[P_ATK];
    morph_set_fader(127);
    frames(32);
    memcpy(pa, trk[0].p, sizeof pa);
    turn(EN_K1 + k, 3);
    frame();
    n = morph_edits(0, ed);
    expect("  at 127 (a live morph): the same turn sets End B's ATK, A is untouched (every stored value)", n == 1u && ed[0].wid == P_ATK && !memcmp(pa, trk[0].p, sizeof pa) && morph_view(0, P_ATK) == ed[0].val);
    {
        const char *unit;
        param_format(&TP[P_ATK], (int16_t)morph_view(0, P_ATK), want, &unit);
    }
    snprintf(un, sizeof un, "ATTACK|%s|", want);
    expect("  the card shows B's value", !strncmp(ui.col[k], un, strlen(un)));
    expect("  the header letter is B in B's colour and the page is a sound page", head_ab_on() && head_ab_b() == 1u);
    {
        uint32_t cx, cy, found = 0;
        ui.force = 1; draw_head(); ui.force = 0;
        for (cy = 6; cy < 18u; cy++)
            for (cx = 34; cx < 46u; cx++)
                found |= swap16(host_screen[cy * 240u + cx]) == T_PEND(0, 1);
        expect("  .. its cushion is drawn right of the badges (x 33 .. 46) in part 1's B colour", found && ink_cols(33, 46) > 8u);
        morph_set_fader(0);
        frames(32);
        ui.force = 1; draw_head(); ui.force = 0;
        found = 0;
        for (cy = 6; cy < 18u; cy++)
            for (cx = 34; cx < 46u; cx++)
                found |= swap16(host_screen[cy * 240u + cx]) == T_PEND(0, 0);
        expect("  .. A below the B end, in A's colour", found && head_ab_b() == 0u);
        morph_page();
        ui.force = 1; draw_head(); ui.force = 0;
        expect("  .. no letter on MORPH", !head_ab_on() && !ink_cols(33, 46));
        go_title("MORPH SET");
        ui.force = 1; draw_head(); ui.force = 0;
        expect("  .. nor on MORPH SET", !head_ab_on() && !ink_cols(33, 46));
        go_title("MIXER");
        ui.force = 1; draw_head(); ui.force = 0;
        expect("  .. nor on the global pages (GLO)", !head_ab_on());
        morph_set_fader(127);
    }
    k = knob_of(P_ATK);
    ed[0].val = 0;
    turn(EN_K1 + k, -2);
    n = morph_edits(0, ed);
    expect("  turning the same value again changes it, never counts again; A still untouched", n == 1u && !memcmp(pa, trk[0].p, sizeof pa));
    /* values both ends share */
    k = knob_of(P_VOICE);
    ui.msg_t = 0;
    memcpy(pa, trk[0].p, sizeof pa);
    turn(EN_K1 + k, 1);
    expect("a value both ends share (VOICE MODE): the knob works, A changes, nothing is marked, the line says SAME ON A AND B", k < 4u && trk[0].p[P_VOICE] != pa[P_VOICE] && morph_edits(0, ed) == 1u && msg_is("SAME ON A AND B"));
    ui.msg_t = 0;
    turn(EN_K1 + k, 1);
    expect("  .. once: the next turn of it says nothing", !ui.msg_t);
    /* the 33rd value */
    {
        uint32_t w, nk = 0;
        morph_set_fader(0);
        morph_set_fader(127);
        for (w = 0; w < EXT_P_LEN && morph_edits(0, ed) < XB_MAX; w++)
            if (morph_b_mode(0, w) == 1u && w != P_SUS && w != P_ATK)
                morph_b_set(0, w, mph[0].base[w]);
        nk = knob_of(P_SUS);
        ui.msg_t = 0;
        memcpy(b0, endp(0, 1), P_COUNT);
        turn(EN_K1 + nk, 4);
        ok = morph_edits(0, ed) == XB_MAX && pin_get(morph_pins(&mph[0]), P_SUS) == 0u && !memcmp(b0, endp(0, 1), P_COUNT) && msg_is("B FULL: 32 VALUES");
        expect("the 33rd value: the knob does not move (End B and A as they were), the line says B FULL: 32 VALUES", ok);
        k = knob_of(P_ATK);
        turn(EN_K1 + k, 2);
        expect("  .. a value already set still turns", memcmp(b0, endp(0, 1), P_COUNT) != 0 && morph_edits(0, ed) == XB_MAX);
    }
    /* automation: not recorded at the B end */
    fresh();
    live_b();
    k = knob_of(P_DEC);
    song.rec = 1u;
    song.playing = 1;
    ui.msg_t = 0;
    n = motion.count;
    turn(EN_K1 + k, 3);
    ok = motion.count == n && msg_is("B: NOT RECORDED") && morph_edits(0, ed) == 1u;
    ui.msg_t = 0;
    turn(EN_K1 + k, 1);
    expect("recording armed: a knob turn at the B end is not recorded as automation, the line says so once", ok && motion.count == n && !ui.msg_t);
    song.rec = 0;
    song.playing = 0;
    /* OCT- reset at the B end */
    fresh();
    live_b();
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 6);
    memcpy(pa, trk[0].p, sizeof pa);
    ok = knob_reset(k) == 1 && morph_view(0, P_ATK) == TP[P_ATK].def && !memcmp(pa, trk[0].p, sizeof pa) && morph_edits(0, ed) == 1u;
    expect("OCT- reset at the B end: End B's value goes to the default, A is untouched", ok);
    /* a patch: edit A, edit B, save, load */
    fresh();
    pos = preset_pos(&total);
    preset_go(pos);
    frames(32);
    sl = slot_of(&trk[0]);
    live_b();
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 5);
    k = knob_of(P_SUS);
    turn(EN_K1 + k, -4);
    morph_set_fader(0);
    frames(32);
    trk[0].p[P_REL] = (int16_t)(trk[0].p[P_REL] > 60 ? 10 : 90);   /* edit A afterwards */
    hold(B_SAVE);
    press(B_OCTUP);
    n = morph_edits(0, ed);
    memcpy(b0, endp(0, 1), P_COUNT);
    memcpy(pa, trk[0].p, sizeof pa);
    ok = up_used(sl) && !strncmp(ui.msg, "SAVED", 5) && n == 2u && up_rec(sl)->xb_n == 2u;
    turn(EN_PRESET, 1);
    frames(32);
    expect("(control) another sound: no hand-set values", morph_edits(0, ed) == 0u);
    turn(EN_PRESET, -1);
    frames(32);
    expect("a patch saved with values set by hand: loading it gives A as saved, End B exactly as saved and the same two values, the fader at A",
           ok && trk[0].user == sl + 1u && mph[0].fader == 0u && !memcmp(pa, trk[0].p, sizeof pa) && !memcmp(b0, endp(0, 1), P_COUNT) && morph_edits(0, ed) == 2u &&
           pin_get(morph_pins(&mph[0]), P_ATK) && pin_get(morph_pins(&mph[0]), P_SUS));
    /* undo of loads (SAVE held), both orders */
    fresh();
    live_b();
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 4);                                 /* a B edit on sound 1 */
    memcpy(pa, trk[0].p, sizeof pa);
    turn(EN_PRESET, 1);
    frames(32);                                         /* sound 2: the undo copy is sound 1 */
    morph_set_fader(127);
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 4);                                 /* a B edit on sound 2 */
    ok = morph_edits(0, ed) == 1u;
    undo_swap();
    frames(32);
    ok &= trk[0].p[P_ATK] == pa[P_ATK] && !memcmp(pa, trk[0].p, P_COUNT * sizeof pa[0]) && morph_edits(0, ed) == 0u;
    expect("undo of a load after a B edit on the sound loaded: sound 1 comes back exactly (A as it was, nothing of B in it), no stale mark", ok);
    undo_swap();
    frames(32);
    ok = morph_edits(0, ed) == 0u && trk[0].user == 0u;
    expect("  .. and redo: sound 2 (its A as loaded; the B edits belonged to the sound that was undone)", ok);
    fresh();
    live_b();
    k = knob_of(P_ATK);
    turn(EN_K1 + k, 4);
    n = morph_view(0, P_ATK);
    memcpy(pa, trk[0].p, sizeof pa);
    morph_set_fader(0);
    turn(EN_PRESET, 1);
    frames(32);
    undo_swap();
    frames(32);
    expect("undo of a load that came after a B edit: the sound as it was, A untouched by the B edit, the morph re-based (no mark of the other sound)",
           !memcmp(pa, trk[0].p, P_COUNT * sizeof pa[0]) && morph_edits(0, ed) == 0u && trk[0].p[P_ATK] == pa[P_ATK] && (uint32_t)morph_view(0, P_ATK) != n + 1000u);
}

/* ------------------------------------------------------------------ KNOBS --- */
static void t_knobs(void)
{
    uint32_t a0, a1, s;
    char amt_txt[8];
    fresh();
    morph_page();
    turn(EN_K1, 10);
    expect("KNOB 1: the selected part's fader (10), the other part's stays (SELECTED)", mph[0].fader == 10u && mph[1].fader == 0u);
    turn(EN_K1, -3);
    expect("KNOB 1 left: the fader back (7)", mph[0].fader == 7u);
    turn(EN_K1, -100);
    expect("KNOB 1 clamps at 0 (A)", mph[0].fader == 0u);
    frame();
    expect("the MORPH card at 0 shows A", card_is(0, "MORPH", "A"));
    turn(EN_K1, 200);
    frame();
    expect("KNOB 1 clamps at 127 (B), the card shows B", mph[0].fader == 127u && card_is(0, "MORPH", "B"));
    turn(EN_K1, -127);
    a0 = mph[0].amount;
    a1 = mph[1].amount;
    turn(EN_K4, 5);
    snprintf(amt_txt, sizeof amt_txt, "%u", (unsigned)(a0 + 5u));
    expect("KNOB 4: AMOUNT of the selected part (+5), the other part's stays", mph[0].amount == a0 + 5u && mph[1].amount == a1 &&
           a0 == ds_amount(trk[0].eng_req, trk[0].preset));
    frame();
    expect("  .. the cards in v3's order: MORPH, ROLL, UNDO, AMOUNT (KNOB 4)", card_is(0, "MORPH", "A") && card_is(1, "ROLL", "A<>B") &&
           card_is(2, "UNDO", "A<>B") && card_is(3, "AMOUNT", amt_txt));
    {   /* the panel's ribbons: a new picture every 4th frame (signature), none with MENU > ANIM OFF */
        uint32_t s0 = graph_signature(), s1;
        ui.frame += 4u;
        s1 = graph_signature();
        ui_prefs |= PREF_ANIM_OFF;
        expect("the MORPH panel animates (~16 /s: a new signature every 4 frames); ANIM OFF: still",
               s0 != s1 && (ui.frame += 4u, graph_signature()) == graph_signature() && (ui.frame += 4u, s1 = graph_signature(), ui.frame += 4u, s1 == graph_signature()));
        ui_prefs &= ~PREF_ANIM_OFF;
    }
    turn(EN_K4, 200);
    expect("KNOB 4 clamps at 100 %", mph[0].amount == 100u);
    turn(EN_K4, -60);
    settle();
    turn(EN_K2, -MORPH_DET);
    s = seed_now(0, 0);
    expect("KNOB 2 left past the detent: ROLL A (end A rolled, a seed shown, B untouched: it is the load's fresh roll)",
           rolls(0, 0) == 2u && s && s == morph.seed && rolls(0, 1) == 2u && !end_is_base(0, 0) && msg_is("ROLLED A"));
    expect("  .. the other part untouched (SELECTED)", rolls(1, 0) == 1u && rolls(1, 1) == 2u);
    settle();
    turn(EN_K2, MORPH_DET);
    expect("KNOB 2 right past the detent: ROLL B", rolls(0, 1) == 3u && rolls(0, 0) == 2u && msg_is("ROLLED B"));
    settle();
    turn(EN_K2, MORPH_DET);
    expect("ROLL B again: B's history is full at MORPH_HIST = 3 entries (the oldest goes; UNDO can go back 2)", rolls(0, 1) == MORPH_HIST);
    settle();
    s = seed_now(0, 1);
    turn(EN_K3, MORPH_DET);
    expect("KNOB 3 right: UNDO B, A stays", rolls(0, 1) == 2u && seed_now(0, 1) != s &&
           rolls(0, 0) == 2u && msg_is("UNDO B"));
    settle();
    turn(EN_K3, -MORPH_DET);
    expect("KNOB 3 left: UNDO A, B stays; no KEEP", rolls(0, 0) == 1u && msg_is("UNDO A") && rolls(0, 1) == 2u);
    /* MORPH SET (0.2.9): SMART, ARP, PITCH, KNOB 4 empty; no TARGET card (ALGORITHM's BOTH on the MORPH page is the target) */
    go_title("MORPH SET");
    frames(32);
    turn(EN_K1, -1); turn(EN_K2, 1); turn(EN_K3, 1);
    frame();
    expect("KNOB 1 left: WILD; KNOB 2 right: ARP ON; KNOB 3 right: PITCH ON (cards, settings bits)",
           morph.cfg == (MC_WILD | MC_ARP | MC_PIT) && card_is(0, "SMART", "WILD") && card_is(1, "ARP", "ON") &&
           card_is(2, "PITCH", "ON") && favorites.factory[15][8] == 7u);
    {
        uint32_t c = morph.cfg, t = morph.target;
        turn(EN_K4, 1); turn(EN_K4, -1);
        frame();
        expect("  .. KNOB 4: an empty column, does nothing (no TARGET card on the page)", morph.cfg == c && morph.target == t &&
               card_is(3, "", "") && !card_is(0, "TARGET", "PART") && !card_is(0, "TARGET", "BOTH"));
    }
    turn(EN_ALGO, 1);
    expect("  .. ALGORITHM on MORPH SET: PART 2 only (no BOTH here)", song.sel == 1u && morph.target == MT_SELECTED);
    turn(EN_ALGO, -1);
    turn(EN_K1, 1); turn(EN_K2, -1); turn(EN_K3, -1);
    frame();
    expect("  .. and back: SMART ON, ARP OFF, PITCH OFF (the defaults)", !morph.cfg && !favorites.factory[15][8]);
}

/* ---------------------------------------------------------------- GESTURE --- */
static void t_gesture(void)
{
    uint32_t i;
    fresh();
    morph_page();
    turn(EN_K3, -2);
    turn(EN_K3, 2);
    expect("KNOB 3: no UNDO below the detent", rolls(0, 1) == 2u && !msg_is("NO UNDO B") && !msg_is("UNDO B") && !msg_is("UNDO A"));
    settle();
    turn(EN_K2, -1);
    expect("KNOB 2 ROLL: ONE detent left rolls A at once", rolls(0, 0) == 2u && rolls(0, 1) == 2u && msg_is("ROLLED A"));
    for (i = 0; i < 20u; i++)
        turn(EN_K2, -1);
    expect("  .. turning on without a rest (20 more detents): still one", rolls(0, 0) == 2u);
    settle();
    turn(EN_K2, -40);
    expect("one fast twist of 40 detents after a rest: exactly one more roll", rolls(0, 0) == 3u);
    settle();
    turn(EN_K2, 1);
    expect("ONE detent right rolls B at once", rolls(0, 1) == 3u && rolls(0, 0) == 3u && msg_is("ROLLED B"));
}

/* ---------------------------------------------------------------- FAR END --- */
static void t_far(void)
{
    fresh();
    morph_page();
    turn(EN_K2, -MORPH_DET); settle();
    turn(EN_K2, MORPH_DET); settle();
    turn(EN_K2, -MORPH_DET); settle();
    turn(EN_K2, MORPH_DET); settle();
    turn(EN_K1, 63);
    frame();
    frame();
    expect("UNDO card at rest: UNDO A<>B", card_is(2, "UNDO", "A<>B"));
    turn(EN_K3, -1);
    frame();
    expect("a twist left building up: <A", card_is(2, "UNDO", "<A"));
    settle();
    turn(EN_K3, 1);
    frame();
    expect("a twist right building up: B>", card_is(2, "UNDO", "B>"));
    settle();
    turn(EN_K1, 100);
    turn(EN_K3, -MORPH_DET);
    expect("fader 100: left still undoes A, B stays", rolls(0, 0) == 2u && rolls(0, 1) == MORPH_HIST && msg_is("UNDO A"));
    settle();
    turn(EN_K1, -100);
    turn(EN_K3, MORPH_DET);
    expect("fader 0: right still undoes B, A stays", rolls(0, 1) == 2u && rolls(0, 0) == 2u && msg_is("UNDO B"));
    settle();
    turn(EN_K3, MORPH_DET); settle();
    turn(EN_K3, MORPH_DET); settle();
    turn(EN_K3, MORPH_DET);
    expect("UNDO with nothing left: says so, nothing changes", rolls(0, 1) == 1u && msg_is("NO UNDO B"));
}

/* ------------------------------------------------------- SELECTED / BOTH --- */
static void t_target(void)
{
    fresh();
    morph_page();
    turn(EN_ALGO, 1);
    expect("ALGORITHM on MORPH (0.2.9: PART 1, BOTH, PART 2): right is BOTH, both badges, part 1 still selected",
           morph.target == MT_BOTH && song.sel == 0u && badge_lit() == 3u && msg_is("BOTH"));
    turn(EN_ALGO, 1);
    expect("ALGORITHM: part 2 selected", song.sel == 1u && morph.target == MT_SELECTED && badge_lit() == 2u);
    turn(EN_K1, 20);
    expect("SELECTED: KNOB 1 now moves part 2's fader only", mph[1].fader == 20u && mph[0].fader == 0u);
    turn(EN_K2, -MORPH_DET);
    expect("SELECTED: ROLL A rolls part 2 only", rolls(1, 0) == 2u && rolls(0, 0) == 1u);
    settle();
    go_title("AMP ENV"); frames(32);
    turn(EN_ALGO, -1);                                  /* (another page: PART 1 / PART 2 only) */
    morph_page();
    expect("ALGORITHM on AMP ENV: part 1, no BOTH", song.sel == 0u && morph.target == MT_SELECTED);
    turn(EN_K2, MORPH_DET);
    expect("ALGORITHM back to part 1: ROLL B rolls part 1 only", rolls(0, 1) == 3u && rolls(1, 1) == 2u);
    settle();
    turn(EN_ALGO, 1);                                   /* BOTH */
    turn(EN_K2, -MORPH_DET);
    expect("BOTH: ROLL A rolls both parts, the same seed shown", rolls(0, 0) == 2u && rolls(1, 0) == 3u &&
           seed_now(0, 0) == seed_now(1, 0) && seed_now(0, 0) == morph.seed);
    expect("  .. each part from its own Original (the ends differ)", memcmp(mph[0].h[0][mph[0].top[0]], mph[1].h[0][mph[1].top[0]], P_COUNT) != 0);
    settle();
    turn(EN_K1, 30);
    expect("BOTH: KNOB 1 moves both faders together", mph[0].fader == 30u && mph[1].fader == 30u);
    {
        uint32_t b0 = mph[0].amount;
        turn(EN_K4, -5);
        expect("BOTH: KNOB 4 sets both parts' AMOUNT", mph[0].amount == b0 - 5u && mph[1].amount == b0 - 5u);
    }
    turn(EN_K3, MORPH_DET);
    expect("BOTH: UNDO B acts on both parts", rolls(0, 1) == 2u && rolls(1, 1) == 1u);
    go_title("MORPH SET"); frames(32);
    expect("BOTH, then MORPH SET: one badge (the selected part), BOTH kept", morph.target == MT_BOTH && badge_lit() == 1u);
    morph_page();
    expect("  .. back on MORPH: both badges again", morph.target == MT_BOTH && badge_lit() == 3u);
}

/* -------------------------------------------------------------------- MIDI --- */
static void cc(uint32_t ch, uint32_t n, uint32_t v)   /* a USB-MIDI CC packet, as the USB IN endpoint queues it */
{
    midi_in_event(0x0Bu | (0xB0u | ch) << 8 | n << 16 | v << 24);
}
static void run(void) { events_block(CTL); frame(); }  /* the audio ISR's events, then the main loop */
static void t_midi(void)
{
    uint32_t s, ok;
    fresh();
    song.g[G_ROUTE] = 0;                                 /* (ROUT CH1-4) */
    cc(0, 20, 100); cc(1, 20, 30); run();
    expect("CC 20 ch 1 -> part 1's fader, ch 2 -> part 2's (SELECTED)", mph[0].fader == 100u && mph[1].fader == 30u);
    song.sel = 1;
    cc(0, 20, 5); run();
    expect("CC 20 ch 1 with part 2 selected: still part 1 (the channel's part, not the selection)", mph[0].fader == 5u &&
           mph[1].fader == 30u);
    song.sel = 0;
    cc(2, 20, 77); cc(3, 20, 77); run();
    expect("CC 20 on ch 3 / ch 4 (hidden parts): ignored", mph[0].fader == 5u && mph[1].fader == 30u);
    cc(0, 25, 127); cc(1, 25, 0); run();
    expect("CC 25 AMOUNT: ch 1 127 -> 100 %, ch 2 0 -> 0 %", mph[0].amount == 100u && mph[1].amount == 0u);
    cc(0, 25, 64); run();
    expect("CC 25 64 -> 50 %", mph[0].amount == 50u);
    cc(0, 21, 127); run();
    s = seed_now(0, 0);
    expect("CC 21 ch 1 >= 64: ROLL A on part 1 (ROLLED A)", rolls(0, 0) == 2u && s && rolls(1, 0) == 1u && msg_is("ROLLED A"));
    cc(0, 21, 100); cc(0, 21, 127); run();
    expect("CC 21 held up (100, 127 without a release): no second roll", rolls(0, 0) == 2u);
    cc(0, 21, 0); cc(0, 21, 64); run();
    expect("CC 21 released below 64, then 64: rolls again", rolls(0, 0) == 3u);
    cc(1, 22, 127); cc(1, 22, 0); run();
    expect("CC 22 ch 2: ROLL B on part 2 only", rolls(1, 1) == 3u && rolls(0, 1) == 2u);
    cc(1, 22, 127); cc(1, 22, 0); run();
    cc(1, 23, 127); cc(1, 23, 0); run();
    expect("CC 23 ch 2 >= 64: UNDO on part 2's B", rolls(1, 1) == 2u && msg_is("UNDO B"));
    cc(1, 23, 30); cc(1, 23, 100); run();
    expect("CC 23 1..63: UNDO A (nothing there), fires once (100 without a release: no second)", rolls(1, 1) == 2u &&
           msg_is("NO UNDO A"));
    cc(1, 23, 0); cc(1, 20, 0); run();
    cc(1, 24, 127); run();
    expect("CC 24 ch 2: unassigned, ignored (no KEEP any more)", rolls(1, 1) == 2u && !msg_is("KEPT >B") && !msg_is("UNDO B"));
    cc(1, 24, 10); run();
    cc(1, 111, 127); run();
    expect("CC 111 86..127: TARGET BOTH", morph.target == MT_BOTH && msg_is("BOTH"));
    cc(0, 20, 90); run();
    expect("BOTH: CC 20 on ch 1 moves both faders", mph[0].fader == 90u && mph[1].fader == 90u);
    cc(1, 20, 12); run();
    expect("BOTH: CC 20 on ch 2 moves both faders", mph[0].fader == 12u && mph[1].fader == 12u);
    cc(1, 21, 0); cc(1, 21, 127); run();
    expect("BOTH: CC 21 on ch 2 rolls A on both parts", rolls(0, 0) == MORPH_HIST && rolls(1, 0) == 2u && seed_now(0, 0) == seed_now(1, 0));
    cc(0, 25, 127); run();
    expect("BOTH: CC 25 sets both parts' AMOUNT", mph[0].amount == 100u && mph[1].amount == 100u);
    cc(0, 111, 63); run();
    ok = morph.target == MT_SELECTED && song.sel == 1u && msg_is("PART 2");
    cc(0, 111, 42); run();
    expect("CC 111 43..85: PART 2 (selected); 0..42: PART 1", ok && morph.target == MT_SELECTED && song.sel == 0u &&
           msg_is("PART 1"));
    cc(0, 113, 0); cc(0, 114, 100); cc(0, 115, 64); run();
    expect("CC 113 SMART (0 = WILD), CC 114 ARP, CC 115 PITCH (>= 64 on)", morph.cfg == (MC_WILD | MC_ARP | MC_PIT));
    cc(0, 113, 127); cc(0, 114, 0); cc(0, 115, 63); run();
    expect("  .. and back", !morph.cfg);
    {   /* TRS MIDI IN: the same path (midi_uart.c um_byte), running status */
        static const uint8_t B[] = {0xB1, 20, 44, 25, 127};
        uint32_t i;
        for (i = 0; i < sizeof B; i++)
            um_byte(B[i]);
        run();
        expect("TRS: CC 20 ch 2 (44) and, running status, CC 25 (127) -> part 2", mph[1].fader == 44u && mph[1].amount == 100u &&
               mph[0].fader == 12u);
    }
    song.g[G_ROUTE] = 1;                                 /* ROUT SEL: every channel the selected part */
    song.sel = 1;
    cc(6, 20, 101); run();
    expect("ROUT SEL: CC 20 on ch 7 -> the selected part (2)", mph[1].fader == 101u && mph[0].fader == 12u);
    song.g[G_ROUTE] = 0;
    song.sel = 0;
    cc(0, 121, 0); cc(0, 21, 127); run();
    expect("CC 121 re-arms the triggers (CC 21 held at 127 before it: a roll now)", rolls(0, 0) == MORPH_HIST);
    cc(0, 21, 0); run();
    {   /* MIDI LEARN: a CC learned is the user's (as for the standard map): CC 20 learned to part 1's LEVEL */
        ml_learn_f(20, 0, P_LEVEL, 1, 0);               /* (CC 20 is the fader's: the learn takes it only forced now) */
        cc(0, 20, 0); run();
        expect("a learned CC 20 sets what it was learned to, not the fader", trk[0].p[P_LEVEL] == 0 && mph[0].fader == 12u);
        ml_clear_all();                                   /* (the number is the fader's again) */
    }
    {   /* the queue: 16 fader values of one part in a block (0.2.11: the MIDI drain's limit a block, seq.c MIDI_DRAIN) take one slot (the last value wins) */
        uint32_t i;
        for (i = 0; i < 16u; i++)
            cc(0, 20, 24u + i);
        run();
        expect("a CC 20 stream in one block: one queue slot, the last value", mph[0].fader == 39u && !mq_lost);
    }
}

/* ------------------------------------------------------------------ MATRIX --- */
static int32_t blk[2u * CTL];
static void t_matrix(void)
{
    track_t *t = &trk[0];
    uint32_t k, ok = 1, from = 1;
    fresh();
    expect("the matrix's sources: MORPH right after LFO2 (append-only), then DUCK (the sidechain), N_MSRC 'MORPH'",
           MS_MORPH == MS_LFO2 + 1 && MS_N >= MS_MORPH + 1 && str_eq(N_MSRC[MS_MORPH], "MORPH"));
    morph_do(MQ_FADER, 1u, 127u);
    morph_do(MQ_FADER, 2u, 64u);
    mix_block(blk, CTL);                            /* (a block: morph_begin puts each part's fader for the matrix) */
    expect("MORPH source: fader / 127 (127 -> 32766, 64 -> 16512), per part, as the block has it (t->mf)",
           mod_tsrc(&trk[0], MS_MORPH, 0) == 32766 && mod_tsrc(&trk[1], MS_MORPH, 0) == 64 * 258);
    t->p[P_PAN] = 0;
    t->p[P_M1SRC] = MS_MORPH; t->p[P_M1DST] = MD_PAN; t->p[P_M1AMT] = 63;
    mod_begin(t);
    k = (uint32_t)t->p[P_PAN];
    mod_end(t);
    morph_do(MQ_FADER, 1u, 0u);
    mix_block(blk, CTL);
    mx_sl[t - trk].ok = 0;                          /* (0.2.25: a mapping probe: the fader's jump at once, not through the matrix's source slew) */
    mod_begin(t);
    ok = t->p[P_PAN] == 0;
    mod_end(t);
    expect("MORPH -> PAN +63: fader 127 pans fully right (+63), fader 0 not at all", k == 63u && ok && t->p[P_PAN] == 0);
    /* no feedback: rolls never make a route from MORPH, SMART rolls only a MORPH route's AMT (SRC DST kept) */
    t->p[P_M2SRC] = MS_LFO; t->p[P_M2DST] = MD_CUT; t->p[P_M2AMT] = 30;
    t->p[P_M3SRC] = MS_ENV; t->p[P_M3DST] = MD_SHP; t->p[P_M3AMT] = -20;
    for (k = 1, ok = 1; k <= 400u; k++) {
        int8_t o[P_COUNT], end[P_COUNT];
        uint32_t id, j;
        for (id = 0; id < P_COUNT; id++)
            o[id] = (int8_t)clamp(t->p[id], -128, 127);
        morph_roll_pure(o, end, ENGINES[eng_idx(t->eng_req)], 100, k, 0, 0);
        for (j = 0; j < NMSLOT; j++)
            ok &= end[MSLOT_ID(j)] != MS_MORPH || o[MSLOT_ID(j)] == MS_MORPH;
        ok &= end[P_M1SRC] == MS_MORPH && end[P_M1DST] == MD_PAN;   /* (SMART 2026-10-12: its AMT rolls too, SRC DST kept) */
        from &= end[P_M2SRC] == MS_LFO && end[P_M2DST] == MD_CUT && end[P_M3SRC] == MS_ENV && end[P_M3DST] == MD_SHP;   /* SMART (2026-10-12): SRC / DST kept */
        for (j = 3; j < NMSLOT; j++)
            from &= !end[MSLOT_ID(j)] == !o[MSLOT_ID(j)];   /* no route added */
    }
    expect("400 SMART rolls at 100 %: every assigned route keeps SRC / DST, none added; the MORPH route keeps SRC / DST (its AMT rolls)", ok && from);
}

/* ------------------------------------------------------------------- VIEW --- */
/* the parameter pages show the blended value (morph_view) and a mark on the moved cards */
static void t_view(void)
{
    uint32_t j, k, pg = 0, slot = 0, id = 0, ex = 0xFFFFu, i;
    int found = 0, ok;
    int32_t before, vb, pb;
    fresh();
    morph_page();
    turn(EN_K2, -MORPH_DET); settle();
    turn(EN_K2, MORPH_DET); settle();
    for (j = 0; j < mph[0].nid && !found; j++) {         /* a continuous value on a plain page, away from its range's edge */
        const param_desc_t *d;
        id = mph[0].ent[j].id;
        if ((mph[0].ent[j].kind & 0x7Fu) != MK_CONT || id >= P_COUNT)
            continue;
        d = track_desc(&trk[0], id);
        if (trk[0].p[id] <= d->min + 2 || trk[0].p[id] >= d->max - 2)
            continue;
        for (i = 0; i < NPAGES && !found; i++)
            for (k = 0; k < 4u; k++)
                if (PAGES[i].scope == SC_TRACK && PAGES[i].graph != GR_MOD && PAGES[i].id[k] == id && !found) {
                    pg = i; slot = k; found = 1;
                }
    }
    for (i = 2; i < P_COUNT && ex == 0xFFFFu; i++) {      /* an untouched one: no entry at i, i - 1 or i - 2 */
        for (j = 0, k = 0; j < mph[0].nid; j++)
            k |= mph[0].ent[j].id >= i - 2u && mph[0].ent[j].id <= i;
        if (!k)
            ex = i;
    }
    expect("a rolled continuous value on a plain page found", found);
    if (!found)
        return;
    mph[0].fader = 127;
    ok = morph_view(0, id) == endp(0, 1)[id] && (morph_view_moved(0, id) == (endp(0, 1)[id] != trk[0].p[id]));
    mph[0].fader = 0;
    ok &= morph_view(0, id) == endp(0, 0)[id] && (morph_view_moved(0, id) == (endp(0, 0)[id] != trk[0].p[id]));
    mph[0].fader = 127;
    expect("(a) fader 127: the card value is End B's (marked if it differs from the Original); fader 0: End A's", ok);
    ui.menu = 0; go_title(PAGES[pg].title); frames(32);
    mph[0].fader = 126; frames(8);                       /* (cloud job G: at 127 the knob is End B's, below it the Original's) */
    vb = morph_view(0, id); pb = trk[0].p[id];
    turn(EN_K1 + slot, 1); frames(8);
    before = trk[0].p[id] - pb;
    ok = before != 0 && (morph_view(0, id) - vb) - before >= -1 && (morph_view(0, id) - vb) - before <= 1 && trk[0].p[id] == pb + before && !morph_edits(0, (xb_edit_t[XB_MAX]){0});
    expect("(b) below the B end a knob turn edits the Original by one step; the shown value follows it", ok);
    mph[0].fader = 127; frames(8);
    vb = morph_view(0, id); pb = trk[0].p[id];
    turn(EN_K1 + slot, 1); frames(8);
    ok = trk[0].p[id] == pb && morph_view(0, id) != vb && morph_view(0, id) == endp(0, 1)[id] && pin_get(morph_pins(&mph[0]), id);
    expect("(b2) at the B end (127) the same turn sets End B's value (the Original is as it was), the shown value is B's", ok);
    if (ex != 0xFFFFu)
        expect("(c) a value the morph never moves: shown as stored, no mark", morph_view(0, ex) == trk[0].p[ex] && !morph_view_moved(0, ex));
}

/* -------------------------------------------------------------------- LOAD --- */
static void t_load(void)
{
    fresh();
    morph_page();
    turn(EN_K2, -MORPH_DET); settle();
    turn(EN_K2, MORPH_DET); settle();
    turn(EN_K1, 50);
    expect("rolled: both ends differ from the Original, the fader at 50", !end_is_base(0, 0) && !end_is_base(0, 1) && mph[0].nid &&
           mph[0].fader == 50u);
    turn(EN_PRESET, 1);
    frames(32);
    expect("a sound load (PRESETS): the fader back at A, End A the sound, End B a fresh roll (seeded, the AMOUNT), no history",
           mph[0].fader == 0u && end_is_base(0, 0) && !end_is_base(0, 1) && rolls(0, 0) == 1u && rolls(0, 1) == 2u &&
           !seed_now(0, 0) && seed_now(0, 1) && mph[0].nid && mph[0].preset == trk[0].preset);
    expect("AMOUNT follows the patch: the loaded factory sound's own AMOUNT shows on KNOB 4 (the rolled one's too)",
           !trk[0].user && mph[0].amount == ds_amount(trk[0].eng_req, trk[0].preset) && mph[0].ramt[1][mph[0].top[1]] == mph[0].amount);
    turn(EN_K4, 7);
    expect("  .. turning KNOB 4 changes the AMOUNT", mph[0].amount != ds_amount(trk[0].eng_req, trk[0].preset));
    turn(EN_PRESET, 1);
    frames(32);
    expect("  .. the next sound load sets it to that sound's AMOUNT again",
           mph[0].amount == ds_amount(trk[0].eng_req, trk[0].preset));
}

/* ---------------------------------------------------------------- SAVE HELD --- */
/* 0.2.17: ONE list of 128 slots (a slot is its saved record, else its factory sound). Hold SAVE: OVERWRITE #NN, the slot the part's sound came from,
 * K1 picks another; OCT+ writes it, End B and AMOUNT with it. There is no 'new' sound, no shadow copy and no NAME screen. */
static void t_save(void)
{
    int ok_both;
    int8_t b0[P_COUNT];
    int16_t fac[P_COUNT];
    uint32_t sd, am, pos, total, t2, sl, e;
    fresh();
    morph_page();
    press(B_SAVE);
    frames(32);
    expect("a press of SAVE on the MORPH page opens the SAVE pages (PATCHES) as everywhere; nothing saved, no dialog",
           !at_home() && str_eq(cur_page()->title, "PATCHES") && !up_count() && !ui.confirm);
    morph_page();
    pos = preset_pos(&total);
    preset_go(pos);                                     /* (a factory sound, as the browser loads it) */
    frames(32);
    sl = slot_of(&trk[0]);
    e = trk[0].eng_req;
    memcpy(fac, trk[0].p, sizeof fac);
    trk[0].p[P_REL] = (int16_t)(trk[0].p[P_REL] > 60 ? 10 : 90);   /* (the user's edit) */
    turn(EN_K4, 20);
    turn(EN_K2, MORPH_DET); settle();                   /* ROLL B at AMOUNT 45 */
    turn(EN_K1, 70);
    expect("a factory sound is in its slot (the browse position), the list is the 128 slots", sl == pos && total == UP_SLOTS && sl < UP_SLOTS);
    hold(B_SAVE);
    expect("SAVE held on MORPH: the OVERWRITE dialog for part 1, offering the slot the sound came from (nothing saved yet); OCT- cancels",
           ui.confirm == CF_SAVE_OVER && ui.confirm_trk == 0u && ui.svs == sl && !up_count() && (press(B_OCTDN), !ui.confirm && !up_count()));
    hold(B_SAVE);
    press(B_OCTUP);
    memcpy(b0, endp(0, 1), P_COUNT);
    sd = seed_now(0, 1);
    am = mph[0].amount;
    expect("  OCT+: SAVED #NN, still on MORPH; one record, in that slot; the part plays that slot now",
           up_count() == 1u && up_used(sl) && !strncmp(ui.msg, "SAVED", 5) && str_eq(cur_page()->title, "MORPH") && trk[0].user == sl + 1u &&
           up_rec(sl)->engine == e && up_value(up_rec(sl), P_REL) == trk[0].p[P_REL]);
    expect("the browser: the slot stays in its place (same position, same length 128)", preset_pos(&t2) == pos && t2 == UP_SLOTS && preset_at(pos) == sl);
    turn(EN_K4, -30);                                   /* AMOUNT 15, another B, another sound */
    turn(EN_PRESET, 1);
    frames(32);
    expect("(control) another sound: End B is another roll", memcmp(endp(0, 1), b0, P_COUNT) != 0);
    turn(EN_PRESET, -1);
    frames(32);
    expect("PRESETS back one: the saved slot loads: fader at A, the saved sound, End B exactly as saved (seed, AMOUNT, values)",
           trk[0].user == sl + 1u && mph[0].fader == 0u && end_is_base(0, 0) && trk[0].p[P_REL] != fac[P_REL] &&
           !memcmp(endp(0, 1), b0, P_COUNT) && seed_now(0, 1) == sd && mph[0].amount == am);
    hold(B_SAVE);
    press(B_OCTUP);
    expect("SAVE held over it again: the same slot (no second one)", up_count() == 1u && up_used(sl) && !strncmp(ui.msg, "SAVED", 5) && preset_pos(&t2) == pos);
    up_ui(1u, sl);                                      /* RESTORE */
    preset_go(pos);
    frames(32);
    expect("RESTORE: the factory sound back in the slot; loading it gives the factory sound",
           !up_count() && preset_at(pos) == sl && trk[0].user == 0u && slot_of(&trk[0]) == sl && !memcmp(trk[0].p, fac, sizeof fac));
    confirm_open(CF_INIT_SOUND, 0u);
    press(B_OCTUP);
    hold(B_SAVE);
    press(B_OCTUP);                                     /* (INIT SOUND is the engine's first sound: SAVE offers the slot it belongs to; OCT+ writes) */
    expect("after INIT SOUND: SAVE held offers the slot of the engine's first sound; OCT+ saves it there (no NAME screen)",
           up_count() == 1u && !name_on() && !strncmp(ui.msg, "SAVED", 5) && trk[0].user != 0u && up_used(trk[0].user - 1u));
    morph_set_target(MT_BOTH);
    hold(B_SAVE);
    ok_both = ui.confirm == CF_SAVE_OVER && ui.confirm_trk == 2u;
    press(B_OCTUP);
    expect("TARGET BOTH on MORPH: OVERWRITE BOTH PATCHES, OCT+: SAVED 1+2 (each part over its own slot)",
           ok_both && str_eq(ui.msg, "SAVED 1+2") && up_count() == 2u && trk[0].user != 0u && trk[1].user != 0u && trk[0].user != trk[1].user);
}

/* ---------------------------------------------------------------- SETTINGS --- */
static void power_cycle(void)                           /* the morph's RAM gone, the settings bytes kept (flash) */
{
    uint8_t keep[3] = {favorites.factory[15][12], favorites.factory[15][13], favorites.factory[15][8]};
    ui_power_on();
    favorites.factory[15][12] = keep[0];
    favorites.factory[15][13] = keep[1];
    favorites.factory[15][8] = keep[2];
    frame();
}
static void t_settings(void)
{
    fresh();
    frames(2500);
    expect("the default AMOUNT and SELECTED write nothing (the bytes stay 0, no flash write at power-on)",
           !favorites.factory[15][12] && !favorites.factory[15][13] && !ui.mset_dirty);
    morph_page();
    turn(EN_K4, 40 - (int)mph[0].amount);
    turn(EN_ALGO, 1);
    turn(EN_ALGO, 1);                                   /* (0.2.9: BOTH, then PART 2) */
    turn(EN_K4, 5 - (int)mph[1].amount);
    turn(EN_ALGO, -1);                                  /* BOTH: not saved */
    go_title("MORPH SET"); frames(32);
    turn(EN_K2, 1);
    turn(EN_K3, 1);
    frame();
    expect("AMOUNT 40 / 5 into the settings bytes (1 + %), BOTH not (bit 7 reserved, 0.2.9), a save queued",
           favorites.factory[15][12] == 41u && favorites.factory[15][13] == 6u && ui.mset_dirty && morph.target == MT_BOTH);
    frames(2200);
    expect("  .. saved once the knob rested ~2 s (settings_save)", !ui.mset_dirty);
    power_cycle();
    expect("after a power cycle: each part's AMOUNT = its sound's (the settings keep 40 / 5 as the player's own), not BOTH (power-on: one part), ARP and PITCH ON; the ends' rolls and faders not kept; "
           "part 1's factory sound: its own End B (its seed and AMOUNT, SMART, no ARP / PITCH: sounds.c)",
           mph[0].amount == ds_amount(trk[0].eng_req, trk[0].preset) && mph[1].amount == ds_amount(trk[1].eng_req, trk[1].preset) &&
           favorites.factory[15][12] == 41u && favorites.factory[15][13] == 6u && morph.target == MT_SELECTED && rolls(0, 0) == 1u && !mph[0].fader &&
           morph.cfg == (MC_ARP | MC_PIT) && !trk[0].user && trk[0].preset < ds_count(trk[0].eng_req) &&
           mph[0].seed[1][mph[0].top[1]] == ds_seed(trk[0].eng_req, trk[0].preset) &&
           mph[0].ramt[1][mph[0].top[1]] == ds_amount(trk[0].eng_req, trk[0].preset) && !(mph[0].rcfg[1][mph[0].top[1]] & MC_SET));
    favorites.factory[15][12] = 0x80u | 41u;            /* an older setting with TARGET BOTH */
    power_cycle();
    frames(2500);
    expect("an older setting's TARGET BOTH bit: power-on is not BOTH; the bit is kept as stored (no rewrite, no flash write)",
           morph.target == MT_SELECTED && favorites.factory[15][12] == (0x80u | 41u) && !ui.mset_dirty);
    morph_page();
    turn(EN_ALGO, 1);
    frames(2500);
    expect("  .. ALGORITHM's BOTH writes nothing either (the bit stays)", morph.target == MT_BOTH && favorites.factory[15][12] == (0x80u | 41u) &&
           !ui.mset_dirty);
    favorites.factory[15][12] = 0xFF; favorites.factory[15][13] = 102;
    frame();
    expect("unknown codes read as the default (15 %); bit 7 read as nothing", mph[0].amount == MORPH_AMOUNT_DEF &&
           mph[1].amount == MORPH_AMOUNT_DEF && morph.target == MT_BOTH);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    t_round();
    t_knobs();
    t_gesture();
    t_far();
    t_target();
    t_midi();
    t_matrix();
    t_view();
    t_load();
    t_save();
    t_settings();
    t_editb();
    printf("morphui: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
