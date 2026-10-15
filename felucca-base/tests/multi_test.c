/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL MULTI (project.c multi.c, SAVE > MULTI, SPEC 5b.4) against simulated NOR flash:
 *   1. SAVE (KNOB 4, OCT+, the NAME screen, OCT+) of a distinct state: part 1 and part 2 on other engines (part 2 FM6 with
 *      an edited patch), a pattern on each, an automation lock, MODE LAYER, TEMPO, DLY TIME, MORPH TARGET BOTH and SMART
 *      OFF, End B rolled on both parts, the fader moved; after a power cycle (RAM gone, flash read again) LOAD (KNOB 2, OCT+)
 *      gives the same parameters, engines, steps, patch, automation, globals, MODE, TARGET, settings, End B (seed and the
 *      values it regenerates), the fader at A; the writes stay in the multi store's two ranges;
 *   2. sixteen slots, each its own, through a power cycle; rewriting one leaves its pair partner as it was; ERASE (dialog);
 *   3. a power cut during a slot write leaves both multis of the pair as they were (A/B, the commit record last);
 *   5. the factory multis; 6. (0.2.12) 0.2.11's six (0.2.13: M01 M02 M07..M10) played from the keys in LAYER, the transport stopped and with PLAY: both parts answer
 *      a key, follow another key, stop or stay latched on the release, and nothing plays without a key.
 *   build/host/multi_test            (DUELL/host_tests.sh "multi") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static long cut_bytes = -1;                         /* the power goes after that many programmed bytes */
static int dead, watch;
static uint32_t stray;                              /* (watch) bytes erased / programmed outside the multi store */
static void hit(uint32_t off, uint32_t n)
{
    if (watch && !((off >= 0x97000u && off + n <= 0x9F000u) || (off >= 0xCE000u && off + n <= 0xD6000u)))
        stray += n;
}
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (dead)
        return -8;
    hit(off, 4096u);
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    hit(off, n);
    for (i = 0; i < n; i++) {
        if (dead || (cut_bytes >= 0 && cut_bytes-- == 0)) {
            dead = 1;
            return -9;
        }
        nor[off + i] &= s[i];
    }
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static char nm_a[24];
static int bad;
static void ck(const char *what, int ok)
{
    printf("multi: %-104s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void power_cycle(void)                        /* the RAM is gone; the flash read again at power-on */
{
    uint32_t k;
    dead = 0;
    cut_bytes = -1;
    ui_power_on();
    for (k = 0; k < NTRK; k++)                       /* (the patches too: another sound than any saved) */
        memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
}
static void pattern(track_t *t, uint32_t k)          /* a pattern seeded by k, every field of a step used */
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        memset(s, 0, sizeof *s);
        step_clear(s);
        if ((i + k) % 3u == 2u)
            continue;                                /* (rests: the record leaves them out) */
        s->time = (i + k) % 7u == 3u ? ST_TIE : ST_NOTE;
        s->n = (uint8_t)(1u + (i + k) % 3u);
        s->note[0] = (uint8_t)(36u + (i * 5u + k * 3u) % 60u);
        s->note[1] = s->n > 1u ? (uint8_t)(s->note[0] + 4u) : 0u;
        s->note[2] = s->n > 2u ? (uint8_t)(s->note[0] + 7u) : 0u;
        s->flags = (uint8_t)((i + k) & 3u);
        step_set_ratchet(s, 1u + (i + k) % 4u);
        step_set_nudge(s, (int32_t)((i * 3u + k) % 16u) - 8);
        s->vel = (uint8_t)(20u + (i * 5u + k) % 100u);
        s->probability = (uint8_t)((i * 11u + k) % 102u);
    }
    t->p[P_SLEN] = (int16_t)(8u + (k * 7u) % 56u);
}
static void press_k(uint32_t knob)                   /* an action knob picked, OCT+ */
{
    turn(knob, 1);
    press(B_OCTUP);
}

/* 6 (DUELL 0.2.12): 0.2.11's six multis (0.2.13: M01 M02 M07..M10) are played from the keys. A multi loaded on a unit in LAYER (MODE RECALL OFF, the default), the
 * transport stopped: a key (or a chord) held for a while, then let go, the audio rendered block by block (mix_block); each part's
 * NOTE-ONS are the notes that become gated in a block (voice gate set, not set the block before). play_run returns the onsets of
 * part pi in on[pi][] (note, block), *n[pi] of them; *held1 / *held2: a part still gated after the release (latched); *late: onsets
 * after the release (from one step on); first[pi]: the block of its first onset */
