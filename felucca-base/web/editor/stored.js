// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor (0.2.25): the unit's stored MULTIS and PATTERN SLOTS as the editor reaches them, through the backup objects (EDITOR_PROTOCOL.md
// "v7: full backup": BACKUP_LIST / GET / PUT; there is no command of their own). A LIST stops playback on the unit; a PUT is checked by the unit
// (shape, version, CRC) and written power-cut safe; the partner records of an object are copied unchanged.
//   multis: ids 13..20, two to an object (magic MUL1, u16 lengths, record 1, record 2; multi.c). A record is the whole multi (both parts, steps,
//           effects, End B seeds, mode, name at byte 8); its first byte is its version (the firmware loads only its own: 6 in 0.2.22 .. 0.2.25).
//   patterns: ids 21..23, 11 + 11 + 10 slots (magic PTS3: compact objects of the unit's own, patslot.js; patslot.c). The editor's record of one slot is 596 bytes: used 0xA7, version 2,
//           LEN DIV SWING GATE QUANTIZE, flags, name 12, 64 x 9-byte steps
// Files on the computer: "duell-multis" (records whole, base64) and "duell-patterns" (records whole). No DOM.
import { BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc } from "../fm1backup.js";
import { CMD, v14, parseStep, stepBytes } from "./proto.js";
import { ID } from "./layout.js";
import { UnitError } from "./unit.js";
import { checkObject, MULTI_VERSION } from "./duellbackup.js";
import { PS as PSF, parsePatObject, buildPatObject, nslotOf, recordProblem } from "./patslot.js";

const u16 = (b, o) => b[o] | b[o + 1] << 8;
const text = (b, from, n) => { let s = ""; for (let i = 0; i < n && b[from + i]; i++) s += String.fromCharCode(b[from + i]); return s; };
const b64 = (bytes) => { let s = ""; for (const x of bytes) s += String.fromCharCode(x); return btoa(s); };
const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
export const M_LABEL = (s) => `M${String(s + 1).padStart(2, "0")}`, P_LABEL = (s) => `P${String(s + 1).padStart(2, "0")}`;

// ---- the backup objects
export async function listObjects(u) {
  const l = await u.req(BACKUP_CMD.LIST, [], 4000);
  if (l[0] !== 1 || l[1] !== 0) throw new UnitError(l[1] === 3 ? "The unit is playing. Stop playback on the unit first." : `The unit refused the list (error ${l[1]}).`, l[1]);
  const sizes = new Map(); for (let i = 0; i < l[2]; i++) sizes.set(l[3 + i * 11], bkR32(l, 3 + i * 11 + 1));
  return sizes;
}
export async function readObjects(u, ids, listed = null) {
  const sizes = listed ?? await listObjects(u), out = new Map();
  for (const id of ids) {
    const size = sizes.get(id) ?? 0, b = new Uint8Array(size);
    for (let off = 0; off < size; off += 256) {
      const c = Math.min(256, size - off), a = await u.req(BACKUP_CMD.GET, [id, ...bkU32(off), c & 127, c >> 7], 2500);
      if (a[1]) throw new UnitError(`The unit refused to read its memory (error ${a[1]}).`, a[1]);
      b.set(bkUnpack(a.slice(9), c), off);
    }
    out.set(id, b);
  }
  return out;
}
export async function writeObject(u, id, b) {
  u.guard();
  const why = checkObject(id, b); if (why) throw new UnitError(`The editor built a record the unit would refuse (${why}). Nothing was written.`);
  const put = async (args) => { const a = await u.req(BACKUP_CMD.PUT, args, 8000); if (a[2]) throw new UnitError(a[2] === 2 ? "The unit refused the data (it is not in the form this firmware stores)." : a[2] === 3 ? "The unit is playing. Stop playback on the unit first." : `The unit refused the write (error ${a[2]}).`, a[2]); };
  await put([0, id, ...bkU32(b.length), ...bkU32(b.length ? bkCrc(b) : 0)]);
  try {
    for (let off = 0; off < b.length; off += 256) await put([1, id, ...bkU32(off), ...bkPack(b.subarray(off, off + 256))]);
    await put([2, id]);
  } catch (e) { await u.req(BACKUP_CMD.PUT, [3, id]).catch(() => {}); throw e; }
}

