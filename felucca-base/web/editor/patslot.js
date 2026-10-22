// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor (job F3): the pattern slot objects of the unit (firmware patslot.c, format PTS3) <-> the editor's records. No DOM, no imports.
//
// The unit holds ONE list of 32 pattern slots P01..P32 in three backup objects (ids 21, 22, 23: 11, 11 and 10 slots). There is no flash for
// fixed-size records of 64 steps, so an object is COMPACT and has a variable length (3584 bytes at most):
//   object  "PTS3", used u16 (the object's length), nslot u8 (11 / 11 / 10), per u8 (11: the slots an object holds), then nslot records
//   record  rlen u16, flags u8 (bit 0 CHANGED: saved by the user, the unit's ERASE gives the default back; bit 1 QUANTIZE), LEN, DIV, SWING, GATE,
//           nlen u8, the name (nlen printable characters), nst u8 (0..64 steps stored, the rest are empty steps), nst compact steps
//   step    c0 = time | accent << 2 | slide << 3 | RATCH << 4 (2 bits) | code << 6; code 0: nothing more; 1: note, velocity; 2: n | c << 3, velocity,
//           chance, nudge (4 bits), c notes
// The editor's own form of one slot is the RECORD of the files "duell-patterns" (and of stored.js decodePattern / encodePattern): 596 bytes,
//   used 0xA7, version 2, LEN, DIV, SWING, GATE, QUANTIZE, flags (bit 0 CHANGED), the name (12, zero padded), 64 steps of 9 bytes (FUN11's packing).
export const PS = { REC: 596, USED: 0xA7, VER: 2, NAME: 12, STEPS: 64, STEP: 9, PER: 11, SLOTS: 32, NOBJ: 3, HDR: 8, OBJ_MAX: 3584, MAGIC: [0x50, 0x54, 0x53, 0x33] };
export const nslotOf = (o) => Math.min(PS.PER, PS.SLOTS - o * PS.PER);   // 11, 11, 10

const BLANK_C0 = 2;   // time REST, nothing else: the empty step a track starts with (not stored after the last real one)

// a 9-byte step -> its compact bytes
export function compactStep(b) {
  const n = b[4] & 7, vel = b[5] & 127, prob = b[8] & 127;
  let nd = 0, c = 0;
  for (let j = 0; j < 4; j++) { nd |= (b[j] >> 7) << j; if (b[j] & 127) c = j + 1; }
  const c0 = ((b[4] >> 3) & 3) | ((b[4] >> 5) & 3) << 2 | ((b[5] >> 7) | (b[8] >> 7) << 1) << 4;
  if (!n && !c && !vel && !prob && !nd) return [c0];
  if (n === 1 && c === 1 && !nd && !prob) return [c0 | 1 << 6, b[0] & 127, vel];
  const out = [c0 | 2 << 6, n | c << 3, vel, prob, nd]; for (let j = 0; j < c; j++) out.push(b[j] & 127);
  return out;
}
// compact bytes at `at` -> { b (9 bytes), len } or null (a step the unit would refuse)
export function expandStep(s, at, end) {
  if (at >= end) return null;
  const c0 = s[at], code = c0 >> 6, b = new Uint8Array(9);
  b[4] = (c0 & 3) << 3 | ((c0 >> 2) & 3) << 5; b[5] = ((c0 >> 4) & 1) << 7; b[8] = ((c0 >> 5) & 1) << 7;
  if (code === 0) return { b, len: 1 };
  if (code === 1) { if (end - at < 3 || s[at + 1] > 127 || s[at + 2] > 127) return null; b[0] = s[at + 1]; b[4] |= 1; b[5] |= s[at + 2]; return { b, len: 3 }; }
  if (code !== 2 || end - at < 5) return null;
  const x = s[at + 1], c = x >> 3;
  if ((x & 7) > 4 || c > 4 || (x & 0xC0) || s[at + 2] > 127 || s[at + 3] > 101 || s[at + 4] > 15 || end - at < 5 + c) return null;
  b[4] |= x & 7; b[5] |= s[at + 2]; b[8] |= s[at + 3];
  for (let j = 0; j < c; j++) { if (s[at + 5 + j] > 127) return null; b[j] = s[at + 5 + j]; }
  for (let j = 0; j < 4; j++) b[j] |= ((s[at + 4] >> j) & 1) << 7;
  return { b, len: 5 + c };
}

