/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL BENCH: what each stage really costs on the unit (developer builds only: DUELL_BENCH=1). The normal build has none of it (core.h
 * DUELL_BENCH 0; felucca.c includes this file only then, in sections of its own placed after everything else: app.ld, app_hot.ld, app_ram.ld).
 * The owner's guide: docs/BENCH_GUIDE.md (and release_drafts/BENCH_STAGE0.md for this line). Its host twin: tests/bench_sim.c (runs this file
 * on the Mac: the list, the table, the log, and the host model's figure of every row, firmware/src/bench_model.h).
 *
 * Stage 0 of the automated bench suite (2026-10-19): the bench of wip/bench0226 (BENCH-A2's rows and screen) ported to the two-core line
 * (stage1a: cloud/base's per-part state and NOTES budget, RAMPLACE). BENCH-B1's synthetic core 1 rows are left out (two-core stage 1's
 * twocore.c replaces them with real part 2 jobs); its STRESS 10M row stays, on one core (K3). New in the log: the NOTES budget's numbers of each
 * part and what limits them, whether the row held the budget open, the RAM code check (DUELL_RAMPLACE builds) and the restart counter.
 *
 * MENU > SYSTEM > BENCH (the GUIDE row's place: menu_items.c; ui_guide.c guide_open opens it). K4 (turned right) or OCT+ starts it; then no hand is
 * needed: the rows of BENCH_SCN run one after the other, each
 *   STOP   STOP ALL (every voice faded, the effect lines flushed), 150 ms
 *   SET    the row's setting, at once (both parts: their sound from INIT and the engine's dearest factory sound, bench_model.h BENCH_DEAR; every
 *          global value at its default; the mode; the master stages; the modules), in the main loop as the UI's own loads. The NOTES budget
 *          (budget.c): the cost rows hold it open (bench_bopen: no numbers, every voice the VOICE CAP and the pool give, as the 0.2.25 bench
 *          measured), the M16 rows play with it as the owner does
 *   SETTLE 700 ms (and until a WAVE table build is done): the engine fades, the mode's dip, the tables
 *   NOTES  the row's notes from an internal generator (USB-MIDI events into the MIDI-in ring, on the parts' own channels; LAYER: part 1's
 *          channel, both parts sound them), 500 ms for the attacks
 *   MEASURE 3 s: the audio interrupt's own time per half (audio.c fm1_alnk0_irq: TIMER4 across the half less TIMER5 nested in it, the same
 *          figure as the CPU bar, ABOUT and the TEST screen: cpu_sum / cpu_n / cpu_pk / cpu_miss), as a share of the half's time (CPU_BUDGET_US):
 *          the average, the peak, the halves missed (over the half's time or overtaken by the DMA); the voices sounding (the average); the
 *          voices the overload rule cut (audio.c shed_count) and the overload guard's LIGHT FX (fx.c ovl_lite): the audio path is the normal
 *          build's, nothing of it is held off for the bench. At its end the budget's numbers (part_notes_shown, part_notes_reason: what the
 *          budget gives this setting, also when it was held open) and, in RAMPLACE builds, the RAM code compared with its flash image (ramplace.c)
 * then the next row. K1 turns the pages of the table (page 1: the state). OCT- stops a run (what was measured stays). Every other button,
 * knob and key does nothing while it runs (the keys are silenced: kb_boot_hold), and nothing of it reaches the flash: the settings' 2 s save
 * timer is held (ui.mset_dirty), the autosave waits 10 s of silence, which a run never has. The level: the notes play at velocity 64 and the
 * screen asks for MASTER low before the start; the bench does not touch MASTER. At the end the session as it was is put back (project_capture /
 * project_restore_runtime, the device settings the rows touched); a power cycle does the same from the last autosave. Nothing starts by itself
 * at power-on. The run's clock is ui_input (ui_input.c: bench_input, many times a frame, in the main loop's wait too).
 * Each row's line goes to the USB serial console as it is done ("BENCH,..." CSV text, tools/bench_log.py), and K2 on the BENCH screen sends
 * them all again. K3: STRESS 10M (the heavy 8 + 8 DUEL scene for 10 minutes, the budget open), not in the automatic list. */

#ifndef BENCH_FN                                       /* (felucca.c: the sections of their own) */
#define BENCH_FN
#define BENCH_RO
#define BENCH_BSS
#endif
#include "bench_model.h"                               /* the host model's figure of every row, the dearest sounds (tests/bench_sim.c writes it) */
#ifndef BENCH_TAG
#define BENCH_TAG "DEV"
#endif
#ifndef BENCH_BOOTS
#define BENCH_BOOTS felucca_dbg.boots                 /* the restart counter (main.c, .noinit: every start counts) */
#endif

/* ------------------------------------------------------------------ the rows --- */
enum { BG_IDLE, BG_VOICE, BG_PART, BG_FILT, BG_BUS, BG_MASTER, BG_SCENE, BG_M16, BG_C1, BG_STRESS, BG_N };
static const char *const BG_NAME[BG_N] BENCH_RO = {"IDLE", "VOICE", "PART", "FILTER", "BUS", "MASTER", "SCENE", "M16", "CORE1", "STRESS"};
typedef struct {
    const char *name;           /* the table's name (tests/bench_sim.c: it fits the column) */
    const char *ref;            /* the row it is compared with (its delta: this - ref), 0 none */
    const char *base;           /* tests/bench_baseline.txt's line of the same stage (the host bench's own delta), 0 none */
    uint8_t grp, mode;          /* BG_*, DM_* */
    uint8_t e[2];               /* engine + 1 (0: the part keeps its power-on sound and plays nothing) */
    uint8_t n[2];               /* notes asked for (LAYER: n[0] on part 1's channel; both parts sound them) */
    uint8_t ins, sat, eq, duck; /* INSERT type, SAT drive, EQ bands 0..3, DUCK on: part 1, both parts with `both` */
    uint8_t both;
    uint8_t cho, dly, rev;      /* sends of the sounding parts */
    uint8_t dmode, rtype, shp;  /* G_DMODE + 1, G_RTYPE + 1, G_SHPITCH + 1 (0: the default) */
    uint8_t shm;                /* G_SHMOD */
    uint8_t comp, width, bm;    /* COMP ratio index (0 off), WIDTH % (0: 100, neutral), BASS MONO (0 off, 1..BM_N) */
    uint8_t flt;                /* part 1's multimode filter: 0 as the sound has it, 1 open, 12 / 24: CUT 41 RES 40 LP at that slope */
    uint8_t sendsoff, m16;      /* GLO > MODULES SENDS OFF; the factory multi M16 STRESS TEST (n[0] keys) */
    uint16_t secs;              /* the measure, s (0: 3) */
} bench_scn_t;

#define E_ANALOG 1u
#define E_PHASE 3u
#define E_LOFI 4u
#define E_VOICE 6u
#define E_TRIO 7u
#define E_WHEEL 8u
#define E_PHYS 10u
#define E_NOISE 12u
#define E_FM6 13u
#define E_WAVE 15u
#define VX(nm, en, k, bs) {.name = nm, .ref = "IDLE ST", .base = bs, .grp = BG_VOICE, .mode = DM_STEREO, .e = {en, 0}, .n = {k, 0}}
#define PT(nm, bs, ...) {.name = nm, .ref = "ANALOG 1", .base = bs, .grp = BG_PART, .mode = DM_STEREO, .e = {E_ANALOG, 0}, .n = {1, 0}, __VA_ARGS__}
#define BS(nm, m, rf, bs, ...) {.name = nm, .ref = rf, .base = bs, .grp = BG_BUS, .mode = m, .e = {E_ANALOG, E_ANALOG}, .n = {1, 1}, __VA_ARGS__}
#define MS(nm, bs, ...) {.name = nm, .ref = "BUS ON ST", .base = bs, .grp = BG_MASTER, .mode = DM_STEREO, .e = {E_ANALOG, E_ANALOG}, .n = {1, 1}, __VA_ARGS__}
#define TYP .e = {E_ANALOG, E_ANALOG}, .ins = 6, .sat = 64, .eq = 1, .both = 1, .cho = 100, .dly = 100, .rev = 100, .dmode = DMD_DIGITAL + 1, \
            .rtype = 3, .comp = 4
#define HVY .e = {E_ANALOG, E_ANALOG}, .ins = 6, .sat = 64, .eq = 1, .both = 1, .cho = 100, .dly = 100, .rev = 100, .dmode = DMD_RAND + 1, \
            .rtype = 4, .shp = SHP_DOWN + 1, .shm = 50, .comp = 4, .width = 150, .bm = 5
#define SC(nm, m, k, ...) {.name = nm, .grp = BG_SCENE, .mode = m, .n = {k, k}, __VA_ARGS__}
static const bench_scn_t BENCH_SCN[] BENCH_RO = {
    /* the idle mixer and the buses: nothing playing, the power-on sounds */
    {.name = "IDLE ST", .base = "idle", .grp = BG_IDLE, .mode = DM_STEREO},
    {.name = "IDLE LAYER", .ref = "IDLE ST", .grp = BG_IDLE, .mode = DM_LAYER},
    {.name = "IDLE DUEL", .ref = "IDLE ST", .grp = BG_IDLE, .mode = DM_SPLIT},
    {.name = "NO FX ST", .ref = "IDLE ST", .grp = BG_IDLE, .mode = DM_STEREO, .sendsoff = 1},
    /* each engine: its dearest factory sound, 1 and 4 notes on part 1 (STEREO, no part stage, no send) */
    VX("ANALOG 1", E_ANALOG, 1, "voice/ANALOG"), VX("ANALOG 4", E_ANALOG, 4, 0),
    VX("FM6 1", E_FM6, 1, "voice/FM6"), VX("FM6 4", E_FM6, 4, 0),
    VX("PHASE 1", E_PHASE, 1, "voice/PHASE"), VX("PHASE 4", E_PHASE, 4, 0),
    VX("LOFI 1", E_LOFI, 1, "voice/LOFI"), VX("LOFI 4", E_LOFI, 4, 0),
    VX("VOICE 1", E_VOICE, 1, "voice/VOICE"), VX("VOICE 4", E_VOICE, 4, 0),
    VX("TRIO 1", E_TRIO, 1, "voice/TRIO"), VX("TRIO 4", E_TRIO, 4, 0),
    VX("WHEEL 1", E_WHEEL, 1, "voice/WHEEL"), VX("WHEEL 4", E_WHEEL, 4, 0),
    VX("PHYS 1", E_PHYS, 1, "voice/PHYS"), VX("PHYS 4", E_PHYS, 4, 0),
    VX("NOISE 1", E_NOISE, 1, "voice/NOISE"), VX("NOISE 4", E_NOISE, 4, 0),
    VX("WAVE 1", E_WAVE, 1, "voice/WAVE"), VX("WAVE 4", E_WAVE, 4, 0),
    /* the part stages, on one ANALOG voice */
    PT("INS SOFT", "insert/SOFT", .ins = 1), PT("INS HARD", "insert/HARD", .ins = 2), PT("INS FOLD", "insert/FOLD", .ins = 3),
    PT("INS FUZZ", "insert/FUZZ", .ins = 4), PT("INS CRUSH", "insert/CRUSH", .ins = 5), PT("INS PHASR", "insert/PHASR", .ins = 6),
    PT("INS FLANG", "insert/FLANG", .ins = 7), PT("INS CHOR", "insert/CHOR", .ins = 8), PT("INS TAPE", "insert/TAPE", .ins = 9),
    PT("INS VINYL", "insert/VINYL", .ins = 10), PT("SAT 64", "part/SAT", .sat = 64),
    PT("EQ 1 BAND", 0, .eq = 1), PT("EQ 2 BANDS", 0, .eq = 2), PT("EQ 3 BANDS", "part/EQ", .eq = 3), PT("DUCK PUMP", "part/DUCK", .duck = 1),
    /* the multimode filter, 4 WAVE voices: open, 12 dB, 24 dB */
    {.name = "FLT OPEN", .grp = BG_FILT, .mode = DM_STEREO, .e = {E_WAVE, 0}, .n = {4, 0}, .flt = 1},
    {.name = "FLT 12DB", .ref = "FLT OPEN", .grp = BG_FILT, .mode = DM_STEREO, .e = {E_WAVE, 0}, .n = {4, 0}, .flt = 12},
    {.name = "FLT 24DB", .ref = "FLT OPEN", .grp = BG_FILT, .mode = DM_STEREO, .e = {E_WAVE, 0}, .n = {4, 0}, .flt = 24},
    /* the buses: both parts one ANALOG voice, one effect fed at 100, against SENDS OFF; STEREO, then DUEL (a bus per part) */
    BS("BUS OFF ST", DM_STEREO, 0, 0, .sendsoff = 1),
    BS("BUS ON ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/nothing_fed_(HALL,_DIGITAL:_pow"),
    BS("CHORUS ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/CHORUS_(STEREO:_the_stereo_chor", .cho = 100),
    BS("DL DIGI ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/DIGITAL", .dly = 100, .dmode = DMD_DIGITAL + 1),
    BS("DL PING ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/PING-PONG", .dly = 100, .dmode = DMD_PING + 1),
    BS("DL RAND ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/RANDOM", .dly = 100, .dmode = DMD_RAND + 1),
    BS("DL OCT+ ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/OCT_UP", .dly = 100, .dmode = DMD_OCTUP + 1),
    BS("RV ROOM ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/ROOM", .rev = 100, .rtype = 1, .shp = SHP_UP + 1),
    BS("RV SPRING ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/SPRING", .rev = 100, .rtype = 2, .shp = SHP_UP + 1),
    BS("RV HALL ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/HALL", .rev = 100, .rtype = 3, .shp = SHP_UP + 1),
    BS("RV SHIM ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/SHIM", .rev = 100, .rtype = 4, .shp = SHP_UP + 1),
    BS("RV RESO ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/RESO", .rev = 100, .rtype = 5, .shp = SHP_UP + 1),
    BS("RV PLATE ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/PLATE", .rev = 100, .rtype = 6, .shp = SHP_UP + 1),
    BS("SHIM DN ST", DM_STEREO, "BUS OFF ST", "bus/STEREO/SHIM_PITCH_DOWN", .rev = 100, .rtype = 4, .shp = SHP_DOWN + 1),
    BS("BUS OFF DU", DM_SPLIT, 0, 0, .sendsoff = 1),
    BS("BUS ON DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/nothing_fed_(HALL,_DIGITAL:_pow"),
    BS("CHORUS DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/CHORUS_(STEREO:_the_stereo_chor", .cho = 100),
    BS("DL DIGI DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/DIGITAL", .dly = 100, .dmode = DMD_DIGITAL + 1),
    BS("DL PING DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/PING-PONG", .dly = 100, .dmode = DMD_PING + 1),
    BS("DL RAND DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/RANDOM", .dly = 100, .dmode = DMD_RAND + 1),
    BS("DL OCT+ DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/OCT_UP", .dly = 100, .dmode = DMD_OCTUP + 1),
    BS("RV ROOM DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/ROOM", .rev = 100, .rtype = 1, .shp = SHP_UP + 1),
    BS("RV SPRING DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/SPRING", .rev = 100, .rtype = 2, .shp = SHP_UP + 1),
    BS("RV HALL DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/HALL", .rev = 100, .rtype = 3, .shp = SHP_UP + 1),
    BS("RV SHIM DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/SHIM", .rev = 100, .rtype = 4, .shp = SHP_UP + 1),
    BS("RV RESO DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/RESO", .rev = 100, .rtype = 5, .shp = SHP_UP + 1),
    BS("RV PLATE DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/PLATE", .rev = 100, .rtype = 6, .shp = SHP_UP + 1),
    BS("SHIM DN DU", DM_SPLIT, "BUS OFF DU", "bus/DUEL/SHIM_PITCH_DOWN", .rev = 100, .rtype = 4, .shp = SHP_DOWN + 1),
    /* the master stages (STEREO: none runs in DUEL) */
    MS("COMP 4:1", "master/COMP", .comp = 4), MS("WIDTH 150", "master/WIDTH", .width = 150), MS("BASS MONO", "master/BASSMONO", .bm = 5),
    /* the scenes: both parts ANALOG's dearest sound, typical and heavy effects, 4 + 4 and 8 + 8 notes asked for (the pool has 8 voices) */
    SC("TYP LAY 4+4", DM_LAYER, 4, TYP), SC("TYP LAY 8+8", DM_LAYER, 8, TYP),
    SC("TYP ST 4+4", DM_STEREO, 4, TYP), SC("TYP ST 8+8", DM_STEREO, 8, TYP),
    SC("TYP DUEL 4+4", DM_SPLIT, 4, TYP), SC("TYP DUEL 8+8", DM_SPLIT, 8, TYP),
    SC("HVY LAY 4+4", DM_LAYER, 4, HVY), SC("HVY LAY 8+8", DM_LAYER, 8, HVY),
    SC("HVY ST 4+4", DM_STEREO, 4, HVY), SC("HVY ST 8+8", DM_STEREO, 8, HVY),
    SC("HVY DUEL 4+4", DM_SPLIT, 4, HVY), SC("HVY DUEL 8+8", DM_SPLIT, 8, HVY),
    /* M16 STRESS TEST (the factory multi, LAYER) with 1, 2, 3 keys */
    {.name = "M16 1 KEY", .grp = BG_M16, .mode = DM_LAYER, .n = {1, 1}, .m16 = 1},
    {.name = "M16 2 KEYS", .grp = BG_M16, .mode = DM_LAYER, .n = {2, 2}, .m16 = 1},
    {.name = "M16 3 KEYS", .grp = BG_M16, .mode = DM_LAYER, .n = {3, 3}, .m16 = 1},
    /* (not in the automatic list: K3 on the BENCH screen) the 8 + 8 heavy DUEL scene for 10 minutes, one core, the budget open (BENCH-B3's
     * STRESS 10M without core 1's load) */
    {.name = "STRESS 10M", .ref = "HVY DUEL 8+8", .grp = BG_STRESS, .mode = DM_SPLIT, .n = {8, 8}, HVY, .secs = 600},
};
#define BENCH_NSCN (sizeof BENCH_SCN / sizeof BENCH_SCN[0])
#define BENCH_NAUTO (BENCH_NSCN - 1u)                  /* (the STRESS row is the last: only K3 runs it) */
#define BR_NONE 0xFFu

