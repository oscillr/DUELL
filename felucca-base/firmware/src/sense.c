/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* KEY SENSE core (DUELL_VELO): the per-key state machine, velocity, aftertouch, calibration. Plain C, no hardware calls
 * (API and data flow: sense.h; every tunable: sense_cal.h).
 *
 * After Velo's method (Charles Vestal, https://github.com/charlesvestal/fm1-velocity, GPL-3.0-only): a key's carbon pill
 * is read by the converter, with the row pulled up (undriven: open ~1023, seated 170 .. 360, the same for a soft and a
 * hard press) or with the row driven (the reading shows the pill's resistance: harder press, lower reading). Written
 * here from the method and its published measurements, no code taken.
 *
 * Per key (undriven readings carry the state, driven readings the strength):
 *   OPEN   --reading < SEATED-->  DOWN (the landing: t_land, the landing depth)
 *   OPEN   --reading < OPEN_MIN-->  TOUCH (the pill is touching, not seated); TOUCH --seated--> DOWN, --open--> OPEN
 *   DOWN   --open reading-->  LIFT;  LIFT --open reading again (SC_REL_N in a row)--> OPEN (the release);
 *                                    LIFT --a closed reading--> DOWN (a landed key can read open for one sample)
 * After the landing the next reading is driven. The lowest driven reading of the first SC_WAIT of them is the landing's
 * strength: velocity = VMIN + (VMAX - VMIN) x u^e, u = 1 at the key's floor (hardest), 0 at its ceiling, on a log scale of
 * the resistance (reading / (1024 - reading)). While the key is held every second reading is driven: the aftertouch is
 * how far below the landing reading it goes (SC_AT_SPAN counts = 127).
 * Floor and ceiling are learned per key from the landings (sense_cal.h): a running low and high quantile that move both
 * ways, margins outside them so the extremes stay rare, clamps, and a reset.
 * The gap guard: a sample later than SC_GAP_US after the key's last one (an interrupts-off window: a flash erase is ~40 ms,
 * and the converter reading in flight is lost) means the time between is UNKNOWN. A key that was held stays held (a gap is
 * never a release, and the open samples of before and after are not "in a row"); a landing that was waiting for its
 * strength gets the fixed velocity; a key seen seated after a gap has an unknown landing (the fixed velocity, no learning).
 *
 * The velocity of a landing reaches the notes through sense_gate (the event step asks once the digital scan has closed the
 * key: the note starts when the velocity is ready, or SC_GATE_MAX_US at the latest). Notes landing within SC_CHORD_US of
 * the first take its velocity. */
#include <stdint.h>
#include "sense.h"

enum { KS_OPEN, KS_TOUCH, KS_DOWN, KS_LIFT };
enum { K_PEND = 1, K_FWAIT = 2, K_UNK = 4, K_ALT = 8, K_BYDRV = 16, K_SEEN = 32 };   /* (the tick's flags: the event step never writes them) */

typedef struct {
    volatile uint8_t fl;             /* K_*: written by the tick only */
    volatile uint8_t vel;            /* the last published landing's velocity: written before rseq counts it */
    volatile uint8_t rseq;           /* counts the landings published (the event step compares it with what it has taken) */
    volatile uint8_t lcount, plc;    /* counts the releases; its value when the last landing was published (a landing the key has been let go since is stale) */
    volatile uint8_t at;             /* aftertouch 0 .. 127 */
    uint8_t st, nopen, nd, cnt, follow, hold;
    uint16_t u_land, d_best, d_ref, d_s;   /* d_s: the smoothed driven reading, counts x 4 (aftertouch) */
    uint16_t lo6, hi6;               /* the learned low and high quantile of the driven landings, counts x 64 (a new key: its min and max) */
    uint32_t t_last, t_land;
} sk_t;

#define SC_RING_N 16u
static struct {
    uint8_t on, at, curve1, walking, walk_done, ch_valid, ch_ready, ch_key, ch_vel;   /* curve1: the MENU curve + 1, 0 = SC_CURVE_DEFAULT (so the state is all zero: .bss, no flash image); on: the setting KEY VELOCITY is ON (0 = OFF, the default since 0.2.31: owner 2026-10-21, pressing a key hard made it very quiet; to be measured); at: the setting AFTERTOUCH is ON (DUELL 1.0, its own line: 0 = OFF, the default; the key sensing runs when either is ON, but the velocity gate and the velocity itself only with KEY VELOCITY, the channel pressure only with AFTERTOUCH) */
    /* The event step's own (written by sense_gate only; the tick never writes them, the event step never writes the tick's: no lock between the two
     * interrupts): the note bits it holds back for their velocity, the bits it has asked about, when it first asked, the landings it has taken */
    uint32_t defer, asked, g_ask[SC_NKEY];
    uint8_t g_seq[SC_NKEY];
    uint32_t now, ch_t0;
    uint32_t moved6, last_save_ms, last_save_ok, force_save;
    uint8_t vo[SC_NKEY];             /* the velocity the event step took for the note that rose */
    uint8_t rd, wr;
    sense_ev_t ring[SC_RING_N];
    sense_stat_t st;
    struct { uint8_t key, soft, hard; uint16_t dmin, dmax; } w;
    sk_t k[SC_NKEY];
} sn;

