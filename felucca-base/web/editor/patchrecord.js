// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: a patch slot's STORED RECORD (firmware/src/upreset.c, "Chunk ... Record"): read, built, described. The record is the unit's own form; PATCH_RAW
// (EDITOR_PROTOCOL.md, cmd 84) moves it whole, so a patch survives a trip through a file bit for bit. Little endian:
//   0 u16 length   2 slot (the unit sets it)   3 revision (the unit sets it)   4 flags   5 engine   6 category (0 none)   7 name, 12 bytes, 0-padded
//   19 number of values   20 [XB the extension block: L, XV, sections] [MB 7 bytes, End B] values: pairs (id, value + 64) or, with BM, a 16 byte bitmap and one byte each
//   [PAT 32: 16 notes, 16 flags] [FM6 112: the 128 7-bit bytes of the FM6 patch packed 8 to 7] [AK: u16 length, then the arp / keys / sequencer block]
// DUELL 1.0: the unit writes the AK block in EVERY record (the arp group and the keys group always; a patch with the arp off has the arp group with all eight values at their defaults,
// 4 bytes), and a record without it (written before 1.0, or an approximate copy by UP_PUT) loads as arp OFF, keys at their defaults: parseRecord reads both the same way (ak null = the defaults).
// Only what differs from the engine's INIT is stored (sparse); five ids have the default 0 in the store (upreset.c up_def). No DOM, no I/O.
import { L, P_E0, descOf, valueText } from "./layout.js";

export const PTF = { PAT: 1, TR: 2, FM6: 4, MB: 8, XB: 16, AK: 32, BM: 64, CB: 128, ALL: 255 };   // TR (0.2.19): the loudness is the pair of P_TRIM and the record carries no mixer level (P_LEVEL); a record without it (0.2.14 .. 0.2.18) held it as P_LEVEL: read as the trim
export const FIX = 20, WORST = 534;                 // the fixed part; the biggest record there can be: the unit's PT_WORST (the firmware refuses a longer one)
// the extension block (firmware/src/up_xb.h): [L: the bytes after it][XV 1][section]..., section = [tag][n data bytes][data]; tag 1 = the extension slots off their default
// (n/2 pairs: wide id 128 + k, value + 64), tag 2 = the hand-set B edits (n/2 pairs: wide id 0..126 or 128..255, value + 64; at most XB_MAX, each id once)
export const NX = 16, XB_MAX = 32, XB_VER = 1, XW_BASE = 128, XB_BLOCK_MAX = 2 + (2 + 2 * NX) + (2 + 2 * XB_MAX);   // 102
export const XB_DEFS = [64, 65, 64, 64, 64, 64, 64, 65, 64, 64, 64, 74, 134, 64, 64, 64];   // slot k's default is XB_DEFS[k] - 64 (up_xb.h XB_DEFS: TWIN LINK is ON, SUB OCTAVE is -1)
export const XB_NAMES = ["TWIN ROUTE", "TWIN LINK", "TWIN KEY TRK 2", "TWIN ENV 2", "TWIN LFO 1", "TWIN LFO 2", "SUB LEVEL", "SUB OCTAVE", "SUB WAVE", "SUB HARMONICS",
  "CYCLE MODE", "CYCLE RISE", "CYCLE FALL", "CYCLE CURVE", "CYCLE SYNC", "SPARE"];
