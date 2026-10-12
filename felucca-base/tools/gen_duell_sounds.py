#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL factory sounds: validate tools/duell_sounds.py and write build/gen/duell_sounds.h.

    tools/gen_duell_sounds.py build/gen/duell_sounds.h      (tools/build.py generate(), DUELL/gen_only.py)
    tools/gen_duell_sounds.py --check                       (validate and print the table only)

A sound is a SPARSE list of (parameter, value) pairs on top of INIT: the track parameters' defaults (params.c TP)
and the engine's EDIT defaults (eng_*.c .edit). The names an author writes are resolved here, from the sources:
  - the engine's EDIT labels (eng_*.c .edit: ANALOG WAVE DTN MIX NOIS CUT RES DRV KTR), or E1..E8;
  - the P_ ids of core.h without the P_ (ATK DEC SUS REL LEVEL GLIDE VOICE CHOR DLY REV FTYPE E2FLT L2PIT ...);
  - the aliases in ALIAS below (ENV2 FLT, LFO2 RATE, ARP MODE, ...);
  - MOD1..MOD8: a matrix slot, (SRC, DST, AMT) with N_MSRC / N_MDST names (DST may also be an EDIT label: RES = E6).
Values: an int (range-checked against TP / the EDIT table), or a value name of that parameter (SAW, LP, LEG, UPDN, 1/16).
Errors (unknown name, value out of range, unknown value name, a parameter twice, a track-only parameter, MODW in the
matrix, a name over 12 characters, a bad category or engine, factory indices not 0..n-1 per engine) stop the build.

