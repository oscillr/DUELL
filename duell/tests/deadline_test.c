/* SPDX-License-Identifier: GPL-3.0-only */
/* The two-core runner's DEAL, TAKE-BACK DEADLINE and WAIT LIMIT (twocore_run.c tcr_costs / tcr_deal / tcr_deadline / tcr_deadline_true / tcr_post_deadline), for the voice split's one-block order
 * (owner 2026-11-05). REWRITTEN 2026-11-05: the test of items 132 / 137 / 142 pinned the old runner's numbers (part 1 on core 0, part 2 the job, the sends and the master after the wait: DL_REF
 * 0xb8db0653a8dfc654, DL_REF2 0x40924773c6c4fbf0, DL_REF3 0x70d45c7084c0d110). Those formulas are gone: on the unit (BENCHVB) they dropped a block at every chord start, because core 0 now runs
 * the sends and the master BEFORE its wait (working docs/reports/voice-split-bench_2026-11-05.md). The grid is the old test's.
 *   GRID     LAYER / STEREO / DUEL x SENDS on / off x PART FX on / off x every engine on part 1 x every engine on part 2 (each its first factory sound, cleaned) x the multimode
 *            filter of each part OFF / 12 dB / 24 dB / 12 dB + TWIN LP; every factory sound of every engine as stored, on both parts, LAYER and DUEL; the 16 factory multis
 *   VALUES   for each setup, both NOTES rules (core 1 up / jobs paused), each block of the half 0..3 and the listed voices of each part 0, 1, 2, 3, 4, 6, 8
 * What must hold, each worked out here on its own from the table's five items (c1f c1v c2f c2v g, which the firmware reads from budget.c):
 *   DEAL     the sets and the two running numbers are those of "each voice, in voice order over the parts, to the core that is less full at that moment" (core 0 starts with the sends and the
 *            master, core 1 with both parts' own stages); no voice went to the fuller core; the two numbers add up to the whole block
 *   DL       the take-back deadline = the half's end - (core 1's share + the blocks still to come) - TCR_SLACK
 *   WL       the wait limit = the later of (the half's end - the blocks still to come, the 1.15 table) and (the same at ceil-true cost - TCR_SLACK), never after the half's end
 *            (a block still to come = core 0's own load of this deal under the two-core rule, everything under the one-core rule)
 *   ORDER    the deadline is never after the wait limit, the limit never after the half's end
 *   WL-SAFE  a limit moved by the true-cost term + the EXACT true cost (budget.c at margin 1.00) of the blocks still to come <= the half's end - TCR_SLACK
 *   NO-GLOBALS  (the defect of BENCHVB) neither number depends on this block's sends and master beyond the deal: with one voice more on core 0 the limit moves by exactly that voice's later blocks
 *            (checked through DL / WL: the formulas here have no other term)
 *   and for the deadline a job is POSTED with (tcr_post_deadline), at 16 post moments (every quarter block of the half):
 *   SAME     wherever the 1.15 deadline is ahead of the post, or the one-core rule holds, the posted deadline IS it
 *   LATER    the posted deadline is never earlier than the 1.15 deadline; the wait limit does not move
 *   INSIDE   a changed deadline (the true-cost take-over, item 137) is after the post and before the wait limit
 *   TRUE     a changed deadline is not later than the deadline of the exact 1.00 items and within 1 % of a block of it
 *   KEPT     wherever the exact true deadline is ahead of the post by more than that 1 % and before the wait limit, a take-over deadline exists
 *   HANDOVER the cost items the main loop hands over (budget.c bd_dl_publish) equal the ones the runner works out itself, and the runner takes the handed-over copy
 *   HASH     the hash of every item, set, number, deadline and limit (DL_REF), and of the posted deadlines (DL_REF2): recorded on the sources of 2026-11-05 with this test
 * PRINTED, not a verdict (the voice caps are still the old budget's, one part a core; they are refitted after the voice costs are measured on the unit): with both parts at their two-core caps
 * (KEY SENSE +57 as on the unit) and the voices dealt, core 1's job of block k, posted at 0.27 block + k x (the fuller core's true load), running core 1's true load: its slack to the wait limit.
 * Build: cc -O1 -w -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -fno-builtin -D_FORTIFY_SOURCE=0
 *        -o build/host/deadline_test tests/deadline_test.c -lm -lpthread   (host_tests.sh name: deadline) */
