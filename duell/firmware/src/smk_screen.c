/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL on the SMK-37: one screen for both parts (SMK_SCREEN, = BOARD_SMK37: hal/fm1_cc.h; ui_draw.c includes this file). The owner's choice of 2026-11-16
 * (working docs/smk37-screen-build-plan_2026-11-16.md, Fable's design rounds: TWIN TABLE, C4):
 *   TWIN TABLE (grid_on SMK_GRID) on the part pages of plain parameter cards: four rows, one per knob pair; knob dot k + 1 at the left, part 1's value (its colour) left aligned,
 *     the label once in the middle, part 2's value right aligned, knob dot k + 5 at the right; a thin bar under each value. Where the two parts' controls differ (the engine pages)
 *     each side carries its own label over its value. The dots are white; a dot takes its part's colour for SMK_LIT_MS after that part's value of that knob changed (a knob turn,
 *     smk_ui.c smk_knob, or any change: MIDI, the editor, a morph).
 *   Both part capsules lit (ui_draw.c draw_head); no footer: the page counter and the battery stand in the line under the header, between the two engine names on the
 *     engine pages (EDIT 1 / EDIT 2; the ENGINE page names them in its first row).
 *   C4: on MULTI FILTER 1/2, TWIN FILTER, CYCLE ENV, AMP ENV, ENV 2, LFO 1 and LFO 2 (2026-11-18: not on the packed pages, which hold no whole group: the EQ, MULTI FILTER 2/2) a strip of both parts' curves sits above the table (part 1 filled, part 2 a line), the rows
 *     lower. AMP ENV / ENV 2: real time across, both parts on one time window (the synth's own times: gen_tables.py TIME_MS_X10, ENV SYNC's note values as voice.c esync_one);
 *     LFO 1 / LFO 2: the real rate (LFO_HZ_X100, or SYNC's note value of the tempo as voice.c lfo_sync_inc) on one time window.
 *   G1 (2026-11-18): the GLOBAL pages MASTER and TEMPO and the shared effects CHORUS|DELAY, DELAY|REVERB: eight cells (smk_g1_draw). PERFORM: the pads, the effect held, DEPTH,
 *     the eight knobs (smk_perf_draw). The faders' pictures: MORPH or a LEVEL for a moment (smk_view_page). MORPH's K1 cell: the two parts' patches (ui_draw.c).
 * The lists, the matrix, the sequencer's pages and FM6's operator pages are drawn as on the FM-1. */

#define SMK_LIT_MS 1000u             /* a dot in its part's colour this long after a change */
#define SMK_HEAD_Y 24                /* the line under the header: the counter, the battery, the engine names */
#define SMK_HEAD_H 20
#define SMK_ROW_Y 44                 /* the table: four rows of 49 */
#define SMK_ROW_H 49
#define SMK_STRIP_Y 24               /* C4: the strip, rows 24 .. 85; the table's rows of 38 from row 88 */
#define SMK_STRIP_H 62
#define SMK_SROW_Y 88
#define SMK_SROW_H 38
#define SMK_VX1 19                   /* part 1's values start here, part 2's end at 240 - SMK_VX1 */
#define SMK_DOT_X1 4                 /* the dots (AF_S_CAP_H wide): x 4 .. 14 and 225 .. 235 */
#define SMK_BAR_W 70
#define SMK_GAP 6                    /* a value -> the middle label */
#define SMK_SW 232                   /* the strip's curves: x 4 .. 235 */
#define SMK_SX 4

static uint32_t smk_turn_ms[2][4];   /* fm1_ms of the last change of part p's knob k */
static uint8_t smk_turn_on[2];       /* bit k: smk_turn_ms[p][k] holds one */
static uint32_t smk_vh[2][4], smk_vkey; /* the values shown (their hash) and the page they were on: a change marks the knob */
static uint32_t smk_row_sig[4], smk_head_sig, smk_strip_sig;
static uint32_t smk_off;            /* C4, AMP ENV / ENV 2: when both gates close (1/10 ms) */

static void smk_turn_mark(uint32_t p, uint32_t k)
{
    smk_turn_ms[p & 1u][k & 3u] = fm1_ms;
    smk_turn_on[p & 1u] |= (uint8_t)(1u << (k & 3u));
}
static int smk_lit(uint32_t p, uint32_t k)
{
    if (!((smk_turn_on[p & 1u] >> (k & 3u)) & 1u))
        return 0;
    if (fm1_ms - smk_turn_ms[p & 1u][k & 3u] < SMK_LIT_MS)
        return 1;
    smk_turn_on[p & 1u] &= (uint8_t)~(1u << (k & 3u));   /* (over: no wrap-around of the clock lights it again) */
    return 0;
}

/* the page is a TWIN TABLE page: a part page of plain parameter cards (scope TRACK or ENGINE, not GLOBAL), not the matrix, the lists, the step pages, FM6's operators, a module's OFF notice */
static int smk_twin_page(const page_t *pg)
{
    uint32_t g = pg->graph;
    if (g == GR_MORPH && pg->scope == SC_MORPH)        /* MORPH (owner 2026-11-18): the table too, PATCH ROLL UNDO AMOUNT per part, both parts' lines over it */
        return !ui.menu && !ui.layer && !ui.perf && !page_over;
    if (ui.menu || ui.layer || ui.perf || page_over || page_global(pg) || (pg->scope != SC_TRACK && pg->scope != SC_ENGINE))
        return 0;
    if (g == GR_MOD || g == GR_EVENTS || g == GR_TOOLS || g == GR_BROWSE || g == GR_PATS || g == GR_SLOTS || g == GR_ROLL || g == GR_SEQHELP || fop_page(g))
        return 0;
    return !off_module(pg) && !act_cols();
}
/* C4: the strip's curve of the page (0: none): 1 AMP ENV, 2 ENV 2, 3 LFO 1, 4 LFO 2, 5 MULTI FILTER, 6 TWIN FILTER, 7 EQ, 8 CYCLE ENV */
static uint32_t smk_strip_kind(const page_t *pg)
{
    uint32_t g = pg->graph, i0 = pg->id[0];
    if (g == GR_MORPH)                                 /* 9: MORPH, both parts' lines, full width, over each other */
        return 9u;
    if (g == GR_ADSR)
        return i0 == P_ATK ? 1u : i0 == P_E2ATK ? 2u : 0u;
    if (g == GR_LFO)
        return i0 == P_LWAVE ? 3u : i0 == P_L2WAVE ? 4u : 0u;
    if (g == GR_FILT)
        return i0 == P_FREEA ? 5u : i0 == P_TWTYPE ? 6u : 0u;   /* (owner 2026-11-18: MULTI FILTER 2/2 is packed, BLEND moved onto it from TWIN CHAIN, and the picture would not show BLEND: no strip) */
    if (g == GR_EQ)                                    /* (owner 2026-11-18: the EQ is packed over three pages, INSERT|EQ, EQ, EQ|DUCK: no page holds the whole EQ, so no strip; a strip only for a group its page holds) */
        return 0u;
    if (g == GR_CYCLE)
        return 8u;
    return 0;
}

/* ---- part p's card of a slot (ui_draw.c draw_columns' and params.c page_desc's rules, for a given part instead of the selected one) */
static uint32_t smk_pg_id(const page_t *pg, uint32_t slot, const track_t *t)
{
    uint32_t id = pg->id[slot & 3u];
    if (pg->scope == SC_ENGINE && id >= P_E0 && id < P_E0 + 8u) {
        const uint8_t *o = ENGINES[t->eng_req % NENGINES]->uiord;
        if (o)
            return P_E0 + o[id - P_E0];
    }
    return id;
}
static const param_desc_t *smk_desc(const page_t *pg, uint32_t slot, track_t *t, int16_t **valp)
{
    uint32_t id = smk_pg_id(pg, slot, t);
    *valp = 0;
    if (id == 0xFFu)
        return 0;
    if (id == P_DKRATE) {
        id = duck_ctx_id(t);
        if (id == 0xFFu)
            return 0;
    }
    *valp = &t->p[id];
    if (pg->scope == SC_TRACK && pg->id[0] == P_KTRK && (id == P_E2FLT || id == P_LD_FLT || id == P_L2FLT))
        return &KT_FLT[id == P_L2FLT ? 2u : id == P_LD_FLT ? 1u : 0u];
    if (id == XW_BASE + X_LFO2 && pg->id[0] == XW_BASE + X_LFO2)
        return &DST_TCUT;
    return track_desc(t, id);
}
/* card_hidden_vp for part t (the part-page rules; the global ones never meet a part's value) */
static int smk_hidden(const track_t *t, const int16_t *vp)
{
    if (!vp)
        return 0;
    if (mods_fade_vp(vp))
        return 1;
    if (twin_card_hidden(t, vp))
        return 1;
    if (duell_mode_req == DM_SPLIT && (vp == &t->p[P_PAN] || vp == &t->p[P_SPRD] || vp == &XV(t, X_CYPAN)))
        return 1;
    if (vp >= &t->p[P_IA] && vp <= &t->p[P_IMIX] && vp != &t->p[P_ITYPE] && !t->p[P_ITYPE])
        return 1;
    if (duck_forced(t) && vp == &t->p[P_DKSRC])
        return 1;
    if (duck_off(t) && (vp == &t->p[P_DKRATE] || vp == &t->p[P_DKTHR] || vp == &t->p[P_DKDEP] || vp == &t->p[P_DKATK] || vp == &t->p[P_DKRLS] || vp == &t->p[P_DKCRV]))
        return 1;
    if (vp == &XV(t, X_STRUM) && !strum_acts(t))
        return 1;
    if (vp == &t->p[P_ATGT] && ENGINES[eng_idx(t->eng_req)]->atgt == ATG_NONE)   /* (ARP 3/3|CHORD: TARGET only where the engine has one) */
        return 1;
    if (vp == &t->p[P_PRIO] && t->p[P_VOICE] == V_POLY)
        return 1;
    if (vp == &t->p[P_DETUNE] && t->p[P_VOICE] != V_UNISON)
        return 1;
    if (vp == &t->p[P_SPRD] && (t->p[P_VOICE] == V_MONO || t->p[P_VOICE] == V_LEGATO))
        return 1;
    if ((vp == &t->p[P_ED_SHP] || vp == &t->p[P_LD_SHP] || vp == &t->p[P_E2SHP] || vp == &t->p[P_L2SHP]) && mod_shp_inert(t))
        return 1;
    if (ENGINES[t->eng_req % NENGINES] == &ENG_ANALOG && vp == &t->p[P_E4] && t->p[P_E0] != 4)
        return 1;
    if (ENGINES[t->eng_req % NENGINES] == &ENG_LOFI && vp == &t->p[P_E2] && t->p[P_E1] != RW_PLS)
        return 1;
    return (vp == &t->p[P_LRATE] && t->p[P_LSYNC]) || (vp == &t->p[P_L2RATE] && t->p[P_L2SYNC]);
}
static int smk_duck_midi(const page_t *pg, const track_t *t)   /* EQ|DUCK with the part's SOURCE MIDI: K3 (RATE's place) is the link to the MIDI setup (params.c DUCK_LINK_SLOT) */
{
    return pg->scope == SC_TRACK && pg->fam == FAM_FX && pg->id[1] == P_DKSRC && t->p[P_DKSRC] == 2;
}
static int smk_slot_hidden(const page_t *pg, uint32_t s, track_t *t)
{
    int16_t *vp;
    if (s == DUCK_LINK_SLOT && smk_duck_midi(pg, t))
        return 0;
    if (pg->id[s] == P_DKRATE && duck_ctx_id(t) == 0xFFu)
        return 1;
    (void)smk_desc(pg, s, t, &vp);
    return vp && smk_hidden(t, vp);
}
static int smk_slot_blank(const page_t *pg, uint32_t s, track_t *t)
{
    int16_t *vp;
    const param_desc_t *d;
    if (s == DUCK_LINK_SLOT && smk_duck_midi(pg, t))
        return 0;
    d = smk_desc(pg, s, t, &vp);
    return !d || !d->label || d->label[0] == '-';
}
/* page_cards for part t: its cards that show, moved left (K1..Kn: the knobs of that part address them so, ui_input.c knob_slot with the part selected) */
static uint32_t smk_cards(const page_t *pg, track_t *t, uint8_t vis[4])
{
    uint32_t s, n = 0, hid = 0;
    for (s = 0; s < 4u; s++)
        vis[s] = (uint8_t)s;
    if (!page_compacts(pg))
        return 4u;
    for (s = 0; s < 4u; s++)
        hid |= (uint32_t)smk_slot_hidden(pg, s, t) << s;
    if (!hid)
        return 4u;
    for (s = 0; s < 4u; s++)
        if (!((hid >> s) & 1u) && !smk_slot_blank(pg, s, t))
            vis[n++] = (uint8_t)s;
    for (s = n; s < 4u; s++)
        vis[s] = 0xFFu;
    return n;
}
static int smk_byp(const page_t *pg, const track_t *t, const int16_t *vp, uint32_t p)
{
    static const struct { const char *title; uint16_t part; } B[] = {
        {"MULTI FILTER 1/2", BY_MULTI}, {"MULTI FILTER 2/2", BY_MULTI}, {"TWIN FILTER", BY_TWIN}, {"TWIN MOD", BY_TWIN},
        {"LFO 1", BY_LFO1}, {"LFO 1 TRIG", BY_LFO1}, {"LFO 1 DEST", BY_LFO1}, {"LFO 2", BY_LFO2}, {"LFO 2 TRIG", BY_LFO2}, {"LFO 2 DEST", BY_LFO2},
        {"ENV 2", BY_ENV2}, {"ENV 2 DEST", BY_ENV2}, {"CYCLE ENV 1/2", BY_CYC}, {"CYCLE ENV 2/2", BY_CYC}};
    static const uint8_t INS[] = {P_ITYPE, P_IA, P_IB, P_IC, P_IMIX}, EQ[] = {P_EQLF, P_EQLG, P_EQMF, P_EQMG, P_EQMQ, P_EQHF, P_EQHG},
                         DK[] = {P_DKSRC, P_DKRATE, P_DKTHR, P_DKDEP, P_DKATK, P_DKRLS, P_DKCRV};
    uint32_t i, m = byp_m[p & 1u];
    if (!m)
        return 0;
    if (vp == &t->p[P_BLEND])                          /* (BLEND acts while the filters are bypassed, as TWIN CHAIN's cards) */
        return 0;
    if (vp == &t->p[P_DIST])                           /* the packed FX pages and the MIXER: by the card's module */
        return (m & BY_SAT) != 0u;
    if (vp == &t->p[P_CHOR] || vp == &t->p[P_DLY] || vp == &t->p[P_REV])
        return (m & BY_SEND) != 0u;
    for (i = 0; i < sizeof INS; i++)
        if (vp == &t->p[INS[i]])
            return (m & BY_INS) != 0u;
    for (i = 0; i < sizeof EQ; i++)
        if (vp == &t->p[EQ[i]])
            return (m & BY_EQ) != 0u;
    for (i = 0; i < sizeof DK; i++)
        if (vp == &t->p[DK[i]])
            return (m & BY_DUCK) != 0u;
    for (i = 0; i < sizeof B / sizeof B[0]; i++)
        if (str_eq(pg->title, B[i].title))
            return (m & B[i].part) != 0u;
    return 0;
}
/* the value a card of part p shows (MORPH: the blended one, as card_vv) */
static int32_t smk_vv(const int16_t *vp, uint32_t p, const track_t *t)
{
    uintptr_t id = ((uintptr_t)vp - (uintptr_t)t->p) / sizeof *vp;
    int32_t v = *vp;
    if ((id < P_COUNT || (id >= XW_BASE && id < EXT_P_LEN)) && morph_view_on(p))
        v = morph_view(p, (uint32_t)id);
    return v;
}

typedef struct {
    char lb[24];                     /* the label as shown (a "\n" is a chosen break); "" none: the full words (NAMING_RULES 3, 4) */
    char lbs[24];                    /* the same with the accepted short forms: used only where the full words do not fit */
    char val[PV_LEN];                /* the value ("\n": a two-line name) */
    const char *unit;
    int32_t ratio;                   /* the bar 0..1000, -1 none */
    uint8_t on, ctr, dim, eng;       /* on: a card; ctr: a bipolar bar from the middle; dim: BYPASS; eng: the ENGINE page's engine (no label) */
    uint8_t nodot, glob, top;        /* nodot: no knob sets it (MIXER's LEVEL: the faders); glob: one value for the unit, in the middle (MIXER's VOLUME);
                                      * top: the label alone on top, the values at a fixed size under it (MORPH's PATCH row: nothing moves when a name changes) */
} smk_cell_t;
/* NAMING_RULES (owner): a word in full wherever it fits, the accepted short form only where it does not. The SMK's table rows have the
 * room the FM-1's cards had not: the labels are built in full here and smk_row falls back to these short forms when a full one does not fit */
static const char *const SMK_FULL[][2] = {{"ALGO", "ALGORITHM"}, {"FDBK", "FEEDBACK"},   /* (owner 2026-11-17: RES and OCT stay short; */
                                          {"AMT", "AMOUNT"}, {"BAL", "BALANCE"}, {"VEL", "VELOCITY"}};   /* the rest long, the short form only where it does not fit) */
static void smk_full_words(char *d, const char *s, uint32_t n)
{
    uint32_t o = 0, i = 0;
    while (s[i] && o + 1u < n) {
        uint32_t j = i, k, m, hit = 0;
        while (s[j] && s[j] != ' ' && s[j] != '\n')
            j++;
        for (k = 0; k < NELEM(SMK_FULL) && !hit; k++) {
            for (m = 0; SMK_FULL[k][0][m] && i + m < j && SMK_FULL[k][0][m] == s[i + m]; m++)
                ;
            if (!SMK_FULL[k][0][m] && i + m == j) {
                const char *w = SMK_FULL[k][1];
                while (*w && o + 1u < n)
                    d[o++] = *w++;
                hit = 1;
            }
        }
        if (!hit)
            while (i < j && o + 1u < n)
                d[o++] = s[i++];
        i = j;
        if (s[i] && o + 1u < n)
            d[o++] = s[i++];
    }
    d[o] = 0;
}
static void smk_cell(const page_t *pg, uint32_t p, uint32_t k, smk_cell_t *c)
{
    track_t *t = &trk[p & 1u];
    uint8_t vis[4];
    uint32_t n = smk_cards(pg, t, vis), s = k < n ? vis[k] : 0xFFu;
    int16_t *vp;
    const param_desc_t *d;
    int32_t vv, rt;
    c->on = c->ctr = c->dim = c->eng = 0;
    c->lb[0] = c->lbs[0] = c->val[0] = 0;
    c->unit = "";
    c->ratio = -1;
    c->nodot = c->glob = c->top = 0;
    if (pg->fam == FAM_GLO && pg->id[0] == P_LEVEL && k == 2u) {   /* MIXER's VOLUME (owner 2026-11-19): the unit's, one value; knob 3 or 7 sets it */
        uint32_t v = smk_volume();
        str_cpy(c->lb, "VOLUME", sizeof c->lb);
        str_cpy(c->lbs, "VOLUME", sizeof c->lbs);
        fmt_int(c->val, (int32_t)v);
        c->unit = "%";
        c->ratio = (int32_t)v * 10;
        c->on = c->glob = 1;
        return;
    }
    if (pg->graph == GR_MORPH) {                       /* MORPH: K1 / K5 the part's patch, ROLL and UNDO (the twist building up on the selected part: its way), AMOUNT */
        static const char *const ML[4] = {"PATCH", "ROLL", "UNDO", "AMOUNT"};
        uint32_t pi = p & 1u;
        int32_t g = (k == 1u || k == 2u) && pi == song.sel ? morph_pending(k - 1u) : 0;
        str_cpy(c->lb, ML[k & 3u], sizeof c->lb);
        str_cpy(c->lbs, ML[k & 3u], sizeof c->lbs);
        c->on = 1;
        if (k == 0u) {
            sound_name(t, c->val);
            c->top = 1;
        }
        else if (k < 3u)
            str_cpy(c->val, g < 0 ? "<A" : g > 0 ? "B>" : "A<>B", sizeof c->val);
        else {
            fmt_int(c->val, (int32_t)morph_amount_of(pi));
            c->unit = "%";
            c->ratio = (int32_t)morph_amount_of(pi) * 10;
        }
        return;
    }
    if (s >= 4u)
        return;
    d = smk_desc(pg, s, t, &vp);
    if (pg->graph == GR_ENGINE && s == 0u) {           /* ENGINE: K1 the engine (its name, no label) */
        str_cpy(c->val, eng_shown(ENGINES[t->eng_req % NENGINES]->name), sizeof c->val);
        c->on = c->eng = 1;
        return;
    }
    if (s == DUCK_LINK_SLOT && smk_duck_midi(pg, t)) { /* EQ|DUCK with SOURCE MIDI: K3 jumps to the settings */
        str_cpy(c->lb, "SETUP", sizeof c->lb);
        str_cpy(c->val, "SETTINGS", sizeof c->val);
        c->on = 1;
        return;
    }
    if (vp && smk_hidden(t, vp))
        return;
    if (!d || !d->label || d->label[0] == '-')
        return;
    str_cpy(c->lbs, quick_label(pg, s) ? quick_label(pg, s) : desc_shown(d), sizeof c->lbs);
    if (vp == &t->p[P_DIST])                           /* (the FX page: DRIVE beside the sends) */
        str_cpy(c->lbs, "DRIVE", sizeof c->lbs);
    else if (vp == &t->p[P_ITYPE] || vp == &t->p[P_DKSRC])   /* (the page's title names the effect) */
        str_cpy(c->lbs, vp == &t->p[P_ITYPE] ? "TYPE" : "SOURCE", sizeof c->lbs);
    else if (vp == &t->p[P_LEVEL])                     /* (MIXER: the faders' level, no knob) */
        str_cpy(c->lbs, "LEVEL", sizeof c->lbs);
    else if (vp == &t->p[P_CHRD])                      /* (ARP 3/3|CHORD: TARGET, CHORD, VOICING, STRUM) */
        str_cpy(c->lbs, "CHORD", sizeof c->lbs);
    smk_full_words(c->lb, c->lbs, sizeof c->lb);
    {   /* (owner 2026-11-17: AMT, BAL, VEL may stand where the full word does not fit; ALGORITHM and FEEDBACK never shorten) */
        char t[24];
        uint32_t i, o = 0;
        str_cpy(t, c->lbs, sizeof t);
        for (i = 0; t[i] && o + 1u < sizeof c->lbs; ) {
            if ((i == 0 || t[i - 1] == ' ' || t[i - 1] == '\n') && t[i + 1] && t[i + 2] && t[i + 3] && !memcmp(t + i, "ALGO", 4) && (!t[i + 4] || t[i + 4] == ' ' || t[i + 4] == '\n')) {
                const char *w = "ALGORITHM"; while (*w && o + 1u < sizeof c->lbs) c->lbs[o++] = *w++; i += 4;
            } else if ((i == 0 || t[i - 1] == ' ' || t[i - 1] == '\n') && t[i + 1] && t[i + 2] && t[i + 3] && !memcmp(t + i, "FDBK", 4) && (!t[i + 4] || t[i + 4] == ' ' || t[i + 4] == '\n')) {
                const char *w = "FEEDBACK"; while (*w && o + 1u < sizeof c->lbs) c->lbs[o++] = *w++; i += 4;
            } else
                c->lbs[o++] = t[i++];
        }
        c->lbs[o] = 0;
    }
    c->on = 1;
    c->nodot = vp == &t->p[P_LEVEL];
    if (vp && smk_byp(pg, t, vp, p)) {
        str_cpy(c->val, "BYPASS", sizeof c->val);
        c->dim = 1;
        return;
    }
    vv = vp ? smk_vv(vp, p, t) : 0;
    param_format(d, (int16_t)vv, c->val, &c->unit);
    if (!c->unit)
        c->unit = "";
    if (vp && vp == &XV(t, X_STRUM) && vv <= 0) {
        str_cpy(c->val, "OFF", sizeof c->val);
        c->unit = "";
    }
    if (pg->scope == SC_TRACK && (rt = twin_card(t, vp, d, vv, c->val, &c->unit)) != -2) {
        c->ratio = rt;
        c->ctr = card_ctr_next;
        card_ctr_next = 0;
    } else
        c->ratio = RATIO(d, enum_rank(d, (int16_t)vv));
    if (d->fmt == F_ENUM)                              /* (a name has no bar, as UT_CARD_ENUM_BAR 0) */
        c->ratio = -1;
    if (str_eq(c->val, "PAT"))                         /* FM6's ALGORITHM: the patch's own (owner 2026-11-17: full words) */
        str_cpy(c->val, "PATCH", sizeof c->val);
    if (c->val[0] == '.' || (c->val[0] == '-' && c->val[1] == '.')) {   /* ".53" -> "0.53" */
        char z[PV_LEN];
        uint32_t neg = c->val[0] == '-';
        str_cpy(z, c->val + neg, sizeof z);
        c->val[neg] = '0';
        str_cpy(c->val + neg + 1u, z, sizeof c->val - neg - 1u);
    }
}
static uint32_t smk_cell_hash(const smk_cell_t *c)
{
    return str_hash(str_hash(str_hash(c->on * 7u + c->dim * 13u + c->ctr * 29u + (uint32_t)(c->ratio + 1) * 977u, c->lb), c->val), c->unit);
}

/* ---- drawing */
/* s with its "\n" a space (d holds PV_LEN) */
static void smk_one_line(char *d, const char *s)
{
    uint32_t i;
    for (i = 0; s[i] && i + 1u < PV_LEN; i++)
        d[i] = s[i] == '\n' ? ' ' : s[i];
    d[i] = 0;
}
static int smk_has_nl(const char *s)
{
    for (; *s; s++)
        if (*s == '\n')
            return 1;
    return 0;
}
static int32_t smk_uw(const char *u) { return u[0] ? text_w(&AF_M, u) : 0; }
/* the face a value takes in room px: N for figures (big), L, the Bold T, else S; -> its width with the unit */
static const aafont_t *smk_vface(const char *v, const char *u, int32_t room, int big, int32_t *w)
{
    const aafont_t *f = big && card_figure(v) && u[0] != '/' ? &AF_N : &AF_L;
    int32_t uw = smk_uw(u);
    if (f == &AF_N && text_w(f, v) + uw > room)
        f = &AF_L;
    if (f == &AF_L && text_w(f, v) + uw > room)
        f = &AF_T;
    if (f == &AF_T && text_w(f, v) + uw > room)
        f = &AF_S;
    *w = text_w(f, v) + uw;
    return f;
}
/* a value and its unit on baseline base, from x (right: ending at x) */
static void smk_val_draw(int32_t x, int right, int32_t base, const aafont_t *f, const char *v, const char *u, uint16_t col)
{
    int32_t w = text_w(f, v) + smk_uw(u);
    if (right)
        x -= w;
    x = cv_text_on(x, card_ty(f, base), f, v, col, T_BG);
    if (u[0])
        cv_text_on(x, card_ty(&AF_M, base), &AF_M, u, col == T_DIM ? T_DIM : T_MID, T_BG);
}
/* a two-line value in S (a long name: "DIMINISHED / HALF-WHOLE"), its second line on base */
static void smk_val2_draw(int32_t x, int right, int32_t base, const char *v, uint16_t col)
{
    char a[PV_LEN];
    uint32_t i;
    for (i = 0; v[i] && v[i] != '\n' && i + 1u < PV_LEN; i++)
        a[i] = v[i];
    a[i] = 0;
    if (right) {
        cv_text_r(x, UT_TY(S, base - 14), &AF_S, a, col, T_BG);
        cv_text_r(x, UT_TY(S, base), &AF_S, v[i] ? v + i + 1 : "", col, T_BG);
    } else {
        cv_text_on(x, UT_TY(S, base - 14), &AF_S, a, col, T_BG);
        cv_text_on(x, UT_TY(S, base), &AF_S, v[i] ? v + i + 1 : "", col, T_BG);
    }
}
static void smk_dot(int32_t x, int32_t top, uint32_t n, uint16_t col)
{
    char dg[2] = {(char)('1' + n), 0};
    cv_rrect(x, top, AF_S_CAP_H, AF_S_CAP_H, AF_S_CAP_H / 2, col, T_BG);
    cv_text_in(x, top + HALF_UP(AF_S_CAP_H - AF_D_CAP_H) - AF_D_CAP_Y, AF_S_CAP_H, &AF_DOT, dg, T_BG, col);
}
static void smk_bar(int32_t x, int32_t y, int32_t w, const smk_cell_t *c, uint32_t p)
{
    uint16_t pc = T_PTHEME(p), rest = ux_mix(T_BASE, pc, UT_CARD_BAR_REST_PCT), gc = c->dim ? T_DIM : pc;
    int32_t fx;
    if (c->ratio < 0 || c->dim)
        return;
    cv_rrect(x, y, w, UT_CARD_BAR_H, UT_CARD_BAR_R, rest, T_BG);
    if (c->ctr) {
        int32_t half = w / 2, ext = (c->ratio - 500) * half / 500, x0 = x + half;
        cv_rrect(ext < 0 ? x0 + ext : x0 - (ext ? 0 : 1), y, ext < 0 ? -ext : ext < 2 ? 2 : ext, UT_CARD_BAR_H, UT_CARD_BAR_R, gc, rest);
        return;
    }
    fx = (c->ratio * w + 500) / 1000;
    cv_rrect(x, y, fx < UT_CARD_BAR_MIN ? UT_CARD_BAR_MIN : fx, UT_CARD_BAR_H, UT_CARD_BAR_R, gc, rest);
}
/* the label's lines: one (l2 = 0) when it fits w on one line, else two at its break or its last space; -> the widest line */
static int32_t smk_label_lines(const char *lb, char *l1, const char **l2)
{
    char one[PV_LEN];
    int32_t i, cut = -1, w1, w2;
    smk_one_line(one, lb);
    for (i = 0; lb[i]; i++)
        if (lb[i] == '\n')
            cut = i;
    if (cut < 0)
        for (i = 0; lb[i]; i++)
            if (lb[i] == ' ')
                cut = i;
    str_cpy(l1, one, PV_LEN);
    *l2 = 0;
    if (cut < 0)
        return text_w(&AF_S, one);
    l1[cut] = 0;
    w1 = text_w(&AF_S, l1);
    w2 = text_w(&AF_S, lb + cut + 1);
    *l2 = lb + cut + 1;
    return w1 > w2 ? w1 : w2;
}

/* one row of the table: knob k's pair. y, h: its screen rows (49, or 38 under the strip) */
static uint16_t smk_gcol(void);
static void smk_row(uint32_t k, int32_t y, int32_t h, const smk_cell_t *c)
{
    const int strip = h < SMK_ROW_H;
    const int32_t vb = strip ? 27 : 35, bar_y = strip ? 31 : 40, lbase = strip ? 24 : 29, dtop = strip ? 14 : 19;   /* (the value's baseline, the bar, the label's baseline, the dots' top: rel to the row) */
    char one[PV_LEN], l1[PV_LEN];
    const char *l2;
    int32_t w[2] = {0, 0}, lw, room, p;
    const aafont_t *f[2] = {&AF_L, &AF_L};
    int two[2] = {0, 0}, diff = c[0].on && c[1].on && !str_eq(c[0].lb, c[1].lb);
    uint16_t col[2];
    cv_begin(240, (uint32_t)h, T_BG);
    cv_rect(4, 0, 232, 1, ux_mix(T_BG, T_TEXT, 14));     /* the rule over the row */
    for (p = 0; p < 2; p++) {
        col[p] = c[p].dim ? T_DIM : vt(T_PTHEME(p), T_BG);
        if (c[p].on && !c[p].nodot)
            smk_dot(p ? 240 - SMK_DOT_X1 - AF_S_CAP_H : SMK_DOT_X1, diff && !strip ? 23 : dtop, k + 4u * (uint32_t)p, smk_lit((uint32_t)p, k) ? vt(T_PTHEME(p), T_BG) : T_TEXT);
    }
    if (c[0].top || c[1].top) {                        /* (owner 2026-11-20: the word PATCH jumped as the names changed) the label centred on top, each name under it on its side,
                                                         * one size (T, S where a name is wider than its half): nothing moves but the names */
        const int32_t lb0 = strip ? 12 : 15, vbt = strip ? 31 : 39, side = 120 - SMK_VX1 - 4;
        cv_text_in(0, UT_TY(S, lb0), 240, &AF_S, c[0].on ? c[0].lb : c[1].lb, T_TEXT, T_BG);
        for (p = 0; p < 2; p++) {
            const aafont_t *ft = &AF_T;
            if (!c[p].on)
                continue;
            smk_one_line(one, c[p].val);
            if (text_w(ft, one) > side)
                ft = &AF_S;
            if (text_w(ft, one) > side)
                cv_scroll = 1;
            smk_val_draw(p ? 240 - SMK_VX1 : SMK_VX1, p, vbt, ft, one, "", col[p]);
            cv_scroll = 0;
        }
        cv_blit(0, (uint32_t)y);
        return;
    }
    if (c[0].glob) {                                   /* one value for the unit (MIXER's VOLUME): its label over it, in the middle, the global colour; knobs k and k + 4 both set it */
        int32_t wv;
        const aafont_t *fv;
        uint16_t gc = smk_gcol();
        smk_one_line(one, c[0].val);
        fv = smk_vface(one, c[0].unit, 120, !strip, &wv);
        cv_text_in(0, UT_TY(S, 15), 240, &AF_S, c[0].lb, T_TEXT, T_BG);
        smk_val_draw(120 - wv / 2, 0, vb + 4, fv, one, c[0].unit, gc);
        cv_blit(0, (uint32_t)y);
        return;
    }
    if (c[0].eng || c[1].eng) {                        /* ENGINE: the two engines' names, no label (one name in the middle when both parts play the same engine) */
        if (c[0].on && c[1].on && str_eq(c[0].val, c[1].val)) {
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "smk engine name centred");
            cv_text_in(0, card_ty(&AF_L, vb), 240, &AF_L, c[0].val, T_TEXT, T_BG);
        } else
            for (p = 0; p < 2; p++)
                if (c[p].on) {
                    f[p] = smk_vface(c[p].val, "", 100, 0, &w[p]);
                    smk_val_draw(p ? 240 - SMK_VX1 : SMK_VX1, p, vb, f[p], c[p].val, "", col[p]);
                }
        cv_blit(0, (uint32_t)y);
        return;
    }
    if (diff) {                                        /* the parts' controls differ: each side its own label over its value */
        const int32_t lb2 = strip ? 13 : 15, vb2 = strip ? 33 : 39, bar2 = strip ? -1 : 43;
        for (p = 0; p < 2; p++) {
            int32_t x = p ? 240 - SMK_VX1 : SMK_VX1, side = 120 - SMK_VX1 - 2;
            smk_one_line(one, c[p].lb);
            if (text_w(&AF_S, one) > side)                  /* (the full words do not fit: the accepted short forms) */
                smk_one_line(one, c[p].lbs);
            if (p)
                cv_text_r(x, UT_TY(S, lb2), &AF_S, one, T_TEXT, T_BG);
            else
                cv_text_on(x, UT_TY(S, lb2), &AF_S, one, T_TEXT, T_BG);
            smk_one_line(one, c[p].val);
            f[p] = smk_vface(one, c[p].unit, side, !strip, &w[p]);
            if (w[p] > side && smk_has_nl(c[p].val))
                smk_val2_draw(x, p, strip ? 37 : 45, c[p].val, col[p]);   /* (two lines, no bar: lower, clear of the label) */
            else
                smk_val_draw(x, p, vb2, f[p], one, c[p].unit, col[p]);
            if (bar2 >= 0)
                smk_bar(p ? 240 - SMK_VX1 - SMK_BAR_W : SMK_VX1, bar2, SMK_BAR_W, &c[p], (uint32_t)p);
        }
        cv_blit(0, (uint32_t)y);
        return;
    }
    /* the same control on both sides (or one side only): the label once in the middle, the values big at the sides. Its full words
     * when they fit beside the values at their normal size, else the accepted short forms (NAMING_RULES 3) */
    {
        static smk_cell_t cc[2] SMK_POOL;
        int32_t w0[2] = {0, 0}, r0;
        const char *full = c[0].on ? c[0].lb : c[1].lb, *sh = c[0].on ? c[0].lbs : c[1].lbs;
        for (p = 0; p < 2; p++)
            if (c[p].on) {
                smk_one_line(one, c[p].val);
                (void)smk_vface(one, c[p].unit, 100, !strip, &w0[p]);
            }
        r0 = 240 - 2 * SMK_VX1 - w0[0] - w0[1] - (c[0].on && c[1].on ? 2 : 1) * SMK_GAP;
        smk_one_line(one, full);
        if (!str_eq(full, sh) && text_w(&AF_S, one) > r0 && (smk_label_lines(full, l1, &l2), !l2 || smk_label_lines(full, l1, &l2) > r0)) {
            cc[0] = c[0];
            cc[1] = c[1];
            str_cpy(cc[0].lb, cc[0].lbs, sizeof cc[0].lb);
            str_cpy(cc[1].lb, cc[1].lbs, sizeof cc[1].lb);
            c = cc;
        }
    }
    lw = c[0].on ? smk_label_lines(c[0].lb, l1, &l2) : c[1].on ? smk_label_lines(c[1].lb, l1, &l2) : 0;
    {
        int32_t w1 = 0, i;
        smk_one_line(one, c[0].on ? c[0].lb : c[1].on ? c[1].lb : "");
        w1 = text_w(&AF_S, one);
        for (i = 0; i < 4; i++) {                      /* the values shrink (N, L, then less room: T, S, two lines) until the label fits between them */
            int32_t lim = i < 2 ? 100 : i == 2 ? 72 : 56;
            for (p = 0; p < 2; p++) {
                if (!c[p].on)
                    continue;
                smk_one_line(one, c[p].val);
                f[p] = smk_vface(one, c[p].unit, lim, !strip && i == 0, &w[p]);
                two[p] = 0;
                if (w[p] > lim && smk_has_nl(c[p].val)) {
                    char a[PV_LEN];
                    uint32_t j;
                    for (j = 0; c[p].val[j] && c[p].val[j] != '\n'; j++)
                        a[j] = c[p].val[j];
                    a[j] = 0;
                    w[p] = text_w(&AF_S, a);
                    if (text_w(&AF_S, c[p].val + j + 1) > w[p])
                        w[p] = text_w(&AF_S, c[p].val + j + 1);
                    two[p] = 1;
                }
            }
            room = 240 - 2 * SMK_VX1 - w[0] - w[1] - 2 * SMK_GAP;
            if (!c[0].on || !c[1].on)
                room = 240 - 2 * SMK_VX1 - w[0] - w[1] - SMK_GAP;
            if (w1 <= room || (l2 && lw <= room))
                break;
        }
    }
    {   /* the label: one line when it fits the room, else its two lines */
        char ol[PV_LEN];
        const char *src = c[0].on ? c[0].lb : c[1].lb;
        int32_t x0 = SMK_VX1 + w[0] + SMK_GAP, x1 = 240 - SMK_VX1 - w[1] - SMK_GAP, cx;
        smk_one_line(ol, src);
        if (!c[0].on && !c[1].on)
            ol[0] = 0;
        if (c[0].on != c[1].on) {                      /* (one side only: the label in the middle of the screen, clear of the value) */
            x0 = c[0].on ? x0 : SMK_VX1;
            x1 = c[1].on ? x1 : 240 - SMK_VX1;
        }
        cx = (x0 + x1) / 2;
        if (ol[0] && text_w(&AF_S, ol) <= x1 - x0) {
            int32_t lx = 120 - text_w(&AF_S, ol) / 2;   /* (centred on the screen, kept between the values) */
            if (lx < x0)
                lx = x0;
            if (lx + text_w(&AF_S, ol) > x1)
                lx = x1 - text_w(&AF_S, ol);
            cv_text_on(lx, UT_TY(S, lbase), &AF_S, ol, T_TEXT, T_BG);
        } else if (ol[0]) {
            const char *b = l2 ? l2 : "";
            int32_t wa = text_w(&AF_S, l1), wb = text_w(&AF_S, b), xa = 120 - wa / 2, xb = 120 - wb / 2;
            (void)cx;
            if (xa < x0) xa = x0;
            if (xa + wa > x1) xa = x1 - wa;
            if (xb < x0) xb = x0;
            if (xb + wb > x1) xb = x1 - wb;
            cv_text_on(xa, UT_TY(S, strip ? 14 : lbase - 7), &AF_S, l1, T_TEXT, T_BG);   /* (two lines: both clear of the bars' row) */
            cv_text_on(xb, UT_TY(S, strip ? 28 : lbase + 7), &AF_S, b, T_TEXT, T_BG);
        }
    }
    for (p = 0; p < 2; p++) {
        int32_t x = p ? 240 - SMK_VX1 : SMK_VX1;
        if (!c[p].on)
            continue;
        if (two[p]) {
            smk_val2_draw(x, p, vb + (strip ? 4 : 6), c[p].val, col[p]);
            continue;
        }
        smk_one_line(one, c[p].val);
        smk_val_draw(x, p, vb, f[p], one, c[p].unit, col[p]);
        smk_bar(p ? 240 - SMK_VX1 - SMK_BAR_W : SMK_VX1, bar_y, SMK_BAR_W, &c[p], (uint32_t)p);
    }
    cv_blit(0, (uint32_t)y);
}

/* the battery at x, y of the canvas (draw_battery's drawing) */
static void smk_battery(int32_t x, int32_t y)
{
    int32_t lvl = batt_shown();
    uint16_t rv[16];
    ramp_lin(rv, T_MID, T_BG);
    if (lvl >= 4) {
        cv_alpha(x, y, UT_BAT_W, UT_BAT_H, BAT_BOLT, rv);
        return;
    }
    cv_alpha(x, y, UT_BAT_W, UT_BAT_H, BAT_OUT, rv);
    if (lvl >= 1)
        cv_rect(x + UT_BAT_FX, y + UT_BAT_FY, lvl >= 3 ? UT_BAT_FW : lvl * (UT_BAT_FW / 3), UT_BAT_FH, lvl == 1 ? T_ACCENT : T_MID);
}
/* the line under the header: the page's counter and the battery; on EDIT 1 / EDIT 2 the parts' engines at the sides (one in the middle when both play the same one) */
static void smk_head_line(const page_t *pg)
{
    char nm[28], ct[8];
    const char *e0 = eng_shown(ENGINES[trk[0].eng_req % NENGINES]->name), *e1 = eng_shown(ENGINES[trk[1].eng_req % NENGINES]->name);
    int names = pg->scope == SC_ENGINE && pg->graph != GR_ENGINE, same = str_eq(e0, e1);
    const int32_t base = 15;                           /* (rel: the capitals' last row, screen row 39) */
    int32_t cw, x, bw = UT_BAT_W + 4;
    uint32_t sig;
    (void)page_parts(nm, ct);
    sig = str_hash(str_hash(str_hash(names * 31u + (uint32_t)batt_shown() * 7u + ux.gen * 101u, ct), e0), e1);
    if (!ui.force && sig == smk_head_sig)
        return;
    smk_head_sig = sig;
    cv_begin(240, SMK_HEAD_H, T_BG);
    cw = ct[0] ? text_w(&AF_M, ct) : 0;
    if (names && same) {                               /* one engine: its name in the middle, the counter and the battery at the right */
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "smk engine name centred");
        cv_text_in(0, UT_TY(S, base), 240, &AF_S, e0, T_TEXT, T_BG);
        x = 240 - SMK_VX1 - bw - cw;
    } else {
        if (names) {
            cv_text_on(SMK_VX1, UT_TY(S, base), &AF_S, e0, vt(T_PTHEME(0), T_BG), T_BG);
            cv_text_r(240 - SMK_VX1, UT_TY(S, base), &AF_S, e1, vt(T_PTHEME(1), T_BG), T_BG);
        }
        x = 120 - (cw + (cw ? 4 : 0) + UT_BAT_W) / 2;
    }
    if (ct[0])
        x = cv_text_on(x, UT_TY(M, base), &AF_M, ct, T_MID, T_BG) + 4;
    smk_battery(x, base - UT_BAT_H + 1);
    cv_blit(0, SMK_HEAD_Y);
}

