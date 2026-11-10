/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * default patterns (the content of the 32 slots of SAVE > PATTERNS) and the parts' sounds at power-on. */
#include "dsp.c"

/* The parts' engine memory (1.5): a part plays one engine at a time, so the engines with large per-part state
 * (engine_t.mem: PHYS's slots, FM6's notes, WAVE's table)
 * keep it in one region per part instead of an array each. The region holds the state of the engine that
 * last claimed it (eng_mem_owner); a switch to another such engine zeroes its engine_t.mem bytes, which is
 * the state each had at power-on, so a switch sounds as it did with the arrays (tests/union_test.c). State
 * that runs on across notes and switches stays outside (WHEEL's wheels).
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
#include "eng_formant.c"
#include "eng_trio.c"
#include "eng_wheel.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_noise.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0) */
#include "eng_wave.c"           /* WAVE (DUELL): the wavetable engine, its tables built from recipes into the part's engine memory */

/* The placeholder of the retired DIGITAL, SAMPLE, GRAIN, DRUM and SLICE (1, 4, 8, 10, 13; core.h ENGI_RETIRED). No code, no presets, never
 * selectable (eng_ok); a stored sound of theirs keeps its values on the track and plays silent here. Its EDIT slots are "-" (blank
 * cards, no knob acts on them: ui_input.c) and wide (-64..127, the stores' range), so a load keeps every stored value as it was
 * (param_fit clamps to the range) and a save writes it back */
static void retired_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void retired_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
#define RETIRED_E {"-", F_INT, -64, 127, 0, 0, 0}
static const engine_t ENG_RETIRED = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {RETIRED_E, RETIRED_E, RETIRED_E, RETIRED_E, RETIRED_E, RETIRED_E, RETIRED_E, RETIRED_E},
    .note_on = retired_note_on,
    .render = retired_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};
#undef RETIRED_E

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG,                 /* 0 */
    &ENG_RETIRED,                /* 1: reserved (DIGITAL, replaced by FM6; its code is gone since DUELL 0.2.14) */
    &ENG_PHASE,                  /* 2 */
    &ENG_LOFI,                   /* 3 */
    &ENG_RETIRED,                /* 4: reserved (SAMPLE, retired in DUELL 0.2.9: silent, core.h ENGI_RETIRED) */
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
    &ENG_TRIO,                   /* 6 */
    &ENG_WHEEL,                  /* 7 */
    &ENG_RETIRED,                /* 8: reserved (GRAIN, retired in DUELL 0.2.9) */
    &ENG_PHYS,                   /* 9 (ENGI_PHYS) */
    &ENG_RETIRED,                /* 10: reserved (DRUM, hidden in DUELL; its code is gone since 0.2.14) */
    &ENG_NOISE,                  /* 11 */
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
    &ENG_RETIRED,                /* 13: reserved (SLICE, retired in DUELL 0.2.9; WAVE keeps 14) */
    &ENG_WAVE,                   /* 14 (ENGI_WAVE) */
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* part t's engine memory goes to its engine (t->engine; voice.c engine_block, once a block: on a switch, after
 * the old engine's fade, before any note on the new one): zeroed (its engine_t.mem bytes) when another
 * engine's state is in it. An engine without memory leaves it as it is: a switch back finds its state. */
static void eng_mem_claim(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), e, n;
    if (p >= NLIVE)                           /* (a hidden part has no region and no engine running) */
        return;
    e = eng_idx(t->engine);
    n = ENGINES[e]->mem;
    if (!n || eng_mem_owner[p] == e + 1u)
        return;
    memset(ENG_MEM(p), 0, (n + 3u) & ~3u);               /* (libc.c: a word at a time) */
    eng_mem_owner[p] = (uint8_t)(e + 1u);
}

/* a track on a retired engine (core.h ENGI_RETIRED: a stored sound of DIGITAL, SAMPLE, GRAIN, DRUM or SLICE; it plays silent): 1, and *code a small
 * number for it (the engine + 1) so the UI says NO ENGINE once (ui_input.c gone_notice); 0 = a playing engine */
static uint8_t snd_said[NTRK];   /* per track: the code last said, 0 = none (a project load clears them: said again) */
static int eng_gone_on(const track_t *t, uint32_t *code)
{
    uint32_t e = t->eng_req % NENGINES;
    if (!ENGI_RETIRED(e))
        return 0;
    *code = e + 1u;
    return 1;
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never a retired one. The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    0,                           /* ANALOG */
    12,                          /* FM6 */
    2, 3, 5, 6, 7, 9,            /* PHASE LOFI VOICE TRIO WHEEL PHYS (not DIGITAL 1, SAMPLE 4, GRAIN 8: retired) */
    11,                          /* NOISE (not DRUM 10, SLICE 13: retired) */
    14,                          /* WAVE (DUELL; last: PATCHES > TYPE keeps an engine filter as its place in this list,
                                  * ui.c ui_lcat, so the engines before keep their places) */
};

