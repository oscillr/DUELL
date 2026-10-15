/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL 0.2.5 PERFORM mode (firmware/src/perform.c the sound, ui_perform.c the page; SPEC "PERFORM mode"). The UI sources against
 * ui_test.c's stubs and the sound through hostsim.c, as the firmware. Built and run by DUELL/host_tests.sh (name: perform).
 *   ENTRY     FX held 0.7 s: PERFORM over the page shown (no FX page); a tap of FX: back to that page (OVERVIEW too); held again: off;
 *             a tap outside PERFORM: the FX pages as ever; a white key pressed with FX held: PERFORM at once, that key its effect;
 *             HOME, a page button, the menu leave it; SELECT turns BALANCE there (no page); power-on: off
 *   KEYS      the 16 white keys: no voice, no MIDI out, nothing recorded, each its effect while held; the black keys stay notes; a key
 *             held from before PERFORM ends as the note it was; a PERFORM key let go after PERFORM ended: its effect off, no note-off
 *   KNOBS     K1 MORPH (the fader of the part(s) chosen), K2 SPACE, K3 FILTER, K4 DEPTH; SPACE and FILTER back to neutral when PERFORM
 *             is left, DEPTH kept; PRESETS = the sound of the part(s) chosen, SELECT = BALANCE; ALGORITHM PART 1 / PART 2 / BOTH (the badges, the MORPH TARGET)
 *   ROLL      ROLL A / B: the MORPH page's roll of that end, once a press, on the part(s) ALGORITHM chose
 *   MUTE      MUTE 1 / 2: the part silent while held (DUEL: its side only, the other bit for bit), P_MUTE untouched, back after
 *   LEDS      the white keys' map: an effect that can run at the mid level, a held key lit
 *   IDLE      PERFORM on and no key held, the knobs moved or not: the output bit for bit as without PERFORM; after a mute: skipped again
 * The effects, ported with the old FX layer's tests (perform_test.c up to 0.2.1), the stage alone on test signals:
 *   TIMING    REPEAT and REVERSE start on the next 1/16 of the transport (sample exact) and end when let go (the live signal back, bit
 *             for bit, after the 2.9 ms ramp); the others at once; stopped: at once; K4 DEPTH 0 %: the live signal
 *   STEREO    REPEAT, REVERSE, TAPE STOP, FREEZE keep left and right apart (DUEL: part 1 and part 2 each its own; a silent side silent)
 *   TOO LONG  a REPEAT / REVERSE longer than the 371 ms buffer at the tempo does nothing (1/8 and REVERSE below 81 BPM, 1/16 below 41,
 *             1/32 below 21); greyed on the page, dark on the keys, its key says why; PUMP with an external clock that does not run
 *   CLICKS    no click in or out, the live signal back bit for bit; 4x full scale: no wrap
 *   OCT       OCT UP / OCT DN: the pitch (2x / 0.5x within 3 cents on the sweep's grid), a silent side silent, no clicks, the shimmer
 *             (K4 DEPTH, 0.5 at 100 %) bounded and dying away, DEPTH 0: none
 *   SPACE     K2: the tails shorter to the left, longer to the right (HALL), the stored values untouched; every reverb and the delay at their
 *             largest with SPACE +100 die away (STEREO, DUEL); the delay's echo in its place (0.2.9: SPACE keeps the delay time); DUEL's other
 *             side silent; turning it no click; back in the middle: the buses' own values again; 0.2.9: a tone into each bus, SPACE turned to
 *             +100, no seam at the blocks' starts (the chorus's depth, SPRING's SIZE glide between samples); RESO's combs off their limit;
 *             the cost of SPACE +100 per reverb model, STEREO and DUEL
 *   COST      the song's instructions a sample, idle and with each effect (proc_pid_rusage) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <sys/wait.h>
#include <math.h>

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("perform: %-100s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}

static void fresh(void)
{
    ui_power_on();
    frame();
}
static void settle(void) { frames(600); }
static uint32_t wkey(uint32_t p) { return key_at(0, p); }   /* white key p (0 = F3) */
static int card_is(uint32_t c, const char *label, const char *val)   /* the card's cache key: "LABEL|VALUE|UNIT..." */
{
    char want[40];
    snprintf(want, sizeof want, "%s|%s|", label, val);
    return !strncmp(ui.col[c], want, strlen(want));
}
static const char *title_now(void) { return cur_page()->title; }
static int under_is(const char *t) { return str_eq(PAGES[ui.page].title, t); }
static void perform_on(void) { hold(B_FX); frame(); }

/* ---------------------------------------------------------------- ENTRY --- */
static void t_entry(void)
{
    char tb[24];
    uint32_t pi;
    fresh();
    go_title("AMP ENV");
    frames(32);
    hold(B_FX);                                         /* (held 0.83 s) */
    frame();
    head_page_title(tb, &pi);
    expect("FX held 0.7 s on AMP ENV: PERFORM, titled PERFORM in the header, over AMP ENV (no FX page)",
           ui.perf && cur_page()->scope == SC_PERF && str_eq(tb, "PERFORM") && under_is("AMP ENV"));
    frames(200);
    expect("  .. it stays on after FX is let go", ui.perf && str_eq(title_now(), "PERFORM"));
    press(B_FX);
    frames(32);
    expect("  a tap of FX: back to AMP ENV, PERFORM off", !ui.perf && str_eq(title_now(), "AMP ENV"));
    press(B_FX);
    frames(32);
    expect("a tap of FX outside PERFORM: the FX pages as ever", !ui.perf && str_eq(title_now(), "FX"));
    btn_down(B_FX);
    frames(500);
    btn_up(B_FX);
    frames(32);
    expect("  FX held 0.5 s (under the hold): a tap, the next FX page", !ui.perf && str_eq(title_now(), "INSERT 1/2"));
    perform_on();
    hold(B_FX);
    frames(32);
    expect("FX held again in PERFORM: off, back to the page under it", !ui.perf && str_eq(title_now(), "INSERT 1/2"));
    go_main_to(HP_MORPH);
    frames(32);
    perform_on();
    expect("from HOME (MORPH): PERFORM (MORPH not shown under it)", ui.perf && !at_home() && str_eq(title_now(), "PERFORM"));
    press(B_FX);
    frames(32);
    expect("  a tap of FX: HOME (MORPH) again", !ui.perf && at_home());
    go_title("ENV 2");
    perform_on();
    press(B_HOME);
    frames(32);
    expect("HOME in PERFORM: off, HOME's page (MORPH)", !ui.perf && str_eq(title_now(), "MORPH"));
    go_title("LFO 1");
    perform_on();
    press(B_ENV);
    frames(32);
    expect("ENV in PERFORM: off, the ENV pages", !ui.perf && cur_page()->fam == FAM_ENV);
    perform_on();
    turn(EN_SELECT, 1);
    turn(EN_SELECT, -3);
    frames(32);
    expect("SELECT turned in PERFORM: no other page, PERFORM stays (it is BALANCE there)", ui.perf && str_eq(title_now(), "PERFORM") && mix_bal == -2);
    mix_bal = 0;
    {
        char ti[16], ct[8];
        page_parts(ti, ct);
        expect("  no page number in the footer (PERFORM is on no family's round)", !ct[0] && str_eq(ti, "PERFORM"));
    }
    hold(B_HOME);
    frames(32);
    expect("HOME held in PERFORM: the menu (PERFORM under it, its keys notes again)", ui.menu && !perf_keys);
    press(B_HOME);
    frames(32);
    expect("  the menu closed: HOME's page, PERFORM off", !ui.menu && !ui.perf && !perf_keys);
    fresh();
    expect("power-on: PERFORM off, the knobs neutral, DEPTH 100 %", !ui.perf && !perf_keys && !perf_space && !perf_filt && perf_depth == 100);
    {   /* a white key pressed with FX held: PERFORM at once, the key its effect, FX's release no page */
        uint32_t k = wkey(PF_R16);
        go_title("AMP ENV");
        frames(32);
        btn_down(B_FX);
        frame();
        key_down(k);
        frame();
        expect("a white key pressed with FX held: PERFORM at once, the key holds its effect (REPEAT 1/16)",
               ui.perf && ((kb_perf >> k) & 1u) && (perf_held & PF_BIT(PF_R16)));
        btn_up(B_FX);
        frames(64);
        key_up(k);
        frames(64);
        expect("  FX let go: PERFORM stays, no FX page; the key let go: its effect off",
               ui.perf && under_is("AMP ENV") && !perf_held && !kb_perf);
        press(B_FX);
        frames(32);
        expect("  a tap of FX: back to AMP ENV", !ui.perf && str_eq(title_now(), "AMP ENV"));
    }
}

/* ----------------------------------------------------------------- KEYS --- */
static void t_keys(void)
{
    uint32_t p, k, ok = 1, mo0;
    fresh();
    go_title("AMP ENV");
    perform_on();
    usb.config = 1;
    mo_r = mo_w = 0;
    for (p = 0; p < PF_KEYS; p++) {
        k = wkey(p);
        key_down(k);
        events_block(CTL);
        ok &= !busy_now() && mo_w == 0u && ((kb_perf >> k) & 1u) && kb_note[k] == KB_SILENT &&
              (p == PF_ROLLA || p == PF_ROLLB ? !perf_held : perf_held == PF_BIT(p));
        key_up(k);
        events_block(CTL);
        ok &= !kb_perf && !perf_held && mo_w == 0u;
    }
    frame();
    expect("the 16 white keys in PERFORM: no voice, no MIDI out, each its effect while held (ROLL: none held)", ok);
    k = key_at(1, 2);                                   /* a black key: a note, sent */
    key_down(k);
    events_block(CTL);
    ok = busy_now() > 0u && mo_w == 1u && !kb_perf;
    key_up(k);
    events_block(CTL);
    expect("  a black key: a note as ever (voice, MIDI note-on and note-off)", ok && mo_w == 2u);
    press(B_FX);
    frame();
    k = wkey(3);
    key_down(k);                                        /* a key held before PERFORM: a note, its note-off after */
    perform_on();
    mo0 = mo_w;
    key_up(k);
    expect("a key held from before PERFORM: the note it was, its note-off sent", mo_w == mo0 + 1u && !kb_perf && !perf_held);
    k = wkey(PF_FRZ);
    key_down(k);
    press(B_FX);                                        /* PERFORM off with FREEZE held */
    frame();
    mo0 = mo_w;
    ok = !ui.perf && (perf_held & PF_BIT(PF_FRZ));
    key_up(k);
    expect("  a PERFORM key let go after PERFORM ended: its effect off, no note-off", ok && !perf_held && mo_w == mo0);
    {   /* nothing recorded: REC armed, playing */
        uint32_t h0;
        perform_on();
        song.rec = 1;
        transport_req = 1;
        events_block(CTL);
        h0 = steps_hash(&trk[0]);
        for (p = 0; p < 8u; p++) {
            key_down(wkey(p));
            events_block(CTL);
            key_up(wkey(p));
            events_block(CTL);
        }
        expect("  REC armed and playing: PERFORM's keys record nothing", steps_hash(&trk[0]) == h0);
        song.rec = 0;
        transport_req = 2;
        events_block(CTL);
    }
    usb.config = 0;
}

/* ---------------------------------------------------------------- KNOBS --- */
static void t_knobs(void)
{
    int16_t pre;
    fresh();
    go_title("AMP ENV");
    perform_on();
    turn(EN_K1, 5);
    frame();
    expect("K1 MORPH: part 1's fader (the MORPH page's), the card MORPH", morph_fader_of(0) == 5u && morph_fader_of(1) == 0u &&
                                                                           card_is(0, "MORPH", "5"));
    turn(EN_K2, 3);
    turn(EN_K3, -2);
    turn(EN_K4, -10);
    frame();
    expect("K2 SPACE +3, K3 FILTER 2 LP, K4 DEPTH 90 %: on their cards",
           perf_space == 3 && perf_filt == -2 && perf_depth == 90 && card_is(1, "SPACE", "+3") && card_is(2, "KILL", "2") &&
           card_is(3, "DEPTH", "90"));
    expect("  DUEL (the power-on MODE): SPACE turned says DUEL: NO WIDTH once (its width cannot work there; the rest of SPACE does)",
           duell_mode == DM_SPLIT && msg_is("DUEL: NO WIDTH") && perf_nw_said);
    {
        uint8_t dm = duell_mode;
        ui.msg_t = 0;
        turn(EN_K2, 1);
        frame();
        expect("  .. turned again: not said again", !msg_is("DUEL: NO WIDTH") && perf_space == 4);
        duell_mode_force(DM_STEREO);
        perf_nw_said = 0;
        turn(EN_K2, -1);
        frame();
        expect("  STEREO: SPACE turned, no message (the width works there)", !msg_is("DUEL: NO WIDTH") && perf_space == 3);
        duell_mode_force(dm);
    }
    {   /* (0.2.9, owner) PRESETS is the sound again, on the part ALGORITHM chose; SELECT is BALANCE */
        static uint16_t shot[240 * H_FOOT];
        uint32_t e0 = trk[0].eng_req, p0 = trk[0].preset, e1 = trk[1].eng_req, q1 = trk[1].preset;
        char l[24], r[24];
        mix_bal = 0;
        ui.force = 1;
        frame();
        memcpy(shot, &host_screen[Y_FOOT * 240], sizeof shot);
        turn(EN_PRESET, 1);
        frame();
        expect("PRESETS in PERFORM: part 1's sound changes, balance untouched, part 2 untouched",
               ui.perf && (trk[0].eng_req != e0 || trk[0].preset != p0) && mix_bal == 0 && trk[1].eng_req == e1 && trk[1].preset == q1);
        ui.force = 1;
        frame();
        expect("  the footer shows the new patch name (row 2 as on every page)", memcmp(shot, &host_screen[Y_FOOT * 240], sizeof shot) != 0);
        turn(EN_SELECT, 2);
        frame();
        expect("SELECT in PERFORM: BALANCE +2, no sound loaded, still PERFORM", mix_bal == 2 && ui.perf &&
                                                                                 trk[1].eng_req == e1 && trk[1].preset == q1);
        expect("  the message says BALANCE 2 (as the MODE page)", msg_is("BALANCE 2"));
        expect("  the footer says it: SELECT: BALANCE +2, ALGO: PART 1",
               perf_foot(l, r) && str_eq(l, "SELECT: BALANCE +2") && str_eq(r, "ALGO: PART 1"));
        turn(EN_SELECT, -5);
        expect("  SELECT left 5: -3 (steps of 1, as the MODE page)", mix_bal == -3 && ui.perf);
        turn(EN_SELECT, 3);
        expect("  back to 0, a stray SELECT turn never ends PERFORM", mix_bal == 0 && ui.perf);
        turn(EN_SELECT, 2);
    }
    turn(EN_ALGO, 1);                                   /* (0.2.9: PART 1, BOTH, PART 2 from left to right) */
    expect("ALGORITHM right: BOTH (both badges, the MORPH TARGET; part 1 still the selected part)", song.sel == 0u &&
                                                                    morph.target == MT_BOTH && badge_lit() == 3u && msg_is("BOTH"));
    turn(EN_ALGO, 1);
    expect("  right again: PART 2 (selected, one badge)", song.sel == 1u && morph.target != MT_BOTH && badge_lit() == 2u);
    turn(EN_ALGO, 1);
    expect("  right at PART 2: PART 2", song.sel == 1u && morph.target != MT_BOTH);
    turn(EN_ALGO, -1);
    expect("  left: BOTH, part 2 kept as the selected part", song.sel == 1u && morph.target == MT_BOTH && badge_lit() == 3u);
    {
        char l[24], r[24];
        expect("  the footer: ALGO: BOTH", perf_foot(l, r) && str_eq(r, "ALGO: BOTH"));
    }
    turn(EN_K1, 7);
    expect("  K1 MORPH with BOTH: both parts' faders (part 2's + 7 on both)", morph_fader_of(0) == 7u && morph_fader_of(1) == 7u);
    {   /* BOTH: PRESETS moves each part one step from its own sound (they stay different) */
        uint32_t e0 = trk[0].eng_req, p0 = trk[0].preset, e1 = trk[1].eng_req, q1 = trk[1].preset;
        turn(EN_PRESET, 1);
        frame();
        expect("  PRESETS with BOTH: both parts moved on from their own sound, selection kept",
               (trk[0].eng_req != e0 || trk[0].preset != p0) && (trk[1].eng_req != e1 || trk[1].preset != q1) && song.sel == 1u && mix_bal == 2);
        expect("  .. and they are still different sounds",
               trk[0].eng_req != trk[1].eng_req || trk[0].preset != trk[1].preset);
        turn(EN_PRESET, -1);
        frame();
        expect("  PRESETS back: each part on its own first sound again", trk[0].eng_req == e0 && trk[0].preset == p0 &&
                                                                       trk[1].eng_req == e1 && trk[1].preset == q1);
    }
    turn(EN_ALGO, -1);
    expect("  left: PART 1", song.sel == 0u && morph.target != MT_BOTH && badge_lit() == 1u);
    turn(EN_ALGO, -1);
    expect("  left at PART 1: PART 1", song.sel == 0u && morph.target != MT_BOTH);
    turn(EN_ALGO, 1);                                   /* BOTH, then out of PERFORM */
    press(B_FX);
    frame();
    frames(640);
    expect("  .. 0.64 s later SPACE and FILTER are neutral, DEPTH kept (90 %)", perf_space == 0 && !perf_filt && perf_depth == 90);
    perf_space = 3;
    expect("  ALGORITHM's BOTH kept (the MORPH page's state); this page shows the selected part's badge alone",
           morph.target == MT_BOTH && song.sel == 0u && badge_lit() == 1u);
    turn(EN_ALGO, 1);
    expect("  ALGORITHM on another page: PART 2 only, BOTH kept for MORPH / PERFORM", song.sel == 1u && morph.target == MT_BOTH && badge_lit() == 2u);
    turn(EN_ALGO, 1);
    expect("  .. right again: still PART 2 (no BOTH there)", song.sel == 1u && morph.target == MT_BOTH);
    turn(EN_ALGO, -2);
    turn(EN_ALGO, -1);
    expect("  .. left: PART 1", song.sel == 0u && morph.target == MT_BOTH);
    perform_on();
    frame();
    expect("  PERFORM again: BOTH as it was left (both badges)", morph.target == MT_BOTH && badge_lit() == 3u);
    turn(EN_ALGO, -1);
    press(B_FX);
    frame();
    {   /* 0.2.9: the MIXER's third card is the same SPACE */
        int16_t rev = TSEL->p[P_REV];
        perf_space = 3;
        for (int hp = 0; hp < 8 && (at_home() || cur_page()->graph != GR_TRK); hp++)
            press(B_HOME);
        frame();
        expect("MIXER: K3 is the SPACE card with PERFORM's value (+3)", card_is(2, "SPACE", "+3"));
        turn(EN_K3, 2);
        frame();
        expect("  K3 moves the same value (+5), the card says it, the part's REV send stays", perf_space == 5 && card_is(2, "SPACE", "+5") && TSEL->p[P_REV] == rev);
        turn(EN_K3, -5);
        frame();
        expect("  back to 0: the card says 0 (not greyed, VAL colour)", perf_space == 0 && card_is(2, "SPACE", "0"));
        perf_space = 3;
    }
    turn(EN_PRESET, 1);
    expect("  balance kept after PERFORM (not saved into patches)", mix_bal == 2);
}

/* (0.2.11, owner) a multi load resets the performance macros (SPACE, FILTER) to neutral; a patch load keeps SPACE */
static void t_multiload(void)
{
    int32_t o[2 * CTL];
    fresh();
    perform_on();
    frame();
    perf_space = 66; perf_filt = 30; perf_depth = 90; sv.spc = 66 * 256; perf_sp_live = 1;
    turn(EN_PRESET, 1);
    frame();
    expect("PRESETS in PERFORM (PERF LEAVE FADE): a patch load starts the glide, not a jump (0.2.14)", perf_space < 66 && perf_space > 0 && perf_filt < 30 && perf_filt > 0);
    frames(640);
    expect("  .. SPACE 0 and FILTER 0 after it, DEPTH kept (90 %)", perf_space == 0 && perf_filt == 0 && perf_depth == 90);
    perf_space = 66; perf_filt = 30; sv.spc = 66 * 256; perf_sp_live = 1;
    project_load(0);
    expect("multi load: the same glide, not a jump", perf_space > 0 && perf_filt > 0 && perf_depth == 90);
    frames(640);
    expect("  .. SPACE 0 and FILTER 0 (DEPTH kept)", perf_space == 0 && perf_filt == 0 && perf_depth == 90);
    perf_space = 66; perf_filt = 30; sv.spc = 66 * 256; perf_sp_live = 1; ui_idle |= PERF_KEEP_BIT;
    turn(EN_PRESET, 1);
    frames(640);
    expect("PERF LEAVE KEEP: a patch load keeps SPACE (66) and FILTER (30)", perf_space == 66 && perf_filt == 30);
    project_load(0);
    frames(640);
    expect("  .. a multi load too", perf_space == 66 && perf_filt == 30);
    ui_idle &= (uint8_t)~PERF_KEEP_BIT;
    perf_space = 0; perf_filt = 0; sv.spc = 0; perf_sp_live = 0; perf_fading = 0;
    (void)o;
}


/* ----------------------------------------------------------------- ROLL --- */
static uint32_t rolls(uint32_t pi, uint32_t e) { return mph[pi].n[e]; }
static void t_roll(void)
{
    uint32_t a0, a1, b0, b1, k;
    fresh();
    go_title("AMP ENV");
    perform_on();
    frame();
    morph_ready(0);
    morph_ready(1);
    a0 = rolls(0, 0);
    a1 = rolls(1, 0);
    k = wkey(PF_ROLLA);
    key_down(k);
    frame();
    frames(200);                                        /* (held: no more rolls) */
    key_up(k);
    frame();
    expect("ROLL A (PART 1): part 1's end A rolled once, part 2 untouched", rolls(0, 0) == a0 + 1u && rolls(1, 0) == a1 &&
                                                                             msg_is("ROLLED A"));
    turn(EN_ALGO, 1);                                   /* BOTH (0.2.9: the middle) */
    b0 = rolls(0, 1);
    b1 = rolls(1, 1);
    k = wkey(PF_ROLLB);
    key_down(k);
    key_up(k);
    key_down(k);
    key_up(k);                                          /* (two quick presses in one pass: two rolls) */
    frame();
    expect("ROLL B with BOTH, pressed twice: both parts' end B rolled twice", rolls(0, 1) == b0 + 2u && rolls(1, 1) == b1 + 2u);
}

/* ------------------------------------------------------------ the song --- */
/* part B of the perform test's song: DIGITAL PAD (preset 5) as converted to FM6 by the 0.2.13 firmware (fm4_convert.c): the track's
 * values and the converted patch, so the song (and the cost thresholds read from it) stay as they were */
static const int16_t SONG_B_P[P_COUNT] = {104,80,90,110,95,0,0,0,0,60,0,0,0,0,0,0,0,0,2,1,64,0,127,0,0,0,0,0,0,16,2,0,64,0,60,30,70,0,0,0,0,0,0,0,40,82,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,70,90,60,60,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,1,0,0,64,96,96,127,0,0,0,0,1,0,0,5,0,36,0,0,30,0,0,0,0,0,0,0,0,0,0,0,0,4};
static const uint8_t SONG_B_PATCH[FP_SIZE] = {99,46,99,46,99,82,82,82,39,0,0,0,0,0,0,1,86,0,3,0,7,43,25,99,46,99,96,96,0,39,0,0,0,0,0,0,3,91,0,1,0,7,43,25,99,46,99,96,96,0,39,0,0,0,0,0,0,3,91,0,2,0,7,43,25,99,46,99,96,96,0,39,0,0,0,0,0,0,3,91,0,1,0,7,99,99,99,99,0,0,0,0,39,0,0,0,0,0,0,0,0,0,1,0,7,99,99,99,99,0,0,0,0,39,0,0,0,0,0,0,0,0,0,1,0,7,99,99,99,99,50,50,50,50,21,7,1,35,0,0,0,0,0,0,24,80,65,68,32,32,32,32,32,32,32};
static const uint8_t SONG_B_PRESET = 4, SONG_B_SLOT = 4;
static void song_setup(void)
{
    static const uint8_t ACID[16] = {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50};
    static const uint8_t AM[4] = {57, 60, 64, 67};
    track_t *t1 = &trk[0], *t2 = &trk[1];
    uint32_t i;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    song.g[G_BPM] = 120;
    host_preset(t1, 0, 4);
    host_preset(t2, ENGI_FM6, 4);
    {   /* part B exactly as the song had it up to 0.2.13 (the cost thresholds below read its content) */
        uint8_t v[FP_SIZE + 1u] = {0};
        memcpy(t2->p, SONG_B_P, sizeof SONG_B_P);
        t2->p[P_TRIM] = SONG_B_P[P_LEVEL];                  /* (0.2.19: the image's id 0 was the one level, the sound's trim now; the legacy DUCK HOLD's id is the TRIM) */
        t2->p[P_LEVEL] = TP[P_LEVEL].def;
        memcpy(v, SONG_B_PATCH, FP_SIZE);
        fm6_set_patch(1, v);
        fm6_slot[1] = SONG_B_SLOT;
        t2->preset = SONG_B_PRESET;
    }
    for (i = 0; i < 16u; i++) {
        uint8_t n = ACID[i];
        put_step(t1, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, 0);
        put_step(t2, i, i % 8u == 0u ? 4u : 0u, AM, i % 8u == 0u ? ST_NOTE : ST_TIE, 0);
    }
    trk[0].p[P_PAN] = -40;
    trk[1].p[P_PAN] = 40;
}
static int32_t song_l[10u * 44100u + CTL], song_r[10u * 44100u + CTL];
static void song_render(uint32_t frames_n, void (*at)(uint32_t t))
{
    uint32_t t, i;
    int32_t o[2 * CTL];
    transport_req = 1;
    for (t = 0; t < frames_n; t += CTL) {
        if (at)
            at(t);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            song_l[t + i] = o[2 * i];
            song_r[t + i] = o[2 * i + 1];
        }
    }
    transport_req = 2;
    mix_block(o, CTL);
}
/* render in a child with prep() applied first, compare with the parent's render (prep 0): the samples that differ, -1 failed */
static int32_t fork_diff(void (*prep)(void), void (*parent_prep)(void), uint32_t frames_n, void (*at)(uint32_t t))
{
    static int32_t b_l[10u * 44100u + CTL], b_r[10u * 44100u + CTL];
    int fd[2];
    pid_t pid;
    uint32_t t;
    int32_t diff = 0;
    if (pipe(fd) || (pid = fork()) < 0)
        return -1;
    if (!pid) {
        song_setup();
        if (prep)
            prep();
        song_render(frames_n, at);
        close(fd[0]);
        if (write(fd[1], song_l, frames_n * 4u) < 0 || write(fd[1], song_r, frames_n * 4u) < 0)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    song_setup();
    if (parent_prep)
        parent_prep();
    song_render(frames_n, at);
    {
        FILE *f = fdopen(fd[0], "rb");
        size_t got = fread(b_l, 4, frames_n, f) + fread(b_r, 4, frames_n, f);
        fclose(f);
        waitpid(pid, 0, 0);
        if (got != 2u * frames_n)
            return -1;
    }
    for (t = 0; t < frames_n; t++)
        diff += b_l[t] != song_l[t] || b_r[t] != song_r[t];
    return diff;
}
static int32_t peak(const int32_t *x, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    for (; a < b; a++)
        m = abs(x[a]) > m ? abs(x[a]) : m;
    return m;
}

/* ----------------------------------------------------------------- MUTE --- */
static uint32_t mute_part;
static void mute_at(uint32_t t)
{
    if (t == 44032u)
        perf_press(PF_M1 + mute_part, 1);
    if (t == 88064u)
        perf_press(PF_M1 + mute_part, 0);
}
static void t_mute(void)
{
    static int32_t c_l[3u * 44100u], c_r[3u * 44100u];
    int32_t before, during, after;
    uint32_t t, rdiff = 0, n = 3u * FS;
    int fd[2];
    pid_t pid;
    char what[160];
    ui_power_on();
    if (pipe(fd) || (pid = fork()) < 0) {
        expect("MUTE: fork", 0);
        return;
    }
    if (!pid) {                                         /* the child: MUTE 1 held 1 .. 2 s */
        song_setup();
        trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;   /* (part 1 dry: its tails would ring on, as they should) */
        duell_mode_force(DM_SPLIT);                     /* DUEL: part 1 left, part 2 right */
        mute_part = 0;
        song_render(n, mute_at);
        close(fd[0]);
        if (write(fd[1], song_l, n * 4u) < 0 || write(fd[1], song_r, n * 4u) < 0 ||
            write(fd[1], (const void *)&pf.mg[0], 4) < 0 || write(fd[1], (const void *)&trk[0].p[P_MUTE], 2) < 0)
            _exit(1);
        _exit(perf_idle() && !pf.mute ? 0 : 3);
    }
    close(fd[1]);
    song_setup();
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    duell_mode_force(DM_SPLIT);
    song_render(n, 0);                                  /* the parent: no mute */
    {
        FILE *f = fdopen(fd[0], "rb");
        int32_t mg = 0;
        int16_t pm = 1;
        int st = 0;
        size_t got = fread(c_l, 4, n, f) + fread(c_r, 4, n, f) + fread(&mg, 4, 1, f) + fread(&pm, 2, 1, f);
        fclose(f);
        waitpid(pid, &st, 0);
        before = peak(c_l, 22050, 44032);
        during = peak(c_l, 44032 + 512, 88064);
        after = peak(c_l, 88064 + 4410, n);
        for (t = 0; t < n; t++)
            rdiff += c_r[t] != song_r[t];
        snprintf(what, sizeof what, "MUTE 1 held (DUEL, part 1 dry): part 1's side silent while held (peak %d -> %d -> %d), P_MUTE untouched",
                 before, during, after);
        expect(what, got == 2u * n + 2u && before > 1000 && during < 64 && after > 1000 && !pm);
        expect("  part 2's side bit for bit as without the mute", rdiff == 0u);
        expect("  let go and the ramp over: the gain open, PERFORM's stages skipped again", mg == 32768 && WIFEXITED(st) && !WEXITSTATUS(st));
    }
}

/* ----------------------------------------------------------------- IDLE --- */
static void idle_prep(void)                              /* PERFORM on, no key held, the knobs moved (DEPTH), or not */
{
    perf_keys = 1;
    ui.perf = 1;
    perf_depth = 33;
}
static void t_idle(void)
{
    int32_t d = fork_diff(idle_prep, 0, 3u * FS, 0);
    expect("idle: PERFORM on, no key held (DEPTH moved): the song bit for bit as without PERFORM", d == 0);
    perf_keys = 0;
    ui.perf = 0;
    perf_depth = 100;
}

/* ----------------------------------------------------------------- LEDS --- */
static int led_on(const uint8_t *a, uint32_t id)
{
    uint8_t q = led_pos[id];
    return q != 0xFF && ((a[q >> 3] >> (q & 7u)) & 1u);
}
static void t_leds(void)
{
    uint32_t k, mi, ok = 1, kh = wkey(PF_DRV);
    uint8_t pos0[sizeof led_pos];
    fresh();
    go_title("AMP ENV");
    perform_on();
    key_down(kh);
    frame();
    {
        ui_leds();                                      /* (its positions made once, then ours: a place per key) */
        memcpy(pos0, led_pos, sizeof pos0);
        memset(led_pos, 0xFF, sizeof led_pos);
        for (k = 0; k < 27u; k++)
            led_pos[14u + k] = (uint8_t)((2u + k / 8u) << 3 | (k % 8u));
        ui_leds();
    }
    for (mi = 0; mi < LEDS_COUNT; mi++) {          /* every LEDS mode: the same picture (MAX does not invert it) */
      settings_leds = (uint8_t)mi;
      ui_leds();
    for (k = 0; k < 27u; k++) {
        int white = !key_black(k), o;
        if (k == kh)
            o = led_on(fm1_led, 14u + k) && !led_on(fm1_led_scl, 14u + k);
        else   /* 0.2.11: only the row markers (places 0, 4, 8, 12) at the in-scale level, steady; the rest dark */
            o = led_on(fm1_led_scl, 14u + k) == (white && key_place(k) < PF_KEYS && !(key_place(k) & 3u)) && !led_on(fm1_led, 14u + k) &&
                !led_on(fm1_led_mid, 14u + k) && !led_on(fm1_led_dim, 14u + k);
        if (!o)
            printf("perform: LED of key %u: lit %d mid %d\n", k, led_on(fm1_led, 14u + k), led_on(fm1_led_mid, 14u + k));
        ok &= o;
    }
    }
    key_up(kh);
    memcpy(led_pos, pos0, sizeof pos0);
    expect("LEDS in PERFORM (0.2.11): the 1st, 5th, 9th, 13th white key steady at the in-scale level (row markers), the held one (DRIVE) lit, all else dark, in every LEDS mode", ok);
}

/* ============================================================ the effects (the stage alone on a test signal) --- */
#define NT (6u * 44100u)
static int32_t in_l[NT], in_r[NT], out_l[NT], out_r[NT];
static void test_signal(double fl, double al, double fr, double ar)
{
    uint32_t t;
    for (t = 0; t < NT; t++) {
        in_l[t] = (int32_t)lrint(al * sin(2 * M_PI * fl * t / FS));
        in_r[t] = (int32_t)lrint(ar * sin(2 * M_PI * fr * t / FS + 1.0));
    }
}
static void perf_reset(void)
{
    memset(&pf, 0, sizeof pf);
    pf.src = pf.next = PF_N;
    pf.pbeat = perf_beat;
    pf.mg[0] = pf.mg[1] = pf.mg[2] = pf.mg[3] = 32768;
    perf_held = perf_act = 0;
    perf_kill = 0;
    perf_space = perf_filt = 0;
    perf_depth = 100;
}
/* the stage alone: samples [0, t1); ev: press (+e + 1) / let go (-(e + 1)) at sample t (a block's start) */
typedef struct { uint32_t t; int e; } ev_t;
static int busy_seen;
static int8_t run_depth = 100;                         /* K4 DEPTH during run() */
static int8_t run_filt;                                /* K3 FILTER during run() */
static void (*run_at)(uint32_t t);                     /* (a test's own change at a block) */
static void run(uint32_t bpm, int playing, const ev_t *ev, uint32_t nev, uint32_t t1)
{
    uint32_t t, k = 0, bt = 0;
    host_tracks_init();
    perf_reset();
    perf_depth = run_depth;
    perf_filt = run_filt;
    song.g[G_BPM] = (int16_t)bpm;
    song.playing = (uint8_t)playing;
    perf_start();
    bt = beat_samples();                                /* (perf_start: a beat at 0; the next ones as seq.c counts them) */
    busy_seen = 0;
    for (t = 0; t < t1; t += CTL) {
        while (k < nev && ev[k].t <= t) {
            perf_press((uint32_t)(ev[k].e > 0 ? ev[k].e - 1 : -ev[k].e - 1), ev[k].e > 0);
            k++;
        }
        if (t >= bt) {                                  /* (seq.c events_block: the beat in this block, before the mix) */
            perf_beat++;
            bt += beat_samples();
        }
        if (run_at)
            run_at(t);
        memcpy(out_l + t, in_l + t, CTL * 4);
        memcpy(out_r + t, in_r + t, CTL * 4);
        if (perf_idle())
            perf_tick(CTL);
        else if (perf_begin(CTL)) {
            busy_seen = 1;
            if ((pf.act & PF_BIT(PF_DRV)) || pf.dw)
                perf_drive(out_l + t, out_r + t, CTL);   /* (fx.c perf_master: DRIVE before the level, here at 1) */
            perf_block(out_l + t, out_r + t, CTL);
        }
    }
    run_filt = 0;
}
#define P16 5512u                                  /* a 1/16 at 120 BPM */
static int same(uint32_t a, uint32_t b)            /* out == in on [a, b), both sides */
{
    uint32_t t;
    for (t = a; t < b; t++)
        if (out_l[t] != in_l[t] || out_r[t] != in_r[t])
            return 0;
    return 1;
}
static int32_t err_vs(uint32_t a, uint32_t b, int32_t (*src)(const int32_t *, uint32_t), int side)
{
    uint32_t t;
    int32_t e = 0;
    for (t = a; t < b; t++) {
        int32_t d = abs((side ? out_r : out_l)[t] - src(side ? in_r : in_l, t));
        e = d > e ? d : e;
    }
    return e;
}
static uint32_t shift_d;                           /* the loop: in[t - shift_d] */
static int32_t delayed(const int32_t *x, uint32_t t) { return x[t - shift_d]; }
static uint32_t rev_a, rev_len, rev_t0;            /* REVERSE: in[rev_a + rev_len - 1 - (t - rev_t0) % rev_len] */
static int32_t reversed(const int32_t *x, uint32_t t) { return x[rev_a + rev_len - 1u - (t - rev_t0) % rev_len]; }

/* ------------------------------------------------------------ TIMING --- */
static void t_timing(void)
{
    char what[160];
    test_signal(440, 12000, 277, 9000);
    {   /* REPEAT 1/16: pressed mid-1/16, records the next 1/16, then loops it */
        ev_t ev[] = {{992, PF_R16 + 1}, {20000, -(PF_R16 + 1)}};
        int32_t el, er, el2;
        run(120, 1, ev, 2, 40000);
        shift_d = P16;
        el = err_vs(2 * P16 + 160, 3 * P16 - 160, delayed, 0);
        er = err_vs(2 * P16 + 160, 3 * P16 - 160, delayed, 1);
        shift_d = 2 * P16;
        el2 = err_vs(3 * P16 + 160, 20000 - 160, delayed, 0);
        expect("REPEAT 1/16: live until the 1/16 after the press + one 1/16 (exact)", same(0, 2 * P16));
        snprintf(what, sizeof what, "  then the 1/16 from that 1/16, looped (error L %d R %d, 2nd pass %d of 12000)", el, er, el2);
        expect(what, el < 100 && er < 100 && el2 < 100);
        expect("  let go: the live signal again after the ramp, bit for bit", same(20000 + 160, 40000));
        expect("  the buffer given back, nothing left running", pf.mode == BM_NONE && !pf.busy);
    }
    {   /* REVERSE: the next 1/8 recorded, then played backwards */
        ev_t ev[] = {{992, PF_REV + 1}, {40000, -(PF_REV + 1)}};
        int32_t el, er;
        run(120, 1, ev, 2, 50000);
        rev_a = P16;
        rev_len = 11025;
        rev_t0 = P16 + rev_len;
        el = err_vs(rev_t0 + 160, rev_t0 + rev_len - 160, reversed, 0);
        er = err_vs(rev_t0 + 160, rev_t0 + rev_len - 160, reversed, 1);
        expect("REVERSE: live until the 1/16 after the press + a 1/8 (exact)", same(0, rev_t0));
        snprintf(what, sizeof what, "  then that 1/8 backwards (error L %d R %d of 12000)", el, er);
        expect(what, el < 100 && er < 100);
        expect("  let go: live again", same(40000 + 160, 50000));
    }
    {   /* stopped: at once */
        ev_t ev[] = {{992, PF_R16 + 1}};
        run(120, 0, ev, 1, 12000);
        expect("stopped: a 1/16 effect starts at once", (perf_act & PF_BIT(PF_R16)) && !same(992, 12000));
    }
    {   /* the immediate ones: TAPE STOP moves at once */
        ev_t ev[] = {{992, PF_TAPE + 1}};
        run(120, 1, ev, 1, 3000);
        expect("TAPE STOP starts at once (not on the 1/16)", same(0, 992) && (perf_act & PF_BIT(PF_TAPE)));
    }
    {   /* DEPTH: REPEAT's level (0 %: live) */
        ev_t ev[] = {{0, PF_R16 + 1}};
        int32_t d = 0;
        uint32_t t;
        test_signal(330, 12000, 220, 12000);
        run_depth = 0;
        run(120, 1, ev, 1, 20000);
        run_depth = 100;
        for (t = 0; t < 20000u; t++)
            d = abs(out_l[t] - in_l[t]) > d ? abs(out_l[t] - in_l[t]) : d;
        snprintf(what, sizeof what, "K4 DEPTH 0 %%: REPEAT held, the live signal (within %d of 12000)", d);
        expect(what, d == 0);
    }
}

/* ------------------------------------------------------------ STEREO --- */
static void t_stereo(void)
{
    static const struct { const char *name; int e; } C[] = {
        {"REPEAT 1/8", PF_R8}, {"REVERSE", PF_REV}, {"TAPE STOP", PF_TAPE}, {"FREEZE", PF_FRZ}};
    uint32_t c;
    char what[160];
    for (c = 0; c < 4u; c++) {
        ev_t ev[] = {{992, C[c].e + 1}, {60000, -(C[c].e + 1)}};
        int32_t pl, pr;
        double cl = 0, cr = 0, nl = 0, nr = 0;
        uint32_t t;
        test_signal(440, 12000, 0, 0);                    /* the right side silent (DUEL: part 2 not playing) */
        run(120, 1, ev, 2, 70000);
        pl = C[c].e == PF_TAPE ? peak(out_l, 2000, 12000) : peak(out_l, 30000, 60000);
        pr = peak(out_r, 0, 70000);
        snprintf(what, sizeof what, "%s: a silent right side stays silent (L %d, R %d)", C[c].name, pl, pr);
        expect(what, pr == 0 && pl > 3000);
        test_signal(440, 12000, 3 * 440, 12000);          /* each side its own pitch */
        run(120, 1, ev, 2, 70000);
        for (t = C[c].e == PF_TAPE ? 1100u : 30000u; t < (C[c].e == PF_TAPE ? 2600u : 50000u); t++) {
            cl += (double)out_l[t] * out_l[t - 1];
            nl += (double)out_l[t] * out_l[t];
            cr += (double)out_r[t] * out_r[t - 1];
            nr += (double)out_r[t] * out_r[t];
        }
        cl = nl > 0 ? cl / nl : 1;
        cr = nr > 0 ? cr / nr : 1;
        snprintf(what, sizeof what, "  each side keeps its own pitch (lag-1 correlation L %.3f, R %.3f)", cl, cr);
        expect(what, cl > 0.99 && cr < 0.99 && cr > 0.9 && cl - cr > 0.008);
    }
    {   /* FREEZE holds: the input stops, the sound goes on */
        ev_t ev[] = {{992, PF_FRZ + 1}, {60000, -(PF_FRZ + 1)}};
        uint32_t t;
        test_signal(440, 12000, 330, 9000);
        for (t = 9000; t < NT; t++)
            in_l[t] = in_r[t] = 0;
        run(120, 1, ev, 2, 70000);
        snprintf(what, sizeof what, "FREEZE: the input gone, it holds what it caught (L %d R %d)", peak(out_l, 20000, 60000),
                 peak(out_r, 20000, 60000));
        expect(what, peak(out_l, 20000, 60000) > 6000 && peak(out_r, 20000, 60000) > 4000 && same(60000 + 160, 70000));
    }
    {   /* TAPE STOP: slower, then silent within its beat */
        ev_t ev[] = {{992, PF_TAPE + 1}, {50016, -(PF_TAPE + 1)}};
        test_signal(440, 12000, 330, 9000);
        run(120, 1, ev, 2, 60000);
        snprintf(what, sizeof what, "TAPE STOP: silent a beat after the press (peak %d), live again after", peak(out_l, 992 + 22050 + 64, 50016));
        expect(what, peak(out_l, 992 + 22050 + 64, 50016) == 0 && peak(out_r, 992 + 22050 + 64, 50016) == 0 &&
                         same(50016 + 160, 60000));
    }
}

/* ---------------------------------------------------------- TOO LONG --- */
static void t_too_long(void)
{
    ev_t ev[] = {{992, PF_R8 + 1}, {40000, -(PF_R8 + 1)}};
    test_signal(440, 12000, 277, 9000);
    run(72, 1, ev, 2, 50000);
    expect("REPEAT 1/8 at 72 BPM (417 ms, the buffer holds 371): nothing happens, bit for bit",
           same(0, 50000) && !busy_seen && !(perf_avail() & PF_BIT(PF_R8)));
    run(120, 1, ev, 2, 50000);
    expect("REPEAT 1/8 at 120 BPM (250 ms): it plays", !same(0, 40000) && (perf_avail() & PF_BIT(PF_R8)));
    song.g[G_BPM] = 81;
    expect("  REPEAT 1/8 and REVERSE (1/8) fit from 81 BPM, 1/16 from 41, 1/32 from 21",
           (perf_avail() & PF_BIT(PF_R8) && perf_avail() & PF_BIT(PF_REV)) &&
               (song.g[G_BPM] = 80, !(perf_avail() & PF_BIT(PF_R8)) && !(perf_avail() & PF_BIT(PF_REV))) &&
               (song.g[G_BPM] = 41, perf_avail() & PF_BIT(PF_R16)) && (song.g[G_BPM] = 40, !(perf_avail() & PF_BIT(PF_R16))) &&
               (song.g[G_BPM] = 21, perf_avail() & PF_BIT(PF_R32)) && (song.g[G_BPM] = 20, !(perf_avail() & PF_BIT(PF_R32))));
    {   /* the page and the LEDs: greyed at 72 BPM, the reason when its key is pressed */
        uint32_t m = 0;
        fresh();
        song.g[G_BPM] = 72;
        go_title("AMP ENV");
        perform_on();
        frame();
        (void)perf_key_leds(&m);
        expect("  at 72 BPM: REPEAT 1/8 and REVERSE greyed (cannot run), REPEAT 1/16 can; the LEDs are only the row markers, whatever can run",
               !perf_can(PF_R8) && !perf_can(PF_REV) && perf_can(PF_R16) && ((m >> wkey(PF_R8)) & 1u) == !(key_place(wkey(PF_R8)) & 3u) && ((m >> wkey(PF_REV)) & 1u) == !(key_place(wkey(PF_REV)) & 3u) &&
                   ((m >> wkey(PF_R16)) & 1u) == !(key_place(wkey(PF_R16)) & 3u));
        key_down(wkey(PF_REV));
        frame();
        expect("  REVERSE pressed there: the header says why (TEMPO UNDER 81 BPM)", msg_is("TEMPO UNDER 81 BPM"));
        key_up(wkey(PF_REV));
        song.g[G_CLOCK] = 1;                            /* CLK USB, no clock: PUMP cannot run */
        song.playing = 0;
        frame();
        key_down(wkey(PF_PUMP));
        frame();
        expect("  CLK USB with no clock: PUMP greyed, its key says NO CLOCK", !perf_can(PF_PUMP) && msg_is("PUMP: NO CLOCK"));
        key_up(wkey(PF_PUMP));
        song.g[G_CLOCK] = 0;
    }
    {   /* a REPEAT held while the tempo slows past its fit stops cleanly */
        uint32_t t;
        int32_t mx = 0;
        host_tracks_init();
        perf_reset();
        song.g[G_BPM] = 120;
        song.playing = 1;
        perf_start();
        test_signal(110, 12000, 110, 12000);
        perf_press(PF_R8, 1);
        for (t = 0; t < 120000u; t += CTL) {
            uint32_t i;
            if (t == 60000u)
                song.g[G_BPM] = 72;
            memcpy(out_l + t % NT, in_l + t % NT, CTL * 4);
            memcpy(out_r + t % NT, in_r + t % NT, CTL * 4);
            if (!perf_idle() && perf_begin(CTL))
                perf_block(out_l + t % NT, out_r + t % NT, CTL);
            for (i = 1; i < CTL && t > 1000u; i++) {
                int32_t d = abs(out_l[t % NT + i] - out_l[t % NT + i - 1]);
                mx = d > mx ? d : mx;
            }
        }
        perf_press(PF_R8, 0);
        expect("  a REPEAT 1/8 held while the tempo drops below its fit: it ends, no click", pf.mode == BM_NONE && mx < 4 * 190);
    }
}

/* ------------------------------------------------------------ CLICKS --- */
static void t_clicks(void)
{
    static const struct { const char *name; int e; uint32_t off; } C[] = {
        {"REPEAT 1/8", PF_R8, 30000}, {"REPEAT 1/16", PF_R16, 30000}, {"REPEAT 1/32", PF_R32, 30000},
        {"REVERSE", PF_REV, 50000}, {"TAPE STOP", PF_TAPE, 30016}, {"FREEZE", PF_FRZ, 30016}};
    uint32_t c, t;
    int32_t own;
    char what[160];
    test_signal(110, 16000, 110, 16000);
    for (own = 0, t = 1; t < NT; t++)
        own = abs(in_l[t] - in_l[t - 1]) > own ? abs(in_l[t] - in_l[t - 1]) : own;
    for (c = 0; c < sizeof C / sizeof C[0]; c++) {
        ev_t ev[] = {{992, C[c].e + 1}, {C[c].off, -(C[c].e + 1)}};
        int32_t mx = 0;
        run(133, 1, ev, 2, C[c].off + 9000);
        for (t = 1; t < C[c].off + 9000; t++) {
            int32_t d = abs(out_l[t] - out_l[t - 1]);
            mx = d > mx ? d : mx;
        }
        snprintf(what, sizeof what, "no clicks: %s on a 110 Hz sine, in and out: largest step %d (sine %d)", C[c].name, mx, own);
        expect(what, mx <= 4 * own);
        expect("  back to the live signal, bit for bit", same(C[c].off + 3000, C[c].off + 9000));
    }
    {   /* full scale squares past Q15 through REPEAT: no wrap */
        ev_t e2[] = {{0, PF_R16 + 1}};
        int32_t pk;
        for (t = 0; t < NT; t++) {
            in_l[t] = (t / 37u) & 1u ? 120000 : -120000;
            in_r[t] = (t / 53u) & 1u ? 120000 : -120000;
        }
        run(120, 1, e2, 1, 100000);
        pk = peak(out_l, 0, 100000) > peak(out_r, 0, 100000) ? peak(out_l, 0, 100000) : peak(out_r, 0, 100000);
        snprintf(what, sizeof what, "4x full scale through REPEAT: peak %d (no wrap)", pk);
        expect(what, pk < 300000);
    }
}

/* -------------------------------------------------------- OCT UP / DN --- */
static double dft_pow(const int32_t *x, uint32_t a, uint32_t b, double f)
{
    double w = 2 * M_PI * f / FS, re = 0, im = 0, c = cos(w), sn = sin(w), cr = 1, ci = 0;
    uint32_t t;
    for (t = a; t < b; t++) {                      /* (a Hann window: leakage of the splices kept local) */
        double h = 0.5 - 0.5 * cos(2 * M_PI * (t - a) / (b - a)), v = x[t] * h, nr;
        re += v * cr;
        im -= v * ci;
        nr = cr * c - ci * sn;
        ci = cr * sn + ci * c;
        cr = nr;
    }
    return re * re + im * im;
}
static double peak_freq(const int32_t *x, uint32_t a, uint32_t b, double f0)
{
    double lo = f0 * 0.85, hi = f0 * 1.15, best = f0, bp = -1, f, g = 0.6180339887;
    int k;
    for (f = lo; f <= hi; f += 0.25) {
        double p = dft_pow(x, a, b, f);
        if (p > bp) {
            bp = p;
            best = f;
        }
    }
    lo = best - 0.25;
    hi = best + 0.25;
    for (k = 0; k < 40; k++) {
        double m1 = hi - g * (hi - lo), m2 = lo + g * (hi - lo);
        if (dft_pow(x, a, b, m1) > dft_pow(x, a, b, m2))
            hi = m2;
        else
            lo = m1;
    }
    return (lo + hi) / 2;
}
static int32_t res_l[NT], res_r[NT];
static void t_harm(void)
{
    char what[200];
    uint32_t t, k;
    run_depth = 0;                                      /* (no shimmer: the pitch alone) */
    for (k = 0; k < 4u; k++) {   /* the pitch, each side its own: on the sweep's grid (k 0, 1), off it (2, 3) */
        int e = k & 1u ? PF_ODN : PF_OUP;
        double r = k & 1u ? 0.5 : 2.0, f0l = k < 2u ? 5.0 * FS / 1024 : 220, f0r = k < 2u ? 8.0 * FS / 1024 : 330;
        double fl, fr, cl, cr;
        ev_t ev[] = {{992, e + 1}};
        test_signal(f0l, 12000, f0r, 9000);
        run(120, 1, ev, 1, 3u * FS);
        for (t = 0; t < 3u * FS; t++) {                /* the shifted part: out less 0.56 of the input */
            res_l[t] = out_l[t] - ((in_l[t] >> 1) + ((in_l[t] >> 1) >> 3));
            res_r[t] = out_r[t] - ((in_r[t] >> 1) + ((in_r[t] >> 1) >> 3));
        }
        fl = peak_freq(res_l, FS / 2u, 3u * FS, f0l * r);
        fr = peak_freq(res_r, FS / 2u, 3u * FS, f0r * r);
        cl = 1200 * log2(fl / (f0l * r));
        cr = 1200 * log2(fr / (f0r * r));
        if (k < 2u) {
            snprintf(what, sizeof what, "%s: %.2f / %.2f Hz in, out at %.2f / %.2f Hz (%+.2f / %+.2f cents)",
                     k ? "OCT DN" : "OCT UP", f0l, f0r, fl, fr, cl, cr);
            expect(what, fabs(cl) <= 3 && fabs(cr) <= 3);
        } else {
            snprintf(what, sizeof what, "  220 / 330 Hz: the strongest splice line %+.1f / %+.1f Hz from %.0f / %.0f (within 21.5)",
                     fl - f0l * r, fr - f0r * r, f0l * r, f0r * r);
            expect(what, fabs(fl - f0l * r) <= FS / 2048.0 && fabs(fr - f0r * r) <= FS / 2048.0);
        }
    }
    run_depth = 100;
    {   /* a silent side stays silent */
        ev_t ev[] = {{992, PF_OUP + 1}, {60000, -(PF_OUP + 1)}};
        test_signal(440, 12000, 0, 0);
        run(120, 1, ev, 2, 70000);
        snprintf(what, sizeof what, "OCT UP: a silent right side stays silent (L %d, R %d)", peak(out_l, 20000, 60000),
                 peak(out_r, 0, 70000));
        expect(what, peak(out_r, 0, 70000) == 0 && peak(out_l, 20000, 60000) > 3000);
    }
    for (k = 0; k < 2u; k++) {   /* no clicks in or out; live again after the ramp */
        int e = k ? PF_ODN : PF_OUP;
        ev_t ev[] = {{992, e + 1}, {40000, -(e + 1)}};
        int32_t mx = 0, own = 0;
        test_signal(110, 16000, 110, 16000);
        for (t = 1; t < NT; t++)
            own = abs(in_l[t] - in_l[t - 1]) > own ? abs(in_l[t] - in_l[t - 1]) : own;
        run_depth = 0;                                  /* (the shifter alone: the shimmer climbs, its steps are its octaves') */
        run(133, 1, ev, 2, 49000);
        run_depth = 100;
        for (t = 1; t < 49000u; t++) {
            int32_t d = abs(out_l[t] - out_l[t - 1]);
            mx = d > mx ? d : mx;
        }
        snprintf(what, sizeof what, "no clicks: %s on a 110 Hz sine, in and out: largest step %d (sine %d)",
                 k ? "OCT DN" : "OCT UP", mx, own);
        expect(what, mx <= 4 * own);
        expect("  back to the live signal, bit for bit; the buffer given back", same(40000 + 3000, 49000) && pf.mode == BM_NONE);
    }
    {   /* the shimmer at its most (DEPTH 100 %: 0.5): bounded, and it dies away */
        ev_t ev[] = {{0, PF_OUP + 1}};
        int32_t pk, tail;
        uint32_t e1 = NT / CTL * CTL, e0 = e1 - FS / 2u;   /* (whole blocks) the last half second */
        for (t = 0; t < NT; t++) {
            in_l[t] = t < 3u * FS ? ((t / 37u) & 1u ? 120000 : -120000) : 0;
            in_r[t] = t < 3u * FS ? ((t / 53u) & 1u ? 120000 : -120000) : 0;
        }
        run(120, 1, ev, 1, e1);
        pk = peak(out_l, 0, e1) > peak(out_r, 0, e1) ? peak(out_l, 0, e1) : peak(out_r, 0, e1);
        tail = peak(out_l, e0, e1) > peak(out_r, e0, e1) ? peak(out_l, e0, e1) : peak(out_r, e0, e1);
        snprintf(what, sizeof what, "OCT UP, DEPTH 100 %% (shimmer 0.5) on 4x full-scale squares: peak %d, 2.5 s after they stop %d", pk, tail);
        expect(what, pk < 300000 && tail <= 16 && pf.hfb == 100 * HB_FB);
    }
    {   /* DEPTH 0: no feedback, the delay holds the input only (half level) */
        ev_t ev[] = {{0, PF_OUP + 1}};
        uint32_t f, diff = 0;
        test_signal(440, 12000, 330, 9000);
        run_depth = 0;
        run(120, 1, ev, 1, 20000);
        run_depth = 100;
        for (f = 1; f <= 2048u; f++) {
            uint32_t s = 20000u - f;
            const int16_t *q = &pb_buf[0][0] + 2u * ((pf.wr - f) & HB_MASK);
            diff += q[0] != (int16_t)(in_l[s] >> 1) || q[1] != (int16_t)(in_r[s] >> 1);
        }
        expect("OCT UP, K4 DEPTH 0 %: no shimmer (the delay holds the input alone)", !diff && !pf.hfb);
    }
}


/* ------------------------------------------------------------ FILTER --- */
static void filt_sweep(uint32_t t)                      /* K3 turned: 0 -> -100 -> +100 -> 0 over 1.5 s, a detent at a time */
{
    int32_t k = (int32_t)(t / CTL) % 140;
    perf_filt = (int8_t)(t < 8u * 44100u / 10u ? (k < 50 ? -2 * k : k < 100 ? -200 + 2 * k + 0 : 0) : 0);
    if (t < 15000u)
        perf_filt = (int8_t)-(int32_t)(t / 150u);
    else if (t < 45000u)
        perf_filt = (int8_t)(-100 + (int32_t)(t - 15000u) / 150);
    else if (t < 60000u)
        perf_filt = (int8_t)(100 - (int32_t)(t - 45000u) / 150);
    else
        perf_filt = 0;
}
static void t_filter(void)
{
    char what[200];
    uint32_t t;
    int32_t own, mx;
    test_signal(1000, 30000, 1000, 30000);              /* DUELL 0.2.9: the ends close completely (30 Hz, 15.4 kHz, then the kill zone) */
    run_filt = -96;
    run(120, 1, 0, 0, 20000);
    snprintf(what, sizeof what, "K3 FILTER at -96 (low-pass at 30 Hz, the kill zone's edge): a 1 kHz sine down to %d of 30000 (0.2.13: 1512, 26 dB; now more than 80 dB)",
             peak(out_l, 10000, 20000));
    expect(what, peak(out_l, 10000, 20000) < 3 && peak(out_r, 10000, 20000) < 3);
    run_filt = -100;
    run(120, 1, 0, 0, 20000);
    snprintf(what, sizeof what, "K3 FILTER all the way left (low-pass 30 Hz, the kill zone): silent (%d)", peak(out_l, 10000, 20000));
    expect(what, !peak(out_l, 10000, 20000) && !peak(out_r, 10000, 20000));
    run_filt = 96;
    run(120, 1, 0, 0, 20000);
    snprintf(what, sizeof what, "K3 FILTER at +96 (high-pass at 15 kHz): a 1 kHz sine down to %d of 30000 (more than 80 dB)", peak(out_l, 10000, 20000));
    expect(what, peak(out_l, 10000, 20000) < 3 && peak(out_r, 10000, 20000) < 3);
    run_filt = 100;
    run(120, 1, 0, 0, 20000);
    snprintf(what, sizeof what, "K3 FILTER all the way right (high-pass 15 kHz, the kill zone): silent (%d)", peak(out_l, 10000, 20000));
    expect(what, !peak(out_l, 10000, 20000) && !peak(out_r, 10000, 20000));
    test_signal(5000, 30000, 5000, 30000);
    run_filt = -50;
    run(120, 1, 0, 0, 20000);
    snprintf(what, sizeof what, "K3 FILTER half left (low-pass ~680 Hz): a 5 kHz sine down to %d of 30000 (24 dB/oct: ~73 dB)", peak(out_l, 10000, 20000));
    expect(what, peak(out_l, 10000, 20000) < 30 && peak(out_r, 10000, 20000) < 30);
    test_signal(1000, 20000, 1000, 20000);
    run_filt = -1;
    run(120, 1, 0, 0, 20000);
    snprintf(what, sizeof what, "  one detent off the middle: a 1 kHz sine kept (%d of 20000)", peak(out_l, 10000, 20000));
    expect(what, peak(out_l, 10000, 20000) > 19000);
    test_signal(110, 16000, 110, 16000);                /* turned all the way round and back: no click, then bypass bit for bit */
    for (own = 0, t = 1; t < NT; t++)
        own = abs(in_l[t] - in_l[t - 1]) > own ? abs(in_l[t] - in_l[t - 1]) : own;
    run_at = filt_sweep;
    run(120, 1, 0, 0, 90000);
    run_at = 0;
    for (mx = 0, t = 1; t < 90000u; t++)
        mx = abs(out_l[t] - out_l[t - 1]) > mx ? abs(out_l[t] - out_l[t - 1]) : mx;
    snprintf(what, sizeof what, "  K3 turned left, right and back to the middle on a 110 Hz sine: largest step %d (sine %d)", mx, own);
    expect(what, mx <= 4 * own);
    expect("  back in the middle: off after its glide (the live signal bit for bit, the stage at rest)", same(60000 + 6000, 90000) && !pf.fa && !pf.ka &&
                                                                                                         !pf.busy);
    for (t = 0; t < NT; t++) {                          /* full scale squares past Q15 */
        in_l[t] = (t / 37u) & 1u ? 120000 : -120000;
        in_r[t] = (t / 53u) & 1u ? 120000 : -120000;
    }
    {
        ev_t e2[] = {{0, PF_R16 + 1}};
        int32_t pk;
        run_filt = -60;
        run(120, 1, e2, 1, 60000);
        pk = peak(out_l, 0, 60000) > peak(out_r, 0, 60000) ? peak(out_l, 0, 60000) : peak(out_r, 0, 60000);
        run_filt = 60;
        run(120, 1, e2, 1, 60000);
        pk = peak(out_l, 0, 60000) > pk ? peak(out_l, 0, 60000) : pk;
        snprintf(what, sizeof what, "  4x full scale through REPEAT + the low-pass / the high-pass: peak %d (no wrap; the square's edges ring)", pk);
        expect(what, pk < 400000);
    }
}

/* ------------------------------------------- FILTER: the 24 dB/oct (0.2.14) --- */
/* least squares DC + sine + cosine at f on x[a, b): the amplitude, the DC, the residual's rms */
static void sine_fit(const int32_t *x, uint32_t a, uint32_t b, double f, double *amp, double *dc, double *res)
{
    double m[3][4] = {{0}}, c[3], e = 0;
    uint32_t t;
    int i, j, k;
    for (t = a; t < b; t++) {
        double v[3] = {1, sin(2 * M_PI * f * t / FS), cos(2 * M_PI * f * t / FS)};
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++)
                m[i][j] += v[i] * v[j];
            m[i][3] += v[i] * x[t];
        }
    }
    for (i = 0; i < 3; i++)
        for (k = i + 1; k < 3; k++)
            for (j = 3; j >= i; j--)
                m[k][j] -= m[k][i] / m[i][i] * m[i][j];
    for (i = 2; i >= 0; i--) {
        c[i] = m[i][3];
        for (j = i + 1; j < 3; j++)
            c[i] -= m[i][j] * c[j];
        c[i] /= m[i][i];
    }
    for (t = a; t < b; t++) {
        double d = x[t] - c[0] - c[1] * sin(2 * M_PI * f * t / FS) - c[2] * cos(2 * M_PI * f * t / FS);
        e += d * d;
    }
    *amp = hypot(c[1], c[2]);
    *dc = c[0];
    *res = sqrt(e / (b - a));
}
/* the gain (dB) of K3 at k on a sine at f (both sides, amplitude 16384, the last 0.5 s of 1.2 s); *res the residual's rms (dBFS), *dc */
static double filt_gain(int k, double f, double *res, double *dc)
{
    double a, d, r, a2, d2, r2;
    test_signal(f, 16384, f, 16384);
    run_filt = (int8_t)k;
    run(120, 0, 0, 0, 52920);
    sine_fit(out_l, 30870, 52920, f, &a, &d, &r);
    sine_fit(out_r, 30870, 52920, f, &a2, &d2, &r2);
    if (res)
        *res = 20 * log10(fmax(r, r2) / 32768 + 1e-30);
    if (dc)
        *dc = fmax(fabs(d), fabs(d2));
    return 20 * log10(fmin(a, a2) / 16384 + 1e-30);
}
/* the design's response at f, K3 at k (steady): the two bilinear stages of g (PF_G between its rows, as perform.c) and the dampings */
static double filt_ideal(int k, double f)
{
    int32_t q = (k < 0 ? -k : k) << 8, u = k > 0 ? (q * PF_UHR) >> 16 : PF_ULO - ((q * PF_ULR) >> 16), i = u >> 8;
    double g = (PF_G[i] + (double)(PF_G[i + 1] - PF_G[i]) * (u & 255) / 256.0) / 134217728.0, w = 2 * M_PI * f / FS, h = 1;
    double kk[2] = {PF_KA / 1073741824.0, PF_KB / 1073741824.0};
    int s;
    for (s = 0; s < 2; s++) {   /* H(s) of the analog prototype at the warped frequency: LP g^2 / (g^2 + k g W + W^2) with W = tan(w / 2) j */
        double W = tan(w / 2), re = g * g - W * W, im = kk[s] * g * W, num = k > 0 ? W * W : g * g;
        h *= num / hypot(re, im);
    }
    return 20 * log10(h);
}
/* the corner of K3 at k (Hz): where its g is */
static double filt_fc(int k)
{
    int32_t q = (k < 0 ? -k : k) << 8, u = k > 0 ? (q * PF_UHR) >> 16 : PF_ULO - ((q * PF_ULR) >> 16), i = u >> 8;
    double g = (PF_G[i] + (double)(PF_G[i + 1] - PF_G[i]) * (u & 255) / 256.0) / 134217728.0;
    return atan(g) * FS / M_PI;
}
/* a radix-2 FFT, n a power of 2 */
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j, len, b;
    for (i = 1, j = 0; i < n; i++) {
        for (b = n >> 1; j & b; b >>= 1)
            j ^= b;
        j ^= b;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1)
        for (i = 0; i < n; i += len)
            for (j = 0; j < len / 2; j++) {
                double a = -2 * M_PI * j / len, wr = cos(a), wi = sin(a), *xr = re + i + j, *xi = im + i + j;
                double vr = xr[len / 2] * wr - xi[len / 2] * wi, vi = xr[len / 2] * wi + xi[len / 2] * wr;
                xr[len / 2] = *xr - vr; xi[len / 2] = *xi - vi;
                *xr += vr; *xi += vi;
            }
}
static double spur_hz;                                 /* (spectrum: where its highest spur was) */
/* the spectrum of x (4096 points from a, Blackman-Harris) against a tone at f0: the highest bin more than 250 Hz from it (*spur), the
 * median of those bins (*floor), the highest bin within 30 Hz of fz1 or fz2 (*zip), all dB under the tone's highest bin */
