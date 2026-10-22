/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Parameter descriptors, formatting and the page table. */
static const char *const N_LWAVE[] = {"SIN", "TRI", "SAW", "SQR", "S&H"};
/* LFO 2 (1.2, #154). SYNC: OFF (RATE), or one cycle per note value of the tempo, longest first (LSYNC_DIV: its N_DIV);
 * TRIG: NOTE restarts it at PHS on a fresh phrase (as before), FREE never; POL: BI -1..+1 (as before), UNI 0..+1 */
static const char *const N_LSYNC[] = {"OFF", "4BAR", "2BAR", "1/1", "1/2", "1/4", "1/8", "8T", "1/16", "16T", "1/32",
                                       "4T", "2T", "32T", "2D", "4D", "8D", "16D"};   /* (append-only; the knob order: LSYNC_ORDER) */
static const uint8_t LSYNC_DIV[18] = {0, 9, 8, 7, 6, 0, 1, 4, 2, 5, 3, 10, 11, 12, 13, 14, 15, 16};
static const char *const N_DKSRC[] = {"PUMP", "PART", "MIDI", "OFF"};   /* DUCK SRC (0.2.22): MIDI is the SIDECHAIN page's device setting (a channel and a note per part: GLO > SIDECHAIN, core.h sc_midi); it was CH1..CH16 (stored 2..17) and NOTE.
                                                                  * 0.2.24 (owner): OFF (stored 3, the default of a new / INIT sound: the ducker does nothing, its other cards are hidden; the knob shows it first: DKSRC_ORDER). PUMP / PART / MIDI keep 0 / 1 / 2;
                                                                  * the sparse patch store keeps PUMP as its own default (upreset.c up_def), so a record of before that left SRC out still reads PUMP */
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
static const char *const N_DMODE[] = {"DIGI", "PING", "RAND", "OCT+"};   /* G_DMODE (fx_bus.c dly_ev): the card's value and the fade reasons, 5 characters at most. 0.2.16: 0 DIGITAL, 1 PING-PONG,
                                                         * 2 RANDOM, 3 OCT UP (not append-only: 0.2.14's DELAY / RAND / OCT+ records are refused, MU_VER 4 / PROJ_REV 2) */
/* the modes in full (the header says one when MODE is turned; the help and the editor use them; <= 9 characters) */
static const char *const N_DMODE_FULL[] = {"DIGITAL", "PING-PONG", "RANDOM", "OCT UP"};
static const char *const N_SLOPE[] = {"-", "12dB", "24dB"};   /* the multimode filter's SLOPE: 1, 2 (0: clamped to 1, no store holds it) */
/* The TWIN FILTER's stored values (ext.h; cloud job B, TWIN_FILTER_DESIGN_2 3.1, EXTENSION_BLOCK_PLAN 2A): TYPE on id 43, CUTOFF 2 on 106, RES 2 on 25, DRIVE on 26, BLEND on 101, and the
 * extension slots X0..X14 (XTP below). ANALOG's own filter TYPE (1.5, #104: LP / BP / HP) was retired: id 101 is BLEND, the engine runs its low-pass only (eng_analog.c) */
static const char *const N_TWTYPE[] = {"OFF", "LP", "BP", "HP", "NOTCH", "LADDER", "COMB"};   /* (append-only: stored; COMB is not offered on WAVE and PHYS) */
static const char *const N_TWROUTE[] = {"SERIES", "PARAL"};   /* PARAL: the card's value must fit 49 px (PARALLEL does not, FILTER_PAGES_PROPOSAL 2.6) */
static const char *const N_SUBOCT[] = {"-2", "-1", "0"};      /* stored 0..2 */
static const char *const N_SUBWAVE[] = {"SINE", "TRI", "SQUARE", "PULSE"};   /* (append-only) */
static const char *const N_CYMODE[] = {"LOOP", "ONCE"};
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
/* P_KTRK (id 27, DUELL 0.2.21) took the id of the retired QNT (OFF / SNAP / WHITE / SEQ; owner 2026-10-12: the OFF-SCALE KEYS setting is global, core.h
 * offscale_g). Every store writes QNT 0 (up_def, project.c) and 0 is KEY TRK off, so no stored sound changes. Its labels are gone */
/* chord keys (chord.c): OFF, the diatonic triad / seventh of the track's ROOT and SCALE on the key, fixed shapes */
static const char *const N_CHRD[] = {"OFF", "DIA3", "DIA7", "MAJ", "MIN", "DOM7", "MAJ7", "MIN7", "SUS4", "POW"};
static const char *const N_OFFSCALE[] = {"SNAP", "PLAY"};   /* core.h offscale_g: OS_SNAP, OS_PLAY (SCALE page K3) */
static const char *const N_VOIC[] = {"CLOSE", "OPEN", "INV1", "INV2", "+OCT"};   /* VC_CLOSE .. VC_BASS */
static const char *const N_VOICE[] = {"POLY", "MONO", "LEG", "UNI"};   /* V_POLY .. V_UNISON */
static const char *const N_GLMODE[] = {"RATE", "TIME"};
static const char *const N_PRIO[] = {"LAST", "LOW", "HIGH"};
static const char *const N_ORDER[] = {"NOTE", "PLAY"};
static const char *const N_CLOCK[] = {"INT", "USB", "TRS", "AUTO"};   /* (stored 0..2 as ever; 3 AUTO, DUELL 0.2.14: core.h CLK_AUTO) */
/* MIDI IN (seq.c midi_pch), GLO > MIDI K3: OWN CH (stored 0) = each part listens on its own channel (P1 CH, P2 CH; default 1 and 2), the others
 * are ignored / SELECTED (1) = every channel plays the part chosen with ALGORITHM (values 2..4, the former CH5-8 .. CH13-16 blocks, read as OWN CH) */
static const char *const N_ROUTE[] = {"OWN CH", "SELECTED"};   /* (was PARTS, SEL: the same stored 0, 1) */
static const char *const N_NOTE[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const N_BM[] = {"OFF", "40 Hz", "50 Hz", "63 Hz", "80 Hz", "100 Hz", "125 Hz", "160 Hz", "200 Hz", "250 Hz", "315 Hz", "400 Hz"};   /* G_BM (core.h, 0.2.19): BASS MONO, OFF or the corner below which the master is mono (fx.c BM_HZ) */
/* COMP's RATIO (core.h G_CR, 0.2.21): 0 is 1:1, the compressor not run at all, shown OFF (0.2.25 layout round, owner: RATIO is the COMPRESSOR page's K1, OFF its lowest value:
 * turning it up switches the compressor on; the stored values are unchanged, 0 = OFF as it was 1:1); the others are the ratio of the gain computer (fx.c CMP_SLOPE) */
static const char *const N_CRAT[] = {"OFF", "1.5:1", "2:1", "3:1", "4:1", "6:1", "8:1", "12:1", "INF"};
static const char *const N_SHP[] = {"UP", "DOWN"};   /* G_SHPITCH (core.h, 0.2.19): SHIM's return an octave up (as ever) or down (0.2.25, owner: BOTH removed; a stored BOTH reads UP) */
static const char *const N_RTYPE[] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};   /* G_RTYPE: the reverb bus's model (fx.c); SHIM, RESO ported from JIANT; PLATE (0.2.14) after the CHOMPI firmware's reverb */
static const char *const N_GO[] = {"--", "GO"};
/* the INSERT's types (1.5, fx.c IT_*; append-only: stored). Its three values A B C mean what the type says (ins_desc) */
static const char *const N_ITYPE[] = {"OFF", "SOFT", "HARD", "FOLD", "FUZZ", "CRUSH", "PHASR", "FLANG", "CHOR", "TAPE", "VINYL"};
/* CRUSH: BITS 1..16 (0.2.14: a name a detent, N_IBITS in ins_bits.h: the whole bit count of the depth that detent plays, 16 only
 * where it is clean) and RATE (the held sample's rate, fx.c INS_HOLD) over the range in 16 even parts (0-terminated) */
static const char *const N_IRATE[] = {"689", "919", "1.1k", "1.4k", "1.8k", "2.2k", "2.8k", "3.7k", "4.4k", "5.5k",
                                      "7.4k", "8.8k", "11k", "15k", "22k", "44k", 0};
/* modulation matrix (mod.c): sources, destinations (E1..E8 = P_E0..P_E7: shown with the engine's labels).
 * Append-only (stored values keep their meaning): S&H SLEW and DEPTH (#132) came after EXPR and E8 */
/* (DUELL: "AENV" is ENV 1, the AMP ENV: stored value 2, mod.c MS_ENV) */
static const char *const N_MSRC[] = {"OFF", "LFO1", "AENV", "VEL", "KEY", "RAND", "MODW", "AT", "EXPR", "S&H", "SLEW",
                                     "ENV2", "LFO2",   /* DUELL: ENV 2, LFO 2 (step 3) */
                                     "MORPH", "DUCK",          /* DUELL step 4b: the part's MORPH fader, 0..1 (mod.c MS_MORPH) */
                                     "CYCLE"};                 /* cloud job B: the CYCLE envelope (mod.c MS_CYCLE; job F) */
static const char *const N_MDST[] = {"OFF", "PITCH", "CUT", "SHP", "AMP", "PAN", "SAT", "CHO", "DLY", "REV", "RAT1",
                                     "VIB", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "DEP1",
                                     "RAT2", "DEP2",    /* DUELL: LFO 2's rate and depth (step 3) */
                                     /* DUELL 0.2.7 (mod.c MD_LEVEL ..; 0.2.19: named TRIM, it moves the PATCH TRIM): the part's TRIM, the INSERT's A B C (the MATRIX page shows its TYPE's
                                      * labels) and MIX, the EQ gains, DUCK DEPTH, ENV 2's times and depths, RES; then the bus values (mod.c mod_bus) */
                                     "TRIM", "INS A", "INS B", "INS C", "IMIX", "EQLOW", "EQMID", "EQHI", "DUCK",
                                     "ATK2", "DEC2", "SUS2", "REL2", "E2FLT", "E2SHP", "FRES",
                                     "CRT", "CDP", "FDBK", "DMDEP", "DMRAT", "RSIZE", "RDAMP",
                                     "FMODE",
                                     "AATK", "ADEC", "ASUS", "AREL",
                                     "CUT2", "FRES2", "FDRV", "FBLND", "SUBLV"};   /* cloud job B (mod.c MD_CUT2 ..; FILTER_PAGES_PROPOSAL 5.1's factory-sound names): filter 2's cutoff, RES, DRIVE, BLEND, the SUB LAYER's LEVEL */   /* T0229: the AMP ENV's times (mod.c MD_AATK ..; shown AMP / ATTACK ..) */          /* DUELL 0.2.25: the multimode filter's MODE (mod.c MD_FMODE; shown in full "MULTI FILTER MODE", the editor's "FILTER MODE") */
/* (a DST name is never an engine's EDIT label: the factory sounds' MOD routes name an engine slot by its label, RES = ANALOG's E6,
 * and tools/gen_duell_sounds.py takes an N_MDST name first; mod_test checks it) */
