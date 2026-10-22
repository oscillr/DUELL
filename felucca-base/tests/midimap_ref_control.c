/* SPDX-License-Identifier: GPL-3.0-only
 * Adapted from MIDI control contribution by ChanceTheMaker (2026).
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Shared USB/TRS channel controls. Included by seq.c after its input helpers.
 * USB and TRS intentionally share channel state, matching the existing routing.
 * A synth part has one live bend/wheel state; channels assigned to the same part
 * share it (last controller wins). CC1 keeps the existing matrix routing. */
typedef struct {
    int16_t bend;                           /* signed 14-bit value, zero = centre */
    uint8_t wheel, pedal, targets;
    uint8_t owned[NTRK];                    /* held/pedal notes per track, at most 128 per channel */
    uint8_t ready, semis, cents;
    uint8_t rpn_msb, rpn_lsb;
    uint8_t bank_msb, bank_lsb;             /* DUELL: CC0 / CC32, applied at the channel's next PROGRAM CHANGE (midi_pc) */
} midi_channel_t;
static midi_channel_t midi_ch[16];
/* Low bits: track + 1. High bit: key released, held by its channel's pedal. */
static uint8_t midi_sel_on[16][128];
#define midi_notes midi_sel_on
static uint16_t midi_owners[NTRK];           /* avoids rescanning all 2048 entries for CC123 */
#define MIDI_PEDAL_NOTE 0x80u

static midi_channel_t *midi_channel(uint32_t ch)
{
    midi_channel_t *c = &midi_ch[ch];
    if (!c->ready) {
        c->semis = 2;
        c->rpn_msb = c->rpn_lsb = 127;
        c->ready = 1;
    }
    return c;
}

static uint32_t midi_targets(uint32_t ch)
{
    return midi_channel(ch)->targets | (1u << trk_index(midi_track(ch)));
}

static void midi_expression_1(track_t *t, const midi_channel_t *c)
{
    int32_t range = ((int32_t)c->semis * 100 + c->cents) * 256 / 100;
    midi_bend_target[trk_index(t)] = (int32_t)c->bend * range / (c->bend < 0 ? 8192 : 8191);
}
/* (DUELL step 5, LAYER: part 1's bend is part 2's too) */
static void midi_expression(track_t *t, const midi_channel_t *c)
{
    midi_expression_1(t, c);
    if (duell_mode == DM_LAYER && t == trk)
        midi_expression_1(&trk[1], c);
}

static void midi_expression_channel(uint32_t ch)
{
    uint32_t i, mask = midi_targets(ch);
    for (i = 0; i < NPART; i++)
        if (mask & (1u << i))
            midi_expression(&trk[i], midi_channel(ch));
}

/* a MIDI note of track t sounds note: one played as itself, or a tone of one played as a chord (chord.c) */
static int midi_note_held(const track_t *t, uint32_t note)
{
    uint32_t ch, id = trk_index(t) + 1u, own = duell_mode == DM_LAYER && id == 2u ? 1u : id;   /* (LAYER: part 2's copy of part 1's) */
    for (ch = 0; ch < 16u; ch++)
        if ((midi_notes[ch][note] & 0x7Fu) == own && !mchord_of(ch, note, id))
            return 1;
    return mchord_held(id, note);
}

/* a key held on track t sounds note (the key's own note, or a tone of its chord) */
static int midi_local_held(const track_t *t, uint32_t note)
{
    uint32_t k, i;
    for (k = 0; k < 27u; k++) {
        if (kb_chn[k] && kb_trk[k] == trk_index(t))
            for (i = 0; i < kb_chn[k]; i++)
                if (kb_chord[k][i] == note)
                    return 1;
        if (t == &trk[1] && kb_chn2[k])             /* (LAYER: part 2's copy of the keys) */
            for (i = 0; i < kb_chn2[k]; i++)
                if (kb_chord2[k][i] == note)
                    return 1;
    }
    return 0;
}

