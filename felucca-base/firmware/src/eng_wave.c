/* SPDX-License-Identifier: GPL-3.0-only */
/* WAVE (DUELL): a wavetable oscillator. Our own code, written for this firmware (no code from other projects).
 *
 * TABLE picks a table, POS scans it. A table is not stored finished: it is a RECIPE (a few lines below that give each harmonic's
 * level for a place in the scan), built when the table is selected into the part's engine memory (engines.c eng_mem): WV_F
 * frames, each a single cycle in WV_NL band-limited versions ("levels"), 8-bit samples:
 *   level l: N = 256 128 128 64 64 samples holding the harmonics 1 .. H = 64 >> l (64 32 16 8 4): four samples a harmonic
 *   at the least, and one more (the guard: sample 0 again, no wrap test). What limits the aliasing is the linear read's
 *   images (the fundamental's at N - 1 harmonics is ~1 / N^2 down), not the 8 bits (within 0.5 dB of exact samples): the
 *   upper levels keep 128 and 64 samples instead of 64 32 16, 7..24 dB less there, for six frames of memory.
 * A voice reads the level whose top harmonic stays below WV_FOLD Hz (~25 kHz: what goes past Nyquist folds back above
 * ~19 kHz) at its pitch, and over the top half octave of a level it cross-fades into the next one (no step in brightness
 * as a note bends or glides across). Above the fifth level (4 harmonics, to 6.3 kHz: SYNC and BEND read that fast on
 * ordinary notes) a sixth: the frame's fundamental alone, a sine (its amplitude kept with the frame, one byte). POS moves between frames; adjacent frames are mixed (every harmonic of a recipe has
 * the same phase in every frame, sine phase, so a mix of two frames is the mix of their spectra: no level dips).
 *
 * The build: one frame at a time, in the MAIN LOOP (wave_service, main.c; never the audio interrupt: a table is ~1 M
 * host instructions, ~8 ms of the device's CPU, which there would be a lump of load or a slow build). Per frame: the recipe's 64 levels, then each level's
 * samples as sums of sines (a quarter of a cycle computed: sine-phase series are odd and quarter-symmetric by their odd and
 * even harmonics), then one gain for the frame (its RMS to WV_RMS, its peak at most 127) and the 8-bit samples. The frame is
 * made in wvj (main-loop scratch, not engine memory) and copied in with the interrupts off (646 bytes, ~0.6 k instructions),
 * after checking the region is still WAVE's and the table still wanted (wv_gen: a TABLE change, an engine switch or a
 * part's memory going to another engine stops the job). The audio side (wave_block, once a block) owns 'want' and 'have':
 * a TABLE change fades the part's voices out over WV_FADE blocks on the old table, then the table is gone and its voices
 * are silent until the new one is built, then they fade in (WV_FADE blocks). Deterministic: integer recipes, the same bytes
 * on every build; the host renders (tests/hostsim.c host_preset / host_ds_load) build at once (wave_build_now).
 *
 * The voice: POS (and the SHP routes: AMP DEST / ENV 2 / LFO 1 / LFO 2 SHP, the matrix's SHP, as an offset on POS, 63 steps
 * the whole scan) ramped over the block; BEND a warp of the read phase (the cycle's first half squeezed, read up to 10 x as
 * fast, the second half stretched: a pulse width for any wave); SYNC hard sync, the read restarting at each cycle of the
 * note at 1 .. 8 x its speed (three octaves), each restart's step smoothed (a two-sample polyBLEP); both choose the level for
 * the fastest part (the band-limit holds there) and a DC tracker takes the mean they make off;
 * DTN a second read DTN cents up (with its own SYNC), mixed half and half. No filter of its own: the multimode filter
 * (voice.c uf_run; ufilt). Our own code: the approach (band-limited levels of a wavetable, a polyBLEP at sync resets) is the
 * textbook one.
 *
 * USER TABLES (DUELL 0.2.14): after the 26 factory tables, TABLE 26..28 are three user slots USR1..USR3 (wtab.c stores them in flash, the
 * editor writes them: EDITOR_PROTOCOL "User wavetables"). A slot holds a table as the recipe's own source data: for each of the WV_F frames the
 * harmonics 1 .. 64, ONE BYTE each (wt_obj_t), a log amplitude relative to the frame's strongest: 0 off, b >= 1 -> 2^(-(255 - b) / 16)
 * (wt_q16, a 16 entry table and a shift). The build is the factory one: wv_frame takes the harmonics from wv_user instead of wv_recipe
 * and goes on (the same scaling to the frame's peak, the same sums, the same WV_RMS gain), so everything downstream (levels, fades, voices) is
 * as for a factory table and the factory tables' bytes do not change. An EMPTY slot plays a sine in every frame and is named EMPTY. The slot's
 * name (up to 8 characters) is the TABLE's shown name; a card shows five. A new table in a slot that is playing fades out and builds again (wv_uver).
 * Phases are not kept (all sine), 19 frames and 64 harmonics: that is what a user table is. */
static char wv_uname[3][9] = {"USR1", "USR2", "USR3"};   /* the user slots' shown names (wt_names) */
/* the tables (append-only: a stored sound keeps its number). Names: what the scan goes through, <= 5 letters (a card shows 5) */
static const char *const N_WV_TABLE[] = {"SAWSQ", "PWM", "HARM", "ODD", "ORGAN", "VOWEL", "BRASS", "BOWED", "RESO", "COMB", "DIGI",
                                         "GLASS", "CHIME", "ROUGH",
                                         /* 0.2.7: classic digital tables (in the spirit of the 1980s wavetable synths), then modern ones */
                                         "BAND", "DUAL", "THIN", "PIPE", "REESE", "HOOVR", "GROWL", "WOBBL", "YELL", "METAL", "STAIR", "AIR",
                                         /* 0.2.14: the user slots: the shown name is the user's (wt_names: up to 8 characters, EMPTY when unused) */
                                         wv_uname[0], wv_uname[1], wv_uname[2]};
#define WV_NT NELEM(N_WV_TABLE)
#define WV_NTF 26u               /* the factory tables; TABLE WV_NTF .. WV_NT - 1 are the user slots */
#define WV_NU 3u                 /* user slots */
#define WV_F 19u                 /* frames a table */
#define WV_NL 5u                 /* levels a frame */
#define WV_H 64u                 /* harmonics of level 0 */
#define WV_FS 646u               /* bytes a frame: its levels (N + 1 each), then its fundamental's amplitude / 2 (WV_FUND) */
#define WV_FUND 645u             /* (the sixth level, above the fifth's range: the fundamental alone, a sine: no table needed) */
#define WV_NPT 165u              /* points a frame's sums take: N / 4 + 1 a level (65 33 33 17 17) */
#define WV_FADE 4u               /* blocks of the fade out (a TABLE change) and in (the table built): 2.9 ms */
#define WV_GSTEP 8192            /* .. its step a block, Q15 */
#define WV_RMS 6350              /* a frame's RMS, 8-bit units x 100 (63.5: half of full scale; a peak above 127 lowers it) */
#define WV_FPMAX (((int32_t)(WV_F - 1u) << 16) - 1)   /* the scan's last place, Q16 frames (the read takes frame f and f + 1) */
#define WV_FPQ ((int32_t)(((WV_F - 1u) << 26) / (127u << 8)))   /* POS (127 << 8) to Q16 frames, Q10 */
#define WV_SAFE 38196000u        /* level 0's top increment: harmonic 64 at WV_FOLD = 25.1 kHz (392 Hz); level l: << l */
static const uint16_t WV_LO[WV_NL] = {0, 257, 386, 515, 580};  /* a level's place in a frame */
static const uint16_t WV_N[WV_NL] = {256, 128, 128, 64, 64};    /* its samples */
static const uint8_t WV_SH[WV_NL] = {24, 25, 25, 26, 26};       /* the read phase's shift to its sample index */

typedef struct {
    uint8_t want, have;          /* the table asked for and the table in memory, + 1 (0: none); the audio side writes want */
    uint8_t fade, rsv;           /* blocks of the fade-out still to run (the old table still in memory) */
} wv_hdr_t;
typedef struct {
    wv_hdr_t h;                  /* (engine_t.mem: only this is cleared on a claim; the frames are read only once built) */
    int8_t tab[WV_F * WV_FS];
} wv_mem_t;
ENG_MEM_FITS("WAVE", sizeof(wv_mem_t));