static void spectrum(const double *x, uint32_t a, double f0, double fz1, double fz2, double *spur, double *floor_, double *zip)
{
    static double re[4096], im[4096], ob[2048];
    uint32_t t, k, no = 0;
    double tone = 0, sp = 0, zp = 0;
    for (t = 0; t < 4096u; t++) {
        double w = 0.35875 - 0.48829 * cos(2 * M_PI * t / 4095) + 0.14128 * cos(4 * M_PI * t / 4095) - 0.01168 * cos(6 * M_PI * t / 4095);
        re[t] = x[a + t] * w;
        im[t] = 0;
    }
    fft(re, im, 4096);
    for (k = 1; k < 2048u; k++) {
        double f = k * (double)FS / 4096, p = re[k] * re[k] + im[k] * im[k];
        if (fabs(f - f0) < 250) {
            tone = p > tone ? p : tone;
        } else {
            if (p > sp)
                sp = p, spur_hz = f;
            ob[no++] = p;
        }
        if (fabs(f - fz1) < 30 || fabs(f - fz2) < 30)
            zp = p > zp ? p : zp;
    }
    for (t = 0; t < no; t++)                            /* (the median: a partial selection sort to the middle) */
        for (k = t + 1; t <= no / 2 && k < no; k++)
            if (ob[k] < ob[t]) {
                double q = ob[t]; ob[t] = ob[k]; ob[k] = q;
            }
    *spur = 10 * log10(tone / (sp + 1e-30));
    *floor_ = 10 * log10(tone / (ob[no / 2] + 1e-30));
    *zip = 10 * log10(tone / (zp + 1e-30));
}
/* the ideally smoothed reference in double: K3's glide as a one-pole a SAMPLE (the same time constant), the coefficients exact of g at every
 * sample (no straight lines, no fixed point), the shares and the mode as perform.c; x in, y out */
