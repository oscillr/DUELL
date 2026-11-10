/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.19: SHIM's PITCH (UP, DOWN; BOTH until 0.2.24, a stored BOTH plays UP since 0.2.25) and MOD (core.h G_SHPITCH, G_SHMOD; fx_bus.c rev_shim / rev_shimx; REVERB page 2 "SHIMMER"). On mode_test.c's rig.
 *   build/host/shimmer_test [DEMODIR]       (DUELL/host_tests.sh "shimmer"; DEMODIR: the listening set, see below)
 * Built on the base commit (-DSHIMMER_BASE=1, no G_SHPITCH there) it prints the hashes of the SHIM before this change, which are the constants below.
 *   DEFAULT   PITCH UP, MOD 0 is the SHIM of 0.2.18 bit for bit: the bus alone (A and B, SIZE / DAMP moving, 6 s of bursts) and the whole mix (a pad and a pluck
 *             through STEREO, LAYER and DUEL)
 *   PITCH     a sine through the return (rev_shimx fed with a 430.66 Hz sine, 20 whole cycles in the 2048-sample line, the grains alone): UP 861.3 Hz, DOWN 215.3 Hz
 *             (within 3 cents), BOTH the two, each at half a single return and 7.9 cents sharp, and nothing at the input's own pitch; UP and DOWN at the same level
 *   MOD       the return's pitch drifts: 40 s in 0.5 s windows: about 0 cents at MOD 1, plus or minus 11 at 50, plus or minus 23 at 100 (25 at most), also on DOWN; the output
 *             differs from MOD 0 and each PITCH from the others, and stays bounded (loud noise, SIZE 127, DAMP 0: under the plain SHIM's peak x 1.5)
 *   CLICK     PITCH UP -> DOWN -> BOTH -> UP, MOD 0 <-> 100 -> 0 over a held tone: no sample step above the steady render's own x 1.5; a TYPE change away and back no worse
 *             than without PITCH and MOD
 *   TAILS     silence after the tails (loud noise, then nothing): every PITCH x MOD ends in exact 0 within 40 s (SIZE 90, 127, 150)
 *   BUSES     bus A and bus B render alike (UP, DOWN, BOTH, MOD); in DUEL a part alone: the other channel is exactly 0
 *   CLEAR     the state is zero after rev_clear, SENDS OFF and STOP ALL's flush (PITCH DOWN, MOD on), silent until a new note; the plain loop hands its phase over and back
 *   UI        REVERB / SHIMMER: PITCH (UP DOWN BOTH) and MOD, top-down; SHIMMER is in the page cycle only while TYPE is SHIM, and is left when TYPE changes; SENDS cards;
 *             carried by a multi
 *   COST      host instructions a sample of the SHIM bus alone: plain, MOD, DOWN, BOTH
 * Demos (WAV) into DEMODIR (argument): pads, plucks and chords (WAVE, NOISE, PHYS, ANALOG sounds) through SHIM: PITCH UP / DOWN / BOTH at MOD 0, 40 and 100, and index.html. */
#define MODE_TEST_NO_MAIN 1
#include "mode_test.c"

/* the SHIM before this change, rendered by the base commit (ff8df90) */
#define H_BUS_A 0x3c328d00u
#define H_BUS_B 0x091f03d3u
#define H_MIX_STEREO 0x379c68a0d823dec8ull   /* (0.2.31: the three mix hashes re-based on purpose, the factory sounds WAVE 1 and PHYS 2 were refined; were 0xb0349e95305d0465, 0xbeaf994edade7675, 0xbc2a1061abc66350)  (job 3, on purpose: part 2 draws its own random numbers in the voice stage (voice.c part_rand), so the shared sequence the PHYS pluck's note-on seed comes from is not advanced by part 2's LFO any more; the scene plays a pad and a PHYS pluck on two parts; before: 0x3f372eed50047294, 0x330c1c778ef2d2bc, 0x1820f49e32024f34. 0.2.25: re-pinned on purpose: #62 VOWEL PAD's LFO 1 -> SUB now glides within each block (eng_wave.c SUB without a zipper); before: 0x48d788950a6e2499, 0x44ecba0fd49884b1, 0x9630cfd1f0e803c2. 0.2.22: re-pinned a third time on purpose: the scene plays #62 VOWEL PAD and #75 GHOST HARP, which now have KEY TRK +51 % (owner); before that: 0x5d413c2d194a9478, 0xda3d519d67bc89d2, 0xee5127709d5c78ac. Earlier: re-pinned again on purpose: the scene plays #75 GHOST HARP, whose PATCH TRIM went 112 -> 113 for the new low cut (owner-approved); first pin at the new LOCUT default: 0x11b435e273a6282c, 0x38a7ce86978f2a06, 0x1d5a2be01b32b1eb. Then: re-pinned on purpose: the scene runs at the new LOCUT default, 75 = 299 Hz, the reverb low cut's new range; the bus alone, above, is unchanged. Before: 0x290f333a12786bd9, 0x9a25b46b919706e7, 0x8862eba6c6787015 at value 61 = 88 Hz) */
#define H_MIX_LAYER 0x61248d80e97da59cull   /* (DUELL 1.0: the three mix hashes re-based on purpose: the factory sounds WAVE 1 and PHYS 2 are the owner's bank now) */
#define H_MIX_DUEL 0xea63f53d61b8b378ull

static uint32_t xs0 = 0x1234567u;
static int32_t rnd(int32_t amp)
{
    xs0 ^= xs0 << 13;
    xs0 ^= xs0 >> 17;
    xs0 ^= xs0 << 5;
    return (int32_t)(((int64_t)(int32_t)xs0 * amp) / 2147483648LL);
}
static uint32_t hfnv(uint32_t h, int32_t v) { return (h ^ (uint32_t)v) * 16777619u; }
static uint64_t hash64(const int32_t *l, const int32_t *r, uint32_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < n; i++) {
        h ^= (uint32_t)l[i]; h *= 1099511628211ull;
        h ^= (uint32_t)r[i]; h *= 1099511628211ull;
    }
    return h;
}

