#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Build Felucca: the app, the update loader and an installable .fwsc package.

  tools/build.py [--release]

Outputs in build/: felucca.bin (app), loader/ota.bin (update loader),
felucca.fwsc (package). A release build (--release; the version is always the VERSION file) writes duell-<VERSION>-beta.fwsc
and a folder release-<VERSION>-beta/ with the package, the app, SHA256SUMS and the licence files (DUELL 0.2.9: no sample data, no sample
attribution any more); the device
shows the version of the VERSION file at the DUELL root ("DUELL 0.2.1 beta"); the package is duell-<VERSION>-beta.fwsc
(identity FM-1_901). Every build also writes duell-latest.fwsc (the same bytes) and duell-version.json {"version": "0.2.1"}.
See BUILDING.md for the toolchain and the SDK.

The JieLi toolchain is Linux x86-64 only. JIELI_TOOLCHAIN points at it; on
macOS (or with JIELI_DOCKER=1) each tool runs in a linux/amd64 container.

Compiler flags (DUELL bench builds; the defaults are unchanged): FELUCCA_OPT replaces the app's "-Os" (e.g. "-O2 -mcpu=r3 -mfprev1 -flto";
the update loader always keeps -Os), FELUCCA_LLVM adds -mllvm options (e.g. "-pi32v2-enable-simd=true"; with -flto they go to the
link's LLVM plugin instead). -flto: felucca.o is LLVM bitcode, linked through the toolchain's LLVMgold.so with the same -O level and
-mcpu. BENCH_TAG (with DUELL_BENCH=1, DUELL_TEST_MODE=1): the device shows "DUELL <VERSION> bench <TAG>" and the package is
duell-<VERSION>-bench-<TAG>.fwsc (no duell-latest.fwsc / duell-version.json then: a bench package is never the latest).
DUELL_BENCH=1 builds MENU > SYSTEM > BENCH (src/bench.c, docs/BENCH_GUIDE.md; bench suite stage 0: e.g. BENCH_TAG=BENCHC0 with
DUELL_RAMPLACE=1). DUELL_CORE1_PROBE=1 (BENCH-B's core 1 probe) is not in this line (core.h stops such a build).
DUELL_TWOCORE (default 1 in every non-bench build since 1.0, with DUELL_RAMPLACE=1; 0 = one core; not with DUELL_CORE1_PROBE): the second core plays part 2, started at every power-on and left alone for the session after a fault (docs/TWOCORE_STAGE1.md, CPU_PLAN.md).
DUELL_HOTRAM (default 3 in every build since 0.2.14; 0 = all in flash; 1, 2, 4..8 need a BENCH_TAG: the experiment builds R1..R8; 4..6 are R3 without some functions, 7 = group 1 only padded to R3's size, 8 = R3 without group 1): the functions of tools/hot_fns.py's groups 1..n run from RAM:
felucca.c is compiled as always, their sections .text.<name> are renamed .hotx.<name> in felucca_hot.o, and the link uses
firmware/app_hot.ld instead of app.ld (the RAM code block at the top of RAM, in call reach of the flash); the no-calls rule of
.ram_text then covers the flash driver only (below _hot_start), and tools/elfcheck.py checks the block.

DUELL_RAMPLACE=1 (two-core stage 1a; needs a BENCH_TAG and DUELL_HOTRAM 3): part 2's voice code in two unused RAM ranges, link-time only
(tools/ram_place.py): the functions of tools/ram_fns_x.py go to range X (0x01C00000, .ramx.*), those of tools/ram_fns_z.py to range Z
(after the pool, .ramz.*); the link uses firmware/app_ram.ld; -DDUELL_RAMPLACE=1 builds in the start-up copy and check (src/ramplace.c),
the wider PC window (hal/fm1_guard.h) and ABOUT's RAM CODE line. tools/elfcheck.py check_ramplace checks the result. Unset: nothing of it.

Every build (device, test, bench, release) ends with the start-up layout check (tools/elfcheck.py: the .noinit objects
whole and of their C size, no LTO-split "bootguard.0", the .noinit size, the entry point, the app slot); it only
reads the ELF, the package bytes are unchanged.
"""
import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

if os.environ.get("DUELL_BENCH") != "1":        # DUELL 1.0: a release (or any non-bench) build runs part 2 on core 1 by default (the automatic one-core fallback is in twocore.c);
    os.environ.setdefault("DUELL_TWOCORE", "1")  # DUELL_TWOCORE=0 / DUELL_RAMPLACE=0 in the environment give the one-core build (the reference of the tests); set before the
    os.environ.setdefault("DUELL_RAMPLACE", "1") # tools imported below read the environment at import (tools/ram_place.py, hot_fns.py)
    os.environ.setdefault("DUELL_HOTRAM", "3")

SRC = Path(__file__).resolve().parents[1]
FW = SRC / "firmware"
OUT = SRC / "build"
GEN = OUT / "gen"
LDR = OUT / "loader"
sys.path.insert(0, str(SRC / "tools"))
import elfcheck  # noqa: E402
import hot_fns  # noqa: E402
import ram_place  # noqa: E402
import fm1pkg_make  # noqa: E402
import lz4blk  # noqa: E402

APP_XIP = 0x02000120                # app.bin offset 0 in the XIP map; the SPL jumps here
APP_SLOT = fm1pkg_make.APP_SLOT
LOADER_LOAD = 0x01C0A800
LOADER_NAME = b"usb_hid_ota.bin"    # the file name the SPL looks for
DOCKER_IMAGE = os.environ.get("JIELI_DOCKER_IMAGE", "debian:bookworm-slim")
CFLAGS = ["-Os", "-ffunction-sections", "-fno-builtin", "-Wall", "-Wno-unused-function"]
LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*\t(.*)$")

# SDK files of AC79NN_SDK_V1.2.1_2023-12-13 (the tested version)
SDK_SHA256 = {
    "uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
    "cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
    "cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d",
}

PRODUCT = "FM-1_900"                # package identity; release builds are FM-1_9XY
VERSION = None                      # unused: the version string comes from build/gen/duell_version.h (the VERSION file)


def read_version():
    """the single source of truth: DUELL/VERSION (0.MINOR.BUILD)"""
    v = (SRC.parent / "VERSION").read_text().strip()
    if not re.fullmatch(r"0\.\d+\.\d+|[1-9]\d*\.\d+", v):
        raise SystemExit(f"build: {SRC.parent / 'VERSION'} must hold 0.MINOR.BUILD or MAJOR.MINOR (1.0), found {v!r}")
    return v


def toolchain():
    tc = os.environ.get("JIELI_TOOLCHAIN")
    if not tc or not (Path(tc) / "pi32v2" / "bin" / "clang").exists():
        raise SystemExit("JIELI_TOOLCHAIN must point at the JieLi Linux toolchain "
                         "(the directory with pi32v2/ and common/; see tools/get_toolchain.sh)")
    return Path(tc).resolve()


def use_docker():
    native = platform.system() == "Linux" and platform.machine() in ("x86_64", "AMD64")
    return os.environ.get("JIELI_DOCKER", "0" if native else "1") == "1"


def tc(tool, *args):
    """run a toolchain binary (pi32v2/bin/..., common/bin/...) with cwd SRC; paths relative to SRC"""
    rel = [str(Path(a).resolve().relative_to(SRC)) if isinstance(a, Path) else a for a in args]
    if tool == "cc":                # the toolchain's cc wrapper needs python3; call clang directly
        tool, rel = "pi32v2/bin/clang", ["-target", "pi32v2", *rel]
    if use_docker():
        cmd = ["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{SRC}:/work",
               "-v", f"{toolchain()}:/opt/jieli:ro", "-w", "/work", DOCKER_IMAGE, f"/opt/jieli/{tool}", *rel]
    else:
        cmd = [str(toolchain() / tool), *rel]
    r = subprocess.run(cmd, cwd=SRC, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f"build: {tool} failed")
    if r.stderr.strip():
        sys.stderr.write(r.stderr)
    return r.stdout


def tc_all(*cmds):
    with ThreadPoolExecutor(len(cmds)) as ex:
        return list(ex.map(lambda c: tc(*c), cmds))


def generate():
    """generated headers (UI fonts, icons, keycaps, palettes, tables, factory sounds and multis; DUELL 0.2.9: no samples)"""
    GEN.mkdir(parents=True, exist_ok=True)
    for old in ("felucca_font.h", "felucca_icons.h", "felucca_samples.h"):   # headers of the bitmap font and icon atlas, the samples (0.2.9)
        (GEN / old).unlink(missing_ok=True)
    tools = SRC / "tools"
    cmds = [[tools / "gen_version.py", GEN / "duell_version.h"],   # the VERSION file -> DUELL_VERSION, DUELL_VERSION_STR
            [tools / "gen_aa_font.py", GEN / "ui_fonts.h", "--preset", "inter-tight"],
            [tools / "gen_aa_icons.py", GEN / "ui_icons.h"],
            [tools / "gen_aa_keycaps.py", GEN / "ui_keycaps.h"],
            [tools / "gen_ui_palettes.py", GEN / "ui_palettes.h"],
            [tools / "gen_splash_logo.py", GEN / "splash_logo.h"],   # the splash lockup mask (assets/splash_logo.svg)
            [tools / "gen_tables.py", GEN / "felucca_tables.h"],
            [tools / "gen_fm6_patches.py", GEN / "felucca_fm6.h"],
            [tools / "gen_duell_sounds.py", GEN / "duell_sounds.h"],   # DUELL factory sounds (tools/duell_sounds.py)
            [tools / "gen_midi_map.py", GEN / "midi_map_defaults.h"],   # the shipped MIDI map (docs/MIDI_MAP_DEFAULTS.csv; cloud job C: gen_only.py had it, this list did not)
            [tools / "gen_duell_multis.py", GEN / "duell_multis.h"]]  # DUELL factory multis M01..M04 (tools/factory_multis); M13..M16 too with DUELL_TEST_MODE=1 (env, as the -D below)
    procs = [subprocess.Popen([sys.executable, *map(str, c)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True) for c in cmds]
    failed = []
    for c, p in zip(cmds, procs):
        sys.stdout.write(p.communicate()[0])
        if p.returncode:
            failed.append(c[0].name)
    if failed:
        raise SystemExit(f"build: {', '.join(failed)} failed")


# ---- update loader

def crc16(d, c=0):
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else c << 1
        c &= 0xFFFF
    return c


def ldr_head(dcrc, a, b, attr, name):
    body = struct.pack("<HIIBBH16s", dcrc, a, b, attr, 0, 0, name)
    return struct.pack("<H", crc16(body)) + body


def ldr_wrap(image):
    """ota.bin = outer JLFS head (32 B) + inner head (32 B, load address) +
    blocks [clen u32][dlen u32][LZ4 block], dlen 4096 except the last"""
    blocks = bytearray()
    for i in range(0, len(image), 4096):
        raw = image[i:i + 4096]
        c = lz4blk.compress(raw)
        if len(c) > 4096 or lz4blk.decompress(c) != raw:
            raise SystemExit(f"loader: block {i // 4096} does not compress below 4096 B")
        blocks += struct.pack("<II", len(c), len(raw)) + c
    inner = ldr_head(crc16(image), len(image), LOADER_LOAD, 0, LOADER_NAME)
    body = inner + bytes(blocks)
    return ldr_head(crc16(body), 0x20, len(body), 0x41, LOADER_NAME) + body


def ldr_screen():
    """DUELL_LOADER_SCREEN=0: the loader without its screen (firmware/loader/ldr_screen.c compiles to nothing), the package named -noscreen"""
    return os.environ.get("DUELL_LOADER_SCREEN", "1") != "0"


def build_loader():
    LDR.mkdir(parents=True, exist_ok=True)
    src = FW / "loader"
    flags = [*CFLAGS, "-Ifirmware/hal", "-Ifirmware/src", f"-DLDR_SCREEN={1 if ldr_screen() else 0}"]
    tc_all(("cc", "-c", src / "crt0_ldr.S", "-o", LDR / "crt0_ldr.o"),
           ("cc", *flags, "-c", src / "loader.c", "-o", LDR / "loader.o"))
    elf = LDR / "loader.elf"
    tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", src / "loader.ld",
       LDR / "crt0_ldr.o", LDR / "loader.o", "-o", elf)
    _, dis, hdr, syms = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, LDR / "loader.bin"),
                               ("common/bin/objdump", "-d", elf),
                               ("common/bin/objdump", "-h", elf),
                               ("common/bin/objdump", "-t", elf))
    (LDR / "loader.dis").write_text(dis)
    for ln in hdr.splitlines():     # the loader rewrites the flash: nothing may run from XIP
        p = ln.split()
        if len(p) > 4 and p[1].startswith(".") and int(p[3], 16) >= 0x02000000 and int(p[2], 16):
            raise SystemExit(f"loader: section {p[1]} at {p[3]} is outside RAM")
    image = (LDR / "loader.bin").read_bytes()
    if not image or len(image) > 0x14000:
        raise SystemExit("loader: image empty or too big")
    ota = ldr_wrap(image)
    (LDR / "ota.bin").write_bytes(ota)
    bss = [int(ln.split()[0], 16) for ln in syms.splitlines() if ln.rstrip().endswith("_bss_end")]
    print(f"loader: image {len(image)} B at {LOADER_LOAD:#x}" + (f", bss end {bss[0]:#x}" if bss else ""))
    print(f"loader: ota.bin {len(ota)} B")
    return ota


# ---- app

def hotram():
    """DUELL_HOTRAM: 3 by default, in every build (0.2.14: R3, tools/hot_fns.py groups 1..3, the hottest audio functions run from RAM;
    DUELL_HOTRAM=0 builds the all-flash layout for a comparison). 1..2 and 4..8: the bench builds R1..R8 (hot_fns.WITHOUT / ONLY / PAD)"""
    v = os.environ.get("DUELL_HOTRAM", "3")
    if v not in ("0", "1", "2", "3", "4", "5", "6", "7", "8"):
        raise SystemExit(f"build: DUELL_HOTRAM {v!r}: 0..8")
    return int(v)


def ramplace():
    """DUELL_RAMPLACE: 1 = the two-core stage 1a RAM placement (tools/ram_place.py, firmware/app_ram.ld); unset or 0: none"""
    v = os.environ.get("DUELL_RAMPLACE", "0")
    if v not in ("0", "1"):
        raise SystemExit(f"build: DUELL_RAMPLACE {v!r}: 0 or 1")
    return v == "1"


def app_ld():
    """the app's linker script: app.ld, or app_hot.ld for a DUELL_HOTRAM build, app_ram.ld for DUELL_RAMPLACE=1"""
    if ramplace():
        return FW / "app_ram.ld"
    return FW / ("app_hot.ld" if hotram() else "app.ld")


def ld_regions():
    """the app linker script's MEMORY regions (app_ld()): name -> (origin, length)"""
    regs = {}
    for m in re.finditer(r"^\s*(\w+)\s*\([rwx]+\)\s*:\s*ORIGIN\s*=\s*(\w+),\s*LENGTH\s*=\s*(\w+)",
                         app_ld().read_text(), re.M):
        n = m.group(3)
        regs[m.group(1)] = (int(m.group(2), 0), int(n[:-1], 0) * 1024 if n.endswith("K") else int(n, 0))
    return regs


def opt_flags():
    """the app's optimisation flags: FELUCCA_OPT (default -Os) and FELUCCA_LLVM (-mllvm options)"""
    opt = os.environ.get("FELUCCA_OPT", "").split() or ["-Os"]
    llvm = os.environ.get("FELUCCA_LLVM", "").split()
    return opt, llvm


def opt_cpu(opt):
    return next((f.split("=", 1)[1] for f in opt if f.startswith("-mcpu=")), None)


def bench_tag():
    t = os.environ.get("BENCH_TAG", "")
    if t and not re.fullmatch(r"[A-Z0-9][A-Z0-9-]{0,6}", t):
        raise SystemExit(f"build: BENCH_TAG {t!r}: 1..7 of A-Z 0-9 -")
    if t and os.environ.get("DUELL_BENCH") != "1":
        raise SystemExit("build: BENCH_TAG needs DUELL_BENCH=1 (and DUELL_TEST_MODE=1)")
    if os.environ.get("DUELL_CORE1_PROBE") == "1" and not t:
        raise SystemExit("build: DUELL_CORE1_PROBE=1 is a bench build: set BENCH_TAG (and DUELL_BENCH=1, DUELL_TEST_MODE=1)")
    if os.environ.get("DUELL_TWOCORE") == "1" and os.environ.get("DUELL_CORE1_PROBE") == "1":
        raise SystemExit("build: DUELL_TWOCORE=1 and DUELL_CORE1_PROBE=1 both drive core 1: one at a time")
    if hotram() not in (0, 3) and not t:
        raise SystemExit("build: DUELL_HOTRAM 1, 2, 4..8 are bench builds: set BENCH_TAG (and DUELL_BENCH=1, DUELL_TEST_MODE=1)")
    if ramplace() and hotram() != 3:
        raise SystemExit("build: DUELL_RAMPLACE=1 needs the hot layout: DUELL_HOTRAM 3")
    return t


def build_app():
    opt, llvm = opt_flags()
    lto = "-flto" in opt
    base = [g for f in CFLAGS for g in (opt if f == "-Os" else [f])] + [a for o in ([] if lto else llvm) for a in ("-mllvm", o)]
    flags = [*base, "-Ifirmware/hal", "-Ifirmware/src", "-Ibuild/gen"]
    tag = bench_tag()
    if tag:
        flags += [f'-DBENCH_TAG="{tag}"', f'-DBENCH_FLAGS="{" ".join(opt + llvm)}"',
                  f'-DFELUCCA_VERSION="DUELL {read_version()} bench {tag}"']
    for flag in ("FELUCCA_FLASH", "FELUCCA_OTA", "FELUCCA_OTA_DRYRUN", "FELUCCA_OTA_RAMONLY", "FELUCCA_CDC",
                 "FELUCCA_UART", "FELUCCA_UAC", "FELUCCA_UAC_TONE", "FELUCCA_UAC_48K", "FELUCCA_ICONS",
                 "FELUCCA_CDC_DEFAULT", "DUELL_TEST_MODE", "DUELL_BENCH", "DUELL_CORE1_PROBE", "DUELL_TWOCORE", "DUELL_RAMPLACE"):
        v = os.environ.get(flag)    # unset: the default in firmware/src/felucca.c (DUELL_TEST_MODE: core.h, 0 = release: no TEST screen, no M13..M16)
        if v in ("0", "1"):
            flags.append(f"-D{flag}={v}")
    v = os.environ.get("DUELL_VELO", "1")   # KEY SENSE (key velocity and aftertouch, sense.c): ON in every build since 0.2.30 (owner, 2026-10-21; before: developer builds only, build.py passed nothing and felucca.c's default 0 held).
    if v in ("0", "1"):                     # DUELL_VELO=0 in the environment builds the old velocity-free shape (host tests compile without the switch: felucca.c's default stays 0)
        flags.append(f"-DDUELL_VELO={v}")
    v = os.environ.get("FELUCCA_USB_LAYOUT")   # firmware/src/usb.c: how the USB composite presents itself (#67), 0..3
    if v in ("0", "1", "2", "3"):
        flags.append(f"-DFELUCCA_USB_LAYOUT={v}")
    flags.append(f'-DFELUCCA_ID="{PRODUCT}"')
    # felucca.c goes to LLVM IR without the optimizer, the main-loop functions (UI, stores, editor) are
    # marked minsize (tools/size_fns.py), then the IR is compiled at -Os. FELUCCA_SIZE=0: -Os everywhere
    size = os.environ.get("FELUCCA_SIZE") != "0"
    cmain = (("cc", *flags, "-S", "-emit-llvm", "-Xclang", "-disable-llvm-optzns", "-c",
              FW / "src" / "felucca.c", "-o", OUT / "felucca.ll") if size else
             ("cc", *flags, "-c", FW / "src" / "felucca.c", "-o", OUT / "felucca.o"))
    tc_all(("cc", "-c", FW / "crt0.S", "-o", OUT / "crt0.o"),
           ("cc", "-c", FW / "hal" / "fm1_vec.S", "-o", OUT / "fm1_vec.o"),
           ("cc", "-c", FW / "hal" / "fm1_isr.S", "-o", OUT / "fm1_isr.o"),
           cmain)
    if size:
        subprocess.run([sys.executable, SRC / "tools" / "size_fns.py", OUT / "felucca.ll", OUT / "felucca_size.ll"],
                       check=True)
        tc("cc", *[f for f in flags if not f.startswith(("-I", "-D", "-W"))], "-c",
           OUT / "felucca_size.ll", "-o", OUT / "felucca.o")
    obj = OUT / "felucca.o"
    if hotram():                    # DUELL_HOTRAM: the hot functions' sections renamed for app_hot.ld (tools/hot_fns.py)
        data, done, missing = hot_fns.hot_place(obj.read_bytes(), hotram())
        obj = OUT / "felucca_hot.o"
        obj.write_bytes(data)
        print(f"hotram:  level {hotram()}: {len(done)} functions to RAM" +
              (f"; inlined on the device (no section, they go with their callers): {', '.join(missing)}" if missing else ""))
    if ramplace():                  # DUELL_RAMPLACE: ranges X and Z (tools/ram_place.py), after the hot set
        data, done, missing = ram_place.place(obj.read_bytes())
        if missing:
            raise SystemExit(f"ramplace: no section of their own (inlined on the device; take them off the lists): {', '.join(missing)}")
        perr, pnotes = ram_place.check_object(data, hot_fns.names(hotram()))
        for n in pnotes:
            print(f"ramplace: {n}")
        if perr:
            raise SystemExit("ramplace: " + "; ".join(perr))
        obj = OUT / "felucca_ram.o"
        obj.write_bytes(data)
        print(f"ramplace: {len(done['X'])} functions to range X, {len(done['Z'])} to range Z")
    elf = OUT / "felucca.elf"
    plugin = []
    if lto:                         # felucca.o is bitcode: the LLVM plugin optimises and generates it at the link
        level = next((f[2:] for f in opt if re.fullmatch(r"-O[0-3sz]", f)), "2")
        plugin = ["-plugin", "/opt/jieli/pi32v2/bin/LLVMgold.so" if use_docker() else str(toolchain() / "pi32v2/bin/LLVMgold.so"),
                  f"-plugin-opt=O{level if level in '0123' else '2'}"]
        if opt_cpu(opt):
            plugin.append(f"-plugin-opt=mcpu={opt_cpu(opt)}")
        plugin += [f"-plugin-opt={o}" for o in llvm]
    pad = [f"--defsym=_hot_pad={hot_fns.PAD[hotram()]}"] if hotram() in hot_fns.PAD else []   # R7: the hot block padded to R3's size
    tc("pi32v2/bin/ld", *plugin, *pad, "-T", app_ld(), OUT / "crt0.o", OUT / "fm1_vec.o", OUT / "fm1_isr.o",
       obj, "-o", elf)
    for sect in ("text.bin", "data.bin", "ramtext.bin", "ramx.bin", "ramz.bin"):
        (OUT / sect).unlink(missing_ok=True)
    dcpu = [f"-mcpu={opt_cpu(opt)}"] if opt_cpu(opt) else []   # (the disassembler knows that core's instructions only when told)
    rp = [("common/bin/objcopy", "-O", "binary", "-j", s, elf, OUT / b) for s, b in ((".ramx", "ramx.bin"), (".ramz", "ramz.bin"))
          ] if ramplace() else []
    *_, syms, dis, rt = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, OUT / "text.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".data", elf, OUT / "data.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".ram_text", elf, OUT / "ramtext.bin"),
                               *rp,
                               ("common/bin/objdump", "-t", elf),
                               ("common/bin/objdump", "-d", *dcpu, elf),
                               ("common/bin/objdump", "-d", *dcpu, "-j", ".ram_text", elf))
    (OUT / "felucca.dis").write_text(dis)

    def symv(name):
        return int(re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M).group(1), 16)
    img = bytearray((OUT / "text.bin").read_bytes())
    # .ram_text and .data follow .text at their load addresses; crt0 copies them by words (DUELL_RAMPLACE: then .ramz and .ramx, main.c)
    loads = [("ramtext.bin", "_rt_load"), ("data.bin", "_data_load")]
    if ramplace():
        loads = sorted(loads + [("ramz.bin", "_rz_load"), ("ramx.bin", "_rx_load")], key=lambda sl: symv(sl[1]))
    for sect, lname in loads:
        load = symv(lname)
        if load % 4:
            raise SystemExit(f"{lname} {load:#x} is not word aligned")
        blob = (OUT / sect).read_bytes() if (OUT / sect).exists() else b""
        if blob:
            if load - APP_XIP < len(img):
                raise SystemExit(f"{lname} overlaps the image")
            img += b"\xff" * (load - APP_XIP - len(img))
            img += blob
    img += b"\xff" * (-len(img) % 4)
    (OUT / "felucca.bin").write_bytes(img)
    return bytes(img), syms, dis, rt