// ---- multis
export function multiRecord(obj, slot) {                    // slot 0..15 -> the record's bytes, or null (not stored: an empty or factory slot)
  if (!obj || obj.length < 8) return null;
  const l = [u16(obj, 4), u16(obj, 6)], h = slot & 1;
  return l[h] ? obj.slice(8 + (h ? l[0] : 0), 8 + (h ? l[0] : 0) + l[h]) : null;
}
export function withMultiRecord(obj, slot, rec) {           // the object with record `slot & 1` replaced (rec null: emptied)
  const recs = [0, 1].map((h) => multiRecord(obj, (slot & ~1) | h) ?? new Uint8Array(0));
  recs[slot & 1] = rec ?? new Uint8Array(0);
  if (!recs[0].length && !recs[1].length) return new Uint8Array(0);
  const b = new Uint8Array(8 + recs[0].length + recs[1].length);
  b.set([0x4D, 0x55, 0x4C, 0x31, recs[0].length & 255, recs[0].length >> 8, recs[1].length & 255, recs[1].length >> 8]); b.set(recs[0], 8); b.set(recs[1], 8 + recs[0].length);
  return b;
}
export const multiName = (rec) => text(rec, 8, 12);
export function setMultiName(rec, name) { const r = rec.slice(); for (let i = 0; i < 12; i++) r[8 + i] = i < name.length ? name.charCodeAt(i) & 127 : 0; return r; }
// why a record cannot go into this unit (null: it can)
export function multiProblem(rec) {
  if (!rec || rec.length < 20 || rec.length > 1916) return "it is not a multi record";
  if (rec[0] !== MULTI_VERSION) return `it was stored by a firmware with multi version ${rec[0]}; this firmware loads version ${MULTI_VERSION} only`;
  if (rec[1] !== 127) return "it was stored by a firmware with another number of parameters";
  for (let i = 0; i < 12; i++) if (rec[8 + i] && (rec[8 + i] < 32 || rec[8 + i] > 126)) return "its name is damaged";
  return null;
}
export async function readMultis(u) {                       // -> { objs: Map id -> bytes, records: [rec | null] x16 }
  const objs = await readObjects(u, [13, 14, 15, 16, 17, 18, 19, 20]);
  return { objs, records: Array.from({ length: 16 }, (_, s) => multiRecord(objs.get(13 + (s >> 1)), s)) };
}
// a record into slot (the object read again first: the partner multi as the unit holds it now)
export async function importMulti(u, slot, rec, name = null) {
  const why = multiProblem(rec); if (why) throw new UnitError(`This multi cannot go into this unit: ${why}.`);
  if (name) rec = setMultiName(rec, name);
  const { objs } = await readMultis(u), id = 13 + (slot >> 1);
  await writeObject(u, id, withMultiRecord(objs.get(id), slot, rec));
}
export const MULTI_FORMAT = "duell-multis";
export function multiFile(list, firmware = "") {           // list: [{slot, bytes}]
  return { format: MULTI_FORMAT, version: 1, firmware, created: new Date().toISOString(),
    multis: list.map(({ slot, bytes }) => ({ slot: slot + 1, label: M_LABEL(slot), name: multiName(bytes), version: bytes[0], size: bytes.length, crc: bkCrc(bytes), data: b64(bytes) })) };
}
export function parseMultiFile(text) {                      // -> { ok, error | multis: [{slot (0-based, from the file), name, bytes, problem}] }
  let f; try { f = typeof text === "string" ? JSON.parse(text) : text; } catch { return { ok: false, error: "This is not a multi file (it is not valid JSON)." }; }
  if (!f || f.format !== MULTI_FORMAT || !Array.isArray(f.multis)) return { ok: false, error: "This is not a DUELL multi file." };
  if (f.version !== 1) return { ok: false, error: `This multi file is version ${f.version}; this editor reads version 1.` };
  const multis = [];
  for (const m of f.multis) {
    let bytes; try { bytes = unb64(String(m.data)); } catch { multis.push({ slot: null, name: String(m.name || "?"), bytes: null, problem: "damaged in the file" }); continue; }
    if (bytes.length !== m.size || bkCrc(bytes) !== m.crc) { multis.push({ slot: null, name: String(m.name || "?"), bytes: null, problem: "damaged in the file (checksum)" }); continue; }
    multis.push({ slot: Number.isInteger(m.slot) ? m.slot - 1 : null, name: multiName(bytes), bytes, problem: multiProblem(bytes) });
  }
  return { ok: true, firmware: String(f.firmware || ""), multis };
}

