/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL ARP TARGET (SPEC 2.3, P_ATGT; seq.c atg_fire, voice.c atg_voice, the engines' units), same sources as the firmware
 * (hostsim.c). build/host/atgt_test (host_tests.sh atgt):
 * 1. ANALOG mask B: one voice for the held key, OSC A (MIX 0, sine) measured at the root on every arp step, OSC B (MIX 127)
 *    at the arp's note (zero crossings over each step).
 * 2. FM6 mask OP2: the carrier OP1's frequency stays the root's on every step, OP2's follows the arp (x2 an octave up).
 * 3. ALL: the classic arp (a voice per arp note, no root voice).
 * 4. old stores: id 86 retired for Felucca's layout (today's default ALL), the default ALL, a short store -> ALL.
 * (Mask ALL bit for bit as before: regress's golden renders.) */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("atgt: %-90s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n))
            return e;
    return 0;
}
static void fresh(uint32_t e)
{
    uint32_t k;
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(rev_ap, 0, sizeof rev_ap);
    memset(&fx, 0, sizeof fx);
    vage = 0;
    rng_state = 0x1234567u;
    memset(trk, 0, sizeof trk);
    memset(&mod, 0, sizeof mod);
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], k ? 0u : e, 0);
    for (k = 1; k < NPART; k++)
        trk[k].p[P_MUTE] = 1;
    song.sel = 0;
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
}
static void arp_setup(track_t *t, int32_t mask)
{
    t->p[P_VOICE] = V_POLY;
    t->p[P_AMODE] = AM_UP;
    t->p[P_AOCT] = 2;                                 /* one key: root, root + 12, root, ... */
    t->p[P_AGATE] = 127;
    t->p[P_APROB] = 127;
    t->p[P_ARATE] = 3;
    t->p[P_GLIDE] = 0;
    t->p[P_ATGT] = (int16_t)mask;
}

/* segments of one arp note: the frequency (Hz) by rising zero crossings, after 15 ms of the step */
#define NSEG 8
static uint32_t seg_note[NSEG], nseg;
static double seg_hz[NSEG];
static void measure(track_t *t, uint32_t nblocks)
{
    static int32_t o[2 * CTL];
    uint32_t b, i, note = 0, since = 0, cross = 0, samples = 0, first = 0, last = 0;
    int32_t prev = 0;
    nseg = 0;
    for (b = 0; b < nblocks; b++) {
        mix_block(o, CTL);
        if (t->atg_note != note) {
            if (note && nseg < NSEG && cross > 2u) {
                seg_note[nseg] = note;
                seg_hz[nseg++] = (double)(cross - 1u) * FS / (double)(last - first);
            }
            note = t->atg_note;
            since = cross = 0;
        }
        for (i = 0; i < CTL; i++, samples++) {
            int32_t x = o[2u * i];
            if (since > FS * 15u / 1000u && prev < 0 && x >= 0) {
                if (!cross)
                    first = samples;
                last = samples;
                cross++;
            }
            prev = x;
            since++;
        }
    }
}
static int near(double a, double b) { return a > b * 0.98 && a < b * 1.02; }
static double hz(uint32_t note) { return 440.0 * pow(2.0, ((double)note - 69.0) / 12.0); }

