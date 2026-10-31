/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
 * KEY SENSE: every tunable of sense.c in one table (key velocity and aftertouch from the FM-1's rubber-dome keys).
 * After Velo's method (Charles Vestal, https://github.com/charlesvestal/fm1-velocity, GPL-3.0-only): a held key's
 * carbon pill is read with its row driven; its resistance falls as the key is pressed harder. Written here from the
 * method and its measurements, no code taken.
 *
 * The first recording from the unit will change THIS FILE only (data, not code). The values below were Velo's
 * numbers from one author's unit (docs/design/VELO_STUDY.md section 1.1) and educated guesses; the readings and the per-key ranges are now
 * the OWNER'S OWN unit (item 123, velo_captures/velo_20261026_091617.csv, 105 presses on 10 keys, guided hard / normal / soft x 5): each line says which.
 *   [V] a Velo measurement   [G] a guess for the recording to replace   [D] a DUELL design choice   [R] fitted to the owner's recording (tests/data/velo_owner_20261026.csv)
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
#define SC_D_MIN 250u               /* [G] a driven reading below this is the hardest press (clamped to it; the recording's lowest driven reading with a key down is 671, none below) */
#define SC_D_MAX 1022u              /* [R] a driven reading above this is no landing (rejected). The owner's unit reads its first driven reading 676 .. 1019 (hard 679 .. 919, normal 770 .. 1009,
                                     * soft 937 .. 1019); 1023 is the open key (the saturated converter: 50 of the 51 driven readings of 1020 or more are exactly 1023, all of them after a release).
                                     * It was 760 [G] from Velo's unit (softest 515 .. 584): every normal and soft press of this unit was thrown away and read from the landing depth instead */

/* ---- the first estimate: the undriven landing depth (a hit reads ~185, a gentle press 270 .. 360) ---- */
#define SC_LAND_HARD 190u           /* [V] landing depth of a hard hit: velocity 127 */
#define SC_LAND_SOFT 360u           /* [V] landing depth of a gentle press: velocity 1 */

/* ---- velocity from the driven reading ---- */
#define SC_VEL_MIN 1u               /* [V] velocity = VEL_MIN + (VEL_MAX - VEL_MIN) x u^EXP, u = 1 at the floor (hardest) */
#define SC_VEL_MAX 127u
/* The curve TYPES (MENU > CONTROL > KEY VELOCITY and AFTERTOUCH, DUELL 1.1; the owner can change the numbers here, nothing else). ONE table for both lines, in the order
 * the menu shows them: SOFTER, SOFT, NORMAL, HARD, HARDER. Each is the exponent x 256 of  out = in ^ exponent  (in 0 .. 1: 1 = the hardest press for the velocity, the
 * strongest squeeze for the aftertouch). Below 1.0 a light touch already gives a high value (softer); above 1.0 it takes a firmer press (harder). Every one rises
 * with the pressure and never falls. NORMAL (256 = 1.00) is today's curve exactly (it is the identity, no rounding). 0.70 is Velo's value [V], 1.00 the former guess
 * [G], the rest geometric steps [D]. The names are the menu's words (PV_LEN 32: spelled out, no abbreviations). */
#define SC_TYPE_N 5u
#define SC_TYPE_NORMAL 2u           /* the index of NORMAL in the table (the default of a setting that was ON before DUELL 1.1) */
#define SC_TYPE_EXP_Q8 {128u, 179u, 256u, 358u, 512u}   /* SOFTER 0.50, SOFT 0.70, NORMAL 1.00, HARD 1.40, HARDER 2.00 */
#define SC_CURVE_DEFAULT 100u       /* [D] MENU curve: 0 = flat 100, 100 = the measured range */

/* ---- per-key floor and ceiling (the hardest and the softest driven landing the key gives) ---- */
#define SC_FLOOR_PRIOR 700u         /* [R] before a key has been played: the typical hardest reading. Lower quartile of the hard presses is 695 (all 33) / 694 (the 24 clean ones, median 702; without F3 690), put 6 counts
                                     * higher on purpose: the recording is the first from a hand with big fingers on small keys (9 of the 33 hard presses are suspect mis-hits), so a weak 'hard' press (the hard cluster has a
                                     * tail to 919) must still map high. The floor then moves down by the learning (3 counts a press) to the hardest presses he really plays. Velo's unit: 440 */
#define SC_CEIL_PRIOR 1000u         /* [R] typical softest reading; the 31 soft presses read 937 .. 1019 (median 1004, upper quartile 1009). 1000 and not 1009: on the pill's log scale the last
                                     * 10 counts are a third of the range, and with 1009 the soft presses of E4 (median 972) play 54 */