/* ---- C4: the curves */
/* e^-x, x >= 0 in Q12 -> Q15 (2^-u by a quadratic: within 0.5 %) */
static int32_t smk_expn(int64_t xq12)
{
    int64_t y = xq12 * 5909 >> 12;                     /* x log2(e), Q12 */
    int32_t i, f, v;
    if (y >= (15 << 12))
        return 0;
    i = (int32_t)(y >> 12);
    f = (int32_t)(y & 4095);
    v = 32768 - (f * 21510 >> 12) + (((f * f) >> 12) * 5125 >> 12);
    return v >> i;
}
/* an envelope time of part t (ATTACK, DECAY or RELEASE id) in 1/10 ms: the table's (1 ms .. 10 s), or ENV SYNC's note value of the tempo (voice.c esync_one) */
static uint32_t smk_env_ms10(const track_t *t, uint32_t id)
{
    int32_t v = clamp(t->p[id], 0, 127);
    if (t->p[P_ESYNC]) {
        uint64_t T = (uint64_t)beat_samples() * ESYNC_UNITS[esync_idx(v)] / 96u;
        return T < CTL ? TIME_MS_X10[0] : (uint32_t)(T * 10000u / FS);
    }
    return TIME_MS_X10[v];
}
/* the nice time windows (1/10 ms): 20 ms .. 100 s */
static uint32_t smk_window(uint64_t need)
{
    static const uint32_t W[] = {200u, 500u, 1000u, 2000u, 5000u, 10000u, 20000u, 50000u, 100000u, 200000u, 500000u, 1000000u};
    uint32_t i;
    for (i = 0; i + 1u < NELEM(W) && W[i] < need; i++)
        ;
    return W[i];
}
/* an envelope's level (Q15) at t ms10: attack linear, decay and release exponential (~99 % after their time: voice.c env_tick), the gate open until off */
static int32_t smk_env_at(uint32_t a, uint32_t d, int32_t s, uint32_t r, uint32_t off, uint32_t t)
{
    int32_t l;
    if (t < off || !r) {
        if (t < a)
            return (int32_t)((uint64_t)t * 32768u / a);
        l = d ? s + (int32_t)(((int64_t)(32768 - s) * smk_expn((int64_t)(t - a) * 18842 / d)) >> 15) : s;
        if (t < off)
            return l;
        return 0;
    }
    l = smk_env_at(a, d, s, 0, off + 1u, off);
    return (int32_t)(((int64_t)l * smk_expn((int64_t)(t - off) * 18842 / r)) >> 15);
}
/* an LFO's period of part t in 1/10 ms: RATE's (0.05 .. 40 Hz) or SYNC's note value (voice.c lfo_sync_inc) */
static uint32_t smk_lfo_ms10(const track_t *t, uint32_t r, uint32_t s)
{
    if (t->p[s]) {
        uint64_t c = div_samples(LSYNC_DIV[(uint32_t)t->p[s] % NELEM(LSYNC_DIV)]);
        return c ? (uint32_t)(c * 10000u / FS) : 1u;
    }
    return 1000000u / LFO_HZ_X100[t->p[r] & 127];
}
/* the strip's points of part p (y per x, canvas rows; n points across SMK_SW): 0 when the part has no curve here */
#define SMK_NPT SMK_SW
static uint32_t smk_scr_curve(uint32_t kind, uint32_t p, int16_t *ys, int32_t *base, uint32_t win)
{
    track_t *t = &trk[p & 1u];
    const int32_t top = 5, bot = SMK_STRIP_H - 4;     /* (rows 5 .. 58) */
    uint32_t x;
    if (kind == 1u || kind == 2u) {
        uint32_t e0 = kind == 1u ? P_ATK : P_E2ATK, a, d, r, off = smk_off;
        int32_t s = clamp(t->p[e0 + 2u], 0, 127) * 32768 / 127;
        if (kind == 1u && eng_idx(t->eng_req) == ENGI_FM6)   /* (FM6's AMP ENV is its carriers' DX7 envelopes: no time picture) */
            return 0;
        a = smk_env_ms10(t, e0);
        d = smk_env_ms10(t, e0 + 1u);
        r = smk_env_ms10(t, e0 + 3u);
        *base = bot;
        for (x = 0; x < SMK_NPT; x++) {
            uint32_t tm = (uint32_t)((uint64_t)x * win / (SMK_NPT - 1u));
            ys[x] = (int16_t)(bot - smk_env_at(a, d, s, r, off, tm) * (bot - top) / 32768);
        }
        return SMK_NPT;
    }
    if (kind == 3u || kind == 4u) {
        uint32_t r = kind == 3u ? P_LRATE : P_L2RATE, s = kind == 3u ? P_LSYNC : P_L2SYNC, per = smk_lfo_ms10(t, r, s), fade = t->p[r + 3u] ? TIME_MS_X10[t->p[r + 3u] & 127] : 0u;
        int32_t uni = t->p[s + 2u] != 0, cy = (top + bot) / 2, a = (bot - top) / 2, lv = XV(t, kind == 3u ? X_LFO1LVL : X_LFO2LVL);
        uint64_t ph0 = (uint64_t)((uint32_t)t->p[r + 2u] << 25);
        *base = uni ? bot : cy;
        for (x = 0; x < SMK_NPT; x++) {
            uint64_t tm = (uint64_t)x * win / (SMK_NPT - 1u), full = (tm << 32) / (per ? per : 1u) + ph0;
            uint32_t cyc = (uint32_t)(full >> 32), rnd = (cyc + 1u) * 2654435761u ^ (p + 1u) * 40503u;
            int32_t w = lfo_wave_of(t->p[r + 1u], rnd * 2246822519u, (uint32_t)full);
            if (uni)
                w = (w + 32768) >> 1;
            if (lv < 127)
                w = w * (lv < 0 ? 0 : lv) / 127;
            if (fade && tm < fade)
                w = (int32_t)((int64_t)w * (int64_t)tm / fade);
            ys[x] = (int16_t)(uni ? bot - w * (bot - top) / 32768 : cy - w * a / 32768);
        }
        return SMK_NPT;
    }
    if (kind == 5u || kind == 6u) {                    /* the filter (twin_graph.c: MULTI, or TWIN), +18 .. -42 dB */
        uint32_t i, f = GF_F0;
        if (kind == 6u && !f2_ty(t))
            return 0;
        *base = bot;
        for (i = 0; i < GF_N; i++, f = (uint32_t)(((uint64_t)f * GF_STEP) >> 16)) {
            int32_t db = twin_graph_db(t, kind == 6u ? 1 : 0, f >> 8) * 10 / 16;
            ys[i] = (int16_t)(top + (180 - clamp(db, -420, 180)) * (bot - top) / 600);
        }
        return GF_N;
    }
    if (kind == 7u) {                                  /* the EQ (graph_eq's three bands), +-12 dB */
        static const uint8_t GK[3] = {P_EQLG, P_EQMG, P_EQHG}, FK[3] = {P_EQLF, P_EQMF, P_EQHF};
        static const int8_t FMAX[3] = {24, 24, 12};
        gq_t n[3], d[3];
        eqc_t cf;
        uint32_t k, i, on = 0, f = GF_F0;
        int32_t cy = (top + bot) / 2, a = (bot - top) / 2;
        for (k = 0; k < 3u; k++) {
            int32_t g = clamp(t->p[GK[k]], -12, 12);
            if (!g)
                continue;
            eq_design(&cf, k, g, clamp(t->p[FK[k]], -24, FMAX[k]), k == 1u ? clamp(t->p[P_EQMQ], -12, 12) : 0);
            eq_quad(&n[k], &d[k], &cf);
            on |= 1u << k;
        }
        *base = cy;
        for (i = 0; i < GF_N; i++, f = (uint32_t)(((uint64_t)f * GF_STEP) >> 16))
            ys[i] = (int16_t)(cy - clamp(eq_curve_db10(n, d, on, f), -120, 120) * a / 120);
        return GF_N;
    }
    {                                                  /* CYCLE ENV (graph_cycle's shape: RISE and FALL in proportion, CURVE, LOOP three times / ONCE) */
        const int big = cur_page()->id[0] == XW_BASE + X_CYCURVE;
        int32_t cv = clamp(XV(t, X_CYCURVE), -64, 63) * 512, w = SMK_NPT - 1, n = big || XV(t, X_CYMODE) ? 1 : 3, cw, rw, fw;
        uint64_t dr, df;
        uint32_t rr = (uint32_t)XV(t, X_CYRISE) & 127u, ff = (uint32_t)XV(t, X_CYFALL) & 127u;
        if (XV(t, X_CYSYNC)) {
            dr = (uint64_t)ESYNC_UNITS[esync_idx((int32_t)rr)] + 2u;
            df = (uint64_t)ESYNC_UNITS[esync_idx((int32_t)ff)] + 2u;
        } else {
            dr = (uint64_t)((1ull << 40) / (ENV_LIN[rr] ? ENV_LIN[rr] : 1u));
            df = (uint64_t)((1ull << 40) / (ENV_LIN[ff] ? ENV_LIN[ff] : 1u));
        }
        cw = XV(t, X_CYMODE) && !big ? w * 2 / 3 : w / n;
        rw = big ? cw / 2 : (int32_t)((uint64_t)cw * dr / (dr + df));
        rw = clamp(rw, 3, cw - 3);
        fw = cw - rw;
        *base = bot;
        for (x = 0; x < SMK_NPT; x++) {
            int32_t k = (int32_t)x / cw, i = (int32_t)x - k * cw, y01;
            if (k >= n) {                              /* (ONCE: it rests at 0; LOOP: the last cycle's end) */
                ys[x] = (int16_t)(XV(t, X_CYMODE) && !big ? bot : ys[x - 1u]);
                continue;
            }
            if (i <= rw) {
                int32_t u = i * 32768 / rw;
                y01 = u + ((cv * ((u * u >> 15) - u)) >> 15);
            } else {
                int32_t u = (i - rw) * 32768 / fw;
                y01 = 32768 - (u + ((cv * ((u * u >> 15) - u)) >> 15));
            }
            ys[x] = (int16_t)(bot - clamp(y01, 0, 32768) * (bot - top) / 32768);
        }
        return SMK_NPT;
    }
}
static int32_t smk_px(uint32_t i, uint32_t n) { return SMK_SX + (int32_t)(i * (SMK_SW - 1u) / (n - 1u)); }
/* the strip: both parts' curves on one axis (part 1 filled under its line, part 2 a line), redrawn when a value it shows changes */
static void smk_strip(const page_t *pg)
{
    static int16_t ys[2][SMK_NPT] SMK_POOL;
    uint32_t kind = smk_strip_kind(pg), n[2], p, i, sig = kind * 7919u + ux.gen * 31u + (uint32_t)song.g[G_BPM] * 131u + beat_samples(), win = 0;
    int32_t base[2] = {SMK_STRIP_H - 4, SMK_STRIP_H - 4};
    uint16_t guide = ux_mix(T_BASE, T_TEXT, UT_GR_GUIDE_PCT);
    for (p = 0; p < 2u; p++)
        for (i = 0; i < EXT_P_LEN; i++)
            sig = (sig ^ (uint16_t)trk[p].p[i]) * 16777619u + p;
    sig ^= (uint32_t)trk[0].eng_req * 3u + (uint32_t)trk[1].eng_req * 5u;
    if (kind == 9u) {                                  /* MORPH: the animation's step, both faders */
        uint32_t tk = mv_tick(), st;
        sig = tk * 2654435761u ^ morph_fader_of(0) * 40503u ^ morph_fader_of(1) * 977u ^ ux.gen * 101u ^ 0x5A5Au;
        if (!ui.force && sig == smk_strip_sig)
            return;
        smk_strip_sig = sig;
        mv_levels(tk);
        st = mv_step == 0xFFFFFFFFu ? 0u : tk - mv_step;
        mv_listen(0, st > 4u ? 4u : st);
        mv_listen(1, st > 4u ? 4u : st);
        mv_step = tk;
        CV_BANDS(240, SMK_STRIP_H, T_BG) {
            mv_field(1, 4, SMK_STRIP_H - 4, 3u);       /* (part 2's lines, then part 1's over them) */
            mv_field(0, 4, SMK_STRIP_H - 4, 3u);
            cv_blit(0, SMK_STRIP_Y + (uint32_t)cv_by);
        }
        return;
    }
    if (!ui.force && sig == smk_strip_sig)
        return;
    smk_strip_sig = sig;
    if (kind == 1u || kind == 2u) {                    /* one time window for both: the longest ATTACK + DECAY and RELEASE fit with room to sustain; both gates close at the same time */
        uint32_t e0 = kind == 1u ? P_ATK : P_E2ATK, ad = 0, rl = 0, core, w, off;
        for (p = 0; p < 2u; p++) {
            const track_t *t = &trk[p];
            uint32_t a = smk_env_ms10(t, e0) + smk_env_ms10(t, e0 + 1u), r = smk_env_ms10(t, e0 + 3u);
            if (kind == 1u && eng_idx(t->eng_req) == ENGI_FM6)
                continue;
            ad = a > ad ? a : ad;
            rl = r > rl ? r : rl;
        }
        core = ad + rl;
        w = smk_window((uint64_t)core * 5u / 4u + 1u);
        off = ad + (w - core) / 2u;
        win = w;
        smk_off = off;
    } else if (kind == 3u || kind == 4u) {             /* one time window for both: two cycles of the slower LFO, its fade-in too */
        uint32_t r = kind == 3u ? P_LRATE : P_L2RATE, s = kind == 3u ? P_LSYNC : P_L2SYNC;
        uint64_t need = 0;
        for (p = 0; p < 2u; p++) {
            const track_t *t = &trk[p];
            uint64_t a = 2u * (uint64_t)smk_lfo_ms10(t, r, s), f = t->p[r + 3u] ? (uint64_t)TIME_MS_X10[t->p[r + 3u] & 127] * 5u / 4u : 0u;
            need = a > need ? a : need;
            need = f > need ? f : need;
        }
        win = smk_window(need);
    }
    for (p = 0; p < 2u; p++)
        n[p] = smk_scr_curve(kind, p, ys[p], &base[p], win);
    CV_BANDS(240, SMK_STRIP_H, T_BG) {
        CV_HL(0, 0, 240, SMK_STRIP_H);                 /* (data: the area under part 1's curve) */
        if (n[0]) {                                    /* part 1: filled to its base line */
            uint16_t fill = ux_mix(T_BG, T_PTHEME(0), 22);
            int32_t x;
            for (x = 0; x < SMK_SW; x++) {
                uint32_t j = (uint32_t)x * (n[0] - 1u) / (SMK_SW - 1u), j1 = j + 1u < n[0] ? j + 1u : j;
                int32_t x0 = smk_px(j, n[0]), x1 = smk_px(j1, n[0]), y = x1 > x0 ? ys[0][j] + (ys[0][j1] - ys[0][j]) * (SMK_SX + x - x0) / (x1 - x0) : ys[0][j];
                if (y < base[0])
                    cv_rect(SMK_SX + x, y, 1, base[0] - y, fill);
                else if (y > base[0])
                    cv_rect(SMK_SX + x, base[0], 1, y - base[0], fill);
            }
        }
        cv_rect(SMK_SX, base[0], SMK_SW, 1, guide);    /* the base line (the envelope's floor, the LFO's zero, 0 dB) */
        for (p = 2u; p-- > 0u;) {                      /* part 2's line, then part 1's over it */
            uint16_t c = vt(T_PTHEME(p), T_BG);
            for (i = 1; i < n[p]; i++)
                cv_line_t(smk_px(i - 1u, n[p]), ys[p][i - 1u], smk_px(i, n[p]), ys[p][i], c, 2);
        }
        cv_blit(0, SMK_STRIP_Y + (uint32_t)cv_by);
    }
}

