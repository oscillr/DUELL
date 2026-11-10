#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""STRESS-ALL: every engine with EVERYTHING ON (owner rule G25, 2026-11-07: "make crazier testing patches ... chords with 20 voices ... the fastest LFO modulating everything").

  python3 tools/gen_stress_suite.py            writes tools/bench_suites/STRESS-ALL.txt

A stress sound is the engine's dearest factory sound (`sound N ENGINE DEAR`, which strips it) with everything put back ON by `set`, on both parts:
  MULTI filter 24 dB, resonant, MODE moved; TWIN filter LP in PARALLEL, BLEND BOTH, its own LFO / ENV depths; SUB pulse at full level with HARMONICS; the CYCLE envelope;
  both LFOs at their fastest; all 8 matrix slots from LFO 1 / LFO 2 / S&H / CYCLE at full depth to PITCH, CUT, SHP, AMP, PAN, SAT, RES and an engine value; SAT, an INSERT;
  SPREAD at the top; all three sends at the top; MORPH rolled to an End B at 100 % with the fader in the middle.
  Owner, 2026-11-07 (the measurement patch, rule G25 refined): EQ OFF on both parts, DUCK OFF, the TWIN type a plain one (LADDER and COMB are allowed to give fewer voices);
  the sends, SAT, the TWIN and everything else ON.
