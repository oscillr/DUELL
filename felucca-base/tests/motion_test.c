/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Real sequencer/project/UI sources: motion lifecycle and compact migration. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int motion_recording(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    t->p[P_REV] = 23; song.sel = 0; song.rec = 1;
    seq_start(); seq_tick(t, CTL); /* first real step */
    t->p[P_REV] = 92;
    bad += check("motion records the selected armed track at its current step", !motion_capture(t, P_REV, 92) &&
        motion.count == 1u && motion.event[0].place == 0u && motion.event[0].value == 92);
    t->p[P_REV] = 110; motion_capture(t, P_REV, 110);
    bad += check("same step/parameter overwrites rather than consuming capacity", motion.count == 1u && motion.event[0].value == 110);
    bad += check("sounding values never replace the original patch base", t->p[P_REV] == 110 && motion_base_value(t, P_REV) == 23);
    project_t q; project_store_t packed;
    project_capture(&q);
    bad += check("project snapshot saves base + independent events while sounding", q.t[0].p[P_REV] == 23 && q.motion.event[0].value == 110 &&
        proj_pack(&packed, &q) && sizeof packed == 3840u);
    seq_stop();
    bad += check("stop before another step restores the original parameter", t->p[P_REV] == 23);
    seq_start(); seq_tick(t, CTL);
    bad += check("recorded motion plays back at step zero", t->p[P_REV] == 110);
    motion_set_enabled(t, 0);
    bad += check("bypass restores base without deleting automation", t->p[P_REV] == 23 && motion_count(t) == 1u && !motion_enabled(t));
    motion_set_enabled(t, 1); motion_step(t, 0, &motion, 1);
    song.rec = 0; t->p[P_REV] = 37; motion_capture(t, P_REV, 37); seq_stop();
    bad += check("manual edits outside recording become the new base", t->p[P_REV] == 37);
    song.rec = 3; seq_start(); seq_tick(t, CTL);
    trk[1].p[P_REV] = 45; motion_capture(&trk[1], P_REV, 45);
    bad += check("only the selected track captures knob motion", motion_count(&trk[1]) == 0u);
    bad += check("transport and engine-switch parameters cannot be captured", !motion_param(P_SLEN) && !motion_param(P_AMODE) &&
        motion_set_event(t, 0, P_SLEN, 8) == 1);
    seq_stop(); motion_clear(t);
    t->p[P_REV] = 42; song.rec = 1; seq_start(); seq_tick(t, CTL);
    motion_clear(t); t->p[P_REV] = 70; motion_capture(t, P_REV, 70); seq_stop();
    bad += check("clearing then recording mid-play keeps the correct new base", t->p[P_REV] == 42);
    return bad;
}
static int motion_capacity(void)
{
    int bad = 0; ui_power_on();
    for (uint32_t i = 0; i < MOTION_MAX; i++)          /* (128 since 1.2: two ids on each of the 64 steps) */
        bad += motion_set_event(&trk[0], i % NSTEP, i < NSTEP ? P_REV : P_DLY, (int16_t)(i % NSTEP));
    motion_store_t saved = motion;
    bad += check("full event pool refuses append without overwriting earlier events", motion_set_event(&trk[1], 0, P_REV, 90) == 2 &&
        !memcmp(&motion, &saved, sizeof saved));
    bad += check("full pool still permits a targeted overwrite", !motion_set_event(&trk[0], 12, P_REV, 100) && motion.count == MOTION_MAX);
    motion_delete_event(&trk[0], 12, P_REV);
    bad += check("deleting one event releases one slot", motion.count == MOTION_MAX - 1u && !motion_set_event(&trk[1], 0, P_REV, 90));
    saved = motion; motion_store_t invalid = motion; invalid.count = MOTION_MAX + 1;
    bad += check("invalid restore leaves the pool intact", motion_replace_track(&trk[0], &invalid) != 0 && !memcmp(&motion, &saved, sizeof saved));
    motion_clear(&trk[0]);
    bad += check("track clear keeps every other track's events", motion.count == 1u && motion.event[0].place >> 6 == 1u);
    return bad;
}
static int probability_playback(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    step_t s = {{60, 64, 0, 0}, 2, ST_NOTE, 0, 90, 1u << DV_KICK, 0, 0};
    bad += check("zero-initialized probability remains legacy 100 percent", step_chance(&s) == 100u);
    step_set_chance(&s, 0); seq_step(t, &s, 0, div_samples(2), 0);
    bad += check("zero percent suppresses the whole chord and drum hits", step_chance(&s) == 0u && !t->seq_n);
    step_set_chance(&s, 100); seq_step(t, &s, 0, div_samples(2), 0);
    bad += check("100 percent plays all chord notes and drum hits", t->seq_n == 3u);
    seq_release(t); step_set_chance(&s, 50); uint32_t heard = 0;
    for (uint32_t i = 0; i < 1000u; i++) { seq_step(t, &s, 0, div_samples(2), 0); heard += t->seq_n != 0; seq_release(t); }
    bad += check("chance is evaluated each repeat with one decision per step", heard > 350u && heard < 650u);
    return bad;
}
static int compact_project(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    t->step[3] = (step_t){{60, 67}, 2, ST_NOTE, SF_ACCENT, 110, 0x81, 0x80, 0};
    step_set_chance(&t->step[3], 25); t->p[P_M5AMT] = 40; t->p[P_ED_FLT] = -50;   /* (DUELL: a matrix slot 5 value on Felucca's OP ENV id) */
    motion_set_event(t, 3, P_REV, 110);
    project_t before, after; project_store_t packed, corrupt;
    project_capture(&before);
    bad += check("FUN11 fits the flash object", sizeof(project_store_t) == 3840u && proj_pack(&packed, &before));
    bad += check("FUN11 round trip preserves signed values/matrix 5..8/probability/motion (parts A B)",
        proj_import(&after, &packed, sizeof packed) && !memcmp(&before.t, &after.t, NSHOWN * sizeof before.t[0]) &&
        !memcmp(&before.motion, &after.motion, sizeof before.motion));
    corrupt = packed; corrupt.raw[112] ^= 1u;
    bad += check("FUN7 torn or corrupted payload is refused", !proj_import(&after, &corrupt, sizeof corrupt));
    corrupt = packed; corrupt.raw[68] = 255; uint32_t sum = proj_hash(corrupt.raw, sizeof corrupt - 4u);
    memcpy(corrupt.raw + sizeof corrupt - 4u, &sum, 4);
    bad += check("compact parameter range is validated even with a correct hash", !proj_import(&after, &corrupt, sizeof corrupt));
    corrupt = packed; uint32_t probability_byte = 68u + P_COUNT + 2u + 8u; corrupt.raw[probability_byte] = 127;
    sum = proj_hash(corrupt.raw, sizeof corrupt - 4u); memcpy(corrupt.raw + sizeof corrupt - 4u, &sum, 4);
    bad += check("invalid probability is refused even with a correct hash", !proj_import(&after, &corrupt, sizeof corrupt));
    project_v6_t old; memset(&old, 0, sizeof old); old.magic = PROJ_MAGIC_V6; old.size = sizeof old;
    memcpy(old.g, before.g, sizeof old.g); old.parts = NPART; old.phys = PROJ_PHYS;
    for (uint32_t k = 0; k < NTRK; k++) {
        for (uint32_t j = 0; j < 61u; j++) old.t[k].p[j] = before.t[k].p[j];
        for (uint32_t j = 0; j < 8u; j++) old.t[k].p[61u + j] = before.t[k].p[P_E0 + j];
        old.t[k].engine = before.t[k].engine; old.t[k].preset = before.t[k].preset;
        for (uint32_t j = 0; j < NSTEP; j++) memcpy(&old.t[k].step[j], &before.t[k].step[j], sizeof(step10_t));
    }
    old.chain.count = before.chain.count;              /* (the rows of FUN6: one slot each) */
    for (uint32_t r = 0; r < CHAIN_ROWS; r++) {
        old.chain.row[r].slot = before.chain.row[r].slot[0];
        old.chain.row[r].repeat = before.chain.row[r].repeat;
    }
    old.sum = proj_hash(&old, sizeof old - 4u);
    bad += check("real FUN6 disk image migrates with FM defaults/100% chance/no motion", proj_import(&after, &old, sizeof old) &&
        after.t[0].p[P_E0] == before.t[0].p[P_E0] && after.t[0].p[P_ED_FLT] == -50 &&
        after.t[0].p[P_M5AMT] == 0 && !after.motion.count && step_chance(&after.t[0].step[3]) == 100u);
    bad += check("migrated FUN6 can be written as fixed-size FUN7", proj_pack(&packed, &after));
    project_save(1); motion_clear(t); t->p[P_M5AMT] = 0; step_set_chance(&t->step[3], 100);
    project_load(1);
    bad += check("actual project save/load restores motion, chance and operator settings", motion_count(t) == 1u &&
        step_chance(&t->step[3]) == 25u && t->p[P_M5AMT] == 40);
    return bad;
}
/* a project of 89 parameters (before the chord keys P_CHRD / P_VOIC; today's FUN9 frame): the engine's values at
 * 81..88, motion ids from 81 on for E0..E7 (m: its events as that firmware numbered them) */
