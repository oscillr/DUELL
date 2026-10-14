/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA core types: tracks, voices, engines, parameters.
 * Four tracks, each a synth part: its own engine, preset, parameters, voices and 64-step
 * pattern. The parts share one budget of NVOICE sounding voices (voice.c). Drums are the DRUM
 * engine (General MIDI map) on any part; the SAMPLE engine's PERC set is retired (drum_from_perc), and DUELL 0.2.9 retired the
 * SAMPLE engine itself (ENGI_SAMPLE).
 * Sections: sizes, parameters, voices and engines, tracks and the song, system. */
#include <stdint.h>

/* ------------------------------------------------------------ sizes --- */
#define NVOICE 8                 /* voices per part, and the budget shared by all parts */
#define NPART 4                  /* synth parts: tracks 1..4 */
#ifndef NSHOWN
#define NSHOWN 2                 /* DUELL: only parts A and B (tracks 1..2) are shown and played; tracks 3..4 stay in every
                                  * stored layout but have no screen, key, step, arp, voice or MIDI channel (NPART stays 4) */
#endif
#define NTRK NPART               /* tracks (the formats and the protocol count these): every track is a part */
#define NSTEP 64
#ifndef DUELL_TEST_MODE
#define DUELL_TEST_MODE 0        /* DUELL developer switch (-DDUELL_TEST_MODE=1; env DUELL_TEST_MODE=1 for build.py / web/emu/build.sh / host_tests.sh): 1 builds in MENU > SYSTEM > TEST (the CPU test screen)
                                  * and the four CPU test multis M13..M16 (tools/gen_duell_multis.py must be run with the same switch); 0 (the release default) leaves both out */
#endif
#define HALF_FRAMES 128          /* I2S half buffer: 2.9 ms at 44.1 kHz (a key waits 0..1 half, then plays 1 half later) */
#ifndef FELUCCA_FM4
#define FELUCCA_FM4 0            /* the DIGITAL engine (eng_digital.c, four-operator FM): kept in the tree, not built; its sounds
                                  * convert to FM6 (fm4_convert.c) */
#endif
#if FELUCCA_FM4
#error "DUELL: FELUCCA_FM4=1 is gone: DIGITAL's OP ENV / OP LEVEL ids (61..80) hold DUELL's own parameters (P_M5SRC..)"
#endif
#define NENGINES 15              /* DUELL: WAVE (14) appended after SLICE (13); the engine numbers never move (the stores and the
                                  * editor protocol hold them: append, never renumber) */
#define ENGI_WAVE 14u            /* DUELL: the wavetable engine (eng_wave.c) */
#define ENGI_DIGITAL 1u          /* reserved: never selectable (eng_ok), its sounds load as FM6 */
/* DUELL 0.2.9 (owner: "REMOVE the sample engines"; WAVE replaces them): SAMPLE (4), GRAIN (8) and SLICE (13) are RETIRED. Their
 * code and the built-in sample data are gone; the numbers stay reserved as DIGITAL's does (engines.c: the silent placeholder "-",
 * never selectable, in no list). A stored sound / part / patch / multi / project that names one loads as it was stored (every value
 * kept, so a save writes it back unchanged) and plays SILENT on the placeholder; the UI says NO ENGINE once (ui_input.c
 * gone_notice) and its engine shows "-". The user sample slots USR1 / USR2 (flash 0xA0000..0xC7FFF) are free (storage.c) */
#define ENGI_SAMPLE 4u
#define ENGI_GRAIN 8u
#define ENGI_SLICE 13u
#define ENGI_RETIRED(e) ((e) == ENGI_SAMPLE || (e) == ENGI_GRAIN || (e) == ENGI_SLICE)   /* a retired engine number (0.2.9) */
#ifndef FELUCCA_DRUM
#define FELUCCA_DRUM 0           /* DUELL: the DRUM engine (eng_drum.c, engine 10) is hidden as DIGITAL is: kept in the tree and in
                                  * every stored layout, never selectable (eng_ok); a sound that names it plays as ANALOG
                                  * (engines.c eng_safe, ui.c set_engine_of, ui_input.c) */
#endif
#if FELUCCA_DRUM
#error "DUELL: FELUCCA_DRUM=1 is gone: DRUM's lane level ids (83..90) hold DUELL's own parameters (P_L2SYNC..)"
#endif
#define NENG_SHOWN (NENGINES - !FELUCCA_FM4 - !FELUCCA_DRUM - 3)   /* the engines one can pick (not the three retired ones): PRESETS, the EDIT layer,
                                                * the editor, in the display order of engines.c ENGINE_ORDER */
#define UP_SLOTS 128u            /* stored patches (upreset.c): shadows of factory sounds and user patches, 128 in all */
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* ------------------------------------------------------- parameters --- */
enum {
    F_INT, F_PCT, F_BIPCT, F_TIME, F_LFOHZ, F_CUTOFF, F_DB, F_SEMI, F_ENUM, F_BPM, F_NOTE,
    F_ONOFF, F_OCT, F_STEPS,
    F_EQDB, F_EQLO, F_EQMID, F_EQHI, F_EQQ,
    F_DKCH, F_DKNOTE, F_DKRLS, F_DKHLD,    /* DUELL DUCK: F_DKCH retired (SRC is an F_ENUM; kept: the fmt numbers are in the protocol), NOTE (0 ANY, n = note n, 1..127), RELEASE (log, 20 ms .. 2 s); ATK is F_INT ms, HOLD (F_DKHLD) 5 ms a step */
       /* DUELL EQ: a gain (-12..+12 dB), the three freqs (a semitone step around 120 Hz /
                                                 * 1 kHz / 8 kHz), Q (1/6 octave steps around 1.0): all 0 at their default */
    F_FILT,                                /* the MULTIMODE FILTER's CUT (DUELL 0.2.9; 0.2.2's bipolar CUT before): a dial step 0..UF_STEPS shown as its cutoff, Hz / kHz
                                            * (appended: the fmt numbers are in the protocol) */
    F_FMODE                                /* DUELL 0.2.9: the MULTIMODE FILTER's MODE, 0 LP .. 64 BP .. 127 HP, shown "LP" "LP>BP" "BP" "BP>HP" "HP" */
};

typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min, max, def;
    const char *const *names;   /* F_ENUM */
    const char *unit;           /* F_INT / F_ENUM optional unit */
} param_desc_t;

