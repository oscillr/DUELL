/* SPDX-License-Identifier: GPL-3.0-only
 * Host experiment (not firmware, not in any default test list): the reverb of the CHOMPI sampler against DUELL's five. Since 0.2.14 its
 * "small" integer rendition is in the firmware as the sixth reverb, PLATE (fx.c, fx_bus.c rev_plate); tests/plate_render.c renders the firmware
 * against it.
 *
 * What CHOMPI uses (all three firmwares: TAPE, TEMPO, WAVE; src/reverb.h, src/fx_engine.h): Emilie Gillet's Mutable
 * Instruments "Rings / Clouds" reverb, the Griesinger plate (4 input allpasses, then a figure-eight of two loops of
 * delay + 2 allpasses, the two long delays slowly modulated), as shipped in DaisySP-style form with a 16-bit buffer.
 * It has no grains in it. The "granular" part of CHOMPI is TEMPO's granular delay in front of it (not rendered here).
 * This file renders that reverb's topology (float: a plain C rendition of the same signal flow; integer: Q15 coefficients,
 * int16 lines, three sizes) and DUELL's ROOM, SPRING, HALL, SHIM and RESO through the real mix path on the same dry
 * sources, writes WAVs and an index.html, and measures the cost with the repo's model (host instructions per sample,
 * 1.7 % CPU per 100, as reverb_test.c / drum_test.c).
 *
 *   build (from felucca-base, after host_tests.sh has made build/gen):
 *     cc -O2 -w -DDUELL_TEST_MODE=1 -Ibuild/gen -Ifirmware/src -Itests -o build/host/revcompare_test tests/revcompare_test.c -lm
 *   build/host/revcompare_test OUTDIR       (WAVs, index.html, report.txt into OUTDIR)
 *
 * The CHOMPI topology and constants below follow firmware/chompi-tape/code/src/reverb.h and fx_engine.h of
 * https://github.com/CHOMPI-Club/CHOMPI (MIT), itself derived from pichenettes/eurorack (Rings, Clouds) by Emilie Gillet (MIT):
 *
 *   Copyright 2014 Emilie Gillet (emilie.o.gillet@gmail.com)
 *   Copyright 2026 CHOMPI Club
 *   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 *   documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 *   rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 *   permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *   The above copyright notice and this permission notice shall be included in all copies or substantial portions of
 *   the Software.
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 *   COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 *   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
#include <sys/stat.h>

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
    return (int32_t)(((int64_t)(int32_t)xs * amp) >> 31);
}

/* ======================================================================= the CHOMPI reverb === */
/* lines, in the order of reverb.h's Reserve list: ap1..ap4, dap1a, dap1b, del1, dap2a, dap2b, del2 */
#define CH_N 10
static const uint16_t CH_LEN[CH_N] = {150, 214, 319, 527, 2182, 2690, 4501, 2525, 2197, 6312};
#define CH_DEL1 6
#define CH_DEL2 9
/* levels (both versions): the send (DUELL's mono send, ROOM's 2580 / 32768 first) x CHOMPI's input gain .3 x 2 (it reads
 * left + right of a mono signal) -> the lines hold int16; the output is the loops' tap (x2, as in reverb.h) times CH_OUT */
#define CH_IN 1548               /* Q12: 2580 / 32768 * 0.6 * 4096 * 8: ROOM's send level x CHOMPI's input gain (.3 x 2), x8 so the lines use their bits */
#define CH_OUT 4096              /* Q12: x1 (the wet comes out about ROOM's level with the x8 of the input; the renders match it exactly anyway) */
/* CHOMPI's own control laws (TAPE: reverb knob v -> SetReverb: amt = 1.3 ln(v + 1); the effect's amount = amt^2 * .8 (the dry/wet,
 * not used here: wet only), lowpass = amt * .6 + .4, diffusion = amt * .6; time = the delay-time knob, 0.05 .. 0.97, as the loops' gain) */
static const double CH_KNOB = 0.7;

