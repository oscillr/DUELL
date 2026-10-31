/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 item 90 (HANDOFF_V2): the NOTES limits and the AVAILABLE VOICES counter, one core and two cores (host_tests.sh polylimits). Build with -DOLD against sources
 * before item 90 to print the old numbers (the two-core column is then "-": the rule did not exist).
 *   TABLE   every engine (its first factory sound, cleaned: no matrix, SUB or TWIN; POLY), both parts alike, LAYER, KEY SENSE on (+57, the device builds): MULTI off,
 *           MULTI 12 dB, MULTI 12 dB + TWIN LP; the notes a part may play on one core and on two cores
 *   STATES  (item 116) the table is printed twice: A = the erased unit's way (sends on, KEY SENSE on), B = SENDS OFF and KEY SENSE off; and the erased unit's factory pair as it is
 *   CHECKS  (a) erased unit, WAVE on both parts, POLY, LAYER: one core >= 2, two cores >= 5 a part (item 116: were 3 and 6), and two cores >= one core everywhere in the table
 *           (b) the counter (item 117/125: each capsule its OWN part's number): with two-core numbers in force one key down shows each part's number less one, not 0; with the jobs paused (bd_one_now) the one-core number
 *           (c) each part has its own voices: 7 + 7 notes sound at once with two-core numbers (the shared pool of 8 is gone)
 *           (d) M02 NIGHT ORGAN as stored (the owner's old session) one core / two cores, printed
 *           (item 132) every cell: the two-core number is the most notes with the globals, the part's stages and its voices within BUDGET_LIMIT (88 %): no hand-over charged
 *           (item 137) the same with BUDGET_MARGIN 1.00 and BUDGET_LIMIT 81 % (true model load + 6 points of measured peak fits core 0 and core 1); item 142: 84 %, the runner's wait limit at true cost
 *   STATE S (item 142) a third table: POLY-MAX-BUDGET's own state (the patch loaded with its morph and the suite's set values), so the unit's numbers in that suite are predicted here */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
static int fails;
static void chk(const char *w, int ok) { printf("polylimits: %-4s %s\n", ok ? "ok" : "FAIL", w); fails += !ok; }
static uint32_t eng_named(const char *n) { uint32_t e; for (e = 0; e < NENGINES; e++) if (str_eq(ENGINES[e]->name, n)) return e; return 0; }
static void boot(void) { memset(nor, 0xFF, sizeof nor); ui_power_on(); flash_ok = 1; multi_boot(); frame(); }
enum { V_OFF, V_M12, V_TW, V_N };
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
    t->p[P_FREEA] = va == V_OFF ? UF_STEPS : 50, t->p[P_FREEC] = 0, t->p[P_FREED] = 1;
    if (va == V_TW)
        t->p[P_TWTYPE] = TT_LP, t->p[P_CUT2] = 10;
}
static uint32_t ks_on = 1;                          /* KEY SENSE counted (+57 host instructions a sample, as the device builds do): state A; state B has it off */
static void numbers(uint32_t *one, uint32_t *two)   /* the part numbers by both rules */
{
    bd_t b;
    bd_build(&b);
    b.fx += ks_on ? 57u : 0u;
#ifdef OLD
    budget_solve(&b, one);
    two[0] = two[1] = 0;
#else
    budget_solve2(&b, one, 0);
    budget_solve2(&b, two, 1);
#endif
}
static uint32_t held(const track_t *t) { uint32_t i, n = 0; for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate && t->v[i].stage != 4u; return n; }
static uint32_t sounding(const track_t *t) { uint32_t i, n = 0; for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].stage != 4u; return n; }
int main(void)
{
    static const char *const VN[V_N] = {"MULTI off", "MULTI 12 dB", "MULTI + TWIN LP"};
    uint32_t t, va, one[2], two[2], k, okt = 1, okho = 1, st;
    int32_t o[2 * CTL];
    printf("polylimits: notes a part in LAYER (both parts the same engine, its first factory sound cleaned), KEY SENSE on: one core / two cores%s\n",
#ifdef OLD
           " (OLD sources: no two-core rule)"
#else
           ""
#endif
    );
    for (st = 0; st < 2u; st++) {   /* state A: the erased unit's way (sends on, KEY SENSE / velocity on); state B: effects (SENDS) and KEY SENSE off (item 116) */
    ks_on = st == 0;
    printf("polylimits: --- state %s\n", st == 0 ? "A (sends on, key sense on)" : "B (SENDS OFF, key sense off)");
    printf("polylimits: %-8s %18s %18s %18s\n", "engine", VN[0], VN[1], VN[2]);
    for (t = 0; t < NENG_SHOWN; t++) {
        uint32_t e = ENGINE_ORDER[t];
        printf("polylimits: %-8s", ENGINES[e]->name);
        for (va = 0; va < V_N; va++) {
            boot();
            duell_mode_force(DM_LAYER);
            for (k = 0; k < 2; k++) {
                trk[k].eng_req = (uint8_t)e;
                apply_preset_to(&trk[k], 0);
                clean(&trk[k], (int)va);
                mph[k].nid = mph[k].fnid = 0;
            }
            frame();
            mods_off = st ? MODS_SENDS : 0;   /* (after the frame: it puts the module state back) */
            numbers(one, two);
#ifndef OLD
            {   /* item 132: the two-core number is the most notes with globals + the part's stages + its voices <= BUDGET_LIMIT, no hand-over charged (item 137: 81 %, margin 1.00) */
                bd_t b;
                uint32_t c, f, g, n;
                bd_build(&b);
                b.fx += ks_on ? 57u : 0u;
                c = BD_U(b.p[0].eng + b.p[0].filt + b.p[0].ext), f = BD_U(b.p[0].ins + b.p[0].sat + b.p[0].eq + b.p[0].misc + b.p[0].base), g = BD_U(b.idle + b.fx);
                for (n = b.p[0].cap; n > 1u && g + f + n * c > BUDGET_LIMIT * 100u; n--)
                    ;
                okho &= BUDGET_LIMIT == 84u && BUDGET_MARGIN == 100u && two[0] == n;
            }
#endif
            if (two[0])
                printf("   %8u / %-6u", one[0], two[0]);
            else
                printf("   %8u / %-6s", one[0], "-");
            okt &= !two[0] || two[0] >= one[0];
        }
        printf("\n");
    }
    }
    mods_off = 0;
    ks_on = 1;
    {   /* DUELL 1.1 item 142: state S = POLY-MAX-BUDGET's own state (tools/gen_polymax_suite.py): each engine's first factory patch LOADED on both parts (its MORPH End B rolled by the
         * load), then the suite's `set` values (CLEAN: chord, voice mode, arp, KEY TRK, the four cutoff depths; the variant's cutoff, mode, slope, TWIN), sends on, KEY SENSE on.
         * The matrix, SUB, CYCLE, TWIN extension values, SAT / EQ / DUCK / INSERT and the morph stay as the patch has them: the unit's NOTES in that suite are these numbers */
        static const struct { const char *e; uint32_t slot; } PT[] = {{"ANALOG", 1}, {"PHASE", 10}, {"WAVE", 21}, {"WHEEL", 18}, {"VOICE", 14}, {"FM6", 7}, {"PHYS", 19}, {"TRIO", 15},
                                                                     {"NOISE", 20}, {"LOFI", 12}};
        static const int16_t CLN[][2] = {{81, 0}, {37, 0}, {17, 0}, {27, 0}, {14, 0}, {5, 0}, {114, 0}, {117, 0}};
        static const int16_t VAR[3][5][2] = {{{45, 82}, {47, 0}, {48, 1}, {43, 0}, {-1, 0}}, {{45, 50}, {47, 0}, {48, 1}, {43, 0}, {-1, 0}}, {{45, 50}, {47, 0}, {48, 1}, {43, 1}, {106, 10}}};
        int32_t ob[2 * CTL];
        uint32_t i, j, ok_eng = 1;
        printf("polylimits: --- state S (POLY-MAX-BUDGET's state: the engine's first factory patch loaded, with its morph, the suite's set values; sends on, key sense on)\n");
        printf("polylimits: %-8s %18s %18s %18s\n", "engine", VN[0], VN[1], VN[2]);
        for (t = 0; t < NENG_SHOWN; t++) {
            uint32_t e = ENGINE_ORDER[t], s = 0;
            for (i = 0; i < NELEM(PT); i++)
                if (eng_named(PT[i].e) == e)
                    s = PT[i].slot;
            printf("polylimits: %-8s", ENGINES[e]->name);
            for (va = 0; va < V_N; va++) {
                boot();
                duell_mode_force(DM_LAYER);
                for (k = 0; k < 2; k++) {
                    pc_recall(k, s - 1u, 0);
                    for (i = 0; i < 40u; i++) { mix_block(ob, CTL); frame(); }
                    ok_eng &= eng_idx(trk[k].eng_req) == e;
                    for (i = 0; i < NELEM(CLN); i++)
                        trk[k].p[CLN[i][0]] = CLN[i][1];
                    for (i = 0; i < 5u; i++)
                        if (VAR[va][i][0] >= 0)
                            trk[k].p[VAR[va][i][0]] = VAR[va][i][1];
                    for (i = 0; i < 10u; i++) { mix_block(ob, CTL); frame(); }
                }
                frame();
                numbers(one, two);
                if (two[0])
                    printf("   %8u / %-6u", one[0], two[0]);
                else
                    printf("   %8u / %-6s", one[0], "-");
                (void)j;
            }
            printf("\n");
        }
        chk("(item 142) state S: every engine's first factory patch is in the slot the suite loads", ok_eng);
    }
#ifndef OLD
    chk("two cores give every engine at least the one-core number", okt);
    chk("(item 132/137/142) the two-core number = the most notes within BUDGET_LIMIT 84 % at margin 1.00 with no hand-over charged (every cell of both tables)", okho);
#endif
    /* (a) erased unit, WAVE both, POLY, LAYER */
    boot();
    duell_mode_force(DM_LAYER);
    for (k = 0; k < 2; k++) { trk[k].eng_req = (uint8_t)eng_named("WAVE"); apply_preset_to(&trk[k], 0); trk[k].p[P_VOICE] = V_POLY; }
    frame();
    numbers(one, two);
    printf("polylimits: erased unit, WAVE both parts (factory sound 0 as is), POLY, LAYER, KEY SENSE on: one core %u + %u, two cores %u + %u\n", one[0], one[1], two[0], two[1]);
#ifndef OLD
    chk("(a) erased unit WAVE LAYER: one core at least 2 a part (item 116, COST-CAL rows: was 3)", one[0] >= 2u && one[1] >= 2u);
    chk("(a) .. two cores at least 5 a part and at least twice the one-core number (item 116: was 6)", two[0] >= 5u && two[1] >= 5u && two[0] >= 2u * one[0] && two[1] >= 2u * one[1]);
    /* (b) the counter, (c) own voices: the two-core numbers in force as budget_poll leaves them on the unit */
    budget_poll();
    for (k = 0; k < 2; k++) bnotes1[k] = (uint8_t)one[k], bnotes[k] = (uint8_t)two[k];
    bd_one_now = 0;
    {
        uint8_t n[2];
        uint32_t c, i;
        fm1_in.notes |= 1u << 12; host_notes |= 1u << 12;  /* one key down, through the key path (LAYER: both parts) */
        for (i = 0; i < 6; i++) { mix_block(o, CTL); frame(); for (k = 0; k < 2; k++) bnotes1[k] = (uint8_t)one[k], bnotes[k] = (uint8_t)two[k]; }
        c = head_notes(n);
        printf("polylimits: counter, two cores, one key down: %u capsules, %u + %u (numbers %u + %u, keys held %u + %u)\n", c, n[0], n[1], two[0], two[1], held(&trk[0]), held(&trk[1]));
        chk("(b) counter with one key down on two cores = each capsule its OWN part's number less its one key (UT_CAP_VOICES_OWN: two numbers in LAYER too; not 0)",
            c == 2u && held(&trk[0]) == 1u && held(&trk[1]) == 1u && n[0] == two[0] - 1u && n[1] == two[1] - 1u);
        bd_one_now = 1;
        c = head_notes(n);
        printf("polylimits: counter, jobs paused (one core), one key down: %u + %u\n", n[0], n[1]);
        chk("(b) .. with the jobs paused each capsule follows its own part's one-core number at once", c == 2u && n[0] == one[0] - 1u && n[1] == one[1] - 1u);
        bd_one_now = 0;
        for (i = 1; i < 8u; i++) {
            fm1_in.notes |= 1u << (12 + i); host_notes |= 1u << (12 + i);
            for (k = 0; k < 3; k++) { mix_block(o, CTL); frame(); }
            for (k = 0; k < 2; k++) bnotes1[k] = (uint8_t)one[k], bnotes[k] = (uint8_t)two[k];
        }
        printf("polylimits: 8 keys on both parts with two-core numbers: %u + %u sound\n", sounding(&trk[0]), sounding(&trk[1]));
        chk("(c) each part its own voices: both parts sound their whole two-core number at once", sounding(&trk[0]) == two[0] && sounding(&trk[1]) == two[1]);
        bd_one_now = 1;
        voice_trim(&trk[0]);   /* (the one-core build has no voice_trim_now; voice_trim fades the same voices: twocore_sim.c checks the instant one) */
        voice_trim(&trk[1]);
        for (i = 0; i < 4; i++) mix_block(o, CTL);
        printf("polylimits: the jobs pause: %u + %u left after the one-block fade\n", sounding(&trk[0]), sounding(&trk[1]));
        chk("(c) .. the jobs pause: the notes above the one-core number fade at once", sounding(&trk[0]) <= one[0] && sounding(&trk[1]) <= one[1]);
        bd_one_now = 0;
    }
#endif
    /* the erased unit's factory pair as it is (ANALOG ACID + FM6 FM6 PAD, a MORPH on each, matrix, SAT, sends), LAYER, key sense on (item 116) */
    boot();
    duell_mode_force(DM_LAYER);
    frame();
    numbers(one, two);
    printf("polylimits: erased unit factory pair (%s / %s) LAYER, KEY SENSE on: one core %u + %u, two cores %u + %u\n", ENGINES[eng_idx(trk[0].eng_req)]->name, ENGINES[eng_idx(trk[1].eng_req)]->name, one[0], one[1], two[0], two[1]);
    /* (d) M02 NIGHT ORGAN as stored */
    boot();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(2);
    frame();
    numbers(one, two);
    printf("polylimits: M02 as stored (%s / %s, voice modes %d / %d): one core %u + %u, two cores %u + %u\n", ENGINES[eng_idx(trk[0].eng_req)]->name, ENGINES[eng_idx(trk[1].eng_req)]->name,
           trk[0].p[P_VOICE], trk[1].p[P_VOICE], one[0], one[1], two[0], two[1]);
    printf("polylimits: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
