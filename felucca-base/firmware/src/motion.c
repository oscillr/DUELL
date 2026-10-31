/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Sparse, bounded step automation. Main-loop writes publish under the same
 * interrupt guard as parameter batches. The ISR only scans MOTION_MAX fixed records (128 since 1.2, FUN10; 64 before).
 * p[] is the sounding value; motion_base_value() is the patch's saved value.
 * Two kinds of record share the MOTION_MAX (core.h MOTION_LOCK): an automation event sets its value at its step and the value
 * holds (to the next event, the loop's restart, a stop); a parameter lock (DUELL 1.0, change list B.8: a knob turned while
 * the part records, motion_knob_lock below) sets its value at its step only when the step plays (its chance passed) and the next step puts back
 * what would sound without it: the latest automation event of the pass up to that step, else the sound's own value
 * (motion_unlock). A lock lasts the whole step, its RATCH parts too; swing only moves the step's start. */
static motion_store_t motion;
/* DUELL 1.1: a target's SLOT: the P_ ids and the extension slots by their wide ids (0..EXT_P_LEN - 1, t->p[slot]), then the virtual targets (EXT_P_LEN + the destination number: lk_virt in mod.c, base 0) */
#define MOTION_SLOTS (EXT_P_LEN + 64u)
#define MOTION_WORDS ((MOTION_SLOTS + 31u) / 32u)
static uint32_t motion_slot(uint32_t tid) { return tid >= MOTION_VBASE ? EXT_P_LEN + (tid - MOTION_VBASE) : tid; }
static int16_t motion_base[NLIVE][MOTION_SLOTS];   /* (DUELL: the shown parts; the hidden ones have no motion: project.c drops it) */
static uint32_t motion_active[NLIVE][MOTION_WORDS];
static uint32_t motion_locked[NLIVE][MOTION_WORDS];   /* the slots a lock holds now (put back at the next step) */
/* the value of a slot of part t now / set it (a virtual one is the lock's amount in lk_virt, and mod_begin's work list is kept: lk_virt_n) */
static void motion_setp(track_t *t, uint32_t slot, int16_t v)
{
    uint32_t k, j;
    if (slot < EXT_P_LEN) {
        t->p[slot] = v;
        return;
    }
    k = trk_index(t);
    j = slot - EXT_P_LEN;
    if (k >= NLIVE || j >= 64u)
        return;
    if ((lk_virt[k][j] != 0) != (v != 0))
        lk_virt_n[k] = (uint8_t)(lk_virt_n[k] + (v ? 1 : -1));
    lk_virt[k][j] = (int8_t)v;
}
static uint8_t motion_base_valid, motion_full;
static uint32_t motion_guard(void)
{
#if defined(FM1_IRQ_TARGET)
    uint32_t f = fm1_icfg();
    fm1_irq_off();
    return f;
#else
    return 0;
#endif
}
static void motion_unguard(uint32_t f)
{
#if defined(FM1_IRQ_TARGET)
    fm1_icfg_set(f);
#else
    (void)f;
#endif
}
static int motion_param(uint32_t id)
{
    /* DUELL 1.1 (owner, HANDOFF_V2 item 34): what a lock or an automation event can record is exactly the mod matrix's destination list (mod.c mod_lock_id: one shared
     * table, the matrix's own order and names). A parameter that is not a matrix destination (LFO WAVE, PHASE and FADE, GLIDE, DETUNE, SPREAD, INSERT TYPE, KEY TRK, the
     * AMP ENV and FILTER ENV pitch and timbre depths, the LFO DEST depths, ...) is no longer recorded. Which of the destinations the unit offers right now is
     * mod_lock_ok (the matrix's mod_dst_offered). Before this round: a list of id ranges of its own */
    return id < 256u && mod_lock_dst(id) != 0u;
}
/* DUELL 1.1: records saved by an older firmware (a multi, a project, a backup) may hold events on targets this firmware no longer records (the matrix list is the only list now), a doubled event or
 * a value out of range: they are dropped, one by one, so the multi still loads with everything else (it was refused whole as DAMAGED in test3). The kept events keep their order */
