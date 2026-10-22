# MIXSPLIT: mix_block as separate stages (job 1 of the second-core work)

Branch `cloud/mixsplit`. Code: `felucca-base/firmware/src/fx.c` (stages), `felucca-base/tests/mixsplit_test.c` and `tests/mixsplit_ref.h` (the test and the old path it compares with). No arithmetic was changed: every stage body is the code that was inline in `mix_block` / `mix_part`, in the same order. Nothing here starts a second core; it only makes the cut lines visible and testable.

## 1. The stages

Order of one block (`mix_block`, called from the audio interrupt, audio.c `audio_block`):

| Stage (function, all noinline) | What it does | Reads | Writes |
|---|---|---|---|
| `mix_events(n)` | EVENTS: `events_block` (MIDI in, notes, sequencer, arp, clocks, PUMP beat count), `dm_step` (MODE change dip), `snd_step` (MODULES SENDS) | MIDI/USB queues, transport, `song.*`, both parts' voices (`v->active`, for the 8-voice budget) | voices' note state, parts' `t->p`, `duck_st[].ph/run/pos`, `dm_*`, `snd_st`, `mod_dk`. No audio buffer |
| `part_voice(m, t, n)` | VOICES of one part into `part_buf` (after `mod_begin`); decides sounding / silent | `t` (voices, parameters, LFOs, envelopes), the matrix and MORPH values already in `t->p`, `dm_split`, `song.g` (tune), MIDI bend | `part_buf`, `t`'s voices/LFOs/envelopes/tail, `mod` (window), `spread_on`, `spread_buf`, `fm6_bus`, `eng_mem[pp]`, `vage`/`voice_kills`, shared RNGs (see table) |
| `part_rest(m)` | the silent way out of a part: effect states at rest, pan and SAT taken at once, `mod_end` | `t`, the other part's DUCK source, `mods_off`, `pf.mute` | `eq`/`ins`/PART FX/DUCK state of the part, `pan_x/dist_x/dist_live/pan_gv[pp]`, `pf.mg[pp]`, `duck_st[pp]` (incl. its key) |
| `part_fx(m)` | PART EFFECT chain on `part_buf` in place: PAN/SAT follow, DUCK key, SAT (`track_dist`), INSERT, EQ, PERFORM MUTE, DUCK, PART FX crossfade; with SPREAD it is `mix_spread`, which also mixes | `t` parameters, `mods_off`, `pf.mute`, other part's `duck_st[].key` and `P_DKSRC` | `part_buf`, the part's INSERT/EQ/SAT/DUCK/pan states, `pfx_x[pp]`, `pfx_dry/pfx_dryd`, `spread_buf`, `t->p[P_DIST]` (this block's SAT), `duck_st[pp].key`; with SPREAD also `mix_l/mix_r/send_*/sp_f*`, `t->peak` |
| `part_mix(m)` | LEVEL, BALANCE, PAN: the part's block added into `mix_l/mix_r` and the sends `send_c/d/r`; SPACE floors; peak | `part_buf`, `t` (LEVEL, TRIM, sends, peak), `mix_bal`, `snd_st`, `dm_split`, `sp_tap`, `pan_g/pan_gv[pp]` | `mix_l`, `mix_r`, `send_c/d/r`, `sp_fc/fd/fr` (all with `+=`), `pan_g/pan_gv[pp]`, `t->peak` |
| `part_end(m)` (inline) | the stored SAT and matrix values back (`mod_end`) | `m` | `t->p` |
| `mix_sends(n)` | SENDS: bus matrix offsets, SPACE, the chorus/delay/reverb buses (A and B in DUEL), side returns, SENDS fade | `send_*`, `sp_f*`, `sa_*`, `song.g`, `sv`, `snd_st`, `dm_*`, both parts' `mod_goff` | `wet`, `mix_l/mix_r` (HALL / PING-PONG side), `snd_dl/dr`, bus states (pool), `sv`, `mod_gkeep/mod_gw` |
| `mix_master(out,n,perf,mg)` | MASTER: PERFORM master, SPACE width, MODE dip, WIDTH, BASS MONO, COMP, level, DC blocker, limiter, soft clip (`master_out`), or `split_master` in DUEL | `mix_l`, `mix_r`, `wet`, `song.*`, `pf`, `sv`, `dm_*` | `mix_l/mix_r/wet`, the stages' own states (`bm`, `wdn`, `cmp`, limiter and DC states), `out` |

`mix_part(t, n)` stays a tiny orchestrator (the one place that fixes a part's stage order); `mix_block` keeps the accumulator clear, the frame setup (`sp_tap`, PERFORM begin), the morph window around each part, `split_stash` after part 1 in DUEL, and the meter/scope taps. The per-block context of one part is `mixpart_t` (t, b = `part_buf`, n, pp, pan_st, dist_st, pan_mv, pq, fxm, spread), passed by pointer from stage to stage. State that lives across blocks stays where it was (`track_t` and the per-part arrays indexed by `pp`); each stage's comment in fx.c lists what it reads and writes.

`events_block` (seq.c) is now declared noinline (fx.c, forward declaration) so it is a function of its own and out of `mix_block`'s body.

## 2. Shared state (what stops the two parts from running at once)

"Part-private" state is not listed (it is indexed by the part and only that part's stages touch it: `trk[pp]` voices and parameters, `eng_mem[pp]`, `pan_x/pan_g/pan_gv/dist_x/dist_live/pfx_x[pp]`, `ins[pp]`, `eqs[pp]`, `pf.mg[pp]`, `duck_st[pp]` except its key, `mod_goff[pp]`, `mph[pp]`, `fm6_lfo[pp]`, `wv_gen[pp]`, `midi_bend_q8[pp]`).

| Shared item | Who reads | Who writes | Proposed fix for two cores |
|---|---|---|---|
| `duck_st[o].key` (DUCK SRC PART: each part's level follower, read by the other part's `duck_part`) | `part_fx` and `part_rest` of the other part; today part 2 reads part 1's key of THIS block, part 1 reads part 2's of the block before | `duck_part` of the keying part | One 32-bit word per part, published by the keying part after its voice stage. To stay bit for bit: part 2's effect stage waits for part 1's key (one flag per block). Else a fixed one-block latency for both parts (changes part 2's duck slightly, a new golden for SRC PART only) |
| `part_buf` (the part's block, single scratch) | `part_fx`, `part_mix`, `duck_part` | `part_voice`, `part_fx` | One buffer per part (`part_buf[NLIVE]`), carried in `mixpart_t.b` already |
| `mix_l`, `mix_r`, `send_c/d/r`, `sp_fc/fd/fr` accumulators (`+=` by both parts, cleared by `mix_block`; `mix_spread` also adds) | `mix_sends`, `mix_master`, `split_stash`, `space_tap` users | `part_mix`, `mix_spread`, `split_stash` (moves part 1's sends to `sa_*`) | Per-part accumulators (or one set per core); the sum at the start of `mix_sends` is plain 32-bit adds, so the result is bit for bit whatever the order. DUEL then needs no `split_stash` (part 1's set is bus A's) |
| `spread_on`, `spread_buf` (voice.c: set by `part_voice`, read by `track_render`, `mix_spread`) | `track_render`, voice engines, `mix_spread` | `part_voice` | The flag is already copied to `mixpart_t.spread`; make `track_render` take it as an argument (it still reads the global); `spread_buf[NLIVE]` |
| `pfx_dry`, `pfx_dryd` (PART FX crossfade scratch) | `pfx_mix`, `pfx_mute` | `part_fx`, `mix_spread` | One pair per part |
| `mod` (the matrix's window, "one part at a time"), `mw` (morph window), `mxf` (morph crossfade), `mod_goff` writer bits `mod_gon`, `mod_dk` (one byte, bit per part) | voices, engines, `mod_end`, `morph_end`, `mix_sends` (`mod_bus`), event code (`mod_dk`) | `mod_begin`, `morph_begin`, `mod_part_put` | Instances per part (`mod[NLIVE]`, `mw[NLIVE]`, `mxf[NLIVE]`); `mod_gon`/`mod_dk` one byte per part (today a read-modify-write of one byte by both parts). The morph's queue `mq_*` stays event-side |
| `fm6_bus[2][FM6_N]` (FM6 operator buses, scratch) | `fm6_render` of any FM6 part | same | Per part (or per core) |
| `formant_nz` (FORMANT breath noise, "one xorshift for all voices"), `voice.c` UNISON start-phase `seed` (a static in the note-on code, voice.c), `mod_seed`, `rng_state` | FORMANT render (`formant_nz`: render stage); the others: note-on and sequencer code | same | `formant_nz` is read AND written in the voice stage by both parts: a per-part seed (the sound of two FORMANT parts changes; one part is unchanged when the seed stays). The others are event-side only: one owner, no change |
| `vage`, `voice_kills`, the 8-voice budget (`voices_busy`, `voice_room`, `voice_victim` look at BOTH parts' `v->active`) | event stage (note-on, steal), `track_render` (ends a voice: `v->active = 0`) | note-on (`vage`, kills); `track_render` (`v->active`, envelope end) | The event stage must see a settled `v->active` of both parts: run it between blocks (as now), or hand the budget a per-block snapshot. `voice_kills` per part |
| `eq_live` (one byte, bit per part) | `part_rest`, `track_eq` | `track_eq`, `eq_rest` | One byte per part |
| `mods_off`, `snd_st`, `snd_panic`, `dm_cur/dm_split/dm_hold/dm_dir/dm_g`, `sp_tap`, `mix_bal`, `perf_space`, `pf.mute`, `pf.act` | all stages | `mix_events` (and `mix_block`'s frame setup), `perf_begin`; the UI/main loop | Read-only during the render phase: publish a per-block snapshot at the barrier after the event stage (already the order today). `volatile` UI-written values (`mods_off`, `perf_*`) must be read once a block into the snapshot, not mid-render |
| `sv` (SPACE state: `sv.sg/ac/ad/ar/spc`), `bm`, `wdn`, `cmp`, limiter/DC states | `space_tap` (`sv.*` read by both parts in `part_mix`), master stages | `mix_sends` (`space_step`), the master stages | Parts read the values of the block before (written after the parts); keep that order (the barrier between the parts and `mix_sends`). Master stage states stay with the master core |
| `wet`, `sa_*`, `snd_dl/dr`, `dly_sd`, `lo_buf`, `dly_buf` (the two delays share it, half each), the bus states `fx`, `fx_b`, `hl`, `hl_b` | `mix_sends`, `mix_master` | `mix_sends` | Only the send stage touches them. If bus A and bus B ever run at once (DUEL on two cores): separate `lo_buf`/`dly_sd`/`wet` (today A and B "run one after the other" in one scratch) |
| `part_lvl`, `bal_lvl` reading `mix_bal` and `song.*` | `part_mix`, `mix_spread`, `duck_part` | UI | Read-only in the render phase |
| `t->peak` (GLO meters) | UI main loop | `part_mix`, `mix_spread` | Single writer per part; the UI read is a plain 32-bit load |
| `meter_bus`, `meter_on`, `scope_buf`, `scope_mix` | UI | `mix_block` taps (after `mix_sends`) | Master core only |

Not checked line by line (a first pass for the owner's audit): the other engines' file-scope statics (`eng_*.c`, `phys_*.c`, `wv_*`, `eng_wheel.c` wheels: per part by `NLIVE` arrays as far as read), `perform.c` effect buffers (master side, after the parts), and the DUELL `morph_apply` scratch (`mw.tid`, `mw.chk`, `mw.tlive`: part of `mw`).

## 3. Sizes and call depth (estimates, not the device)

The device toolchain (JieLi clang 4) is not available here. The figures below are the host compiler's (Apple clang, arm64, `-Os`, noinline honoured) on the `regress.c` unity build; the host test build itself defines `__attribute__(x)` away (tests/hostsim.c line 21), so noinline is a no-op in the host tests. Scale to the device with the ratio of the old `mix_block` (host 12,320 B, device about 9.4 KB, factor about 0.76); the old split of the task text (voices 2.4 KB, master 0.7 KB, events 3.6 KB) matches the scaled host numbers.

| Function | Host arm64 -Os | Device estimate (x 0.76) |
|---|---|---|
| `mix_block` (old, everything inline) | 12,320 B | about 9.4 KB |
| `mix_block` (now) | 692 B | about 0.5 KB |
| `part_voice` | 3,172 B | about 2.4 KB |
| `part_rest` | 388 B | about 0.3 KB |
| `part_fx` | 800 B | about 0.6 KB |
| `part_mix` | 1,016 B | about 0.8 KB |
| `mix_sends` | 1,148 B | about 0.9 KB |
| `mix_master` | 852 B | about 0.65 KB |
| `mix_events` | 120 B | about 0.1 KB |
| `events_block` (own function now) | 4,904 B | about 3.7 KB |

Sum now 13,092 B against 12,320 B: about +6 % of code (about +0.6 KB on the device: prologues, the `mixpart_t` hand-over). This is flash code: `mix_block` was NOT in the hot RAM set (tools/hot_fns.py lists no `mix_block`; its own header says "7.9 KB, mostly cold paths: too big"), so tools/hot_fns.py is unchanged and the hot RAM budget (12,512 B, 244 B free) is not affected by this change. The new functions run from the flash through the cache like `mix_block` did. Owner's Mac build: check the linked sizes, the elfcheck, and the audio load on the unit (the bench), because the flash/cache layout moved.

Call levels: audio_block -> mix_block -> one stage (`part_voice`/`part_fx`/`part_mix`/`mix_sends`/`mix_master`) -> the existing callees. One level more than before for the part code (`mix_part` itself is a small static function the compiler is free to inline into `mix_block`); the deepest chain (`part_voice` -> `track_render` -> engine -> helpers) is one frame deeper than inline, and `mix_block`'s own frame is smaller (it no longer holds every stage's locals), so the peak stack should not grow by much; check the stack margin on the device.

## 4. Test

`./host_tests.sh mixsplit` (also in the default list). Five scenes (STEREO with SPREAD/matrix PAN/INSERT/HALL; DUEL with EQ/SAT/PUMP/BALANCE; STEREO with DUCK SRC PART, PAN step, BASS MONO/WIDTH/COMP and a silent tail; LAYER with MUTE, DRIVE, PART FX OFF, SENDS OFF, SPACE and a MODE change; DUEL with SPREAD, DUCK SRC PART, SPACE and a silent part), each rendered in its own process from power-on by: the old combined path (`mixsplit_ref.h`, verbatim copy of the pre-refactor `mix_part`/`mix_block`), the firmware's `mix_block`, and the stages called in sequence from the test. Every output sample (hash) and each part's peak must be equal. A coverage run of the three paths reaches every branch of the new stages except `ins_rest` (rare), the ramped-pan peak update, and the MORPH window (`morph_end`; the MORPH path is covered by the `mrender` and `morph` goldens through `mix_block`).

## 5. Host test status

All host tests pass as on main except two things. `editorjson` fails on this machine before and after the change (no emsdk node here). `bench` (report-only) fails on the INSERT rows only (+11 to +21 %, 10 to 15 host instructions a sample over tests/bench_baseline.txt; every other row within 10 %): the host `-O1` build lets the host compiler inline differently now that `mix_part` is cut into functions (hostsim.c defines `__attribute__` away, so `noinline` does nothing there). With `-fno-inline` the INSERT, SAT and EQ rows of the old and the new code are equal (INSERT SOFT 114.0 old, 113.3 new; TAPE 141.9 and 137.4), so the stage cut itself adds no per-sample work. The baseline file was NOT regenerated: the device cost of the INSERT chain is for the owner's unit bench to settle. `comp_test.c` and `master_test.c` check the order of the master stages in the source text; they now look in `mix_master` instead of `mix_block` (same assertions).

## 6. Job 2 (partstate, branch `cloud/partstate`): the two parts' stages made independent

Goal: the per-part stages (`part_voice`, `part_rest` / `part_fx`, `part_mix` + `part_end`, with `morph_begin` / `morph_end` around them) can run in any order, or interleaved stage by stage, with the same output bit for bit. No arithmetic changed; no golden or hash changed. Nothing starts a second core.

### 6.1 What moved where (as first built; 6.5 has the layout after the cost clean-up, which differs in the places it names)

| Step | Before (shared) | Now (per part) | Where |
|---|---|---|---|
| 1 | `part_buf`, `pfx_dry`, `pfx_dryd`, `spread_on`, `spread_buf`, `fm6_bus` | `part_scr_t pscr[NLIVE]` fields `buf`, `dry`, `dryd`, `sbuf`, `spread_on`, `fm6` | core.h; `track_render` reads `pscr[PART_OF(t)]` (flag and buffer, local copies), `spread_clear(sbuf)` / `spread_sum(out, sbuf)`; `fm6_note_compute` / `fm6_core_render` get the part's buses as a parameter; `pfx_mix`, `pfx_mute`, `mix_spread`, `part_fx` use `pscr[pp]`. `fx_bus.c rev_switch` borrows `pscr[0].buf` after the parts (send stage) |
| 2 | `mix_l`, `mix_r`, `send_c/d/r`, `sp_fc/fd/fr` (`+=` by both parts, cleared in `mix_block`, `split_stash` after part 1 in DUEL) | `part_acc_t pacc[NLIVE]` (`l r sc sd sr fc fd fr`), cleared by `part_voice` (`pacc_clear`, floors only with `sp_tap`), added into by `part_mix`, `mix_spread`, `space_tap`; new stage `mix_gather(n, bs)` after the last part sums them in the fixed order 0, 1 into the globals (DUEL: part 1 to `sa_*` / `sp_f*_a`, the others to `send_*` / `sp_f*`, which is what `split_stash` did, so `split_stash` left the firmware) | fx.c |
| 3 | `mod` (matrix window), `mw` (morph window), `mxf` (morph cross-fade); `mod_gon`, `mod_dk`, `eq_live` (one byte, a bit per part) | `modw[NLIVE]` (`MODW(t)`), `mw[NLIVE]`, `mxf[NLIVE]` (`MXF(t)`); `mod_gon[NLIVE]`, `mod_dk[NLIVE]`, `eq_live[NLIVE]` (a byte per part; `mod_gon_any()` for the send stage) | mod.c, morph.c (`morph_begin`, `morph_end`), core.h, voice.c (`track_render`, `lfo_tick`, `uf_run` gets the cross-fade as a parameter via `vmod_t.mx`), eng_analog / lofi / phase / trio / wheel (`MXF(t)`), fx.c |
| 4 | `dm_*`, `snd_st`, `mods_off`, `perf_*`, `pf.mute`, `mix_bal`, `sp_tap`, `perf_held` / `perf_kill` (mute goal), the voice budget, read by each part's stages | `blockstate_t` filled once by `blk_snap` in `mix_block` (after `mix_events` and PERFORM's begin, before the first part); the stages get it by pointer (`mixpart_t.bs`) and only read it: `part_voice(m, t, n, bs)`, `pfx_mode / pfx_snap / pfx_mix(.., bs)`, `bal_lvl(t, lvl, bal)`, `perf_mute / perf_mute_sd / perf_mute_rest(.., goal)`, `pfx_mute(.., goal)`, `mix_gather(n, bs)` | fx.c, perform.c |

`mix_sends` and `mix_master` still read the live variables (nothing writes them between the parts and those stages). `voices_busy` is in the snapshot (the budget at the snapshot) but no part stage reads it: the budget is used by note-on and voice stealing in the event stage, which must ask `voices_busy()` / `voice_room()` live (a note-on changes it inside the block). `blk_snap` calls `voices_busy()` once a block (32 voice checks); drop the field if it stays unused.

### 6.2 Job 3: shared state that is still there (marked `/* TWO-CORE BLOCKER, job 3 */` in the code)

| # | What | Where | Why it was left |
|---|---|---|---|
| a | DUCK SRC PART: part 2 reads part 1's key of THIS block, part 1 reads part 2's of the block before | fx.c `duck_part` (`kv = ok->key`) and its callers in `part_fx` / `part_rest` | changes audio to fix (a fixed one-block latency for both, or part 2 waits for part 1's key) |
| b | `formant_nz`, the FORMANT breath noise, one xorshift for both parts, read and written in the voice stage | eng_formant.c `formant_render` | a per-part generator changes the VOICE engine's FORMANT output |
| c | **new, found by the test**: the shared generator `rng_state` (libc.c `rng()`) is drawn in the voice stage: the S&H value of LFO 1 of parts 1 and 2 on every LFO wrap (voice.c `lfo_rand`, called from `lfo_tick`), and PHYS's model reset under a sounding note (eng_phys.c `phys_render`). Two LFOs that wrap in the same block draw in the order the parts run | voice.c `lfo_rand`, eng_phys.c | a per-part generator changes the S&H values of parts 1 and 2 and every note-on seed after them (the shared sequence). Checked: with `LFO_SHARED_RNG 0` (own xorshift for every part) all schedules of the test agree, only the old hash differs. Part 3 / 4 (hidden) already have their own |

Read-only during the parts and not snapshotted: `song.*`, `sv.*` (SPACE state: written by `mix_sends` after the parts), `tune_key`, `midi_bend_q8[]`, `duck_st[o].ph/run/pos` (event stage), the other part's parameters (`trk[o].p[P_DKSRC]`). Event-side only: `vage`, `voice_kills`, `mod_seed`, the UNISON `seed`, `mq_*`.

### 6.3 RAM and code

`.data + .bss` (device sizes from the declarations; the host test prints the same struct sizes), NLIVE = 2:

| | bytes |
|---|---|
| removed: `part_buf` 128, `pfx_dry` 128, `pfx_dryd` 128, `spread_buf` 128, `spread_on` 1, `fm6_bus` 256, `mod` 84, `mw` 516, `mxf` 38, `mod_gon` 1, `mod_dk` 1, `eq_live` 1 | 1,410 |
| added: `pscr` 2 x 772 = 1,544, `pacc` 2 x 1,024 = 2,048, `modw` 2 x 84 = 168, `mw` 2 x 516 = 1,032, `mxf` 2 x 38 = 76, `mod_gon` / `mod_dk` / `eq_live` 3 x 2 = 6 | 4,874 |
| **increase** | **3,464** (51,248 -> about 54,712 of 73,728) |

Stack: `blockstate_t` (36 B) is a local of `mix_block`. `vmod_t` grew by 4 B (a pointer; one per voice on the stack of `track_render`). Time: `mix_block` no longer clears five global arrays; the parts clear their own (`pacc_clear`) and `mix_gather` sums: about 640 more word accesses a block than before (about 1,100 with SPACE's floors), an estimate to check with the bench.

Hot set (tools/hot_fns.py, NOT edited): touched by steps 1 to 4: `mix_spread` (its arguments, the accumulators and the snapshot through `m`), `uf_run` (new parameter `mx` for the cross-fade), `uf_voice` (passes `m->mx`), `fm6_render` (passes the part's buses), `fm6_note_compute` (new parameter, `fm6_core_render` is inlined into it on the device), `spread_clear` (buffer as a parameter), `mod_voice` (the window as a parameter), `morph_end` (the part's `mw` / `mxf`). Untouched: the other 23 (`eq_run`, `track_dist`, `track_insert`, `duck_run`, `ins_swept`, the reverbs, `dly_mod`, `fx_buses_mod`, `wave_render`, `wv_*`, `fm6_op_*`, `fm6_carriers`, `fm6_note_of`, `uf_coef`, `sine_i`, `osc_sine`, `mod_voice_add`, `perf_flanger`, `perf_drive`). Host sizes (x86-64, `-Os -fno-inline`, only the direction is meaningful; the device is the owner's to measure, 200 B are free in the hot block): `mix_spread` +74, `uf_run` -9, `uf_voice` +11, `fm6_render` +3, `fm6_note_compute` +23 (with `fm6_core_render` -11), `spread_clear` -10, `mod_voice` -19, `morph_end` +47: about +109 B in all. If the hot block is over its limit, `morph_end` (47 B for the part index) and `mix_spread` are the two to look at first.

### 6.4 Test

`./host_tests.sh partindep` (also in the default list): tests/partindep_test.c with the scenes of tests/partindep_scn.h. Six passing scenes (STEREO, DUEL, LAYER; SPREAD on, EQ, SAT, PART FX crossfade, PAN step, MUTE, MODE change, silent parts, MORPH on both parts, the matrix's bus offsets of both parts; no DUCK SRC PART, no FORMANT), each from power-on in its own process, rendered with the stages in six schedules: A (part 1 whole, then part 2), B (part 2 whole, then part 1), C and D (stage by stage, 1 then 2 and 2 then 1) and E, F (mixed). All must equal the old combined path (tests/mixsplit_ref.h), `mix_block`, and the hashes the verbatim job 1 `mixsplit_ref.h` gave for the same scenes (commit d524422; `OLD_HASH` in the test). Scene 6 is the tripwire for blocker (c): A, C, E equal the old order, B, D, F must differ. Each scene also has to take the paths it names (MORPH on both parts, bus offsets of both parts, SPREAD, silent part, PART FX fade, MUTE, SPACE, EQ states). Self-check: the same section builds the test against a copy of the firmware with one shared write put back (the block scratch of both parts made one, one morph window, one matrix window) and requires the test to fail.

`tests/mixsplit_ref.h` had to follow the moved state; the changes are listed in its header (names of the moved scratch, the snapshot argument, mix_spread / space_tap adding into an accumulator that is added into the shared arrays at once); the arithmetic and the order are the old ones, pinned by `OLD_HASH`. `tests/mixsplit_test.c`'s staged path now ends with `mix_gather`; the other tests only changed where they read `mod`, `mw`, `mxf`, `mod_gon`, `mod_dk`, `eq_live` (`modw[0]`, `mw[0]`, `mxf[0]`, per-part bytes).

Host status: as in section 5. On this machine `morph` (a division trap in the SMART matrix roll at AMOUNT 100: floating point exception, on the unchanged branch too), `mode` (the SPLIT cost check), `sounds` (does not compile here), `editorjson` (no emsdk) and `stereochorus` also fail on `cloud/mixsplit` before any change of job 2.

### 6.5 Job 2b: the cost clean-up (same branch, later commits)

The owner's Mac check found the first build of section 6 too dear: four host cost checks failed (`perform` SPACE, `cpuscen` M06 4+4 with MORPH, `uf` KEY TRK, the `bench` idle row 527 against 455) and the hot RAM free fell from 200 B to 104 B. The cost checks and the bench count instructions with a macOS counter, so they report and pass on Linux. Here the same scenes were counted with valgrind (callgrind, `-O1 -fno-builtin` as the bench, 400 blocks of 32 samples, host instructions a sample, the work of `mix_block` and `wave_service`), cloud/mixsplit against this branch:

| scene | cloud/mixsplit | partstate (first build) | partstate now |
|---|---|---|---|
| idle (STEREO, nothing playing) | 1,040 | +90 | **+6** |
| one PHASE voice | 1,177 | +80 | **+2** |
| both parts ANALOG, 4 notes each, every send | 2,731 | not measured | **+8** |
| SPACE +100 on top of that (the feature's own cost) | +120.9 | not measured | **+118.2** (less than before the work) |
| KEY TRK on PHASE, 8 voices (KTRK 63 minus 0) | 2.31 | 2.31 | **1.73** |

(Those absolute figures are valgrind's, about 2.3 x the Mac's `bench` numbers because they include the whole `mix_block`; the differences are what matter.) Where the first build's +90 came from, idle: `mix_gather` summing five arrays of both parts (+45), `memset` of the parts' accumulators (+24), the voice-budget count in the snapshot (+10), and about +11 of per-part index arithmetic (`% NLIVE` on a pointer difference, a multiply by the window size) in `mod_begin`, `track_render`, `morph_end`, `mod_end`, `lfo_tick`.

What changed:
- Accumulators: a part ASSIGNS its block into `pacc[pp]` (no clear, no read-modify-write); `acc_prep` (flash, once a block, called from `part_fx` / `part_mix`, not from the hot `mix_spread`) records in `pacc_on[pp]` that the part wrote (bit 0) and which sends are not 0 (bits 1..3), and `space_tap` assigns the floors and sets bit 4. `mix_gather` sums only the parts that wrote: nothing sounding is the old clear loop, one or two parts are one fused loop each; SPLIT, more than two parts and mixed sends go through `gather_general`. The sums are the same integer adds.
- The matrix window (`mdw`), the MORPH cross-fade (`mxw`), the morph window's open flag and part index (`mwon`, `mwpi`) and the bytes `mgon`, `mdk`, `eqlv` are fields of `track_t` (+128 B a track, 8,256 -> 8,768 B, still a multiple of 16): a reader has them at a fixed offset from `t`, no index, no division. The morph blend's work arrays stay per part (`mwork[NLIVE]`).
- The FM6 operator buses are on `fm6_render`'s stack (`bus[3 * FM6_N]`, +256 B of stack there): `fm6_note_compute` and `fm6_core_render` have their old parameters, and no per-part FM6 scratch is left.
- The multimode filter's SLOPE cross-fade reaches `uf_run` through `voice_t.msl` (set by `track_render` per voice; the structure keeps its size): `uf_run`, `uf_voice`, `mod_voice` have their old parameters. `uf_voice` is not touched any more.
- `blk_snap` no longer counts the voice budget (nothing read it).

RAM (`.data + .bss`, device sizes) against cloud/mixsplit: `track_t` +512, `pscr` 1,032 (516 a part), `pacc` 2,048, `pacc_on` 2, `mwork` 1,016, minus the globals that went (1,410): **+3,200 B** (the first build: +3,464). Stack: +256 B in `fm6_render`'s frame, `blockstate_t` 36 B in `mix_block`.

Hot set, host sizes (x86-64 `-Os -fno-inline`, direction only): `uf_run` -15, `mix_spread` -19, `spread_clear` -10, `mod_voice` +4, `morph_end` +24 (the part index into `mph`), `fm6_core_render` -27 (inlined into `fm6_note_compute` on the device). Every other hot function is byte for byte the same size as on cloud/mixsplit, so the sum is below zero (about -43 B on the host, about -33 B with the 0.76 scale): the target of at most +16 B is met on this estimate; the owner's elfcheck decides.

`partindep` follows: its mutation checks now put one shared write back into the block scratch (`fx.c`), the morph window state (`morph.c`, `t->mwon` / `t->mwpi` of part 1 made part 0's) and the matrix window (`core.h` `MODW`); all three are still detected.

## 7. Job 3 (branch `cloud/partstate2`): the blockers closed, and the two-core hand-over simulated on host threads

### 7.1 One-sided ducking (blocker a)

The rule (the owner's): DUCK SRC = PART may be on ONE part only. The part with PART is ducked by the other part's level; the other part has NO ducker: its SRC is forced to OFF (3) and its DUCK cards are not drawn.
- params.c: `duck_set_part(p)` (part p's SRC was just set: knob `ui_input.c`, editor SET `editor.c`, a patch load `upreset.c`, UNDO `ui.c`; if it is PART the other part goes OFF, the part set last wins), `duck_src_norm()` (a pair loaded together, a multi, a project: the lower index keeps PART, the other is OFF; also called at the head of every block, `fx.c mix_events`, so a pair that broke the rule through a path not listed is mended before the parts run), `duck_forced(t)` (the other part has PART).
- MIDI CC cannot set SRC (`midi_control.c midi_cc_set` refuses `P_DKSRC`); the matrix has no such destination; morph and motion exclude it.
- Hide rule (`ui_draw.c card_hidden_vp`): the forced part's SRC card is not drawn, nor are its RATE / THR / DEPTH / ATK / RLS / CURVE cards (the OFF hide that was there, plus the SRC card).
- Audio (`fx.c duck_part`, `blockstate_t.duck_key[]`): the ducked part reads the other part's key follower of the block before, from the block's snapshot (`blk_snap` copies `duck_st[].key` before either part runs). One direction, no waiting. A ducked part 1 read the key of the block before already: bit for bit the same. A ducked part 2 read part 1's key of THIS block: now one block (0.73 ms) older.
- `tests/duckpart_test.c` (`./host_tests.sh duckpart`) renders the old path (the old `duck_part` verbatim, key read live) and the new one: part 2 ducked by part 1 (a plucked chord every 58 ms keys a held chord): largest sample difference 309 (output peak 18,597), the RMS of the difference 51.6 dB below the output; part 1 ducked by part 2: no difference. It also checks the rule (set, norm, hide, MIDI).
- Goldens changed by it: `multi_render_golden.txt` M02 at faders 0 / 63 / 127 (M02 has PUMP depth 61 on part 1 and PART depth 32 on part 2: part 1's PUMP is forced OFF and the key is a block old). No other render depends on DUCK SRC PART. regenerated, with a comment in the file.
- Factory data with PART on BOTH parts: none. Checked: the factory sounds (`tools/duell_sounds.py`: ten sounds with a ducker, all PUMP), the sixteen factory multis (`tools/factory_multis/M01..M16.bin`), the owner multis (`tools/owner_multis.py`: five, their records are the same files), the test scenes (`tests/scenes`, `tests/scenes/spare`). Only two multis have PART at all, one part each, so the rule changes only the ducker of the OTHER part: M02 (part 2 PART depth 32; part 1 PUMP depth 61 is lost) and M04 (part 1 PART depth 70; part 2 has PUMP at depth 0: nothing is lost). Nothing was changed in the data.

### 7.2 A random generator per part (blockers b and c)

- FORMANT noise (`eng_formant.c`): `formant_nz[NLIVE]`. A part's generator starts at its first note-on: the first part to play FORMANT starts at the seed the shared generator always had (0x2545F491), a second part at 0x6C8E9CF5. A scene with FORMANT on one part is bit for bit what it was (checked against cloud/partstate for either part); two parts differ.
- The voice stage's draws from the shared `rng()` (`dsp.c part_rand`; LFO 1's S&H on every LFO wrap and PHYS's model reset under a sounding note): part 1 keeps the shared generator, part 2 (and the hidden parts) draw from their own (`track_t.lfo_rnd` for the LFO, `track_t.rngs` for PHYS). `LFO_SHARED_RNG` 3 -> 1.
- Goldens changed: `multi_render_golden.txt` S13_TEST_1_BASE and S14_TEST_2_PHASE at faders 63 / 127 (both parts read the matrix's S&H source); `shimmer_test.c` its three mix hashes (a pad and a PHYS pluck on two parts: the pluck's note-on seed); `golden.txt` 12 renders (the PHYS presets / sounds, ANALOG 15_GLASS_SHARDS, PHASE 05_CRYSTAL_CAVE, TRIO 05_CIRCUIT_BEND, NOISE 09_MODEM, FM6 07_ALIEN_CHIMES, listed in its header). These are NOT only scenes with both parts sounding: a silent part's LFO also drew from the shared sequence at every wrap, so with part 2 on its own generator the sequence part 1, the arp, the note-on seeds see is shifted. Checked: with `LFO_SHARED_RNG` 3 and PHYS on `rng()` again `golden.txt` has 0 changes; FORMANT and the ducking change none of it. To keep those twelve bit for bit the shared sequence would have to be advanced by the other part's wraps in the old order, which is exactly the order dependence this removes.
- Found by the simulation (7.3), not by the earlier tests: `esync_k` / `esync2_k` (voice.c, the envelope's ENV SYNC steps of the block) were two globals both parts' voice stages wrote and read inside one `track_render`. Sequential schedules cannot see a scratch that lives inside one stage; two threads do. Now `track_t.esk` / `esk2`. Bit for bit.
- Other file-scope statics checked (nm of the host build, then ThreadSanitizer on all nine scenes): per part already (`eng_mem`, `fm6_eff`, `fm6_lfo_*`, `wv_gen`, `drw_t` / `drw_v`, `pan_*`, `dist_*`, `pfx_x`, `ins`, `eqs`, `duck_st`, `pf.mg`, `mod_goff`, `midi_bend_q8`), event stage only (`vage`, `voice_kills`, `mod_seed`, the UNISON `seed`, `rng_state` outside part 1's draws, `mq_*`, `motion_*`), main loop only (`wvj`, `fm6_own_ok`, `wt_used`, `fm6_own`), send / master stages (`lo_buf`, `dly_*`, `fx*`, `hl*`, `sv`, `bm`, `wdn`, `cmp`, the limiter). `sv` (SPACE) is read by both parts in `space_tap` and written by `mix_sends` after them: read-only while the parts run. `tune_key`, `song.*`, the other part's parameters: read-only. Part 1 still draws from `rng()` in its voice stage, and the event stage draws from it before the parts: one core at a time, in a fixed order.

### 7.3 The host-thread simulation (`tests/twocore_sim.c`, `./host_tests.sh twocore`)

Core 0 (the main thread): `mix_events`, PERFORM's begin, the snapshot, part 1's stages, then `mix_gather`, `mix_sends`, `mix_master`. Core 1 (a pthread): part 2's stages (`morph_begin`, `part_voice`, `part_fx` or `part_rest`, `part_mix`, `part_end`, `morph_end`) into its own `pscr` / `pacc` / track.
- Hand-over: one 64-bit job word (job number | job ^ MAGIC << 32, so a job and its check cannot tear), a claim word (job << 2 | owner: whoever changes it from free first renders part 2 for that block), a done word (core 1 writes the job number, release / acquire), a bounded spin (3,000) on the done word: late, core 0 tries the claim; if core 1 had not started it renders part 2 itself ("fell back") and core 1 skips; if core 1 had started it waits (bounded, a "crash" counted past 40 million spins). Counters: blocks, ok, fell back, late running, skipped, bad check, crash.
- Random sleeps on core 1 before the claim (7 % of the blocks) and after it (5 %). In a run: 27,560 blocks over the nine scenes, 17,096 in time, 9,544 fell back, 920 waited for a running core 1, 1,594 skipped by core 1, 0 torn words, 0 crashes; the output equals the single-thread path and the old combined path in every scene, with and without delays.
- Self-checks: core 1 writing part 1's REVERB send once must change the output (done in the binary); `twocore_mut` builds three mutated copies of the firmware (the block scratch of both parts made one, the matrix window made one, the ENV SYNC steps made one) and each must fail. `./host_tests.sh twocore_tsan` (slow, not in the default list; `TSAN_SCENES="0 8"`) runs the same under ThreadSanitizer: no data race.
- This models the protocol and the data separation, not the device's timing, the cache, or the second core's start-up.

### 7.4 RAM and hot set against cloud/partstate

`track_t` +16 B (esk, esk2 and rngs, the reserve pad shrunk: still a multiple of 16) = +64 B in `trk[]`; `blockstate_t` +8 B on the stack (`duck_key[2]`). No new global arrays (`formant_nz[NLIVE]` +4 B replaces the old 4 B). The hot set: no hot function is edited by job 3 (duck_part, `part_rand`, the FORMANT functions and `esync_coef` are not hot).

## 8. Job 4 (branch `cloud/notes`, from `cloud/partstate`): the NOTES budget

`firmware/src/budget.c` and `budget.h` (flash code, called once a UI frame and by the tests, never per sample, not in `tools/hot_fns.py`). Docs: CPU_PLAN section 5 is the design, this is what was built. One core only (the second-core definition comes with stage 3; nothing here depends on the second-core work).

### 8.1 The model

One table (top of `budget.c`), in the units of `tests/bench_baseline.txt` (host instructions a sample), turned into 1/100 of a percent of the time budget by `BD_U` with `BUDGET_MARGIN` 115 %. `BUDGET_LIMIT` is 80 (percent), `NOTES_MAX` 10, the voice pool `NVOICE` 8 (so 8 is the most a part gets until the pool grows). All three are named constants.

| item | % of the time budget, margin applied | source |
|---|---|---|
| one voice, engine only: ANALOG 6.9, FM6 8.0, PHASE 6.0, LOFI 4.2, VOICE 8.1, TRIO 7.0, WHEEL 7.5, PHYS 16.6, NOISE 6.8, WAVE 6.8 | | bench `voice/<ENGINE>` less the 12 dB filter (every factory sound runs it, KEY TRK 32) |
| multimode filter a voice: 12 dB 1.4, 24 dB 2.5 | | `uf_test.c` figures (4.8 % and 8.6 % for 4 voices) |
| MORPH on the part a voice 0.6, an engine choice cross-faded +2.3 | | the 0.2.11 VOICE CAP's figures |
| INSERT: SOFT 2.8, HARD 2.5, FOLD 2.6, FUZZ 2.8, CRUSH 1.7, PHASR 3.4, FLANG 2.6, CHOR 2.7, TAPE 3.3, VINYL 3.3 | | bench `insert/*` |
| SAT 1.7, EQ 0.2 + 0.9 a band with a gain not 0, DUCK 1.2 | | bench `part/*` |
| KEY TRK 0.04, a matrix slot 0.3, an LFO a slot reads 0.2, the arp 0.6 | | not in the bench: valgrind ratios (4 ANALOG voices) and `uf_test.c`; covered by the margin |
| idle mix with SENDS OFF: STEREO/LAYER 8.2, DUEL 7.7 | | bench `idle` less `bus/*/nothing_fed` |
| sends on, HALL + DIGITAL: STEREO/LAYER +7.8, DUEL +17.2; chorus +2.8 / +2.4 | | bench `bus/*` |
| delay mode (against DIGITAL fed): PING-PONG +0.9, RANDOM +4.3, OCT UP +4.3 (DUEL: 0, +4.2, +4.2) | | bench `bus/*` |
| reverb (against HALL): ROOM -1.2, SPRING -0.8, SHIM +1.3, RESO -1.5, PLATE 0, SHIM PITCH DOWN +2.6, SHIM + MOD +2.7 (DUEL, against its HALL: -1.8, -0.8, +3.4, -2.4, +0.1, +6.0, +6.2) | | bench `bus/*` |
| COMP 1.9, WIDTH 1.3, BASS MONO 1.3 (not in DUEL) | | bench `master/*` |

`global_cost()` adds the idle mix, the buses (the delay and the reverb run whenever SENDS is on, the chorus when a part that sounds sends to it, none while SENDS OFF) and the master stages. DUEL has its own column of the table (the buses run twice, once a side), measured, not doubled. A muted part costs nothing. `part_cost(t)` is ONE voice (engine, filter slope, morph extras: the filter is per voice, so it sits here and not in `part_fixed` as the task sketched it). `part_fixed(t)` is INSERT, SAT, EQ, ducker, KEY TRK, matrix slots, LFOs, arp.

### 8.2 The rule (and one change from the sketch)

A part's number is the most notes it can play while the OTHER part plays the same number (or its own limit if lower), with the worst split of the shared voice pool taken (the dearer part first): `global + both parts' stages + worst(n, min(n, other cap)) <= limit`, never below 1, never above `min(NOTES_MAX, pool, the engine's own limit)`.

The task asked for `floor((limit - global - other fixed - this fixed - other voices x their cost) / this cost)` with the other part "at its own cap", iterated twice. That form was built and rejected: two equal parts come out as 1 and 1 or as an unequal pair that depends on the order, and a version that gave the cheaper part the leftover notes raised a part's number when it got dearer (the monotone test caught it: 16 raises in the 16 multis). The rule above has the same shape (the other part's notes x their cost is in it), is symmetric, depends on the settings only (never on what plays, on the order of calls or on a half-solved other number), switching an item on can only lower the numbers, and the pair always fits. The cost: a cheap part beside a dear one gets the dear one's number, not the leftover.

`part_notes(part)` is the number, `part_notes_shown(part)` what the UI shows (the same, engine limit included), `part_notes_reason(part)` what limits it: `NR_ENGINE` (its own voices or the engine's limit), `NR_FILTER` (the 24 dB slope), `NR_INSERT`, `NR_SAT`, `NR_EQ`, `NR_FX` (the global effects), `NR_OTHER` (the other part's voices), `NR_MISC` (ducker, matrix, LFOs, arp, morph), `NR_NONE`. It is found by taking each item out in turn and seeing whether the part gains a note (the largest gain, then the dearest item). No UI was added.

### 8.3 Where it is used (the voice cap)

`budget_poll()` (main loop, next to `vcap_poll`) stores the numbers in `bnotes[]` (voice.c). `trk_nvoice()` is now `min(engine limit, old VOICE CAP, bnotes)`, so a new note over the number steals within the part exactly as at an engine's own limit (the oldest released voice, else the oldest held one, never the lowest note: restarted in place, no cut). New: `voice_trim()` (called at the start of `voice_alloc` and `mono_play`, only while `bnotes` is set): voices above the part's number, left from before it dropped, fade out with `voice_kill` (the one-block fade, 0.73 ms) at the part's next note-on. Functions touched in voice.c: `trk_nvoice` (it only reads `bnotes`; used by `voice_alloc`, `mono_play`, `trk_note_off_1`'s UNISON count and the new `voice_trim`), `voice_alloc`, `mono_play`, new `voice_trim`; main.c `main` (one call). None is in the hot list (`tools/hot_fns.py` unchanged). `UNISON` parts get fewer unison voices when the number is below 8, as they did under the old cap.

The old VOICE CAP code is kept and still applies, as the stricter of the two. It was NOT removed: in the 16 factory multis the budget is stricter or equal in all 32 parts (table below), so it could go, but its tests (`cpuscen` section C, the P table) need the Mac's instruction counter and could not be run here. Remove it after `./host_tests.sh cpuscen` runs on the Mac with the budget in `ui_test.c`'s frame.

### 8.4 NOTES a part, the 16 factory multis (default effects as loaded; and in cpuscen's state, MORPH on End B)

| multi | parts | mode | as loaded | MORPH on | old cap |
|---|---|---|---|---|---|
| M01 REESE SWAMP | WAVE / ANALOG | LAYER | 3 / 3 (UNISON: one note) | 3 / 3 | 8 / 8 |
| M02 NIGHT ORGAN | WHEEL / WAVE | LAYER | 3 / 3 | 2 / 2 | 8 / 8 |
| M03 PAD SPLIT | WAVE / ANALOG | LAYER | 3 / 3 | 2 / 2 | 8 / 8 |
| M04 DUB CHORD | FM6 / ANALOG | LAYER | 3 / 3 | 2 / 2 | 6 / 8 |
| M05 WHISPER TIDE | WAVE / VOICE | LAYER | 2 / 2 | 2 / 2 | 8 / 4 |
| M06 TINE STRINGS | TRIO / FM6 | LAYER | 3 / 3 | 2 / 2 | 8 / 6 |
| M07 FINAL REEL | WAVE / TRIO | LAYER | 8 / 8 | 3 / 3 | 8 / 8 |
| M08 MONO DUEL | TRIO / ANALOG | DUEL | 3 / 3 (mono) | 3 / 3 | 8 / 8 |
| M09 ACID ETHER | ANALOG / ANALOG | LAYER | 3 / 3 (mono) | 2 / 2 | 8 / 8 |
| M10 WIRED SNAIL | ANALOG / ANALOG | LAYER | 2 / 2 | 2 / 2 | 8 / 8 |
| M11 EERIE SIGNAL | VOICE / WHEEL | LAYER | 3 / 3 | 3 / 3 | 4 / 8 |
| M12 DEAD BUFFER | WAVE / NOISE | LAYER | 8 / 8 | 3 / 3 | 8 / 8 |
| M13 COLD CHAPEL | WAVE / PHYS | LAYER | 2 / 2 | 2 / 2 | 8 / 3 |
| M14 GARGLE ACID | WAVE / ANALOG | LAYER | 3 / 3 (mono) | 3 / 3 | 8 / 8 |
| M15 WOOD & SKIN | LOFI / PHYS | LAYER | 2 / 2 | 2 / 2 | 8 / 3 |
| M16 STRESS TEST | PHYS / WAVE | LAYER | 1 / 1 | 1 / 1 | 3 / 8 |

(`./host_tests.sh budget` prints the full table with the global and part costs and the reasons.) One core with a margin of 1.15 and a limit of 80 % gives 2 to 3 notes a part for most multis, as CPU_PLAN section 3 (one core) said; M07 and M12 as loaded fit 8 + 8 (pool-limited). The MORPH column is lower because an End B that switches a filter on or changes the slope costs every voice (the 0.2.11 cap counted it too).

### 8.5 What exceeds the budget

With 4 + 4 notes held (cpuscen's scene, MORPH on, fader in the middle) these change sound against the unbudgeted render because a part has fewer than 4 notes: M02 to M07, M10 to M13, M15, M16 (12 of 16). M01, M08, M09, M14 do not change (mono or unison parts play one note). With the notes the budget allows held, every multi renders bit for bit the same with the budget on and off, except M10 (its arp's release tails sound 6 voices from 2 held notes: over the budget in voices, so it changes). No factory data was changed. `ui_test.c`'s frame does not call `budget_poll`, so every existing test and golden stays as it was; only the firmware's main loop and `budget_test` do.

### 8.6 Test

`./host_tests.sh budget` (`tests/budget_test.c`, no instruction counter needed): (a) the table against `tests/bench_baseline.txt`: every engine, INSERT type, SAT, EQ, DUCK, bus effect in STEREO and DUEL, master stage within 5 %; the worst-case scene of each MODE: fixed part within 5 % (1,446 against 1,401 STEREO, 1,688 against 1,640 DUEL), totals at 1 + 1 and 4 + 4 notes between the bench and +12 % (the model errs high: its voice is 511 against the scene's 456 because a first voice costs more than the following ones); (b) the table above; (c) monotone on the 16 multis (352 steps) and 5000 random settings (10,000 steps): no raise; (d) steal: 12 note-ons with budgets 4, 1 and one worked out from settings: at most that many voices, largest step at a steal within 1.5 x the calm before (1.05, 1.22 and 0.93 x); the trim (8 held, the number drops to 3: 5 fade out, 0.85 x); (e) pure: the same numbers in any order, after any history, and in `budget_poll`; bit for bit as 7.5. The firmware keeps generator states that a load does not reset, so every bit-for-bit render runs in a child process.

Not measured: the absolute accuracy on the unit (the table is the Mac's bench x 1.15; CPU_PLAN stage 2 replaces it). The matrix, LFO, arp and KEY TRK lines are estimates.

## 9. Branch `cloud/base`: job 3 and job 4 merged

`cloud/base` = `cloud/partstate2` (d1cadf6, job 3) with `cloud/notes2` (4cfcdf8, job 4, the NOTES budget) merged in. Both sit on `cloud/partstate` (a7f3944), so later jobs start here.
- Conflicts: only text. `host_tests.sh` (the usage line, the `duckpart` / `twocore` and `budget` comment blocks, the default list, the test cases: both sets kept; `budget` follows `twocore` in the default list) and this file (job 4's section 7 became section 8, with its inner numbers and the references to it in `host_tests.sh` and `docs/CPU_PLAN.md`). `voice.c` merged without a conflict (job 3: the generators per part, ENV SYNC steps in `track_t`; job 4: `trk_nvoice`, `voice_alloc`, `mono_play`, `voice_trim`: different functions). No golden or fingerprint was regenerated by the merge.
- Checked on the merged tree: the budget's printed NOTES of the 16 factory multis (loaded and MORPH) and its bit-for-bit-within-budget lines are identical to `cloud/notes2` alone: the per-part generators and the one-sided ducking do not change them (the budget test's multis render the same with the budget on and off, whatever the generator).
- From `cloud/notes` nothing was brought over: `cloud/notes2` already has `part_notes_shown` and the per-item reason (`part_notes_reason`, `NR_*`), the same thing as `cloud/notes`' `BR_*` / `part_notes_why`.
