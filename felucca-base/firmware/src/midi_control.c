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
/* Per channel and key 4 bits (memory trim 1; was a byte: 2,048 B, now 1,024 B; two keys a byte, the even key low): bits 0..2 track + 1 (0: not held;
 * NPART is 2), bit 3 (MIDI_PEDAL_NOTE) key released, held by its channel's pedal. Read and written by the MIDI event path only (events_block, the audio
 * interrupt), so two keys sharing a byte never meet two writers. */
static uint8_t midi_sel_on[16][64];
_Static_assert(NPART < 8, "midi_control.c: a key's track + 1 is 3 bits");
static uint32_t midi_note_get(uint32_t ch, uint32_t note) { return (midi_sel_on[ch][note >> 1] >> ((note & 1u) << 2)) & 15u; }
static void midi_note_put(uint32_t ch, uint32_t note, uint32_t v)
{
    uint32_t sh = (note & 1u) << 2;
    midi_sel_on[ch][note >> 1] = (uint8_t)((midi_sel_on[ch][note >> 1] & ~(15u << sh)) | (v & 15u) << sh);
}
#define MIDI_OWNER(v) ((v) & 7u)                 /* a key's track + 1 */
static uint16_t midi_owners[NTRK];           /* avoids rescanning all 2048 entries for CC123 */
#define MIDI_PEDAL_NOTE 8u

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
        if (MIDI_OWNER(midi_note_get(ch, note)) == own && !mchord_of(ch, note, id))
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
    uint32_t id = MIDI_OWNER(midi_note_get(ch, note));
    midi_note_put(ch, note, 0);
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
    uint32_t id = MIDI_OWNER(midi_note_get(ch, note));
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
        midi_note_put(ch, note, trk_index(t) + 1u);
        midi_owners[trk_index(t)]++;
        c->owned[trk_index(t)]++;
    } else if (id) {
        if (c->pedal)
            midi_note_put(ch, note, midi_note_get(ch, note) | MIDI_PEDAL_NOTE);
        else
            midi_release(ch, note);
    }
}