static void motion_sanitize(motion_store_t *m)
{
    uint32_t i, j, n = 0;
    if (m->count > MOTION_MAX)
        m->count = MOTION_MAX;
    for (i = 0; i < m->count; i++) {
        const motion_event_t *e = &m->event[i];
        int dup = 0;
        if (!motion_param(MOTION_TID(e)) || e->value < -64 || e->value > 127)
            continue;
        for (j = 0; j < n; j++)
            dup |= m->event[j].place == e->place && (m->event[j].param & 0x7Fu) == (e->param & 0x7Fu);
        if (!dup)
            m->event[n++] = *e;
    }
    for (i = n; i < m->count; i++)
        memset(&m->event[i], 0, sizeof m->event[i]);
    m->count = (uint8_t)n;
}
static int motion_valid(const motion_store_t *m)
{
    uint32_t i, j;
    if (m->count > MOTION_MAX || (m->on & ~((1u << NTRK) - 1u))) return 0;
    for (i = 0; i < m->count; i++) {
        const motion_event_t *e = &m->event[i];
        if (!motion_param(MOTION_TID(e)) || e->value < -64 || e->value > 127) return 0;
        for (j = 0; j < i; j++)
            if (m->event[j].place == e->place && (m->event[j].param & 0x7Fu) == (e->param & 0x7Fu)) return 0;
    }
    return 1;
}
static int motion_enabled(const track_t *t) { return (motion.on >> trk_index(t)) & 1u; }
static uint32_t motion_count(const track_t *t)
{
    uint32_t i, n = 0, k = trk_index(t);
    for (i = 0; i < motion.count; i++) n += MOTION_TRK(&motion.event[i]) == k;
    return n;
}
/* track t's parameter locks (of motion_count) */
static uint32_t motion_lock_count(const track_t *t)
{
    uint32_t i, n = 0, k = trk_index(t);
    for (i = 0; i < motion.count; i++) n += MOTION_TRK(&motion.event[i]) == k && (motion.event[i].param & MOTION_LOCK);
    return n;
}
/* the steps of track k that hold a lock, bit s = step s (the roll and the grid mark them) */
static uint64_t motion_lock_steps(uint32_t k)
{
    uint32_t i;
    uint64_t m = 0;
    for (i = 0; i < motion.count; i++)
        if (MOTION_TRK(&motion.event[i]) == k && (motion.event[i].param & MOTION_LOCK))
            m |= (uint64_t)1 << (motion.event[i].place & 63u);
    return m;
}
/* the lock of track t on step / id: 1 and its value in *v, 0 = none */
static int motion_lock_get(const track_t *t, uint32_t step, uint32_t id, int16_t *v)
{
    uint32_t i, place = MOTION_PLACE(trk_index(t), step, id);
    for (i = 0; i < motion.count; i++)
        if (motion.event[i].place == place && motion.event[i].param == MOTION_PARAM(id, MOTION_LOCK)) {
            *v = motion.event[i].value;
            return 1;
        }
    return 0;
}
/* the parameters track k's MOTION will change (PLAY ON, at least one event): a bit per P_* id in m[].
 * Main loop only (the ISR never writes the events); at most MOTION_MAX records, once per card redraw. */
