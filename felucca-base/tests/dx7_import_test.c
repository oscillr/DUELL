/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.19's DX7 import: the editor converts a DX7 voice (web/editor/dx7import.js) into a DUELL patch RECORD of the FM6 engine; this test takes those records
 * (web/test_librarian.mjs writes the fixture from the synthetic banks of web/test_dx7_synth.mjs: 63 voices, the one damaged voice left out; with DX7_BANKS=dir
 * it adds the banks found there, and the count check then needs at least 63) and checks, with the
 * firmware's own code, that
 *   1. the store takes each record (up_put_raw: pt_rec_ok, name, values) and holds exactly its bytes;
 *   2. up_load brings every parameter of the editor's neutral FM6 patch (values) and the voice itself (the FM6 patch, 128 bytes) into the part: the same bytes
 *      the editor sent, SLOT OWN;
 *   3. the part RENDERS bit for bit what the FM6 engine renders when it loads the same voice (fm6_set_patch) with the same parameters: a converted voice sounds
 *      as the voice, nothing is lost on the way (pack, record, the store, the load).
 *   build/host/dx7_import_test FIXTURE        (DUELL/host_tests.sh "dx7import" makes the fixture with the emsdk node) */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad;
static void ck(const char *what, int ok)
{
    printf("dx7import: %-120s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static uint32_t unhex(const char *s, uint32_t n, uint8_t *out)   /* n hex chars -> bytes */
{
    uint32_t i;
    for (i = 0; i < n / 2u; i++)
        out[i] = (uint8_t)(hexv(s[2u * i]) * 16 + hexv(s[2u * i + 1u]));
    return n / 2u;
}

#define SECS 0.9
#define HOLD 0.6
static void reset_part(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(eng_mem, 0, sizeof eng_mem);
    memset(fm6_eff, 0, sizeof fm6_eff);
    memset(fm6_lfo, 0, sizeof fm6_lfo);
    host_tracks_init();
    host_preset(&trk[0], ENGI_FM6, 0);
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = 0;
    song.sel = 0;
}
static uint32_t render(int32_t *out, uint32_t total)   /* note 60 held HOLD s, 2 s in all; returns the peak */
{
    static int32_t blk[CTL];
    uint32_t i, j, hold = (uint32_t)(HOLD * FS), peak = 0;
    trk_note_on(&trk[0], 60u, 100u);
    for (i = 0; i < total; i += CTL) {
        if (i / CTL == hold / CTL)
            trk_note_off(&trk[0], 60u);
        memset(blk, 0, sizeof blk);
        track_render(&trk[0], blk, CTL);
        for (j = 0; j < CTL && i + j < total; j++) {
            int32_t x = blk[j];
            out[i + j] = x;
            peak = (uint32_t)(x < 0 ? -x : x) > peak ? (uint32_t)(x < 0 ? -x : x) : peak;
        }
    }
    return peak;
}

int main(int argc, char **argv)
{
    static int32_t A[FS], B[FS], C[FS];
    static char line[8192];
    FILE *f;
    uint32_t n = 0, stored = 0, bytes_ok = 0, params_ok = 0, patch_ok = 0, name_ok = 0, sound_ok = 0, silent = 0, differs = 0, total = (uint32_t)(SECS * FS);
    if (argc < 2 || !(f = fopen(argv[1], "r"))) {
        fprintf(stderr, "usage: dx7_import_test FIXTURE\n");
        return 2;
    }
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    frame();
    while (fgets(line, sizeof line, f)) {
        char *tab[6];
        uint8_t pk[FM6_PACKED], rec[PT_WORST + 8], v[FP_SIZE + 1u], want_name[13] = {0};
        int16_t val[P_COUNT];
        uint32_t i, k = 0, slot = n % 128u, reclen, ok;
        char *p = line, *c;
        for (i = 0; i < 6u; i++) {
            tab[i] = p;
            c = strchr(p, i < 5u ? '\t' : '\n');
            if (!c)
                break;
            *c = 0;
            p = c + 1;
        }
        if (i < 5u)
            continue;
        n++;
        unhex(tab[3], 2u * FM6_PACKED, pk);
        reclen = unhex(tab[4], (uint32_t)strlen(tab[4]), rec);
        for (i = 0, p = tab[5]; i < P_COUNT; i++) {
            val[i] = (int16_t)strtol(p, &c, 10);
            p = c + 1;
        }
        (void)k;
        /* 1: the store takes it, holds its bytes */
        ok = up_put_raw(slot, rec, reclen) == 0 && up_used(slot);
        stored += ok;
        {
            const uint8_t *r = pt_raw(slot);
            uint32_t j;
            int same = r && pt_rd16(r) == reclen;
            for (j = 0; same && j < reclen; j++)
                if (j != 2u && j != 3u && r[j] != rec[j])
                    same = 0;
            bytes_ok += same;
        }
        /* 2: the load: parameters and the patch */
        reset_part();
        up_load(slot);
        ok = 1;
        for (i = 0; i < P_COUNT; i++)
            if (!param_kept(i) || i == P_LEVEL)
                ok &= trk[0].p[i] == val[i];
        params_ok += ok;
        fm6_unpack(pk, v);
        {
            uint8_t cmp[FP_SIZE + 1u];
            memcpy(cmp, fm6_patch[0], sizeof cmp);
            patch_ok += !memcmp(cmp, v, FP_SIZE) && trk[0].p[P_E7] == 8;
        }
        {   /* the voice's own name travels in the record's patch (10 characters) and the patch name is the voice's */
            for (i = 0; i < 12u; i++)
                want_name[i] = (uint8_t)pt_raw(slot)[7u + i];
            name_ok += want_name[0] && !strncmp((const char *)want_name, tab[2], 12);
        }
        A[0] = 0;
        {
            uint32_t peak = render(A, total);
            silent += peak == 0;
        }
        /* 3: the engine loading the same voice with the same parameters */
        reset_part();
        for (i = 0; i < P_COUNT; i++)
            trk[0].p[i] = val[i];
        trk[0].eng_req = ENGI_FM6;
        fm6_set_patch(0, v);
        fm6_adopt(0);
        render(B, total);
        sound_ok += !memcmp(A, B, total * sizeof A[0]);
        /* and the test is not blind: the same voice with the engine's plain defaults (no filter) does not render the same */
        reset_part();
        trk[0].eng_req = ENGI_FM6;
        trk[0].p[P_E7] = 8;
        fm6_set_patch(0, v);
        fm6_adopt(0);
        render(C, total);
        differs += memcmp(A, C, total * sizeof A[0]) != 0;
    }
    fclose(f);
    ck("the fixture holds the 63 voices of the synthetic banks (one damaged voice left out by the parser; more with DX7_BANKS)", getenv("DX7_BANKS") ? n >= 63u : n == 63u);
    ck("every converted record is taken by the store (up_put_raw)", stored == n);
    ck("  and held byte for byte (but the slot and revision bytes, which the unit sets)", bytes_ok == n);
    ck("  the record's name is the voice's cleaned name", name_ok == n);
    ck("up_load: every parameter of the editor's neutral FM6 patch is in the part (all but the part's own: PAN, MUTE, the arp ids)", params_ok == n);
    ck("  and the voice is the part's FM6 patch, SLOT OWN (the 155 bytes fm6_unpack gives are the editor's voice)", patch_ok == n);
    ck("the part renders bit for bit what the FM6 engine renders when it loads the same voice with the same parameters (0.9 s, note 60, velocity 100)", sound_ok == n);
    ck("  and every voice sounds (none silent)", silent == 0);
    ck("  the check is not blind: with the engine's plain defaults (no MULTI FILTER) nearly every voice renders differently (the filter is in the converted patch)", differs * 100u >= n * 90u);
    printf("dx7import: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
