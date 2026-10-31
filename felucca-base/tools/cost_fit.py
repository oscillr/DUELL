#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""DUELL 1.1 item 105: turns the COST-CAL run (tools/gen_cost_suite.py, results.csv of tools/bench_run.py) into the NOTES budget's cost table.

  python3 tools/cost_fit.py fit bench_runs/<run>/results.csv [--out DIR] [--src firmware/src] [--agg max|median]
        -> DIR/report.txt (the plain table), DIR/cost_table.json (the measured table), DIR/cost_rows.csv (every row with its spread),
           DIR/budget_rows.patch (the proposed replacement of the cost rows in budget.c, budget_feat.c, budget.h: a patch FILE, nothing is edited)
  python3 tools/cost_fit.py validate bench_runs/<run>/results.csv --table DIR/cost_table.json [--polymax POLYMAX/results.csv] [--assume sends,morph] [--limit L --margin M]
        (defaults: BUDGET_LIMIT and BUDGET_MARGIN / 100 read from firmware/src/budget.h; item 137: 81 and 1.00)
        -> predicts the held-out runs (CAL.H.*, never in the fit) and, with a POLY-MAX run, the most keys of each engine, and prints the error
  python3 tools/cost_fit.py selftest        the Mac side without a unit: a synthetic run from a known table, fitted back, validated, the patch tried with `patch --dry-run`