#define DUELL_TWOCORE 1
#define TC_AUTO_ARM 0
#define UI_TEST_NO_MAIN 1
#include <stdint.h>
#include "ui_test.c"

#define DL_REF 0x313d30e7676c922bull                                 /* the items, the deal, the deadline and the wait limit (2026-11-05, the voice split's one-block order) */
#define DL_REF2 0xa882020c92f83bedull                                 /* the posted deadlines at 16 post moments (2026-11-05) */
#define T_TRUE(x) (((x) * 100u + BUDGET_DL_MARGIN - 1u) / BUDGET_DL_MARGIN)

static uint64_t h = 1469598103934665603ull, h2 = 1469598103934665603ull;
static uint64_t n_val, n_post, n_chg, n_chg_blk[4], n_nott, n_lim_true[4];
static uint64_t bad_deal, bad_dl, bad_wl, bad_order, bad_wlsafe, bad_same, bad_later, bad_inside, bad_true, bad_kept, bad_hand;
static void put(uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        h ^= (v >> (8u * i)) & 0xFFu;
        h *= 1099511628211ull;
    }
    n_val++;
}
static void put2(uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        h2 ^= (v >> (8u * i)) & 0xFFu;
        h2 *= 1099511628211ull;
    }
}
static void boot(void) { ui_power_on(); multi_boot(); frame(); }
enum { V_OFF, V_M12, V_M24, V_TW, V_N };
static void clean(track_t *t, int va)
{
    uint32_t i;
    for (i = 0; i < NMSLOT; i++)
        t->p[MSLOT_ID(i)] = t->p[MSLOT_ID(i) + 1] = t->p[MSLOT_ID(i) + 2] = 0;
    t->p[P_TWTYPE] = TT_OFF, t->p[P_DRV2] = 0;
    for (i = 0; i < NX; i++)
        if (i != X_LINK && i != X_LFO1LVL && i != X_LFO2LVL)
            XV(t, i) = 0;
    t->p[P_KTRK] = 0, t->p[P_LD_FLT] = t->p[P_E2FLT] = t->p[P_ED_FLT] = t->p[P_L2FLT] = 0;
    t->p[P_VOICE] = V_POLY, t->p[P_AMODE] = 0;
    t->p[P_FREEA] = va == V_OFF ? UF_STEPS : 50, t->p[P_FREEC] = 0, t->p[P_FREED] = va == V_M24 ? 2 : 1;
    if (va == V_TW)
        t->p[P_TWTYPE] = TT_LP, t->p[P_CUT2] = 10;
}
/* the deal as this test works it out: n0 / n1 listed voices; the items it, cv[] the voice costs; gives the sets and the two loads; ok = 0 if a voice went to the fuller core */
typedef struct { uint32_t sel[2], f0, f1; int ok; } deal_t;
static deal_t deal_of(uint32_t g, uint32_t fx, const uint32_t *cv, uint32_t n0, uint32_t n1)
{
    deal_t d = {{0, 0}, g, fx, 1};
    uint32_t p, r;
    for (p = 0; p < 2u; p++)
        for (r = 0; r < (p ? n1 : n0); r++) {
            if (d.f1 <= d.f0)
                d.sel[p] |= 1u << r, d.f1 += cv[p];
            else
                d.f0 += cv[p];
        }
    return d;
}
/* the loads of a GIVEN deal (the firmware's sets) at other costs: the exact true ones */
static void load_of(const uint32_t *sel, uint32_t g, uint32_t fx, const uint32_t *cv, uint32_t n0, uint32_t n1, uint32_t *f0, uint32_t *f1)
{
    uint32_t p, r;
    *f0 = g, *f1 = fx;
    for (p = 0; p < 2u; p++)
        for (r = 0; r < (p ? n1 : n0); r++)
            *((sel[p] >> r) & 1u ? f1 : f0) += cv[p];
}
static uint32_t clampn(uint32_t x) { return x > TCR_NEED_MAX ? TCR_NEED_MAX : x; }
static int32_t ticks_of(uint32_t x) { return (int32_t)(x * TCR_BLOCK_TICKS / 10000u); }

