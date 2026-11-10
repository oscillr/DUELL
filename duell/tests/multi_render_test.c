/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.17: the factory multis render bit for bit as they did before the 14 new factory sounds (bank 114 -> 128).
 * A factory multi (tools/factory_multis/Mnn.bin) carries its sounds as parameter bytes plus the identity (engine, factory index);
 * adding sounds appends new indices per engine and moves only the browse numbers (#NN), so a multi must sound the same.
 * DUELL 0.2.23: the sixteen factory multis M01..M16 (tools/factory_multis.py; loaded as with MODE RECALL ON: their MODE, BALANCE, XFADE)
 * and the fourteen TEST SCENES of tests/scenes.h (0.2.22's factory multis and CPU TEST multis, byte for byte, loaded as they were then: the CPU
 * TEST ones with their MODE, the others in the power-on MODE), whose hashes are the ones of 0.2.22. Each is loaded as the LOAD page does it and
 * played (both parts: a chord of 3 notes, 1.5 s, then the release, 1 s), at MORPH fader 0, 63 and 127 (End A, the middle, End B: the seeds
 * and rolls of the part's sound), and the output is hashed (FNV-1a 64). Every render runs in its own fork()ed child (state as at boot:
 * noise seeds, FX buffers, limiter).
 *   build/host/multi_render_test GOLDEN        compare with the file (tests/multi_render_golden.txt)
 *   MULTI_RENDER_UPDATE=1 ... GOLDEN           write it (ONLY on a firmware whose sounds are the reference) */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
#include "scenes.h"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static const uint8_t CH1[3] = {48, 55, 64}, CH2[3] = {60, 67, 72};
/* the renders: a factory slot (scene 0) or a scene loaded into the slot it had (recall: with its MODE) */
typedef struct { const char *name; uint32_t slot; const char *scene; int recall; } rn_t;
static const rn_t RN[] = {
    {"M01", 0, 0, 1}, {"M02", 1, 0, 1}, {"M03", 2, 0, 1}, {"M04", 3, 0, 1}, {"M05", 4, 0, 1}, {"M06", 5, 0, 1}, {"M07", 6, 0, 1}, {"M08", 7, 0, 1},
    {"M09", 8, 0, 1}, {"M10", 9, 0, 1}, {"M11", 10, 0, 1}, {"M12", 11, 0, 1}, {"M13", 12, 0, 1}, {"M14", 13, 0, 1}, {"M15", 14, 0, 1}, {"M16", 15, 0, 1},
    {"S01_REESE_SWAMP", 0, "M01_REESE_SWAMP", 0}, {"S02_WHISPER_TIDE", 1, "M02_WHISPER_TIDE", 0}, {"S03_EERIE_SIGNAL", 2, "M03_EERIE_SIGNAL", 0},
    {"S04_WIRED_SNAIL", 3, "M04_WIRED_SNAIL", 0}, {"S05_DEAD_BUFFER", 4, "M05_DEAD_BUFFER", 0}, {"S06_ACID_ETHER", 5, "M06_ACID_ETHER", 0},
    {"S07_COLD_CHAPEL", 6, "M07_COLD_CHAPEL", 0}, {"S08_GARGLE_ACID", 7, "M08_GARGLE_ACID", 0}, {"S09_NIGHT_ORGAN", 8, "M09_NIGHT_ORGAN", 0},
    {"S10_FINAL_REEL", 9, "M10_FINAL_REEL", 0}, {"S13_TEST_1_BASE", 12, "M13_TEST_1_BASE", 1}, {"S14_TEST_2_PHASE", 13, "M14_TEST_2_PHASE", 1},
    {"S15_TEST_3_HEAVY", 14, "M15_TEST_3_HEAVY", 1}, {"S16_TEST_4_MIX", 15, "M16_TEST_4_MIX", 1},
    /* 0.2.24: the spare multis of 0.2.23 (tests/scenes/spare, tools/factory_multis.py SPARE), loaded as the factory slots are: the hashes they had as M06 / M07 / M14 */
    {"X06_TINE_STRINGS", 5, "spare/M06_TINE_STRINGS", 1}, {"X07_BOWL_AND_WIND", 6, "spare/M07_BOWL_AND_WIND", 1}, {"X14_SEVENOAKS", 13, "spare/M14_SEVENOAKS", 1},
    /* 0.2.25: BASILDON left M13 for the owner's COLD CHAPEL (spare data; its M13 hashes of 0.2.25) */
    {"X13_BASILDON", 12, "spare/M13_BASILDON", 1},
    /* 0.2.25: MONO DUEL left M08 for the owner's HAZY TENSOR (spare data; loaded as a test scene in its old slot, with its MODE: DUEL) */
    {"X08_MONO_DUEL", 7, "spare/M08_MONO_DUEL", 1}};
#define NR (sizeof RN / sizeof *RN)
static const uint32_t FAD[3] = {0, 63, 127};

static uint64_t render(const rn_t *rn, uint32_t fader)
{
    uint64_t h = 1469598103934665603ull;
    FILE *dump = 0;                                     /* MULTI_DUMP=DIR (cloud job J's proof renders): the output as int32 left / right pairs to DIR/<name>_f<fader>.i32 */
    if (getenv("MULTI_DUMP")) {
        char dn[256];
        snprintf(dn, sizeof dn, "%s/%s_f%u.i32", getenv("MULTI_DUMP"), rn->name, (unsigned)fader);
        dump = fopen(dn, "wb");
    }
    int32_t o[2 * CTL];
    uint32_t i, b, nb = (uint32_t)(1.5 * FS) / CTL, nr = (uint32_t)(1.0 * FS) / CTL;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    multi_boot();
    frame();
    song.master_q12 = 4096;
    if (rn->scene) {                                    /* the test scenes assume BASS MONO OFF (0.2.22's hashes); 0.2.24: a unit setting, ON at 200 Hz on a new unit: set OFF here, explicitly */
        scene_put(rn->slot, rn->scene);
        song.bm = 0;
        frame();
    }
    if (rn->recall)
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(rn->slot);
    for (i = 0; i < 40; i++) { memset(o, 0, sizeof o); mix_block(o, CTL); while (wave_service()) ; }
    morph_set_target(1);
    morph_set_fader(fader);
    for (i = 0; i < 3; i++) { trk_note_on(&trk[0], CH1[i], 100); trk_note_on(&trk[1], CH2[i], 100); }
    for (b = 0; b < nb + nr; b++) {
        if (b == nb)
            for (i = 0; i < 3; i++) { trk_note_off(&trk[0], CH1[i]); trk_note_off(&trk[1], CH2[i]); }
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        while (wave_service()) ;
        for (i = 0; i < 2u * CTL; i++) { h ^= (uint32_t)o[i]; h *= 1099511628211ull; }
        if (dump)
            fwrite(o, 4, 2u * CTL, dump);
    }
    if (dump)
        fclose(dump);
    return h;
}

int main(int argc, char **argv)
{
    char line[128];
    uint64_t got[3 * NR];
    uint32_t m, f, bad = 0, n = 0;
    int upd = getenv("MULTI_RENDER_UPDATE") != 0;
    FILE *g;
    if (argc < 2)
        return fprintf(stderr, "usage: multi_render_test GOLDEN\n"), 2;
    for (m = 0; m < NR; m++)
        for (f = 0; f < 3u; f++) {
            int pf[2];
            uint64_t h = 0;
            if (pipe(pf))
                return 1;
            if (!fork()) {
                h = render(&RN[m], FAD[f]);
                if (write(pf[1], &h, 8) != 8)
                    _exit(1);
                _exit(0);
            }
            if (read(pf[0], &h, 8) != 8)
                h = 0;
            wait(0);
            close(pf[0]); close(pf[1]);
            got[m * 3 + f] = h;
        }
    if (upd) {
        g = fopen(argv[1], "w");
        if (!g)
            return perror(argv[1]), 1;
        fprintf(g, "# tests/multi_render_test.c: FNV-1a 64 of each factory multi's output (3-note chords on both parts, fader 0 / 63 / 127).\n"
                   "# Written on 0.2.16's sounds (114): the 14 new factory sounds of 0.2.17 must not change it.\n"
                   "# Re-baselined on purpose in 0.2.22: the reverb LOW CUT's new range (20 Hz .. 2 kHz) and default (75 = 299 Hz; the factory multis carry it): 40 of the 42 renders; with LOCUT 0 all 42 are the 0.2.21 ones.\n"
                   "# 0.2.23: M01..M16 the sixteen factory multis of 0.2.23 (new); Snn_NAME the test scenes (tests/scenes.h): the 0.2.22 hashes of Mnn, unchanged.\n"
                   "# 0.2.24: the owner's COLD CHAPEL, ACID ETHER, GARGLE ACID back (M06, M09, M14 = the S07, S06, S08 records, loaded with recall), FINAL REEL M08 -> M07,\n"
                   "# MONO DUEL M09 -> M08, M03 PAD SPLIT's pad CUT opened; Xnn_NAME the spare multis of 0.2.23 (tests/scenes/spare): their 0.2.23 hashes as M06 / M07 / M14.\n"
                   "# 0.2.24: BASS MONO is a unit setting, ON at 200 Hz on a new unit: the factory multis render with it (their STEREO / LAYER hashes re-made once); the test scenes set it OFF (hashes as before).\n"
                   "# 0.2.25: the part EQ in 32-bit multiplies with its rounding carried (fx.c eq_run): the 33 renders of the multis and scenes with an EQ re-made once (M12, M13, M15, M16, S05, S13..S16, X07, X14).\n"
                   "# 0.2.25: each EQ band on its own; the HIGH band runs whenever its gain is set (0.2.24 ran it only with MID Q off its middle): M02, M03, M08, M13, M16, S13..S16 re-made once.\n"
                   "# 0.2.25: SHIM's PITCH BOTH removed (owner): M16 STRESS TEST, the one multi that stored it, plays UP: its three renders re-made once.\n"
                   "# 0.2.25: M02..M06 are the owner's edited multis (his backup of 2026-10-17): their 15 hashes re-made once; the others are unchanged.\n"
                   "# 0.2.25: M13 is the owner's original COLD CHAPEL (= S07's record) in the slot of BASILDON: M13's three hashes re-made once; X13_BASILDON the old M13 record as a spare scene (loaded as a test scene, BASS MONO off: not the hashes it had as M13).\n"
                   "# 0.2.29: the HALL's quiet lines round to the nearest (fx.c hall_end: its tail no longer gated near -50 dBFS): the 51 renders of the multis and scenes with REVERB TYPE HALL\n"
                   "# re-made once (M07, M09..M13, S03..S07, S10, S13, S14, S16, X07, X14); the others are unchanged.\n"
                   "# 0.2.25: the SAT follower takes the matrix-modulated SAT (fx.c dist_follow, wip/satmod0230): M12, S05 re-made once.\n"
                   "# 0.2.25: M07 is the owner's refined FINAL REEL (his multi file of 2026-10-17): its three hashes re-made once.\n"
                   "# 0.2.25: M08 is the owner's HAZY TENSOR (his multi file of 2026-10-17) in the slot of MONO DUEL: M08's three hashes re-made once; X08_MONO_DUEL the old M08 record as a spare scene.\n"
                   "# 0.2.25 matrix round (release_drafts/MATRIX_DESTINATIONS.md): the source slew (mod.c mx_slew) M01 M02 M03 M05 M07 M08 M09 M12 M13 M14 M16, S01 S02 S05 S07 S08 S09 S10 S15,\n"
                   "# X06 X07 X14; the in-block mix ramps and SAT's blend ramp (fx.c mix_ramp, track_dist_ramp) M08 M09 M10 M12 S05: 66 of 105 renders re-made once; the others are unchanged.\n"
                   "# Job 3 (cloud/partstate2), on purpose, 7 of the 102: (1) one-sided ducking (DUCK SRC PART on one part only, the other part's ducker forced OFF, the key a block old):\n"
                   "# M02, whose part 1 had a PUMP ducker beside part 2's PART, loses that ducker and hears part 1's key a block later: its three renders re-made once. (2) each part its own\n"
                   "# random generator in the voice stage (part 2 no longer draws from the shared one): S13_TEST_1_BASE and S14_TEST_2_PHASE, the test scenes whose two parts both read the\n"
                   "# matrix's S&H source, at fader 63 and 127: four renders re-made once. Every other render is unchanged.\n"
                   "# hunt1 (release_drafts/CLICK_HUNT1.md): the fader's steps of the sends, PATCH LEVEL and PAN are ramped inside the block (mod.c mod_mixmv counts the open morph window): M08 f127,\n"
                   "# M09 f63 f127, M12 f63 f127 re-made once (2 to 16 samples in the first 12 ms differ, by at most 15 of a peak of 17,000: the step of the fader set at the start).\n"
                   "# hunt1: SAT out of 0 and back into 0 without a step (fx.c track_dist_ramp): M12 f127 and S05_DEAD_BUFFER f127 re-made once (their matrix route lets the followed SAT run down\n"
                   "# through the integer 0 at 1.43 s, 9 blocks; from there on the render differs by at most 89 of a peak of 12,900 / 19,700, 0.5 %). The other 103 renders are unchanged.\n"
                   "# hunt1: the SPREAD mix's gains in a line while a route or the MORPH window moves them, SAT's blend ramp on a SPREAD part (fx.c spread_run / spread_post): M03 f63 f127, M05 f0 f63 f127,\n"
                   "# M06 f63 f127, M07 f0 f63 f127, M10 f63 f127, M11 f63 f127 re-made once (14 of 105; the start of the render, the stored PAN's follower and the fader's first steps on SPREAD parts:\n"
                   "# the largest difference is 266 of a peak of 17,000 (M10 f127), most 4 to 95). The other 91 renders are unchanged.\n"
                   "# hunt1: LFO 1 and LFO 2 for their own depths glide at most MX_SLEW a block (voice.c track_render): 82 of 105 renders re-made once. Counted with a probe: in each of them the slew clamps a\n"
                   "# used LFO value (a note's restart of the LFO, a saw's / square's edge, an S&H step); in 10 more it clamps one that no output uses and the render is bit for bit as before (M02 f0, M07 f0,\n"
                   "# M10 f63 f127, M14 f63 f127, S04 f63 f127, S08 f63 f127); the other 13 never clamp. A pitched sound's samples differ by up to its peak after such a step (the pitch step comes a few\n"
                   "# ms later, so the phase differs from there on); that is not a click.\n"
                   "# pedal hunt (release_drafts/PEDAL_BASS_HUNT.md): the ducker keeps 2 more bits of every ducked sample (fx.c duck_run), and PHASE slews a jumping DCW / ENV / SUB (eng_phase.c pd_sl):\n"
                   "# the renders listed as CHANGED by the test before this commit were re-made once (the multis and scenes with a working ducker or a moving PHASE shape).\n\n"
                   "# hunt4 (release_drafts/HUNT4.md): PHASE's moving DCW no longer steps at the block seams (eng_phase.c phase_render_0): S14 TEST 2 PHASE, S15 TEST 3 HEAVY and X14 SEVENOAKS re-made.\n\n"
                   "# 1.0: the owner's duell-multis-13 file (13 records, byte for byte) is the factory M01..M05 and M08..M15 (M06, M07, M16 unchanged): their 39 renders re-made once; the test scenes are unchanged.\n"
                   "# 1.0 volume check (release_drafts/VOLUME_CHECK_V1.md): the TRIM of both parts of M01 M02 M04 M05 M06 M07 M08 M13 M15 moved by the same dB (tools/multi_trim.py): their 27 renders are made again (one re-base for 1.0 in all).\n"
                   "# 1.0 MORPH B (release_drafts/MORPH_B_ROLL_V1.md): End B of M01..M15 rolled again with the firmware's ROLL B (tools/morph_b_roll.py, fixed seeds): their fader 63 and 127 renders (30) made again; fader 0 (End A) is unchanged.\n"
                   "# 1.0 owner's answers: M04 DUB CHORD's TRIM one more step down (peak ceiling) and its seed k = 20: its three renders made again. M05 / M11 name VOICE sounds by their closed-up factory indices (CROONER left the bank): same sound, same hashes.\n");
        for (m = 0; m < NR; m++)
            for (f = 0; f < 3u; f++)
                fprintf(g, "%s f%u %016llx\n", RN[m].name, FAD[f], (unsigned long long)got[m * 3 + f]);
        fclose(g);
        printf("multi_render: wrote %s\n", argv[1]);
        return 0;
    }
    g = fopen(argv[1], "r");
    if (!g)
        return perror(argv[1]), 1;
    while (fgets(line, sizeof line, g)) {
        char nm[32]; unsigned fd; unsigned long long hv;
        if (line[0] == '#' || sscanf(line, "%31s f%u %llx", nm, &fd, &hv) != 3)
            continue;
        for (m = 0; m < NR; m++)
            for (f = 0; f < 3u; f++)
                if (!strcmp(nm, RN[m].name) && fd == FAD[f]) {
                    int ok = got[m * 3 + f] == hv;
                    printf("multi_render: %-16s fader %3u %016llx %s\n", nm, fd, (unsigned long long)got[m * 3 + f], ok ? "ok" : "CHANGED");
                    bad += !ok; n++;
                }
    }
    fclose(g);
    printf("multi_render: %u renders compared, %u changed\n", n, bad);
    return bad || n != 3u * NR;
}
