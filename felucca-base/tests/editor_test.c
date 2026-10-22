/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The firmware editor handler, USB framing and UART recovery against RAM flash.
 * Build with the generated tables and the same flags as hostsim.c. */
static unsigned char host_samples[3][0x14000];   /* RAM flash at 0xA0000 (the user sample region until 0.2.9: nothing may write it now) */
#define FELUCCA_OTA 1
#define FELUCCA_FLASH 0
#define FELUCCA_VERSION "TEST"
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1                                    /* (as the developer builds: MENU > USB SERIAL switches the console; tests/menu_product_test.c builds it with 0) */
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main

static uint32_t host_progress = 1, host_erases, host_writes;
static uint8_t host_wire[4096];
static uint32_t host_wire_n;
static void host_drain(void)
{
    while (so_r != so_w) {
        uint32_t p = sx_out_q[so_r++ % SXQ], cin = p & 15u, i;
        uint32_t n = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        for (i = 0; i < n && host_wire_n < sizeof host_wire; i++)
            host_wire[host_wire_n++] = (uint8_t)(p >> (8u * (i + 1u)));
    }
}
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { host_drain(); fm1_ms++; }
static void fm1_wdt_feed(void)
{
    fm1_ms++;
    if (host_progress && transport_req == 2u) { seq_stop(); transport_req = 0; }
}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }   /* (MENU > SCREEN OFF: lcd.c) */
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
static void panel_setup(void) {}
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static uint32_t flash_ok = 1;
static void audio_silence(void) {}
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static uint8_t *host_flash_ptr(uint32_t off) { return &host_samples[0][0] + off - 0xA0000u; }
static int fl_erase4k(uint32_t off, uint32_t *took)
{
    memset(host_flash_ptr(off), 0xFF, 4096); *took = 0; host_erases++; return 0;
}
static int fl_write(uint32_t off, const void *p, uint32_t n)
{
    memcpy(host_flash_ptr(off), p, n); host_writes++; return 0;
}
static int st_read(uint32_t off, void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_erase(uint32_t off) { (void)off; return -1; }
#include "../firmware/src/storage.c"
#include "../firmware/src/editor.c"

static int check(const char *what, int ok)
{
    printf("editor: %-70s %s\n", what, ok ? "ok" : "FAIL");
    return !ok;
}
static void reset(void)
{
    uint32_t t, i;
    memset(&song, 0, sizeof song); memset(trk, 0, sizeof trk);
    memset(&ed_w, 0, sizeof ed_w); memset(&ui, 0, sizeof ui);
    memset(&favorites, 0, sizeof favorites); memset(&settings, 0, sizeof settings); settings_init();
    midi_map_ready = 0; ml_off();                       /* (the MIDI MAP: the power-on list again, nothing armed) */
    memset(&um, 0, sizeof um);
    pt_boot();                                          /* (DUELL: the PATCH store empty again: no stored patch) */
    host_progress = 1; host_erases = host_writes = host_wire_n = 0;
    transport_req = panic_req = 0; sx_ready = sx_collect = sx_busy = 0;
    so_r = so_w = mi_r = mi_w = 0; midi_in_overflow = 0; usb.config = 1;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (t = 0; t < NTRK; t++) {
        track_defaults(&trk[t]); set_engine_of(&trk[t], 0); apply_preset_to(&trk[t], 0);
        trk[t].engine = trk[t].eng_req; track_defaults_steps(&trk[t]);
    }
}
static uint32_t request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i;
    host_wire_n = 0; ota_frame_done();
    sysex_byte(0xF0); sysex_byte(ED_HDR0); sysex_byte(ED_HDR1); sysex_byte(ED_HDR2); sysex_byte((uint8_t)cmd);
    for (i = 0; i < n; i++) sysex_byte(a[i]);
    sysex_byte(0xF7); ed_service(); host_drain();
    return host_wire_n;
}
static uint32_t pack7(const uint8_t *p, uint32_t n, uint8_t *a)
{
    uint32_t o = 0, i;
    while (n) {
        uint32_t k = n > 7u ? 7u : n, m = o++;
        a[m] = 0;
        for (i = 0; i < k; i++, n--) { a[m] |= (*p >> 7) << i; a[o++] = *p++ & 127u; }
    }
    return o;
}

/* 1.1 parameter locks over MOTION (64): ops 5 (set a lock), 6 (clear a step's locks, 127 every step's), 7 (the query
 * with the kinds); the query and ops 1..4 reply as before (a lock's id without bit 7) */
static int motion_locks(void)
{
    int bad = 0;
    uint32_t n;
    uint8_t a[8];
    reset();
    motion_clear(&trk[1]);
    a[0] = 1; a[1] = 3; a[2] = 2; a[3] = P_REV; a[4] = (uint8_t)((60 + 8192) & 127); a[5] = (uint8_t)((60 + 8192) >> 7);
    request(ED_MOTION, a, 6);                                           /* an automation event, step 2 */
    a[1] = 5; a[2] = 5; a[4] = (uint8_t)((100 + 8192) & 127); a[5] = (uint8_t)((100 + 8192) >> 7);
    n = request(ED_MOTION, a, 6);                                       /* a lock, step 5 */
    bad += check("MOTION op 5 sets a lock: rc 0, two records, then their kinds 0 1",
        host_wire[6] == 0 && host_wire[8] == 2 && host_wire[10] == 2 && host_wire[11] == P_REV &&
        host_wire[14] == 5 && host_wire[15] == P_REV && n == 5u + 5u + 2u * 4u + 2u + 1u &&
        host_wire[18] == 0 && host_wire[19] == 1 && motion_lock_count(&trk[1]) == 1u);
    n = request(ED_MOTION, a, 1);
    bad += check("MOTION query as before 1.1: no kinds, the lock's id without bit 7", n == 5u + 5u + 2u * 4u + 1u &&
        host_wire[15] == P_REV);
    a[1] = 7; n = request(ED_MOTION, a, 2);
    bad += check("MOTION op 7: the query with the kinds", n == 5u + 5u + 2u * 4u + 2u + 1u && host_wire[19] == 1);
    a[1] = 5; a[2] = 64; n = request(ED_MOTION, a, 6);
    bad += check("MOTION op 5 refuses step 64 (rc 1)", host_wire[6] == 1);
    a[1] = 6; a[2] = 2; request(ED_MOTION, a, 3);
    bad += check("MOTION op 6 on step 2: its automation event stays", host_wire[6] == 0 && host_wire[8] == 2);
    a[2] = 127; request(ED_MOTION, a, 3);
    bad += check("MOTION op 6, 127: every lock goes, the automation stays", host_wire[6] == 0 && host_wire[8] == 1 &&
        host_wire[14] == 0 && !motion_lock_count(&trk[1]));
    motion_clear(&trk[1]);
    /* 1.2 (INFO 41 01 00 01): 128 records; ops 1..7 reply as before (max 64, at most 64 records), op 8 all of them */
    {
        uint32_t i, ok = 1;
        for (i = 0; i < MOTION_MAX - 1u; i++)            /* 127 on track 2: two ids a step */
            ok &= !motion_set_event(&trk[1], i % NSTEP, i < NSTEP ? P_REV : P_DLY, (int16_t)(i % 100u));
        a[1] = 5; a[2] = 63; a[3] = P_PAN; a[4] = (uint8_t)((-3 + 8192) & 127); a[5] = (uint8_t)((-3 + 8192) >> 7);
        n = request(ED_MOTION, a, 6);                   /* the 128th: a lock of PAN -3 */
        ok &= host_wire[6] == 0 && motion.count == MOTION_MAX && host_wire[8] == 64 && host_wire[9] == 64 &&
              n == 5u + 5u + 64u * 4u + 64u + 1u;
        bad += check("MOTION 1.2: 128 records on one track; op 5's reply as before: max 64, its first 64 listed", ok);
        a[1] = 3; a[2] = 0; a[3] = P_CHOR; a[4] = 0; a[5] = 64;
        n = request(ED_MOTION, a, 6);
        bad += check("  the 129th: rc 2 (full)", host_wire[6] == 2 && motion.count == MOTION_MAX);
        a[1] = 8; n = request(ED_MOTION, a, 2);
        ok = n == 5u + 7u + 128u * 4u + 128u + 1u && host_wire[8] == 0 && host_wire[9] == 1 && host_wire[10] == 0 &&
             host_wire[11] == 1 && host_wire[12 + 127u * 4u] == 63 && host_wire[13 + 127u * 4u] == P_PAN &&
             ed_rv(&host_wire[14 + 127u * 4u]) == -3 && host_wire[12 + 128u * 4u + 127u] == 1 &&
             host_wire[12 + 128u * 4u] == 0 && n <= sizeof ed_out;
        bad += check("  op 8: count and max as two 7-bit bytes (128), all 128 records, then their kinds", ok);
        a[1] = 8; a[2] = 0;
        bad += check("  op 8 with a further byte: no reply", !request(ED_MOTION, a, 3));
        motion_clear(&trk[1]);
    }
    return bad;
}

static int preferences(void)
{
    int bad = 0;
    uint8_t a[4] = {0, 3, 0, 0};          /* (palette 3: a colour theme; DUELL's 7 is GREY, whose T_BG is the neutral one) */
    reset();
    uint32_t n = request(ED_INFO, a, 0);
    bad += check("INFO explicitly tags display capabilities (song rows 0 since DUELL, the SONG lanes tag gone) without changing command 33",
        ED_SONG_RETIRED == 33 && ED_UI_STATE == 34 && ED_FAV_SET == 38 &&
        host_wire[n - 57] == 0 && host_wire[n - 56] == 0x55 &&
        host_wire[n - 55] == 1 && host_wire[n - 54] == 9 &&
        host_wire[n - 53] == 0x4d && host_wire[n - 52] == 1 &&
        host_wire[n - 51] == 64u && host_wire[n - 50] == 1 &&   /* (64 kept: 41 01 says 128) */
        host_wire[n - 49] == 0x42 && host_wire[n - 48] == 1 && host_wire[n - 47] == 63 &&   /* (the cloud plan's job A: bit 5, the patch store as logical records, object 40 + a slot; DUELL 0.2.15: bit 4, the patch store's chunks 6..15 as backup ids 27..31 and 35..39; DUELL 0.2.14: bit 2, multis and pattern slots in the backup; bit 3, the user wavetables 24..26 and cmds 81..83) */
        host_wire[n - 46] == 0x46 && host_wire[n - 45] == 1 && host_wire[n - 44] == FM6_NFACTORY &&
        host_wire[n - 43] == 0 &&                        /* (no bank since 1.0.3) */
        host_wire[n - 42] == 0x53 && host_wire[n - 41] == 1 && host_wire[n - 40] == 3 &&
        host_wire[n - 39] == 0x50 && host_wire[n - 38] == 1 && host_wire[n - 37] == 3 &&   /* FM6 v2: no bank, preset patches */
        host_wire[n - 36] == 0x4E && host_wire[n - 35] == 1 && host_wire[n - 34] == 33 &&   /* MENU settings: 33 items (0.2.25 hunt4: USB LEVEL, id 10, removed); was 34 items (0.2.25: DRIVE, id 43, removed; 0.2.23: + WIDTH, DRIVE, ids 42, 43; 0.2.22: + SIDECHAIN P1 CH, P1 NOTE, P2 CH, P2 NOTE, ids 38..41; 0.2.21: + COMP's THRESH, RATIO, ATTACK, RELEASE, ids 34..37; 0.2.19: OUTPUT EQ, id 9, retired; + BASS MONO, id 33; 0.2.14: + PERF LEAVE, id 32); (DUELL 0.2.2: STYLE, LARGE, FX LATCH, HELP retired too; 0.2.9: CPU METER, id 26; 0.2.11: MODE RECALL, id 27; 0.2.14: SCOPE, OVERVIEW retired, GLO > MODULES' four, ids 28..31) */
        host_wire[n - 33] == 0x52 && host_wire[n - 32] == 1 && host_wire[n - 31] == 4 &&   /* RATCH */
        host_wire[n - 30] == 0x4C && host_wire[n - 29] == 1 && host_wire[n - 28] == 1 &&   /* 1.1 parameter locks */
        host_wire[n - 27] == 0x41 && host_wire[n - 26] == 1 && host_wire[n - 25] == 0 && host_wire[n - 24] == 1 &&   /* 1.2: 128 */
        host_wire[n - 23] == 0x54 && host_wire[n - 22] == 1 && host_wire[n - 21] == NUDGE_DIV &&   /* 1.2: NUDGE */
        host_wire[n - 20] == 0x56 && host_wire[n - 19] == 1 && host_wire[n - 18] == 32 &&   /* 1.4.1: FM6 voice bank */
        host_wire[n - 17] == 0x43 && host_wire[n - 16] == 1 && host_wire[n - 15] == ML_N &&
        host_wire[n - 14] == 0x58 && host_wire[n - 13] == 1 && host_wire[n - 12] == (UP_SLOTS & 127) && host_wire[n - 11] == (UP_SLOTS >> 7) &&
        host_wire[n - 10] == 0x5A && host_wire[n - 9] == 1 && host_wire[n - 8] == 1 &&
        host_wire[n - 7] == 0x59 && host_wire[n - 6] == 1 && host_wire[n - 5] == NX &&   /* cloud plan job I: scope 2, the extension slots (NX) */   /* DUELL 0.2.19: PATCH_RAW */   /* DUELL 0.2.15: the patch capacity, 128 as two 7-bit bytes */   /* 1.5: MIDI LEARN */
        host_wire[n - 4] == 0x4B && host_wire[n - 3] == 1 && host_wire[n - 2] == 1);   /* DUELL: the MIDI MAP (cmds 85..88) */   /* DUELL 0.2.19: PATCH_RAW */   /* DUELL 0.2.15: the patch capacity, 128 as two 7-bit bytes */   /* 1.5: MIDI LEARN */
    request(ED_UI_SET, a, 2);
    bad += check("UI_SET updates the actual palette and reports RAM-only saving",
        host_wire[5] == 3 && settings.palette == 3 && T_BG == UI_PALETTES[3].bg[ux.part]);
    a[1] = NPALETTES; request(ED_UI_SET, a, 2);
    bad += check("out-of-range palette leaves the display unchanged", host_wire[5] == 1 && settings.palette == 3);
    a[0] = 1; a[1] = 1; request(ED_UI_SET, a, 2);
    bad += check("the retired font weight is not supported (rc 2), UI_STATE says 127",
        host_wire[5] == 2 && host_wire[8] == 9 && host_wire[10] == 127);
    a[0] = 2; request(ED_UI_SET, a, 2);
    bad += check("unsupported preference is reported without applying it", host_wire[5] == 2);
    a[0] = ENGI_FM6; a[1] = 0; a[2] = 64; a[3] = 1;
    request(ED_FAV_SET, a, 4);
    bad += check("FAV_SET marks FM6 as a normal engine (DUELL: DRUM is hidden: refused)", host_wire[5] == 3 && favorite_has(ENGI_FM6, 0));
    request(ED_FAV_GET, a, 4);
    bad += check("FAV_GET reads the bounded favorite range", host_wire[5] == 0 && host_wire[10] == 1);
    a[0] = NENGINES; a[1] = 127; request(ED_FAV_SET, a, 4);          /* (DUELL 0.2.17: every slot of the 128 holds a sound, the factory one or a saved one) */
    bad += check("a slot that holds its factory sound can be starred (FAV is the slot's)", host_wire[5] == 3 && favorite_has(NENGINES, 127));
    up_store(127, "Saved"); request(ED_FAV_SET, a, 4);
    bad += check("  and a saved sound in it keeps the star without changing its sound", host_wire[5] == 3 && favorite_has(NENGINES, 127));
    a[1] = 0; a[2] = 1; request(ED_FAV_SET, a, 4);                   /* (the slot past the last: refused) */
    bad += check("  a slot past the 128 cannot be starred", host_wire[5] == 1);
    a[1] = 127; a[2] = 0;
    a[3] = 32; request(ED_FAV_GET, a, 4);
    bad += check("favorite range cannot cross the end of user slots", host_wire[5] == 1);
    request(ED_FAV_SET, a, 3);
    bad += check("short favorite writes return an error without reading absent bytes", host_wire[5] == 1);
    bad += check("UI_STATE rejects unexpected request bytes", request(ED_UI_STATE, a, 1) == 0);
    a[0] = 0; n = request(ED_SONG_RETIRED, a, 1);
    bad += check("SONG (33) is retired: no reply, the id stays taken", n == 0);
    return bad;
}

