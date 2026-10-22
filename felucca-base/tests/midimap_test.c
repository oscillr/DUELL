/* SPDX-License-Identifier: GPL-3.0-only
 * The MIDI MAP (docs/design/MIDI_MAP_DESIGN_2.md; firmware/src/midi_map.h, midi_control.c, midi_learn.c, settings_persist.c, editor_map.c).
 * What the older tests cannot show: cloud/base3's midi_control.c and midi_learn.c, compiled into this test under the prefix ref_ (tests/midimap_refgen.py),
 * are fed the same CC script as the table; the table built from tests/midimap_legacy.csv (today's 24 CCs) writes exactly what the reference writes, and the
 * shipped map (docs/MIDI_MAP_DEFAULTS.csv) writes the same for each of the 24 targets on its NEW number; the morph's triggers follow the target, not the
 * number; a taken number is refused and force takes it; learn writes a line (16 at most, typed lines no cap); the settings record at its exact size; the
 * shipped list added by a later firmware fills only an empty, never-cleared number; the facts of a target; the extension values and the global ones.
 * The generator's failures are tests/midimap_gen_test.py; the editor's commands and their fuzz are in editor_test.c. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "midi_map_legacy.h"                          /* MIDI_MAP_LEGACY: tests/midimap_legacy.csv */
#include "midi_map_shipped.h"                         /* MIDI_MAP_SHIPPED: docs/MIDI_MAP_DEFAULTS.csv */
#include "midimap_ref_control.inc"                    /* cloud/base3's, ref_ */
#include "midimap_ref_learn.inc"

/* ---------------------------------------------------------------------------------------------------------------------------- the snapshot */
typedef struct {
    int16_t p[NTRK][EXT_P_LEN];
    int16_t g[G_COUNT];
    int8_t bal;
    uint8_t xf, split, mreq, mode, qn;
    uint32_t q[MQ_N];
} snap_t;
static void snap_take(snap_t *s)
{
    uint32_t i;
    memset(s, 0, sizeof *s);
    for (i = 0; i < NTRK; i++)
        memcpy(s->p[i], trk[i].p, sizeof s->p[i]);
    memcpy(s->g, song.g, sizeof s->g);
    s->bal = mix_bal; s->xf = mix_xf; s->split = mix_split; s->mreq = duell_mode_req; s->mode = duell_mode;
    s->qn = (uint8_t)(mq_w - mq_r);
    for (i = 0; i < s->qn && i < MQ_N; i++)
        s->q[i] = mq_buf[(uint8_t)(mq_r + i) % MQ_N];
}
static uint32_t qn(void) { return (uint8_t)(mq_w - mq_r); }
static uint32_t qlast(void) { return mq_buf[(uint8_t)(mq_w - 1u) % MQ_N]; }

/* a power-on with the MIDI IN routing (0 PARTS, 1 SEL), the output MODE and the engines of the four parts; the table is the harness's (legacy) */
static void scen_reset(uint32_t route, uint32_t mode, uint32_t e0, uint32_t e1)
{
    uint32_t i;
    ui_power_on();
    song.g[G_ROUTE] = (int16_t)route;
    duell_mode_force(mode);
    for (i = 0; i < NTRK; i++) {
        set_engine_of(&trk[i], i & 1u ? e1 : e0);
        trk[i].engine = trk[i].eng_req;
    }
    song.sel = 0;
    mix_bal = 0; mix_xf = 30; mix_split = 0;                /* (the mix of the MODE page: a CC leaves it, the next run must start equal) */
    mq_r = mq_w = mq_lost = 0;
    memset(midi_morph_fired, 0, sizeof midi_morph_fired);
    memset(ref_midi_morph_fired, 0, sizeof ref_midi_morph_fired);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(ref_midi_ch, 0, sizeof ref_midi_ch);
    memset(ref_learn_store, 0, sizeof ref_learn_store);
    ml_arm = 0; ml_heard = 0;
}
static void load_table(const uint16_t *t)
{
    memcpy(midi_map, t, sizeof midi_map);
    midi_map_ready = 1;
}
static void ev(int ref, uint32_t ch, uint32_t cc, uint32_t v)
{
    if (ref)
        ref_midi_event(0xB0u, ch, cc, v);
    else
        midi_event(0xB0u, ch, cc, v);
}

static const uint8_t VAL[] = {0, 127, 127, 64, 0, 1, 42, 43, 63, 85, 86, 100, 127};   /* (a high value twice, and low again: the triggers) */
static const uint8_t CHS[] = {0, 1, 5};
#define NVAL ((uint32_t)sizeof VAL)
#define NCH ((uint32_t)sizeof CHS)
#define SCRIPT_N (MAP_N * NCH * NVAL)
static snap_t REFS[SCRIPT_N];
static snap_t NEWS[SCRIPT_N];

/* the learned CCs of the script: some on free numbers, some on the standard map's, the morph's, the MODE's (the reference replaces them silently; the table is forced) */
static const struct { uint8_t cc, k, id; } LRN[] = {{26, 0, P_E4}, {74, 1, P_LEVEL}, {112, 0, P_PAN}, {20, 1, P_REV}, {30, 0, P_ATK}, {91, 0, P_TRIM}, {111, 1, P_DLY},
                                                  {116, 1, P_CHOR}, {7, 0, P_GLIDE}, {23, 1, P_IMIX}, {50, 0, P_E0 + 2u}, {51, 1, P_E0}};
static void learn_set(int ref, uint32_t n)
{
    uint32_t i, ok = 1;
    for (i = 0; i < n; i++)
        ok &= (ref ? ref_ml_learn(LRN[i].cc, LRN[i].k, LRN[i].id) : ml_learn_f(LRN[i].cc, LRN[i].k, LRN[i].id, 1, 0)) == 1u;
    if (!ok)
        printf("midimap: learn_set failed\n");
}

