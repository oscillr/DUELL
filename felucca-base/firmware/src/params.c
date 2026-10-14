/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Parameter descriptors, formatting and the page table. */
static const char *const N_LWAVE[] = {"SIN", "TRI", "SAW", "SQR", "S&H"};
/* LFO 2 (1.2, #154). SYNC: OFF (RATE), or one cycle per note value of the tempo, longest first (LSYNC_DIV: its N_DIV);
 * TRIG: NOTE restarts it at PHS on a fresh phrase (as before), FREE never; POL: BI -1..+1 (as before), UNI 0..+1 */
static const char *const N_LSYNC[] = {"OFF", "4BAR", "2BAR", "1/1", "1/2", "1/4", "1/8", "8T", "1/16", "16T", "1/32",
                                       "4T", "2T", "32T", "2D", "4D", "8D", "16D"};   /* (append-only; the knob order: LSYNC_ORDER) */
static const uint8_t LSYNC_DIV[18] = {0, 9, 8, 7, 6, 0, 1, 4, 2, 5, 3, 10, 11, 12, 13, 14, 15, 16};
static const char *const N_DKSRC[] = {"PUMP", "PART", "CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7", "CH8", "CH9", "CH10", "CH11", "CH12",
                                       "CH13", "CH14", "CH15", "CH16"};   /* DUCK SRC: the MIDI channel is part of the source */
static const char *const N_DKCRV[] = {"LIN", "EXP"};
static const char *const N_LTRIG[] = {"NOTE", "FREE"};
static const char *const N_LPOL[] = {"BI", "UNI"};
static const char *const N_AMODE[] = {"OFF", "UP", "DN", "UPDN", "RND", "ORD", "REPEAT",   /* (append-only: seq.c AM_*) */
                                       "DNUP", "UP+8", "CONV", "DIVG", "PINKY", "THUMB", "WALK", "CHORD", "SEQ"};
static const char *const N_DIV[] = {"1/4", "1/8", "1/16", "1/32", "8T", "16T", "1/2", "1/1", "2BAR", "4BAR",
                                     "4T", "2T", "32T", "2D", "4D", "8D", "16D"};   /* (append-only: stored indices; fx.c div_samples) */
static const char *const N_SCALE[] = {"CHR", "MAJ", "MIN", "DOR", "MIX", "PEN", "MPEN", "HARM",
                                    "PHRY", "LYD", "LOC", "MEL", "BLUES", "WHOLE", "DIMHW", "DIMWH"};
static const char *const N_HOLD[] = {"OFF", "ARP", "ALL"};   /* P_AHOLD: 0/1 as before, 2 = ALL (keys latch with the arp off too) */
static const char *const N_ONOFF[] = {"OFF", "ON"};
static const char *const N_SLOPE[] = {"-", "12dB", "24dB"};   /* the multimode filter's SLOPE: 1, 2 (0: a store of before, uf_legacy) */
/* ANALOG's filter TYPE (1.5, #104; eng_analog.c): the SVF's low-pass (as always), band-pass, high-pass output */
static const char *const N_FTYPE[] = {"LP", "BP", "HP"};
/* ENV SYNC (1.5, #175; voice.c esync_coef): ATK DEC REL as note values of the tempo, shortest first. 0..127 split
 * evenly over these 25 (param_format's F_INT name list: value v is ESYNC_NAMES[v * 25 / 128]); ESYNC_UNITS: each in
 * 1/384 of a bar of 4/4 (a quarter 96), 0 = the shortest time ATK / DEC / REL 0 has. 0-terminated (param_format) */
#define ESYNC_N 25u
static const char *const ESYNC_NAMES[ESYNC_N + 1u] = {
    "0", "1/64T", "1/64", "1/32T", "1/64D", "1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D",
    "1/4", "1/2T", "1/4D", "1/2", "1/1T", "1/2D", "1/1", "1/1D", "2BAR", "3BAR", "4BAR", 0};
static const uint16_t ESYNC_UNITS[ESYNC_N] = {0, 4, 6, 8, 9, 12, 16, 18, 24, 32, 36, 48, 64, 72,
                                              96, 128, 144, 192, 256, 288, 384, 576, 768, 1152, 1536};
static const param_desc_t ESYNC_DESC[6] = {   /* ATK DEC REL with ENV SYNC on: the range and defaults as ever */
    {"ATK", F_INT, 0, 127, 10, ESYNC_NAMES, 0},
    {"DEC", F_INT, 0, 127, 70, ESYNC_NAMES, 0},
    {"REL", F_INT, 0, 127, 60, ESYNC_NAMES, 0},
    {"ATK2", F_INT, 0, 127, 10, ESYNC_NAMES, 0},  /* DUELL: ENV 2's (ENV SYNC is both envelopes') */
    {"DEC2", F_INT, 0, 127, 70, ESYNC_NAMES, 0},
    {"REL2", F_INT, 0, 127, 60, ESYNC_NAMES, 0},
};
static uint32_t esync_idx(int32_t v) { return (uint32_t)(v & 127) * ESYNC_N >> 7; }   /* (param_format's split) */
/* P_QUANT (QNT) is RETIRED (owner 2026-10-12): the OFF-SCALE KEYS setting is global (core.h offscale_g, SNAP / PLAY), no page or key path reads
 * this any more; the id and its names stay so stored formats and the editor's table hold */
static const char *const N_QUANT[] = {"OFF", "SNAP", "WHITE", "SEQ"};
/* chord keys (chord.c): OFF, the diatonic triad / seventh of the track's ROOT and SCALE on the key, fixed shapes */
static const char *const N_CHRD[] = {"OFF", "DIA3", "DIA7", "MAJ", "MIN", "DOM7", "MAJ7", "MIN7", "SUS4", "POW"};
static const char *const N_OFFSCALE[] = {"SNAP", "PLAY"};   /* core.h offscale_g: OS_SNAP, OS_PLAY (SCALE page K3) */
static const char *const N_VOIC[] = {"CLOSE", "OPEN", "INV1", "INV2", "+OCT"};   /* VC_CLOSE .. VC_BASS */
static const char *const N_VOICE[] = {"POLY", "MONO", "LEG", "UNI"};   /* V_POLY .. V_UNISON */
static const char *const N_GLMODE[] = {"RATE", "TIME"};
static const char *const N_PRIO[] = {"LAST", "LOW", "HIGH"};
static const char *const N_ALLOC[] = {"ROT", "REUSE"};
static const char *const N_ORDER[] = {"NOTE", "PLAY"};
static const char *const N_CLOCK[] = {"INT", "USB", "TRS"};
/* MIDI IN (seq.c midi_pch), GLO > MIDI K3: OWN CH (stored 0) = each part listens on its own channel (P1 CH, P2 CH; default 1 and 2), the others
 * are ignored / SELECTED (1) = every channel plays the part chosen with ALGORITHM (values 2..4, the former CH5-8 .. CH13-16 blocks, read as OWN CH) */