static void filt_ref(const int32_t *x, double *y, uint32_t n0, void (*at)(uint32_t t))
{
    double z[4] = {0, 0, 0, 0}, fk = 0, kk[2] = {PF_KA / 1073741824.0, PF_KB / 1073741824.0};
    int32_t fa = 0, fm = 0, ifk = 0;
    uint32_t t, i, s;
    for (t = 0; t < n0; t += CTL) {
        int32_t m, wm, same, tk, ta, d;
        if (at)
            at(t);
        m = perf_filt < 0 ? -perf_filt : perf_filt;
        wm = perf_filt > 0 ? 1 : perf_filt < 0 ? 0 : fm;
        if (!fa && wm != fm)
            fm = wm, ifk = 0, fk = 0;
        same = perf_filt && wm == fm;
        tk = same ? (m < PF_KILL ? m : PF_KILL) << 8 : 0;
        d = tk - ifk;                                   /* (the integer glide decides the fades, as perform.c) */
        ifk += d > -16 && d < 16 ? d : d >> 4;
        ta = same || ifk > PF_KOPEN ? 32768 : 0;
        for (i = 0; i < CTL; i++) {
            double v = x[t + i], o = v, u, g;
            int32_t ui;
            fk += (tk - fk) * (1 - pow(15.0 / 16.0, 1.0 / CTL));
            if (!fa && !ta) {
                y[t + i] = v;
                continue;
            }
            u = fm ? fk * PF_UHR / 65536 : PF_ULO - fk * PF_ULR / 65536;
            ui = (int32_t)u;
            g = (PF_G[ui >> 8] + (double)(PF_G[(ui >> 8) + 1] - PF_G[ui >> 8]) * (u - (ui & ~255)) / 256) / 134217728.0;
            for (s = 0; s < 2; s++) {
                double a1 = 1 / (1 + g * (g + kk[s])), a2 = g * a1, a3 = g * a2, v3 = o - z[2 * s + 1];
                double v1 = a1 * z[2 * s] + a2 * v3, v2 = z[2 * s + 1] + a2 * z[2 * s] + a3 * v3;
                z[2 * s] = 2 * v1 - z[2 * s];
                z[2 * s + 1] = 2 * v2 - z[2 * s + 1];
                o = fm ? o - v2 - kk[s] * v1 : v2;
            }
            fa += clamp(ta - fa, -PF_FADE, PF_FADE);
            y[t + i] = v + (o - v) * fa / 32768.0;
        }
        if (!fa)
            z[0] = z[1] = z[2] = z[3] = 0, ifk = 0, fk = 0;
    }
}
static int zip_a, zip_b;                               /* the sweep: K3 from zip_a to zip_b, a detent every 20 ms after 0.3 s */
static void zip_at(uint32_t t)
{
    uint32_t ms = t / 441u * 10u;
    int st = ms < 300u ? 0 : (int)(ms - 300u) / 20, span = abs(zip_b - zip_a), k = st > span ? span : st;
    perf_filt = (int8_t)(zip_a + (zip_b > zip_a ? k : -k));
}
static void swing_at(uint32_t t) { perf_filt = (int8_t)(t < 22050u ? -60 : 60); }
static void t_filter24(void)
{
    static double ref[NT], fx[NT];
    char what[220];
    double r, dc, g, worst = 0, wres = -999, wdc = 0;
    uint32_t t, k, ok;
    {   /* the middle: off, bit for bit, the stage never runs */
        test_signal(1000, 20000, 330, 12000);
        run_filt = 0;
        run(120, 1, 0, 0, 44100);
        expect("K3 FILTER (0.2.14, 24 dB/oct) in the middle: the live signal bit for bit, the stage never runs", same(0, 44100) && !busy_seen);
    }
    {   /* the open ends: flat (+-0.5 dB) from 30 Hz to 15 kHz one detent off the middle */
        static const double F[6] = {30, 100, 1000, 5000, 10000, 15000};
        double lo = 0, hi = 0;
        for (k = 0; k < 12u; k++) {
            g = filt_gain(k < 6u ? -1 : 1, F[k % 6u], 0, 0);
            lo = g < lo ? g : lo;
            hi = g > hi ? g : hi;
        }
        snprintf(what, sizeof what, "  a detent off the middle (low-pass %.1f kHz, high-pass %.1f Hz): 30 Hz .. 15 kHz within %+.2f .. %+.2f dB (+-0.5)",
                 filt_fc(-1) / 1000, filt_fc(1), lo, hi);
        expect(what, lo >= -0.5 && hi <= 0.5);
    }
    {   /* -50 and +50: the passband flat, -3 dB at the corner, no bump, -24 dB/oct away from it, the design's response within 0.2 dB */
        double fl = filt_fc(-50), fh = filt_fc(50), pl = 0, ph = 0, bump = -99, dev = 0, sl, sh, c1, c2;
        static const double PB[4] = {0.1, 0.2, 0.3, 0.5}, NB[4] = {0.5, 0.7, 0.85, 1.0};
        for (k = 0; k < 4u; k++) {
            g = filt_gain(-50, fl * PB[k], 0, 0);
            pl = fabs(g) > pl ? fabs(g) : pl;
            g = filt_gain(50, fh / PB[k], 0, 0);
            ph = fabs(g) > ph ? fabs(g) : ph;
            g = filt_gain(-50, fl * NB[k], 0, 0);
            bump = g > bump ? g : bump;
            dev = fabs(g - filt_ideal(-50, fl * NB[k])) > dev ? fabs(g - filt_ideal(-50, fl * NB[k])) : dev;
            g = filt_gain(50, fh / NB[k], 0, 0);
            bump = g > bump ? g : bump;
            dev = fabs(g - filt_ideal(50, fh / NB[k])) > dev ? fabs(g - filt_ideal(50, fh / NB[k])) : dev;
        }
        c1 = filt_gain(-50, fl, 0, 0);
        c2 = filt_gain(50, fh, 0, 0);
        snprintf(what, sizeof what, "  K3 -50 (low-pass %.0f Hz) / +50 (high-pass %.0f Hz): passband within %.2f / %.2f dB, at the corner %.2f / %.2f dB, highest %+.3f dB (no bump)",
                 fl, fh, pl, ph, c1, c2, bump);
        expect(what, pl <= 0.5 && ph <= 0.5 && fabs(c1 + 3.01) < 0.3 && fabs(c2 + 3.01) < 0.3 && bump <= 0.05);
        sl = filt_gain(-50, 6 * fl, 0, 0) - filt_gain(-50, 3 * fl, 0, 0);    /* (3 fc .. 6 fc: the bilinear zero at Nyquist adds ~0.7 dB at 6 fc) */
        sh = filt_gain(50, fh / 8, 0, 0) - filt_gain(50, fh / 4, 0, 0);
        dev = fabs(filt_gain(-50, 6 * fl, 0, 0) - filt_ideal(-50, 6 * fl)) > dev ? fabs(filt_gain(-50, 6 * fl, 0, 0) - filt_ideal(-50, 6 * fl)) : dev;
        dev = fabs(filt_gain(50, fh / 8, 0, 0) - filt_ideal(50, fh / 8)) > dev ? fabs(filt_gain(50, fh / 8, 0, 0) - filt_ideal(50, fh / 8)) : dev;
        snprintf(what, sizeof what, "  slopes: low-pass 3 fc -> 6 fc %.2f dB, high-pass fc/4 -> fc/8 %.2f dB (-24 +-1); the design's response within %.3f dB", sl, sh, dev);
        expect(what, fabs(sl + 24) <= 1 && fabs(sh + 24) <= 1 && dev <= 0.2);
    }
    {   /* the noise floor: sines at -6 dBFS (100 Hz, 1 kHz, 8 kHz) at fixed settings: the residual after the fit (0.2.13: -66 .. -89 dBFS, DC up to 1416) */
        static const int K[10] = {-1, -20, -50, -80, -95, 1, 20, 50, 80, 95};
        static const double F[3] = {100, 1000, 8000};
        for (k = 0; k < 30u; k++) {
            filt_gain(K[k / 3u], F[k % 3u], &r, &dc);
            wres = r > wres ? r : wres;
            wdc = dc > wdc ? dc : wdc;
        }
        snprintf(what, sizeof what, "  noise floor at K3 -1 -20 -50 -80 -95 +1 +20 +50 +80 +95 (sines 100 Hz / 1 kHz / 8 kHz): at most %.1f dBFS (-95), DC %.2f", wres, wdc);
        expect(what, wres <= -95 && wdc <= 0.5);
    }
    {   /* silence after a tone: exactly 0 (no limit cycle, no DC stuck in the states) */
        static const int K[10] = {-1, -20, -50, -80, -95, 1, 20, 50, 80, 95};
        for (ok = 1, k = 0; k < 10u; k++) {
            test_signal(1000, 16384, 50, 16384);
            for (t = 22050; t < NT; t++)
                in_l[t] = in_r[t] = 0;
            run_filt = (int8_t)K[k];
            run(120, 0, 0, 0, 66150);
            ok &= !peak(out_l, 44100, 66150) && !peak(out_r, 44100, 66150);
        }
        expect("  silence after a tone (1 kHz / 50 Hz), every setting: the output exactly 0 within 0.5 s", ok);
    }
    {   /* no zipper: K3 turned a detent every 20 ms over a 1 kHz sine; the spectrum against the ideally smoothed reference's */
        static const int Z[4][2] = {{0, -45}, {0, 55}, {-45, 0}, {55, 0}};
        double wz = 999, wf = 999, wsp = 0;
        for (k = 0; k < 4u; k++) {
            uint32_t a, n0 = 3u * 44100u;
            test_signal(1000, 16384, 1000, 16384);
            zip_a = Z[k][0];
            zip_b = Z[k][1];
            run_at = zip_at;
            run(120, 0, 0, 0, n0);
            run_at = 0;
            filt_ref(in_l, ref, n0, zip_at);
            for (t = 0; t < n0; t++)
                fx[t] = out_l[t];
            for (a = 0; a + 4096u <= n0; a += 2048u) {
                double s1, f1, z1, s2, f2, z2, hz;
                spectrum(fx, a, 1000, 1000 + FS / CTL - 2000, 1000 + FS / CTL, &s1, &f1, &z1);   /* (the block rate's sidebands: 378 Hz, 2378 Hz) */
                hz = spur_hz;
                spectrum(ref, a, 1000, 1000 + FS / CTL - 2000, 1000 + FS / CTL, &s2, &f2, &z2);
                wz = z1 < wz ? z1 : wz;
                wf = f1 < wf ? f1 : wf;
                if (s1 < 70 && s2 - s1 > wsp)               /* (a spur that matters: within 70 dB of the tone) */
                    wsp = s2 - s1;
                if (getenv("ZIP_VERBOSE"))
                    printf("zip %d->%d seg %.2f s: spur %.1f at %.0f Hz (ideal %.1f at %.0f Hz), floor %.1f, block-rate %.1f\n", zip_a, zip_b, a / 44100.0, s1, hz, s2, spur_hz, f1, z1);
            }
        }
        snprintf(what, sizeof what, "  K3 turned a detent every 20 ms (0 -> -45, 0 -> +55 and back) on 1 kHz: block-rate sidebands %.1f dB under the tone, the floor %.1f dB under, "
                 "a spur within 70 dB of it at most %.1f dB above the ideally smoothed filter's", wz, wf, wsp);
        expect(what, wz >= 70 && wf >= 70 && wsp <= 1.5);
    }
    {   /* stable at the extremes: squares at 8x full scale and past the input's clamp, every 10 detents, then silence: bounded, and back to 0 */
        static const double F[4] = {20, 110, 1000, 7000};
        int32_t pk = 0;
        for (ok = 1, k = 0; k < 4u * 21u; k++) {
            int32_t amp = k & 1u ? 400000 : 262144;
            for (t = 0; t < NT; t++)
                in_l[t] = in_r[t] = t >= 33075u ? 0 : sin(2 * M_PI * F[k / 21u] * t / FS) >= 0 ? amp : -amp;
            run_filt = (int8_t)(-100 + 10 * (int)(k % 21u));
            run(120, 0, 0, 0, 66150);
            pk = peak(out_l, 0, 33075) > pk ? peak(out_l, 0, 33075) : pk;
            ok &= !peak(out_l, 55125, 66150) && !peak(out_r, 55125, 66150);
        }
        snprintf(what, sizeof what, "  squares at 8x full scale and 400000 (past the clamp), K3 -100 .. +100: peak %d (no wrap), silent again after", pk);
        expect(what, ok && pk < 700000);
    }
    {   /* turned across the middle at once (-60 -> +60): no click; the low-pass sweeps open, fades out, rests, the high-pass fades in and closes */
        int32_t own = 0, mx = 0;
        test_signal(110, 16000, 110, 16000);
        for (t = 1; t < NT; t++)
            own = abs(in_l[t] - in_l[t - 1]) > own ? abs(in_l[t] - in_l[t - 1]) : own;
        run_at = swing_at;
        run(120, 0, 0, 0, 66150);
        run_at = 0;
        for (t = 1; t < 66150u; t++)
            mx = abs(out_l[t] - out_l[t - 1]) > mx ? abs(out_l[t] - out_l[t - 1]) : mx;
        snprintf(what, sizeof what, "  turned -60 -> +60 at once on a 110 Hz sine: largest step %d (sine %d), the high-pass at work after (%u, share %d)", mx, own,
                 (unsigned)pf.fm, (int)pf.fa);
        expect(what, mx <= 2 * own && pf.fm == 1u && pf.fa == 32768);
    }
}

