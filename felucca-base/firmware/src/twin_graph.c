/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The TWIN FILTER's graph (cloud job E, for the pages of job H): twin_graph_db(t, which, hz) is the magnitude of filter 1 (which 0), filter 2 (1) or the whole chain (2) at hz, in 1/16 dB, in closed
 * form from the settings alone (the knobs, no modulation; integer only, as ui_graph.c's pictures, whose helpers it uses: this file is included after ui_graph.c). Filter 1: graph_filt's MULTI
 * FILTER (MODE, SLOPE, RES at CUT; flat at the open end with MODE LP). Filter 2: LP / BP / HP / NOTCH as TRIO's SVF (its damping law, so the resonance stops at Q 4: the softer top; the band-pass is
 * the SVF's plain band output times 1.37, as trio_render has it), LADDER 1 / |(1 + jx)^4 + k| (x = f / cutoff: the bass thins by 1 / (1 + k)), COMB the teeth of a feedback comb tuned to C3 (moved by
 * TUNE), damped by its loop's one-pole. DRIVE is not drawn. The whole chain: SERIES the two curves added (BLEND above 0 lets the unfiltered input in before filter 2, below 0 filter 1's output in after
 * it, by amplitude), PARALLEL their amplitude mix (the phases are not known here: a peak against a notch does not cancel in this picture) */
/* a gain in 1/16 dB to an amplitude (Q16) and back */
static int32_t tg_amp(int32_t db16) { return cmp_exp2(-((db16 * 2721) >> 10)); }
static int32_t tg_db(int32_t a_q16) { return ((cmp_lg((uint32_t)(a_q16 > 0 ? a_q16 : 1)) - 4096) * 385) >> 10; }
/* (1 - w) A + w B in amplitude, A and B in 1/16 dB, w Q12 */
static int32_t tg_mix(int32_t a, int32_t b, int32_t w) { return tg_db((int32_t)(((int64_t)tg_amp(a) * (4096 - w) + (int64_t)tg_amp(b) * w) >> 12)); }

