/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 item 132 (HANDOFF_V2): the two-core runner's deadline and wait limit (twocore_run.c tcr_deadline) are the same, bit for bit, as before item 132, which raised the NOTES
 * budget's BUDGET_LIMIT 80 -> 90 and stopped charging the hand-over BUDGET_HO in the caps (budget.c budget_solve2). tcr_deadline reads only part_fixed, part_cost and global_cost
 * (BD_U, BUDGET_MARGIN 1.15 kept); this test proves the values did not move.
 *   GRID     LAYER / STEREO / DUEL x SENDS on / off x PART FX on / off x every engine on part 1 x every engine on part 2 (each its first factory sound, cleaned) x the multimode
 *            filter of each part OFF / 12 dB / 24 dB / 12 dB + TWIN LP; and every factory sound of every engine as stored, on both parts, LAYER and DUEL
 *   VALUES   for each setup, both NOTES rules (core 1 up / jobs paused), each block of the half 0..3 and the sounding voices of each part 0, 1, 2, 3, 4, 6, 8: the five cost items
 *            (c1f c1v c2f c2v g), the deadline and the wait limit (ticks after the half's start)
 *   CHECK    the FNV-1a hash of all of them equals DL_REF, the hash the same test printed on the sources before item 132 (scratch copy, 2026-10-26)
 * DUELL 1.1 item 137 (owner: BUDGET_MARGIN 1.15 -> 1.00 for the caps, BUDGET_LIMIT 88 -> 81): tcr_deadline reads part_fixed_dl / part_cost_dl / global_cost_dl (BUDGET_DL_MARGIN 1.15),
 * so DL_REF must still hold. The deadline a job is POSTED with (tcr_post_deadline) has one change: under the two-core rule, when the 1.15 deadline is already past at the post,
 * the true-cost deadline (tcr_deadline_true) if it is still ahead and before the wait limit (the old code had no take-over there). For every setup, rule, block, voice count
 * and 16 post moments (every quarter block of the half) the second part checks:
 *   SAME     wherever the 1.15 deadline is ahead of the post, or the one-core rule holds, the posted deadline IS it (bit for bit)
 *   LATER    the posted deadline is never earlier than the 1.15 deadline; the wait limit is the 1.15 one, never moved (it is in DL_REF)
 *   INSIDE   a changed deadline is after the post and before the wait limit
 *   TRUE     a changed deadline is not later than the deadline of the exact 1.00 items (part_fixed, part_cost, global_cost: BUDGET_MARGIN 100) and within 1 % of a block of it
 *   KEPT     wherever the exact true deadline is ahead of the post by more than that 1 % and before the wait limit, a take-over deadline exists (the item's purpose)
 *            and it prints where the posted deadline differs (counts by block and post moment) and the hash of the posted deadlines (DL_REF2, recorded by item 137)
 *   CORE 1   (the BUDGET_LIMIT 81 choice) with both parts at their two-core caps (budget_solve2, KEY SENSE +57 as on the unit), block 0's job, posted 0.27 block into the half
 *            (polymax median 196 us) and running the true cost of part 2, ends at least 6 points of a block (the measured peak over the average) before the 1.15 wait limit
 * DUELL 1.1 item 142 (HANDOFF_V2 item 139, owner-approved): the WAIT LIMIT is the later of the 1.15 one and the same rest at TRUE cost (the 1.15 items brought back, rounded up) less
 * TCR_SLACK (0.2 block), never after the half's end; BUDGET_LIMIT 81 -> 84. DL_REF now hashes the 1.15 wait limit as this test works it out (the items and the deadline are the firmware's):
 * it still holds, so items, deadline and the old limit are unchanged. The firmware's new limit:
 *   WL-TRUE  it is exactly max(1.15 limit, t0 + H - ceil-true rest - TCR_SLACK), never after t0 + H (this test's own formula, every setup, rule, block and voice count)
 *   WL-LATER never earlier than the 1.15 limit and never after the half's end
 *   WL-SAFE  where it moved, it + the EXACT true rest (part_fixed / part_cost / global_cost at margin 1.00; one-core rule with part 2) <= t0 + H - TCR_SLACK
 *   CORE 1   both parts at their two-core caps of BUDGET_LIMIT (KEY SENSE +57), every block k: core 1's job k, posted at 0.27 block + k x (globals + the dearer of part 1 and the job,
 *            true cost) and running part 2's true cost, + 6 points of a block (the peak allowance) ends before the wait limit (least slack printed, also with 13 points: the measured
 *            worst single job, item 139); CORE 0: BUDGET_LIMIT + 6 <= 90. The least slack by limit 80 .. 88 is printed (caps worked out here at each limit)
 *   and SAME / LATER / INSIDE / TRUE / KEPT above against the new limit, the new hashes DL_REF2 (posted deadlines) and DL_REF3 (the new limits), the limits changed by block
 * Build: cc -O1 -w -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -fno-builtin -D_FORTIFY_SOURCE=0
 *        -o build/host/deadline_test tests/deadline_test.c -lm -lpthread   (host_tests.sh name: deadline) */