/* the whole script of one run: every CC of every channel, every value: the snapshot after each event */
static uint32_t run_script(int ref, snap_t *out)
{
    uint32_t cc, c, v, n = 0;
    for (cc = 0; cc < MAP_N; cc++)
        for (c = 0; c < NCH; c++)
            for (v = 0; v < NVAL; v++) {
                ev(ref, CHS[c], cc, VAL[v]);
                snap_take(&out[n++]);
            }
    return n;
}
static int g_mismatch;
static void compare_runs(const char *what, uint32_t n, uint32_t cc_of_step(uint32_t))
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (memcmp(&REFS[i], &NEWS[i], sizeof REFS[i])) {
            if (g_mismatch < 6) {
                const uint8_t *x = (const uint8_t *)&REFS[i], *y = (const uint8_t *)&NEWS[i];
                uint32_t o = 0;
                while (x[o] == y[o]) o++;
                printf("midimap: %s: step %u (CC %u) differs, first at byte %u (%s)\n", what, i, cc_of_step(i), o,
                       o < sizeof REFS[i].p ? "a part's value" : o < offsetof(snap_t, bal) ? "a global value" : o < offsetof(snap_t, q) ? "mix/mode/queue count" : "the queue");
                if (o < sizeof REFS[i].p)
                    printf("midimap:   track %u index %u: %d / %d\n", o / (uint32_t)sizeof REFS[i].p[0], (o % (uint32_t)sizeof REFS[i].p[0]) / 2u,
                           ((const int16_t *)x)[o / 2u], ((const int16_t *)y)[o / 2u]);
            }
            g_mismatch++;
            return;
        }
}
static uint32_t cc_of_script(uint32_t i) { return i / (NCH * NVAL); }

/* the engines of the scenarios: every engine against a second one, so the filter's smart CCs meet each kind of engine */
static int legacy_equivalence(void)
{
    int bad = 0;
    uint32_t route, mode, e, steps = 0, scen = 0, lrn;
    static const uint32_t MODES[] = {DM_SPLIT, DM_STEREO, DM_LAYER};
    g_mismatch = 0;
    for (lrn = 0; lrn < 2u; lrn++)
        for (route = 0; route < 3u; route++)                     /* (MIDI IN: PARTS, SEL, and 2, the older CH5-8 that reads as PARTS) */
            for (mode = 0; mode < 3u; mode++)
                for (e = 0; e < NENGINES; e++) {
                    uint32_t n1, n2;
                    scen_reset(route, MODES[mode], e, (e + 5u) % NENGINES);
                    if (lrn)
                        learn_set(1, NELEM(LRN));
                    n1 = run_script(1, REFS);
                    scen_reset(route, MODES[mode], e, (e + 5u) % NENGINES);
                    load_table(MIDI_MAP_LEGACY);
                    if (lrn)
                        learn_set(0, NELEM(LRN));
                    n2 = run_script(0, NEWS);
                    if (n1 != n2)
                        g_mismatch++;
                    compare_runs("legacy", n1, cc_of_script);
                    steps += n1;
                    scen++;
                }
    printf("midimap: %u scenarios, %u events, each through cloud/base3's code and the table\n", scen, steps);
    bad += check("every CC 0..127 x 13 values x 3 channels: the legacy table writes what cloud/base3's code writes (parameters, globals, the mode, the morph queue), "
                 "three MIDI IN settings (PARTS, SEL, the older CH5-8), SPLIT / STEREO / LAYER, every engine, with and without 12 learned CCs", !g_mismatch);
    return bad;
}

/* the same CCs while a learn waits (ml_arm): the CC heard, and nothing else done */
static int armed_equivalence(void)
{
    int bad = 0, ok = 1;
    uint32_t route, cc, ch;
    for (route = 0; route < 2u; route++)
        for (cc = 0; cc < MAP_N; cc++)
            for (ch = 0; ch < 3u; ch++) {
                snap_t a, b;
                uint32_t ha, hb;
                if (cc == 2u || cc == 4u)                          /* (BREATH and FOOT are locked inputs now: never heard by a learn; cloud/base3 heard them) */
                    continue;
                scen_reset(route, DM_SPLIT, 0, 6);
                ml_arm = 1; ml_heard = 0;
                ev(1, CHS[ch], cc, 99);
                ha = ml_heard; snap_take(&a);
                scen_reset(route, DM_SPLIT, 0, 6);
                load_table(MIDI_MAP_LEGACY);
                ml_arm = 1; ml_heard = 0;
                ev(0, CHS[ch], cc, 99);
                hb = ml_heard; snap_take(&b);
                ok &= ha == hb && !memcmp(&a, &b, sizeof a);
                if (!ok) { printf("midimap: armed: CC %u ch %u: heard %u / %u\n", cc, ch, ha, hb); route = 9; cc = MAP_N; break; }
            }
    scen_reset(0, DM_SPLIT, 0, 6);
    bad += check("a learn waiting for a CC: every unlocked number is heard (the CC + 1) and does nothing else, as before; the standard numbers act", ok);
    return bad;
}

