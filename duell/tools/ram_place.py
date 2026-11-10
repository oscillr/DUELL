#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL_RAMPLACE=1 (two-core stage 1a, developer builds only): part 2's voice code moved into two unused RAM ranges, link-time only.

  ram_place.py IN.o OUT.o          (build.py calls place() and check_object(); by hand: rename and print the checks)

The same mechanism as tools/hot_fns.py: felucca.c is compiled as always (-ffunction-sections: function f is section .text.f), then the
listed sections are renamed in felucca_hot.o, .text.f -> .ramx.f (range X, tools/ram_fns_x.py) or .ramz.f (range Z, tools/ram_fns_z.py),
the same length, in place. firmware/app_ram.ld links .ramx.* at 0x01C00000 and .ramz.* after the pool; main.c fm1_cstart copies both
from their load images first thing at power-on and compares them (fm1_ramplace_check). The C, the compiled code and the host builds are
untouched: the normal build (no DUELL_RAMPLACE) is byte for byte the one of cloud/base.

Checks on the object (before the link; the linker also fails on a call out of reach, "relocation truncated to fit"):
  - every listed function has a section of its own (none inlined away), and no function is in two lists or in the hot set;
  - a function in X is called (or its address taken in code) only from X, Z or the hot block: the flash cannot reach X;
  - a function in X calls (or takes the address of) only functions in X, Z or the hot block: X cannot reach the flash, and core 1 must not
    run flash code from it (the shared cache, BENCH-B3).