static int32_t step_of_range(const int32_t *x, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    for (uint32_t i = a + 1u; i < b; i++) {
        int32_t d = x[i] - x[i - 1u];
        m = (d < 0 ? -d : d) > m ? (d < 0 ? -d : d) : m;
    }
    return m;
}

/* a clean SHIM on both buses (the sends of the caller's own) */
static void shim_start(int pitch, int mod, int size, int damp)
{
    host_tracks_init();
    memset(&fx, 0, sizeof fx);
    memset(&fx_b, 0, sizeof fx_b);
    memset(&hl, 0, sizeof hl);
    memset(&hl_b, 0, sizeof hl_b);
    rev_clear();
    rev_clear_b();
    song.g[G_RTYPE] = 3;
    song.g[G_RLOCUT] = 0;
    song.g[G_RSIZE] = (int16_t)size;
    song.g[G_RDAMP] = (int16_t)damp;
    fx.rtype = fx_b.rtype = 3;
#ifndef SHIMMER_BASE
    song.g[G_SHPITCH] = (int16_t)pitch;
    song.g[G_SHMOD] = (int16_t)mod;
#else
    (void)pitch; (void)mod;
#endif
}

#ifndef SHIMMER_BASE
/* ---------------------------------------------------------------- the pitch --- */
static double dft_mag(const int32_t *x, uint32_t n, double f)
{
    double re = 0, im = 0;
    for (uint32_t i = 0; i < n; i++) {
        double a = 2 * M_PI * f * i / FS;
        re += x[i] * cos(a);
        im += x[i] * sin(a);
    }
    return sqrt(re * re + im * im) / n;
}
/* the return of a sine into rev_shimx (the grains alone: t is what ROOM would give), n samples after `settle`; mod and pitch as asked */
#define F_IN (20.0 * 44100.0 / 2048.0)         /* 430.66 Hz: 20 whole cycles in SHIM's 2048-sample line, so a grain's restart is no phase jump (an off tone's pitch reads off by the jump) */
static int32_t ret_buf[40u * 44100u];
static void ret_run(int pitch, int mod, uint32_t settle, uint32_t n, double f_in, uint32_t bus_b)
{
    static int32_t t[CTL], o[CTL];
    uint32_t b, i, k = 0;
    shim_start(pitch, mod, 90, 60);
    for (b = 0; b < (settle + n) / CTL; b++) {
        for (i = 0; i < CTL; i++) {
            t[i] = (int32_t)(8000 * sin(2 * M_PI * f_in * (b * CTL + i) / FS));
            o[i] = 0;
        }
        if (bus_b) {
            if (fx_b.sh_x == 0)
                fx_b.sh_x = 0;
            rev_shimx_b(t, o, CTL);
        } else
            rev_shimx(t, o, CTL);
        if (b * CTL >= settle && k + CTL <= n)
            for (i = 0; i < CTL; i++)
                ret_buf[k++] = (bus_b ? shim_b : shim).fb[i];
    }
}
/* the frequency of x over [a, b): first and last rising zero crossing, interpolated */
static double zc_freq(const int32_t *x, uint32_t a, uint32_t b)
{
    double first = -1, last = -1;
    uint32_t cnt = 0;
    for (uint32_t i = a + 1u; i < b; i++)
        if (x[i - 1u] <= 0 && x[i] > 0) {
            double p = (i - 1u) + (double)(-x[i - 1u]) / (double)(x[i] - x[i - 1u]);
            if (first < 0) first = p;
            last = p;
            cnt++;
        }
    return cnt > 3u ? (cnt - 1u) * (double)FS / (last - first) : 0.0;
}
static double cents(double f, double ref) { return 1200.0 * log2(f / ref); }

/* ------------------------------------------------------------ the bus alone --- */
static void bus_hashes(uint32_t *ha, uint32_t *hb)
{
    static int32_t r[CTL], r2[CTL], w[CTL], w2[CTL];
    uint32_t b, i, h = 2166136261u, h2 = 2166136261u, nb = 6u * FS / CTL;
    shim_start(0, 0, 90, 60);
    xs0 = 0x2468ACEu;
    for (b = 0; b < nb; b++) {
        if (b % 500u == 0u) {
            song.g[G_RSIZE] = (int16_t)((uint32_t)rnd(1 << 30) % 128u);
            song.g[G_RDAMP] = (int16_t)((uint32_t)rnd(1 << 30) % 128u);
        }
        for (i = 0; i < CTL; i++) {
            r[i] = (b / 300u) % 3u != 2u ? rnd(b % 2000u < 1000u ? 60000 : 3000) : 0;
            r2[i] = r[i] / 2;
            w[i] = w2[i] = 0;
        }
        rev_run(3, r, w, CTL);
        rev_run_b(3, r2, w2, CTL);
        for (i = 0; i < CTL; i++) {
            h = hfnv(h, w[i]);
            h2 = hfnv(h2, w2[i]);
        }
    }
    *ha = h, *hb = h2;
}
#endif

