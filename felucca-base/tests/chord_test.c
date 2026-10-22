/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Chord keys (firmware/src/chord.c; SCL > CHORD: CHRD, VOIC) against the real keyboard, MIDI IN, arp, recording
 * and voice code: the diatonic triads and sevenths of several scales and roots as music theory has them, the
 * fixed shapes, the voicings, at most 4 notes, the names, MONO plays the root, a key's or a MIDI note's release
 * ends exactly the notes it started (CHRD / VOIC changed while held too), shared notes, live recording writes the
 * chord into one step, the ARP gets the chord as held notes, a kit (the DRUM engine, SAMPLE PERC) ignores CHRD, MIDI OUT
 * of the keys, and OFF plays exactly as before. The GLOBAL scale (core.h; owner 2026-10-12): CHROMATIC by default, OFF-SCALE
 * KEYS SNAP (nearest scale note, a tie down) / PLAY, both parts the same note, DIA3 / DIA7 stack the scale and play plain
 * MAJ / MAJ7 when chromatic, fixed shapes keep their intervals from the snapped root, arps and SEQUENCER steps follow the scale,
 * the settings, the patch AK block (an old SCALE read and discarded) and the factory sounds (no SCALE).
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int gate_note(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static uint32_t ngated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}
static void reset(void)
{
    ui_power_on();
    memset(kb_chn, 0, sizeof kb_chn); memset(mchord, 0, sizeof mchord); memset(chord_last, 0, sizeof chord_last);
    fm1_in.notes = kb_prev = 0; mo_w = mo_r = 0; usb.config = 1;
    trk[0].p[P_VOICE] = V_POLY; scale_g = 1; root_g = 0; offscale_g = OS_PLAY;   /* C major, POLY; keys as played (the root rule of the chords; SNAP has its own test) */
    events_block(CTL);
}
/* the chord track 0 plays on note r: "n1 n2 n3 [n4]" compared with want (0-terminated) */
static int chord_is(uint32_t r, const uint8_t *want)
{
    uint8_t out[CHORD_MAX];
    uint32_t n = chord_build(&trk[0], r, out), i;
    for (i = 0; i < n; i++) if (want[i] != out[i]) return 0;
    return i == CHORD_MAX || !want[i];
}
static int name_is(uint32_t r, const char *want)
{
    uint8_t out[CHORD_MAX];
    int32_t root;
    uint16_t mask;
    char b[12];
    chord_make(&trk[0], r, out, &root, &mask);
    chord_name(b, (uint32_t)root, mask);
    if (!str_eq(b, want)) printf("  name of %u: %s, want %s\n", r, b, want);
    return str_eq(b, want);
}
static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    midi_enqueue(st >> 4 | st << 8 | d1 << 16 | d2 << 24, 1);
    events_block(CTL);
}
#define K_C4 7u                                   /* keys from F3 (0): C4 7, D4 9, E4 11, G4 14 */
#define K_D4 9u
#define K_E4 11u