static void midi_release(uint32_t ch, uint32_t note)
{
    uint32_t id = midi_notes[ch][note] & 0x7Fu;
    midi_notes[ch][note] = 0;
    if (id) {
        midi_owners[id - 1u]--;
        if (!--midi_ch[ch].owned[id - 1u])
            midi_ch[ch].targets &= (uint8_t)~(1u << (id - 1u));
    }
    if (id) {
        mchord_t *m = mchord_of(ch, note, id);
        uint8_t nn[CHORD_MAX];
        uint32_t n = 1, i;
        nn[0] = (uint8_t)note;
        if (m) {                                  /* a chord: exactly the notes it started */
            n = m->n;
            for (i = 0; i < n; i++)
                nn[i] = m->note[i];
            m->id = 0;
        }
        for (i = 0; i < n; i++)
            if (!midi_local_held(&trk[id - 1u], nn[i]))
                input_off(&trk[id - 1u], nn[i]);  /* input_off also checks other MIDI owners */
        if (duell_mode == DM_LAYER && id == 1u) {  /* part 2's copy, its own chord */
            mchord_t *m2 = mchord_of(ch, note, 2u);
            n = 1;
            nn[0] = (uint8_t)note;
            if (m2) {
                n = m2->n;
                for (i = 0; i < n; i++)
                    nn[i] = m2->note[i];
                m2->id = 0;
            }
            for (i = 0; i < n; i++)
                if (!midi_local_held(&trk[1], nn[i]))
                    input_off(&trk[1], nn[i]);
        }
    }
}

/* a MIDI note-on (ch, note) of track t: its chord (chord.c) or the note alone. A note another key or MIDI
 * note holds already sounds: not started again */
static void midi_play(track_t *t, uint32_t ch, uint32_t note, uint32_t vel)
{
    uint8_t nn[CHORD_MAX];
    uint32_t n = chord_build(t, note, nn), i, f = 0, lay, rt;
    if (n > 1u || nn[0] != note)
        for (f = 0; f < MCHORD_N && mchord[f].id; f++)
            ;
    if (f == MCHORD_N) {                          /* no room to keep a chord: the note alone */
        n = 1;
        nn[0] = (uint8_t)note;
    }
    lay = duell_mode == DM_LAYER && t == trk;
    rt = lay ? lay_route(note) : 3u;                /* (LAYER, SPLIT set: below it part 1 alone, at or above part 2 alone) */
    if (rt & 1u) {
        for (i = 0; i < n; i++)
            if (!midi_note_held(t, nn[i]) && !midi_local_held(t, nn[i])) {
                if (lay)
                    lay_input(t, nn[i], vel);
                else
                    input_on(t, nn[i], vel);
            }
        if (n > 1u || nn[0] != note) {
            mchord_t *m = &mchord[f];
            m->ch = (uint8_t)ch;
            m->src = (uint8_t)note;
            m->n = (uint8_t)n;
            for (i = 0; i < n; i++)
                m->note[i] = nn[i];
            m->id = (uint8_t)(trk_index(t) + 1u);
        }
    }
    if (lay && (rt & 2u)) {     /* LAYER: part 2 gets the note too, its own chord and arp (id 2) */
        track_t *t2 = &trk[1];
        n = chord_build(t2, note, nn);
        f = 0;
        if (n > 1u || nn[0] != note)
            for (f = 0; f < MCHORD_N && mchord[f].id; f++)
                ;
        if (f == MCHORD_N) {
            n = 1;
            nn[0] = (uint8_t)note;
        }
        for (i = 0; i < n; i++)
            if (!midi_note_held(t2, nn[i]) && !midi_local_held(t2, nn[i]))
                lay_input(t2, nn[i], vel);
        if (n > 1u || nn[0] != note) {
            mchord_t *m = &mchord[f];
            m->ch = (uint8_t)ch;
            m->src = (uint8_t)note;
            m->n = (uint8_t)n;
            for (i = 0; i < n; i++)
                m->note[i] = nn[i];
            m->id = 2u;
        }
    }
}

