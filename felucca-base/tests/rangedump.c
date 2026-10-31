/* SPDX-License-Identifier: GPL-3.0-only */
/* The stored ranges of every wide value, per engine: min,max for wide id 0..155 (127 the free entry), as the unit's param_desc_of gives them. They are part of the stored
 * format (patch records PT_VER 4, End B blocks, multis: each value in the bits of its range), so the web editor (web/editor/patchrecord.js rangeOf) must have the same.
 *   cc -w -Ibuild/gen -Ifirmware/src -o build/host/rangedump tests/rangedump.c -lm && build/host/rangedump > web/fixtures/patch_ranges.txt
 * web/test_ranges.mjs compares the editor's table with that file. */
#define main hostsim_main
#include "hostsim.c"
#undef main
int main(void)
{
    uint32_t e, w;
    for (e = 0; e < NENGINES; e++) {
        printf("%u:", e);
        for (w = 0; w < EXT_P_LEN; w++) {
            const param_desc_t *d = param_desc_of(e, w);
            printf("%s%d,%d", w ? ";" : "", d->min, d->max);
        }
        printf("\n");
    }
    return 0;
}
