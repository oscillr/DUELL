#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL cloud job J: the conversion of sounds, multis and patch records for the engine filters' removal (docs/design/TWIN_FILTER_DESIGN_2.md 1.5, the owner's decisions of 2026-10-19 and
2026-10-20). ANALOG's VCF and NOISE's filter become filter 1 (the MULTI FILTER), TRIO's filter becomes filter 2, filter 1's RES gets ANALOG's softer floor, WAVE's own SUB becomes the SUB LAYER
(SINE, OCTAVE -1), the engine LOFI's first knob CHIP is BITS.

    tools/convert_twin.py [--check] [--force] [--out DIR] [--report FILE] [FILE ...]

FILE: tools/duell_sounds.py or tools/owner_sounds.py (the sound lists, edited by text, the comments kept), a multi record (*.bin, MU_VER 7), a patch record (*.rec, as PATCH_RAW moves it), or an
editor export (*.duell-patches.json, *.duell-multis.json, duell-backup-*.json). No FILE: tools/duell_sounds.py, tools/owner_sounds.py and tools/factory_multis/M*.bin. The converted copies go to
--out DIR (default: over the input, the original stays in git); --check changes nothing and lists what would change (exit status 1 when anything would).

THE RULES (a value the table does not name stays; AMOUNTs, PATCH LEVELs, End B seeds, names and categories are never touched):
  cutoff dial        round((256 x CUT - 40) / 396), 0..82 (the engine's table index in 1/256 -> filter 1's dial step, 1.547 table steps)
  depths and routes  x 256 / 396 (an ENV 2 / LFO 1 / LFO 2 / AMP FLT depth, a matrix AMT to CUT): the same sweep in octaves. A route that aims at the engine's CUT slot (a per-block destination,
                     AMT 64 = the whole range) becomes a CUT route: the offset in table steps is AMT x 127 / 64 for a unipolar source and AMT x 127 / 128 for a bipolar one (mod.c), a CUT route
                     moves AMT steps: x 2 x 127 x 64 / (128 x 99) or x 127 x 64 / (128 x 99). The design table says "CUT to CUT": the two are the same for a bipolar source and 2 x for a
                     unipolar one, which none of the table's rows meets (they route to CUT itself)
  ANALOG             VCF -> filter 1 (CUT dial, RES 1 : 1, MODE LP, SLOPE 12 dB, filter 2 OFF), KTR x 63 / 299 -> KEY TRK, DRV stays; E5 E6 E8 unused
  NOISE              FREQ -> filter 1 CUT, MODE = round(RES x 64 / 127), RES x 7800 / 7600 (its damping floor 0.10 became 0.145: 2.0 .. 0.10 over 7800), TRK x 63 / 127 -> KEY TRK; E3 E4 E5 unused
  TRIO               its filter -> filter 2: TYPE = MODE (LP BP HP NOTCH), LINK OFF, CUTOFF 2 by the dial formula, RES 2 = RES, the ENV 2 / LFO 1 / LFO 2 depths -> filter 2's own amounts, AMP FLT -> a
                     matrix route AENV to CUTOFF 2, a CUT route -> CUTOFF 2, a route to its RES slot -> RES 2; filter 1 keeps what it had, its depths 0, its KEY TRK 0 while it is open; E5 E6 E7 unused
  filter 1 RES       r' = round((2399 x 127 + 5200 r) / 7600) on every other filter 1 whose SLOPE is 12 dB and which can run (a RES above 0, a CUT below the open end, a MODE above LP, or a KEY TRK that
                     closes the notes below C3): RES 0 becomes 40, the same damping. A FILTER RES route x 5200 / 7600. A 24 dB filter 1 is not touched (uf_run's 24 dB path did not change)
  WAVE               SUB (E5) -> SUB LEVEL: the level whose SUB_CURVE value is nearest 200 x SUB (both laws end in voice_amp's 24000 / 32768, so the curve of chain_sub.c against the old
                     amplitude 200 x SUB is the whole match); OCTAVE -1 and WAVE SINE are the defaults; the SUB slot unused. PHASE and WHEEL: the SUB slot stays as stored, unread (the owner retunes)
  LOFI               the label CHIP is BITS (sound lists only; the ids did not move)
DRIVE 0, ROUTE SERIES, BLEND at its middle: the defaults, so nothing is written for them. KEY TRK is always written for the three engines (the default 32 and the BASS rule would change it).

NOT CONVERTED, REPORTED: an ANALOG or NOISE part whose old filter 1 was in use as well (two filters in series cannot become one: the owner decides), a route to KTR / TRK / MODE (no destination;
it is dropped), a B edit (the hand-set End B list) that names a converted value, the project / "current music" object of a backup.

THE GUARD (a second run must not convert twice): a sound list carries the marker CONVERT_TWIN = 1 (the first run adds it; a list with it is refused); a record is skipped when its sha256 is in
tools/convert_twin_done.json (every record the tool wrote), or when the freed engine slots of ANALOG / TRIO / NOISE are all 0 (an old engine filter at CUT 0 is silent: a record like that is of the
new layout); an editor export is skipped when its "firmware" is not older than this tree's VERSION. --force overrides the guards. The filter-1 RES step cannot be seen in a record on its own:
do not feed the tool a record the new firmware saved."""
import argparse
import ast
import base64
import hashlib
import importlib.util
import json
import re
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
SRC = ROOT / "firmware" / "src"
LEDGER = TOOLS / "convert_twin_done.json"
MARKER = "CONVERT_TWIN"


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


gs = load(TOOLS / "gen_duell_sounds.py", "gen_duell_sounds")
ieo = load(TOOLS / "import_owner_edits.py", "import_owner_edits")

# ------------------------------------------------------------------------------------------------ the numbers
UF_STEPS = 82
BIPOLAR_SRC = {"KEY", "RAND", "S&H", "SLEW"}                   # mod.c MS_BIPOLAR; LFO1 / LFO2 are bipolar unless their POL is set
RES_ADD = 2399 * 127                                           # filter 1's RES: r' = (RES_ADD + 5200 r) / 7600


def rnd(num, den):
    """round half away from zero of num / den (den > 0)"""
    return (2 * num + den) // (2 * den) if num >= 0 else -((2 * -num + den) // (2 * den))


def clamp(x, lo, hi):
    return max(lo, min(hi, x))


def dial(cut):
    return clamp(rnd(256 * cut - 40, 396), 0, UF_STEPS)


def depth(d):
    return clamp(rnd(d * 256, 396), -64, 63)


def res_new(r):
    return clamp(rnd(RES_ADD + 5200 * r, 7600), 0, 127)


# the old EDIT tables (labels, defaults) of the engines that changed; every other engine is read from the sources
OLD_EDIT = {
    "ANALOG": [("WAVE", 0), ("DTN", 10), ("MIX", 64), ("NOIS", 0), ("CUT", 90), ("RES", 30), ("DRV", 0), ("KTR", 64)],
    "TRIO": [("WAVE", 0), ("INT2", 0), ("INT3", -12), ("DTN", 6), ("MODE", 0), ("CUT", 80), ("RES", 40), ("PW", 64)],
    "NOISE": [("MODE", 0), ("COLR", 64), ("FREQ", 80), ("RES", 30), ("TRK", 64), ("DENS", 0), ("DRFT", 0), ("CRSH", 0)],
    "WAVE": [("TABLE", 0), ("POS", 0), ("BEND", 0), ("SYNC", 0), ("SUB", 0), ("DTN", 0), ("STEPS", 0), ("-", 0)],
    "PHASE": [("WAVE 1", 0), ("WAVE 2", 0), ("DCW", 60), ("ENV", 64), ("DTN", 0), ("LINE", 0), ("SUB", 0), ("-", 0)],
    "WHEEL": [("REG", 4), ("SUB", 0), ("BODY", 0), ("TOP", 0), ("PERC", 0), ("CLICK", 40), ("DRV", 0), ("ROTR", 1)],
    "LOFI": [("CHIP", 0), ("WAVE", 0), ("DUTY", 64), ("CRSH", 0), ("SWP", 0), ("VIB", 0), ("ARP", 0), ("TONE", 90)],
}
OLD_NAMES = {("TRIO", 4): ["LP", "BP", "HP", "NOT"]}            # a value written as a name in a sound list
FREED = {"ANALOG": (4, 5, 7), "TRIO": (4, 5, 6), "NOISE": (2, 3, 4)}   # the E slots that are unused now
ENGINE_NAMES = ["ANALOG", "DIGITAL", "PHASE", "LOFI", "SAMPLE", "VOICE", "TRIO", "WHEEL", "GRAIN", "PHYS", "DRUM", "NOISE", "FM6", "SLICE", "WAVE"]
RETIRED_ENGINES = {"DIGITAL", "SAMPLE", "GRAIN", "DRUM", "SLICE"}
NEW_STORE_ZERO = {"P_KTRK", "P_FREEA", "P_FREED", "P_L2TRIG", "P_LTRIG", "P_DKCRV", "P_DKSRC"}   # upreset.c up_def
XB_DEFAULT_STORED = {1: 1, 7: 1, 11: 10, 12: 70}                # up_xb.h XB_DEFS as values: LINK ON, SUB OCTAVE -1 (stored 1), CYCLE RISE 10, FALL 70


class Ctx:
    def __init__(self):
        self.ids = gs.parse_ids()
        self.name_of = {v: k for k, v in self.ids.items()}
        self.tp, self.names = gs.parse_tp(self.ids)
        self.xtp = gs.parse_xtp(self.names)
        self.xnames = gs.xtp_names(self.xtp)
        engines, _ = gs.parse_engines()
        self.eng_by_name = {e["name"]: e for e in engines.values()}
        self.e0 = self.ids["P_E0"]
        self.msrc, self.mdst = self.names["N_MSRC"], self.names["N_MDST"]
        self.ms = {n: k for k, n in enumerate(self.msrc)}
        self.md = {n: k for k, n in enumerate(self.mdst)}
        i = self.ids
        self.FA, self.FB, self.FC, self.FD, self.KT = i["P_FREEA"], i["P_FREEB"], i["P_FREEC"], i["P_FREED"], i["P_KTRK"]
        self.ED, self.E2, self.LD, self.L2 = i["P_ED_FLT"], i["P_E2FLT"], i["P_LD_FLT"], i["P_L2FLT"]
        self.TY, self.C2, self.R2 = i["P_TWTYPE"], i["P_CUT2"], i["P_RES2"]
        self.LPOL, self.L2POL = i["P_LPOL"], i["P_L2POL"]
        self.M1 = i["P_M1SRC"]
        xs = {n: v[0] for n, v in self.xtp.items()}
        self.X_LINK, self.X_KTRK2, self.X_ENV2, self.X_LFO1, self.X_LFO2, self.X_SUBLVL = (xs[n] for n in ("X_LINK", "X_KTRK2", "X_ENV2", "X_LFO1", "X_LFO2", "X_SUBLVL"))
        self.xname_of = {v[0]: n for n, v in self.xtp.items()}
        m = re.search(r"SUB_CURVE\[18\]\s*=\s*\{([^}]*)\}", (SRC / "chain_sub.c").read_text())
        self.curve = [int(x) for x in m.group(1).split(",")]
        # the ids the rules read or write (the part model)
        self.rt = [self.e0 + k for k in range(8)] + [self.FA, self.FB, self.FC, self.FD, self.KT, self.ED, self.E2, self.LD, self.L2, self.TY, self.C2, self.R2,
                                                       self.LPOL, self.L2POL, self.X_LINK, self.X_KTRK2, self.X_ENV2, self.X_LFO1, self.X_LFO2, self.X_SUBLVL]
        # a key's spelling in a sound list for each id the rules add (gen_duell_sounds.build's names)
        self.key_of = {self.FA: "UF CUT", self.FB: "UF RES", self.FC: "UF MODE", self.FD: "UF SLOPE", self.KT: "KTRK", self.TY: "TWTYPE", self.C2: "CUT2", self.R2: "RES2",
                       self.ED: "AMP FLT", self.E2: "ENV2 FLT", self.LD: "LFO FLT", self.L2: "LFO2 FLT", self.X_LINK: "TWIN LINK", self.X_KTRK2: "TWIN KTRK2", self.X_ENV2: "TWIN ENV2",
                       self.X_LFO1: "TWIN LFO1", self.X_LFO2: "TWIN LFO2", self.X_SUBLVL: "SUB LEVEL"}

    def sub_gain(self, level):
        """chain_sub.c sub_gain without the VOICE_FS factor: the curve (Q15) at LEVEL"""
        x = level + (level >> 6)
        i = x >> 3
        return self.curve[i] + (((self.curve[i + 1] - self.curve[i]) * (x & 7)) >> 3)

    def sub_level(self, sub):
        """WAVE's SUB x 200 (Q15 of a full-scale sine, added before voice_amp) against the SUB LAYER's curve (Q15 of VOICE_FS, added at unity): voice_amp's 24000 / 32768 stands on both sides,
        so the LEVEL is the one whose curve value is nearest 200 x SUB (the smaller on a tie)"""
        want = 200 * sub
        return min(range(128), key=lambda lv: (abs(self.sub_gain(lv) - want), lv))

    def edit_default(self, eng, k, old):
        if old and eng in OLD_EDIT:
            return OLD_EDIT[eng][k][1]
        e = self.eng_by_name.get(eng)
        return e["edit"][k][3] if e and k < len(e["edit"]) else 0

    def labels(self, eng):
        if eng in OLD_EDIT:
            return [r[0] for r in OLD_EDIT[eng]]
        return [r[0] for r in self.eng_by_name[eng]["edit"]]

    def xdefault(self, pid):
        return self.xtp[self.xname_of[pid]][4] if pid in self.xname_of else 0

    def store_default(self, eng, pid, old):
        """upreset.c up_def: what a record that leaves id pid out means (an extension slot: its default)"""
        if pid >= 128:
            return self.xdefault(pid)
        if pid >= self.e0:
            return self.edit_default(eng, pid - self.e0, old)
        if pid == 0:
            return 104
        nm = self.name_of.get(pid)
        if nm in NEW_STORE_ZERO:
            return 0
        t = self.tp.get(nm)
        return t[3] if t else 0

    def sound_default(self, eng, pid, cat, name):
        """the value a sound list's INIT gives id pid (gen_duell_sounds: a BASS / PERC / KICK sound without KTRK gets 0)"""
        if pid >= 128:
            return self.xdefault(pid)
        if pid >= self.e0:
            return self.edit_default(eng, pid - self.e0, True)
        if pid == self.KT and (cat in ("BASS", "PERC") or "KICK" in name):
            return 0
        t = self.tp.get(self.name_of.get(pid))
        return t[3] if t else 0


# --------------------------------------------------------------------------------------------- the part model
class Part:
    """one part's relevant values (ids -> numbers, every id of Ctx.rt present) and its matrix routes [src, dst, amt] (numbers)"""

    def __init__(self, eng, v, routes):
        self.eng, self.v, self.routes = eng, dict(v), [list(r) for r in routes]
        self.origin = list(range(len(self.routes)))      # for each route, the index it had in the list given (None: added by a rule)
        self.notes, self.skip = [], None


def unipolar_src(ctx, part, src):
    n = ctx.msrc[src] if 0 <= src < len(ctx.msrc) else ""
    if n == "LFO1":
        return part.v[ctx.LPOL] != 0
    if n == "LFO2":
        return part.v[ctx.L2POL] != 0
    return n not in BIPOLAR_SRC


def slot_to_cut(ctx, part, amt, src):
    """AMT of a route to the engine's CUT slot -> AMT of a CUT route in dial steps (see the top)"""
    k = 127 * (2 if unipolar_src(ctx, part, src) else 1)
    return clamp(rnd(amt * k * 64, 128 * 99), -64, 63)


def convert_part(ctx, part):
    """apply the rules to part in place; True when anything changed (part.skip says why not, part.notes what was dropped)"""
    v, eng, md = part.v, part.eng, ctx.md
    E = lambda k: ctx.e0 + k
    before = (dict(v), [list(r) for r in part.routes])
    slope24 = v[ctx.FD] == 2
    f1_active = v[ctx.FA] < UF_STEPS or v[ctx.FC] > 0
    if eng in FREED and all(v[E(k)] == 0 for k in FREED[eng]):
        part.skip = "the freed engine slots are all 0: already the new layout"
        return False

    def add_route(src, dst, amt):
        if len(part.routes) < 8:
            part.routes.append([src, dst, amt])
            part.origin.append(None)
        else:
            part.notes.append("no free matrix slot for the route %s > %s %d" % (ctx.msrc[src], ctx.mdst[dst], amt))

    def map_routes(table, dropped, res_scale=None):
        out, org = [], []
        for n, (src, dst, amt) in enumerate(part.routes):
            if dst in dropped:
                part.notes.append("route %s > %s dropped (no destination)" % (ctx.msrc[src], ctx.mdst[dst]))
                continue
            if dst in table:
                nd = table[dst]
                if dst == md["CUT"]:
                    amt = depth(amt)
                elif nd in (md["CUT"], md["CUT2"]):
                    amt = slot_to_cut(ctx, part, amt, src)
                elif res_scale and nd == md["FRES"]:
                    amt = clamp(rnd(amt * res_scale[0], res_scale[1]), -64, 63)
                dst = nd
            out.append([src, dst, amt])
            org.append(part.origin[n])
        part.routes, part.origin = out, org

    if eng == "ANALOG" and f1_active:
        # two filters in series (the VCF, then filter 1 set by the owner): filter 1 stays what it was, the VCF (a 12 dB low-pass) becomes filter 2's LP (SERIES: filter 1 then filter 2; two linear
        # low-pass filters in series commute), as TRIO's filter does: LINK OFF, CUTOFF 2 by the dial formula, RES 2 on TRIO's damping law (8192 - 7168 r / 127 against the VCF's 7600), KTR -> KEY TRK 2,
        # the depths -> filter 2's own amounts, AMP FLT -> AENV to CUTOFF 2
        cut, res, ktr = v[E(4)], v[E(5)], v[E(7)]
        v[ctx.TY], v[ctx.C2], v[ctx.R2], v[ctx.X_LINK] = 1, dial(cut), clamp(rnd(res * 7600, 7168), 0, 127), 0
        v[ctx.X_KTRK2] = rnd(ktr * 63, 299)
        v[ctx.X_ENV2], v[ctx.X_LFO1], v[ctx.X_LFO2] = depth(v[ctx.E2]), depth(v[ctx.LD]), depth(v[ctx.L2])
        ed = depth(v[ctx.ED])
        for d in (ctx.E2, ctx.LD, ctx.L2, ctx.ED):
            v[d] = 0
        for k in FREED[eng]:
            v[E(k)] = 0
        map_routes({md["CUT"]: md["CUT2"], md["E5"]: md["CUT2"], md["E6"]: md["FRES2"]}, {md["E8"]})
        if ed:
            add_route(ctx.ms["AENV"], md["CUT2"], ed)
        part.notes.append("filter 1 was in use as well as the VCF: the VCF is filter 2 (LP), filter 1 is as it was")
        if not slope24:
            v[ctx.FB] = res_new(v[ctx.FB])
            for r in part.routes:
                if r[1] == md["FRES"]:
                    r[2] = clamp(rnd(r[2] * 5200, 7600), -64, 63)
    elif eng in ("ANALOG", "NOISE"):
        if f1_active:
            part.skip = "CONFLICT: filter 1 (CUT %d, MODE %d) was in use as well as the engine's filter; the owner decides" % (v[ctx.FA], v[ctx.FC])
            return False
        if eng == "ANALOG":
            cut, res, trk = v[E(4)], v[E(5)], rnd(v[E(7)] * 63, 299)
            v[ctx.FB], v[ctx.FC] = res, 0
            map_routes({md["CUT"]: md["CUT"], md["E5"]: md["CUT"], md["E6"]: md["FRES"]}, {md["E8"]})
        else:
            cut, res, trk = v[E(2)], v[E(3)], rnd(v[E(4)] * 63, 127)
            v[ctx.FB], v[ctx.FC] = clamp(rnd(res * 7800, 7600), 0, 127), rnd(res * 64, 127)
            map_routes({md["CUT"]: md["CUT"], md["E3"]: md["CUT"], md["E4"]: md["FRES"]}, {md["E5"]}, (7800, 7600))
        v[ctx.FA], v[ctx.FD], v[ctx.KT] = dial(cut), 1, trk
        for k in FREED[eng]:
            v[E(k)] = 0
        for d in (ctx.ED, ctx.E2, ctx.LD, ctx.L2):
            v[d] = depth(v[d])
    else:
        if eng == "TRIO":
            mode, cut, res = v[E(4)], v[E(5)], v[E(6)]
            v[ctx.TY], v[ctx.C2], v[ctx.R2], v[ctx.X_LINK] = 1 + (mode & 3), dial(cut), res, 0
            v[ctx.X_ENV2], v[ctx.X_LFO1], v[ctx.X_LFO2] = depth(v[ctx.E2]), depth(v[ctx.LD]), depth(v[ctx.L2])
            ed = depth(v[ctx.ED])
            for d in (ctx.E2, ctx.LD, ctx.L2, ctx.ED):
                v[d] = 0
            for k in FREED[eng]:
                v[E(k)] = 0
            if not f1_active:
                v[ctx.KT] = 0
            map_routes({md["CUT"]: md["CUT2"], md["E6"]: md["CUT2"], md["E7"]: md["FRES2"]}, {md["E5"]})
            if ed:
                add_route(ctx.ms["AENV"], md["CUT2"], ed)
        # filter 1 as it was: the RES step (the same damping under the softer floor) and the FILTER RES routes
        if not slope24:
            if v[ctx.FB] != 0 or v[ctx.FA] < UF_STEPS or v[ctx.FC] > 0 or v[ctx.KT] != 0:   # (a filter that can run: its CUT or MODE, or KEY TRK closing the notes below C3)
                v[ctx.FB] = res_new(v[ctx.FB])
            for r in part.routes:
                if r[1] == md["FRES"]:
                    r[2] = clamp(rnd(r[2] * 5200, 7600), -64, 63)
        if eng == "WAVE":
            sub = v[E(4)]
            if sub > 0:
                v[ctx.X_SUBLVL] = ctx.sub_level(sub)
                v[E(4)] = 0
            for r in part.routes:
                if r[1] == md["E5"]:
                    r[1] = md["SUBLV"]
                    part.notes.append("route %s > SUB now moves SUB LEVEL (the same AMT)" % ctx.msrc[r[0]])
    return (v, part.routes) != before


# ------------------------------------------------------------------------------------------------ sound lists
class SoundList:
    """a duell_sounds.py / owner_sounds.py: the text, its syntax tree, the evaluated module"""

    def __init__(self, path):
        self.path = Path(path)
        self.src = self.path.read_bytes()
        self.tree = ast.parse(self.src.decode("utf-8"))
        self.mod = load(self.path, "ct_" + self.path.stem)
        self.starts = [0]
        for ln in self.src.split(b"\n")[:-1]:
            self.starts.append(self.starts[-1] + len(ln) + 1)

    def off(self, lineno, col):                    # (ast columns are UTF-8 byte offsets)
        return self.starts[lineno - 1] + col

    def span(self, node):
        return self.off(node.lineno, node.col_offset), self.off(node.end_lineno, node.end_col_offset)

    def sounds(self):
        for node in ast.walk(self.tree):
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == "dict":
                kw = {k.arg: k.value for k in node.keywords}
                if "engine" in kw and "idx" in kw and isinstance(kw.get("p"), ast.Dict):
                    yield ast.literal_eval(kw["engine"]), ast.literal_eval(kw["idx"]), kw["p"]

    def final(self):
        lst = getattr(self.mod, "SOUNDS", None) or getattr(self.mod, "OWNER_SOUNDS", [])
        return {(s["engine"], s["idx"]): s for s in lst}


def key_target(ctx, eng, key):
    """a sound list's key -> ('mod', n) | an id | None"""
    m = re.fullmatch(r"MOD([1-8])", key)
    if m:
        return ("mod", int(m.group(1)))
    labels = ctx.labels(eng)
    em = re.fullmatch(r"E([1-8])", key)
    if em:
        return ctx.e0 + int(em.group(1)) - 1
    if key in labels:
        return ctx.e0 + labels.index(key)
    for k, n in enumerate(labels):                    # "WAVE 1" is written WAVE1 (or WAVE)
        if " " in n and n[-2:] in (" 1", " 2") and (key == n.replace(" ", "") or (n.endswith(" 1") and key == n[:-2])):
            return ctx.e0 + k
    if key in ctx.xnames and f"P_{gs.ALIAS.get(key, key)}" not in ctx.ids:
        return ctx.xtp[ctx.xnames[key]][0]
    return ctx.ids.get("P_" + gs.ALIAS.get(key, key))


def route_of(ctx, eng, val):
    src, dst, amt = val
    if dst not in ctx.md:                              # an engine EDIT label that is no matrix name is the engine's slot (a label of an engine the rules do not touch stays as written)
        labels = ctx.labels(eng)
        dst = "E%d" % (labels.index(dst) + 1) if dst in labels else dst
    return [ctx.ms[src], ctx.md.get(dst, dst), int(amt)]


def num_of(ctx, eng, pid, val):
    if isinstance(val, str):
        names = OLD_NAMES.get((eng, pid - ctx.e0))
        return names.index(val) if names and val in names else 0
    return int(val)


def lit_entry_delete(sl, kn, vn):
    """the span of an entry of a dict literal (key start .. value end, its comma, the line when it is alone on it)"""
    a, b, src = sl.span(kn)[0], sl.span(vn)[1], sl.src
    while src[b:b + 1] == b" ":
        b += 1
    if src[b:b + 1] == b",":
        b += 1
    while src[b:b + 1] == b" ":
        b += 1
    ls, le = src.rfind(b"\n", 0, a) + 1, src.find(b"\n", b)
    if src[ls:a].strip() == b"" and src[b:le].strip() == b"":
        return ls, le + 1, b""
    return a, b, b""


def insert_in_dict(sl, node, text):
    """whole lines before the closing brace of the dict literal (or inline when the brace shares a line)"""
    end = sl.off(node.end_lineno, node.end_col_offset) - 1
    ls = sl.src.rfind(b"\n", 0, end) + 1
    last = sl.span(node.values[-1])[1] if node.values else None
    comma = last is not None and re.match(rb"[ \t]*,", sl.src[last:end]) is None       # (the last entry may have no comma: it needs one before ours)
    edits = [(last, last, b",")] if comma else []
    if sl.src[ls:end].strip() == b"":
        return edits + [(ls, ls, text.encode())]
    return edits + [(end, end, (" " + " ".join(x.strip() for x in text.strip().split("\n")) + " ").encode())]


WHY = {"ANALOG": "the VCF is filter 1", "NOISE": "the engine's filter is filter 1", "TRIO": "the filter is filter 2 (LINK OFF), filter 1 stays open", "WAVE": "SUB is the SUB LAYER"}


def convert_sound_list(ctx, path, report):
    """-> (the converted bytes or None)"""
    sl = SoundList(path)
    if re.search(rb"^%s\s*=" % MARKER.encode(), sl.src, re.M):
        raise SystemExit("convert_twin: %s already carries %s: converted before (--force to run it again)" % (path, MARKER))
    final = sl.final()
    edits, subtle = [], {}
    for eng, idx, pnode in sl.sounds():
        fin = final.get((eng, idx))
        if fin is None:
            continue
        name, cat = fin["name"], fin["cat"]
        lit = {ast.literal_eval(k): (k, v) for k, v in zip(pnode.keys, pnode.values) if k is not None}
        explicit, routes = {}, {}
        for key, val in fin["p"].items():
            t = key_target(ctx, eng, key)
            if isinstance(t, tuple):
                routes[t[1]] = route_of(ctx, eng, val)
            elif t is not None:
                explicit[t] = num_of(ctx, eng, t, val)
        old = {pid: explicit.get(pid, ctx.sound_default(eng, pid, cat, name)) for pid in ctx.rt}
        rkeys = sorted(routes)
        part = Part(eng, old, [routes[k] for k in rkeys])
        if eng == "LOFI" and "CHIP" in lit:
            edits.append(sl.span(lit["CHIP"][0]) + (b'"BITS"',))
        dead = {"PHASE": ctx.e0 + 6, "WHEEL": ctx.e0 + 1}.get(eng)             # the SUB knob of PHASE / WHEEL is gone: the entry goes (the owner retunes the sound with the SUB LAYER)
        for key, (kn, vn) in lit.items():
            if dead is not None and key_target(ctx, eng, key) == dead:
                edits.append(lit_entry_delete(sl, kn, vn))
                report.append("%s %s: SUB %s dropped (the knob is gone; retune with the SUB LAYER)" % (eng, name, explicit.get(dead)))
        if not convert_part(ctx, part):
            if part.skip:
                report.append("%s %s: %s" % (eng, name, part.skip))
            continue
        report.append("%s %s: converted%s" % (eng, name, "".join("; " + n for n in part.notes)))
        new, here = part.v, []
        rename = {"ANALOG": {ctx.e0 + 4: ctx.C2, ctx.e0 + 5: ctx.R2, ctx.e0 + 7: ctx.X_KTRK2, ctx.E2: ctx.X_ENV2, ctx.LD: ctx.X_LFO1, ctx.L2: ctx.X_LFO2} if new[ctx.TY] == 1 else
                              {ctx.e0 + 4: ctx.FA, ctx.e0 + 5: ctx.FB, ctx.e0 + 7: ctx.KT},
                  "NOISE": {ctx.e0 + 2: ctx.FA, ctx.e0 + 3: ctx.FB, ctx.e0 + 4: ctx.KT},
                  "TRIO": {ctx.e0 + 4: ctx.TY, ctx.e0 + 5: ctx.C2, ctx.e0 + 6: ctx.R2, ctx.E2: ctx.X_ENV2, ctx.LD: ctx.X_LFO1, ctx.L2: ctx.X_LFO2},
                  "WAVE": {ctx.e0 + 4: ctx.X_SUBLVL}}.get(eng, {})
        written = set()
        for key, (kn, vn) in lit.items():
            t = key_target(ctx, eng, key)
            if t is None or isinstance(t, tuple) or t not in new:
                continue
            tgt = rename.get(t)
            if tgt is not None and explicit.get(t) is not None:     # renamed in place: the recipe's comment stays beside it
                here.append(sl.span(kn) + (('"%s"' % ctx.key_of[tgt]).encode(),))
                here.append(sl.span(vn) + (str(new[tgt]).encode(),))
                written.add(tgt)
                written.add(t)
            elif new[t] != old[t]:
                if new[t] == 0 and t in (ctx.ED, ctx.E2, ctx.LD, ctx.L2) or (t >= ctx.e0 and t < ctx.e0 + 8 and new[t] == 0):
                    here.append(lit_entry_delete(sl, kn, vn))        # (a depth that moved to filter 2, a slot that is unused now)
                else:
                    here.append(sl.span(vn) + (str(new[t]).encode(),))
                written.add(t)
            else:
                written.add(t)
        add = []
        for pid in ctx.rt:
            if pid in written or pid in (ctx.LPOL, ctx.L2POL) or ctx.e0 <= pid < ctx.e0 + 8:
                continue
            if new[pid] != old[pid] or (pid == ctx.KT and eng in FREED):
                if new[pid] == 0 and pid in (ctx.ED, ctx.E2, ctx.LD, ctx.L2):
                    continue
                add.append((ctx.key_of[pid], str(new[pid])))
        # the routes: a survivor keeps its key (the tuple replaced when it changed), a dropped one is deleted, a new one takes a free slot
        orig = [routes[k] for k in rkeys]
        now = {o: r for o, r in zip(part.origin, part.routes) if o is not None}
        for n, key in enumerate(rkeys):
            mk, r = "MOD%d" % key, now.get(n)
            if r is None:
                if mk in lit:
                    here.append(lit_entry_delete(sl, *lit[mk]))
                else:
                    subtle[(eng, idx, key)] = None
            elif r != orig[n]:
                if mk in lit:
                    here.append(sl.span(lit[mk][1]) + (('("%s", "%s", %d)' % (ctx.msrc[r[0]], ctx.mdst[r[1]], r[2])).encode(),))
                else:
                    subtle[(eng, idx, key)] = (ctx.msrc[r[0]], ctx.mdst[r[1]], r[2])
        extra = [r for o, r in zip(part.origin, part.routes) if o is None]
        free = [k for k in range(1, 9) if "MOD%d" % k not in fin["p"]]
        for r in extra:
            add.append(("MOD%d" % free.pop(0), '("%s", "%s", %d)' % (ctx.msrc[r[0]], ctx.mdst[r[1]], r[2])))
        if add:
            txt = "        # convert_twin.py (cloud job J): %s\n" % WHY.get(eng, "filter 1's RES step") + "".join('        "%s": %s,\n' % kv for kv in add)
            here.extend(insert_in_dict(sl, pnode, txt))
        edits.extend(here)
    edits.extend(fix_subtle(sl, subtle))
    if not edits:
        return None
    out = sl.src
    for a, b, rep in sorted(edits, key=lambda e: (e[0], e[1]), reverse=True):
        out = out[:a] + rep + out[b:]
    if subtle:                                   # the SUBTLE loop learns the fourth element (the AMT of a route this tool changed) and a dropped route
        o_loop = b'''    for _slot, _src, _dst in _routes:
        assert f"MOD{_slot}" not in _s["p"], (_n, _slot)     # (a free slot: the sound's own routes stay)
        _s["p"][f"MOD{_slot}"] = (_src, _dst, subtle(_e, _k, _slot))'''
        n_loop = b'''    for _slot, _src, _dst, *_amt in _routes:                 # (tools/convert_twin.py, cloud job J: a route it changed carries its AMT as a fourth element, a dropped one has SRC None)
        if _src is None:
            continue
        assert f"MOD{_slot}" not in _s["p"], (_n, _slot)     # (a free slot: the sound's own routes stay)
        _s["p"][f"MOD{_slot}"] = (_src, _dst, _amt[0] if _amt else subtle(_e, _k, _slot))'''
        if o_loop not in out:
            raise SystemExit("convert_twin: the SUBTLE loop of %s is not the one this tool knows" % path)
        out = out.replace(o_loop, n_loop)
    doc_end = sl.tree.body[0].end_lineno if sl.tree.body and isinstance(sl.tree.body[0], ast.Expr) else 0
    at = sl.starts[doc_end] if doc_end < len(sl.starts) else len(sl.src)
    for a, b, rep in sorted(edits, key=lambda e: e[0]):          # (the marker goes above every edit: the module docstring ends before them)
        assert a >= at
    mk = ("\n# %s: tools/convert_twin.py has run on this list (the engine filters left, cloud job J); a second run is refused (TWIN_FILTER_DESIGN_2.md 1.5).\n%s = 1\n" % (MARKER, MARKER)).encode()
    return out[:at] + mk + out[at:]


def fix_subtle(sl, fixes):
    """duell_sounds.py's SUBTLE list adds routes in code, their AMT from a formula: a converted one gets a fourth element (the AMT) and its DST; a dropped one (slot, None, None)"""
    edits = []
    if not fixes:
        return edits
    for node in ast.walk(sl.tree):
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "SUBTLE" for t in node.targets):
            for tup in node.value.elts:
                eng, idx = ast.literal_eval(tup.elts[0]), ast.literal_eval(tup.elts[1])
                for r in tup.elts[4].elts:
                    slot = ast.literal_eval(r.elts[0])
                    if (eng, idx, slot) in fixes:
                        fx = fixes[(eng, idx, slot)]
                        a, b = sl.span(r)
                        edits.append((a, b, (b"(%d, None, None)" % slot) if fx is None else ('(%d, "%s", "%s", %d)' % (slot, fx[0], fx[1], fx[2])).encode()))
    return edits


