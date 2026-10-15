/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.14 diagnosis: what still sounds after STOP ALL in M02 WHISPER TIDE with PERFORM effects played and SPACE at maximum.
 * M02 loaded (LAYER), a chord held, PLAY, PERFORM R16 / FREEZE / OCT UP played, SPACE +100, STOP ALL (transport_req 4), 20 s rendered;
 * the level per 100 ms, the time to -60 / -80 dB (dBFS of 16 bit) and the voices still active, per carrier (sends zeroed to isolate). */
#define FELUCCA_FLASH 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
#include <math.h>
#define SEC ((uint32_t)(FS / CTL))
#define SLOTS 1300u
static double lvl[SLOTS];
static uint32_t voices_alive(void)
{
    uint32_t k, i, n = 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            n += trk[k].v[i].active != 0;
    return n;
}
/* mode: 0 as is, 1 no sends (dry), 2 reverb only, 3 delay only, 4 chorus only, 5 SENDS OFF switch (mods_off) before the stop, 6 STOP ALL twice-style flush none
 * perf: 1 PERFORM effects played and SPACE +100; 0 none */
static void run(const char *name, uint32_t mode, int perf)
{
    static int32_t out[2 * CTL];
    uint32_t b, k, chord = 0, nb, voice_end = 0, at60 = 0, at80 = 0, stop_b;
    double acc = 0, pk = 0;
    uint32_t cnt = 0, slot = 0;
    memset(lvl, 0, sizeof lvl);
    ui_power_on();
    for (k = 0; k < NTRK; k++) memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
    song.master_q12 = 2048;                          /* (felucca_init: the default MASTER) */
    for (k = 0; k < 27u; k++)
        if (kb_map(&trk[0], k) == 60u || kb_map(&trk[0], k) == 64u || kb_map(&trk[0], k) == 67u)
            chord |= 1u << k;
    duell_mode_req = DM_LAYER;
    project_load(1);
    for (b = 0; b < 8u; b++) { mix_block(out, CTL); while (wave_service()) ; }
    for (k = 0; k < NTRK; k++) {
        if (mode == 1 || mode == 3 || mode == 4) trk[k].p[P_REV] = 0;
        if (mode == 1 || mode == 2 || mode == 4) trk[k].p[P_DLY] = 0;
        if (mode == 1 || mode == 2 || mode == 3) trk[k].p[P_CHOR] = 0;
    }
    if (mode == 5) mods_off |= MODS_SENDS;
    transport_req = 1;
    if (perf & 2) { perf_space = 100; sv.spc = 100 * 256; }
    for (b = 0; b < 12u * SEC; b++) {                 /* 12 s: chord held 0..4 s, then let go (latched), PERFORM effects played 3..9 s */
        fm1_in.notes = b < 4u * SEC ? chord : 0u;
        if (perf & 1) {
            uint32_t s = b / SEC;
            perf_held = s == 3 || s == 4 ? PF_BIT(PF_R16) : s == 5 || s == 6 ? PF_BIT(PF_FRZ) : s == 7 || s == 8 ? PF_BIT(PF_OUP) : 0u;
            kb_perf = perf_held ? 1u : 0u;
        }
        mix_block(out, CTL);
        while (wave_service()) ;
        { uint32_t i; for (i = 0; i < 2*CTL; i++) { double a = fabs((double)out[i]); if (a > pk) pk = a; } }
    }
    /* STOP ALL; effects keys let go as the owner would (PERFORM stays on, SPACE stays at max) */
    perf_held = 0; kb_perf = 0; fm1_in.notes = 0;
    transport_req = 4;
    if (mode == 6) memset(dly_buf, 0, sizeof dly_buf);                 /* the carrier flushed at the stop, to see what is left */
    if (mode == 7) { rev_clear(); rev_clear_b(); }
    if (mode == 8) { memset(cho_buf, 0, sizeof cho_buf); memset(cho_buf_b, 0, sizeof cho_buf_b); }
    if (mode == 9) { memset(dly_buf, 0, sizeof dly_buf); dm_lines_clear(); }
    stop_b = 0;
    nb = 120u * SEC;
    for (b = 0; b < nb; b++) {
        uint32_t i;
        double e = 0;
        mix_block(out, CTL);
        while (wave_service()) ;
        if (voices_alive()) voice_end = b + 1u;
        for (i = 0; i < CTL; i++) { double l = out[2 * i], r = out[2 * i + 1]; e += l * l + r * r; }
        acc += e; cnt += 2u * CTL;
        if (cnt >= 2u * FS / 10u && slot < SLOTS) { lvl[slot++] = acc / cnt; acc = 0; cnt = 0; }
    }
    (void)stop_b; (void)pk;
    for (b = 0; b < slot; b++) {
        double db = lvl[b] > 0 ? 10.0 * log10(lvl[b] / (32768.0 * 32768.0)) : -200;
        if (db > -60) at60 = b + 1;
        if (db > -80) at80 = b + 1;
    }
    if (mode == 0 && perf == 3) printf("stoptail: M02 globals: RTYPE %d RSIZE %d RDAMP %d DFDBK %d DMODE %d CDEPTH %d; sends P_CHOR %d P_DLY %d P_REV %d / %d %d %d\n",
        song.g[G_RTYPE], song.g[G_RSIZE], song.g[G_RDAMP], song.g[G_DFDBK], song.g[G_DMODE], song.g[G_CDEPTH],
        trk[0].p[P_CHOR], trk[0].p[P_DLY], trk[0].p[P_REV], trk[1].p[P_CHOR], trk[1].p[P_DLY], trk[1].p[P_REV]);
    printf("stoptail: %-34s voices dead after %5.2f s | last 100ms slot above -60 dB: %5.1f s, above -80 dB: %5.1f s | ", name, voice_end * (double)CTL / FS,
           at60 / 10.0, at80 / 10.0);
    printf("RMS dBFS at 0.1/0.5/1/2/4/8/16/32/64/119 s:");
    {   static const uint32_t at[10] = {0, 4, 9, 19, 39, 79, 159, 319, 639, 1189};
        for (b = 0; b < 10; b++) printf(" %.0f", lvl[at[b]] > 0 ? 10.0 * log10(lvl[at[b]] / (32768.0 * 32768.0)) : -200.0);
    }
    printf("\n");
}
#ifndef STOPTAIL_NO_MAIN
int main(void)
{
    memset(nor, 0xFF, sizeof nor);
    run("M02, effects + SPACE max", 0, 3);
    run("  .. no sends (dry; SPACE floor)", 1, 3);
    run("  .. reverb send only", 2, 3);
    run("  .. delay send only", 3, 3);
    run("  .. chorus send only", 4, 3);
    run("  .. SENDS OFF switch", 5, 3);
    run("  .. delay flushed at the stop", 6, 3);
    run("  .. reverb flushed at the stop", 7, 3);
    run("  .. chorus flushed at the stop", 8, 3);
    run("  .. all lines flushed at the stop", 9, 3);
    run("M02, effects, SPACE 0", 0, 1);
    run("  .. no sends", 1, 1);
    run("M02, SPACE max, no effects", 0, 2);
    run("M02, nothing (plain)", 0, 0);
    return 0;
}
#endif
