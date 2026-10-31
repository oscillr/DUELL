#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 OSCILL/R (DUELL). Part of DUELL, a modified version of Felucca.
"""DUELL 1.1 item 105: what the cost calibration suite is made of. Shared by tools/gen_cost_suite.py (writes tools/bench_suites/COST-CAL.txt) and tools/cost_fit.py
(reads the run's results.csv). One place holds: the engines, the feature list (what is switched on ALONE from the baseline), every test as a config, the console words a config
becomes, and the model that turns a cost table back into a predicted CPU figure (used by the fit's validation).

A config (dict) is one scene: engine, cores, keys, mode, and the settings switched on. The baseline is: the engine's dearest factory sound cleaned (multimode filter bypassed, no TWIN,
SUB, matrix, MORPH, INSERT, SAT, EQ, DUCK, arp, sends), POLY, LAYER, MODULES SENDS off, no master stage, KEY SENSE off, the NOTES budget held open, both parts the same sound.
Units of the CPU figure: the unit's `avg` column = core 0's audio interrupt time in % of one half buffer (TIMER5's nested time taken out: the CPU bar). The budget table's unit is
host instructions a sample, 3281.25 = 100 % (budget.h), so instr = % x 32.8125.
"""
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "firmware", "src")
PCT = 32.8125                      # host instructions a sample per 1 % of a half (budget.h: 3281.25 = 100 %)

# ------------------------------------------------------------------------------------------------ ids from the sources (so no number is typed twice)


def _strip(t):
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    return re.sub(r"//[^\n]*", " ", t)


def _p_ids():
    t = open(os.path.join(SRC, "core.h")).read()
    i = t.index("/* per-track parameters */")
    j = re.compile(r"\n\s*P_COUNT\s*\n\};").search(t, i).end()
    body = _strip(t[t.index("{", i - 40):j])
    names = [x.strip() for x in body.replace("{", "").replace("}", "").replace(";", "").split(",") if x.strip()]
    return {nm: k for k, nm in enumerate(names)}


def _g_ids():
    t = open(os.path.join(SRC, "core.h")).read()
    i = t.index("/* global parameters */")
    j = re.compile(r"\n\s*G_COUNT\s*\n\};").search(t, i).end()
    body = _strip(t[t.index("{", i - 40):j])
    names = [x.strip() for x in body.replace("{", "").replace("}", "").replace(";", "").split(",") if x.strip()]
    return {nm: k for k, nm in enumerate(names)}


def _x_ids():
    t = _strip(open(os.path.join(SRC, "ext.h")).read())
    i = t.index("X_ROUTE,")
    j = t.index("X_STRUM", i)
    names = [x.strip() for x in t[i:j + 7].split(",") if x.strip()]
    return {nm: 128 + k for k, nm in enumerate(names)}


def _enum_after(path, first):
    t = _strip(open(os.path.join(SRC, path)).read())
    i = t.index(first)
    j = t.index("}", i)
    names = [x.strip() for x in t[i:j].split(",") if x.strip()]
    return {nm: k for k, nm in enumerate(names)}


PID = _p_ids()
GID = _g_ids()
XID = _x_ids()
MS = _enum_after("mod.c", "MS_OFF,")
MD = _enum_after("mod.c", "MD_OFF,")
AM = _enum_after("seq.c", "AM_OFF,")
# the ids the older suites type by hand (tools/gen_polymax_suite.py): a wrong parse stops here, not on the unit
assert PID["P_FREEA"] == 45 and PID["P_FREEC"] == 47 and PID["P_FREED"] == 48 and PID["P_TWTYPE"] == 43 and PID["P_CUT2"] == 106 and PID["P_KTRK"] == 27, "core.h P_ ids moved"
assert PID["P_VOICE"] == 37 and PID["P_AMODE"] == 17 and PID["P_LD_FLT"] == 14 and PID["P_E2FLT"] == 114 and PID["P_L2FLT"] == 117 and PID["P_ED_FLT"] == 5, "core.h P_ ids moved"
assert GID["G_COUNT"] == 27 and XID["X_SUBLVL"] == 134 and XID["X_ROUTE"] == 128 and XID["X_STRUM"] == 155, "ids moved"
assert MS["MS_CYCLE"] == 15 and MD["MD_PAN"] == 5 and AM["AM_CHORD"] == 14, "mod.c / seq.c enums moved"
G_DMODE, G_RTYPE, G_SHPITCH, G_SHMOD = GID["G_INFO"], GID["G_RTYPE"], GID["G_FREE26"], GID["G_NAME"]    # core.h: G_DMODE = G_INFO, G_SHPITCH = G_FREE26, G_SHMOD = G_NAME
assert (G_DMODE, G_RTYPE, G_SHPITCH, G_SHMOD) == (15, 24, 26, 17)
TT = {"LP": 1, "BP": 2, "HP": 3, "NOTCH": 4, "LAD": 5, "COMB": 6}      # params.c: TT_OFF, TT_LP, TT_BP, TT_HP, TT_NOTCH, TT_LADDER, TT_COMB

