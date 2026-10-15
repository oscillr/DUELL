// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: a patch slot's STORED RECORD (firmware/src/upreset.c, "Chunk ... Record"): read, built, described. The record is the unit's own form; PATCH_RAW
// (EDITOR_PROTOCOL.md, cmd 84) moves it whole, so a patch survives a trip through a file bit for bit. Little endian:
//   0 u16 length   2 slot (the unit sets it)   3 revision (the unit sets it)   4 flags   5 engine   6 category (0 none)   7 name, 12 bytes, 0-padded
//   19 number of values   20 [SH 2 bytes, retired] [MB 7 bytes, End B] values: pairs (id, value + 64) or, with BM, a 16 byte bitmap and one byte each
//   [PAT 32: 16 notes, 16 flags] [FM6 112: the 128 7-bit bytes of the FM6 patch packed 8 to 7] [AK: u16 length, then the arp / keys / sequencer block]
// Only what differs from the engine's INIT is stored (sparse); five ids have the default 0 in the store (upreset.c up_def). No DOM, no I/O.
import { L, P_E0, descOf, valueText } from "./layout.js";

export const PTF = { PAT: 1, TR: 2, FM6: 4, MB: 8, SH: 16, AK: 32, BM: 64, CB: 128, ALL: 255 };   // TR (0.2.19): the loudness is the pair of P_TRIM and the record carries no mixer level (P_LEVEL); a record without it (0.2.14 .. 0.2.18) held it as P_LEVEL: read as the trim
export const FIX = 20, WORST = 440;                 // the fixed part; a safe upper bound on a record (the unit's PT_WORST is smaller: 435)
export const CATEGORIES = ["-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "DRUM", "FX", "OTHER"];
const STORE_ZERO = new Set([45, 48, 84, 92, 105]);   // CUT, SLOPE, LFO 2 TRIG, LFO 1 TRIG, DUCK CURVE: 0 in the store (the editor's defaults are 82, 1, 1, 1, 1)
const AK_IDS = [17, 18, 19, 20, 21, 22, 23, 24, 26, 81, 82, 29, 30, 31, 32, 94];   // the ordinals 0..15: ARP x8, SCALE (retired), CHRD, VOIC, LEN DIV SWG GATE QNT
const AK_GROUP = (o) => (o < 8 ? 1 : o < 11 ? 2 : 4);

export const engineIndex = (name) => L.ENG.findIndex((e) => e.name === name);
const P_TRIM = L.ID?.P_TRIM ?? 109, P_LEVEL_STORE_DEF = 104;   // id 0 is the mixer level (default 112) but the store's default for it is the old LEVEL's 104 (upreset.c up_def); a record never writes it now
export function storeDefault(eng, id) {
  if (id === 0) return P_LEVEL_STORE_DEF;
  if (STORE_ZERO.has(id)) return 0;
  return id >= P_E0 ? (L.ENG[eng]?.edit[id - P_E0]?.def ?? 0) : L.TP[id].def;
}
export const storeDefaults = (eng) => Array.from({ length: L.P_COUNT }, (_, i) => storeDefault(eng, i));

// the FM6 patch: 128 seven-bit bytes <-> the 112 bytes of the record (upreset.c upf_pk_put / upf_pk_get)
export function packPatch(p128) {
  const out = new Uint8Array(112); let acc = 0, n = 0, o = 0;
  for (let i = 0; i < 128; i++) { acc |= (p128[i] & 127) << n; n += 7; while (n >= 8) { out[o++] = acc & 255; acc >>>= 8; n -= 8; } }
  return out;
}
export function unpackPatch(p112) {
  const out = new Uint8Array(128); let acc = 0, n = 0, o = 0;
  for (let i = 0; i < 128; i++) { while (n < 7) { acc |= p112[o++] << n; n += 8; } out[i] = acc & 127; acc >>>= 7; n -= 7; }
  return out;
}

const rd16 = (b, o) => b[o] | b[o + 1] << 8;
function layout(b) {                                // where the optional parts start (upreset.c pt_layout_x); total = the length the flags imply
  const f = b[4]; let o = FIX; const at = {};
  at.sh = o; if (f & PTF.SH) o += 2;
  at.mb = o; if (f & PTF.MB) o += 7;
  at.pr = o; o += f & PTF.BM ? 16 + b[19] : 2 * b[19];
  at.pat = o; if (f & PTF.PAT) o += 32;
  at.fm = o; if (f & PTF.FM6) o += 112;
  at.ak = o;
  if (f & PTF.AK) { if (o + 2 > rd16(b, 0)) return { at, total: o + 65536 }; o += 2 + rd16(b, o); }
  return { at, total: o };
}
const nameOf = (b, o) => { let s = ""; for (let i = 0; i < 12 && b[o + i]; i++) s += String.fromCharCode(b[o + i]); return s; };

// bytes -> { ok, error } or the record decoded: { ok, len, slot, rev, flags, engine, cat, name, values[P_COUNT], pairs, mb, pat, fm6 (128 bytes) | null, ak | null }
export function parseRecord(bytes) {
  const b = bytes instanceof Uint8Array ? bytes : Uint8Array.from(bytes), bad = (error) => ({ ok: false, error });
  if (b.length < FIX || b.length > WORST) return bad("The record has the wrong size.");
  if (rd16(b, 0) !== b.length) return bad("The record's length field does not match its size.");
  if (b[4] & ~PTF.ALL) return bad("The record has flags this editor does not know.");
  if (!(b[4] & PTF.CB)) return bad("The record is in the form of a firmware before 0.2.14, which no longer loads.");
  if (b[5] >= L.NENGINES) return bad("The record names an engine that does not exist.");
  if (!b[7]) return bad("The record has no name.");
  const { at, total } = layout(b);
  if (total !== b.length) return bad("The record's parts do not add up to its length.");
  const eng = b[5], values = storeDefaults(eng), pairs = new Map(), flags = b[4];
  if (flags & PTF.BM) {
    let j = 0;
    for (let i = 0; i < 128; i++) if (b[at.pr + (i >> 3)] >> (i & 7) & 1) {
      if (i >= L.P_COUNT) return bad("The record stores a parameter that does not exist.");
      const v = b[at.pr + 16 + j++] - 64; values[i] = v; pairs.set(i, v);
    }
    if (j !== b[19]) return bad("The record's value count does not match its bitmap.");
  } else for (let i = 0; i < b[19]; i++) {
    const id = b[at.pr + 2 * i], v = b[at.pr + 2 * i + 1] - 64;
    if (id < L.P_COUNT) { values[id] = v; pairs.set(id, v); }
  }
  if (!(flags & PTF.TR)) { values[P_TRIM] = values[0]; pairs.set(P_TRIM, values[0]); values[0] = P_LEVEL_STORE_DEF; pairs.delete(0); }   // a record before 0.2.19: its LEVEL was the sound's loudness
  for (const v of pairs.values()) if (v > 127) return bad("The record stores a value out of range.");
  const out = { ok: true, len: b.length, slot: b[2], rev: b[3], flags, engine: eng, cat: b[6], name: nameOf(b, 7), values, pairs,
    mb: flags & PTF.MB ? b.slice(at.mb, at.mb + 7) : null,
    pat: flags & PTF.PAT ? { note: b.slice(at.pat, at.pat + 16), flags: b.slice(at.pat + 16, at.pat + 32) } : null,
    fm6: flags & PTF.FM6 && eng === engineIndex("FM6") ? unpackPatch(b.subarray(at.fm, at.fm + 112)) : null, ak: null };
  if (flags & PTF.AK) {                              // the block: groups present, their values (the other values of a present group are the engine's defaults), the steps
    const c = b.subarray(at.ak + 2, at.ak + 2 + rd16(b, at.ak)); let p = 2, g = c[0], vals = new Map(), nst = 0, ok = c.length >= 2 && !(g & ~15) && (g & 7);
    if (ok) {
      const grp = g & 7, def = (o) => (o === 8 ? 0 : (descOf({ eng, p: values }, AK_IDS[o])?.def ?? 0));
      for (let o = 0; o < 16; o++) if (AK_GROUP(o) & grp) vals.set(AK_IDS[o], def(o));
      if (g & 8) { const bm = c[1] | c[2] << 8; p = 3; for (let o = 0; o < 16; o++) if (bm >> o & 1) vals.set(AK_IDS[o], c[p++] - 64); }
      else for (let i = 0; i < c[1]; i++, p += 2) vals.set(c[p], c[p + 1] - 64);
      if (grp & 4) { const map = c[p] | c[p + 1] << 8; p += 2; for (let i = 0; i < 16; i++) if (map >> i & 1) nst++; p += 9 * nst; }
      ok = p === c.length;
    }
    if (ok) { out.ak = { groups: g & 7, values: vals, steps: nst }; for (const [id, v] of vals) if (id !== 26) out.values[id] = v; }
  }
  return out;
}

// the record for a sound. values: P_COUNT numbers (each -64..127); mb: 7 bytes or null; pat {note[16], flags[16]} or null; patch128: the FM6 patch (128 seven-bit
// bytes) for an FM6 sound; ak: the block bytes (u16 length first) or null. Slot and revision are 0: the unit sets them when it writes.
export function buildRecord({ engine, name, cat = 0, values, mb = null, pat = null, patch128 = null, ak = null }) {
  const f0 = engine === engineIndex("FM6") && patch128 ? PTF.FM6 : 0;
  const nm = [...String(name)].map((c) => c.charCodeAt(0)).filter((c) => c >= 32 && c <= 126).slice(0, 12);
  if (!nm.length) throw new Error("A patch needs a name (1 to 12 plain characters).");
  const ids = [], vs = [];
  for (let i = 0; i < L.P_COUNT; i++) {
    const v = i === 0 ? P_LEVEL_STORE_DEF : values[i]; if (!Number.isInteger(v) || v < -64 || v > 127) throw new Error(`Parameter ${i} has a value the store cannot hold (${v}).`);
    if (v !== storeDefault(engine, i)) { ids.push(i); vs.push(v + 64); }
  }
  let f = f0 | PTF.TR | PTF.CB | (mb && mb.some((x) => x) ? PTF.MB : 0) | (pat ? PTF.PAT : 0) | (ak ? PTF.AK : 0) | (ids.length > 16 ? PTF.BM : 0);
  const parts = [], head = new Uint8Array(FIX);
  head[4] = f; head[5] = engine; head[6] = cat; nm.forEach((c, i) => { head[7 + i] = c; }); head[19] = ids.length;
  parts.push(head);
  if (f & PTF.MB) parts.push(Uint8Array.from(mb));
  if (f & PTF.BM) { const bm = new Uint8Array(16); for (const i of ids) bm[i >> 3] |= 1 << (i & 7); parts.push(bm, Uint8Array.from(vs)); }
  else parts.push(Uint8Array.from(ids.flatMap((id, j) => [id, vs[j]])));
  if (f & PTF.PAT) parts.push(Uint8Array.from([...pat.note, ...pat.flags]));
  if (f & PTF.FM6) parts.push(packPatch(patch128));
  if (f & PTF.AK) parts.push(Uint8Array.from(ak));
  const len = parts.reduce((s, p) => s + p.length, 0), out = new Uint8Array(len); let o = 0;
  for (const p of parts) { out.set(p, o); o += p.length; }
  out[0] = len & 255; out[1] = len >> 8;
  return out;
}

// ---- the readable dump (for diffing): what differs from the engine's defaults, named as the editor's pages name it
let NAMES = null;
function paramNames() {
  if (NAMES) return NAMES;
  const where = new Map();
  for (const pg of L.PAGES) if (pg.scope === "TRACK") pg.ids.forEach((id) => { if (id >= 0 && !where.has(id)) where.set(id, pg.title); });
  const seen = new Set(); NAMES = [];
  for (let id = 0; id < L.P_COUNT; id++) {
    let n = id >= P_E0 ? "ENGINE" : `${where.get(id) ?? "PART"} ${L.TP[id]?.label ?? id}`;
    if (id >= P_E0) n = `ENGINE E${id - P_E0 + 1}`;
    if (seen.has(n)) n += ` #${id}`;
    seen.add(n); NAMES.push(n);
  }
  return NAMES;
}
export function describeRecord(r) {                  // -> { name, engine, category, flags[], params: { "MULTI FILTER CUT": "71 (6.8 kHz)" ... } }
  const names = paramNames(), eng = L.ENG[r.engine], part = { eng: r.engine, p: r.values }, params = {}, defs = storeDefaults(r.engine);
  for (let id = 0; id < L.P_COUNT; id++) {
    if (r.values[id] === defs[id] && !(r.ak && r.ak.values.has(id))) continue;
    const d = id >= P_E0 ? (eng?.edit[id - P_E0] ? descOf(part, id) : null) : L.TP[id];
    let key = names[id];
    if (id >= P_E0 && d?.label) key = `${eng.name} ${d.label}`;
    const t = d ? valueText(d, r.values[id]) : { val: String(r.values[id]), unit: "" };
    params[key] = `${r.values[id]}${t.val !== String(r.values[id]) || t.unit ? ` (${t.val}${t.unit ? " " + t.unit : ""})` : ""}`;
  }
  const flags = ["PAT", "FM6", "MB", "AK"].filter((n) => r.flags & PTF[n]).map((n) => ({ PAT: "pattern", FM6: "FM6 patch", MB: "End B", AK: "arp/keys/sequencer" }[n]));
  return { name: r.name, engine: eng?.name ?? "?", category: CATEGORIES[r.cat] || "-", carries: flags, params };
}
