/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM-1 LCD wiring: SPI1 on PC9 CLK, PC10 DO, PC7 CS,
 * PC8 D/C; backlight PA2, active low. The panel protocol is src/lcd.c.
 *
 *   fm1_lcd_hw_init()          pins, backlight on, SPI1 master at BAUD 4 (FM1_LCD_BL_LATE 1: backlight off, see below)
 *   fm1_lcd_baud(b)            SPI1 clock = lsb / (b + 1)
 *   fm1_lcd_send_cmd(c)        D/C low, CS low, one byte (then fm1_lcd_wait)
 *   fm1_lcd_send_data(p, n)    D/C high, CS low, DMA n bytes from RAM p
 *   fm1_lcd_wait()             SPI done (or timeout, counted), pending cleared
 *   fm1_lcd_deselect()         CS high
 *   fm1_lcd_backlight(on)      PA2 low (on) / high (off); the TIMER5 scan writes port A too: IRQs off around it
 */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

/* FM1_LCD_BL_LATE (DUELL 1.1 item 163, owner: no screen before the splash): 1 = fm1_lcd_hw_init drives PA2 HIGH
 * (backlight off) and src/lcd.c lcd_init turns it on after DISPON, when the panel shows its first black frame. Only the
 * main firmware sets it (src/felucca.c, before this header). Default 0 = PA2 low (on) at once, exactly as before: the
 * update loader (loader/loader.c, ldr_screen.c) and every other user keep today's order. PA2 is also the key matrix
 * 595's /OE: with 1 the matrix outputs are enabled only at the end of lcd_init (nothing reads keys before that:
 * fm1_input_init and the TIMER5 scan come later in main.c). tests/lcd_boot_test.c */
#ifndef FM1_LCD_BL_LATE
#define FM1_LCD_BL_LATE 0
#endif

#ifndef FM1_LCD_HOST_REGS                 /* (tests/lcd_boot_test.c: the registers as host variables) */
#define FM1_LCD_PC_OUT (*(volatile uint32_t *)0x50080u)
#define FM1_LCD_PC_DIR (*(volatile uint32_t *)0x50088u)
#define FM1_LCD_IOMAP_CON1 (*(volatile uint32_t *)0x51020u)
#define FM1_LCD_SPI_CON (*(volatile uint32_t *)0x11D00u)
#define FM1_LCD_SPI_BAUD (*(volatile uint32_t *)0x11D04u)
#define FM1_LCD_SPI_BUF (*(volatile uint32_t *)0x11D08u)
#define FM1_LCD_SPI_ADR (*(volatile uint32_t *)0x11D0Cu)
#define FM1_LCD_SPI_CNT (*(volatile uint32_t *)0x11D10u)
#define FM1_LCD_PA_OUT (*(volatile uint32_t *)0x50000u)
#define FM1_LCD_PA_DIR (*(volatile uint32_t *)0x50008u)
#endif
#define FM1_LCD_BL (1u << 2)
#define FM1_LCD_CS (1u << 7)
#define FM1_LCD_DC (1u << 8)
#define FM1_LCD_CLK (1u << 9)
#define FM1_LCD_DO (1u << 10)

static uint32_t fm1_lcd_timeouts;

FM1_INLINE void fm1_lcd_hw_init(void)
{
#if FM1_LCD_BL_LATE
    FM1_LCD_PA_OUT |= FM1_LCD_BL;         /* off (high) before PA2 becomes an output: never a low pulse */
#else
    FM1_LCD_PA_OUT &= ~FM1_LCD_BL;
#endif
    FM1_LCD_PA_DIR &= ~FM1_LCD_BL;
    FM1_LCD_IOMAP_CON1 |= 0x10u;
    FM1_LCD_PC_OUT |= FM1_LCD_CS;
    FM1_LCD_PC_OUT &= ~(FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
    FM1_LCD_PC_DIR &= ~(FM1_LCD_CS | FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
    FM1_LCD_SPI_CON = 0x4021u;
    FM1_LCD_SPI_BAUD = 4u;
}

FM1_INLINE void fm1_lcd_backlight(uint32_t on)
{
    if (on)
        FM1_LCD_PA_OUT &= ~FM1_LCD_BL;
    else
        FM1_LCD_PA_OUT |= FM1_LCD_BL;
}

FM1_INLINE void fm1_lcd_baud(uint32_t b) { FM1_LCD_SPI_BAUD = b; }

static void fm1_lcd_wait(void)
{
    uint32_t n;
    for (n = 0; n < 4000000u && !(FM1_LCD_SPI_CON & 0x8000u); n++)
        ;
    if (n == 4000000u)
        fm1_lcd_timeouts++;
    FM1_LCD_SPI_CON |= 0x4000u;
}

FM1_INLINE void fm1_lcd_deselect(void) { FM1_LCD_PC_OUT |= FM1_LCD_CS; }

FM1_INLINE void fm1_lcd_send_cmd(uint8_t c)
{
    FM1_LCD_PC_OUT &= ~FM1_LCD_DC;
    FM1_LCD_PC_OUT &= ~FM1_LCD_CS;
    FM1_LCD_SPI_CON |= 0x4000u;
    FM1_LCD_SPI_BUF = c;
}

FM1_INLINE void fm1_lcd_send_data(const void *p, uint32_t n)
{
    FM1_LCD_PC_OUT |= FM1_LCD_DC;
    FM1_LCD_PC_OUT &= ~FM1_LCD_CS;
    FM1_LCD_SPI_CON |= 0x4000u;
    FM1_LCD_SPI_ADR = (uint32_t)(uintptr_t)p;
    FM1_LCD_SPI_CNT = n;
}