# ------------------------------------------------------------------------------------------------ engines
# name: (engine index in ENGINES[], the engine's own voice limit per part (core.h engine_t.poly; 0: the pool), the dearest sound's number (bench_model.h BENCH_DEAR))
ENGINES = {
    "ANALOG": (0, 0), "PHASE": (2, 0), "LOFI": (3, 0), "VOICE": (5, 4), "TRIO": (6, 0), "WHEEL": (7, 0), "PHYS": (9, 3), "NOISE": (11, 0), "FM6": (12, 6), "WAVE": (14, 0),
}
ENG_ORDER = ["ANALOG", "PHASE", "WAVE", "WHEEL", "VOICE", "FM6", "PHYS", "TRIO", "NOISE", "LOFI"]     # (gen_polymax_suite.py's order)
NVOICE = 8                                                   # core.h NVOICE: the pool a part has today (budget OPEN: every voice the engine and the pool give)
SOUND_COUNTS_FALLBACK = {"ANALOG": 20, "PHASE": 11, "LOFI": 9, "VOICE": 8, "TRIO": 13, "WHEEL": 7, "PHYS": 12, "NOISE": 11, "FM6": 14, "WAVE": 23}


def sound_counts():
    """the factory sounds of each engine: sounds.c DS_ENG_AT from build/gen/duell_sounds.h; else the numbers seen in the 1.1.0-test5 tree"""
    try:
        t = open(os.path.join(HERE, "..", "build", "gen", "duell_sounds.h")).read()
        m = re.search(r"DS_ENG_AT\[[^\]]*\]\s*=\s*\{([^}]*)\}", t)
        at = [int(x) for x in m.group(1).split(",")]
        return {e: at[ENGINES[e][0] + 1] - at[ENGINES[e][0]] for e in ENGINES}
    except (OSError, AttributeError, ValueError, IndexError):
        return dict(SOUND_COUNTS_FALLBACK)


def eng_cap(eng):
    return ENGINES[eng][1] or NVOICE


def counts_for(eng, quick=False, cores=1):
    """the key counts of the baseline: 1, 2, 4, 8, 12 (the ticket), those the unit can carry: a count whose predicted load (the 1.1 table, HINT) is over MAXPCT would miss halves and its
    figure would be clipped, so it is left out (WHEEL, FM6, PHYS and WAVE on one core stop earlier). An engine with its own limit stops at it. quick: 1, 4, 8"""
    cap = ENGINES[eng][1]
    ks = [1, 4, 8] if quick else [1, 2, 4, 8, 12]
    if cap:
        ks = sorted({k for k in ks if k < cap} | {cap})
    ok = [k for k in ks if predict_avg(base_cfg(eng=eng, cores=cores, K=k), HINT) <= MAXPCT]
    return ok if len(ok) >= 2 else ks[:2]


# ------------------------------------------------------------------------------------------------ configs
def base_cfg(**kw):
    c = dict(eng="ANALOG", sound="DEAR", cores=1, K=2, mode="LAYER", sends=False, filt=0, fmode=0, ktrk=0, twin=None, twdrv=0, twpar=0, twmod=0, sub=None, uni=0, morph=None,
             mx=None, ins=0, sat=0, eq=0, duck=0, cho=0, dly=0, rev=0, dmode=0, rtype=2, shp=0, shm=0, comp=0, width=100, bm=0, keysense="OFF", mute2=0, byp=[])
    c.update(kw)
    return c


INS_NAMES = ["", "SOFT CLIP", "HARD CLIP", "FOLD", "FUZZ", "BIT CRUSHER", "PHASER", "FLANGER", "CHORUS", "TAPE", "VINYL"]
DLY_NAMES = ["DIGITAL", "PING-PONG", "RANDOM", "OCT UP"]
REV_NAMES = ["ROOM", "SPRING", "HALL", "SHIM", "RESO", "PLATE"]
SUB_WAVES = {"SINE": 0, "TRI": 1, "SQ": 2, "PULSE": 3}
SWEEP_S = 2.4                                                # the test's length with the keys down, s
WINDOW = (0.9, SWEEP_S)                                      # the CPU window, s
NOTES = ["C3", "E3", "G3", "B3", "D4", "F4", "A4", "C5", "E5", "G5", "B5", "D6"]


