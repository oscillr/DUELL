# SPDX-License-Identifier: GPL-3.0-only
"""DUELL: the End B block (MBV) of a patch, in Python. The SAME format as firmware/src/morph.c (morph_bv_make / morph_bv_size / morph_bv_load) and
web/editor/patchrecord.js (endBSize, buildEndB): no second format, only a second reader and writer of it.

    [0]      flags: 1 = the marks of the values set by hand follow (20 B), 2 = FM6: End B's 65 patch bytes follow (57 B)
    [1]      the part's AMOUNT knob + 1 (0: not stored)
    [2..21]  a bit per wide id (0..126 the ids, 127 never, 128..155 the extension slots): End B differs from the Original there
             then the values of the set bits in wide id order, each as (value - min) in ceil(log2(max - min + 1)) bits, LSB first, padded to the byte
    [marks]  20 bytes: a bit per wide id, End B's values set by hand
    [FM6]    57 bytes: 65 bytes 0..99, 7 bits each, LSB first
The ranges are the unit's (web/editor/layout.json, written from the firmware's own tables: web/test_ranges.mjs compares them with tests/rangedump.c).
Used by tools/gen_duell_sounds.py (a pre-loaded patch's 'bend'), tools/convert_endb.py and tools/import_owner_edits.py."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAYOUT = json.loads((ROOT / "web" / "editor" / "layout.json").read_text())
E0 = LAYOUT["P_E0"]
HDR, PIN_BYTES, FM_N, FM_BYTES = 22, 20, 65, 57
FM6_ENGINE = next(i for i, e in enumerate(LAYOUT["ENG"]) if e["name"] == "FM6")
# the wide values End B never differs in (firmware/src/morph.c mbv_never)
MBV_NEVER = {0, 5, 21, 22, 23, 24, 29, 30, 31, 32, 40, 81, 82, 86, 94, 105, 111, 112, 115, 116, 117, 118, 149, 153, 154, 155}


def wid_ok(w):
    return 0 <= w < 127 or 128 <= w < 156


def rng(eng, w):
    d = LAYOUT["XTP"][w - 128] if w >= 128 else LAYOUT["ENG"][eng]["edit"][w - E0] if w >= E0 else LAYOUT["TP"][w]
    return d["min"], d["max"]


def bits(eng, w):
    lo, hi = rng(eng, w)
    n, b = hi - lo + 1, 0
    while b < 16 and (1 << b) < n:
        b += 1
    return b


def get_bits(b, at, n):
    v = 0
    for i in range(n):
        v |= (b[(at + i) >> 3] >> ((at + i) & 7) & 1) << i
    return v, at + n


def put_bits(buf, at, v, n):
    for i in range(n):
        if v >> i & 1:
            buf[(at + i) >> 3] |= 1 << ((at + i) & 7)
    return at + n


def code(eng, w, v):
    lo, hi = rng(eng, w)
    return min(max(v, lo), hi) - lo


def decode(b, eng):
    """block bytes -> dict(amt, vals {wid: End B value}, marks [wid], fm [65] or None, size); ValueError when it is no block (morph_bv_size)"""
    b = bytes(b)
    if len(b) < HDR or b[0] & ~3 or b[1] > 101 or (b[0] & 2 and eng != FM6_ENGINE):
        raise ValueError("no End B block")
    vals, at = {}, HDR * 8
    for w in range(160):
        if b[2 + (w >> 3)] >> (w & 7) & 1:
            if not wid_ok(w) or w in MBV_NEVER:
                raise ValueError("End B block: a bit on wide id %d" % w)
            c, at = get_bits(b, at, bits(eng, w))
            lo, hi = rng(eng, w)
            vals[w] = min(lo + c, hi)
    at = (at + 7) & ~7
    marks, fm = [], None
    if b[0] & 1:
        for w in range(160):
            if b[at // 8 + (w >> 3)] >> (w & 7) & 1:
                if not wid_ok(w):
                    raise ValueError("End B block: a mark on wide id %d" % w)
                marks.append(w)
        at += PIN_BYTES * 8
    if b[0] & 2:
        fm = []
        for _ in range(FM_N):
            v, at = get_bits(b, at, 7)
            if v > 99:
                raise ValueError("End B block: an FM6 byte over 99")
            fm.append(v)
    size = (at + 7) // 8
    if size > len(b):
        raise ValueError("End B block: cut short")
    return dict(amt=b[1], vals=vals, marks=marks, fm=fm, size=size)


def encode(eng, amt, vals, ref, marks=(), fm=None):
    """vals {wid: End B value} (only where End B differs from the Original), ref {wid: Original value} (None: not checked) -> block bytes (morph_bv_make's output)"""
    out = bytearray(HDR + 96 + PIN_BYTES + FM_BYTES)
    out[1] = amt
    for w in sorted(vals):
        if not wid_ok(w) or w in MBV_NEVER:
            raise ValueError("End B: wide id %d never differs between the ends" % w)
        if ref is not None and code(eng, w, vals[w]) == code(eng, w, ref[w]):
            raise ValueError("End B: wide id %d equals the Original" % w)
        out[2 + (w >> 3)] |= 1 << (w & 7)
    at = HDR * 8
    for w in sorted(vals):
        at = put_bits(out, at, code(eng, w, vals[w]), bits(eng, w))
    at = (at + 7) & ~7
    if marks:
        out[0] |= 1
        for w in marks:
            out[at // 8 + (w >> 3)] |= 1 << (w & 7)
        at += PIN_BYTES * 8
    if fm is not None:
        if eng != FM6_ENGINE or len(fm) != FM_N or any(not 0 <= v <= 99 for v in fm):
            raise ValueError("End B: the FM6 bytes are 65 values 0..99, FM6 only")
        out[0] |= 2
        for v in fm:
            at = put_bits(out, at, v, 7)
    return bytes(out[:(at + 7) // 8])


def wrap_items(items, ind=12, width=150):
    """items ['a: 1', ..] -> lines of at most `width` columns, each indented `ind` (the form tools/duell_sounds.py writes its values in)"""
    lines, cur = [], ""
    for it in items:
        if cur and len(cur) + len(it) + 2 + ind > width:
            lines.append(cur.rstrip())
            cur = ""
        cur += it + ", "
    if cur:
        lines.append(cur.rstrip())
    return "\n".join(" " * ind + l for l in lines)


def sound_kwargs(bend, marks, fm):
    """the text of a sound's `bend=` (and `bpin=`, `bfm=`) keywords, as tools/duell_sounds.py holds them; bend {label: value}, marks [labels], fm [65] or None"""
    kw = ["bend={\n" + wrap_items(["%r: %d" % (k, v) for k, v in bend.items()]) + "\n         }" if bend else "bend={}"]
    if marks:
        kw.append("bpin=%r" % list(marks))
    if fm is not None:
        kw.append("bfm=[\n" + wrap_items([str(v) for v in fm]) + "\n         ]")
    return kw
