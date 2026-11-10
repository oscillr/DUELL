/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL NOTES budget (docs/CPU_PLAN.md section 5; budget.c): how many notes each part may play at once, worked out from what is switched
 * on, so the predicted worst case stays under BUDGET_LIMIT of the audio time budget (one core). All costs are in 1/100 of a percent of the
 * time budget of a half buffer (10000 = the whole budget), the scale of tests/cpu_bench.c and tests/bench_baseline.txt (3281.25 host
 * instructions a sample = 100 %). The functions are pure: the same settings give the same numbers, whenever and in whatever order they
 * are called. Unity build: budget.c is included after fx.c (it reads the master and bus settings), voice.c only reads bnotes[]. */
#ifndef DUELL_BUDGET_H
#define DUELL_BUDGET_H

#define BUDGET_LIMIT 84u        /* the share of the time budget the worst case may use (percent), the margin below applied to every cost (DUELL 1.1 item 132, owner: 80 -> 90; item 133: 88 with the
                                 * margin 1.15). DUELL 1.1 item 137 (owner: the model charges the true measured cost, margin 1.00; cap = the most notes whose true model load + 6 points of measured
                                 * steady peak over the average (HANDOFF_V2 item 130) fits, for core 0 and core 1): core 0 / one core: 90 - 6 = 84; core 1: block 0's job (posted ~0.27 block into
                                 * the half, the part's true cost) + 6 points must end before the runner's wait limit, which keeps its 1.15 table (BUDGET_DL_MARGIN): over the 19,472 setups of
                                 * tests/deadline_test.c with both parts at their caps that holds up to 81 (least slack 8.9 points; 82: 3.5, 84: -5.3 = the job past the wait limit = a silent part-2
                                 * block + pause). So 81: true peak <= 87 % on core 0. DUELL 1.1 item 142 (owner-approved, HANDOFF_V2 item 139): the wait limit is the later of the 1.15 one
                                 * and the true-cost one less TCR_SLACK (twocore_run.c tcr_deadline); core 1's job k (every block) + 6 points then ends before it at the caps up to 86
                                 * (tests/deadline_test.c CORE 1: least slack after 6 points 84: 10.9, 85: 6.9, 86: 2.9, 87: -1.1; after 13 points at 84: 3.9), so core 0's own rule binds:
                                 * 90 - 6 = 84. Predicted true peak <= 90 % on core 0. Read only by budget.c budget_solve2 and budget_pool (both rules; the pool: 2 x this over the two cores); the runner's deadline does not read it */
#define BUDGET_MARGIN 100u      /* every cost of the table below times this (percent) for the NOTES caps. DUELL 1.1 item 137: 115 -> 100, the table is the unit's own COST-CAL measurement (item 116) and
                                 * the raw model was >= the measured average in every held-out run and >= the measured steady peak in every clean POLY-MAX row (item 130) */
#define BUDGET_DL_MARGIN 115u   /* DUELL 1.1 item 137: the two-core runner's deadline and wait limit (twocore_run.c tcr_deadline, through part_dl / global_cost_dl) keep the
                                 * 1.15 table they always had, so they are the same bit for bit (tests/deadline_test.c); only the block-0 take-over gets a true-cost second chance (tcr_deadline_true)
                                 * and (item 142) the wait limit is the later of the 1.15 one and the true-cost one less TCR_SLACK */
#define BUDGET_HO 66u           /* DUELL 1.1 item 90, the two-core rule: the hand-over's cost on each core, host instructions a sample (2 %: the one-engine tests of the owner's BENCHM1 read
                                 * 1.5 to 2.0 points more on core 0 with core 1 on than BENCHC1 without it). DUELL 1.1 item 132 (owner): NOT CHARGED any more (COST-CAL's estimate was
                                 * -28, i.e. nothing): budget.c no longer reads it; kept as the figure tools/cost_fit.py fits and reads. The runner's deadline never read it */

/* what limits a part's number (part_notes_reason): the item whose removal would give the part the most notes */
enum { NR_NONE,                 /* nothing: the part has all it can have (every voice slot) */
       NR_ENGINE,               /* the engine itself: its voice cost, or what its memory holds (PHYS 3; FM6) */
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
static void part_dl(const track_t *t, uint32_t *fixed, uint32_t *voice);   /* item 137: part_fixed and part_cost with BUDGET_DL_MARGIN (1.15), for the two-core runner's deadline and wait limit only
                                                 * (item 142: both from one bd_part) */
static uint32_t global_cost_dl(void);           /* .. and global_cost */
static uint32_t part_notes(int part);           /* the budget's number of notes for the part by the rule in force, 1 .. its ceiling (two cores: the pool with the other part silent; budget.c budget_solve2) */
static uint8_t part_notes_shown(int part);      /* the same number (the bench's rows) */
static uint8_t part_notes_reason(int part);     /* NR_*: what limits it */
static void budget_poll(void);                  /* main loop, every frame: hands the numbers and the voice pool to the note-on path (voice.c trk_nvoice, bd_pool) */
static uint32_t bd_two_now(void);               /* DUELL 1.1 item 90: 1 = the voice pool (two cores) is the rule in force (core 1 up, no fault, jobs not paused), 0 = the one-core pair rule */

/* THE DEADLINE'S COST ITEMS, HANDED OVER BY THE MAIN LOOP (2026-11-05; the unit's timers, BENCHVD: working them out in the audio interrupt at every half's first block cost 171 to 188 us there, 6 % of
 * the audio time, the largest single piece of the block that makes a chord start late; working docs/reports/voice-split-bench_2026-11-05.md). budget_poll (main loop, every frame) calls
 * bd_dl_publish: the five items (part_dl of both parts, global_cost_dl) into the copy the audio side is NOT reading, then one byte says which copy is the new one. The runner (twocore_run.c tcr_costs)
 * reads the copy that byte names: the audio interrupt cannot be interrupted by the main loop, so it always reads a whole copy. The items are an estimate for the deal and the deadline; after a change
 * of a setting they are at most one main-loop pass old. Before the first publish (and in host tests that never run the main loop's pass) the runner works them out itself, as before */
typedef struct { uint32_t c1f, c1v, c2f, c2v, g; } bd_dl_t;
static bd_dl_t bd_dl_buf[2];
static volatile uint8_t bd_dl_i, bd_dl_ok;
static void bd_dl_publish(void);

#endif
