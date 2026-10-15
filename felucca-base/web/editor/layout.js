// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the firmware's own tables (layout.json, written by tests/editordump.c) and what is derived from them: descriptors that
// depend on the sound (engine hooks, INSERT type, ENV SYNC, ARP target), value text, the hide-rule predicates, the tab layout.
// No DOM, no I/O here (the tests import it).
import LAYOUT from "./layout.json" with { type: "json" };

export const L = LAYOUT;
export const ID = L.ID;
export const P_E0 = L.P_E0;
export const NPART = 2;

// ---- descriptors
const dynFor = (eng, e0, e1, k) => {
  let hit = null;
  for (const [a, b, kk, d] of L.ENG[eng].dyn) if (kk === k && a === e0 && (b === -1 || b === e1)) hit = d;
  return hit;
};
// part: { eng, p: [P_COUNT values] }
export function descOf(part, id) {
  const p = part.p;
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
  if (id >= P_E0) return unusedLabel(descOf(part, id)) ? "UNUSED" : null;
  if (id >= ID.P_IA && id <= ID.P_IMIX && id !== ID.P_ITYPE && !p[ID.P_ITYPE]) return "INSERT OFF";
  if (id === ID.P_LRATE && p[ID.P_LSYNC]) return "SYNC ON";
  if (id === ID.P_L2RATE && p[ID.P_L2SYNC]) return "SYNC ON";
  if (e.ownenv && (id === ID.P_ATK || id === ID.P_DEC || id === ID.P_SUS || id === ID.P_REL)) return "OWN ENV";
  return null;
}
// the matrix destination dst of this part: why it does nothing (the device's mod_dst_why, minus PAN-in-DUEL: the editor cannot read the output mode)
export function dstWhy(part, dst) {
  const e = L.ENG[part.eng], M = L.MD;
  if (!e) return "NO ENGINE";
  if (dst >= M.E1 && dst < M.E1 + 8) {
    const k = dst - M.E1;
    return unusedLabel(e.edit[k]) ? "UNUSED" : e.why[k];
  }
  if (dst >= M.INSA && dst <= M.IMIX && !part.p[ID.P_ITYPE]) return "INS OFF";
  if (dst === M.RES && !e.ufilt) return "ENGINE";
  return null;
}
// the name of matrix destination dst as the unit shows it (E1..E8 by the engine's labels, INS A..C by the INSERT type)
export function dstName(part, dst) {
  const M = L.MD, names = L.TP[ID.P_M1SRC + 1].names;
  if (dst >= M.E1 && dst < M.E1 + 8) { const d = descOf(part, P_E0 + dst - M.E1); return unusedLabel(d) ? "-" : d.label; }
  if (dst >= M.INSA && dst < M.INSA + 3) return descOf(part, ID.P_IA + dst - M.INSA).label;
  return names[dst];
}

// ---- pages
export const pageByTitle = (title, fam) => L.PAGES.find((pg) => pg.title === title && (!fam || pg.fam === fam));
export const pageVisible = (pg, eng) => pg.engines.includes(eng);