static int framing(void)
{
    uint32_t i, bad = 0;
    int16_t lvl0;
    static const uint8_t rt[] = {0xF0, 0x7D, 0xF8, 0x46, 0xFE, 0x4C, ED_PING, 0xFF, 0xF7};
    static const uint8_t aborted[] = {0xF0, 0x7D, 0x46, 0x4C, ED_SET, 0, P_LEVEL, 0x90, 0, 64, 0xF7};
    reset();
    lvl0 = TSEL->p[P_LEVEL];
    for (i = 0; i < sizeof rt; i++) sysex_byte(rt[i]);
    ed_service(); host_drain();
    bad += check("realtime bytes interleaved in SysEx do not enter the editor frame",
                 host_wire_n == 7u && host_wire[4] == ED_PING && host_wire[5] == 0);
    ota_frame_done(); host_wire_n = 0;
    for (i = 0; i < sizeof aborted; i++) sysex_byte(aborted[i]);
    ed_service(); host_drain();
    bad += check("a non-realtime status aborts SysEx without changing parameters",
                 !sx_ready && !host_wire_n && TSEL->p[P_LEVEL] == lvl0);
    bad += check("a valid request works after an aborted frame", request(ED_PING, 0, 0) == 7u);
    ota_frame_done();
    midi_in_event(0x467DF004u);                          /* F0 7D 46 */
    midi_in_event(0x00004C04u);                          /* unfinished SysEx */
    midi_in_event(0x643C9009u);
    sysex_byte(0xF7);
    bad += check("a channel event in another USB packet aborts an unfinished SysEx",
                 !sx_ready && mi_w == 1u && midi_in_q[0] == 0x643C9009u);
    midi_in_event(0x643C8009u);                          /* wrong CIN */
    midi_in_event(0x643CFF09u);                          /* wrong status */
    midi_in_event(0xFF3C9009u);                          /* not a seven-bit velocity */
    midi_in_event(0x64FF9009u);                          /* not a seven-bit note */
    bad += check("USB channel packets reject mismatched CIN and high data bits", mi_w == 1u);
    midi_in_event(0xFF05C00Cu);                          /* one-byte status: padding is ignored */
    midi_in_event(0xFF07D00Du);
    bad += check("USB program and channel pressure keep their one-byte payload", mi_w == 3u);
    mi_w = UINT32_MAX - 5u; mi_r = mi_w;
    for (i = 0; i < MQ + 8u; i++) midi_in_event(0x643C9009u);
    bad += check("USB MIDI ring stops at capacity across counter wrap", mi_w - mi_r == MQ && midi_in_overflow);
    mi_r = mi_w; midi_in_event(0x643C9009u);
    bad += check("USB rejects new messages until the audio consumer clears overflow", mi_w == mi_r);
    midi_in_overflow = 0; midi_in_event(0x643C9009u);
    bad += check("USB resumes after the audio consumer clears overflow", mi_w - mi_r == 1u);
    return bad;
}

static int uart_recovery(void)
{
    uint32_t i;
    int bad = 0;
    reset();
    um_byte(0x90); um_byte(50);
    for (i = 0; i < UM_RING; i++) um_ring[i] = 0;
    um_ring[126] = 0x90; um_ring[127] = 72; um_ring[0] = 99;
    uart_midi_take(UM_RING + 1u);
    bad += check("UART DMA overrun never combines a stale partial note with new data",
                 !mi_w && midi_in_overflow && um.drops == 2u && um.bytes == UM_RING && !um.pend);
    midi_in_overflow = 0; um_byte(0x90); um_byte(72); um_byte(99);
    bad += check("UART receives a complete fresh note after DMA recovery",
                 mi_w == 1u && midi_in_q[0] == 0x63489009u);
    reset();
    um_byte(0x90); um_byte(50);
    for (i = 0; i < UM_RING; i++) um_ring[i] = 60;
    uart_midi_take(UM_RING + 2u);
    bad += check("UART discards running-status data after lost bytes until a new status",
                 !mi_w && midi_in_overflow && um.drops == 2u && !um.st && !um.got);
    midi_in_overflow = 0;
    um_byte(0x91); um_byte(70); um_byte(100);
    bad += check("UART recovers immediately on a complete channel message", mi_w == 1u && midi_in_q[0] == 0x64469109u);
    reset(); mi_w = MQ;
    um_byte(0x90); um_byte(60); um_byte(100);
    bad += check("UART queue overflow latches the same recovery flag as USB", midi_in_overflow && um.drops == 1u && mi_w == MQ);
    mi_r = mi_w; um_byte(61); um_byte(101);
    bad += check("UART rejects a new stream until audio clears overflow", mi_w == mi_r && um.drops == 2u);
    midi_in_overflow = 0; um_byte(62); um_byte(102);
    bad += check("UART resumes valid running status after audio clears overflow",
                 mi_w - mi_r == 1u && midi_in_q[mi_r % MQ] == 0x663E9009u);
    return bad;
}

static int steps(void)
{
    uint8_t a[80] = {0, 1, 60, 0, 0, 0, ST_NOTE, SF_ACCENT, 100, 0x12, 0x02, 1};
    step_t before;
    uint32_t n, ok = 1;
    int bad = 0;
    reset();
    bad += check("legacy 8-byte step writes take the note",
                 request(ED_STEP_SET, a, 9) == 21u && TSEL->step[0].note[0] == 60);
    bad += check("full step writes (0.2.14: the three lane bytes are read and dropped) take the note",
                 request(ED_STEP_SET, a, 12) == 21u && TSEL->step[0].note[0] == 60);
    before = TSEL->step[0]; a[2] = 71;
    for (n = 2; n <= sizeof a; n++) {
        if (n == 9u || n == 12u || n == 13u) continue;   /* (14: a[13], the ratchet, 0 is refused) */
        ok &= !request(ED_STEP_SET, a, n) && !memcmp(&before, &TSEL->step[0], sizeof before);
    }
    bad += check("partial or oversized step payloads never mutate a valid step", ok);
    a[0] = 1; a[1] = 0; memcpy(a + 2, (const uint8_t[]){1,64,0,0,0,ST_NOTE,0,99}, 8);
    bad += check("TRACK_STEP accepts its legacy payload on an unselected track",
                 request(ED_TRACK_STEP, a, 10) == 22u && trk[1].step[0].note[0] == 64 && song.sel == 0);
    before = trk[1].step[0]; a[3] = 65;
    bad += check("TRACK_STEP rejects an incomplete grid extension",
                 !request(ED_TRACK_STEP, a, 11) && !memcmp(&before, &trk[1].step[0], sizeof before));
    /* RATCH (INFO 52 01 04): the hits 1..4 after the chance; the flags byte stays accent | slide both ways */
    memcpy(a, (const uint8_t[]){0, 1, 60, 0, 0, 0, ST_NOTE, SF_ACCENT, 100, 0, 0, 0, 80, 3}, 14);
    n = request(ED_STEP_SET, a, 14);
    bad += check("STEP_SET with the ratchet: x3 kept, replied after the chance, flags without it",
                 n == 21u && step_ratchet(&TSEL->step[0]) == 3u && step_chance(&TSEL->step[0]) == 80u &&
                 host_wire[n - 3] == 3 && host_wire[n - 4] == 80 && host_wire[n - 2] == 8 && host_wire[12] == SF_ACCENT);
    a[7] = SF_SLIDE; n = request(ED_STEP_SET, a, 13);
    ok = n == 21u && step_ratchet(&TSEL->step[0]) == 3u && TSEL->step[0].flags == (SF_SLIDE | 2u << SF_RATCH_SH);
    n = request(ED_STEP_SET, a, 9);
    bad += check("STEP_SET without the ratchet (an older editor) keeps the step's own",
                 ok && n == 21u && step_ratchet(&TSEL->step[0]) == 3u);
    before = TSEL->step[0]; ok = 1;
    for (n = 0; n < 8u; n++) {
        a[13] = (uint8_t)(n < 4u ? 0u : 5u + n);
        ok &= !request(ED_STEP_SET, a, 14) && !memcmp(&before, &TSEL->step[0], sizeof before);
    }
    bad += check("STEP_SET refuses a ratchet outside 1..4 and leaves the step", ok);
    memcpy(a, (const uint8_t[]){1, 2, 1, 64, 0, 0, 0, ST_NOTE, 0, 99, 0, 0, 0, 100, 4}, 15);
    n = request(ED_TRACK_STEP, a, 15);
    ok = n == 22u && step_ratchet(&trk[1].step[2]) == 4u && host_wire[n - 3] == 4;
    a[14] = 1; n = request(ED_TRACK_STEP, a, 15);
    bad += check("TRACK_STEP sets the ratchet of any track (x4, then back to x1)",
                 ok && n == 22u && step_ratchet(&trk[1].step[2]) == 1u && !(trk[1].step[2].flags & SF_RATCH));
    /* NUDGE (1.2, INFO 54 01 16): the nudge + 8 after the ratchet, both ways; a write without it keeps the step's */
    memcpy(a, (const uint8_t[]){0, 1, 60, 0, 0, 0, ST_NOTE, SF_ACCENT, 100, 0, 0, 0, 80, 1, 8 - 5}, 15);
    n = request(ED_STEP_SET, a, 15);
    ok = n == 21u && step_nudge(&TSEL->step[0]) == -5 && host_wire[n - 2] == 3 && host_wire[12] == SF_ACCENT &&
         (TSEL->step[0].flags & (SF_ACCENT | SF_SLIDE)) == SF_ACCENT;
    a[7] = SF_SLIDE; n = request(ED_STEP_SET, a, 14);
    ok &= n == 21u && step_nudge(&TSEL->step[0]) == -5 && host_wire[12] == SF_SLIDE;
    n = request(ED_STEP_SET, a, 9);
    ok &= step_nudge(&TSEL->step[0]) == -5;
    bad += check("STEP_SET with the nudge (-5): kept, replied + 8 after the ratchet; a write without it keeps it", ok);
    before = TSEL->step[0];
    a[14] = 16;
    bad += check("STEP_SET refuses a nudge byte above 15 and leaves the step",
                 !request(ED_STEP_SET, a, 15) && !memcmp(&before, &TSEL->step[0], sizeof before));
    memcpy(a, (const uint8_t[]){1, 7, 1, 64, 0, 0, 0, ST_NOTE, 0, 99, 0, 0, 0, 100, 1, 15}, 16);
    n = request(ED_TRACK_STEP, a, 16);
    bad += check("TRACK_STEP sets the nudge of any track (+7)", n == 22u && step_nudge(&trk[1].step[7]) == 7 &&
                 host_wire[n - 2] == 15);
    return bad;
}

