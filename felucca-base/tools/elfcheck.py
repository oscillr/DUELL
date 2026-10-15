#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The start-up layout check of the app ELF (DUELL, 2026-10-14; run by tools/build.py after every link).

  tools/elfcheck.py <felucca.elf> [--probe] [--hot] [--image-size N]   (prints the .noinit table, exit 1 on a failure)

Why: bench build B (-flto) did not start on a real FM-1. The LTO plugin internalised the .noinit objects
(bootguard, fm1_crash, settings), LLVM assumed their initial value was 0, shrank each 32-bit magic word to a
one-byte flag and split the structs (symbols bootguard.0, fm1_crash.0, settings.0). .noinit is not zeroed at
boot, so garbage read as "magic present", bootguard.failed counted up from garbage and fm1_cstart entered UBOOT
(docs/BENCH_B_BOOT_STUDY.md, hal/fm1_irq.h). This reads the ELF (no tools, no dependencies) and fails when:
  (a) a .noinit object is split or shrunk: a .noinit symbol with a '.' in its name (name.0, name.b: LTO / GlobalOpt
      / SROA), an object the sources declare in .noinit missing or not of its size in NOINIT_OBJECTS, a .noinit
      object the table does not know (a new one: add it with its sizeof), or one the sources do not declare;
  (b) the .noinit section's size is off the expected one (NOINIT_SIZE) by more than NOINIT_SLACK, or it leaves the
      NOINIT region (it must end before boot_info);
  (c) the entry point is not 0x02000120, or the image exceeds the app slot;
  (d) (check_ramcode, DUELL 2026-10-15) the RAM code (.ram_text: the flash driver, and in a DUELL_HOTRAM build the hot audio
      code after _hot_start) leaves RAM, overlaps data / pool / stacks / .noinit, holds interrupt / USB / MIDI / update code,
      is out of a direct call's reach of .text, or hot code appears without --hot (DUELL_HOTRAM); .noinit not at 0x01C7C000.