static const char *const N_ENGNAME[] = {"ANALOG", "-" /* DIGITAL: retired */, "PHASE", "LOFI", "-" /* SAMPLE: retired, 0.2.9 */, "VOICE",
                                             "TRIO", "WHEEL", "-" /* GRAIN: retired */, "PHYS", "-" /* DRUM: retired, 0.2.14 */, "NOISE", "FM6",
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
    [P_LEVEL] = PD("LVL", F_DB, 0, 127, 112),      /* 0.2.19: the MIXER level, unity (112 = 0 dB) by default; the sound's loudness is P_TRIM */
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
    [P_RES2] = PD("RES", F_PCT, -64, 127, 0),       /* TWIN FILTER RES 2 (id 25, was ROOT): 0..127 (TRIO's damping law) for LP BP HP NOTCH LADDER, FEEDBACK -64..+63 on COMB (tw_desc): the stored range is both. Default 0 (stored: nothing; a chosen type starts from RES 0) */
    [P_DRV2] = PD("DRIVE", F_PCT, 0, 127, 0),       /* TWIN FILTER DRIVE (id 26, was SCALE): 0 = not run */
    [P_KTRK] = PD("KTRK", F_KTRK, -63, 63, 32),    /* 0.2.21: the multimode filter's KEY TRK (core.h UF_KT), -100..+100 %; 0.2.22 (owner): the default is 32 (shown +51 %; 50 % is between 31 and 32): a new / INIT sound and the factory sounds track the keys, except the BASS, PERC and KICK ones, which set 0 (tools/gen_duell_sounds.py), and the STORED default stays 0 (upreset.c up_def) */
    [P_TRANS] = PD("TRN", F_SEMI, -24, 24, 0),
    [P_SLEN] = PD("LEN", F_STEPS, 1, NSTEP, 16),
    [P_SDIV] = PE("DIV", N_DIV, 2),
    [P_SSWING] = PD("SWG", F_PCT, 0, 100, 0),
    [P_SGATE] = PD("GATE", F_PCT, 1, 127, 64),
    [P_DIST] = PD("SAT", F_PCT, 0, 127, 0),
    [P_CHOR] = PD("CHO", F_PCT, 0, 127, 0),
    [P_DLY] = PD("DLY", F_PCT, 0, 127, 0),
    [P_REV] = PD("REV", F_PCT, 0, 127, 0),
    [P_VOICE] = PE("VCE", N_VOICE, 0),
    [P_GLIDE] = PD("GLD", F_TIME, 0, 127, 0),
    [P_GLMODE] = PE("GLMOD", N_GLMODE, 0),
    [P_PRIO] = PE("PRIO", N_PRIO, 0),
    [P_TWTYPE] = PE("TYPE", N_TWTYPE, 0),       /* TWIN FILTER TYPE (id 43, was ALLOC: on no page, never read since T0227): OFF = filter 2 not run, the sound as before */
    [P_DETUNE] = PD("DTUNE", F_INT, 0, 127, 40),
    [P_PAN] = PD("PAN", F_BIPCT, -64, 63, 0),
    [P_MUTE] = PE("MUTE", N_ONOFF, 0),
    /* the SLICER's ids (removed). The MULTIMODE FILTER (DUELL 0.2.9, voice.c uf_run; core.h UF_STEPS), on the engines with engine_t.ufilt
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
    [P_BLEND] = PD("BLEND", F_BIPCT, -64, 63, 0), /* TWIN FILTER BLEND (id 101, was ANALOG's filter TYPE): -64 filter 1 alone, 0 both, +63 filter 2 alone */
    [P_ESYNC] = PE("ESYNC", N_ONOFF, 0),          /* 1.5 (#175): OFF = ATK DEC REL in ms, as before (voice.c esync) */
    [P_EQHG] = PD("HIGH", F_EQDB, -12, 12, 0),
    [P_EQHF] = PD("HI HZ", F_EQHI, -24, 12, 0),   /* HIGH shelf's corner, 8 kHz at 0 (2 .. 16 kHz) */
    /* DUELL DUCK (SPEC 5c.4, fx.c duck_*): a note-on on CH (and NOTE) ducks the part's level; CH 0 (OFF) or DEPTH 0 = nothing */
    [P_DKCRV] = PE("CURVE", N_DKCRV, 1),           /* the RELEASE's shape: LIN (a straight line, the earlier curve), EXP (a share of what is left: a pump's release, the default since 0.2.5; the sparse patch store keeps LIN as its own default: upreset.c up_def) */
    [P_CUT2] = {"CUTOFF", F_INT, -63, 82, 0, 0, 0}, /* TWIN FILTER CUTOFF 2 (id 106, was the retired DUCK NOTE, 0.2.24 / 0.2.25's EQ switch): ONE stored value with two meanings (tw_desc): LINK on = OFFSET -63..+63 dial steps from filter 1's cutoff, LINK off = CUTOFF 0..82, on COMB TUNE -24..+24 semitones; the stored range is all of them */
    [P_DKDEP] = PD("DEPTH", F_PCT, 0, 100, 0),
    [P_DKATK] = {"ATK", F_INT, 1, 100, 5, 0, "ms"},
    [P_TRIM] = PD("TRIM", F_DB, 0, 127, 104),      /* 0.2.19: the PATCH TRIM (core.h), on the id of the legacy DUCK HOLD (0.2.5, never read); the old default level */
    [P_DKRLS] = PD("RLS", F_DKRLS, 0, 67, 36),
    [P_DKSRC] = PE("SRC", N_DKSRC, 3),             /* OFF (0.2.24, stored 3: the default), PUMP (stored 0: every beat of RATE), PART (the other part's level), MIDI (0.2.22: the notes of the SIDECHAIN page's channel and note for this part) */
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

/* The extension slots' descriptors (ext.h, EXTENSION_BLOCK_PLAN 2A; cloud job B): one for each of the NX slots, wide id 128 + k, beside TP[]. Ranges and defaults are the plan's; where it left a
 * default open this table picks (and the report says so): CYCLE MODE LOOP, RISE 10 and FALL 70 (ENV 2's ATK2 and DEC2 defaults: TP[P_E2ATK], TP[P_E2DEC]), RES 2 0.
 * Every label fits a 50 px card in the small font (tests/xvalues_test.c measures them with the unit's own font table); a longer name would break at a space or be cut.
 * The pages (job H) and the editor's layout generator (tests/editordump.c, job I) read this table: they need no other. A value read by other code through XV(t, k) */
static const param_desc_t XTP[NX] = {
    [X_ROUTE] = PE("ROUTE", N_TWROUTE, 0),         /* SERIES (filter 1 into filter 2) / PARAL(LEL): value text PARAL, 46 px in the medium font */
    [X_LINK] = PE("LINK", N_ONOFF, 1),             /* ON: filter 2's cutoff = filter 1's effective cutoff + OFFSET (CUTOFF 2); OFF: CUTOFF 2 is its own cutoff */
    [X_KTRK2] = PD("KTRK2", F_KTRK, -63, 63, 0),   /* filter 2's own KEY TRK, as P_KTRK's */
    [X_ENV2] = PD("ENV2", F_BIPCT, -64, 63, 0),    /* ENV 2 -> CUTOFF 2 (TUNE on COMB) */
    [X_LFO1] = PD("LFO1", F_BIPCT, -64, 63, 0),    /* LFO 1 -> CUTOFF 2 */
    [X_LFO2] = PD("LFO2", F_BIPCT, -64, 63, 0),    /* LFO 2 -> CUTOFF 2 */
    [X_SUBLVL] = PD("LEVEL", F_PCT, 0, 127, 0),    /* SUB LEVEL: 0 = not run */
    [X_SUBOCT] = PE("OCTAVE", N_SUBOCT, 1),        /* -2, -1, 0 stored 0..2; -1 */
    [X_SUBWAVE] = PE("WAVE", N_SUBWAVE, 0),
    [X_SUBHARM] = PD("HARM", F_PCT, 0, 127, 0),    /* SUB HARMONICS (the word is 9 letters: more than a card holds), 0 = not run */
    [X_CYMODE] = PE("MODE", N_CYMODE, 0),          /* LOOP (the cycling one) / ONCE */
    [X_CYRISE] = PD("RISE", F_TIME, 0, 127, 10),   /* the ATK time table; note values with SYNC on (xdesc) */
    [X_CYFALL] = PD("FALL", F_TIME, 0, 127, 70),   /* the DEC time table */
    [X_CYCURVE] = PD("CURVE", F_BIPCT, -64, 63, 0),
    [X_CYSYNC] = PE("SYNC", N_ONOFF, 0),           /* CYCLE's own SYNC switch (not ENV SYNC) */
    [NX - 1u] = PFREE,                             /* X15: spare */
};
static const param_desc_t XTP_SYNC[2] = {          /* CYCLE RISE / FALL with SYNC on: as ENV SYNC's ATK / DEC (ESYNC_DESC) */
    {"RISE", F_INT, 0, 127, 10, ESYNC_NAMES, 0},
    {"FALL", F_INT, 0, 127, 70, ESYNC_NAMES, 0},
};
/* the TWIN FILTER's one-value-two-meanings: CUTOFF 2 is filter 1's OFFSET while LINK is on, filter 2's own CUTOFF with LINK off, TUNE on COMB (semitones around the note); RES 2 is RES (0..127) or, on COMB,
 * FDBK (FEEDBACK, -64..+63). TP[P_CUT2] / TP[P_RES2] hold the range of all of them (what is stored and clamped); tw_desc gives the one that applies (a page card, the editor, the matrix) */
enum { TT_OFF, TT_LP, TT_BP, TT_HP, TT_NOTCH, TT_LADDER, TT_COMB };   /* P_TWTYPE (N_TWTYPE) */
static const param_desc_t TWD_OFFSET = {"OFFSET", F_DSEMI, -TW_OFF_MAX, TW_OFF_MAX, 0, 0, 0};   /* (1.0, owner: whole semitones, 3 octaves; the stored value past it is clamped by the filter and shown clamped) */
static const param_desc_t TWD_CUTOFF = {"CUTOFF", F_FILT, 0, UF_STEPS, UF_STEPS, 0, 0};
static const param_desc_t TWD_TUNE = {"TUNE", F_SEMI, -24, 24, 0, 0, 0};
static const param_desc_t TWD_RES = {"RES", F_PCT, 0, 127, 0, 0, 0};
static const param_desc_t TWD_FDBK = {"FDBK", F_BIPCT, -64, 63, 0, 0, 0};
static const param_desc_t SCALE_D = PE("SCL", N_SCALE, 0);     /* the instrument's SCALE and ROOT (core.h scale_g, root_g: the SCALE page, ui_input.c scale_knob): not parameters of a part, so no P_ id (25 and 26 are the TWIN FILTER's now) */
static const param_desc_t ROOT_D = PD("ROOT", F_NOTE, 0, 11, 0);
static const param_desc_t XFREE_D = PFREE;
/* extension slot k's descriptor for part t (CYCLE's RISE / FALL by its SYNC switch); t 0: the stored range alone */
static const param_desc_t *xdesc(const track_t *t, uint32_t k)
{
    if (k >= NX)
        return &XFREE_D;
    if (t && XV(t, X_CYSYNC) && (k == X_CYRISE || k == X_CYFALL))
        return &XTP_SYNC[k - X_CYRISE];
    return &XTP[k];
}
static const param_desc_t *tw_desc(const track_t *t, uint32_t id)
{
    int comb = t->p[P_TWTYPE] == TT_COMB;
    if (id == P_CUT2)
        return comb ? &TWD_TUNE : XV(t, X_LINK) ? &TWD_OFFSET : &TWD_CUTOFF;
    return comb ? &TWD_FDBK : &TWD_RES;
}
/* every common track parameter back to its default, the extension slots too (a sound load, INIT) */
static void ext_defaults(track_t *t)
{
    uint32_t k;
    for (k = 0; k < NX; k++)
        XV(t, k) = XTP[k].def;
}

/* the INSERT's A B C as its TYPE means them (TP's ranges and defaults): the drives DRIVE TONE LEVEL, CRUSH BITS RATE
 * LPF, the swept ones RATE DEPTH FDBK, TAPE WOW SAT TONE, VINYL DUST NOISE WARP; OFF TP's own */
#define PU(l, f, df, n, u) {l, f, 0, 127, df, n, u}
static const param_desc_t INS_DESC[5][3] = {
    {PU("DRIVE", F_PCT, 64, 0, 0), PU("TONE", F_CUTOFF, 96, 0, 0), PU("LEVEL", F_DB, 96, 0, 0)},
    {PU("BITS", F_INT, 99, N_IBITS, 0), PU("RATE", F_INT, 96, N_IRATE, "Hz"), PU("LPF", F_CUTOFF, 96, 0, 0)},
    {PU("RATE", F_LFOHZ, 64, 0, 0), PU("DEPTH", F_PCT, 96, 0, 0), PU("FDBK", F_PCT, 96, 0, 0)},
    {PU("WOW", F_PCT, 47, 0, 0), PU("SAT", F_PCT, 71, 0, 0), PU("TONE", F_CUTOFF, 110, 0, 0)},     /* TAPE (0.2.14) */
    {PU("DUST", F_PCT, 47, 0, 0), PU("NOISE", F_PCT, 47, 0, 0), PU("WARP", F_PCT, 17, 0, 0)},      /* VINYL (0.2.14) */
};
#undef PU
static const param_desc_t *ins_desc(int32_t type, uint32_t k)
{
    if (type <= 0 || type >= (int32_t)NELEM(N_ITYPE))
        return &TP[P_IA + k];
    return &INS_DESC[type < 5 ? 0 : type == 5 ? 1 : type < 9 ? 2 : type - 6][k];   /* (IT_SOFT .. IT_FUZZ, IT_CRUSH, the swept ones, TAPE, VINYL) */
}

static const param_desc_t GP[G_COUNT + 7u] = {
    [G_BPM] = PD("TEMPO", F_BPM, 40, 240, 120),
    [G_SWING] = PD("SWG", F_PCT, 0, 100, 0),
    [G_CLOCK] = PE("CLK", N_CLOCK, CLK_AUTO),   /* DUELL 0.2.14: AUTO by default (a fresh unit, ERASE DATA, RESTORE LAST OFF) */
    [G_TUNE] = PD("TUNE", F_TUNEHZ, TUNE_MIN, TUNE_MAX, TUNE_DEF),   /* 0.2.22: the frequency of A4 in 0.1 Hz (was cents); edited on MENU > AUDIO TUNE (menu_items.c MI_TUNE) */
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
    [G_DMRATE] = PD("MRATE", F_LFOHZ, 0, 80, 44),
    [G_DMDEP] = PD("MDEPTH", F_PCT, 0, 100, 0),
    /* MIDI IN's routing, edited on GLO > MIDI K3 (DUELL 0.1; MENU > MIDI > MIDI IN in 1.2..1.5, up to 1.1.5 GLO > SYSTEM ROUT),
     * stored with the project as before. (was "--": stored 0 = CH1-4, as MIDI IN always was) */
    [G_ROUTE] = PE("ROUT", N_ROUTE, 0),
    [G_DMODE] = PE("MODE", N_DMODE, DMD_DIGITAL),   /* 0.2.16 (on the id of the old, unused G_INFO): the delay's mode, DIGI(TAL) (the plain delay, as ever) / PING(-PONG) / RAND(OM) / OCT+ (UP) */
    [G_SLOT] = PD("SLOT", F_INT, 1, 16, 1),   /* DUELL: SAVE > MULTI's slot, M01..M16 (project.c multi.c) */
    [G_SHMOD] = PD("MOD", F_PCT, 0, 100, 0),   /* 0.2.19 (on the id of the old, never shown G_NAME): SHIM's drift of the return's pitch, 0 = off (the old SHIM, bit for bit) */
    [G_LOAD] = PE("LOAD", N_GO, 0),
    [G_SAVE] = PE("SAVE", N_GO, 0),
    [G_ENGSEL] = PE("ENG", N_ENGNAME, 0),
    [G_RLOCUT] = PD("LOCUT", F_LOCUT, 0, 127, 75),   /* 0.2.14 (on the id of the old, unused G_ENGGO): the reverb's low cut; 0.2.22: 20 Hz .. 2 kHz, the default 75 = 299 Hz (61 = 88 Hz up to 0.2.21) */
    [G_CLRSEQ] = PE("CLRSQ", N_GO, 0),
    [G_INITSND] = PE("INIT", N_GO, 0),
    /* the reverb's model on the REVERB page: the id of the old GM drum channel (G_DRCH, inert since 1.0) */
    [G_RTYPE] = PE("TYPE", N_RTYPE, 2),         /* HALL at power-on since 1.2 (ROOM before; projects keep theirs) */
    /* inert: they set the GM drum part (level, reverb send), which is gone (drums are the DRUM engine
     * on any part). On no page; kept so the ids and G_COUNT, which the project format and the
     * editor protocol depend on, do not move */
    [G_PINGSPREAD] = PD("SPREAD", F_PCT, 0, 100, 100),   /* DUELL: the id of the old GM drum part's level; 100 = full left / right, as PING-PONG ran */
    /* G_FREE26 (0.2.16): the delay's PING-PONG switch was here, it is the MODE PING-PONG now (G_DMODE); 0.2.19: SHIM's PITCH (G_SHPITCH) */
    [G_SHPITCH] = PE("PITCH", N_SHP, SHP_UP),
    [G_CT] = PD("THRESH", F_CTHR, 0, CMP_THR_N - 1u, CMP_THR_DEF),   /* 0.2.21: the master compressor (COMP page, song.cp[], a device setting): THRESH -40 .. 0 dB, default -17 */
    [G_CR] = PD("RATIO", F_CRAT, 0, 8, 0),                        /* .. 1:1 (off) .. INF; 1:1 is the default: nothing changes for anyone who does not use it */
    [G_CA] = PD("ATTACK", F_CATK, 0, CMP_ATK_N - 1u, CMP_ATK_DEF),   /* .. 0.1 .. 100 ms, 10 ms */
    [G_CL] = PD("RELEASE", F_CREL, 0, CMP_REL_N - 1u, CMP_REL_DEF),  /* .. 20 ms .. 2 s, 200 ms */
    [G_WD] = PD("WIDTH", F_PCT, 0, 200, WD_DEF),                  /* 0.2.23: the master stereo width (WIDTH page, song.ms[0], a device setting): 0 mono .. 100 as it is .. 200 %; shown as its value */
    [G_BM] = PD("BMONO", F_BMONO, 0, BM_N, BM_DEF),   /* 0.2.19: BASS MONO (the full name: the editor, the header, the help), the pseudo id past the stored globals (core.h G_BM): song.bm, MASTER K1; "OFF" or the Hz; 0.2.24: a unit setting, 200 Hz by default */
};

/* DELAY MODE (core.h G_DMODE): the mode the delay RUNS in as the UI knows it: the stored mode (a value past OCT UP: DIGITAL). 0.2.22: in DUEL too (SPLIT: two buses, each
 * a part's own side, all mono: DIGI and PING the plain delay, RAND and OCT+ the events, centred) */
static uint32_t dly_mode_ui(void)
{
    uint32_t m = (uint32_t)song.g[G_DMODE];
    return m >= DMD_N ? (uint32_t)DMD_DIGITAL : m;
}
/* 1: the delay has its MODE card in the output mode now (0.2.22: every mode, DUEL too; the FX summary asks it, ui_draw.c) */
static uint32_t dly_modes_here(void) { return 1; }
/* 1 while the delay RUNS in an event mode (RANDOM, OCT UP) as the UI knows it */
static uint32_t dly_ev_ui(void) { uint32_t m = dly_mode_ui(); return m == DMD_RAND || m == DMD_OCTUP; }
/* in RANDOM and OCT UP the MDEPTH card (G_DMDEP, the delay's modulation depth) is CHANCE: the share of the clock edges that fire an event (stored in the same
 * id: a DEPTH of 40 is a CHANCE of 40 %); RATE does nothing there (dly_mode_why) */
static const param_desc_t DMD_CHANCE = PD("CHANCE", F_PCT, 0, 100, 0);
static const param_desc_t *glob_desc(uint32_t id)
{
    return id == G_DMDEP && dly_ev_ui() ? &DMD_CHANCE : &GP[id];
}
/* why the delay's card for song.g[id] does nothing in this mode (<= 7 characters; 0: it works). Only what a selection makes impossible (owner rule):
 *  - DUEL (SPLIT: each part on its own side, all mono: PING is the plain delay there, the events are centred): SPREAD, "DUEL" (MODE is there, 0.2.22);
 *  - MRATE (the delay's modulation rate, fx_bus.c dly_mod: DIGITAL and PING-PONG only; the event modes have none) in RANDOM / OCT UP: the mode's name;
 *  - SPREAD (the width of the side: PING-PONG's repeats and the events') in DIGITAL: the plain delay has no side, "DIGI" */
static const char *dly_mode_why(uint32_t id)
{
    uint32_t m = dly_mode_ui();
    if (id == G_PINGSPREAD && duell_mode_req == DM_SPLIT)
        return "DUEL";
    if (id == G_DMRATE && dly_ev_ui())
        return N_DMODE[m];
    if (id == G_PINGSPREAD && m == DMD_DIGITAL)
        return N_DMODE[DMD_DIGITAL];
    return 0;
}

/* the stored MODE in full ("DIGITAL", "PING-PONG", "RANDOM", "OCT UP"; a value past that: DIGITAL): the header says it when MODE is turned */
static const char *dly_mode_full(void)
{
    uint32_t m = (uint32_t)song.g[G_DMODE];
    return N_DMODE_FULL[m >= DMD_N ? (uint32_t)DMD_DIGITAL : m];
}

/* A switch of MODE (the card's turn, its reset, ui_input.c; was: the value before). Into RANDOM / OCT UP with CHANCE (G_DMDEP) at 0: CHANCE starts at DMD_CHANCE_DEF and
 * dmd_auto remembers that the unit set it (returns 1: the caller says it). Back to DIGITAL / PING-PONG with that value untouched: the DEPTH of the delay's modulation goes back to
 * 0, so the delay sounds as it did before the visit (the same stored value is both). A DEPTH that was not 0 is left as it is (it becomes the CHANCE, or stays the
 * modulation). Any turn of that card clears dmd_auto (ui_input.c). Not stored: after a restart the value stays as it is */
static uint8_t dmd_auto;
static uint32_t dly_mode_changed(int32_t was)
{
    uint32_t ev_now = song.g[G_DMODE] == DMD_RAND || song.g[G_DMODE] == DMD_OCTUP, ev_was = was == DMD_RAND || was == DMD_OCTUP;
    if (ev_now && !ev_was && song.g[G_DMDEP] == 0) {
        song.g[G_DMDEP] = DMD_CHANCE_DEF;
        dmd_auto = 1;
        return 1;
    }
    if (!ev_now && ev_was && dmd_auto && song.g[G_DMDEP] == DMD_CHANCE_DEF)
        song.g[G_DMDEP] = 0;
    if (!ev_now)
        dmd_auto = 0;
    return 0;
}

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
    if (!arp_mode(t) || k >= ATG_KINDS || v > (uint32_t)ATG_DESC[k].max)   /* (0.2.14: the arp as it plays now, MODULES' ARP OFF / SEQ OFF) */
        return 0;
    return ATG_BITS[k][v];
}

