/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the project format (firmware/src/project.c, -DPROJ_HOST part). DUELL 0.2.14 reads one format only: FUN11 ("FUNB",
 * 3840 bytes, parts A and B, P_COUNT parameters, revision 3). What it checks:
 *  - a project round trips (globals, selection, values, steps with RATCH / NUDGE / chance, locks and automation, the FM6 patches, the name),
 *    tracks 3..4 load with their power-on sound;
 *  - damaged ones are refused (hash, a byte of the name or a patch, ranges), and a step or patch that cannot be packed is not packed;
 *  - a track on a retired engine (DIGITAL 1, SAMPLE 4, GRAIN 8, DRUM 10, SLICE 13) loads as stored, nothing converted;
 *  - every OLD format is refused, with a valid hash: the FUN11 of 0.2.13 and before (revision 0), Felucca's FUN10 (its magic), FUN9, FUN8
 *    and FUN7 as firmware 1.0 .. 1.5 wrote them (tests/old_pack.h), an old length, a project whose P_COUNT, NPART or PHYS byte is another;
 *    the project in a longer, retained slot; garbage.
 * Run by host_tests.sh "project". */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1                             /* (trk_def_engine: engines.c, through hostsim.c) */
#include "../firmware/src/project.c"
#include "old_pack.h"                         /* (FUN10 .. FUN7 as firmware before 0.2.14 wrote them) */

static int check(const char *what, int ok)
{
    printf("%-60s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
static void rehash(project_store_t *st)       /* a damaged / altered image with the right hash: the format check, not the hash, must say no */
{
    uint32_t sum = proj_hash(st->raw, PROJ_STORE_SIZE - 4u);
    memcpy(st->raw + PROJ_STORE_SIZE - 4u, &sum, 4);
}
static void base_project(project_t *a)
{
    uint32_t t, i;
    memset(a, 0, sizeof *a);
    a->magic = PROJ_MAGIC; a->size = sizeof *a; a->parts = NPART; a->phys = PROJ_PHYS;
    for (i = 0; i < G_COUNT; i++)
        a->g[i] = (int16_t)(100 + i);
    a->g[G_RTYPE] = 1;
    a->sel = 1;
    chain_defaults(&a->chain);
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < P_COUNT; i++)
            a->t[t].p[i] = (int16_t)(param_desc_of(0, i)->def);
        a->t[t].engine = 0;
        a->t[t].preset = (uint8_t)(t + 3u);
        for (i = 0; i < NSTEP; i++)
            a->t[t].step[i].time = ST_REST;
        memcpy(a->fm6[t], FM6_INIT, FM6_PACKED);
    }
    a->sum = proj_sum(a);
}

