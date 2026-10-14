/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Robustness against damaged or crafted stored data and editor requests (the 1.0.4 audit's items 5-10): the real
 * C paths of the editor build (editor_test.c is the base: hostsim, stubs, RAM flash).
 *   (user sample slots: gone with the sample engines in DUELL 0.2.9; their crafted-slot checks went with them)
 *   projects            an engine number past the last refused (every format, and a RAM restore); a retained
 *                       older-format RAM slot bounded as one from flash
 *   user presets        a bank from flash or a backup: each record's pattern inside its fields
 *   editor              SMP_END (unsupported since 0.2.9) / a malformed BACKUP_PUT does not stop the transport */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"

static int projects(void)
{
    static project_t p, q;
    static project_store_t st;
    static project_v5_t v5;
    int bad = 0;
    uint32_t i, pos, sum, e0;
    reset();
    project_capture(&p);
    bad += check("a captured project packs and imports", proj_pack(&st, &p) && proj_import(&q, &st, PROJ_STORE_SIZE));
    pos = 68u + st.raw[66];                                 /* track 1's engine byte (after its parameters) */
    for (e0 = NENGINES; e0 < 256u; e0 += 37u) {
        st.raw[pos] = (uint8_t)e0;
        sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u);
        memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
        if (proj_import(&q, &st, PROJ_STORE_SIZE)) break;
    }
    bad += check("FUN8 with an engine past the last (a valid hash) is not a project", e0 >= 256u);
    memset(&v5, 0, sizeof v5);                              /* FUN5 the same */
    v5.magic = PROJ_MAGIC_V5; v5.size = sizeof v5;
    for (i = 0; i < G_COUNT; i++) v5.g[i] = GP[i].def;
    for (i = 0; i < NTRK; i++) { v5.t[i].engine = trk[i].engine; memcpy(v5.t[i].p, trk[i].p, sizeof v5.t[i].p); }
    v5.t[2].engine = NENGINES;
    v5.sum = proj_hash(&v5, sizeof v5 - 4u);
    bad += check("FUN5 with an engine past the last is not a project", !proj_import(&q, &v5, sizeof v5));
    p.t[1].engine = 200; p.sum = proj_sum(&p);              /* a RAM project_t (valid magic, size and sum) */
    e0 = trk[1].eng_req;
    bad += check("restoring a project with an engine past the last is refused, nothing changes",
                 project_restore_runtime(&p) == 1 && trk[1].eng_req == e0);
    /* a retained RAM slot holding FUN5 (its own size): bounded as one from flash (vel 200 cannot be packed) */
    reset();
    v5.t[2].engine = trk[2].engine;
    v5.t[0].step[0] = (step10_t){{60}, 1, ST_NOTE, 0, 200, 0, 0};
    v5.sum = proj_hash(&v5, sizeof v5 - 4u);
    memset(&proj_slot[2], 0, sizeof proj_slot[2]);
    memcpy(&proj_slot[2], &v5, sizeof v5);
    project_load(2);
    bad += check("a retained FUN5 RAM slot loads bounded (velocity 200 -> inside 0..127)",
                 trk[0].step[0].n == 1u && trk[0].step[0].note[0] == 60u && trk[0].step[0].vel <= 127u);
    return bad;
}

static int user_presets(void)
{
    int bad = 0;
    up_bank_t *bk = &up_bank[0];
    up_rec_t *r;
    reset();
    memset(bk, 0, sizeof *bk);
    bk->magic = UP_BANK_MAGIC; bk->rsize = sizeof(up_rec_t); bk->nslot = UP_PER_BANK;
    r = &bk->r[0];                                          /* a note pattern with a note > 127 and stray flags */
    r->used = UP_USED; r->ver = 2; r->engine = 0; r->np = 8; r->name[0] = 'A';
    r->note[3] = 200; r->flags[3] = 0x43;
    r->note[4] = 0; r->flags[4] = 0x03;                     /* a rest with flags */
    r = &bk->r[1];                                          /* a drum grid: accents where there is no hit */
    r->used = UP_USED; r->ver = UP_VER_GRID; r->engine = ENGI_DRUM; r->np = 8; r->name[0] = 'G';
    r->note[0] = 0x05; r->flags[0] = 0xFF;
    up_bank_check(0, sizeof *bk);
    bad += check("a bank from flash: a note pattern's note inside 0..127, its flags inside their fields",
                 up_used(0) && bk->r[0].note[3] == (200u & 127u) && bk->r[0].flags[3] == 3u && !bk->r[0].flags[4]);
    bad += check("a bank from flash: a drum grid's accents only on its hits", up_used(1) && bk->r[1].note[0] == 5u &&
                 bk->r[1].flags[0] == 5u);
    return bad;
}

static int stop_after_checks(void)
{
    int bad = 0;
    uint8_t a[16] = {0};
    reset();
    song.playing = 1; host_progress = 1;                    /* the transport runs and could be stopped */
    a[0] = 0; a[1] = 1; a[2] = 2;
    request(ED_SMP_END, a, 3);                              /* (no sample slots since 0.2.9: unsupported, rc 6) */
    bad += check("SMP_END is refused (unsupported) without stopping the transport", host_wire[6] == 6 && song.playing);
    a[0] = 0; a[1] = 2;
    request(ED_BACKUP_PUT, a, 2);                           /* BEGIN without its arguments */
    bad += check("a malformed BACKUP_PUT is refused without stopping the transport", host_wire[7] == 1 && song.playing);
    a[0] = 1; a[1] = 2;
    request(ED_BACKUP_PUT, a, 9);                           /* data with no session */
    bad += check("BACKUP_PUT data without a session: refused, the transport runs", host_wire[7] == 5 && song.playing);
    song.playing = 0;
    return bad;
}

int main(void)
{
    int bad = projects() + user_presets() + stop_after_checks();
    printf("%s\n", bad ? "ROBUSTNESS TEST FAILED" : "robustness test passed");
    return bad != 0;
}
