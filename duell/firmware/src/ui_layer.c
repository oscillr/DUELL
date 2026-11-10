/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca quick layers: while a page button is held, the keys and KNOB 1..4
 * are its shortcuts and its map shows over the page. One table (LAYERS) drives the gesture, the keys, the knobs,
 * the LEDs and the overlay:
 *   FX   (DUELL 0.2.2: not a layer any more. The FX hold layer and its performance effects are removed: FX is a plain page button, a
 *              short press opens the FX pages and holding it does nothing special. The LAYER_RES slot keeps the other layers' ids)
 *   GLO  QUICK (DUELL 0.2.11, LK_QUICK): GLO's MAIN PAGE, graph GR_GLO, which draw_layer draws, as a layer over any page (QUICK GLO: hold GLO; a tap
 *              still opens GLO's pages). The keys play notes there as on every page; 0.2.24 (owner: PAN could not be set from the panel): KNOB 1 LEVEL 1,
 *              KNOB 2 LEVEL 2, KNOB 3 PAN 1, KNOB 4 PAN 2 (glo_knob; item 117: K1 K3 part 1, K2 K4 part 2, as the screen: part 1 left, part 2 right), and the MUTES moved to the keys: with GLO held OCT- mutes /
 *              unmutes part 1, OCT+ part 2 (layer_glo_oct: on the press, no octave on the release); between the parts' columns the level
 *              meters (glo_meters). Let go: the page under it is back. 0.2.21: GLO's page is gone from the page round (a tap of GLO opens TEMPO): this layer is its only place, so the RESTART of GLO held + PLAY, which only worked on that page, is gone too)
 *   EDIT SET   QUICK FILTER (DUELL 0.2.9): KNOB 1..4 CUT RES MODE and the AMP ENV's REL (ly_page: 0.2.14, the multimode filter on every
 *              engine; up to 0.2.8 ENG, No., FAV: the ENGINE page has them). 0.2.11: the keys stay notes (ly_keys): no INIT key and no engine quick actions
 *              any more (INIT is EDIT > ENGINE, K4). OCT- puts back the four
 *   LFO ENV ARP QUICK (DUELL 0.2.9, LK_QUICK): QUICK LFO (LFO 1 RATE, its FLT depth, LFO 2 RATE, its FLT depth; a RATE whose LFO is synced:
 *              its SYNC), QUICK ENV (AMP ATK REL, ENV 2's FLT depth and DEC), QUICK ARP (MODE RATE GATE OCT), the selected part's (ly_page).
 *              The keys stay notes (no kb_mask: they are not the layer's, a key is no combo), OCT- / OCT+ keep the octave, no put back,
 *              no lock (#83: two quick taps step the family's pages as before). One seen bit for the three and GLO (bit 0)
 *   SEQ  SET   (1.2, on every page) TOOLS: the white keys from F3 the sequence tools
 *              (ui_tools.c: CLEAR, REVERSE, SHIFT < >, RANDOM, COOK, TRANS + / TRANS - on E4, F4; 0.2.14: the DRUM tracks' BEAT and lane tools are gone); KNOB 1..4 LEN DIV SWING GATE (the PATTERN page's up to
 *              1.1.5: the page is gone, they are here). Each action one undo (SAVE held), OCT- the track as the layer
 *              opened (tl_open)
 *   REC  SET   (1.1.5, on every page; REC's tap still arms / disarms) DUELL 1.0 layout (QUICK_REC_NEW_LAYOUT.png; FIRMWARE_CHANGE_LIST_2026-10-28.md
 *              B.6): four cells of template 8 (grid_on 14, 2 x 2, knob c in cell c; the layer draws no panel of its own): KNOB 1 CLICK (OFF REC ON),
 *              KNOB 2 COUNT (COUNT-IN: OFF 1 BAR 2 BARS), KNOB 3 LEVEL (CLICK LEVEL: LOW MID HIGH), each with the choice's segment line; KNOB 4
 *              CLEAR: NO SEQUENCE (dimmed) when the selected track holds nothing, else TURN TO CLEAR; turning it opens the "CLEAR Tn SEQUENCE?"
 *              dialog (CF_CLEAR_SEQ: OCT+ YES, OCT- NO; the steps and automation go, one undo, SAVE held). The white keys F G A B no
 *              longer act here (keys and LEDs). The settings are MENU > AUDIO's (ui.c rp_get / rp_put: one store), saved when the layer
 *              closes; OCT- puts back the settings and the track as the layer opened (tl_open)
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
enum { LK_HOLD, LK_SET, LK_QUICK };   /* (LK_QUICK, 0.2.9: a QUICK EDIT layer, its keys stay notes) */
typedef struct {
    uint8_t btn, kind, fam;            /* the button, HOLD / SET, the family whose first page KNOB 1..4 edit */
    const char *head;                  /* (FAM_HOME: the layer's own knobs) the header over the map */
    const char *name, *tag;            /* the footer's page name and number place: "GLO" "SET" */
} layer_t;
static const layer_t LAYERS[LAYER_N] = {
    {0, 0, 0, "", "", ""},
    {0, 0, 0, "", "", ""},                                  /* (LAYER_RES: the FX hold layer, removed in 0.2.2) */
    {B_GLO, LK_QUICK, FAM_HOME, "QUICK GLO", "GLO", ""},
    {B_EDIT, LK_SET, FAM_EDIT, "QUICK MULTI", "EDIT", "SET"},   /* (0.2.9: every layer's header is QUICK and the page, the bolt before it in the accent: ui_draw.c head_msg; QUICK SCALE removed, owner 2026-10-28: SCL is a plain page button) */
    {B_SEQ, LK_SET, FAM_SEQ, "QUICK SEQ", "SEQ", "TOOLS"},
    {B_REC, LK_SET, FAM_HOME, "QUICK REC", "REC", "SET"},
    {B_LFO, LK_QUICK, FAM_LFO, "QUICK LFO", "LFO", ""},
    {B_ENV, LK_QUICK, FAM_ENV, "QUICK ENV", "ENV", ""},
    {B_ARP, LK_QUICK, FAM_ARP, "QUICK ARP", "ARP", ""},
};
static const uint8_t LY_KC[LAYER_N] = {0, 0, KC_GLO, KC_EDIT, KC_SEQ, KC_REC, KC_LFO, KC_ENV, KC_ARP};
/* SEQ TOOLS' knobs: LEN DIV SWING GATE (the length the tools work in; up to 1.1.5 the PATTERN page's) */
static const page_t LY_SEQ = {"PATTERN", FAM_SEQ, SC_TRACK, GR_STEPS, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}};
/* DUELL 0.2.9, the QUICK EDIT layers' knobs, the selected part's (EDIT, LFO, ENV, ARP: LY_Q[q], q = ly_q(l)): the same parameters, cards
 * (names, values, greying), motion and MIDI LEARN as on their pages. QUICK FILTER's K1..K3 are the MULTIMODE filter's CUT RES MODE (ids 45..47)
 * on every engine, K4 the AMP ENV's REL (0.2.14, owner: up to 0.2.13 ANALOG TRIO NOISE showed their own filter here, and ANALOG's TYPE
 * switched that one, not the multimode filter he meant; their own filters stay on their EDIT pages VCF, TRIO FILTER, NOISE FILT; all ten
 * shown engines have the multimode filter, ufilt 1 or 2; SLOPE stays on the FILTER page). QUICK LFO: a synced LFO's RATE does nothing (its
 * card is faded on the LFO page), so its knob is the active one, SYNC (to OFF: RATE again) */
static const page_t LY_Q[4] = {
    {"MULTI FILTER", FAM_EDIT, SC_TRACK, GR_NONE, {P_FREEA, P_FREEB, P_FREEC, P_REL}},   /* 0.2.20 (owner): CUT RES MODE, REL */
    {"LFO", FAM_LFO, SC_TRACK, GR_NONE, {P_LRATE, P_LD_FLT, P_L2RATE, XW_BASE + X_LFO2}},   /* (DUELL 1.1: K4 is LFO 2's TWIN CUTOFF depth; its MULTI CUTOFF depth no longer acts) */
    {"ENV", FAM_ENV, SC_TRACK, GR_NONE, {P_ATK, P_REL, P_E2FLT, P_E2DEC}},
    {"ARP", FAM_ARP, SC_TRACK, GR_NONE, {P_AMODE, P_ARATE, P_AGATE, P_AOCT}},
};
static page_t ly_qp;                                    /* LY_Q[q] as the part's engine and SYNC make it now (ly_page) */
static uint32_t ly_q(uint32_t l) { return l == LAYER_EDIT ? 0u : l == LAYER_LFO ? 1u : l == LAYER_ENV ? 2u : l == LAYER_ARP ? 3u : 4u; }
static const page_t *ly_page(uint32_t l)
{
    uint32_t q = ly_q(l);
    if (q > 3u)
        return l == LAYER_SEQ ? &LY_SEQ : 0;
    ly_qp = LY_Q[q];                                    /* (QUICK FILTER: the multimode filter on every engine, 0.2.14) */
    if (q == 0u && twin_on(TSEL)) {                     /* QUICK MULTI with filter 2 on (cloud job H, decided 2026-10-19): LINKED (and COMB, which follows the key by itself): CUT RES DRIVE AMP REL; UNLINKED: CUT 1 RES 1 CUT 2 RES 2 (the absolute CUTOFF 2) */
        if (XV(TSEL, X_LINK) || TSEL->p[P_TWTYPE] == TT_COMB) {
            ly_qp.id[2] = P_DRV2;
        } else {
            ly_qp.id[2] = P_CUT2;
            ly_qp.id[3] = P_RES2;
        }
    }
    if (q == 1u) {                                      /* a synced LFO: its SYNC */
        if (TSEL->p[P_LSYNC])
            ly_qp.id[0] = P_LSYNC;
        if (TSEL->p[P_L2SYNC])
            ly_qp.id[2] = P_L2SYNC;
    }
    return &ly_qp;
}
static int ly_two(void) { return twin_on(TSEL) && !XV(TSEL, X_LINK) && TSEL->p[P_TWTYPE] != TT_COMB; }   /* QUICK MULTI shows both filters' cutoffs and resonances (ly_page) */
/* the name a QUICK card shows where the generic one does not say which filter: QUICK MULTI with both filters on (ui_draw.c draw_columns), MULTI CUTOFF / MULTI RES / TWIN CUTOFF / TWIN RES in
 * two lines (DUELL 1.1, owner; the group cells above say MULTI FILTER and TWIN FILTER; the page keeps its name); QUICK LFO's K2 and K4 (each LFO's cutoff depth) MULTI CUTOFF; 0: the card's own */
static const char *quick_label(const page_t *pg, uint32_t s)
{
    static const char *const L[4] = {"MULTI\nCUTOFF", "MULTI\nRES", "TWIN\nCUTOFF", "TWIN\nRES"};
    if (pg != &ly_qp)
        return 0;
    if (ly_qp.id[3] == P_RES2 && ly_two())
        return L[s & 3u];
    if (ly_qp.id[1] == P_LD_FLT && ly_qp.id[3] == XW_BASE + X_LFO2 && (s == 1u || s == 3u))   /* QUICK LFO */
        return s == 1u ? "MULTI\nCUTOFF" : "TWIN\nCUTOFF";
    return 0;
}
/* the layer takes the keys (seq.c kb_mask). 0.2.11: not QUICK FILTER's either (EDIT's keys used to be INIT and the engine's quick actions: an F played
 * erased the sound); every quick layer's keys play */
static int ly_keys(uint32_t l) { return l && LAYERS[l % LAYER_N].kind != LK_QUICK && l != LAYER_EDIT; }
static int ly_lock(uint32_t l) { return l && LAYERS[l % LAYER_N].kind != LK_QUICK; }   /* a double tap locks it open (#83; EDIT's keeps its lock) */
static uint32_t ly_seen(uint32_t l) { return LAYERS[l % LAYER_N].kind == LK_QUICK ? 1u : 1u << l; }   /* its bit in layer_seen */
#define LY_OPEN 2u                     /* ui.ly_t0: the map opened (no tap any more) */
#define LY_COMBO 4u                    /* .. by a combo */
#define LY_DEAD 8u                     /* .. pressed where there is no layer: does nothing */
#define layer_seen (favorites.factory[15][31])   /* bit l: layer l opened once (a byte no engine uses); bit 0 (LAYER_NONE's, never set
                                                  * before): the QUICK EDIT layers LFO, ENV, ARP, one for the three (ly_seen) */
/* (favorites.factory[15][14..23], 10 bytes: the FX layer's key map up to 0.2.1, retired in 0.2.2: not read or written any more, never reused) */

static struct {
    uint8_t l, trk, loaded, oct;       /* SET: the layer and the track of the snapshot; EDIT: a sound loaded since
                                        * it opened (SEQ: a tool used); OCT- / OCT+ pressed in a SET layer (bits) */
    uint8_t cook;                      /* SEQ: COOK pressed since it opened */
    uint8_t rp_dirty;                  /* REC: a setting changed (settings_save when the layer lets go) */
    uint8_t qid[4];                    /* QUICK MULTI: the ids of its four when it opened (OCT-: put back) */
    int16_t v[9];                      /* the values when it opened: SEQ LEN DIV SWING GATE; REC the settings
                                        * (ui_rec_prefs); EDIT its four (4 .. 7); the tempo (8) */
    uint8_t quiet;                     /* #39: a layer closed after it opened: KNOB 1..4 do nothing until quiet_t + */
    uint32_t quiet_t;                  /* LY_QUIET_MS (fm1_ms) */
    uint8_t dt_l, dt_hint, dtap;       /* #83: the layer of the last tap that opened a page (0 none), its hint said; the
                                        * press now armed is that tap's second (a double tap) */
    uint32_t dt_ms;                    /* .. when that tap was let go (fm1_ms) */
    struct { uint8_t page, act, seq, fam[FAM_COUNT], rec, go; } nv;   /* .. the page it left (put back by the
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
        if (ly_keys(l))                                 /* (0.2.9: the QUICK EDIT layers' keys stay notes; 0.2.11: GLO's and EDIT's too) */
            m |= ly_bit(l);
    return m;
}
static uint32_t layer_btn(void) { return LAYERS[ui.layer % LAYER_N].btn; }
static const char *layer_head(void) { return LAYERS[ui.layer % LAYER_N].head; }
/* the footer's page place for the layer shown ("GLO" | "SET") */
static void layer_foot_title(char *nm, char *ct)
{
    const layer_t *l = &LAYERS[ui.layer % LAYER_N];
    str_cpy(nm, l->name, 12);
    ct[0] = 0;                                          /* (DUELL 1.1: no SET / HOLD / TOOLS word in the footer corner) */
}
static uint32_t layer_open(void) { return ui.ly && (ui.ly_t0 & LY_OPEN) && ly_down(ui.ly) && layer_allowed() ? ui.ly : 0u; }
static int layer_set_open(void)
{
    return layer_open() && LAYERS[layer_open()].kind == LK_SET;
}
static uint32_t layer_held(void) { return ui.ly && ly_down(ui.ly) && layer_allowed(); }
/* GLO's MIXER page shown (no menu, dialog or NAME over it): drawn by draw_layer, as the hold-GLO QUICK layer. 0.2.25 layout round (owner): the MIXER is GLO's first page again
 * (graph GR_GLO, scope SC_GMIX); the old RESTART of GLO held + PLAY on that page is not revived (layer_play) */
static int glo_pg(void) { return layer_allowed() && cur_page()->graph == GR_GLO; }

/* each pass, before the keys: what the ISR (seq.c keyboard_block) gives to a layer. Armed: its button; none
 * armed: any layer button not already down (one pressed meanwhile is armed below; one held over from before
 * is ignored, its keys stay notes); armed but dead (pressed under a dialog, a menu or NAME that has closed
 * since): nothing, its keys stay notes */
static void layer_masks(void)
{
    uint32_t m = !layer_allowed() || (ui.ly && (ui.ly_t0 & LY_DEAD)) ? 0u
               : ui.ly ? (ly_keys(ui.ly) ? ly_bit(ui.ly) : 0u) : layer_bits() & ~fm1_in.buttons;
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
        if (pressed & ly_bit(l)) {
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
    if (!(layer_seen & ly_seen(l))) {
        layer_seen |= (uint8_t)ly_seen(l);
        settings_save();                                /* (deferred while playing) */
    }
    if (lys.dt_hint && ui.msg_t && ui.msg[0] == 'H' && ui.msg[5] == '[') {   /* (0.2.9: the tap's "HOLD [..] QUICK" goes: the layer's */
        ui.msg_t = 0;                                   /* QUICK header shows at once) */
        lys.dt_hint = 0;
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
        lys.v[k] = l == LAYER_SEQ ? TSEL->p[P_SLEN + k] : 0;
    if (l == LAYER_EDIT)                                /* QUICK FILTER: its four as they are (OCT-); the ids too (cloud job H: they follow filter 2's state, which a MIDI message may change while it is held) */
        for (k = 0; k < 4u; k++) {
            lys.qid[k] = ly_page(l)->id[k];
            lys.v[4u + k] = ly_page(l)->id[k] == 0xFFu ? 0 : TSEL->p[ly_page(l)->id[k]];
        }
    lys.v[8] = song.g[G_BPM];
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
    lys.nv.page = ui.page;                              /* (#83: the page this tap leaves, for a double tap) */
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
    if (!(layer_seen & ly_seen(l)) && ui.msg_t == m && !name_on() && !ui.confirm && !ui.menu) {
        ui_say("HOLD [", KC[LY_KC[l]].label);
        str_cpy(ui.msg + str_len(ui.msg), "] QUICK", sizeof ui.msg - str_len(ui.msg));
        ui.msg_t = 90;                                  /* ~1.5 s */
        lys.dt_hint = 1;
    }
    if (!acted && !name_on() && !ui.confirm && !ui.menu && ly_lock(l)) {   /* a page opened: a second tap soon locks the layer (not a */
                                                    /* QUICK EDIT layer's: its second tap is the next page, as before) */
        lys.dt_l = (uint8_t)l;
        lys.dt_ms = fm1_ms;
    }
}

/* QUICK FILTER, OCT-: its four as the layer opened (1 = one of them changed). Not recorded (a put back, not a turn) */
static int quick_put_back(void)
{
    uint32_t k;
    int d = 0;
    for (k = 0; k < 4u; k++)
        if (lys.qid[k] != 0xFFu && TSEL->p[lys.qid[k]] != lys.v[4u + k]) {
            TSEL->p[lys.qid[k]] = lys.v[4u + k];
            d = 1;
        }
    return d;
}

/* SEQ TOOLS: white key place p's action on the selected track (ui_tools.c), "STOP TO EDIT" while a song plays */
static void tools_key(uint32_t p)
{
    const char *m;
    char n[4];
    if (!tl_cell(p))
        return;
    if (!(m = tl_do(TSEL, p)))
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

/* REC's settings, KNOB 1..3: CLICK, COUNT-IN, CLICK LEVEL as MENU > AUDIO steps them (ui.c rp_*) */
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
static const char W_NOTE[16] = {'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G'};

/* a key pressed in layer l (k: 0 = F3 .. 26 = G5) */
static void layer_key(uint32_t l, uint32_t k)
{
    uint32_t p = key_place(k);
    if (l == LAYER_SEQ) {                               /* (QUICK REC: the white keys no longer act, FIRMWARE_CHANGE_LIST_2026-10-28.md B.6) */
        if (!key_black(k))
            tools_key(p);
    }
}
/* each pass: the keys the layer got now */
static void layer_keys(uint32_t keys)
{
    uint32_t k, l = layer_open() ? layer_open() : ui.layer ? ui.layer : 0u;
    for (k = 0; keys && k < 27u; k++)
        if ((keys >> k) & 1u)
            layer_key(l, k);
}

/* KNOB 1..4 in the open layer: its own (REC's settings), else its four (ly_page: SEQ's, the QUICK EDIT layers'; 0.2.9: EDIT's
 * are CUT RES MODE REL, up to 0.2.8 ENG No. FAV), edited as on their page (edit_param: motion, MIDI LEARN) */
/* 0.2.10: OCT- held + a knob in a QUICK EDIT layer (EDIT, LFO, ENV, ARP): that card to its default, as on a page (ui_input.c knob_reset,
 * "RESET <CARD>"). The turn is not applied, the knob's further detents are ignored until it has been still (kr_on), and OCT- used so is a
 * modifier: its release neither puts back (QUICK FILTER, layer_oct) nor moves the octave (LFO ENV ARP, oct_rel: oct_mod). OCT- pressed
 * and let go with no knob turned keeps its meaning (QUICK FILTER: put back; the others: the octave). 1: the turn was taken here */
static int layer_quick_reset(uint32_t l, uint32_t k)
{
    uint32_t dn = (fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u, up = (fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u;
    uint8_t pg = ui.page;
    int r;
    if (kr_on[k]) {                                     /* a reset's knob still turning: ignored until it has been still */
        if (fm1_ms - kr_t[k] < KR_QUIET_MS) {
            kr_t[k] = fm1_ms;
            return 1;
        }
        kr_on[k] = 0;
    }
    if (ly_q(l) > 3u || !dn || up)
        return 0;
    ui.page = (uint8_t)page_first(LAYERS[l].fam);
    page_over = ly_page(l);
    r = knob_reset(k);
    page_over = 0;
    ui.page = pg;
    if (!r)
        return 0;
    oct_mod = 1;
    kr_on[k] = 1;
    kr_t[k] = fm1_ms;
    return 1;
}
static void rec_clear_ask(void);                       /* (QUICK REC, KNOB 4: below, with the LEDs' rec_clearable) */
static void layer_knob(uint32_t k, int32_t s)
{
    uint32_t l = layer_open();
    if (l && off_layer(l)) {                            /* item 125: QUICK ARP / QUICK SEQ with the module OFF: any knob closes the layer and jumps to MODULES */
        if (kr_on[k]) {
            if (fm1_ms - kr_t[k] < KR_QUIET_MS) {
                kr_t[k] = fm1_ms;
                return;
            }
            kr_on[k] = 0;
        }
        layer_let_go(1);
        off_knob(k, s);
        return;
    }
    if (l && layer_quick_reset(l, k))
        return;
    if (l == LAYER_REC) {                        /* CLICK, COUNT-IN, LEVEL; KNOB 4 CLEAR: the dialog */
        if (k < 3u)
            rec_set(k, (uint32_t)clamp((int32_t)rp_get(RL_F[k]) + s, 0, 2));
        else if (s)
            rec_clear_ask();
    } else if (l == LAYER_GLO) {                        /* QUICK GLO: K1 LEVEL 1, K2 LEVEL 2, K3 PAN 1, K4 PAN 2 (the MIXER page's own; item 117) */
        glo_knob(k, s);
    } else if (l) {                                     /* the page's knobs, wherever the page is */
        uint8_t pg = ui.page;
        ui.page = (uint8_t)page_first(LAYERS[l].fam);
        page_over = ly_page(l);
        edit_param(k, s);
        page_over = 0;
        ui.page = pg;
    }
}

/* QUICK GLO's and the MIXER's knobs (0.2.24; item 117, TEMPLATE 4: the order follows the screen, part 1 left and part 2 right on both rows): KNOB 1 LEVEL 1, KNOB 2 LEVEL 2, KNOB 3 PAN 1, KNOB 4 PAN 2 (recorded as
 * on MIXER; MIDI LEARN picks all four). PAN in DUEL: its card is not drawn (part 1 is left, part 2 right: ui_draw.c card_hidden_vp) and the knob does nothing. The ONE place the order lives: glo_knob,
 * glo_cards (below) and ui_input.c reset_desc (the quick reset) read it the same way: part = k & 1, parameter = k >> 1 (0 LEVEL, 1 PAN) */
static void glo_knob(uint32_t k, int32_t s)
{
    track_t *t = &trk[k & 1u];
    uint32_t id = (k >> 1) & 1u ? P_PAN : P_LEVEL;
    int16_t *vp = &t->p[id];
    if (id == P_PAN && duell_mode_req == DM_SPLIT)
        return;
    *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, TP[id].max - TP[id].min), TP[id].min, TP[id].max);
    motion_capture(t, id, *vp);
    ml_knob(t, id);                                     /* (MIDI LEARN: Tk LEVEL / Tk PAN picked) */
}
/* OCT- / OCT+ pressed while GLO is held (0.2.24, owner: the mutes left the knobs for the keys): OCT- flips part 1's MUTE, OCT+ part 2's, at once (P_MUTE, latched; MIDI LEARN picks it);
 * the hold becomes a combo (GLO's release is no tap) and the key's release moves no octave (ui_input.c: oct_pend is not set). 1 = taken here. The quick reset (OCT- + a knob) never
 * was QUICK GLO's (layer_quick_reset: the four quick EDIT layers only), so nothing is lost */
static int layer_glo_held(void) { return ui.ly == LAYER_GLO && ly_down(LAYER_GLO) && layer_allowed(); }
static int layer_glo_oct(uint32_t b)
{
    uint32_t k = b == B_OCTUP;
    int16_t m;
    if (ui.ly != LAYER_GLO || !ly_down(LAYER_GLO) || !layer_allowed())
        return 0;
    if (!(ui.ly_t0 & (LY_OPEN | LY_DEAD))) {            /* GLO down, not open yet: a key with it is a combo, it opens now */
        ui.ly_t0 |= LY_OPEN | LY_COMBO;
        layer_opened(LAYER_GLO);
    } else {
        ui.ly_t0 |= LY_COMBO;
    }
    m = (int16_t)!trk[k].p[P_MUTE];
    ml_knob(&trk[k], P_MUTE);
    trk[k].p[P_MUTE] = m;
    ui_say(k ? "PART 2: " : "PART 1: ", m ? "MUTE" : "UNMUTE");   /* (0.2.25: the action's word; no MUTED / LIVE state words) */
    ui.force = 1;
    return 1;
}

/* PLAY with GLO held open: RESTART (from the top, playing on); stopped: PLAY. 1 = done here. GLO's lock is not
 * held: PLAY starts and stops there as in every layer (#183: locked, PLAY restarted and never stopped) */
static int layer_play(void)
{
    return 0;                                           /* (0.2.25: PLAY is PLAY on the MIXER page too: the restart gesture of the old GLO page is not part of the MIXER's return) */
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
    if ((mine & 1u) && !oct_mod) {                      /* OCT-: back to when it opened (not after a knob reset with it: layer_quick_reset) */
        if (lys.l == LAYER_EDIT) {                      /* QUICK FILTER: its four */
            if (quick_put_back())
                ui_message("VALUES PUT BACK");
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
/* QUICK REC, KNOB 4 CLEAR turned: the "CLEAR Tn SEQUENCE?" dialog (CF_CLEAR_SEQ, the TOOLS page's CLEAR: OCT+ YES clears the selected track's steps and automation and
 * ARP's latched chord, one undo, SAVE held; OCT- NO). Nothing recorded: the cell says NO SEQUENCE and the turn says so. lys.loaded: OCT- in the layer afterwards puts the
 * track back as the layer opened (tl_open), whatever the dialog's answer was */
static void rec_clear_ask(void)
{
    if (!rec_clearable()) {
        ui_message("NOTHING TO CLEAR");
        return;
    }
    lys.loaded = 1;
    confirm_open(CF_CLEAR_SEQ, song.sel);
}
static uint32_t layer_leds(uint32_t *br)
{
    uint32_t k, m = 0, n = 0, l = ui.layer;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), b = (uint32_t)key_black(k), on = 0, can = 0;
        if (l == LAYER_SEQ) {                    /* the tools breathe (not while a song plays) */
            can = !b && tl_cell(p);
        }                                               /* (QUICK REC: the white keys no longer act, so they stay dark) */
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
    *ink = st == LS_DIM ? T_DIM : T_TEXT;                /* (PAGES_V2_perform-quick.png: an idle key is a neutral grey cell, white name; the held one lights) */
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
    return T_BG;
}
/* a cell at x, y, h px high: big (h > 30) the icon over the name; a name: compact, the icon at the right (none: no
 * icon); else a black key's: two icons at the right. tri: a HOLD cell in a SET layer */
/* a cell's box: its fill on the panel (cells are 4 px apart from x 6, y 4). Returns the fill */
static uint16_t lc_box(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t fill)
{
    { CV_HL(x, y, w, h); cv_rrect(x, y, w, h, 4, fill, T_BG); }
    return fill;
}
#include "ui_boxes.c"                                   /* DUELL 1.0 layout: the box grid of PERFORM and QUICK SEQ (box_grid; LAYOUT_IMPLEMENTATION_PLAN.md block D) */
/* GLO (DUELL 5b.3), two parts: a column each (88 px, x 6 and 146; part 1 is the left of KNOB 1 and 2, part 2 the right): one tall
 * cell with the part's badge and the word in M; between the columns (x 98 .. 142) the parts' level meters (glo_meters; a muted part's bar greyed
 * and empty). 0.2.25 layout round (owner): the word is always MUTE: muted, the cell is lit in the part's colour (the badge dark); sounding, the cell is
 * dark (RAISE, the word MID). No LIVE / MUTED / ON / OFF */
#define GC_W 88
#define GC_X(k) (6 + 140 * (int32_t)(k))
/* ------------------------------------------------ GLO's level meters --- */
/* One bar per part between the columns: the part's own level before MASTER, peak, -48 .. 0 dBFS (0 dBFS = 32768 in the mix's Q15; what
 * the part adds to the mix with its LEVEL, DIST, INSERT, EQ and DUCK applied, before PAN and the sends: fx.c mix_part's t->peak, the
 * MIXER's too; in SPLIT the bus of the part's side as well, with its delay and reverb return: fx.c meter_bus, scanned only while this page
 * shows). The audio path keeps a running maximum (no work added in STEREO and LAYER); here each frame takes it and clears it. The bar
 * follows the peak up at once and falls at 36 dB/s; a thin tick holds the highest peak 1.2 s and then falls the same way; the part's
 * colour up to -6 dBFS (the master's limiter starts at -5), the accent from -6 dBFS, red from -1 dBFS; a lamp over the bar stays red 1.5 s
 * after a peak past -1 dBFS (CLIP). Solid colours only (POLICE and every theme). Units: 16 per octave of amplitude (6 dB = 16 units / 2.67) */
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
static uint16_t gm_zone(int32_t u16, uint32_t k)       /* the colour for a level (a zone colour too like the part's own, as POLICE's red part 1: the text colour) */
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
    sig += ux.gen * 4099u + (uint32_t)T_BG * 131u;
    if (!ui.force && sig == gm.sig)
        return;
    gm.sig = sig;
    cv_begin(GM_W, GM_H, T_BG);
    cv_bg = T_BG;
    for (i = 0; i < 5u; i++) {                          /* the scale: the dB figures between the bars, a hair line each side */
        int32_t y = GM_BY + GM_BH - (DB[i] + 48) * GM_BH / 48;
        cv_text_in(12, y - AF_S_CAP_Y - AF_S_CAP_H / 2, 20, &AF_S, DBN[i], T_MID, T_BG);
    }
    for (k = 0; k < NSHOWN; k++) {
        int32_t x = k ? GM_W - 12 : 0, yb = GM_BY + GM_BH, z1 = GM_BH * (GM_UHOT - GM_U0) / 128, z2 = GM_BH * (GM_UCLIP - GM_U0) / 128, n;
        { CV_HL(x, GM_BY, 12, GM_BH); cv_rect(x, GM_BY, 12, GM_BH, T_RAISE);          /* the track */ }
        n = px[k] < z1 ? px[k] : z1;                    /* the part's colour, then the accent, then red */
        if (n > 0)
            { CV_HL(x, yb - n, 12, n); cv_rect(x, yb - n, 12, n, glo_sounding(k) ? T_PTHEME(k) : T_DIM); }
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
/* DUELL 1.1 MIXER, TEMPLATE 4 (item 117; FINAL_V1_09_mixer, ui_tokens.h UT_T4_*): two MUTE cards (round, 116 x 60: the word MUTE n, the hint "Hold GLO" / "and press OCT -" ("OCT +") with its keycaps standing on the text's
 * baselines, the part's small level meter at the right), the four cards under them (grid_on 7, ui_draw.c card_t4_paint). A muted part's card is filled in its colour (ink text) as before */
static void glo_mute_cards(void)
{
    uint32_t e;
    for (e = 0; e < NSHOWN; e++) {
        uint32_t mu = trk[e].p[P_MUTE] != 0, kc = e ? KC_OCTUP : KC_OCTDN;
        int32_t x = UT_T4_X0 + UT_T4_DX * (int32_t)e, y = UT_T4_MUTE_Y - Y_GRAPH, kx;
        uint16_t fill = mu ? T_PTHEME(e) : T_BG, ink = mu ? T_INK : T_TEXT, grey = mu ? T_INK : ux_mix(T_BASE_P1, T_TEXT, UT_GREY_PCT);
        char lb[8] = "MUTE 1";
        lb[5] = (char)('1' + e);
        { CV_HL(x, y, UT_T4_W, UT_T4_MUTE_H); cv_rrect(x, y, UT_T4_W, UT_T4_MUTE_H, UT_T4_R, fill, T_BG); }
        cv_text_on(x + UT_T4_PAD, y + UT_TY(S, UT_T4_LBL_BASE), &AF_S, lb, grey, fill);
        kx = cv_text_on(x + UT_T4_PAD, y + UT_TY(M, UT_T4_HOLD_BASE), &AF_M, "Hold", ink, fill) + UT_T4_KEY_GAP;
        cv_keycap(kx, y + UT_T4_HOLD_BASE - KC_H + 2, KC_GLO, T_KEY, T_KINK, fill);
        kx = cv_text_on(x + UT_T4_PAD, y + UT_TY(M, UT_T4_PRESS_BASE), &AF_M, "and press", ink, fill) + UT_T4_KEY_GAP;
        cv_keycap(kx, y + UT_T4_PRESS_BASE - KC_H + 2, kc, T_KEY, T_KINK, fill);
    }
}
/* the two level meters (own strips, each frame when a level moved): a 6 x 44 bar at the MUTE card's right, the 48 dB range over its height, the level in the part's colour from the bottom, the peak hold a 2 px tick */
static void glo_meters7(void)
{
    uint32_t k;
    uint32_t sig = 0x7D5u;
    int32_t px[NSHOWN], hp[NSHOWN];
    for (k = 0; k < NSHOWN; k++) {
        px[k] = gm_px(gm.lvl[k]) * UT_T4_METER_H / GM_BH;
        hp[k] = gm_px(gm.hold[k]) * UT_T4_METER_H / GM_BH;
        sig = (sig * 33u + (uint32_t)px[k]) * 33u + (uint32_t)hp[k] + glo_sounding(k) * 3u + (uint32_t)trk[k].p[P_MUTE] * 5u;
    }
    sig += ux.gen * 4099u;
    if (!ui.force && sig == gm.sig)
        return;
    gm.sig = sig;
    for (k = 0; k < NSHOWN; k++) {
        uint16_t under = trk[k].p[P_MUTE] ? T_PTHEME(k) : T_BG, track = trk[k].p[P_MUTE] ? T_RAISE : ux_mix(T_BASE_P1, T_TEXT, UT_T4_TRACK_PCT);
        cv_begin(UT_T4_METER_W, UT_T4_METER_H, under);
        cv_rrect(0, 0, UT_T4_METER_W, UT_T4_METER_H, UT_T4_METER_R, track, under);
        if (px[k] > 0)
            cv_rrect(0, UT_T4_METER_H - px[k], UT_T4_METER_W, px[k], px[k] < 2 * UT_T4_METER_R ? px[k] / 2 : UT_T4_METER_R, glo_sounding(k) ? T_PTHEME(k) : T_DIM, track);
        if (hp[k] > 0)
            cv_rect(0, UT_T4_METER_H - 1 - hp[k], UT_T4_METER_W, 2, T_TEXT);
        cv_blit((uint32_t)(UT_T4_X0 + UT_T4_DX * (int32_t)k + UT_T4_METER_X), (uint32_t)(UT_T4_MUTE_Y + UT_T4_METER_Y));
    }
}
static void layer_glo(void)
{
    uint32_t e;
    for (e = 0; e < NSHOWN; e++) {
        uint32_t mu = trk[e].p[P_MUTE] != 0, kc = e ? KC_OCTUP : KC_OCTDN;
        int32_t x = GC_X(e), y = 4 + 112 - KC_H - 6;
        uint16_t fill = mu ? T_PTHEME(e) : T_BG;
        fill = lc_box(x, 4, GC_W, 112, fill);
        cv_part(x + 8, 4 + HALF_UP(112 - 20), 20, e, !mu, fill);   /* the part's badge (20 px) and MUTE, M, on the cell's middle */
        GFX_HOOK_ALIGN(0, 4, 0, 4 + 112, AL_V, "glo cell word centred up/down");
        cv_text_on(x + 32, 4 + CAP_IN(M, 112), &AF_M, "MUTE", mu ? T_INK : T_MID, fill);
        /* the keys that flip it, at the cell's foot: hold GLO (the layer) and OCT- for part 1, OCT+ for part 2; on the MIXER page GLO's keycap says it is held */
        if (ui.layer == LAYER_GLO) {
            GFX_HOOK_ALIGN(x, 0, x + GC_W, 0, AL_H, "glo cell key centred across");
            cv_keycap(x + HALF_UP(GC_W - kc_w(kc)), y, kc, T_KEY, T_KINK, fill);
        } else {
            int32_t w = kc_w(KC_GLO) + 4 + text_w(&AF_S, "+") + 4 + kc_w(kc), kx = x + HALF_UP(GC_W - w);
            kx = cv_keycap(kx, y, KC_GLO, T_KEY, T_KINK, fill) + 4;
            GFX_HOOK_ALIGN(0, y, 0, y + KC_H, AL_V | AL_PASS, "glo cell + on its keycaps' line");
            kx = cv_text_on(kx, y - 1, &AF_S, "+", mu ? T_INK : T_MID, fill) + 4;
            cv_keycap(kx, y, kc, T_KEY, T_KINK, fill);
        }
    }
}
/* Item 120: the four QUICK EDIT layers are template 1 (grid_on 1: ui_draw.c ui_draw_page `want`): the picture card (76 rows) holds one line a group, the numbers of the group's knobs in dots and its name; the four cards are
 * template 1's. DUELL 1.0 layout: QUICK SEQ is an area page (grid_on 16: the tools' box grid, layer_seq), QUICK REC four cells (grid_on 14, layer_cards).
 * DUELL 0.2.9, the QUICK EDIT layers' panel (before item 120): a cell over each group of their cards, its name (M) on its middle, so the cards that share
 * a label (LFO 1's FLT and LFO 2's) are told apart: QUICK FILTER: FILTER (K1..K3), AMP (K4); QUICK LFO: LFO 1, LFO 2; QUICK ENV: AMP ENV,
 * ENV 2; QUICK ARP: ARP. The cells line up with the cards (CARD_X), 3 px in */
static const char *const LQ_NAME[4][2] = {{"MULTI FILTER", "AMP"}, {"LFO 1", "LFO 2"}, {"AMP ENV", "FILTER ENV"}, {"ARP", 0}};
static const uint8_t LQ_SPLIT[4] = {3, 2, 2, 4};       /* the first card of the second group */
static void layer_quick(uint32_t q, int32_t y, int32_t h)
{
    uint32_t g, split = LQ_SPLIT[q];
    const char *const *nm = LQ_NAME[q];
    static const char *const TWO[2] = {"MULTI FILTER", "TWIN FILTER"};
    if (q == 0u && ly_two()) {                          /* (cloud job H: MULTI CUTOFF, MULTI RES | TWIN CUTOFF, TWIN RES) */
        nm = TWO;
        split = 2;
    }
    if (grid_on == 1) {                                 /* item 120, template 1 (PAGES_V2_perform-quick.png, the 76 px card of the part pages): a line a group, the numbers of its knobs in dots, then its name (L, M where L is too wide) */
        uint32_t ng = nm[1] ? 2u : 1u;
        for (g = 0; g < ng; g++) {
            uint32_t k0 = g ? split : 0u, k1 = g ? 4u : split, k;
            int32_t base = ng == 1u ? 46 : g ? 63 : 31, x = 9;
            const aafont_t *f = &AF_L;
            for (k = k0; k < k1; k++, x += AF_S_CAP_H + 2) {
                char dg[2] = {(char)('1' + k), 0};
                int32_t dy = base - 13;
                cv_rrect(x, dy, AF_S_CAP_H, AF_S_CAP_H, AF_S_CAP_H / 2, T_MID, T_BG);
                cv_text_in(x, dy + HALF_UP(AF_S_CAP_H - AF_D_CAP_H) - AF_D_CAP_Y, AF_S_CAP_H, &AF_DOT, dg, T_BG, T_MID);
            }
            x += 6;
            if (x + text_w(f, nm[g]) > 232)
                f = &AF_M;
            cv_text_on(x, f == &AF_L ? UT_TY(L, base) : UT_TY(M, base), f, nm[g], T_TEXT, T_BG);
        }
        (void)y;
        (void)h;
        return;
    }
    for (g = 0; g < 2u && nm[g]; g++) {
        int32_t x0 = CARD_X(g ? split : 0u) + 3, x1 = CARD_X((g ? 4u : split) - 1u) + CARD_W - 3;
        uint16_t ink, fill = lc_box(x0, y, x1 - x0, h, lc_fill(LS_OFF, &ink));
        GFX_HOOK_ALIGN(x0, y, x1, y + h, AL_HV, "quick layer group name centred");
        cv_text_in(x0, y + CAP_IN(M, h), x1 - x0, &AF_M, nm[g], ink, fill);
    }
}
/* SEQ TOOLS: the white keys' tools, 4 a row from F3 (the sequence's: CLEAR REVERSE SHIFT < SHIFT >, RANDOM COOK TRANS + TRANS -) */
static const char *const TL_NAME[TL_N] = {"CLEAR", "REVERSE", "SHIFT <", "SHIFT >", "RANDOM", "COOK", "TRANS +", "TRANS -"};
#define TL_BIG ((1u << TL_CLEAR) | (1u << TL_COOK))      /* QUICK_SEQ_NEW_LAYOUT.png: CLEAR and COOK in the larger size, the rest in the smaller */
/* DUELL 1.0 layout (QUICK_SEQ_NEW_LAYOUT.png): the eight tools as a 4 x 2 box grid in the big area (ui_boxes.c box_grid; rows UT_BOX_QS_*), the white
 * keys F G A B over C D E F, the key letter in the corner, the names centred */
static void layer_seq(void)
{
    box_t b[8];
    uint32_t p;
    for (p = 0; p < 8u; p++) {
        b[p].name = p < TL_N ? TL_NAME[p] : "";
        b[p].key = W_NOTE[p];
        b[p].big = (uint8_t)((TL_BIG >> p) & 1u);
        b[p].state = LS_OFF;
    }
    box_grid(UT_BOX_QS_Y0, 2u, 4u, UT_BOX_QS_DY, UT_BOX_QS_H, b);
}

/* QUICK GLO's and the MIXER's four cards (item 117, TEMPLATE 4): KNOB 1 LEVEL 1, KNOB 2 LEVEL 2, KNOB 3 PAN 1, KNOB 4 PAN 2 (the part's colour; DIM when it is muted; PAN not drawn in DUEL; PAN shows its own signed
 * number, no % sign, and its bar fills from the middle) */
static void glo_cards(void)
{
    char val[PV_LEN];
    const char *unit;
    uint32_t c;
    for (c = 0; c < 4u; c++) {
        uint32_t k = c & 1u, id = (c >> 1) & 1u ? P_PAN : P_LEVEL;
        if (id == P_PAN && duell_mode_req == DM_SPLIT) {
            draw_column_hide(c);
            continue;
        }
        param_format(&TP[id], trk[k].p[id], val, &unit);
        if (id == P_PAN) {
            unit = "";
            card_ctr_next = 1;
        }
        draw_column(c, id == P_PAN ? (k ? "PAN 2" : "PAN 1") : (k ? "LEVEL 2" : "LEVEL 1"), val, unit, glo_sounding(k) ? T_PTHEME(k) : T_DIM,
                    RATIO(&TP[id], trk[k].p[id]), id == P_PAN ? ICON_AUTO : trk_icon(k, 1));
    }
}
static void layer_cards(uint32_t l)
{
    uint32_t c;
    if (l == LAYER_GLO) {
        glo_cards();
    } else if (l == LAYER_REC) {                        /* DUELL 1.0 layout (QUICK_REC_NEW_LAYOUT.png): four cells CLICK, COUNT, LEVEL (choices of three: the segment line), CLEAR */
        for (c = 0; c < 3u; c++) {
            uint32_t v = rp_get(RL_F[c]);
            cell_seg_n = 3;                             /* (template 8: the CHOICE this card shows, for this card only) */
            cell_seg_i = (uint8_t)(v % 3u);
            draw_column(c, RL_LABEL[c], rl_name(c, v), "", VAL(c), -1, RL_ICON[c]);
        }
        cell_seg_n = cell_seg_i = 0;                    /* CLEAR: a text, no line */
        draw_column(3, "CLEAR", rec_clearable() ? "TURN TO CLEAR" : "NO SEQUENCE", "", rec_clearable() ? VAL(3u) : T_DIM, -1, ICON_NONE);
    } else {                                            /* the layer's four (ly_page: SEQ's, the QUICK EDIT layers') */
        uint8_t pg = ui.page;
        ui.page = (uint8_t)page_first(LAYERS[l].fam);
        page_over = ly_page(l);
        draw_columns();
        page_over = 0;
        ui.page = pg;
    }
}

static void draw_layer(void)
{
    uint32_t l = ui.layer ? ui.layer % LAYER_N : LAYER_GLO, sig = l * 7919u + ux.gen * 977u;   /* (no layer: GLO's page) */
    if (off_layer(l) && grid_on == 13) {                /* item 125: QUICK ARP / QUICK SEQ with the module OFF: only the notice */
        if (ui.force || sig != ui.layer_sig) {
            ui.layer_sig = sig;
            graph_off(off_layer(l));
        }
        return;
    }
    layer_cards(l);
    sig += vals_hash(TSEL) * 2246822519u;               /* (DUELL 1.1 general rule, ui_graph.c vals_hash: every value the panel could show) */
    if (l == LAYER_GLO)
        sig += (uint32_t)(trk[0].p[P_MUTE] | trk[1].p[P_MUTE] << 1) * 131u + (duell_mode_req == DM_SPLIT) * 977u;
    else if (l == LAYER_REC)
        sig += (uint32_t)ui_rec_prefs * 31u + (uint32_t)rec_clearable() * 5u + song.sel * 977u;
    else if (l == LAYER_SEQ)
        sig += song.sel * 977u;
    else if (l == LAYER_EDIT)
        sig += (TSEL->eng_req % NENGINES) * 31u + song.sel * 977u + (uint32_t)ly_page(l)->id[2] * 4099u + (uint32_t)ly_page(l)->id[3] * 65537u;   /* (cloud job H: its cards follow filter 2) */
    if (l == LAYER_REC && grid_on == 14)                /* QUICK REC (template 8): the cells are the whole body, no panel of its own */
        return;
    if (ui.force || sig != ui.layer_sig) {
        ui.layer_sig = sig;
        CV_BANDS(240, H_GRAPH, T_BG) {                 /* (in bands: gfx.c CV_BANDS) */
            cv_bg = T_BG;
            if (l == LAYER_GLO && grid_on == 7)
                glo_mute_cards();
            else if (l == LAYER_GLO)
                layer_glo();
            else if (l == LAYER_SEQ)
                layer_seq();
            else if (l == LAYER_REC)
                ;                                           /* (QUICK REC outside template 8, PERFORM on: an empty panel; its four are the cards) */
            else
                layer_quick(ly_q(l), 4, 112);              /* (QUICK FILTER, LFO, ENV, ARP) */
            cv_blit(0, Y_GRAPH + (uint32_t)cv_by);
        }
        if (l == LAYER_GLO)
            gm.sig = 0;                                 /* (the strip over the panel just drawn) */
    }
    if (l == LAYER_GLO) {                               /* the level meters: their own strip, each frame */
        glo_meter_step();
        if (grid_on == 7)
            glo_meters7();
        else
            glo_meters();
    }
}

#include "ui_perform.c"                                 /* DUELL 0.2.5: the PERFORM mode (FX held; its page, knobs, keys) */
#include "ui_guide.c"                                   /* DUELL 0.2.11: the first-start GUIDE (after the splash; MENU > SYSTEM > GUIDE) */