/* ---- fixed point: log2 and 2^-x (tables of 32 steps, linear between) ---- */
static const uint16_t SC_LOG2[33] = {0, 11, 22, 33, 44, 54, 63, 73, 82, 92, 100, 109, 118, 126, 134, 142, 150,
                                     157, 165, 172, 179, 186, 193, 200, 207, 213, 220, 226, 232, 238, 244, 250, 256};
static const uint16_t SC_EXP2N[33] = {65535, 64132, 62757, 61413, 60097, 58809, 57549, 56316, 55109, 53928, 52773, 51642, 50535, 49452,
                                      48393, 47356, 46341, 45348, 44376, 43425, 42495, 41584, 40693, 39821, 38968, 38133, 37316, 36516,
                                      35734, 34968, 34219, 33486, 32768};

static uint32_t sc_log2q8(uint32_t x)            /* floor(256 log2 x), x >= 1 */
{
    uint32_t n = 31u, m, i, r, a;
    while (!(x >> n))
        n--;
    m = n ? (x << (32u - n)) : 0u;               /* the bits below the leading one, left aligned */
    i = m >> 27;
    r = (m >> 11) & 0xFFFFu;
    a = SC_LOG2[i];
    return n * 256u + a + (((uint32_t)SC_LOG2[i + 1u] - a) * r >> 16);
}
static uint32_t sc_res_log(uint32_t d)           /* log2 of the pill's resistance ~ d / (1024 - d), Q8 (an offset is fine: only differences are used) */
{
    d = d < 1u ? 1u : d > 1022u ? 1022u : d;
    return sc_log2q8(d * 4096u / (1024u - d));
}
static uint32_t sc_pow12(uint32_t u12, uint32_t e_q8)   /* (u12 / 4096)^(e_q8 / 256) x 4096 */
{
    uint32_t q, ip, fp, i, a, b, v;
    if (u12 == 0u)
        return 0;
    if (u12 >= 4096u)
        return 4096u;
    if (e_q8 == 256u)
        return u12;
    q = (3072u - sc_log2q8(u12)) * e_q8 >> 8;    /* -log2(u) x e, Q8: 3072 = 12 x 256 */
    ip = q >> 8;
    if (ip > 15u)
        return 0;
    fp = q & 255u;
    i = fp >> 3;
    a = SC_EXP2N[i];
    b = SC_EXP2N[i + 1u];
    v = a - ((a - b) * (fp & 7u) >> 3);
    return (v >> (4u + ip)) + ((v >> (3u + ip)) & 1u);
}

/* ---- events ---- */
static void sc_ev_push(uint32_t type, uint32_t key, uint32_t a, uint32_t b, uint32_t t)
{
    sense_ev_t *e = &sn.ring[sn.wr % SC_RING_N];
    e->type = (uint8_t)type;
    e->key = (uint8_t)key;
    e->a = (uint8_t)a;
    e->b = (uint8_t)b;
    e->t = t;
    sn.wr++;
    if ((uint8_t)(sn.wr - sn.rd) > SC_RING_N)
        sn.rd = (uint8_t)(sn.wr - SC_RING_N);
}
uint32_t sense_event_pop(sense_ev_t *e)
{
    if (sn.rd == sn.wr)
        return 0;
    *e = sn.ring[sn.rd % SC_RING_N];
    sn.rd++;
    return 1;
}
void sense_stat_get(sense_stat_t *s) { *s = sn.st; }
uint32_t sense_state_bytes(void) { return (uint32_t)sizeof sn; }