#define DUELL_TWOCORE 1
#define TC_AUTO_ARM 0
#define UI_TEST_NO_MAIN 1
#include <stdint.h>
#include "ui_test.c"

#define DL_REF 0xb8db0653a8dfc654ull                    /* the pre-item-132 sources (BUDGET_LIMIT 80, BUDGET_HO charged), this test, 2026-10-26 (item 142: with the 1.15 limit as worked out here) */
#define DL_REF2 0x40924773c6c4fbf0ull                   /* item 142: the posted deadlines (tcr_post_deadline) at 16 post moments with the new wait limit (item 137's was 0xccfd48c6e1c8ecc7) */
#define DL_REF3 0x70d45c7084c0d110ull                   /* item 142: the new wait limits (recorded on the item 142 sources, 2026-10-27) */
static uint64_t h3 = 1469598103934665603ull;
static uint64_t n_lim_chg[4], bad_wltrue, bad_wllater, bad_wlsafe, n_lim;
static int32_t lim_chg_max;
static void put3(uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        h3 ^= (v >> (8u * i)) & 0xFFu;
        h3 *= 1099511628211ull;
    }
}
#define T_TRUE(x) (((x) * 100u + BUDGET_DL_MARGIN - 1u) / BUDGET_DL_MARGIN)
/* the wait limit, ticks after the half's start (negative: before it), as the runner must have it: old = the 1.15 rest; new = the later of that and the ceil-true rest + TCR_SLACK */
static int32_t lim_of(uint32_t f1, uint32_t v1, uint32_t f2, uint32_t v2, uint32_t g, uint32_t k, uint32_t a1, uint32_t a2, uint32_t two, int32_t *old)
{
    uint32_t c1 = f1 + a1 * v1, c2 = f2 + a2 * v2, need = c2 + g + (TCR_BLOCKS - 1u - k) * (two ? c1 + g : c1 + c2 + g), rest, rt, c1t = T_TRUE(f1) + a1 * T_TRUE(v1);
    int32_t lo, lt;
    need = need > TCR_NEED_MAX ? TCR_NEED_MAX : need;
    rest = need > c2 ? need - c2 : 0u;
    lo = (int32_t)TCR_HALF_TICKS - (int32_t)(rest * TCR_BLOCK_TICKS / 10000u);
    rt = T_TRUE(g) + (TCR_BLOCKS - 1u - k) * (two ? c1t + T_TRUE(g) : c1t + T_TRUE(f2) + a2 * T_TRUE(v2) + T_TRUE(g));
    rt = rt > TCR_NEED_MAX ? TCR_NEED_MAX : rt;
    lt = (int32_t)TCR_HALF_TICKS - (int32_t)(rt * TCR_BLOCK_TICKS / 10000u) - (int32_t)TCR_SLACK_TICKS;
    *old = lo;
    lo = lt > lo ? lt : lo;
    return lo < (int32_t)TCR_HALF_TICKS ? lo : (int32_t)TCR_HALF_TICKS;
}
static uint64_t h2 = 1469598103934665603ull;
static uint64_t n_post, n_chg, n_chg_blk[4], n_chg_j[16], n_nott, n_nott_true, bad_same, bad_later, bad_inside, bad_true, bad_kept;
static int32_t chg_min = 0x7fffffff, chg_max;           /* how much later a changed deadline is (ticks) */
static void put2(uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        h2 ^= (v >> (8u * i)) & 0xFFu;
        h2 *= 1099511628211ull;
    }
}

