/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MIDI LEARN (1.5, Discussion #170) and the MIDI MAP's edits (docs/design/MIDI_MAP_DESIGN_2.md): a controller's CC set to any knob's
 * parameter, on the device. GLO > MIDI LEARN KNOB 1 (DUELL step 6, ui_input.c midi_knob; up to step 5 GLO held + D4) turns it on and off.
 * On: a knob turned on any page (HOME, the sound's pages, MIXER: the parameter of a track it edits) picks its parameter on the selected
 * track, as it edits it; the next CC that comes (midi_control.c midi_map_cc: any channel ROUT hears) is written to the MIDI MAP as a line
 * of that parameter, and stays until something else is learned on it. Then turn the next knob. OCT- clears the picked parameter's CC,
 * OCT+ (or GLO + D4 again) is done. The footer's first row says what is picked and its CC (ml_line).
 * THE MAP (midi_control.c midi_map, midi_map.h): one line per CC number. One CC moves one parameter and one parameter has one CC:
 * a number that moves something else is NOT taken (the learn refuses and says whose it is, "CC74 IS CUTOFF"; map_set's force takes it:
 * the editor, a later page's yes); a parameter learned on another number moves there. At most ML_N lines are LEARNED (a learned line is
 * one track's: midi_map.h), typed lines (MAP_SET, the editor) have no cap. The 23 locked numbers are never assigned (MAP_LOCKED_CC).
 * Clearing a learned line gives its number back to the shipped list's line, if it had one and its parameter has no other number: what
 * the number did before the learn (a forced learn over a shipped line is undone by it). A line the user clears or moves away
 * (map_clear, a typed or shipped line moved) is marked MAP_CLEARED: a later firmware's shipped list does not fill it again.
 * The editor reads and edits the learned lines (editor_learn.c LEARN_GET / LEARN_SET) and the whole map (editor_map.c). The table is
 * kept with the settings (settings_persist.c). Its state: ui.ml (ui.c) */
#define ML_N MAP_LEARN_MAX
static volatile uint8_t *const ml_io = ui.ml.io;         /* (midi_control.c ml_arm, ml_heard) */

/* --- the lines ---------------------------------------------------------------------------------------------------------- */
/* the part a line is of, as a key: 0 the channel's (or global), 1..4 track 0..3 (a typed PART1 / PART2 is track 0 / 1; a learned line is pinned) */
static uint32_t map_pk(uint32_t l)
{
    uint32_t r = MAP_RULE(l);
    if (MAP_WHO(l) == MAP_W_LEARNED)
        return r + 1u;
    return r == MAP_R_PART1 ? 1u : r == MAP_R_PART2 ? 2u : 0u;
}
static void map_put(uint32_t cc, uint32_t l)             /* (the ISR reads the two bytes: never half of one) */
{
    (void)map_line(cc);                                  /* (the shipped list first, if the table was never loaded) */
    fm1_irq_off();
    midi_map[cc] = (uint16_t)l;
    fm1_irq_on();
}
static uint32_t map_default(uint32_t cc) { return cc < MAP_N ? MIDI_MAP_DEFAULTS[cc] : MAP_EMPTY; }
/* the number that moves parameter (code, key pk) except `skip`; MAP_N none */
static uint32_t map_find(uint32_t code, uint32_t pk, uint32_t skip)
{
    uint32_t cc, l;
    for (cc = 0; cc < MAP_N; cc++)
        if (cc != skip && !MAP_IS_EMPTY(l = map_line(cc)) && !MAP_IS_LOCKED(l) && MAP_TARGET(l) == code && map_pk(l) == pk)
            return cc;
    return MAP_N;
}
static uint32_t ml_count(void)
{
    uint32_t cc, n = 0;
    for (cc = 0; cc < MAP_N; cc++)
        n += MAP_WHO(map_line(cc)) == MAP_W_LEARNED && !MAP_IS_EMPTY(map_line(cc));
    return n;
}
/* the number of the n-th learned line (by CC), MAP_N none; LEARN_GET lists them so */
static uint32_t ml_nth(uint32_t n)
{
    uint32_t cc;
    for (cc = 0; cc < MAP_N; cc++)
        if (MAP_WHO(map_line(cc)) == MAP_W_LEARNED && !MAP_IS_EMPTY(map_line(cc)) && !n--)
            return cc;
    return MAP_N;
}
/* the CC of parameter id on track k that is pinned to that track (learned, or typed on part 1 / 2), + 1; 0 none */
static uint32_t ml_cc_of(uint32_t k, uint32_t id)
{
    uint32_t cc = map_find(id, k + 1u, MAP_N);
    return cc < MAP_N ? cc + 1u : 0u;
}
/* a line that does nothing any more (its number is free; with a shipped line there, or its parameter's shipped number: not refilled) */
static void map_unmap(uint32_t cc, uint32_t restore)
{
    uint32_t d = map_default(cc);
    if (restore && !MAP_IS_EMPTY(d) && !MAP_IS_LOCKED(d) && map_find(MAP_TARGET(d), map_pk(d), cc) >= MAP_N) {
        map_put(cc, d);                                  /* the number is the shipped line's again */
        return;
    }
    map_put(cc, MAP_EMPTY | (MAP_IS_EMPTY(d) ? 0u : MAP_CLEARED));
}

/* --- the names ---------------------------------------------------------------------------------------------------------- */
static const char *const MAP_ACT_NAME[MAP_T_RES_SMART - MAP_T_MORPH + 1u] = {
    "MORPH", "ROLL A", "ROLL B", "UNDO", "ROLL", "MORPH AMOUNT", "MORPH TARGET", "MORPH SMART", "MORPH ARP", "MORPH PITCH", "MODE", "BALANCE", "XFADE",
    "SPLIT NOTE", "CUTOFF", "RESONANCE"};
static const char *const MAP_X_NAME[NX] = {"ROUTE", "LINK", "TWIN KEY TRK", "ENV TO TWIN CUTOFF", "LFO 1 TO TWIN CUTOFF", "LFO 2 TO TWIN CUTOFF", "SUB LEVEL",
                                           "SUB OCTAVE", "SUB WAVE", "SUB HARMONICS", "CYCLE MODE", "CYCLE RISE", "CYCLE FALL", "CYCLE CURVE",
                                           "CYCLE SYNC", "-"};
/* the name of line l's parameter (b holds 24): "CUTOFF", "MORPH", "T2 ATTACK" (a learned line leads with its track) */
static void map_name(uint32_t l, char *b)
{
    uint32_t code = MAP_TARGET(l), k = MAP_WHO(l) == MAP_W_LEARNED ? MAP_RULE(l) : MAP_RULE(l) == MAP_R_PART1 ? 0u : MAP_RULE(l) == MAP_R_PART2 ? 1u : trk_index(TSEL);
    const char *s = 0;
    b[0] = 0;
    if (MAP_WHO(l) == MAP_W_LEARNED) {
        b[0] = 'T';
        b[1] = (char)('1' + k);
        b[2] = ' ';
        b[3] = 0;
    }
    if (code < P_COUNT) {
        s = code == P_TWTYPE ? "TWIN TYPE" : code == P_CUT2 ? "TWIN CUTOFF" : code == P_RES2 ? "TWIN RES" : code == P_DRV2 ? "DRIVE" : code == P_BLEND ? "BLEND" : 0;
        if (s)
            str_cpy(b + str_len(b), s, 24u - str_len(b));
        else
            ev_name(&trk[k % NTRK], code, b + str_len(b));
    } else if (code >= XW_BASE && code < XW_BASE + NX) {
        str_cpy(b + str_len(b), MAP_X_NAME[code - XW_BASE], 24u - str_len(b));
    } else if (code >= MAP_T_MORPH && code <= MAP_T_RES_SMART) {
        str_cpy(b + str_len(b), MAP_ACT_NAME[code - MAP_T_MORPH], 24u - str_len(b));
    } else if (code >= MAP_T_G0 && code < MAP_T_G0 + G_COUNT) {
        desc_said(&GP[code - MAP_T_G0], b + str_len(b), 24u - str_len(b));
    } else {
        str_cpy(b + str_len(b), "-", 24u - str_len(b));
    }
}

/* --- what a target is (for the unit's page and the editor) ---------------------------------------------------------- */
/* 1: a target code the table may hold (a part value, an extension slot, an action, a global value) */
static int map_code_ok(uint32_t code)
{
    return code < P_COUNT || (code >= XW_BASE && code < XW_BASE + NX - 1u) || (code >= MAP_T_MORPH && code <= MAP_T_RES_SMART) ||
           (code >= MAP_T_G0 && code < MAP_T_G0 + G_COUNT);
}
/* the part rules the target may have: the generator's table (tools/gen_midi_map.py) */
static int map_rule_ok(uint32_t code, uint32_t rule)
{
    if ((code >= MAP_T_MODE && code <= MAP_T_SPLIT) || code >= MAP_T_G0)
        return rule == MAP_R_GLOBAL;
    if (code >= MAP_T_TARGET && code <= MAP_T_PITCH)
        return rule == MAP_R_CHANNEL || rule == MAP_R_GLOBAL;
    return rule != MAP_R_GLOBAL;
}
/* kind, where it is saved, whether AUTOMATION records a CC move of it, whether it does nothing now (for part t, the editor's selected one).
 * A CC on an extension slot is not recorded until the motion escape (id 127) is built */
static void map_target_info(uint32_t code, const track_t *t, map_info_t *r)
{
    r->kind = MAP_K_CONTINUOUS;
    r->saved = MAP_S_PATCH;
    r->automation = r->inert = 0;
    if (code < P_COUNT) {
        const param_desc_t *d = track_desc(t, code);
        r->kind = d->max - d->min == 1 ? MAP_K_SWITCH : d->fmt == F_ENUM ? MAP_K_STEPPED : MAP_K_CONTINUOUS;
        r->saved = code == P_LEVEL || code == P_PAN || code == P_MUTE ? MAP_S_MULTI : MAP_S_PATCH;
        r->automation = (uint8_t)motion_param(code);
        r->inert = d->max <= d->min || !d->label || d->label[0] == '-' || code == P_DKSRC ||
                   ((code == P_IA || code == P_IB || code == P_IC || code == P_IMIX) && !t->p[P_ITYPE]);
        if (code == P_TWTYPE || code == P_CUT2 || code == P_RES2 || code == P_DRV2 || code == P_BLEND) {   /* (ext.h aliases: the TWIN filter) */
            r->kind = code == P_TWTYPE ? MAP_K_STEPPED : MAP_K_CONTINUOUS;
            r->inert = (code != P_TWTYPE && !t->p[P_TWTYPE]) || (code == P_BLEND && !XV(t, X_ROUTE));
        }
    } else if (code >= XW_BASE && code < XW_BASE + NX) {
        uint32_t k = code - XW_BASE;
        r->kind = k == X_ROUTE || k == X_LINK || k == X_CYMODE || k == X_CYSYNC ? MAP_K_SWITCH : k == X_SUBOCT || k == X_SUBWAVE ? MAP_K_STEPPED : MAP_K_CONTINUOUS;
        r->inert = k >= NX - 1u || (k <= X_LFO2 && !t->p[P_TWTYPE]) || (k >= X_SUBOCT && k <= X_SUBHARM && !XV(t, X_SUBLVL));
    } else if (code >= MAP_T_G0 && code < MAP_T_G0 + G_COUNT) {
        const param_desc_t *d = &GP[code - MAP_T_G0];
        r->kind = d->fmt == F_ENUM ? MAP_K_STEPPED : MAP_K_CONTINUOUS;
        r->saved = MAP_S_MULTI;
    } else {
        r->saved = code >= MAP_T_TARGET && code <= MAP_T_PITCH ? MAP_S_DEVICE : MAP_S_MULTI;
        r->kind = code == MAP_T_ROLL_A || code == MAP_T_ROLL_B || code == MAP_T_UNDO || code == MAP_T_ROLL ? MAP_K_ACTION :
                  code == MAP_T_TARGET || code == MAP_T_MODE ? MAP_K_STEPPED : code >= MAP_T_SMART && code <= MAP_T_PITCH ? MAP_K_SWITCH : MAP_K_CONTINUOUS;
        if (code == MAP_T_CUTOFF_SMART || code == MAP_T_RES_SMART) {
            r->saved = MAP_S_PATCH;
            r->automation = 1;
            r->inert = !(MIDI_CC_ENG[eng_idx(t->eng_req)] >> (code == MAP_T_CUTOFF_SMART ? 4 : 0) & 15u);
        } else if (code == MAP_T_XFADE || code == MAP_T_SPLIT) {
            r->inert = duell_mode != DM_LAYER;       /* (LAYER's only: the MODE page) */
        }
    }
}

/* --- editing the map ---------------------------------------------------------------------------------------------------- */
/* line (code, rule) of kind who on cc. who TYPED: rule is a part rule; who LEARNED: rule is the track 0..3 (a part value or an extension
 * slot only). The number must be free (empty, or already this parameter's) unless force; a refusal fills name (24 bytes, may be 0) with
 * the owner's. The parameter's other number, if any, is given up. MAP_OK, MAP_E_LOCKED (a standard number), MAP_E_CONFLICT (the number
 * is another's), MAP_E_BAD, MAP_E_FULL (MAP_LEARN_MAX learned lines already). Main loop only */
static uint32_t map_set(uint32_t cc, uint32_t code, uint32_t rule, uint32_t who, uint32_t force, char *name)
{
    uint32_t l, cur, j, pk, n, cc2;
    if (cc >= MAP_N || (who != MAP_W_TYPED && who != MAP_W_LEARNED) || !map_code_ok(code) || rule > 3u)
        return MAP_E_BAD;
    if (who == MAP_W_LEARNED ? (rule >= NTRK || code >= MAP_T_MORPH) : !map_rule_ok(code, rule))
        return MAP_E_BAD;
    if (MAP_LOCKED_CC(cc))
        return MAP_E_LOCKED;
    l = MAP_LINE(code, rule, who);
    pk = map_pk(l);
    cur = map_line(cc);
    if (!MAP_IS_EMPTY(cur) && !(MAP_TARGET(cur) == code && map_pk(cur) == pk) && !force) {
        if (name)
            map_name(cur, name);
        return MAP_E_CONFLICT;
    }
    j = map_find(code, pk, cc);
    if (who == MAP_W_LEARNED) {
        for (n = cc2 = 0; cc2 < MAP_N; cc2++)
            n += cc2 != cc && cc2 != j && MAP_WHO(map_line(cc2)) == MAP_W_LEARNED && !MAP_IS_EMPTY(map_line(cc2));
        if (n >= ML_N)
            return MAP_E_FULL;
    }
    if (j < MAP_N)
        map_unmap(j, MAP_WHO(map_line(j)) == MAP_W_LEARNED);
    map_put(cc, l);
    return MAP_OK;
}
/* the number cc has no line any more (the user's choice: not filled again by a later firmware's shipped list); MAP_E_LOCKED for a standard number */
static uint32_t map_clear(uint32_t cc)
{
    uint32_t l;
    if (cc >= MAP_N)
        return MAP_E_BAD;
    if (MAP_LOCKED_CC(cc))
        return MAP_E_LOCKED;
    l = map_line(cc);
    if (!MAP_IS_EMPTY(l))
        map_put(cc, MAP_EMPTY | MAP_CLEARED);
    return MAP_OK;
}
/* RESET MAP: exactly the shipped list, nothing learned, nothing typed, no line marked cleared */
static void map_reset(void)
{
    uint32_t cc;
    for (cc = 0; cc < MAP_N; cc++)
        map_put(cc, MIDI_MAP_DEFAULTS[cc]);
}
/* the table as it must be: the 23 locked numbers locked and nothing else, every target one the firmware knows, one number per parameter (the
 * lowest), at most ML_N learned lines. Applies to a stored map (settings_persist.c) */
static void map_sanitize(void)
{
    uint32_t cc, l, n = 0;
    for (cc = 0; cc < MAP_N; cc++) {
        l = midi_map[cc];
        if (MAP_LOCKED_CC(cc)) {
            l = MAP_LINE(MAP_T_LOCKED, 0, MAP_W_LOCKED);
        } else if (MAP_WHO(l) == MAP_W_LOCKED || !(map_code_ok(MAP_TARGET(l)) &&
                   (MAP_WHO(l) == MAP_W_LEARNED ? MAP_RULE(l) < NTRK && MAP_TARGET(l) < MAP_T_MORPH : map_rule_ok(MAP_TARGET(l), MAP_RULE(l))))) {
            l = MAP_EMPTY | (l & MAP_CLEARED);
        } else if (map_find(MAP_TARGET(l), map_pk(l), cc) < cc) {
            l = MAP_EMPTY | MAP_CLEARED;
        }
        if (MAP_WHO(l) == MAP_W_LEARNED && !MAP_IS_EMPTY(l) && ++n > ML_N)
            l = MAP_EMPTY | MAP_CLEARED;
        midi_map[cc] = (uint16_t)l;
    }
    midi_map_ready = 1;
}
/* a later firmware's shipped list: a shipped line fills a number that is empty and was never cleared, if its parameter has no number */
static void map_merge_new(void)
{
    uint32_t cc, d;
    for (cc = 0; cc < MAP_N; cc++)
        if (MAP_IS_EMPTY(midi_map[cc]) && !(midi_map[cc] & MAP_CLEARED) && !MAP_IS_EMPTY(d = map_default(cc)) && !MAP_IS_LOCKED(d) &&
            map_find(MAP_TARGET(d), map_pk(d), cc) >= MAP_N)
            midi_map[cc] = (uint16_t)d;
}

/* --- the learn page's calls ------------------------------------------------------------------------------------------------ */
static uint32_t ml_row_code, ml_row_rule;                /* the editor's MAP_ARM: the row whose CC the next one heard becomes (while ml_row_on) */
static uint8_t ml_row_on;
static uint8_t ml_conflict_cc;                           /* the CC the last learn refused + 1, for the page's "take it?" (0 none) */
static char ml_conflict_txt[24];                         /* .. and the footer's first row while it stands: "CC74 IS CUTOFF" */
/* cc -> parameter id of track k, in place of what the parameter had; force takes a number that is another's.
 * 1 learned, 0 no room (ML_N others), 2 a CC never learned (or a parameter that is none), 3 the number is another's (refused) */
static uint32_t ml_learn_f(uint32_t cc, uint32_t k, uint32_t id, uint32_t force, char *name)
{
    uint32_t r;
    if (k >= NTRK || !(id < P_COUNT || (id >= XW_BASE && id < XW_BASE + NX - 1u)))
        return 2;
    r = map_set(cc, id, k, MAP_W_LEARNED, force, name);
    return r == MAP_OK ? 1u : r == MAP_E_FULL ? 0u : r == MAP_E_CONFLICT ? 3u : 2u;
}
static uint32_t ml_learn(uint32_t cc, uint32_t k, uint32_t id) { return ml_learn_f(cc, k, id, 0, 0); }
/* takes away cc's learned line and parameter id of track k's (either: MAP_N / P_COUNT for none); 1 = one went. The number goes back to its shipped line */
#define ML_NOCC 0x80u                                   /* ml_drop: no CC to match (a learned CC is 0..127) */
static int ml_drop(uint32_t cc, uint32_t k, uint32_t id)
{
    uint32_t i, l, gone = 0;
    for (i = 0; i < MAP_N; i++)
        if (MAP_WHO(l = map_line(i)) == MAP_W_LEARNED && !MAP_IS_EMPTY(l) && (i == cc || (MAP_TARGET(l) == id && MAP_RULE(l) == k))) {
            map_unmap(i, 1);
            gone = 1;
        }
    return gone;
}

/* every learned CC cleared (GLO > MIDI LEARN KNOB 2 CLEAR, the editor's LEARN_SET op 2), the settings saved; 1 = there were */
static int ml_clear_all(void)
{
    uint32_t i, n = 0, l;
    for (i = 0; i < MAP_N; i++)
        if (MAP_WHO(l = map_line(i)) == MAP_W_LEARNED && !MAP_IS_EMPTY(l)) {
            map_unmap(i, 1);
            n++;
        }
    if (n) {
        ui.foot_sig = 0;
        settings_save();                         /* (deferred while playing) */
    }
    return n != 0u;
}
static void ml_off(void)
{
    ui.ml.on = ui.ml.pick = 0;
    ml_arm = 0;
    ml_heard = 0;
    ml_row_on = 0;
    ml_conflict_cc = 0;
    ui.foot_sig = 0;
}
static void ml_toggle(void)
{
    if (ui.ml.on) {
        ml_off();
        ui_message("LEARN DONE");
        return;
    }
    ui.ml.on = 1;
    ui.ml.pick = 0;
    ml_heard = 0;
    ui_message("MIDI LEARN ON");
}
/* a knob turned parameter id of track t (ui_input.c): picked while learning */
static void ml_knob(const track_t *t, uint32_t id)
{
    if (!ui.ml.on || id >= EXT_P_LEN)
        return;
    ml_row_on = 0;
    ml_conflict_cc = 0;
    ui.ml.trk = (uint8_t)trk_index(t);
    ui.ml.id = (uint8_t)id;
    ui.ml.pick = 1;
    ml_heard = 0;
    ml_arm = 1;
}
/* the editor's MAP_ARM: the next CC heard becomes the line (code, rule) (a typed line: no cap); ml_off() cancels */
static void ml_arm_row(uint32_t code, uint32_t rule)
{
    ml_row_on = 1;
    ml_row_code = code;
    ml_row_rule = rule;
    ui.ml.on = ui.ml.pick = 1;
    ui.ml.trk = (uint8_t)(rule == MAP_R_PART2 ? 1u : 0u);
    ui.ml.id = (uint8_t)(code < P_COUNT ? code : 0u);
    ml_heard = 0;
    ml_arm = 1;
    ui.foot_sig = 0;
}
/* the picked parameter's name: "T2 CUTOFF", "T1 FILT ENV ATTACK" (b: 24 bytes; T0227: the full name, as AUTOMATION's list: ui_events.c ev_name) */
static void ml_name(char *b)
{
    if (ml_row_on) {
        map_name(MAP_LINE(ml_row_code, ml_row_rule, MAP_W_TYPED), b);
        return;
    }
    b[0] = 'T';
    b[1] = (char)('1' + ui.ml.trk);
    b[2] = ' ';
    if (ui.ml.id >= XW_BASE)
        map_name(MAP_LINE(ui.ml.id, ui.ml.trk, MAP_W_LEARNED), b);
    else
        ev_name(&trk[ui.ml.trk % NTRK], ui.ml.id, b + 3);
}
/* "CC74 IS CUTOFF" on the footer: the number a learn did not take */
static void ml_refused(uint32_t cc, const char *owner)
{
    char a[12];
    a[0] = 'C';
    a[1] = 'C';
    fmt_int(a + 2, (int32_t)cc);
    str_cpy(a + str_len(a), " IS ", 5);
    ui_say(a, owner);
    ml_conflict_cc = (uint8_t)(cc + 1u);
    str_cpy(ml_conflict_txt, a, sizeof ml_conflict_txt);
    str_cpy(ml_conflict_txt + str_len(ml_conflict_txt), owner, sizeof ml_conflict_txt - str_len(ml_conflict_txt));
    ui.foot_sig = 0;
}
/* each frame: a CC heard for the picked parameter is learned */
static void ml_poll(void)
{
    uint32_t cc = ml_heard, r;
    char a[12], b[24];
    if (!cc)
        return;
    ml_heard = 0;
    if (!ui.ml.on || !ui.ml.pick)
        return;
    cc--;
    if (ml_row_on) {
        r = map_set(cc, ml_row_code, ml_row_rule, MAP_W_TYPED, 0, b);
        if (r == MAP_E_CONFLICT) {
            ml_refused(cc, b);
            return;
        }
        if (r != MAP_OK) {
            ui_message("CC NOT LEARNABLE");
            return;
        }
    } else {
        r = ml_learn_f(cc, ui.ml.trk, ui.ml.id, 0, b);
        if (r == 3u) {
            ml_refused(cc, b);                   /* (default no: the number stays what it was) */
            return;
        }
        if (r != 1u) {
            ui_message(r ? "CC NOT LEARNABLE" : "LEARN FULL");
            return;
        }
    }
    a[0] = 'C';
    a[1] = 'C';
    fmt_int(a + 2, (int32_t)cc);
    str_cpy(a + str_len(a), " -> ", 5);
    ml_name(b);
    ui_say(a, b);
    ui.ml.pick = 0;
    ml_arm = 0;
    if (ml_row_on)
        ml_off();                                /* (the editor's row: done, the unit leaves MIDI LEARN) */
    ui.foot_sig = 0;
    ml_conflict_cc = 0;
    settings_save();                             /* (deferred while playing) */
}
/* OCT- / OCT+ while learning (not on an action page): OCT- clears the picked parameter's CC, OCT+ is done */
static void ml_oct(uint32_t oct)
{
    char b[24];
    if (oct & 2u) {
        ml_off();
        ui_message("LEARN DONE");
    } else if (oct & 1u) {
        if (!ui.ml.pick || ml_row_on || !ml_drop(ML_NOCC, ui.ml.trk, ui.ml.id)) {
            ui_message(ui.ml.pick ? "NO CC TO CLEAR" : "TURN A KNOB");
            return;
        }
        ml_name(b);
        ui_say(b, " CC CLEARED");
        ml_conflict_cc = 0;
        ui.foot_sig = 0;
        settings_save();
    }
}
/* the footer's first row while learning: "TURN A KNOB", "T2 CUT: SEND A CC", "T2 CUT = CC74" */
static void ml_line(char *b)
{
    uint32_t cc;
    if (!ui.ml.pick) {
        str_cpy(b, "LEARN: TURN A KNOB", 24);
        return;
    }
    if (ml_conflict_cc) {                                /* a CC was refused: whose number it is, until the next pick */
        str_cpy(b, ml_conflict_txt, 24);
        return;
    }
    ml_name(b);
    cc = ml_row_on ? 0u : ml_cc_of(ui.ml.trk, ui.ml.id);
    if (!cc) {
        str_cpy(b + str_len(b), ": SEND A CC", 12);
        return;
    }
    str_cpy(b + str_len(b), " = CC", 6);
    fmt_int(b + str_len(b), (int32_t)cc - 1);
}
