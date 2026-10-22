#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Click hunt probe (release_drafts/CLICK_HUNT1.md): builds build/hunt/bench_probe, the bench twin (tests/bench_sim.c) with a log of what changes from block to block.

  python3 tools/hunt_probe.py            # build/hunt/bench_probe
  HUNT_LOG=log.txt python3 tools/bench_run.py --sim build/hunt/bench_probe --record --suite OWNER-1 --only FM6-83 --runs RUNS
  (the clicks of the test: python3 tools/bench_analyze.py RUNS/*/audio/FM6-83.wav ... or the AUDIO section of the run; the sample of a click is the frame
   number, and the log's B<frame> lines are the blocks (32 frames) of the same recording: B<frame> is the first frame of a block)

The firmware is not changed: the script copies firmware/ and tests/ to build/hunt/fb, inserts a few calls (hooks) into the copy of fx.c, eng_fm6.c, eng_formant.c and
tests/bench_sim.c, and compiles tests/hunt_trace.c (which includes bench_sim.c and defines the hooks) with the flags host_tests.sh uses for bench_sim. The audio is the
same as bench_sim's, bit for bit (checked: the WAV of a test with and without the probe compare equal).

Environment of the probe (all optional; HUNT_LOG is the log file, no log without it):
  HUNT_LOG=path        one line per block of a recorded test and per changed value (see tests/hunt_trace.c)
  HUNT_WIN=lo,hi       the frames in which the stage taps (S1..S4: the engine's block, after SAT, after the INSERT, after the EQ) and the formant dump are logged
  HUNT_FREEZE=a,b|all  these ids of the parts (core.h P_*) stay at the value they had at HUNT_FAT (the morph's and the matrix's moves of them are off): bisects a click
  HUNT_FAT=frame       when HUNT_FREEZE / HUNT_SET start (default 0)
  HUNT_SET=id=value    this id is forced every block (HUNT_SET=95=0: SPREAD off)
  HUNT_FMFREEZE=1      FM6: the patch bytes stay as at the first note-on
  HUNT_CC="-DX=1"       (when building the probe) extra compiler flags
"""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
OUT = BASE / "build" / "hunt"
FB = OUT / "fb"


def sub(path, old, new):
    s = path.read_text()
    if old not in s:
        sys.exit(f"hunt_probe: the anchor is gone from {path.name}: {old[:60]!r}")
    path.write_text(s.replace(old, new, 1))


def hooks():
    fx = FB / "firmware/src/fx.c"
    f6 = FB / "firmware/src/eng_fm6.c"
    fm = FB / "firmware/src/eng_formant.c"
    bs = FB / "tests/bench_sim.c"
    # after mod_begin: the block's values of the part (t->p after the morph and the matrix)
    sub(fx, "    mod_begin(t);                                       /* the matrix's per-block values into t->p (mod.c) */\n", "    mod_begin(t);\n    hunt_blk(t);\n")
    sub(fx, "static int32_t pfx_x[NLIVE];", "static void hunt_blk(track_t *t);\nstatic void hunt_dist(uint32_t pp, int32_t dx, int32_t di, int32_t w0);\nstatic void hunt_io(uint32_t pp, const int32_t *a, const int32_t *b, uint32_t n);\n"
                  "static void hunt_stg(uint32_t pp, uint32_t id, const int32_t *b, uint32_t n);\nstatic int32_t pfx_x[NLIVE];")
    sub(fx, "    m->fxm = fxm;\n    m->pan_mv = pan_mv;", "    hunt_dist(pp, dist_x[pp], t->p[P_DIST], m->dist_rw0);\n    m->fxm = fxm;\n    m->pan_mv = pan_mv;")
    sub(fx, "    if (track_render(t, b, n))\n", "    if (track_render(t, b, n) | (hunt_stg(m->pp, 1, b, n), 0))\n")
    sub(fx, "            if (m->dist_rw0 != dist_x[pp] && (m->dist_rw0 < 12 * 256 || dist_x[pp] < 12 * 256))\n",
        "            int32_t hb_[64];\n            memcpy(hb_, b, n * sizeof b[0]);\n            if (m->dist_rw0 != dist_x[pp] && (m->dist_rw0 < 12 * 256 || dist_x[pp] < 12 * 256))\n")
    sub(fx, "                track_dist(t, b, n);\n            if (t->p[P_ITYPE] | t->ins_run)\n", "                track_dist(t, b, n);\n            hunt_io(pp, hb_, b, n);\n            hunt_stg(pp, 2, b, n);\n            if (t->p[P_ITYPE] | t->ins_run)\n")
    sub(fx, "                track_insert(t, b, n);                  /* the INSERT (1.5): off, nothing */\n", "                track_insert(t, b, n);\n            hunt_stg(pp, 3, b, n);\n")
    sub(fx, "                track_eq(t, b, n, 0u);                  /* the EQ (DUELL): all bands at 0 dB, nothing */\n        }\n", "                track_eq(t, b, n, 0u);\n            hunt_stg(pp, 4, b, n);\n        }\n")
    # FM6: the patch as the engine reads it
    sub(f6, "static void fm6_render(track_t *t", "struct fm6_note_s;\nstatic void hunt_fm6(uint32_t tr, const uint8_t *p, fm6_note_t *n, voice_t *v);\nstatic void fm6_render(track_t *t")
    sub(f6, "    fm6_note_ol(n, p);", "    hunt_fm6(tr, p, n, v);\n    fm6_note_ol(n, p);")
    # VOICE: the formant block
    sub(fm, "static void formant_render(track_t *t", "static void hunt_fmt(track_t *t, voice_t *v, int32_t *fa, int32_t *ba, int32_t k, uint32_t ratio, uint32_t bws, uint32_t f0, const vmod_t *m);\nstatic void formant_render(track_t *t")
    sub(fm, "    fres_coef(&r1, (uint32_t)fa[0], (uint32_t)ba[0]);", "    hunt_fmt(t, v, fa, ba, k, ratio, bws, f0, m);\n    fres_coef(&r1, (uint32_t)fa[0], (uint32_t)ba[0]);")
    # the twin: the recording's frame count at each block
    sub(bs, "        mix_block(ob, CTL);\n", "        hunt_frame = rec_f ? rec_frames : 0xFFFFFFFFu;\n        mix_block(ob, CTL);\n        hunt_after(ob, CTL);\n")
    sub(bs, "static void sim_half(void)", "static uint32_t hunt_frame;\nstatic void hunt_after(const int32_t *o, uint32_t n);\nstatic void sim_half(void)")


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    FB.mkdir(parents=True)
    for d in ("firmware", "tests"):
        shutil.copytree(BASE / d, FB / d, ignore=shutil.ignore_patterns("*.pyc", "__pycache__"))
    (FB / "build").mkdir()
    for d in ("gen", "gen_test"):
        shutil.copytree(BASE / "build" / d, FB / "build" / d)
    hooks()
    fv = re.search(r'#define DUELL_VERSION_STR "(.*)"', (BASE / "build/gen/duell_version.h").read_text()).group(1)
    cmd = ["cc", "-O1", "-fno-builtin", "-D_FORTIFY_SOURCE=0", "-w", "-DHUNT_PROBE=1", "-DDUELL_TEST_MODE=1", "-Ibuild/gen_test", "-Ibuild/gen", "-Ifirmware/src", "-Itests",
           f"-DFELUCCA_VERSION=\"{fv}\"", "-o", str(OUT / "bench_probe"), "tests/hunt_trace.c", "-lm"]
    cmd[1:1] = os.environ.get("HUNT_CC", "").split()          # extra compiler flags (hunt3: -DX=1 to try a variant)
    subprocess.run(cmd, cwd=FB, check=True)
    print(f"hunt_probe: {OUT / 'bench_probe'}")


if __name__ == "__main__":
    main()
