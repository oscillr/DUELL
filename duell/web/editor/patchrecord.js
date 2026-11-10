// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: a patch slot's STORED RECORD (firmware/src/upreset.c, "Chunk ... Record", PT_VER 4): read, built, described. The record is the unit's own form; PATCH_RAW
// (EDITOR_PROTOCOL.md, cmd 84) moves it whole, so a patch survives a trip through a file bit for bit. Little endian:
//   0 u16 length   2 slot (the unit sets it)   3 revision (the unit sets it)   4 flags   5 engine   6 category (0 none)   7 name, 12 bytes, 0-padded
//   19 number of values   20 the values: a 20 byte bitmap (a bit per WIDE id: 0..126 the parameter ids, 127 never, 128..155 the extension slots) of the values off their stored default,
//   then those values in wide id order, each as (value - minimum) in the bits of its range (see rangeOf), LSB first, the stream padded to the byte
//   [MB End B as values: morph.c's MBV block (parseEndB)] [PAT 32: 16 notes, 16 flags] [FM6 112: the 128 7-bit bytes of the FM6 patch packed 8 to 7] [AK: u16 length, then the arp / keys / sequencer block]
// The AK block (upreset.c ak_build): g (groups), u16 bitmap by ordinal (a bit per value off its default), those values at range bits padded to the byte, then, with the SEQ group, a u16 step map and 7 bytes a stored step.
// The unit writes the AK block in EVERY record (the arp group and the keys group always); a record without it loads as arp OFF, keys at their defaults (ak null = the defaults).
// The ranges are the unit's (params.c TP, each engine's edit table, XTP) and are part of the stored format: rangeOf reads them from layout.json, which the unit's own tables write
// (tests/editordump.c); tests/rangedump.c dumps the unit's table for the check in web/test_ranges.mjs. No DOM, no I/O.
import { L, P_E0, descOf, valueText } from "./layout.js";

export const PTF = { PAT: 1, TR: 2, FM6: 4, MB: 8, AK: 32, CB: 128, ALL: 175 };   // TR (0.2.19): the loudness is the pair of P_TRIM and the record carries no mixer level (P_LEVEL); a record without it held it as P_LEVEL: read as the trim. (PT_VER 3's XB 16 and BM 64 are gone: a record with them is refused)
export const FIX = 20, VB = 20, WORST = 577;        // the fixed part; the values' bitmap; the biggest record there can be: the unit's PT_WORST (the firmware refuses a longer one)
export const NX = 28, XW_BASE = 128;                // the extension slots (DUELL 1.1: NX 28)
export const XB_DEFS = [64, 65, 64, 64, 64, 64, 64, 65, 64, 64, 64, 74, 134, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 191, 191, 64];   // slot k's default is XB_DEFS[k] - 64 (up_xb.h XB_DEFS: TWIN LINK is ON, SUB OCTAVE is -1, the two LFO LEVELs are 127)
export const XB_NAMES = ["TWIN ROUTE", "TWIN LINK", "TWIN KEY TRACK", "TWIN FILTER ENV DEPTH", "TWIN LFO 1 DEPTH", "TWIN LFO 2 DEPTH", "SUB LEVEL", "SUB OCTAVE", "SUB WAVE", "SUB HARMONICS",
  "CYCLE MODE", "CYCLE RISE", "CYCLE FALL", "CYCLE CURVE", "CYCLE SYNC",
  "AMP ENV SATURATION", "FILTER ENV FILTER BLEND", "FILTER ENV FILTER DRIVE", "FILTER ENV MULTI RES", "CYCLE TWIN CUTOFF", "CYCLE PAN", "CYCLE PITCH", "LFO 2 TWIN RES", "LFO 2 MULTI MODE", "LFO 2 FILTER BLEND", "LFO 1 LEVEL", "LFO 2 LEVEL", "SPARE"];
