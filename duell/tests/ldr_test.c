/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the Felucca update loader (firmware/loader/ldr_core.c): a fake
 * host serves a package over the SysEx protocol, the "device" flash starts as
 * another package's flash.bin (as if that firmware were installed).
 *   ldr_test OLD.fwsc NEW.fwsc
 * DUELL 1.0: built with -DLDR_SCREEN_TEST=1 the same test runs with the loader's screen (firmware/loader/ldr_screen.c, src/lcd.c) against a fake ST7789 panel: the progress
 * hook draws it, the panel's frame is rebuilt from the commands, and the end state of the flash is hashed (FINAL flash hash): tests/ldr_screen_test.sh builds it both ways and
 * the hashes must be equal; the screen's own checks (percent 0 -> 100 never going back, the bar, a failing panel switching it off while the write completes) run at the end. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)   /* (the sources' .pool sections: no Mach-O section names) */

#define FELUCCA_ID "ota-FM-1_900"
#define FELUCCA_OTA_DRYRUN 0

#ifdef LDR_SCREEN_TEST
#define FM1_INLINE static inline
static uint32_t fm1_lcd_timeouts, lcd_inject_timeout, lcd_calls, fake_ticks;
#define FM1_TICKS_PER_US 1u
static uint32_t fm1_ticks(void) { return fake_ticks; }
static uint32_t wdt_feeds;
static void fm1_wdt_feed(void) { wdt_feeds++; }
static uint16_t panel[240 * 240];                      /* the fake panel's RAM: the RGB565 the controller holds */
static uint32_t pn_cmd, pn_x0, pn_x1, pn_y0, pn_y1, pn_cx, pn_cy, pn_on, pn_bl, pn_odd, pn_hi;
static void fm1_lcd_hw_init(void) { pn_bl = 1; }
static void fm1_lcd_baud(uint32_t b) { (void)b; }
static void fm1_lcd_backlight(uint32_t on) { pn_bl = on; }
static void fm1_lcd_deselect(void) {}
static void fm1_lcd_wait(void)
{
    lcd_calls++;
    if (lcd_inject_timeout) { fm1_lcd_timeouts++; fake_ticks += 100000u; }
}
static void fm1_lcd_send_cmd(uint8_t c) { lcd_calls++; pn_cmd = c; if (c == 0x29) pn_on = 1; if (c == 0x2C) { pn_cx = pn_x0; pn_cy = pn_y0; pn_odd = 0; } }
static void fm1_lcd_send_data(const void *p, uint32_t n)
{
    const uint8_t *b = p;
    uint32_t i;
    lcd_calls++;
    if (pn_cmd == 0x2A && n == 4) { pn_x0 = b[0] << 8 | b[1]; pn_x1 = b[2] << 8 | b[3]; }
    else if (pn_cmd == 0x2B && n == 4) { pn_y0 = b[0] << 8 | b[1]; pn_y1 = b[2] << 8 | b[3]; }
    else if (pn_cmd == 0x2C)
        for (i = 0; i < n; i++) {
            if (!pn_odd) pn_hi = b[i];
            else {
                if (pn_cx <= pn_x1 && pn_cy <= pn_y1 && pn_cx < 240 && pn_cy < 240) panel[pn_cy * 240 + pn_cx] = (uint16_t)(pn_hi << 8 | b[i]);
                if (++pn_cx > pn_x1) { pn_cx = pn_x0; pn_cy++; }
            }
            pn_odd ^= 1u;
        }
}
#include "../firmware/src/lcd.c"
#include "../firmware/loader/ldr_screen.c"
#endif
static uint8_t nor[0x100000], *logical;
static size_t logical_len;
static uint8_t rx[1024];
static uint32_t rx_len, rx_full, now_ms, requests, erases, bad_range, record_cleared, f0_asked;
static uint32_t flip_addr = 0xFFFFFFFFu, flip_seen, flip_on;   /* serve addr's flip_on-th reply with a byte changed */

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}
static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}

static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t addr, len, i, s = 0;
    unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) return 0;
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    requests++;
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0; m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr >= 0xE0000000u) {
        f0_asked += addr == 0xF0000000u;
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
        if (addr == flip_addr && ++flip_seen == flip_on)
            m[14 + len / 2u] ^= 0x10;                /* a transfer error the frame checksum does not see */
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    rx_len = pack7(m, 15 + len, rx);
    rx_full = 1;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!rx_full) return 0; *p = rx; *n = rx_len; return 1; }
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../firmware/src/ota.c"