def config_lines(c):
    """the suite words of a config, in the order the unit needs: mode, sounds, settings, MORPH last (its Original is the sound as set)"""
    L = [f"mode {c['mode']}", f"sound 1 {c['eng']} {c['sound']}", f"sound 2 {c['eng']} {c['sound']}",
         "mods ALL ON", "mods SENDS ON" if c["sends"] else "mods SENDS OFF",
         f"master COMP {c['comp']}", f"master WIDTH {c['width']}", f"master BM {c['bm']}", f"keysense {c['keysense']}", "budget OPEN"]
    if c["cores"] == 1:
        L.append("tune core1 off")                       # item 112 (BENCHV3 and later): core 1 treated as down, core 0 plays part 2, the one-core budget. (`tune slack 400000` did this before DUELL 1.1 item 90 and no longer does, HANDOFF_V2 item 110)
    parts = [1, 2]
    for p in parts:
        s = []

        def st(name, v):
            s.append(f"set {p} {PID[name] if name in PID else XID[name]} {v}")
        if c["filt"]:
            st("P_FREEA", 50), st("P_FREEC", c["fmode"]), st("P_FREED", 2 if c["filt"] == 24 else 1)
            if c["ktrk"]:
                st("P_KTRK", 40)
        if c["twin"]:
            st("P_TWTYPE", TT[c["twin"]]), st("P_CUT2", 0 if c["twin"] == "COMB" else 10), st("P_RES2", 30)
            if c["twdrv"]:
                st("P_DRV2", 64)
            if c["twpar"]:
                st("X_ROUTE", 1)
            if c["twmod"]:
                st("X_ENV2", 20)
        if c["sub"]:
            st("X_SUBLVL", 90), st("X_SUBWAVE", SUB_WAVES[c["sub"]])
        if c["uni"]:
            st("P_VOICE", 3)
        if c["mx"]:
            st("P_M1SRC", {"KEY": MS["MS_KEY"], "LFO": MS["MS_LFO"], "CYC": MS["MS_CYCLE"]}[c["mx"]]), st("P_M1DST", MD["MD_PAN"]), st("P_M1AMT", 40)
        if c["ins"]:
            st("P_ITYPE", c["ins"]), st("P_IA", 64), st("P_IB", 64), st("P_IC", 64), st("P_IMIX", 127)
        if c["sat"]:
            st("P_DIST", 64)
        if c["eq"] >= 1:
            st("P_EQLG", 5), st("P_EQLF", -4)
        if c["eq"] >= 2:
            st("P_EQMG", -4), st("P_EQMF", 5), st("P_EQMQ", 3)
        if c["eq"] >= 3:
            st("P_EQHG", 6), st("P_EQHF", 2)
        if c["duck"]:
            st("P_DKDEP", 90), st("P_DKATK", 20), st("P_DKRLS", 70), st("P_DKSRC", 0)
        if c["cho"]:
            st("P_CHOR", c["cho"])
        if c["dly"]:
            st("P_DLY", c["dly"])
        if c["rev"]:
            st("P_REV", c["rev"])
        if p == 2 and c["mute2"]:
            st("P_MUTE", 1)
        L += s
    L += [f"set g {G_DMODE} {c['dmode']}", f"set g {G_RTYPE} {c['rtype']}"] if (c["dmode"] or c["rtype"] != 2) else []
    if c["shp"]:
        L.append(f"set g {G_SHPITCH} {c['shp']}")
    if c["shm"]:
        L.append(f"set g {G_SHMOD} {c['shm']}")
    for (tgt, name) in c["byp"]:
        if tgt == "g":
            L.append(f"bypass g {name} ON")
        else:
            L += [f"bypass 1 {name} ON", f"bypass 2 {name} ON"]
    if c["morph"]:
        for p in parts:
            L += [f"morph {p} ROLL 17 50", f"morph {p} FADER 64"]
    return L


def event_lines(c):
    """the notes: LAYER plays part 1's channel on both parts, STEREO and DUEL each part its own channel; the same chord on each. K = 0: none"""
    K = c["K"]
    if not K:
        return []
    chans = [1] if c["mode"] == "LAYER" else [1, 2]
    L = ["at 0.0  chord " + " ".join(NOTES[:K]) + f" vel 100 ch {ch}" for ch in chans]
    if c["morph"] == "SW":
        L += [f"at 0.0  morph 0 127 over {SWEEP_S} ch {ch}" for ch in [1, 2]]
    L.append(f"at {SWEEP_S}  release")
    return L


# ------------------------------------------------------------------------------------------------ the features (each switched on ALONE from the baseline)
# kind V: costs a voice (measured with 1 and 4 keys: a slope and a fixed part), P: a part's fixed stage (2 and 6 keys: should not change), G: the global side (2 keys).
# ref: the feature it is measured against (default: the baseline). row: the budget.c / budget_feat.c row it feeds.
FEATURES = []


def feat(key, label, kind, patch, ref=None, engines=("ANALOG",), ks=None, row="", modes=("LAYER",), quick=False, **extra):
    FEATURES.append(dict(key=key, label=label, kind=kind, patch=patch, ref=ref, engines=engines, ks=ks, row=row, modes=modes, quick=quick, **extra))


