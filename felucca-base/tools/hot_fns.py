#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL_HOTRAM (bench builds R1..R3 only): which audio functions run from on-chip RAM, and how they get there.

  hot_fns.py IN.o OUT.o LEVEL      (build.py calls hot_place(); this command line is for a look by hand)

The hypothesis (2026-10-15): the audio code runs from the flash (XIP) through a 32 KB instruction cache, and where the
linker happens to put it changes the missed halves 2-4x (bench A / D-off / C). The official M-VAVE firmware keeps all its
audio code in SRAM. These builds move the hottest functions to RAM and nothing else: the compiled code is the same.

How (and why not a section attribute in C): the JieLi clang 4 never inlines a function into a caller of another section
(tried: a two-line static helper stays a call when the caller has __attribute__((section)), and a callee with a section is
not inlined into a .text caller either). A HOT attribute would change the code it marks: in a first R1 build wv_lvl lost its
inlined sample loop (wv_read: 250 -> 72 B, a call back into the flash per block) and wave_render 1148 -> 782 B. So the C is
left alone: felucca.c is compiled exactly as for bench A (-ffunction-sections: a function f is section .text.f), then the
listed sections are renamed .text.f -> .hotx.f in felucca.o (the same length: the section-name bytes are edited in place;
a tail-merged name such as .rela.text.f becomes .rela.hotx.f with it, relocations follow their section by index), and
firmware/app_hot.ld links .hotx.* after the flash driver in .ram_text (the top of RAM, in direct-call reach of the flash).

The groups (device sizes from bench A's ELF, -Os; shares: host sampling of mix_block, `sample` on the -O1 -fno-inline host
build of tests/bench_test.c (the bench scene, bench_multi.h) and tests/cpu_scenarios_test.c (M01..M04, M14, M15), each sample
given to the innermost function that is a symbol of the device ELF):
  1 the per-voice inner loops: the multimode filter, FM6's operators, WAVE's render and its sample loop   3054 B
  2 + the effects: EQ, inserts, DIST, DUCK, voice spread, reverb ROOM / SHIM, the modulated delay     + 4554 B
  3 + what else is hot and fits: FM6's note and voice render, WAVE's line, the sine, mod, morph, PERFORM stages   + 4710 B
