/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca quick layers: while a page button is held, the keys and KNOB 1..4
 * are its shortcuts and its map shows over the page. One table (LAYERS) drives the gesture, the keys, the knobs,
 * the LEDs and the overlay:
 *   FX   (DUELL 0.2.2: not a layer any more. The FX hold layer and its performance effects are removed: FX is a plain page button, a
 *              short press opens the FX pages and holding it does nothing special. The LAYER_RES slot keeps the other layers' ids)
 *   GLO  (DUELL: not a layer; the LAYER_GLO slot only names GLO's MAIN PAGE, graph GR_GLO, which draw_layer draws. The keys play notes there as
 *              on every page; KNOB 1 MUTE 1 and 2 MUTE 2 (twist right mutes, left unmutes, glo_knob), KNOB 3, 4 LEVEL 1, 2; between the parts' columns the level
 *              meters (glo_meters). GLO held + PLAY: from the top without stopping)
 *   SCL  SET   GLOBAL (core.h; the header lights both parts): any key: its note name is ROOT; KNOB 1..3 SCALE ROOT OFF-SCALE (LY_SCL: the SCALE
 *              page's three; CHRD VOIC TRN are the selected part's and live on the CHORD page only: no mixing); the LEDs show the root
 *              lit and the scale's notes breathing
 *   EDIT SET   the first white key (F3) INIT (LY_INIT; the dialog). No engine keys: the engine is picked on the ENGINE page
 *              (EDIT's first) or by KNOB 1 here; KNOB 1 ENG, 2 No. (the engine's sounds), 3 FAV. Sound loads as on
 *              PRESETS: the steps stay, SAVE held undoes, the editor gets RELOAD; they apply while playing too.
 *              1.4.1: the black keys the selected track's engine's quick actions (EQA below: DRUM 1..8 its lanes'
 *              MUTE; other engines none yet, the keys dark)
 *   SEQ  SET   (1.2, on every page) TOOLS: the white keys from F3 the sequence tools
 *              (ui_tools.c: CLEAR, REVERSE, SHIFT < >, RANDOM, COOK; on a DRUM track BEAT and the lane's CLEAR REVERSE
 *              FILL RANDOM), black keys 1..8 the lane (DRUM); KNOB 1..4 LEN DIV SWING GATE (the PATTERN page's up to
 *              1.1.5: the page is gone, they are here). Each action one undo (SAVE held), OCT- the track as the layer
 *              opened (tl_open)
 *   REC  SET   (1.1.5, on every page; REC's tap still arms / disarms) the white keys from F3: CLEAR (the selected
 *              track's steps and automation, no dialog: one undo, SAVE held), CLICK (OFF REC ON), COUNT-IN (OFF 1 BAR
 *              2 BARS), CLICK LEVEL (LOW MID HIGH), each press the next value; KNOB 1..3 the same three. The settings
 *              are MENU > AUDIO's (ui.c rp_get / rp_put: one store), saved when the layer closes; OCT- puts back the
 *              settings and the track as the layer opened (tl_open). Replaces the old REC held on SEQ (Discussion #91:
 *              the CLEAR Tn SEQUENCE? dialog and its "HOLD: CLEAR" hint)
 * The gesture: let go before HOLD (the menu: 0.3 .. 0.6 s) with nothing else touched: a tap, the button's page.
 * Held past HOLD alone: the map (a peek), letting go does nothing. A key, a knob or a button meanwhile: a combo,
 * the map at once, no tap. Keys pressed with the button down are the layer's (seq.c keyboard_block): silent, no
 * MIDI, never recorded; keys held before stay notes. Only one layer at a time: a second layer button is ignored.
 * PLAY and REC work in every layer (GLO held + PLAY: RESTART; GLO's lock: PLAY as always, #183); SAVE, HOME, SEQ and
 * the other page buttons are swallowed. In SET layers OCT± do not shift the octave: OCT- (on release, not with OCT+)
 * puts back what the layer changed since it opened. A HOLD key's effect lasts until the key is let go, the map with
 * it. No layer in the menu, a dialog, NAME or the UPDATE MODE countdown. After a tap, until the layer has been opened
 * once: "HOLD [GLO] QUICK" (the seen bits are kept with the settings, favorites.c spare byte).
 * The lock (Discussion #83): a double tap (the second press within LY_DTAP_MS of the first tap, both let go before
 * HOLD, nothing else touched) opens the map and keeps it open with no button held (ui.lock): ly_down() counts the lock
 * as the button held, so the keys (seq.c keyboard_block: kb_lock), KNOB 1..4, OCT- act exactly as held (PLAY starts
 * and stops: GLO's RESTART needs GLO held, layer_play). A tap of its button closes it (the press hands it back to the
 * button: held on, a peek; let go, closed; never a tap), so do another page button, HOME, SAVE, SEQ (each then acts as
 * always), the menu, a dialog, NAME. The first tap is not deferred (a single tap opens its page at once, no lag): it
 * opens the page and remembers the page it left (lys.nv); the second tap locks the layer over that page, put back, so
 * a double tap leaves the page as it was. A first tap that acted instead of opening a page (EDIT on STEP: clear the
 * step; on USER / PROJECT: rename) arms no double tap: two quick taps there still act twice. */
#include "ui_tools.c"                                   /* SEQ TOOLS' actions */
enum { LK_HOLD, LK_SET };
typedef struct {
    uint8_t btn, kind, fam;            /* the button, HOLD / SET, the family whose first page KNOB 1..4 edit */
    const char *head;                  /* (FAM_HOME: the layer's own knobs) the header over the map */
    const char *name, *tag;            /* the footer's page name and number place: "GLO" "SET" */
} layer_t;
static const layer_t LAYERS[LAYER_N] = {
    {0, 0, 0, "", "", ""},
    {0, 0, 0, "", "", ""},                                  /* (LAYER_RES: the FX hold layer, removed in 0.2.2) */
    {B_GLO, LK_SET, FAM_HOME, "[GLO] SET", "GLO", "SET"},
    {B_SCL, LK_SET, FAM_SCL, "[SCL] SET", "SCL", "SET"},
    {B_EDIT, LK_SET, FAM_HOME, "[EDIT] SET", "EDIT", "SET"},
    {B_SEQ, LK_SET, FAM_SEQ, "[SEQ] TOOLS", "SEQ", "TOOLS"},
    {B_REC, LK_SET, FAM_HOME, "[REC] SET", "REC", "SET"},
};
static const uint8_t LY_KC[LAYER_N] = {0, 0, KC_GLO, KC_SCL, KC_EDIT, KC_SEQ, KC_REC};
/* SCL's knobs: the SCALE page's, global (cur_page() while the layer edits or draws them: page_over) */
static const page_t LY_SCL = {"SCALE", FAM_SCL, SC_SCALE, GR_SCALE, {0xFF, 0xFF, 0xFF, 0xFF}};
/* SEQ TOOLS' knobs: LEN DIV SWING GATE (the length the tools work in; up to 1.1.5 the PATTERN page's) */
static const page_t LY_SEQ = {"PATTERN", FAM_SEQ, SC_TRACK, GR_STEPS, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}};
static const page_t *ly_page(uint32_t l) { return l == LAYER_SCL ? &LY_SCL : l == LAYER_SEQ ? &LY_SEQ : 0; }
#define LY_OPEN 2u                     /* ui.ly_t0: the map opened (no tap any more) */
#define LY_COMBO 4u                    /* .. by a combo */
#define LY_DEAD 8u                     /* .. pressed where there is no layer: does nothing */
#define LY_INIT 0u                     /* EDIT: the white key of INIT, the first (F3), its map cell (no engine keys any more) */
#define layer_seen (favorites.factory[15][31])   /* bit l: layer l opened once (a byte no engine uses) */
/* (favorites.factory[15][14..23], 10 bytes: the FX layer's key map up to 0.2.1, retired in 0.2.2: not read or written any more, never reused) */

static struct {
    uint8_t l, trk, loaded, oct;       /* SET: the layer and the track of the snapshot; EDIT: a sound loaded since
                                        * it opened (SEQ: a tool used); OCT- / OCT+ pressed in a SET layer (bits) */
    uint8_t cook;                      /* SEQ: COOK pressed since it opened */
    uint8_t rp_dirty;                  /* REC: a setting changed (settings_save when the layer lets go) */
    uint8_t mute[NPART];               /* EDIT: the DRUM lanes' mutes when it opened (eng_drum.c drum_mute; OCT-) */
    uint32_t eqh;                      /* EDIT: the keys (bits 0..26) of the HOLD quick actions held */
    int16_t v[9];                      /* the values when it opened: SCL: SCALE ROOT OFF-SCALE (global);
                                        * SEQ LEN DIV SWING GATE; REC the settings (ui_rec_prefs) */
    uint8_t quiet;                     /* #39: a layer closed after it opened: KNOB 1..4 do nothing until quiet_t + */
    uint32_t quiet_t;                  /* LY_QUIET_MS (fm1_ms) */
    uint8_t dt_l, dt_hint, dtap;       /* #83: the layer of the last tap that opened a page (0 none), its hint said; the
                                        * press now armed is that tap's second (a double tap) */
    uint32_t dt_ms;                    /* .. when that tap was let go (fm1_ms) */
    struct { uint8_t home, page, act, seq, fam[FAM_COUNT], rec, go; } nv;   /* .. the page it left (put back by the
                                        * lock); REC: the arming before its tap, the transport that tap started */
} lys;
#define LY_QUIET_MS 250u
#define LY_DTAP_MS 300u                /* #83: the second press of a double tap at most this long after the first tap */

static int layer_allowed(void) { return !ui.menu && !ui.confirm && !ui.uboot && !name_on(); }
static uint32_t ly_bit(uint32_t l) { return 1u << panel.btn[LAYERS[l].btn]; }
static uint32_t ly_down(uint32_t l) { return l && ((fm1_in.buttons & ly_bit(l)) != 0u || ui.lock == l); }   /* (locked: held) */
static uint32_t layer_bits(void)
{
    uint32_t l, m = 0;
    for (l = LAYER_GLO; l < LAYER_N; l++)
        if (l != LAYER_GLO)                             /* (DUELL step 6: GLO is a page, not a held layer; 0.2.2: FX neither) */
            m |= ly_bit(l);                             /* (1.2: every layer on every page, SEQ's too) */
    return m;
}
static uint32_t layer_btn(void) { return LAYERS[ui.layer % LAYER_N].btn; }
static const char *layer_head(void) { return LAYERS[ui.layer % LAYER_N].head; }
/* the footer's page place for the layer shown ("GLO" | "SET") */
static void layer_foot_title(char *nm, char *ct)
{
    const layer_t *l = &LAYERS[ui.layer % LAYER_N];
    str_cpy(nm, l->name, 12);
    str_cpy(ct, l->tag, 8);
}
static uint32_t layer_open(void) { return ui.ly && (ui.ly_t0 & LY_OPEN) && ly_down(ui.ly) && layer_allowed() ? ui.ly : 0u; }
static int layer_set_open(void)
{
    return layer_open() && LAYERS[layer_open()].kind == LK_SET;
}
static uint32_t layer_held(void) { return ui.ly && ly_down(ui.ly) && layer_allowed(); }
/* GLO's main page shown (no menu, dialog or NAME over it): drawn by draw_layer, GLO held + PLAY restarts (layer_play). The keys and the knobs
 * are the page's own, as on every page (glo_knob) */
static int glo_pg(void) { return !ui.home && layer_allowed() && cur_page()->graph == GR_GLO; }

/* each pass, before the keys: what the ISR (seq.c keyboard_block) gives to a layer. Armed: its button; none
 * armed: any layer button not already down (one pressed meanwhile is armed below; one held over from before
 * is ignored, its keys stay notes); armed but dead (pressed under a dialog, a menu or NAME that has closed
 * since): nothing, its keys stay notes */
static void layer_masks(void)
{
    uint32_t m = !layer_allowed() || (ui.ly && (ui.ly_t0 & LY_DEAD)) ? 0u
               : ui.ly ? ly_bit(ui.ly) : layer_bits() & ~fm1_in.buttons;
    kb_mask = m;
    kb_lock = (uint8_t)(m && ui.lock ? 1u : 0u);        /* (no button held: the ISR's) */
}

/* a layer button pressed (its edge) while none is armed: it is now, a dead one where no layer opens */
static void layer_arm(uint32_t pressed, uint32_t now)
{
    uint32_t l;
    if (ui.ly)
        return;
    for (l = LAYER_GLO; l < LAYER_N; l++)
        if (l != LAYER_GLO && (pressed & ly_bit(l))) {
            ui.ly = (uint8_t)l;
            ui.ly_t0 = (now & ~15u) | 1u | (layer_allowed() ? 0u : LY_DEAD);
            lys.dtap = lys.dt_l == l && fm1_ms - lys.dt_ms <= LY_DTAP_MS;   /* (#83: the second tap of a double tap?) */
            lys.dt_l = 0;
            return;
        }
}

/* the armed layer lets go (its button up, or its lock closed); one that
 * opened, or a combo: KNOB 1..4 quiet for a while (#39: the knob still turning is not the page's) */
static void layer_let_go(uint32_t quiet)
{
    if (lys.rp_dirty) {                                 /* REC: its settings kept, as MENU does when it closes */
        lys.rp_dirty = 0;
        settings_save();                                /* (deferred while playing) */
    }
    if (quiet) {
        lys.quiet = 1;
        lys.quiet_t = fm1_ms;
    }
    ui.ly_t0 = 0;
    ui.ly = 0;
    ui.lock = 0;
}

/* #83, each pass before the layer's buttons are read: a locked layer closes. Its own button pressed: the lock goes,
 * the button held keeps it open (a peek: let go, it closes, no tap). Another button but PLAY, REC and OCT- / OCT+
 * (another page button, another layer's, HOME, SAVE, SEQ): closed now, and that button acts as always. Any other
 * button pressed between the taps of a double tap: no double tap */
static void layer_lock_input(uint32_t pressed)
{
    uint32_t keep = 1u << panel.btn[B_PLAY] | 1u << panel.btn[B_REC] | 1u << panel.btn[B_OCTDN] | 1u << panel.btn[B_OCTUP];
    if (lys.dt_l && (pressed & ~ly_bit(lys.dt_l)))
        lys.dt_l = 0;
    if (!ui.lock)
        return;
    if (pressed & ly_bit(ui.lock))
        ui.lock = 0;
    else if (pressed & ~keep)
        layer_let_go(1);
}
static int layer_locked(void) { return ui.lock && ui.layer == ui.lock; }   /* (the map's header: the lock's mark) */

static void layer_opened(uint32_t l);
/* #83: the second tap of a double tap: the page the first one left comes back, and the layer locks open over it */
static void layer_lock(uint32_t l, uint32_t now)
{
    ui.home = lys.nv.home;
    ui.page = lys.nv.page;
    memcpy(ui.fam_last, lys.nv.fam, sizeof ui.fam_last);
    ui.act = lys.nv.act;
    song.seq_mode = lys.nv.seq;
    ui.entry_open = 0;
    if (lys.dt_hint || l == LAYER_REC)
        ui.msg_t = 0;                                   /* (the first tap's "HOLD [..] QUICK", REC's) */
    if (l == LAYER_REC) {                               /* REC: the arming as before the first tap, and the */
        song.rec = lys.nv.rec;                          /* transport it started stopped again */
        if (lys.nv.go && transport_req == 1u)
            transport_req = 0;
        else if (lys.nv.go && (song.playing || seq_counting()))
            transport_req = 2;
        ui.force = 1;
    }
    ui.lock = (uint8_t)l;
    ui.ly_t0 = (now & ~15u) | 1u | LY_OPEN;
    ui.force = 1;
    layer_opened(l);
}

/* the layer opened: once seen, no more hint; SET: what OCT- will put back */
static void layer_opened(uint32_t l)
{
    uint32_t k;
    if (!((layer_seen >> l) & 1u)) {
        layer_seen |= (uint8_t)(1u << l);
        settings_save();                                /* (deferred while playing) */
    }
    lys.l = (uint8_t)l;
    lys.trk = song.sel;
    lys.loaded = 0;
    lys.cook = 0;
    if (l == LAYER_SEQ || l == LAYER_REC) {             /* the track as it is now (OCT-) */
        memcpy(tl_open.step, TSEL->step, sizeof tl_open.step);
        motion_snapshot_track(TSEL, &tl_open.mo);
    }
    for (k = 0; k < 4u; k++)
        if (l == LAYER_SEQ) {
            lys.v[k] = TSEL->p[P_SLEN + k];
        } else {
            lys.v[k] = k == 0u ? scale_g : k == 1u ? root_g : k == 2u ? offscale_g : 0;   /* (the global three, core.h) */
        }
    lys.v[8] = song.g[G_BPM];
    memcpy(lys.mute, drum_mute, sizeof lys.mute);
    if (l == LAYER_REC)
        lys.v[0] = ui_rec_prefs;
}

/* the armed button: 0, or the layer whose button was tapped (let go) */
static uint32_t layer_gesture(uint32_t now, uint32_t combo)
{
    uint32_t *t0 = &ui.ly_t0, l = ui.ly;
    if (!l)
        return 0;
    if (!layer_allowed()) {
        *t0 |= LY_DEAD;
        ui.lock = 0;                                    /* (the menu, a dialog, NAME: the lock closes) */
    }
    if (ly_down(l)) {
        if (!(*t0 & (LY_OPEN | LY_DEAD)) &&
            (combo || now - (*t0 & ~15u) >= (uint32_t)HOLD_MS[settings_hold % 4u] * 1000u * FM1_TICKS_PER_US)) {
            *t0 |= LY_OPEN | (combo ? LY_COMBO : 0u);
            layer_opened(l);
        }
        return 0;
    }
    l = *t0 & (LY_OPEN | LY_DEAD) || combo ? 0u : l;   /* (a knob turned as it was let go: a combo, no tap) */
    if (l && lys.dtap) {                                /* #83: a double tap: locked open (no tap) */
        lys.dtap = 0;
        layer_lock(l, now);
        return 0;
    }
    layer_let_go(*t0 & LY_OPEN || combo);
    return l;
}

/* #39: KNOB 1..4 belong to no page while a layer lets go: the frame its button is let go (the turns read then
 * were made with it held), while its map still shows (its keys held after it), and LY_QUIET_MS after it closed.
 * Their turns are dropped there (ui_input); before, they edited the page under the layer (an ARP page: ARP on) */
static int layer_knobs_quiet(void)
{
    if (lys.quiet && fm1_ms - lys.quiet_t >= LY_QUIET_MS)
        lys.quiet = 0;
    return layer_allowed() && (ui.ly || ui.layer || lys.quiet);
}

/* the map shows while the button is held open, and after it while its keys are still held */
static void layer_show(void)
{
    uint32_t show = layer_allowed() ? (layer_open() ? layer_open() : kb_layer ? ui.layer : 0u) : 0u;
    if (show != ui.layer) {
        ui.layer = (uint8_t)show;
        ui.force = 1;
    }
}

/* a tap: the button's page; the hint until its layer has been opened once (unless the tap said something, or
 * opened NAME: EDIT on USER / PROJECT renames; never over a dialog or the menu) */
static void layer_tap(uint32_t l)
{
    uint8_t m = ui.msg_t;
    int acted;
    lys.nv.home = ui.home;                              /* (#83: the page this tap leaves, for a double tap) */
    lys.nv.page = ui.page;
    lys.nv.act = ui.act;
    lys.nv.seq = song.seq_mode;
    memcpy(lys.nv.fam, ui.fam_last, sizeof lys.nv.fam);
    lys.nv.rec = song.rec;
    if (l == LAYER_REC) {                               /* REC: arms / disarms (no page); said nothing: no double tap */
        uint32_t idle = !song.playing && !seq_counting() && transport_req != 1u;
        acted = !rec_tap();
        lys.nv.go = (uint8_t)(idle && transport_req == 1u);
    } else {
        acted = page_tap(LAYERS[l].btn);
    }
    lys.dt_hint = 0;
    if (!((layer_seen >> l) & 1u) && ui.msg_t == m && !name_on() && !ui.confirm && !ui.menu) {
        ui_say("HOLD [", KC[LY_KC[l]].label);
        str_cpy(ui.msg + str_len(ui.msg), "] QUICK", sizeof ui.msg - str_len(ui.msg));
        ui.msg_t = 90;                                  /* ~1.5 s */
        lys.dt_hint = 1;
    }
    if (!acted && !name_on() && !ui.confirm && !ui.menu) {   /* a page opened: a second tap soon locks the layer */
        lys.dt_l = (uint8_t)l;
        lys.dt_ms = fm1_ms;
    }
}

/* EDIT: a sound load in the layer. The first one takes the undo copy as the track is now (when the layer
 * opened, unless KNOB 2 / 3 had loaded already), so OCT- and SAVE held go back to it */
static uint32_t snd_id(void) { return TSEL->eng_req | (uint32_t)TSEL->preset << 8 | (uint32_t)TSEL->user << 16; }
static void edit_load(uint32_t e, int32_t step)
{
    uint32_t id = snd_id();
    if (!lys.loaded)
        undo.keep = 0;
    if (e < NENGINES) {
        if (e == TSEL->eng_req % NENGINES)
            return;                                     /* its engine already: the sound stays */
        select_engine(e);
        preset_hinted();
    } else {
        eng_list_step(step);
    }
    lys.loaded |= snd_id() != id;
}

/* SEQ TOOLS: white key place p's action on the selected track (ui_tools.c), "STOP TO EDIT" while a song plays */
static void tools_key(uint32_t p)
{
    const char *m;
    char n[4];
    if (!tl_cell(TSEL, p))
        return;
    if (!(m = tl_do(TSEL, p, ui.lane)))
        return;
    if (p == TL_COOK && lys.cook < 99u) {               /* "COOKED 3": the third since the layer opened */
        lys.cook++;
        fmt_int(n, lys.cook);
        ui_say("COOKED ", n);
    } else {
        ui_message(m);
    }
    lys.loaded = 1;
    ui.force = 1;
}

/* REC: CLEAR, the selected track's steps and automation (and ARP's latched chord): one undo (SAVE held), no dialog,
 * as SEQ TOOLS' CLEAR (up to 1.1.4 REC held on SEQ asked first, CF_CLEAR_SEQ). Its own undo copy (not a sound load's) */
static void rec_clear(void)
{
    track_t *t = TSEL;
    if (seq_is_empty(t) && !motion_count(t)) {
        ui_message("NOTHING TO CLEAR");
        return;
    }
    undo.keep = 0;
    load_begin(t, UNDO_PAT);
    track_defaults_steps(t);
    load_end(t);
    t->nheld = 0;
    t->arp_phys = 0;
    ui_message("SEQUENCE CLEARED");
    lys.loaded = 1;
    ui.force = 1;
}
/* REC's settings, row r (G3 A3 B3, KNOB 1..3): CLICK, COUNT-IN, CLICK LEVEL as MENU > AUDIO steps them (ui.c rp_*) */
static const uint8_t RL_F[3] = {RP_CLICK, RP_COUNTIN, RP_LEVEL};
static const char *const RL_LABEL[3] = {"CLICK", "COUNT", "LEVEL"};   /* (COUNT-IN: too wide for a card or a key cell) */
static const uint8_t RL_ICON[3] = {ICON_TEMPO, ICON_TIME, ICON_LEVEL};
static const char *rl_name(uint32_t r, uint32_t v)
{
    return (r == 0u ? CLICK_N : r == 1u ? COUNTIN_N : CLKLVL_N)[v % 3u];
}
static void rec_set(uint32_t r, uint32_t v)
{
    if (v == rp_get(RL_F[r]))
        return;
    rp_put(RL_F[r], v);
    lys.rp_dirty = 1;
}
static void rec_key(uint32_t p)
{
    uint32_t r = p - 1u, v;
    if (p == 0u) {
        rec_clear();
        return;
    }
    v = (rp_get(RL_F[r]) + 1u) % 3u;                    /* the next value, round */
    rec_set(r, v);
    ui_say(r == 2u ? "CLICK LEVEL " : r ? "COUNT-IN " : "CLICK ", rl_name(r, v));
}

static const char W_NOTE[16] = {'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G'};

/* 1.4.1 ENGINE QUICK ACTIONS: the EDIT layer's black keys (1..11: F#3 .. F#5) do what the selected track's engine
 * offers there: EQA, per engine up to 11, black key n the n-th; past them (and on an engine with none) a key is dark
 * and does nothing. An action: its cell's name, HOLD (on while its key is held: act down, then up; a corner triangle,
 * breathes) or SET (act on each press), its handler and its LED (on: lit). They are performance state: no sound load,
 * no undo copy, not recorded; OCT- puts back what they changed since the layer opened (eqa_put_back). A new engine's
 * actions: a table and its row in EQA, and what OCT- puts back in eqa_put_back.
 *   DRUM  1..8 MUTE of the lanes KICK .. BELL (eng_drum.c drum_mute; the cells the kit's names), latched: lit =
 *         sounding, dark = muted (as GLO's track mutes); the cell of a muted lane as a muted track's (KEY) */
enum { EQ_SHOW_SEL, EQ_SHOW_MUTE };                /* the cell: on = the selection's fill; on = idle, off = KEY */
typedef struct {
    uint8_t kind, show, arg;                       /* LK_HOLD / LK_SET, EQ_SHOW_*, the handler's argument */
    const char *(*name)(const track_t *t, uint32_t arg);   /* the cell's (2 .. 3 characters) */
    void (*act)(track_t *t, uint32_t arg, uint32_t down);  /* SET: pressed (down 1); HOLD: pressed, let go (0) */
    uint32_t (*on)(const track_t *t, uint32_t arg);        /* the LED lit, the cell's state */
} eqa_t;
typedef struct { uint8_t eng, n; const eqa_t *a; } eqa_set_t;

static void eqa_drum_mute(track_t *t, uint32_t l, uint32_t down)
{
    uint32_t k = trk_index(t) % NPART;
    if (!down)
        return;
    drum_mute[k] ^= (uint8_t)(1u << l);
    ui_say(drum_lane_name(t, l), (drum_mute[k] >> l) & 1u ? " MUTED" : " ON");
}
static uint32_t eqa_drum_on(const track_t *t, uint32_t l) { return !((drum_mute[trk_index(t) % NPART] >> l) & 1u); }
#define EQA_LANE(l) {LK_SET, EQ_SHOW_MUTE, l, drum_lane_abbr, eqa_drum_mute, eqa_drum_on}
static const eqa_t EQA_DRUM[NLANE] = {EQA_LANE(0), EQA_LANE(1), EQA_LANE(2), EQA_LANE(3), EQA_LANE(4), EQA_LANE(5),
    EQA_LANE(6), EQA_LANE(7)};
static const eqa_set_t EQA[] = {
    {ENGI_DRUM, NLANE, EQA_DRUM},
};
typedef char eqa_fits[NLANE <= 11 ? 1 : -1];       /* (11 black keys) */

static uint32_t eqa_count(const track_t *t)        /* the actions of t's engine (black keys 1..n) */
{
    uint32_t i, e = t->eng_req % NENGINES;
    for (i = 0; i < NELEM(EQA); i++)
        if (EQA[i].eng == e)
            return EQA[i].n;
    return 0;
}
static const eqa_t *eqa_of(const track_t *t, uint32_t p)   /* black key p's (0 = F#3), 0 none */
{
    uint32_t i, e = t->eng_req % NENGINES;
    for (i = 0; i < NELEM(EQA); i++)
        if (EQA[i].eng == e)
            return p < EQA[i].n ? &EQA[i].a[p] : 0;
    return 0;
}
/* black key k (its place p) pressed in the EDIT layer */
static void eqa_key(uint32_t k, uint32_t p)
{
    const eqa_t *a = eqa_of(TSEL, p);
    if (!a)
        return;
    if (a->kind == LK_HOLD)
        lys.eqh |= 1u << k;
    a->act(TSEL, a->arg, 1);
    ui.force = 1;
}
/* each pass: the HOLD actions whose keys were let go */
static void eqa_release(void)
{
    uint32_t r = lys.eqh & ~kb_layer, k;
    const eqa_t *a;
    lys.eqh &= ~r;
    for (k = 0; r && k < 27u; k++)
        if (((r >> k) & 1u) && (a = eqa_of(TSEL, key_place(k))) != 0 && a->kind == LK_HOLD) {
            a->act(TSEL, a->arg, 0);
            ui.force = 1;
        }
}
/* OCT- in the EDIT layer: the actions' state as the layer opened (1 = something changed) */
static int eqa_put_back(void)
{
    int d = memcmp(drum_mute, lys.mute, sizeof drum_mute) != 0;
    memcpy(drum_mute, lys.mute, sizeof drum_mute);
    return d;
}
/* the map's signature of the actions: their LEDs and names */
static uint32_t eqa_sig(void)
{
    uint32_t i, n = eqa_count(TSEL), h = n;
    for (i = 0; i < n; i++) {
        const eqa_t *a = eqa_of(TSEL, i);
        h = h * 33u + a->on(TSEL, a->arg) + (uint32_t)(uintptr_t)a->name(TSEL, a->arg) * 7u;
    }
    return h;
}

/* a key pressed in layer l (k: 0 = F3 .. 26 = G5) */
static void layer_key(uint32_t l, uint32_t k)
{
    uint32_t p = key_place(k);
    if (l == LAYER_SCL) {
        root_g = (int16_t)((k + 5u) % 12u);             /* the key's note name (F3 = F): the global ROOT */
    } else if (l == LAYER_EDIT && !key_black(k)) {
        if (p == LY_INIT)
            confirm_open(CF_INIT_SOUND, song.sel);      /* (the dialog closes the layer) */
    } else if (l == LAYER_EDIT) {
        eqa_key(k, p);                                  /* (black keys: the engine's quick actions) */
    } else if (l == LAYER_REC) {
        if (!key_black(k) && p < 4u)
            rec_key(p);
    } else if (l == LAYER_SEQ) {
        if (!key_black(k))
            tools_key(p);
        else if (p < NLANE && drum_track(TSEL))
            ui.lane = (uint8_t)p;                       /* (black keys 1..8: the lane, as on the grid) */
    }
}
/* each pass: the keys the layer got now */
static void layer_keys(uint32_t keys)
{
    uint32_t k, l = layer_open() ? layer_open() : ui.layer ? ui.layer : 0u;
    for (k = 0; keys && k < 27u; k++)
        if ((keys >> k) & 1u)
            layer_key(l, k);
    if (lys.eqh)
        eqa_release();
}

/* KNOB 1..4 in the open layer: its own (EDIT's sound, REC's settings), else the four of its page */
static void layer_knob(uint32_t k, int32_t s)
{
    uint32_t l = layer_open();
    if (l == LAYER_EDIT) {                       /* ENG, No., FAV */
        if (k == 0u)
            edit_load(eng_step(TSEL->eng_req, s), 0);
        else if (k == 1u)
            edit_load(NENGINES, s);
        else if (k == 2u)
            preset_mark(s > 0);
    } else if (l == LAYER_REC) {                        /* CLICK, COUNT-IN, LEVEL (KNOB 4: none) */
        if (k < 3u)
            rec_set(k, (uint32_t)clamp((int32_t)rp_get(RL_F[k]) + s, 0, 2));
    } else if (l) {                                     /* the page's knobs, wherever the page is */
        uint8_t h = ui.home, pg = ui.page;
        ui.home = 0;
        ui.page = (uint8_t)page_first(LAYERS[l].fam);
        page_over = ly_page(l);
        edit_param(k, s);
        page_over = 0;
        ui.home = h;
        ui.page = pg;
    }
}

/* GLO's page, KNOB 1 MUTE 1 and 2 MUTE 2 (the MORPH page's twist gesture with ONE detent: instant, once per twist): each knob is its own part. A click either way flips it: muted <-> sounding (P_MUTE, latched). KNOB 3, 4 LEVEL 1, 2 (recorded as on MIXER; MIDI LEARN picks all four) */
static void glo_knob(uint32_t k, int32_t s)
{
    if (k < 2u) {
        int32_t d = gesture_det(2u + k, s, 1);   /* (one detent: a mute is instant) */
        int16_t m;
        if (!d)
            return;
        m = (int16_t)!trk[k].p[P_MUTE];                 /* either way: the state flips */
        ml_knob(&trk[k], P_MUTE);                       /* (MIDI LEARN: MUTE 1 / MUTE 2 picked) */
        trk[k].p[P_MUTE] = m;
        ui_say(k ? "PART 2 " : "PART 1 ", m ? "MUTED" : "UNMUTED");
    } else {
        int16_t *vp = &trk[k - 2u].p[P_LEVEL];
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, TP[P_LEVEL].max - TP[P_LEVEL].min), TP[P_LEVEL].min, TP[P_LEVEL].max);
        motion_capture(&trk[k - 2u], P_LEVEL, *vp);
        ml_knob(&trk[k - 2u], P_LEVEL);                 /* (MIDI LEARN: Tk LEVEL picked) */
    }
}