static uint32_t wv_gen[NLIVE];   /* a part's build number: the audio side counts it up for every table it then wants */


/* ------------------------------------------------------------------------------------------------- user tables --- */
/* A stored user table (also the flash object of wtab.c and the backup object of ids 24..26, 1228 bytes): amp[f * WV_H + k - 1] is
 * harmonic k of frame f (a log amplitude, below), name the user's name (0 padded, ASCII 32..126) */
#define WT_VER 1u
typedef struct {
    uint8_t ver, nf, nh, flags;  /* WT_VER, WV_F, WV_H, 0 */
    char name[8];
    uint8_t amp[WV_F * WV_H];
} wt_obj_t;
_Static_assert(sizeof(wt_obj_t) == 1228u, "user wavetable object layout");
static wt_obj_t wt_ram[WV_NU] __attribute__((section(".pool")));   /* the slots as loaded (main loop only) */
static uint8_t wt_used;          /* bit u: slot u holds a table */
static uint8_t wv_uver[WV_NU];   /* a slot's version: counted up (main loop) whenever its table is stored or erased */
static uint8_t wv_bver[NLIVE];   /* the version of the user table a part's build began with (main loop; audio compares) */
static const uint32_t WV_AMP[16] = {65536, 62757, 60097, 57549, 55109, 52773, 50535, 48393, 46341, 44376, 42495, 40693, 38968, 37316,
                                    35734, 34219};   /* round(65536 * 2^(-r / 16)): the Q16 of a byte's amplitude, below */
/* a byte -> the harmonic's level (Q16, 65536 = the frame's strongest): 0 off; b: 2^(-(255 - b) / 16). 16 steps an octave (0.38 dB), 255 -> -95 dB */
static int32_t wt_q16(uint32_t b) { return b ? (int32_t)(WV_AMP[(255u - b) & 15u] >> ((255u - b) >> 4)) : 0; }
/* the object's shape, as a stored one is accepted (0: not): size, version, frames, harmonics, flags, a printable name, some harmonic on */
static int wt_obj_ok(const uint8_t *b, uint32_t len)
{
    uint32_t i, on = 0;
    if (len != sizeof(wt_obj_t) || b[0] != WT_VER || b[1] != WV_F || b[2] != WV_H || b[3])
        return 0;
    for (i = 0; i < 8u; i++)
        if (b[4u + i] && (b[4u + i] < 32u || b[4u + i] > 126u))
            return 0;
    for (i = 12u; i < len; i++)
        on |= b[i];
    return on != 0u;
}
/* the shown names: the user's, USRn when it has none, EMPTY for a slot with no table (TABLE still plays: a sine) */
static void wt_names(void)
{
    uint32_t u, i;
    for (u = 0; u < WV_NU; u++) {
        const char *src = wt_ram[u].name;
        for (i = 0; i < 9u; i++)
            wv_uname[u][i] = 0;
        if (!(wt_used >> u & 1u)) {
            memcpy(wv_uname[u], "EMPTY", 5);
        } else if (!src[0]) {
            memcpy(wv_uname[u], "USR", 3);
            wv_uname[u][3] = (char)('1' + u);
        } else {
            for (i = 0; i < 8u; i++)
                wv_uname[u][i] = src[i];
        }
    }
}
/* ------------------------------------------------------------------------------------------------ the recipes --- */
/* Each fills a[1..64] with the harmonics' levels (Q16, 65536 = 1, a sign is a phase flip) for the place x in the scan (Q16:
 * 0 the first frame, 65536 the last). Integer only (the device has no FPU) */
static int32_t wv_sin(uint32_t turn16) { return sine_i(turn16 << 16); }   /* sin(2 pi turn), turn Q16, Q15 */
static int32_t wv_exp2(int32_t x16)                                         /* 2^x, x Q16 (0 .. 15), Q16 */
{
    uint32_t i = (uint32_t)x16 >> 16, f = (uint32_t)x16 & 0xFFFFu;
    uint32_t m = 65536u + ((f * (43024u + ((f * 22512u) >> 16))) >> 16);   /* 1 + f (0.6565 + 0.3435 f): within 0.3 % */
    return (int32_t)(m << i);
}
/* the share (Q16) of harmonic number j (1..) that a count c (Q16) lets in: 0 below, rising over one step, 1 above */
static int32_t wv_step(int32_t c16, uint32_t j)
{
    int32_t d = c16 - (int32_t)(j - 1u) * 65536;
    return d < 0 ? 0 : d > 65536 ? 65536 : d;
}

/* ------------------------------------------------------------------------------------------------- the build --- */
static struct {
    uint8_t on, part, table, frame;   /* the job: building table 'table' of part 'part', frame 'frame' next */
    uint32_t gen;                     /* .. for that part's build number */
    int32_t a[WV_H + 1u];             /* the frame's harmonic levels, Q10 (the largest 1023) */
    int32_t oe[2u * WV_NPT];          /* each level's odd and even sums at its first quarter cycle */
    int8_t out[WV_FS];                /* the frame */
} wvj;

/* the slot changed (stored, erased, restored; main loop): a build of a user table is dropped (wave_service starts it again if a part
 * still wants it, with the new table), a part that has it built fades out and builds again (wave_block compares wv_uver) */
static void wt_changed(uint32_t u)
{
    wv_uver[u]++;
    wt_names();
    if (wvj.on && wvj.table >= WV_NTF)
        wvj.on = 0;
}

static uint64_t wv_isqrt(uint64_t v)
{
    uint64_t r = 0, b = (uint64_t)1 << 62;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}
/* the levels of the drawbars 0..8 (Q16): -3 dB a step, 8 = 1 */
static const uint16_t WV_DB[9] = {0, 5843, 8250, 11654, 16462, 23253, 32846, 46395, 65535};
/* ORGAN: the drawbars 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1' (the harmonics 1 2 3 4 5 6 8; the 16' is the SUB LAYER's, SINE OCTAVE -1, chain_sub.c),
 * five registrations along the scan */
static const uint8_t WV_ORG_K[7] = {1, 2, 3, 4, 5, 6, 8};
static const uint8_t WV_ORG[5][7] = {{8, 0, 0, 0, 0, 0, 0}, {8, 8, 0, 0, 0, 0, 0}, {8, 8, 8, 0, 0, 0, 0}, {8, 8, 8, 8, 0, 0, 0},
                                     {8, 8, 8, 8, 8, 8, 8}};
/* VOWEL: the first three formants of an adult male's A E I O U (Hz; Peterson and Barney's averages), their widths and levels;
 * placed for a C3 (130.8 Hz: harmonic F / 130.8, x 501 in Q16), as any table they move with the note */
static const uint16_t WV_VOW[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
static const uint16_t WV_VOW_BW[3] = {90, 110, 170};
static const uint16_t WV_VOW_A[3] = {65535, 39000, 23000};
/* CHIME: a free bar's (a chime tube's) modes 1 : 2.76 : 5.40 : 8.93 : 13.3 : 18.6 : 24.8 : 31.9 : 39.8 : 48.6, rounded to
 * harmonics (a single cycle cannot hold the inharmonic ratios themselves) */
static const uint8_t WV_CHIME[10] = {1, 3, 5, 9, 13, 19, 25, 32, 40, 49};
/* PIPE: a pipe organ's ranks, the harmonic each sounds (8' 4' 2 2/3' 2', then the mixture's 1 1/3' 1' 2/3' 1/2' 1/3' 1/4') */
static const uint8_t WV_PIPE[10] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32};
/* METAL: a struck circular membrane's (a plate's) first modes, 1 : 1.594 : 2.136 : 2.296 : 2.653 : 2.918 : 3.156 : 3.501 (Q16) */
static const uint32_t WV_MEMB[8] = {65536, 104464, 139985, 150471, 173867, 191234, 206832, 229442};
/* GROWL: a man's U, short U, AW, AH, AE (F1 F2 F3, Hz; Peterson and Barney's averages: the opening "oo-wah" of a growl), placed
 * for C2; YELL: AH, AE, EH, short I, EE (F1 F2) and a strong fixed peak at 3 kHz (the singer's formant: the ring), placed for C3 */