POOL_TAIL_MIN = 8192        # the pool-tail rule: pool end (or range Z's end) to the stack guard keeps this much spare
POOL_TAIL_MIN_BENCH = 4096  # 4,096 B again (owner, 2026-10-19; it was 2,816 B for BENCHM1 until the T0229 ramp code was shrunk and mix_ramp moved to range X, join0234) ... in a DUELL_BENCH developer build ONLY (owner's decision 2026-10-19: the bench's 4.4 KB session copy and the
                            # two-core RAM code share the pool; BENCHT1 keeps 4,788 B). A normal or non-bench build keeps POOL_TAIL_MIN.


def pool_tail_min(env=None):
    env = os.environ if env is None else env
    return POOL_TAIL_MIN_BENCH if env.get("DUELL_BENCH") == "1" and env.get("BENCH_TAG") else POOL_TAIL_MIN


def check(img, syms, dis, rt):
    errors, notes = [], []
    m = re.search(r"^([0-9a-f]+) .*\s_start$", syms, re.M)
    if not m or int(m.group(1), 16) != APP_XIP:
        errors.append(f"_start is not at {APP_XIP:#x}")
    if img[:4] != bytes.fromhex("04818000"):
        errors.append(f"image starts with {img[:4].hex()}, not the entry stub")
    hm = re.search(r"^([0-9a-f]+) .*\s_hot_start$", syms, re.M)
    hot = int(hm.group(1), 16) if hm else None     # (DUELL_HOTRAM: the hot audio code follows the flash driver in .ram_text)
    drv = [ln for ln in rt.splitlines() if LINE.match(ln) and (hot is None or int(LINE.match(ln).group(1), 16) < hot)]
    rt_calls = [ln for ln in drv if re.search(r"\bcall\b", ln)]
    if rt_calls:                    # the flash driver runs with the flash off: no calls into XIP
        errors.append(f".ram_text contains calls: {rt_calls[:3]}")
    else:
        notes.append(f".ram_text: {len(drv)} insns, no calls" + ("" if hot is None else
                     f" (the flash driver; then {len([ln for ln in rt.splitlines() if LINE.match(ln)]) - len(drv)} insns of hot audio code)"))
    for ln in dis.splitlines():     # nothing may call or load an address in the chip ROM
        mm = LINE.match(ln)
        if not mm:
            continue
        for v in re.findall(r"= (-?\d+) <|call -?\d+ <[^:>]*: ([0-9a-f]+) >", mm.group(3)):
            val = (int(v[0]) & 0xFFFFFFFF) if v[0] else int(v[1], 16) & 0xFFFFFFFF
            if 0xFFC00000 <= val < 0xFFD00000:
                errors.append(f"reference to ROM address {val:#010x}")
    if len(img) > APP_SLOT:
        errors.append(f"image {len(img)} B exceeds the app slot")

    def sym(name):
        mm = re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M)
        return int(mm.group(1), 16) if mm else 0
    region = ld_regions()
    ram_org, ram_len = region["RAM"]
    pool_len = region["POOL"][1]
    bss = sym("_bss_end") - ram_org
    pool = sym("_pool_end") - sym("_pool_start")
    notes.append(f"image {len(img)} B; RAM .data+.bss {bss} B of {ram_len}; pool {pool} B of {pool_len}")
    if region["RAM"][0] + ram_len != region["POOL"][0]:
        errors.append("app.ld: POOL does not start where RAM ends")
    if ram_len - bss < 4096:                      # keep >= 4 KiB of .bss spare (state the audio side touches)
        errors.append(f"RAM headroom {ram_len - bss} B < 4096 B")
    if sym("_rz_end"):                            # DUELL_RAMPLACE: range Z takes the pool's spare end
        notes.append(f"pool + range Z {sym('_rz_end') - sym('_pool_start')} B of {pool_len} (range Z {sym('_rz_end') - sym('_rz_start')} B)")
        pool = sym("_rz_end") - sym("_pool_start")
    need = pool_tail_min()
    if pool_len - pool < need:                    # keep >= 8 KiB of the pool spare (4 KiB in a DUELL_BENCH developer build)
        errors.append(f"pool headroom {pool_len - pool} B < {need} B")
    elif need != POOL_TAIL_MIN:
        notes.append(f"pool tail rule lowered to {need} B for this DUELL_BENCH developer build (normal builds keep {POOL_TAIL_MIN} B)")
    return errors, notes