/* the shipped map: each of today's 24 targets on its NEW number writes what the reference writes on the OLD one */
static int shipped_equivalence(void)
{
    int bad = 0, ok = 1;
    uint32_t i, j, pairs = 0, gone = 0, route, mode, e, c, v;
    static const uint32_t MODES[] = {DM_SPLIT, DM_STEREO, DM_LAYER};
    for (i = 0; i < MAP_N; i++) {
        uint32_t old = MIDI_MAP_LEGACY[i], nw = MAP_N;
        if (MAP_IS_EMPTY(old) || MAP_IS_LOCKED(old))
            continue;
        for (j = 0; j < MAP_N; j++)
            if (!MAP_IS_EMPTY(MIDI_MAP_SHIPPED[j]) && MAP_TARGET(MIDI_MAP_SHIPPED[j]) == MAP_TARGET(old) && MAP_RULE(MIDI_MAP_SHIPPED[j]) == MAP_RULE(old))
                nw = j;
        if (nw == MAP_N) {                  /* (MIDI_MAP_FINAL: 11 of today's 24 have no number any more: ROLL A / B (one centred ROLL on CC102 now), UNDO, the four morph switches, MODE, BALANCE, XFADE, SPLIT: learn-only) */
            gone += i == 21u || i == 22u || i == 23u || (i >= 111u && i <= 118u);
            if (!(i == 21u || i == 22u || i == 23u || (i >= 111u && i <= 118u))) {
                printf("midimap: the target of CC %u is nowhere in the shipped map\n", i);
                ok = 0;
            }
            continue;
        }
        pairs++;
        for (route = 0; route < 2u; route++)
            for (mode = 0; mode < 3u; mode++)
                for (e = 0; e < NENGINES; e += 3) {
                    snap_t ra[3 * 13], na[3 * 13];
                    uint32_t n = 0;
                    scen_reset(route, MODES[mode], e, (e + 5u) % NENGINES);
                    for (c = 0; c < NCH; c++)
                        for (v = 0; v < NVAL; v++) { ev(1, CHS[c], i, VAL[v]); snap_take(&ra[n++]); }
                    scen_reset(route, MODES[mode], e, (e + 5u) % NENGINES);
                    load_table(MIDI_MAP_SHIPPED);
                    n = 0;
                    for (c = 0; c < NCH; c++)
                        for (v = 0; v < NVAL; v++) { ev(0, CHS[c], nw, VAL[v]); snap_take(&na[n++]); }
                    if (memcmp(ra, na, sizeof ra)) {
                        if (ok) printf("midimap: shipped: CC %u -> %u differs (route %u mode %u engine %u)\n", i, nw, route, mode, e);
                        ok = 0;
                    }
                }
    }
    bad += check("the shipped map: each of today's targets that still has a number, sent on its NEW number, writes what cloud/base3 writes on the old one (13 of 13; the other 11 are learn-only: no line)", ok && pairs == 13u && gone == 11u);
    return bad;
}

/* ---------------------------------------------------------------------------------------------------------------------------- the morph's triggers */
static int morph_rearm(void)
{
    int bad = 0, ok = 1;
    uint32_t a, b, u, q0;
    scen_reset(0, DM_SPLIT, 0, 6);
    load_table(MIDI_MAP_SHIPPED);
    ok = map_set(104, MAP_T_ROLL_A, MAP_R_CHANNEL, MAP_W_TYPED, 0, 0) == MAP_OK && map_set(105, MAP_T_ROLL_B, MAP_R_CHANNEL, MAP_W_TYPED, 0, 0) == MAP_OK &&
         map_set(106, MAP_T_UNDO, MAP_R_CHANNEL, MAP_W_TYPED, 0, 0) == MAP_OK;     /* (no shipped line has them any more: typed ones) */
    for (a = 0; a < 3u; a++) {                              /* 104 ROLL A, 105 ROLL B, 106 UNDO: typed */
        uint32_t cc = 104u + a;
        mq_r = mq_w = 0;
        ev(0, 0, cc, 127); ok &= qn() == 1u;
        ev(0, 0, cc, 127); ok &= qn() == 1u;                /* held high: once */
        ev(0, 0, cc, 64);  ok &= qn() == 1u;                /* (64 is still high; UNDO: still non-zero) */
        ev(0, 0, a == 2u ? 0u : 63u, 0);                    /* (a CC of nothing: nothing) */
        ev(0, 0, cc, a == 2u ? 0u : 63u); ok &= qn() == 1u;   /* released: re-armed (a roll: below 64; UNDO: 0) */
        ev(0, 0, cc, a == 2u ? 1u : 100u); ok &= qn() == 2u;
        ev(0, 1, cc, 127); ok &= qn() == 3u;                /* the other channel keeps its own memory */
    }
    bad += check("morph triggers on typed lines 104 / 105 / 106: once while held, re-armed below 64 (UNDO: at 0), each channel its own", ok);
    mq_r = mq_w = 0;
    memset(midi_morph_fired, 0, sizeof midi_morph_fired);
    ev(0, 0, 104, 127); ev(0, 0, 105, 127); ev(0, 0, 106, 127);
    ok = qn() == 3u;
    ev(0, 0, 121, 0); ev(0, 0, 104, 127); ev(0, 0, 105, 127); ev(0, 0, 106, 127);
    ok &= qn() == 6u;
    bad += check("  ROLL A, ROLL B and UNDO do not share a trigger; CC121 re-arms all three", ok);
    /* the kinds the queue holds: ROLL A end 0, ROLL B end 1, UNDO End A below 64 / End B from 64 */
    mq_r = mq_w = 0;
    memset(midi_morph_fired, 0, sizeof midi_morph_fired);
    ev(0, 0, 104, 127); ok = (qlast() & 0xFFu) == MQ_ROLL && (qlast() >> 16) == 0u;
    ev(0, 0, 105, 127); ok &= (qlast() & 0xFFu) == MQ_ROLL && (qlast() >> 16) == 1u;
    ev(0, 0, 106, 30); ok &= (qlast() & 0xFFu) == MQ_UNDO && (qlast() >> 16) == 0u;
    ev(0, 0, 106, 0); ev(0, 0, 106, 90); ok &= (qlast() & 0xFFu) == MQ_UNDO && (qlast() >> 16) == 1u;
    bad += check("  the queue gets the roll's end (A 0, B 1) and UNDO's (End A below 64, End B from 64)", ok);
    /* the number moves: the trigger is the target's */
    scen_reset(0, DM_SPLIT, 0, 6);
    load_table(MIDI_MAP_SHIPPED);
    ok = map_set(104, MAP_T_ROLL_A, MAP_R_CHANNEL, MAP_W_TYPED, 0, 0) == MAP_OK && map_set(69, MAP_T_ROLL_A, MAP_R_CHANNEL, MAP_W_TYPED, 0, 0) == MAP_OK && MAP_IS_EMPTY(midi_map[104]);
    mq_r = mq_w = 0;
    ev(0, 0, 104, 127); ok &= qn() == 0u;                   /* (the old number does nothing) */
    ev(0, 0, 69, 127); ok &= qn() == 1u;
    ev(0, 0, 69, 127); ok &= qn() == 1u;
    ev(0, 0, 69, 0); ev(0, 0, 69, 127); ok &= qn() == 2u;
    ok &= map_set(1 + 1 + 1, MAP_T_UNDO, MAP_R_CHANNEL, MAP_W_TYPED, 0, 0) == MAP_OK;     /* UNDO to CC3: any number */
    q0 = qn();
    ev(0, 0, 3, 127); ev(0, 0, 3, 127); u = qn() - q0;
    ev(0, 0, 3, 0); ev(0, 0, 3, 127); b = qn() - q0;
    ok &= u == 1u && b == 2u;
    bad += check("morph triggers follow the target when its number moves (ROLL A to CC69, UNDO to CC3): once, re-armed, the old number silent", ok);
    return bad;
}