/* the engines one can pick (never a retired one), in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e)
{
    return e < NENGINES && !ENGI_RETIRED(e);
}
/* the engine a sound that names e plays on: e itself; a retired one (DIGITAL, SAMPLE, GRAIN, DRUM, SLICE) is the silent placeholder:
 * no engine filter lists it (PATCHES > TYPE), ALL and its category do */
static uint32_t eng_safe(uint32_t e)
{
    return e % NENGINES;
}
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = 0u;                                  /* (a retired one: no place, ANALOG's (the ENGINE knob steps from there)) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

#include "sounds.c"              /* DUELL: the factory sounds the browser lists (build/gen/duell_sounds.h) */

/* DUELL's 32 default patterns: the content the 32 slots P01..P32 of SAVE > PATTERNS hold on a new unit and after ERASE (patslot.c
 * ps_default; they are ordinary slots, all overwritable, no second list: the arp's SEQ mode, the sequencer and the editor read the
 * same slots). Our own set of classic rhythmic / melodic idioms, written for this firmware (no melody of a recording; no drum
 * imitations: this is not a drum machine, PATTERNS_PROPOSAL.md). A pattern has LEN steps (8..32) on DIV (2 = 1/16, 1 = 1/8, 4 = 1/8T); SWING 0
 * and GATE 64 for all. EVERY note is ROOT + offset in semitones (O(x), ROOT = C4 = 60,
 * the note the SEQUENCER arp mode reads as offset 0: arp_seq_notes uses note - 60), so a pattern works on any engine and
 * in the SEQ arp on the held notes; in the step sequencer it plays from C4 (OCT- / SCL TRANS move it; it is the same bar
 * for a bass, a pad or a lead). 0 = rest. Flags: 1 accent, 2 slide, 4 tie (holds the previous note).
 * Names: at most 12 characters from the unit's set. `PAT(n)` of a factory sound (preset_t.pat) is the number n of this list (P01 = 1).
 * The table of idioms is in SPEC.md (PATTERNS). */
