/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* 1.5's sequencer edits (Discussions #178, #173), on the real sequencer and UI sources (with the stubs of tests/ui_test.c):
 *   LEN   SEQ > STEP's KNOB 3 on a note (ui.c note_set_len): right writes clean TIE steps after it (a REST or an empty
 *         step; a chord ties whole), stopping at the next step holding notes (NOTE AHEAD; a REST's kept notes too) and
 *         at LEN (END OF PATTERN); left turns its last TIEs back into RESTs, never the note itself; nothing else moves
 *         (the steps' locks stay); one undo per turn (SAVE held), redo; on a REST / TIE / empty step KNOB 3 is TIME as
 *         before; the DRUM grid's KNOB 3 is still HIT. Played: one note-on, held to the end of its last TIE (as a
 *         recorded hold plays).
 *   VEL   step_t.vel as it plays (seq.c seq_step: 0 = 96, ACC 127, a DRUM step's accented hit 127, its other hits the
 *         step's); SEQ > AUTOMATION: no VEL rows until + ADD VEL's OCT+ (SHOW) lists every note step inside LEN, KNOB 4
 *         1..127, EDIT back to 96, SAVE held undoes, KNOB 2 nothing, HIDE / leaving the page hides them; a pattern
 *         untouched plays as before (the golden renders: tests/regress.c).
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* track 1 alone (ANALOG, POLY), 16 steps of 1/16, GATE 64, no swing; SEQ > STEP on step `cur` */
static track_t *roll(uint32_t cur)
{
    track_t *t;
    ui_power_on();
    song.sel = 0;
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    t->p[P_VOICE] = V_POLY;
    song.g[G_SWING] = 0;
    song.g[G_BPM] = 120;
    go_page(GR_ROLL);
    ui.cursor = (uint16_t)cur;
    frame();
    return t;
}
static void put(track_t *t, uint32_t i, uint32_t note, uint32_t time)
{
    t->step[i] = (step_t){{(uint8_t)note}, note ? 1u : 0u, (uint8_t)time, 0, 0};
}
static int is_tie(const step_t *s)                 /* a clean TIE, as live recording writes one */
{
    static const step_t TIE = {{0}, 0, ST_TIE};
    return !memcmp(s, &TIE, sizeof *s);
}
static int is_rest(const step_t *s)
{
    static const step_t REST = {{0}, 0, ST_REST};
    return !memcmp(s, &REST, sizeof *s);
}
/* the steps = ref but from..to (inclusive) */
static int same_but(const track_t *t, const step_t *ref, uint32_t from, uint32_t to)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if ((i < from || i > to) && memcmp(&t->step[i], &ref[i], sizeof ref[i]))
            return 0;
    return 1;
}

/* note-ons over `blocks` blocks from PLAY; *off: the first block after them where nothing of the track sounds */
static uint32_t play(track_t *t, uint32_t blocks, uint32_t *off, uint8_t *vel, uint32_t nvel)
{
    uint32_t b, n = 0, i;
    *off = 0;
    seq_start();
    for (b = 0; b < blocks; b++) {
        uint32_t v0 = vage;
        seq_tick(t, CTL);
        for (; v0 < vage; v0++, n++)
            for (i = 0; i < NVOICE; i++)
                if (t->v[i].age == v0 + 1u && n < nvel)
                    vel[n] = t->v[i].vel;
        if (n && !*off && !t->seq_n)
            *off = b;
    }
    seq_stop();
    return n;
}

static int len_edit(void)
{
    int bad = 0, ok;
    step_t ref[NSTEP];
    track_t *t;
    uint32_t i;

    t = roll(1);
    t->step[1] = (step_t){{60, 64}, 2, ST_NOTE, SF_ACCENT, 90};   /* a chord on step 2 */
    put(t, 2, 0, ST_REST);
    t->step[3] = (step_t){{0}, 0, ST_NOTE, 0, 0, 0, 0, 40};        /* empty, a CHANCE of 40 % */
    put(t, 5, 67, ST_NOTE);
    motion_set_lock(t, 2, P_ATK, 9);
    memcpy(ref, t->step, sizeof ref);
    turn(EN_K3, 1);
    ok = is_tie(&t->step[2]) && same_but(t, ref, 2, 2) && note_len(t, 1) == 2u && motion_find(t, 2, P_ATK) >= 0;
    turn(EN_K3, 1);
    ok &= is_tie(&t->step[3]) && same_but(t, ref, 2, 3) && note_len(t, 1) == 3u;
    bad += check("LEN: KNOB 3 right on a note: the next REST, then an empty step, clean TIEs (its lock kept)", ok);
    turn(EN_K3, 1);
    ok = note_len(t, 1) == 4u && is_tie(&t->step[4]);
    ui.msg_t = 0;
    turn(EN_K3, 1);
    ok &= note_len(t, 1) == 4u && msg_is("NOTE AHEAD") && !memcmp(&t->step[5], &ref[5], sizeof ref[5]);
    bad += check("  it stops before the next note (NOTE AHEAD, that note kept)", ok);
    turn(EN_K3, -1);
    ok = note_len(t, 1) == 3u && is_rest(&t->step[4]);
    for (i = 0; i < 6u; i++)
        turn(EN_K3, -1);
    ok &= note_len(t, 1) == 1u && is_rest(&t->step[2]) && is_rest(&t->step[3]) && t->step[1].n == 2u &&
          t->step[1].time == ST_NOTE;
    bad += check("  left: its last TIEs become RESTs, down to LEN 1 (the note stays)", ok);
    hold(B_SAVE);                                    /* (the turns 1.5 s apart or less: one undo) */
    ok = !memcmp(t->step, ref, sizeof ref);
    hold(B_SAVE);
    ok &= note_len(t, 1) == 1u && is_rest(&t->step[3]);
    bad += check("  SAVE held: the knob's turns undone at once (back as it was); held again: redone", ok);
    frames(2000);
    turn(EN_K3, 2);
    frames(2000);
    turn(EN_K3, 1);
    ok = note_len(t, 1) == 4u;
    hold(B_SAVE);
    ok &= note_len(t, 1) == 3u && is_rest(&t->step[4]);
    bad += check("  turns apart: each its own undo", ok);

    t = roll(15);                                     /* the last step of LEN */
    put(t, 15, 60, ST_NOTE);
    turn(EN_K3, 1);
    bad += check("  on the last step: END OF PATTERN (no wrap)", note_len(t, 15) == 1u && msg_is("END OF PATTERN") &&
                 t->step[0].time == ST_REST);
    t = roll(3);                                      /* a REST: KNOB 3 is TIME, as before */
    turn(EN_K3, -1);
    ok = t->step[3].time == ST_TIE;
    turn(EN_K3, -1);
    ok &= t->step[3].time == ST_NOTE;   /* (empty: still TIME) */
    t->step[4] = (step_t){{0}, 0, ST_REST, 0, 0, 1, 0, 0};    /* a REST keeping a hit: not overwritten */
    put(t, 3, 50, ST_NOTE);
    turn(EN_K3, 1);
    ok &= note_len(t, 3) == 1u && msg_is("NOTE AHEAD") && t->step[4].hit == 1u;
    bad += check("  a REST / TIE / empty step: KNOB 3 TIME (NOTE TIE REST); a REST keeping notes or hits is kept", ok);

    ui_power_on();                                    /* the DRUM grid: KNOB 3 is still HIT */
    song.sel = 3;
    t = &trk[3];
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    go_page(GR_ROLL);
    ui.cursor = 2;
    ui.lane = 0;
    frame();
    grid_hit(t, 2, 0, 1);
    turn(EN_K3, 1);
    ok = drum_track(t) && t->step[3].time != ST_TIE && (step_lanes(&t->step[2]) & 1u);
    turn(EN_K3, -1);
    ok &= !(step_lanes(&t->step[2]) & 1u) && t->step[3].time != ST_TIE;
    bad += check("  the DRUM grid: KNOB 3 HIT as before, no TIE written", ok);
    return bad;
}

static int len_play(void)
{
    int bad = 0;
    uint32_t n1, n3, off1, off3, p16 = div_samples(2), b16;
    uint8_t vel[8];
    track_t *t;
    t = roll(0);
    t->step[0] = (step_t){{60, 64}, 2, ST_NOTE, 0, 100};
    b16 = p16 / CTL;
    n1 = play(t, b16 * 6u, &off1, vel, 8);
    (void)note_set_len(t, 0, 3);
    n3 = play(t, b16 * 6u, &off3, vel, 8);
    printf("ui:   LEN 1: off at block %u, LEN 3: at block %u (a step %u blocks)\n", off1, off3, b16);
    bad += check("LEN 3 plays: one chord (2 note-ons), held through the TIEs to the end of the third step (LEN 1: its GATE)",
                 n1 == 2u && n3 == 2u && off1 < b16 && off3 > 2u * b16 + b16 / 2u && off3 <= 3u * b16 + 2u);
    return bad;
}

static int vel(void)
{
    int bad = 0, ok;
    uint32_t off, n;
    uint8_t v[8];
    uint16_t rw[EV_ROWS];
    step_t ref[NSTEP];
    track_t *t;

    t = roll(0);
    t->p[P_SLEN] = 1;
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 0};
    n = play(t, 8, &off, v, 8);
    ok = n == 1u && v[0] == 96u;
    t->step[0].vel = 50;
    n = play(t, 8, &off, v, 8);
    ok &= n == 1u && v[0] == 50u;
    t->step[0].flags = SF_ACCENT;
    n = play(t, 8, &off, v, 8);
    ok &= n == 1u && v[0] == 127u;
    bad += check("VEL plays: a step's 0 at 96, 50 at 50, an ACC step at 127", ok);
    t->step[0] = (step_t){{0}, 0, ST_NOTE, 0, 40, 3, 2};          /* hits on lanes 1, 2; lane 2 accented */
    n = play(t, 8, &off, v, 8);
    ok = n == 2u && ((v[0] == 40u && v[1] == 127u) || (v[0] == 127u && v[1] == 40u));
    bad += check("  a step's hits: the step's VEL, an accented one 127 (no velocity per lane)", ok);

    t = roll(0);
    for (n = 0; n < 16u; n += 4u)
        t->step[n] = (step_t){{(uint8_t)(48 + n)}, 1, ST_NOTE, 0, 100};   /* recorded from the panel's keys: 100 */
    t->step[8].flags = SF_ACCENT;
    t->step[20] = (step_t){{70}, 1, ST_NOTE, 0, 30};                     /* past LEN */
    memcpy(ref, t->step, sizeof ref);
    go_auto_top(); frame();
    ok = ev_list(rw) == 3u;
    go_auto_add(EV_VEL, 4); frame();
    ok &= str_eq(act_name(4), "SHOW");
    press(B_OCTUP);
    n = ev_list(rw);
    ok &= msg_is("VEL SHOWN") && n == 7u && rw[2] == EVC(EVK_VEL, 0) && rw[3] == EVC(EVK_VEL, 4) &&
          rw[5] == EVC(EVK_VEL, 12) && auto_cur() == EVC(EVK_VEL, 4) && !memcmp(t->step, ref, sizeof ref);
    bad += check("AUTOMATION: no VEL rows; + ADD VEL OCT+ (SHOW): one per note step inside LEN, nothing changed", ok);
    turn(EN_K4, -10);
    ok = t->step[4].vel == 90u && same_but(t, ref, 4, 4);
    frames(2000);
    turn(EN_K4, 100);
    ok &= t->step[4].vel == 127u;
    turn(EN_K2, 1);
    ok &= t->step[4].vel == 127u && t->step[5].vel == 0u && auto_cur() == EVC(EVK_VEL, 4);
    hold(B_SAVE);
    ok &= t->step[4].vel == 90u;
    bad += check("  KNOB 4 the step's velocity (1..127), KNOB 2 nothing; SAVE held: the turn undone", ok);
    press(B_EDIT);
    ok = t->step[4].vel == 0u && msg_is("VEL 96") && auto_cur() == EVC(EVK_VEL, 4) && ev_list(rw) == n;
    for (n = 0; n < 20u; n++)
        turn(EN_K4, -1);
    ok &= t->step[4].vel == 76u;
    bad += check("  EDIT: back to 96 (stored 0), the row stays", ok);
    {   /* drawn: DIM where it does not play (the ACC step) */
        ok = auto_pick(EVC(EVK_VEL, 8)) && !ev_vel_plays(&t->step[8]) && ev_vel_plays(&t->step[4]);
        bad += check("  an ACC step's VEL does not play (DIM)", ok);
    }
    ui.ev_row = 0xFFFFu;                              /* + ADD, on the page (go_auto_add enters it again) */
    ui.ev_id = EV_VEL;
    frame();
    ok = str_eq(act_name(4), "HIDE");
    press(B_OCTUP);
    ok &= msg_is("VEL HIDDEN") && ev_list(rw) == 3u;
    press(B_OCTUP);
    ok &= ev_list(rw) > 3u;
    go_home(); frame();
    go_auto_top(); frame();
    ok &= ev_list(rw) == 3u;
    bad += check("  HIDE hides them; leaving the page too", ok);
    return bad;
}

int main(void)
{
    int bad = len_edit() + len_play() + vel();
    printf("%s\n", bad ? "STEP 1.5 TEST FAILED" : "step 1.5 tests passed");
    return bad != 0;
}
