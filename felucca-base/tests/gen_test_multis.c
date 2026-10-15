/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL: builds the four CPU TEST multis (tools/factory_multis/M13.bin..M16.bin) with the real firmware code: each part's sound loaded
 * by the sound loader (set_engine_of + apply_preset_to), the arp, chords, VOICE, REL, effects, EQ, ducker, MODE and tempo set through
 * the parameters, saved by multi.c's packer (project_save_as), the record read back from the store. Then every record is loaded again
 * in a fresh power-on and printed (engines, patch names, mode, tempo, arp / chord / voice / effect settings) and played (a key held
 * with the chord on: the voices busy must reach 8).
 *   cc -O1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -DFELUCCA_VERSION='"x"' -o build/host/gen_test_multis tests/gen_test_multis.c -lm
 *   build/host/gen_test_multis tools/factory_multis        (writes M13.bin..M16.bin; without an argument only prints) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

enum { E_ANALOG = 0, E_PHASE = 2, E_LOFI = 3, E_WHEEL = 7, E_PHYS = 9, E_NOISE = 11, E_FM6 = 12 };
typedef struct {
    const char *name;
    uint8_t eng[2], idx[2];
    uint8_t mode, rtype, ping, insert[2];
    uint8_t sends[3];                            /* CHO DLY REV of both parts */
    uint8_t rate;                                /* the arp's RATE (N_DIV index) */
} scn_t;
/* sound indexes: duell_sounds.py (engine, factory index): WHEEL 3 JAZZ CLUB, ANALOG 9 SILK CHORUS, PHASE 5 CRYSTAL CAVE, FM6 4 TINE ROOM,
 * NOISE 2 SEA ORGAN, LOFI 2 WOW FLUTTER (PHYS has 3 voices per part: it cannot fill the 8) */
