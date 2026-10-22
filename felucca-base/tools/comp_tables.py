#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(0.2.24: ATTACK and RELEASE are fine, 40 steps a decade, and fx.c computes the coefficient (cmp_coef); CMP_AT / CMP_RT below are the 0.2.21 tables, kept as the reference the defaults are checked against by tests/comp_test.c)
DUELL 0.2.21 COMP, the master compressor (firmware/src/fx.c comp_stage): prints the tables that fx.c carries.

  CMP_LG   256 log2(1 + (i + 1/2) / 128), i = 0..127: the fraction of an octave of an amplitude (cmp_lg)
  CMP_EX   32768 2^(-i / 128), i = 0..128: the gain of i / 128 octave of reduction, Q15 (cmp_exp2)
  CMP_AT   the ATTACK settings as {m, sh}: the detector moves pk += d x m >> sh every hop of 8 samples; m / 2^sh is the nearest to the
           printed time (core.h CMP_ATK_US) as a one-pole of the hop: k = 1 - exp(-8 / (time x 44100))
  CMP_RT   the same for the RELEASE settings (core.h CMP_REL_MS), less the hold: the printed release is the time to cover 63 % of a fall,
           the detector's hold included (an average of 544 samples, 12.3 ms: tests/comp_test.c measures it), so the pole is the printed time less that

Usage: python3 tools/comp_tables.py        (prints the C text; fx.c must carry exactly these numbers: tests/comp_test.c checks the times they give)"""
import math

FS = 44100.0
HOLD = 544.0                                            # samples: the hold's mean delay (a window of 8 blocks of 64 and the one running)
ATK_US = [100, 150, 200, 300, 500, 700, 1000, 1500, 2000, 3000, 5000, 7000, 10000, 15000, 20000, 30000, 50000, 70000, 100000]
REL_MS = [20, 30, 50, 70, 100, 150, 200, 300, 500, 700, 1000, 1500, 2000]


def lg_table():
    return [min(255, int(round(256 * math.log2(1 + (i + 0.5) / 128)))) for i in range(128)]


def ex_table():
    return [int(round(32768 * 2 ** (-i / 128))) for i in range(129)]


def best_for(tau_hops):
    """the coefficient m / 2^sh (m 1..255, sh 1..24) whose one-pole, per hop, has the time constant nearest to tau_hops"""
    k = 1 - math.exp(-1.0 / tau_hops)
    best = None
    for sh in range(1, 25):
        m = round(k * 2 ** sh)
        if m < 1 or m > 255 or m >= 2 ** sh:
            continue
        err = abs(-1 / math.log(1 - m / 2 ** sh) / tau_hops - 1)
        if best is None or err < best[0] - 1e-9:
            best = (err, m, sh)
    return best


def rows(a, w):
    return "\n".join("    " + ", ".join(str(x) for x in a[k:k + w]) + "," for k in range(0, len(a), w))


if __name__ == "__main__":
    print("static const uint8_t CMP_LG[128] = {\n" + rows(lg_table(), 16) + "\n};")
    print("static const uint16_t CMP_EX[129] = {\n" + rows(ex_table(), 12) + "\n};")
    at = [best_for(us * 1e-6 * FS / 8) for us in ATK_US]
    rt = [best_for((ms * 1e-3 * FS - HOLD) / 8) for ms in REL_MS]
    print("CMP_AT = { " + ", ".join("{%d, %d}" % (b[1], b[2]) for b in at) + " };")
    print("CMP_RT = { " + ", ".join("{%d, %d}" % (b[1], b[2]) for b in rt) + " };")
    print("# worst error of the pole: attack %.2f %%, release %.2f %%" % (max(b[0] for b in at) * 100, max(b[0] for b in rt) * 100))
