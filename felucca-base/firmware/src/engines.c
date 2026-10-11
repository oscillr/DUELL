/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * factory patterns (SAVE > PATTERNS) and the parts' sounds at power-on. */
#include "dsp.c"

/* The parts' engine memory (1.5): a part plays one engine at a time, so the engines with large per-part state
 * (engine_t.mem: PHYS's slots, GRAIN's grains and index, DRUM's lanes, FM6's notes, SLICE's reverse windows)
 * keep it in one region per part instead of an array each. The region holds the state of the engine that
 * last claimed it (eng_mem_owner); a switch to another such engine zeroes its engine_t.mem bytes, which is
 * the state each had at power-on, so a switch sounds as it did with the arrays (tests/union_test.c). State
 * that runs on across notes and switches stays outside (WHEEL's wheels, GRAIN's random numbers).
 * Sized by the largest, PHYS (3 x SYMP's slot); an engine that needs more than ENG_MEM_PART grows it for
 * every part (x NPART of the pool). Zero-initialised pool section: main.c fm1_cstart clears it. */
#define ENG_MEM_PART 12888u
static union {
    uint64_t align;
    uint8_t b[ENG_MEM_PART];
} eng_mem[NLIVE] __attribute__((section(".pool")));   /* (DUELL: the shown parts only; tracks 3..4 never render) */
static uint8_t eng_mem_owner[NPART];       /* engine index + 1 whose state is in the region, 0 = none (zeros) */
#define ENG_MEM(p) ((uint32_t)(p) < NLIVE ? (void *)eng_mem[(uint32_t)(p)].b : (void *)0)   /* part p's region, 0 for a hidden part */
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= ENG_MEM_PART, name " state over ENG_MEM_PART")

#include "eng_analog.c"
#include "eng_phase.c"
#include "eng_lofi.c"
#include "eng_sample.c"
#include "eng_formant.c"
#include "eng_trio.c"
#include "eng_wheel.c"
#include "eng_grain.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_drum.c"           /* DRUM: the 8-lane kit (drum_voice.c) */
#include "eng_noise.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0) */
#include "fm4_convert.c"        /* DIGITAL's tables, and its sounds -> FM6 */
#if FELUCCA_FM4
#include "eng_digital.c"        /* DIGITAL: four-operator FM (retired; FELUCCA_FM4=1 builds it) */
#endif
#if FELUCCA_SLICE
#include "eng_slice.c"
#endif

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG,                 /* 0 */
#if FELUCCA_FM4
    &ENG_DIGITAL,                /* 1 (ENGI_DIGITAL) */
#else
    &ENG_FM4_GONE,               /* 1: reserved (DIGITAL, retired: its sounds convert to FM6, fm4_convert.c) */
#endif
    &ENG_PHASE,                  /* 2 */
    &ENG_LOFI,                   /* 3 */
    &ENG_SAMPLE,                 /* 4 */
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
    &ENG_TRIO,                   /* 6 */
    &ENG_WHEEL,                  /* 7 */
    &ENG_GRAIN,                  /* 8 */
    &ENG_PHYS,                   /* 9 (ENGI_PHYS) */
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
    &ENG_NOISE,                  /* 11 */
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
#if FELUCCA_SLICE
    &ENG_SLICE,                  /* 13 (FELUCCA_SLICE=0 builds without it) */
#endif
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* part t's engine memory goes to its engine (t->engine; voice.c engine_block, once a block: on a switch, after
 * the old engine's fade, before any note on the new one): zeroed (its engine_t.mem bytes) when another
 * engine's state is in it. An engine without memory leaves it as it is: a switch back finds its state. */
static void eng_mem_claim(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), e, k, n;
    uint32_t *w;
    if (p >= NLIVE)                           /* (a hidden part has no region and no engine running) */
        return;
    e = eng_idx(t->engine);
    n = ENGINES[e]->mem;
    if (!n || eng_mem_owner[p] == e + 1u)
        return;
    w = (uint32_t *)ENG_MEM(p);
    for (k = 0; k < (n + 3u) / 4u; k++)
        w[k] = 0;
    eng_mem_owner[p] = (uint8_t)(e + 1u);
}

/* the sample source a track's sound plays that has no data (SAMPLE's SET, GRAIN's SRC, SLICE's SRC: an empty or
 * invalid user slot, a set or PIANO this build lacks): its name (it plays a sine: eng_sample.c smp_sine), 0 = none;
 * *code a small number for it (1.., per engine and source) so the UI says it once (ui_input.c sample_notice) */
