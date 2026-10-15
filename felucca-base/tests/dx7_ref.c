/* SPDX-License-Identifier: GPL-3.0-only */
/* A small independent float model of the DX7 voice core, for comparing the FM6 engine with (tests/dx7_compat.c renders the
 * engine's side). Own code, written for this test; it does not include or link any firmware file and shares no code with
 * fm6_core.c or Dexed. It reads the .syx itself (own parser), renders in double precision, per sample, and writes WAVs
 * (44.1 kHz mono 16 bit) in the engine's level scale (one carrier of amplitude 1.0 = 6000).
 *   dx7_ref OUTROOT FILE.syx ...      (same note rule, velocity 100, 1.5 s held, 2.0 s in all as dx7_compat)
 * WHAT IT MODELS: algorithms (graph from the editor's table: the wiring is NOT independently checked), ratio / fixed
 * frequency, fine, detune, the 4-stage amplitude EG (rate law, attack curve, flat-stage holds), rate scaling, level
 * scaling (4 curves), velocity sensitivity, feedback, the 7 Hz DC high-pass of the engine, key-up.
 * WHAT IT DOES NOT: the pitch EG, the LFO (pitch and amplitude modulation), amplitude-mod sensitivity, OKS. Voices that use
 * them are marked "uncovered" by the python side and kept out of the deviation statistics.
 * Constants that come from measurements of the real DX7 (the output-level curve below level 20, the EG rate law, the
 * velocity and exponential-scaling curves) are the published ones as I know them from the msfa/Dexed analyses; the
 * reference is therefore independent in CODE, not in DATA: it catches parse / wiring / integration errors, not a
 * wrong measured table shared by both. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define SR 44100.0
static const uint8_t ALGT[32][6] = {      /* from web/editor.html FM6.ALG: per operator (op6 first) in-bus, out-bus, add, feedback flags */
    {0xc1,0x11,0x11,0x14,0x01,0x14},{0x01,0x11,0x11,0x14,0xc1,0x14},{0xc1,0x11,0x14,0x01,0x11,0x14},{0xc1,0x11,0x94,0x01,0x11,0x14},
    {0xc1,0x14,0x01,0x14,0x01,0x14},{0xc1,0x94,0x01,0x14,0x01,0x14},{0xc1,0x11,0x05,0x14,0x01,0x14},{0x01,0x11,0xc5,0x14,0x01,0x14},
    {0x01,0x11,0x05,0x14,0xc1,0x14},{0x01,0x05,0x14,0xc1,0x11,0x14},{0xc1,0x05,0x14,0x01,0x11,0x14},{0x01,0x05,0x05,0x14,0xc1,0x14},
    {0xc1,0x05,0x05,0x14,0x01,0x14},{0xc1,0x05,0x11,0x14,0x01,0x14},{0x01,0x05,0x11,0x14,0xc1,0x14},{0xc1,0x11,0x02,0x25,0x05,0x14},
    {0x01,0x11,0x02,0x25,0xc5,0x14},{0x01,0x11,0x11,0xc5,0x05,0x14},{0xc1,0x14,0x14,0x01,0x11,0x14},{0x01,0x05,0x14,0xc1,0x14,0x14},
    {0x01,0x14,0x14,0xc1,0x14,0x14},{0xc1,0x14,0x14,0x14,0x01,0x14},{0xc1,0x14,0x14,0x01,0x14,0x04},{0xc1,0x14,0x14,0x14,0x04,0x04},
    {0xc1,0x14,0x14,0x04,0x04,0x04},{0xc1,0x05,0x14,0x01,0x14,0x04},{0x01,0x05,0x14,0xc1,0x14,0x04},{0x04,0xc1,0x11,0x14,0x01,0x14},
    {0xc1,0x14,0x01,0x14,0x04,0x04},{0x04,0xc1,0x11,0x14,0x04,0x04},{0xc1,0x14,0x04,0x04,0x04,0x04},{0xc4,0x04,0x04,0x04,0x04,0x04}};
