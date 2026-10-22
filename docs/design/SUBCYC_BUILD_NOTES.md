# SUB LAYER, CYCLE and WAVE STEPS: build notes (cloud job F)

What was built from `SUB_AND_CYCLE_DESIGN.md` (the decisions there are the ones built), and the choices the design left open. Private.

## Files

| Part | Files |
|---|---|
| SUB LAYER | `chain_sub.c` (`chain_sub_on`, `chain_sub`, `sub_gain`, `sub_step`, `sub_pass`), `eng_fm6.c` (`fm6_sub_follow`) |
| CYCLE | `cycle.c` (`cycle_tick`, `cycle_value`, `cycle_start`), hooks in `voice.c` (`voice_start`, `track_render`) |
| WAVE STEPS | `eng_wave.c` (`wave_steps`, the `.edit` slot, table 24 STAIR), hook in `voice.c` `track_render`, `morph.c` |
| NOTES | `budget_feat.c` (est. rows) |
| Tests | `subcyc_test.c` (+ `subcyc_ref.h`), `wave_test.c` section 10, `mx_step_test.c`, `partindep_scn.h` scenes 9..11 (also run by `twocore_sim`), `host_tests.sh` block `subcyc` (the four mutations) |

## Choices

- **SUB phase.** One accumulator at `m->inc >> 2`; the octave's phase is the accumulator shifted 0, 1 or 2 bits, so OCTAVE, WAVE and HARMONICS are phase locked and a switch keeps the phase.
- **TRI** is read a quarter cycle on: it starts at its zero crossing like the sine, and a SINE to TRI switch has no jump between waves.
- **PULSE** at width 1/4 has the same peak as SQUARE (its high level is full scale), so its fundamental is 0.47 of the SQUARE's.
- **HARMONICS** adds up to 0.5 of the sub's level an octave above (0 to 64), then up to 0.5 two octaves above (64 to 127). An octave above the Nyquist rate is left out.
- **LEVEL** curve: `SUB_CURVE`, (x / 128) ^ 1.6 in 17 points, 127 = `VOICE_FS` (a full-scale engine oscillator). An estimate for the owner to set by ear.
- **UNISON** sub is at the key's velocity (`mvel / vel` undoes the unison share), so LEVEL means the same in POLY and UNISON.
- **A new voice in the chain** (a new note, or LEVEL going above 0 on a voice on the plain path) starts the gain at once: the amp ramp or `chain_block`'s one-block cross-fade is the attack (a gain glide on top made a squared ramp).
- **FM6.** `fm6_sub_follow` returns the loudest carrier's `gain_out` against 2^25 (a carrier at full level), Q15. Measured on the host: the sub's level is the curve's gain times that value within 1 % over a decay and the release on every preset that reaches a steady stretch (subcyc_test). The engine's own peak is 0.64 to 1.17 of the follow value, so it is a level, not an exact peak.
- **CYCLE curve sign.** `y = x + c (x^2 - x)`, the fall is `1 - y` of the same shape: CURVE + is slow first on both segments (the rise lingers low, the fall lingers high), CURVE - is fast first (a plucked decay is a fall with a negative CURVE). This reads "mirrored on the two segments" as a flip in value.
- **CYCLE times.** RISE and FALL both go through `ENV_LIN` (the linear attack step of the ATK table), so RISE n takes as long as ATK n; FALL n is a straight line that takes as long as ATK n (DEC n's time constant is the same table). With CYCLE SYNC the steps are `esync_one(beat_samples(), v, 0)`, kept per part until the tempo or a value changes.
- **STEPS.** Stored 0 = OFF, n = n + 1 places (the enum has no meaningless value). Place k of n + 1 is `k (127 << 8) / n` in knob units, the first and last at the ends of the scan; 19 places are the 19 frames.
- **Morph of STEPS.** A roll never touches it (SMART: excluded; WILD: it draws as the old unused slot did, so no stored End B changes). Two counts in the ends blend (rounded), OFF against a count flips at the middle.

## State (ordinary RAM, arrays `[NLIVE][NVOICE]`, each part's own row)

| Array | Bytes |
|---|---|
| `sub_ph` 4 B, `sub_g` 2 B, `sub_cfg` 2 B | 128 |
| `cy_lin` 4 B, `cy_y` 2 B, `cy_st` 1 B (+ `cy_memo` 16 B a part) | 112 + 32 |
| `wv_stk` 1 B, `wv_stn` 1 B | 32 |
| Total | 304 |