/* ---- float: the signal flow of reverb.h, one tick = one sample */
typedef struct {
    float *bf[CH_N];
    uint32_t sz[CH_N], w[CH_N];
    float lp1, lp2, ph1, ph2;
    float kap, klp, krt;
} chf_t;
static float chf_mem[CH_N][6320];
static chf_t chf;
static float chf_q(float x)      /* FORMAT_16_BIT: Compress (x32768, truncate, clip) then Decompress */
{
    int32_t v = (int32_t)(x * 32768.0f);
    v = v > 32767 ? 32767 : v < -32768 ? -32768 : v;
    return (float)v / 32768.0f;
}
static void chf_params(double knob, double time)
{
    double amt = 1.3 * log(knob + 1.0);
    chf.klp = (float)(amt * 0.6 + 0.4);
    chf.kap = (float)(amt * 0.6);
    chf.krt = (float)time;
}
static void chf_clear(void)
{
    uint32_t k;
    memset(chf_mem, 0, sizeof chf_mem);
    for (k = 0; k < CH_N; k++) {
        chf.bf[k] = chf_mem[k];
        chf.sz[k] = CH_LEN[k] + 1u;
        chf.w[k] = 0;
    }
    chf.lp1 = chf.lp2 = chf.ph1 = chf.ph2 = 0;
}
static inline float chf_rd(int k, uint32_t d)
{
    int32_t i = (int32_t)chf.w[k] - (int32_t)d;
    return chf.bf[k][i < 0 ? i + (int32_t)chf.sz[k] : i];
}
static inline void chf_wr(int k, float v) { chf.bf[k][chf.w[k]] = chf_q(v); }
static inline float chf_interp(int k, float off)
{
    int32_t oi = (int32_t)off;
    float fr = off - (float)oi, a = chf_rd(k, (uint32_t)oi), b = chf_rd(k, (uint32_t)oi + 1u);
    return a + (b - a) * fr;
}
static inline float chf_ap(int k, float acc, float kap_read, float kap_write)
{
    float t = chf_rd(k, CH_LEN[k] - 1u);          /* Read(ap TAIL, k) ; WriteAllPass(ap, -k) */
    acc += t * kap_read;
    chf_wr(k, acc);
    return acc * kap_write + t;
}
static void chf_run(const int32_t *in, int32_t *wl, int32_t *wr, uint32_t n)
{
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        int32_t x = in[i] < -262143 ? -262143 : in[i] > 262143 ? 262143 : in[i];
        float acc = (float)x * (CH_IN / 4096.0f) / 32768.0f, apout, o1, o2;
        const float kap = chf.kap;
        for (k = 0; k < 4u; k++)
            acc = chf_ap((int)k, acc, kap, -kap);
        apout = acc;
        chf.ph2 += 0.3f / (float)FS;
        chf.ph2 -= chf.ph2 >= 1.f ? 1.f : 0.f;
        acc = apout + chf_interp(CH_DEL2, 6261.0f + 50.0f * cosf(chf.ph2 * 6.2831853f)) * chf.krt;
        chf.lp1 += chf.klp * (acc - chf.lp1);
        acc = chf.lp1;
        acc = chf_ap(4, acc, -kap, kap);
        acc = chf_ap(5, acc, kap, -kap);
        chf_wr(CH_DEL1, acc);
        o1 = acc * 2.f;
        chf.ph1 += 0.5f / (float)FS;
        chf.ph1 -= chf.ph1 >= 1.f ? 1.f : 0.f;
        acc = apout + chf_interp(CH_DEL1, 4460.0f + 40.0f * cosf(chf.ph1 * 6.2831853f)) * chf.krt;
        chf.lp2 += chf.klp * (acc - chf.lp2);
        acc = chf.lp2;
        acc = chf_ap(7, acc, kap, -kap);
        acc = chf_ap(8, acc, -kap, kap);
        chf_wr(CH_DEL2, acc);
        o2 = acc * 2.f;
        for (k = 0; k < CH_N; k++)
            if (++chf.w[k] >= chf.sz[k]) chf.w[k] = 0;
        o1 *= 32768.0f * (CH_OUT / 4096.0f);
        o2 *= 32768.0f * (CH_OUT / 4096.0f);
        wl[i] = (int32_t)o1;
        wr[i] = (int32_t)o2;
    }
}

/* ---- integer: Q15 coefficients, int16 lines, int32 accumulator (kept within +-65535 before every multiply: 65535 * 32767 < 2^31).
 * Three sizes: scale = 1 (full rate, CHOMPI's lengths), 0.5 (processed at 22.05 kHz, all lengths and the drift halved: the
 * same times, the HALL trick), and "small" = half rate AND lengths x0.546: 5.9 k samples = ROOM's comb + allpass buffers
 * (4937 + 997 samples), i.e. no new RAM. Half rate: send 1 2 1 low-passed every other sample in, outputs between samples. */
