# SPDX-License-Identifier: GPL-3.0-only
"""DUELL_RAMPLACE (two-core stage 1a, developer builds only): the code of range Z, the spare end of the memory pool.

Z starts after .pool (app_ram.ld: .ramz follows .pool in the POOL region; 0x01C6CE6C in cloud/base 85dee78) and must end below the
stack guard at 0x01C74000 (28.4 KB in that build). From there a direct call reaches the whole flash image and the flash reaches it,
and X (tools/ram_fns_x.py) and the hot block reach it too: so Z takes what the X engines call, and the rest of part 2's voice path
that is not yet hot (tools/hot_fns.py groups 1..3 stay where they are; the hot block keeps its headroom).

The list: the part_voice stage's call graph (fx.c, with the engines' render / block / done / amp entries) minus the hot set and range X,
plus the morph window around a part (morph_begin, its blend) and the matrix's mod_end. Measured on cloud/base 85dee78 (-Os, device bytes):
  FM6 (what is not hot)      fm6_sync 678, fm6_block 250, morph_fm6 240, fm6_done 230, fm6_env_advance 170, morph_fm_v 130,
                             atg_fm6 110, fm6_penv_advance 84, fm_byte 56, morph_stale 46                              1,994 B
  WAVE (what is not hot)     wv_speed 306, wave_block 190, wv_dmix 50                                                     546
  WHEEL                      wheel_render 872, wheel_block 694, drw_tilt 144, drw_level 68                             1,778
  voice loop and LFOs        part_voice 2,442, lfo_tick 274, uf_edge 160, esync_one 146, atg_voice 130, lfo_wave_of 110,
                             esync_coefs 106, atgt_bits 98, lfo_sync_inc 58, esync_coef 24, esync2_coef 24, spread_sum 24  3,596
  mod matrix                 mod_begin 1,102, mod_tsrc 196, mod_depth 114, mod_part_put 104, mod_gain 84, mod_end 42      1,642
  morph                      morph_apply 948, morph_begin 774, morph_dip 40                                               1,762
  helpers                    __udivdi3 262, memcpy 172, div_divlu 144, memset 126, div_samples 72, beat_samples 68,
                             part_rand 60, div_nlz32 58; PHYS's phys_slot_of 80, phys_reset 62                          1,104
                                                                                                                    about 12.4 KB
memset and memcpy are called from everywhere (the UI too): main.c fm1_cstart copies X and Z right after the boot guard, before anything
that could call them (tools/elfcheck.py check_ramplace follows every call before the copy).
"""
import os