/* DUELL 0.2.9: no user sample slots (the sample engines are retired). The commands keep their numbers and reply shapes and never
 * touch flash or the transport: BEGIN / WRITE / END rc 6 (unsupported), ERASE rc 0 (nothing to erase), INFO no slots */
static int samples(void)
{
    uint8_t a[640] = {0}, data[256];
    uint32_t n, i;
    int bad = 0;
    reset(); memset(host_samples, 0, sizeof host_samples);
    for (i = 0; i < sizeof data; i++) data[i] = (uint8_t)(i * 37u);
    song.playing = 1;
    bad += check("SMP_BEGIN: rc 6 (unsupported), no erase, the transport left alone",
                 request(ED_SMP_BEGIN, a, 1) == 8u && host_wire[5] == 0 && host_wire[6] == 6 && !host_erases && song.playing);
    a[0] = 1; a[1] = 0; a[2] = 4; a[3] = 0;
    n = pack7(data, 256, a + 4) + 4u;
    bad += check("SMP_WRITE: rc 6, the offset echoed, nothing written",
                 request(ED_SMP_WRITE, a, n) == 11u && host_wire[5] == 1 && host_wire[7] == 4 && host_wire[9] == 6 && !host_writes &&
                 song.playing);
    a[0] = 0; n = pack7(data, 200, a + 1) + 1u;
    bad += check("SMP_END: rc 6, nothing written", request(ED_SMP_END, a, n) == 8u && host_wire[6] == 6 && !host_writes);
    a[0] = 2;
    bad += check("SMP_ERASE (an archive's empty slot, USR3 too): rc 0, nothing erased",
                 request(ED_SMP_ERASE, a, 1) == 8u && host_wire[5] == 2 && host_wire[6] == 0 && !host_erases && song.playing);
    a[0] = 3;
    bad += check("  a slot past USR3: no reply", request(ED_SMP_ERASE, a, 1) == 0u);
    bad += check("SMP_INFO: no slots", request(ED_SMP_INFO, a, 0) == 8u && host_wire[5] == 0 && host_wire[6] == 0);
    for (i = 0; i < sizeof host_samples; i++)
        if ((&host_samples[0][0])[i])
            break;
    bad += check("the old sample region is untouched", i == sizeof host_samples && !host_erases && !host_writes);
    host_progress = 1;
    song.playing = 1; host_progress = 0; a[0] = 0; a[1] = 0;
    bad += check("user-preset STORE does not mutate RAM when audio cannot stop",
                 request(ED_UP_STORE, a, 2) == 8u && host_wire[6] == 2 && !up_used(0));
    song.playing = 0; up_store(0, "SAVED"); song.playing = 1;
    bad += check("slot RESTORE (UP_ERASE) of a saved slot reports failure when audio cannot stop, and keeps it",
                 request(ED_UP_ERASE, a, 1) == 8u && host_wire[6] == 2 && up_used(0));
    bad += check("  a slot that holds its factory sound already answers rc 0 and writes nothing (even playing)",
                 (a[0] = 1, request(ED_UP_ERASE, a, 1) == 8u) && host_wire[6] == 0 && !up_used(1));
    song.playing = 0; up_put(0, 0);
    return bad;
}

/* (DUELL: the song protocol went with the song mode: ED_SONG_RETIRED keeps the id) */
static int malformed_saves(void)
{
    static const uint8_t slot[] = {1, 16}, name[] = {0, 'X', 0, 'Y'};
    const uint8_t save[] = {1, 0};
    int bad = 0;
    reset();
    bad += check("PROJECT refuses an invalid slot instead of wrapping to slot 0",
                 !request(ED_PROJECT, slot, sizeof slot) && !project_used(0));
    bad += check("UP_STORE rejects bytes after its name terminator without saving",
                 request(ED_UP_STORE, name, sizeof name) == 8u && host_wire[6] == 1 && !up_used(0));
    bad += check("a successful PROJECT save confirms its slot", request(ED_PROJECT, save, sizeof save) == 9u && project_used(0));
    TSEL->p[P_LEVEL] = 21; song.playing = 1; host_progress = 0;
    bad += check("PROJECT never confirms a used slot when STOP timed out (DUELL: the slot is a MULTI)",
                 !request(ED_PROJECT, save, sizeof save));
    return bad;
}

/* FM6 patches (cmds 68..71): a track's own patch, the retired bank ("no bank"), the factory patches, the user presets'
 * patches (target 3), malformed frames */
static int fm6_patches(void)
{
    int bad = 0;
    uint8_t a[2 + FM6_PACKED], pk[FM6_PACKED];
    uint32_t n, i;
    reset();
    a[0] = ED_FM6_FACTORY; a[1] = 3;
    n = request(ED_FM6_GET, a, 2);
    bad += check("FM6_GET factory 4: the packed record", n == 5u + 3u + FM6_PACKED + 1u && host_wire[7] == 0 &&
                 !memcmp(host_wire + 8, FM6_FACTORY[3], FM6_PACKED));
    memcpy(pk, FM6_FACTORY[3], FM6_PACKED);
    memcpy(pk + 118, "MY PATCH  ", 10);
    trk[1].eng_req = ENGI_FM6;
    trk[1].p[P_E7] = 3;
    a[0] = ED_FM6_TRACK; a[1] = 1; memcpy(a + 2, pk, FM6_PACKED);
    n = request(ED_FM6_PUT, a, sizeof a);
    bad += check("FM6_PUT part B: rc 0, the track's patch, SLOT OWN (not F4: the name differs)", n == 9u &&
                 host_wire[7] == 0 && !memcmp(fm6_patch[1] + FP_NAME, "MY PATCH  ", 10) &&
                 trk[1].p[P_E7] == FM6_OWN && fm6_slot[1] == FM6_OWN);
    fm6_poll();
    bad += check("  .. the main loop keeps it", !memcmp(fm6_patch[1] + FP_NAME, "MY PATCH  ", 10));
    a[1] = 1;
    n = request(ED_FM6_GET, a, 2);
    bad += check("FM6_GET part B: as sent", host_wire[7] == 0 && !memcmp(host_wire + 8, pk, FM6_PACKED));
    a[0] = ED_FM6_TRACK; a[1] = 1; memcpy(a + 2, FM6_FACTORY[5], FM6_PACKED);
    request(ED_FM6_PUT, a, sizeof a);
    bad += check("FM6_PUT of a factory patch unchanged: SLOT shows it (F6)", trk[1].p[P_E7] == 5 && fm6_slot[1] == 5u);
    trk[1].p[P_E7] = FM6_OWN;                            /* (back to MY PATCH for the SLOT trip below) */
    a[0] = ED_FM6_TRACK; a[1] = 1; memcpy(a + 2, pk, FM6_PACKED);
    request(ED_FM6_PUT, a, sizeof a);
    trk[1].p[P_E7] = 1;
    fm6_poll();
    bad += check("SLOT OWN -> F2 loads the factory patch", !memcmp(fm6_patch[1] + FP_NAME, FM6_FACTORY[1] + 118, 10));
    trk[1].p[P_E7] = 6;
    fm6_poll();
    trk[1].p[P_E7] = FM6_OWN;
    fm6_poll();
    bad += check("  .. and back to OWN brings the own patch back", !memcmp(fm6_patch[1] + FP_NAME, "MY PATCH  ", 10) &&
                 fm6_slot[1] == FM6_OWN);
    trk[1].p[P_E7] = 20;                                 /* (a 1.0.2 B slot that escaped a clamp) */
    fm6_poll();
    bad += check("a SLOT past OWN is OWN: the patch stays, nothing reloads", trk[1].p[P_E7] == FM6_OWN &&
                 !memcmp(fm6_patch[1] + FP_NAME, "MY PATCH  ", 10));
    for (i = 0; i < 3u; i++) {                           /* the bank: GET, PUT, ERASE answer "no bank" (3) */
        a[0] = ED_FM6_BANK; a[1] = 4; memcpy(a + 2, pk, FM6_PACKED);
        if (i < 2u)
            request(i ? ED_FM6_PUT : ED_FM6_GET, a, i ? sizeof a : 2u);
        else {
            a[0] = 4;
            request(ED_FM6_ERASE, a, 1);
        }
        bad += check(i == 0u ? "FM6_GET of the bank: rc 3, no bank" : i == 1u ? "FM6_PUT to the bank: rc 3, no bank" :
                               "FM6_ERASE: rc 3, no bank", host_wire[i == 2u ? 6 : 7] == 3u);
    }
    n = request(ED_FM6_LIST, a, 0);
    {   /* factory 8, bank 0, then used + name per factory slot */
        uint32_t p = 7, k, ok = host_wire[5] == FM6_NFACTORY && host_wire[6] == 0;
        for (k = 0; k < FM6_NFACTORY && p < n; k++) {
            ok &= host_wire[p++] == 1u;
            while (host_wire[p]) p++;
            p++;
        }
        bad += check("FM6_LIST: 8 factory names, nbank 0", ok && k == FM6_NFACTORY && p == n - 1u);
    }
    /* target 3: an FM6 user preset's patch */
    song.sel = 1;
    up_store(5, "MINE");                                 /* part B (MY PATCH, OWN) -> slot 6 */
    a[0] = ED_FM6_USER; a[1] = 5;
    request(ED_FM6_GET, a, 2);
    bad += check("UP_STORE of an FM6 track: FM6_GET user 6 gives its patch", host_wire[7] == 0 &&
                 !memcmp(host_wire + 8 + 118, "MY PATCH  ", 10));
    memcpy(a + 2, pk, FM6_PACKED);
    memcpy(a + 2 + 118, "EDITOR    ", 10);
    a[2 + 14] = 120;                                     /* OP6 output level 120: stored as 99 */
    request(ED_FM6_PUT, a, sizeof a);
    i = host_wire[7];
    request(ED_FM6_GET, a, 2);
    bad += check("FM6_PUT user 6, then GET: stored in range (a level of 120 -> 99)", i == 0u && host_wire[7] == 0 &&
                 host_wire[8 + 14] == 99 && !memcmp(host_wire + 8 + 118, "EDITOR    ", 10));
    fm6_load_slot(1, 0);
    up_load(5);
    bad += check("  .. UP_LOAD 6 plays it, SLOT OWN", !memcmp(fm6_patch[1] + FP_NAME, "EDITOR    ", 10) &&
                 trk[1].p[P_E7] == FM6_OWN);
    up_rename(5, "RENAMED");
    a[0] = ED_FM6_USER; a[1] = 5;
    request(ED_FM6_GET, a, 2);
    bad += check("  .. a rename keeps it", host_wire[7] == 0 && !memcmp(host_wire + 8 + 118, "EDITOR    ", 10));
    trk[0].eng_req = 0;
    song.sel = 0;
    up_store(6, "KIT");
    a[0] = ED_FM6_USER; a[1] = 6; memcpy(a + 2, pk, FM6_PACKED);
    request(ED_FM6_GET, a, 2);
    i = host_wire[7];
    request(ED_FM6_PUT, a, sizeof a);
    bad += check("an ANALOG user preset has no patch: GET rc 2, PUT rc 1", i == 2u && host_wire[7] == 1u);
    a[1] = 9;                                            /* an empty slot */
    request(ED_FM6_PUT, a, sizeof a);
    i = host_wire[7];
    a[1] = UP_SLOTS;
    request(ED_FM6_GET, a, 2);
    bad += check("an empty user slot: PUT rc 1; past the slots: GET rc 1", i == 1u && host_wire[7] == 1u);
    up_put(5, 0);
    a[1] = 5;
    request(ED_FM6_GET, a, 2);
    bad += check("an erased preset's patch is gone (GET rc 2)", host_wire[7] == 2u);
    a[0] = ED_FM6_TRACK; a[1] = 4;
    request(ED_FM6_PUT, a, sizeof a);
    i = host_wire[7];
    request(ED_FM6_PUT, a, 20);
    bad += check("FM6_PUT: a fifth track or a short record: rc 1", i == 1u && host_wire[7] == 1u);
    a[0] = 4; a[1] = 0;
    request(ED_FM6_GET, a, 2);
    bad += check("FM6_GET of an unknown target: rc 1", host_wire[7] == 1u);
    return bad;
}