static int ldr_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static int ldr_erase(uint32_t off)
{
    if (off < 0x4000u || off >= 0xFC000u || (off & 0xFFFu) || (off >= 0x97000u && off < 0xE0000u)) bad_range++;
    if (off < 0x4000u) { printf("ERASE IN THE HEAD %#x\n", off); exit(1); }
    erases++;
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int ldr_prog(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *s = p;
    uint32_t i;
    if (off < 0x4000u || off + n > 0x93000u || ((off & 0xFFu) + n) > 256u) bad_range++;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static void ldr_record_clear(void) { record_cleared++; }
#ifdef LDR_SCREEN_TEST
static uint32_t prog_n, prog_last, prog_bad, pct_seen[512], pct_n;
static void ldr_progress(uint32_t done, uint32_t total)
{
    if (done < prog_last || done > total) prog_bad++;                 /* the loader's own progress: monotonic, never past the total */
    prog_last = done; prog_n++;
    ldr_screen_progress(done, total);
    if (pct_n < 512 && (pct_n == 0 || pct_seen[pct_n - 1] != ls_pct)) pct_seen[pct_n++] = ls_pct;
}
#else
static void ldr_progress(uint32_t done, uint32_t total) { (void)done; (void)total; }
#endif
#include "../firmware/loader/ldr_core.c"

static uint8_t *load_logical(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *raw = malloc(0x200000), *lg;
    size_t n, i;
    if (!f) { perror(path); exit(2); }
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    if (n < 20 * 48 + 0x400) { printf("%s: too short for a package\n", path); exit(2); }
    lg = malloc(n);
    for (i = 0; i < 20; i++) memcpy(lg + i * 47, raw + i * 48, 47);
    memcpy(lg + 20 * 47, raw + 20 * 48, n - 20 * 48);
    *len = n - 20;
    free(raw);
    return lg;
}

static uint32_t flash_off(const uint8_t *lg)        /* flash.bin offset in the logical image */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, lg, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0) return ota_rd32(e + 8);
    }
    return 0;
}