/* ------------------------------------------------- FLANGER, PHASER --- */
static double mean_of(const int32_t *x, uint32_t a, uint32_t b)
{
    double m = 0;
    uint32_t t;
    for (t = a; t < b; t++)
        m += x[t];
    return m / (b - a);
}
static double rms_of(const int32_t *x, uint32_t a, uint32_t b)
{
    double m = 0;
    uint32_t t;
    for (t = a; t < b; t++)
        m += (double)x[t] * x[t];
    return sqrt(m / (b - a));
}
/* the largest |out[t] - out[t + d]| over [a, b), both sides: the output repeats after d samples */
static int32_t period_err(uint32_t a, uint32_t b, uint32_t d)
{
    uint32_t t;
    int32_t e = 0;
    for (t = a; t < b; t++) {
        int32_t x = abs(out_l[t] - out_l[t + d]), y = abs(out_r[t] - out_r[t + d]);
        e = x > e ? x : e;
        e = y > e ? y : e;
    }
    return e;
}
static void t_mod(void)
{
    static const struct { const char *name; int e; uint32_t s16; double dc; } M[] = {{"FLANGER", PF_FLG, 16, 1.275},
                                                                                     {"PHASER", PF_PHS, 8, 1.2}};
    static int32_t ref_l[NT], ref_r[NT];
    char what[200];
    uint32_t m, t;
    int32_t own;
    for (m = 0; m < 2u; m++) {
        int e = M[m].e;
        {   /* silence in: silence out; a silent side stays silent */
            ev_t ev[] = {{992, e + 1}, {60000, -(e + 1)}};
            test_signal(440, 0, 0, 0);
            run(120, 1, ev, 2, 70000);
            snprintf(what, sizeof what, "%s: silence in, silence out (exact)", M[m].name);
            expect(what, !peak(out_l, 0, 70000) && !peak(out_r, 0, 70000) && busy_seen);
            test_signal(440, 12000, 0, 0);
            run(120, 1, ev, 2, 70000);
            snprintf(what, sizeof what, "  a silent right side stays silent (L %d, R %d)", peak(out_l, 20000, 60000), peak(out_r, 0, 70000));
            expect(what, !peak(out_r, 0, 70000) && peak(out_l, 20000, 60000) > 6000 && !same(2000, 60000));
        }
        {   /* no DC: a zero-mean mix stays zero-mean; its level about the input's */
            ev_t ev[] = {{0, e + 1}};
            double dm, lv;
            uint32_t r = 12345;
            for (t = 0; t < NT; t++) {
                r = r * 1103515245u + 12345u;
                in_l[t] = (int32_t)((r >> 16) & 0x3FFF) - 8192;
                r = r * 1103515245u + 12345u;
                in_r[t] = (int32_t)((r >> 16) & 0x3FFF) - 8192;
            }
            {
                int32_t ml = (int32_t)lrint(mean_of(in_l, FS, 4u * FS)), mr = (int32_t)lrint(mean_of(in_r, FS, 4u * FS));
                for (t = 0; t < 4u * FS; t++) {
                    in_l[t] -= ml;
                    in_r[t] -= mr;
                }
            }
            run(120, 1, ev, 1, 4u * FS);
            dm = fmax(fabs(mean_of(out_l, FS, 4u * FS)), fabs(mean_of(out_r, FS, 4u * FS)));
            lv = 20 * log10(rms_of(out_l, FS, 4u * FS) / rms_of(in_l, FS, 4u * FS));
            snprintf(what, sizeof what, "  no DC: zero-mean noise (%.0f rms) in, the mean out %.2f; its level %+.1f dB", rms_of(in_l, FS, 4u * FS), dm, lv);
            expect(what, dm < 4.0 && lv > -6 && lv < 1.5);
            for (t = 0; t < NT; t++)
                in_l[t] = in_r[t] = 8000;
            run(120, 1, ev, 1, 2u * FS);
            {
                int32_t ml = (int32_t)lrint(mean_of(out_l, FS, 2u * FS)), mr = (int32_t)lrint(mean_of(out_r, FS, 2u * FS));
                snprintf(what, sizeof what, "  DC through, steady, at the peaks' gain (%.3f): 8000 in, L %d R %d out", M[m].dc, ml, mr);
                expect(what, fabs(ml - 8000 * M[m].dc) <= 80 && fabs(mr - 8000 * M[m].dc) <= 80 && peak(out_l, FS, 2u * FS) - abs(ml) <= 8);
            }
        }
        {   /* bounded: 4x full-scale squares through it with the FILTER and REPEAT */
            ev_t ev[] = {{0, PF_R16 + 1}, {0, e + 1}};
            int32_t pk;
            for (t = 0; t < NT; t++) {
                in_l[t] = (t / 37u) & 1u ? 120000 : -120000;
                in_r[t] = (t / 53u) & 1u ? 120000 : -120000;
            }
            run_filt = -30;
            run(120, 1, ev, 2, 100000);
            pk = peak(out_l, 0, 100000) > peak(out_r, 0, 100000) ? peak(out_l, 0, 100000) : peak(out_r, 0, 100000);
            snprintf(what, sizeof what, "  4x full scale with the FILTER + REPEAT: peak %d (no wrap)", pk);
            expect(what, pk < 300000);
        }
        {   /* the sweep in time: a 16-sample pattern comes out the same a sweep later, at 120 and at 90 BPM */
            static const uint32_t BPM[2] = {120, 90};
            static const int16_t PAT[16] = {9000, -3000, 4000, 12000, -8000, 2000, -11000, 6000, 0, 7000, -5000, -12000,
                                            3000, 10000, -6000, -8000};
            uint32_t b;
            ev_t ev[] = {{0, e + 1}};
            for (t = 0; t < NT; t++)
                in_l[t] = in_r[t] = PAT[t % 16u];
            for (b = 0; b < 2u; b++) {
                uint32_t per = M[m].s16 * (FS * 60u / BPM[b] / 4u), wrong = M[m].s16 * (FS * 60u / 120u / 4u);
                int32_t ok_e, bad_e;
                run(BPM[b], 1, ev, 1, 8192u + 2u * per);
                ok_e = period_err(8192u, 8192u + per, per);
                bad_e = period_err(8192u, 8192u + per, b ? wrong : per / 2u);
                snprintf(what, sizeof what, "  %u BPM: the output repeats after %s (%u samples): error %d; after %s %d",
                         BPM[b], m ? "1/2 bar" : "a bar", per, ok_e, b ? "120 BPM's" : "half of it", bad_e);
                expect(what, ok_e <= 160 && bad_e > 1000);
            }
        }
        {   /* no click in or out (a 110 Hz sine); live again after the ramp, bit for bit */
            ev_t ev[] = {{992, e + 1}, {40000, -(e + 1)}};
            int32_t mx = 0;
            test_signal(110, 16000, 110, 16000);
            for (own = 0, t = 1; t < NT; t++)
                own = abs(in_l[t] - in_l[t - 1]) > own ? abs(in_l[t] - in_l[t - 1]) : own;
            run(133, 1, ev, 2, 49000);
            for (t = 1; t < 49000u; t++) {
                int32_t d = abs(out_l[t] - out_l[t - 1]);
                d = abs(out_r[t] - out_r[t - 1]) > d ? abs(out_r[t] - out_r[t - 1]) : d;
                mx = d > mx ? d : mx;
            }
            snprintf(what, sizeof what, "  no clicks on a 110 Hz sine, in and out: largest step %d (sine %d)", mx, own);
            expect(what, mx <= 4 * own);
            expect("  let go: the live signal again after the ramp, bit for bit; the stage off",
                   same(40000 + 640, 49000) && !pf.busy && !pf.fw && !pf.pw && !pf.fn);
        }
        {   /* stacking: with REPEAT 1/16 and the FILTER, let go of it: the rest as without it, bit for bit (after the ramp) */
            ev_t ev[] = {{0, PF_R16 + 1}, {992, e + 1}, {30000, -(e + 1)}};
            uint32_t diff = 0, during = 0;
            ev_t ev1[] = {{0, PF_R16 + 1}};
            test_signal(330, 12000, 220, 12000);
            run_filt = -40;
            run(120, 1, ev1, 1, 50000);                 /* REPEAT and the FILTER alone */
            memcpy(ref_l, out_l, 50000u * 4u);
            memcpy(ref_r, out_r, 50000u * 4u);
            run_filt = -40;
            run(120, 1, ev, 3, 50000);
            for (t = 0; t < 50000u; t++) {
                if (t >= 30000u + 640u)
                    diff += out_l[t] != ref_l[t] || out_r[t] != ref_r[t];
                if (t >= 12000u && t < 30000u)
                    during += out_l[t] != ref_l[t];
            }
            snprintf(what, sizeof what, "  with REPEAT 1/16 + the FILTER: it changes them (%u samples), let go they are as alone", during);
            expect(what, during > 10000u && !diff && (perf_act & PF_BIT(PF_R16)));
        }
    }
    {   /* both at once: bounded and both running; the right side a quarter sweep behind (L != R) */
        ev_t ev[] = {{0, PF_FLG + 1}, {0, PF_PHS + 1}};
        uint32_t lr = 0;
        test_signal(330, 12000, 330, 12000);
        for (t = 0; t < NT; t++)
            in_r[t] = in_l[t];
        run_filt = 30;
        run(120, 1, ev, 2, 80000);
        for (t = 20000; t < 80000u; t++)
            lr += out_l[t] != out_r[t];
        snprintf(what, sizeof what, "FLANGER + PHASER + the high-pass: the same input both sides, L and R apart (%u samples), peak %d", lr,
                 peak(out_l, 0, 80000));
        expect(what, lr > 50000u && peak(out_l, 0, 80000) < 40000 && pf.fw == 32768 && pf.pw == 32768);
    }
    {   /* K4 DEPTH: their level (0 % = the live signal) */
        ev_t ev[] = {{0, PF_FLG + 1}, {0, PF_PHS + 1}};
        int32_t d = 0;
        test_signal(330, 12000, 220, 12000);
        run_depth = 0;
        run(120, 1, ev, 2, 20000);
        run_depth = 100;
        for (t = 0; t < 20000u; t++)
            d = abs(out_l[t] - in_l[t]) > d ? abs(out_l[t] - in_l[t]) : d;
        snprintf(what, sizeof what, "K4 DEPTH 0 %%: FLANGER and PHASER held, the live signal (within %d of 12000)", d);
        expect(what, d == 0);
    }
}