enum {                          /* per-track parameters */
    P_LEVEL,
    P_ATK, P_DEC, P_SUS, P_REL,
    P_ED_FLT, P_ED_PIT, P_ED_SHP, P_EQLG,      /* P_EQLG: Felucca's P_ED_FX (never read), now the EQ's LOW gain (DUELL, core.h PL_*) */
    P_LRATE, P_LWAVE, P_LPHASE, P_LFADE,
    P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP,
    P_AMODE, P_ARATE, P_AOCT, P_AGATE,
    P_ASWING, P_APROB, P_AHOLD, P_AORDER,
    P_ROOT, P_SCALE, P_QUANT, P_TRANS,
    P_SLEN, P_SDIV, P_SSWING, P_SGATE,
    P_DIST, P_CHOR, P_DLY, P_REV,
    P_VOICE, P_GLIDE, P_PAN, P_MUTE,
    P_GLMODE, P_PRIO, P_ALLOC, P_DETUNE,
    P_FREEA, P_FREEB, P_FREEC, P_FREED,        /* were the SLICER insert's SLCR PAT RATE DEPTH (removed). The MULTIMODE FILTER (DUELL 0.2.9; 0.2.2's universal
                                                * filter): P_FREEA its CUT (0..UF_STEPS, the open end the default), P_FREEB RES (0..127), P_FREEC MODE (0 LP .. 64 BP
                                                * .. 127 HP), P_FREED SLOPE (1 = 12 dB, 2 = 24 dB; 0 only in a store of before: uf_legacy), read only on the engines
                                                * with engine_t.ufilt (voice.c uf_run). A store of Felucca's layout (the SLICER's values here: 1, 1, 127) takes the
                                                * defaults (param_retired) */
    P_M1SRC, P_M1DST, P_M1AMT,                 /* modulation matrix (mod.c): 8 slots of SRC, DST, AMT (DUELL: slots 5..8 */
    P_M2SRC, P_M2DST, P_M2AMT,                 /* on the ids that were DIGITAL's OP ENV, see PL_* below); new common */
    P_M3SRC, P_M3DST, P_M3AMT,                 /* parameters go just before P_E0 (user presets and projects map by */
    P_M4SRC, P_M4DST, P_M4AMT,                 /* count), or onto a free id (none left) */
    P_M5SRC, P_M5DST, P_M5AMT,
    P_M6SRC, P_M6DST, P_M6AMT,
    P_M7SRC, P_M7DST, P_M7AMT,
    P_M8SRC, P_M8DST, P_M8AMT,
    P_E2ATK, P_E2DEC, P_E2SUS, P_E2REL,        /* DUELL: ENV 2 (voice.c env2_tick), a matrix source only (mod.c MS_ENV2) */
    P_L2RATE, P_L2WAVE, P_L2PHASE, P_L2FADE,   /* DUELL: LFO 2 (voice.c lfo_tick), LFO 1's RATE WAVE PHS FADE; matrix only */
    P_CHRD, P_VOIC,                            /* chord keys (chord.c): one key plays a chord; its voicing */
    P_L2SYNC, P_L2TRIG, P_L2POL,               /* DUELL: LFO 2's SYNC TRIG POL (on DRUM's lane level ids, as the rest: */
    P_ATGT,                                    /* DUELL (SPEC 2.3): ARP TARGET, the arp's units (params.c atgt_bits); on id 86 */
    P_EQLF, P_EQMG, P_EQMF, P_EQMQ,            /* DUELL EQ (fx.c track_eq): LOW freq, MID gain / freq / Q, on DRUM's lane ids 87..90 */
    P_LSYNC, P_LTRIG, P_LPOL,                  /* 1.2 (#154): LFO SYNC (OFF, a note value), TRIG (NOTE, FREE), POL (BI, UNI) */
    P_SQNT,                                    /* 1.2 (#162): QUANTIZE, the steps' recorded / nudged timing (ON: none) */
    P_SPRD,                                    /* (#148): SPREAD, the voices of a POLY / UNISON track around PAN (fx.c) */
    P_ITYPE, P_IA, P_IB, P_IC, P_IMIX,         /* 1.5 (#78, #177): the INSERT, its TYPE, three values and MIX (fx.c) */
    P_FTYPE,                                   /* 1.5 (#104): ANALOG's filter TYPE, LP (as before) / BP / HP (eng_analog.c) */
    P_ESYNC,                                   /* 1.5 (#175): ENV SYNC: ATK DEC REL as note values of the tempo (voice.c) */
    P_EQHG, P_EQHF,                            /* DUELL EQ: HIGH gain and freq (P_COUNT 113, P_E0 105: stores map by count) */
    P_DKCRV, P_DKNOTE, P_DKDEP, P_DKATK, P_DKHLD, P_DKRLS,   /* DUELL DUCK (SPEC 5c.4; fx.c duck_*): the sidechain (P_COUNT 119, P_E0 111). P_DKCRV: the RELEASE's CURVE (LIN, EXP); was the MIDI CH (merged into P_DKSRC) */
    P_DKSRC, P_DKRATE, P_DKTHR,                /* the trigger SOURCE (0 PUMP, 1 PART, 2..17 CH1..16, the MIDI channel), then KNOB 2 by SRC: PUMP's RATE, PART's THRESHOLD, a CH's NOTE (P_DKNOTE) (P_COUNT 122, P_E0 114) */
    P_E2FLT, P_E2SHP,                          /* DUELL: ENV 2 DEST (the classic arrangement, SPEC 2.4): ENV 2's direct cutoff and shape depths, */
    P_L2PIT, P_L2FLT, P_L2SHP,                 /* LFO 2 DEST: LFO 2's pitch, cutoff, shape depths (as P_ED_* / P_LD_*; voice.c track_render) (P_COUNT 127, P_E0 119: the last ids
                                                * MIDI LEARN's codes (ML_E0 120) and the 7-bit stores allow) */
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
    P_COUNT
};

enum {                          /* global parameters */
    G_BPM, G_SWING, G_CLOCK, G_TUNE,
    G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX,
    G_RSIZE, G_RDAMP, G_CRATE, G_CDEPTH,
    G_MIDI, G_SYNC, G_ROUTE, G_INFO,   /* G_ROUTE: MIDI IN, 0 PARTS (the parts' MIDI channels, GLO > MIDI; seq.c midi_pch), 1 SEL;
                                        * 2..4 (older: CH5-8 CH9-12 CH13-16) read as PARTS */
    G_SLOT, G_NAME, G_LOAD, G_SAVE,
    G_ENGSEL, G_ENGGO,          /* no page: the editor switches the engine with a SET of G_ENGSEL; G_ENGGO is
                                 * unused (ids are fixed by the formats and the protocol) */
    G_CLRSEQ, G_INITSND,
    G_RTYPE,                    /* REVERB TYPE: 0 ROOM, 1 SPRING, 2 HALL (1.2, the power-on default), 3 SHIM, 4 RESO (JIANT ports) (fx.c). Was
                                 * G_DRCH, the GM drum part's MIDI channel (inert since 1.0, never read); projects
                                 * of formats before FUN7 load it as ROOM (project.c proj_rtype_room) */
    G_PINGSPREAD,               /* DUELL: the PING-PONG's SPREAD (DLY 2), 0..100 %, stored directly, default 100 (GP def; 0 = the repeats
                                 * centred). Was G_DRLVL, the GM drum part's level (inert; proj_drums_to_part reads it from an old
                                 * project and sets 100) */
    G_PINGPONG,                 /* DUELL (SPEC 5c.2): the delay's PING-PONG, 0 OFF (stored 0 = as before), 1 ON; was G_DRREV, the
                                 * GM drum part's reverb send (inert since 1.0; proj_drums_to_part reads and clears it) */
    G_COUNT
};
#define G_DRLVL G_PINGSPREAD   /* (the old name, for the import of a project with the GM drum part) */
/* DUELL 0.2.5 (SPEC 5c.2): the delay's own modulation (DLY 2/2, fx_bus.c dly_mod) takes two global ids that nothing read: G_SYNC (a
 * placeholder, always 0: its range was the one name "--") is its DEPTH, 0 = off (every store has 0 there); G_MIDI (an USB / TRS
 * choice nothing ever read) is its RATE, a store's old 0 or 1 is only a slow rate that is heard at DEPTH > 0. G_COUNT, the project
 * formats and the protocol's ids do not move */