static const char *const N_ROUTE[] = {"OWN CH", "SELECTED"};   /* (was PARTS, SEL: the same stored 0, 1) */
static const char *const N_NOTE[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const N_DASH[] = {"--"};
static const char *const N_RTYPE[] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO"};   /* G_RTYPE: the reverb bus's model (fx.c); SHIM, RESO ported from JIANT */
static const char *const N_GO[] = {"--", "GO"};
/* the INSERT's types (1.5, fx.c IT_*; append-only: stored). Its three values A B C mean what the type says (ins_desc) */
static const char *const N_ITYPE[] = {"OFF", "SOFT", "HARD", "FOLD", "FUZZ", "CRUSH", "PHASR", "FLANG", "CHOR"};
/* CRUSH: BITS 1..16 and RATE (the held sample's rate, fx.c INS_HOLD) over the range in 16 even parts (0-terminated) */
static const char *const N_IBITS[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15",
                                      "16", 0};
static const char *const N_IRATE[] = {"689", "919", "1.1k", "1.4k", "1.8k", "2.2k", "2.8k", "3.7k", "4.4k", "5.5k",
                                      "7.4k", "8.8k", "11k", "15k", "22k", "44k", 0};
/* modulation matrix (mod.c): sources, destinations (E1..E8 = P_E0..P_E7: shown with the engine's labels).
 * Append-only (stored values keep their meaning): S&H SLEW and DEPTH (#132) came after EXPR and E8 */
/* (DUELL: "AENV" is ENV 1, the AMP ENV: stored value 2, mod.c MS_ENV) */
static const char *const N_MSRC[] = {"OFF", "LFO1", "AENV", "VEL", "KEY", "RAND", "MODW", "AT", "EXPR", "S&H", "SLEW",
                                     "ENV2", "LFO2",   /* DUELL: ENV 2, LFO 2 (step 3) */
                                     "MORPH", "DUCK"};         /* DUELL step 4b: the part's MORPH fader, 0..1 (mod.c MS_MORPH) */
static const char *const N_MDST[] = {"OFF", "PITCH", "CUT", "SHP", "AMP", "PAN", "DIST", "CHO", "DLY", "REV", "RAT1",
                                     "VIB", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "DEP1",
                                     "RAT2", "DEP2",    /* DUELL: LFO 2's rate and depth (step 3) */
                                     /* DUELL 0.2.7 (mod.c MD_LEVEL ..): the part's LEVEL, the INSERT's A B C (the MATRIX page shows its TYPE's
                                      * labels) and MIX, the EQ gains, DUCK DEPTH, ENV 2's times and depths, RES; then the bus values (mod.c mod_bus) */
                                     "LEVEL", "INS A", "INS B", "INS C", "IMIX", "EQLOW", "EQMID", "EQHI", "DUCK",
                                     "ATK2", "DEC2", "SUS2", "REL2", "E2FLT", "E2SHP", "FRES",
                                     "CRT", "CDP", "FDBK", "DMDEP", "DMRAT", "RSIZE", "RDAMP"};
/* (a DST name is never an engine's EDIT label: the factory sounds' MOD routes name an engine slot by its label, RES = ANALOG's E6,
 * and tools/gen_duell_sounds.py takes an N_MDST name first; mod_test checks it) */
static const char *const N_ENGNAME[] = {"ANALOG", FELUCCA_FM4 ? "DIGITAL" : "-", "PHASE", "LOFI", "-" /* SAMPLE: retired, 0.2.9 */, "VOICE",
                                             "TRIO", "WHEEL", "-" /* GRAIN: retired */, "PHYS", "DRUM", "NOISE", "FM6",
                                             "-",      /* 13 (SLICE: retired, DUELL 0.2.9) */
                                             "WAVE",   /* 14 (DUELL) */
};

/* ARP TARGET's names (atgt_bits below) */
static const char *const N_ATG_NONE[] = {"ALL", "ALL", "ALL", "ALL", "ALL", "ALL", "ALL", "ALL", "ALL"};
static const char *const N_ATG_ANALOG[] = {"ALL", "A", "B", "ALL", "ALL", "ALL", "ALL", "ALL", "ALL"};
static const char *const N_ATG_TRIO[] = {"ALL", "OSC1", "OSC2", "OSC3", "1+2", "2+3", "1+3", "ALL", "ALL"};
static const char *const N_ATG_FM6[] = {"ALL", "OP1", "OP2", "OP3", "OP4", "OP5", "OP6", "ODD", "EVEN"};

#define PD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}
#define PFREE {"-", F_INT, 0, 0, 0, 0, 0}   /* a free id (core.h P_FREE*) */
#define PE(l, n, df) {l, F_ENUM, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df, n, 0}

static const param_desc_t TP[P_COUNT] = {
    [P_LEVEL] = PD("LVL", F_DB, 0, 127, 104),
    [P_ATK] = PD("ATK", F_TIME, 0, 127, 10),
    [P_DEC] = PD("DEC", F_TIME, 0, 127, 70),
    [P_SUS] = PD("SUS", F_PCT, 0, 127, 90),
    [P_REL] = PD("REL", F_TIME, 0, 127, 60),
    [P_ED_FLT] = PD("FLT", F_BIPCT, -64, 63, 0),
    [P_ED_PIT] = PD("PIT", F_BIPCT, -64, 63, 0),
    [P_ED_SHP] = PD("SHP", F_BIPCT, -64, 63, 0),
    [P_EQLG] = PD("LOW", F_EQDB, -12, 12, 0),      /* DUELL EQ (fx.c track_eq): all 0 = off, bit for bit as before */
    [P_LRATE] = PD("RATE", F_LFOHZ, 0, 127, 60),
    [P_LWAVE] = PE("WAVE", N_LWAVE, 0),
    [P_LPHASE] = PD("PHS", F_INT, 0, 127, 0),
    [P_LFADE] = PD("FADE", F_TIME, 0, 127, 0),
    [P_LD_PIT] = PD("PIT", F_BIPCT, -64, 63, 0),
    [P_LD_FLT] = PD("FLT", F_BIPCT, -64, 63, 0),
    [P_LD_SHP] = PD("SHP", F_BIPCT, -64, 63, 0),
    [P_LD_AMP] = PD("AMP", F_PCT, 0, 127, 0),
    [P_AMODE] = PE("MODE", N_AMODE, 0),
    [P_ARATE] = PE("RATE", N_DIV, 2),
    [P_AOCT] = PD("OCT", F_INT, 1, 4, 1),
    [P_AGATE] = PD("GATE", F_PCT, 1, 127, 64),
    [P_ASWING] = PD("SWG", F_PCT, 0, 100, 0),
    [P_APROB] = PD("PROB", F_PCT, 0, 127, 127),
    [P_AHOLD] = PE("HOLD", N_HOLD, 0),
    [P_AORDER] = PE("ORD", N_ORDER, 0),
    [P_ROOT] = PD("ROOT", F_NOTE, 0, 11, 0),
    [P_SCALE] = PE("SCL", N_SCALE, 0),
    [P_QUANT] = PE("QNT", N_QUANT, 0),
    [P_TRANS] = PD("TRN", F_SEMI, -24, 24, 0),
    [P_SLEN] = PD("LEN", F_STEPS, 1, NSTEP, 16),
    [P_SDIV] = PE("DIV", N_DIV, 2),
    [P_SSWING] = PD("SWG", F_PCT, 0, 100, 0),
    [P_SGATE] = PD("GATE", F_PCT, 1, 127, 64),
    [P_DIST] = PD("DST", F_PCT, 0, 127, 0),
    [P_CHOR] = PD("CHO", F_PCT, 0, 127, 0),
    [P_DLY] = PD("DLY", F_PCT, 0, 127, 0),
    [P_REV] = PD("REV", F_PCT, 0, 127, 0),
    [P_VOICE] = PE("VCE", N_VOICE, 0),
    [P_GLIDE] = PD("GLD", F_TIME, 0, 127, 0),
    [P_GLMODE] = PE("GLMOD", N_GLMODE, 0),
    [P_PRIO] = PE("PRIO", N_PRIO, 0),
    [P_ALLOC] = PE("ALLOC", N_ALLOC, 0),
    [P_DETUNE] = PD("DTUNE", F_INT, 0, 127, 40),
    [P_PAN] = PD("PAN", F_BIPCT, -64, 63, 0),
    [P_MUTE] = PE("MUTE", N_ONOFF, 0),
    /* the SLICER's ids (removed). The MULTIMODE FILTER (DUELL 0.2.9, voice.c uf_run; core.h uf_legacy), on the engines with engine_t.ufilt
     * only: CUT (dial steps, 30 Hz .. 16 kHz, the open end the default), RES (0 = Q 0.7), MODE (LP .. BP .. HP), SLOPE (12 / 24 dB). The
     * defaults are the bypass (MODE LP, CUT open: the filter is not run, bit for bit as without it) */
    [P_FREEA] = PD("CUT", F_FILT, 0, UF_STEPS, UF_STEPS),
    [P_FREEB] = PD("RES", F_PCT, 0, 127, 0),
    [P_FREEC] = PD("MODE", F_FMODE, 0, UF_HP, 0),
    [P_FREED] = {"SLOPE", F_ENUM, 1, 2, 1, N_SLOPE, 0},
#define MSLOT(k) [P_M##k##SRC] = PE("SRC" #k, N_MSRC, 0), [P_M##k##DST] = PE("DST" #k, N_MDST, 0), \
                 [P_M##k##AMT] = PD("AMT" #k, F_BIPCT, -64, 63, 0)
    MSLOT(1), MSLOT(2), MSLOT(3), MSLOT(4), MSLOT(5), MSLOT(6), MSLOT(7), MSLOT(8),
#undef MSLOT
    /* DUELL: the free ids (core.h PL_*: Felucca's P_ED_FX, DIGITAL's OP ENV, DRUM's lane levels; the ones no DUELL parameter
     * took yet): no label, no range, on no page, in no list; a stored value loads as 0 */
    /* DUELL: ENV 2 (voice.c env2_tick), ENV 1's ranges and defaults; routed only through the matrix (mod.c MS_ENV2). The
     * labels end in 2: unique for the editor's library keys and in every list (ENV 1's are ATK DEC SUS REL) */
    [P_E2ATK] = PD("ATK2", F_TIME, 0, 127, 10),
    [P_E2DEC] = PD("DEC2", F_TIME, 0, 127, 70),
    [P_E2SUS] = PD("SUS2", F_PCT, 0, 127, 90),
    [P_E2REL] = PD("REL2", F_TIME, 0, 127, 60),
    /* DUELL: LFO 2 (voice.c lfo_tick, LFO 1's code), its ranges and defaults; routed only through the matrix (MS_LFO2) */
    [P_L2RATE] = PD("RATE2", F_LFOHZ, 0, 127, 60),
    [P_L2WAVE] = PE("WAVE2", N_LWAVE, 0),
    [P_L2PHASE] = PD("PHS2", F_INT, 0, 127, 0),
    [P_L2FADE] = PD("FADE2", F_TIME, 0, 127, 0),
    [P_CHRD] = PE("CHRD", N_CHRD, 0),
    [P_VOIC] = PE("VOIC", N_VOIC, 0),
    [P_L2SYNC] = PE("SYNC2", N_LSYNC, 0),
    [P_L2TRIG] = PE("TRIG2", N_LTRIG, 1),   /* DUELL 0.2.2: FREE (free-running) is the default, NOTE the option */
    [P_L2POL] = PE("POL2", N_LPOL, 0),
    [P_ATGT] = PE("TRGT", N_ATG_FM6, 0),         /* ARP TARGET (SPEC 2.3): the device shows the engine's (atgt_desc) */
    [P_EQLF] = PD("LO HZ", F_EQLO, -24, 24, 0),   /* LOW shelf's corner, 120 Hz at 0 (30 .. 480 Hz) */
    [P_EQMG] = PD("MID", F_EQDB, -12, 12, 0),
    [P_EQMF] = PD("MD HZ", F_EQMID, -24, 24, 0),  /* MID's centre, 1 kHz at 0 (250 Hz .. 4 kHz) */
    [P_EQMQ] = PD("Q", F_EQQ, -12, 12, 0),        /* MID's Q, 1.0 at 0 (0.25 .. 4) */
    [P_LSYNC] = PE("SYNC", N_LSYNC, 0),
    [P_LTRIG] = PE("TRIG", N_LTRIG, 1),    /* DUELL 0.2.2: FREE by default (was NOTE); the sparse patch store keeps NOTE as its own default: upreset.c up_def */
    [P_LPOL] = PE("POL", N_LPOL, 0),
    [P_SQNT] = PE("QNTZ", N_ONOFF, 1),            /* ON: every step on its start, as before 1.2 */
    [P_SPRD] = PD("SPRD", F_PCT, 0, 127, 0),      /* #148: 0 = every voice on PAN, as before (fx.c mix_spread) */
    /* the INSERT (1.5, fx.c track_insert): TYPE OFF = as before. A B C as the editor protocol names them; the device
     * shows the type's own labels (ins_desc) */
    [P_ITYPE] = PE("INSRT", N_ITYPE, 0),
    [P_IA] = PD("INS A", F_PCT, 0, 127, 64),
    [P_IB] = PD("INS B", F_PCT, 0, 127, 96),
    [P_IC] = PD("INS C", F_PCT, 0, 127, 96),
    [P_IMIX] = PD("MIX", F_PCT, 0, 127, 127),
    [P_FTYPE] = PE("TYPE", N_FTYPE, 0),           /* 1.5 (#104): ANALOG's filter, LP = as before (eng_analog.c) */
    [P_ESYNC] = PE("ESYNC", N_ONOFF, 0),          /* 1.5 (#175): OFF = ATK DEC REL in ms, as before (voice.c esync) */
    [P_EQHG] = PD("HIGH", F_EQDB, -12, 12, 0),
    [P_EQHF] = PD("HI HZ", F_EQHI, -24, 12, 0),   /* HIGH shelf's corner, 8 kHz at 0 (2 .. 16 kHz) */
    /* DUELL DUCK (SPEC 5c.4, fx.c duck_*): a note-on on CH (and NOTE) ducks the part's level; CH 0 (OFF) or DEPTH 0 = nothing */
    [P_DKCRV] = PE("CURVE", N_DKCRV, 1),           /* the RELEASE's shape: LIN (a straight line, the earlier curve), EXP (a share of what is left: a pump's release, the default since 0.2.5; the sparse patch store keeps LIN as its own default: upreset.c up_def) */
    [P_DKNOTE] = PD("NOTE", F_DKNOTE, 0, 127, 0),
    [P_DKDEP] = PD("DEPTH", F_PCT, 0, 100, 0),
    [P_DKATK] = {"ATK", F_INT, 1, 100, 5, 0, "ms"},
    [P_DKHLD] = PD("HOLD", F_DKHLD, 0, 100, 0),    /* LEGACY (0.2.5): on no page, never read by the DSP (a pump is attack and release); the id, the stored value and the protocol keep it */
    [P_DKRLS] = PD("RLS", F_DKRLS, 0, 67, 36),
    [P_DKSRC] = PE("SRC", N_DKSRC, 0),             /* PUMP (stored 0: every beat of RATE), PART (the other part's level), CH1..16 (that channel's notes, NOTE) */
    [P_DKRATE] = PE("RATE", N_DIV, 0),             /* PUMP's division (N_DIV: 1/4 = four on the floor) */
    [P_DKTHR] = PD("THR", F_PCT, 0, 100, 30),      /* PART: the key level above which the duck starts */
    /* DUELL: ENV 2 DEST and LFO 2 DEST (the classic arrangement, SPEC 2.4): ENV 2 / LFO 2's own depths, ENV 1 DEST's and LFO 1
     * DEST's units and ranges (voice.c track_render adds them where ENV 1's and LFO 1's go). Default 0: every stored sound
     * loads as before (a store without them takes the default); a new factory sound sets ENV 2 FLT (+40) itself */
    [P_E2FLT] = PD("FLT", F_BIPCT, -64, 63, 0),
    [P_E2SHP] = PD("SHP", F_BIPCT, -64, 63, 0),
    [P_L2PIT] = PD("PIT", F_BIPCT, -64, 63, 0),
    [P_L2FLT] = PD("FLT", F_BIPCT, -64, 63, 0),
    [P_L2SHP] = PD("SHP", F_BIPCT, -64, 63, 0),
};

/* the INSERT's A B C as its TYPE means them (TP's ranges and defaults): the drives DRIVE TONE LEVEL, CRUSH BITS RATE
 * LPF, the swept ones RATE DEPTH FDBK; OFF TP's own */
#define PU(l, f, df, n, u) {l, f, 0, 127, df, n, u}
static const param_desc_t INS_DESC[3][3] = {
    {PU("DRIVE", F_PCT, 64, 0, 0), PU("TONE", F_CUTOFF, 96, 0, 0), PU("LEVEL", F_DB, 96, 0, 0)},
    {PU("BITS", F_INT, 64, N_IBITS, 0), PU("RATE", F_INT, 96, N_IRATE, "Hz"), PU("LPF", F_CUTOFF, 96, 0, 0)},
    {PU("RATE", F_LFOHZ, 64, 0, 0), PU("DEPTH", F_PCT, 96, 0, 0), PU("FDBK", F_PCT, 96, 0, 0)},
};
#undef PU
static const param_desc_t *ins_desc(int32_t type, uint32_t k)
{
    if (type <= 0 || type >= (int32_t)NELEM(N_ITYPE))
        return &TP[P_IA + k];
    return &INS_DESC[type < 5 ? 0 : type == 5 ? 1 : 2][k];   /* (IT_SOFT .. IT_FUZZ, IT_CRUSH, the swept ones) */
}

static const param_desc_t GP[G_COUNT] = {
    [G_BPM] = PD("TEMPO", F_BPM, 40, 240, 120),
    [G_SWING] = PD("SWG", F_PCT, 0, 100, 0),
    [G_CLOCK] = PE("CLK", N_CLOCK, 0),
    [G_TUNE] = PD("TUNE", F_INT, -50, 50, 0),     /* cents; edited on MENU > AUDIO TUNE (1.2: menu_items.c MI_TUNE) */
    [G_DTIME] = PE("TIME", N_DIV, 1),
    [G_DFDBK] = PD("FDBK", F_PCT, 0, 120, 60),
    [G_DCOLOR] = PD("COLR", F_PCT, 0, 127, 70),
    [G_DMIX] = PD("MIX", F_PCT, 0, 127, 90),
    [G_RSIZE] = PD("SIZE", F_PCT, 0, 127, 90),
    [G_RDAMP] = PD("DAMP", F_PCT, 0, 127, 60),
    [G_CRATE] = PD("CRT", F_LFOHZ, 0, 127, 40),
    [G_CDEPTH] = PD("CDP", F_PCT, 0, 127, 60),
    /* G_MIDI, G_SYNC, G_INFO: the old SYSTEM page's (gone in 1.2), on no page: MIDI's value was never read (its card
     * showed the USB state), SYNC was a placeholder, CPU a reading; the USB state and the CPU load are MENU > SYSTEM >
     * INFO's now (ui_menu.c). Kept: the ids and G_COUNT are fixed by the formats and the protocol (0.2.5: MIDI and SYNC carry the delay's modulation, below) */
    /* DUELL 0.2.5: the delay's MODULATION (SPEC 5c.2) on these two ids (core.h G_DMDEP, G_DMRATE): DEPTH 0 = off (the delay as before, bit for
     * bit), 100 = the time swings +- 10 ms (never more than a quarter of the delay time); RATE as the chorus's, 0.05 .. 3.4 Hz, 0.51 Hz at power-on */
    [G_DMRATE] = PD("RATE", F_LFOHZ, 0, 80, 44),
    [G_DMDEP] = PD("DEPTH", F_PCT, 0, 100, 0),
    /* MIDI IN's routing, edited on GLO > MIDI K3 (DUELL 0.1; MENU > MIDI > MIDI IN in 1.2..1.5, up to 1.1.5 GLO > SYSTEM ROUT),
     * stored with the project as before. (was "--": stored 0 = CH1-4, as MIDI IN always was) */
    [G_ROUTE] = PE("ROUT", N_ROUTE, 0),
    [G_INFO] = PD("CPU", F_INT, 0, 0, 0),
    [G_SLOT] = PD("SLOT", F_INT, 1, 16, 1),   /* DUELL: SAVE > MULTI's slot, M01..M16 (project.c multi.c) */
    [G_NAME] = PE("NAME", N_DASH, 0),
    [G_LOAD] = PE("LOAD", N_GO, 0),
    [G_SAVE] = PE("SAVE", N_GO, 0),
    [G_ENGSEL] = PE("ENG", N_ENGNAME, 0),
    [G_ENGGO] = PE("SET", N_GO, 0),
    [G_CLRSEQ] = PE("CLRSQ", N_GO, 0),
    [G_INITSND] = PE("INIT", N_GO, 0),
    /* the reverb's model on the REVERB page: the id of the old GM drum channel (G_DRCH, inert since 1.0) */
    [G_RTYPE] = PE("TYPE", N_RTYPE, 2),         /* HALL at power-on since 1.2 (ROOM before; projects keep theirs) */
    /* inert: they set the GM drum part (level, reverb send), which is gone (drums are the DRUM engine
     * on any part). On no page; kept so the ids and G_COUNT, which the project format and the
     * editor protocol depend on, do not move */
    [G_PINGSPREAD] = PD("SPREAD", F_PCT, 0, 100, 100),   /* DUELL: the id of the old GM drum part's level; 100 = full left / right, as PING-PONG ran */
    /* DUELL (SPEC 5c.2): the delay's PING-PONG on the id of the old GM drum part's reverb send; stored 0 = OFF, as every older store */
    [G_PINGPONG] = PE("PING", N_ONOFF, 0),
};

/* ARP TARGET (SPEC 2.3, P_ATGT): the arp's units of a part's engine (engine_t.atgt), as combinations a value each: 0 ALL is
 * the classic arp (a note per step); any other value keeps one voice per held key on the root and moves the units in its
 * bits (the engine's order: ANALOG bit 0 OSC A, 1 OSC B; TRIO bit k OSC k+1; FM6 bit k the patch's operator k, OP6 = 0,
 * OP1 = 5) to the arp's note (seq.c atg_fire, voice.c atg_voice). Every list is 9 long ("ALL" past the engine's own),
 * so any stored value (0..8) shows; a value past the engine's own plays as ALL (atgt_bits) */
static const uint8_t ATG_BITS[ATG_KINDS][9] = {
    {0},
    {0, 1, 2},
    {0, 1, 2, 4, 3, 6, 5},
    {0, 1u << 5, 1u << 4, 1u << 3, 1u << 2, 1u << 1, 1u, (1u << 5) | (1u << 3) | (1u << 1), (1u << 4) | (1u << 2) | 1u},
};
static const param_desc_t ATG_DESC[ATG_KINDS] = {
    {"TRGT", F_ENUM, 0, 0, 0, N_ATG_NONE, 0},
    {"TRGT", F_ENUM, 0, 2, 0, N_ATG_ANALOG, 0},
    {"TRGT", F_ENUM, 0, 6, 0, N_ATG_TRIO, 0},
    {"TRGT", F_ENUM, 0, 8, 0, N_ATG_FM6, 0},
};
/* the units of t's engine that follow the arp, 0 = ALL (the classic arp; also with the arp off) */
static uint32_t atgt_bits(const track_t *t)
{
    uint32_t k = ENGINES[t->engine]->atgt, v = (uint32_t)t->p[P_ATGT];
    if (!t->p[P_AMODE] || k >= ATG_KINDS || v > (uint32_t)ATG_DESC[k].max)
        return 0;
    return ATG_BITS[k][v];
}

static const param_desc_t *track_desc(const track_t *t, uint32_t id)
{
    if (id == P_ATGT) {                               /* ARP TARGET: the engine's units */
        uint32_t k = ENGINES[eng_idx(t->eng_req)]->atgt;
        return &ATG_DESC[k < ATG_KINDS ? k : 0];
    }
    if (id >= P_E0 && id <= P_E7) {                   /* the engine asked for (t->engine follows after a fade) */
        const engine_t *e = ENGINES[eng_idx(t->eng_req)];
        const param_desc_t *d = e->desc ? e->desc(t, id - P_E0) : 0;   /* a mode-dependent label / names */
        return d ? d : &e->edit[id - P_E0];
    }
    if (id >= P_IA && id <= P_IC)                     /* the INSERT's values as its TYPE names them */
        return ins_desc(t->p[P_ITYPE], id - P_IA);
    if (t->p[P_ESYNC] && (id == P_ATK || id == P_DEC || id == P_REL))   /* ENV SYNC: note values (1.5, #175) */
        return &ESYNC_DESC[id == P_ATK ? 0 : id == P_DEC ? 1 : 2];
    if (t->p[P_ESYNC] && (id == P_E2ATK || id == P_E2DEC || id == P_E2REL))   /* .. ENV 2's too (DUELL) */
        return &ESYNC_DESC[id == P_E2ATK ? 3 : id == P_E2DEC ? 4 : 5];
    return &TP[id];
}

/* the descriptor of parameter id for engine e without the engine's desc hook (the device display
 * only; range and default are the same): what the editor protocol, user presets and projects use */
static const param_desc_t *param_desc_of(uint32_t e, uint32_t id)
{
    return id >= P_E0 ? &ENGINES[e]->edit[id - P_E0] : &TP[id];
}

/* a retired F_ENUM value kept as an alias, so stored values stay valid: DRUM KIT 1..3, once HAND CYM H+CYM, play 66 10 77
 * (eng_drum.c DK_PLAYS). (SAMPLE SET and GRAIN SRC had theirs until DUELL 0.2.9 retired both engines.)
 * It shows the original's name; knobs step over it and the editor's SET lands on the original. -> the value v
 * stands for */
static int32_t enum_orig(const param_desc_t *d, int32_t v)
{
    if (d->names == N_DRUM_KIT)
        return v >= 0 && v < DK_COUNT ? (int32_t)DK_PLAYS[v] : v;
    return v;
}

/* a stored value as the parameter takes it (projects, user presets, motion): inside d's range, and a retired
 * DRUM KIT (1..3) as the kit it plays, so KIT never holds one again. FM6's SLOT is never stored as a
 * bank slot: 9..40 (1.4.1's B1..B32; 1.0.2's B1..B27 were 8..34) is 8, OWN, and the patch comes with what is loaded */
static int32_t param_fit(const param_desc_t *d, int32_t v)
{
    v = clamp(v, d->min, d->max);
    if (d->names == N_FM6_PATCH)
        return v > (int32_t)FM6_OWN ? (int32_t)FM6_OWN : v;
    return d->names == N_DRUM_KIT ? enum_orig(d, v) : v;
}

/* v is a value a knob passes over: an alias, or one of FM6's bank slots without a voice */
static int enum_skip(const param_desc_t *d, int32_t v)
{
    return enum_orig(d, v) != v || (d->names == N_FM6_PATCH && !fm6_slot_ok(v));
}
/* a knob moved an F_ENUM from `from` to v: past any value it skips in that direction; with none left to the end, the
 * last one it does not skip on the way from `from` (else `from`) */
static int32_t enum_step(const param_desc_t *d, int32_t from, int32_t v)
{
    int32_t dir = v > from ? 1 : -1, w = v;
    while (w != from && enum_skip(d, w))
        w = w + dir > d->max || w + dir < d->min ? from : w + dir;
    while (w == from && v != from && enum_skip(d, v))
        v -= dir;
    return w == from ? v : w;
}

/* #48: the note divisions in the order of their length, longest first (triplets between their neighbours), on the
 * knobs and the gauges; the stored values (N_DIV indices: projects, presets, the editor protocol) stay.
 * -> the shown order of d's values (index: position, entry: value), 0 = the values' own order */
static const uint8_t DIV_ORDER[17] = {9, 8, 7, 13, 6, 14, 11, 0, 15, 10, 1, 16, 4, 2, 5, 3, 12};
/* 4BAR 2BAR 1/1 1/2D 1/2 1/4D 2T 1/4 1/8D 4T 1/8 1/16D 8T 1/16 16T 1/32 32T (by length) */
static const uint8_t LSYNC_ORDER[18] = {0, 1, 2, 3, 14, 4, 15, 12, 5, 16, 11, 6, 17, 7, 8, 9, 10, 13};   /* OFF, then the same */
/* the matrix lists, grouped on the knob (stored indices stay: the later ones were appended) */
static const uint8_t MSRC_ORDER[15] = {0, 2, 11, 1, 12, 3, 4, 5, 6, 7, 8, 9, 10, 13, 14};   /* OFF AENV ENV2 LFO1 LFO2 VEL..SLEW MORPH */
static const uint8_t MDST_ORDER[46] = {0, 1, 2, 38, 3, 4, 12, 13, 14, 15, 16, 17, 18, 19, 23, 5, 11, 10, 20, 21, 22, 32, 33, 34, 35,
                                       36, 37, 6, 24, 25, 26, 27, 28, 29, 30, 31, 7, 8, 9, 39, 40, 41, 42, 43, 44, 45};
/* (0.2.7) OFF, the voice's PITCH CUT RES SHP AMP, the engine's E1..E8, LEVEL PAN, the LFOs' VIB RAT1 DEP1 RAT2 DEP2, ENV 2's
 * ATK2 .. E2SHP, the part's effects DIST INSERT (A B C MIX) EQ (LOW MID HIGH) DUCK, the sends CHO DLY REV, then the bus values */
_Static_assert(sizeof MDST_ORDER == NELEM(N_MDST) && sizeof MSRC_ORDER == NELEM(N_MSRC), "the matrix lists' knob order: every value once");
/* the arp's MODE: OFF, SEQ (appended last, shown second so it is not missed), then the rest as stored */
static const uint8_t AMODE_ORDER[16] = {0, 15, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
static const uint8_t *enum_order(const param_desc_t *d)
{
    return d->names == N_DIV ? DIV_ORDER : d->names == N_LSYNC ? LSYNC_ORDER
           : d->names == N_AMODE ? AMODE_ORDER : d->names == N_MSRC ? MSRC_ORDER : d->names == N_MDST ? MDST_ORDER : 0;
}
static int32_t enum_rank(const param_desc_t *d, int32_t v)   /* v's place in the shown order (+ min): the gauges */
{
    const uint8_t *o = enum_order(d);
    int32_t r;
    for (r = 0; o && r < d->max - d->min; r++)
        if (o[r] == v - d->min)
            break;
    return o ? r + d->min : v;
}
/* a knob turned `steps` (signed) from v: clamped, over the shown order, past aliases (enum_step) */
static int32_t param_turn(const param_desc_t *d, int32_t v, int32_t steps)
{
    const uint8_t *o = enum_order(d);
    if (o)
        return o[clamp(enum_rank(d, v) - d->min + steps, 0, d->max - d->min)] + d->min;
    if (d->names == ESYNC_NAMES) {                    /* ENV SYNC: a note value a step (the first value showing it) */
        int32_t k = clamp((int32_t)esync_idx(v) + steps, 0, ESYNC_N - 1);
        return (k * 128 + ESYNC_N - 1) / ESYNC_N;
    }
    return enum_step(d, v, clamp(v + steps, d->min, d->max));
}

static uint8_t fmt_named;        /* the last param_format was a name (F_ENUM, even "4"): ui_draw.c's digits do not roll it */

/* value string (<= 5 chars) and unit for a parameter value */
static void param_format(const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    *unit = "";
    fmt_named = d->fmt == F_ENUM;
    if (d == &GP[G_BPM] && song.g[G_CLOCK]) {       /* DUELL 1.3: an external clock sets the tempo; the TEMPO knob shows EXT */
        str_cpy(val, "EXT", 6);
        fmt_named = 1;
        return;
    }
    switch (d->fmt) {
    case F_PCT:                                     /* a 0..100 range (SWG) is its value, others a share of 127 */
        fmt_int(val, d->max == 100 ? v : (v * 100 + 63) / 127);
        *unit = "%";
        break;
    case F_BIPCT:
        fmt_int(val, v * 100 / 64);
        if (v > 0) {
            char t[8];
            fmt_int(t, v * 100 / 64);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "%";
        break;
    case F_TIME: {
        uint32_t ms10 = TIME_MS_X10[v & 127];
        if (ms10 < 100u) {
            fmt_fix(val, (int32_t)ms10, 1);
            *unit = "ms";
        } else if (ms10 < 10000u) {
            fmt_int(val, (int32_t)((ms10 + 5u) / 10u));
            *unit = "ms";
        } else {
            fmt_fix(val, (int32_t)(ms10 / 100u), 2);
            if (ms10 >= 100000u)
                fmt_fix(val, (int32_t)(ms10 / 1000u), 1);
            *unit = "s";
        }
        break;
    }
    case F_LFOHZ: {
        uint32_t h = LFO_HZ_X100[v & 127];
        if (h < 1000u)
            fmt_fix(val, (int32_t)h, 2);
        else
            fmt_fix(val, (int32_t)(h / 10u), 1);
        *unit = "Hz";
        break;
    }
    case F_CUTOFF: {
        uint32_t h = CUTOFF_HZ[v & 127];
        if (h < 1000u) {
            fmt_int(val, (int32_t)h);
            *unit = "Hz";
        } else {
            fmt_fix(val, (int32_t)(h / 100u), 1);
            *unit = "kHz";
        }
        break;
    }
    case F_DB:
        if (v <= 0) {
            str_cpy(val, "OFF", 6);
        } else {
            fmt_fix(val, LEVEL_DB_X10[v], 1);
            *unit = "dB";
        }
        break;
    case F_SEMI:
        fmt_int(val, v);
        if (v > 0) {
            char t[8];
            fmt_int(t, v);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "st";
        break;
    case F_ENUM:
        str_cpy(val, d->names[v < d->min ? d->min : v > d->max ? d->max : v], 6);
        if (d->unit)
            *unit = d->unit;
        break;
    case F_BPM:
        fmt_int(val, v);
        *unit = "BPM";
        break;
    case F_NOTE:
        str_cpy(val, N_NOTE[v % 12], 6);
        break;
    case F_ONOFF:
        str_cpy(val, N_ONOFF[v ? 1 : 0], 6);
        break;
    case F_STEPS:
        fmt_int(val, v);
        *unit = "STEP";
        break;
    case F_EQDB:                                    /* DUELL EQ: whole dB */
        fmt_int(val, v);
        if (v > 0) {
            char t[8];
            fmt_int(t, v);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "dB";
        break;
    case F_EQLO:
    case F_EQMID:
    case F_EQHI: {
        uint32_t h = (eq_hz_q8(d->fmt == F_EQLO ? 0u : d->fmt == F_EQMID ? 1u : 2u, v) + 128u) >> 8;
        if (h < 1000u) {
            fmt_int(val, (int32_t)h);
            *unit = "Hz";
        } else {
            fmt_fix(val, (int32_t)((h + 50u) / 100u), 1);
            *unit = "kHz";
        }
        break;
    }
    case F_FILT:                                    /* DUELL 0.2.9: the multimode filter's CUT, its cutoff (core.h UF_STEPS) */
        v = 40 + 396 * clamp(v, 0, UF_STEPS);          /* (its table index, Q8: between two entries) */
        v = CUTOFF_HZ[v >> 8] + (v < (127 << 8) ? ((CUTOFF_HZ[(v >> 8) + 1] - CUTOFF_HZ[v >> 8]) * (v & 255)) >> 8 : 0);
        if (v < 1000) {
            fmt_int(val, v);
            *unit = "Hz";
        } else {
            fmt_fix(val, v / 100, 1);
            *unit = "kHz";
        }
        break;
    case F_FMODE:                                   /* .. its MODE: the ends and the middle by name, a blend as the two */
        str_cpy(val, v <= 0 ? "LP" : v < 64 ? "LP>BP" : v == 64 ? "BP" : v < UF_HP ? "BP>HP" : "HP", 6);
        break;
    case F_DKNOTE:
        if (v <= 0)
            str_cpy(val, "ANY", 6);
        else {
            char t[8];
            fmt_int(val, v / 12 - 1);                     /* C4 = 60 (the value is the note; 0 = ANY, note 0 unused) */
            str_cpy(t, val, 6);
            str_cpy(val, N_NOTE[v % 12], 6);
            str_cpy(val + str_len(val), t, 6);
        }
        break;
    case F_DKHLD:
        fmt_int(val, v * 5);
        *unit = "ms";
        break;
    case F_DKRLS: {
        uint32_t ms = duck_rls_ms(v);
        if (ms < 1000u) {
            fmt_int(val, (int32_t)ms);
            *unit = "ms";
        } else {
            fmt_fix(val, (int32_t)(ms / 10u), 2);
            *unit = "s";
        }
        break;
    }
    case F_EQQ: {                                   /* 100 x 2^(v / 6) */
        static const uint8_t Q6[6] = {100, 112, 126, 141, 159, 178};
        int32_t a = v >= 0 ? v / 6 : -((5 - v) / 6), j = v - a * 6;
        fmt_fix(val, a >= 0 ? (int32_t)Q6[j] << a : (int32_t)Q6[j] >> -a, 2);
        break;
    }
    default:
        if (d->names) {                               /* F_INT with a 0-terminated name list: the range */
            uint32_t k = 0;                           /* split evenly over the names (engine desc hooks) */
            while (d->names[k])
                k++;
            str_cpy(val, d->names[(uint32_t)(clamp(v, d->min, d->max) - d->min) * k / (uint32_t)(d->max - d->min + 1)], 6);
        } else {
            fmt_int(val, v);
        }
        if (d->unit)
            *unit = d->unit;
        break;
    }
}

/* ------------------------------------------------------------ pages --- */
enum { FAM_HOME, FAM_ENV, FAM_LFO, FAM_FX, FAM_SCL, FAM_EDIT, FAM_GLO, FAM_SAVE, FAM_ARP, FAM_SEQ, FAM_TRK,
       FAM_COUNT };
enum { SC_TRACK, SC_GLOBAL, SC_ENGINE, SC_STEP, SC_TRK,   /* SC_TRK: the TRACKS page (ui_input.c tracks_edit) */
       SC_MORPH,         /* DUELL 4b: HOME's MORPH pages: no parameter, the morph's controls (ui_input.c morph_knob) */
       SC_MODE,          /* DUELL step 5: HOME's MODE page: the output MODE, no parameter (ui_input.c mode_knob) */
       SC_TEST,          /* DUELL 0.1-beta: GLO's TEST page (CPU test): no parameter, ui_input.c test_knob */
       SC_SCALE,         /* DUELL: the SCL button's first page, GLOBAL (core.h scale_g, root_g, offscale_g): K1 SCALE, K2 ROOT, K3 OFF-SCALE; no parameter id
                          * (ui_input.c scale_knob); page_global() below: the header lights both parts */
       SC_MIDI,          /* DUELL step 6: GLO's MIDI pages (all MIDI settings live here): MIDI (KNOB 1, 2 the parts' channels, KNOB 3 MIDI IN),
                          * SIDECHAIN (KNOB 1, 2 the selected part's DUCK SRC and NOTE: the track's own parameters), MIDI LEARN (KNOB 1 LEARN, KNOB 2 CLEAR):
                          * ui_input.c midi_knob */
       SC_PERF };        /* DUELL 0.2.5: the PERFORM mode's page (PERF_PAGE below; ui_perform.c perf_knob): no parameter id */
enum { GR_NONE, GR_ADSR, GR_LFO, GR_STEPS, GR_ARP, GR_SCALE, GR_FX, GR_ROLL, GR_BROWSE, GR_SLOTS, GR_USER /* (retired: no page) */, GR_TRK,
       GR_SLCR /* (retired: no page, no graph; kept so the ids after it hold) */, GR_MOD, GR_PATS, GR_TOOLS, GR_CHORD, GR_SLICES /* (retired with SLICE, 0.2.9: no page) */, GR_EVENTS,
       GR_FMOP, GR_FMOP2, GR_FMENV, GR_INS,
       GR_ENGINE,        /* GR_ENGINE: EDIT's first page: ENGINE, SOUND, FAV, INIT (SPEC 1.2) */   /* (GR_STEPS: SEQ TOOLS' knobs; GR_FMOP..: FM6's operator pages, ui_fm6op.c) */
       GR_MORPH, GR_MORPHSET,     /* DUELL 4b: HOME's MORPH (fader, AMOUNT, ROLL, UNDO) and MORPH SET (SMART ARP PITCH; 0.2.9: TARGET is ALGORITHM's BOTH) */
       GR_MODE,                   /* DUELL step 5: HOME's MODE (SPLIT / STEREO / LAYER, SPEC 5b.1) */
       GR_GLO, GR_MIDI, GR_MLEARN,   /* DUELL step 6: GLO (MUTE 1, MUTE 2, the parts' LEVELs and the level meters), MIDI (GR_MIDI: channels, MIDI IN), MIDI LEARN */
       GR_SCHAIN,                    /* DUELL 0.1: SIDECHAIN (the selected part's DUCK SRC / NOTE; was KNOB 3, 4 of the MIDI page) */
       GR_TEST,                      /* DUELL 0.1-beta: GLO's TEST page (the CPU test: multi, engines, patches, CPU load; ui_menu.c graph_test) */
       GR_TEMPO,                     /* DUELL 2026-10-12: GLO's TEMPO page (KNOB 1 TEMPO, 2 TAP, 3 SWG, 4 CLK: ui_input.c tempo_tap) */
       GR_PERF };                    /* DUELL 0.2.5: PERFORM's key map (ui_perform.c graph_perf) */

typedef struct {
    const char *title;
    uint8_t fam, scope, graph;
    uint8_t id[4];               /* param ids; 0xFF = empty slot */
} page_t;

#define TEMPO_TAP_SLOT 1u        /* TEMPO's TAP card: the knob that is no parameter (ui_input.c edit_param, tempo_tap; ui_draw.c) */

static const page_t PAGES[] = {
    {"AMP ENV", FAM_ENV, SC_TRACK, GR_ADSR, {P_ATK, P_DEC, P_SUS, P_REL}},   /* ENV 1, the amp's (titles: 11 characters at
                                                                             * most, ui_graph.c page_parts: so not "AMP ENV DEST") */
    {"AMP DEST", FAM_ENV, SC_TRACK, GR_NONE, {P_ED_PIT, P_ED_FLT, P_ED_SHP, P_ESYNC}},   /* PITCH CUTOFF SHAPE as LFO 1 / 2 DEST and the matrix's DEST list;
                                                                             * 1.5: ESYNC, ENV's times as note values */
    {"ENV 2", FAM_ENV, SC_TRACK, GR_ADSR, {P_E2ATK, P_E2DEC, P_E2SUS, P_E2REL}},   /* DUELL: ENV 2, the filter envelope (and a
                                                                             * matrix source); ESYNC above is both's */
    {"ENV 2 DEST", FAM_ENV, SC_TRACK, GR_NONE, {P_E2FLT, P_E2SHP, 0xFF, 0xFF}},   /* DUELL: ENV 2's own depths (PITCH: the matrix) */
    {"LFO 1", FAM_LFO, SC_TRACK, GR_LFO, {P_LWAVE, P_LRATE, P_LSYNC, P_LFADE}},   /* WAVE RATE SYNC FADE: the shape, the speed (SYNC replaces RATE) */
    {"LFO 1 TRIG", FAM_LFO, SC_TRACK, GR_LFO, {P_LTRIG, P_LPHASE, P_LPOL, 0xFF}},   /* how it starts: TRIG, the start PHS, POL */
    {"LFO 1 DEST", FAM_LFO, SC_TRACK, GR_NONE, {P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP}},
    {"LFO 2", FAM_LFO, SC_TRACK, GR_LFO, {P_L2WAVE, P_L2RATE, P_L2SYNC, P_L2FADE}},   /* DUELL: LFO 2, a matrix source */
    {"LFO 2 TRIG", FAM_LFO, SC_TRACK, GR_LFO, {P_L2TRIG, P_L2PHASE, P_L2POL, 0xFF}},
    {"LFO 2 DEST", FAM_LFO, SC_TRACK, GR_NONE, {P_L2PIT, P_L2FLT, P_L2SHP, 0xFF}},   /* DUELL: LFO 2's own depths (AMP: the matrix) */
    {"MATRIX 1-4", FAM_LFO, SC_TRACK, GR_MOD, {0xFF, P_M1SRC, P_M1DST, P_M1AMT}},   /* KNOB 1: the slot (mod_ui_slot) */
    {"MATRIX 5-8", FAM_LFO, SC_TRACK, GR_MOD, {0xFF, P_M5SRC, P_M5DST, P_M5AMT}}, /* DUELL: slots 5..8, the same layout */
    /* FAM_FX in the order the sound flows (fx.c mix_part): the part's DIST (FX page, KNOB 1) -> INSERT -> EQ -> DUCK -> level, pan and the
     * sends (FX page KNOBS 2-4), then the global buses side by side: CHORUS and the delay (DLY 1, 2) summed, the REVERB added last (fx_bus.c) */
    {"FX", FAM_FX, SC_TRACK, GR_FX, {P_DIST, P_CHOR, P_DLY, P_REV}},
    {"INSERT 1/2", FAM_FX, SC_TRACK, GR_INS, {P_ITYPE, P_IA, P_IB, P_IC}},   /* 1.5: the track's INSERT (fx.c) */
    {"INSERT 2/2", FAM_FX, SC_TRACK, GR_INS, {P_IMIX, 0xFF, 0xFF, 0xFF}},  /* .. its dry / wet */
    {"EQ 1/2", FAM_FX, SC_TRACK, GR_NONE, {P_EQLF, P_EQLG, P_EQHF, P_EQHG}},     /* DUELL (SPEC 5c.3): the part's EQ after its INSERT: LOW and HIGH (where, then how much), */
    {"EQ 2/2", FAM_FX, SC_TRACK, GR_NONE, {P_EQMF, P_EQMQ, P_EQMG, 0xFF}},   /* then the MID: HZ, Q, dB */
    {"DUCK 1/2", FAM_FX, SC_TRACK, GR_NONE, {P_DKSRC, P_DKRATE, P_DKDEP, 0xFF}},   /* DUELL (SPEC 5c.4): the part's sidechain; KNOB 2 is RATE / THR / NOTE by SRC (duck_ctx_id) */
    {"DUCK 2/2", FAM_FX, SC_TRACK, GR_NONE, {P_DKATK, P_DKRLS, P_DKCRV, 0xFF}},
    {"CHORUS", FAM_FX, SC_GLOBAL, GR_NONE, {G_CRATE, G_CDEPTH, 0xFF, 0xFF}},
    {"DELAY 1/2", FAM_FX, SC_GLOBAL, GR_NONE, {G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX}},
    {"DELAY 2/2", FAM_FX, SC_GLOBAL, GR_NONE, {G_PINGPONG, G_PINGSPREAD, G_DMRATE, G_DMDEP}},   /* DUELL: PING-PONG (STEREO / LAYER; fx_bus.c), the delay's MODULATION: RATE, DEPTH */
    {"REVERB", FAM_FX, SC_GLOBAL, GR_NONE, {G_RTYPE, G_RSIZE, G_RDAMP, 0xFF}},   /* TYPE: ROOM / SPRING / HALL / SHIM / RESO */
    /* SCL (the button printed SEL): page 1 SCALE is GLOBAL (K1 SCALE, K2 ROOT, K3 OFF-SCALE SNAP / PLAY: the instrument's, core.h), page 2 CHORD is the
     * SELECTED PART's (K1 CHRD, K2 VOIC, K3 TRN: chord.c). No page mixes the two kinds (owner rule) */
    {"SCALE", FAM_SCL, SC_SCALE, GR_SCALE, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"CHORD", FAM_SCL, SC_TRACK, GR_CHORD, {P_CHRD, P_VOIC, P_TRANS, 0xFF}},
    /* SPEC 1.2: EDIT's first sub-page is the ENGINE page (the engine is never hidden behind a hold), then the engine's */
    {"ENGINE", FAM_EDIT, SC_TRACK, GR_ENGINE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* KNOB 1 ENGINE, 2 SOUND, 3 FAV, 4 INIT */
    {"EDIT 1", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E0, P_E1, P_E2, P_E3}},
    {"EDIT 2", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E4, P_E5, P_E6, P_E7}},
    {"FILTER", FAM_EDIT, SC_TRACK, GR_NONE, {P_FREEA, P_FREEB, P_FREEC, P_FREED}},   /* the MULTIMODE filter (DUELL 0.2.9), on every engine (engine_t.ufilt): K1 CUT K2 RES K3 MODE K4 SLOPE;
                                                                             * the generic cutoff routes, CC74 / CC71 and the matrix's CUT / FRES drive it */
    {"VCF", FAM_EDIT, SC_TRACK, GR_NONE, {P_FTYPE, P_E4, P_E5, P_E7}},   /* ANALOG's OWN filter (1.5, #104): TYPE CUT RES KTR; only its own knobs move it (named VCF so no page but the multimode one is called FILTER) */
    {"TRIO FILTER", FAM_EDIT, SC_TRACK, GR_NONE, {P_E4, P_E5, P_E6, 0xFF}},   /* TRIO's own filter: MODE CUT RES (its EDIT 2 has them beside PW) */
    {"NOISE FILT", FAM_EDIT, SC_TRACK, GR_NONE, {P_E2, P_E3, 0xFF, 0xFF}},   /* NOISE's own filter: FREQ RES (its SRC page has them beside MODE COLR) */
    {"OP TUNE", FAM_EDIT, SC_TRACK, GR_FMOP, {0xFF, 0xFF, 0xFF, 0xFF}},    /* FM6 only: KNOB 1 the operator, then its */
    {"OP ENV", FAM_EDIT, SC_TRACK, GR_FMENV, {0xFF, 0xFF, 0xFF, 0xFF}},     /* bytes of the track's patch (ui_fm6op.c): */
    {"OP MODE", FAM_EDIT, SC_TRACK, GR_FMOP2, {0xFF, 0xFF, 0xFF, 0xFF}}, /* CRS FINE LEVEL / STG RATE LEVEL / MODE DTUN VEL */
    {"VOICE 1/2", FAM_EDIT, SC_TRACK, GR_NONE, {P_VOICE, P_GLIDE, P_GLMODE, P_PRIO}},
    {"VOICE 2/2", FAM_EDIT, SC_TRACK, GR_NONE, {P_DETUNE, P_SPRD, P_ALLOC, P_PAN}},   /* unison DTUNE and SPRD (#148) together, the poly ALLOC, the voice's PAN (MUTE: GLO, MIXER) */
    /* 1.2 (Discussion #153): a family's pages go round in this order, a tap of its button each (ui.c open_family) */
    /* DUELL (SPEC 1.3, step 6; reworked 2026-10-12): GLO's pages, in this order (the CPU TEST is not one of them: MENU > SYSTEM > TEST): the main page GLO (graph GR_GLO: KNOB 1 MUTE 1 and 2 MUTE 2,
     * each its own part (a twist right mutes it, left unmutes it, once per twist), KNOB 3, 4 LEVEL 1, 2; between the parts' columns their level
     * meters; the keys play notes: ui_layer.c glo_knob), then TEMPO (graph GR_TEMPO): KNOB 1 TEMPO (inert, EXT, with an external MIDI clock), KNOB 2 TAP
     * (TEMPO_TAP_SLOT: a detent either way is one tap), KNOB 3 SWG, KNOB 4 CLK (the clock source INT, USB or TRS; the tempo is edited nowhere else:
     * SELECT is the page knob), then SIDECHAIN (KNOB 1 SC SRC, 2 SC NOTE), then ALL the MIDI settings (none in MENU): MIDI (KNOB 1 P1 CH, 2 P2 CH,
     * 3 MIDI IN: OWN CH / SELECTED, 4 none) and MIDI LEARN (KNOB 1 LEARN, 2 CLEAR) */
    {"GLO", FAM_GLO, SC_GLOBAL, GR_GLO, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"TEMPO", FAM_GLO, SC_GLOBAL, GR_TEMPO, {G_BPM, 0xFF, G_SWING, G_CLOCK}},   /* K1 TEMPO, K2 TAP (TEMPO_TAP_SLOT), K3 SWG, K4 CLK (ui_input.c tempo_knob) */
    {"SIDECHAIN", FAM_GLO, SC_MIDI, GR_SCHAIN, {P_DKSRC, P_DKNOTE, 0xFF, 0xFF}},
    {"MIDI", FAM_GLO, SC_MIDI, GR_MIDI, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"MIDI LEARN", FAM_GLO, SC_MIDI, GR_MLEARN, {0xFF, 0xFF, 0xFF, 0xFF}},
    /* SAVE's taps (1.5, #173; names and order 2026-10-11, 2026-10-12): PATCHES (one list of the sounds; SAVE's first tap), PATTERNS (Felucca's PHRASES, with DUELL's slots P1..P16), MULTI
     * (DUELL: Felucca's PROJECT, sixteen multis M01..M16: SLOT, LOAD ERASE SAVE; project.c multi.c), TOOLS.
     * DUELL has no song mode: GLO keeps the TEMPO page, Felucca's SONG page is gone */
    {"PATCHES", FAM_SAVE, SC_GLOBAL, GR_BROWSE, {0xFF, 0xFF, 0xFF, 0xFF}},  /* DUELL: ONE list, factory sounds, user patches, empty places: K1 PATCH, K2 TYPE, K3 FAV, K4 ERASE
                                                                             * (ui.c pl_*, ui_input.c; replaced PRESETS and USER PATCHES) */
    {"PATTERNS", FAM_SAVE, SC_GLOBAL, GR_PATS, {0xFF, 0xFF, 0xFF, 0xFF}},   /* SLOT, LOAD ERASE SAVE (ui.c pat_load; the 16 slots
                                                                             * patslot.c); the tap after PRESETS */
    {"MULTIS", FAM_SAVE, SC_GLOBAL, GR_SLOTS, {G_SLOT, G_LOAD, 0xFF, G_SAVE}},   /* KNOB 1 the multi, then LOAD ERASE SAVE */
    {"TOOLS", FAM_SAVE, SC_GLOBAL, GR_TOOLS, {G_CLRSEQ, G_INITSND, 0xFF, 0xFF}},
    {"ARP 1/3", FAM_ARP, SC_TRACK, GR_ARP, {P_AMODE, P_AHOLD, P_ARATE, P_AGATE}},   /* MODE, HOLD (also REC outside the sequencer), the time: RATE, GATE */
    {"ARP 2/3", FAM_ARP, SC_TRACK, GR_NONE, {P_ASWING, P_APROB, P_AOCT, P_AORDER}},
    {"ARP 3/3", FAM_ARP, SC_TRACK, GR_NONE, {P_ATGT, 0xFF, 0xFF, 0xFF}},   /* DUELL: ARP TARGET (SPEC 2.3) */
    {"STEP", FAM_SEQ, SC_STEP, GR_ROLL, {0, 1, 2, 3}},
    {"AUTOMATION", FAM_SEQ, SC_TRACK, GR_EVENTS, {0xFF, 0xFF, 0xFF, 0xFF}},  /* the locks, events, CHANCE and RATCH as a
                                                                             * list, PLAY and CLEAR (ui_events.c) */
    /* HOME's pages, in this order, a tap of HOME (or a SELECT detent) each, then OVERVIEW (ui.home: the old HOME screen), then
     * the round again (ui.c home_tap, page_select). DUELL 3.5 (SPEC 3.5): MORPH is the main page (KNOB 1 the fader A..B, 2
     * ROLL A / B, 3 UNDO of the far end, 4 AMOUNT: twist gestures past a detent, ui_input.c morph_knob), then MORPH SET (KNOB 1
     * SMART, 2 ARP, 3 PITCH, 4 empty: 0.2.9, TARGET is ALGORITHM's BOTH), the output MODE (SPEC 5b.1: KNOB 1 SPLIT / STEREO / LAYER; core.h duell_mode, kept with
     * the settings: ui_input.c mode_knob, mode_settings), the MIXER (LEVEL PAN REV MUTE). No P_* id: the morph is not a
     * parameter */
    {"MORPH", FAM_TRK, SC_MORPH, GR_MORPH, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"MORPH SET", FAM_TRK, SC_MORPH, GR_MORPHSET, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"MODE", FAM_TRK, SC_MODE, GR_MODE, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"MIXER", FAM_TRK, SC_TRK, GR_TRK, {0, 1, 2, 3}},
    /* the matrix pages again as the ENV family's last two (after AMP ENV, AMP DEST, ENV 2, ENV 2 DEST): the same title, graph and ids as the LFO
     * family's MOD 1 / MOD 2, so ENV 2 is routable without the LFO button. Appended here so no earlier PAGES index moves; the family
     * order, the counter and the remembered page work by index + family, the help by title (the same hint), mod_ui_slot is shared */
    {"MATRIX 1-4", FAM_ENV, SC_TRACK, GR_MOD, {0xFF, P_M1SRC, P_M1DST, P_M1AMT}},
    {"MATRIX 5-8", FAM_ENV, SC_TRACK, GR_MOD, {0xFF, P_M5SRC, P_M5DST, P_M5AMT}},
};
#define NPAGES (sizeof(PAGES) / sizeof(PAGES[0]))
/* The CPU TEST screen (DUELL 0.2.2): on no button and in no family's round (not in PAGES: the GLO group is GLO, TEMPO, SIDECHAIN, MIDI, MIDI LEARN).
 * MENU > SYSTEM > TEST (ui.menu 3, ui_menu.c) shows it by making this the current page (ui.c page_over) while it draws and takes the knobs: the multi, each
 * part's engine and patch, the CPU meter; K1 MORPH, K2 ROLL, K3 LOAD MULTI, K4 RESET (ui_input.c test_knob) */
#if DUELL_TEST_MODE                                      /* developer builds only (DUELL_TEST_MODE); SC_TEST / GR_TEST stay in the enums so no id moves */
static const page_t TEST_PAGE = {"TEST", FAM_GLO, SC_TEST, GR_TEST, {0xFF, 0xFF, 0xFF, 0xFF}};
#endif
/* PERFORM (DUELL 0.2.5, SPEC "PERFORM mode"): a mode, not a page of a family's round. A long press of FX (or a key pressed with FX held)
 * shows it over the page the unit was on (ui.c cur_page: ui.perf); a tap of FX goes back to that page, HOME and every other page button
 * leave it for their own page. In FAM_FX (FX's LED lit), on no family's round (no page number, SELECT turns nothing): K1 MORPH, K2 SPACE,
 * K3 FILTER, K4 DEPTH (ui_perform.c perf_knob) */
static const page_t PERF_PAGE = {"PERFORM", FAM_FX, SC_PERF, GR_PERF, {0xFF, 0xFF, 0xFF, 0xFF}};
/* GLOBAL pages: they edit the instrument, not the selected part, so the header's part badges light BOTH parts equally (ui_draw.c badge_lit) and
 * the page says it. One flag for the header: the SCALE page and GLO's pages except SIDECHAIN (the selected part's DUCK SRC / NOTE) and TEST (the selected part's MORPH). A page
 * never mixes global and per-part controls. (Other global pages: CHORUS, DLY 1/2, REVERB, MODE, MULTIS, TEMPO: see the audit) */
static int page_global(const page_t *pg)
{
    return pg->scope == SC_SCALE || (pg->fam == FAM_GLO && pg->graph != GR_SCHAIN && pg->scope != SC_TEST);
}
static uint8_t mod_ui_slot;      /* the MOD pages: the matrix slot of the page (0..3: MOD 1..4, MOD 2 5..8) KNOB 2..4 edit */
/* a MOD page's first slot (0: MOD, 4: MOD 2) */
static uint32_t mod_page_base(const page_t *pg) { return (uint32_t)(pg->id[1] - P_M1SRC) / 3u; }

/* DUCK 1's KNOB 2 (the page lists P_DKRATE): the stored value of the SRC's own: PUMP RATE, PART THR, a CH's NOTE; each keeps its value */
static uint32_t duck_ctx_id(const track_t *t)
{
    return t->p[P_DKSRC] == 0 ? P_DKRATE : t->p[P_DKSRC] == 1 ? P_DKTHR : P_DKNOTE;
}

static const param_desc_t *page_desc(const page_t *pg, uint32_t slot, int16_t **valp)
{
    uint32_t id = pg->id[slot];
    if (id == 0xFFu) {
        *valp = 0;
        return 0;
    }
    if (pg->scope == SC_STEP || pg->scope == SC_TRK || pg->scope == SC_MORPH || pg->scope == SC_MODE || pg->scope == SC_TEST ||
        pg->scope == SC_SCALE || pg->scope == SC_PERF) {
        *valp = 0;
        return 0;
    }
    if (pg->scope == SC_GLOBAL) {
        *valp = &song.g[id];
        return &GP[id];
    }
    if (id == P_DKRATE)
        id = duck_ctx_id(TSEL);
    if (pg->graph == GR_MOD)                          /* SRC DST AMT of the slot shown */
        id += 3u * mod_ui_slot;
    *valp = &TSEL->p[id];
    return track_desc(TSEL, id);
}
