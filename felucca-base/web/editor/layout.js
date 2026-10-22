// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the firmware's own tables (layout.json, written by tests/editordump.c) and what is derived from them: descriptors that
// depend on the sound (engine hooks, INSERT type, ENV SYNC, ARP target), value text, the hide-rule predicates, the tab layout.
// No DOM, no I/O here (the tests import it).
import LAYOUT from "./layout.json" with { type: "json" };

export const L = LAYOUT;
// 0.2.25: tests/editordump.c writes an enum's names from index 0, not from its min (params.c N_SLOPE {"-", "12dB", "24dB"}, SLOPE 1..2 came out as "-", "12dB": the
// editor showed "-" for 12 dB). The only enum whose range does not start at 0 is SLOPE; put its names right here until the dump is fixed (see the 0.2.25 report)
for (const d of [L.TP[48]]) if (d?.names && d.min > 0 && d.names[0] === "-" && d.names.length === d.max - d.min + 1) d.names = d.names.slice(1).concat(d.names.length === 2 && d.names[1] === "12dB" ? ["24dB"] : []);
export const ID = L.ID;
export const P_E0 = L.P_E0;
export const NPART = 2;

// ---- the extension slots (cloud job I; firmware ext.h, params.c XTP; protocol scope 2): 15 stored values that have no parameter id. A part holds them in part.x[k] (k 0 .. NX-1, read through
// GET / TRACK_PARAM scope 2); a page or card refers to one by scope 2 and its slot number (pageControls: a page id of XW_BASE + k is slot k). X: the slot numbers by name (L.XID)
export const NX = L.NX ?? 0;
export const XW_BASE = L.XW_BASE ?? 128;
export const X = L.XID ?? {};
export const xDefaults = () => (L.XTP ?? []).map((d) => d.def);
// the descriptor of slot k of this part: the stored range, CYCLE's RISE and FALL as note values while its own SYNC switch is on
export function xdescOf(part, k) {
  if (part?.x?.[X.X_CYSYNC] && (k === X.X_CYRISE || k === X.X_CYFALL)) return L.XSYNC[k - X.X_CYRISE];
  return L.XTP[k];
}
// the TWIN FILTER's CUTOFF 2 and RES 2 are one stored value each with two or three meanings (params.c tw_desc): CUTOFF 2 is OFFSET while LINK is on, CUTOFF with LINK off, TUNE on COMB;
// RES 2 is RES, or FDBK (the editor writes FEEDBACK) on COMB
export const twinComb = (part) => part.p[ID.P_TWTYPE] === L.TW?.COMB;
export const twinLinked = (part) => (part.x?.[X.X_LINK] ?? L.XTP?.[X.X_LINK]?.def ?? 1) !== 0;
export function twDesc(part, id) {
  const T = L.TW, comb = twinComb(part);
  if (!T) return L.TP[id];
  if (id === ID.P_CUT2) return comb ? T.TUNE : twinLinked(part) ? T.OFFSET : T.CUTOFF;
  return comb ? T.FDBK : T.RES;
}

