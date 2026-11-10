/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.9: swaps ONE part's sound in a factory multi record (tools/factory_multis/Mnn.bin) and leaves every other byte as it was.
 * Written for M03 DEAD BUFFER, whose part 1 was GRAIN's #2 STRETCH (the GRAIN engine is retired): the new sound is loaded onto that
 * part with the firmware's own code, as the PATCHES browser loads it (the multi loaded first, then set_engine_of + apply_preset_to: a
 * sound load keeps the part's own values, PAN, MUTE and the sequencer's, ui.c param_kept_on), and its values are written into the
 * record's part: the engine byte, the factory index byte and the part's parameter bytes (value + 64). The rest stays byte for byte:
 * the header (name, MODE, MORPH TARGET and SET), the globals (tempo, the effects), the part's AMOUNT, its End B block (the seed
 * and the roll's settings: End B is rolled again from the new sound with the same seed), its steps, the other part, the automation and
 * the mix bytes. Checked here: the record decodes (multi.c mu_unpack), only the part's bytes changed, and loaded it gives the new sound.
 *   cc -O1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -DFELUCCA_VERSION='"x"' -o build/host/swap_multi_sound tests/swap_multi_sound.c -lm
 *   build/host/swap_multi_sound IN.bin PART ENGINE INDEX OUT.bin [ID=VALUE ..]   (PART 1 or 2; ENGINE the engine number, INDEX its
 *                                                   factory sound; ID=VALUE: a parameter set after the load, the owner's own edits of
 *                                                   the part that are not the old sound's, carried over by hand: core.h P_ ids)
 * M03 (0.2.9): the owner had edited part 1 beyond STRETCH (compared with STRETCH as it loaded): LFO 1 RATE 46 (SIN) feeding matrix slot 2
 * LFO1 -> PAN +35 (an auto-pan), DIST 23, slot 3 LFO2 -> DIST -8, DUCK CURVE 0. They are carried over (FROZEN AIR's own slot 3, LFO1 ->
 * DTN, gives way to his):
 *   build/host/swap_multi_sound tools/factory_multis/M03.bin 1 14 17 tools/factory_multis/M03.bin 9=46 10=0 33=23 52=1 53=5 54=35 55=12 56=6 57=-8 105=0 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define SW_MAX 2048u
int main(int argc, char **argv)
{
    static uint8_t in[SW_MAX], out[SW_MAX];
    static project_t q;
    uint8_t blk[NSHOWN][MB_N], amt[NSHOWN], set[6];
    uint32_t len, part, eng, idx, pos, t, i, np, at = 0, nchg = 0;
    FILE *f;
    if (argc < 6)
        return fprintf(stderr, "usage: swap_multi_sound IN.bin PART ENGINE INDEX OUT.bin [ID=VALUE ..]\n"), 2;
    part = (uint32_t)atoi(argv[2]) - 1u;
    eng = (uint32_t)atoi(argv[3]);
    idx = (uint32_t)atoi(argv[4]);
    if (part >= NSHOWN || !eng_ok(eng) || idx >= ds_count(eng))
        return fprintf(stderr, "swap_multi_sound: part 1 or 2, a pickable engine and one of its factory sounds\n"), 2;
    f = fopen(argv[1], "rb");
    if (!f)
        return perror(argv[1]), 1;
    len = (uint32_t)fread(in, 1, sizeof in, f);
    fclose(f);
    ui_power_on();
    project_capture(&q);
    memset(blk, 0, sizeof blk); memset(amt, 0, sizeof amt); memset(set, 0, sizeof set);
    if (!mu_unpack(&q, blk, amt, set, in, len))
        return fprintf(stderr, "swap_multi_sound: %s is not a multi record\n", argv[1]), 1;
    /* the multi as it plays: its parts restored (multi.c project_load's path), then the new sound onto the part */
    proj_bound(&q);
    if (project_restore_runtime(&q))
        return fprintf(stderr, "swap_multi_sound: the record does not restore\n"), 1;
    song.sel = (uint8_t)part;
    set_engine_of(&trk[part], eng);
    apply_preset_to(&trk[part], idx);
    for (i = 6; i < (uint32_t)argc; i++) {             /* the owner's own edits, carried over */
        int id = -1, v = 0;
        if (sscanf(argv[i], "%d=%d", &id, &v) != 2 || id < 0 || id >= (int)P_COUNT || (id >= (int)P_E0 && id < (int)P_E0 + 8))
            return fprintf(stderr, "swap_multi_sound: %s: ID=VALUE, a P_ id of the part (not an engine value)\n", argv[i]), 2;
        trk[part].p[id] = (int16_t)clamp(v, param_desc_of(eng, (uint32_t)id)->min, param_desc_of(eng, (uint32_t)id)->max);
    }
    /* the record's part: walk as tools/gen_duell_multis.py check() does */
    np = in[1];
    pos = MU_G_AT + 2u * in[2];
    for (t = 0; t < NSHOWN; t++) {
        uint32_t fl = in[pos + 3];
        if (t == part)
            at = pos;
        pos += 4u + MB_N + np;
        for (i = 0, nchg = 0; i < 8u; i++)
            nchg += (uint32_t)__builtin_popcount(in[pos + i]);
        pos += 8u + PS_STEP * nchg + (fl & 1u ? FM6_PACKED : 0u);
    }
    if (np != P_COUNT || (in[at + 3] & 1u) || eng == ENGI_FM6)
        return fprintf(stderr, "swap_multi_sound: only a record of this firmware's P_COUNT, no FM6 patch on either side\n"), 1;
    memcpy(out, in, len);
    out[at] = (uint8_t)eng;
    out[at + 1u] = (uint8_t)trk[part].preset;
    for (i = 0; i < P_COUNT; i++)
        out[at + 4u + MB_N + i] = (uint8_t)(clamp(trk[part].p[i], -64, 127) + 64);
    for (i = 0, nchg = 0; i < len; i++)
        if (out[i] != in[i]) {
            nchg++;
            if (i < at || i >= at + 4u + MB_N + P_COUNT || (i >= at + 2u && i < at + 4u + MB_N))
                return fprintf(stderr, "swap_multi_sound: byte %u outside the part's sound changed\n", i), 1;
        }
    /* loaded back: the new sound on the part, the other part as it was */
    {
        static project_t c;
        project_t *o = &c;
        uint8_t b2[NSHOWN][MB_N], a2[NSHOWN], s2[6];
        ui_power_on();
        project_capture(o);
        memset(b2, 0, sizeof b2); memset(a2, 0, sizeof a2); memset(s2, 0, sizeof s2);
        if (!mu_unpack(o, b2, a2, s2, out, len) || o->t[part].engine != eng || o->t[part].preset != out[at + 1u] ||
            memcmp(b2, blk, sizeof blk) || memcmp(a2, amt, sizeof amt) || memcmp(s2, set, sizeof set) ||
            o->t[1u - part].engine != q.t[1u - part].engine || o->t[1u - part].preset != q.t[1u - part].preset)
            return fprintf(stderr, "swap_multi_sound: the new record does not decode as intended (engine %u #%u, blk %d amt %d set %d, "
                           "other %u #%u)\n", o->t[part].engine, o->t[part].preset, !memcmp(b2, blk, sizeof blk), !memcmp(a2, amt, sizeof amt),
                           !memcmp(s2, set, sizeof set), o->t[1u - part].engine, o->t[1u - part].preset), 1;
    }
    printf("swap_multi_sound: part %u: engine %u #%u -> %s #%u %s; %u bytes of %u changed (the part's engine, index and values only)\n",
           part + 1u, in[at], in[at + 1u], ENGINES[eng]->name, idx, ds_name(eng, idx), nchg, len);
    f = fopen(argv[5], "wb");
    if (!f)
        return perror(argv[5]), 1;
    fwrite(out, 1, len, f);
    fclose(f);
    return 0;
}