/* UP_PUT -> UP_GET: every value round-trips through the v4 record (a byte each), negative ones too */
static int user_preset_roundtrip(void)
{
    static uint8_t a[16 + 2u * P_COUNT + 32u + 18u];
    int16_t want[P_COUNT], got[P_COUNT];
    uint32_t k = 0, i, p, ok, n;
    int bad = 0;
    reset();
    a[k++] = 3; a[k++] = 0;                                /* slot U04, ANALOG */
    memcpy(a + k, "ROUND", 5); k += 5; a[k++] = 0;
    for (i = 0; i < P_COUNT; i++) {
        const param_desc_t *d = param_desc_of(0, i);
        int16_t v = d->def;
        uint32_t u;
        if (i == P_TRIM) v = 90;                              /* (0.2.19: the patch's loudness; the mixer level, P_LEVEL, is no patch's: below) */
        if (i == P_PAN) v = -20;
        if (i == P_ED_FLT) v = -40;
        if (i == P_LD_FLT) v = d->min;
        if (i == P_E1) v = d->max;
        want[i] = v; u = (uint32_t)(v + 8192);
        a[k++] = u & 127u; a[k++] = (u >> 7) & 127u;
    }
    for (i = 0; i < 16u; i++) { a[k++] = i & 1u ? 0u : (uint8_t)(48u + i); a[k++] = 0; }
    want[P_LEVEL] = (int16_t)up_def(0, P_LEVEL);              /* a patch holds no mixer level: the sent 112 is not stored, the record reads its stored default */
    request(ED_UP_PUT, a, k);
    bad += check("UP_PUT stores a v4 record (no flash here: kept in RAM)",
                 host_wire[6] != 1u && up_used(3) && up_rec(3)->ver == UP_VER);
    up_values(up_rec(3), got);
    for (i = 0, ok = 1; i < P_COUNT; i++) ok &= got[i] == want[i];
    bad += check("UP_PUT keeps every value (PAN -20, FLT -40, min and max)", ok);
    a[0] = 3;
    request(ED_UP_GET, a, 1);
    ok = host_wire[5] == 3 && host_wire[6] == 1 && host_wire[7] == 0 && !memcmp(host_wire + 8, "ROUND", 6);
    for (i = 0, p = 14; i < P_COUNT; i++, p += 2u)
        ok &= (int32_t)(host_wire[p] | host_wire[p + 1] << 7) - 8192 == want[i];
    for (i = 0; i < 16u; i++, p += 2u)
        ok &= host_wire[p] == (i & 1u ? 0u : 48u + i);
    bad += check("UP_GET returns the values and the pattern UP_PUT sent", ok);
    bad += check("  .. then kind 0 and (1.2) the category: none sent, ANALOG's (OTHER)",
                 host_wire[p] == 0u && host_wire[p + 1u] == CAT_OTHER && host_wire[p + 2u] == 1u && host_wire[p + 3u] == 0xF7);   /* (0.2.17: then SAVED 1) */
    {   /* 1.2 (Discussion #90): UP_PUT's category after the kind's 16 bytes; UP_LIST lists the categories after the slots */
        uint32_t k0 = k;
        a[k++] = 0;
        for (i = 0; i < 16u; i++) a[k++] = 0;
        a[k++] = CAT_PAD;
        a[0] = 3;
        request(ED_UP_PUT, a, k);
        ok = host_wire[6] != 1u && up_cat(3) == CAT_PAD;
        a[k - 1u] = CAT_N;
        request(ED_UP_PUT, a, k);
        ok &= host_wire[6] == 1u && up_cat(3) == CAT_PAD;
        a[0] = 3;
        request(ED_UP_GET, a, 1);
        ok &= host_wire[p] == 0u && host_wire[p + 1u] == CAT_PAD;
        a[0] = 2; a[1] = 2;
        n = request(ED_UP_LIST, a, 2);
        ok &= host_wire[5] == 2u && host_wire[6] == 2u && host_wire[n - 3u] == 0u && host_wire[n - 2u] == 1u && host_wire[n - 4u] == CAT_PAD;   /* (the categories, then SAVED: slot 2 factory 0, slot 3 saved 1) */
        bad += check("UP_PUT with a category (after the kind's 16 bytes; past the last: rc 1), UP_GET and UP_LIST give it", ok);
        k = k0;
    }
    return bad;
}

/* the frames of cmd in host_wire: how many, and the args of the last (into *args) */
static uint32_t wire_frames(uint32_t cmd, const uint8_t **args)
{
    uint32_t i, k = 0;
    for (i = 0; i + 5u < host_wire_n; i++)
        if (host_wire[i] == 0xF0 && host_wire[i + 1] == ED_HDR0 && host_wire[i + 4] == cmd) {
            k++;
            if (args) *args = host_wire + i + 5;
        }
    return k;
}
static uint32_t sync_pass(void)                          /* one main-loop pass of the pushes */
{
    host_wire_n = 0;
    fm1_ms += 30u;
    ed_sync();
    host_drain();
    return host_wire_n;
}

/* #65: WATCH while watching keeps what is not pushed yet; the editor's own sound load is not echoed as RELOAD */
static int live_sync(void)
{
    int bad = 0;
    uint8_t a[4] = {1, 0, 0, 0};
    uint32_t i, other = 0;
    const uint8_t *x = 0;
    reset();
    usb.resets = 0;
    request(ED_WATCH, a, 1);
    bad += check("WATCH 1 starts watching", host_wire[5] == 1 && ed_w.on && !sync_pass());
    TSEL->p[P_LEVEL] = 77;                               /* a device change, not pushed yet */
    request(ED_WATCH, a, 1);
    bad += check("WATCH 1 while watching keeps a pending change: CHANGED still goes out",
                 sync_pass() && wire_frames(ED_CHANGED, &x) == 1u && x[1] == P_LEVEL && ed_rv(x + 2) == 77);
    TSEL->p[P_LEVEL] = 66;
    a[0] = 0; request(ED_WATCH, a, 1);
    a[0] = 1; request(ED_WATCH, a, 1);
    bad += check("WATCH 0 then WATCH 1 starts from the values as they are (no push)", !sync_pass());
    TSEL->p[P_LEVEL] = 55; trk[1].p[P_PAN] = 3;
    a[0] = 3; request(ED_WATCH, a, 1);
    sync_pass();
    bad += check("WATCH 3 while watching: CHANGED kept, TRACK_CHANGED from the mix as it is",
                 host_wire_n && wire_frames(ED_CHANGED, 0) == 1u && !wire_frames(ED_TRACK_CHANGED, 0) && ed_w.v4);
    usb.resets++;
    TSEL->p[P_LEVEL] = 44;
    request(ED_WATCH, a, 1);
    bad += check("WATCH after a USB reset starts over", !sync_pass());

    /* PRESET */
    TSEL->p[P_PAN] = -20;                                /* pending; a sound load keeps P_PAN and (0.2.19) the mixer P_LEVEL: it sets the trim, P_TRIM */
    a[0] = 0; a[1] = 2;
    request(ED_PRESET, a, 2);
    sync_pass();
    bad += check("the editor's own PRESET: no RELOAD, no CHANGED for what it loaded, a pending CHANGED stays",
                 TSEL->preset == 2 && !wire_frames(ED_RELOAD, 0) && wire_frames(ED_CHANGED, &x) == 1u &&
                 x[1] == P_PAN && !sync_pass());
    for (i = 0; i < 3u; i++) {                           /* any preset of any engine */
        a[0] = (uint8_t)(i ? 12u : 4u); a[1] = (uint8_t)i;
        request(ED_PRESET, a, 2);
        other += sync_pass() != 0;
    }
    bad += check("PRESET of other engines: no push at all", !other && ed_w.eng == ed_eng(TSEL));
    a[0] = 1; a[1] = G_ENGSEL; a[2] = (8192 + 5) & 127; a[3] = (8192 + 5) >> 7;
    request(ED_SET, a, 4);
    bad += check("SET of G_ENGSEL: no RELOAD", ed_eng(TSEL) == 5u && !sync_pass());
    sync_reload = 1;                                     /* a load on the device, not pushed yet */
    a[0] = 0; a[1] = 1;
    request(ED_PRESET, a, 2);
    sync_pass();
    bad += check("a RELOAD due before the editor's PRESET still goes out", wire_frames(ED_RELOAD, 0) == 1u);
    a[0] = 0; request(ED_WATCH, a, 1);
    sync_reload = 0;
    a[0] = 0; a[1] = 3;
    request(ED_PRESET, a, 2);
    a[0] = 1; request(ED_WATCH, a, 1);
    bad += check("not watching: PRESET as before, WATCH then starts from it", !sync_pass() && TSEL->preset == 3);
    return bad;
}

/* #64: SysEx through the USB packet path (ep1_take) at full speed: a 256-byte BACKUP_PUT piece (293 pack7 bytes, 101
 * event packets, 7 USB packets) arrives whole; one request at a time always works, a frame sent before the reply
 * to the one before is dropped whole (no reply; never a partial frame) */
static uint32_t usb_frame(const uint8_t *f, uint32_t n, uint8_t *usbp)   /* F0..F7 -> USB-MIDI event packets */
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        uint32_t k = n - i >= 3u ? 3u : n - i, cin = k == 3u && i + 3u < n ? 4u : k == 3u ? 7u : 4u + k;
        usbp[o++] = (uint8_t)cin;
        usbp[o++] = f[i];
        usbp[o++] = k > 1u ? f[i + 1] : 0;
        usbp[o++] = k > 2u ? f[i + 2] : 0;
        i += k;
    }
    return o;
}
static uint32_t usb_feed(const uint8_t *p, uint32_t n)   /* whole 64-byte USB packets, as the host sends them */
{
    uint32_t o, refused = 0;
    for (o = 0; o < n; o += 64u)
        while (!ep1_take(p + o, n - o > 64u ? 64u : n - o))
            refused++;                                  /* NAK: the host sends it again */
    return refused;
}
static uint32_t put_frame(uint8_t *f, uint32_t op, uint32_t off, uint32_t count)
{
    static const uint8_t zero[256];
    uint32_t n = 0, i;
    f[n++] = 0xF0; f[n++] = ED_HDR0; f[n++] = ED_HDR1; f[n++] = ED_HDR2; f[n++] = ED_BACKUP_PUT;
    f[n++] = (uint8_t)op; f[n++] = 2;
    if (op == 0u) {
        for (i = 0; i < 5u; i++) f[n++] = (uint8_t)((sizeof(project_store_t) >> (7u * i)) & (i == 4u ? 15u : 127u));
        for (i = 0; i < 5u; i++) f[n++] = 0;
    } else if (op == 1u) {
        for (i = 0; i < 5u; i++) f[n++] = (uint8_t)((off >> (7u * i)) & (i == 4u ? 15u : 127u));
        n += pack7(zero, count, f + n);
    }
    f[n++] = 0xF7;
    return n;
}
static int usb_burst(void)
{
    static uint8_t f[700], u[1000], u2[2000];
    uint32_t off, n, k, ok = 1, max = 0;
    const uint8_t *x = 0;
    int bad = 0;
    reset();
    usb.rx_pend = 0;
    n = put_frame(f, 0, 0, 0);
    usb_feed(u, usb_frame(f, n, u)); host_wire_n = 0; ed_service(); host_drain();
    bad += check("BACKUP_PUT begin through ep1_take", wire_frames(ED_BACKUP_PUT, &x) == 1u && !x[2]);
    for (off = 0; off < sizeof(project_store_t); off += 256u) {
        uint32_t c = sizeof(project_store_t) - off > 256u ? 256u : sizeof(project_store_t) - off;
        n = put_frame(f, 1, off, c);
        if (n > max) max = n;
        k = usb_frame(f, n, u);
        usb_feed(u, k);
        host_wire_n = 0; ed_service(); host_drain();
        ok &= wire_frames(ED_BACKUP_PUT, &x) == 1u && !x[2] && sx_ready == 0;
    }
    bad += check("256-byte pieces in 64-byte USB packets at full speed: every piece taken (rc 0)",
                 ok && ed_bk_pos == sizeof(project_store_t) && max == 4u + 1u + 7u + 293u + 1u && max <= sizeof sx_frame);
    n = put_frame(f, 3, 0, 0);
    usb_feed(u, usb_frame(f, n, u)); host_wire_n = 0; ed_service(); host_drain();
    bad += check("abort through ep1_take", wire_frames(ED_BACKUP_PUT, &x) == 1u && !x[2] && !ed_bk_put);

    /* two frames back to back, before the reply: the second is dropped whole, the first answered intact */
    n = put_frame(f, 0, 0, 0);
    usb_feed(u, usb_frame(f, n, u)); host_wire_n = 0; ed_service(); host_drain();
    k = usb_frame(f, put_frame(f, 1, 0, 256), u2);
    k += usb_frame(f, put_frame(f, 1, 256, 256), u2 + k);
    usb_feed(u2, k);
    host_wire_n = 0; ed_service(); ed_service(); host_drain();
    bad += check("pipelined: the first piece is taken, the second (sent before its reply) dropped whole",
                 wire_frames(ED_BACKUP_PUT, &x) == 1u && !x[2] && ed_bk_pos == 256u && !sx_ready);
    n = put_frame(f, 1, 512, 256);
    usb_feed(u, usb_frame(f, n, u)); host_wire_n = 0; ed_service(); host_drain();
    bad += check("... so the next piece is refused (rc 1, offset), as a host that waits never sees",
                 wire_frames(ED_BACKUP_PUT, &x) == 1u && x[2] == 1u && ed_bk_pos == 256u);
    request(ED_BACKUP_PUT, (const uint8_t[]){3, 2}, 2);
    return bad;
}

