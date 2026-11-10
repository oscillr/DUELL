/* SPDX-License-Identifier: GPL-3.0-only */
/* memset / memcpy that move a word at a time (libc.c; tests/loadcost_test.c builds the same code). The byte loops they replace cost
 * 2 and 3 instructions a byte on the pi32: clearing a 16 KB slice of the delay line took 33 k instructions, a bus' reverb buffers
 * 20 k, a project's tracks copied with interrupts off ~50 k, all of it inside one audio half buffer at a load or a mode change. Here:
 * the head bytes up to a word boundary, then 16 bytes an iteration, then the tail. memcpy takes the word path only when source and
 * destination have the same alignment (otherwise the byte loop: the pi32 must not be given an unaligned word).
 * The word accesses are volatile on purpose: the pi32 backend otherwise fuses neighbouring word loads / stores into 64-bit ones (d[...]),
 * and a buffer is only guaranteed 4-byte aligned here. Needs <stdint.h> types; the compiler must not turn the loops into calls of
 * memset / memcpy (the build's -fno-builtin). */
#ifndef MEMFAST_H
#define MEMFAST_H
typedef volatile uint32_t __attribute__((may_alias)) mf_word_t;

static inline void *mf_memset(void *d, int c, unsigned n)
{
    unsigned char *p = d, b = (unsigned char)c;
    uint32_t w = b * 0x01010101u;
    while (n && ((uint32_t)(uintptr_t)p & 3u)) {
        *p++ = b;
        n--;
    }
    if (n >= 4u) {
        mf_word_t *q = (mf_word_t *)p;
        for (; n >= 16u; n -= 16u, q += 4) {
            q[0] = w;
            q[1] = w;
            q[2] = w;
            q[3] = w;
        }
        for (; n >= 4u; n -= 4u)
            *q++ = w;
        p = (unsigned char *)q;
    }
    while (n--)
        *p++ = b;
    return d;
}

static inline void *mf_memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    if (n >= 8u && !(((uint32_t)(uintptr_t)p ^ (uint32_t)(uintptr_t)q) & 3u)) {
        while ((uint32_t)(uintptr_t)p & 3u) {
            *p++ = *q++;
            n--;
        }
        {
            mf_word_t *a = (mf_word_t *)p;
            const mf_word_t *b = (const mf_word_t *)q;
            for (; n >= 16u; n -= 16u, a += 4, b += 4) {
                uint32_t w0 = b[0], w1 = b[1], w2 = b[2], w3 = b[3];
                a[0] = w0;
                a[1] = w1;
                a[2] = w2;
                a[3] = w3;
            }
            for (; n >= 4u; n -= 4u)
                *a++ = *b++;
            p = (unsigned char *)a;
            q = (const unsigned char *)b;
        }
    }
    while (n--)
        *p++ = *q++;
    return d;
}
#endif
