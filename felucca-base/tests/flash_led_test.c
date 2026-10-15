/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; DUELL changes (C) 2026 OSCILL/R */
/* DUELL 0.2.14, from Felucca 1.6 (#236): the LEDs go dark during a flash erase (storage_hw.c st_erase).
 * The erase keeps the interrupts off for ~17-40 ms, so the LED scan (hal/fm1_input.h fm1_input_tick, a timer ISR) stops;
 * the column it left lit would burn at full duty for the whole erase. Here the real st_erase / st_prog / st_read run
 * against a stub flash driver, stub interrupt switches and stub audio duck, and the real LED scan on the GPIO as memory
 * (as tests/input_test.c). Every event is logged in order. Checked:
 *   - the scan has a column lit when the erase is asked for (else the test proves nothing);
 *   - the order: audio ducked, IRQs off, the LED rows written 0, the erase (the rows still 0 while it runs, no other
 *     LED write in between), IRQs on, the scan tick (pending meanwhile) lights the rows again, then the audio fades in;
 *   - after the erase the rows are the scan's own (FM1__LIT of its column), and the scan goes on as before;
 *   - an erase refused (outside the storage areas) touches neither the LEDs nor the audio;
 *   - st_prog (a page, and a ducked burst) and st_read write no LED line (only the erase was changed, as upstream). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t host_now;
#define FM1_INPUT_NOW() host_now
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_time.h"
#include "../firmware/hal/fm1_gpio.h"
static volatile uint32_t host_reg[16][64];
static uint32_t host_ticks(void) { return host_now += 240u; }

/* the event log */
enum { E_DUCK, E_UNDUCK, E_IRQ_OFF, E_IRQ_ON, E_LED, E_ERASE, E_PROG, E_READ };
static const char *const E_NAME[] = {"DUCK", "UNDUCK", "IRQ_OFF", "IRQ_ON", "LED", "ERASE", "PROG", "READ"};
#define NEV 256
static struct { uint8_t e; uint32_t v; } ev[NEV];
static uint32_t nev, led_now, logging;
static void log_ev(uint32_t e, uint32_t v)
{
    if (logging && nev < NEV) {
        ev[nev].e = (uint8_t)e;
        ev[nev].v = v;
        nev++;
    }
}
#define FM1_LED_TRACE(m) (led_now = (m), log_ev(E_LED, (m)))
#undef FM1_PR
#define FM1_PR(p, r) (*(&host_reg[(p) & 15u][((r) / 4u) & 63u]))
#define fm1_ticks host_ticks
#include "../firmware/hal/fm1_input.h"

/* what storage_hw.c needs from the firmware around it */
static int32_t abuf[64];
static struct { uint32_t erase_max_us, erase_last_us, prog_max_us, erases; } felucca_dbg;
static uint32_t irq_off, led_at_erase = 0xFFu, irq_at_erase;
static uint32_t irq_save(void)
{
    irq_off = 1;
    log_ev(E_IRQ_OFF, 0);
    return 0;
}
static void irq_restore(uint32_t f)
{
    (void)f;
    irq_off = 0;
    log_ev(E_IRQ_ON, 0);
    fm1_input_tick();                                  /* the scan tick, pending while the IRQs were off, runs at once */
}
static void audio_duck(void) { log_ev(E_DUCK, 0); }
static void audio_unduck(void) { log_ev(E_UNDUCK, 0); }
#define FL_FAR(fn) (fn)
#define FL_STORE_OK(off, n) ((off) >= 0x10000u && (off) + (n) <= 0x20000u)
static int fl_erase4k_ram(uint32_t off, uint32_t *took_us)
{
    (void)off;
    led_at_erase = led_now;                            /* the rows while the erase runs (nothing can write them: IRQs off) */
    irq_at_erase = irq_off;
    log_ev(E_ERASE, led_now);
    *took_us = 17000u;
    return 0;
}
static int fl_prog_ram(uint32_t off, const uint8_t *src, uint32_t n, uint32_t *took_us)
{
    (void)off; (void)src; (void)n;
    log_ev(E_PROG, 0);
    *took_us = 500u;
    return 0;
}
static int fl_read_ram(uint32_t off, uint8_t *dst, uint32_t n)
{
    (void)off;
    memset(dst, 0xFF, n);
    log_ev(E_READ, 0);
    return 0;
}
#include "../firmware/src/storage_hw.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
}
static int find(uint32_t e, uint32_t from)          /* the first event e at or after from, -1: none */
{
    uint32_t i;
    for (i = from; i < nev; i++)
        if (ev[i].e == e)
            return (int)i;
    return -1;
}
static uint32_t count(uint32_t e)
{
    uint32_t i, n = 0;
    for (i = 0; i < nev; i++)
        n += ev[i].e == e;
    return n;
}
static int no_led_irq_off(void)                     /* 1: no LED write between an IRQ_OFF and its IRQ_ON */
{
    uint32_t i, in = 0;
    for (i = 0; i < nev; i++) {
        in = ev[i].e == E_IRQ_OFF ? 1u : ev[i].e == E_IRQ_ON ? 0u : in;
        if (in && ev[i].e == E_LED)
            return 0;
    }
    return 1;
}
static void dump(void)
{
    uint32_t i;
    for (i = 0; i < nev; i++)
        printf(" %s%s", E_NAME[ev[i].e], ev[i].e == E_LED ? (ev[i].v ? "(on)" : "(0)") : "");
    printf("\n");
}