The top 30 (share of mix_block's samples: the mean of the two scenes; size in bench A; cumulative; group or why not):
   1 uf_run            14.9 % (bench 12.7, multis 17.1)   1074 B  cum 14.9 %   1074 B  R1
   2 fm6_op_run         8.1 % (bench 13.3, multis  3.0)    318 B  cum 23.1 %   1392 B  R1
   3 mix_block          5.9 % (bench  6.2, multis  5.7)   7994 B  cum 29.0 %   9386 B  -  7.9 KB, mostly cold paths: too big
   4 eq_run             5.5 % (bench 10.7, multis  0.3)    388 B  cum 34.5 %   9774 B  R2
   5 fm6_op_fb          5.4 % (bench  8.7, multis  2.0)    140 B  cum 39.9 %   9914 B  R1
   6 wave_render        4.4 % (bench  3.0, multis  5.8)   1148 B  cum 44.3 %  11062 B  R1
   7 analog_render      4.2 % (bench  0.0, multis  8.3)   1998 B  cum 48.5 %  13060 B  -  not in the bench scene
   8 wv_lvl             3.9 % (bench  2.6, multis  5.2)    250 B  cum 52.4 %  13310 B  R1
   9 ins_swept          3.4 % (bench  6.4, multis  0.4)    768 B  cum 55.7 %  14078 B  R2
  10 rev_hall           3.3 % (bench  0.1, multis  6.5)   1622 B  cum 59.0 %  15700 B  -  not in the bench scene
  11 uf_voice           3.1 % (bench  2.5, multis  3.7)     90 B  cum 62.1 %  15790 B  R1
  12 fm6_note_compute   2.3 % (bench  3.9, multis  0.8)   1126 B  cum 64.5 %  16916 B  R3
  13 fm6_render         2.2 % (bench  3.7, multis  0.7)    710 B  cum 66.7 %  17626 B  R3
  14 rev_room           2.1 % (bench  4.0, multis  0.2)    310 B  cum 68.8 %  17936 B  R2
  15 analog_render_f    2.0 % (bench  0.0, multis  4.0)   1920 B  cum 70.8 %  19856 B  -  not in the bench scene
  16 rev_hall_b         1.8 % (bench  0.0, multis  3.5)   1614 B  cum 72.6 %  21470 B  -  not in the bench scene
  17 wv_line            1.5 % (bench  1.0, multis  1.9)    610 B  cum 74.1 %  22080 B  R3
  18 track_dist         1.5 % (bench  1.2, multis  1.8)    218 B  cum 75.6 %  22298 B  R2
  19 sine_i             1.4 % (bench  0.8, multis  2.1)     40 B  cum 77.0 %  22338 B  R3
  20 dly_mod            1.3 % (bench  2.7, multis  0.0)   1192 B  cum 78.3 %  23530 B  R2
  21 split_master       1.3 % (bench  0.0, multis  2.6)    910 B  cum 79.6 %  24440 B  -  not in the bench scene
  22 fx_buses_m         1.3 % (bench  0.0, multis  2.5)    700 B  cum 80.9 %  25140 B  -  not in the bench scene
  23 mix_spread         1.2 % (bench  1.5, multis  0.9)    656 B  cum 82.1 %  25796 B  R2
  24 mod_begin          1.0 % (bench  0.9, multis  1.1)   1082 B  cum 83.1 %  26878 B  -  1.1 KB for 0.9 %: did not fit
  25 wv_warp            0.9 % (bench  0.5, multis  1.3)     34 B  cum 84.0 %  26912 B  R1
  26 split_buses        0.9 % (bench  0.0, multis  1.7)    846 B  cum 84.9 %  27758 B  -  not in the bench scene
  27 pd_wave            0.8 % (bench  0.0, multis  1.6)    198 B  cum 85.7 %  27956 B  -  not in the bench scene
  28 osc_sine           0.8 % (bench  0.5, multis  1.2)     40 B  cum 86.5 %  27996 B  R3
  29 duck_run           0.8 % (bench  1.4, multis  0.2)    418 B  cum 87.3 %  28414 B  R2
  30 trio_render        0.6 % (bench  0.0, multis  1.3)   2612 B  cum 88.0 %  31026 B  -  not in the bench scene
In the bench scene R1 covers 43 % of those samples, R2 73 %, R3 88 % (M01..M04, M14, M15: 38 / 42 / 51 %: their ANALOG and
HALL reverb stay in the flash). Host shares only: the device's own cost per function is not measured.
The RAM it may use: app_hot.ld RAMCODE, 15,436 B (0x01C7C100..0x01C7FD4C), of it the flash driver 2,924 B: 12,512 B for these
(R3 uses 12,320). Other free RAM is out of a direct call's reach of the flash (below 0x01C6E6C6) or not RAM without a cache
setup DUELL does not do (the 28 KB of D-cache ways at 0x01F28000 the vendor's SDK frees in closed code).
A function inlined everywhere on the device has no section of its own: it goes where its callers go (listed as missing).
Only functions that the audio interrupt runs are listed; the interrupt entry, USB, MIDI, the update path stay in the flash.
"""
import struct
import sys

GROUPS = {
    1: ["uf_run", "uf_voice", "fm6_op_run", "fm6_op_fb", "wave_render", "wv_lvl", "wv_warp"],
    2: ["eq_run", "ins_swept", "track_dist", "duck_run", "mix_spread", "track_insert",
        "rev_room", "rev_shim", "dly_mod", "fx_buses_mod"],
    3: ["fm6_note_compute", "fm6_render", "fm6_carriers", "fm6_note_of", "wv_line", "sine_i", "osc_sine",
        "uf_coef", "spread_clear", "mod_voice", "mod_voice_add", "morph_end", "perf_flanger", "perf_drive"],
}
# 0.2.14 merge (2026-10-15): the set re-checked against the merged code. track_dist grew 218 -> 316 B (the DIST blend), mix_spread 656 -> 798 B
# (MODULES PART FX), and the 12,596 B of R3 overflowed RAMCODE by 84 B. perf_phaser (384 B) went back to the flash: it runs only while PERFORM's
# FX is held on that stage, not in the steady state; the new PERFORM filter (perform.c perf_filter, 852 B) is in the flash too (it did not fit).
# The MODULES paths (pfx_*, snd_*) run only during a fade or an OFF: in the flash. Set now 12,212 B of 12,512 B.
# DELAY MODE (RAND, OCT+; 2026-10-15): dly_ev (2,118 B), dlyev_read (316 B) and fx_buses_ev (128 B) are new audio functions and are NOT in the set (it is unchanged:
# 12,292 B of 12,512 B, no room for 2.5 KB); like fx_buses_m they run from the flash through the cache. The default DELAY path is the code that was.
# 0.2.14 pruning (hidden code removed): mulq16 is inlined in all its callers now (the DRUM engine's out-of-line calls are gone), so it is no function and
# left the set (31 functions, 12,180 B of 12,512 B). No other name of the set moved.
# 0.2.14 PLATE (rev_plate, rev_plate_b; the sixth reverb): in the flash. Estimated 0.85 .. 1.3 KB each on the device (arm64 -Os: 1,236 B; rev_room is
# 448 B there and 310 B on the device, rev_hall 1,576 B there and 1,622 B on the device) against the 300 B left in RAMCODE. Not in group 2/3.
# 0.2.14 rel0214c merge (2026-10-15; names, dx7fix, lofi, plate, stoptail, perfleave on top of the pruned set): the set is unchanged. Grown inside it:
# track_insert 182 -> 228 B (TAPE / VINYL dispatch), fx_buses_mod 122 -> 136 B (the LOCUT call), fm6_note_compute 1,126 -> 1,160 B (the AMS fix): the
# hot block is 12,292 B of 12,512 B (220 B free). In the flash: rev_plate (1,594 B), rev_locut (276 B), ins_lofi (1,028 B), snd_lines_clear and
# the STOP ALL machinery (snd_*), which run once or in a fade. Nothing went back to the flash.
# 0.2.14 wip/prune2 (2026-10-15; the old-format readers removed: project.c, multi.c, upreset.c, settings_persist.c, core.h helpers; -4,688 B of flash): the R3 set is
# unchanged (none of its functions touched; 12,292 B of 12,512 B, 220 B free, elfcheck ok).
# 0.2.15 rel0214d merge (wip/usertables, wip/delaymode, prune2; 2026-10-15): the set is unchanged (31 functions). The user-table branch in wv_frame (eng_wave.c) and wtab.c are in the
# flash (wv_frame builds a table on a table change, not per block); the hot block grew by 20 B to 12,312 B of 12,512 B (200 B free), from the wave functions' user-table
# checks. elfcheck ok.
# 0.2.19 SHIM PITCH / MOD: the set is unchanged (31 functions). rev_shim only gained the test that calls rev_shimx (fx_bus.c, 1.8 KB in the flash, not in the set: it runs only with PITCH DOWN / BOTH or MOD on);
# 0.2.19, all merged (level split, BASS MONO, SHIM PITCH / MOD): 31 functions as before, 12,364 B of 12,512 B (148 B free), elfcheck ok.
# 0.2.19 with OUTPUT EQ (the low cut and BASS+) removed: master_out and master_out_split lost them; 31 functions, 12,348 B of 12,512 B (164 B free), elfcheck ok.
# the hot block grew by 60 B to 12,372 B of 12,512 B (140 B free), elfcheck ok. The default SHIM runs the loop of 0.2.18 in RAM, bit for bit.
# 0.2.21 STEREO CHORUS (fx_bus.c cho_tap: a second tap in the chorus loops): the set is unchanged (31 functions); dly_mod and fx_buses_mod grew with the second read, the hot block
# is 12,496 B of 12,512 B (16 B free, was 164), elfcheck ok. fx_buses_m (the plain bus, inlined into fx_buses) and dly_ev are in the flash. Nothing went back to the flash.
# 0.2.22 DELAY MODES IN DUEL: the event delay is a second copy for bus B (dly_ev_b, fx_buses_ev_b, dlyev_read_b: flash, +2.6 KB, not in the set); the set is unchanged (31 functions), the hot block
# stays 12,500 B of 12,512 B (12 B free). Bus B's state dlyev_b is in the pool (.data+.bss unchanged: in the .bss it moved addresses and grew duck_run and mod_voice by 12 B), elfcheck ok.
# 0.2.21 merged (KEY TRK, COMP, GLO round, STEREO CHORUS, chorus skip): the set is unchanged (31 functions); track_render (KEY TRK's multiply) and comp_stage are in the flash (not in the set), as are cho_zrun and the skip
# (fx_buses_m is inlined into fx_buses, in the flash); the hot block is 12,500 B of 12,512 B (12 B free), elfcheck ok. Nothing went back to the flash.
# 0.2.25 PART EQ IN 32-BIT MULTIPLIES (fx.c eq_run: 32 x 32 -> 64 multiply-accumulates instead of 64 x 64 products and a 64-bit clamp): the set is unchanged
# (31 functions); eq_run 388 -> 208 B (its loop 85 -> 32 device instructions a sample a band), the hot block 12,492 -> 12,312 B of 12,512 B (200 B free), elfcheck ok.
# Bisection builds (2026-10-15, after the unit's R1..R3 bench: BASE 75.0 -> 89.5 with R2's functions in RAM, 88.7 with R3's): levels
# 4..6 are R3's set (group 1..3) without the functions named here, to find which of R2's functions raise the idle load.
# The host cannot tell (all of them run every block in the bench scene: eq_run 9 x, ins_swept 2 x, rev_room / rev_shim / dly_mod 1 x a block).
# R4 = R3 minus eq_run (the biggest R2 share in the bench scene, 10.7 % of mix_block's samples, 64-bit state in memory)
# R5 = R3 minus the per-track chain (EQ, INSERT, DIST, DUCK, SPREAD)
# R6 = R3 minus the bus effects (ROOM, SHIM, the modulated delay)
WITHOUT = {
    4: ["eq_run"],
    5: ["eq_run", "ins_swept", "track_insert", "track_dist", "duck_run", "mix_spread"],
    6: ["rev_room", "rev_shim", "dly_mod", "fx_buses_mod"],
}
# Layout bisection builds (2026-10-15, after R4..R6 showed that no single group removes the high BASE):
# R7 = ONLY group 1 (R1's functions, same addresses as in R1 and R3: group 1 comes first) in RAM, the hot block padded with zeros at its
#      end to R3's size (PAD[7] bytes, app_hot.ld _hot_pad): hot block, stack, data, pool addresses as R3's. High BASE = layout, not code.
# R8 = R3's set minus group 1 (the functions R2 and R3 added; no filter, no FM6 operators, no WAVE render). BASE ~75 = the culprit is
#      an interaction with group 1 or the amount.
WITHOUT[7] = []
WITHOUT[8] = GROUPS[1]
ONLY = {7: [1]}                                 # level -> the groups it takes (default: 1..3)
PAD = {7: 12320}                                # level -> the hot block's size (bytes after _hot_start; R3's: 12,320 B)
OLD, NEW = b".text.", b".hotx."                # the same length: renamed in place


def names(level):
    """the functions of groups 1..level (levels 4..6: groups 1..3 without WITHOUT[level]), in group order (the order they are laid out in RAM)"""
    if level in ONLY:
        return [f for g in ONLY[level] for f in GROUPS[g]]
    top, skip = (3, WITHOUT[level]) if level in WITHOUT else (level, [])
    return [f for g in range(1, top + 1) for f in GROUPS[g] if f not in skip]


def hot_place(obj, level):
    """-> (the object with the hot functions' sections renamed, [renamed names], [names without a section])"""
    d = bytearray(obj)
    if d[:4] != b"\x7fELF" or d[4] != 1 or d[5] != 1:
        raise SystemExit("hot_fns: not a 32-bit little-endian ELF")
    shoff, = struct.unpack_from("<I", d, 32)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 46)
    sh = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]
    base = sh[shstrndx][4]

    def name(off):
        return bytes(d[base + off:d.index(b"\0", base + off)])
    by_name = {name(s[0]): s for s in sh}
    done, missing = [], []
    for f in names(level):
        s = by_name.get(OLD + f.encode())
        if s is None or s[1] != 1:              # SHT_PROGBITS
            missing.append(f)
            continue
        at = base + s[0]
        d[at:at + len(OLD)] = NEW
        done.append(f)
    left = {name(s[0]) for s in sh}
    if any(n.startswith(OLD) and n[len(OLD):].decode() in done for n in left):
        raise SystemExit("hot_fns: a renamed section kept its old name (a shared string?)")
    return bytes(d), done, missing


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(__doc__.strip().splitlines()[2])
        sys.exit(2)
    out, done, missing = hot_place(open(sys.argv[1], "rb").read(), int(sys.argv[3]))
    open(sys.argv[2], "wb").write(out)
    print(f"hot_fns: {len(done)} renamed .text.* -> .hotx.*; no section (inlined): {missing}")