/* the page's body: the strip (C4 pages), the line under the header (the others) and the four rows; called by ui_draw.c draw_columns in place of the cards */
static void smk_twin_draw(void)
{
    const page_t *pg = cur_page();
    uint32_t k, p, key = (uint32_t)ui.page * 7919u + (uint32_t)trk[0].eng_req * 31u + (uint32_t)trk[1].eng_req * 977u + 1u;
    int strip = smk_strip_kind(pg) != 0u;
    smk_cell_t c[2];
    if (strip)
        smk_strip(pg);
    else
        smk_head_line(pg);
    for (k = 0; k < 4u; k++) {
        uint32_t sig;
        for (p = 0; p < 2u; p++) {
            uint32_t h;
            smk_cell(pg, p, k, &c[p]);
            h = str_hash(str_hash(c[p].on * 3u + 1u, c[p].val), c[p].unit);
            if (key == smk_vkey && h != smk_vh[p][k])  /* (a value changed on the page: its knob's dot lights; a new page: nothing) */
                smk_turn_mark(p, k);
            smk_vh[p][k] = h;
        }
        sig = smk_cell_hash(&c[0]) * 31u + smk_cell_hash(&c[1]) + (uint32_t)smk_lit(0, k) * 101u + (uint32_t)smk_lit(1, k) * 211u + (uint32_t)strip * 7u + ux.gen * 13u;
        if (ui.force || sig != smk_row_sig[k]) {
            smk_row_sig[k] = sig;
            smk_row(k, strip ? SMK_SROW_Y + (int32_t)k * SMK_SROW_H : SMK_ROW_Y + (int32_t)k * SMK_ROW_H, strip ? SMK_SROW_H : SMK_ROW_H, c);
        }
    }
    smk_vkey = key;
}

