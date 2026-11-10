/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PLATE, the sixth reverb model (src/fx.c constants and plate_gain, fx_bus.c rev_plate; G_RTYPE 5; based on the reverb of the CHOMPI firmware, MIT), on the
 * Mac through hostsim.c as reverb_test.c.
 *   build/host/plate_test [DEMODIR]        (host_tests.sh plate; run_tests.sh; DEMODIR: the listening set, see below)
 * 1. determinism: the same sends from the same start give the same output, sample for sample (bus A, and bus B against bus A), also
 *    after a model change away and back; bus A and bus B fed alike give the same wet and side.
 * 2. loud input: noise and square waves at the largest send, SIZE 127 / 150 (SPACE's), DAMP 0 / 127: the output stays bounded and
 *    the 16-bit lines hardly touch their limit (clamped, never wrapped).
 * 3. decay: RT60 (Schroeder -5 .. -35 dB x2) rises with SIZE; near the host comparison's fit to HALL's (3.0 s at 90, 6.2 s at 127).
 * 4. stability at the corners (SIZE 127 / 150 with DAMP 0 and 127, loud noise and squares, then silence): bounded, the tail ends in
 *    exact silence (the wet bus and the stereo difference), the end of the tail has no offset.
 * 5. no DC: the mean of a long tail after a zero-mean burst is tiny against its rms.
 * 6. stereo: the left and right of the tail are decorrelated (|r| < 0.5), mid and side both present.
 * 7. bus A / B (SPLIT): each bus alone: the other bus's wet exactly 0; the same sends give the same return on both.
 * 8. model changes to and from PLATE while it rings (all five others, both ways): no click, the new one starts silent; the old
 *    one's state cleared (PLATE after another model starts from silence: the same as a fresh start).
 * 9. cost: host instructions per sample of the reverb alone, against rev_room, rev_hall, rev_shim (drum_test's ratio).
 * The lines: PLATE's ten lines fit ROOM's two buffers exactly as laid out (static asserts in fx.c; here the last cell of each buffer
 * is never written by it).
 * Demos (WAV) into DEMODIR: a pluck and a pad chord through PLATE at SIZE 40 / 90 / 127. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("plate: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static uint32_t xs = 0x1234567u;
static int32_t noise(int32_t amp)
{
    xs ^= xs << 13;
    xs ^= xs >> 17;
    xs ^= xs << 5;
    return (int32_t)(((int64_t)(int32_t)xs * amp) / 2147483648LL);   /* (towards 0: a floor makes -0.5 of DC in the test's own sends) */
}

/* a clean bus A on model m (the delay and chorus quiet: only the reverb) */
static void start(int m, int32_t size, int32_t damp)
{
    host_tracks_init();
    memset(&fx, 0, sizeof fx);
    memset(&hl, 0, sizeof hl);
    rev_clear();
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    song.g[G_RTYPE] = (int16_t)m;
    song.g[G_RLOCUT] = 0;                   /* (0.2.14: the reverb alone; the low cut is tests/locut_test.c) */
    song.g[G_RSIZE] = (int16_t)size;
    song.g[G_RDAMP] = (int16_t)damp;
    song.g[G_DMDEP] = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    fx.rtype = (uint8_t)m;
}
static void start_b(int m, int32_t size, int32_t damp)
{
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl_b, 0, sizeof hl_b);
    rev_clear_b();
    memset(cho_buf_b, 0, sizeof cho_buf_b);
    fx_b.rtype = (uint8_t)m;
    (void)size;
    (void)damp;
}

/* the sends of block b by kind: 0 noise bursts and silences, 1 loud noise, 2 square wave, 3 a 220 Hz sine for 1 s */
static void sends(int kind, uint32_t b, int32_t amp, int32_t *r)
{
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        uint32_t t = b * CTL + i;
        switch (kind) {
        case 0: r[i] = (b / 300u) % 3u != 2u ? noise(b % 2000u < 1000u ? amp : amp / 20) : 0; break;
        case 1: r[i] = noise(amp); break;
        case 2: r[i] = ((t / 100u) & 1u) ? amp : -amp; break;
        default: r[i] = t < FS ? (int32_t)(amp * sin(2 * M_PI * 220 * t / FS)) : 0; break;
        }
    }
}

/* ------------------------------------------------------ 1. determinism --- */
static uint32_t run_hash(int bus_b, uint32_t secs, int away)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, h = 2166136261u, nb = secs * FS / CTL;
    uint32_t x0 = xs;
    xs = 0x2468ACEu;
    start(5, 90, 60);
    start_b(5, 90, 60);
    for (b = 0; b < nb; b++) {
        if (b % 700u == 0u) {
            song.g[G_RSIZE] = (int16_t)((uint32_t)noise(1 << 30) % 128u);
            song.g[G_RDAMP] = (int16_t)((uint32_t)noise(1 << 30) % 128u);
        }
        if (away && b == nb / 3u)
            song.g[G_RTYPE] = 2;
        if (away && b == nb / 3u + 400u)
            song.g[G_RTYPE] = 5;
        sends(0, b, 90000, r);
        for (i = 0; i < CTL; i++)
            c[i] = d[i] = 0;
        if (bus_b)
            fx_buses_m_b(DLY_HALF - 1u, delay_split(), c, d, r, w, CTL, 0u, 0);
        else
            fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            h = (h ^ (uint32_t)w[i]) * 16777619u;
            h = (h ^ (uint32_t)(bus_b ? 0 : hl.side ? rev_side[i] : 0)) * 16777619u;
        }
    }
    xs = x0;
    return h;
}
static void test_determinism(void)
{
    uint32_t a = run_hash(0, 12, 0), b = run_hash(0, 12, 0), c = run_hash(1, 12, 0), d = run_hash(0, 12, 1), e = run_hash(0, 12, 1);
    char w[160];
    snprintf(w, sizeof w, "determinism: 12 s of sends twice from the start, hash 0x%08x", a);
    check(w, a == b && a != 2166136261u);
    check("determinism: with a change to HALL and back in the middle, twice the same", d == e && d != a);
    check("determinism: bus B runs it too (a different hash: its wet carries no side)", c != 2166136261u);
}