/* -------------------------------------------------------------- DRIVE --- */
/* the share of x[a, b) not in its fundamental f: what is left after the least-squares sine at f, of all (b - a: many periods) */
static double harm_share(const int32_t *x, uint32_t a, uint32_t b, double f)
{
    double sc = 0, ss = 0, cc = 0, cs = 0, sn = 0, e = 0, r = 0, ca, sa, det;
    uint32_t t;
    for (t = a; t < b; t++) {
        double c = cos(2 * M_PI * f * t / FS), s_ = sin(2 * M_PI * f * t / FS);
        cc += c * c;
        ss += s_ * s_;
        cs += c * s_;
        sc += x[t] * c;
        sn += x[t] * s_;
        e += (double)x[t] * x[t];
    }
    det = cc * ss - cs * cs;
    ca = (sc * ss - sn * cs) / det;
    sa = (sn * cc - sc * cs) / det;
    for (t = a; t < b; t++) {
        double d = x[t] - ca * cos(2 * M_PI * f * t / FS) - sa * sin(2 * M_PI * f * t / FS);
        r += d * d;
    }
    return e > 0 ? r / e : 0;
}
/* (perform.c PERF_DRIVE_MODEL, built with -DPERF_DRIVE_MODEL=1..3 by hand: the share of the 220 Hz sine's energy that DEPTH 100 turns into
 * other frequencies at least, the peak at most: FOLD 30 %, 16384; TUBE 5 %; FUZZ 10 %; CRUSH 2 % (its steps and its aliases), 24576) */
