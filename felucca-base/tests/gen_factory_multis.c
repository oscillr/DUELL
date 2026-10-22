/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL factory multis (0.2.11; 0.2.23: all sixteen, the table from tools/factory_multis.py): builds the factory multis
 * (tools/factory_multis/Mnn.bin) with the firmware's own code: each part's factory sound loaded as the PATCHES list loads it
 * (set_engine_of + apply_preset_to: its values, End B seed and AMOUNT), then the multi's own per-part settings (the mix, the sends,
 * HOLD, chord keys, a matrix route, the arp and its SEQUENCER pattern, the insert, EQ and ducker), the globals (tempo, chorus, delay,
 * reverb), the MODE, BALANCE, XFADE and SPLIT, saved by multi.c's packer (project_save_as) and read back from the store. The table is
 * tools/factory_multis.py's; run that tool (`build`), which writes the C table, compiles and runs this. The recipes: DUELL/SOUND_RECIPES.md, chapter 7.
 *   build/host/gen_factory_multis build tools/factory_multis    writes the built slots' Mnn.bin (tools/factory_multis.py build does it)
 *   build/host/gen_factory_multis dump [SLOT ..]                 what the factory multis of the generated header hold (default: all)
 *   build/host/gen_factory_multis loud                           the loudness of the factory multis (needs the header regenerated with them)
 *   build/host/gen_factory_multis search SLOT PART AMOUNT N [SEED]  (0.2.13) End B candidates of PART at AMOUNT: level, brightness against End A
 *   build/host/gen_factory_multis eval SLOT [S1 A1 S2 A2]         (0.2.13) End A against End B (both parts, each alone; 3 keys and C4)
 *   build/host/gen_factory_multis moves SLOT [S1 A1 [S2 A2]]       the values End B moves (optionally rolled again with these seeds / AMOUNTs)
 * (dump, loud, search, eval, moves: compiled against the generated build/gen_test/duell_multis.h, which holds the multis)
 * Loudness ('loud'): each multi loaded in a fresh power-on with its own MODE (as MODE RECALL ON) and played 10 s as it is meant to
 * be played (a multi with a stored step pattern on a part without the SEQUENCER arp: PLAY, no key; any other, the six since 0.2.12:
 * the keys C4 E4 G4 held, or C4 alone in the second table: the arps run on the internal clock), the master output (after MASTER at its default and the limiter)
 * measured from 2 s to 10 s: K-weighted (ITU-R BS.1770 pre-filter and RLB high-pass, no gate), -0.691 + 10 log10 of the
 * channels' summed mean square, full scale 32767 = 0 (LUFS-like, relative); also the peak. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

enum { DV_1_16 = 2 };                             /* N_DIV: the steps' DIV */
#define END (-1)

typedef struct {
    uint8_t eng, idx;                            /* the factory sound (engine, factory index) */
    const char *sound;                           /* its name (checked) */
    int16_t set[96];                             /* (P_ id, value) pairs after the load, END-terminated */
    const char *steps;                           /* the SEQUENCER arp's pattern (space separated): "." rest, a signed offset in
                                                  * semitones from the held key, suffix A accent; 0: no pattern */
    uint8_t len;
    uint16_t bseed;                              /* End B: 0 the sound's own; else rolled again with this seed (the sound's AMOUNT) */
    uint8_t amt;                                 /* 0.2.13: 0 the sound's AMOUNT; else End B rolled at this AMOUNT (%) and the part's AMOUNT set to it */
} part_t;
typedef struct {
    const char *name;
    uint8_t slot;                                /* 0..15: M01..M16 */
    uint8_t mode;                                /* DM_SPLIT (DUEL), DM_STEREO, DM_LAYER */
    int8_t bal;
    uint8_t xf;                                  /* XFADE 0..100 (LAYER) */
    uint8_t split;                               /* 0 OFF, else note + 1 (LAYER) */
    int16_t g[40];                               /* (G_ id, value) pairs, END-terminated */
    part_t p[2];
} multi_t;

/* DUELL 0.2.23: the table is tools/factory_multis.py's (MULTIS), written as C by its `build` (build/gen_mm/factory_multis_def.h);
 * the slots it marks `kept` are records kept byte for byte and are not in it */
