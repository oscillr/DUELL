/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA core types: tracks, voices, engines, parameters.
 * Four tracks, each a synth part: its own engine, preset, parameters, voices and 64-step
 * pattern. The parts share one budget of NVOICE sounding voices (voice.c). DUELL 0.2.9 retired the SAMPLE, GRAIN and SLICE
 * engines, 0.2.14 the hidden DIGITAL and DRUM ones (ENGI_RETIRED below).
 * Sections: sizes, parameters, voices and engines, tracks and the song, system. */
#include <stdint.h>
#include "ext.h"            /* DUELL cloud plan wave 0 (docs/design/CLOUD_FEAT_PLAN.md s3): NX, XV(t,k), the slot names; constants part */

/* ------------------------------------------------------------ sizes --- */
#define NVOICE 8                 /* voices per part, and the budget shared by all parts */
#ifndef NPART
#define NPART 2                  /* synth parts: tracks 1..2 (DUELL: parts A and B). The hidden parts 3 and 4 are gone (memory trim 1, 7 KB of RAM);
                                  * only a host test rig builds with -DNPART=4 (host_tests.sh regress: four parts play, the shown ones stay 2) */
#endif
#ifndef NSHOWN
#define NSHOWN 2                 /* DUELL: the parts with a screen, keys, steps, arp, voice and MIDI channel: A and B */
#endif
_Static_assert(NSHOWN <= NPART, "core.h: NSHOWN parts are shown, NPART parts exist");
#define NTRK NPART               /* tracks (the formats and the protocol count these): every track is a part */
#define NSTEP 64
#ifndef DUELL_TEST_MODE
#define DUELL_TEST_MODE 0        /* DUELL developer switch (-DDUELL_TEST_MODE=1; env DUELL_TEST_MODE=1 for build.py / web/emu/build.sh / host_tests.sh): 1 builds in MENU > SYSTEM > TEST (the CPU test screen)
                                  * (tools/gen_duell_multis.py must be run with the same switch; since 0.2.23 both builds hold the same sixteen factory multis); 0 (the release default) leaves it out */
#endif
#ifndef DUELL_BENCH
#define DUELL_BENCH 0            /* DUELL developer switch (-DDUELL_BENCH=1; env DUELL_BENCH=1 + BENCH_TAG for tools/build.py): 1 builds in MENU > SYSTEM > BENCH, the
                                  * stage-timing bench (src/bench.c, docs/BENCH_GUIDE.md). 0 (every normal build) leaves all of it out: not a byte of it is compiled */
#endif
#if defined(DUELL_CORE1_PROBE) && DUELL_CORE1_PROBE
#error "DUELL_CORE1_PROBE (BENCH-B's synthetic core 1 rows) is not in this line: two-core stage 1 brings core 1 (twocore.c)"
#endif
#define HALF_FRAMES 128          /* I2S half buffer: 2.9 ms at 44.1 kHz (a key waits 0..1 half, then plays 1 half later) */
#define NENGINES 15              /* DUELL: WAVE (14) appended after SLICE (13); the engine numbers never move (the stores and the
                                  * editor protocol hold them: append, never renumber) */
#define ENGI_WAVE 14u            /* DUELL: the wavetable engine (eng_wave.c) */
/* Retired engine numbers. The engine numbers never move (the stores and the editor protocol hold them), so a retired engine keeps its
 * number as a reserved one: DIGITAL (1, replaced by FM6), SAMPLE (4), GRAIN (8), the DRUM kit (10) and SLICE (13) (DUELL 0.2.9:
 * "REMOVE the sample engines", WAVE replaces them; DUELL 0.2.14: the hidden DIGITAL and DRUM engines are gone from the code too, no
 * conversion of their sounds). They have no code and no presets (engines.c: the silent placeholder "-"), are never selectable
 * (eng_ok) and are in no list. A stored sound / part / patch / multi / project that names one loads as it was stored (every value
 * kept, so a save writes it back unchanged) and plays SILENT on the placeholder; the UI says NO ENGINE once (ui_input.c gone_notice)
 * and its engine shows "-" */
#define ENGI_RETIRED(e) ((e) == 1u || (e) == 4u || (e) == 8u || (e) == 10u || (e) == 13u)
#define NENG_RETIRED 5
#define NENG_SHOWN (NENGINES - NENG_RETIRED)   /* the engines one can pick: PRESETS, the EDIT layer, the editor, in the display order of engines.c ENGINE_ORDER */
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
    F_FMODE,                               /* DUELL 0.2.9: the MULTIMODE FILTER's MODE, 0 LP .. 64 BP .. 127 HP, shown "LP" "LP>BP" "BP" "BP>HP" "HP" */
    F_LOCUT,                               /* DUELL 0.2.14: the REVERB LOW CUT, 0 OFF, else its corner in Hz (LOCUT_HZ; 0.2.22: from 1000 Hz in kHz) */
    F_BMONO,                                /* DUELL 0.2.19: BASS MONO, 0 OFF, 1..BM_N the corner in Hz (BM_HZ), the number big and "Hz" as its unit (a name "100 Hz" is too wide for a card) */
    F_KTRK,                                /* DUELL 0.2.21: the multimode filter's KEY TRK, -63..+63 shown -100..+100 % (63 = 100 %: one octave of cutoff per octave played) */
    F_CTHR,                                /* DUELL 0.2.21 COMP: THRESH, 0..40 shown -40 .. 0 dB */
    F_CRAT,                                /* .. RATIO, 0..8: 1:1 1.5:1 2:1 3:1 4:1 6:1 8:1 12:1 INF */
    F_CATK,                                /* .. ATTACK, 0..18: CMP_ATK_US, shown in ms */
    F_CREL,                                /* .. RELEASE, 0..12: CMP_REL_MS, shown in ms, from 1000 in s */
    F_TUNEHZ,                              /* DUELL 0.2.22: MASTER TUNE, the value is the frequency of A4 in 0.1 Hz (TUNE_MIN .. TUNE_MAX), shown "440.0" Hz */
    F_DSEMI                                /* TWIN FILTER (ext.h, cloud job B): filter 2's OFFSET, whole dial steps shown as semitones with one decimal (a dial step is 0.1103 octave: 1.3236 st), "+1.3" "-83.4" st */
};

typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min, max, def;
    const char *const *names;   /* F_ENUM */
    const char *unit;           /* F_INT / F_ENUM optional unit */
} param_desc_t;