static void midi_note_event(uint32_t ch, uint32_t note, uint32_t vel)
{
    midi_channel_t *c = midi_channel(ch);
    uint32_t id = midi_notes[ch][note] & 0x7Fu;
    if (vel) {
        track_t *t = midi_track(ch);
        /* Repeated notes replace the previous press, including a pedal-held one. */
        if (id)
            midi_release(ch, note);
        midi_expression(t, c);
        if (t != TSEL && duell_mode != DM_LAYER)       /* (LAYER: part 1 plays both, whichever is selected) */
            midi_hint = (uint8_t)(trk_index(t) + 1u);
        c->targets |= (uint8_t)(1u << trk_index(t));
        midi_play(t, ch, note, vel);
        midi_notes[ch][note] = (uint8_t)(trk_index(t) + 1u);
        midi_owners[trk_index(t)]++;
        c->owned[trk_index(t)]++;
    } else if (id) {
        if (c->pedal)
            midi_notes[ch][note] |= MIDI_PEDAL_NOTE;
        else
            midi_release(ch, note);
    }
}

static void midi_pedal_up(uint32_t ch)
{
    uint32_t note;
    midi_channel(ch)->pedal = 0;
    for (note = 0; note < 128u; note++)
        if (midi_notes[ch][note] & MIDI_PEDAL_NOTE)
            midi_release(ch, note);
}

/* Preset/project panic and CC120 must discard ownership so a later pedal-up
 * or note-off cannot release notes subsequently started on another patch. */
static void __attribute__((noinline)) midi_forget_track(uint32_t track)
{
    uint32_t ch, note;
    for (ch = 0; ch < 16u; ch++) {
        if (!(midi_ch[ch].targets & (1u << track)))
            continue;
        for (note = 0; note < 128u; note++)
            if ((midi_notes[ch][note] & 0x7Fu) == track + 1u)
                midi_notes[ch][note] = 0;
        midi_ch[ch].targets &= (uint8_t)~(1u << track);
        midi_ch[ch].owned[track] = 0;
    }
    midi_owners[track] = 0;
    mchord_forget(track);
    midi_bend_q8[track] = midi_bend_target[track] = 0;
}

static void midi_silence_track(uint32_t track)
{
    track_t *t = &trk[track];
    uint32_t i;
    trk_all_off(t);
    t->nheld = t->arp_phys = t->rh_n = 0;
    arp_forget(t);
    t->seq_n = t->seq_hold = t->slide_glide = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active)
            voice_kill(&t->v[i]);             /* one-block fade, regardless of RELEASE */
    midi_forget_track(track);
}

static int midi_track_held(uint32_t track)
{
    uint32_t k;
    if (midi_owners[track])
        return 1;
    for (k = 0; k < 27u; k++)
        if ((fm1_in.notes & (1u << k)) && kb_trk[k] == track)
            return 1;
    return 0;
}

/* The standard CC map (#103): always on, these CCs only, on the track the channel plays (ROUT as notes). A CC
 * sets its parameter as a knob would, 0..127 over the parameter's range (64: the middle of a bipolar one), and
 * AUTOMATION records it as a knob move (motion_capture: the selected track, armed and playing). CC74 / CC71 are
 * the engine's own cutoff / resonance (MIDI_CC_ENG); an engine without one ignores them. On the engines with no filter
 * of their own (engine_t.ufilt) they are the multimode filter's CUT and RES (MCC_UF; DUELL 0.2.9: CC74 0..127 over the whole CUT dial, 30 Hz
 * .. the open end; 0.2.2's 64 = OFF is gone: a MIDI LEARN map on CUT spans the new dial the same way). CC121 resets none
 * of them (RP-015: they are the sound's) */
#define MCC_CUT 0xFEu
#define MCC_RES 0xFFu
static const uint8_t MIDI_CC_MAP[][2] = {
    {5, P_GLIDE}, {7, P_LEVEL}, {10, P_PAN}, {71, MCC_RES}, {72, P_REL}, {73, P_ATK}, {74, MCC_CUT}, {75, P_DEC},
    {91, P_REV}, {93, P_CHOR}, {94, P_DLY},
};
/* per engine (ENGINES[] order): CC74's E index + 1 in the high nibble, CC71's in the low one, 0 = none; MCC_UF (0xFF): the engine's
 * multimode filter, CC74 its CUT and CC71 its RES (DUELL 0.2.2) */
