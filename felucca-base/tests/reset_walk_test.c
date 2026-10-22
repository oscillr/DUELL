/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.24 (owner: on AMP ENV, OCT- + a knob did not reset): the quick-reset gesture (OCT- held + a knob turned = that card to its default,
 * ui_input.c knob_reset) walked over EVERY page of the table and every knob that has a value: the value is set off its default, the gesture is made,
 * the value must be the default and the header must say RESET. Pages that are no values (the action pages, STEP, AUTOMATION, ENGINE, PATCHES,
 * PATTERNS, MULTIS, TOOLS, the FM6 operator pages, TEST) are skipped and listed. Every engine's EDIT 1 / 2 pages are walked too, and AMP ENV with
 * ENV SYNC on and off. Built and run by DUELL/host_tests.sh (name: resetwalk). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, ncheck, nknobs;
static void expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("resetwalk: FAIL %s\n", what);
        bad++;
    }
}
static int said_reset(void) { return ui.msg_t && ui.msg[0] == 'R' && ui.msg[1] == 'E' && ui.msg[2] == 'S' && ui.msg[3] == 'E' && ui.msg[4] == 'T' && ui.msg[5] == ' '; }
static void quiet(void) { frames(400); }

/* a value off the default that the knob offers: the next one up, else the next one down */
static int16_t off_def(const param_desc_t *d)
{
    return d->def < d->max ? (int16_t)(d->def + 1) : (int16_t)(d->def - 1);
}

/* the knobs of page i (the current one) that carry a parameter: set off, reset, check */
static void walk_page(uint32_t i, const char *tag)
{
    uint32_t k, j, n;
    uint8_t vis[4];
    for (k = 0; k < 4u; k++) {
        int16_t *vp;
        const param_desc_t *d = page_desc(&PAGES[i], k, &vp);
        char what[96];
        if (!d || !vp || d->max == d->min || (d->label && d->label[0] == '-'))
            continue;
        if (card_hidden_vp(vp))
            continue;
        nknobs++;
        *vp = off_def(d);
        n = page_cards(&PAGES[i], vis);                     /* T0227: the cards that show moved left (as the value now makes them): the knob of slot k */
        for (j = 0; j < n && vis[j] != k; j++)
            ;
        if (j >= n) {                                       /* (its own value hid it: SYNC on hides nothing of its own) */
            *vp = d->def;
            continue;
        }
        ui.msg_t = 0;
        quiet();
        btn_down(B_OCTDN); frame();
        turn(EN_K1 + j, 1);
        snprintf(what, sizeof what, "%s%s K%u %s: OCT- + the knob resets to the default (%d, was %d, is %d), said RESET", PAGES[i].title, tag, j + 1u, d->label ? d->label : "?",
                 d->def, off_def(d), *vp);
        expect(what, *vp == d->def && said_reset());
        btn_up(B_OCTDN); frame(); frame();
        song.octave = 0;
    }
}

static int walkable(uint32_t i)
{
    const page_t *pg = &PAGES[i];
    if (pg->scope == SC_STEP || pg->scope == SC_TEST || pg->graph == GR_EVENTS || pg->graph == GR_ENGINE || pg->graph == GR_BROWSE || pg->graph == GR_PATS ||
        pg->graph == GR_SLOTS || pg->graph == GR_TOOLS || pg->graph == GR_FMOP || pg->graph == GR_FMOP2 || pg->graph == GR_FMENV)
        return 0;
    return 1;
}