/* ---- MENU_DESC / MENU_SET (72, 73): the menu's settings over the editor ---- */
typedef struct { uint32_t index, id, kind, nnames, tab, rest; int32_t value, min, max; char name[16], names[20][12], tabname[16], unit[16]; } menu_item_t;
static uint32_t menu_desc(uint32_t index, menu_item_t *it)   /* the reply's payload length; it parsed */
{
    uint8_t a[1] = {(uint8_t)index};
    uint32_t n = request(ED_MENU_DESC, a, 1), p = 14, k;
    memset(it, 0, sizeof *it);
    if (n < 8u || host_wire[4] != ED_MENU_DESC || host_wire[n - 1] != 0xF7) return 0;
    it->index = host_wire[5]; it->id = host_wire[6];
    if (n < 15u) return n - 6u;
    it->kind = host_wire[7];
    it->value = ed_rv(host_wire + 8); it->min = ed_rv(host_wire + 10); it->max = ed_rv(host_wire + 12);
    for (k = 0; p < n - 1u && host_wire[p] && k < 15u; ) it->name[k++] = (char)host_wire[p++];
    p++;
    if (it->kind == 1u || it->kind == 2u) {                 /* (a number, 0.2.22: or a number in tenths: its unit, then the tab) */
        for (k = 0; p < n - 1u && host_wire[p] && k < 15u; ) it->unit[k++] = (char)host_wire[p++];
        p++;
    }
    while (it->kind == 0u && p < n - 1u && it->nnames < 20u && (int32_t)it->nnames < it->max - it->min + 1) {   /* (kind 0: max - min + 1) */
        for (k = 0; p < n - 1u && host_wire[p] && k < 11u; ) it->names[it->nnames][k++] = (char)host_wire[p++];
        p++; it->nnames++;
    }
    it->tab = 127;                                          /* 1.0.5: then the row's tab, index and name */
    if (p < n - 1u) {
        it->tab = host_wire[p++];
        for (k = 0; p < n - 1u && host_wire[p] && k < 15u; ) it->tabname[k++] = (char)host_wire[p++];
        p++;
    }
    it->rest = p < n - 1u ? n - 1u - p : 0u;                /* (bytes left over: none) */
    return n - 6u;
}
static uint32_t menu_set(uint32_t id, int32_t v)            /* -> rc; host_wire[6], [7..8]: id, value */
{
    uint8_t a[3] = {(uint8_t)id, (uint8_t)((v + 8192) & 127), (uint8_t)(((v + 8192) >> 7) & 127)};
    uint32_t n = request(ED_MENU_SET, a, 3);
    return n == 10u && host_wire[4] == ED_MENU_SET ? host_wire[5] : 99u;
}
static int menu_protocol(void)
{
    /* DUELL 0.2.14: the new names (ANIMATION, BUTTON HOLD, OUTPUT EQ, STEP KEYS WRITE / ADD, MULTI LOAD KEEPS MODE / SETS MODE), SCALE LEDS in
     * DISPLAY, SCOPE (18) and OVERVIEW (23) retired, HOME OPENS without OVERVIEW; a fresh unit's defaults: KNOB ACCEL ON, USB SERIAL OFF,
     * CLICK REC (settings_persist.c settings_defaults, as persist_boot runs it on a unit with no settings) */
#define MDN 33u
    static const char *const WANT[MDN][2] = {
        {"COLOR", 0}, {"ANIMATION", "ON,OFF,IDLE"}, {"LEDS", "OFF,LOW,MEDIUM,MAX"},
        {"BUTTON HOLD", "0.3 s,0.4 s,0.5 s,0.6 s"}, {"KNOB ACCEL", "OFF,ON"},
        {"USB SERIAL", "ON,OFF"},                                                                      /* (USB LEVEL, id 10, retired in DUELL 0.2.25) */
        {"CLICK", "OFF,REC,ON"}, {"CLICK LEVEL", "LOW,MID,HIGH"}, {"COUNT-IN", "OFF,1 BAR,2 BARS"},   /* (1.1: appended) */
        {"RESTORE LAST", "ON,OFF"}, {"SCALE LEDS", "AUTO,OFF"},                                        /* (1.2) */
        {"SCREEN OFF", "NEVER,5 MIN,15 MIN,30 MIN,60 MIN"},
        {"STEP PREVIEW", "OFF,ON"}, {"STEP KEYS", "WRITE,ADD"},                                        /* (1.2; 0.2.14: SCOPE retired) */
        {"TUNE", 0},   /* (1.2: the project's TUNE, a number; DUELL 0.1: MIDI IN, id 21, left MENU for GLO > CHANNELS) */
        {"HOME OPENS", "MORPH,MODE"},                                                                /* (DUELL 0.1, id 25; 0.2.14: no OVERVIEW; 0.2.25: no MIXER, GLO's first page now) */
        {"CPU METER", "ON,OFF"},                                                                     /* (DUELL 0.2.9, id 26) */
        {"MULTI LOAD", "KEEPS MODE,SETS MODE"},                                                      /* (DUELL 0.2.11, id 27) */
        {"ARP", "OFF,ON"}, {"SEQ", "OFF,ON"}, {"SENDS", "OFF,ON"}, {"PART FX", "OFF,ON"},
        {"PERF LEAVE", "FADE,KEEP"},          /* (DUELL 0.2.14: id 32, CONTROL) */
        {"BASS MONO", "OFF,40 Hz,50 Hz,63 Hz,80 Hz,100 Hz,125 Hz,160 Hz,200 Hz,250 Hz,315 Hz,400 Hz"},   /* (DUELL 0.2.19: id 33, the MASTER page, tab MASTER: no MENU tab) */
        {"THRESH", 0}, {"RATIO", "OFF,1.5:1,2:1,3:1,4:1,6:1,8:1,12:1,INF"},                              /* (DUELL 0.2.21: ids 34..37, GLO > COMP, tab MASTER; THRESH a number in dB) */
        {"ATTACK", "0.1 ms,0.15 ms,0.2 ms,0.3 ms,0.5 ms,0.7 ms,1 ms,1.5 ms,2 ms,3 ms,5 ms,7 ms,10 ms,15 ms,20 ms,30 ms,50 ms,70 ms,100 ms"},
        {"RELEASE", "20 ms,30 ms,50 ms,70 ms,100 ms,150 ms,200 ms,300 ms,500 ms,700 ms,1 s,1.5 s,2 s"},
        {"P1 CH", 0}, {"P1 NOTE", 0}, {"P2 CH", 0}, {"P2 NOTE", 0},   /* (DUELL 0.2.22: ids 38..41, GLO > SIDECHAIN, tab SIDECHAIN; numbers) */
        {"WIDTH", 0}};                                                /* (DUELL 0.2.23: id 42, GLO > WIDTH, tab MASTER; a number; 0.2.25: id 43, DRIVE, is gone) */
        /* (DUELL 0.2.14: GLO > MODULES, ids 28..31, tab MODULES) */
    static const uint8_t ID[MDN] = {0, 3, 4, 5, 6, 11, 12, 13, 14, 15, 16, 17, 19, 20, 22, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42};   /* (1 STYLE, 2 LARGE, 7 FX LATCH, 8 BPM LOCK, 18 SCOPE,
                                                                                                         * 21 MIDI IN, 23 OVERVIEW, 24 HELP: retired) */
    static const int32_t DEF[MDN] = {-1, 0, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 4400, 0, 0, 0, 1, 1, 1, 1, 0, 8, -17, 0, 80, 40, 10, 36, 10, 36, 100};   /* (0.2.23 WIDTH 100 %; 0.2.14 MODULES: ON; PERF LEAVE: FADE; 0.2.19 BASS MONO: OFF; 0.2.21 COMP: THRESH -17 dB, RATIO 1:1, ATTACK 10 ms, RELEASE 200 ms) */   /* (COLOR: the default palette; 0.2.14: ACCEL ON,
                                                                                                    * USB SERIAL OFF, CLICK REC) */
    static const uint8_t TAB[MDN] = {0, 0, 0, 1, 1, 3, 2, 2, 2, 3, 0, 0, 1, 1, 2, 1, 0, 1, 4, 4, 4, 4, 1, 5, 5, 5, 5, 5, 6, 6, 6, 6, 5};   /* DISPLAY CONTROL AUDIO
                                                                                                     * SYSTEM (DUELL 0.1: no MIDI tab) */
    static const char *const TABN[7] = {"DISPLAY", "CONTROL", "AUDIO", "SYSTEM", "MODULES", "MASTER", "SIDECHAIN"};   /* (0.2.14: MODULES, no MENU tab: GLO > MODULES; 0.2.19: MASTER, the GLO > MASTER page) */
    int bad = 0, ok = 1;
    uint32_t i, k, n;
    menu_item_t it;
    char joined[400];
    const char *json = getenv("MENU_JSON");
    FILE *jf = json ? fopen(json, "w") : 0;
    reset();
    settings_defaults(favorites.factory[15]);          /* (a new unit: persist_boot with no settings) */
    cmp_set = 0, cmp_last = 0xFFFFFFFFu, cmp_sync();   /* (0.2.21: and the compressor's first look: the defaults, THRESH -17 dB .. as the frame does) */
    ms_last = 0xFFFFFFFFu, ms_sync();                  /* (0.2.23: WIDTH and DRIVE's first look: the defaults, 100 % and 0) */
    if (jf) fprintf(jf, "[");
    for (i = 0; i < MDN; i++) {
        n = menu_desc(i, &it);
        joined[0] = 0;
        for (k = 0; k < it.nnames; k++) { if (k) strcat(joined, ","); strcat(joined, it.names[k]); }
        ok &= i >= 32u ? n > 10u && it.index == i && it.id == ID[i] && it.kind == 1 && it.min == 0 && it.max == (i == 32u ? 200 : 100) &&
                         !it.nnames && !strcmp(it.unit, i == 32u ? "%" : "") && !strcmp(it.name, WANT[i][0])   /* (0.2.23: numbers: WIDTH 0..200 %, DRIVE 0..100) */
              : i >= 28u ? n > 10u && it.index == i && it.id == ID[i] && it.kind == 1 && it.min == (((i - 28u) & 1u) ? 0 : 1) && it.max == (((i - 28u) & 1u) ? 127 : 16) &&
                         !it.nnames && !it.unit[0] && !strcmp(it.name, WANT[i][0])   /* (0.2.22: numbers: a channel 1..16, a note 0..127) */
              : i == 26u || i == 27u ? n > 10u && it.index == i && it.id == ID[i] && it.kind == 1 && it.min == 0 && it.max == (i == 26u ? 120 : 80) &&
                         !it.nnames && !it.unit[0] && !strcmp(it.name, WANT[i][0])   /* (0.2.24: ATTACK 0..120, RELEASE 0..80 as numbers: the index of a fine time) */
              : i == 24u ? n > 10u && it.index == i && it.id == ID[i] && it.kind == 1 && it.min == -40 && it.max == 0 &&
                         !it.nnames && !strcmp(it.unit, "dB") && !strcmp(it.name, "THRESH")   /* (a number: dB) */
              : i == 14u ? n > 10u && it.index == i && it.id == ID[i] && it.kind == 2 && it.min == 4153 && it.max == 4662 &&
                         !it.nnames && !strcmp(it.unit, "Hz") && !strcmp(it.name, "TUNE")     /* (0.2.22: a number in tenths: A4 in 0.1 Hz, 415.3 .. 466.2) */
                       : n > 10u && it.index == i && it.id == ID[i] && it.kind == 0 && it.min == 0 && !it.unit[0] &&
                         it.max + 1 == (int32_t)it.nnames && !strcmp(it.name, WANT[i][0]) &&
                         (WANT[i][1] ? !strcmp(joined, WANT[i][1]) : it.nnames == NPALETTES);
        ok &= it.value == (i == 0u ? (int32_t)settings.palette : DEF[i]) &&
              it.tab == TAB[i] && !strcmp(it.tabname, TABN[TAB[i]]) && !it.rest;
        if (i == 0)
            for (k = 0; k < NPALETTES; k++) ok &= !strcmp(it.names[k], UI_PALETTES[k].name);
        if (!ok) { printf("  MENU_DESC %u: %s [%s] value %d max %d tab %u %s\n", i, it.name, joined, it.value, it.max, it.tab, it.tabname); break; }
        if (jf) {
            fprintf(jf, "%s\n {\"id\":%u,\"kind\":%u,\"min\":%d,\"max\":%d,\"value\":%d,\"name\":\"%s\",\"names\":[", i ? "," : "",
                    it.id, it.kind, it.min, it.max, it.value, it.name);
            for (k = 0; k < it.nnames; k++) fprintf(jf, "%s\"%s\"", k ? "," : "", it.names[k]);
            fprintf(jf, "],\"unit\":\"%s\",\"tab\":%u,\"tabName\":\"%s\"}", it.unit, it.tab, it.tabname);
        }
    }
    if (jf) { fprintf(jf, "]\n"); fclose(jf); }
    bad += check("MENU_DESC: 24 items (0.2.14: GLO > MODULES ARP SEQ SENDS PART FX, ids 28..31, OFF / ON, tab 4 MODULES; SCOPE (18) and OVERVIEW (23) retired, new names, the new defaults; 0.2.11: MODE RECALL, id 27; was 20 (STYLE (1), LARGE (2), FX LATCH (7) and HELP (24) retired in DUELL 0.2.2; was 24: 1.0.4's 11 in the menu's order, BPM LOCK (id 8) retired in DUELL, then 1.1's CLICK, CLICK LEVEL, COUNT-IN, 1.2's RESTORE LAST, SCALE LEDS, SCREEN OFF, 1.2's SCOPE, STEP PREVIEW, CHORD ENTRY, MIDI IN, TUNE, 1.3's HOME, 1.5's HELP), ids 0..25 but 1 2 7 8 21 24, names, defaults; TUNE a number in tenths 4153..4662 Hz/10 (0.2.22; was -50..50 ct)", ok);
    bad += check("MENU_DESC (1.0.5): after the names (TUNE: the unit) each item's tab, index and name (DISPLAY CONTROL AUDIO SYSTEM)", ok);
    {   /* an older editor reads the names and stops: the tab is past them, nothing it reads moved */
        uint32_t m = menu_desc(2, &it), p = 14, q;   /* (LEDS, a DISPLAY row) */
        for (q = 0; q < 1u + it.nnames; q++) { while (host_wire[p]) p++; p++; }   /* name, the names */
        ok = m > 10u && host_wire[p] == 0 && !strcmp((const char *)host_wire + p + 1, "DISPLAY") &&
             p + 1u + 8u == m + 6u - 1u;
        bad += check("MENU_DESC: the tab comes after every byte of the 1.0.4 reply (older editors ignore it)", ok);
    }
    ok = 1;
    for (i = 0; i < MDN; i++) {
        menu_desc(i, &it);
        ok &= strcmp(it.name, "CALIBRATION") && strcmp(it.name, "BUTTON MAP") && strcmp(it.name, "ABOUT") && strcmp(it.name, "TEST") &&
              strcmp(it.name, "GUIDE") && strcmp(it.name, "ERASE DATA") && strcmp(it.name, "SCOPE") && strcmp(it.name, "OVERVIEW");
    }
    n = menu_desc(MDN, &it);
    ok &= n == 2u && it.index == MDN && it.id == 127;
    n = menu_desc(127, &it);
    bad += check("MENU_DESC: no BUTTON MAP / TEST / GUIDE / ABOUT / ERASE DATA, no SCOPE / OVERVIEW; an index past the list answers index, 127 (no item)",
                 ok && n == 2u && it.index == 127 && it.id == 127);
    {
        uint8_t a[3] = {0, 0, 0};
        ok = request(ED_MENU_DESC, a, 0) == 0 && request(ED_MENU_DESC, a, 2) == 0 && request(ED_MENU_SET, a, 2) == 0 &&
             request(ED_MENU_SET, a, 1) == 0;
        bad += check("MENU_DESC / MENU_SET with the wrong length: no reply", ok);
    }

    /* MENU_SET: each setting, applied as the menu applies it; RAM only here (no flash: rc 3). A new unit (0.2.14's defaults: ui_prefs
     * ACCEL | SERIAL OFF = 0x44, ui_prefs2 the flag 0x80, CLICK REC) */
    reset();
    settings_defaults(favorites.factory[15]);
    ui.force = 0;
    ok = ui_prefs == 0x44u && ui_prefs2 == 0x80u && ui_rec_prefs == 1u;
    ok &= menu_set(0, 2) == 3 && host_wire[6] == 0 && ed_rv(host_wire + 7) == 2 && settings.palette == 2u &&
         T_BG == UI_PALETTES[2].bg[ux.part] && ui.force;
    ok &= menu_set(1, 1) == 1 && menu_set(2, 1) == 1 && menu_set(24, 1) == 1 && ui_prefs == 0x44u && ui_prefs2 == 0x80u;   /* (STYLE 1, LARGE 2, HELP 24: retired in DUELL 0.2.2, unknown ids: rc 1, nothing set) */
    ok &= menu_set(3, 1) == 3 && (ui_prefs & PREF_ANIM_OFF);
    ok &= menu_set(4, 0) == 3 && settings_leds == LEDS_OFF && menu_set(4, 1) == 3 && settings_leds == LEDS_DIM_LO;
    ok &= menu_set(5, 3) == 3 && settings_hold == 3u;
    ok &= menu_set(6, 1) == 3 && (ui_prefs & PREF_ACCEL);
    ok &= menu_set(7, 1) == 1 && !(ui_prefs & 1u);   /* (FX LATCH, id 7: retired in DUELL 0.2.2, an unknown id: rc 1, its bit stays clear) */
    ok &= menu_set(8, 1) == 1 && !(ui_prefs & 16u);   /* (BPM LOCK, id 8: retired, an unknown id: rc 1) */
    ok &= menu_set(9, 2) == 1 && host_wire[6] == 9;   /* (OUTPUT EQ, id 9: retired in DUELL 0.2.19, an unknown id: rc 1) */
    ok &= menu_set(10, 1) == 1 && host_wire[6] == 10 && !(ui_prefs & 8u);   /* (USB LEVEL, id 10: retired in DUELL 0.2.25, an unknown id: rc 1; the bit stays clear) */
    ok &= menu_set(12, 1) == 3 && click_mode == CLICK_REC && menu_set(12, 2) == 3 && click_mode == CLICK_ON;
    ok &= menu_set(13, 0) == 3 && click_lvl == 0u;
    ok &= menu_set(14, 2) == 3 && cin_bars == 2u;
    ok &= menu_set(15, 1) == 3 && (ui_prefs & PREF_RESTORE_OFF);
    ok &= menu_set(16, 1) == 3 && (ui_rec_prefs & 0x80u) && click_mode == CLICK_ON && cin_bars == 2u;   /* (its own bit) */
    ok &= menu_set(17, 0) == 3 && scr_get() == 0u && menu_set(17, 9) == 3 && scr_get() == 4u &&   /* (clamped) */
          menu_set(17, 2) == 3 && scr_get() == 2u && ui_scr == 1u;
    ok &= menu_set(18, 1) == 1 && ui_prefs2 == 0x80u && !scope_mix;                              /* (SCOPE, id 18: retired in DUELL 0.2.14, an unknown id: rc 1) */
    ok &= menu_set(19, 1) == 3 && ui_prefs2 == 0x82u && menu_set(20, 1) == 3 && ui_prefs2 == 0x86u;   /* (1.2: their own byte) */
    ok &= menu_set(21, 1) == 1 && song.g[G_ROUTE] == 0 &&       /* (MIDI IN, id 21: retired, an unknown id) */  menu_set(22, 4387) == 0 && song.g[G_TUNE] == 4387;   /* (1.2: the
                                                         * project's, rc 0: in the music, no settings record) */
    ok &= menu_set(23, 1) == 1 && favorites.factory[15][26] == 0u && ui_prefs2 == 0x86u && ui_scr == 1u;   /* (OVERVIEW, id 23: retired in DUELL 0.2.14, an unknown id: rc 1) */
    ok &= menu_set(25, 1) == 3 && ui_home_pg == HP_MODE && ui_prefs2 == 0x86u;                       /* (DUELL 0.1: HOME OPENS, its own byte) */
    ok &= menu_set(26, 1) == 3 && (ui_prefs2 & 0x10u) && ui_prefs2 == 0x96u && ui_home_pg == HP_MODE;   /* (DUELL 0.2.9: CPU METER OFF, bit 4 of its byte; set = OFF, clear = ON) */
    ok &= menu_set(27, 1) == 3 && (ui_prefs2 & 0x40u) && ui_prefs2 == 0xD6u;                     /* (DUELL 0.2.11 MODE RECALL ON, 0.2.14 MULTI LOAD SETS MODE: bit 6 of its byte) */
    ok &= ui_idle == 0u && menu_set(28, 0) == 3 && ed_rv(host_wire + 7) == 0 && ui_idle == 2u && mods_off == MODS_ARP &&   /* (0.2.14 MODULES: ARP OFF, bit 1 of [15][10]; */
          menu_set(29, 0) == 3 && menu_set(30, 0) == 3 && menu_set(31, 0) == 3 && ui_idle == 30u && mods_off == MODS_ALL &&   /* SEQ, SENDS, PART FX bits 2..4; */
          menu_set(30, 1) == 3 && ed_rv(host_wire + 7) == 1 && ui_idle == 22u && menu_set(31, 9) == 3 && ed_rv(host_wire + 7) == 1 && ui_idle == 6u &&   /* ON clears, clamped */
          ui_prefs2 == 0xD6u;                                                                         /* .. nothing else moved) */
    ok &= menu_set(32, 1) == 3 && ed_rv(host_wire + 7) == 1 && ui_idle == 0x26u && perf_keep() && mods_off == (MODS_ARP | MODS_SEQ) &&   /* (0.2.14 PERF LEAVE KEEP: bit 5 of [15][10]; the MODULES bits stay) */
          menu_set(32, 9) == 3 && ed_rv(host_wire + 7) == 1 && ui_prefs2 == 0xD6u;
    ok &= menu_set(33, 5) == 3 && ed_rv(host_wire + 7) == 5 && song.bm == 5 && bm_sel == 5u && ui_ms_set[0] == MS_TAG && ui_ms_set[3] == 6u && ui_prefs2 == 0xD6u && ui_idle == 0x26u &&   /* (0.2.24 BASS MONO 100 Hz: a unit setting, the settings byte [15][21] = 6, rc 3 = saved; the audio side at once) */
          menu_set(33, 99) == 3 && ed_rv(host_wire + 7) == (int32_t)BM_N && menu_set(33, 0) == 3 && ed_rv(host_wire + 7) == 0 && song.bm == 0 && ui_ms_set[3] == 1u;
    menu_set(33, 5);
    ok &= menu_set(34, -10) == 3 && ed_rv(host_wire + 7) == -10 && song.cp[0] == 30 && (cmp_cfg & 255u) == 30u && (cmp_set >> 24) == 0xC8u &&   /* (0.2.21 COMP: THRESH -10 dB = index 30; the settings word, the audio side; rc 3 here: no flash) */
          menu_set(34, -99) == 3 && ed_rv(host_wire + 7) == -40 && menu_set(34, 99) == 3 && ed_rv(host_wire + 7) == 0 && menu_set(34, -10) == 3 &&   /* clamped */
          menu_set(35, 5) == 3 && ed_rv(host_wire + 7) == 5 && song.cp[1] == 5 && (cmp_cfg >> 8 & 15u) == 5u && menu_set(35, 99) == 3 && ed_rv(host_wire + 7) == 8 && menu_set(35, 5) == 3 &&
          menu_set(36, 7) == 3 && song.cp[2] == 7 && menu_set(36, 999) == 3 && ed_rv(host_wire + 7) == 120 && menu_set(36, 7) == 3 &&
          menu_set(37, 9) == 3 && song.cp[3] == 9 && menu_set(37, 999) == 3 && ed_rv(host_wire + 7) == 80 && menu_set(37, 9) == 3 &&
          cmp_cfg == (30u | 5u << 8 | 7u << 16 | 9u << 24) && cmp_set == (0xC8000000u | 30u | 5u << 6 | 7u << 10 | 9u << 17) && ui_prefs2 == 0xD6u && ui_idle == 0x26u && song.bm == 5;   /* (a device setting: no other settings byte, the music untouched) */
    {   /* 0.2.22 SIDECHAIN: ids 38..41, the ducker's MIDI channel and note per part, a device setting in the settings bytes [15][14..17] (sc_midi is the audio side's copy) */
        persist_t pe;
        ok = menu_set(38, 5) == 3 && ed_rv(host_wire + 7) == 5 && sc_midi[0] == 4u && ui_sc_set[0] == 5u &&
             menu_set(39, 0) == 3 && ed_rv(host_wire + 7) == 0 && sc_midi[1] == 0u && ui_sc_set[1] == 1u &&
             menu_set(40, 99) == 3 && ed_rv(host_wire + 7) == 16 && sc_midi[2] == 15u && menu_set(40, -4) == 3 && ed_rv(host_wire + 7) == 1 && sc_midi[2] == 0u &&
             menu_set(41, 127) == 3 && ed_rv(host_wire + 7) == 127 && sc_midi[3] == 127u && menu_set(41, 300) == 3 && ed_rv(host_wire + 7) == 127 &&
             sc_get(0) == 4u && sc_get(1) == 0u && sc_get(2) == 0u && sc_get(3) == 127u;
        memset(&pe, 0, sizeof pe);
        settings_export(&pe);
        ok &= pe.favorites.factory[15][14] == 5u && pe.favorites.factory[15][15] == 1u && pe.favorites.factory[15][16] == 1u && pe.favorites.factory[15][17] == 128u && song.g[G_BPM] == song.g[G_BPM];
        bad += check("MENU_SET 38..41 (SIDECHAIN): P1 CH, P1 NOTE, P2 CH, P2 NOTE set, clamped (1..16, 0..127), applied to the audio side's copy and stored in the settings record (bytes 14..17)", ok);
        menu_set(38, 10); menu_set(39, 36); menu_set(40, 10); menu_set(41, 36);
        ok = sc_midi[0] == 9u && sc_midi[1] == 36u && sc_midi[2] == 9u && sc_midi[3] == 36u;
        bad += check("  .. the defaults again: channel 10, note 36 (C2)", ok);
    }
    {   /* 0.2.23 WIDTH: id 42, a device setting in the settings bytes [15][18..21] (wd_sel: the audio side's copy) */
        ok = menu_set(42, 150) == 3 && ed_rv(host_wire + 7) == 150 && song.ms[0] == 150 && wd_sel == 150u && ui_ms_set[0] == MS_TAG && ui_ms_set[1] == 150u &&
             menu_set(42, 999) == 3 && ed_rv(host_wire + 7) == 200 && menu_set(42, -5) == 3 && ed_rv(host_wire + 7) == 0 && wd_sel == 0u &&
             menu_set(43, 64) == 1 && host_wire[6] == 43 && ui_ms_set[2] == 0u &&   /* (0.2.25: the saturator's DRIVE, id 43, is gone: an unknown id) */
             song.cp[1] == 5 && song.bm == 5;
        bad += check("MENU_SET 42 (WIDTH): set, clamped (0..200 %), applied to the audio side and stored in the settings bytes (tag, WIDTH); 43 (the removed DRIVE) is an unknown id", ok);
        menu_set(42, 100);
        bad += check("  .. the default again: WIDTH 100 % (the stage off)", wd_sel == 100u);
    }
    for (i = 0; i < MDN; i++) {                         /* MENU_DESC reads them back */
        static const int32_t SET[MDN] = {2, 1, 1, 3, 1, 1, 2, 0, 2, 1, 1, 2, 1, 1, 4387, 1, 1, 1, 0, 0, 1, 1, 1, 5, -10, 5, 7, 9, 10, 36, 10, 36, 100};   /* (0.2.25: HOME OPENS MODE, 1: MIXER is gone) */
        menu_desc(i, &it);
        ok &= it.value == SET[i];
    }
    bad += check("MENU_SET: every setting applied as the menu does (palette, EQ, CLICK, COUNT-IN at once, RESTORE LAST, SCALE LEDS, SCREEN OFF, STEP PREVIEW, STEP KEYS, HOME OPENS, CPU METER, MULTI LOAD), read back", ok);
    ok = menu_set(3, 0) == 3 && !(ui_prefs & PREF_ANIM_OFF) && (ui_prefs & PREF_ACCEL) &&
         menu_set(26, 0) == 3 && !(ui_prefs2 & 0x10u) && ui_prefs2 == 0xC6u &&   /* (CPU METER back to ON: its bit clears, the others stay) */
         menu_set(27, 0) == 3 && ui_prefs2 == 0x86u;   /* (MULTI LOAD back to KEEPS MODE) */
    bad += check("MENU_SET: a flag back to its default leaves the other flags", ok);

    /* clamping: the reply carries the device's value */
    ok = menu_set(0, 50) == 3 && ed_rv(host_wire + 7) == (int32_t)NPALETTES - 1 && settings.palette == NPALETTES - 1u;
    ok &= menu_set(0, -5) == 3 && ed_rv(host_wire + 7) == 0 && settings.palette == 0u;
    ok &= menu_set(4, 99) == 3 && ed_rv(host_wire + 7) == 3 && settings_leds == LEDS_INV;
    ok &= menu_set(5, 8191) == 3 && ed_rv(host_wire + 7) == 3 && settings_hold == 3u;
    ok &= menu_set(6, 7) == 3 && ed_rv(host_wire + 7) == 1 && (ui_prefs & PREF_ACCEL);
    ok &= menu_set(14, 9) == 3 && ed_rv(host_wire + 7) == 2 && cin_bars == 2u;
    ok &= menu_set(12, -1) == 3 && ed_rv(host_wire + 7) == 0 && click_mode == CLICK_OFF;
    ok &= menu_set(25, 99) == 3 && ed_rv(host_wire + 7) == 1 && ui_home_pg == HP_MODE;   /* (0.2.25: MODE the last; an old editor's 2, MIXER, or 3, OVERVIEW: MODE) */
    ok &= menu_set(22, -99) == 0 && ed_rv(host_wire + 7) == 4153 && song.g[G_TUNE] == 4153;
    ok &= menu_set(22, 4663) == 0 && ed_rv(host_wire + 7) == 4662 && song.g[G_TUNE] == 4662;
    ok &= menu_set(22, 8000) == 0 && ed_rv(host_wire + 7) == 4662 && song.g[G_TUNE] == 4662;
    ok &= menu_set(22, 4400) == 0 && ed_rv(host_wire + 7) == 4400 && song.g[G_TUNE] == 4400;
    song.g[G_TUNE] = -5;                                 /* (an older project's cents, or a stray value: MENU_DESC reads 440.0) */
    { menu_item_t t; uint32_t q; for (q = 0; q < 29u; q++) { menu_desc(q, &t); if (t.id == 22u) break; } ok &= t.id == 22u && t.value == 4400; }
    song.g[G_TUNE] = 4400;
    bad += check("MENU_SET: out-of-range values clamped, the reply says the value the device took (TUNE 4153..4662 = 415.3 .. 466.2 Hz; an out-of-range stored value reads 440.0)", ok);

    /* an id nobody has */
    {
        static uint8_t fav0[sizeof favorites], set0[sizeof settings];
        uint8_t hold0 = settings_hold, leds0 = settings_leds;
        memcpy(fav0, &favorites, sizeof favorites); memcpy(set0, &settings, sizeof settings);
        ok = menu_set(44, 1) == 1 && host_wire[6] == 44 && ed_rv(host_wire + 7) == 1;   /* (0.2.23: 42, 43 WIDTH, DRIVE; 0.2.14: 28..31 are MODULES', 32 PERF LEAVE, 0.2.19: 33 BASS MONO, 0.2.21: 34..37 COMP, 0.2.22: 38..41 SIDECHAIN) */
        ok &= menu_set(126, -3) == 1 && host_wire[6] == 126 && ed_rv(host_wire + 7) == -3;
        ok &= menu_set(127, 0) == 1 && host_wire[6] == 127;
        ok &= !memcmp(fav0, &favorites, sizeof favorites) && !memcmp(set0, &settings, sizeof settings) &&
              hold0 == settings_hold && leds0 == settings_leds;
        bad += check("MENU_SET: an unknown id answers rc 1, id and value echoed, nothing changed", ok);
    }

    /* the MENU page shown: the editor's change redraws it */
    ui.menu = 1; ui.force = 0;
    menu_set(6, 0);
    bad += check("MENU_SET while the MENU page is shown: the page redraws (ui.force)", ui.force && ui.menu == 1);
    ui.menu = 0;

#if FELUCCA_CDC
    /* USB SERIAL: the reply first, the re-enumeration 200 ms later (usb_serial_apply, called every frame). (usb.up
     * stays 0 here: the host has no USB registers; usb.config 0 = the device off the bus, enumerated afresh) */
    {
        uint32_t t0;
        reset();
        usb_cdc_on = 1;
        usb_serial_apply();
        ok = usb_cdc_on && usb.config;
        ok &= menu_set(11, 1) == 3 && host_wire[6] == 11 && ed_rv(host_wire + 7) == 1 && (ui_prefs & PREF_SERIAL_OFF);
        t0 = fm1_ms;
        usb_serial_apply();
        ok &= usb_cdc_on && usb.config;                 /* the reply has left; USB as it was */
        fm1_ms = t0 + 150u; usb_serial_apply();
        ok &= usb_cdc_on && usb.config;
        fm1_ms = t0 + 201u; usb_serial_apply();
        ok &= !usb_cdc_on && !usb.config;               /* off the bus: usb_retry attaches again without the console */
        bad += check("MENU_SET USB SERIAL OFF: reply first, applied ~200 ms later (the device re-enumerates)", ok);
        usb.config = 1;
        ok = menu_set(11, 0) == 3 && !(ui_prefs & PREF_SERIAL_OFF);
        t0 = fm1_ms;
        ui.menu = 1;                                    /* the MENU shown: applied when it closes, as the menu's own */
        fm1_ms = t0 + 500u;                             /* (ui_input calls usb_serial_apply only with the menu closed) */
        ok &= !usb_cdc_on && usb.config;
        ui.menu = 0; usb_serial_apply();
        ok &= usb_cdc_on && !usb.config;
        usb.config = 1;
        ok &= menu_set(11, 0) == 3;                     /* unchanged: no re-enumeration */
        fm1_ms += 300u; usb_serial_apply();
        ok &= usb_cdc_on && usb.config;
        bad += check("MENU_SET USB SERIAL ON: applied later (with the MENU open: when it closes); unchanged: no drop", ok);
    }
#endif
    reset();
    return bad;
}