# Every hardware register access lives in hal/. In src/ and loader/ (comments stripped):
#  - no volatile pointer cast, except of a C object's address (`*(volatile T *)&x`: a read-once
#    of a RAM flag shared with an ISR, e.g. usb.c ota_wire_send);
#  - no literal in a register or reserved window (core SFRs 0x10000-0x13FFF, SFC 0x40000-0x43FFF,
#    GPIO/IOMAP 0x50000-0x51FFF, CPU 0x1EE0000-0x1EEFFFF, RAM top 0x01C7F000- (boot info,
#    mailbox, vectors), XIP 0x02000000-0x020FFFFF);
#  - no inline asm, except the empty compiler barrier RING_PUBLISH() (emits no instruction);
#  - no HAL register macro (a hal/ #define that is, or expands to, a volatile access).
# Linker symbols (_bss_start[], _rt_load[], ...) are plain C objects and pass.
MMIO_LIT = re.compile(r"\b0x0*(1[0-3][0-9a-f]{3}|4[0-3][0-9a-f]{3}|5[01][0-9a-f]{3}|1ee[0-9a-f]{4}|"
                      r"1c7f[0-9a-f]{3}|20[0-9a-f]{5})u?l?\b", re.I)
MMIO_CAST = re.compile(r"\(\s*(?:const\s+)?volatile\b[^()]*\*\s*\)(?!\s*&)")
MMIO_ASM = re.compile(r"\b(?:__asm__|asm)\b(?!\s+volatile\s*\(\s*\"\"\s*:::\s*\"memory\"\s*\))")


