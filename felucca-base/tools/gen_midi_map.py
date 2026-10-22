#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The shipped MIDI map (docs/MIDI_MAP_DEFAULTS.csv) -> a C header with the 128 default lines.

  tools/gen_midi_map.py [--csv FILE] [--sym NAME] OUT.h

The csv (docs/MIDI_MAP_DEFAULTS.csv, the map of docs/design/MIDI_MAP_DESIGN_2.md section 4) has one line per CC number:

  cc,target,part_rule,tag

  cc         0..127
  target     a name from the fixed list below: the P_ and G_ names of core.h (read from core.h), the ext.h aliases (P_TWTYPE ..)
             and the X_ slot names, the actions, LOCKED (the 23 locked numbers) or REMOVED
  part_rule  P (= CHANNEL, the part the CC's channel plays), PART1, PART2, G (= GLOBAL, any channel)
  tag        SHIPPED or LOCKED

Lines starting with # and blank lines are comments. REMOVED: the target that today sits on this number (the old numbers of the
24 CCs of cloud/base3, TODAY below) is dropped on purpose; the line puts nothing in the table and may sit beside the line that has
the number now (CC 20 was the MORPH fader: `20,REMOVED,P,SHIPPED` says it is gone, whatever the map puts on 20).

The generator FAILS (exit 1, a message on stderr) on: a duplicate CC, a value above 127, an unknown target, a locked standard
number given to a parameter (or a LOCKED line anywhere else), one of today's 24 shipped targets that appears nowhere (on its old
or its new number) unless a line says REMOVED on its old number; and on a target that would sit on two numbers (one CC moves
one parameter and one parameter has one CC), an unknown part rule or tag, and a rule the target cannot have.

