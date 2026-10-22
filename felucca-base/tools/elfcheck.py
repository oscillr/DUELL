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
    "tc_nv": 12,                   # hal/fm1_core1.h (DUELL_TWOCORE builds only): the second core's record across a reset
}
NOINIT_OPTIONAL = {"bench_c1_nv", "tc_nv"}  # compiled in only by some builds
NOINIT_SIZE = {"default": 0xDC,    # release 0.2.12 / 0.2.13 and bench builds A, C (and B2): 220 B
               "probe": 0xE8}      # DUELL_CORE1_PROBE=1 (bench D, E): + bench_c1_nv; DUELL_TWOCORE=1: + tc_nv
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
    contents = {n: data[s[4]:s[4] + s[5]] for n, s in zip(names, sh) if n in (".ramx", ".ramz", ".ram_text") and s[1] == 1}
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
    return {"entry": entry, "sections": sections, "symbols": symbols, "image_end": image_end, "contents": contents}


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


def check_ramcode(elf, hot=False, ramplace=False):
    """-> (errors, notes) for the RAM code; hot: a DUELL_HOTRAM build (hot audio code expected after _hot_start); ramplace: a
    DUELL_RAMPLACE build (ranges X and Z: check_ramplace; without it, neither may exist)"""
    errors, notes = [], []
    for n in (".ramx", ".ramz"):
        if not ramplace and elf["sections"].get(n, (0, 0))[1]:
            errors.append(f"{n} (DUELL_RAMPLACE's RAM code) in a build without DUELL_RAMPLACE")
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


# DUELL_RAMPLACE (two-core stage 1a, firmware/app_ram.ld, tools/ram_place.py): part 2's voice code in range X (0x01C00000..0x01C07F00, closed:
# nothing there may call the flash, the flash cannot call it) and range Z (after the pool, below the stack guard: reaches and is reached by
# everything). main.c fm1_cstart copies both from the image before any other call; the PC limit's RAM window starts at range X.
RANGE_X = (0x01C00000, 0x01C07F00)
STACK_GUARD = 0x01C74000
HOT_HEADROOM = 228                             # bytes the hot block keeps free up to boot_info (cloud/base 85dee78: 228)
DIS_LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*\t(.*)$")
DIS_LABEL = re.compile(r"^([A-Za-z_.$][\w.$]*):\s*$")
DIS_TARGET = re.compile(r"\b(call|goto)\s+-?\d+\s+<[^:>]*:\s*([0-9a-f]+)\s*>")
DIS_BARE = re.compile(r"\b(call|goto)\s+(-?\d+)\s*$")   # (inside an if { } body objdump prints no target: it is address + 4 + offset)


def dis_target(addr, text):
    """the direct target of a call / goto instruction, or None"""
    m = DIS_TARGET.search(text)
    if m:
        return int(m.group(2), 16)
    m = DIS_BARE.search(text)
    return (addr + 4 + int(m.group(2))) & 0xFFFFFFFF if m else None


def dis_functions(dis):
    """the objdump -d text -> {function: [(address, instruction text)]}"""
    fns, cur = {}, None
    for ln in dis.splitlines():
        m = DIS_LABEL.match(ln)
        if m:
            cur = fns.setdefault(m.group(1), [])
            continue
        m = DIS_LINE.match(ln)
        if m and cur is not None:
            cur.append((int(m.group(1), 16), m.group(3)))
    return fns