/* ------------------------------------------------------------------ the results --- */
enum { RF_DONE = 1, RF_SKIP = 2, RF_C1 = 4, RF_C1FAIL = 8, RF_ABORT = 16 };
typedef struct {
    uint16_t avg, pk;           /* x10 % of the half */
    uint16_t miss, shed;        /* halves missed, voices cut by the overload rule */
    uint16_t vox;               /* voices sounding, x10 */
    uint8_t lfx;                /* the share of the time LIGHT FX was engaged, % */
    uint8_t flags;              /* RF_* */
    uint16_t c1, c1pk;          /* core 1: its busy share of the halves, x10 % (average, the longest) */
    uint16_t late;              /* halves core 1 had not started (still busy with the one before) */
    uint16_t over;              /* halves core 1 needed more than 80 % of */
    uint32_t c1units;           /* core 1: units of its load done a half, x100 */
    uint16_t c1holds;           /* core 1 parked for a flash access during the row (FLASH HOLDS) */
    uint8_t bn[2], bw[2];       /* the NOTES budget's number of each part (part_notes_shown) and what limits it (NR_*), at the measure's end */
    uint8_t bopen;              /* the row held the budget open (a cost row) */
    uint8_t rp;                 /* the RAM code check at the row's end: 0 OK, bit 0 range X, bit 1 range Z differ; 0xFF: no RAM code (not RAMPLACE) */
} bench_res_t;
static bench_res_t bench_res[BENCH_NSCN] BENCH_BSS;
static uint8_t bench_refi[BENCH_NSCN] BENCH_BSS;                 /* the ref row's index (bench_resolve) */
static uint8_t bench_dear[NENGINES] BENCH_BSS;                   /* the dearest factory sound of each engine (bench_model.h; tests/bench_sim.c may set it) */

enum { BS_IDLE, BS_STOP, BS_SET, BS_SETTLE, BS_ATTACK, BS_MEASURE, BS_END, BS_DONE };
static struct {
    uint8_t on;                 /* a run is going */
    uint8_t holds;              /* the sound is the bench's (until it is put back) */
    uint8_t phase;              /* BS_* */
    uint8_t row, last, first;   /* the row now; the run's last row; its first (STRESS: one row) */
    uint8_t page;               /* the page shown (0: the state) */
    uint8_t aborted, model_ok, done_once, dear_ok, persist;
    uint32_t t, t_run;          /* the phase's start, the run's (fm1_ms) */
    uint32_t n0, s0, m0, shed0; /* the meter's counters at the measure's start */
    uint32_t vsum, vn, lfn;     /* voices summed over the frames, frames, frames with LIGHT FX */
    uint8_t notes[16], nnotes, nsent, nch[16];
    uint32_t sig;               /* the screen's signature */
    uint32_t drawn;             /* when the screen was last drawn during a run (fm1_ms) */
} bench BENCH_BSS;
/* the session put back at the end */
static project_t bench_saved __attribute__((section(".pool.zbench")));
static struct {                                        /* the audio side's words the rows set (the UI's own syncs do not run while the BENCH screen is up) */
    uint32_t cmp;
    uint8_t wd, bm, mods, mode, bal, xf, split;
    int8_t space;
} bench_keep BENCH_BSS;

static BENCH_FN int32_t bench_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }   /* (core.h clamp is inlined
                                                       * everywhere in the normal build: a call from here would make it a function of its own) */
static BENCH_FN void bench_tl_tick(void);              /* (stage 1, below) */
static BENCH_FN void bench_sess_end(void);
static BENCH_FN uint32_t bench_secs(const bench_scn_t *s) { return s->secs ? s->secs : 3u; }
static BENCH_FN uint32_t bench_sig_n(uint32_t n)      /* FNV-1a of the first n rows' names: bench_model.h's BENCH_MODEL_SIG (all of BENCH-B's), _SIGA (BENCH-A's) */
{
    uint32_t i, h = 2166136261u;
    const char *c;
    for (i = 0; i < n && i < BENCH_NSCN; i++)
        for (c = BENCH_SCN[i].name; ; c++) {
            h = (h ^ (uint8_t)*c) * 16777619u;
            if (!*c)
                break;
        }
    return h;
}
static BENCH_FN uint32_t bench_find(const char *name)
{
    uint32_t i;
    for (i = 0; name && i < BENCH_NSCN; i++)
        if (str_eq(BENCH_SCN[i].name, name))
            return i;
    return BR_NONE;
}
static BENCH_FN void bench_resolve(void)
{
    uint32_t i;
    for (i = 0; i < BENCH_NSCN; i++)
        bench_refi[i] = (uint8_t)bench_find(BENCH_SCN[i].ref);
    if (!bench.dear_ok) {                              /* (once: tests/bench_sim.c may set its own after) */
        for (i = 0; i < NENGINES && i < sizeof BENCH_DEAR; i++)
            bench_dear[i] = BENCH_DEAR[i];
        bench.dear_ok = 1;
    }
    bench.model_ok = (BENCH_NSCN == BENCH_MODEL_N && bench_sig_n(BENCH_NSCN) == BENCH_MODEL_SIG) ||
                     (BENCH_NSCN == BENCH_MODEL_NA && bench_sig_n(BENCH_NSCN) == BENCH_MODEL_SIGA);   /* (the header is BENCH-B's list; BENCH-A's is its start) */
}
static BENCH_FN uint32_t bench_model(uint32_t i) { return bench.model_ok && i < BENCH_MODEL_N ? BENCH_MODEL_AVG[i] : 0u; }

/* ------------------------------------------------------------------ BENCH-B: the second core --- */

/* ------------------------------------------------------------------ the log (USB serial console, CSV) --- */
#ifndef BENCH_OUT
#if FELUCCA_CDC
#define BENCH_OUT(s) do { if (cdc.dtr) { con.stalled = 0; con_puts(s); } } while (0)   /* (each line its own 20 ms if the Mac stops reading) */
#else
#define BENCH_OUT(s) ((void)0)
#endif
#endif
#ifndef BENCH_REPLY
#if FELUCCA_CDC
#define BENCH_REPLY(s) con_puts(s)                       /* (a console word's reply: inside console.c's command, so the console is open) */
#else
#define BENCH_REPLY(s) ((void)0)
#endif
#endif
static BENCH_FN char *bench_put(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}
static BENCH_FN char *bench_num(char *d, int32_t v)
{
    fmt_int(d, v);
    return d + str_len(d);
}
static BENCH_FN char *bench_fixn(char *d, int32_t v, uint32_t dec)   /* v / 10^dec: "63.4", "0.0", "1.12", "-1.2" (libc.c fmt_fix leaves the 0 out: ".85") */
{
    uint32_t p = dec == 2u ? 100u : 10u, u = (uint32_t)(v < 0 ? -v : v);
    if (v < 0)
        *d++ = '-';
    d = bench_num(d, (int32_t)(u / p));
    *d++ = '.';
    if (dec == 2u)
        *d++ = (char)('0' + u / 10u % 10u);
    *d++ = (char)('0' + u % 10u);
    *d = 0;
    return d;
}
static BENCH_FN char *bench_x10(char *d, int32_t v) { return bench_fixn(d, v, 1u); }
static const char *const BENCH_NR[NR_N] BENCH_RO = {"-", "ENGINE", "FILTER", "INSERT", "SAT", "EQ", "FX", "OTHER", "MISC"};   /* (budget.h NR_*) */
static BENCH_FN uint32_t bench_rp_now(void)            /* the RAM code compared with its flash image now (ramplace.c; about 1 ms): ABOUT's state too */
{
#if DUELL_RAMPLACE
    rp_state = (uint8_t)ramplace_check();
    return rp_state;
#else
    return 0xFFu;
#endif
}
static BENCH_FN const char *bench_rp_word(uint32_t rp)
{
    return rp == 0xFFu ? "-" : !rp ? "OK" : rp == 1u ? "FAIL X" : rp == 2u ? "FAIL Z" : "FAIL X Z";
}
static BENCH_FN void bench_line_head(char *b)
{
    char *d = bench_put(b, "BENCH,#,name,group,mode,notes,avg,peak,miss,shed,lfx,vox,model,ratio,ref,delta,model_delta,base,c1,c1pk,c1late,c1over,c1units,c1holds,busier,clean,notes1,notes2,why1,why2,budget,ramcode,boots\r\n");
    (void)d;
}
static BENCH_FN void bench_line(char *b, uint32_t i)            /* one row as CSV (b: 256 bytes) */
{
    static const char *const MN[DM_N] BENCH_RO = {"DUEL", "STEREO", "LAYER"};
    const bench_scn_t *s = &BENCH_SCN[i];
    const bench_res_t *r = &bench_res[i];
    uint32_t ri = bench_refi[i], m = bench_model(i), clean;
    char *d = b;
    d = bench_put(d, "BENCH,");
    d = bench_num(d, (int32_t)i + 1);
    d = bench_put(d, ",");
    d = bench_put(d, s->name);
    d = bench_put(d, ",");
    d = bench_put(d, BG_NAME[s->grp]);
    d = bench_put(d, ",");
    d = bench_put(d, MN[s->mode % DM_N]);
    d = bench_put(d, ",");
    d = bench_num(d, s->n[0]);                         /* (LAYER: part 1's notes, both parts sound them) */
    d = bench_put(d, "+");
    d = bench_num(d, s->mode == DM_LAYER ? s->n[0] : s->n[1]);
    d = bench_put(d, ",");
    if (!(r->flags & RF_DONE)) {
        d = bench_put(d, r->flags & RF_SKIP ? "SKIPPED" : "NOT RUN");
        d = bench_put(d, ",,,,,,");
    } else {
        d = bench_x10(d, r->avg);
        d = bench_put(d, ",");
        d = bench_x10(d, r->pk);
        d = bench_put(d, ",");
        d = bench_num(d, r->miss);
        d = bench_put(d, ",");
        d = bench_num(d, r->shed);
        d = bench_put(d, ",");
        d = bench_num(d, r->lfx);
        d = bench_put(d, ",");
        d = bench_x10(d, r->vox);
        d = bench_put(d, ",");
    }
    if (m)
        d = bench_x10(d, (int32_t)m);
    d = bench_put(d, ",");
    if (m && (r->flags & RF_DONE)) {                   /* device / model: "1.12" */
        d = bench_fixn(d, (int32_t)((r->avg * 100u + m / 2u) / m), 2u);
    }
    d = bench_put(d, ",");
    if (ri != BR_NONE) {
        d = bench_put(d, BENCH_SCN[ri].name);
        d = bench_put(d, ",");
        if ((r->flags & RF_DONE) && (bench_res[ri].flags & RF_DONE))
            d = bench_x10(d, (int32_t)r->avg - (int32_t)bench_res[ri].avg);
        d = bench_put(d, ",");
        if (m && bench_model(ri))
            d = bench_x10(d, (int32_t)m - (int32_t)bench_model(ri));
    } else
        d = bench_put(d, ",,");
    d = bench_put(d, ",");
    if (s->base) {                                     /* (a ',' in a baseline name is written ';': one CSV field) */
        const char *c;
        for (c = s->base; *c; c++)
            *d++ = *c == ',' ? ';' : *c;
        *d = 0;
    }
    d = bench_put(d, ",");
    if (r->flags & RF_C1) {
        d = bench_x10(d, r->c1);
        d = bench_put(d, ",");
        d = bench_x10(d, r->c1pk);
        d = bench_put(d, ",");
        d = bench_num(d, r->late);
        d = bench_put(d, ",");
        d = bench_num(d, r->over);
        d = bench_put(d, ",");
        d = bench_num(d, (int32_t)r->c1units);
        d = bench_put(d, ",");
        d = bench_num(d, r->c1holds);
        d = bench_put(d, ",");
        d = bench_x10(d, r->c1 > r->avg ? r->c1 : r->avg);   /* (the busier core) */
    } else
        d = bench_put(d, ",,,,,,");
    clean = (r->flags & RF_DONE) && !r->miss && !(r->flags & RF_C1FAIL);
    d = bench_put(d, ",");
    d = bench_put(d, !(r->flags & RF_DONE) ? "" : clean ? "CLEAN" : (r->flags & RF_C1FAIL) ? "C1 FAILED" : "MISSED");
    if (r->flags & RF_DONE) {                          /* (stage 0: the NOTES budget, the RAM code, the restart counter) */
        uint32_t p;
        for (p = 0; p < 2u; p++) {
            d = bench_put(d, ",");
            d = bench_num(d, r->bn[p]);
        }
        for (p = 0; p < 2u; p++) {
            d = bench_put(d, ",");
            d = bench_put(d, BENCH_NR[r->bw[p] % NR_N]);
        }
        d = bench_put(d, r->bopen ? ",OPEN," : ",ON,");
        d = bench_put(d, bench_rp_word(r->rp));
    } else
        d = bench_put(d, ",,,,,,");
    d = bench_put(d, ",");
    d = bench_num(d, (int32_t)BENCH_BOOTS);
    bench_put(d, "\r\n");
}
/* ---- core 1 (a DUELL_TWOCORE build): the session's mode and the runner's counters, for the log and the Mac's verdicts (BENCH_TWOCORE1.md).
 * bench_tc_get copies them (tcs: twocore_run.c; tc: twocore.c). The Mac twin has no core 1; bench_sim.c supplies bench_tc_get (BENCH_TC_SIM)
 * so the format and the runner's rules are tested. A build with neither prints "-" for the mode and nothing for the counters. */
