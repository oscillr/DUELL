/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.14, 0.2.16: the delay's MODES (core.h G_DMODE: 0 DIGITAL, 1 PING-PONG, 2 RANDOM, 3 OCT UP; fx_bus.c dly_ev, fx.c dlyev_*). On mode_test.c's rig.
 * The scenes keep 0.2.14's names for the test's own arguments (dm 0 = the plain delay, 1 RAND, 2 OCT+, 7 a stored value past the modes; pp: the old PING switch),
 * mapped by mapm() onto the 0.2.16 mode (dm 0 + pp = PING-PONG; dm 1 / 2 = RANDOM / OCT UP) and onto dly_plain_pp (the plain delay the events cross-fade from).
 *   build/host/dlyev_test   (DUELL/host_tests.sh "dlyev")
 *   DIGITAL / PING-PONG  (and a stored value past OCT UP, and RANDOM / OCT UP in DUEL = SPLIT): the render is bit for bit the delay as before (hashes of the base commit
 *              02f849e, built with -DDLYEV_BASE=1; the same hashes in 0.2.16, where PING off / on became DIGITAL / PING-PONG): plain, PING-PONG, PING-PONG + modulation, LAYER, DUEL
 *   DETERMINISM  the same settings render the same twice (RAND, OCT+), RAND and OCT+ differ from DELAY and from each other; CHANCE 0 fires no event
 *   CLOCK      the first edge comes one delay time after PLAY (samples), twice that at half the tempo; TIME 1/4 doubles it
 *   CLICK      DELAY -> RAND -> OCT+ -> DELAY in jumps (also with PING-PONG on): no sample step above the plain delay's own (x 1.3 + 50)
 *   TAILS      silence after the tails (FDBK 40) in both modes: the output is 0 at the end
 *   LOUD       FDBK 120, COLR 127, MIX 127, CHANCE 100, loud chords: bounded under full scale, no wrap
 *   SENDS/PANIC  SENDS OFF and STOP ALL's flush clear the voices, the generator, the line and the crossfade; the output is silent until a new note
 *   UI         the cards: DEPTH is CHANCE in the event modes, PING and RATE fade with the mode's name, MODE fades in DUEL, the page, SENDS OFF
 *   COST       instructions per sample of the whole mix, a plucked part: DELAY, PING-PONG, RAND, OCT+ (CHANCE 0 / 50 / 100) */
#define MODE_TEST_NO_MAIN 1
#include "mode_test.c"

#ifdef DLYEV_BASE
#define SET_MODE(m) ((void)0)
#define CLR_EV() ((void)0)
#else
static int t_pp;                                        /* the old PING switch of the scene running: dm 0 is PING-PONG with it */
static int mapm(int m) { return m == 0 ? (t_pp ? DMD_PING : DMD_DIGITAL) : m == 1 ? DMD_RAND : m == 2 ? DMD_OCTUP : m; }
#define SET_MODE(m) do { song.g[G_DMODE] = (int16_t)mapm(m); } while (0)
#define CLR_EV() do { dlyev_clear(); t_pp = 0; dly_plain_pp = 0; } while (0)
#endif

/* the hashes of the delay before the modes (build this file on the base commit with -DDLYEV_BASE=1 to print them) */
#define H_PLAIN 0xf0dbf91d23f4f72dull
#define H_PP 0xaea52df65b15556full
#define H_PPMOD 0xe4f21e4fcf70a0dbull
#define H_LAYER 0xf0dbf91d23f4f72dull
#define H_DUEL 0xa03f89dc033b06b0ull

static uint64_t hash_of(const int32_t *l, const int32_t *r, uint32_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < n; i++) {
        h ^= (uint32_t)l[i];
        h *= 1099511628211ull;
        h ^= (uint32_t)r[i];
        h *= 1099511628211ull;
    }
    return h;
}
static int32_t step_of(const int32_t *x, uint32_t n)
{
    int32_t m = 0;
    for (uint32_t i = 1; i < n; i++) {
        int32_t d = x[i] - x[i - 1];
        m = (d < 0 ? -d : d) > m ? (d < 0 ? -d : d) : m;
    }
    return m;
}
static double rms_of(const int32_t *x, uint32_t a, uint32_t b)
{
    double s = 0;
    for (uint32_t i = a; i < b; i++)
        s += (double)x[i] * x[i];
    return b > a ? sqrt(s / (b - a)) : 0.0;
}

/* a plucked part (ANALOG PLUCK, every note a delay send at 127) for `sec` seconds: a note every 0.5 s, 120 ms long. dm: the mode at the start, chg[k]: the mode
 * from second k+1 on (-1: no change). dep: DEPTH / CHANCE */