/* PLAY with GLO held open: RESTART (from the top, playing on); stopped: PLAY. 1 = done here. GLO's lock is not
 * held: PLAY starts and stops there as in every layer (#183: locked, PLAY restarted and never stopped) */
static int layer_play(void)
{
    if (!glo_pg() || ui.ly || !(fm1_in.buttons & ly_bit(LAYER_GLO)))
        return 0;
    ui.pg_down &= (uint16_t)~ly_bit(LAYER_GLO);         /* (GLO's release after the restart is no tap: no next page) */
    if (song.g[G_CLOCK] && song.playing) {
        ui_message("RESTART: CLK IS EXT");
        return 1;
    }
    transport_req = song.playing ? 3u : 1u;
    return 1;
}

/* OCT- / OCT+ pressed with a SET layer's button down before it opened: a combo, so it opens now,
 * before oct_taps and layer_oct read it (else the press was nobody's: no put back / ALL OFF, no octave) */
static void layer_oct_open(uint32_t pressed)
{
    uint32_t l = ui.ly, oct = 1u << panel.btn[B_OCTDN] | 1u << panel.btn[B_OCTUP];
    if ((pressed & oct) && l && !(ui.ly_t0 & (LY_OPEN | LY_DEAD)) && layer_held() &&
        LAYERS[l].kind == LK_SET) {
        ui.ly_t0 |= LY_OPEN | LY_COMBO;
        layer_opened(l);
    }
}

