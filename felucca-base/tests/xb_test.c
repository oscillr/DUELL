/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The cloud plan's job A (the store), the parts the other tests cannot show:
 *   1. firmware/src/up_xb.h on its own (bytes in, bytes out): xb_pack / xb_unpack (the hand-set B edit list: 32 at most, wide id 127 never, values -64..127, an id once),
 *      the extension block (build, check, read; defaults left out; an unknown tag skipped wherever it sits; every one-byte change and every truncation of a good block
 *      checked under AddressSanitizer: nothing is read past the bytes given, the answer is a block or 0, and a block that is taken reads back what it says);
 *   2. the flash map (storage.c st_sector with the real FL_STORE_OK of hal/fm1_flash.h, cut out of the header by host_tests.sh): the 22 patch chunks' 44 sectors and
 *      OBJ_VELO's pair (0xC6000 / 0xC7000) are inside FL_STORE_OK, all different, no other object's; a velocity record of 128 bytes survives a cut at every byte of a save.
 *   build/host/xb_test            (DUELL/host_tests.sh "xb") */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)   /* (the sources' .pool sections: no Mach-O section names) */
#include "../firmware/src/ext.h"
#include "../firmware/src/up_xb.h"
#include "fl_macros.h"     /* (the FL_* range macros of hal/fm1_flash.h) */

static uint8_t nor[0x100000];
static int fail_bytes = -1;                  /* power loss after any programmed byte */
static uint32_t prog_off[128], prog_n[128], nprog;   /* the order of the programs of a save */
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (nprog < 128u) {
        prog_off[nprog] = off;
        prog_n[nprog++] = n;
    }
    for (i = 0; i < n; i++) {
        if (fail_bytes == 0)
            return -9;
        if (fail_bytes > 0)
            fail_bytes--;
        nor[off + i] &= s[i];
    }
    return 0;
}
#include "../firmware/src/storage.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("xb: %-130s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t rng_s = 12345u;
static uint32_t rnd(void) { rng_s = rng_s * 1664525u + 1013904223u; return rng_s >> 8; }

/* field by field: xb_edit_t has a padding byte (u8 id, i16 value) that memcmp would read (it differs with the compiler and the sanitizer) */
static int xe_eq(const xb_edit_t *a, const xb_edit_t *b, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (a[i].wid != b[i].wid || a[i].val != b[i].val)
            return 0;
    return 1;
}