// ---- descriptors
const dynFor = (eng, e0, e1, k) => {
  let hit = null;
  for (const [a, b, kk, d] of L.ENG[eng].dyn) if (kk === k && a === e0 && (b === -1 || b === e1)) hit = d;
  return hit;
};
// part: { eng, p: [P_COUNT values] }
export function descOf(part, id) {
  const p = part.p;
  if ((id === ID.P_CUT2 || id === ID.P_RES2) && L.TW) return twDesc(part, id);
  if (id >= P_E0) {
    const k = id - P_E0, e = L.ENG[part.eng] || L.ENG[0];
    return dynFor(part.eng, p[P_E0], p[P_E0 + 1], k) || e.edit[k];
  }
  if (id === ID.P_ATGT) return L.ATG[L.ENG[part.eng]?.atgt || 0];
  if (id >= ID.P_IA && id <= ID.P_ILAST) return L.INS[Math.min(Math.max(p[ID.P_ITYPE] | 0, 0), L.INS.length - 1)][id - ID.P_IA];
  if (p[ID.P_ESYNC]) {
    if (id === ID.P_ATK) return L.ESYNC[0];
    if (id === ID.P_DEC) return L.ESYNC[1];
    if (id === ID.P_REL) return L.ESYNC[2];
    if (id === ID.P_E2ATK) return L.ESYNC[3];
    if (id === ID.P_E2DEC) return L.ESYNC[4];
    if (id === ID.P_E2REL) return L.ESYNC[5];
  }
  return L.TP[id];
}
export const gdesc = (id) => L.GP[id];
// the globals the delay MODE changes (core.h G_DMODE: 0 DIGITAL, 1 PING-PONG, 2 RANDOM, 3 OCT UP; a value past OCT UP plays as DIGITAL). g: the unit's globals.
// In RANDOM / OCT UP the card DEPTH (G_DMDEP) is CHANCE and MRATE does nothing; in DIGITAL SPREAD does nothing: those cards are hidden. (The unit also hides
// MODE and SPREAD in DUEL; the editor cannot read the output mode.) The editor's MODE list has the full names.
export const dlyMode = (g) => { const m = g?.[ID.G_DMODE] | 0; return m >= 0 && m < L.DMD.why.length ? m : 0; };
const isEv = (m) => m === 2 || m === 3;
let modeDesc = null;
export const gdescOf = (g, id) => {
  if (id === ID.G_DMDEP && isEv(dlyMode(g))) return L.DMD.chance;
  if (id === ID.G_DMODE) return (modeDesc ??= { ...L.GP[id], names: L.DMD.full });
  return L.GP[id];
};
// MODE written to v while it was `was`: what the unit does to the DEPTH / CHANCE id with it (params.c dly_mode_changed). g: the globals (already holding the new MODE),
// auto: the editor set that CHANCE itself. Returns { depth, auto }: depth the value to write (null: none)
export function dmdAfterMode(g, was, v, auto) {
  const ev = isEv, dep = g[ID.G_DMDEP];
  if (ev(v) && !ev(was) && dep === 0) return { depth: L.DMD.chanceDef, auto: true };
  if (!ev(v) && ev(was) && auto && dep === L.DMD.chanceDef) return { depth: 0, auto: false };
  return { depth: null, auto: ev(v) ? auto : false };
}
// SHIM's PITCH and MOD (core.h G_SHPITCH, G_SHMOD; the unit's page SHIMMER, REVERB page 2) apply only while TYPE is SHIM (3): with another model they are hidden, with the model's name as the reason.
// The unit also leaves the page out of its page cycle then (ui.c page_visible); the editor leaves the SHIMMER card out (app.js)
export const shimShown = (g) => g?.[ID.G_RTYPE] === 3;
export function ginert(g, id) {
  if ((id === ID.G_SHPITCH || id === ID.G_SHMOD) && !shimShown(g)) return L.GP[ID.G_RTYPE].names[g?.[ID.G_RTYPE] | 0] ?? "TYPE";
  const m = dlyMode(g);
  if (id === ID.G_DMRATE && isEv(m)) return L.DMD.why[m];
  return id === ID.G_PINGSPREAD && m === 0 ? L.DMD.why[0] : null;
}

// the shown text of value v: { val, unit }
export function valueText(d, v) {
  if (d.zeroText != null && v === 0) return { val: d.zeroText, unit: "" };   // a value of 0 that means "not run" (TWIN DRIVE: OFF)
  if (d.blend) {                                                            // TWIN BLEND (1.0, owner): the filters' names, the percent of the way from BOTH (the unit's twin_card): MULTI at -64, BOTH at 0, TWIN at +63
    if (v <= d.min) return { val: "MULTI", unit: "" };
    if (v >= d.max) return { val: "TWIN", unit: "" };
    return v === 0 ? { val: "BOTH", unit: "" } : { val: `${v < 0 ? "MULTI" : "TWIN"} ${Math.trunc(Math.abs(v) * 100 / (v < 0 ? 64 : 63))}%`, unit: "" };
  }
  if (d.texts) return { val: d.texts[v - d.min] ?? String(v), unit: "" };   // a number drawn as a slider whose positions have words (WAVE's STEPS: OFF, 2 .. 19)
  if (d.names) return { val: d.names[v - d.min] ?? String(v), unit: "" };
  if (d.fk >= 0) { const t = L.FMTS[d.fk][v - d.min]; if (t) return { val: t[0], unit: t[1] }; }
  return { val: String(v), unit: d.unit || "" };
}
export const isEnum = (d) => !!d.names;
export const clampTo = (d, v) => Math.min(d.max, Math.max(d.min, v));
// enum values in the order the unit's knob steps them (aliases left out)
export const enumOrder = (d) => d.order || Array.from({ length: d.max - d.min + 1 }, (_, i) => d.min + i);
export const unusedLabel = (d) => !d.label || d.label[0] === "-";