// The tabs. Every firmware page appears in exactly one of: a tab (editable here), DUPLICATE (the same controls shown elsewhere), or
// NOT_EDITABLE (with the reason). test_editor.mjs fails when PAGES holds a page that is in none of them, so a new page cannot slip by.
// Each part column has its own tab strip (PART_TABS); the global column (the middle one, or the third) has GLOBAL_TABS.
export const PART_TABS = [
  { key: "engine", label: "ENGINE", pages: ["ENGINE", "EDIT 1", "EDIT 2", "VOICE 1/2", "VOICE 2/2", "MULTI FILTER", "VCF", "TRIO FILTER", "NOISE FILT"] },
  { key: "mod", label: "MOD", pages: ["AMP ENV", "AMP DEST", "ENV 2", "ENV 2 DEST", "LFO 1", "LFO 1 TRIG", "LFO 1 DEST", "LFO 2", "LFO 2 TRIG", "LFO 2 DEST"] },
  { key: "matrix", label: "MATRIX", pages: ["MATRIX 1-4", "MATRIX 5-8"] },
  { key: "fx", label: "FX", pages: ["FX", "INSERT 1/2", "INSERT 2/2", "EQ 1/2", "EQ 2/2", "DUCK 1/2", "DUCK 2/2"] },
  { key: "arpseq", label: "ARP / SEQ", pages: ["ARP 1/3", "ARP 2/3", "ARP 3/3", "CHORD", "STEP"] },
  // 0.2.19: the pattern piano roll (roll.js / rollview.js). Its LEN DIV SWG GATE are the SEQUENCER card's ids (SEQ_PARAMS); the firmware page STEP stays listed under ARP / SEQ
  { key: "roll", label: "PIANO ROLL", pages: [] },

];
// the title of the unit's master-stage page: "BASS MONO" while it is its only card, "MASTER" later (firmware params.c MASTER_PAGE_TITLE; this one place here)
export const MASTER_TITLE = "BASS MONO";
export const GLOBAL_TABS = [
  { key: "global", label: "GLOBAL", pages: ["CHORUS", "DELAY 1/2", "DELAY 2/2", "REVERB", "SHIMMER", MASTER_TITLE, "TEMPO"] },
  { key: "patches", label: "PATCHES", pages: ["PATCHES"] },
  { key: "multis", label: "MULTIS", pages: ["MULTIS"] },
  { key: "midi", label: "MIDI", pages: ["MIDI LEARN"] },
  { key: "tables", label: "WAVETABLES", pages: [] },
  { key: "settings", label: "SETTINGS", pages: ["MODULES"] },
];
// pages whose ids are shown on another card (the same parameters)
export const DUPLICATE = {
  "SIDECHAIN": "the DUCK cards on FX (the same SRC and NOTE)",
};
export const NOT_EDITABLE = [
  ["SCALE", "SCALE, ROOT and OFF-SCALE are the instrument's, not parameters: the editor protocol has no command for them"],
  ["GLO", "MUTE and the mixer LEVEL (LVL) of both parts are in each column's header; the level meters are not sent"],
  ["MIDI", "the parts' MIDI channels (P1 CH, P2 CH) are not reachable by the protocol; MIDI IN is on the MIDI tab"],
  ["PATTERNS", "pattern slots P1..P16: only in the backup so far (no command to browse or save them)"],
  ["TOOLS", "CLEAR SEQ and INIT SOUND are actions on the unit"],
  ["AUTOMATION", "recorded motion and locks: not in this version (the protocol has MOTION)"],
  ["MORPH", "the MORPH fader, ROLL and UNDO have no protocol command"],
  ["MORPH SET", "SMART, ARP and PITCH of the morph have no protocol command"],
  ["MODE", "the output MODE (DUEL, STEREO, LAYER), BALANCE, XFADE and SPLIT have no protocol command"],
  ["MIXER", "The mixer LEVEL (LVL), PAN and MUTE are in each column's header, the sends on the FX tab; meters are not sent. The sound's own loudness, its TRIM, is on the ENGINE tab (VOICE 2/2): it is saved with the patch, the mixer level is not"],
  ["OP TUNE", "FM6 operator frequencies: the patch bytes travel by FM6_GET / FM6_PUT, not in this version"],
  ["OP ENV", "FM6 operator envelopes: the patch bytes travel by FM6_GET / FM6_PUT, not in this version"],
  ["OP MODE", "FM6 operator mode, detune, velocity: the patch bytes travel by FM6_GET / FM6_PUT, not in this version"],
];
export function uncoveredPages() {
  const used = new Set([...PART_TABS, ...GLOBAL_TABS].flatMap((t) => t.pages));
  const dup = new Set(Object.keys(DUPLICATE).map((k) => k.split("#")[0]));
  const no = new Set(NOT_EDITABLE.map((x) => x[0]));
  return L.PAGES.filter((pg) => !used.has(pg.title) && !dup.has(pg.title) && !no.has(pg.title)).map((pg) => pg.title);
}

// the sequencer parameters that are on no firmware page (the SEQ tab's card)
export const SEQ_PARAMS = [ID.P_SLEN, ID.P_SDIV, ID.P_SSWING, ID.P_SGATE, ID.P_SQNT];
// the parameters of the header: the mixer LEVEL (LVL), PAN, MUTE (0.2.19: a sound load keeps all three; the sound's loudness is P_TRIM, a card of the ENGINE tab)
export const HEAD_PARAMS = [ID.P_LEVEL, ID.P_PAN, ID.P_MUTE];

// the controls of one page for a part: [{id, scope}] (an empty slot is left out); DUCK's second knob follows SRC, a matrix page is the slot grid
export function pageControls(pg, part) {
  const out = [];
  for (const id of pg.ids) {
    if (id < 0) continue;
    let rid = id;
    if (id === ID.P_DKRATE) rid = part.p[ID.P_DKSRC] === 0 ? ID.P_DKRATE : part.p[ID.P_DKSRC] === 1 ? ID.P_DKTHR : ID.P_DKNOTE;
    out.push({ id: rid, scope: pg.scope === "GLOBAL" ? 1 : 0 });
  }
  return out;
}
export function matrixSlots(pg) { const base = pg.ids[1]; return [0, 1, 2, 3].map((k) => ({ src: base + 3 * k, dst: base + 3 * k + 1, amt: base + 3 * k + 2, n: (base - ID.P_M1SRC) / 3 + k + 1 })); }

export const engineLabel = (e) => L.ENG[e]?.name ?? "?";
// the engines one can pick, in the unit's order; a part that already plays another (DRUM, a retired one) lists it too
export const pickable = (cur) => (L.ORDER.includes(cur) ? L.ORDER : [...L.ORDER, cur]);
export const CATEGORIES = ["-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "DRUM", "FX", "OTHER"];
