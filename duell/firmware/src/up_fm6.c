/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* A stored FM6 sound's patch (DUELL). Since the PATCH store (upreset.c) an FM6 sound carries its track's 128-byte patch
 * (eng_fm6.c fm6_pack, 7-bit bytes packed 8 to 7: UPF_PK = 112 bytes) INSIDE its own record (PTF_FM6): one write stores both,
 * so a record can never meet another record's patch (Felucca kept the patches in a separate object, tagged with a hash of
 * the record; that object, the retired FM6 patch bank of 1.0..1.0.2 and their migration are gone). A record without a patch
 * (the editor's UP_PUT sends none) loads as before 1.0.3: SLOT F n that factory patch, OWN the init voice.
 * The packing helpers (upf_pk_get / upf_pk_put) are in upreset.c, above the store. Included by upreset.c. */

static int upf_fm6(uint32_t k) { return up_used(k) && up_engine_raw(k) == ENGI_FM6; }

/* slot k's patch -> pk (FM6_PACKED bytes), 0 = it has one */
static int upf_get(uint32_t k, uint8_t *pk)
{
    const uint8_t *p = pt_raw(k);
    uint32_t sh, mb, pr, pat, fm;
    if (!p || p[5] != ENGI_FM6 || !(p[4] & PTF_FM6))
        return 1;
    pt_layout(p, &sh, &mb, &pr, &pat, &fm);
    upf_pk_get(p + fm, pk);
    return 0;
}

/* the editor (ED_FM6_PUT, a user preset): slot k's patch = pk (FM6_PACKED bytes; every value into its range), the rest of
 * the record as it is; up_put's result */
static int upf_put(uint32_t k, const uint8_t *pk)
{
    uint8_t v[FP_SIZE + 1u], c[FM6_PACKED];
    up_rec_t r;
    int rc;
    if (!upf_fm6(k))
        return 1;
    r = *up_rec(k);
    fm6_unpack(pk, v);
    fm6_pack(v, c);
    up_pk_in = c;
    up_ak_in = pt_ak_raw(k);                           /* (the arp / keys / pattern block stays) */
    rc = up_put(k, &r);
    up_pk_in = 0;
    up_ak_in = 0;
    return rc;
}

/* up_load: track t (its values just loaded from slot k) gets the record's patch; without one, as before 1.0.3:
 * SLOT F n that factory patch, OWN the init voice. SLOT then shows F n or OWN (fm6_adopt) */
static void upf_track_load(track_t *t, uint32_t k)
{
    uint8_t pk[FM6_PACKED], v[FP_SIZE + 1u];
    uint32_t tr = (uint32_t)(t - trk);
    if (tr >= NTRK || t->eng_req != ENGI_FM6)
        return;
    if (upf_get(k, pk)) {
        int32_t s = t->p[P_E7];
        memcpy(pk, s >= 0 && s < (int32_t)FM6_NFACTORY ? FM6_FACTORY[s] : FM6_INIT, FM6_PACKED);
    }
    fm6_unpack(pk, v);
    fm6_set_patch(tr, v);
    fm6_adopt(tr);
}