/* ---- floor and ceiling of a key (counts), derived from its learned range ---- */
static void sk_range(const sk_t *k, int32_t *f6, int32_t *c6)
{
    int32_t lo = k->lo6, hi = k->hi6, span, m, mid;
    if (!k->cnt) {                               /* never played: the typical key */
        lo = (int32_t)SC_FLOOR_PRIOR * 64;
        hi = (int32_t)SC_CEIL_PRIOR * 64;
    } else if (k->cnt < SC_FAST_N) {             /* new: the min and max so far, widened to the typical span around them */
        int32_t want = ((int32_t)SC_CEIL_PRIOR - (int32_t)SC_FLOOR_PRIOR) * 64;
        if (hi - lo < want) {
            int32_t ext = want - (hi - lo);
            lo -= ext / 2;
            hi += ext - ext / 2;
        }
    }
    span = hi - lo;
    m = span * (int32_t)SC_MARGIN_PCT / 100;
    lo -= m;
    hi += m;
    span = hi - lo;
    mid = (hi + lo) / 2;
    if (span < (int32_t)SC_SPAN_MIN * 64) {
        lo = mid - (int32_t)SC_SPAN_MIN * 32;
        hi = lo + (int32_t)SC_SPAN_MIN * 64;
    } else if (span > (int32_t)SC_SPAN_MAX * 64) {
        lo = mid - (int32_t)SC_SPAN_MAX * 32;
        hi = lo + (int32_t)SC_SPAN_MAX * 64;
    }
    if (lo < (int32_t)SC_FLOOR_MIN * 64)
        lo = (int32_t)SC_FLOOR_MIN * 64;
    if (lo > (int32_t)SC_FLOOR_MAX * 64)
        lo = (int32_t)SC_FLOOR_MAX * 64;
    if (hi < (int32_t)SC_CEIL_MIN * 64)
        hi = (int32_t)SC_CEIL_MIN * 64;
    if (hi > (int32_t)SC_CEIL_MAX * 64)
        hi = (int32_t)SC_CEIL_MAX * 64;
    if (hi < lo + (int32_t)SC_SPAN_MIN * 64)
        hi = lo + (int32_t)SC_SPAN_MIN * 64;
    *f6 = lo;
    *c6 = hi;
}
void sense_cal_get(uint32_t note, uint32_t *floor, uint32_t *ceil)
{
    int32_t f, c;
    if (note >= SC_NKEY)
        return;
    sk_range(&sn.k[note], &f, &c);
    *floor = (uint32_t)(f + 32) >> 6;
    *ceil = (uint32_t)(c + 32) >> 6;
}

/* ---- velocity ---- */
static uint32_t sk_map(const sk_t *k, uint32_t d)    /* a driven landing reading -> 1 .. 127 */
{
    int32_t f6, c6, lf, lc, ld, u;
    uint32_t f, c, r;
    sk_range(k, &f6, &c6);
    f = (uint32_t)(f6 + 32) >> 6;
    c = (uint32_t)(c6 + 32) >> 6;
    lf = (int32_t)sc_res_log(f);
    lc = (int32_t)sc_res_log(c);
    ld = (int32_t)sc_res_log(d);
    if (lc <= lf)
        return SC_VEL_UNKNOWN;
    u = (lc - ld) * 4096 / (lc - lf);            /* 4096 at the floor (hardest), 0 at the ceiling */
    u = u < 0 ? 0 : u > 4096 ? 4096 : u;
    r = sc_pow12((uint32_t)u, SC_EXP_Q8);
    return SC_VEL_MIN + (((SC_VEL_MAX - SC_VEL_MIN) * r + 2048u) >> 12);
}
static uint32_t sk_depth_vel(uint32_t u)         /* the first estimate: the undriven landing depth */
{
    int32_t v;
    v = (int32_t)SC_VEL_MIN + ((int32_t)SC_LAND_SOFT - (int32_t)u) * (int32_t)(SC_VEL_MAX - SC_VEL_MIN) / (int32_t)(SC_LAND_SOFT - SC_LAND_HARD);
    return (uint32_t)(v < (int32_t)SC_VEL_MIN ? (int32_t)SC_VEL_MIN : v > (int32_t)SC_VEL_MAX ? (int32_t)SC_VEL_MAX : v);
}
static uint32_t sc_curve(uint32_t v)          /* MENU curve: flat 100 .. the measured range */
{
    int32_t x = 100 + ((int32_t)v - 100) * (int32_t)sense_curve_get() / 100;
    return (uint32_t)(x < 1 ? 1 : x > 127 ? 127 : x);
}