static uint32_t motion_mask(uint32_t k, uint32_t m[(P_COUNT + 31u) / 32u])
{
    uint32_t i, any = 0;
    memset(m, 0, ((P_COUNT + 31u) / 32u) * sizeof m[0]);
    if (k >= NTRK || !((motion.on >> k) & 1u)) return 0;
    for (i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if (MOTION_TRK(e) != k || MOTION_TID(e) >= P_COUNT) continue;   /* (the cards of the P_ ids only) */
        m[MOTION_TID(e) / 32u] |= 1u << (MOTION_TID(e) % 32u);
        any = 1;
    }
    return any;
}
static int16_t motion_base_value(const track_t *t, uint32_t id)
{
    uint32_t k = trk_index(t), slot;
    if (id >= MOTION_VBASE) return 0;                   /* (a virtual target's own value is 0: no offset) */
    if (id >= EXT_P_LEN) return 0;
    if (k >= NLIVE) return t->p[id];
    slot = id;
    return (motion_active[k][slot / 32u] >> (slot % 32u)) & 1u ? motion_base[k][slot] : t->p[id];
}
static void motion_restore(track_t *t)
{
    uint32_t k = trk_index(t), id, f;
    if (k >= NLIVE) return;
    f = motion_guard();
    for (id = 0; id < MOTION_SLOTS; id++)
        if ((motion_active[k][id / 32u] >> (id % 32u)) & 1u) motion_setp(t, id, motion_base[k][id]);
    memset(motion_active[k], 0, sizeof motion_active[k]);
    memset(motion_locked[k], 0, sizeof motion_locked[k]);
    motion_unguard(f);
}
static void motion_rebase(track_t *t)
{
    uint32_t k = trk_index(t), f;
    if (k >= NLIVE) return;
    f = motion_guard();
    motion_restore(t);
    memcpy(motion_base[k], t->p, sizeof t->p);
    motion_base_valid = (uint8_t)((motion_base_valid & ~(1u << k)) | (song.playing ? 1u << k : 0u));
    motion_unguard(f);
}
static void motion_begin(void)
{
    uint32_t k;
    for (k = 0; k < NLIVE; k++) {
        motion_restore(&trk[k]);
        memcpy(motion_base[k], trk[k].p, sizeof trk[k].p);
    }
    motion_base_valid = (1u << NLIVE) - 1u;
}
static void motion_end(void)
{
    for (uint32_t k = 0; k < NLIVE; k++) motion_restore(&trk[k]);
    motion_base_valid = 0;
}
static void motion_set_enabled(track_t *t, uint32_t on)
{
    uint32_t f = motion_guard(), b = 1u << trk_index(t);
    motion.on = (uint8_t)(on ? motion.on | b : motion.on & ~b);
    if (!on) motion_restore(t);
    motion_unguard(f);
}
static void motion_clear(track_t *t)
{
    uint32_t f = motion_guard(), k = trk_index(t), i, n = 0;
    motion_restore(t);
    for (i = 0; i < motion.count; i++)
        if (MOTION_TRK(&motion.event[i]) != k) motion.event[n++] = motion.event[i];
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion.on &= (uint8_t)~(1u << k);
    motion_rebase(t);
    motion_full = 0;
    motion_unguard(f);
}
static void motion_reset(track_t *t) { motion_clear(t); }
/* a record of kind 0 (automation) or MOTION_LOCK on track t's step / id: a new one, or the one there (of either kind)
 * becomes this. 0 ok, 1 invalid, 2 the MOTION_MAX are used (motion_full: "PARAMETER LOCKS FULL") */
static int motion_put(track_t *t, uint32_t step, uint32_t id, int16_t value, uint32_t kind)
{
    uint32_t k = trk_index(t), i, f;
    const param_desc_t *d;
    if (k >= NTRK || step >= NSTEP || !motion_param(id)) return 1;
    d = lock_desc_of(eng_idx(t->eng_req), id);
    if (value < d->min || value > d->max || value < -64 || value > 127) return 1;
    value = (int16_t)param_fit(d, value);                    /* (a retired KIT: the kit it plays) */
    f = motion_guard();
    for (i = 0; i < motion.count; i++)
        if (motion.event[i].place == MOTION_PLACE(k, step, id) && (motion.event[i].param & 0x7Fu) == (id & 0x7Fu)) break;
    if (i == MOTION_MAX) { motion_full = 1; motion_unguard(f); return 2; }
    motion.event[i].place = MOTION_PLACE(k, step, id);
    motion.event[i].param = MOTION_PARAM(id, kind);
    motion.event[i].value = (int8_t)value;
    RING_PUBLISH();
    if (i == motion.count) motion.count++;
    motion.on |= (uint8_t)(1u << k);
    motion_unguard(f);
    return 0;
}
static int motion_set_event(track_t *t, uint32_t step, uint32_t id, int16_t value) { return motion_put(t, step, id, value, 0); }
static int motion_set_lock(track_t *t, uint32_t step, uint32_t id, int16_t value)
{
    return motion_put(t, step, id, value, MOTION_LOCK);
}
static void motion_delete_event(track_t *t, uint32_t step, uint32_t id)   /* (either kind) */
{
    uint32_t f = motion_guard(), place = MOTION_PLACE(trk_index(t), step, id), i, n = 0;
    motion_restore(t);
    for (i = 0; i < motion.count; i++)
        if (motion.event[i].place != place || (motion.event[i].param & 0x7Fu) != (id & 0x7Fu)) motion.event[n++] = motion.event[i];
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion_unguard(f);
}
/* the locks of track t's step (NSTEP and up: of every step); their automation events stay. A lock sounding now goes
 * back at the next step as always (motion_locked). The locks it removed */