static int32_t tg_f1(const track_t *t, uint32_t hz)
{
    int32_t md = clamp(t->p[P_FREEC], 0, UF_HP), res = clamp(t->p[P_FREEB], 0, 127), cut = clamp(t->p[P_FREEA], 0, UF_STEPS), s24 = t->p[P_FREED] == 2, db;
    int32_t wb = md <= 64 ? md << 4 : ((UF_HP - md) * 65) >> 2, wl = md < 64 ? 1024 - wb : 0, wh = md > 64 ? 1024 - wb : 0, k1 = s24 ? UF_K24A : UF_K(res);
    uint32_t fc = gf_cut_hz(40 + 396 * cut);
    if (cut >= UF_STEPS && md <= 0)                      /* (the bypass: the filter is not run) */
        return 0;
    db = svf_db10(hz << 8, fc, wl, wb, wh, k1, k1);
    if (s24)
        db += svf_db10(hz << 8, fc, wl, wb, wh, UF_K24B(res), UF_K24B(res));
    return db * 16 / 10;
}
static int32_t tg_f2(const track_t *t, uint32_t hz)
{
    uint32_t ty = f2_ty(t), fc;
    int32_t res = t->p[P_RES2], cq = XV(t, X_LINK) ? clamp(clamp(t->p[P_FREEA], 0, UF_STEPS) * 256 + TW_OFF_Q8(clamp(t->p[P_CUT2], -TW_OFF_MAX, TW_OFF_MAX)), 0, UF_STEPS * 256) : clamp(t->p[P_CUT2], 0, UF_STEPS) * 256;
    if (!ty)
        return 0;
    if (ty == TT_COMB) {                                 /* teeth every f0 = C3 (130.8 Hz) moved by TUNE; the loop: a = |fb| |damping| */
        int32_t fb = clamp(res, -64, 63) * 61, tune = clamp(t->p[P_CUT2], -24, 24), a, c, den, num, nm = 4096 - (((fb < 0 ? -fb : fb) * 7) / 10);
        uint64_t f0 = ((uint64_t)33488u * pitch_inc((uint32_t)(960 + 16 * tune))) / pitch_inc(960u);   /* C3 in Hz Q8, moved */
        uint32_t ph = (uint32_t)(((uint64_t)hz << 40) / (f0 ? f0 : 1u)), w = (uint32_t)(((uint64_t)hz << 32) / FS);   /* cycles of the tooth, the angle of the one-pole */
        int32_t cw = sine_i(w + 0x40000000u), cth = sine_i(ph + 0x40000000u), p2 = (int32_t)((int64_t)18432 * 32768 / (34816 - (cw >> 1)));   /* |damping|^2 = 0.5625 / (1.0625 - 0.5 cos w), Q15 */
        a = (fb * (int32_t)ins_isqrt((uint32_t)p2 << 15)) >> 12;   /* the loop's gain: fb x |damping|, Q15 */
        c = cth;
        den = (int32_t)((((int64_t)1 << 30) + (int64_t)a * a - 2 * (int64_t)a * c) >> 15);   /* Q15 */
        num = (int32_t)(((int64_t)nm * 8 * nm * 8) >> 15);
        return gr_db10(gr_lg64((uint64_t)(num > 0 ? num : 1)), gr_lg64((uint64_t)(den > 0 ? den : 1))) * 16 / 10;
    }
    fc = gf_cut_hz(40 + ((396 * cq) >> 8));
    if (ty == TT_LADDER) {                               /* 1 / |(1 + jx)^4 + k|, k = 4.4 RES / 127 */
        uint64_t r = ((uint64_t)hz << 16) / (fc ? fc : 1u);   /* x = f / fc, Q16 */
        int64_t k = (int64_t)clamp(res, 0, 127) * 142 * 16, x2, x3, x4, re, im;
        if (r > 6u << 16) {                              /* far above: 1 / x^8 */
            int32_t lx = gr_lg64(r) - gr_lg64(65536u);
            return -((8 * lx * 7706) >> 16) * 16 / 10;
        }
        x2 = (int64_t)((r * r) >> 16);
        x3 = (x2 * (int64_t)r) >> 16;
        x4 = (x2 * x2) >> 16;
        re = 65536 - 6 * x2 + x4 + k;
        im = 4 * (int64_t)r - 4 * x3;
        return -gr_db10(gr_lg64((uint64_t)(re * re + im * im) + 1u), gr_lg64(1ull << 32)) * 16 / 10;
    }
    {   /* TRIO's SVF: the weights of the output (Q10) and the band-pass scale kb = 1 (the SVF's plain band output) */
        int32_t k = 8192 - clamp(res, 0, 127) * 7168 / 127, wl = ty == TT_LP || ty == TT_NOTCH ? 1024 : 0, wb = ty == TT_BP ? 1400 : 0, wh = ty == TT_HP || ty == TT_NOTCH ? 1024 : 0;
        return svf_db10(hz << 8, fc, wl, wb, wh, k, 4096) * 16 / 10;
    }
}
int32_t twin_graph_db(const track_t *t, int which, uint32_t hz)
{
    int32_t d1, d2, bl = clamp(t->p[P_BLEND], -64, 63);
    hz = hz < 1u ? 1u : hz > 22050u ? 22050u : hz;
    if (which == 0)
        return tg_f1(t, hz);
    if (which == 1)
        return tg_f2(t, hz);
    d1 = tg_f1(t, hz);
    if (!f2_ty(t))
        return d1;
    d2 = tg_f2(t, hz);
    if (XV(t, X_ROUTE))
        return tg_mix(d1, d2, ((bl + 64) * 4128) >> 7);
    if (bl > 0)
        return tg_mix(d1, 0, bl * 65) + d2;
    return bl < 0 ? tg_mix(d1 + d2, d1, -bl * 64) : d1 + d2;
}