static const uint16_t WV_GRL[5][3] = {{300, 870, 2240}, {440, 1020, 2240}, {570, 840, 2410}, {730, 1090, 2440}, {660, 1720, 2410}};
static const uint16_t WV_YEL[5][3] = {{730, 1090, 3000}, {660, 1720, 3000}, {530, 1840, 3000}, {390, 1990, 3000}, {270, 2290, 3000}};
static const uint16_t WV_FBW[2][3] = {{60, 80, 120}, {90, 110, 300}};     /* their widths (Hz) */
static const uint8_t WV_FG[2][4] = {{4, 64, 48, 24}, {3, 32, 64, 96}};   /* the saw under them, then each peak's gain (Q4) */
/* STEPS: a frame's steps a cycle */
static const uint8_t WV_STEPS[WV_F] = {64, 48, 40, 32, 26, 22, 19, 16, 14, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3};
/* a resonance's level (Q16) d (Q16 harmonics) from its centre, its width w (at least one harmonic): w^2 / (w^2 + d^2) */
static int32_t wv_res(int64_t d16, int64_t w16)
{
    int64_t w2;
    w16 = w16 < 65536 ? 65536 : w16;
    w2 = w16 * w16;
    return (int32_t)((w2 * 65536) / (w2 + d16 * d16));
}

/* FM: sin(c t + I sin(m t)) (c, m whole numbers: an odd wave, all sines) projected onto the harmonics: 128 samples of the wave
 * (I Q16 radians), then b_k = 4 / 256 x sum f(n) sin(2 pi k n / 256) (Q16) */
static void wv_fm(uint32_t c, uint32_t m, int32_t i16, int32_t *a)
{
    int32_t *td = wvj.oe;                                    /* (the sums' space, not yet in use) */
    uint32_t it = (uint32_t)(((int64_t)i16 * 10430) >> 16), n, k;   /* I / 2 pi, Q16 */
    for (n = 1; n < 128u; n++) {
        uint32_t ph = c * n * (1u << 24) + (uint32_t)(int32_t)SINE[(m * n * 4u) & 1023u] * it * 2u;
        td[n] = sine_i(ph);
    }
    for (k = 1; k <= WV_H; k++) {
        int64_t acc = 0;
        uint32_t i;
        for (n = 1, i = 4u * k; n < 128u; n++, i += 4u * k)
            acc += (int64_t)td[n] * SINE[i & 1023u];
        a[k] = (int32_t)(acc >> 20);
    }
}