/* ---- the packed pages' titles (owner 2026-11-18): "CHORUS|DELAY" in PAGES is drawn CHORUS . DELAY, a dot between the two names (the fonts have no middle dot); placed as head_title_ink
 * places a title (T, centred, kept in the room; S when T does not fit). 0: no '|', the title is drawn as ever */
static int smk_title_dot(const char *tb, int glob)
{
    char a[28], b[28];
    uint32_t i, j;
    int32_t lb = glob ? UT_TITLE_GL : UT_TITLE_L, rb = glob ? UT_TITLE_GR : UT_TITLE_R, wa, wb, gap = 13, w, x, ct;
    const aafont_t *f = &AF_T;
    for (i = 0; tb[i] && tb[i] != '|'; i++)
        ;
    if (!tb[i] || i >= sizeof a)
        return 0;
    for (j = 0; j < i; j++)
        a[j] = tb[j];
    a[i] = 0;
    str_cpy(b, tb + i + 1u, sizeof b);
    wa = text_w(f, a);
    wb = text_w(f, b);
    if (wa + gap + wb > rb - lb) {
        f = &AF_S;
        wa = text_w(f, a);
        wb = text_w(f, b);
    }
    w = wa + gap + wb;
    x = UT_TITLE_CX - w / 2;
    if (x + w > rb)
        x = rb - w;
    if (x < lb)
        x = lb;
    ct = UT_TITLE_BASE - Y_HEAD - AF_S_CAP_H + 1;     /* (the capitals' top row: T and S have the same cap height) */
    cv_text_on(x, UT_TY(S, UT_TITLE_BASE - Y_HEAD), f, a, T_TEXT, T_BG);
    cv_rrect(x + wa + gap / 2 - 2, ct + AF_S_CAP_H / 2 - 2, 4, 4, 2, T_TEXT, T_BG);
    cv_text_on(x + wa + gap, UT_TY(S, UT_TITLE_BASE - Y_HEAD), f, b, T_TEXT, T_BG);
    return 1;
}