#if !DUELL_TWOCORE   /* (join0233: was also BENCH_TC_SIM only: bench_tc_why needs TCR_NAMES in every bench build, the RAM1 + bench build did not compile) */
#include "twocore_reasons.h"
#endif
enum { BTC_BLOCKS, BTC_INTIME, BTC_LATE, BTC_VLATE, BTC_SILENT, BTC_WAIT, BTC_OWN, BTC_REFUSED, BTC_NOANS, BTC_PAUSES, BTC_RESUMES, BTC_TRIMS, BTC_PARKS, BTC_VLOK, BTC_PFAULT, BTC_PRUN, BTC_PWIN, BTC_PLATE, BTC_N };   /* (BENCHT3: the last five: very late blocks that paused nothing; pauses by reason) */
typedef struct { uint32_t c[BTC_N]; uint32_t stack; uint8_t up, failed, armed, boot_last; } bench_tcv_t;
#if DUELL_TWOCORE
#define BENCH_TC 1
static BENCH_FN void bench_tc_get(bench_tcv_t *v)
{
    v->c[BTC_BLOCKS] = tcs.blocks, v->c[BTC_INTIME] = tcs.in_time, v->c[BTC_LATE] = tcs.late, v->c[BTC_VLATE] = tcs.vlate;
    v->c[BTC_SILENT] = tcs.silent, v->c[BTC_WAIT] = tcs.wait, v->c[BTC_OWN] = tcs.own, v->c[BTC_REFUSED] = tcs.refused;
    v->c[BTC_NOANS] = tcs.noanswer, v->c[BTC_PAUSES] = tcs.pauses, v->c[BTC_RESUMES] = tcs.resumes, v->c[BTC_TRIMS] = tcs.trims;
    v->c[BTC_PARKS] = tc.parks;
    v->c[BTC_VLOK] = tcs.vlate_ok, v->c[BTC_PFAULT] = tcs.pause_fault, v->c[BTC_PRUN] = tcs.pause_run, v->c[BTC_PWIN] = tcs.pause_win, v->c[BTC_PLATE] = tcs.pause_late;
    v->stack = tc_stack_peak(fm1_c1_stack, FM1_C1_STACK_WORDS + 64u);
    v->up = tc.up, v->failed = tc.failed, v->armed = tc.armed, v->boot_last = tc.boot_last;
}
#elif defined(BENCH_TC_SIM)
#define BENCH_TC 1
static void bench_tc_get(bench_tcv_t *v);
#else
#define BENCH_TC 0
#endif
/* ---- BENCHT2: core 1's timing, the own split and the runtime switches (twocore_run.c tcm_*, tcm_cfg), for the result line, `bench tcm` and `bench info`. The Mac twin supplies
 * the same functions (BENCH_TC_SIM) so the format and the words are tested. Times in us; the histograms' edges are in twocore_run.c (delay, run, margin). */
#define BTM_NB 8
#define BTM_NEV 16u
typedef struct {
    uint32_t posted, own[5], n, badclk;                /* jobs posted; no job because core 1 was down / a flash park / paused / the deadline was past / (BENCHT3) the floor: the measured run time does not fit; jobs measured, impossible clocks */
    int32_t sd_max, sd_mean, rt_max, rt_mean, mg_min, mg_mean;   /* start delay, run time, finish margin (us) */
    uint32_t nv, vrt_max, vrt_mean, vsd_max, vblk[4];  /* the very late jobs that were measured: their run time, delay, block in the half */
    uint32_t hsd[BTM_NB], hrt[BTM_NB], hmg[BTM_NB];
    uint32_t st_max, st_mean[4], dl_mean[4];           /* where in the half (us after its start): the latest start of a job; per block of the half the mean start and the mean deadline */
    uint32_t c1x10, c1pkx10;                           /* core 1's busy share (x10 %) over the test, and of its busiest half */
    uint32_t slack, calm_ms, lrun, vrun;               /* the settings in force (slack in ticks) */
    uint32_t vwin_k, vwin_ms, floor;                   /* BENCHT3: very late blocks (K) within the window (ms) that pause the jobs (K 0: off); the deadline's floor on / off */
    uint32_t ev_n, ev_k;                               /* very late jobs seen, kept in the ring */
    struct { uint32_t blk, at_ms, a1, a2; int32_t sd, rt, mg; } ev[BTM_NEV];
} bench_tmv_t;
#if DUELL_TWOCORE && TC_MEAS
#define BENCH_TM 1
_Static_assert(BTM_NEV == TCM_NEV, "bench.c: the ring of very late jobs");
static BENCH_FN void bench_tm_get(bench_tmv_t *v)
{
    uint32_t n = tcm.n, i, blocks = tcs.blocks - tcm.b0, k = tcm.ev_n < BTM_NEV ? tcm.ev_n : BTM_NEV;
    memset(v, 0, sizeof *v);
    v->posted = tcm.posted, v->n = n, v->badclk = tcm.badclk;
    for (i = 0; i < 5u; i++)
        v->own[i] = tcm.own[i];
    for (i = 0; i < 4u; i++)
        v->vblk[i] = tcm.vblk[i];
    if (n) {
        v->sd_max = (int32_t)(tcm.sd_max / 24u), v->sd_mean = (int32_t)(tcm.sd_sum / n / 24u);
        v->rt_max = (int32_t)(tcm.rt_max / 24u), v->rt_mean = (int32_t)(tcm.rt_sum / n / 24u);
        v->mg_min = tcm.mg_min / 24, v->mg_mean = (int32_t)(tcm.mg_sum / (int64_t)n / 24);
    }
    v->st_max = tcm.st_max / 24u;
    for (i = 0; i < 4u; i++)
        if (tcm.nb[i])
            v->st_mean[i] = (uint32_t)(tcm.st_sum[i] / tcm.nb[i] / 24u), v->dl_mean[i] = (uint32_t)(tcm.dl_sum[i] / tcm.nb[i] / 24u);
    v->nv = tcm.nv, v->vrt_max = tcm.vrt_max / 24u, v->vsd_max = tcm.vsd_max / 24u;
    v->vrt_mean = tcm.nv ? (uint32_t)(tcm.vrt_sum / tcm.nv / 24u) : 0u;
    for (i = 0; i < 8u; i++)
        v->hsd[i] = tcm.hsd[i], v->hrt[i] = tcm.hrt[i], v->hmg[i] = tcm.hmg[i];
    if (blocks) {
        v->c1x10 = (uint32_t)(tcm.rt_sum * 1000u / ((uint64_t)blocks * TCR_BLOCK_TICKS));
        v->c1pkx10 = tcm.hpk * 1000u / TCR_HALF_TICKS;
    }
    v->slack = tcm_cfg.slack, v->calm_ms = tcm_cfg.calm_ticks / (1000u * TC_TICKS_PER_US), v->lrun = tcm_cfg.lrun, v->vrun = tcm_cfg.vrun;
    v->vwin_k = tcm_cfg.vwin_k, v->vwin_ms = tcm_cfg.vwin_ticks / (1000u * TC_TICKS_PER_US), v->floor = tcm_cfg.floor;
    v->ev_n = tcm.ev_n, v->ev_k = k;
    for (i = 0; i < k; i++) {                          /* oldest first */
        const tcm_ev_t *e = &tcm.ev[(tcm.ev_n - k + i) % BTM_NEV];
        v->ev[i].blk = e->blk, v->ev[i].at_ms = (uint32_t)((uint64_t)e->at * TCR_BLOCK_TICKS / (1000u * TC_TICKS_PER_US));
        v->ev[i].a1 = e->a1, v->ev[i].a2 = e->a2, v->ev[i].sd = e->sd / 24, v->ev[i].rt = e->rt / 24, v->ev[i].mg = e->mg / 24;
    }
}
static BENCH_FN void bench_tm_clear(void) { tcm.clr = 1; }
/* what: 0 slack (ticks), 1 calm (ms), 2 late run, 3 very late run, 4 window K, 5 window (ms), 6 floor (0 / 1); val 0xFFFFFFFF: the default */
static BENCH_FN void bench_tm_set(uint32_t what, uint32_t val)
{
    uint32_t d = val == 0xFFFFFFFFu;
    if (what == 0)
        tcm_cfg.slack = d ? TCR_SLACK_TICKS : val;
    else if (what == 1)
        tcm_cfg.calm_ticks = d ? TCR_CALM_TICKS : val * 1000u * TC_TICKS_PER_US;
    else if (what == 2)
        tcm_cfg.lrun = d ? TCR_LATE_RUN : val;
    else if (what == 3)
        tcm_cfg.vrun = d ? TCR_VLATE_RUN : val;
    else if (what == 4)
        tcm_cfg.vwin_k = d ? TCR_VWIN_K : val;
    else if (what == 5)
        tcm_cfg.vwin_ticks = d ? TCR_VWIN_TICKS : val * 1000u * TC_TICKS_PER_US;
    else
        tcm_cfg.floor = d ? 0u : val;                  /* (BENCHT4: the floor is off by default) */
}
#elif defined(BENCH_TC_SIM)
#define BENCH_TM 1
static void bench_tm_get(bench_tmv_t *v);
static void bench_tm_clear(void);
static void bench_tm_set(uint32_t what, uint32_t val);
#else
#define BENCH_TM 0
#endif

/* the session's mode as a word: "2" and why "OK", or "1" and the reason: NOANSWER (core 1 did not answer at the start), FAIL-<R> (stopped during
 * this session: HANG, BADJOB, FLASH ..), PREV-<R> (the last session ended in a strike: this power-on is one core, the next tries again),
 * SKIPBOOT (the boot guard), NOTARMED (no start armed) */
static BENCH_FN char *bench_tc_why(char *d, const bench_tcv_t *v)
{
    static const char *const nm[] = TCR_NAMES;
    uint32_t r;
#if BENCH_TC
    if (v->up && !v->failed)
        return bench_put(d, "OK");
    if (v->failed == TCR_NOANSWER)
        return bench_put(d, "NOANSWER");
    if (v->failed)
        return bench_put(bench_put(d, "FAIL-"), nm[v->failed % TCR_N]);
    r = v->boot_last;
    if (r == TCR_SKIPBOOT)
        return bench_put(d, "SKIPBOOT");
    if (tc_is_strike(r))
        return bench_put(bench_put(d, "PREV-"), nm[r % TCR_N]);
    return bench_put(d, "NOTARMED");
#else
    (void)nm, (void)r, (void)v;
    return bench_put(d, "NOTBUILT");
#endif
}
static BENCH_FN char *bench_tc_mode(char *d, const bench_tcv_t *v)   /* "2" or "1" ("-": a build without core 1) */
{
#if BENCH_TC
    return bench_put(d, v->up && !v->failed ? "2" : "1");
#else
    (void)v;
    return bench_put(d, "-");
#endif
}
/* how deep core 0's stack has been, in bytes: the first word from the bottom that is no longer the paint (main.c c0_stack_paint at start-up) */
static BENCH_FN uint32_t c0_stack_peak(const uint32_t *lo, const uint32_t *top)
{
    while (lo < top && *lo == C0_STK_FILL)
        lo++;
    return (uint32_t)(((uintptr_t)top - (uintptr_t)lo));
}
static BENCH_FN void bench_line_info(char *b)
{
    char *d = bench_put(b, "BENCH,INFO,");
    d = bench_put(d, FELUCCA_VERSION);
    d = bench_put(d, ",tag ");
    d = bench_put(d, BENCH_TAG);
    d = bench_put(d, ",model ");
    d = bench_put(d, bench.model_ok ? "ok" : "stale");
    d = bench_put(d, ",half_us ");
    d = bench_num(d, (int32_t)CPU_BUDGET_US);
    d = bench_put(d, ",ramcode ");
    d = bench_put(d, bench_rp_word(bench_rp_now()));
#if DUELL_RAMPLACE
    d = bench_put(d, " ");
    d = bench_num(d, (int32_t)((ramplace_bytes() + 512u) / 1024u));
    d = bench_put(d, " KB");
#endif
    d = bench_put(d, ",boots ");
    d = bench_num(d, (int32_t)BENCH_BOOTS);
    d = bench_put(d, ",c0stack ");                   /* core 0's stacks, deepest use so far: user / exception, of 24,320 / 7,936 B */
    {
        uint32_t *sb[4];
        c0_stack_bounds(sb);
        d = bench_num(d, (int32_t)c0_stack_peak(sb[0], sb[1]));
        d = bench_put(d, "/");
        d = bench_num(d, (int32_t)c0_stack_peak(sb[2], sb[3]));
    }
    {
        bench_tcv_t v;
        memset(&v, 0, sizeof v);
#if BENCH_TC
        bench_tc_get(&v);
#endif
        d = bench_put(d, ",cores ");
        d = bench_tc_mode(d, &v);
        d = bench_put(d, ",why ");
        d = bench_tc_why(d, &v);
    }
#if BENCH_TM
    {
        bench_tmv_t m;
        bench_tm_get(&m);
        d = bench_put(d, ",slack ");
        d = bench_num(d, (int32_t)m.slack);
        d = bench_put(d, ",calm ");
        d = bench_num(d, (int32_t)m.calm_ms);
        d = bench_put(d, ",lrun ");
        d = bench_num(d, (int32_t)m.lrun);
        d = bench_put(d, ",vrun ");
        d = bench_num(d, (int32_t)m.vrun);
        d = bench_put(d, ",vwin ");
        d = bench_num(d, (int32_t)m.vwin_k);
        d = bench_put(d, "/");
        d = bench_num(d, (int32_t)m.vwin_ms);
        d = bench_put(d, ",floor ");
        d = bench_num(d, (int32_t)m.floor);
    }
#endif
    bench_put(d, "\r\n");
}
static BENCH_FN void bench_line_end(char *b)
{
    char *d = bench_put(b, "BENCH,END,");
    d = bench_num(d, bench.last + 1);
    d = bench_put(d, " rows,");
    d = bench_num(d, (int32_t)((fm1_ms - bench.t_run) / 1000u));
    d = bench_put(d, " s,");
    bench_put(d, bench.aborted ? "STOPPED\r\n" : "DONE\r\n");
}
static BENCH_FN void bench_con_dump(void)              /* K2 on the BENCH screen: every row again (USB serial console) */
{
    char b[256];
    uint32_t i;
    bench_resolve();
    bench_line_info(b);
    BENCH_OUT(b);
    bench_line_head(b);
    BENCH_OUT(b);
    for (i = 0; i < BENCH_NSCN; i++) {
        bench_line(b, i);
        BENCH_OUT(b);
    }
    bench_put(b, "BENCH,END\r\n");
    BENCH_OUT(b);
}

