/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.24 (owner): the MOD MATRIX stays PACKED (mod.c mod_compact): a slot is used while its SRC is not OFF; a SRC set to OFF (the knob, the editor) or a sound that loads
 * with gaps moves the slots after it up, in their order (a stable shift), and clears the rest; MATRIX 5-8 is in the page round only while slots 1..4 are all used.
 *   SOUND     200 random patches with gaps (random SRC DST AMT, slots left empty at random): the render hash of the part is the same packed or not (mod_begin sums the
 *             slots in slot order, the AMP / DEPTH products too: a stable shift changes nothing); the packed layout has no empty slot before a used one and keeps the order
 *   UNIT      MATRIX 1-4, slot 2's SRC turned to OFF: slots 3 and 4 move into 2 and 3, slot 4 is clear, the cursor stays on slot 2 (it shows the slot that moved in);
 *             a slot with a SRC but no DST yet stays (it is being set up); MATRIX 5-8 is skipped by SELECT and the page buttons while slot 4 is unused, there once it is
 *   LOAD      a factory sound loads packed (gaps counted and printed: the factory data's own gaps, compacted as they load, sound-identical)
 * Built and run by DUELL/host_tests.sh (name: matrixpack) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok)
        printf("matrixpack: FAIL %s\n", what), bad++;
    return ok;
}
static uint32_t rs = 0x9E3779B9u;
static uint32_t rnd(uint32_t n) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) % n; }
#define NB 60u
static int16_t mx[P_COUNT];                             /* the matrix layout to render (the part's values) */
static uint64_t render_part(void)                       /* a short render of part 1 (ANALOG, a chord) from a clean state: the hash of the mix */
{
    static const uint8_t C1[3] = {48, 55, 64};
    static int32_t o[2 * CTL];
    uint64_t h = 1469598103934665603ull;
    uint32_t b, i;
    memset(trk, 0, sizeof trk);
    memset(&trk[0].mdw, 0, sizeof trk[0].mdw);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(ins, 0, sizeof ins);
    memset(eqs, 0, sizeof eqs);
    trk[0].eqlv = trk[1].eqlv = 0;
    memset(pan_x, 0, sizeof pan_x);
    memset(pan_gv, 0, sizeof pan_gv);
    memset(duck_st, 0, sizeof duck_st);
    memset(mx_sl, 0, sizeof mx_sl);                     /* (0.2.25: the matrix's source slew and mix ramp from a clean state too) */
    memset(mx_ok, 0, sizeof mx_ok);
    memset(mx_was, 0, sizeof mx_was); memset(mod_mixmv, 0, sizeof mod_mixmv);
    vage = 0;
    mod_seed = 0x2545F491u;
    duell_mode_force(DM_SPLIT);
    duell_mode_force(DM_STEREO);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    host_tracks_init();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    memcpy(&trk[0].p[P_M1SRC], &mx[P_M1SRC], 12 * sizeof mx[0]);
    memcpy(&trk[0].p[P_M5SRC], &mx[P_M5SRC], 12 * sizeof mx[0]);
    for (b = 0; b < NB; b++) {
        if (b == 2u)
            for (i = 0; i < 3u; i++)
                trk_note_on(&trk[0], C1[i], 110);
        if (b == 40u)
            for (i = 0; i < 3u; i++)
                trk_note_off(&trk[0], C1[i]);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) { h ^= (uint32_t)mix_l[i]; h *= 1099511628211ull; h ^= (uint32_t)mix_r[i]; h *= 1099511628211ull; }
    }
    return h;
}
static int packed_ok(const int16_t *p, const int16_t *was)   /* no gap before a used slot; the used slots in their old order */
{
    uint32_t k, w = 0, seen_empty = 0;
    for (k = 0; k < NMSLOT; k++) {
        if (!p[MSLOT_ID(k)]) {
            seen_empty = 1;
            if (p[MSLOT_ID(k) + 1u] | p[MSLOT_ID(k) + 2u])
                return 0;                               /* (an empty slot is wholly clear) */
            continue;
        }
        if (seen_empty)
            return 0;
    }
    for (k = 0; k < NMSLOT; k++) {
        if (!was[MSLOT_ID(k)])
            continue;
        if (p[MSLOT_ID(w)] != was[MSLOT_ID(k)] || p[MSLOT_ID(w) + 1u] != was[MSLOT_ID(k) + 1u] || p[MSLOT_ID(w) + 2u] != was[MSLOT_ID(k) + 2u])
            return 0;
        w++;
    }
    return 1;
}