enum {                          /* per-track parameters */
    P_LEVEL,                    /* DUELL 0.2.19 (owner, 2026-10-15): the MIXER LEVEL, the part's volume the player sets on the mixer: NOT part of a patch (a sound load,
                                 * factory or stored, never touches it; param_kept), kept with a multi and the project, default unity (112 = 0 dB). The sound's own
                                 * loudness is P_TRIM (below); the part plays at P_TRIM x P_LEVEL (fx.c part_lvl). Through 0.2.18 this one id was both */
    P_ATK, P_DEC, P_SUS, P_REL,
    P_ED_FLT, P_ED_PIT, P_ED_SHP, P_EQLG,      /* P_EQLG: Felucca's P_ED_FX (never read), now the EQ's LOW gain (DUELL, core.h PL_*) */
    P_LRATE, P_LWAVE, P_LPHASE, P_LFADE,
    P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP,
    P_AMODE, P_ARATE, P_AOCT, P_AGATE,
    P_ASWING, P_APROB, P_AHOLD, P_AORDER,
    P_RES2, P_DRV2, P_KTRK, P_TRANS,           /* P_RES2 / P_DRV2: the TWIN FILTER's RES 2 and DRIVE (ext.h), on the retired ROOT and SCALE ids 25, 26 (the scale and root are the instrument's: scale_g, root_g, params.c SCALE_D / ROOT_D). P_KTRK (DUELL 0.2.21): the MULTIMODE FILTER's KEY TRK, on the id of the retired QNT (was P_QUANT; see KTRK below) */
    P_SLEN, P_SDIV, P_SSWING, P_SGATE,
    P_DIST, P_CHOR, P_DLY, P_REV,
    P_VOICE, P_GLIDE, P_PAN, P_MUTE,
    P_GLMODE, P_PRIO, P_TWTYPE, P_DETUNE,      /* P_TWTYPE: the TWIN FILTER's TYPE (ext.h), on the retired ALLOC id 43 (it was never read) */
    P_FREEA, P_FREEB, P_FREEC, P_FREED,        /* were the SLICER insert's SLCR PAT RATE DEPTH (removed). The MULTIMODE FILTER (DUELL 0.2.9; 0.2.2's universal
                                                * filter): P_FREEA its CUT (0..UF_STEPS, the open end the default), P_FREEB RES (0..127), P_FREEC MODE (0 LP .. 64 BP
                                                * .. 127 HP), P_FREED SLOPE (1 = 12 dB, 2 = 24 dB), read only on the engines
                                                * with engine_t.ufilt (voice.c uf_run) */
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
    P_BLEND,                                   /* the TWIN FILTER's BLEND (ext.h) on id 101, the retired ANALOG filter TYPE (1.5, #104: LP / BP / HP; every factory ANALOG sound is LP; the engine runs LP only, eng_analog.c, until its own filter goes) */
    P_ESYNC,                                   /* 1.5 (#175): ENV SYNC: ATK DEC REL as note values of the tempo (voice.c) */
    P_EQHG, P_EQHF,                            /* DUELL EQ: HIGH gain and freq (P_COUNT 113, P_E0 105: stores map by count) */
    P_DKCRV, P_CUT2, P_DKDEP, P_DKATK,         /* P_CUT2: the TWIN FILTER's CUTOFF 2 (ext.h) on id 106, the retired DUCK NOTE (0.2.24 / 0.2.25's EQ switch, then free) */
    P_TRIM,                                    /* DUELL 0.2.19 (owner, 2026-10-15): the PATCH TRIM, the sound's own loudness, stored WITH a patch (a factory sound's tuned level, a patch record, a multi's
                                                * part): loading a sound sets it, SAVE stores it, RESTORE brings the factory one. On the id of the legacy DUCK HOLD (0.2.5: on no page, never read, always 0 in
                                                * a record): P_COUNT and every format keep their size; the multi version and the project revision moved so no HOLD is read as a trim (a patch record: PTF_TR).
                                                * 0..127 as P_LEVEL (F_DB, 112 = 0 dB), default 104 (the old default level). Not morphed (morph.c pins End B to A) */
    P_DKRLS,                                   /* DUELL DUCK (SPEC 5c.4; fx.c duck_*): the sidechain (P_COUNT 119, P_E0 111). P_DKCRV: the RELEASE's CURVE (LIN, EXP); was the MIDI CH (merged into P_DKSRC) */
    P_DKSRC, P_DKRATE, P_DKTHR,                /* the trigger SOURCE (0 PUMP, 1 PART, 2 MIDI; 0.2.22: it was 2..17 CH1..16, the channel is the SIDECHAIN page's now), then KNOB 2 by SRC: PUMP's RATE, PART's THRESHOLD (MIDI: none; P_DKNOTE is retired) (P_COUNT 122, P_E0 114) */
    P_E2FLT, P_E2SHP,                          /* DUELL: ENV 2 DEST (the classic arrangement, SPEC 2.4): ENV 2's direct cutoff and shape depths, */
    P_L2PIT, P_L2FLT, P_L2SHP,                 /* LFO 2 DEST: LFO 2's pitch, cutoff, shape depths (as P_ED_* / P_LD_*; voice.c track_render) (P_COUNT 127, P_E0 119: the last ids
                                                * MIDI LEARN's codes (ML_E0 120) and the 7-bit stores allow) */
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
    P_COUNT
};

/* The retired EQ switch's name stays, now an alias of the reserved empty entry p[P_COUNT] (127, always 0): fx.c EQ_ON, expanded inside the hot mix_spread, still reads "p[P_DKNOTE] == 0", so
 * that function's source is untouched (tools/hot_fns.py: the hot block is closed) while id 106 is the TWIN FILTER's CUTOFF 2 (P_CUT2) and may hold any value without switching the EQ off */
