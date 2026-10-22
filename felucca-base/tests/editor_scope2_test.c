/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL) */
/* The editor protocol's scope 2 (cloud job I, web/EDITOR_PROTOCOL.md "Scope 2"): an extension slot (id 0 .. NX-1, ext.h XV) through GET, SET, DESC, the CHANGED push and
 * TRACK_PARAM's scope byte, with the C paths of the editor build (editor_test.c is the base: hostsim, stubs, RAM flash).
 *   DESC               every slot's descriptor (label, format, range, default, the enum's names) equals the table's; CYCLE RISE / FALL are the note-value ones while SYNC is on
 *   GET / SET          a round trip, the value clamped to the descriptor, a spare slot (X15) and ids past NX unchanged / refused; SET LINK runs twin_link_set (CUTOFF 2 converted)
 *   CHANGED            a device-side change goes out as scope 2 (coalesced like scope 0: 20 ms, the latest value), the editor's own SET and a PRESET do not echo
 *   TRACK_PARAM        the scope byte: scope 2 reaches that track's slot, scope 0 and the old forms are as before, a bad scope byte or id gets no reply
 *   TWIN TYPE          SET scope 0 id P_TWTYPE is twin_type_set (COMB refused on WAVE and PHYS)
 *   DUMP / UP_GET      still 127 + 27 values (the librarian uses PATCH_RAW) */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"

/* 0.2.31: reset() leaves the engine's first factory sound on both parts, which now runs filter 2, the SUB layer and a CYCLE time of its own; the scope 2 tests read the slots' defaults */
static void reset_plain(void)
{
    uint32_t t, k;
    reset();
    for (t = 0; t < NTRK; t++) {
        for (k = 0; k < NX; k++)
            XV(&trk[t], k) = xdesc(&trk[t], k)->def;
        trk[t].p[P_TWTYPE] = 0;
    }
}
#define reset reset_plain

static int rd(uint32_t scope, uint32_t id, int32_t *v)    /* GET: the reply's value, 0 when there is none */
{
    uint8_t a[2] = {(uint8_t)scope, (uint8_t)id};
    uint32_t n = request(ED_GET, a, 2);
    if (n < 10u || host_wire[5] != scope || host_wire[6] != id)
        return 0;
    *v = ed_rv(host_wire + 7);
    return 1;
}
static int wr(uint32_t scope, uint32_t id, int32_t v, int32_t *got)   /* SET: the reply's (clamped) value */
{
    uint8_t a[4] = {(uint8_t)scope, (uint8_t)id, (uint8_t)((v + 8192) & 127), (uint8_t)((v + 8192) >> 7)};
    uint32_t n = request(ED_SET, a, 4);
    if (n < 10u || host_wire[5] != scope || host_wire[6] != id)
        return 0;
    *got = ed_rv(host_wire + 7);
    return 1;
}

/* the CHANGED frames of up to four main-loop passes (a push moves the round robin on, so the next change can wait one pass), the last one's args in *args */
static uint32_t sync_frames(const uint8_t **args)
{
    static uint8_t last[4];
    uint32_t i, n = 0;
    for (i = 0; i < 4u; i++) {
        const uint8_t *x = 0;
        sync_pass();
        if (wire_frames(ED_CHANGED, &x)) { memcpy(last, x, 4); if (args) *args = last; }
        n += wire_frames(ED_CHANGED, 0);
    }
    return n;
}

