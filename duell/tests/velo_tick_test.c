/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* Tests of KEY SENSE, after Velo's method (Charles Vestal, github.com/charlesvestal/fm1-velocity, GPL-3.0-only; see firmware/src/sense.c). */
/* The converter stage of the scan tick (hal/fm1_input.h, DUELL_VELO) against a simulated key population: the real fm1_input_tick runs,
 * on the host's register file (as tests/input_test.c does for the LED test), with
 *   - the 74HC595 chain modelled from the shift/latch trace (which column is latched when the converter starts),
 *   - the digital rows read from the simulated contacts (tests/velo_pill.h: bounce included),
 *   - the converter (fm1_adc_start / done / take / abort) reading the pill of the key at the latched column and the row: pull-up
 *     reading or driven reading (the row pin's DIR bit), a conversion time, the pot and the battery on their channels,
 *   - optionally the pull-up NOT recovered when the row is read after it was driven (worst case: the key reads open).
 * Built twice (host_tests.sh velo): -DDUELL_VELO=1 with the sense core, and -DDUELL_VELO=0 (the tick as it was); the edges the digital
 * scan makes of the same hand are hashed in both and must be equal when the setting is OFF.
 * The real converter's timing, the LED lines against the conversion, and the pull-up's recovery CANNOT be tested here: the numbers below
 * (60 us conversion, a pull-up that recovers at once or never) are the two ends of what the unit may do.
 *   cc -O2 -Itests -DDUELL_VELO=1 tests/velo_tick_test.c -lm [--cost MODE N]  (MODE off | idle | held for valgrind --toggle-collect=tick_entry) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "velo_pill.h"
#ifndef DUELL_VELO
#define DUELL_VELO 1
#endif
#if DUELL_VELO
#include "../firmware/src/sense.c"
int velo_cal_load(uint8_t *buf, uint32_t n) { (void)buf; (void)n; return -1; }   /* (the store of the patch store job: none here) */
int velo_cal_save(const uint8_t *buf, uint32_t n) { (void)buf; (void)n; return 0; }
#endif

static uint32_t host_now;                          /* TIMER4 ticks (24 MHz) */
#define FM1_INPUT_NOW() host_now
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_time.h"
#include "../firmware/hal/fm1_gpio.h"
static volatile uint32_t host_reg[16][64];
static uint32_t host_bus = 6u;                     /* TIMER4 ticks per register access (0.25 us) */
static uint32_t host_acc;                          /* register accesses */
#undef FM1_PR
#ifdef VELO_COST            /* the cost build (valgrind --tool=callgrind): a register access is just a memory access, the converter a constant */
#define FM1_PR(p, r) (host_reg[(p) & 15u][((r) / 4u) & 63u])
#else
#define FM1_PR(p, r) (*(host_acc++, host_now += host_bus, &host_reg[(p) & 15u][((r) / 4u) & 63u]))
#endif
static uint32_t host_ticks(void) { return host_now += 2u; }
#define fm1_ticks host_ticks
static uint32_t sr_shift, sr_word = 0xFFFFu;
#define FM1_SR_BIT_TRACE(b) (sr_shift = (sr_shift << 1) | (uint32_t)(b))
#define FM1_SR_LATCH_TRACE() (sr_word = sr_shift & 0xFFFFu)
#define FM1_ADC_NB_EXT 1                           /* this file is the converter */
#include "../firmware/hal/fm1_adc.h"