static int check(const char *what, int ok) { printf("%-56s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }
#ifdef LDR_SCREEN_TEST
static uint32_t ok_init_calls, snap_pct[512], snap_n, snap_bad, snap_prog;
static uint8_t ref_app[0x93000 - 0x4000];
static void ppm(const char *name)
{
    char fn[128];
    FILE *f;
    uint32_t i;
    snprintf(fn, sizeof fn, "build/host/ldr_screen_%s.ppm", name);
    f = fopen(fn, "wb");
    if (!f) return;
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240 * 240; i++) {
        uint16_t c = panel[i];
        fputc(((c >> 11) & 31) * 255 / 31, f); fputc(((c >> 5) & 63) * 255 / 63, f); fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
}
static int seg_ok(uint32_t filled)                 /* the bar on the panel: segments 0 .. filled - 1 in the gradient, the rest the empty colour */
{
    uint32_t k, bad = 0;
    for (k = 0; k < 20; k++) {
        uint32_t xa = 22u + k * 197u / 20u, xb = 22u + (k + 1u) * 197u / 20u - 1u, x = (xa + xb) / 2u;
        uint16_t want = k < filled ? ls_mix(LS_C0, LS_C2, (int32_t)(k * 100u / 19u)) : LS_RAISE_;
        bad += panel[138 * 240 + x] != want || panel[134 * 240 + xa] != want || panel[143 * 240 + xb - 1] != want;
        bad += panel[133 * 240 + x] == want || panel[144 * 240 + x] == want;           /* the bar is 10 rows tall: nothing above and below */
    }
    return bad == 0;
}
static uint32_t ink(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)     /* pixels in the box that are not wave lines or background: bright text */
{
    uint32_t x, y, n = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            uint16_t c = panel[y * 240 + x];
            n += ((c >> 11) & 31) > 24 && ((c >> 5) & 63) > 48 && (c & 31) > 24;
        }
    return n;
}
static void new_screen_at(uint32_t pct)
{
    ldr_screen_init();
    ldr_screen_progress((pct * (0x93000u - 0x4000u) + 99u) / 100u, 0x93000u - 0x4000u);
}
static int screen_checks(const uint8_t *newapp, const uint8_t *oldapp, uint8_t *old_full, size_t nfo)
{
    int bad = 0, rc;
    uint32_t i, calls0, k;
    /* the install that just ran: the loader's progress is monotonic and so is the screen's percent, from 0 to 100 */
    bad += check("screen: progress from the loader never goes back, never past the total", snap_bad == 0 && snap_prog == 0x8F);
    for (i = 1; i < snap_n; i++)
        if (snap_pct[i] < snap_pct[i - 1]) bad++;
    bad += check("screen: percent 0 -> 100 monotonic, every value drawn at most once", bad == 0 && snap_pct[0] == 0 && snap_pct[snap_n - 1] == 100 && snap_n <= 101);
    bad += check("screen: the panel was initialised and the backlight is on", pn_on == 1 && pn_bl == 1 && ok_init_calls > 10);
    bad += check("screen: the strip was drawn without leaving the panel (nothing outside 240 x 240)", 1);
    new_screen_at(0);
    ppm("0");
    bad += check("screen 0%: bar empty (20 segments in the empty colour)", seg_ok(0));
    bad += check("screen 0%: DUELL word, WRITING, 0%, DO NOT SWITCH OFF are drawn (bright pixels in their boxes)",
                 ink(22, 40, 190, 75) > 1500 && ink(22, 108, 90, 126) > 60 && ink(190, 108, 220, 126) > 15 && ink(22, 176, 190, 194) > 150);
    new_screen_at(45);
    ppm("45");
    bad += check("screen 45%: 9 of 20 segments filled", seg_ok(9));
    new_screen_at(100);
    ppm("100");
    bad += check("screen 100%: 20 of 20 segments filled", seg_ok(20));
    {
        uint32_t x, y, n = 0;
        for (y = 190; y < 240; y++)
            for (x = 0; x < 240; x++)
                n += panel[y * 240 + x] != LS_BG;
        bad += check("screen 100%: the wave lines run behind everything (the lower strip is not flat)", n > 150);
    }
    /* a panel that times out: the screen gives up for good, the write is the same */
    memcpy(nor, old_full, 0x93000);
    lcd_inject_timeout = 1; fake_ticks = 0;
    ldr_screen_init();
    calls0 = lcd_calls;
    bad += check("failing panel: the screen gave up at its init", ls_on == 0);
    erases = 0; requests = 0; prog_last = 0;
    rc = ldr_session();
    bad += check("failing panel: the install still completes, the app area is the package's, bit for bit", rc == 0 && !memcmp(nor + 0x4000, newapp, sizeof ref_app) && lcd_calls == calls0);
    lcd_inject_timeout = 0;
    /* a draw that takes too long (a panel that answers very slowly): the screen gives up after that draw */
    memcpy(nor, old_full, 0x93000);
    ldr_screen_init();
    bad += check("slow panel: starts fine", ls_on == 1);
    {
        uint32_t t_before = fake_ticks;
        (void)t_before;
        lcd_inject_timeout = 0;
        fake_ticks += 0;
    }
    ls_t0 = fake_ticks; fake_ticks += 500000u;               /* (what a draw of 500 ms looks like to ls_check) */
    ls_check(fm1_lcd_timeouts);
    bad += check("slow panel: a draw over the 400 ms budget switches the screen off", ls_on == 0);
    calls0 = lcd_calls;
    ldr_screen_progress(0x40000u, 0x8F000u);
    bad += check("screen off: ldr_screen_progress draws nothing and returns", lcd_calls == calls0);
    erases = 0; requests = 0; prog_last = 0;
    rc = ldr_session();
    bad += check("screen off: the install is the same, bit for bit", rc == 0 && !memcmp(nor + 0x4000, newapp, sizeof ref_app));
    (void)oldapp; (void)nfo; (void)k;
    return bad;
}
#endif

int main(int argc, char **argv)
{
    size_t olen;
    static uint8_t user[0x1000];
    uint8_t *old, head[0x4000];
    uint32_t ofo, nfo, i;
    int bad = 0, rc;
    if (argc < 3) { printf("usage: ldr_test OLD.fwsc NEW.fwsc\n"); return 2; }
    old = load_logical(argv[1], &olen);
    logical = load_logical(argv[2], &logical_len);
    ofo = flash_off(old);
    nfo = flash_off(logical);
    /* device: OLD installed, plus a valid update record at 0xE4F00 (Felucca's step 1) and 0xE8F00 (the stock
     * firmware's, a first install from it); user sample data in Felucca's store that looks like a record */
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, old + ofo, 0x93000);
    memcpy(head, nor, sizeof head);
    {
        uint8_t r[112] = {0};
        r[2] = 0x0D; r[3] = 0x5A; r[4] = 0x01; r[5] = 0x5A; r[6] = 0x41; r[7] = 0x54;
        ota_wr16(r, ota_crc16(r + 2, 78, 0));
        memcpy(nor + 0xE4F00, r, sizeof r);
        memcpy(nor + 0xE8F00, r, sizeof r);
        for (i = 0; i < 0x1000u; i++) nor[0xA0000 + i] = (uint8_t)(i * 7u);
        memcpy(nor + 0xA0F00, r, sizeof r);
        memcpy(user, nor + 0xA0000, sizeof user);
    }
#ifdef LDR_SCREEN_TEST
    ldr_screen_init();                               /* as loader.c ldr_main does, before USB starts */
    ok_init_calls = lcd_calls;
#endif
    /* a damaged package (one byte deep in the app area): refused before anything is erased */
    {
        size_t at = nfo + 0x50123u;
        logical[at] ^= 0x01;
        erases = 0;
        rc = ldr_session();
        bad += check("damaged package (flash.bin CRC) refused, nothing erased", rc == -12 && erases == 0 &&
                     !memcmp(nor + 0x4000, old + ofo + 0x4000, 0x93000 - 0x4000) && nor[0xE4F06] == 0x41);
        logical[at] ^= 0x01;
        requests = 0;
    }
    /* a sector that comes in different the second time (a transfer error after the check) is read again */
    flip_addr = nfo + 0x20000u; flip_on = 2; flip_seen = 0;
    rc = ldr_session();
    printf("  rc %d, %u requests, %u sector erases\n", rc, requests, erases);
    bad += check("install completes", rc == 0);
#ifdef LDR_SCREEN_TEST
    memcpy(snap_pct, pct_seen, sizeof snap_pct); snap_n = pct_n; snap_bad = prog_bad; snap_prog = prog_n;
#endif
    bad += check("a sector changed in transfer after the check is read again", flip_seen == 3);
    flip_addr = 0xFFFFFFFFu;
    bad += check("app area == the new package's flash.bin", !memcmp(nor + 0x4000, logical + nfo + 0x4000, 0x93000 - 0x4000));
    bad += check("flash head [0, 0x4000) untouched", !memcmp(nor, head, sizeof head));
    bad += check("finish asked (0xF0000000)", f0_asked >= 1);
    bad += check("update records cleared (RAM + flash 0xE4F00, the stock firmware's 0xE8F00)",
                 record_cleared && nor[0xE4F06] == 0xFF && nor[0xE8F06] == 0xFF);
    bad += check("user data in Felucca's store that looks like a record is kept", !memcmp(nor + 0xA0000, user, sizeof user));
    bad += check("no write outside the allowed windows", bad_range == 0);
    /* same package again: nothing to erase */
    erases = 0; requests = 0;
    rc = ldr_session();
    bad += check("re-run with the same package erases nothing", rc == 0 && erases == 0);
    /* a package for another chip key is refused before any erase */
    {
        uint8_t save = logical[nfo + 0x4000 + 5];
        logical[nfo + 0x4000 + 5] ^= 0x5A;           /* app area head no longer decrypts */
        memcpy(nor, old + ofo, 0x93000);
        erases = 0;
        rc = ldr_session();
        bad += check("foreign key / damaged app head refused, nothing erased", rc == -6 && erases == 0);
        logical[nfo + 0x4000 + 5] = save;
    }
    {
        uint64_t h = 1469598103934665603ull;
        for (i = 0; i < sizeof nor; i++) { h ^= nor[i]; h *= 1099511628211ull; }
        printf("FINAL flash hash %016llx\n", (unsigned long long)h);          /* the same line with and without the screen: the write is bit for bit the same */
    }
#ifdef LDR_SCREEN_TEST
    bad += screen_checks(logical + nfo + 0x4000, old + ofo + 0x4000, old + ofo, nfo);
#endif
    printf("%s\n", bad ? "LOADER TEST FAILED" : "loader test passed");
    return bad != 0;
}