/* OCT- / OCT+ in a SET layer: no octave; OCT- let go puts back what the layer changed. Returns the taps left */
static uint32_t layer_oct(uint32_t pressed, uint32_t oct)
{
    uint32_t dn = panel.btn[B_OCTDN], up = panel.btn[B_OCTUP], k, mine;
    if (layer_set_open())
        lys.oct |= (uint8_t)(((pressed >> dn) & 1u) | ((pressed >> up) & 1u) << 1);
    mine = oct & lys.oct;
    if (mine & 1u) {                                    /* OCT-: back to when it opened */
        if (lys.l == LAYER_EDIT) {
            uint32_t ld = lys.loaded;
            if (ld)
                undo_swap();                            /* (the copy from its first load) */
            lys.loaded = 0;
            if (eqa_put_back() && !ld)                  /* (the quick actions: the lanes' mutes) */
                ui_message("MUTES PUT BACK");
        } else if (lys.l == LAYER_SEQ || lys.l == LAYER_REC) {
            track_t *t = &trk[lys.trk % NTRK];
            {
                if (lys.loaded) {                       /* (SAVE held then: the tools / the clear again) */
                    tl_undo_take(t);
                    memcpy(t->step, tl_open.step, sizeof t->step);
                    if (motion_replace_track(t, &tl_open.mo))
                        motion_full = 1;
                }
                if (lys.l == LAYER_REC) {               /* REC: the settings as it opened */
                    lys.rp_dirty |= (uint8_t)(ui_rec_prefs != (uint8_t)lys.v[0]);
                    ui_rec_prefs = (uint8_t)lys.v[0];
                    rp_apply();
                } else {
                    for (k = 0; k < 4u; k++)
                        t->p[P_SLEN + k] = lys.v[k];
                }
                lys.loaded = 0;
                lys.cook = 0;
                ui_message(lys.l == LAYER_REC ? "REC PUT BACK" : "SEQUENCE PUT BACK");
            }
        } else {
            scale_g = lys.v[0];
            root_g = lys.v[1];
            offscale_g = (uint8_t)lys.v[2];
            ui_message("SCALE PUT BACK");
        }
    }
    lys.oct &= (uint8_t)(((fm1_in.buttons >> dn) & 1u) | ((fm1_in.buttons >> up) & 1u) << 1);
    return oct & ~mine;
}