static const int LVL20[20] = {0,5,9,13,17,20,23,25,27,29,31,33,35,37,39,41,42,43,45,46};   /* the DX7's output-level curve below 20 (measured) */
static int lvl_idx(int l) { return l >= 20 ? 28 + l : LVL20[l]; }     /* 1 step = 0.75 dB */
static const double EXPS[33] = {0,1,2,3,4,5,6,7,8,9,11,14,16,19,23,27,33,39,47,56,66,80,94,110,126,142,158,174,190,206,222,238,250};
static const int VELT[64] = {0,70,86,97,106,114,121,126,132,138,142,148,152,156,160,163,166,170,173,174,178,181,184,186,189,190,194,196,198,200,202,205,206,209,211,214,216,218,220,222,224,225,227,229,230,232,233,235,237,238,240,241,242,243,244,246,246,248,249,250,251,252,253,254};

typedef struct { int r[4], l[4], bp, ld, rd, lc, rc, rs, ams, kvs, ol, mode, fc, ff, det; } op_t;
typedef struct { op_t op[6]; int alg, fb, trnsp; char name[11]; } voice_t;

static void parse(const uint8_t *b, voice_t *v)       /* a 128-byte packed record (DX7 bulk format), op6 first */
{
    int k, i;
    for (k = 0; k < 6; k++) {
        const uint8_t *o = b + 17 * k;
        op_t *p = &v->op[k];
        for (i = 0; i < 4; i++) { p->r[i] = o[i] & 127; p->l[i] = o[4 + i] & 127; }
        p->bp = o[8] & 127; p->ld = o[9] & 127; p->rd = o[10] & 127;
        p->lc = o[11] & 3; p->rc = (o[11] >> 2) & 3;
        p->rs = o[12] & 7; p->det = (o[12] >> 3) & 15;
        p->ams = o[13] & 3; p->kvs = (o[13] >> 2) & 7;
        p->ol = o[14] & 127; p->mode = o[15] & 1; p->fc = (o[15] >> 1) & 31; p->ff = o[16] & 127;
        if (p->det > 14) p->det = 14;
        { int *q[] = {p->r, p->r+1, p->r+2, p->r+3, p->l, p->l+1, p->l+2, p->l+3}; for (i = 0; i < 8; i++) if (*q[i] > 99) *q[i] = 99; }
        if (p->ol > 99) p->ol = 99; if (p->ff > 99) p->ff = 99; if (p->bp > 99) p->bp = 99; if (p->ld > 99) p->ld = 99; if (p->rd > 99) p->rd = 99;
    }
    v->alg = b[110] & 31; v->fb = b[111] & 7; v->trnsp = b[117] & 127; if (v->trnsp > 48) v->trnsp = 48;
    for (i = 0; i < 10; i++) { int c = b[118 + i] & 127; v->name[i] = c < 32 || c > 126 ? ' ' : c; } v->name[10] = 0;
}

static double scale_steps(int note, const op_t *o)    /* level scaling, in 0.75 dB steps (+ = louder) */
{
    int off = note - o->bp - 17;
    int depth = off >= 0 ? o->rd : o->ld, curve = off >= 0 ? o->rc : o->lc;
    int g = off >= 0 ? (off + 1) / 3 : (-(off - 1)) / 3;       /* distance from the breakpoint in groups of 3 semitones */
    double s;
    if (curve == 0 || curve == 3) s = (double)g * depth * 329 / 4096;
    else s = EXPS[g > 32 ? 32 : g] * depth * 329 / 32768.0;
    return curve < 2 ? -s : s;
}

typedef struct { double lvl; int ix, down, rising; double target; double hold; int rscale; int lt[4]; } eg_t;