#define MCC_UF 0xFFu
static const uint8_t MIDI_CC_ENG[NENGINES] = {
    MCC_UF, 0x00, MCC_UF, MCC_UF, 0x00, MCC_UF, MCC_UF, MCC_UF,   /* ANALOG UF (cloud job J: was CUT RES; the reference follows the engines), - (1, DIGITAL: retired), PHASE UF, LOFI UF, - (SAMPLE: retired), VOICE UF,
                                                       * TRIO UF (was CUT RES), WHEEL UF */
    0x00, MCC_UF, 0x00, MCC_UF, MCC_UF,               /* - (GRAIN: retired), PHYS UF, - (10, DRUM: retired in DUELL 0.2.14), NOISE UF (was FREQ RES), FM6 UF */
    0x00,                                             /* - (13, SLICE: retired in DUELL 0.2.9) */
    MCC_UF,                                           /* WAVE UF (DUELL) */
};
/* a CC sets parameter id of track t as a knob would: 0..127 over its range */
static void midi_cc_set(track_t *t, uint32_t id, uint32_t value)
{
    const param_desc_t *d = param_desc_of(eng_idx(t->eng_req), id);
    int32_t v = d->min + ((int32_t)value * (d->max - d->min) + 63) / 127;
    if (d->max <= d->min || t->p[id] == v || id == P_DKSRC)   /* (DUCK SRC is the owner's switch: PART on one part only, params.c; a CC does not set it) */
        return;
    t->p[id] = (int16_t)v;
    (void)motion_capture(t, id, (int16_t)v);
}
/* MIDI LEARN (1.5, Discussion #170): up to ML_N CCs, each set to one parameter of one track, whatever channel it
 * comes on (a channel ROUT ignores stays ignored) and whichever track that channel plays. Kept with the settings in
 * the 32 bytes of favorites.factory[14] (no engine 14: favorites.c; all 0 in every older setting = nothing learned),
 * an entry two bytes, as a 16-bit value LSB first: bits 0..6 the CC, 7..8 the track, 9..15 the parameter's code, 0 =
 * an empty entry: a common parameter its P_* id + 1 (1 .. ML_E0 - 1: their ids never move, new ones go before P_E0),
 * an engine parameter ML_E0 + k for P_E0 + k (P_E0 moves when common ones are added). A learned CC no longer reaches
 * the standard map (#103) on any channel: the controller is the user's now. The CCs with a meaning of their own
 * (midi_control's cases, CC0 / 32 bank select, CC11 EXPR, CC96 / 97 data increment, CC120..127) are never learned. The UI
 * (midi_learn.c) writes the entries with the interrupts off; ml_arm / ml_heard hand a CC heard while learning to it */
#define ML_N 16u
#define ML_E0 120u
typedef char ml_ids_fit[P_E0 < ML_E0 && P_COUNT - P_E0 == 8 ? 1 : -1];
static uint8_t *const ml_tab;               /* favorites.factory[14] (midi_learn.c: a build without the UI, none) */
static volatile uint8_t *const ml_io;       /* ui.ml.io (midi_learn.c; not globals here: the ISR's data keeps its layout) */
#define ml_arm (ml_io[0])                   /* the UI waits for a CC (MIDI LEARN with a parameter picked) */
#define ml_heard (ml_io[1])                 /* .. the CC that came meanwhile, + 1 (0 none) */
static uint32_t ml_id(uint32_t code)        /* an entry's code -> P_* id, P_COUNT: none this firmware knows */
{
    return code >= ML_E0 ? P_E0 + code - ML_E0 : code && code <= P_E0 ? code - 1u : P_COUNT;
}
static int ml_free_cc(uint32_t cc)          /* (CC1 6 38 64 98..101 120..127: midi_control's own) */
{
    return cc && cc < 120u && cc != 1u && cc != 6u && cc != 11u && cc != 32u && cc != 38u && cc != 64u &&
           (cc < 96u || cc > 101u);
}
/* 1: cc is learned (set here) or taken by MIDI LEARN waiting for one */
static int __attribute__((noinline)) midi_learned(uint32_t cc, uint32_t value)
{
    uint32_t i, e, id, hit = 0;
    if (!ml_tab || !ml_free_cc(cc))
        return 0;
    if (ml_arm) {
        if (!ml_heard)
            ml_heard = (uint8_t)(cc + 1u);
        return 1;
    }
    for (i = 0; i < ML_N; i++) {
        e = ml_tab[2u * i] | (uint32_t)ml_tab[2u * i + 1u] << 8;
        if ((e >> 9) && (e & 127u) == cc) {
            if ((id = ml_id(e >> 9)) < P_COUNT)
                midi_cc_set(&trk[(e >> 7) & 3u], id, value);
            hit = 1;
        }
    }
    return hit;
}