static int scope2(void)
{
    int bad = 0, ok;
    uint32_t k, i;
    int32_t v = 0;
    const uint8_t *x = 0;
    uint8_t a[8];
    reset();

    /* DESC */
    ok = 1;
    for (k = 0; k < NX; k++) {
        const param_desc_t *d = xdesc(TSEL, k);
        uint32_t o = 5u;
        a[0] = 2; a[1] = (uint8_t)k;
        request(ED_DESC, a, 2);
        ok &= host_wire[5] == 2 && host_wire[6] == k && host_wire[7] == d->fmt;
        ok &= ed_rv(host_wire + 8) == d->min && ed_rv(host_wire + 10) == d->max && ed_rv(host_wire + 12) == d->def;
        o = 14u;
        ok &= !strcmp((const char *)host_wire + o, d->label);
        o += strlen(d->label) + 1u;
        ok &= !strcmp((const char *)host_wire + o, d->unit ? d->unit : "");
        o += (d->unit ? strlen(d->unit) : 0u) + 1u;
        if (d->fmt == F_ENUM && d->names)
            for (i = 0; i <= (uint32_t)(d->max - d->min); i++) {
                ok &= !strcmp((const char *)host_wire + o, d->names[d->min + (int32_t)i]);
                o += strlen(d->names[d->min + (int32_t)i]) + 1u;
            }
    }
    bad += check("DESC scope 2: all NX slots, label, format, range, default, unit and the enum's names equal the table's", ok);
    a[0] = 2; a[1] = NX;
    ok = request(ED_DESC, a, 2) == 0u;
    a[0] = 3; a[1] = 0;
    ok &= request(ED_DESC, a, 2) == 0u;
    a[0] = 2; a[1] = 0;
    ok &= request(ED_GET, a, 2) > 0u;
    a[1] = NX;
    ok &= request(ED_GET, a, 2) == 0u;
    bad += check("scope 2: an id past NX-1 and a scope past 2 get no reply (DESC, GET)", ok);
    XV(TSEL, X_CYSYNC) = 1;
    a[0] = 2; a[1] = X_CYRISE;
    request(ED_DESC, a, 2);
    ok = host_wire[7] == F_INT && !strcmp((const char *)host_wire + 14, "RISE") && ed_rv(host_wire + 8) == 0 && ed_rv(host_wire + 10) == 127;
    XV(TSEL, X_CYSYNC) = 0;
    bad += check("DESC scope 2: CYCLE RISE with SYNC on is the note-value descriptor (F_INT, 0 .. 127)", ok);

    /* GET / SET */
    ok = rd(2, X_CYRISE, &v) && v == xdesc(TSEL, X_CYRISE)->def && rd(2, X_LINK, &v) && v == 1;
    bad += check("GET scope 2: the defaults (CYCLE RISE 10, TWIN LINK ON)", ok);
    ok = wr(2, X_CYFALL, 55, &v) && v == 55 && XV(TSEL, X_CYFALL) == 55 && rd(2, X_CYFALL, &v) && v == 55;
    ok &= wr(2, X_SUBLVL, 500, &v) && v == 127 && XV(TSEL, X_SUBLVL) == 127;
    ok &= wr(2, X_ROUTE, 9, &v) && v == 1 && XV(TSEL, X_ROUTE) == 1;
    ok &= wr(2, X_KTRK2, -100, &v) && v == -63 && XV(TSEL, X_KTRK2) == -63;
    ok &= wr(2, X_CYCURVE, 63, &v) && v == 63 && wr(2, X_CYCURVE, -64, &v) && v == -64;
    ok &= wr(2, X_SUBOCT, -3, &v) && v == 0 && XV(TSEL, X_SUBOCT) == 0;
    ok &= wr(2, NX - 1u, 77, &v) && v == XV(TSEL, NX - 1u) && v == 0;
    bad += check("SET scope 2: stored and echoed, clamped to the descriptor (127, 1, -63, -64 .. 63, 0), the spare slot X15 stays", ok);
    ok = 1;
    for (k = 0; k < P_COUNT; k++) {
        a[0] = 0; a[1] = (uint8_t)k;
        ok &= request(ED_GET, a, 2) > 0u;
    }
    ok &= TSEL->p[127] == 0;
    bad += check("scope 2 does not touch scope 0: index 127 (the reserved empty entry) stays 0", ok);

    /* LINK and TYPE: the knobs' setters */
    reset();
    TSEL->p[P_TWTYPE] = TT_LP; TSEL->p[P_FREEA] = 30; TSEL->p[P_CUT2] = 5; XV(TSEL, X_LINK) = 1;
    ok = wr(2, X_LINK, 0, &v) && v == 0 && XV(TSEL, X_LINK) == 0 && TSEL->p[P_CUT2] == 35;
    ok &= wr(2, X_LINK, 1, &v) && v == 1 && TSEL->p[P_CUT2] == 5;
    bad += check("SET LINK scope 2 is twin_link_set: CUTOFF 2 5 (offset) <-> 35 (30 + 5) with filter 1's CUTOFF 30", ok);
    TSEL->eng_req = ENGI_WAVE; TSEL->p[P_TWTYPE] = TT_LP;
    ok = wr(0, P_TWTYPE, TT_COMB, &v) && v == TT_LP && TSEL->p[P_TWTYPE] == TT_LP;
    TSEL->eng_req = 0;
    ok &= wr(0, P_TWTYPE, TT_COMB, &v) && v == TT_COMB && TSEL->p[P_CUT2] == 0 && TSEL->p[P_RES2] == 0;
    bad += check("SET P_TWTYPE is twin_type_set: COMB refused on WAVE (the reply says LP), taken on ANALOG with CUTOFF 2 / RES 2 reset", ok);

    /* the CHANGED push */
    reset();
    usb.resets = 0;
    a[0] = 3; request(ED_WATCH, a, 1);
    sync_pass();
    XV(TSEL, X_CYFALL) = 90;
    sync_pass();
    ok = wire_frames(ED_CHANGED, &x) == 1u && x[0] == 2 && x[1] == X_CYFALL && ed_rv(x + 2) == 90 && !sync_pass();
    bad += check("CHANGED push: a device-side change of an extension slot goes out as scope 2, once", ok);
    XV(TSEL, X_SUBLVL) = 10; XV(TSEL, X_SUBHARM) = 20;
    ok = sync_frames(0) == 2u;
    bad += check("CHANGED push: two slots, two frames", ok);
    wr(2, X_CYRISE, 44, &v);
    ok = !sync_frames(0);
    bad += check("CHANGED push: the editor's own SET of a slot is not echoed", ok);
    XV(TSEL, X_CYCURVE) = 5;
    sync_pass();                                             /* pushed: the slot's 20 ms start */
    XV(TSEL, X_CYCURVE) = 6;
    fm1_ms += 6u; host_wire_n = 0; ed_sync(); host_drain();  /* 6 ms later: too soon, nothing goes out */
    ok = !host_wire_n;
    XV(TSEL, X_CYCURVE) = 7;
    sync_pass();
    ok &= wire_frames(ED_CHANGED, &x) == 1u && x[0] == 2 && x[1] == X_CYCURVE && ed_rv(x + 2) == 7 && !sync_pass();
    bad += check("CHANGED push: coalesced like scope 0 (not within 20 ms of the slot's last push), the latest value goes out", ok);
    a[0] = 0; a[1] = 2;
    request(ED_PRESET, a, 2);
    ok = !sync_frames(0) && !wire_frames(ED_RELOAD, 0);
    bad += check("the editor's own PRESET: no push of the slots it loaded", ok);
    XV(TSEL, X_LINK) = XV(TSEL, X_LINK) ? 0 : 1; XV(TSEL, X_ROUTE) = XV(TSEL, X_ROUTE) ? 0 : 1;   /* (both flipped: factory 2 is the owner's BEHIVE since 0.2.31, which already has LINK OFF and ROUTE PARAL) */
    ok = sync_frames(0) == 2u;
    bad += check("CHANGED push: after a PRESET the device's own changes of slots are pushed again", ok);
    a[0] = 0; request(ED_WATCH, a, 1);

    /* TRACK_PARAM */
    reset();
    {
        uint8_t t[5];
        uint32_t n;
        t[0] = 1; t[1] = X_SUBLVL; t[2] = (uint8_t)((40 + 8192) & 127); t[3] = (uint8_t)((40 + 8192) >> 7); t[4] = 2;
        n = request(ED_TRACK_PARAM, t, 5);
        ok = n >= 11u && host_wire[5] == 1 && host_wire[6] == X_SUBLVL && ed_rv(host_wire + 7) == 40 && host_wire[9] == 2 &&
             XV(&trk[1], X_SUBLVL) == 40 && XV(&trk[0], X_SUBLVL) == 0;
        t[2] = (uint8_t)((999 + 8192) & 127); t[3] = (uint8_t)((999 + 8192) >> 7); t[1] = X_CYFALL;
        request(ED_TRACK_PARAM, t, 5);
        ok &= XV(&trk[1], X_CYFALL) == 127;
        { const uint8_t r[3] = {1, X_CYFALL, 2}; n = request(ED_TRACK_PARAM, r, 3); }   /* a read: track, id, scope */
        ok &= n >= 11u && host_wire[9] == 2 && ed_rv(host_wire + 7) == 127;
        bad += check("TRACK_PARAM with scope 2: that track's slot (part 2 while part 1 is selected), clamped, the reply carries the scope", ok);
        { const uint8_t r1[3] = {1, NX, 2}, r2[3] = {1, 0, 1}, r3[3] = {1, 0, 3}, r4[3] = {NTRK, 0, 2};
          ok = request(ED_TRACK_PARAM, r1, 3) == 0u;
          ok &= request(ED_TRACK_PARAM, r2, 3) == 0u;
          ok &= request(ED_TRACK_PARAM, r3, 3) == 0u;
          ok &= request(ED_TRACK_PARAM, r4, 3) == 0u;
        }
        bad += check("TRACK_PARAM: an id past NX-1, a scope of 1 or 3 and a track past the last get no reply", ok);
        t[0] = 0; t[1] = P_PAN; t[2] = (uint8_t)((12 + 8192) & 127); t[3] = (uint8_t)((12 + 8192) >> 7);
        n = request(ED_TRACK_PARAM, t, 4);
        ok = n == 10u && trk[0].p[P_PAN] == 12 && host_wire_n == 5u + 2u + 2u + 1u;
        t[4] = 0;
        n = request(ED_TRACK_PARAM, t, 5);
        ok &= n == 11u && trk[0].p[P_PAN] == 12 && host_wire[9] == 0;
        bad += check("TRACK_PARAM without a scope byte (and with scope 0): as before", ok);
    }

    /* DUMP, UP_GET */
    reset();
    request(ED_DUMP, 0, 0);
    ok = host_wire_n == 5u + 2u + 2u * (P_COUNT + G_COUNT) + 1u;
    bad += check("DUMP: still P_COUNT + G_COUNT values (the librarian uses PATCH_RAW for the extension block)", ok);
    return bad;
}

int main(void)
{
    int bad = scope2();
    printf("%s\n", bad ? "EDITOR SCOPE 2 TEST FAILED" : "editor scope 2 test passed");
    return bad != 0;
}
