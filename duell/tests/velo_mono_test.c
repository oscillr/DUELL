/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* DUELL 1.1 (owner: a REALLY hard hit sometimes plays very quiet): harder must always give an equal or higher velocity, never a lower one.
 *   cc -O2 -w -Ifirmware/src -Itests tests/velo_mono_test.c -lm     (host_tests.sh velomono)
 *   (a) the mapping (sk_map) over every driven reading 0 .. 1023, a key never calibrated (after ERASE) and a calibrated one, every curve type: non-increasing in the
 *       reading (a lower reading is a harder press), 127 at the hardest end
 *   (b) the whole press through sense_update, the way the scan feeds it: the landing (undriven), the first driven reading (the strength), then 4.4 ms later the next
 *       driven reading of the pill relaxing (900). Strength swept from the softest to far past the pill range (driven 1022 .. 150, landing 360 .. 150), every curve, never
 *       calibrated and calibrated: the velocity never drops as the hit gets harder. Before the fix a driven reading under SC_D_MIN (250) was thrown away and the
 *       relaxed reading after it was taken as the landing: the hardest hits came out soft */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/sense.c"
int velo_cal_load(uint8_t *buf, uint32_t n) { (void)buf; (void)n; return -1; }
int velo_cal_save(const uint8_t *buf, uint32_t n) { (void)buf; (void)n; return 0; }
static int fails;
static void chk(const char *w, int ok) { printf("velomono: %-4s %s\n", ok ? "ok" : "FAIL", w); fails += !ok; }
static const char *const TN[SC_TYPE_N] = {"SOFTER", "SOFT", "NORMAL", "HARD", "HARDER"};
static void calibrated(sk_t *k) { k->cnt = 40; k->lo6 = 640u * 64u; k->hi6 = 980u * 64u; }   /* an example of a played-in key: hardest 640, softest 980 (item 123: the owner's unit reads 676 .. 1019) */
static uint32_t press(uint32_t note, uint32_t u, uint32_t dhit, int cal, uint32_t *t)
{
    sk_t *k = &sn.k[note];
    uint8_t seq = k->rseq;
    if (cal)
        calibrated(k);
    else
        k->cnt = 0, k->lo6 = k->hi6 = 0;
    sense_update(note, 1023, 0, *t), *t += 2200;
    sense_update(note, u, 0, *t), *t += 2200;                  /* the landing (undriven) */
    sense_update(note, dhit, 1, *t), *t += 2200;               /* the strength (driven) */
    sense_update(note, u + 20u, 0, *t), *t += 2200;
    sense_update(note, 900, 1, *t), *t += 2200;                /* the pill relaxing (driven), 4.4 ms after the strength (900 and over: "open" by value, the way a soft press reads on the owner's unit) */
    sense_update(note, u + 20u, 0, *t), *t += 2200;
    sense_update(note, 1023, 0, *t), *t += 2200;               /* let go */
    sense_update(note, 1023, 0, *t), *t += 2200;
    sense_update(note, 1023, 0, *t), *t += 20000;
    return k->rseq != seq ? k->vel : 0u;
}
int main(void)
{
    uint32_t ty, cal, d, t = 1000;
    sense_enable(1);
    printf("velomono: (a) driven reading -> velocity (every 50 counts shown; checked at every count)\n");
    for (cal = 0; cal < 2u; cal++)
        for (ty = 0; ty < SC_TYPE_N; ty++) {
            sk_t k;
            uint32_t prev = 127u, mono = 1, top;
            char w[128];
            memset(&k, 0, sizeof k);
            if (cal)
                calibrated(&k);
            sense_vel_type_set(ty);
            printf("velomono:   %-11s %-6s", cal ? "calibrated" : "never cal.", TN[ty]);
            top = sk_map(&k, SC_D_MIN);
            for (d = SC_D_MIN; d <= SC_D_MAX; d++) {
                uint32_t v = sk_map(&k, d);
                mono &= v <= prev;
                prev = v;
                if (d % 50u == 0u)
                    printf(" %u:%u", d, v);
            }
            printf("\n");
            snprintf(w, sizeof w, "(a) %s %s: the velocity never rises as the reading rises (a softer press), 127 at the hardest reading", cal ? "calibrated" : "never calibrated", TN[ty]);
            chk(w, mono && top == 127u);
        }
    printf("velomono: (b) the whole press, strength from soft to far past the pill range\n");
    for (cal = 0; cal < 2u; cal++)
        for (ty = 0; ty < SC_TYPE_N; ty++) {
            uint32_t s, prev = 0, mono = 1, last = 0;
            char w[160];
            sense_vel_type_set(ty);
            printf("velomono:   %-11s %-6s", cal ? "calibrated" : "never cal.", TN[ty]);
            for (s = 0; s <= 90u; s++) {                       /* driven 1022 .. 150 (item 123: the owner's unit reads 676 .. 1019; 900 .. 1022 is a soft press), landing 360 .. 150 */
                uint32_t dh = 1022u - s * 972u / 90u, u = 360u - s * 210u / 90u, v = press(3, u, dh, (int)cal, &t);
                mono &= v >= prev;
                if (v < prev)
                    printf(" [DROP at driven %u: %u after %u]", dh, v, prev);
                prev = v;
                last = v;
                if (s % 10u == 0u)
                    printf(" %u:%u", dh, v);
            }
            printf("\n");
            snprintf(w, sizeof w, "(b) %s %s: a harder hit never plays quieter, and the hardest hits play 127", cal ? "calibrated" : "never calibrated", TN[ty]);
            chk(w, mono && last == 127u);
        }
    printf("velomono: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