static uint32_t motion_clear_locks(track_t *t, uint32_t step)
{
    uint32_t f = motion_guard(), k = trk_index(t), i, n = 0, gone;
    for (i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if (MOTION_TRK(e) == k && (e->param & MOTION_LOCK) && (step >= NSTEP || (e->place & 63u) == step)) continue;
        motion.event[n++] = *e;
    }
    gone = motion.count - n;
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion_unguard(f);
    return gone;
}
/* ---- DUELL 1.0 (owner, change list B.8): recording parameter locks with the knobs ----
 * While the selected part records (song.rec, its bit: the header's REC mark shows it), a panel knob that sets a lockable
 * parameter (motion_param and mod_lock_ok: the matrix destination list, the same rule as the PARAMETER LOCKS list) records a
 * PARAMETER LOCK of that parameter with the value the knob set: with the transport stopped on the step at the sequencer's
 * cursor, while playing on the step that is playing now. Turns inside one step change that step's lock (motion_put: one record
 * per step and parameter; the last value stays). The knob's value sounds at once, but the patch's own value is kept as the
 * base (motion_base, motion_active): the next start, a stop and a save put back or store the patch's own value, the lock
 * plays on its step. Record off: the knob only changes the sound, as before.
 * ui_input.c sets these around a panel knob's turn (knob_rec_begin / knob_rec_end); everything here runs in the main loop
 * (the knob handling), never in the audio interrupt; the shared records are written under motion_guard as before. */
static uint8_t motion_knob_on;                  /* a panel knob's turn is being applied now */
static uint8_t motion_knob_cursor = NSTEP;      /* the sequencer cursor's step (the lock's step while stopped); NSTEP = none */
static uint8_t motion_knob_hit;                 /* a lock was recorded during this turn (ui_input.c finishes its undo copy) */
static void (*motion_knob_undo)(track_t *t);    /* the UI's undo copy (ui.c motion_undo_take), taken before a lock is written */
/* a panel knob's turn begins: the selected part records and the transport is stopped: the patch's values are taken as the
 * base first (when not already), so the value from before this turn is kept as the patch's own */
static void motion_knob_begin(uint32_t cursor, void (*undo)(track_t *t))
{
    track_t *t = TSEL;
    uint32_t k = trk_index(t), f;
    motion_knob_on = 1;
    motion_knob_cursor = (uint8_t)(cursor < NSTEP ? cursor : NSTEP);
    motion_knob_undo = undo;
    motion_knob_hit = 0;
    if (k >= NLIVE || !((song.rec >> k) & 1u) || song.playing || ((motion_base_valid >> k) & 1u))
        return;
    f = motion_guard();
    memcpy(motion_base[k], t->p, sizeof t->p);
    motion_base_valid |= (uint8_t)(1u << k);
    motion_unguard(f);
}
/* the turn is over: 1 = it recorded a lock */
static int motion_knob_end(void)
{
    int hit = motion_knob_hit;
    motion_knob_on = 0;
    motion_knob_cursor = NSTEP;
    motion_knob_undo = 0;
    motion_knob_hit = 0;
    return hit;
}
/* the lock of part t's parameter id with value: stopped, on the cursor's step; playing, on the step playing now (before the
 * first step has started: step 1). 1 = recorded, 0 = not (the records are full), -1 = no step inside the pattern's length
 * (the turn is then a live edit, as with record off) */