#include "factory_multis_def.h"
#define NM (sizeof MM / sizeof *MM)

static int parse_steps(track_t *t, const char *s, uint32_t len)
{
    uint32_t i = 0;
    for (i = 0; i < NSTEP; i++)
        step_clear(&t->step[i]);
    t->p[P_SLEN] = (int16_t)len;
    t->p[P_SDIV] = DV_1_16;
    if (!s)
        return 0;
    for (i = 0; *s && i < NSTEP; i++) {
        step_t *st = &t->step[i];
        while (*s == ' ') s++;
        if (*s == '.') { st->time = ST_REST; s++; }
        else {
            int32_t sg = 1, v = 0;
            if (*s == '+' || *s == '-')
                sg = *s++ == '-' ? -1 : 1;
            if (*s < '0' || *s > '9') {
                fprintf(stderr, "bad step '%s'\n", s);
                exit(1);
            }
            while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
            st->time = ST_NOTE;
            st->n = 1;
            st->vel = 100;
            st->note[0] = (uint8_t)(60 + sg * v);
            for (; *s == 'A'; s++)
                st->flags |= SF_ACCENT;
        }
        while (*s == ' ') s++;
    }
    if (i != len) {
        fprintf(stderr, "pattern has %u steps, LEN %u\n", i, len);
        exit(1);
    }
    return 1;
}

static void build(const multi_t *m)
{
    uint32_t k, i;
    ui_power_on();
    for (k = 0; k < 2u; k++) {
        const part_t *p = &m->p[k];
        track_t *t = &trk[k];
        set_engine_of(t, p->eng);
        apply_preset_to(t, p->idx);
        t->engine = t->eng_req;
        if (!str_eq(ds_name(p->eng, p->idx), p->sound)) {
            fprintf(stderr, "%s part %u: engine %u #%u is %s, not %s\n", m->name, k + 1u, p->eng, p->idx, ds_name(p->eng, p->idx), p->sound);
            exit(1);
        }
        for (i = 0; p->set[i] != END; i += 2u) {
            uint32_t id = (uint32_t)p->set[i];
            if (id == P_M8SRC && t->p[P_M8SRC]) {
                fprintf(stderr, "%s part %u: matrix slot 8 is the sound's own\n", m->name, k + 1u);
                exit(1);
            }
            t->p[id] = p->set[i + 1u];
        }
        parse_steps(t, p->steps, p->len);
    }
    for (k = 0; k < 2u; k++)
        if (m->p[k].bseed || m->p[k].amt) {
            uint8_t blk[MB_N];
            morph_blk_make(k, blk);
            morph_roll_cfg(k, 1u, m->p[k].bseed ? m->p[k].bseed : (uint32_t)(blk[1] | blk[2] << 8), m->p[k].amt ? m->p[k].amt : blk[3], blk[4]);
            if (m->p[k].amt)
                morph_ready(k)->amount = m->p[k].amt;   /* (the record's AMOUNT byte and the block's knob byte) */
        }
    for (i = 0; m->g[i] != END; i += 2u)
        song.g[m->g[i]] = m->g[i + 1u];
    duell_mode_req = m->mode;
    mix_bal = m->bal;
    mix_xf = m->xf;
    mix_split = m->split;
    song.sel = 0;
}

