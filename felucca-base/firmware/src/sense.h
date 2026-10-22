/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* KEY SENSE (DUELL_VELO builds): key velocity and aftertouch from the FM-1's 20 lower keys. After Velo's method
 * (Charles Vestal, github.com/charlesvestal/fm1-velocity, GPL-3.0-only); see sense.c.
 *
 * Plain C, no hardware calls, only <stdint.h>. Everything runs on core 0: sense_update from the scan tick (TIMER5,
 * interrupt), sense_gate / sense_vel / sense_at_* from the audio interrupt's event step, the settings API and
 * sense_service from the main loop. Nothing here writes flash from the tick: the calibration store is
 * velo_cal_save, called from sense_service (main loop) only.
 *
 * Data flow:  tick:   sense_want_drive(note)  -> drive the row for this reading?
 *                     sense_update(note, adc, driven, t_us)
 *             events: cur = sense_gate(cur, prev)   the key bits, a note whose velocity is not ready yet held back
 *                     sense_vel(note)                the velocity for a note that rose
 *                     sense_at_value()               the strongest held key's aftertouch (channel pressure)
 * Tunables: sense_cal.h. Notes 0 .. 19 are analog (F3 .. C5); notes 20 .. 26 are fixed at SC_TOP_VEL.
 * Threads: the tick and the event step are two interrupts of core 0 (the tick preempts the event step) and share no lock: every field has ONE
 * writer (sense.c: the tick's are in sk_t, the event step's in sn.asked / g_ask / g_seq / defer / vo). The event step reads a landing's velocity
 * only after the tick has counted it (sk_t.rseq, written last).
 * Settings KEY VELOCITY and AFTERTOUCH (DUELL 1.0: two lines, both OFF by default): both OFF, sense_gate returns its input, sense_vel returns 100, sense_want_drive returns 0,
 * sense_update ignores everything: the build plays as one without the feature. AFTERTOUCH alone: the sensing runs for the pressure; sense_gate and sense_vel stay as OFF
 * (the velocity is the fixed 100, no key waits). KEY VELOCITY alone: no pressure (sense_at_value 0). */
#pragma once
#include <stdint.h>
#include "sense_cal.h"

/* ---- the calibration store: provided by the patch store job (job A); the tests use a RAM stub ---- */
#define SENSE_CAL_BYTES (4u + 4u * SC_NKEY + 2u)    /* 'V' 'C' version nkey, (floor, ceiling) x 20, CRC-16 */
/* n bytes of the stored record into buf; the number of bytes read, or < 0 when nothing is stored */
int velo_cal_load(uint8_t *buf, uint32_t n);
/* n bytes to the store (a flash write: main loop only, never from the tick); 0 = stored */
int velo_cal_save(const uint8_t *buf, uint32_t n);

/* ---- the scan tick ---- */
uint32_t sense_want_drive(uint32_t note);                    /* 1: read this key's next sample with its row driven */
void sense_update(uint32_t note, uint32_t adc, uint32_t driven, uint32_t t_us);   /* one reading (0 .. 1023) */
void sense_missed(uint32_t note);                            /* a slot that could not be read (converter busy): no sample */

/* ---- the event step (keyboard_block) ---- */
uint32_t sense_gate(uint32_t cur, uint32_t prev);            /* the debounced key bits as the notes may start them now */
uint32_t sense_vel(uint32_t note);                           /* the velocity of the note that rose (100 when OFF) */
uint32_t sense_at_value(void);                               /* 0 .. 127: the strongest held analog key (0 when OFF) */

/* ---- settings (for the later menu page; no page here) ---- */
void sense_enable(uint32_t on);                              /* default OFF (owner, 2026-10-21; the state is all zero at power-on = OFF; the menu setting turns it ON) */
uint32_t sense_enabled(void);                                /* KEY VELOCITY is ON */
void sense_at_enable(uint32_t on);                           /* AFTERTOUCH (DUELL 1.0, its own setting; default OFF): the channel pressure the keys send; sense_at_value is 0 while it is OFF */
uint32_t sense_at_enabled(void);
uint32_t sense_running(void);                                /* either one is ON: the key sensing runs */
void sense_curve_set(uint32_t pct);                          /* 0 = flat 100 .. 100 = the measured range */
uint32_t sense_curve_get(void);
void sense_reset(void);                                      /* calibration back to the priors, all key states cleared */
void sense_walk_start(void);                                 /* the calibration walk: each key in turn, soft then hard presses */
int32_t sense_walk_step(uint32_t *soft, uint32_t *hard);     /* the key to light (0 .. 19) and its counts so far; -1 when done */
uint32_t sense_walk_done(void);
void sense_walk_cancel(void);
void sense_service(uint32_t now_ms, uint32_t idle_ok);       /* main loop: the due calibration store; idle_ok 0 while a key is held */
void sense_cal_restore(void);                                /* boot / after a store job: velo_cal_load into the keys */
void sense_cal_store(void);                                  /* velo_cal_save now (main loop) */
uint32_t sense_cal_export(uint8_t *buf);                     /* SENSE_CAL_BYTES into buf (tests, the store) */
uint32_t sense_cal_import(const uint8_t *buf);               /* 1 = accepted */
void sense_cal_get(uint32_t note, uint32_t *floor, uint32_t *ceil);   /* the key's floor and ceiling in counts (derived, with margins) */

/* ---- event ring and statistics (log, tests) ---- */
enum { SE_LAND = 1, SE_LIFT, SE_AT, SE_GAP, SE_UNKNOWN };
typedef struct {
    uint8_t type, key, a, b;    /* SE_LAND: a = velocity, b = 1 if from a driven reading; SE_AT: a = value */
    uint32_t t;                 /* us */
} sense_ev_t;
uint32_t sense_event_pop(sense_ev_t *e);                     /* 1 = an event was returned */
typedef struct {
    uint32_t lands, lands_driven, unknown, gaps, lifts, bounces, rejected;
} sense_stat_t;
void sense_stat_get(sense_stat_t *s);
uint32_t sense_state_bytes(void);                            /* the RAM the sense state uses */