static void __attribute__((noinline)) midi_cc_map(track_t *t, uint32_t cc, uint32_t value)
{
    uint32_t i, id = 0xFFFFu, e;
    if (midi_learned(cc, value))                /* (a learned CC is the user's: not the map's) */
        return;
    for (i = 0; i < NELEM(MIDI_CC_MAP); i++)
        if (MIDI_CC_MAP[i][0] == cc)
            id = MIDI_CC_MAP[i][1];
    if (id >= MCC_CUT) {
        if (id == 0xFFFFu)
            return;
        e = MIDI_CC_ENG[eng_idx(t->eng_req)] >> (id == MCC_CUT ? 4 : 0) & 15u;
        if (!e)
            return;
        id = e == 15u ? (id == MCC_CUT ? P_FREEA : P_FREEB) : P_E0 + e - 1u;   /* (15: the multimode filter's CUT, RES) */
    }
    midi_cc_set(t, id, value);
}

/* DUELL 4b, SPEC 4.4 / 3.4a: the MORPH's CCs, on each part's channel (the part the channel plays, as its notes: ROUT
 * CH1-4 channel 1 part 1, channel 2 part 2, the hidden parts' channels ignored; SEL the selected part):
 *   CC 20 the fader 0..127; CC 25 AMOUNT 0..127 -> 0..100 %; CC 111 TARGET 0-42 PART 1, 43-85 PART 2, 86-127 BOTH (global; 0.2.9:
 *   ALGORITHM's selector, ui_input.c algo_step: BOTH keeps the selected part as the edit target);
 *   CC 21 ROLL A, 22 ROLL B: a value >= 64 fires once; the channel's CC re-arms below 64. CC 23 UNDO: 1..63 undoes End A once,
 *   64..127 End B once (the knob's left / right), 0 re-arms. CC 24 (KEEP, removed in
 *   SPEC 3.5) is unassigned; CC 113 SMART, 114 ARP, 115 PITCH: 0-63 off, 64-127 on (global).
 * TARGET a part: a CC acts on its channel's part only; BOTH: a CC on either channel acts on both parts (morph.c
 * morph_mask). None of these numbers is in the standard map (#103) or a MIDI-defined controller (20-31, 102-119 are
 * undefined in MIDI 1.0); MIDI LEARN can still take one (a learned CC is the user's, as for the standard map). Queued
 * for the main loop (morph.c morph_push: a roll is no work for the audio ISR).
 * The MOD WHEEL (CC1) moves the fader too, always (no switch), as CC 20 does (same queue, same
 * TARGET rules), while it stays the matrix's MODW source (mod.c mod_midi); in LAYER it moves both parts' faders */
#define MQ_PART_ALL 0x7Fu                                  /* (morph_push's part: every shown part, ui_input.c morph_poll) */
static uint8_t midi_morph_fired[16];                      /* bit k: CC 21 + k at or above 64 on the channel (fired) */
#define MIDI_MORPH_CC(cc) (((cc) >= 20u && (cc) <= 23u) || (cc) == 25u || (cc) == 111u || ((cc) >= 113u && (cc) <= 115u))
static void __attribute__((noinline)) midi_morph(uint32_t ch, uint32_t cc, uint32_t value)
{
    uint32_t pi = trk_index(midi_track(ch)), b;
    if (pi >= NSHOWN)
        return;
    if (cc == 20u) {
        morph_push(MQ_FADER, pi, value);
    } else if (cc == 25u) {
        morph_push(MQ_AMOUNT, pi, (value * 100u + 63u) / 127u);
    } else if (cc == 111u) {
        morph_push(MQ_TARGET, pi, value < 43u ? 0u : value < 86u ? 1u : 2u);
    } else if (cc >= 113u) {
        morph_push(cc == 113u ? MQ_SMART : cc == 114u ? MQ_ARP : MQ_PITCH, pi, value >= 64u);
    } else {
        b = 1u << (cc - 21u);
        if (cc == 23u) {
            if (!value) {
                midi_morph_fired[ch] &= (uint8_t)~b;
            } else if (!(midi_morph_fired[ch] & b)) {
                midi_morph_fired[ch] |= (uint8_t)b;
                morph_push(MQ_UNDO, pi, value >= 64u);
            }
        } else if (value < 64u) {
            midi_morph_fired[ch] &= (uint8_t)~b;        /* released: armed again */
        } else if (!(midi_morph_fired[ch] & b)) {
            midi_morph_fired[ch] |= (uint8_t)b;
            morph_push(MQ_ROLL, pi, cc == 22u);
        }
    }
}