int main(void)
{
    uint32_t i, j, k, n, ok;
    uint8_t buf[XB_BLOCK_MAX + 16], xv[NX], xv2[NX];
    xb_edit_t e[XB_MAX + 4], e2[XB_MAX + 4];

    /* ---- 1a: the constants */
    ck("constants: XB_MAX 32, NX 16, XB_BLOCK_MAX 102 (L, XV, tag 1 with all 16 slots, tag 2 with 32 edits), XB_VER 1, an edit is a u8 id and an i16 value",
       XB_MAX == 32u && NX == 16u && XB_BLOCK_MAX == 102u && XB_VER == 1u && sizeof(xb_edit_t) == 4u && XW_BASE == 128u && XWID(X_CYSYNC) == 142u);
    ck("defaults: slot 1 (TWIN LINK) is ON and slot 7 (SUB OCTAVE) is stored 1 (-1), CYCLE RISE 10 / FALL 70 (job B's descriptors; 74 / 134), the others 0", xb_def(X_LINK) == 65u && xb_def(X_SUBOCT) == 65u && ({ uint32_t d = 1; for (i = 0; i < NX; i++) d &= i == X_LINK || i == X_SUBOCT || (i == X_CYRISE && xb_def(i) == 74u) || (i == X_CYFALL && xb_def(i) == 134u) || xb_def(i) == 64u; d; }));

    /* ---- 1b: xb_pack / xb_unpack */
    for (i = 0; i < XB_MAX; i++)
        e[i] = (xb_edit_t){(uint8_t)(i < 20u ? i * 5u : 128u + i), (int16_t)(i * 4 - 64)};
    n = xb_pack(e, XB_MAX, buf);
    ok = n == 2u * XB_MAX && xb_unpack(buf, n, e2) == XB_MAX && xe_eq(e, e2, XB_MAX);
    ck("pack / unpack: 32 edits round trip, 2 bytes each (wide id, value + 64)", ok && buf[0] == 0u && buf[1] == 0u && buf[2] == 5u && buf[3] == 4u);
    for (i = 0; i < XB_MAX + 4u; i++)
        e[i] = (xb_edit_t){(uint8_t)i, (int16_t)i};
    ck("  more than XB_MAX edits: only the first 32 are written", xb_pack(e, XB_MAX + 4u, buf) == 2u * XB_MAX);
    e[0] = (xb_edit_t){7, -1000}; e[1] = (xb_edit_t){8, 1000}; e[2] = (xb_edit_t){127, 5}; e[3] = (xb_edit_t){9, 3};
    n = xb_pack(e, 4, buf);
    ck("  a value is held to -64 .. +127, an edit with the reserved wide id 127 is left out", n == 6u && buf[1] == 0u && buf[3] == 191u && buf[4] == 9u && xb_unpack(buf, n, e2) == 3u);
    ck("  an empty list: 0 bytes, and unpack of 0 bytes is 0 edits", xb_pack(e, 0, buf) == 0u && xb_unpack(buf, 0, e2) == 0u);
    {
        uint8_t b[70];
        memset(b, 0, sizeof b);
        for (i = 0; i < 35u; i++) { b[2u * i] = (uint8_t)(i + 1u); b[2u * i + 1u] = 64; }
        ck("unpack refuses (XB_ERR): an odd length, 33 edits, a value byte 192, wide id 127, an id twice",
           xb_unpack(b, 3, e2) == XB_ERR && xb_unpack(b, 66, e2) == XB_ERR && xb_unpack(b, 64, e2) == 32u && (b[5] = 192, xb_unpack(b, 10, e2) == XB_ERR) && (b[5] = 64, b[4] = 127, xb_unpack(b, 10, e2) == XB_ERR) &&
           (b[4] = 1, xb_unpack(b, 10, e2) == XB_ERR) && (b[4] = 3, xb_unpack(b, 10, e2) == 5u));
    }
    {   /* random bytes: whatever unpack takes obeys the rules and packs back to the same bytes */
        uint32_t taken = 0, rules = 1;
        for (k = 0; k < 200000u; k++) {
            uint8_t b[2u * XB_MAX + 4u], c[2u * XB_MAX + 4u];
            uint32_t len = rnd() % (2u * XB_MAX + 3u), got;
            for (i = 0; i < len; i++)
                b[i] = (uint8_t)(rnd() % 4u ? rnd() % 6u * 20u : rnd());       /* (few distinct ids and small values: duplicates are common) */
            if (k & 1u)
                for (i = 0; i + 1u < len; i += 2u)
                    b[i + 1u] = (uint8_t)(rnd() % 192u);
            got = xb_unpack(b, len, e2);
            if (got == XB_ERR)
                continue;
            taken++;
            rules &= got * 2u == len && got <= XB_MAX && xb_pack(e2, got, c) == len && !memcmp(b, c, len);
            for (i = 0; i < got; i++) {
                rules &= e2[i].wid != 127u && e2[i].val >= -64 && e2[i].val <= 127;
                for (j = 0; j < i; j++)
                    rules &= e2[j].wid != e2[i].wid;
            }
        }
        ck("random bytes (200000 lists): what unpack takes is within the rules and packs back to the same bytes", rules && taken > 1000u);
    }

    /* ---- 1c: the block */
    for (i = 0; i < NX; i++)
        xv[i] = xb_def(i);
    ck("every slot at its default, no edit: no block (length 0)", xb_block_build(xv, e, 0, buf) == 0u);
    xv[X_SUBLVL] = 64u + 100u;
    n = xb_block_build(xv, e, 0, buf);
    ck("  one slot off (SUB LEVEL 100): [5][1][1][2][wide id 134][164]: 6 bytes", n == 6u && buf[0] == 5u && buf[1] == 1u && buf[2] == XB_T_EXT && buf[3] == 2u && buf[4] == XWID(X_SUBLVL) && buf[5] == 164u &&
       xb_block_ok(buf, n) == n);
    for (i = 0; i < XB_MAX; i++)
        e[i] = (xb_edit_t){(uint8_t)(i < 10u ? i : 128u + i), (int16_t)(i * 3 - 60)};
    n = xb_block_build(xv, e, 3, buf);
    ck("  and 3 edits: tag 2 follows tag 1 ([13][1][1][2][..][2][6][..]): 14 bytes", n == 14u && buf[0] == 13u && buf[6] == XB_T_EDIT && buf[7] == 6u && xb_block_ok(buf, n) == n);
    memset(xv2, 0, sizeof xv2);
    for (i = 0; i < NX; i++) xv2[i] = xb_def(i);
    ck("  read back: the slot, the three edits, the other slots at their defaults", xb_block_read(buf, xv2, e2) == 3u && xv2[X_SUBLVL] == 164u && xe_eq(e, e2, 3u) &&
       ({ uint32_t d = 1; for (i = 0; i < NX; i++) d &= i == X_SUBLVL || xv2[i] == xb_def(i); d; }));
    for (i = 0; i < NX; i++)
        xv[i] = (uint8_t)(xb_def(i) + 1u + i);
    n = xb_block_build(xv, e, XB_MAX, buf);
    ck("the biggest block: all 16 slots off default and 32 edits is XB_BLOCK_MAX = 102 bytes, taken, read back equal", n == XB_BLOCK_MAX && xb_block_ok(buf, n) == n &&
       ({ for (i = 0; i < NX; i++) xv2[i] = xb_def(i); xb_block_read(buf, xv2, e2) == XB_MAX && !memcmp(xv, xv2, NX) && xe_eq(e, e2, XB_MAX); }));
    {   /* an unknown tag skipped wherever it sits, with any content */
        uint8_t src[XB_BLOCK_MAX], b[XB_BLOCK_MAX + 40];
        uint32_t m = xb_block_build(xv, e, 5, src), cuts[3] = {2u, 2u + 2u + 2u * NX, m}, c, u = 1;      /* before tag 1, between the tags, after tag 2 */
        for (c = 0; c < 3u; c++) {
            uint8_t ins[8] = {77, 5, 1, 2, 3, 4, 5, 0}, unk2[2] = {200, 0};
            uint32_t at = cuts[c], t;
            memcpy(b, src, at);
            memcpy(b + at, ins, 7);
            memcpy(b + at + 7, unk2, 2);
            memcpy(b + at + 9, src + at, m - at);
            b[0] = (uint8_t)(src[0] + 9u);
            for (i = 0; i < NX; i++) xv2[i] = xb_def(i);
            t = xb_block_ok(b, m + 9u) == m + 9u && xb_block_read(b, xv2, e2) == 5u && !memcmp(xv2, xv, NX) && xe_eq(e2, e, 5u);
            u &= t;
        }
        ck("an unknown tag (77 with 5 bytes, 200 empty) before, between or after the known ones: skipped, the block reads the same", u);
    }
    {   /* an id the firmware has no slot for, in tag 1, is skipped */
        uint8_t b[] = {7, XB_VER, XB_T_EXT, 4, 200, 70, XWID(X_LFO1), 71, 0};
        for (i = 0; i < NX; i++) xv2[i] = xb_def(i);
        ck("tag 1 with wide id 200 and 128 + NX: taken, the unknown ids skipped, the known slot read", xb_block_ok(b, 8) == 8u && xb_block_read(b, xv2, e2) == 0u && xv2[X_LFO1] == 71u &&
           (b[4] = XW_BASE + NX, b[6] = XW_BASE + NX - 1u, xb_block_ok(b, 8) == 8u));
    }
    {   /* the damaged ones */
        uint8_t g[XB_BLOCK_MAX], b[XB_BLOCK_MAX + 8];
        uint32_t m = xb_block_build(xv, e, 4, g), r = 1;
        memcpy(b, g, m); b[0]++; r &= xb_block_ok(b, m) == 0u && xb_block_ok(b, m + 1u) == 0u;      /* L one too long: past the bytes given, or a section that is not there */
        r &= xb_block_ok(g, m - 1u) == 0u;                                                                                      /* the record is one byte short of the block */
        memcpy(b, g, m); b[0]--; r &= xb_block_ok(b, m) == 0u;                                                                    /* L short: the last section runs past */
        memcpy(b, g, m); b[1] = 0; r &= xb_block_ok(b, m) == 0u;
        memcpy(b, g, m); b[1] = 2; r &= xb_block_ok(b, m) == 0u;
        memcpy(b, g, m); b[0] = 0; r &= xb_block_ok(b, m) == 0u;
        r &= xb_block_ok(g, 0) == 0u && xb_block_ok(g, 1) == 0u;
        ck("a block with L short or long, XV 0 or 2, L 0, or fewer bytes than L says: refused (0)", r);
    }
    {   /* every one-byte change and every truncation of good blocks: ASAN-checked reads, a block or 0, and what is taken reads back */
        uint32_t t, tot = 0, taken = 0, safe = 1;
        for (t = 0; t < 4u; t++) {
            uint8_t src[XB_BLOCK_MAX];
            uint32_t m = xb_block_build(xv, e, t == 0u ? 0u : t == 1u ? 1u : t == 2u ? 7u : XB_MAX, src);
            for (i = 0; i < m; i++) {
                uint32_t v;
                for (v = 0; v < 256u; v++) {
                    uint8_t *b = malloc(m);
                    uint32_t r;
                    memcpy(b, src, m);
                    b[i] = (uint8_t)v;
                    r = xb_block_ok(b, m);                           /* (ASAN: a read past b[m - 1] is a report and an abort) */
                    tot++;
                    if (r) {
                        xb_edit_t ee[XB_MAX];
                        uint8_t x2[NX];
                        taken++;
                        safe &= r <= m;
                        xb_defaults(x2);
                        safe &= xb_block_read(b, x2, ee) <= XB_MAX;
                    }
                    free(b);
                }
            }
            for (i = 0; i <= m; i++) {
                uint8_t *b = malloc(i ? i : 1u);
                memcpy(b, src, i);
                safe &= xb_block_ok(b, i) == 0u || i == m;
                free(b);
            }
        }
        printf("xb: %u one-byte changes of 4 good blocks tried, %u of them are still a block\n", tot, taken);
        ck("every one-byte change and every truncation of good blocks: no read past the end (AddressSanitizer), a block or 0, a block that is taken reads back", safe && tot > 2000u);
    }
    {   /* random byte strings of the right size: never past the end, and whatever is taken is a well-formed block */
        uint32_t taken = 0, safe = 1;
        for (k = 0; k < 300000u; k++) {
            uint32_t len = 2u + rnd() % 109u, r;
            uint8_t *b = malloc(len);
            for (i = 0; i < len; i++)
                b[i] = (uint8_t)(rnd() % 3u == 0 ? rnd() : (i == 1u ? 1u : rnd() % 4u));
            if (k & 1u) { b[0] = (uint8_t)(len - 1u); b[1] = XB_VER; }
            r = xb_block_ok(b, len);
            if (r) {
                xb_edit_t ee[XB_MAX];
                uint8_t x2[NX];
                taken++;
                safe &= r == 1u + b[0] && r <= len;
                xb_defaults(x2);
                (void)xb_block_read(b, x2, ee);
            }
            free(b);
        }
        ck("random strings (300000): no read past the end; whatever is taken has L + 1 bytes within the string", safe && taken > 100u);
    }

    /* ---- 2: the flash map */
    {
        uint32_t o, o2, c, sec_ok = 1, distinct = 1, other = 0, patch_ok = 1, nobj = 0;
        for (o = 0; o < OBJ_COUNT; o++) {
            if (OBJ_RETIRED(o))
                continue;
            nobj++;
            for (c = 0; c < 2u; c++) {
                sec_ok &= FL_STORE_OK(st_sector(o, c), 4096u) != 0;
                for (o2 = o; o2 < OBJ_COUNT; o2++) {
                    uint32_t c2;
                    if (OBJ_RETIRED(o2))
                        continue;
                    for (c2 = 0; c2 < 2u; c2++)
                        if ((o2 != o || c2 != c) && st_sector(o2, c2) == st_sector(o, c)) {
                            distinct = 0;
                        }
                }
            }
        }
        ck("OBJ_COUNT is 50 (ids 43..48 the patch chunks 16..21, 49 OBJ_VELO), and every non-retired object's two sectors are inside FL_STORE_OK and all different", OBJ_COUNT == 50u && OBJ_VELO == 49u &&
           OBJ_PATCHX0 + 16 == OBJ_VELO && sec_ok && distinct);
        for (c = 16; c < 22u; c++) {
            uint32_t ob = OBJ_PATCHX0 + (c - 6u);
            patch_ok &= st_sector(ob, 0) == 0xBA000u + (c - 16u) * 0x2000u && st_sector(ob, 1) == st_sector(ob, 0) + 0x1000u && ob == 43u + (c - 16u);
        }
        ck("  chunks 16..21 = objects 43..48 at 0xBA000 .. 0xC5FFF (the formula continues in the same stride), OBJ_VELO 0xC6000 / 0xC7000", patch_ok && st_sector(OBJ_VELO, 0) == 0xC6000u && st_sector(OBJ_VELO, 1) == 0xC7000u);
        ck("  chunk 21's B sector ends at 0xC6000, OBJ_VELO's pair at 0xC8000 where OBJ_PATCH0 starts: the whole 0xA6000 .. 0xC7FFF is used, and all of it is inside FL_STORE_OK",
           st_sector(OBJ_PATCHX0 + 15, 1) + 4096u == 0xC6000u && st_sector(OBJ_VELO, 1) + 4096u == st_sector(OBJ_PATCH0, 0) && FL_STORE_OK(0xA6000u, 0x22000u));
        ck("  outside every object and outside FL_STORE_OK (what erase_test plants a pattern in): 0xEA000 and 0xF0000", !FL_STORE_OK(0xEA000u, 4096u) && !FL_STORE_OK(0xF0000u, 4096u) && ({ uint32_t hit = 0;
            for (o = 0; o < OBJ_COUNT; o++) for (c = 0; c < 2u; c++) if (!OBJ_RETIRED(o) && (st_sector(o, c) == 0xEA000u || st_sector(o, c) == 0xF0000u)) hit++; !hit; }));
        printf("xb: %u objects in the map (retired ones left out)\n", nobj);
        /* a velocity record of 128 bytes: saved, loaded, torn at every byte */
        {
            uint8_t a[128], b2[128], got[160];
            uint32_t cut, torn_ok = 1, tried = 0;
            memset(nor, 0xFF, sizeof nor);
            for (i = 0; i < 128u; i++) { a[i] = (uint8_t)(i * 7u + 3u); b2[i] = (uint8_t)(255u - i); }
            ck("OBJ_VELO: 128 bytes saved and loaded (128 returned, the same bytes)", st_save(OBJ_VELO, a, 128) == 0 && st_load(OBJ_VELO, got, sizeof got) == 128 && !memcmp(got, a, 128));
            for (cut = 0; cut < 128u + 32u + 4u; cut++) {
                memset(nor, 0xFF, sizeof nor);
                st_save(OBJ_VELO, a, 128);
                fail_bytes = (int)cut;
                st_save(OBJ_VELO, b2, 128);
                fail_bytes = -1;
                tried++;
                if (st_load(OBJ_VELO, got, sizeof got) != 128 || (memcmp(got, a, 128) && memcmp(got, b2, 128)))
                    torn_ok = 0;
                if (cut + 8u < 128u + 32u && memcmp(got, a, 128))
                    torn_ok = 0;                                      /* (before the commit record is written the old record is in charge) */
            }
            {   /* the commit record is the LAST thing a save programs, whatever is in the payload (the safety rule behind every store: patch chunks, multis, velocity) */
                uint8_t big[3000];
                uint32_t obs[3] = {OBJ_VELO, OBJ_PATCHX0 + 15, OBJ_PATCH0}, lens[3] = {128u, 3000u, 3584u - 4u}, q, last_ok = 1;
                memset(big, 0x5A, sizeof big);
                for (q = 0; q < 3u; q++) {
                    uint32_t base = st_sector(obs[q], 0), p;
                    memset(nor, 0xFF, sizeof nor);
                    nprog = 0;
                    st_save_to(obs[q], big, lens[q] > sizeof big ? sizeof big : lens[q], 0);
                    last_ok &= nprog >= 2u && prog_off[nprog - 1u] == base && prog_n[nprog - 1u] == 32u;      /* the 32-byte header at the sector's start, last */
                    for (p = 0; p + 1u < nprog; p++)
                        last_ok &= prog_off[p] >= base + ST_PAYLOAD_OFF && prog_off[p] < base + 4096u;          /* everything before it is payload */
                }
                nprog = 0;
                ck("a save programs the payload first and the 32-byte commit record LAST (velocity record, a patch chunk, a full sector): the order that makes a cut harmless", last_ok);
            }
            ck("  a power cut at every byte of a save: the old record or the new one, never anything else (the old one until the commit record is written)", torn_ok && tried > 100u);
            fail_bytes = -1;
        }
    }

    printf(bad ? "xb: FAILED (%d)\n" : "xb: all ok\n", bad);
    return bad ? 1 : 0;
}