static void wv_recipe(uint32_t tb, int32_t x, int32_t *a)
{
    uint32_t k, j, seg = (uint32_t)x * 4u, i4 = seg >> 16 > 3u ? 3u : seg >> 16;
    int32_t f4 = seg >> 16 > 3u ? 65536 : (int32_t)(seg & 0xFFFFu);   /* (ORGAN, VOWEL: 4 segments between 5 keys) */
    for (k = 0; k <= WV_H; k++)
        a[k] = 0;
    switch (tb) {
    case 4:                                                  /* ORGAN: the registrations, faded between */
        for (j = 0; j < 7u; j++) {
            int32_t u = WV_DB[WV_ORG[i4][j]], w = WV_DB[WV_ORG[i4 + 1u][j]];
            a[WV_ORG_K[j]] = u + (int32_t)(((int64_t)(w - u) * f4) >> 16);
        }
        return;
    case 10:                                                 /* DIGI: FM 1:1, the index 0 -> 7 (a sine to a bright DX-like tone) */
        wv_fm(1, 1, x * 7, a);
        return;
    case 11:                                                 /* GLASS: FM 1:7, the index 0.2 -> 3.2 (sparse high partials) */
        wv_fm(1, 7, 13107 + x * 3, a);
        return;
    case 12: {                                               /* CHIME: the bar's modes, each r times the one before, r 0.3 -> 0.85 */
        int32_t r = 19661 + (int32_t)(((int64_t)x * 36045) >> 16), v = 65536;
        for (j = 0; j < 10u; j++, v = (int32_t)(((int64_t)v * r) >> 16))
            a[WV_CHIME[j]] = v;
        return;
    }
    case 17: {                                               /* PIPE: the ranks drawn one by one (1 -> 10 of them), each a little */
        int32_t c = 65536 + x * 9;                           /* softer than the one before, a pipe's own overtones 1, 1/4, 1/9 */
        for (j = 0; j < 10u; j++) {
            int32_t g = (int32_t)(((int64_t)(65536 - (int32_t)j * 4500) * wv_step(c, j + 1u)) >> 16);
            for (k = 1; k <= 3u && WV_PIPE[j] * k <= WV_H; k++)
                a[WV_PIPE[j] * k] += g / (int32_t)(k * k);
        }
        return;
    }
    case 23: {                                               /* METAL: the hum (the fundamental) and the membrane's modes, the plate */
        uint32_t s = (uint32_t)wv_exp2(65536 + x * 2);       /* shrinking: S 2 -> 8 harmonics; */
        int32_t lv = 52429;                                  /* a mode at r S lies between two harmonics and is shared */
        a[1] = 65536;                                        /* between them (a cluster: the roughness of a metal partial), */
        for (j = 0; j < 8u; j++, lv = (lv * 4) / 5) {        /* each mode 0.8 x the one before, the signs alternating */
            uint32_t p = (uint32_t)(((uint64_t)WV_MEMB[j] * s) >> 16), h = p >> 16;
            int32_t fr = (int32_t)(p & 0xFFFFu), sv = j & 1u ? -lv : lv;
            if (h <= WV_H)
                a[h] += (int32_t)(((int64_t)sv * (65536 - fr)) >> 16);
            if (h + 1u <= WV_H)
                a[h + 1u] += (int32_t)(((int64_t)sv * fr) >> 16);
        }
        return;
    }
    default:
        break;
    }
    for (k = 1; k <= WV_H; k++) {
        int32_t v = 65536 / (int32_t)k;                     /* (the saw's 1/k, the start of most) */
        switch (tb) {
        case 0:                                              /* SAWSQ: saw -> square, the even harmonics fade out */
            if (!(k & 1u))
                v = (int32_t)(((int64_t)v * (65536 - x)) >> 16);
            break;
        case 1: {                                            /* PWM: pulse width 50 % -> 5 %: sin(pi k w) / k */
            uint32_t w = 32768u - (uint32_t)(((int64_t)x * 29491) >> 16);   /* w Q16: 0.5 .. 0.05 */
            v = (wv_sin((k * w) >> 1) * 2) / (int32_t)k;
            break;
        }
        case 2:                                              /* HARM: sine -> saw, the harmonics come in one by one, */
            v = (int32_t)(((int64_t)v * wv_step(wv_exp2(x * 6), k)) >> 16);   /* their count 1 -> 64 exponential in POS */
            break;
        case 3:                                              /* ODD: sine -> square, the odd harmonics one by one (1 -> 32 of them) */
            v = k & 1u ? (int32_t)(((int64_t)v * wv_step(wv_exp2(x * 5), (k + 1u) >> 1)) >> 16) : 0;
            break;
        case 5: {                                            /* VOWEL: A E I O U, the formants glide between them */
            int64_t sum = 0;
            for (j = 0; j < 3u; j++) {
                int32_t fq = WV_VOW[i4][j] + (int32_t)(((int64_t)(WV_VOW[i4 + 1u][j] - WV_VOW[i4][j]) * f4) >> 16);
                int64_t c16 = (int64_t)fq * 501, w16 = (int64_t)WV_VOW_BW[j] * 501, d = ((int64_t)k << 16) - c16, w2;
                w16 = w16 < 65536 ? 65536 : w16;
                w2 = w16 * w16;
                sum += ((int64_t)WV_VOW_A[j] * (int32_t)((w2 * 65536) / (w2 + d * d))) >> 16;   /* a resonance: w^2 / (w^2 + d^2) */
            }
            v = (int32_t)(sum * 8 / (8 + (int32_t)k));       /* the voice's gentle tilt */
            break;
        }
        case 6: {                                            /* BRASS: K^2 / (K^2 + k^2), K 1.5 -> 12: the harmonics grow in, */
            int64_t K = (int64_t)wv_exp2(x * 3) * 3 / 2, K2 = K * K;   /* as a brass tone's with its loudness */
            v = (int32_t)((K2 * 65536) / (K2 + ((int64_t)k * k << 32)));
            break;
        }
        case 7: {                                            /* BOWED: a bowed string, sin(pi k b) / k (the bow at b of the */
            uint32_t b = (uint32_t)(((uint64_t)65536 << 16) / (4u * 65536u + 12u * (uint32_t)x));   /* length): b 1/4 -> 1/16 */
            v = (wv_sin((k * b) >> 1) * 2) / (int32_t)k;     /* (near the bridge: brighter, nasal) */
            break;
        }
        case 8: {                                            /* RESO: a saw through a resonant low-pass (Q 6) swept from */
            int64_t c = wv_exp2(65536 + x * 5), r = ((int64_t)k << 32) / c, om = 65536 - ((r * r) >> 16);   /* harmonic 2 to 64 */
            int64_t den = om * om + (r / 6) * (r / 6);
            uint64_t sq = wv_isqrt((uint64_t)den);
            v = (int32_t)((((int64_t)1 << 32) / (int64_t)(sq ? sq : 1)) / (int32_t)k);
            break;
        }
        case 9: {                                            /* COMB: a saw through a comb, its notches every 1 / t harmonics, */
            uint32_t t = (uint32_t)(((uint64_t)65536 << 16) / (3u * (uint32_t)wv_exp2(x * 3)));   /* t 1/3 -> 1/24 */
            int32_t g = 65536 + ((wv_sin(k * t + 16384u) * 58982) >> 15);   /* 1 + 0.9 cos(2 pi k t) */
            v = (int32_t)(((int64_t)v * g) / 124518);
            break;
        }
        case 13: {                                           /* ROUGH: saw -> random levels and signs, k^-1/2 (a fixed draw: */
            uint32_t h = k * 2654435761u, r = 13107u + (((h >> 16) * 52429u) >> 16);   /* the same on every build) */
            int64_t is = ((int64_t)1 << 32) / (int64_t)wv_isqrt((uint64_t)k << 32);
            int32_t rnd = (int32_t)((((int64_t)x * r >> 16) * is) >> 16);
            v = (int32_t)(((int64_t)v * (65536 - x)) >> 16) + ((h >> 15) & 1u ? -rnd : rnd);
            break;
        }
        case 14: {                                           /* BAND: a band of harmonics (a raised cosine, half width c / 5, at */
            int32_t c = wv_exp2(65536 + x * 3), hw = c / 5 < 98304 ? 98304 : c / 5, d = ((int32_t)k << 16) - c;   /* least 1.5) */
            d = d < 0 ? -d : d;                              /* moving up from harmonic 2 to 16 over a softer fundamental */
            v = d < hw ? 32768 + wv_sin((uint32_t)(((int64_t)d << 15) / hw) + 16384u) : 0;
            v += k == 1u ? 26214 : 0;
            break;
        }
        case 15: {                                           /* DUAL: a saw through two resonant peaks (width c / 5), one rising */
            int32_t ca = wv_exp2(65536 + x * 4), cb = wv_exp2(327680 - x * 4);   /* from harmonic 2 to 32, the other falling */
            int64_t kk = (int64_t)k << 16;                   /* from 32 to 2 */
            v = (16384 + 3 * wv_res(kk - ca, ca / 5) + 3 * wv_res(kk - cb, cb / 5)) / (int32_t)k;
            break;
        }
        case 16: {                                           /* THIN: a saw through a 24 dB high shelf ((1/8 + r^4) / (1 + r^4), */
            int64_t r = ((int64_t)k << 32) / wv_exp2((x * 5) >> 1), r2 = (r * r) >> 16, r4 = (r2 * r2) >> 16;   /* r = k / c: the lows */
            v = (int32_t)(((int64_t)v * (((8192 + r4) * 65536) / (65536 + r4))) >> 16);   /* -18 dB at most) swept from */
            /* harmonic 1 to 5.7: full to thin and buzzy (a high-pass that keeps a little body: high notes do not vanish) */
            break;
        }
        case 18: {                                           /* REESE: three saws, the outer two drifting against the middle one */
            int32_t g = 65536 + 4 * wv_sin(((k * (uint32_t)x) >> 1) + 16384u);   /* by 0 -> half a cycle: 1 + 2 cos(k d), */
            v = (int32_t)(((int64_t)v * g) / 196608);        /* d 0 -> pi: what detuned saws pass through in half a beat */
            break;
        }
        case 19: {                                           /* HOOVR: three saw-and-pulse waves an octave apart (1, 0.75, 0.5), */
            uint32_t w = 32768u - (((uint32_t)x * 26214u) >> 16), m, o;   /* the pulse width 50 % -> 10 % */
            for (v = 0, o = 0; o < 3u; o++)
                if (!(k & ((1u << o) - 1u))) {
                    m = k >> o;
                    v += (int32_t)(((int64_t)(65536 - (int32_t)o * 16384) * ((32768 + wv_sin((m * w) >> 1) * 2) / (int32_t)m)) >> 16);
                }
            break;
        }
        case 20:                                             /* GROWL: a saw under a man's vowels, U -> AH -> AE, strong peaks */
        case 22: {                                           /* YELL: AH -> EE, F2 rising and a strong 3 kHz ring */
            const uint16_t(*fq)[3] = tb == 20u ? WV_GRL : WV_YEL;
            uint32_t y = tb == 22u;
            int64_t sum = (int64_t)WV_FG[y][0] << 12, sc = y ? 501 : 1002;   /* (C3 130.8 Hz, C2 65.4 Hz: x 65536 / f) */
            for (j = 0; j < 3u; j++) {
                int32_t f = fq[i4][j] + (int32_t)(((int64_t)(fq[i4 + 1u][j] - fq[i4][j]) * f4) >> 16);
                sum += ((int64_t)WV_FG[y][j + 1u] * wv_res(((int64_t)k << 16) - f * sc, (int64_t)WV_FBW[y][j] * sc)) >> 4;
            }
            v = (int32_t)(sum / (int32_t)k);
            break;
        }
        case 21: {                                           /* WOBBL: a saw through a 24 dB resonant low-pass (two 12 dB Q 3 */
            int64_t r = ((int64_t)k << 32) / wv_exp2(32768 + x * 3), om = 65536 - ((r * r) >> 16);   /* stages: 1 / ((1 - r^2)^2 + */
            int64_t den = ((om * om) >> 16) + ((r / 3) * (r / 3) >> 16);   /* (r / 3)^2)), its cutoff from harmonic 1.4 to 11: */
            v = (int32_t)((((int64_t)1 << 32) / (den ? den : 1)) / (int32_t)k);   /* the wobble bass's filter, along the scan */
            break;
        }
        case 24: {                                           /* STEPS: a sine held in S steps a cycle (S 64 -> 3, one a frame): */
            uint32_t s = WV_STEPS[((uint32_t)x * (WV_F - 1u) + 32768u) >> 16], r = k % s;   /* the harmonics n S +- 1 at */
            v = r == 1u || r == s - 1u ? ((k + 1u) / s & 1u ? -v : v) : 0;   /* (-1)^n / k: a low sample rate's sound */
            break;
        }
        default: {                                           /* AIR: a soft flute (harmonics 1 2 3) and a breath growing in: */
            uint32_t h = (k * 2654435761u) ^ (k * 2246822507u >> 13), r;   /* the harmonics from 4 up at random levels */
            int64_t is = ((int64_t)1 << 33) / (int64_t)wv_isqrt((uint64_t)k << 32);    /* and signs (a fixed draw), (4 / k)^1/2, */
            int32_t g = 3277 + (int32_t)(((int64_t)x * 36045) >> 16), rnd;   /* 0.05 -> 0.6 */
            h *= 2246822507u;
            r = 19661u + (((h >> 16) * 45875u) >> 16);
            rnd = (int32_t)((((int64_t)g * r >> 16) * is) >> 16);
            v = k == 1u ? 65536 : k == 2u ? 13107 : k == 3u ? 6554 : (h >> 15) & 1u ? -rnd : rnd;
            break;
        }
        }
        a[k] = v;
    }
}

/* ------------------------------------------------------------------------------------------------- the frame --- */
static int32_t wv_q8(int32_t y, uint64_t g)              /* y x g (Q32), rounded, into -127..127 (symmetric) */
{
    uint64_t m = (uint64_t)(y < 0 ? -(int64_t)y : y) * g;
    int32_t q = (int32_t)((m + 0x80000000ull) >> 32);
    q = q > 127 ? 127 : q;
    return y < 0 ? -q : q;
}