#define P_DKNOTE P_COUNT
#define TUNE_MIN 4153           /* DUELL 0.2.22: MASTER TUNE (G_TUNE) is the frequency of A4 in 0.1 Hz: 415.3 .. 466.2 Hz (Roland's range), 510 steps, */
#define TUNE_MAX 4662           /* default 440.0 Hz; a stored value outside the range (an older project's cents) reads as the default (voice.c tune_get) */
#define TUNE_DEF 4400
enum {                          /* global parameters */
    G_BPM, G_SWING, G_CLOCK, G_TUNE,
    G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX,
    G_RSIZE, G_RDAMP, G_CRATE, G_CDEPTH,
    G_MIDI, G_SYNC, G_ROUTE, G_INFO,   /* G_ROUTE: MIDI IN, 0 PARTS (the parts' MIDI channels, GLO > MIDI; seq.c midi_pch), 1 SEL;
                                        * 2..4 (older: CH5-8 CH9-12 CH13-16) read as PARTS */
    G_SLOT, G_NAME, G_LOAD, G_SAVE,
    G_ENGSEL, G_RLOCUT,         /* G_ENGSEL: no page, the editor switches the engine with a SET of G_ENGSEL. G_RLOCUT (0.2.14, on the id of the unused
                                 * G_ENGGO, so G_COUNT and the formats do not move): the REVERB LOW CUT, K4 of the REVERB page. 0 OFF, 1..127 a one-pole
                                 * high-pass on the reverb bus's input, 20 Hz .. 450 Hz exponential (F_LOCUT; fx_bus.c rev_locut); default 61 (88 Hz).
                                 * DUELL 0.2.22 (owner, 2026-10-16): 20 Hz .. 2 kHz over the same 127 values (3.7 % a step), default 75 = 299 Hz */
    G_CLRSEQ, G_INITSND,
    G_RTYPE,                    /* REVERB TYPE: 0 ROOM, 1 SPRING, 2 HALL (1.2, the power-on default), 3 SHIM, 4 RESO (JIANT ports), 5 PLATE (0.2.14) (fx.c). Was
                                 * G_DRCH, the GM drum part's MIDI channel (inert since 1.0, never read) */
    G_PINGSPREAD,               /* DUELL: the PING-PONG's SPREAD (DLY 2), 0..100 %, stored directly, default 100 (GP def; 0 = the repeats
                                 * centred). Was G_DRLVL, the GM drum part's level (inert) */
    G_FREE26,                   /* UNUSED since 0.2.16 (stored 0, read by nothing): was the delay's PING-PONG switch (0 OFF, 1 ON; before that G_DRREV, the GM
                                 * drum part's reverb send, inert since 1.0). PING-PONG is a MODE of the delay now (G_DMODE 1). The id stays so G_COUNT
                                 * and the formats do not move */
    G_COUNT
};
/* DUELL 0.2.5 (SPEC 5c.2): the delay's own modulation (DLY 2/2, fx_bus.c dly_mod) takes two global ids that nothing read: G_SYNC (a
 * placeholder, always 0: its range was the one name "--") is its DEPTH, 0 = off (every store has 0 there); G_MIDI (an USB / TRS
 * choice nothing ever read) is its RATE, a store's old 0 or 1 is only a slow rate that is heard at DEPTH > 0. G_COUNT, the project
 * formats and the protocol's ids do not move */
/* DUELL 0.2.19 (owner request): SHIM's two settings, REVERB page 2 "SHIMMER" (shown only while TYPE is SHIM, ui.c page_visible), on the two global ids that nothing
 * reads: G_FREE26 (the old PING-PONG switch, stored 0 since 0.2.16) is the PITCH of the return, G_NAME (a card "NAME" that no page ever showed; stored 0) its MOD.
 * Both read 0 in every older store, and 0 is the old sound (PITCH UP, MOD 0: fx_bus.c rev_shim runs its plain loop, bit for bit), so G_COUNT, the
 * multi version and the project revision do not move. PITCH: 0 UP (the octave up, as ever), 1 DOWN (an octave down: the same two crossfaded grains at 0.5
 * of the write speed), 2 BOTH (the two returns, each at 1/2 and 7.9 cents sharp: fx_bus.c). MOD 0..100 %: a slow drift of the return's pitch (two sines, 0.29 and 0.47 Hz, +-25 cents at 100) */
/* DUELL 0.2.24 / 0.2.25 had an EQ ON / OFF switch on the retired DUCK NOTE id (P_DKNOTE, 106). The 0.2.25 layout round (owner) removed it: the id is free again (a stored
 * value reads 0, params.c PFREE) and the EQ follows its three gains (a band at 0 dB is not run, fx.c track_eq). Editor: one BYPASS sets the gains to 0 dB and brings them back */
/* DUELL 0.2.19 (owner, 2026-10-15): BASS MONO is a SOUND setting, the page MASTER's (GLO family, after SHIMMER; 0.2.21: it was FX's last page; no MENU row). Every global id is used, so it has the pseudo id
 * G_BM = G_COUNT: not a stored global (the project and multi formats keep G_COUNT 27 and their size), it lives in song.bm and is stored in a spare byte of the
 * project (byte 65) and of the multi record (byte 5, bits 1..4). 0 OFF, 1..BM_N the corner (fx.c BM_HZ: 40 .. 400 Hz). The page machinery (ui.c page_desc, params.c GP)
 * treats it as a global parameter; the editor reaches it as MENU setting id 33 (editor_menu.c) */
#define G_BM G_COUNT
#define BM_N 11u
#define BM_DEF 8                                         /* 0.2.24 (owner): ON at 200 Hz on a new unit (BM_HZ[7]); a DEVICE setting since 0.2.24: the settings record's byte [15][21] (ui.c ms_sync, stored + 1: 0 = the default), not the project's or a multi's */
static const uint16_t BM_HZ[BM_N] = {40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400};   /* the corners in Hz (fx.c bass_mono's table BM_C is computed for these) */
/* DUELL 0.2.21 (owner, 2026-10-16): the MASTER COMPRESSOR, the page COMP (GLO, after BASS MONO). Four more pseudo ids past the stored globals, like G_BM: G_CT THRESH (0..40 = -40 .. 0 dB),
 * G_CR RATIO (0 = 1:1, the stage is not run; 1.5 2 3 4 6 10 20 INF), G_CA ATTACK (0.1 .. 100 ms), G_CL RELEASE (20 ms .. 2 s). They live in song.cp[0..3] and are a DEVICE setting (not in a multi, not in the
 * project): packed in the settings record's retired `rsv` word (settings_persist.c; cmp_set). The tables below are the nominal times; fx.c holds the shift triples that realise them (comp_stage) */
