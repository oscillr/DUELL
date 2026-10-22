/* SPDX-License-Identifier: GPL-3.0-only */
/* Cloud job B (the values): two scenarios written against the sources as they were on cloud/base3 (only what they had), so the same function gives the same number there and here. The
 * constants in tests/xvalues_test.c were made by building this file against cloud/base3 (felucca-base/tests/xvalues_ref_main.c-style driver: hostsim.c, mode_scn.h, this file, a main that
 * prints the two numbers). Needs hostsim.c (and mode_scn.h's scn_eng) included first.
 *   xv_roll_hash    the End B of every id but the five the TWIN FILTER took over (25 26 43 101 106): all live engines, 24 seeds, AMOUNT 10 / 40 / 100, SMART and WILD, with and without PITCH and ARP,
 *                   both parts' streams, STEREO on and off, an Original with random values in every range (the matrix slots with sources below 15 and destinations below 51); a stored seed
 *                   (a patch's saved End B) rolls this End B: it must not move
 *   xv_matrix_hash  a busy two-part STEREO render with the matrix on: every per-note destination (PITCH CUT SHP AMP) from ENV, ENV 2, VEL, KEY, RAND, the LFOs; PAN, the sends, an engine slot,
 *                   RES, FMODE per block; the second list empty (no CUTOFF 2, no CYCLE): mod_voice must run as it always did */