int main(void)
{
    int bad = 0, ok;
    uint32_t t, i, k;
    project_t a, c, d;
    static project_store_t st, st2, slot;
    static uint8_t old[3840];

    bad += check("layout: name at 3824, A B's patches at 3568, 1618 spare before them, P_COUNT 127 at byte 66, revision 3",
                 PROJ_NAME_OFF == 3824u && PROJ_FM6_OFF == 3568u && PROJ_NTRK == 2u && P_COUNT == 127u && PROJ_REV == 3u &&
                 68u + PROJ_NTRK * (P_COUNT + 2u + NSTEP * 9u) + sizeof(chain_config_t) + sizeof(motion_store_t) + 1618u == PROJ_FM6_OFF);

    /* a round trip */
    base_project(&a);
    for (i = 0; i < P_COUNT; i++) { a.t[0].p[i] = (int16_t)(i % 100u); a.t[1].p[i] = (int16_t)(-(int32_t)(i % 60u)); }
    a.t[1].engine = ENGI_FM6;
    for (k = 0; k < NSTEP; k++) {
        step_t *s = &a.t[0].step[k];
        s->note[0] = (uint8_t)(36u + k); s->note[1] = (uint8_t)(48u + k % 7u); s->n = (uint8_t)(1u + k % 2u); s->time = ST_NOTE;
        s->flags = (uint8_t)(k & 3u); s->vel = (uint8_t)(64u + k);
        step_set_ratchet(s, 1u + k % 4u);
        step_set_nudge(s, (int32_t)(k % 16u) - 8);
        step_set_chance(s, k % 3u ? 100u : 30u + k % 40u);
    }
    for (k = 0; k < FM6_PACKED; k++) { a.fm6[0][k] = (uint8_t)(k * 3u & 127u); a.fm6[1][k] = (uint8_t)(k * 5u & 127u); }
    a.motion.count = 4; a.motion.on = 3;
    a.motion.event[0] = (motion_event_t){0u << 6 | 1u, P_REV, 90};
    a.motion.event[1] = (motion_event_t){1u << 6 | 2u, (uint8_t)(P_REV | MOTION_LOCK), 40};
    a.motion.event[2] = (motion_event_t){1u << 6 | 6u, (uint8_t)(P_E5 | MOTION_LOCK), -3};
    a.motion.event[3] = (motion_event_t){0u << 6 | 7u, P_E3, 21};
    memcpy(a.name, "lofi jam", 8);
    a.sum = proj_sum(&a);
    ok = proj_pack(&st, &a) && ((uint32_t *)st.raw)[0] == 0x46554E42u && st.raw[66] == P_COUNT && st.raw[67] == PROJ_REV && proj_import(&c, &st, sizeof st);
    bad += check("FUN11 round trip: packs and imports", ok);
    ok = ok && c.sel == 1 && !memcmp(c.g, a.g, sizeof a.g) && !memcmp(c.t, a.t, NSHOWN * sizeof a.t[0]) &&
         !memcmp(&c.motion, &a.motion, sizeof a.motion) && !memcmp(c.fm6, a.fm6, NSHOWN * sizeof a.fm6[0]) &&
         !memcmp(c.name, "LOFI JAM\0\0\0\0", 12) && !memcmp(st.raw + PROJ_NAME_OFF, "LOFI JAM\0\0\0\0", 12) && proj_ok(&c);
    bad += check("  globals, selection, both parts' values / steps (RATCH, NUDGE, chance), locks, automation, FM6 patches, the name (upper case)", ok);
    ok = 1;
    for (t = NSHOWN; t < NTRK; t++)
        ok &= c.t[t].preset == PROJ_DEF_SOUND && c.t[t].engine == trk_def_engine(t) && c.t[t].step[0].time == ST_REST &&
              !memcmp(c.fm6[t], FM6_INIT, FM6_PACKED);
    bad += check("  tracks 3..4 are not stored: their power-on sound, no steps, the init patch", ok);
    bad += check("  imported and packed again: the same bytes", proj_pack(&st2, &c) && !memcmp(&st, &st2, sizeof st));

    /* the name */
    memset(a.name, 0, sizeof a.name);
    a.sum = proj_sum(&a);
    ok = proj_pack(&st2, &a);
    for (i = 0, k = 1; i < PROJ_NAME_LEN; i++) k &= st2.raw[PROJ_NAME_OFF + i] == 0u;
    bad += check("no name: the tail all zero, it loads with no name", ok && k && proj_import(&c, &st2, sizeof st2) && !c.name[0]);
    memcpy(a.name, "ABCDEFGHIJKL", 12);
    a.sum = proj_sum(&a);
    bad += check("a name of 12 characters (no terminator)", proj_pack(&st2, &a) && proj_import(&c, &st2, sizeof st2) && !memcmp(c.name, "ABCDEFGHIJKL", 12));
    st2.raw[PROJ_NAME_OFF + 3] ^= 1u;
    bad += check("a name byte changed: the hash refuses the project", !proj_import(&c, &st2, sizeof st2));
    st2.raw[PROJ_NAME_OFF + 3] = 7u;                      /* not printable, the hash right */
    rehash(&st2);
    bad += check("a name byte outside 32..126: the project loads, no name", proj_import(&c, &st2, sizeof st2) && !c.name[0]);

    /* damage */
    st.raw[PROJ_FM6_OFF + 5] ^= 1u;
    bad += check("an FM6 patch byte changed: the hash refuses it", !proj_import(&c, &st, sizeof st));
    st.raw[PROJ_FM6_OFF + 5] ^= 1u;
    bad += check("  put back: loads again", proj_import(&c, &st, sizeof st));
    st2 = st; st2.raw[PROJ_FM6_OFF + 5] = 200u; rehash(&st2);
    bad += check("an FM6 patch byte above 127 (hash right): the project loads, the byte masked", proj_import(&c, &st2, sizeof st2) &&
                 c.fm6[0][5] == (200u & 127u));
    base_project(&d);
    for (i = 0; i < FM6_PACKED; i++) d.fm6[1][i] = (uint8_t)(i | 0x80u);
    d.sum = proj_sum(&d);
    bad += check("a patch byte above 127 cannot be packed", !proj_pack(&st2, &d));
    base_project(&d);
    d.t[0].step[3].n = 1; d.t[0].step[3].time = ST_NOTE; d.t[0].step[3].note[0] = 200; d.t[0].step[3].note[2] = 128; d.t[0].step[3].vel = 255;
    d.sum = proj_sum(&d);
    ok = proj_pack(&st2, &d) && proj_import(&c, &st2, sizeof st2);
    bad += check("a step out of range packs inside it (note, velocity 127): it loads",
                 ok && c.t[0].step[3].note[0] == 127u && c.t[0].step[3].note[2] == 127u && c.t[0].step[3].vel == 127u);
    base_project(&d);
    d.t[0].p[P_LEVEL] = 300;
    d.sum = proj_sum(&d);
    bad += check("a value that does not fit a byte (-64..127) cannot be packed", !proj_pack(&st2, &d));
    st2 = st; st2.raw[68] = 250; rehash(&st2);
    bad += check("a value byte above 191 (hash right): refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[68u + P_COUNT] = NENGINES; rehash(&st2);
    bad += check("an engine number past the last (hash right): refused", !proj_import(&c, &st2, sizeof st2));
    bad += check("one byte short, or none: refused", !proj_import(&c, &st, sizeof st - 1u) && !proj_import(&c, &st, 0));

    /* retired engines: loaded as stored */
    {
        static const uint8_t GONE[5] = {1, 4, 8, 10, 13};
        base_project(&d);
        for (t = 0; t < NTRK; t++)
            d.t[t].engine = GONE[t];
        d.t[1].p[P_E0] = 4;                             /* (SAMPLE SET 4, PERC: stays that) */
        d.t[1].p[P_LEVEL] = 77;
        d.t[1].step[0] = (step_t){{36, 42, 0, 0}, 2, ST_NOTE, SF_ACCENT, 100};
        d.motion.count = 2;
        d.motion.event[0] = (motion_event_t){1u << 6 | 3u, P_E4, 20};
        d.motion.event[1] = (motion_event_t){0u << 6 | 3u, P_E4, 70};
        d.sum = proj_sum(&d);
        ok = proj_pack(&st2, &d) && proj_import(&c, &st2, sizeof st2);
        for (t = 0; t < NSHOWN; t++)
            ok &= c.t[t].engine == GONE[t] && !memcmp(c.t[t].p, d.t[t].p, sizeof d.t[t].p) && !memcmp(c.t[t].step, d.t[t].step, sizeof d.t[t].step);
        bad += check("tracks on retired engines (1, 4): loaded as stored, nothing converted, the motion kept",
                     ok && c.motion.count == 2u && motion_valid(&c.motion) && proj_ok(&c));
    }

    /* every old format is refused (with a valid hash) and changes nothing */
    base_project(&a);
    a.sel = 0;
    a.sum = proj_sum(&a);
    memset(&c, 0x5A, sizeof c);
    d = c;
    ok = proj_pack(&st, &a);
    st2 = st; st2.raw[67] = 0; rehash(&st2);
    bad += check("OLD: a FUN11 of 0.2.13 and before (revision 0, valid hash): refused", ok && !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[67] = 1; rehash(&st2);
    bad += check("  a FUN11 of 0.2.14 / 0.2.15 (revision 1: the PING switch, the old MODE values; valid hash): refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[67] = 2; rehash(&st2);
    bad += check("  a FUN11 of 0.2.16 .. 0.2.18 (revision 2: one level, P_TRIM's id was the legacy DUCK HOLD; valid hash): refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[67] = 4; rehash(&st2);
    bad += check("  a revision above this firmware's: refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[66] = (uint8_t)(P_COUNT - 5u); rehash(&st2);
    bad += check("  a FUN11 of fewer parameters (122, 119, 113, 111 were mapped by count): refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[66] = (uint8_t)(P_COUNT + 1u); rehash(&st2);
    bad += check("  .. of more: refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[63] = 0; rehash(&st2);
    bad += check("  `parts` 0 (the GM drum part era): refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[64] = 0; rehash(&st2);
    bad += check("  `phys` 0 (PHYS with DUST): refused", !proj_import(&c, &st2, sizeof st2));
    {
        project_t f = a;
        f.motion.count = 2;
        f.motion.event[0] = (motion_event_t){0u << 6 | 1u, P_REV, 90};
        f.motion.event[1] = (motion_event_t){1u << 6 | 3u, P_E3, 50};
        f.sum = proj_sum(&f);
        bad += check("OLD: FUN10 (Felucca 1.2 .. 1.5, four tracks) as it wrote it: refused", old_pack10(old, &f, 111u) == 3840u &&
                     !proj_import(&c, old, 3840));
        bad += check("OLD: FUN10 of 104 parameters: refused", old_pack10(old, &f, 104u) == 3840u && !proj_import(&c, old, 3840));
        bad += check("OLD: FUN9 (1.1, 3648 bytes): refused", old_pack(old, &f, OLD_FUN9, 99u) == 3648u && !proj_import(&c, old, 3648));
        bad += check("OLD: FUN8 (1.0.x, 3584 bytes): refused", old_pack(old, &f, OLD_FUN8, 91u) == 3584u && !proj_import(&c, old, 3584));
        bad += check("OLD: FUN7 (3388 bytes): refused", old_pack(old, &f, OLD_FUN7, 91u) == 3388u && !proj_import(&c, old, 3388));
        memset(&slot, 0xA5, sizeof slot);
        memcpy(slot.raw, old, 3388);
        bad += check("OLD: a retained 3840-byte slot holding a FUN7 record: refused", !proj_import(&c, &slot, sizeof slot));
        memset(&slot, 0xA5, sizeof slot);
        bad += check("a slot of garbage is no project", !proj_import(&c, &slot, sizeof slot));
        memset(&slot, 0, sizeof slot);
        bad += check("a slot of zeros is no project", !proj_import(&c, &slot, sizeof slot));
    }
    bad += check("refused imports change nothing in the project they were given", !memcmp(&c, &d, sizeof c));
    bad += check("the in-RAM struct form (the old retained slot) is no stored project", !proj_import(&c, &a, (int)sizeof a));

    printf("%s\n", bad ? "PROJECT FORMAT TEST FAILED" : "project format test passed");
    return bad != 0;
}