def mmio_check():
    def strip(s):
        s = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), s, flags=re.S)
        return re.sub(r"//[^\n]*", "", s)
    defs = {}
    for h in sorted((FW / "hal").glob("*.h")):
        for m in re.finditer(r"^#define\s+(\w+)(?:\([^)]*\))?\s+(.*)$", strip(h.read_text()), re.M):
            defs[m.group(1)] = m.group(2)
    regs = {n for n, b in defs.items() if re.search(r"\bvolatile\b", b)}
    while True:                                   # macros built on register macros (FM1_WR_LIMIT_H -> FM1_X2)
        more = {n for n, b in defs.items() if n not in regs and set(re.findall(r"\w+", b)) & regs}
        if not more:
            break
        regs |= more
    errors = []
    for f in sorted([*(FW / "src").glob("*.[ch]"), *(FW / "loader").glob("*.c")]):
        for no, ln in enumerate(strip(f.read_text()).splitlines(), 1):
            where = f"{f.relative_to(FW)}:{no}"
            for rx, what in ((MMIO_LIT, "register/window address"), (MMIO_CAST, "volatile pointer cast"),
                             (MMIO_ASM, "inline asm")):
                m = rx.search(ln)
                if m:
                    errors.append(f"{where}: {what} outside hal/ ({m.group(0).strip()}); add a hal/ helper")
            used = set(re.findall(r"\b[A-Z_][A-Z0-9_]*\b", ln)) & regs
            if used:
                errors.append(f"{where}: HAL register macro {sorted(used)[0]} outside hal/; use a hal/ helper")
    return errors


