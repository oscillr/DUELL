# SPDX-License-Identifier: GPL-3.0-only
"""DUELL_RAMPLACE (two-core stage 1a, developer builds only): the code of range X, the old program RAM 0x01C00000..0x01C07F00.

X is 31.75 KB (32,512 B; crt0 keeps a debug area at 0x01C07F00). From there a direct call (+-4 MiB) does not reach the flash
(0x02000120 is 0x400120 away) and the flash cannot call into it. So X holds closed clusters only: the render side of seven engines.
Each engine is entered through its engine_t pointers (render, amp: no reach limit) and calls nothing but itself and range Z or the hot
block (RAM to RAM calls always reach). What they called in the flash before (memset, part_rand, PHYS's phys_slot_of / phys_reset,
which phys_note_on in the flash also calls) is in range Z (tools/ram_fns_z.py), which both the flash and X reach.

The sets are the closures of each engine's audio entries (render, amp) in the call graph of felucca.o (relocations), measured on cloud/base
85dee78 (-Os): no function of a set has a caller in the flash. tools/ram_place.py checks that again at every RAMPLACE build (before the
link) and tools/elfcheck.py checks the linked result (no call from X leaves RAM). Device sizes of that build:
  PHYS 8,256 B, ANALOG 8,646, TRIO 3,464, PHASE 3,722, LOFI 2,490, VOICE 2,404, NOISE 1,650: 30,632 B of 32,512.
"""
import os

ENGINES = {
    "PHYS": ["phys_render", "px_string_block", "px_string_run", "px_memb_block", "px_modal_block", "px_modal_run",
             "px_string_excite", "px_symp_run", "px_symp_block", "px_modal_exciter", "px_exp2", "px_svf_coef",
             "phys_symp_pitches", "px_sin", "px_modal_q", "px_isqrt64"],
    "ANALOG": ["analog_render", "analog_render_m", "analog_render_x", "atg_analog", "atg_osc_b"],
    "TRIO": ["trio_render", "trio_pass", "trio_naive"],
    "PHASE": ["phase_render", "phase_render_m", "pd_line", "pd_setup", "pd_wave", "pd_sl", "pd_pair", "pd_gap"],
    "LOFI": ["lofi_render", "lofi_render_m", "lofi_amp"],
    "VOICE": ["formant_render", "fres_coef", "vowel_at", "fmt_ratio"],
    "NOISE": ["noise_render", "noise_inc"],
}
NAMES = [f for fs in ENGINES.values() for f in fs]
# DUELL_TWOCORE=1: core 1's own code (hal/fm1_core1.h: the waiting loop fm1_c1_main with the job inlined, and the entry stub fm1_c1_entry, both in the section
# .c1_text) is linked into this range by firmware/app_ram.ld (KEEP(*(.c1_text*)) in .ramx): inside the PC window, and it calls only RAM (the part chain is in range Z).
# It is not a .text.f section, so ram_place does not rename it; elfcheck counts it in the range (ELF_NAMES).
C1_NAMES = ["fm1_c1_main"]
# join0234: T0229's mix_ramp (588 B, the ramped mix of PAN, PATCH LEVEL and the sends) is in range X in TWOCORE builds only: range Z had no room for it with the 4,096 B bench pool tail.
# It is called from Z (part_mix) and calls no function at all (CONFIRMED in the disassembly; the build's ram_place / call-reach checks decide); no flash code calls it.
# hunt1: spread_run (fx.c: SAT's blend ramp around the hot mix_spread, and the notes spread_post takes) is there too, for the same reason: part 2's chain calls it on core 1. It calls mix_spread (the hot block)
# and track_dist_ramp (range Z); spread_post, which mixes SPREAD's gains in a line, runs on core 0 in the flash (mix_gather).
# hunt3: mod_tsrc and mod_part_put (296 B; the matrix's source value and a part's put of its values: called by mod_begin / mod_depth in range Z only, they call nothing) moved from range Z
# to X in TWOCORE builds: the click fixes (INSERT ramp, FM6 start ramp) grew range Z, and the pool tail (4,096 B rule) has only 8 B to spare. X has the room (316 B).
X_FROM_Z = ["mod_tsrc", "mod_part_put"]
if os.environ.get("DUELL_TWOCORE") == "1":
    NAMES = NAMES + ["mix_ramp", "spread_run", "f2_go", "f2_svf", "f2_comb", "f2_comp", "f2_drv", "sub_pass"] + X_FROM_Z   # (final line: hunt1's spread_run, hunt3's two moves from Z, cloud jobs E and F's functions)
ELF_NAMES = NAMES + (C1_NAMES if os.environ.get("DUELL_TWOCORE") == "1" else [])
LO, HI = 0x01C00000, 0x01C07F00                 # the range (app_ram.ld RAMX)