typedef struct {
    int half;                    /* 1: runs at FS / 2 */
    double sc;                   /* the lengths' scale against CHOMPI's (before the rate) */
    uint32_t len[CH_N], sz[CH_N], w[CH_N];
    int16_t *bf[CH_N];
    uint32_t base1, base2, dep1, dep2;   /* the two modulated reads: base and depth in Q8 samples */
    uint32_t inc1, inc2, ph1, ph2;       /* 0.5 Hz and 0.3 Hz per tick */
    int32_t lp1, lp2, kap, klp, krt;
    int32_t xp, op_l, op_r;      /* half rate: the send's last sample, the last outputs */
    uint32_t total;              /* samples of line */
    uint32_t clips;
    int32_t peak;                /* the largest line value written (headroom check) */
    int16_t *mem;
} chi_t;
static int16_t chi_mem_full[21700], chi_mem_half[10900], chi_mem_small[6000];
static chi_t chi[3];
static void chi_setup(chi_t *c, int half, double sc, int16_t *mem)
{
    uint32_t k, tot = 0;
    double s = sc * (half ? 0.5 : 1.0);
    memset(c, 0, sizeof *c);
    c->half = half;
    c->sc = s;
    for (k = 0; k < CH_N; k++) {
        c->len[k] = (uint32_t)(CH_LEN[k] * s + 0.5);
        if (c->len[k] < 4u) c->len[k] = 4u;
        c->sz[k] = c->len[k] + 1u;
        c->bf[k] = mem + tot;
        tot += c->sz[k];
    }
    c->total = tot;
    c->mem = mem;
    c->dep1 = (uint32_t)(40.0 * s * 256.0 + 0.5);
    c->dep2 = (uint32_t)(50.0 * s * 256.0 + 0.5);
    if (c->dep1 < 256u) c->dep1 = 256u;
    if (c->dep2 < 256u) c->dep2 = 256u;
    c->base1 = c->len[CH_DEL1] * 256u - c->dep1 - 256u;     /* the read reaches base + depth + 1 = the line's length */
    c->base2 = c->len[CH_DEL2] * 256u - c->dep2 - 256u;
    c->inc1 = (uint32_t)(4294967296.0 * 0.5 / (half ? FS / 2.0 : (double)FS));
    c->inc2 = (uint32_t)(4294967296.0 * 0.3 / (half ? FS / 2.0 : (double)FS));
}
static void chi_clear(chi_t *c)
{
    memset(c->mem, 0, c->total * sizeof(int16_t));
    memset(c->w, 0, sizeof c->w);
    c->lp1 = c->lp2 = 0;
    c->ph1 = c->ph2 = 0;
    c->xp = c->op_l = c->op_r = 0;
    c->clips = 0;
    c->peak = 0;
}
static void chi_params(chi_t *c, double knob, double time)
{
    double amt = 1.3 * log(knob + 1.0);
    c->klp = (int32_t)((amt * 0.6 + 0.4) * 32768.0);
    c->kap = (int32_t)((amt * 0.6) * 32768.0);
    c->krt = (int32_t)(time * 32768.0);
    if (c->klp > 32767) c->klp = 32767;
}
/* Ranges (no clamps on the way): a line value is an int16 and an allpass writes (and goes on with) the clipped value, so every
 * product is int16-ish x Q15; the low-pass difference is at most 2 x 32767 (the sum is clamped once before it): 65534 * 32767 < 2^31 */
