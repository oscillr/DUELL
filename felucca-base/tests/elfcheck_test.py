#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/elfcheck.py on tiny fake ELFs (./host_tests.sh elfcheck): the release's .noinit table passes; bench B's
LTO split (bootguard.0 ...), a shrunk magic word, a wrong .noinit size, entry point or image size fail. The RAM code
(check_ramcode, the DUELL_HOTRAM bench builds R1..R3): the release's flash driver and the hot layout pass; hot code without
the switch, none with it, out of call reach, an interrupt / USB function in RAM, RAM code over boot_info, the stacks or .noinit fail.
ELFCHECK_ELF=<felucca.elf> also checks a real one (add ELFCHECK_PROBE=1 for a core 1 probe build, ELFCHECK_HOT=1 for R1..R3)."""
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import elfcheck  # noqa: E402

# the 0.2.12 / 0.2.13 release ELF's .noinit (read from build/felucca.elf of the published packages)
RELEASE = [("fm1_crash", 0x01C7C000, 64), ("felucca_dbg", 0x01C7C040, 92), ("bootguard", 0x01C7C09C, 12),
           ("panel", 0x01C7C0A8, 32), ("settings", 0x01C7C0C8, 16), ("crash_seen", 0x01C7C0D8, 4)]
DECLARED = {n: "test" for n, _, _ in RELEASE}


def fake_elf(syms, noinit_size=0xDC, entry=0x02000120, text_size=0x6E598, ram=None, noinit_at=0x01C7C000):
    """a minimal ELF32 LE: .text (one PT_LOAD at 0x02000120), .noinit (NOBITS), .symtab, .strtab, .shstrtab; ram: a .ram_text
    section (addr, size, [(symbol, addr, size)]) as section 6 (its symbols: functions; names starting with '_': linker labels)"""
    shstr = b"\0.text\0.noinit\0.symtab\0.strtab\0.shstrtab\0.ram_text\0"
    off = {n: shstr.index(n.encode() + b"\0") for n in (".text", ".noinit", ".symtab", ".strtab", ".shstrtab", ".ram_text")}
    strtab, symtab = b"\0", bytes(16)
    for name, addr, size in syms:
        symtab += struct.pack("<IIIBBH", len(strtab), addr, size, 0x11, 0, 2)   # GLOBAL OBJECT in section 2
        strtab += name.encode() + b"\0"
    for name, addr, size in (ram[2] if ram else []):
        symtab += struct.pack("<IIIBBH", len(strtab), addr, size, 0x00 if name.startswith("_") else 0x02, 0, 6)  # LOCAL FUNC
        strtab += name.encode() + b"\0"
    text = b"\x04\x81\x80\x00"
    body = bytearray(52 + 32)                     # ELF header + one program header
    t_off = len(body); body += text
    sym_off = len(body); body += symtab
    str_off = len(body); body += strtab
    shs_off = len(body); body += shstr
    body += bytes(-len(body) % 4)
    sh_off = len(body)
    shdrs = [bytes(40),
             struct.pack("<IIIIIIIIII", off[".text"], 1, 6, 0x02000120, t_off, len(text), 0, 0, 2, 0),
             struct.pack("<IIIIIIIIII", off[".noinit"], 8, 3, noinit_at, t_off, noinit_size, 0, 0, 4, 0),
             struct.pack("<IIIIIIIIII", off[".symtab"], 2, 0, 0, sym_off, len(symtab), 4, 1, 4, 16),
             struct.pack("<IIIIIIIIII", off[".strtab"], 3, 0, 0, str_off, len(strtab), 0, 0, 1, 0),
             struct.pack("<IIIIIIIIII", off[".shstrtab"], 3, 0, 0, shs_off, len(shstr), 0, 0, 1, 0)]
    if ram:
        shdrs.append(struct.pack("<IIIIIIIIII", off[".ram_text"], 1, 7, ram[0], t_off, ram[1], 0, 0, 4, 0))
    body += b"".join(shdrs)
    body[0:52] = (b"\x7fELF\x01\x01\x01" + bytes(9) +
                  struct.pack("<HHIIIIIHHHHHH", 2, 0xF0, 1, entry, 52, sh_off, 0, 52, 32, 1, 40, len(shdrs), 5))
    body[52:84] = struct.pack("<IIIIIIII", 1, t_off, 0x02000120, 0x02000120, text_size, text_size, 5, 4)
    return bytes(body)


def run(label, data, want_fail, needle=None, **kw):
    errs = elfcheck.check(elfcheck.read_elf(data), kw.pop("declared", DECLARED), **kw)
    ok = bool(errs) == want_fail and (needle is None or any(needle in e for e in errs))
    print(f"{'PASS' if ok else 'FAIL'}  {label}: " + ("; ".join(errs[:2]) if errs else "no error"))
    return ok


def fake_obj(secs):
    """a relocatable ELF32 LE with PROGBITS sections of these names (RELA for .rela.*); '@name': the name shares the next
    entry's string (clang's tail merge: .text.f inside .rela.text.f) -> (bytes, the names)"""
    shstr, offs, names = bytearray(b"\0"), [], []
    plain = [n.lstrip("@") for n in secs]
    for i, n in enumerate(secs):
        if n.startswith("@"):
            offs.append(None)
            continue
        at = len(shstr)
        shstr += n.encode() + b"\0"
        offs.append(at)
        if i and offs[i - 1] is None:               # the one before is this one's tail
            offs[i - 1] = at + len(n) - len(plain[i - 1])
    sto = len(shstr)
    shstr += b".shstrtab\0"
    body = bytearray(52)
    shs_off = len(body)
    body += shstr + bytes(-len(shstr) % 4)
    sh_off = len(body)
    hdrs = [bytes(40)] + [struct.pack("<IIIIIIIIII", offs[i], 4 if plain[i].startswith(".rela") else 1, 6, 0, 52, 0, 0, 0, 2, 0)
                          for i in range(len(secs))]
    hdrs.append(struct.pack("<IIIIIIIIII", sto, 3, 0, 0, shs_off, len(shstr), 0, 0, 1, 0))
    body += b"".join(hdrs)
    body[0:52] = (b"\x7fELF\x01\x01\x01" + bytes(9) +
                  struct.pack("<HHIIIIIHHHHHH", 1, 0xF0, 1, 0, 0, sh_off, 0, 52, 0, 0, 40, len(hdrs), len(hdrs) - 1))
    return bytes(body), plain


def obj_names(data):
    shoff, = struct.unpack_from("<I", data, 32)
    _, shnum, shstrndx = struct.unpack_from("<HHH", data, 46)
    sh = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * 40) for i in range(shnum)]
    base = sh[shstrndx][4]
    return [data[base + s[0]:data.index(b"\0", base + s[0])].decode() for s in sh[1:-1]]


DRIVER = [("_rt_start", 0x01C00000, 0), ("fl_jedec_ram", 0x01C00000, 0x14C), ("fl_prog_ram", 0x01C0014C, 0xA20)]
HOT_AT = 0x01C7C100                            # firmware/app_hot.ld RAMCODE
HOT = [("_rt_start", HOT_AT, 0), ("fl_jedec_ram", HOT_AT, 0x14C), ("fl_prog_ram", HOT_AT + 0x14C, 0xA20),
       ("_hot_start", HOT_AT + 0xB6C, 0), ("uf_run", HOT_AT + 0xB6C, 0x432), ("wave_render", HOT_AT + 0xF9E, 0x47C),
       ("_etext", 0x0206E6C4, 0)]


def ram(label, data, hot, want_fail, needle=None):
    errs, notes = elfcheck.check_ramcode(elfcheck.read_elf(data), hot)
    ok = bool(errs) == want_fail and (needle is None or any(needle in e for e in errs))
    print(f"{'PASS' if ok else 'FAIL'}  {label}: " + ("; ".join(errs[:2]) if errs else "; ".join(notes)))
    return ok


def main():
    good = True
    rel = elfcheck.read_elf(fake_elf(RELEASE))
    good &= elfcheck.noinit_table(rel) == sorted((a, n, s) for n, a, s in RELEASE) and rel["entry"] == 0x02000120
    good &= run("release table", fake_elf(RELEASE), False)
    lto = [s for s in RELEASE if s[0] != "bootguard"] + [("bootguard.0", 0x01C7C0AC, 4), ("bootguard.1", 0x01C7C0B0, 4),
                                                          ("bootguard.2", 0x01C7C0B4, 4)]
    good &= run("bench B's bootguard.0 (LTO split)", fake_elf(lto), True, "bootguard.0")
    good &= run("bench B: bootguard missing as one object", fake_elf(lto), True, "bootguard (12 B) is missing")
    good &= run("a GlobalOpt bool (settings.b)", fake_elf([s for s in RELEASE if s[0] != "settings"] +
                                                          [("settings.b", 0x01C7C0C8, 1)]), True, "settings.b")
    good &= run("a shrunk magic word (bootguard 4 B)", fake_elf([s if s[0] != "bootguard" else ("bootguard", s[1], 4)
                                                                 for s in RELEASE]), True, "the source declares 12")
    good &= run(".noinit size 0xC8 (bench B)", fake_elf(RELEASE, 0xC8), True, "expected 0xdc")
    good &= run(".noinit size in the slack (0xE0)", fake_elf(RELEASE, 0xE0), False)
    good &= run("entry point 0x02000000", fake_elf(RELEASE, entry=0x02000000), True, "entry point")
    good &= run("image over the slot", fake_elf(RELEASE, text_size=0x8DFC0), True, "app slot")
    good &= run("unknown .noinit object", fake_elf(RELEASE + [("new_guard", 0x01C7C0DC, 4)]), True, "unknown")
    good &= run("a source object not in the table", fake_elf(RELEASE), True, "NOINIT_OBJECTS",
                declared={**DECLARED, "new_guard": "x.c"})
    good &= run("probe build (bench_c1_nv, 0xE8)", fake_elf(RELEASE + [("bench_c1_nv", 0x01C7C0DC, 12)], 0xE8), False,
                declared={**DECLARED, "bench_c1_nv": "bench.c"}, probe=True)
    good &= run("leaves the NOINIT region", fake_elf(RELEASE), True, "region", region=(0x01C7C000, 0x40))
    # the RAM code (check_ramcode): the release's flash driver at 0x01C00000; a DUELL_HOTRAM build's block at the top of RAM
    good &= ram("release: the flash driver at 0x01C00000", fake_elf(RELEASE, ram=(0x01C00000, 0xB6C, DRIVER)), False, False)
    good &= ram("hot build (R1..R3 layout)", fake_elf(RELEASE, ram=(HOT_AT, 0x141A, HOT)), True, False)
    good &= ram("hot code in a build without DUELL_HOTRAM", fake_elf(RELEASE, ram=(HOT_AT, 0x141A, HOT)), False, True, "without")
    good &= ram("DUELL_HOTRAM build with no hot code", fake_elf(RELEASE, ram=(0x01C00000, 0xB6C, DRIVER)), True, True, "no hot")
    good &= ram("hot code out of call reach (at 0x01C00000)",
                fake_elf(RELEASE, ram=(0x01C00000, 0x141A, [(n, a - HOT_AT + 0x01C00000 if a < 0x02000000 else a, z)
                                                             for n, a, z in HOT])), True, True, "reach")
    good &= ram("the audio interrupt in RAM", fake_elf(RELEASE, ram=(HOT_AT, 0x141A, HOT + [("fm1_alnk0_irq", HOT_AT + 0x141A - 4, 4)])),
                True, True, "USB")
    good &= ram("USB code in RAM", fake_elf(RELEASE, ram=(HOT_AT, 0x141A, HOT + [("usb_poll", HOT_AT + 0x141A - 4, 4)])),
                True, True, "USB")
    good &= ram("RAM code over boot_info", fake_elf(RELEASE, ram=(0x01C7F000, 0xE00, [])), False, True, "boot_info")
    good &= ram("RAM code over the stacks", fake_elf(RELEASE, ram=(0x01C7B000, 0x800, [])), False, True, "stacks")
    good &= ram("RAM code over .noinit", fake_elf(RELEASE, ram=(0x01C7C000, 0x800, [])), False, True, ".noinit")
    good &= ram(".noinit moved", fake_elf(RELEASE, noinit_at=0x01C7C100), False, True, "not 0x1c7c000")
    # tools/hot_fns.py renames .text.<f> -> .hotx.<f> in place (the code is untouched): a fake object as clang writes it
    # (.text.uf_run's name tail-merged into .rela.text.uf_run's), uf_run renamed, wv_lvl's section absent (inlined), a
    # look-alike (.text.uf_run2) and a .rodata.uf_run left alone
    import hot_fns
    obj, names = fake_obj([".text.lcd_spin", "@.text.uf_run", ".rela.text.uf_run", ".text.uf_run2", ".rodata.uf_run"])
    out, done, missing = hot_fns.hot_place(obj, 1)
    got = obj_names(out)
    want = [".text.lcd_spin", ".hotx.uf_run", ".rela.hotx.uf_run", ".text.uf_run2", ".rodata.uf_run"]
    ok = (got == want and done == ["uf_run"] and "wv_lvl" in missing and len(out) == len(obj)
          and sum(a != b for a, b in zip(obj, out)) == 4)    # (only ".text." -> ".hotx.": 4 bytes of one string)
    print(f"{'PASS' if ok else 'FAIL'}  hot_fns: {names} -> {got}; renamed {done}")
    good &= ok
    src = elfcheck.source_noinit(Path(__file__).resolve().parents[1] / "firmware")   # the sources' list, all in the table
    miss = sorted(set(src) - set(elfcheck.NOINIT_OBJECTS))
    print(f"{'FAIL' if miss or not set(DECLARED) <= set(src) else 'PASS'}  sources declare {sorted(src)}")
    good &= not miss and set(DECLARED) <= set(src)
    real = os.environ.get("ELFCHECK_ELF")
    if real:
        e = elfcheck.read_elf(Path(real).read_bytes())
        print("\n".join(elfcheck.report(e)))
        errs = elfcheck.check(e, {**src, **{n: "(elf)" for _, n, _ in elfcheck.noinit_table(e) if n in elfcheck.NOINIT_OBJECTS}},
                              os.environ.get("ELFCHECK_PROBE") == "1")
        rerr, rnotes = elfcheck.check_ramcode(e, os.environ.get("ELFCHECK_HOT") == "1")
        print("\n".join(rnotes))
        errs += rerr
        print(f"{'PASS' if not errs else 'FAIL'}  {real}: " + ("; ".join(errs) or "no error"))
        good &= not errs
    return 0 if good else 1


if __name__ == "__main__":
    sys.exit(main())