/* ------------------------------------------------------------------ a row's setting --- */
static BENCH_FN void bench_note(uint32_t ch, uint32_t note, uint32_t on)
{
    uint32_t pkt = (on ? 0x09u : 0x08u) | ((on ? 0x90u : 0x80u) | (ch & 15u)) << 8 | (note & 127u) << 16 | (on ? 64u : 0u) << 24;
    (void)midi_enqueue(pkt, 1u);
}
static const uint8_t BENCH_N8[8] BENCH_RO = {48, 52, 55, 59, 62, 65, 69, 72};
static BENCH_FN void bench_part(uint32_t p, const bench_scn_t *s)   /* a part from INIT, then the row's sound and stages (as tests/cpu_bench.c setpart) */
{
    track_t *t = &trk[p];
    uint32_t i, e = s->e[p] ? s->e[p] - 1u : TRK_DEF[p][0], st = s->e[p] && (p == 0u || s->both);
    for (i = 0; i < P_E0; i++)                         /* every common value at its default: the mixer, the arp, the matrix, the stages */
        t->p[i] = TP[i].def;
    init_engine_of(t, e);
    apply_preset_to(t, s->e[p] ? bench_dear[e % NENGINES] : TRK_DEF[p][1]);
    t->p[P_LEVEL] = TP[P_LEVEL].def;
    t->p[P_PAN] = TP[P_PAN].def;
    t->p[P_MUTE] = 0;
    if (!s->e[p])
        return;                                        /* (a part that plays nothing keeps its power-on sound with its sends, as on the host) */
    t->p[P_SUS] = 127;
    t->p[P_CHRD] = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_AMODE] = 0;
    t->p[P_CHOR] = s->cho, t->p[P_DLY] = s->dly, t->p[P_REV] = s->rev;
    t->p[P_ITYPE] = st ? s->ins : 0;
    if (st && s->ins)
        t->p[P_IA] = t->p[P_IB] = t->p[P_IC] = 64, t->p[P_IMIX] = 127;
    t->p[P_DIST] = st ? s->sat : 0;
    t->p[P_EQLG] = t->p[P_EQMG] = t->p[P_EQHG] = 0;
    if (st && s->eq >= 1u)
        t->p[P_EQLG] = 5, t->p[P_EQLF] = -4;
    if (st && s->eq >= 2u)
        t->p[P_EQMG] = -4, t->p[P_EQMF] = 5, t->p[P_EQMQ] = 3;
    if (st && s->eq >= 3u)
        t->p[P_EQHG] = 6, t->p[P_EQHF] = 2;
    t->p[P_DKDEP] = 0;
    if (st && s->duck)
        t->p[P_DKDEP] = 90, t->p[P_DKATK] = 20, t->p[P_DKRLS] = 70, t->p[P_DKSRC] = 0;
    if (p == 0u && s->flt == 1u)
        t->p[P_FREEA] = UF_STEPS, t->p[P_FREEC] = 0, t->p[P_KTRK] = 0;
    else if (p == 0u && (s->flt == 12u || s->flt == 24u))
        t->p[P_FREEA] = 41, t->p[P_FREEB] = 40, t->p[P_FREEC] = 0, t->p[P_KTRK] = 0, t->p[P_FREED] = s->flt == 24u ? 2 : 1;
}
static BENCH_FN void bench_load_m16(void)              /* M16 STRESS TEST as a multi LOAD has it (multi.c project_load: slot 16 as stored, the factory one unless
                                                        * the owner saved over it), then LAYER whatever MULTI LOAD says */
{
    project_load(15u);
}
static BENCH_FN void bench_set(const bench_scn_t *s)            /* the row's whole setting (main loop). The UI's per-frame syncs (ui_input.c:
                                                        * cmp_sync, ms_sync, mods_apply, mode_settings) are not running while the BENCH screen is up, so
                                                        * the audio side's words are set here directly, as tests/cpu_bench.c does; the settings stay */
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_LOAD && i != G_SAVE)
            song.g[i] = GP[i].def;
    song.g[G_RLOCUT] = 0;                              /* (as tests/cpu_bench.c) */
    bench_bopen = (uint8_t)(s->grp != BG_M16);         /* (the cost rows: the NOTES budget held open from the next frame on, before the notes;
                                                        * the M16 rows: the budget as the owner plays it) */
    mods_off = (uint8_t)(s->sendsoff ? MODS_SENDS : 0u);
    perf_space = 0;
    mix_bal = 0, mix_xf = 0, mix_split = 0;
    if (s->m16)
        bench_load_m16();
    else
        for (i = 0; i < 2u; i++)
            bench_part(i, s);
    if (s->dmode) song.g[G_DMODE] = (int16_t)(s->dmode - 1u);
    if (s->rtype) song.g[G_RTYPE] = (int16_t)(s->rtype - 1u);
    if (s->shp) song.g[G_SHPITCH] = (int16_t)(s->shp - 1u);
    song.g[G_SHMOD] = (int16_t)s->shm;
    cmp_cfg = (uint32_t)CMP_THR_DEF | (uint32_t)s->comp << 8 | (uint32_t)CMP_ATK_DEF << 16 | (uint32_t)CMP_REL_DEF << 24;
    wd_sel = (uint8_t)(s->width ? s->width : 100u);
    bm_sel = s->bm;
    duell_mode_req = s->mode;
    ui.force = 1;
}
static BENCH_FN void bench_notes_on(const bench_scn_t *s)
{
    uint32_t p, i;
    bench.nnotes = 0;
    if (s->m16) {                                      /* M16: keys C4 E4 G4 on part 1's channel (LAYER: both parts) */
        static const uint8_t K[3] BENCH_RO = {60, 64, 67};
        for (i = 0; i < s->n[0] && i < 3u; i++)
            bench.notes[bench.nnotes] = K[i], bench.nch[bench.nnotes++] = midi_pch[0];
    } else
        for (p = 0; p < 2u; p++) {
            if (!s->e[p] || (s->mode == DM_LAYER && p == 1u))
                continue;                              /* (LAYER: part 2 takes part 1's notes; its own channel is ignored there) */
            for (i = 0; i < s->n[p] && i < 8u; i++)
                bench.notes[bench.nnotes] = (uint8_t)(BENCH_N8[i] + 7u * p), bench.nch[bench.nnotes++] = midi_pch[p];
        }
    for (i = 0; i < bench.nnotes; i++)
        bench_note(bench.nch[i], bench.notes[i], 1u);
}
static BENCH_FN void bench_notes_off(void)
{
    uint32_t i;
    for (i = 0; i < bench.nnotes; i++)
        bench_note(bench.nch[i], bench.notes[i], 0u);
    bench.nnotes = 0;
}

/* ------------------------------------------------------------------ the run --- */
static BENCH_FN void bench_keep_session(void)
{
    project_capture(&proj_scratch);                    /* (through proj_scratch, as every other caller: a new pointer here would change */
    memcpy(&bench_saved, &proj_scratch, sizeof bench_saved);   /* how the compiler builds project.c's functions for the normal code) */
    bench_keep.cmp = cmp_cfg;
    bench_keep.wd = wd_sel, bench_keep.bm = bm_sel, bench_keep.mods = mods_off;
    bench_keep.mode = duell_mode_req;
    bench_keep.bal = (uint8_t)mix_bal, bench_keep.xf = mix_xf, bench_keep.split = mix_split;
    bench_keep.space = perf_space;
}
static BENCH_FN void bench_restore_session(void)
{
    transport_req = 4;                                 /* STOP ALL first */
    memcpy(&proj_scratch, &bench_saved, sizeof proj_scratch);
    (void)project_restore_runtime(&proj_scratch);
    cmp_cfg = bench_keep.cmp;
    wd_sel = bench_keep.wd, bm_sel = bench_keep.bm, mods_off = bench_keep.mods;
    duell_mode_req = bench_keep.mode;
    mix_bal = (int8_t)bench_keep.bal, mix_xf = bench_keep.xf, mix_split = bench_keep.split;
    perf_space = bench_keep.space;
    bench_bopen = 0;                                   /* (the NOTES budget as before the run) */
    ui.force = 1;
}
static BENCH_FN void bench_start(uint32_t first, uint32_t last)
{
    uint32_t i;
    bench_sess_end();                                  /* (stage 1: a console session ends first: the owner's session back, then kept again) */
    bench_resolve();
    for (i = first; i <= last; i++)
        memset(&bench_res[i], 0, sizeof bench_res[i]);
    if (!bench.holds)
        bench_keep_session();
    bench.on = bench.holds = 1;
    bench.first = bench.row = (uint8_t)first;
    bench.last = (uint8_t)last;
    bench.aborted = 0;
    bench.phase = BS_STOP;
    bench.t = bench.t_run = fm1_ms;
    bench.page = 0;
    transport_req = 4;
    {
        char b[256];
        bench_line_info(b);
        BENCH_OUT(b);
        bench_line_head(b);
        BENCH_OUT(b);
    }
    ui.force = 1;
}
static BENCH_FN void bench_finish(void)
{
    char b[256];
    bench_notes_off();
    bench_restore_session();
    bench.on = 0;
    bench.holds = 0;
    bench.phase = BS_DONE;
    kb_boot_hold = 0;                                  /* (the keys sound again) */
    if (bench.persist) {                               /* (the settings write held during the run, now) */
        persist_pending = 1;
        bench.persist = 0;
    }
    bench.done_once = 1;
    bench_line_end(b);
    BENCH_OUT(b);
    ui_message(bench.aborted ? "BENCH STOPPED" : "BENCH DONE");
    ui.force = 1;
}
static BENCH_FN int bench_skip(const bench_scn_t *s)            /* a row that cannot run now */
{
    (void)s;
    return 0;
}
static BENCH_FN uint32_t bench_voices(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NLIVE; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active != 0;
    return n;
}
static BENCH_FN void bench_measure_begin(void)
{
    do {
        bench.n0 = cpu_n;
        bench.s0 = cpu_sum;
    } while (bench.n0 != cpu_n);
    bench.m0 = cpu_miss;
    bench.shed0 = shed_count;
    cpu_pk = 0;
    bench.vsum = bench.vn = bench.lfn = 0;
}
static BENCH_FN void bench_measure_end(bench_res_t *r)
{
    uint32_t n, sum;
    do {
        n = cpu_n;
        sum = cpu_sum;
    } while (n != cpu_n);
    n -= bench.n0;
    sum -= bench.s0;
    r->avg = (uint16_t)(n ? bench_clamp((int32_t)((uint64_t)sum * 1000u / ((uint64_t)n * CPU_BUDGET_US)), 0, 9999) : 0);
    r->pk = (uint16_t)bench_clamp((int32_t)((uint64_t)cpu_pk * 1000u / CPU_BUDGET_US), 0, 9999);
    r->miss = (uint16_t)bench_clamp((int32_t)(cpu_miss - bench.m0), 0, 65535);
    r->shed = (uint16_t)bench_clamp((int32_t)(shed_count - bench.shed0), 0, 65535);
    r->vox = (uint16_t)(bench.vn ? bench.vsum * 10u / bench.vn : 0u);
    r->lfx = (uint8_t)(bench.vn ? bench.lfn * 100u / bench.vn : 0u);
    {
        uint32_t p;
        for (p = 0; p < 2u; p++) {                     /* (what the budget gives this setting, also when the row held it open) */
            r->bn[p] = part_notes_shown((int)p);
            r->bw[p] = part_notes_reason((int)p);
        }
    }
    r->bopen = bench_bopen;
    r->rp = (uint8_t)bench_rp_now();                   /* (after the counters: its 1 ms is not in the row) */
    r->flags |= RF_DONE;
}
/* every ui_input pass (bench_input): the run's next step. 1 = the sound is the bench's */
static BENCH_FN int bench_main_tick(void)
{
    const bench_scn_t *s;
    uint32_t dt;
    if (!bench.on) {
        bench_tl_tick();                               /* (stage 1: a console session's holds, its result line) */
        return bench.holds;
    }
    s = &BENCH_SCN[bench.row];
    dt = fm1_ms - bench.t;
    kb_boot_hold = 1;                                  /* (the keys silent while it runs) */
    scrn.idle = fm1_ms;                                /* (MENU > SCREEN OFF: awake while it runs) */
    ui.mset_dirty = 0;                                 /* (the settings' save timer held: nothing of a row is written) */
    as.t = fm1_ms;                                     /* (the autosave's 10 s of quiet never pass: rows without notes, IDLE and the core 1
                                                        * IDL rows, ran into it: project.c autosave_poll wrote 0xE5000 mid-row) */
    if (persist_pending == 1u) {                       /* (a settings write asked for before the run: after it) */
        bench.persist = 1;
        persist_pending = 0;
    }
    switch (bench.phase) {
    case BS_STOP:
        if (dt >= 150u) {
            if (bench_skip(s)) {
                bench_res[bench.row].flags |= RF_SKIP;
                bench.phase = BS_END;
            } else {
                bench_set(s);
                bench.phase = BS_SETTLE;
            }
            bench.t = fm1_ms;
        }
        break;
    case BS_SETTLE:
        if ((dt >= 700u && !wvj.on) || dt >= 3000u) {
            bench_notes_on(s);
            bench.phase = BS_ATTACK;
            bench.t = fm1_ms;
        }
        break;
    case BS_ATTACK:
        if (dt >= 500u) {
            bench_measure_begin();
            bench.phase = BS_MEASURE;
            bench.t = fm1_ms;
        }
        break;
    case BS_MEASURE:
        bench.vsum += bench_voices();
        bench.vn++;
        bench.lfn += ovl_lite != 0u;
        if (dt >= bench_secs(s) * 1000u) {
            bench_measure_end(&bench_res[bench.row]);
            bench.phase = BS_END;
            bench.t = fm1_ms;
        }
        break;
    case BS_END:
        {
            char b[256];
            bench_line(b, bench.row);
            BENCH_OUT(b);
        }
        bench_notes_off();
        if (bench.row >= bench.last) {
            bench_finish();
            break;
        }
        bench.row++;
        transport_req = 4;
        bench.phase = BS_STOP;
        bench.t = fm1_ms;
        break;
    default:
        break;
    }
    return 1;
}

