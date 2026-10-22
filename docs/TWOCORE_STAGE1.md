# TWO-CORE stage 1: the second core's device code, the unit protocol, and the work list

Branch `cloud/stage1` (from `cloud/base`). Everything here is behind the build flag `DUELL_TWOCORE` (default 0). With the flag off, the preprocessor removes every new line: the normal firmware and every host test are unchanged (checked when the branch was made: the preprocessed `felucca.c` is identical, and `./host_tests.sh` gives the same results as on `cloud/base`).

Stage 1 goal: core 1 plays part 2 (its voices, part effects and part mix into its own sums) while core 0 plays part 1, then gathers, runs the shared effects and the master. The sound must be the one-core sound sample for sample. Stage 1 is a developer build: core 1 runs only at a power-on that was armed once (as the earlier bench did).

## 1. What this branch adds

| File | What |
|---|---|
| `felucca-base/firmware/src/twocore.h` | The words (`tc_mb`), the check value, the reasons (`TCR_*`) and which are strikes, core 1's side (`tc_c1_step`, one pass of its waiting loop), core 0's state type, the stack mark. Plain C, compiles on the unit and on a computer. |
| `felucca-base/firmware/src/twocore.c` | Core 0's side: the hand-over primitives (`tc_post`, `tc_take`, `tc_answer`, `tc_closed`), the safety net (`tc_fail`, `tc_hold_clean`, `tc_watch`), the flash park (`tc_flash_park`), power-on (`tc_boot_reason`, `tc_boot`, `tc_arm`). The runner and the job are in `src/twocore_run.c` (section 8). |
| `felucca-base/firmware/hal/fm1_core1.h` | The unit's half of the hal: core 1's registers, its entry, its stack limit, its waiting loop in RAM (calls `tc_c1_step`), `fm1_cpu1_start`. From the bench code that ran on the owner's unit, changed to the words below. |
| `felucca-base/firmware/hal/fm1_sys.h` | `fm1_cpu1_halt` (hold core 1; first in every reset path through `FM1_RESET_PREP`), `fm1_supply_stock` / `fm1_supply_read` (M-VAVE's stock supply levels: VDC14 1.60 V, SYSVDD 1.38 V, one step per 100 us, up only), all under `DUELL_TWOCORE`. |
| `felucca-base/firmware/hal/fm1_irq.h` | `FM1_RESET_PREP()` in the fault report (nothing without the flag). |
| `felucca.c`, `main.c`, `storage_hw.c` | The hooks, each under `#if DUELL_TWOCORE`: the includes; core 1 held at the very start of `fm1_cstart`; `tc_boot` before `audio_init`; `tc_watch` every main-loop pass; `tc_hold_clean` before an update; `tc_flash_park` before every flash read, erase and program. |
| `felucca-base/tools/build.py` | `DUELL_TWOCORE=0/1` passed through like the other flags; `=1` needs a `BENCH_TAG` (a developer build) and is refused together with `DUELL_CORE1_PROBE`; its elfcheck expects the larger `.noinit` then. |
| `felucca-base/tools/elfcheck.py` | `tc_nv` (12 B) in `NOINIT_OBJECTS`, optional (only `DUELL_TWOCORE` builds have it). The source scan reads every source, compiled in or not, so the record is declared by its name in `hal/fm1_core1.h`, not through a macro. |
| `felucca-base/tests/twocore_hal_host.h` | The computer's half of the hal: atomics, a fault hook on every load and store, a simulated clock, a simulated start and hold. |
| `felucca-base/tests/twocore_stage1_test.c` | `./host_tests.sh tcstage1` (in the default list since job 5): the primitives and the safety net, one thread, every case exact (section 7). |

RAM with the flag on (estimated from the declarations): core 1's stacks 4,352 B, the pattern block 1,024 B, the words 128 B, core 0's state 32 B, the record 12 B (`.noinit`), about 5.5 KB in all; code: core 0's side well under 1 KB, core 1's loop a few hundred bytes (in RAM). Nothing with the flag off.

A `DUELL_TWOCORE=1` device build does not link yet: the linker scripts do not place `.c1_text` (local work, section 6). The job (`tc_job_run`) and the runner are cloud job 5 (section 8).

## 2. The unit protocol

### 2.1 The words

All in one object `tc_mb`, two 64-byte lines, one writer per line. Every access is one aligned 32-bit load or store (`TC_LD`, `TC_ST`). Nothing assumes a 64-bit store, a read-modify-write of a shared word, or an atomic swap between the cores: whether the chip has one is not known.

| Word | Writer | Meaning |
|---|---|---|
| `job` | core 0 | the job number: 1, 2, 3 ... (+1 for every job posted; never reused within a power-on; 32 bits last weeks at one job a block). Written last. |
| `arg`, `chk` | core 0 | what to do (the runner's encoding), and `TC_CHECK(job, arg)`. Written before `job`. |
| `take` | core 0 | "I want job N back": N |
| `alive` | core 1 | 1 once it runs its waiting loop |
| `claim` | core 1 | N: "I play job N" |
| `yld` | core 1 | N: "you play job N" (core 1 saw `take == N` before it claimed it, and never touches job N) |
| `done` | core 1 | N: job N finished (its sums are written) |
| `beat` | core 1 | +1 after every job, and every 4,096 polls of the waiting loop |
| `bad`, `bad_job` | core 1 | jobs refused, the last refused number |
| `misread` | core 1 | pattern words read wrong (16 a job) |
| `reread` | core 1 | jobs whose words fitted only at a second or third read |

Every word is compared by equality with a job number, so a stale word (an older number) simply means "not yet". A word ahead of the last job posted cannot happen and is a fault (`TCA_BAD`).

### 2.2 Who decides

Core 1 is the only one that decides who plays a job. It answers every job exactly once: `claim` (then `done`), or `yld`, or a refusal (`bad_job`). Core 0 never concludes "core 1 has not started" from a missing claim: it writes `take` and waits for the answer.

Why this form: a race-free "whoever comes first" with two plain words (each core writes its flag, then reads the other's) needs every core's store to be seen before its following load, across the cores. Whether the chip's `csync` gives that is not known. The form above needs only that each core's stores arrive in the order `TC_SYNC` sets (core 0: `arg`, `chk`, then `job`; core 1: part 2's sums, then `done`), the ordinary message passing the bench relied on. The cost: if core 1 is stuck before it answers, core 0 cannot take part 2 back (part 2 is silent until the heartbeat stops core 1, 100 ms). That happens only in a fault.

### 2.3 Core 1's step (`tc_c1_step`, inlined into the RAM loop)

1. `job` unchanged: count a poll (a beat every 4,096), return.
2. Read `arg`, `chk` after a sync. The number must be the next one (`last + 1`) and the check must fit. If not: read again (up to 2 more times, `TC_REREAD`). Still not: refuse (`bad_job = N`, `bad + 1`), never run. A fit at a re-read: run, `reread + 1` (counted, not a fault: the words it uses are right).
3. `take == N`: `yld = N`, return.
4. `claim = N`, sync, the job (`TC_JOB_RUN(arg)`), sync, `done = N`, a beat, 16 words of the pattern block checked (`misread`).

### 2.4 Core 0, one block (the runner: cloud job 5)

```
 events, the block's snapshot (blk_snap)
 if jobs run (core 1 up, not paused, not parked, the last job closed): N = tc_post(arg)
 part 1's stages
 wait for done == N until the block's deadline
   done            gather (sums 1 + sums 2), sends, master
   not done:       tc_take(N); wait for core 1's answer
     yld           core 0 plays part 2 itself (the same samples)                      counted: late
     claim         wait until the end of the half for done
       done        fine                                                               counted: very late
       not done    part 2 silent for this block (core 0 must not touch part 2);      counted: very late
                   jobs pause; part 2 stays silent until the open job is done, then
                   core 0 plays it (the late job's sums are thrown away)
     refused       core 0 plays part 2 itself; a strike (BADJOB)
     nothing       part 2 silent, jobs pause (core 1 is stuck: an idle core 1 answers
                   within microseconds); tc_watch stops it after 100 ms without a beat
```

After `tc_fail` core 0 owns part 2 again. If core 1 never claimed the open job, part 2's state is untouched and core 0 simply continues it. If it had claimed it, part 2's state may be half written: its voices are faded out and it starts cleanly (one click at worst).

The deadline comes from the half's own clock: the latest moment at which core 0 can still play part 2 itself and finish the half in time (from the NOTES cost table). A "very late" or a run of "late" blocks pauses the jobs (core 1 stays alive in its loop; it cannot be restarted once stopped, so a pause is the only "try again later"); they resume after a calm period.

## 3. The safety net

| Reason | Found by | Strike |
|---|---|---|
| `TCR_BADJOB` | a refused job (`bad`) | yes |
| `TCR_MISREAD` | a pattern word read wrong | yes |
| `TCR_BADWORD` | a core 1 word ahead of the last job | yes |
| `TCR_HANG` | no beat for 100 ms (`tc_watch`) | yes |
| `TCR_FLASH` | not idle within 20 ms at a flash access | yes |
| `TCR_NOANSWER` | no `alive` within 20 ms at the start | yes |
| `TCR_WDT`, `TCR_CRASH` | the record says core 1 ran, and the reset was a watchdog or a crash | yes (at the next power-on) |
| `TCR_RESET` | another reset while it ran | no |
| `TCR_SKIPBOOT` | the boot guard: the last start died in its first 30 s; one core this time | no |
| `TCR_OK` | a power-off, or a clean hold | no |

Late and very late blocks, load and flash parks within 20 ms are never strikes. A strike stops core 1 for the session (`tc_fail`: held; it cannot be restarted until the next power-on). The record across a reset (`tc_nv`, `.noinit`) keeps the last reason. Counting strikes, storing them, the notice and the menu line come in later stages.

## 4. The hal

Defined before `twocore.h` is included. Unit: `hal/fm1_core1.h` (`volatile` loads and stores, `csync`, TIMER4, the start and hold). Computer: `tests/twocore_hal_host.h` (C11 atomics, acquire loads and release stores: ThreadSanitizer does not model a fence on its own, so the order must sit on the words for part 2's plain data to count as ordered; `TC_SYNC` a full fence; a load hook `tc_host_ld_hook` and a store hook for fault injection; `tc_host_ticks`, a simulated clock advanced by `tc_host_tick_step` at every read; `tc_host_start_ok`, `tc_host_c1_started` to start a core-1 thread). `TC_JOB_RUN` may be set to a test's own job function.

## 5. What cloud job 5 must do

1. **The runner** (core 0, every block, in the audio interrupt): section 2.4 with the primitives of `twocore.c`; the deadline inside the half from the NOTES cost table; counters (in time, late, very late, paused blocks); the pause and its resume after a calm period; honour `tc.park` (no job while it is set: core 0 plays part 2 itself); after `tc_fail` during a claimed job, core 0 owns part 2 again and must start it cleanly (its voices faded out).
2. **The job** `tc_job_run(arg)`: part 2's stages for the block (`morph_begin`, `part_voice`, `part_fx` or `part_rest`, `part_mix`, `part_end`, `morph_end`) into part 2's own accumulators, with the block's snapshot that core 0 wrote before `job`.
3. **The simulation** (`tests/twocore_sim.c` updated): the real `tc_c1_step` on a thread with the computer hal, the real runner on the main thread; injected faults (torn reads, late jobs, a wrong check value, a stale done word: the output equal to the one-core output; a hung core 1, before and after its claim: equal up to the hang, part 2 silent until the stop, then core 0 plays it, every block counted); the late path taken and counted; a deliberate shared write caught; DUEL bus B placeholders.
4. **Host-side parts** of the core-1 start (the boot logic is here; the runner's view of `tc.up`) and of the fallback fade (extra notes fade at once when jobs pause or core 1 stops).
5. Wire `tcstage1` (and the new simulation) into the default list of `host_tests.sh`.

Rules: bit-exact audio, goldens unchanged, nothing added to the hot set (`tools/hot_fns.py` unchanged), no per-sample work, small commits.

## 6. What the local stage-1 build needs (cannot be done without the unit's tools)

- Linker scripts: `KEEP(*(.c1_text*))` inside the PC limit's RAM window (the bench put it at the end of `app_hot.ld`'s `.ram_text`, after the hot functions); `.pool.ztwocore` and `.noinit.ztwocore` placed after the existing objects (the patterns `*(.pool*)` and `*(.noinit*)` already take them; check that the existing objects keep their addresses). `tools/elfcheck.py` knows `tc_nv`; check the `.noinit` size of a real `DUELL_TWOCORE=1` link.
- Part 2's code in RAM: the engines in range X (old program RAM, 31.75 KB; code there cannot call the flash), the voice loop, filter, matrix, morph and the rest of the engines in range Z (end of the memory pool, about 28 KB, can call everything); the start-up copy; the PC limit's RAM window widened to cover them.
- The developer page: arm a start (`tc_arm`, then a restart), and the rows: core 1's load, late and very late blocks, re-reads, stack peak (`tc_stack_peak`), flash parks.
- The device build and the hot RAM check.

## 7. Tests

`./host_tests.sh tcstage1` (`tests/twocore_stage1_test.c`), 41 checks: a job claimed, run and done; `take` before core 1 looked (`yld`, not run) and after its claim (finished, run once); a check value that stays wrong (refused, never run, `BADJOB`) and one wrong read that fits at the re-read (run, `reread`, no strike); a wrong job number; a stale done word (not done) and one ahead (`BADWORD`); the heartbeat (idle beats keep it up for 1 s; 90 ms without a beat is fine; over 100 ms is `HANG`); a pattern word read wrong (`MISREAD`); the flash park (an idle core 1 parks at once and is counted; a job left open stops core 1 after 20 ms, `FLASH`); every case of the record across a reset and of the armed one-shot start; the stack mark. Checked by hand that each of five mutations of the protocol code (no `take` check, no re-read limit, no misread count, a stale done accepted, no idle beat) makes it fail.

## 8. Job 5 (branch `cloud/job5`, from `cloud/stage1`): the job, the runner, the simulation

Everything is under `DUELL_TWOCORE`; with the flag off the preprocessed `felucca.c` is token for token the one of `cloud/stage1` (checked: release and `DUELL_TEST_MODE=1`), every golden is unchanged. The flag-on firmware parses with the device flags (`clang --target=i686-unknown-elf -ffreestanding -fsyntax-only -Wall`, no new warning).

### 8.1 What was built

| Where | What |
|---|---|
| `src/twocore_run.c` (new, included by `fx.c` after `mix_part`) | the job `tc_job_run`; the runner `tcr_begin`, `tcr_part2` (with `tcr_settle`, `tcr_play2`, `tcr_fault`, `tcr_pause`); the deadline `tcr_deadline`; `tc_half_begin`, `tc_trim_poll`; the counters `tcs`, the runner's state `tcr`, the job's context `tc_ctx` |
| `src/fx.c` | `mix_block`: under the flag `tcr_begin`, part 1's stages, `tcr_part2` in place of the loop over both parts (the flag-off text is the old loop). `mix_gather` and `gather_general`: part 2 is not read while `tcr.skip2` |
| `src/audio.c` | `tc_half_begin(t0)` before the half's first block (the half's clock for the deadline) |
| `src/main.c` | `tc_trim_poll()` beside `tc_watch` |
| `src/voice.c` | `voice_trim_now(t)`: the fade at once (8.6) |
| `hal/fm1_core1.h` | `TC_CTX_SECT`: `tc_ctx` goes to `.pool.ztwocore` beside `tc_mb` |
| `tests/twocore_sim.c` | rewritten to the unit's protocol (8.7) |
| `tests/twocore_hal_host.h`, `tests/hostsim.c` | the hal: a store hook that returns the value stored, `tc_host_tick_div` / `tc_host_stop` (the clock stands still unless a delay runs; it does not pass a tick core 1 waits for), `tc_host_is_c1` (a held core 1 writes nothing); hostsim includes the hal and `twocore.c` only when a test sets the flag |
| `host_tests.sh` | `twocore` and `tcstage1` in the default list; two more mutations for `twocore_mut`; `twocore_tsan` runs `-tsan` |

### 8.2 The job

`tc_job_run(arg)` (core 1, called by `tc_c1_step` after its claim): `morph_begin`, `part_voice`, then `part_fx` + `part_mix` + `part_end` or `part_rest`, `morph_end`, on part 2 (`trk[1]`, its own `pscr[1]` / `pacc[1]`), in the order of `mix_part`. Its inputs: `arg` = n | 16-bit hash of the snapshot << 16, and the block's snapshot in `tc_ctx` (one `blockstate_t`, 40 B, in its own 64-byte line; core 0 fills it before `tc_post`, core 1 reads it after its claim, nobody in between). The hash (FNV over the words) is done: a snapshot that does not fit it (or an n other than CTL, the only block size; the stages get the constant CTL) runs nothing and is refused like a wrong check value: core 1 writes `bad_job` and `bad` (its own words), `tc_c1_step` still answers `done`, and the runner reads `bad` at the done word and treats it as a refusal (`tcr_part2`). Part 2's state is untouched then, so core 0 plays the block itself. `/* STAGE 2: bus B */` marks the empty spot after the stages.

### 8.3 The runner

`mix_block` calls `tcr_begin` after `blk_snap`: it settles the old job (`tcr_settle`: closed now, or core 1 was stopped: a claimed job means part 2 may be half written, `tcr.fade2`), does a trim asked by the main loop for part 1, resumes the jobs after the calm period, and posts this block's job only when core 1 is up, not paused, not parked (`tc.park`), the last job is closed and the block's deadline is still ahead. Otherwise core 0 plays part 2 itself in the one-core order (`tcr_play2`: the same stages, so the same samples) or, if core 1 still holds an older job, part 2 stays silent (`tcs.wait`). After part 1, `tcr_part2`:

1. waits for `done` until the deadline (`tc_answer` in a loop);
2. done (and `bad` still 0): "in time";
3. not done: `tc_take`, then waits up to `TCR_ANSWER_US` for core 1's answer:
   - `yld`: core 0 plays part 2 itself, no click: "late"; `TCR_LATE_RUN` late blocks in a row pause the jobs;
   - `claim`: waits for `done` until the wait limit `tcr.lim` (BENCHT4; up to BENCHT3 the end of the half): the latest moment at which core 1's done word still leaves core 0 the globals of this block and every later block of the half at the whole one-core cost (the deadline's `cap` plus part 2's own cost; see docs/TWOCORE_DIAG.md). Done: fine, "very late" (BENCHT4: it never pauses by itself). Not done: part 2 silent for the block (`tcr.skip2`, `mix_gather` does not read it), "very late" and "silent", and the jobs pause (a fault). The job stays open: while it is, no new job and part 2 stays silent (`wait`) until `tcr_settle` finds it closed; then core 0 plays part 2 (the late job's sums are thrown away);
   - no answer: "no answer", the same as the claim that does not finish (core 1 is stuck; `tc_watch` stops it after 100 ms);
   - a late `yld` after the wait: core 0 plays part 2;
   - refused (a check value, the snapshot) or a word ahead: `tc_fail` (a strike), core 0 plays part 2 from then on. If core 1 had claimed the open job, part 2's voices fade out first (`voice_kill`) and it starts cleanly; if not, it continues untouched.

Lateness is never a strike. A pause lasts `TCR_CALM_MS`; core 1 stays alive in its loop.

### 8.4 The deadline

`tcr_deadline` (once a block, before the post; the table's items once a half, at its first block): the half's end, less part 2's cost at its current settings (`part_fixed` + sounding voices x `part_cost`, from `budget.c`'s table), less the rest of this block (the globals: gather, sends, master), less every block still to come at the whole one-core cost (both parts and the globals: the worst case, core 1 late again), less `TCR_SLACK_TICKS`. In 1/100 % of a block's time the table's numbers become ticks with `need x TCR_BLOCK_TICKS / 10000`. Core 1 is idle (the last job is closed) when this runs, so part 2's settings and voices can be read. Measured in the simulation (mean of the posted jobs, microseconds after the half's start, the half is 2,901 us): scene 0 (two parts, 4 + 3 voices) 1,221 / 1,644 / 2,067 / 2,490 for blocks 0 to 3; scene 3 (MORPH on both, dearer) block 0 is past its deadline in half of the halves (core 0 plays part 2 then: counted as `own`).

### 8.5 Constants

| Name | Value | Meaning |
|---|---|---|
| `TCR_FRAME_TICKS` | 544 | TIMER4 ticks per I2S frame (audio.c `DAC_TICKS`) |
| `TCR_BLOCK_TICKS`, `TCR_HALF_TICKS`, `TCR_BLOCKS` | 17,408, 69,632 (2.9 ms), 4 | one block, one half, blocks in a half (`CTL` 32, `HALF_FRAMES` 128) |
| `TCR_SLACK_TICKS` | 3,481 (145 us, 5 % of a half) | kept back from the deadline for what the table does not hold (events, note starts, the interrupt's entry) |
| `TCR_ANSWER_US` | 100 | after the take, core 1 answers within this (an idle core 1 in microseconds), else it is stuck |
| `TCR_LATE_RUN` | 4 | late blocks in a row that pause the jobs |
| `TCR_VLATE_RUN`, `TCR_VWIN_K` | 0, 0 (off; BENCHT3: 4 and 8 within `TCR_VWIN_MS` 2,000; BENCHT2: 1) | very late blocks (job finished by the wait limit) in a row / within the window that pause the jobs; BENCHT4: never by default, switchable by `bench vrun` / `bench vwin` for a diagnosis (docs/TWOCORE_DIAG.md); a real fault, a job not finished by the wait limit, or 4 late blocks in a row still pause |
| `TCR_CALM_MS` | 10,000 (240 million ticks) | how long a pause lasts |
| `TCR_NEED_MAX` | 200,000 | the cost sums are clamped (a tick count of it fits 32 bits) |
| (stage 1, unchanged) `TC_HANG_MS` 100, `TC_PARK_US` 20,000, `TC_REREAD` 2 | | the heartbeat, the flash park, the re-reads |

### 8.6 Counters, the flash park, the start, the fade at once

Counters (`tcs`, 48 B; the developer page shows these, the flash parks are `tc.parks`): `blocks`, `in_time`, `late`, `vlate`, `silent`, `wait`, `own`, `refused`, `noanswer`, `pauses`, `resumes`, `trims`. Every block is counted once in `in_time + late + vlate + refused + own + wait` (checked in every simulation run); `silent` = the very late blocks that did not finish + `wait`.

Flash park: the runner posts no job while `tc.park` and plays part 2 itself; `tc_flash_park` and `tc_watch` are the stage-1 code. The simulation parks in the middle of play (output equal, one park counted, `own` blocks meanwhile) and with core 1 stuck (`FLASH` after 20 ms of simulated time).

Start: a session without an armed start, or `tc_host_start_ok = 0` (NOANSWER), runs on one core: every block is `own`, the output equals the old combined path bit for bit, and the record says why (`TCR_NONE`; `TCR_NOANSWER`, a strike).

Fade at once (`voice_trim_now`, `tc_trim_poll`): when the jobs pause or core 1 stops, notes above the part's number fade out in the next block (`voice_kill`: released notes first, the oldest first, then the oldest held), not at the next note-on as `voice_trim` does. `tc_trim_poll` (main loop) only asks; the audio interrupt does it, for part 2 only when core 0 owns it. Under the flag only; in stage 1 the numbers are the one-core ones. Tested: 8 notes held on part 2, the number drops to 3, then a very late block pauses the jobs (or a wrong check value stops core 1): 5 notes fade out in the next block, the 3 youngest stay, the largest sample step 1.5 x the calm before it at most (budget_test (d)'s rule).

### 8.7 The simulation (`./host_tests.sh twocore`, `twocore_tsan`)

Core 1: a pthread running the real `tc_c1_step` loop with the computer hal; its job is the real `tc_job_run` (wrapped by `sim_job`: delays, the bus B check). Core 0: the main thread running the firmware's own `mix_block` (the runner) with `tc_half_begin`, `tc_watch` and `tc_trim_poll` at each half (4 blocks of 32). The clock is simulated and stands still unless a delay is running: a healthy core 1 is never late because the machine was busy, and each late case is the block it is aimed at. Nine scenes of `partindep_scn.h`, each in a child process with one fault:

| Run | What is injected | Result required |
|---|---|---|
| none | | output = old combined path; no late block; core 1 wrote no bus B state |
| torn1 | core 1 reads a new job with the old arg / chk once | re-read, run, counted; output equal |
| tornp | persistently | refused, BADJOB, a strike; core 0 plays part 2 from then on; output equal |
| late_pre | core 1 sleeps before it looks | take, yld, "late"; output equal |
| late_post | core 1 sleeps after its claim, done in the half | "very late", pause, resume; output equal |
| late_run | 4 and 5 late blocks in a row | pause, resume; output equal |
| badchk | the store hook corrupts chk | refused, a strike; output equal |
| stale | the load hook returns the older number for done | not done, the deadline path, done in the half; output equal |
| ahead | a done word ahead (aimed at a job core 1 has not looked at) | BADWORD, a strike; output equal |
| badsnap | a bit of the snapshot core 1 reads is wrong | refused (hash), a strike; output equal |
| hang_pre, hang_post | core 1 hangs before / after its claim | silent until `tc_watch` stops it (HANG, 100 ms simulated); output equal up to the hang, then equal to a one-core run that leaves out exactly the counted silent blocks (after the claim: and fades part 2's voices out) |
| park | a flash park in the middle of play | output equal, one park, core 1 still up |
| park_stuck | a flash park with core 1 stuck | FLASH after 20 ms simulated; output as hang_post |
| silent_late | core 1 finishes a job after its half | part 2 silent, pause, then core 0 plays it (counters only: the late job renders later state) |
| unarmed, noanswer | no start / a failed start | one core, output equal, the record says why |
| shared, busb | self-checks | core 1 writing part 1's REVERB send changes the output; core 1 writing bus B's delay half is seen by the check |

Bus B (`fx_b`, `hl_b`, `sa_*`, `sp_f*_a`, the delay's half B) is summed before and after every job on the core-1 thread; in stage 1 it never changes (0 of 26,702 jobs). Totals over the nine scenes (one run each, 27,560 blocks): none 26,702 in time and 858 `own` (the two dearest scenes: the deadline was already past); late_pre 863 late; late_post 671 very late, 671 pauses, 671 resumes; late_run 81 late, 18 pauses; torn1 3,815 re-reads; hang_pre / hang_post 9 x 1 very late, 1,283 silent (143 blocks a scene: 100 ms of simulated time), then core 0; 555 checks, 0 failed. Three mutated copies of the firmware from job 3 (the parts share the block scratch / the matrix window / the ENV SYNC steps) and two new ones (`mix_gather` reads part 2 while it is silent; the fade-out of a stopped core 1's voices removed) must make the simulation fail: they do. ThreadSanitizer (`twocore_tsan`, scenes 0 and 8): no data race.

### 8.8 What the simulation does not cover (read before the unit)

- A silent window with core 1 still running (very late, not done by the half's end, the job finishes later): core 0 runs the next blocks' events and `mix_sends` meanwhile. The event stage can start a note on part 2's voices and `mix_sends` reads part 2's matrix bus offsets (`mod_goff`) while core 1 writes them. Nothing crashes; at worst one block of wrong bus offsets or a part 2 note that starts garbled. It is a fault path (the half was already missed). The simulation holds core 1 back in `silent_late` until the next half, so it is not raced; ThreadSanitizer does not run it.
- A word ahead found while core 1 is in a claimed job: part 2's voices fade out and restart (one click). The simulation aims the word at an unclaimed job to keep the output equal.
- Timing: the deadline is the NOTES table (host figures x 1.15), not the unit's. The first numbers from the unit (late / very late blocks, the deadline's slack) decide `TCR_SLACK_TICKS`.
- Cache and ordering: the hal's acquire / release atomics are stronger than `csync`. The stage-1 protocol was built for the weaker order, and the snapshot is guarded by its hash, but only the unit shows it.

### 8.9 RAM and code (estimates)

RAM with the flag on, added by job 5: `tcs` 48 B, `tcr` 52 B (both `.bss`), `tc_ctx` 64 B (one line, `.pool.ztwocore`): 164 B. Code (host clang x86-64 `-Os`, nothing inlined; device about 0.76 of that, as MIXSPLIT_NOTES section 3): the runner on core 0 (flash) 1,623 B, `voice_trim_now` 277 B, the changes in `mix_block` / `mix_gather` / `gather_general` +54 B; core 1's side (`tc_job_run` 175, `tc_job_refuse` 57, `tc_ctx_hash` 44) 276 B: about 2.2 KB host, 1.7 KB on the device, of which about 0.2 KB in RAM with part 2's code. Nothing is in the hot set (`tools/hot_fns.py` unchanged); `mix_block`, `mix_gather` and `gather_general` are flash code; the flag-off build has none of it.

### 8.10 What the local stage-1 build needs

- Part 2's code and tables core 1 reaches from `tc_job_run`: `docs/TWOCORE_JOB5_REACH.txt` (found from the compiled calls and the address-taken functions and tables: a superset; 283 functions, about 52 KB on the device, 25 of them already in the hot RAM set; the tables with pointers (the engine descriptors) and the read-only tables). They go into the RAM ranges X and Z (section 6) or stay reachable for core 1; the engines X cannot call the flash.
- Core 1's stack: the deepest chain found is `tc_job_run` > `part_voice` > `track_render` > `wave_render` > `wv_line` > `wv_osc` > `wv_lvl` > `wv_read_sine` > `sine_i`, 1,680 B on the host (x86-64 frames); about 1.1 to 1.3 KB on the device, in the 4 KB user stack of `fm1_c1_stack`. Read `tc_stack_peak` on the unit.
- New objects: `tc_ctx` (64 B, `TC_CTX_SECT`) in `.pool.ztwocore` after `tc_mb` (the pool's tail needs 64 B more), `tcs` and `tcr` in `.bss` (100 B); no new `.noinit` object (`tc_nv` as before); no new section (`.c1_text` as in section 6).
- The developer page's rows: the counters `tcs` (blocks, in time, late, very late, silent, refused, pauses, resumes) and `tc.parks`.
