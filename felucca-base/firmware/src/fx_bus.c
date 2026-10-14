/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The shared effect buses (chorus, tempo delay, reverb: ROOM, SPRING, HALL, SHIM, RESO), as a template: fx.c includes it twice
 * (DUELL step 5), for bus A (BUS_FN(f) = f: the buses as they always were, on their own globals, so STEREO and LAYER
 * compile to the same code as before) and for bus B (part 2's in SPLIT: f_b, on fx_b, hl_b, rev_comb_b, rev_u_b,
 * cho_buf_b and dly_buf's second half). The includer defines BUS_FX BUS_HL BUS_COMB BUS_U BUS_CHO BUS_DLY BUS_FN. */

/* ROOM (G_RTYPE 0): 4 damped combs + 2 allpasses (Freeverb-like, mono), added to out */
static __attribute__((noinline)) void BUS_FN(rev_room)(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k;
    int32_t size = 25000 + SV_RSIZE * 50, damp = 32767 - song.g[G_RDAMP] * 200;   /* (SV_RSIZE: song.g's, or PERFORM's SPACE: fx.c sv) */
    if (size > 32300)
        size = 32300;                                   /* (SPACE's largest: 0.986, the combs' loop below 1) */
    for (i = 0; i < n; i++) {
        int32_t a = 0;
        int16_t *c = BUS_COMB;
        int32_t in = mulq15(rev_in[i], 2580);           /* 1/8 at -4 dB: level as before the allpass fix */
        for (k = 0; k < 4u; k++) {
            int32_t o = c[BUS_FX.comb_i[k]];
            BUS_FX.comb_lp[k] = o + mulq15(BUS_FX.comb_lp[k] - o, 32767 - damp);
            c[BUS_FX.comb_i[k]] = (int16_t)clamp(in + mulq15(BUS_FX.comb_lp[k], size), -32768, 32767);
            if (++BUS_FX.comb_i[k] >= REV_COMB[k])
                BUS_FX.comb_i[k] = 0;
            a += o;
            c += REV_COMB[k];
        }
        c = BUS_U.ap;
        for (k = 0; k < 2u; k++) {
            int32_t o = c[BUS_FX.ap_i[k]];
            int32_t v = a + (o >> 1);
            c[BUS_FX.ap_i[k]] = (int16_t)clamp(v, -32768, 32767);
            a = o - a;                                  /* Freeverb: out = buf - in (o - v is a notch comb) */
            if (++BUS_FX.ap_i[k] >= REV_AP[k])
                BUS_FX.ap_i[k] = 0;
            c += REV_AP[k];
        }
        out[i] += a;
    }
}

/* SPRING's loop and taps while SIZE glides (0.2.9; rev_spring): the loop's length moves dq (Q8) a sample from s0 + dq (the block's first
 * sample) on, and every read (the loop's end, the far end, the second pickup) is between two samples. Before, the length stepped a whole
 * sample once a block (the loop's end and the second pickup read at whole samples): a click a block while SIZE moved (PERFORM's SPACE
 * moves it over ~600 samples: ~0.45 s of clicks). At rest (SIZE still) rev_spring's own loop runs, as ever */
static __attribute__((noinline)) void BUS_FN(spring_glide)(const int32_t *rev_in, int32_t *out, uint32_t n, int32_t g, int32_t kl,
                                                          int32_t s0, int32_t dq, int32_t wob)
{
    uint32_t i, k;
    int16_t *ln = BUS_COMB;
    int32_t *ap = BUS_U.sp, q = s0;
    for (i = 0; i < n; i++) {
        uint32_t wp = BUS_FX.sp_w, j = (wp & 3u) * (SP_N + 1u), d, d2, d3;
        int32_t x = mulq15(rev_in[i], 2580), r, p, o, w, l3, f, f2, f3, a, b;
        q += dq;
        d = (uint32_t)q >> 8, f = q & 255;              /* the loop's end, between samples */
        a = ln[(wp - d) & SP_MASK], b = ln[(wp - d - 1u) & SP_MASK];
        r = a + (((b - a) * f) >> 8);
        w = (q >> 1) + wob;                             /* the far end (with the block's wobble) */
        d2 = (uint32_t)w >> 8, f2 = w & 255;
        l3 = (q * 3) >> 2;                              /* the second pickup */
        d3 = (uint32_t)l3 >> 8, f3 = l3 & 255;
        BUS_FX.sp_lp += mulq15(r - BUS_FX.sp_lp, kl);
        o = BUS_FX.sp_lp * g;
        x += (o + ((o >> 31) & 32767)) >> 15;
        o = x - BUS_FX.sp_hp + BUS_FX.sp_he;
        BUS_FX.sp_he = o & 63;
        BUS_FX.sp_hp += o >> 6;
        x -= BUS_FX.sp_hp;
        p = ap[j];
        ap[j] = x;
        for (k = 1; k <= SP_N; k++) {
            int32_t v = (x - ap[j + k]) * SP_A;
            o = ap[j + k];
            x = ((v + ((v >> 31) & 4095)) >> 12) + p;
            p = o;
            ap[j + k] = x;
        }
        ln[wp & SP_MASK] = (int16_t)clamp(x, -32768, 32767);
        BUS_FX.sp_w = (uint16_t)(wp + 1u);
        a = ln[(wp - d2) & SP_MASK], b = ln[(wp - d2 - 1u) & SP_MASK];
        o = ln[(wp - d3) & SP_MASK];
        out[i] += (a + (((b - a) * f2) >> 8)) * 4 + (o + (((ln[(wp - d3 - 1u) & SP_MASK] - o) * f3) >> 8)) * 2;
    }
}

/* SPRING (see the top), added to out */
static __attribute__((noinline)) void BUS_FN(rev_spring)(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, s = (uint32_t)SV_RSIZE;
    int32_t g = 19661 + (int32_t)s * 85;                /* the loop's gain: 0.6 .. 0.93 */
    if (g > 31800)
        g = 31800;                                      /* (SPACE's largest: 0.97) */
    int32_t kl = 26000 - song.g[G_RDAMP] * 160;         /* its low-pass: ~9 kHz .. ~1.3 kHz */
    int32_t len = (int32_t)(1323u + ((s * 1323u) >> 7)) << 8, L, L2, L3, f, w;
    int16_t *ln = BUS_COMB;
    int32_t *ap = BUS_U.sp;
    if (!BUS_FX.sp_size)
        BUS_FX.sp_size = len;
    BUS_FX.sp_ph += 2u * LFO_INC[24];                       /* the wobble: a slow sine, 1.5 samples deep */
    {   /* SIZE glides (a sample a block at most), between samples a sample at a time (spring_glide); the last 1/8 sample at once */
        int32_t s0 = BUS_FX.sp_size, dq = clamp(len - s0, -256, 256) / (int32_t)CTL;   /* (towards 0) */
        if (dq && n == CTL) {
            BUS_FX.sp_size = s0 + dq * (int32_t)CTL;
            BUS_FN(spring_glide)(rev_in, out, n, g, kl, s0, dq, (osc_sine(BUS_FX.sp_ph) * 3) >> 8);
            return;
        }
        BUS_FX.sp_size = len;
    }
    L = BUS_FX.sp_size >> 8;
    w = (BUS_FX.sp_size >> 1) + ((osc_sine(BUS_FX.sp_ph) * 3) >> 8);   /* the far end, Q8 */
    L2 = w >> 8;
    f = w & 255;
    L3 = (L * 3) >> 2;
    for (i = 0; i < n; i++) {
        uint32_t wp = BUS_FX.sp_w, j = (wp & 3u) * (SP_N + 1u);
        int32_t x = mulq15(rev_in[i], 2580), r = ln[(wp - (uint32_t)L) & SP_MASK], p, o;
        int32_t t0 = ln[(wp - (uint32_t)L2) & SP_MASK], t1 = ln[(wp - (uint32_t)L2 - 1u) & SP_MASK];
        BUS_FX.sp_lp += mulq15(r - BUS_FX.sp_lp, kl);
        o = BUS_FX.sp_lp * g;
        x += (o + ((o >> 31) & 32767)) >> 15;           /* towards 0: a loop of floors would hold an offset */
        o = x - BUS_FX.sp_hp + BUS_FX.sp_he;                    /* the low cut, its step's remainder kept (as */
        BUS_FX.sp_he = o & 63;                              /* dc_block): no dead band to hold an offset in the loop */
        BUS_FX.sp_hp += o >> 6;
        x -= BUS_FX.sp_hp;
        p = ap[j];                                      /* the chain: ap[j + k], stage k's output 4 samples ago */
        ap[j] = x;
        for (k = 1; k <= SP_N; k++) {                   /* (lossless: bounded by the loop's input, no clamp) */
            int32_t v = (x - ap[j + k]) * SP_A;         /* towards 0, as the loop's gain: floors would feed */
            o = ap[j + k];                              /* the loop a little offset and noise for ever */
            x = ((v + ((v >> 31) & 4095)) >> 12) + p;
            p = o;
            ap[j + k] = x;
        }
        ln[wp & SP_MASK] = (int16_t)clamp(x, -32768, 32767);
        BUS_FX.sp_w = (uint16_t)(wp + 1u);
        out[i] += (t0 + (((t1 - t0) * f) >> 8)) * 4 + ln[(wp - (uint32_t)L3) & SP_MASK] * 2;
    }
}

/* the lines' gains for SIZE s: RT60 = 0.5 s * 2^(s / 30); a line of D samples (22.05 kHz) loses 60 dB in
 * RT60 * 22050 / D passes: g = 2^(-D * 3 log2(10) / (22050 * RT60)) = 2^(-D * 59.2 / 65536 * 2^(-s / 30)),
 * at most 0.99, over sqrt(7) (the matrix's rows have 7 entries of +-1: / sqrt(7) makes it orthogonal) */
static __attribute__((noinline)) void BUS_FN(hall_gains)(int32_t s)
{
    uint32_t q = exp2_neg((uint32_t)s * 65536u / 30u), k, g;
    for (k = 0; k < HL_NL; k++) {
        g = exp2_neg((HL_LEN[k] * 59u * q) >> 15);
        BUS_HL.g[k] = (int16_t)(((g > 32440u ? 32440u : g) * 6193u) >> 15);
    }
    BUS_HL.sz = (int16_t)(s + 1);
}

/* HALL (see the top): a block of the send (n = CTL samples: audio.c) -> the mid added to out, the side into
 * BUS_HL.sd. At half the rate (HL_NC samples) in passes over the block, each with few states (every line is
 * longer than a block: what the lines give in this block was written before it): the send, every other
 * sample, low cut, diffusers (BUS_HL.bx); the lines' ends at their drifting taps, low-passed, scaled (BUS_HL.y); the
 * outputs and the matrix; back into the lines; the outputs back to 44.1 kHz. */
static __attribute__((noinline)) void BUS_FN(rev_hall)(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    int16_t *b = BUS_COMB, *a = BUS_U.ap;
    uint32_t i, k, s, m, d;
    int32_t kd = HL_KD(song.g[G_RDAMP]);
    int32_t xp = BUS_HL.x, hp = BUS_HL.hp, he = BUS_HL.he, mp = BUS_HL.m, sp = BUS_HL.s;
    (void)n;
    if (BUS_HL.sz != SV_RSIZE + 1)
        BUS_FN(hall_gains)(SV_RSIZE);                   /* (SPACE: its SIZE; the lines' gains stay at most 0.99) */
    for (i = 0; i < HL_NC; i++) {
        int32_t x = (xp + 2 * rev_in[2u * i] + rev_in[2u * i + 1u]) >> 2, v;   /* every other sample, 1 2 1 */
        xp = rev_in[2u * i + 1u];
        x = mulq15(clamp(x, -262143, 262143), 2580);    /* (ROOM's level) */
        v = x - hp + he;                                /* the low cut, its step's remainder kept */
        he = v & 31;
        hp += v >> 5;
        BUS_HL.bx[i] = x - hp;
    }
    BUS_HL.x = xp, BUS_HL.hp = hp, BUS_HL.he = he;
    for (k = 0; k < HL_NAP; k++) {                      /* the diffusers */
        d = BUS_HL.d[k];
        HL_RUNS(d, HL_AP[k], m, s, hall_ap(a + d, HL_APG[k], BUS_HL.bx + s, m));
        BUS_HL.d[k] = (uint16_t)d;
        a += HL_AP[k];
    }
    BUS_HL.ph += HL_LFO;
    for (k = 0; k < HL_NL; k++) {                       /* the lines' ends */
        int32_t g = BUS_HL.g[k], f = 0, lo;
        if ((HL_LPM >> k) & 1u)
            d = BUS_HL.i[k];                                /* (low-passed: the line's end) */
        else {                                          /* (drifting: HL_MOD + 2 +- HL_MOD samples short of it) */
            uint32_t r = (uint32_t)(((HL_MOD + 2) << 8) +
                                    ((osc_sine(BUS_HL.ph * (1u + (k & 1u)) + k * 0x2C000000u) * HL_MOD) >> 7));
            f = (int32_t)(r & 255u);
            d = BUS_HL.i[k] + (r >> 8);
            d = d >= HL_LEN[k] ? d - HL_LEN[k] : d;
        }
        s = d + HL_LEN[k] / 2u;                         /* quiet? (two samples half a line apart) */
        s = s >= HL_LEN[k] ? s - HL_LEN[k] : s;
        lo = (b[d] < 0 ? -b[d] : b[d]) + (b[s] < 0 ? -b[s] : b[s]) < HL_LOUD;
        if ((HL_LPM >> k) & 1u) {
            HL_RUNS(d, HL_LEN[k], m, s, hall_end_lp(b + d, BUS_HL.y[k] + s, m, g, lo, &BUS_HL.lp[k], &BUS_HL.le[k], kd));
        } else {
            HL_RUNS(d, HL_LEN[k], m, s, hall_end(b + d, f, BUS_HL.y[k] + s, m, g, lo));
        }
        b += HL_LEN[k] + 1u;
    }
    for (i = 0; i < HL_NC; i++) {
        int32_t y0 = BUS_HL.y[0][i], y1 = BUS_HL.y[1][i], y2 = BUS_HL.y[2][i], y3 = BUS_HL.y[3][i];
        int32_t y4 = BUS_HL.y[4][i], y5 = BUS_HL.y[5][i], y6 = BUS_HL.y[6][i], y7 = BUS_HL.y[7][i], x = BUS_HL.bx[i];
        int32_t t, p0, p1, p2, p3, p4, p5, p6;
        BUS_HL.l[i] = y0 + y3 + y4 - y7;                    /* the outputs */
        BUS_HL.r[i] = y1 + y2 + y5 + y6;
        /* the matrix: C = [0 1; -1 Q], Q (7 x 7) = chi(j - i), the quadratic character mod 7 (+1 at distances
         * 1 2 4, -1 at 3 5 6): row 1 + i of Q y = 2 (the three at +1) - (the sum of the 7) + y_1+i */
        t = y1 + y2 + y3 + y4 + y5 + y6 + y7;
        p0 = y2 + y3 + y5, p1 = y3 + y4 + y6, p2 = y4 + y5 + y7, p3 = y5 + y6 + y1;
        p4 = y6 + y7 + y2, p5 = y7 + y1 + y3, p6 = y1 + y2 + y4;
        y0 += t;
        BUS_HL.y[0][i] = (int16_t)clamp(t + x, -32768, 32767);   /* (+ the send: signs spread over the rows) */
        BUS_HL.y[1][i] = (int16_t)clamp(2 * p0 + y1 - y0 + x, -32768, 32767);
        BUS_HL.y[2][i] = (int16_t)clamp(2 * p1 + y2 - y0 + x, -32768, 32767);
        BUS_HL.y[3][i] = (int16_t)clamp(2 * p2 + y3 - y0 + x, -32768, 32767);
        BUS_HL.y[4][i] = (int16_t)clamp(2 * p3 + y4 - y0 + x, -32768, 32767);
        BUS_HL.y[5][i] = (int16_t)clamp(2 * p4 + y5 - y0 + x, -32768, 32767);
        BUS_HL.y[6][i] = (int16_t)clamp(2 * p5 + y6 - y0 + x, -32768, 32767);
        BUS_HL.y[7][i] = (int16_t)clamp(2 * p6 + y7 - y0 - x, -32768, 32767);
    }
    b = BUS_COMB;
    for (k = 0; k < HL_NL; k++) {                       /* back into the lines (and the mirror after a wrap) */
        d = BUS_HL.i[k];
        HL_RUNS(d, HL_LEN[k], m, s, for (i = 0; i < m; i++) b[d + i] = BUS_HL.y[k][s + i]; if (!d) b[HL_LEN[k]] = b[0]);
        BUS_HL.i[k] = (uint16_t)d;
        b += HL_LEN[k] + 1u;
    }
    for (i = 0; i < HL_NC; i++) {                       /* back to 44.1 kHz, between samples */
        int32_t mm = (BUS_HL.l[i] + BUS_HL.r[i]) * HL_OUT, ss = (BUS_HL.l[i] - BUS_HL.r[i]) * HL_OUT;
        out[2u * i] += (mp + mm) >> 1;
        out[2u * i + 1u] += mm;
        BUS_HL.sd[2u * i] = (sp + ss) >> 1;
        BUS_HL.sd[2u * i + 1u] = ss;
        mp = mm;
        sp = ss;
    }
    BUS_HL.m = mp, BUS_HL.s = sp;
    BUS_HL.side = 1;
}

/* SHIM (see the top; ported from JIANT's rev_shimmer), added to out. ROOM on the send plus the octave-up return of its own last
 * block; the pitch shifter's two crossfaded grains read BUS_SHIM.buf at twice the speed it is written */
static __attribute__((noinline)) void BUS_FN(rev_shim)(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    int32_t in[CTL], t[CTL];
    uint32_t i, j;
    for (i = 0; i < n; i++) {
        in[i] = rev_in[i] + mulq15(BUS_SHIM.fb[i], SH_AMT);
        t[i] = 0;
    }
    BUS_FN(rev_room)(in, t, n);
    for (i = 0; i < n; i++) {
        int32_t x = 0, o = t[i] - BUS_FX.sh_hp + BUS_FX.sh_he;      /* the low cut, its step's remainder kept (exact silence) */
        BUS_FX.sh_he = o & 511;
        BUS_FX.sh_hp += o >> 9;
        t[i] -= BUS_FX.sh_hp;
        t[i] = t[i] > SH_OGATE ? t[i] - SH_OGATE : t[i] < -SH_OGATE ? t[i] + SH_OGATE : 0;   /* (ROOM's rounding cycles) */
        out[i] += t[i];
        BUS_SHIM.buf[BUS_FX.sh_w & (SH_LEN - 1u)] = (int16_t)clamp(t[i] >> 1, -32768, 32767);
        for (j = 0; j < 2u; j++) {                      /* the delay falls a sample a sample: read at twice the speed */
            uint32_t ph = (BUS_FX.sh_ph + j * (SH_LEN / 2u)) & (SH_LEN - 1u), d = SH_LEN - 1u - ph;
            int32_t w = (int32_t)(ph < SH_LEN / 2u ? ph : SH_LEN - 1u - ph);   /* the triangles: their sum flat */
            x += (BUS_SHIM.buf[(BUS_FX.sh_w - d) & (SH_LEN - 1u)] * w) >> 9;
        }
        BUS_FX.sh_ph++;
        BUS_FX.sh_w++;
        x = x > SH_GATE ? x - SH_GATE : x < -SH_GATE ? x + SH_GATE : 0;   /* a soft gate: no halo sustained by rounding */
        BUS_SHIM.fb[i] = x;                             /* (into the next block's input) */
    }
}

/* RESO (see the top; ported from JIANT's rev_reso), added to out. The scale is the one of the part this bus carries (bus A in
 * SPLIT: part 1; B: part 2; otherwise the first part that is not a drum kit) */
static __attribute__((noinline)) void BUS_FN(rev_reso)(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, per[4], mask, s, k0 = BUS_SPLITB, k1 = (BUS_SPLITB || dm_split) ? k0 + 1u : (uint32_t)NTRK;
    const track_t *t = &trk[k0];
    int32_t rt, sz = SV_RSIZE, dk = 32767 - song.g[G_RDAMP] * 200, note[4], deg = 0, nn;
    int32_t fb = sz <= 100 ? 26000 + sz * 52 : clamp(31200 + (sz - 100) * 58, 0, 32740);   /* 0.79 .. 0.9992 */
    for (k = k0; k < k1; k++)
        if (ENGINES[eng_idx(trk[k].eng_req)] != &ENG_DRUM) { t = &trk[k]; break; }
    mask = scale_mask(t) & 0xFFFu;
    rt = root_of(t);                                    /* (the global ROOT, core.h) */
    if (!mask) mask = 0xFFFu;
    for (nn = 48 + rt, k = 0, s = 0; k < 4u && s < 48u; s++, nn++)   /* the I, III, V, VII from C3 up */
        if ((mask >> (uint32_t)((nn - rt + 120) % 12)) & 1u) {
            if (deg % 2 == 0)
                note[k++] = nn;
            deg++;
        }
    for (; k < 4u; k++) note[k] = note[k - 1] + 12;
    for (k = 0; k < 4u; k++) {
        uint32_t inc = pitch_inc((uint32_t)clamp(note[k] * 16, 0, 2047));
        per[k] = 0xFFFFFFFFu / ((inc >> 8) | 1u);          /* the period, Q8 */
        if (per[k] > (RS_LEN - 2u) << 8) per[k] = (RS_LEN - 2u) << 8;
    }
    for (i = 0; i < n; i++) {
        int32_t in = mulq15(rev_in[i], 3600), a = 0;
        for (k = 0; k < 4u; k++) {
            int16_t *c = BUS_COMB + k * RS_LEN;
            uint32_t d = per[k] >> 8, f = per[k] & 255u;
            int32_t x0 = c[(BUS_FX.rs_w - d) & (RS_LEN - 1u)], x1 = c[(BUS_FX.rs_w - d - 1u) & (RS_LEN - 1u)];
            int32_t o = x0 + (((x1 - x0) * (int32_t)f) >> 8);
            BUS_FX.rs_lp[k] = o + mulq15(BUS_FX.rs_lp[k] - o, 32767 - dk);
            c[BUS_FX.rs_w & (RS_LEN - 1u)] = (int16_t)clamp(in + mulq15(BUS_FX.rs_lp[k], fb), -32768, 32767);
            a += o;
        }
        BUS_FX.rs_w++;
        out[i] += a;
    }
}

/* the model ty's block, added to out */
static inline void BUS_FN(rev_run)(uint32_t ty, const int32_t *rev_in, int32_t *out, uint32_t n)
{
    switch (ty) {
    case 1: BUS_FN(rev_spring)(rev_in, out, n); break;
    case 2: BUS_FN(rev_hall)(rev_in, out, n); break;
    case 3: BUS_FN(rev_shim)(rev_in, out, n); break;
    case 4: BUS_FN(rev_reso)(rev_in, out, n); break;
    default: BUS_FN(rev_room)(rev_in, out, n); break;
    }
}

/* the reverb's buffers and states to silence (the model changed) */
static void BUS_FN(rev_clear)(void)
{
    uint32_t i;
    memset(BUS_COMB, 0, sizeof BUS_COMB);               /* (memset: libc.c a word at a time; element loops here cost ~50 k instructions */
    memset(&BUS_U, 0, sizeof BUS_U);                    /* a bus in the audio interrupt: ap (int16: 997 of them, an odd count the int32 view */
    for (i = 0; i < 4u; i++)                            /* misses one of) and sp, the union's both views) */
        BUS_FX.comb_lp[i] = BUS_FX.rs_lp[i] = 0;
    BUS_FX.sh_hp = BUS_FX.sh_he = 0;
    memset(BUS_SHIM.buf, 0, sizeof BUS_SHIM.buf);       /* SHIM's line and its return */
    memset(BUS_SHIM.fb, 0, sizeof BUS_SHIM.fb);
    for (i = 0; i < HL_NL; i++)
        BUS_HL.lp[i] = BUS_HL.le[i] = 0;
    BUS_FX.sp_lp = BUS_FX.sp_hp = BUS_FX.sp_he = 0;
    BUS_HL.x = BUS_HL.m = BUS_HL.s = BUS_HL.hp = BUS_HL.he = 0;
}

/* the model changed (to rt): the old one's block fades out (HALL's stereo difference with it), its buffers are
 * cleared, the new one starts from silence */
static __attribute__((noinline)) void BUS_FN(rev_switch)(const int32_t *rev_in, int32_t *wet, uint32_t n, int32_t rt)
{
    int32_t *t = part_buf, g = 65536, d = 65536 / (int32_t)n;
    uint32_t i;
    for (i = 0; i < n; i++)
        t[i] = 0;
    BUS_FN(rev_run)(BUS_FX.rtype, rev_in, t, n);
    for (i = 0; i < n; i++, g -= d) {
        wet[i] += mulq16(t[i], (uint32_t)g);
        BUS_HL.sd[i] = mulq16(BUS_HL.sd[i], (uint32_t)g);   /* (HALL only: BUS_HL.side set) */
    }
    BUS_FN(rev_clear)();
    BUS_FX.rtype = (uint8_t)rt;
}

/* the chorus's DEPTH over a block (0.2.9): from the depth the last block reached (*cdq, Q8) to this block's (cdepth), *cst a sample
 * (the read point's sweep is (sine + 1) x depth: a depth that jumped at a block's start jumped the read point, up to ~3 samples a block
 * while PERFORM's SPACE or the matrix moved it: a click a block, a buzz at 1.4 kHz). The loops read (sine + 32768) x (cdq >> 4) >> 12: a
 * depth that holds (cst 0, cdq = cdepth << 8) gives (sine + 32768) x cdepth >> 8 exactly, as before (bit for bit). What a block cannot
 * reach (under 32 / 256 of a sample of depth) the next one starts from */
static inline void BUS_FN(cho_glide)(int32_t cdepth, int32_t *cdq, int32_t *cst)
{
    int32_t t = cdepth << 8, q = BUS_FX.cho_on ? BUS_FX.cho_dq : t;
    *cdq = q;
    *cst = (t - q) >> CTL_LOG2;
    BUS_FX.cho_dq = t;
    BUS_FX.cho_on = 1;
}

/* The delay's MODULATION (DUELL 0.2.5, SPEC 5c.2): the delay time wobbles, a tape / chorus-like movement, with its own small LFO (not
 * LFO 1 / 2, not the matrix): G_DMDEP DEPTH (0 off .. 100: the time swings +- 10 ms, never more than a quarter of the delay time, so a short
 * delay keeps its feel) and G_DMRATE RATE (the chorus's rates, 0.05 .. 3.4 Hz). DEPTH 0 and at rest: fx_buses_m runs the loop below it
 * as ever (bit for bit, no cost); this runs while DEPTH > 0 and while the depth glides back to 0.
 * Two taps read the line at dl + oL and dl + oR (linear interpolation, dly_tap): oL = dev sin(pl), oR = dev sin(pr + 1/2 turn), pr
 * running 1/16 faster than pl, so they start in opposite phase and drift: the two channels never wobble together. In STEREO and LAYER
 * (dsd given) the return is their mid, and half their difference goes to the left and from the right of the dry mix (the side, as
 * PING-PONG's, added here: mix_l, mix_r), the image widens; the line is fed back from the mid (every repeat is chorused again). In DUEL
 * (dsd 0: a part's own side) the left tap alone, bus B a quarter turn behind bus A. PING-PONG: the left return reads at dl + oL, the
 * loop's end at 2 dl + oR (its side goes through dsd as ever, SPREAD applies).
 * Per sample one or two interpolated reads (the read positions run on, one add each); the LFOs once a block: the offsets run on a
 * straight line between the block's two sine values from where they were (so a change of DEPTH, RATE or the delay time never jumps),
 * and the depth reached glides at most half a sample a block (0.6 s from 0 to the top): no click turning DEPTH. State per bus (fxs_t
 * dm_*): the buses never share it. */
#define DM_STEP 128                                     /* the depth glides at most 0.5 sample (Q8) a block */
static __attribute__((noinline)) void BUS_FN(dly_mod)(uint32_t dmask, uint32_t dl, const int32_t *cho_in, const int32_t *dly_in,
                                                      int32_t *wet, uint32_t n, uint32_t pp, int32_t *dsd)
{
    uint32_t i, st = dsd != 0, pl = BUS_FX.dm_pl + (BUS_SPLITB ? 0x40000000u : 0u), pr = BUS_FX.dm_pr + 0x80000000u;
    uint32_t inc = LFO_INC[clamp(song.g[G_DMRATE], 0, 127)], w = BUS_FX.dly_w, ppos, qpos, pstep, qstep;
    int32_t fb = SV_DFB, col = 2000 + song.g[G_DCOLOR] * 240, dmix = song.g[G_DMIX] * 258;
    int32_t cdepth = SV_CDEP, dev = clamp(song.g[G_DMDEP], 0, 100) * 1129, cap = (int32_t)(dl << 6);
    int32_t xl = BUS_FX.dm_xl, xr = BUS_FX.dm_xr, el, er, sl, sr, cdq, cst;
    uint32_t cinc = LFO_INC[song.g[G_CRATE] & 127] / CTL;
    int32_t room = (int32_t)(dmask - 2u - (pp ? dl << 1 : dl)) << 8;   /* the line's end: a tap never reads past the oldest sample */
    cap = cap > room ? (room > 0 ? room : 0) : cap;
    dev = dev > cap ? cap : dev;                        /* (the target, Q8 samples: 100 % = 441 samples = 10 ms, 1129 a percent) */
    BUS_FX.dm_dev += clamp(dev - BUS_FX.dm_dev, -DM_STEP, DM_STEP);
    dev = BUS_FX.dm_dev > cap ? cap : BUS_FX.dm_dev;
    BUS_FX.dm_dev = dev;
    BUS_FN(cho_glide)(cdepth, &cdq, &cst);
    el = ((sine_i(pl + inc) * (dev >> 3)) >> 12) << 8;  /* where the offsets are to be at the block's end (Q16) */
    er = ((sine_i(pr + inc + (inc >> 4)) * (dev >> 3)) >> 12) << 8;
    sl = (el - xl) >> CTL_LOG2;
    sr = (er - xr) >> CTL_LOG2;
    BUS_FX.dm_pl += inc;
    BUS_FX.dm_pr += inc + (inc >> 4);
    /* the read positions (Q16): w - delay; a sample on they move by 1 less the offset's step */
    ppos = (w << 16) - (dl << 16) - (uint32_t)(xl + sl);
    pstep = 65536u - (uint32_t)sl;
    qpos = (w << 16) - ((pp ? dl << 1 : dl) << 16) - (uint32_t)(xr + sr);
    qstep = 65536u - (uint32_t)sr;
    for (i = 0; i < n; i++) {
        int32_t y = 0, a, b, r, x;
        /* chorus: as fx_buses_m's */
        BUS_CHO[BUS_FX.cho_w & (CHO_LEN - 1u)] = (int16_t)clamp(cho_in[i] >> 1, -32768, 32767);
        BUS_FX.cho_ph += cinc;
        cdq += cst;
        r = (400 << 8) + (((osc_sine(BUS_FX.cho_ph) + 32768) * (cdq >> 4)) >> 12);
        {
            uint32_t ri = (uint32_t)r >> 8;
            int32_t f = r & 255, c0 = BUS_CHO[(BUS_FX.cho_w - ri) & (CHO_LEN - 1u)];
            int32_t c1 = BUS_CHO[(BUS_FX.cho_w - ri - 1u) & (CHO_LEN - 1u)];
            y += (c0 + (((c1 - c0) * f) >> 8)) << 1;
        }
        BUS_FX.cho_w++;
        if (pp) {                                       /* PING-PONG (STEREO / LAYER): the left return and the loop's end, each its own tap */
            int32_t yl, yr;
            a = dly_tap(BUS_DLY, dmask, ppos);
            b = dly_tap(BUS_DLY, dmask, qpos);
            BUS_FX.dly_lp += mulq15(b - BUS_FX.dly_lp, col);
            r = mulq15(BUS_FX.dly_lp, fb);
            BUS_DLY[w & dmask] = (int16_t)clamp((dly_in[i] >> 1) + mulq15(r, fb), -32768, 32767);
            yl = mulq15(a << 1, dmix);
            yr = mulq15(r << 1, dmix);
            y += (yl + yr) >> 1;
            dsd[i] = (yl - yr) >> 1;
        } else if (st) {                                /* two taps: the mid is the return, half the difference the side */
            a = dly_tap(BUS_DLY, dmask, ppos);
            b = dly_tap(BUS_DLY, dmask, qpos);
            x = (a + b) >> 1;
            BUS_FX.dly_lp += mulq15(x - BUS_FX.dly_lp, col);
            BUS_DLY[w & dmask] = (int16_t)clamp((dly_in[i] >> 1) + mulq15(BUS_FX.dly_lp, fb), -32768, 32767);
            y += mulq15(x << 1, dmix);
            r = mulq15(a - b, dmix);
            mix_l[i] += r;
            mix_r[i] -= r;
        } else {                                        /* DUEL: the one tap */
            x = dly_tap(BUS_DLY, dmask, ppos);
            BUS_FX.dly_lp += mulq15(x - BUS_FX.dly_lp, col);
            BUS_DLY[w & dmask] = (int16_t)clamp((dly_in[i] >> 1) + mulq15(BUS_FX.dly_lp, fb), -32768, 32767);
            y += mulq15(x << 1, dmix);
        }
        w++;
        ppos += pstep;
        qpos += qstep;
        wet[i] = y;
    }
    BUS_FX.dly_w = w;
    BUS_FX.dm_xl = xl + sl * (int32_t)n;
    BUS_FX.dm_xr = xr + sr * (int32_t)n;
}

/* process the three buses for one block (the delay line BUS_DLY[0 .. dmask], the delay dl); sends in, wet stereo-equal out.
 * pp (DUELL, SPEC 5c.2; STEREO and LAYER only, the callers of SPLIT pass 0 and the branch is gone): the delay PING-PONGs. The
 * mono send goes into one line of 2 x dl that feeds back from its end with the square of the feedback (the same decay in
 * time as the plain delay). The left return is the line at dl (the repeats 1, 3, 5 ...: the send, then the loop's turns), the
 * right the loop's own feedback tap (the repeats 2, 4, 6 ...: the line's end, low-passed, times FDBK). One line, no second
 * one. The delay's return is then no longer stereo-equal: wet gets its mid ((L + R) / 2) and dsd its side ((L - R) / 2,
 * added to the left and taken from the right: rev_side_mix does the same for HALL's). pp 0: the plain delay, bit for bit.
 * dl is at most half the line (the caller: delay_split). While the delay's modulation runs, fx_buses_mod (above) is called instead */
static inline void BUS_FN(fx_buses_m)(uint32_t dmask, uint32_t dl, const int32_t *cho_in, const int32_t *dly_in,
                                      const int32_t *rev_in, int32_t *wet, uint32_t n, uint32_t pp, int32_t *dsd)
{
    uint32_t i;
    int32_t fb = SV_DFB, col = 2000 + song.g[G_DCOLOR] * 240;   /* (SV_*: song.g's, or PERFORM's SPACE: fx.c sv) */
    int32_t dmix = song.g[G_DMIX] * 258;
    int32_t cdepth = SV_CDEP, rt, cdq, cst;
    uint32_t cinc = LFO_INC[song.g[G_CRATE] & 127] / CTL;
    BUS_FN(cho_glide)(cdepth, &cdq, &cst);
    for (i = 0; i < n; i++) {
        int32_t y = 0, x, r;
        /* chorus: modulated short delay, 5..15 ms */
        BUS_CHO[BUS_FX.cho_w & (CHO_LEN - 1u)] = (int16_t)clamp(cho_in[i] >> 1, -32768, 32767);
        BUS_FX.cho_ph += cinc;
        cdq += cst;
        r = (400 << 8) + (((osc_sine(BUS_FX.cho_ph) + 32768) * (cdq >> 4)) >> 12);   /* Q8 delay: read between samples */
        {
            uint32_t ri = (uint32_t)r >> 8;
            int32_t f = r & 255, c0 = BUS_CHO[(BUS_FX.cho_w - ri) & (CHO_LEN - 1u)];
            int32_t c1 = BUS_CHO[(BUS_FX.cho_w - ri - 1u) & (CHO_LEN - 1u)];
            y += (c0 + (((c1 - c0) * f) >> 8)) << 1;
        }
        BUS_FX.cho_w++;
        /* delay with a low-passed feedback */
        if (pp) {
            int32_t x2 = BUS_DLY[(BUS_FX.dly_w - 2u * dl) & dmask], yl, yr;
            x = BUS_DLY[(BUS_FX.dly_w - dl) & dmask];
            BUS_FX.dly_lp += mulq15(x2 - BUS_FX.dly_lp, col);
            r = mulq15(BUS_FX.dly_lp, fb);              /* the right return: the loop's end, once round */
            BUS_DLY[BUS_FX.dly_w & dmask] =
                (int16_t)clamp((dly_in[i] >> 1) + mulq15(r, fb), -32768, 32767);
            BUS_FX.dly_w++;
            yl = mulq15(x << 1, dmix);
            yr = mulq15(r << 1, dmix);
            y += (yl + yr) >> 1;
            dsd[i] = (yl - yr) >> 1;
        } else {
            x = BUS_DLY[(BUS_FX.dly_w - dl) & dmask];
            BUS_FX.dly_lp += mulq15(x - BUS_FX.dly_lp, col);
            BUS_DLY[BUS_FX.dly_w & dmask] =
                (int16_t)clamp((dly_in[i] >> 1) + mulq15(BUS_FX.dly_lp, fb), -32768, 32767);
            BUS_FX.dly_w++;
            y += mulq15(x << 1, dmix);
        }
        wet[i] = y;
    }
    rt = song.g[G_RTYPE];
    rt = rt >= 1 && rt <= 4 ? rt : 0;                   /* (an unknown value: ROOM) */
    BUS_HL.side = 0;
    if (rt != BUS_FX.rtype)
        BUS_FN(rev_switch)(rev_in, wet, n, rt);
    else
        BUS_FN(rev_run)((uint32_t)rt, rev_in, wet, n);
}

/* the same as fx_buses_m while the delay's MODULATION runs (dly_mod for the chorus and the delay, then the reverb as fx_buses_m's tail): a
 * function of its own, so the plain path above is not touched by it (DEPTH 0: bit for bit and the same cost as before) */
static __attribute__((noinline)) void BUS_FN(fx_buses_mod)(uint32_t dmask, uint32_t dl, const int32_t *cho_in, const int32_t *dly_in,
                                                           const int32_t *rev_in, int32_t *wet, uint32_t n, uint32_t pp, int32_t *dsd)
{
    int32_t rt;
    BUS_FN(dly_mod)(dmask, dl, cho_in, dly_in, wet, n, pp, dsd);
    rt = song.g[G_RTYPE];
    rt = rt >= 1 && rt <= 4 ? rt : 0;
    BUS_HL.side = 0;
    if (rt != BUS_FX.rtype)
        BUS_FN(rev_switch)(rev_in, wet, n, rt);
    else
        BUS_FN(rev_run)((uint32_t)rt, rev_in, wet, n);
}