#define G_DMDEP G_SYNC
#define G_DMRATE G_MIDI

/* stored parameters of an older layout -> today's P_* order. A store keeps np = the P_COUNT it was
 * written with; common parameters are only ever added just before P_E0, so the first np - 8 are
 * P_LEVEL.. in order and the last 8 are P_E0..P_E7; the parameters a store does not have take def[]
 * (user presets, projects of formats 1 and 2) */
static void params_by_count(int16_t *out, const int16_t *in, uint32_t np, const int16_t *def)
{
    uint32_t i, nc = np - 8u;
    for (i = 0; i < P_E0; i++)
        out[i] = i < nc ? in[i] : def[i];
    for (i = 0; i < 8u; i++)
        out[P_E0 + i] = in[nc + i];
}

/* DUELL EQ (SPEC 5c.3): the per-part 3-band EQ's parameters are offsets from their defaults, so a stored 0 (an older store on the
 * ids it reuses, a new id's default) is the EQ off: GAIN a dB step (-12..12), FREQ a semitone step around the band's centre
 * (LOW 120 Hz -24..24, MID 1 kHz -24..24, HIGH 8 kHz -24..12), MID Q a step of 1/6 octave around 1.0 (-12..12: 0.25 .. 4) */
#define EQ_HZ_LOW 120u
#define EQ_HZ_MID 1000u
#define EQ_HZ_HIGH 8000u
/* DUELL DUCK: RELEASE step v (0..67) in ms, 20 x 2^(v / 10): 20 ms .. 2.1 s */
static uint32_t duck_rls_ms(int32_t v)
{
    static const uint16_t T10[10] = {1000, 1072, 1149, 1231, 1320, 1414, 1516, 1625, 1741, 1866};
    uint32_t u = (uint32_t)(v < 0 ? 0 : v > 67 ? 67 : v);
    return (20u * T10[u % 10u] << (u / 10u)) / 1000u;
}
/* band 0 LOW, 1 MID, 2 HIGH: the frequency of semitone step r, Hz x 256 */
static uint32_t eq_hz_q8(uint32_t band, int32_t r)
{
    static const uint16_t SEMI[12] = {32768, 34716, 36781, 38968, 41285, 43740, 46341, 49096, 52016, 55109, 58386, 61858};   /* 2^(j/12) / 2 Q16 */
    int32_t o = r >= 0 ? r / 12 : -((11 - r) / 12), j;
    uint32_t base = band == 0u ? EQ_HZ_LOW : band == 1u ? EQ_HZ_MID : EQ_HZ_HIGH, f;
    j = r - o * 12;
    f = (base * SEMI[j]) >> 7;                                  /* base x 2^(j/12) x 256 */
    return o >= 0 ? f << o : f >> -o;
}

/* DUELL (step 3): Felucca's layout gave 29 ids to things DUELL does not have: P_ED_FX (8, never read), DIGITAL's OP ENV /
 * OP LEVEL (61..80; DIGITAL is retired, its sounds convert to FM6) and DRUM's lane levels (83..90; DRUM is hidden). DUELL
 * puts its own parameters there (matrix slots 5..8, ENV 2, LFO 2; the rest P_FREE*), so P_COUNT stayed 111 (the EQ later took five of the free ids and grew it to 113) and no 7-bit
 * id (motion records, MIDI LEARN codes, the editor protocol, user preset records) has to grow. A store of Felucca's layout
 * (a project before FUN11, a user preset record before UP_VER_DUELL, a MIDI LEARN map without the DUELL mark) holds the
 * old meanings there: its loader maps it by count as before, lets DIGITAL's conversion read the operator values at their
 * old places (PL_FM*), then params_retire puts every retired id at today's default, so no old value reaches a new
 * parameter. PL_*: Felucca's ids, for reading such a store only */
#define PL_ED_FX 8u
#define PL_FM1_ATK 61u                 /* + 5 x op (0..3) + ATK DEC SUS REL LEVEL (0..4) */
#define PL_FM4_LEVEL 80u
#define PL_LN0 83u                     /* DRUM's lane levels KICK .. BELL */
#define PL_LN7 90u
#define PL_NRETIRED 33u                /* the 29 ids above + P_FREEA..D (the SLICER's: DUELL 0.2.9's multimode filter, CUT RES MODE SLOPE) */
static int param_retired(uint32_t id)
{
    return id == PL_ED_FX || (id >= PL_FM1_ATK && id <= PL_FM4_LEVEL) || (id >= PL_LN0 && id <= PL_LN7) ||
           (id >= P_FREEA && id <= P_FREED);   /* (the SLICER's ids, in a store of Felucca's layout: the multimode filter takes its defaults: open, the bypass) */
}
/* Felucca's defaults at the retired ids (OP ENV ATK DEC 0, SUS 127, REL 0, LEVEL 127; the lane levels 127; ED_FX 0) into
 * def: what a store of Felucca's layout older than those ids loads there by count (DIGITAL's conversion reads them) */
static void params_def_felucca(int16_t *def)
{
    uint32_t i;
    def[PL_ED_FX] = 0;
    for (i = PL_FM1_ATK; i <= PL_FM4_LEVEL; i++)
        def[i] = (int16_t)((i - PL_FM1_ATK) % 5u == 2u || (i - PL_FM1_ATK) % 5u == 4u ? 127 : 0);
    for (i = PL_LN0; i <= PL_LN7; i++)
        def[i] = 127;
}
static void params_retire(int16_t *p, const int16_t *def)
{
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        if (param_retired(i))
            p[i] = def[i];
}

/* DUELL 0.2.9 MULTIMODE FILTER (SPEC 2.5, voice.c uf_run). CUT (P_FREEA) is a dial of UF_STEPS steps, each the old universal filter's
 * step (1.55 steps of the cutoff table, 30 Hz .. 16 kHz over 127): step d is table index (40 + 396 d) / 256, so 0 is 30 Hz and
 * UF_STEPS 16 kHz (the open end). SLOPE (P_FREED) is 1 or 2 in this firmware, never 0: a store that holds 0 there (or leaves it out: the
 * patch store's up_def) was written before 0.2.9 and its P_FREEA is 0.2.2's bipolar CUT (0 OFF, -64..-1 a low-pass, 1..63 a
 * high-pass). uf_legacy (serial: 1 on a ufilt 2 engine, which takes the bypass instead) makes it today's CUT + MODE + SLOPE 12 dB: OFF the open low-pass (the bypass), a low-pass the same cutoff
 * exactly (d = 82 + old), a high-pass MODE HP at d = old (0.16 of a table step higher: 0.01 octave). Every loader runs it before it
 * fits the values (upreset.c up_values; project.c proj_uf: projects, multis, the autosave); a second run changes nothing */
