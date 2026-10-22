/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.24: a synthetic PATCHES export, made by the firmware's own code (tests/owner_import_test.py, tools/import_owner_edits.py).
 * Every factory sound (browse position n = patch slot n) is loaded on part 1 as the PATCHES list loads it (set_engine_of + apply_preset_to),
 * saved with SAVE's own function (up_patch_save_part: the values, End B, the arp block) and read back as the editor's PATCH_RAW reads it
 * (up_raw_get): the records are what a unit holds after the owner saved the slot. Written as the editor's "duell-patches" file (web/editor/patchfile.js:
 * number, name, engine, category, saved, crc32, record in base64), so the editor's own parser reads it too.
 *   build/host/owner_export_gen OUT.json [SLOT PARAM_ID VALUE ...]   (SLOT 1..128; the edit is applied to the loaded sound before it is saved: "the owner turned a knob")
 *   a SLOT with PARAM_ID -1 and VALUE n renames nothing and skips nothing: reserved. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static uint32_t crc32_of(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, i, k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8u; k++) c = c >> 1 ^ (c & 1u ? 0xEDB88320u : 0u);
    }
    return ~c;
}
static void b64(FILE *f, const uint8_t *p, uint32_t n)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint32_t i;
    for (i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16 | (i + 1 < n ? (uint32_t)p[i + 1] << 8 : 0u) | (i + 2 < n ? p[i + 2] : 0u);
        fputc(T[v >> 18 & 63], f); fputc(T[v >> 12 & 63], f);
        fputc(i + 1 < n ? T[v >> 6 & 63] : '=', f); fputc(i + 2 < n ? T[v & 63] : '=', f);
    }
}

int main(int argc, char **argv)
{
    uint32_t n, nlist, first = 1;
    FILE *f;
    if (argc < 2)
        return fprintf(stderr, "usage: owner_export_gen OUT.json [SLOT ID VALUE ...]\n"), 2;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    f = fopen(argv[1], "w");
    if (!f)
        return perror(argv[1]), 1;
    fprintf(f, "{\"format\":\"duell-patches\",\"version\":1,\"app\":\"owner_export_gen\",\"firmware\":\"x\",\"source\":\"synthetic\",\"patches\":[\n");
    nlist = ds_list_n();
    for (n = 0; n < nlist && n < UP_SLOTS; n++) {
        uint32_t k, e = ds_list_at(n, &k), a;
        uint8_t raw[PT_WORST + 16];
        uint32_t len;
        track_t *t = &trk[0];
        char nm[16];
        set_engine_of(t, e);
        apply_preset_to(t, k);
        t->engine = t->eng_req;
        t->user = 0;
        for (a = 2; a + 2 < (uint32_t)argc; a += 3)
            if ((uint32_t)atoi(argv[a]) == n + 1u)
                t->p[atoi(argv[a + 1])] = (int16_t)atoi(argv[a + 2]);
        if (up_patch_save_part(0, n)) {
            fprintf(stderr, "slot %u not saved\n", n + 1u);
            return 1;
        }
        len = up_raw_get(n, raw);
        if (!len) {
            fprintf(stderr, "slot %u: no record\n", n + 1u);
            return 1;
        }
        str_cpy(nm, ds_name(e, k), sizeof nm);
        fprintf(f, "%s{\"number\":%u,\"name\":\"%s\",\"engine\":\"%s\",\"category\":\"-\",\"saved\":true,\"crc32\":\"%08x\",\"record\":\"", first ? "" : ",\n",
                n + 1u, nm, ENGINES[e]->name, crc32_of(raw, len));
        b64(f, raw, len);
        fprintf(f, "\"}");
        first = 0;
    }
    fprintf(f, "\n]}\n");
    fclose(f);
    printf("owner_export_gen: %u records -> %s\n", n, argv[1]);
    return 0;
}