/* ---- the simulated converter ---- */
static struct {
    int busy;
    uint32_t ready_at, val, col, drv_row;          /* drv_row: the key-matrix row (3 / 4) driven for it, 0 none */
} adcm;
static hand_t H;
static uint32_t conv_us = 60u, slow_us = 150u;     /* conversion times (us) */
static int pot_val = 612, batt_val = 701, adc_never_done;
static uint32_t n_start_key, n_start_pot, n_start_batt, n_start_driven, forced_notes, recov;
static uint32_t sim_us;
static uint32_t latched_col(void)
{
    uint32_t c;
    for (c = 0; c < 11u; c++)
        if (!((sr_word >> c) & 1u))
            return c;
    return 99u;
}
static double drift_c;
#if DUELL_VELO
static void fm1_adc_start(uint32_t ch, uint32_t con);   /* (defined below the HAL header: they read its key map) */
static uint32_t fm1_adc_done(void);
static uint32_t fm1_adc_take(void);
static void fm1_adc_abort(void);
#endif
#include "../firmware/hal/fm1_input.h"
#if DUELL_VELO
static void fm1_adc_start(uint32_t ch, uint32_t con)
{
    uint32_t col = latched_col(), t = host_now / 24u;
    adcm.busy = 1;
    adcm.drv_row = 0;
    adcm.val = 1023u;
#ifdef VELO_COST
    adcm.ready_at = 0;
    adcm.val = (host_reg[FM1_PA][FM1_DIR / 4u] & (3u << 7)) == (3u << 7) ? 1023u : 480u;
    (void)col; (void)t; (void)con; (void)ch;
    return;
#endif
    if (ch == FM1_ADC_MASTER) {
        adcm.val = (uint32_t)pot_val;
        adcm.ready_at = host_now + slow_us * 24u;
        n_start_pot++;
    } else if (ch == FM1_ADC_BATT) {
        adcm.val = (uint32_t)batt_val;
        adcm.ready_at = host_now + slow_us * 24u;
        n_start_batt++;
    } else {
        uint32_t r = ch == SC_CH_PA7 ? 3u : 4u, driven = !((host_reg[FM1_PA][FM1_DIR / 4u] >> (r + 4u)) & 1u);
        int id = col < 11u ? FM1_KEYMAP[r][col] : -1;
        (void)con;
        adcm.ready_at = host_now + conv_us * 24u;
        n_start_key++;
        if (driven) {
            n_start_driven++;
            adcm.drv_row = r;
        }
        if (id >= 14 && id < 14 + (int)NK)
            adcm.val = pill_read(&keys[id - 14], press_at(&H, (uint32_t)(id - 14), t), t + 2u, (int)driven, drift_c);
    }
    adcm.col = col;
}
static uint32_t fm1_adc_done(void) { return adcm.busy && !adc_never_done && host_now >= adcm.ready_at; }
static uint32_t fm1_adc_take(void)
{
    adcm.busy = 0;
    return adcm.val;
}
static void fm1_adc_abort(void) { adcm.busy = 0; }
#endif

static __attribute__((noinline)) void tick_entry(void) { fm1_input_tick(); }

