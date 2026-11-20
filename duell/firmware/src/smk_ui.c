/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL on the M-Vave SMK-37 (BOARD_SMK37 only; felucca.c includes it after ui_layer.c): the main loop's side of the owner's
 * control plan (research/smk37-elite-port-study_2026-11-06.md, section 5). The interrupt side (pads, wheels, the outer keys) is
 * src/smk_play.c; the button table and the knob reader's entry are panel.c.
 *   K1..K4 part 1's four cards, K5..K8 part 2's, on every page (the screen switches to that part); on a page that edits no part
 *     K5..K8 do what K1..K4 do. PATCH held: the knobs change their part's sound (PRESETS).
 *   < >  SELECT.  CHORD  ARP 3/3|CHORD.  NOTE REPEAT  PERFORM on / off (on its release); its eight knobs any parameter: NOTE REPEAT held + a knob
 *     turned on any page puts that knob's parameter on the same knob of PERFORM (owner 2026-11-18; ui.c smk_pf_*).  G1 pages: K1..K4 the left
 *     cells, K5..K8 the right ones (smk_g1_half).  KNOB BANK  the LFO pages,
 *     FADER BANK  the ENV pages, both together  MATRIX.  BT  MENU on / off.  PATCH  HOME and the SAVE pages; on MORPH K1 / K5 the
 *     part's patch (owner 2026-11-18).
 *   Faders: 1 LEVEL 1, 2 LEVEL 2 absolute (the level is the fader's position, also after a load); 3 MORPH 1, 4 MORPH 2 with
 *     take-over (it acts once it reaches the value).
 *   Button LEDs on or off (the SMK has no dimmer); the pads' LEDs: the effect held lit. */

static uint8_t smk_nr_held, smk_nr_used, smk_nr_done;   /* NOTE REPEAT down; a knob was turned meanwhile (no PERFORM on / off at the release); the knobs assigned */
static int32_t smk_preset_acc;                  /* PATCH held: the detents for PRESETS (panel_enc EN_PRESET) */

static int smk_part_page(void)                  /* the page shown edits a part (K5..K8 then show and edit part 2) */
{
    return !ui.menu && (smk_stack_page(cur_page()) || (!page_global(cur_page()) && cur_page()->scope != SC_MODE));   /* (the stacked pages: PATCHES, PATTERNS, TOOLS too, part 2 on K5..K8) */
}

static int smk_octdn_only(void)
{
    return ((fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u) && !((fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u);
}
/* PERFORM's knob k (0..7): its parameter (ui.c smk_pf_get) turned by s; OCTAVE- held: to its default */
static void smk_perf_turn(uint32_t k, int32_t s)
{
    uint32_t part, id, kind = smk_pf_get(k, &part, &id), role = EN_K1 + (k & 3u);
    int reset = smk_octdn_only();
    if (reset)
        oct_mod = 1;
    if (kind == SPK_MACRO) {
        if (id == MAC_MORPH)
            morph_act(MQ_FADER, 1u << part, reset ? 0u : (uint32_t)clamp((int32_t)morph_fader_of(part) + accel(role, s, 127), 0, 127), part);
        else if (id == MAC_SPACE) {
            if (reset)
                perf_space = 0;
            else
                space_turn(role, s);
        } else if (id == MAC_KILL) {
            perf_fading = 0;
            perf_filt = (int8_t)(reset ? 0 : clamp(perf_filt + accel(role, s, 200), -100, 100));
        } else if (id >= MAC_TIMBRE && id <= MAC_MOTION)
            mac_part_put(id, 1u << part, reset ? 0 : mac_v[part % NLIVE][id - MAC_TIMBRE] + accel(role, s, 200));
        return;
    }
    if (kind == SPK_GLOBAL) {
        const param_desc_t *d;
        int16_t *vp;
        if (id == SG_VOL) {
            smk_volume_put(reset ? 100u : (uint32_t)clamp((int32_t)smk_volume() + accel(role, s, 100), 0, 100));
            return;
        }
        if (SG_SPECIAL(id) || !(d = glob_desc(id)))
            return;
        vp = smk_gvp(id);
        if (card_hidden_vp(vp))
            return;
        *vp = (int16_t)(reset ? d->def : param_turn(d, *vp, accel(role, s, d->fmt == F_ENUM ? 0 : d->max - d->min)));
        if (vp == &song.g[G_DMDEP])
            dmd_auto = 0;
        return;
    }
    if (kind == SPK_TRACK) {
        track_t *t = &trk[part];
        const param_desc_t *d = track_desc(t, id);
        int32_t v;
        if (!d || d->max == d->min)
            return;
        v = reset ? d->def : param_turn(d, t->p[id], accel(role, s, d->fmt == F_ENUM ? 0 : d->max - d->min));
        if (id == P_TWTYPE)
            twin_type_set(t, v == TT_COMB && (t->eng_req == ENGI_WAVE || t->eng_req == ENGI_PHYS) ? TT_LADDER : v);
        else if (id == XW_BASE + X_LINK)
            twin_link_set(t, v);
        else
            t->p[id] = (int16_t)v;
        if (id == P_DKSRC)
            duck_set_part(part);
        motion_capture(t, id, t->p[id]);
    }
}
/* NOTE REPEAT held + knob k (0..7) turned: that knob's parameter on this page (smk_screen.c smk_target) goes to PERFORM's knob k, said in the header */
static void smk_assign(uint32_t k)
{
    uint32_t part, id, kind;
    char nm[24], kn[12];
    smk_nr_used = 1;
    if ((smk_nr_done >> k) & 1u)                     /* (once per knob while NOTE REPEAT is held) */
        return;
    smk_nr_done |= (uint8_t)(1u << k);
    kind = smk_target(k, &part, &id);
    if (kind == SPK_NONE) {
        ui_message("NOTHING TO ASSIGN");
        return;
    }
    smk_pf_put(k, kind, part, id);
    smk_pf_name(k, nm);
    str_cpy(kn, "KNOB ", sizeof kn);
    fmt_int(kn + 5, (int32_t)k + 1);
    str_cpy(kn + str_len(kn), ": ", sizeof kn - str_len(kn));
    ui_say(kn, nm);
}
/* a G1 page's K5..K8: the right half's cells (id2) edited as a page of their own (ui.c cur_page: smk_pg_over), OCTAVE- held: the default */
static void smk_g1_half(uint32_t k, int32_t s)
{
    static page_t half;
    const page_t *pg = cur_page();
    uint32_t i;
    half = *pg;
    for (i = 0; i < 4u; i++)
        half.id[i] = pg->id2[i];
    half.graph = GR_NONE;
    half.g8 = SMK_G8_HALF;
    smk_pg_over = &half;
    if (smk_octdn_only()) {
        (void)knob_reset(k);
        oct_mod = 1;
    } else {
        edit_param(k, s);
    }
    smk_pg_over = 0;
}

static int32_t smk_knob(uint32_t k)
{
    int32_t a = fm1_enc_take(k), b = fm1_enc_take(k + 4u);
    if (!a && !b)
        return 0;
    if (smk_ov_shown) {                             /* a fader's or the pads' picture is on the screen (no knob on it): the turn only brings the page back (owner 2026-11-18:
                                                     * the knobs always do what the screen shows); the same fader motion or pad hold does not bring it back (flicker) */
        if (smk_ov_shown == 4u)
            smk_pad_block = 1;
        else {
            smk_ov_block = 1;
            smk_ov_ms = fm1_ms;
        }
        smk_ov = 0;
        return 0;
    }
    if ((fm1_in.buttons >> panel.btn[B_HOME]) & 1u) {   /* PATCH held: its part's sound, nothing else (no HOME, no MENU after it) */
        ui.home_t0 |= 2u;
        if (smk_part_page() || ui.perf)
            track_select(b && !a ? 1u : a && !b ? 0u : song.sel);
        smk_preset_acc += a + b;
        return 0;
    }
    if (!ui.perf && !ui.menu && cur_page()->scope == SC_MORPH && cur_page()->graph != GR_MORPHSET && k == 0u) {
        track_select(b && !a ? 1u : a && !b ? 0u : song.sel);   /* MORPH's K1 / K5: the part's patch (owner 2026-11-18; the faders do MORPH) */
        smk_preset_acc += a + b;
        return 0;
    }
    if (smk_nr_held && ((fm1_in.buttons >> SMK_B_NOTE_REPEAT) & 1u)) {   /* NOTE REPEAT held: the knob's parameter goes to PERFORM's same knob */
        if (!ui.perf && !ui.menu) {
            if (a)
                smk_assign(k);
            if (b)
                smk_assign(k + 4u);
        }
        return 0;
    }
    if (ui.perf && !ui.menu && !layer_held()) {     /* PERFORM: its eight knobs */
        if (a)
            smk_perf_turn(k, a);
        if (b)
            smk_perf_turn(k + 4u, b);
        return 0;
    }
    if (!ui.menu && !layer_held() && cur_page()->g8) {   /* a G1 page: K1..K4 the left cells (the page's knobs as ever), K5..K8 the right ones */
        if (b)
            smk_g1_half(k, b);
        return a;
    }
    if (!ui.menu && !layer_held() && cur_page()->fam == FAM_GLO && cur_page()->id[0] == P_LEVEL && (k == 0u || k == 2u)) {   /* MIXER (owner 2026-11-19): LEVEL is the faders' (no knob),
                                                                                               * knob 3 or 7 the unit's VOLUME */
        if (k == 2u && a + b) {
            int reset = smk_octdn_only();
            if (reset)
                oct_mod = 1;
            smk_volume_put(reset ? 100u : (uint32_t)clamp((int32_t)smk_volume() + accel(EN_K3, a + b, 100), 0, 100));
        }
        return 0;
    }
    if (!smk_part_page() || cur_page()->graph == GR_MOD)   /* (a part's MATRIX: knobs 5..8 do what 1..4 do, on the part shown; owner 2026-11-18) */
        return a + b;
    if (b && k == 2u && cur_page()->graph == GR_TOOLS && smk_stack_page(cur_page()))   /* (TOOLS: knob 7 has no job; RESTORE ALL PATCHES is knob 3, part 1's half) */
        b = 0;
    if (!a && !b)
        return 0;
    if (a)                                          /* (the TWIN TABLE: the turned knob's dot in its part's colour, smk_screen.c) */
        smk_turn_mark(0u, k);
    if (b)
        smk_turn_mark(1u, k);
    if (b && !a) {
        track_select(1u);
        return b;
    }
    if (a && !b) {
        track_select(0u);
        return a;
    }
    return song.sel ? b : a;
}

static int32_t smk_preset_take(void)
{
    int32_t s = smk_preset_acc;
    smk_preset_acc = 0;
    return s;
}

static void smk_chord_page(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_CHORD)
            break;
    if (i >= NPAGES)
        return;
    if (ui.perf)
        perf_off();
    ui.page = (uint8_t)i;
    ui.fam_last[PAGES[i].fam] = ui.page;
    page_entered();
}

/* KNOB BANK (LFO) / FADER BANK (ENV) let go alone: that family's page, the next one when already there; MATRIX is its own round */
static void smk_bank_tap(uint32_t fam)
{
    uint32_t p;
    if (ui.perf)
        perf_off();
    if (cur_page()->fam == fam && cur_page()->graph != GR_MOD) {
        open_family(fam);
        return;
    }
    p = ui.fam_last[fam];
    if (PAGES[p].fam != fam || PAGES[p].graph == GR_MOD)
        p = page_first(fam);
    ui.page = (uint8_t)p;
    ui.fam_last[fam] = ui.page;
    page_entered();
}
static void smk_matrix(void)                    /* KNOB BANK + FADER BANK: MATRIX 1-4 (the LFO round's copy); again: 5-8, then the other part's 1-4, 5-8 (owner 2026-11-18: a page per part) */
{
    uint32_t i;
    if (!ui.perf && !ui.menu && cur_page()->graph == GR_MOD) {
        for (i = (uint32_t)ui.page + 1u; i < NPAGES && PAGES[i].graph != GR_MOD; i++)
            ;
        if (i < NPAGES && PAGES[i].fam == cur_page()->fam) {
            ui.page = (uint8_t)i;
            page_entered();
            return;
        }
        track_select(song.sel ^ 1u);
    }
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_LFO && PAGES[i].graph == GR_MOD)
            break;
    if (i >= NPAGES)
        return;
    if (ui.perf)
        perf_off();
    ui.page = (uint8_t)i;
    page_entered();
}
static void smk_menu(void)                      /* BT: MENU on / off (it was PATCH held) */
{
    if (ui.menu) {
        menu_close();
        return;
    }
    ui.menu = 6;                                /* (as HOME held did: the list of the sections, at the section it was left at) */
    ui.confirm = 0;
    name_close();
    ui.force = 1;
    song.seq_mode = 0;
}

/* the buttons without an FM-1 label (panel.c SMK_B_EXTRA): their presses here, taken out of what ui_input sees */
static uint8_t smk_bank_down, smk_bank_chord;
static uint32_t smk_buttons(uint32_t pressed)
{
    uint32_t banks = (fm1_in.buttons >> SMK_B_KNOB_BANK & 1u) | (fm1_in.buttons >> SMK_B_FADER_BANK & 1u) << 1, up;
    if ((pressed >> SMK_B_BT) & 1u)
        smk_menu();
    if (!ui.menu && !ui.confirm && !name_on()) {
        if ((pressed >> SMK_B_NOTE_REPEAT) & 1u) {   /* NOTE REPEAT: PERFORM on / off at its release, unless a knob was turned while it was held (an assignment) */
            smk_nr_held = 1;
            smk_nr_used = smk_nr_done = 0;
        }
        if (smk_nr_held && !((fm1_in.buttons >> SMK_B_NOTE_REPEAT) & 1u)) {
            smk_nr_held = 0;
            if (!smk_nr_used) {
                if (ui.perf)
                    perf_leave();
                else
                    perf_enter();
            }
        }
        if ((pressed >> SMK_B_CHORD) & 1u)
            smk_chord_page();
        smk_bank_down |= (uint8_t)(((pressed >> SMK_B_KNOB_BANK) & 1u) | ((pressed >> SMK_B_FADER_BANK) & 1u) << 1);
        if (banks == 3u && !smk_bank_chord) {
            smk_bank_chord = 1;
            smk_matrix();
        }
        up = smk_bank_down & ~banks;
        if (up && !smk_bank_chord) {
            if (up & 1u)
                smk_bank_tap(FAM_LFO);
            else
                smk_bank_tap(FAM_ENV);
        }
    }
    smk_bank_down &= (uint8_t)banks;
    if (!banks)
        smk_bank_chord = 0;
    if ((pressed >> SMK_B_LEFT) & 1u)
        smk_sel_steps--;
    if ((pressed >> SMK_B_RIGHT) & 1u)
        smk_sel_steps++;
    if (!((fm1_in.buttons >> SMK_B_NOTE_REPEAT) & 1u))
        smk_nr_held = 0;                            /* (let go in the menu or a dialog: nothing) */
    return pressed & ~SMK_B_EXTRA;
}

/* ---- the faders: 1 LEVEL 1, 2 LEVEL 2, 3 MORPH 1, 4 MORPH 2. Take-over: a fader acts once it has reached the value it stands
 * for (or crossed it), so nothing jumps; a change from elsewhere (a knob, a sound load, MIDI) lets it go again */
static uint8_t smk_fd_on[4], smk_fd_last[4] = {0xFF, 0xFF, 0xFF, 0xFF};
static int16_t smk_fd_wrote[4];
static int32_t smk_fd_value(uint32_t f)
{
    return f < 2u ? trk[f].p[P_LEVEL] : (int32_t)morph_fader_of(f - 2u);
}
static void smk_faders(void)
{
    uint32_t f;
    if (fm1_in.frames < 300u)                       /* (the readings settle) */
        return;
    for (f = 0; f < 4u; f++) {
        int32_t v = fm1_in.fader[f], cur = smk_fd_value(f), last = smk_fd_last[f];
        if (f < 2u) {                               /* LEVEL 1 / 2 absolute (owner 2026-11-17): the fader's position is the level, at
                                                     * power-on and after any load; nothing else changes it */
            int32_t lv = clamp(v, TP[P_LEVEL].min, TP[P_LEVEL].max);
            if (trk[f].p[P_LEVEL] != lv) {
                trk[f].p[P_LEVEL] = (int16_t)lv;
                if (last != 0xFF && v != last) {
                    motion_capture(&trk[f], P_LEVEL, trk[f].p[P_LEVEL]);
                    smk_fader_moved(f);             /* (the screen shows the level for a moment: smk_screen.c) */
                }
            }
            smk_fd_last[f] = (uint8_t)v;
            continue;
        }
        if (last == 0xFF) {
            smk_fd_last[f] = (uint8_t)v;
            continue;
        }
        if (v == last)
            continue;
        smk_fd_last[f] = (uint8_t)v;
        smk_fader_moved(f);                         /* (the screen shows both parts' MORPH for a moment: smk_screen.c) */
        if (smk_fd_on[f] && cur != smk_fd_wrote[f])
            smk_fd_on[f] = 0;                       /* moved elsewhere since: take over again */
        if (!smk_fd_on[f] && ((last <= cur && v >= cur) || (last >= cur && v <= cur) || (v - cur <= 2 && cur - v <= 2)))
            smk_fd_on[f] = 1;
        if (!smk_fd_on[f])
            continue;
        if (f < 2u) {
            trk[f].p[P_LEVEL] = (int16_t)clamp(v, TP[P_LEVEL].min, TP[P_LEVEL].max);
            motion_capture(&trk[f], P_LEVEL, trk[f].p[P_LEVEL]);
        } else {
            morph_act(MQ_FADER, morph_mask(f - 2u), (uint32_t)v, f - 2u);
        }
        smk_fd_wrote[f] = (int16_t)smk_fd_value(f);   /* (no full redraw: it flickered at every fader step, owner 2026-11-16) */
    }
}

/* ---- the button LEDs (hal/smk_input.h fm1_led[], LED i = button i; SEQ PLAY's green 16 and red 18) from ui_leds' picture */
static void smk_leds(const uint8_t *lit, int playing)
{
    uint32_t i;
    for (i = 0; i < SMK_NLED; i++)
        fm1_led[i] = (uint8_t)((lit[i >> 3] >> (i & 7u)) & 1u);
    fm1_led[SMK_B_NOTE_REPEAT] = (uint8_t)(ui.perf != 0);
    fm1_led[SMK_B_KNOB_BANK] = (uint8_t)(!ui.perf && !ui.menu && cur_page()->fam == FAM_LFO);
    fm1_led[SMK_B_FADER_BANK] = (uint8_t)(!ui.perf && !ui.menu && (cur_page()->fam == FAM_ENV || cur_page()->graph == GR_MOD));
    fm1_led[SMK_B_BT] = (uint8_t)(ui.menu != 0);
    fm1_led[SMK_B_CHORD] = cur_page()->graph == GR_CHORD && !ui.perf && !ui.menu;
    fm1_led[SMK_B_LEFT] = fm1_led[SMK_B_RIGHT] = 0;
    fm1_led[16] = (uint8_t)!playing;                /* SEQ PLAY: green while stopped, */
    fm1_led[18] = (uint8_t)(playing != 0);          /* red while playing */
}

/* ---- the pads' LEDs: the PERFORM effect held lit, the others a faint mark (lcd.c padled_*) */
static uint32_t smk_pad_seen;
static void smk_pad_leds(void)
{
    uint32_t p, held = perf_held, fresh = held & ~smk_pad_seen;
    smk_pad_seen = held;
    {   /* ROLL A / B only fire (no held bit): their press counted (perform.c perf_roll_n) */
        static uint8_t rn[2];
        uint32_t r;
        for (r = 0; r < 2u; r++)
            if (perf_roll_n[r] != rn[r]) {
                rn[r] = perf_roll_n[r];
                fresh |= PF_BIT(PF_ROLLA + r);
                smk_pad_lit = PF_BIT(PF_ROLLA + r);
                smk_pad_ms = fm1_ms;
            }
    }
    if (fresh)
        smk_pad_block = 0;                              /* (a new pad press: the pads' picture again, owner 2026-11-18) */
    for (p = 0; p < 16u; p++) {
        if ((held >> p) & 1u)
            padled_set(p, 120, 120, 120);
        else
            padled_set(p, 6, 6, 6);
    }
    padled_flush();
}

static void smk_ui_poll(void)                   /* each ui_input pass */
{
    smk_faders();
    smk_pad_leds();
}