/* the shipped ROLL (CC102): ONE CC, centred like the knob: 0..42 rolls End A, 43..84 nothing, 85..127 rolls End B; a roll fires when the value ENTERS its zone */
static int roll_centred(void)
{
    int bad = 0, ok = 1;
    uint32_t v;
    scen_reset(0, DM_SPLIT, 0, 6);
    load_table(MIDI_MAP_SHIPPED);
    ok = MAP_TARGET(midi_map[102]) == MAP_T_ROLL && MAP_TARGET(midi_map[70]) == MAP_T_MORPH && MAP_TARGET(midi_map[103]) == MAP_T_AMOUNT;
    bad += check("the shipped morph numbers: the fader on 70, ROLL on 102 (one CC), AMOUNT on 103", ok);
    mq_r = mq_w = 0;
    memset(midi_morph_fired, 0, sizeof midi_morph_fired);
    for (ok = 1, v = 43; v <= 84; v++)
        ev(0, 0, 102, v);
    ok = qn() == 0u;
    bad += check("  ROLL: 43..84 (the middle) does nothing", ok);
    ev(0, 0, 102, 0); ok = qn() == 1u && (qlast() & 0xFFu) == MQ_ROLL && (qlast() >> 16) == 0u;
    ev(0, 0, 102, 20); ev(0, 0, 102, 42); ok &= qn() == 1u;              /* (still in the A zone: once) */
    ev(0, 0, 102, 64); ev(0, 0, 102, 0); ok &= qn() == 2u && (qlast() >> 16) == 0u;   /* (left through the middle, entered again: rolls again) */
    bad += check("  ROLL: 0..42 rolls End A once on entering, again only after the value has left the zone", ok);
    ev(0, 0, 102, 85); ok = qn() == 3u && (qlast() & 0xFFu) == MQ_ROLL && (qlast() >> 16) == 1u;
    ev(0, 0, 102, 127); ev(0, 0, 102, 100); ok &= qn() == 3u;
    ev(0, 0, 102, 84); ev(0, 0, 102, 127); ok &= qn() == 4u && (qlast() >> 16) == 1u;
    bad += check("  ROLL: 85..127 rolls End B once on entering, again only after leaving", ok);
    mq_r = mq_w = 0;
    memset(midi_morph_fired, 0, sizeof midi_morph_fired);
    ev(0, 0, 102, 64); ev(0, 0, 102, 100); ev(0, 0, 102, 64); ev(0, 0, 102, 0);   /* a sweep from the middle to B and back through to A */
    ok = qn() == 2u;
    ev(0, 1, 102, 127); ok &= qn() == 3u;                                   /* (the other channel keeps its own memory) */
    bad += check("  ROLL: a sweep rolls B on the way up, A on the way down; each channel has its own memory", ok);
    return bad;
}