/* ---- G1 (owner 2026-11-16 / 18): a GLOBAL page of eight cells, two columns, K1..K4 left and K5..K8 right, each cell its label over its value, in the global colour: mint (#6EE4A8) on VIOLET,
 * the theme's own global colour elsewhere. A cell the setting makes inapplicable is empty (its knob does nothing: ui_draw.c card_hidden_vp); the cells keep their knobs (no moving left) */
#ifdef SMK_GCOL_TEST
static uint16_t smk_gtest;
#endif
static uint16_t smk_gcol(void)                         /* the GLOBAL colour (owner 2026-11-19: a third colour on every theme, VIOLET's mint stronger) */
{
#ifdef SMK_GCOL_TEST
    if (smk_gtest)                                     /* (tests/ui_render.c SMK_GCOL: a colour tried in the pictures; not in a firmware build) */
        return smk_gtest;
#endif
    if (ux.bw)
        return vt(ux.gth, T_BG);
    switch (ux.pal % NPALETTES) {                      /* (owner 2026-11-20: from the colour wheel against the theme's two part colours, at their saturation and brightness) */
    case 0: return RGB(0x2C, 0xDC, 0xFF);              /* VIOLET: cyan, the triad of violet and pink (was mint #6EE4A8) */
    case 1: return RGB(0x1D, 0x99, 0xFF);              /* ORANGE: azure, the complement (was teal) */
    case 3: return RGB(0xFF, 0x3E, 0xA1);              /* COBALT: pink, the triad of its blues (was amber) */
    case 4: return RGB(0x8D, 0xBC, 0x6D);              /* PALE: sage, the complement, as muted as its parts (was dark green) */
    default: return vt(UI_PALETTES[ux.pal % NPALETTES].g, T_BG);   /* MINT, OCEAN, POLICE, GREY: the theme's own g (tools/gen_ui_palettes.py GLOBAL), kept */
    }
}
static int16_t *smk_gvp(uint32_t id)                   /* (params.c page_desc's SC_GLOBAL places) */
{
    return id == G_BM ? &song.bm : id >= G_WD ? &song.ms[0] : id > G_BM ? &song.cp[(id - G_CT) & 3u] : &song.g[id];
}
static const struct { uint8_t id; const char *lb; } SMK_G1_LB[] = {
    {G_WD, "STEREO WIDTH"}, {G_BM, "BASS MONO"}, {G_CR, "RATIO"}, {G_CT, "THRESHOLD"}, {G_CA, "ATTACK"}, {G_CL, "RELEASE"}, {SG_VOL, "MASTER VOLUME"}, {G_TUNE, "TUNE"},
    {G_CLOCK, "CLOCK"}, {G_BPM, "TEMPO"}, {G_SWING, "SWING"}, {SG_CLICK, "CLICK"}, {SG_CIN, "COUNT-IN"}, {SG_CLVL, "CLICK LEVEL"},
    {G_CRATE, "CHORUS RATE"}, {G_CDEPTH, "CHORUS DEPTH"}, {G_DMODE, "MODE"}, {G_DTIME, "TIME"}, {G_DFDBK, "FEEDBACK"}, {G_DCOLOR, "COLOR"}, {G_PINGSPREAD, "SPREAD"}, {G_DMIX, "MIX"},
    {G_DMRATE, "DELAY MOD RATE"}, {G_DMDEP, "DELAY MOD DEPTH"}, {G_RTYPE, "TYPE"}, {G_RSIZE, "SIZE"}, {G_RDAMP, "DAMP"}, {G_RLOCUT, "LOW CUT"}, {G_SHPITCH, "SHIMMER PITCH"}, {G_SHMOD, "SHIMMER MOD"}};