static int motion_knob_lock(track_t *t, uint32_t id, int16_t value)
{
    uint32_t k = trk_index(t), len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), step, f;
    f = motion_guard();
    if (song.playing)
        step = t->seq_pos >= 0x7FFFFFFFu ? 0u : t->seq_idx % len;   /* (seq_start: step 1 fires on the first block) */
    else
        step = motion_knob_cursor;
    if (step < len && ((motion_base_valid >> k) & 1u))
        motion_active[k][id / 32u] |= 1u << (id % 32u);   /* the knob's value is not the patch's: put back at a stop / start */
    motion_unguard(f);
    if (step >= len)
        return -1;
    if (motion_knob_undo)
        motion_knob_undo(t);
    motion_knob_hit = 1;
    return motion_set_lock(t, step, id, value) == 0;
}
static int motion_capture(track_t *t, uint32_t id, int16_t value)
{
    uint32_t k = trk_index(t), idx, len, period, f;
    int r;
    if (!motion_param(id) || k >= NLIVE) return 0;
    if (motion_knob_on && t == TSEL && ((song.rec >> k) & 1u) && mod_lock_ok(t, id) &&
        (r = motion_knob_lock(t, id, value)) >= 0)
        return r;                                /* (a panel knob: a lock, never an automation event) */
    if (!rec_on(t) || t != TSEL || motion_knob_on) {   /* (a panel knob that records nothing: a live edit, as with record off) */
        /* A live edit becomes a new base, even when an earlier motion value is
         * currently sounding. Subsequent recorded events can still override it. */
        f = motion_guard();
        if ((motion_base_valid >> k) & 1u) motion_base[k][id] = value;
        motion_unguard(f);
        return 0;
    }
    len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    period = div_samples((uint32_t)t->p[P_SDIV]);
    f = motion_guard();
    idx = t->seq_idx % len;
    if (t->seq_pos < 0x7FFFFFFFu && t->seq_pos > step_samples(t, period, idx) / 2u) idx = (idx + 1u) % len;
    /* Mark the immediate knob value transient too, so a stop before its next
     * quantized step still restores the original patch. */
    if ((motion_base_valid >> k) & 1u) motion_active[k][id / 32u] |= 1u << (id % 32u);
    motion_unguard(f);
    return motion_set_event(t, idx, id, value);
}
/* step `step` starts: the locks of the step before go back to what sounds without them: the latest automation event
 * of track t's id at a step up to this one in this pass (motion_restore at step 0 starts each pass), else the sound's
 * own value */
static __attribute__((noinline)) void motion_unlock(track_t *t, uint32_t step, const motion_store_t *m)
{
    uint32_t k = trk_index(t), id, i;
    if (k >= NLIVE) return;
    for (id = 0; id < MOTION_SLOTS; id++) {
        int32_t best = -1;
        int16_t v;
        uint32_t tid = id >= EXT_P_LEN ? MOTION_VBASE + (id - EXT_P_LEN) : id;
        if (!((motion_locked[k][id / 32u] >> (id % 32u)) & 1u))
            continue;
        v = motion_base[k][id];
        for (i = 0; i < m->count; i++) {
            const motion_event_t *e = &m->event[i];
            int32_t s = (int32_t)(e->place & 63u);
            if (MOTION_TRK(e) != k || (e->param & MOTION_LOCK) || MOTION_TID(e) != tid || s > (int32_t)step || s <= best)
                continue;                               /* (automation only, no MOTION_LOCK) */
            best = s;
            v = (int16_t)param_fit(lock_desc_of(eng_idx(t->eng_req), tid), e->value);
        }
        motion_setp(t, id, v);
    }
    memset(motion_locked[k], 0, sizeof motion_locked[k]);
}
/* step `step` of track t starts (seq.c seq_tick); play: it plays (its chance passed): its locks apply */
static __attribute__((noinline)) void motion_step(track_t *t, uint32_t step, const motion_store_t *m, uint32_t play)
{
    uint32_t k = trk_index(t), i, w, any = 0;
    if (k >= NLIVE || !((m->on >> k) & 1u)) return;
    if (!((motion_base_valid >> k) & 1u)) {
        memcpy(motion_base[k], t->p, sizeof t->p);
        motion_base_valid |= (uint8_t)(1u << k);
    }
    for (w = 0; w < MOTION_WORDS; w++)
        any |= motion_locked[k][w];
    if (!step) motion_restore(t);
    else if (any) motion_unlock(t, step, m);
    for (i = 0; i < m->count; i++) {
        const motion_event_t *e = &m->event[i];
        uint32_t tid = MOTION_TID(e), id = motion_slot(tid);
        if (MOTION_TRK(e) != k || MOTION_STEP(e) != step || ((e->param & MOTION_LOCK) && !play)) continue;
        const param_desc_t *d = lock_desc_of(eng_idx(t->eng_req), tid);
        motion_setp(t, id, (int16_t)param_fit(d, e->value));
        motion_active[k][id / 32u] |= 1u << (id % 32u);
        if (e->param & MOTION_LOCK)
            motion_locked[k][id / 32u] |= 1u << (id % 32u);
    }
}
static void motion_snapshot_track(track_t *t, motion_store_t *out)
{
    uint32_t f = motion_guard(), k = trk_index(t), i;
    memset(out, 0, sizeof *out);
    out->on = motion.on & (uint8_t)(1u << k);
    for (i = 0; i < motion.count; i++)
        if (MOTION_TRK(&motion.event[i]) == k) out->event[out->count++] = motion.event[i];
    motion_unguard(f);
}
static int motion_replace_track(track_t *t, const motion_store_t *in)
{
    uint32_t k = trk_index(t), n = motion.count - motion_count(t), f;
    if (!motion_valid(in) || n + in->count > MOTION_MAX) return 2;
    for (uint32_t i = 0; i < in->count; i++) if (MOTION_TRK(&in->event[i]) != k) return 1;
    f = motion_guard();
    motion_clear(t);
    memcpy(motion.event + motion.count, in->event, in->count * sizeof in->event[0]);
    motion.count += in->count;
    motion.on |= in->on & (uint8_t)(1u << k);
    motion_unguard(f);
    return 0;
}

