/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the project format (firmware/src/project.c, -DPROJ_HOST part). DUELL 0.2.14 reads one format only: FUN11 ("FUNB",
 * 3840 bytes, parts A and B, P_COUNT parameters, revision 6: memory trim 1 (two parts, no song chain block) and the cloud plan's job A (per part the NX extension slots and the hand-set B edit list after the values)). What it checks:
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

    bad += check("layout: name at 3824, A B's patches at 3568, 1666 spare before them (no edits), P_COUNT 127 at byte 66, revision 7, two parts, no song chain",
                 PROJ_NAME_OFF == 3824u && PROJ_FM6_OFF == 3568u && PROJ_NTRK == 2u && P_COUNT == 127u && PROJ_REV == 7u && NPART == 2u &&
                 68u + PROJ_NTRK * (P_COUNT + 1u + NX + 1u + 2u + NSTEP * 9u) + sizeof(motion_store_t) + 1666u == PROJ_FM6_OFF);

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
    bad += check("  two parts exist and are stored (NPART 2: no hidden tracks 3..4, no song chain)", NTRK == 2u && sizeof c.t == 2u * sizeof c.t[0] && st.raw[63] == 2u);
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
    st2 = st; st2.raw[68u + P_COUNT + 2u + NX + PROJ_MB_N] = NENGINES; rehash(&st2);
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

    /* PROJ_REV 6: per part the NX extension slots (dense) and the hand-set B edit list, after the values */
    {
        project_t e, c0;
        uint32_t at = 68u + P_COUNT, ins = at + 1u + NX, j;       /* part A: the byte nx, the NX slots, then the byte ne */
        base_project(&e);
        for (j = 0; j < NX; j++) { e.t[0].x[j] = (int16_t)(j * 7u) - 50; e.t[1].x[j] = (int16_t)(127 - 3 * (int)j); }
        e.t[0].x[3] = -64; e.t[0].x[4] = 127;
        e.xe_n[0] = XB_MAX; e.xe_n[1] = 3;
        for (j = 0; j < XB_MAX; j++) { e.xe[0][j].wid = (uint8_t)(j < 31u ? j * 4u : 128u + 5u); e.xe[0][j].val = (int16_t)(j * 5) - 64; }
        e.xe[1][0] = (xb_edit_t){2, 127}; e.xe[1][1] = (xb_edit_t){126, -64}; e.xe[1][2] = (xb_edit_t){255, 0};
        for (j = 0; j < PROJ_MB_N; j++) { e.mb[0][j] = (uint8_t)(0x4Du + j * 3u); e.mb[1][j] = (uint8_t)(j ? 0x11u * j : 0x4Du); }   /* (PROJ_REV 7: End B's block, any 7 bytes: the morph checks them when it uses them) */
        e.sum = proj_sum(&e);
        ok = proj_pack(&st, &e) && proj_import(&c0, &st, sizeof st);
        bad += check("rev 5: the extension slots and 32 + 3 hand-set B edits round trip (both parts)", ok && !memcmp(c0.t[0].x, e.t[0].x, sizeof e.t[0].x) &&
                     !memcmp(c0.t[1].x, e.t[1].x, sizeof e.t[1].x) && c0.xe_n[0] == XB_MAX && c0.xe_n[1] == 3u &&
                     !memcmp(c0.xe[0], e.xe[0], sizeof e.xe[0]) && !memcmp(c0.xe[1], e.xe[1], 3u * sizeof e.xe[1][0]) && proj_ok(&c0));
        bad += check("  imported and packed again: the same bytes", proj_pack(&st2, &c0) && !memcmp(&st, &st2, sizeof st));
        bad += check("  nx on the wire is NX, ne is the list's length", st.raw[at] == NX && st.raw[ins] == XB_MAX);
        bad += check("rev 7: End B's 7-byte block of each part round trips and sits right after the part's edit list", !memcmp(c0.mb, e.mb, sizeof e.mb) &&
                     !memcmp(st.raw + ins + 1u + 2u * XB_MAX, e.mb[0], PROJ_MB_N) && PROJ_MB_N == 7u);
        base_project(&e);
        ok = proj_pack(&st, &e) && proj_import(&c0, &st, sizeof st);
        for (j = 0; j < NX; j++) ok &= c0.t[0].x[j] == e.t[0].x[j];
        bad += check("  no slot set, no edit: round trips (the base project's slots), ne 0", ok && !c0.xe_n[0] && !c0.xe_n[1] && st.raw[ins] == 0u);
        /* a newer firmware's longer list: the extra bytes are ignored, everything after them is read from where it moved to */
        st2 = st;
        memmove(st2.raw + ins + 4u, st2.raw + ins, PROJ_FM6_OFF - ins - 4u);
        st2.raw[at] = NX + 4u;
        memset(st2.raw + ins, 0x6B, 4);
        rehash(&st2);
        bad += check("a longer slot list (nx = NX + 4, a newer firmware): loads, the extra bytes ignored, the rest as it was",
                     proj_import(&c, &st2, sizeof st2) && !memcmp(&c, &c0, sizeof c));
        /* an older, shorter list: the slots it does not hold are their defaults */
        st2 = st;
        memmove(st2.raw + at + 1u + 10u, st2.raw + ins, PROJ_FM6_OFF - ins);
        st2.raw[at] = 10u;
        for (j = 0; j < NX; j++) st2.raw[at + 1u + j] = (uint8_t)(j < 10u ? 64 + 20 + j : 0xEE);   /* (bytes past nx are the next fields now: overwritten below) */
        memcpy(st2.raw + at + 1u + 10u, st.raw + ins, PROJ_FM6_OFF - ins);
        memset(st2.raw + PROJ_FM6_OFF - 6u, 0, 6);
        rehash(&st2);
        ok = proj_import(&c, &st2, sizeof st2);
        for (j = 0; j < NX; j++) ok &= c.t[0].x[j] == (j < 10u ? (int16_t)(20 + j) : (int16_t)xb_def(j) - 64);
        bad += check("a shorter slot list (nx = 10, an older firmware): loads, the other six are their defaults", ok && c.t[1].x[15] == c0.t[1].x[15]);
        st2 = st; st2.raw[ins] = XB_MAX + 1u; rehash(&st2);
        bad += check("a list of 33 edits (hash right): refused", !proj_import(&c, &st2, sizeof st2));
        e.xe_n[0] = 2; e.xe[0][0] = (xb_edit_t){9, 3}; e.xe[0][1] = (xb_edit_t){9, 4};
        e.sum = proj_sum(&e);
        ok = proj_pack(&st2, &e);
        st2.raw[ins] = 2; st2.raw[ins + 1] = 9; st2.raw[ins + 2] = 67; st2.raw[ins + 3] = 9; st2.raw[ins + 4] = 68; rehash(&st2);
        bad += check("an edit id twice (hash right): refused", !proj_import(&c, &st2, sizeof st2));
        st2.raw[ins + 3] = 127; rehash(&st2);
        bad += check("wide id 127 in an edit (hash right): refused", !proj_import(&c, &st2, sizeof st2));
        st2.raw[ins + 3] = 10; st2.raw[ins + 4] = 192; rehash(&st2);
        bad += check("an edit value byte above 191 (hash right): refused", !proj_import(&c, &st2, sizeof st2));
        st2 = st; st2.raw[at + 1u + 5u] = 192; rehash(&st2);
        bad += check("a slot value byte above 191 (hash right): refused", !proj_import(&c, &st2, sizeof st2));
        e.xe_n[0] = XB_MAX + 1u;
        bad += check("33 edits cannot be packed", !proj_pack(&st2, &e));
        e.xe_n[0] = 1; e.xe[0][0] = (xb_edit_t){127, 0};
        bad += check("an edit with wide id 127 cannot be packed", !proj_pack(&st2, &e));
        e.xe[0][0] = (xb_edit_t){9, 0}; e.t[0].x[2] = 128;
        bad += check("a slot value outside -64..127 cannot be packed", !proj_pack(&st2, &e));
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
    st2 = st; st2.raw[67] = 3; rehash(&st2);
    bad += check("  a FUN11 of 0.2.19 .. 0.2.21 (revision 3: DUCK SRC 2..17 were the MIDI channels; valid hash): refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[67] = 4; rehash(&st2);
    bad += check("  a FUN11 of 0.2.22 .. 0.2.25 (revision 4: four parts, the song chain block, no extension slots; valid hash): refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[67] = 5; rehash(&st2);
    bad += check("  a FUN11 of revision 5 (either memory trim 1's two-part format or job A's slots alone; valid hash): refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[67] = 6; rehash(&st2);
    bad += check("  a FUN11 of revision 6 (job A's lists, no End B block; valid hash): refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[67] = 8; rehash(&st2);
    bad += check("  a revision above this firmware's: refused", !proj_import(&c, &st2, sizeof st2));
    st2 = st; st2.raw[63] = 4; rehash(&st2);
    bad += check("  `parts` 4 (the hidden parts 3 and 4 of 0.2.25 and before; valid hash): refused", !proj_import(&c, &st2, sizeof st2));
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