int main(void)
{
    uint32_t i, e;
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    duell_mode_req = DM_STEREO;
    duell_mode = DM_STEREO;
    song.g[G_RTYPE] = 3;                                    /* SHIM: the SHIMMER page is in the round */
    song.g[G_DMODE] = 1;                                    /* PING-PONG: SPREAD and MRATE both apply */
    song.cp[1] = 4;                                         /* COMP on: THRESH ATTACK RELEASE apply */
    init_engine_of(TSEL, 0u);                               /* ANALOG (the full INIT): the AMP ENV is the track's */
    TSEL->p[P_ITYPE] = 1;                                   /* INSERT on: A B C MIX apply */
    frames(64);
    for (i = 0; i < NPAGES; i++) {
        if (!walkable(i)) {
            printf("resetwalk: skipped %s (no values: an action, a list or the operator's bytes)\n", PAGES[i].title);
            continue;
        }
        if (!page_visible(i))
            continue;
        ui.page = (uint8_t)i;
        page_entered();
        frames(64);
        walk_page(i, "");
    }
    /* every engine's EDIT 1 / 2 (the engine's own eight), and the engine's own filter page */
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e))
            continue;
        init_engine_of(TSEL, e);
        TSEL->p[P_ITYPE] = 1;
        frames(64);
        for (i = 0; i < NPAGES; i++) {
            if (!walkable(i) || !page_visible(i) || PAGES[i].fam != FAM_EDIT)
                continue;
            ui.page = (uint8_t)i;
            page_entered();
            frames(64);
            walk_page(i, ENGINES[e]->name);
        }
    }
    /* AMP ENV with ENV SYNC on (the note-value descriptors) */
    init_engine_of(TSEL, 0u);
    TSEL->p[P_ESYNC] = 1;
    go_title("AMP ENV");
    frames(64);
    walk_page((uint32_t)ui.page, " ESYNC");
    TSEL->p[P_ESYNC] = 0;
    /* AMP ENV reached as the owner reaches it: a tap of ENV, then OCT- and the knob within the same second */
    ui_power_on();
    duell_mode_req = DM_STEREO;
    init_engine_of(TSEL, 0u);
    frames(64);
    press(B_ENV);
    expect("a tap of ENV opens AMP ENV", str_eq(cur_page()->title, "AMP ENV"));
    TSEL->p[P_ATK] = 90;
    btn_down(B_OCTDN); frame();
    turn(EN_K1, 1);
    expect("right after the ENV tap: OCT- + KNOB 1 resets ATK", TSEL->p[P_ATK] == TP[P_ATK].def && said_reset());
    btn_up(B_OCTDN); frame(); frame();
    /* .. and with the knob turned in the same frame as the OCT- press */
    quiet();
    TSEL->p[P_DEC] = 5;
    host_enc[panel.enc[EN_K2]] += panel.dir[EN_K2];
    btn_down(B_OCTDN); frame();
    expect("OCT- pressed and KNOB 2 turned in one frame: DEC reset", TSEL->p[P_DEC] == TP[P_DEC].def);
    btn_up(B_OCTDN); frame(); frame();
    /* the owner's case (0.2.24): a LIVE MORPH (End B rolled, the fader in the middle): the card shows live + (blend - Original), so the stored default alone is not what is
     * shown or heard; the reset must land the SOUNDING value on the default (ui_input.c reset_desc moves the stored value by the difference) */
    quiet();
    morph_act(MQ_ROLL, morph_mask(song.sel), 1u, song.sel); frames(64);
    morph_act(MQ_FADER, morph_mask(song.sel), 64u, song.sel); frames(300);
    expect("a live morph on the part (End B rolled, the fader at 64)", morph_view_on(song.sel));
    for (i = 0; i < 4u; i++) {
        static const uint8_t ID[4] = {P_ATK, P_DEC, P_SUS, P_REL};
        char what[96];
        TSEL->p[ID[i]] = (int16_t)(TP[ID[i]].def + 30);
        frames(64);
        btn_down(B_OCTDN); frame();
        turn(EN_K1 + i, 1);
        snprintf(what, sizeof what, "live morph, AMP ENV K%u %s: the SOUNDING value is the default after the reset (shown %d, default %d)", i + 1u, TP[ID[i]].label,
                 morph_view(song.sel, ID[i]), TP[ID[i]].def);
        expect(what, morph_view(song.sel, ID[i]) == TP[ID[i]].def && said_reset());
        btn_up(B_OCTDN); frame(); frame();
        quiet();
    }
    printf("resetwalk: %d checks over %d knobs, %d failed\n", ncheck, nknobs, bad);
    return bad ? 1 : 0;
}
