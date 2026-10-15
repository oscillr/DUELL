/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca 1.2: the SEQ TOOLS layer's actions (ui_layer.c: SEQ held, on every page). Included by ui_layer.c. The white
 * keys from F3 (TL_*):
 *   F3 CLEAR  G3 REVERSE  A3 SHIFT <  B3 SHIFT >       the whole sequence
 *   C4 RANDOM D4 COOK                                    new / a little changed
 * (DUELL 0.2.14: the DRUM engine is gone, and with it E4 BEAT and the four lane tools G4..C5)
 * Each works on the selected track's steps 1..LEN (the steps past LEN stay) and is one undo: SAVE held swaps back
 * the copy taken just before it (ui.c undo, UNDO_PAT; unlike a load the track's automation stays), so after a few
 * COOKs SAVE held takes back the last one only; the layer's OCT- puts back the track as the layer opened (tl_open).
 * A step that held something and is empty after loses its locks (as EDIT's step clear); REVERSE and SHIFT move the
 * steps' automation and locks with them. RANDOM and COOK draw from libc.c rng(): no seed is kept, each press
 * differs. A sequence of ties reversed keeps its note first: N T T -> N T T at the mirrored place */
enum { TL_CLEAR, TL_REV, TL_LEFT, TL_RIGHT, TL_RANDOM, TL_COOK, TL_N };

/* the track as the layer opened (OCT-): in the pool (RAM's .bss is tight), taken by layer_opened */
static struct { step_t step[NSTEP]; motion_store_t mo; } tl_open __attribute__((section(".pool")));

static uint32_t tl_len(const track_t *t) { return (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); }
static uint32_t tl_rnd(uint32_t n) { return n ? rng() % n : 0u; }
static int tl_gate(const step_t *s) { return s->time == ST_NOTE && s->n; }
/* place p's action is there */
static int tl_cell(uint32_t p) { return p < TL_N; }

/* the undo copy of t as it is now (ui.c load_begin, but the automation is copied and stays): one action */
static void tl_undo_take(track_t *t)
{
    uint32_t i = trk_index(t);
    undo.trk = (uint8_t)(i + 1u);
    undo.what = UNDO_PAT;
    undo.keep = 0;                                      /* (the next load or action takes its own copy) */
    undo.eng = t->eng_req;
    undo.preset = t->preset;
    undo.user = t->user;
    memcpy(undo.p, t->p, sizeof undo.p);
    memcpy(undo.step, t->step, sizeof undo.step);
    memcpy(undo.fm6, fm6_patch[i], FP_SIZE);
    undo.fm6_slot = fm6_slot[i];
    undo.pat = pat_sig[i];
    undo.from = pat_from[i];
    undo.from_h = pat_from_h[i];
    undo.patn = pat_last[i];
    motion_snapshot_track(t, &undo.motion_backup);
}

/* steps 1..len of t that are not empty (a REST); after the action the ones emptied lose their locks */
static uint64_t tl_used(const track_t *t, uint32_t len)
{
    uint64_t m = 0;
    uint32_t i;
    for (i = 0; i < len; i++)
        if (t->step[i].time != ST_REST)
            m |= (uint64_t)1 << i;
    return m;
}
static void tl_unlock(track_t *t, uint64_t was, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++)
        if (((was >> i) & 1u) && t->step[i].time == ST_REST)
            (void)motion_clear_locks(t, i);
}

/* step j of t becomes the undo copy's step src[j] (taken just before), its automation and locks with it */
static void tl_order(track_t *t, uint32_t len, const uint8_t *src)
{
    uint8_t to[NSTEP];
    uint32_t j, k = trk_index(t), f;
    for (j = 0; j < len; j++) {
        t->step[j] = undo.step[src[j]];
        to[src[j]] = (uint8_t)j;
    }
    f = motion_guard();
    for (j = 0; j < motion.count; j++) {
        motion_event_t *e = &motion.event[j];
        if ((e->place >> 6) == k && (e->place & 63u) < len)
            e->place = (uint8_t)(k << 6 | to[e->place & 63u]);
    }
    motion_unguard(f);
}

/* REVERSE: the mirror order; a note's ties (after it) would come before it, so the note goes first again */
static void tl_reverse(track_t *t, uint32_t len)
{
    uint8_t src[NSTEP], h;
    uint32_t j, e, x;
    for (j = 0; j < len; j++)
        src[j] = (uint8_t)(len - 1u - j);
    for (j = 0; j < len; j++) {
        for (e = j; e < len && undo.step[src[e]].time == ST_TIE; e++)
            ;
        if (e == j)
            continue;
        if (e < len && undo.step[src[e]].time == ST_NOTE) {   /* ties j .. e - 1, their note at e: the note to j */
            for (h = src[e], x = e; x > j; x--)
                src[x] = src[x - 1u];
            src[j] = h;
        }
        j = e;                                          /* (past the run and its note) */
    }
    tl_order(t, len, src);
}

/* SHIFT: rotated by one step inside LEN, d = 1 later (>), len - 1 earlier (<) */
static void tl_shift(track_t *t, uint32_t len, uint32_t d)
{
    uint8_t src[NSTEP];
    uint32_t j;
    for (j = 0; j < len; j++)
        src[j] = (uint8_t)((j + len - d) % len);
    tl_order(t, len, src);
}

/* ----------------------------------------------------------- melodic --- */
static int32_t tl_in_scale(const track_t *t, int32_t n) { return (int32_t)((scale_mask(t) >> (uint32_t)((n - root_of(t) + 120) % 12)) & 1u); }
/* the next note of the scale above (d 1) or below (d -1) n */
static int32_t tl_neigh(const track_t *t, int32_t n, int32_t d)
{
    uint32_t g = 12;
    do
        n += d;
    while (g-- && !tl_in_scale(t, n));
    return clamp(n, 0, 127);
}
/* RANDOM on a melodic track: the rhythm stays (the NOTE steps, their ties, accents and chance), the notes are new: a
 * walk through the track's scale (ROOT, SCL) 9 semitones either side of the pattern's middle (an empty pattern:
 * around the root near C4, on an 8th-note rhythm with some rests). A chord keeps its shape, each note snapped
 * down into the scale */
static void tl_random_notes(track_t *t, uint32_t len)
{
    uint8_t deg[20];
    int32_t c = 0, lo, d, x;
    uint32_t i, k, g = 0, nd = 0, at;
    for (i = 0; i < len; i++)
        if (tl_gate(&t->step[i])) {
            c += t->step[i].note[0];
            g++;
        }
    if (g) {
        c /= (int32_t)g;
    } else {                                            /* no rhythm: 8ths, a few rests, always step 1 */
        c = 60 + root_of(t) % 12;
        c -= c > 66 ? 12 : 0;
        for (i = 0; i < len; i++) {
            step_clear(&t->step[i]);
            if (i % 2u == 0u && (!i || tl_rnd(4))) {
                t->step[i].time = ST_NOTE;
                t->step[i].n = 1;
                t->step[i].vel = 96;
            }
        }
    }
    for (lo = clamp(c - 9, 12, 100), x = lo; x <= lo + 18 && nd < sizeof deg; x++)
        if (tl_in_scale(t, x))
            deg[nd++] = (uint8_t)x;
    if (!nd)
        return;
    at = nd / 2u + tl_rnd(5);
    at = at >= 2u ? at - 2u : 0u;
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        if (!tl_gate(s))
            continue;
        at = at >= nd ? nd - 1u : at;
        d = (int32_t)deg[at] - s->note[0];
        s->note[0] = deg[at];
        for (k = 1; k < s->n && k < 4u; k++) {
            x = clamp(s->note[k] + d, 1, 127);
            while (!tl_in_scale(t, x) && x > 1)
                x--;
            s->note[k] = (uint8_t)x;
        }
        if (!tl_rnd(6)) {                               /* a leap now and then, else a step or two */
            at = tl_rnd(nd);
        } else {
            x = (int32_t)at + (int32_t)tl_rnd(5) - 2;
            at = (uint32_t)(x < 0 ? -x : x >= (int32_t)nd ? 2 * ((int32_t)nd - 1) - x : x);
        }
    }
}