static void eg_stage(eg_t *e, const op_t *o, double outsteps, int ix)
{
    double act;
    int q, sr;
    e->ix = ix;
    if (ix >= 4) return;
    act = ((lvl_idx(o->l[ix]) >> 1) << 6) + outsteps * 32.0 - 4256;
    if (act < 16) act = 16;
    e->target = act / 256.0;                       /* octaves of amplitude (6 dB each) */
    e->rising = e->target > e->lvl;
    e->hold = 0;
    if (e->target == e->lvl || (ix == 0 && o->l[0] == 0)) {
        sr = o->r[ix] + e->rscale; if (sr > 99) sr = 99;
        e->hold = (sr < 77 ? 1764000.0 * pow(2.0, -sr / 6.5) : 20.0 * (99 - sr)) / (ix == 0 && o->l[0] == 0 && sr < 77 ? 20 : 1);
    }
    (void)q;
}

static void render(const voice_t *v, int note, int vel, double *out, int n, int hold)
{
    double ph[6] = {0}, y1[6] = {0}, y2[6] = {0}, fq[6], outsteps[6], gain[6];
    eg_t eg[6];
    double bus[3], f0, dc = 0, xl;
    int k, i, nt = note + v->trnsp - 24;
    if (nt < 0) nt = 0; if (nt > 127) nt = 127;
    f0 = 440.0 * pow(2.0, (nt - 69) / 12.0);
    for (k = 0; k < 6; k++) {
        const op_t *o = &v->op[k];
        double ol = lvl_idx(o->ol) + scale_steps(nt, o);
        double vs;
        int x = nt / 3 - 7;
        if (ol > 127) ol = 127;
        vs = (double)(((o->kvs * (VELT[(vel > 127 ? 127 : vel) >> 1] - 239) + 7) >> 3) << 4) / 32.0;   /* velocity term, in steps */
        outsteps[k] = ol + vs; if (outsteps[k] < 0) outsteps[k] = 0;
        x = x < 0 ? 0 : x > 31 ? 31 : x;
        memset(&eg[k], 0, sizeof eg[k]);
        eg[k].rscale = (o->rs * x) >> 3; eg[k].down = 1;
        eg_stage(&eg[k], o, outsteps[k], 0);
        if (!o->mode) fq[k] = f0 * (o->fc ? o->fc : 0.5) * (1 + o->ff / 100.0) * pow(2.0, (o->det - 7) * 0.55 / 1200.0);
        else fq[k] = pow(10.0, (o->fc & 3) + o->ff / 100.0);
        gain[k] = 0;
    }
    for (i = 0; i < n; i++) {
        int blk = i % 32 == 0;
        double sum = 0;
        if (i == hold) for (k = 0; k < 6; k++) { eg[k].down = 0; eg_stage(&eg[k], &v->op[k], outsteps[k], 3); }
        for (k = 0; k < 6; k++) {                 /* the EG moves once per 32 samples */
            eg_t *e = &eg[k];
            const op_t *o = &v->op[k];
            if (!blk) continue;
            if (e->hold > 0) { e->hold -= 32; if (e->hold <= 0) { e->hold = 0; eg_stage(e, o, outsteps[k], e->ix + 1); } }
            if (e->ix < 3 || (e->ix < 4 && !e->down)) {
                int qr = ((o->r[e->ix] * 41) >> 6) + e->rscale;
                double inc;
                if (qr > 63) qr = 63;
                inc = (double)((4 + (qr & 3)) << (7 + (qr >> 2))) / 16777216.0;
                if (e->hold > 0) continue;
                if (e->rising) {
                    if (e->lvl < 1716.0 * 65536 / 16777216) e->lvl = 1716.0 * 65536 / 16777216;
                    e->lvl += floor(17.0 - e->lvl) * inc;
                    if (e->lvl >= e->target) { e->lvl = e->target; eg_stage(e, o, outsteps[k], e->ix + 1); }
                } else {
                    e->lvl -= inc;
                    if (e->lvl <= e->target) { e->lvl = e->target; eg_stage(e, o, outsteps[k], e->ix + 1); }
                }
            }
            gain[k] = pow(2.0, e->lvl - 14.0);     /* cycles of phase deviation / carrier amplitude */
        }
        bus[0] = bus[1] = bus[2] = 0;
        {
            int has[3] = {0, 0, 0};
            for (k = 0; k < 6; k++) {
                int f = ALGT[v->alg][k], inb = (f >> 4) & 3, outb = f & 3, add = (f & 4) != 0;
                double mod = 0, y;
                if (gain[k] < 1e-7) { if (!add) has[outb] = 0; ph[k] += fq[k] / SR; ph[k] -= floor(ph[k]); continue; }
                if ((f & 0xc0) == 0xc0) mod = (y1[k] + y2[k]) * 0.5 * pow(2.0, v->fb - 8.0) * (v->fb ? 1 : 0);
                else if (inb && has[inb]) mod = bus[inb - 1];
                y = sin(2 * M_PI * (ph[k] + mod)) * gain[k];
                if ((f & 0xc0) == 0xc0) { y2[k] = y1[k]; y1[k] = y; }
                if (!has[outb]) add = 0;
                if (outb) { if (add) bus[outb - 1] += y; else bus[outb - 1] = y; } else sum += y;
                has[outb] = 1;
                ph[k] += fq[k] / SR; ph[k] -= floor(ph[k]);
            }
        }
        xl = sum * 6000.0;
        out[i] = xl - dc;
        dc += (xl - dc) * (1.0 / 1024);
    }
}