static uint8_t snd_said[NTRK];   /* per track: the code last said, 0 = none (a project load clears them: said again) */
static const char *snd_missing(const track_t *t, uint32_t *code)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL;
#if FELUCCA_SLICE
    if (e == &ENG_SLICE) {
        si = slc_src_of(t->p);
        *code = 64u + si;
        return slc_get(si) ? 0 : N_SLC_SRC[si];
    }
#endif
    if (e != &ENG_SAMPLE && e != &ENG_GRAIN)
        return 0;
    *code = (e == &ENG_GRAIN ? 32u : 1u) + si % 32u;
    return smp_set_missing(si) ? SMP_ALL_NAMES[si] : 0;
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with FELUCCA_FM4 it follows FM6). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    0,                           /* ANALOG */
    12,                          /* FM6 */
#if FELUCCA_FM4
    1,                           /* DIGITAL */
#endif
    2, 3, 4, 5, 6, 7, 8, 9,      /* PHASE LOFI SAMPLE VOICE TRIO WHEEL GRAIN PHYS */
    11,                          /* NOISE */
#if FELUCCA_SLICE
    13,                          /* SLICE */
#endif
#if FELUCCA_DRUM
    10,                          /* DRUM (hidden in DUELL) */
#endif
};

/* the engines one can pick (engine 1 only with FELUCCA_FM4), in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e)
{
    return e < NENGINES && (FELUCCA_FM4 || e != ENGI_DIGITAL) && (FELUCCA_DRUM || e != ENGI_DRUM);
}
/* the engine a sound that names e plays on: e itself, DIGITAL (hidden) FM6, DRUM (hidden, DUELL) ANALOG */
static uint32_t eng_safe(uint32_t e) { return eng_ok(e) ? e : e == ENGI_DRUM ? 0u : ENGI_FM6; }
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = eng_safe(e);                         /* (DIGITAL without FELUCCA_FM4: its sounds play as FM6; DRUM: ANALOG) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

#include "sounds.c"              /* DUELL: the factory sounds the browser lists (build/gen/duell_sounds.h) */

/* DUELL factory patterns: the default content of the 16 pattern slots P01..P16 (SAVE > PATTERNS loads one into the selected
 * part, ui.c pat_load); our own set of classic rhythmic / melodic idioms, written for this firmware (no melody of a
 * recording). One bar of 16 steps (1/16 notes), LEN 16. EVERY note is ROOT + offset in semitones (O(x), ROOT = C4 = 60,
 * the note the SEQUENCER arp mode reads as offset 0: arp_seq_notes uses note - 60), so a pattern works on any engine and
 * in the SEQ arp on the held notes; in the step sequencer it plays from C4 (OCT- / SCL TRANS move it; it is the same bar
 * for a bass, a pad or a lead). 0 = rest. Flags: 1 accent, 2 slide, 4 tie (holds the previous note).
 * Names: at most 12 characters. DRUM and SLICE take them as drum / slice numbers (keys 60 + offset), nothing special.
 * The table of idioms is in SPEC.md (PATTERNS). */