/* ------------------------------------------------------------------ the screen --- */
#define BENCH_ROWS 13u                                 /* table rows a page */
static BENCH_FN uint32_t bench_pages(void) { return 1u + (BENCH_NSCN + BENCH_ROWS - 1u) / BENCH_ROWS; }
static BENCH_FN void bench_open(void)
{
    bench_resolve();
    bench.page = 0;
    bench.sig = 0;
}
static BENCH_FN void bench_pct(char *b, uint32_t x10, int dec)  /* x10 % -> "63" or "63.4" */
{
    if (dec)
        bench_fixn(b, (int32_t)x10, 1u);
    else
        fmt_int(b, (int32_t)((x10 + 5u) / 10u));
}
static BENCH_FN const char *bench_state(char *b)
{
    char *d;
    if (bench.on) {
        d = bench_put(b, "RUN ");
        d = bench_num(d, bench.row + 1);
        d = bench_put(d, "/");
        bench_num(d, bench.last + 1);
    } else
        bench_put(b, bench.done_once ? (bench.aborted ? "STOPPED" : "DONE") : "READY");
    return b;
}
static BENCH_FN void bench_draw_state(int32_t y0)               /* page 1: what it is, where it stands */
{
    char b[48], v[16];
    const bench_scn_t *s = &BENCH_SCN[bench.row];
    uint32_t done = 0, i, left;
    int32_t y = 22 - y0;
    for (i = 0; i < BENCH_NAUTO; i++)
        done += (bench_res[i].flags & (RF_DONE | RF_SKIP)) != 0u;
    cv_text_on(10, y, &AF_S, "DEVELOPER BENCH BUILD", T_REC, T_BG);
    y += 15;
    cv_text_on(10, y, &AF_S, FELUCCA_VERSION, T_MID, T_BG);
    y += 18;
    cv_text_on(10, y, &AF_M, bench_state(b), bench.on ? T_ACCENT : T_TEXT, T_BG);
    y += 22;
    cv_rect(10, y, 220, 6, T_RAISE);
    cv_rect(10, y, (int32_t)(220u * done / BENCH_NAUTO), 6, T_THEME);
    y += 12;
    if (bench.on) {
        cv_text_on(10, y, &AF_S, s->name, T_TEXT, T_BG);
        left = 0;
        for (i = bench.row; i <= bench.last; i++)
            left += 1350u + bench_secs(&BENCH_SCN[i]) * 1000u;
        bench_put(v, "~");
        bench_num(v + 1, (int32_t)(left / 60000u + 1u));
        bench_put(v + str_len(v), " MIN LEFT");
        cv_text_r(230, y, &AF_S, v, T_MID, T_BG);
        y += 15;
        bench_put(b, "NOW ");
        bench_pct(b + 4, (uint32_t)(song.cpu_q8 * 1000u / 256u), 0);
        bench_put(b + str_len(b), " %");
        cv_text_on(10, y, &AF_S, b, T_MID, T_BG);
        y += 15;
    } else {
        cv_text_on(10, y, &AF_S, bench.done_once ? "K1: THE TABLE. K4 OR OCT+: AGAIN" : "K4 OR OCT+: START (ABOUT 7 MIN)", T_TEXT, T_BG);
        y += 15;
        cv_text_on(10, y, &AF_S, "TURN MASTER LOW FIRST. HANDS OFF", T_MID, T_BG);
        y += 15;
        cv_text_on(10, y, &AF_S, "K2: SEND THE LOG (USB SERIAL)", T_MID, T_BG);
        y += 15;
        cv_text_on(10, y, &AF_S, "K3: STRESS 10 MIN (HEAVY 8+8 DUEL)", T_MID, T_BG);
        y += 15;
    }
    cv_text_on(10, y, &AF_S, bench.model_ok ? "MODEL: HOST, SAME ROWS" : "MODEL: STALE (NOT SHOWN)", bench.model_ok ? T_MID : T_REC, T_BG);
    y += 15;
#if BENCH_TC
    {
        bench_tcv_t tv;
        volatile uint8_t *tz = (volatile uint8_t *)&tv;   /* (no memset call: it is in RAM range Z and this function sits at the very end of the flash, exactly 4 MB (the jump's reach) from it: any growth of the code before it broke the link, 1.0 job F1) */
        uint32_t zi;
        for (zi = 0; zi < sizeof tv; zi++)
            tz[zi] = 0;
        bench_tc_get(&tv);
        bench_put(b, tv.up && !tv.failed ? "TWO CORES: CORE 1 RUNS PART 2" : "ONE CORE: ");
        if (!(tv.up && !tv.failed))
            bench_tc_why(b + str_len(b), &tv);
        cv_text_on(10, y, &AF_S, b, tv.up && !tv.failed ? T_ACCENT : T_REC, T_BG);
        y += 15;
    }
#endif
}
static BENCH_FN int bench_is_c1_page(uint32_t first)
{
    return first < BENCH_NSCN && BENCH_SCN[first].grp == BG_C1;
}
static BENCH_FN void bench_draw_table(uint32_t pg, int32_t y0)  /* a page of the table: NAME AVG PEAK MISS MODEL RATIO (B1 rows: C0 PEAK MISS C1 LATE) */
{
    char v[16];
    uint32_t first = (pg - 1u) * BENCH_ROWS, i, c1p = 0;
    int32_t y = 20 - y0;
    for (i = first; i < first + BENCH_ROWS && i < BENCH_NSCN; i++)
        c1p |= bench_is_c1_page(i);
    cv_text_on(4, y, &AF_S, "ROW", T_MID, T_BG);
    cv_text_r(122, y, &AF_S, c1p ? "C0" : "AVG", T_MID, T_BG);
    cv_text_r(152, y, &AF_S, "PEAK", T_MID, T_BG);
    cv_text_r(180, y, &AF_S, "MISS", T_MID, T_BG);
    cv_text_r(208, y, &AF_S, c1p ? "C1" : "MDL", T_MID, T_BG);
    cv_text_r(238, y, &AF_S, c1p ? "LATE" : "X", T_MID, T_BG);
    y += 14;
    for (i = first; i < first + BENCH_ROWS && i < BENCH_NSCN; i++, y += 14) {
        const bench_res_t *r = &bench_res[i];
        uint32_t m = bench_model(i), c1r = BENCH_SCN[i].grp == BG_C1;
        uint16_t nc = !(r->flags & RF_DONE) ? T_DIM : (r->flags & RF_C1FAIL) || r->miss ? T_REC : (r->shed || r->lfx) ? cpu_col(1u, T_BG) : T_TEXT;
        if (bench.on && i == bench.row)
            cv_rect(0, y - 1, 240, 14, T_RAISE);
        cv_text_on(4, y, &AF_S, BENCH_SCN[i].name, nc, bench.on && i == bench.row ? T_RAISE : T_BG);
        if (!(r->flags & RF_DONE)) {
            cv_text_r(238, y, &AF_S, r->flags & RF_SKIP ? "SKIP" : "--", T_DIM, T_BG);
            continue;
        }
        bench_pct(v, r->avg, r->avg < 100u);
        cv_text_r(122, y, &AF_S, v, T_TEXT, T_BG);
        bench_pct(v, r->pk, 0);
        cv_text_r(152, y, &AF_S, v, r->pk >= 1000u ? T_REC : T_TEXT, T_BG);
        fmt_int(v, r->miss > 9999u ? 9999 : (int32_t)r->miss);
        cv_text_r(180, y, &AF_S, v, r->miss ? T_REC : T_MID, T_BG);
        if (c1r) {
            if (r->flags & RF_C1) {
                bench_pct(v, r->c1, 0);
                cv_text_r(208, y, &AF_S, v, T_TEXT, T_BG);
                fmt_int(v, r->late > 9999u ? 9999 : (int32_t)r->late);
                cv_text_r(238, y, &AF_S, v, r->late ? T_REC : T_MID, T_BG);
            }
        } else if (m) {
            bench_pct(v, m, m < 100u);
            cv_text_r(208, y, &AF_S, v, T_MID, T_BG);
            bench_fixn(v, (int32_t)((r->avg * 100u + m / 2u) / m), 2u);
            cv_text_r(238, y, &AF_S, v, T_TEXT, T_BG);
        }
    }
}
static BENCH_FN uint32_t bench_screen_sig(void)
{
    uint32_t h = 2166136261u, i;
    h = (h ^ bench.page) * 16777619u;
    h = (h ^ (bench.on * 7u + bench.phase * 131u + bench.row * 4099u + bench.done_once * 3u + bench.aborted * 5u)) * 16777619u;
    for (i = 0; i < BENCH_NSCN; i++)
        h = (h ^ (bench_res[i].flags + bench_res[i].avg * 3u)) * 16777619u;
    if (bench.on && bench.page == 0u)
        h = (h ^ (fm1_ms / 500u)) * 16777619u;         /* (page 1 while it runs: twice a second) */
    return h;
}
static BENCH_FN void bench_draw(void)
{
    char t[24];
    uint32_t sig = bench_screen_sig(), band;
    if (!ui.force && sig == bench.sig)
        return;
    if (bench.on && !ui.force && fm1_ms - bench.drawn < 250u)
        return;                                        /* (while it runs at most 4 frames a second: the measure sees the screen's work too) */
    bench.sig = sig;
    bench.drawn = fm1_ms;
    ui.force = 0;
    for (band = 0; band < 2u; band++) {
        int32_t y0 = (int32_t)band * 120;
        cv_begin(240, 120, T_BG);
        if (band == 0u) {
            cv_text_on(4, 2, &AF_S, "BENCH " BENCH_TAG, T_THEME, T_BG);
            bench_put(t, "PAGE ");
            bench_num(t + 5, bench.page + 1);
            bench_put(t + str_len(t), "/");
            bench_num(t + str_len(t), (int32_t)bench_pages());
            cv_text_r(236, 2, &AF_S, t, T_MID, T_BG);
            cv_rect(0, 17, 240, 1, T_RAISE);
        }
        if (bench.page == 0u)
            bench_draw_state(y0);
        else
            bench_draw_table(bench.page, y0);
        if (band == 1u)
            cv_text_on(4, 224 - y0, &AF_S, bench.on ? "K1 PAGE   OCT- STOP" : "K1 PAGE   K4/OCT+ START   OCT- BACK", T_DIM, T_BG);
        cv_blit(0, (uint32_t)y0);
    }
}

/* ------------------------------------------------------------------ the keys --- */
static BENCH_FN void bench_page_step(int32_t s)
{
    int32_t p = (int32_t)bench.page + (s > 0 ? 1 : -1);
    uint32_t n = bench_pages();
    bench.page = (uint8_t)(p < 0 ? 0 : p >= (int32_t)n ? (int32_t)n - 1 : p);
    ui.force = 1;
}
/* every ui_input pass, first (ui_input.c): the run's clock; on the BENCH screen (ui.menu 5) it takes the frame: the UI's own syncs, keys
 * and knobs wait. Not running: OCT+ or K4 right start; K1 the pages; OCT- back to the menu's tabs; HOME and the page buttons do nothing
 * here (OCT- first). K2 sends the log again, K3 the STRESS run (10 minutes). Running: OCT- stops it, K1 the pages */
static BENCH_FN int bench_input(uint32_t pressed, uint32_t notes)
{
    int32_t s;
    uint32_t up = (pressed >> panel.btn[B_OCTUP]) & 1u, dn = (pressed >> panel.btn[B_OCTDN]) & 1u;
    (void)notes;
    bench_main_tick();
    if (ui.menu != 5u)
        return 0;
    scrn.idle = fm1_ms;                                /* (MENU > SCREEN OFF: awake on this screen) */
    if ((s = panel_enc(EN_K1)) != 0)
        bench_page_step(s);
    if (bench.on) {
        if (dn) {
            bench.aborted = 1;
            bench_res[bench.row].flags |= RF_ABORT;
            bench_finish();
        }
    } else if (dn) {
        ui.menu = 1, ui.force = 1;
    } else if (up || panel_enc(EN_K4) > 0) {
        bench_start(0, BENCH_NAUTO - 1u);
    } else if (panel_enc(EN_K2) > 0) {
        bench_con_dump();                              /* SEND LOG: every row again to the USB serial console */
        ui_message(cdc.dtr ? "LOG SENT" : "NO SERIAL CONSOLE OPEN");
    } else if (panel_enc(EN_K3) > 0) {
        bench_start(BENCH_NSCN - 1u, BENCH_NSCN - 1u);   /* STRESS 10 MIN (one core) */
    }
    enc_drop();
    return 1;
}

/* ================================================================== bench suite stage 1: the console words and the timeline player ===
 * The Mac's runner (tools/bench_run.py) drives the unit over the USB serial console, which bench builds present whatever MENU > USB SERIAL says
 * (menu_items.c usb_serial_apply). One console word, `bench ...` (console.c: three lines; the lines stay under the console's 64 characters). A
 * reply is one line, "OK ..." or "ERR ...". The words:
 *   bench info                    the INFO line (version, tag, RAM code check, restarts) and a STATE line
 *   bench reset                   begins a bench session (the owner's session is kept, to be put back) or restarts it: STOP ALL, every part and every
 *                                 global value from INIT (the bench's IDLE ST row), the timeline emptied; no run, the NOTES budget as the owner plays it
 *   bench end                     the session over: the owner's session put back, the keys sound again (also 3 minutes after the last word)
 *   bench load multi N            multi M01..M16 (as MULTI LOAD / MIDI program change: ui.c pc_recall); the reply has its name
 *   bench load patch PART NN      patch slot #NN (1..128) to part 1 or 2 (the sound only); the reply has its name
 *   bench mode DUEL|STEREO|LAYER
 *   bench set PART NAME VALUE     a part's value (PART 1 or 2; NAME is the label the unit shows, SRC8 DST8 AMT8 RATE2 WAVE2 CUT ..., or its id number;
 *   bench set g NAME VALUE        VALUE a number or an enum's name: LFO2, S&H, CUT ..) / a global value (BPM ..): clamped to the range, as the editor's
 *   bench scene NAME              a row of the bench's list as its setting (BENCH_SCN: "HVY DUEL 8+8"), the NOTES budget held open
 *   bench budget OPEN|ON
 *   bench ev T on CH NOTE VEL     the timeline: T is ms from the test's start; CH 1..16. Events must come in time order (ERR order otherwise).
 *            T off CH NOTE        The player puts each into the MIDI-in ring at the START OF THE HALF (128 frames) it falls in, so it plays at the
 *            T at CH VAL          same sample in every run: bench_half below, called from audio.c (bench builds only). Replies "OK n" (n: places free)
 *            T cc CH CC VAL       or "ERR full": the runner streams the events ahead of the clock. `flash`: a settings save with unchanged values
 *            T ramp CH CC|AT A B MS   (main loop, so within a frame of T). A ramp moves a controller (or the channel aftertouch) from A to B over MS
 *            T flash              ms, one value a half (at most 8 at once). MORPH is CC20 on the part's channel (it is worked off in the main
 *            clear                loop: not sample exact)
 *   bench measure T0 T1           the CPU window of the next run, ms from its start (default: the whole run)
 *   bench run NAME MS             starts a test of MS ms (the timeline's time 0 is the next half); at its end one BTH (header) and one BT line (the
 *                                 result: CSV, fields below); `bench stop` ends it early (STOPPED)
 *   bench save-test               the same settings save at once
 * The per-test reset (bench_reset_phases below, the list is in release_drafts/BENCH_STAGE1.md) happens at the first half of a run, in the audio interrupt. */