static const char *smk_g1_label(uint32_t id)
{
    uint32_t i;
    if (id == G_DMDEP && song.g[G_DMODE] >= DMD_RAND)  /* (RANDOM and OCTAVE UP: the knob is the events' CHANCE, as on the FM-1) */
        return "DELAY CHANCE";
    for (i = 0; i < NELEM(SMK_G1_LB); i++)
        if (SMK_G1_LB[i].id == id)
            return SMK_G1_LB[i].lb;
    return "";
}
static int smk_g1_byp(uint32_t id)                     /* (BYPASS of the shared effects, as ui_draw.c byp_card by the page) */
{
    uint32_t b = id == G_CRATE || id == G_CDEPTH ? BYS_CHO : id == G_BM || id == G_WD ? BYS_MST : id >= G_CT && id <= G_CL ? BYS_CMP
               : id == G_RTYPE || id == G_RSIZE || id == G_RDAMP || id == G_RLOCUT || id == G_SHPITCH || id == G_SHMOD ? BYS_REV
               : id == G_DMODE || id == G_DTIME || id == G_DFDBK || id == G_DCOLOR || id == G_PINGSPREAD || id == G_DMRATE || id == G_DMDEP || id == G_DMIX ? BYS_DLY : 0u;
    return (byp_sh & b) != 0u;
}
typedef struct {
    char lb[24], val[PV_LEN];
    const char *unit;
    int32_t ratio;
    uint8_t on, dim, ctr;
} smk_g1c_t;
static uint32_t smk_g1_id(const page_t *pg, uint32_t k) { return k < 4u ? pg->id[k] : pg->id2[k & 3u]; }
static void smk_g1_cell(const page_t *pg, uint32_t k, smk_g1c_t *c)
{
    uint32_t id = smk_g1_id(pg, k);
    const param_desc_t *d;
    int16_t *vp;
    c->on = c->dim = c->ctr = 0;
    c->lb[0] = c->val[0] = 0;
    c->unit = "";
    c->ratio = -1;
    if (pg->graph == GR_TEMPO && k == TEMPO_TAP_SLOT) {   /* TAP: the taps' tempo for 2 s after the last, else TAP (an external clock: no cell), as the FM-1's card */
        uint32_t n = tempo_tapping();
        if (clock_src())
            return;
        str_cpy(c->lb, "TAP", sizeof c->lb);
        if (n >= 2u) {
            fmt_int(c->val, song.g[G_BPM]);
            c->unit = "BPM";
        } else
            str_cpy(c->val, n ? "1" : "TAP", sizeof c->val);
        c->on = 1;
        return;
    }
    if (id == 0xFFu || id == SG_NONE)
        return;
    str_cpy(c->lb, smk_g1_label(id), sizeof c->lb);
    if (id == SG_VOL) {
        uint32_t v = smk_volume();
        fmt_int(c->val, (int32_t)v);
        c->unit = "%";
        c->ratio = (int32_t)v * 10;
        c->on = 1;
        return;
    }
    if (id == SG_CLICK || id == SG_CIN || id == SG_CLVL) {
        uint32_t f = id == SG_CLICK ? RP_CLICK : id == SG_CIN ? RP_COUNTIN : RP_LEVEL, v = rp_get(f);
        str_cpy(c->val, (id == SG_CLICK ? CLICK_N : id == SG_CIN ? COUNTIN_N : CLKLVL_N)[v % 3u], sizeof c->val);
        c->on = 1;
        return;
    }
    vp = smk_gvp(id);
    d = glob_desc(id);
    if (!d || card_hidden_vp(vp))
        return;
    c->on = 1;
    if (smk_g1_byp(id)) {
        str_cpy(c->val, "BYPASS", sizeof c->val);
        c->dim = 1;
        return;
    }
    if (id == G_DMODE) {                               /* (owner 2026-11-18: the MODE in full words) */
        static const char *const DM[DMD_N] = {"DIGITAL", "PING-PONG", "RANDOM", "OCTAVE UP"};
        str_cpy(c->val, DM[(uint32_t)clamp(*vp, 0, DMD_N - 1)], sizeof c->val);
        return;
    }
    param_format(d, *vp, c->val, &c->unit);
    if (!c->unit)
        c->unit = "";
    if (id == G_RTYPE && str_eq(c->val, "SHIM"))
        str_cpy(c->val, "SHIMMER", sizeof c->val);
    if (d->fmt != F_ENUM)
        c->ratio = RATIO(d, enum_rank(d, *vp));
    if (c->val[0] == '.' || (c->val[0] == '-' && c->val[1] == '.')) {   /* ".53" -> "0.53" */
        char z[PV_LEN];
        uint32_t neg = c->val[0] == '-';
        str_cpy(z, c->val + neg, sizeof z);
        c->val[neg] = '0';
        str_cpy(c->val + neg + 1u, z, sizeof c->val - neg - 1u);
    }
}
#define SMK_CELL_X 6                                   /* a G1 cell's text from x 6 of its half; its dot ends at x 114 */
static void smk_g1_paint(uint32_t k, const smk_g1c_t *c, int lit)
{
    const int32_t lbase = 15, vb = 39, bar_y = 43, room = 114 - SMK_CELL_X - AF_S_CAP_H - 6;
    uint16_t gc = smk_gcol(), rule = ux_mix(T_BG, T_TEXT, 14);
    char one[PV_LEN];
    int32_t w;
    const aafont_t *f;
    cv_begin(120, SMK_ROW_H, T_BG);
    cv_rect(k < 4u ? 4 : 0, 0, 116, 1, rule);
    if (c->on) {
        cv_text_on(SMK_CELL_X, UT_TY(S, lbase), &AF_S, c->lb, gc, T_BG);
        smk_dot(114 - AF_S_CAP_H, vb - AF_S_CAP_H + 1, k, lit ? gc : T_TEXT);
        smk_one_line(one, c->val);
        f = smk_vface(one, c->unit, room, 1, &w);
        smk_val_draw(SMK_CELL_X, 0, vb, f, one, c->unit, c->dim ? T_DIM : T_TEXT);
        if (c->ratio >= 0 && !c->dim) {
            int32_t fx = (c->ratio * room + 500) / 1000;
            cv_rrect(SMK_CELL_X, bar_y, room, UT_CARD_BAR_H, UT_CARD_BAR_R, ux_mix(T_BASE, gc, UT_CARD_BAR_REST_PCT), T_BG);
            cv_rrect(SMK_CELL_X, bar_y, fx < UT_CARD_BAR_MIN ? UT_CARD_BAR_MIN : fx, UT_CARD_BAR_H, UT_CARD_BAR_R, gc, ux_mix(T_BASE, gc, UT_CARD_BAR_REST_PCT));
        }
    }
    cv_blit(k < 4u ? 0u : 120u, (uint32_t)(SMK_ROW_Y + (int32_t)(k & 3u) * SMK_ROW_H));
}
static uint32_t smk_g1_sig[8], smk_g1_vh[8], smk_g1_key, smk_g1_ms[8];
static void smk_g1_draw(void)
{
    const page_t *pg = cur_page();
    uint32_t k, key = (uint32_t)ui.page * 7919u + 3u;
    smk_g1c_t c;
    smk_head_line(pg);
    for (k = 0; k < 8u; k++) {
        uint32_t h, sig;
        int lit;
        smk_g1_cell(pg, k, &c);
        h = str_hash(str_hash(c.on * 3u + 1u, c.val), c.unit);
        if (key == smk_g1_key && h != smk_g1_vh[k])
            smk_g1_ms[k] = fm1_ms | 1u;                /* (a value changed: its dot in the global colour for a moment) */
        smk_g1_vh[k] = h;
        lit = smk_g1_ms[k] && fm1_ms - smk_g1_ms[k] < SMK_LIT_MS;
        if (!lit)
            smk_g1_ms[k] = 0;
        sig = str_hash(str_hash(str_hash(c.on * 7u + c.dim * 13u + (uint32_t)(c.ratio + 1) * 977u + (uint32_t)lit * 31u + ux.gen * 101u, c.lb), c.val), c.unit);
        if (ui.force || sig != smk_g1_sig[k]) {
            smk_g1_sig[k] = sig;
            smk_g1_paint(k, &c, lit);
        }
    }
    smk_g1_key = key;
}

/* ---- PERFORM (owner 2026-11-18, NOTE REPEAT): the eight knobs; a pad held shows the pads' picture on any page (smk_pads_draw) (from the pads'
 * pressure, perf_depth) as a number and a bar; then the eight knobs, any parameter (NOTE REPEAT held + a knob turned on any page puts that knob's parameter on the same knob here:
 * smk_ui.c; ui.c smk_pf_*). No DEPTH or MORPH knob (the pressure and the faders do them) */
static int perf_can(uint32_t e);                       /* (ui_perform.c) */
static const char *const SMK_PF_FULL[16] = {"REPEAT 1/8", "REPEAT 1/16", "REPEAT 1/32", "REVERSE", "TAPE STOP", "FREEZE", "OCTAVE UP", "OCTAVE DOWN",
                                            "FLANGER", "PHASER", "DRIVE", "PUMP", "ROLL A", "ROLL B", "MUTE 1", "MUTE 2"};
#define SMK_PF_PAD_Y 46                                /* (owner 2026-11-18: the pads only while one is held, a 4 x 4 grid; else the knobs fill the page) */
#define SMK_PF_PAD_X 2
#define SMK_PF_PAD_W 59
#define SMK_PF_PAD_H 36
#define SMK_PF_DEPTH_Y (SMK_PF_PAD_Y + 4 * SMK_PF_PAD_H + 6)
#define SMK_PF_CELL_Y SMK_PF_PAD_Y
#define SMK_PF_CELL_H 48
#define SMK_PF_TXT 9                                    /* (the label and value inside a 48 px row: centred) */
/* PERFORM knob k's name ("MULTI CUTOFF 1": the parameter, its part's number) into b (24) */
static void smk_pk_name(uint32_t kind, uint32_t part, uint32_t id, char *b)   /* (also the MENU's PERFORM KNOB rows: menu_items.c) */
{
    char n[24];
    b[0] = 0;
    if (kind == SPK_MACRO) {
        str_cpy(b, MAC_NAME[id % (MAC_N + 1u)], 24);
        if (id == MAC_SPACE || id == MAC_KILL)
            return;
    } else if (kind == SPK_GLOBAL) {
        str_cpy(b, smk_g1_label(id)[0] ? smk_g1_label(id) : desc_shown(glob_desc(id)), 24);
        return;
    } else if (kind == SPK_TRACK) {
        const char *l = id == P_FREEA ? "MULTI CUTOFF" : id == P_FREEB ? "MULTI RES" : id == P_DIST ? "DRIVE" : id == P_ITYPE ? "INSERT TYPE" : id == P_DKSRC ? "DUCK SOURCE" : 0;
        if (!l) {
            smk_full_words(n, desc_shown(track_desc(&trk[part], id)), sizeof n);
            smk_one_line(b, n);
        } else
            str_cpy(b, l, 24);
    } else
        return;
    if (str_len(b) + 3u < 24u) {
        str_cpy(b + str_len(b), part ? " 2" : " 1", 3);
    }
}
static void smk_pf_name(uint32_t k, char *b)
{
    uint32_t part, id, kind = smk_pf_get(k, &part, &id);
    smk_pk_name(kind, part, id, b);
}
/* .. and its value (val PV_LEN), unit, bar (-1 none), a bipolar bar */
static void smk_pf_value(uint32_t k, char *val, const char **unit, int32_t *ratio, int *ctr)
{
    uint32_t part, id, kind = smk_pf_get(k, &part, &id);
    int32_t x;
    *unit = "";
    *ratio = -1;
    *ctr = 0;
    val[0] = 0;
    if (kind == SPK_MACRO) {
        if (id == MAC_MORPH) {
            uint32_t f = morph_fader_of(part);
            if (f == 0u || f == 127u)
                str_cpy(val, f ? "B" : "A", PV_LEN);
            else
                fmt_int(val, (int32_t)f);
            *ratio = (int32_t)f * 1000 / 127;
            return;
        }
        x = id == MAC_SPACE ? perf_space : id == MAC_KILL ? perf_filt : mac_v[part % NLIVE][(id - MAC_TIMBRE) % MAC_NPART];
        if (id == MAC_KILL) {
            if (x)
                fmt_int(val, x < 0 ? -x : x);
            else
                str_cpy(val, "OFF", PV_LEN);
            *unit = x < 0 ? "LP" : x > 0 ? "HP" : "";
        } else {
            val[0] = x < 0 ? '-' : '+';
            fmt_int(val + (x != 0), x < 0 ? -x : x);
        }
        *ratio = (x + 100) * 5;
        *ctr = 1;
        return;
    }
    if (kind == SPK_GLOBAL) {
        smk_g1c_t c;
        page_t g = {"", FAM_GLO, SC_GLOBAL, GR_NONE, {(uint8_t)id, 0xFF, 0xFF, 0xFF}, SMK_G8, {0xFF, 0xFF, 0xFF, 0xFF}};
        smk_g1_cell(&g, 0, &c);
        str_cpy(val, c.on ? c.val : "--", PV_LEN);
        *unit = c.on ? c.unit : "";
        *ratio = c.on ? c.ratio : -1;
        return;
    }
    if (kind == SPK_TRACK) {
        track_t *t = &trk[part];
        const param_desc_t *d = track_desc(t, id);
        int32_t v = smk_vv(&t->p[id], part, t);
        if (!d) {
            str_cpy(val, "--", PV_LEN);
            return;
        }
        param_format(d, (int16_t)v, val, unit);
        if (!*unit)
            *unit = "";
        if (d->fmt != F_ENUM)
            *ratio = RATIO(d, enum_rank(d, (int16_t)v));
    }
}
static uint32_t smk_pf_sig[8], smk_pf_top_sig;
/* the pads while one is held (owner 2026-11-18): a 4 x 4 grid, every name in full (on two lines where one does not fit), the held one lit, one that cannot run now dim;
 * DEPTH under it. No pad held: only the eight knobs (no strip, no DEPTH) */
static void smk_pf_pads(uint32_t held)
{
    uint32_t p;
    for (p = 0; p < 16u; p++) {
        const char *n = SMK_PF_FULL[p];
        char l1[16], l2[16];
        int lit = (int)((held >> p) & 1u), can = perf_can(p) != 0;
        uint16_t fc = lit ? T_ACCENT : can ? ux_mix(T_BASE, T_THEME, 25) : ux_mix(T_BASE, T_DIM, 30), tc = lit || can ? T_TEXT : T_DIM;
        uint32_t i, sp = 0;
        cv_begin(SMK_PF_PAD_W, SMK_PF_PAD_H, T_BG);
        cv_rrect(0, 0, SMK_PF_PAD_W - 2, SMK_PF_PAD_H - 2, 3, fc, T_BG);
        for (i = 0; n[i]; i++)
            if (n[i] == ' ')
                sp = i;
        if (!sp || text_w(&AF_S, n) <= SMK_PF_PAD_W - 6)   /* (one word stays whole: REVERSE, FLANGER) */
            cv_text_in(0, UT_TY(S, (SMK_PF_PAD_H - 2 + AF_S_CAP_H) / 2), SMK_PF_PAD_W - 2, &AF_S, n, tc, fc);
        else {
            str_cpy(l1, n, sp + 1u < sizeof l1 ? sp + 1u : sizeof l1);
            str_cpy(l2, n + sp + 1u, sizeof l2);
            cv_text_in(0, UT_TY(S, (SMK_PF_PAD_H - 2) / 2 - 2), SMK_PF_PAD_W - 2, &AF_S, l1, tc, fc);
            cv_text_in(0, UT_TY(S, (SMK_PF_PAD_H - 2) / 2 + AF_S_CAP_H + 2), SMK_PF_PAD_W - 2, &AF_S, l2, tc, fc);
        }
        cv_blit(SMK_PF_PAD_X + (p & 3u) * SMK_PF_PAD_W, SMK_PF_PAD_Y + (p >> 2) * SMK_PF_PAD_H);
    }
}
static void smk_perf_draw(void)
{
    uint32_t k, sig;
    uint16_t gc = smk_gcol(), rule = ux_mix(T_BG, T_TEXT, 14);
    smk_head_line(cur_page());
    for (k = 0; k < 8u; k++) {                         /* the eight knobs: K1..K4 left, K5..K8 right; a part's value in its colour, a global one in the global colour */
        char nm[24], val[PV_LEN];
        const char *unit;
        int32_t ratio;
        int ctr;
        uint32_t part, id, kind = smk_pf_get(k, &part, &id);
        uint16_t vc = kind == SPK_TRACK || (kind == SPK_MACRO && id != MAC_SPACE && id != MAC_KILL) ? vt(T_PTHEME(part), T_BG) : gc;
        smk_pf_name(k, nm);
        smk_pf_value(k, val, &unit, &ratio, &ctr);
        sig = str_hash(str_hash(str_hash(kind * 7u + part * 13u + id * 31u + ux.gen * 101u + (uint32_t)(ratio + 1) * 977u, nm), val), unit);
        if (!ui.force && sig == smk_pf_sig[k])
            continue;
        smk_pf_sig[k] = sig;
        cv_begin(120, SMK_PF_CELL_H, T_BG);
        if (k & 3u)
            cv_rect(k < 4u ? 4 : 0, 0, 116, 1, rule);
        if (kind != SPK_NONE) {
            int32_t x;
            smk_dot(SMK_CELL_X, SMK_PF_TXT + 3, k, T_TEXT);
            cv_text_on(SMK_CELL_X + AF_S_CAP_H + 5, UT_TY(S, SMK_PF_TXT + 13), &AF_S, nm, T_TEXT, T_BG);
            x = cv_text_on(SMK_CELL_X + AF_S_CAP_H + 5, UT_TY(T, SMK_PF_TXT + 27), &AF_T, val, vc, T_BG);
            if (unit[0])
                cv_text_on(x, UT_TY(M, SMK_PF_TXT + 27), &AF_M, unit, T_MID, T_BG);
        }
        cv_blit(k < 4u ? 0u : 120u, (uint32_t)(SMK_PF_CELL_Y + (int32_t)(k & 3u) * SMK_PF_CELL_H));
    }
}