export const CATEGORIES = ["-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "DRUM", "FX", "OTHER"];
const STORE_ZERO = new Set([27, 45, 48, 84, 92, 105, 111]);   // KEY TRK (0.2.22: the editor default is 32), CUT, SLOPE, LFO 2 TRIG, LFO 1 TRIG, DUCK CURVE, DUCK SRC (0.2.24: the editor default is OFF, 3; the store keeps PUMP, 0): 0 in the store (the editor defaults are 32, 82, 1, 1, 1, 1, 3)
const AK_IDS = [17, 18, 19, 20, 21, 22, 23, 24, 127, 81, 82, 29, 30, 31, 32, 94];   // the ordinals 0..15: ARP x8, SCALE (retired: no id, 127), CHRD, VOIC, LEN DIV SWG GATE QNT
const AK_GROUP = (o) => (o < 8 ? 1 : o < 11 ? 2 : 4);
const P_SLEN = 29;                                  // the sequencer's LEN: at most 16 in a record

// ---- the unit's range table: wide id w of an engine -> { min, max, def }. Ids 0..118 the part parameters (TP), 119..126 the engine's eight edit values, 127 the free entry (0 .. 0), 128..155 the extension slots (XTP)
export const widOk = (w) => (w >= 0 && w < 127) || (w >= XW_BASE && w < XW_BASE + NX);       // mbv_wid
export function rangeOf(eng, w) {
  if (w === 127) return { min: 0, max: 0, def: 0 };
  if (w >= XW_BASE) return L.XTP[w - XW_BASE];
  return w >= P_E0 ? L.ENG[eng].edit[w - P_E0] : L.TP[w];
}
export function widBits(eng, w) {                   // morph.c wv_bits: the bits of the range
  const r = rangeOf(eng, w), s = r.max - r.min + 1; let b = 0;
  while (b < 16 && (1 << b) < s) b++;
  return b;
}
const widCode = (eng, w, v) => { const r = rangeOf(eng, w); return Math.max(r.min, Math.min(r.max, v)) - r.min; };   // wv_code
const widVal = (eng, w, c) => { const r = rangeOf(eng, w); return Math.min(r.min + c, r.max); };                       // wv_val
function bsPut(b, at, v, n) { for (let i = 0; i < n; i++, at.n++) if (v >> i & 1) b[at.n >> 3] |= 1 << (at.n & 7); }          // upreset.c / morph.c bs_put
function bsGet(b, at, n) { let v = 0; for (let i = 0; i < n; i++, at.n++) v |= (b[at.n >> 3] >> (at.n & 7) & 1) << i; return v; }
// the wide values End B never differs in (morph.c mbv_never): P_LEVEL, P_MUTE, the arp's SWING / PROB / HOLD / ORDER, the sequencer's five, CHRD, VOIC, ARP TARGET, QUANTIZE, the DUCK's CURVE / SOURCE / RATE,
// the gone ED / E2 / L2 depths, CYCLE to PITCH and the three last slots
const NEVER = new Set([0, 5, 21, 22, 23, 24, 29, 30, 31, 32, 40, 81, 82, 86, 94, 105, 111, 112, 115, 116, 117, 118, 149, 153, 154, 155]);

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
const NFM = 65, MBV_HDR = 22, MBV_FMN = 57, PIN_BYTES = 20, FM6_ENGINE = () => engineIndex("FM6");
// The End B block (morph.c MBV) at b[at] with avail bytes there, for an engine eng: its length, 0 = no block (morph_bv_size).
//   [0] flags (1 the marks of the hand-set values follow, 2 the 65 FM6 End B bytes follow)   [1] AMOUNT + 1 (0 not stored; at most 101)   [2..21] a bit per wide id: End B differs from the Original
//   then the values of the set bits at their range bits, padded to the byte   [flag 1] 20 bytes of marks   [flag 2] 57 bytes: 65 FM6 bytes, 7 bits each (each at most 99)
export function endBSize(b, at, avail, eng) {
  if (avail < MBV_HDR || eng >= L.NENGINES || (b[at] & ~3) || b[at + 1] > 101 || ((b[at] & 2) && eng !== FM6_ENGINE())) return 0;
  let nb = 0;
  for (let w = 0; w < 160; w++) if (b[at + 2 + (w >> 3)] >> (w & 7) & 1) { if (!widOk(w) || NEVER.has(w)) return 0; nb += widBits(eng, w); }
  let len = MBV_HDR + (nb + 7 >> 3);
  if (b[at] & 1) {
    if (len + PIN_BYTES > avail) return 0;
    for (let w = 0; w < 160; w++) if ((b[at + len + (w >> 3)] >> (w & 7) & 1) && !widOk(w)) return 0;
    len += PIN_BYTES;
  }
  if (b[at] & 2) {
    if (len + MBV_FMN > avail) return 0;
    const p = { n: (at + len) * 8 };
    for (let i = 0; i < NFM; i++) if (bsGet(b, p, 7) > 99) return 0;
    len += MBV_FMN;
  }
  return len;
}
// An End B block (the form endBSize reads; morph.c morph_bv_make): engine; amount: the part's AMOUNT knob 0..100 or null (not stored); values: { wide id: End B's value } for the values that differ from the Original
// (a wide id that never differs, or none, is refused); marks: wide ids set by hand or null; fm6: End B's 65 FM6 bytes (0..99) or null (FM6 only)
export function buildEndB({ engine, amount = null, values = {}, marks = null, fm6 = null }) {
  const ids = Object.keys(values).map(Number).sort((a, b) => a - b), hdr = new Uint8Array(MBV_HDR), bits = new Uint8Array(200), at = { n: 0 };
  if (amount !== null && !(Number.isInteger(amount) && amount >= 0 && amount <= 100)) throw new Error("The AMOUNT of End B is 0 to 100.");
  hdr[1] = amount === null ? 0 : amount + 1;
  for (const w of ids) {
    if (!widOk(w) || NEVER.has(w)) throw new Error(`Parameter ${w} never differs between the ends.`);
    hdr[2 + (w >> 3)] |= 1 << (w & 7); bsPut(bits, at, widCode(engine, w, values[w]), widBits(engine, w));
  }
  const parts = [hdr, bits.subarray(0, at.n + 7 >> 3)];
  if (marks) { const m = new Uint8Array(PIN_BYTES); for (const w of marks) { if (!widOk(w)) throw new Error(`Parameter ${w} cannot be marked.`); m[w >> 3] |= 1 << (w & 7); } hdr[0] |= 1; parts.push(m); }
  if (fm6) {
    if (engine !== FM6_ENGINE() || fm6.length !== NFM || fm6.some((x) => !(x >= 0 && x <= 99))) throw new Error("End B of an FM6 sound holds 65 bytes, 0 to 99.");
    const f = new Uint8Array(MBV_FMN), q = { n: 0 }; fm6.forEach((x) => bsPut(f, q, x, 7)); hdr[0] |= 2; parts.push(f);
  }
  const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0)); let o = 0;
  for (const p of parts) { out.set(p, o); o += p.length; }
  return out;
}
function valueBits(b, o, eng) {                     // upreset.c pt_vbits: the bits of the values the bitmap at b[o] names
  let nb = 0;
  for (let w = 0; w < VB * 8; w++) if ((b[o + (w >> 3)] >> (w & 7) & 1) && widOk(w)) nb += widBits(eng, w);
  return nb;
}
function layout(b) {                                // where the parts start (upreset.c pt_layout_x); total = the length the flags imply
  const f = b[4], rl = rd16(b, 0), eng = b[5], at = {}; let o = FIX;
  at.pr = o; at.mb = at.pat = at.fm = at.ak = o;
  if (o + VB > rl || eng >= L.NENGINES) return { at, total: o + 65536 };   // (a total that cannot be the record's)
  o += VB + (valueBits(b, o, eng) + 7 >> 3);
  at.mb = o; if (f & PTF.MB) { const n = o < rl ? endBSize(b, o, rl - o, eng) : 0; if (!n) return { at, total: o + 65536 }; o += n; }
  at.pat = o; if (f & PTF.PAT) o += 32;
  at.fm = o; if (f & PTF.FM6) o += 112;
  at.ak = o;
  if (f & PTF.AK) { if (o + 2 > rl) return { at, total: o + 65536 }; o += 2 + rd16(b, o); }
  return { at, total: o };
}
const nameOf = (b, o) => { let s = ""; for (let i = 0; i < 12 && b[o + i]; i++) s += String.fromCharCode(b[o + i]); return s; };