/* ---------------------------------------------- 2. loud input, 4. corners --- */
static uint32_t lines_at_limit(uint32_t *total)
{
    uint32_t i, n = 0;
    for (i = 0; i < sizeof rev_comb / 2u; i++)
        n += rev_comb[i] >= 32767 || rev_comb[i] <= -32768;
    for (i = 0; i < sizeof rev_u.ap / 2u; i++)
        n += rev_u.ap[i] >= 32767 || rev_u.ap[i] <= -32768;
    *total = (uint32_t)(sizeof rev_comb / 2u + sizeof rev_u.ap / 2u);
    return n;
}
typedef struct { int32_t pk, side_pk; uint32_t silent_at, lim, tot; double mean_tail, rms_tail; } run_t;
/* secs of sends (kind, amp), then silence for tail_s: the largest |wet|, |side|, the first block after which both stay exactly 0 */
static run_t corner(int32_t size, int32_t damp, int kind, int32_t amp, uint32_t loud_s, uint32_t tail_s)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, nb = (loud_s + tail_s) * FS / CTL, nl = loud_s * FS / CTL, lim_max = 0, ts, k;
    run_t o = {0, 0, 0, 0, 0, 0, 0};
    double sm = 0, sq = 0;
    uint32_t cnt = 0;
    start(5, size, damp);
    for (b = 0; b < nb; b++) {
        if (b < nl)
            sends(kind, b, amp, r);
        else
            for (i = 0; i < CTL; i++) r[i] = 0;
        for (i = 0; i < CTL; i++)
            c[i] = d[i] = 0;
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t a = abs(w[i]), s = abs(rev_side[i]);
            o.pk = a > o.pk ? a : o.pk;
            o.side_pk = s > o.side_pk ? s : o.side_pk;
            if (a || s)
                o.silent_at = b + 1u;
            if (b >= nl + FS / CTL && b < nl + 3u * FS / CTL) {      /* the tail 1 .. 3 s after the end (still ringing at every setting) */
                sm += w[i];
                sq += (double)w[i] * w[i];
                cnt++;
            }
        }
        if (b == nl - 1u || b == nl / 2u) {
            k = lines_at_limit(&ts);
            if (k > lim_max) lim_max = k;
            o.tot = ts;
        }
    }
    o.lim = lim_max;
    o.mean_tail = cnt ? sm / cnt : 0;
    o.rms_tail = cnt ? sqrt(sq / cnt) : 0;
    return o;
}
static void test_loud_and_corners(void)
{
    static const int32_t SZ[] = {0, 127, 150}, DP[] = {0, 127};
    static const char *const KN[] = {"noise bursts", "full noise", "square", "sine"};
    uint32_t si, di, kind, ok_all = 1, silent_all = 1, worst_lim = 0, worst_limsq = 0, worst_sil = 0;
    int32_t worst_pk = 0, worst_side = 0;
    char w[200];
    for (si = 0; si < 3u; si++)
        for (di = 0; di < 2u; di++)
            for (kind = 1; kind <= 2u; kind++) {
                run_t o = corner(SZ[si], DP[di], (int)kind, 90000, 2, 40);
                uint32_t sil = o.silent_at;
                ok_all &= o.pk > 1000 && o.pk < 400000 && o.side_pk < 400000;
                silent_all &= sil < (2u + 38u) * FS / CTL;
                worst_pk = o.pk > worst_pk ? o.pk : worst_pk;
                worst_side = o.side_pk > worst_side ? o.side_pk : worst_side;
                if (kind == 1u) worst_lim = o.lim > worst_lim ? o.lim : worst_lim;
                else worst_limsq = o.lim > worst_limsq ? o.lim : worst_limsq;
                worst_sil = sil > worst_sil ? sil : worst_sil;
                (void)KN;
            }
    snprintf(w, sizeof w, "loud (send 90000, 2 s noise / square), SIZE 0 127 150 x DAMP 0 127: peak wet %d, side %d: bounded", worst_pk, worst_side);
    check(w, ok_all);
    snprintf(w, sizeof w, "  the 16-bit lines at their limit (clamped, never wrapped): noise at most %u, full-scale squares %u of 5934 cells", worst_lim, worst_limsq);
    check(w, worst_lim < 5934u / 20u && worst_limsq < 5934u / 8u);
    snprintf(w, sizeof w, "  every tail ends in exact silence (wet and side) within 40 s: the last sound in block %u of %u", worst_sil,
             (2u + 40u) * FS / CTL);
    check(w, silent_all);
}

