// SPDX-License-Identifier: GPL-3.0-only
// DUELL 1.1 editor: the DEMO content. Nothing here is used with a real unit. (Until the firmware had the twelve new extension slots this file added them as a demo-only layer; they are now the
// firmware's own: layout.json NX 28, slots 15 to 26. What is left is the content that makes the simulated unit look lived in: multi names, matrix rows, values on the new knobs.)
import { L, X, ID, P_E0, xDefaults } from "./layout.js";
import { preselectMatrix } from "./mockunit.js";

// the demo's content: names of multis, matrix rows and values on the new knobs, so every control shows a value
const idx = (list, name) => list.indexOf(name);
// DEMO ONLY until the firmware has them (owner, 2026-10-25): two more matrix sources at the END of the source list, BREATH CONTROLLER (MIDI CC2) and FOOT CONTROLLER (MIDI CC4). The picker shows them
// in the same category-over-name style (labels.js MATRIX_GROUPS, group OTHER). When the firmware's own list already has them (layout.json), nothing is changed.
export function enableDemoSources() {
  for (let k = 0; k < 8; k++) {
    const d = L.TP[ID.P_M1SRC + 3 * k];
    if (!d?.names || d.names.includes("BRTH") || d.names.includes("BREATH")) continue;   // (the firmware has BREATH and FOOT now, DUELL 1.1)
    d.names = [...d.names, "BRTH", "FOOT"]; d.max += 2;
    if (d.order) d.order = [...d.order, d.max - 1, d.max];
  }
}
export function seedDemo(m) {
  if (!m) return;
  m.demoTwin = true;                                                  // (every sound that loads in the demo gets the TWIN FILTER on, see mockunit.js demoTwinOn)
  L.MULTIS.slice(0, 16).forEach(([i, name]) => { m.multiStored[i] = name; });
  m.rebuild();
  const srcNames = L.TP[49].names, dstNames = L.TP[50].names;
  const demoX = {   // part 0, part 1: slot key -> value (stored units, -64 to 63 for a depth; 0 to 127 for a level)
    0: { X_AMPSAT: 12, X_E2BLND: 10, X_E2DRV: 6, X_E2RES: 14, X_CYTW: 8, X_CYPAN: 10, X_CYPIT: 0, X_L1LVL: 100, X_L2TWR: 6, X_L2MOD: 0, X_L2BLND: 8, X_L2LVL: 127, X_ENV2: 20, X_LFO1: 5, X_LFO2: 8, X_KTRK2: 0 },
    1: { X_AMPSAT: 0, X_E2BLND: 0, X_E2DRV: 0, X_E2RES: 7, X_CYTW: 0, X_CYPAN: 17, X_CYPIT: 0, X_L1LVL: 127, X_L2TWR: 0, X_L2MOD: 5, X_L2BLND: 0, X_L2LVL: 71, X_ENV2: 0, X_LFO1: 0, X_LFO2: 0, X_KTRK2: 0 },
  };
  m.parts.forEach((q, p) => {
    q.x = xDefaults().map((d, i) => q.x?.[i] ?? d);
    for (const [key, v] of Object.entries(demoX[p])) q.x[X[key]] = v;
    q.p[ID.P_TWTYPE] = L.TW ? 5 : q.p[ID.P_TWTYPE];                 // TWIN FILTER on (LADDER), so its controls show
    if (L.ENG[q.eng].name === "ANALOG") q.p[P_E0] = 4;                // WAVEFORM PWM: a wave that has a TIMBRE input, so the TIMBRE amounts show
  });
  // amounts on the depth knobs of the existing set (stored units): AMP ENV PITCH and TIMBRE, FILTER ENV MULTI CUTOFF, LFO 1 PITCH, MULTI CUTOFF, TIMBRE and AMP
  const depths = { 0: { 7: 12, 114: 24, 13: 5, 14: 6, 15: 10 }, 1: { 6: 4, 114: 12, 13: 3, 15: -6, 16: 12 } };
  m.parts.forEach((q, p) => { for (const [id, v] of Object.entries(depths[p])) q.p[+id] = v; });
  // a few matrix rows in each part: a source to a destination at a moderate amount
  const matrix = [[["MODW", "CUT", 32], ["AT", "SHP", 20], ["VEL", "FRES", -14], ["LFO1", "PITCH", 7]], [["ENV2", "SAT", 24], ["LFO2", "PAN", 12], ["KEY", "SHP", -8]]];
  m.parts.forEach((q) => { for (let k = 0; k < 8; k++) { q.p[49 + 3 * k] = 0; q.p[50 + 3 * k] = 0; q.p[51 + 3 * k] = 0; } });   // (empty first: the rest is filled with the unit's preselection below)
  matrix.forEach((rows, p) => rows.forEach(([s, d, a], k) => {
    const si = idx(srcNames, s), di = idx(dstNames, d), q = m.parts[p].p;
    if (si >= 0 && di >= 0) { q[49 + 3 * k] = si; q[50 + 3 * k] = di; q[51 + 3 * k] = a; }
  }));
  m.parts.forEach((q) => preselectMatrix(q.p));                      // the empty slots get the unit's preselected pairs at amount 0 (EDITOR_PROTOCOL.md, "MATRIX preselection")
  // the PARAMETER LOCKS: a few records in each part (the stored value of a parameter, or an amount in the range -64..63 for PITCH, TIMBRE, VOICE AMP, the bus values and SPACE); the step is 0-based
  m.motion = [
    { track: 0, step: 4, tid: ID.P_FREEA, v: 52, lock: true },     // LOCK 05 MULTI CUTOFF
    { track: 0, step: 8, tid: ID.P_PAN, v: -32, lock: true },       // LOCK 09 PAN
    { track: 0, step: 12, tid: 192 + 56, v: 16, lock: true },       // LOCK 13 SPACE (+25 %)
    { track: 0, step: 0, tid: 46, v: 20, lock: false },             // AUTO 01 MULTI RES
    { track: 1, step: 2, tid: 36, v: 60, lock: true },              // LOCK 03 REVERB SEND
    { track: 1, step: 0, tid: 192 + 3, v: -20, lock: false },       // AUTO 01 TIMBRE
  ];
  // the MIDI map: CC to part and parameter id (MIDI MAP tab)
  m.learn = [[74, 0, 45], [71, 0, 46], [73, 0, 1], [72, 0, 4], [75, 0, 114], [76, 0, 14], [77, 1, 45], [78, 1, 38]].map(([cc, track, id]) => ({ cc, track, id }));
}