static void midi_control(uint32_t ch, uint32_t cc, uint32_t value)
{
    midi_channel_t *c = midi_channel(ch);
    uint32_t i, mask;
    switch (cc) {
    case 1:
        c->wheel = (uint8_t)value;
        if (trk_index(midi_track(ch)) < NSHOWN)       /* DUELL: the wheel is the MORPH fader too (as CC 20) */
            morph_push(MQ_FADER, duell_mode == DM_LAYER ? MQ_PART_ALL : trk_index(midi_track(ch)), value);
        break;
    case 0:                                        /* DUELL: BANK SELECT MSB / LSB, kept for the next PROGRAM CHANGE */
        c->bank_msb = (uint8_t)value;
        break;
    case 32:
        c->bank_lsb = (uint8_t)value;
        break;
    case 120:                                      /* All Sound Off: ignores the pedal */
        mask = midi_targets(ch);
        if (duell_mode == DM_LAYER && (mask & 1u))
            mask |= 2u;                            /* (LAYER: part 1's notes sound on part 2 too) */
        for (i = 0; i < NTRK; i++)
            if (mask & (1u << i))
                midi_silence_track(i);
        break;
    case 123:                                      /* All Notes Off: normal releases, honours pedal */
        mask = midi_targets(ch);
        for (i = 0; i < 128u; i++)
            if (midi_notes[ch][i])
                midi_note_event(ch, i, 0);
        for (i = 0; i < NTRK; i++)
            if ((mask & (1u << i)) && !midi_track_held(i)) {
                trk_all_off(&trk[i]);
                trk[i].nheld = trk[i].arp_phys = trk[i].rh_n = 0;
                arp_forget(&trk[i]);
            }
        break;
    case 121:                                      /* Reset All Controllers, keep bend sensitivity */
        c->bend = 0;
        c->wheel = 0;
        c->rpn_msb = c->rpn_lsb = 127;
        midi_morph_fired[ch] = 0;                   /* (the MORPH's triggers armed again) */
        midi_expression_channel(ch);
        midi_pedal_up(ch);
        break;
    case 64:
        if (value >= 64u)
            c->pedal = 1;
        else
            midi_pedal_up(ch);
        break;
    case 101: c->rpn_msb = (uint8_t)value; break;
    case 100: c->rpn_lsb = (uint8_t)value; break;
    case 99: case 98:                              /* NRPN selection cancels RPN data entry */
        c->rpn_msb = c->rpn_lsb = 127;
        break;
    case 6: case 38:
        if (!c->rpn_msb && !c->rpn_lsb) {           /* RPN 0: +/-0..24 semitones, 0..99 cents */
            if (cc == 6u)
                c->semis = (uint8_t)(value > 24u ? 24u : value);
            else
                c->cents = (uint8_t)(value > 99u ? 99u : value);
            midi_expression_channel(ch);
        }
        break;
    default:
        if (!MIDI_MORPH_CC(cc))
            midi_cc_map(midi_track(ch), cc, value);
        else if (!midi_learned(cc, value))              /* (a learned one is the user's) */
            midi_morph(ch, cc, value);
        break;
    }
}