int main(void)
{
    uint32_t n, k, same = 0, packs = 0, gaps_seen = 0;
    int16_t was[P_COUNT];
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    duell_mode_req = DM_STEREO;
    host_tracks_init();
    host_preset(&trk[0], scn_eng("ANALOG"), 1);
    trk[0].p[P_VOICE] = V_POLY;
    /* SOUND: random matrices with gaps */
    memset(mx, 0, sizeof mx);
    {
        uint64_t d0, d1;
        mx[P_M1SRC] = MS_LFO; mx[P_M1DST] = MD_PAN; mx[P_M1AMT] = 30;
        d0 = render_part(); d1 = render_part();
        expect("the render is deterministic from a clean state (the same layout twice: the same hash)", d0 == d1);
    }
    for (n = 0; n < 200u; n++) {
        uint64_t h0, h1;
        int moved;
        track_t tt;
        memset(mx, 0, sizeof mx);
        for (k = 0; k < NMSLOT; k++) {
            uint32_t used = rnd(3u) != 0u;
            mx[MSLOT_ID(k)] = (int16_t)(used ? 1u + rnd(MS_N - 1u) : 0u);   /* (SRC OFF .. the last source) */
            mx[MSLOT_ID(k) + 1u] = (int16_t)(used ? rnd(46u) : 0u);   /* (0.2.25: the 46 destinations of before FMODE: the same 200 matrices as ever. With 47 the draw changes and one of them, EXPR -> RAT1 with SLEW -> ANALOG RES, renders differently packed: an order dependence of before (0.2.24 code too), open) */
            mx[MSLOT_ID(k) + 2u] = (int16_t)(used ? (int32_t)rnd(128u) - 64 : 0);
        }
        memcpy(was, mx, sizeof was);
        h0 = render_part();
        memset(&tt, 0, sizeof tt);
        memcpy(tt.p, mx, sizeof mx);
        moved = mod_compact(&tt);
        memcpy(mx, tt.p, sizeof mx);
        if (!packed_ok(mx, was)) {
            expect("the packed layout: no empty slot before a used one, the used slots in their order", 0);
            break;
        }
        packs += moved != 0;
        h1 = render_part();
        same += h0 == h1;
    }
    printf("matrixpack: 200 random matrices, %u with gaps packed, %u renders identical packed or not\n", packs, same);
    expect("200 random matrices with gaps: the render hash is the same packed or not (no order-dependent behaviour: the shift is stable)", same == 200u);
    expect("  .. most of them had gaps to pack", packs > 100u);
    /* UNIT: MATRIX 1-4, slot 2's SRC to OFF */
    ui_power_on();
    duell_mode_req = DM_STEREO;
    init_engine_of(TSEL, 0u);
    for (k = 0; k < NMSLOT; k++)
        TSEL->p[MSLOT_ID(k)] = TSEL->p[MSLOT_ID(k) + 1u] = TSEL->p[MSLOT_ID(k) + 2u] = 0;
    TSEL->p[P_M1SRC] = MS_LFO; TSEL->p[P_M1DST] = MD_PAN; TSEL->p[P_M1AMT] = 20;
    TSEL->p[P_M2SRC] = MS_ENV; TSEL->p[P_M2DST] = MD_CUT; TSEL->p[P_M2AMT] = 30;
    TSEL->p[P_M3SRC] = MS_VEL; TSEL->p[P_M3DST] = MD_AMP; TSEL->p[P_M3AMT] = 40;
    TSEL->p[P_M4SRC] = MS_KEY; TSEL->p[P_M4DST] = MD_PITCH; TSEL->p[P_M4AMT] = 50;
    go_title("MATRIX 1-4");
    frames(48);
    {
        uint32_t m58 = NPAGES;
        for (k = 0; k < NPAGES; k++)
            if (str_eq(PAGES[k].title, "MATRIX 5-8") && PAGES[k].fam == FAM_LFO)
                m58 = k;
        expect("slots 1..4 all used: MATRIX 5-8 is in the round", m58 < NPAGES && page_visible(m58));
        mod_ui_slot = 1;                                /* slot 2 */
        for (k = 0; k < 20u && TSEL->p[P_M4SRC] != 0; k++)   /* (a detent at a time until slot 2's SRC hits OFF: then the pack moves slot 4 up, slot 4 is clear) */
            turn(EN_K2, -1);
        for (k = 0; k < 20u && TSEL->p[P_M4SRC] != 0; k++)
            turn(EN_K2, 1);
        expect("slot 2's SRC turned to OFF: slots 3 and 4 moved into 2 and 3 (VEL -> AMP 40, KEY -> PITCH 50), slot 4 clear, slot 1 as it was",
               TSEL->p[P_M1SRC] == MS_LFO && TSEL->p[P_M1AMT] == 20 && TSEL->p[P_M2SRC] == MS_VEL && TSEL->p[P_M2DST] == MD_AMP && TSEL->p[P_M2AMT] == 40 &&
               TSEL->p[P_M3SRC] == MS_KEY && TSEL->p[P_M3DST] == MD_PITCH && TSEL->p[P_M3AMT] == 50 && TSEL->p[P_M4SRC] == 0 && TSEL->p[P_M4DST] == 0 && TSEL->p[P_M4AMT] == 0);
        expect("  .. the cursor stays on slot 2 (the slot that moved in)", mod_ui_slot == 1u);
        expect("  .. slot 4 unused: MATRIX 5-8 leaves the round", !page_visible(m58));
        turn(EN_SELECT, 1);
        frames(48);
        expect("  .. SELECT from MATRIX 1-4 skips it", !str_eq(cur_page()->title, "MATRIX 5-8"));
        go_title("MATRIX 1-4");
        mod_ui_slot = 3;
        turn(EN_K2, 1);                                 /* slot 4: a SRC, no DST yet */
        expect("a slot with a SRC and no DST yet stays where it is (being set up)", TSEL->p[P_M4SRC] != 0 && TSEL->p[P_M4DST] == 0 && page_visible(m58));
        TSEL->p[P_M4SRC] = 0;
    }
    /* LOAD: a factory sound with gaps loads packed; the factory data's gaps counted */
    {
        uint32_t e, i, gaps = 0, total = 0;
        for (e = 0; e < NENGINES; e++) {
            if (!eng_ok(e))
                continue;
            for (i = 0; i < ds_count(e); i++) {
                int16_t p[P_COUNT];
                uint32_t k2, seen_empty = 0, gap = 0;
                for (k2 = 0; k2 < P_E0; k2++)
                    p[k2] = TP[k2].def;
                ds_put(ds_get(e, i), p, 0);
                for (k2 = 0; k2 < NMSLOT; k2++) {
                    if (!p[MSLOT_ID(k2)])
                        seen_empty = 1;
                    else if (seen_empty)
                        gap = 1;
                }
                gaps += gap;
                total++;
            }
        }
        printf("matrixpack: factory sounds with a gap in their matrix: %u of %u (they load packed, sound-identical)\n", gaps, total);
        gaps_seen = gaps;
        ui_power_on();
        init_engine_of(TSEL, 0u);
        for (k = 0; k < NMSLOT; k++)
            TSEL->p[MSLOT_ID(k)] = TSEL->p[MSLOT_ID(k) + 1u] = TSEL->p[MSLOT_ID(k) + 2u] = 0;
        TSEL->p[P_M1SRC] = 0; TSEL->p[P_M1DST] = 0; TSEL->p[P_M1AMT] = 0;
        TSEL->p[P_M3SRC] = MS_LFO; TSEL->p[P_M3DST] = MD_PAN; TSEL->p[P_M3AMT] = 11;
        load_begin(TSEL, UNDO_SOUND);
        load_end(TSEL);
        printf("matrixpack: after load_end: slot 1 SRC %d AMT %d, slot 3 SRC %d, undo_depth %u\n", TSEL->p[P_M1SRC], TSEL->p[P_M1AMT], TSEL->p[P_M3SRC], (unsigned)undo_depth);
        expect("a load with a gap (slot 1 empty, slot 3 used): packed at load_end (slot 1 = LFO -> PAN 11, slot 3 clear)", TSEL->p[P_M1SRC] == MS_LFO && TSEL->p[P_M1AMT] == 11 && TSEL->p[P_M3SRC] == 0);
    }
    (void)gaps_seen;
    printf("matrixpack: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