# ----------------------------------------------------------------------------------------- binary records
MB_N, PS_STEP, MU_G_AT, PT_FIX = 7, 9, 20, 20


def part_from_dense(ctx, eng, vals, xs):
    full = {pid: (xs[pid - 128] if pid >= 128 else vals[pid]) for pid in ctx.rt}
    routes = []
    for k in range(8):
        s, d, a = vals[ctx.M1 + 3 * k], vals[ctx.M1 + 3 * k + 1], vals[ctx.M1 + 3 * k + 2]
        if s:
            routes.append([s, d, a])
    return Part(eng, full, routes)


def part_to_dense(ctx, part, vals, xs):
    for pid, val in part.v.items():
        if pid >= 128:
            xs[pid - 128] = val
        else:
            vals[pid] = val
    for k in range(8):
        r = part.routes[k] if k < len(part.routes) else [0, 0, 0]
        vals[ctx.M1 + 3 * k], vals[ctx.M1 + 3 * k + 1], vals[ctx.M1 + 3 * k + 2] = r


def convert_multi(ctx, b, report, label):
    """a multi record (MU_VER 7) -> the converted record, or None"""
    if len(b) < MU_G_AT or b[0] != 7:
        raise ValueError("%s: not a version 7 multi record" % label)
    np_, ng = b[1], b[2]
    out, pos, changed = bytearray(b), MU_G_AT + 2 * ng, False
    for part_no in range(2):
        eng = ENGINE_NAMES[b[pos]] if b[pos] < len(ENGINE_NAMES) else None
        flags = b[pos + 3]
        vat = pos + 4 + MB_N
        vals = {i: b[vat + i] - 64 for i in range(np_)}
        nx = b[vat + np_]
        xs = [b[vat + np_ + 1 + k] - 64 for k in range(nx)]
        ne = b[vat + np_ + 1 + nx]
        edits = {b[vat + np_ + 2 + nx + 2 * i] for i in range(ne)}
        if eng and eng not in RETIRED_ENGINES:
            part = part_from_dense(ctx, eng, vals, xs)
            if convert_part(ctx, part):
                touched = edits & (set(ctx.rt) | set(range(ctx.M1, ctx.M1 + 24)))
                if touched:
                    report.append("%s part %d (%s): B edits name converted values (%s): not converted" % (label, part_no + 1, eng, sorted(touched)))
                else:
                    part_to_dense(ctx, part, vals, xs)
                    for i in range(np_):
                        out[vat + i] = vals[i] + 64
                    for k in range(nx):
                        out[vat + np_ + 1 + k] = xs[k] + 64
                    changed = True
                    report.append("%s part %d (%s): converted%s" % (label, part_no + 1, eng, "".join("; " + n for n in part.notes)))
            elif part.skip:
                report.append("%s part %d (%s): %s" % (label, part_no + 1, eng, part.skip))
        pos = vat + np_ + 1 + nx + 1 + 2 * ne
        pos += 8 + PS_STEP * sum(bin(x).count("1") for x in b[pos:pos + 8]) + (128 if flags & 1 else 0)
    return bytes(out) if changed else None


