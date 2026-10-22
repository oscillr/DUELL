/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.24 (owner): an ENGINE CHANGE keeps the sound's settings (ui.c set_engine_of). Up to 0.2.23 it was an INIT: the engine's defaults and its FIRST FACTORY SOUND
 * replaced the whole patch (apply_preset_to: every common parameter to its default, then the sound's values). Now only the engine and its own eight (P_E0..P_E7) change.
 *   KEEP      a part with every common parameter (below P_E0) set to a value off its default, switched through all ten engines and back: the common values, the slot it
 *             came from (t->user) and the mixer are untouched at every step; the engine's own eight are its defaults, or carried over where the label / format / range / list
 *             match (CUT RES on ANALOG <-> TRIO / NOISE's FREQ RES: CUT carries, FREQ does not; DTN; SUB; DRV), clamped
 *   BACK      switching back to the engine the part came from puts its own eight back as they were (eng_prev, 18 bytes a part), and a third engine in between does not
 *   SOUND     the part plays after every switch (a note renders above silence); FM6 loads its SLOT's patch
 *   MORPH     AMOUNT stays; the fader goes to A with a fresh End B (the old one was rolled for the old engine's own eight: for the morph another engine is another sound); PERFORM's SPACE / KILL / DEPTH untouched (no PERF LEAVE fade)
 *   INIT      INIT SOUND (init_engine_of) still resets everything: the defaults and the first factory sound
 *   EDITOR    the editor's SET G_ENGSEL and the ENGINE page's K1 go through the same set_engine_of (tests/editor_test.c, engine_page_test.c cover the paths)
 * Built and run by DUELL/host_tests.sh (name: engswitch) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok)
        printf("engswitch: FAIL %s\n", what), bad++;
    return ok;
}
static uint32_t rs = 0x12345u;
static uint32_t rnd(uint32_t n) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) % n; }
/* a value off the default inside the range (the matrix SRCs: a source, so a slot is used; AMT not 0) */
static int16_t off_def(uint32_t id)
{
    const param_desc_t *d = &TP[id];
    int32_t v = d->def, tries = 0;
    if (d->max <= d->min)
        return d->def;
    while ((v == d->def || v == 0) && tries++ < 20)
        v = d->min + (int32_t)rnd((uint32_t)(d->max - d->min + 1));
    return (int16_t)v;
}
static int plays(track_t *t)                            /* a note renders above silence within 80 blocks (the part's own peak, before the master) */
{
    static int32_t o[2 * CTL];
    uint32_t b;
    int32_t pk = 0;
    for (b = 0; b < 4u; b++) {                           /* (the switch released the voices: panic_req is the next block's; the note after that, as a player's would be) */
        mix_block(o, CTL);
        while (wave_service()) ;
    }
    t->peak = 0;
    trk_note_on(t, 57, 110);
    for (b = 0; b < 80u; b++) {
        mix_block(o, CTL);
        while (wave_service()) ;
        pk = t->peak > pk ? t->peak : pk;
        t->peak = 0;
    }
    trk_note_off(t, 57);
    for (b = 0; b < 120u; b++) {
        mix_block(o, CTL);
        while (wave_service()) ;
    }
    return pk > 200;
}
static int common_same(const int16_t *a, const int16_t *b, uint32_t *first)
{
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        if (a[i] != b[i]) {
            *first = i;
            return 0;
        }
    return 1;
}

