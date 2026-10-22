/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.21: the STEREO CHORUS (fx_bus.c cho_tap; G_CRATE RATE, G_CDEPTH DEPTH). On mode_test.c's rig.
 *   build/host/stereochorus_test   (DUELL/host_tests.sh "stereochorus")
 * Before: the chorus bus was ONE modulated tap (5 .. 15 ms), summed into the bus's mid. Now, in STEREO and LAYER (the bus has a side), two taps of the
 * same line, the right one CHO_OFF (a quarter turn) behind the left in the LFO: the mid goes to the wet bus, half the difference as a side to the left and
 * from the right of the dry mix (like PING-PONG's, not through SPREAD). DUEL (SPLIT: each part's own side) and the chorus of a bus without a side: the one tap, as before.
 *   BUS      the bus alone (fx_buses; plain delay, the delay's modulation, PING-PONG, RANDOM: every path that has a chorus loop), mono pulses and noise in:
 *            the left and right wet differ (correlation < 0.6), the left is the old mono chorus (within 1 LSB), the mid (= the old sum's wet) is bounded and
 *            still moves with the chorus (differs from the DEPTH 0 mid); DEPTH 0: L == R == the old chorus bit for bit, no side
 *   SPLIT    DUEL's buses A and B (fx_buses_m / _mod and the _b ones, no side): the old mono chorus bit for bit, A == B, nothing on the dry mix
 *   MIX      the whole mix_block, a held chord: STEREO and LAYER wide (L != R), DUEL one part alone: the other channel exactly 0 in every sample; chorus send 0 and
 *            DUEL render bit for bit what 0.2.20 rendered (hashes printed by this file built on that commit with -DSC_BASE=1), exact silence after the tails
 *   CLICK    RATE and DEPTH changed in jumps: no sample step above the steady chorus's own
 *   SPACE    PERFORM's SPACE +100 with the stereo chorus: bounded, the level within 1.5 dB of the mono chorus's
 *   COST     host instructions per sample of the bus stage, mono (DUEL's tap) against stereo */
#define MODE_TEST_NO_MAIN 1
#include "mode_test.c"

/* what 0.2.20 rendered (build this file on c755ca0 with -DSC_BASE=1 to print them) */
#define H_ST_SEND0 0x95e535e6638a3a73ull
#define H_ST_DEPTH0 0xb8a39b104ccea803ull
#define H_DUEL_CHO 0xec69d9e302f65abdull

static uint64_t sc_fnv(const int32_t *x, uint32_t n, uint64_t h)
{
    for (uint32_t i = 0; i < n; i++) {
        h ^= (uint32_t)x[i];
        h *= 1099511628211ull;
    }
    return h;
}
static uint32_t sc_xs = 0x2545F491u;
static int32_t sc_rnd(int32_t amp)
{
    sc_xs ^= sc_xs << 13;
    sc_xs ^= sc_xs >> 17;
    sc_xs ^= sc_xs << 5;
    return (int32_t)(sc_xs >> 8) % (2 * amp + 1) - amp;
}

/* ----------------------------------------------------- the bus alone --- */
#define BN (3u * FS)                                    /* samples of a bus run */
static int32_t bL[BN], bR[BN], bM[BN], bin[BN];
enum { P_PLAIN, P_MOD, P_PING, P_RAND, P_OLD, P_OLDMOD };
static void bus_cfg(uint32_t mode, int depth, int rate)
{
    dsp_clean(mode);
    song.g[G_CDEPTH] = (int16_t)depth;
    song.g[G_CRATE] = (int16_t)rate;
    song.g[G_DMIX] = 0;
    song.g[G_DFDBK] = 0;
    song.g[G_DMDEP] = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    song.g[G_RTYPE] = 0;
    song.g[G_RLOCUT] = 0;
    song.g[G_PINGSPREAD] = 100;
    song.g[G_DTIME] = 1;
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    dly_plain_pp = 0;
    dlyev_clear();
    if (mode == DM_SPLIT)
        return;
}
static void bus_set_path(uint32_t path)
{
    song.g[G_DMDEP] = path == P_MOD || path == P_OLDMOD ? 50 : 0;
    song.g[G_DMODE] = (int16_t)(path == P_PING ? DMD_PING : path == P_RAND ? DMD_RAND : DMD_DIGITAL);
    if (path == P_RAND)
        song.g[G_DMDEP] = 0;                            /* (CHANCE 0: the plain echo, the chorus loop of dly_ev) */
}
/* n samples of bin through the chorus bus on path: bL, bR the two channels as mix_block's loop adds them (wet + the dry side), bM the wet */
static void bus_run(uint32_t path, uint32_t n)
{
    static int32_t z[CTL], w[CTL];
    uint32_t t, i;
    bus_set_path(path);
    memset(z, 0, sizeof z);
    for (t = 0; t + CTL <= n; t += CTL) {
        for (i = 0; i < CTL; i++)
            mix_l[i] = mix_r[i] = w[i] = 0;
        if (path == P_OLD)
            fx_buses_m(DLY_LEN - 1u, delay_samples(), bin + t, z, z, w, CTL, 0u, 0);
        else if (path == P_OLDMOD)
            fx_buses_mod(DLY_LEN - 1u, delay_samples(), bin + t, z, z, w, CTL, 0u, 0);
        else
            fx_buses(bin + t, z, z, w, CTL);
        for (i = 0; i < CTL; i++) {
            bL[t + i] = w[i] + mix_l[i];
            bR[t + i] = w[i] + mix_r[i];
            bM[t + i] = w[i];
        }
    }
}
static double sc_corr(const int32_t *a, const int32_t *b, uint32_t n)
{
    double sab = 0, saa = 0, sbb = 0;
    for (uint32_t i = 0; i < n; i++) {
        sab += (double)a[i] * b[i];
        saa += (double)a[i] * a[i];
        sbb += (double)b[i] * b[i];
    }
    return saa > 0 && sbb > 0 ? sab / sqrt(saa * sbb) : 1.0;
}
static double sc_rms(const int32_t *a, uint32_t n)
{
    double s = 0;
    for (uint32_t i = 0; i < n; i++)
        s += (double)a[i] * a[i];
    return sqrt(s / n);
}
static void fill_noise(uint32_t n, int32_t amp)
{
    for (uint32_t i = 0; i < n; i++)
        bin[i] = sc_rnd(amp);
}
static void fill_pulses(uint32_t n, uint32_t period, int32_t amp)
{
    for (uint32_t i = 0; i < n; i++)
        bin[i] = i % period == 0 ? amp : 0;
}
static int32_t g_oldL[BN], g_midD0[BN];
static const char *const PNAME[] = {"plain", "delay mod", "PING-PONG", "RANDOM"};
static int sc_bounded(const int32_t *x, uint32_t n, int32_t lim)
{
    for (uint32_t i = 0; i < n; i++)
        if (x[i] > lim || x[i] < -lim)
            return 0;
    return 1;
}
static void sc_bus(void)
{
    uint32_t path, md, i;
    for (md = DM_STEREO; md <= DM_LAYER; md++) {
        for (path = P_PLAIN; path <= P_RAND; path++) {
            double cn, cp, mono_move, lr_old = 0;
            int32_t pk_mid, pk_sideL;
            char nm[160];
            /* mono pulses, DEPTH 127, RATE 44 */
            bus_cfg(md, 127, 44);
            fill_pulses(BN, 1500, 16000);
            bus_run(path, BN);
            cp = sc_corr(bL, bR, BN);
            pk_mid = peak_of(bM, BN);
            snprintf(nm, sizeof nm, "BUS %s %s: a mono pulse train, DEPTH 127: left and right differ (correlation %.2f < 0.6)", md == DM_STEREO ? "STEREO" : "LAYER", PNAME[path], cp);
            expect(nm, cp < 0.6);
            bus_cfg(md, 60, 70);
            fill_pulses(BN, 1500, 16000);
            bus_run(path, BN);
            snprintf(nm, sizeof nm, "BUS %s %s: DEPTH 60, RATE 70 (correlation %.2f < 0.6)", md == DM_STEREO ? "STEREO" : "LAYER", PNAME[path], sc_corr(bL, bR, BN));
            expect(nm, sc_corr(bL, bR, BN) < 0.6);
            /* noise: the sum bounded, the old mono chorus is the left (+-1 LSB), the mid still moves */
            bus_cfg(md, 127, 44);
            sc_xs = 0x2545F491u;
            fill_noise(BN, 12000);
            bus_run(path, BN);
            cn = sc_corr(bL, bR, BN);
            pk_sideL = peak_of(bL, BN);
            expect("  noise 12000: L, R, L + R bounded (|L| |R| <= 12000 x 1.05, the mid <= the input)", sc_bounded(bL, BN, 12600) && sc_bounded(bR, BN, 12600) && pk_mid <= 12000 + 2 && pk_sideL > 0);
            memcpy(g_oldL, bL, sizeof bL);              /* (the wide render, kept for the mono comparison below) */
            {
                static int32_t wide_m[BN];
                memcpy(wide_m, bM, sizeof bM);
                if (path == P_PLAIN || path == P_MOD) {
                    int32_t worst = 0;
                    bus_cfg(md, 127, 44);
                    sc_xs = 0x2545F491u;
                    fill_noise(BN, 12000);
                    bus_run(path == P_PLAIN ? P_OLD : P_OLDMOD, BN);
                    for (i = 0; i < BN; i++) {
                        int32_t d = g_oldL[i] - bL[i];
                        d = d < 0 ? -d : d;
                        worst = d > worst ? d : worst;
                    }
                    expect("  the left channel is the old mono chorus (DUEL's tap) within 1 LSB", worst <= 1);
                    lr_old = sc_rms(bL, BN);
                    (void)lr_old;
                }
                /* the mid still has the chorus: it differs from the DEPTH 0 mid by a good share of its level */
                bus_cfg(md, 0, 44);
                sc_xs = 0x2545F491u;
                fill_noise(BN, 12000);
                bus_run(path, BN);
                memcpy(g_midD0, bM, sizeof bM);
                {
                    static int32_t dd[BN];
                    for (i = 0; i < BN; i++)
                        dd[i] = wide_m[i] - g_midD0[i];
                    mono_move = sc_rms(dd, BN) / sc_rms(g_midD0, BN);
                }
                snprintf(nm, sizeof nm, "  the mid (the mono sum's chorus) differs from the DEPTH 0 mid: rms %.2f of its level (> 0.3); L/R correlation of noise %.2f", mono_move, cn);
                expect(nm, mono_move > 0.3);
                /* DEPTH 0: both taps equal: the side is exactly 0, the mid the old chorus */
                {
                    int32_t zside = 0;
                    for (i = 0; i < BN; i++)
                        zside |= bL[i] - bR[i];
                    expect("  DEPTH 0: L == R exactly (no side)", zside == 0);
                }
            }
        }
        /* DEPTH 0 against the old mono chorus, bit for bit (plain and modulated bus; the other paths agree with the plain one) */
        for (path = P_PLAIN; path <= P_MOD; path++) {
            static int32_t refm[BN];
            bus_cfg(md, 0, 44);
            sc_xs = 0x2545F491u;
            fill_noise(BN, 12000);
            bus_run(path == P_PLAIN ? P_OLD : P_OLDMOD, BN);
            memcpy(refm, bM, sizeof bM);
            bus_cfg(md, 0, 44);
            sc_xs = 0x2545F491u;
            fill_noise(BN, 12000);
            bus_run(path, BN);
            expect("  DEPTH 0: the whole wet is the old chorus's bit for bit", !memcmp(refm, bM, sizeof bM) && !memcmp(refm, bL, sizeof bL) && !memcmp(refm, bR, sizeof bR));
        }
    }
    /* silence after the tails: the line holds 46 ms, then exact zeros, every path */
    for (path = P_PLAIN; path <= P_RAND; path++) {
        int32_t nz = 0;
        bus_cfg(DM_STEREO, 127, 80);
        sc_xs = 0x2545F491u;
        for (i = 0; i < BN; i++)
            bin[i] = i < FS ? sc_rnd(12000) : 0;
        bus_run(path, BN);
        for (i = FS + 4096u; i < BN; i++)
            nz |= bL[i] | bR[i];
        printf("stereochorus: silence after the input stops (%s): last 1.9 s %s\n", PNAME[path], nz ? "NOT zero" : "exactly zero");
        expect("  the tails end in exact silence (left, right)", nz == 0);
    }
}

/* ------------------------------------------------ DUEL's buses A and B --- */
static void sc_split(void)
{
    static int32_t za[BN], zb[BN], ref[BN];
    static int32_t z[CTL];
    uint32_t t, i, v;
    int ok = 1, eq = 1, dry = 1, nz = 0;
    for (v = 0; v < 2u; v++) {                           /* 0: the plain bus, 1: the delay's modulation running */
        uint32_t dl;
        bus_cfg(DM_SPLIT, 127, 44);
        song.g[G_DMDEP] = (int16_t)(v ? 50 : 0);
        sc_xs = 0x2545F491u;
        fill_noise(BN, 12000);
        dl = delay_split();
        memset(z, 0, sizeof z);
        for (t = 0; t + CTL <= BN; t += CTL) {
            static int32_t w[CTL];
            for (i = 0; i < CTL; i++)
                mix_l[i] = mix_r[i] = 0;
            if (v)
                fx_buses_mod(DLY_HALF - 1u, dl, bin + t, z, z, w, CTL, 0u, 0);
            else
                fx_buses_m(DLY_HALF - 1u, dl, bin + t, z, z, w, CTL, 0u, 0);
            memcpy(za + t, w, sizeof w);
            for (i = 0; i < CTL; i++)
                dry &= mix_l[i] == 0 && mix_r[i] == 0;
            if (v)
                fx_buses_mod_b(DLY_HALF - 1u, dl, bin + t, z, z, w, CTL, 0u, 0);
            else
                fx_buses_m_b(DLY_HALF - 1u, dl, bin + t, z, z, w, CTL, 0u, 0);
            memcpy(zb + t, w, sizeof w);
            for (i = 0; i < CTL; i++)
                dry &= mix_l[i] == 0 && mix_r[i] == 0;
        }
        for (i = 0; i < BN; i++)
            eq &= za[i] == zb[i], nz |= za[i];
        /* the same as the stereo bus's old tap, from the same state (the whole-line bus, dsd 0) */
        bus_cfg(DM_STEREO, 127, 44);
        sc_xs = 0x2545F491u;
        fill_noise(BN, 12000);
        bus_run(v ? P_OLDMOD : P_OLD, BN);
        memcpy(ref, bM, sizeof bM);
        for (i = 0; i < BN; i++)
            ok &= ref[i] == za[i];
    }
    expect("SPLIT: buses A and B (plain and with the delay's modulation) give the same chorus, it sounds, and it is the old mono tap bit for bit", eq && nz && ok);
    expect("  .. and nothing goes onto the dry mix (no side in DUEL)", dry);
}

/* ------------------------------------------------------- the whole mix --- */
static void sc_chord(uint32_t mode, uint32_t part, int depth, int rate, int send, uint32_t hold_blocks, uint32_t tail_blocks, uint32_t both)
{
    static const uint8_t CH[3] = {52, 59, 64};
    uint32_t k;
    dsp_clean(mode);
    song.g[G_CDEPTH] = (int16_t)depth;
    song.g[G_CRATE] = (int16_t)rate;
    song.g[G_RTYPE] = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    song.g[G_DMDEP] = 0;
    for (k = 0; k < 2u; k++) {
        host_preset(&trk[k], eng_of("ANALOG"), 0);
        trk[k].p[P_TRIM] = 100;
        trk[k].p[P_PAN] = 0;
        trk[k].p[P_SUS] = 127;
        trk[k].p[P_REL] = 40;
        trk[k].p[P_AMODE] = 0;
        trk[k].p[P_CHOR] = (int16_t)send;
        trk[k].p[P_DLY] = trk[k].p[P_REV] = 0;
    }
    rec_n = 0;
    mblk(8);
    for (k = 0; k < 3u; k++) {
        trk_note_on(&trk[part], CH[k], 110);
        if (both)
            trk_note_on(&trk[1u - part], CH[k] + 5u, 110);
    }
    mblk(hold_blocks);
    for (k = 0; k < 3u; k++) {
        trk_note_off(&trk[part], CH[k]);
        if (both)
            trk_note_off(&trk[1u - part], CH[k] + 5u);
    }
    mblk(tail_blocks);
}
static uint32_t sc_1s(void) { return FS / CTL; }
static void sc_mix(void)
{
    uint32_t md, part;
    for (md = DM_STEREO; md <= DM_LAYER; md++) {
        double c;
        int32_t last = 0;
        uint32_t i;
        sc_chord(md, 0, 127, 44, 127, sc_1s() * 3u / 2u, 3u * sc_1s(), 0);
        c = sc_corr(rec_l, rec_r, rec_n);
        printf("stereochorus: %s, a held ANALOG chord, CHO send 127 DEPTH 127 RATE 44: L/R correlation %.3f, peak L %d R %d\n", md == DM_STEREO ? "STEREO" : "LAYER", c, peak_of(rec_l, rec_n), peak_of(rec_r, rec_n));
        {
            double dcorr;
            sc_chord(md, 0, 127, 44, 0, sc_1s() * 3u / 2u, 3u * sc_1s(), 0);
            dcorr = sc_corr(rec_l, rec_r, rec_n);
            expect("MIX: with the chorus send on, left and right differ more than the dry chord's own", c < dcorr);
        }
        {
            double mx;
            static int32_t sm[MAXB * CTL];
            sc_chord(md, 0, 127, 44, 127, sc_1s() * 3u / 2u, 3u * sc_1s(), 0);
            for (i = 0; i < rec_n; i++)
                sm[i] = (rec_l[i] + rec_r[i]) / 2;
            mx = sc_rms(sm, rec_n);
            sc_chord(md, 0, 0, 44, 127, sc_1s() * 3u / 2u, 3u * sc_1s(), 0);
            for (i = 0; i < rec_n; i++)
                sm[i] = (rec_l[i] + rec_r[i]) / 2 - sm[i];
            printf("stereochorus: %s mono sum (L + R) / 2 of the chord: rms %.0f; the chorus's movement in it (against DEPTH 0): %.0f rms\n", md == DM_STEREO ? "STEREO" : "LAYER", mx, sc_rms(sm, rec_n));
            expect("MIX: the mono sum still has the chorus (it differs from the DEPTH 0 sum)", sc_rms(sm, rec_n) > 0.05 * mx);
        }
        sc_chord(md, 0, 127, 44, 127, sc_1s() * 3u / 2u, 4u * sc_1s(), 0);
        for (i = rec_n - 4096u; i < rec_n; i++)
            last |= rec_l[i] | rec_r[i];
        expect("MIX: exact silence at the end of the tails (4 s after the release)", last == 0);
    }
    /* DUEL isolation: one part alone, chorus on, every sample of the other channel 0 */
    for (part = 0; part < 2u; part++) {
        uint32_t i;
        int32_t oth = 0;
        sc_chord(DM_SPLIT, part, 127, 44, 127, sc_1s() * 3u / 2u, 2u * sc_1s(), 0);
        for (i = 0; i < rec_n; i++)
            oth |= (part ? rec_l[i] : rec_r[i]);
        printf("stereochorus: DUEL part %u alone, chorus 127: own peak %d, the other channel %s\n", part + 1u, peak_of(part ? rec_r : rec_l, rec_n), oth ? "LEAKS" : "exactly 0");
        expect("DUEL: the other channel is exactly 0 in every sample", oth == 0 && peak_of(part ? rec_r : rec_l, rec_n) > 500);
    }
    {
        uint64_t h1, h2, h3;
        sc_chord(DM_STEREO, 0, 80, 44, 0, sc_1s(), sc_1s(), 0);
        h1 = sc_fnv(rec_r, rec_n, sc_fnv(rec_l, rec_n, 1469598103934665603ull));
        sc_chord(DM_STEREO, 0, 0, 44, 127, sc_1s(), sc_1s(), 0);
        h2 = sc_fnv(rec_r, rec_n, sc_fnv(rec_l, rec_n, 1469598103934665603ull));
        sc_chord(DM_SPLIT, 0, 90, 44, 127, sc_1s(), sc_1s(), 1);
        h3 = sc_fnv(rec_r, rec_n, sc_fnv(rec_l, rec_n, 1469598103934665603ull));
        printf("stereochorus: hashes: STEREO send 0 %016llx, STEREO DEPTH 0 %016llx, DUEL both parts %016llx\n", (unsigned long long)h1, (unsigned long long)h2, (unsigned long long)h3);
#ifdef SC_BASE
        return;
#else
        expect("0.2.20 compatibility: the chorus send 0 render is bit for bit as before", h1 == H_ST_SEND0);
        expect("  .. STEREO with DEPTH 0 (both taps equal) is bit for bit as before", h2 == H_ST_DEPTH0);
        expect("  .. DUEL (the one tap) is bit for bit as before", h3 == H_DUEL_CHO);
#endif
    }
}

/* ------------------------------------------------- no click, SPACE, cost --- */
static int32_t sc_step(const int32_t *x, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    for (uint32_t i = a + 1u; i < b; i++) {
        int32_t d = x[i] - x[i - 1u];
        m = (d < 0 ? -d : d) > m ? (d < 0 ? -d : d) : m;
    }
    return m;
}
static void sine_in(uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        bin[i] = (int32_t)(9000.0 * sin(6.283185307 * 440.0 * (double)i / 44100.0));
}
/* RATE / DEPTH moved on path, as a walk (a step every 2 blocks, gliding: a knob turned by hand) or in jumps (8 settings, one every 0.375 s); the largest sample step */
static void click_run(uint32_t path, uint32_t jumps, uint32_t n, int32_t *sl, int32_t *sr)
{
    static int32_t z[CTL], w[CTL];
    const int D[8] = {0, 127, 20, 127, 0, 90, 5, 127}, R[8] = {44, 44, 120, 3, 60, 127, 0, 44};
    uint32_t t, i;
    bus_cfg(DM_STEREO, 0, 44);
    bus_set_path(path == P_OLD ? P_PLAIN : path);
    sine_in(n);
    memset(z, 0, sizeof z);
    for (t = 0; t + CTL <= n; t += CTL) {
        uint32_t seg = t / (n / 8u), b = t / CTL;
        if (jumps) {
            song.g[G_CDEPTH] = (int16_t)D[seg > 7u ? 7u : seg];
            song.g[G_CRATE] = (int16_t)R[seg > 7u ? 7u : seg];
        } else {
            song.g[G_CDEPTH] = (int16_t)(b / 2u % 254u < 127u ? b / 2u % 254u : 253u - b / 2u % 254u);   /* up and down 0 .. 127 */
            song.g[G_CRATE] = (int16_t)(b / 3u % 254u < 127u ? b / 3u % 254u : 253u - b / 3u % 254u);
        }
        for (i = 0; i < CTL; i++)
            mix_l[i] = mix_r[i] = w[i] = 0;
        if (path == P_OLD)
            fx_buses_m(DLY_LEN - 1u, delay_samples(), bin + t, z, z, w, CTL, 0u, 0);
        else
            fx_buses(bin + t, z, z, w, CTL);
        for (i = 0; i < CTL; i++) {
            bL[t + i] = w[i] + mix_l[i];
            bR[t + i] = w[i] + mix_r[i];
        }
    }
    *sl = sc_step(bL, 1000, n);
    *sr = sc_step(bR, 1000, n);
}
static void sc_click(void)
{
    uint32_t n = 3u * FS, jumps;
    int32_t steady_l, steady_r, jl, jr, ol, orr, dummy;
    bus_cfg(DM_STEREO, 127, 120);                       /* the steady chorus at the largest slope: DEPTH 127, RATE 120 */
    sine_in(n);
    bus_run(P_PLAIN, n);
    steady_l = sc_step(bL, FS / 4u, n);
    steady_r = sc_step(bR, FS / 4u, n);
    for (jumps = 0; jumps < 2u; jumps++) {
        click_run(P_PLAIN, jumps, n, &jl, &jr);
        click_run(P_OLD, jumps, n, &ol, &orr);
        (void)dummy;
        printf("stereochorus: click, %s: largest sample step: stereo L %d R %d, the old mono chorus %d (steady DEPTH 127 RATE 120: L %d R %d)\n",
               jumps ? "RATE and DEPTH jumping 8 times" : "RATE and DEPTH walked up and down", jl, jr, ol, steady_l, steady_r);
        if (jumps)
            expect("CLICK: DEPTH / RATE in jumps: no step above the old mono chorus's (x 1.1 + 20)", jl <= ol * 11 / 10 + 20 && jr <= ol * 11 / 10 + 20);
        else
            expect("CLICK: DEPTH / RATE walked: no step above the old mono chorus's (x 1.1 + 20)", jl <= ol * 11 / 10 + 20 && jr <= ol * 11 / 10 + 20);
    }
}
static void sc_space(void)
{
    double lv[2];
    int32_t pk[2];
    uint32_t v;
    for (v = 0; v < 1u; v++) {
        uint32_t i;
        dsp_clean(DM_STEREO);
        song.g[G_CDEPTH] = 127;
        song.g[G_CRATE] = 44;
        song.g[G_RTYPE] = 0;
        song.g[G_DMDEP] = 0;
        song.g[G_DMODE] = DMD_DIGITAL;
        for (i = 0; i < 2u; i++) {
            host_preset(&trk[i], eng_of("ANALOG"), 0);
            trk[i].p[P_TRIM] = 100;
            trk[i].p[P_PAN] = 0;
            trk[i].p[P_SUS] = 127;
            trk[i].p[P_CHOR] = 127;
            trk[i].p[P_DLY] = trk[i].p[P_REV] = 0;
        }
        perf_space = 100;
        rec_n = 0;
        mblk(8);
        trk_note_on(&trk[0], 52, 110);
        trk_note_on(&trk[0], 59, 110);
        trk_note_on(&trk[0], 64, 110);
        mblk(2u * sc_1s());
        {
            static int32_t seg_l[MAXB * CTL];
            uint32_t a = rec_n / 2u;
            for (i = a; i < rec_n; i++)
                seg_l[i - a] = rec_l[i];
            lv[v] = (sc_rms(seg_l, rec_n - a) + 0.0);
            pk[v] = peak_of(rec_l, rec_n) > peak_of(rec_r, rec_n) ? peak_of(rec_l, rec_n) : peak_of(rec_r, rec_n);
            if (!v)
                lv[1] = sc_rms(rec_r + a, rec_n - a);
        }
        perf_space = 0;
        break;
    }
    printf("stereochorus: SPACE +100 with the stereo chorus: rms L %.0f R %.0f, peak %d (full scale 32767)\n", lv[0], lv[1], pk[0]);
    expect("SPACE: +100 with the stereo chorus on: bounded under full scale and the channels balanced (L / R rms within 3 dB)", pk[0] < 32000 && lv[0] > 0 && lv[1] > 0 && lv[0] / lv[1] < 1.42 && lv[1] / lv[0] < 1.42);
}
static double bus_cost(uint32_t path)
{
    uint64_t i0;
    uint32_t reps = 8, r, t;
    static int32_t z[CTL], w[CTL];
    bus_cfg(DM_STEREO, 100, 44);
    bus_set_path(path);
    sc_xs = 0x2545F491u;
    fill_noise(BN, 12000);
    memset(z, 0, sizeof z);
    for (t = 0; t + CTL <= FS / 2u; t += CTL)
        path == P_OLD ? fx_buses_m(DLY_LEN - 1u, delay_samples(), bin + t, z, z, w, CTL, 0u, 0) : fx_buses(bin + t, z, z, w, CTL);
    i0 = instr_now();
    for (r = 0; r < reps; r++)
        for (t = 0; t + CTL <= BN; t += CTL) {
            if (path == P_OLD)
                fx_buses_m(DLY_LEN - 1u, delay_samples(), bin + t, z, z, w, CTL, 0u, 0);
            else if (path == P_OLDMOD)
                fx_buses_mod(DLY_LEN - 1u, delay_samples(), bin + t, z, z, w, CTL, 0u, 0);
            else
                fx_buses(bin + t, z, z, w, CTL);
        }
    return i0 ? (double)(instr_now() - i0) / (reps * (BN / CTL * CTL)) : 0.0;
}
static void sc_cost(void)
{
    double m = bus_cost(P_OLD), s = bus_cost(P_PLAIN), mm = bus_cost(P_OLDMOD), sm;
    bus_cfg(DM_STEREO, 100, 44);
    sm = bus_cost(P_MOD);
    printf("stereochorus: cost, host instructions per sample of the bus stage (chorus + plain delay + ROOM): mono tap (DUEL's) %.1f, stereo %.1f (%+.1f); with the delay's modulation: mono %.1f, stereo %.1f (%+.1f)\n", m, s, s - m, mm, sm, sm - mm);
    expect("COST: measured, the stereo chorus adds under 60 (90 with the delay modulation) instructions per sample", m > 0 && s > 0 && s - m < 60.0 && sm - mm < 90.0);
}

/* ------------------------------------------------------------ the listening set --- */
/* stereochorus_test DEMODIR TAG: a held ANALOG SOFT PAD chord, a WAVE chord and an ANALOG pluck phrase through the chorus at three RATE / DEPTH settings, into
 * DEMODIR/TAG_<sound>_<setting>.wav (stereo, 16 bit). Built as it is, TAG is "stereo"; built with -DCHO_OFF=0u (the second tap on the first: the chorus of 0.2.20, bit for bit,
 * the goldens say so) the same file is the "mono" one. index.html (A/B players) is written by the caller (a script), not by this file */
static void sc_wav(const char *path, const int32_t *l, const int32_t *r, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, data = n * 4u, v;
    uint16_t s;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f); v = 36u + data; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    v = FS; fwrite(&v, 4, 1, f); v = FS * 4u; fwrite(&v, 4, 1, f);
    s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (i = 0; i < n; i++) {
        int32_t a = l[i] > 32767 ? 32767 : l[i] < -32768 ? -32768 : l[i], c = r[i] > 32767 ? 32767 : r[i] < -32768 ? -32768 : r[i];
        s = (uint16_t)a; fwrite(&s, 2, 1, f);
        s = (uint16_t)c; fwrite(&s, 2, 1, f);
    }
    fclose(f);
}
static void sc_demo(const char *dir, const char *tag)
{
    static const struct { const char *name; int rate, depth; } SET[3] = {{"default_rate40_depth60", 40, 60}, {"slow_wide_rate20_depth110", 20, 110}, {"fast_shallow_rate70_depth35", 70, 35}};
    static const char *const SRC[3] = {"padanalog", "chordwave", "pluckanalog"};
    static const uint8_t PADN[3] = {48, 55, 64}, PLKN[8] = {60, 64, 67, 72, 67, 64, 62, 59}, CHDA[4] = {57, 60, 64, 71}, CHDB[4] = {53, 57, 60, 69};
    uint32_t s, v, b, j, ms = FS / 1000u;
    for (s = 0; s < 3u; s++)
        for (v = 0; v < 3u; v++) {
            char path[400];
            uint32_t secs = s == 2u ? 6u : 8u, nb = secs * FS / CTL;
            dsp_clean(DM_STEREO);
            song.g[G_RTYPE] = 0;
            song.g[G_DMDEP] = 0;
            song.g[G_DMODE] = DMD_DIGITAL;
            song.g[G_CRATE] = (int16_t)SET[v].rate;
            song.g[G_CDEPTH] = (int16_t)SET[v].depth;
            if (s == 0u)
                host_preset(&trk[0], eng_of("ANALOG"), 1);       /* SOFT PAD */
            else if (s == 1u)
                host_ds_load(&trk[0], eng_of("WAVE"), 0);        /* PWM PAD */
            else
                host_ds_load(&trk[0], eng_of("ANALOG"), 12);     /* PIZZICATO */
            trk[0].p[P_CHOR] = 110;
            trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
            trk[0].p[P_PAN] = 0;
            rec_n = 0;
            for (b = 0; b < nb; b++) {
                uint32_t t = b * CTL;
                if (s == 0u) {
                    if (t < CTL) for (j = 0; j < 3u; j++) trk_note_on(&trk[0], PADN[j], 90);
                    if (t >= 4500u * ms && t < 4500u * ms + CTL) for (j = 0; j < 3u; j++) trk_note_off(&trk[0], PADN[j]);
                } else if (s == 1u) {
                    if (t < CTL) for (j = 0; j < 4u; j++) trk_note_on(&trk[0], CHDA[j], 90);
                    if (t >= 2000u * ms && t < 2000u * ms + CTL) for (j = 0; j < 4u; j++) trk_note_off(&trk[0], CHDA[j]);
                    if (t >= 3000u * ms && t < 3000u * ms + CTL) for (j = 0; j < 4u; j++) trk_note_on(&trk[0], CHDB[j], 90);
                    if (t >= 5000u * ms && t < 5000u * ms + CTL) for (j = 0; j < 4u; j++) trk_note_off(&trk[0], CHDB[j]);
                } else {
                    uint32_t step = t / (450u * ms), w = t % (450u * ms);
                    if (step < 8u && w < CTL) trk_note_on(&trk[0], PLKN[step], 100);
                    if (step < 8u && w >= 150u * ms && w < 150u * ms + CTL) trk_note_off(&trk[0], PLKN[step]);
                }
                mblk(1);
            }
            snprintf(path, sizeof path, "%s/%s_%s_%s.wav", dir, tag, SRC[s], SET[v].name);
            sc_wav(path, rec_l, rec_r, rec_n);
            printf("stereochorus: demo %s (peak L %d R %d)\n", path, peak_of(rec_l, rec_n), peak_of(rec_r, rec_n));
        }
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 2) {
        sc_demo(argv[1], argv[2]);
        return 0;
    }
#ifdef SC_BASE
    sc_mix();
    return 0;
#else
    sc_bus();
    sc_split();
    sc_mix();
    sc_click();
    sc_space();
    sc_cost();
    printf("stereochorus: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
#endif
}