After the link (reach_check, with the ELF's addresses): every call / jump relocation in the object is within +-4 MiB of its target.
"""
import struct
import sys

import hot_fns
import ram_fns_x
import ram_fns_z

OLD = b".text."
PREFIX = {"X": b".ramx.", "Z": b".ramz."}       # the same length as .text.: renamed in place
R_CALL = 2                                      # R_PI32V2_LONG_JUMP_23M2 (call / goto: +-4 MiB)
R_ABS_INSN = 5                                  # a 32-bit address in an instruction pair (no reach limit)
CALL_REACH = 0x400000


def lists():
    """{'X': [...], 'Z': [...]}"""
    return {"X": list(ram_fns_x.NAMES), "Z": list(ram_fns_z.NAMES)}


def _sections(d):
    if d[:4] != b"\x7fELF" or d[4] != 1 or d[5] != 1:
        raise SystemExit("ram_place: not a 32-bit little-endian ELF")
    shoff, = struct.unpack_from("<I", d, 32)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 46)
    sh = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]
    return sh, sh[shstrndx][4]


def _cstr(d, off):
    return bytes(d[off:d.index(b"\0", off)])


def place(obj, groups=None):
    """-> (the object with the listed functions' sections renamed, {'X': [renamed], 'Z': [...]}, [names without a section])"""
    groups = groups or lists()
    d = bytearray(obj)
    sh, base = _sections(d)
    by_name = {_cstr(d, base + s[0]): s for s in sh}
    done, missing = {k: [] for k in groups}, []
    for k, fs in groups.items():
        for f in fs:
            s = by_name.get(OLD + f.encode())
            if s is None or s[1] != 1:          # SHT_PROGBITS
                missing.append(f)
                continue
            at = base + s[0]
            d[at:at + len(OLD)] = PREFIX[k]
            done[k].append(f)
    left = {_cstr(d, base + s[0]) for s in sh}
    moved = {f for fs in done.values() for f in fs}
    if any(n.startswith(OLD) and n[len(OLD):].decode() in moved for n in left):
        raise SystemExit("ram_place: a renamed section kept its old name (a shared string?)")
    return bytes(d), done, missing


def graph(obj):
    """the object's references: [(from section, offset, type, to section, to symbol, addend)], from the RELA sections"""
    d = obj
    sh, base = _sections(d)
    names = [_cstr(d, base + s[0]).decode() for s in sh]
    syms = []
    for s in sh:
        if s[1] == 2:                           # SHT_SYMTAB
            strtab = sh[s[6]][4]
            for j in range(s[5] // 16):
                st_name, value, size, info, _, shndx = struct.unpack_from("<IIIBBH", d, s[4] + j * 16)
                syms.append((_cstr(d, strtab + st_name).decode(), value, info & 0xF, shndx))
    out = []
    for s in sh:
        if s[1] != 4:                           # SHT_RELA
            continue
        src = names[s[7]]
        for j in range(s[5] // 12):
            off, info, add = struct.unpack_from("<IIi", d, s[4] + j * 12)
            n, value, typ, shndx = syms[info >> 8]
            to = names[shndx] if 0 < shndx < len(names) else ""
            out.append((src, off, info & 0xFF, to, n if typ != 3 else "", add + (value if typ == 3 else 0)))
    return out


def _fn(sec):
    for p in (OLD, *PREFIX.values(), hot_fns.NEW):
        p = p.decode()
        if sec.startswith(p):
            return sec[len(p):], p
    return None, None


def check_object(obj, hot_names=()):
    """obj: the object after hot_place and place. -> (errors, notes)"""
    errors, notes = [], []
    g = lists()
    seen = {}
    for k, fs in g.items():
        for f in fs:
            if f in seen:
                errors.append(f"{f} is in range {seen[f]} and range {k}")
            seen[f] = k
            if f in hot_names:
                errors.append(f"{f} is in the hot set (tools/hot_fns.py) and in range {k}")
    ram = (".ramx.", ".ramz.", hot_fns.NEW.decode())
    xcalls_out, xcallers_flash, xptr = [], [], 0
    for src, off, typ, to, sym, add in graph(obj):
        sf, sp = _fn(src)
        tf, tp = _fn(to)
        if sp == ".ramx." and typ in (R_CALL, R_ABS_INSN) and tp is not None and tp not in ram:
            xcalls_out.append(f"{sf} -> {tf}")
        if tp == ".ramx." and sp not in ram and typ == R_CALL:
            xcallers_flash.append(f"{sf} -> {tf}")
        if tp == ".ramx." and typ not in (R_CALL,):
            xptr += 1
    if xcalls_out:
        errors.append(f"range X calls code outside RAM (X can reach only RAM): {sorted(set(xcalls_out))[:6]}")
    if xcallers_flash:
        errors.append(f"flash code calls range X directly (out of reach; only pointers may enter X): {sorted(set(xcallers_flash))[:6]}")
    notes.append(f"range X closed: no call from X leaves RAM, no flash code calls into X ({xptr} pointer references enter X)")
    return errors, notes


def reach_check(obj, elf):
    """every call / jump relocation of the object within +-4 MiB of its target, at the linked addresses. elf: elfcheck.read_elf()"""
    addr = {}
    for n, a, sz, sec in elf["symbols"]:
        if n and sec in (".text", ".ram_text", ".ramx", ".ramz"):
            addr.setdefault(n, a)
    errors, far, worst = [], 0, 0
    for src, off, typ, to, sym, add in graph(obj):
        if typ != R_CALL:
            continue
        sf, _ = _fn(src)
        tf, _ = _fn(to) if not sym else (sym, None)
        if sf not in addr or tf not in addr:
            continue
        site, tgt = addr[sf] + off, addr[tf] + add
        dist = abs(tgt - site)
        if (tgt >= 0x02000000) != (site >= 0x02000000):
            worst = max(worst, dist)
        if dist >= CALL_REACH:
            errors.append(f"call {sf}+{off:#x} -> {tf} is {dist:#x} away (reach {CALL_REACH:#x})")
            far += 1
    return errors[:6], f"call reach: every call between RAM and the flash within {worst:#x} of {CALL_REACH:#x}"


def functions(obj):
    """{function: (prefix, section bytes, sorted relocations (offset, type, target function or symbol or section, addend))} of an object"""
    d = obj
    sh, base = _sections(d)
    names = [_cstr(d, base + s[0]).decode() for s in sh]
    objs = {}                                   # section -> [(start, end, name)] of its named data objects
    for s in sh:
        if s[1] == 2:
            strtab = sh[s[6]][4]
            for j in range(s[5] // 16):
                st_name, value, size, info, _, shndx = struct.unpack_from("<IIIBBH", d, s[4] + j * 16)
                if info & 0xF == 1 and size and 0 < shndx < len(names):      # STT_OBJECT
                    objs.setdefault(names[shndx], []).append((value, value + size, _cstr(d, strtab + st_name).decode()))

    def where(to, add):
        """a data reference as (object, offset in it), or the 16 bytes it points at (a string): the same in two builds whose data moved"""
        for lo, hi, n in objs.get(to, ()):
            if lo <= add < hi:
                return (n, add - lo)
        s = sh[names.index(to)] if to in names else None
        return ("@" + to, bytes(d[s[4] + add:s[4] + add + 16]) if s and s[1] == 1 else add)
    rel = {}
    for src, off, typ, to, sym, add in graph(d):
        tf, _ = _fn(to)
        tgt = (sym, add) if sym else (tf, add) if tf else where(to, add)
        rel.setdefault(src, []).append((off, typ, tgt))
    out = {}
    for n, s in zip(names, sh):
        f, p = _fn(n)
        if f and s[1] == 1:
            out[f] = (p, bytes(d[s[4]:s[4] + s[5]]), sorted(rel.get(n, [])))
    return out


def compare(ref, new, which=None):
    """the placed functions of new (sections .ramx / .ramz) against the same functions of ref (a build without DUELL_RAMPLACE): the same
    bytes and the same relocations (only the link address differs). which: the names to compare (default: every placed one). -> (same, diff)"""
    a, b = functions(ref), functions(new)
    names = which or sorted(f for f, v in b.items() if v[0] in (".ramx.", ".ramz."))
    same, diff = [], []
    for f in names:
        if f in a and f in b and a[f][1:] == b[f][1:]:
            same.append(f)
        else:
            diff.append(f)
    return same, diff


if __name__ == "__main__" and len(sys.argv) == 4 and sys.argv[1] == "--compare":
    ra, rb = open(sys.argv[2], "rb").read(), open(sys.argv[3], "rb").read()
    same, diff = compare(ra, rb)
    fa, fb = functions(ra), functions(rb)
    nb = sum(len(fb[f][1]) for f in same)
    print(f"ram_place --compare: {len(same)} placed functions ({nb} B) the same object code and relocations as in {sys.argv[2]}; differ: {diff}")
    other = [f for f in fb if f in fa and fa[f][1:] != fb[f][1:]]
    print(f"  all functions: {len(fb)}; differing anywhere: {len(other)} {other[:12]}")
    sys.exit(1 if diff else 0)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__.strip().splitlines()[2])
        sys.exit(2)
    out, done, missing = place(open(sys.argv[1], "rb").read())
    open(sys.argv[2], "wb").write(out)
    e, n = check_object(out, hot_fns.names(3))
    print(f"ram_place: X {len(done['X'])}, Z {len(done['Z'])} renamed; no section: {missing}")
    print("\n".join(n + e))