static void le32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
int main(int argc, char **argv)
{
    int a;
    static double y[88200 * 2];
    static int16_t pcm[88200 * 2];
    if (argc < 3) { printf("dx7_ref: skipped: no banks (set DX7_BANKS=<dir of .syx banks>, or use tests/dx7_compat.sh)\n"); return 0; }
    for (a = 2; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb");
        static uint8_t d[8192];
        char bank[128], dir[512], path[700];
        const char *b = strrchr(argv[a], '/');
        int k, i, n;
        if (!f) continue;
        n = fread(d, 1, sizeof d, f); fclose(f);
        if (n < 4104) continue;
        b = b ? b + 1 : argv[a];
        snprintf(bank, sizeof bank, "%s", b);
        if (strlen(bank) > 4) bank[strlen(bank) - 4] = 0;
        for (i = 0; bank[i]; i++) if (bank[i] == ' ') bank[i] = '_';
        snprintf(dir, sizeof dir, "%s/%s", argv[1], bank);
        snprintf(path, sizeof path, "mkdir -p '%s'", dir); if (system(path)) return 1;
        for (k = 0; k < 32; k++) {
            voice_t v;
            char lo[11], fn[800];
            int note, total = 88200, hold = 66150;
            uint8_t h[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,2,0,16,0,'d','a','t','a',0,0,0,0};
            FILE *w;
            parse(d + 6 + 128 * k, &v);
            { char nm[11]; strcpy(nm, v.name); for (i = 10; i && nm[i-1] == ' '; i--) nm[i-1] = 0;
              for (i = 0; i < 11; i++) lo[i] = nm[i] >= 'A' && nm[i] <= 'Z' ? nm[i] + 32 : nm[i];
              note = strstr(lo, "bass") || strstr(lo, "sub") || strstr(lo, "kick") ? 36 : strstr(lo, "low") || strstr(lo, "deep") ? 48 : 60;
              render(&v, note, 100, y, total, hold);
              for (i = 0; nm[i]; i++) if (!((nm[i]>='0'&&nm[i]<='9')||(nm[i]>='A'&&nm[i]<='Z')||(nm[i]>='a'&&nm[i]<='z'))) nm[i] = '_';
              if (!nm[0]) strcpy(nm, "VOICE");
              snprintf(fn, sizeof fn, "%s/%02d-%s.wav", dir, k + 1, nm); }
            for (i = 0; i < total; i++) { double s = y[i]; pcm[i] = s > 32767 ? 32767 : s < -32768 ? -32768 : (int16_t)lrint(s); }
            le32(h + 4, 36 + total * 2); le32(h + 24, 44100); le32(h + 28, 88200); le32(h + 40, total * 2);
            w = fopen(fn, "wb"); fwrite(h, 1, 44, w);
            for (i = 0; i < total; i++) { uint8_t q[2] = {pcm[i] & 255, (pcm[i] >> 8) & 255}; fwrite(q, 1, 2, w); }
            fclose(w);
        }
    }
    return 0;
}