#define UF_STEPS 82
#define UF_HP 127                      /* MODE: 0 LP, 64 BP, 127 HP */
static void uf_legacy(int16_t *p, uint32_t serial)
{
    int32_t c = p[P_FREEA];
    if (p[P_FREED])
        return;
    if (serial) {                      /* an engine with a filter of its own (engine_t.ufilt 2: ANALOG, TRIO, NOISE) used 45..48 for nothing before 0.2.9;
                                        * a value there is a leftover of another engine's (a switch kept it): the multimode filter, in series after
                                        * the engine's, takes the bypass, so a sound of before plays bit for bit as it did */
        p[P_FREEA] = UF_STEPS; p[P_FREEB] = 0; p[P_FREEC] = 0; p[P_FREED] = 1;
        return;
    }
    c = c < -64 ? -64 : c > 63 ? 63 : c;
    p[P_FREED] = 1;
    p[P_FREEC] = (int16_t)(c > 0 ? UF_HP : 0);
    p[P_FREEA] = (int16_t)(c > 0 ? c : UF_STEPS + c);
}

/* engine indices the stores name (engines.c ENGINES[]: append-only) */
#define ENGI_PHYS 9u
#define ENGI_DRUM 10u
/* PHYS MODEL DRUM (MODEL 4, before 1.0) -> the DRUM engine, its E values in place: {MODEL, TUNE, TONE, DECY,
 * SNAP, ACC, KICK 0..127, PERC 0..127} -> {KIT, TUNE, TONE, DECY, SNAP, ACC, KICK 0..1, DRV 0}. 1 = it was one
 * (its engine is ENGI_DRUM now); projects (project.c) and user presets (upreset.c) */
static int drum_from_phys(uint32_t engine, int16_t *e)
{
    if (engine != ENGI_PHYS || e[0] != 4)
        return 0;
    e[0] = (int16_t)((e[7] < 0 ? 0 : e[7] > 127 ? 127 : e[7]) >> 5);   /* PERC -> KIT: STD HAND CYM H+CYM */
    e[6] = (int16_t)(e[6] >= 64);                                       /* KICK: PUNCH, ROUND */
    e[7] = 0;
    return 1;
}

/* SAMPLE SET 4 was PERC, the GM-mapped drum kit (tools/gen_samples.py); retired after 1.0.2: its index stays
 * (SET / GRAIN SRC 4 is an alias of PIANO, USR1..3 keep 5..7). A sound that selected it is the DRUM engine with
 * its default kit (eng_drum.c DRUM_PRESETS[0]: the same GM key map, so its patterns still play as drums): its E
 * values become the kit's, the rest of the sound (mix, sends, matrix, ..) stays. 1 = it was one (its engine is
 * ENGI_DRUM now); projects (project.c proj_perc), user presets (upreset.c up_migrate), factory preset 4 and
 * favourites (ui.c, settings_persist.c). Idempotent */
#define SMP_SET_PERC 4u
#define DRUM_KIT_E {0, 64, 70, 64, 64, 100, 0, 0}   /* {KIT STD, TUNE, TONE, DECY, SNAP, ACC, KICK PUNCH, DRV} */
static int drum_from_perc(uint32_t engine, int16_t *e)
{
    static const int16_t KIT[8] = DRUM_KIT_E;
    uint32_t i;
    if (engine != ENGI_SAMPLE || e[0] != (int16_t)SMP_SET_PERC)
        return 0;
    for (i = 0; i < 8u; i++)
        e[i] = KIT[i];
    return 1;
}

/* ------------------------------------------------- voices, engines --- */
enum { V_POLY, V_MONO, V_LEGATO, V_UNISON };   /* P_VOICE */
typedef struct {
    uint8_t note, vel, gate, active;
    uint8_t stage;               /* env: 0 off, 1 attack, 2 decay/sustain, 3 release */
    uint8_t side;                /* SPREAD (fx.c mix_spread): 0 left of PAN, 1 right; set as the voice starts */
    uint8_t stage2;              /* ENV 2 (DUELL, voice.c env2_tick): 0 off, 1 attack, 2 decay/sustain, 3 release */
    int32_t env;                 /* Q24 */
    int32_t env_out;             /* last control-rate amplitude, Q15 */
    int32_t env2;                /* ENV 2, Q24 (ticked only while a matrix slot reads it) */
    int32_t pitch16, pitch_cur;  /* 1/16 semitone, with glide */
    int32_t gstep;               /* glide TIME mode: 1/16 st per control tick, 0 = RATE mode */
    int32_t fine;                /* unison detune: phase increment * (1 + fine / 4096) */
    uint32_t ph[3];
    int32_t s[8];                /* engine state (filters, envs) */
    int32_t uf[4];               /* the multimode filter's integrators, its two stages (voice.c uf_run; engine_t.ufilt engines only) */
    int16_t ufw[3];              /* .. its output's weights of the last block (input, band-pass, low-pass; Q10): the next one ramps from them */
    uint8_t uft;                 /* .. how it last ran: 0 not (from rest next time), 1 12 dB, 2 24 dB */
    int32_t apitch;              /* ARP TARGET (SPEC 2.3, voice.c atg_voice): the arp pitch of the masked units, 1/16
                                  * semitone, with glide */
    int32_t again;               /* .. their level (the arp's gate and accent), Q15, smoothed */
    uint32_t age;
    uint16_t xg;                 /* LAYER XFADE: this note's gain, Q15 (voice_start from track_t lay_xg), 0 = unity: the sound untouched */
    uint8_t mvel;                /* mod.c: the note-on velocity (before UNISON scaling) and RAND of the note */
    int16_t mrnd;
} voice_t;

typedef struct {                 /* an LFO's state (voice.c lfo_tick): track_t's LFO 1 (its lfo_* fields) and LFO 2 */
    uint32_t ph;
    int32_t val;                 /* Q15, POL applied */
    int32_t fade;                /* Q15 ramp after a fresh phrase */
    uint32_t rnd;                /* the S&H value of this cycle (0: none yet) */
    int16_t prev;                /* .. of the cycle before */
} lfo_t;