/* ---- calibration ---- */
static void sk_moved(uint32_t a, uint32_t b)
{
    sn.moved6 += a > b ? a - b : b - a;
}
static void sk_cal(sk_t *k, uint32_t d)
{
    uint32_t x = d * 64u, eta;
    uint32_t lo = k->lo6, hi = k->hi6, nlo, nhi;
    if (!k->cnt) {                               /* the first landing */
        k->lo6 = k->hi6 = (uint16_t)x;
        k->cnt = 1;
        sn.moved6 += 64u * 8u;
        return;
    }
    if (k->cnt < SC_FAST_N) {                    /* a new key: min and max so far */
        nlo = x < lo ? x : lo;
        nhi = x > hi ? x : hi;
        k->cnt++;
        if (k->cnt == SC_FAST_N) {               /* seasoned: from here a running quantile, started on the widened range */
            int32_t want = ((int32_t)SC_CEIL_PRIOR - (int32_t)SC_FLOOR_PRIOR) * 64;
            if ((int32_t)(nhi - nlo) < want) {
                int32_t ext = want - (int32_t)(nhi - nlo);
                nlo -= (uint32_t)(ext / 2);
                nhi += (uint32_t)(ext - ext / 2);
            }
        }
    } else {
        eta = SC_ETA_Q6;
        nlo = x < lo ? lo - eta * (1000u - SC_Q_LO) / 1000u : lo + eta * SC_Q_LO / 1000u;
        nhi = x > hi ? hi + eta * (1000u - SC_Q_HI) / 1000u : hi - eta * SC_Q_HI / 1000u;
        if (k->cnt < 255u)
            k->cnt++;
    }
    if ((int32_t)nlo < (int32_t)(SC_D_MIN * 64u))
        nlo = SC_D_MIN * 64u;
    if (nhi > SC_D_MAX * 64u)
        nhi = SC_D_MAX * 64u;
    if (nhi < nlo + 8u * 64u)
        nhi = nlo + 8u * 64u;
    sk_moved(lo, nlo);
    sk_moved(hi, nhi);
    k->lo6 = (uint16_t)nlo;
    k->hi6 = (uint16_t)nhi;
}

/* ---- the calibration walk ---- */
static void sk_walk_press(sk_t *k, uint32_t d)
{
    if (sn.w.soft < SC_WALK_SOFT)
        sn.w.soft++;
    else if (sn.w.hard < SC_WALK_HARD)
        sn.w.hard++;
    if (d < sn.w.dmin)
        sn.w.dmin = (uint16_t)d;
    if (d > sn.w.dmax)
        sn.w.dmax = (uint16_t)d;
    if (sn.w.soft >= SC_WALK_SOFT && sn.w.hard >= SC_WALK_HARD) {
        uint32_t lo = sn.w.dmin * 64u, hi = sn.w.dmax * 64u;
        if (hi < lo + 24u * 64u)
            hi = lo + 24u * 64u;
        k->lo6 = (uint16_t)lo;
        k->hi6 = (uint16_t)hi;
        k->cnt = SC_FAST_N;
        sn.w.key++;
        sn.w.soft = sn.w.hard = 0;
        sn.w.dmin = 0xFFFFu;
        sn.w.dmax = 0;
        if (sn.w.key >= SC_NKEY) {
            sn.walking = 0;
            sn.walk_done = 1;
            sn.force_save = 1;
        }
    }
}
void sense_walk_start(void)
{
    sn.walking = 1;
    sn.walk_done = 0;
    sn.w.key = sn.w.soft = sn.w.hard = 0;
    sn.w.dmin = 0xFFFFu;
    sn.w.dmax = 0;
}
int32_t sense_walk_step(uint32_t *soft, uint32_t *hard)
{
    if (!sn.walking)
        return -1;
    if (soft)
        *soft = sn.w.soft;
    if (hard)
        *hard = sn.w.hard;
    return (int32_t)sn.w.key;
}
uint32_t sense_walk_done(void) { return sn.walk_done; }
void sense_walk_cancel(void) { sn.walking = 0; }