export const CATEGORIES = ["-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "DRUM", "FX", "OTHER"];
const STORE_ZERO = new Set([27, 45, 48, 84, 92, 105, 111]);   // KEY TRK (0.2.22: the editor default is 32), CUT, SLOPE, LFO 2 TRIG, LFO 1 TRIG, DUCK CURVE, DUCK SRC (0.2.24: the editor default is OFF, 3; the store keeps PUMP, 0): 0 in the store (the editor defaults are 32, 82, 1, 1, 1, 1, 3)
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
  at.xb = o; if (f & PTF.XB) { if (o + 1 > rd16(b, 0)) return { at, total: o + 65536 }; o += 1 + b[o]; }   // (no byte for the block's length: a total that cannot be the record's)
  at.mb = o; if (f & PTF.MB) o += 7;
  at.pr = o; o += f & PTF.BM ? 16 + b[19] : 2 * b[19];
  at.pat = o; if (f & PTF.PAT) o += 32;
  at.fm = o; if (f & PTF.FM6) o += 112;
  at.ak = o;
  if (f & PTF.AK) { if (o + 2 > rd16(b, 0)) return { at, total: o + 65536 }; o += 2 + rd16(b, o); }
  return { at, total: o };
}
// The extension block at bytes[at] with avail bytes there (xb_block_ok, up_xb.h): { ok, error } or { ok, length (1 + L), ext: Map slot -> value (the slots it names), edits: [{wid, val}], unknownTags: [tag] }.
// A wide id of tag 1 outside 128 .. 128 + NX - 1 is skipped (a newer firmware's slot); the block is damaged (the record refused) in every case the firmware lists
export function parseXBlock(bytes, at = 0, avail = bytes.length - at) {
  const b = bytes, bad = (error) => ({ ok: false, error: `The extension block is damaged (${error}).` });
  if (avail < 2 || b[at] < 1 || 1 + b[at] > avail) return bad("its length");
  if (b[at + 1] !== XB_VER) return bad("version " + b[at + 1]);
  const end = at + 1 + b[at], ext = new Map(), edits = [], unknownTags = []; let p = at + 2; const seen = new Set();
  while (p < end) {
    if (p + 2 > end) return bad("a section is cut off");
    const tag = b[p], n = b[p + 1]; p += 2;
    if (p + n > end) return bad("a section runs past the block");
    if (tag === 1 || tag === 2) {
      if (seen.has(tag)) return bad(`tag ${tag} twice`);
      if (n & 1) return bad(`tag ${tag} has an odd length`);
      seen.add(tag);
      for (let i = 0; i < n; i += 2) if (b[p + i + 1] > 191) return bad(`tag ${tag} has a value out of range`);
      if (tag === 1) for (let i = 0; i < n; i += 2) { const w = b[p + i]; if (w >= XW_BASE && w < XW_BASE + NX) ext.set(w - XW_BASE, b[p + i + 1] - 64); }
      else {
        if (n / 2 > XB_MAX) return bad("more than 32 edits");
        const ids = new Set();
        for (let i = 0; i < n; i += 2) {
          if (b[p + i] === 127) return bad("wide id 127");
          if (ids.has(b[p + i])) return bad(`wide id ${b[p + i]} twice`);
          ids.add(b[p + i]); edits.push({ wid: b[p + i], val: b[p + i + 1] - 64 });
        }
      }
    } else unknownTags.push(tag);
    p += n;
  }
  return { ok: true, length: 1 + b[at], ext, edits, unknownTags };
}
const nameOf = (b, o) => { let s = ""; for (let i = 0; i < 12 && b[o + i]; i++) s += String.fromCharCode(b[o + i]); return s; };

