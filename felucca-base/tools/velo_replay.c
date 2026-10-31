/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* KEY SENSE replay (tools/velo_replay.py builds and runs this): the raw readings of a recording (docs/VELO_CSV.md,
 * `ts_us,key,adc,drive`) through the firmware's own sense core (firmware/src/sense.c, nothing re-implemented), one landing
 * per output line: `ts_us,key,velocity,from_driven_reading`. Options: --curve PCT (0 .. 100, default 100);
 * --cal FILE (start from stored ranges instead of the typical key: lines `key,hardest,softest` in converter counts, the pair the unit's
 * store holds (no margins since item 123); keys not listed stay the typical key); --ranges (after the landings, one line `# range,key,floor,ceiling`
 * per key: the floor and ceiling the unit maps by at the end). Used by tools/velo_fit.py.
 * After Velo's method (see sense.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/sense.c"

int velo_cal_load(uint8_t *buf, uint32_t n)       /* (no store here: the replay starts from the typical key, as a new unit) */
{
    (void)buf;
    (void)n;
    return -1;
}
int velo_cal_save(const uint8_t *buf, uint32_t n)
{
    (void)buf;
    (void)n;
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f = stdin;
    char line[256];
    uint32_t curve = 100, rows = 0, bad = 0, i, ranges = 0;
    const char *calfile = 0;
    for (i = 1; i < (uint32_t)argc; i++) {
        if (!strcmp(argv[i], "--curve") && i + 1 < (uint32_t)argc)
            curve = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--cal") && i + 1 < (uint32_t)argc)
            calfile = argv[++i];
        else if (!strcmp(argv[i], "--ranges"))
            ranges = 1;
        else if (strcmp(argv[i], "-")) {
            f = fopen(argv[i], "r");
            if (!f) {
                fprintf(stderr, "velo_replay: cannot open %s\n", argv[i]);
                return 2;
            }
        }
    }
    sense_enable(1);
    sense_curve_set(curve);
    if (calfile) {                                                  /* ranges to start from, through the unit's own import */
        uint8_t rec[SENSE_CAL_BYTES];
        uint32_t o = 4u, c, n, lo[SC_NKEY] = {0}, hi[SC_NKEY] = {0};
        FILE *cf = fopen(calfile, "r");
        if (!cf) {
            fprintf(stderr, "velo_replay: cannot open %s\n", calfile);
            return 2;
        }
        while (fgets(line, sizeof line, cf)) {
            unsigned k, a, b;
            if (sscanf(line, "%u,%u,%u", &k, &a, &b) == 3 && k < SC_NKEY)
                lo[k] = a, hi[k] = b;
        }
        fclose(cf);
        rec[0] = 'V', rec[1] = 'C', rec[2] = SC_CAL_VERSION, rec[3] = SC_NKEY;
        for (n = 0; n < SC_NKEY; n++) {
            uint32_t l = lo[n] * 64u, h = hi[n] * 64u;
            rec[o++] = (uint8_t)l, rec[o++] = (uint8_t)(l >> 8);
            rec[o++] = (uint8_t)h, rec[o++] = (uint8_t)(h >> 8);
        }
        c = sc_crc16(rec, o);
        rec[o++] = (uint8_t)c, rec[o++] = (uint8_t)(c >> 8);
        if (!sense_cal_import(rec)) {
            fprintf(stderr, "velo_replay: the ranges were refused\n");
            return 2;
        }
    }
    while (fgets(line, sizeof line, f)) {
        unsigned long long ts;
        unsigned key, adc, drv;
        sense_ev_t e;
        if (line[0] == '#' || line[0] == '\n' || line[0] == 't')       /* comment, blank line, the header */
            continue;
        if (sscanf(line, "%llu,%u,%u,%u", &ts, &key, &adc, &drv) != 4 || adc > 1023u) {
            bad++;
            continue;
        }
        rows++;
        if (key >= SC_NKEY)                                         /* the top 7 keys have no converter */
            continue;
        sense_update(key, adc, drv != 0u, (uint32_t)ts);
        while (sense_event_pop(&e))
            if (e.type == SE_LAND)
                printf("%llu,%u,%u,%u\n", (unsigned long long)e.t, e.key, e.a, e.b);
    }
    if (ranges)
        for (i = 0; i < SC_NKEY; i++) {
            uint32_t f = 0, c = 0;
            sense_cal_get(i, &f, &c);
            printf("# range,%u,%u,%u\n", i, f, c);
        }
    fprintf(stderr, "velo_replay: %u rows, %u unreadable\n", rows, bad);
    return 0;
}