static uint64_t xv_fnv(uint64_t h, uint32_t x)
{
    h ^= x;
    return h * 0x100000001B3ull;
}
static uint32_t xv_lcg(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s >> 8;
}
static int xv_five(uint32_t id) { return id == 25u || id == 26u || id == 43u || id == 101u || id == 106u; }
static uint64_t xv_roll_hash(void)
{
    static const uint8_t CFG[6] = {0, MC_WILD, MC_ARP | MC_PIT, MC_WILD | MC_ARP | MC_PIT, MC_ARP, MC_PIT};
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t e, seed, ai, ci, id, part, st;
    static const uint8_t AMT[3] = {10, 40, 100};
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        int8_t o[P_COUNT], end[P_COUNT];
        if (ENGI_RETIRED(e))
            continue;
        for (seed = 1; seed <= 24u; seed++) {
            uint32_t rs = seed * 7919u + e * 104729u;
            for (id = 0; id < P_COUNT; id++) {               /* an Original: the default, or a random value in the range (every third) */
                const param_desc_t *d = id >= P_E0 ? &en->edit[id - P_E0] : &TP[id];
                int32_t v = d->def, lo = d->min, hi = d->max;
                if (xv_five(id)) {
                    o[id] = 0;
                    continue;
                }
                if (en == &ENG_WAVE && id == P_E6)
                    lo = hi = 0;                               /* (WAVE's E7 as it was when the hash was made, an unused slot: STEPS has a range now; a roll draws it as it did, morph.c MORPH_E7_OLD) */
                if (id >= P_M1SRC && id <= P_M8AMT) {
                    uint32_t k = (id - P_M1SRC) % 3u;
                    lo = 0;
                    hi = k == 0 ? 14 : k == 1 ? 50 : 63;      /* sources below 15, destinations below 51: what cloud/base3 had */
                    if (k == 2)
                        lo = -64;
                }
                if (hi > lo && xv_lcg(&rs) % 3u == 0)
                    v = lo + (int32_t)(xv_lcg(&rs) % (uint32_t)(hi - lo + 1));
                else
                    (void)xv_lcg(&rs);
                o[id] = (int8_t)(v < -128 ? -128 : v > 127 ? 127 : v);
            }
            for (ai = 0; ai < 3u; ai++)
                for (ci = 0; ci < 6u; ci++)
                    for (part = 0; part < 2u; part++)
                        for (st = 0; st < 2u; st++) {
                            uint8_t sv = mrc;
                            mrc = (uint8_t)(CFG[ci] | (st ? MC_STEREO : 0u));
                            morph_roll_pure(o, end, en, AMT[ai], seed, part, st);
                            mrc = sv;
                            for (id = 0; id < P_COUNT; id++)
                                if (!xv_five(id))
                                    h = xv_fnv(h, (uint32_t)(uint8_t)end[id]);
                        }
        }
    }
    return h;
}
static uint64_t xv_matrix_hash(void)
{
    static const uint8_t C1[4] = {48, 55, 60, 64}, C2[3] = {67, 72, 76};
    static int32_t o[2 * CTL];
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t k, i, b, nb = 2u * FS / CTL;
    host_tracks_init();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    host_preset(&trk[1], scn_eng("PHASE"), 2);
    song.g[G_RTYPE] = 0;
    song.g[G_DMIX] = 100;
    for (k = 0; k < 2u; k++) {
        trk[k].p[P_VOICE] = V_POLY;
        trk[k].p[P_TRIM] = 120;
        trk[k].p[P_CHOR] = 60;
        trk[k].p[P_DLY] = 50;
        trk[k].p[P_REV] = 70;
        trk[k].p[P_SUS] = 100;
        trk[k].p[P_REL] = 60;
        trk[k].p[P_E2FLT] = 20;
        trk[k].p[P_L2FLT] = 10;
        trk[k].p[P_LD_FLT] = 12;
    }
    /* part 1: per note (ENV -> CUT, ENV 2 -> SHP, VEL -> AMP, KEY -> PITCH, RAND -> CUT, LFO -> PITCH, LFO 2 -> AMP), part 2: per block and mixed */
    trk[0].p[P_M1SRC] = MS_ENV;  trk[0].p[P_M1DST] = MD_CUT;   trk[0].p[P_M1AMT] = 30;
    trk[0].p[P_M2SRC] = MS_ENV2; trk[0].p[P_M2DST] = MD_SHP;   trk[0].p[P_M2AMT] = -25;
    trk[0].p[P_M3SRC] = MS_VEL;  trk[0].p[P_M3DST] = MD_AMP;   trk[0].p[P_M3AMT] = 20;
    trk[0].p[P_M4SRC] = MS_KEY;  trk[0].p[P_M4DST] = MD_PITCH; trk[0].p[P_M4AMT] = 6;
    trk[0].p[P_M5SRC] = MS_RAND; trk[0].p[P_M5DST] = MD_CUT;   trk[0].p[P_M5AMT] = 14;
    trk[0].p[P_M6SRC] = MS_LFO;  trk[0].p[P_M6DST] = MD_PITCH; trk[0].p[P_M6AMT] = 3;
    trk[0].p[P_M7SRC] = MS_LFO2; trk[0].p[P_M7DST] = MD_AMP;   trk[0].p[P_M7AMT] = 10;
    trk[0].p[P_M8SRC] = MS_ENV;  trk[0].p[P_M8DST] = MD_PAN;   trk[0].p[P_M8AMT] = 25;
    trk[1].p[P_M1SRC] = MS_LFO;  trk[1].p[P_M1DST] = MD_E1 + 4; trk[1].p[P_M1AMT] = 20;
    trk[1].p[P_M2SRC] = MS_ENV;  trk[1].p[P_M2DST] = MD_RES;   trk[1].p[P_M2AMT] = 30;
    trk[1].p[P_M3SRC] = MS_VEL;  trk[1].p[P_M3DST] = MD_REV;   trk[1].p[P_M3AMT] = 40;
    trk[1].p[P_M4SRC] = MS_LFO2; trk[1].p[P_M4DST] = MD_FMODE; trk[1].p[P_M4AMT] = 20;
    trk[1].p[P_M5SRC] = MS_KEY;  trk[1].p[P_M5DST] = MD_DLY;   trk[1].p[P_M5AMT] = 15;
    trk[1].p[P_M6SRC] = MS_ENV2; trk[1].p[P_M6DST] = MD_CUT;   trk[1].p[P_M6AMT] = 22;
    for (b = 0; b < nb; b++) {
        if (b == 6u) {
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], C1[i], 60u + 20u * i);
            for (i = 0; i < 3u; i++)
                trk_note_on(&trk[1], C2[i], 70u + 15u * i);
        }
        if (b == nb * 2u / 3u) {
            for (i = 0; i < 4u; i++)
                trk_note_off(&trk[0], C1[i]);
            for (i = 0; i < 3u; i++)
                trk_note_off(&trk[1], C2[i]);
        }
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            h = xv_fnv(h, (uint32_t)o[i]);
    }
    return h;
}