static int theory(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    static const char *const MAJ3[7] = {"C", "Dm", "Em", "F", "G", "Am", "Bdim"};
    static const char *const MAJ7[7] = {"Cmaj7", "Dm7", "Em7", "Fmaj7", "G7", "Am7", "Bm7b5"};
    static const uint8_t CMAJ[7] = {60, 62, 64, 65, 67, 69, 71};
    uint32_t i;
    reset();
    t->p[P_CHRD] = CH_DIA3;
    bad += check("C major DIA3 on D: D F A", chord_is(62, (const uint8_t[]){62, 65, 69, 0}));
    bad += check("C major DIA3 on B: B D F (diminished)", chord_is(71, (const uint8_t[]){71, 74, 77, 0}));
    t->p[P_CHRD] = CH_DIA7;
    bad += check("C major DIA7 on G: G B D F", chord_is(67, (const uint8_t[]){67, 71, 74, 77}));
    bad += check("C major DIA7 on C: C E G B", chord_is(60, (const uint8_t[]){60, 64, 67, 71}));
    for (i = 0, ok = 1; i < 7u; i++) {
        t->p[P_CHRD] = CH_DIA3; ok &= name_is(CMAJ[i], MAJ3[i]);
        t->p[P_CHRD] = CH_DIA7; ok &= name_is(CMAJ[i], MAJ7[i]);
    }
    bad += check("C major: C Dm Em F G Am Bdim, Cmaj7 Dm7 Em7 Fmaj7 G7 Am7 Bm7b5", ok);
    scale_g = 2; root_g = 9; t->p[P_CHRD] = CH_DIA3;   /* A minor */
    bad += check("A minor DIA3 on B: B D F", chord_is(71, (const uint8_t[]){71, 74, 77, 0}));
    bad += check("A minor DIA3 on A: A C E (Am), on C: C E G, on E: E G B (Em)",
                 chord_is(69, (const uint8_t[]){69, 72, 76, 0}) && chord_is(60, (const uint8_t[]){60, 64, 67, 0}) &&
                 chord_is(64, (const uint8_t[]){64, 67, 71, 0}) && name_is(69, "Am") && name_is(64, "Em"));
    scale_g = 7;                                             /* A harmonic minor */
    bad += check("A harmonic minor DIA3 on E: E G# B (E), on C: C E G# (Caug)",
                 chord_is(64, (const uint8_t[]){64, 68, 71, 0}) && name_is(64, "E") &&
                 chord_is(60, (const uint8_t[]){60, 64, 68, 0}) && name_is(60, "Caug"));
    t->p[P_CHRD] = CH_DIA7;
    bad += check("A harmonic minor DIA7 on E: E G# B D (E7), on G#: G# B D F (G#dim7), on A: AmM7",
                 chord_is(64, (const uint8_t[]){64, 68, 71, 74}) && name_is(64, "E7") && name_is(68, "G#dim7") &&
                 name_is(69, "AmM7"));
    scale_g = 1; root_g = 3;                           /* Eb major */
    bad += check("Eb major DIA7 on Bb: Bb D F Ab (A#7)", chord_is(58, (const uint8_t[]){58, 62, 65, 68}) && name_is(58, "A#7"));
    scale_g = 3; root_g = 2; t->p[P_CHRD] = CH_DIA3;   /* D dorian */
    bad += check("D dorian DIA3 on D: Dm, on G: G (the major IV)", name_is(62, "Dm") && name_is(67, "G") &&
                 chord_is(67, (const uint8_t[]){67, 71, 74, 0}));
    scale_g = 1; root_g = 0;
    bad += check("a root outside the scale keeps its pitch, the shape of the scale note below (C# in C major: C#-F-G#, C's 0-4-7)",
                 chord_is(61, (const uint8_t[]){61, 65, 68, 0}) && name_is(61, "C#"));
    bad += check("  Eb (D's shape: minor) and F# (F's shape: major) too",
                 chord_is(63, (const uint8_t[]){63, 66, 70, 0}) && name_is(63, "D#m") && chord_is(66, (const uint8_t[]){66, 70, 73, 0}));
    t->p[P_CHRD] = CH_DIA7;
    bad += check("  DIA7 on C#: C's seventh shape on C# (C#-F-G#-C)", chord_is(61, (const uint8_t[]){61, 65, 68, 72}));
    t->p[P_CHRD] = CH_MAJ; scale_g = 2;
    bad += check("  a fixed shape ignores the scale (MAJ on C#: C#-F-G#)", chord_is(61, (const uint8_t[]){61, 65, 68, 0}));
    t->p[P_CHRD] = CH_DIA3; scale_g = 1;
    scale_g = 0; root_g = 7;                           /* CHROMATIC: no scale to stack (the root is no matter) */
    bad += check("CHROMATIC: DIA3 plays the plain MAJ triad (A: A C# E, F#: F# A# C#), DIA7 the MAJ7 (C: C E G B)",
                 chord_is(69, (const uint8_t[]){69, 73, 76, 0}) && name_is(69, "A") && name_is(66, "F#") &&
                 (t->p[P_CHRD] = CH_DIA7, chord_is(60, (const uint8_t[]){60, 64, 67, 71})) && name_is(60, "Cmaj7"));
    t->p[P_CHRD] = CH_DIA3;
    scale_g = 5; root_g = 0;                           /* C major pentatonic: every other note */
    bad += check("pentatonic DIA3: in the scale (C E A: C6)", chord_is(60, (const uint8_t[]){60, 64, 69, 0}) && name_is(60, "C6"));
    {   /* every scale, root, mode: every note in the scale */
        uint32_t s, r, n, k, note;
        uint8_t out[CHORD_MAX], ob[CHORD_MAX];
        uint32_t m = 0;
        ok = 1;
        for (s = 1; s <= (uint32_t)SCALE_D.max; s++)
            for (r = 0; r < 12u; r++)
                for (note = 36; note < 96u; note++) {
                    scale_g = (int16_t)s; root_g = (int16_t)r;
                    t->p[P_CHRD] = (int16_t)(note & 1u ? CH_DIA3 : CH_DIA7);
                    n = chord_build(t, note, out);
                    ok &= n == (note & 1u ? 3u : 4u);
                    ok &= out[0] == note;                      /* the root is never moved */
                    if ((scale_mask(t) >> ((note + 120u - r) % 12u)) & 1u) {
                        for (k = 0; k < n; k++)
                            ok &= (scale_mask(t) >> ((out[k] + 120u - r) % 12u)) & 1u;
                    } else {                                   /* outside the scale: the shape of the scale note below */
                        uint32_t b = note;
                        while (!((scale_mask(t) >> ((b + 120u - r) % 12u)) & 1u)) b--;
                        ok &= chord_build(t, b, ob) == n;
                        for (k = 0; k < n; k++)
                            ok &= out[k] - note == ob[k] - b;
                        m++;
                    }
                }
        bad += check("DIA3 / DIA7 of every scale and root: 3 / 4 notes, the root as played; in the scale every tone in key, "
                     "outside it the shape of the scale note below", ok && m > 0);
    }
    return bad;
}

