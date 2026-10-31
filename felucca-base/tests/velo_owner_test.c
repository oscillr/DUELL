/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* KEY VELOCITY on the OWNER'S OWN keys (HANDOFF_V2 item 123). The first recording from his unit (tests/data/velo_owner_20261026.csv: 105 presses on 10 keys, guided:
 * hard / normal / soft x 5 per key; docs/VELO_CSV.md) is replayed through the firmware's own sense core (firmware/src/sense.c) on a FRESH unit (nothing stored: the typical
 * key of sense_cal.h) and the velocities are held to what he feels:
 *   cc -O2 -w -Ifirmware/src -Itests -o velo_owner tests/velo_owner_test.c -lm ; ./velo_owner [RECORDING.csv]     (run from felucca-base; host_tests.sh velo)
 *   1  the press counts of the recording (33 hard, 30 normal, 31 soft, each measured from a driven reading, none from the landing-depth fallback)
 *   2  per key with at least 5 presses of each kind (F3 A3 C4 E4 G4 C5): median hard > normal > soft, hard median >= 100, soft median <= 40
 *   3  all keys: hard median >= 110, no more than 5 % of the hard presses below 60, normal median 50 .. 100, soft median <= 40
 *   4  monotonic over the whole range: the mapping at every reading 250 .. 1022 (the typical key and every key as the replay left it) and the whole press through
 *      sense_update from the softest reading 1022 (900 and over is "open" by value, a very soft pill) down to far past the range: never quieter when harder
 *   6  AFTERTOUCH (item 126): replayed with AFTERTOUCH on, no more than 5 % of the presses of any style show a value above 20 within 400 ms of the landing (the finger
 *      pressing in is not a squeeze); a held key with no extra pressure reads 0 after settling; a synthetic squeeze after the settle still runs 0 .. 127 monotonically (every type)
 *   5  one wild first reading does not define a key's range (the old min / max start did); the calibration is stored once the key was played and 60 s have passed (not 300 s) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/sense.c"

static int saves;
int velo_cal_load(uint8_t *buf, uint32_t n) { (void)buf; (void)n; return -1; }
int velo_cal_save(const uint8_t *buf, uint32_t n) { (void)buf; (void)n; saves++; return 0; }