/* ------------------------------------------------------------- the whole mix --- */
/* a part through SHIM. kind 0 pad (a long dyad + fifth), 1 pluck (a phrase), 2 chord (two stabs held). eng / k: the factory sound (hostsim.c host_ds_load). secs of audio into rec_l / rec_r */
static const uint8_t PADN[3] = {48, 55, 64}, PLKN[8] = {60, 64, 67, 72, 67, 64, 62, 59}, CHDA[4] = {57, 60, 64, 71}, CHDB[4] = {53, 57, 60, 69};
static void mix_scene(uint32_t mode, uint32_t e, uint32_t k, int kind, int pitch, int mod, uint32_t secs, int send)
{
    uint32_t b, nb = secs * FS / CTL, j, p;
    dsp_clean(mode);
    song.g[G_BPM] = 100;
    song.g[G_RTYPE] = 3;
    song.g[G_RSIZE] = 100;
    song.g[G_RDAMP] = 60;
    song.g[G_RLOCUT] = GP[G_RLOCUT].def;               /* (the new multi's default: 75 = 299 Hz since 0.2.22, 61 = 88 Hz before) */
    song.g[G_DMDEP] = 0;
    song.g[G_DMODE] = DMD_DIGITAL;
    fx.rtype = fx_b.rtype = 3;
#ifndef SHIMMER_BASE
    song.g[G_SHPITCH] = (int16_t)pitch;
    song.g[G_SHMOD] = (int16_t)mod;
#else
    (void)pitch; (void)mod;
#endif
    for (p = 0; p < 2u; p++) {
        host_ds_load(&trk[p], e, k);
        trk[p].p[P_REV] = (int16_t)send;
        trk[p].p[P_DLY] = trk[p].p[P_CHOR] = 0;
        trk[p].p[P_PAN] = 0;
    }
    rec_n = 0;
    for (b = 0; b < nb; b++) {
        uint32_t t = b * CTL, ms = FS / 1000u;
        for (p = 0; p < (mode == DM_SPLIT ? 1u : 1u); p++) {
            if (kind == 0) {
                if (t < CTL) for (j = 0; j < 3u; j++) trk_note_on(&trk[p], PADN[j], 90);
                if (t >= 3500u * ms && t < 3500u * ms + CTL) for (j = 0; j < 3u; j++) trk_note_off(&trk[p], PADN[j]);
            } else if (kind == 1) {
                uint32_t step = t / (450u * ms), w = t % (450u * ms);
                if (step < 8u && w < CTL) trk_note_on(&trk[p], PLKN[step], 100);
                if (step < 8u && w >= 150u * ms && w < 150u * ms + CTL) trk_note_off(&trk[p], PLKN[step]);
            } else {
                if (t < CTL) for (j = 0; j < 4u; j++) trk_note_on(&trk[p], CHDA[j], 90);
                if (t >= 900u * ms && t < 900u * ms + CTL) for (j = 0; j < 4u; j++) trk_note_off(&trk[p], CHDA[j]);
                if (t >= 2600u * ms && t < 2600u * ms + CTL) for (j = 0; j < 4u; j++) trk_note_on(&trk[p], CHDB[j], 90);
                if (t >= 3500u * ms && t < 3500u * ms + CTL) for (j = 0; j < 4u; j++) trk_note_off(&trk[p], CHDB[j]);
            }
        }
        mblk(1);
    }
}

#ifndef SHIMMER_BASE
static void wav(const char *path, const int32_t *l, const int32_t *r, uint32_t n, int32_t gain_q8)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, data = n * 4u, v;
    uint16_t s;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); v = 36u + data; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    v = FS; fwrite(&v, 4, 1, f); v = FS * 4u; fwrite(&v, 4, 1, f);
    s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (i = 0; i < n; i++) {
        int32_t a = (int32_t)(((int64_t)l[i] * gain_q8) >> 8), c = (int32_t)(((int64_t)r[i] * gain_q8) >> 8);
        a = a > 32767 ? 32767 : a < -32768 ? -32768 : a;
        c = c > 32767 ? 32767 : c < -32768 ? -32768 : c;
        s = (uint16_t)a; fwrite(&s, 2, 1, f);
        s = (uint16_t)c; fwrite(&s, 2, 1, f);
    }
    fclose(f);
}
static void demo(const char *dir)
{
    /* the sounds: factory indices of tools/duell_sounds.py (engine, idx) */
    static const struct { const char *tag, *label, *eng; uint32_t k; int kind; int send; } SRC[] = {
        {"pad_wave", "PAD: WAVE #1 VOWEL PAD (a held dyad and fifth)", "WAVE", 1, 0, 110},
        {"pad_noise", "PAD: NOISE SEA ORGAN (a held dyad and fifth)", "NOISE", 2, 0, 110},
        {"pluck_phys", "PLUCK: PHYS GHOST HARP (a phrase)", "PHYS", 2, 1, 100},
        {"pluck_analog", "PLUCK: ANALOG PIZZICATO (a phrase)", "ANALOG", 12, 1, 100},
        {"chord_analog", "CHORD: ANALOG SILK CHORUS (two chords)", "ANALOG", 9, 2, 100},
        {"chord_wave", "CHORD: WAVE PWM PAD (two chords)", "WAVE", 0, 2, 100}};
    static const struct { const char *tag; int pitch, mod; } VAR[] = {
        {"up_mod0", SHP_UP, 0}, {"up_mod40", SHP_UP, 40}, {"up_mod100", SHP_UP, 100},
        {"down_mod0", SHP_DOWN, 0}, {"down_mod40", SHP_DOWN, 40}, {"down_mod100", SHP_DOWN, 100}};
    static const char *const PN[2] = {"UP", "DOWN"};
    uint32_t s, v;
    char path[400], idx[400];
    FILE *h;
    snprintf(idx, sizeof idx, "%s/index.html", dir);
    h = fopen(idx, "w");
    if (!h) return;
    fprintf(h, "<!doctype html><meta charset=utf-8><title>DUELL 0.2.19 SHIM: PITCH and MOD</title>"
               "<style>body{font:15px/1.4 system-ui;margin:24px;max-width:1100px}table{border-collapse:collapse}td,th{padding:4px 10px;border-bottom:1px solid #ccc;text-align:left}"
               "audio{width:260px;height:30px}h2{margin-top:28px}</style>"
               "<h1>DUELL 0.2.19: SHIM reverb, PITCH and MOD</h1>"
               "<p>Each row is one sound through the SHIM reverb (TYPE SHIM, SIZE 100, DAMP 60, LOCUT 75 = the defaults of a new multi). Columns: MOD 0 (off, the old sound), 40 (medium), 100 (high). "
               "Rows: PITCH UP (the octave up, as ever), DOWN (an octave down). About 9 s each: the sound, then its tail.</p>");
    for (s = 0; s < sizeof SRC / sizeof SRC[0]; s++) {
        fprintf(h, "<h2>%s</h2><table><tr><th>PITCH</th><th>MOD 0</th><th>MOD 40</th><th>MOD 100</th></tr>", SRC[s].label);
        for (v = 0; v < 6u; v++) {
            int32_t pk, gain;
            mix_scene(DM_STEREO, scn_eng(SRC[s].eng), SRC[s].k, SRC[s].kind, VAR[v].pitch, VAR[v].mod, 9, SRC[s].send);
            pk = peak_of(rec_l, rec_n) > peak_of(rec_r, rec_n) ? peak_of(rec_l, rec_n) : peak_of(rec_r, rec_n);
            gain = pk > 0 ? (int32_t)(256.0 * 24000.0 / (pk > 24000 ? pk : 24000)) : 256;   /* only down to 24000, never up; the same gain would differ per file: keep 1.0 */
            (void)gain;
            snprintf(path, sizeof path, "%s/%s_%s.wav", dir, SRC[s].tag, VAR[v].tag);
            wav(path, rec_l, rec_r, rec_n, 256);
            if (v % 3u == 0u) fprintf(h, "<tr><td>%s</td>", PN[VAR[v].pitch]);
            fprintf(h, "<td><audio controls preload=none src=\"%s_%s.wav\"></audio></td>", SRC[s].tag, VAR[v].tag);
            if (v % 3u == 2u) fprintf(h, "</tr>");
            printf("shimmer: demo %s (peak %d)\n", path, (int)pk);
        }
        fprintf(h, "</table>");
    }
    fprintf(h, "</body>");
    fclose(h);
}
#endif