static void pack_fun7_89(project_store_t *out, const project_t *q, const motion_store_t *m)
{
    uint8_t *b = out->raw; uint32_t pos = 68u, t, i, magic = PROJ_MAGIC_V10, size = PROJ_STORE_SIZE, sum;   /* (Felucca's) */
    memset(out, 0, sizeof *out); memcpy(b, &magic, 4); memcpy(b + 4, &size, 4);
    memcpy(b + 8, q->g, sizeof q->g); b[62] = q->sel; b[63] = q->parts; b[64] = q->phys; b[66] = 89;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < 89u; i++) b[pos++] = (uint8_t)(q->t[t].p[i < 81u ? i : P_E0 + i - 81u] + 64);
        b[pos++] = q->t[t].engine; b[pos++] = q->t[t].preset;
        for (i = 0; i < NSTEP; i++) {
            const step_t *s = &q->t[t].step[i];
            memcpy(b + pos, s->note, 4); pos += 4;
            b[pos++] = (uint8_t)(s->n | s->time << 3 | s->flags << 5);
            b[pos++] = s->vel; b[pos++] = s->hit; b[pos++] = s->acc; b[pos++] = s->probability;
        }
    }
    memcpy(b + pos, &q->chain, sizeof q->chain); pos += sizeof q->chain;
    memcpy(b + pos, m, sizeof *m);
    sum = proj_hash(b, PROJ_STORE_SIZE - 4u); memcpy(b + PROJ_STORE_SIZE - 4u, &sum, 4);
}
static int fun7_89(void)
{
    int bad = 0, ok; ui_power_on(); track_t *t = &trk[0];
    project_t before, after; project_store_t old; motion_store_t m;
    uint32_t i, k;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < 8u; i++) trk[k].p[P_E0 + i] = (int16_t)(ENGINES[trk[k].eng_req]->edit[i].min + (int16_t)(k + i) %
            (ENGINES[trk[k].eng_req]->edit[i].max - ENGINES[trk[k].eng_req]->edit[i].min + 1));
    t->p[PL_FM1_ATK] = 33; t->p[P_REV] = 20;           /* (Felucca's OP1 ATK there: DUELL's default after the load) */
    t->step[2] = (step_t){{60, 64, 67}, 3, ST_NOTE, 0, 100};
    project_capture(&before);
    memset(&m, 0, sizeof m);                                       /* as the 89-parameter firmware numbered them */
    m.count = 3; m.on = 1;
    m.event[0] = (motion_event_t){3, P_REV, 90};
    m.event[1] = (motion_event_t){5, 81, 40};                      /* its P_E0 (81) */
    m.event[2] = (motion_event_t){6, 61, 20};                      /* FM OP1 ATK: 61 then and now */
    pack_fun7_89(&old, &before, &m);
    ok = proj_import(&after, &old, sizeof old);
    for (k = 0; ok && k < NSHOWN; k++) {
        for (i = 0; i < 8u; i++) ok &= after.t[k].p[P_E0 + i] == before.t[k].p[P_E0 + i];
        for (i = 0; i < 81u; i++) ok &= after.t[k].p[i] == (param_retired(i) ? TP[i].def : before.t[k].p[i]);
        ok &= after.t[k].p[P_CHRD] == 0 && after.t[k].p[P_VOIC] == 0;
        for (i = PL_LN0; i <= PL_LN7; i++) ok &= after.t[k].p[i] == TP[i].def;
    }
    bad += check("89 parameters: E0..E7 at P_E0.., the chord keys OFF / CLOSE, DUELL's ids defaults, the rest in place", ok &&
        after.t[0].p[PL_FM1_ATK] == TP[PL_FM1_ATK].def && !memcmp(after.t[0].step, before.t[0].step, sizeof before.t[0].step));
    bad += check("  its motion: E0 (81) -> P_E0, REV kept, FM OP1 ATK (61, now DUELL's) dropped",
        after.motion.count == 2u && after.motion.event[0].param == P_REV && after.motion.event[1].param == P_E0 &&
        after.motion.event[1].value == 40);
    bad += check("  written again (P_COUNT parameters): the same parts", proj_pack(&old, &after) && old.raw[66] == P_COUNT &&
        proj_import(&before, &old, sizeof old) && !memcmp(&before.t, &after.t, NSHOWN * sizeof before.t[0]));
    m.event[1].param = 82;                                          /* (any id P_E0 .. P_E7 of then moves up) */
    pack_fun7_89(&old, &before, &m);
    bad += check("  E1 (82) -> P_E1", proj_import(&after, &old, sizeof old) && after.motion.event[1].param == P_E1);
    return bad;
}
static int loads(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    int16_t v;
    uint32_t e0 = t->eng_req, e1 = e0 == ENGI_FM6 ? 0u : ENGI_FM6;
    t->p[P_REV] = 21; motion_set_event(t, 0, P_REV, 100); t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    motion_set_lock(t, 1, P_E0, motion_base_value(t, P_E0));
    apply_preset_to(t, 1);
    bad += check("1.1.5: a sound load of the same engine keeps the motion (both kinds, the engine's own E1 too), the pattern",
                 motion_count(t) == 2u && motion_lock_get(t, 1, P_E0, &v) && motion_enabled(t) && t->step[0].note[0] == 60);
    undo_swap();                                   /* (back to the sound before it: REV 21) */
    set_engine_of(t, e1);
    bad += check("  another engine: the motion on the common parameters (REV) stays, the engine's own (E1) goes, PLAY ON",
                 motion_count(t) == 1u && !motion_lock_get(t, 1, P_E0, &v) && motion_enabled(t) && t->step[0].note[0] == 60);
    undo_swap(); bad += check("sound undo restores the original motion pool and base", motion_count(t) == 2u && t->p[P_REV] == 21 &&
                              t->eng_req == e0);
    undo_swap(); bad += check("sound redo restores the loaded motion state", motion_count(t) == 1u && t->eng_req == e1);
    undo_swap();                                   /* (DUELL has no song mode: the SONG checks that were here went with it) */
    return bad;
}
static int repeat_mode(void)
{
    int bad = 0; ui_power_on(); track_t *t = &trk[0];
    t->p[P_AMODE] = 6; t->p[P_AOCT] = 4; t->p[P_APROB] = 127;
    arp_add(t, 72); arp_add(t, 60);
    arp_tick(t, CTL);
    bad += check("REPEAT retriggers the last played note without octave traversal", t->arp_note == 60);
    t->arp_pos = 0xFFFFFFF; arp_tick(t, CTL);
    bad += check("REPEAT remains on the same note on the next pulse", t->arp_note == 60);
    arp_remove(t, 60); arp_remove(t, 72); arp_tick(t, CTL);
    bad += check("REPEAT releases after the last held key is released", !t->nheld && !t->arp_note);
    t->p[P_AHOLD] = 1; arp_add(t, 65); arp_remove(t, 65); t->arp_pos = 0xFFFFFFF; arp_tick(t, CTL);
    bad += check("REPEAT supports ARP HOLD", t->nheld == 1u && t->arp_note == 65);
    return bad;
}

