/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The few libc routines the compiler may call implicitly (struct copies,
 * zeroing). Freestanding: no other libc. */
#include "memfast.h"                    /* word-at a-time loops: a clear or a copy was 2-3 instructions a byte */
void *memset(void *d, int c, unsigned n) { return mf_memset(d, c, n); }
void *memcpy(void *d, const void *s, unsigned n) { return mf_memcpy(d, s, n); }

/* 64-bit division (the EQ coefficients, fx.c: 7 divisions a band, 21 a part, every time a value of the EQ moves): the firmware links no
 * compiler runtime. The first version shifted and subtracted 64 times (about 1050 pi32 instructions a division, 44 k for both parts'
 * EQs at once, in the audio interrupt). Now Hacker's Delight divlu / udivdi3: the pi32 divides 32 by 32 in hardware, so a 64-bit
 * division is two or three of those and a few multiplications (about 100 instructions), the same exact quotient (tests/loadcost_test.c
 * checks it against the compiler's on a few million operands). */
static inline unsigned div_nlz32(uint32_t x)
{
    unsigned n = 32;
    uint32_t y;
    y = x >> 16; if (y) { n -= 16; x = y; }
    y = x >> 8;  if (y) { n -= 8;  x = y; }
    y = x >> 4;  if (y) { n -= 4;  x = y; }
    y = x >> 2;  if (y) { n -= 2;  x = y; }
    y = x >> 1;  if (y) return n - 2;
    return n - x;
}
/* (u1:u0) / v for u1 < v (the quotient fits 32 bits), by 16-bit digits */
static uint32_t div_divlu(uint32_t u1, uint32_t u0, uint32_t v)
{
    const uint32_t b = 65536u;
    uint32_t un1, un0, vn1, vn0, q1, q0, un32, un21, un10, rhat;
    unsigned s = div_nlz32(v);
    v <<= s;
    vn1 = v >> 16;
    vn0 = v & 0xFFFFu;
    un32 = s ? (u1 << s) | (u0 >> (32u - s)) : u1;
    un10 = u0 << s;
    un1 = un10 >> 16;
    un0 = un10 & 0xFFFFu;
    q1 = un32 / vn1;
    rhat = un32 - q1 * vn1;
    while (q1 >= b || q1 * vn0 > b * rhat + un1) {
        q1--;
        rhat += vn1;
        if (rhat >= b)
            break;
    }
    un21 = un32 * b + un1 - q1 * v;
    q0 = un21 / vn1;
    rhat = un21 - q0 * vn1;
    while (q0 >= b || q0 * vn0 > b * rhat + un0) {
        q0--;
        rhat += vn1;
        if (rhat >= b)
            break;
    }
    return q1 * b + q0;
}
unsigned long long __udivdi3(unsigned long long n, unsigned long long d)
{
    uint32_t dh = (uint32_t)(d >> 32), nh = (uint32_t)(n >> 32);
    if (!d)
        return 0;
    if (!dh) {
        uint32_t dl = (uint32_t)d;
        if (!nh)
            return (uint32_t)n / dl;
        if (nh < dl)
            return div_divlu(nh, (uint32_t)n, dl);
        {
            uint32_t q1 = nh / dl, k = nh - q1 * dl;
            return (unsigned long long)q1 << 32 | div_divlu(k, (uint32_t)n, dl);
        }
    } else {
        unsigned s = div_nlz32(dh);
        uint32_t v1 = (uint32_t)((d << s) >> 32);
        unsigned long long u1 = n >> 1, q0;
        q0 = (unsigned long long)div_divlu((uint32_t)(u1 >> 32), (uint32_t)u1, v1);
        q0 = (q0 << s) >> 31;
        if (q0)
            q0--;
        if (n - q0 * d >= d)
            q0++;
        return q0;
    }
}

long long __divdi3(long long n, long long d)
{
    unsigned long long q = __udivdi3(n < 0 ? 0ull - (unsigned long long)n : (unsigned long long)n,
                                     d < 0 ? 0ull - (unsigned long long)d : (unsigned long long)d);
    return (n < 0) != (d < 0) ? (long long)(0ull - q) : (long long)q;
}

int memcmp(const void *a, const void *b, unsigned n)
{
    const unsigned char *p = a, *q = b;
    for (; n; n--, p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static unsigned str_len(const char *s)
{
    unsigned n = 0;
    while (s[n])
        n++;
    return n;
}

static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}

/* decimal with an optional fixed point: fmt_fix(buf, -63, 1) -> "-6.3" */
static void fmt_fix(char *b, int32_t v, int decimals)
{
    char t[12];
    int n = 0, neg = v < 0, i;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if (n == decimals)
            t[n++] = '.';
    } while (u || n <= decimals);
    if (neg)
        t[n++] = '-';
    for (i = 0; i < n; i++)
        b[i] = t[n - 1 - i];
    b[n] = 0;
}

static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }

static uint32_t rng_state = 0x1234567u;
static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
