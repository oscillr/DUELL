/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Fuzz harness: random editor SysEx frames (with and without the header, any command, 0..1100 bytes, sometimes no
 * terminator) and raw USB-MIDI packets through the real usb.c / editor.c paths of the host build. run_tests.sh runs
 * it briefly under ASan / UBSan with a fixed seed.   fuzz_ed [ITERATIONS [SEED [ONLY_PATCH_SESSIONS]]] */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"                                /* hostsim, the stubs, RAM flash, reset() */
static uint32_t rs = 0x9E3779B9u;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static void fz_feed(const uint8_t *p, uint32_t n)
{
    uint32_t o, tries;
    for (o = 0; o < n; o += 64u) {
        for (tries = 0; tries < 50u && !ep1_take(p + o, n - o > 64u ? 64u : n - o); tries++) {
            fm1_ms += 2u; ed_service(); host_drain();
        }
    }
}
/* PATCH_RAW (84) and the backup's patch records (65..67, object 40) with structured, usually plausible records whose extension block / edit list is random or damaged
 * (the cloud plan's job A): a record encoded by the store from a random up_rec_t (slots, 0..32 edits, duplicates and wide id 127 included), then bytes flipped,
 * the length byte and the block's length byte nudged; sent as BEGIN / DATA / COMMIT with the right CRC (so the store's own checks, pt_rec_ok / xb_block_ok, decide). */