/* 1.2 (Discussion #95): the ARP modes appended after REPEAT (seq.c AM_*), each over a 3- and a 4-note chord, OCT,
 * ORD PLAY, HOLD, REPEAT switched in and out, WALK's random steps the same from the same seed */
static void arp_fresh(track_t *t, int32_t mode, int32_t oct, const uint8_t *notes, uint32_t n)
{
    uint32_t i;
    if (t->arp_note)
        arp_off(t);
    t->nheld = 0; t->arp_phys = 0;
    t->p[P_AMODE] = (int16_t)mode; t->p[P_AOCT] = (int16_t)oct; t->p[P_APROB] = 127;
    for (i = 0; i < n; i++)
        arp_add(t, notes[i]);
}
static int arp_plays(track_t *t, const uint8_t *want, uint32_t n)   /* the next n steps play these notes */
{
    uint32_t i;
    int ok = 1;
    for (i = 0; i < n; i++) {
        t->arp_pos = 0xFFFFFFF;
        arp_tick(t, CTL);
        ok &= t->arp_note == want[i] && !t->arp_ch[0];
        if (t->arp_note != want[i])
            printf("    step %u: %u, not %u\n", (unsigned)i, (unsigned)t->arp_note, (unsigned)want[i]);
    }
    return ok;
}
static int arp_new_modes(void)
{
    static const uint8_t CEG[3] = {60, 64, 67}, CEGB[4] = {60, 64, 67, 71}, GCE[3] = {67, 60, 64};
    static const uint8_t FIVE[5] = {60, 64, 67, 71, 74};
    int bad = 0, ok;
    uint32_t i, k;
    uint8_t a[32], b[32];
    track_t *t;
    ui_power_on();
    t = &trk[0];
    ok = TP[P_AMODE].max == 15 && str_eq(TP[P_AMODE].names[6], "REPEAT") && str_eq(TP[P_AMODE].names[7], "DNUP") &&
         str_eq(TP[P_AMODE].names[8], "UP+8") && str_eq(TP[P_AMODE].names[9], "CONV") &&
         str_eq(TP[P_AMODE].names[10], "DIVG") && str_eq(TP[P_AMODE].names[11], "PINKY") &&
         str_eq(TP[P_AMODE].names[12], "THUMB") && str_eq(TP[P_AMODE].names[13], "WALK") &&
         str_eq(TP[P_AMODE].names[14], "CHORD") && str_eq(TP[P_AMODE].names[15], "SEQ");   /* (DUELL: SEQ, the sequencer, appended) */
    for (i = 0; i <= 15u; i++)
        ok &= strlen(TP[P_AMODE].names[i]) <= 6u;
    bad += check("ARP modes: OFF..REPEAT keep their numbers, DNUP UP+8 CONV DIVG PINKY THUMB WALK CHORD SEQ appended (<= 6 chars)", ok);
    /* the modes before 1.2 play as they did */
    arp_fresh(t, AM_UP, 1, CEG, 3);   ok = arp_plays(t, (const uint8_t[]){60, 64, 67, 60}, 4);
    arp_fresh(t, AM_DN, 1, CEG, 3);   ok &= arp_plays(t, (const uint8_t[]){67, 64, 60, 67}, 4);
    arp_fresh(t, AM_UPDN, 1, CEG, 3); ok &= arp_plays(t, (const uint8_t[]){60, 64, 67, 64, 60}, 5);
    bad += check("ARP UP, DN, UPDN as before", ok);
    /* 1.2: ORD plays the notes in the order pressed (it played as UP before), across OCT, ARP 2's ORD NOTE or PLAY */
    arp_fresh(t, AM_ORD, 1, GCE, 3);  ok = arp_plays(t, (const uint8_t[]){67, 60, 64, 67}, 4);
    arp_fresh(t, AM_ORD, 2, GCE, 3);  ok &= arp_plays(t, (const uint8_t[]){67, 60, 64, 79, 72, 76, 67}, 7);
    arp_fresh(t, AM_ORD, 1, CEG, 3);  ok &= arp_plays(t, (const uint8_t[]){60, 64, 67, 60}, 3);
    t->p[P_AORDER] = 1;
    arp_fresh(t, AM_ORD, 1, GCE, 3);  ok &= arp_plays(t, (const uint8_t[]){67, 60, 64}, 3);
    t->p[P_AORDER] = 0;
    arp_fresh(t, AM_ORD, 1, GCE, 3);  arp_remove(t, 60); arp_add(t, 62);   /* C let go, D pressed: G E D */
    ok &= arp_plays(t, (const uint8_t[]){67, 64, 62, 67}, 4);
    bad += check("ARP ORD (1.2): G C E pressed play G C E (not C E G), across OCT 2, either ORD switch; a new note last", ok);
    arp_fresh(t, AM_DNUP, 1, CEG, 3);  ok = arp_plays(t, (const uint8_t[]){67, 64, 60, 64, 67, 64}, 6);
    arp_fresh(t, AM_DNUP, 1, CEGB, 4); ok &= arp_plays(t, (const uint8_t[]){71, 67, 64, 60, 64, 67, 71}, 7);
    arp_fresh(t, AM_DNUP, 2, CEG, 3);  ok &= arp_plays(t, (const uint8_t[]){79, 76, 72, 67, 64, 60, 64, 67, 72, 76, 79}, 11);
    bad += check("ARP DNUP: G E C E; B G E C E G for 4; across OCT 2", ok);
    arp_fresh(t, AM_UP8, 1, CEG, 3);  ok = arp_plays(t, (const uint8_t[]){60, 64, 67, 72, 60}, 5);
    arp_fresh(t, AM_UP8, 1, CEGB, 4); ok &= arp_plays(t, (const uint8_t[]){60, 64, 67, 71, 72, 60}, 6);
    arp_fresh(t, AM_UP8, 2, CEG, 3);  ok &= arp_plays(t, (const uint8_t[]){60, 64, 67, 72, 76, 79, 84, 60}, 8);
    t->p[P_AORDER] = 1;                               /* ORD PLAY: as played, G C E, then G an octave up */
    arp_fresh(t, AM_UP8, 1, GCE, 3);  ok &= arp_plays(t, (const uint8_t[]){67, 60, 64, 79, 67}, 5);
    t->p[P_AORDER] = 0;
    bad += check("ARP UP+8: C E G C'; C E G B C'; OCT 2 ends on C''; ORD PLAY: the first played an octave over the top", ok);
    arp_fresh(t, AM_CONV, 1, CEG, 3);  ok = arp_plays(t, (const uint8_t[]){60, 67, 64, 60}, 4);
    arp_fresh(t, AM_CONV, 1, CEGB, 4); ok &= arp_plays(t, (const uint8_t[]){60, 71, 64, 67, 60}, 5);
    arp_fresh(t, AM_CONV, 2, CEG, 3);  ok &= arp_plays(t, (const uint8_t[]){60, 79, 64, 76, 67, 72, 60}, 7);
    arp_fresh(t, AM_DIVG, 1, CEG, 3);  ok &= arp_plays(t, (const uint8_t[]){64, 67, 60, 64}, 4);
    arp_fresh(t, AM_DIVG, 1, CEGB, 4); ok &= arp_plays(t, (const uint8_t[]){67, 64, 71, 60, 67}, 5);
    bad += check("ARP CONV: C G E, C B E G (outside in), OCT 2; DIVG: E G C, G E B C (inside out)", ok);
    arp_fresh(t, AM_PINKY, 1, CEG, 3);  ok = arp_plays(t, (const uint8_t[]){60, 67, 64, 67, 60}, 5);
    arp_fresh(t, AM_PINKY, 1, CEGB, 4); ok &= arp_plays(t, (const uint8_t[]){60, 71, 64, 71, 67, 71, 60}, 7);
    arp_fresh(t, AM_PINKY, 2, CEG, 3);  ok &= arp_plays(t, (const uint8_t[]){60, 79, 64, 79, 67, 79, 72, 79, 76, 79, 60}, 11);
    arp_fresh(t, AM_THUMB, 1, CEG, 3);  ok &= arp_plays(t, (const uint8_t[]){60, 64, 60, 67, 60}, 5);
    arp_fresh(t, AM_THUMB, 1, CEGB, 4); ok &= arp_plays(t, (const uint8_t[]){60, 64, 60, 67, 60, 71, 60}, 7);
    arp_fresh(t, AM_THUMB, 1, CEG, 1);  ok &= arp_plays(t, (const uint8_t[]){60, 60, 60}, 3);   /* (one note) */
    bad += check("ARP PINKY: C G E G, C B E B G B, the OCT 2 top; THUMB: C E C G, C E C G C B; one note", ok);
    /* CHORD: the held notes together, an octave up each step through OCT; the first 4 of more */
    arp_fresh(t, AM_CHORD, 2, CEG, 3);
    ok = 1;
    for (i = 0; i < 4u; i++) {
        uint32_t o = 12u * (i & 1u);
        t->arp_pos = 0xFFFFFFF;
        arp_tick(t, CTL);
        ok &= t->arp_note == 60u + o && t->arp_ch[0] == 64u + o && t->arp_ch[1] == 67u + o && !t->arp_ch[2];
    }
    arp_fresh(t, AM_CHORD, 1, FIVE, 5);
    t->arp_pos = 0xFFFFFFF;
    arp_tick(t, CTL);
    ok &= t->arp_note == 60u && t->arp_ch[0] == 64u && t->arp_ch[1] == 67u && t->arp_ch[2] == 71u;
    t->arp_off = 1;                                    /* its gate ends: all four end */
    arp_tick(t, 2);
    ok &= !t->arp_note && !t->arp_ch[0] && !t->arp_ch[1] && !t->arp_ch[2];
    arp_fresh(t, AM_CHORD, 1, CEG, 3);
    t->arp_pos = 0xFFFFFFF;
    arp_tick(t, CTL);
    arp_remove(t, 60); arp_remove(t, 64); arp_remove(t, 67);
    arp_tick(t, CTL);                                  /* let go: the chord ends */
    ok &= !t->nheld && !t->arp_note && !t->arp_ch[0] && !t->arp_ch[1];
    bad += check("ARP CHORD: C E G together, then an octave up (OCT 2); 5 held: the first 4; the gate and a release end them all", ok);
    /* WALK: from the bottom, one place up or down each step, inside the list; the same from the same seed */
    ok = 1;
    for (k = 0; k < 2u; k++) {
        rng_state = 0x2468ACEu;
        arp_fresh(t, AM_WALK, 2, CEGB, 4);             /* the list: C E G B C' E' G' B' */
        for (i = 0; i < 32u; i++) {
            t->arp_pos = 0xFFFFFFF;
            arp_tick(t, CTL);
            (k ? b : a)[i] = t->arp_note;
        }
    }
    ok &= a[0] == 60u && !memcmp(a, b, sizeof a);
    {
        static const uint8_t L[8] = {60, 64, 67, 71, 72, 76, 79, 83};
        uint32_t pos[32], up = 0, dn = 0;
        for (i = 0; i < 32u; i++) {
            for (pos[i] = 0; pos[i] < 8u && L[pos[i]] != a[i]; pos[i]++)
                ;
            ok &= pos[i] < 8u;
            if (i) {
                ok &= pos[i] == pos[i - 1u] + 1u || pos[i] + 1u == pos[i - 1u];
                up += pos[i] > pos[i - 1u];
                dn += pos[i] < pos[i - 1u];
            }
        }
        ok &= up > 4u && dn > 4u;
    }
    rng_state = 0x13579BDu;
    arp_fresh(t, AM_WALK, 2, CEGB, 4);
    for (i = 0; i < 32u; i++) {
        t->arp_pos = 0xFFFFFFF;
        arp_tick(t, CTL);
        b[i] = t->arp_note;
    }
    ok &= memcmp(a, b, sizeof a) != 0;
    arp_remove(t, 71); arp_remove(t, 67);              /* notes let go: back inside the shorter list */
    for (i = 0; i < 8u; i++) {
        t->arp_pos = 0xFFFFFFF;
        arp_tick(t, CTL);
        ok &= t->arp_note == 60 || t->arp_note == 64 || t->arp_note == 72 || t->arp_note == 76;
    }
    bad += check("ARP WALK: from the bottom, a step up or down each time inside the list; the same seed the same walk", ok);
    /* HOLD: the latched chord keeps its mode; a new chord replaces it */
    t->p[P_AHOLD] = 1;
    arp_fresh(t, AM_PINKY, 1, CEG, 3);
    arp_remove(t, 60); arp_remove(t, 64); arp_remove(t, 67);
    ok = t->nheld == 3u && arp_plays(t, (const uint8_t[]){60, 67, 64, 67}, 4);
    arp_add(t, 62); arp_add(t, 65); arp_add(t, 69);   /* D F A: replaces it, from the top of the pattern */
    ok &= t->nheld == 3u && arp_plays(t, (const uint8_t[]){62, 69, 65, 69}, 4);
    arp_remove(t, 62); arp_remove(t, 65); arp_remove(t, 69);
    t->p[P_AHOLD] = 0;
    bad += check("ARP HOLD: PINKY keeps playing the latched C E G; D F A replaces it", ok);
    /* REPEAT in and out: the last played note alone, then the new mode over the whole chord again */
    arp_fresh(t, AM_REPEAT, 2, CEG, 3);
    ok = arp_plays(t, (const uint8_t[]){67, 67}, 2);
    t->p[P_AMODE] = AM_THUMB;                          /* (its step count goes on: step 2 of C E C G C' ..) */
    ok &= arp_plays(t, (const uint8_t[]){60, 67, 60, 72}, 4);
    t->p[P_AMODE] = AM_REPEAT;
    ok &= arp_plays(t, (const uint8_t[]){67, 67}, 2);
    t->p[P_AMODE] = AM_CHORD;
    t->arp_pos = 0xFFFFFFF;
    arp_tick(t, CTL);
    ok &= t->arp_ch[0] && t->arp_ch[1];
    t->p[P_AMODE] = AM_REPEAT;
    ok &= arp_plays(t, (const uint8_t[]){67}, 1);      /* (CHORD's other notes ended) */
    bad += check("ARP REPEAT switched to THUMB and back, CHORD to REPEAT: each plays its own", ok);
    /* recording: CHORD writes its notes into one step, as the keys' chords (POLY) */
    song.sel = 0; song.rec = 1;
    t->p[P_VOICE] = V_POLY;
    for (i = 0; i < NSTEP; i++)
        t->step[i] = (step_t){0};
    seq_start();
    seq_tick(t, CTL);
    arp_fresh(t, AM_CHORD, 1, CEG, 3);
    t->arp_pos = 0xFFFFFFF;
    arp_tick(t, CTL);
    for (i = 0, k = 0; i < NSTEP; i++)
        k += t->step[i].n == 3u && t->step[i].note[0] == 60 && t->step[i].note[1] == 64 && t->step[i].note[2] == 67;
    seq_stop();
    song.rec = 0;
    bad += check("ARP CHORD recorded: C E G into one step", k == 1u);
    arp_fresh(t, AM_OFF, 1, CEG, 0);
    return bad;
}
/* ------------------------------------------------ 1.1 parameter locks --- */
/* the transport on to the next step of t (its seq_idx changes) */
static void lock_next(track_t *t)
{
    uint32_t i = t->seq_idx, n = 0;
    while (t->seq_idx == i && n++ < 100000u)
        seq_tick(t, CTL);
}
/* track 1 alone: 8 NOTE steps, REV 20, swing sw */
static track_t *lock_track(int32_t sw)
{
    track_t *t;
    uint32_t i;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 8;
    t->p[P_SSWING] = (int16_t)sw;
    song.g[G_SWING] = 0;
    t->p[P_REV] = 20;
    for (i = 0; i < 8u; i++)
        t->step[i] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    return t;
}
/* two passes from step 0: REV at each step's start as want[]; 1 = all as wanted */
static int lock_passes(track_t *t, const int16_t *want)
{
    uint32_t i, pass;
    seq_start();
    seq_tick(t, CTL);
    for (pass = 0; pass < 2u; pass++)
        for (i = 0; i < 8u; i++) {
            if (pass || i)
                lock_next(t);
            if (t->seq_idx != i || t->p[P_REV] != want[i]) {
                printf("motion:   pass %u step %u: REV %d, wanted %d\n", pass, i, t->p[P_REV], want[i]);
                return 0;
            }
        }
    return 1;
}
static int lock_playback(void)
{
    static const int16_t want[8] = {20, 20, 100, 20, 60, 60, 90, 60};   /* locks on 2, 6; automation on 4 */
    int bad = 0, ok;
    uint32_t n;
    track_t *t = lock_track(0);
    project_t q;
    ok = !motion_set_lock(t, 2, P_REV, 100) && !motion_set_event(t, 4, P_REV, 60) && !motion_set_lock(t, 6, P_REV, 90);
    bad += check("a lock is one of the shared 64 records, bit 7 of its id (MOTION_LOCK)", ok && motion.count == 3u &&
        motion.event[0].param == (P_REV | MOTION_LOCK) && motion.event[1].param == P_REV && motion_lock_count(t) == 2u &&
        motion_count(t) == 3u && motion_lock_steps(0) == ((1u << 2) | (1u << 6)));
    bad += check("lock: its value on its step only, back after it (the sound's own, or the automation held)",
        lock_passes(t, want));
    lock_next(t); lock_next(t); lock_next(t);
    project_capture(&q);
    bad += check("  a lock sounding: the sound's own value is the base, a project saves that", t->seq_idx == 2u &&
        t->p[P_REV] == 100 && motion_base_value(t, P_REV) == 20 && q.t[0].p[P_REV] == 20);
    motion_set_enabled(t, 0);
    bad += check("  AUTOMATION PLAY OFF: the sound's own value at once, the locks kept", t->p[P_REV] == 20 &&
        motion_lock_count(t) == 2u);
    motion_set_enabled(t, 1);
    lock_next(t); lock_next(t); lock_next(t); lock_next(t);
    seq_stop();
    bad += check("  STOP while a lock sounds puts the sound's own value back", t->p[P_REV] == 20);
    t = lock_track(100);
    motion_set_lock(t, 2, P_REV, 100); motion_set_event(t, 4, P_REV, 60); motion_set_lock(t, 6, P_REV, 90);
    bad += check("  with SWING 100: the same values step by step", lock_passes(t, want));
    seq_stop();
    t = lock_track(0);                                   /* the chance: a step that does not play applies no lock */
    motion_set_lock(t, 2, P_REV, 100);
    step_set_chance(&t->step[2], 0);
    {
        static const int16_t w0[8] = {20, 20, 20, 20, 20, 20, 20, 20};
        bad += check("  CHANCE 0 %: the step does not play, nor its lock", lock_passes(t, w0));
    }
    seq_stop();
    step_set_chance(&t->step[2], 100);
    {
        static const int16_t w1[8] = {20, 20, 100, 20, 20, 20, 20, 20};
        bad += check("  CHANCE 100 %: its lock", lock_passes(t, w1));
    }
    seq_stop();
    step_set_chance(&t->step[2], 50);                    /* the one roll: the lock with the notes, or neither */
    seq_start(); seq_tick(t, CTL);
    for (n = 0, ok = 1; n < 400u; n++) {
        lock_next(t);
        if (t->seq_idx == 2u)
            ok &= (t->p[P_REV] == 100) == (t->seq_n != 0u);
    }
    bad += check("  CHANCE 50 %: the lock applies exactly when the step's notes play", ok);
    seq_stop();
    t = lock_track(0);                                   /* RATCH x3: the lock over all its parts */
    motion_set_lock(t, 2, P_REV, 100);
    step_set_ratchet(&t->step[2], 3);
    seq_start(); seq_tick(t, CTL);
    lock_next(t); lock_next(t);
    {
        uint32_t v0 = vage;
        for (ok = t->seq_idx == 2u; t->seq_idx == 2u;) {
            ok &= t->p[P_REV] == 100;
            seq_tick(t, CTL);
        }
        bad += check("  RATCH x3: its 3 parts all with the lock, back at the next step", ok && vage - v0 >= 3u &&
            t->p[P_REV] == 20);
    }
    seq_stop();
    t = lock_track(0);                                   /* an engine parameter, read by the note-on */
    t->p[P_E4] = 10;
    motion_set_lock(t, 1, P_E4, 90);
    motion_set_lock(t, 1, P_E4, 300);                    /* (out of its range: refused, the lock as it was) */
    seq_start(); seq_tick(t, CTL); lock_next(t);
    bad += check("  an engine parameter: set at the step's start (before its notes), in its range", t->p[P_E4] == 90 &&
        t->seq_n == 1u);
    lock_next(t);
    bad += check("  .. and back at the next step", t->p[P_E4] == 10);
    seq_stop();
    return bad;
}
static int lock_edit(void)
{
    int bad = 0;
    int16_t v;
    track_t *t = lock_track(0);
    motion_set_event(t, 3, P_REV, 50);
    bad += check("a lock replaces an automation event of its step and id (one record, either kind)",
        !motion_set_lock(t, 3, P_REV, 70) && motion.count == 1u && motion_lock_get(t, 3, P_REV, &v) && v == 70);
    motion_set_event(t, 3, P_REV, 40);
    bad += check("  .. and back (an automation event over a lock)", motion.count == 1u && !motion_lock_count(t));
    motion_set_lock(t, 3, P_ATK, 9); motion_set_lock(t, 5, P_ATK, 9); motion_set_lock(&trk[1], 3, P_REV, 1);
    bad += check("clear a step's locks: its automation and the other steps' and tracks' stay",
        motion_clear_locks(t, 3) == 1u && motion.count == 3u && motion_lock_count(t) == 1u && motion_lock_count(&trk[1]) == 1u);
    bad += check("clear every step's locks (NSTEP)", motion_clear_locks(t, NSTEP) == 1u && !motion_lock_count(t) &&
        motion_count(t) == 1u);
    motion_delete_event(&trk[1], 3, P_REV);
    bad += check("delete an event deletes a lock too (either kind)", !motion_count(&trk[1]));
    return bad;
}
static int lock_project(void)
{
    int bad = 0;
    track_t *t = lock_track(0);
    project_t before, after;
    project_store_t a, b;
    uint32_t i, ok = 1;
    motion_set_event(t, 1, P_REV, 33);                   /* no lock: as every project before 1.1 */
    project_capture(&before);
    ok = proj_pack(&a, &before) && proj_import(&after, &a, sizeof a) && proj_pack(&b, &after) && !memcmp(&a, &b, sizeof a);
    for (i = 0; i < before.motion.count; i++)
        ok &= before.motion.event[i].param < 128u && !(before.motion.event[i].param & MOTION_LOCK);
    bad += check("a project without locks: the bytes as before (ids < 128), the same round trip", ok);
    motion_set_lock(t, 2, P_REV, 101); motion_set_lock(t, 7, P_E4, 77);
    project_capture(&before);
    ok = proj_pack(&a, &before) && proj_import(&after, &a, sizeof a) && !memcmp(&before.t, &after.t, NSHOWN * sizeof before.t[0]) &&
         !memcmp(&before.motion, &after.motion, sizeof before.motion);   /* (DUELL: FUN11 keeps parts A B) */
    bad += check("FUN9 round trip keeps the locks (bit 7 of the id byte), their values and steps", ok &&
        after.motion.event[1].param == (P_REV | MOTION_LOCK) && after.motion.event[2].value == 77);
    project_save(2);
    motion_clear(t);
    project_load(2);
    bad += check("  saved and loaded: the locks play again", motion_lock_count(t) == 2u && lock_passes(t,
        (const int16_t[8]){20, 33, 101, 33, 33, 33, 33, 33}));
    seq_stop();
    {   /* a lock on an engine id of a store of fewer parameters (a FUN7 of 89, before the chord keys; a FUN8 of 91,
         * before the DRUM lane levels, as a 1.1 development build wrote it): moves with its id, keeps bit 7 */
        static const uint32_t np[2] = {89u, 91u};
        for (uint32_t n = 0; n < 2u; n++) {
            motion_store_t m = before.motion;
            for (i = 0; i < m.count; i++)
                if ((m.event[i].param & 0x7Fu) == P_E4)
                    m.event[i].param = (uint8_t)((P_E4 - (P_COUNT - np[n])) | MOTION_LOCK);
            ok = proj_motion_ids(&m, np[n]);
            bad += check(n ? "  FUN8 of 91: a lock's id moves up with P_E0 (87 -> P_E4), still a lock"
                           : "  FUN7 of 89: a lock's id moves up with P_E0 (85 -> P_E4), still a lock",
                ok && m.event[2].param == (P_E4 | MOTION_LOCK) && m.event[1].param == (P_REV | MOTION_LOCK) &&
                m.event[0].param == P_REV);
            m = before.motion;
            m.event[2].param = (uint8_t)(np[n] | MOTION_LOCK);
            bad += check("    an id that store could not name (np): refused", !proj_motion_ids(&m, np[n]));
        }
    }
    return bad;
}
/* the UI: hold a step, turn a knob (ui_input.c lock_*) */
static void lk_down(uint32_t k) { fm1_in.notes |= 1u << k; host_notes |= 1u << k; frame(); }
static void lk_up(uint32_t k) { fm1_in.notes &= ~(1u << k); frame(); }
static int lock_ui(void)
{
    int bad = 0, ok;
    int16_t v;
    track_t *t;
    uint32_t k0, k4, kn;
    ui_power_on();
    song.sel = 3;                                        /* track 4: DRUM, the grid */
    t = &trk[3];
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    go_home();
    open_family(FAM_SEQ);
    frame();
    k0 = key_at(0, 0); k4 = key_at(0, 4);
    grid_hit(t, 0, 0, 1);                                /* a KICK on step 1 */
    bad += check("the grid on SEQ > STEP; the locks' knobs: HOME's (DRUM: E2..E5)", grid_on() && lock_id(0) == P_E1 &&
        lock_id(3) == P_E4);
    lk_down(k4);
    bad += check("a step key down: its step held (an empty one gets its hit at once)", lock_held() == (1u << 4) &&
        (t->step[4].hit & 1u));
    turn(EN_K1, 3);
    ok = motion_lock_get(t, 4, P_E1, &v) && v == motion_base_value(t, P_E1) + 3;
    turn(EN_K1, 2);
    bad += check("  KNOB 1 turned: step 5 gets a lock of E2 (TUNE), from the sound's own value", ok &&
        motion_lock_get(t, 4, P_E1, &v) && v == motion_base_value(t, P_E1) + 5 && t->p[P_E1] == motion_base_value(t, P_E1));
    lk_up(k4);
    bad += check("  let go: the hit stays, the lock is there, the sound unchanged", (t->step[4].hit & 1u) &&
        motion_lock_count(t) == 1u && !lock_held());
    lk_down(k0);
    turn(EN_K2, -4);
    lk_up(k0);
    bad += check("a key pressed on a hit and held for a lock: the hit stays", (t->step[0].hit & 1u) &&
        motion_lock_get(t, 0, P_E2, &v));
    tap_key(k0);
    bad += check("  tapped alone: the hit goes (when let go), its lock stays", !(t->step[0].hit & 1u) &&
        motion_lock_count(t) == 2u);
    fm1_in.notes |= 1u << k0 | 1u << k4; host_notes |= 1u << k0 | 1u << k4; frame();
    turn(EN_K4, 1);
    fm1_in.notes = 0; frame();
    bad += check("two step keys held: both get the lock", motion_lock_get(t, 0, P_E4, &v) && motion_lock_get(t, 4, P_E4, &v) &&
        motion_lock_count(t) == 4u);
    lk_down(k4);
    press(B_EDIT);
    lk_up(k4);
    bad += check("a step held + [EDIT]: its locks go, the step and the others' locks stay", motion_lock_count(t) == 2u &&
        !motion_lock_get(t, 4, P_E1, &v) && motion_lock_get(t, 0, P_E4, &v) && (t->step[4].hit & 1u) &&
        str_eq(ui.msg, "LOCKS CLEARED"));
    bad += check("the steps with a lock (the grid marks them): step 1", motion_lock_steps(3) == 1u);
    open_family(FAM_FX);                                 /* the lock's knobs: the sound page shown last */
    open_family(FAM_SEQ);
    frame();
    bad += check("from FX: the knobs lock DIST CHO DLY REV", lock_id(0) == P_DIST && lock_id(3) == P_REV);
    lk_down(k4); turn(EN_K4, 5); lk_up(k4);
    bad += check("  step 5 gets a REV lock", motion_lock_get(t, 4, P_REV, &v) && v == t->p[P_REV] + 5);
    go_auto_top();
    frame();
    bad += check("AUTOMATION: EVENT 0, LOCK 3", motion_count(t) - motion_lock_count(t) == 0u && motion_lock_count(t) == 3u);
    /* the roll: the step being entered is the one held */
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    go_home();
    open_family(FAM_SEQ);
    frame();
    for (kn = 0; kn < 27u && kb_map(t, kn) > 127u; kn++)
        ;
    lk_down(kn);                                         /* (1.2, #133: not armed, a key only plays: no step held) */
    ok = !ui.entry_open && !lock_held() && !t->step[0].n;
    turn(EN_K3, 4);
    lk_up(kn);
    bad += check("the roll, not armed: a key held writes no step and holds none; KNOB 3 locks nothing",
                 ok && !motion_lock_count(t) && !t->step[0].n && ui.cursor == 0u);
    song.rec = 1;                                        /* armed, stopped: step recording */
    lk_down(kn);
    ok = ui.entry_open && lock_held() == 1u && t->step[0].n == 1u;
    turn(EN_K3, 4);                                      /* ANALOG's HOME: E5 E6 ATK REL */
    bad += check("the roll, armed (step recording): a note key held (the entry) is the step held; KNOB 3 locks ATK, TIME untouched", ok &&
        motion_lock_get(t, 0, P_ATK, &v) && v == t->p[P_ATK] + 4 && t->step[0].time == ST_NOTE);
    lk_up(kn);
    bad += check("  let go: the note and the lock on step 1, the cursor on", ui.cursor == 1u && t->step[0].n == 1u &&
        motion_lock_count(t) == 1u && motion_lock_steps(0) == 1u);
    cursor_set(0);
    press(B_EDIT);
    bad += check("  [EDIT] alone clears the step and its locks", !t->step[0].n && !motion_lock_count(t));
    return bad;
}
/* 1.1.5: lock edits by hand are one undo (SAVE held), redo too */
static int lock_undo(void)
{
    int bad = 0;
    int16_t v;
    track_t *t;
    uint32_t k4;
    ui_power_on();
    song.sel = 3;
    t = &trk[3];
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    go_home();
    open_family(FAM_SEQ);
    frame();
    k4 = key_at(0, 4);
    lk_down(k4);
    turn(EN_K1, 3);
    turn(EN_K1, 2);
    lk_up(k4);
    bad += check("lock undo: a step held, KNOB 1 turned twice: one lock", motion_lock_get(t, 4, P_E1, &v) &&
        v == motion_base_value(t, P_E1) + 5);
    hold(B_SAVE);
    bad += check("  SAVE held: the lock is gone (one undo for the whole turn), the step's hit stays",
        !motion_lock_count(t) && (t->step[4].hit & 1u));
    hold(B_SAVE);
    bad += check("  held again (redo): the lock is back at its value", motion_lock_get(t, 4, P_E1, &v) &&
        v == motion_base_value(t, P_E1) + 5);
    frames(2000);
    lk_down(k4);
    press(B_EDIT);
    lk_up(k4);
    bad += check("  a step held + [EDIT] clears it", !motion_lock_count(t) && str_eq(ui.msg, "LOCKS CLEARED"));
    hold(B_SAVE);
    bad += check("  SAVE held brings it back", motion_lock_get(t, 4, P_E1, &v) && v == motion_base_value(t, P_E1) + 5);
    frames(2000);
    lk_down(k4);
    press(B_EDIT);
    lk_up(k4);
    lk_down(k4);
    press(B_EDIT);                                       /* (no lock there now: NO LOCKS, the undo copy kept) */
    lk_up(k4);
    hold(B_SAVE);
    bad += check("  [EDIT] on a step without locks takes no undo copy (the clear before stays undoable)",
        motion_lock_count(t) == 1u);
    return bad;
}
/* 1.1.5: SEQ > AUTO LIST (ui_events.c); 1.2: the AUTOMATION page, its rows: PLAY, QUANTIZE, the records (and CHANCE / RATCH / NUDGE), + ADD */
static uint32_t rows_n(void) { uint16_t rw[EV_ROWS]; return ev_list(rw); }   /* PLAY, QUANTIZE and + ADD included */
static uint32_t row_at(uint32_t r) { uint16_t rw[EV_ROWS]; return r < ev_list(rw) ? rw[r] : 0xFFFFu; }
static int auto_list(void)
{
    int bad = 0, ok;
    int16_t v;
    track_t *t;
    uint32_t id0, id1, i;
    ui_power_on();
    song.sel = 0;
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    go_home();
    cursor_set(4);
    go_page(GR_EVENTS);
    frame();
    id0 = ui.ev_id;
    ok = auto_cur() == EVC(EVK_TOP, 0) && str_eq(act_name(4), "CLEAR") && !act_ready();
    turn(EN_K1, 2);                                      /* (past QUANTIZE, 1.2) */
    bad += check("AUTOMATION: empty, PLAY then + ADD LOCK (on the SEQ cursor's step, the first knob of the sound page)",
        ok && cur_page()->graph == GR_EVENTS && rows_n() == 3u && auto_cur() == EVC(EVK_ADD, 0) && ui.ev_step == 4u &&
        id0 == lock_id(0) && str_eq(act_name(4), "ADD") && act_ready());
    press(B_OCTUP);
    bad += check("  OCT+: a lock on step 5 at the sound's own value, its row selected",
        motion_lock_get(t, 4, id0, &v) && v == motion_base_value(t, id0) && ui.ev_row == 2u && auto_cur() >> 8 == EVK_REC &&
        str_eq(ui.msg, "LOCK ADDED") && motion_enabled(t));
    turn(EN_K4, 3);
    bad += check("  KNOB 4: its value", motion_lock_get(t, 4, id0, &v) && v == motion_base_value(t, id0) + 3);
    turn(EN_K2, 2);
    bad += check("  KNOB 2: its step (5 -> 7), the value kept", !motion_lock_get(t, 4, id0, &v) &&
        motion_lock_get(t, 6, id0, &v) && v == motion_base_value(t, id0) + 3);
    turn(EN_K3, 1);
    id1 = MOTION_ID(&motion.event[0]);
    bad += check("  KNOB 3: the next parameter, from the sound's own value", id1 > id0 && ev_id_ok(t, id1) &&
        motion_lock_get(t, 6, id1, &v) && v == motion_base_value(t, id1) && motion_count(t) == 1u);
    press(B_OCTUP);
    bad += check("  OCT+ on the row: LOCK -> AUTO", motion_count(t) == 1u && !motion_lock_count(t) &&
        str_eq(act_name(4), "LOCK"));
    press(B_OCTUP);
    ok = motion_lock_count(t) == 1u;
    press(B_OCTUP);
    bad += check("  and back (AUTO -> LOCK -> AUTO)", ok && !motion_lock_count(t));
    turn(EN_K1, 1);
    turn(EN_K2, -2);
    bad += check("  KNOB 1: + ADD LOCK; KNOB 2 there: its step", auto_cur() == EVC(EVK_ADD, 0) && ui.ev_step == 2u);
    press(B_OCTUP);
    bad += check("  OCT+: a second lock on step 3, first in the list (by step)", motion_count(t) == 2u &&
        motion_lock_get(t, 2, ui.ev_id, &v) && rows_n() == 5u && ui.ev_row == 2u &&
        (motion.event[row_at(2) & 0xFFu].place & 63u) == 2u);
    turn(EN_K1, 5);
    press(B_OCTUP);
    bad += check("  OCT+ on + ADD LOCK where one is: ALREADY LISTED, that row selected", motion_count(t) == 2u &&
        str_eq(ui.msg, "ALREADY LISTED") && ui.ev_row == 2u);
    press(B_EDIT);
    bad += check("  EDIT: the row's record goes", motion_count(t) == 1u && !motion_lock_get(t, 2, ui.ev_id, &v) &&
        str_eq(ui.msg, "DELETED"));
    /* (DUELL: SAVE held is the overwrite dialog, not an undo: the undo / redo checks that were here are gone) */
    frames(2000);
    turn(EN_K1, -5);
    turn(EN_K1, 2);                                      /* (the first record: the row after PLAY and QUANTIZE) */
    v = motion.event[0].value;
    turn(EN_K4, 2); turn(EN_K4, 2); turn(EN_K4, 2);
    ok = motion.event[0].value == v + 6;
    bad += check("  KNOB 4 turned on (three turns)", ok);
    for (i = 0; motion.count < MOTION_MAX; i++)          /* the 128 used (track 2) */
        motion_set_event(&trk[1], i % NSTEP, i < NSTEP ? P_REV : P_DLY, 10);
    turn(EN_K1, 5);
    press(B_OCTUP);
    bad += check("  the 128 used: OCT+ adds nothing, AUTOMATION FULL", motion_count(t) == 1u &&
        str_eq(ui.msg, "AUTOMATION FULL") && !act_ready());
    motion_clear(&trk[1]);
    for (i = 0; i < P_COUNT; i++)                        /* names: an id each, none of DIGITAL's, lanes on DRUM only */
        if (ev_id_ok(t, i) && !motion_param(i))
            break;
    bad += check("  KNOB 3's list: what motion records and the sound has", i == P_COUNT);
    {
        char a[12], b[12];
        ev_name(t, P_ED_FLT, a); ev_name(t, P_LD_FLT, b);
        bad += check("  names: ENV FLT / LFO FLT", str_eq(a, "ENV FLT") && str_eq(b, "LFO FLT"));
    }
    ui_prefs |= PREF_LARGE;
    bad += check("  MENU > LARGE: the list layout with labels in M (as the other lists)", large_kind() == LK_LABEL);
    ui_prefs &= ~PREF_LARGE;
    return bad;
}
int main(void)
{
    int bad = motion_recording() + motion_capacity() + probability_playback() + compact_project() + fun7_89() + loads() + repeat_mode();
    bad += arp_new_modes();
    bad += lock_playback() + lock_edit() + lock_project() + auto_list();
#ifdef MOTION_TEST_STALE
    /* Felucca-era groups that need the DRUM grid (track 4) or SAVE held = undo, both gone in DUELL: compiled, not run. Port, then move up. */
    bad += lock_ui() + lock_undo();
#endif
    printf("%s\n", bad ? "MOTION TEST FAILED" : "motion/chance/compact storage tests passed"); return bad != 0;
}