def rd16(b, o):
    return b[o] | b[o + 1] << 8


def convert_patch(ctx, b, report, label):
    """a patch record (PT_VER 3, as PATCH_RAW moves it) -> the converted record, or None. The values are written the way upreset.c pt_encode does: pairs, or a bitmap above 16"""
    if len(b) < PT_FIX or rd16(b, 0) != len(b):
        raise ValueError("%s: not a patch record" % label)
    f, PTF = b[4], ieo.PTF
    if not f & PTF["CB"] or not f & PTF["TR"]:
        report.append("%s: a record of a firmware before 0.2.19: not converted" % label)
        return None
    eng = ENGINE_NAMES[b[5]] if b[5] < len(ENGINE_NAMES) else None
    if eng is None or eng in RETIRED_ENGINES:
        return None
    o, xs, xedit = PT_FIX, [ctx.xdefault(128 + k) for k in range(16)], b""
    if f & PTF["XB"]:
        L = b[o]
        at = o + 2
        while at < o + 1 + L:
            tag, n = b[at], b[at + 1]
            data = b[at + 2:at + 2 + n]
            if tag == 1:
                for i in range(0, n, 2):
                    xs[data[i] - 128] = data[i + 1] - 64
            elif tag == 2:
                xedit = bytes(data)
            at += 2 + n
        o += 1 + L
    mb = bytes(b[o:o + MB_N]) if f & PTF["MB"] else b""
    o += MB_N if f & PTF["MB"] else 0
    n, old_vals = b[19], {}
    if f & PTF["BM"]:
        j = 0
        for i in range(128):
            if b[o + (i >> 3)] >> (i & 7) & 1:
                old_vals[i] = b[o + 16 + j] - 64
                j += 1
        o += 16 + n
    else:
        for i in range(n):
            old_vals[b[o + 2 * i]] = b[o + 2 * i + 1] - 64
        o += 2 * n
    pat = bytes(b[o:o + 32]) if f & PTF["PAT"] else b""
    o += 32 if f & PTF["PAT"] else 0
    fm = bytes(b[o:o + 112]) if f & PTF["FM6"] else b""
    o += 112 if f & PTF["FM6"] else 0
    ak = bytes(b[o:]) if f & PTF["AK"] else b""
    vals = {pid: old_vals.get(pid, ctx.store_default(eng, pid, True)) for pid in range(127)}
    part = part_from_dense(ctx, eng, vals, xs)
    if xedit and {xedit[i] for i in range(0, len(xedit), 2)} & (set(ctx.rt) | set(range(ctx.M1, ctx.M1 + 24))):
        report.append("%s: B edits name converted values: not converted" % label)
        return None
    if not convert_part(ctx, part):
        if part.skip:
            report.append("%s (%s): %s" % (label, eng, part.skip))
        return None
    part_to_dense(ctx, part, vals, xs)
    report.append("%s (%s): converted%s" % (label, eng, "".join("; " + x for x in part.notes)))
    ak_ids = set()                                   # the arp / keys / sequencer ids the AK block carries: not in the value list
    if ak:
        grp = ak[2] & 7
        for k, nm in enumerate(ieo.AK_NAMES):
            if (1 if k < 8 else 2 if k < 11 else 4) & grp:
                ak_ids.add(ctx.ids.get(nm, -1))
    pairs = [(pid, vals[pid]) for pid in range(1, 127) if pid not in ak_ids and vals[pid] != ctx.store_default(eng, pid, False)]
    ext = [(128 + k, xs[k]) for k in range(16) if xs[k] != ctx.xdefault(128 + k)]
    sect = (bytes([1, 2 * len(ext)]) + b"".join(bytes([w, v + 64]) for w, v in ext) if ext else b"") + (bytes([2, len(xedit)]) + xedit if xedit else b"")
    xb = (bytes([1 + len(sect), 1]) + sect) if sect else b""
    nf = (f & ~(PTF["XB"] | PTF["BM"])) | (PTF["XB"] if xb else 0) | (PTF["BM"] if len(pairs) > 16 else 0)
    if nf & PTF["BM"]:
        bm = bytearray(16)
        for pid, _ in pairs:
            bm[pid // 8] |= 1 << (pid % 8)
        body = bytes(bm) + bytes(v + 64 for _, v in pairs)
    else:
        body = b"".join(bytes([pid, v + 64]) for pid, v in pairs)
    out = bytearray(b[:PT_FIX])
    out[4], out[19] = nf, len(pairs)
    out += xb + mb + body + pat + fm + ak
    out[0], out[1] = len(out) & 255, len(out) >> 8
    if len(out) > 534:
        raise ValueError("%s: the converted record is %d bytes, over 534" % (label, len(out)))
    return bytes(out)


# ------------------------------------------------------------------------------------------------ files
def sha(b):
    return hashlib.sha256(b).hexdigest()


def ledger():
    return json.loads(LEDGER.read_text()) if LEDGER.exists() else {"outputs": []}


def ver(s):
    return tuple(int(x) for x in re.findall(r"\d+", str(s))[:3])


def convert_json(ctx, path, report, force):
    j = json.loads(Path(path).read_text())
    led, changed, fmt = ledger(), False, j.get("format")
    vf = ROOT.parent / "VERSION"
    if not force and vf.exists() and j.get("firmware") and ver(j["firmware"]) >= ver(vf.read_text()):
        report.append("%s: exported by firmware %s, not older than this tree (%s): skipped (--force)" % (path, j["firmware"], vf.read_text().strip()))
        return None
    if fmt == "duell-patches":
        for it in j.get("patches", []):
            if not isinstance(it.get("record"), str):
                continue
            b = base64.b64decode(it["record"])
            if sha(b) in led["outputs"] and not force:
                report.append("%s: already converted (ledger): skipped" % it.get("name", "?"))
                continue
            nb = convert_patch(ctx, b, report, "%s #%s" % (it.get("name", "?"), it.get("number")))
            if nb:
                it["record"], it["crc32"] = base64.b64encode(nb).decode(), "%08x" % zlib.crc32(nb)
                if "size" in it:
                    it["size"] = len(nb)
                changed = True
    elif fmt == "duell-multis":
        for it in j.get("multis", []):
            b = base64.b64decode(it["data"])
            if sha(b) in led["outputs"] and not force:
                report.append("M%02d: already converted (ledger): skipped" % it.get("slot", 0))
                continue
            nb = convert_multi(ctx, b, report, "M%02d %s" % (it.get("slot", 0), it.get("name", "?")))
            if nb:
                it["data"], it["crc"], it["size"] = base64.b64encode(nb).decode(), zlib.crc32(nb), len(nb)
                changed = True
    elif fmt == "duell-backup":
        for o in j.get("objects", []):
            b = base64.b64decode(o.get("data", ""))
            nb = None
            if not b:
                continue
            if o.get("id") == 40:
                nb = convert_patch(ctx, b, report, "backup patch slot %s" % o.get("slot"))
            elif b[:4] == b"MUL1":
                ln, at, recs, ok = [rd16(b, 4), rd16(b, 6)], 8, [], False
                for h in range(2):
                    r = bytes(b[at:at + ln[h]])
                    nr = convert_multi(ctx, r, report, "backup multi object %s record %d" % (o.get("id"), h + 1)) if r else None
                    recs.append(nr or r)
                    ok |= bool(nr)
                    at += ln[h]
                if ok:
                    nb = b"MUL1" + len(recs[0]).to_bytes(2, "little") + len(recs[1]).to_bytes(2, "little") + recs[0] + recs[1]
            elif o.get("id") == 0:
                report.append("backup object 0 (the current music) is not converted")
            if nb:
                o["data"], o["size"], o["crc"] = base64.b64encode(nb).decode(), len(nb), zlib.crc32(nb)
                changed = True
    else:
        report.append("%s: neither a duell-patches, duell-multis nor a duell-backup file: skipped" % path)
        return None
    return (json.dumps(j, indent=1) + "\n").encode() if changed else None


def run(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("files", nargs="*")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--out", default=None)
    ap.add_argument("--report", default=None)
    a = ap.parse_args(argv)
    ctx = Ctx()
    files = [Path(f) for f in a.files] or [TOOLS / "duell_sounds.py", TOOLS / "owner_sounds.py"] + sorted((TOOLS / "factory_multis").glob("M*.bin"))
    report, outputs, todo, led = [], [], 0, ledger()
    for f in files:
        if not f.exists():
            report.append("%s: missing" % f)
            continue
        if f.suffix == ".py":
            new = convert_sound_list(ctx, f, report)
        elif f.suffix in (".bin", ".rec"):
            b = f.read_bytes()
            if sha(b) in led["outputs"] and not a.force:
                report.append("%s: already converted (ledger): skipped" % f.name)
                continue
            new = (convert_multi if f.suffix == ".bin" else convert_patch)(ctx, b, report, f.stem)
        elif f.suffix == ".json":
            new = convert_json(ctx, f, report, a.force)
        else:
            report.append("%s: not a file the tool knows" % f)
            continue
        if new is None:
            continue
        todo += 1
        dest = Path(a.out) / f.name if a.out else f
        if not a.check:
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(new)
            if f.suffix in (".bin", ".rec"):
                outputs.append(sha(new))
        report.append("%s: %s" % (f.name, "would change" if a.check else "written to %s" % dest))
    if outputs:
        led["outputs"] = sorted(set(led["outputs"]) | set(outputs))
        LEDGER.write_text(json.dumps(led, indent=1) + "\n")
    text = "\n".join(report)
    if a.report:
        Path(a.report).write_text(text + "\n")
    print(text)
    return 1 if a.check and todo else 0


if __name__ == "__main__":
    sys.exit(run(sys.argv[1:]))