static int shapes_voicings(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    reset();
    t->p[P_CHRD] = CH_MAJ; bad += check("MAJ on C4: C E G", chord_is(60, (const uint8_t[]){60, 64, 67, 0}) && name_is(60, "C"));
    t->p[P_CHRD] = CH_MIN; bad += check("MIN: C Eb G (Cm)", chord_is(60, (const uint8_t[]){60, 63, 67, 0}) && name_is(60, "Cm"));
    t->p[P_CHRD] = CH_DOM7; bad += check("DOM7: C E G Bb (C7)", chord_is(60, (const uint8_t[]){60, 64, 67, 70}) && name_is(60, "C7"));
    t->p[P_CHRD] = CH_MAJ7; bad += check("MAJ7: C E G B (Cmaj7)", chord_is(60, (const uint8_t[]){60, 64, 67, 71}) && name_is(60, "Cmaj7"));
    t->p[P_CHRD] = CH_MIN7; bad += check("MIN7: C Eb G Bb (Cm7)", chord_is(60, (const uint8_t[]){60, 63, 67, 70}) && name_is(60, "Cm7"));
    t->p[P_CHRD] = CH_SUS4; bad += check("SUS4: C F G (Csus4)", chord_is(60, (const uint8_t[]){60, 65, 67, 0}) && name_is(60, "Csus4"));
    t->p[P_CHRD] = CH_POW; bad += check("POW: C G C (C5)", chord_is(60, (const uint8_t[]){60, 67, 72, 0}) && name_is(60, "C5"));
    t->p[P_CHRD] = CH_MIN; scale_g = 1;
    bad += check("a fixed shape ignores the scale (MIN on D# in C major: D# F# A#)", chord_is(63, (const uint8_t[]){63, 66, 70, 0}));
    t->p[P_CHRD] = CH_MAJ;
    t->p[P_VOIC] = VC_OPEN; bad += check("OPEN: C G E (1-5-3)", chord_is(60, (const uint8_t[]){60, 67, 76, 0}));
    t->p[P_VOIC] = VC_INV1; bad += check("INV1: E G C", chord_is(60, (const uint8_t[]){64, 67, 72, 0}));
    t->p[P_VOIC] = VC_INV2; bad += check("INV2: G C E", chord_is(60, (const uint8_t[]){67, 72, 76, 0}));
    t->p[P_VOIC] = VC_BASS; bad += check("+OCT: C3 C E G", chord_is(60, (const uint8_t[]){48, 60, 64, 67}));
    t->p[P_CHRD] = CH_MAJ7;
    bad += check("+OCT of a seventh: C3 C E B (the fifth dropped, 4 notes)", chord_is(60, (const uint8_t[]){48, 60, 64, 71}));
    t->p[P_VOIC] = VC_OPEN; bad += check("OPEN seventh: C G B E", chord_is(60, (const uint8_t[]){60, 67, 71, 76}));
    t->p[P_VOIC] = VC_INV1; bad += check("INV1 seventh: E G B C", chord_is(60, (const uint8_t[]){64, 67, 71, 72}));
    t->p[P_VOIC] = VC_INV2; bad += check("INV2 seventh: G B C E", chord_is(60, (const uint8_t[]){67, 71, 72, 76}));
    t->p[P_CHRD] = CH_POW; t->p[P_VOIC] = VC_INV1;
    bad += check("INV1 of POW: G C (the doubled root once)", chord_is(60, (const uint8_t[]){67, 72, 0}));
    {   /* every mode, voicing and note: 1..4 notes, ascending, each once, inside 0..127; the name fits */
        uint32_t c, v, s, note, n, k;
        uint8_t out[CHORD_MAX];
        int32_t r;
        uint16_t mask;
        char b[12];
        ok = 1;
        for (s = 0; s <= (uint32_t)SCALE_D.max; s++)
            for (c = 1; c <= (uint32_t)TP[P_CHRD].max; c++)
                for (v = 0; v <= (uint32_t)TP[P_VOIC].max; v++)
                    for (note = 0; note < 128u; note++) {
                        scale_g = (int16_t)s; t->p[P_CHRD] = (int16_t)c; t->p[P_VOIC] = (int16_t)v;
                        n = chord_make(t, note, out, &r, &mask);
                        ok &= n >= 1u && n <= CHORD_MAX && (mask & 1u);
                        for (k = 1; k < n; k++) ok &= out[k] > out[k - 1u];
                        for (k = 0; k < n; k++) ok &= out[k] <= 127u;
                        chord_name(b, (uint32_t)r, mask);
                        ok &= str_len(b) >= 1u && str_len(b) <= 7u;
                    }
        bad += check("every scale x CHRD x VOIC x note: 1..4 notes, ascending, once each, 0..127, a name", ok);
    }
    t->p[P_CHRD] = CH_OFF;
    bad += check("OFF: the note alone", chord_is(61, (const uint8_t[]){61, 0}));
    return bad;
}