typedef struct {                 /* per-voice control-rate modulation, computed in voice.c */
    uint32_t inc;                /* phase increment of the base pitch */
    int32_t pitch16;
    int32_t amp0, amp1;          /* Q15 ramp over the block */
    int32_t cutoff;              /* 0..127 << 8 */
    int32_t shape;               /* 0..127 << 8 */
    int32_t envq15;              /* env value (for engines that use it as a mod source) */
    int32_t fine;                /* the residual below 1/16 semitone in inc: unison detune, TUNE, bend (1/4096) */
    uint32_t amask;              /* ARP TARGET (SPEC 2.3): the engine's units that follow the arp (bit per unit, the
                                  * engine's order), 0 = none (every unit on pitch16 / inc, as always: the rest unset) */
    int32_t adelta;              /* .. their pitch: pitch16 + adelta (1/16 semitone), with the same fine */
    int32_t again;               /* .. their level, Q15 (voice.c atg_voice) */
} vmod_t;

/* DUELL MORPH (morph.c, SPEC 3.3): the cross-fade of a choice that differs between the morph's two ends. While a part's block
 * renders (fx.c mix_block: morph_begin .. morph_end) t->p holds the choice of one end (value a) and mxf the other (b) with its
 * weight w (Q15, 1..32767; 0 = no fade, the code path as it always was). Readers: the LFO waves (voice.c lfo_tick), ANALOG's
 * WAVE 0..4 and filter TYPE (eng_analog.c analog_render_m), PHASE's WAVE / WAVE2 (eng_phase.c), TRIO's MODE (eng_trio.c),
 * WHEEL's REG (eng_wheel.c), LOFI's WAVE PLS / TRI / SAW (eng_lofi.c). mxf.on = 0: every reader skips it */
enum { MX_LWAVE, MX_L2WAVE, MX_FTYPE, MX_SLOPE, MX_E0, MX_N = MX_E0 + 8 };   /* MX_E0 + k: P_E0 + k of the part's engine; MX_SLOPE: the multimode filter's SLOPE (0.2.9) */
typedef struct {
    uint8_t on;                  /* some w below is not 0 */
    uint8_t b[MX_N];             /* the other end's choice */
    uint16_t w[MX_N];            /* its weight, Q15 */
} mxf_t;
static mxf_t mxf;
/* DUELL step 5, SPEC 5b.1: the output MODE. SPLIT: part 1 hard left, part 2 hard right, exactly (fx.c folds every stereo source
 * of a part to its side); STEREO: Felucca's mix (free PAN, SPREAD, the shared stereo buses); LAYER: STEREO's mix, and every note
 * played (keys, MIDI channel 1) reaches both parts as input, each handling it by its own arp / chord (seq.c key_on, midi_control.c midi_play). The values are stored (the settings byte, ui.c
 * ui_mode_set; MIDI CC 112): append-only.
 * duell_mode_req: what the UI (MODE page, the settings) or CC 112 asks for; duell_mode: the mode the notes are routed by (the
 * audio ISR takes the request at a block start: seq.c mode_apply, every note let go); the mix follows it through a ~50 ms
 * dip (fx.c dm_step). The product's default is SPLIT: the settings byte 0 (ui_input.c mode_settings) and every power-on
 * (ui_power_on / the boot path). The bare DSP (hostsim, regress, the goldens) starts in STEREO, Felucca's mix bit for bit */
enum { DM_SPLIT, DM_STEREO, DM_LAYER, DM_N };
static volatile uint8_t duell_mode_req = DM_STEREO;
static uint8_t duell_mode = DM_STEREO;
/* the morph's flag (rule 3b): PAN and SPREAD morph only while they are heard, in STEREO and LAYER (not in SPLIT) */
#define duell_stereo (duell_mode != DM_SPLIT)
/* the MODE page's other three (KNOB 2..4), kept with the settings ([15][5..7], ui.c) and in each MULTI; CC 116 117 118.
 * mix_bal: BALANCE -100..+100 (0: both parts as they are; > 0 fades part 1 out, < 0 part 2, linear, fx.c bal_lvl; every mode);
 * mix_xf: XFADE 0..100 % (0 = off; LAYER only: a note's velocity shares its level between the parts, seq.c xf_gain);
 * mix_split: SPLIT 0 = OFF, else the note + 1 (LAYER only: below it part 1 plays alone, at or above it part 2) */
static volatile int8_t mix_bal;
static volatile uint8_t mix_xf, mix_split;

typedef struct {
    const char *name;
    int8_t e[8];                 /* P_E0..P_E7 (signed: an interval below the note; every value fits) */
    uint8_t env[4];              /* ATK DEC SUS REL */
    int8_t fenv;                 /* ENV -> FILTER amount (-64..63) */
    uint8_t mono;                /* 1 = MONO (bass / lead), 0 = POLY */
    /* the rest of the patch; each value is stored + 1, 0 = the default */
    uint8_t fx[4];               /* DIST, CHORUS, DELAY, REVERB sends */
    uint8_t pat;                 /* Felucca's old pattern number (DUELL's own 16 PATTERNS replaced those 13; nothing reads it now). Was: only a hint. A sound load
                                  * never touches the steps; SEQ > PATTERNS loads a pattern (ui.c pat_load). The
                                  * arp is the track's too: a preset does not set it */
} preset_t;
#define FX(d, c, dl, r) .fx = {(d) + 1, (c) + 1, (dl) + 1, (r) + 1}
#define PAT(n) .pat = (n)

struct track;
typedef struct {                 /* an engine (engines.c ENGINES[]; the eng_*.c files) */
    const char *name;            /* "ANALOG" (PRESETS, the editor) */
    const char *page_title[2];   /* EDIT 1 and EDIT 2 */
    param_desc_t edit[8];        /* P_E0..P_E7 */
    const preset_t *presets;
    uint8_t npresets;
    uint8_t knob[4];             /* HOME: the four parameters on KNOB 1..4 */
    uint8_t poly;                /* voice cap for POLY and UNISON, 0 = NVOICE */
    uint8_t sampled;             /* 1 = plays recorded material (a position, not a phase): voice.c keeps
                                  * no phases over a retrigger, spreads none for UNISON, renders at SUS 0 */
    uint8_t keep;                /* bit k: s[k] is kept when a sounding voice is retriggered (filters) */
    uint8_t oneshot;             /* 1 = hits (DRUM): always POLY, no glide; amp ends its voices (eng_drum.c) */
    void (*note_on)(struct track *t, voice_t *v);
    void (*render)(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m);
    /* optional (0 = none): the voice amplitude instead of the ADSR curve, once per control tick;
     * gets the ADSR value (Q15, env_tick already ran: it still gates the voice), returns Q15 */
    int32_t (*amp)(struct track *t, voice_t *v, int32_t adsr);
    /* optional: a mode-dependent descriptor of EDIT k (the same range and default as edit[k],
     * another label / value names), 0 = edit[k] */
    const param_desc_t *(*desc)(const struct track *t, uint32_t k);
    /* optional: once per block and part, before its voices (also with no voice sounding) */
    void (*block)(struct track *t);
    /* optional: the note of key k (0 = the lowest) when the engine maps the keys itself, else -1
     * (then the scale keyboard of seq.c kb_map) */
    int32_t (*keys)(const struct track *t, uint32_t k);
    /* 1 = the engine's own envelopes are the voice's amplitude (FM6): voice.c renders it at 1.0 (no ADSR, no
     * velocity; LFO -> AMP, the matrix's AMP and the fades still apply; ENV is 0) and the voice ends when
     * done() says so (once per control tick, before the render), not at the end of the ADSR's release */
    uint8_t ownenv;
    int (*done)(struct track *t, voice_t *v);
    /* bytes of the part's engine memory (engines.c eng_mem, at most ENG_MEM_PART) the engine keeps its state
     * in, 0 = none: a part plays one engine at a time, so they share one region; on a switch to the engine
     * (after the old one's fade) the region is zeroed when another engine had it (eng_mem_claim) */
    uint16_t mem;
    uint8_t atgt;                /* ARP TARGET's units (SPEC 2.3, params.c atgt_bits): ATG_*, 0 = none (ALL only) */
    uint8_t ufilt;               /* DUELL 0.2.2: 1 = the engine has no filter of its own: the cutoff routes (ENV / LFO FLT, the matrix's CUT) and the FILTER page's
                                  * CUT RES MODE SLOPE (P_FREEA..D) drive the per-voice multimode filter (0.2.9; voice.c uf_run) and no longer the engine's tone control.
                                  * 2 (ANALOG, TRIO, NOISE; DUELL 0.2.9 "FILTER"): the engine keeps its own filter and its routes (CC74 / CC71, FLT depths, the
                                  * matrix's CUT) exactly as they are; the multimode filter runs IN SERIES after it, on CUT RES MODE SLOPE alone (the matrix's FRES
                                  * moves its RES), bypassed by default (MODE LP, CUT open: bit for bit as without it) */
} engine_t;
enum { ATG_NONE, ATG_ANALOG, ATG_TRIO, ATG_FM6, ATG_KINDS };