/* user slot u's frame f for the build: the stored bytes -> the harmonics' levels (Q16) in a[1..]. An empty slot: a sine */
static void wv_user(uint32_t u, uint32_t f, int32_t *a)
{
    uint32_t k;
    for (k = 0; k <= WV_H; k++)
        a[k] = 0;
    if (u < WV_NU && (wt_used >> u & 1u))
        for (k = 1; k <= WV_H; k++)
            a[k] = wt_q16(wt_ram[u].amp[f * WV_H + k - 1u]);
    else
        a[1] = 65536;
}

/* frame f of table tb into out (WV_FS bytes): the recipe, the levels' sums, one gain, the samples */
static void wv_frame(uint32_t tb, uint32_t f, int8_t *out)
{
    int32_t *a = wvj.a, *oe = wvj.oe, big = 0;
    uint32_t l, k, n, at = 0, top = 0;
    uint64_t pk = 0, ss = 0, g;
    if (tb >= WV_NTF)                                       /* DUELL 0.2.14: a user slot's bytes are the recipe's levels */
        wv_user(tb - WV_NTF, f, a);
    else
        wv_recipe(tb, (int32_t)((f * 65536u) / (WV_F - 1u)), a);
    for (k = 1; k <= WV_H; k++)
        big = a[k] > big ? a[k] : -a[k] > big ? -a[k] : big;
    for (k = 1; k <= WV_H; k++) {                           /* Q10, the largest 1023: 64 terms x 2^25 stay in 32 bits */
        a[k] = big ? (int32_t)(((int64_t)a[k] * 1023 + (a[k] < 0 ? -big / 2 : big / 2)) / big) : 0;
        ss += (uint64_t)((int64_t)a[k] * a[k]);
        top = a[k] ? k : top;
    }
    for (l = 0; l < WV_NL; l++) {
        uint32_t N = WV_N[l], H = WV_H >> l < top ? WV_H >> l : top, q = N / 4u, m = 1024u / N;   /* (SINE: 1024 points a cycle;
                                                             * the sums stop at the frame's last harmonic: ORGAN's 8, CHIME's 49) */
        for (n = 0; n <= q; n++, at++) {
            int32_t o = 0, e = 0;
            uint32_t s = 2u * n * m, i;
            for (k = 1, i = n * m; k <= H; k += 2, i += s)  /* odd harmonics */
                o += a[k] * SINE[i & 1023u];
            for (k = 2, i = s; k <= H; k += 2, i += s)      /* even ones */
                e += a[k] * SINE[i & 1023u];
            oe[2u * at] = o;
            oe[2u * at + 1u] = e;
            {
                uint64_t u = (uint64_t)(o < 0 ? -(int64_t)o : o) + (uint64_t)(e < 0 ? -(int64_t)e : e);
                pk = u > pk ? u : pk;                       /* the larger of |o + e|, |o - e| */
            }
        }
    }
    /* the gain (Q32): RMS to WV_RMS / 100 (the level-0 RMS: 32767 x sqrt(sum a^2 / 2) in the sums' units), the peak at most 127 */
    {
        uint64_t rms = wv_isqrt(ss * 32767ull * 32767ull / 2u);
        g = rms ? ((uint64_t)WV_RMS << 32) / (rms * 100u) : 0;
        if (pk && (uint64_t)127 << 32 < g * pk)
            g = ((uint64_t)127 << 32) / pk;
    }
    out[WV_FUND] = (int8_t)wv_q8((a[1] * 32767) >> 1, g);   /* the fundamental's amplitude / 2 (a square's is 1.27 x its peak) */
    for (l = 0, at = 0; l < WV_NL; l++) {                   /* f(n) = o + e, f(N/2 - n) = o - e, f(N/2 + n) = e - o, f(N - n) = -f(n) */
        uint32_t N = WV_N[l], q = N / 4u;
        int8_t *s = out + WV_LO[l];
        for (n = 0; n <= q; n++, at++) {
            int32_t o = oe[2u * at], e = oe[2u * at + 1u], u = wv_q8(o + e, g), w = wv_q8(o - e, g);
            s[n] = (int8_t)u;
            s[N / 2u - n] = (int8_t)w;
            s[N / 2u + n] = (int8_t)-w;
            s[N - n] = (int8_t)-u;                          /* (n 0: s[N], the guard, = s[0] = 0) */
        }
    }
}

#if defined(FM1_IRQ_TARGET) || defined(HOST_IRQ_HOOK)    /* (HOST_IRQ_HOOK: tests/loadcost_test.c counts the windows) */
#define WV_LOCK() fm1_irq_off()
#define WV_UNLOCK() fm1_irq_on()
#else
#define WV_LOCK() ((void)0)                              /* (host builds: one thread) */
#define WV_UNLOCK() ((void)0)
#endif

/* MAIN LOOP (main.c: every pass and in its wait; the browser build's web_frame; ui_test.c frame): one frame of a table build,
 * if a part waits for one. 1 = it did some work (more may follow), 0 = nothing to do */
static int wave_service(void)
{
    uint32_t p, ok;
    wv_mem_t *w;
    if (!wvj.on) {
        for (p = 0; p < NLIVE && !wvj.on; p++) {
            w = (wv_mem_t *)ENG_MEM(p);
            if (eng_mem_owner[p] == ENGI_WAVE + 1u && w->h.want && !w->h.have && !w->h.fade) {
                wvj.on = 1;
                wvj.part = (uint8_t)p;
                wvj.table = (uint8_t)(w->h.want - 1u);
                wvj.frame = 0;
                wvj.gen = wv_gen[p];
                if (wvj.table >= WV_NTF && wvj.table - WV_NTF < WV_NU)
                    wv_bver[p] = wv_uver[wvj.table - WV_NTF];   /* (the user table this build is made from) */
            }
        }
        if (!wvj.on)
            return 0;
    }
    p = wvj.part;
    w = (wv_mem_t *)ENG_MEM(p);
    wv_frame(wvj.table, wvj.frame, wvj.out);
    WV_LOCK();                                           /* (the audio side must not see half a frame, nor lose its region under it) */
    ok = eng_mem_owner[p] == ENGI_WAVE + 1u && wv_gen[p] == wvj.gen && w->h.want == wvj.table + 1u && !w->h.have;
    if (ok)
        memcpy(w->tab + wvj.frame * WV_FS, wvj.out, WV_FS);
    if (ok && wvj.frame + 1u == WV_F)
        w->h.have = w->h.want;                          /* built: the voices fade in */
    WV_UNLOCK();
    if (!ok || ++wvj.frame == WV_F)
        wvj.on = 0;
    return 1;
}

static uint32_t wv_table_of(int32_t v) { return v < 0 ? 0u : (uint32_t)v >= WV_NT ? WV_NT - 1u : (uint32_t)v; }

/* AUDIO, once a block and part (engine_t.block): the table asked for. A change fades out the old table (WV_FADE blocks), then
 * drops it (the build may begin); a change back to the table in memory during its fade keeps it */
static void wave_block(track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), tb;
    wv_mem_t *w = (wv_mem_t *)ENG_MEM(p);
    if (!w)
        return;
    tb = wv_table_of(t->p[P_E0]) + 1u;
    if (w->h.fade && !--w->h.fade) {
        w->h.have = 0;
        wv_gen[p]++;
    }
    if (tb > WV_NTF && w->h.want == tb && w->h.have == tb && !w->h.fade && wv_bver[p] != wv_uver[tb - 1u - WV_NTF])
        w->h.fade = WV_FADE;                             /* the slot's table was stored again: out, then built anew */
    if (w->h.want != tb) {
        w->h.want = (uint8_t)tb;
        if (w->h.have == tb)
            w->h.fade = 0;
        else if (w->h.have && !w->h.fade)
            w->h.fade = WV_FADE;
        else if (!w->h.have)
            wv_gen[p]++;
    }
}