// ---- the hide rule: why a control of this part does nothing now (null: it applies; else it is hidden, the reason is for the tests and tooltips). Only what a selection makes impossible.
export function inertReason(part, id) {
  const e = L.ENG[part.eng], p = part.p;
  if (!e) return "NO ENGINE";
  if (id === ID.P_CUT2 || id === ID.P_RES2 || id === ID.P_DRV2 || id === ID.P_BLEND) return twinWhy(part, id);   // (cloud job I: filter 2's values; the TYPE itself is never hidden)
  if (id >= P_E0) return unusedLabel(descOf(part, id)) ? "UNUSED" : null;
  if (id >= ID.P_IA && id <= ID.P_IMIX && id !== ID.P_ITYPE && !p[ID.P_ITYPE]) return "INSERT OFF";
  if (id === ID.P_LRATE && p[ID.P_LSYNC]) return "SYNC ON";
  if (id === ID.P_L2RATE && p[ID.P_L2SYNC]) return "SYNC ON";
  if (e.ownenv && (id === ID.P_ATK || id === ID.P_DEC || id === ID.P_SUS || id === ID.P_REL)) return "OWN ENV";
  if ((id === ID.P_ED_SHP || id === ID.P_LD_SHP || id === ID.P_E2SHP || id === ID.P_L2SHP) && shapeInert(part)) return "WAVE";   // 0.2.25: no shape input with this wave
  if (p[ID.P_DKSRC] === 3 && (id === ID.P_DKRATE || id === ID.P_DKTHR || id === ID.P_DKDEP || id === ID.P_DKATK || id === ID.P_DKRLS || id === ID.P_DKCRV)) return "DUCK OFF";   // 0.2.24: DUCK SRC OFF (3): nothing ducks, the other DUCK cards are hidden (the unit's card_hidden_vp)
  return null;
}
// 0.2.25 (owner: "many VOICE controls seem to do nothing"): the editor's own hide rule for the VOICE card, from the firmware (voice.c, fx.c; docs/VOICE_CONTROLS.md).
// The unit draws these cards always; the editor hides what cannot be heard in the current VOICES setting and says why. null: it applies.
//   NOTE PRIORITY: MONO, LEGATO, UNISON only (voice.c mono_pick; POLY sounds every key)
//   (VOICE ALLOCATION, id 43, was here: POLY only; the id is the TWIN FILTER's TYPE since cloud job B and no voice rule applies to it)
//   UNISON DETUNE: UNISON only (mono_play: the voices' fine offsets; one voice has none)
//   GLIDE MODE: only while GLIDE is above 0 (glide_set: no glide, no mode)
//   STEREO SPREAD: POLY and UNISON only (fx.c spread_on: one voice cannot be spread; the unit also hides it in DUEL, which the editor cannot see)
export const V = { POLY: 0, MONO: 1, LEGATO: 2, UNISON: 3 };
export function voiceWhy(part, id) {
  const p = part.p, vm = p[ID.P_VOICE] | 0;
  if (id === 42 && vm === V.POLY) return "VOICE POLY";
  // (id 43 was VOICE ALLOCATION; since cloud job B it is the TWIN FILTER's TYPE, which the voice mode does not touch: its two rules are gone)
  if (id === 44 && vm !== V.UNISON) return "VOICE NOT UNISON";
  if (id === ID.P_GLMODE && !(p[ID.P_GLIDE] > 0)) return "GLIDE 0";
  if (id === 95 && (vm === V.MONO || vm === V.LEGATO)) return "VOICE ONE VOICE";
  return null;
}
// cloud job I (FILTER_PAGES_PROPOSAL 3 and 5.2): the TWIN FILTER's hide rules. ref: a part parameter id (ID.P_TWTYPE, P_CUT2, P_RES2, P_DRV2, P_BLEND) or an extension slot, written "x" + the slot number
// (twinX(X.X_LINK)). null: it applies; else the reason (shown in the card's tooltip, labels.js WHY): TWIN OFF (TYPE is OFF: only TYPE itself is drawn), TWIN COMB (the comb follows the played key itself:
// LINK and KEY TRACK are not drawn). LINK changes the label of CUTOFF 2 (twDesc) and hides nothing. COMB is not a TYPE of WAVE and PHYS (twinTypeOffered)
export const twinX = (k) => `x${k}`;
export function twinWhy(part, ref) {
  if (ref === ID.P_TWTYPE) return null;
  if (!part.p[ID.P_TWTYPE]) return "TWIN OFF";
  return twinComb(part) && (ref === twinX(X.X_LINK) || ref === twinX(X.X_KTRK2)) ? "TWIN COMB" : null;
}
// COMB needs the filter's delay lines in the engine's memory: WAVE and PHYS do not have the room (twin_type_set refuses it; a stored COMB plays as LP there)
export const twinTypeOffered = (part, v) => !(v === L.TW?.COMB && ["WAVE", "PHYS"].includes(L.ENG[part.eng]?.name));
// the flow of the sound through the two filters, one line (TWIN CHAIN's flow line): SERIES MULTI then TWIN with the DRIVE between them (chain_f2.c); PARALLEL the DRIVE first, then both filters side by side (the unit draws them as two branches)
export function twinFlow(part) {
  const par = !!part.x?.[X.X_ROUTE];
  return par ? "ENGINE > DRIVE > MULTI + TWIN > AMP" : "ENGINE > MULTI > DRIVE > TWIN > AMP";
}