/* ---- landing and its velocity ---- */
static void sk_publish(sk_t *k, uint32_t key, uint32_t vel, uint32_t flags, uint32_t t)
{
    k->vel = (uint8_t)vel;                       /* (the value first, then the flag the event step looks at) */
    k->fl = (uint8_t)((k->fl & ~(K_PEND | K_FWAIT | K_UNK | K_BYDRV)) | flags);
    k->plc = k->lcount;
    k->rseq++;                                   /* (last: the event step looks at this) */
    sc_ev_push(SE_LAND, key, vel, (flags & K_BYDRV) ? 1u : 0u, t);
    sn.st.lands++;
    if (flags & K_BYDRV)
        sn.st.lands_driven++;
    if (flags & K_UNK)
        sn.st.unknown++;
}
static void sk_release_followers(uint32_t leader, uint32_t t)
{
    uint32_t j;
    for (j = 0; j < SC_NKEY; j++) {
        sk_t *f = &sn.k[j];
        if ((f->fl & K_FWAIT) && f->follow == leader + 1u) {
            f->follow = 0;
            sk_publish(f, j, sn.ch_vel, (f->fl & K_UNK) ? K_UNK : 0u, t);
        }
    }
}
/* the landing's velocity is known (vel; flags K_BYDRV / K_UNK): a chord follower takes its leader's */
static void sk_finish(sk_t *k, uint32_t key, uint32_t vel, uint32_t flags, uint32_t t)
{
    if (k->follow && sn.ch_valid && sn.ch_key + 1u == k->follow) {
        if (sn.ch_ready) {
            k->follow = 0;
            sk_publish(k, key, sn.ch_vel, flags & K_UNK, t);
        } else {                                 /* the leader is not in yet: wait for it (its own wait is bounded) */
            k->vel = (uint8_t)vel;
            k->fl = (uint8_t)((k->fl & ~(K_PEND | K_UNK | K_BYDRV)) | K_FWAIT | (flags & (K_UNK | K_BYDRV)));
        }
        return;
    }
    if (k->follow) {                             /* its leader has been replaced: on its own */
        k->follow = 0;
    } else if (sn.ch_valid && sn.ch_key == key) {
        sn.ch_vel = (uint8_t)vel;
        sn.ch_ready = 1;
        sk_publish(k, key, vel, flags, t);
        sk_release_followers(key, t);
        return;
    }
    sk_publish(k, key, vel, flags, t);
}
static void sk_final_driven(sk_t *k, uint32_t key, uint32_t t)
{
    uint32_t d = k->d_best, vel;
    if (sn.walking) {
        if (key == sn.w.key)
            sk_walk_press(k, d);
    } else {
        sk_cal(k, d);
    }
    vel = sk_map(k, d);
    sk_finish(k, key, vel, K_BYDRV, t);
}
static void sk_final_unknown(sk_t *k, uint32_t key, uint32_t t)
{
    sk_finish(k, key, SC_VEL_UNKNOWN, K_UNK, t);
}
static void sk_final_depth(sk_t *k, uint32_t key, uint32_t t)   /* the first estimate, from the landing depth */
{
    sk_finish(k, key, sk_depth_vel(k->u_land), 0u, t);
}
static void sk_land(sk_t *k, uint32_t key, uint32_t u, uint32_t t, uint32_t after_gap)
{
    k->st = KS_DOWN;
    k->nopen = 0;
    k->nd = 0;
    k->hold = 0;
    k->at = 0;
    k->d_ref = 0;
    k->d_best = 0xFFFFu;
    k->u_land = (uint16_t)u;
    k->t_land = t;
    k->follow = 0;
    k->fl = (uint8_t)((k->fl & K_SEEN) | K_PEND);
    if (!after_gap && sn.ch_valid && (uint32_t)(t - sn.ch_t0) <= SC_CHORD_US && sn.ch_key != key) {
        k->follow = (uint8_t)(sn.ch_key + 1u);    /* inside the first note's window */
    } else {
        sn.ch_valid = 1;
        sn.ch_key = (uint8_t)key;
        sn.ch_t0 = t;
        sn.ch_ready = 0;
    }
    if (after_gap)
        sk_final_unknown(k, key, t);             /* the landing happened in the gap: its strength is unknown */
    else if (SC_WAIT == 0u)
        sk_final_depth(k, key, t);
}
static void sk_lift(sk_t *k, uint32_t key, uint32_t t)
{
    k->lcount++;                                 /* (before the forced landing below: that one is the press that just ended, not stale) */
    if (k->fl & (K_PEND | K_FWAIT)) {
        k->follow = 0;
        sk_final_depth(k, key, t);               /* let go before the strength was in: the first estimate */
    }
    k->st = KS_OPEN;
    k->nopen = 0;
    k->at = 0;
    k->d_ref = 0;
    k->fl &= (uint8_t)~(K_ALT | K_PEND | K_FWAIT);
    sn.st.lifts++;
    sc_ev_push(SE_LIFT, key, 0, 0, t);
}
static void sk_gap(sk_t *k, uint32_t key, uint32_t t)
{
    sn.st.gaps++;
    sc_ev_push(SE_GAP, key, 0, 0, t);
    switch (k->st) {
    case KS_DOWN:
    case KS_LIFT:
        k->st = KS_DOWN;                         /* held stays held: not a release */
        k->nopen = 0;                            /* .. and the open samples before and after are not in a row */
        if (k->fl & (K_PEND | K_FWAIT)) {        /* the strength waited for is lost */
            k->follow = 0;
            sk_final_unknown(k, key, t);
        }
        k->d_ref = 0;                            /* the aftertouch reference starts again */
        k->hold = 0;
        break;
    case KS_TOUCH:
        k->st = KS_OPEN;
        break;
    default:
        break;
    }
}
static void sk_driven(sk_t *k, uint32_t key, uint32_t d, uint32_t t)
{
    if (k->fl & K_PEND) {
        if ((uint32_t)(t - k->t_land) > SC_VEL_WIN_US) {         /* too late to be the landing's (a bounce or a lost slot): the landing depth, the first estimate */
            sk_final_depth(k, key, t);
            return;
        }
        if (d < SC_D_MIN || d > SC_D_MAX) {
            sn.st.rejected++;
            return;
        }
        k->nd++;
        if (d < k->d_best)
            k->d_best = (uint16_t)d;
        if (k->nd == 1u) {
            k->d_ref = (uint16_t)d;
            k->d_s = (uint16_t)(d * 4u);
        }
        if (k->nd >= SC_WAIT)
            sk_final_driven(k, key, t);
        return;
    }
    if (d < SC_D_MIN || d > SC_D_MAX)
        return;
    {                                            /* held: the aftertouch, from the driven reading smoothed (noise would flutter it) */
        uint32_t v, ds;
        if (!k->d_ref)
            k->d_s = (uint16_t)(d * 4u);
        else
            k->d_s = (uint16_t)((int32_t)k->d_s + (((int32_t)(d * 4u) - (int32_t)k->d_s) >> SC_AT_SMOOTH));
        ds = ((uint32_t)k->d_s + 2u) >> 2;
        if (!k->d_ref || ds > k->d_ref) {
            k->d_ref = (uint16_t)ds;             /* the landing reading, or the key relaxing: zero */
        } else if (++k->hold >= SC_AT_LEAK_N) {
            k->hold = 0;
            k->d_ref--;                          /* a pressure held creeps into the reference */
        }
        v = k->d_ref - ds;
        v = v > SC_AT_FLOOR ? (v - SC_AT_FLOOR) * 127u / (SC_AT_SPAN - SC_AT_FLOOR) : 0u;
        v = v > 127u ? 127u : v;
        if ((v > k->at ? v - k->at : k->at - v) >= 8u)
            sc_ev_push(SE_AT, key, v, 0, t);
        k->at = (uint8_t)v;
    }
}