feat("F12", "MULTI 12 dB", "V", dict(filt=12), row="BD_F12", engines="ALL", quick=True)
feat("F24", "MULTI 24 dB", "V", dict(filt=24), row="BD_F24", engines="ALL", quick=True)
feat("FBP", "MULTI 12 dB, BP mode (type independent?)", "V", dict(filt=12, fmode=64), ref="F12", row="(check)")
feat("KTRK", "KEY TRK on the running filter", "V", dict(filt=12, ktrk=1), ref="F12", row="BD_KTRK")
feat("MORPH_ST", "MORPH present, fader still", "V", dict(morph="ST"), row="BD_MORPH", engines="ALL", quick=True)
feat("MORPH_SW", "MORPH present, fader moving", "V", dict(morph="SW"), row="BD_MORPH")
feat("TW_LPB", "TWIN LP, MULTI bypassed", "V", dict(twin="LP"), row="BD_TW_CHAIN_BYP + BD_TW_SVF")
feat("TW_LP", "TWIN LP after MULTI 12", "V", dict(filt=12, twin="LP"), ref="F12", row="BD_TW_SVF", engines="ALL", quick=True)
feat("TW_BP", "TWIN BP after MULTI 12", "V", dict(filt=12, twin="BP"), ref="F12", row="BD_TW_SVF (same group?)")
feat("TW_LAD", "TWIN LADDER after MULTI 12", "V", dict(filt=12, twin="LAD"), ref="F12", row="BD_TW_LADDER")
feat("TW_COMB", "TWIN COMB after MULTI 12", "V", dict(filt=12, twin="COMB"), ref="F12", row="BD_TW_COMB")
feat("TW_DRV", "TWIN LP + DRIVE", "V", dict(filt=12, twin="LP", twdrv=1), ref="TW_LP", row="BD_TW_DRIVE")
feat("TW_PAR", "TWIN LP + ROUTE PARALLEL", "V", dict(filt=12, twin="LP", twpar=1), ref="TW_LP", row="BD_TW_PAR")
feat("TW_MOD", "TWIN LP + ENV 2 to CUTOFF 2", "V", dict(filt=12, twin="LP", twmod=1), ref="TW_LP", row="BD_TW_MOD")
for w in ("SINE", "TRI", "SQ", "PULSE"):
    feat("SUB_" + w, "SUB " + w, "V", dict(sub=w), row={"SINE": "BD_SUB_SINE", "TRI": "BD_SUB_TRI", "SQ": "BD_SUB_PLS", "PULSE": "BD_SUB_PLS"}[w], engines="ALL" if w == "SQ" else ("ANALOG",),
         quick=(w == "SQ"))
feat("UNI", "UNISON (all the part's voices a key)", "V", dict(uni=1), ks=(1, 2), row="(check: cost per voice as the baseline's)")
feat("MX_KEY", "one matrix slot KEY to PAN", "V", dict(mx="KEY"), row="BD_MX_SLOT")
feat("MX_LFO", "one matrix slot LFO 1 to PAN", "V", dict(mx="LFO"), ref="MX_KEY", row="BD_LFO")
feat("MX_CYC", "one matrix slot CYCLE to PAN", "V", dict(mx="CYC"), ref="MX_KEY", row="BD_CYCLE")
for i in range(1, 11):
    feat(f"INS{i}", "INSERT " + INS_NAMES[i], "P", dict(ins=i), row=f"BD_INS[{i}]", quick=(i in (1, 6)))
feat("SAT", "SAT (DIST 64)", "P", dict(sat=1), row="BD_SAT", quick=True)
for n in (1, 2, 3):
    feat(f"EQ{n}", f"EQ {n} band(s)", "P", dict(eq=n), row="BD_EQ_BASE + n x BD_EQ_BAND", quick=(n == 3))