// 0.2.25: the unit's MODULES switches (MENU 28 ARP, 29 SEQ, 30 SENDS, 31 PART FX; ui.c mods_of / mods_fade) hide what they switch off. menu: the unit's MENU items (null: not read)
const MOD_IDS = { 28: [17, 18, 19, 20, 21, 22, 23, 24, 86], 29: [30, 31], 30: [34, 35, 36], 31: [33, 96, 97, 98, 99, 100, 87, 8, 89, 90, 88, 104, 103, 111, 112, 113, 107, 108, 110, 105] };
const MOD_GLOBAL_SENDS = [10, 11, 4, 5, 6, 7, 25, 12, 13, 15, 24, 8, 9, 21, 26, 17];
const modOff = (menu, mid) => { const m = menu?.find((x) => x.id === mid); return !!m && m.names?.[m.value - m.min] === "OFF"; };
export function modulesWhy(menu, id, part = null) {
  if (!menu) return null;
  if (part == null) return MOD_GLOBAL_SENDS.includes(id) && modOff(menu, 30) ? "MODULES SENDS" : null;
  if (MOD_IDS[28].includes(id)) {
    if (id === 23 && part.p[23] !== 1) return null;                               // (HOLD: only its ARP latch needs the arpeggiator)
    if (modOff(menu, 28)) return "MODULES ARP";
    return modOff(menu, 29) && part.p[ID.P_AMODE] === 15 ? "MODULES SEQ" : null;
  }
  for (const mid of [29, 30, 31]) if (MOD_IDS[mid].includes(id) && modOff(menu, mid)) return mid === 29 ? "MODULES SEQ" : mid === 30 ? "MODULES SENDS" : "MODULES PART FX";
  return null;
}
// 0.2.24: the matrix stays packed (the unit's mod.c mod_compact): MATRIX 5-8 is shown only while slots 1..4 are all used (slot 4's SRC not OFF)
export const matrixSecondShown = (part) => (part.p[ID.P_M4SRC] | 0) !== 0;
// a control whose range goes below zero (0.2.24: its fill grows from the zero point; 0.2.25, owner: ONLY these): a number with a negative and a positive end (pan, tune, offsets, depths, key track).
// A 0 .. 100 or 0 .. 200 control with its default in the middle (oscillator MIX 50, WIDTH 100) is a plain slider: its fill grows from the left like any other
export const isBipolar = (d) => !d.names && d.min < 0 && d.max > 0;
export const bipolarZero = (d) => 0;
// 0.2.25 (owner: the hide rule for modulation; the unit's mod.c mod_shp_inert / mod_e_inert, exported by the firmware as layout.json MXH): the destinations the current wave of
// the part's engine leaves without effect (ANALOG's SHAPE outside PWM / SYNC, LOFI's outside PLS and DUTY on TRI SAW NOIS, TRIO's PW / INT2 / INT3 / DTN where no oscillator takes them)
const mxhOf = (eng) => L.MXH?.eng?.find((x) => x[0] === eng);
export function dstWaveInert(part, dst) {
  const x = mxhOf(part.eng);
  if (!x || x[2] < 0) return false;
  const r = x[3].find((y) => y[0] === (part.p[P_E0 + x[2]] | 0));
  return !!r && r[1].includes(dst);
}
// the SHAPE input unused with this wave: the SHAPE depth cards (AMP DEST, LFO 1 DEST, ENV 2 DEST, LFO 2 DEST) hide, as on the unit (ui_draw.c card_hidden_vp)
export const shapeInert = (part) => dstWaveInert(part, L.MD.SHP);
// the matrix destination dst of this part: why it does nothing (the device's mod_dst_why, minus PAN-in-DUEL: the editor cannot read the output mode)
export function dstWhy(part, dst, g = null) {
  const e = L.ENG[part.eng], M = L.MD;
  if (!e) return "NO ENGINE";
  if (dst >= M.E1 && dst < M.E1 + 8) {
    const k = dst - M.E1;
    return unusedLabel(e.edit[k]) ? "UNUSED" : e.why[k] ?? (dstWaveInert(part, dst) ? "WAVE" : null);
  }
  if ((dst === M.SHP || dst === M.E2SHP) && dstWaveInert(part, dst)) return "WAVE";
  if (dst >= M.INSA && dst <= M.IMIX && !part.p[ID.P_ITYPE]) return "INS OFF";
  if (M.CUT2 != null && dst >= M.CUT2 && dst <= M.BLEND && !part.p[ID.P_TWTYPE]) return "TWIN OFF";   // (the unit's mod_dst_why: filter 2's values are offered while its TYPE is not OFF; a stored route keeps playing)
  if (dst === M.RES && !e.ufilt) return "ENGINE";
  if (dst === M.BUS + 4 && g && isEv(dlyMode(g))) return L.DMD.why[dlyMode(g)];   // (the delay modulation's RATE in RANDOM / OCT UP, as the unit's mod_dst_why)
  if (M.AATK != null && dst >= M.AATK && dst < M.AATK + 4 && e.ownenv) return "OWN ENV";   // (T0229: the AMP ENV times; FM6's operators have their own envelopes)
  return null;   // (T0229, owner: FILTER RES is never dimmed or hidden by the cutoff: no CUT OPEN)
}
// 0.2.25: the DEST list offers dst for this part (the unit's mod.c mod_dst_offered): not removed (the EQ gains, the engine's list slots: layout.json MXH), and nothing
// the selection makes do nothing (dstWhy). The value a slot holds stays in the list (a stored route keeps working)
export function dstOffered(part, dst, g = null) {
  if (dst <= 0) return true;
  if (L.MXH?.rm?.includes(dst) || mxhOf(part.eng)?.[1].includes(dst)) return false;
  return !dstWhy(part, dst, g);
}
// the name of matrix destination dst as the unit shows it (E1..E8 by the engine's labels, INS A..C by the INSERT type)
export function dstName(part, dst) {
  const M = L.MD, names = L.TP[ID.P_M1SRC + 1].names;
  if (dst >= M.E1 && dst < M.E1 + 8) { const d = descOf(part, P_E0 + dst - M.E1); return unusedLabel(d) ? "-" : d.label; }
  if (dst >= M.INSA && dst < M.INSA + 3) return descOf(part, ID.P_IA + dst - M.INSA).label;
  return names[dst];
}