#define SC_INIT_BELOW 45u           /* [G] first landing of a key (no stored value): the floor starts this far below it */
#define SC_INIT_ABOVE 85u           /* [G] .. and the ceiling this far above it */
#define SC_Q_LO 25u                 /* [D] the floor follows this per mille of the landings from below (a low quantile) */
#define SC_Q_HI 25u                 /* [D] .. the ceiling this per mille from above */
#define SC_ETA_FAST_Q6 384u         /* [G] quantile step while a key is new, counts x 64 (6 counts) */
#define SC_ETA_Q6 192u              /* [G] quantile step afterwards (3 counts); both ways, so a drift is followed */
#define SC_FAST_N 12u               /* [D] landings that count as new */
#define SC_MARGIN_PCT 0u            /* [R] floor / ceiling lie this much of the span outside the two quantiles (was 18 on Velo's span of 130; on this unit's span of 305 it put the ceiling past the open key) */
#define SC_SPAN_MIN 150u            /* [R] floor to ceiling, at least (the windows below leave 100) */
#define SC_SPAN_MAX 400u            /* [R] .. and at most (this unit's typical span is 305; it was 260) */
#define SC_FLOOR_MIN 560u           /* [R] window of the floor (hardest): the lowest driven reading of the recording is 671, Velo's unit 389 .. 510 */
#define SC_FLOOR_MAX 800u           /* [R] (F3's hard presses read 770 .. 919, the other keys' 679 .. 803) */
#define SC_CEIL_MIN 900u            /* [R] window of the ceiling (softest) */
#define SC_CEIL_MAX 1020u           /* [R] (the softest first reading of the recording is 1019) */
#define SC_CAL_SAVE_MOVED 20u       /* [D] the store is due when the calibration has moved this many counts in all (sum over keys) */
#define SC_CAL_SAVE_MS 60000u       /* [R] .. and at most once in this time, with no analog key held (was 300 s: a play session shorter than 5 minutes was never stored, item 102 (C)). Flash: the store is an
                                     * A/B pair of 4 KB sectors, one erase per save, alternating; 100,000 erases per sector is the usual figure for this class of NOR, the part's datasheet was not checked = 200,000 saves; a save needs 20 counts of movement (about 70 presses in steady play), so
                                     * even 20 saves a day are 27 years, and 60 s caps the worst case at 1,440 a day (140 days of non-stop playing) */

/* ---- chords ---- */
#define SC_CHORD_US 5000u           /* [V] notes landing within this of the first take its velocity (Velo's lower bound; to be tuned) */

/* ---- aftertouch (the held, driven reading) ---- */
#define SC_AT_SPAN 140u             /* [V] counts below the SETTLED reading for 127 (item 126: was 64, Velo's unit; the owner's unit: median settled reading of a soft hold 830, of a hard hold 689 = 141 counts; NO squeeze in the recording, to be confirmed by one) */
#define SC_AT_MIN_US 40000u         /* [V] no aftertouch before this long after the landing (the shortest settle in the recording: 2 ms hard, 20 ms normal, 33 ms soft) */
#define SC_AT_STILL_US 30000u       /* [V] the reading must stay within SC_AT_STILL_BAND this long to count as settled (about 7 driven readings) */
#define SC_AT_STILL_BAND 6u         /* [G] counts: the same as SC_AT_FLOOR */
#define SC_AT_MAX_US 300000u        /* [V] settled at the latest this long after the landing, whatever the reading does (soft presses of the recording: the slowest settles in 264 ms) */
#define SC_AT_SMOOTH 2u             /* [D] the driven reading is smoothed 1 / 2^this per reading before the aftertouch is taken from it */
#define SC_AT_FLOOR 6u              /* [G] counts below the reference that are noise: no aftertouch for them (the rest spans SC_AT_SPAN) */
/* (DUELL 1.1: SC_AT_LEAK_N is gone. The reference crept down one count every 32 readings, so a held squeeze faded to 0 in 9 .. 14 s. The reference now stays at
 * the highest reading since the landing; the release to 0 is the key lift (sk_lift) and a driven reading above SC_D_MAX while held (sk_driven).) */
#define SC_AT_DEAD 2u               /* [D] a change below this is not sent (channel pressure out) */

/* ---- the calibration walk (sense_walk_*): presses of each key, soft then hard ---- */
#define SC_WALK_SOFT 3u
#define SC_WALK_HARD 3u

/* ---- stored calibration (velo_cal_load / velo_cal_save of the patch store job) ---- */
#define SC_CAL_VERSION 2u           /* [R] 2 (item 123): a stored version-1 record (the old ranges of Velo's unit: the owner's unit held 560 / 720) is not accepted: every key starts on the new defaults once */