/* ----------------------------------------------------------- 5. no DC --- */
/* quiet noise in (near the lines' rounding), 5 s: a rounding bias in the loops would show as a steady offset in the output */
static void test_dc(void)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    static const int32_t SZ[] = {40, 127, 150}, AM[] = {40, 400};
    uint32_t si, ai, b, i, ok = 1;
    double worst = 0;
    char what[200];
    for (si = 0; si < 3u; si++)
        for (ai = 0; ai < 2u; ai++) {
            double sm = 0, sq = 0;
            uint32_t cnt = 0;
            start(5, SZ[si], 60);
            xs = 0x5A5A5A5u + si * 77u + ai;
            for (b = 0; b < 6u * FS / CTL; b++) {
                for (i = 0; i < CTL; i++) {
                    c[i] = d[i] = 0;
                    r[i] = noise(AM[ai]);
                }
                fx_buses(c, d, r, w, CTL);
                if (b >= 2u * FS / CTL)
                    for (i = 0; i < CTL; i++) {
                        sm += w[i];
                        sq += (double)w[i] * w[i];
                        cnt++;
                    }
            }
            sm /= cnt;
            sq = sqrt(sq / cnt);
            ok &= sq > 0.5 && fabs(sm) < 0.1 * sq;
            worst = fabs(sm) / (sq + 1e-9) > worst ? fabs(sm) / (sq + 1e-9) : worst;
        }
    snprintf(what, sizeof what, "no DC: 6 s of quiet noise (send 40 / 400) at SIZE 40 / 127 / 150: the output's mean at most %.3f of its rms", worst);
    check(what, ok);
}