int main(int argc, char **argv)
{
#ifdef SHIMMER_BASE
    {
        uint32_t ha, hb;
        uint64_t m[3];
        static const uint32_t MODES[3] = {DM_STEREO, DM_LAYER, DM_SPLIT};
        for (uint32_t i = 0; i < 3u; i++) {
            mix_scene(MODES[i], scn_eng("WAVE"), 1, 0, 0, 0, 5, 100);
            m[i] = hash64(rec_l, rec_r, rec_n);
            mix_scene(MODES[i], scn_eng("PHYS"), 2, 1, 0, 0, 5, 100);
            m[i] ^= hash64(rec_l, rec_r, rec_n) * 31u;
        }
        (void)ha; (void)hb; (void)argc; (void)argv;
        printf("shimmer: BASE mix hashes STEREO 0x%016llxull LAYER 0x%016llxull DUEL 0x%016llxull\n", (unsigned long long)m[0], (unsigned long long)m[1], (unsigned long long)m[2]);
        return 0;
    }
#else
    uint32_t ha, hb, i, k;
    /* ---- DEFAULT: bit for bit */
    bus_hashes(&ha, &hb);
    printf("shimmer: bus hashes A %08x B %08x\n", ha, hb);
    expect("DEFAULT: PITCH UP, MOD 0 on the bus alone (A and B, SIZE / DAMP moving, bursts): the SHIM of 0.2.18, bit for bit", ha == H_BUS_A && hb == H_BUS_B);
    {
        static const uint32_t MODES[3] = {DM_STEREO, DM_LAYER, DM_SPLIT};
        uint64_t m[3], ref[3] = {H_MIX_STEREO, H_MIX_LAYER, H_MIX_DUEL};
        int ok = 1;
        for (i = 0; i < 3u; i++) {
            mix_scene(MODES[i], scn_eng("WAVE"), 1, 0, 0, 0, 5, 100);
            m[i] = hash64(rec_l, rec_r, rec_n);
            mix_scene(MODES[i], scn_eng("PHYS"), 2, 1, 0, 0, 5, 100);
            m[i] ^= hash64(rec_l, rec_r, rec_n) * 31u;
            ok &= m[i] == ref[i];
        }
        printf("shimmer: mix hashes STEREO 0x%016llxull LAYER 0x%016llxull DUEL 0x%016llxull\n", (unsigned long long)m[0], (unsigned long long)m[1], (unsigned long long)m[2]);
        expect("DEFAULT: a pad and a pluck through SHIM in STEREO, LAYER and DUEL: the whole mix is the 0.2.18 render bit for bit", ok);
    }
    /* ---- PITCH */
    {
        static const char *const PN[3] = {"UP", "DOWN", "stored BOTH"};
        double want[3] = {2 * F_IN, F_IN / 2, 2 * F_IN};
        for (k = 0; k < 3u; k++) {
            double m110, m220, m440, m880, f;
            ret_run((int)k, k == 1u ? 0 : 1, FS / 2u, FS, F_IN, 0);   /* (UP and a stored BOTH with MOD 1: the pitch shifter's path) */
            m110 = dft_mag(ret_buf, FS, F_IN / 4), m220 = dft_mag(ret_buf, FS, F_IN / 2), m440 = dft_mag(ret_buf, FS, F_IN), m880 = dft_mag(ret_buf, FS, 2 * F_IN);
            f = zc_freq(ret_buf, 0, FS);
            printf("shimmer: PITCH %s: |110| %.1f |220| %.1f |440| %.1f |880| %.1f, zero crossings %.2f Hz (%+.1f cents of %.0f)\n", PN[k], m110, m220, m440, m880, f,
                   want[k] > 0 ? cents(f, want[k]) : 0.0, want[k]);
            if (k == 2u)                                    /* 0.2.25 (owner): BOTH is gone; a stored 2 plays UP */
                expect("PITCH: a stored BOTH (2, removed in 0.2.25) plays UP: the sine comes back at 861.3 Hz (within 3 cents), no 220 Hz",
                       fabs(cents(f, 2 * F_IN)) < 3.0 && m880 > 8 * m440 && m880 > 8 * m220);
            else if (k == 0)
                expect("PITCH UP: a sine of 430.66 Hz comes back at 861.3 Hz (within 3 cents), no 220 Hz and no 440 Hz to speak of", fabs(cents(f, 2 * F_IN)) < 3.0 && m880 > 8 * m440 && m880 > 8 * m220);
            else if (k == 1)
                expect("PITCH DOWN: a sine of 430.66 Hz comes back at 215.3 Hz, an octave below (within 3 cents), nothing at 440 or 880 Hz to speak of",
                       fabs(cents(f, F_IN / 2)) < 3.0 && m220 > 8 * m440 && m220 > 8 * m880);
        }
        {
            double up, dn, bo;
            ret_run(0, 1, FS / 2u, FS, F_IN, 0); up = dft_mag(ret_buf, FS, 2 * F_IN);
            ret_run(1, 0, FS / 2u, FS, F_IN, 0); dn = dft_mag(ret_buf, FS, F_IN / 2);
            ret_run(2, 1, FS / 2u, FS, F_IN, 0); bo = dft_mag(ret_buf, FS, 2 * F_IN);
            printf("shimmer: the return's level (sine amplitude of the bin): UP %.0f, DOWN %.0f, a stored BOTH %.0f\n", up, dn, bo);
            expect("PITCH: UP and DOWN return at the same level (within 1.5 dB); a stored BOTH exactly as UP", fabs(20 * log10(up / dn)) < 1.5 && bo == up);
        }
    }
    /* ---- MOD */
    {
        double mx[3] = {0, 0, 0}, mn[3] = {0, 0, 0};
        static const int MODV[3] = {0, 50, 100};
        for (k = 0; k < 3u; k++) {
            uint32_t w, nw;
            ret_run(0, MODV[k] ? MODV[k] : 1, FS / 2u, 40u * FS, F_IN, 0);   /* (MOD 0 would hand the block to the plain loop, which has no return to read here: 1 % = 0.25 cents) */
            nw = 40u * FS / (FS / 2u) - 1u;
            mx[k] = -1e9, mn[k] = 1e9;
            for (w = 0; w < nw; w++) {
                double c = cents(zc_freq(ret_buf, w * (FS / 2u), (w + 1u) * (FS / 2u)), 2 * F_IN);
                mx[k] = c > mx[k] ? c : mx[k];
                mn[k] = c < mn[k] ? c : mn[k];
            }
            printf("shimmer: MOD %d: the return's pitch over 40 s (0.5 s windows) from %+.1f to %+.1f cents\n", MODV[k], mn[k], mx[k]);
        }
        expect("MOD ~0: the octave (+-1 cent over 40 s); MOD 100: drifts to +-25 cents at most and at least +-10; MOD 50: about half",
               mx[0] < 1.0 && mn[0] > -1.0 && mx[2] <= 26.0 && mn[2] >= -26.0 && (mx[2] - mn[2]) > 20.0 && (mx[1] - mn[1]) > 8.0 && (mx[1] - mn[1]) < (mx[2] - mn[2]) * 0.7);
        ret_run(1, 100, FS / 2u, 40u * FS, F_IN, 0);
        {
            double a = 1e9, z = -1e9;
            for (i = 0; i < 40u * FS / (FS / 2u) - 1u; i++) {
                double c = cents(zc_freq(ret_buf, i * (FS / 2u), (i + 1u) * (FS / 2u)), F_IN / 2);
                a = c < a ? c : a;
                z = c > z ? c : z;
            }
            printf("shimmer: MOD 100 on PITCH DOWN: %+.1f .. %+.1f cents\n", a, z);
            expect("MOD on PITCH DOWN: the same drift (+-25 cents at most, 20 cents wide at least)", a >= -26.0 && z <= 26.0 && z - a > 20.0);
        }
    }
    {
        /* the output differs and is bounded: loud noise, the corner (SIZE 127, DAMP 0), then 2 s of silence */
        static int32_t r[CTL], w[CTL];
        int32_t pk[4] = {0, 0, 0, 0};
        uint32_t cfg, b, diff = 0;
        static uint32_t hh[4];
        static const int PIT[4] = {0, 0, 1, 2}, MD[4] = {0, 100, 100, 100};
        for (cfg = 0; cfg < 4u; cfg++) {
            uint32_t h = 2166136261u;
            shim_start(PIT[cfg], MD[cfg], 127, 0);
            xs0 = 0x777u;
            for (b = 0; b < 8u * FS / CTL; b++) {
                for (i = 0; i < CTL; i++) {
                    r[i] = b < 5u * FS / CTL ? rnd(b % 400u < 200u ? 120000 : 20000) : 0;
                    w[i] = 0;
                }
                rev_run(3, r, w, CTL);
                for (i = 0; i < CTL; i++) {
                    pk[cfg] = w[i] > pk[cfg] ? w[i] : -w[i] > pk[cfg] ? -w[i] : pk[cfg];
                    h = hfnv(h, w[i]);
                }
            }
            hh[cfg] = h;
        }
        diff = hh[0] != hh[1] && hh[0] != hh[2] && hh[1] != hh[2] && hh[2] != hh[3];
        printf("shimmer: loud noise, SIZE 127, DAMP 0: peaks UP/0 %d, UP/100 %d, DOWN/100 %d, stored BOTH (UP)/100 %d\n", (int)pk[0], (int)pk[1], (int)pk[2], (int)pk[3]);
        expect("MOD changes the output (and each PITCH differs from the others)", diff);
        expect("MOD / DOWN / a stored BOTH at the corner: bounded (under the plain SHIM's peak x 1.5)", pk[1] < pk[0] * 3 / 2 && pk[2] < pk[0] * 3 / 2 && pk[3] < pk[0] * 3 / 2);
    }
    /* ---- CLICK: a held tone through the SHIM bus alone; the changes in jumps. The same events once with PITCH and MOD as listed, once with them at 0 (what a TYPE change does by itself) */
    {
        static int32_t r[CTL], w[CTL], rec[10u * 44100u];
        static const struct { uint32_t sec; int pitch, mod, type; } EV[] = {
            {2, 1, 0, 3}, {3, 2, 0, 3}, {4, 0, 0, 3}, {5, 0, 100, 3}, {6, 0, 0, 3}, {7, 1, 60, 3}, {8, 1, 60, 0}, {9, 1, 60, 3}};
        int32_t st[2][8], steady[2];
        uint32_t pass, e, ok = 1;
        for (pass = 0; pass < 2u; pass++) {
            uint32_t b, n = 0;
            shim_start(0, 0, 90, 60);
            for (b = 0; b < 10u * FS / CTL; b++) {
                for (e = 0; e < sizeof EV / sizeof EV[0]; e++)
                    if (b == EV[e].sec * FS / CTL) {
                        song.g[G_SHPITCH] = (int16_t)(pass ? 0 : EV[e].pitch);
                        song.g[G_SHMOD] = (int16_t)(pass ? 0 : EV[e].mod);
                        song.g[G_RTYPE] = (int16_t)EV[e].type;
                    }
                for (i = 0; i < CTL; i++) {
                    double t = (double)(b * CTL + i) / FS;
                    r[i] = (int32_t)(5000 * sin(2 * M_PI * 220 * t) + 3000 * sin(2 * M_PI * 330 * t) + 2000 * sin(2 * M_PI * 495 * t));
                    w[i] = 0;
                }
                if (song.g[G_RTYPE] != fx.rtype)        /* (a TYPE change as fx_buses does it: the old model's block fades out, the new one starts clear) */
                    rev_switch(r, w, CTL, song.g[G_RTYPE]);
                else
                    rev_run(fx.rtype, r, w, CTL);
                for (i = 0; i < CTL; i++)
                    rec[n++] = w[i];
            }
            steady[pass] = step_of_range(rec, 1u * FS, 2u * FS);
            for (e = 0; e < sizeof EV / sizeof EV[0]; e++)
                st[pass][e] = step_of_range(rec, EV[e].sec * FS - 64u, EV[e].sec * FS + FS / 4u);
        }
        for (e = 0; e < sizeof EV / sizeof EV[0]; e++) {
            int type_ev = EV[e].type != 3 || (e && EV[e - 1u].type != 3);
            printf("shimmer: CLICK at %us (PITCH %d, MOD %d, TYPE %d): the largest step %d (the same event with PITCH UP, MOD 0: %d; steady %d)\n", EV[e].sec, EV[e].pitch, EV[e].mod, EV[e].type,
                   (int)st[0][e], (int)st[1][e], (int)steady[0]);
            ok &= type_ev ? st[0][e] <= st[1][e] * 3 / 2 + 50 : st[0][e] <= steady[0] * 3 / 2 + 8;
        }
        expect("CLICK: PITCH UP -> DOWN -> a stored BOTH (UP) -> UP, MOD 0 <-> 100 -> 0 over a held tone: no step above the steady render's x 1.5; a TYPE change away and back: no more than it is without", ok);
    }
    /* ---- TAILS */
    {
        static int32_t r[CTL], w[CTL];
        uint32_t cfg, size, ok = 1;
        for (size = 0; size < 3u; size++)
            for (cfg = 0; cfg < 9u; cfg++) {
                uint32_t b, last_nz = 0, nb = 90u * FS / CTL;
                shim_start((int)(cfg / 3u), (int)(cfg % 3u) * 50, size == 2u ? 150 : size ? 127 : 90, 60);
                xs0 = 0x999u;
                for (b = 0; b < nb; b++) {
                    for (i = 0; i < CTL; i++) {
                        r[i] = b < 3u * FS / CTL ? rnd(100000) : 0;
                        w[i] = 0;
                    }
                    rev_run(3, r, w, CTL);
                    for (i = 0; i < CTL; i++)
                        if (w[i]) last_nz = b * CTL + i;
                }
                printf("shimmer: TAILS SIZE %d PITCH %u MOD %u: the last non-zero sample at %.1f s\n", size == 2u ? 150 : size ? 127 : 90, cfg / 3u, (cfg % 3u) * 50, (double)last_nz / FS);
                ok &= last_nz < 40u * FS;
            }
        expect("TAILS: after loud noise, every PITCH x MOD (0, 50, 100), SIZE 90, 127 and 150 (SPACE's largest): exact silence within 40 s (plain UP: 8.5 s at SIZE 127, 18 s at 150)", ok);
    }
    /* ---- BUSES */
    {
        static int32_t r[CTL], wa[CTL], wb[CTL];
        uint32_t cfg, ok = 1;
        for (cfg = 0; cfg < 3u; cfg++) {
            uint32_t b;
            shim_start((int)cfg, cfg ? 70 : 0, 90, 60);
            xs0 = 0x4242u;
            for (b = 0; b < 4u * FS / CTL; b++) {
                for (i = 0; i < CTL; i++) {
                    r[i] = (b / 200u) % 2u ? 0 : rnd(50000);
                    wa[i] = wb[i] = 0;
                }
                rev_run(3, r, wa, CTL);
                rev_run_b(3, r, wb, CTL);
                for (i = 0; i < CTL; i++)
                    ok &= wa[i] == wb[i];
            }
        }
        expect("BUSES: bus A and bus B fed alike give the same return (PITCH UP / DOWN / a stored BOTH, MOD 70)", ok);
        {
            int32_t pl, pr;
            mix_scene(DM_SPLIT, scn_eng("ANALOG"), 9, 0, 1, 100, 4, 127);
            pl = peak_of(rec_l, rec_n), pr = peak_of(rec_r, rec_n);
            printf("shimmer: DUEL part 1 alone, PITCH DOWN MOD 100: peaks left %d right %d\n", (int)pl, (int)pr);
            expect("BUSES: DUEL, part 1 alone through SHIM DOWN + MOD: the right channel is exactly 0, the left sounds", pr == 0 && pl > 1000);
        }
    }
    /* ---- CLEAR */
    {
        static int32_t r[CTL], w[CTL];
        uint32_t b, ok = 1;
        shim_start(1, 80, 90, 60);
        xs0 = 0x5151u;
        for (b = 0; b < FS / CTL; b++) {
            for (i = 0; i < CTL; i++) { r[i] = rnd(40000); w[i] = 0; }
            rev_run(3, r, w, CTL);
            rev_run_b(3, r, w, CTL);
        }
        ok &= fx.sh_x == 1 && fx.sh_pd != 0 && fx.sh_l1 != 0 && fx_b.sh_x == 1;
        rev_clear();
        rev_clear_b();
        ok &= !fx.sh_x && !fx.sh_pu && !fx.sh_pd && !fx.sh_l1 && !fx.sh_l2 && !fx.sh_g && !fx.sh_cfg && !fx_b.sh_x && !fx_b.sh_pd && !fx_b.sh_g;
        for (i = 0; i < SH_LEN; i++)
            ok &= shim.buf[i] == 0 && shim_b.buf[i] == 0;
        for (i = 0; i < CTL; i++)
            ok &= shim.fb[i] == 0 && shim_b.fb[i] == 0;
        expect("CLEAR: rev_clear (a model change, SENDS OFF, STOP ALL) leaves the PITCH / MOD state, the line and the return at zero on both buses", ok);
    }
    {
        uint32_t kind;
        for (kind = 0; kind < 2u; kind++) {
            uint32_t b, seen = 0, nz = 0, pre;
            int32_t pk_after = 0;
            mix_scene(DM_STEREO, scn_eng("ANALOG"), 12, 1, 1, 80, 1, 127);
            trk_note_on(&trk[0], 52, 100);
            trk_note_on(&trk[0], 59, 100);
            for (b = 0; b < 2u * FS / CTL; b++) {
                if (b == FS / 8u / CTL) { trk_note_off(&trk[0], 52); trk_note_off(&trk[0], 59); }
                mix_block(ob, CTL);
            }
            pre = fx.sh_x == 1 && fx.sh_pd != 0 && fx.sh_l1 != 0;
            if (kind) snd_panic = 1; else mods_off |= MODS_SENDS;
            for (b = 0; b < FS / CTL; b++) {
                mix_block(ob, CTL);
                if (snd_st == SND_OFF && snd_clr > DLY_LEN / DM_CHUNK && !seen) {
                    nz = fx.sh_x | fx.sh_pu | fx.sh_pd | fx.sh_l1 | fx.sh_g | fx.sh_hp;
                    for (i = 0; i < SH_LEN; i++) nz |= (uint32_t)(shim.buf[i] != 0);
                    seen = 1 + (nz == 0);
                }
            }
            mods_off &= ~MODS_SENDS;
            for (b = 0; b < 3u * FS / CTL; b++) {
                mix_block(ob, CTL);
                if (b > FS / CTL) {
                    int32_t a = ob[0] < 0 ? -ob[0] : ob[0], c = ob[1] < 0 ? -ob[1] : ob[1];
                    pk_after = a > pk_after ? a : pk_after;
                    pk_after = c > pk_after ? c : pk_after;
                }
            }
            printf("shimmer: flush kind %u: seen %u nz %u pk_after %d\n", kind, seen, nz, (int)pk_after);
            expect(kind ? "PANIC: STOP ALL's flush clears SHIM's PITCH / MOD state and line (DOWN, MOD 80 sounding); the unit is silent after it until a new note" :
                   "SENDS OFF: the same", pre && seen == 2 && pk_after == 0);
        }
    }
    /* ---- the plain loop takes over and gives back */
    {
        static int32_t r[CTL], w[CTL];
        uint32_t b;
        int ok = 1;
        shim_start(0, 0, 90, 60);
        for (b = 0; b < 3u * FS / CTL; b++) {
            if (b == FS / CTL) song.g[G_SHMOD] = 30;
            if (b == 2u * FS / CTL) song.g[G_SHMOD] = 0;
            if (b == FS / CTL / 2u || b == 5u * FS / (2u * CTL)) ok &= fx.sh_x == 0;
            for (i = 0; i < CTL; i++) { r[i] = (b < FS / CTL) ? rnd(30000) : 0; w[i] = 0; }
            rev_run(3, r, w, CTL);
            if (b == FS / CTL + 3u) ok &= fx.sh_x == 1;
        }
        ok &= fx.sh_x == 0;
        expect("TAKE-OVER: UP + MOD 0 runs the plain loop (the state at rest); MOD on runs rev_shimx from the plain loop's phase; MOD back to 0 hands the phase back", ok);
    }
    /* ---- UI */
    {
        uint32_t pr = 0, ps = 0, ok;
        const param_desc_t *d;
        int16_t *vp;
        for (i = 0; i < NPAGES; i++) {
            if (str_eq(PAGES[i].title, "REVERB")) pr = i;
            if (str_eq(PAGES[i].title, "SHIMMER")) ps = i;
        }
        expect("UI: SHIMMER is the page after REVERB (GLO family, GLOBAL; 0.2.21: the global effects are GLO's pages), K1 PITCH, K2 MOD, K3 and K4 empty; REVERB is still TYPE SIZE DAMP LOCUT",
               ps == pr + 1u && PAGES[ps].fam == FAM_GLO && PAGES[ps].scope == SC_GLOBAL && PAGES[ps].id[0] == G_SHPITCH && PAGES[ps].id[1] == G_SHMOD && PAGES[ps].id[2] == 0xFF &&
               PAGES[ps].id[3] == 0xFF && PAGES[pr].id[0] == G_RTYPE && PAGES[pr].id[1] == G_RSIZE && PAGES[pr].id[2] == G_RDAMP && PAGES[pr].id[3] == G_RLOCUT);
        d = page_desc(&PAGES[ps], 0, &vp);
        ok = d && !strcmp(d->label, "PITCH") && d->max == 1 && d->def == 0 && vp == &song.g[G_SHPITCH] && !strcmp(d->names[0], "UP") && !strcmp(d->names[1], "DOWN");
        d = page_desc(&PAGES[ps], 1, &vp);
        ok = ok && d && !strcmp(d->label, "MOD") && d->min == 0 && d->max == 100 && d->def == 0 && vp == &song.g[G_SHMOD];
        expect("UI: PITCH is UP / DOWN (default UP; 0.2.25: BOTH removed), MOD 0..100 % (default 0): a new unit sounds as 0.2.18", ok);
        expect("UI: both are SENDS cards (MODULES SENDS OFF hides them) and travel with a multi; G_COUNT did not move", mods_of(1, G_SHPITCH) == MODS_SENDS && mods_of(1, G_SHMOD) == MODS_SENDS &&
               MU_GLOBAL(G_SHPITCH) && MU_GLOBAL(G_SHMOD) && G_COUNT == 27);
        for (k = 0; k < 6u; k++) {
            uint32_t nxt, prv;
            song.g[G_RTYPE] = (int16_t)k;
            ui.page = (uint8_t)pr; page_entered();
            nxt = page_next(1);
            prv = page_next(-1);
            ui.page = (uint8_t)(k == 3u ? ps : pr);
            if (k == 3u) {
                expect("UI: TYPE SHIM: REVERB's next page is SHIMMER, and SHIMMER's next is the master page that follows it in GLO (0.2.25: STEREO MASTER)", nxt == ps && page_next(1) == ps + 1u && str_eq(PAGES[ps + 1u].title, MASTER_PAGE_TITLE) && page_visible(ps));
            } else {
                ok = nxt != ps && !page_visible(ps) && prv != ps;
                expect(k == 0u ? "UI: TYPE ROOM: the page cycle skips SHIMMER (REVERB's next is the master page, the previous of it is REVERB)" :
                       "UI: the same for SPRING, HALL, RESO, PLATE: SHIMMER is not in the page cycle", ok && nxt == ps + 1u);
            }
        }
        /* on SHIMMER when the TYPE leaves SHIM (the editor, a multi, MIDI): the screen goes to REVERB */
        song.g[G_RTYPE] = 3;
        ui.page = (uint8_t)ps; page_entered();
        ui.force = 1; ui_draw();
        ok = ui.page == ps && ui.col[0][0] != 0x7f && ui.col[1][0] != 0x7f;
        expect("UI: SHIMMER on screen: PITCH and MOD are drawn", ok);
        song.g[G_RTYPE] = 0;
        ui.force = 1; ui_draw();
        expect("UI: the TYPE leaves SHIM while SHIMMER is open: the screen goes to REVERB (not to the EDIT pages)", ui.page == pr);
        mods_off |= MODS_SENDS;
        song.g[G_RTYPE] = 3;
        ui.page = (uint8_t)ps; page_entered();
        ui.force = 1; ui_draw();
        ok = ui.col[0][0] == 0x7f && ui.col[1][0] == 0x7f;
        mods_off &= ~MODS_SENDS;
        expect("UI: SENDS OFF: the SHIMMER cards are not drawn (as REVERB's)", ok);
        song.g[G_RTYPE] = 2;
        ui.page = (uint8_t)pr; page_entered();
    }
    /* ---- COST: the bus alone and the mix */
    {
        static int32_t r[CTL], w[CTL];
        double c[5], m[3];
        static const int PIT[5] = {0, 0, 1, 2, 0}, MD[5] = {0, 50, 0, 0, 100};
        uint32_t cfg, b;
        for (cfg = 0; cfg < 5u; cfg++) {
            uint64_t i0;
            uint32_t nb = 4u * FS / CTL;
            shim_start(PIT[cfg], MD[cfg], 90, 60);
            for (i = 0; i < CTL; i++) r[i] = rnd(30000);
            i0 = instr_now();
            for (b = 0; b < nb; b++) {
                for (i = 0; i < CTL; i++) w[i] = 0;
                rev_run(3, r, w, CTL);
            }
            c[cfg] = i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
        }
        shim_start(0, 0, 90, 60);
        {
            uint64_t i0;
            for (i = 0; i < CTL; i++) r[i] = rnd(30000);
            i0 = instr_now();
            for (b = 0; b < 4u * FS / CTL; b++) { for (i = 0; i < CTL; i++) w[i] = 0; rev_run(0, r, w, CTL); }
            m[0] = i0 ? (double)(instr_now() - i0) / (4u * FS) : 0;
        }
        printf("shimmer: COST host instructions / sample, the reverb alone: ROOM %.1f; SHIM plain %.1f, MOD 50 %.1f, DOWN %.1f, BOTH %.1f, MOD 100 %.1f\n", m[0], c[0], c[1], c[2], c[3], c[4]);
        expect("COST: PITCH DOWN / BOTH and MOD stay under ROOM x 2.5 (the device estimate 1.7 % per 100)", c[0] == 0.0 || (c[2] < m[0] * 2.5 && c[3] < m[0] * 3.0 && c[1] < m[0] * 3.0));
    }
    if (argc > 1)
        demo(argv[1]);
    printf("shimmer: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
#endif
}