What is measured: the unit's `avg` column, core 0's audio interrupt time as % of one half buffer (the CPU bar; TIMER5's nested time, the key scan, is taken out). Instructions a sample = % x 32.8125
(budget.h: 3281.25 = 100 %). Per engine: avg = a + s x voices sounding (`vox` column), a straight line through the key counts 1, 2, 4, 8, 12: s is the cost of one more voice, a the part's
fixed work and the mix; on one core and on two (the critical path: x = voices of one part). Per feature: the same scene with the feature on, against the baseline line (or the feature it is
built on): a slope change is a per-voice cost, an intercept change a fixed cost. The spread comes from the repeats and from the line's own scatter. A row is GOOD (spread under 10 %), OK
(under 25 %), LOW (more) or BELOW NOISE (the value is under two spreads: a 0.1 % step of the unit's figure is 3.3 instructions).

THE ONE-CORE ARM (item 110): a '.1C.' test is made with `tune slack 400000`, which in this build does NOT make core 0 play part 2 itself while core 1 is up (twocore_run.c tcr_begin,
the item 90 branch: under the two-core rule a missed deadline still posts the job). load_run finds such a test (tc 2, tcown 0, core 1 busy) and marks its rows fake1c. The fit then
reads a '.1C.' row as avg + c1 (core 0 plus core 1 = both parts: the one-core equivalent, the hand-over still inside it), takes the hand-over from the two-core intercepts as an
ESTIMATE (never written to the patch), and uses the same rows as additional two-core data (avg and c1 apart). A '.1C.' test that really played on one core is read as before.
Standard library only.
"""
import argparse
import csv
import difflib
import json
import math
import os
import random
import re
import shutil
import statistics
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cost_defs as D  # noqa: E402

PCT = D.PCT
FAKE_C1_MIN = 0.5                  # % of a half: core 1 busier than this in a '.1C.' test that took nothing over (tcown 0) means core 1 is still playing part 2
PEAK_MAX = 98.0                    # a run whose peak touched the half's end is clipped: its average says nothing about the cost
SIGMA_FLOOR = 0.07                 # % of a half: the least spread one run can have (the figure has 0.1 % steps), when the repeats are too few to say
SRC_DEFAULT = os.path.join(HERE, "..", "firmware", "src")


# ------------------------------------------------------------------------------------------------ the run
def num(r, k, d=0.0):
    try:
        return float(r.get(k) or d)
    except (TypeError, ValueError):
        return d


def load_run(path, all_rows=False):
    """{test name: [row, ...]} with the rows that can be used: DONE, no missed half, peak under PEAK_MAX, and for two cores really on two cores; and the list of the unusable ones.
    all_rows: the unusable ones stay in the dict too (row['ok'] False), for POLY-MAX, whose point is where the halves start to miss"""
    rows, bad = {}, []
    with open(path, newline="") as fh:
        for r in csv.DictReader(fh):
            name = r.get("test", "")
            if not name.startswith("CAL.") and not name.startswith("POLY_"):
                continue
            why = ""
            if r.get("status", "DONE") != "DONE":
                why = "status " + r.get("status", "?")
            elif num(r, "miss") > 0:
                why = f"{int(num(r, 'miss'))} halves missed"
            elif num(r, "peak") > PEAK_MAX:
                why = f"peak {num(r, 'peak'):.1f} %"
            is1c = ".1C." in name or name.endswith("_1C") or "_1C_" in name
            # item 110: a one-core test on which core 1 kept playing part 2 (the unit's session is on two cores, core 0 took nothing over, core 1 is busy) is not a one-core test
            fake = is1c and str(r.get("tc", "")) == "2" and num(r, "tcown") == 0 and num(r, "c1") > FAKE_C1_MIN
            if why:
                pass
            elif ".2C." in name or name.endswith("_2C") or "_2C_" in name or fake:
                if str(r.get("tc", "2")) not in ("2", ""):
                    why = "the unit played on one core"
                elif num(r, "tcsilent") > 0 or num(r, "tcvlate") > 0:
                    why = "part 2 silent or a very late block"
            av, c1v, vx = num(r, "avg"), num(r, "c1"), num(r, "vox")
            # tot: the whole scene on core 0 + core 1 (both parts). An idle scene (no voices) has no part 2 work: core 1's share there is its own loop, not the scene's
            row = dict(avg=av, peak=num(r, "peak"), vox=vx, c1=c1v, tot=(av + c1v) if (is1c and vx > 0) else av, fake1c=bool(fake), is1c=is1c, tcown=num(r, "tcown"),
                       miss=num(r, "miss"), uac=num(r, "uac_under"), tcsilent=num(r, "tcsilent"), rep=int(num(r, "rep", 1) or 1), ok=not why, why=why)
            if why:
                bad.append((name, row["rep"], why))
            if not why or all_rows:
                rows.setdefault(name, []).append(row)
    return rows, bad


# ------------------------------------------------------------------------------------------------ small statistics
def mean(x):
    return sum(x) / len(x)


def sd(x):
    return statistics.stdev(x) if len(x) > 1 else None


def ols(xs, ys):
    """y = a + s x. -> (a, s, se_a, se_s, n, rms); the errors need two spare points, else None"""
    n = len(xs)
    mx, my = mean(xs), mean(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    if n < 2 or sxx < 1e-9:
        return None
    s = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    a = my - s * mx
    if n > 2:
        sse = sum((y - a - s * x) ** 2 for x, y in zip(xs, ys))
        sig = math.sqrt(sse / (n - 2))
    else:
        sig = None
    return dict(a=a, s=s, n=n, sig=sig, sxx=sxx, mx=mx,
                se_s=(sig / math.sqrt(sxx) if sig is not None else None),
                se_a=(sig * math.sqrt(1.0 / n + mx * mx / sxx) if sig is not None else None))


def tag_of(val, se):
    if se is None:
        return "ONE RUN"
    if abs(val) < 2 * se or val == 0:
        return "BELOW NOISE"
    r = se / abs(val)
    return "GOOD" if r < 0.10 else "OK" if r < 0.25 else "LOW"


class Fit:
    """the rows of a run, by plan name; the lines and effects measured from them"""

    def __init__(self, rows):
        self.rows = rows
        reps = [sd([r["avg"] for r in v]) for v in rows.values() if len(v) > 1]
        reps = [x for x in reps if x is not None]
        self.sigma = max(math.sqrt(mean([x * x for x in reps])) if reps else SIGMA_FLOOR, SIGMA_FLOOR)     # pooled spread of one run, % of a half
        self.rows_out = []                                          # the table rows for the report
        self.lines = {}
        all1c = [r for k, v in rows.items() for r in v if r["is1c"]]
        self.n1c, self.nfake = len(all1c), sum(1 for r in all1c if r["fake1c"])
        self.fake = self.nfake > 0 and self.nfake >= 0.5 * self.n1c       # the one-core arm is not one-core: its rows are read as avg + c1 (and as extra two-core data)

    def pts(self, sec, eng, cores, feat, per_part=False):
        """[(voices, instructions, core 1 instructions, K)] of every usable run of (section, engine, cores, feature)"""
        out = []
        for k, nm in D.names_for(sec, eng, cores, feat):
            for r in self.rows.get(nm, []):
                # a one-core name: the whole scene (avg + c1; a really one-core row has c1 about 0, so this is its avg)
                out.append((r["vox"] / 2.0 if per_part else r["vox"], (r["tot"] if cores == 1 else r["avg"]) * PCT, r["c1"] * PCT, k))
        if cores == 2:                                              # a fake one-core test is a two-core test: core 0's avg and core 1's c1 apart
            for k, nm in D.names_for(sec, eng, 1, feat):
                for r in self.rows.get(nm, []):
                    if r["fake1c"]:
                        out.append((r["vox"] / 2.0 if per_part else r["vox"], r["avg"] * PCT, r["c1"] * PCT, k))
        return out

    def mean_at(self, sec, eng, cores, feat, k):
        p = [x for x in self.pts(sec, eng, cores, feat) if x[3] == k]
        return (mean([x[0] for x in p]), mean([x[1] for x in p]), len(p)) if p else None

    def base_line(self, eng, cores):
        """avg = a + s x voices through the baseline's key counts (voices of one part on two cores: the critical path). Counts the pool or the engine capped are left out and kept in L['capped']"""
        xs, ys, cs, capped = [], [], [], []
        cap = D.eng_cap(eng)
        for v, y, c, k in self.pts("B", eng, cores, "BASE", per_part=(cores == 2)):
            want = min(k, cap) * (1 if cores == 2 else 2)
            if v < 0.85 * want:
                capped.append((k, v, y))
                continue
            xs.append(v), ys.append(y), cs.append(c)
        if len(set(xs)) < 2:
            return None
        L = ols(xs, ys)
        L["capped"] = capped
        if cores == 2:
            L["c1"] = ols(xs, cs)
        return L

    def put(self, key, label, val, se, n, old=None, note="", tag=None):
        self.rows_out.append(dict(key=key, label=label, val=val, se=se, n=n, tag=tag or tag_of(val, se), old=old, note=note))
        return val

    # --- what a feature adds
    def effect(self, fk, mode_tag="", eng="ANALOG"):
        """the feature on (ANALOG, one core) against its reference: dict(n, mean, se_mean, and for two or more voice counts s, a, se_s, se_a), or None"""
        f = D.FEAT[fk]
        sec = "F" if f["kind"] != "G" else "G"
        mine = self.pts(sec, eng, 1, fk + mode_tag)
        if not mine:
            return None
        ref = f["ref"]
        sig = self.sigma * PCT
        if ref in (None, "NONE"):
            if mode_tag:                                           # DUEL / STEREO: against their own baseline, same count
                def refn(v, k):
                    m = self.mean_at("G", eng, 1, "G_BASE" + mode_tag, k)
                    return m[1] if m else float("nan")
            else:
                B = self.lines.get((eng, 1))

                def refn(v, k):
                    return B["a"] + B["s"] * v if B else float("nan")
        else:
            rf = D.FEAT[ref]
            rsec = "F" if rf["kind"] != "G" else "G"
            rpts = self.pts(rsec, eng, 1, ref + mode_tag)
            RL = ols([x[0] for x in rpts], [x[1] for x in rpts]) if len({x[0] for x in rpts}) >= 2 else None

            def refn(v, k):
                m = self.mean_at(rsec, eng, 1, ref + mode_tag, k)
                if m:
                    return m[1]
                return RL["a"] + RL["s"] * v if RL else float("nan")
        pts = []
        for (v, y, c, k) in mine:
            r = refn(v, k)
            if r == r:
                pts.append((v, y - r, k))
        if not pts:
            return None
        ds = [p[1] for p in pts]
        out = dict(n=len(pts), mean=mean(ds), pts=pts)
        sdv = sd(ds)
        out["se_mean"] = (sdv / math.sqrt(len(ds))) if sdv is not None else sig / math.sqrt(len(ds))
        xs = [p[0] for p in pts]
        if len(set(xs)) >= 2:
            L = ols(xs, ds)
            out.update(s=L["s"], a=L["a"], se_s=L["se_s"] if L["se_s"] is not None else sig * math.sqrt(2.0) / math.sqrt(L["sxx"]),
                       se_a=L["se_a"] if L["se_a"] is not None else sig * 1.5)
        return out

    def cross(self, fk, agg):
        """a per-voice feature on every engine: {engine: (instructions a voice, spread, n)}; the other engines were run with one voice count, the fixed part taken as 0"""
        res = {}
        sig = self.sigma * PCT
        for e in D.ENG_ORDER:
            L = self.lines.get((e, 1))
            if not L:
                continue
            if e == "ANALOG":
                o = self.effect(fk)
                if o and "s" in o:
                    res[e] = (o["s"], o["se_s"], o["n"])
                continue
            dv = [(y - self.line_at(L, v)) / v for (v, y, c, k) in self.pts("X", e, 1, fk) if v > 0]
            if dv:
                res[e] = (mean(dv), (sd(dv) or sig / 4.0) / math.sqrt(len(dv)), len(dv))
        return res

    @staticmethod
    def line_at(L, x):
        return L["a"] + L["s"] * x


def fit_table(rows, model, agg="max"):
    """the whole fit. rows from load_run; model: the table now in the sources (read_model), for the rows the run does not reach. -> (Fit, T)"""
    F = Fit(rows)
    g, ge = {}, {}
    T = dict(eng={}, eng2={}, g=g, ge=ge, meta={}, sigma_pct=F.sigma)
    sig = F.sigma * PCT
    lines2 = {}
    for e in D.ENG_ORDER:
        for cores in (1, 2):
            L = F.base_line(e, cores)
            if L:
                (F.lines if cores == 1 else lines2)[(e, 1) if cores == 1 else e] = L
    A1 = F.lines.get(("ANALOG", 1))

    def se_of(L):
        return L["se_s"] if L["se_s"] is not None else sig / math.sqrt(max(L["sxx"], 1.0))
    # ---------------------------------------------------------- engines
    for e in D.ENG_ORDER:
        old = model["eng"].get(e)
        L1, L2 = F.lines.get((e, 1)), lines2.get(e)
        T["eng"][e] = F.put(f"eng.{e}", f"{e} one voice, MULTI bypassed (one core)" + (", from avg + c1" if F.fake else ""), L1["s"], se_of(L1), L1["n"], old,
                            note=("capped (the pool or the engine): " + ", ".join(f"{k} keys -> {v:.0f} voices" for k, v, _ in L1["capped"])) if L1["capped"] else "") if L1 else old
        T["eng2"][e] = F.put(f"eng2.{e}", f"{e} one voice on the critical path (two cores; the voices of one part)", L2["s"], se_of(L2), L2["n"], T["eng"].get(e),
                             note="the model charges the one-core row on both paths" + ("; two-core data from the .2C. and the (fake) .1C. tests" if F.fake else "")) if L2 else T["eng"].get(e)
        if L2 and L2.get("c1"):
            F.put(f"core1.{e}", f"{e} one voice on core 1 (its busy share, two cores)", L2["c1"]["s"], se_of(L2["c1"]), L2["c1"]["n"], T["eng"].get(e), note="core 1 against core 0, same part")
    # ---------------------------------------------------------- the mix, a part's own work, the hand-over
    idle = F.mean_at("G", "ANALOG", 1, "G_IDLE", 0)
    idle_d = F.mean_at("G", "ANALOG", 1, "G_IDLE_DUEL", 0)
    g["idle"] = {"LAYER": F.put("idle.LAYER", "the mix with nothing playing, SENDS OFF (STEREO and LAYER)", idle[1], sig / math.sqrt(idle[2]), idle[2], model["idle"]["LAYER"]) if idle else model["idle"]["LAYER"],
                 "DUEL": F.put("idle.DUEL", "the mix with nothing playing, SENDS OFF (DUEL)", idle_d[1], sig / math.sqrt(idle_d[2]), idle_d[2], model["idle"]["DUEL"]) if idle_d else model["idle"]["DUEL"]}
    if F.fake and idle:
        # item 110: the one-core arm ran on two cores, so its intercept holds the hand-over. Take the hand-over from the two-core lines (core 0's intercept less the idle mix less the
        # part's work on core 1, the parts being the same sound and the cores the same speed) and give the part's work as the one-core intercept less that, per part. Both are ESTIMATES:
        # they hold core 1's own loop and any speed difference; the patch never takes them
        ho = [lines2[e]["a"] - g["idle"]["LAYER"] - lines2[e]["c1"]["a"] for e in lines2 if lines2[e].get("c1")]
        hoe = statistics.median(ho) if ho else float(model["ho"])
        pon = [(F.lines[(e, 1)]["a"] - hoe - g["idle"]["LAYER"]) / 2.0 for e in D.ENG_ORDER if (e, 1) in F.lines]
        g["part_on"] = F.put("part_on", "a part's own work while it sounds (BD_PART_ON): ESTIMATE, one-core intercept (avg + c1) less the idle mix and the estimated hand-over, per part",
                             statistics.median(pon), (sd(pon) or sig) / math.sqrt(len(pon)), len(pon), model["part_on"], note=f"over {len(pon)} engines {min(pon):.0f} .. {max(pon):.0f}; needs a real one-core run",
                             tag="ESTIMATE") if pon else model["part_on"]
        g["ho"] = F.put("ho", "the two-core hand-over on core 0 (BUDGET_HO): ESTIMATE, core 0's two-core intercept less the idle mix less core 1's intercept", hoe, (sd(ho) or sig) / math.sqrt(max(len(ho), 1)),
                        len(ho), model["ho"], note=(f"over {len(ho)} engines {min(ho):.0f} .. {max(ho):.0f}; " if ho else "") + "includes core 1's own loop; needs a real one-core run", tag="ESTIMATE") if ho else model["ho"]
    else:
        pon = [(F.lines[(e, 1)]["a"] - g["idle"]["LAYER"]) / 2.0 for e in D.ENG_ORDER if (e, 1) in F.lines] if idle else []
        g["part_on"] = F.put("part_on", "a part's own work while it sounds (BD_PART_ON): the baseline lines' intercept less the idle mix, per part", statistics.median(pon),
                             (sd(pon) or sig) / math.sqrt(len(pon)), len(pon), model["part_on"], note=f"over {len(pon)} engines {min(pon):.0f} .. {max(pon):.0f}") if pon else model["part_on"]
        ho = [lines2[e]["a"] - g["idle"]["LAYER"] - g["part_on"] for e in lines2] if idle else []
        g["ho"] = F.put("ho", "the two-core hand-over on core 0 (BUDGET_HO): the two-core line's intercept less the idle mix and one part's work", statistics.median(ho), (sd(ho) or sig) / math.sqrt(len(ho)),
                        len(ho), model["ho"], note=f"over {len(ho)} engines {min(ho):.0f} .. {max(ho):.0f}") if ho else model["ho"]

    # ---------------------------------------------------------- per-voice features
    def pv(key, fk, label, old, fixed_key=None):
        o = F.effect(fk)
        if not o or "s" not in o:
            g[key] = old
            return None
        fixed = o["a"] / 2.0 if abs(o["a"]) > 2 * o["se_a"] else 0.0
        g[key] = F.put(key, label, o["s"], o["se_s"], o["n"], old, note=f"+ a fixed part of {fixed:.0f} a part" if fixed else "")
        if fixed_key:
            g[fixed_key] = fixed
        return g[key]

    def shared(key, fk, label, old):
        res = F.cross(fk, agg)
        if not res:
            g[key] = old
            return
        ge[key] = {e: v[0] for e, v in res.items()}
        for e, (v, se, n) in res.items():
            F.put(f"{key}@{e}", f"{label} on {e}", v, se, n, old, note="" if e == "ANALOG" else "one voice count, the fixed part taken as 0")
        vals = [v[0] for v in res.values()]
        g[key] = F.put(key, f"{label}, the row all engines share ({agg} of {len(vals)} engines)", max(vals) if agg == "max" else statistics.median(vals), max(v[1] for v in res.values()),
                       sum(v[2] for v in res.values()), old, note=f"engines {min(vals):.0f} .. {max(vals):.0f}")
    shared("f12", "F12", "MULTI 12 dB, a voice", model["f12"])
    shared("f24", "F24", "MULTI 24 dB, a voice", model["f24"])
    pv("ktrk", "KTRK", "KEY TRK on the running filter, a voice", model["ktrk"])
    pv("morph_st", "MORPH_ST", "MORPH present, fader still, a voice", model["morph"], "morph_fix_st")
    pv("morph_sw", "MORPH_SW", "MORPH present, fader moving, a voice", model["morph"], "morph_fix_sw")
    shared("morph_st", "MORPH_ST", "MORPH present, fader still, a voice", model["morph"])
    for key, fk, label in (("tw_svf", "TW_LP", "TWIN LP after MULTI 12 (SVF group), a voice"), ("tw_ladder", "TW_LAD", "TWIN LADDER after MULTI 12, a voice"), ("tw_comb", "TW_COMB", "TWIN COMB after MULTI 12, a voice"),
                           ("tw_drive", "TW_DRV", "TWIN DRIVE, a voice"), ("tw_par", "TW_PAR", "TWIN ROUTE PARALLEL, a voice"), ("tw_mod", "TW_MOD", "TWIN modulation (ENV 2 to CUTOFF 2), a voice")):
        pv(key, fk, label, model[key])
    o = F.effect("TW_LPB")
    if o and "s" in o:
        g["tw_chain_byp"] = F.put("tw_chain_byp", "TWIN LP with MULTI bypassed, a voice, less the SVF row (the chain path's extra)", o["s"] - g["tw_svf"], math.hypot(o["se_s"], 0.0), o["n"], model["tw_chain_byp"])
    else:
        g["tw_chain_byp"] = model["tw_chain_byp"]
    pv("tw_bp", "TW_BP", "TWIN BP after MULTI 12 (same group as LP?), a voice", model["tw_svf"])
    g.pop("tw_bp", None)
    for w in ("SINE", "TRI", "SQ", "PULSE"):
        pv("sub_" + w, "SUB_" + w, f"SUB {w}, a voice", model["sub_" + w])
    shared("sub_SQ_x", "SUB_SQ", "SUB SQUARE, a voice", model["sub_SQ"])
    ge.pop("sub_SQ_x", None)
    g.pop("sub_SQ_x", None)
    pv("mx_slot", "MX_KEY", "one matrix slot, a voice", model["mx_slot"], "mx_fix")
    g.setdefault("mx_fix", 0.0)
    pv("mx_lfo", "MX_LFO", "LFO 1 read by a slot, a voice (less the slot)", model["mx_lfo"])
    pv("mx_cyc", "MX_CYC", "CYCLE read by a slot, a voice (less the slot)", model["mx_cyc"])
    for k in ("morph_fix_st", "morph_fix_sw"):
        g.setdefault(k, 0.0)
    uni = F.pts("F", "ANALOG", 1, "UNI")
    if A1 and len({u[0] for u in uni}) >= 2:
        U = ols([u[0] for u in uni], [u[1] for u in uni])
        g["uni_extra"] = F.put("uni_extra", "UNISON: a voice's cost above the plain voice's (the model charges none)", U["s"] - A1["s"], math.hypot(se_of(U), se_of(A1)), U["n"], 0,
                               note=f"voices sounding {min(u[0] for u in uni):.0f} .. {max(u[0] for u in uni):.0f}")
    else:
        g["uni_extra"] = 0.0
    # ---------------------------------------------------------- the part's fixed stages: the delta of both parts / 2
    g["ins"] = [0.0] + [float(model["ins"][i]) for i in range(1, 11)]
    for i in range(1, 11):
        o = F.effect(f"INS{i}")
        if o:
            g["ins"][i] = F.put(f"ins.{i}", f"INSERT {D.INS_NAMES[i]}, a part", o["mean"] / 2.0, o["se_mean"] / 2.0, o["n"], model["ins"][i], note=_vdep(o))
    for key, fk, label in (("sat", "SAT", "SAT 64, a part"), ("duck", "DUCK", "DUCK, a part")):
        o = F.effect(fk)
        g[key] = F.put(key, label, o["mean"] / 2.0, o["se_mean"] / 2.0, o["n"], model[key], note=_vdep(o)) if o else model[key]
    eqs = []
    for n in (1, 2, 3):
        o = F.effect(f"EQ{n}")
        if o:
            eqs.append((n, o["mean"] / 2.0, o["se_mean"] / 2.0, o["n"]))
            F.put(f"eq.{n}", f"EQ {n} band(s), a part", o["mean"] / 2.0, o["se_mean"] / 2.0, o["n"], model["eq_base"] + n * model["eq_band"], note=_vdep(o))
    if len(eqs) >= 2:
        Q = ols([e[0] for e in eqs], [e[1] for e in eqs])
        sq = max(e[2] for e in eqs)
        g["eq_band"] = F.put("eq_band", "EQ: one band with a gain, a part (line through 1, 2, 3 bands)", Q["s"], sq, len(eqs), model["eq_band"])
        g["eq_base"] = F.put("eq_base", "EQ: the stage's fixed part (line through 1, 2, 3 bands)", Q["a"], sq * 1.5, len(eqs), model["eq_base"])
    else:
        g["eq_band"], g["eq_base"] = model["eq_band"], model["eq_base"]
    # ---------------------------------------------------------- the buses and the master (two keys; DUEL against its own baseline)
    for mkey, tag in (("LAYER", ""), ("DUEL", "_DUEL")):
        for key, fk, label, old in (("bus", "G_BUS", "SENDS ON, nothing fed (HALL + DIGITAL)", model["bus"][mkey]), ("cho", "G_CHO", "CHORUS fed", model["cho"][mkey]),
                                    ("shd", "G_SHD", "SHIM, PITCH DOWN", model["shd"][mkey]), ("shm", "G_SHM", "SHIM UP, MOD 50", model["shm"][mkey]),
                                    ("shdm", "G_SHDM", "SHIM DOWN, MOD 50", model["shm"][mkey] + model["shd"][mkey] - model["bus"][mkey])):
            o = F.effect(fk, tag)
            g.setdefault(key, {})[mkey] = F.put(f"{key}.{mkey}", f"{label} [{mkey}] (the bus section against SENDS OFF)", o["mean"], o["se_mean"], o["n"], old) if o else old
        g.setdefault("dly", {})[mkey] = []
        g.setdefault("rev", {})[mkey] = []
        for i in range(4):
            o = F.effect(f"G_DLY{i}", tag)
            g["dly"][mkey].append(F.put(f"dly.{mkey}.{i}", f"DELAY {D.DLY_NAMES[i]} fed [{mkey}]", o["mean"], o["se_mean"], o["n"], model["dly"][mkey][i]) if o else model["dly"][mkey][i])
        for i in range(6):
            o = F.effect(f"G_REV{i}", tag)
            g["rev"][mkey].append(F.put(f"rev.{mkey}.{i}", f"REVERB {D.REV_NAMES[i]} fed [{mkey}]", o["mean"], o["se_mean"], o["n"], model["rev"][mkey][i]) if o else model["rev"][mkey][i])
    st = F.effect("G_BUS", "_STEREO")
    if st:
        F.put("bus.STEREO", "SENDS ON, nothing fed [STEREO] (the model uses the LAYER row)", st["mean"], st["se_mean"], st["n"], g["bus"]["LAYER"], note="check: STEREO against LAYER")
    for key, fk, label, old in (("comp", "G_COMP", "master COMPRESSOR 4:1", model["comp"]), ("width", "G_WIDTH", "master WIDTH 150 %", model["width"]), ("bm", "G_BM", "master BASS MONO", model["bm"]),
                                ("keysense", "G_KSV", "KEY VELOCITY on (only the audio interrupt's share: the scan tick is not in the unit's figure)", 57)):
        o = F.effect(fk)
        g[key] = F.put(key, label, o["mean"], o["se_mean"], o["n"], old, note="NOT the scan tick (TIMER5 is taken out of the figure): its cost is not measured here" if key == "keysense" else "") if o else old
    # ---------------------------------------------------------- checks that are not table rows
    checks = []
    for fk, on, label in (("BYP_SAT", "SAT", "SAT with its BYPASS on"), ("BYP_INS", "INS4", "INSERT FUZZ with its BYPASS on"), ("BYP_EQ", "EQ3", "EQ 3 with its BYPASS on"),
                          ("BYP_MULTI", "F24", "MULTI 24 dB with its BYPASS on"), ("BYP_TWIN", "TW_LP", "TWIN LP with its BYPASS on"), ("G_BYPC", "G_CHO", "CHORUS send with the CHORUS BYPASS on"),
                          ("G_BYPD", "G_DLY0", "DELAY send with the DELAY BYPASS on"), ("G_BYPR", "G_REV2", "REVERB send with the REVERB BYPASS on"), ("G_BYPM", "G_ALLM", "COMP + WIDTH + BASS MONO with the master BYPASS on")):
        o = F.effect(fk)
        if o:
            checks.append((label, o["mean"], o["se_mean"], o["n"], f"minus the same without the BYPASS ({on}): 0 = the BYPASS saves nothing, minus the whole cost = it saves all"))
    for fk, label, txt in (("G_REV0S", "REVERB SHIM with send 0, against SENDS ON with no type chosen", "0 = the send level does not matter: the bus runs at send 0"),
                           ("G_DLY0S", "DELAY OCT UP with send 0, against the same", "0 = the send level does not matter"), ("G_MUTE2", "part 2 muted, against both parts sounding", "negative: the muted part's voices are gone")):
        o = F.effect(fk)
        if o:
            checks.append((label, o["mean"], o["se_mean"], o["n"], txt))
    T["checks"] = checks
    # ---------------------------------------------------------- the factory sounds
    T["sounds"] = {}
    for e, n in D.sound_counts().items():
        L = F.lines.get((e, 1))
        if not L:
            continue
        vals = []
        for s in range(n):
            for (v, y, c, k) in F.pts("S", e, 1, f"S{s}"):
                if v > 0:
                    vals.append((s, (y - L["a"]) / v))
        if vals:
            vs = [v for _, v in vals]
            T["sounds"][e] = dict(n=len(vs), min=min(vs), mean=mean(vs), max=max(vs), dear=max(vals, key=lambda x: x[1])[0], row=T["eng"].get(e))
    return F, T


def _vdep(o):
    if "s" in o and o["se_s"] is not None and abs(o["s"]) > 3 * o["se_s"] and abs(o["s"]) > 1.0:
        return f"WARNING: depends on the voices ({o['s']:+.1f} an extra voice): not a fixed part stage"
    return ""


# ------------------------------------------------------------------------------------------------ the model now in the sources
def read_model(src):
    def rd(f):
        return open(os.path.join(src, f)).read()
    b, bf, bh = rd("budget.c"), rd("budget_feat.c"), rd("budget.h")

    def d(t, name):
        m = re.search(r"#define\s+" + name + r"\s+(\d+)u?", t)
        return int(m.group(1))

    def arr(t, name):
        m = re.search(name + r"(?:\[[^\]]*\])+\s*=\s*\{(.*?)\};", t, re.S)
        return [int(x) for x in re.findall(r"\d+", m.group(1))]
    eng = {}
    for m in re.finditer(r"\{&ENG_(\w+),\s*(\d+)\}", b):
        eng["VOICE" if m.group(1) == "FORMANT" else m.group(1)] = int(m.group(2))
    ins = arr(b, "BD_INS")
    dly, rev = arr(b, "BD_DLY"), arr(b, "BD_REV")
    M = dict(eng=eng, f12=d(b, "BD_F12"), f24=d(b, "BD_F24"), part_on=d(b, "BD_PART_ON"), morph=d(b, "BD_MORPH"), ins=ins, sat=d(b, "BD_SAT"), eq_base=d(b, "BD_EQ_BASE"), eq_band=d(b, "BD_EQ_BAND"),
             duck=d(b, "BD_DUCK"), ktrk=d(b, "BD_KTRK"), mx_slot=d(b, "BD_MX_SLOT"), mx_lfo=d(b, "BD_LFO"), mx_cyc=d(bf, "BD_CYCLE"),
             idle=dict(zip(("LAYER", "DUEL"), arr(b, "BD_IDLE"))), bus=dict(zip(("LAYER", "DUEL"), arr(b, "BD_BUS"))), cho=dict(zip(("LAYER", "DUEL"), arr(b, "BD_CHO"))),
             dly={"LAYER": dly[:4], "DUEL": dly[4:]}, rev={"LAYER": rev[:6], "DUEL": rev[6:]}, shd=dict(zip(("LAYER", "DUEL"), arr(b, "BD_SHD"))), shm=dict(zip(("LAYER", "DUEL"), arr(b, "BD_SHM"))),
             comp=d(b, "BD_COMP"), width=d(b, "BD_WIDTH"), bm=d(b, "BD_BMONO"), ho=d(bh, "BUDGET_HO"),
             sub_SQ=d(bf, "BD_SUB_PLS"), sub_PULSE=d(bf, "BD_SUB_PLS"), sub_SINE=d(bf, "BD_SUB_SINE"), sub_TRI=d(bf, "BD_SUB_TRI"),
             tw_chain_byp=d(bf, "BD_TW_CHAIN_BYP"), tw_svf=d(bf, "BD_TW_SVF"), tw_ladder=d(bf, "BD_TW_LADDER"), tw_comb=d(bf, "BD_TW_COMB"), tw_drive=d(bf, "BD_TW_DRIVE"), tw_par=d(bf, "BD_TW_PAR"),
             tw_mod=d(bf, "BD_TW_MOD"), steps=d(bf, "BD_STEPS"))
    return M


def table_from_model(M):
    """the sources' table in the shape predict_avg reads (the selftest's truth is this, perturbed)"""
    g = dict(f12=M["f12"], f24=M["f24"], ktrk=M["ktrk"], morph_st=M["morph"], morph_sw=M["morph"], morph_fix_st=0.0, morph_fix_sw=0.0, tw_svf=M["tw_svf"], tw_ladder=M["tw_ladder"], tw_comb=M["tw_comb"],
             tw_chain_byp=M["tw_chain_byp"], tw_drive=M["tw_drive"], tw_par=M["tw_par"], tw_mod=M["tw_mod"], sub_SINE=M["sub_SINE"], sub_TRI=M["sub_TRI"], sub_SQ=M["sub_SQ"], sub_PULSE=M["sub_PULSE"],
             mx_slot=M["mx_slot"], mx_lfo=M["mx_lfo"], mx_cyc=M["mx_cyc"], mx_fix=0.0, uni_extra=0.0, part_on=M["part_on"], ins=[0.0] + list(M["ins"][1:]), sat=M["sat"], eq_base=M["eq_base"],
             eq_band=M["eq_band"], duck=M["duck"], idle=dict(M["idle"]), bus=dict(M["bus"]), cho=dict(M["cho"]), dly=json.loads(json.dumps(M["dly"])), rev=json.loads(json.dumps(M["rev"])), shd=dict(M["shd"]),
             shm=dict(M["shm"]), shdm={m: M["shm"][m] + M["shd"][m] - M["bus"][m] for m in M["bus"]}, comp=M["comp"], width=M["width"], bm=M["bm"], keysense=57.0, ho=float(M["ho"]))
    return dict(eng={e: float(v) for e, v in M["eng"].items()}, eng2={e: float(v) for e, v in M["eng"].items()}, g=g, ge={}, meta={})


# ------------------------------------------------------------------------------------------------ the report
def conf_summary(F):
    c = {}
    for r in F.rows_out:
        c[r["tag"]] = c.get(r["tag"], 0) + 1
    return c


SECTIONS = [("eng.", "one voice of each engine, MULTI bypassed, one core"), ("eng2.", "one voice on the critical path, two cores"), ("core1.", "one voice on core 1 (its busy share)"),
            ("idle.", "the mix"), ("part_on", "a part's own work and the hand-over"), ("ho", "a part's own work and the hand-over"),
            ("f12", "the multimode filter"), ("f24", "the multimode filter"), ("ktrk", "the multimode filter"), ("morph", "MORPH"), ("tw_", "TWIN FILTER"), ("sub_", "SUB"), ("mx_", "matrix, UNISON"), ("uni", "matrix, UNISON"),
            ("ins.", "a part's stages (per part)"), ("sat", "a part's stages (per part)"), ("eq", "a part's stages (per part)"), ("duck", "a part's stages (per part)"),
            ("bus", "the buses (the whole bus section against SENDS OFF)"), ("cho.", "the buses (the whole bus section against SENDS OFF)"), ("dly.", "the buses (the whole bus section against SENDS OFF)"),
            ("rev.", "the buses (the whole bus section against SENDS OFF)"), ("sh", "the buses (the whole bus section against SENDS OFF)"), ("comp", "the master stages"), ("width", "the master stages"),
            ("bm", "the master stages"), ("keysense", "KEY SENSE")]


def section_of(key):
    for pre, name in SECTIONS:
        if key.startswith(pre):
            return name
    return "other"


def peak_stats(rows):
    q = sorted(r["peak"] / r["avg"] for v in rows.values() for r in v if r["avg"] > 10.0)
    return (q[len(q) // 2], q[int(len(q) * 0.95)], q[-1], len(q)) if q else None


def report(F, T, model, bad, nrows, path_csv, rows=None):
    L = []
    P = L.append
    P("DUELL COST-CAL: the measured cost table (host instructions a sample; 3281.25 = 100 % of a half; the unit's own figure x 32.8125)")
    P(f"run: {path_csv}   rows used {nrows}   unusable {len(bad)}   one run's spread {F.sigma:.2f} % of a half = {F.sigma * PCT:.1f} instructions")
    P("tag: GOOD spread under 10 %, OK under 25 %, LOW more, BELOW NOISE the value is under two spreads (only an upper bound), ONE RUN no spread known.  old = the row in the sources now, x = measured / old.")
    P("A voice row is a straight line's slope through the key counts; a part's stage is the difference of both parts divided by two; a bus row is the whole bus section against SENDS OFF (budget.c's definition).")
    P("")
    if F.n1c:
        if F.fake:
            P(f"DATA CHECK: THE ONE-CORE ARM IS NOT ONE-CORE. {F.nfake} of {F.n1c} usable '.1C.' runs had the unit on two cores with core 1 busy and nothing taken over (tcown 0): `tune slack` does not")
            P("  make core 0 play part 2 itself while core 1 is up (twocore_run.c, item 90). Those rows are read as avg + c1 (core 0 plus core 1 = both parts, the hand-over still inside) and also as")
            P("  extra two-core data (avg and c1 apart). Per-voice and feature costs are fine that way; the hand-over and the part's own work are ESTIMATES (never in the patch); a one-core run's own")
            P("  numbers (the 1-core critical path, the true hand-over, whether core 0 alone fits) are NOT measured. A real one-core arm is needed for them (HANDOFF_V2 item 110).")
        elif F.nfake:
            P(f"DATA CHECK: {F.nfake} of {F.n1c} '.1C.' runs were not really one-core (two cores, core 1 busy, tcown 0); the other {F.n1c - F.nfake} were. Mixed: the fake ones are read as avg + c1; look at them.")
        else:
            P(f"DATA CHECK: all {F.n1c} '.1C.' runs were really on one core (core 1 not playing part 2).")
    if bad:
        P("unusable runs (left out of every fit): " + "; ".join(f"{n} #{r}: {w}" for n, r, w in bad[:12]) + (" ..." if len(bad) > 12 else ""))
    groups = {}
    for r in F.rows_out:
        groups.setdefault(section_of(r["key"]), []).append(r)
    for sec, lst in groups.items():
        P("")
        P(f"== {sec}")
        P(f"{'row':62s} {'measured':>8s} {'+-':>6s} {'n':>3s} {'old':>5s} {'x':>5s}  {'tag':11s} note")
        for r in lst:
            old = "" if r["old"] is None else f"{r['old']:.0f}"
            x = "" if not r["old"] else f"{r['val'] / r['old']:.2f}"
            P(f"{r['label'][:62]:62s} {r['val']:8.1f} {('' if r['se'] is None else format(r['se'], '6.1f')):>6s} {r['n']:3d} {old:>5s} {x:>5s}  {r['tag']:11s} {r['note']}")
    if T["checks"]:
        P("")
        P("== checks (not table rows)")
        for (lab, v, se, n, txt) in T["checks"]:
            P(f"  {lab[:56]:56s} {v:8.1f} +-{se:5.1f} (n {n})  {txt}")
    if T["sounds"]:
        P("")
        P("== the engines' factory sounds (one voice, cleaned, MULTI bypassed; the engine row in use is the dearest sound's)")
        for e, s in T["sounds"].items():
            P(f"  {e:7s} {s['n']:3d} sounds   min {s['min']:6.0f}   mean {s['mean']:6.0f}   max {s['max']:6.0f} (sound {s['dear']})   engine row {'' if s['row'] is None else format(s['row'], '.0f')}")
    P("")
    P("== in words")
    r = [(e, T["eng2"][e] / T["eng"][e]) for e in T["eng2"] if T["eng"].get(e) and T["eng2"].get(e)]
    if r:
        P("  two cores against one core, the same voice (critical path / one core): " + ", ".join(f"{e} {x:.2f}" for e, x in r) + "   (1.00: core 1 as fast as core 0 and no cache cost)")
    if F.fake:
        sp = []
        for e in D.ENG_ORDER:
            p4 = [x for x in F.pts("B", e, 2, "BASE", per_part=True) if x[3] == 4]
            if p4:
                a0, a1 = mean([x[1] for x in p4]) / PCT, mean([x[2] for x in p4]) / PCT
                sp.append(f"{e} {a0:.1f} / {a1:.1f} (sum {a0 + a1:.1f})")
        if sp:
            P("  two cores, 4 keys a part, core 0 avg / core 1 c1 (the sum is the one-core equivalent plus the hand-over): " + "; ".join(sp))
    ps = peak_stats(rows) if rows else None
    if ps:
        P(f"  the peak half against the average over {ps[3]} runs: median x{ps[0]:.2f}, 95 % of runs under x{ps[1]:.2f}, worst x{ps[2]:.2f}: an average of 80 % / x{ps[1]:.2f} = {80 / ps[1]:.0f} % average "
          f"is where the worst half of an ordinary run touches 80 %")
    P(f"  rows by tag: {conf_summary(F)}")
    return "\n".join(L) + "\n"


# ------------------------------------------------------------------------------------------------ the patch
def rd_text(src, f):
    return open(os.path.join(src, f)).read()


def sub_define(t, name, val, changed):
    m = re.search(r"(#define\s+" + name + r"\s+)(\d+)(u?)", t)
    new = int(round(val))
    if m and int(m.group(2)) != new:
        changed.append((name, int(m.group(2)), new))
        t = t[:m.start(2)] + str(new) + t[m.end(2):]
    return t


def sub_array(t, name, flat, changed, shape=None):
    m = re.search(r"(" + name + r"(?:\[[^\]]*\])*\s*(?:BENCH_RO\s*)?=\s*\{)(.*?)(\};)", t, re.S)
    old = [int(x) for x in re.findall(r"\d+", m.group(2))]
    new = [int(round(v)) for v in flat]
    if old == new:
        return t
    changed.append((name, old, new))
    it = iter(new)
    body = re.sub(r"\d+", lambda _m: str(next(it)), m.group(2))
    return t[:m.start(2)] + body + t[m.end(2):]


def usable(F, key):
    """a measured row is put in the patch when it has a spread and is not noise; else the old row stays (a patch line the run could not support would be a guess)"""
    for r in F.rows_out:
        if r["key"] == key:
            return r["tag"] in ("GOOD", "OK", "LOW")
    return False


def make_patch(F, T, src):
    g = T["g"]
    files = {f: rd_text(src, f) for f in ("budget.c", "budget_feat.c", "budget.h")}
    new = dict(files)
    ch = []
    b, bf, bh = new["budget.c"], new["budget_feat.c"], new["budget.h"]
    keep = []

    def take(key, val, old):
        if usable(F, key):
            return val
        keep.append(key)
        return old
    M = read_model(src)
    for name, key in (("BD_F12", "f12"), ("BD_F24", "f24"), ("BD_PART_ON", "part_on"), ("BD_SAT", "sat"), ("BD_EQ_BASE", "eq_base"), ("BD_EQ_BAND", "eq_band"), ("BD_DUCK", "duck"),
                      ("BD_KTRK", "ktrk"), ("BD_MX_SLOT", "mx_slot"), ("BD_LFO", "mx_lfo"), ("BD_COMP", "comp"), ("BD_WIDTH", "width"), ("BD_BMONO", "bm")):
        b = sub_define(b, name, take(key, g[key], M[{"f12": "f12", "f24": "f24", "part_on": "part_on", "sat": "sat", "eq_base": "eq_base", "eq_band": "eq_band", "duck": "duck", "ktrk": "ktrk",
                                                    "mx_slot": "mx_slot", "mx_lfo": "mx_lfo", "comp": "comp", "width": "width", "bm": "bm"}[key]]), ch)
    b = sub_define(b, "BD_MORPH", take("morph_sw", max(g["morph_sw"], g["morph_st"]), M["morph"]), ch)
    # BD_ENG: one number each
    def eng_sub(m):
        nm = "VOICE" if m.group(1) == "FORMANT" else m.group(1)
        v = take(f"eng.{nm}", T["eng"].get(nm), int(m.group(2)))
        if v is not None and int(round(v)) != int(m.group(2)):
            ch.append((f"BD_ENG {nm}", int(m.group(2)), int(round(v))))
        return "{&ENG_" + m.group(1) + ", " + str(int(round(v if v is not None else int(m.group(2))))) + "}"
    b = re.sub(r"\{&ENG_(\w+),\s*(\d+)\}", eng_sub, b)
    ins_old = M["ins"]
    ins_new = [0] + [take(f"ins.{i}", g["ins"][i], ins_old[i]) for i in range(1, 11)]
    if max(ins_new) > 255:
        b = b.replace("static const uint8_t BD_INS[]", "static const uint16_t BD_INS[]")
    b = sub_array(b, "BD_INS", ins_new, ch)
    for name, key in (("BD_IDLE", "idle"), ("BD_BUS", "bus"), ("BD_CHO", "cho"), ("BD_SHD", "shd"), ("BD_SHM", "shm")):
        vals = [take(f"{'idle' if key == 'idle' else key}.{m}" if key != "idle" else f"idle.{m}", g[key][m], M[key][m]) for m in ("LAYER", "DUEL")]
        b = sub_array(b, name, vals, ch)
    dly = [take(f"dly.{m}.{i}", g["dly"][m][i], M["dly"][m][i]) for m in ("LAYER", "DUEL") for i in range(4)]
    rev = [take(f"rev.{m}.{i}", g["rev"][m][i], M["rev"][m][i]) for m in ("LAYER", "DUEL") for i in range(6)]
    b = sub_array(b, "BD_DLY", dly, ch)
    b = sub_array(b, "BD_REV", rev, ch)
    for name, key, mk in (("BD_SUB_PLS", "sub_SQ", "sub_SQ"), ("BD_SUB_SINE", "sub_SINE", "sub_SINE"), ("BD_SUB_TRI", "sub_TRI", "sub_TRI"), ("BD_CYCLE", "mx_cyc", "mx_cyc"),
                          ("BD_TW_CHAIN_BYP", "tw_chain_byp", "tw_chain_byp"), ("BD_TW_SVF", "tw_svf", "tw_svf"), ("BD_TW_LADDER", "tw_ladder", "tw_ladder"), ("BD_TW_COMB", "tw_comb", "tw_comb"),
                          ("BD_TW_DRIVE", "tw_drive", "tw_drive"), ("BD_TW_PAR", "tw_par", "tw_par"), ("BD_TW_MOD", "tw_mod", "tw_mod")):
        bf = sub_define(bf, name, take(key, g[key], M[mk]), ch)
    bh = sub_define(bh, "BUDGET_HO", take("ho", g["ho"], M["ho"]), ch)
    new = {"budget.c": b, "budget_feat.c": bf, "budget.h": bh}
    out = []
    for f in ("budget.c", "budget_feat.c", "budget.h"):
        out += list(difflib.unified_diff(files[f].splitlines(True), new[f].splitlines(True), fromfile=f"a/firmware/src/{f}", tofile=f"b/firmware/src/{f}", n=2))
    return "".join(out), ch, keep


# ------------------------------------------------------------------------------------------------ validation
def budget_h_define(name, fallback, src=SRC_DEFAULT):
    """a number #defined in firmware/src/budget.h (validate's defaults follow the firmware: item 137), the fallback if the file or the define is missing"""
    try:
        m = re.search(r"#define\s+" + name + r"\s+(\d+)u?\b", open(os.path.join(src, "budget.h")).read())
        return float(m.group(1)) if m else fallback
    except OSError:
        return fallback


def predict_max(cfg, T, margin, limit, off=0.0):
    """the most keys the table allows for the scene (the budget's rule: predicted % x margin <= limit; off: a measured offset to add to the prediction, % of a half)"""
    n = 0
    for k in range(1, D.eng_cap(cfg["eng"]) + 1):
        if (D.predict_avg(dict(cfg, K=k), T) + off) * margin > limit:
            break
        n = k
    return n


def measured_pct(r, cores, T):
    """the figure the table predicts for a scene: a one-core scene is avg + c1 (both parts); a fake one-core row still holds the two-core hand-over, taken off (the table's ho)"""
    if cores == 1:
        return r["tot"] - (T["g"]["ho"] / PCT if r["fake1c"] and r["vox"] > 0 else 0.0)
    return r["avg"]


def validate(rows, T, margin=1.00, limit=81.0, polymax=None, assume=(), out=print):
    """the held-out runs (CAL.H.*) against the table: the CPU figure, and the most keys the table allows against the most keys the MEASUREMENT allows (the table corrected by the run's own error:
    what the notes counter would have shown had the table been exact at that scene). With a POLY-MAX run: the same against the unit's real most keys"""
    plan = D.plan_by_name()
    errs = []
    for name in sorted(n for n in rows if n.startswith("CAL.H.")):
        t = plan.get(name)
        if not t:
            continue
        m = mean([measured_pct(r, t["cfg"]["cores"], T) for r in rows[name]])
        p = D.predict_avg(t["cfg"], T)
        errs.append((name, m, p, t["cfg"]))
    out(f"held-out runs (never in the fit): measured against predicted CPU, % of a half; most keys by the budget rule (predicted x {margin} <= {limit:.0f} %)")
    if not errs:
        out("  none in this run")
    for (n, m, p, cfg) in errs:
        nt, nm = predict_max(cfg, T, margin, limit), predict_max(cfg, T, margin, limit, m - p)
        out(f"  {n:26s} measured {m:6.2f}  predicted {p:6.2f}  error {p - m:+6.2f} ({(p - m) / m * 100 if m else 0:+5.1f} %)   keys: table {nt}, measured {nm}")
    stats = {}
    if errs:
        ab = [abs(p - m) for _, m, p, _c in errs]
        rel = [(p - m) / m for _, m, p, _c in errs if m]
        stats = dict(n=len(errs), mean_abs_pts=mean(ab), max_abs_pts=max(ab), mean_rel=mean(rel), worst_rel=max(rel, key=abs), under=sum(1 for _, m, p, _c in errs if p < m),
                     keys_off=[predict_max(c, T, margin, limit) - predict_max(c, T, margin, limit, m - p) for _, m, p, c in errs])
        out(f"  {stats['n']} runs: mean |error| {stats['mean_abs_pts']:.2f} points, worst {stats['max_abs_pts']:.2f} points, mean {stats['mean_rel'] * 100:+.1f} %, worst {stats['worst_rel'] * 100:+.1f} %, "
            f"{stats['under']} predicted BELOW the measurement (a model that is too cheap is the one that crackles)")
        ko = stats["keys_off"]
        out(f"  most keys, table against measurement: exact in {sum(1 for x in ko if x == 0)} of {len(ko)}, too many by {sum(1 for x in ko if x > 0)} (worst +{max(ko)}), too few by {sum(1 for x in ko if x < 0)} (worst {min(ko)})")
    if polymax:
        prow, _ = load_run(polymax, all_rows=True)
        groups = {}
        for nm, rs in prow.items():
            m = re.match(r"POLY_(\w+?)_(MOFF|M12|M12TW)_(1C|2C)_N(\d+)$", nm)
            if m:
                groups.setdefault(m.group(1, 2, 3), []).append((int(m.group(4)), rs))
        out("")
        out(f"POLY-MAX: the most keys the unit played (no missed half, no silent part 2, peak <= 95 %, as tools/polymax_report.py) against the table's budget rule (predicted % x {margin} <= {limit:.0f} %)")
        ass = set(assume)
        diffs = []
        for (e, v, c), lst in sorted(groups.items()):
            lst.sort(key=lambda x: x[0])
            best = 0
            for k, rs in lst:
                if not all(r["miss"] == 0 and r["uac"] == 0 and r["tcsilent"] == 0 and r["peak"] <= 95.0 for r in rs):
                    break
                best = k
            cfg = D.base_cfg(eng=e, cores=1 if c == "1C" else 2, mode="LAYER", filt=0 if v == "MOFF" else 12, twin="LP" if v == "M12TW" else None, sends="sends" in ass,
                             morph="ST" if "morph" in ass else None, cho=100 if "cho" in ass else 0)
            pm = predict_max(cfg, T, margin, limit)
            errs2 = [D.predict_avg(dict(cfg, K=k), T) - mean([measured_pct(r, cfg["cores"], T) for r in rs]) for k, rs in lst if all(r["miss"] == 0 for r in rs)]
            diffs.append(pm - best)
            out(f"  {e:7s} {v:6s} {c}: unit {best:2d} keys, table {pm:2d} keys ({pm - best:+d})   CPU predicted less measured: mean {mean(errs2) if errs2 else 0:+.2f} points over {len(errs2)} counts")
        if diffs:
            out(f"  {len(diffs)} settings: table = unit in {sum(1 for d in diffs if d == 0)}, table gives more in {sum(1 for d in diffs if d > 0)}, fewer in {sum(1 for d in diffs if d < 0)}. "
                f"(The unit's most keys is where a half is missed, the table's is the {limit:.0f} % budget with the margin: the table should give FEWER by design; the factory sounds also run what --assume names.)")
    return stats


# ------------------------------------------------------------------------------------------------ the synthetic run (selftest)
def perturb(T0, rnd):
    def p(v):
        return v * rnd.uniform(0.8, 1.25)
    T = json.loads(json.dumps(T0))

    def walk(o):
        if isinstance(o, dict):
            for k in o:
                o[k] = walk(o[k]) if isinstance(o[k], (dict, list)) else p(o[k])
            return o
        return [walk(x) if isinstance(x, (dict, list)) else p(x) for x in o]
    walk(T["eng"]), walk(T["eng2"])
    for k, v in T["g"].items():
        if k in ("morph_fix_st", "morph_fix_sw", "mx_fix", "uni_extra"):
            continue
        T["g"][k] = walk(v) if isinstance(v, (dict, list)) else p(v)
    T["g"]["ins"][0] = 0.0
    T["g"]["idle"]["LAYER"] = T["g"]["idle"]["LAYER"] * 1.0
    T["g"]["morph_fix_st"], T["g"]["morph_fix_sw"], T["g"]["mx_fix"], T["g"]["uni_extra"] = 6.0, 9.0, 3.0, 4.0     # a fixed morph and matrix cost, a UNISON extra: to be found again
    return T


def part_path_instr(cfg, T):
    """what core 1 does in a two-core scene, instructions: the two-core figure less the globals (idle mix, buses, master, key sense) and the hand-over: one part's path"""
    if not D.vox_per_part(cfg):
        return 0.0
    c2 = dict(cfg, cores=2, sends=0, comp=0, width=100, bm=0, keysense="OFF")
    return D.predict_avg(c2, T) * PCT - T["g"]["idle"]["DUEL" if cfg["mode"] == "DUEL" else "LAYER"] - T["g"]["ho"]


def synth_csv(path, T, seed=1, noise=0.07, fake1c=False):
    """fake1c: the one-core arm does not become one-core (the real smoke run's picture): the '.1C.' tests are played on two cores, tc 2, tcown 0, core 1 busy with part 2. The two cores
    are the same speed then (T's eng2 = eng), as on the unit"""
    rnd = random.Random(seed)
    plan = D.build_plan(False)
    cols = ["test", "rep", "reps", "text", "verdict", "why", "status", "dur_s", "avg", "peak", "miss", "vox", "tc", "tcsilent", "tcvlate", "tcown", "c1", "uac_under"]
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        for t in plan:
            cfg = t["cfg"]
            cap = D.eng_cap(cfg["eng"])
            nv = D.vox_per_part(cfg)
            for rep in range(1, t["repeat"] + 1):
                two = cfg["cores"] == 2 or fake1c
                avg = D.predict_avg(dict(cfg, cores=2) if two else cfg, T) + rnd.gauss(0, noise)
                c1 = 0.0
                if fake1c:
                    c1 = part_path_instr(cfg, T) / D.PCT + (rnd.gauss(0, noise) if nv else 0.0)
                elif cfg["cores"] == 2:
                    c1 = (T["g"]["part_on"] + nv * T["eng"][cfg["eng"]] * 1.04) / D.PCT
                w.writerow(dict(test=t["name"], rep=rep, reps=t["repeat"], text=t["text"], verdict="PASS", status="DONE", dur_s=2.7, avg=f"{max(avg, 0):.1f}", peak=f"{avg * 1.12:.1f}", miss=0,
                                vox=f"{2 * nv:.1f}", tc="2" if two else "1", tcsilent=0, tcvlate=0, tcown=0, c1=f"{max(c1, 0):.1f}", uac_under=0))
    return path


def synth_poly(path, T, seed=2, noise=0.1):
    """a POLY-MAX run (tools/gen_polymax_suite.py) as the truth table would give it: 10 engines x 3 filter settings x 2 cores x 1..8 keys; a half is missed when the peak passes 100 %"""
    rnd = random.Random(seed)
    cols = ["test", "rep", "status", "avg", "peak", "miss", "vox", "tc", "tcsilent", "tcvlate", "c1", "uac_under"]
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        for e in D.ENG_ORDER:
            for v in ("MOFF", "M12", "M12TW"):
                for c in ("2C", "1C"):
                    for k in range(1, 9):
                        cfg = D.base_cfg(eng=e, cores=1 if c == "1C" else 2, K=k, filt=0 if v == "MOFF" else 12, twin="LP" if v == "M12TW" else None)
                        avg = D.predict_avg(cfg, T) + rnd.gauss(0, noise)
                        pk = avg * 1.12
                        w.writerow(dict(test=f"POLY_{e}_{v}_{c}_N{k}", rep=1, status="DONE", avg=f"{min(avg, 99):.1f}", peak=f"{min(pk, 100):.1f}", miss=int(pk > 100), vox=2 * D.vox_per_part(cfg),
                                        tc="2" if c == "2C" else "1", tcsilent=0, tcvlate=0, c1=0, uac_under=0))
    return path


def selftest_fake1c(model, tmp):
    """item 110: a run whose '.1C.' tests are really two-core (the real smoke run's picture). The fit must say so, read avg + c1 as the one-core cost, give the per-voice and feature rows right,
    mark the hand-over and the part's work as ESTIMATES (kept out of the patch), and use the fake rows as extra two-core data. And a clean run must NOT be called fake."""
    ok = True
    T0 = perturb(table_from_model(model), random.Random(11))
    T0["eng2"] = dict(T0["eng"])                                       # the same speed on both cores, as the unit's smoke run shows
    csvp = synth_csv(os.path.join(tmp, "fake.csv"), T0, seed=3, fake1c=True)
    rows, bad = load_run(csvp)
    F, T1 = fit_table(rows, model)
    nf = sum(1 for v in rows.values() for r in v if r["fake1c"])
    n1 = sum(1 for v in rows.values() for r in v if r["is1c"])
    print(f"selftest fake one-core arm: {n1} '.1C.' runs, {nf} found not one-core, F.fake {F.fake}")
    good = F.fake and nf >= 0.95 * n1 and n1 > 0           # (a scene with no voices has no core 1 work to see)
    print(f"  {'ok  ' if good else 'FAIL'} the one-core arm is found out")
    ok = ok and good
    cl = {"a": 0}

    def chk(label, got, want, tol_abs, tol_rel=0.12):
        g = abs(got - want) <= max(tol_abs, tol_rel * abs(want))
        print(f"  {'ok  ' if g else 'FAIL'} {label:34s} fitted {got:8.1f}  truth {want:8.1f}")
        return g
    for e in D.ENG_ORDER:
        ok &= chk(f"engine {e} (avg + c1)", T1["eng"][e], T0["eng"][e], 6.0)
        ok &= chk(f"engine {e} (two cores)", T1["eng2"][e], T0["eng2"][e], 8.0)
    for k in ("f12", "f24", "tw_svf", "sub_SINE", "mx_slot", "sat", "duck", "comp"):
        ok &= chk(k, T1["g"][k], T0["g"][k], 8.0, 0.2)
    ok &= chk("idle LAYER", T1["g"]["idle"]["LAYER"], T0["g"]["idle"]["LAYER"], 6.0)
    ok &= chk("part_on (estimate)", T1["g"]["part_on"], T0["g"]["part_on"], 10.0, 0.2)
    ok &= chk("ho (estimate)", T1["g"]["ho"], T0["g"]["ho"], 12.0, 0.3)
    tags = {r["key"]: r["tag"] for r in F.rows_out}
    g2 = tags.get("ho") == "ESTIMATE" and tags.get("part_on") == "ESTIMATE"
    print(f"  {'ok  ' if g2 else 'FAIL'} ho and part_on are tagged ESTIMATE")
    ok &= g2
    txt = report(F, T1, model, bad, sum(len(v) for v in rows.values()), csvp, rows)
    g3 = "THE ONE-CORE ARM IS NOT ONE-CORE" in txt
    print(f"  {'ok  ' if g3 else 'FAIL'} the report says so")
    ok &= g3
    st = validate(rows, T1, out=lambda x: None)
    g4 = bool(st) and st["mean_abs_pts"] <= 1.2
    print(f"  {'ok  ' if g4 else 'FAIL'} held-out runs predicted from avg + c1 (mean error {st['mean_abs_pts'] if st else float('nan'):.2f} points)")
    ok &= g4
    text, changes, keep = make_patch(F, T1, SRC_DEFAULT)
    g5 = not any(c[0] in ("BUDGET_HO", "BD_PART_ON") for c in changes)
    print(f"  {'ok  ' if g5 else 'FAIL'} the patch leaves BUDGET_HO and BD_PART_ON alone ({len(changes)} rows changed)")
    ok &= g5
    # a run with only a few fake rows is not called fake as a whole; the mixed message names them
    rows3, _ = load_run(synth_csv(os.path.join(tmp, "clean.csv"), T0, seed=4))
    F3, _T3 = fit_table(rows3, model)
    g6 = (not F3.fake) and F3.nfake == 0 and F3.n1c > 0
    print(f"  {'ok  ' if g6 else 'FAIL'} a really one-core run is not called fake ({F3.n1c} '.1C.' runs, {F3.nfake} fake)")
    ok &= g6
    return bool(ok)


def selftest(src):
    ok = True
    model = read_model(src)
    T0 = perturb(table_from_model(model), random.Random(7))
    tmp = tempfile.mkdtemp(prefix="costfit_")
    csvp = synth_csv(os.path.join(tmp, "results.csv"), T0)
    rows, bad = load_run(csvp)
    F, T1 = fit_table(rows, model)
    print(f"selftest: synthetic run {len(rows)} tests, {len(bad)} unusable, one run's spread {F.sigma:.2f} %")

    def chk(label, got, want, tol_abs, tol_rel=0.12):
        nonlocal ok
        good = abs(got - want) <= max(tol_abs, tol_rel * abs(want))
        if not good:
            ok = False
        print(f"  {'ok  ' if good else 'FAIL'} {label:34s} fitted {got:8.1f}  truth {want:8.1f}")
    for e in D.ENG_ORDER:
        chk(f"engine {e}", T1["eng"][e], T0["eng"][e], 6.0)
        chk(f"engine {e} (two cores)", T1["eng2"][e], T0["eng2"][e], 8.0)
    for k in ("f12", "f24", "ktrk", "tw_svf", "tw_ladder", "tw_comb", "tw_drive", "tw_par", "tw_mod", "tw_chain_byp", "sub_SINE", "sub_TRI", "sub_SQ", "mx_slot", "mx_lfo", "mx_cyc", "uni_extra",
              "morph_st", "morph_sw", "part_on", "ho", "sat", "duck", "eq_band", "comp", "width", "bm"):
        chk(k, T1["g"][k], T0["g"][k], 8.0, 0.2 if k not in ("uni_extra", "tw_mod", "tw_par", "ktrk", "mx_cyc") else 0.9)
    for i in (1, 4, 6, 10):
        chk(f"insert {i}", T1["g"]["ins"][i], T0["g"]["ins"][i], 5.0)
    for m in ("LAYER", "DUEL"):
        chk(f"idle {m}", T1["g"]["idle"][m], T0["g"]["idle"][m], 6.0)
        # (the scene "SENDS ON, nothing fed" is charged by budget.c as delay DIGITAL and reverb HALL fed: the synthetic truth is that structure, so the row the fit finds for it is that sum)
        chk(f"bus {m}", T1["g"]["bus"][m], T0["g"]["dly"][m][0] + T0["g"]["rev"][m][2] - T0["g"]["bus"][m], 6.0)
        chk(f"chorus {m}", T1["g"]["cho"][m], T0["g"]["cho"][m], 6.0)
        chk(f"delay[2] {m}", T1["g"]["dly"][m][2], T0["g"]["dly"][m][2], 6.0)
        chk(f"reverb[3] {m}", T1["g"]["rev"][m][3], T0["g"]["rev"][m][3], 6.0)
        chk(f"shim down {m}", T1["g"]["shd"][m], T0["g"]["shd"][m], 6.0)
    # the held-out runs: the fitted table must predict them as well as the truth does
    print("validate (the fitted table against the held-out synthetic runs):")
    st = validate(rows, T1)
    if not st or st["mean_abs_pts"] > 0.9:
        ok = False
        print("  FAIL: the held-out error is too large")
    pm = synth_poly(os.path.join(tmp, "polymax.csv"), T0)
    print("validate against a synthetic POLY-MAX run:")
    validate(rows, T1, polymax=pm, out=lambda x: print(x) if "settings:" in x else None)
    # the patch: made on the sources, tried on a copy with `patch --dry-run`
    text, changes, keep = make_patch(F, T1, src)
    pp = os.path.join(tmp, "budget_rows.patch")
    open(pp, "w").write(text)
    cp = os.path.join(tmp, "src")
    os.makedirs(os.path.join(cp, "firmware", "src"))
    for f in ("budget.c", "budget_feat.c", "budget.h"):
        shutil.copy(os.path.join(src, f), os.path.join(cp, "firmware", "src", f))
    r = subprocess.run(["patch", "-p1", "--dry-run", "-i", pp], cwd=cp, capture_output=True, text=True)
    print(f"patch --dry-run: rc {r.returncode}, {len(changes)} rows changed, {len(keep)} kept ({r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()})")
    if r.returncode != 0 or not changes:
        ok = False
    # a truncated run (a missed half in the middle, one test missing): the fit must still go through and tag what it cannot say
    rows2 = {k: v for k, v in rows.items() if not k.startswith("CAL.F.ANALOG.1C.TW_COMB")}
    F2, T2 = fit_table(rows2, model)
    print(f"truncated run (TW_COMB missing): tw_comb stays the old row {T2['g']['tw_comb']:.0f} = {model['tw_comb']}: {'ok' if T2['g']['tw_comb'] == model['tw_comb'] else 'FAIL'}")
    ok = ok and T2["g"]["tw_comb"] == model["tw_comb"]
    ok = ok and selftest_fake1c(model, tmp)
    shutil.rmtree(tmp, ignore_errors=True)
    print("selftest:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


# ------------------------------------------------------------------------------------------------ main
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fit")
    f.add_argument("csv")
    f.add_argument("--out")
    f.add_argument("--src", default=SRC_DEFAULT)
    f.add_argument("--agg", choices=("max", "median"), default="max", help="rows shared by every engine (MULTI, MORPH): the dearest engine's (max, the safe default) or the median")
    v = sub.add_parser("validate")
    v.add_argument("csv")
    v.add_argument("--table", required=True)
    v.add_argument("--polymax")
    v.add_argument("--assume", default="", help="what the POLY-MAX factory sounds also run: sends,morph,cho")
    v.add_argument("--limit", type=float, default=budget_h_define("BUDGET_LIMIT", 81.0), help="default: BUDGET_LIMIT of firmware/src/budget.h (item 137: 81)")
    v.add_argument("--margin", type=float, default=budget_h_define("BUDGET_MARGIN", 100.0) / 100.0, help="default: BUDGET_MARGIN / 100 of firmware/src/budget.h (item 137: 1.00)")
    s = sub.add_parser("selftest")
    s.add_argument("--src", default=SRC_DEFAULT)
    a = ap.parse_args(argv)
    if a.cmd == "selftest":
        return selftest(a.src)
    rows, bad = load_run(a.csv)
    if a.cmd == "validate":
        T = json.load(open(a.table))
        validate(rows, T, a.margin, a.limit, a.polymax, [x for x in a.assume.split(",") if x])
        return 0
    model = read_model(a.src)
    F, T = fit_table(rows, model, a.agg)
    out = a.out or os.path.join(os.path.dirname(os.path.abspath(a.csv)), "cost_fit")
    os.makedirs(out, exist_ok=True)
    rep = report(F, T, model, bad, sum(len(v) for v in rows.values()), a.csv, rows)
    open(os.path.join(out, "report.txt"), "w").write(rep)
    T2 = dict(T)
    T2["meta"] = {r["key"]: dict(val=r["val"], se=r["se"], n=r["n"], tag=r["tag"], old=r["old"]) for r in F.rows_out}
    json.dump(T2, open(os.path.join(out, "cost_table.json"), "w"), indent=1)
    with open(os.path.join(out, "cost_rows.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["key", "label", "measured_instr", "spread", "n", "tag", "old_instr", "note"])
        for r in F.rows_out:
            w.writerow([r["key"], r["label"], f"{r['val']:.2f}", "" if r["se"] is None else f"{r['se']:.2f}", r["n"], r["tag"], r["old"], r["note"]])
    text, changes, keep = make_patch(F, T, a.src)
    open(os.path.join(out, "budget_rows.patch"), "w").write(text)
    print(rep)
    print(f"cost_fit: {out}/report.txt  cost_table.json  cost_rows.csv  budget_rows.patch ({len(changes)} rows would change, {len(keep)} kept because the run did not support them)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
