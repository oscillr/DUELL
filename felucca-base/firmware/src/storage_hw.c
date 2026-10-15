/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The firmware side of storage.c (FELUCCA_FLASH): flash reads in short IRQ-off windows so the
 * audio keeps up, erases and writes only inside the storage areas, the audio ducked to silence
 * first (DUELL 0.2.11). The host tests (storage_test.c) provide their own st_* on a simulated NOR. */
static uint8_t flash_ok;                 /* JEDEC id matched at boot (persist_boot) */
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static void audio_silence(void)                 /* IRQs off: the DMA would loop stale audio (buzz) */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}
/* DUELL 0.2.11: the flash duck (audio.c audio_duck, SPEC 8.2a) around every erase and long program burst, nested: storage.c's
 * st_save_to holds one duck over its erase, its payload and its commit record (one dip, not three). The driver's own times
 * (took, us) go to felucca_dbg (the TEST screen shows the longest erase) */
static uint8_t st_ducks;
static void st_duck_begin(void)
{
    if (!st_ducks++)
        audio_duck();
}
static void st_duck_end(void)
{
    if (st_ducks && !--st_ducks)
        audio_unduck();
}
#define ST_DUCK_BEGIN() st_duck_begin()
#define ST_DUCK_END() st_duck_end()
static int st_erase(uint32_t off)
{
    uint32_t took = 0, f;
    int rc;
    if (!FL_STORE_OK(off, 0x1000u))
        return -8;
    st_duck_begin();
    f = irq_save();
    audio_silence();                            /* (already silent after the duck; kept for a duck that timed out) */
    fm1__led_lines(0);                          /* DUELL 0.2.14, from Felucca 1.6 (#236): the erase stops the LED scan (IRQs off */
    rc = FL_FAR(fl_erase4k_ram)(off, &took);    /* for ~17-40 ms): the column lit at that moment would burn at full duty and the */
    irq_restore(f);                             /* rest go dark, a flash of the panel. All rows dark instead: the panel just drops
                                                 * out, and the scan tick (fm1_input_tick, pending while the IRQs were off) lights
                                                 * the rows again at its next pass, before the audio fades back in (st_duck_end).
                                                 * Order: audio ducked, LEDs off, erase, LEDs back, audio back (SPEC 8.2a) */
    st_duck_end();
    felucca_dbg.erase_last_us = took;
    felucca_dbg.erases++;
    if (took > felucca_dbg.erase_max_us)
        felucca_dbg.erase_max_us = took;
    return rc;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)   /* a page (256 bytes) per IRQ-off window, as fl_write */
{
    const uint8_t *s = src;
    int rc = 0, burst = n > 256u;               /* more than one page: a burst of IRQ-off windows, ducked */
    if (!FL_STORE_OK(off, n))                   /* only inside the storage areas */
        return -8;
    if (burst)
        st_duck_begin();
    while (n && !rc) {
        uint32_t k = 256u - (off & 0xFFu), took = 0, f;
        k = k > n ? n : k;
        f = irq_save();
        rc = FL_FAR(fl_prog_ram)(off, s, k, &took);
        irq_restore(f);
        if (took > felucca_dbg.prog_max_us)
            felucca_dbg.prog_max_us = took;
        off += k;
        s += k;
        n -= k;
    }
    if (burst)
        st_duck_end();
    return rc;
}
