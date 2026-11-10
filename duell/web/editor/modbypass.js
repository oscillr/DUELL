// SPDX-License-Identifier: GPL-3.0-only
// DUELL 1.1 editor: BYPASS (owner, HANDOFF_V2 item 61a, round 2). A TEMPORARY switch per module and per effect for sound design: a bypassed filter passes the sound unchanged, a bypassed LFO or envelope sends nothing to
// its destinations, a bypassed effect is skipped. The unit holds bits (volatile: never saved with a patch, a multi or a project; gone after power off). A part's bits (its modules and its effects) also clear when a sound loads into that part; the SHARED mask (the buses and master stages) does NOT clear on a sound load, it stays until pressed again (owner, 2026-10-25). No saved value is ever changed.
// EDITOR_PROTOCOL.md, "BYPASS (proposal)". No DOM, no I/O.
// Per part (two bytes of 7 bits, 11 bits used): the modules and this part's effects. Global (one byte, 5 bits used): the shared buses and the master stages.
export const MODULES = {
  mfilter: { bit: 0, label: "MULTI FILTER", what: "pass" },
  tfilter: { bit: 1, label: "TWIN FILTER", what: "pass" },
  lfo1: { bit: 2, label: "LFO 1", what: "send" },
  lfo2: { bit: 3, label: "LFO 2", what: "send" },
  fenv: { bit: 4, label: "FILTER ENV", what: "send" },
  cycle: { bit: 5, label: "CYCLE ENV", what: "send" },
  sat: { bit: 6, label: "SATURATOR", what: "skip" },
  insert: { bit: 7, label: "INSERT", what: "skip" },
  eq: { bit: 8, label: "EQUALIZER", what: "skip" },
  duck: { bit: 9, label: "DUCKING", what: "skip" },
  sends: { bit: 10, label: "SENDS", what: "skip" },
};
export const GLOBALS = {
  chorus: { bit: 0, label: "CHORUS", what: "skip" },
  delay: { bit: 1, label: "DELAY", what: "skip" },
  reverb: { bit: 2, label: "REVERB", what: "skip" },
  stereo: { bit: 3, label: "STEREO MASTER", what: "skip" },
  comp: { bit: 4, label: "COMPRESSOR", what: "skip" },
};
export const EFFECT_KEYS = ["sat", "insert", "eq", "duck", "sends"];
export const PART_EFFECT_BITS = EFFECT_KEYS.reduce((m, k) => m | 1 << MODULES[k].bit, 0);   // 1984
export const GLOBAL_ALL = Object.values(GLOBALS).reduce((m, g) => m | 1 << g.bit, 0);        // 31
export const MASK_PART = 2047, MASK_GLOBAL = 31;
export const hasBit = (mask, spec) => ((mask | 0) >> spec.bit & 1) === 1;
export const flip = (mask, spec, max) => ((mask | 0) ^ (1 << spec.bit)) & max;
export const lo7 = (m) => m & 127, hi7 = (m) => (m >> 7) & 127, join7 = (lo, hi) => (lo & 127) | (hi & 127) << 7;