#define CHI_MQ(a, k) ((((a) * (k)) + 16384) >> 15)   /* (rounded to nearest: a floor leaves a DC the loops keep) */
static inline int32_t chi_rd(const chi_t *c, int k, uint32_t d)
{
    int32_t i = (int32_t)c->w[k] - (int32_t)d;
    return c->bf[k][i < 0 ? i + (int32_t)c->sz[k] : i];
}
#define CHI_INL static inline __attribute__((always_inline))
/* diag: count clips and track the largest line value (the renders); the cost run has it off (a constant in the two callers below) */
CHI_INL int32_t chi_wr(chi_t *c, int k, int32_t v, const int diag)      /* the clipped value */
{
    if (v > 32767) { v = 32767; if (diag) c->clips++; } else if (v < -32768) { v = -32768; if (diag) c->clips++; }
    if (diag) { if (v > c->peak) c->peak = v; else if (-v > c->peak) c->peak = -v; }
    c->bf[k][c->w[k]] = (int16_t)v;
    return v;
}
static inline int32_t chi_interp(const chi_t *c, int k, uint32_t q8)
{
    uint32_t d = q8 >> 8;
    int32_t a = chi_rd(c, k, d), b = chi_rd(c, k, d + 1u);
    return a + (((b - a) * (int32_t)(q8 & 255u)) >> 8);
}
CHI_INL int32_t chi_ap(chi_t *c, int k, int32_t acc, int32_t kr, int32_t kw, const int diag)
{
    int32_t t = chi_rd(c, k, c->len[k] - 1u);
    acc = chi_wr(c, k, acc + CHI_MQ(t, kr), diag);
    return CHI_MQ(acc, kw) + t;
}
/* one tick: the send x (the lines' scale) -> the two loops' taps */
CHI_INL void chi_tick(chi_t *c, int32_t x, int32_t *o1, int32_t *o2, const int diag)
{
    int32_t acc = x > 32767 ? 32767 : x < -32768 ? -32768 : x, apout, k;
    uint32_t q;
    for (k = 0; k < 4; k++)
        acc = chi_ap(c, k, acc, c->kap, -c->kap, diag);
    apout = acc > 32767 ? 32767 : acc < -32768 ? -32768 : acc;
    c->ph2 += c->inc2;
    q = c->base2 + (uint32_t)((osc_sine(c->ph2 + 0x40000000u) * (int32_t)c->dep2) >> 15);
    acc = apout + CHI_MQ(chi_interp(c, CH_DEL2, q), c->krt);
    acc = acc > 32767 ? 32767 : acc < -32768 ? -32768 : acc;
    c->lp1 += CHI_MQ(acc - c->lp1, c->klp);
    acc = c->lp1;
    acc = chi_ap(c, 4, acc, -c->kap, c->kap, diag);
    acc = chi_ap(c, 5, acc, c->kap, -c->kap, diag);
    acc = chi_wr(c, CH_DEL1, acc, diag);
    *o1 = acc * 2;
    c->ph1 += c->inc1;
    q = c->base1 + (uint32_t)((osc_sine(c->ph1 + 0x40000000u) * (int32_t)c->dep1) >> 15);
    acc = apout + CHI_MQ(chi_interp(c, CH_DEL1, q), c->krt);
    acc = acc > 32767 ? 32767 : acc < -32768 ? -32768 : acc;
    c->lp2 += CHI_MQ(acc - c->lp2, c->klp);
    acc = c->lp2;
    acc = chi_ap(c, 7, acc, c->kap, -c->kap, diag);
    acc = chi_ap(c, 8, acc, -c->kap, c->kap, diag);
    acc = chi_wr(c, CH_DEL2, acc, diag);
    *o2 = acc * 2;
    for (k = 0; k < CH_N; k++)
        if (++c->w[k] >= c->sz[k]) c->w[k] = 0;
}
CHI_INL void chi_run_impl(chi_t *c, const int32_t *in, int32_t *wl, int32_t *wr, uint32_t n, const int diag)
{
    uint32_t i;
    if (!c->half) {
        for (i = 0; i < n; i++) {
            int32_t x = in[i] < -262143 ? -262143 : in[i] > 262143 ? 262143 : in[i], o1, o2;
            chi_tick(c, (x * CH_IN) >> 12, &o1, &o2, diag);
            wl[i] = (o1 * CH_OUT) >> 12;
            wr[i] = (o2 * CH_OUT) >> 12;
        }
        return;
    }
    for (i = 0; i < n / 2u; i++) {
        int32_t x = (c->xp + 2 * in[2u * i] + in[2u * i + 1u]) >> 2, o1, o2, l, r;
        c->xp = in[2u * i + 1u];
        x = x < -262143 ? -262143 : x > 262143 ? 262143 : x;
        chi_tick(c, (x * CH_IN) >> 12, &o1, &o2, diag);
        l = (o1 * CH_OUT) >> 12;
        r = (o2 * CH_OUT) >> 12;
        wl[2u * i] = (c->op_l + l) >> 1;
        wr[2u * i] = (c->op_r + r) >> 1;
        wl[2u * i + 1u] = l;
        wr[2u * i + 1u] = r;
        c->op_l = l;
        c->op_r = r;
    }
}

static __attribute__((noinline)) void chi_run(chi_t *c, const int32_t *in, int32_t *wl, int32_t *wr, uint32_t n) { chi_run_impl(c, in, wl, wr, n, 1); }
static __attribute__((noinline)) void chi_run_nodiag(chi_t *c, const int32_t *in, int32_t *wl, int32_t *wr, uint32_t n) { chi_run_impl(c, in, wl, wr, n, 0); }

/* ============================================================== the models, one interface === */
enum { M_ROOM, M_SPRING, M_HALL, M_SHIM, M_RESO, M_CHF, M_CHI, M_CHH, M_CHS, M_N };
static const char *const M_NAME[M_N] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "CHOMPI", "CHOMPI_int", "CHOMPI_half", "CHOMPI_small"};
static double ch_time[M_N];      /* the loops' gain each CHOMPI version uses (matched to HALL's RT60 below) */