const printable = (b, from, n) => { for (let i = 0; i < n; i++) if (b[from + i] < 32 || b[from + i] > 126) return false; return true; };
// a canonical record (596 bytes): null when the unit would take it, else a short reason
export function recordProblem(r) {
  if (!r || r.length !== PS.REC || r[0] !== PS.USED || r[1] !== PS.VER) return "not a pattern record";
  const [len, div, swing, gate, qnt] = [r[2], r[3], r[4], r[5], r[6]];
  if (len < 1 || len > 64 || div > 16 || swing > 100 || gate < 1 || gate > 127 || qnt > 1 || (r[7] & ~1)) return "a setting out of range";
  let n = 0; while (n < PS.NAME && r[8 + n]) n++;
  for (let i = n; i < PS.NAME; i++) if (r[8 + i]) return "a name with a gap";
  if (!printable(r, 8, n)) return "an unprintable name";
  for (let s = 0; s < 64; s++) {
    const t = 20 + s * 9;
    if ((r[t + 4] & 128) || (r[t + 4] & 7) > 4 || ((r[t + 4] >> 3) & 3) > 2 || (r[t + 8] & 127) > 101 || r[t + 6] || r[t + 7]) return "a damaged step";
  }
  return null;
}
// a canonical record -> the compact record bytes
export function packRecord(r) {
  let nl = 0; while (nl < PS.NAME && r[8 + nl]) nl++;
  const steps = []; let last = 0;
  for (let s = 0; s < 64; s++) { const c = compactStep(r.subarray(20 + s * 9, 29 + s * 9)); steps.push(c); if (!(c.length === 1 && c[0] === BLANK_C0)) last = s + 1; }
  const body = [(r[7] & 1) | (r[6] ? 2 : 0), r[2], r[3], r[4], r[5], nl, ...r.subarray(8, 8 + nl), last];
  for (let s = 0; s < last; s++) body.push(...steps[s]);
  return Uint8Array.from([body.length & 255, body.length >> 8, ...body]);
}
// the compact record at `at` (before `end`) -> { rec (596), len } or null
export function unpackRecord(s, at, end) {
  if (end - at < 9) return null;
  const n = 2 + (s[at] | s[at + 1] << 8); if (end - at < n || n < 9) return null;
  const fl = s[at + 2], nl = s[at + 7]; if (nl > PS.NAME || n < 9 + nl || fl > 3 || !printable(s, at + 8, nl)) return null;
  const nst = s[at + 8 + nl]; if (nst > 64) return null;
  const r = new Uint8Array(PS.REC); r.set([PS.USED, PS.VER, s[at + 3], s[at + 4], s[at + 5], s[at + 6], (fl >> 1) & 1, fl & 1]);
  r.set(s.subarray(at + 8, at + 8 + nl), 8);
  for (let i = nst; i < 64; i++) r[20 + i * 9 + 4] = 2 << 3;                 // the empty step: time REST
  let p = at + 9 + nl;
  for (let i = 0; i < nst; i++) { const x = expandStep(s, p, at + n); if (!x) return null; r.set(x.b, 20 + i * 9); p += x.len; }
  if (p !== at + n || recordProblem(r)) return null;
  return { rec: r, len: n };
}
// object o (0..2) as the unit holds it -> { recs: [596-byte records] } or { error }
export function parsePatObject(o, b) {
  if (!b || b.length < PS.HDR || b.length > PS.OBJ_MAX) return { error: "not a pattern slot object" };
  if (PS.MAGIC.some((x, i) => b[i] !== x)) return { error: b[3] === 0x32 ? "a pattern slot object of an older firmware (16 slots)" : "not a pattern slot object" };
  if ((b[4] | b[5] << 8) !== b.length || b[6] !== nslotOf(o) || b[7] !== PS.PER) return { error: "not a pattern slot object" };
  const recs = []; let at = PS.HDR;
  for (let i = 0; i < b[6]; i++) { const x = unpackRecord(b, at, b.length); if (!x) return { error: "damaged pattern slot" }; recs.push(x.rec); at += x.len; }
  return at === b.length ? { recs } : { error: "not a pattern slot object" };
}
// object o from its 596-byte records (one for every slot of the object) -> bytes, or null when they do not fit the object's 3584 bytes
export function buildPatObject(o, recs) {
  const parts = recs.map(packRecord), total = PS.HDR + parts.reduce((a, p) => a + p.length, 0);
  if (recs.length !== nslotOf(o) || total > PS.OBJ_MAX) return null;
  const b = new Uint8Array(total); b.set([...PS.MAGIC, total & 255, total >> 8, recs.length, PS.PER]);
  let at = PS.HDR; for (const p of parts) { b.set(p, at); at += p.length; }
  return b;
}