static void midi_pedal_up(uint32_t ch)
{
    uint32_t note;
    midi_channel(ch)->pedal = 0;
    for (note = 0; note < 128u; note++)
        if (midi_note_get(ch, note) & MIDI_PEDAL_NOTE)
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
            if (MIDI_OWNER(midi_note_get(ch, note)) == track + 1u)
                midi_note_put(ch, note, 0);
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

/* THE MIDI MAP (midi_map.h, docs/design/MIDI_MAP_DESIGN_2.md): one table of 128 lines, one per CC number, in place of the lists
 * this file had (the standard map #103, the morph's 9 CCs, the MODE's 4, the 16 learn slots). A CC sets the parameter its line names
 * as a knob would, 0..127 over the parameter's range (64: the middle of a bipolar one), and AUTOMATION records a part value as a knob
 * move (motion_capture: the selected track, armed and playing). The line's part rule says whose: MAP_R_CHANNEL the part the CC's channel
 * plays (ROUT as notes), PART1 / PART2 that part, GLOBAL any channel (also one ROUT does not play, before the routing: the MODE's
 * and the effects block); a LEARNED line is pinned to its track (midi_map.h). A number that is not in the table does nothing; the 23
 * locked standard numbers are handled by midi_control below as they always were. CC74 / CC71 (CUTOFF_SMART, RES_SMART) are the engine's own
 * cutoff / resonance (MIDI_CC_ENG); an engine without one ignores them. On the engines with no filter of their own (engine_t.ufilt: every engine since cloud job J) they
 * are the multimode filter's CUT and RES (MCC_UF; DUELL 0.2.9: 0..127 over the whole CUT dial, 30 Hz .. the open end). CC121 resets none
 * of them (RP-015: they are the sound's). A part value on an extension slot (128 + slot) is written to XV(t, slot), End A only; AUTOMATION
 * does not record it until the motion escape (id 127) is built (EXTENSION_BLOCK_PLAN 3.3). The table is in the pool (midi_map, 256 B);
 * it is the shipped list (midi_map_defaults.h, generated by tools/gen_midi_map.py from docs/MIDI_MAP_DEFAULTS.csv) until the settings
 * record gives another (settings_persist.c), and the main loop edits it with the interrupts off a line at a time (midi_learn.c) */
#include "midi_map.h"
#include "midi_map_defaults.h"
static uint16_t midi_map[MAP_N] __attribute__((section(".pool")));
static uint8_t midi_map_ready;                  /* 0: not yet loaded (power-on RAM): the shipped list is copied in at the first use */
static void map_defaults(void)
{
    memcpy(midi_map, MIDI_MAP_DEFAULTS, sizeof midi_map);
    midi_map_ready = 1;
}
static uint32_t map_line(uint32_t cc)
{
    if (!midi_map_ready)
        map_defaults();
    return midi_map[cc & 127u];
}
#define MCC_UF 0xFFu
/* per engine (ENGINES[] order): CC74's E index + 1 in the high nibble, CC71's in the low one, 0 = none; MCC_UF (0xFF): the engine's
 * multimode filter, CC74 its CUT and CC71 its RES (DUELL 0.2.2) */
static const uint8_t MIDI_CC_ENG[NENGINES] = {
    MCC_UF, 0x00, MCC_UF, MCC_UF, 0x00, MCC_UF, MCC_UF, MCC_UF,   /* ANALOG UF (its CUT RES were E5 E6 until cloud job J), - (1, DIGITAL: retired), PHASE UF, LOFI UF, - (SAMPLE: retired), VOICE UF,
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
/* the extension slots' ranges (ext.h), the same scaling; End A only (XV is the part's current value, never End B) */
static const int8_t MAP_XRANGE[NX][2] = {
    {0, 1}, {0, 1}, {-63, 63}, {-64, 63}, {-64, 63}, {-64, 63}, {0, 127}, {0, 2}, {0, 3}, {0, 127}, {0, 1}, {0, 127}, {0, 127}, {-64, 63}, {0, 1}, {0, 0},
};
static void midi_xv_set(track_t *t, uint32_t slot, uint32_t value)
{
    int32_t lo = MAP_XRANGE[slot][0], hi = MAP_XRANGE[slot][1], v = lo + ((int32_t)value * (hi - lo) + 63) / 127;
    if (hi > lo)                                    /* (no motion_capture: the escape is not built) */
        XV(t, slot) = (int16_t)v;
}
/* a global value (the effects block) as a knob would, 0..127 over its range */
static void midi_glob_set(uint32_t g, uint32_t value)
{
    const param_desc_t *d = &GP[g];
    int32_t v = d->min + ((int32_t)value * (d->max - d->min) + 63) / 127;
    if (d->max > d->min)
        song.g[g] = (int16_t)v;
}

/* MIDI LEARN's state (midi_learn.c): the UI waits for a CC (ml_arm); the CC that came meanwhile (ml_heard). Not globals here: the ISR's
 * data keeps its layout. A build without the UI has no ml_io */
static volatile uint8_t *const ml_io;       /* ui.ml.io (midi_learn.c) */
#define ml_arm (ml_io[0])                   /* the UI waits for a CC (MIDI LEARN with a parameter picked, or the editor's MAP_ARM) */
#define ml_heard (ml_io[1])                 /* .. the CC that came meanwhile, + 1 (0 none) */
#define ml_free_cc(cc) (!MAP_LOCKED_CC(cc))

/* DUELL 4b, SPEC 4.4 / 3.4a: the MORPH's actions, on each part's channel (the part the channel plays, as its notes: ROUT CH1-4 channel 1
 * part 1, channel 2 part 2, the hidden parts' channels ignored; SEL the selected part):
 *   MORPH the fader 0..127; AMOUNT 0..127 -> 0..100 %; TARGET 0-42 PART 1, 43-85 PART 2, 86-127 BOTH (global; 0.2.9:
 *   ALGORITHM's selector, ui_input.c algo_step: BOTH keeps the selected part as the edit target);
 *   ROLL (one CC, centred like the knob): 0..42 rolls End A, 43..84 nothing, 85..127 rolls End B; a roll fires when the value ENTERS its zone and again
 *   only after the value has left it. ROLL A, ROLL B (typed lines only): a value >= 64 fires once; the channel's CC re-arms below 64. UNDO: 1..63 undoes End A once,
 *   64..127 End B once (the knob's left / right), 0 re-arms. SMART, ARP, PITCH: 0-63 off, 64-127 on (global).
 * TARGET a part: a CC acts on its channel's part only; BOTH: a CC on either channel acts on both parts (morph.c
 * morph_mask). Queued for the main loop (morph.c morph_push: a roll is no work for the audio ISR).
 * The MOD WHEEL (CC1) moves the fader too, always (no switch), as the MORPH line does (same queue, same
 * TARGET rules), while it stays the matrix's MODW source (mod.c mod_midi); in LAYER it moves both parts' faders.
 * The triggers' memory (midi_morph_fired) is a bit of the TARGET, not of the CC number: ROLL A 0, ROLL B 1, UNDO 2, ROLL's A zone 3, B zone 4 */
#define MQ_PART_ALL 0x7Fu                                  /* (morph_push's part: every shown part, ui_input.c morph_poll) */
static uint8_t midi_morph_fired[16];                      /* bit k: ROLL A / ROLL B / UNDO (k = target - MAP_T_ROLL_A) at or above 64 on the channel (fired) */
static void __attribute__((noinline)) midi_morph(uint32_t ch, uint32_t pi, uint32_t code, uint32_t value)
{
    uint32_t b;
    if (pi >= NSHOWN)
        return;
    if (code == MAP_T_MORPH) {
        morph_push(MQ_FADER, pi, value);
    } else if (code == MAP_T_AMOUNT) {
        morph_push(MQ_AMOUNT, pi, (value * 100u + 63u) / 127u);
    } else if (code == MAP_T_TARGET) {
        morph_push(MQ_TARGET, pi, value < 43u ? 0u : value < 86u ? 1u : 2u);
    } else if (code == MAP_T_ROLL) {
        uint32_t za = 1u << (MAP_T_ROLL - MAP_T_ROLL_A), zb = za << 1;
        if (value >= 43u)
            midi_morph_fired[ch] &= (uint8_t)~za;                 /* left the A zone: armed again */
        else if (!(midi_morph_fired[ch] & za)) {
            midi_morph_fired[ch] |= (uint8_t)za;
            morph_push(MQ_ROLL, pi, 0u);
        }
        if (value < 85u)
            midi_morph_fired[ch] &= (uint8_t)~zb;
        else if (!(midi_morph_fired[ch] & zb)) {
            midi_morph_fired[ch] |= (uint8_t)zb;
            morph_push(MQ_ROLL, pi, 1u);
        }
    } else if (code >= MAP_T_SMART) {
        morph_push(code == MAP_T_SMART ? MQ_SMART : code == MAP_T_ARP ? MQ_ARP : MQ_PITCH, pi, value >= 64u);
    } else {
        b = 1u << (code - MAP_T_ROLL_A);
        if (code == MAP_T_UNDO) {
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
            morph_push(MQ_ROLL, pi, code == MAP_T_ROLL_B);
        }
    }
}

/* DUELL step 5 (SPEC 5b.1): the MODE is global (not a part's): 0-42 SPLIT, 43-85 STEREO, 86-127 LAYER, on any channel (also one ROUT does
 * not play: the MODE is no part's), a value that keeps the mode does nothing. BALANCE 0..127 -> -100..+100, 64 = 0; XFADE 0..127 -> 0..100 %;
 * SPLIT 0 = OFF, 1..127 = the note n. Queued for nothing: mode_apply runs here, in the ISR */
static void mode_apply(void);                   /* (seq.c) */
/* 1: code is a global target and is set now */
static int __attribute__((noinline)) midi_glob_act(uint32_t code, uint32_t value)
{
    if (code >= MAP_T_G0 && code < MAP_T_G0 + G_COUNT)
        midi_glob_set(code - MAP_T_G0, value);
    else if (code == MAP_T_BALANCE)                 /* 0..127 -> -100..+100, 64 = 0 */
        mix_bal = (int8_t)(value == 64u ? 0 : value < 64u ? ((int32_t)value - 64) * 100 / 64 : ((int32_t)value - 64) * 100 / 63);
    else if (code == MAP_T_XFADE)                   /* 0..127 -> 0..100 % */
        mix_xf = (uint8_t)((value * 100u + 63u) / 127u);
    else if (code == MAP_T_SPLIT)                   /* 0 OFF, 1..127 the note n (stored n + 1) */
        mix_split = (uint8_t)(value ? value + 1u : 0u);
    else if (code == MAP_T_MODE) {
        duell_mode_req = (uint8_t)(value < 43u ? DM_SPLIT : value < 86u ? DM_STEREO : DM_LAYER);
        mode_apply();
    } else
        return 0;
    return 1;
}
/* the global lines, before the MIDI IN routing; 1: the CC is done. A learn waiting for a CC takes it instead (post routing) */
static int __attribute__((noinline)) midi_map_global(uint32_t cc, uint32_t value)
{
    uint32_t l = map_line(cc);
    if (MAP_RULE(l) != MAP_R_GLOBAL || MAP_WHO(l) == MAP_W_LOCKED || MAP_WHO(l) == MAP_W_LEARNED || (ml_io && ml_arm))
        return 0;
    return midi_glob_act(MAP_TARGET(l), value);
}

/* the line of a CC that is not a locked number, after the routing: whose parameter, then the set */
static void __attribute__((noinline)) midi_map_cc(uint32_t ch, track_t *ct, uint32_t cc, uint32_t value)
{
    uint32_t l = map_line(cc), code = MAP_TARGET(l), rule = MAP_RULE(l), id, e;
    track_t *t;
    if (MAP_IS_LOCKED(l))                           /* (96 97 122 124..127: not acted on) */
        return;
    if (ml_io && ml_arm) {                          /* a learn waits for a CC: this one is taken by it, whatever it was */
        if (!ml_heard)
            ml_heard = (uint8_t)(cc + 1u);
        return;
    }
    if (MAP_IS_EMPTY(l))
        return;
    if (MAP_WHO(l) == MAP_W_LEARNED)                /* (a learned CC is one track's, whatever channel it comes on) */
        t = &trk[rule];
    else if (rule == MAP_R_PART1 || rule == MAP_R_PART2)
        t = &trk[rule - MAP_R_PART1];
    else
        t = ct;                                      /* (CHANNEL: ct, the part the channel plays) */
    if (code < P_COUNT) {
        midi_cc_set(t, code, value);
    } else if (code >= XW_BASE && code < XW_BASE + NX) {
        midi_xv_set(t, code - XW_BASE, value);
    } else if (code == MAP_T_CUTOFF_SMART || code == MAP_T_RES_SMART) {
        e = MIDI_CC_ENG[eng_idx(t->eng_req)] >> (code == MAP_T_CUTOFF_SMART ? 4 : 0) & 15u;
        if (!e)
            return;
        id = e == 15u ? (code == MAP_T_CUTOFF_SMART ? P_FREEA : P_FREEB) : P_E0 + e - 1u;   /* (15: the multimode filter's CUT, RES) */
        midi_cc_set(t, id, value);
    } else if (code >= MAP_T_MORPH && code <= MAP_T_PITCH) {
        midi_morph(ch, trk_index(t), code, value);
    } else {
        (void)midi_glob_act(code, value);           /* (a global target on a part rule: typed by hand; the editor refuses it) */
    }
}

static void midi_control(uint32_t ch, uint32_t cc, uint32_t value)
{
    midi_channel_t *c = midi_channel(ch);
    uint32_t i, mask;
    switch (cc) {
    case 1:
        c->wheel = (uint8_t)value;
        if (trk_index(midi_track(ch)) < NSHOWN)       /* DUELL: the wheel is the MORPH fader too (as the MORPH line) */
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
            if (midi_note_get(ch, i))
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
        midi_map_cc(ch, midi_track(ch), cc, value);
        break;
    }
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
            if (midi_note_get(ch, note))
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
    if (st == 0xB0u && midi_map_global(d1, d2))
        return;                                    /* (DUELL step 5: the MODE, the global lines, any channel) */
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