static void scene(uint32_t mode, int dm, int pp, int dep, int fdbk, int chance_sec, const int *chg, uint32_t sec)
{
    uint32_t k, ms = FS / 1000u, b, nb = sec * 1000u * ms / CTL, nt = 0;
    dsp_clean(mode);
    CLR_EV();
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    song.g[G_DTIME] = 1;                                /* 1/8: 250 ms */
    song.g[G_DFDBK] = (int16_t)fdbk;
    song.g[G_DMIX] = 127;
    song.g[G_DCOLOR] = 90;
    t_pp = pp;
    dly_plain_pp = (uint8_t)pp;
    song.g[G_PINGSPREAD] = 100;
    song.g[G_RTYPE] = 0;
    song.g[G_DMRATE] = 44;
    song.g[G_DMDEP] = (int16_t)dep;
    (void)chance_sec;
    for (k = 0; k < 2u; k++) {
        host_preset(&trk[k], eng_of("ANALOG"), 8);
        trk[k].p[P_TRIM] = 100;
        trk[k].p[P_PAN] = 0;
        trk[k].p[P_AMODE] = 0;
        trk[k].p[P_CHOR] = trk[k].p[P_REV] = 0;
        trk[k].p[P_DLY] = 127;
    }
    SET_MODE(dm);
    rec_n = 0;
    for (b = 0; b < nb; b++) {
        uint32_t t = b * CTL, sidx = t / (FS / 2u), within = t % (FS / 2u);
        if (within < CTL) {
            trk_note_on(&trk[0], sidx % 2u ? 55u : 52u, 100);
            if (mode != DM_SPLIT)
                trk_note_on(&trk[0], sidx % 2u ? 62u : 59u, 100);
            nt++;
        }
        if (within >= 120u * ms && within < 120u * ms + CTL) {
            trk_note_off(&trk[0], sidx % 2u ? 55u : 52u);
            if (mode != DM_SPLIT)
                trk_note_off(&trk[0], sidx % 2u ? 62u : 59u);
        }
        if (chg && b % (1000u * ms / CTL) == 0 && b / (1000u * ms / CTL) >= 1u && b / (1000u * ms / CTL) <= 8u) {
            int v = chg[b / (1000u * ms / CTL) - 1u];
            if (v >= 100)
                song.g[G_DMODE] = (int16_t)(v - 100);       /* (100 + a 0.2.16 mode, as it is) */
            else if (v >= 0)
                SET_MODE(v);
        }
        mblk(1);
    }
}
static double cost(uint32_t mode, int dm, int pp, int dep)
{
    uint64_t i0;
    uint32_t nb = 2u * FS / CTL;
    scene(mode, dm, pp, dep, 60, 0, 0, 1);
    i0 = instr_now();
    {
        uint32_t b;
        for (b = 0; b < nb; b++) {
            if (b % (FS / 2u / CTL) == 0) {
                trk_note_on(&trk[0], 52, 100);
                trk_note_on(&trk[0], 59, 100);
            }
            if (b % (FS / 2u / CTL) == (FS / 8u / CTL)) {
                trk_note_off(&trk[0], 52);
                trk_note_off(&trk[0], 59);
            }
            mix_block(ob, CTL);
        }
    }
    return i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
}
static int32_t keep_l[MAXB * CTL], keep_r[MAXB * CTL];