def check_ramplace(elf, img, dis, app_xip=APP_XIP):
    """-> (errors, notes) for ranges X and Z. img: the app image (felucca.bin) the package carries; dis: objdump -d of the ELF"""
    import ram_fns_x
    import ram_fns_z
    errors, notes = [], []
    secs = elf["sections"]
    sym = {}
    for n, a, sz, sec in elf["symbols"]:
        sym.setdefault(n, a)
    rx, rz, rt = secs.get(".ramx"), secs.get(".ramz"), secs.get(".ram_text")
    if not rx or not rx[1] or not rz or not rz[1]:
        return ["DUELL_RAMPLACE build without range X or Z code"], notes
    xlo, xhi, zlo, zhi = rx[0], rx[0] + rx[1], rz[0], rz[0] + rz[1]
    if not (RANGE_X[0] <= xlo and xhi <= RANGE_X[1]):
        errors.append(f"range X {xlo:#x}..{xhi:#x} leaves {RANGE_X[0]:#x}..{RANGE_X[1]:#x}")
    pool = secs.get(".pool", (0, 0))
    if not (pool[0] + pool[1] <= zlo and zhi <= STACK_GUARD):
        errors.append(f"range Z {zlo:#x}..{zhi:#x} is not between the pool's end {pool[0] + pool[1]:#x} and the stack guard {STACK_GUARD:#x}")
    for name, lo, hi in ((".ramx", xlo, xhi), (".ramz", zlo, zhi)):
        for n in (".data", ".bss", ".pool", ".noinit", ".ram_text"):
            s = secs.get(n)
            if s and s[1] and lo < s[0] + s[1] and s[0] < hi:
                errors.append(f"{name} {lo:#x}..{hi:#x} overlaps {n} {s[0]:#x}..{s[0] + s[1]:#x}")
    # the functions: exactly the lists, nothing of the interrupt / USB / MIDI / update paths
    for name, mod, lo, hi in ((".ramx", ram_fns_x, xlo, xhi), (".ramz", ram_fns_z, zlo, zhi)):
        got = {n for n, a, sz, sec in elf["symbols"] if sec == name and n and sz and not n.startswith(".")}   # (sz: no linker labels)
        want = set(getattr(mod, "ELF_NAMES", mod.NAMES))
        if got != want:
            errors.append(f"{name} holds {sorted(got - want)[:4]} not listed, lacks {sorted(want - got)[:4]}")
        bad = [n for n in got if FLASH_ONLY.search(n)]
        if bad:
            errors.append(f"interrupt / USB / MIDI / update code in {name}: {bad[:4]}")
    # the start-up copy: symbols at the sections' bounds, load images inside the package's image and equal to the sections
    for tag, name, lo, hi in (("rx", ".ramx", xlo, xhi), ("rz", ".ramz", zlo, zhi)):
        s0, s1, ld = sym.get(f"_{tag}_start"), sym.get(f"_{tag}_end"), sym.get(f"_{tag}_load")
        if s0 != lo or s1 is None or not (hi <= s1 <= hi + 3) or ld is None:
            errors.append(f"_{tag}_start / _{tag}_end / _{tag}_load do not bound {name} ({s0}, {s1}, {ld})")
            continue
        off = ld - app_xip
        body = elf["contents"].get(name, b"")
        if ld % 4 or off < 0 or off + len(body) > len(img) or bytes(img[off:off + len(body)]) != body:
            errors.append(f"{name}'s load image at {ld:#x} is not in the app image as linked")
    fns = dis_functions(dis)
    cst = fns.get("fm1_cstart", [])
    vals = {k: sym.get(k) for k in ("_rx_load", "_rz_load", "_rx_end", "_rz_end", "_pc_ram_lo")}

    def loads(insns, v):
        return [i for i, (_, t) in enumerate(insns) if v is not None and re.search(rf"= {v}\b", t)]
    at = {ins[0][0]: n for n, ins in fns.items() if ins}

    def in_xz(a):
        return xlo <= a < xhi or zlo <= a < zhi

    def reaches_xz(f, seen):
        """f (a function in the flash) or what it calls, directly, runs code in X or Z"""
        if f in seen:
            return None
        seen.add(f)
        for a, t in fns.get(f, ()):
            tgt = dis_target(a, t)
            if tgt is None or (fns[f] and fns[f][0][0] <= tgt <= fns[f][-1][0]):
                continue
            if in_xz(tgt):
                return f
            hit = at.get(tgt) and reaches_xz(at[tgt], seen)
            if hit:
                return hit
        return None
    copy_at, before = None, []                            # the copy: fm1_cstart itself, or one function it calls; calls before it
    for i, (a, t) in enumerate(cst):
        if loads([(0, t)], vals["_rx_load"]) or loads([(0, t)], vals["_rz_load"]):
            copy_at = i
            break
        tgt = dis_target(a, t)
        if tgt is not None and re.search(r"\bcall\b", t):
            callee = at.get(tgt)
            if callee and loads(fns[callee], vals["_rx_load"]) and loads(fns[callee], vals["_rz_load"]):
                copy_at = i
                break
            hit = in_xz(tgt) and hex(tgt) or callee and reaches_xz(callee, set())
            if hit or not callee:
                errors.append(f"fm1_cstart calls {callee or hex(tgt)} before it copies ranges X and Z, and it runs X / Z code ({hit or '?'})")
            before.append(callee or hex(tgt))
    if copy_at is None:
        errors.append("fm1_cstart does not copy ranges X and Z (no load of _rx_load / _rz_load)")
    # the PC limit window: from range X to the end of the hot block, set by the guard (inlined into fm1_cstart)
    lo_pc, hi_pc = vals["_pc_ram_lo"], sym.get("_rt_end")
    guard = cst + fns.get("fm1_guard_enable", [])
    if lo_pc is None or hi_pc is None or lo_pc > min(xlo, zlo, rt[0] if rt else xlo) or hi_pc < max(xhi, zhi):
        errors.append(f"the PC window {lo_pc}..{hi_pc} does not cover X, Z and the hot block")
    elif not loads(guard, lo_pc):
        errors.append(f"the PC limit is not set from _pc_ram_lo ({lo_pc:#x}): hal/fm1_guard.h without DUELL_RAMPLACE?")
    for c1 in ("fm1_c1_entry", "fm1_c1_main"):             # DUELL_TWOCORE: core 1 runs only code inside the PC window (its loop and entry are in range X)
        if c1 in sym and not (xlo <= sym[c1] < xhi and lo_pc is not None and hi_pc is not None and lo_pc <= sym[c1] < hi_pc):
            errors.append(f"{c1} at {sym[c1]:#x} is not in range X inside the PC window {lo_pc}..{hi_pc} (app_ram.ld: KEEP(*(.c1_text*)) in .ramx)")
    # no call from X out of RAM, no direct call into X from the flash; pointer calls inside X counted
    ram_lo, ram_hi = RANGE_X[0], RAM_TOP
    x_out, into_x, x_ind = [], [], 0
    for f, ins in fns.items():
        for a, t in ins:
            tgt = dis_target(a, t)
            in_x = xlo <= a < xhi
            if in_x and re.search(r"\bcall\s+r\d+\b|\bgoto\s+r\d+\b|\bpc\s*=\s*r\d+", t):
                x_ind += 1
            if tgt is None:
                continue
            if in_x and not (ram_lo <= tgt < ram_hi):
                x_out.append(f"{f} -> {tgt:#x}")
            if a >= 0x02000000 and xlo <= tgt < xhi:
                into_x.append(f"{f} -> {tgt:#x}")
    if x_out:
        errors.append(f"range X calls the flash: {x_out[:4]}")
    if into_x:
        errors.append(f"the flash calls range X directly: {into_x[:4]}")
    if x_ind:
        errors.append(f"range X makes {x_ind} calls through a pointer (they could reach the flash)")
    head = RAM_TOP - (rt[0] + rt[1]) if rt else 0
    if rt and head < HOT_HEADROOM:
        errors.append(f"hot block headroom {head} B < {HOT_HEADROOM} B")
    notes.append(f"range X {xlo:#010x}..{xhi:#010x} {xhi - xlo} B of {RANGE_X[1] - RANGE_X[0]} ({RANGE_X[1] - xhi} B free): closed (no call "
                 f"leaves RAM, none through a pointer, none from the flash); range Z {zlo:#010x}..{zhi:#010x} {zhi - zlo} B "
                 f"({STACK_GUARD - zhi} B free up to the stack guard)")
    notes.append(f"start-up: fm1_cstart copies X and Z before any code there can run (before it only {', '.join(sorted(set(before))) or 'nothing'}, flash only); load images in the app image, equal to the sections; PC window "
                 f"{lo_pc:#010x}..{hi_pc - 1:#010x} covers X, Z and the hot block ({head} B hot headroom)")
    return errors, notes