/* DUELL step 5 (SPEC 5b.1, 4.4): CC 112 sets the output MODE, global (not a part's): 0-42 SPLIT, 43-85 STEREO, 86-127
 * LAYER, on any channel (also one ROUT does not play: the MODE is no part's), a value that keeps the mode does nothing. Free
 * in the map: the standard CCs (#103) and the MORPH's (20-25, 111) are others, SPEC 4.4's block ends at 110, and MIDI 1.0
 * leaves 102-119 undefined. MIDI LEARN can take it (ml_free_cc): learned, or MIDI LEARN waiting for a CC, it is the
 * user's, as any learned CC (only on a channel ROUT plays). Queued for nothing: mode_apply runs here, in the ISR */
#define MIDI_MODE_CC 112u
/* the MODE page's other three, global too (any channel; learned = the user's, as CC 112): CC 116 BALANCE (0..127 -> -100..+100,
 * 64 = 0), CC 117 XFADE (0..127 -> 0..100 %), CC 118 SPLIT (0 = OFF, 1..127 = the note n). Free: not in MIDI_CC_MAP, not the
 * morph's (20-25, 111, 113-115), not Felucca's panics (120-127) */
#define MIDI_BAL_CC 116u
#define MIDI_XF_CC 117u
#define MIDI_SPLIT_CC 118u
static void mode_apply(void);                   /* (seq.c) */
static int __attribute__((noinline)) midi_mode_cc(uint32_t cc, uint32_t value)
{
    uint32_t i, e;
    if (ml_tab) {
        if (ml_arm)
            return 0;
        for (i = 0; i < ML_N; i++) {
            e = ml_tab[2u * i] | (uint32_t)ml_tab[2u * i + 1u] << 8;
            if ((e >> 9) && (e & 127u) == cc)
                return 0;
        }
    }
    if (cc == MIDI_BAL_CC)                          /* 0..127 -> -100..+100, 64 = 0 */
        mix_bal = (int8_t)(value == 64u ? 0 : value < 64u ? ((int32_t)value - 64) * 100 / 64 : ((int32_t)value - 64) * 100 / 63);
    else if (cc == MIDI_XF_CC)                      /* 0..127 -> 0..100 % */
        mix_xf = (uint8_t)((value * 100u + 63u) / 127u);
    else if (cc == MIDI_SPLIT_CC)                   /* 0 OFF, 1..127 the note n (stored n + 1) */
        mix_split = (uint8_t)(value ? value + 1u : 0u);
    else {
        duell_mode_req = (uint8_t)(value < 43u ? DM_SPLIT : value < 86u ? DM_STEREO : DM_LAYER);
        mode_apply();
    }
    return 1;
}

/* DUELL (SPEC 4.2a): PROGRAM CHANGE, with the BANK SELECT (CC0 MSB, CC32 LSB, bank = MSB << 7 | LSB) the channel last
 * sent. A recall is far too heavy for the audio ISR: it is queued here and the main loop runs it (ui.c pc_poll), as the
 * MORPH's actions. The channel's part as for its notes (ROUT CH1-4: channel 1 part 1, 2 part 2, the hidden parts' channels
 * ignored; SEL: the selected part; LAYER: part 1's channel, SEL any).
 *   LAYER: bank 0, program 0..15 = MULTI M01..M16 (the MULTI page's LOAD; an empty slot nothing); any other bank / program
 *   ignored. SPLIT, STEREO: a SOUND for the channel's part only: bank 0 program n = entry n of the PATCHES list (ALL:
 *   factory sounds with their stored copies in place, then the user patches; the empty places are no entry); bank 1 program n = stored patch n (slot 0..127);
 *   other banks, programs past the list and empty slots ignored. Dropped if PCQ_N recalls are already waiting */