/* the host renders (tests/hostsim.c) and tests: part t's table built at once (as the main loop would, then) */
static __attribute__((unused)) void wave_build_now(track_t *t)
{
    uint32_t i;
    if ((uint32_t)(t - trk) >= NLIVE || t->engine != ENGI_WAVE)
        return;
    for (i = 0; i < 64u; i++) {
        wv_mem_t *w = (wv_mem_t *)ENG_MEM(t - trk);
        wave_block(t);
        while (wave_service())
            ;
        if (w->h.have && w->h.have == w->h.want)
            return;
    }
}

/* -------------------------------------------------------------------------------------------------- the voice --- */
/* v->s[]: 0 the scan place at the end of the last block (Q16 frames, -1: none yet), 1 the voice's table gain (Q15, the fades),
 * 2 3 a SYNC reset's correction still owed to the next block's first sample (the oscillator's, DTN's), 4 5 the synced read
 * phases (the oscillator's, DTN's; without SYNC they follow the free phases, so SYNC comes in without a jump), 6 the DC
 * tracker's level (BEND and SYNC move the cycle's mean; << 8), 7 DTN's share in blocks (bits 16..19, 0 .. WV_DF; bits 0..15 were the engine's own SUB level, retired in cloud job J: the global SUB LAYER,
 * chain_sub.c, replaces it; hunt3: bits 20..22 counted the note's first blocks, no longer used). ph[0] the oscillator, ph[1] DTN's.
 * DTN without a zipper (0.2.25, owner: turning DTN on a held note crackled, clearest on a sine, an EMPTY user slot): DTN's second
 * read fades in and out over WV_DF blocks when DTN leaves or reaches 0 (DTN's other changes are pitch only: no step). (hunt3: in a note's first 4 blocks it used to follow at once; ENV 2 opening DTN at
 * the attack (DIGI PLUCK) switched the second read in with a step of its mix, so now it glides from the first block like at any other time). A value that does not move plays bit for bit as before */
#define WV_DF 8u                 /* blocks of DTN's fade in / out: 5.8 ms (wv_dmix's weights: WV_DF x 4096 = 32768) */

static int32_t wv_s7(const int16_t *p) { return p[P_E5] ? (int32_t)(WV_DF << 16) : 0; }
static uint8_t wv_stk[NLIVE][NVOICE], wv_stn[NLIVE][NVOICE];   /* STEPS: the place a voice holds and the count it was held for (wave_steps, below the engine) */
static void wave_note_on(track_t *t, voice_t *v)
{
    const wv_mem_t *w = (const wv_mem_t *)ENG_MEM(t - trk);
    uint32_t k;
    for (k = 0; k < 8u; k++)
        v->s[k] = 0;
    v->s[0] = -1;
    v->s[1] = w && w->h.have && w->h.have == w->h.want ? 32767 : 0;
    v->s[7] = wv_s7(t->p);
    v->ph[1] = v->ph[0] + 0x55555555u;                  /* (DTN's read: a third of a cycle on, no beating from in phase) */
    v->ph[2] = 0;
    v->s[4] = (int32_t)v->ph[0];
    v->s[5] = (int32_t)v->ph[1];
    if ((uint32_t)(t - trk) < NLIVE && (uint32_t)(v - t->v) < NVOICE)
        wv_stn[t - trk][v - t->v] = 0;                  /* STEPS: no place held yet (wave_steps takes the nearest) */
}

/* one level's read into dst[n]: the read phases rp (32 bit), the scan place fp (Q16 frames) stepping dfp a sample */
static void wv_read(const int8_t *tab, uint32_t lo, uint32_t sh, const uint32_t *rp, int32_t fp, int32_t dfp, int32_t *dst,
                    uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint32_t ph = rp[i], wq = ((uint32_t)fp >> 4) & 0xFFFu;
        const int8_t *a = tab + ((uint32_t)fp >> 16) * WV_FS + lo + (ph >> sh);
        int32_t fr = (int32_t)((ph >> (sh - 12u)) & 0xFFFu);
        int32_t x0 = a[0], x1 = a[1], y0 = a[WV_FS], y1 = a[WV_FS + 1u];
        int32_t l = (x0 << 12) + (x1 - x0) * fr, h = (y0 << 12) + (y1 - y0) * fr;
        dst[i] = (l + ((((h - l) >> 6) * (int32_t)wq) >> 6)) >> 4;   /* 127 -> 32512 */
        fp += dfp;
    }
}
/* the sixth level: the frames' fundamentals (WV_FUND, mixed as the frames), a sine at the read phase */
static void wv_read_sine(const int8_t *tab, const uint32_t *rp, int32_t fp, int32_t dfp, int32_t *dst, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        const int8_t *a = tab + ((uint32_t)fp >> 16) * WV_FS + WV_FUND;
        int32_t f0 = a[0], fa = (f0 << 12) + (a[WV_FS] - f0) * (int32_t)(((uint32_t)fp >> 4) & 0xFFFu);   /* Q12 */
        dst[i] = (sine_i(rp[i]) * (fa >> 6)) >> 12;     /* x 2 (it is halved) x 256 (8 bits -> Q15) */
        fp += dfp;
    }
}
/* the oscillator's read at level l (WV_NL: the sixth, the sine), cross-faded (lw Q12, 0: none) into level l + 1 */
static void wv_lvl(const int8_t *tab, uint32_t l, const uint32_t *rp, int32_t fp, int32_t dfp, int32_t *dst, uint32_t n)
{
    if (l < WV_NL)
        wv_read(tab, WV_LO[l], WV_SH[l], rp, fp, dfp, dst, n);
    else
        wv_read_sine(tab, rp, fp, dfp, dst, n);
}
static void wv_osc(const int8_t *tab, uint32_t l, int32_t lw, const uint32_t *rp, int32_t fp, int32_t dfp, int32_t *dst,
                   uint32_t n)
{
    wv_lvl(tab, l, rp, fp, dfp, dst, n);
    if (lw) {
        int32_t tmp[CTL];
        uint32_t i;
        wv_lvl(tab, l + 1u, rp, fp, dfp, tmp, n);
        for (i = 0; i < n; i++)
            dst[i] += ((tmp[i] - dst[i]) * lw) >> 12;
    }
}

/* BEND: the read phase warped, the cycle's first half squeezed into b of it, the second half stretched over the rest (a skew,
 * like a pulse width for any wave). The squeezed half is read s times as fast, s = 10^(BEND / 127): 1 .. 10 in even steps of
 * pitch (b = 1 / 2s: 1/2 .. 1/20), as SYNC's ratio. 20-bit phase in, the slopes so the products stay in 32 bits */
typedef struct { uint32_t b, k1, k2, slope; } wv_warp_t;
static void wv_warp_set(wv_warp_t *wp, int32_t bend)
{
    uint32_t s = (uint32_t)wv_exp2(bend * 217706 / 127), b = (0x80000000u / s) << 4;   /* s Q16 (log2 10 = 3.32); b = 2^35 / s, Q20 */
    wp->b = b;
    wp->k1 = 0x80000000u / b;
    wp->k2 = 0x80000000u / (1048576u - b);
    wp->slope = s;                                       /* the steepest part's speed, Q16 */
}
static void wv_warp(const wv_warp_t *wp, uint32_t *rp, uint32_t n)
{
    uint32_t i, b = wp->b, k1 = wp->k1, k2 = wp->k2;
    for (i = 0; i < n; i++) {
        uint32_t p = rp[i] >> 12;
        rp[i] = p < b ? p * k1 : 0x80000000u + (p - b) * k2;
    }
}

/* the read phases of a block into rp: free (SYNC 0), or hard-synced to the free phase *php at ratio r16 (Q16, 1 .. 8): the
 * read restarts each time the free phase wraps. A restart is a step in the wave: its sample index (1..n, n: the next block's
 * first) and how far after the step that sample lies (Q15 of a sample) go to ev / et, for the correction (wv_blep) */