// ---- pages
export const partTabs = (eng) => PART_TABS.filter((t) => !t.only || L.ENG[eng]?.name === t.only);
export const pageByTitle = (title, fam) => L.PAGES.find((pg) => pg.title === title && (!fam || pg.fam === fam));
export const pageVisible = (pg, eng) => pg.engines.includes(eng);

// The tabs. Every firmware page appears in exactly one of: a tab (editable here), DUPLICATE (the same controls shown elsewhere), or
// NOT_EDITABLE (with the reason). test_editor.mjs fails when PAGES holds a page that is in none of them, so a new page cannot slip by.
// Each part column has its own tab strip (PART_TABS); the global column (the middle one, or the third) has GLOBAL_TABS.
export const PART_TABS = [
  { key: "engine", label: "ENGINE", pages: ["ENGINE", "EDIT 1", "EDIT 2", "VOICE 1/2", "VOICE 2/2"] },   // 0.2.21: the signal order; the multimode filter moved to FILTER (cloud job I); no engine has a filter of its own since cloud job J
  // cloud job I (FILTER_PAGES_PROPOSAL 3): filter 1 (MULTI FILTER, CUTOFF MOD) and filter 2 (TWIN FILTER, TWIN MOD, TWIN CHAIN: extension slots and ids 25, 26, 43, 101, 106 that have no unit page in this base) on one tab, in the order the sound flows
  { key: "filter", label: "FILTER", pages: ["MULTI FILTER 1/2", "MULTI FILTER 2/2"] },
  { key: "envelope", label: "ENVELOPE", pages: ["AMP ENV", "AMP DEST", "ENV 2", "ENV 2 DEST"] },
  { key: "lfo", label: "LFO", pages: ["LFO 1", "LFO 1 TRIG", "LFO 1 DEST", "LFO 2", "LFO 2 TRIG", "LFO 2 DEST"] },
  { key: "matrix", label: "MATRIX", pages: ["MATRIX 1-4", "MATRIX 5-8"] },
  { key: "fx", label: "FX", pages: ["FX", "INSERT 1/2", "INSERT 2/2", "EQ 1/2", "EQ 2/2", "DUCK 1/2", "DUCK 2/2"] },
  { key: "arpseq", label: "ARP", pages: ["ARP 1/3", "ARP 2/3", "ARP 3/3", "CHORD", "STEP"] },
  // 0.2.19: the pattern piano roll (roll.js / rollview.js). Its LEN DIV SWG GATE are the SEQUENCER card's ids (SEQ_PARAMS); the firmware page STEP stays listed under ARP
  { key: "roll", label: "SEQ", pages: [] },
];
// the title of the unit's master-stage page (a GLO page, after SHIMMER): "STEREO MASTER" since the 0.2.25 layout round (K1 BASS MONO, K2 WIDTH: it replaced WIDTH and MASTER;
// firmware params.c MASTER_PAGE_TITLE; this one place here); then COMPRESSOR (K1 RATIO, K2 THRESH, K3 ATTACK, K4 RELEASE)
export const MASTER_TITLE = "STEREO MASTER";
export const GLOBAL_TABS = [
  { key: "global", label: "GLOBAL", pages: ["TEMPO", "CHORUS", "DELAY 1/2", "DELAY 2/2", "REVERB", "SHIMMER", MASTER_TITLE, "COMPRESSOR"] },
  { key: "patches", label: "PATCHES", pages: ["PATCHES"] },
  { key: "multis", label: "MULTIS", pages: ["MULTIS"] },
  { key: "patterns", label: "PATTERNS", pages: ["PATTERNS"] },   // 0.2.25: the pattern slots P01..P16 through the backup objects 21..23 (stored.js)
  { key: "midi", label: "MIDI", pages: ["SIDECHAIN", "MIDI LEARN"] },
  { key: "tables", label: "WAVETABLES", pages: [] },
  { key: "settings", label: "SETTINGS", pages: ["MODULES"] },
];
// pages whose ids are shown on another card (the same parameters)
export const DUPLICATE = {
  // cloud job H (the unit pages) meets job I (the editor tabs): the same controls are the FILTER tab's cards and the engine / envelope cards
  "TWIN FILTER": "the FILTER tab's TWIN FILTER card (TYPE, LINK, OFFSET / CUTOFF / TUNE, RESONANCE / FEEDBACK)",
  "TWIN MOD": "the FILTER tab's TWIN MOD card",
  "TWIN CHAIN": "the FILTER tab's TWIN CHAIN card",
  "SUB OSC": "the ENGINE tab's SUB OSC card beside the engine's card",
  "OP TUNE": "the ENGINE tab's OPERATORS editor (FM6): OPERATOR 1 to 6",
  "OP ENV": "the ENGINE tab's OPERATORS editor (FM6): OPERATOR 1 to 6",
  "OP MODE": "the ENGINE tab's OPERATORS editor (FM6): OPERATOR 1 to 6",
  "CYCLE ENV 1/2": "the ENVELOPE tab's CYCLE ENV card (MODE, SYNC, RISE, FALL, CURVE)",
  "CYCLE ENV 2/2": "the ENVELOPE tab's CYCLE ENV card",
  MIXER: "the unit's GLO > MIXER page (0.2.25 layout round: GLO's first page; LEVEL 1, PAN 1, LEVEL 2, PAN 2, the MUTE cells): each part column's MIXER block (LEVEL, PAN, MUTE)",
  "SEND FX": "the unit's SEND FX page (FX's last page) shows the selected part's three sends (the FX tab's CHO DLY REV cards) beside the effects they feed (the GLOBAL tab's cards); its twists only jump to those pages (0.2.24: the page says so under the cards, TURN = JUMP TO SETTINGS, and which effect is inactive for the part)",
};
export const NOT_EDITABLE = [
  ["SCALE", "SCALE, ROOT and OFF-SCALE are the instrument's, not parameters: the editor protocol has no command for them"],
  ["MIDI", "the parts' MIDI channels (P1 CH, P2 CH) are not reachable by the protocol; MIDI IN is on the MIDI tab"],
  ["TOOLS", "CLEAR SEQ is the SEQ tab's Clear; INIT SOUND is the Init patch... button of each part; RESTORE ALL is on the PATCHES tab"],
  ["AUTOMATION", "recorded motion and locks: not in this version (the protocol has MOTION)"],
  ["MORPH", "the MORPH fader, ROLL and UNDO have no protocol command; End B is rolled from a seed, not stored as values (see below)"],
  ["MORPH SET", "SMART, ARP and PITCH of the morph have no protocol command"],
  ["MODE", "the output MODE (DUEL, STEREO, LAYER), BALANCE, XFADE and SPLIT have no protocol command"],
];
export function uncoveredPages() {
  const used = new Set([...PART_TABS, ...GLOBAL_TABS].flatMap((t) => t.pages));
  const dup = new Set(Object.keys(DUPLICATE).map((k) => k.split("#")[0]));
  const no = new Set(NOT_EDITABLE.map((x) => x[0]));
  return L.PAGES.filter((pg) => !used.has(pg.title) && !dup.has(pg.title) && !no.has(pg.title)).map((pg) => pg.title);
}