/* ------------------------------------------------- tracks, the song --- */
enum { ST_NOTE, ST_TIE, ST_REST };
#define SF_ACCENT 1u
#define SF_SLIDE 2u
#define SF_RATCH_SH 3u                   /* RATCH: the hits of a NOTE step - 1 (0..3: x1..x4, seq.c seq_ratchet); */
#define SF_RATCH (3u << SF_RATCH_SH)     /* bits 3..4, so a user preset's pattern (flag 4 = tie) carries it too */
#define NLANE 8                  /* drum lanes of a step (the DRUM engine's: eng_drum.c DRUM_LANE_NOTE) */
typedef struct {                 /* acid-style step: up to 4 notes (POLY), time, accent, slide; drum hits */
    uint8_t note[4];
    uint8_t n;                   /* notes in use, 0 = empty */
    uint8_t time;                /* ST_NOTE / ST_TIE / ST_REST */
    uint8_t flags;               /* SF_ACCENT | SF_SLIDE | SF_RATCH */
    uint8_t vel;
    uint8_t hit;                 /* bit l: lane l hits (its GM note, on any engine): the DRUM grid */
    uint8_t acc;                 /* bit l: that hit is accented (velocity 127) */
    uint8_t probability;         /* 0 = legacy 100%; 1..100 = percent, 101 = silent */
} step_t;
typedef struct {                 /* a step as formats 1..4 (projects to FUN4) stored it: no hits */
    uint8_t note[4];
    uint8_t n, time, flags, vel;
} step8_t;

/* Probability keeps zero-initialized and legacy patterns at 100%. */
static uint32_t step_chance(const step_t *s) { return !s->probability ? 100u : s->probability <= 100u ? s->probability : 0u; }
static void step_set_chance(step_t *s, uint32_t chance) { s->probability = (uint8_t)(chance >= 100u ? 0u : chance ? chance : 101u); }
/* RATCH: a NOTE step plays its notes and hits this many times (1..4), in equal parts of the step; every older
 * pattern holds 0 there, x1 */
static uint32_t step_ratchet(const step_t *s) { return ((s->flags & SF_RATCH) >> SF_RATCH_SH) + 1u; }
static void step_set_ratchet(step_t *s, uint32_t hits)
{
    s->flags = (uint8_t)((s->flags & ~SF_RATCH) | ((hits < 1u ? 0u : hits > 4u ? 3u : hits - 1u) << SF_RATCH_SH));
}
/* NUDGE (1.2, #162, D98): a step's timing, in 1/NUDGE_DIV of the track's DIV, -8..+7 (half a step early .. 7/16 late);
 * live recording keeps it, SEQ > AUTOMATION's NUDGE rows and STEP (steps held + PRESETS) set it, the track's QUANTIZE
 * OFF (P_SQNT, AUTOMATION's QUANTIZE row) plays it (seq.c seq_tick).
 * Four bits of the flags no other field uses: 4 (bit 3 of the nudge, its sign) and 32 64 128 (bits 0..2). 0 = on the
 * step: every older pattern. A user preset's pattern keeps none (upreset.c up_pat_norm) */
#define NUDGE_DIV 16
#define SF_NUDGE (4u | 0xE0u)
static int32_t step_nudge(const step_t *s)
{
    uint32_t u = (uint32_t)(s->flags >> 5) | (uint32_t)(s->flags & 4u) << 1;
    return (int32_t)(u ^ 8u) - 8;
}
static void step_set_nudge(step_t *s, int32_t v)
{
    uint32_t u = (uint32_t)(v < -8 ? -8 : v > 7 ? 7 : v) & 15u;
    s->flags = (uint8_t)((s->flags & ~SF_NUDGE) | (u & 7u) << 5 | (u >> 1 & 4u));
}
#define MOTION_MAX 128u          /* 1.2 (FUN10): 64 before */
/* Four tracks x64 steps fit one byte. Values retain their signed parameter range (-64..127: an int8_t, 1.2).
 * param: the P_* id (< 128: P_COUNT is at most 127, project.c), bit 7 (MOTION_LOCK, 1.1) a parameter lock: the value
 * sounds on that step only and goes back after it (motion.c motion_step); without it an automation event, the
 * value holds until another one changes it. One record per (track, step, id), of either kind */
#define MOTION_LOCK 0x80u
#define MOTION_ID(e) ((uint32_t)(e)->param & 0x7Fu)
typedef struct { uint8_t place, param; int8_t value; } motion_event_t;
typedef struct { uint8_t count, on, rsv[2]; motion_event_t event[MOTION_MAX]; } motion_store_t;
_Static_assert(sizeof(motion_store_t) == 388u, "motion disk layout (FUN10)");
#define PS_SLOTS 16u             /* DUELL: the pattern slots P1..P16 (patslot.c, SAVE > PATTERNS) */
#define MU_SLOTS 16u             /* DUELL: the multis M01..M16 (project.c multi.c, SAVE > MULTI) */
#define CHAIN_ROWS 16u
/* A song section (1.2, FUN10; a stored format only: DUELL has no song mode, project.c writes an empty chain and ignores
 * the one it reads): the PROJECT slot each track plays (0..3, A..D; CHAIN_SILENT "-": that track plays
 * nothing there) and its repeats (1..16). Up to 1.1 a row was {slot, repeat}, all four tracks on that slot
 * (chain_v9_t, FUN6..FUN9): it loads as a section with the four on it (project.c chain_from_v9) */