def main():
    global PRODUCT
    ap = argparse.ArgumentParser()
    ap.add_argument("--release", nargs="?", const="", metavar="X.Y", help="release build: a folder release-<VERSION>-beta/ too. The version is always the VERSION file; an X.Y given here is ignored")
    ap.add_argument("--sdk", type=Path, help="JieLi AC79 SDK checkout (default: $AC79_SDK)")
    a = ap.parse_args()
    ver = read_version()
    tag = f"{ver}-beta" if ver.startswith("0.") else ver    # 1.0 and later: duell-1.0.fwsc, no -beta
    if a.release:
        print(f"note: --release {a.release} ignored; the version is {ver} (the VERSION file)")
    btag = bench_tag()
    if btag and a.release:
        raise SystemExit("build: a bench build (BENCH_TAG) is never a release")
    name = f"duell-{ver}-bench-{btag}.fwsc" if btag else f"duell-{tag}.fwsc"   # the package of the VERSION file, release or not
    if not ldr_screen():
        name = name.replace(".fwsc", "-noscreen.fwsc")
    PRODUCT = "FM-1_901"                        # package identity (one digit each: the identity has room for two)
    fm1pkg_make.SDK = a.sdk
    for rel, sha in SDK_SHA256.items():          # fail early without the SDK
        if hashlib.sha256(fm1pkg_make.sdk_file(rel)).hexdigest() != sha:
            print(f"warning: SDK {rel} differs from AC79NN_SDK_V1.2.1; the package will not match the reference")
    OUT.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(2) as ex:
        gen, ldr = ex.submit(generate), ex.submit(build_loader)
        gen.result()
        ota = ldr.result()
    img, syms, dis, rt = build_app()
    errors, notes = check(img, syms, dis, rt)
    elf = elfcheck.read_elf((OUT / "felucca.elf").read_bytes())     # the start-up layout (tools/elfcheck.py)
    lay_err = elfcheck.check(elf, elfcheck.source_noinit(FW), "1" in (os.environ.get("DUELL_CORE1_PROBE"), os.environ.get("DUELL_TWOCORE")), len(img),
                             ld_regions()["NOINIT"])
    ram_err, ram_notes = elfcheck.check_ramcode(elf, bool(hotram()), ramplace())
    lay_err += ram_err
    notes += ram_notes
    if ramplace():                  # the RAM ranges X and Z (tools/elfcheck.py check_ramplace; the reach of every call: tools/ram_place.py)
        rp_err, rp_notes = elfcheck.check_ramplace(elf, img, dis, APP_XIP)
        r_err, r_note = ram_place.reach_check((OUT / "felucca_ram.o").read_bytes(), elf)
        lay_err += rp_err + r_err
        notes += rp_notes + [r_note]
    errors += lay_err
    if not lay_err:
        nsect = elf["sections"][".noinit"]
        notes.append(f"start-up layout: .noinit {nsect[1]:#x} B, " +
                     ", ".join(f"{n} {sz}" for _, n, sz in elfcheck.noinit_table(elf)) + "; entry, slot ok")
    hal_err = mmio_check()
    errors += hal_err
    if not hal_err:
        notes.append("register access: hal/ only (src/, loader/ clean)")
    for n in notes:
        print("  ok   ", n)
    for e in errors:
        print("  FAIL ", e)
    if errors:
        raise SystemExit("build: checks failed")
    pkg = fm1pkg_make.ufw(fm1pkg_make.flash_image(img, fm1pkg_make.KEY), ota, PRODUCT)
    (OUT / name).write_bytes(pkg)
    if not btag:
        (OUT / "duell-latest.fwsc").write_bytes(pkg)                  # stable name for the installer page
        (OUT / "duell-version.json").write_text(json.dumps({"version": ver}) + "\n")
    opt, llvm = opt_flags()
    print(f"flags    {' '.join(opt + llvm)}" + (f"  (bench {btag}: DUELL {ver} bench {btag})" if btag else ""))
    print(f"app      {OUT / 'felucca.bin'}  {len(img)} B")
    print(f"loader   {LDR / 'ota.bin'}  {len(ota)} B")
    print(f"package  {OUT / name}  {len(pkg)} B, identity {PRODUCT}")
    if a.release:                   # what a release carries: the package, the app and every licence they need
        rel = OUT / f"release-{tag}"
        shutil.rmtree(rel, ignore_errors=True)
        (rel / "LICENSES").mkdir(parents=True)
        app = f"duell-{tag}-app.bin"
        (rel / name).write_bytes(pkg)
        (rel / app).write_bytes(img)
        (rel / "SHA256SUMS").write_text("".join(f"{hashlib.sha256(b).hexdigest()}  {n}\n"
                                                for n, b in ((name, pkg), (app, bytes(img)))))
        for f in sorted((SRC / "LICENSES").glob("*.txt")):
            shutil.copy(f, rel / "LICENSES" / f.name)
        for doc in ("LICENSE", "LICENSING.md"):
            shutil.copy(SRC / doc, rel / doc)
        print(f"release  {rel}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