// the sequencer parameters that are on no firmware page (the SEQ tab's card)
export const SEQ_PARAMS = [ID.P_SLEN, ID.P_SDIV, ID.P_SSWING, ID.P_SGATE, ID.P_SQNT];
// the parameters of each part column's MIXER block: the mixer LEVEL, PAN, MUTE (0.2.19: a sound load keeps all three; the sound's loudness is P_TRIM, PATCH LEVEL, in the PATCH block)
export const HEAD_PARAMS = [ID.P_LEVEL, ID.P_PAN, ID.P_MUTE];

// the controls of one page for a part: [{id, scope}] (an empty slot is left out); DUCK's second knob follows SRC, a matrix page is the slot grid
export function pageControls(pg, part) {
  const out = [];
  for (const id of pg.ids) {
    if (id < 0) continue;
    let rid = id;
    if (id === ID.P_DKRATE) {   // DUCK's second knob follows SRC: RATE for PUMP, THR for PART, none for MIDI (0.2.22: the channel and the note are the device's SIDECHAIN setup, GLO > SIDECHAIN)
      if (part.p[ID.P_DKSRC] >= 2) continue;
      rid = part.p[ID.P_DKSRC] === 1 ? ID.P_DKTHR : ID.P_DKRATE;
    }
    out.push(rid >= XW_BASE ? { id: rid - XW_BASE, scope: 2 } : { id: rid, scope: pg.scope === "GLOBAL" ? 1 : 0 });   // (a page id of XW_BASE + k is extension slot k: scope 2)
  }
  return out;
}
export function matrixSlots(pg) { const base = pg.ids[1]; return [0, 1, 2, 3].map((k) => ({ src: base + 3 * k, dst: base + 3 * k + 1, amt: base + 3 * k + 2, n: (base - ID.P_M1SRC) / 3 + k + 1 })); }

export const engineLabel = (e) => L.ENG[e]?.name ?? "?";
// the engines one can pick, in the unit's order; a part that already plays another (DRUM, a retired one) lists it too
export const pickable = (cur) => (L.ORDER.includes(cur) ? L.ORDER : [...L.ORDER, cur]);
export const CATEGORIES = ["-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "DRUM", "FX", "OTHER"];