static uint32_t wv_phases(uint32_t *php, uint32_t *spp, uint32_t inc, uint32_t r16, uint32_t *rp, uint8_t *ev, uint16_t *et,
                          uint32_t n)
{
    uint32_t ph = *php, sp = *spp, i, ne = 0;
    if (r16 <= 65536u) {
        for (i = 0; i < n; i++) {
            rp[i] = ph;
            ph += inc;
        }
        *php = ph;
        *spp = ph;                                       /* (the synced phase follows: SYNC turned up starts from here) */
        return 0;
    } else {
        uint64_t s64 = ((uint64_t)inc * r16) >> 16;
        uint32_t sinc = s64 > 0x7FFFFFFFu ? 0x7FFFFFFFu : (uint32_t)s64;
        for (i = 0; i < n; i++) {
            rp[i] = sp;
            ph += inc;
            sp += sinc;
            if (ph < inc) {                              /* the free phase wrapped: the synced one starts over where it is */
                sp = (uint32_t)(((uint64_t)ph * r16) >> 16);
                ev[ne] = (uint8_t)(i + 1u);
                et[ne++] = (uint16_t)(ph / ((inc >> 15) | 1u) > 32767u ? 32767u : ph / ((inc >> 15) | 1u));   /* (32-bit divide) */
            }
        }
    }
    *php = ph;
    *spp = sp;
    return ne;
}
/* one sample of the read (level l, no cross-fade) at phase ph, scan place fp */
static int32_t wv_read1(const int8_t *tab, uint32_t l, uint32_t ph, int32_t fp)
{
    uint32_t r = ph;
    int32_t s;
    wv_lvl(tab, l, &r, fp, 0, &s, 1);
    return s;
}
/* the SYNC restarts' steps smoothed (a two-sample polyBLEP): a step of d (the wave from its value at the free phase's wrap, end,
 * to its start, 0: every table starts at 0) at t (Q15) before sample k adds d (t^2 / 2) to sample k - 1 and -d (1 - t)^2 / 2 to
 * sample k; k = n is the next block's first: that part is owed (*owe) */
static void wv_blep(int32_t *s, uint32_t n, const uint8_t *ev, const uint16_t *et, uint32_t ne, int32_t end, int32_t *owe)
{
    uint32_t j;
    s[0] += *owe;
    *owe = 0;
    for (j = 0; j < ne; j++) {
        int32_t t = et[j], u = 32768 - t, d = -end, k = ev[j];
        s[k - 1] += (int32_t)(((int64_t)d * ((t * t) >> 15)) >> 16);
        if ((uint32_t)k < n)
            s[k] -= (int32_t)(((int64_t)d * ((u * u) >> 15)) >> 16);
        else
            *owe = -(int32_t)(((int64_t)d * ((u * u) >> 15)) >> 16);
    }
}

#ifdef WV_TEST_HOOKS
static uint8_t wv_test_l0, wv_test_noblep;               /* tests/wave_test.c: always level 0; no SYNC correction (to measure them) */
#endif
/* the level for an increment ie (with what multiplies the read's speed), *lw the cross-fade into the next one (Q12) */
static uint32_t wv_level(uint64_t ie, int32_t *lw)
{
    uint32_t l = 0;
    uint64_t hi, lo;
#ifdef WV_TEST_HOOKS
    if (wv_test_l0) {
        *lw = 0;
        return 0;
    }
#endif
    while (l < WV_NL && ie > (uint64_t)WV_SAFE << l)
        l++;
    hi = (uint64_t)WV_SAFE << l;
    lo = hi - (hi * 4799u >> 14);                        /* half an octave under it (x 0.7071): a level's top harmonics go out over a
                                                         * half octave of speed (SYNC and BEND sweep it fast: a sparse bright table,
                                                         * GLASS, lost them within a few % of MORPH travel at a quarter octave) */
    *lw = l < WV_NL && ie > lo ? (int32_t)((uint32_t)(ie - lo) / ((uint32_t)(hi - lo) >> 12)) : 0;   /* (hi < 2^31 here: 32-bit) */
    if (*lw > 4096)
        *lw = 4096;
    return l;
}

/* one oscillator (the main one, k 0, or DTN's, k 1) into dst: its phases, the warp, the read, the SYNC corrections */
static void wv_line(const wv_mem_t *w, voice_t *v, uint32_t k, uint32_t inc, uint32_t r16, const wv_warp_t *wp, uint32_t l,
                    int32_t lw, int32_t fp, int32_t dfp, int32_t *dst, uint32_t n)
{
    uint32_t rp[CTL], ne, sp = (uint32_t)v->s[4u + k];
    uint8_t ev[CTL];
    uint16_t et[CTL];
    ne = wv_phases(&v->ph[k], &sp, inc, r16, rp, ev, et, n);
    v->s[4u + k] = (int32_t)sp;
    if (wp)
        wv_warp(wp, rp, n);
    wv_osc(w->tab, l, lw, rp, fp, dfp, dst, n);
    if (r16 > 65536u
#ifdef WV_TEST_HOOKS
        && !wv_test_noblep
#endif
    ) {
        uint32_t pe = (r16 & 0xFFFFu) << 16;              /* the synced phase at the free one's wrap: r mod 1 */
        if (wp)
            wv_warp(wp, &pe, 1);
        wv_blep(dst, n, ev, et, ne, ne ? wv_read1(w->tab, l, pe, fp + dfp * (int32_t)ev[0]) : 0, &v->s[2u + k]);
    } else {
        v->s[2u + k] = 0;
    }
}

/* SYNC's ratio (*r16), BEND's warp (*wp) and the level they read at (*lw its cross-fade): once a block and voice, out of line (DUELL 0.2.25: in
 * the flash; their divides and 64-bit compares were 0.3 KB of the hot block, which DTN's smoothing now uses) */
static __attribute__((noinline)) uint32_t wv_speed(uint32_t inc, int32_t sync, int32_t bend, uint32_t *r16, wv_warp_t *wp, int32_t *lw)
{
    uint64_t ie = inc;
    if (sync > 0) {
        *r16 = (uint32_t)wv_exp2(sync * 3 * 65536 / 127);
        ie = (ie * *r16) >> 16;
    }
    if (bend > 0) {
        wv_warp_set(wp, bend);
        ie = (ie * wp->slope) >> 16;
    }
    return wv_level(ie, lw);
}
/* DTN's share moving from d0 to d1 (of WV_DF) over the block: s1 = the oscillator + share x (DTN - it) / 2 (at WV_DF: (s1 + s2) / 2, as
 * the steady mix; out of line: it runs only for WV_DF blocks after DTN leaves or reaches 0) */
static __attribute__((noinline)) void wv_dmix(int32_t *s1, const int32_t *s2, int32_t d0, int32_t d1, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t wq = (d0 << 12) + (((d1 - d0) * (int32_t)i << 12) / (int32_t)CTL);   /* Q15 (WV_DF x 4096 = 32768) */
        s1[i] += (((s2[i] - s1[i]) >> 1) * wq) >> 15;
    }
}

