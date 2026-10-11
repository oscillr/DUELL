/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SPEAKER EQ (fx.c master_out; MENU > SPEAKER EQ, #42): FLAT LOWCUT BASS+.
 *   the response: sines 40 Hz .. 8 kHz through master_out in each mode. FLAT passes them all (only the ~2 Hz DC
 *   blocker); LOWCUT cuts below ~110 Hz (12 dB/oct), BASS+ below ~220 Hz; both leave 1 kHz and up alone.
 *   BASS+: a 55 Hz tone gains energy at 250 Hz..1 kHz, the sub is cut, the output settles back to 0 after the
 *   tone and nothing reaches full scale.
 * #180 (BASS+ distorted a kick): a full-scale kick pattern (the DRUM voices KICK PUNCH / ROUND, 4 hits, at full scale
 *   and twice that: a loud mix) through each mode. BASS+ keeps the samples off the rails (under 30000: the soft clip
 *   tops out at 31600) with no flat tops, and leaves the limiter no busier than LOWCUT (its envelope's peak at most
 *   +0.5 dB): the harmonics of 1.4 peaked at 1.5 .. 3.5 x the kick itself, and the limiter pulled the whole mix down
 *   by up to 16 dB on every hit. The harmonics' energy against the kick's: 1.4 -5 dB, now -11 dB.
 * The EQ is on the master: the speaker, the headphone / line out (the same DAC) and USB audio (uac_tap reads the
 * same output) all get it. */
#include <math.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static double goertzel_db(const int32_t *x, uint32_t n, double f)
{
    double w = 2.0 * cos(2.0 * M_PI * f / FS), s1 = 0, s2 = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double s0 = x[i] + w * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return 10.0 * log10(s1 * s1 + s2 * s2 - w * s1 * s2 + 1.0);
}

static int32_t buf[3][FS];

static double tone_db(uint32_t mode, double f)     /* gain of a -9 dB sine through master_out, dB */
{
    static int32_t y[FS / 2u];
    uint32_t i, n = FS / 2u, skip = FS / 8u;
    double in = 0, out;
    fx_lowcut = (uint8_t)mode;
    lc_l1 = lc_l2 = lc_r1 = lc_r2 = dc_l = dc_r = dce_l = dce_r = 0;
    memset(lce, 0, sizeof lce);
    memset(&sb, 0, sizeof sb);                   /* (BASS+'s state: DUELL step 5 made it a struct) */
    lim_env = LIM_T;
    for (i = 0; i < n; i++) {
        int32_t l = (int32_t)(6000.0 * sin(2.0 * M_PI * f * i / FS)), r = l;
        if (i >= skip)
            in += (double)l * l;
        master_out(&l, &r);
        y[i] = l;
    }
    for (out = 0, i = skip; i < n; i++)
        out += (double)y[i] * y[i];
    return 10.0 * log10(out / in);
}

static void master_reset(uint32_t mode)
{
    fx_lowcut = (uint8_t)mode;
    lc_l1 = lc_l2 = lc_r1 = lc_r2 = dc_l = dc_r = dce_l = dce_r = 0;
    memset(lce, 0, sizeof lce);
    memset(&sb, 0, sizeof sb);                   /* (BASS+'s state: DUELL step 5 made it a struct) */
    lim_env = LIM_T;
}

#define HD_MAX -9.0       /* the harmonics' energy against the kick's, dB (1.4: -5.0 .. -6.2) */
/* #180: a kick pattern (4 hits, one each 0.5 s) at 1 and 2 x full scale through each mode; fails counted */
static uint32_t kick_pattern(void)
{
    static const char *const NAME[2] = {"PUNCH", "ROUND"};
    static float f[2 * FS];
    static dv_coef_t c;
    static dv_voice_t v;
    static dv_metal_t mb;
    uint32_t type, gi, mode, i, k, fails = 0;
    for (type = 0; type < 2u; type++) {
        dv_param_t p;
        int32_t y[CTL], m[CTL], pk = 1;
        dv_default(&p, type);
        dv_setup(&c, &p);
        dv_init(&v, c.type);
        memset(&mb, 0, sizeof mb);
        dv_metal_tune(&mb, &c);
        for (i = 0; i < 2u * FS; i += CTL) {
            if (i % (FS / 2u) < CTL)
                dv_trigger(&v);
            dv_metal_run(&mb, m, CTL);
            dv_run(&c, &v, m, y, CTL);
            for (k = 0; k < CTL; k++) {
                f[i + k] = (float)y[k];
                pk = abs(y[k]) > pk ? abs(y[k]) : pk;
            }
        }
        for (gi = 1; gi <= 2u; gi++) {
            int32_t env[3], peak = 0;
            double ein, eb, hd;
            uint32_t flat = 0;
            for (mode = 0; mode < 3u; mode++) {
                int32_t last = 0;
                uint32_t run = 0;
                master_reset(mode);
                env[mode] = 0;
                for (i = 0; i < 2u * FS; i++) {
                    int32_t l = (int32_t)(f[i] * 32767.0f * (float)gi / (float)pk), r = l;
                    master_out(&l, &r);
                    env[mode] = lim_env > env[mode] ? lim_env : env[mode];
                    if (mode == 2u) {
                        peak = abs(l) > peak ? abs(l) : peak;
                        run = l == last && abs(l) > 16384 ? run + 1u : 0;
                        flat += run >= 2u;          /* 3 equal samples in a row up there */
                        last = l;
                    }
                }
            }
            master_reset(2);
            for (i = 0, ein = eb = 0; i < 2u * FS; i++) {   /* the harmonics alone, against the kick */
                int32_t l = (int32_t)(f[i] * 32767.0f * (float)gi / (float)pk), h = spk_bass(&sb, l);
                ein += (double)l * l;
                eb += (double)h * h;
            }
            hd = 10.0 * log10(eb / ein);
            printf("speaker: kick %s x%u: limiter peak FLAT %+.1f LOWCUT %+.1f BASS+ %+.1f dB, BASS+ peak %d, flat tops %u, "
                   "harmonics %.1f dB of the kick\n", NAME[type], gi, 20.0 * log10(env[0] / (double)LIM_T),
                   20.0 * log10(env[1] / (double)LIM_T), 20.0 * log10(env[2] / (double)LIM_T), peak, flat, hd);
            fails += peak >= 30000 || flat || env[2] > env[1] + env[1] / 16 || hd > HD_MAX;
        }
    }
    printf("speaker: #180 BASS+ headroom on a full-scale kick %s\n", fails ? "FAIL" : "ok");
    return fails;
}