static void analog_b(void)
{
    uint32_t e = eng_by_name("ANALOG"), i, nv, ok, moves, mix;
    for (mix = 0; mix < 2u; mix++) {
        track_t *t = &trk[0];
        fresh(e);
        t->p[P_E0] = 3;                               /* SIN */
        t->p[P_E1] = 0;                               /* DTN 0 */
        t->p[P_E2] = mix ? 127 : 0;                   /* MIX: OSC A alone / OSC B alone */
        t->p[P_E3] = t->p[P_E5] = t->p[P_E6] = t->p[P_E7] = 0;
        t->p[P_E4] = 127;
        t->p[P_SUS] = 127;
        arp_setup(t, 2);                              /* B */
        input_on(t, 60, 100);
        measure(t, FS * 3u / CTL);
        for (i = nv = 0; i < NVOICE; i++)
            nv += t->v[i].active && t->v[i].gate;
        for (i = 0, ok = nseg >= 4u, moves = 0; i < nseg; i++) {
            ok &= near(seg_hz[i], hz(mix ? seg_note[i] : 60));
            moves += seg_note[i] == 72u;
            if (!near(seg_hz[i], hz(mix ? seg_note[i] : 60)))
                printf("  seg %u note %u: %.1f Hz\n", i, seg_note[i], seg_hz[i]);
        }
        check(mix ? "ANALOG mask B: OSC B on the arp's note every step (60 / 72)"
                  : "ANALOG mask B: OSC A on the root (60) every step, one voice for the key",
              ok && moves >= 2u && (mix || (nv == 1u && t->v[0].note == 60)));
        input_off(t, 60);
    }
}

static void fm6_op2(void)
{
    uint32_t e = eng_by_name("FM6"), b, i, ok = 1, n60 = 0, n72 = 0;
    int32_t car = 0, m60 = 0, m72 = 0;
    track_t *t = &trk[0];
    static int32_t o[2 * CTL];
    fresh(e);
    arp_setup(t, 2);                                  /* OP2: patch index 4 (OP1 = 5) */
    input_on(t, 60, 100);
    for (b = 0; b < FS * 2u / CTL; b++) {
        fm6_note_t *n;
        mix_block(o, CTL);
        for (i = 0; i < NVOICE && !(t->v[i].active && t->v[i].gate); i++)
            ;
        if (i == NVOICE || !(n = fm6_note_of(t, &t->v[i])) || !n->live)
            continue;
        if (!car)
            car = n->op[5].freq;
        ok &= n->op[5].freq == car;                   /* (no LFO / pitch env in the patch: exactly) */
        if (t->atg_note == 60) { m60 = n->op[4].freq; n60++; }
        if (t->atg_note == 72) { m72 = n->op[4].freq; n72++; }
    }
    if ((fm6_eff[0].p[4 * FP_OP + FP_MODE] & 1u))
        printf("  (OP2 of this patch is fixed-frequency)\n");
    printf("  OP1 %d, OP2 at 60 %d, at 72 %d (ratio %.4f)\n", car, m60, m72, m60 ? (double)m72 / m60 : 0.0);
    check("FM6 mask OP2: OP1 (carrier) constant on the root, OP2 x2 on the arp's octave",
          ok && car && n60 && n72 && m60 && (double)m72 / m60 > 1.99 && (double)m72 / m60 < 2.01);
    input_off(t, 60);
}

static void classic(void)
{
    uint32_t b, i, seen72 = 0, ok = 1;
    track_t *t = &trk[0];
    static int32_t o[2 * CTL];
    fresh(eng_by_name("ANALOG"));
    arp_setup(t, 0);                                  /* ALL */
    input_on(t, 60, 100);
    for (b = 0; b < FS * 2u / CTL; b++) {
        mix_block(o, CTL);
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].gate) {
                seen72 |= t->v[i].note == 72;
                ok &= t->v[i].note == t->arp_note;
            }
        ok &= !t->atg_note;
    }
    check("ALL: the classic arp (the arp's notes as voices, no root voice held)", ok && seen72);
    input_off(t, 60);
}

static void stores(void)
{
    /* (0.2.14: stores of Felucca's layout are no longer read, so nothing maps P_ATGT on id 86, DRUM's lane in that layout, by count) */
    check("P_ATGT on id 86: default ALL (0), range 0..8", P_ATGT == 86 && TP[P_ATGT].def == 0 && TP[P_ATGT].max == 8);
}

int main(void)
{
    analog_b();
    fm6_op2();
    classic();
    stores();
    printf("atgt: %s\n", bad ? "FAIL" : "all ok");
    return bad != 0;
}