Globals heavy: SHIMMER-type reverb settings of the earlier suites, COMPRESSOR, WIDTH, BASS MONO.
Tests per engine: both parts, KEYS keys each, the budget OPEN (no limit: the test says what the unit can do, the screen's number is checked against it later).
"""
from pathlib import Path

ENGINES = ["ANALOG", "PHASE", "LOFI", "VOICE", "TRIO", "WHEEL", "PHYS", "NOISE", "FM6", "WAVE"]
SHOWN = {"PHYS": "PHYSICAL"}                     # (the console's engine word is the stored key; names in the suite say PHYSICAL)
NOTES = ["C", "D", "E", "F", "G", "A", "B"]
GLOBALS = ["mods ALL ON", "mods SENDS ON", "master COMP 4", "master WIDTH 150", "master BM 3",
           "set g 15 3", "set g 24 3", "set g 26 1", "set g 17 50"]


def keys(n, start=2):
    out, o, i = [], start, 0
    while len(out) < n:
        out.append(f"{NOTES[i]}{o}")
        i += 1
        if i == 7:
            i, o = 0, o + 1
    return " ".join(out)


def everything_on(p, eng):
    s = [
        f"set {p} 45 52", f"set {p} 46 96", f"set {p} 47 40", f"set {p} 48 2",          # MULTI: CUT, RES, MODE moved, 24 dB
        f"set {p} 43 1",                                                                  # TWIN TYPE: LP (owner 2026-11-07: a plain type; LADDER / COMB may give fewer voices)
        f"set {p} 106 40", f"set {p} 101 0", f"set {p} 128 1",                            # TWIN CUTOFF, BLEND BOTH, ROUTE PARALLEL
        f"set {p} 131 40", f"set {p} 132 50", f"set {p} 133 -50",                         # TWIN ENV 2 / LFO 1 / LFO 2 depths
        f"set {p} 134 127", f"set {p} 135 0", f"set {p} 136 3", f"set {p} 137 127",       # SUB: full level, -2 octaves, PULSE, HARMONICS
        f"set {p} 139 5", f"set {p} 140 5",                                               # CYCLE: fast rise and fall
        f"set {p} 9 127", f"set {p} 10 3", f"set {p} 77 127", f"set {p} 78 4",            # LFO 1 and LFO 2 at their fastest
        f"set {p} 5 40", f"set {p} 6 10", f"set {p} 7 40", f"set {p} 13 10", f"set {p} 14 -40", f"set {p} 15 30",   # the LFOs' own depth cards
    ]
    srcs = [1, 12, 9, 15, 1, 12, 9, 15]                                                   # LFO1, LFO2, S&H, CYCLE
    dsts = [1, 2, 3, 4, 5, 6, 12, 2]                                                      # PITCH, CUT, SHP, AMP, PAN, SAT, E1, CUT
    for k in range(8):
        b = 49 + 3 * k
        s += [f"set {p} {b} {srcs[k]}", f"set {p} {b + 1} {dsts[k]}", f"set {p} {b + 2} {63 if k % 2 == 0 else -50}"]
    s += [
        f"set {p} 33 50",                                                                 # SAT
        f"set {p} 96 3", f"set {p} 97 90", f"set {p} 98 90", f"set {p} 99 90", f"set {p} 100 127",   # an INSERT, its A B C, MIX
        # (EQ and DUCK stay OFF: owner 2026-11-07)
        f"set {p} 95 127",                                                                # SPREAD at the top
        f"set {p} 34 127", f"set {p} 35 127", f"set {p} 36 127",                          # the three sends
        f"set {p} 2 90", f"set {p} 3 110", f"set {p} 4 90",                               # AMP ENV: long, held
        f"morph {p} ROLL 11 100", f"morph {p} FADER 64",                                  # MORPH: an End B at 100 %, the fader in the middle
    ]
    return s


def test(name, desc, eng, events, extra=()):
    lines = [f"test {name}  {desc}", "mode STEREO", f"sound 1 {eng} DEAR", f"sound 2 {eng} DEAR", *GLOBALS]
    lines += everything_on(1, eng) + everything_on(2, eng) + list(extra)
    lines += ["budget OPEN", "tune core1 on", *events, "at 5.0  release", "tail 1", "measure 1.0 5.0", "expect report", ""]
    return "\n".join(lines)


def one_engine():
    """STRESS-ONE: how many voices ONE engine plays cleanly, part 1 alone, budget OPEN: 8 / 10 / 12 keys (PHYSICAL 2 / 3 / 4)"""
    out = ["# STRESS-ONE (2026-11-07): how many voices ONE engine plays cleanly with the measurement patch (gen_stress_suite.py: everything on but EQ and DUCK, TWIN LP), part 1 alone, budget OPEN.", "suite STRESS-ONE", ""]
    for e in ENGINES:
        nm = SHOWN.get(e, e)
        for k in ((2, 3, 4) if e == "PHYS" else (8, 10, 12)):
            lines = [f"test SO_{nm}_{k}  {nm}, everything on, part 1 alone, {k} keys", "mode STEREO", f"sound 1 {e} DEAR", *GLOBALS] + everything_on(1, e)
            lines += ["budget OPEN", "tune core1 on", f"at 0.0  chord {keys(k)} vel 127 ch 1", "at 5.0  release", "tail 1", "measure 1.0 5.0", "expect report", ""]
            out.append("\n".join(lines))
    Path(__file__).resolve().parents[1].joinpath("tools/bench_suites/STRESS-ONE.txt").write_text("\n".join(out))


def main():
    one_engine()
    out = [__doc__.replace('"""', "").strip().replace("\n", "\n# ").join(["# ", ""]), "suite STRESS-ALL", ""]
    for e in ENGINES:
        nm = SHOWN.get(e, e)
        out.append(test(f"ST_{nm}_10", f"{nm}, everything on, both parts, 10 keys each, no limit", e,
                        [f"at 0.0  chord {keys(10)} vel 127 ch 1", f"at 0.0  chord {keys(10, 3)} vel 127 ch 2"]))
    for e in ("ANALOG", "WAVE"):
        out.append(test(f"ST_{e}_ARP", f"{e}, everything on, both parts, a 20-key chord on part 1 played by the fastest ARP (UP, 4 octaves, long gate, HOLD), "
                        f"a 10-key chord on part 2", e,
                        [f"at 0.0  chord {keys(20)} vel 127 ch 1", f"at 0.0  chord {keys(10, 3)} vel 127 ch 2"],
                        extra=["set 1 17 1", "set 1 18 16", "set 1 19 4", "set 1 20 127", "set 1 23 1", "set 1 4 127"]))
        out.append(test(f"ST_{e}_CHORD20", f"{e}, everything on, both parts, 20 keys each with CHORD mode on (each key a chord)", e,
                        [f"at 0.0  chord {keys(20)} vel 127 ch 1", f"at 0.0  chord {keys(20, 3)} vel 127 ch 2"],
                        extra=["set 1 81 9", "set 2 81 9"]))
    Path(__file__).resolve().parents[1].joinpath("tools/bench_suites/STRESS-ALL.txt").write_text("\n".join(out))


if __name__ == "__main__":
    main()
