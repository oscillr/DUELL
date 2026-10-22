/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL) */
/* The update loader's screen (DUELL 1.0): while the firmware is written the panel shows the DUELL word over the splash's wave lines, WRITING and the percent, a segmented
 * bar (20 segments of 5 percent) and DO NOT SWITCH OFF, as in release_drafts/LOADER_SCREEN_DESIGN.png (tools/loader_screen_design.py is the same picture).
 *
 * The write comes first, always. The screen is a guest of the loader:
 *   - it is drawn from two places only: ldr_screen_init() (before USB starts: the panel's init takes ~0.5 s, the host's first enumeration waits for it) and
 *     ldr_screen_progress() (after a sector is written and verified: the flash is idle then, nothing of the write runs at the same time);
 *   - every panel call is bounded (src/lcd.c: spins and fm1_lcd_wait have a timeout counter), the watchdog is fed between bands, and ONE failure of any kind
 *     (a timeout of the panel's SPI, a draw that took more than LS_BUDGET_MS) switches the screen off for good: from then on both functions return at once
 *     and the loader runs as if it had no screen;
 *   - it never returns a value, never touches the flash, the package, the USB state or the update record; its RAM is its own (a 240 x 40 strip, the wave points).
 * LDR_SCREEN 0 (tools/build.py, DUELL_LOADER_SCREEN=0) compiles both functions to nothing: the loader-free fallback package.
 * Needs (loader.c, tests/ldr_test.c): lcd.c and its hal functions, fm1_ticks / FM1_TICKS_PER_US, fm1_wdt_feed. Integer only, no 64-bit arithmetic. */
#ifndef LDR_SCREEN
#define LDR_SCREEN 1
#endif

#if LDR_SCREEN
#include "ldr_screen_data.h"

#define LS_BAND_H 40u                           /* the strip: 240 x 40 pixels, 19,200 B */
#define LS_UP_Y 104u                            /* the band that changes with the percent: WRITING, the percent, the bar (rows 104 .. 143) */
#define LS_NL 15                                /* the wave lines (ui_draw.c SPL_N) */
#define LS_NP 120                               /* a point every 2 px */
#define LS_BUDGET_MS 400u                       /* a draw longer than this: the screen gives up */
#define LS_STEP 12u                             /* the splash's animation step shown (a still frame) */

static uint16_t ls_buf[240u * LS_BAND_H];       /* byte-swapped RGB565, the order the panel takes */
static int16_t ls_y[LS_NL][LS_NP];              /* the wave lines' heights */
static uint8_t ls_on;                           /* 1: drawing; 0: never started or given up */
static uint8_t ls_pct = 255u;                   /* the percent on the panel (255: none yet) */
static uint32_t ls_pct_hist[2];                 /* (the host test reads these) last percent drawn and the number of draws */
static uint32_t ls_t0;

