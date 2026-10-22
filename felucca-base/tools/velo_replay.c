/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* KEY SENSE replay (tools/velo_replay.py builds and runs this): the raw readings of a recording (docs/VELO_CSV.md,
 * `ts_us,key,adc,drive`) through the firmware's own sense core (firmware/src/sense.c, nothing re-implemented), one landing
 * per output line: `ts_us,key,velocity,from_driven_reading`. Options: --curve PCT (0 .. 100, default 100).
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
    uint32_t curve = 100, rows = 0, bad = 0, i;
    for (i = 1; i < (uint32_t)argc; i++) {
        if (!strcmp(argv[i], "--curve") && i + 1 < (uint32_t)argc)
            curve = (uint32_t)atoi(argv[++i]);
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
    fprintf(stderr, "velo_replay: %u rows, %u unreadable\n", rows, bad);
    return 0;
}