uint32_t sense_want_drive(uint32_t note)
{
    sk_t *k;
    if (!(sn.on | sn.at) || note >= SC_NKEY)
        return 0;
    k = &sn.k[note];
    if (k->st != KS_DOWN)
        return 0;
    if (k->fl & K_PEND)
        return 1;                                /* just landed: the strength */
    return (k->fl & K_ALT) ? 0u : 1u;            /* then every second reading */
}
void sense_missed(uint32_t note) { (void)note; }

void sense_update(uint32_t note, uint32_t adc, uint32_t driven, uint32_t t)
{
    sk_t *k;
    uint32_t gap, open;
    if (!(sn.on | sn.at) || note >= SC_NKEY)
        return;
    k = &sn.k[note];
    sn.now = t;
    adc = adc > 1023u ? 1023u : adc;
    gap = (k->fl & K_SEEN) && (uint32_t)(t - k->t_last) > SC_GAP_US;
    k->t_last = t;
    k->fl |= K_SEEN;
    if (gap)
        sk_gap(k, note, t);
    open = adc >= SC_OPEN_MIN;
    switch (k->st) {
    case KS_OPEN:
        if (driven)
            break;
        if (adc < SC_SEATED_MAX)
            sk_land(k, note, adc, t, gap);
        else if (!open)
            k->st = KS_TOUCH;
        break;
    case KS_TOUCH:
        if (driven)
            break;
        if (adc < SC_SEATED_MAX)
            sk_land(k, note, adc, t, gap);
        else if (open)
            k->st = KS_OPEN;
        break;
    default:                                     /* DOWN, LIFT */
        if (open) {
            if (++k->nopen >= SC_REL_N)
                sk_lift(k, note, t);
            else
                k->st = KS_LIFT;
        } else {
            if (k->st == KS_LIFT) {
                k->st = KS_DOWN;                 /* it bounced */
                sn.st.bounces++;
            }
            k->nopen = 0;
            if (driven)
                sk_driven(k, note, adc, t);
        }
        if (k->st == KS_DOWN || k->st == KS_LIFT) {
            k->fl = (uint8_t)((k->fl & ~K_ALT) | (driven ? K_ALT : 0u));
            if ((k->fl & K_PEND) && (uint32_t)(t - k->t_land) > SC_WAIT_US)
                sk_final_depth(k, note, t);      /* the strength did not come in time: the first estimate */
        }
        break;
    }
}