int main(void)
{
    uint32_t c, i, before;
    int duck, off, led0, er, on, relit, undk;
    static uint8_t page[1024];
    for (c = 0; c < FM1_NCOL; c++)                     /* every column has lit LEDs */
        fm1_led[c] = (uint8_t)(2u << (c & 3u));
    for (i = 0; i < 3u * FM1_NCOL + 1u; i++)           /* the scan runs a while and stops mid-frame, a column lit */
        fm1_input_tick();
    before = led_now;
    check("the scan left a column lit when the erase is asked for", before != 0u);

    printf("-- st_erase (a storage sector)\n");
    logging = 1;
    nev = 0;
    check("st_erase returns the driver's 0", st_erase(0x10000u) == 0);
    logging = 0;
    printf("events:");
    dump();
    duck = find(E_DUCK, 0);
    off = find(E_IRQ_OFF, 0);
    led0 = off < 0 ? -1 : find(E_LED, (uint32_t)off);
    er = find(E_ERASE, 0);
    on = find(E_IRQ_ON, 0);
    for (relit = on < 0 ? -1 : on; relit >= 0 && (uint32_t)relit < nev; relit++)   /* the first lit write after the IRQs */
        if (ev[relit].e == E_LED && ev[relit].v)
            break;
    undk = find(E_UNDUCK, 0);
    check("the audio ducks first, then the IRQs go off", duck == 0 && off > duck);
    check("the LED rows are written 0 after the IRQs are off and before the erase",
          led0 > off && er > led0 && ev[led0].v == 0u);
    for (i = (uint32_t)(led0 < 0 ? 0 : led0) + 1u; er > 0 && i < (uint32_t)er; i++)
        if (ev[i].e == E_LED)
            break;
    check("no other LED write between the dark rows and the erase", er > 0 && i == (uint32_t)er);
    check("the erase runs with the IRQs off and every LED row dark", irq_at_erase == 1u && led_at_erase == 0u);
    check("the IRQs come back after the erase, then the scan lights the rows again", on > er && relit > on);
    check("the audio fades in last, after the LEDs are back", undk > relit && (uint32_t)undk == nev - 1u);
    check("one erase, one duck, one unduck", count(E_ERASE) == 1u && count(E_DUCK) == 1u && count(E_UNDUCK) == 1u);
    check("the rows after the erase are the scan's own (the next column's LEDs)", led_now == FM1__LIT(fm1__tick_col) &&
          led_now != 0u);
    for (i = 0; i < FM1_NCOL; i++)
        fm1_input_tick();
    check("the scan goes on as before (a frame later: lit)", led_now == FM1__LIT(fm1__tick_col) && led_now != 0u);
    check("felucca_dbg counts the erase and its time", felucca_dbg.erases == 1u && felucca_dbg.erase_last_us == 17000u);

    printf("-- the other paths\n");
    before = led_now;
    logging = 1;
    nev = 0;
    check("st_erase outside the storage areas is refused (-8)", st_erase(0x1000u) == -8);
    check("  .. and touches neither the LEDs nor the audio", nev == 0u && led_now == before);
    nev = 0;
    st_prog(0x10000u, page, 256u);
    check("st_prog, one page: no duck, no LED write inside its IRQ-off window (unchanged)",
          no_led_irq_off() && count(E_DUCK) == 0u && count(E_PROG) == 1u);
    nev = 0;
    st_prog(0x10000u, page, sizeof page);
    check("st_prog, a burst: ducked, no LED write inside its IRQ-off windows (unchanged)",
          count(E_DUCK) == 1u && count(E_UNDUCK) == 1u && count(E_PROG) == 4u && no_led_irq_off() && led_now != 0u);
    nev = 0;
    st_read(0x10000u, page, 512u);
    check("st_read: no duck, the LEDs lit after", count(E_DUCK) == 0u && count(E_READ) == 2u && led_now != 0u);
    logging = 0;

    printf(fails ? "flash LEDs: %d FAILED\n" : "flash LEDs: all passed\n", fails);
    return fails != 0;
}