#if PERF_DRIVE_MODEL == 0
#define DV_T_HARM 0.3
#elif PERF_DRIVE_MODEL == 1
#define DV_T_HARM 0.05
#elif PERF_DRIVE_MODEL == 2
#define DV_T_HARM 0.1
#else
#define DV_T_HARM 0.02
#endif
#define DV_T_PEAK (PERF_DRIVE_MODEL == 3 ? 24576 : 16384)
static void t_drive(void)
{
    char what[200];
    double h0, h100, hdry;
    uint32_t t;
    int32_t own, mx;
    ev_t ev[] = {{992, PF_DRV + 1}, {60000, -(PF_DRV + 1)}};
    test_signal(220, 12000, 0, 0);
    hdry = harm_share(in_l, 20000, 50000, 220);
    run_depth = 100;
    run(120, 1, ev, 2, 70000);
    h100 = harm_share(out_l, 20000, 50000, 220);
    snprintf(what, sizeof what, "DRIVE, DEPTH 100 %%, on a 220 Hz sine (12000): %.0f %% of it in harmonics (the sine %.1f %%); peak %d",
             100 * h100, 100 * hdry, peak(out_l, 20000, 50000));
    expect(what, h100 > DV_T_HARM && peak(out_l, 20000, 50000) <= DV_T_PEAK);
    expect("  a silent right side stays silent; let go: the live signal again, bit for bit", !peak(out_r, 0, 70000) && same(60000 + 640, 70000));
    run_depth = 0;
    test_signal(220, 8000, 220, 8000);
    run(120, 1, ev, 2, 70000);
    h0 = harm_share(out_l, 20000, 50000, 220);
    snprintf(what, sizeof what, "  DEPTH 0 %% on a quieter sine (8000): almost clean (%.2f %% harmonics), its level kept (peak %d)", 100 * h0,
             peak(out_l, 20000, 50000));
    expect(what, h0 < 0.01 && abs(peak(out_l, 20000, 50000) - 8000) < 400);
    run_depth = 100;
    test_signal(110, 16000, 110, 16000);
    for (own = 0, t = 1; t < NT; t++)
        own = abs(in_l[t] - in_l[t - 1]) > own ? abs(in_l[t] - in_l[t - 1]) : own;
    run_depth = 0;
    run(133, 1, ev, 2, 70000);
    run_depth = 100;
    for (mx = 0, t = 1; t < 70000u; t++)
        mx = abs(out_l[t] - out_l[t - 1]) > mx ? abs(out_l[t] - out_l[t - 1]) : mx;
    snprintf(what, sizeof what, "  DEPTH 0 on a 110 Hz sine, in and out: largest step %d (sine %d)", mx, own);
    expect(what, mx <= 4 * own);
    for (t = 0; t < NT; t++) {
        in_l[t] = (t / 37u) & 1u ? 120000 : -120000;
        in_r[t] = (t / 53u) & 1u ? 120000 : -120000;
    }
    run(120, 1, ev, 2, 70000);
    snprintf(what, sizeof what, "  4x full scale in: peak %d (bounded)", peak(out_l, 2000, 60000) > peak(out_r, 2000, 60000) ? peak(out_l, 2000, 60000) : peak(out_r, 2000, 60000));
    expect(what, peak(out_l, 2000, 60000) <= DV_T_PEAK && peak(out_r, 2000, 60000) <= DV_T_PEAK);
}
/* DRIVE on the song: MASTER still scales it (DRIVE comes before the level); its loudness against the dry song */
static uint32_t drv_master;
static void drv_prep(void) { song.master_q12 = drv_master; }
static void drv_at(uint32_t t)
{
    if (t == 0u)
        perf_press(PF_DRV, 1);
}
static void t_drive_song(void)
{
    static int32_t a_l[3u * 44100u + CTL];
    uint32_t n = 3u * FS, t;
    double rd, rw, lv;
    int32_t pa, pb;
    char what[200];
    song_setup();
    perf_reset();
    song_render(n, 0);
    rd = rms_of(song_l, FS, n);
    song_setup();
    perf_reset();
    song_render(n, drv_at);
    rw = rms_of(song_l, FS, n);
    memcpy(a_l, song_l, n * 4u);
    pa = peak(song_l, FS, n);
    perf_press(PF_DRV, 0);
    lv = 20 * log10(rw / rd);
    song_setup();
    perf_reset();
    song.master_q12 = 1024;                             /* MASTER at a quarter */
    song_render(n, drv_at);
    song.master_q12 = 4096;
    pb = peak(song_l, FS, n);
    perf_press(PF_DRV, 0);
    for (t = 0; t < 1024u; t += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
    }
    snprintf(what, sizeof what, "DRIVE held on the song (DEPTH 100 %%): %+.1f dB against it dry; MASTER at a quarter: peak %d -> %d (it follows)",
             lv, pa, pb);
    expect(what, lv > -6 && lv < 6 && pb < pa / 3 && pb > pa / 6);
}

/* --------------------------------------------------------------- PUMP --- */
static void t_pump(void)
{
    char what[200];
    uint32_t b = FS * 60u / 120u, k, t;
    int32_t in_pk = 12000, deep = 0, back = 1 << 30, own, mx;
    ev_t ev[] = {{992, PF_PUMP + 1}, {5u * FS - 32u * 0u, -(PF_PUMP + 1)}};
    test_signal(440, 12000, 440, 12000);
    run(120, 1, ev, 1, 5u * FS);
    for (k = 1; k < 9u; k++) {                          /* beats 1 .. 8 (the press came after beat 0's) */
        int32_t d = peak(out_l, k * b + 150u, k * b + 500u), r = peak(out_l, k * b + b * 9u / 10u, k * b + b - 64u);
        deep = d > deep ? d : deep;
        back = r < back ? r : back;
    }
    snprintf(what, sizeof what, "PUMP, DEPTH 100 %%, 120 BPM: every beat the master ducks (0.95, 2 ms), 150 .. 500 samples on at most %d of %d (-%.0f dB), back to %d before the next",
             deep, in_pk, 20 * log10((double)in_pk / (deep ? deep : 1)), back);
    expect(what, deep < in_pk / 8 && back > in_pk * 9 / 10 && peak(out_l, 992, b) == peak(in_l, 992, b));
    {
        int32_t lvl[4];
        int dep;
        for (dep = 0; dep < 4; dep++) {                 /* DEPTH 25 50 75 %: the duck as deep */
            run_depth = (int8_t)(25 * (dep + 1));
            run(120, 1, ev, 1, 2u * FS);
            lvl[dep] = peak(out_l, b + 200u, b + 500u);
        }
        run_depth = 0;
        run(120, 1, ev, 1, 2u * FS);
        run_depth = 100;
        snprintf(what, sizeof what, "  K4 DEPTH 25 / 50 / 75 / 100 %%: ducks to %d / %d / %d / %d of 12000; DEPTH 0: nothing (bit for bit)",
                 lvl[0], lvl[1], lvl[2], lvl[3]);
        expect(what, lvl[0] > lvl[1] && lvl[1] > lvl[2] && lvl[2] > lvl[3] && abs(lvl[1] - 12000 * 53 / 100) < 600 && same(0, 2u * FS));
    }
    {   /* let go mid-release: the duck releases, no new one; then the stage rests */
        ev_t e2[] = {{992, PF_PUMP + 1}, {b + 1024u, -(PF_PUMP + 1)}};
        run(120, 1, e2, 2, 4u * FS);
        expect("  let go: the duck releases, no new one on the next beats, the stage at rest (live, bit for bit)",
               peak(out_l, 2u * b + 200u, 2u * b + 500u) > 11500 && same(3u * b, 4u * FS) && !pf.busy);
    }
    test_signal(110, 16000, 110, 16000);
    for (own = 0, t = 1; t < NT; t++)
        own = abs(in_l[t] - in_l[t - 1]) > own ? abs(in_l[t] - in_l[t - 1]) : own;
    run(120, 1, ev, 1, 3u * FS);
    for (mx = 0, t = 1; t < 3u * FS; t++)
        mx = abs(out_l[t] - out_l[t - 1]) > mx ? abs(out_l[t] - out_l[t - 1]) : mx;
    snprintf(what, sizeof what, "  no click: its 2 ms duck on a 110 Hz sine: largest step %d (sine %d)", mx, own);
    expect(what, mx <= 4 * own);
}
/* PUMP on the song: the dips on the transport's beats (seq.c counts them), the parts' DUCK settings untouched */
static void pump_at(uint32_t t)
{
    if (t == 2048u)
        perf_press(PF_PUMP, 1);
}
static void t_pump_song(void)
{
    static int32_t dl[3u * 44100u + CTL];
    uint32_t n = 3u * FS, b = FS * 60u / 120u, k, t, pre = 0;
    double dip = 0, back = 0;
    int16_t dk0, dk1;
    char what[200];
    song_setup();
    perf_reset();
    dk0 = trk[0].p[P_DKDEP];
    dk1 = trk[1].p[P_DKDEP];
    song_render(n, pump_at);                            /* PUMP held from 2048 */
    perf_press(PF_PUMP, 0);
    memcpy(dl, song_l, n * 4u);
    song_setup();
    perf_reset();
    song_render(n, 0);                                  /* the song dry */
    for (t = 0; t < 2048u; t++)
        pre += dl[t] != song_l[t];
    for (k = 1; k < 5u; k++) {                          /* each beat (the transport's, seq.c): just after it, and just before the next */
        dip += 20 * log10((rms_of(dl, k * b + 64u, k * b + 600u) + 1) / (rms_of(song_l, k * b + 64u, k * b + 600u) + 1)) / 4;
        back += 20 * log10((rms_of(dl, k * b + b - 1500u, k * b + b - 300u) + 1) / (rms_of(song_l, k * b + b - 1500u, k * b + b - 300u) + 1)) / 4;
    }
    snprintf(what, sizeof what, "PUMP on the song (120 BPM): %.0f dB just after each beat, %.1f dB just before the next; the DUCKs' settings untouched",
             dip, back);
    expect(what, dip < -15 && back > -2 && trk[0].p[P_DKDEP] == dk0 && trk[1].p[P_DKDEP] == dk1);
    (void)pre;                                          /* (the two renders follow each other: the buses' tails differ a little) */
}


/* ============================================================ K2 SPACE (fx.c sv, fx_bus.c SV_*) --- */
static int32_t sp_l[25u * 44100u + CTL], sp_r[25u * 44100u + CTL];
/* the song for frames, the transport stopped at stop (its notes let go: the tails ring on); SPACE at sp from the start (no glide) */
static void space_render(int sp, uint32_t frames, uint32_t stop)
{
    uint32_t t, i, st = 0;
    int32_t o[2 * CTL];
    perf_space = (int8_t)sp;
    sv.spc = sp * 256;                                  /* (at once: no glide from the middle) */
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (t >= stop && !st) {
            transport_req = 2;
            st = 1;
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            sp_l[t + i] = o[2 * i];
            sp_r[t + i] = o[2 * i + 1];
        }
    }
    transport_req = 2;
    mix_block(o, CTL);
}
static void space_clear(void)                           /* the buses from silence (as a power-on), SPACE neutral */
{
    perf_space = 0;
    memset(&sv, 0, sizeof sv);
    sv.sg = sv.sg0 = 32768;
    memset(dly_buf, 0, sizeof dly_buf);
    dm_lines_clear();
    fx.dm_dev = fx_b.dm_dev = 0;
    fx.cho_ph = fx_b.cho_ph = fx.cho_w = fx_b.cho_w = 0;   /* (the chorus's LFO from the top: two renders compared sample for sample, 0.2.9's
                                                         * send floor feeds the chorus with the sends at 0) */
    fx.dm_xl = fx.dm_xr = fx_b.dm_xl = fx_b.dm_xr = 0;
    dc_l = dc_r = dce_l = dce_r = 0;                    /* (the master's DC blockers and low cuts from rest: a side silent is 0) */
}
static double energy(const int32_t *x, uint32_t a, uint32_t b)
{
    double e = 0;
    for (; a < b; a++)
        e += (double)x[a] * x[a];
    return e;
}
/* 0.2.9 (the owner: SPACE turned to the right glitched). A tone straight into one bus's send (the chorus, or the reverb of each model), in
 * STEREO and DUEL, SPACE turned from the middle to +100 (as fast as it glides) and held: the return's roughness at a block's first samples
 * against the others (the mean third difference; 1 = no seam). Before: the chorus's depth jumped once a block (x 15 in STEREO), SPRING's
 * loop length a whole sample a block (x 3.4) */
static int32_t gl_w[700u * CTL];
static double space_seams(uint32_t which, uint32_t rt, uint32_t mode, int32_t *pk)
{
    uint32_t b, i, t = 0, nb = 0, no = 0;
    double eb = 0, eo = 0;
    space_clear();
    duell_mode_force(mode);
    song.g[G_RTYPE] = (int16_t)rt;
    song.g[G_RSIZE] = 90;
    song.g[G_RDAMP] = 60;
    song.g[G_CDEPTH] = 60;
    song.g[G_DMDEP] = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    for (b = 0; b < 700u; b++, t += CTL) {
        for (i = 0; i < CTL; i++) {
            double ph = 2 * M_PI * 1000.0 * (t + i) / 44100.0;
            int32_t x = (int32_t)(8000 * sin(ph) + 3000 * sin(ph * 3.7));
            send_c[i] = which ? 0 : x;
            send_r[i] = which ? x : 0;
            send_d[i] = mix_l[i] = mix_r[i] = 0;
        }
        perf_space = (int8_t)(b < 200u ? 0 : 100);
        if (dm_split) {
            for (i = 0; i < CTL; i++)
                sa_c[i] = send_c[i], sa_d[i] = send_d[i], sa_r[i] = send_r[i], send_c[i] = send_d[i] = send_r[i] = 0;
        }
        if (perf_space || sv.spc) {                     /* (as mix_block) */
            space_step();
            if (sv.on || sv.sg0 != 32768) {
                space_sends(send_c, send_d, send_r, sp_fc, sp_fd, sp_fr, CTL);
                if (dm_split)
                    space_sends(sa_c, sa_d, sa_r, sp_fc_a, sp_fd_a, sp_fr_a, CTL);
            }
        }
        if (dm_split) {
            split_buses(CTL);
            for (i = 0; i < CTL; i++)
                gl_w[t + i] = mix_l[i];
        } else {
            fx_buses(send_c, send_d, send_r, wet, CTL);
            for (i = 0; i < CTL; i++)
                gl_w[t + i] = wet[i];
        }
    }
    *pk = peak(gl_w, 0, t);
    for (i = 200u * CTL + 3u; i < t; i++) {             /* (a step between i - 1 and i shows at i .. i + 2) */
        double d = fabs((double)gl_w[i] - 3.0 * gl_w[i - 1] + 3.0 * gl_w[i - 2] - gl_w[i - 3]);
        if (i % CTL <= 2u)
            eb += d, nb++;
        else
            eo += d, no++;
    }
    perf_space = 0;
    space_clear();
    return (eb / nb) / (eo / no + 1e-9);
}
static uint32_t reso_rails(void)                        /* RESO's comb cells at the int16 limit, both buses */
{
    uint32_t i, c = 0;
    for (i = 0; i < 4u * RS_LEN; i++)
        c += (rev_comb[i] >= 32767 || rev_comb[i] <= -32768) + (rev_comb_b[i] >= 32767 || rev_comb_b[i] <= -32768);
    return c;
}
/* SPACE's stereo width (0.2.9, fx.c space_width): the test song (part 1 at PAN -40, part 2 at +40: stereo), STEREO and LAYER, every reverb */
static void t_space_width(void)
{
    char what[240];
    uint32_t m, k, t;
    for (m = 0; m < 2u; m++) {
        uint32_t neq = 0;
        double sm[3] = {0}, ss[3] = {0}, rr[6];
        int32_t pk[3] = {0}, pmax = 0;
        for (k = 0; k < 6u; k++) {
            uint32_t sp;
            for (sp = 0; sp < 3u; sp++) {                /* -100, 0, +100 */
                song_setup();
                space_clear();
                duell_mode_force(m ? DM_LAYER : DM_STEREO);
                song.g[G_RTYPE] = (int16_t)k;
                space_render(sp == 0u ? -100 : sp == 1u ? 0 : 100, 3u * FS, 3u * FS);
                sm[sp] = ss[sp] = 0;
                for (t = FS; t < 3u * FS; t++) {
                    double a = ((double)sp_l[t] + sp_r[t]) / 2, d = ((double)sp_l[t] - sp_r[t]) / 2;
                    sm[sp] += a * a, ss[sp] += d * d;
                    if (sp == 0u && t >= 2u * FS)
                        neq += sp_l[t] != sp_r[t];
                }
                pk[sp] = peak(sp_l, FS, 3u * FS) > peak(sp_r, FS, 3u * FS) ? peak(sp_l, FS, 3u * FS) : peak(sp_r, FS, 3u * FS);
            }
            rr[k] = 0;
            pmax = pk[2] > pmax ? pk[2] : pmax;
        }
        (void)rr;
        snprintf(what, sizeof what, "  %s, the song, every reverb: SPACE -100 mono, L == R (%u samples differ in the last second); +100 peak %d",
                 m ? "LAYER" : "STEREO", neq, pmax);
        expect(what, !neq && pmax < 32768);
    }
    {   /* the stage alone (space_width) on a stereo block: noise in the mid and (r x) in the side, SPACE held at +100 / -100 for 0.5 s: the side
         * x 1.9 less the guard, the loudness kept (the guard: within 1 dB), at -100 no side */
        static const double SR[3] = {0.1, 0.5, 1.0};
        double out[3][2], rr[3];
        uint32_t j, b, i;
        for (j = 0; j < 3u; j++)
            for (k = 0; k < 2u; k++) {
                double ei = 0, eo = 0, so = 0, si = 0;
                sw_a = 4096, sw_b = 0, sw_em = sw_es = 0, sw_g = 4096;
                sv.spc = sv.spc0 = k ? -25600 : 25600;
                for (b = 0; b < 700u; b++) {
                    for (i = 0; i < CTL; i++) {
                        int32_t mm = (int32_t)(rng() >> 18) - 8192, ss2 = (int32_t)(((int32_t)(rng() >> 18) - 8192) * SR[j]);
                        mix_l[i] = mm + ss2, mix_r[i] = mm - ss2, wet[i] = 0;
                        if (b >= 200u)
                            ei += (double)mix_l[i] * mix_l[i] + (double)mix_r[i] * mix_r[i], si += (double)ss2 * ss2;
                    }
                    space_width(CTL);
                    if (b >= 200u)
                        for (i = 0; i < CTL; i++) {
                            double d = (mix_l[i] - (double)mix_r[i]) / 2;
                            eo += (double)mix_l[i] * mix_l[i] + (double)mix_r[i] * mix_r[i], so += d * d;
                        }
                }
                out[j][k] = k ? so : 10 * log10(eo / ei);
                if (!k)
                    rr[j] = 10 * log10(so / si);
            }
        sv.spc = sv.spc0 = 0;
        sw_a = 4096, sw_b = 0, sw_em = sw_es = 0, sw_g = 4096;
        snprintf(what, sizeof what, "  the width stage, SPACE +100 (side / mid 0.1, 0.5, 1): the side %+.1f %+.1f %+.1f dB, the loudness %+.1f %+.1f %+.1f dB; -100: no side",
                 rr[0], rr[1], rr[2], out[0][0], out[1][0], out[2][0]);
        expect(what, rr[0] > 4.5 && rr[1] > 1.5 && rr[2] > 0.5 && fabs(out[0][0]) < 1 && fabs(out[1][0]) < 1 && fabs(out[2][0]) < 1 &&
               out[0][1] == 0 && out[1][1] == 0 && out[2][1] == 0);
    }
    {   /* turning SPACE -100 -> +100 while the song plays: no seam at the blocks' starts on the output; DUEL: the width never runs */
        int32_t o[2 * CTL];
        double eb = 0, eo = 0;
        uint32_t nb = 0, no = 0;
        song_setup();
        space_clear();
        duell_mode_force(DM_STEREO);
        transport_req = 1;
        for (t = 0; t < 4u * FS; t += CTL) {
            uint32_t i, b = t / CTL;
            perf_space = (int8_t)(b < 200u ? -100 : b < 400u ? (int32_t)b - 300 : 100);
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++)
                sp_l[t + i] = o[2 * i];
        }
        transport_req = 2;
        for (t = 200u * CTL + 3u; t < 4u * FS; t++) {
            double d = fabs((double)sp_l[t] - 3.0 * sp_l[t - 1] + 3.0 * sp_l[t - 2] - sp_l[t - 3]);
            if (t % CTL <= 2u)
                eb += d, nb++;
            else
                eo += d, no++;
        }
        perf_space = 0;
        snprintf(what, sizeof what, "  the width turned -100 -> +100 while the song plays: block seams x %.2f (1 = none)", (eb / nb) / (eo / no));
        expect(what, (eb / nb) / (eo / no) < 1.3);
        space_clear();
        sw_a = 4096, sw_b = 0;
        song_setup();
        duell_mode_force(DM_SPLIT);
        space_render(-100, FS, FS);
        space_render(100, FS, FS);
        expect("  DUEL: SPACE -100 and +100, the width never ran (its gains at rest: the two parts stay apart)", sw_a == 4096 && sw_b == 0);
        space_clear();
    }
}