/* ---- the event step ---- */
/* a closed key's landing, if one is in: the velocity is the key's last published landing, not yet taken and not stale (the key let go since: a
 * landing the digital scan never confirmed). Past SC_GATE_MAX_US from the first ask: the fixed velocity. */
static uint32_t sk_taken(const sk_t *k, uint32_t note, uint32_t *vel)
{
    uint32_t b = 1u << note, e = k->rseq;
    if (!(sn.asked & b)) {
        sn.asked |= b;
        sn.g_ask[note] = sn.now;
    }
    if (e != sn.g_seq[note] && k->plc == k->lcount) {
        *vel = k->vel;
        sn.g_seq[note] = (uint8_t)e;
        sn.asked &= ~b;
        return 1;
    }
    if ((uint32_t)(sn.now - sn.g_ask[note]) > SC_GATE_MAX_US) {      /* never came: the fixed velocity */
        *vel = SC_VEL_UNKNOWN;
        sn.g_seq[note] = (uint8_t)e;
        sn.asked &= ~b;
        return 1;
    }
    return 0;
}
uint32_t sense_gate(uint32_t cur, uint32_t prev)
{
    uint32_t n, v;
    if (!sn.on)
        return cur;
    for (n = 0; n < SC_NKEY; n++) {
        uint32_t b = 1u << n;
        sk_t *k = &sn.k[n];
        if ((cur & b) && !(prev & b)) {          /* a key the digital scan has closed */
            if (sk_taken(k, n, &v)) {
                sn.vo[n] = (uint8_t)sc_curve(v);
                sn.defer &= ~b;
            } else {
                sn.defer |= b;
                cur &= ~b;
            }
        } else if (!(cur & b) && (sn.defer & b)) {   /* let go while it waited: it plays for this block, the release follows */
            sn.defer &= ~b;
            if (!sk_taken(k, n, &v))
                v = SC_VEL_UNKNOWN;
            sn.asked &= ~b;
            sn.vo[n] = (uint8_t)sc_curve(v);
            cur |= b;
        }
    }
    return cur;
}
uint32_t sense_vel(uint32_t note)
{
    if (!sn.on)
        return 100u;
    return note < SC_NKEY ? sn.vo[note] : SC_TOP_VEL;
}
uint32_t sense_at_value(void)
{
    uint32_t n, m = 0;
    if (!sn.at)
        return 0;
    for (n = 0; n < SC_NKEY; n++)
        if (sn.k[n].st == KS_DOWN && sn.k[n].at > m)
            m = sn.k[n].at;
    return m;
}

/* ---- settings ---- */
static void sc_clear_keys(void)
{
    uint32_t n;
    for (n = 0; n < SC_NKEY; n++) {
        sk_t *k = &sn.k[n];
        k->fl = 0;
        k->st = KS_OPEN;
        k->nopen = k->nd = k->follow = k->hold = 0;
        k->at = 0;
        k->d_ref = 0;
        sn.vo[n] = 100;
    }
    sn.defer = sn.asked = 0;
    for (n = 0; n < SC_NKEY; n++)
        sn.g_seq[n] = sn.k[n].rseq;
    sn.ch_valid = sn.ch_ready = 0;
    sn.rd = sn.wr = 0;
}
void sense_enable(uint32_t on)
{
    on = on != 0u;                               /* (the stored form: on) */
    if (on != sn.on) {
        sn.on = (uint8_t)on;
        sc_clear_keys();
    }
}
uint32_t sense_enabled(void) { return sn.on; }
void sense_at_enable(uint32_t on)                /* DUELL 1.0: AFTERTOUCH on its own line; the keys' states restart whenever the setting changes */
{
    on = on != 0u;
    if (on != sn.at) {
        sn.at = (uint8_t)on;
        sc_clear_keys();
    }
}
uint32_t sense_at_enabled(void) { return sn.at; }
uint32_t sense_running(void) { return (uint32_t)(sn.on | sn.at); }   /* the key sensing is running (the budget's 2 %) */
void sense_curve_set(uint32_t pct) { sn.curve1 = (uint8_t)((pct > 100u ? 100u : pct) + 1u); }
uint32_t sense_curve_get(void) { return sn.curve1 ? sn.curve1 - 1u : SC_CURVE_DEFAULT; }
void sense_reset(void)
{
    uint32_t n;
    sc_clear_keys();
    for (n = 0; n < SC_NKEY; n++) {
        sn.k[n].cnt = 0;
        sn.k[n].lo6 = sn.k[n].hi6 = 0;
    }
    sn.walking = sn.walk_done = 0;
    sn.moved6 = 0;
    sn.force_save = 0;
    sn.st = (sense_stat_t){0};
}