static uint16_t ls_sw(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

static int32_t ls_sine(uint32_t ph)             /* ui / dsp.c sine_i, from a quarter wave */
{
    uint32_t i = ph >> 22, j = (i + 1u) & 1023u;
    int32_t a, b;
    a = i <= 256u ? LS_SINE_Q[i] : i <= 512u ? LS_SINE_Q[512u - i] : i <= 768u ? -LS_SINE_Q[i - 512u] : -LS_SINE_Q[1024u - i];
    b = j <= 256u ? LS_SINE_Q[j] : j <= 512u ? LS_SINE_Q[512u - j] : j <= 768u ? -LS_SINE_Q[j - 512u] : -LS_SINE_Q[1024u - j];
    return a + (((b - a) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}

static uint16_t ls_mix(uint16_t a, uint16_t b, int32_t pct)   /* ui_mix: a + (b - a) * pct / 100 per channel, native RGB565 */
{
    static const uint8_t SH[3] = {11, 5, 0}, MK[3] = {31, 63, 31};
    uint32_t out = 0, k;
    for (k = 0; k < 3u; k++) {
        int32_t x = (a >> SH[k]) & MK[k], y = (b >> SH[k]) & MK[k], d = (y - x) * pct;
        out |= (uint32_t)(x + (d + (d >= 0 ? 50 : -50)) / 100) << SH[k];
    }
    return (uint16_t)out;
}

static void ls_px(int32_t x, int32_t y, uint32_t y0, uint16_t c)
{
    y -= (int32_t)y0;
    if (x >= 0 && x < 240 && y >= 0 && y < (int32_t)LS_BAND_H)
        ls_buf[(uint32_t)y * 240u + (uint32_t)x] = c;
}

static void ls_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t by, uint16_t c)   /* cv_line */
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy, guard = 600;
    while (guard--) {
        int32_t e2 = 2 * err;
        ls_px(x0, y0, by, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void ls_wave_points(void)                /* ui_draw.c splash_frame, the lines' heights, once */
{
    uint32_t m = LS_STEP * 0x00800000u, j, i;
    for (j = 0; j < (uint32_t)LS_NL; j++) {
        int32_t j2 = (int32_t)j * 2 - (LS_NL - 1);
        for (i = 0; i < (uint32_t)LS_NP; i++) {
            uint32_t x = i * 2u, xt = x * 17895697u / 100u;
            int32_t mc = 120 + ((ls_sine(xt * 90u + m) * 22) >> 15);
            int32_t w = 170 + ((ls_sine(xt * 130u + m * 2u + 0x40000000u) * 40) >> 15);
            int32_t mo = ((int32_t)x - 120) * 256 / 240 + 128 + (ls_sine(m) >> 8);
            int32_t calm = ls_sine(xt * 100u + j * 0x05000000u + m);
            int32_t rip = (ls_sine(xt * 370u + j * 0x0C000000u + m * 3u) * 3 + ls_sine(xt * 710u - j * 0x1B000000u + m * 5u) * 2) / 5;
            mo = mo < 0 ? 0 : mo > 256 ? 256 : mo;
            ls_y[j][i] = (int16_t)(mc + j2 * w / (2 * (LS_NL - 1)) + ((calm * 5 + (rip * mo / 256) * 4) >> 15));
        }
    }
}

static void ls_logo(uint32_t y0)                /* the DUELL word at (22, 40), blended in the text colour over what the strip holds */
{
    uint32_t i, p = 0;
    for (i = 0; i < (uint32_t)sizeof LS_LOGO; i++) {
        uint32_t v = LS_LOGO[i] >> 4, n = (LS_LOGO[i] & 15u) + 1u, e = p + n;
        for (; v && p < e; p++) {
            int32_t py = 40 + (int32_t)(p / LS_LOGO_W) - (int32_t)y0, px = 22 + (int32_t)(p % LS_LOGO_W);
            if (py >= 0 && py < (int32_t)LS_BAND_H)
                ls_buf[(uint32_t)py * 240u + (uint32_t)px] = ls_sw(ls_mix(ls_sw(ls_buf[(uint32_t)py * 240u + (uint32_t)px]), LS_TEXT, (int32_t)(v * 100u / 15u)));
        }
        p = e;
    }
}

static void ls_alpha(const uint8_t *d, uint32_t w, uint32_t h, int32_t x, int32_t y, uint32_t y0)   /* a 4-bit alpha bitmap in the text colour */
{
    uint32_t r, c, k = 0;
    for (r = 0; r < h; r++)
        for (c = 0; c < w; c++, k++) {
            uint32_t a = (k & 1u) ? d[k >> 1] & 15u : d[k >> 1] >> 4;
            int32_t py = y + (int32_t)r - (int32_t)y0, px = x + (int32_t)c;
            if (a && py >= 0 && py < (int32_t)LS_BAND_H && px >= 0 && px < 240)
                ls_buf[(uint32_t)py * 240u + (uint32_t)px] = ls_sw(ls_mix(ls_sw(ls_buf[(uint32_t)py * 240u + (uint32_t)px]), LS_TEXT, (int32_t)(a * 100u / 15u)));
        }
}

static void ls_percent(uint32_t pct, uint32_t y0)   /* "45%", right-aligned to x = 218, text top 108 */
{
    char s[4];
    uint32_t n = 0, i, adv = 0;
    int32_t x;
    if (pct >= 100u)
        s[n++] = 1;
    if (pct >= 10u)
        s[n++] = (char)(pct / 10u % 10u);
    s[n++] = (char)(pct % 10u);
    s[n++] = 10;                                  /* % */
    for (i = 0; i < n; i++)
        adv += LS_GLYPH[(uint32_t)(uint8_t)s[i] * 7u + 6u];
    x = 218 - (int32_t)adv;
    for (i = 0; i < n; i++) {
        const uint8_t *g = LS_GLYPH + (uint32_t)(uint8_t)s[i] * 7u;
        ls_alpha(LS_GDATA + (g[0] | g[1] << 8), g[2], g[3], x + (int32_t)(int8_t)g[4], 108 + (int32_t)(int8_t)g[5], y0);
        x += g[6];
    }
}

static void ls_bar(uint32_t pct, uint32_t y0)   /* 20 segments across x 22 .. 217, rows 134 .. 143 */
{
    uint32_t k, x, y;
    for (k = 0; k < 20u; k++) {
        uint32_t xa = 22u + k * 197u / 20u, xb = 22u + (k + 1u) * 197u / 20u - 1u;
        uint16_t c = ls_sw(k < pct / 5u ? ls_mix(LS_C0, LS_C2, (int32_t)(k * 100u / 19u)) : LS_RAISE_);
        for (y = 134u; y < 144u; y++)
            if (y >= y0 && y < y0 + LS_BAND_H)
                for (x = xa; x < xb; x++)
                    ls_buf[(y - y0) * 240u + x] = c;
    }
}

static void ls_band(uint32_t y0, uint32_t h, uint32_t pct)   /* render rows y0 .. y0 + h of the screen into the strip and send them */
{
    uint32_t i, j, n = 240u * h;
    uint16_t bg = ls_sw(LS_BG);
    for (i = 0; i < n; i++)
        ls_buf[i] = bg;
    for (j = 0; j < (uint32_t)LS_NL; j++) {
        uint16_t c = ls_sw(ls_mix(ls_mix(LS_C0, LS_C2, (int32_t)j * 100 / (LS_NL - 1)), LS_BG, 18));
        for (i = 1; i < (uint32_t)LS_NP; i++)
            ls_line((int32_t)(i * 2u - 2u), ls_y[j][i - 1u], (int32_t)(i * 2u), ls_y[j][i], y0, c);
    }
    if (y0 < 74u && y0 + h > 40u)
        ls_logo(y0);
    if (y0 < 126u && y0 + h > 108u) {
        ls_alpha(LS_WRITING, LS_WRITING_W, LS_WRITING_H, 22 + LS_WRITING_BX, 108 + LS_WRITING_BY, y0);
        ls_percent(pct, y0);
    }
    if (y0 < 144u && y0 + h > 134u)
        ls_bar(pct, y0);
    if (y0 < 196u && y0 + h > 176u)
        ls_alpha(LS_DONOT, LS_DONOT_W, LS_DONOT_H, 22 + LS_DONOT_BX, 176 + LS_DONOT_BY, y0);
    lcd_blit(0, y0, 240, h, ls_buf);
    lcd_sync();                                   /* the strip is sent before anything else runs (and before the next one is drawn into it) */
}

static void ls_give_up(void) { ls_on = 0; }

static void ls_check(uint32_t timeouts0)         /* one failure ends the screen */
{
    uint32_t took = (fm1_ticks() - ls_t0) / (1000u * FM1_TICKS_PER_US);
    if (fm1_lcd_timeouts != timeouts0 || took > LS_BUDGET_MS)
        ls_give_up();
}

/* before usb_start(): the panel's init and the whole screen at 0 percent */
static void ldr_screen_init(void)
{
    static const uint8_t BAND[8] = {0, 40, 80, 104, 144, 184, 224, 240};
    uint32_t t0 = fm1_lcd_timeouts, b;
    ls_t0 = fm1_ticks();
    lcd_init();
    fm1_lcd_backlight(1);
    fm1_wdt_feed();
    ls_wave_points();
    ls_on = 1;
    for (b = 0; b < 7u && ls_on; b++) {
        fm1_wdt_feed();
        ls_band(BAND[b], (uint32_t)(BAND[b + 1u] - BAND[b]), 0u);
        if (fm1_lcd_timeouts != t0)
            ls_give_up();
    }
    ls_pct = 0u;
    ls_pct_hist[0] = 0u;
}

/* after a sector is written and verified: the percent of the app area, the strip with WRITING, the percent and the bar redrawn when the percent changed */
static void ldr_screen_progress(uint32_t done, uint32_t total)
{
    uint32_t pct, t0;
    if (!ls_on || !total)
        return;
    pct = done >= total ? 100u : done * 100u / total;
    if (pct == ls_pct)
        return;
    t0 = fm1_lcd_timeouts;
    ls_t0 = fm1_ticks();
    fm1_wdt_feed();
    ls_band(LS_UP_Y, LS_BAND_H, pct);
    ls_pct = (uint8_t)pct;
    ls_pct_hist[0] = pct;
    ls_pct_hist[1]++;
    ls_check(t0);
}
#else
static void ldr_screen_init(void) {}
static void ldr_screen_progress(uint32_t done, uint32_t total) { (void)done; (void)total; }
#endif