/* --------------------------------------------------------- the LEDs --- */
/* lit = in effect now (held, the value, a track sounding), breathing = can be pressed (*br: dark .. ~60 % and
 * back, ~1.1 s, hal/fm1_input.h fm1_led_breath; was a hard 250 ms blink, #119), dark = nothing there */
static int glo_sounding(uint32_t t) { return !trk[t].p[P_MUTE]; }
static int rec_clearable(void) { return !seq_is_empty(TSEL) || motion_count(TSEL) != 0u; }
static uint32_t layer_leds(uint32_t *br)
{
    uint32_t k, m = 0, n = 0, l = ui.layer;
    uint32_t mask = scale_mask(TSEL), root = (uint32_t)root_of(TSEL) % 12u;
    const eqa_t *a;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), b = (uint32_t)key_black(k), on = 0, can = 0, e;
        if (l == LAYER_SCL) {                    /* the root lit, the scale's notes breathe */
            e = (k + 5u + 12u - root) % 12u;
            on = e == 0u;
            can = (mask >> e) & 1u;
        } else if (l == LAYER_EDIT && !b) {             /* INIT breathes (no engine keys) */
            can = p == LY_INIT;
        } else if (l == LAYER_EDIT && (a = eqa_of(TSEL, p)) != 0) {   /* the quick actions: on lit (DRUM: sounding), */
            on = a->on(TSEL, a->arg) != 0u;             /* a HOLD one off breathes, none dark */
            can = a->kind == LK_HOLD;
        } else if (l == LAYER_SEQ) {                    /* the tools breathe (not while a song plays); DRUM: the lane */
            on = b && p < NLANE && drum_track(TSEL) && p == ui.lane;   /* lit, the other lanes breathe */
            can = b ? p < NLANE && drum_track(TSEL) : tl_cell(TSEL, p);
        } else if (l == LAYER_REC && !b && p < 4u) {    /* a setting on (LEVEL: the click on) lit, the rest breathe; */
            on = p == 1u ? rp_get(RP_CLICK) != 0u : p == 2u ? rp_get(RP_COUNTIN) != 0u :   /* CLEAR: something to */
                 p == 3u && rp_get(RP_CLICK) != 0u;    /* clear, not while a song plays */
            can = p ? 1u : rec_clearable();
        }
        m |= on << k;
        n |= (can & !on) << k;
    }
    if (br)
        *br = n;
    return m;
}