/* ------------------------------------------------------------ 3. decay --- */
#define IRN (14u * FS / CTL * CTL)
static int32_t irl[IRN];
static double rt60(const int32_t *h, uint32_t n)
{
    static double e[IRN];
    double s = 0, t5 = -1, t35 = -1;
    uint32_t i;
    for (i = n; i-- > 0;) {
        s += (double)h[i] * h[i];
        e[i] = s;
    }
    for (i = 0; i < n; i++) {
        double db = 10 * log10(e[i] / e[0] + 1e-30);
        if (t5 < 0 && db <= -5) t5 = i;
        if (t35 < 0 && db <= -35) { t35 = i; break; }
    }
    return t35 < 0 ? 99 : (t35 - t5) * 2.0 / FS;
}
static double plate_rt60(int32_t size, int32_t damp)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, nb = IRN / CTL;
    start(5, size, damp);
    xs = 0x13579BDu;
    for (b = 0; b < nb; b++) {
        for (i = 0; i < CTL; i++) {
            c[i] = d[i] = 0;
            r[i] = b * CTL + i < 2205u ? noise(60000) : 0;             /* a 50 ms noise burst */
        }
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++)
            irl[b * CTL + i] = w[i] + (hl.side ? rev_side[i] : 0);
    }
    return rt60(irl, IRN);
}
static void test_decay(void)
{
    static const int32_t SZ[] = {0, 40, 90, 127};
    double rt[4];
    uint32_t k;
    char w[200];
    for (k = 0; k < 4u; k++)
        rt[k] = plate_rt60(SZ[k], 60);
    snprintf(w, sizeof w, "RT60 at SIZE 0 / 40 / 90 / 127 (DAMP 60): %.2f / %.2f / %.2f / %.2f s: rising", rt[0], rt[1], rt[2], rt[3]);
    check(w, rt[0] < rt[1] && rt[1] < rt[2] && rt[2] < rt[3]);
    check("  SIZE 0 about 0.2 .. 1.0 s", rt[0] > 0.2 && rt[0] < 1.0);
    check("  SIZE 90 about HALL's 3.0 s (2.3 .. 3.7)", rt[2] > 2.3 && rt[2] < 3.7);
    check("  SIZE 127 about HALL's 6.2 s (4.8 .. 7.6)", rt[3] > 4.8 && rt[3] < 7.6);
    snprintf(w, sizeof w, "  DAMP 0 / 60 / 127 at SIZE 90: %.2f / %.2f / %.2f s: a darker loop is no longer", plate_rt60(90, 0), rt[2], plate_rt60(90, 127));
    check(w, plate_rt60(90, 0) >= rt[2] - 0.05 && plate_rt60(90, 127) <= rt[2] + 0.05);
}

/* ------------------------------------------------------------- 6. stereo --- */
static void test_stereo(void)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, nb = 10u * FS / CTL;
    double sll = 0, srr = 0, slr = 0, sm = 0, ss = 0;
    char what[200];
    start(5, 90, 60);
    xs = 0x2468ACEu;
    for (b = 0; b < nb; b++) {
        for (i = 0; i < CTL; i++) {
            c[i] = d[i] = 0;
            r[i] = b < FS / CTL / 2u ? noise(60000) : 0;
        }
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            double l = w[i] + rev_side[i], rr = w[i] - rev_side[i];
            if (b >= 4u * FS / CTL) {
                sll += l * l;
                srr += rr * rr;
                slr += l * rr;
                sm += (double)w[i] * w[i];
                ss += (double)rev_side[i] * rev_side[i];
            }
        }
    }
    snprintf(what, sizeof what, "stereo: the tail's left / right correlation %.2f (decorrelated: |r| < 0.5), side / mid rms %.2f", slr / sqrt(sll * srr + 1e-9),
             sqrt(ss / (sm + 1e-9)));
    check(what, fabs(slr / sqrt(sll * srr + 1e-9)) < 0.5 && ss > 0.1 * sm && sm > 0);
}