static void dump(uint32_t k)
{
    uint32_t pi, i;
    char nm[16];
    ui_power_on();
    if (!project_factory(k)) {
        printf("M%02u: no factory multi\n", k + 1u);
        return;
    }
    project_load(k);
    printf("M%02u %-12s MODE %s  BPM %d SWING %d  DLY %s fdbk %d mix %d  REV %s size %d damp %d  PING %d  DMOD %d  BAL %d XF %u SPLIT %u\n",
           k + 1u, proj_name, DM_NAME[duell_mode_req], song.g[G_BPM], song.g[G_SWING], N_DIV[song.g[G_DTIME] % 17], song.g[G_DFDBK],
           song.g[G_DMIX], N_RTYPE[song.g[G_RTYPE] % 5], song.g[G_RSIZE], song.g[G_RDAMP], song.g[G_DMODE] == DMD_PING, song.g[G_DMDEP], mix_bal, mix_xf, mix_split);
    for (pi = 0; pi < NSHOWN; pi++) {
        const track_t *t = &trk[pi];
        uint8_t b[MB_N];
        uint32_t used = 0;
        memset(b, 0, sizeof b);
        morph_blk_make(pi, b);
        sound_name(t, nm);
        printf("  part %u: %-6s #%-2u %-12s | End B seed %u AMOUNT %u | LVL %d | VCE %s TRN %d | arp %s hold %s | CHRD %s %s | "
               "CHO %d DLY %d REV %d | M8 %s>%s %d | LEN %d | ATK %d | ARP RATE %s GATE %d OCT %d | UF CUT %d SLOPE %d\n",
               pi + 1u, ENGINES[t->eng_req % NENGINES]->name, t->preset, nm, b[1] | b[2] << 8, morph_amount_of(pi), t->p[P_TRIM],
               N_VOICE[t->p[P_VOICE] % 4], t->p[P_TRANS], N_AMODE[t->p[P_AMODE] % 16], N_HOLD[t->p[P_AHOLD] % 3], N_CHRD[t->p[P_CHRD] % 10],
               N_VOIC[t->p[P_VOIC] % 5], t->p[P_CHOR], t->p[P_DLY], t->p[P_REV], N_MSRC[t->p[P_M8SRC] % 15], N_MDST[t->p[P_M8DST] % 23],
               t->p[P_M8AMT], t->p[P_SLEN], t->p[P_ATK], N_DIV[t->p[P_ARATE] % 17], t->p[P_AGATE], t->p[P_AOCT], t->p[P_FREEA], t->p[P_FREED]);
        printf("    steps:");
        for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++) {
            const step_t *s = &t->step[i];
            uint32_t j;
            if (s->time == ST_REST || (!s->n && s->time != ST_TIE)) {
                printf(" .");
                continue;
            }
            used++;
            printf(" ");
            if (s->time == ST_TIE)
                printf("-");
            for (j = 0; j < s->n && s->time != ST_TIE; j++)
                printf("%s%u", j ? "+" : "", s->note[j]);
            if (s->flags & SF_ACCENT) printf("A");
            if (s->flags & SF_SLIDE) printf("S");
        }
        printf("%s\n", used ? "" : " (none)");
    }
}