/* (DUELL: the DRUM KIT 1..3 checks went with the DRUM engine, which cannot be selected any more) */

/* 1.5 MIDI LEARN (78 LEARN_GET, 79 LEARN_SET; editor_learn.c): the map as the device keeps it (midi_learn.c) */
static int learn_entry(uint32_t i, uint32_t used, uint32_t cc, uint32_t k, uint32_t id)   /* (a LEARN_GET reply) */
{
    const uint8_t *e = &host_wire[6u + 4u * i];      /* (ML_N, then the entries) */
    return e[0] == used && e[1] == cc && e[2] == k && e[3] == id;
}
static int learn_protocol(void)
{
    int bad = 0, ok;
    uint32_t n, i;
    reset();
    n = request(ED_LEARN_GET, 0, 0);
    ok = n == 6u + 1u + 4u * ML_N && host_wire[4] == ED_LEARN_GET && host_wire[5] == ML_N;
    for (i = 0; i < ML_N; i++)
        ok &= !memcmp(&host_wire[6u + 4u * i], "\0\0\0\0", 4);
    bad += check("LEARN_GET: 16 entries, all empty at first", ok);
    {   /* (the numbers are free ones: this test runs on the legacy map, where CC74 and CC20 are the standard map's and the morph's) */
        const uint8_t a[] = {0, 26, 1, P_E0 + 2}, b[] = {0, 27, 0, P_LEVEL}, c[] = {0, 27, 1, P_PAN}, cf[] = {0, 27, 1, P_PAN, 1};
        n = request(ED_LEARN_SET, a, 4);
        ok = n == 6u + 2u + 4u * ML_N && host_wire[4] == ED_LEARN_SET && (host_wire[5] == 0u || host_wire[5] == 3u) &&
             host_wire[6] == ML_N && !memcmp(&host_wire[7], (const uint8_t[]){1, 26, 1, P_E0 + 2}, 4);   /* (rc, then GET's) */
        request(ED_LEARN_SET, b, 4);
        n = request(ED_LEARN_SET, c, 4);                 /* (CC27 again: T2 PAN, but the number is T1 LEVEL's: refused, rc 6, the owner after the list) */
        ok &= host_wire[5] == 6u && n == 6u + 2u + 4u * ML_N + 9u && !memcmp(&host_wire[6u + 1u + 4u * ML_N], "T1 LEVEL", 9);
        n = request(ED_LEARN_GET, 0, 0);
        ok &= learn_entry(0, 1, 26, 1, P_E0 + 2) && learn_entry(1, 1, 27, 0, P_LEVEL) && learn_entry(2, 0, 0, 0, 0);
        n = request(ED_LEARN_SET, cf, 5);                /* (with force: the new editor takes it) */
        ok &= (host_wire[5] == 0u || host_wire[5] == 3u);
        n = request(ED_LEARN_GET, 0, 0);
        ok &= learn_entry(0, 1, 26, 1, P_E0 + 2) && learn_entry(1, 1, 27, 1, P_PAN) && learn_entry(2, 0, 0, 0, 0);
        bad += check("LEARN_SET op 0: learns (a track 2 value on CC26); CC27 taken by T1 LEVEL: refused (rc 6 and its name), forced: T2 PAN in its place", ok);
    }
    {
        const uint8_t t74[] = {0, 74, 0, P_LEVEL}, t74f[] = {0, 74, 0, P_LEVEL, 1};
        n = request(ED_LEARN_SET, t74, 4);
        ok = host_wire[5] == 6u && !memcmp(&host_wire[6u + 1u + 4u * ML_N], "CUTOFF", 7) && ml_cc_of(0, P_LEVEL) == 0u;   /* (the standard map's number is not taken silently) */
        request(ED_LEARN_SET, t74f, 5);
        ok &= ml_cc_of(0, P_LEVEL) == 75u;
        bad += check("LEARN_SET on a shipped number (CC74): refused with the owner's name, with force 1 taken", ok);
    }
    {
        const uint8_t bad1[] = {0, 64, 0, 1}, bad2[] = {0, 21, 2, 1}, bad3[] = {0, 21, 0, P_COUNT}, bad4[] = {0, 120, 0, 1};
        ok = request(ED_LEARN_SET, bad1, 4) && host_wire[5] == 1u;
        ok &= request(ED_LEARN_SET, bad2, 4) && host_wire[5] == 1u;
        ok &= request(ED_LEARN_SET, bad3, 4) && host_wire[5] == 1u;
        ok &= request(ED_LEARN_SET, bad4, 4) && host_wire[5] == 1u;
        n = request(ED_LEARN_GET, 0, 0);
        ok &= learn_entry(0, 1, 26, 1, P_E0 + 2) && learn_entry(1, 1, 27, 1, P_PAN) && learn_entry(2, 1, 74, 0, P_LEVEL);
        ml_drop(74, NTRK, P_COUNT);
        for (i = 0; i < ML_N - 2u; i++) {
            const uint8_t f[] = {0, (uint8_t)(40u + i), 0, (uint8_t)i};
            request(ED_LEARN_SET, f, 4);
        }
        { const uint8_t f[] = {0, 90, 1, 40}; ok &= request(ED_LEARN_SET, f, 4) && host_wire[5] == 5u; }
        bad += check("LEARN_SET refuses CC64 / CC120, track 3, a parameter past P_COUNT (rc 1); a 17th: rc 5 (full)", ok);
    }
    {
        const uint8_t one[] = {1, 26}, all[] = {2}, odd[] = {3};
        request(ED_LEARN_SET, one, 2);
        n = request(ED_LEARN_GET, 0, 0);
        ok = learn_entry(0, 1, 27, 1, P_PAN) && learn_entry(1, 1, 40, 0, 0) && ml_count() == ML_N - 1u;
        ok &= request(ED_LEARN_SET, odd, 1) && host_wire[5] == 1u;
        request(ED_LEARN_SET, all, 1);
        ok &= host_wire[5] != 1u && host_wire[5] != 5u && !ml_count() && !memcmp(midi_map, MIDI_MAP_DEFAULTS, sizeof midi_map);
        ok &= request(ED_LEARN_GET, odd, 1) == 0u;          /* (GET takes no arguments) */
        bad += check("LEARN_SET op 1 clears one CC, op 2 all; unknown ops rc 1; GET with arguments: no reply", ok);
    }
    (void)n;
    return bad;
}