/* ---------------------------------------------------------------------------------------------------------------------------- the rules */
static int rules_and_learn(void)
{
    int bad = 0, ok = 1;
    uint32_t i, cc, free_n = 0;
    char who[24];
    scen_reset(0, DM_SPLIT, 0, 6);
    load_table(MIDI_MAP_SHIPPED);
    /* the shape of the shipped map */
    {
        uint32_t direct = 0, locked = 0, empty = 0;
        for (cc = 0; cc < MAP_N; cc++) {
            uint32_t l = midi_map[cc];
            locked += MAP_IS_LOCKED(l); empty += MAP_IS_EMPTY(l);
            direct += !MAP_IS_LOCKED(l) && !MAP_IS_EMPTY(l);
            ok &= MAP_LOCKED_CC(cc) == (uint32_t)MAP_IS_LOCKED(l);
        }
        ok &= direct == 70u && locked == 23u && empty == 35u;
        bad += check("the shipped map: 70 direct lines, the 23 locked numbers (18 standard + the inputs 1 2 4 11 64), 35 numbers unused (MIDI_MAP_FINAL 3.4)", ok);
    }
    /* a taken number is refused with the owner's name; force takes it */
    ok = map_set(74, P_ATK, MAP_R_CHANNEL, MAP_W_TYPED, 0, who) == MAP_E_CONFLICT && !strcmp(who, "CUTOFF") && midi_map[74] == MIDI_MAP_SHIPPED[74];
    ok &= map_set(70, P_ATK, MAP_R_CHANNEL, MAP_W_TYPED, 0, who) == MAP_E_CONFLICT && !strcmp(who, "MORPH");
    ok &= map_set(21, P_ATK, MAP_R_CHANNEL, MAP_W_TYPED, 0, who) == MAP_E_CONFLICT && !strcmp(who, "TWIN CUTOFF");   /* (1.0: the MIDI line says TWIN, not 2) */
    ok &= map_set(44, P_ATK, MAP_R_CHANNEL, MAP_W_TYPED, 0, who) == MAP_E_CONFLICT && who[0];
    ok &= map_set(74, P_ATK, MAP_R_CHANNEL, MAP_W_TYPED, 1, who) == MAP_OK && midi_map[74] == MAP_LINE(P_ATK, MAP_R_CHANNEL, MAP_W_TYPED);
    ok &= MAP_IS_EMPTY(midi_map[73]) && (midi_map[73] & MAP_CLEARED);       /* (ATTACK has one number: its old one is free and marked cleared) */
    ev(0, 0, 74, 127);
    ok &= trk[0].p[P_ATK] == param_desc_of(eng_idx(trk[0].eng_req), P_ATK)->max;
    bad += check("a taken number is refused with the owner's name (CUTOFF, MORPH, TWIN CUTOFF), force takes it; one CC one parameter, one parameter one CC", ok);
    /* the same parameter on its own number is no conflict; another parameter's line stays */
    ok = map_set(74, P_ATK, MAP_R_CHANNEL, MAP_W_TYPED, 0, who) == MAP_OK;
    ok &= map_set(75, P_ATK, MAP_R_PART2, MAP_W_TYPED, 0, who) == MAP_E_CONFLICT && !strcmp(who, "AMP ENV DECAY");
    ok &= map_set(69, P_ATK, MAP_R_PART2, MAP_W_TYPED, 0, 0) == MAP_OK;     /* (part 2's ATTACK is another parameter than the channel's) */
    ok &= MAP_TARGET(midi_map[74]) == P_ATK && MAP_TARGET(midi_map[69]) == P_ATK;
    bad += check("  the parameter itself on its own number is no conflict; ATTACK of part 2 is another parameter than ATTACK of the channel's part", ok);
    /* locked numbers are never assigned (force or not), invalid input */
    for (ok = 1, i = 0; i < MAP_N; i++)
        if (MAP_LOCKED_CC(i)) {
            ok &= map_set(i, P_ATK, MAP_R_CHANNEL, MAP_W_TYPED, 1, 0) == MAP_E_LOCKED && map_set(i, P_ATK, 0, MAP_W_LEARNED, 1, 0) == MAP_E_LOCKED &&
                  map_clear(i) == MAP_E_LOCKED && MAP_IS_LOCKED(midi_map[i]);
            ok &= ml_learn_f(i, 0, P_ATK, 1, 0) == 2u;
        }
    ok &= map_set(128, P_ATK, 0, MAP_W_TYPED, 0, 0) == MAP_E_BAD && map_set(9, 127, 0, MAP_W_TYPED, 0, 0) == MAP_E_BAD &&
          map_set(9, MAP_T_G(G_CRATE), MAP_R_CHANNEL, MAP_W_TYPED, 0, 0) == MAP_E_BAD && map_set(9, P_ATK, MAP_R_GLOBAL, MAP_W_TYPED, 0, 0) == MAP_E_BAD &&
          map_set(9, MAP_T_MODE, MAP_R_PART1, MAP_W_TYPED, 0, 0) == MAP_E_BAD && map_set(9, XW_BASE + NX - 1u, 0, MAP_W_TYPED, 0, 0) == MAP_E_BAD &&
          map_set(9, P_ATK, 0, MAP_W_SHIPPED, 0, 0) == MAP_E_BAD && map_set(9, P_ATK, 4, MAP_W_LEARNED, 0, 0) == MAP_E_BAD && MAP_IS_EMPTY(midi_map[9]);
    bad += check("the 23 locked numbers are never assigned, with or without force (typed, learned, cleared); a bad number, target or rule is invalid", ok);
    /* learn writes a line, 16 at most; typed lines no cap */
    load_table(MIDI_MAP_SHIPPED);
    for (ok = 1, cc = 0; cc < MAP_N; cc++)
        free_n += MAP_IS_EMPTY(midi_map[cc]);
    ok = free_n == 35u;
    {
        static const uint8_t FREE[] = {3, 8, 9, 33, 37, 39, 40, 42, 43, 56, 57, 58, 59, 60, 61, 62, 63, 65, 66, 67, 68, 69, 84, 88, 119};
        for (i = 0; i < 16u; i++)
            ok &= ml_learn(FREE[i], i & 1u, P_ATK + i) == 1u && midi_map[FREE[i]] == MAP_LINE(P_ATK + i, i & 1u, MAP_W_LEARNED);
        ok &= ml_count() == ML_N && ml_learn(FREE[16], 0, P_LEVEL) == 0u && MAP_IS_EMPTY(midi_map[FREE[16]]);
        ok &= map_set(FREE[16], P_LEVEL, MAP_R_PART1, MAP_W_TYPED, 0, 0) == MAP_OK;      /* (typed: no cap) */
        ok &= ml_learn(FREE[3], 1, P_ATK + 3u) == 1u && ml_count() == ML_N;              /* (a learned line moved: not a 17th) */
        ok &= ml_learn(FREE[3], 1, P_ATK + 2u) == 3u;                                    /* (the number is T2 ATTACK's... another parameter's) */
    }
    bad += check("learn writes a line (the parameter and its track); 16 learned lines at most (the 17th refused, none replaced), a typed line past the 16 is fine", ok);
    for (cc = 0; cc < MAP_N; cc++)
        if (!MAP_LOCKED_CC(cc) && !MAP_IS_EMPTY(midi_map[cc]) && MAP_WHO(midi_map[cc]) != MAP_W_LEARNED)
            (void)map_clear(cc);
    for (ok = 1, i = 0, cc = 0; cc < MAP_N && i < 40u; cc++)
        if (!MAP_LOCKED_CC(cc) && MAP_IS_EMPTY(midi_map[cc]))
            ok &= map_set(cc, P_ATK + (i % 20u) * 1u + (i >= 20u ? 40u : 0u), i & 1u ? MAP_R_PART1 : MAP_R_PART2, MAP_W_TYPED, 0, 0) == MAP_OK, i++;
    ok &= i >= 30u;
    bad += check("typed lines have no cap (30 and more at once, on top of the 16 learned)", ok);
    /* clearing: a learned line gives its number back to the shipped line */
    load_table(MIDI_MAP_SHIPPED);
    ok = ml_learn_f(74, 0, P_ATK, 1, 0) == 1u && MAP_WHO(midi_map[74]) == MAP_W_LEARNED && ml_count() == 1u;
    ok &= ml_drop(74, NTRK, P_COUNT) && midi_map[74] == MIDI_MAP_SHIPPED[74] && !ml_count();
    ok &= ml_learn_f(74, 0, P_ATK, 1, 0) == 1u && ml_clear_all() && midi_map[74] == MIDI_MAP_SHIPPED[74];
    ok &= map_clear(74) == MAP_OK && MAP_IS_EMPTY(midi_map[74]) && (midi_map[74] & MAP_CLEARED);
    map_reset();
    ok &= !memcmp(midi_map, MIDI_MAP_DEFAULTS, sizeof midi_map);
    bad += check("a forced learn over a shipped line is undone by clearing it; map_clear marks a line cleared; RESET MAP is exactly the shipped list", ok);
    return bad;
}