/* the parameter knob k (0..7) edits on the page shown, for PERFORM (smk_ui.c: NOTE REPEAT held + a knob): a part page's card (with its part), a G1 cell, a MACROS knob. 0: none there */
static uint32_t smk_target(uint32_t k, uint32_t *part, uint32_t *id)
{
    const page_t *pg = cur_page();
    *part = (k >> 2) & 1u;
    if (ui.menu || ui.layer || ui.perf || page_over)
        return SPK_NONE;
    if (pg->g8) {
        uint32_t g = smk_g1_id(pg, k);
        if ((pg->graph == GR_TEMPO && k == TEMPO_TAP_SLOT) || g == 0xFFu || g == SG_NONE || g == SG_CLICK || g == SG_CIN || g == SG_CLVL || g == G_CLOCK)
            return SPK_NONE;
        *id = g;
        return SPK_GLOBAL;
    }
    if (pg->scope == SC_MACRO) {
        uint32_t m = (pg->graph == GR_MAC2 ? 4u : 0u) + (k & 3u);
        if (m > MAC_KILL)
            return SPK_NONE;
        *id = m;
        return SPK_MACRO;
    }
    if (pg->graph == GR_MORPH)                         /* (MORPH's controls are no parameters) */
        return SPK_NONE;
    if (smk_twin_page(pg)) {
        track_t *t = &trk[*part];
        uint8_t vis[4];
        uint32_t n = smk_cards(pg, t, vis), s = (k & 3u) < n ? vis[k & 3u] : 0xFFu, pid;
        int16_t *vp;
        const param_desc_t *d;
        if (s > 3u || (pg->graph == GR_ENGINE && s == 0u) || (s == DUCK_LINK_SLOT && smk_duck_midi(pg, t)))
            return SPK_NONE;
        d = smk_desc(pg, s, t, &vp);
        if (!d || !vp || d->max == d->min || (d->label && d->label[0] == '-'))
            return SPK_NONE;
        pid = (uint32_t)(vp - t->p);
        if (pid >= EXT_P_LEN)
            return SPK_NONE;
        *id = pid;
        return SPK_TRACK;
    }
    return SPK_NONE;
}

/* ---- the faders' pictures (owner 2026-11-17): while fader 3 or 4 moves, the MORPH page (both parts' pictures); while fader 1 or 2 moves, that part's level, a big number and a bar in the
 * part's colour; the page comes back SMK_OV_MS after the last move. Drawn as the pages are: what changed only (the level's number and bar; MORPH's own signatures) */
#define SMK_OV_MS 1000u
static uint8_t smk_ov;                                 /* 1 LEVEL 1, 2 LEVEL 2, 3 MORPH (smk_ov_shown: ui_graph.c); 4 the pads (smk_pads_on) */
static uint32_t smk_ov_ms, smk_lv_sig;
static uint8_t smk_ov_block;                           /* a knob ended a fader's picture: the same fader motion does not bring it back (it flickered, owner 2026-11-18) */
static uint32_t smk_fdm_ms;                            /* the last MORPH fader step (smk_fd_busy) */
static void smk_fader_moved(uint32_t f)
{
    if (f >= 2u)
        smk_fdm_ms = fm1_ms | 1u;
    if (smk_ov_block && fm1_ms - smk_ov_ms <= SMK_OV_MS) {   /* (still the motion the knob interrupted: no picture, the page stays) */
        smk_ov_ms = fm1_ms;
        return;
    }
    smk_ov_block = 0;
    smk_ov = (uint8_t)(f < 2u ? f + 1u : 3u);
    smk_ov_ms = fm1_ms;
}
/* a MORPH fader moved less than SMK_OV_MS ago: the part colour (A -> B) waits until it rests, so a fader step never redraws the whole page (ui_draw.c; owner 2026-11-18: flicker) */
static int smk_fd_busy(void)
{
    return smk_fdm_ms && fm1_ms - smk_fdm_ms <= SMK_OV_MS;
}
/* the pads' picture (owner 2026-11-18): on any page while a pad is held (ROLL A / B, which only fire: for SMK_OV_MS), a 4 x 4 grid with every name and DEPTH;
 * a knob turned ends it until the next pad press */
static uint32_t smk_pad_lit, smk_pad_ms;
static uint8_t smk_pad_block;
static uint32_t smk_pads_on(void)
{
    uint32_t lit = perf_held & ~PF_ROLL;
    if (smk_pad_lit && fm1_ms - smk_pad_ms <= SMK_OV_MS)
        lit |= smk_pad_lit & PF_ROLL;
    return smk_pad_block ? 0u : lit;
}
static void smk_level_draw(uint32_t p)
{
    char v[PV_LEN], lb[12];
    const char *u = "";
    int32_t lv = trk[p & 1u].p[P_LEVEL], w, fx;
    uint16_t pc = vt(T_PTHEME(p), T_BG);
    uint32_t sig = (uint32_t)lv * 31u + p * 7u + ux.gen * 101u;
    if (ui.force) {                                    /* (only the rows no canvas covers: no black flash) */
        lcd_fill(0, H_HEAD, 240, 70 - H_HEAD, T_BG);
        lcd_fill(0, 190, 240, 50, T_BG);
    }
    draw_head();
    if (!ui.force && sig == smk_lv_sig)
        return;
    smk_lv_sig = sig;
    param_format(&TP[P_LEVEL], (int16_t)lv, v, &u);
    if (!u)
        u = "";
    str_cpy(lb, p ? "LEVEL 2" : "LEVEL 1", sizeof lb);
    fx = (int32_t)RATIO(&TP[P_LEVEL], lv) * 200 / 1000;
    CV_BANDS(240, 120, T_BG) {                         /* (taller than the unit's canvas: in bands, gfx.c CV_BANDS; cut short it left a hole, owner 2026-11-18) */
        const aafont_t *f = smk_vface(v, u, 200, 1, &w);
        cv_text_in(0, UT_TY(T, 20), 240, &AF_T, lb, T_TEXT, T_BG);
        smk_val_draw(120 - w / 2, 0, 70, f, v, u, pc);
        cv_rrect(20, 90, 200, 6, 3, ux_mix(T_BASE, pc, UT_CARD_BAR_REST_PCT), T_BG);
        cv_rrect(20, 90, fx < 3 ? 3 : fx, 6, 3, pc, ux_mix(T_BASE, pc, UT_CARD_BAR_REST_PCT));
        cv_blit(0, 70u + (uint32_t)cv_by);
    }
    ui.force = 0;
}
/* a MORPH fader moving (owner 2026-11-18): only the two parts' MORPH lines, each over half the screen (part 1 above, part 2 below) with its name and the fader's
 * place (A, B or the number) in the part's colour; no knob cell, so the screen never shows knobs that do something else (a knob turned ends the picture: smk_ui.c smk_knob) */
#define SMK_MO_H ((240 - H_HEAD - 2) / 2)
static uint32_t smk_mo_sig;
static void smk_morph_draw(const page_t *mp)
{
    uint32_t tk = mv_tick(), n, k, sig = tk * 2654435761u ^ morph_fader_of(0) * 40503u ^ morph_fader_of(1) * 977u ^ ux.gen * 101u;
    if (ui.force) {                                    /* (only the rows no canvas covers: no black flash) */
        lcd_fill(0, H_HEAD, 240, 2, T_BG);
        lcd_fill(0, H_HEAD + 2 + 2 * SMK_MO_H, 240, 240 - H_HEAD - 2 - 2 * SMK_MO_H, T_BG);
    }
    smk_pg_over = mp;                                  /* (the header says MORPH) */
    draw_head();
    smk_pg_over = 0;
    if (!ui.force && sig == smk_mo_sig)
        return;
    smk_mo_sig = sig;
    mv_levels(tk);
    n = mv_step == 0xFFFFFFFFu ? 0u : tk - mv_step;
    mv_listen(0, n > 4u ? 4u : n);
    mv_listen(1, n > 4u ? 4u : n);
    mv_step = tk;
    for (k = 0; k < 2u; k++) {
        uint32_t f = morph_fader_of(k);
        char v[8];
        if (f == 0u || f == 127u)
            str_cpy(v, f ? "B" : "A", sizeof v);
        else
            fmt_int(v, (int32_t)f);
        CV_BANDS(240, SMK_MO_H, T_BG) {                /* (taller than the unit's canvas: in bands; cut short it left a hole and broke the lines, owner 2026-11-18) */
            cv_text_on(8, UT_TY(S, 14), &AF_S, k ? "MORPH 2" : "MORPH 1", T_TEXT, T_BG);
            cv_text_r(232, UT_TY(T, 16), &AF_T, v, vt(T_PTHEME(k), T_BG), T_BG);
            mv_field(k, 22, SMK_MO_H - 4, 5u);
            cv_blit(0, (uint32_t)(H_HEAD + 2 + (int32_t)k * SMK_MO_H + cv_by));
        }
    }
    ui.force = 0;
}
static void smk_pads_draw(uint32_t held)
{
    uint32_t p, sig = held * 2654435761u ^ (uint32_t)(uint8_t)perf_depth * 40503u ^ ux.gen * 101u;
    if (ui.force) {                                    /* (the rows and edges no canvas covers) */
        lcd_fill(0, H_HEAD, 240, SMK_PF_PAD_Y - H_HEAD, T_BG);
        lcd_fill(0, SMK_PF_PAD_Y, SMK_PF_PAD_X, 4 * SMK_PF_PAD_H, T_BG);
        lcd_fill(SMK_PF_PAD_X + 4 * SMK_PF_PAD_W, SMK_PF_PAD_Y, 240 - SMK_PF_PAD_X - 4 * SMK_PF_PAD_W, 4 * SMK_PF_PAD_H, T_BG);
        lcd_fill(0, SMK_PF_PAD_Y + 4 * SMK_PF_PAD_H, 240, SMK_PF_DEPTH_Y - SMK_PF_PAD_Y - 4 * SMK_PF_PAD_H, T_BG);
    }
    draw_head();
    for (p = 0; p < 16u; p++)
        sig = (sig ^ (uint32_t)perf_can(p)) * 16777619u;
    if (!ui.force && sig == smk_pf_top_sig)
        return;
    smk_pf_top_sig = sig;
    smk_pf_pads(held);
    cv_begin(240, 240 - SMK_PF_DEPTH_Y, T_BG);         /* DEPTH: the label, the bar, the number (ROLL alone: no pressure, empty) */
    {
        char v[8];
        int32_t dp = held & ~PF_ROLL ? perf_depth : 0;
        int32_t bx = 8 + text_w(&AF_S, "DEPTH") + 10, bw = 232 - 48 - bx, fx = dp * bw / 100;
        fmt_int(v, dp);
        cv_text_on(8, UT_TY(S, 15), &AF_S, "DEPTH", T_TEXT, T_BG);
        cv_rrect(bx, 8, bw, 5, 2, ux_mix(T_BASE, T_THEME, UT_CARD_BAR_REST_PCT), T_BG);
        if (fx > 0)
            cv_rrect(bx, 8, fx < 3 ? 3 : fx, 5, 2, T_THEME, ux_mix(T_BASE, T_THEME, UT_CARD_BAR_REST_PCT));
        {
            int32_t x = 232 - text_w(&AF_M, "%") - text_w(&AF_T, v);
            x = cv_text_on(x, UT_TY(T, 15), &AF_T, v, T_TEXT, T_BG);
            cv_text_on(x, UT_TY(M, 15), &AF_M, "%", T_MID, T_BG);
        }
    }
    cv_blit(0, SMK_PF_DEPTH_Y);
    ui.force = 0;
}
/* ---- the stacked pages (owner 2026-11-18, the two-part pages design): SEQUENCER, PARAMETER LOCKS, PATCHES, PATTERNS, TOOLS show both parts, part 1 above with
 * knobs 1 to 4, part 2 below with knobs 5 to 8. Each half: the part's content (its roll, its locks, its list, its tools) in its colour, then its four knobs: the
 * numbered dot and the label, the value under them. The cards say what the FM-1's cards say (draw_columns run for that part with smk_cap_on); each part keeps its
 * own cursor, lock row and pattern pick (ui.c smk_sq). MULTIS stays one list (a multi is both parts) */
