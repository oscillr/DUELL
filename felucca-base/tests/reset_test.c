/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.9: OCT- held + a knob turned = that card to its DEFAULT (ui_input.c knob_reset), and OCT- / OCT+ move the octave when LET GO (oct_rel).
 *   RESET      a plain parameter (AMP ENV ATK), PERFORM's four cards, MIXER's LEVEL / PAN / SPACE (0.2.10), MODE's BAL, TEMPO (BPM, SWG): to the table's default / the natural
 *              zero; once per gesture ("RESET .." in the header), the turn is not applied, further detents are ignored until the knob has been still ~300 ms
 *              (also after OCT- is let go); a switch (MIXER MUTE), a greyed card (MIXER PAN in DUEL mode) and an action page: nothing; normal turning is unchanged
 *   OCTAVE     a tap moves it on release (not on press); OCT- used as a modifier moves no octave; both together still reset it to 0
 * Built and run by DUELL/host_tests.sh (name: reset): the UI sources against ui_test.c's stub display and knobs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("reset: FAIL %s\n", what);
        bad++;
    }
    return ok;
}
static int said_reset(void) { return ui.msg_t && ui.msg[0] == 'R' && ui.msg[1] == 'E' && ui.msg[2] == 'S' && ui.msg[3] == 'E' && ui.msg[4] == 'T' && ui.msg[5] == ' '; }
static void quiet(void) { frames(400); }                    /* the knob still for > 300 ms */
static void page(const char *title) { ui_power_on(); go_title(title); song.octave = 0; ui.msg_t = 0; frames(400); }

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* a plain parameter */
    page("AMP ENV");
    TSEL->p[P_ATK] = 90;
    turn(EN_K1, 3);
    expect("no OCT-: a turn is a turn (ATK moved up)", TSEL->p[P_ATK] == 93 && !said_reset());
    btn_down(B_OCTDN); frame();
    turn(EN_K1, 3);
    expect("OCT- + KNOB 1: ATK to its default, said once", TSEL->p[P_ATK] == TP[P_ATK].def && said_reset());
    ui.msg_t = 0;
    turn(EN_K1, 5); turn(EN_K1, -2); turn(EN_K1, 1);
    expect("  .. further detents of the gesture are ignored, no creeping, no second message", TSEL->p[P_ATK] == TP[P_ATK].def && !said_reset());
    btn_up(B_OCTDN); frame();
    turn(EN_K1, 4);
    expect("  .. also after OCT- is let go while the knob goes on", TSEL->p[P_ATK] == TP[P_ATK].def);
    expect("  .. and OCT- used as a modifier moved no octave", song.octave == 0);
    quiet();
    turn(EN_K1, 4);
    expect("  .. the knob still for 300 ms: it turns again", TSEL->p[P_ATK] == TP[P_ATK].def + 4);
    TSEL->p[P_DEC] = 5;
    btn_down(B_OCTDN); frame();
    turn(EN_K2, 1);
    expect("another knob of the same hold resets its own card", TSEL->p[P_DEC] == TP[P_DEC].def && said_reset());
    btn_up(B_OCTDN); frame(); frame();
    expect("  .. no octave on release", song.octave == 0);

    /* the octave: on release; a tap; both together */
    page("AMP ENV");
    btn_down(B_OCTUP); frame(); frame();
    expect("OCT+ held: the octave has not moved yet", song.octave == 0);
    btn_up(B_OCTUP); frame();
    expect("  .. let go: +1", song.octave == 1);
    btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame(); frame();
    expect("OCT- tapped: -1 on release", song.octave == 0);
    btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame(); frame();
    expect("OCT- tapped again: -1", song.octave == -1);
    song.octave = 2;
    btn_down(B_OCTDN); frame(); btn_down(B_OCTUP); frame();
    expect("OCT- + OCT+ together: the octave to 0", song.octave == 0);
    btn_up(B_OCTDN); frame(); btn_up(B_OCTUP); frame(); frame();
    expect("  .. and no step on their release", song.octave == 0);
    song.octave = 1;
    btn_down(B_OCTUP); frame(); btn_down(B_OCTDN); frame(); btn_up(B_OCTUP); frame(); btn_up(B_OCTDN); frame(); frame();
    expect("  .. either order", song.octave == 0);
    song.octave = 3;
    press(B_OCTUP);
    expect("OCT+ at +3 stays", song.octave == 3);
    song.octave = 0;

    /* a modifier turn does not move the octave, even when OCT+ is tapped meanwhile? (that is the chord: 0) */
    page("AMP ENV");
    btn_down(B_OCTDN); frame(); turn(EN_K1, 1); btn_up(B_OCTDN); frame(); frame();
    expect("OCT- + a knob + let go: no octave", song.octave == 0);
    btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame(); frame();
    expect("  .. the next tap is a tap again (the modifier flag is gone)", song.octave == -1);

    /* PERFORM */
    page("AMP ENV");
    ui.perf = 1; frames(32);
    morph_act(MQ_FADER, morph_mask(song.sel), 60u, song.sel);
    perf_space = 40; perf_filt = -50; perf_depth = 30;
    btn_down(B_OCTDN); frame();
    turn(EN_K1, 1);
    expect("PERFORM K1 MORPH: 0 (A)", morph_fader_of(song.sel) == 0u && said_reset());
    quiet(); turn(EN_K2, 1);
    expect("PERFORM K2 SPACE: 0", perf_space == 0);
    quiet(); turn(EN_K3, -1);
    expect("PERFORM K3 FILTER: 0", perf_filt == 0);
    quiet(); turn(EN_K4, -1);
    expect("PERFORM K4 DEPTH: 100", perf_depth == 100);
    btn_up(B_OCTDN); frame(); frame();
    quiet();
    turn(EN_K3, 5);
    expect("PERFORM without OCT-: normal turn", perf_filt == 5);
    ui.perf = 0;

    /* MIXER */
    page("MIXER");
    duell_mode_req = DM_STEREO;
    TSEL->p[P_LEVEL] = 40; TSEL->p[P_PAN] = 20; TSEL->p[P_REV] = 60; TSEL->p[P_MUTE] = 1; perf_space = 40;
    btn_down(B_OCTDN); frame();
    turn(EN_K1, 1);
    expect("MIXER K1 LEVEL: its default", TSEL->p[P_LEVEL] == TP[P_LEVEL].def && said_reset());
    quiet(); turn(EN_K2, 1);
    expect("MIXER K2 PAN: centre", TSEL->p[P_PAN] == 0);
    quiet(); turn(EN_K3, 1);
    expect("MIXER K3 SPACE: 0, said RESET SPACE, the part's REV send untouched", perf_space == 0 && TSEL->p[P_REV] == 60 && msg_is("RESET SPACE"));
    quiet(); ui.msg_t = 0; turn(EN_K4, -1);
    expect("MIXER K4 MUTE is a switch: the turn goes as ever", TSEL->p[P_MUTE] == 0 && !said_reset());
    btn_up(B_OCTDN); frame(); frame();
    expect("  .. (the hold was a modifier: no octave on release)", song.octave == 0);
    song.octave = 0;
    duell_mode_req = DM_SPLIT;
    TSEL->p[P_PAN] = 20;
    quiet();
    btn_down(B_OCTDN); frame(); turn(EN_K2, 1); btn_up(B_OCTDN); frame(); frame();
    expect("MIXER PAN in DUEL mode (greyed): nothing", TSEL->p[P_PAN] == 20 && song.octave == 0);
    duell_mode_req = DM_STEREO;

    /* MODE: BAL */
    page("MODE");
    mix_bal = 50;
    btn_down(B_OCTDN); frame(); turn(EN_K2, -1); btn_up(B_OCTDN); frame();
    expect("MODE K2 BAL: centre", mix_bal == 0);

    /* TEMPO */
    page("TEMPO");
    song.g[G_BPM] = 150; song.g[G_SWING] = 40; song.g[G_CLOCK] = 0;
    btn_down(B_OCTDN); frame(); turn(EN_K1, 1); quiet(); turn(EN_K3, 1);
    expect("TEMPO K1 / K3: their defaults", song.g[G_BPM] == GP[G_BPM].def && song.g[G_SWING] == GP[G_SWING].def);
    quiet(); ui.msg_t = 0;
    turn(EN_K2, 1);
    expect("TEMPO K2 TAP is no parameter: no reset", !said_reset());
    btn_up(B_OCTDN); frame();

    /* an action page: OCT- stays "back" */
    page("TOOLS");
    expect("TOOLS is an action page", act_cols() != 0u);
    TSEL->p[P_ATK] = 90;
    btn_down(B_OCTDN); frame(); turn(EN_K1, 1);
    expect("action page: no reset", !said_reset());
    btn_up(B_OCTDN); frame(); frame();

    /* 0.2.10: the QUICK EDIT layers reset their cards too (ui_layer.c layer_quick_reset); OCT- then neither puts back nor moves the octave */
    page("MIXER");
    btn_down(B_ENV); frames(640);
    expect("QUICK ENV open", ui.layer == LAYER_ENV);
    TSEL->p[P_ATK] = 90; ui.msg_t = 0;
    btn_down(B_OCTDN); frame();
    turn(EN_K1, 3);
    expect("QUICK ENV + OCT- + KNOB 1: AMP ATK to its default, said", TSEL->p[P_ATK] == TP[P_ATK].def && said_reset() && ui.layer == LAYER_ENV);
    ui.msg_t = 0;
    turn(EN_K1, 4);
    expect("  .. further detents ignored (no creeping)", TSEL->p[P_ATK] == TP[P_ATK].def && !said_reset());
    btn_up(B_OCTDN); frame(); frame();
    expect("  .. OCT- let go: no octave", song.octave == 0);
    btn_up(B_ENV); frames(64); quiet();
    page("MIXER");
    btn_down(B_ARP); frames(640);
    TSEL->p[P_AOCT] = (int16_t)(TP[P_AOCT].def + 2);
    btn_down(B_OCTDN); frame(); turn(EN_K4, -1); btn_up(B_OCTDN); frame(); frame();
    expect("QUICK ARP + OCT- + KNOB 4: OCT to its default, no octave", ui.layer == LAYER_ARP && TSEL->p[P_AOCT] == TP[P_AOCT].def && song.octave == 0);
    btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame(); frame();
    expect("  .. a plain OCT- tap there is still the octave", song.octave == -1);
    btn_up(B_ARP); frames(64); quiet();
    page("MIXER");
    set_engine_of(TSEL, 2u);                                 /* PHASE: QUICK FILTER is the multimode filter's CUT RES MODE + AMP REL */
    {
        int16_t c0, r0;
        TSEL->p[P_FREEB] = (int16_t)(TP[P_FREEB].def + 20);
        r0 = TSEL->p[P_FREEB];
        c0 = TSEL->p[P_FREEA];
        btn_down(B_EDIT); frames(640);
        expect("QUICK FILTER open", ui.layer == LAYER_EDIT);
        turn(EN_K1, -4);
        expect("  CUT turned", TSEL->p[P_FREEA] != c0);
        quiet(); ui.msg_t = 0;
        btn_down(B_OCTDN); frame(); turn(EN_K2, 1);
        expect("QUICK FILTER + OCT- + KNOB 2: RES to its default, said", TSEL->p[P_FREEB] == TP[P_FREEB].def && said_reset());
        btn_up(B_OCTDN); frame(); frame();
        expect("  .. OCT- let go after the reset: no put back (CUT stays turned), no octave",
               TSEL->p[P_FREEA] != c0 && TSEL->p[P_FREEB] == TP[P_FREEB].def && song.octave == 0 && !msg_is("VALUES PUT BACK"));
        quiet();
        btn_down(B_OCTDN); frame(); btn_up(B_OCTDN); frame(); frame();
        expect("  .. a plain OCT- tap: put back as it opened (CUT and RES), no octave",
               TSEL->p[P_FREEA] == c0 && TSEL->p[P_FREEB] == r0 && song.octave == 0 && msg_is("VALUES PUT BACK"));
        btn_up(B_EDIT); frames(64);
    }

    printf("reset: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
