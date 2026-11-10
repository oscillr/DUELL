#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL 0.2.24: the owner's own edits of patches and multis become the FACTORY set (docs/OWNER_REFINE.md is the owner's guide).

    tools/import_owner_edits.py [--dry-run] [--audit] [--replace] FILE...

FILE is what the editor writes (any number, any mix):
  *.duell-patches.json   the PATCHES tab's "Export bank": the patches the owner SAVED in his slots (each a stored record, whole)
  *.duell-multis.json    the MULTIS tab's "Download file" (format duell-multis, version 1: records whole, base64, size, CRC-32, record version; 0.2.25)
  duell-backup-*.json    the Backup page's file: the patch store (the same records) and the multis (M01..M16 he SAVED)

What it does, with nothing of his changed (values are copied as they are; the audit is only REPORTED):
  sounds  a saved patch in slot N replaces the factory sound at browse position N (tools/duell_sounds.py order of the build, layout.json BROWSE) when its
          values differ from that factory sound: written to tools/owner_sounds.py (OWNER_SOUNDS, in duell_sounds.py's form, read by
          tools/gen_duell_sounds.py with_owner: whole sounds, so the name, category, every value and End B come from his record). A patch equal to the
          factory sound writes nothing, and an earlier entry for that slot is dropped (he undid it).
  multis  a saved multi in slot M replaces the factory record tools/factory_multis/Mnn.bin when its bytes differ (kept byte for byte, `owner`), listed in
          tools/owner_multis.py (read by tools/factory_multis.py apply_owner).
  --audit then regenerates the sound table and runs the factory sound audit (tests/sound_audit.c) and tools/factory_multis.py check, and prints the FAIL
          lines: his values are never changed to pass them.
  --replace  start from nothing (the default merges: a slot not in the files keeps its earlier owner entry).