#define SMK_ST_Y0 24                                   /* half p: rows SMK_ST_Y0 + p * SMK_ST_H .. + SMK_ST_H - 1 */
#define SMK_ST_H 108
#define SMK_ST_KY 72                                   /* (in the half) the knob line's first row */
#define SMK_ST_CW 60                                   /* a knob's column */
static void draw_columns(void);                         /* (ui_draw.c) */
static int smk_stack_page(const page_t *pg)
{
    uint32_t g = pg->graph;
    if (ui.menu || ui.layer || ui.perf || page_over || ui.guide || off_module(pg))
        return 0;
    return g == GR_ROLL || g == GR_EVENTS || g == GR_BROWSE || g == GR_PATS || g == GR_TOOLS;
}
/* the screen as part p sees it: its cursor, rows and pick live, its colour as THEME; smk_st_out puts everything back */
static struct { uint8_t sel, pend, swapped; uint16_t th, sl, tn, ln; } smk_vw;
static void smk_st_in(uint32_t p)
{
    smk_vw.sel = song.sel;
    smk_vw.th = ux.theme; smk_vw.sl = ux.sel; smk_vw.tn = ux.tint; smk_vw.ln = ux.lane;
    smk_vw.swapped = p != song.sel;
    if (smk_vw.swapped) {
        smk_sq_put(&smk_sq[song.sel & 1u]);
        smk_sq_get(&smk_sq[p & 1u]);
        smk_vw.pend = pl_pend_on;
        pl_pend_on = 0;
        song.sel = (uint8_t)p;
    }
    ux.theme = ux.pth[p & 1u]; ux.sel = ux.psel[p & 1u]; ux.tint = ux.ptint[p & 1u]; ux.lane = ux.plane[p & 1u];
}
static void smk_st_out(void)
{
    if (smk_vw.swapped) {
        smk_sq_put(&smk_sq[song.sel & 1u]);
        smk_sq_get(&smk_sq[smk_vw.sel & 1u]);
        pl_pend_on = smk_vw.pend;
        song.sel = smk_vw.sel;
    }
    ux.theme = smk_vw.th; ux.sel = smk_vw.sl; ux.tint = smk_vw.tn; ux.lane = smk_vw.ln;
}
/* part p's four cards, as the FM-1 page would show them (TOOLS: its own rows, no cards) */
static void smk_st_cards(void)
{
    uint8_t g = grid_on;
    uint32_t k;
    for (k = 0; k < 4u; k++)
        smk_cap[k].on = 0;
    if (cur_page()->graph == GR_TOOLS)
        return;
    grid_on = 16;                                      /* (the FM-1's AREA template: its cards' wording, the OCT+ pills) */
    smk_cap_on = 1;
    draw_columns();
    smk_cap_on = 0;
    grid_on = g;
}
/* PATCHES, PATTERNS: the FM-1's list (ui_graph.c graph_browse, graph_pats) with three rows instead of five, the picked one in the middle (at the list's ends the first or
 * last three), moved into the half */
#define SMK_ST_LTOP 6                                  /* (in the half) the first row's top */
static void smk_st_list(void)
{
    uint8_t g = grid_on;
    grid_on = 16;                                      /* (AREA_PIC: the list of the AREA template) */
    smk_l5_rows = 3;
    cv_oy = SMK_ST_LTOP - UT_L5_Y0;
    if (cur_page()->graph == GR_BROWSE)
        graph_browse();
    else
        graph_pats();
    cv_oy = 0;
    smk_l5_rows = 5;
    grid_on = g;
}
/* part p's roll: the 16 steps of its bank across the screen, its notes in the part's colour (the cursor step's full, the others dimmer), the cursor's frame, the
 * step playing, a lock's mark under its step */
#define SMK_RL_X0 8
#define SMK_RL_CW 14
#define SMK_RL_Y 4
#define SMK_RL_H 60
static void smk_st_roll(const track_t *t, uint32_t p)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = (uint32_t)ui.bank * 16u, ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    uint64_t locks = motion_lock_steps(p);
    int32_t lo = 127, hi = -1, span, bh, gw = (int32_t)ncol * SMK_RL_CW + 1;
    uint16_t pc = T_THEME, dimc = ux_mix(T_BASE, pc, UT_T7_NOTE_PCT);
    for (i = 0; i < ncol; i++) {                      /* the notes' range: at least an octave, centred on them */
        uint32_t s = pr_src(t, base + i, len);
        if (s == NSTEP)
            continue;
        for (j = 0; j < seq_steps(t)[s].n; j++) {
            int32_t n = seq_steps(t)[s].note[j];
            if (n < lo) lo = n;
            if (n > hi) hi = n;
        }
    }
    if (hi < 0)
        lo = hi = 60;
    if (hi - lo < 11) {
        int32_t m = (lo + hi) / 2;
        lo = m - 6;
        hi = lo + 11;
    }
    span = hi - lo + 1;
    bh = SMK_RL_H / span > 3 ? SMK_RL_H / span : 3;
    cv_rect(SMK_RL_X0, SMK_RL_Y, gw, SMK_RL_H, ux_mix(T_BG, pc, 10));
    for (i = 0; i <= ncol; i++)                       /* the step lines, the beats brighter */
        cv_rect(SMK_RL_X0 + (int32_t)i * SMK_RL_CW, SMK_RL_Y, 1, SMK_RL_H, i % 4u ? T_GRID : ux_mix(T_BASE, T_TEXT, UT_T7_KEYW_PCT));
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i, s = pr_src(t, si, len);
        int32_t x = SMK_RL_X0 + (int32_t)i * SMK_RL_CW;
        if (song.playing && si == t->seq_idx)
            cv_rect(x + SMK_RL_CW / 2, SMK_RL_Y, 1, SMK_RL_H, T_ACCENT);
        if (s != NSTEP) {
            const step_t *st = &seq_steps(t)[s];
            uint16_t col = s == ui.cursor || si == ui.cursor ? pc : dimc;
            for (j = 0; j < st->n; j++) {
                int32_t y = SMK_RL_Y + (hi - st->note[j]) * (SMK_RL_H - bh) / (span - 1 > 0 ? span - 1 : 1);
                if (s == si)
                    cv_rect(x + 2, y, SMK_RL_CW - 3, bh, (st->flags & SF_ACCENT) ? T_TEXT : col);
                else                                      /* a TIE: the bar on through the line before */
                    cv_rect(x - 1, y, SMK_RL_CW + 1, bh, col);
            }
        }
        if (si == ui.cursor)
            cv_frame(x, SMK_RL_Y - 2, SMK_RL_CW + 1, SMK_RL_H + 4, T_TEXT);
        if ((locks >> si) & 1u)
            cv_rect(x + 3, SMK_RL_Y + SMK_RL_H + 3, SMK_RL_CW - 6, 2, T_ACCENT);
    }
}
/* TOOLS: part p's CLEAR PATTERN and INITIALIZE SOUND (knobs 1, 2 / 5, 6), part 1's half also RESTORE ALL PATCHES (knob 3; it acts on every slot), the OCT+ pill that does
 * the one picked; the picked one's name in the accent */
static void smk_st_tools(uint32_t p)
{
    static const char *const NM[3] = {"CLEAR PATTERN", "INITIALIZE SOUND", "RESTORE ALL PATCHES"};
    uint32_t k, n = p ? 2u : 3u;
    int restore = up_count() != 0u;
    for (k = 0; k < n; k++) {
        int32_t top = 8 + (int32_t)k * 32, on = k < 2u || restore, picked = act_col() == k + 1u && song.sel == p;
        uint16_t c = !on ? T_DIM : picked ? T_ACCENT : T_TEXT;
        smk_dot(9, top, k + 4u * p, on ? T_TEXT : T_DIM);
        cv_text_on(26, UT_TY(M, top + AF_S_CAP_H - 1), &AF_M, NM[k], c, T_BG);
        cv_keycap(196, top - 1, KC_OCTUP, on ? (picked ? T_ACCENT : T_THEME) : T_DIM, T_KINK, T_BG);
        if (k == 2u && !restore)
            cv_text_on(26, UT_TY(S, top + AF_S_CAP_H + 13), &AF_S, "NOTHING TO RESTORE", T_DIM, T_BG);
    }
}
/* the knob line: four columns, the dot and the label, the value under them in the part's colour (a key pill: the pill) */
static void smk_st_knobs(uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        int32_t x = (int32_t)k * SMK_ST_CW + 2, y = SMK_ST_KY, w, room = SMK_ST_CW - 4, vx = x + AF_S_CAP_H + 3, vroom = room - AF_S_CAP_H - 3;
        uint32_t kn;
        int32_t kid;
        char one[PV_LEN], lb[PV_LEN];
        const aafont_t *f;
        if (!smk_cap[k].on)
            continue;
        smk_one_line(lb, smk_cap[k].lb);               /* the label over the whole column, the dot before the value under it */
        if (text_w(&AF_S, lb) > room)
            cv_scroll = 1;
        cv_free_text(x, UT_TY(S, y + AF_S_CAP_H), &AF_S, lb, T_MID, T_BG, room);
        cv_scroll = 0;
        smk_dot(x, y + 20, k + 4u * p, smk_lit(p, k) ? T_THEME : T_TEXT);
        kid = kc_tag(smk_cap[k].v, &kn);
        if (kid >= 0) {                                 /* (an action: the OCT+ pill, lit once its knob picked it) */
            cv_keycap(vx, y + 19, (uint32_t)kid, smk_cap[k].vc, T_KINK, T_BG);
            continue;
        }
        smk_one_line(one, smk_cap[k].v);
        f = smk_vface(one, smk_cap[k].u, vroom, 0, &w);
        if (w > vroom)
            cv_scroll = 1;
        smk_val_draw(vx, 0, y + 20 + AF_S_CAP_H - 1, f, one, smk_cap[k].u, smk_cap[k].vc);
        cv_scroll = 0;
    }
}
static uint32_t smk_st_sig[2];
static uint32_t smk_st_hash(uint32_t p)               /* what half p shows (in its view) */
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = (uint32_t)ui.page * 7919u + ux.gen * 13u + p * 101u, k, total;
    for (k = 0; k < 4u; k++)
        h = str_hash(str_hash(str_hash(h * 31u + smk_cap[k].on + (uint32_t)smk_cap[k].vc * 3u + (uint32_t)smk_lit(p, k) * 7u, smk_cap[k].lb), smk_cap[k].v), smk_cap[k].u);
    switch (pg->graph) {
    case GR_ROLL: {
        const uint8_t *b = (const uint8_t *)seq_steps(t);
        for (k = 0; k < sizeof t->step; k++)
            h = h * 16777619u ^ b[k];
        h += (uint32_t)ui.cursor * 977u + (uint32_t)ui.bank * 31u + (uint32_t)t->p[P_SLEN] * 7u + (uint32_t)motion_lock_steps(p) * 3u;
        if (song.playing)
            h += (uint32_t)t->seq_idx * 65537u + 1u;
        break;
    }
    case GR_EVENTS:                                    /* (the selected row's own values are its cards') */
        h += (uint32_t)ui.ev_row * 977u + motion_count(t) * 31u + motion_lock_count(t) * 7u + (uint32_t)t->p[P_SQNT] + (uint32_t)ui.ev_vel * 3u;
        break;
    case GR_BROWSE:
        h += pl_cursor(&total) * 977u + total * 31u + list_mode() * 7u + (uint32_t)list_fav(pl_cursor_slot()) * 3u + up_count();
        break;
    case GR_PATS:
        h += pat_pick() * 977u;
        break;
    default:
        h += act_col() * 977u + song.sel * 31u + up_count();
        break;
    }
    return h;
}
static void smk_stack_draw(void)
{
    const page_t *pg = cur_page();
    uint32_t p;
    for (p = 0; p < 2u; p++) {
        uint32_t sig;
        int32_t y0 = SMK_ST_Y0 + (int32_t)p * SMK_ST_H;
        smk_st_in(p);
        smk_st_cards();
        sig = smk_st_hash(p);
        if (ui.force || sig != smk_st_sig[p]) {
            smk_st_sig[p] = sig;
            CV_BANDS(240, SMK_ST_H, T_BG) {
                cv_bg = T_BG;
                switch (pg->graph) {
                case GR_ROLL:
                    smk_st_roll(TSEL, p);
                    break;
                case GR_EVENTS:                        /* (ui_graph.c graph_events, three rows moved into the half) */
                    lk_rows = 3;
                    lk_dy = SMK_ST_LTOP - (68 - LK_FILL_DY);
                    graph_events();
                    lk_rows = 4;
                    lk_dy = 0;
                    break;
                case GR_BROWSE:
                case GR_PATS:
                    smk_st_list();
                    break;
                default:
                    smk_st_tools(p);
                    break;
                }
                if (pg->graph != GR_TOOLS)
                    smk_st_knobs(p);
                if (p == 0u)                           /* (the line between the parts) */
                    cv_rect(4, SMK_ST_H - 1, 232, 1, ux_mix(T_BG, T_TEXT, 14));
                cv_blit(0, (uint32_t)(y0 + cv_by));
            }
        }
        smk_st_out();
    }
}

static void ui_draw_page(uint32_t counting);
static void smk_view_page(uint32_t counting)
{
    uint32_t v = smk_ov, i, pads = smk_pads_on();
    if (v && fm1_ms - smk_ov_ms > SMK_OV_MS)
        v = smk_ov = 0;
    if (pads)                                          /* (a pad held: its picture, over a fader's) */
        v = 4u;
    if (ui.menu || ui.layer || ui.guide || ui.uboot || ui.confirm || name_on() || counting || oct_show_on())
        v = 0;
    if (v != smk_ov_shown) {
        smk_ov_shown = (uint8_t)v;
        ui.force = 1;
    }
    if (v == 3u) {                                     /* MORPH: the two parts' lines only (smk_morph_draw) */
        for (i = 0; i < NPAGES && PAGES[i].graph != GR_MORPH; i++)
            ;
        if (i < NPAGES) {
            if (grid_on != SMK_LVGRID) {
                grid_on = SMK_LVGRID;
                ui.force = 1;
            }
            smk_morph_draw(&PAGES[i]);
            return;
        }
    }
    if (v == 4u) {
        if (grid_on != SMK_LVGRID) {
            grid_on = SMK_LVGRID;
            ui.force = 1;
        }
        smk_pads_draw(pads);
        return;
    }
    if (v == 1u || v == 2u) {
        if (grid_on != SMK_LVGRID) {
            grid_on = SMK_LVGRID;
            ui.force = 1;
        }
        smk_level_draw(v - 1u);
        return;
    }
    ui_draw_page(counting);
}