// bytes -> { ok, error } or the record decoded: { ok, len, slot, rev, flags, engine, cat, name, values[P_COUNT], pairs, xv[NX] (the extension slots, their defaults where the record does not name them), mb (End B's block bytes) | null, pat, fm6 (128 bytes) | null, ak | null }
export function parseRecord(bytes) {
  const b = bytes instanceof Uint8Array ? bytes : Uint8Array.from(bytes), bad = (error) => ({ ok: false, error });
  if (b.length < FIX + VB || b.length > WORST) return bad("The record has the wrong size.");
  if (rd16(b, 0) !== b.length) return bad("The record's length field does not match its size.");
  if (b[4] & ~PTF.ALL) return bad("The record has flags this editor does not know.");
  if (!(b[4] & PTF.CB)) return bad("The record is in the form of a firmware before 0.2.14, which no longer loads.");
  if (b[5] >= L.NENGINES) return bad("The record names an engine that does not exist.");
  if (!b[7]) return bad("The record has no name.");
  const { at, total } = layout(b);
  if (total !== b.length) return bad("The record's parts do not add up to its length.");
  const eng = b[5], values = storeDefaults(eng), pairs = new Map(), flags = b[4], xv = XB_DEFS.map((d) => d - 64);
  const p = { n: (at.pr + VB) * 8 }; let n = 0;
  for (let w = 0; w < VB * 8; w++) if (b[at.pr + (w >> 3)] >> (w & 7) & 1) {
    if (!widOk(w)) return bad("The record stores a parameter that does not exist.");
    const v = widVal(eng, w, bsGet(b, p, widBits(eng, w))); n++;
    if (w < L.P_COUNT) { values[w] = v; pairs.set(w, v); } else xv[w - XW_BASE] = v;
  }
  if (n !== b[19]) return bad("The record's value count does not match its bitmap.");
  if (!(flags & PTF.TR)) { values[P_TRIM] = values[0]; pairs.set(P_TRIM, values[0]); values[0] = P_LEVEL_STORE_DEF; pairs.delete(0); }   // a record before 0.2.19: its LEVEL was the sound's loudness
  const out = { ok: true, len: b.length, slot: b[2], rev: b[3], flags, engine: eng, cat: b[6], name: nameOf(b, 7), values, pairs, xv,
    mb: flags & PTF.MB ? b.slice(at.mb, at.pat) : null,
    pat: flags & PTF.PAT ? { note: b.slice(at.pat, at.pat + 16), flags: b.slice(at.pat + 16, at.pat + 32) } : null,
    fm6: flags & PTF.FM6 && eng === engineIndex("FM6") ? unpackPatch(b.subarray(at.fm, at.fm + 112)) : null, ak: null };
  if (flags & PTF.AK) {                              // the block (upreset.c ak_dec): groups present, their values (the other values of a present group are the engine's defaults), the steps
    const c = b.subarray(at.ak + 2, at.ak + 2 + rd16(b, at.ak)), g = c[0] & 7, vals = new Map(); let ok = c.length >= 3 && !(c[0] & ~7) && g, nst = 0;
    if (ok) {
      const bm = c[1] | c[2] << 8; let nb = 0;
      for (let o = 0; o < 16; o++) if (AK_GROUP(o) & g && o !== 8) vals.set(AK_IDS[o], rangeOf(eng, AK_IDS[o]).def);
      for (let o = 0; o < 16 && ok; o++) if (bm >> o & 1) { if (o === 8 || !(AK_GROUP(o) & g)) ok = false; else nb += widBits(eng, AK_IDS[o]); }
      if (ok && 3 + (nb + 7 >> 3) > c.length) ok = false;
      if (ok) {
        const q = { n: 24 };
        for (let o = 0; o < 16; o++) if (bm >> o & 1) vals.set(AK_IDS[o], widVal(eng, AK_IDS[o], bsGet(c, q, widBits(eng, AK_IDS[o]))));
        let pos = 3 + (nb + 7 >> 3);
        if (g & 4) {
          if (pos + 2 > c.length) ok = false;
          else { const map = c[pos] | c[pos + 1] << 8; pos += 2; for (let i = 0; i < 16; i++) if (map >> i & 1) { if (pos + 7 > c.length) ok = false; nst++; pos += 7; }
            if (vals.get(P_SLEN) > 16) vals.set(P_SLEN, 16); }
        }
        ok = ok && pos === c.length;
      }
    }
    if (ok) { out.ak = { groups: g, values: vals, steps: nst }; for (const [id, v] of vals) out.values[id] = v; }
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
  return null;
}

// the record for a sound (upreset.c pt_encode). values: P_COUNT numbers (each -64..127); mb: End B's block (endBSize says whether it is one) or null; pat {note[16], flags[16]} or null; patch128: the FM6 patch
// (128 seven-bit bytes) for an FM6 sound; ak: the block bytes (u16 length first, as ak_build writes it) or null; ext: the NX extension values (-64..127) or null (all at their default).
// A value is written when it differs from the store's default inside its range; the mixer level never; the ids of an AK group the block carries are in the block. Slot and revision are 0: the unit sets them when it writes.
export function buildRecord({ engine, name, cat = 0, values, mb = null, pat = null, patch128 = null, ak = null, ext = null }) {
  const nm = [...String(name)].map((c) => c.charCodeAt(0)).filter((c) => c >= 32 && c <= 126).slice(0, 12);
  if (!nm.length) throw new Error("A patch needs a name (1 to 12 plain characters).");
  if (ext && ext.length < NX) throw new Error(`The extension values are ${NX} numbers.`);
  if (ak && (ak.length < 5 || rd16(ak, 0) !== ak.length - 2)) throw new Error("The arp block's length does not match its bytes.");
  const hasAk = !!ak && (ak[2] & 7) !== 0, gm = hasAk ? ak[2] & 7 : 0;
  const mbb = mb && mb.length ? Uint8Array.from(mb) : null;
  if (mbb && endBSize(mbb, 0, mbb.length, engine) !== mbb.length) throw new Error("End B is not a block the unit takes.");
  const bm = new Uint8Array(VB), bits = new Uint8Array(VB * 16), at = { n: 0 }; let n = 0;
  for (let w = 0; w < 156; w++) {
    const ao = AK_IDS.indexOf(w);
    if (!widOk(w) || w === 0 || (ao >= 0 && (AK_GROUP(ao) & gm))) continue;
    let v, d;
    if (w < L.P_COUNT) {
      v = values[w]; d = storeDefault(engine, w);
      if (!Number.isInteger(v) || v < -64 || v > 127) throw new Error(`Parameter ${w} has a value the store cannot hold (${v}).`);
    } else {
      const k = w - XW_BASE; d = XB_DEFS[k] - 64; v = ext ? ext[k] : d;
      if (!Number.isFinite(v)) throw new Error("An extension value is not a number.");
      v = Math.max(-64, Math.min(127, Math.round(v)));
    }
    if (widCode(engine, w, v) !== widCode(engine, w, d)) { bm[w >> 3] |= 1 << (w & 7); bsPut(bits, at, widCode(engine, w, v), widBits(engine, w)); n++; }
  }
  const f = (engine === engineIndex("FM6") && patch128 ? PTF.FM6 : 0) | PTF.TR | PTF.CB | (mbb ? PTF.MB : 0) | (pat && !(gm & 4) ? PTF.PAT : 0) | (hasAk ? PTF.AK : 0);
  const head = new Uint8Array(FIX);
  head[4] = f; head[5] = engine; head[6] = cat; nm.forEach((c, i) => { head[7 + i] = c; }); head[19] = n;
  const parts = [head, bm, bits.subarray(0, at.n + 7 >> 3)];
  if (mbb) parts.push(mbb);
  if (f & PTF.PAT) parts.push(Uint8Array.from([...pat.note, ...pat.flags]));
  if (f & PTF.FM6) parts.push(packPatch(patch128));
  if (hasAk) parts.push(Uint8Array.from(ak));
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
  // the named filter parameters say what the unit and the editor call them (DUELL 1.1 naming rule), not the page title + stored code
  const nm = { P_FREEA: "MULTI CUTOFF", P_FREEB: "MULTI RES", P_FREEC: "MULTI MODE", P_FREED: "MULTI SLOPE", P_KTRK: "KEY TRACK", P_E2FLT: "FILTER ENV DEPTH",
    P_LD_FLT: "LFO 1 DEPTH", P_L2FLT: "LFO 2 DEPTH", P_CUT2: "TWIN CUTOFF", P_RES2: "TWIN RES" };
  const fix = { P_FREEA: 45, P_FREEB: 46, P_FREEC: 47, P_FREED: 48 };   // (the MULTI FILTER 1/2 ids, as dx7import.js; layout.json's ID table lists only some)
  for (const [k, v] of Object.entries(nm)) { const i = L.ID[k] ?? fix[k]; if (i != null && i < L.P_COUNT) NAMES[i] = v; }
  return NAMES;
}
export function describeRecord(r) {                  // -> { name, engine, category, flags[], params: { "MULTI CUTOFF": "71 (6.8 kHz)" ... } }
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
  return { name: r.name, engine: eng?.name ?? "?", category: CATEGORIES[r.cat] || "-", carries: flags, params, ext };
}
