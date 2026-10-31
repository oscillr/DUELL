/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL) */
/* DUELL 1.1 item 163: the backlight's place in the panel's start (owner: "there shouldn't be a screen before the splash").
 * The REAL hal/fm1_lcd_hw.h and src/lcd.c with their registers as host variables (FM1_LCD_HOST_REGS): every byte the
 * SPI sends is logged with the level of PA2 at that moment (PA2 = the backlight, active low, and the key matrix 595's /OE).
 * Built twice (host_tests.sh lcdboot):
 *   -DFM1_LCD_BL_LATE=1 (the main firmware, src/felucca.c): PA2 an output and HIGH (dark) from fm1_lcd_hw_init through
 *     SWRESET, SLPOUT, the setup, the whole black fill and DISPON; LOW (lit) when lcd_init returns, and nothing sent after it.
 *   plain (the update loader, loader/loader.c + ldr_screen.c, which turns the backlight on itself after lcd_init): PA2
 *     LOW from fm1_lcd_hw_init on, every command and pixel with it lit, exactly as before item 163.
 * Both: the same panel command list (SWRESET .. DISPON), the black frame complete (57,600 pixels) before DISPON. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FM1_LCD_HOST_REGS 1
static uint32_t h_pc_out, h_pc_dir, h_iomap, h_spi_con = 0x8000u, h_spi_baud, h_spi_adr;
static uint32_t h_pa_out, h_pa_dir = 0xFFFFFFFFu;     /* power-on: every port A pin an input, OUT 0 */
#define BL (1u << 2)
#define LOGN 4096
static struct { uint8_t data; uint32_t v, n, pa_out, pa_dir; } lg[LOGN];
static uint32_t nlg, nbytes_black_before_dispon, dispon_at = 0xFFFFFFFFu;
static uint32_t *slot_cmd(void)                    /* FM1_LCD_SPI_BUF = c: a command byte */
{
    lg[nlg].data = 0; lg[nlg].pa_out = h_pa_out; lg[nlg].pa_dir = h_pa_dir;
    return &lg[nlg++].v;
}
static uint32_t *slot_cnt(void)                    /* FM1_LCD_SPI_CNT = n: a data transfer of n bytes */
{
    lg[nlg].data = 1; lg[nlg].pa_out = h_pa_out; lg[nlg].pa_dir = h_pa_dir;
    return &lg[nlg++].n;
}
#define FM1_LCD_PC_OUT h_pc_out
#define FM1_LCD_PC_DIR h_pc_dir
#define FM1_LCD_IOMAP_CON1 h_iomap
#define FM1_LCD_SPI_CON h_spi_con                  /* bit 15 always set: every transfer done at once */
#define FM1_LCD_SPI_BAUD h_spi_baud
#define FM1_LCD_SPI_BUF (*slot_cmd())
#define FM1_LCD_SPI_ADR h_spi_adr
#define FM1_LCD_SPI_CNT (*slot_cnt())
#define FM1_LCD_PA_OUT h_pa_out
#define FM1_LCD_PA_DIR h_pa_dir
#include "../firmware/hal/fm1_lcd_hw.h"
#include "../firmware/src/lcd.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("lcdboot: %-118s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int lit(uint32_t pa_out, uint32_t pa_dir) { return !(pa_dir & BL) && !(pa_out & BL); }
static int dark(uint32_t pa_out, uint32_t pa_dir) { return !(pa_dir & BL) && (pa_out & BL); }

int main(void)
{
    static const uint8_t SEQ_CMDS[] = {0x01, 0x11, 0x3A, 0x36, 0x21, 0x13, 0x2A, 0x2B, 0x2C, 0x29};
    uint8_t cmds[32];
    uint32_t i, nc = 0, black = 0, all_dark = 1, all_lit = 1, after_dispon = 0;
    int late = FM1_LCD_BL_LATE;
    h_pa_out = 0x00000000u;                         /* worst case for the late order: OUT low, so a plain DIR switch would light it */
    lcd_init();
    for (i = 0; i < nlg; i++) {
        if (!lg[i].data) {
            if (nc < sizeof cmds)
                cmds[nc] = (uint8_t)lg[i].v;
            nc++;
            if (lg[i].v == 0x29u)
                dispon_at = i;
        } else if (nc && cmds[nc - 1u] == 0x2C)
            black += lg[i].n / 2u;
        if (dispon_at != 0xFFFFFFFFu && i > dispon_at)
            after_dispon++;
        if (!dark(lg[i].pa_out, lg[i].pa_dir))
            all_dark = 0;
        if (!lit(lg[i].pa_out, lg[i].pa_dir))
            all_lit = 0;
    }
    nbytes_black_before_dispon = black;
    printf("lcdboot: build %s, %u SPI transfers, %u commands\n", late ? "FM1_LCD_BL_LATE 1 (main firmware)" : "default (update loader)", nlg, nc);
    check("the panel commands: SWRESET SLPOUT COLMOD MADCTL INVON NORON, the window, RAMWR, DISPON last",
          nc == sizeof SEQ_CMDS && !memcmp(cmds, SEQ_CMDS, sizeof SEQ_CMDS));
    check("the black frame is whole (57,600 pixels) and sent before DISPON", black == 57600u && dispon_at == nlg - 1u && after_dispon == 0);
    check("lcd_busy clear at the end (DISPON waited for the fill's last transfer)", lcd_busy == 0);
    if (late) {
        check("PA2 an output and HIGH (backlight off, matrix 595 off) at every command and pixel transfer, DISPON included", all_dark);
        check("PA2 LOW (backlight on) when lcd_init returns: right after DISPON", lit(h_pa_out, h_pa_dir));
    } else {
        check("PA2 an output and LOW (backlight on) at every command and pixel transfer, from fm1_lcd_hw_init (as before)", all_lit);
        check("PA2 LOW when lcd_init returns (the loader's fm1_lcd_backlight(1) after it changes nothing)", lit(h_pa_out, h_pa_dir));
    }
    {   /* the HAL alone: fm1_lcd_hw_init's own end state */
        h_pa_out = 0; h_pa_dir = 0xFFFFFFFFu;
        fm1_lcd_hw_init();
        if (late)
            check("fm1_lcd_hw_init leaves PA2 an output, HIGH (off)", dark(h_pa_out, h_pa_dir));
        else
            check("fm1_lcd_hw_init leaves PA2 an output, LOW (on), as before", lit(h_pa_out, h_pa_dir));
        h_pa_out = 0; h_pa_dir = 0;                  /* PA2 already an output and LOW: an earlier stage left the backlight on */
        fm1_lcd_hw_init();
        if (late)
            check("an earlier stage left the backlight on: fm1_lcd_hw_init turns it off", dark(h_pa_out, h_pa_dir));
        else
            check("an earlier stage left the backlight on: fm1_lcd_hw_init keeps it on", lit(h_pa_out, h_pa_dir));
        if ((h_pa_out & ~BL) != 0 || (h_pa_dir & ~BL) != 0)
            check("no other port A pin touched", 0);
    }
    printf(bad ? "lcdboot: %d failed\n" : "lcdboot: all passed\n", bad);
    return bad != 0;
}