/* ------------------------------------------------------ the overlay --- */
/* One template: the header names the button and the kind ("[GLO] SET"); the cards are KNOB 1..4; the panel the
 * map of the keys as cells (the key's note name, a Fukiai icon, a name); the footer the keycaps. A cell: RAISE
 * (can be pressed), the selection's fill (the value now, SET), the accent (held, HOLD), DIM (cannot now: pressing
 * it says why), KEY (a muted track, as the MUTE badge); HOLD cells in a SET layer: a corner triangle */
#define LC_X(c) (6 + 58 * (int32_t)(c))                 /* cell column c: 54 px wide, 4 px apart */
#define LC_W 54
#define LC_H 42                                          /* the big cells: rows at y 4 and 50 */
enum { LS_OFF, LS_SEL, LS_HELD, LS_WAIT, LS_DIM, LS_MUTE };

static uint16_t lc_fill(uint32_t st, uint16_t *ink)
{
    *ink = st == LS_DIM ? T_DIM : T_THEME;
    if (st == LS_MUTE) {
        *ink = T_INK;
        return T_KEY;
    }
    if (st == LS_HELD) {
        *ink = T_BG;
        return T_ACCENT;
    }
    if (st == LS_SEL || st == LS_WAIT) {
        *ink = T_INK;
        return T_THEME;
    }
    return T_RAISE;
}
/* a cell at x, y, h px high: big (h > 30) the icon over the name; a name: compact, the icon at the right (none: no
 * icon); else a black key's: two icons at the right. tri: a HOLD cell in a SET layer */