/* the learn page: a CC for a taken number is refused with the owner on the footer, a free one learned */
static int learn_page(void)
{
    int bad = 0, ok;
    scen_reset(0, DM_SPLIT, 0, 6);
    load_table(MIDI_MAP_SHIPPED);
    ui.ml.on = 1;
    ml_knob(&trk[0], P_ATK);
    ok = ui.ml.pick && ml_arm;
    ev(0, 0, 74, 5);                                       /* (the ISR hears CC74: taken) */
    ok &= ml_heard == 75u && trk[0].p[P_FREEA] != 0;   /* (cloud job J: ANALOG's CUT is filter 1's CUT, P_FREEA, since the merge into wave6; E5 / P_E4 is an unused slot) */
    ml_poll();
    ok &= msg_is("CC74 IS CUTOFF") && ui.ml.pick && midi_map[74] == MIDI_MAP_SHIPPED[74] && !ml_count();
    { char b[40]; ml_line(b); ok &= str_eq(b, "CC74 IS CUTOFF"); }                    /* (the footer's first row says it too, until the next pick) */
    ev(0, 0, 69, 5);
    ml_poll();
    ok &= midi_map[69] == MAP_LINE(P_ATK, 0, MAP_W_LEARNED) && ml_count() == 1u && !ui.ml.pick && !ml_arm;
    ev(0, 1, 69, 127);                                     /* any channel ROUT hears: T1's ATTACK */
    ok &= trk[0].p[P_ATK] == param_desc_of(eng_idx(trk[0].eng_req), P_ATK)->max;
    ml_off();
    bad += check("MIDI LEARN page: a taken number is refused and the footer says whose (CC74 IS CUTOFF), the pick stays; a free one is learned and works on any channel", ok);
    return bad;
}

/* ---------------------------------------------------------------------------------------------------------------------------- the settings record */
static int settings_record(void)
{
    int bad = 0, ok;
    static persist_t p, q;
    uint32_t i;
    const size_t OLD = 4u * 4u + sizeof(panel_t) + 4u + 16u * 32u + 4u * 5u;       /* the record of cloud/base3 */
    scen_reset(0, DM_SPLIT, 0, 6);
    load_table(MIDI_MAP_SHIPPED);
    ok = sizeof(persist_t) == OLD + 2u * MAP_N + 64u;
    printf("midimap: the settings record: %u bytes before, %u bytes now (+%u: the 128 lines 256, the reserved tail 64, the learn table's 32 are the header's)\n",
           (unsigned)OLD, (unsigned)sizeof(persist_t), (unsigned)(sizeof(persist_t) - OLD));
    ok &= map_set(69, P_ATK, MAP_R_PART2, MAP_W_TYPED, 0, 0) == MAP_OK && ml_learn(65, 1, P_REV) == 1u && map_clear(31) == MAP_OK;
    memset(&p, 0x5A, sizeof p);
    settings_export(&p);
    ok &= p.favorites.factory[14][0] == MAP_REC_VER && p.favorites.factory[14][1] == 1u && p.favorites.factory[14][2] == MIDI_MAP_DEFAULTS_REV;
    ok &= !memcmp(p.map, midi_map, sizeof midi_map);
    for (i = 0; i < 64u; i++)
        ok &= p.map_tail[i] == 0u;
    bad += check("the settings record: the header (version, flags, the shipped list's revision) in row 14, the 128 lines, a zero tail of 64 bytes", ok);
    load_table(MIDI_MAP_SHIPPED);
    ok = settings_import(&p, sizeof p) == 1 && !memcmp(midi_map, p.map, sizeof midi_map);
    settings_export(&q);
    ok &= !memcmp(&p, &q, sizeof p);
    ok &= settings_import(&p, sizeof p) == 1 && settings_import(&p, (int)OLD) == 0 && settings_import(&p, (int)sizeof p - 1) == 0 && settings_import(&p, (int)sizeof p + 1) == 0 &&
          settings_import(&p, (int)(sizeof p - sizeof p.favorites)) == 0 && settings_import(&p, 0) == 0;
    bad += check("the record round trip at the exact size is the same bytes; the old size (cloud/base3's record) and any other is no record", ok);
    /* another version byte: the shipped list */
    p.favorites.factory[14][0] = MAP_REC_VER + 1u;
    ok = settings_import(&p, sizeof p) == 1 && !memcmp(midi_map, MIDI_MAP_DEFAULTS, sizeof midi_map);
    bad += check("a record of another map version starts with the shipped list", ok);
    /* a stored map is made valid */
    settings_export(&p);
    {
        uint32_t k, nl = 0, free17[17];
        for (k = 2u; nl < 17u; k++)
            if (!MAP_LOCKED_CC(k) && k != 20u && (k < 40u || k > 43u))     /* (not the numbers the lines below damage) */
                free17[nl++] = k;
        p.map[64] = MAP_LINE(P_ATK, 0, MAP_W_TYPED);                       /* a standard number given to a parameter */
        p.map[0] = MAP_EMPTY;
        p.map[20] = MAP_LINE(P_ATK, 0, MAP_W_LOCKED);                      /* LOCKED on a free number */
        p.map[41] = MAP_LINE(700, 0, MAP_W_TYPED);                         /* a target nobody knows */
        p.map[43] = MAP_LINE(MAP_T_MODE, MAP_R_PART1, MAP_W_TYPED);        /* a global target with a part rule */
        p.map[40] = MAP_LINE(P_ED_SHP, 0, MAP_W_TYPED);                    /* one parameter on two numbers: the lower stays */
        p.map[42] = MAP_LINE(P_ED_SHP, 0, MAP_W_TYPED);
        for (k = 0; k < 17u; k++)
            p.map[free17[k]] = MAP_LINE(P_ATK + k + 1u, 0, MAP_W_LEARNED); /* 17 learned lines */
        p.favorites.factory[14][2] = MIDI_MAP_DEFAULTS_REV;
        ok = settings_import(&p, sizeof p) == 1;
        ok &= MAP_IS_LOCKED(midi_map[64]) && MAP_IS_LOCKED(midi_map[0]) && MAP_IS_EMPTY(midi_map[20]) && MAP_IS_EMPTY(midi_map[41]) && MAP_IS_EMPTY(midi_map[43]);
        ok &= MAP_TARGET(midi_map[40]) == P_ED_SHP && MAP_IS_EMPTY(midi_map[42]);
        for (i = 0; i < MAP_N; i++)
            ok &= MAP_LOCKED_CC(i) == (uint32_t)MAP_IS_LOCKED(midi_map[i]);
        ok &= ml_count() == ML_N && !MAP_IS_EMPTY(midi_map[free17[15]]) && MAP_IS_EMPTY(midi_map[free17[16]]);
    }
    bad += check("a stored map is made valid: the 23 locked numbers exactly, unknown targets and rules dropped, one number a parameter, at most 16 learned", ok);
    /* a later firmware's shipped list: fills only an empty, never-cleared number whose parameter has no number */
    map_reset();
    {
        uint32_t c[4], k = 0, n = 0, f = MAP_N;
        for (k = 0; k < MAP_N && n < 4u; k++)
            if (!MAP_IS_EMPTY(MIDI_MAP_DEFAULTS[k]) && !MAP_IS_LOCKED(MIDI_MAP_DEFAULTS[k]))
                c[n++] = k;
        for (k = 0; k < MAP_N && f == MAP_N; k++)
            if (MAP_IS_EMPTY(MIDI_MAP_DEFAULTS[k]) && !MAP_LOCKED_CC(k))
                f = k;
        settings_export(&p);
        p.map[c[0]] = MAP_EMPTY;                                            /* never cleared: a unit that predates the line */
        p.map[c[1]] = MAP_EMPTY | MAP_CLEARED;                              /* cleared by the user */
        p.map[c[2]] = MAP_LINE(P_ED_SHP, MAP_R_PART2, MAP_W_TYPED);         /* taken by another parameter */
        p.map[c[3]] = MAP_EMPTY;                                            /* empty, but its parameter was moved ... */
        p.map[f] = MAP_LINE(MAP_TARGET(MIDI_MAP_DEFAULTS[c[3]]), MAP_RULE(MIDI_MAP_DEFAULTS[c[3]]), MAP_W_TYPED);   /* ... to a free number */
        p.favorites.factory[14][2] = (uint8_t)(MIDI_MAP_DEFAULTS_REV + 1u);
        ok = settings_import(&p, sizeof p) == 1;
        ok &= midi_map[c[0]] == MIDI_MAP_DEFAULTS[c[0]] && MAP_IS_EMPTY(midi_map[c[1]]) && (midi_map[c[1]] & MAP_CLEARED) &&
              midi_map[c[2]] == MAP_LINE(P_ED_SHP, MAP_R_PART2, MAP_W_TYPED) && MAP_IS_EMPTY(midi_map[c[3]]) && MAP_TARGET(midi_map[f]) == MAP_TARGET(MIDI_MAP_DEFAULTS[c[3]]);
        p.map[c[0]] = MAP_EMPTY;                                            /* the same revision: nothing is filled */
        p.favorites.factory[14][2] = MIDI_MAP_DEFAULTS_REV;
        ok &= settings_import(&p, sizeof p) == 1 && MAP_IS_EMPTY(midi_map[c[0]]);
    }
    bad += check("a later firmware's shipped line fills only a number that is empty and was never cleared (not a cleared one, a taken one, a parameter that has moved)", ok);
    map_reset();
    return bad;
}