static void model_reset(int m, int32_t size, int32_t damp)
{
    song.g[G_RTYPE] = (int16_t)(m <= M_RESO ? m : 0);
    song.g[G_RSIZE] = (int16_t)size;
    song.g[G_RDAMP] = (int16_t)damp;
    rev_clear();
    fx.rtype = (uint8_t)song.g[G_RTYPE];
    fx.sp_w = 0;
    fx.sp_size = 0;
    fx.sp_ph = 0;
    hl.ph = 0;
    hl.sz = 0;
    hl.side = 0;
    if (m == M_CHF) {
        chf_clear();
        chf_params(CH_KNOB, ch_time[m]);
    } else if (m >= M_CHI) {
        chi_t *c = &chi[m - M_CHI];
        chi_clear(c);
        chi_params(c, CH_KNOB, ch_time[m]);
    }
}
/* the model's response to the block `in` (CTL samples): left and right wet (ours: the mid, HALL's side as the mix adds it) */
static void model_wet(int m, const int32_t *in, int32_t *wl, int32_t *wr, uint32_t n)
{
    uint32_t i;
    if (m == M_CHF) { chf_run(in, wl, wr, n); return; }
    if (m >= M_CHI) { chi_run(&chi[m - M_CHI], in, wl, wr, n); return; }
    for (i = 0; i < n; i++) wl[i] = 0;
    hl.side = 0;
    rev_run((uint32_t)m, in, wl, n);
    for (i = 0; i < n; i++) wr[i] = wl[i];
    if (hl.side)
        for (i = 0; i < n; i++) {
            wl[i] += rev_side[i];
            wr[i] -= rev_side[i];
        }
}

#define IR_AMP 60000
/* ---- RT60 (Schroeder, -5 .. -35 dB, x2) of a model's impulse response, mono sum */
#define IRN (10u * FS / CTL * CTL)
static double ir_e[IRN];
static int32_t ir_h[IRN];
static double model_rt60(int m, int32_t size, int32_t damp)
{
    static int32_t in[CTL], wl[CTL], wr[CTL];
    uint32_t t, i;
    double s = 0, t5 = -1, t35 = -1;
    model_reset(m, size, damp);
    xs = 0x1234567u;
    for (t = 0; t < IRN; t += CTL) {
        for (i = 0; i < CTL; i++) in[i] = t + i < FS / 20u ? noise(IR_AMP) : 0;      /* a 50 ms noise burst: loud, so the tail stays above the models' quiet gates */
        model_wet(m, in, wl, wr, CTL);
        for (i = 0; i < CTL; i++) ir_h[t + i] = (wl[i] + wr[i]) / 2;
    }
    {   /* (ROOM's loop keeps a few LSB of offset after its tail, as the firmware notes say: take the final half second's mean off) */
        double dc = 0;
        for (i = IRN - FS / 2u; i < IRN; i++) dc += ir_h[i];
        dc /= FS / 2u;
        for (i = 0; i < IRN; i++) ir_h[i] -= (int32_t)(dc < 0 ? dc - 0.5 : dc + 0.5);
    }
    for (i = IRN; i-- > 0;) {
        s += (double)ir_h[i] * ir_h[i];
        ir_e[i] = s;
    }
    if (s <= 0) return 0;
    for (i = 0; i < IRN; i++) {
        double db = 10 * log10(ir_e[i] / ir_e[0] + 1e-30);
        if (t5 < 0 && db <= -5) t5 = i;
        if (t35 < 0 && db <= -35) { t35 = i; break; }
    }
    return t35 < 0 ? 99 : (t35 - t5) * 2.0 / FS;
}

/* ================================================================================ the renders === */
#define SECS 10u
#define NFR (SECS * FS)
typedef struct { const char *name, *what; } src_t;
static const src_t SRC[3] = {
    {"pluck", "ANALOG PLUCK chord (C major), one hit, then the tail"},
    {"click", "DRUM KIT burst: kick, snare and hats for one bar, then the tail"},
    {"pad", "ANALOG SOFT PAD, one held note (4 s), then the tail"}};
static void setup_src(int src)
{
    static const uint8_t CHD[3] = {60, 64, 67};
    uint32_t i;
    track_t *tr;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    tr = &trk[0];
    if (src == 0) {
        song.g[G_BPM] = 100;
        host_preset(tr, 0, 8);                                     /* ANALOG PLUCK */
        for (i = 0; i < 16u; i++)
            put_step(tr, i, i == 0 ? 3u : 0u, CHD, i == 0 ? ST_NOTE : ST_REST, 0);
    } else if (src == 1) {
        song.g[G_BPM] = 100;
        host_drums(tr);
        for (i = 0; i < 16u; i++) {
            uint8_t dn[3], k = 0;
            if (i % 4u == 0u || i == 10u) dn[k++] = 36;
            if (i == 4u || i == 12u) dn[k++] = 38;
            if (i % 2u == 0u) dn[k++] = 42;
            put_step(tr, i, k, dn, k ? ST_NOTE : ST_REST, 0);
        }
    } else {
        uint32_t p, pad = 1;
        const uint8_t N0 = 55;
        song.g[G_BPM] = 60;                                        /* one bar = 4 s */
        for (p = 0; p < ENGINES[0]->npresets; p++)
            if (!strcmp(ENGINES[0]->presets[p].name, "SOFT PAD")) pad = p;
        host_preset(tr, 0, pad);
        for (i = 0; i < 16u; i++)
            put_step(tr, i, i == 0 ? 1u : 0u, &N0, i == 0 ? ST_NOTE : i < 14u ? ST_TIE : ST_REST, 0);
    }
    tr->p[P_REV] = 90;
    tr->p[P_DLY] = tr->p[P_CHOR] = 0;
}
static int32_t send_peak;
static const double STOP_S[3] = {2.3, 2.3, 3.9};
/* the dry source is run through the engines ONCE per source (the drum voices' noise carries over between runs, so two runs
 * differ) and kept: the part mix (left, right: no reverb in it) and the reverb send. Every model then gets exactly these. */