/* DUELL MIDI MAP (85 MAP_GET, 86 MAP_SET, 87 MAP_ARM, 88 MAP_RESET; editor_map.c): the whole table, typed lines, the rules */
static uint32_t map_wire_line(uint32_t cc)             /* a line out of the last MAP_GET reply (chunk 32 lines; line 3 bytes after the chunk and version) */
{
    const uint8_t *e = &host_wire[7u + 3u * (cc % 32u)];
    return e[0] | (uint32_t)e[1] << 7 | (uint32_t)e[2] << 14;
}
static uint32_t map_get_line(uint32_t cc)
{
    uint8_t a[1];
    a[0] = (uint8_t)(cc / 32u);
    request(ED_MAP_GET, a, 1);
    return map_wire_line(cc);
}
static int map_protocol(void)
{
    int bad = 0, ok;
    uint32_t n, c, i;
    uint8_t a[8];
    reset();
    for (ok = 1, c = 0; c < 4u; c++) {
        a[0] = (uint8_t)c;
        n = request(ED_MAP_GET, a, 1);
        ok &= n == 5u + 2u + 96u + 1u && host_wire[4] == ED_MAP_GET && host_wire[5] == c && host_wire[6] == MAP_REC_VER;
        for (i = 0; i < 32u; i++)
            ok &= map_wire_line(32u * c + i) == midi_map[32u * c + i];
    }
    bad += check("MAP_GET: four chunks of 32 lines, 3 bytes a line, every line as the table has it", ok);
    a[0] = 4; a[1] = P_ATK; a[2] = 0;
    n = request(ED_MAP_GET, a, 3);
    ok = host_wire[6] == P_ATK && host_wire[8] == MAP_K_CONTINUOUS && host_wire[9] == MAP_S_PATCH && host_wire[10] == 1u &&
         host_wire[11] == 0u && !memcmp(&host_wire[12], "AMP ENV ATTACK", 15);
    a[1] = MAP_T_ROLL_A & 127; a[2] = MAP_T_ROLL_A >> 7;
    n = request(ED_MAP_GET, a, 3);
    ok &= host_wire[8] == MAP_K_ACTION && host_wire[10] == 0u;
    a[1] = 127; a[2] = 0;
    n = request(ED_MAP_GET, a, 3);
    ok &= host_wire[8] == 0u && host_wire[12] == 0u;           /* (127 is the motion escape: no target) */
    bad += check("MAP_GET 4 code: kind, where saved, automation, inert, the name of a target (an action fires once; 127 is none)", ok);
    {   /* MAP_SET: a typed line */
        const uint8_t s1[] = {0, 30, P_E0 & 127, P_E0 >> 7, MAP_R_CHANNEL, 0};
        n = request(ED_MAP_SET, s1, 6);
        ok = n == 5u + 3u + 3u + 1u + 1u && host_wire[5] == MAP_OK && host_wire[7] == 30u;
        ok &= midi_map[30] == MAP_LINE(P_E0, MAP_R_CHANNEL, MAP_W_TYPED) && (host_wire[8] | host_wire[9] << 7 | host_wire[10] << 14) == midi_map[30];
        bad += check("MAP_SET 0: CC30 moves E1 of the channel's part (a typed line); the reply holds the line", ok);
    }
    {   /* a number that moves something else, a locked number, invalid */
        const uint8_t s74[] = {0, 74, P_ATK, 0, MAP_R_CHANNEL, 0}, s74f[] = {0, 74, P_ATK, 0, MAP_R_CHANNEL, 1}, s64[] = {0, 64, P_ATK, 0, 0, 1};
        const uint8_t sbad[] = {0, 31, 127, 0, 0, 0}, sg[] = {0, 31, MAP_T_G(G_CRATE) & 127, MAP_T_G(G_CRATE) >> 7, MAP_R_CHANNEL, 0};
        n = request(ED_MAP_SET, s74, 6);
        ok = host_wire[5] == MAP_E_CONFLICT && !memcmp(&host_wire[n - 8], "CUTOFF", 7) && midi_map[74] == MIDI_MAP_DEFAULTS[74];
        n = request(ED_MAP_SET, s74f, 6);
        ok &= host_wire[5] == MAP_OK && midi_map[74] == MAP_LINE(P_ATK, MAP_R_CHANNEL, MAP_W_TYPED) && map_get_line(73) == (MAP_EMPTY | MAP_CLEARED);
        n = request(ED_MAP_SET, s64, 6);
        ok &= host_wire[5] == MAP_E_LOCKED && MAP_IS_LOCKED(midi_map[64]);
        n = request(ED_MAP_SET, sbad, 6);
        ok &= host_wire[5] == MAP_E_BAD && MAP_IS_EMPTY(midi_map[31]);
        n = request(ED_MAP_SET, sg, 6);
        ok &= host_wire[5] == MAP_E_BAD && MAP_IS_EMPTY(midi_map[31]);          /* (a global value wants the rule GLOBAL) */
        bad += check("MAP_SET: a taken number is refused with the owner's name (CUTOFF), force takes it and the parameter's old number is freed; CC64 locked; 127 and a bad rule invalid", ok);
    }
    {
        const uint8_t clr[] = {1, 30}, clrl[] = {1, 64}, clre[] = {1, 31};
        n = request(ED_MAP_SET, clr, 2);
        ok = host_wire[5] == MAP_OK && MAP_IS_EMPTY(midi_map[30]) && (midi_map[30] & MAP_CLEARED);
        n = request(ED_MAP_SET, clrl, 2);
        ok &= host_wire[5] == MAP_E_LOCKED;
        n = request(ED_MAP_SET, clre, 2);
        ok &= host_wire[5] == MAP_OK && !(midi_map[31] & MAP_CLEARED);
        bad += check("MAP_SET 1: a number cleared (marked cleared by the user), a standard number refused, an empty one stays unmarked", ok);
    }
    {
        const uint8_t arm[] = {P_ATK, 0, MAP_R_PART2}, armbad[] = {127, 0, 0}, armg[] = {MAP_T_G(G_CRATE) & 127, MAP_T_G(G_CRATE) >> 7, MAP_R_GLOBAL};
        n = request(ED_MAP_ARM, arm, 3);
        ok = n == 7u && host_wire[5] == 0u && ui.ml.on && ml_arm && ui.ml.pick;
        ml_heard = 45 + 1;                                             /* (the ISR hears CC45) */
        ml_poll();
        ok &= midi_map[45] == MAP_LINE(P_ATK, MAP_R_PART2, MAP_W_TYPED) && !ml_arm && !ui.ml.on;
        n = request(ED_MAP_ARM, armbad, 3);
        ok &= host_wire[5] == 3u && !ml_arm;
        n = request(ED_MAP_ARM, armg, 3);
        ok &= host_wire[5] == 0u && ml_arm;
        n = request(ED_MAP_ARM, 0, 0);
        ok &= host_wire[5] == 0u && !ml_arm && !ui.ml.on;
        bad += check("MAP_ARM: the next CC heard becomes the row's line (a typed line, PART2); an invalid row 3; no argument cancels", ok);
    }
    {
        n = request(ED_MAP_RESET, 0, 0);
        ok = n == 5u + 2u + 1u && host_wire[5] == 0u && !memcmp(midi_map, MIDI_MAP_DEFAULTS, sizeof midi_map);
        n = request(ED_MAP_RESET, a, 1);
        ok &= host_wire[5] == 3u;
        bad += check("MAP_RESET: exactly the shipped list (a call with an argument: rc 3)", ok);
    }
    {   /* a fuzz of MAP_SET / MAP_ARM / LEARN_SET: any bytes, any length: the table stays valid (locked numbers, known targets, one number a parameter, at most 16 learned) */
        uint32_t r = 12345u, k, cnt;
        uint16_t keep[MAP_N];
        for (k = 0, ok = 1; k < 30000u && ok; k++) {
            uint8_t f[8];
            r = r * 1103515245u + 12345u;
            n = (r >> 16) % 9u;
            for (i = 0; i < n; i++) {
                r = r * 1103515245u + 12345u;
                f[i] = (uint8_t)((r >> 16) % ((k & 3u) == 0u ? 128u : 6u) + (i == 1u && (k & 3u) == 1u ? 0u : 0u));
            }
            if (k % 7u == 0u && n >= 6u) { f[0] = 0; f[1] = (uint8_t)((r >> 8) % 128u); f[2] = (uint8_t)((r >> 3) % 128u); f[3] = (uint8_t)((r >> 11) & 3u); f[4] = (uint8_t)((r >> 5) & 3u); f[5] = (uint8_t)((r >> 20) & 1u); }
            request(k % 11u == 0u ? ED_MAP_ARM : k % 5u == 0u ? ED_LEARN_SET : ED_MAP_SET, f, n);
            if (ml_arm && (k % 3u == 0u)) { ml_heard = (uint8_t)(1u + (r >> 9) % 128u); ml_poll(); }
            memcpy(keep, midi_map, sizeof keep);
            map_sanitize();
            ok &= !memcmp(keep, midi_map, sizeof keep);
            for (cnt = c = 0; c < MAP_N; c++)
                cnt += MAP_WHO(midi_map[c]) == MAP_W_LEARNED && !MAP_IS_EMPTY(midi_map[c]);
            ok &= cnt <= ML_N;
            for (c = 0; c < MAP_N; c++)
                ok &= MAP_LOCKED_CC(c) == (uint32_t)MAP_IS_LOCKED(midi_map[c]);
        }
        bad += check("fuzz: 30000 MAP_SET / MAP_ARM / LEARN_SET frames of any bytes: the table stays valid (nothing to sanitize)", ok);
    }
    reset();
    return bad;
}