/* COOK on a melodic track: 15..25 % of the NOTE steps (at least one; more when they undid each other): a note to the
 * next scale note up or down (mostly) or an octave, an accent on / off, a chance of 50 / 75 % or back to 100 */
static void tl_cook_notes(track_t *t, uint32_t len, uint32_t g)
{
    uint32_t m = g * (15u + tl_rnd(11)) / 100u, i, k, r, n, sig = steps_sig(t), tries = 64;
    for (m = m ? m : 1u; tries-- && (m || steps_sig(t) == sig); m -= m != 0u) {   /* (undone again: one more) */
        step_t *s;
        n = tl_rnd(g);
        for (i = 0; i < len && (!tl_gate(&t->step[i]) || n--); i++)
            ;
        s = &t->step[i % NSTEP];
        r = tl_rnd(8);
        if (r == 6u) {
            s->flags ^= SF_ACCENT;
        } else if (r == 7u) {
            step_set_chance(s, step_chance(s) < 100u ? 100u : tl_rnd(2) ? 75u : 50u);
        } else {
            int32_t d = tl_rnd(2) ? 1 : -1, o = r == 5u ? d * 12 : 0;
            for (k = 0; k < s->n && k < 4u; k++)       /* (an octave out of 24 .. 108: a scale step instead) */
                if (s->note[k] + o < 24 || s->note[k] + o > 108)
                    o = 0;
            for (k = 0; k < s->n && k < 4u; k++)
                s->note[k] = (uint8_t)(o ? s->note[k] + o : tl_neigh(t, s->note[k], d));
        }
    }
}

/* ------------------------------------------------------------ actions --- */
/* the action of place p on track t: its message; 0 = nothing done (said why) */
static const char *tl_do(track_t *t, uint32_t p)
{
    uint32_t len = tl_len(t), i, n = 0;
    uint64_t was = tl_used(t, len);
    const char *msg = 0;
    if (p == TL_COOK) {                                 /* (nothing there: nothing to cook, no undo copy) */
        for (i = 0; i < len; i++)
            n += (uint32_t)tl_gate(&t->step[i]);
        if (!n) {
            ui_message("NOTHING TO COOK");
            return 0;
        }
    }
    tl_undo_take(t);
    switch (p) {
    case TL_CLEAR:
        for (i = 0; i < len; i++)
            step_clear(&t->step[i]);
        msg = "SEQUENCE CLEARED";
        break;
    case TL_REV: tl_reverse(t, len); msg = "REVERSED"; break;
    case TL_LEFT: tl_shift(t, len, len - 1u); msg = "SHIFTED LEFT"; break;
    case TL_RIGHT: tl_shift(t, len, 1u); msg = "SHIFTED RIGHT"; break;
    case TL_RANDOM:
        tl_random_notes(t, len);
        msg = "RANDOM NOTES";
        break;
    case TL_COOK:
        tl_cook_notes(t, len, n);
        msg = "COOKED";
        break;
    default:
        break;
    }
    tl_unlock(t, was, len);
    return msg;
}