static uint32_t key_of(uint32_t note)            /* the key whose note (octave 0) is note */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (kb_map(&trk[0], k) == note) return k;
    return 27u;
}
/* a biquad (direct form 1) */
typedef struct { double b0, b1, b2, a1, a2, x1, x2, y1, y2; } bq_t;
static double bq(bq_t *f, double x)
{
    double y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1; f->x1 = x; f->y2 = f->y1; f->y1 = y;
    return y;
}
static void kw_init(bq_t *sh, bq_t *hp, double fs)    /* BS.1770's two stages at fs (the analogue prototype's constants) */
{
    double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196, K = tan(M_PI * f0 / fs);
    double Vh = pow(10.0, G / 20.0), Vb = pow(Vh, 0.4996667741545416), a0 = 1.0 + K / Q + K * K;
    memset(sh, 0, sizeof *sh);
    sh->b0 = (Vh + Vb * K / Q + K * K) / a0;
    sh->b1 = 2.0 * (K * K - Vh) / a0;
    sh->b2 = (Vh - Vb * K / Q + K * K) / a0;
    sh->a1 = 2.0 * (K * K - 1.0) / a0;
    sh->a2 = (1.0 - K / Q + K * K) / a0;
    f0 = 38.13547087602444; Q = 0.5003270373238773; K = tan(M_PI * f0 / fs);
    memset(hp, 0, sizeof *hp);
    hp->b0 = 1.0; hp->b1 = -2.0; hp->b2 = 1.0;
    hp->a1 = 2.0 * (K * K - 1.0) / (1.0 + K / Q + K * K);
    hp->a2 = (1.0 - K / Q + K * K) / (1.0 + K / Q + K * K);
}
static uint32_t ov_seed[2], ov_amt[2], quiet;    /* search: End B of part 1 / 2 rolled again with this seed / AMOUNT (0: as stored) */
static double r_lu, r_br, r_pk;                  /* the last loud(): K-weighted level, brightness, peak */
static uint32_t at_b, b_seed;                    /* b_seed: End B rolled again with this seed (the soloed part) before measuring */                            /* loud: 1 = both MORPH faders at End B */
static void loud(uint32_t k, uint32_t nkeys, uint32_t solo)   /* solo: 0 both parts, 1 / 2 the other muted */
{
    double d = 0, r = 0, px[2] = {0, 0};
    static const uint8_t KEYS[3] = {60, 64, 67};
    uint32_t kmask = 0;
    bq_t sh[2], hp[2];
    uint32_t b, i, c, nb = (uint32_t)(10.0 * FS) / CTL, b0 = (uint32_t)(2.0 * FS) / CTL, pat = 0, n = 0;
    double e = 0, pk = 0;
    int32_t out[2 * CTL];
    ui_power_on();
    if (!project_factory(k))
        return;
    ui_prefs2 |= PREF_MODE_RECALL >> 16;         /* (0.2.12: the multi's own MODE, as it is meant to be played: all LAYER; with
                                                  * MODE RECALL OFF the keys played part 1 alone in a power-on's DUEL) */
    for (i = 0; i < nkeys; i++)
        kmask |= 1u << key_of(KEYS[i]);         /* (the keys of a power-on: the multi's TRN must not move them) */
    project_load(k);
    song.master_q12 = 2048;                      /* (MASTER at its default position, main.c) */
    if (solo)
        trk[2u - solo].p[P_MUTE] = 1;
    if (b_seed && solo) {
        uint8_t blk[MB_N];
        morph_blk_make(solo - 1u, blk);
        morph_roll_cfg(solo - 1u, 1u, b_seed, blk[3], blk[4]);
    }
    for (c = 0; c < 2u; c++)
        if (ov_seed[c] || ov_amt[c]) {
            uint8_t blk[MB_N];
            morph_blk_make(c, blk);
            morph_roll_cfg(c, 1u, ov_seed[c] ? ov_seed[c] : (uint32_t)(blk[1] | blk[2] << 8), ov_amt[c] ? ov_amt[c] : blk[3], blk[4]);
        }
    if (at_b)
        morph_ready(0)->fader = morph_ready(1)->fader = 127;
    for (c = 0; c < 2u; c++)
        for (i = 0; i < NSTEP; i++)
            pat |= trk[c].step[i].time == ST_NOTE && trk[c].step[i].n && trk[c].p[P_AMODE] == 0;
    kw_init(&sh[0], &hp[0], FS);
    sh[1] = sh[0];
    hp[1] = hp[0];
    for (i = 0; i < 8u; i++) {
        mix_block(out, CTL);                     /* (the MODE taken, its dip over) */
        while (wave_service())                   /* (WAVE: the tables built, as the main loop does between blocks) */
            ;
    }
    if (pat)
        transport_req = 1;
    else
        fm1_in.notes = kmask;
    for (b = 0; b < nb; b++) {
        mix_block(out, CTL);
        while (wave_service())
            ;
        if (b < b0)
            continue;
        for (i = 0; i < CTL; i++)
            for (c = 0; c < 2u; c++) {
                double x = out[2u * i + c] / 32767.0, y = bq(&hp[c], bq(&sh[c], x));
                e += y * y;
                r += x * x;
                d += (x - px[c]) * (x - px[c]);
                px[c] = x;
                pk = fabs(x) > pk ? fabs(x) : pk;
            }
        n += CTL;
    }
    fm1_in.notes = 0;
    r_lu = -0.691 + 10 * log10(e / n + 1e-30);
    r_br = r > 0 ? sqrt(d / r) * FS / (2.0 * M_PI) : 0.0;
    r_pk = 20 * log10(pk + 1e-12);
    if (quiet)
        return;
    printf("M%02u %-12s %-6s %-8s %s K-weighted %6.1f dB   peak %5.1f dBFS   brightness %5.0f Hz\n", k + 1u, mu_names[k], solo == 1u ? "part 1" : solo == 2u ? "part 2" : "", pat ? "PLAY" : nkeys == 1u ? "C4" : "C4 E4 G4", at_b ? "End B" : "End A",
           -0.691 + 10 * log10(e / n + 1e-30), 20 * log10(pk + 1e-12), r > 0 ? sqrt(d / r) * FS / (2.0 * M_PI) : 0.0);
}