/* a cell's box: its fill on the panel (cells are 4 px apart from x 6, y 4). Returns the fill */
static uint16_t lc_box(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t fill)
{
    cv_rrect(x, y, w, h, 4, fill, T_SURF);
    return fill;
}
static void lcell(int32_t x, int32_t y, int32_t h, const char *note, uint32_t icon, uint32_t icon2, const char *name,
                  uint32_t st, int tri)
{
    uint16_t ink, fill = lc_fill(st, &ink), idle = fill == T_RAISE;
    fill = lc_box(x, y, LC_W, h, fill);
    if (note)
        cv_text_on(x + 5, y + 3, &AF_S, note, idle ? T_MID : ink, fill);
    if (h > 30) {                                   /* the icon's cell over the name, 6 px between, centred */
        int32_t t = y + HALF_UP(h - (16 + 6 + AF_S_CAP_H));
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V | AL_N(2), "layer cell icon + name centred up/down");
        GFX_HOOK_ALIGN(x, 0, x + LC_W, 0, AL_H | AL_PASS, "layer cell icon / name centred across");
        cv_icon_in(x, t, LC_W, 0, 16, icon, ink, fill);
        GFX_HOOK_ALIGN(x, 0, x + LC_W, 0, AL_H | AL_PASS, "layer cell icon / name centred across");
        cv_text_in(x, t + 22 - AF_S_CAP_Y, LC_W, &AF_S, name, ink, fill);
    } else if (name && h > 24) {                           /* the same, 12 px, 2 px between (the icon at a side) */
        int32_t t = y + HALF_UP(h - (12 + 2 + AF_S_CAP_H));
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V | AL_N(2), "layer cell icon + name centred up/down");
        cv_icon_on(note ? x + LC_W - 17 : x + 5, t, 12, icon, ink, fill);
        GFX_HOOK_ALIGN(x, 0, x + LC_W, 0, AL_H | AL_PASS, "layer cell icon / name centred across");
        cv_text_in(x, t + 14 - AF_S_CAP_Y, LC_W, &AF_S, name, ink, fill);
    } else if (name && *name) {                           /* a key's row: the icon and the name on its middle */
        if (!note && icon < ICON_COUNT) {                  /* (the EDIT layer: the engine's icon instead of the key) */
            GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
            cv_icon_in(x + 3, y, 0, h, 12, icon, ink, fill);
            GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
            cv_text_r(x + LC_W - 3, y + CAP_IN(S, h), &AF_S, name, ink, fill);
        } else {
            GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
            cv_text_r(x + LC_W - 5, y + CAP_IN(S, h), &AF_S, name, ink, fill);
        }
    } else if (!name) {                                    /* (an empty name: the note alone) */
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
        cv_icon_in(x + LC_W - 31, y, 0, h, 12, icon, ink, fill);
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "layer key cell icon / name centred up/down");
        cv_icon_in(x + LC_W - 17, y, 0, h, 12, icon2, ink, fill);
    }
    if (tri) {
        int32_t j;
        for (j = 0; j < 4; j++)
            cv_rect(x + LC_W - 7 + j, y + 2 + j, 4 - j, 1, ink);
    }
}
/* GLO (DUELL 5b.3), two parts: a column each (88 px, x 6 and 146; part 1 is the left of KNOB 1 and 2, part 2 the right): one tall
 * cell with the part's badge and the word in M (LIVE, or MUTED in the key colour with the badge dark); between the columns (x 98 .. 142) the
 * parts' level meters (glo_meters; a muted part's bar greyed and empty). A part's cell carries its colour */
#define GC_W 88
#define GC_X(k) (6 + 140 * (int32_t)(k))
static void gcell(int32_t x, int32_t y, int32_t w, int32_t h, int32_t k, const char *word, uint32_t st, uint32_t lit)
{
    uint16_t ink, fill = lc_fill(st, &ink);
    if (ink == T_THEME)                                 /* the part's colour for what the selection's ink / fill is */
        ink = T_PTHEME((uint32_t)k);
    if (fill == T_THEME)
        fill = T_PTHEME((uint32_t)k);
    fill = lc_box(x, y, w, h, fill);
    cv_part(x + 8, y + HALF_UP(h - 20), 20, (uint32_t)k, lit, fill);   /* the part's badge (20 px) and the word, M, on the cell's middle */
    GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "glo cell word centred up/down");
    cv_text_on(x + 32, y + CAP_IN(M, h), &AF_M, word, ink, fill);
}

/* ------------------------------------------------ GLO's level meters --- */
/* One bar per part between the columns: the part's own level before MASTER, peak, -48 .. 0 dBFS (0 dBFS = 32768 in the mix's Q15; what
 * the part adds to the mix with its LEVEL, DIST, INSERT, EQ and DUCK applied, before PAN and the sends: fx.c mix_part's t->peak, the
 * MIXER's too; in SPLIT the bus of the part's side as well, with its delay and reverb return: fx.c meter_bus, scanned only while this page
 * shows). The audio path keeps a running maximum (no work added in STEREO and LAYER); here each frame takes it and clears it. The bar
 * follows the peak up at once and falls at 36 dB/s; a thin tick holds the highest peak 1.2 s and then falls the same way; the part's
 * colour up to -6 dBFS (the master's limiter starts at -5), the accent from -6 dBFS, red from -1 dBFS; a lamp over the bar stays red 1.5 s
 * after a peak past -1 dBFS (CLIP). Solid colours only (HI-CON and every theme). Units: 16 per octave of amplitude (6 dB = 16 units / 2.67) */