#define PCQ_N 8u
#define PCQ_MULTI 0xFFu                                    /* (the entry's part: LAYER's multi) */
static volatile uint32_t pcq_buf[PCQ_N];                   /* part | program << 8 | bank << 16 */
static volatile uint8_t pcq_w, pcq_r;                      /* (the ISR writes pcq_w, the main loop pcq_r) */
static void __attribute__((noinline)) midi_pc(uint32_t ch, uint32_t prog)
{
    const midi_channel_t *c = midi_channel(ch);
    uint32_t bank = (uint32_t)c->bank_msb << 7 | c->bank_lsb, pi = trk_index(midi_track(ch)), w = pcq_w;
    if (duell_mode == DM_LAYER) {
        if (bank || prog >= MU_SLOTS)
            return;
        pi = PCQ_MULTI;
    } else if (pi >= NSHOWN || bank > 1u || (bank && prog >= UP_SLOTS)) {
        return;
    }
    if ((uint8_t)(w - pcq_r) >= PCQ_N)
        return;
    pcq_buf[w % PCQ_N] = pi | prog << 8 | bank << 16;
    pcq_w = (uint8_t)(w + 1u);
}

/* ROUT CH1-4 listens to channels 1..4 only (1.2: CH5-8, CH9-12, CH13-16 to their four). A switch to a block from
 * SEL or another block (events_block, before the queue) lets go of what the other channels hold: their notes
 * released (also pedal-held ones), their pedal, bend, wheel and RPN selection reset, so no note can hang on a
 * channel that is no longer heard. Each part's bend then follows its own channel of the block. */
static void __attribute__((noinline)) midi_route_block(void)
{
    uint32_t ch, note;
    for (ch = 0; ch < 16u; ch++) {
        midi_channel_t *c = midi_channel(ch);
        if (midi_in_block(ch)) {
            midi_expression(midi_track(ch), c);
            continue;
        }
        for (note = 0; note < 128u; note++)
            if (midi_notes[ch][note])
                midi_release(ch, note);
        c->pedal = c->wheel = 0;
        c->bend = 0;
        c->rpn_msb = c->rpn_lsb = 127;
        midi_morph_fired[ch] = 0;
    }
}

/* Keep the occasional controller/panic dispatch outside the hot rendering loop. Channel voice messages only
 * (realtime and clock are handled in events_block, SysEx never reaches here). With ROUT CH1-4 channels
 * 5..16 are ignored entirely (with CH5-8 .. CH13-16 every channel outside the block): notes, bend, CCs (CC1/11/64, RPN, and the CC120/121/123 panic and reset),
 * channel aftertouch, so they stay free for other instruments. */
static void __attribute__((noinline)) midi_event(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2)
{
    if (st == 0xB0u && (d1 == MIDI_MODE_CC || (d1 >= MIDI_BAL_CC && d1 <= MIDI_SPLIT_CC)) && midi_mode_cc(d1, d2))
        return;                                    /* (DUELL step 5: the MODE, any channel) */
    if ((st == 0x90u || st == 0x80u) &&                   /* (DUELL DUCK, fx.c duck_note: a trigger note, before the routing drops or plays it) */
        duck_note(ch, d1, st == 0x90u && d2, song.g[G_ROUTE] == 1 ? 1u : duell_mode == DM_LAYER ? ch == midi_pch[0] : (uint32_t)midi_in_block(ch)))
        return;
    if (song.g[G_ROUTE] != 1 && !midi_in_block(ch))
        return;
    if (duell_mode == DM_LAYER && song.g[G_ROUTE] != 1 && ch != midi_pch[0])
        return;                                    /* (LAYER: part 2's channel is not heard; part 1's plays both) */
    if (st == 0x90u || st == 0x80u)
        midi_note_event(ch, d1, st == 0x90u ? d2 : 0);
    else if (st == 0xE0u) {
        midi_channel(ch)->bend = (int16_t)((int32_t)(d1 | (d2 << 7)) - 8192);
        midi_expression_channel(ch);
    } else if (st == 0xB0u) {
        mod_midi(midi_track(ch), st, d1, d2);
        if (duell_mode == DM_LAYER)
            mod_midi(&trk[1], st, d1, d2);         /* (LAYER: the wheel, EXPR and aftertouch are part 2's too) */
        midi_control(ch, d1, d2);
    } else if (st == 0xC0u) {
        midi_pc(ch, d1);                           /* DUELL: PROGRAM CHANGE (queued for the main loop) */
    } else if (st == 0xD0u) {
        mod_midi(midi_track(ch), st, d1, d2);
        if (duell_mode == DM_LAYER)
            mod_midi(&trk[1], st, d1, d2);
    }
}