int main(void)
{
    uint32_t n;
    int32_t plain_step;
    uint64_t hp, hpp, hppm, hl, hd;
    double rms_plain, rms_r0;
    scene(DM_STEREO, 0, 0, 0, 60, 0, 0, 3);
    n = rec_n;
    hp = hash_of(rec_l, rec_r, n);
    plain_step = step_of(rec_l, n);
    rms_plain = rms_of(rec_l, 0, n);
    memcpy(keep_l, rec_l, sizeof(int32_t) * n);
    memcpy(keep_r, rec_r, sizeof(int32_t) * n);
    scene(DM_STEREO, 0, 1, 0, 60, 0, 0, 3);
    hpp = hash_of(rec_l, rec_r, rec_n);
    scene(DM_STEREO, 0, 1, 60, 60, 0, 0, 3);
    hppm = hash_of(rec_l, rec_r, rec_n);
    scene(DM_LAYER, 0, 0, 0, 60, 0, 0, 3);
    hl = hash_of(rec_l, rec_r, rec_n);
    scene(DM_SPLIT, 0, 0, 0, 60, 0, 0, 3);
    hd = hash_of(rec_l, rec_r, rec_n);
    printf("dlyev: base hashes: plain %016llx, PING-PONG %016llx, PING-PONG + DEPTH 60 %016llx, LAYER %016llx, DUEL %016llx\n", (unsigned long long)hp,
           (unsigned long long)hpp, (unsigned long long)hppm, (unsigned long long)hl, (unsigned long long)hd);
#ifdef DLYEV_BASE
    printf("dlyev: BASE cost: plain %.1f, PING-PONG %.1f\n", cost(DM_STEREO, 0, 0, 0), cost(DM_STEREO, 0, 1, 0));
    return 0;
#endif
#ifndef DLYEV_BASE
    expect("DELAY: plain, PING-PONG, PING-PONG + modulation, LAYER, DUEL are bit for bit the delay before the modes (the base commit's hashes)",
           hp == H_PLAIN && hpp == H_PP && hppm == H_PPMOD && hl == H_LAYER && hd == H_DUEL);
    scene(DM_STEREO, 0, 0, 100, 60, 0, 0, 3);
    scene(DM_STEREO, 7, 0, 0, 60, 0, 0, 3);
    expect("DELAY: a stored mode past OCT+ (7) plays as DELAY, bit for bit", hash_of(rec_l, rec_r, rec_n) == hp);
    scene(DM_SPLIT, 1, 0, 0, 60, 0, 0, 3);
    expect("DUEL: RAND stored: the plain delay of DUEL, bit for bit (the events are STEREO / LAYER only)", hash_of(rec_l, rec_r, rec_n) == hd);
    scene(DM_SPLIT, 2, 0, 100, 60, 0, 0, 3);
    {
        uint64_t a = hash_of(rec_l, rec_r, rec_n);
        scene(DM_SPLIT, 0, 0, 100, 60, 0, 0, 3);
        expect("DUEL: OCT+ stored, CHANCE 100: DUEL's delay with DEPTH 100 (the modulation), bit for bit; the right channel is silence",
               a == hash_of(rec_l, rec_r, rec_n) && peak_of(rec_r, rec_n) == 0);
    }

    /* determinism and difference */
    {
        uint64_t a, b, c, d;
        double rl_r;
        scene(DM_STEREO, 1, 0, 50, 60, 0, 0, 4);
        a = hash_of(rec_l, rec_r, rec_n);
        memcpy(keep_l, rec_l, sizeof(int32_t) * rec_n);
        scene(DM_STEREO, 1, 0, 50, 60, 0, 0, 4);
        b = hash_of(rec_l, rec_r, rec_n);
        expect("RAND: the same settings render the same twice (deterministic generator)", a == b);
        scene(DM_STEREO, 2, 0, 50, 60, 0, 0, 4);
        c = hash_of(rec_l, rec_r, rec_n);
        scene(DM_STEREO, 2, 0, 50, 60, 0, 0, 4);
        d = hash_of(rec_l, rec_r, rec_n);
        expect("OCT+: the same settings render the same twice", c == d);
        scene(DM_STEREO, 0, 0, 0, 60, 0, 0, 4);
        expect("RAND and OCT+ differ from DELAY and from each other", a != hash_of(rec_l, rec_r, rec_n) && c != hash_of(rec_l, rec_r, rec_n) && a != c);
        scene(DM_STEREO, 1, 0, 0, 60, 0, 0, 4);
        rms_r0 = rms_of(rec_l, 0, rec_n);
        scene(DM_STEREO, 0, 0, 0, 60, 0, 0, 4);
        rl_r = rms_of(rec_l, 0, rec_n);
        printf("dlyev: CHANCE 0 RAND rms %.0f against DELAY %.0f\n", rms_r0, rl_r);
        expect("RAND CHANCE 0 (no event): the level is the plain delay's within 1 dB (the plain voice; the right channel 10 ms behind)",
               rms_r0 > 0.0 && fabs(20.0 * log10(rms_r0 / rl_r)) < 1.0);
        scene(DM_STEREO, 1, 0, 100, 60, 0, 0, 4);
        expect("RAND CHANCE 100: bounded, the level within 4 dB of the plain delay's", peak_of(rec_l, rec_n) < 32767 && fabs(20.0 * log10(rms_of(rec_l, 0, rec_n) / rl_r)) < 4.0);
        scene(DM_STEREO, 1, 0, 0, 60, 0, 0, 4);
        {
            double d0 = 0;
            for (uint32_t i = 0; i < rec_n; i++)
                d0 = fabs((double)rec_l[i] - rec_r[i]) > d0 ? fabs((double)rec_l[i] - rec_r[i]) : d0;
            expect("RAND: the image is stereo (Haas: left and right differ)", d0 > 300.0);
        }
        song.g[G_PINGSPREAD] = 0;
        scene(DM_STEREO, 1, 0, 70, 60, 0, 0, 3);
        song.g[G_PINGSPREAD] = 0;
        {
            /* scene() sets SPREAD 100; set 0 and render again by hand */
            uint32_t b2, nb = 3u * FS / CTL;
            double d0 = 0;
            dsp_clean(DM_STEREO);
            CLR_EV();
            song.g[G_BPM] = 120; song.g[G_DTIME] = 1; song.g[G_DFDBK] = 60; song.g[G_DMIX] = 127; song.g[G_DCOLOR] = 90; song.g[G_PINGSPREAD] = 0;
            song.g[G_DMDEP] = 70; SET_MODE(1);
            host_preset(&trk[0], eng_of("ANALOG"), 8);
            trk[0].p[P_DLY] = 127; trk[0].p[P_REV] = trk[0].p[P_CHOR] = 0; trk[0].p[P_TRIM] = 100;
            rec_n = 0;
            for (b2 = 0; b2 < nb; b2++) {
                if (b2 % (FS / 2u / CTL) == 0) trk_note_on(&trk[0], 52, 100);
                if (b2 % (FS / 2u / CTL) == FS / 8u / CTL) trk_note_off(&trk[0], 52);
                mblk(1);
            }
            for (uint32_t i = 0; i < rec_n; i++)
                d0 = fabs((double)rec_l[i] - rec_r[i]) > d0 ? fabs((double)rec_l[i] - rec_r[i]) : d0;
            expect("RAND, SPREAD 0: left and right equal (the side is SPREAD's)", d0 == 0.0 && peak_of(rec_l, rec_n) > 1000);
        }
    }

    /* the clock: the first edge, counted from PLAY */
    {
        uint32_t first[4], c;
        for (c = 0; c < 4u; c++) {
            uint32_t b;
            dsp_clean(DM_STEREO);
            CLR_EV();
            song.g[G_BPM] = c == 1u ? 60 : 120;
            song.g[G_CLOCK] = 0;
            song.g[G_DTIME] = c == 2u ? 0 : c == 3u ? 2 : 1;      /* 1/8, 1/4, 1/16 */
            song.g[G_DMDEP] = 100;
            SET_MODE(1);
            host_preset(&trk[0], eng_of("ANALOG"), 8);
            trk[0].p[P_DLY] = 0;
            mblk(37u);                                           /* (some blocks before PLAY: the clock runs free) */
            dlyev_start();
            first[c] = 0;
            for (b = 0; b < 8u * FS / CTL; b++) {
                mblk(1);
                if (!first[c] && (dlyev.ev1 != 4u || dlyev.ev0 != 4u))
                    first[c] = b + 1u;
            }
        }
        printf("dlyev: first edge after PLAY, blocks of %d samples: 1/8 at 120 BPM %u, 1/8 at 60 %u, 1/4 at 120 %u, 1/16 at 120 %u\n", (int)CTL, first[0], first[1],
               first[2], first[3]);
        /* 1/8 at 120 BPM = 11025 samples = 172.3 blocks; the edge's event is seen in the block after the edge */
        expect("CLOCK: the first edge is one delay time after PLAY (1/8 at 120 BPM: 11025 samples)", first[0] >= 11025u / CTL && first[0] <= 11025u / CTL + 3u);
        expect("CLOCK: half the tempo, twice the time (1/8 at 60 BPM)", first[1] >= 22050u / CTL && first[1] <= 22050u / CTL + 3u);
        expect("CLOCK: TIME 1/4 doubles it, 1/16 halves it", first[2] >= 22050u / CTL && first[2] <= 22050u / CTL + 3u && first[3] >= 5512u / CTL && first[3] <= 5512u / CTL + 3u);
    }

    /* what the events do: a sine burst straight into the delay bus (fx_buses), no parts */
    {
        static int32_t in[CTL], zero[CTL], wl[96000];
        uint32_t t, k, cnt[2] = {0, 0}, b;
        uint32_t ev[4] = {0, 0, 0, 0}, nev = 0, ups = 0, panok = 1;
        int32_t pmax = 0;
        for (k = 0; k < 2u; k++) {                                /* 0: RAND with CHANCE 0 (the plain echo), 1: OCT+ CHANCE 100 */
            dsp_clean(DM_STEREO);
            CLR_EV();
            song.g[G_BPM] = 120; song.g[G_CLOCK] = 0; song.g[G_DTIME] = 1; song.g[G_DFDBK] = 0; song.g[G_DMIX] = 127; song.g[G_DCOLOR] = 127;
            song.g[G_PINGSPREAD] = 100; song.g[G_RTYPE] = 0; song.g[G_RLOCUT] = 0; song.g[G_DMRATE] = 44;
            song.g[G_DMDEP] = k ? 100 : 0;
            SET_MODE(k ? 2 : 1);
            memset(zero, 0, sizeof zero);
            fx_buses(zero, zero, zero, wet, CTL);                 /* (the start: x ramps, the voices) */
            dlyev_start();
            for (t = 0; t < 30000u; t += CTL) {
                for (b = 0; b < CTL; b++)
                    in[b] = t + b < 13230u ? (int32_t)(12000.0 * sin(6.283185307 * 220.5 * (double)(t + b) / 44100.0)) : 0;
                for (b = 0; b < CTL; b++) wet[b] = 0;
                fx_buses(zero, in, zero, wet, CTL);
                for (b = 0; b < CTL; b++)
                    wl[t + b] = wet[b] + dly_sd[b];
            }
            for (t = 17500u + 1; t < 22000u; t++)
                cnt[k] += (wl[t] < 0) != (wl[t - 1] < 0);
        }
        printf("dlyev: a 220.5 Hz burst: zero crossings in samples 17500..22000: plain echo %u, OCT+ %u\n", cnt[0], cnt[1]);
        expect("EVENTS OCT+: the repeat is an octave up (twice the zero crossings of the plain echo)", cnt[0] > 30 && cnt[1] > 3u * cnt[0] / 2u && cnt[1] < 5u * cnt[0] / 2u);
        CLR_EV();
        dlyev.rng = 0x2545F491u;
        for (k = 0; k < 4000u; k++) {                           /* the draws: RAND, CHANCE 50 % (thr 16384): about half, the four events evenly */
            dlyev.md = 0;
            dlyev.ev1 = 4;
            dlyev_edge(1, 16384);
            if (dlyev.ev1 != 4) {
                nev++;
                ev[dlyev.ev1 & 3u]++;
            }
        }
        expect("EVENTS RAND: CHANCE 50 fires on about half the edges, the four events evenly (RETRIG, REVERSE, UP, DOWN)",
               nev > 1800u && nev < 2200u && ev[0] > 400u && ev[1] > 400u && ev[2] > 400u && ev[3] > 400u && ev[0] + ev[1] + ev[2] + ev[3] == nev);
        for (k = 0; k < 4000u; k++) {                           /* OCT+: always UP, the pan inside +-0.6 */
            dlyev_edge(2, 32767);
            ups += dlyev.ev1 == 2;
            pmax = dlyev.v[dlyev.cur].npan > pmax ? dlyev.v[dlyev.cur].npan : -dlyev.v[dlyev.cur].npan > pmax ? -dlyev.v[dlyev.cur].npan : pmax;
            panok &= dlyev.v[dlyev.cur].npan >= -19661 && dlyev.v[dlyev.cur].npan <= 19661;
        }
        printf("dlyev: OCT+, CHANCE 100: %u of 4000 edges up, largest pan %d of 32768\n", ups, pmax);
        expect("EVENTS OCT+: CHANCE 100 is an octave up on every edge, the pan inside +-0.6 and spread over it", ups == 4000u && panok && pmax > 18000);
        CLR_EV();
        for (k = 0; k < 4000u; k++)
            dlyev_edge(1, 0);
        expect("EVENTS: CHANCE 0 never fires", dlyev.ev1 == 4 && dlyev.ev0 == 4);
    }

    /* the voices on a ramp in the line (value = position): each event's direction and speed */
    {
        const uint32_t W = 40000u, DL = 3000u, PER = 3000u;
        uint32_t ev, ok_all = 1, k;
        int32_t first[5], step_sum[5];
        for (ev = 0; ev < 5u; ev++) {
            dlyev_v_t v;
            int32_t c, l, r, prev = 0, sum = 0;
            for (k = 0; k < 65536u; k++) dly_buf[k] = (int16_t)((int32_t)k - 32768);
            memset(&v, 0, sizeof v);
            v.nev = (uint8_t)ev;
            dlyev_in(&v, W << 16, DL << 16, PER);
            for (k = 0; k < 1100u; k++) {                     /* (the fade-in, then 76 samples at full level) */
                c = l = r = 0;
                dlyev_read(&v, W << 16, DL << 16, &c, &l, &r);
                if (k >= 1030u) {
                    if (k > 1030u) sum += c - prev;
                    else first[ev] = c;
                    prev = c;
                }
            }
            step_sum[ev] = sum;
            ok_all &= !v.fin;
        }
        printf("dlyev: voices on a ramp: read after the fade (RETRIG %d, REVERSE %d, UP %d, DOWN %d, NONE %d); change over 69 samples: %d %d %d %d %d\n", first[0], first[1], first[2],
               first[3], first[4], step_sum[0], step_sum[1], step_sum[2], step_sum[3], step_sum[4]);
        expect("VOICES: NONE reads one delay time back and stands still on the line; RETRIG 1 1/8 of it back", ok_all && step_sum[4] == 0 && first[4] == (int32_t)(W - DL) - 32768 && step_sum[0] == 0 &&
               first[0] == (int32_t)(W - DL - DL / 8u) - 32768);
        expect("VOICES: REVERSE runs backwards one sample a sample, UP two forwards (an octave up), DOWN half (an octave down)",
               step_sum[1] == -69 && step_sum[2] == 138 && step_sum[3] >= 33 && step_sum[3] <= 36);
    }

    /* no click switching */
    {
        static const int chg[8] = {1, 2, 0, 1, 0, 2, 0, -1};
        int32_t st_plain, st_r, st_o, st_sw, st_swp, st_plainp, st_rp, st_op, base, basep;
        double pk;
        scene(DM_STEREO, 0, 0, 40, 60, 0, 0, 9);
        st_plain = step_of(rec_l, rec_n);
        scene(DM_STEREO, 1, 0, 40, 60, 0, 0, 9);
        st_r = step_of(rec_l, rec_n);
        scene(DM_STEREO, 2, 0, 40, 60, 0, 0, 9);
        st_o = step_of(rec_l, rec_n);
        base = st_plain > st_r ? st_plain : st_r;
        base = base > st_o ? base : st_o;                   /* (the events' own steps: an octave up has twice the slope of the attack it repeats) */
        scene(DM_STEREO, 0, 0, 40, 60, 0, chg, 9);
        st_sw = step_of(rec_l, rec_n);
        pk = peak_of(rec_l, rec_n);
        scene(DM_STEREO, 0, 1, 40, 60, 0, 0, 9);
        st_plainp = step_of(rec_l, rec_n);
        scene(DM_STEREO, 1, 1, 40, 60, 0, 0, 9);
        st_rp = step_of(rec_l, rec_n);
        scene(DM_STEREO, 2, 1, 40, 60, 0, 0, 9);
        st_op = step_of(rec_l, rec_n);
        basep = st_plainp > st_rp ? st_plainp : st_rp;
        basep = basep > st_op ? basep : st_op;
        scene(DM_STEREO, 0, 1, 40, 60, 0, chg, 9);
        st_swp = step_of(rec_l, rec_n);
        printf("dlyev: switching DELAY/RAND/OCT+ in jumps (CHANCE 40): largest step %d (steady: DELAY %d, RAND %d, OCT+ %d); PING-PONG on: %d (steady %d, %d, %d)\n", st_sw,
               st_plain, st_r, st_o, st_swp, st_plainp, st_rp, st_op);
        expect("CLICK: modes switched in jumps, no click (largest step under 1.3 x the largest steady mode's + 50)", st_sw < 13 * base / 10 + 50 && pk < 32767);
        expect("CLICK: the same with PING-PONG on (the crossfade runs the ping-pong's return)", st_swp < 13 * basep / 10 + 50);
        {                                                   /* 0.2.16: DIGITAL <-> PING-PONG is a mode switch now (was the PING switch): the same smoothness as before, and all four modes in every order */
            static const int chg2[8] = {101, 100, 101, 100, 101, 100, 101, -1};
            static const int chg3[8] = {103, 101, 102, 100, 103, 100, 101, -1};
            int32_t st_tg, st_all, bt = st_plain > st_plainp ? st_plain : st_plainp;
            scene(DM_STEREO, 0, 0, 0, 60, 0, chg2, 9);
            st_tg = step_of(rec_l, rec_n);
            scene(DM_STEREO, 0, 0, 40, 60, 0, chg3, 9);
            st_all = step_of(rec_l, rec_n);
            printf("dlyev: DIGITAL <-> PING-PONG in jumps: largest step %d (steady: DIGITAL %d, PING-PONG %d); all four modes in jumps: %d (base %d)\n", st_tg, st_plain, st_plainp, st_all, base > basep ? base : basep);
            expect("CLICK: DIGITAL <-> PING-PONG switched in jumps, no click (under 1.3 x the steady steps + 50: the old PING switch's smoothness)", st_tg < 13 * bt / 10 + 50);
            expect("CLICK: DIGITAL, PING-PONG, RANDOM, OCT UP in any order, no click", st_all < 13 * (base > basep ? base : basep) / 10 + 50);
        }
    }

    /* tails and loud input */
    {
        uint32_t m;
        for (m = 1; m <= 2u; m++) {
            uint32_t b, nb = 14u * FS / CTL;
            int32_t pk_end;
            dsp_clean(DM_STEREO);
            CLR_EV();
            song.g[G_BPM] = 120; song.g[G_DTIME] = 1; song.g[G_DFDBK] = 40; song.g[G_DMIX] = 127; song.g[G_DCOLOR] = 90; song.g[G_DMDEP] = 100;
            song.g[G_PINGSPREAD] = 100; song.g[G_RTYPE] = 0;
            host_preset(&trk[0], eng_of("ANALOG"), 8);
            trk[0].p[P_DLY] = 127; trk[0].p[P_REV] = trk[0].p[P_CHOR] = 0; trk[0].p[P_TRIM] = 100;
            SET_MODE(m);
            rec_n = 0;
            for (b = 0; b < nb; b++) {
                if (b == 0) { trk_note_on(&trk[0], 52, 100); trk_note_on(&trk[0], 59, 100); }
                if (b == (FS / 8u) / CTL) { trk_note_off(&trk[0], 52); trk_note_off(&trk[0], 59); }
                mblk(1);
            }
            pk_end = peak_of(rec_l + rec_n - FS, FS) > peak_of(rec_r + rec_n - FS, FS) ? peak_of(rec_l + rec_n - FS, FS) : peak_of(rec_r + rec_n - FS, FS);
            printf("dlyev: mode %u, CHANCE 100, FDBK 40: peak %d, the last second's peak %d\n", m, (int)peak_of(rec_l, rec_n), (int)pk_end);
            expect(m == 1u ? "TAILS RAND: after a note and the tails (14 s, FDBK 40, CHANCE 100) the last second is silent" : "TAILS OCT+: the same", pk_end == 0);
        }
        for (m = 0; m <= 2u; m++) {
            uint32_t b, nb = 6u * FS / CTL, ov = 0;
            int32_t pk = 0;
            dsp_clean(DM_STEREO);
            CLR_EV();
            song.g[G_BPM] = 120; song.g[G_DTIME] = m == 2u ? 2 : 1; song.g[G_DFDBK] = 120; song.g[G_DMIX] = 127; song.g[G_DCOLOR] = 127; song.g[G_DMDEP] = m ? 100 : 0;
            song.g[G_PINGSPREAD] = 100; song.g[G_RTYPE] = 0;
            host_preset(&trk[0], eng_of("ANALOG"), 8);
            trk[0].p[P_DLY] = 127; trk[0].p[P_REV] = trk[0].p[P_CHOR] = 0; trk[0].p[P_TRIM] = 127;
            SET_MODE(m);
            rec_n = 0;
            for (b = 0; b < nb; b++) {
                if (b % (FS / 4u / CTL) == 0) { trk_note_on(&trk[0], 40, 127); trk_note_on(&trk[0], 47, 127); trk_note_on(&trk[0], 52, 127); }
                if (b % (FS / 4u / CTL) == (FS / 16u) / CTL) { trk_note_off(&trk[0], 40); trk_note_off(&trk[0], 47); trk_note_off(&trk[0], 52); }
                mblk(1);
            }
            pk = peak_of(rec_l, rec_n) > peak_of(rec_r, rec_n) ? peak_of(rec_l, rec_n) : peak_of(rec_r, rec_n);
            for (uint32_t i = 0; i < 65536u; i++)
                ov += dly_buf[i] == 32767 || dly_buf[i] == -32768;
            printf("dlyev: LOUD mode %u: FDBK 120, COLR 127, MIX 127, loud chords every 0.25 s: peak %d, line samples at the rail %u\n", m, (int)pk, ov);
            expect("LOUD: bounded under full scale (the master's limiter holds), the loop does not wrap", pk < 32767 && pk > 1000);
        }
    }

    /* SENDS OFF and STOP ALL's flush */
    {
        uint32_t kind;
        for (kind = 0; kind < 2u; kind++) {
            uint32_t b, running = 0, seen = 0, rail = 0;
            int32_t pk_after = 0;
            dsp_clean(DM_STEREO);
            CLR_EV();
            song.g[G_BPM] = 120; song.g[G_DTIME] = 1; song.g[G_DFDBK] = 80; song.g[G_DMIX] = 127; song.g[G_DCOLOR] = 90; song.g[G_DMDEP] = 100;
            song.g[G_RTYPE] = 0;
            host_preset(&trk[0], eng_of("ANALOG"), 8);
            trk[0].p[P_DLY] = 127; trk[0].p[P_REV] = trk[0].p[P_CHOR] = 0; trk[0].p[P_TRIM] = 100;
            SET_MODE(kind ? 2 : 1);
            rec_n = 0;
            trk_note_on(&trk[0], 52, 100);
            for (b = 0; b < 2u * FS / CTL; b++) {
                if (b == FS / 8u / CTL) trk_note_off(&trk[0], 52);
                mix_block(ob, CTL);
            }
            running = dlyev.on == 1 && dlyev.x == 32768 && dlyev.rng != 0x2545F491u;
            if (kind) { snd_panic = 1; } else { mods_off |= MODS_SENDS; }
            for (b = 0; b < FS / CTL; b++) {
                uint32_t nz = 0, i;
                mix_block(ob, CTL);                                /* the fade (22.5 ms), then the lines cleared (6.5 ms) */
                if (snd_st == SND_OFF && snd_clr > DLY_LEN / DM_CHUNK && !seen) {
                    for (i = 0; i < 65536u; i++)
                        nz += dly_buf[i] != 0;
                    rail = nz;
                    seen = 1 + (nz == 0 && dlyev.on == 0 && dlyev.x == 0 && dlyev.rng == 0 && !dlyev.v[0].act && !dlyev.v[1].act && !dlyev.v[0].fin && !dlyev.v[1].fout);
                }
            }
            printf("dlyev: %s with the %s running: at the end of the flush the line holds %u non-zero samples, state zero: %s\n", kind ? "STOP ALL's flush" : "SENDS OFF",
                   kind ? "OCT+" : "RAND", rail, seen == 2 ? "yes" : "no");
            expect(kind ? "PANIC: STOP ALL's flush clears the line, voices, generator, clock and crossfade (dlyev is zero when the sends are off)" : "SENDS OFF: the same",
                   running && seen == 2);
            mods_off &= ~MODS_SENDS;
            for (b = 0; b < 2u * FS / CTL; b++) {
                mix_block(ob, CTL);                                 /* ON again: the sends fade in over silent lines */
                if (b > FS / CTL)
                    pk_after = ob[0] > pk_after ? ob[0] : -ob[0] > pk_after ? -ob[0] : pk_after;
            }
            expect(kind ? "PANIC: after the flush and with no new note the output stays silent" : "SENDS: ON again over the cleared lines, silent until a new note", pk_after == 0);
        }
    }

    /* the cards */
    {
        const param_desc_t *d;
        int16_t *vp;
        uint32_t p1 = 0, p2 = 0, i, m, k, ok;
        static const char *const FULLN[4] = {"DIGITAL", "PING-PONG", "RANDOM", "OCT UP"};
        static const char *const CARD[4] = {"DIGI", "PING", "RAND", "OCT+"};
        for (i = 0; i < NPAGES; i++) {
            if (str_eq(PAGES[i].title, "DELAY 1/2")) p1 = i;
            if (str_eq(PAGES[i].title, "DELAY 2/2")) p2 = i;
            expect("UI: there is no page DELAY MODE (the mode is the first card of DELAY 1/2)", !str_eq(PAGES[i].title, "DELAY MODE"));
        }
        /* the order, top-down: MODE first, MIX last; the delay has two pages */
        expect("UI: DELAY 1/2 is MODE, TIME, FDBK, COLR; DELAY 2/2 is SPREAD, MRATE, MDEPTH, MIX (MODE first, MIX last)",
               PAGES[p1].id[0] == G_DMODE && PAGES[p1].id[1] == G_DTIME && PAGES[p1].id[2] == G_DFDBK && PAGES[p1].id[3] == G_DCOLOR &&
               PAGES[p2].id[0] == G_PINGSPREAD && PAGES[p2].id[1] == G_DMRATE && PAGES[p2].id[2] == G_DMDEP && PAGES[p2].id[3] == G_DMIX &&
               p2 == p1 + 1u && PAGES[p1].scope == SC_GLOBAL && PAGES[p1].fam == FAM_FX && PAGES[p2].scope == SC_GLOBAL && PAGES[p2].fam == FAM_FX);
        d = page_desc(&PAGES[p1], 0, &vp);
        ok = d && !strcmp(d->label, "MODE") && d->max == 3 && d->def == DMD_DIGITAL && vp == &song.g[G_DMODE];
        for (m = 0; m < 4u && d; m++)
            ok &= !strcmp(d->names[m], CARD[m]) && strlen(d->names[m]) <= 5u;
        expect("UI: MODE is K1: DIGI, PING, RAND, OCT+ (cards of 5 characters at most), the power-on default is DIGITAL (the old PING off)", ok);
        for (m = 0; m < 4u; m++) {
            song.g[G_DMODE] = (int16_t)m;
            ok &= !strcmp(dly_mode_full(), FULLN[m]);
        }
        expect("UI: the modes in full (header): DIGITAL, PING-PONG, RANDOM, OCT UP", ok);
        expect("UI: MODE is a SENDS card (MODULES SENDS OFF fades it), carried by a multi; the engine switch (G_ENGSEL) is not", mods_of(1, G_DMODE) == MODS_SENDS && MU_GLOBAL(G_DMODE) &&
               !MU_GLOBAL(G_ENGSEL) && mods_of(1, G_PINGSPREAD) == MODS_SENDS);
        /* every mode: which cards are faded, with which reason (STEREO, LAYER, and the stored mode in DUEL) */
        {
            static const uint32_t CARDS[8] = {G_DMODE, G_DTIME, G_DFDBK, G_DCOLOR, G_PINGSPREAD, G_DMRATE, G_DMDEP, G_DMIX};
            static const char *const WHY[4][8] = {          /* [mode][card]: 0 = live */
                {0, 0, 0, 0, "DIGI", 0, 0, 0},                /* DIGITAL: the plain delay has no side: SPREAD fades */
                {0, 0, 0, 0, 0, 0, 0, 0},                     /* PING-PONG: everything lives (the modulation too) */
                {0, 0, 0, 0, 0, "RAND", 0, 0},                /* RANDOM: no modulation: RATE fades; DEPTH is CHANCE */
                {0, 0, 0, 0, 0, "OCT+", 0, 0}};               /* OCT UP: the same */
            for (k = 0; k < 3u; k++) {
                duell_mode_req = k == 0 ? DM_STEREO : k == 1 ? DM_LAYER : DM_SPLIT;
                for (m = 0; m < 4u; m++) {
                    uint32_t c;
                    song.g[G_DMODE] = (int16_t)m;
                    ok = 1;
                    for (c = 0; c < 8u; c++) {
                        const char *w = dly_mode_why(CARDS[c]), *e = k == 2u ? (CARDS[c] == G_DMODE || CARDS[c] == G_PINGSPREAD ? "DUEL" : 0) : WHY[m][c];
                        ok &= (!w && !e) || (w && e && !strcmp(w, e));
                    }
                    d = page_desc(&PAGES[p2], 2, &vp);
                    ok &= d && !strcmp(d->label, k != 2u && m >= DMD_RAND ? "CHANCE" : "MDEPTH") && (k != 2u && m >= DMD_RAND ? d->max == 100 : 1);
                    expect(k == 0 ? "UI STEREO: each mode's faded cards and reasons (DIGI: SPREAD; RAND / OCT+: RATE), DEPTH is CHANCE in RAND / OCT+" :
                           k == 1 ? "UI LAYER: the same" : "UI DUEL: whatever the stored mode, MODE and SPREAD fade with DUEL, RATE and DEPTH live (the plain delay with its modulation)", ok);
                }
            }
            /* the cards on screen (the hide rule, ui_draw.c draw_column_hide): DELAY 2/2 is SPREAD, MRATE, DEPTH / CHANCE, MIX in K1..K4; a hidden card leaves its column empty */
            {
                static const uint8_t HID[3][4][4] = {   /* [output mode][delay mode][K1..K4] 1 = not drawn */
                    {{1, 0, 0, 0}, {0, 0, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}},   /* STEREO: DIGI hides SPREAD; PING hides nothing; RAND and OCT+ hide MRATE */
                    {{1, 0, 0, 0}, {0, 0, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}},   /* LAYER: the same */
                    {{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}}};  /* DUEL: SPREAD hidden whatever the stored mode (MRATE and DEPTH stay: the plain delay with its modulation) */
                for (k = 0; k < 3u; k++) {
                    duell_mode_req = k == 0 ? DM_STEREO : k == 1 ? DM_LAYER : DM_SPLIT;
                    for (m = 0; m < 4u; m++) {
                        uint32_t c;
                        song.g[G_DMODE] = (int16_t)m;
                        ui.page = (uint8_t)p2; page_entered();
                        ui.force = 1; ui_draw();
                        ok = 1;
                        for (c = 0; c < 4u; c++)
                            ok &= (ui.col[c][0] == 0x7f) == HID[k][m][c];
                        expect("DELAY 2/2 on screen: the hidden cards are not drawn, the others keep their columns", ok);
                    }
                }
                ui.page = (uint8_t)p1; page_entered();
                song.g[G_DMODE] = DMD_DIGITAL; duell_mode_req = DM_SPLIT;
                ui.force = 1; ui_draw();
                expect("DELAY 1/2 in DUEL: the MODE card (K1) is not drawn, TIME FDBK COLR are", ui.col[0][0] == 0x7f && ui.col[1][0] != 0x7f && ui.col[3][0] != 0x7f);
            }
            duell_mode_req = DM_STEREO;
            song.g[G_DMODE] = 9;
            expect("UI: a stored mode past OCT UP plays as DIGITAL (SPREAD is hidden, MDEPTH is MDEPTH)",
                   dly_mode_why(G_PINGSPREAD) && !strcmp(dly_mode_why(G_PINGSPREAD), "DIGI") && !dly_mode_why(G_DMRATE) &&
                       !strcmp(page_desc(&PAGES[p2], 2, &vp)->label, "MDEPTH"));
        }
        /* the CHANCE default: MODE turned into RANDOM / OCT UP with the stored DEPTH 0 sets CHANCE 40 % (said); back to a plain mode it goes back to 0; a DEPTH set by hand stays */
        ui.page = (uint8_t)p1; page_entered();
        song.g[G_DMDEP] = 0; song.g[G_DMODE] = DMD_DIGITAL; dmd_auto = 0; ui.msg[0] = 0;
        edit_param(0, 1);
        expect("MODE DIGITAL -> PING-PONG: no CHANCE, the header says the mode in full", song.g[G_DMODE] == DMD_PING && song.g[G_DMDEP] == 0 && !strcmp(ui.msg, "PING-PONG"));
        edit_param(0, 1);
        expect("DEFAULT: PING-PONG -> RANDOM with DEPTH 0: CHANCE starts at 40, said in the header",
               song.g[G_DMODE] == DMD_RAND && song.g[G_DMDEP] == 40 && DMD_CHANCE_DEF == 40 && !strcmp(ui.msg, "RANDOM: CHANCE 40 %"));
        edit_param(0, 1);
        expect("DEFAULT: RANDOM -> OCT UP keeps CHANCE", song.g[G_DMODE] == DMD_OCTUP && song.g[G_DMDEP] == 40 && !strcmp(ui.msg, "OCT UP"));
        edit_param(0, -1); edit_param(0, -1);
        expect("DEFAULT: back to PING-PONG: the modulation DEPTH is 0 again (the delay as it was)", song.g[G_DMODE] == DMD_PING && song.g[G_DMDEP] == 0);
        edit_param(0, 1);
        song.g[G_DMDEP] = 40; dmd_auto = 0;   /* (the player's own 40) */
        edit_param(0, -1);
        expect("DEFAULT: a CHANCE the player set (or turned) stays when MODE goes back to a plain mode", song.g[G_DMODE] == DMD_PING && song.g[G_DMDEP] == 40);
        song.g[G_DMDEP] = 25; song.g[G_DMODE] = DMD_DIGITAL; ui.msg[0] = 0;
        edit_param(0, 2);
        expect("DEFAULT: a DEPTH that is not 0 is left alone (it becomes the CHANCE), only the mode is said", song.g[G_DMODE] == DMD_RAND && song.g[G_DMDEP] == 25 && !strcmp(ui.msg, "RANDOM"));
        song.g[G_DMDEP] = 0; song.g[G_DMODE] = DMD_DIGITAL;
        duell_mode_req = DM_SPLIT;
        edit_param(0, 1);
        expect("DEFAULT: in DUEL the MODE card is faded: no change, no CHANCE", song.g[G_DMODE] == DMD_DIGITAL && song.g[G_DMDEP] == 0);
        duell_mode_req = DM_STEREO;
        song.g[G_DMODE] = DMD_DIGITAL; song.g[G_DMDEP] = 0; dmd_auto = 0;
    }

    /* cost */
    {
        double c0 = cost(DM_STEREO, 0, 0, 0), cp = cost(DM_STEREO, 0, 1, 0), r0 = cost(DM_STEREO, 1, 0, 0), r50 = cost(DM_STEREO, 1, 0, 50), r100 = cost(DM_STEREO, 1, 0, 100);
        double o50 = cost(DM_STEREO, 2, 0, 50), o100 = cost(DM_STEREO, 2, 0, 100), m60 = cost(DM_STEREO, 0, 0, 60);
        printf("dlyev: COST instructions / sample (whole mix, one plucked part, every 0.5 s): DELAY %.1f, DELAY + DEPTH 60 %.1f, PING-PONG %.1f; RAND CHANCE 0 %.1f, 50 %.1f, 100 %.1f; "
               "OCT+ 50 %.1f, 100 %.1f\n", c0, m60, cp, r0, r50, r100, o50, o100);
        printf("dlyev: COST against DELAY: RAND +%.1f / +%.1f / +%.1f, OCT+ +%.1f / +%.1f; against PING-PONG: RAND 50 %+.1f\n", r0 - c0, r50 - c0, r100 - c0, o50 - c0, o100 - c0, r50 - cp);
        expect("COST: between events (CHANCE 0) an event mode adds under 60 instructions per sample over the plain delay, the worst case (CHANCE 100) under 200",
               c0 == 0.0 || (r0 - c0 < 60.0 && r100 - c0 < 200.0 && o100 - c0 < 200.0));
    }
#endif
    printf("dlyev: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
