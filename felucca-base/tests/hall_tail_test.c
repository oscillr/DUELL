/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.29: the HALL's tail has no gate (owner, T0226: M07 FINAL REEL's hall "cut like a noise gate"; docs/TAIL_CUT_FINDINGS.md).
 * Until 0.2.28 a quiet HALL line rounded its gain's product towards zero (fx.c hall_end, HL_LOUD): the tail fell at its natural rate to
 * ~-50 dBFS (the bus, 32767 full scale) and then at ~200 dB/s to silence. Here a 0.3 s noise burst (or an impulse) goes into the reverb send
 * (fx_buses, bus A; bus B is the same code, fx_bus.c), then silence, and the return (wet + the HALL side) is measured in 100 ms windows:
 *   (1) no gate: while the return is above -72 dBFS (GATE_TOP), no window is more than GATE_DB under the one before (the natural fall is 1.5 .. 4 dB
 *       a window at SIZE 60..127; the old gate fell 15 .. 35 dB a window), at SIZE 60 / 90 / 117 / 127 and bursts of -12 / -30 / -45 dBFS;
 *   (2) the tail reaches -75 dBFS (it used to stop near -50 .. -60);
 *   (3) it still ends in exact silence (no limit cycle): SIZE 0 .. 150 (SPACE's top), DAMP 0 / 60 / 127, noise / 100 Hz / impulse, 0 / -40 dBFS,
 *       silent within 15 s;
 *   (4) ROOM, SPRING, SHIM, RESO, PLATE untouched: their hashes over the same burst (the code of 0.2.28: TAIL_UPDATE=1 prints them).
 *   build/host/hall_tail_test */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define GATE_DB 8.0
#define GATE_TOP (-72.0)               /* above this the tail must fall at its own rate (below it: the 16-bit lines' last LSBs, -75 .. -80) */
#ifndef REF_ROOM                    /* the 0.2.28 hashes (f8046c6, TAIL_UPDATE=1) */
#define REF_ROOM 0x7e9433ad6aedb715ull
#define REF_SPRING 0xf570c74dd5d108b0ull
#define REF_SHIM 0x0cfd1b9eb116b228ull
#define REF_RESO 0x2bcc24a058ad4255ull
#define REF_PLATE 0x925195a27243ed9bull
#endif
static double db(double ms) { return ms > 0 ? 10 * log10(ms / (32767.0 * 32767.0)) : -999; }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)
/* sig: 0 noise, 1 100 Hz, 2 an impulse; returns the 100 ms window levels from the burst's end (lv[0..*nw)), the seconds to exact silence (-1: not within sec) */
static double run(int type, int size, int damp, int sig, double a_db, double sec, double *lv, uint32_t *nw, uint64_t *hash)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, per = (uint32_t)(0.1 * FS / CTL), on = (uint32_t)(0.3 * FS / CTL), nb = on + (uint32_t)(sec * FS / CTL), rng = 12345, k = 0, zero = 0;
    double a = pow(10, a_db / 20) * 32767, s = 0, end = -1;
    uint64_t h = 1469598103934665603ull;
    host_tracks_init();
    song.g[G_RTYPE] = (int16_t)type;
    song.g[G_RSIZE] = (int16_t)size;
    song.g[G_RDAMP] = (int16_t)damp;
    memset(c, 0, sizeof c);
    memset(d, 0, sizeof d);
    for (b = 0; b < nb; b++) {
        int nz = 0;
        for (i = 0; i < CTL; i++) {
            int32_t x = 0;
            rng = rng * 1664525u + 1013904223u;
            if (b < on) {
                if (sig == 0)
                    x = (int32_t)(((double)(int32_t)rng / 2147483648.0) * a * 1.732);
                else if (sig == 1)
                    x = (int32_t)(sin(2 * M_PI * 100 * (double)(b * CTL + i) / FS) * a * 1.414);
                else
                    x = b == 0 && i == 0 ? (int32_t)(a * 8) : 0;
            }
            r[i] = x;
        }
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t y = w[i] + (hl.side ? rev_side[i] : 0);
            s += (double)y * y;
            nz |= y != 0;
            h ^= (uint32_t)y;
            h *= 1099511628211ull;
        }
        if (b >= on) {
            zero = nz ? 0 : zero + 1;
            if (zero == 16u * per && end < 0)          /* (1.6 s of exact zeros: it has ended) */
                end = (double)(b + 1 - on - zero) * CTL / FS;
            if ((b + 1 - on) % per == 0) {
                if (lv && k < *nw)
                    lv[k] = db(s / (per * CTL));
                k++;
                s = 0;
            }
        } else
            s = 0;
    }
    if (nw)
        *nw = k < *nw ? k : *nw;
    if (hash)
        *hash = h;
    return end;
}
int main(void)
{
    static const int SZ[4] = {60, 90, 117, 127}, DM[4] = {60, 60, 51, 60};
    static const double AD[3] = {-12, -30, -45};
    uint32_t si, ai, k;
    printf("HALL tail, 100 ms windows after a 0.3 s noise burst (dBFS of the bus):\n");
    for (si = 0; si < 4u; si++)
        for (ai = 0; ai < 3u; ai++) {
            double lv[120], worst = 0, low = 0;
            uint32_t nw = 120, at = 0;
            run(2, SZ[si], DM[si], 0, AD[ai], 12.0, lv, &nw, 0);
            for (k = 1; k < nw; k++) {
                double nx = lv[k] < -100 ? -100 : lv[k];   /* (exact silence counts as -100 dBFS: a stop from above -80 is a gate) */
                if (lv[k - 1] > GATE_TOP && lv[k - 1] - nx > worst)
                    worst = lv[k - 1] - nx, at = k;
                if (lv[k] > -200 && lv[k] < low)
                    low = lv[k];
            }
            printf("  SIZE %3d DAMP %2d burst %5.1f: largest fall above -72 dBFS %5.1f dB / 100 ms (at %.1f s, %.1f -> %.1f), lowest window %.1f dBFS\n",
                   SZ[si], DM[si], AD[ai], worst, at * 0.1, at ? lv[at - 1] : 0, lv[at], low);
            CHECK(worst <= GATE_DB, "HALL SIZE %d burst %.0f dBFS: a fall of %.1f dB in 100 ms above -72 dBFS (a gate)", SZ[si], AD[ai], worst);
            CHECK(low <= -75, "HALL SIZE %d burst %.0f dBFS: the tail stops at %.1f dBFS (should reach -75)", SZ[si], AD[ai], low);
        }
    {
        static const int S3[7] = {0, 30, 60, 90, 127, 140, 150}, D3[3] = {0, 60, 127};
        static const double A3[2] = {0, -40};
        uint32_t s, dd, g, a, n = 0;
        double longest = 0;
        for (s = 0; s < 7u; s++)
            for (dd = 0; dd < 3u; dd++)
                for (g = 0; g < 3u; g++)
                    for (a = 0; a < 2u; a++) {
                        double e = run(2, S3[s], D3[dd], (int)g, A3[a], 15.0, 0, 0, 0);
                        n++;
                        CHECK(e >= 0, "HALL SIZE %d DAMP %d signal %u at %.0f dBFS: not silent within 15 s (a limit cycle)", S3[s], D3[dd], g, A3[a]);
                        if (e > longest)
                            longest = e;
                    }
        printf("exact silence: %u renders, all silent, the longest tail %.2f s\n", n, longest);
    }
    {   /* the other reverbs: bit for bit (the hashes of 0.2.28) */
        static const char *const NM[6] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};
        static const uint64_t REF[6] = {REF_ROOM, REF_SPRING, 0, REF_SHIM, REF_RESO, REF_PLATE};
        uint32_t t;
        int upd = getenv("TAIL_UPDATE") != 0;
        for (t = 0; t < 6u; t++) {
            uint64_t h = 0;
            if (t == 2u)
                continue;
            run((int)t, 90, 60, 0, -20, 6.0, 0, 0, &h);
            if (upd)
                printf("  %s 0x%016llxull\n", NM[t], (unsigned long long)h);
            else
                CHECK(h == REF[t], "%s: not bit for bit as 0.2.28 (0x%016llx)", NM[t], (unsigned long long)h);
        }
    }
    printf(fails ? "hall_tail: %d FAIL\n" : "hall_tail: all passed\n", fails);
    return fails != 0;
}