/* PRINTED: core 1's job at the old two-core caps (see the header) */
static double c1_min = 1e9, c1_blk[4] = {1e9, 1e9, 1e9, 1e9};
static uint32_t c1_n6, c1_n;
static char c1_where[160];
static void core1_slack(void)
{
    bd_t b;
    uint32_t n[2], k;
    bd_build(&b);
    b.fx += 57u;                                        /* (KEY SENSE: device builds) */
    if (!b.p[1].on)
        return;
    {
        uint32_t g = BD_UDL(b.idle + b.fx), gt = BD_U(b.idle + b.fx);
        uint32_t f1 = BD_UDL(b.p[0].ins + b.p[0].sat + b.p[0].eq + b.p[0].misc + b.p[0].base), f2 = BD_UDL(b.p[1].ins + b.p[1].sat + b.p[1].eq + b.p[1].misc + b.p[1].base);
        uint32_t f1t = BD_U(b.p[0].ins + b.p[0].sat + b.p[0].eq + b.p[0].misc + b.p[0].base), f2t = BD_U(b.p[1].ins + b.p[1].sat + b.p[1].eq + b.p[1].misc + b.p[1].base);
        uint32_t cv[2] = {BD_UDL(b.p[0].eng + b.p[0].filt + b.p[0].ext), BD_UDL(b.p[1].eng + b.p[1].filt + b.p[1].ext)};
        uint32_t cvt[2] = {BD_U(b.p[0].eng + b.p[0].filt + b.p[0].ext), BD_U(b.p[1].eng + b.p[1].filt + b.p[1].ext)};
        uint32_t f0x, f1x, f0c, f1c, per, cvc[2];
        deal_t d;
        budget_solve2(&b, n, 1);                        /* (the firmware's own caps at its limit: the old rule, one part a core) */
        if (!b.p[0].on)
            n[0] = 0, f1 = f1t = 0;
        d = deal_of(0, 0, cv, n[0], n[1]);              /* (2026-11-07: the split by the voice costs alone, the balance at 0 here: twocore_run.c tcr_deal) */
        load_of(d.sel, 0, g + f1 + f2, cv, n[0], n[1], &d.f0, &d.f1);   /* (the table's loads: the globals and the stages are core 1's) */
        load_of(d.sel, 0, gt + f1t + f2t, cvt, n[0], n[1], &f0x, &f1x);
        cvc[0] = T_TRUE(cv[0]), cvc[1] = T_TRUE(cv[1]);
        load_of(d.sel, 0, T_TRUE(g) + T_TRUE(f1) + T_TRUE(f2), cvc, n[0], n[1], &f0c, &f1c);   /* (the ceil-true loads the runner's limit uses) */
        (void)f1c;
        per = f0x > f1x ? f0x : f1x;
        for (k = 0; k < TCR_BLOCKS; k++) {
            const uint32_t left = TCR_BLOCKS - 1u - k;
            uint32_t later = clampn(left * d.f0), rt = clampn(left * f0c);
            int32_t lo = (int32_t)TCR_HALF_TICKS - ticks_of(later), lt = (int32_t)TCR_HALF_TICKS - ticks_of(rt) - (int32_t)TCR_SLACK_TICKS, lim = lt > lo ? lt : lo;
            double sl;
            lim = lim < (int32_t)TCR_HALF_TICKS ? lim : (int32_t)TCR_HALF_TICKS;
            sl = (lim * 10000.0 / TCR_BLOCK_TICKS - (2700.0 + k * per + f1x)) / 100.0 - 6.0;   /* points of a block, after the 6 points of peak allowance */
            c1_n++;
            c1_n6 += sl < 0.0;
            if (sl < c1_blk[k])
                c1_blk[k] = sl;
            if (sl < c1_min) {
                c1_min = sl;
                snprintf(c1_where, sizeof c1_where, "mode %d, %s %u + %s %u notes, block %u", duell_mode_req, ENGINES[eng_idx(trk[0].eng_req)]->name, n[0],
                         ENGINES[eng_idx(trk[1].eng_req)]->name, n[1], k);
            }
        }
    }
}
static uint32_t setups;
static void sweep(void)                                 /* every rule, block and voice count of the present settings */
{
    static const uint8_t NA[] = {0, 1, 2, 3, 4, 6, 8};
    uint32_t two, k, a, b;
    int nofit;
    core1_slack();
    {   /* HANDOVER: the items the main loop hands over (budget.c bd_dl_publish) are the ones the runner works out itself, and the runner takes them */
        uint32_t d1f, d1v, d2f, d2v, dg;
        part_dl(&trk[0], &d1f, &d1v), part_dl(&trk[1], &d2f, &d2v), dg = global_cost_dl();
        bd_dl_publish();
        tcr.blk = 0, tcr.cost_ok = 0;
        tcr.c1f = tcr.c1v = tcr.c2f = tcr.c2v = tcr.g = 0xFFFFFFFFu;
        tcr_costs();
        bad_hand += !bd_dl_ok || tcr.c1f != d1f || tcr.c1v != d1v || tcr.c2f != d2f || tcr.c2v != d2v || tcr.g != dg;
        bd_dl_ok = 0;                                   /* (the rest of the sweep: the runner's own working out, as before) */
    }
    for (two = 0; two < 2u; two++) {
        uint32_t ecv[2], eg, efx;                       /* the exact true items (BUDGET_MARGIN 100) */
        tc.up = 1, tc.failed = 0, tcr.paused = (uint8_t)!two;
        tcr.cost_ok = 0;
        tcr.t0 = 1000000u;
        ecv[0] = part_cost(&trk[0]), ecv[1] = part_cost(&trk[1]), eg = global_cost(), efx = part_fixed(&trk[0]) + part_fixed(&trk[1]);
        for (k = 0; k < TCR_BLOCKS; k++)
            for (a = 0; a < NELEM(NA); a++)
                for (b = 0; b < NELEM(NA); b++) {
                    const uint32_t left = TCR_BLOCKS - 1u - k;
                    uint32_t dl, lim, j, cv[2], cvt[2], later, rt, need, x0, x1;
                    int32_t lo, lt, want_lim, want_dl, got_lim, got_dl;
                    deal_t d, dt;
                    tcr.blk = (uint8_t)k;
                    tc_blk.vp[0].nl = NA[a];
                    tc_blk.vp[1].nl = NA[b];
                    tcr_costs();
                    tcr_deal();
                    dl = tcr_deadline(tcr.t0, &nofit);
                    lim = tcr.lim;
                    got_dl = (int32_t)(dl - tcr.t0), got_lim = (int32_t)(lim - tcr.t0);
                    cv[0] = tcr.c1v, cv[1] = tcr.c2v, cvt[0] = T_TRUE(tcr.c1v), cvt[1] = T_TRUE(tcr.c2v);
                    d = deal_of(0, 0, cv, NA[a], NA[b]);   /* (2026-11-07: the split by the voice costs alone; the loads: the globals and the stages are core 1's) */
                    load_of(d.sel, 0, tcr.g + tcr.c1f + tcr.c2f, cv, NA[a], NA[b], &d.f0, &d.f1);
                    load_of(d.sel, 0, T_TRUE(tcr.g) + T_TRUE(tcr.c1f) + T_TRUE(tcr.c2f), cvt, NA[a], NA[b], &dt.f0, &dt.f1);
                    bad_deal += d.sel[0] != tc_blk.sel[0] || d.sel[1] != tc_blk.sel[1] || d.f0 != tcr.f0 || d.f1 != tcr.f1 || dt.f0 != tcr.f0t || dt.f1 != tcr.f1t ||
                                d.f0 + d.f1 != tcr.g + tcr.c1f + tcr.c2f + NA[a] * cv[0] + NA[b] * cv[1];
                    {   /* no voice went to the fuller core: replay the deal and look at each step */
                        uint32_t p, r, q0 = 0, q1 = 0;
                        for (p = 0; p < 2u; p++)
                            for (r = 0; r < (p ? NA[b] : NA[a]); r++) {
                                if ((tc_blk.sel[p] >> r) & 1u)
                                    bad_deal += q1 > q0, q1 += cv[p];
                                else
                                    bad_deal += q1 <= q0, q0 += cv[p];   /* (a voice of core 0: core 1 was the fuller one) */
                            }
                    }
                    later = clampn(left * (tcr.two ? d.f0 : d.f0 + d.f1));
                    need = clampn(d.f1 + later);
                    rt = clampn(left * (tcr.two ? dt.f0 : dt.f0 + dt.f1));
                    lo = (int32_t)TCR_HALF_TICKS - ticks_of(later);
                    lt = (int32_t)TCR_HALF_TICKS - ticks_of(rt) - (int32_t)TCR_SLACK_TICKS;
                    want_lim = lt > lo ? lt : lo;
                    want_lim = want_lim < (int32_t)TCR_HALF_TICKS ? want_lim : (int32_t)TCR_HALF_TICKS;
                    want_dl = (int32_t)TCR_HALF_TICKS - ticks_of(need) - (int32_t)TCR_SLACK_TICKS;
                    bad_dl += got_dl != want_dl;
                    bad_wl += got_lim != want_lim;
                    bad_order += got_dl > got_lim || got_lim > (int32_t)TCR_HALF_TICKS;
                    load_of(tc_blk.sel, 0, efx + eg, ecv, NA[a], NA[b], &x0, &x1);   /* the firmware's deal at the EXACT true costs (2026-11-07: the globals are core 1's) */
                    if (got_lim != lo) {
                        n_lim_true[k]++;
                        bad_wlsafe += got_lim + ticks_of(clampn(left * (tcr.two ? x0 : x0 + x1))) > (int32_t)(TCR_HALF_TICKS - TCR_SLACK_TICKS);
                    }
                    put(tcr.c1f), put(tcr.c1v), put(tcr.c2f), put(tcr.c2v), put(tcr.g), put(tcr.two);
                    put(tc_blk.sel[0]), put(tc_blk.sel[1]), put(tcr.f0), put(tcr.f1), put(tcr.f0t), put(tcr.f1t);
                    put((uint32_t)got_dl), put((uint32_t)got_lim), put((uint32_t)nofit);
                    for (j = 0; j < 16u; j++) {         /* the deadline a job is posted with, at every quarter block of the half */
                        uint32_t now = tcr.t0 + j * (TCR_BLOCK_TICKS / 4u), pd, dtx;
                        int nf2, past = (int32_t)(dl - now) <= 0;
                        pd = tcr_post_deadline(now, &nf2);
                        put2(pd - tcr.t0);
                        n_post++;
                        dtx = tcr.t0 + TCR_HALF_TICKS - (uint32_t)ticks_of(clampn(x1 + left * x0)) - TCR_SLACK_TICKS;   /* the exact true take-over deadline (two-core rule) */
                        bad_later += (int32_t)(pd - dl) < 0 || tcr.lim != lim;
                        if (!past || !tcr.two) {
                            bad_same += pd != dl;
                            continue;
                        }
                        n_nott++;
                        if (pd == dl) {
                            bad_kept += (int32_t)(dtx - now) > (int32_t)(TCR_BLOCK_TICKS / 100u) && (int32_t)(dtx - lim) < 0;
                            continue;
                        }
                        n_chg++, n_chg_blk[k]++;
                        bad_inside += !((int32_t)(pd - now) > 0 && (int32_t)(pd - lim) < 0);
                        bad_true += (int32_t)(pd - dtx) > 0 || (int32_t)(dtx - pd) > (int32_t)(TCR_BLOCK_TICKS / 100u);
                    }
                }
    }
    tc_blk.vp[0].nl = tc_blk.vp[1].nl = 0;
    tcr.paused = 0, tc.up = 0, tcr.cost_ok = 0;
    setups++;
}
int main(void)
{
    static const int MODES[3] = {DM_LAYER, DM_STEREO, DM_SPLIT};
    uint32_t m, s, f, e1, e2, v1, v2, k, i;
    for (m = 0; m < 3u; m++)
        for (s = 0; s < 4u; s++)                        /* SENDS on/off x PART FX on/off */
            for (e1 = 0; e1 < NENG_SHOWN; e1++) {
                boot();
                duell_mode_force(MODES[m]);
                for (e2 = 0; e2 < NENG_SHOWN; e2++)
                    for (v1 = 0; v1 < V_N; v1++)
                        for (v2 = 0; v2 < V_N; v2++) {
                            uint32_t ve[2] = {v1, v2}, en[2] = {ENGINE_ORDER[e1], ENGINE_ORDER[e2]};
                            for (k = 0; k < 2u; k++) {
                                trk[k].eng_req = (uint8_t)en[k];
                                apply_preset_to(&trk[k], 0);
                                clean(&trk[k], (int)ve[k]);
                                mph[k].nid = mph[k].fnid = 0;
                            }
                            mods_off = (s & 1u ? MODS_SENDS : 0u) | (s & 2u ? MODS_PFX : 0u);
                            sweep();
                        }
                mods_off = 0;
            }
    for (m = 0; m < 2u; m++)                            /* every factory sound as stored (inserts, SAT, EQ, matrix, SUB, TWIN, morph-free), both parts */
        for (e1 = 0; e1 < NENG_SHOWN; e1++) {
            uint32_t en = ENGINE_ORDER[e1], n = ds_count(en);
            boot();
            duell_mode_force(m ? DM_SPLIT : DM_LAYER);
            for (f = 0; f < n; f++) {
                for (k = 0; k < 2u; k++) {
                    trk[k].eng_req = (uint8_t)en;
                    apply_preset_to(&trk[k], f);
                    mph[k].nid = mph[k].fnid = 0;
                }
                sweep();
            }
        }
    for (i = 1; i <= 16u; i++) {                        /* the 16 factory multis as loaded (MODE RECALL), morphs included */
        boot();
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
        project_load((int)i);
        frame();
        sweep();
    }
    printf("deadline: %u setups, %llu values, hash 0x%016llx (DL_REF 0x%016llx); posted deadlines %llu, hash 0x%016llx (DL_REF2 0x%016llx)\n", setups, (unsigned long long)n_val,
           (unsigned long long)h, (unsigned long long)DL_REF, (unsigned long long)n_post, (unsigned long long)h2, (unsigned long long)DL_REF2);
    printf("deadline: %s DEAL: every voice to the less full core, the sets and the loads as worked out here (%llu wrong)\n", bad_deal ? "FAIL" : "ok  ", (unsigned long long)bad_deal);
    printf("deadline: %s DL: the take-back deadline = the half's end - (core 1's share + the blocks still to come) - TCR_SLACK (%llu wrong)\n", bad_dl ? "FAIL" : "ok  ", (unsigned long long)bad_dl);
    printf("deadline: %s WL: the wait limit = the later of the 1.15 one and the true-cost one less TCR_SLACK, from the blocks still to come only (%llu wrong; set by the true-cost term, by block 0..3: %llu %llu %llu %llu)\n",
           bad_wl ? "FAIL" : "ok  ", (unsigned long long)bad_wl, (unsigned long long)n_lim_true[0], (unsigned long long)n_lim_true[1], (unsigned long long)n_lim_true[2], (unsigned long long)n_lim_true[3]);
    printf("deadline: %s ORDER: the deadline never after the wait limit, the limit never after the half's end (%llu wrong)\n", bad_order ? "FAIL" : "ok  ", (unsigned long long)bad_order);
    printf("deadline: %s WL-SAFE: a limit set by the true-cost term + the exact true cost of the blocks still to come <= the half's end - TCR_SLACK (%llu wrong)\n", bad_wlsafe ? "FAIL" : "ok  ", (unsigned long long)bad_wlsafe);
    printf("deadline:   posted deadlines: 1.15 deadline past under the two-core rule %llu, of them given a true-cost take-over deadline %llu (by block 0..3: %llu %llu %llu %llu)\n", (unsigned long long)n_nott,
           (unsigned long long)n_chg, (unsigned long long)n_chg_blk[0], (unsigned long long)n_chg_blk[1], (unsigned long long)n_chg_blk[2], (unsigned long long)n_chg_blk[3]);
    printf("deadline: %s SAME: the posted deadline is the 1.15 one wherever that is ahead of the post or the one-core rule holds (%llu wrong)\n", bad_same ? "FAIL" : "ok  ", (unsigned long long)bad_same);
    printf("deadline: %s LATER: never earlier than the 1.15 deadline, the wait limit never moved (%llu wrong)\n", bad_later ? "FAIL" : "ok  ", (unsigned long long)bad_later);
    printf("deadline: %s INSIDE: a changed deadline is after the post and before the wait limit (%llu wrong)\n", bad_inside ? "FAIL" : "ok  ", (unsigned long long)bad_inside);
    printf("deadline: %s TRUE: a changed deadline is not later than the exact 1.00 deadline and within 1 %% of a block of it (%llu wrong)\n", bad_true ? "FAIL" : "ok  ", (unsigned long long)bad_true);
    printf("deadline: %s KEPT: where the exact true deadline is ahead of the post (by > 1 %% of a block) and before the wait limit, a take-over deadline exists (%llu missing)\n",
           bad_kept ? "FAIL" : "ok  ", (unsigned long long)bad_kept);
    printf("deadline: %s HANDOVER: the cost items the main loop hands over are the ones the runner works out itself, and the runner takes them (%llu wrong)\n", bad_hand ? "FAIL" : "ok  ", (unsigned long long)bad_hand);
    printf("deadline: %s HASH: the items, deals, deadlines and limits equal DL_REF, the posted deadlines DL_REF2\n", h == DL_REF && h2 == DL_REF2 ? "ok  " : "FAIL");
    printf("deadline:   (printed, no verdict: the caps are the old budget's) core 1's job at the two-core caps of BUDGET_LIMIT %u, voices dealt, posted 0.27 block + k x the fuller core's true load: least slack to the wait limit after 6 points "
           "%.1f (%s); by block 0..3: %.1f %.1f %.1f %.1f; %u of %u setup x blocks under 0\n", BUDGET_LIMIT, c1_min, c1_where, c1_blk[0], c1_blk[1], c1_blk[2], c1_blk[3], c1_n6, c1_n);
    return h != DL_REF || h2 != DL_REF2 || bad_hand || bad_deal || bad_dl || bad_wl || bad_order || bad_wlsafe || bad_same || bad_later || bad_inside || bad_true || bad_kept;
}