static void wave_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const wv_mem_t *w = (const wv_mem_t *)ENG_MEM(t - trk);
    int32_t g0 = v->s[1], g1, pq, fp1, fp0, dfp, lw, dtn = p[P_E5], bend = p[P_E2], sync = p[P_E3];
    int32_t d0 = (int32_t)((uint32_t)v->s[7] >> 16 & 15u), d1;
    uint32_t i, l, inc = m->inc, r16 = 65536u;
    int32_t s1[CTL], s2[CTL];
    wv_warp_t wp;
    vmod_t mg;
    if (n > CTL)
        n = CTL;
    if (!w || !w->h.have) {                              /* no table (being built): silent */
        v->s[1] = 0;
        return;
    }
    g1 = w->h.have == w->h.want ? g0 + WV_GSTEP : g0 - WV_GSTEP;
    g1 = g1 < 0 ? 0 : g1 > 32767 ? 32767 : g1;
    v->s[1] = g1;
    if (!g0 && !g1)
        return;
    /* POS: the knob plus the SHP routes (64 << 8 the centre), over the whole scan */
    pq = clamp((p[P_E1] << 8) + (m->shape - (64 << 8)), 0, 127 << 8);
    fp1 = (pq * WV_FPQ) >> 10;                           /* the whole scan, Q16 frames (32-bit: no 64-bit divide on the device) */
    fp1 = fp1 > WV_FPMAX ? WV_FPMAX : fp1;
    fp0 = v->s[0] < 0 ? fp1 : v->s[0];
    dfp = (fp1 - fp0) / (int32_t)CTL;
    v->s[0] = fp0 + dfp * (int32_t)n;
    /* SYNC: the read's ratio to the note, 1 .. 8 (three octaves, even steps of pitch); BEND: the warp. Both read faster than the
     * note: the level is chosen for the fastest part (the band-limit holds there) */
    l = wv_speed(inc, sync, bend, &r16, &wp, &lw);
    wv_line(w, v, 0, inc, r16, bend > 0 ? &wp : 0, l, lw, fp0, dfp, s1, n);
    d1 = dtn ? (d0 < (int32_t)WV_DF ? d0 + 1 : (int32_t)WV_DF) : (d0 ? d0 - 1 : 0);   /* (DTN's share: on, off, or on its way) */
    v->s[7] = d1 << 16;
    if (dtn || d0) {                                     /* DTN: a second read DTN cents up, half and half */
        wv_line(w, v, 1, cents_inc(m->pitch16, dtn, m->fine), r16, bend > 0 ? &wp : 0, l, lw, fp0, dfp, s2, n);
        if (d0 + d1 == 2 * (int32_t)WV_DF)            /* (steady: as ever) */
            for (i = 0; i < n; i++)
                s1[i] = (s1[i] + s2[i]) >> 1;
        else
            wv_dmix(s1, s2, d0, d1, n);
    } else {
        v->ph[1] += inc * n;                             /* (DTN's phase runs on: turned up, it starts without a jump in pitch) */
        v->s[5] = (int32_t)v->ph[1];
    }
    if (bend > 0 || sync > 0 || v->s[6]) {               /* the DC tracker: the cycle's mean (~7 Hz) taken off */
        int32_t dc = v->s[6];
        for (i = 0; i < n; i++) {
            dc += ((s1[i] << 8) - dc) >> 10;
            s1[i] -= dc >> 8;
        }
        v->s[6] = !bend && !sync && dc > -1024 && dc < 1024 ? 0 : dc;   /* (off again once it has settled near 0) */
    }
    mg = *m;                                             /* the table gain on the amplitude ramp (the fades) */
    mg.amp0 = mulq15(m->amp0, g0);
    mg.amp1 = mulq15(m->amp1, g1);
    for (i = 0; i < n; i++)
        out[i] += voice_amp(s1[i], &mg, i);
}

/* STEPS (E7, P_E6): OFF, 2 .. 19 places the scan's position is rounded to (stored 0 = OFF, n = n + 1 places: no stored value is meaningless). 19 places = every frame of a table. POS, and every
 * route that moves it (the SHP routes: AMP DEST / ENV 2 / LFO 1 / LFO 2 SHP, the matrix's SHP, ENV 2 and LFO SHP depths, CYCLE, RAND per note), then lands on a place: a movement becomes a
 * staircase. wave_render's block ramp (fp0 to fp1, 32 samples = 0.7 ms) is the fixed short cross-fade between two places (a step of one block: no click, as a POS jump); the frames are all in sine
 * phase, so the cross-fade is a mix of two spectra. It is made HERE, on m->shape before the engine renders (track_render calls wave_steps for WAVE with STEPS above 0; wave_render is not touched):
 * m->shape is set so that POS plus the shape routes (the same sum wave_render makes: pq = clamp((E1 << 8) + (shape - (64 << 8)), 0, 127 << 8)) is the chosen place.
 * HYSTERESIS: the place held stays until the target passes the edge between two places by a quarter of a place (3/4 of a place from the held one): a route that hovers on an edge does not flicker.
 * The place held is a voice's (wv_stk, with the count it was held for, wv_stn; 0: none yet, set by a note-on: a new note takes the nearest place; a count that changed does the same) */
static const char *const N_WV_STEPS[19] = {"OFF", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19"};
static __attribute__((noinline)) void wave_steps(track_t *t, voice_t *v, vmod_t *m)
{
    uint32_t ti = (uint32_t)(t - trk), vi = (uint32_t)(v - t->v);
    int32_t n1 = clamp(t->p[P_E6], 1, 18), base = t->p[P_E1] << 8, tgt, u, k, d;
    if (ti >= NLIVE || vi >= NVOICE)
        return;
    tgt = clamp(base + (m->shape - (64 << 8)), 0, 127 << 8);
    u = tgt * n1;                                        /* in places of (127 << 8) / n1: the held place k is at k (127 << 8) */
    k = wv_stk[ti][vi];
    d = u - k * (127 << 8);
    if (wv_stn[ti][vi] != n1 || 4 * (d < 0 ? -d : d) > 3 * (127 << 8)) {   /* none held, another count, or past the edge by a quarter: the nearest place */
        k = (u + (127 << 7)) / (127 << 8);
        wv_stk[ti][vi] = (uint8_t)k;
        wv_stn[ti][vi] = (uint8_t)n1;
    }
    m->shape = (64 << 8) + k * (127 << 8) / n1 - base;
}

static const preset_t WAVE_PRESETS[] = {
    /* name, {TABLE, POS, BEND, SYNC, -, DTN, STEPS, -}, {A D S R}, fenv, mono: the host tests' renders and costs (tests/regress.c);
     * DUELL's factory sounds are tools/duell_sounds.py's. The last one is the costliest voice: SYNC, BEND and DTN at once (E5 was the engine's own SUB until cloud job J) */
    {"SAW PAD", {0, 10, 0, 0, 0, 9, 0, 0}, {40, 90, 100, 70}, 0, 0, FX(0, 40, 20, 50), PAT(5)},
    {"PWM STRING", {1, 40, 0, 0, 0, 0, 0, 0}, {30, 90, 100, 60}, 0, 0, FX(0, 50, 20, 60), PAT(5)},
    {"HARM KEYS", {2, 70, 0, 0, 0, 0, 0, 0}, {0, 80, 40, 50}, 0, 0, FX(0, 20, 20, 40), PAT(7)},
    {"ODD BASS", {3, 50, 0, 0, 0, 0, 0, 0}, {0, 70, 90, 30}, 0, 1, FX(0, 0, 10, 10), PAT(2)},
    {"ORGAN", {4, 64, 0, 0, 0, 0, 0, 0}, {0, 127, 127, 20}, 0, 0, FX(0, 30, 0, 30), PAT(6)},
    {"VOWEL PAD", {5, 50, 0, 0, 0, 8, 0, 0}, {50, 90, 100, 70}, 0, 0, FX(0, 40, 20, 60), PAT(5)},
    {"SYNC LEAD", {0, 20, 0, 70, 0, 0, 0, 0}, {0, 80, 100, 40}, 0, 1, FX(0, 20, 30, 30), PAT(4)},
    {"BENT DIGI", {10, 60, 60, 0, 0, 0, 0, 0}, {0, 90, 60, 50}, 0, 0, FX(0, 20, 30, 40), PAT(7)},
    {"ALL ON", {11, 80, 50, 60, 0, 12, 0, 0}, {10, 90, 100, 60}, 0, 0, FX(0, 30, 20, 40), PAT(5)},
};

static const uint8_t WAVE_UIORD[8] = {0, 1, 2, 3, 5, 6, 4, 7};   /* card position -> E index: ENGINE TABLE POS BEND; EDIT 1 SYNC DTN STEPS and the unused SUB slot (hidden); the stored ids never move */

static const engine_t ENG_WAVE = {
    .name = "WAVE",
    .page_title = {"TABLE", "OSC"},
    .edit = {
        {"TABLE", F_ENUM, 0, WV_NT - 1, 0, N_WV_TABLE, 0},
        {"POS", F_PCT, 0, 127, 0, 0, 0},
        {"BEND", F_PCT, 0, 127, 0, 0, 0},
        {"SYNC", F_PCT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},                 /* was SUB: the global SUB LAYER (chain_sub.c) replaces it */
        {"DTN", F_INT, 0, 127, 0, 0, "ct"},
        {"STEPS", F_ENUM, 0, 18, 0, N_WV_STEPS, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = WAVE_PRESETS,
    .npresets = NELEM(WAVE_PRESETS),
    .note_on = wave_note_on,
    .render = wave_render,
    .block = wave_block,
    .knob = {P_E1, P_E2, P_E3, P_REL},
    .uiord = WAVE_UIORD,
    .keep = 0xF3,                /* the scan place, the table gain, SYNC's phases, the DC blocker */
    .mem = sizeof(wv_hdr_t),     /* (only the header is cleared on a claim: the frames are read only after a build) */
    .ufilt = 1,                  /* no filter of its own: the multimode filter (DUELL 0.2.9; voice.c uf_run: CUT RES MODE SLOPE, P_FREEA..D) */
};