feat("DUCK", "DUCK (PUMP)", "P", dict(duck=1), row="BD_DUCK", quick=True)
feat("BYP_SAT", "SAT with its BYPASS on", "P", dict(sat=1, byp=[("p", "SAT")]), ref="SAT", row="(does BYPASS save the cost?)")
feat("BYP_INS", "INSERT FUZZ with its BYPASS on", "P", dict(ins=4, byp=[("p", "INS")]), ref="INS4", row="(does BYPASS save the cost?)")
feat("BYP_EQ", "EQ 3 with its BYPASS on", "P", dict(eq=3, byp=[("p", "EQ")]), ref="EQ3", row="(does BYPASS save the cost?)")
feat("BYP_MULTI", "MULTI 24 dB with its BYPASS on", "V", dict(filt=24, byp=[("p", "MULTI")]), ref="F24", row="(does BYPASS save the cost?)")
feat("BYP_TWIN", "TWIN LP with its BYPASS on", "V", dict(filt=12, twin="LP", byp=[("p", "TWIN")]), ref="TW_LP", row="(does BYPASS save the cost?)")
# the global side (K = 2, ref: the same scene with SENDS OFF unless named)
feat("G_IDLE", "nothing playing, SENDS OFF", "G", dict(K=0), ks=(0,), row="BD_IDLE", modes=("LAYER", "DUEL"), quick=True, ref="NONE")
feat("G_BUS", "SENDS ON, no send fed (HALL + DIGITAL power-on)", "G", dict(sends=True), ks=(2,), row="BD_BUS", modes=("LAYER", "STEREO", "DUEL"), quick=True)
feat("G_CHO", "CHORUS send 100", "G", dict(sends=True, cho=100), ks=(2,), row="BD_CHO", modes=("LAYER", "DUEL"), quick=True)
for i, n in enumerate(DLY_NAMES):
    feat(f"G_DLY{i}", f"DELAY {n}, send 100", "G", dict(sends=True, dly=100, dmode=i), ks=(2,), row=f"BD_DLY[{i}]", modes=("LAYER", "DUEL"), quick=(i == 2))
for i, n in enumerate(REV_NAMES):
    feat(f"G_REV{i}", f"REVERB {n}, send 100", "G", dict(sends=True, rev=100, rtype=i), ks=(2,), row=f"BD_REV[{i}]", modes=("LAYER", "DUEL"), quick=(i == 3))
feat("G_SHD", "REVERB SHIM, PITCH DOWN", "G", dict(sends=True, rev=100, rtype=3, shp=1), ks=(2,), row="BD_SHD", modes=("LAYER", "DUEL"))
feat("G_SHM", "REVERB SHIM UP, MOD 50", "G", dict(sends=True, rev=100, rtype=3, shm=50), ks=(2,), row="BD_SHM", modes=("LAYER", "DUEL"))
feat("G_SHDM", "REVERB SHIM DOWN, MOD 50", "G", dict(sends=True, rev=100, rtype=3, shp=1, shm=50), ks=(2,), row="(the model adds two extras, not measured)")
feat("G_REV0S", "REVERB SHIM, send 0 (does the send level matter?)", "G", dict(sends=True, rtype=3), ks=(2,), ref="G_BUS", row="(send level)")
feat("G_DLY0S", "DELAY OCT UP, send 0 (does the send level matter?)", "G", dict(sends=True, dmode=3), ks=(2,), ref="G_BUS", row="(send level)")
feat("G_BYPC", "CHORUS send 100 with the CHORUS BYPASS on", "G", dict(sends=True, cho=100, byp=[("g", "CHO")]), ks=(2,), ref="G_CHO", row="(does BYPASS save the cost?)")
feat("G_BYPD", "DELAY send 100 with the DELAY BYPASS on", "G", dict(sends=True, dly=100, byp=[("g", "DLY")]), ks=(2,), ref="G_DLY0", row="(does BYPASS save the cost?)")
feat("G_BYPR", "REVERB send 100 with the REVERB BYPASS on", "G", dict(sends=True, rev=100, byp=[("g", "REV")]), ks=(2,), ref="G_REV2", row="(does BYPASS save the cost?)")
feat("G_COMP", "master COMPRESSOR 4:1", "G", dict(comp=4), ks=(2,), row="BD_COMP", quick=True)
feat("G_WIDTH", "master WIDTH 150 %", "G", dict(width=150), ks=(2,), row="BD_WIDTH", quick=True)
feat("G_BM", "master BASS MONO", "G", dict(bm=8), ks=(2,), row="BD_BMONO", quick=True)
feat("G_BYPM", "COMP + WIDTH + BASS MONO with the master BYPASS on", "G", dict(comp=4, width=150, bm=8, byp=[("g", "MST"), ("g", "CMP")]), ks=(2,), ref="G_ALLM", row="(does BYPASS save the cost?)")
feat("G_ALLM", "COMP + WIDTH + BASS MONO together", "G", dict(comp=4, width=150, bm=8), ks=(2,), row="(sum check)")
feat("G_KSV", "KEY VELOCITY on (the scan tick is not on the CPU bar)", "G", dict(keysense="VEL"), ks=(2,), row="KEY SENSE 57 (budget.c:201)")
feat("G_KSB", "KEY VELOCITY + AFTERTOUCH on", "G", dict(keysense="BOTH"), ks=(2,), row="KEY SENSE 57 (budget.c:201)")
feat("G_MUTE2", "part 2 muted", "G", dict(mute2=1), ks=(2,), row="(a muted part costs nothing?)")
FEAT = {f["key"]: f for f in FEATURES}
CROSS_KEYS = [f["key"] for f in FEATURES if f["engines"] == "ALL"]       # the features also measured on every other engine (2 keys)