static const param_desc_t *track_desc(const track_t *t, uint32_t id)
{
    if (id >= P_COUNT)                                /* an extension slot (ext.h): XTP; 127, the reserved empty entry (and the retired SCALE ordinal of the keys block): a free id */
        return id >= XW_BASE ? xdesc(t, id - XW_BASE) : &XFREE_D;
    if (id == P_CUT2 || id == P_RES2)                 /* TWIN FILTER: one value, the meaning by LINK / TYPE */
        return tw_desc(t, id);
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
    return id >= XW_BASE ? xdesc(0, id - XW_BASE) : id >= P_COUNT ? &XFREE_D : id >= P_E0 ? &ENGINES[e]->edit[id - P_E0] : &TP[id];   /* (a wide id: the stored range, XTP; 127: free) */
}

/* a stored value as the parameter takes it (projects, user presets, motion): inside d's range. FM6's SLOT is never stored as a
 * bank slot: 9..40 (1.4.1's B1..B32; 1.0.2's B1..B27 were 8..34) is 8, OWN, and the patch comes with what is loaded */
static int32_t param_fit(const param_desc_t *d, int32_t v)
{
    v = clamp(v, d->min, d->max);
    if (d->names == N_FM6_PATCH)
        return v > (int32_t)FM6_OWN ? (int32_t)FM6_OWN : v;
    return v;
}