#define GM_X 98                                         /* the strip (x), 44 px: bars 12 px at 0 and 32 */
#define GM_W 44
#define GM_Y 4                                          /* .. and y in the panel */
#define GM_H 92
#define GM_BY 9                                         /* the bars: top (0 dBFS) at 9, 76 px for 48 dB */
#define GM_BH 76
#define GM_U0 112                                       /* -48 dBFS (128 = 8 octaves under 240) */
#define GM_UHOT 224                                     /* -6 dBFS */
#define GM_UCLIP 237                                    /* -1 dBFS */
#define GM_CLIP_A 29205                                 /* .. as an amplitude */
#define GM_HOLD_MS 1200u
#define GM_LAMP_MS 1500u
static struct {
    int32_t lvl[NSHOWN], hold[NSHOWN];                  /* units x 16 */
    uint32_t hold_t[NSHOWN], clip_t[NSHOWN], t;
    uint32_t sig;
    uint8_t seen;
} gm;
/* 16 log2(a): 240 at 32768 (a < 16: 0) */
static int32_t gm_units(int32_t a)
{
    static const uint8_t F[16] = {0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15};   /* 16 log2(1 + i / 16) */
    int32_t lg = 0;
    if (a < 16)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    return lg * 16 + (int32_t)F[((a << 4) >> lg) & 15];
}
static int32_t gm_px(int32_t u16)                       /* units x 16 -> bar pixels */
{
    return clamp((u16 / 16 - GM_U0) * GM_BH / 128, 0, GM_BH);
}
/* each frame while the page shows: the audio path's peaks into the bars */
static void glo_meter_step(void)
{
    uint32_t k, now = fm1_ms, dt = now - gm.t;
    gm.t = now;
    if (!gm.seen || dt > 200u)
        dt = 33u;                                       /* (first frame, or the page was not drawn: one frame's fall) */
    meter_on = 255;                                     /* fx.c: the SPLIT buses are scanned for a while */
    for (k = 0; k < NSHOWN; k++) {
        int32_t a = trk[k].peak, u;
        trk[k].peak = 0;
        if (duell_mode_req == DM_SPLIT) {               /* SPLIT: the bus of the part's side (its returns too) */
            int32_t b;
            fm1_irq_off();
            b = meter_bus[k];
            meter_bus[k] = 0;
            fm1_irq_on();
            a = a > b ? a : b;
        }
        if (!glo_sounding(k))
            a = 0;
        u = gm_units(a) * 16;
        gm.lvl[k] = u > gm.lvl[k] ? u : gm.lvl[k] - (int32_t)(dt * 3u >> 1);   /* 96 units/s = 36 dB/s */
        if (gm.lvl[k] < 0)
            gm.lvl[k] = 0;
        if (u >= gm.hold[k]) {
            gm.hold[k] = u;
            gm.hold_t[k] = now;
        } else if (now - gm.hold_t[k] > GM_HOLD_MS) {
            gm.hold[k] -= (int32_t)(dt * 3u >> 1);
            if (gm.hold[k] < gm.lvl[k])
                gm.hold[k] = gm.lvl[k];
        }
        if (a >= GM_CLIP_A)
            gm.clip_t[k] = now | 1u;
        if (gm.clip_t[k] && now - gm.clip_t[k] > GM_LAMP_MS)
            gm.clip_t[k] = 0;
    }
    gm.seen = 1;
}
static uint16_t gm_zone(int32_t u16, uint32_t k)       /* the colour for a level (a zone colour too like the part's own, as HI-CON's red part 1: the text colour) */
{
    uint16_t c = u16 / 16 >= GM_UCLIP ? T_REC : u16 / 16 >= GM_UHOT ? T_ACCENT : T_PTHEME(k);
    return c != T_PTHEME(k) && ux_cr(c, T_PTHEME(k)) < 160u ? T_TEXT : c;
}
/* the strip: scale in the middle, a bar each side; redrawn only when a bar, a tick or a lamp changed (or the panel was) */
static void glo_meters(void)
{
    static const int8_t DB[5] = {0, -12, -24, -36, -48};
    static const char *const DBN[5] = {"0", "-12", "-24", "-36", "-48"};
    uint32_t k, i;
    uint32_t sig = 0x6D5u;
    int32_t px[NSHOWN], hp[NSHOWN];
    for (k = 0; k < NSHOWN; k++) {
        px[k] = gm_px(gm.lvl[k]);
        hp[k] = gm_px(gm.hold[k]);
        sig = (sig * 33u + (uint32_t)px[k]) * 33u + (uint32_t)hp[k];
        sig = sig * 33u + (gm.clip_t[k] != 0u) + (uint32_t)gm_zone(gm.hold[k], k) * 7u + glo_sounding(k) * 3u;
    }
    sig += ux.gen * 4099u + (uint32_t)T_SURF * 131u;
    if (!ui.force && sig == gm.sig)
        return;
    gm.sig = sig;
    cv_begin(GM_W, GM_H, T_SURF);
    cv_bg = T_SURF;
    for (i = 0; i < 5u; i++) {                          /* the scale: the dB figures between the bars, a hair line each side */
        int32_t y = GM_BY + GM_BH - (DB[i] + 48) * GM_BH / 48;
        cv_text_in(12, y - AF_S_CAP_Y - AF_S_CAP_H / 2, 20, &AF_S, DBN[i], T_MID, T_SURF);
    }
    for (k = 0; k < NSHOWN; k++) {
        int32_t x = k ? GM_W - 12 : 0, yb = GM_BY + GM_BH, z1 = GM_BH * (GM_UHOT - GM_U0) / 128, z2 = GM_BH * (GM_UCLIP - GM_U0) / 128, n;
        cv_rect(x, GM_BY, 12, GM_BH, T_RAISE);          /* the track */
        n = px[k] < z1 ? px[k] : z1;                    /* the part's colour, then the accent, then red */
        if (n > 0)
            cv_rect(x, yb - n, 12, n, glo_sounding(k) ? T_PTHEME(k) : T_DIM);
        if (px[k] > z1)
            cv_rect(x, yb - (px[k] < z2 ? px[k] : z2), 12, (px[k] < z2 ? px[k] : z2) - z1, gm_zone(GM_UHOT * 16, k));
        if (px[k] > z2)
            cv_rect(x, yb - px[k], 12, px[k] - z2, gm_zone(GM_UCLIP * 16, k));
        cv_rect(x, yb - z1, 12, 1, T_BG);               /* the -6 dBFS and -1 dBFS lines, where the colour changes */
        cv_rect(x, yb - z2, 12, 1, T_BG);
        if (hp[k] > 0)                                  /* the peak hold: a 2 px tick in the zone's colour */
            cv_rect(x, yb - hp[k] - (hp[k] < GM_BH ? 1 : 0), 12, 2, gm_zone(gm.hold[k], k) == T_PTHEME(k) ? T_TEXT : gm_zone(gm.hold[k], k));
        cv_rect(x, GM_BY - 1, 12, 1, T_BG);             /* the 0 dBFS line above the bar */
        cv_rect(x, 0, 12, 6, gm.clip_t[k] ? gm_zone(GM_UCLIP * 16, k) : T_RAISE);   /* the CLIP lamp */
    }
    cv_blit(GM_X, Y_GRAPH + GM_Y);
}
static void layer_glo(void)
{
    uint32_t e;
    for (e = 0; e < NSHOWN; e++) {
        gcell(GC_X(e), 4, GC_W, 112, (int32_t)e, trk[e].p[P_MUTE] ? "MUTED" : "LIVE", trk[e].p[P_MUTE] ? LS_MUTE : LS_OFF, !trk[e].p[P_MUTE]);
    }
}
static void layer_scl(void)                             /* KNOB 1's scales, 4 x 4, the one now selected */
{
    uint32_t i, sc = (uint32_t)scale_of(TSEL);
    for (i = 0; i < 16u && i <= (uint32_t)TP[P_SCALE].max; i++) {
        int32_t x = LC_X(i % 4u), y = 4 + 29 * (int32_t)(i / 4u);
        uint16_t ink, fill = lc_fill(i == sc ? LS_SEL : LS_OFF, &ink);
        fill = lc_box(x, y, LC_W, 25, fill);
        GFX_HOOK_ALIGN(x, y, x + LC_W, y + 25, AL_HV, "scale cell name centred");
        cv_text_in(x, y + CAP_IN(S, 25), LC_W, &AF_S, TP[P_SCALE].names[i], ink, fill);
    }
}
/* 1.4.1: the engine's quick actions (black keys 1..n) in a row of n cells under the engines, 18 px high, 4 px apart
 * (8: 25 px, as SEQ TOOLS' lanes; 11: 17 px): the name; SET EQ_SHOW_MUTE: off a muted track's KEY; a HOLD one's
 * corner triangle */
static void eqa_row(uint32_t n)
{
    uint32_t i, w = (228u - 4u * (n - 1u)) / n;
    for (i = 0; i < n; i++) {
        const eqa_t *a = eqa_of(TSEL, i);
        uint32_t on = a->on(TSEL, a->arg) != 0u, st = a->show == EQ_SHOW_MUTE ? (on ? LS_OFF : LS_MUTE)
                    : on ? (a->kind == LK_HOLD ? LS_HELD : LS_SEL) : LS_OFF;
        int32_t x = 6 + (int32_t)((w + 4u) * i), j;
        uint16_t ink, fill = lc_box(x, 100, (int32_t)w, 18, lc_fill(st, &ink));
        GFX_HOOK_ALIGN(x, 100, x + (int32_t)w, 118, AL_HV, "edit quick action cell name centred");
        cv_text_in(x, 100 + CAP_IN(S, 18), (int32_t)w, &AF_S, a->name(TSEL, a->arg), ink, fill);
        for (j = 0; a->kind == LK_HOLD && j < 4; j++)
            cv_rect(x + (int32_t)w - 7 + j, 102 + j, 4 - j, 1, ink);
    }
}
static void layer_edit(void)                            /* INIT on F3, the quick actions (or the sound) under it */
{                                                        /* (no engine keys: the ENGINE page and KNOB 1 pick the engine) */
    uint32_t na = eqa_count(TSEL);
    char n[2] = {W_NOTE[LY_INIT], 0};
    lcell(LC_X(0), 4, 28, n, ICON_X_WARN, 0, "INIT", LS_OFF, 0);
    if (na)                                             /* the black keys' quick actions in the sound's place (KNOB 2 */
        eqa_row(na);                                    /* still shows its number) */
    else
        engine_sound_row(104);
}

/* SEQ TOOLS: the white keys' tools, 4 a row from F3 (the whole sequence; RANDOM COOK BEAT; the lane's), on a DRUM
 * track the lanes of black keys 1..8 under them (the selected one: the selection's fill). A melodic track: the two
 * rows of the sequence's tools only */
static const uint8_t TL_ICON[TL_N] = {ICON_CLEAR, ICON_X_REVERSE, ICON_X_LEFT, ICON_X_RIGHT, ICON_PROB, ICON_W_SH,
    ICON_DRUM, 0, ICON_CLEAR, ICON_X_REVERSE, ICON_STEPS, ICON_PROB};
static const char *const TL_NAME[TL_N] = {"CLEAR", "REVERSE", "SHIFT", "SHIFT", "RANDOM", "COOK", "BEAT", 0,
    "CLR", "REV", "FILL", "RND"};
