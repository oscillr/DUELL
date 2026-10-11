/* SPDX-License-Identifier: GPL-3.0-only */
/* The long-window loudness measure of the host tools (tests/sound_audit.c, tests/roll_loudness.c); include after hostsim.c.
 *
 * A render (lz_render, in a forked child: every render from the boot state): the notes held LZ_HOLD s, then LZ_TAIL s of
 * release, measured on the mix before MASTER and the master limiter (fx.c mix_l / mix_r + wet, full scale 32767). Per block
 * the energy of an ear-weighted copy (a 1-pole high-pass at 100 Hz and a 1-pole low-pass at 6 kHz on each channel: a cheap
 * approximation of an A-weighting, so a dark pad does not read as loud as its sub-bass energy and a whistle no louder than
 * it is), the raw energy, and the energy of the first difference (brightness). The loudness (dBFS, weighted):
 *   - a sound that sustains (the last 1.5 s before key-off within 6 dB of its loudest 1.5 s: long enough that two
 *     detuned oscillators' beating, ~10 dB in a 0.5 s window on a pad, averages out): the loudest 1.5 s window (a pad's or
 *     a riser's body, wherever its envelope has it);
 *   - a sound that does not (a pluck, a stab, a decay to silence): the loudest 0.5 s window.
 * Brightness: the RMS frequency of the raw signal over that window (sqrt(diff energy / energy), in Hz: a spectral centroid
 * proxy; a ratio of two of them is what the tools compare). Peak: the whole render, raw. */
#define LZ_HOLD 4.0               /* the default hold (s); the variable lz_hold overrides it (the sound audit: 10 s, the tap: 1 s) */
#define LZ_HOLD_MAX 10.0
#define LZ_TAIL 1.0
#define LZ_NBMAX ((uint32_t)((LZ_HOLD_MAX + LZ_TAIL) * FS) / CTL)
static double lz_hold = LZ_HOLD;  /* set before lz_spawn (the child forks with it) */
#define LZ_HP_HZ 100.0
#define LZ_LP_HZ 6000.0

typedef struct {
    double loud, raw, bright, peak;   /* dBFS weighted, dBFS raw, Hz, dBFS */
    double w15, w05, last15;          /* the windows (dBFS weighted): the loudest 1.5 s, 0.5 s; the 1.5 s before key-off */
    double peak_t;                    /* where the peak is (s from the note-on) */
    int sus, nan;                     /* sustains: the 1.5 s window was used */
    /* the time-resolved measure (a hold of 7 s or more; weighted, STEREO sum of both channels unless 'ch') */
    double l1, l3, l9;                /* level at 1 s, 3 s, 9 s (the 0.5 s window centred there) */
    double med16;                     /* median of the ten 0.5 s windows of seconds 1..6 */
    double t3;                        /* s until a 0.5 s window is within 3 dB of the plateau (the loudest 1.5 s) */
    double cmed, cw15;                /* the same median and loudest 1.5 s, per channel (RMS of the louder channel: SPLIT's side) */
} lz_t;

static double lz_db(double ms) { return ms > 0 ? 10 * log10(ms / (32767.0 * 32767.0)) : -200; }

/* the loudest window of nw blocks over e[0..n): its mean square and where it starts */
static double lz_win(const double *e, uint32_t n, uint32_t nw, uint32_t *at)
{
    double s = 0, best = -1;
    uint32_t i;
    for (i = 0; i < n; i++) {
        s += e[i];
        if (i >= nw)
            s -= e[i - nw];
        if (i + 1u >= nw && s > best) {
            best = s;
            *at = i + 1u - nw;
        }
    }
    return best / ((double)nw * CTL * 2.0);
}