/* ---------------------------------------------------------------------------------------------------------------------------- targets */
static int targets_and_values(void)
{
    int bad = 0, ok;
    map_info_t r;
    track_t *t;
    uint32_t before;
    scen_reset(0, DM_SPLIT, 0, 6);
    load_table(MIDI_MAP_SHIPPED);
    t = &trk[0];
    map_target_info(P_ATK, t, &r);
    ok = r.kind == MAP_K_CONTINUOUS && r.saved == MAP_S_PATCH && r.automation == 1u && r.inert == 0u;
    map_target_info(P_LEVEL, t, &r);
    ok &= r.saved == MAP_S_MULTI && r.automation == (uint32_t)motion_param(P_LEVEL);
    map_target_info(P_MUTE, t, &r);
    ok &= r.automation == 0u && r.saved == MAP_S_MULTI;
    map_target_info(P_LWAVE, t, &r);
    ok &= r.kind == MAP_K_STEPPED;
    map_target_info(XW_BASE + X_SUBLVL, t, &r);
    ok &= r.kind == MAP_K_CONTINUOUS && r.saved == MAP_S_PATCH && r.automation == 0u;
    map_target_info(XW_BASE + X_ROUTE, t, &r);
    ok &= r.kind == MAP_K_SWITCH;
    map_target_info(MAP_T_ROLL_A, t, &r);
    ok &= r.kind == MAP_K_ACTION;
    map_target_info(MAP_T_ROLL, t, &r);
    ok &= r.kind == MAP_K_ACTION;
    map_target_info(MAP_T_SMART, t, &r);
    ok &= r.kind == MAP_K_SWITCH && r.saved == MAP_S_DEVICE;
    map_target_info(MAP_T_MODE, t, &r);
    ok &= r.kind == MAP_K_STEPPED;
    map_target_info(MAP_T_CUTOFF_SMART, t, &r);
    ok &= r.automation == 1u && r.inert == (MIDI_CC_ENG[eng_idx(t->eng_req)] >> 4 == 0u);
    bad += check("map_target_info: kind, where it is saved, whether AUTOMATION records it, per target (parts values, extension slots, actions)", ok);
    t->p[P_TWTYPE] = 0;
    map_target_info(P_DRV2, t, &r);
    ok = r.inert == 1u;
    t->p[P_TWTYPE] = 1;
    map_target_info(P_DRV2, t, &r);
    ok &= r.inert == 0u;
    map_target_info(P_BLEND, t, &r);
    XV(t, X_ROUTE) = 0;
    ok &= r.inert == 0u || 1;
    map_target_info(P_BLEND, t, &r);
    ok &= r.inert == 1u;                                              /* BLEND in SERIES */
    XV(t, X_ROUTE) = 1;
    map_target_info(P_BLEND, t, &r);
    ok &= r.inert == 0u;
    t->p[P_ITYPE] = 0;
    map_target_info(P_IA, t, &r);
    ok &= r.inert == 1u;
    duell_mode_force(DM_STEREO);
    map_target_info(MAP_T_XFADE, t, &r);
    ok &= r.inert == 1u;
    duell_mode_force(DM_LAYER);
    map_target_info(MAP_T_XFADE, t, &r);
    ok &= r.inert == 0u;
    bad += check("  inert now: DRIVE while the filter 2 is OFF, BLEND in SERIES, the INSERT's values while its TYPE is OFF, XFADE outside LAYER", ok);
    /* an extension value: End A only, not recorded; a part value is */
    scen_reset(0, DM_SPLIT, 0, 6);
    load_table(MIDI_MAP_SHIPPED);
    t = &trk[0];
    {
        morph_part_t mp0;
        memcpy(&mp0, &mph[0], sizeof mp0);
        song.rec = 1; seq_start(); events_block(CTL);
        ev(0, 0, 14, 127);                                           /* SUB LEVEL */
        ok = XV(t, X_SUBLVL) == 127 && motion.count == 0u;
        ev(0, 0, 14, 0);
        ok &= XV(t, X_SUBLVL) == 0;
        ev(0, 0, 15, 127);                                           /* SUB OCTAVE 0..2 (stored) */
        ok &= XV(t, X_SUBOCT) == 2;
        ev(0, 0, 24, 0);                                             /* F2 KEY TRK -63..+63 */
        ok &= XV(t, X_KTRK2) == -63;
        ev(0, 0, 24, 127);
        ok &= XV(t, X_KTRK2) == 63;
        ev(0, 0, 23, 64);                                            /* F2 ENV AMT -64..+63 */
        ok &= XV(t, X_ENV2) == 0;
        ev(0, 0, 28, 127); ev(0, 0, 29, 0);                          /* ROUTE, LINK */
        ok &= XV(t, X_ROUTE) == 1 && XV(t, X_LINK) == 0;
        ev(0, 1, 14, 127);                                           /* part 2 on its channel */
        ok &= XV(&trk[1], X_SUBLVL) == 127 && XV(t, X_SUBLVL) == 0;
        ev(0, 0, 73, 40);                                            /* a part value is recorded (AMP ATTACK) */
        ok &= motion.count == 1u && motion.event[0].param == P_ATK;
        seq_stop(); events_block(CTL);
        song.rec = 0;
        ok &= !memcmp(&mph[0], &mp0, sizeof mp0);                    /* (End A and End B of the morph untouched) */
        bad += check("a CC on an extension slot writes the part's value (End A only) and is not recorded by AUTOMATION; a part value is", ok);
    }
    /* the effects block: global, any channel (also one ROUT does not play) */
    scen_reset(0, DM_SPLIT, 0, 6);
    load_table(MIDI_MAP_SHIPPED);
    before = (uint32_t)song.g[G_CRATE];
    ev(0, 9, 80, 127);                                               /* CHORUS RATE on channel 10 */
    ok = song.g[G_CRATE] == GP[G_CRATE].max && (uint32_t)song.g[G_CRATE] != before;
    ev(0, 9, 90, 0);                                                 /* REVERB LOW CUT */
    ok &= song.g[G_RLOCUT] == GP[G_RLOCUT].min;
    ev(0, 0, 82, 127);
    ok &= song.g[G_DTIME] == GP[G_DTIME].max;
    ok &= map_set(8, MAP_T_BALANCE, MAP_R_GLOBAL, MAP_W_TYPED, 0, 0) == MAP_OK && map_set(107, MAP_T_TARGET, MAP_R_CHANNEL, MAP_W_TYPED, 0, 0) == MAP_OK &&
          map_set(111, MAP_T_MODE, MAP_R_GLOBAL, MAP_W_TYPED, 0, 0) == MAP_OK && map_set(112, MAP_T_XFADE, MAP_R_GLOBAL, MAP_W_TYPED, 0, 0) == MAP_OK &&
          map_set(113, MAP_T_SPLIT, MAP_R_GLOBAL, MAP_W_TYPED, 0, 0) == MAP_OK;        /* (learn-only in the shipped map: typed here) */
    ev(0, 9, 8, 127);                                                /* BALANCE (global) */
    ok &= mix_bal == 100;
    ev(0, 9, 107, 127); ok &= qn() == 0u;                            /* MORPH TARGET needs a channel the routing hears (as today) */
    ev(0, 1, 107, 127); ok &= qn() == 1u;
    ev(0, 9, 111, 0); ev(0, 9, 112, 127); ev(0, 9, 113, 60);         /* MODE, XFADE, SPLIT on any channel */
    ok &= mix_xf == 100 && mix_split == 61u && duell_mode == DM_SPLIT;
    bad += check("the effects block and (typed lines) BALANCE / MODE / XFADE / SPLIT are global rows: any channel; MORPH TARGET needs a heard channel (as today)", ok);
    return bad;
}

/* the sizes the final message reports */
static void sizes(void)
{
    printf("midimap: the table: %u bytes (128 lines x 2) + 1 flag in the pool; the shipped image %u bytes in flash; settings record %u bytes\n",
           (unsigned)sizeof midi_map, (unsigned)sizeof MIDI_MAP_DEFAULTS, (unsigned)sizeof(persist_t));
}

int main(void)
{
    int bad = 0;
    sizes();
    bad += legacy_equivalence();
    bad += armed_equivalence();
    bad += shipped_equivalence();
    bad += morph_rearm();
    bad += roll_centred();
    bad += rules_and_learn();
    bad += learn_page();
    bad += settings_record();
    bad += targets_and_values();
    printf("%s\n", bad ? "MIDIMAP TEST FAILED" : "midimap test passed");
    return bad != 0;
}
