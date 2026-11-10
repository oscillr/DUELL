/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL pre-loaded patches: End B as exact values (owner 2026-10-29). Host tool, uses the firmware's own code (like tests/gen_factory_multis.c):
 *   build/host/gen_sound_endb dump [stereo]    for each of the 128 pre-loaded patches: load it as the PATCHES list does (set_engine_of + apply_preset_to: its values, End B
 *                                              rolled from the seed and AMOUNT), SAVE it with the patch save (upreset.c up_patch_save_part), print the PT_VER 4 record:
 *                                              "R <engine> <idx> <len> <hex>" and "O <engine> <idx> <the Original, one value per wide id>". The End B block (MBV) is inside the record; tools/convert_endb.py reads it.
 *   build/host/gen_sound_endb verify [stereo]  for each patch: End A and End B as heard from the OLD way (seed roll) against the NEW way (the End B block of
 *                                              build/gen/duell_sounds.h DS_ENDB loaded as a saved patch loads it): Original values, FM6 patch, End B values, End B FM6 bytes.
 *                                              Prints "DIFF .." lines and "verify: N patches, D differences".
 * Compiled by tools/convert_endb.py against the generated header; run nothing else. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{ const uint8_t *s = src; uint32_t i; if ((off & 0xFFu) + n > 256u) return -1; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
static void power_cycle(void) { ui_power_on(); flash_ok = 1; pt_boot(); frame(); }

static void settle(track_t *t) { int f; for (f = 0; f < 200 && t->engine != t->eng_req; f++) frame(); t->engine = t->eng_req; }
static void load_sound(uint32_t e, uint32_t k, int stereo, const uint8_t *blk)
{
    track_t *t;
    power_cycle();
    song.playing = 0;
    if (stereo)
        duell_mode_force(DM_STEREO);
    t = &trk[0];
    set_engine_of(t, e);
    morph_ldv = blk;                                   /* the New way: End B as a saved patch carries it (consumed by morph_rebase) */
    apply_preset_to(t, k);
    settle(t);
    (void)morph_ready(0);
    morph_ldv = 0;
}

typedef struct { int16_t p[EXT_P_LEN], b[EXT_P_LEN]; uint8_t fm[FP_SIZE + 1u], fmb[NFM], amt; } view_t;
static void view(view_t *v)
{
    morph_part_t *mp = &mph[0];
    uint32_t i;
    memset(v, 0, sizeof *v);
    for (i = 0; i < P_COUNT; i++) v->p[i] = motion_base_value(&trk[0], i);
    for (i = 0; i < NX; i++) v->p[XW_BASE + i] = (int16_t)clamp(XV(&trk[0], i), -64, 127);
    mp->fader = 127; mp->sm_r = 32768;
    morph_effective_x(0, 1, v->b);
    mp->fader = 0;
    memcpy(v->fm, fm6_patch[0], FP_SIZE);
    if (morph_isfm(mp)) {
        uint8_t live[NFM];
        morph_fm_live(0, live);
        for (i = 0; i < NFM; i++) v->fmb[i] = (uint8_t)clamp((int32_t)live[i] + mp->fh[1][mp->top[1]][i] - (int32_t)mp->fb[i], 0, 99);
    }
    v->amt = mp->amount;
}

#ifdef DS_NENDB
static const uint8_t *endb_of(uint32_t g) { return DS_ENDB_AT[g + 1u] > DS_ENDB_AT[g] ? DS_ENDB + DS_ENDB_AT[g] : 0; }
#endif

/* "views": what the firmware's own load gives (no ldv): every sound's Original, End B as heard and FM6 bytes, one line each. Built twice, with the header as it is and with its
 * DS_ENDB_AT all equal (= the seed roll), the two outputs are compared by diff (0 differences = the block load gives what the seed roll gave) */
static void views(int stereo)
{
    uint32_t e, k, w;
    static view_t v;
    for (e = 0; e < NENGINES; e++)
        for (k = 0; k < ds_count(e); k++) {
            load_sound(e, k, stereo, 0);
            view(&v);
            printf("%u %u amt %u |", e, k, v.amt);
            for (w = 0; w < EXT_P_LEN; w++) printf(" %d/%d", v.p[w], v.b[w]);
            printf(" |");
            for (w = 0; w < NFM; w++) printf(" %u", v.fmb[w]);
            printf("\n");
        }
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "views")) { memset(nor, 0xFF, sizeof nor); views(argc > 2 && !strcmp(argv[2], "stereo")); return 0; }
    uint32_t e, k, n = 0, nd = 0, w;
    int stereo = argc > 2 && !strcmp(argv[2], "stereo"), dump = argc > 1 && !strcmp(argv[1], "dump"), ver = argc > 1 && !strcmp(argv[1], "verify");
    if (!dump && !ver) { fprintf(stderr, "usage: gen_sound_endb dump|verify [stereo]\n"); return 2; }
    memset(nor, 0xFF, sizeof nor);
    for (e = 0; e < NENGINES; e++)
        for (k = 0; k < ds_count(e); k++) {
            if (dump) {
                const uint8_t *p;
                uint32_t len, i;
                load_sound(e, k, stereo, 0);
                if (up_patch_save_part(0, 0) != 0) { fprintf(stderr, "save failed %u %u\n", e, k); return 1; }
                len = pt_dir[0].len;
                p = pt_raw(0);
                printf("R %u %u %u ", e, k, len);
                for (i = 0; i < len; i++) printf("%02x", p[i]);
                printf("\n");
                {   /* the Original as loaded (a value per wide id, for the generator's own check) */
                    static view_t v;
                    view(&v);
                    printf("O %u %u", e, k);
                    for (i = 0; i < EXT_P_LEN; i++) printf(" %d", v.p[i]);
                    printf("\n");
                }
            } else {
#ifdef DS_NENDB
                static view_t a, b;
                uint32_t g = DS_ENG_AT[e] + k, nbad = 0;
                load_sound(e, k, stereo, 0);
                view(&a);
                if (!endb_of(g)) { printf("DIFF %u %u: no End B block in the header\n", e, k); nd++; n++; continue; }
                load_sound(e, k, stereo, endb_of(g));
                view(&b);
                for (w = 0; w < EXT_P_LEN; w++) {
                    if (w == 127u) continue;
                    if (a.p[w] != b.p[w]) { printf("DIFF %u %u %s: Original wid %u %d -> %d\n", e, k, ds_name(e, k), w, a.p[w], b.p[w]); nbad++; }
                    if (a.b[w] != b.b[w]) { printf("DIFF %u %u %s: End B wid %u %d -> %d\n", e, k, ds_name(e, k), w, a.b[w], b.b[w]); nbad++; }
                }
                if (memcmp(a.fm, b.fm, FP_SIZE)) { printf("DIFF %u %u %s: FM6 patch\n", e, k, ds_name(e, k)); nbad++; }
                if (memcmp(a.fmb, b.fmb, NFM)) { printf("DIFF %u %u %s: End B FM6 bytes\n", e, k, ds_name(e, k)); nbad++; }
                nd += nbad;
#else
                (void)w; (void)nd;
                fprintf(stderr, "the header has no DS_ENDB\n");
                return 1;
#endif
            }
            n++;
        }
    if (ver) printf("verify: %u patches, %u differences (%s)\n", n, nd, stereo ? "STEREO" : "SPLIT");
    return 0;
}