static int keys(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t before;
    reset();
    t->p[P_CHRD] = CH_DIA3;
    key_down(K_D4);
    bad += check("a key plays its chord (D4: D F A), MIDI OUT the three", gate_note(t, 62) && gate_note(t, 65) && gate_note(t, 69) &&
                 ngated(t) == 3u && mo_w == 3u && last_note == 62);
    bad += check("  chord_last: Dm for the CHORD page", chord_last[0].root == 62 && chord_last[0].n == 3u);
    t->p[P_CHRD] = CH_MIN7; t->p[P_VOIC] = VC_BASS;               /* changed while held */
    key_up(K_D4);
    bad += check("its release ends exactly those (CHRD / VOIC changed meanwhile), MIDI OUT their note-offs",
                 !ngated(t) && mo_w == 6u && ((midi_out_q[5] >> 8) & 0xF0u) == 0x80u);
    t->p[P_CHRD] = CH_DIA3; t->p[P_VOIC] = VC_CLOSE;
    mo_w = mo_r = 0;
    key_down(K_C4); key_down(K_E4);                                /* C E G and E G B: E and G shared */
    before = mo_w;
    bad += check("two chords: the shared notes sound once (C E G B)", ngated(t) == 4u && gate_note(t, 71) && before == 4u);
    key_up(K_C4);
    bad += check("  C up: C ends, E G stay for the E key", !gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67) && gate_note(t, 71));
    key_up(K_E4);
    bad += check("  E up: nothing left, MIDI OUT balanced", !ngated(t) && mo_w == 8u);
    t->p[P_VOICE] = V_MONO;
    key_down(K_C4 + 1u);                                           /* C#: the root as played */
    bad += check("MONO: the root alone (C# in C major: C#)", ngated(t) == 1u && gate_note(t, 61));
    t->p[P_VOICE] = V_POLY;
    key_up(K_C4 + 1u);
    bad += check("  released (MONO switched off meanwhile)", !ngated(t));
    t->p[P_VOICE] = V_LEGATO;
    key_down(K_E4);
    bad += check("LEGATO: the root alone (E)", ngated(t) == 1u && gate_note(t, 64));
    key_up(K_E4);
    t->p[P_VOICE] = V_POLY;
    scale_g = 1; root_g = 0; offscale_g = OS_PLAY;
    key_down(K_C4 + 1u);
    bad += check("OFF-SCALE PLAY, C major DIA3: the black key C# plays a chord on C# (C# F G#)",
                 ngated(t) == 3u && gate_note(t, 61) && gate_note(t, 65) && gate_note(t, 68) && !gate_note(t, 60));
    key_up(K_C4 + 1u);
    offscale_g = OS_SNAP;
    key_down(K_C4 + 1u);
    bad += check("OFF-SCALE SNAP snaps the key to the scale first (C# -> C: C E G)",
                 ngated(t) == 3u && gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67) && !gate_note(t, 61));
    key_up(K_C4 + 1u);
    offscale_g = OS_PLAY;
    t->p[P_CHRD] = CH_OFF;
    key_down(K_C4 + 1u);
    bad += check("CHRD OFF with a scale, PLAY: the key's real note (C#)", ngated(t) == 1u && gate_note(t, 61));
    key_up(K_C4 + 1u);
    before = mo_w;
    key_down(K_C4);
    bad += check("OFF: one note, as before", ngated(t) == 1u && gate_note(t, 60) && mo_w == before + 1u);
    key_up(K_C4);
    return bad;
}

static int arp_rec(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i;
    reset();
    t->p[P_CHRD] = CH_DIA7;
    t->p[P_AMODE] = 1;
    key_down(K_C4);
    bad += check("ARP: one key, the chord as held notes (C E G B)", t->nheld == 4u && t->held[0] == 60 && t->held[1] == 64 &&
                 t->held[2] == 67 && t->held[3] == 71);
    key_up(K_C4);
    bad += check("  key up: nothing held", !t->nheld && !t->arp_phys);
    t->p[P_AMODE] = 0;
    for (i = 0; i < NSTEP; i++) t->step[i] = (step_t){{0}, 0, ST_REST, 0, 0};
    t->p[P_CHRD] = CH_DIA3;
    song.rec = 1; song.playing = 1;
    t->seq_idx = 0; t->seq_pos = 0;
    key_down(K_D4);
    bad += check("live recording: the chord into one step (D F A)", t->step[0].n == 3u && t->step[0].time == ST_NOTE &&
                 t->step[0].note[0] == 62 && t->step[0].note[1] == 65 && t->step[0].note[2] == 69);
    key_up(K_D4);
    t->p[P_CHRD] = CH_MAJ7; t->p[P_VOIC] = VC_BASS;
    t->seq_idx = 4;
    key_down(K_C4);
    bad += check("  a 4-note voicing: 4 notes in the step (the most a step holds)", t->step[4].n == 4u && t->step[4].note[0] == 48);
    key_up(K_C4);
    song.rec = 0; song.playing = 0;
    t->p[P_VOICE] = V_MONO; t->p[P_CHRD] = CH_DIA3; t->p[P_VOIC] = VC_CLOSE;
    song.rec = 1; song.playing = 1; t->seq_idx = 8;
    key_down(K_E4);
    bad += check("  MONO records its root alone", t->step[8].n == 1u && t->step[8].note[0] == 64);
    key_up(K_E4);
    song.rec = 0; song.playing = 0;
    return bad;
}