/* DUELL 0.2.22 (owner, 2026-10-16): the sidechain's MIDI SETUP is a DEVICE setting, not part of a sound. GLO > SIDECHAIN: per part (K1 P1 CH, K2 P1 NOTE, K3 P2 CH, K4 P2 NOTE) the MIDI channel and the note
 * that start the DUCK of a part whose DUCK SRC is MIDI (fx.c duck_note, sc_midi[]). Kept in the settings record (favorites.factory[15][14..17], 0 = the default; ui.c ui_sc_set), never in a patch, a multi or the
 * project, so a load leaves them alone; editable whatever the SRC is. Defaults: channel 10, note 36 (C2, the GM kick). The note 0 is ANY (every note of the channel) */
#define SC_CH_DEF 9u
#define SC_NOTE_DEF 36u
#define G_CT (G_COUNT + 1)
#define G_CR (G_COUNT + 2)
#define G_CA (G_COUNT + 3)
#define G_CL (G_COUNT + 4)
/* 0.2.24 (owner: ATTACK and RELEASE moved in big steps): both are FINE now, 40 steps a decade on a log scale (CMP_T40: 10^(j/40) in Q16): ATTACK index 0..120 = 0.1 ms x 10^(i/40)
 * (0.1 .. 100 ms, 5.9 % a step), RELEASE 0..80 = 20 ms x 10^(i/40) (20 ms .. 2 s). The defaults land exactly where they were: 10 ms (80) and 200 ms (40). fx.c computes the
 * detector's coefficient from the time (cmp_coef), no table of settings any more. The settings word is re-tagged (0xC8: 7 bits each): a 0.2.21..0.2.23 word reads as the defaults */
#define CMP_ATK_N 121u
#define CMP_REL_N 81u
#define CMP_THR_N 41u
#define CMP_THR_DEF 23                                   /* -17 dB */
#define CMP_ATK_DEF 80                                   /* 10 ms */
#define CMP_REL_DEF 40                                   /* 200 ms */
static const uint16_t CMP_T40[40] = {                    /* 10^(j/40) x 6553.6 (Q16 / 10: so 10^(39/40) fits 16 bits) */
    6554, 6942, 7353, 7789, 8250, 8739, 9257, 9806, 10387, 11002, 11654, 12345, 13076, 13851, 14672, 15541, 16462, 17437, 18471, 19565,
    20724, 21952, 23253, 24631, 26090, 27636, 29274, 31008, 32846, 34792, 36854, 39037, 41350, 43801, 46396, 49145, 52057, 55142, 58409, 61870};
static inline uint32_t cmp_t40(uint32_t base, uint32_t i)   /* base x 10^(i/40), rounded (base: the time at index 0) */
{
    uint32_t dec = i / 40u, j = i % 40u, v = CMP_T40[j];
    while (dec--)
        v *= 10u;
    return (uint32_t)(((uint64_t)base * v * 10u + 32768u) >> 16);
}
#define CMP_ATK_US(i) cmp_t40(100u, (i))                 /* ATTACK index -> microseconds */
#define CMP_REL_MS(i) cmp_t40(20u, (i))                  /* RELEASE index -> milliseconds */
static uint32_t cmp_set;                                 /* the settings record's word for the compressor (settings_persist.c): 0xC8 << 24 | thr | ratio << 6 | atk << 10 | rel << 17; no tag = the defaults */
/* DUELL 0.2.23 (owner, 2026-10-16): the master WIDTH, its own GLO page, a DEVICE setting like COMP (not in a patch, a multi or the project; a load never touches it):
 * G_WD the page WIDTH's K1 WIDTH (0..200 %, 100 = as it is: the stage is not run; fx.c width_stage). Pseudo id past COMP's; it lives in song.ms[0] and is stored in the settings record's
 * bytes favorites.factory[15][18..21] (ui.c ui_ms_set: a tag byte, then WIDTH; bytes of the FX layer's old key map, retired in 0.2.2: an untagged record = the defaults). It does not run
 * in DUEL (its sides are the two parts). 0.2.25 (owner): the master saturator of 0.2.23 / 0.2.24 (G_SD, the DRIVE) is removed; its stored byte [20] is ignored */
#define G_WD (G_COUNT + 5)
#define WD_DEF 100
#define G_SHPITCH G_FREE26
#define G_SHMOD G_NAME
enum { SHP_UP, SHP_DOWN, SHP_N };          /* 0.2.25 (owner): BOTH (2) is gone; a stored 2 (or any value out of range) reads UP (project.c, fx_bus.c rev_shimx) */
#define G_DMDEP G_SYNC
#define G_DMRATE G_MIDI
/* DUELL 0.2.16 (owner, 2026-10-15): the DELAY's MODE, the first delay setting (DELAY 1/2 K1), on the id of the inert G_INFO (a "CPU" reading nothing wrote or read
 * since 1.2): 0 DIGITAL (the plain delay: clean repeats, low-passed feedback (COLR), the optional modulation; the old delay with PING off), 1 PING-PONG
 * (the repeats alternate left and right, the modulation too; the old PING ON), 2 RANDOM, 3 OCT UP (clock-synced events on the repeats, 0.2.14's RAND and OCT+);
 * a value past that plays as DIGITAL. There is no mode called DELAY: all are delays. (0.2.14/0.2.15 stored 0 DELAY / 1 RAND / 2 OCT+ here and the
 * PING-PONG switch on G_FREE26: those records are refused, MU_VER 4 / PROJ_REV 2.) In RANDOM and OCT UP the DELAY 2/2 card MDEPTH (G_DMDEP) is CHANCE, the share of
 * clock edges that fire an event, and RATE is faded (params.c dly_mode_why); SPREAD (G_PINGSPREAD) sets the width of PING-PONG and of the events and is
 * faded in DIGITAL. Only in STEREO and LAYER (SPLIT keeps the plain delay) */
#define G_DMODE G_INFO
enum { DMD_DIGITAL, DMD_PING, DMD_RAND, DMD_OCTUP, DMD_N };
/* The CHANCE a switch of MODE into RANDOM / OCT UP starts with when the stored DEPTH / CHANCE is 0 (owner, 2026-10-15: "a reasonable level when switching modes"). The CHOMPI
 * TEMPO firmware has no such number: its randomness knob starts at 0 (granularDelay.h alt_control_ = 0) and an event fires on 0.5 x that share of the clock edges
 * (at most 50 %); 40 % here is 4 edges in 10. Shown in the header ("CHANCE 40 %"). Going back to a plain mode takes it away again if the knob was not touched meanwhile */