static int fails;
static int check(const char *what, int ok)
{
    printf("%-84s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
    return ok;
}

/* ---- the digital view of the keys ---- */
static int digital_closed(uint32_t note, uint32_t t)
{
    if (note < NK)
        return contact_closed(&H, note, t);
    return (forced_notes >> note) & 1u;
}
static void set_inputs(uint32_t p, uint32_t t)
{
    uint32_t a = 0xFFFFFFFFu, r;
    for (r = 1; r < 5u; r++) {
        int id = FM1_KEYMAP[r][p];
        if (id < 14 || !digital_closed((uint32_t)(id - 14), t))
            continue;
#if DUELL_VELO
        if (recov && adcm.drv_row == r && adcm.col == p)      /* its pull-up has not recovered from the drive: reads open */
            continue;
#endif
        a &= ~(1u << (r + 4u));
    }
    host_reg[FM1_PA][FM1_IN / 4u] = a;
    host_reg[FM1_PB][FM1_IN / 4u] = 0xFFFFFFFFu;
}

/* ---- the run ---- */
typedef struct { uint32_t t, note, up; } edge_t;
static edge_t edges[8192];
static uint32_t nedge, prev_notes, rows_out_ticks, out_after_idle;
static uint32_t t_key_upd[NK][2048], n_key_upd[NK];
static void reset(void)
{
    uint32_t i;
    memset((void *)&fm1_in, 0, sizeof fm1_in);
    memset((void *)&fm1_in_stat, 0, sizeof fm1_in_stat);
    memset((void *)host_reg, 0, sizeof host_reg);
    memset(&adcm, 0, sizeof adcm);
    fm1__tick_col = 0;
    sr_word = 0xFFFFu;
    nedge = prev_notes = rows_out_ticks = out_after_idle = 0;
    sim_us = 100000u;
    forced_notes = recov = adc_never_done = 0;
    conv_us = 60u;
    slow_us = 150u;
    drift_c = 0.0;
    host_acc = 0;
    n_start_key = n_start_pot = n_start_batt = n_start_driven = 0;
    memset(n_key_upd, 0, sizeof n_key_upd);
    for (i = 0; i < 16u; i++)
        host_reg[FM1_PA][FM1_IN / 4u] = 0xFFFFFFFFu;
    fm1_input_init();
#if DUELL_VELO
    memset(&fm1_an, 0, sizeof fm1_an);
    fm1_an.pot = fm1_an.batt = -1;
    sense_enable(0);
    sense_reset();
#endif
    memset(&H, 0, sizeof H);
}
static uint32_t last_seen[NK];
static void one_tick(uint32_t late_us)
{
    sim_us += late_us;
    host_now = sim_us * 24u;
    set_inputs(fm1__tick_col, sim_us);
    tick_entry();
    sim_us += 100u;
    if (fm1_in.notes != prev_notes) {
        uint32_t d = fm1_in.notes ^ prev_notes, k;
        for (k = 0; k < 27u; k++)
            if (((d >> k) & 1u) && nedge < 8192u) {
                edges[nedge].t = sim_us;
                edges[nedge].note = k;
                edges[nedge].up = (fm1_in.notes >> k) & 1u;
                nedge++;
            }
        prev_notes = fm1_in.notes;
    }
#if DUELL_VELO
    {   /* which key got a reading in this tick: sense's own last-sample times */
        uint32_t k;
        for (k = 0; k < NK; k++)
            if ((sn.k[k].fl & K_SEEN) && sn.k[k].t_last != last_seen[k]) {
                last_seen[k] = sn.k[k].t_last;
                if (n_key_upd[k] < 2048u)
                    t_key_upd[k][n_key_upd[k]++] = sn.k[k].t_last;
            }
    }
    if ((host_reg[FM1_PA][FM1_DIR / 4u] & ((1u << 7) | (1u << 8))) != ((1u << 7) | (1u << 8)))
        rows_out_ticks++;
#endif
}
static void run_until(uint32_t t_us)
{
    while (sim_us < t_us)
        one_tick(0);
}
static uint32_t hash_edges(void)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < nedge; i++) {
        h = (h ^ edges[i].t) * 16777619u;
        h = (h ^ (edges[i].note * 2u + edges[i].up)) * 16777619u;
    }
    return h;
}
static void make_hand(uint32_t seed, uint32_t n)
{
    pl_rng = seed;
    keys_make();
    hand_make(&H, n, 2, 3, 0.15);
}
#if DUELL_VELO
static void on(void)
{
    sense_enable(1);
    sense_curve_set(100);
}
#endif