static int midi_in(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, free = 1;
    reset();
    t->p[P_CHRD] = CH_DIA3;
    midi(0x90, 62, 100);
    bad += check("MIDI IN: a note plays its chord (D F A)", gate_note(t, 62) && gate_note(t, 65) && gate_note(t, 69) && ngated(t) == 3u);
    t->p[P_CHRD] = CH_OFF;
    midi(0x80, 62, 0);
    t->p[P_CHRD] = CH_DIA3;
    midi(0x90, 61, 100);
    bad += check("MIDI IN, a note outside the scale (C#): its chord on C# (C# F G#), not snapped",
                 gate_note(t, 61) && gate_note(t, 65) && gate_note(t, 68) && !gate_note(t, 60) && ngated(t) == 3u);
    midi(0x80, 61, 0);
    t->p[P_CHRD] = CH_OFF;
    midi(0x90, 61, 100);
    bad += check("  CHRD OFF: the note itself", ngated(t) == 1u && gate_note(t, 61));
    midi(0x80, 61, 0);
    t->p[P_CHRD] = CH_DIA3;
    midi(0x90, 62, 100); midi(0x80, 62, 0);
    for (i = 0; i < MCHORD_N; i++) free &= !mchord[i].id;
    bad += check("  its note-off ends the chord (CHRD OFF meanwhile), the table empty", !ngated(t) && free);
    t->p[P_CHRD] = CH_DIA3;
    midi(0x90, 60, 100); key_down(K_E4);                           /* MIDI C E G, key E G B */
    bad += check("MIDI and a key share notes: C E G B once each", ngated(t) == 4u);
    midi(0x80, 60, 0);
    bad += check("  MIDI C off: C ends, E G stay for the key", !gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67));
    key_up(K_E4);
    bad += check("  key up: nothing left", !ngated(t));
    midi(0xB0, 64, 127); midi(0x90, 65, 100); midi(0x80, 65, 0);
    bad += check("pedal: the chord (F A C) held after its note-off", gate_note(t, 65) && gate_note(t, 69) && gate_note(t, 72));
    midi(0xB0, 64, 0);
    bad += check("  pedal up: released", !ngated(t));
    for (i = 0; i < MCHORD_N + 2u; i++) midi(0x90, 36 + i, 100);   /* more chords than the table holds */
    for (i = 0, free = 1; i < 2u; i++) free &= gate_note(t, 36 + MCHORD_N + i);   /* (the last two: their root alone) */
    for (i = 0; i < MCHORD_N + 2u; i++) midi(0x80, 36 + i, 0);
    bad += check("a full chord table: further notes play alone; all end", free && !ngated(t) && !t->nmono);
    midi(0x90, 62, 100); midi(0xB0, 120, 0);
    for (i = 0, free = 1; i < MCHORD_N; i++) free &= !mchord[i].id;
    bad += check("All Sound Off forgets the track's MIDI chords", free && !ngated(t));
    return bad;
}

static int kits(void)
{
    int bad = 0;
    uint8_t out[CHORD_MAX];
    reset();
    song.sel = 0;
    trk[0].eng_req = trk[0].engine = 4;                             /* a retired engine (SAMPLE, DUELL 0.2.9; DRUM, 0.2.14: no kits at all any more): */
    trk[0].p[P_CHRD] = CH_DIA3;                                     /* chords as any part (silent) */
    bad += check("a retired engine: chords as on any part (no kit exception since 0.2.14)", chord_build(&trk[0], 60, out) == 3u);
    return bad;
}

/* ARP MODE SEQ: the step pattern as the arp's rhythm (C4 / rest / E4 accent / G4, a held C3: C3 - E3 G3) */
static uint32_t sq_vel(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].note == t->arp_note)
            return t->v[i].vel;
    return 0;
}
static int arp_seq_mode(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, per, got[8], vel[8];
    static const uint8_t want[8] = {48, 0, 52, 55, 48, 0, 52, 55};
    reset();
    for (i = 0; i < NSTEP; i++) t->step[i] = (step_t){{0}, 0, ST_REST, 0, 0};
    t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[2] = (step_t){{64}, 1, ST_NOTE, SF_ACCENT, 100};
    t->step[3] = (step_t){{67}, 1, ST_NOTE, 0, 100};
    t->p[P_SLEN] = 4; scale_g = 0; t->p[P_AOCT] = 1; t->p[P_APROB] = 127; t->p[P_AGATE] = 64;
    t->p[P_ARATE] = 2; t->p[P_AMODE] = AM_SEQ;
    per = div_samples(2);
    t->nheld = 1; t->held[0] = 48; memset(held_key[0], 0, sizeof held_key[0]);
    t->arp_pos = 0xFFFFFFF; t->arp_idx = 0xFFFFFFFFu;
    for (i = 0; i < 8; i++) {
        arp_tick(t, per);
        got[i] = t->arp_note; vel[i] = got[i] ? sq_vel(t) : 0;
    }
    for (i = 0; i < 8; i++) { if (getenv("SQDBG")) printf("%u:%u v%u\n", i, got[i], vel[i]); if (got[i] != want[i]) bad++; }
    bad += check("ARP SEQ: held C3, steps C4 / rest / E4 / G4 -> C3, rest, E3, G3 (twice)", !bad);
    bad += check("  accent on the E step only (127 against 100)", vel[2] == 127 && vel[0] == 100 && vel[3] == 100);
    arp_tick(t, per / 4u); arp_tick(t, per / 2u);
    bad += check("  the gate ends the note (AGATE 64 = half a step)", t->arp_note == 0);
    t->p[P_AMODE] = 0; trk_all_off(t); t->nheld = 0;
    return bad;
}


