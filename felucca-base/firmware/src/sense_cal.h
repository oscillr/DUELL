/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
 * KEY SENSE: every tunable of sense.c in one table (key velocity and aftertouch from the FM-1's rubber-dome keys).
 * After Velo's method (Charles Vestal, https://github.com/charlesvestal/fm1-velocity, GPL-3.0-only): a held key's
 * carbon pill is read with its row driven; its resistance falls as the key is pressed harder. Written here from the
 * method and its measurements, no code taken.
 *
 * The first recording from the unit will change THIS FILE only (data, not code). The values below are Velo's
 * numbers from one author's unit (docs/design/VELO_STUDY.md section 1.1) and educated guesses: each line says which.
 *   [V] a Velo measurement   [G] a guess for the recording to replace   [D] a DUELL design choice
 * Converter counts are 10 bit, 0 .. 1023. Times are microseconds. */
#pragma once

#define SC_NKEY 20u                 /* analog keys: notes 0 .. 19 (F3 .. C5); notes 20 .. 26 have no converter */
#define SC_NNOTE 27u

/* ---- the converter slots (hal/fm1_input.h; 11 columns a frame, an analog key once every two frames) ---- */
#define SC_ADC_CON_KEY 0x204Fu      /* [V] the CON word of a key reading: start delay 2 in bits 15..12, bit 3 set, clock divider 7 in bits 2..0 (Velo's documented working
                                     * pair, fm1_adc.h header; the blocking read's divider is 6 = 0x4E). Not yet tried on a unit (~60 us per conversion must fit the 100 us tick) */
#define SC_ADC_CON_SLOW 0xF04Eu     /* [D] the pot and the battery (high impedance sources): the blocking read's word, as before */
#define SC_DRIVE_LEVEL 2u           /* [V] the row's drive strength while a held key is read (0 .. 3; Velo's default 2) */
#define SC_CH_PA7 0u                /* [G] converter channel of row PA7 (notes 9 .. 19); Velo: rows PA7, PA8 are channels 0, 1 */
#define SC_CH_PA8 1u                /* [G] converter channel of row PA8 (notes 0 .. 8, the OCT buttons) */
#define SC_POT_EVERY 16u            /* [D] the MASTER pot is read once every this many frames (~18 ms; it is smoothed per UI frame) */
#define SC_BATT_EVERY 256u          /* [D] the battery once every this many frames (~0.28 s; slow IIR in main.c) */

/* ---- the key state machine (undriven readings, the row pulled up) ---- */
#define SC_OPEN_MIN 900u            /* [V] a reading at or above this is an open key (rest ~1023) */
#define SC_SEATED_MAX 420u          /* [V] below this the pill is seated (170 .. 200 hard, 270 .. 360 gentle) */
#define SC_REL_N 2u                 /* [V] open samples in a row for a release (a landed key can read open for one sample) */
#define SC_GAP_US 7000u             /* [D] no sample for this long: the gap guard (3 sample periods of 2.2 ms; a flash erase is ~40 ms) */
#define SC_WAIT 1u                  /* [G] driven readings after the landing before the velocity is taken (0: the landing depth alone) */
#define SC_WAIT_US 6000u            /* [D] at most this long after the landing: the first estimate (landing depth) is used */
#define SC_VEL_WIN_US 6000u         /* [D] a driven reading later than this after the landing no longer measures the landing */
#define SC_GATE_MAX_US 12000u       /* [D] a key the digital scan has closed waits at most this long for its velocity */
#define SC_VEL_UNKNOWN 100u         /* [D] velocity when nothing could be measured (a gap, a late reading): today's fixed one */
#define SC_TOP_VEL 100u             /* [D] the top 7 keys (notes 20 .. 26): fixed */

/* ---- readings that can mean something ---- */
#define SC_D_MIN 250u               /* [G] a driven reading below this is not a pill (rejected) */
#define SC_D_MAX 760u               /* [G] a driven reading above this is no landing (rejected); Velo: softest 515 .. 584 */

/* ---- the first estimate: the undriven landing depth (a hit reads ~185, a gentle press 270 .. 360) ---- */
#define SC_LAND_HARD 190u           /* [V] landing depth of a hard hit: velocity 127 */
#define SC_LAND_SOFT 360u           /* [V] landing depth of a gentle press: velocity 1 */

/* ---- velocity from the driven reading ---- */
#define SC_VEL_MIN 1u               /* [V] velocity = VEL_MIN + (VEL_MAX - VEL_MIN) x u^EXP, u = 1 at the floor (hardest) */
#define SC_VEL_MAX 127u
#define SC_EXP_Q8 256u              /* [G] the curve exponent x 256 (Velo: 0.7 = 179). 1.0 here: the floor and ceiling carry margins */
#define SC_CURVE_DEFAULT 100u       /* [D] MENU curve: 0 = flat 100, 100 = the measured range */

/* ---- per-key floor and ceiling (the hardest and the softest driven landing the key gives) ---- */
#define SC_FLOOR_PRIOR 440u         /* [V] before a key has been played: typical hardest reading (Velo: 389 .. 448; keys 389 .. 510) */
#define SC_CEIL_PRIOR 570u          /* [V] typical softest reading (Velo: 515 .. 584) */
#define SC_INIT_BELOW 45u           /* [G] first landing of a key (no stored value): the floor starts this far below it */
#define SC_INIT_ABOVE 85u           /* [G] .. and the ceiling this far above it */
#define SC_Q_LO 25u                 /* [D] the floor follows this per mille of the landings from below (a low quantile) */
#define SC_Q_HI 25u                 /* [D] .. the ceiling this per mille from above */
#define SC_ETA_FAST_Q6 384u         /* [G] quantile step while a key is new, counts x 64 (6 counts) */
#define SC_ETA_Q6 192u              /* [G] quantile step afterwards (3 counts); both ways, so a drift is followed */
#define SC_FAST_N 12u               /* [D] landings that count as new */
#define SC_MARGIN_PCT 18u           /* [G] floor / ceiling lie this much of the span outside the two quantiles: the extremes stay rare */
#define SC_SPAN_MIN 70u             /* [G] floor to ceiling, at least (Velo's minimum span 80 was its calibration's weak point) */
#define SC_SPAN_MAX 260u            /* [G] .. and at most */
#define SC_FLOOR_MIN 300u           /* [G] clamp of the floor (hardest) */
#define SC_FLOOR_MAX 560u
#define SC_CEIL_MIN 400u            /* [G] clamp of the ceiling (softest) */
#define SC_CEIL_MAX 720u
#define SC_CAL_SAVE_MOVED 20u       /* [D] the store is due when the calibration has moved this many counts in all (sum over keys) */
#define SC_CAL_SAVE_MS 300000u      /* [D] .. and at most once in this time, with no analog key held */

/* ---- chords ---- */
#define SC_CHORD_US 5000u           /* [V] notes landing within this of the first take its velocity (Velo's lower bound; to be tuned) */

/* ---- aftertouch (the held, driven reading) ---- */
#define SC_AT_SPAN 64u              /* [V] counts below the landing reading for 127 */
#define SC_AT_SMOOTH 2u             /* [D] the driven reading is smoothed 1 / 2^this per reading before the aftertouch is taken from it */
#define SC_AT_FLOOR 6u              /* [G] counts below the reference that are noise: no aftertouch for them (the rest spans SC_AT_SPAN) */
#define SC_AT_LEAK_N 32u            /* [D] the reference creeps down one count every this many driven readings */
#define SC_AT_DEAD 2u               /* [D] a change below this is not sent (channel pressure out) */

/* ---- the calibration walk (sense_walk_*): presses of each key, soft then hard ---- */
#define SC_WALK_SOFT 3u
#define SC_WALK_HARD 3u

/* ---- stored calibration (velo_cal_load / velo_cal_save of the patch store job) ---- */
#define SC_CAL_VERSION 1u