#define DMD_CHANCE_DEF 40

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
 * OP LEVEL (61..80; DIGITAL, retired) and DRUM's lane levels (83..90; DRUM, retired; both engines' code is gone since 0.2.14). DUELL
 * puts its own parameters there (matrix slots 5..8, ENV 2, LFO 2; the rest P_FREE*), so no 7-bit id (motion records, MIDI LEARN
 * codes, the editor protocol, user preset records) had to grow. Stores of Felucca's layout are no longer read (0.2.14), so these names
 * are only the layout's check (the _Static_assert at track_t) and the tests' */
#define PL_ED_FX 8u
#define PL_FM1_ATK 61u                 /* + 5 x op (0..3) + ATK DEC SUS REL LEVEL (0..4) */
#define PL_FM4_LEVEL 80u
#define PL_LN0 83u                     /* DRUM's lane levels KICK .. BELL */
#define PL_LN7 90u

/* DUELL 0.2.9 MULTIMODE FILTER (SPEC 2.5, voice.c uf_run). CUT (P_FREEA) is a dial of UF_STEPS steps, each the old universal filter's
 * step (1.55 steps of the cutoff table, 30 Hz .. 16 kHz over 127): step d is table index (40 + 396 d) / 256, so 0 is 30 Hz and
 * UF_STEPS 16 kHz (the open end). SLOPE (P_FREED) is 1 or 2 (0.2.14: a store of before 0.2.9 that held 0.2.2's bipolar CUT is no longer converted) */
#define UF_STEPS 82
#define UF_HP 127                      /* MODE: 0 LP, 64 BP, 127 HP */
/* DUELL 0.2.21 KEY TRK (P_KTRK, voice.c track_render): the multimode filter's cutoff follows the played note, around C3 (MIDI 60, pitch16 960: the
 * engine's own ANALOG KTR centre). -63..+63 shown -100..+100 %; +63 = one octave of cutoff per octave of pitch. One dial step is 0.1103 octave
 * (1.547 table steps of 14.02 a octave), so an octave is 2320.1 in the Q8 dial and +100 % moves it 12.084 a 1/16 semitone: per unit of KTRK
 * 0.19181, taken as 785 / 4096 (0.19165, 0.08 % short: 0.004 octave over five octaves). 0 adds nothing (the paths stay as they were, bit for bit) */
#define UF_KT(k, pc) ((((k) * ((pc) - 60 * 16)) * 785) >> 12)
/* TWIN FILTER OFFSET (1.0, owner 2026-10-21): the stored value of CUTOFF 2 with LINK on is WHOLE SEMITONES, -TW_OFF_MAX..+TW_OFF_MAX (3 octaves; a stored value beyond clamps). A semitone is
 * 127 / (12 log2(16000 / 30)) = 1.16828 table steps = 0.755252 dial steps = 193.3446 in the Q8 dial (49496 / 256 = 193.34375, 0.0009 short: 0.03 of a Q8 unit at 36 semitones).
 * TW_OFF_Q8: semitones -> Q8 dial (the filter); TW_OFF_STEPS / TW_STEPS_OFF: semitones <-> whole dial steps, rounded (LINK on / off, MORPH, the graphs) */
#define TW_OFF_MAX 36
#define TW_OFF_Q8(st) ((st) < 0 ? -(((-(st)) * 49496 + 128) >> 8) : (((st) * 49496 + 128) >> 8))
#define TW_OFF_STEPS(st) (((st) * 3093 + ((st) < 0 ? -2048 : 2048)) / 4096)
#define TW_STEPS_OFF(d) (((d) * 5423 + ((d) < 0 ? -2048 : 2048)) / 4096)

/* engine indices the stores name (engines.c ENGINES[]: append-only) */
#define ENGI_PHYS 9u

/* ------------------------------------------------- voices, engines --- */
enum { V_POLY, V_MONO, V_LEGATO, V_UNISON };   /* P_VOICE */
typedef struct {
    uint8_t note, vel, gate, active;
    uint8_t stage;               /* env: 0 off, 1 attack, 2 decay/sustain, 3 release */
    uint8_t side;                /* SPREAD (fx.c mix_spread): 0 left of PAN, 1 right; set as the voice starts */
    uint8_t stage2;              /* ENV 2 (DUELL, voice.c env2_tick): 0 off, 1 attack, 2 decay/sustain, 3 release */
    uint8_t chn;                 /* the voice chain (cloud job B, voice_chain.h): where the voice was last block, VCH_OFF / VCH_RUN, VCH_FRESH for a note that has not rendered, VCH_NEW seen by the stage hooks on a first block (in the pad byte before env: voice_t keeps its size) */
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
    uint8_t uft;                 /* .. how it last ran: 0 not (from rest next time), 1 12 dB, 2 24 dB, 3 bypassed while sounding (voice.c uf_edge) */
    int32_t apitch;              /* ARP TARGET (SPEC 2.3, voice.c atg_voice): the arp pitch of the masked units, 1/16
                                  * semitone, with glide */
    int32_t again;               /* .. their level (the arp's gate and accent), Q15, smoothed */
    uint32_t age;
    uint16_t xg;                 /* LAYER XFADE: this note's gain, Q15 (voice_start from track_t lay_xg), 0 = unity: the sound untouched */
    uint8_t mvel;                /* mod.c: the note-on velocity (before UNISON scaling) and RAND of the note */
    int16_t mrnd;
    int32_t lastc, lastd, offc;       /* hunt3 (voice.c voice_step): this voice's contribution to the last sample of its last block, and the change from the sample before (lastd); the step a re-strike made and still has to cancel */
    uint8_t offn, offp;          /* .. the samples of that step's decay left; 1: voice_start asks for the step to be measured in the next block */
    uint16_t msl;                /* MORPH (voice.c uf_run): the multimode filter's SLOPE cross-fade of the part, weight Q15 | 0x8000 when the other end is 24 dB; 0 = none */
} voice_t;
enum { VCH_OFF, VCH_NEW, VCH_RUN, VCH_FRESH };   /* voice_t.chn (voice_chain.h): last block on the plain path / the first block in the chain (stages start from rest) / in the chain / a note that has not rendered yet */