static void t_space_glitch(void)
{
    static const char *const RT[6] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};
    char what[240];
    uint32_t m, k;
    {   /* the chorus's DEPTH at +100: the knob's largest (762), not 2x it; a stored depth above that would be kept (none can be: 127 x 6) */
        static const int16_t CD[4] = {0, 60, 100, 127};
        int32_t got[4], ok = 1;
        for (k = 0; k < 4u; k++) {
            space_clear();
            song.g[G_CDEPTH] = CD[k];
            sv.spc = 25600;
            sv.on = 1;
            space_vals();
            got[k] = sv.cdep;
            ok &= got[k] == (CD[k] * 6 > 762 ? CD[k] * 6 : 762);
        }
        space_clear();
        snprintf(what, sizeof what, "  SPACE +100: the chorus DEPTH %d %d %d %d (stored 0 60 100 127): the knob's largest, 762 (0.2.8: 1524)", got[0], got[1],
                 got[2], got[3]);
        expect(what, ok);
    }
    for (m = 0; m < 2u; m++) {
        double worst = 0, r[6];
        int32_t pk, pmax = 0;
        for (k = 0; k < 6u; k++) {
            r[k] = space_seams(k ? 1u : 0u, k ? k - 1u : 0u, m ? DM_SPLIT : DM_STEREO, &pk);
            worst = r[k] > worst ? r[k] : worst;
            pmax = pk > pmax ? pk : pmax;
        }
        snprintf(what, sizeof what, "  %s, a tone into the bus, SPACE 0 -> +100: block seams x %.2f chorus, x %.2f %.2f %.2f %.2f %.2f ROOM .. RESO (1 = none), peak %d",
                 m ? "DUEL" : "STEREO", r[0], r[1], r[2], r[3], r[4], r[5], pmax);
        expect(what, worst < 1.3 && pmax < 65536);
    }
    /* the send floor (0.2.9, the owner: SPACE adds space also when the sends are 0): every send 0, the song stopped at 1 s, its tail from 1.5 s
     * (the reverb and the delay alone: the dry has ended) at SPACE +100 against the middle (silent there), STEREO and DUEL; DUEL with part 1
     * alone: its bus's floor only, the right side (bus B) exactly 0 */
    for (m = 0; m < 3u; m++) {
        double e0 = 0, e1 = 0;
        int32_t rpk = 0;
        uint32_t sp, t;
        for (sp = 0; sp < 2u; sp++) {
            song_setup();
            space_clear();
            duell_mode_force(m ? DM_SPLIT : DM_STEREO);
            for (k = 0; k < 2u; k++)
                trk[k].p[P_REV] = trk[k].p[P_DLY] = trk[k].p[P_CHOR] = 0;
            if (m == 2u)
                for (t = 0; t < 16u; t++)
                    trk[1].step[t].time = ST_REST, trk[1].step[t].n = 0;
            space_render(sp ? 100 : 0, 3u * FS, FS);
            if (sp)
                e1 = energy(sp_l, FS + FS / 2u, 3u * FS) + energy(sp_r, FS + FS / 2u, 3u * FS), rpk = peak(sp_r, 0, 3u * FS);
            else
                e0 = energy(sp_l, FS + FS / 2u, 3u * FS) + energy(sp_r, FS + FS / 2u, 3u * FS);
        }
        if (m < 2u) {
            snprintf(what, sizeof what, "  %s, every send 0: the tail from 1.5 s %.0f dB in the middle, %.1f dB at SPACE +100 (its send floor: the space added)",
                     m ? "DUEL" : "STEREO", 10 * log10(e0 / (3.0 * FS) + 1e-9) - 90.3, 10 * log10(e1 / (3.0 * FS) + 1e-9) - 90.3);
            expect(what, e0 / (3.0 * FS) < 1e2 && e1 / (3.0 * FS) > 1e5);   /* (the middle under -70 dB, +100 over -40 dB) */
        } else {
            snprintf(what, sizeof what, "  DUEL, part 1 alone, every send 0, SPACE +100: its tail %.1f dB on the left, the right side (bus B) exactly 0",
                     10 * log10(e1 / (3.0 * FS) + 1e-9) - 90.3);
            expect(what, e1 / (3.0 * FS) > 1e5 && rpk == 0);
        }
    }
    /* RESO: a held chord (part 1 and part 2, M01's reverb send and SIZE 90), SPACE +100 for 3 s: its combs (nearly) off their int16 limit (a
     * hard clip inside the loop). Before (SIZE to 150: 0.9992, the send x 1.6): 5 % of the comb cells at the limit at any moment; the middle
     * itself touches it now and then (a few cells in 1 s) */
    for (m = 0; m < 2u; m++) {
        static const uint8_t CH[4] = {48, 55, 60, 64};
        int32_t o[2 * CTL];
        uint32_t t, rails = 0, scans = 0;
        song_setup();
        space_clear();
        duell_mode_force(m ? DM_SPLIT : DM_STEREO);
        song.g[G_RTYPE] = 4;
        song.g[G_RSIZE] = 90;
        song.g[G_RDAMP] = 60;
        trk[0].p[P_REV] = trk[1].p[P_REV] = 89;
        for (k = 0; k < 4u; k++)
            trk_note_on(&trk[0], CH[k], 110), trk_note_on(&trk[1], CH[k] + 12u, 110);
        for (t = 0; t < 4u * FS; t += CTL) {
            perf_space = (int8_t)(t < FS ? 0 : 100);
            mix_block(o, CTL);
            if ((t / CTL) % 16u == 0u) {
                if (t >= FS)
                    rails += reso_rails(), scans++;
            }
        }
        for (k = 0; k < 4u; k++)
            trk_note_off(&trk[0], CH[k]), trk_note_off(&trk[1], CH[k] + 12u);
        perf_space = 0;
        snprintf(what, sizeof what, "  RESO, a held chord, SPACE +100 (SIZE to %d at most, its send x 1, its floor 10 %%): %.2f %% of the comb cells at the int16 limit (0.2.8: 5 %%)",
                 SV_RESO_TOP, 100.0 * rails / (scans * 8.0 * RS_LEN));
        expect(what, rails * 100u < scans * 8u * RS_LEN);
        space_clear();
    }
}

static void t_space(void)
{
    char what[240];
    static const char *const RT[6] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};
    double e[3];
    uint32_t k, t, n;
    int sp;
    /* the tails: shorter to the left, longer to the right; the stored values untouched */
    for (k = 0; k < 3u; k++) {
        int16_t g0[4];
        song_setup();
        space_clear();
        duell_mode_force(DM_STEREO);
        song.g[G_RTYPE] = 2;                            /* HALL */
        g0[0] = song.g[G_RSIZE];
        g0[1] = song.g[G_DFDBK];
        g0[2] = song.g[G_CDEPTH];
        g0[3] = trk[0].p[P_REV];
        sp = k == 0u ? -100 : k == 1u ? 0 : 100;
        space_render(sp, 4u * FS, FS);
        e[k] = energy(sp_l, FS + FS / 2u, 4u * FS) + energy(sp_r, FS + FS / 2u, 4u * FS);
        if (k == 2u)
            expect("  the stored SIZE, FDBK, chorus DEPTH and the parts' sends untouched by SPACE",
                   g0[0] == song.g[G_RSIZE] && g0[1] == song.g[G_DFDBK] && g0[2] == song.g[G_CDEPTH] && g0[3] == trk[0].p[P_REV]);
    }
    snprintf(what, sizeof what, "K2 SPACE (HALL, the song stopped at 1 s): the tail from 1.5 s %+.1f dB at -100, %+.1f dB at +100 against the middle",
             10 * log10(e[0] / e[1]), 10 * log10(e[2] / e[1]));
    expect(what, e[0] < e[1] / 8 && e[2] > e[1] * 2);
    /* no runaway: every reverb at its largest SIZE, the delay at FDBK 127, the sends full, SPACE +100: the tail dies away (STEREO and DUEL) */
    for (k = 0; k < 12u; k++) {
        int32_t p1, pk;
        double em, el;
        song_setup();
        space_clear();
        duell_mode_force(k < 6u ? DM_STEREO : DM_SPLIT);
        song.g[G_RTYPE] = (int16_t)(k % 6u);
        song.g[G_RSIZE] = 127;
        song.g[G_RDAMP] = 0;
        song.g[G_DFDBK] = 127;
        song.g[G_DMIX] = 127;
        song.g[G_CDEPTH] = 127;
        trk[0].p[P_REV] = trk[1].p[P_REV] = 127;
        trk[0].p[P_DLY] = trk[1].p[P_DLY] = 127;
        trk[0].p[P_CHOR] = trk[1].p[P_CHOR] = 127;
        n = 24u * FS;
        space_render(100, n, FS);
        p1 = peak(sp_l, n - FS, n) > peak(sp_r, n - FS, n) ? peak(sp_l, n - FS, n) : peak(sp_r, n - FS, n);
        pk = peak(sp_l, 0, n) > peak(sp_r, 0, n) ? peak(sp_l, 0, n) : peak(sp_r, 0, n);
        em = energy(sp_l, 8u * FS, 10u * FS) + energy(sp_r, 8u * FS, 10u * FS);
        el = energy(sp_l, n - 2u * FS, n) + energy(sp_r, n - 2u * FS, n);
        snprintf(what, sizeof what, "  SPACE +100, %s %s, SIZE 127, FDBK 127, sends full: peak %d; 7 .. 9 s after the stop -> 21 .. 23 s: %.0f dB (it dies away), %d",
                 k < 6u ? "STEREO" : "DUEL", RT[k % 6u], pk, 10 * log10(el / (em + 1) + 1e-12), p1);
        expect(what, pk <= 32767 && el < em / 4);
    }
    /* the delay time: an echo's place (the delay's return alone: the render with the send less the one without), the middle against
     * -100 / +100, a 1/8 at 120 BPM (11025 samples): SPACE leaves the time alone (0.2.9, the owner: its pitch glide sounded weird) */
    {
        static int32_t a_l[FS];
        uint32_t on[3], snd;
        for (k = 0; k < 3u; k++) {
            int32_t th;
            for (snd = 0; snd < 2u; snd++) {
                song_setup();
                space_clear();
                duell_mode_force(DM_STEREO);
                for (t = 1; t < 16u; t++)
                    trk[0].step[t].time = ST_REST, trk[0].step[t].n = 0;
                for (t = 0; t < 16u; t++)
                    trk[1].step[t].time = ST_REST, trk[1].step[t].n = 0;
                trk[0].p[P_REV] = trk[0].p[P_CHOR] = 0;
                trk[0].p[P_DLY] = (int16_t)(snd ? 0 : 127);
                song.g[G_DFDBK] = 0;
                song.g[G_DMIX] = 127;
                song.g[G_DTIME] = 1;                    /* 1/8 (N_DIV: 1/4 1/8 1/16 1/32 ..) */
                sp = k == 0u ? -100 : k == 1u ? 0 : 100;
                perf_space = (int8_t)sp;
                sv.spc = sp * 256;
                {   /* 4 s for the time's glide, the transport stopped (nothing plays) */
                    int32_t o[2 * CTL];
                    for (t = 0; t < 4u * FS; t += CTL)
                        mix_block(o, CTL);
                }
                space_render(sp, FS, FS / 8u);          /* step 0's note (the transport from the top), stopped at 1/16 */
                if (!snd)
                    memcpy(a_l, sp_l, sizeof a_l);
            }
            for (t = 0; t < FS; t++)
                a_l[t] -= sp_l[t];                      /* (the delay's return alone) */
            th = peak(a_l, 0, FS) / 10;
            for (on[k] = 0; on[k] < FS && abs(a_l[on[k]]) < th; on[k]++)
                ;
        }
        snprintf(what, sizeof what, "  the delay's echo (a 1/8, FDBK 0): at %u samples in the middle, %u at -100, %u at +100 (SPACE keeps the delay time)", on[1],
                 on[0], on[2]);
        expect(what, on[1] > 10000 && on[1] < 12500 && abs((int)on[0] - (int)on[1]) <= 8 && abs((int)on[2] - (int)on[1]) <= 8);
    }
    /* DUEL: part 1 alone, SPACE turned both ways: the right side (bus B, part 2's) stays silent */
    {
        uint32_t sil = 0;
        song_setup();
        space_clear();
        duell_mode_force(DM_SPLIT);
        for (t = 0; t < 16u; t++)
            trk[1].step[t].time = ST_REST, trk[1].step[t].n = 0;
        space_render(-100, 2u * FS, 2u * FS);
        sil += peak(sp_r, 0, 2u * FS) != 0;
        space_render(100, 2u * FS, 2u * FS);
        sil += peak(sp_r, 0, 2u * FS) != 0;
        expect("  DUEL, part 1 alone, SPACE -100 and +100: the right side (part 2's bus B) silent, exactly", !sil && peak(sp_l, 0, 2u * FS) > 1000);
    }
    /* turning it: no click (the song with its sends; SPACE swept 0 -> -100 -> +100 -> 0 over 4 s, a detent a block), against still */
    {
        int32_t mx_t = 0, mx_s = 0, o[2 * CTL];
        static int32_t ref[6u * 44100u + CTL];
        song_setup();
        space_clear();
        duell_mode_force(DM_STEREO);
        song.g[G_RTYPE] = 2;
        space_render(0, 6u * FS, 6u * FS);
        memcpy(ref, sp_l, 6u * FS * 4u);
        for (t = 1; t < 6u * FS; t++)
            mx_s = abs(ref[t] - ref[t - 1]) > mx_s ? abs(ref[t] - ref[t - 1]) : mx_s;
        song_setup();
        space_clear();
        duell_mode_force(DM_STEREO);
        song.g[G_RTYPE] = 2;
        transport_req = 1;
        for (t = 0; t < 6u * FS; t += CTL) {
            uint32_t i, b = t / CTL;                    /* 0 .. -100 in 100 blocks, then to +100 in 200, then to 0 in 100 (a detent a block) */
            if (b >= 200u && b < 300u) perf_space = (int8_t)-(int32_t)(b - 200u);
            else if (b >= 300u && b < 500u) perf_space = (int8_t)(-100 + (int32_t)(b - 300u));
            else if (b >= 500u && b < 600u) perf_space = (int8_t)(100 - (int32_t)(b - 500u));
            else if (b >= 600u) perf_space = 0;
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++)
                sp_l[t + i] = o[2 * i];
        }
        transport_req = 2;
        mix_block(o, CTL);
        for (t = 1; t < 6u * FS; t++)
            mx_t = abs(sp_l[t] - sp_l[t - 1]) > mx_t ? abs(sp_l[t] - sp_l[t - 1]) : mx_t;
        snprintf(what, sizeof what, "  SPACE turned round (-100 .. +100 .. 0) while the song plays: largest step %d (still: %d)", mx_t, mx_s);
        expect(what, mx_t <= mx_s * 3 / 2);
        for (t = 0; t < 4u * FS; t += CTL)
            mix_block(o, CTL);
        expect("  back in the middle, its glides over: the buses' own values again (the plain delay)", !sv.on && sv.sg == 32768);
    }
    t_space_glitch();
    t_space_width();
}
/* -------------------------------------------- the song: after an effect --- */
static void after_at(uint32_t t)
{
    if (t == 44032u)
        perf_press(PF_R16, 1);
    if (t == 88064u)
        perf_press(PF_R16, 0);
}
static void t_after(void)
{
    uint32_t f;
    int32_t o[2 * CTL];
    song_setup();
    perf_reset();
    song_render(2u * FS, after_at);
    for (f = 0; f < 1024u; f += CTL)
        mix_block(o, CTL);
    expect("the song: after REPEAT is let go and its ramp is over, PERFORM's stages skipped again", perf_idle() && !pf.busy &&
                                                                                                          pf.mode == BM_NONE);
}


/* ------------------------------------------------- PERF LEAVE (0.2.14) --- */
/* the fade through the UI: PERFORM left, the values glide to neutral in 0.3 .. 0.5 s; KEEP leaves them; re-entering stops it; a SPACE the MIXER
 * set alone is not taken by merely opening and leaving PERFORM */