#define ROOT 60
#define O(x) (ROOT + (x))
#define T_ 4
#define A_ 1
#define S_ 2
static const struct {
    const char *name;
    uint8_t note[16], flags[16];
} PATTERNS[] = {
    /* 1 disco bass: 16ths jumping root / octave, the bar's last beat walks up to the 5th and b7 */
    {"MORODER OCT", {O(0), O(12), O(0), O(12), O(0), O(12), O(0), O(12), O(0), O(12), O(0), O(12), O(0), O(12), O(7), O(10)},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 2 303 acid line: 16ths with rests, octave jumps, accents and slides */
    {"ACID LINE", {O(0), O(0), O(12), O(0), O(3), 0, O(0), O(10), O(0), 0, O(12), O(7), O(0), O(3), 0, O(5)},
     {A_, 0, A_ | S_, 0, S_, 0, 0, A_ | S_, 0, 0, A_, S_, 0, 0, 0, S_}},
    /* 3 house bass: the root on the off-beat 8ths only, the last one an octave up */
    {"OFFBEAT", {0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(12), 0},
     {0, 0, A_, 0, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0}},
    /* 4 techno rumble: three 16ths after every beat, the beat itself empty (the kick sits there) */
    {"TECHNO RUMBL", {0, O(0), O(0), O(0), 0, O(0), O(0), O(0), 0, O(0), O(0), O(0), 0, O(0), O(0), O(-2)},
     {0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0}},
    /* 5 trance / psy rolling bass: unbroken 16ths, the octave on the off-beat 8ths */
    {"TRANCE ROLL", {O(0), O(0), O(12), O(0), O(0), O(0), O(12), O(0), O(0), O(0), O(12), O(0), O(0), O(0), O(12), O(10)},
     {A_, 0, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0}},
    /* 6 straight 8th-note pulse (new wave / driving rock bass), the last 8th on the 5th */
    {"8TH PULSE", {O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(7), 0},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 7 arpeggio: a minor triad climbing two octaves and falling back, 16ths */
    {"ARP UPDOWN", {O(0), O(3), O(7), O(12), O(15), O(19), O(24), O(19), O(15), O(12), O(7), O(3), O(0), O(3), O(7), O(3)},
     {A_, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* 8 Berlin school run: the root as a pedal on every other 16th, a figure climbing and falling between */
    {"BERLIN RUN", {O(0), O(7), O(0), O(10), O(0), O(12), O(0), O(10), O(0), O(7), O(0), O(3), O(0), O(5), O(0), O(3)},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 9 Detroit / house stab rhythm: syncopated chord hits (3-3-4-3-3 feel), root, minor 3rd, 5th */
    {"DETROIT STAB", {O(0), 0, 0, O(3), 0, 0, O(7), 0, 0, 0, O(0), 0, 0, O(3), 0, 0},
     {A_, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* 10 funk 16th groove: the one, ghost 16ths, octave pops, a walk-down fill */
    {"FUNK 16TH", {O(0), 0, O(0), O(12), 0, O(0), 0, O(10), 0, O(0), 0, O(0), O(12), 0, O(7), O(5)},
     {A_, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0}},
    /* 11 dub half-time: few long notes, the root, a 5th below after the space */
    {"DUB HALFTIME", {O(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, O(-5), 0, 0, 0},
     {A_, T_, T_, T_, T_, T_, T_, 0, 0, 0, 0, 0, 0, T_, T_, 0}},
    /* 12 reggae skank: the off-beat of beats 2 and 4, a bubbling 5th in between */
    {"SKANK", {0, 0, 0, 0, 0, 0, O(0), 0, 0, 0, 0, O(7), 0, 0, O(0), 0},
     {0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, A_, 0}},
    /* 13 galloping bass: eighth + two 16ths on every beat, the last beat up a minor 3rd */
    {"GALLOP", {O(0), 0, O(0), O(0), O(0), 0, O(0), O(0), O(0), 0, O(0), O(0), O(3), 0, O(3), O(3)},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 14 minor pentatonic riff: root, b3, 4, 5, b7 (and the b7 below), a slide into the 4th */
    {"PENTA RIFF", {O(0), 0, O(3), O(5), 0, O(7), 0, O(5), O(3), 0, O(0), 0, O(-2), 0, O(0), 0},
     {A_, 0, 0, S_, 0, A_, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0}},
    /* 15 breakbeat syncopation: the hits of a broken kick / snare pattern (1, a of 1, 2&, ...) */
    {"BREAKBEAT", {O(0), 0, 0, O(0), 0, 0, O(5), 0, 0, O(0), O(0), O(12), 0, 0, O(7), 0},
     {A_, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, A_, 0}},
    /* 16 four on the floor: one short hit on every beat (a kick-style gate for any sound) */
    {"FOUR FLOOR", {O(0), 0, 0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0},
     {A_, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
};
#undef S_
#undef A_
#undef T_
#undef O
#undef ROOT
#define NPATTERNS (sizeof PATTERNS / sizeof PATTERNS[0])

/* the parts at power-on (engine, preset, PATTERNS[n - 1] in the sequencer, 0 = empty: all are): bass, pad, lead, drums.
 * DUELL: the preset is the engine's factory sound k (sounds.c; ANALOG 4 = SQUELCH), INIT for an engine with none yet
 * (FM6 until its sounds come); the host renders (tests/hostsim.c host_preset) still read Felucca's presets with it */
static const uint8_t TRK_DEF[NPART][3] = {{0, 4, 0}, {ENGI_FM6, 4, 0}, {3, 0, 0}, {ENGI_DRUM, 0, 0}}; /* ANALOG ACID,
                                                                       * FM6 PAD (was DIGITAL PAD), LOFI PULSE LD, DRUM KIT */
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