typedef struct {                 /* an LFO's state (voice.c lfo_tick): track_t's LFO 1 (its lfo_* fields) and LFO 2 */
    uint32_t ph;
    int32_t val;                 /* Q15, POL applied */
    int32_t fade;                /* Q15 ramp after a fresh phrase */
    uint32_t rnd;                /* the S&H value of this cycle (0: none yet) */
    int16_t prev;                /* .. of the cycle before */
} lfo_t;

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
    int32_t ucut;                /* filter 1's effective cutoff for this voice and block, Q8 dial steps (voice.c; UF_TOP when it is bypassed or the engine has none): NOISE's loudness make-up reads it (cloud job J) */
} vmod_t;

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
    /* 1 = the engine's own envelopes are the voice's amplitude (FM6): voice.c renders it at 1.0 (no ADSR, no
     * velocity; LFO -> AMP, the matrix's AMP and the fades still apply; ENV is 0) and the voice ends when
     * done() says so (once per control tick, before the render), not at the end of the ADSR's release */
    uint8_t ownenv;
    int (*done)(struct track *t, voice_t *v);
    /* bytes of the part's engine memory (engines.c eng_mem, at most ENG_MEM_PART) the engine keeps its state
     * in, 0 = none: a part plays one engine at a time, so they share one region; on a switch to the engine
     * (after the old one's fade) the region is zeroed when another engine had it (eng_mem_claim) */
    uint16_t mem;
    const uint8_t *uiord;        /* DUELL 0.2.18: the order the EDIT 1 / EDIT 2 cards show the eight E slots in (card position -> E index; 0 = E1..E8 in order, the stored ids never move):
                                  * FM6 shows SLOT first (params.c pg_id) */
    uint8_t atgt;                /* ARP TARGET's units (SPEC 2.3, params.c atgt_bits): ATG_*, 0 = none (ALL only) */
    uint8_t ufilt;               /* DUELL 0.2.2: 1 = the engine has no filter of its own: the cutoff routes (ENV / LFO FLT, the matrix's CUT) and the FILTER page's
                                  * CUT RES MODE SLOPE (P_FREEA..D) drive the per-voice multimode filter (0.2.9; voice.c uf_run) and no longer the engine's tone control.
                                  * Every engine has it since cloud job J (ANALOG, TRIO and NOISE, which kept their own filter as 2, lost it: TWIN_FILTER_DESIGN_2); 0 only for an
                                  * engine slot that is not one (engines.c). The multimode filter is bypassed by default (MODE LP, CUT open: bit for bit as without it) */
} engine_t;
enum { ATG_NONE, ATG_ANALOG, ATG_TRIO, ATG_FM6, ATG_KINDS };

/* ------------------------------------------------- tracks, the song --- */
enum { ST_NOTE, ST_TIE, ST_REST };
#define SF_ACCENT 1u
#define SF_SLIDE 2u
#define SF_RATCH_SH 3u                   /* RATCH: the hits of a NOTE step - 1 (0..3: x1..x4, seq.c seq_ratchet); */
#define SF_RATCH (3u << SF_RATCH_SH)     /* bits 3..4, so a user preset's pattern (flag 4 = tie) carries it too */
typedef struct {                 /* acid-style step: up to 4 notes (POLY), time, accent, slide (DUELL 0.2.14: the drum lane hits of 1.0..0.2.13 are gone; the stores keep their two bytes, always 0) */
    uint8_t note[4];
    uint8_t n;                   /* notes in use, 0 = empty */
    uint8_t time;                /* ST_NOTE / ST_TIE / ST_REST */
    uint8_t flags;               /* SF_ACCENT | SF_SLIDE | SF_RATCH */
    uint8_t vel;
    uint8_t probability;         /* 0 = legacy 100%; 1..100 = percent, 101 = silent */
} step_t;
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
#define MOTION_ESC 127u          /* the "extension escape" (EXTENSION_BLOCK_PLAN 3.3, format only: nothing writes or reads it yet): a record whose id is 127 names an
                                  * extension slot in its value byte and the next record carries the value. No real parameter id is 127 (the assert below) */
#define MOTION_ID(e) ((uint32_t)(e)->param & 0x7Fu)
typedef struct { uint8_t place, param; int8_t value; } motion_event_t;
typedef struct { uint8_t count, on, rsv[2]; motion_event_t event[MOTION_MAX]; } motion_store_t;
_Static_assert(sizeof(motion_store_t) == 388u, "motion disk layout (FUN10)");
/* CRUSH's BITS knob, 0.2.14 (fx.c ins_bits_step): INSERT A is the detent of 1 .. 16 bits, a different depth at every detent (ins_bits.h).
 * The stores of before 0.2.14 held 8 detents a bit; they are no longer read, so nothing converts them */
#include "ins_bits.h"
#define PS_SLOTS 32u             /* DUELL: the pattern slots P01..P32, ONE list for the whole device (patslot.c, SAVE > PATTERNS) */
#define MU_SLOTS 16u             /* DUELL: the multis M01..M16 (project.c multi.c, SAVE > MULTI) */

#define NMSLOT 8u
typedef struct {                 /* the part in mix_part, its window: one per part (MIXSPLIT job 2; the parts' stages share none): a field of track_t, so a reader has it at a fixed offset from t */
    uint8_t on;                  /* a slot is active: track_render calls mod_voice */
    uint8_t e2;                  /* DUELL: an active slot reads ENV 2: track_render ticks it (voice.c env2_tick) */
    uint8_t l2;                  /* DUELL: an active slot reads LFO 2: track_render ticks it (voice.c lfo_tick) */
    int32_t lfo2;                /* DUELL: LFO 2 as the slots read it this block (FADE, DEPTH2 applied; Q15) */
    int32_t ldep2;               /* DUELL: DEPTH2, LFO 2's gain (Q15, 32767 = none) */
    uint8_t amp;                 /* an AMP slot: the voice gain applies */
    uint8_t nv, nk;              /* voice terms, per-block parameters held */
    int32_t pit, cut, shp, gain; /* per voice, from the per-track sources (gain Q15) */
    int32_t ldep;                /* DEPTH: the track LFO's gain (Q15; 32767 = none, track_render leaves it alone) */
    uint8_t vsrc[NMSLOT], vdst[NMSLOT];
    int8_t vamt[NMSLOT];
    uint8_t kid[NMSLOT];         /* per-block: the parameter, its stored value */
    int16_t keep[NMSLOT];
} mod_win_t;