#ifdef BENCH_HOST
static uint8_t bench_noreset;
#endif
#define TL_N 256u
#define TL_RAMPS 8u
enum { TK_NONE, TK_ON, TK_OFF, TK_AT, TK_CC, TK_RAMP, TK_FLASH };
enum { TS_IDLE, TS_ARMED, TS_RUN, TS_DONE, TS_STOP };
typedef struct {
    uint32_t h;                 /* the half (since the run's start) it is due at */
    uint16_t len;               /* RAMP: halves */
    uint8_t kind, ch, a, b, c, d;   /* ON/OFF: ch note vel; AT: ch val; CC: ch cc val; RAMP: ch cc(0x80 AT) from to */
} tl_ev_t;
typedef struct { uint32_t h0; uint16_t len; uint8_t ch, cc, from, to, last, on; } tl_ramp_t;
static struct {
    tl_ev_t ev[TL_N];
    volatile uint32_t w, r;     /* the ring: the main loop writes w, the audio interrupt r */
    tl_ramp_t rp[TL_RAMPS];
    uint32_t last_h;            /* the last event queued (they must come in order) */
    volatile uint8_t state;     /* TS_* */
    uint8_t sess, win, flash_req, reported, flashes, rsv;
    uint16_t lost;              /* events the MIDI-in ring or the ramp table refused */
    uint32_t h, dur, m0, m1;    /* the half now, the run's length, the CPU window (halves) */
    uint32_t n0, s0, c0, d0;    /* the meter's counters at the window's start: cpu_n, cpu_sum, cpu_miss, shed_count */
    uint32_t n1, s1, c1, d1, pk1, vsum, vn, lfn;   /* .. at its end; the peak; voices summed, halves, halves with LIGHT FX */
    uint32_t last_cmd;          /* fm1_ms of the last bench word */
    uint32_t ms0, ms1;          /* `bench measure`, ms (0, 0xFFFFFFFF: the whole run) */
    char name[20];
    bench_tcv_t tc0;            /* core 1's counters at the run's start (the result carries the differences) */
} btl BENCH_BSS;

static BENCH_FN uint32_t bench_ms_to_h(uint32_t ms) { return (ms * 441u * 2u + 1280u) / 2560u; }   /* 44,100 / 128 halves a second, rounded */

/* ---- the per-test reset: every random generator, LFO or modulation phase that runs without notes, back to its start value. Called from
 * bench_half at the first half of a run (the audio interrupt: nothing renders meanwhile). The list, and how it was found, is in BENCH_STAGE1.md */
/* (a macro, not a function taking the buses' addresses: fx / hl are never passed around in the normal code, and an address given to a function of its own would let the
 * compiler assume less in the hot bus code, which then grows (the hot RAM block is full to its last 228 bytes)) */
#define BENCH_RESET_BUS(f, h)                                                                                  \
    do {                                                                                                       \
        uint32_t k_;                                                                                           \
        (f).dly_w = (f).cho_w = (f).cho_ph = 0;                                                                \
        (f).dm_pl = (f).dm_pr = 0, (f).dm_xl = (f).dm_xr = (f).dm_dev = 0;                                     \
        (f).pl_ph1 = (f).pl_ph2 = 0;                                                                           \
        (f).sh_ph = (f).sp_ph = 0;                                                                             \
        (h).ph = 0;                                                                                            \
        for (k_ = 0; k_ < 4u; k_++) /* the write points of the reverb lines (the lines are cleared by STOP ALL's flush: all 0) */ \
            (f).comb_i[k_] = 0;                                                                                \
        (f).ap_i[0] = (f).ap_i[1] = 0;                                                                         \
        for (k_ = 0; k_ < 10u; k_++)                                                                           \
            (f).pl_w[k_] = 0;                                                                                  \
        (f).sh_w = (f).sh_pu = (f).sh_pd = (f).sh_l1 = (f).sh_l2 = 0;                                          \
        (f).rs_w = (f).sp_w = 0;                                                                               \
        for (k_ = 0; k_ < HL_NL; k_++)                                                                         \
            (h).i[k_] = 0;                                                                                     \
        (h).d[0] = (h).d[1] = 0;                                                                               \
    } while (0)
static BENCH_FN void bench_reset_phases(void)
{
    uint32_t p, i;
    rng_state = 0x1234567u;                            /* libc.c: the shared generator (events, the arp, note-on seeds, part 1's voice stage) */
    mod_seed = 0x2545F491u;                            /* mod.c: RAND of a note-on */
    bench_mono_seed = 0x1234567u;                      /* voice.c mono_play: the random start phases of a UNISON note */
    for (p = 0; p < NLIVE; p++) {
        track_t *t = &trk[p];
        memset(&t->lfo1, 0, sizeof t->lfo1);           /* LFO 1 and LFO 2: phase, value, fade, S&H, previous */
        memset(&t->lfo2, 0, sizeof t->lfo2);
        t->rngs = 0;                                   /* the part's own voice-stage generator */
        t->rr = 0;                                     /* POLY ROTATE */
        t->sp_alt = 0;                                 /* SPREAD: the side of the next voice */
        t->m_rnd = 0;
        t->m_env = t->m_env2 = 0;
        t->mw = t->at = t->ex_off = 0;                 /* the controllers of the test before: CC1, channel aftertouch, CC11 (mod.c) */
        t->m_vel = t->m_key = 0;
        memset(&ins[p], 0, sizeof ins[p]);             /* the INSERT (phaser, flanger, chorus, tape, vinyl): a silent part keeps its line, phases and noise */
        t->ins_run = 0;
        t->dist_hp = DIST_OFF;                         /* SAT: its filters start from the first sample again */
        t->dist_lp1 = t->dist_lp2 = 0;
        for (i = 0; i < NVOICE; i++)                   /* the voices that are not sounding: their leftovers (an oscillator keeps its phase from the note before) */
            if (!t->v[i].active)
                memset(&t->v[i], 0, sizeof t->v[i]);
        formant_nz[p] = 0;                             /* eng_formant.c: the part's breath-noise generator starts at its first note */
    }
    formant_used = 0;
    memset(f2s, 0, sizeof f2s);                        /* chain_f2.c: filter 2's state, the TRIO-style cutoff drift and its generator `nst` per voice: seeded once (`if (!s->nst)`) and kept from note to note, so a test started on a
                                                         * voice that had played another patch drifted differently (benchsim: 11 'does not repeat after reset'; WAVE_J item 2 guessed filter 1; it is the chain's filter 2) */
    memset(cy_lin, 0, sizeof cy_lin);                  /* cycle.c (0.2.31): the CYCLE envelope's progress, value and stage per voice: a new note starts the rise from where the generator stands, so a test after another patch's started elsewhere */
    memset(cy_y, 0, sizeof cy_y);
    memset(cy_st, 0, sizeof cy_st);
    memset(cy_memo, 0, sizeof cy_memo);
    memset(mx_sl, 0, sizeof mx_sl);                    /* mod.c (T0229): the matrix's source slew (the values of the block before, their sources, which slots hold one) */
    memset(mod_mixmv, 0, sizeof mod_mixmv);            /* .. and the mix ramp's state (fx.c): which parts' matrix moved PAN / LEVEL / a send, the values the last block ended at */
    memset(mx_was, 0, sizeof mx_was);
    memset(mx_ok, 0, sizeof mx_ok);
    memset(mx_lv, 0, sizeof mx_lv);
    memset(sp_cur, 0, sizeof sp_cur);
    memset(sp_prv, 0, sizeof sp_prv);
    memset(sp_go, 0, sizeof sp_go);
    memset(sp_ok, 0, sizeof sp_ok);
    memset(mx_sd, 0, sizeof mx_sd);
    memset(dist_x, 0, sizeof dist_x);                  /* SAT's follower (T0229: it follows the matrix-modulated SAT too, so it carries the test before's value) */
    memset(dist_live, 0, sizeof dist_live);
    for (p = 0; p < NLIVE; p++) {                      /* WHEEL: the rotary's phases, speeds and share (the rotor turns without a note), the voices' leftovers */
        drw_t[p].rph[0] = drw_t[p].rph[1] = 0;
        drw_t[p].rsp[0] = drw_t[p].rsp[1] = 0;
        drw_t[p].rwet = 0;
        for (i = 0; i < NVOICE; i++)
            if (!trk[p].v[i].active)
                memset(&drw_v[p][i], 0, sizeof drw_v[p][i]);
    }
    for (p = 0; p < NLIVE; p++) {                      /* FM6's LFO (running without a note) */
        fm6_lfo[p].phase = fm6_lfo[p].dstate = 0;
        fm6_lfo[p].rnd = 0;
    }
    BENCH_RESET_BUS(fx, hl);                           /* the effect buses: the chorus, the delay's modulation, the reverbs' drifts and write points */
    BENCH_RESET_BUS(fx_b, hl_b);
    fx.lo_lp = fx.lo_k = fx.lo_g = fx_b.lo_lp = fx_b.lo_k = fx_b.lo_g = 0;   /* the reverb's LOW CUT from rest (as snd_lines_clear): in the joined line the owner's M07 (LOW CUT 79) left its low-pass set and
                                                       * M08 after it did not repeat (benchsim); found by clearing the fx fields one at a time, join0233 */
    dlyev_clear();                                     /* the delay's RANDOM / OCT UP voices and generator */
    for (p = 0; p < NLIVE; p++)
        memset(eqs[p].s, 0, sizeof eqs[p].s);
    lim_env = lim_env_r = LIM_T;                       /* the master: limiters, SPACE's width, BASS MONO, the compressor, WIDTH, the DC blockers, PERFORM's lines */
    sw_a = 4096, sw_b = 0, sw_em = sw_es = 0, sw_g = 4096;
    memset(&bm, 0, sizeof bm);
    memset(&cmp, 0, sizeof cmp);
    memset(&wdn, 0, sizeof wdn);
    dc_l = dc_r = dce_l = dce_r = 0;
    for (i = 0; i < 16u; i++) {                        /* MIDI: each channel's bend, wheel, pedal and RPN selection (what CC121 does), the MORPH triggers armed again */
        midi_channel_t *c = midi_channel(i);
        c->bend = 0, c->wheel = 0, c->pedal = 0;
        c->rpn_msb = c->rpn_lsb = 127;
        midi_morph_fired[i] = 0;
        midi_expression_channel(i);
    }
    memset(duck_st, 0, sizeof duck_st);                /* DUCK (PUMP): the beat counter, the envelope, the key followers */
    perf_lines_clear();
    vage = 0;                                          /* voice.c: the voices' age clock (the oldest-first steals compare ages) */
}

/* ---- the audio interrupt's half (audio.c, before the half's clock starts) ---- */
static BENCH_FN void bench_emit(uint32_t kind, uint32_t ch, uint32_t a, uint32_t b)
{
    uint32_t pkt;
    ch &= 15u;
    if (kind == TK_ON)
        pkt = 0x09u | (0x90u | ch) << 8 | (a & 127u) << 16 | (b & 127u) << 24;
    else if (kind == TK_OFF)
        pkt = 0x08u | (0x80u | ch) << 8 | (a & 127u) << 16;
    else if (kind == TK_AT)
        pkt = 0x0Du | (0xD0u | ch) << 8 | (a & 127u) << 16;
    else
        pkt = 0x0Bu | (0xB0u | ch) << 8 | (a & 127u) << 16 | (b & 127u) << 24;
    if (!midi_enqueue(pkt, 1u) && btl.lost < 65535u)
        btl.lost++;
}
static BENCH_FN void bench_half(void)
{
    uint32_t h, i;
    if (btl.state == TS_ARMED) {
#ifdef BENCH_HOST
        if (!bench_noreset)                            /* (host test: the reset switched off, to see that the repeat check can fail) */
#endif
            bench_reset_phases();
        btl.h = 0;
        btl.state = TS_RUN;
    } else
        btl.h++;
    h = btl.h;
    if (h == btl.m0) {                                  /* the CPU window opens: the meter's counters as they are at the start of this half */
        btl.n0 = cpu_n, btl.s0 = cpu_sum, btl.c0 = cpu_miss, btl.d0 = shed_count;
        cpu_pk = 0;
        btl.vsum = btl.vn = btl.lfn = 0;
        btl.win = 1;
    }
    fm1_irq_off();                                     /* the MIDI-in ring has other writers */
    while (btl.r != btl.w && btl.ev[btl.r % TL_N].h <= h) {
        const tl_ev_t *e = &btl.ev[btl.r % TL_N];
        if (e->kind == TK_RAMP) {
            for (i = 0; i < TL_RAMPS && btl.rp[i].on; i++)
                ;
            if (i == TL_RAMPS) {
                btl.lost++;
            } else {
                tl_ramp_t *q = &btl.rp[i];
                q->h0 = e->h, q->len = e->len ? e->len : 1u, q->ch = e->ch, q->cc = e->a, q->from = e->b, q->to = e->c, q->on = 1;
                q->last = 255;
            }
        } else if (e->kind == TK_FLASH)
            btl.flash_req = 1;
        else
            bench_emit(e->kind, e->ch, e->a, e->b);
        RING_PUBLISH();
        btl.r++;
    }
    for (i = 0; i < TL_RAMPS; i++) {                   /* the ramps: one value a half, when it changed */
        tl_ramp_t *q = &btl.rp[i];
        uint32_t k;
        int32_t v;
        if (!q->on)
            continue;
        k = h - q->h0;
        v = k >= q->len ? (int32_t)q->to : (int32_t)q->from + ((int32_t)q->to - (int32_t)q->from) * (int32_t)k / (int32_t)q->len;
        if ((uint8_t)v != q->last) {
            q->last = (uint8_t)v;
            if (q->cc & 0x80u)
                bench_emit(TK_AT, q->ch, (uint32_t)v, 0);
            else
                bench_emit(TK_CC, q->ch, q->cc, (uint32_t)v);
        }
        if (k >= q->len)
            q->on = 0;
    }
    fm1_irq_on();
    if (btl.win) {
        btl.vsum += bench_voices();
        btl.vn++;
        btl.lfn += ovl_lite != 0u;
    }
    if (btl.win && (h >= btl.m1 || h >= btl.dur)) {      /* the window closes: the counters as they are at the start of this half */
        btl.n1 = cpu_n, btl.s1 = cpu_sum, btl.c1 = cpu_miss, btl.d1 = shed_count, btl.pk1 = cpu_pk;
        btl.win = 0;
    }
    if (h >= btl.dur) {
        btl.state = TS_DONE;
        bench_tl_on = 0;
    }
}