#define ROOT 60
#define O(x) (ROOT + (x))
#define T_ 4
#define A_ 1
#define S_ 2
#define NPAT_STEPS 32u
static const struct {
    const char *name;
    uint8_t len, div;
    uint8_t note[NPAT_STEPS], flags[NPAT_STEPS];
} PATTERNS[] = {
    /* 1 disco bass: 16ths jumping root / octave, the bar's last beat walks up to the 5th and b7 */
    {"MORODER OCT", 16, 2, {O(0), O(12), O(0), O(12), O(0), O(12), O(0), O(12), O(0), O(12), O(0), O(12), O(0), O(12), O(7), O(10)},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 2 303 acid line: 16ths with rests, octave jumps, accents and slides */
    {"ACID LINE", 16, 2, {O(0), O(0), O(12), O(0), O(3), 0, O(0), O(10), O(0), 0, O(12), O(7), O(0), O(3), 0, O(5)},
     {A_, 0, A_ | S_, 0, S_, 0, 0, A_ | S_, 0, 0, A_, S_, 0, 0, 0, S_}},
    /* 3 house bass: the root on the off-beat 8ths only, the last one an octave up */
    {"OFFBEAT", 16, 2, {0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(12), 0},
     {0, 0, A_, 0, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0}},
    /* 4 techno rumble: three 16ths after every beat, the beat itself empty (the kick sits there) */
    {"TECHNO RUMBL", 16, 2, {0, O(0), O(0), O(0), 0, O(0), O(0), O(0), 0, O(0), O(0), O(0), 0, O(0), O(0), O(-2)},
     {0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0}},
    /* 5 trance / psy rolling bass: unbroken 16ths, the octave on the off-beat 8ths */
    {"TRANCE ROLL", 16, 2, {O(0), O(0), O(12), O(0), O(0), O(0), O(12), O(0), O(0), O(0), O(12), O(0), O(0), O(0), O(12), O(10)},
     {A_, 0, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0}},
    /* 6 straight 8th-note pulse (new wave / driving rock bass), the last 8th on the 5th */
    {"8TH PULSE", 16, 2, {O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(7), 0},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 7 minimal: a repeating cell where one upper note changes each bar */
    {"GLASS LOOP", 16, 2, {O(0), O(7), O(3), O(7), O(0), O(7), O(3), O(7), O(0), O(8), O(3), O(8), O(0), O(8), O(3), O(8)},
     {A_, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* 8 Berlin school run: the root as a pedal on every other 16th, a figure climbing and falling between */
    {"BERLIN RUN", 16, 2, {O(0), O(7), O(0), O(10), O(0), O(12), O(0), O(10), O(0), O(7), O(0), O(3), O(0), O(5), O(0), O(3)},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 9 Detroit / house stab rhythm: syncopated chord hits (3-3-4-3-3 feel), root, minor 3rd, 5th */
    {"DETROIT STAB", 16, 2, {O(0), 0, 0, O(3), 0, 0, O(7), 0, 0, 0, O(0), 0, 0, O(3), 0, 0},
     {A_, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* 10 funk 16th groove: the one, ghost 16ths, octave pops, a walk-down fill */
    {"FUNK 16TH", 16, 2, {O(0), 0, O(0), O(12), 0, O(0), 0, O(10), 0, O(0), 0, O(0), O(12), 0, O(7), O(5)},
     {A_, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0}},
    /* 11 dub half-time: few long notes, the root, a 5th below after the space */
    {"DUB HALFTIME", 16, 2, {O(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, O(-5), 0, 0, 0},
     {A_, T_, T_, T_, T_, T_, T_, 0, 0, 0, 0, 0, 0, T_, T_, 0}},
    /* 12 reggae skank: the off-beat of beats 2 and 4, a bubbling 5th in between */
    {"SKANK", 16, 2, {0, 0, 0, 0, 0, 0, O(0), 0, 0, 0, 0, O(7), 0, 0, O(0), 0},
     {0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, A_, 0}},
    /* 13 galloping bass: eighth + two 16ths on every beat, the last beat up a minor 3rd */
    {"GALLOP", 16, 2, {O(0), 0, O(0), O(0), O(0), 0, O(0), O(0), O(0), 0, O(0), O(0), O(3), 0, O(3), O(3)},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 14 minor pentatonic riff: root, b3, 4, 5, b7 (and the b7 below), a slide into the 4th */
    {"PENTA RIFF", 16, 2, {O(0), 0, O(3), O(5), 0, O(7), 0, O(5), O(3), 0, O(0), 0, O(-2), 0, O(0), 0},
     {A_, 0, 0, S_, 0, A_, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0}},
    /* 15 boogie-woogie bass: root, 3rd, 5th, 6th, b7 in pairs */
    {"BOOGIE BASS", 16, 2, {O(0), O(0), O(4), O(4), O(7), O(7), O(9), O(9), O(10), O(10), O(9), O(9), O(7), O(7), O(4), O(4)},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 16 synthpop hook on dotted rhythms */
    {"DOTTED HOOK", 16, 2, {O(12), 0, 0, O(10), 0, 0, O(7), 0, 0, O(5), 0, 0, O(7), 0, 0, 0},
     {A_, 0, 0, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0}},
    /* 17 trap / halftime: a long root and a slide down an octave */
    {"SUB DROP", 16, 2, {O(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, O(-12), 0, 0, 0},
     {A_, T_, T_, T_, T_, T_, T_, 0, 0, 0, 0, 0, A_ | S_, T_, T_, T_}},
    /* 18 Latin: the anticipated bass of salsa and son */
    {"TUMBAO", 16, 2, {0, 0, 0, 0, 0, 0, O(0), 0, 0, 0, 0, 0, O(7), 0, O(12), O(7)},
     {0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, A_, 0}},
    /* 19 krautrock: driving 8ths between root and octave */
    {"MOTORIK", 16, 2, {O(0), 0, O(0), 0, O(12), 0, O(0), 0, O(0), 0, O(0), 0, O(12), 0, O(0), 0},
     {A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 20 jazz: a walking bass in quarters up and down a minor scale */
    {"WALK LINE", 8, 1, {O(0), O(3), O(5), O(6), O(7), O(5), O(3), O(2)},
     {A_, 0, 0, 0, A_, 0, 0, 0}},
    /* 21 UK garage: the skipping sub bass of 2-step */
    {"2-STEP SUB", 16, 2, {O(0), 0, 0, O(0), 0, 0, O(7), 0, 0, 0, O(0), 0, 0, O(3), 0, O(5)},
     {A_, 0, 0, 0, 0, 0, A_, 0, 0, 0, A_, 0, 0, 0, 0, 0}},
    /* 22 neo-soul: sparse low ghost notes with one accent */
    {"GHOST FUNK", 16, 2, {O(0), 0, 0, O(12), 0, 0, 0, O(0), 0, 0, O(10), 0, 0, O(0), 0, 0},
     {A_, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* 23 blues 12/8: a rolling triplet groove */
    {"TRIPLET SWAY", 12, 4, {O(0), 0, O(7), O(3), 0, O(7), O(0), 0, O(7), O(5), 0, O(7)},
     {A_, 0, 0, A_, 0, 0, A_, 0, 0, A_, 0, 0}},
    /* 24 trance: one chord gated 3-3-3-3-2-2 */
    {"TRANCE GATE", 16, 2, {O(0), 0, 0, O(0), 0, 0, O(0), 0, 0, O(0), 0, O(0), 0, O(0), 0, 0},
     {A_, 0, 0, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0}},
    /* 25 90s rave: stabs on the one, the a of one and the and of two */
    {"RAVE STABS", 16, 2, {O(7), 0, 0, O(7), 0, 0, 0, O(10), 0, 0, 0, 0, O(7), 0, 0, O(12)},
     {A_, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, A_, 0, 0, A_}},
    /* 26 electric-piano comping: short chords on a 3-3-2 rhythm with ties */
    {"EP COMP", 16, 2, {O(0), 0, 0, 0, O(7), 0, 0, 0, O(3), 0, 0, 0, O(10), 0, 0, 0},
     {A_, T_, 0, 0, 0, T_, 0, 0, A_, 0, 0, 0, 0, T_, 0, 0}},
    /* 27 synthwave lead hook */
    {"NEON HOOK", 16, 2, {O(12), 0, O(10), 0, O(7), 0, O(10), 0, O(12), 0, 0, 0, O(15), 0, O(12), 0},
     {A_, 0, 0, 0, 0, 0, 0, 0, A_, T_, T_, 0, 0, 0, 0, 0}},
    /* 28 pop / minor: a call and an answer (32 steps) */
    {"NIGHT CALL", 32, 2, {O(7), 0, O(7), O(10), 0, O(12), 0, O(10), O(7), 0, 0, 0, 0, 0, 0, 0, O(5), 0, O(5), O(8), 0, O(10), 0, O(8), O(3), 0, 0, 0, 0, 0, 0, 0},
     {A_, 0, 0, 0, 0, 0, 0, 0, A_, T_, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0, A_, T_, T_, T_, 0, 0, 0, 0}},
    /* 29 ambient: two long tied chord roots a bar */
    {"PAD BREATH", 16, 2, {O(0), 0, 0, 0, 0, 0, 0, 0, O(7), 0, 0, 0, 0, 0, 0, 0},
     {A_, T_, T_, T_, T_, T_, T_, T_, A_, T_, T_, T_, T_, T_, T_, T_}},
    /* 30 FM marimba ostinato: the root alternating with the octave and a moving third */
    {"DX OSTINATO", 16, 2, {O(0), 0, O(12), 0, O(7), 0, O(12), 0, O(3), 0, O(12), 0, O(5), 0, O(12), 0},
     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* 31 FM bells and tines: sparse tones in 5ths and octaves with ties */
    {"DX BELLS", 16, 2, {O(12), 0, 0, 0, O(19), 0, 0, 0, O(16), 0, 0, 0, O(24), 0, 0, 0},
     {A_, 0, 0, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, T_, 0, 0}},
    /* 32 prog: a riff in 7/8 (14 steps) */
    {"SEVEN EIGHT", 14, 2, {O(0), 0, O(0), 0, O(7), 0, O(3), 0, O(0), 0, O(10), 0, O(7), 0},
     {A_, 0, 0, 0, 0, 0, A_, 0, 0, 0, 0, 0, 0, 0}},
};
#undef S_
#undef A_
#undef T_
#undef O
#undef ROOT
#define NPATTERNS (sizeof PATTERNS / sizeof PATTERNS[0])

/* the parts at power-on (engine, preset, PATTERNS[n - 1] in the sequencer, 0 = empty: all are): bass, pad (lead and drums: the hidden parts 3 and 4, gone, memory trim 1).
 * DUELL: the preset is the engine's factory sound k (sounds.c; ANALOG 4 = SQUELCH), INIT for an engine with none yet
 * (FM6 until its sounds come); the host renders (tests/hostsim.c host_preset) still read Felucca's presets with it */
static const uint8_t TRK_DEF[NPART][3] = {{0, 4, 0}, {ENGI_FM6, 4, 0}   /* ANALOG ACID, FM6 PAD (was DIGITAL PAD) */
#if NPART > 2
                                          , {3, 0, 0}, {0, 0, 0}                /* (the host rig with four parts: LOFI PULSE LD, ANALOG) */
#endif
};
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
