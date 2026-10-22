/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.25 (owner: the hide rule applies to modulation, its depths and destinations; docs/MOD_INERT_TABLE.md): the table of what a wave leaves without effect
 * (mod.c mod_shp_inert, mod_e_inert), proved by rendering. For each of the ten engines and every value of the slot that picks its wave (ANALOG WAVE, LOFI WAVE, TRIO
 * WAVE; the other engines their first sound), a held note:
 *   SHAPE  the shape input moved (AMP DEST SHP 63 and LFO 1 DEST SHP 63, the LFO running) against none: the render is bit for bit the same exactly where the table
 *          says SHAPE does nothing, and differs everywhere else
 *   SLOTS  TRIO's INT2 INT3 DTN PW and LOFI's DUTY turned from one end to the other: the same exactly where the table says so, different elsewhere
 *   LIST   the removed destinations (mod_dst_removed): the EQ gains on every engine; ANALOG WAVE, PHASE WAVE 1 / WAVE 2 / LINE, LOFI BITS / WAVE / ARP, TRIO WAVE /
 *          MODE, PHYS MODEL, NOISE MODE, FM6 ALG; not WHEEL's REG or ROTR, nor any other slot
 * Built and run by DUELL/host_tests.sh (name: modinert) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "mode_scn.h"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok || getenv("MODINERT_ALL"))
        printf("modinert: %-110s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}
#define NB 160u
static void clean(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(&trk[0].mdw, 0, sizeof trk[0].mdw);
    memset(mph, 0, sizeof mph);
    memset(&morph, 0, sizeof morph);
    memset(&trk[0].mxw, 0, sizeof trk[0].mxw);
    memset(ins, 0, sizeof ins);
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_ch, 0, sizeof midi_ch);
    memset(midi_owners, 0, sizeof midi_owners);
    memset(mchord, 0, sizeof mchord);
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(kb_chn, 0, sizeof kb_chn);
    memset(pan_x, 0, sizeof pan_x);
    memset(pan_g, 0, sizeof pan_g);
    memset(pan_gv, 0, sizeof pan_gv);
    memset(dist_x, 0, sizeof dist_x);
    memset(dist_live, 0, sizeof dist_live);
    memset(duck_st, 0, sizeof duck_st);
    memset(mod_goff, 0, sizeof mod_goff);
    for (i = 0; i < NLIVE; i++)
        mx_sl[i].ok = 0, mx_ok[i] = 0;
    memset(mx_was, 0, sizeof mx_was); memset(mod_mixmv, 0, sizeof mod_mixmv);
    trk[0].mgon = trk[1].mgon = trk[0].mdk = trk[1].mdk = 0, mod_gw = 0;
    mix_bal = 0;
    mix_xf = mix_split = 0;
    vage = 0;
    mod_seed = 0x2545F491u;
    fm1_in.notes = 0;
    kb_prev = 0;
    mi_r = mi_w = 0;
    duell_mode_force(DM_SPLIT);
    duell_mode_force(DM_STEREO);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    lim_env = lim_env_r = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    memset(&bm, 0, sizeof bm);
    host_tracks_init();
    song.g[G_RTYPE] = 0;
    song.g[G_RLOCUT] = 0;
    song.sel = 0;
    song.playing = 0;
    song.grid = 0;
}


/* a held chord-less note on part 1 of engine e, its first sound with wave slot key = w (key < 0: none), slot k at v (k < 0: none), shp: the shape input moved; the hash */
static uint64_t render(uint32_t e, int key, int w, int k, int v, int shp)
{
    static int32_t o[2 * CTL];
    track_t *t = &trk[0];
    uint64_t h = 1469598103934665603ull;
    uint32_t b, i;
    clean();
    host_preset(t, e, 0);
    if (key >= 0)
        t->p[P_E0 + key] = (int16_t)w;
    if (k >= 0)
        t->p[P_E0 + k] = (int16_t)v;
    t->p[P_VOICE] = V_MONO;
    t->p[P_SPRD] = 0;
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    t->p[P_ED_SHP] = (int16_t)(shp ? 63 : 0);
    t->p[P_LD_SHP] = (int16_t)(shp ? 63 : 0);
    t->p[P_LRATE] = 70;
    t->p[P_LFADE] = 0;
    t->p[P_LSYNC] = 0;
    for (b = 0; b < NB; b++) {
        if (b == 2u)
            trk_note_on(t, 57, 110);
        mix_block(o, CTL);
        if (b >= 8u)
            for (i = 0; i < CTL; i++) { h ^= (uint32_t)(mix_l[i] + mix_r[i]); h *= 1099511628211ull; }
    }
    return h;
}