/* ---- the result line (main loop) ---- */
static BENCH_FN uint32_t bench_uac(uint32_t k)         /* the USB audio counters (FELUCCA_UAC builds): 0 hz, 1 underruns, 2 overruns, 3 missed, 4 stalls */
{
#if defined(FELUCCA_UAC) && FELUCCA_UAC && !defined(BENCH_HOST)
    return k == 0u ? (uac.r48 ? 48000u : 44100u) : k == 1u ? uac.underruns : k == 2u ? uac.overruns : k == 3u ? uac.missed : uac.stalls;
#else
    return k == 0u ? 44100u : 0u;
#endif
}
static BENCH_FN void bench_result(void)
{
    char b[1024], *d;
    bench_tcv_t tv;
    bench_tmv_t tm;
    uint32_t n = btl.n1 - btl.n0, sum = btl.s1 - btl.s0, rp = bench_rp_now(), p;
    uint32_t avg = n ? (uint32_t)bench_clamp((int32_t)((uint64_t)sum * 1000u / ((uint64_t)n * CPU_BUDGET_US)), 0, 9999) : 0u;
    uint32_t pk = (uint32_t)bench_clamp((int32_t)((uint64_t)btl.pk1 * 1000u / CPU_BUDGET_US), 0, 9999);
    d = bench_put(b, "BTH,name,status,avg,peak,miss,shed,lfx,vox,halves,notes1,notes2,why1,why2,budget,ramcode,boots,lost,flashes,uac_hz,uac_under,uac_over,uac_missed,uac_stalls,erase_max_us,c1,c1pk,c1late,c1over,c1units,c1holds,tc,tcwhy,tcblocks,tcintime,tclate,tcvlate,tcsilent,tcwait,tcown,tcrefused,tcnoans,tcpauses,tcresumes,tctrims,tcparks,tcvlok,tcpfault,tcprun,tcpwin,tcplate,c1stack,tmjobs,tmposted,tmowndown,tmownpark,tmownpause,tmowndl,tmbadclk,tmsdmax,tmsdmean,tmrtmax,tmrtmean,tmmgmin,tmmgmean,tmvn,tmvrtmax,tmvrtmean,tmvsdmax,tmvblk0,tmvblk1,tmvblk2,tmvblk3,hsd0,hsd1,hsd2,hsd3,hsd4,hsd5,hsd6,hsd7,hrt0,hrt1,hrt2,hrt3,hrt4,hrt5,hrt6,hrt7,hmg0,hmg1,hmg2,hmg3,hmg4,hmg5,hmg6,hmg7,tmstmax,tmst0,tmst1,tmst2,tmst3,tmdl0,tmdl1,tmdl2,tmdl3,tmslack,tmcalm,tmlrun,tmvrun,tmownfl,tmvwink,tmvwinms,tmfloor\r\n");
    (void)d;
    BENCH_OUT(b);
    d = bench_put(b, "BT,");
    d = bench_put(d, btl.name);
    d = bench_put(d, btl.state == TS_STOP ? ",STOPPED," : ",DONE,");
    d = bench_x10(d, (int32_t)avg);
    d = bench_put(d, ",");
    d = bench_x10(d, (int32_t)pk);
    d = bench_put(d, ",");
    d = bench_num(d, (int32_t)bench_clamp((int32_t)(btl.c1 - btl.c0), 0, 65535));
    d = bench_put(d, ",");
    d = bench_num(d, (int32_t)bench_clamp((int32_t)(btl.d1 - btl.d0), 0, 65535));
    d = bench_put(d, ",");
    d = bench_num(d, (int32_t)(btl.vn ? btl.lfn * 100u / btl.vn : 0u));
    d = bench_put(d, ",");
    d = bench_x10(d, (int32_t)(btl.vn ? btl.vsum * 10u / btl.vn : 0u));
    d = bench_put(d, ",");
    d = bench_num(d, (int32_t)n);
    for (p = 0; p < 2u; p++) {                         /* the NOTES budget's numbers of the parts as they stand at the end, and what limits them */
        d = bench_put(d, ",");
        d = bench_num(d, part_notes_shown((int)p));
    }
    for (p = 0; p < 2u; p++) {
        d = bench_put(d, ",");
        d = bench_put(d, BENCH_NR[part_notes_reason((int)p) % NR_N]);
    }
    d = bench_put(d, bench_bopen ? ",OPEN," : ",ON,");
    d = bench_put(d, bench_rp_word(rp));
    d = bench_put(d, ",");
    d = bench_num(d, (int32_t)BENCH_BOOTS);
    d = bench_put(d, ",");
    d = bench_num(d, btl.lost);
    d = bench_put(d, ",");
    d = bench_num(d, btl.flashes);
    for (p = 0; p < 5u; p++) {
        d = bench_put(d, ",");
        d = bench_num(d, (int32_t)bench_uac(p));
    }
    d = bench_put(d, ",");
    d = bench_num(d, (int32_t)felucca_dbg.erase_max_us);
    memset(&tv, 0, sizeof tv);
    memset(&tm, 0, sizeof tm);
#if BENCH_TM
    bench_tm_get(&tm);
#endif
#if BENCH_TC
    bench_tc_get(&tv);
    for (p = 0; p < BTC_N; p++)                        /* the counters of this test: now less the start's */
        tv.c[p] -= btl.tc0.c[p];
    /* core 1: c1 = its busy share (the run times of its jobs over the test, x10 %), c1pk = its busiest half; c1units stays empty; c1late = late blocks, c1over = very late blocks, c1holds = flash parks */
    d = bench_put(d, ",");
    if (BENCH_TM)
        d = bench_x10(d, (int32_t)tm.c1x10);
    d = bench_put(d, ",");
    if (BENCH_TM)
        d = bench_x10(d, (int32_t)tm.c1pkx10);
    d = bench_put(d, ",");
    d = bench_num(d, (int32_t)tv.c[BTC_LATE]);
    d = bench_put(d, ",");
    d = bench_num(d, (int32_t)tv.c[BTC_VLATE]);
    d = bench_put(d, ",,");
    d = bench_num(d, (int32_t)tv.c[BTC_PARKS]);
#else
    d = bench_put(d, ",,,,,,");                        /* (no core 1 in this build) */
#endif
    d = bench_put(d, ",");
    d = bench_tc_mode(d, &tv);
    d = bench_put(d, ",");
    d = bench_tc_why(d, &tv);
    for (p = 0; p < BTC_N; p++) {
        d = bench_put(d, ",");
        if (BENCH_TC)
            d = bench_num(d, (int32_t)tv.c[p]);
    }
    d = bench_put(d, ",");
    if (BENCH_TC)
        d = bench_num(d, (int32_t)tv.stack);
    {   /* BENCHT2: core 1's timing (fields after c1stack; all empty in a build without it) */
        int32_t f[17];
        uint32_t q;
        f[0] = (int32_t)tm.n, f[1] = (int32_t)tm.posted, f[2] = (int32_t)tm.own[0], f[3] = (int32_t)tm.own[1], f[4] = (int32_t)tm.own[2], f[5] = (int32_t)tm.own[3];
        f[6] = (int32_t)tm.badclk, f[7] = tm.sd_max, f[8] = tm.sd_mean, f[9] = tm.rt_max, f[10] = tm.rt_mean, f[11] = tm.mg_min, f[12] = tm.mg_mean;
        f[13] = (int32_t)tm.nv, f[14] = (int32_t)tm.vrt_max, f[15] = (int32_t)tm.vrt_mean, f[16] = (int32_t)tm.vsd_max;
        for (q = 0; q < 17u; q++) {
            d = bench_put(d, ",");
            if (BENCH_TM)
                d = bench_num(d, f[q]);
        }
        for (q = 0; q < 4u + 3u * BTM_NB; q++) {
            d = bench_put(d, ",");
            if (BENCH_TM)
                d = bench_num(d, (int32_t)(q < 4u ? tm.vblk[q] : q < 12u ? tm.hsd[q - 4u] : q < 20u ? tm.hrt[q - 12u] : tm.hmg[q - 20u]));
        }
        for (q = 0; q < 9u; q++) {
            d = bench_put(d, ",");
            if (BENCH_TM)
                d = bench_num(d, (int32_t)(q == 0 ? tm.st_max : q < 5u ? tm.st_mean[q - 1u] : tm.dl_mean[q - 5u]));
        }
        d = bench_put(d, ",");
        if (BENCH_TM)
            d = bench_num(d, (int32_t)tm.slack);
        d = bench_put(d, ",");
        if (BENCH_TM)
            d = bench_num(d, (int32_t)tm.calm_ms);
        d = bench_put(d, ",");
        if (BENCH_TM)
            d = bench_num(d, (int32_t)tm.lrun);
        d = bench_put(d, ",");
        if (BENCH_TM)
            d = bench_num(d, (int32_t)tm.vrun);
        for (q = 0; q < 4u; q++) {                     /* BENCHT3: own because of the floor, the window rule, the floor switch */
            d = bench_put(d, ",");
            if (BENCH_TM)
                d = bench_num(d, (int32_t)(q == 0 ? tm.own[4] : q == 1 ? tm.vwin_k : q == 2 ? tm.vwin_ms : tm.floor));
        }
    }
    bench_put(d, "\r\n");
    BENCH_OUT(b);
}

/* ---- the session ---- */
static BENCH_FN void bench_flash_write(void)           /* a settings save with unchanged values: the record as it is in the flash, written back (the other copy) */
{
#if FELUCCA_FLASH
    persist_t p;
    if (!flash_ok)
        return;
    if (st_load(OBJ_SETTINGS, &p, sizeof p) != (int)sizeof p) {   /* no settings record yet (a new unit): the settings as they stand, which the unit would save itself */
        persist_base(&p);
        settings_export(&p);
    }
    if (st_save(OBJ_SETTINGS, &p, sizeof p) == 0 && btl.flashes < 255u)
        btl.flashes++;
#endif
}
static BENCH_FN void bench_sess_begin(void)
{
    if (btl.sess)
        return;
    if (!bench.holds)
        bench_keep_session();
    bench.holds = 1;
    btl.sess = 1;
}
static BENCH_FN void bench_sess_end(void)
{
    if (!btl.sess)
        return;
    bench_tl_on = 0;
    btl.state = TS_IDLE;
    btl.r = btl.w;
    bench_restore_session();
    btl.sess = 0;
    bench.holds = 0;
    kb_boot_hold = 0;
    if (bench.persist) {
        persist_pending = 1;
        bench.persist = 0;
    }
}
static BENCH_FN void bench_tl_tick(void)  /* every ui_input pass (bench_main_tick), main loop */
{
    if (!btl.sess)
        return;
    kb_boot_hold = 1;                                  /* (as a row: the keys silent, the screen awake, nothing reaches the flash) */
    scrn.idle = fm1_ms;
    ui.mset_dirty = 0;
    as.t = fm1_ms;
    if (persist_pending == 1u) {
        bench.persist = 1;
        persist_pending = 0;
    }
    if ((btl.state == TS_DONE || btl.state == TS_STOP) && !btl.reported) {
        btl.reported = 1;
        bench_result();
    }
    if (btl.flash_req) {
        btl.flash_req = 0;
        bench_flash_write();
    }
    if (btl.state != TS_RUN && btl.state != TS_ARMED && fm1_ms - btl.last_cmd > 180000u)
        bench_sess_end();                              /* (the runner went away) */
}