The header holds `static const uint16_t SYM[128]` (SYM = MIDI_MAP_DEFAULTS), every line a MAP_LINE(...) of firmware/src/midi_map.h
written with the C names, so a renumbering of core.h moves nothing silently, and MIDI_MAP_REV (1..255, from the csv's rows): the
settings record keeps it, and a later shipped list fills only a number that is empty and was never cleared.
"""
import re
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "firmware" / "src"
DOCS_CSV = HERE.parents[1] / "docs" / "MIDI_MAP_DEFAULTS.csv"

# the actions (firmware/src/midi_map.h MAP_T_*), the fixed part of the list of targets
ACTIONS = ["MORPH", "ROLL_A", "ROLL_B", "UNDO", "ROLL", "AMOUNT", "TARGET", "SMART", "ARP", "PITCH", "MODE", "BALANCE", "XFADE", "SPLIT",
           "CUTOFF_SMART", "RES_SMART"]
SPECIAL = ["LOCKED", "REMOVED"]
GLOBAL_ONLY = {"MODE", "BALANCE", "XFADE", "SPLIT"}            # act on any channel, before the MIDI IN routing
EITHER = {"TARGET", "SMART", "ARP", "PITCH"}                   # global in effect, but heard only on a channel the routing hears (as today)
LOCKED_CC = [0, 1, 2, 4, 6, 11, 32, 38, 64] + list(range(96, 102)) + list(range(120, 128))   # MIDI_MAP_FINAL: 18 standard + the 5 inputs 1 2 4 11 64
assert len(LOCKED_CC) == 23

# the 24 CCs of cloud/base3 (midi_control.c: MIDI_CC_MAP 11, the morph's 9, the MODE's 4), on today's numbers
TODAY = {5: "P_GLIDE", 7: "P_LEVEL", 10: "P_PAN", 71: "RES_SMART", 72: "P_REL", 73: "P_ATK", 74: "CUTOFF_SMART", 75: "P_DEC",
         91: "P_REV", 93: "P_CHOR", 94: "P_DLY",
         20: "MORPH", 21: "ROLL_A", 22: "ROLL_B", 23: "UNDO", 25: "AMOUNT", 111: "TARGET", 113: "SMART", 114: "ARP", 115: "PITCH",
         112: "MODE", 116: "BALANCE", 117: "XFADE", 118: "SPLIT"}
assert len(TODAY) == 24

RULES = {"P": 0, "CHANNEL": 0, "PART1": 1, "PART2": 2, "G": 3, "GLOBAL": 3}


class MapError(Exception):
    pass


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", " ", s, flags=re.S)
    return re.sub(r"//[^\n]*", " ", s)


def enum_names(text, first):
    """The enumerators of the enum that starts with `first`, in order (no explicit values in these enums)."""
    m = re.search(r"enum\s*\{\s*(" + first + r"\b.*?)\}", text, flags=re.S)
    if not m:
        raise MapError(f"gen_midi_map: no enum starting with {first} in the sources")
    names = []
    for tok in m.group(1).split(","):
        tok = tok.strip()
        if not tok:
            continue
        if "=" in tok:
            raise MapError(f"gen_midi_map: {tok!r}: explicit enum values are not handled")
        names.append(tok)
    return names


def known_targets():
    """name -> C expression of its target code, read from core.h and ext.h."""
    core = strip_comments((SRC / "core.h").read_text())
    ext = strip_comments((SRC / "ext.h").read_text())
    out = {}
    for n in enum_names(core, "P_LEVEL"):
        if n != "P_COUNT":
            out[n] = n
    for n in enum_names(core, "G_BPM"):
        if n != "G_COUNT":
            out[n] = f"MAP_T_G({n})"
    for n in enum_names(ext, "X_ROUTE"):
        out[n] = f"MAP_T_X({n})"
    for m in re.finditer(r"^\s*#\s*define\s+(P_[A-Z0-9_]+)\s+(P_[A-Z0-9_]+)\s*$", ext, flags=re.M):
        if m.group(2) not in out:
            raise MapError(f"gen_midi_map: ext.h alias {m.group(1)} names {m.group(2)}, which core.h does not have")
        out[m.group(1)] = m.group(1)
    for a in ACTIONS:
        out[a] = f"MAP_T_{a}"
    return out


def read_csv(path):
    rows = []
    header = False
    for no, raw in enumerate(Path(path).read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        f = [x.strip() for x in line.split(",")]
        if not header:
            if f != ["cc", "target", "part_rule", "tag"]:
                raise MapError(f"{path}:{no}: the first line must be the header cc,target,part_rule,tag")
            header = True
            continue
        if len(f) != 4:
            raise MapError(f"{path}:{no}: four columns expected (cc,target,part_rule,tag): {raw!r}")
        if not re.fullmatch(r"\d+", f[0]):
            raise MapError(f"{path}:{no}: the CC {f[0]!r} is not a number")
        rows.append((no, int(f[0]), f[1], f[2], f[3]))
    if not header:
        raise MapError(f"{path}: no header line cc,target,part_rule,tag")
    return rows


def build(path):
    """-> (lines {cc: (cexpr, rule, who)}, rev). Raises MapError."""
    names = known_targets()
    rows = read_csv(path)
    seen = {}
    lines = {}
    removed = set()
    used = {}
    for no, cc, target, rule, tag in rows:
        where = f"{path}:{no}"
        if cc > 127:
            raise MapError(f"{where}: CC {cc} is above 127")
        if target == "REMOVED":                       # a statement about today's number, not a line of the map: it may sit beside the line that has the number now
            if cc not in TODAY:
                raise MapError(f"{where}: REMOVED on CC {cc}, which is not one of today's 24 CCs (nothing to remove)")
            if cc in removed:
                raise MapError(f"{where}: CC {cc} is REMOVED twice")
            removed.add(cc)
            continue
        if cc in seen:
            raise MapError(f"{where}: duplicate CC {cc} (first on line {seen[cc]})")
        seen[cc] = no
        if tag not in ("SHIPPED", "LOCKED"):
            raise MapError(f"{where}: unknown tag {tag!r} (SHIPPED or LOCKED)")
        if rule not in RULES:
            raise MapError(f"{where}: unknown part rule {rule!r} (P, PART1, PART2, G)")
        if target not in names and target not in SPECIAL:
            raise MapError(f"{where}: unknown target {target!r}")
        if cc in LOCKED_CC:
            if tag != "LOCKED" or target != "LOCKED":
                raise MapError(f"{where}: CC {cc} is a locked standard number and cannot be given to {target} (target and tag LOCKED)")
        elif tag == "LOCKED" or target == "LOCKED":
            raise MapError(f"{where}: CC {cc} is not one of the 23 locked numbers")
        if target == "LOCKED":
            lines[cc] = ("MAP_T_LOCKED", 0, 3)
            continue
        r = RULES[rule]
        if target in GLOBAL_ONLY or target.startswith("G_"):
            ok = r == 3
        elif target in EITHER:
            ok = r in (0, 3)
        else:
            ok = r in (0, 1, 2)
        if not ok:
            raise MapError(f"{where}: {target} cannot have the part rule {rule}")
        key = (target, r if r in (1, 2) else 0)
        if key in used:
            raise MapError(f"{where}: {target} is on CC {used[key]} already (one parameter has one CC)")
        used[key] = cc
        lines[cc] = (names[target], r, 0)
    present = {t for (t, _) in used}
    for old, target in sorted(TODAY.items()):
        if target not in present and old not in removed:
            raise MapError(f"{path}: today's CC {old} ({target}) is nowhere in the map: give it a line or say REMOVED on CC {old}")
    for cc in LOCKED_CC:
        if cc not in lines:
            raise MapError(f"{path}: the locked standard number {cc} has no LOCKED line")
    rev = zlib.crc32(";".join(f"{cc}:{lines[cc][0]}:{lines[cc][1]}:{lines[cc][2]}" for cc in sorted(lines)).encode()) & 0xFF or 1
    return lines, rev


def emit(lines, rev, sym, src):
    out = [f"/* generated by tools/gen_midi_map.py from {src}; do not edit */",
           f"#ifndef DUELL_{sym}_H", f"#define DUELL_{sym}_H",
           '#include "midi_map.h"', f"#define {sym}_REV {rev}u", f"static const uint16_t {sym}[MAP_N] = {{"]
    for cc in range(128):
        if cc in lines:
            t, r, w = lines[cc]
            out.append(f"    MAP_LINE({t}, {r}, {w}),   /* {cc} */")
        else:
            out.append(f"    MAP_EMPTY,   /* {cc} */")
    out.append("};")
    out.append(f"#define {sym}_LINES {len(lines)}u")
    out.append("#endif")
    return "\n".join(out) + "\n"


def main(argv):
    csv = DOCS_CSV
    sym = "MIDI_MAP_DEFAULTS"
    args = list(argv)
    out = None
    while args:
        a = args.pop(0)
        if a == "--csv":
            csv = Path(args.pop(0))
        elif a == "--sym":
            sym = args.pop(0)
        elif out is None:
            out = Path(a)
        else:
            sys.exit(f"gen_midi_map: unexpected argument {a!r}")
    if out is None:
        sys.exit(__doc__)
    try:
        lines, rev = build(csv)
    except MapError as e:
        sys.stderr.write(f"gen_midi_map: {e}\n")
        return 1
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(emit(lines, rev, sym, Path(csv).name))
    n = sum(1 for v in lines.values() if v[2] != 3)
    print(f"gen_midi_map: {csv.name}: {n} lines + {len(lines) - n} locked -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