# held-out combinations (never in the fit): the additive model is asked for these
HOLD = [
    ("ANALOG", 1, 3, "LAYER", dict(filt=12, sub="SINE", ins=8, eq=3, sends=True, cho=100, rev=100, rtype=2)),
    ("WAVE", 1, 4, "LAYER", dict(filt=24, twin="LP", sat=1, mx="LFO", sends=True, dly=100, dmode=1)),
    ("FM6", 1, 3, "LAYER", dict(filt=12, morph="ST", sends=True, rev=100, rtype=3, duck=1)),
    ("PHASE", 2, 4, "LAYER", dict(filt=12, twin="LAD", sub="SQ", ins=4)),
    ("TRIO", 2, 6, "LAYER", dict(filt=24, eq=2, sends=True, cho=100, dly=100, dmode=2, comp=4)),
    ("LOFI", 1, 6, "LAYER", dict(filt=12, ins=2)),
    ("WHEEL", 2, 3, "LAYER", dict(morph="SW", sends=True, rev=100, rtype=5, width=150, bm=8)),
    ("NOISE", 1, 3, "LAYER", dict(filt=12, twin="COMB", sub="PULSE")),
    ("VOICE", 2, 3, "LAYER", dict(filt=24, sat=1, ins=9, sends=True, rev=100, rtype=0, dly=100, dmode=3)),
    ("ANALOG", 2, 8, "LAYER", dict(filt=12, twin="LP", twdrv=1, twpar=1, twmod=1, mx="CYC", ins=6, sends=True, cho=100, rev=100, dly=100)),
    ("PHYS", 1, 2, "LAYER", dict(filt=12, eq=1, sends=True, cho=100)),
    ("ANALOG", 1, 4, "DUEL", dict(filt=12, sends=True, rev=100, rtype=3, shp=1, shm=50, dly=100, dmode=3)),
    ("WAVE", 2, 4, "STEREO", dict(sat=1, ins=5, sends=True, cho=100, rev=100)),
]


# ------------------------------------------------------------------------------------------------ the plan: every test
def tname(sec, eng, cores, feat, k):
    return f"CAL.{sec}.{eng}.{'1C' if cores == 1 else '2C'}.{feat}.K{k}"


def kfit(cfg):
    """the largest key count up to cfg's K whose planned load stays under MAXPCT (at least 1)"""
    k = cfg["K"]
    while k > 1 and predict_avg(dict(cfg, K=k), HINT) > MAXPCT:
        k -= 1
    return k


def build_plan(quick=False, sounds=True, cal1=False):
    """[dict(name, sec, eng, cores, feat, K, cfg, repeat, text)] in run order: baseline, features (ANALOG), the same on the other engines, the global side, the sound survey, the held-out set.
    Every count is cut down to what the unit carries (kfit), so no run is clipped by its own load.
    cal1 (item 112, suite COST-CAL1): only the short ONE-CORE set, every test with `tune core1 off`: the baseline of every engine at 1, 4, 8 keys (cut to what one core carries; every test twice), the idle mix and a muted part, the
    quick features (one core), and the one-core held-out combinations"""
    plan = []

    def add(sec, eng, cores, feat_key, k, cfg, rep, text):
        cfg = dict(cfg, K=k)
        if k:
            cfg["K"] = kfit(cfg)
        k0, k = k, cfg["K"]
        if k != k0:
            text = re.sub(r"\b\d+ keys", f"{k} keys", text)
        if any(t["name"] == tname(sec, eng, cores, feat_key, k) for t in plan):
            return
        plan.append(dict(name=tname(sec, eng, cores, feat_key, k), sec=sec, eng=eng, cores=cores, feat=feat_key, K=k, cfg=cfg, repeat=rep, text=text))
    if cal1:
        quick = True                                                                    # (the quick feature list, 1 / 4 / 8 keys; the baselines twice for a spread)
    rep_b, rep_f = (2, 2) if cal1 else (1, 1) if quick else (2, 2)
    for e in ENG_ORDER:                                                                 # B: every engine, filter bypassed, no effects, SENDS off; one core and two
        for cores in ((1,) if cal1 else (1, 2)):
            for k in counts_for(e, quick, cores):
                add("B", e, cores, "BASE", k, base_cfg(eng=e, cores=cores, K=k), rep_b, f"{e} baseline, {'two cores' if cores == 2 else 'one core'}, {k} keys")
    for f in FEATURES:                                                                  # F / G: each feature alone, ANALOG, one core
        if quick and not f["quick"] and not (cal1 and f["key"] == "G_MUTE2"):
            continue
        for mode in (f["modes"] if not cal1 or f["key"] != "G_MUTE2" else ("LAYER",)):
            ks = f["ks"] or ((1, 3) if f["kind"] == "V" else (2, 6) if f["kind"] == "P" else (2,))
            if quick and f["kind"] in ("V", "P"):
                ks = (1, 3) if f["kind"] == "V" else (2,)
            for k in ks:
                cfg = base_cfg(**{**dict(K=k, mode=mode), **f["patch"]})
                tag = f["key"] + ("" if mode == "LAYER" else "_" + mode)
                add("F" if f["kind"] != "G" else "G", "ANALOG", 1, tag, k, cfg, rep_f, f"ANALOG {f['label']}, {mode}, {k} keys")
    if not quick:                                                                       # X: the per-voice features on the other engines (2 keys)
        for e in ENG_ORDER:
            if e == "ANALOG":
                continue
            for fk in CROSS_KEYS:
                f = FEAT[fk]
                add("X", e, 1, fk, 2, base_cfg(eng=e, K=2, **f["patch"]), 1, f"{e} {f['label']}, 2 keys")
    for mode in ("DUEL", "STEREO"):                                                     # (the base of the DUEL and STEREO global rows: nothing fed, SENDS off)
        if mode == "STEREO" and quick:
            continue
        add("G", "ANALOG", 1, "G_BASE_" + mode, 2, base_cfg(K=2, mode=mode), rep_f, f"ANALOG baseline, {mode}, 2 keys")
    if sounds and not quick:                                                            # S: every factory sound of every engine, up to 4 keys, one core
        for e, n in sound_counts().items():
            for s in range(n):
                add("S", e, 1, f"S{s}", 4, base_cfg(eng=e, sound=s, K=4), 1, f"{e} factory sound {s}, 4 keys")
    for i, (e, cores, k, mode, patch) in enumerate(HOLD, 1):                            # H: held out
        if cal1 and cores != 1:
            continue
        add("H", e, cores, f"H{i}", k, base_cfg(eng=e, cores=cores, K=k, mode=mode, **patch), rep_f, f"held-out {i}: {e}, {'two cores' if cores == 2 else 'one core'}, {k} keys, {mode}, " + ", ".join(f"{a}={b}" for a, b in patch.items()))
    return plan