/* ---- the words ---- */
static BENCH_FN int bench_tok(const char **pp, char *out, uint32_t max)   /* the next word of the line; 0 if none */
{
    const char *s = *pp;
    uint32_t n = 0;
    while (*s == ' ')
        s++;
    if (!*s)
        return 0;
    while (*s && *s != ' ') {
        if (n + 1u < max)
            out[n++] = *s;
        s++;
    }
    out[n] = 0;
    *pp = s;
    return 1;
}
static BENCH_FN int bench_int(const char *s, int32_t *v)               /* a decimal number, signed */
{
    int32_t x = 0, neg = 0;
    if (*s == '-')
        neg = 1, s++;
    if (*s < '0' || *s > '9')
        return 0;
    for (; *s >= '0' && *s <= '9'; s++)
        x = x * 10 + (*s - '0');
    if (*s)
        return 0;
    *v = neg ? -x : x;
    return 1;
}
static BENCH_FN int bench_arg(const char **pp, int32_t *v)             /* the next word as a number */
{
    char w[16];
    return bench_tok(pp, w, sizeof w) && bench_int(w, v);
}
static BENCH_FN int bench_ieq(const char *a, const char *b)            /* equal, ignoring case */
{
    for (; *a && *b; a++, b++) {
        char x = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a, y = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;
        if (x != y)
            return 0;
    }
    return !*a && !*b;
}
static BENCH_FN void bench_say(const char *a, const char *b, const char *c)   /* "a b c" and a line end */
{
    BENCH_REPLY(a);
    if (b) {
        BENCH_REPLY(" ");
        BENCH_REPLY(b);
    }
    if (c) {
        BENCH_REPLY(" ");
        BENCH_REPLY(c);
    }
    BENCH_REPLY("\r\n");
}
static BENCH_FN void bench_err(const char *why) { bench_say("ERR", why, 0); }
static BENCH_FN int bench_val(const param_desc_t *d, const char *w, int32_t *v)   /* a value: a number, or an enum's name */
{
    uint32_t i;
    if (bench_int(w, v))
        return 1;
    if (d->fmt == F_ENUM && d->names)
        for (i = 0; i <= (uint32_t)d->max; i++)
            if (bench_ieq(d->names[i], w)) {
                *v = (int32_t)i;
                return 1;
            }
    return 0;
}
static BENCH_FN void bench_con_set(const char *p)
{
    char w[24], x[24];
    int32_t id = -1, v = 0, part = 0;
    const param_desc_t *d = 0;
    uint32_t i;
    if (!bench_tok(&p, w, sizeof w) || !bench_tok(&p, x, sizeof x)) {
        bench_err("usage: set PART|g NAME VALUE");
        return;
    }
    if (w[0] == 'g' && !w[1]) {                        /* a global value */
        if (!bench_int(x, &id))
            for (i = 0; i < G_COUNT; i++)
                if (GP[i].label && bench_ieq(GP[i].label, x)) {
                    id = (int32_t)i;
                    break;
                }
        if (id < 0 || id >= (int32_t)G_COUNT) {
            bench_err("no such global value");
            return;
        }
        d = &GP[id];
    } else {
        if (!bench_int(w, &part) || part < 1 || part > 2) {
            bench_err("PART is 1 or 2, or g");
            return;
        }
        if (!bench_int(x, &id))
            for (i = 0; i < P_E0; i++)
                if (TP[i].label && TP[i].label[0] && bench_ieq(TP[i].label, x)) {
                    id = (int32_t)i;
                    break;
                }
        if (id < 0 || id >= (int32_t)P_COUNT) {
            bench_err("no such value");
            return;
        }
        d = param_desc_of(trk[part - 1].eng_req % NENGINES, (uint32_t)id);
    }
    if (!bench_tok(&p, w, sizeof w) || !bench_val(d, w, &v)) {
        bench_err("no such value to set");
        return;
    }
    if (d->max > d->min)
        v = bench_clamp(v, d->min, d->max);
    if (part)
        trk[part - 1].p[id] = (int16_t)v;
    else
        song.g[id] = (int16_t)v;
    ui.force = 1;
    bench_say("OK set", 0, 0);
}
static BENCH_FN void bench_con_ev(const char *p)
{
    char k[12], w[12];
    int32_t t, ch = 1, a = 0, b = 0, c = 0, ms = 0;
    tl_ev_t *e;
    uint32_t h;
    if (!bench_tok(&p, k, sizeof k)) {
        bench_err("usage: ev T KIND ..");
        return;
    }
    if (bench_ieq(k, "clear")) {
        if (btl.state == TS_RUN || btl.state == TS_ARMED) {
            bench_err("running");
            return;
        }
        btl.r = btl.w;
        btl.last_h = 0;
        bench_say("OK clear", 0, 0);
        return;
    }
    if (!bench_int(k, &t) || t < 0 || t > 3600000 || !bench_tok(&p, k, sizeof k)) {
        bench_err("usage: ev T KIND ..");
        return;
    }
    h = bench_ms_to_h((uint32_t)t);
    if (btl.w - btl.r >= TL_N) {
        bench_err("full");
        return;
    }
    if (h < btl.last_h) {
        bench_err("order");
        return;
    }
    e = &btl.ev[btl.w % TL_N];
    memset(e, 0, sizeof *e);
    e->h = h;
    if (bench_ieq(k, "flash"))
        e->kind = TK_FLASH;
    else {
        if (!bench_arg(&p, &ch) || ch < 1 || ch > 16) {
            bench_err("CH is 1..16");
            return;
        }
        if (bench_ieq(k, "on")) {
            if (!bench_arg(&p, &a) || !bench_arg(&p, &b) || a < 0 || a > 127 || b < 1 || b > 127) {
                bench_err("usage: ev T on CH NOTE VEL");
                return;
            }
            e->kind = TK_ON, e->a = (uint8_t)a, e->b = (uint8_t)b;
        } else if (bench_ieq(k, "off")) {
            if (!bench_arg(&p, &a) || a < 0 || a > 127) {
                bench_err("usage: ev T off CH NOTE");
                return;
            }
            e->kind = TK_OFF, e->a = (uint8_t)a;
        } else if (bench_ieq(k, "at")) {
            if (!bench_arg(&p, &a) || a < 0 || a > 127) {
                bench_err("usage: ev T at CH VAL");
                return;
            }
            e->kind = TK_AT, e->a = (uint8_t)a;
        } else if (bench_ieq(k, "cc")) {
            if (!bench_arg(&p, &a) || !bench_arg(&p, &b) || a < 0 || a > 127 || b < 0 || b > 127) {
                bench_err("usage: ev T cc CH CC VAL");
                return;
            }
            e->kind = TK_CC, e->a = (uint8_t)a, e->b = (uint8_t)b;
        } else if (bench_ieq(k, "ramp")) {
            if (!bench_tok(&p, w, sizeof w) || !bench_arg(&p, &b) || !bench_arg(&p, &c) || !bench_arg(&p, &ms) || b < 0 || b > 127 || c < 0 || c > 127 ||
                ms < 1 || ms > 180000 || (!bench_ieq(w, "AT") && (!bench_int(w, &a) || a < 0 || a > 127))) {
                bench_err("usage: ev T ramp CH CC|AT A B MS");
                return;
            }
            e->kind = TK_RAMP, e->a = bench_ieq(w, "AT") ? 0x80u : (uint8_t)a, e->b = (uint8_t)b, e->c = (uint8_t)c;
            e->len = (uint16_t)(bench_ms_to_h((uint32_t)ms) ? bench_ms_to_h((uint32_t)ms) : 1u);
        } else {
            bench_err("KIND: on off at cc ramp flash clear");
            return;
        }
        e->ch = (uint8_t)(ch - 1);
    }
    btl.last_h = h;
    RING_PUBLISH();
    btl.w++;
    {
        char n[12];
        fmt_int(n, (int32_t)(TL_N - (btl.w - btl.r)));
        bench_say("OK", n, 0);
    }
}
static BENCH_FN void bench_con(const char *p)
{
    char w[16], x[24];
    int32_t n = 0, m = 0;
    btl.last_cmd = fm1_ms;
    if (!bench_tok(&p, w, sizeof w)) {
        bench_say("ERR", "bench info reset end load mode set scene budget ev measure run stop save-test slack calm pause lrun vrun vwin floor tcm", 0);
        return;
    }
    if (bench_ieq(w, "info") || bench_ieq(w, "state")) {
        char b[256], *d;
        if (bench_ieq(w, "info")) {
            bench_resolve();
            bench_line_info(b);
            BENCH_REPLY(b);
        }
        d = bench_put(b, "BENCH,STATE,sess ");
        d = bench_num(d, btl.sess);
        d = bench_put(d, ",run ");
        d = bench_num(d, btl.state);
        d = bench_put(d, ",evfree ");
        d = bench_num(d, (int32_t)(TL_N - (btl.w - btl.r)));
        d = bench_put(d, ",lost ");
        d = bench_num(d, btl.lost);
        d = bench_put(d, ",halves ");
        d = bench_num(d, (int32_t)btl.h);
        d = bench_put(d, ",uptime_ms ");
        d = bench_num(d, (int32_t)fm1_ms);
        d = bench_put(d, ",scn ");
        d = bench_num(d, (int32_t)BENCH_NSCN);
        bench_put(d, "\r\n");
        BENCH_REPLY(b);
        return;
    }
    if (bench.on) {
        bench_err("a BENCH run is going (OCT- stops it)");
        return;
    }
    if (bench_ieq(w, "reset")) {
        bench_tl_on = 0;
        btl.state = TS_IDLE;
        btl.r = btl.w;
        btl.last_h = 0;
        memset(btl.rp, 0, sizeof btl.rp);
        btl.ms0 = 0, btl.ms1 = 0xFFFFFFFFu;
        bench_sess_begin();
        if (ui.menu == 5u)                             /* (the BENCH screen left, as OCT- does: it takes the UI's frame, so MORPH, program changes and the syncs would not run) */
            ui.menu = 1, ui.force = 1;
        transport_req = 4;                             /* STOP ALL; then every value from INIT (IDLE ST: STEREO, both parts' power-on sounds) */
        bench_set(&BENCH_SCN[0]);
        bench_bopen = 0;                               /* (the NOTES budget as the owner plays it; `scene` and `budget` hold it open) */
#if BENCH_TM
        for (n = 0; n < 7; n++)                        /* (the margin settings back to the build's: slack, calm, late run, very late run, window K and ms, floor) */
            bench_tm_set((uint32_t)n, 0xFFFFFFFFu);
        bench_tm_clear();
#endif
        bench_say("OK reset", 0, 0);
        return;
    }
    if (bench_ieq(w, "end")) {
        bench_sess_end();
#if BENCH_TM
        for (n = 0; n < 7; n++)
            bench_tm_set((uint32_t)n, 0xFFFFFFFFu);
#endif
        bench_say("OK end", 0, 0);
        return;
    }
#if BENCH_TM
    if (bench_ieq(w, "slack") || bench_ieq(w, "calm") || bench_ieq(w, "pause") || bench_ieq(w, "lrun") || bench_ieq(w, "vrun")) {   /* BENCHT2: the margin settings, until bench reset / end */
        char kind = w[0];
        uint32_t what = kind == 's' ? 0u : kind == 'c' ? 1u : kind == 'l' ? 2u : 3u;
        x[0] = 0;
        if (kind == 'p') {
            if (!bench_tok(&p, x, sizeof x) || !(bench_ieq(x, "on") || bench_ieq(x, "off"))) {
                bench_err("usage: pause on | off (off: no pause after late or very late blocks; real faults still pause)");
                return;
            }
            bench_tm_set(2, bench_ieq(x, "off") ? 1000u : 0xFFFFFFFFu);
            bench_tm_set(3, bench_ieq(x, "off") ? 0u : 0xFFFFFFFFu);      /* (vrun 0: the very late run rule is off, as it is by default since BENCHT4) */
            bench_tm_set(4, bench_ieq(x, "off") ? 0u : 0xFFFFFFFFu);   /* (the window rule too; real faults still pause) */
            bench_say("OK pause", bench_ieq(x, "off") ? "off" : "on", 0);
            return;
        }
        if (bench_tok(&p, x, sizeof x) && (bench_ieq(x, "default") || bench_ieq(x, "def")))
            n = -1;
        else if (!x[0] || !bench_int(x, &n) || n < 0 || n > (what == 0u ? 400000 : what == 1u ? 80000 : 1000) || (what == 2u && n < 1)) {
            bench_err(what == 0u ? "usage: slack TICKS 0..400000 | default (24 ticks = 1 us)" : what == 1u ? "usage: calm MS 0..80000 | default" :
                      what == 2u ? "usage: lrun N 1..1000 | default (late blocks in a row that pause the jobs)" : "usage: vrun N 0..1000 | default (very late blocks in a row that pause the jobs; 0: off, the default since BENCHT4)");
            return;
        }
        bench_tm_set(what, n < 0 ? 0xFFFFFFFFu : (uint32_t)n);
        bench_say("OK", w, x);
        return;
    }
    if (bench_ieq(w, "vwin") || bench_ieq(w, "floor")) {   /* BENCHT3: vwin K [MS] (K very late blocks within MS pause the jobs; K 0: off), floor 0|1 (the deadline's floor) */
        char x2[16];
        int32_t k = -1, ms = -1;
        uint32_t isf = w[0] == 'f' || w[0] == 'F';
        x[0] = 0, x2[0] = 0;
        if (bench_tok(&p, x, sizeof x) && (bench_ieq(x, "default") || bench_ieq(x, "def"))) {
            bench_tm_set(isf ? 6u : 4u, 0xFFFFFFFFu);
            if (!isf)
                bench_tm_set(5u, 0xFFFFFFFFu);
            bench_say("OK", w, x);
            return;
        }
        if (!x[0] || !bench_int(x, &k) || k < 0 || k > (isf ? 1 : 16)) {
            bench_err(isf ? "usage: floor 0 | 1 | default (the deadline's floor)" : "usage: vwin K [MS] K 0..16 MS 1..60000 | default");
            return;
        }
        if (!isf && bench_tok(&p, x2, sizeof x2) && (!bench_int(x2, &ms) || ms < 1 || ms > 60000)) {
            bench_err("usage: vwin K [MS] K 0..16 MS 1..60000 | default");
            return;
        }
        bench_tm_set(isf ? 6u : 4u, (uint32_t)k);
        if (!isf && ms > 0)
            bench_tm_set(5u, (uint32_t)ms);
        bench_say("OK", w, x);
        return;
    }
    if (bench_ieq(w, "tcm")) {                         /* the newest very late jobs, oldest first */
        bench_tmv_t m;
        char b[200], *d;
        uint32_t i;
        bench_tm_get(&m);
        d = bench_put(b, "BENCH,TCM,very late jobs ");
        d = bench_num(d, (int32_t)m.ev_n);
        d = bench_put(d, ",shown ");
        d = bench_num(d, (int32_t)m.ev_k);
        bench_put(d, " (ms since the test's start, block in the half, voices part 1 + part 2, delay, run, margin in us)\r\n");
        BENCH_REPLY(b);
        for (i = 0; i < m.ev_k; i++) {
            d = bench_put(b, "BENCH,TCM,");
            d = bench_num(d, (int32_t)m.ev[i].at_ms);
            d = bench_put(d, ",");
            d = bench_num(d, (int32_t)m.ev[i].blk);
            d = bench_put(d, ",");
            d = bench_num(d, (int32_t)m.ev[i].a1);
            d = bench_put(d, ",");
            d = bench_num(d, (int32_t)m.ev[i].a2);
            d = bench_put(d, ",");
            d = bench_num(d, m.ev[i].sd);
            d = bench_put(d, ",");
            d = bench_num(d, m.ev[i].rt);
            d = bench_put(d, ",");
            d = bench_num(d, m.ev[i].mg);
            bench_put(d, "\r\n");
            BENCH_REPLY(b);
        }
        return;
    }
#endif
    if (!btl.sess) {
        bench_err("no session: bench reset first");
        return;
    }
    if (bench_ieq(w, "load")) {
        char nm[16];
        if (!bench_tok(&p, w, sizeof w)) {
            bench_err("usage: load multi N | patch PART NN");
            return;
        }
        if (bench_ieq(w, "multi")) {
            if (!bench_arg(&p, &n) || n < 1 || n > (int32_t)MU_SLOTS || !project_used((uint32_t)n - 1u)) {
                bench_err("no such multi");
                return;
            }
            pc_recall(PCQ_MULTI, (uint32_t)n - 1u, 0);
            project_name((uint32_t)n - 1u, nm);
            fmt_int(x, n);
            bench_say("OK multi", x, nm);
        } else if (bench_ieq(w, "patch")) {
            if (!bench_arg(&p, &n) || !bench_arg(&p, &m) || n < 1 || n > 2 || m < 1 || m > (int32_t)UP_SLOTS) {
                bench_err("usage: load patch PART(1,2) NN(1..128)");
                return;
            }
            pc_recall((uint32_t)n - 1u, (uint32_t)m - 1u, 0);
            sl_name((uint32_t)m - 1u, nm);
            fmt_int(x, m);
            bench_say("OK patch", x, nm);
        } else
            bench_err("usage: load multi N | patch PART NN");
        return;
    }
    if (bench_ieq(w, "mode")) {
        static const char *const MN[3] BENCH_RO = {"DUEL", "STEREO", "LAYER"};
        uint32_t i;
        if (bench_tok(&p, x, sizeof x))
            for (i = 0; i < 3u; i++)
                if (bench_ieq(MN[i], x)) {
                    duell_mode_req = (uint8_t)i;       /* (DM_SPLIT 0, DM_STEREO 1, DM_LAYER 2: the order of the log's MN) */
                    bench_say("OK mode", MN[i], 0);
                    return;
                }
        bench_err("mode: DUEL STEREO LAYER");
        return;
    }
    if (bench_ieq(w, "set")) {
        bench_con_set(p);
        return;
    }
    if (bench_ieq(w, "scene")) {
        uint32_t i;
        while (*p == ' ')
            p++;
        i = bench_find(p);
        if (i == BR_NONE) {
            bench_err("no such row");
            return;
        }
        bench_set(&BENCH_SCN[i]);                      /* (a cost row: the budget open) */
        bench_say("OK scene", p, 0);
        return;
    }
    if (bench_ieq(w, "budget")) {
        if (bench_tok(&p, x, sizeof x) && (bench_ieq(x, "OPEN") || bench_ieq(x, "ON"))) {
            bench_bopen = (uint8_t)bench_ieq(x, "OPEN");
            bench_say("OK budget", bench_bopen ? "OPEN" : "ON", 0);
        } else
            bench_err("budget: OPEN or ON");
        return;
    }
    if (bench_ieq(w, "ev")) {
        bench_con_ev(p);
        return;
    }
    if (bench_ieq(w, "measure")) {
        if (bench_arg(&p, &n) && bench_arg(&p, &m) && n >= 0 && m > n) {
            btl.ms0 = (uint32_t)n, btl.ms1 = (uint32_t)m;
            bench_say("OK measure", 0, 0);
        } else
            bench_err("usage: measure T0 T1 (ms)");
        return;
    }
    if (bench_ieq(w, "run")) {
        uint32_t i;
        if (btl.state == TS_RUN || btl.state == TS_ARMED) {
            bench_err("a test is running");
            return;
        }
        if (!bench_tok(&p, x, sizeof x) || !bench_arg(&p, &n) || n < 100 || n > 3600000) {
            bench_err("usage: run NAME MS");
            return;
        }
        for (i = 0; x[i] && i < sizeof btl.name - 1u; i++)
            btl.name[i] = x[i];
        btl.name[i] = 0;
        btl.dur = bench_ms_to_h((uint32_t)n);
        btl.m0 = bench_ms_to_h(btl.ms0);
        btl.m1 = btl.ms1 == 0xFFFFFFFFu ? btl.dur : bench_ms_to_h(btl.ms1);
        btl.lost = 0, btl.flashes = 0, btl.win = 0, btl.flash_req = 0, btl.reported = 0;
        btl.n1 = btl.n0 = btl.s1 = btl.s0 = btl.c1 = btl.c0 = btl.d1 = btl.d0 = btl.pk1 = 0;
        btl.vsum = btl.vn = btl.lfn = 0;
#if BENCH_TC
        bench_tc_get(&btl.tc0);
#endif
#if BENCH_TM
        bench_tm_clear();                              /* (the audio interrupt clears the timing figures at its next block) */
#endif
        memset(btl.rp, 0, sizeof btl.rp);
        btl.h = 0;
        btl.state = TS_ARMED;
        RING_PUBLISH();
        bench_tl_on = 1;
        bench_say("OK run", btl.name, 0);
        return;
    }
    if (bench_ieq(w, "stop")) {
        if (btl.state == TS_RUN || btl.state == TS_ARMED) {
            bench_tl_on = 0;
            btl.state = TS_STOP;
        }
        bench_say("OK stop", 0, 0);
        return;
    }
    if (bench_ieq(w, "save-test")) {
        bench_flash_write();
        bench_say(btl.flashes ? "OK save-test" : "ERR save-test: nothing written", 0, 0);
        return;
    }
    bench_err("unknown word");
}