/* the measure of a render whose per-block energies are we (weighted), re (raw), de (difference), peak raw */
/* the level (dBFS) of the nw blocks from block b0 of e; div 2: the sum of two channels, 1: one channel */
static double lz_lvl(const double *e, uint32_t nb, uint32_t b0, uint32_t nw, double div)
{
    double s = 0;
    uint32_t i;
    for (i = b0; i < b0 + nw && i < nb; i++)
        s += e[i];
    return lz_db(s / ((double)nw * CTL * div));
}
static double lz_median(double *v, uint32_t n)
{
    uint32_t i, j;
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && v[j - 1] > v[j]; j--) {
            double t = v[j];
            v[j] = v[j - 1];
            v[j - 1] = t;
        }
    return n & 1u ? v[n / 2u] : 0.5 * (v[n / 2u - 1u] + v[n / 2u]);
}
static lz_t lz_measure(const double *we, const double *wl, const double *wr, const double *re, const double *de, double peak,
                       int nan, uint32_t nb, uint32_t off)
{
    lz_t L;
    L.peak_t = 0;
    uint32_t n15 = (uint32_t)(1.5 * FS) / CTL, n05 = (uint32_t)(0.5 * FS) / CTL;
    uint32_t a15 = 0, a05 = 0, at, i, nw, k;
    double m15 = lz_win(we, nb, n15, &a15), m05 = lz_win(we, nb, n05, &a05), last = 0, r = 0, d = 0;
    memset(&L, 0, sizeof L);
    for (i = off > n15 ? off - n15 : 0; i < off; i++)
        last += we[i];
    last /= (double)(off > n15 ? n15 : off) * CTL * 2.0;
    L.w15 = lz_db(m15);
    L.w05 = lz_db(m05);
    L.last15 = lz_db(last);
    L.sus = L.last15 >= L.w15 - 6.0;
    at = L.sus ? a15 : a05;
    nw = L.sus ? n15 : n05;
    L.loud = L.sus ? L.w15 : L.w05;
    for (i = at; i < at + nw; i++) {
        r += re[i];
        d += de[i];
    }
    L.raw = lz_db(r / ((double)nw * CTL * 2.0));
    L.bright = r > 0 ? sqrt(d / r) * FS / (2.0 * M_PI) : 0;
    L.peak = peak > 0 ? 20 * log10(peak / 32767.0) : -200;
    L.nan = nan;
    if (off >= (uint32_t)(7.0 * FS) / CTL) {      /* (a long hold: the time-resolved columns) */
        double v[10], tot_l = 0, tot_r = 0, vl[10], pl, pr;
        uint32_t s, nwin = (uint32_t)(0.5 * FS) / CTL, sec = (uint32_t)(1.0 * FS) / CTL, q = nwin / 2u;
        L.l1 = lz_lvl(we, nb, sec - q, nwin, 2.0);
        L.l3 = lz_lvl(we, nb, 3u * sec - q, nwin, 2.0);
        L.l9 = lz_lvl(we, nb, 9u * sec - q, nwin, 2.0);
        for (k = 0; k < 10u; k++)
            v[k] = lz_lvl(we, nb, sec + k * nwin, nwin, 2.0);
        L.med16 = lz_median(v, 10u);
        for (s = 0; s + nwin <= nb; s++)
            if (lz_lvl(we, nb, s, nwin, 2.0) >= L.w15 - 3.0)
                break;
        L.t3 = (s + q) * (double)CTL / FS;
        for (i = 0; i < nb; i++) {
            tot_l += wl[i];
            tot_r += wr[i];
        }
        {
            const double *wc = tot_l >= tot_r ? wl : wr;
            for (k = 0; k < 10u; k++)
                vl[k] = lz_lvl(wc, nb, sec + k * nwin, nwin, 1.0);
            L.cmed = lz_median(vl, 10u);
            pl = lz_win(wc, nb, n15, &a15) * 2.0;     /* (lz_win divides by two channels) */
            pr = pl;
            L.cw15 = lz_db(pr);
        }
    }
    return L;
}

/* the render: set up part 1 (setup(arg): the sound, the morph), the notes held, measured; in a forked child. lz_spawn
 * starts one (several may run at once: the host's cores), lz_collect waits for it; lz_render does both */
typedef void (*lz_setup_t)(void *arg);
typedef struct { pid_t pid; int fd; } lz_job_t;
static lz_job_t lz_spawn(lz_setup_t setup, void *arg, const uint8_t *notes, uint32_t nn)
{
    lz_job_t j = {-1, -1};
    int fd[2];
    if (pipe(fd))
        return j;
    fflush(stdout);
    j.pid = fork();
    if (!j.pid) {
        static double we[LZ_NBMAX], wl[LZ_NBMAX], wr[LZ_NBMAX], re[LZ_NBMAX], de[LZ_NBMAX];
        int32_t out[2 * CTL];
        double hx[2] = {0}, hy[2] = {0}, ly[2] = {0}, px[2] = {0}, peak = 0;
        double ah = 1.0 / (1.0 + 2 * M_PI * LZ_HP_HZ / FS), bl = 1.0 - exp(-2 * M_PI * LZ_LP_HZ / FS);
        uint32_t b, i, c, off = (uint32_t)(lz_hold * FS) / CTL, nb = (uint32_t)((lz_hold + LZ_TAIL) * FS) / CTL, pk_b = 0;
        int nan = 0;
        lz_t L;
        close(fd[0]);
        setup(arg);
        for (i = 0; i < nn; i++)
            input_on(&trk[0], notes[i], 100);
        for (b = 0; b < nb; b++) {
            if (b == off)
                for (i = 0; i < nn; i++)
                    input_off(&trk[0], notes[i]);
            mix_block(out, CTL);
            we[b] = wl[b] = wr[b] = re[b] = de[b] = 0;
            for (i = 0; i < CTL; i++)
                for (c = 0; c < 2u; c++) {
                    double x = (double)((c ? mix_r[i] : mix_l[i]) + wet[i]), h;
                    if (x != x)
                        nan = 1;
                    h = ah * (hy[c] + x - hx[c]);    /* 1-pole high-pass */
                    hx[c] = x;
                    hy[c] = h;
                    ly[c] += bl * (h - ly[c]);       /* 1-pole low-pass */
                    we[b] += ly[c] * ly[c];
                    (c ? wr : wl)[b] += ly[c] * ly[c];
                    re[b] += x * x;
                    de[b] += (x - px[c]) * (x - px[c]);
                    px[c] = x;
                    if (fabs(x) > peak) {
                        peak = fabs(x);
                        pk_b = b;
                    }
                }
        }
        L = lz_measure(we, wl, wr, re, de, peak, nan, nb, off);
        L.peak_t = pk_b * (double)CTL / FS;
        if (write(fd[1], &L, sizeof L) != (ssize_t)sizeof L)
            _exit(2);
        _exit(0);
    }
    close(fd[1]);
    j.fd = fd[0];
    return j;
}
static lz_t lz_collect(lz_job_t j)
{
    lz_t L;
    int st = 0;
    memset(&L, 0, sizeof L);
    if (j.fd < 0 || read(j.fd, &L, sizeof L) != (ssize_t)sizeof L)
        L.nan = 1;
    if (j.fd >= 0)
        close(j.fd);
    if (j.pid > 0)
        waitpid(j.pid, &st, 0);
    if (st)
        L.nan = 1;
    return L;
}
static lz_t lz_render(lz_setup_t setup, void *arg, const uint8_t *notes, uint32_t nn)
{
    return lz_collect(lz_spawn(setup, arg, notes, nn));
}