static int32_t cap_l[NFR], cap_r[NFR], cap_s[NFR];
static int cap_src = -1;
static void capture(int src)
{
    uint32_t t, i;
    setup_src(src);
    model_reset(M_ROOM, 90, 60);
    transport_req = 1;
    send_peak = 0;
    for (t = 0; t < NFR; t += CTL) {
        if (t <= (uint32_t)(STOP_S[src] * FS) && t + CTL > (uint32_t)(STOP_S[src] * FS))
            transport_req = 2;
        for (i = 0; i < CTL; i++)
            send_c[i] = send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = 0;
        events_block(CTL);
        for (i = 0; i < NPART; i++)
            mix_part(&trk[i], CTL);
        for (i = 0; i < CTL; i++) {
            int32_t a = send_r[i] < 0 ? -send_r[i] : send_r[i];
            if (a > send_peak) send_peak = a;
            cap_l[t + i] = mix_l[i];
            cap_r[t + i] = mix_r[i];
            cap_s[t + i] = send_r[i];
        }
    }
    cap_src = src;
    printf("revcompare: source %s captured: the reverb send peaks at %d\n", SRC[src].name, (int)send_peak);
}
/* one render of source src through model m (-1: dry). gq: the wet's gain (Q10). The wet's energy into *en; the clips of the
 * integer versions into *clips. Writes a WAV when f is not NULL */
static void render(int src, int m, int32_t size, int32_t gq, FILE *f, double *en, uint32_t *clips, int32_t *peak)
{
    static int32_t wl[CTL], wr[CTL], sd[CTL];
    uint32_t t, i;
    int32_t mg = (int32_t)song.master_q12;
    double e = 0;
    if (cap_src != src) capture(src);
    model_reset(m < 0 ? M_ROOM : m, size, 60);
    if (f) wav_hdr(f, NFR);
    for (t = 0; t < NFR; t += CTL) {
        for (i = 0; i < CTL; i++) {
            send_c[i] = send_d[i] = 0;
            send_r[i] = cap_s[t + i];
            wl[i] = wr[i] = 0;
        }
        if (m >= 0) {
            if (m <= M_RESO) {
                for (i = 0; i < CTL; i++) wet[i] = 0;
                hl.side = 0;
                fx_buses(send_c, send_d, send_r, wet, CTL);
                for (i = 0; i < CTL; i++) {
                    wl[i] = wet[i] + (hl.side ? rev_side[i] : 0);
                    wr[i] = wet[i] - (hl.side ? rev_side[i] : 0);
                }
            } else
                model_wet(m, send_r, wl, wr, CTL);
        }
        for (i = 0; i < CTL; i++) {
            int32_t a = (wl[i] * gq) >> 10, b = (wr[i] * gq) >> 10, l, r;
            e += (double)a * a + (double)b * b;
            l = (((cap_l[t + i] + a) >> 2) * mg) >> 10;
            r = (((cap_r[t + i] + b) >> 2) * mg) >> 10;
            master_out(&l, &r);
            if (f) wav_put(f, l, r);
        }
    }
    (void)sd;
    *en = e;
    *clips = m >= M_CHI ? chi[m - M_CHI].clips : 0;
    *peak = m >= M_CHI ? chi[m - M_CHI].peak : 0;
}

/* ===================================================================================== the cost === */
static double cost_of(int m)           /* host instructions / sample of the model alone, on noise */
{
    static int32_t in[CTL], wl[CTL], wr[CTL];
    uint32_t b, i, nb = 3u * FS / CTL;
    uint64_t i0;
    host_tracks_init();
    model_reset(m, 90, 60);
    for (b = 0; b < 40; b++) {         /* (warm up, fill the lines) */
        for (i = 0; i < CTL; i++) in[i] = noise(30000);
        model_wet(m, in, wl, wr, CTL);
    }
    for (i = 0; i < CTL; i++) in[i] = noise(30000);
    i0 = instr_now();
    for (b = 0; b < nb; b++) {
        if (m >= M_CHI) chi_run_nodiag(&chi[m - M_CHI], in, wl, wr, CTL);   /* (no clip / peak counters) */
        else model_wet(m, in, wl, wr, CTL);
    }
    return i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
}