End B (morph.c morph_rebase_part): 'bamt' (the AMOUNT %, default by category: CAT_BAMT) and 'bseed' (1..65535, chosen by
tests/sound_audit.c SOUND_BSEED=1 so End B keeps A's loudness and roughly its brightness; absent: the hash of (engine, idx)).

Output (firmware/src/sounds.c reads it): DS_PAIR (2 bytes a pair), DS_SOUNDS (engine-major: sound k of engine e is
DS_SOUNDS[DS_ENG_AT[e] + k], k = its 'idx', the identity favourites, shadows and the End B seed use: append-only per
engine), DS_LIST (the browse order: category, then engine in ENGINE_ORDER, then name).
"""
import importlib.util
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "firmware" / "src"

ALIAS = {
    "ENV2 ATK": "E2ATK", "ENV2 DEC": "E2DEC", "ENV2 SUS": "E2SUS", "ENV2 REL": "E2REL",
    "ENV2 FLT": "E2FLT", "ENV2 SHP": "E2SHP",
    "AMP FLT": "ED_FLT", "AMP PIT": "ED_PIT", "AMP SHP": "ED_SHP",
    "LFO RATE": "LRATE", "LFO WAVE": "LWAVE", "LFO PHS": "LPHASE", "LFO FADE": "LFADE",
    "LFO TRIG": "LTRIG", "LFO PIT": "LD_PIT", "LFO FLT": "LD_FLT", "LFO SHP": "LD_SHP", "LFO AMP": "LD_AMP",
    "LFO2 RATE": "L2RATE", "LFO2 WAVE": "L2WAVE", "LFO2 PHS": "L2PHASE", "LFO2 FADE": "L2FADE",
    "LFO2 SYNC": "L2SYNC", "LFO2 TRIG": "L2TRIG", "LFO2 POL": "L2POL",
    "LFO2 PIT": "L2PIT", "LFO2 FLT": "L2FLT", "LFO2 SHP": "L2SHP",
    "ARP MODE": "AMODE", "ARP RATE": "ARATE", "ARP OCT": "AOCT", "ARP GATE": "AGATE", "ARP SWING": "ASWING",
    "ARP PROB": "APROB", "ARP HOLD": "AHOLD", "ARP ORDER": "AORDER",
    "DST": "DIST", "CHO": "CHOR", "INSERT": "ITYPE", "INS A": "IA", "INS B": "IB", "INS C": "IC", "INS MIX": "IMIX",
    "UF CUT": "FREEA", "UF RES": "FREEB",   # DUELL 0.2.2: the universal filter's CUT (-64..63, 0 OFF) and RES, on PHASE LOFI VOICE WHEEL GRAIN PHYS FM6 SLICE
    "FILTER": "FTYPE", "STEREO SPRD": "SPRD", "REVERB": "REV",   # (GRAIN has an EDIT label SPRD: write E6 there)
}
# a sound load leaves these to the track (ui.c param_kept), so a sound may not set them; P_LEVEL, the arp (AMODE..
# AORDER) and the chord keys (CHRD VOIC) are the exceptions: a factory sound sets its level (its loudness trim)
# and may lend the arp and the keys (ui.c ds_arp, ds_keys)
TRACK_ONLY = {"PAN", "MUTE", "ROOT", "QUANT", "TRANS", "SLEN", "SDIV", "SSWING", "SGATE", "SQNT"}
KEY_IDS = ["CHRD", "VOIC"]               # lent like the arp (ui.c ds_keys): a pad may set chord keys. No SCALE: the scale is the instrument's (core.h)
ARP_IDS = ["AMODE", "ARATE", "AOCT", "AGATE", "ASWING", "APROB", "AHOLD", "AORDER"]
CATS = ["BASS", "LEAD", "PAD", "PLUCK", "KEYS", "PERC", "FX", "OTHER"]
MAX_PAIRS = 64
# a factory sound's End B AMOUNT (%) by category (owner, 2026-10-11): conventional sounds move a little, pads (drones too)
# and effects more. A sound's own 'bamt' overrides it (by a small step, the reason in its recipe comment)
CAT_BAMT = {"BASS": 7, "KEYS": 11, "PLUCK": 11, "PERC": 11, "LEAD": 17, "PAD": 27, "FX": 31, "OTHER": 17}
NENG_TABLE = 16                   # DS_ENG_AT covers engine numbers 0..15 (favorites.c: 16 engines)


def die(msg):
    raise SystemExit(f"gen_duell_sounds: {msg}")


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", " ", s, flags=re.S)
    return re.sub(r"//[^\n]*", " ", s)


def name_arrays(text):
    """static const char *const N_X[] = {"A", "B", ...}; -> {N_X: [..]} (0-terminated lists end at the 0)"""
    out = {}
    for m in re.finditer(r"static const char \*const (\w+)\[[^\]]*\]\s*=\s*\{(.*?)\};", text, re.S):
        body = strip_comments(m.group(2))
        if "#" in body:
            continue
        out[m.group(1)] = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
    return out


def parse_ids():
    core = strip_comments((SRC / "core.h").read_text())
    m = re.search(r"enum\s*\{\s*P_LEVEL\b(.*?)P_COUNT", core, re.S)
    if not m:
        die("core.h: the P_ enum not found")
    ids = ["P_LEVEL"] + re.findall(r"\b(P_\w+)\b", m.group(1))
    return {n: i for i, n in enumerate(ids)}


def parse_tp(ids):
    """params.c TP[]: {P_id: (label, lo, hi, default, names or None)}"""
    text = (SRC / "params.c").read_text()
    names = name_arrays(text)
    tp = {}
    body = text[text.index("static const param_desc_t TP[P_COUNT]"):]
    body = body[:body.index("\n};")]
    for m in re.finditer(r"\[(P_\w+)\]\s*=\s*PD\(\"([^\"]*)\",\s*\w+,\s*(-?\w+),\s*(-?\w+),\s*(-?\w+)\)", body):
        def num(x):
            try:
                return int(x)
            except ValueError:
                return {"NSTEP": 64}.get(x)
        tp[m.group(1)] = (m.group(2), num(m.group(3)), num(m.group(4)), num(m.group(5)), None)
    for m in re.finditer(r"\[(P_\w+)\]\s*=\s*PE\(\"([^\"]*)\",\s*(\w+),\s*(-?\d+)\)", body):
        n = names[m.group(3)]
        tp[m.group(1)] = (m.group(2), 0, len(n) - 1, int(m.group(4)), n)
    for m in re.finditer(r"\[(P_\w+)\]\s*=\s*\{\"([^\"]*)\",\s*\w+,\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),", body):
        tp[m.group(1)] = (m.group(2), int(m.group(3)), int(m.group(4)), int(m.group(5)), None)
    for k in range(1, 9):
        tp[f"P_M{k}SRC"] = (f"SRC{k}", 0, len(names["N_MSRC"]) - 1, 0, names["N_MSRC"])
        tp[f"P_M{k}DST"] = (f"DST{k}", 0, len(names["N_MDST"]) - 1, 0, names["N_MDST"])
        tp[f"P_M{k}AMT"] = (f"AMT{k}", -64, 63, 0, None)
    return tp, names


def parse_engines():
    """engines.c ENGINES[] and ENGINE_ORDER, the eng_*.c tables: {index: {name, edit: [(label, lo, hi, def, names)]}}"""
    etext = (SRC / "engines.c").read_text()
    block = etext[etext.index("ENGINES[NENGINES] = {"):]
    block = block[:block.index("};")]
    sym = {}
    for m in re.finditer(r"&ENG_(\w+),\s*/\*\s*(\d+)", block):
        sym.setdefault(int(m.group(2)), []).append(m.group(1))
    ob = etext[etext.index("ENGINE_ORDER[NENG_SHOWN] = {"):]
    ob = strip_comments(ob[ob.index("{") + 1:ob.index("};")])
    order = [int(x) for x in re.findall(r"^\s*([\d, ]+)", ob, re.M) for x in re.findall(r"\d+", x)]
    tables = {}
    for f in sorted(SRC.glob("eng_*.c")):
        text = f.read_text()
        arrays = name_arrays(text)
        for m in re.finditer(r"static const engine_t ENG_(\w+)\s*=\s*\{(.*?)\n\};", text, re.S):
            b = m.group(2)
            nm = re.search(r"\.name\s*=\s*\"([^\"]+)\"", b)
            ed = b[b.index(".edit"):] if ".edit" in b else ""
            rows = []
            for r in re.finditer(r"\{\"([^\"]*)\",\s*(F_\w+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*(\w+)", ed):
                lab, lo, hi, df, nl = r.group(1), r.group(3).strip(), r.group(4).strip(), r.group(5).strip(), r.group(6)
                nlist = arrays.get(nl) if nl != "0" else None

                def num(x):
                    try:
                        return int(x)
                    except ValueError:
                        return None
                lo_, hi_, df_ = num(lo), num(hi), num(df)
                if hi_ is None and "SMP_NALL" in hi:
                    hi_ = 7                           # SMP_NALL - 1: 5 built-in sets + USR1..3 (eng_sample.c)
                if hi_ is None and nlist:
                    hi_ = len(nlist) - 1
                rows.append((lab, lo_, hi_, df_, nlist))
                if len(rows) == 8:
                    break
            tables[m.group(1)] = {"name": nm.group(1) if nm else m.group(1), "edit": rows}
    engines = {}
    for idx, syms in sym.items():
        for s in syms:
            if s in tables:
                engines[idx] = tables[s]
                break
    return engines, order


def parse_cats():
    text = strip_comments((SRC / "category.c").read_text())
    m = re.search(r"enum\s*\{\s*(CAT_NONE.*?)\}", text, re.S)
    lst = [x.strip() for x in m.group(1).split(",") if x.strip()]
    return {n[4:]: i for i, n in enumerate(lst) if n != "CAT_N"}


def load_data(path):
    spec = importlib.util.spec_from_file_location("duell_sounds", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def resolve_value(where, label, v, lo, hi, names):
    if isinstance(v, bool):
        v = int(v)
    if isinstance(v, str):
        if not names or v not in names:
            die(f"{where}: {label}: no value named {v!r}" + (f" (one of {', '.join(names)})" if names else ""))
        v = names.index(v)
    if not isinstance(v, int):
        die(f"{where}: {label}: value {v!r} is not an int or a value name")
    if lo is None or hi is None:
        die(f"{where}: {label}: the range could not be read from the sources: cannot validate")
    if not lo <= v <= hi:
        die(f"{where}: {label}: {v} out of range {lo}..{hi}")
    if not -128 <= v <= 127:
        die(f"{where}: {label}: {v} does not fit a byte")
    return v


def build(data):
    ids = parse_ids()
    tp, names = parse_tp(ids)
    engines, order = parse_engines()
    cats = parse_cats()
    eng_by_name = {e["name"]: i for i, e in engines.items()}
    hidden = {"DRUM", "DIGITAL"}
    sounds = []
    seen = {}
    for n, s in enumerate(data.SOUNDS):
        where = f"sound {n} ({s.get('name', '?')})"
        for key in ("engine", "idx", "name", "cat", "p"):
            if key not in s:
                die(f"{where}: no '{key}'")
        unknown = set(s) - {"engine", "idx", "name", "cat", "p", "mono", "recipe", "bamt", "bseed"}
        if unknown:
            die(f"{where}: unknown keys {sorted(unknown)}")
        name = s["name"]
        if not 0 < len(name) <= 12 or not all(32 <= ord(c) < 127 for c in name) or '"' in name or "\\" in name:
            die(f"{where}: the name must be 1..12 plain characters")
        if s["engine"] not in eng_by_name or s["engine"] in hidden:
            die(f"{where}: engine {s['engine']!r} unknown or hidden ({', '.join(sorted(eng_by_name))})")
        e = eng_by_name[s["engine"]]
        if s["cat"] not in CATS:
            die(f"{where}: category {s['cat']!r} not one of {CATS}")
        edit = engines[e]["edit"]
        if len(edit) != 8:
            die(f"{where}: {s['engine']}'s EDIT table could not be read (8 rows)")
        elabels = {r[0]: k for k, r in enumerate(edit)}
        pairs = {}

        def put(pid, v, label):
            if pid in pairs:
                die(f"{where}: {label} set twice")
            pairs[pid] = v

        for label, v in s["p"].items():
            m = re.fullmatch(r"MOD([1-8])", label)
            if m:
                k = int(m.group(1))
                if not (isinstance(v, tuple) and len(v) == 3):
                    die(f"{where}: {label} must be (SRC, DST, AMT)")
                src, dst, amt = v
                if src == "MODW":
                    die(f"{where}: {label}: MODW moves the MORPH fader now: no MODW routes in factory sounds")
                if isinstance(dst, str) and dst not in names["N_MDST"] and dst in elabels:   # (CUT SHP ...: the matrix's own)
                    dst = f"E{elabels[dst] + 1}"
                put(ids[f"P_M{k}SRC"], resolve_value(where, label + " SRC", src, *tp[f"P_M{k}SRC"][1:3],
                                                     names["N_MSRC"]), label)
                put(ids[f"P_M{k}DST"], resolve_value(where, label + " DST", dst, *tp[f"P_M{k}DST"][1:3],
                                                     names["N_MDST"]), label)
                put(ids[f"P_M{k}AMT"], resolve_value(where, label + " AMT", amt, -64, 63, None), label)
                continue
            em = re.fullmatch(r"E([1-8])", label)
            if label in elabels or em:
                k = elabels[label] if label in elabels else int(em.group(1)) - 1
                if label in elabels and (label in ALIAS or f"P_{label}" in ids):
                    die(f"{where}: {label!r} is both an EDIT label and a track parameter: write E{k + 1}")
                lab, lo, hi, df, nl = edit[k]
                put(ids["P_E0"] + k, resolve_value(where, label, v, lo, hi, nl), label)
                continue
            pname = "P_" + ALIAS.get(label, label)
            if pname not in ids or pname.startswith("P_E") and re.fullmatch(r"P_E\d", pname):
                die(f"{where}: unknown parameter {label!r}")
            if pname not in tp or tp[pname][1] == tp[pname][2] == 0 and tp[pname][0] == "-":
                die(f"{where}: {label!r} is a free / retired id")
            if pname[2:] in TRACK_ONLY:
                die(f"{where}: {label!r} belongs to the track (ui.c param_kept): a sound does not set it")
            lab, lo, hi, df, nl = tp[pname]
            put(ids[pname], resolve_value(where, label, v, lo, hi, nl), label)
        if s.get("mono"):
            vid = ids["P_VOICE"]
            if vid not in pairs:
                pairs[vid] = names["N_VOICE"].index("LEG")      # as the old presets' mono flag: LEGATO
        if len(pairs) > MAX_PAIRS:
            die(f"{where}: {len(pairs)} pairs, at most {MAX_PAIRS}")
        idx = s["idx"]
        key = (e, idx)
        if key in seen:
            die(f"{where}: {s['engine']} idx {idx} also used by {seen[key]}")
        seen[key] = name
        arp = any(ids["P_" + a] in pairs for a in ARP_IDS)
        keys = any(ids["P_" + a] in pairs for a in KEY_IDS)
        bamt, bseed = s.get("bamt", CAT_BAMT[s["cat"]]), s.get("bseed", 0)
        if not isinstance(bamt, int) or isinstance(bamt, bool) or not 1 <= bamt <= 40:
            die(f"{where}: bamt {bamt!r}: the End B AMOUNT is 1..40 (%)")
        if not isinstance(bseed, int) or isinstance(bseed, bool) or not 0 <= bseed <= 65535:
            die(f"{where}: bseed {bseed!r}: 1..65535 (0 / absent: the hash of (engine, idx))")
        sounds.append({"e": e, "idx": idx, "name": name, "cat": s["cat"], "mono": bool(s.get("mono")), "arp": arp,
                       "keys": keys, "pairs": sorted(pairs.items()), "recipe": s.get("recipe", ""), "bamt": bamt,
                       "bseed": bseed})
    for e in {x["e"] for x in sounds}:
        got = sorted(x["idx"] for x in sounds if x["e"] == e)
        if got != list(range(len(got))):
            die(f"{engines[e]['name']}: factory idx must be 0..{len(got) - 1} (append-only), got {got}")
    if len(sounds) > 255:
        die("more than 255 sounds: widen DS_LIST / DS_ENG_AT")
    sounds.sort(key=lambda x: (x["e"], x["idx"]))
    rank = {e: i for i, e in enumerate(order)}
    lst = sorted(range(len(sounds)), key=lambda g: (CATS.index(sounds[g]["cat"]), rank.get(sounds[g]["e"], 99),
                                                    sounds[g]["name"]))
    return sounds, lst, engines, cats, {v: k for k, v in ids.items()}


def emit(path, sounds, lst, engines, cats, idname):
    L = ["/* generated by tools/gen_duell_sounds.py from tools/duell_sounds.py: DUELL's factory sounds (read by",
         " * firmware/src/sounds.c). Do not edit: edit tools/duell_sounds.py */", "#pragma once", "",
         f"#define DS_N {len(sounds)}u", f"#define DS_NENG {NENG_TABLE}u"]
    npairs = sum(len(s["pairs"]) for s in sounds)
    L += [f"#define DS_NPAIRS {npairs}u", "",
          "static const ds_pair_t DS_PAIR[DS_NPAIRS ? DS_NPAIRS : 1u] = {"]
    at = 0
    for s in sounds:
        s["at"] = at
        L.append(f"    /* {engines[s['e']]['name']} {s['idx']} {s['name']} */")
        row = [f"{{{idname[i]}, {v}}}" for i, v in s["pairs"]]
        for i in range(0, len(row), 6):
            L.append("    " + ", ".join(row[i:i + 6]) + ",")
        at += len(s["pairs"])
    L += ["};", "", "/* engine-major: sound k of engine e = DS_SOUNDS[DS_ENG_AT[e] + k] (k: the factory index) */",
          "static const ds_sound_t DS_SOUNDS[DS_N ? DS_N : 1u] = {"]
    for s in sounds:
        fl = " | ".join(f for f, on in (("DSF_MONO", s["mono"]), ("DSF_ARP", s["arp"]), ("DSF_KEYS", s["keys"])) if on) or "0"
        L.append(f"    {{\"{s['name']}\", {s['e']}, {cats[s['cat']]}, {fl}, {len(s['pairs'])}, {s['bamt']}, {s['at']}, "
                 f"{s['bseed']}u}},"
                 f"   /* {s['cat']}; {s['recipe']} */")
    L += ["};"]
    eng_at = [sum(1 for s in sounds if s["e"] < e) for e in range(NENG_TABLE + 1)]
    L += ["static const uint8_t DS_ENG_AT[DS_NENG + 1u] = {" + ", ".join(map(str, eng_at)) + "};",
          "/* the browse order (BASS LEAD PAD PLUCK KEYS FX OTHER; within one by ENGINE_ORDER, then name): DS_SOUNDS indices */",
          "static const uint8_t DS_LIST[DS_N ? DS_N : 1u] = {" + ", ".join(map(str, lst)) + "};", ""]
    Path(path).write_text("\n".join(L))
    return npairs


def table(sounds, lst, engines):
    print(f"{'#':>3} {'NAME':<13}{'CAT':<6} {'ENGINE':<7}{'IDX':>4} {'PAIRS':>5}  ARP MONO BAMT BSEED")
    for n, g in enumerate(lst):
        s = sounds[g]
        print(f"{n + 1:>3} {s['name']:<13}{s['cat']:<6} {engines[s['e']]['name']:<7}{s['idx']:>4} {len(s['pairs']):>5}"
              f"  {'ON ' if s['arp'] else '-  '} {'Y' if s['mono'] else '-'}    {s['bamt']:>4} {s['bseed'] or 'hash':>5}")


def main(argv):
    data = load_data(Path(__file__).resolve().parent / "duell_sounds.py")
    sounds, lst, engines, cats, idname = build(data)
    if len(argv) > 1 and argv[1] != "--check":
        n = emit(argv[1], sounds, lst, engines, cats, idname)
        size = 2 * n + len(sounds) * (12 + 4) + sum(len(s["name"]) + 1 for s in sounds) + NENG_TABLE + 1
        print(f"gen_duell_sounds: {len(sounds)} sounds, {n} pairs, ~{size} B flash -> {argv[1]}")
    else:
        table(sounds, lst, engines)


if __name__ == "__main__":
    main(sys.argv)