static int fails;
static void chk(int ok, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void chk(int ok, const char *fmt, ...)
{
    char w[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(w, sizeof w, fmt, ap);
    va_end(ap);
    printf("velo_owner: %-4s %s\n", ok ? "ok" : "FAIL", w);
    fails += !ok;
}

enum { HARD, NORMAL, SOFT, NSTYLE };
static const char *const SN[NSTYLE] = {"hard", "normal", "soft"};
#define MAXV 64
static struct { uint8_t v[MAXV]; uint32_t n, drv; } cell[SC_NKEY][NSTYLE];
static const char *const KN[SC_NKEY] = {"F3", "F#3", "G3", "G#3", "A3", "A#3", "B3", "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4", "C5"};

static int cmpu8(const void *a, const void *b) { return (int)*(const uint8_t *)a - (int)*(const uint8_t *)b; }
static uint32_t pct(uint8_t *v, uint32_t n, uint32_t q)     /* sorted[n * q / 4]: q = 1 quarter, 2 middle, 3 three-quarter */
{
    qsort(v, n, 1, cmpu8);
    return v[n * q / 4u];
}
static void dist(const char *name, const uint8_t *src, uint32_t n)
{
    uint8_t v[512];
    memcpy(v, src, n);
    qsort(v, n, 1, cmpu8);
    printf("velo_owner:   %-7s %3u presses  lowest %3u  quarter %3u  middle %3u  three-quarter %3u  highest %3u\n", name, n, v[0], v[n / 4], v[n / 2], v[3 * n / 4], v[n - 1]);
}

/* the replay: rows ts_us,key,adc,drive; '# segment,NAME,keynumber,style,ts' lines say what the guide asked for from there on */
static int replay(const char *path, uint8_t *all_v[NSTYLE], uint32_t all_n[NSTYLE])
{
    FILE *f = fopen(path, "r");
    char line[256];
    int seg_key = -1, seg_style = -1;
    uint32_t i;
    sense_ev_t e;
    if (!f)
        return 0;
    while (fgets(line, sizeof line, f)) {
        unsigned long long ts;
        unsigned key, adc, drv;
        if (!strncmp(line, "# segment,", 10)) {
            char nm[16], st[16];
            unsigned kk;
            if (sscanf(line, "# segment,%15[^,],%u,%15[^,]", nm, &kk, st) == 3) {
                seg_key = (int)kk;
                seg_style = !strcmp(st, "hard") ? HARD : !strcmp(st, "normal") ? NORMAL : !strcmp(st, "soft") ? SOFT : -1;
            }
            continue;
        }
        if (line[0] == '#' || line[0] == '\n' || line[0] == 't')
            continue;
        if (sscanf(line, "%llu,%u,%u,%u", &ts, &key, &adc, &drv) != 4 || key >= SC_NKEY)
            continue;
        sense_update(key, adc, drv != 0u, (uint32_t)ts);
        while (sense_event_pop(&e))
            if (e.type == SE_LAND && (int)e.key == seg_key && seg_style >= 0) {      /* (a press of another key during a segment is a neighbour touched by accident: not counted) */
                uint32_t c = cell[e.key][seg_style].n;
                if (c < MAXV) {
                    cell[e.key][seg_style].v[c] = e.a;
                    cell[e.key][seg_style].n++;
                    cell[e.key][seg_style].drv += e.b;
                }
            }
    }
    fclose(f);
    for (i = 0; i < NSTYLE; i++) {
        uint32_t k;
        all_n[i] = 0;
        for (k = 0; k < SC_NKEY; k++) {
            memcpy(all_v[i] + all_n[i], cell[k][i].v, cell[k][i].n);
            all_n[i] += cell[k][i].n;
        }
    }
    return 1;
}

static uint32_t T = 1000000u;
static uint32_t press(uint32_t note, uint32_t landing, uint32_t dhit, uint32_t *measured)   /* one whole press the way the scan feeds it; its velocity (0 = none) */
{
    sk_t *k = &sn.k[note];
    uint8_t seq = k->rseq;
    sense_ev_t e;
    uint32_t v = 0;
    sense_update(note, 1023, 0, T += 2200);
    sense_update(note, landing, 0, T += 2200);
    sense_update(note, dhit, 1, T += 2200);
    sense_update(note, 190, 0, T += 2200);
    sense_update(note, 900, 1, T += 2200);
    sense_update(note, 190, 0, T += 2200);
    sense_update(note, 1023, 0, T += 2200);
    sense_update(note, 1023, 0, T += 2200);
    sense_update(note, 1023, 0, T += 20000);
    *measured = 0;
    while (sense_event_pop(&e))
        if (e.type == SE_LAND) {
            v = e.a;
            *measured = e.b;
        }
    return k->rseq != seq ? v : 0u;
}


/* ---- 6 aftertouch ---- */
static struct { uint32_t n, bad, worst; } atc[NSTYLE];
static void at_replay(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    int seg_key = -1, seg_style = -1, cur_key = -1, cur_style = -1;
    uint32_t land_t = 0, mx = 0;
    sense_ev_t e;
    if (!f)
        return;
    memset(atc, 0, sizeof atc);
    while (fgets(line, sizeof line, f)) {
        unsigned long long ts;
        unsigned key, adc, drv;
        if (!strncmp(line, "# segment,", 10)) {
            char nm[16], st[16];
            unsigned kk;
            if (sscanf(line, "# segment,%15[^,],%u,%15[^,]", nm, &kk, st) == 3) {
                seg_key = (int)kk;
                seg_style = !strcmp(st, "hard") ? HARD : !strcmp(st, "normal") ? NORMAL : !strcmp(st, "soft") ? SOFT : -1;
            }
            continue;
        }
        if (line[0] == '#' || line[0] == '\n' || line[0] == 't')
            continue;
        if (sscanf(line, "%llu,%u,%u,%u", &ts, &key, &adc, &drv) != 4 || key >= SC_NKEY)
            continue;
        sense_update(key, adc, drv != 0u, (uint32_t)ts);
        while (sense_event_pop(&e))
            if (e.type == SE_LAND && (int)e.key == seg_key && seg_style >= 0) {
                if (cur_key >= 0) {                  /* the press before is closed: counted */
                    atc[cur_style].n++;
                    atc[cur_style].bad += mx > 20u;
                    atc[cur_style].worst = mx > atc[cur_style].worst ? mx : atc[cur_style].worst;
                }
                cur_key = seg_key;
                cur_style = seg_style;
                land_t = (uint32_t)e.t;
                mx = 0;
            }
        if ((int)key == cur_key && (uint32_t)((uint32_t)ts - land_t) <= 400000u && sn.k[key].at > mx)
            mx = sn.k[key].at;
    }
    if (cur_key >= 0) {
        atc[cur_style].n++;
        atc[cur_style].bad += mx > 20u;
        atc[cur_style].worst = mx > atc[cur_style].worst ? mx : atc[cur_style].worst;
    }
    fclose(f);
}
static void feed(uint32_t note, uint32_t r)         /* one undriven and one driven reading, the way the scan alternates */
{
    sense_update(note, 190, 0, T += 2200);
    sense_update(note, r, 1, T += 2200);
}
static uint32_t at_of(uint32_t note) { return sn.k[note].at; }

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tests/data/velo_owner_20261026.csv";
    static uint8_t buf[NSTYLE][512];
    uint8_t *all_v[NSTYLE] = {buf[HARD], buf[NORMAL], buf[SOFT]};
    uint32_t all_n[NSTYLE], k, s, d, bad;
    uint32_t med[SC_NKEY][NSTYLE];
    int keys_ok = 0;

    sense_enable(1);
    sense_curve_set(100);
    sense_reset();
    if (!replay(path, all_v, all_n)) {
        printf("velo_owner: FAIL cannot open %s (run from felucca-base)\n", path);
        return 2;
    }
    printf("velo_owner: the recording replayed on a fresh unit (typical key %u .. %u), velocity 1 .. 127\n", SC_FLOOR_PRIOR, SC_CEIL_PRIOR);
    for (s = 0; s < NSTYLE; s++)
        dist(SN[s], all_v[s], all_n[s]);
    printf("velo_owner: per key, middle velocity hard / normal / soft (presses)\n");
    for (k = 0; k < SC_NKEY; k++) {
        if (!cell[k][HARD].n && !cell[k][NORMAL].n && !cell[k][SOFT].n)
            continue;
        for (s = 0; s < NSTYLE; s++)
            med[k][s] = cell[k][s].n ? pct(cell[k][s].v, cell[k][s].n, 2) : 0u;
        printf("velo_owner:   %-4s %3u / %3u / %3u   (%u / %u / %u)\n", KN[k], med[k][HARD], med[k][NORMAL], med[k][SOFT], cell[k][HARD].n, cell[k][NORMAL].n, cell[k][SOFT].n);
    }

    /* 1 the recording */
    {
        uint32_t drv = 0;
        for (k = 0; k < SC_NKEY; k++)
            for (s = 0; s < NSTYLE; s++)
                drv += cell[k][s].drv;
        chk(all_n[HARD] == 33u && all_n[NORMAL] == 30u && all_n[SOFT] == 31u, "the recording holds 33 hard, 30 normal and 31 soft presses on their own keys (%u / %u / %u)", all_n[HARD], all_n[NORMAL], all_n[SOFT]);
        chk(drv == all_n[HARD] + all_n[NORMAL] + all_n[SOFT], "every one of them is measured from a driven reading, none falls back to the landing depth (%u of %u)", drv, all_n[HARD] + all_n[NORMAL] + all_n[SOFT]);
    }
    /* 2 per key */
    for (k = 0; k < SC_NKEY; k++) {
        char w[200];
        if (cell[k][HARD].n < 5u || cell[k][NORMAL].n < 5u || cell[k][SOFT].n < 5u)
            continue;
        keys_ok++;
        snprintf(w, sizeof w, "%s: median hard %u > normal %u > soft %u, hard >= 100, soft <= 40", KN[k], med[k][HARD], med[k][NORMAL], med[k][SOFT]);
        chk(med[k][HARD] > med[k][NORMAL] && med[k][NORMAL] > med[k][SOFT] && med[k][HARD] >= 100u && med[k][SOFT] <= 40u, "%s", w);
    }
    chk(keys_ok == 6, "six keys have at least 5 presses of each kind (%d)", keys_ok);
    /* 3 all keys */
    {
        uint32_t mh = pct(all_v[HARD], all_n[HARD], 2), mn = pct(all_v[NORMAL], all_n[NORMAL], 2), ms = pct(all_v[SOFT], all_n[SOFT], 2), low = 0, i;
        for (i = 0; i < all_n[HARD]; i++)
            low += all_v[HARD][i] < 60u;
        chk(mh >= 110u, "all keys: the hard presses' middle velocity is %u (>= 110)", mh);
        chk(low * 20u <= all_n[HARD], "all keys: %u of %u hard presses below 60 (at most 5 %%)", low, all_n[HARD]);
        chk(mn >= 50u && mn <= 100u, "all keys: the normal presses' middle velocity is %u (50 .. 100)", mn);
        chk(ms <= 40u, "all keys: the soft presses' middle velocity is %u (<= 40)", ms);
        chk(mh > mn && mn > ms, "all keys: hard %u > normal %u > soft %u", mh, mn, ms);
    }
    /* 4 monotonic: the mapping on every key as the replay left it, and on the typical key; every curve type */
    {
        uint32_t ty, n;
        uint32_t mono = 1, tops = 1;
        for (ty = 0; ty < SC_TYPE_N; ty++) {
            sense_vel_type_set(ty);
            for (n = 0; n <= SC_NKEY; n++) {
                sk_t typical, *kk = n < SC_NKEY ? &sn.k[n] : &typical;
                uint32_t prev = 127u;
                if (n == SC_NKEY)
                    memset(&typical, 0, sizeof typical);
                for (d = SC_D_MIN; d <= SC_D_MAX; d++) {
                    uint32_t v = sk_map(kk, d);
                    if (v > prev)
                        mono = 0;
                    prev = v;
                }
                tops &= sk_map(kk, SC_D_MIN) == 127u;
            }
        }
        sense_vel_type_set(SC_TYPE_NORMAL);
        chk(mono && tops, "the mapping never rises as the reading rises (250 .. 1022), all %u keys as the replay left them and the typical key, every curve type, 127 at the hardest", SC_NKEY);
    }
    {
        uint32_t ty, cal, prev, mono, last, first, meas_all, m;
        for (cal = 0; cal < 2u; cal++)
            for (ty = 0; ty < SC_TYPE_N; ty++) {
                sense_vel_type_set(ty);
                prev = 0;
                mono = 1;
                last = 0;
                first = 0;
                meas_all = 1;
                for (d = 1022; d >= 150u; d -= (d > 600u ? 3u : 25u)) {      /* from the softest pill reading to far past the range */
                    uint32_t v, landing = 180u + (d > 1022u - 100u ? 0u : 0u);
                    sk_t *kk = &sn.k[3];
                    if (cal)
                        kk->cnt = 40u, kk->lo6 = 640u * 64u, kk->hi6 = 980u * 64u;
                    else
                        kk->cnt = 0u, kk->lo6 = kk->hi6 = 0u;
                    v = press(3, landing, d, &m);
                    meas_all &= m == 1u;
                    if (v < prev)
                        mono = 0;
                    if (!first)
                        first = v;
                    prev = v;
                    last = v;
                }
                chk(mono && last == 127u && meas_all, "the whole press from reading 1022 down to 150 (%s key, curve %u): never quieter when harder, ends at 127, every one measured", cal ? "played-in" : "typical", ty);
            }
        sense_vel_type_set(SC_TYPE_NORMAL);
    }
    /* 5 a wild first reading, the store */
    {
        uint32_t f0, c0, f1, c1, v, m, i;
        sense_reset();
        saves = 0;
        v = press(5, 180, 300, &m);                          /* the very first press of the key: a reading far harder than the pill ever gives */
        sense_cal_get(5, &f0, &c0);
        chk(v == 127u && f0 >= SC_FLOOR_PRIOR - 7u && f0 <= SC_FLOOR_PRIOR && c0 == SC_CEIL_PRIOR, "a wild first reading (300) moves the floor by one step only: floor %u ceiling %u (typical %u .. %u)", f0, c0, SC_FLOOR_PRIOR, SC_CEIL_PRIOR);
        v = press(5, 180, 1022, &m);
        sense_cal_get(5, &f1, &c1);
        chk(v == 1u && c1 >= SC_CEIL_PRIOR && c1 <= SC_CEIL_PRIOR + 7u && f1 >= f0, "and a wild second one (1022) the ceiling by one step: floor %u ceiling %u", f1, c1);
        v = press(5, 180, 696, &m);
        chk(v >= 120u, "the next hard press (696) still plays %u (>= 120): the old min / max start would have read it on a scale centred on the first two", v);
        for (i = 0; i < 12u; i++)                            /* a short session: the quantiles move by well over SC_CAL_SAVE_MOVED counts */
            press(5, 180, i & 1u ? 1020u : 520u, &m);
        sense_service(1000, 1);                              /* the clock of the store starts here */
        sense_service(1000u + SC_CAL_SAVE_MS - 1000u, 1);
        chk(saves == 0, "the store is not due before %u s (SC_CAL_SAVE_MS)", SC_CAL_SAVE_MS / 1000u);
        sense_service(1000u + SC_CAL_SAVE_MS + 1000u, 0);
        chk(saves == 0, "  .. and not while a key is held");
        sense_service(1000u + SC_CAL_SAVE_MS + 1000u, 1);
        chk(saves == 1, "  .. and it is written once it is due and the keys are idle (a 1 minute play session is kept; it was 5 minutes)");
        sense_service(1000u + SC_CAL_SAVE_MS + 2000u, 1);
        chk(saves == 1, "  .. and not again until the calibration has moved again");
    }
    /* 6 aftertouch */
    {
        uint32_t i, ty, mono, top, prev, v, flat, mxs, noisy;
        uint32_t m;
        sense_reset();
        sense_enable(1);
        sense_at_enable(1);
        at_replay(path);
        for (s = 0; s < NSTYLE; s++) {
            printf("velo_owner:   aftertouch, %-6s %2u presses, %2u with a value above 20 within 400 ms of the landing (highest %u)\n", SN[s], atc[s].n, atc[s].bad, atc[s].worst);
            chk(atc[s].n >= 30u && atc[s].bad * 20u <= atc[s].n, "aftertouch: at most 5 %% of the %s presses show a value above 20 within 400 ms of the landing (%u of %u)", SN[s], atc[s].bad, atc[s].n);
        }
        /* a landing ramp (the finger pressing in, 960 down to 800 in 80 ms) and 2 s held with +-2 counts of noise: 0 the whole time */
        for (ty = 0; ty < SC_TYPE_N; ty++) {
            sense_reset();
            sense_enable(1);
            sense_at_enable(1);
            sense_at_type_set(ty);
            T += 100000;
            sense_update(4, 1023, 0, T += 2200);
            sense_update(4, 190, 0, T += 2200);
            sense_update(4, 960, 1, T += 2200);
            mxs = 0;
            for (i = 0; i < 700u; i++) {
                uint32_t r = i < 18u ? 960u - 160u * i / 18u : 800u + ((i * 7u) % 5u) - 2u;
                feed(4, r);
                mxs = at_of(4) > mxs ? at_of(4) : mxs;
            }
            chk(mxs == 0u, "aftertouch type %u: a landing ramp 960 -> 800 and 2 s held with +-2 counts of noise reads 0 the whole time (highest %u)", ty, mxs);
        }
        /* a squeeze after the settle: 800 held 150 ms, then 3 counts a reading down to 560: 0 .. 127, never falls, ends at 127; and let go slowly it falls back */
        for (ty = 0; ty < SC_TYPE_N; ty++) {
            sense_reset();
            sense_enable(1);
            sense_at_enable(1);
            sense_at_type_set(ty);
            T += 100000;
            sense_update(4, 1023, 0, T += 2200);
            sense_update(4, 190, 0, T += 2200);
            sense_update(4, 960, 1, T += 2200);
            for (i = 0; i < 18u; i++)
                feed(4, 960u - 160u * i / 18u);
            flat = 0;
            for (i = 0; i < 40u; i++) {
                feed(4, 800);
                flat |= at_of(4);
            }
            mono = 1;
            top = 0;
            prev = 0;
            for (i = 0; i < 100u; i++) {
                feed(4, 800u - 3u * i);
                v = at_of(4);
                if (v < prev)
                    mono = 0;
                prev = v;
                top = v > top ? v : top;
            }
            for (i = 0; i < 20u; i++)
                feed(4, 500);
            m = at_of(4);
            noisy = sense_at_value();
            chk(flat == 0u && mono && top >= 120u && m == 127u && noisy == 127u, "aftertouch type %u: 0 while held flat, then a squeeze of 3 counts a reading rises monotonically to %u (127 at the end: %u, sense_at_value %u)", ty, top, m, noisy);
        }
        sense_at_type_set(SC_TYPE_NORMAL);
        /* and a squeeze that is only just past the span: the value at SC_AT_SPAN counts below the settled reading is full scale; half of it is about half */
        sense_reset();
        sense_enable(1);
        sense_at_enable(1);
        T += 100000;
        sense_update(6, 1023, 0, T += 2200);
        sense_update(6, 190, 0, T += 2200);
        sense_update(6, 900, 1, T += 2200);
        for (i = 0; i < 50u; i++)
            feed(6, 800);
        for (i = 0; i < 20u; i++)
            feed(6, 800u - (SC_AT_SPAN + SC_AT_FLOOR) / 2u);
        v = at_of(6);
        chk(v >= 56u && v <= 72u, "aftertouch: a squeeze half of SC_AT_SPAN deep reads about half scale (%u)", v);
        for (i = 0; i < 20u; i++)
            feed(6, 800u - SC_AT_SPAN - SC_AT_FLOOR);
        chk(at_of(6) == 127u, "aftertouch: SC_AT_SPAN counts of squeeze (with the %u counts of noise floor) is full scale (%u)", SC_AT_FLOOR, at_of(6));
    }
    printf("velo_owner: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