#define CHAIN_SILENT 4u
typedef struct { uint8_t slot[NTRK], repeat; } chain_row_t;
typedef struct {
    uint8_t count, rsv[3];
    chain_row_t row[CHAIN_ROWS];
} chain_config_t;
typedef struct { uint8_t slot, repeat; } chain_row_v9_t;   /* frozen: the rows of FUN6..FUN9 */
typedef struct {
    uint8_t count, rsv[3];
    chain_row_v9_t row[CHAIN_ROWS];
} chain_v9_t;
_Static_assert(sizeof(chain_config_t) == 84u && sizeof(chain_v9_t) == 36u, "song chain layouts (FUN10 / FUN6..FUN9)");

typedef struct track {
    int16_t p[P_COUNT];
    uint8_t engine, preset;      /* engine: what the audio ISR renders */
    uint8_t eng_req;             /* engine the UI asked for (the ISR switches at a block start) */
    uint8_t user;                /* user preset slot + 1 the sound came from (UI), 0 = none */
    voice_t v[NVOICE];
    /* LFO 1: its fields by their names (lfo_ph ..), or as an lfo_t (lfo1) for voice.c lfo_tick, which LFO 2 shares */
    union {
        struct {
            uint32_t lfo_ph;
            int32_t lfo_val;     /* Q15 */
            int32_t lfo_fade;    /* Q15 ramp after note-on */
            uint32_t lfo_rnd;
            int16_t lfo_prev;    /* the S&H value of the cycle before (mod.c SLEW glides from it to lfo_rnd's) */
        };
        lfo_t lfo1;
    };
    lfo_t lfo2;                  /* DUELL: LFO 2 (ticked only while a matrix slot reads it: mod.c MS_LFO2) */
    /* keyboard / arp input: held notes in press order */
    uint8_t held[16];
    uint8_t nheld;
    uint8_t arp_phys;            /* keys physically held for the arp */
    uint8_t arp_ph;              /* the arp has played a note since the keys were pressed: its next steps are the same phrase (voice.c: the LFO's FADE, and its phase with TRIG NOTE (the option; the default is FREE), restart once, not every step) */
    /* arp runtime */
    uint32_t arp_pos;            /* q8 samples into the current arp step */
    uint32_t arp_idx;
    uint8_t arp_note;            /* sounding arp note, 0 = none */
    uint8_t arp_walk;            /* WALK: the place in the note list it stands on */
    uint8_t arp_ch[3];           /* CHORD: the other notes sounding with arp_note, 0 = none */
    /* ARP TARGET (SPEC 2.3, seq.c atg_fire): with units masked the arp moves the held voices' masked units (no arp_note
     * sounds then: arp_off counts the step's gate down) */
    uint8_t atg_note;            /* the arp's note now, 0 = none yet (the units on the root) */
    uint8_t atg_vel;             /* its gate: the step's velocity (accent 127), 0 = off (a rest, the gate over) */
    uint8_t atg_lat;             /* HOLD: keys let go whose root voices still sound (latched) */
    uint32_t arp_off;            /* q8 sample time of its note-off (ARP TARGET: of the step's gate) */
    /* sequencer */
    step_t step[NSTEP];
    uint32_t seq_pos;            /* q8 samples into the current step */
    uint16_t seq_idx;
    uint8_t seq_notes[4 + NLANE];   /* sounding seq notes (the step's notes, then its hits) */
    uint8_t seq_n;
    uint8_t seq_hold;            /* last step slides: keep the notes until the next step */
    uint8_t slide_glide;         /* next legato note glides (slide) */
    uint8_t rat_left;            /* RATCH: repeats of the playing step still to come (seq.c seq_ratchet) */
    uint32_t seq_off;
    uint8_t seq_active;          /* any step programmed */
    uint8_t rskip_idx;           /* live recording put notes into the step about to play: */
    uint8_t rskip_n, rskip[NLANE];   /* do not trigger them again there (they sound already) */
    /* NUDGE (QUANTIZE OFF, seq.c seq_tick): a step plays at its start + its nudge. Latched as a step starts: */
    uint32_t nd_late;            /* the step playing now plays at this seq_pos + 1 (0: it played) */
    uint32_t nd_early;           /* the next step plays early, at this seq_pos + 1 of this one (0: on its start) */
    uint8_t nd_fired;            /* that step + 1 once it played early (its start then plays nothing), 0 = none */
    /* live recording of held notes (seq.c rec_hold): the steps they are held into become TIEs */
    uint8_t rh_n, rh_note[4];    /* recorded notes still held, 0 = none */
    uint8_t rh_start;            /* the step they were recorded into */
    uint8_t rh_ties;             /* TIE steps written after it */
    uint8_t rh_last;             /* the last of them; rh_bak: what it held (an early release puts it back) */
    step_t rh_bak;
    /* mono */
    uint8_t mono_stack[8];
    uint8_t nmono;
    uint8_t mono_note;           /* note the MONO / LEGATO / UNISON voice(s) play, 0 = none */
    uint8_t rr;                  /* POLY ROTATE: next voice to try */
    /* mix runtime */
    int32_t peak;
    int32_t dist_hp, dist_lp1, dist_lp2;   /* DIST insert state (fx.c) */
    uint8_t tail;                /* blocks to mix after the last voice (the DIST / INSERT tail) */
    uint8_t ins_run;             /* the INSERT still sounds (fx.c track_insert: its wet share above 0) */
    uint8_t mf;                  /* DUELL 4b: the part's MORPH fader as this block has it (morph.c morph_begin; the matrix's
                                  * MORPH source, mod.c), 0..127 */
    int16_t armp, aholdp;        /* P_AMODE / P_AHOLD as last seen by the ISR */
    /* engine switch (voice.c engine_block): the old engine's voices fade out, then it switches */
    uint8_t xf_on, xf;           /* fading; blocks of the fade still to render */
    int16_t pe_old[8];           /* P_E0..P_E7 of the sounding engine: the fade renders with these */
    uint8_t xp_n, xp_note[4], xp_vel[4];   /* note-ons during the fade, played on the new engine */
    /* modulation matrix (mod.c): MIDI performance controllers of the track's channel, 0..127 */
    uint8_t mw, at;              /* CC1 mod wheel, channel aftertouch */
    uint8_t ex_off;              /* 127 - CC11 expression (0: full, the MIDI default) */
    uint8_t m_vel, m_key, m_vi;  /* the latest note-on: velocity, note, voice index (per-block destinations) */
    uint8_t sp_alt;              /* SPREAD: the side the next voice starts on (alternating, voice.c voice_start) */
    int16_t m_rnd;               /* .. its RAND */
    int32_t m_env;               /* the amp envelope of voice m_vi, last block (Q15) */
    int32_t m_env2;              /* .. its ENV 2 (DUELL), last block (Q15) */
    uint16_t lay_xg;             /* LAYER XFADE (core.h mix_xf): the gain (Q15) the next voices of this part start with, 0 = none (unity) */
    uint8_t rsv_sz[4];           /* (sizeof(track_t) a multiple of 16: below; DUELL: 15 before ENV 2, LFO 2; 4b: 7 before mf; EQ: 4 less for P_COUNT 113; DUCK: 4 more for 119; DUCK 3: 6 more for 122, the old pad of 6 gone and 4 added; ENV 2 / LFO 2 DEST: 2 more for 127, the p[] growth padded to 4) */
} track_t;
/* the audio ISR indexes trk[] with a multiply by a small immediate and a shift while the size is a multiple of 16; 1.5's
 * new track parameters (the INSERT, the filter TYPE, ENV SYNC) and state would make it no multiple (the constant
 * loaded first: +3 % on the ISR's static cost, target_budget.py): one pad, rsv_sz, keeps it */
