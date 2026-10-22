/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL NOTES budget (docs/CPU_PLAN.md section 5; budget.c): how many notes each part may play at once, worked out from what is switched
 * on, so the predicted worst case stays under BUDGET_LIMIT of the audio time budget (one core). All costs are in 1/100 of a percent of the
 * time budget of a half buffer (10000 = the whole budget), the scale of tests/cpu_bench.c and tests/bench_baseline.txt (3281.25 host
 * instructions a sample = 100 %). The functions are pure: the same settings give the same numbers, whenever and in whatever order they
 * are called. Unity build: budget.c is included after fx.c (it reads the master and bus settings), voice.c only reads bnotes[]. */
#ifndef DUELL_BUDGET_H
#define DUELL_BUDGET_H

#define BUDGET_LIMIT 80u        /* the share of the time budget the worst case may use (percent): 20 % stay for note starts, steal fades, USB and key scan */
#define BUDGET_MARGIN 115u      /* every cost of the table below times this (percent): the table is host figures, the unit is not measured yet (CPU_PLAN section 5.7) */
#define NOTES_MAX 10u           /* the most notes a part is ever given; the voice pool (NVOICE) caps it further until the pool grows */

/* what limits a part's number (part_notes_reason): the item whose removal would give the part the most notes */
enum { NR_NONE,                 /* nothing: the part has all it can have (NOTES_MAX, the pool) */
       NR_ENGINE,               /* the engine itself: its voice cost, or its own voice limit (PHYS 3, VOICE 4, FM6 6) */
       NR_FILTER,               /* the multimode filter's 24 dB slope (a voice) */
       NR_INSERT,               /* the part's INSERT */
       NR_SAT,                  /* the part's SAT */
       NR_EQ,                   /* the part's EQ bands */
       NR_FX,                   /* the effects: chorus, delay mode, reverb type, SHIM, COMP, WIDTH, BASS MONO (the buses of DUEL twice) */
       NR_OTHER,                /* the other part's notes */
       NR_MISC,                 /* the ducker, the matrix, the LFOs, the arp, the morph */
       NR_N };

static uint32_t part_cost(const track_t *t);    /* ONE voice of the part t (engine, filter slope, morph extras) */
static uint32_t part_fixed(const track_t *t);   /* the part's own cost whatever it plays (INSERT, SAT, EQ bands, ducker, matrix, LFOs, arp) */
static uint32_t global_cost(void);              /* the idle mix, the sends and the master stages of the mode */
static uint32_t part_notes(int part);           /* the budget's number of notes for the part, 1 .. NOTES_MAX (0: a part that never sounds) */
static uint8_t part_notes_shown(int part);      /* what the UI shows: the same number, with the engine's own voice limit */
static uint8_t part_notes_reason(int part);     /* NR_*: what limits it */
static void budget_poll(void);                  /* main loop, every frame: hands the numbers to the note-on path (voice.c trk_nvoice) */

#endif