_ALL = []


def names_for(sec, eng, cores, feat_key):
    """[(K, test name)] of a (section, engine, cores, feature) as the plans (full and quick) hold them: the key counts are cut to what the unit carries, so a reader asks the plan, not a guess"""
    if not _ALL:
        for q in (False, True):
            _ALL.extend(build_plan(q))
    seen, out = set(), []
    for t in _ALL:
        if (t["sec"], t["eng"], t["cores"], t["feat"]) == (sec, eng, cores, feat_key) and t["name"] not in seen:
            seen.add(t["name"])
            out.append((t["K"], t["name"]))
    return sorted(out)


def plan_by_name():
    d = {}
    names_for("B", "ANALOG", 1, "BASE")
    for t in _ALL:
        d.setdefault(t["name"], t)
    return d


def test_lines(t):
    cfg = t["cfg"]
    quiet = not cfg["K"]                                   # (no keys: the test has no event to give it a length, so the tail is the length)
    L = [f"test {t['name']}  {t['text']}"] + config_lines(cfg) + event_lines(cfg)
    L += [f"measure {WINDOW[0]} {WINDOW[1]}", f"tail {SWEEP_S + 0.3 if quiet else 0.3}", "settle 1.2", "expect report", f"repeat {t['repeat']}", ""]
    return L


def seconds_of(t):
    """the run time of one test with its repeats, s: reset 0.3, the words (about 0.06 s each), settle 1.2, the run (length + tail 0.3), the result line and the loop's sleeps 0.7"""
    words = len(config_lines(t["cfg"])) + 3
    return t["repeat"] * (0.3 + 0.06 * words + 1.2 + SWEEP_S + 0.3 + 0.7)


MAXPCT = 70.0                      # a planned test is cut down to the key count whose predicted load (the table of 1.1, HINT) stays under this: a clipped run measures nothing
HINT = dict(                       # the rows of budget.c / budget_feat.c of 1.1.0-test5, frozen: for PLANNING only (the fit reads the sources' rows itself, cost_fit.read_model)
    eng=dict(ANALOG=109, FM6=184, PHASE=124, LOFI=78, VOICE=165, TRIO=100, WHEEL=166, PHYS=433, NOISE=101, WAVE=123),
    eng2={},
    g=dict(f12=89, f24=123, ktrk=1, morph_st=16, morph_sw=16, morph_fix_st=0, morph_fix_sw=0, tw_svf=86, tw_ladder=88, tw_comb=90, tw_chain_byp=42, tw_drive=46, tw_par=15, tw_mod=5,
           sub_SINE=30, sub_TRI=26, sub_SQ=43, sub_PULSE=43, mx_slot=8, mx_lfo=5, mx_cyc=1, mx_fix=0, uni_extra=0, part_on=64, ins=[0, 94, 88, 92, 96, 61, 106, 86, 89, 105, 103], sat=46,
           eq_base=6, eq_band=26, duck=34, idle=dict(LAYER=292, DUEL=261), bus=dict(LAYER=209, DUEL=484), cho=dict(LAYER=306, DUEL=552),
           dly=dict(LAYER=[234, 264, 357, 357], DUEL=[491, 488, 612, 612]), rev=dict(LAYER=[221, 219, 236, 319, 240, 241], DUEL=[483, 477, 492, 679, 521, 502]),
           shd=dict(LAYER=362, DUEL=766), shm=dict(LAYER=367, DUEL=775), shdm=dict(LAYER=410, DUEL=1057), comp=54, width=39, bm=38, keysense=57, ho=66))