int main(void)
{
    uint32_t mode, i, fails = 0;
    double h[3], sub[3];
    int32_t peak = 0, tail = 0;
    {   /* the response of the three modes */
        static const double F[8] = {40, 80, 110, 220, 440, 1000, 4000, 8000};
        static const char *const NAME[3] = {"FLAT", "LOWCUT", "BASS+"};
        double g[3][8];
        uint32_t k;
        for (mode = 0; mode < 3u; mode++) {
            printf("speaker: %-6s", NAME[mode]);
            for (k = 0; k < 8u; k++) {
                g[mode][k] = tone_db(mode, F[k]);
                printf(" %4.0f Hz %5.1f", F[k], g[mode][k]);
            }
            printf(" dB\n");
        }
        for (k = 0; k < 8u; k++)                        /* FLAT: flat */
            fails += fabs(g[0][k]) > 0.3;
        fails += g[1][0] > -9.0 || g[1][2] > -4.0 || g[1][2] < -8.0;   /* LOWCUT: ~-6 dB at 110 Hz, 40 Hz well down */
        fails += fabs(g[1][5]) > 0.5 || fabs(g[1][7]) > 0.5;          /* .. 1 kHz and up untouched */
        fails += g[2][3] > -4.0;                                       /* BASS+: ~220 Hz (the tones below it come */
        fails += g[2][4] < -3.0 || fabs(g[2][5]) > 1.0;                /* back as harmonics); 440 Hz only the */
        fails += fabs(g[2][6]) > 0.5 || fabs(g[2][7]) > 0.5;          /* high-pass (was -5.7 dB: the bass path */
        printf("speaker: FLAT flat, LOWCUT ~110 Hz, BASS+ ~220 Hz (440 Hz not cancelled), 1 kHz and up kept %s\n",
               fails ? "FAIL" : "ok");                                 /* cancelled it, #42) */
    }
    for (mode = 0; mode < 3u; mode++) {
        fx_lowcut = (uint8_t)mode;
        lc_l1 = lc_l2 = lc_r1 = lc_r2 = 0;
        lim_env = LIM_T;
        for (i = 0; i < FS; i++) {
            int32_t l = i < FS / 2u ? (int32_t)(12000.0 * sin(2.0 * M_PI * 55.0 * i / FS)) : 0, r = l;
            master_out(&l, &r);
            buf[mode][i] = l;
            if (mode == 2u) {
                peak = l < 0 ? (-l > peak ? -l : peak) : (l > peak ? l : peak);
                if (i > FS - 2000u)
                    tail = abs(l) > tail ? abs(l) : tail;
            }
        }
        sub[mode] = goertzel_db(buf[mode] + 4000, FS / 2u - 4000u, 55.0);
        h[mode] = 0;
        for (i = 5; i <= 17u; i += 2)               /* the odd harmonics 275 .. 935 Hz */
            h[mode] += pow(10.0, goertzel_db(buf[mode] + 4000, FS / 2u - 4000u, 55.0 * i) / 10.0);
        h[mode] = 10.0 * log10(h[mode]);
    }
    printf("speaker: 55 Hz tone, 275..935 Hz harmonics OFF %.1f LOWCUT %.1f BASS+ %.1f dB; 55 Hz OFF %.1f BASS+ %.1f dB; "
           "peak %d, tail %d\n", h[0], h[1], h[2], sub[0], sub[2], peak, tail);
    fails += h[2] < h[0] + 20.0;                    /* the harmonics are there */
    fails += sub[2] > sub[0] - 6.0;                 /* the sub is cut */
    fails += peak >= 32767 || tail > 4;
    printf("speaker: BASS+ %s\n", fails ? "FAIL" : "ok");
    fails += kick_pattern();
    return fails != 0;
}