It only reads: the package bytes do not change.
"""
import re
import struct
import sys
from pathlib import Path

APP_XIP = 0x02000120
APP_SLOT = 0x8DFBC                 # fm1pkg_make.APP_SLOT (XIP 0x02000120 + 0x8DFBC)

# Every object the sources put in .noinit, with its C size (sizeof). Most hold a 32-bit magic or guard word first.
NOINIT_OBJECTS = {
    "fm1_crash": 64,               # hal/fm1_irq.h fm1_crash_t: magic "CRSH" + 15 words (the crash record)
    "felucca_dbg": 92,             # src/audio.c: magic "DBG1" + 22 words (diagnostics)
    "bootguard": 12,               # src/core.h: magic, failed, pending (the boot guard: 2 failed starts -> UBOOT)
    "panel": 32,                   # src/panel.c panel_t: magic + the learned button / encoder table
    "settings": 16,                # src/panel.c: magic "SET4", palette, lowcut, zoom
    "crash_seen": 4,               # src/main.c: fm1_crash.count at the last boot
    "bench_c1_nv": 12,             # src/bench.c (DUELL_CORE1_PROBE builds only): the core 1 probe's guard
}
NOINIT_OPTIONAL = {"bench_c1_nv"}  # compiled in only by some builds
NOINIT_SIZE = {"default": 0xDC,    # release 0.2.12 / 0.2.13 and bench builds A, C (and B2): 220 B
               "probe": 0xE8}      # DUELL_CORE1_PROBE=1 (bench D, E): + bench_c1_nv
NOINIT_SLACK = 8                   # bytes the section may move (alignment) before the build fails
NOINIT_DECL = re.compile(r"\b(\w+)\s+__attribute__\s*\(\(\s*section\s*\(\s*\"\.noinit[\w.]*\"\s*\)")


def read_elf(data):
    """ELF32 little-endian -> {'entry', 'sections': {name: (addr, size)}, 'symbols': [(name, addr, size, section)],
    'image_end': the highest load address + file size of the XIP program headers}"""
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise ValueError("not a 32-bit little-endian ELF")
    entry, phoff, shoff = struct.unpack_from("<III", data, 24)
    phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from("<HHHHH", data, 42)
    sh = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]

    def cstr(off):
        return data[off:data.index(b"\0", off)].decode("latin-1")
    names = [cstr(sh[shstrndx][4] + s[0]) for s in sh]
    sections = {n: (s[3], s[5]) for n, s in zip(names, sh) if n}
    symbols = []
    for s in sh:
        if s[1] != 2:              # SHT_SYMTAB
            continue
        strtab = sh[s[6]][4]
        for j in range(1, s[5] // 16):
            st_name, value, size, info, _, shndx = struct.unpack_from("<IIIBBH", data, s[4] + j * 16)
            if info & 0xF in (3, 4):            # STT_SECTION, STT_FILE
                continue
            sec = names[shndx] if 0 < shndx < len(names) else ""
            symbols.append((cstr(strtab + st_name), value, size, sec))
    image_end = APP_XIP
    for i in range(phnum):
        p_type, _, _, paddr, filesz = struct.unpack_from("<IIIII", data, phoff + i * phentsize)
        if p_type == 1 and filesz and APP_XIP <= paddr < APP_XIP + 0x01000000:
            image_end = max(image_end, paddr + filesz)
    return {"entry": entry, "sections": sections, "symbols": symbols, "image_end": image_end}


def source_noinit(fw):
    """the names the firmware sources declare in .noinit (comments stripped; loader/ is its own image)"""
    found = {}
    for f in sorted([*Path(fw).glob("src/*.[ch]"), *Path(fw).glob("hal/*.[ch]")]):
        s = re.sub(r"/\*.*?\*/", " ", f.read_text(errors="replace"), flags=re.S)
        s = re.sub(r"//[^\n]*", "", s)
        for m in NOINIT_DECL.finditer(s):
            found[m.group(1)] = f.name
    return found


def noinit_table(elf):
    return sorted((a, n, sz) for n, a, sz, sec in elf["symbols"] if sec == ".noinit" and n)


def check(elf, declared, probe=False, image_size=None, region=None):
    """-> list of errors (empty: the layout is the release's kind). declared: source_noinit()'s names;
    region: (origin, length) of app.ld's NOINIT (None: not checked)"""
    errors = []
    if elf["entry"] != APP_XIP:
        errors.append(f"entry point {elf['entry']:#010x}, not {APP_XIP:#010x}")
    size = image_size if image_size is not None else elf["image_end"] - APP_XIP
    if size > APP_SLOT:
        errors.append(f"image {size} B exceeds the app slot ({APP_SLOT} B)")
    for n in sorted(set(declared) - set(NOINIT_OBJECTS)):
        errors.append(f".noinit object {n} ({declared[n]}) is not in tools/elfcheck.py NOINIT_OBJECTS: add it with its sizeof")
    syms = noinit_table(elf)
    have = {}
    for addr, n, sz in syms:
        if "." in n:
            errors.append(f".noinit symbol {n} at {addr:#x} ({sz} B): {n.split('.')[0]} was split or shrunk by the "
                          f"optimiser (LTO): its magic word is no longer a 32-bit compare")
            continue
        have[n] = (addr, sz)
        if n not in NOINIT_OBJECTS:
            errors.append(f".noinit symbol {n} at {addr:#x} is unknown (not in NOINIT_OBJECTS)")
        elif n not in declared:
            errors.append(f".noinit symbol {n} is not declared in the sources")
        elif sz != NOINIT_OBJECTS[n]:
            errors.append(f".noinit object {n} is {sz} B, the source declares {NOINIT_OBJECTS[n]} B")
    for n in sorted(set(declared) & set(NOINIT_OBJECTS)):
        if n not in have and (n not in NOINIT_OPTIONAL or any(s[1].startswith(n + ".") for s in syms)):
            errors.append(f".noinit object {n} ({NOINIT_OBJECTS[n]} B) is missing from the ELF as one object")
    sect = elf["sections"].get(".noinit")
    if not sect:
        errors.append("no .noinit section")
    else:
        want = NOINIT_SIZE["probe" if probe else "default"]
        if abs(sect[1] - want) > NOINIT_SLACK:
            errors.append(f".noinit is {sect[1]:#x} B, expected {want:#x} +- {NOINIT_SLACK} "
                          f"(tools/elfcheck.py NOINIT_SIZE; a changed layout must be looked at)")
        if region and not (region[0] <= sect[0] and sect[0] + sect[1] <= region[0] + region[1]):
            errors.append(f".noinit {sect[0]:#x}+{sect[1]:#x} leaves the NOINIT region")
    return errors


# RAM code (.ram_text): the flash driver (hal/fm1_flash.h, no calls) and, in a DUELL_HOTRAM build (bench R1..R3, firmware/app_hot.ld),
# the hot audio functions after it (_hot_start). Both layouts must keep it in RAM below boot_info, clear of the data, the pool, the
# stacks and .noinit, and keep the interrupt / USB / MIDI / update paths in the flash (as in the release); hot code must be within a
# direct call's reach (R_PI32V2_LONG_JUMP_23M2: +-4 MiB) of the whole .text, both ways.
RAM_LO, RAM_TOP = 0x01C00000, 0x01C7FD4C      # RAM0 .. boot_info (the SDK's linker.ld: 0x1c7fe00 - 128 - 52)
STACKS = (0x01C74000, 0x01C7C000)              # app.ld / app_hot.ld: _guard0 .. _sstack_top
NOINIT_ADDR = 0x01C7C000                       # .noinit (the crash record the update loader keeps: loader.ld)
CALL_REACH = 0x400000
FLASH_ONLY = re.compile(r"_irq$|^usb_|^uac_|^ota_|^ed_|^midi_|^uart_")


def check_ramcode(elf, hot=False):
    """-> (errors, notes) for the RAM code; hot: a DUELL_HOTRAM build (hot audio code expected after _hot_start)"""
    errors, notes = [], []
    secs, sym = elf["sections"], {n: a for n, a, _, _ in elf["symbols"]}
    ni = secs.get(".noinit")
    if ni and ni[0] != NOINIT_ADDR:
        errors.append(f".noinit at {ni[0]:#x}, not {NOINIT_ADDR:#x}")
    rt = secs.get(".ram_text")
    if not rt or not rt[1]:
        if hot:
            errors.append("DUELL_HOTRAM build without RAM code (.ram_text empty)")
        return errors, notes
    lo, hi = rt[0], rt[0] + rt[1]
    if not (RAM_LO <= lo and hi <= RAM_TOP):
        errors.append(f".ram_text {lo:#x}..{hi:#x} is not in RAM below boot_info ({RAM_LO:#x}..{RAM_TOP:#x})")
    for n in (".data", ".bss", ".pool", ".noinit"):
        s = secs.get(n)
        if s and s[1] and lo < s[0] + s[1] and s[0] < hi:
            errors.append(f".ram_text {lo:#x}..{hi:#x} overlaps {n} {s[0]:#x}..{s[0] + s[1]:#x}")
    if lo < STACKS[1] and STACKS[0] < hi:
        errors.append(f".ram_text {lo:#x}..{hi:#x} overlaps the stacks {STACKS[0]:#x}..{STACKS[1]:#x}")
    hs = sym.get("_hot_start")
    fns = sorted((a, n, sz) for n, a, sz, sec in elf["symbols"] if sec == ".ram_text" and n and not n.startswith(("_", ".")))
    hot_fns = [f for f in fns if hs is not None and f[0] >= hs]
    if hot and not hot_fns:
        errors.append("DUELL_HOTRAM build: no hot function after _hot_start in .ram_text")
    if not hot and hot_fns:
        errors.append(f"hot code in RAM without DUELL_HOTRAM: {[n for _, n, _ in hot_fns[:4]]}")
    bad = [n for _, n, _ in fns if FLASH_ONLY.search(n)]
    if bad:
        errors.append(f"interrupt / USB / MIDI / update code in RAM (it stays in the flash): {bad[:6]}")
    drv = (hs if hs is not None else hi) - lo
    note = f".ram_text {lo:#010x}..{hi:#010x}: flash driver {drv} B"
    if hot_fns:
        et = sym.get("_etext", elf["image_end"])
        reach = et - lo
        if reach >= CALL_REACH:
            errors.append(f"hot code at {lo:#x} is {reach:#x} from the end of .text {et:#x}: beyond a call's reach {CALL_REACH:#x}")
        note += (f", hot audio code {hi - hs} B ({len(hot_fns)} functions), call reach {reach:#x} of {CALL_REACH:#x}, "
                 f"{RAM_TOP - hi} B free up to boot_info")
    notes.append(note)
    return errors, notes


def report(elf):
    sect = elf["sections"].get(".noinit", (0, 0))
    rows = [f".noinit {sect[0]:#x} size {sect[1]:#x} ({sect[1]} B); entry {elf['entry']:#010x}; "
            f"image {elf['image_end'] - APP_XIP} B of {APP_SLOT}"]
    rows += [f"  {a:#010x} {sz:4d} {n}" for a, n, sz in noinit_table(elf)]
    return rows


def main(argv):
    args = list(argv)
    probe = "--probe" in args
    if probe:
        args.remove("--probe")
    if "--hot" in args:
        args.remove("--hot")
    img = None
    if "--image-size" in args:
        i = args.index("--image-size")
        img = int(args[i + 1], 0)
        del args[i:i + 2]
    if len(args) != 1:
        print(__doc__.strip().splitlines()[2])
        return 2
    elf = read_elf(Path(args[0]).read_bytes())
    fw = Path(__file__).resolve().parents[1] / "firmware"
    declared = source_noinit(fw)
    # the ELF may come from another tree: names it holds that this tree's sources lack count as declared
    declared.update({n: "(elf)" for _, n, _ in noinit_table(elf) if n in NOINIT_OBJECTS and "." not in n})
    print("\n".join(report(elf)))
    errors = check(elf, declared, probe, img)
    rerr, rnotes = check_ramcode(elf, "--hot" in argv)
    errors += rerr
    print("\n".join(rnotes))
    for e in errors:
        print("  FAIL ", e)
    print("start-up layout: " + ("FAIL" if errors else "ok (the release's kind)"))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