static uint64_t h = 1469598103934665603ull;
static uint64_t n_val;
static void put(uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        h ^= (v >> (8u * i)) & 0xFFu;
        h *= 1099511628211ull;
    }
    n_val++;
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
/* CORE 1 (item 137, extended by item 142): both parts at their two-core caps at limit L (KEY SENSE +57 as on the unit): core 1's job k is posted at S + k P (S = 0.27 block, the polymax median
 * of item 139; P = the globals + the dearer of part 1 and the job, true cost) and runs part 2's true cost; its slack to the runner's wait limit of block k, points of a block */
#define NL 9u                                           /* limits 80 .. 88 */
static double c1_min[NL], c1_min13 = 1e9, c1_old_min = 1e9;   /* (c1_old: the same check against the 1.15 limit: what the change is for) */
static uint32_t c1_old_n6;
static uint32_t c1_n6[NL], c1_bad, c1_blk_min;
static double c1_blk[4] = {1e9, 1e9, 1e9, 1e9};
static char c1_where[160];
static uint32_t cap_at(const bd_part_t *r, uint32_t g, uint32_t L)   /* budget_solve2's two-core rule at limit L */
{
    uint32_t f = BD_U(r->ins + r->sat + r->eq + r->misc + r->base), c = BD_U(r->eng + r->filt + r->ext), k;
    for (k = r->cap; k > 1u; k--)
        if (g + f + k * c <= L * 100u)
            break;
    return k;
}
static void core1_slack(void)
{
    bd_t b;
    uint32_t n[2], L, k;
    bd_build(&b);
    b.fx += 57u;                                        /* (KEY SENSE: device builds) */
    if (!b.p[1].on)
        return;
    for (L = 80u; L < 80u + NL; L++) {
        uint32_t gu = BD_U(b.idle + b.fx);
        uint32_t f1 = BD_UDL(b.p[0].ins + b.p[0].sat + b.p[0].eq + b.p[0].misc + b.p[0].base), v1 = BD_UDL(b.p[0].eng + b.p[0].filt + b.p[0].ext);
        uint32_t f2 = BD_UDL(b.p[1].ins + b.p[1].sat + b.p[1].eq + b.p[1].misc + b.p[1].base), v2 = BD_UDL(b.p[1].eng + b.p[1].filt + b.p[1].ext), g = BD_UDL(b.idle + b.fx);
        uint32_t f1t = BD_U(b.p[0].ins + b.p[0].sat + b.p[0].eq + b.p[0].misc + b.p[0].base), v1t = BD_U(b.p[0].eng + b.p[0].filt + b.p[0].ext);
        uint32_t f2t = BD_U(b.p[1].ins + b.p[1].sat + b.p[1].eq + b.p[1].misc + b.p[1].base), v2t = BD_U(b.p[1].eng + b.p[1].filt + b.p[1].ext);
        uint32_t job, c1p, per;
        if (L == BUDGET_LIMIT)
            budget_solve2(&b, n, 1);                    /* (the firmware's own caps at its limit) */
        else
            n[0] = cap_at(&b.p[0], gu, L), n[1] = cap_at(&b.p[1], gu, L);
        if (!b.p[0].on)
            n[0] = 0, f1 = v1 = f1t = v1t = 0;
        job = f2t + n[1] * v2t;
        c1p = f1t + n[0] * v1t;
        per = gu + (c1p > job ? c1p : job);
        for (k = 0; k < TCR_BLOCKS; k++) {
            int32_t old, lim = lim_of(f1, v1, f2, v2, g, k, n[0], n[1], 1u, &old);
            double sl = (lim * 10000.0 / TCR_BLOCK_TICKS - (2700.0 + k * per + job)) / 100.0;   /* points of a block before the allowance */
            if (sl - 6.0 < c1_min[L - 80u])
                c1_min[L - 80u] = sl - 6.0;
            c1_n6[L - 80u] += sl < 6.0;
            if (L != BUDGET_LIMIT)
                continue;
            {
                double so = (old * 10000.0 / TCR_BLOCK_TICKS - (2700.0 + k * per + job)) / 100.0;
                if (so - 6.0 < c1_old_min)
                    c1_old_min = so - 6.0;
                c1_old_n6 += so < 6.0;
            }
            if (sl - 6.0 < c1_blk[k])
                c1_blk[k] = sl - 6.0;
            if (sl - 13.0 < c1_min13)
                c1_min13 = sl - 13.0;
            c1_bad += sl < 6.0;
            if (sl - 6.0 <= c1_min[L - 80u])
                snprintf(c1_where, sizeof c1_where, "mode %d, %s %u + %s %u notes, block %u", duell_mode_req, ENGINES[eng_idx(trk[0].eng_req)]->name, n[0],
                         ENGINES[eng_idx(trk[1].eng_req)]->name, n[1], k);
        }
    }
}
static void set_active(track_t *t, uint32_t n) { uint32_t i; for (i = 0; i < NVOICE; i++) t->v[i].active = i < n; }
static uint32_t setups;
static void sweep(void)                                 /* every rule, block and voice count of the present settings */
{
    static const uint8_t NA[] = {0, 1, 2, 3, 4, 6, 8};
    uint32_t two, k, a, b;
    int nofit;
    uint8_t keep[2][NVOICE];
    for (k = 0; k < NVOICE; k++)
        keep[0][k] = trk[0].v[k].active, keep[1][k] = trk[1].v[k].active;
    core1_slack();
    for (two = 0; two < 2u; two++) {
        uint32_t e1f, e1v, e2f, e2v, eg;                /* item 137: the exact true items (BUDGET_MARGIN 100) */
        tc.up = 1, tc.failed = 0, tcr.paused = (uint8_t)!two;
        tcr.cost_ok = 0;
        tcr.t0 = 1000000u;
        e1f = part_fixed(&trk[0]), e1v = part_cost(&trk[0]), e2f = part_fixed(&trk[1]), e2v = part_cost(&trk[1]), eg = global_cost();
        for (k = 0; k < TCR_BLOCKS; k++)
            for (a = 0; a < NELEM(NA); a++)
                for (b = 0; b < NELEM(NA); b++) {
                    uint32_t dl, lim, j;
                    tcr.blk = (uint8_t)k;
                    set_active(&trk[0], NA[a]);
                    set_active(&trk[1], NA[b]);
                    dl = tcr_deadline(tcr.t0, &nofit);
                    {   /* item 142: the wait limit. DL_REF takes the 1.15 limit as worked out here from the firmware's items (unchanged = the items and the old formula unchanged) */
                        int32_t old, want = lim_of(tcr.c1f, tcr.c1v, tcr.c2f, tcr.c2v, tcr.g, k, NA[a], NA[b], tcr.two, &old), got = (int32_t)(tcr.lim - tcr.t0);
                        uint32_t xr;
                        put(tcr.c1f), put(tcr.c1v), put(tcr.c2f), put(tcr.c2v), put(tcr.g), put(tcr.two);
                        put(dl - tcr.t0), put((uint32_t)old), put((uint32_t)nofit);
                        put3((uint32_t)got);
                        n_lim++;
                        bad_wltrue += got != want;
                        bad_wllater += got < old || got > (int32_t)TCR_HALF_TICKS;
                        xr = eg + (TCR_BLOCKS - 1u - k) * (tcr.two ? e1f + NA[a] * e1v + eg : e1f + NA[a] * e1v + e2f + NA[b] * e2v + eg);   /* the EXACT true rest */
                        xr = xr > TCR_NEED_MAX ? TCR_NEED_MAX : xr;
                        if (got != old) {
                            n_lim_chg[k]++;
                            if (got - old > lim_chg_max)
                                lim_chg_max = got - old;
                            bad_wlsafe += got + (int32_t)(xr * TCR_BLOCK_TICKS / 10000u) > (int32_t)(TCR_HALF_TICKS - TCR_SLACK_TICKS);
                        }
                    }
                    lim = tcr.lim;
                    for (j = 0; j < 16u; j++) {         /* item 137: the deadline a job is posted with, at every quarter block of the half */
                        uint32_t now = tcr.t0 + j * (TCR_BLOCK_TICKS / 4u), pd, need, dt;
                        int nf2, past = (int32_t)(dl - now) <= 0;
                        pd = tcr_post_deadline(now, &nf2);
                        put2(pd - tcr.t0);
                        n_post++;
                        need = e2f + NA[b] * e2v + eg + (TCR_BLOCKS - 1u - k) * (e1f + NA[a] * e1v + eg);   /* (two-core rule) */
                        need = need > TCR_NEED_MAX ? TCR_NEED_MAX : need;
                        dt = tcr.t0 + TCR_HALF_TICKS - need * TCR_BLOCK_TICKS / 10000u - TCR_SLACK_TICKS;
                        bad_later += (int32_t)(pd - dl) < 0 || tcr.lim != lim;
                        if (!past || !tcr.two) {
                            bad_same += pd != dl;
                            continue;
                        }
                        n_nott++;
                        if (pd == dl) {
                            bad_kept += (int32_t)(dt - now) > (int32_t)(TCR_BLOCK_TICKS / 100u) && (int32_t)(dt - lim) < 0;
                            continue;
                        }
                        n_nott_true++, n_chg++, n_chg_blk[k]++, n_chg_j[j]++;
                        bad_inside += !((int32_t)(pd - now) > 0 && (int32_t)(pd - lim) < 0);
                        bad_true += (int32_t)(pd - dt) > 0 || (int32_t)(dt - pd) > (int32_t)(TCR_BLOCK_TICKS / 100u);
                        if ((int32_t)(pd - dl) < chg_min) chg_min = (int32_t)(pd - dl);
                        if ((int32_t)(pd - dl) > chg_max) chg_max = (int32_t)(pd - dl);
                    }
                }
    }
    for (k = 0; k < NVOICE; k++)
        trk[0].v[k].active = keep[0][k], trk[1].v[k].active = keep[1][k];
    tcr.paused = 0, tc.up = 0, tcr.cost_ok = 0;
    setups++;
}
int main(void)
{
    static const int MODES[3] = {DM_LAYER, DM_STEREO, DM_SPLIT};
    uint32_t m, s, f, e1, e2, v1, v2, k, i;
    for (i = 0; i < NL; i++)
        c1_min[i] = 1e9;
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
    printf("deadline: %u setups, %llu values, hash 0x%016llx (reference 0x%016llx)\n", setups, (unsigned long long)n_val, (unsigned long long)h, (unsigned long long)DL_REF);
    printf("deadline: %s the runner's deadline, wait limit and cost items are the same as before item 132 for every setup\n", h == DL_REF ? "ok  " : "FAIL");
    printf("deadline: item 137, posted deadlines: %llu (setup x rule x block x voices x 16 post moments); 1.15 deadline past under the two-core rule: %llu, of them given a take-over deadline: %llu\n",
           (unsigned long long)n_post, (unsigned long long)n_nott, (unsigned long long)n_nott_true);
    printf("deadline:   changed by block 0..3: %llu %llu %llu %llu; by post moment (quarter blocks 0..15):", (unsigned long long)n_chg_blk[0], (unsigned long long)n_chg_blk[1],
           (unsigned long long)n_chg_blk[2], (unsigned long long)n_chg_blk[3]);
    for (i = 0; i < 16u; i++)
        printf(" %llu", (unsigned long long)n_chg_j[i]);
    printf("\ndeadline:   a changed deadline is %d .. %d ticks (%.3f .. %.3f blocks) later than the 1.15 one\n", n_chg ? chg_min : 0, chg_max, n_chg ? chg_min / (double)TCR_BLOCK_TICKS : 0.0,
           chg_max / (double)TCR_BLOCK_TICKS);
    printf("deadline:   posted-deadline hash 0x%016llx (DL_REF2 0x%016llx)\n", (unsigned long long)h2, (unsigned long long)DL_REF2);
    printf("deadline: %s SAME: the posted deadline is the 1.15 one wherever that is ahead of the post or the one-core rule holds (%llu wrong)\n", bad_same ? "FAIL" : "ok  ", (unsigned long long)bad_same);
    printf("deadline: %s LATER: never earlier than the 1.15 deadline, the wait limit never moved (%llu wrong)\n", bad_later ? "FAIL" : "ok  ", (unsigned long long)bad_later);
    printf("deadline: %s INSIDE: a changed deadline is after the post and before the wait limit (%llu wrong)\n", bad_inside ? "FAIL" : "ok  ", (unsigned long long)bad_inside);
    printf("deadline: %s TRUE: a changed deadline is not later than the exact 1.00 deadline and within 1 %% of a block of it (%llu wrong)\n", bad_true ? "FAIL" : "ok  ", (unsigned long long)bad_true);
    printf("deadline: %s KEPT: where the exact true deadline is ahead of the post (by > 1 %% of a block) and before the wait limit, a take-over deadline exists (%llu missing)\n",
           bad_kept ? "FAIL" : "ok  ", (unsigned long long)bad_kept);
    printf("deadline: %s the posted-deadline hash equals DL_REF2\n", h2 == DL_REF2 ? "ok  " : "FAIL");
    printf("deadline: item 142, the wait limit: %llu limits; moved later (by block 0..3): %llu %llu %llu %llu, at most %d ticks (%.3f block); new-limit hash 0x%016llx (DL_REF3 0x%016llx)\n",
           (unsigned long long)n_lim, (unsigned long long)n_lim_chg[0], (unsigned long long)n_lim_chg[1], (unsigned long long)n_lim_chg[2], (unsigned long long)n_lim_chg[3], lim_chg_max,
           lim_chg_max / (double)TCR_BLOCK_TICKS, (unsigned long long)h3, (unsigned long long)DL_REF3);
    printf("deadline: %s WL-TRUE: the wait limit is max(the 1.15 limit, t0 + H - ceil-true rest - TCR_SLACK), never after t0 + H (%llu wrong)\n", bad_wltrue ? "FAIL" : "ok  ", (unsigned long long)bad_wltrue);
    printf("deadline: %s WL-LATER: never earlier than the 1.15 limit, never after the half's end (%llu wrong)\n", bad_wllater ? "FAIL" : "ok  ", (unsigned long long)bad_wllater);
    printf("deadline: %s WL-SAFE: a moved limit + the exact true rest (1.00 items) <= t0 + H - TCR_SLACK (%llu wrong)\n", bad_wlsafe ? "FAIL" : "ok  ", (unsigned long long)bad_wlsafe);
    printf("deadline: %s the new-limit hash equals DL_REF3\n", h3 == DL_REF3 ? "ok  " : "FAIL");
    printf("deadline:   CORE 1 by BUDGET_LIMIT (caps worked out at each limit, least slack after 6 points / setup x blocks under 6):");
    for (i = 0; i < NL; i++)
        printf(" %u: %.1f/%u", 80u + i, c1_min[i], c1_n6[i]);
    printf("\n");
    printf("deadline: %s CORE 1 at the caps of BUDGET_LIMIT %u / margin %u %%: every block's job (posted 0.27 block + k periods) + 6 points ends before the wait limit (least slack after 6 points %.1f: %s; "
           "by block 0..3: %.1f %.1f %.1f %.1f; after 13 points %.1f; %u under 6)\n",
           c1_bad ? "FAIL" : "ok  ", BUDGET_LIMIT, BUDGET_MARGIN, c1_min[BUDGET_LIMIT - 80u], c1_where, c1_blk[0], c1_blk[1], c1_blk[2], c1_blk[3], c1_min13, c1_bad);
    printf("deadline:   (the same caps against the 1.15 wait limit of before item 142: least slack after 6 points %.1f, %u setup x blocks under 6)\n", c1_old_min, c1_old_n6);
    printf("deadline: %s CORE 0: BUDGET_LIMIT %u + 6 points of peak <= 90 %%\n", BUDGET_LIMIT + 6u <= 90u ? "ok  " : "FAIL", BUDGET_LIMIT);
    return h != DL_REF || bad_same || bad_later || bad_inside || bad_true || bad_kept || h2 != DL_REF2 || c1_bad || bad_wltrue || bad_wllater || bad_wlsafe || h3 != DL_REF3 ||
           BUDGET_LIMIT + 6u > 90u || BUDGET_LIMIT < 80u || BUDGET_LIMIT >= 80u + NL;
}