_Static_assert(P_EQLG == PL_ED_FX && P_M5SRC == PL_FM1_ATK && P_E2ATK == P_M8AMT + 1 && P_L2FADE == PL_FM4_LEVEL &&
               P_L2SYNC == PL_LN0 && P_EQMQ == PL_LN7 && P_EQMQ + 1 == P_LSYNC && P_ESYNC + 3 == P_DKCRV && P_DKRLS + 1 == P_DKSRC && P_DKTHR + 1 == P_E2FLT && P_L2SHP + 1 == P_E0 && P_E0 == 119 && P_COUNT == 127,
               "DUELL: the retired ids (PL_*) and the layout");
/* an LFO's ids: RATE WAVE PHS FADE in a row, SYNC TRIG POL in a row (voice.c lfo_tick reads them from the first) */
_Static_assert(P_LWAVE == P_LRATE + 1 && P_LFADE == P_LRATE + 3 && P_LPOL == P_LSYNC + 2 && P_L2WAVE == P_L2RATE + 1 &&
               P_L2FADE == P_L2RATE + 3 && P_L2POL == P_L2SYNC + 2, "LFO ids in a row");
_Static_assert(sizeof(track_t) % 16u == 0, "track_t: a multiple of 16 bytes (a cheap trk[] index on the target)");

typedef struct {
    int16_t g[G_COUNT];
    uint8_t playing, seq_mode;
    uint8_t grid;                /* 1: the keys are the DRUM grid (ui.c grid_on): only the lane keys play (seq.c);
                                  * 2: NAME (ui_name.c): no key plays */
    uint8_t rec;                 /* live recording armed: bit per track */
    uint8_t sel;                 /* selected track 0..NTRK-1: keys, pages, editor */
    int8_t octave;
    uint32_t tick;               /* sub-blocks since play */
    uint32_t cpu_q8;             /* audio ISR load, 1/256 */
    uint32_t master_q12;
    int32_t batt_raw;            /* smoothed ADC ch3 (battery divider), 0 = not read yet */
} song_t;

static track_t trk[NTRK];        /* the instrument: four parts */
static song_t song;
/* the CPU load meter's counters (audio.c writes them per I2S half, ui_menu.c cpu_meter_poll reads them) and the half's time */
#define CPU_BUDGET_US (HALF_FRAMES * 1000000u / FS)
static volatile uint32_t cpu_sum, cpu_n, cpu_pk, cpu_miss;
#ifndef NLIVE
#define NLIVE NSHOWN             /* parts that render and own engine / insert / wheel / motion state (RAM is sized by it); a test rig that
                                  * plays all four parts builds with -DNLIVE=4 (host_tests.sh regress) */
#endif
#define TRK_HIDDEN(t) ((uint32_t)((t) - trk) >= NLIVE)   /* DUELL: tracks 3..4 never render, sound or own engine memory */
#define TSEL (&trk[song.sel])    /* the selected track */

/* ONE GLOBAL SCALE (DUELL; owner design 2026-10-12, replaces SCALE LINK): SCALE and ROOT are settings of the instrument, not of a patch,
 * a part, a MULTI or a factory sound: scale_g (0 = CHROMATIC, the default; an unknown value reads as it) and root_g (0 = C), and the
 * OFF-SCALE KEYS setting offscale_g: OS_SNAP (the default) moves a key or MIDI note outside the scale to the NEAREST scale note (a tie
 * goes DOWN) before it reaches the parts (seq.c note_snap); OS_PLAY lets it pass. Chromatic: nothing to snap. The per-part P_SCALE /
 * P_ROOT / P_QUANT ids stay reserved (patch, MULTI and editor formats do not change) but nothing reads them (a MULTI or an old patch
 * record that holds a scale is read and discarded). Kept with the settings,
 * favorites.factory[15][3] (bit 7 the format flag, bit 6 PLAY, bits 0-3 the scale) and [4] (the root): ui.c ui_scale_set / scale_sync;
 * a zero byte (a new unit, any older blob) is CHROMATIC, C, SNAP. EVERY consumer reads scale_of / root_of (or scale_mask, note_snap:
 * seq.c); the UI edits scale_g / root_g / offscale_g only (ui_input.c scale_knob, the SCL quick layer). A note already sounding keeps
 * its pitch (every source remembers the notes it started) */
enum { OS_SNAP, OS_PLAY, OS_N };
static volatile int16_t scale_g, root_g;               /* the global SCALE (0 = CHROMATIC) and ROOT (0 = C) */
static volatile uint8_t scale_gen;                     /* bumped when SCALE / ROOT / OFF-SCALE change (ui.c scale_sync); seq.c keyboard_block rebuilds the arp's chords */
static volatile uint8_t offscale_g;                    /* OS_SNAP / OS_PLAY */
static int32_t scale_of(const track_t *t) { (void)t; return scale_g; }
static int32_t root_of(const track_t *t) { (void)t; return root_g; }

/* SWING of a track's step clock: the track's own plus the global one, at most 100 (#31). The sequencer
 * (seq.c step_samples) times steps with it. At 100 the even
 * steps are 1.4 x the straight length and the odd ones 0.6 x. */
#define SWING_MAX 100
static inline int32_t track_swing(const track_t *t)
{
    int32_t sw = t->p[P_SSWING] + song.g[G_SWING];
    return sw < 0 ? 0 : sw > SWING_MAX ? SWING_MAX : sw;
}
/* the length of step idx of a straight length `base`, swung: even steps longer, odd ones shorter */
static inline uint32_t swing_step_len(const track_t *t, uint32_t base, uint32_t idx)
{
    int32_t sw = track_swing(t) * (int32_t)base / 250;
    return base + (uint32_t)((idx & 1u) ? -sw : sw);
}

/* ----------------------------------------------------------- system --- */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* slot store before the index update */
static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR in main.c) */
/* boot-loop guard (main.c): two boots in a row that die in the first 30 s -> UBOOT */
#define BOOTGUARD_MAGIC 0x42475244u
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));