/* The GLOBAL scale (core.h): one SCALE and ROOT for the whole instrument, OFF-SCALE KEYS SNAP / PLAY */
static int in_mask(int32_t note, int32_t scale, int32_t root) { return (SCALE_MASK[scale] >> ((note - root + 120) % 12)) & 1; }
static int chord_of(track_t *t, uint32_t r, const uint8_t *want)
{
    uint8_t out[CHORD_MAX];
    uint32_t n = chord_build(t, r, out), i;
    for (i = 0; i < n; i++) if (want[i] != out[i]) return 0;
    return i == CHORD_MAX || !want[i];
}
static int global_scale(void)
{
    int bad = 0, ok;
    uint32_t i, k;
    reset();
    /* a new unit / an old blob: CHROMATIC, C, SNAP */
    memset(&favorites, 0, sizeof favorites);
    scale_last = 0xFFFFFFFFu; scale_g = 9; root_g = 4; offscale_g = OS_PLAY;
    scale_sync();
    bad += check("first boot (zero settings): CHROMATIC, C, SNAP", scale_g == 0 && root_g == 0 && offscale_g == OS_SNAP);
    ui_scale_set = 0x0Au; ui_root_set = 5u;                 /* an old SCALE LINK blob (link OFF, a scale, a root): read as the defaults */
    scale_last = 0xFFFFFFFFu;
    scale_sync();
    bad += check("  an old SCALE LINK blob reads CHROMATIC, C, SNAP too", scale_g == 0 && root_g == 0 && offscale_g == OS_SNAP);
    /* settings round trip */
    scale_g = 7; root_g = 5; offscale_g = OS_PLAY;
    scale_sync();
    ok = (ui_scale_set & 0x80u) && (ui_scale_set & 15u) == 7u && (ui_scale_set & 0x40u) && ui_root_set == 5u;
    {   persist_t p;
        settings_export(&p);
        scale_g = 2; root_g = 9; offscale_g = OS_SNAP;
        memset(&favorites, 0, sizeof favorites);
        scale_last = 0xFFFFFFFFu;
        scale_sync();
        ok &= scale_g == 0 && root_g == 0 && offscale_g == OS_SNAP;
        settings_import(&p, sizeof p);
        scale_sync();
        ok &= scale_g == 7 && root_g == 5 && offscale_g == OS_PLAY; }
    bad += check("settings: SCALE, ROOT and OFF-SCALE round trip", ok);
    bad += check("MENU > CONTROL has no SCALE LINK row", menu_n(MI_HOMEPG) == (uint32_t)HP_COUNT && MI_TAB[MI_CLICK] == MTAB_AUDIO);
    reset();
    scale_g = 0; root_g = 0; offscale_g = OS_SNAP;
    /* SNAP: the nearest scale note, a tie goes down; PLAY passes; CHROMATIC has nothing to snap */
    bad += check("CHROMATIC + SNAP: every key plays as it is", note_snap(&trk[0], 61) == 61 && note_snap(&trk[0], 66) == 66);
    scale_g = 1;                                            /* C major */
    ok = note_snap(&trk[0], 61) == 60 && note_snap(&trk[0], 63) == 62 && note_snap(&trk[0], 66) == 65 && note_snap(&trk[0], 68) == 67 &&
         note_snap(&trk[0], 70) == 69 && note_snap(&trk[0], 64) == 64;
    bad += check("SNAP, C major: a black key (a tie between two white ones) goes DOWN (C# C, D# D, F# F, G# G, A# A), a scale note stays", ok);
    scale_g = 5;                                            /* C major pentatonic C D E G A */
    ok = note_snap(&trk[0], 65) == 64 && note_snap(&trk[0], 71) == 72 && note_snap(&trk[0], 66) == 67 && note_snap(&trk[0], 70) == 69;
    bad += check("SNAP, pentatonic: the NEAREST note (F -> E, B -> C up, F# -> G, A# -> A)", ok);
    root_g = 2; scale_g = 1;                                // D major: F# C# in, F C out
    bad += check("SNAP follows ROOT (D major: F -> E, C -> B)", note_snap(&trk[0], 65) == 64 && note_snap(&trk[0], 60) == 59);
    offscale_g = OS_PLAY;
    bad += check("PLAY: out-of-scale notes pass unchanged", note_snap(&trk[0], 65) == 65 && note_snap(&trk[0], 60) == 60);
    /* a key and a MIDI note take the same path; both parts sound the same note */
    root_g = 0; scale_g = 1; offscale_g = OS_SNAP;
    trk[0].p[P_CHRD] = trk[1].p[P_CHRD] = CH_OFF; trk[0].p[P_TRANS] = trk[1].p[P_TRANS] = 0;
    song.octave = 0;
    duell_mode_force(DM_LAYER);
    key_down(K_C4 + 1u);                                    /* C# */
    bad += check("SNAP, LAYER: the key C# sounds C on BOTH parts (and goes out as C on MIDI OUT)",
                 gate_note(&trk[0], 60) && gate_note(&trk[1], 60) && !gate_note(&trk[0], 61) && !gate_note(&trk[1], 61) &&
                 ((midi_out_q[mo_w - 1u] >> 16) & 0xFFu) == 60u);
    key_up(K_C4 + 1u);
    bad += check("  and its release ends them", !ngated(&trk[0]) && !ngated(&trk[1]));
    midi(0x90, 66, 100);                                    /* F# on part 1's channel: LAYER plays both */
    bad += check("SNAP: a MIDI note follows the same path (F# -> F on both parts)",
                 gate_note(&trk[0], 65) && gate_note(&trk[1], 65) && !gate_note(&trk[0], 66));
    midi(0x80, 66, 0);
    bad += check("  its note-off ends it", !ngated(&trk[0]) && !ngated(&trk[1]));
    offscale_g = OS_PLAY;
    key_down(K_C4 + 1u);
    bad += check("PLAY: the key plays C# on both parts", gate_note(&trk[0], 61) && gate_note(&trk[1], 61));
    key_up(K_C4 + 1u);
    /* the chords: DIA stacks the scale; chromatic plays plain MAJ / MAJ7; fixed shapes keep their intervals from the snapped root */
    offscale_g = OS_SNAP;
    trk[0].p[P_CHRD] = trk[1].p[P_CHRD] = CH_DIA3;
    key_down(K_D4);
    bad += check("DIA3 in C major stacks scale degrees (D4: D F A) on both parts",
                 gate_note(&trk[0], 62) && gate_note(&trk[0], 65) && gate_note(&trk[0], 69) && gate_note(&trk[1], 62) && gate_note(&trk[1], 65) &&
                 gate_note(&trk[1], 69) && ngated(&trk[0]) == 3u && ngated(&trk[1]) == 3u);
    key_up(K_D4);
    trk[0].p[P_CHRD] = trk[1].p[P_CHRD] = CH_DIA7;
    key_down(K_D4 - 2u);                                    /* C4 */
    bad += check("DIA7 in C major: C E G B", gate_note(&trk[0], 60) && gate_note(&trk[0], 64) && gate_note(&trk[0], 67) && gate_note(&trk[0], 71) &&
                 ngated(&trk[0]) == 4u);
    key_up(K_D4 - 2u);
    scale_g = 0;
    trk[0].p[P_CHRD] = CH_DIA3; trk[1].p[P_CHRD] = CH_DIA7;
    key_down(K_D4);
    bad += check("CHROMATIC: DIA3 plays the MAJ triad (D F# A), DIA7 the MAJ7 (D F# A C#)",
                 gate_note(&trk[0], 62) && gate_note(&trk[0], 66) && gate_note(&trk[0], 69) && ngated(&trk[0]) == 3u &&
                 gate_note(&trk[1], 62) && gate_note(&trk[1], 66) && gate_note(&trk[1], 69) && gate_note(&trk[1], 73) && ngated(&trk[1]) == 4u);
    key_up(K_D4);
    scale_g = 1;
    trk[0].p[P_CHRD] = trk[1].p[P_CHRD] = CH_MIN;
    key_down(K_C4 + 1u);                                    /* C# snaps to C, MIN from C: C Eb G (Eb is NOT snapped again) */
    bad += check("a FIXED shape keeps its intervals from the snapped root (MIN on C# -> C: C Eb G, Eb not re-snapped)",
                 gate_note(&trk[0], 60) && gate_note(&trk[0], 63) && gate_note(&trk[0], 67) && ngated(&trk[0]) == 3u && gate_note(&trk[1], 63));
    key_up(K_C4 + 1u);
    ok = 1;
    for (i = 36; i < 96u; i++) {                            /* DIA3 / DIA7 with SNAP: the root is in the scale, every tone is in the scale */
        uint8_t o[CHORD_MAX];
        uint32_t n, j;
        trk[0].p[P_CHRD] = (int16_t)(i & 1u ? CH_DIA3 : CH_DIA7);
        for (k = 1; k <= (uint32_t)SCALE_D.max; k++) {
            scale_g = (int16_t)k; root_g = (int16_t)(i % 12u);
            n = chord_build(&trk[0], i, o);
            for (j = 0; j < n; j++) ok &= in_mask(o[j], (int32_t)k, root_g);
        }
    }
    bad += check("SNAP: every DIA3 / DIA7 tone of every scale and root is in the scale", ok);
    /* arps use the held (snapped) notes; the SEQUENCER steps follow the global scale (degrees; semitones when chromatic) */
    scale_g = 1; root_g = 0; offscale_g = OS_SNAP;
    trk[0].p[P_CHRD] = CH_OFF; trk[0].p[P_AMODE] = AM_UP;
    key_down(K_C4 + 1u);
    bad += check("ARP: the held note is the snapped one (C# -> C)", trk[0].nheld == 1u && trk[0].held[0] == 60);
    key_up(K_C4 + 1u);
    trk[0].p[P_AMODE] = 0; trk_all_off(&trk[0]); trk[0].nheld = 0;
    for (i = 0; i < NSTEP; i++) { trk[0].step[i] = trk[1].step[i] = (step_t){{0}, 0, ST_REST, 0, 0}; }
    for (i = 0; i < 12u; i++) trk[0].step[i] = trk[1].step[i] = (step_t){{(uint8_t)(60 + i)}, 1, ST_NOTE, 0, 100};
    trk[0].p[P_SLEN] = trk[1].p[P_SLEN] = 12; trk[0].p[P_AOCT] = trk[1].p[P_AOCT] = 1;
    for (k = 0, ok = 1; k < 2u; k++)
        for (i = 0; i < 12u; i++) {
            uint32_t list[1] = {48}, vel, m;
            uint8_t o[4];
            trk[k].arp_idx = i;
            m = arp_seq_notes(&trk[k], list, 1, o, &vel);
            ok &= m == 1u && in_mask(o[0], 1, 0);
        }
    bad += check("SEQUENCER steps of both parts land in the global scale", ok);
    scale_g = 0;
    { uint32_t list[1] = {48}, vel; uint8_t o[4];
      trk[1].arp_idx = 1; (void)arp_seq_notes(&trk[1], list, 1, o, &vel);
      bad += check("  CHROMATIC: they move in semitones (the C# step gives 49)", o[0] == 49); }
    /* a note already sounding keeps its pitch when the scale changes; every note ends */
    scale_g = 1; offscale_g = OS_SNAP;
    trk[0].p[P_CHRD] = CH_OFF;
    key_down(K_C4 + 1u);
    scale_g = 0;
    bad += check("the scale changed while a key is held: it keeps its note (C), the release ends it", gate_note(&trk[0], 60));
    key_up(K_C4 + 1u);
    bad += check("  nothing hangs", !ngated(&trk[0]) && !ngated(&trk[1]));
    /* loading a sound never touches the global scale; no factory sound carries one */
    {   uint32_t e, nl = 0, nk = 0;
        ok = 1;
        scale_g = 7; root_g = 5; offscale_g = OS_PLAY;
        for (k = 0; k < 2u; k++)
            for (e = 0; e < NENGINES; e++) {
                uint32_t n = ds_count(e);
                if (!n) continue;
                init_engine_of(&trk[k], e);
                for (i = 0; i < n && i < 30u; i++) {
                    uint32_t j;
                    const ds_sound_t *ds = ds_get(e, i);
                    nk += (ds->flags & DSF_KEYS) != 0;
                    nl++;
                    apply_preset_to(&trk[k], i);
                    ok &= scale_g == 7 && root_g == 5 && offscale_g == OS_PLAY;
                }
            }
        bad += check("loading sounds on either part leaves the global SCALE, ROOT and OFF-SCALE alone; no factory pad sets a SCALE", ok && nl > 20u && nk > 0u); }
    scale_g = 0; root_g = 0; offscale_g = OS_SNAP;
    return bad;
}