static void t_leave_ui(void)
{
    uint32_t i, n;
    int8_t prev_f, prev_s;
    int mono = 1, ok;
    fresh();
    perform_on();
    frame();
    perf_space = 0; perf_filt = 0;
    turn(EN_K2, 100);
    turn(EN_K3, -100);
    frame();
    expect("setup: SPACE and KILL turned to the ends in PERFORM", perf_space >= 90 && perf_filt <= -90);
    press(B_FX);                                         /* leave: FADE (the default) */
    prev_f = perf_filt; prev_s = perf_space;
    {
        int8_t f0 = perf_filt, s0 = perf_space;
        int maxstep = 0;
        n = 0;
        for (i = 0; i < 80u && (perf_filt || perf_space); i++) {
            int d;
            frame();
            n++;
            d = abs(perf_filt - prev_f) > abs(perf_space - prev_s) ? abs(perf_filt - prev_f) : abs(perf_space - prev_s);
            maxstep = d > maxstep ? d : maxstep;
            mono &= perf_filt >= prev_f && perf_space <= prev_s;
            prev_f = perf_filt; prev_s = perf_space;
        }
        (void)f0; (void)s0;
        expect("PERF LEAVE FADE: leaving PERFORM glides KILL and SPACE to 0, monotonic, a few detents a frame (no jump)", mono && !perf_filt && !perf_space && maxstep <= 5);
        printf("perform:   fade length %u ms from SPACE %d / KILL %d\n", n * 16u, 100, -100);
        expect("  .. in 0.3 .. 0.5 s from the ends", n * 16u >= 300u && n * 16u <= 500u);
    }
    expect("  .. DEPTH untouched", perf_depth == 100);
    /* KEEP: nothing moves */
    ui_idle |= PERF_KEEP_BIT;
    perform_on(); frame();
    turn(EN_K2, 100); turn(EN_K3, -100); frame();
    {
        int8_t s = perf_space, f = perf_filt;
        press(B_FX);
        frames(1500);
        ok = perf_space == s && perf_filt == f && s != 0 && f != 0 && !perf_fading;
        expect("PERF LEAVE KEEP: leaving PERFORM keeps SPACE and KILL (1.5 s later the same)", ok);
        perform_on(); frame();
        char want[12];
        snprintf(want, sizeof want, "+%d", s);
        expect("  .. back in PERFORM the cards show them, KILL not greyed", card_is(1, "SPACE", want) && !strncmp(ui.col[2], "KILL|", 5));
        perf_space = 0; perf_filt = 0;
        press(B_FX);
    }
    ui_idle &= (uint8_t)~PERF_KEEP_BIT;
    /* re-enter during the fade: it stops where it is */
    fresh();
    perform_on(); frame();
    turn(EN_K2, 100); turn(EN_K3, -100); frame();
    press(B_FX);
    frames(96);
    {
        int8_t s = perf_space, f = perf_filt;
        ok = perf_fading && (s > 0 || f < 0);
        btn_down(B_FX);                                   /* (FX held + a white key: PERFORM at once, before the 0.7 s hold) */
        key_down(wkey(0));
        frame(); frame();
        key_up(wkey(0));
        btn_up(B_FX);
        frame();
        ok &= ui.perf;
        frames(800);
        expect("re-entering PERFORM during the fade stops it: the values stay where they were", ok && !perf_fading && perf_space >= s - 4 && perf_filt <= f + 4 && (perf_space || perf_filt));
    }
    /* a SPACE set on the MIXER alone is not taken by opening and leaving PERFORM */
    fresh();
    perf_space = 40; perf_sp_live = 0;
    perform_on(); frame();
    press(B_FX);
    frames(800);
    expect("a SPACE set on the MIXER alone (+40) stays when PERFORM is opened and left", perf_space == 40);
    fresh();
    perform_on(); frame();
    turn(EN_K2, 30);
    press(B_FX);
    frames(800);
    expect("  .. PERFORM's own SPACE (moved there) is 0 after leaving, and a patch loaded afterwards starts from the patch's own sends", perf_space == 0 && sv.spc <= 256 * 30);
    perf_space = 0; perf_filt = 0; sv.spc = 0;
    perf_fading = 0; perf_sp_live = 0;
}

static uint32_t lv_first, lv_last;
static int32_t lv_step_c, lv_step_r;
static void leave_at(uint32_t t)
{
    fm1_ms = 100000u + t / 44u;                          /* (44.1 samples a ms) */
    if (t == 0u && (perf_filt || perf_space))
        perf_fade_begin(0);
    perf_fade_step();
}
/* a child renders the song with the KILL (and SPACE) set and the fade started at once; the parent without: where do they differ? */
static int leave_diff(void (*prep)(void), uint32_t frames_n)
{
    static int32_t b_l[10u * 44100u + CTL], b_r[10u * 44100u + CTL];
    int fd[2];
    pid_t pid;
    uint32_t t;
    if (pipe(fd) || (pid = fork()) < 0)
        return 0;
    if (!pid) {
        song_setup();
        prep();
        song_render(frames_n, leave_at);
        close(fd[0]);
        if (write(fd[1], song_l, frames_n * 4u) < 0 || write(fd[1], song_r, frames_n * 4u) < 0)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    song_setup();
    song_render(frames_n, leave_at);
    {
        FILE *f = fdopen(fd[0], "rb");
        size_t got = fread(b_l, 4, frames_n, f) + fread(b_r, 4, frames_n, f);
        fclose(f);
        waitpid(pid, 0, 0);
        if (got != 2u * frames_n)
            return 0;
    }
    lv_first = 0xFFFFFFFFu; lv_last = 0; lv_step_c = lv_step_r = 0;
    for (t = 0; t < frames_n; t++) {
        if (b_l[t] != song_l[t] || b_r[t] != song_r[t]) {
            lv_first = t < lv_first ? t : lv_first;
            lv_last = t;
        }
        if (t) {
            int32_t d = abs(b_l[t] - b_l[t - 1u]), e = abs(song_l[t] - song_l[t - 1u]);
            lv_step_c = d > lv_step_c ? d : lv_step_c;
            lv_step_r = e > lv_step_r ? e : lv_step_r;
        }
    }
    return 1;
}
static void lv_kill_prep(void) { perf_reset(); perf_filt = -60; sv.spc = 0; }
static void lv_kill_hp_prep(void) { perf_reset(); perf_filt = 100; }
static void lv_both_prep(void) { perf_reset(); perf_filt = -85; }
static void t_leave_audio(void)
{
    int ok = leave_diff(lv_kill_prep, 4u * FS);
    expect("audio: KILL -60 (a low-pass) faded on leaving: the song differs while it fades, then is bit for bit the dry song again (the filter's own settling inside 2 s)", ok && lv_first < FS / 4 && lv_last < 2u * FS);
    printf("perform:   last differing sample %u (%.0f ms)\n", lv_last, lv_last / 44.1);
    expect("  .. no click at the fade: no step of the faded song above 1.2 x the dry song's own largest step", lv_step_c <= lv_step_r * 12 / 10 + 8);
    ok = leave_diff(lv_both_prep, 4u * FS);
    expect("audio: KILL -85 (30 .. 150 Hz low-pass): back to the dry song bit for bit, no click", ok && lv_last < 2u * FS && lv_step_c <= lv_step_r * 12 / 10 + 8);
    ok = leave_diff(lv_kill_hp_prep, 4u * FS);
    printf("perform:   +100: last differing sample %u (%.0f ms), largest step %d (dry song %d)\n", lv_last, lv_last / 44.1, lv_step_c, lv_step_r);
    expect("audio: KILL +100 (the kill zone, silence) faded on leaving: no click (steps within 1.2 x the dry song's), bit for bit after the fade", ok && lv_last < 2u * FS && lv_step_c <= lv_step_r * 12 / 10 + 8);
    perf_reset();
}
static void lv_sp_prep(void) { perf_reset(); perf_space = 100; perf_sp_live = 1; }
static void t_leave_space(void)
{
    /* SPACE: the buses carry a tail, so the end is judged where the song has stopped sounding the old tail; the engine state returns to the middle */
    int32_t o[2 * CTL];
    uint32_t t;
    song_setup();
    perf_reset();
    perf_space = 100; perf_sp_live = 1; sv.spc = 100 * 256;
    fm1_ms = 200000u;
    perf_fade_begin(0);
    for (t = 0; t < 44100u; t += CTL) {
        fm1_ms = 200000u + t / 44u;
        perf_fade_step();
        mix_block(o, CTL);
    }
    expect("audio: SPACE +100 left and faded: the engine's SPACE is exactly 0 and the width stage off after 1 s (the output is the patch's own bus path)", perf_space == 0 && sv.spc == 0 && !sv.on);
    perf_reset();
    sv.spc = 0;
    (void)lv_sp_prep;
}

/* D (0.2.14): in PERFORM the black keys play in the scale, whatever OFF-SCALE says */
static void t_black_scale(void)
{
    static const uint8_t SC[] = {1, 2, 5, 9, 11};         /* MAJ MIN PEN WHOLE BLUES' neighbours: MAJ, MIN, PEN, WHOLE-ish, MEL (indices into SCALE_MASK) */
    uint32_t si, ro, k, inside = 1, outside = 1, chrom = 1, white_same = 1, any_off = 0, any_moved = 0;
    fresh();
    perform_on();
    frame();
    usb.config = 1;
    offscale_g = OS_PLAY;                                 /* the setting that never snapped */
    for (si = 0; si < sizeof SC; si++)
        for (ro = 0; ro < 12u; ro += 5u) {
            scale_g = SC[si];
            root_g = (int8_t)ro;
            for (k = 0; k < 27u; k++) {
                uint32_t note, raw = (uint32_t)(53 + (int32_t)k + 12 * song.octave + trk[song.sel].p[P_TRANS]);
                if (!key_black(k))
                    continue;
                ui.perf = 1; perf_keys = 1;
                key_down(k);
                events_block(CTL);
                note = kb_note[k];
                key_up(k);
                events_block(CTL);
                inside &= (SCALE_MASK[SC[si]] >> ((note + 120u - ro) % 12u)) & 1u;
                any_off |= !((SCALE_MASK[SC[si]] >> ((raw + 120u - ro) % 12u)) & 1u);
                any_moved |= note != raw;
                ui.perf = 0; perf_keys = 0;
                key_down(k);
                events_block(CTL);
                outside &= kb_note[k] == raw;
                key_up(k);
                events_block(CTL);
            }
        }
    expect("D: PERFORM, OFF-SCALE PLAY: every black key lands on a note of MAJ MIN PEN .. at roots C F A# (the scale and root of the SCALE page)", inside && any_off && any_moved);
    expect("  .. outside PERFORM the same keys play as they always did with PLAY (off-scale notes unchanged)", outside);
    scale_g = 0;
    for (k = 0; k < 27u; k++) {
        uint32_t raw = (uint32_t)(53 + (int32_t)k + 12 * song.octave + trk[song.sel].p[P_TRANS]);
        if (!key_black(k)) continue;
        ui.perf = 1; perf_keys = 1;
        key_down(k); events_block(CTL);
        chrom &= kb_note[k] == raw;
        key_up(k); events_block(CTL);
    }
    expect("  .. SCALE CHROMATIC (off): the black keys are untouched in PERFORM", chrom);
    scale_g = 1; root_g = 0; offscale_g = OS_SNAP;
    for (k = 0; k < 27u; k++) {
        uint32_t a, b;
        if (!key_black(k)) continue;
        ui.perf = 1; perf_keys = 1;
        key_down(k); events_block(CTL); a = kb_note[k]; key_up(k); events_block(CTL);
        ui.perf = 0; perf_keys = 0;
        key_down(k); events_block(CTL); b = kb_note[k]; key_up(k); events_block(CTL);
        white_same &= a == b;
    }
    expect("  .. with OFF-SCALE SNAP the keys are the same in and outside PERFORM (the setting's own snap)", white_same);
    scale_g = 0; root_g = 0; offscale_g = OS_SNAP;
    ui.perf = 0; perf_keys = 0;
    usb.config = 0;
    (void)outside;
}


/* -------------------------------------------------------------- COST --- */
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
/* the song's instructions a sample with effects es (n of them) held, DEPTH dep, K3 FILTER cost_filt */
static int8_t cost_filt, cost_space;
static int16_t cost_rt = -1;                            /* (the SPACE per model check: G_RTYPE, -1 the song's) */
static uint8_t cost_mode = DM_STEREO;
static double cost_run(const int *es, uint32_t n, int dep)
{
    uint32_t f, k;
    uint64_t i0;
    int32_t o[2 * CTL];
    song_setup();
    perf_reset();
    duell_mode_force(cost_mode);                        /* (the test song in STEREO, ROOM: the numbers SPEC 5e quotes) */
    if (cost_rt >= 0)
        song.g[G_RTYPE] = cost_rt;
    transport_req = 1;
    for (f = 0; f < FS; f += CTL)
        mix_block(o, CTL);
    perf_depth = (int8_t)dep;
    perf_filt = cost_filt;
    perf_space = cost_space;
    for (k = 0; k < n; k++)
        perf_press((uint32_t)es[k], 1);
    i0 = instr_now();
    for (f = 0; f < 2u * FS; f += CTL)
        mix_block(o, CTL);
    for (k = 0; k < PF_N; k++)
        perf_press(k, 0);
    perf_depth = 100;
    perf_filt = 0;
    perf_space = 0;
    transport_req = 2;
    return i0 ? (double)(instr_now() - i0) / (2.0 * FS) : 0;
}
static double cost_idle, cost_rep;
static void t_cost(void)
{
    static const int R16[] = {PF_R16}, OUP[] = {PF_OUP}, FRZ[] = {PF_FRZ}, TAPE[] = {PF_TAPE}, FLG[] = {PF_R16, PF_FLG}, PHS[] = {PF_R16, PF_PHS};
    static const int DRV[] = {PF_R16, PF_DRV}, PMP[] = {PF_R16, PF_PUMP}, ALL[] = {PF_R16, PF_FLG, PF_PHS, PF_DRV, PF_PUMP, PF_M2};
    double harm0, harm, frz, tape, flg, phs, drv, pmp, all, flt, flh, spc;
    char what[200];
    cost_idle = cost_run(0, 0, 100);
    cost_rep = cost_run(R16, 1, 100);
    harm0 = cost_run(OUP, 1, 0);
    harm = cost_run(OUP, 1, 100);
    frz = cost_run(FRZ, 1, 100);
    tape = cost_run(TAPE, 1, 100);
    flg = cost_run(FLG, 2, 100);
    phs = cost_run(PHS, 2, 100);
    drv = cost_run(DRV, 2, 100);
    pmp = cost_run(PMP, 2, 100);
    cost_space = 100;
    spc = cost_run(0, 0, 100);
    cost_space = 0;
    cost_filt = -50;
    flt = cost_run(R16, 1, 100);
    cost_filt = 50;
    flh = cost_run(R16, 1, 100);
    cost_filt = -50;
    all = cost_run(ALL, 6, 100);
    cost_filt = 0;
    if (!cost_idle) {
        printf("perform: cost: no instruction counter on this host\n");
        return;
    }
    snprintf(what, sizeof what, "cost: the song %.0f instructions / sample idle; REPEAT 1/16 +%.0f, TAPE STOP +%.0f, FREEZE +%.0f",
             cost_idle, cost_rep - cost_idle, tape - cost_idle, frz - cost_idle);
    expect(what, cost_rep - cost_idle < 200 && tape - cost_idle < 200 && frz - cost_idle < 250);
    snprintf(what, sizeof what, "  OCT UP over REPEAT 1/16 (the harmonizer itself): +%.0f, with the shimmer +%.0f (device ~%.1f / %.1f %%, about 2 %%)",
             harm0 - cost_rep, harm - cost_rep, (harm0 - cost_rep) * 0.017, (harm - cost_rep) * 0.017);
    expect(what, (harm - cost_rep) * 0.017 <= 2.2);
    snprintf(what, sizeof what, "  over REPEAT 1/16: FLANGER +%.0f, PHASER +%.0f, DRIVE +%.0f, PUMP +%.0f, K3 FILTER -50 +%.0f / +50 +%.0f (device ~%.1f %% at most, 2.5 %% each)",
             flg - cost_rep, phs - cost_rep, drv - cost_rep, pmp - cost_rep, flt - cost_rep, flh - cost_rep,
             fmax(fmax(fmax(flg, phs), fmax(fmax(drv, pmp), flt)), flh) * 0.017 - cost_rep * 0.017);
    expect(what, fmax(fmax(fmax(flg, phs), fmax(fmax(drv, pmp), flt)), flh) - cost_rep <= 2.5 / 0.017);
    snprintf(what, sizeof what, "  K2 SPACE +100 (the sends' gain, the SIZE the models read; 0.2.8 also moved the delay's taps: +57): +%.0f over idle (device ~%.1f %%)",
             spc - cost_idle, (spc - cost_idle) * 0.017);
    expect(what, (spc - cost_idle) * 0.017 <= 1.0);
    {   /* per reverb model and mode: SPACE +100 against the middle, in the loadcost scale (33 host instructions a sample = 1 % of a half) */
        static const char *const RT[6] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};
        double d[2][6], worst = -1e9, worst2 = -1e9, c0;
        uint32_t m, k;
        for (m = 0; m < 2u; m++)
            for (k = 0; k < 6u; k++) {
                cost_mode = m ? DM_SPLIT : DM_STEREO;
                cost_rt = (int16_t)k;
                c0 = cost_run(0, 0, 100);
                cost_space = 100;
                d[m][k] = (cost_run(0, 0, 100) - c0) / 32.8;
                cost_space = 0;
                if (m)
                    worst2 = d[m][k] > worst2 ? d[m][k] : worst2;
                else
                    worst = d[m][k] > worst ? d[m][k] : worst;
            }
        cost_mode = DM_STEREO;
        cost_rt = -1;
        snprintf(what, sizeof what, "  SPACE +100 over the middle, %% of a half: STEREO %s %+.2f %s %+.2f %s %+.2f %s %+.2f %s %+.2f %s %+.2f; DUEL %+.2f %+.2f %+.2f %+.2f %+.2f %+.2f",
                 RT[0], d[0][0], RT[1], d[0][1], RT[2], d[0][2], RT[3], d[0][3], RT[4], d[0][4], RT[5], d[0][5], d[1][0], d[1][1], d[1][2], d[1][3], d[1][4], d[1][5]);
        expect(what, worst < 1.5 && worst2 < 1.5);       /* (STEREO / LAYER: the floors and the width; DUEL: two buses' gains and floors) */
    }
    {   /* the width stage itself (fx.c space_width), on a block of noise, SPACE +60 */
        uint64_t i0;
        uint32_t b, i, nb = 4000;
        sv.spc = sv.spc0 = 60 * 256;
        for (i = 0; i < CTL; i++)
            mix_l[i] = (int32_t)(rng() >> 17) - 16384, mix_r[i] = (int32_t)(rng() >> 17) - 16384;
        i0 = instr_now();
        for (b = 0; b < nb; b++)
            space_width(CTL);
        i0 = instr_now() - i0;
        sv.spc = sv.spc0 = 0;
        sw_a = 4096, sw_b = 0, sw_em = sw_es = 0;
        snprintf(what, sizeof what, "  SPACE's width stage: %.1f host instructions a sample (STEREO / LAYER only; about 12)", (double)i0 / (nb * CTL));
        expect(what, (double)i0 / (nb * CTL) < 14.0);
    }
    snprintf(what, sizeof what, "  all at once (REPEAT, FLANGER, PHASER, DRIVE, PUMP, MUTE 2, FILTER): +%.0f over idle (device ~%.1f %%)",
             all - cost_idle, (all - cost_idle) * 0.017);
    expect(what, (all - cost_idle) * 0.017 <= 12.0);
}

int main(void)
{
    t_entry();
    t_keys();
    t_knobs();
    t_multiload();
    t_leave_ui();
    t_black_scale();
    t_roll();
    t_mute();
    t_idle();
    t_leds();
    t_timing();
    t_stereo();
    t_too_long();
    t_clicks();
    t_harm();
    t_filter();
    t_filter24();
    t_mod();
    t_drive();
    t_pump();
    t_after();
    t_drive_song();
    t_pump_song();
    t_space();
    t_leave_audio();
    t_leave_space();
    t_cost();
    printf("%s (%d checks)\n", bad ? "PERFORM TEST FAILED" : "perform test passed", ncheck);
    return bad != 0;
}