#ifndef EDITOR_TEST_NO_MAIN                              /* (robust_test.c, fuzz_*.c: this file is their base) */
/* cloud job G (EDITABLE B): the editor's value writes edit A at the B end too (the web editor shows A; End B's values are the unit's knobs') */
static int editb_editor(void)
{
    int bad = 0;
    uint8_t a[8];
    xb_edit_t ed[XB_MAX];
    int8_t b0[P_COUNT];
    int16_t before;
    morph_set_amount(40);
    morph_roll(1);
    morph_set_fader(127);
    memcpy(b0, morph_cur(&mph[0], 1), P_COUNT);
    before = trk[0].p[P_E6];
    a[0] = 0; a[1] = P_E6; a[2] = (uint8_t)((8192 + 90) & 127); a[3] = (uint8_t)((8192 + 90) >> 7);
    request(ED_SET, a, 4);
    bad += check("EDITABLE B: at fader 127 an editor value write (SET) edits A: the stored value is the written one, End B and its list as they were",
                 morph_b_mode(0, P_E6) == 1u && trk[0].p[P_E6] == 90 && before != 90 && !morph_edits(0, ed) && !memcmp(b0, morph_cur(&mph[0], 1), P_COUNT));
    return bad;
}

int main(void)
{
    int bad = preferences() + motion_locks() + framing() + uart_recovery() + steps() + samples() + malformed_saves() +
              fm6_patches() + user_preset_roundtrip() + live_sync() + usb_burst() + menu_protocol() +
              learn_protocol() + editb_editor() + map_protocol();
    printf("%s\n", bad ? "EDITOR TEST FAILED" : "editor test passed");
    return bad != 0;
}
#endif