int main(void)
{
    int16_t want[P_COUNT], e0[8];
    uint32_t i, e, first = 0, ok = 1, n = 0;
    char what[160];
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    duell_mode_req = DM_STEREO;
    duell_mode_force(DM_STEREO);
    init_engine_of(TSEL, 0u);                           /* ANALOG, its first sound */
    for (i = 0; i < P_E0; i++)                          /* every common parameter off its default (a custom patch) */
        TSEL->p[i] = off_def(i);
    TSEL->p[P_MUTE] = 0;
    TSEL->p[P_LEVEL] = 100;
    TSEL->p[P_TRIM] = 110;
    TSEL->p[P_VOICE] = V_POLY;
    TSEL->p[P_DKSRC] = 0;
    TSEL->p[P_ITYPE] = 2;
    TSEL->p[P_ATK] = 2; TSEL->p[P_SUS] = 120; TSEL->p[P_DEC] = 60;   /* (a note that can be heard within the render: no arp, no chord, no duck, no amp modulation) */
    TSEL->p[P_FREEA] = 127; TSEL->p[P_FREEB] = 20; TSEL->p[P_FREEC] = 0; TSEL->p[P_FREED] = 0;   /* (the multimode filter open) */
    TSEL->p[P_LD_AMP] = 0; TSEL->p[P_GLIDE] = 0; TSEL->p[P_AMODE] = 0; TSEL->p[P_CHRD] = 0; TSEL->p[P_DKDEP] = 0;
    TSEL->p[P_M1SRC] = TSEL->p[P_M2SRC] = TSEL->p[P_M3SRC] = TSEL->p[P_M4SRC] = TSEL->p[P_M5SRC] = TSEL->p[P_M6SRC] = TSEL->p[P_M7SRC] = TSEL->p[P_M8SRC] = MS_LFO;
    TSEL->p[P_M1DST] = TSEL->p[P_M2DST] = TSEL->p[P_M3DST] = TSEL->p[P_M4DST] = TSEL->p[P_M5DST] = TSEL->p[P_M6DST] = TSEL->p[P_M7DST] = TSEL->p[P_M8DST] = MD_PAN;
    song.master_q12 = 4096;
    TSEL->user = 5;                                     /* from slot #5 */
    memcpy(want, TSEL->p, sizeof want);
    memcpy(e0, &TSEL->p[P_E0], sizeof e0);
    {   /* a live morph and PERFORM state to watch */
        morph_act(MQ_ROLL, morph_mask(song.sel), 1u, song.sel); frames(64);
        morph_act(MQ_FADER, morph_mask(song.sel), 40u, song.sel);
        morph_act(MQ_AMOUNT, morph_mask(song.sel), 37u, song.sel); frames(300);
        perf_space = 23; perf_filt = 11; perf_depth = 66;
    }
    expect("the morph is live before the switches (End B rolled, fader 40, AMOUNT 37)", morph_view_on(song.sel) && morph_fader_of(song.sel) == 40u && morph_amount_of(song.sel) == 37u);
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en;
        uint32_t k;
        if (!eng_ok(e) || e == 0u)
            continue;
        set_engine_of(TSEL, e);
        frames(8);
        en = ENGINES[e];
        n++;
        snprintf(what, sizeof what, "-> %s: every common parameter as it was", en->name);
        if (!common_same(want, TSEL->p, &first)) {
            snprintf(what, sizeof what, "-> %s: common parameter %u (%s) changed: %d -> %d", en->name, first, TP[first].label ? TP[first].label : "?", want[first], TSEL->p[first]);
            expect(what, 0);
        }
        snprintf(what, sizeof what, "-> %s: the slot (#%u) and the engine are as expected", en->name, 5u);
        expect(what, TSEL->user == 5u && TSEL->eng_req == e);
        for (k = 0, ok = 1; k < 8u; k++) {
            int16_t v = TSEL->p[P_E0 + k];
            ok &= v >= en->edit[k].min && v <= en->edit[k].max;
        }
        snprintf(what, sizeof what, "-> %s: its own eight inside their ranges", en->name);
        expect(what, ok);
        {
            int pl = plays(TSEL);
            snprintf(what, sizeof what, "-> %s: the part plays a note", en->name);
            expect(what, pl);
        }
        if (!common_same(want, TSEL->p, &first)) {
            snprintf(what, sizeof what, "-> %s: common parameter %u changed by playing", en->name, first);
            expect(what, 0);
            memcpy(TSEL->p, want, P_E0 * sizeof want[0]);
        }
    }
    expect("all nine other engines visited", n == 9u);
    printf("engswitch: after the switches: fader %u, AMOUNT %u, live %d\n", morph_fader_of(song.sel), morph_amount_of(song.sel), morph_view_on(song.sel));
    expect("the morph: AMOUNT as before; the fader at A with a fresh End B (the old B was rolled for the old engine's own eight: another engine is another sound for the morph, SPEC 3.5: a loaded sound starts at A)",
           morph_amount_of(song.sel) == 37u && morph_fader_of(song.sel) == 0u && morph_view_on(song.sel));
    expect("PERFORM's SPACE, KILL and DEPTH untouched (no PERF LEAVE fade: an engine change is no sound load)", perf_space == 23 && perf_filt == 11 && perf_depth == 66);
    /* BACK and the carry-over */
    init_engine_of(TSEL, 0u);
    for (i = 0; i < P_E0; i++)
        TSEL->p[i] = want[i];
    TSEL->p[P_E0 + 2] = 33;                             /* ANALOG MIX 33, NOIS 77, DTN 9, DRV 50 (its own CUT and RES are gone: filter 1 is every engine's) */
    TSEL->p[P_E0 + 3] = 77;
    TSEL->p[P_E0 + 1] = 9;
    TSEL->p[P_E0 + 6] = 50;
    memcpy(e0, &TSEL->p[P_E0], sizeof e0);
    {
        uint32_t trio = 0, noise = 0, phase = 0, wheel = 0;
        for (e = 0; e < NENGINES; e++) {
            if (str_eq(ENGINES[e]->name, "TRIO")) trio = e;
            if (str_eq(ENGINES[e]->name, "NOISE")) noise = e;
            if (str_eq(ENGINES[e]->name, "PHASE")) phase = e;
            if (str_eq(ENGINES[e]->name, "WHEEL")) wheel = e;
        }
        set_engine_of(TSEL, trio);
        expect("ANALOG -> TRIO: DTN carries over (the same label, format and range; TRIO's DTN is 0..50: 9 fits), the rest TRIO's defaults",
               TSEL->p[P_E0 + 3] == 9 && TSEL->p[P_E0 + 0] == ENGINES[trio]->edit[0].def && TSEL->p[P_E0 + 1] == ENGINES[trio]->edit[1].def && TSEL->p[P_E0 + 2] == ENGINES[trio]->edit[2].def);
        set_engine_of(TSEL, noise);
        expect("TRIO -> NOISE: nothing carries (no common label left), NOISE's defaults", TSEL->p[P_E0 + 1] == ENGINES[noise]->edit[1].def && TSEL->p[P_E0 + 3] == ENGINES[noise]->edit[3].def);
        set_engine_of(TSEL, 0u);
        expect("NOISE -> ANALOG (not the engine before: TRIO was): no restore, ANALOG's defaults (DTN 10, DRV 0)",
               TSEL->p[P_E0 + 1] == ENGINES[0]->edit[1].def && TSEL->p[P_E0 + 6] == ENGINES[0]->edit[6].def);
        TSEL->p[P_E0 + 2] = 33; TSEL->p[P_E0 + 3] = 77; TSEL->p[P_E0 + 1] = 9; TSEL->p[P_E0 + 6] = 50;
        set_engine_of(TSEL, phase);
        expect("ANALOG -> PHASE: PHASE's defaults, DTN carries (the same control), WAVE does not (another list)", TSEL->p[P_E0 + 4] == 9 && TSEL->p[P_E0 + 0] == ENGINES[phase]->edit[0].def && TSEL->p[P_E0 + 2] == ENGINES[phase]->edit[2].def);
        TSEL->p[P_E0 + 2] = 100;                        /* PHASE DCW 100 */
        set_engine_of(TSEL, 0u);
        expect("PHASE -> ANALOG, the engine before: ANALOG's own eight back as they were (MIX 33, NOIS 77, DTN 9, DRV 50)", TSEL->p[P_E0 + 2] == 33 && TSEL->p[P_E0 + 3] == 77 && TSEL->p[P_E0 + 1] == 9 && TSEL->p[P_E0 + 6] == 50);
        set_engine_of(TSEL, phase);
        expect("  .. and back to PHASE: its DCW 100 again", TSEL->p[P_E0 + 2] == 100);
        set_engine_of(TSEL, wheel);
        set_engine_of(TSEL, phase);
        expect("PHASE -> WHEEL -> PHASE: PHASE's eight as they were (WHEEL was the engine before)", TSEL->p[P_E0 + 2] == 100);
        set_engine_of(TSEL, 0u);
        expect("  .. but WHEEL -> PHASE -> ANALOG: ANALOG's defaults again (PHASE was the engine before, not ANALOG), MIX carried from nowhere: the default",
               TSEL->p[P_E0 + 2] == ENGINES[0]->edit[2].def);
        expect("  .. the common parameters still as they were through all of it", common_same(want, TSEL->p, &first));
        set_engine_of(TSEL, 0u);
        expect("the same engine again: nothing changes (no undo copy, no voice release)", TSEL->eng_req == 0 && common_same(want, TSEL->p, &first));
    }
    /* INIT still resets everything */
    init_engine_of(TSEL, 0u);
    expect("INIT SOUND (init_engine_of): the first factory sound, the common parameters at the sound's values (not the custom ones)", !common_same(want, TSEL->p, &first) && TSEL->user == 0);
    printf("engswitch: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