/* ---- the stored calibration: 'V' 'C' version nkey, (lo, hi) x 20 as little endian words, CRC-16 ---- */
static uint32_t sc_crc16(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFu, i, j;
    for (i = 0; i < n; i++) {
        c ^= (uint32_t)p[i] << 8;
        for (j = 0; j < 8u; j++)
            c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
    }
    return c & 0xFFFFu;
}
uint32_t sense_cal_export(uint8_t *buf)
{
    uint32_t n, o = 4u, c;
    buf[0] = 'V';
    buf[1] = 'C';
    buf[2] = SC_CAL_VERSION;
    buf[3] = SC_NKEY;
    for (n = 0; n < SC_NKEY; n++) {
        const sk_t *k = &sn.k[n];
        uint32_t lo = k->cnt ? k->lo6 : 0u, hi = k->cnt ? k->hi6 : 0u;
        if (k->cnt && k->cnt < SC_FAST_N) {      /* a new key: the widened range it is mapped by */
            int32_t want = ((int32_t)SC_CEIL_PRIOR - (int32_t)SC_FLOOR_PRIOR) * 64, w = (int32_t)(hi - lo);
            if (w < want) {
                lo -= (uint32_t)((want - w) / 2);
                hi += (uint32_t)((want - w) - (want - w) / 2);
            }
        }
        buf[o++] = (uint8_t)lo;
        buf[o++] = (uint8_t)(lo >> 8);
        buf[o++] = (uint8_t)hi;
        buf[o++] = (uint8_t)(hi >> 8);
    }
    c = sc_crc16(buf, o);
    buf[o++] = (uint8_t)c;
    buf[o++] = (uint8_t)(c >> 8);
    return o;
}
uint32_t sense_cal_import(const uint8_t *buf)
{
    uint32_t n, o = 4u, c;
    if (buf[0] != 'V' || buf[1] != 'C' || buf[2] != SC_CAL_VERSION || buf[3] != SC_NKEY)
        return 0;
    c = sc_crc16(buf, 4u + 4u * SC_NKEY);
    if (buf[4u + 4u * SC_NKEY] != (uint8_t)c || buf[5u + 4u * SC_NKEY] != (uint8_t)(c >> 8))
        return 0;
    for (n = 0; n < SC_NKEY; n++, o += 4u) {
        uint32_t lo = buf[o] | (uint32_t)buf[o + 1u] << 8, hi = buf[o + 2u] | (uint32_t)buf[o + 3u] << 8;
        sk_t *k = &sn.k[n];
        if (!lo && !hi) {
            k->cnt = 0;
            k->lo6 = k->hi6 = 0;
            continue;
        }
        lo = lo < SC_D_MIN * 64u ? SC_D_MIN * 64u : lo > SC_D_MAX * 64u ? SC_D_MAX * 64u : lo;
        hi = hi > SC_D_MAX * 64u ? SC_D_MAX * 64u : hi;
        if (hi < lo + 8u * 64u)
            hi = lo + 8u * 64u;
        k->lo6 = (uint16_t)lo;
        k->hi6 = (uint16_t)hi;
        k->cnt = SC_FAST_N;
    }
    return 1;
}
void sense_cal_restore(void)
{
    uint8_t buf[SENSE_CAL_BYTES];
    if (velo_cal_load(buf, SENSE_CAL_BYTES) >= (int)SENSE_CAL_BYTES)
        (void)sense_cal_import(buf);
}
void sense_cal_store(void)
{
    uint8_t buf[SENSE_CAL_BYTES];
    uint32_t n = sense_cal_export(buf);
    if (velo_cal_save(buf, n) == 0) {
        sn.moved6 = 0;
        sn.force_save = 0;
    }
}
void sense_service(uint32_t now_ms, uint32_t idle_ok)
{
    if (!idle_ok)
        return;
    if (!sn.last_save_ok) {                       /* (the first call: the clock of the store starts here) */
        sn.last_save_ok = 1;
        sn.last_save_ms = now_ms;
    }
    if (sn.force_save || (sn.moved6 >= SC_CAL_SAVE_MOVED * 64u && now_ms - sn.last_save_ms >= SC_CAL_SAVE_MS)) {
        sn.last_save_ms = now_ms;
        sense_cal_store();
    }
}
