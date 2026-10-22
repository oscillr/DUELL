/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DUELL cloud plan, job A (the store): the EXTENSION BLOCK of a patch record (docs/design/EXTENSION_BLOCK_PLAN.md section 3) and the
 * hand-set B edit list (docs/design/EDITABLE_B_DESIGN.md section 3). Pure data code: bytes in, bytes out; no flash, no globals.
 * Included by core.h after ext.h (NX); upreset.c / multi.c / project.c use it, job G (editable B) packs and unpacks its list with it.
 *
 * Wide id: one byte for every stored value. 0..126 the old parameter ids, 127 RESERVED (the motion escape, core.h MOTION_ESC),
 * 128..255 the extension slots (ext.h XWID(k): slot k = 128 + k; 128 + NX .. 255 are slots a newer firmware may have).
 * Values are stored as value + 64, a byte (-64 .. +127).
 *
 * The block, in a patch record when the flag PTF_XB is set (upreset.c; it replaces the retired 2-byte shadow marker, same place):
 *   [L: 1 byte, the bytes after it][XV: version, 1][section]...         section: [tag][n: bytes of data][data]
 *   tag 1 (XB_T_EXT)   the extension values: n/2 pairs (wide id 128..255, value + 64), only the ones off their default. An id the
 *                      firmware has no slot for (>= 128 + NX, or below 128) is skipped. A slot the block does not name is its default.
 *   tag 2 (XB_T_EDIT)  the hand-set B edits: n/2 pairs (wide id 0..126 or 128..255, value + 64), at most XB_MAX, the ABSOLUTE value of
 *                      End B, each id once. Job G reads and writes it with xb_unpack / xb_pack.
 *   any other tag      skipped by its n. A tag 1 or 2 twice, an odd n, a section past the block's end, a value byte over 191, wide id 127
 *                      in tag 2, more than XB_MAX edits, a version other than XB_VER: the block is damaged and the record is refused.
 * The multis and the project do not use sections: they hold the same two things densely (multi.c, project.c).
 *
 * xb_pack / xb_unpack are the pair list alone (what tag 2 holds, and what a multi's per-part edit list holds after its count byte). */
#ifndef DUELL_UP_XB_H
#define DUELL_UP_XB_H

typedef struct { uint8_t wid; int16_t val; } xb_edit_t;     /* one hand-set B value: wide id, absolute value (-64 .. +127) */
#define XB_MAX 32                                           /* hand-set B edits at most */
#define XB_VER 1u                                           /* the block's version byte */
#define XB_T_EXT 1u
#define XB_T_EDIT 2u
#define XB_ERR 0xFFFFFFFFu                                  /* xb_unpack: a damaged list */
#define XB_BLOCK_MAX (1u + 1u + (2u + 2u * NX) + (2u + 2u * XB_MAX))   /* the biggest block: L, XV, tag 1 with every slot, tag 2 with 32 edits (102) */

/* the stored default of slot k (value + 64): what a record or multi that does not name the slot means. Job B: keep this equal to the
 * descriptors' defaults of the slots (TWIN LINK is ON and SUB OCTAVE is -1 = stored 1 by EXTENSION_BLOCK_PLAN 2A; CYCLE RISE / FALL are
 * "open" there; job B picked ENV 2's: RISE 10, FALL 70, stored 74 and 134; the store only needs the table to be the same on the way in and out) */
#define XB_DEFS {64u, 65u, 64u, 64u, 64u, 64u, 64u, 65u, 64u, 64u, 64u, 74u, 134u, 64u, 64u, 64u}
_Static_assert(NX == 16, "up_xb.h: XB_DEFS lists the 16 slots");
static uint8_t xb_def(uint32_t k) { static const uint8_t D[NX] = XB_DEFS; return D[k]; }   /* slot k's default, value + 64 */
static void xb_defaults(uint8_t *xv)                        /* xv: NX bytes, value + 64 */
{
    uint32_t k;
    for (k = 0; k < NX; k++)
        xv[k] = xb_def(k);
}

/* the edit list e[0..n) -> out (2 bytes each: wide id, value + 64); the bytes written. At most XB_MAX edits are written; a value is
 * held to -64 .. +127; an edit with wide id 127 (the reserved one) is left out */
static uint32_t xb_pack(const xb_edit_t *e, uint32_t n, uint8_t *out)
{
    uint32_t i, w = 0;
    for (i = 0; i < n && w < 2u * XB_MAX; i++) {
        int32_t v = e[i].val < -64 ? -64 : e[i].val > 127 ? 127 : e[i].val;
        if (e[i].wid == 127u)
            continue;
        out[w++] = e[i].wid;
        out[w++] = (uint8_t)(v + 64);
    }
    return w;
}
/* len bytes of pairs -> e (room for XB_MAX); the number of edits, or XB_ERR if the list is damaged: an odd length, over XB_MAX edits, a
 * value byte over 191, wide id 127, an id twice */
static uint32_t xb_unpack(const uint8_t *in, uint32_t len, xb_edit_t *e)
{
    uint32_t n = len / 2u, i, j;
    if ((len & 1u) || n > XB_MAX)
        return XB_ERR;
    for (i = 0; i < n; i++) {
        if (in[2u * i] == 127u || in[2u * i + 1u] > 191u)
            return XB_ERR;
        for (j = 0; j < i; j++)
            if (in[2u * j] == in[2u * i])
                return XB_ERR;
    }
    for (i = 0; i < n; i++) {
        e[i].wid = in[2u * i];
        e[i].val = (int16_t)((int32_t)in[2u * i + 1u] - 64);
    }
    return n;
}

/* the extension values xv (NX bytes, value + 64) and the edits e[0..ne) -> out (XB_BLOCK_MAX bytes) as a block; its length (0: nothing to
 * store, no block: every slot at its default and no edit) */
static uint32_t xb_block_build(const uint8_t *xv, const xb_edit_t *e, uint32_t ne, uint8_t *out)
{
    uint32_t k, at = 2u, n1 = 0, n2;
    uint8_t tmp[2u * XB_MAX];
    out[1] = (uint8_t)XB_VER;
    for (k = 0; k < NX; k++)
        n1 += xv[k] != xb_def(k);
    if (n1) {
        out[at++] = (uint8_t)XB_T_EXT;
        out[at++] = (uint8_t)(2u * n1);
        for (k = 0; k < NX; k++)
            if (xv[k] != xb_def(k)) {
                out[at++] = (uint8_t)XWID(k);
                out[at++] = xv[k];
            }
    }
    n2 = xb_pack(e, ne, tmp);
    if (n2) {
        out[at++] = (uint8_t)XB_T_EDIT;
        out[at++] = (uint8_t)n2;
        for (k = 0; k < n2; k++)
            out[at++] = tmp[k];
    }
    if (at == 2u)
        return 0;
    out[0] = (uint8_t)(at - 1u);
    return at;
}
/* a block at b (avail bytes there): its length (1 + L), or 0 if it is damaged (see the top) */
static uint32_t xb_block_ok(const uint8_t *b, uint32_t avail)
{
    uint32_t end, at = 2u, seen = 0;
    xb_edit_t tmp[XB_MAX];
    if (avail < 2u || b[0] < 1u || 1u + (uint32_t)b[0] > avail || b[1] != XB_VER)
        return 0;
    end = 1u + b[0];
    while (at < end) {
        uint32_t tag, n;
        if (at + 2u > end)
            return 0;
        tag = b[at];
        n = b[at + 1u];
        at += 2u;
        if (at + n > end)
            return 0;
        if (tag == XB_T_EXT || tag == XB_T_EDIT) {
            uint32_t i;
            if ((seen >> tag & 1u) || (n & 1u))
                return 0;
            seen |= 1u << tag;
            if (tag == XB_T_EDIT) {
                if (xb_unpack(b + at, n, tmp) == XB_ERR)
                    return 0;
            } else {
                for (i = 0; i < n / 2u; i++)
                    if (b[at + 2u * i + 1u] > 191u)
                        return 0;
            }
        }
        at += n;
    }
    return end;
}
/* a block that xb_block_ok took -> xv (NX bytes, set to the defaults first by the caller, then the slots the block names) and the edits e
 * (room for XB_MAX); the number of edits */
static uint32_t xb_block_read(const uint8_t *b, uint8_t *xv, xb_edit_t *e)
{
    uint32_t end = 1u + b[0], at = 2u, ne = 0;
    while (at + 2u <= end) {
        uint32_t tag = b[at], n = b[at + 1u], i;
        at += 2u;
        if (tag == XB_T_EXT)
            for (i = 0; i < n / 2u; i++) {
                uint32_t w = b[at + 2u * i];
                if (w >= XW_BASE && w < XW_BASE + NX)
                    xv[w - XW_BASE] = b[at + 2u * i + 1u];
            }
        else if (tag == XB_T_EDIT)
            ne = xb_unpack(b + at, n, e);
        at += n;
    }
    return ne;
}
#endif /* DUELL_UP_XB_H */