End B: the record's End B block is imported as exact values (bend / bpin / bfm / bamt of the sound, tools/endb.py), so an End B the owner edited is kept.
Refused with a reason (never silently): a record that does not read, an engine different from the factory sound's (a sound's engine is its identity), a name that
another sound already has, the FM6 operator patch and SEQUENCER steps (not representable in duell_sounds.py: only the values are imported).
Run from anywhere; exit 1 on a refusal.
"""
import argparse
import base64
import importlib.util
import json
import re
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
import endb                                                          # the End B block (the firmware's format), shared with gen_duell_sounds.py / convert_endb.py
CATS = ["", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "PERC", "FX", "OTHER"]        # category.c (record byte; 0: none)
PTF = dict(PAT=1, TR=2, FM6=4, MB=8, XB=16, AK=32, BM=64, CB=128)   # MB (PT_VER 4): End B as values; XB and BM (16, 64) are PT_VER 3's and are refused
AK_NAMES = ["P_AMODE", "P_ARATE", "P_AOCT", "P_AGATE", "P_ASWING", "P_APROB", "P_AHOLD", "P_AORDER", "P_SCALE", "P_CHRD", "P_VOIC", "P_SLEN", "P_SDIV", "P_SSWING", "P_SGATE", "P_SQNT"]
STORE_ZERO = ["P_KTRK", "P_FREEA", "P_FREED", "P_L2TRIG", "P_LTRIG", "P_DKCRV", "P_DKSRC"]       # upreset.c up_def: the stored default is 0 (0.2.24: DUCK SRC too, its stored default stays PUMP)
RETIRED_ID = 127            # the SCALE (ordinal 8 of a record's keys group) is retired, its id 25 is the TWIN FILTER's RES 2 now (cloud job B): the value is read and dropped (no name for 127)
SKIP_NAMES = {"LEVEL", "PAN", "MUTE", "ROOT", "QUANT", "SLEN", "SDIV", "SSWING", "SGATE", "SQNT", "SCALE"}   # the track's own (gen_duell_sounds TRACK_ONLY)


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class Ctx:
    """the firmware's tables, read from its sources by the generators' own parsers"""

    def __init__(self):
        self.gs = load(TOOLS / "gen_duell_sounds.py", "gen_duell_sounds")
        self.fm = load(TOOLS / "factory_multis.py", "factory_multis")
        self.ids = self.gs.parse_ids()
        self.name_of = {v: k for k, v in self.ids.items()}
        self.tp, _ = self.gs.parse_tp(self.ids)
        self.engines, _ = self.gs.parse_engines()
        self.e0 = self.ids["P_E0"]
        self.layout = json.loads((ROOT / "web" / "editor" / "layout.json").read_text())
        self.browse = self.layout["BROWSE"]
        base = load(TOOLS / "duell_sounds.py", "duell_sounds").SOUNDS
        self.base = {(s["engine"], s["idx"]): s for s in base}
        self.eng_names = self.fm.ENGINES
        _, names = self.gs.parse_tp(self.ids)
        self.wlabel = self.gs.bend_labels(self.ids, self.gs.parse_xtp(names))   # {wide id: label} of the End B values ('bend' keys)
        self.xnames = set(self.gs.xtp_names(self.gs.parse_xtp(names)))      # every spelling of an extension slot (SUB LEVEL, TWIN LINK, X_SUBLVL ..)

    def default(self, eng, pid):
        """the value a sound has before its pairs (params.c, the engine's EDIT default); None: unknown"""
        if pid >= self.e0:
            rows = self.engines.get(eng, {}).get("edit", [])
            k = pid - self.e0
            return rows[k][3] if k < len(rows) else None
        t = self.tp.get(self.name_of[pid])
        return t[3] if t else None

    def rng(self, eng, w):
        """the stored range (min, max) of wide id w for engine eng: the unit's param_desc_of (layout.json is written from the firmware's own tables; web/test_ranges.mjs compares it with tests/rangedump.c)"""
        if w == 127:
            return 0, 0
        d = self.layout["XTP"][w - 128] if w >= 128 else self.layout["ENG"][eng]["edit"][w - self.e0] if w >= self.e0 else self.layout["TP"][w]
        return d["min"], d["max"]

    def bits(self, eng, w):
        lo, hi = self.rng(eng, w)
        n, b = hi - lo + 1, 0
        while b < 16 and (1 << b) < n:
            b += 1
        return b

    def store_default(self, eng, pid):
        if pid == 0:
            return 104
        if self.name_of[pid] in STORE_ZERO:
            return 0
        return self.default(eng, pid)


MBV_NEVER, wid_ok, get_bits = endb.MBV_NEVER, endb.wid_ok, endb.get_bits     # the wide values End B never differs in (morph.c mbv_never), the id check, the bit reader
PT_FIX, PT_VB, PT_WORST = 20, 20, 577       # upreset.c PT_VER 4: the fixed part, the values' bitmap, the biggest record


def mbv_size(ctx, b, o, avail, eng):
    """the length of the End B block (morph.c MBV, morph_bv_size) at b[o:], 0 when it is no block"""
    if avail < 22 or (b[o] & ~3) or b[o + 1] > 101 or (b[o] & 2 and eng != ctx.eng_names.index("FM6")):
        return 0
    nb = 0
    for w in range(160):
        if b[o + 2 + (w >> 3)] >> (w & 7) & 1:
            if not wid_ok(w) or w in MBV_NEVER:
                return 0
            nb += ctx.bits(eng, w)
    n = 22 + (nb + 7) // 8
    if b[o] & 1:
        if n + 20 > avail:
            return 0
        for w in range(160):
            if b[o + n + (w >> 3)] >> (w & 7) & 1 and not wid_ok(w):
                return 0
        n += 20
    if b[o] & 2:
        if n + 57 > avail:
            return 0
        at = (o + n) * 8
        for _ in range(65):
            v, at = get_bits(b, at, 7)
            if v > 99:
                return 0
        n += 57
    return n


def parse_record(ctx, b):
    """a stored record (firmware upreset.c PT_VER 4 / web/editor/patchrecord.js) -> dict(engine, cat, name, values{id: v}, mb (End B block bytes), steps, fm6, ext) or raises ValueError"""
    if len(b) < PT_FIX + PT_VB or len(b) > PT_WORST or b[0] | b[1] << 8 != len(b):
        raise ValueError("wrong size or length field")
    f = b[4]
    if not f & PTF["CB"]:
        raise ValueError("a record of a firmware before 0.2.14")
    if f & (PTF["XB"] | PTF["BM"]):
        raise ValueError("a record of an older DUELL version (PT_VER 3), which this firmware does not load")
    if f & ~(PTF["PAT"] | PTF["TR"] | PTF["FM6"] | PTF["MB"] | PTF["AK"] | PTF["CB"]):
        raise ValueError("unknown flags")
    eng = b[5]
    if eng >= len(ctx.eng_names):
        raise ValueError("unknown engine")
    name = b[7:19].split(b"\0")[0].decode("ascii", "replace")
    if not name or any(not 32 <= c <= 126 for c in b[7:19].split(b"\0")[0]):
        raise ValueError("no usable name")
    vals, ext, at, nv = {}, {}, (PT_FIX + PT_VB) * 8, 0
    for w in range(PT_VB * 8):                       # the values: a bit per wide id, each at the bits of its range, in wide id order
        if b[PT_FIX + (w >> 3)] >> (w & 7) & 1:
            if not wid_ok(w):
                raise ValueError("it stores a parameter that does not exist")
            code, at = get_bits(b, at, ctx.bits(eng, w))
            lo, hi = ctx.rng(eng, w)
            v = min(lo + code, hi)
            nv += 1
            if w < 127:
                vals[w] = v
            else:
                ext[w - 128] = v
    if nv != b[19]:
        raise ValueError("the value count does not match the bitmap")
    o = PT_FIX + PT_VB + (((at - (PT_FIX + PT_VB) * 8) + 7) >> 3)
    mb = None
    if f & PTF["MB"]:
        n = mbv_size(ctx, b, o, len(b) - o, eng) if o < len(b) else 0
        if not n:
            raise ValueError("the End B block is damaged")
        mb = bytes(b[o:o + n])
        o += n
    o += 32 if f & PTF["PAT"] else 0
    fm6 = bool(f & PTF["FM6"])
    o += 112 if f & PTF["FM6"] else 0
    steps = 0
    if f & PTF["AK"]:
        ln = b[o] | b[o + 1] << 8
        c = b[o + 2:o + 2 + ln]
        grp = c[0] & 7 if len(c) >= 3 and not c[0] & ~7 else 0
        bm = c[1] | c[2] << 8 if grp else 0
        if grp and not bm >> 8 & 1:                  # (the retired SCALE ordinal 8 in the bitmap: the unit ignores the whole block)
            ak = {}
            for k, nm in enumerate(AK_NAMES):
                if k != 8 and (1 if k < 8 else 2 if k < 11 else 4) & grp:
                    ak[ctx.ids[nm]] = ctx.default(eng, ctx.ids[nm])
            at = 24
            for k in range(16):
                if bm >> k & 1:
                    code, at = get_bits(c, at, ctx.bits(eng, ctx.ids[AK_NAMES[k]]))
                    lo, hi = ctx.rng(eng, ctx.ids[AK_NAMES[k]])
                    ak[ctx.ids[AK_NAMES[k]]] = min(lo + code, hi)
            if grp & 4:
                p = 3 + ((at - 24 + 7) >> 3)
                steps = bin(c[p] | c[p + 1] << 8).count("1")
            vals.update(ak)
    full = {}
    for pid in range(ctx.e0 + 8):
        d = ctx.store_default(eng, pid)
        if d is not None:
            lo, hi = ctx.rng(eng, pid)                  # (upreset.c pt_encode leaves out a value whose CODE equals the stored default's: the default fitted into the range, as a load fits it: FREED's stored 0 is 1 = 12 dB)
            full[pid] = min(max(d, lo), hi)
    full.update(vals)
    if not f & PTF["TR"]:                            # before 0.2.19: the LEVEL was the trim
        full[ctx.ids["P_TRIM"]] = full.get(0, 104)
    return dict(engine=eng, cat=b[6], name=name, values=full, mb=mb, steps=steps, fm6=fm6, ext=ext)


def sound_from_record(ctx, rec, factory):
    """the duell_sounds.py form of a record: p = every value off the INIT defaults (not the track's own), End B from the block"""
    eng = rec["engine"]
    p = {}
    for pid in sorted(rec["values"]):
        nm = ctx.name_of.get(pid)
        if nm is None or (pid < ctx.e0 and nm[2:] in SKIP_NAMES):
            continue
        d = ctx.default(eng, pid)
        if d is None:
            continue
        if rec["values"][pid] != d:
            p[("E%d" % (pid - ctx.e0 + 1)) if pid >= ctx.e0 else nm[2:]] = rec["values"][pid]
    xtp = ctx.gs.parse_xtp(ctx.gs.parse_tp(ctx.ids)[1])   # DUELL 1.0: the extension slots come from the RECORD (this used to copy the recipe's, which lost the owner's BEHIVE and every other changed slot)
    for slot, v in sorted(rec.get("ext", {}).items()):
        xn = next(n for n, x in xtp.items() if x[0] == 128 + slot)
        wid, label, lo, hi, df, nl = xtp[xn]
        if v != df:
            p["%s %s" % (ctx.gs.XGROUP[xn[2:]], label)] = v
    cat0 = CATS[rec["cat"]] if rec["cat"] else factory["cat"]       # (a BASS / PERC / KICK sound without KEY TRK gets 0 from the generator: write the record's value, 32 included)
    if (cat0 in ("BASS", "PERC") or "KICK" in rec["name"]) and "KTRK" not in p:
        p["KTRK"] = rec["values"][ctx.ids["P_KTRK"]]
    s = dict(engine=ctx.eng_names[eng], idx=factory["idx"], name=rec["name"], cat=CATS[rec["cat"]] if rec["cat"] else factory["cat"], p=p)
    if rec["mb"]:                                    # PT_VER 4: End B as exact values (the block of the record): 'bend' {label: End B value} (where it differs from the Original), 'bpin' (the marks of the values set by hand), 'bfm' (FM6 bytes), 'bamt' (the AMOUNT)
        d = endb.decode(rec["mb"], eng)
        if d["amt"]:
            s["bamt"] = d["amt"] - 1
        s["bend"] = {ctx.wlabel[w]: v for w, v in sorted(d["vals"].items())}
        if d["marks"]:
            s["bpin"] = [ctx.wlabel[w] for w in d["marks"]]
        if d["fm"] is not None:
            s["bfm"] = d["fm"]
    return s


def build_list(ctx, lst):
    """gen_duell_sounds.build over a whole sound list without the owner overlay -> {(engine index, idx): built sound}"""
    sounds = ctx.gs.build(type("D", (), {"SOUNDS": lst, "NO_OWNER": True}))[0]
    return {(x["e"], x["idx"]): x for x in sounds}


def same_sound(ctx, a, b):
    """equal as sounds: the same values (a pair equal to the default counts as absent), name, category, End B (the block of values, else the AMOUNT); a factory seed of 0 (the hash) matches any"""
    pa, pb = dict(a["pairs"]), dict(b["pairs"])
    eng = a["e"]
    for pid in set(pa) | set(pb):
        d = ctx.default(eng, pid) if pid < 255 else None
        if pa.get(pid, d) != pb.get(pid, d):
            return False
    if a.get("endb") or b.get("endb"):               # End B as values: the block is the End B (its AMOUNT byte too); the seed no longer counts
        return (a["name"], a["cat"], a["e"], a["idx"], a.get("endb")) == (b["name"], b["cat"], b["e"], b["idx"], b.get("endb"))
    return (a["name"], a["cat"], a["e"], a["idx"], a["bamt"]) == (b["name"], b["cat"], b["e"], b["idx"], b["bamt"]) and \
        (not a["bseed"] or not b["bseed"] or a["bseed"] == b["bseed"])


def read_files(paths):
    """-> (patch records {slot: bytes}, multi records {slot0: bytes}, notes)"""
    patches, multis, notes = {}, {}, []
    for path in paths:
        try:
            j = json.loads(Path(path).read_text())
        except Exception as e:
            notes.append("%s: not a JSON file (%s)" % (path, e))
            continue
        if j.get("format") == "duell-patches":
            for it in j.get("patches", []):
                if not isinstance(it.get("record"), str):
                    notes.append("%s: %s is a factory copy (values only, never saved): skipped" % (Path(path).name, it.get("name", "?")))
                    continue
                b = base64.b64decode(it["record"])
                if it.get("crc32") is not None and "%08x" % zlib.crc32(b) != str(it["crc32"]).lower().zfill(8):
                    notes.append("%s: %s fails its checksum: skipped" % (Path(path).name, it.get("name", "?")))
                    continue
                if not isinstance(it.get("number"), int):
                    notes.append("%s: %s has no slot number: skipped" % (Path(path).name, it.get("name", "?")))
                    continue
                patches[it["number"] - 1] = b
        elif j.get("format") == "duell-multis" and j.get("version") == 1:                      # web/editor/stored.js multiFile / parseMultiFile
            for it in j.get("multis", []):
                nm = str(it.get("name", "?"))
                try:
                    b = base64.b64decode(str(it["data"]), validate=True)
                except Exception:
                    notes.append("%s: %s is damaged in the file: skipped" % (Path(path).name, nm))
                    continue
                if len(b) != it.get("size") or zlib.crc32(b) != it.get("crc"):
                    notes.append("%s: %s fails its size or checksum: skipped" % (Path(path).name, nm))
                    continue
                if not isinstance(it.get("slot"), int) or not 1 <= it["slot"] <= 16:
                    notes.append("%s: %s has no slot 1..16: skipped" % (Path(path).name, nm))
                    continue
                if it.get("version") != b[0]:
                    notes.append("%s: %s: the file says record version %s, the record has %d: skipped" % (Path(path).name, nm, it.get("version"), b[0]))
                    continue
                multis[it["slot"] - 1] = bytes(b)
        elif j.get("format") in ("duell-backup", "felucca-backup"):
            objs = {}
            for o in j.get("objects", []):
                b = base64.b64decode(o.get("data", ""))
                if b and zlib.crc32(b) != o.get("crc"):
                    notes.append("%s: object %s fails its checksum: skipped" % (Path(path).name, o.get("id")))
                    continue
                if o.get("id") == 40:                                                    # a backup file v5: a patch record {id: 40, slot} (PT_VER 4), whole
                    if b and isinstance(o.get("slot"), int) and 0 <= o["slot"] < 128:
                        patches[o["slot"]] = bytes(b)
                    continue
                objs[o["id"]] = b
            best = {}
            for oid, b in objs.items():
                if oid in (6, 7, 9, 10, 11, 12) or 27 <= oid <= 31 or 35 <= oid <= 39:        # the patch store's chunks
                    if len(b) < 4 or b[0] | b[1] << 8 != 0x5450:
                        continue
                    off = 4
                    while off + 20 <= len(b):
                        rl = b[off] | b[off + 1] << 8
                        if rl < 20:
                            break
                        slot, rev = b[off + 2], b[off + 3]
                        old = best.get(slot)
                        if old is None or ((rev - old[0]) & 255 < 128 and rev != old[0]):
                            best[slot] = (rev, b[off:off + rl])
                        off += rl
            for slot, (_, rec) in best.items():
                patches[slot] = bytes(rec[:2]) + b"\0\0" + bytes(rec[4:])               # slot and revision are the unit's: zeroed, as PATCH_RAW reads
            for o in range(8):
                b = objs.get(13 + o, b"")
                if len(b) < 8 or b[:4] != b"MUL1":
                    continue
                l = [b[4] | b[5] << 8, b[6] | b[7] << 8]
                at = 8
                for h in range(2):
                    if l[h]:
                        multis[2 * o + h] = bytes(b[at:at + l[h]])
                    at += l[h]
        else:
            notes.append("%s: neither a duell-patches, duell-multis nor a duell-backup file: skipped" % path)
    return patches, multis, notes


def py(v, ind=0):
    if isinstance(v, dict):
        return "{" + ", ".join("%r: %s" % (k, py(x)) for k, x in v.items()) + "}"
    return repr(v)


def write_sounds(entries, path):
    lines = ['# SPDX-License-Identifier: GPL-3.0-only',
             '"""DUELL 0.2.24: the owner\'s own edits of factory sounds, written by tools/import_owner_edits.py from what his unit saved (docs/OWNER_REFINE.md).',
             'Each entry replaces the factory sound with the same (engine, idx) in tools/duell_sounds.py as the build sees it (tools/gen_duell_sounds.py with_owner).',
             'Do not edit by hand: import again, or delete an entry to go back to the recipe sound."""', "", "OWNER_SOUNDS = ["]
    for s in sorted(entries.values(), key=lambda s: (s["engine"], s["idx"])):
        tail = ""
        if "bend" in s:                                   # End B as values, in the form duell_sounds.py holds it (tools/endb.py sound_kwargs)
            tail = ",\n         " + ",\n         ".join(endb.sound_kwargs(s["bend"], s.get("bpin"), s.get("bfm")))
        lines.append("    dict(engine=%r, idx=%d, name=%r, cat=%r%s%s,\n         p=%s%s)," % (
            s["engine"], s["idx"], s["name"], s["cat"], ", bseed=%d" % s["bseed"] if "bseed" in s else "", ", bamt=%d" % s["bamt"] if "bamt" in s else "", py(s["p"]), tail))
    lines.append("]")
    Path(path).write_text("\n".join(lines) + "\n")


def write_multis(entries, path):
    lines = ['# SPDX-License-Identifier: GPL-3.0-only',
             '"""DUELL 0.2.24: multis the owner saved on his unit, written by tools/import_owner_edits.py (docs/OWNER_REFINE.md). The record of each is tools/factory_multis/Mnn.bin,',
             'kept byte for byte; tools/factory_multis.py apply_owner puts these in its list as `kept`, `owner` (exempt from the similarity check)."""', "", "OWNER_MULTIS = ["]
    for m in sorted(entries.values(), key=lambda m: m["slot"]):
        lines.append("    dict(slot=%d, name=%r, mode=%r, parts=[dict(sound=%r), dict(sound=%r)])," % (       # (no description, 0.2.25)
            m["slot"], m["name"], m["mode"], m["parts"][0]["sound"], m["parts"][1]["sound"]))
    lines.append("]")
    Path(path).write_text("\n".join(lines) + "\n")


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("files", nargs="+")
    ap.add_argument("--dry-run", action="store_true", help="report only, write nothing")
    ap.add_argument("--audit", action="store_true", help="then run the sound audit and the multi check and print the FAIL lines")
    ap.add_argument("--replace", action="store_true", help="start from no owner entries instead of merging")
    ap.add_argument("--out", default=str(TOOLS), help="where owner_sounds.py / owner_multis.py are written (default tools/)")
    a = ap.parse_args(argv)
    ctx = Ctx()
    out = Path(a.out)
    refused, report = [], []
    patches, multis, notes = read_files(a.files)
    report += notes
    # ---- sounds
    own = {} if a.replace or not (out / "owner_sounds.py").exists() else {(s["engine"], s["idx"]): s for s in load(out / "owner_sounds.py", "os_old").OWNER_SOUNDS}
    base_by_name = {s["name"]: k for k, s in ctx.base.items()}
    fm6_seen = steps_seen = unchanged = changed = 0
    cand = {}
    for slot in sorted(patches):
        if slot >= len(ctx.browse):
            refused.append("slot #%d: there are 128 slots" % (slot + 1))
            continue
        eng_i, idx = ctx.browse[slot]
        eng = ctx.eng_names[eng_i]
        fac = ctx.base.get((eng, idx))
        try:
            rec = parse_record(ctx, patches[slot])
        except (ValueError, IndexError, KeyError) as e:
            refused.append("slot #%d: the record does not read (%s)" % (slot + 1, e))
            continue
        if rec["engine"] != eng_i:
            refused.append("slot #%d %s: engine %s, the factory sound %s is %s (the engine is the sound's identity): not imported" % (
                slot + 1, rec["name"], ctx.eng_names[rec["engine"]], fac["name"] if fac else "?", eng))
            continue
        s = sound_from_record(ctx, rec, fac)
        other = base_by_name.get(s["name"])
        if other and other != (eng, idx):
            refused.append("slot #%d: the name %s is #%d's already: not imported" % (slot + 1, s["name"], ctx.browse.index([ctx.eng_names.index(other[0]), other[1]]) + 1))
            continue
        fm6_seen += rec["fm6"]
        steps_seen += bool(rec["steps"])
        cand[(eng, idx)] = (slot, s)
    base_list = list(ctx.base.values())
    fac_built = build_list(ctx, base_list)
    good = dict(cand)
    try:
        new_built = build_list(ctx, [cand[k][1] if k in cand else v for k, v in ctx.base.items()])
    except SystemExit:                                   # one of them breaks the build: find it, refuse it with the generator's own words
        good = {}
        for k, (slot, s) in cand.items():
            try:
                build_list(ctx, [s if kk == k else v for kk, v in ctx.base.items()])
                good[k] = (slot, s)
            except SystemExit as e:
                refused.append("slot #%d %s: %s" % (slot + 1, s["name"], e))
        new_built = build_list(ctx, [good[k][1] if k in good else v for k, v in ctx.base.items()])
    ekey = lambda k: (ctx.eng_names.index(k[0]), k[1])
    for k, (slot, s) in sorted(good.items(), key=lambda kv: kv[1][0]):
        e_new, e_fac = new_built[ekey(k)], fac_built[ekey(k)]
        if same_sound(ctx, e_new, e_fac):
            if k in own:
                del own[k]
                report.append("#%d %s: equals the recipe sound again, owner entry dropped" % (slot + 1, s["name"]))
            unchanged += 1
        else:
            fp = dict(e_fac["pairs"])
            diff = ["%s %s>%s" % (ctx.name_of.get(pid, pid)[2:], fp.get(pid, ctx.default(e_fac["e"], pid)), v) for pid, v in e_new["pairs"]
                    if v != fp.get(pid, ctx.default(e_fac["e"], pid))]
            if e_new["name"] != e_fac["name"]:
                diff.append("name %s>%s" % (e_fac["name"], e_new["name"]))
            if e_new.get("endb") != e_fac.get("endb"):
                diff.append("End B values")
            if k not in own or own[k] != s:
                changed += 1
                report.append("#%d %s: edited (%s)" % (slot + 1, s["name"], ", ".join(diff[:8]) + (" ..." if len(diff) > 8 else "") if diff else "End B only"))
            own[k] = s
    if fm6_seen:
        report.append("note: %d FM6 patch(es) in the files: their operator data is not imported (duell_sounds.py has no place for it); the values are" % fm6_seen)
    if steps_seen:
        report.append("note: %d patch(es) carry SEQUENCER steps: not imported" % steps_seen)
    # ---- multis
    mown = {} if a.replace or not (out / "owner_multis.py").exists() else {m["slot"]: m for m in load(out / "owner_multis.py", "om_old").OWNER_MULTIS}
    by_idx = {}
    for s in [x for k, x in ctx.base.items() if k not in own] + list(own.values()):  # (the names as the build will have them)
        by_idx[(s["engine"], s["idx"])] = s["name"]
    mchanged = 0
    for slot in sorted(multis):
        b = multis[slot]
        cur = TOOLS / "factory_multis" / ("M%02d.bin" % (slot + 1))
        if cur.exists() and cur.read_bytes() == b:
            continue
        if len(b) < 20 or b[0] != 8 or b[1] != len(ctx.tp) and b[1] != 127:
            refused.append("M%02d: not a version 8 multi record: not imported" % (slot + 1))
            continue
        try:
            parts, _ = ctx.fm.parts_of(b)
            nm = b[8:20].split(b"\0")[0].decode("ascii")
            names = [by_idx[(ctx.eng_names[e], i)] for e, i, _ in parts]
        except (KeyError, IndexError, UnicodeDecodeError) as e:
            refused.append("M%02d: the record names a sound that does not exist (%s): not imported" % (slot + 1, e))
            continue
        if b[4] not in (0, 1, 2) or not nm:
            refused.append("M%02d: MODE or name unusable: not imported" % (slot + 1))
            continue
        what = ""                                       # (0.2.25, owner: no description of an edited multi: it would describe a sound he changed)
        mown[slot + 1] = dict(slot=slot + 1, name=nm, mode={0: "DUEL", 1: "STEREO", 2: "LAYER"}[b[4]], parts=[dict(sound=n) for n in names], what=what, bytes=b)
        mchanged += 1
        report.append("M%02d %s: edited (%d bytes, %s + %s)" % (slot + 1, nm, len(b), names[0], names[1]))
    # ---- write
    print("import_owner_edits: %d patches read (%d edited, %d equal to the recipe sound), %d multis edited" % (len(patches), changed, unchanged, mchanged))
    for line in report:
        print("  " + line)
    for line in refused:
        print("  REFUSED: " + line)
    if not a.dry_run:
        write_sounds(own, out / "owner_sounds.py")
        write_multis(mown, out / "owner_multis.py")
        for slot, m in mown.items():
            if "bytes" in m:
                (out / "factory_multis").mkdir(exist_ok=True)
                (out / "factory_multis" / ("M%02d.bin" % slot)).write_bytes(m["bytes"])
        print("  wrote %s (%d sounds) and %s (%d multis)" % (out / "owner_sounds.py", len(own), out / "owner_multis.py", len(mown)))
    if a.audit and not a.dry_run and out == TOOLS:
        base = ROOT.parent
        py_ = sys.executable
        subprocess.run([py_, str(TOOLS / "gen_duell_sounds.py"), str(ROOT / "build/gen/duell_sounds.h")], cwd=ROOT, check=True)
        cc = ["cc", "-O2", "-w", "-DDUELL_TEST_MODE=1", "-Ibuild/gen_test", "-Ibuild/gen", "-Ifirmware/src", "-Itests", "-o", "build/host/sound_audit", "tests/sound_audit.c", "-lm"]
        if subprocess.run(cc, cwd=ROOT).returncode == 0:
            r = subprocess.run(["build/host/sound_audit"], cwd=ROOT, capture_output=True, text=True)
            fails = [l for l in r.stdout.splitlines() if "FAIL" in l]
            print("  sound audit: %s" % ("%d line(s) with FAIL (reported, his values are not changed):" % len(fails) if fails else "no FAIL"))
            for l in fails:
                print("    " + l)
        r = subprocess.run([py_, str(TOOLS / "factory_multis.py"), "check"], cwd=ROOT, capture_output=True, text=True)
        print("  multi check: " + (r.stdout.strip().splitlines() or ["?"])[-1])
    return 1 if refused else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