/* ===================================================================================== main === */
#ifndef REVCOMPARE_NO_MAIN
/* the jobs: a source at a SIZE (the pad once more at SIZE 127, the longest tail) */
typedef struct { const char *tag; int src; int32_t size; } job_t;
static const job_t JOBS[4] = {{"pluck", 0, 90}, {"click", 1, 90}, {"pad", 2, 90}, {"padlong", 2, 127}};
static void fit_chompi(int32_t size, double *hall_rt, double *rt)
{
    int m, k;
    *hall_rt = model_rt60(M_HALL, size, 60);
    for (m = 0; m < M_N; m++) rt[m] = m <= M_RESO ? model_rt60(m, size, 60) : 0;
    for (m = M_CHF; m < M_N; m++) {
        double lo = 0.05, hi = 0.97;
        for (k = 0; k < 14; k++) {
            double mid = (lo + hi) / 2;
            ch_time[m] = mid;
            if (model_rt60(m, size, 60) < *hall_rt) lo = mid; else hi = mid;
        }
        ch_time[m] = (lo + hi) / 2;
        rt[m] = model_rt60(m, size, 60);
    }
}
int main(int argc, char **argv)
{
    char path[600], dir[500];
    FILE *rp, *ix;
    int m, j;
    double rt[4][M_N], hall_rt[4], en[M_N], cost[M_N], raw_db[4][M_N], tm[4][M_N];
    int32_t gq[M_N];
    uint32_t clips[M_N];
    int32_t peak[M_N];
    snprintf(dir, sizeof dir, "%s", argc > 1 ? argv[1] : "build/revcompare");
    mkdir(dir, 0755);
    snprintf(path, sizeof path, "%s/report.txt", dir);
    rp = fopen(path, "w");
    if (!rp) { printf("revcompare: cannot write %s\n", path); return 1; }
#define OUT(...) do { printf(__VA_ARGS__); fprintf(rp, __VA_ARGS__); } while (0)
    chi_setup(&chi[0], 0, 1.0, chi_mem_full);
    chi_setup(&chi[1], 1, 1.0, chi_mem_half);
    chi_setup(&chi[2], 1, 0.546, chi_mem_small);
    OUT("revcompare: CHOMPI line memory (int16 samples / bytes): full %u / %u, half rate %u / %u, small %u / %u (ROOM's buffers: 5934 / 11868)\n",
        chi[0].total, chi[0].total * 2, chi[1].total, chi[1].total * 2, chi[2].total, chi[2].total * 2);
    OUT("revcompare: CHOMPI knob %.2f -> lowpass %.2f, diffusion %.2f (its own laws); the loops' gain is fitted so its RT60 equals HALL's at the same SIZE\n",
        CH_KNOB, 1.3 * log(CH_KNOB + 1) * 0.6 + 0.4, 1.3 * log(CH_KNOB + 1) * 0.6);

    for (j = 0; j < 4; j++) {
        uint32_t cl;
        int32_t pk;
        double e0;
        int s = JOBS[j].src;
        int32_t size = JOBS[j].size;
        if (j == 0 || j == 3) {      /* (the fit per SIZE: 90 for the first three, 127 for the last) */
            fit_chompi(size, &hall_rt[j], rt[j]);
            for (m = 0; m < M_N; m++) tm[j][m] = ch_time[m];
            OUT("revcompare: SIZE %d, DAMP 60: RT60 (Schroeder -5..-35 dB x2 of the impulse response, mono sum):\n", (int)size);
            for (m = 0; m < M_N; m++) {
                OUT("revcompare:   %-13s RT60 %5.2f s", M_NAME[m], rt[j][m]);
                if (m >= M_CHF) OUT("   loop gain %.3f", ch_time[m]);
                OUT("\n");
            }
        } else {
            memcpy(rt[j], rt[0], sizeof rt[0]);
            memcpy(tm[j], tm[0], sizeof tm[0]);
            hall_rt[j] = hall_rt[0];
            for (m = M_CHF; m < M_N; m++) ch_time[m] = tm[0][m];
        }
        for (m = 0; m < M_N; m++)
            render(s, m, size, 1024, NULL, &en[m], &clips[m], &peak[m]);
        for (m = 0; m < M_N; m++) {
            double g = sqrt(en[M_ROOM] / (en[m] + 1e-9));
            g = g < 0.05 ? 0.05 : g > 30 ? 30 : g;
            gq[m] = (int32_t)(g * 1024.0 + 0.5);
            raw_db[j][m] = 10 * log10((en[m] + 1e-9) / (en[M_ROOM] + 1e-9));
        }
        snprintf(path, sizeof path, "%s/%s_dry.wav", dir, JOBS[j].tag);
        { FILE *f = fopen(path, "wb"); render(s, -1, size, 1024, f, &e0, &cl, &pk); fclose(f); }
        for (m = 0; m < M_N; m++) {
            FILE *f;
            snprintf(path, sizeof path, "%s/%s_%s.wav", dir, JOBS[j].tag, M_NAME[m]);
            f = fopen(path, "wb");
            render(s, m, size, gq[m], f, &e0, &cl, &pk);
            fclose(f);
        }
        OUT("revcompare: %s: wet level against ROOM before matching:", JOBS[j].tag);
        for (m = 0; m < M_N; m++) OUT(" %s %+.1f dB%s", M_NAME[m], raw_db[j][m], m == M_N - 1 ? "\n" : ",");
        OUT("revcompare: %s: int16 lines, CHOMPI_int / half / small: clips %u / %u / %u, largest value %d / %d / %d of 32767\n", JOBS[j].tag, clips[M_CHI], clips[M_CHH], clips[M_CHS], peak[M_CHI], peak[M_CHH], peak[M_CHS]);
    }

    for (m = 0; m < M_N; m++) cost[m] = cost_of(m);
    OUT("revcompare: cost, host instructions / sample of the reverb alone (device estimate 1.7 %% CPU per 100):\n");
    for (m = 0; m < M_N; m++)
        OUT("revcompare:   %-13s %6.0f  ~%.1f %%   x%.2f of ROOM%s\n", M_NAME[m], cost[m], cost[m] * 0.017, cost[M_ROOM] ? cost[m] / cost[M_ROOM] : 0.0,
            m == M_CHF ? "   (float on the host's FPU: not a device figure)" : "");
    fclose(rp);

    snprintf(path, sizeof path, "%s/index.html", dir);
    ix = fopen(path, "w");
    fprintf(ix, "<!doctype html><meta charset=utf-8><title>DUELL reverb comparison</title>"
                "<style>body{font:15px sans-serif;max-width:62em;margin:1em auto}td,th{padding:3px 8px;text-align:left;vertical-align:middle}"
                "h2{margin-top:1.4em}audio{width:300px;height:30px}</style>"
                "<h1>DUELL reverbs against the CHOMPI reverb</h1>"
                "<p>The same dry source and send through each reverb (ours through the real mix path), SIZE 90 DAMP 60 on ours. CHOMPI is its Rings/Clouds-style plate at its reverb knob %.2f, "
                "with its loop gain fitted so the tail lasts as long as HALL's. <b>The wet level of every file is matched to ROOM's</b> (same total wet energy), so you hear character, not loudness. "
                "44.1 kHz stereo 16-bit.</p>"
                "<p><b>Listen first:</b> pad_CHOMPI against pad_HALL and pad_ROOM, then pluck_CHOMPI, then click_CHOMPI, then padlong_* (SIZE 127, the longest tail). "
                "CHOMPI = the float rendition (the reference); CHOMPI_int = integer port, full rate; CHOMPI_half = integer at 22.05 kHz; CHOMPI_small = integer, half rate, shorter lines (fits ROOM's RAM). "
                "The three integer ones should sound like the float one; the further they drift, the more the cheap versions cost in sound.</p>",
            CH_KNOB);
    for (j = 0; j < 4; j++) {
        fprintf(ix, "<h2>%s</h2><p>%s%s</p><table><tr><td>dry</td><td><audio controls preload=none src=\"%s_dry.wav\"></audio></td><td></td></tr>",
                JOBS[j].tag, SRC[JOBS[j].src].what, j == 3 ? " (SIZE 127)" : "", JOBS[j].tag);
        for (m = 0; m < M_N; m++)
            fprintf(ix, "<tr><td><b>%s</b></td><td><audio controls preload=none src=\"%s_%s.wav\"></audio></td><td>RT60 %.1f s, wet before matching %+.1f dB vs ROOM</td></tr>",
                    M_NAME[m], JOBS[j].tag, M_NAME[m], rt[j][m], raw_db[j][m]);
        fprintf(ix, "</table>");
    }
    fprintf(ix, "<p><a href=report.txt>report.txt</a> (RT60, levels, cost)</p>");
    fclose(ix);
    printf("revcompare: wrote %s\n", dir);
    return 0;
}
#endif