// ---- pattern slots (P01..P32): ONE list for the whole device (owner A8: 32 slots, none empty, nothing protected). The unit holds them in the backup objects 21..23 (11, 11 and 10 slots,
// magic PTS3, a compact variable-length object: patslot.js); PS.PER is the object header's byte 7. The unit reports all 32 slots, each with a pattern (its defaults until the user saves over them).
const PS = { REC: PSF.REC, USED: PSF.USED, VER: PSF.VER, PER: PSF.PER };
const PAT_IDS = [21, 22, 23];
export const PATTERN_SLOTS = PSF.SLOTS;
const perOf = (objs) => { const b = objs.get(PAT_IDS[0]); return b && b.length >= 8 && PSF.MAGIC.every((x, i) => b[i] === x) ? b[7] : PS.PER; };   // the slots an object holds, from the header
export function patternRecords(objs) {               // -> the 596-byte records of every slot the objects hold, in slot order (null for a slot the objects cannot show)
  const out = [];
  PAT_IDS.forEach((id, o) => { const r = parsePatObject(o, objs.get(id)); for (let i = 0; i < (r.recs ? r.recs.length : nslotOf(o)); i++) out.push(r.recs ? r.recs[i] : null); });
  return out;
}
export function patternSlotCount(objs) { return PAT_IDS.every((id, o) => !parsePatObject(o, objs.get(id)).error) ? Math.min(PATTERN_SLOTS, patternRecords(objs).length) : 0; }
export function patternRecord(objs, slot) { const per = perOf(objs), o = Math.floor(slot / per), r = PAT_IDS[o] != null ? parsePatObject(o, objs.get(PAT_IDS[o])) : null; return r?.recs?.[slot % per] ?? null; }
export function withPatternRecord(obj, slot, rec, per = PS.PER) {   // object `slot / per` with the slot's record replaced -> bytes, or null when the pattern does not fit the object
  const o = Math.floor(slot / per), r = parsePatObject(o, obj);
  if (r.error) throw new UnitError(`The editor cannot change this pattern object (${r.error}).`);
  r.recs[slot % per] = rec; return buildPatObject(o, r.recs);
}
// one step (proto.js parseStep's form) <-> 9 bytes (patslot.c ps_pack_step / ps_unpack_step: NUDGE in bit 7 of the notes, RATCHET in bit 7 of VEL and CHANCE)
export function packStep(s) {
  const r = Math.max(1, Math.min(4, s.ratchet | 0 || 1)) - 1, nd = ((s.nudge ?? 8) - 8) & 15, b = new Uint8Array(9);
  for (let j = 0; j < 4; j++) b[j] = (s.notes[j] & 127) | ((nd >> j) & 1) << 7;
  b[4] = Math.min(s.n, 4) | Math.min(s.time, 2) << 3 | (s.flags & 3) << 5;
  b[5] = (s.vel & 127) | (r & 1) << 7; b[8] = (s.chance > 101 ? 0 : s.chance & 127) | (r >> 1) << 7;
  return b;
}
export function unpackStep(b, index) {
  let nd = 0; const notes = [];
  for (let j = 0; j < 4; j++) { nd |= (b[j] >> 7) << j; notes.push(b[j] & 127); }
  const r = (b[5] >> 7) | (b[8] >> 7) << 1;
  return { index, n: b[4] & 7, notes, time: (b[4] >> 3) & 3, flags: (b[4] >> 5) & 3, vel: b[5] & 127, hits: [0, 0, 0], chance: b[8] & 127, ratchet: r + 1, nudge: ((nd ^ 8) - 8) + 8 };
}
export function decodePattern(rec) {                        // -> { name, len, div, swing, gate, qnt, steps[64] }
  return { name: text(rec, 8, 12), len: rec[2], div: rec[3], swing: rec[4], gate: rec[5], qnt: rec[6], steps: Array.from({ length: 64 }, (_, i) => unpackStep(rec.subarray(20 + 9 * i, 29 + 9 * i), i)) };
}
export function encodePattern({ name, len, div, swing, gate, qnt, steps }) {
  const r = new Uint8Array(PS.REC);
  r.set([PS.USED, PS.VER, len, div, swing, gate, qnt ? 1 : 0, 1]);   // (byte 7: CHANGED, a slot saved by the user; the unit's ERASE gives the default back)
  const nm = String(name || "").replace(/[^ -~]/g, "").slice(0, 12); for (let i = 0; i < nm.length; i++) r[8 + i] = nm.charCodeAt(i);
  steps.forEach((s, i) => r.set(packStep(s), 20 + 9 * i));
  return r;
}
export async function readPatterns(u) {                     // -> { objs, slots: [{slot, name, len}] x what the unit reports (32 slots, each with a pattern; none when the firmware has the old 16) }
  const listed = await listObjects(u), objs = await readObjects(u, PAT_IDS, listed), recs = patternSlotCount(objs) ? patternRecords(objs) : [];
  return { objs, slots: recs.map((r, s) => ({ slot: s, name: text(r, 8, 12), len: r[2] })) };
}
// slot -> part: the 64 steps and LEN DIV SWING GATE QUANTIZE (patslot.c ps_load_trk: the sound stays). rec: a record (from the unit or a file)
export async function recallPattern(u, part, rec, onProgress = () => {}) {
  u.guard();
  const p = decodePattern(rec);
  for (const s of p.steps) { await u.req(CMD.TRACK_STEP, [part, s.index, ...stepBytes(s)]); onProgress(s.index + 1, 64); }
  for (const [id, v] of [[ID.P_SLEN, p.len], [ID.P_SDIV, p.div], [ID.P_SSWING, p.swing], [ID.P_SGATE, p.gate], [ID.P_SQNT, p.qnt]]) { await u.req(CMD.TRACK_PARAM, [part, id, ...v14(v)]); u.set(part, id, v, true); }
  u.emit("reload", { part });
}
// part -> a record (read from the unit: its 64 steps and the pattern settings)
export async function patternOfPart(u, part, name) {
  const steps = []; for (let i = 0; i < 64; i++) steps.push(parseStep(await u.req(CMD.TRACK_STEP, [part, i]), 1));
  const p = u.parts[part].p;
  return encodePattern({ name, len: p[ID.P_SLEN], div: p[ID.P_SDIV], swing: p[ID.P_SSWING], gate: p[ID.P_SGATE], qnt: p[ID.P_SQNT], steps });
}
export async function storePattern(u, slot, rec) {          // the slot gets the record (a slot is never emptied: the unit's ERASE gives its default back)
  if (!rec) throw new UnitError("A pattern slot cannot be empty. Nothing was written.");
  const { objs } = await readPatterns(u), id = PAT_IDS[Math.floor(slot / perOf(objs))];
  if (!id) throw new UnitError("This unit does not report the 32 pattern slots (its firmware is older than this editor).");
  const b = withPatternRecord(objs.get(id), slot, rec, perOf(objs));
  if (!b) throw new UnitError("This pattern does not fit the room the unit has for the slots of its group (about eleven patterns of 64 steps with chords). Make it shorter or replace a long one. Nothing was written.");
  await writeObject(u, id, b);
}
export const PATTERN_FORMAT = "duell-patterns";
export function patternFile(list, firmware = "") {         // list: [{slot, bytes}]
  return { format: PATTERN_FORMAT, version: 1, firmware, created: new Date().toISOString(),
    patterns: list.map(({ slot, bytes }) => ({ slot: slot + 1, label: P_LABEL(slot), name: text(bytes, 8, 12), length: bytes[2], crc: bkCrc(bytes), data: b64(bytes) })) };
}
export function parsePatternFile(t) {
  let f; try { f = typeof t === "string" ? JSON.parse(t) : t; } catch { return { ok: false, error: "This is not a pattern file (it is not valid JSON)." }; }
  if (!f || f.format !== PATTERN_FORMAT || f.version !== 1 || !Array.isArray(f.patterns)) return { ok: false, error: "This is not a DUELL pattern file." };
  const patterns = [];
  for (const p of f.patterns) {
    let bytes = null; try { bytes = unb64(String(p.data)); } catch { /* damaged */ }
    const ok = bytes && bytes.length === PS.REC && bkCrc(bytes) === p.crc && !recordProblem(bytes);
    patterns.push({ slot: Number.isInteger(p.slot) ? p.slot - 1 : null, name: ok ? text(bytes, 8, 12) : String(p.name || "?"), bytes: ok ? bytes : null, problem: ok ? null : "damaged in the file" });
  }
  return { ok: true, patterns };
}
