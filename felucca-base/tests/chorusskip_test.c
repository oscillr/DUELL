/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.21: the chorus loop of the plain bus (fx_bus.c fx_buses_m) is left out of a block whose chorus input is all 0 while the whole chorus line is 0
 * (cho_zrun): the saved work is the second tap and the side, ~46 host instructions a sample in STEREO and LAYER. It must be INVISIBLE: bit for bit what the
 * loop gives. This file is built twice (host_tests.sh "chorusskip"): as it is (the skip on) and with -DCHO_NOSKIP (the firmware without the skip, the loop
 * of the stereo chorus as it was); both run the same long scenario and print a hash of every output sample and, every 64 blocks, of the chorus lines and
 * the buses' states (write index, LFO phase, depth reached, the lines themselves). The two hash lists must be equal (the script compares them), and the skip build
 * must have skipped (blocks counted by CHO_SKIP_STATS), the reference none.
 *   SCENES    1 silence, DEPTH / RATE turned while it is silent (the glide's state moves on), a chord with the chorus send 0 (nothing reaches the bus),
 *             the send up (the line fills), a held chord with DEPTH and RATE turned, the release and the tails (the line empties, the skip starts again after
 *             2048 samples), short notes with gaps either side of the 2048 samples, the reverb alone (send 0 to the chorus, a reverb send), the delay (its tail
 *             goes through the plain bus), the delay's modulation and PING-PONG and RANDOM (the paths that always run the chorus; the counter still counts for the plain one after)
 *   FUZZ      6 s of random events per mode: note on / off of both parts, the chorus, delay and reverb sends, DEPTH, RATE, the delay MODE and modulation
 *   MODES     STEREO, LAYER and DUEL (SPLIT: buses A and B, each its own counter) */
#define MODE_TEST_NO_MAIN 1
#define CHO_SKIP_STATS 1
#include "mode_test.c"

static uint64_t cs_fnv(const void *p, uint32_t bytes, uint64_t h)
{
    const uint8_t *b = p;
    for (uint32_t i = 0; i < bytes; i++) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}
static uint32_t cs_xs = 0x2545F491u;
static uint32_t cs_rnd(uint32_t n)
{
    cs_xs ^= cs_xs << 13;
    cs_xs ^= cs_xs >> 17;
    cs_xs ^= cs_xs << 5;
    return (cs_xs >> 8) % n;
}
static uint64_t cs_h, cs_hs;                            /* the outputs; the states */
static uint32_t cs_blocks;
static void cs_state(void)
{
    cs_hs = cs_fnv(cho_buf, sizeof cho_buf, cs_hs);
    cs_hs = cs_fnv(cho_buf_b, sizeof cho_buf_b, cs_hs);
    cs_hs = cs_fnv(&fx.cho_w, sizeof fx.cho_w, cs_hs);
    cs_hs = cs_fnv(&fx.cho_ph, sizeof fx.cho_ph, cs_hs);
    cs_hs = cs_fnv(&fx.cho_dq, sizeof fx.cho_dq, cs_hs);
    cs_hs = cs_fnv(&fx.cho_on, sizeof fx.cho_on, cs_hs);
    cs_hs = cs_fnv(&fx_b.cho_w, sizeof fx_b.cho_w, cs_hs);
    cs_hs = cs_fnv(&fx_b.cho_ph, sizeof fx_b.cho_ph, cs_hs);
    cs_hs = cs_fnv(&fx_b.cho_dq, sizeof fx_b.cho_dq, cs_hs);
    cs_hs = cs_fnv(&fx_b.cho_on, sizeof fx_b.cho_on, cs_hs);
}
static void cs_blk(uint32_t n)                          /* n blocks, every output sample hashed */
{
    while (n--) {
        mix_block(ob, CTL);
        cs_h = cs_fnv(ob, sizeof ob, cs_h);
        if ((++cs_blocks & 63u) == 0)
            cs_state();
    }
}
static const uint32_t CS_1S = FS / CTL;
static void cs_setup(uint32_t mode)
{
    uint32_t k;
    dsp_clean(mode);
    cs_h = 1469598103934665603ull;
    cs_hs = 1469598103934665603ull;
    cs_blocks = 0;
    cho_skipped = cho_skipped_b = 0;
    song.g[G_CDEPTH] = 60;
    song.g[G_CRATE] = 44;
    song.g[G_RTYPE] = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    song.g[G_DMDEP] = 0;
    song.g[G_DMIX] = 60;
    song.g[G_DFDBK] = 50;
    song.g[G_PINGSPREAD] = 100;
    song.g[G_DTIME] = 1;
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    for (k = 0; k < 2u; k++) {
        host_preset(&trk[k], eng_of(k ? "ANALOG" : "ANALOG"), 0);
        trk[k].p[P_TRIM] = 100;
        trk[k].p[P_PAN] = k ? 40 : -40;
        trk[k].p[P_SUS] = 127;
        trk[k].p[P_REL] = 60;
        trk[k].p[P_AMODE] = 0;
        trk[k].p[P_CHOR] = trk[k].p[P_DLY] = trk[k].p[P_REV] = 0;
    }
}
static void cs_notes(uint32_t part, int on, uint32_t base)
{
    static const uint8_t CH[3] = {0, 7, 12};
    for (uint32_t k = 0; k < 3u; k++) {
        if (on)
            trk_note_on(&trk[part], 48u + base + CH[k], 110);
        else
            trk_note_off(&trk[part], 48u + base + CH[k]);
    }
}
static void cs_scenes(uint32_t mode)
{
    uint32_t k, g;
    cs_setup(mode);
    cs_blk(CS_1S / 2u);                                         /* 1 silence from power-on (the line is zero: the skip starts after 2048 samples) */
    for (k = 0; k < 40u; k++) {                                 /* 2 DEPTH and RATE turned while silent */
        song.g[G_CDEPTH] = (int16_t)(k * 3u);
        song.g[G_CRATE] = (int16_t)(100u - k);
        cs_blk(3);
    }
    cs_notes(0, 1, 0);                                          /* 3 a chord, chorus send 0, a reverb send 0: nothing reaches the bus */
    cs_blk(CS_1S / 2u);
    cs_notes(0, 0, 0);
    cs_blk(CS_1S / 2u);
    trk[0].p[P_CHOR] = 127;                                     /* 4 the send up: the line fills, then a held chord with DEPTH and RATE turned */
    trk[1].p[P_CHOR] = 90;
    cs_notes(0, 1, 0);
    cs_notes(1, 1, 5);
    for (k = 0; k < 60u; k++) {
        song.g[G_CDEPTH] = (int16_t)(127u - (k * 2u));
        song.g[G_CRATE] = (int16_t)(k * 2u);
        cs_blk(8);
    }
    cs_notes(0, 0, 0);                                          /* 5 the release and the tails: the line empties, the skip starts again */
    cs_notes(1, 0, 5);
    cs_blk(CS_1S * 2u);
    for (g = 20; g <= 140u; g += 20u) {                         /* 6 short notes, gaps around the line's length (2048 samples = 64 blocks) */
        cs_notes(0, 1, g % 24u);
        cs_blk(9);
        cs_notes(0, 0, g % 24u);
        cs_blk(g * 2u / 3u + 40u);
        song.g[G_CDEPTH] = (int16_t)(g);
    }
    cs_blk(CS_1S);
    trk[0].p[P_CHOR] = 0;                                       /* 7 the reverb alone */
    trk[1].p[P_CHOR] = 0;
    trk[0].p[P_REV] = 100;
    song.g[G_RTYPE] = 2;
    cs_notes(0, 1, 3);
    cs_blk(CS_1S / 2u);
    cs_notes(0, 0, 3);
    cs_blk(CS_1S * 2u);
    trk[0].p[P_REV] = 0;
    trk[0].p[P_DLY] = 110;                                      /* 8 the delay (its tail goes through the plain bus), then its modulation, PING-PONG, RANDOM */
    song.g[G_DFDBK] = 90;
    cs_notes(0, 1, 2);
    cs_blk(CS_1S / 3u);
    cs_notes(0, 0, 2);
    cs_blk(CS_1S);
    song.g[G_DMDEP] = 50;
    trk[0].p[P_CHOR] = 100;
    cs_notes(0, 1, 2);
    cs_blk(CS_1S / 3u);
    cs_notes(0, 0, 2);
    cs_blk(CS_1S);
    trk[0].p[P_CHOR] = 0;
    cs_blk(CS_1S / 2u);
    song.g[G_DMDEP] = 0;
    cs_blk(CS_1S / 2u);                                         /*  .. back to the plain bus with the modulation gliding out */
    song.g[G_DMODE] = DMD_PING;
    trk[0].p[P_CHOR] = 100;
    cs_notes(0, 1, 9);
    cs_blk(CS_1S / 3u);
    cs_notes(0, 0, 9);
    cs_blk(CS_1S / 2u);
    trk[0].p[P_CHOR] = 0;
    cs_blk(CS_1S);
    song.g[G_DMODE] = DMD_RAND;
    song.g[G_DMDEP] = 40;
    trk[0].p[P_CHOR] = 100;
    cs_notes(0, 1, 9);
    cs_blk(CS_1S / 3u);
    cs_notes(0, 0, 9);
    trk[0].p[P_CHOR] = 0;
    cs_blk(CS_1S);
    song.g[G_DMODE] = DMD_DIGITAL;
    cs_blk(CS_1S * 3u);
    cs_state();
}
static void cs_fuzz(uint32_t mode, uint32_t seconds)
{
    uint32_t t, e;
    cs_setup(mode);
    for (t = 0; t < seconds * CS_1S / 8u; t++) {
        uint32_t part = cs_rnd(2);
        switch (cs_rnd(14)) {
        case 0: case 1: cs_notes(part, 1, cs_rnd(24)); break;
        case 2: case 3: cs_notes(part, 0, cs_rnd(24)); break;
        case 4: trk[part].p[P_CHOR] = (int16_t)(cs_rnd(3) == 0 ? 0 : cs_rnd(128)); break;
        case 5: trk[part].p[P_DLY] = (int16_t)(cs_rnd(3) == 0 ? 0 : cs_rnd(128)); break;
        case 6: trk[part].p[P_REV] = (int16_t)(cs_rnd(3) == 0 ? 0 : cs_rnd(128)); break;
        case 7: song.g[G_CDEPTH] = (int16_t)cs_rnd(128); break;
        case 8: song.g[G_CRATE] = (int16_t)cs_rnd(128); break;
        case 9: song.g[G_DMDEP] = (int16_t)(cs_rnd(3) == 0 ? 0 : cs_rnd(100)); break;
        case 10: song.g[G_DMODE] = (int16_t)(cs_rnd(4) == 0 ? DMD_PING : cs_rnd(5) == 0 ? DMD_RAND : cs_rnd(6) == 0 ? DMD_OCTUP : DMD_DIGITAL);   /* (0.2.22: in DUEL too, per bus) */ break;
        case 11: song.g[G_RTYPE] = (int16_t)cs_rnd(6); break;
        case 12: for (e = 0; e < 3u; e++) trk_note_off(&trk[part], 48u + cs_rnd(36)); break;
        default: break;
        }
        cs_blk(1u + cs_rnd(cs_rnd(5) == 0 ? 120u : 12u));      /* mostly short, now and then a gap past the line's 64 blocks */
    }
    cs_state();
}
int main(void)
{
    static const char *const MN[3] = {"STEREO", "LAYER", "DUEL"};
    static const uint32_t MD[3] = {DM_STEREO, DM_LAYER, DM_SPLIT};
    uint32_t m;
    uint32_t skipped = 0, blocks = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (m = 0; m < 3u; m++) {
        cs_scenes(MD[m]);
        printf("HASH %s scenes out %016llx lines+state %016llx\n", MN[m], (unsigned long long)cs_h, (unsigned long long)cs_hs);
        skipped += cho_skipped + cho_skipped_b;
        blocks += cs_blocks;
        printf("chorusskip: %s scenes: %u blocks, the plain bus left the chorus out of %u (A) + %u (B)\n", MN[m], cs_blocks, cho_skipped, cho_skipped_b);
        cs_xs = 0x2545F491u + m;
        cs_fuzz(MD[m], 6);
        printf("HASH %s fuzz out %016llx lines+state %016llx\n", MN[m], (unsigned long long)cs_h, (unsigned long long)cs_hs);
        skipped += cho_skipped + cho_skipped_b;
        blocks += cs_blocks;
        printf("chorusskip: %s fuzz: %u blocks, left out %u (A) + %u (B)\n", MN[m], cs_blocks, cho_skipped, cho_skipped_b);
    }
#ifdef CHO_NOSKIP
    expect("reference build (-DCHO_NOSKIP): the chorus is never left out", skipped == 0);
#else
    expect("skip build: the plain bus left the chorus out of a good share of the blocks (a silent or send-0 mix)", skipped > blocks / 10u);
#endif
    printf("chorusskip: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
