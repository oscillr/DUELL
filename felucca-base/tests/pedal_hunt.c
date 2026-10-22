/* SPDX-License-Identifier: GPL-3.0-only
 * PEDAL BASS hunt (release_drafts/PEDAL_BASS_HUNT.md): the twin (tests/bench_sim.c) with its own main. build/host/pedal_hunt dump N: patch N's
 * values with names, End A and End B and what differs (what the fader moves). Not part of the test list.
 * Build (bench_sim.c has its own main, so a copy with that one renamed is included):
 *   sed 's/^int main(int argc, char \*\*argv)$/int bench_main(int argc, char **argv)/' tests/bench_sim.c > tests/.bs_copy.c
 *   cc -O1 -fno-builtin -D_FORTIFY_SOURCE=0 -w -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests '-DFELUCCA_VERSION="x"' -o build/host/pedal_hunt tests/pedal_hunt.c -lm */

#include ".bs_copy.c"

#include <stdio.h>
static const param_desc_t *pdesc(track_t *t, uint32_t id)
{
    if (id < (uint32_t)P_E0)
        return &TP[id];
    return &ENGINES[t->engine]->edit[id - P_E0];
}
static void show(const param_desc_t *d, int v, char *o)
{
    if (d->fmt == F_ENUM && d->names && v >= d->min && v <= d->max)
        sprintf(o, "%d (%s)", v, d->names[v - d->min]);
    else
        sprintf(o, "%d", v);
}
int main(int argc, char **argv)
{
    char b[64], x[48], y[48], z[48];
    uint32_t id;
    int16_t a[P_COUNT], e[P_COUNT];
    track_t *t = &trk[0];
    sim_power_on();
    bench_resolve();
    snprintf(b, sizeof b, "load patch 1 %d", argc > 2 ? atoi(argv[2]) : 18);
    tl_setup("STEREO", b);
    printf("engine %s  fader %u  amount %u\n", ENGINES[t->engine]->name, morph_fader_of(0), morph_amount_of(0));
    morph_effective(0, 0, a);
    morph_effective(0, 1, e);
    for (id = 0; id < P_COUNT; id++) {
        const param_desc_t *d = id < (uint32_t)P_E0 ? &TP[id] : &ENGINES[t->engine]->edit[id - P_E0];
        if (!d->label || !d->label[0])
            continue;
        show(d, t->p[id], x);
        show(d, a[id], y);
        show(d, e[id], z);
        printf("%3u %-10s stored %-14s A %-14s B %-14s %s\n", id, d->label, x, y, z, a[id] != e[id] ? "<== MORPH MOVES" : "");
    }
    return 0;
}