int main(void)
{
    static const char *const EN[10] = {"ANALOG", "PHASE", "LOFI", "VOICE", "TRIO", "WHEEL", "PHYS", "NOISE", "FM6", "WAVE"};
    char line[200];
    uint32_t j, nshp = 0, nslot = 0, okall;
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    for (j = 0; j < 10u; j++) {
        uint32_t e = scn_eng(EN[j]);
        const engine_t *en = ENGINES[e];
        int key = en == &ENG_LOFI ? 1 : (en == &ENG_ANALOG || en == &ENG_TRIO) ? 0 : -1;
        int w0 = key < 0 ? 0 : en->edit[key].min, w1 = key < 0 ? 0 : en->edit[key].max, w;
        for (w = w0; w <= w1; w++) {
            track_t q;
            int inert, k;
            uint64_t a, b;
            memset(&q, 0, sizeof q);
            q.eng_req = (uint8_t)e;
            q.engine = (uint8_t)e;
            if (key >= 0)
                q.p[P_E0 + key] = (int16_t)w;
            inert = mod_shp_inert(&q);
            a = render(e, key, w, -1, 0, 0);
            b = render(e, key, w, -1, 0, 1);
            snprintf(line, sizeof line, "SHAPE %s %s %s: the table says %s, the render %s", EN[j], key >= 0 ? en->edit[key].label : "", key >= 0 ? en->edit[key].names[w] : "",
                     inert ? "no effect" : "an effect", a == b ? "the same" : "different");
            expect(line, inert == (a == b));
            nshp++;
            for (k = 0; k < 8; k++) {
                const param_desc_t *d = &en->edit[k];
                if (k == key || !(en == &ENG_TRIO ? (k == 1 || k == 2 || k == 3 || k == 7) : en == &ENG_LOFI ? k == 2 : 0))
                    continue;
                inert = mod_e_inert(&q, (uint32_t)k);
                a = render(e, key, w, k, d->min, 0);
                b = render(e, key, w, k, d->max, 0);
                snprintf(line, sizeof line, "SLOT %s %s %s: %s the table says %s, the render %s", EN[j], en->edit[key].label, en->edit[key].names[w], d->label,
                         inert ? "no effect" : "an effect", a == b ? "the same" : "different");
                expect(line, inert == (a == b));
                nslot++;
            }
        }
    }
    printf("modinert: SHAPE checked on %u engine / wave pairs, the slots on %u\n", nshp, nslot);
    {   /* LIST */
        static const struct { const char *e; uint32_t m; } RM[10] = {{"ANALOG", 0x01}, {"PHASE", 0x23}, {"LOFI", 0x43}, {"VOICE", 0}, {"TRIO", 0x11}, {"WHEEL", 0},
                                                                    {"PHYS", 0x01}, {"NOISE", 0x01}, {"FM6", 0x01}, {"WAVE", 0}};
        int32_t d;
        for (j = 0, okall = 1; j < 10u; j++) {
            uint32_t e = scn_eng(RM[j].e), k;
            okall &= mod_dst_removed(e, MD_EQLO) && mod_dst_removed(e, MD_EQMID) && mod_dst_removed(e, MD_EQHI) && mod_dst_removed(e, MD_RSIZE) && mod_dst_removed(e, MD_DEPTH) && mod_dst_removed(e, MD_DEPTH2);
            for (k = 0; k < 8u; k++)
                okall &= mod_dst_removed(e, MD_E1 + (int32_t)k) == (int)((RM[j].m >> k) & 1u);
            for (d = 1; d < MD_N; d++)
                if (!MD_ENGINE(d) && d != MD_EQLO && d != MD_EQMID && d != MD_EQHI && d != MD_RSIZE && d != MD_DEPTH && d != MD_DEPTH2)
                    okall &= !mod_dst_removed(e, d);
        }
        expect("LIST: removed: the EQ gains, REVERB SIZE (T0229) and the LFO depths DEPTH / DEPTH2 (1.0) on every engine and exactly the list engine slots named (WHEEL REG and ROTR stay)", okall);
        expect("LIST: the slots named are the ones the engines call so", str_eq(ENGINES[scn_eng("PHASE")]->edit[5].label, "LINE") && str_eq(ENGINES[scn_eng("LOFI")]->edit[6].label, "ARP") &&
               str_eq(ENGINES[scn_eng("TRIO")]->edit[0].label, "WAVE") && str_eq(ENGINES[scn_eng("FM6")]->edit[0].label, "ALG") && str_eq(ENGINES[scn_eng("WHEEL")]->edit[7].label, "ROTR"));
    }
    printf("modinert: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
