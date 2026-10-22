/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.14 STOP ALL flushes the effect lines (fx.c snd_panic: the MODULES SENDS OFF machinery as a one-shot). M02, SPACE +100, every mode
 * (LAYER / DUEL / STEREO / SPLIT's DUEL) and every reverb type: after STOP ALL the level is under -80 dB within 0.3 s, no click at the fade (the largest
 * sample-to-sample step stays inside the signal's own), the sends are back (the next chord's level within 1.5 dB of a never-flushed run), a double press on a
 * stopped unit flushes too, a single STOP leaves the tails, the user's SENDS OFF stays OFF. Build: see host_tests.sh (panic) */
#define STOPTAIL_NO_MAIN 1
#include "stoptail_test.c"
static int fails;
static void expect(const char *what, int ok)
{
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static uint32_t CHORD;
typedef struct { double t80, rms_held, rms_a, rms_b, rms_c, step_after, step_before, rms_new; uint32_t st_end, panic_end, off_end; } res_t;
static double db(double e) { return e > 0 ? 10.0 * log10(e / (32768.0 * 32768.0)) : -200.0; }
/* kind: 0 reference (no first chord, a chord at 3 s), 1 STOP ALL at 2 s, 2 single STOP at 2 s (transport_req 2), 3 STOP ALL with SENDS OFF set by the user,
 *       4 the unit never played (stopped), the keys released, then STOP ALL (a double press on a stopped unit) */
static res_t scen(uint32_t dmode, uint32_t rtype, uint32_t kind)
{
    static int32_t out[2 * CTL], prev[2];
    res_t r;
    uint32_t b, k, blk = 0;
    double e[3] = {0, 0, 0}, en = 0, eh = 0, t80 = 0;
    uint32_t n[3] = {0, 0, 0}, nn = 0, nh = 0;
    int32_t maxstep_b = 0, maxstep_a = 0;
    memset(&r, 0, sizeof r);
    ui_power_on();
    for (k = 0; k < NTRK; k++) memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
    song.master_q12 = 2048;
    for (k = 0; k < 27u; k++)
        if (kb_map(&trk[0], k) == 60u || kb_map(&trk[0], k) == 64u || kb_map(&trk[0], k) == 67u)
            CHORD |= 1u << k;
    duell_mode_req = dmode;
    scene_put(1, "M02_WHISPER_TIDE");                 /* (0.2.23: the test scene, tests/scenes.h) */
    project_load(1);
    for (b = 0; b < 8u; b++) { mix_block(out, CTL); while (wave_service()) ; }
    song.g[G_RTYPE] = (uint8_t)rtype;
    song.g[G_RLOCUT] = 75;                              /* (the LOCUT on, its default: its filter state is flushed too) */
    for (b = 0; b < 8u; b++) { mix_block(out, CTL); while (wave_service()) ; }
    perf_space = 100; sv.spc = 100 * 256;
    if (kind == 3) mods_off |= MODS_SENDS;
    if (kind != 4 && kind != 0) transport_req = 1;
    prev[0] = prev[1] = 0;
    for (b = 0; b < 7u * SEC; b++) {                 /* 0..2 s music, 2 s the stop, 2..3 s quiet window, 3..5 s the chord again, 3..7 s the chord and its tail measured */
        uint32_t t10 = b * 100u / SEC;               /* in 10 ms steps */
        uint32_t i;
        int hold = (kind == 0) ? (t10 >= 300 && t10 < 500) : (kind == 4 ? (t10 < 200) : (t10 < 200 || (t10 >= 300 && t10 < 500)));
        fm1_in.notes = hold ? CHORD : 0u;
        if (b == 2u * SEC) {
            if (kind == 1 || kind == 3 || kind == 4) transport_req = 4;
            if (kind == 2) transport_req = 2;
            if (kind == 5) { transport_req = 4; memset(dly_buf, 0, sizeof dly_buf); snd_lines_clear(); }
        }
        if (b == 2u * SEC + 1u && kind == 4 && 0) {}
        mix_block(out, CTL);
        while (wave_service()) ;
        if (b >= 2u * SEC && b < 3u * SEC) {             /* the last block above -80 dB (blocks of CTL samples) */
            double q = 0;
            uint32_t z;
            for (z = 0; z < 2 * CTL; z++) q += (double)out[z] * out[z];
            if (db(q / (2 * CTL)) > -80.0) t80 = (b + 1u - 2u * SEC) * (double)CTL / FS;
        }
        for (i = 0; i < CTL; i++) {
            int32_t l = out[2 * i], rr = out[2 * i + 1], s;
            s = l - prev[0]; if (s < 0) s = -s;
            if (b < 2u * SEC) { if (s > maxstep_b) maxstep_b = s; }
            else if (b < 2u * SEC + SEC / 2u && s > maxstep_a) maxstep_a = s;
            prev[0] = l; prev[1] = rr;
            if (b >= 2u * SEC + 4u * SEC / 10u && b < 2u * SEC + 7u * SEC / 10u) { e[0] += (double)l * l + (double)rr * rr; n[0] += 2; }
            if (b >= 2u * SEC + 7u * SEC / 10u && b < 3u * SEC) { e[1] += (double)l * l + (double)rr * rr; n[1] += 2; }
            if (b >= 3u * SEC && b < 7u * SEC) { eh += (double)l * l + (double)rr * rr; nh += 2; }
            if (b >= 6u * SEC) { en += (double)l * l + (double)rr * rr; nn += 2; }
        }
        blk++;
    }
    (void)blk;
    r.t80 = t80; r.rms_held = nh ? eh / nh : 0;
    r.rms_a = n[0] ? e[0] / n[0] : 0; r.rms_b = n[1] ? e[1] / n[1] : 0; r.rms_new = nn ? en / nn : 0;
    r.step_before = maxstep_b; r.step_after = maxstep_a;
    r.st_end = snd_st; r.panic_end = snd_panic; r.off_end = (mods_off & MODS_SENDS) != 0u;
    return r;
}
int main(void)
{
    static const char *const dn[3] = {"DUEL", "STEREO", "LAYER"}, *const rn[6] = {"ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"};
    uint32_t m, t;
    memset(nor, 0xFF, sizeof nor);
    for (m = 0; m < 3u; m++)
        for (t = 0; t < 6u; t++) {
            char nm[160];
            res_t ref = scen(m, t, 5), fl = scen(m, t, 1), sg = scen(m, t, 2), st = scen(m, t, 4);
            double d = db(fl.rms_new) - db(ref.rms_new), dh = db(fl.rms_held) - db(ref.rms_held);
            printf("%s / %s: under -80 dB after %.0f ms; %.1f dB (0.4..0.7 s), %.1f dB (0.7..1 s); step %g (before %g); single STOP %.1f dB; stopped-unit %.1f dB; chord again %.2f dB (3..7 s: the chord and its tail) %.2f dB (6..7 s) vs the lines cleared at once\n",
                   dn[m], rn[t], fl.t80 * 1000.0, db(fl.rms_a), db(fl.rms_b), fl.step_after, fl.step_before, db(sg.rms_a), db(st.rms_a), dh, d);
            snprintf(nm, sizeof nm, "%s / %s: STOP ALL: below -80 dB from 0.4 s on (the dry path alone, SPACE's width and the master's filters, rings out to 0.38 s at worst with SPACE +100; the effect lines are empty after ~30 ms)", dn[m], rn[t]);
            expect(nm, db(fl.rms_a) < -80.0 && db(fl.rms_b) < -80.0 && fl.t80 <= 0.4);
            snprintf(nm, sizeof nm, "%s / %s: no click: largest step no more than the signal's own", dn[m], rn[t]);
            expect(nm, fl.step_after <= fl.step_before * 1.0 + 1.0);
            snprintf(nm, sizeof nm, "%s / %s: sends back: the next chord and its tail (4 s) within 1.5 dB of the same run with the lines cleared at once", dn[m], rn[t]);
            expect(nm, dh > -1.5 && dh < 1.5 && fl.st_end == SND_ON && !fl.panic_end);
            snprintf(nm, sizeof nm, "%s / %s: a double press on a stopped unit flushes too (< -80 dB from 0.4 s)", dn[m], rn[t]);
            expect(nm, db(st.rms_a) < -80.0);
            snprintf(nm, sizeof nm, "%s / %s: single STOP leaves the tails (above -60 dB)", dn[m], rn[t]);
            expect(nm, db(sg.rms_a) > -60.0);
        }
    {
        res_t so = scen(2, 2, 3);
        printf("SENDS OFF set, STOP ALL: state %u panic %u user OFF %u, the dry sound alone is under -80 dB after %.0f ms\n", so.st_end, so.panic_end, so.off_end, so.t80 * 1000.0);
        expect("SENDS OFF stays OFF after the panic (state OFF, switch still set, panic ended)", so.st_end == SND_OFF && so.off_end && !so.panic_end);
        expect("  .. and silent after the stop (< -80 dB)", db(so.rms_a) < -80.0);
    }
    printf("panic: %s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