int main(int argc, char **argv)
{
    uint32_t i;
    if (argc > 2 && str_eq(argv[1], "build")) {
        for (i = 0; i < NM; i++) {
            uint8_t rec[2048];
            const uint8_t *r;
            uint32_t len, slot = MM[i].slot;
            char path[512];
            FILE *f;
            build(&MM[i]);
            str_cpy(proj_name, MM[i].name, sizeof proj_name);
            if (project_save_as(slot, MM[i].name)) { printf("save failed\n"); return 1; }
            mu_obj_read(slot / 2u);
            r = mu_rec(slot & 1u, &len);
            memcpy(rec, r, len);
            snprintf(path, sizeof path, "%s/M%02u.bin", argv[2], slot + 1u);
            f = fopen(path, "wb");
            if (!f) return perror(path), 1;
            fwrite(rec, 1, len, f);
            fclose(f);
            printf("M%02u %-12s %u bytes -> %s\n", slot + 1u, MM[i].name, len, path);
        }
        return 0;
    }
    if (argc > 1 && str_eq(argv[1], "dump")) {
        if (argc > 2)
            for (i = 2; i < (uint32_t)argc; i++)
                dump((uint32_t)atoi(argv[i]) - 1u);
        else
            for (i = 0; i < MU_SLOTS; i++) {
                ui_power_on();
                if (project_factory(i))
                    dump(i);
            }
        return 0;
    }
    if (argc > 4 && str_eq(argv[1], "seeds")) {    /* seeds SLOT PART N: End B candidates of a part (the LCG of tests/sound_audit.c) */
        uint32_t k = (uint32_t)atoi(argv[2]) - 1u, pt = (uint32_t)atoi(argv[3]), n = (uint32_t)atoi(argv[4]), seed;
        uint8_t blk[MB_N];
        ui_power_on();
        project_load(k);
        morph_blk_make(pt - 1u, blk);
        seed = blk[1] | blk[2] << 8;
        printf("M%02u part %u: End B seed %u AMOUNT %u cfg %u\n", k + 1u, pt, seed, blk[3], blk[4]);
        loud(k, 3u, pt);
        for (i = 0; i < n; i++) {
            b_seed = seed;
            at_b = 1;
            printf("seed %5u: ", seed);
            loud(k, 3u, pt);
            seed = (seed * 1103515245u + 12345u) % 65535u + 1u;
        }
        return 0;
    }
    if (argc > 5 && str_eq(argv[1], "search")) {   /* search SLOT PART AMOUNT N [SEED_OTHER]: End B of PART rolled with N seeds at AMOUNT
                                                    * (the other part at AMOUNT, its own seed or SEED_OTHER), both parts, C4 E4 G4: level and
                                                    * brightness of End B against End A (0.2.13: M01 REESE SWAMP's End B at 17 %) */
        uint32_t k = (uint32_t)atoi(argv[2]) - 1u, pt = (uint32_t)atoi(argv[3]) - 1u, am = (uint32_t)atoi(argv[4]), n = (uint32_t)atoi(argv[5]), seed = 1u;
        double la, ba;
        quiet = 1;
        loud(k, 3u, 0);
        la = r_lu; ba = r_br;
        printf("M%02u End A: %6.1f dB brightness %5.0f Hz\n", k + 1u, la, ba);
        ov_amt[0] = ov_amt[1] = am;
        ov_seed[1u - pt] = argc > 6 ? (uint32_t)atoi(argv[6]) : 0u;
        at_b = 1;
        for (i = 0; i < n; i++) {
            seed = (seed * 1103515245u + 12345u) % 65535u + 1u;
            ov_seed[pt] = seed;
            loud(k, 3u, 0);
            printf("seed %5u: End B %6.1f dB (%+5.1f) peak %5.1f brightness %5.0f Hz (%+5.1f %%)\n", seed, r_lu, r_lu - la, r_pk, r_br, 100.0 * (r_br / ba - 1.0));
        }
        return 0;
    }
    if (argc > 2 && str_eq(argv[1], "eval")) {     /* eval SLOT [SEED1 AMOUNT1 SEED2 AMOUNT2]: End A and End B, C4 E4 G4 and C4, each part alone too */
        uint32_t k = (uint32_t)atoi(argv[2]) - 1u, nk, so;
        if (argc > 6) {
            ov_seed[0] = (uint32_t)atoi(argv[3]); ov_amt[0] = (uint32_t)atoi(argv[4]);
            ov_seed[1] = (uint32_t)atoi(argv[5]); ov_amt[1] = (uint32_t)atoi(argv[6]);
        }
        quiet = 1;
        for (so = 0; so < 3u; so++)
            for (nk = 3u; nk != 0u && nk <= 3u; nk = nk == 3u ? 1u : 0u) {
                double la, ba, pa;
                at_b = 0; loud(k, nk, so); la = r_lu; ba = r_br; pa = r_pk;
                at_b = 1; loud(k, nk, so);
                printf("M%02u %-6s %-8s End A %6.1f dB peak %5.1f bright %5.0f Hz | End B %6.1f dB (%+5.1f) peak %5.1f bright %5.0f Hz (%+5.1f %%)\n",
                       k + 1u, so ? (so == 1u ? "part 1" : "part 2") : "both", nk == 3u ? "C4 E4 G4" : "C4", la, pa, ba, r_lu, r_lu - la, r_pk, r_br, 100.0 * (r_br / ba - 1.0));
            }
        return 0;
    }
    if (argc > 2 && str_eq(argv[1], "moves")) {    /* moves SLOT: the values End B moves on each part (the faders at 127) */
        uint32_t k = (uint32_t)atoi(argv[2]) - 1u, pi, id;
        int32_t out[2 * CTL];
        ui_power_on();
        project_load(k);
        for (pi = 0; pi < 2u && argc > 4u + 2u * pi; pi++) {   /* moves SLOT [SEED1 AMOUNT1 [SEED2 AMOUNT2]]: End B rolled again first */
            uint8_t blk[MB_N];
            morph_blk_make(pi, blk);
            morph_roll_cfg(pi, 1u, (uint32_t)atoi(argv[3u + 2u * pi]), (uint32_t)atoi(argv[4u + 2u * pi]), blk[4]);
        }
        morph_ready(0)->fader = morph_ready(1)->fader = 127;
        for (i = 0; i < 16u; i++)
            mix_block(out, CTL);
        for (pi = 0; pi < 2u; pi++) {
            printf("M%02u part %u End B moves:", k + 1u, pi + 1u);
            for (id = 0; id < P_COUNT; id++)
                if (morph_view(pi, id) != trk[pi].p[id])
                    printf(" %s(%d>%d)", param_desc_of(trk[pi].engine, id)->label, trk[pi].p[id], (int)morph_view(pi, id));
            printf("\n");
        }
        return 0;
    }
    if (argc > 1 && str_eq(argv[1], "loud")) {
        for (i = 0; i < MU_SLOTS; i++)
            loud(i, 3u, 0);
        for (i = 0; i < MU_SLOTS; i++)
            loud(i, 1u, 0);
        at_b = 1;                                /* End B (both faders at B), the 3 keys / PLAY */
        for (i = 0; i < MU_SLOTS; i++)
            loud(i, 3u, 0);
        at_b = 0;
        if (argc > 2)                            /* loud parts: each part alone (3 keys / PLAY) */
            for (i = 0; i < 4u * MU_SLOTS; i++) {
                at_b = i >= 2u * MU_SLOTS;
                loud(i % (2u * MU_SLOTS) / 2u, 3u, 1u + i % 2u);
            }
        return 0;
    }
    fprintf(stderr, "usage: gen_factory_multis build DIR | dump [SLOT ..] | loud\n");
    return 2;
}