HINT["eng2"] = dict(HINT["eng"])


# ------------------------------------------------------------------------------------------------ the model: a cost table -> a predicted CPU figure
def vox_per_part(cfg):
    cap = eng_cap(cfg["eng"])
    return cap if cfg["uni"] else min(cfg["K"], cap)


def predict_avg(cfg, T):
    """the CPU figure (% of a half) the table T predicts for the config. T: see cost_fit.py (instructions a sample); the same structure as budget.c (voice items, a part's fixed items, the
    globals; one core: both parts on core 0; two cores: one part's path plus the hand-over)"""
    g = T["g"]
    ge = T.get("ge", {})
    eng = cfg["eng"]

    def gv(key):                                                # (a row measured on every engine: this engine's, else the shared one)
        return ge.get(key, {}).get(eng, g[key])
    m = "DUEL" if cfg["mode"] == "DUEL" else "LAYER"
    c = T["eng"][eng]
    f = 0.0
    if cfg["filt"]:
        f += gv("f24") if cfg["filt"] == 24 else gv("f12")
        if cfg["ktrk"]:
            f += g["ktrk"]
    if cfg["twin"]:
        ty = {"LP": "tw_svf", "BP": "tw_svf", "HP": "tw_svf", "NOTCH": "tw_svf", "LAD": "tw_ladder", "COMB": "tw_comb"}[cfg["twin"]]
        f += g[ty] + (0 if cfg["filt"] else g["tw_chain_byp"]) + (g["tw_drive"] if cfg["twdrv"] else 0) + (g["tw_par"] if cfg["twpar"] else 0) + (g["tw_mod"] if cfg["twmod"] else 0)
    if cfg["sub"]:
        f += g["sub_" + cfg["sub"]]
    if cfg["morph"]:
        f += g["morph_sw"] if cfg["morph"] == "SW" else gv("morph_st")
    if cfg["mx"]:
        f += g["mx_slot"] + (g["mx_lfo"] if cfg["mx"] == "LFO" else g["mx_cyc"] if cfg["mx"] == "CYC" else 0)
    if cfg["uni"]:
        f += g.get("uni_extra", 0)
    fixed = g["part_on"] + (g["ins"][cfg["ins"]] if cfg["ins"] else 0) + (g["sat"] if cfg["sat"] else 0) + ((g["eq_base"] + cfg["eq"] * g["eq_band"]) if cfg["eq"] else 0) + (g["duck"] if cfg["duck"] else 0)
    if cfg["morph"]:
        fixed += g.get("morph_fix_sw" if cfg["morph"] == "SW" else "morph_fix_st", 0.0)
    if cfg["mx"]:
        fixed += g.get("mx_fix", 0.0)
    n = vox_per_part(cfg)
    if n == 0:
        fixed -= g["part_on"]                                   # (a part that plays nothing has no own work; its stages, if any, still run)
    glob = g["idle"][m]
    if cfg["sends"]:
        bus = g["bus"][m]
        dm, rt = cfg["dmode"], cfg["rtype"]
        bus += g["dly"][m][dm] - g["bus"][m]
        if rt == 3 and cfg["shp"] == 1:
            bus += g["shd"][m] - g["bus"][m] + ((g["shdm"][m] - g["shd"][m]) if cfg["shm"] else 0)
        elif rt == 3 and cfg["shm"]:
            bus += g["shm"][m] - g["bus"][m]
        else:
            bus += g["rev"][m][rt] - g["bus"][m]
        if cfg["cho"]:
            bus += g["cho"][m] - g["bus"][m]
        glob += bus
    if m == "LAYER":
        glob += (g["comp"] if cfg["comp"] else 0) + (g["width"] if cfg["width"] != 100 else 0) + (g["bm"] if cfg["bm"] else 0)
    if cfg["keysense"] != "OFF":
        glob += g.get("keysense", 0)
    if cfg["cores"] == 1:
        parts = 1 if cfg["mute2"] else 2
        instr = glob + parts * (fixed + n * (c + f))
    else:
        instr = glob + (g["ho"] if n else 0.0) + fixed + n * (T["eng2"].get(eng, c) + f)
    return instr / PCT


def load_json(path):
    with open(path) as fh:
        return json.load(fh)