typedef struct track {
    int16_t p[EXT_P_LEN];        /* P_COUNT ids, a gap at 127, then the NX extension slots (ext.h XV); every store and format still counts P_COUNT */
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
    uint8_t seq_notes[4];       /* sounding seq notes (the step's notes) */
    uint8_t seq_n;
    uint8_t seq_hold;            /* last step slides: keep the notes until the next step */
    uint8_t slide_glide;         /* next legato note glides (slide) */
    uint8_t rat_left;            /* RATCH: repeats of the playing step still to come (seq.c seq_ratchet) */
    uint32_t seq_off;
    uint8_t seq_active;          /* any step programmed */
    uint8_t rskip_idx;           /* live recording put notes into the step about to play: */
    uint8_t rskip_n, rskip[4];       /* do not trigger them again there (they sound already) */
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
    mod_win_t mdw;               /* MIXSPLIT job 2: the part's matrix window (mod.c) and MORPH cross-fade (mxw), the morph window's state (morph.c: mwon, the part's index mwpi), and one byte each: mgon (a bus offset is not 0), mdk (a slot moves DUCK DEPTH), eqlv (the EQ states may hold a signal). In the track so the
                                  * audio code has them at a fixed offset from t: no per-part index, no division (the parts' stages share none of them) */
    mxf_t mxw;
    uint8_t mwon, mwpi, mgon, mdk, eqlv;
    uint32_t esk[3], esk2[3];    /* job 3: the envelope's ENV SYNC steps of this block (voice.c esync_coef / env_tick; ENV 2's: esk2): the part's own */
    uint32_t rngs;               /* job 3: this part's own random state for the voice stage (voice.c part_rand: parts 2.. draw here, part 1 from libc.c rng()) */
    /* cloud job B (docs/design/EXTENSION_BLOCK_PLAN.md 3.4, CLOUD_FEAT_PLAN): the matrix's SECOND LIST, for what the hot mod_voice must not learn (mod.c mod_begin fills it, mod_f2_voice and
     * mod_cycle_voice read it; the part's own, written by the one core that renders the part): the slots whose destination is CUTOFF 2 (per-voice sources: f2m; the per-track sources' sum:
     * f2add, Q8 dial steps) and whose source is CYCLE with a per-note destination (cym), CYCLE read by an active slot (cy: the cycle ticks, job F), the latest note's CYCLE (m_cycle, Q15: the
     * per-block destinations read it as ENV 2's m_env2) and this voice's ENV for filter 2 (venv, Q15). All 0 with the second list empty */
    int32_t m_cycle, f2add, venv;
    uint8_t f2m, cym, cy;
    uint8_t rsv_sz[2];           /* (sizeof(track_t) a multiple of 16: below; 0.2.14: 12 more, the step_t hit / acc bytes and the drum notes gone; DUELL: 15 before ENV 2, LFO 2; 4b: 7 before mf; EQ: 4 less for P_COUNT 113; DUCK: 4 more for 119; DUCK 3: 6 more for 122, the old pad of 6 gone and 4 added; ENV 2 / LFO 2 DEST: 2 more for 127, the p[] growth padded to 4; wave 0: p[] 17 entries longer = 34 B, the pad 4 -> 2) */
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
_Static_assert(P_COUNT < XW_BASE && EXT_P_LEN == 144, "ext.h: the grown p[] (P_COUNT 127, a gap at 127, NX slots)");
_Static_assert(P_COUNT <= MOTION_ESC && XW_BASE == MOTION_ESC + 1u, "no real parameter id is 127 (MOTION_ESC, the reserved extension escape); the slots' wide ids start at 128");
#define DUELL_EXT_TYPES_DONE
#include "ext.h"                /* second time: the hook declarations */
#include "up_xb.h"              /* the patch record's extension block and the hand-set B edit list (job A, the store) */

typedef struct {
    int16_t g[G_COUNT];
    uint8_t playing, seq_mode;
    uint8_t grid;                /* 2: NAME (ui_name.c): no key plays (0 otherwise; 1 was the DRUM grid, gone in 0.2.14) */
    uint8_t rec;                 /* live recording armed: bit per track */
    uint8_t sel;                 /* selected track 0..NTRK-1: keys, pages, editor */
    int8_t octave;
    uint32_t tick;               /* sub-blocks since play */
    uint32_t cpu_q8;             /* audio ISR load, 1/256 */
    uint32_t master_q12;
    int32_t batt_raw;            /* smoothed ADC ch3 (battery divider), 0 = not read yet */
    int16_t bm;                  /* G_BM: BASS MONO, 0 OFF .. BM_N (0.2.19; at the end: no other field moves) */
    int16_t cp[4];               /* G_CT G_CR G_CA G_CL: the master compressor's THRESH, RATIO, ATTACK, RELEASE (0.2.21; a device setting: cmp_set) */
    int16_t ms[1];               /* G_WD: the master WIDTH (0..200 %) (0.2.23; a device setting: ui.c ms_sync) */
} song_t;

static track_t trk[NTRK];        /* the instrument: parts A and B */
#define HOTG_BSS __attribute__((section(".bss.hotg")))   /* variables that the hot (RAM-placed) functions read, in a section of their own: the compiler's
                                  * global merging packs small variables into blocks in the order of their size, so what sits at the start of a block (a 2-byte
                                  * load) or deep in it (a 4-byte load, a second address constant) changes whenever any other small variable grows or
                                  * shrinks. That moved the hot block's free bytes by 20 B when NPART went 4 -> 2 (memory trim 1). Pinned: song, and the dry mix
                                  * pair (fx.c mixlr, as one object: the left is the right + 128). Check the free bytes after a change of small globals (build.py) */
#define SCRATCH_BSS __attribute__((section(".bss.scr")))   /* main-loop scratch buffers kept in ordinary RAM instead of the pool (memory trim 1, item 5: the pool tail is where the
                                  * core-1 code of range Z grows; the ordinary RAM had 18 KB to spare). A section of its own, so the compiler's merging of small globals is not disturbed.
                                  * Only buffers that the main loop alone touches (no interrupt, no core 1, no second owner at the same time) may use it */
static song_t song HOTG_BSS;
/* DUELL 0.2.14: G_CLOCK is the CHOICE (GLO > TEMPO K4 CLK): 0 INT, 1 USB, 2 TRS (as every older store), 3 AUTO (the default: the internal
 * clock, or the MIDI clock arriving on USB or TRS). What reads "is an external clock running" asks clock_src() (midi_clock.c): the ACTIVE
 * source, 0 INT, 1 USB, 2 TRS (a fixed choice: itself; AUTO: the port it locked onto, else 0) */