#define PR_MAX 400u
typedef struct { uint8_t note[PR_MAX]; uint16_t blk[PR_MAX]; uint32_t n, first, late, gated_after; } pr_t;
static uint32_t pr_key(uint32_t note)                /* the key whose note (a power-on's octave and TRN 0) is note */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (kb_map(&trk[0], k) == note)
            return k;
    return 27u;
}
static void play_run(uint32_t slot, uint32_t keys, uint32_t hold_blocks, uint32_t after_blocks, int play, pr_t *r)
{
    static int32_t out[2 * CTL];
    uint8_t prev[NSHOWN][128], cur[128];
    uint32_t b, pi, v, n;
    power_cycle();
    duell_mode_req = DM_LAYER;                       /* (the owner's unit: LAYER; MODE RECALL OFF) */
    project_load(slot);
    memset(prev, 0, sizeof prev);
    memset(r, 0, NSHOWN * sizeof *r);
    for (pi = 0; pi < NSHOWN; pi++)
        r[pi].first = 0xFFFFu;
    for (b = 0; b < 8u; b++) {                       /* (the MODE taken, the tables built) */
        mix_block(out, CTL);
        while (wave_service())
            ;
    }
    if (play)
        transport_req = 1;
    for (b = 0; b < hold_blocks + after_blocks; b++) {
        fm1_in.notes = b < hold_blocks ? keys : 0u;
        mix_block(out, CTL);
        while (wave_service())
            ;
        for (pi = 0; pi < NSHOWN; pi++) {
            memset(cur, 0, sizeof cur);
            for (v = 0; v < NVOICE; v++)
                if (trk[pi].v[v].active && trk[pi].v[v].gate)
                    cur[trk[pi].v[v].note & 127u] = 1;
            for (n = 0; n < 128u; n++)
                if (cur[n] && !prev[pi][n]) {
                    if (r[pi].n < PR_MAX) {
                        r[pi].note[r[pi].n] = (uint8_t)n;
                        r[pi].blk[r[pi].n] = (uint16_t)b;
                    }
                    r[pi].n++;
                    if (r[pi].first == 0xFFFFu)
                        r[pi].first = b;
                    if (b >= hold_blocks + 2u * (uint32_t)(FS / CTL) / 10u)   /* (more than 1/5 s after the release: no tail of a step) */
                        r[pi].late++;
                }
            if (b == hold_blocks + after_blocks - 1u)
                for (n = 0; n < 128u; n++)
                    r[pi].gated_after += cur[n];
            memcpy(prev[pi], cur, sizeof cur);
        }
    }
    fm1_in.notes = 0;
    if (play)
        transport_req = 2;
    mix_block(out, CTL);
}
/* the onsets of a and b the same, b's notes d semitones higher (the same notes at the same blocks otherwise) */
static int pr_shifted(const pr_t *a, const pr_t *b, int32_t d)
{
    uint32_t i;
    if (a->n != b->n || !a->n)
        return 0;
    for (i = 0; i < a->n && i < PR_MAX; i++)
        if (a->blk[i] != b->blk[i] || (int32_t)b->note[i] != (int32_t)a->note[i] + d)
            return 0;
    return 1;
}
static void keys_drive(void)
{
    static pr_t r[NSHOWN], q[NSHOWN], p[NSHOWN], z[NSHOWN];
    uint32_t c4, d4, e4, g4, k, ok, sec = (uint32_t)(FS / CTL), chord;
    memset(nor, 0xFF, sizeof nor);                   /* (the factory multis, nothing saved) */
    power_cycle();
    c4 = pr_key(60); d4 = pr_key(62); e4 = pr_key(64); g4 = pr_key(67);
    chord = 1u << c4 | 1u << e4 | 1u << g4;
    if (0) {
    uint32_t c4 = pr_key(60), d4 = pr_key(62), e4 = pr_key(64), g4 = pr_key(67), k, ok, sec = (uint32_t)(FS / CTL);
    }
    ck("(the keys C4 D4 E4 G4 found on a power-on's keyboard)", c4 < 27u && d4 < 27u && e4 < 27u && g4 < 27u);
    /* the bass and house multis: one key, 2 s, then 1 s let go, the transport stopped (0.2.13 slots: M08 GARGLE ACID, M01 REESE SWAMP, M09 NIGHT ORGAN) */
    static const uint8_t BASS[3] = {7, 0, 8}, AMB[3] = {6, 1, 9};
    uint32_t j;
    for (j = 0; j < 3u; j++) {
        k = BASS[j];
        play_run(k, 1u << c4, 2u * sec, sec, 0, r);
        ok = !song.playing && r[0].n >= 1u && r[1].n >= 4u && r[0].late == 0u && r[1].late == 0u && !r[0].gated_after && !r[1].gated_after;
        if (k != 8u)                                 /* (M08, M01: part 1 plays the key itself, at once, one note for the hold) */
            ok &= r[0].first <= 1u && r[0].n == 1u && r[0].note[0] == 60u - 24u;
        else                                         /* (M09: the organ's chord stabs, part 1 an arp too: off-beat 1/8) */
            ok &= r[0].n >= 4u && r[0].first < sec / 2u;
        ok &= r[1].first < sec / 2u;                 /* (part 2's arp from the key-down, on the internal clock) */
        printf("multi:   M%02u C4 held 2 s, stopped: part 1 %u note-ons (first in block %u), part 2 %u (first in block %u); after the release %u / %u\n",
               k + 1u, r[0].n, r[0].first, r[1].n, r[1].first, r[0].late, r[1].late);
        ck(k == 7u ? "0.2.12 M08 GARGLE ACID, LAYER, STOPPED: a key plays both parts (bass at once, the acid arp's steps), the release ends both"
           : k == 0u ? "0.2.12 M01 REESE SWAMP: the same (the Reese at once, the 8-step acid arp), the release ends both"
                     : "0.2.12 M09 NIGHT ORGAN: the organ stabs and the bass arp from the key (no PLAY), the release ends both", ok);
        if (getenv("PR_DBG")) {
            uint32_t i, j;
            for (j = 0; j < 2u; j++) {
                printf("part %u:", j + 1u);
                for (i = 0; i < r[j].n && i < PR_MAX; i++) printf(" %u@%u", r[j].note[i], r[j].blk[i]);
                printf("\n");
            }
        }
        play_run(k, 1u << d4, 2u * sec, sec, 0, q);
        ck("  another key (D4): both parts play the same figure two semitones up (follows the keys)", pr_shifted(&r[0], &q[0], 2) && pr_shifted(&r[1], &q[1], 2));
        play_run(k, 0u, 2u * sec, 0u, 1, z);
        play_run(k, 1u << d4, 2u * sec, sec, 1, p);
        ck("  PLAY: no key, nothing plays (no stored pattern of its own); a key with PLAY plays what it plays stopped",
           z[0].n == 0u && z[1].n == 0u && pr_shifted(&r[0], &p[0], 2) && pr_shifted(&r[1], &p[1], 2));
    }
    /* the ambient and cinematic ones: a chord, then let go: both parts sound from the chord and stay latched (HOLD ALL) */
    for (j = 0; j < 3u; j++) {                       /* (0.2.13 slots: M07 COLD CHAPEL, M02 WHISPER TIDE, M10 FINAL REEL) */
        k = AMB[j];
        play_run(k, chord, sec, 3u * sec, 0, r);
        ok = r[0].n >= 3u && r[0].first <= 1u && r[1].n >= 1u && r[0].gated_after >= 3u && r[1].late == (k == 9u ? 0u : r[1].late) &&
             (k == 9u ? r[1].gated_after >= 3u : r[1].late >= 1u);
        printf("multi:   M%02u C4 E4 G4 1 s, 3 s let go: part 1 %u note-ons, %u notes still gated; part 2 %u (%u after the release), %u gated\n",
               k + 1u, r[0].n, r[0].gated_after, r[1].n, r[1].late, r[1].gated_after);
        ck(k == 6u ? "0.2.12 M07 COLD CHAPEL: a chord latches the pad, the bowl arpeggiates it slowly and goes on after the release"
           : k == 1u ? "0.2.12 M02 WHISPER TIDE: a chord latches the pad, the choir picks its notes and goes on after the release"
                     : "0.2.12 M10 FINAL REEL: a chord latches the brass and the strings (both keyed, no arp)", ok);
        play_run(k, 0u, 2u * sec, 0u, 1, z);
        ck("  PLAY without a key: nothing plays", z[0].n == 0u && z[1].n == 0u);
    }
}