/* v is a value a knob passes over: one of FM6's bank slots without a voice */
static int enum_skip(const param_desc_t *d, int32_t v)
{
    return d->names == N_FM6_PATCH && !fm6_slot_ok(v);
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
static const uint8_t MSRC_ORDER[16] = {0, 2, 11, 15, 1, 12, 3, 4, 5, 6, 7, 8, 9, 10, 13, 14};   /* OFF AENV ENV2 CYCLE LFO1 LFO2 VEL..SLEW MORPH DUCK (CYCLE after ENV 2: the envelopes together) */
static const uint8_t MDST_ORDER[56] = {0, 1, 2, 38, 46, 51, 52, 53, 54, 3, 4, 55, 12, 13, 14, 15, 16, 17, 18, 19, 23, 5, 11, 10, 20, 21, 22, 47, 48, 49, 50, 32, 33, 34, 35,
                                       36, 37, 6, 24, 25, 26, 27, 28, 29, 30, 31, 7, 8, 9, 39, 40, 41, 42, 43, 44, 45};
/* (0.2.7) OFF, the voice's PITCH CUT RES (0.2.25: then FMODE, the filter's MODE; cloud job B: then filter 2's CUT 2, RES 2, DRIVE, BLEND) SHP AMP (then the SUB LEVEL), the engine's E1..E8, LEVEL PAN, the LFOs' VIB RAT1 DEP1 RAT2 DEP2, ENV 2's
 * ATK2 .. E2SHP, the part's effects DIST INSERT (A B C MIX) EQ (LOW MID HIGH) DUCK, the sends CHO DLY REV, then the bus values */
_Static_assert(sizeof MDST_ORDER == NELEM(N_MDST) && sizeof MSRC_ORDER == NELEM(N_MSRC), "the matrix lists' knob order: every value once");
/* the arp's MODE: OFF, SEQ (appended last, shown second so it is not missed), then the rest as stored */
static const uint8_t AMODE_ORDER[16] = {0, 15, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
static const uint8_t CLK_ORDER[4] = {CLK_AUTO, 0, 1, 2};   /* CLK as shown: AUTO INT USB TRS */
static const uint8_t DKSRC_ORDER[4] = {3, 0, 1, 2};        /* DUCK SRC as shown (0.2.24): OFF PUMP PART MIDI */
static const uint8_t *enum_order(const param_desc_t *d)
{
    return d->names == N_DIV ? DIV_ORDER : d->names == N_LSYNC ? LSYNC_ORDER
           : d->names == N_AMODE ? AMODE_ORDER : d->names == N_CLOCK ? CLK_ORDER : d->names == N_DKSRC ? DKSRC_ORDER : d->names == N_MSRC ? MSRC_ORDER : d->names == N_MDST ? MDST_ORDER : 0;
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

/* COMP's times: x / 1000 as "0.15", "1.5", "10" (a hundredth at most, no trailing zero) */
static void cmp_fmt_time(char *val, uint32_t x)
{
    uint32_t w = x / 1000u, f = x % 1000u / 10u, n;
    fmt_int(val, (int32_t)w);
    if (f) {
        n = str_len(val);
        val[n++] = '.';
        val[n++] = (char)('0' + f / 10u);
        if (f % 10u)
            val[n++] = (char)('0' + f % 10u);
        val[n] = 0;
    }
}
/* value string (<= 5 chars) and unit for a parameter value */
static void param_format(const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    *unit = "";
    fmt_named = d->fmt == F_ENUM;
    if (d == &GP[G_BPM] && clock_src()) {           /* DUELL 1.3: an external clock sets the tempo; the TEMPO knob shows EXT */
        str_cpy(val, "EXT", 6);
        fmt_named = 1;
        return;
    }
    switch (d->fmt) {
    case F_PCT:                                     /* a 0..100 range (SWG) is its value, others a share of 127 */
        fmt_int(val, d->max == 100 || d->max == 200 ? v : (v * 100 + 63) / 127);   /* (0.2.23: WIDTH's 0..200 too) */
        *unit = "%";
        break;
    case F_BIPCT:                                   /* 1.0 (owner): -100 at the bottom (-64), 0 in the middle, +100 at the top (+63): v * 100 / 63 above 0, v * 100 / 64 below (was / 64 both: +63 read +98) */
        fmt_int(val, v * 100 / (v > 0 ? 63 : 64));
        if (v > 0) {
            char t[8];
            fmt_int(t, v * 100 / 63);
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
            if (val[0] == '.' || (val[0] == '-' && val[1] == '.')) {   /* 0.2.19: unity is the mixer's default, "0.0", not ".0" (a leading zero under 1 dB) */
                char *d = val + (val[0] == '-');
                d[3] = 0;
                d[2] = d[1];
                d[1] = '.';
                d[0] = '0';
            }
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
    case F_DSEMI:                                   /* TWIN FILTER OFFSET (1.0, owner): WHOLE semitones, -36..+36 (a stored value past it shows what the filter does: clamped), "0" "+7" "-12" */
        v = v < -TW_OFF_MAX ? -TW_OFF_MAX : v > TW_OFF_MAX ? TW_OFF_MAX : v;
        fmt_int(val, v);
        if (v > 0) {
            char t[8];
            fmt_int(t, v);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "st";
        break;
    case F_KTRK:                                    /* KEY TRK: -63..+63 as -100..+100 % (63 = 100 %: an octave of cutoff per octave) */
        fmt_int(val, (v * 100 + (v > 0 ? 31 : v < 0 ? -31 : 0)) / 63);
        if (v > 0) {
            char t[8];
            fmt_int(t, (v * 100 + 31) / 63);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "%";
        break;
    case F_FMODE:                                   /* .. its MODE: the ends and the middle by name, a blend as the two */
        str_cpy(val, v <= 0 ? "LP" : v < 64 ? "LP>BP" : v == 64 ? "BP" : v < UF_HP ? "BP>HP" : "HP", 6);
        break;
    case F_LOCUT:                                   /* the reverb's LOW CUT: OFF, else the corner in Hz */
        if (v <= 0) {
            str_cpy(val, "OFF", 6);
        } else {
            if (LOCUT_HZ[v & 127] < 1000u) {            /* 0.2.22: from 1000 Hz in kHz ("1.0 kHz", as the filter's CUT) */
                fmt_int(val, LOCUT_HZ[v & 127]);
                *unit = "Hz";
            } else {
                fmt_fix(val, (int32_t)((LOCUT_HZ[v & 127] + 50u) / 100u), 1);
                *unit = "kHz";
            }
        }
        break;
    case F_TUNEHZ:                                  /* MASTER TUNE: A4 in Hz, one decimal ("440.0"; a value out of range reads 440.0) */
        fmt_fix(val, v >= TUNE_MIN && v <= TUNE_MAX ? v : TUNE_DEF, 1);
        *unit = "Hz";
        break;
    case F_CTHR:                                    /* COMP: THRESH in dB, -40 .. 0 */
        fmt_int(val, clamp(v, 0, (int32_t)CMP_THR_N - 1) - 40);
        *unit = "dB";
        break;
    case F_CRAT:                                    /* COMP: RATIO */
        str_cpy(val, N_CRAT[clamp(v, 0, 8)], 6);
        break;
    case F_CATK:                                    /* COMP: ATTACK, 0.1 .. 100 ms */
        cmp_fmt_time(val, CMP_ATK_US((uint32_t)clamp(v, 0, (int32_t)CMP_ATK_N - 1)));
        *unit = "ms";
        break;
    case F_CREL: {                                  /* COMP: RELEASE, 20 ms .. 2 s */
        uint32_t ms = CMP_REL_MS((uint32_t)clamp(v, 0, (int32_t)CMP_REL_N - 1));
        if (ms < 1000u) {
            fmt_int(val, (int32_t)ms);
            *unit = "ms";
        } else {
            cmp_fmt_time(val, ms);
            *unit = "s";
        }
        break;
    }
    case F_BMONO:                                   /* BASS MONO: OFF, else the corner in Hz */
        if (v <= 0 || v > (int32_t)BM_N) {
            str_cpy(val, "OFF", 6);
        } else {
            fmt_int(val, BM_HZ[v - 1]);
            *unit = "Hz";
        }
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
enum { SC_TRACK, SC_GLOBAL, SC_ENGINE, SC_STEP, SC_TRK,   /* SC_TRK: HOME's MIXER page (0.2.14 .. 0.2.25; retired: no page has it) */
       SC_MORPH,         /* DUELL 4b: HOME's MORPH pages: no parameter, the morph's controls (ui_input.c morph_knob) */
       SC_MODE,          /* DUELL step 5: HOME's MODE page: the output MODE, no parameter (ui_input.c mode_knob) */
       SC_TEST,          /* DUELL 0.1-beta: GLO's TEST page (CPU test): no parameter, ui_input.c test_knob */
       SC_SCALE,         /* DUELL: the SCL button's first page, GLOBAL (core.h scale_g, root_g, offscale_g): K1 SCALE, K2 ROOT, K3 OFF-SCALE; no parameter id
                          * (ui_input.c scale_knob); page_global() below: the header lights both parts */
       SC_MIDI,          /* DUELL step 6: GLO's MIDI pages (all MIDI settings live here): MIDI (KNOB 1, 2 the parts' channels, KNOB 3 MIDI IN),
                          * SIDECHAIN (KNOB 1, 2 the selected part's DUCK SRC and NOTE: the track's own parameters), MIDI LEARN (KNOB 1 LEARN, KNOB 2 CLEAR):
                          * ui_input.c midi_knob */
       SC_PERF,          /* DUELL 0.2.5: the PERFORM mode's page (PERF_PAGE below; ui_perform.c perf_knob): no parameter id */
       SC_MODS,          /* DUELL 0.2.14: GLO's MODULES page (core.h MODS_*: four device switches, ui.c mods_put; ui_input.c mods_knob): no parameter id */
       SC_FXSUM,
       SC_GMIX };        /* 0.2.25 layout round: GLO's MIXER page (both parts' LEVEL and PAN, the MUTE cells, the meters: ui_layer.c, as the hold-GLO layer): no parameter id */       /* DUELL 0.2.22: FX's last page, SEND FX (the SELECTED part's three send levels with the global effect they feed under them: K1 CHORUS, K2 DELAY,
                          * K3 REVERB; a twist jumps to that effect's page in GLO; ui_draw.c, ui_input.c fxsum_knob): no parameter id, nothing stored (the levels are P_CHOR, P_DLY, P_REV) */
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
       GR_PERF,                      /* DUELL 0.2.5: PERFORM's key map (ui_perform.c graph_perf) */
       GR_MODS,                      /* DUELL 0.2.14: GLO's MODULES (ARP SEQ SENDS PART FX ON / OFF; ui_graph.c graph_mods: a line each) */
       GR_MSTG,                      /* DUELL 0.2.23: GLO's WIDTH and MASTER (the scope; in DUEL a line that says why the cards are not there: ui_graph.c graph_mstg) */
       GR_FXSUM,
       GR_EQ,                        /* 0.2.25 layout round (owner): the part EQ's curve (EQ 1/2, 2/2; ui_graph.c graph_eq) */
       GR_COMP,                      /* .. COMPRESSOR's transfer curve and gain reduction (graph_comp) */
       GR_FILT,                      /* .. the filter's response: MULTI FILTER 1/2, 2/2 and the TWIN FILTER's three pages: both filters in one picture (graph_filt, twin_graph.c) */
       GR_SUB,                       /* .. SUB OSC: the sub's wave small, with its octave copies when HARM is up (ui_graph.c graph_sub) */
       GR_CYCLE };                   /* .. CYCLE ENV 1/2, 2/2: the cycle's shapes, the curve (ui_graph.c graph_cycle) */                   /* DUELL 0.2.24: FX's SEND FX (the hint that a twist jumps to the effect's GLO page, and which effect is inactive for this part: ui_graph.c graph_fxsum) */

typedef struct {
    const char *title;
    uint8_t fam, scope, graph;
    uint8_t id[4];               /* param ids; 0xFF = empty slot */
} page_t;

#define TEMPO_TAP_SLOT 2u        /* TEMPO's TAP card: the knob that is no parameter (ui_input.c edit_param, tempo_tap; ui_draw.c) */

/* The title of GLO's master-stage page (header, tests, help): "BASS MONO" while that was its only card (the owner, 2026-10-16); 0.2.24 (owner): the SATURATOR's DRIVE joined it
 * and the page was "MASTER"; 0.2.25 (owner): the saturator is removed, the page keeps the name MASTER with its one card, BASS MONO (the editor has MASTER_TITLE in web/editor/layout.js) */
#define MASTER_PAGE_TITLE "STEREO MASTER"   /* 0.2.25 layout round (owner): BASS MONO and WIDTH on one page, this exact title */
static const page_t PAGES[] = {
    {"AMP ENV", FAM_ENV, SC_TRACK, GR_ADSR, {P_ATK, P_DEC, P_SUS, P_REL}},   /* ENV 1, the amp's (titles: 11 characters at
                                                                             * most, ui_graph.c page_parts: so not "AMP ENV DEST") */
    {"AMP DEST", FAM_ENV, SC_TRACK, GR_ADSR, {P_ED_PIT, P_ED_FLT, P_ED_SHP, P_ESYNC}},   /* PITCH CUTOFF SHAPE as LFO 1 / 2 DEST and the matrix's DEST list;
                                                                             * 1.5: ESYNC, ENV's times as note values */
    {"ENV 2", FAM_ENV, SC_TRACK, GR_ADSR, {P_E2ATK, P_E2DEC, P_E2SUS, P_E2REL}},   /* DUELL: ENV 2, the filter envelope (and a
                                                                             * matrix source); ESYNC above is both's */
    {"ENV 2 DEST", FAM_ENV, SC_TRACK, GR_ADSR, {P_E2FLT, P_E2SHP, 0xFF, 0xFF}},   /* DUELL: ENV 2's own depths (PITCH: the matrix) */
    {"LFO 1", FAM_LFO, SC_TRACK, GR_LFO, {P_LWAVE, P_LRATE, P_LSYNC, P_LFADE}},   /* WAVE RATE SYNC FADE: the shape, the speed (SYNC replaces RATE) */
    {"LFO 1 TRIG", FAM_LFO, SC_TRACK, GR_LFO, {P_LTRIG, P_LPHASE, P_LPOL, 0xFF}},   /* how it starts: TRIG, the start PHS, POL */
    {"LFO 1 DEST", FAM_LFO, SC_TRACK, GR_LFO, {P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP}},   /* 0.2.25 layout round (owner): the DEST pages draw their envelope's / LFO's curve as its first page (ui_graph.c env_of / lfo_is2) */
    {"LFO 2", FAM_LFO, SC_TRACK, GR_LFO, {P_L2WAVE, P_L2RATE, P_L2SYNC, P_L2FADE}},   /* DUELL: LFO 2, a matrix source */
    {"LFO 2 TRIG", FAM_LFO, SC_TRACK, GR_LFO, {P_L2TRIG, P_L2PHASE, P_L2POL, 0xFF}},
    {"LFO 2 DEST", FAM_LFO, SC_TRACK, GR_LFO, {P_L2PIT, P_L2FLT, P_L2SHP, 0xFF}},   /* DUELL: LFO 2's own depths (AMP: the matrix) */
    {"MATRIX 1-4", FAM_LFO, SC_TRACK, GR_MOD, {0xFF, P_M1SRC, P_M1DST, P_M1AMT}},   /* KNOB 1: the slot (mod_ui_slot) */
    {"MATRIX 5-8", FAM_LFO, SC_TRACK, GR_MOD, {0xFF, P_M5SRC, P_M5DST, P_M5AMT}}, /* DUELL: slots 5..8, the same layout */
    /* FAM_FX (0.2.21: only the PART's own pages) in the order the sound flows (fx.c mix_part): the part's DIST (FX page, KNOB 1) -> INSERT -> EQ -> DUCK -> level, pan and the
     * sends (FX page KNOBS 2-4); the global buses (CHORUS, the delay, the REVERB) are GLO's pages now, and the last page here, SEND FX, shows the part's three sends beside them */
    {"FX", FAM_FX, SC_TRACK, GR_FX, {P_DIST, P_CHOR, P_DLY, P_REV}},
    {"INSERT 1/2", FAM_FX, SC_TRACK, GR_INS, {P_ITYPE, P_IA, P_IB, P_IC}},   /* 1.5: the track's INSERT (fx.c) */
    {"INSERT 2/2", FAM_FX, SC_TRACK, GR_INS, {P_IMIX, 0xFF, 0xFF, 0xFF}},  /* .. its dry / wet */
    {"EQ 1/2", FAM_FX, SC_TRACK, GR_EQ, {P_EQLF, P_EQLG, P_EQMF, P_EQMG}},   /* DUELL (SPEC 5c.3): the part's EQ after its INSERT. 0.2.25 layout round (owner): no ON / OFF switch, each band's frequency then its gain:
                                                                             * K1 LOW FREQ, K2 LOW GAIN, K3 MID FREQ, K4 MID GAIN; the panel draws the curve of the three bands (ui_graph.c graph_eq) */
    {"EQ 2/2", FAM_FX, SC_TRACK, GR_EQ, {P_EQMQ, P_EQHF, P_EQHG, 0xFF}},       /* .. K1 MID Q, K2 HIGH FREQ, K3 HIGH GAIN (K4 none) */
    {"DUCK 1/2", FAM_FX, SC_TRACK, GR_NONE, {P_DKSRC, P_DKRATE, P_DKDEP, 0xFF}},   /* DUELL (SPEC 5c.4): the part's sidechain; KNOB 2 is RATE / THR / NOTE by SRC (duck_ctx_id) */
    {"DUCK 2/2", FAM_FX, SC_TRACK, GR_NONE, {P_DKCRV, P_DKATK, P_DKRLS, 0xFF}},   /* 0.2.18: the CURVE (a type) first, then the times */
    {"SEND FX", FAM_FX, SC_FXSUM, GR_FXSUM, {0xFF, 0xFF, 0xFF, 0xFF}},   /* 0.2.22 (owner): the FX family's LAST page, part-scoped (only the selected part's badge): K1 CHORUS, K2 DELAY, K3 REVERB show THIS part's send level (P_CHOR, P_DLY, P_REV: the FX page edits them) and what the global effect is (delay mode and time, reverb type and size); K4 no card. A twist of K1 / K2 / K3 jumps to GLO's CHORUS / DELAY 1/2 / REVERB (ui_input.c fxsum_knob), no value changes */
    /* SCL (the button printed SEL): page 1 SCALE is GLOBAL (K1 SCALE, K2 ROOT, K3 OFF-SCALE SNAP / PLAY: the instrument's, core.h), page 2 CHORD is the
     * SELECTED PART's (K1 CHRD, K2 VOIC, K3 TRN: chord.c). No page mixes the two kinds (owner rule) */
    {"SCALE", FAM_SCL, SC_SCALE, GR_SCALE, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"CHORD", FAM_SCL, SC_TRACK, GR_CHORD, {P_CHRD, P_VOIC, P_TRANS, 0xFF}},
    /* SPEC 1.2: EDIT's first sub-page is the ENGINE page (the engine is never hidden behind a hold), then the engine's */
    /* T0227 (owner): the ENGINE page changes the engine and starts its controls: K1 ENGINE, K2..K4 the engine's first three (card positions 0..2: engine_t.uiord);
     * no sound list, no FAV, no INIT here (the patches are PATCHES' and SAVE's, INIT SOUND is SAVE > TOOLS). EDIT 1 holds positions 3..6, EDIT 2 position 7, and only
     * on an engine that has one (ui.c page_visible). (Was: K1 ENGINE, K2 SOUND, K3 FAV, K4 INIT; EDIT 1 positions 0..3, EDIT 2 4..7) */
    {"ENGINE", FAM_EDIT, SC_ENGINE, GR_ENGINE, {0xFF, P_E0, P_E1, P_E2}},
    {"EDIT 1", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E3, P_E4, P_E5, P_E6}},
    {"EDIT 2", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E7, 0xFF, 0xFF, 0xFF}},
    /* the filters in signal order (0.2.21, owner): the SUB OSC, then the MULTIMODE filter (voice.c uf_run), on every engine (engine_t.ufilt); the engines' own filter pages (VCF on ANALOG, TRIO FILTER,
     * NOISE FILT) are gone since cloud job J: ANALOG's VCF and NOISE's filter are the MULTI FILTER, TRIO's is the TWIN FILTER's LP BP HP NOTCH */
    {"SUB OSC", FAM_EDIT, SC_TRACK, GR_SUB, {XW_BASE + X_SUBWAVE, XW_BASE + X_SUBOCT, XW_BASE + X_SUBLVL, 0xFF}},   /* SUB HARMONICS is disabled (owner, 2026-10-21): no HARM card, the page is WAVE, OCTAVE, LEVEL (X_SUBHARM stays reserved, its stored value is never played); cloud job H (owner-approved 2026-10-19): the SUB LAYER (job F), after the engine's own pages and before filter 1 (the voice chain's order: engine, SUB, filter 1, filter 2):
                                                                             * K1 WAVE (the type first), K2 OCTAVE, K3 HARM, K4 LEVEL (the mix last). Values in the extension slots (ext.h XV): no MIDI LEARN or parameter lock on these cards (their wide ids are not 7-bit) */
    {"MULTI FILTER 1/2", FAM_EDIT, SC_TRACK, GR_FILT, {P_FREEA, P_FREEB, P_FREEC, P_FREED}},   /* the MULTIMODE filter (DUELL 0.2.9): K1 CUT K2 RES K3 MODE K4 SLOPE (0.2.20, owner: back to this order);
                                                                             * the generic cutoff routes (on the engines with no filter of their own), CC74 / CC71 and the matrix's CUT / FRES drive it */
    {"MULTI FILTER 2/2", FAM_EDIT, SC_TRACK, GR_FILT, {P_KTRK, P_E2FLT, P_LD_FLT, P_L2FLT}},   /* 0.2.21: K1 KEY TRK (voice.c: the cutoff follows the note); 0.2.22: K2 ENV 2, K3 LFO 1, K4 LFO 2 (owner: no AMP ENV card) -> the cutoff:
                                                                             * the SAME stored values as the DEST pages' FLT cards (P_E2FLT, P_LD_FLT, P_L2FLT; page_desc gives them their names here). A depth connects two modules, so it is
                                                                             * reachable at both: on the multimode filter's page and on its source's DEST page (owner 2026-10-16). All four cards show on every engine since cloud job J */
    /* cloud job H (owner-approved 2026-10-19, FILTER_PAGES_PROPOSAL Option B): filter 2, the TWIN FILTER, after filter 1 in the order the sound flows. TWIN FILTER: K1 TYPE, K2 LINK, K3 OFFSET / CUTOFF / TUNE (tw_desc), K4 RES / FDBK;
     * TWIN MOD (skipped while TYPE is OFF: ui.c page_visible): KEY TRACK FILT ENV LFO 1 LFO 2 as MULTI FILTER 2/2; TWIN CHAIN (skipped while TYPE is OFF): ROUTE, DRIVE, BLEND, K4 empty. What hides: ui_draw.c card_hidden_vp */
    {"TWIN FILTER", FAM_EDIT, SC_TRACK, GR_FILT, {P_TWTYPE, XW_BASE + X_LINK, P_CUT2, P_RES2}},
    {"TWIN MOD", FAM_EDIT, SC_TRACK, GR_FILT, {XW_BASE + X_KTRK2, XW_BASE + X_ENV2, XW_BASE + X_LFO1, XW_BASE + X_LFO2}},
    {"TWIN CHAIN", FAM_EDIT, SC_TRACK, GR_FILT, {XW_BASE + X_ROUTE, P_DRV2, P_BLEND, 0xFF}},
    {"OP TUNE", FAM_EDIT, SC_TRACK, GR_FMOP, {0xFF, 0xFF, 0xFF, 0xFF}},    /* FM6 only: KNOB 1 the operator, then its */
    {"OP ENV", FAM_EDIT, SC_TRACK, GR_FMENV, {0xFF, 0xFF, 0xFF, 0xFF}},     /* bytes of the track's patch (ui_fm6op.c): */
    {"OP MODE", FAM_EDIT, SC_TRACK, GR_FMOP2, {0xFF, 0xFF, 0xFF, 0xFF}}, /* CRS FINE LEVEL / STG RATE LEVEL / MODE DTUN VEL */
    {"VOICE 1/2", FAM_EDIT, SC_TRACK, GR_NONE, {P_VOICE, P_PRIO, P_DETUNE, 0xFF}},   /* 0.2.18, top-down: how the voices are used (MODE, PRIORITY, DETUNE). T0227 (owner): VOICE ALLOCATION is no
                                                                             * control any more: the voices always rotate (voice.c voice_alloc; the stored P_ALLOC byte stays in the formats, ignored) */
    {"VOICE 2/2", FAM_EDIT, SC_TRACK, GR_NONE, {P_GLMODE, P_GLIDE, P_SPRD, P_TRIM}},   /* 0.2.18: the glide (GLMOD, GLD), then the stereo end: SPRD (#148); 0.2.19: and the sound's output last, its TRIM (the PATCH TRIM, stored with the
                                                                             * patch; the player's MIXER LEVEL is the MIXER page's K4). PAN, a mixer setting a sound load leaves alone (ui.c param_kept), left this page: it is the MIXER's K2 (and the editor's part header) */
    /* 1.2 (Discussion #153): a family's pages go round in this order, a tap of its button each (ui.c open_family) */
    /* DUELL (SPEC 1.3, step 6; reworked 2026-10-12, 2026-10-16): GLO's pages, in this order (the CPU TEST is not one of them: MENU > SYSTEM > TEST): TEMPO (graph GR_TEMPO): KNOB 1 CLK,
     * KNOB 2 TEMPO (inert, EXT, with an external MIDI clock), KNOB 3 TAP (TEMPO_TAP_SLOT: a detent either way is one tap), KNOB 4 SWG (the tempo is edited nowhere else: SELECT is the page knob);
     * (0.2.25 layout round: MIXER first, GLO's first page again: LEVEL 1, PAN 1, LEVEL 2, PAN 2, the MUTE cells and the meters, as the hold-GLO layer)
     * then the GLOBAL effects (0.2.21, they were FX's pages): CHORUS, DELAY 1/2, DELAY 2/2, REVERB, SHIMMER (only while the reverb TYPE is SHIM), STEREO MASTER (0.2.25: BASS MONO, WIDTH), COMPRESSOR;
     * then MODULES, then the MIDI-related pages together: SIDECHAIN (0.2.22: the ducker's MIDI setup, a device setting: KNOB 1 P1 CH, 2 P1 NOTE, 3 P2 CH, 4 P2 NOTE), MIDI (KNOB 1 P1 CH, 2 P2 CH, 3 MIDI IN: OWN CH / SELECTED, 4 none), MIDI LEARN (KNOB 1 LEARN, 2 CLEAR).
     * The old first page GLO (MUTE 1, 2, LEVEL 1, 2 and the meters, graph GR_GLO) left the round in 0.2.21 and came back in 0.2.25 as MIXER (owner); HOME's MIXER page is gone */
    {"MIXER", FAM_GLO, SC_GMIX, GR_GLO, {0xFF, 0xFF, 0xFF, 0xFF}},   /* 0.2.25 layout round (owner): the MIXER is GLO's FIRST page: K1 LEVEL 1, K2 PAN 1, K3 LEVEL 2, K4 PAN 2 (ui_layer.c glo_knob, glo_cards), the parts' MUTE cells and the
                                                                             * level meters (ui_layer.c layer_glo, drawn by draw_layer like the hold-GLO QUICK layer, the same layout); the mutes stay on OCT- / OCT+ with GLO held */
    {"TEMPO", FAM_GLO, SC_GLOBAL, GR_TEMPO, {G_CLOCK, G_BPM, 0xFF, G_SWING}},   /* 0.2.18, top-down: K1 CLK (the source decides whether the tempo is live), K2 TEMPO, K3 TAP (TEMPO_TAP_SLOT), K4 SWG */
    {"CHORUS", FAM_GLO, SC_GLOBAL, GR_NONE, {G_CRATE, G_CDEPTH, 0xFF, 0xFF}},
    {"DELAY 1/2", FAM_GLO, SC_GLOBAL, GR_NONE, {G_DMODE, G_DTIME, G_DFDBK, G_DCOLOR}},   /* 0.2.16, top-down: MODE first (DIGITAL, PING-PONG, RANDOM, OCT UP: fx_bus.c; in DUEL all mono, PING-PONG as DIGITAL), then the loop: TIME, FDBK, COLR */
    {"DELAY 2/2", FAM_GLO, SC_GLOBAL, GR_NONE, {G_PINGSPREAD, G_DMRATE, G_DMDEP, G_DMIX}},   /* SPREAD (PING-PONG, RANDOM, OCT UP), the delay's MODULATION RATE, DEPTH (CHANCE in RANDOM / OCT UP), MIX last */
    {"REVERB", FAM_GLO, SC_GLOBAL, GR_NONE, {G_RTYPE, G_RSIZE, G_RDAMP, G_RLOCUT}},   /* K4 LOCUT (0.2.14); TYPE: ROOM / SPRING / HALL / SHIM / RESO / PLATE */
    {"SHIMMER", FAM_GLO, SC_GLOBAL, GR_NONE, {G_SHPITCH, G_SHMOD, 0xFF, 0xFF}},   /* 0.2.19: SHIM's own page (ui.c page_visible: skipped by the page buttons and SELECT unless TYPE is SHIM): PITCH (UP, DOWN, BOTH), MOD */
    {MASTER_PAGE_TITLE, FAM_GLO, SC_GLOBAL, GR_MSTG, {G_BM, G_WD, 0xFF, 0xFF}},   /* 0.2.25 layout round (owner): STEREO MASTER, one page for the master stereo stages where WIDTH and MASTER were: K1 BASS MONO (where the low end goes mono:
                                                                             * BASS / MONO, two lines), K2 WIDTH (0..200 %, 100 = as it is; fx.c width_stage runs first, BASS MONO has the last word below its corner); the panel pictures the mono
                                                                             * region and the widening (ui_graph.c graph_mstg); not in DUEL (no cards, the panel says why) */
    {"COMPRESSOR", FAM_GLO, SC_GLOBAL, GR_COMP, {G_CR, G_CT, G_CA, G_CL}},   /* 0.2.21 (owner): the master compressor (fx.c comp_stage). 0.2.25 layout round (owner): K1 RATIO (OFF its lowest value: the stored 1:1, the stage not run;
                                                                             * turning it up switches it on), K2 THRESH, K3 ATTACK, K4 RELEASE; at OFF the other three have nothing to act on: their cards are not drawn; not in DUEL either
                                                                             * (ui_draw.c card_hidden_vp); the panel: the transfer curve and the gain reduction (ui_graph.c graph_comp) */
    {"MODULES", FAM_GLO, SC_MODS, GR_MODS, {0xFF, 0xFF, 0xFF, 0xFF}},   /* DUELL 0.2.14 (owner): K1 ARP, K2 SEQ, K3 SENDS, K4 PART FX, each ON / OFF (core.h MODS_*) */
    {"SIDECHAIN", FAM_GLO, SC_MIDI, GR_SCHAIN, {0xFF, 0xFF, 0xFF, 0xFF}},   /* 0.2.22 (owner): the sidechain's MIDI SETUP, a device setting (not in a patch or a multi): K1 P1 CH, K2 P1 NOTE, K3 P2 CH, K4 P2 NOTE (ui_input.c midi_knob, sc_*); DUCK 1/2, 2/2 keep SRC, RATE, DEPTH, ATK, RLS, CURVE */
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
    {"ARP 1/3", FAM_ARP, SC_TRACK, GR_ARP, {P_AMODE, P_AHOLD, P_AORDER, P_AOCT}},   /* 0.2.18, top-down: what it plays: MODE, HOLD (also REC outside the sequencer), ORD, OCT */
    {"ARP 2/3", FAM_ARP, SC_TRACK, GR_ARP, {P_ARATE, P_AGATE, P_ASWING, P_APROB}},   /* 0.2.25 layout round (owner): ARP 1/3 and 2/3 draw the MODE's pattern (ui_graph.c graph_arp) */   /* .. then when: RATE, GATE, SWG, PROB */
    {"ARP 3/3", FAM_ARP, SC_TRACK, GR_NONE, {P_ATGT, 0xFF, 0xFF, 0xFF}},   /* DUELL: ARP TARGET (SPEC 2.3) */
    {"STEP", FAM_SEQ, SC_STEP, GR_ROLL, {0, 1, 2, 3}},
    {"AUTOMATION", FAM_SEQ, SC_TRACK, GR_EVENTS, {0xFF, 0xFF, 0xFF, 0xFF}},  /* the locks, events, CHANCE and RATCH as a
                                                                             * list, PLAY and CLEAR (ui_events.c) */
    /* HOME's pages, in this order, a tap of HOME (or a SELECT detent) each, then
     * the round again (0.2.14: no OVERVIEW) (ui.c home_tap, page_select). DUELL 3.5 (SPEC 3.5): MORPH is the main page (KNOB 1 the fader A..B, 2
     * ROLL A / B, 3 UNDO of the far end, 4 AMOUNT: twist gestures past a detent, ui_input.c morph_knob), then MORPH SET (KNOB 1
     * SMART, 2 ARP, 3 PITCH, 4 empty: 0.2.9, TARGET is ALGORITHM's BOTH), the output MODE (SPEC 5b.1: KNOB 1 SPLIT / STEREO / LAYER; core.h duell_mode, kept with
     * the settings: ui_input.c mode_knob, mode_settings), the MIXER (MUTE PAN SPACE LEVEL: the mixer level). No P_* id: the morph is not a
     * parameter */
    {"MORPH", FAM_TRK, SC_MORPH, GR_MORPH, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"MORPH SET", FAM_TRK, SC_MORPH, GR_MORPHSET, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"MODE", FAM_TRK, SC_MODE, GR_MODE, {0xFF, 0xFF, 0xFF, 0xFF}},
    /* (0.2.25 layout round, owner: HOME's MIXER page is gone: the mixer is GLO's first page, MIXER, and the hold-GLO layer; SPACE is PERFORM's K2) */
    /* the matrix pages again as the ENV family's last two (after AMP ENV, AMP DEST, ENV 2, ENV 2 DEST): the same title, graph and ids as the LFO
     * family's MOD 1 / MOD 2, so ENV 2 is routable without the LFO button. Appended here so no earlier PAGES index moves; the family
     * order, the counter and the remembered page work by index + family, the help by title (the same hint), mod_ui_slot is shared */
    /* cloud job H (owner-approved 2026-10-19): the CYCLE envelope (job F) in the ENV family after ENV 2 DEST and before the matrix pages (the family goes round in table order, so the pages are put here and no earlier index moves).
     * CYCLE has five stored values: 1/2 MODE (LOOP, ONCE), SYNC (OFF / ON), RISE, FALL; 2/2 CURVE alone on K1 */
    {"CYCLE ENV 1/2", FAM_ENV, SC_TRACK, GR_CYCLE, {XW_BASE + X_CYMODE, XW_BASE + X_CYSYNC, XW_BASE + X_CYRISE, XW_BASE + X_CYFALL}},
    {"CYCLE ENV 2/2", FAM_ENV, SC_TRACK, GR_CYCLE, {XW_BASE + X_CYCURVE, 0xFF, 0xFF, 0xFF}},
    {"MATRIX 1-4", FAM_ENV, SC_TRACK, GR_MOD, {0xFF, P_M1SRC, P_M1DST, P_M1AMT}},
    {"MATRIX 5-8", FAM_ENV, SC_TRACK, GR_MOD, {0xFF, P_M5SRC, P_M5DST, P_M5AMT}},
};
#define NPAGES (sizeof(PAGES) / sizeof(PAGES[0]))
/* The CPU TEST screen (DUELL 0.2.2): on no button and in no family's round (not in PAGES: the GLO group is MIXER, TEMPO, CHORUS, DELAY 1/2, 2/2, REVERB, SHIMMER, STEREO MASTER, COMPRESSOR, MODULES, SIDECHAIN, MIDI, MIDI LEARN).
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
 * the page says it. One flag for the header: the SCALE page and GLO's pages (0.2.21: the global effects are GLO's now) (SIDECHAIN too: both parts' MIDI setup) except TEST (the selected part's MORPH). A page
 * never mixes global and per-part controls. (Other global pages: MODE, MULTIS: see the audit) */
static int page_global(const page_t *pg)
{
    return pg->scope == SC_SCALE || (pg->fam == FAM_GLO && pg->scope != SC_TEST);
}
static uint8_t mod_ui_slot;      /* the MOD pages: the matrix slot of the page (0..3: MOD 1..4, MOD 2 5..8) KNOB 2..4 edit */
/* a MOD page's first slot (0: MOD, 4: MOD 2) */
static uint32_t mod_page_base(const page_t *pg) { return (uint32_t)(pg->id[1] - P_M1SRC) / 3u; }

/* SEND FX (0.2.22): card k's stored send level (the selected part's) and the GLO page its twist jumps to (the page whose first card is that id) */
static const uint8_t FXSUM_ID[3] = {P_CHOR, P_DLY, P_REV};
static const uint8_t FXSUM_TO[3] = {G_CRATE, G_DMODE, G_RTYPE};
/* GLO > SIDECHAIN's NOTE card (0.2.22): the note that ducks the part, 0 = ANY, else the note (F_DKNOTE names it); the default is the GM kick */
static const param_desc_t SC_NOTE_D = PD("NOTE", F_DKNOTE, 0, 127, SC_NOTE_DEF);

/* 1 on FX > DUCK 1/2 while the SELECTED part's DUCK SRC is MIDI (0.2.22): K4 is the link card to GLO > SIDECHAIN, where the channel and note are set */
static uint32_t duck_midi_page(const page_t *pg)
{
    return pg->scope == SC_TRACK && pg->fam == FAM_FX && pg->id[0] == P_DKSRC && TSEL->p[P_DKSRC] == 2;
}
/* DUCK 1's KNOB 2 (the page lists P_DKRATE): the stored value of the SRC's own: PUMP RATE, PART THR; MIDI and OFF have none (the retired P_DKNOTE: a blank card); each keeps its value */
static uint32_t duck_ctx_id(const track_t *t)
{
    return t->p[P_DKSRC] == 0 ? P_DKRATE : t->p[P_DKSRC] == 1 ? P_DKTHR : 0xFFu;   /* (0.2.24: no card at all for MIDI / OFF: P_DKNOTE is the EQ switch now) */
}
/* DUCK SRC OFF (0.2.24): the ducker does nothing and its other cards (RATE / THR, DEPTH, ATK, RLS, CURVE) are hidden (ui_draw.c card_hidden_vp) */
static int duck_off(const track_t *t) { return t->p[P_DKSRC] == 3; }
/* DUCK SRC PART on ONE part only (job 3, the owner's rule): the part with SRC = PART is ducked by the other part's level, and the other part has NO ducker at all, its SRC
 * is OFF (3) and its cards are not drawn (ui_draw.c card_hidden_vp). So the keying part never reads the ducked part's key, and only one direction of the hand-over is left
 * (fx.c duck_part: the ducked part reads the key of the block before, from the block's snapshot). Where it is applied:
 *   duck_set_part(p)   part p's SRC was just set from outside (the knob, the editor SET, a patch load, UNDO): if it is PART, the other part goes OFF (the part set last wins)
 *   duck_src_norm()    both parts at once (a multi, a project, the first block after anything above that was missed; fx.c mix_events): the lower index keeps PART,
 *                      the other part is OFF. Idempotent: it writes only when the pair breaks the rule
 * MIDI CC and the matrix cannot set SRC (midi_control.c midi_cc_set refuses it; the matrix has no such destination), nor can a morph or motion */
static void duck_set_part(uint32_t p)
{
    if (p < 2u && trk[p].p[P_DKSRC] == 1)
        trk[p ^ 1u].p[P_DKSRC] = 3;
}
static void duck_src_norm(void)
{
    if (trk[0].p[P_DKSRC] == 1)
        trk[1].p[P_DKSRC] = 3;
    else if (trk[1].p[P_DKSRC] == 1)
        trk[0].p[P_DKSRC] = 3;
}
/* the part's ducker is taken away by the other part's PART (its SRC card and the other DUCK cards are not drawn) */
static int duck_forced(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk);
    return p < 2u && trk[p ^ 1u].p[P_DKSRC] == 1 && (t->p[P_DKSRC] != 1 || p > 0u);
}

/* the parameter id of card `slot` of page pg: the page table's, except EDIT 1 / EDIT 2 of an engine that shows its E slots in another order (engine_t.uiord, FM6:
 * SLOT first). The stored ids never move; only which card shows which */
static uint32_t pg_id(const page_t *pg, uint32_t slot)
{
    uint32_t id = pg->id[slot & 3u];
    if (pg->scope == SC_ENGINE && id >= P_E0 && id < P_E0 + 8u) {
        const uint8_t *o = ENGINES[TSEL->eng_req % NENGINES]->uiord;
        if (o)
            return P_E0 + o[id - P_E0];                        /* (card position id - P_E0 -> E index) */
    }
    return id;
}
/* MULTI FILTER 2/2 (0.2.21, 0.2.22): ENV 2's, LFO 1's and LFO 2's cutoff amounts are the DEST pages' P_E2FLT, P_LD_FLT, P_L2FLT (one stored value each, TP's "FLT"); here a card
 * names its source, so the page reads KTRK, ENV 2, LFO 1, LFO 2. Same range, format and default as TP's */
static const param_desc_t KT_FLT[3] = {{"ENV 2", F_BIPCT, -64, 63, 0, 0, 0}, {"LFO 1", F_BIPCT, -64, 63, 0, 0, 0}, {"LFO 2", F_BIPCT, -64, 63, 0, 0, 0}};
static const param_desc_t *page_desc(const page_t *pg, uint32_t slot, int16_t **valp)
{
    uint32_t id = pg_id(pg, slot);
    if (id == 0xFFu) {
        *valp = 0;
        return 0;
    }
    if (pg->scope == SC_STEP || pg->scope == SC_TRK || pg->scope == SC_MORPH || pg->scope == SC_MODE || pg->scope == SC_TEST ||
        pg->scope == SC_SCALE || pg->scope == SC_PERF || pg->scope == SC_MODS || pg->scope == SC_FXSUM) {
        *valp = 0;
        return 0;
    }
    if (pg->scope == SC_GLOBAL) {
        *valp = id == G_BM ? &song.bm : id >= G_WD ? &song.ms[0] : id > G_BM ? &song.cp[(id - G_CT) & 3u] : &song.g[id];   /* (G_BM, 0.2.19, G_CT .. G_CL, 0.2.21, G_WD, 0.2.23: past the stored globals) */
        return glob_desc(id);
    }
    if (id == P_DKRATE) {
        id = duck_ctx_id(TSEL);
        if (id == 0xFFu) {                            /* (SRC MIDI / OFF: no second card) */
            *valp = 0;
            return 0;
        }
    }
    if (pg->graph == GR_MOD)                          /* SRC DST AMT of the slot shown */
        id += 3u * mod_ui_slot;
    *valp = &TSEL->p[id];
    if (pg->scope == SC_TRACK && pg->id[0] == P_KTRK && (id == P_E2FLT || id == P_LD_FLT || id == P_L2FLT))
        return &KT_FLT[id == P_L2FLT ? 2u : id == P_LD_FLT ? 1u : 0u];
    return track_desc(TSEL, id);
}

/* T0227 unit label pass (0.2.25, owner; release_drafts/LAYOUT_PROPOSAL.md section F): the name a card SHOWS, where it is not the stored label. The stored labels stay
 * the keys (the icons, the tests, the factory sound tool, the editor's tables); only the screen and the header say these. The rule: the full word when it fits the card
 * (50 px), a two-word name that does not fit breaks into two lines at its space (ui_draw.c label_split: the value on the third line, no gauge), a single word that does
 * not fit keeps its accepted short form (THRESH, FREQ, ALLOC, FDBK, DAMP, POL, OCT, TRANSP, POS, PW, PERC, RES, Q). A "\n" is a break chosen here. One thing, one word:
 * DETUNE everywhere, KEY TRACK everywhere, CUTOFF for every filter's cutoff and depth, FILT ENV for the second (filter) envelope */
static const char *const LBL_SHOWN[][2] = {
    {"ATK", "ATTACK"}, {"DEC", "DECAY"}, {"SUS", "SUSTAIN"}, {"REL", "RELEASE"},
    {"ATK2", "ATTACK"}, {"DEC2", "DECAY"}, {"SUS2", "SUSTAIN"}, {"REL2", "RELEASE"},
    {"PIT", "PITCH"}, {"FLT", "CUTOFF"}, {"SHP", "SHAPE"}, {"ESYNC", "SYNC"},
    {"PHS", "PHASE"}, {"PHS2", "PHASE"}, {"RATE2", "RATE"}, {"WAVE2", "WAVE"}, {"FADE2", "FADE"}, {"SYNC2", "SYNC"},
    {"TRIG", "TRIGGER"}, {"TRIG2", "TRIGGER"}, {"POL2", "POL"},
    {"SAT", "DRIVE"}, {"CHO", "CHORUS"}, {"DLY", "DELAY"}, {"REV", "REVERB"},
    {"INSRT", "TYPE"}, {"LPF", "FILTER"},
    {"SRC", "SOURCE"}, {"THR", "THRESH"}, {"RLS", "RELEASE"},
    {"VOIC", "VOICING"}, {"TRN", "TRANSP"}, {"TRGT", "TARGET"}, {"ORD", "ORDER"},
    {"KTRK", "KEY TRACK"}, {"KTR", "KEY TRACK"}, {"TRK", "KEY TRACK"},
    {"KTRK2", "KEY TRACK"}, {"ENV2", "FILT ENV"}, {"LFO1", "LFO 1"}, {"LFO2", "LFO 2"},   /* the TWIN MOD page (XTP's stored labels are the keys): the same names as MULTI FILTER 2/2 */
    {"CUT", "CUTOFF"}, {"VCE", "MODE"}, {"PRIO", "PRIORITY"}, {"GLMOD", "GLIDE MODE"}, {"GLD", "GLIDE"}, {"SPRD", "SPREAD"},
    {"DTUNE", "DETUNE"}, {"DTN", "DETUNE"}, {"DTUN", "DETUNE"},
    {"SWG", "SWING"}, {"CLK", "CLOCK"}, {"COLR", "COLOR"}, {"CRT", "RATE"}, {"CDP", "DEPTH"},
    {"MRATE", "MOD RATE"}, {"MDEPTH", "MOD DEPTH"}, {"LOCUT", "LOW CUT"}, {"CLRSQ", "CLEAR"},
    {"LVL", "LEVEL"}, {"SCL", "SCALE"}, {"LEN", "LENGTH"}, {"DIV", "DIVISION"},
    /* the engines (EDIT 1 / EDIT 2 and their mode-dependent cards) */
    {"NOIS", "NOISE"}, {"DRV", "DRIVE"}, {"LINE", "MODE"},
    {"DCW", "WAVE\nSHAPING"}, {"ENV", "SHAPING\nENV"},                  /* PHASE (owner, 0.2.25) */
    {"CRSH", "CRUSH"}, {"SWP", "SWEEP"}, {"VIB", "VIBRATO"}, {"DCY", "DECAY"},
    {"VOWL 1", "VOWEL 1"}, {"VOWL 2", "VOWEL 2"}, {"BRTH", "BREATH"}, {"RAND", "RANDOM"},
    {"ROTR", "ROTARY"},
    {"STRC", "STRUCT"}, {"BRIT", "BRIGHT"}, {"ACC", "ACCENT"}, {"EXC", "MIX"}, {"CHRD", "CHORD"},
    {"DENS", "DENSITY"}, {"DRFT", "DRIFT"},
    {"ALG", "ALGO"}, {"FB", "FDBK"}, {"MLVL", "MOD LEVEL"}, {"MRAT", "MOD RATIO"}, {"MEG", "MOD ENV"}, {"VMOD", "VEL MOD"},
    {"CRS", "COARSE"}, {"STG", "STAGE"},                                 /* FM6's operator pages (ui_fm6op.c) */
};
static const char *label_shown(const char *l)
{
    uint32_t i;
    if (!l)
        return l;
    for (i = 0; i < NELEM(LBL_SHOWN); i++)
        if (str_eq(l, LBL_SHOWN[i][0]))
            return LBL_SHOWN[i][1];
    return l;
}
static const char *desc_shown(const param_desc_t *d)
{
    static const struct { uint8_t id; const char *name; } TS[] = {
        {P_TRIM, "PATCH\nLEVEL"}, {P_APROB, "CHANCE"}, {P_EQLF, "LOW\nFREQ"}, {P_EQLG, "LOW\nGAIN"}, {P_EQMF, "MID\nFREQ"}, {P_EQMG, "MID\nGAIN"},
        {P_EQMQ, "MID Q"}, {P_EQHF, "HIGH\nFREQ"}, {P_EQHG, "HIGH\nGAIN"}, {P_CHRD, "TYPE"},
    };
    uint32_t i;
    for (i = 0; i < NELEM(TS); i++)
        if (d == &TP[TS[i].id])
            return TS[i].name;
    if (d == &GP[G_SHMOD])
        return "DRIFT";
    if (d == &GP[G_BM])
        return "BASS\nMONO";
    if (d == &KT_FLT[0])                              /* MULTI FILTER 2/2: the filter envelope's cutoff depth */
        return "FILT ENV";
    if (d->names == N_FM6_PATCH)                      /* FM6's SLOT: where the voice comes from (the MULTIS page's SLOT stays) */
        return "SOURCE";
    if (d->names == N_TRIO_MODE)                      /* TRIO's filter MODE is its TYPE */
        return "TYPE";
    return label_shown(d->label);
}
/* the same in one line (b holds n): the header, AUTOMATION, MIDI LEARN */
static void desc_said(const param_desc_t *d, char *b, uint32_t n)
{
    uint32_t i;
    str_cpy(b, d && d->label ? desc_shown(d) : "", n);
    for (i = 0; b[i]; i++)
        if (b[i] == '\n')
            b[i] = ' ';
}
/* a page's title as the header shows it (the stored titles stay the keys: the help, the tests, the editor's tables). The second envelope is the FILTER envelope */
static const char *title_shown(const char *t)
{
    static const char *const TT[][2] = {
        {"ENV 2", "FILT ENV"}, {"ENV 2 DEST", "FILT DEST"}, {"SEND FX", "SENDS"},
        /* the engines' EDIT 1 / EDIT 2 titles */
        {"PHS", "OSC"}, {"MOTN", "MOTION"}, {"VOWL", "SPEECH"}, {"BARS", "DRAWBARS"}, {"EXCT", "EXCITER"}, {"SRC", "SOURCE"}, {"MOVE", "MOTION"},
    };
    uint32_t i;
    for (i = 0; i < NELEM(TT); i++)
        if (str_eq(t, TT[i][0]))
            return TT[i][1];
    return t;
}