static void layer_seq(void)
{
    uint32_t p, st = LS_OFF, drum = (uint32_t)drum_track(TSEL);
    char n[3] = {0, 0, 0}, nm[8];
    for (p = 0; p < TL_N; p++) {
        if (!tl_cell(TSEL, p))
            continue;
        n[0] = W_NOTE[p];
        str_cpy(nm, TL_NAME[p], sizeof nm);
        if (p >= TL_LCLR) {                             /* the lane's: "CLR BD" */
            str_cpy(nm + str_len(nm), " ", 2);
            str_cpy(nm + str_len(nm), drum_lane_abbr(TSEL, ui.lane), sizeof nm - str_len(nm));
        }
        lcell(LC_X(p % 4u), 4 + 32 * (int32_t)(p / 4u), 28, n, TL_ICON[p], 0, nm, st, 0);
    }
    for (p = 0; drum && p < NLANE; p++) {               /* the lanes (black keys 1..8): 8 cells of 25 px, 4 px apart */
        int32_t x = 6 + 29 * (int32_t)p;
        uint16_t ink, fill = lc_box(x, 100, 25, 18, lc_fill(p == ui.lane % NLANE ? LS_SEL : LS_OFF, &ink));
        GFX_HOOK_ALIGN(x, 100, x + 25, 118, AL_HV, "tools lane cell name centred");
        cv_text_in(x, 100 + CAP_IN(S, 18), 25, &AF_S, drum_lane_abbr(TSEL, p), ink, fill);
    }
}

/* REC: a row a key (26 px, 4 apart): the key's cell (its note, icon and name), then CLEAR's track and what it holds,
 * or the setting's three values, the one now with the selection's fill (as MENU > AUDIO steps them) */
#define LR_KW 78                                         /* the key's cell; the values 46 px each: x 6 .. 234 */
#define LR_VW 46
#define LR_H 26
static void layer_rec(void)
{
    uint32_t r, v;
    char n[3] = {0, 0, 0}, b[16];
    for (r = 0; r < 4u; r++) {
        int32_t y = 4 + (LR_H + 4) * (int32_t)r, x = 6, vx = 6 + LR_KW + 4;
        uint32_t dim = r == 0u && !rec_clearable();
        uint16_t ink, fill = lc_box(x, y, LR_KW, LR_H, lc_fill(dim ? LS_DIM : LS_OFF, &ink));
        n[0] = W_NOTE[r];
        GFX_HOOK_ALIGN(0, y, 0, y + LR_H, AL_V, "rec layer key cell on its middle");
        cv_text_on(x + 5, y + CAP_IN(S, LR_H), &AF_S, n, fill == T_RAISE || fill == T_SURF ? T_MID : ink, fill);
        GFX_HOOK_ALIGN(0, y, 0, y + LR_H, AL_V, "rec layer key cell on its middle");
        cv_icon_in(x + 15, y, 0, LR_H, 12, r ? RL_ICON[r - 1u] : ICON_CLEAR, ink, fill);
        GFX_HOOK_ALIGN(0, y, 0, y + LR_H, AL_V, "rec layer key cell on its middle");
        cv_text_r(x + LR_KW - 5, y + CAP_IN(S, LR_H), &AF_S, r ? RL_LABEL[r - 1u] : "CLEAR", ink, fill);
        if (r == 0u) {                                   /* "T2 SEQUENCE" / "T2 EMPTY" */
            int32_t w = 234 - vx;
            b[0] = 'T';
            b[1] = (char)('1' + song.sel);
            str_cpy(b + 2, rec_clearable() ? " SEQUENCE" : " EMPTY", sizeof b - 2);
            fill = lc_box(vx, y, w, LR_H, lc_fill(dim ? LS_DIM : LS_OFF, &ink));
            GFX_HOOK_ALIGN(vx, y, vx + w, y + LR_H, AL_HV, "rec layer clear cell centred");
            cv_text_in(vx, y + CAP_IN(S, LR_H), w, &AF_S, b, ink, fill);
            continue;
        }
        for (v = 0; v < 3u; v++, vx += LR_VW + 4) {
            fill = lc_box(vx, y, LR_VW, LR_H, lc_fill(v == rp_get(RL_F[r - 1u]) ? LS_SEL : LS_OFF, &ink));
            GFX_HOOK_ALIGN(vx, y, vx + LR_VW, y + LR_H, AL_HV, "rec layer value cell centred");
            cv_text_in(vx, y + CAP_IN(S, LR_H), LR_VW, &AF_S, rl_name(r - 1u, v), ink, fill);
        }
    }
}

/* GLO's four cards: KNOB 1 MUTE 1 and 2 MUTE 2 (ON = muted, OFF), KNOB 3, 4 LEVEL 1, 2 (the part's
 * colour; DIM when it is not sounding) */
static void glo_cards(void)
{
    char val[12];
    const char *unit;
    uint32_t c;
    for (c = 0; c < NSHOWN; c++) {
        uint32_t mu = trk[c].p[P_MUTE] != 0;
        draw_column(c, c ? "MUTE 2" : "MUTE 1", mu ? "ON" : "OFF", "", mu ? T_PTHEME(c) : VAL(c), -1, ICON_MUTE);
    }
    for (c = 0; c < NSHOWN; c++) {
        param_format(&TP[P_LEVEL], trk[c].p[P_LEVEL], val, &unit);
        draw_column(2u + c, c ? "LEVEL 2" : "LEVEL 1", val, unit, glo_sounding(c) ? T_PTHEME(c) : T_DIM,
                    RATIO(&TP[P_LEVEL], trk[c].p[P_LEVEL]), trk_icon(c, 1));
    }
}
static void layer_cards(uint32_t l)
{
    char val[12];
    uint32_t c;
    if (l == LAYER_GLO) {
        glo_cards();
    } else if (l == LAYER_EDIT) {
        engine_columns(0);
    } else if (l == LAYER_REC) {                        /* CLICK COUNT-IN LEVEL, an empty card */
        for (c = 0; c < 3u; c++) {
            uint32_t v = rp_get(RL_F[c]);
            draw_column(c, RL_LABEL[c], rl_name(c, v), "", c < 2u && !v ? T_DIM : VAL(c), (int32_t)v * 500, RL_ICON[c]);
        }
        draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
    } else {                                            /* the page's four (SCL: LY_SCL) */
        uint8_t h = ui.home, pg = ui.page;
        ui.home = 0;
        ui.page = (uint8_t)page_first(LAYERS[l].fam);
        page_over = ly_page(l);
        draw_columns();
        page_over = 0;
        ui.home = h;
        ui.page = pg;
    }
}

static void draw_layer(void)
{
    uint32_t l = ui.layer ? ui.layer % LAYER_N : LAYER_GLO, sig = l * 7919u + ux.gen * 977u;   /* (no layer: GLO's page) */
    layer_cards(l);
    if (l == LAYER_GLO)
        sig += (uint32_t)(trk[0].p[P_MUTE] | trk[1].p[P_MUTE] << 1 | trk[2].p[P_MUTE] << 2 | trk[3].p[P_MUTE] << 3) * 131u;
    else if (l == LAYER_SCL)
        sig += (uint32_t)scale_of(TSEL) * 31u + (uint32_t)root_of(TSEL) * 7u + (uint32_t)offscale_g * 3u;
    else if (l == LAYER_REC)
        sig += (uint32_t)ui_rec_prefs * 31u + (uint32_t)rec_clearable() * 5u + song.sel * 977u;
    else if (l == LAYER_SEQ)
        sig += (uint32_t)drum_track(TSEL) * 31u + ui.lane * 5u + (uint32_t)TSEL->p[P_E0] * 131u + song.sel * 977u;
    else
        sig += snd_id() * 31u + (uint32_t)preset_favorite() * 5u + up_gen * 101u +
               eqa_sig() * 2654435761u;
    if (ui.force || sig != ui.layer_sig) {
        ui.layer_sig = sig;
        cv_begin(240, H_GRAPH, T_BG);
        cv_rrect(3, 0, 234, H_GRAPH, 5, T_SURF, T_BG);
        cv_bg = T_SURF;
        if (l == LAYER_GLO)
            layer_glo();
        else if (l == LAYER_SCL)
            layer_scl();
        else if (l == LAYER_SEQ)
            layer_seq();
        else if (l == LAYER_REC)
            layer_rec();
        else
            layer_edit();
        cv_blit(0, Y_GRAPH);
        if (l == LAYER_GLO)
            gm.sig = 0;                                 /* (the strip over the panel just drawn) */
    }
    if (l == LAYER_GLO) {                               /* the level meters: their own strip, each frame */
        glo_meter_step();
        glo_meters();
    }
}