/* ------------------------------------------------------ 7. bus A / bus B --- */
static void test_buses(void)
{
    static int32_t c[CTL], d[CTL], r[CTL], wa[CTL], wb[CTL], sa[CTL];
    uint32_t b, i, nb = 6u * FS / CTL, za = 1, zb = 1, same = 1, anyA = 0, anyB = 0;
    int pass;
    for (pass = 0; pass < 3; pass++) {              /* 0: only A fed, 1: only B fed, 2: both alike */
        start(5, 100, 50);
        start_b(5, 100, 50);
        xs = 0x2468ACEu;
        for (b = 0; b < nb; b++) {
            sends(0, b, 90000, r);
            for (i = 0; i < CTL; i++)
                c[i] = d[i] = 0;
            for (i = 0; i < CTL; i++)
                wa[i] = wb[i] = 0;
            if (pass != 1)
                fx_buses_m(DLY_HALF - 1u, delay_split(), c, d, r, wa, CTL, 0u, 0);
            for (i = 0; i < CTL; i++)
                sa[i] = rev_side[i];
            if (pass != 0)
                fx_buses_m_b(DLY_HALF - 1u, delay_split(), c, d, r, wb, CTL, 0u, 0);
            for (i = 0; i < CTL; i++) {
                if (pass == 0) { zb &= wb[i] == 0; anyA |= wa[i] != 0; }
                if (pass == 1) { za &= wa[i] == 0; anyB |= wb[i] != 0; }
                if (pass == 2) same &= wa[i] == wb[i];
            }
            (void)sa;
        }
    }
    check("SPLIT: bus A fed alone: bus B's return exactly 0 in every sample, A sounds", zb && anyA);
    check("SPLIT: bus B fed alone: bus A's return exactly 0 in every sample, B sounds", za && anyB);
    check("SPLIT: the same sends on both buses give the same return (the states are each bus's own)", same);
}

/* --------------------------------------------------- 8. model changes --- */
static const char *const MN[] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};
static void test_switch(void)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, dir, m;
    char what[200];
    for (m = 0; m < 5u; m++)
        for (dir = 0; dir < 2u; dir++) {
            int from = dir ? 5 : (int)m, to = dir ? (int)m : 5;
            int32_t prev = 0, step = 0, own = 0, after = 0;
            start(from, 90, 60);
            for (b = 0; b < 4u * FS / CTL; b++) {
                if (b == 2u * FS / CTL)
                    song.g[G_RTYPE] = (int16_t)to;
                for (i = 0; i < CTL; i++) {
                    c[i] = d[i] = 0;
                    r[i] = b < FS / CTL ? (int32_t)(30000 * sin(2 * M_PI * 220 * (b * CTL + i) / FS)) : 0;
                }
                fx_buses(c, d, r, w, CTL);
                for (i = 0; i < CTL; i++) {
                    int32_t L = w[i] + (hl.side ? rev_side[i] : 0), s = abs(L - prev);
                    int32_t an = abs(w[i]) + (hl.side ? abs(rev_side[i]) : 0);
                    if (b >= 2u * FS / CTL - 4u && b <= 2u * FS / CTL + 4u)
                        step = s > step ? s : step;
                    else if (b > FS / CTL + 100u && b < 2u * FS / CTL - 4u)
                        own = s > own ? s : own;
                    if (b > 2u * FS / CTL)
                        after = an > after ? an : after;
                    prev = L;
                }
            }
            snprintf(what, sizeof what, "%s -> %s while the tail rings: largest step %d (the tail's own %d), silent after: %d", MN[from], MN[to], step, own, after);
            check(what, step <= 2 * own + 64 && after == 0 && fx.rtype == to);
        }
}