/* 0.2.9: a stored step follows the global scale at playback (seq_step), the chord keys under the arp follow it too (arp_rechord) */
static int snap_follow(void)
{
    int bad = 0;
    track_t *t = &trk[0], *d = &trk[1];
    uint32_t i, list[16], cnt, n;
    reset();
    for (i = 0; i < NSTEP; i++) { t->step[i] = d->step[i] = (step_t){{0}, 0, ST_REST, 0, 0}; }
    t->step[0] = d->step[0] = (step_t){{61}, 1, ST_NOTE, 0, 100};
    t->p[P_SLEN] = d->p[P_SLEN] = 4;
    scale_g = 1; root_g = 0; offscale_g = OS_SNAP;
    trk_all_off(t); seq_step(t, &t->step[0], 0, div_samples(2), SEQ_ROLLED);
    bad += check("step 61, C major SNAP: sounds C (60), the stored note stays 61", t->seq_n == 1u && t->seq_notes[0] == 60 && gate_note(t, 60) && t->step[0].note[0] == 61);
    scale_g = 1; root_g = 2;
    trk_all_off(t); seq_step(t, &t->step[0], 0, div_samples(2), SEQ_ROLLED);
    bad += check("  ROOT D (D major has C#): the same step now sounds 61", t->seq_notes[0] == 61 && gate_note(t, 61));
    scale_g = 2; root_g = 0;
    trk_all_off(t); seq_step(t, &t->step[0], 0, div_samples(2), SEQ_ROLLED);
    bad += check("  C minor: 60 again (a tie goes down)", t->seq_notes[0] == 60);
    scale_g = 0;
    trk_all_off(t); seq_step(t, &t->step[0], 0, div_samples(2), SEQ_ROLLED);
    bad += check("  CHROMATIC: 61 as stored", t->seq_notes[0] == 61);
    scale_g = 1; root_g = 0; offscale_g = OS_PLAY;
    trk_all_off(t); seq_step(t, &t->step[0], 0, div_samples(2), SEQ_ROLLED);
    bad += check("  OFF-SCALE PLAY: 61 passes unchanged", t->seq_notes[0] == 61);
    offscale_g = OS_SNAP;
    /* chord keys under the arp: C major -> C minor with DIA3 on D4 gives D F Ab */
    reset();
    scale_g = 1; root_g = 0; offscale_g = OS_SNAP; scale_last = scale_pack();
    t->p[P_CHRD] = CH_DIA3; t->p[P_AMODE] = AM_UP; t->p[P_AOCT] = 1;
    key_down(K_D4);
    t->arp_idx = 5; t->arp_pos = 77; t->arp_ph = 1;
    n = arp_list(t, list, &cnt);
    bad += check("arp, C major DIA3 on D4: D F A", n == 3u && list[0] == 62 && list[1] == 65 && list[2] == 69);
    scale_g = 2; scale_sync(); keyboard_block();
    n = arp_list(t, list, &cnt);
    bad += check("  scale to C minor while held: the arp list is D F Ab", n == 3u && list[0] == 62 && list[1] == 65 && list[2] == 68);
    bad += check("  the phrase goes on (arp_idx, arp_pos, arp_ph kept), nothing retriggered, held[] as pressed",
                 t->arp_idx == 5u && t->arp_pos == 77u && t->arp_ph == 1u && t->nheld == 3u && t->held[2] == 69 && ngated(t) == 0u);
    key_up(K_D4);
    bad += check("  the key up ends it cleanly", !t->nheld && !t->arp_phys && !ngated(t));
    t->p[P_CHRD] = CH_OFF;
    scale_g = 1; scale_sync(); keyboard_block();
    key_down(K_C4 + 1u);                                            /* a plain key: C# snaps to C */
    scale_g = 1; root_g = 2; scale_sync(); keyboard_block();
    n = arp_list(t, list, &cnt);
    bad += check("  a plain held key still follows (C# : C in C major, C# in D major), held[] unchanged", n == 1u && list[0] == 61 && t->held[0] == 60);
    key_up(K_C4 + 1u);
    t->p[P_AMODE] = 0; trk_all_off(t); t->nheld = 0;
    scale_g = 0; root_g = 0; scale_sync(); keyboard_block();
    return bad;
}

int main(void)
{
    int bad = global_scale() + theory() + shapes_voicings() + keys() + arp_rec() + arp_seq_mode() + midi_in() + kits() + snap_follow();
    printf("%s\n", bad ? "CHORD TEST FAILED" : "chord keys test passed");
    return bad != 0;
}