static void fz_cmd(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    static uint8_t f[400], u[800];
    uint32_t n = 0, j;
    f[n++] = 0xF0; f[n++] = ED_HDR0; f[n++] = ED_HDR1; f[n++] = ED_HDR2; f[n++] = (uint8_t)cmd;
    for (j = 0; j < na && n < sizeof f - 2u; j++)
        f[n++] = a[j] & 0x7Fu;
    f[n++] = 0xF7;
    fz_feed(u, usb_frame(f, n, u));
    fm1_ms += 1u + rnd() % 8u;
    ed_service(); host_drain();
}
static uint32_t fz_pack7(const uint8_t *p, uint32_t n, uint8_t *out)
{
    uint32_t k = 0;
    while (n) {
        uint32_t g = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < g; i++) m |= (uint32_t)(p[i] >> 7) << i;
        out[k++] = (uint8_t)m;
        for (i = 0; i < g; i++) out[k++] = p[i] & 127u;
        p += g; n -= g;
    }
    return k;
}
static void fz_u32(uint8_t *a, uint32_t v) { uint32_t i; for (i = 0; i < 5u; i++) a[i] = (uint8_t)((v >> (7u * i)) & 127u); }
static void fz_patch(uint32_t *sent)
{
    static uint8_t rec[PT_WORST + 40], a[300];
    up_rec_t r;
    uint32_t i, n, slot = rnd() % UP_SLOTS, k, via_backup = rnd() & 1u, off, crc;
    memset(&r, 0, sizeof r);
    r.used = UP_USED; r.ver = UP_VER; r.engine = (uint8_t)(rnd() % NENGINES); r.np = P_COUNT;
    memcpy(r.name, "FUZZ", 4);
    for (i = 0; i < P_COUNT; i++)
        up_set_value(&r, i, i == P_LEVEL ? 104 : (int16_t)up_def(r.engine, i));
    for (i = 0; i < NX; i++)
        r.xz[i] = rnd() % 3u ? 0u : (uint8_t)rnd();                 /* (any byte: up_xval holds the value inside -64 .. 127 + the XOR) */
    r.xb_n = (uint8_t)(rnd() % 4u == 0 ? 0u : rnd() % (XB_MAX + 1u));
    for (i = 0; i < r.xb_n; i++)
        r.xb[i] = (xb_edit_t){(uint8_t)(rnd() % 8u == 0 ? 127u : rnd() % 6u == 0 ? 128u + rnd() % 128u : rnd() % 127u), (int16_t)((int)(rnd() % 256u) - 90)};
    n = pt_encode(slot, 0, &r, 0);
    memcpy(rec, pt_enc, n);
    rec[2] = rec[3] = 0;
    k = rnd() % 4u;                                                 /* damage: 0 none, 1 flipped bytes, 2 the block's L, 3 the record's length field */
    if (k == 1u)
        for (i = 1u + rnd() % 4u; i; i--)
            rec[rnd() % n] ^= (uint8_t)(1u << (rnd() % 8u));
    else if (k == 2u && (rec[4] & PTF_XB))
        rec[PT_FIX] = (uint8_t)(rec[PT_FIX] + (rnd() & 1u ? 1u : 255u));
    else if (k == 3u)
        n = n > PT_FIX + 1u && rnd() & 1u ? n - 1u : n + (n < PT_WORST ? 1u : 0u);
    crc = st_crc32(rec, n);
    if (via_backup) {
        /* (the snapshot first: LIST) */
        fz_cmd(ED_BACKUP_LIST, a, 0);
        a[0] = (uint8_t)(1u + rnd() % 4u); fz_cmd(ED_BACKUP_LIST, a, 1);
        a[0] = 0; a[1] = 40; fz_u32(a + 2, n); fz_u32(a + 7, crc); a[12] = (uint8_t)slot; fz_cmd(ED_BACKUP_PUT, a, 13);
        for (off = 0; off < n; off += 256u) {
            uint32_t g = n - off > 256u ? 256u : n - off;
            a[0] = 1; a[1] = 40; fz_u32(a + 2, off);
            fz_cmd(ED_BACKUP_PUT, a, 7u + fz_pack7(rec + off, g, a + 7));
        }
        a[0] = 2; a[1] = 40; fz_cmd(ED_BACKUP_PUT, a, 2);
        a[0] = 40; fz_u32(a + 1, 0); a[6] = (uint8_t)(rnd() % 128u); a[7] = (uint8_t)(rnd() % 4u); a[8] = (uint8_t)slot; fz_cmd(ED_BACKUP_GET, a, 9);
    } else {
        a[0] = 2; a[1] = (uint8_t)slot; a[2] = (uint8_t)(n & 127u); a[3] = (uint8_t)(n >> 7); fz_u32(a + 4, crc); fz_cmd(ED_PATCH_RAW, a, 9);
        for (off = 0; off < n; off += 112u) {
            uint32_t g = n - off > 112u ? 112u : n - off;
            a[0] = 3; a[1] = (uint8_t)slot; a[2] = (uint8_t)(off & 127u); a[3] = (uint8_t)(off >> 7);
            fz_cmd(ED_PATCH_RAW, a, 4u + fz_pack7(rec + off, g, a + 4));
        }
        a[0] = 4; a[1] = (uint8_t)slot; fz_cmd(ED_PATCH_RAW, a, 2);
        a[0] = 1; a[1] = (uint8_t)slot; a[2] = 0; a[3] = 0; a[4] = (uint8_t)(1u + rnd() % 112u); fz_cmd(ED_PATCH_RAW, a, 5);
    }
    (*sent)++;
}
int main(int argc, char **argv)
{
    static uint8_t f[1300], u[2600];
    uint32_t iters = argc > 1 ? (uint32_t)atoi(argv[1]) : 200000u, i, n, L, j, cmd, raw = 0, frames = 0, patches = 0;
    uint32_t only = argc > 3 ? (uint32_t)atoi(argv[3]) : 0u;
    if (argc > 2) rs = (uint32_t)atoi(argv[2]) | 1u;
    reset();
    usb.rx_pend = 0;
    for (i = 0; i < iters; i++) {
        uint32_t mode = rnd() % 16u;
        if (only || rnd() % 40u == 0) {                     /* a structured patch record session (see fz_patch); `fuzz_ed N SEED 1` sends nothing else */
            fz_patch(&patches);
            continue;
        }
        if (mode == 0) {                                    /* raw random USB-MIDI event packets */
            uint32_t k = 4u * (1u + rnd() % 16u);
            for (j = 0; j < k; j++) u[j] = (uint8_t)rnd();
            ep1_take(u, k); raw++;
        } else {
            n = 0; f[n++] = 0xF0;
            if (mode == 1) { f[n++] = (uint8_t)(rnd() & 0x7F); f[n++] = (uint8_t)(rnd() & 0x7F); f[n++] = (uint8_t)(rnd() & 0x7F); }
            else { f[n++] = ED_HDR0; f[n++] = ED_HDR1; f[n++] = ED_HDR2; }
            cmd = mode < 4u ? (rnd() & 0x7F) : 1u + rnd() % 70u;
            f[n++] = (uint8_t)cmd;
            L = rnd() % 4u == 0 ? rnd() % 1100u : rnd() % 40u;
            for (j = 0; j < L; j++) {
                uint32_t r = rnd();
                f[n++] = (uint8_t)(r % 8u == 0 ? 0x7F : r % 8u == 1 ? 0 : r % 8u == 2 ? 1 : (r >> 8) & 0x7F);
            }
            if (rnd() % 32u) f[n++] = 0xF7;                 /* sometimes no terminator */
            fz_feed(u, usb_frame(f, n, u)); frames++;
        }
        fm1_ms += 1u + rnd() % 40u;
        ed_service(); host_drain();
        if (rnd() % 64u == 0) { host_wire_n = 0; ed_sync(); host_drain(); }

    }
    printf("fuzz ok: %u frames, %u raw packets, %u patch record sessions (%u patches stored at the end)\n", frames, raw, patches, up_count());
    return 0;
}