int main(int argc, char **argv)
{
    uint32_t hash_off, k;
    if (argc >= 4 && !strcmp(argv[1], "--cost")) {       /* for valgrind --tool=callgrind --toggle-collect=tick_entry */
        uint32_t n = (uint32_t)atoi(argv[3]), i;
        reset();
        make_hand(0x2545F491u, 4);
#if DUELL_VELO
        if (strcmp(argv[2], "off"))
            on();
#endif
        if (!strcmp(argv[2], "held")) {                  /* six analog keys, held the whole run, three on each analog row */
            static const uint32_t held[6] = {1, 4, 6, 10, 14, 17};
            for (i = 0; i < 6u; i++) {
                pill_press_t *p = &H.p[held[i]][0];
                memset(p, 0, sizeof *p);
                p->tc = 100000u;
                p->tr = 100000u + 60000000u;
                p->s = 0.5;
                p->hold = 0.7;
                H.n[held[i]] = 1;
            }
        } else {
            memset(&H, 0, sizeof H);
        }
        for (i = 0; i < n; i++)
            one_tick(0);
        printf("cost run %s: %u ticks, %u register accesses\n", argv[2], n, host_acc);
        return 0;
    }

    printf("-- the digital scan: the same hand through the tick (sense OFF) and through the tick as it was\n");
    reset();
    make_hand(0x2545F491u, 300);
    run_until(H.t_end);
    hash_off = hash_edges();
    printf("HASH edges %08x (%u edges)\n", hash_off, nedge);
    {
        uint32_t presses = 0, i, up = 0, down = 0;
        for (k = 0; k < NK; k++)
            presses += H.n[k];
        for (i = 0; i < nedge; i++) {
            up += edges[i].up;
            down += !edges[i].up;
        }
        check("every press made exactly one note edge up and one down (bouncy contacts)", up == presses && down == presses);
    }
#if DUELL_VELO
    printf("   the stage with the setting OFF: %u key readings started, %u drives, rows left driven in %u ticks\n", n_start_key, n_start_driven, rows_out_ticks);
    check("OFF: no key is read, no row is driven", n_start_key == 0u && n_start_driven == 0u && rows_out_ticks == 0u);
    check("OFF: the pot and the battery are still read in their slots (each reading counted once for the main loop)",
          fm1_an.pot == pot_val && fm1_an.batt == batt_val && fm1_an.pot_n + 1u >= n_start_pot && fm1_an.batt_n + 1u >= n_start_batt && fm1_an.pot_n > 1000u);
    {
        uint32_t frames = fm1_an.fc;
        printf("   %u frames: %u pot readings (one in %u frames), %u battery readings (one in %u)\n", frames, n_start_pot, SC_POT_EVERY, n_start_batt, SC_BATT_EVERY);
        check("  .. the pot every 16 frames, the battery every 256 (the pot's frame first)",
              n_start_pot + 2u >= frames / SC_POT_EVERY && n_start_pot <= frames / SC_POT_EVERY + 1u && n_start_batt + 2u >= frames / SC_BATT_EVERY &&
              n_start_batt <= frames / SC_BATT_EVERY + 1u);
    }
    check("the drive strength of the rows is set at init (SC_DRIVE_LEVEL)",
          (((host_reg[FM1_PA][FM1_HD0 / 4u] >> 7) & 1u) == (SC_DRIVE_LEVEL & 1u)) && (((host_reg[FM1_PA][FM1_HD / 4u] >> 8) & 1u) == ((SC_DRIVE_LEVEL >> 1) & 1u)));

    printf("-- every analog key is read once every two frames\n");
    reset();
    on();
    slow_us = 150u;                                  /* the pot and the battery take 1.5 ticks: no key slot is lost for them */
    run_until(sim_us + 600000u);
    {
        uint32_t worst = 0, best = 0xFFFFFFFFu, bad = 0;
        for (k = 0; k < NK; k++) {
            uint32_t i;
            if (n_key_upd[k] < 20u)
                bad++;
            for (i = 1; i < n_key_upd[k]; i++) {
                uint32_t d = t_key_upd[k][i] - t_key_upd[k][i - 1];
                if (d > worst)
                    worst = d;
                if (d < best)
                    best = d;
            }
        }
        printf("   interval between a key's readings: %u .. %u us\n", best, worst);
        check("  all 20 analog keys are read, 2.2 ms apart (+- one tick)", !bad && best >= 2150u && worst <= 2250u);
    }
    check("  the pot and the battery are read too", fm1_an.pot == pot_val && fm1_an.batt == batt_val);
    check("  nothing is driven while no key is held", n_start_driven == 0u && rows_out_ticks == 0u);
    check("  no reading was dropped (60 us conversions)", fm1_an.timeouts == 0u);

    printf("-- each key, alone: one landing, its own, from a driven reading; the digital note on and off\n");
    {
        uint32_t good = 0, i;
        for (k = 0; k < NK; k++) {
            sense_ev_t e;
            uint32_t lands = 0, wrongkey = 0, driven = 0;
            reset();
            on();
            pl_rng = 0x1111u + k;
            keys_make();
            press_add(&H, k, 150000u, 120000u, 0.55, 0.7);
            H.n[k] = 1;
            H.p[k][0].nb = 0;
            run_until(400000u);
            while (sense_event_pop(&e))
                if (e.type == SE_LAND) {
                    lands++;
                    wrongkey += e.key != k;
                    driven += e.b;
                }
            if (lands == 1u && !wrongkey && driven == 1u && nedge == 2u && edges[0].note == k && edges[0].up && !edges[1].up)
                good++;
            (void)i;
        }
        check("  20 keys: exactly one landing each (its own key, from a driven reading), one note on, one off", good == NK);
    }
    {
        uint32_t i, up = 0;
        reset();
        on();
        forced_notes = 0x7Fu << 20;
        run_until(sim_us + 200000u);
        for (i = 0; i < nedge; i++)
            up += edges[i].up;
        {
            sense_ev_t e;
            uint32_t ev = 0;
            while (sense_event_pop(&e))
                ev++;
            check("  the top 7 keys play digitally, are never driven and make no sense event",
                  up == 7u && ev == 0u && n_start_driven == 0u && fm1_in.notes == (0x7Fu << 20));
        }
    }

    printf("-- a held key is read with its row driven, one reading in two\n");
    reset();
    on();
    pl_rng = 0x4242u;
    keys_make();
    press_add(&H, 10, 150000u, 400000u, 0.6, 0.7);
    H.p[10][0].nb = 0;
    run_until(150000u - 5000u);
    check("  before the press: nothing driven", n_start_driven == 0u);
    run_until(150000u + 300000u);
    {
        uint32_t drives = n_start_driven, reads = 0, i;
        for (i = 0; i < n_key_upd[10]; i++)
            if (t_key_upd[10][i] >= 150000u)
                reads++;
        printf("   key 10 from the contact to 300 ms on: %u readings, %u of them driven\n", reads, drives);
        check("  about half of the held key's readings were driven", drives * 100u >= reads * 42u && drives * 100u <= reads * 58u);
    }
    run_until(150000u + 400000u + 40000u);
    check("  after the release: the row is on its pull-up again, drives stopped", (host_reg[FM1_PA][FM1_DIR / 4u] & (3u << 7)) == (3u << 7));
    {
        uint32_t d0 = n_start_driven;
        run_until(sim_us + 30000u);
        check("  .. and nothing is driven any more", n_start_driven == d0);
    }

    printf("-- the digital scan with the stage on: the same hand, the setting ON, the pull-up recovering at once / not at all\n");
    {
        uint32_t mode, i, presses = 0;
        static edge_t ref[8192];
        uint32_t nref;
        for (k = 0; k < NK; k++)
            presses += H.n[k];
        reset();
        make_hand(0x2545F491u, 300);
        run_until(H.t_end);
        memcpy(ref, edges, sizeof edges);
        nref = nedge;
        for (mode = 0; mode < 2u; mode++) {
            uint32_t up = 0, down = 0, late_max = 0, early = 0, lt;
            reset();
            on();
            recov = mode;
            make_hand(0x2545F491u, 300);
            run_until(H.t_end);
            for (i = 0; i < nedge; i++) {
                up += edges[i].up;
                down += !edges[i].up;
            }
            printf("   %s: %u keys read, %u of them driven, %u dropped; %u note-ons, %u note-offs (hand: %u presses)\n",
                   mode ? "pull-up not recovered (driven row reads open)" : "pull-up recovered at once", fm1_an.keys, fm1_an.drives, fm1_an.timeouts, up, down,
                   nref / 2u);
            check(mode ? "  not recovered: every press still exactly one note on and one off" : "  recovered: every press still exactly one note on and one off",
                  nedge == nref && up == nref / 2u && down == nref / 2u);
            {   /* note-ons: the same list in the same order at the same times; releases: per note, the k-th release against the k-th */
                uint32_t j, ons = 0, offs_checked = 0;
                lt = 0;
                for (i = 0, j = 0; i < nedge; i++) {
                    if (!edges[i].up)
                        continue;
                    while (j < nref && !ref[j].up)
                        j++;
                    if (j >= nref || ref[j].t != edges[i].t || ref[j].note != edges[i].note)
                        early++;
                    j++;
                    ons++;
                }
                for (i = 0; i < nedge; i++) {
                    uint32_t m, seen = 0;
                    if (edges[i].up)
                        continue;
                    for (m = 0; m < i; m++)
                        seen += !edges[m].up && edges[m].note == edges[i].note;
                    for (m = 0, j = 0; m < nref; m++)
                        if (!ref[m].up && ref[m].note == edges[i].note) {
                            if (j++ == seen) {
                                int32_t d = (int32_t)edges[i].t - (int32_t)ref[m].t;
                                if (d < 0)
                                    early++;
                                else if ((uint32_t)d > late_max)
                                    late_max = (uint32_t)d;
                                break;
                            }
                        }
                    offs_checked++;
                }
                (void)ons;
                (void)offs_checked;
            }
            printf("   note-ons differing in time or order from the tick without the stage: %u; a release comes at most %.1f ms later\n", early, late_max / 1000.0);
            check("  every note-on at exactly the same time as without the stage, same order; no release earlier", lt == 0u && early == 0u);
            check("  releases at most 3 ms later (the driven frames count neither way)", late_max <= 3000u);
        }
        (void)presses;
    }

    printf("-- the converter is slower than a tick / never finishes\n");
    {
        uint32_t up = 0, down = 0, i;
        static edge_t ref[8192];
        uint32_t nref;
        reset();
        make_hand(0x2545F491u, 120);
        run_until(H.t_end);
        memcpy(ref, edges, sizeof edges);
        nref = nedge;
        reset();
        on();
        conv_us = 140u;
        make_hand(0x2545F491u, 120);
        run_until(H.t_end);
        for (i = 0; i < nedge; i++) {
            up += edges[i].up;
            down += !edges[i].up;
        }
        printf("   140 us conversions: %u key readings dropped of %u started\n", fm1_an.timeouts, n_start_key);
        check("  every key reading is dropped, none is delivered, nothing stays driven", fm1_an.keys == 0u && fm1_an.timeouts > 1000u && rows_out_ticks == 0u);
        check("  the digital scan is untouched", nedge == nref && !memcmp(edges, ref, nedge * sizeof edges[0]));
        reset();
        on();
        adc_never_done = 1;
        make_hand(0x2545F491u, 40);
        run_until(H.t_end);
        check("  a converter that never finishes: dropped each time, the scan goes on, rows on the pull-up", fm1_an.timeouts > 1000u && rows_out_ticks == 0u && fm1_an.keys == 0u);
        reset();
        on();
        slow_us = 250u;                                     /* the pot and the battery take 2.5 ticks */
        run_until(sim_us + 100000u);
        check("  a slow pot / battery reading (250 us) still arrives, the keys lose a slot or two", fm1_an.pot == pot_val && fm1_an.batt == batt_val && fm1_an.keys > 800u);
        reset();
        on();
        slow_us = 900u;                                     /* longer than FM1_AN_SLOW_MAX ticks: dropped */
        run_until(sim_us + 100000u);
        check("  a pot reading slower than FM1_AN_SLOW_MAX ticks is dropped, the keys go on", fm1_an.pot == -1 && fm1_an.timeouts > 0u && fm1_an.keys > 500u);
    }

    printf("-- a late tick and a 40 ms interrupts-off window\n");
    {
        sense_ev_t e;
        uint32_t lands = 0, lifts = 0, unknown = 0, t0;
        reset();
        on();
        pl_rng = 0x5151u;
        keys_make();
        press_add(&H, 12, 150000u, 600000u, 0.6, 0.7);        /* held through the window */
        press_add(&H, 3, 310000u, 200000u, 0.6, 0.7);         /* pressed inside the window (300 .. 340 ms) */
        H.p[12][0].nb = H.p[3][0].nb = 0;
        run_until(300000u);
        t0 = sim_us;
        sim_us += 40000u;                                   /* no ticks for 40 ms (a flash erase) */
        while (sim_us < 900000u) {
            one_tick(0);
            while (sense_event_pop(&e)) {
                lands += e.type == SE_LAND;
                lifts += e.type == SE_LIFT;
                unknown += e.type == SE_LAND && e.a == SC_VEL_UNKNOWN;
            }
        }
        (void)t0;
        printf("   lands %u lifts %u unknown %u, key readings %u dropped %u\n", lands, lifts, unknown, fm1_an.keys, fm1_an.timeouts);
        check("  a held key through the window: no release, no second landing; both keys land and lift once", lands == 2u && lifts == 2u);
        check("  the key pressed inside the window has an unknown landing (the fixed velocity)", unknown == 1u);
        {   /* the clock */
            uint32_t a, b;
            while (fm1__tick_col != 5u)
                one_tick(0);
            one_tick(0);
            a = fm1_an.us;
            host_now = (sim_us + 300u) * 24u;
            sim_us += 300u;
            one_tick(0);
            one_tick(0);
            b = fm1_an.us;
            printf("   clock: %u -> %u us over two ticks, the first 300 us late\n", a, b);
            check("  a late tick: the clock advances by the real time (400 + 100 us over the next two ticks)", b - a == 500u);
        }
        (void)unknown;
    }
#else
    (void)k;
    printf("   (the tick as it was: no converter stage in this build)\n");
#endif
    printf("velo tick: %d failed\n", fails);
    return fails != 0;
}