int main(void)
{
    static track_t ref[NSHOWN];
    static uint8_t patch[FP_SIZE + 1u], endb[NSHOWN][P_COUNT], fendb[NSHOWN][NFM];
    static motion_store_t mref;
    uint32_t seedb[NSHOWN], k, pi, ok;
    memset(nor, 0xFF, sizeof nor);
    power_cycle();

    /* 1: a distinct state, saved to M05 through the page */
    set_engine_of(&trk[0], 3u);                      /* part 1: engine 3, part 2: FM6 (other engines) */
    apply_preset_to(&trk[0], 1u);
    set_engine_of(&trk[1], ENGI_FM6);
    apply_preset_to(&trk[1], 2u);
    trk[0].p[P_LEVEL] = 91;
    trk[1].p[P_PAN] = -20;
    trk[1].p[P_AMODE] = 2;                           /* (the arp is a part's parameter) */
    fm6_patch[1][3] = (uint8_t)((fm6_patch[1][3] + 17u) % 99u);   /* the FM6 patch edited: its own (SLOT OWN) */
    fm6_adopt(1);
    pattern(&trk[0], 1u);
    pattern(&trk[1], 2u);
    trk[0].p[P_M2SRC] = MS_LFO, trk[0].p[P_M2DST] = MD_RSIZE, trk[0].p[P_M2AMT] = 25;   /* 0.2.7: the new destinations travel too */
    trk[1].p[P_M6SRC] = MS_ENV2, trk[1].p[P_M6DST] = MD_EQHI, trk[1].p[P_M6AMT] = -40;
    trk[1].p[P_M8SRC] = MS_MODW, trk[1].p[P_M8DST] = MD_DMRATE, trk[1].p[P_M8AMT] = 60;
    motion.count = 1;                                /* an automation lock on part 2 */
    motion.event[0].place = (uint8_t)(1u << 6 | 5u);
    motion.event[0].param = P_ATK | MOTION_LOCK;
    motion.event[0].value = 33;
    motion.on = 2;
    song.g[G_BPM] = 137;
    song.g[G_DTIME] = 5;
    song.g[G_DMODE] = DMD_PING;
    song.g[G_DMDEP] = 37;                                /* the delay's modulation (0.2.5) travels with the multi */
    song.g[G_DMRATE] = 51;
    song.g[G_RTYPE] = 3;
    song.g[G_RLOCUT] = 77;                               /* the reverb's low cut (0.2.14) travels with the multi */
    song.g[G_SHPITCH] = SHP_BOTH;                        /* SHIM's PITCH and MOD (0.2.19) travel with the multi */
    song.g[G_SHMOD] = 37;
    frame();
    duell_mode_req = DM_LAYER;
    morph_set_target(1);
    morph_set_cfg(MC_WILD, 1);                       /* WILD */
    frames(100);
    morph_roll(1);                                   /* End B rolled (both parts: TARGET BOTH) */
    morph_set_fader(70);
    frame();
    song.playing = 0;
    go_title("MULTIS");
    song.g[G_SLOT] = 11;
    frame();
    ck("SAVE > MULTI is SAVE's page, sixteen slots (KNOB 1 M01..M16), M11 empty (M01..M10: the factory multis)", cur_page()->graph == GR_SLOTS &&
       cur_page()->fam == FAM_SAVE && GP[G_SLOT].max == 16 && !project_used(10));
    watch = 1;
    press_k(EN_K4);                                  /* SAVE: an empty slot: the NAME screen */
    ck("SAVE on an empty slot opens NAME (placeholder MULTI M11)", name_on() && str_eq(nm.ph, "MULTI M11"));
    str_cpy(nm.s, "DUEL JAM", sizeof nm.s);
    nm.len = nm.cur = 8;
    press(B_OCTUP);
    watch = 0;
    ck("OCT+: SAVED M11, written in the multi store's ranges only", msg_is("SAVED M11") && project_used(10) && !stray);
    for (pi = 0; pi < NSHOWN; pi++) {                /* (what was saved: the save may roll End B again from its seed) */
        ref[pi] = trk[pi];
        seedb[pi] = morph_seed_of(pi, 1);
        memcpy(endb[pi], morph_cur(&mph[pi], 1), P_COUNT);
        memcpy(fendb[pi], mph[pi].fh[1][mph[pi].top[1]], NFM);
    }
    memcpy(patch, fm6_patch[1], sizeof patch);
    mref = motion;
    power_cycle();
    ck("after a power cycle: M11 DUEL JAM listed, M12 empty, M01 still the factory multi; the state is the power-on one", project_used(10) &&
       !project_used(11) && project_factory(0) && str_eq(mu_names[10], "DUEL JAM") && trk[0].eng_req != 3u &&
       song.g[G_BPM] != 137 && duell_mode_req != DM_LAYER);
    go_title("MULTIS");
    song.g[G_SLOT] = 11;
    frame();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;              /* (0.2.11: this test reads the stored MODE back: MODE RECALL ON; the option's own tests are below) */
    press_k(EN_K2);                                  /* LOAD (no dialog) */
    frames(50);
    ok = 1;
    for (pi = 0; pi < NSHOWN; pi++)
        ok &= !memcmp(trk[pi].p, ref[pi].p, sizeof ref[pi].p) && trk[pi].eng_req == ref[pi].eng_req &&
              !memcmp(trk[pi].step, ref[pi].step, sizeof ref[pi].step);
    ck("LOAD: LOADED M11; both parts' engines, every parameter (sound, arp, mix, LEN .., 0.2.7's matrix DSTs) and all 64 steps equal",
       !ui.confirm && msg_is("LOADED M11") && ok);
    ck("  part 2's FM6 patch (edited) equal; the automation lock equal", !memcmp(fm6_patch[1], patch, sizeof patch) &&
       motion.count == mref.count && !memcmp(&motion.event[0], &mref.event[0], sizeof mref.event[0]) && motion.on == mref.on);
    ck("  TEMPO, DLY TIME, PING-PONG, DLY MODULATION DEPTH / RATE, REVERB TYPE; MODE LAYER; WILD; the saved TARGET BOTH not loaded (0.2.9: ALGORITHM's, not saved)",
       song.g[G_BPM] == 137 && song.g[G_DTIME] == 5 && song.g[G_DMODE] == DMD_PING && song.g[G_DMDEP] == 37 && song.g[G_DMRATE] == 51 && song.g[G_RTYPE] == 3 && song.g[G_RLOCUT] == 77 &&
       duell_mode_req == DM_LAYER && morph.target == MT_SELECTED && (morph.cfg & MC_WILD));
    ck("  SHIM's PITCH BOTH and MOD 37 (0.2.19: the ids of the old PING-PONG switch and NAME card) are back", song.g[G_SHPITCH] == SHP_BOTH && song.g[G_SHMOD] == 37);
    ok = 1;
    for (pi = 0; pi < NSHOWN; pi++)
        ok &= morph_seed_of(pi, 1) == seedb[pi] && !memcmp(morph_cur(&mph[pi], 1), endb[pi], P_COUNT) &&
              !memcmp(mph[pi].fh[1][mph[pi].top[1]], fendb[pi], NFM) && mph[pi].fader == 0u;
    ck("  End B of each part: the same seed and the same values (FM6 bytes too); the fader at A", ok);

    /* the scale is the instrument's (core.h): a multi's stored per-part SCALE / QNT (ids reserved, format unchanged) is read and ignored on load,
     * and a load never touches the global SCALE, ROOT, OFF-SCALE */
    trk[0].p[P_SCALE] = 9; trk[1].p[P_SCALE] = 4; trk[0].p[P_QUANT] = 2;
    scale_g = 7; root_g = 5; offscale_g = OS_PLAY;
    project_save_as(15, "SCALE TST");
    trk[0].p[P_SCALE] = trk[1].p[P_SCALE] = trk[0].p[P_QUANT] = 0;
    scale_g = 3; root_g = 2; offscale_g = OS_SNAP;
    project_load(15);
    ck("a multi with a stored per-part SCALE / QNT loads without them; the global scale, root and OFF-SCALE are as they were",
       trk[0].p[P_SCALE] == 0 && trk[1].p[P_SCALE] == 0 && trk[0].p[P_QUANT] == 0 && scale_g == 3 && root_g == 2 && offscale_g == OS_SNAP);
    scale_g = 0; root_g = 0;

    /* 2: sixteen slots, each its own; a rewrite keeps the pair partner */
    for (k = 0; k < MU_SLOTS; k++) {
        char nm2[8] = "MULTI  ";   /* ("MULTI A" ..) */
        nm2[6] = (char)('A' + k);
        song.g[G_BPM] = (int16_t)(60 + k);
        trk[0].p[P_LEVEL] = (int16_t)(10 + k);
        pattern(&trk[1], 100u + k);
        watch = 1;
        project_save_as(k, nm2);
        watch = 0;
    }
    ck("16 saves: each SAVED, nothing written outside the multi store", project_used(15) && !stray);
    power_cycle();
    ok = 1;
    for (k = 0; k < MU_SLOTS; k++) {
        static track_t r;
        char nm2[8] = "MULTI  ";
        nm2[6] = (char)('A' + k);
        pattern(&r, 100u + k);
        project_load(k);
        ok &= project_used(k) && str_eq(mu_names[k], nm2) && song.g[G_BPM] == (int16_t)(60 + k) &&
              trk[0].p[P_LEVEL] == (int16_t)(10 + k) && !memcmp(trk[1].step, r.step, sizeof r.step) &&
              trk[1].p[P_SLEN] == r.p[P_SLEN];
    }
    ck("after a power cycle: M01..M16 each load their own (name, tempo, level, part 2's pattern)", ok);
    song.g[G_BPM] = 200;
    project_save_as(6, "NEW 7");                     /* M07: its partner is M08 */
    power_cycle();
    project_load(7);
    ok = str_eq(mu_names[7], "MULTI H") && song.g[G_BPM] == 67;
    project_load(6);
    ck("M07 rewritten: M07 the new one, its pair partner M08 as it was", ok && str_eq(mu_names[6], "NEW 7") && song.g[G_BPM] == 200);
    go_title("MULTIS");
    song.g[G_SLOT] = 12;
    frame();
    press_k(EN_K3);                                  /* ERASE: the dialog */
    ck("ERASE asks first (ERASE M12?)", ui.confirm == CF_ERASE_MULTI);
    press(B_OCTUP);
    power_cycle();
    ck("  OCT+: ERASED; after a power cycle M12 empty, its partner M11 kept", !project_used(11) && project_used(10) &&
       str_eq(mu_names[10], "MULTI K"));
    go_title("MULTIS");
    song.g[G_SLOT] = 4;
    frame();
    press_k(EN_K4);
    ck("SAVE over a used slot asks OVERWRITE M04?", ui.confirm == CF_OVR_PROJ);
    press(B_OCTDN);

    /* 3: a power cut during the write of M10 (the second range: object 4, 0xCE000): M09 and M10 as they were */
    song.g[G_BPM] = 222;
    cut_bytes = 1500;
    ck("a power cut during M10's write: SAVE ERROR", project_save_as(9, "CUT") != 0);
    power_cycle();
    project_load(9);
    ok = str_eq(mu_names[9], "MULTI J") && song.g[G_BPM] == 69;
    project_load(8);
    ck("  after the power comes back: M10 and its partner M09 as they were", ok && song.g[G_BPM] == 68 && str_eq(mu_names[8], "MULTI I"));

    /* 4: MIDI PROGRAM CHANGE / BANK SELECT (SPEC 4.2a) and the MOD WHEEL on the MORPH fader, through the real input path */
    {
        static const uint8_t TRS_PC[] = {0xC1u, 3u};          /* (TRS: PC 3 on channel 2) */
        uint32_t e, kk, i;
        int16_t p0[P_COUNT];
        power_cycle();
        song.g[G_ROUTE] = 0;                                  /* (ROUT CH1-4) */
        duell_mode_req = DM_LAYER;
        events_block(CTL);
        song.g[G_BPM] = 99;
        midi_in_event(0x0Cu | 0xC0u << 8 | 11u << 16);        /* PC 11 ch 1: M12, erased above (empty) */
        midi_in_event(0x0Cu | 0xC0u << 8 | 40u << 16);        /* PC 40: no multi */
        events_block(CTL); frame();
        ok = song.g[G_BPM] == 99;
        song.g[G_BPM] = 123;
        project_save_as(11, "PC M12");
        song.g[G_BPM] = 99;
        duell_mode_req = DM_LAYER;
        events_block(CTL);
        midi_in_event(0x0Cu | 0xC0u << 8 | 11u << 16);
        events_block(CTL); frame();
        ck("LAYER: PC 11 ch 1 loads M12 (TEMPO 123); while M12 was empty and PC 40: nothing", ok && song.g[G_BPM] == 123 &&
           proj_cur == 11u);

        duell_mode_req = DM_SPLIT;
        events_block(CTL);
        set_engine_of(&trk[1], ENGI_FM6);
        trk[1].p[P_LEVEL] = 55;
        memcpy(p0, trk[0].p, sizeof p0);
        e = ds_list_at(3, &kk);                          /* (program 3 = slot 3 = the factory sound at browse position 3) */
        for (i = 0; i < sizeof TRS_PC; i++)
            um_byte(TRS_PC[i]);
        events_block(CTL); frame();
        ok = e < NENGINES && trk[1].eng_req == e && trk[1].preset == kk && !trk[1].user &&   /* (DUELL: a factory sound's */
             trk[1].p[P_TRIM] == ds_value(ds_get(e, kk), P_TRIM, TP[P_TRIM].def) &&          /* TRIM is its loudness; 0.2.19: the mixer level 55 stays) */
             trk[1].p[P_LEVEL] == 55 &&
             !memcmp(trk[0].p, p0, sizeof p0) && song.sel == 0u;
        up_store_trk(&trk[0], 5u, "PC USER", 0);              /* a user sound in STORE slot 5 */
        midi_in_event(0x0Bu | 0xB1u << 8 | 0u << 16 | 0u << 24);    /* bank 0 / 1 on ch 2 */
        midi_in_event(0x0Bu | 0xB1u << 8 | 32u << 16 | 1u << 24);
        midi_in_event(0x0Cu | 0xC1u << 8 | 5u << 16);
        events_block(CTL); frame();
        ok &= trk[1].user == 6u && trk[1].eng_req == trk[0].eng_req;
        midi_in_event(0x0Cu | 0xC1u << 8 | 7u << 16);         /* bank 1 kept: slot 7 holds no saved sound, its factory sound loads */
        events_block(CTL); frame();
        {
            uint32_t k7, e7 = ds_list_at(7, &k7);
            ck("SPLIT: PC 3 ch 2 (TRS) = slot 3 on part 2 only (TRIM: the sound's; the mixer LEVEL stays); bank 1 + PC 5 = slot 5 (saved); PC 7 = slot 7 (the factory sound)",
               ok && trk[1].user == 0u && trk[1].eng_req == e7 && trk[1].preset == k7 && slot_of(&trk[1]) == 7u);
        }

        morph_set_target(0);
        midi_in_event(0x0Bu | 0xB0u << 8 | 1u << 16 | 127u << 24);  /* the wheel on ch 1 */
        events_block(CTL); frame();
        ok = mph[0].fader == 127u && mph[1].fader == 0u && trk[0].mw == 127u && trk[1].mw == 0u;
        duell_mode_req = DM_LAYER;
        events_block(CTL);
        midi_in_event(0x0Bu | 0xB0u << 8 | 1u << 16 | 40u << 24);
        events_block(CTL); frame();
        ck("CC1 127 ch 1: part 1's fader 127 (part 2's not), MODW 127; LAYER: CC1 40 both faders 40, both MODW",
           ok && mph[0].fader == 40u && mph[1].fader == 40u && trk[0].mw == 40u && trk[1].mw == 40u);
    }

    /* 5: the factory multis M01..M10 (build/gen/duell_multis.h): in ROM, shown while the slot is empty in flash (0.2.13 order: REESE SWAMP and
     * WHISPER TIDE first, then the four emulator-made ones, then COLD CHAPEL, GARGLE ACID, NIGHT ORGAN, FINAL REEL) */
    {
        static const char *const FN[10] = {"REESE SWAMP", "WHISPER TIDE", "EERIE SIGNAL", "WIRED SNAIL", "DEAD BUFFER", "ACID ETHER",
                                           "COLD CHAPEL", "GARGLE ACID", "NIGHT ORGAN", "FINAL REEL"};
        static const uint8_t FE[10][2] = {{ENGI_WAVE, 0}, {ENGI_WAVE, 5},            /* 0.2.11: WAVE + ANALOG / VOICE */
                                          {5, 7}, {0, 0}, {ENGI_WAVE, 11}, {0, 0},   /* VOICE WHEEL; ANALOG; WAVE NOISE (GRAIN's STRETCH until 0.2.8); ANALOG */
                                          {ENGI_WAVE, 9}, {ENGI_WAVE, 0}, {ENGI_WAVE, 0}, {ENGI_WAVE, 6}};   /* 0.2.11: WAVE + PHYS / ANALOG / ANALOG / TRIO */
        static const uint8_t FK[10][2] = {{12, 4}, {14, 6}, {2, 3}, {13, 15}, {17, 9}, {3, 0}, {3, 1}, {11, 4}, {4, 0}, {20, 9}};
        static const char *const SN[10][2] = {{"REESE BASS", "SQUELCH"}, {"DIGITAL PAD", "WHISPER PAD"},
                                              {"VESPERS", "JAZZ CLUB"}, {"SFORZANDO", "GLASS SHARDS"}, {"FROZEN AIR", "MODEM"},
                                              {"RUBBER BAND", "LADDER BASS"}, {"EVOLVING PAD", "SINGING BOWL"},
                                              {"GROWL BASS", "SQUELCH"}, {"DRAWBARS", "LADDER BASS"},
                                              {"BRASS PAD", "STRING MACH"}};
        static const uint8_t SIX[6] = {0, 1, 6, 7, 8, 9};             /* 0.2.11's six (gen_factory_multis.c): M01 M02 M07..M10 since 0.2.13 */
        static const uint8_t BPM[6] = {124, 77, 71, 127, 121, 71};   /* their TEMPO */
        static uint8_t before[0x100000];
        uint32_t n, st;
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        ok = 1;
        for (k = 0; k < 10u; k++)
            ok &= project_used(k) && project_factory(k) && str_eq(mu_names[k], FN[k]);
        ck("empty flash: M01..M10 are the factory multis (0.2.13: REESE SWAMP, WHISPER TIDE, EERIE SIGNAL .. FINAL REEL), M11 empty",
           ok && !project_used(10) && !project_factory(10));
        ok = 1;
        ui_prefs2 |= PREF_MODE_RECALL >> 16;           /* (0.2.11: LAYER is read back from the multi) */
        for (k = 0; k < 10u; k++) {                   /* each part's engine + factory index names the intended sound */
            project_load(k);
            for (pi = 0; pi < NSHOWN; pi++)
                ok &= trk[pi].eng_req == FE[k][pi] && trk[pi].preset == FK[k][pi] && str_eq(ds_name(FE[k][pi], FK[k][pi]), SN[k][pi]) &&
                      (morph_seed_of(pi, 1) != 0u);
            ok &= duell_mode_req == DM_LAYER && proj_cur == k;
        }
        ck("LOAD of M01..M10: the intended sound on each part (REESE BASS+SQUELCH, .., BRASS PAD+STRING MACH), LAYER, End B seeded", ok);
        {   /* 0.2.13 (owner): M01 REESE SWAMP's AMOUNT 17 % on both parts, End B rolled at 17 % (seeds 11710 / 41045) */
            project_load(0);
            ok &= morph_amount_of(0) == 17u && morph_amount_of(1) == 17u && morph_seed_of(0, 1) == 11710u && morph_seed_of(1, 1) == 41045u;
        }
        ck("  0.2.13: M01 REESE SWAMP: AMOUNT 17 % on both parts, End B its own (seeds 11710 / 41045)", ok);
        /* 0.2.11's six, driven by the keys since 0.2.12: no stored step pattern plays on PLAY; M07 COLD CHAPEL / M02 WHISPER TIDE latch the pad
         * (HOLD ALL) under a slow latched arp (UP 1/2, RND 1/1); M08 GARGLE ACID / M01 REESE SWAMP the bass on the keys and a 1/16 SEQUENCER acid
         * arp (16 / 8 steps); M09 the organ (chord keys MIN7) and the bass both 1/8 SEQUENCER arps of 8 steps; M10 both parts latched, no arp;
         * no key split stored */
        ok = 1;
        {
            static const int8_t AM[6][2] = {{0, 15}, {0, 4}, {0, 1}, {0, 15}, {15, 15}, {0, 0}};      /* (seq.c AM_*: UP 1, RND 4, SEQ 15) */
            static const int8_t AR[6][2] = {{-1, 2}, {-1, 7}, {-1, 6}, {-1, 2}, {1, 1}, {-1, -1}};    /* (N_DIV: 1/16 2, 1/8 1, 1/2 6, 1/1 7) */
            static const int8_t AH[6][2] = {{0, 0}, {2, 2}, {2, 2}, {0, 0}, {0, 0}, {2, 2}};
            static const int8_t LN[6][2] = {{0, 8}, {0, 0}, {0, 0}, {0, 16}, {8, 8}, {0, 0}};          /* (the SEQUENCER pattern's LEN; 0: no steps) */
            uint32_t j;
            for (j = 0; j < 6u; j++) {
                k = SIX[j];
                project_load(k);
                ok &= song.g[G_BPM] == BPM[j] && mix_xf == 0u && mix_split == 0u;
                for (pi = 0; pi < NSHOWN; pi++) {
                    const track_t *t = &trk[pi];
                    for (n = st = 0; n < NSTEP; n++)
                        st += t->step[n].time == ST_NOTE && t->step[n].n;
                    ok &= t->p[P_AMODE] == AM[j][pi] && t->p[P_AHOLD] == AH[j][pi];
                    ok &= AR[j][pi] < 0 || t->p[P_ARATE] == AR[j][pi];
                    ok &= LN[j][pi] ? st >= 2u && t->p[P_SLEN] == LN[j][pi] : st == 0u;
                    for (n = 0; n < NSTEP && LN[j][pi]; n++)   /* (offsets from C4 within two octaves, one note a step) */
                        ok &= t->step[n].time != ST_NOTE || (t->step[n].n == 1u && t->step[n].note[0] >= 36u && t->step[n].note[0] <= 84u);
                }
                ok &= trk[0].p[P_CHRD] == (k == 8u ? 7 : k == 9u ? 9 : 0);   /* (M10: BRASS PAD's own POW) */
            }
        }
        ck("  the six: tempo, no split, arps (M07/M02 slow latched, M08/M01 1/16 SEQ acid 16/8 steps, M09 1/8 SEQ 8+8), M09 MIN7, no stray steps", ok);
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
        memcpy(before, nor, sizeof nor);
        go_title("MULTIS");
        song.g[G_SLOT] = 1;
        frame();
        press_k(EN_K3);
        ck("ERASE of an unchanged factory slot: ALREADY FACTORY, no dialog, nothing written", msg_is("ALREADY FACTORY") && !ui.confirm &&
           !memcmp(before, nor, sizeof nor));
        song.playing = 0;
        song.g[G_BPM] = 151;
        project_save_as(3, "MY STAB");                /* M04 WIRED SNAIL overwritten (0.2.13; M02 before) */
        power_cycle();
        ck("SAVE over M04: after a power cycle it holds MY STAB (not factory), M03 M05 M06 still factory", str_eq(mu_names[3], "MY STAB") &&
           !project_factory(3) && project_factory(2) && project_factory(4) && project_factory(5));
        project_load(3);
        ck("  and loads the saved one (TEMPO 151)", song.g[G_BPM] == 151);
        go_title("MULTIS");
        song.g[G_SLOT] = 4;
        frame();
        press_k(EN_K3);
        press(B_OCTUP);
        power_cycle();
        project_load(3);
        ck("ERASE of the overwrite restores WIRED SNAIL (after a power cycle too); it loads again", project_factory(3) &&
           str_eq(mu_names[3], "WIRED SNAIL") && trk[0].preset == 13u && song.g[G_BPM] != 151);
        n = project_rename(5, "ELASTIC 2");
        power_cycle();
        ck("rename of the factory M06 (ACID ETHER) stores it under the new name; the music of the record is the factory one", n == 0u &&
           str_eq(mu_names[5], "ELASTIC 2") && !project_factory(5) && project_used(5));
        project_load(5);
        ck("  and it loads RUBBER BAND + LADDER BASS as before", trk[0].eng_req == 0u && trk[0].preset == 3u && trk[1].preset == 0u);
    }


    /* 3b: the CPU TEST multis M13..M16 (tools/factory_multis, tests/gen_test_multis.c) and GLO > TEST */
    {
        static const char *const TN[4] = {"TEST 1 BASE", "TEST 2 PHASE", "TEST 3 HEAVY", "TEST 4 MIX"};
        static const uint8_t TE[4][2] = {{7, 0}, {2, 0}, {12, 2}, {11, 3}};   /* WHEEL ANALOG; PHASE ANALOG; FM6 PHASE; NOISE LOFI */
        static const uint8_t TK[4][2] = {{3, 9}, {5, 9}, {4, 5}, {2, 2}};
        static const char *const TS[4][2] = {{"JAZZ CLUB", "SILK CHORUS"}, {"CRYSTAL CAVE", "SILK CHORUS"}, {"TINE ROOM", "CRYSTAL CAVE"},
                                              {"SEA ORGAN", "WOW FLUTTER"}};
        char b[24];
        uint32_t f0;
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        ok = 1;
        for (k = 0; k < 4u; k++)
            ok &= project_used(12u + k) && project_factory(12u + k) && str_eq(mu_names[12u + k], TN[k]);
        for (k = 10; k < 12u; k++)
            ok &= !project_used(k);
        ck("empty flash: M13..M16 are the factory TEST multis, M11 M12 stay empty", ok);
        ok = 1;
        for (k = 0; k < 4u; k++) {
            project_load(12u + k);
            for (pi = 0; pi < NSHOWN; pi++)
                ok &= trk[pi].eng_req == TE[k][pi] && trk[pi].preset == TK[k][pi] && str_eq(ds_name(TE[k][pi], TK[k][pi]), TS[k][pi]) &&
                      trk[pi].p[P_AMODE] != 0 && trk[pi].p[P_CHRD] == 6 && trk[pi].p[P_VOICE] == V_POLY && trk[pi].p[P_ITYPE] != 0 &&
                      trk[pi].p[P_EQLG] != 0 && trk[pi].p[P_DKDEP] != 0 && trk[pi].p[P_CHOR] && trk[pi].p[P_DLY] && trk[pi].p[P_REV];
            ok &= song.g[G_BPM] == 138 && duell_mode_req == (k == 2u ? DM_SPLIT : DM_LAYER) && proj_cur == 12u + k;
        }
        ck("LOAD of M13..M16: engines, sounds, arp ON, MAJ7 chords, POLY, INSERT, EQ, ducker, sends, 138 BPM, SPLIT only for TEST 3", ok);
        project_load(14);
        ck("  TEST 3 HEAVY: SHIM reverb, PING-PONG on, FLANGER inserts on both parts", song.g[G_RTYPE] == 3 && song.g[G_DMODE] == DMD_PING &&
           trk[0].p[P_ITYPE] == 7 && trk[1].p[P_ITYPE] == 7);
        /* the TEST screen: MENU > SYSTEM > TEST (K1 right on the item opens it), not a page of GLO */
        ui.menu = 1; ui.menu_sel = MI_TEST; frames(16);
        turn(EN_K1, 1);
        ck("MENU > SYSTEM > TEST: opened with K1 right; the TEST screen (GR_TEST), no parameter on its knobs", ui.menu == 3u && TEST_PAGE.graph == GR_TEST &&
           TEST_PAGE.id[0] == 0xFFu && str_eq(TEST_PAGE.title, "TEST"));
        frames(16);
        test_pick = 0xFFu;
        project_cur_slot();
        proj_cur = PROJ_NO_SLOT;
        turn(EN_K3, 1);
        ck("K3 LOAD MULTI right: the next used multi (from none: M02 after the first used M01) is loaded at once, 'LOADED M02 WHISPER TIDE'",
           proj_cur == 1u && msg_is("LOADED M02 WHISPER TIDE") && trk[0].preset == 14u && trk[1].eng_req == 5u && !ui.confirm);
        frame();
        frames(48);
        test_multi_name(b);
        ck("  the panel's multi line follows (M02 WHISPER TIDE)", str_eq(b, "M02 WHISPER TIDE"));
        turn(EN_K3, 2);
        ck("  2 detents right: M03 skipped through, M04 (stepping), loaded: M04 WIRED SNAIL", proj_cur == 3u && msg_is("LOADED M04 WIRED SNAIL"));
        turn(EN_K3, 6);
        ck("  6 more right: M10 FINAL REEL (the factory multis stepped through)", proj_cur == 9u && msg_is("LOADED M10 FINAL REEL"));
        turn(EN_K3, 1);
        ck("  one more right: empty M11 M12 skipped, M13 TEST 1 BASE (WHEEL JAZZ CLUB + ANALOG SILK CHORUS)", proj_cur == 12u &&
           trk[0].eng_req == 7u && trk[1].eng_req == 0u && str_eq(ds_name(trk[0].eng_req, trk[0].preset), "JAZZ CLUB") &&
           str_eq(ds_name(trk[1].eng_req, trk[1].preset), "SILK CHORUS") && msg_is("LOADED M13 TEST 1 BASE"));
        test_multi_name(b);
        sound_name(&trk[0], nm_a);
        ck("  the engine and patch names the panel draws follow (WHEEL / JAZZ CLUB, M13 TEST 1 BASE)", str_eq(b, "M13 TEST 1 BASE") &&
           str_eq(nm_a, "JAZZ CLUB") && str_eq(ENGINES[trk[0].eng_req]->name, "WHEEL"));
        turn(EN_K3, 40);
        ck("  many detents right: stops at the last used, M16 TEST 4 MIX (NOISE + LOFI)", proj_cur == 15u && trk[0].eng_req == 11u && trk[1].eng_req == 3u);
        f0 = (uint32_t)proj_cur;
        turn(EN_K3, 1);
        ck("  right at the end: nothing", proj_cur == f0);
        turn(EN_K3, -2);
        ck("  2 left: M14 TEST 2 PHASE (PHASE + ANALOG)", proj_cur == 13u && trk[0].eng_req == 2u);
        turn(EN_K3, -2);
        ck("  2 more left: M13, then (empties skipped) M10", proj_cur == 9u);
        /* K4 RESET */
        cpu_miss = 7u;
        cpu_n = 1000u;
        cpu_sum = 400000u;
        test_t = 0;
        frames(160);
        cpum.hold = 91u;
        cpum.peak = 91u;
        cpum.miss = cpu_miss;
        cpum.base = 0u;
        ck("(a state to reset: 7 missed blocks, peak hold 91 %)", test_missed() == 7u && cpum.hold == 91u);
        turn(EN_K4, 1);
        ck("K4 RESET: 'RESET'; the peak hold, the missed count and the average window are cleared (the power-on count of ABOUT stays)",
           msg_is("RESET") && test_missed() == 0u && cpum.hold < 91u && cpu_pk == 0u && cpu_miss == 7u && cpum.n0 == cpu_n && !cpu_recent());
        cpu_miss = 9u;
        frames(1200);
        ck("  a new miss after RESET is counted from zero (2) and shows as recent", test_missed() == 2u && cpu_recent());
        cpu_miss = 0u;
        turn(EN_K4, -1);
        ck("  K4 turned left resets too", msg_is("RESET") && test_missed() == 0u);
        /* K1 MORPH, K2 ROLL: the MORPH page's */
        project_load(12);
        ck("  (still on the TEST screen after a load)", ui.menu == 3u);
        morph_set_target(0);
        song.sel = 0;
        turn(EN_K1, 20);
        ck("K1 MORPH: the selected part's fader moves (MORPH TARGET: selected)", morph_fader_of(0) > 0u && morph_fader_of(1) == 0u);
        morph_set_target(2);
        f0 = morph_fader_of(0);
        turn(EN_K1, -5);
        ck("  MORPH TARGET BOTH: both parts' faders", morph_fader_of(0) < f0 && morph_fader_of(1) == morph_fader_of(0));
        turn(EN_K2, -MORPH_DET);
        ck("K2 ROLL left past the detent: ROLLED A (as the MORPH page)", msg_is("ROLLED A"));
        frames(600);
        turn(EN_K2, MORPH_DET);
        ck("  right: ROLLED B", msg_is("ROLLED B"));
        ck("  the TEST screen is still shown (K1..K4 never leave it, K1 left included)", ui.menu == 3u);
        press(B_OCTDN);
        ck("  OCT-: back to the menu tabs, TEST the row", ui.menu == 1u && ui.menu_sel == MI_TEST);
        /* ABOUT's first view is the splash animation: endless, frozen by ANIM OFF, gone once SELECT scrolls */
        ui.menu = 2; ui.menu_scroll = 0; about_t0 = fm1_ms;
        frames(32);
        f0 = about_step();
        frames(2000);
        ck("ABOUT, first view: the animation steps on without end (a step every 66 ms, 2 s later still running)", about_step() > f0 + 20u);
        ui_prefs |= PREF_ANIM_OFF;
        ck("  MENU > ANIM OFF: the first frame, still", about_step() == 0u);
        ui_prefs &= ~PREF_ANIM_OFF;
        ui.menu_scroll = 18;
        ck("  scrolled (SELECT): the information, no animation", about_step() == 0u);
        ui.menu = 0;
    }

    /* 4: the first start (DUELL 0.1). A unit whose settings sector holds nothing valid (erased, stock firmware's data, garbage) starts in
     * VIOLET, MODE LAYER, factory multi M01 loaded as the page's LOAD does it; the settings are then saved, so the next start is
     * an ordinary one (the default sounds are back only if nobody saved: here the saved settings win); an older / other valid
     * settings blob keeps its theme and mode, nothing is forced */
    {
        static track_t m01[NSHOWN];
        static persist_t pb;
        uint32_t m01_bpm, m01_mode, m01_bal, sector, i, m01_lo[2];
        int16_t g_copy[G_COUNT];
        /* the reference: what M01 loads to */
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        ui_prefs2 |= PREF_MODE_RECALL >> 16;           /* (0.2.11: the reference reads M01's stored MODE) */
        project_load(0);
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
        memcpy(m01, trk, sizeof m01);
        m01_bpm = (uint32_t)song.g[G_BPM];
        m01_mode = duell_mode_req;
        m01_bal = (uint32_t)(uint8_t)mix_bal;
        memcpy(g_copy, song.g, sizeof g_copy);
        ck("M01 is the factory REESE SWAMP (0.2.13; read through the factory path)", str_eq(mu_names[0], "REESE SWAMP") && project_factory(0));

        for (sector = 0; sector < 3u; sector++) {     /* 0 erased flash, 1 garbage (stock firmware's data), 2 zeros */
            memset(nor, 0xFF, sizeof nor);
            for (i = 0; sector && i < 0x2000u; i++)
                nor[0xFC000u + i] = sector == 1u ? (uint8_t)(i * 131u + 7u) : 0u;
            power_cycle();
            memset(&settings, 0, sizeof settings);      /* (.noinit after a power-off: nothing) */
            settings.palette = 7777u;
            first_boot_pending = 0;
            memset(&persist_saved, 0, sizeof persist_saved);
            persist_boot();
            settings_init();
            ck(sector == 0u ? "first start on erased flash: counted as new (no valid settings)" :
               sector == 1u ? "  garbage in the flash: counted as new too" : "  zeros in the flash: counted as new too", first_boot_pending == 1u);
            ck("  the theme is VIOLET (the default index, UI_DEFAULT_INDEX 0)", settings.palette == UI_DEFAULT_INDEX &&
               str_eq(UI_PALETTES[settings.palette].name, "VIOLET"));
            if (autosave_boot(1))
                first_boot_pending = 0;
            ck("  no autosave on a new unit", first_boot_pending == 1u);
            ck("  (before it runs the unit has its default sounds, not M01's)", trk[0].eng_req != m01[0].eng_req || trk[0].preset != m01[0].preset);
            frames(160);
            ck("  MODE is LAYER (asked for and in its settings byte)", duell_mode_req == DM_LAYER && ui_mode_set == DM_LAYER && m01_mode == DM_LAYER);
            ck("  M01 is loaded: both parts' engines, presets and sounds as M01 holds them",
               trk[0].eng_req == m01[0].eng_req && trk[0].preset == m01[0].preset && trk[1].eng_req == m01[1].eng_req &&
               trk[1].preset == m01[1].preset && !memcmp(trk[0].p, m01[0].p, sizeof trk[0].p) && !memcmp(trk[1].p, m01[1].p, sizeof trk[1].p));
            ck("  TEMPO, the globals and BALANCE as M01 holds them", (uint32_t)song.g[G_BPM] == m01_bpm && !memcmp(song.g, g_copy, sizeof g_copy) &&
               (uint8_t)mix_bal == m01_bal);
            ck("  the first-start flag is used up", first_boot_pending == 0u);
            events_block(CTL);                        /* (the audio side takes the MODE: LAYER)  */
            if (sector == 0u) {                       /* tester bug: a black key played a semitone apart on the two parts */
                uint32_t kk;
                for (kk = 0; kk < 2u; kk++) {
                    static const uint32_t KEY2[2] = {0, 7};   /* F3 / F#3, then C4 / C#4 (key 0 = F3) */
                    uint32_t j;
                    for (j = 0; j < 2u; j++) {
                        uint32_t key = KEY2[kk] + j, lo[2], n;
                        for (i = 0; i < 2u; i++)
                            lo[i] = 255u;
                        key_down(key);
                        for (n = 0; n < 400u; n++) {        /* (part 2's arp starts its notes on its clock: audio blocks) */
                            events_block(CTL);
                            for (i = 0; i < 2u; i++) {
                                uint32_t v;
                                for (v = 0; v < NVOICE; v++)
                                    if (trk[i].v[v].active && trk[i].v[v].gate && trk[i].v[v].note < lo[i])
                                        lo[i] = trk[i].v[v].note;
                            }
                        }
                        key_up(key);
                        frames(32);
                        if (j == 0u)
                            m01_lo[0] = lo[0], m01_lo[1] = lo[1];
                        else {
                            printf("multi: M01 keys %u,%u: part 1 lowest note %u -> %u, part 2 %u -> %u\n", key - 1u, key, m01_lo[0], lo[0], m01_lo[1], lo[1]);
                            ck("  M01 first boot: a key and the key above it (C, C#) are one semitone apart on BOTH parts, the same root on both (whole octaves apart)",
                               lo[0] == m01_lo[0] + 1u && lo[1] == m01_lo[1] + 1u && (m01_lo[1] + 120u - m01_lo[0]) % 12u == 0u &&
                               (lo[1] + 120u - lo[0]) % 12u == 0u && lo[0] < 128u && lo[1] < 128u);   /* (0.2.13: REESE SWAMP's bass is TRN -24) */
                        }
                    }
                }
            }
            frames(2400);
            if (sector == 0u) {                       /* (0.2.11: the write waits for silence; frames() renders no audio, so the voices of the keys played above never die: let them end) */
                uint32_t k, v;
                for (k = 0; k < NTRK; k++)
                    for (v = 0; v < NVOICE; v++)
                        trk[k].v[v].active = 0;
                frames(2400);
                settings_poll();                      /* (the main loop's call: frame() makes none) */
            }
            ck("  the settings are saved (a valid record in the flash now)", (st_load(OBJ_SETTINGS, &pb, sizeof pb) > 0) && pb.favorites.factory[15][9] == DM_LAYER);
            /* the next start: an ordinary one, theme and mode as saved */
            power_cycle();
            memset(&settings, 0, sizeof settings);
            settings.palette = 7777u;
            first_boot_pending = 0;
            persist_boot();
            settings_init();
            ck("  next start: not new any more (settings valid), VIOLET and LAYER kept", first_boot_pending == 0u && settings.palette == UI_DEFAULT_INDEX &&
               ui_mode_set == DM_LAYER);
            if (sector == 2u)
                break;
        }

        /* an older or other valid settings blob keeps its theme, mode, scale and root; the unit starts as it was left: the autosave is restored
         * (RESTORE LAST ON, bit 128 clear), not M01; DUELL 0.2.2: a part muted in the autosave starts audible */
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        memset(&pb, 0, sizeof pb);
        settings_export(&pb);
        pb.palette = palette_to_stored(4u);
        pb.favorites.factory[15][9] = DM_STEREO;
        pb.favorites.factory[15][26] = 1u;
        pb.favorites.factory[15][3] = 0x80u | 5u;          /* the global scale (format flag + scale 5) and root 7 */
        pb.favorites.factory[15][4] = 7u;
        pb.favorites.factory[15][30] &= (uint8_t)~128u;    /* RESTORE LAST ON (bit 7 clear, as every older blob) */
        ck("an older DUELL settings record (theme 4, STEREO, 0.1 beta's HOME view, a scale) is stored", st_save(OBJ_SETTINGS, &pb, sizeof pb) == 0);
        {
            uint32_t pass;
            power_cycle();                                 /* an autosave from an earlier session: part 1 MUTED, tempo 77 */
            host_tracks_init();
            trk[0].p[P_MUTE] = 1;
            song.g[G_BPM] = 77;
            project_capture(&proj_scratch);
            proj_wire_gen++;
            ck("  an autosave with a muted part 1 and tempo 77 is on the flash",
               proj_pack(&proj_wire, &proj_scratch) && st_save(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire) == 0);
            for (pass = 0; pass < 2u; pass++) {            /* 0: RESTORE LAST ON (the older blob), 1: OFF (bit 128 set) */
                power_cycle();
                memset(&settings, 0, sizeof settings);
                first_boot_pending = 0;
                if (pass) {
                    pb.favorites.factory[15][30] |= 128u;
                    ck("  RESTORE LAST OFF stored (a settings blob)", st_save(OBJ_SETTINGS, &pb, sizeof pb) == 0);
                    power_cycle();
                    memset(&settings, 0, sizeof settings);
                }
                persist_boot();
                settings_init();
                ck(pass ? "  OFF is read from the blob (editor id 15 value 1)" : "  the older blob reads RESTORE LAST ON (editor id 15 value 0)",
                   menu_get(MI_RESTORE) == pass && !!(ui_prefs & PREF_RESTORE_OFF) == (int)pass);
                if (autosave_boot(1))
                    first_boot_pending = 0;
                frames(300);
                if (!pass) {
                    ck("  power-on restores the autosave as left (tempo 77, no M01, not a first start)", first_boot_pending == 0u && song.g[G_BPM] == 77 &&
                       proj_cur == PROJ_NO_SLOT && !str_eq(proj_name, "REESE SWAMP"));
                    ck("  .. the muted part 1 starts audible (MUTE cleared at power-on)", !trk[0].p[P_MUTE] && !trk[1].p[P_MUTE]);
                    ck("  .. the theme (4), MODE (STEREO), scale 5 and root 7 stay as stored", settings.palette == palette_from_stored(pb.palette) &&
                       duell_mode_req == DM_STEREO && (ui_scale_set & 0xCFu) == (0x80u | 5u) && ui_root_set == 7u);
                } else {
                    ck("  RESTORE LAST OFF: the autosave is not restored, and it is not a first start either (no M01)", first_boot_pending == 0u &&
                       song.g[G_BPM] != 77 && !str_eq(proj_name, "REESE SWAMP") && proj_cur == PROJ_NO_SLOT);
                }
            }
        }
        /* a multi that holds a muted part: a LOAD keeps the stored mute (the GLO page shows MUTED) */
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        memset(&settings, 0, sizeof settings);
        persist_boot();
        settings_init();
        project_load(0);
        trk[1].p[P_MUTE] = 1;
        ck("  (M01 overwritten with part 2 muted)", project_save(0) == 0 && !project_factory(0));
        project_load(0);
        ck("  a LOAD of that multi keeps its stored mute (part 2 MUTED on the GLO page)", trk[1].p[P_MUTE] == 1 && !trk[0].p[P_MUTE]);
        /* DUELL 0.2.14: the shorter record of the first DUELL builds (no user_hi) is no record any more: refused, the unit starts as a new one */
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        memset(&pb, 0, sizeof pb);
        settings_export(&pb);
        pb.palette = palette_to_stored(4u);
        pb.favorites.factory[15][26] = 1u;
        ck("  the shorter older record (without the patches' favourites 32..127) is stored", st_save(OBJ_SETTINGS, &pb, (uint32_t)(sizeof pb - sizeof pb.favorites.user_hi)) == 0);
        power_cycle();
        memset(&settings, 0, sizeof settings);
        first_boot_pending = 0;
        persist_boot();
        settings_init();
        frames(100);
        ck("  and it is refused (0.2.14): the unit starts as new (its default theme, the home byte 0), nothing of the record taken over, no crash",
           settings.palette != palette_from_stored(pb.palette) && favorites.factory[15][26] == 0u && menu_get(MI_HOMEPG) == 0u);
    }


    /* 0.2.11: MENU > CONTROL > MODE RECALL (KEEP by default): a multi load leaves the unit's MODE; LOAD sets the one saved in the multi */
    {
        static const uint8_t MODES[3] = {DM_LAYER, DM_SPLIT, DM_STEREO};
        static persist_t pb;
        uint32_t m, k, pi;
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        ok = !(ui_prefs2 & (PREF_MODE_RECALL >> 16)) && menu_get(MI_MODERECALL) == 0u && str_eq(menu_vname(MI_MODERECALL, 0), "KEEPS MODE") &&
             str_eq(menu_vname(MI_MODERECALL, 1), "SETS MODE") && str_eq(MI_NAME[MI_MODERECALL], "MULTI LOAD") && MI_TAB[MI_MODERECALL] == MTAB_CONTROL;
        ck("MODE RECALL (0.2.14: MULTI LOAD KEEPS MODE / SETS MODE): KEEP by default (CONTROL tab)", ok);
        ok = 1;
        for (m = 0; m < 3u; m++)                      /* KEEP: M01, M02, M04 (all LAYER): the mode stays */
            for (k = 0; k < 3u; k++) {
                duell_mode_req = MODES[m];
                project_load(k == 0 ? 0u : k == 1 ? 1u : 3u);
                ok &= duell_mode_req == MODES[m];
                for (pi = 0; pi < NSHOWN; pi++)
                    ok &= trk[pi].eng_req < NENGINES;
            }
        ck("KEEP: a multi load leaves the MODE as it is, from LAYER, DUEL and STEREO alike", ok);
        ok = 1;
        for (m = 0; m < 3u; m++) {                    /* a multi saved in each mode: LOAD sets it, KEEP does not */
            duell_mode_req = MODES[m];
            ok &= project_save_as(8u + m, "MODE") == 0;
        }
        for (m = 0; m < 3u; m++) {
            duell_mode_req = MODES[(m + 1u) % 3u];
            project_load(8u + m);
            ok &= duell_mode_req == MODES[(m + 1u) % 3u];
        }
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
        for (m = 0; m < 3u; m++) {
            duell_mode_req = MODES[(m + 1u) % 3u];
            project_load(8u + m);
            ok &= duell_mode_req == MODES[m];
            {
                uint32_t len;
                const uint8_t *r;
                mu_obj_read((8u + m) / 2u);
                r = mu_rec((8u + m) & 1u, &len);
                ok &= len > 4u && r[4] == MODES[m];   /* (the header byte 4 is the saved MODE) */
            }
        }
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
        ck("a save still writes the MODE (header byte 4); LOAD applies it from any mode, KEEP does not", ok);
        {   /* BALANCE, XFADE, SPLIT follow the option too (saved in the multi either way) */
            uint32_t ref_bal;
            ok = 1;
            for (m = 0; m < 3u; m++) {
                duell_mode_req = MODES[m];
                mix_bal = -37; mix_xf = 41; mix_split = 61;
                ok &= project_save_as(8u, "BAL") == 0;
                mix_bal = 23; mix_xf = 17; mix_split = 71;
                project_load(8u);
                ok &= mix_bal == 23 && mix_xf == 17 && mix_split == 71;
                ui_prefs2 |= PREF_MODE_RECALL >> 16;
                project_load(8u);
                ok &= mix_bal == -37 && mix_xf == 41 && mix_split == 61;
                ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
            }
            ck("BALANCE / XFADE / SPLIT: OFF keeps them after a load (from each mode), ON applies the multi's", ok);
            mix_bal = 5; mix_xf = 6; mix_split = 7;
            ui_prefs2 |= PREF_MODE_RECALL >> 16;
            project_load(0);
            ref_bal = (uint32_t)(uint8_t)mix_bal;
            ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
            mix_bal = 5;
            project_load(0);
            ck("  M01 (factory): ON applies its own BALANCE, OFF leaves it", mix_bal == 5 && ref_bal != 5u);
        }
        /* a MIDI program change recalls through the same project_load */
        duell_mode_req = DM_STEREO;
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
        project_load(8u);
        ck("  (KEEP: also a recall by program change, the same path)", duell_mode_req == DM_STEREO);
        /* the option in the settings: an older blob (bit clear) reads KEEP; LOAD survives a save and a power cycle */
        memset(&pb, 0, sizeof pb);
        settings_export(&pb);
        pb.favorites.factory[15][24] = 0x07u;
        memset(nor, 0xFF, sizeof nor);
        ck("  an older settings blob (bits 0-2 only) is stored", st_save(OBJ_SETTINGS, &pb, (uint32_t)sizeof pb) == 0);
        power_cycle();
        memset(&settings, 0, sizeof settings);
        persist_boot();
        settings_init();
        ck("  ..reads MODE RECALL KEEP (editor id 27 value 0), the other three flags kept", menu_get(MI_MODERECALL) == 0u && (ui_prefs2 & 7u) == 7u);
        menu_put(MI_MODERECALL, 1);
        settings_export(&pb);
        ck("  LOAD is saved in bit 6 of favorites.factory[15][24]", pb.favorites.factory[15][24] == (0x47u | 0x80u));   /* (+ 0.2.14's defaults flag, bit 7) */
        memset(&favorites, 0, sizeof favorites);
        ck("  ..and imported again as LOAD", settings_import(&pb, (int)sizeof pb) && menu_get(MI_MODERECALL) == 1u);
        ui_prefs2 &= (uint8_t)~(PREF_MODE_RECALL >> 16);
    }

    {   /* DUELL 0.2.14: a multi record of firmware before (version 2 or 1, or another P_COUNT) in flash is no multi: its slot lists as empty,
         * a LOAD changes nothing and does not crash, a factory slot shows its factory multi, the next SAVE replaces the old record */
        static uint8_t old[MU_REC_MAX], keep[MU_REC_MAX];
        uint32_t len, cut = 0, o, v, ok2 = 1;
        char nm[PROJ_NAME_LEN + 1u];
        ui_power_on();
        trk[0].p[P_LEVEL] = 77;
        ok2 &= project_save_as(10u, "OLD ONE") == 0 && project_used(10u);
        for (v = 0; v < 3u; v++) {
            o = 10u / 2u;
            mu_obj_read(o);
            {
                const uint8_t *r = mu_rec(10u & 1u, &len);
                memcpy(old, r, len);
            }
            if (v == 0u) old[0] = 2u; else if (v == 1u) old[0] = 1u; else { old[0] = 3u; old[1] = (uint8_t)(P_COUNT - 1u); }
            memcpy(keep, old, len);
            mu_obj_read(o);
            ok2 &= mu_obj_write(o, mu_put_rec(10u & 1u, 1, 0, &cut, keep, len)) == 0;
            mu_obj_read(o);
            mu_note(o);
            trk[0].p[P_LEVEL] = 55;
            ok2 &= !project_used(10u) && !project_info(10u, nm) && !nm[0] && !project_factory(10u);
            project_load(10u);                       /* (EMPTY SLOT: nothing loaded) */
            ok2 &= trk[0].p[P_LEVEL] == 55;
            ok2 &= project_erase(10u) == 1;          /* (an empty slot: nothing to erase) */
            trk[0].p[P_LEVEL] = 77;
        }
        ck("a multi record of version 2 / 1 or another P_COUNT: the slot lists empty (\"--\"), a LOAD and an ERASE change nothing", ok2);
        /* a factory slot (M01) holding such a record shows its factory multi */
        o = 0;
        mu_obj_read(o);
        {
            uint32_t l0;
            const uint8_t *r = mu_rec(0u, &l0);
            ok2 = DM_LEN[df_of(0u)] <= sizeof old;
            memcpy(old, DM_REC + DM_AT[df_of(0u)], DM_LEN[df_of(0u)]);
            old[0] = 2u;
            (void)r;
            ok2 &= mu_obj_write(o, mu_put_rec(0u, 1, 0, &cut, old, DM_LEN[df_of(0u)])) == 0;
        }
        mu_obj_read(o);
        mu_note(o);
        ok2 &= project_used(0u) && project_factory(0u) && project_info(0u, nm) && !strcmp(nm, "REESE SWAMP");
        ck("  a factory slot (M01) holding one shows its factory multi (REESE SWAMP) and loads it", ok2 && (project_load(0u), 1));
        ok2 = project_save_as(10u, "NEW ONE") == 0 && project_used(10u) && project_info(10u, nm) && !strcmp(nm, "NEW ONE");
        ck("  a SAVE over the old record replaces it, the multi loads again", ok2);
        project_erase(10u);
    }
    keys_drive();                                    /* 6: M05..M10 played from the keys (0.2.12) */
    printf(bad ? "multi: FAILED (%d)\n" : "multi: all ok\n", bad);
    return bad ? 1 : 0;
}
