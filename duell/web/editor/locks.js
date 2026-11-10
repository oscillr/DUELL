// SPDX-License-Identifier: GPL-3.0-only
// DUELL 1.1 editor: the PARAMETER LOCKS tab's model (owner, 2026-10-25, HANDOFF_V2 item 69). The sequencer's records (MOTION: a LOCK sounds on its step only, an AUTO event holds until the next one) name
// exactly the matrix destinations. This file maps a destination to the target id the unit records (firmware mod.c mod_lock_id, EDITOR_PROTOCOL.md "LOCK TARGETS past 127") and back, and names it
// in the unit's words (mod.c mod_dst_line). No DOM, no I/O.
import { L, ID, P_E0, descOf, xdescOf, dstOffered, dlyMode, twinComb, engineLabel, lockKind, lockTarget } from "./layout.js";
import { longDesc, MATRIX_GROUPS, GROUP_WORD, INSERT_LONG } from "./labels.js";

// destination -> the part's stored parameter (mod.c LOCK_ID); the destinations of lockKind "virtual" / "extension" come from layout.js lockTarget, the engine slots E1..E8 are P_E0 + d - E1
const PID = { 2: ID.P_FREEA, 5: ID.P_PAN, 6: 33, 7: 34, 8: 35, 9: 36, 10: ID.P_LRATE, 11: 13, 21: ID.P_L2RATE, 23: ID.P_TRIM, 24: ID.P_IA, 25: ID.P_IA + 1, 26: ID.P_IA + 2, 27: ID.P_IMIX, 31: ID.P_DKDEP,
  32: ID.P_E2ATK, 33: ID.P_E2DEC, 34: ID.P_E2SUS, 35: ID.P_E2REL, 36: ID.P_E2FLT, 38: 46, 46: ID.P_FREEC, 47: ID.P_ATK, 48: ID.P_DEC, 49: ID.P_SUS, 50: ID.P_REL, 51: ID.P_CUT2, 52: ID.P_RES2, 53: ID.P_DRV2, 54: ID.P_BLEND };
export const SUB_SLOT = 6;                                       // SUB LEVEL is extension slot 6: target id 134
// the target id of matrix destination d (0: not lockable: OFF, the LFO depths, the EQ gains, FILTER ENV TIMBRE)
export function targetOfDst(d) {
  const M = L.MD;
  if (d <= 0) return 0;
  if (d >= M.E1 && d < M.E1 + 8) return P_E0 + d - M.E1;
  const k = lockKind(d);
  return k === "virtual" || k === "extension" ? lockTarget(d) : PID[d] ?? 0;
}
const REV = new Map(); for (let d = 1; d < 57; d++) { const t = targetOfDst(d); if (t) REV.set(t, d); }
export const dstOfTarget = (tid) => REV.get(tid) ?? 0;
// the matrix's own order (the editor's lists: MATRIX_GROUPS.dst), the lockable destinations only
export const DST_ORDER = MATRIX_GROUPS.dst.flatMap(([, vs]) => vs).filter((d) => targetOfDst(d));
export const orderOfTarget = (tid) => { const i = DST_ORDER.indexOf(dstOfTarget(tid)); return i < 0 ? 999 : i; };
// the destinations offered for this part now (the unit's mod_lock_ok = the matrix's offer rule); a unit without the wide tag cannot lock the ones past 127
export const offeredDsts = (part, g, wide) => DST_ORDER.filter((d) => dstOffered(part, d, g) && (wide || targetOfDst(d) < 128));

