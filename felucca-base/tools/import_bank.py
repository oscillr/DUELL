#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 1.0 (owner, 2026-10-21 evening): the factory bank IS the 128 records of tools/factory_bank.duell-patches.json (the editor's "factory bank export").

    tools/import_bank.py [--write]  [FILE]

Reads the bank file (format duell-patches v1: per patch number, name, engine, category, crc32, the stored record in base64), checks every
record against its crc32, decodes it exactly as the firmware does (upreset.c: the sparse pairs on the STORE defaults, the extension block, End B,
the arp / keys block) and writes
  tools/duell_sounds.py            the 128 factory sounds in the form tools/gen_duell_sounds.py reads (idx = the old index of a sound of the same
                                   engine and name, else the next free one: favourites keep their meaning)
  tests/factory_bank_records.txt   one line per slot: NN crc32(8 hex) record(hex) trim: the record as the file has it, slot and revision bytes 0, and the TRIM the build gives
                                   it (tools/factory_bank_trims.json, the level balance; else the record's own); tests/bank_test.c
                                   loads each factory slot through the firmware, saves it and compares the bytes with these lines
Nothing of a patch is changed: values are copied, not tuned. A patch the sound list cannot hold is REPORTED (exit 1), never bent.
Without --write it only reports what it would do.
"""
import argparse
import base64
import importlib.util
import json
import re
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
DEFAULT_FILE = TOOLS / "factory_bank.duell-patches.json"
TRIMS_FILE = TOOLS / "factory_bank_trims.json"           # {"NN": TRIM}: the level balance (BANK_LEVELS.md): the only thing changed on top of the bank, the patch TRIM
CATS = ["", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "PERC", "FX", "OTHER"]       # category.c (the record's byte 6; the editor says DRUM for PERC)
PTF = dict(PAT=1, TR=2, FM6=4, MB=8, XB=16, AK=32, BM=64, CB=128)
AK_IDS = ["P_AMODE", "P_ARATE", "P_AOCT", "P_AGATE", "P_ASWING", "P_APROB", "P_AHOLD", "P_AORDER", None, "P_CHRD", "P_VOIC", "P_SLEN", "P_SDIV", "P_SSWING", "P_SGATE", "P_SQNT"]
STORE_ZERO = ["P_KTRK", "P_FREEA", "P_FREED", "P_L2TRIG", "P_LTRIG", "P_DKCRV", "P_DKSRC"]     # upreset.c up_def
TRACK_ONLY = {"LEVEL", "PAN", "MUTE", "QUANT", "TRANS", "SLEN", "SDIV", "SSWING", "SGATE", "SQNT"}
XB_WIDE = 128


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class Ctx:
    def __init__(self):
        self.gs = load(TOOLS / "gen_duell_sounds.py", "gen_duell_sounds")
        self.ids = self.gs.parse_ids()
        self.name_of = {v: k for k, v in self.ids.items()}
        self.tp, self.names = self.gs.parse_tp(self.ids)
        self.xtp = self.gs.parse_xtp(self.names)
        self.engines, self.order = self.gs.parse_engines()
        self.e0 = self.ids["P_E0"]
        self.pcount = max(self.ids.values()) + 1 + 1           # the P_ enum without P_COUNT, then P_COUNT (core.h); checked below against the record bytes' ids

    def param_default(self, eng, pid):
        if pid >= self.e0:
            rows = self.engines[eng]["edit"]
            return rows[pid - self.e0][3] if pid - self.e0 < len(rows) else 0
        return self.tp["" + self.name_of[pid]][3]

    def store_default(self, eng, pid):
        if pid == 0:
            return 104
        if pid < self.e0 and self.name_of[pid] in STORE_ZERO:
            return 0
        return self.param_default(eng, pid)


def parse_record(ctx, b):
    """-> dict(engine, cat, name, values{id: v} (the 119 + 8 stored ids on the STORE defaults, the arp / keys block put back), ext{slot: v}, mb, ak, fm6)"""
    if b[0] | b[1] << 8 != len(b):
        raise ValueError("length field")
    f = b[4]
    if not f & PTF["CB"] or not f & PTF["TR"]:
        raise ValueError("an old record form")
    eng = b[5]
    name = b[7:19].split(b"\0")[0].decode("ascii")
    o = 20
    ext = {}
    edits = []
    if f & PTF["XB"]:
        end = 21 + b[20]
        if b[21] != 1:
            raise ValueError("extension block version")
        p = 22
        while p < end:
            tag, n = b[p], b[p + 1]
            p += 2
            if tag == 1:
                for i in range(0, n, 2):
                    ext[b[p + i] - XB_WIDE] = b[p + i + 1] - 64
            elif tag == 2:
                edits += [(b[p + i], b[p + i + 1] - 64) for i in range(0, n, 2)]
            p += n
        o = end
    mb = None
    if f & PTF["MB"]:
        mb = bytes(b[o:o + 7])
        o += 7
    n = b[19]
    vals = {}
    if f & PTF["BM"]:
        j = 0
        for i in range(128):
            if b[o + (i >> 3)] >> (i & 7) & 1:
                vals[i] = b[o + 16 + j] - 64
                j += 1
        o += 16 + n
    else:
        for i in range(n):
            vals[b[o + 2 * i]] = b[o + 2 * i + 1] - 64
        o += 2 * n
    pat = bool(f & PTF["PAT"])
    o += 32 if pat else 0
    o += 112 if f & PTF["FM6"] else 0
    fm6 = bytes(b[o - 112:o]) if f & PTF["FM6"] else None
    ak = None
    if f & PTF["AK"]:
        ln = b[o] | b[o + 1] << 8
        c = b[o + 2:o + 2 + ln]
        g = c[0]
        grp = g & 7
        akv = {}
        for k, nm in enumerate(AK_IDS):
            og = 1 if k < 8 else 2 if k < 11 else 4
            if og & grp and nm:
                akv[ctx.ids[nm]] = ctx.param_default(eng, ctx.ids[nm])
        p = 2
        if g & 8:
            bm = c[1] | c[2] << 8
            p = 3
            for k in range(16):
                if bm >> k & 1:
                    if AK_IDS[k]:
                        akv[ctx.ids[AK_IDS[k]]] = c[p] - 64
                    p += 1
        else:
            for _ in range(c[1]):
                akv[c[p]] = c[p + 1] - 64
                p += 2
        steps = 0
        if grp & 4:
            m = c[p] | c[p + 1] << 8
            steps = bin(m).count("1")
        ak = dict(groups=grp, values=akv, steps=steps)
    full = {}
    for pid in range(ctx.e0 + 8):
        full[pid] = ctx.store_default(eng, pid)
    full.update(vals)
    if ak:
        full.update(ak["values"])
    return dict(engine=eng, cat=b[6], name=name, values=full, ext=ext, edits=edits, mb=mb, ak=ak, fm6=fm6, pat=pat)


def sound_from_record(ctx, rec, idx):
    eng = rec["engine"]
    en = ctx.engines[eng]["name"]
    p = {}
    for pid in range(ctx.e0 + 8):
        nm = ctx.name_of.get(pid)
        v = rec["values"][pid]
        if pid == 0:
            continue                                            # the MIXER LEVEL: never in a record
        if pid >= ctx.e0:
            label = "E%d" % (pid - ctx.e0 + 1)
        else:
            if nm is None:
                continue
            label = nm[2:]
            if label in TRACK_ONLY:
                continue
        if v != ctx.param_default(eng, pid):
            p[label] = v
    stray = []
    for k in range(1, 9):                                       # the matrix as MODk tuples (the generator packs the used slots up)
        ks, kd, ka = (ctx.ids["P_M%d%s" % (k, x)] for x in ("SRC", "DST", "AMT"))
        src, dst, amt = rec["values"][ks], rec["values"][kd], rec["values"][ka]
        for x in ("SRC", "DST", "AMT"):
            p.pop("M%d%s" % (k, x), None)
        if src:
            p["MOD%d" % k] = (msrc_name(ctx, src), mdst_name(ctx, dst), amt)
        elif dst or amt:
            stray.append(k)
    used = [k for k in range(1, 9) if "MOD%d" % k in p]
    if used != list(range(1, len(used) + 1)):
        stray.append(-1)
    rec["stray"] = stray
    for nm in ("LTRIG", "L2TRIG"):                              # both LFO TRIGs are written, FREE or not (the sound audit's rule: a sound that leaves one out fails; the stored default is NOTE, the sound list's FREE)
        p[nm] = rec["values"][ctx.ids["P_" + nm]]
    # KEY TRK: a BASS / PERC / KICK sound without the pair gets 0 from the generator: write the value whenever it is not 0 (the generator's INIT is the params.c default 32)
    cat = CATS[rec["cat"]]
    if (cat in ("BASS", "PERC") or "KICK" in rec["name"]) and "KTRK" not in p:
        p["KTRK"] = rec["values"][ctx.ids["P_KTRK"]]
    for k, v in sorted(rec["ext"].items()):
        xn = next(n for n, x in ctx.xtp.items() if x[0] == XB_WIDE + k)
        wid, label, lo, hi, df, nl = ctx.xtp[xn]
        if v != df:
            grp = ctx.gs.XGROUP[xn[2:]]
            p[f"{grp} {label}"] = v
    s = dict(engine=en, idx=idx, name=rec["name"], cat=cat, p=p)
    mb = rec["mb"]
    rec["mb_ok"] = bool(mb) and mb[0] == ord("M") and mb[3] == mb[5] - 1
    if mb and mb[0] == ord("M"):
        seed, amt = mb[1] | mb[2] << 8, mb[5] - 1
        if seed:
            s["bseed"] = seed
        if amt != ctx.gs.CAT_BAMT[cat]:
            s["bamt"] = amt
    return s


def py(v):
    if isinstance(v, tuple):
        return "(" + ", ".join(py(x) for x in v) + ")"
    return repr(v)


def msrc_name(ctx, v):
    n = ctx.names["N_MSRC"]
    return n[v] if 0 <= v < len(n) else v


def mdst_name(ctx, v):
    n = ctx.names["N_MDST"]
    return n[v] if 0 <= v < len(n) else v


def write_sounds(ctx, sounds, path):
    L = ['# SPDX-License-Identifier: GPL-3.0-only',
         '"""DUELL factory sounds: the data (tools/gen_duell_sounds.py validates it and writes build/gen/duell_sounds.h).',
         '',
         'DUELL 1.0 (owner, 2026-10-21): the 128 factory sounds are the owner\'s bank, the records of tools/factory_bank.duell-patches.json as exported (no refinement,',
         'no retuning, the names of the file), written here by tools/import_bank.py. The earlier recipe list (SOUND_RECIPES.md, the refinement passes) is in git history.',
         'Every sound: engine, idx (its factory index within the engine: the identity favourites and the shadows use; APPEND-ONLY per engine), name, cat, p: the',
         'parameters that differ from INIT (params.c TP defaults and the engine\'s EDIT defaults; E1..E8 are the engine\'s eight; MOD1..MOD8 are (SRC, DST, AMT)',
         'matrix slots; extension slots as "<group> <label>"), bseed / bamt: End B (the roll\'s seed and the AMOUNT %). Do not edit by hand: import again.',
         '"""',
         '',
         '# CONVERT_TWIN: the engine filters of the earlier sound lists are the twin filter\'s already (tools/convert_twin.py, a second run is refused).',
         'CONVERT_TWIN = 1',
         '',
         'SOUNDS = [']
    for s in sounds:
        p = dict(s["p"])
        items = []
        for k in sorted(p, key=lambda x: (0 if x.startswith("MOD") else 1, x)):
            pass
        order = list(p)
        L.append("    dict(engine=%r, idx=%d, name=%r, cat=%r%s%s,%s" % (
            s["engine"], s["idx"], s["name"], s["cat"], ", bseed=%d" % s["bseed"] if "bseed" in s else "", ", bamt=%d" % s["bamt"] if "bamt" in s else "",
            "  # #%d" % s["number"]))
        L.append("         p={")
        row = []
        for k in order:
            v = p[k]
            row.append("%r: %s" % (k, py(v)))
        line = "            "
        for i, r in enumerate(row):
            if len(line) + len(r) > 150:
                L.append(line.rstrip())
                line = "            "
            line += r + ", "
        L.append(line.rstrip())
        L.append("         }),")
    L.append("]")
    Path(path).write_text("\n".join(L) + "\n")


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("file", nargs="?", default=str(DEFAULT_FILE))
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args(argv)
    ctx = Ctx()
    j = json.loads(Path(a.file).read_text())
    if j.get("format") != "duell-patches" or j.get("version") != 1:
        sys.exit("not a duell-patches v1 file")
    items = j["patches"]
    if len(items) != 128 or [it["number"] for it in items] != list(range(1, 129)):
        sys.exit("expected the 128 patches numbered 1..128")
    problems = []
    recs = []
    for it in items:
        b = base64.b64decode(it["record"])
        if "%08x" % zlib.crc32(b) != it["crc32"].lower():
            problems.append("#%d %s: the record fails its crc32" % (it["number"], it["name"]))
        try:
            r = parse_record(ctx, b)
        except (ValueError, IndexError, KeyError) as e:
            problems.append("#%d %s: the record does not read (%s)" % (it["number"], it["name"], e))
            continue
        if r["name"] != it["name"] or ctx.engines[r["engine"]]["name"] != it["engine"]:
            problems.append("#%d %s: the record's name / engine differ from the file's fields" % (it["number"], it["name"]))
        if r["pat"] or r["edits"] or (r["ak"] and r["ak"]["steps"]) or (r["ak"] and r["ak"]["groups"] & 4):
            problems.append("#%d %s: a pattern, hand-set B edits or SEQ steps (not representable in the sound list)" % (it["number"], it["name"]))
        recs.append((it, b, r))
    # idx: the old index of a sound of the same engine and name, else the next free one
    old = load(TOOLS / "duell_sounds.py", "old_sounds").SOUNDS if (TOOLS / "duell_sounds.py").exists() else []
    old_idx = {(s["engine"], s["name"]): s["idx"] for s in old}
    taken = {}
    sounds = []
    for it, b, r in recs:
        en = ctx.engines[r["engine"]]["name"]
        k = old_idx.get((en, r["name"]))
        if k is not None and (en, k) not in taken:
            taken[(en, k)] = r["name"]
    nxt = {}
    for it, b, r in recs:
        en = ctx.engines[r["engine"]]["name"]
        k = old_idx.get((en, r["name"]))
        if k is None or taken.get((en, k)) != r["name"]:
            n = nxt.get(en, 0)
            while (en, n) in taken:
                n += 1
            taken[(en, n)] = r["name"]
            nxt[en] = n + 1
            k = n
        s = sound_from_record(ctx, r, k)
        s["number"] = it["number"]
        if r["stray"]:
            problems.append("#%d %s: matrix slots with a DST or AMT but no SRC, or a gap in the used slots (%s): the sound list packs them" % (it["number"], r["name"], r["stray"]))
        if r["mb"] and not r["mb_ok"]:
            problems.append("#%d %s: End B block with a roll AMOUNT different from the knob AMOUNT or no mark" % (it["number"], r["name"]))
        sounds.append(s)
    # a gap in an engine's indices (old sounds that left the bank): compact, keeping the order of the old indices
    by_e = {}
    for s in sounds:
        by_e.setdefault(s["engine"], []).append(s)
    for en, lst in by_e.items():
        for n, s in enumerate(sorted(lst, key=lambda s: s["idx"])):
            s["idx"] = n
    sounds.sort(key=lambda s: s["number"])
    trims = {int(k): v for k, v in json.loads(TRIMS_FILE.read_text()).items()} if TRIMS_FILE.exists() else {}
    own_trim = {}
    for s, (it, b, r) in zip(sounds, recs):
        own_trim[s["number"]] = r["values"][ctx.ids["P_TRIM"]]
        if s["number"] in trims:
            s["p"]["TRIM"] = trims[s["number"]]
    changed = sum(1 for n, t in trims.items() if t != own_trim[n])
    print("import_bank: %d records read, %d problems, %d trims changed (tools/factory_bank_trims.json)" % (len(recs), len(problems), changed))
    for p in problems:
        print("  PROBLEM", p)
    if a.write and not problems:
        write_sounds(ctx, sounds, TOOLS / "duell_sounds.py")
        lines = []
        for it, b, r in recs:
            z = bytearray(b)
            z[2] = z[3] = 0
            lines.append("%02d %08x %s %d" % (it["number"], zlib.crc32(bytes(z)) & 0xFFFFFFFF, bytes(z).hex(), trims.get(it["number"], r["values"][ctx.ids["P_TRIM"]])))
        (ROOT / "tests" / "factory_bank_records.txt").write_text("\n".join(lines) + "\n")
        print("written: tools/duell_sounds.py, tests/factory_bank_records.txt")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