/* ---- 1.1.5: SEQ > PARAMETER LOCKS (ui_events.c ev_*): the records of a track as a list, edited one by one ---- */
/* the record of track t on step / id (either kind): its index, -1 none */
static int32_t motion_find(const track_t *t, uint32_t step, uint32_t id)
{
    uint32_t i, place = MOTION_PLACE(trk_index(t), step, id);
    for (i = 0; i < motion.count; i++)
        if (motion.event[i].place == place && (motion.event[i].param & 0x7Fu) == (id & 0x7Fu))
            return (int32_t)i;
    return -1;
}
/* the indices of track k's records in list order (by step, then by id): n of them into idx[MOTION_MAX] */
static uint32_t motion_rows(uint32_t k, uint8_t *idx)
{
    uint32_t i, j, n = 0;
    for (i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        uint32_t key = (uint32_t)(e->place & 63u) << 8 | MOTION_TID(e);
        if (MOTION_TRK(e) != k)
            continue;
        for (j = n; j > 0u; j--) {                       /* (insertion: at most MOTION_MAX) */
            const motion_event_t *o = &motion.event[idx[j - 1u]];
            if (((uint32_t)(o->place & 63u) << 8 | MOTION_TID(o)) <= key)
                break;
            idx[j] = idx[j - 1u];
        }
        idx[j] = (uint8_t)i;
        n++;
    }
    return n;
}
/* record i of track t moves to step / id with value v (its kind kept): 0 ok, 1 invalid or another record is there.
 * What it set sounds until the next step (motion_restore puts back the sound's values, as a delete does) */
static int motion_move(track_t *t, uint32_t i, uint32_t step, uint32_t id, int16_t v)
{
    const param_desc_t *d;
    int32_t at;
    uint32_t f;
    if (i >= motion.count || MOTION_TRK(&motion.event[i]) != trk_index(t) || step >= NSTEP || !motion_param(id))
        return 1;
    at = motion_find(t, step, id);
    if (at >= 0 && (uint32_t)at != i)
        return 1;
    d = lock_desc_of(eng_idx(t->eng_req), id);
    if (v < d->min || v > d->max || v < -64 || v > 127)
        return 1;
    f = motion_guard();
    if (motion.event[i].place != MOTION_PLACE(trk_index(t), step, id) || MOTION_TID(&motion.event[i]) != id)
        motion_restore(t);                              /* (a value alone: what sounds stays) */
    motion.event[i].place = MOTION_PLACE(trk_index(t), step, id);
    motion.event[i].param = MOTION_PARAM(id, motion.event[i].param & MOTION_LOCK);
    motion.event[i].value = (int8_t)param_fit(d, v);
    motion_unguard(f);
    return 0;
}
/* an id whose meaning is the engine's own: EDIT's E1..E8 (DUELL: Felucca's DIGITAL operator and DRUM lane ids are common
 * parameters of DUELL's, core.h PL_*) */
static int motion_engine_id(uint32_t id)
{
    return id >= P_E0 && id <= P_E7;
}
/* a sound load put engine `to` where `from` was: when they differ, track t's records on the engine's own ids go (their
 * meaning changed); the ones on the common parameters (LEVEL, ENV, LFO, FILTER ENV, the sends, GLIDE, PAN, DETUNE)
 * stay, both kinds. The records it removed */
static uint32_t motion_sound_loaded(track_t *t, uint32_t from, uint32_t to)
{
    uint32_t f, k = trk_index(t), i, n = 0, gone;
    if (eng_idx(from) == eng_idx(to))
        return 0;
    f = motion_guard();
    for (i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if (MOTION_TRK(e) == k && motion_engine_id(MOTION_TID(e)))
            continue;
        motion.event[n++] = *e;
    }
    gone = motion.count - n;
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    motion_unguard(f);
    return gone;
}