// bytes -> { ok, error } or the record decoded: { ok, len, slot, rev, flags, engine, cat, name, values[P_COUNT], pairs, xv[NX] (the extension slots, their defaults where the record does not name them), edits [{wid, val}] ([] none), mb, pat, fm6 (128 bytes) | null, ak | null }
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
  const eng = b[5], values = storeDefaults(eng), pairs = new Map(), flags = b[4], xv = XB_DEFS.map((d) => d - 64);
  let edits = [];
  if (flags & PTF.XB) {
    const x = parseXBlock(b, at.xb, b.length - at.xb);
    if (!x.ok || x.length !== 1 + b[at.xb]) return bad(x.error ?? "The extension block is damaged.");
    for (const [k, v] of x.ext) xv[k] = v;
    edits = x.edits;
  }
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
  const out = { ok: true, len: b.length, slot: b[2], rev: b[3], flags, engine: eng, cat: b[6], name: nameOf(b, 7), values, pairs, xv, edits,
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

// null when the unit would take bytes as a record (pt_rec_ok and up_put_raw, upreset.c; the slot and revision bytes are the unit's own and not looked at), else a short reason
export function recordProblem(bytes) {
  const b = bytes instanceof Uint8Array ? bytes : Uint8Array.from(bytes), r = parseRecord(b);
  if (!r.ok) return r.error.replace(/\.$/, "");
  let i = 0;
  while (i < 12 && b[7 + i]) { if (b[7 + i] < 32 || b[7 + i] > 126) return "the name has a character that is not printable"; i++; }
  for (; i < 12; i++) if (b[7 + i]) return "the name is not padded with 0";
  const { at } = layout(b);
  for (let j = 0; j < b[19]; j++) {                  // every stored value inside what the store reads (a byte, + 64), every id one that exists
    if (b[4] & PTF.BM) { if (b[at.pr + 16 + j] > 191) return "a value is out of range"; }
    else if (b[at.pr + 2 * j] >= L.P_COUNT || b[at.pr + 2 * j + 1] > 191) return "it stores a parameter that does not exist or a value out of range";
  }
  return null;
}

// ext (NX values or null) and edits -> the extension block bytes ([] when both are empty), as xb_block_build writes it
function buildXBlock(ext, edits) {
  if (ext && ext.length !== NX) throw new Error(`The extension values are ${NX} numbers.`);
  const clamp = (v) => { if (!Number.isFinite(v)) throw new Error("An extension value is not a number."); return Math.max(-64, Math.min(127, Math.round(v))); };
  const t1 = [], t2 = [], ids = new Set();
  for (let k = 0; k < NX; k++) if (ext && clamp(ext[k]) + 64 !== XB_DEFS[k]) t1.push(XW_BASE + k, clamp(ext[k]) + 64);
  if (edits.length > XB_MAX) throw new Error(`A record holds at most ${XB_MAX} hand-set B values.`);
  for (const e of edits) {
    if (!Number.isInteger(e.wid) || e.wid < 0 || e.wid > 255) throw new Error(`Parameter ${e.wid} is not a wide id.`);
    if (e.wid === 127) throw new Error("Wide id 127 is reserved and cannot be an edit.");
    if (ids.has(e.wid)) throw new Error(`Parameter ${e.wid} is edited twice.`);
    ids.add(e.wid); t2.push(e.wid, clamp(e.val) + 64);
  }
  if (!t1.length && !t2.length) return [];
  const body = [XB_VER, ...(t1.length ? [1, t1.length, ...t1] : []), ...(t2.length ? [2, t2.length, ...t2] : [])];
  return Uint8Array.from([body.length, ...body]);
}

// the record for a sound. values: P_COUNT numbers (each -64..127); mb: 7 bytes or null; pat {note[16], flags[16]} or null; patch128: the FM6 patch (128 seven-bit
// bytes) for an FM6 sound; ak: the block bytes (u16 length first) or null; ext: the NX extension values (-64..127) or null (all at their default); edits: the hand-set
// B values [{wid, val}] (at most XB_MAX, each id once, not 127). The extension block is written as xb_block_build does and only when there is something in it.
// Slot and revision are 0: the unit sets them when it writes.
export function buildRecord({ engine, name, cat = 0, values, mb = null, pat = null, patch128 = null, ak = null, ext = null, edits = [] }) {
  const f0 = engine === engineIndex("FM6") && patch128 ? PTF.FM6 : 0;
  const nm = [...String(name)].map((c) => c.charCodeAt(0)).filter((c) => c >= 32 && c <= 126).slice(0, 12);
  if (!nm.length) throw new Error("A patch needs a name (1 to 12 plain characters).");
  const ids = [], vs = [];
  for (let i = 0; i < L.P_COUNT; i++) {
    const v = i === 0 ? P_LEVEL_STORE_DEF : values[i]; if (!Number.isInteger(v) || v < -64 || v > 127) throw new Error(`Parameter ${i} has a value the store cannot hold (${v}).`);
    if (v !== storeDefault(engine, i)) { ids.push(i); vs.push(v + 64); }
  }
  const xb = buildXBlock(ext, edits);
  let f = f0 | (xb.length ? PTF.XB : 0) | PTF.TR | PTF.CB | (mb && mb.some((x) => x) ? PTF.MB : 0) | (pat ? PTF.PAT : 0) | (ak ? PTF.AK : 0) | (ids.length > 16 ? PTF.BM : 0);
  const parts = [], head = new Uint8Array(FIX);
  head[4] = f; head[5] = engine; head[6] = cat; nm.forEach((c, i) => { head[7 + i] = c; }); head[19] = ids.length;
  parts.push(head);
  if (xb.length) parts.push(xb);
  if (f & PTF.MB) parts.push(Uint8Array.from(mb));
  if (f & PTF.BM) { const bm = new Uint8Array(16); for (const i of ids) bm[i >> 3] |= 1 << (i & 7); parts.push(bm, Uint8Array.from(vs)); }
  else parts.push(Uint8Array.from(ids.flatMap((id, j) => [id, vs[j]])));
  if (f & PTF.PAT) parts.push(Uint8Array.from([...pat.note, ...pat.flags]));
  if (f & PTF.FM6) parts.push(packPatch(patch128));
  if (f & PTF.AK) parts.push(Uint8Array.from(ak));
  const len = parts.reduce((s, p) => s + p.length, 0), out = new Uint8Array(len); let o = 0;
  if (len > WORST) throw new Error(`The record is ${len} bytes; the unit stores at most ${WORST}.`);
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
  const ext = {};                                    // the extension slots off their default, by name
  r.xv?.forEach((v, k) => { if (v + 64 !== XB_DEFS[k]) ext[`X${k} ${XB_NAMES[k]}`] = String(v); });
  const edits = r.edits?.length ?? 0;
  if (edits) flags.push(`${edits} hand-set B value${edits > 1 ? "s" : ""}`);
  return { name: r.name, engine: eng?.name ?? "?", category: CATEGORIES[r.cat] || "-", carries: flags, params, ext, edits };
}