static const scn_t SC[4] = {
    {"TEST 1 BASE",  {E_WHEEL, E_ANALOG}, {3, 9}, DM_LAYER, 2, 0, {6, 8}, {40, 40, 40}, 2},
    {"TEST 2 PHASE", {E_PHASE, E_ANALOG}, {5, 9}, DM_LAYER, 2, 0, {6, 8}, {40, 40, 40}, 2},
    {"TEST 3 HEAVY", {E_FM6, E_PHASE},    {4, 5}, DM_SPLIT, 3, 1, {7, 7}, {50, 50, 50}, 2},
    {"TEST 4 MIX",   {E_NOISE, E_LOFI},   {2, 2}, DM_LAYER, 2, 0, {8, 6}, {60, 60, 60}, 2},
};
static int rel_value(void)                       /* the REL whose text is nearest to 500 ms */
{
    int v, best = 0, bd = 1 << 30;
    for (v = 0; v <= 127; v++) {
        char b[16];
        const char *u;
        int ms = 0, d;
        param_format(&TP[P_REL], v, b, &u);
        {
            const char *p = b;
            while (*p >= '0' && *p <= '9') ms = ms * 10 + *p++ - '0';
            if (*p == '.') {                     /* "1.2" seconds */
                int f = p[1] >= '0' ? p[1] - '0' : 0;
                ms = ms * 1000 + f * 100;
            } else if (u && u[0] == 's' && !u[1])
                ms *= 1000;
        }
        d = ms > 500 ? ms - 500 : 500 - ms;
        if (d < bd) { bd = d; best = v; }
    }
    return best;
}
static void build(const scn_t *s)
{
    uint32_t k;
    ui_power_on();
    for (k = 0; k < 2u; k++) {
        track_t *t = &trk[k];
        set_engine_of(t, s->eng[k]);
        apply_preset_to(t, s->idx[k]);
        t->engine = t->eng_req;
        t->p[P_AMODE] = 1;                       /* arp UP */
        t->p[P_ARATE] = s->rate;
        t->p[P_CHRD] = 6;                        /* MAJ7: a 4-note chord from one key */
        t->p[P_VOIC] = 0;                        /* CLOSE */
        t->p[P_VOICE] = V_POLY;
        t->p[P_REL] = (int16_t)rel_value();
        t->p[P_CHOR] = s->sends[0];
        t->p[P_DLY] = s->sends[1];
        t->p[P_REV] = s->sends[2];
        t->p[P_ITYPE] = s->insert[k];
        t->p[P_IMIX] = 80;
        t->p[P_EQLG] = 3;
        t->p[P_EQMG] = 2;
        t->p[P_EQHG] = -2;
        t->p[P_DKSRC] = 0;                       /* the ducker: PUMP, 1/4, 40 % */
        t->p[P_DKRATE] = 0;
        t->p[P_DKDEP] = 40;
    }
    song.g[G_BPM] = 138;
    song.g[G_CDEPTH] = 60;
    song.g[G_DMIX] = 60;
    song.g[G_RTYPE] = s->rtype;
    song.g[G_DMODE] = s->ping ? DMD_PING : DMD_DIGITAL;
    duell_mode_req = s->mode;
    song.sel = 0;
}
static uint32_t key_of(uint32_t note)           /* the key whose note (octave 0, part 1's scale) is note */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (kb_map(&trk[0], k) == note) return k;
    return 27u;
}
static void show(void)
{
    uint32_t k, busy = 0, i;
    char b[16], nm[16];
    const char *u;
    printf("  multi '%s' tempo %d mode %s  REVERB %s size %d  PING-PONG %s  delay mix %d fdbk %d  chorus depth %d\n", proj_name, song.g[G_BPM],
           DM_NAME[duell_mode_req], N_RTYPE[song.g[G_RTYPE]], song.g[G_RSIZE], song.g[G_DMODE] == DMD_PING ? "ON" : "off", song.g[G_DMIX], song.g[G_DFDBK], song.g[G_CDEPTH]);
    for (k = 0; k < 2u; k++) {
        const track_t *t = &trk[k];
        sound_name(t, nm);
        param_format(&TP[P_REL], t->p[P_REL], b, &u);
        printf("  part %u: engine %-6s patch #%u '%s' | arp %s rate %s | chord %s voicing %s voice %s | REL %s%s | sends CHO %d DLY %d REV %d | insert %s mix %d | EQ %d/%d/%d | duck %s depth %d\n",
               k + 1, ENGINES[t->eng_req % NENGINES]->name, t->preset, nm, N_AMODE[t->p[P_AMODE]], N_DIV[t->p[P_ARATE]], N_CHRD[t->p[P_CHRD]],
               N_VOIC[t->p[P_VOIC]], N_VOICE[t->p[P_VOICE]], b, u ? u : "", t->p[P_CHOR], t->p[P_DLY], t->p[P_REV], N_ITYPE[t->p[P_ITYPE]],
               t->p[P_IMIX], t->p[P_EQLG], t->p[P_EQMG], t->p[P_EQHG], N_DKSRC[t->p[P_DKSRC]], t->p[P_DKDEP]);
    }
    /* played: key C held with the chord on, the arp running, the voices busy sampled over 4 s */
    fm1_in.notes = 1u << key_of(60);
    if (duell_mode_req == DM_SPLIT) {            /* SPLIT: the keys play the selected part only; part 2 on MIDI channel 2 (its own chord and arp) */
        midi_in_event(0x09u | 0x90u << 8 | 60u << 16 | 100u << 24);
        midi_in_event(0x09u | 0x91u << 8 | 60u << 16 | 100u << 24);
    }
    for (i = 0; i < 4u * FS / CTL; i++) {
        uint32_t n;
        events_block(CTL);
        n = voices_busy();
        busy = n > busy ? n : busy;
    }
    fm1_in.notes = 0;
    midi_in_event(0x08u | 0x80u << 8 | 60u << 16);
    midi_in_event(0x08u | 0x81u << 8 | 60u << 16);
    for (i = 0; i < 2u * FS / CTL; i++) events_block(CTL);
    printf("  voices busy at most %u of %u\n", busy, NVOICE * 1u);
}
int main(int argc, char **argv)
{
    uint32_t i, o, h, len;
    printf("REL value for ~500 ms: %d\n", rel_value());
    for (i = 0; i < 4u; i++) {
        uint8_t rec[2000];
        const uint8_t *r;
        build(&SC[i]);
        str_cpy(proj_name, SC[i].name, sizeof proj_name);
        if (project_save_as(12u + i, SC[i].name)) { printf("save failed\n"); return 1; }
        o = (12u + i) / 2u; h = (12u + i) & 1u;
        mu_obj_read(o);
        r = mu_rec(h, &len);
        memcpy(rec, r, len);
        printf("M%u %s: record %u bytes\n", 13u + i, SC[i].name, len);
        if (argc > 1) {
            char path[256];
            FILE *f;
            snprintf(path, sizeof path, "%s/M%02u.bin", argv[1], 13u + i);
            f = fopen(path, "wb");
            fwrite(rec, 1, len, f);
            fclose(f);
        }
    }
    /* verify: a fresh power-on, each factory record loaded back (from ROM when the generated header has it) and played */
    for (i = 0; i < 4u; i++) {
        ui_power_on();
        if (!project_factory(12u + i)) { printf("M%u is not a factory multi in this build (generate, rebuild, run again)\n", 13u + i); continue; }
        project_load(12u + i);
        printf("M%u (%s):\n", 13u + i, mu_names[12u + i]);
        show();
    }
    return 0;
}
