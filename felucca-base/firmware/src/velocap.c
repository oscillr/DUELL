/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca. */
/* KEY SENSE CAPTURE, the main-loop side (DUELL_VELO_CAPTURE=1, developer builds only; velocap.h has the tick side).
 * velocap_drain() writes the queued rows into the USB serial console's output ring WITHOUT ever waiting: when the ring has no room for a whole
 * line it stops and tries again on the next call; with no port open (cdc.dtr 0) it throws the rows away (not counted as dropped). Lines (CR LF):
 *   VC,V,ts_us,key,adc,drive      one reading (docs/VELO_CSV.md, the same four numbers)
 *   VC,N,ts_us,key,velocity,floor,ceiling   a note-on: the velocity the unit played and the key's range (counts) at that moment
 *   VC,R,key,floor,ceiling        the learned range of a key (all 20 when the port opens, then every 5 s)
 *   VC,dropped N                  N rows were dropped (ring full) since the last such line
 *   VC,S,rows_sent,dropped_total,ring_peak,notes_lost     every 5 s
 * Called from main.c's loop (after cdc_task, and in the 15 ms wait). Never from an interrupt. */
#define VN_N 16u
static struct {
    uint32_t ts[VN_N];
    uint8_t key[VN_N], vel[VN_N];
    uint16_t fl[VN_N], ce[VN_N];
    volatile uint32_t wr, rd;
    uint32_t lost;                      /* note-ons dropped (ring full) */
    uint32_t sent, dropped_seen, due_ms, rr, was_open;
} vn VC_BSS;

static void velocap_note(uint32_t key, uint32_t vel)       /* the event step (key_on, seq.c): one note-on */
{
    uint32_t f = 0, c = 0, i;
    if (!vc.on)
        return;
    if (key >= VC_NKEY || vn.wr - vn.rd >= VN_N) {
        vn.lost += key < VC_NKEY;
        return;
    }
    sense_cal_get(key, &f, &c);
    i = vn.wr % VN_N;
    vn.ts[i] = fm1_an.us;
    vn.key[i] = (uint8_t)key;
    vn.vel[i] = (uint8_t)vel;
    vn.fl[i] = (uint16_t)f;
    vn.ce[i] = (uint16_t)c;
    VC_BARRIER();
    vn.wr++;
}

static char *vc_dec(char *d, uint32_t v)
{
    char b[10];
    uint32_t n = 0;
    do
        b[n++] = (char)('0' + v % 10u);
    while ((v /= 10u) != 0u);
    while (n)
        *d++ = b[--n];
    return d;
}
static char *vc_f(char *d, char tag, uint32_t a, uint32_t b, uint32_t c, uint32_t e, uint32_t f, uint32_t nf)
{
    uint32_t v[5], i;
    v[0] = a, v[1] = b, v[2] = c, v[3] = e, v[4] = f;
    *d++ = 'V';
    *d++ = 'C';
    *d++ = ',';
    *d++ = tag;
    for (i = 0; i < nf; i++) {
        *d++ = ',';
        d = vc_dec(d, v[i]);
    }
    *d++ = '\r';
    *d++ = '\n';
    return d;
}
static int vc_emit(const char *b, const char *e)          /* the line into the console's ring if it fits whole; 0: no room */
{
    uint32_t n = (uint32_t)(e - b);
    if (CO_N - (co_w - co_r) < n)
        return 0;
    while (b < e) {
        cdc_out[co_w % CO_N] = (uint8_t)*b++;
        RING_PUBLISH();
        co_w++;
    }
    return 1;
}

static void velocap_con(const char *p)                     /* console word (console.c con_exec) */
{
    if (con_word(&p, "on")) {
        uint32_t k;
        for (k = 0; k < VC_NKEY; k++)
            vc.pn[k] = vc.post[k] = 0;
        vc.rd = vc.wr;
        vn.rd = vn.wr;
        vc.on = 1;
        con_puts("OK velocap on\r\n");
    } else if (con_word(&p, "off")) {
        vc.on = 0;
        con_puts("OK velocap off\r\n");
    } else
        con_puts(vc.on ? "OK velocap is on\r\n" : "OK velocap is off\r\n");
}

static void velocap_drain(void)                            /* main loop */
{
    char b[72], *e;
    uint32_t guard;
    if (!cdc.dtr || !vc.on) {                                      /* nobody listens: nothing is kept (and not counted as dropped) */
        vc.rd = vc.wr;
        vn.rd = vn.wr;
        vn.was_open = 0;
        return;
    }
    if (!vn.was_open) {                                    /* the port has just opened: the ranges, and the counters start over for this recording */
        vn.was_open = 1;
        vn.rr = 1;                                         /* 1 + key = the next range line */
        vn.due_ms = fm1_ms + 5000u;
        vn.dropped_seen = vc.dropped;
    }
    if (vn.rr) {                                           /* the range table, a line at a time as room allows */
        for (guard = 0; vn.rr && guard < 8u; guard++) {
            uint32_t f = 0, c = 0, k = vn.rr - 1u;
            sense_cal_get(k, &f, &c);
            e = vc_f(b, 'R', k, f, c, 0, 0, 3);
            if (!vc_emit(b, e))
                return;
            vn.rr = vn.rr < VC_NKEY ? vn.rr + 1u : 0u;
        }
        if (vn.rr)
            return;
    }
    if (vc.dropped != vn.dropped_seen) {
        uint32_t d = vc.dropped, n = d - vn.dropped_seen;
        char *p = b;
        const char *s = "VC,dropped ";
        while (*s)
            *p++ = *s++;
        p = vc_dec(p, n);
        *p++ = '\r';
        *p++ = '\n';
        if (!vc_emit(b, p))
            return;
        vn.dropped_seen = d;
    }
    for (guard = 0; vn.rd != vn.wr && guard < 8u; guard++) {   /* the note-ons first (few, and they say what the unit decided) */
        uint32_t i = vn.rd % VN_N;
        e = vc_f(b, 'N', vn.ts[i], vn.key[i], vn.vel[i], vn.fl[i], vn.ce[i], 5);
        if (!vc_emit(b, e))
            return;
        vn.rd++;
    }
    for (guard = 0; vc.rd != vc.wr && guard < 64u; guard++) {  /* the readings */
        uint32_t i = vc.rd % VC_N, w = vc.w[i];
        char *p = vc_f(b, 'V', vc.ts[i], w >> 11, w & 1023u, (w >> 10) & 1u, 0, 4);
        if (!vc_emit(b, p))
            return;
        vc.sent++;
        VC_BARRIER();
        vc.rd++;
    }
    if ((int32_t)(fm1_ms - vn.due_ms) >= 0) {              /* every 5 s: the status line, the ranges again */
        vn.due_ms = fm1_ms + 5000u;
        vn.rr = 1;
        e = vc_f(b, 'S', vc.sent, vc.dropped, vc.peak, vn.lost, 0, 4);
        vc_emit(b, e);
    }
}