// the destination names in full, as the unit's DEST line says them (mod.c mod_dst_line FULL)
const FULL = ["NONE", "PITCH", "MULTI CUTOFF", "TIMBRE", "AMP LEVEL", "PAN", "SATURATION", "CHORUS SEND", "DELAY SEND", "REVERB SEND", "LFO 1 RATE", "LFO 1 DEPTH", "", "", "", "", "", "", "", "",
  "LFO 1 DEPTH", "LFO 2 RATE", "LFO 2 DEPTH", "PATCH LEVEL", "", "", "", "INSERT MIX", "EQ LOW GAIN", "EQ MID GAIN", "EQ HIGH GAIN", "DUCK DEPTH", "FILTER ENV ATTACK", "FILTER ENV DECAY", "FILTER ENV SUSTAIN",
  "FILTER ENV RELEASE", "FILTER ENV MULTI CUTOFF", "FILTER ENV TIMBRE", "MULTI RES", "CHORUS RATE", "CHORUS DEPTH", "DELAY FEEDBACK", "DELAY MOD DEPTH", "DELAY MOD RATE", "REVERB SIZE", "REVERB DAMPING", "MULTI MODE",
  "AMP ENV ATTACK", "AMP ENV DECAY", "AMP ENV SUSTAIN", "AMP ENV RELEASE", "TWIN CUTOFF", "TWIN RES", "FILTER DRIVE", "FILTER BLEND", "SUB LEVEL", "SPACE"];
export function dstName(part, d, g) {
  const M = L.MD;
  if (d >= M.E1 && d < M.E1 + 8) { const x = descOf(part, P_E0 + d - M.E1); return !x.label || x.label[0] === "-" ? "-" : `${engineLabel(part.eng)} ${longDesc(x, `e:${engineLabel(part.eng)}`).label}`; }   // (the unit: the engine's name, then the knob's)
  if (d >= M.INSA && d < M.INSA + 3) { const x = descOf(part, ID.P_IA + d - M.INSA); return part.p[ID.P_ITYPE] ? `INSERT ${INSERT_LONG[x.label] ?? x.label}` : `INSERT ${"ABC"[d - M.INSA]}`; }
  if (d === M.BUS + 3 && g && [2, 3].includes(dlyMode(g))) return "DELAY CHANCE";
  if (M.CUT2 != null && twinComb(part) && d === M.CUT2) return "TWIN TUNE";
  if (M.CUT2 != null && twinComb(part) && d === M.RES2) return "TWIN FEEDBACK";
  return FULL[d] ?? String(d);
}
// the same name for a picker that already shows the group heading: the AMP ENV and FILTER ENV words are in the heading (as the matrix lists do)
export const dstNameInGroup = (part, d, g) => { const n = dstName(part, d, g), w = GROUP_WORD[d]; return w && n.startsWith(w) ? n.slice(w.length) : n; };

// the value of a row, as the unit's list shows it: a virtual target is an AMOUNT (signed percent, -64..+63), the others are the parameter's own value
export const VIRT_DESC = { label: "AMOUNT", fmt: 2, min: -64, max: 63, def: 0, fk: 3, unit: "", names: null, order: null };
export function valueDesc(part, tid) {
  if (tid >= 192) return VIRT_DESC;
  if (tid === 128 + SUB_SLOT) return longDesc(xdescOf(part, SUB_SLOT), `x${SUB_SLOT}`);
  return longDesc(descOf(part, tid), tid >= P_E0 ? `e:${engineLabel(part.eng)}` : tid);
}
// the value a new lock starts with: the sound's own (a lock that changes nothing yet); 0 for an amount
export function ownValue(part, tid) {
  const d = valueDesc(part, tid);
  const v = tid >= 192 ? 0 : tid === 128 + SUB_SLOT ? part.x?.[SUB_SLOT] ?? d.def : part.p[tid];
  return Math.min(d.max, Math.max(d.min, v ?? d.def));
}
// the descriptor of a step number 1..max (shown 1-based; a record is 0-based)
const stepMemo = new Map();
export const stepDesc = (max) => { max = Math.min(64, Math.max(1, max)); if (!stepMemo.has(max)) stepMemo.set(max, { label: "STEP", fmt: 1, min: 1, max, def: 1, fk: -1, unit: "", names: null, order: null }); return stepMemo.get(max); };