#define CLK_AUTO 3
static uint32_t clock_src(void);
/* the CPU load meter's counters (audio.c writes them per I2S half, ui_menu.c cpu_meter_poll reads them) and the half's time */
#define CPU_BUDGET_US (HALF_FRAMES * 1000000u / FS)
static volatile uint32_t cpu_sum, cpu_n, cpu_pk, cpu_miss;
#if DUELL_BENCH
#define C0_STK_FILL 0xC0C0C0C0u                                              /* main.c c0_stack_paint, bench.c c0_stack_peak: core 0's stack marks, developer builds only */
static void c0_stack_bounds(uint32_t **b);                                   /* b[0..3] = the user stack's bottom and top, the exception stack's bottom and top (main.c; a host test names its own) */
#endif
#if DUELL_BENCH                  /* src/bench.c (bench builds only): its hooks in ui_input.c (first: the BENCH screen's input, the run's clock), ui_menu.c (its
                                  * drawing), ui_guide.c (the BENCH row opens it) and budget.c (the cost rows hold the NOTES budget open) */
static void bench_open(void);
static void bench_draw(void);
static int bench_input(uint32_t pressed, uint32_t notes);   /* ui_input.c, first: 1 = the BENCH screen took the frame */
static uint8_t bench_bopen __attribute__((section(".bench_bss")));   /* 1: budget.c budget_poll gives no numbers (bnotes 0: the budget off), the cost rows */
static uint32_t bench_mono_seed __attribute__((section(".bench_data"))) = 0x1234567u;   /* voice.c mono_play's start-phase seed, at file level in bench builds (the reset: bench.c) */
static void bench_con(const char *args);                    /* console.c: the console word `bench ...` (stage 1: src/bench.c) */
static void bench_half(void);                               /* audio.c, bench builds only: the start of a half (the timeline player) */
static volatile uint8_t bench_tl_on __attribute__((section(".bench_bss")));   /* 1: a timeline test is running (audio.c calls bench_half then) */
#endif
#ifndef NLIVE
#define NLIVE NPART              /* parts that render and own engine / insert / wheel / motion state (RAM is sized by it): every part */
#endif
#define TRK_HIDDEN(t) ((uint32_t)((t) - trk) >= NLIVE)   /* (no hidden part exists any more: 0 for every track of the instrument) */
#define PART_OF(t) ((uint32_t)((t) - trk) % NLIVE)   /* the part's index 0..NLIVE-1 (the per-part arrays) */

/* MIXSPLIT job 2 (docs/MIXSPLIT_NOTES.md): the scratch of one part's block. Each part owns one, so the stages of two parts share no scratch
 * (they could run in any order, or on two cores). Written and read inside one part's stages only. fx_bus.c rev_switch borrows pscr[0].buf
 * after the parts (the send stage; nothing of a part is alive then) */
typedef struct {
    int32_t buf[CTL];            /* the part's block: voices in (part_voice), the effects in place (part_fx), the mix out (part_mix) */
    int32_t dry[CTL], dryd[CTL]; /* PFX_FADE: the dry block (and SPREAD's side) of the PART FX crossfade (fx.c pfx_mix, pfx_mute) */
    int32_t sbuf[CTL];           /* SPREAD: the left voices (voice.c track_render, fx.c mix_spread) */
    uint8_t spread_on;           /* SPREAD runs for the part's block (set by fx.c part_voice, read by voice.c track_render) */
} part_scr_t;
static part_scr_t pscr[NLIVE];
#define MXF(t) ((t)->mxw)        /* part t's MORPH cross-fade (morph.c morph_begin / morph_end; the readers: voice.c lfo_tick, the engines' render functions) */
#define MODW(t) ((t)->mdw)       /* part t's matrix window (mod.c) */
#define TSEL (&trk[song.sel])    /* the selected track */

/* DUELL 0.2.14 GLO > MODULES (SPEC "Modules"): four DEVICE switches, ARP, SEQ, SENDS, PART FX, each ON (the default) or OFF. Kept in the
 * settings byte favorites.factory[15][10] bits 1..4 (ui.c ui_mods; a SET bit is OFF, so a new unit, ERASE DATA and every older record
 * read all four ON), never in a patch, a multi or the project. ui_input copies them here every frame (ui.c mods_apply); the audio side
 * reads only this byte. The stored values of the arp, the steps, the sends and the part effects are never touched by a switch */
#define MODS_ARP 1u              /* ARP OFF: no arp plays, whatever its MODE (seq.c: arp_mode below); the keys play directly */
#define MODS_SEQ 2u              /* SEQ OFF: no stored step plays (seq.c seq_play), nor the arp's SEQUENCER mode (it plays the steps) */
#define MODS_SENDS 4u            /* SENDS OFF: the chorus, delay and reverb buses and SPACE skipped after a ~22 ms fade (fx.c snd_*) */
#define MODS_PFX 8u              /* PART FX OFF: each part's DIST, INSERT, EQ and DUCK skipped after a ~22 ms crossfade (fx.c pfx_*) */
#define MODS_ALL 15u
static volatile uint8_t mods_off;
/* the arp's MODE as it plays now: the stored P_AMODE, or 0 (OFF) while MODULES switches it off: ARP OFF every mode, SEQ OFF the SEQUENCER
 * mode (15, seq.c AM_SEQ: it plays the steps). Every read that decides whether the arp plays goes through it (seq.c, params.c atgt_bits) */
#define AM_SEQ_ID 15
static inline int32_t arp_mode(const track_t *t)
{
    int32_t m = t->p[P_AMODE];
    return !m || (mods_off & MODS_ARP) || (m == AM_SEQ_ID && (mods_off & MODS_SEQ)) ? 0 : m;
}
/* .. its HOLD as it acts now. HOLD ARP (1) means "the arp keeps its chord": with the arp switched off by MODULES there is no chord to
 * keep, so no latch (0). HOLD ALL (2) latches the keys whatever the arp does. An arp whose own MODE is OFF: HOLD as stored (as ever) */
static inline int32_t arp_hold(const track_t *t)
{
    int32_t h = t->p[P_AHOLD];
    return h == 1 && t->p[P_AMODE] && !arp_mode(t) ? 0 : h;
}

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
