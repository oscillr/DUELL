/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro)
 * (from genkigenki/sloop-fm1 smk37; OberVave: +-2 semitones, its range the program's BEND, obv_smk.h) */
/* The SMK-37 Pro's pitch wheel as a bend (OberVave: obv_smk.h smk_wheels, into the engine's ctl_bend):
 * raw ADC reading -> 1/4096 semitone, +-SMK_PW_ST semitones. Self-calibrating: the rest (centre) is the first
 * reading, the range grows to what the wheel has reached, a dead band of SMK_PW_DEAD around the centre.
 * While the wheel rests in the dead band the centre follows a slow drift, at most one step per SMK_PW_DRIFT_MS
 * (in time, not in calls: the main loop calls this thousands of times a second, and a centre that moves that
 * fast is dragged along by a slow touch of the wheel; the spring then brings the wheel back to its real rest,
 * outside the dragged dead band, and a bend stays on whatever track is selected). No hardware here: the host
 * tests (tests/smk_wheel_test.c) run it. */
#pragma once
#include <stdint.h>

#define SMK_PW_ST 2               /* +- semitones at the end of the travel (OberVave: x the program's BEND / 2) */
#define SMK_PW_DEAD 12            /* raw counts around the centre: no bend */
#define SMK_PW_SPAN0 80           /* the range assumed until the wheel goes further */
#define SMK_PW_DRIFT_MS 250u      /* the centre follows a resting wheel by one count per this */

typedef struct {
    int32_t pc, plo, phi;         /* centre, lowest and highest reading seen (pc 0: not calibrated yet) */
    uint32_t drift_ms;            /* when the centre last moved (or the wheel left the dead band) */
} smk_pw_t;

static int32_t smk_pw_bend(smk_pw_t *w, int32_t p, uint32_t now_ms)
{
    int32_t d, b, up, dn;
    if (!w->pc) {
        w->pc = p ? p : 1;
        w->plo = p - SMK_PW_SPAN0;
        w->phi = p + SMK_PW_SPAN0;
        w->drift_ms = now_ms;
    }
    if (p < w->plo)
        w->plo = p;
    if (p > w->phi)
        w->phi = p;
    d = p - w->pc;
    if (d > -SMK_PW_DEAD && d < SMK_PW_DEAD) {
        if (now_ms - w->drift_ms >= SMK_PW_DRIFT_MS) {   /* resting: the centre follows a slow drift */
            w->pc += d > 0 ? 1 : d < 0 ? -1 : 0;
            w->drift_ms = now_ms;
        }
        return 0;
    }
    w->drift_ms = now_ms;                                /* (bent: the drift waits until it rests again) */
    up = w->phi - w->pc - SMK_PW_DEAD;
    dn = w->pc - w->plo - SMK_PW_DEAD;
    b = d > 0 ? (d - SMK_PW_DEAD) * (SMK_PW_ST * 4096) / (up > 1 ? up : 1)
              : (d + SMK_PW_DEAD) * (SMK_PW_ST * 4096) / (dn > 1 ? dn : 1);
    return b < -SMK_PW_ST * 4096 ? -SMK_PW_ST * 4096 : b > SMK_PW_ST * 4096 ? SMK_PW_ST * 4096 : b;
}