GROUPS = {
    "FM6": ["fm6_sync", "fm6_block", "morph_fm6", "fm6_done", "fm6_env_advance", "morph_fm_v", "atg_fm6",
            "fm6_penv_advance", "fm_byte", "morph_stale"],
    "WAVE": ["wv_speed", "wave_block", "wv_dmix"],
    "WHEEL": ["wheel_render", "wheel_block", "drw_tilt", "drw_level"],
    "VOICE LOOP": ["part_voice", "lfo_tick", "uf_edge", "esync_one", "atg_voice", "lfo_wave_of", "esync_coefs", "atgt_bits",
                   "lfo_sync_inc", "esync_coef", "esync2_coef", "spread_sum"],
    "MOD": ["mod_begin", "mod_tsrc", "mod_depth", "mod_part_put", "mod_gain", "mod_end"],
    "MORPH": ["morph_apply", "morph_begin", "morph_dip"],
    "HELPERS": ["__udivdi3", "memcpy", "div_divlu", "memset", "div_samples", "beat_samples", "part_rand", "div_nlz32",
                "phys_slot_of", "phys_reset"],
}
NAMES = [f for fs in GROUPS.values() for f in fs]
# DUELL_TWOCORE=1 (two-core stage 1, the merge of cloud/job5 with this line): what core 1 runs beyond the list above. The real call graph of the job (tc_job_run,
# fm1_c1_main: the relocations of the compiled object, tools/ram_reach.py, docs/TWOCORE_REACH_REAL.md) reaches 153 functions; 109 are placed already (hot 22, X 39, Z 48), 44 are not:
# 16 of them only hang on the engine table's pointers (note_on, desc: core 0's events and the UI call them, the job never does) and stay in the flash; .c1_text goes to
# range X (tools/ram_fns_x.py C1_NAMES); the other 28 are here: part_fx / part_mix / part_rest and what they call (EQ, inserts, ducking, sends, the mutes), and
# core 1's two helpers. Only with the flag: the RAM1 build (RAMPLACE alone) keeps its 87 functions byte for byte. Device bytes of that build: 7,270 B (join0233, with T0229's track_dist_ramp, and mix_ramp in range X; the list was re-made with tools/ram_reach.py on the joined line; join0233-shrink: range Z 20,912 B in the bench two-core build).
TWOCORE_GROUPS = {
    "PART CHAIN": ["part_fx", "part_mix", "part_rest", "pfx_mix", "pfx_mute", "pfx_snap", "pfx_restart", "acc_prep", "part_lvl", "pan_follow", "dist_follow",
                   "space_tap", "duck_part", "perf_mute", "perf_mute_sd", "perf_mute_rest",
                   "track_dist_ramp", "mx_slew"],   # (join0233: T0229's matrix round: SAT's blend ramp, which part 2's job calls (mix_ramp is in range X, tools/ram_fns_x.py); mx_slew, eq_taylor and eq_q28 are out-of-line helpers made in join0233-shrink so that mod_begin, mod_depth and eq_design stay small; mod_param is inlined into mod_begin and is no longer a function)
    "EQ AND INSERTS": ["track_eq", "eq_design", "eq_taylor", "eq_q28", "eq_hz_q8", "eq_rest", "ins_lofi", "ins_drive", "ins_isqrt", "ins_rest", "__divdi3"],
    "CORE 1": ["tc_c1_init", "retired_render", "vs_render_set_c1", "voice_render"],   # (B2: tc_sh_hash, the share job's check word, vs_render_set_c1, core 1's share loop, and voice_render, the ONE copy of the render step for every caller (2026-11-04) (range Z: .c1_text in range X cannot reach flash, and X may not call flash callees)); calls from .c1_text
    # cloud wave 1, job B: what core 1 now reaches in flash that did not exist before (tools/ram_reach.py on the merged base: 1,430 device bytes): the voice chain's sequence
    # (chain_block and what it calls: they run only once a feature job makes chain_on true, but part_voice calls them), the matrix's CYCLE routes (mod_cycle_voice) and mod_begin's
    # descriptor lookup for a per-block destination (mod_pd, track_desc, xdesc: this one is on an existing path: any matrix route to a per-block destination on part 2)
    "VOICE CHAIN (job B)": ["chain_block", "chain_f1", "chain_stages", "chain_raw", "mod_cycle_voice", "mod_pd", "track_desc", "xdesc"],
    # cloud wave 2, jobs E and F (tools/ram_reach.py on the merged cloud/base3-wave3, 7,668 device bytes): TWIN's filter 2 (chain_f2 and the filter types, DRIVE, the plan, the cutoff sum, mod_f2_voice),
    # the SUB layer (chain_sub, sub_pass, chain_sub_on, fm6_sub_follow), the CYCLE envelope (cycle_tick, cycle_value) and WAVE STEPS (wave_steps), chain_voice (no longer folded into chain_block)
    "TWIN, SUB, CYCLE, STEPS (jobs E, F)": ["chain_f2", "chain_f2_pre", "chain_f2_on", "f2_lad", "f2_plan", "f2_cut", "mod_f2_voice",
                   "chain_sub", "chain_sub_on", "fm6_sub_follow", "cycle_tick", "cycle_value", "wave_steps", "chain_voice"],
    # THE VOICE SPLIT and the one-block step (owner 2026-11-05; tools/ram_reach.py on BENCHVB's object, 1,588 device bytes): core 1's job is now its sets of both parts' voices and BOTH parts' own stages
    # (twocore_run.c tc_job_run: tc_blk_hash, tc_set_run, vs_merge, part_stages), so what those call in the flash comes here: voice_render's re-strike step (voice_step), TWIN's bypass mix (f2_bypmix,
    # f2_bypsave) and part_fx's helpers (plain_fx, spread_fx, spread_duck, the PART FX bypass fxb_*). The control pass (mod_begin, morph_begin, the LFOs, the engines' block entries) runs on core 0 only
    # now: 24 functions of this range (6,446 B) are no longer core 1's. Three of them, MORPH's (1,976 B, no caller in range X or the hot block), go back to the flash to make the room (VOICE_SPLIT_OUT)
    "VOICE SPLIT (2026-11-05)": ["part_stages", "tc_blk_hash", "tc_set_run", "vs_merge", "voice_step", "f2_bypmix", "f2_bypsave",
                   "plain_fx", "spread_fx", "spread_duck", "fxb_pre", "fxb_sends", "fxb_mix", "fxb_done", "fxb_save"],
    # THE TWO-PHASE JOB (2026-11-07; twocore_run.c tc_job_run): core 1 checks the snapshot's hash (tc_ctx_hash) and runs the globals of the block before (fx.c mix_globals: the sends, the meters, the
    # master) before its sets. Both were flash: core 1's loop (.c1_text, range X) cannot reach the flash, so they come here (Z reaches the flash code they call)
    "TWO-PHASE JOB (2026-11-07)": ["tc_ctx_hash", "mix_globals"],
    # THE NOTE PATH (2026-11-08, the WAVE crackle; owner yes to cutting the pool tail floor, rule G10, to make the room): core 0's MIDI drain, note start and note end ran from the flash, and
    # the first note of a chord found them out of the cache: 89..201 us for one note start (22..50 warm), the drain up to 406 us, in the first block of a half already 92 % full. In RAM they cost the same every time
    "NOTE PATH (2026-11-08)": ["midi_in_take", "midi_event", "midi_note_event", "input_on", "lay_input", "midi_note_held", "midi_local_held", "midi_track",
                   "trk_note_on_1", "trk_note_on_now", "voice_start", "cycle_start", "voice_trim_now", "trk_nvoice_eng", "voice_pool_room", "trk_nvoice",
                   "vp_now", "vp_sounding", "voice_oldest", "trk_note_off_1", "vs_block", "wave_note_on"],
    # THE EVENT STEP (2026-11-10, owner yes; room: the screen canvas 240 x 104 on the unit, gfx.c CV_BANDS, 9,600 B): the knob test's last missed buffer (pool 7000, bench B1110A) spent 444 us of
    # its first block in the event step, 288 us of it in the MIDI step, both from the flash: events_block itself (the transport, the parts' arp and hold, the sequencer, the arp, the clock) and
    # what it runs every block (strum_block, the keyboard's sense_gate, velo_at_block, duck_pump), and a CC's path (midi_map_cc to the parameter, the MORPH lines, the global targets, the matrix's MIDI sources)
    "EVENT STEP (2026-11-10)": ["events_block", "strum_block", "sense_gate", "velo_at_block", "duck_pump", "mod_midi",
                   "midi_map_cc", "map_line", "midi_cc_set", "param_desc_of", "motion_capture", "midi_morph", "morph_push", "midi_glob_act",
                   "midi_map_global", "midi_in_block", "mo_flush", "midi_out_event", "midi_expression_channel", "midi_release", "input_off"],   # (the last two: every MIDI note-off)
    # THE BUTTON SCAN (2026-11-10, owner: "b"; room: the screen canvas 240 x 96 on the unit, tools/build.py CV_ROWS, 3,840 B more): TIMER5's tick runs nested in the audio render
    # 10,000 times a second; from the flash it cost ~7.3 us there, so 2026-11-08 skipped every second tick in the render and the LEDs flickered with the load (main.c). Every
    # tick again, its code from RAM: the scan itself and the key sensing it calls (fm1__anim_frame, the power-on sweep only, stays in the flash; fm1_timer5_irq stays: elfcheck)
    "BUTTON SCAN (2026-11-10)": ["fm1_input_tick", "fm1__keys_frame", "fm1__sr_bits", "sense_update", "sense_want_drive", "sk_driven", "sk_finish", "sk_range",
                   "sk_land", "sk_publish", "sk_final_depth", "sc_pow12", "sc_log2q8", "sc_ev_push"],
}
VOICE_SPLIT_OUT = ("fm6_sync", "morph_stale", "fm_byte")   # (2026-11-08, the 1.2 test build: range Z was 668 B short of the pool tail floor, rule G10; these three run on core 0 only, in the control pass or the main loop, FM6 / MORPH set-up work, not per sample: back to the flash, about 780 B)
# (BENCHVB to BENCHVD had MORPH's three out of the range, "to make the room": the room was never short (9,376 B free), and the control pass calls morph_begin for both parts in every block and at every note start, on core 0. Back in the range since 2026-11-05)
TWOCORE = os.environ.get("DUELL_TWOCORE") == "1"
if TWOCORE:
    NAMES = NAMES + [f for fs in TWOCORE_GROUPS.values() for f in fs]
    NAMES = [f for f in NAMES if f not in ("mod_tsrc", "mod_part_put", "ins_lofi", "ins_drive", "part_voice") + VOICE_SPLIT_OUT]   # hunt3: mod_tsrc, mod_part_put, and B2 (2026-11-04): ins_lofi, ins_drive are in range X in TWOCORE builds (ram_fns_x.py X_FROM_Z)
# (2026-11-07, owner: no LED blackout at a save) the erase's LED steps (hal/fm1_flash.h fl_erase4k_led, fl_led_col): they run while the flash is off,
# so they must be in RAM in every RAMPLACE build; the hot block has no room for them (its 228 B floor, rule G10) and its driver makes no calls
NAMES = NAMES + ["fl_erase4k_led", "fl_led_col"]
HI = 0x01C74000                                 # the stack guard (app_ram.ld _guard0): Z ends below it