/* ------------------------------------------------------------- 9. cost --- */
static double cost_of(int m)
{
    static int32_t r[CTL], w[CTL];
    uint32_t b, i, nb = 4u * FS / CTL;
    uint64_t i0;
    start(m, 90, 60);
    for (i = 0; i < CTL; i++)
        r[i] = noise(30000);
    i0 = instr_now();
    for (b = 0; b < nb; b++) {
        for (i = 0; i < CTL; i++)
            w[i] = 0;
        rev_run((uint32_t)m, r, w, CTL);
    }
    return (double)(instr_now() - i0) / (nb * CTL);
}
static void test_cost(void)
{
    double room = cost_of(0), hall = cost_of(2), shim = cost_of(3), pl = cost_of(5);
    char w[220];
    if (!room) {
        printf("plate: cost not measured (no instruction counter)\n");
        return;
    }
    snprintf(w, sizeof w, "cost, host instructions per sample (device CPU at 1.7 %% per 100): ROOM %.0f (%.1f %%), HALL %.0f (%.1f %%), SHIM %.0f (%.1f %%), PLATE %.0f (%.1f %%) = %.2f x ROOM",
             room, room * 0.017, hall, hall * 0.017, shim, shim * 0.017, pl, pl * 0.017, pl / room);
    check(w, pl < 2.0 * room);
}

/* ------------------------------------------------------------- demos --- */
static void wav(const char *path, const int32_t *l, const int32_t *r, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, data = n * 4u, v;
    uint16_t s;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); v = 36u + data; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    v = FS; fwrite(&v, 4, 1, f); v = FS * 4u; fwrite(&v, 4, 1, f);
    s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (i = 0; i < n; i++) {
        int32_t a = l[i] > 32767 ? 32767 : l[i] < -32768 ? -32768 : l[i], b = r[i] > 32767 ? 32767 : r[i] < -32768 ? -32768 : r[i];
        s = (uint16_t)a; fwrite(&s, 2, 1, f);
        s = (uint16_t)b; fwrite(&s, 2, 1, f);
    }
    fclose(f);
}
static void demo(const char *dir)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL], dl[8 * FS], dr[8 * FS];
    static const int32_t SZ[] = {40, 90, 127};
    uint32_t k, b, i;
    char path[300];
    for (k = 0; k < 3u; k++) {
        start(5, SZ[k], 60);
        for (b = 0; b < 8u * FS / CTL; b++) {
            for (i = 0; i < CTL; i++) {
                uint32_t t = b * CTL + i;
                c[i] = d[i] = 0;
                r[i] = t < FS / 2 ? (int32_t)(20000 * sin(2 * M_PI * 330 * t / FS) * exp(-6.0 * t / FS)) : 0;
            }
            fx_buses(c, d, r, w, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t dry = r[i] / 2, s = hl.side ? rev_side[i] : 0;
                dl[b * CTL + i] = dry + w[i] + s;
                dr[b * CTL + i] = dry + w[i] - s;
            }
        }
        snprintf(path, sizeof path, "%s/plate_pluck_%d.wav", dir, (int)SZ[k]);
        wav(path, dl, dr, 8u * FS);
        printf("plate: demo %s\n", path);
    }
}

int main(int argc, char **argv)
{
    test_determinism();
    test_loud_and_corners();
    test_dc();
    test_decay();
    test_stereo();
    test_buses();
    test_switch();
    test_cost();
    if (argc > 1)
        demo(argv[1]);
    if (bad) {
        printf("PLATE TEST FAILED\n");
        return 1;
    }
    printf("PLATE TEST PASSED\n");
    return 0;
}
