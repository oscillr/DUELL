/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.5 PERFORM mode: its page and its controls (the sound: perform.c; SPEC "PERFORM mode"). Included at the end of ui_layer.c.
 *   ON / OFF   FX held 0.7 s (btn_hold, SAVE's hold) turns PERFORM on, and off again; a white key pressed while FX is held turns it on
 *              at once and is already PERFORM's key (seq.c keyboard_block; FX's release then opens no page). A tap of FX in PERFORM goes
 *              back to the page it was shown over; HOME, SAVE and every other page button leave it for their own page (ui.c perf_off:
 *              page_entered, go_home). A tap of FX outside PERFORM opens the FX pages as ever. PERFORM starts OFF at power-on.
 *   PAGE       PERFORM (PERF_PAGE, FAM_FX: FX's LED lit): the header names it with the part badges (BOTH: both lit) and the REC mark;
 *              the cards K1 MORPH (the MORPH page's fader: morph_act MQ_FADER on the part(s) chosen), K2 SPACE, K3 FILTER (-100 .. 100,
 *              0 = neutral; back to 0 when PERFORM is left), K4 DEPTH (0 .. 100 %, kept until power-off); the panel the 4 x 4 map of
 *              the white keys, a row per group of four keys: a held key's cell in the accent, one that cannot run now greyed (its key
 *              pressed says why); the footer's first row what SELECT (BALANCE) and ALGORITHM (PART 1 / BOTH / PART 2) do here, its
 *              second row the engine and the patch name as on every page (PRESETS changes it; ui_draw.c draw_foot).
 *   PRESETS    the sound of the part(s) ALGORITHM chose, as on every page (BOTH: each part one step on from its own sound); perf_presets.
 *   ALGORITHM  PART 1 / BOTH / PART 2: the part(s) K1 MORPH and ROLL A / B act on. 0.2.9: the same state as the MORPH page's
 *              (ui_input.c algo_step; leaving PERFORM keeps it; other pages pick PART 1 / PART 2 only).
 *   SELECT     BALANCE (the MODE page's: mode_knob; page_select does nothing here: PERFORM is one page). MASTER as ever.
 *   LEDS       the white keys their map: an effect that can run at the mid level, a held key lit, one that cannot run dark; the black
 *              keys (notes) lit while held or playing. */
static const char *const PF_CELL[PF_KEYS][2] = {
    {"REPEAT", "1/8"}, {"REPEAT", "1/16"}, {"REPEAT", "1/32"}, {"REVERSE", 0},
    {"TAPE", "STOP"}, {"FREEZE", 0}, {"OCT UP", 0}, {"OCT DN", 0},
    {"FLANGER", 0}, {"PHASER", 0}, {"DRIVE", 0}, {"PUMP", 0},
    {"ROLL A", 0}, {"ROLL B", 0}, {"MUTE 1", 0}, {"MUTE 2", 0},
};

static uint8_t perf_nw_said;           /* "DUEL: NO WIDTH" shown since PERFORM opened (perf_knob: SPACE turned in DUEL) */
/* PUMP needs the beats of the tempo: with an external clock they come only while it runs (the arp's steps: seq.c events_block) */
static int perf_pump_ok(void) { return !song.g[G_CLOCK] || song.playing || (clock_state() == 1u && !midi_clock.stopped); }
/* effect e can run now (the main loop's view: the ISR's perf_avail, and PUMP's clock) */
static int perf_can(uint32_t e) { return ((perf_avail() >> e) & 1u) && (e != PF_PUMP || perf_pump_ok()); }
/* why e cannot run (its key pressed: the header says it) */
static void perf_say_why(uint32_t e)
{
    static const char *const MIN_BPM[PF_REV + 1u] = {"TEMPO UNDER 81 BPM", "TEMPO UNDER 41 BPM", "TEMPO UNDER 21 BPM", "TEMPO UNDER 81 BPM"};
    if (e <= PF_REV)
        ui_message(MIN_BPM[e]);
    else if (e == PF_PUMP)
        ui_message(clock_state() == 2u ? "PUMP: NO CLOCK" : "PUMP: CLOCK STOPPED");
}
/* the places (0 = F3) of the white keys PERFORM holds */
static uint32_t perf_places(void)
{
    uint32_t k, m = 0, h = kb_perf;
    for (k = 0; h && k < 27u; k++)
        if ((h >> k) & 1u) {
            m |= 1u << key_place(k);
            h &= ~(1u << k);
        }
    return m;
}

static void perf_enter(void)
{
    if (ui.perf)
        return;
    ui.perf_home = ui.home;
    ui.home = 0;
    ui.perf = 1;
    song.seq_mode = 0;                                  /* (STEP's entry is not PERFORM's) */
    ui.entry_open = 0;
    ui.act = 0;
    ui.hot_t = 0;
    ui.perf_seen = 0;
    perf_filt = 0;                                      /* (SPACE keeps its value: it is the MIXER's SPACE card too, 0.2.9) */
    perf_nw_said = 0;
    ui.force = 1;
}
/* a tap of FX (or FX held again): back to the page PERFORM was shown over */
static void perf_leave(void)
{
    uint8_t h = ui.perf_home;
    perf_off();
    if (h)
        go_home();
    else
        page_entered();
}

/* each pass of ui_input (after the layers have taken their buttons; lay: a layer's button held) */
static void perf_input(uint32_t pressed, uint32_t now, uint32_t lay)
{
    uint32_t fxbit = 1u << panel.btn[B_FX], fx, k, held, fresh;
    int ok = layer_allowed();
    (void)pressed;
    fx = btn_hold(&ui.fx_t0, B_FX, now, ok && !lay);
    if (perf_fx_hit) {                                  /* a white key with FX held: PERFORM at once, FX's press used up */
        perf_fx_hit = 0;
        if (ok)
            perf_enter();
        ui.fx_t0 |= 2u;
        ui.pg_down &= (uint16_t)~fxbit;
    }
    if (fx == BT_HOLD) {                                /* FX held 0.7 s: PERFORM on, or off again (no FX page on the release) */
        ui.pg_down &= (uint16_t)~fxbit;
        if (ui.perf)
            perf_leave();
        else
            perf_enter();
    } else if (fx == BT_TAP && ui.perf && ok) {         /* a tap in PERFORM: back */
        perf_leave();
    }
    perf_fxb = ok && !ui.perf && !lay && !(ui.fx_t0 & 2u) ? fxbit : 0u;   /* (the ISR: a key with FX down is PERFORM's) */
    perf_keys = (uint8_t)(ui.perf && ok);
    if (!ok)
        perf_kill = 1;                                  /* (the menu, a dialog, NAME: every effect off until its keys are let go) */
    else if (!kb_perf)
        perf_kill = 0;
    for (k = 0; k < 2u; k++)                            /* ROLL A / B: the MORPH page's roll, once a press */
        while (ui.perf_rolls[k] != perf_roll_n[k]) {
            ui.perf_rolls[k]++;
            if (ok)
                morph_act(MQ_ROLL, morph_mask(song.sel), k, song.sel);
        }
    held = perf_places();
    fresh = held & ~ui.perf_seen;
    ui.perf_seen = held;
    for (k = 0; fresh && k < PF_KEYS; k++)              /* a key that cannot run now: why */
        if (((fresh >> k) & 1u) && !perf_can(k))
            perf_say_why(k);
}

static void perf_signed(char *v, int32_t x);
/* SPACE's knob, PERFORM's K2 and the MIXER's third card (0.2.9): the one global value perf_space; role = the knob's EN_K* */
static void space_turn(uint32_t role, int32_t s)
{
    perf_space = (int8_t)clamp(perf_space + accel(role, s, 200), -100, 100);
    if (duell_mode == DM_SPLIT && !perf_nw_said) {      /* (0.2.9: SPACE's stereo width cannot work in DUEL, its sides are the two parts: */
        perf_nw_said = 1;                               /* said once since PERFORM opened, the rest of SPACE works) */
        ui_message("DUEL: NO WIDTH");
    }
}
/* SPACE's card in column col (PERFORM's K2, the MIXER's K3) */
static void space_card(uint32_t col)
{
    char val[12];
    perf_signed(val, perf_space);                       /* SPACE: 0 = the sound's own delay, reverb and chorus */
    draw_column(col, "SPACE", val, "", VAL(col), (perf_space + 100) * 5, ICON_REVERB);
}

/* KNOB 1..4 on the PERFORM page */
static void perf_knob(uint32_t slot, int32_t s)
{
    switch (slot) {
    case 0:                                             /* MORPH: the MORPH page's KNOB 1 */
        morph_act(MQ_FADER, morph_mask(song.sel), (uint32_t)clamp((int32_t)morph_fader_of(song.sel) + accel(EN_K1, s, 127), 0, 127),
                  song.sel);
        break;
    case 1:
        space_turn(EN_K2, s);
        break;
    case 2:
        perf_filt = (int8_t)clamp(perf_filt + accel(EN_K3, s, 200), -100, 100);
        break;
    default:
        perf_depth = (int8_t)clamp(perf_depth + accel(EN_K4, s, 100), 0, 100);
        break;
    }
}
/* PRESETS in PERFORM: as on every page, the selected part's sound (preset_step: the list, the name message). With ALGORITHM on BOTH each
 * part moves the same step from ITS OWN sound (the two stay different; the selected part's name is the one said last) */
static void perf_presets(int32_t s)
{
    uint32_t was = song.sel;
    if (morph.target != MT_BOTH || lock_held()) {
        presets_turn(s);
        return;
    }
    song.sel = (uint8_t)(was ^ 1u);
    preset_step(s);
    song.sel = (uint8_t)was;
    preset_step(s);
    sync_reload = 1;
    ui.force = 1;
}
/* -------------------------------------------------------------- the page --- */
static void perf_signed(char *v, int32_t x)              /* "+40", "-40", "0" */
{
    v[0] = x < 0 ? '-' : '+';
    fmt_int(v + (x != 0), x < 0 ? -x : x);
}
static void perf_cards(void)
{
    char val[12];
    uint32_t f = morph_fader_of(song.sel);
    int32_t x;
    if (f == 0u || f == 127u)                           /* (the MORPH page's card: the ends by their letters) */
        str_cpy(val, f ? "B" : "A", sizeof val);
    else
        fmt_int(val, (int32_t)f);
    draw_column(0, "MORPH", val, "", VAL(0u), (int32_t)f * 1000 / 127, ICON_MIX);
    space_card(1u);
    x = perf_filt;                                      /* FILTER: OFF in the middle, a low-pass left, a high-pass right */
    if (x)
        fmt_int(val, x < 0 ? -x : x);
    draw_column(2, "FILTER", x ? val : "OFF", x < 0 ? "LP" : x > 0 ? "HP" : "", VAL(2u), (x + 100) * 5, ICON_CUTOFF);
    fmt_int(val, perf_depth);
    draw_column(3, "DEPTH", val, "%", VAL(3u), perf_depth * 10, ICON_LEVEL);
}
/* the footer's first row in PERFORM: what SELECT and ALGORITHM do here (l, r hold 24); 0 = not PERFORM */
static int perf_foot(char *l, char *r)
{
    if (!ui.perf || cur_page()->scope != SC_PERF)
        return 0;
    str_cpy(l, "SELECT: BALANCE ", 24);
    perf_signed(l + str_len(l), mix_bal);
    str_cpy(r, "ALGO: ", 24);
    str_cpy(r + str_len(r), morph.target == MT_BOTH ? "BOTH" : song.sel ? "PART 2" : "PART 1", 24 - str_len(r));
    return 1;
}
static uint32_t perf_sig(void)
{
    uint32_t h = perf_places() * 2654435761u ^ perf_act * 40503u, k;
    for (k = 0; k < PF_KEYS; k++)
        h = (h ^ (uint32_t)perf_can(k)) * 16777619u;
    return h ^ ((uint32_t)(uint8_t)perf_space | (uint32_t)(uint8_t)perf_filt << 8 | (uint32_t)(uint8_t)perf_depth << 16) * 0x9E3779B1u;
}
/* the map: a cell per white key, four a row (the keyboard's groups), a held key in the accent, one that cannot run greyed */
#define PF_CY(r) (4 + 30 * (int32_t)(r))                /* rows at y 4 34 64 94, 26 px (to 120 of the panel's 122) */
#define PF_CH 26
static void graph_perf(void)
{
    uint32_t p, held = perf_places();
    for (p = 0; p < PF_KEYS; p++) {
        int32_t x = LC_X(p % 4u), y = PF_CY(p / 4u);
        uint32_t st = !perf_can(p) ? LS_DIM : !((held >> p) & 1u) ? LS_OFF
                    : (PF_Q >> p) & 1u && !((perf_act >> p) & 1u) ? LS_WAIT : LS_HELD;   /* (REPEAT, REVERSE: waiting for the 1/16) */
        uint16_t ink, fill = lc_fill(st, &ink);
        fill = lc_box(x, y, LC_W, PF_CH, fill);
        if (PF_CELL[p][1]) {                            /* two lines, their capitals 4 px apart, together on the cell's middle */
            int32_t t = y + HALF_UP(PF_CH - (2 * AF_S_CAP_H + 4)) - AF_S_CAP_Y;
            GFX_HOOK_ALIGN(0, y, 0, y + PF_CH, AL_V | AL_N(2), "perform cell lines centred up/down");
            GFX_HOOK_ALIGN(x, 0, x + LC_W, 0, AL_H | AL_PASS, "perform cell line centred across");
            cv_text_in(x, t, LC_W, &AF_S, PF_CELL[p][0], ink, fill);
            GFX_HOOK_ALIGN(x, 0, x + LC_W, 0, AL_H | AL_PASS, "perform cell line centred across");
            cv_text_in(x, t + AF_S_CAP_H + 4, LC_W, &AF_S, PF_CELL[p][1], ink, fill);
        } else {
            GFX_HOOK_ALIGN(x, y, x + LC_W, y + PF_CH, AL_HV, "perform cell name centred");
            cv_text_in(x, y + CAP_IN(S, PF_CH), LC_W, &AF_S, PF_CELL[p][0], ink, fill);
        }
    }
}
/* the keys' LEDs in PERFORM: returns the lit ones (the held PERFORM keys, the black keys held or playing); *mid the row markers
 * (0.2.11, owner: the 1st, 5th, 9th and 13th key of the 16, to count the rows by; the UI lights them steadily at the LEDS
 * mode's in-scale level; every other grid key stays dark unless held) */
static uint32_t perf_key_leds(uint32_t *mid)
{
    uint32_t k, lit = kb_perf | (((fm1_in.notes & ~kb_layer) | play_leds()) & ~PF_WHITE), m = 0;
    if (mid) {
        for (k = 0; k < 27u; k++)
            if (((PF_WHITE >> k) & 1u) && key_place(k) < PF_KEYS && !(key_place(k) & 3u))
                m |= 1u << k;
        *mid = m & ~lit;
    }
    return lit;
}
