// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the editor protocol's encodings (web/EDITOR_PROTOCOL.md). Pure functions, no I/O.
export const CMD = {
  INFO: 1, GET: 2, SET: 3, DUMP: 4, DESC: 5, STEP_GET: 6, STEP_SET: 7, PRESET: 8, PROJECT: 9, NAMES: 10,
  UP_LIST: 16, UP_GET: 17, UP_PUT: 18, UP_STORE: 19, UP_LOAD: 20, UP_ERASE: 21, WATCH: 22, CHANGED: 23, RELOAD: 24, PING: 25, STEP_CHANGED: 26,
  FM6_GET: 68, FM6_PUT: 69, PATCH_RAW: 84,
  TRACK: 27, TRACK_MIX: 28, TRACK_DUMP: 29, TRACK_STEP: 30, TRACK_PARAM: 31, TRACK_CHANGED: 32, MENU_DESC: 72, MENU_SET: 73, LEARN_GET: 78, LEARN_SET: 79,
};
export const v14 = (v) => { const x = v + 8192; return [x & 127, x >> 7 & 127]; };
export const rv14 = (a, o = 0) => ((a[o] | a[o + 1] << 7) - 8192);
export const asc = (s) => [...s].map((c) => c.charCodeAt(0) & 127);
export const cstr = (s) => [...asc(s), 0];
export function rstr(a, pos) { let s = ""; while (pos < a.length && a[pos]) s += String.fromCharCode(a[pos++]); return [s, pos + 1]; }

// INFO: version, counts, engine names, tagged blocks
const TAG_LEN = { 0x55: 3, 0x4d: 4, 0x42: 3, 0x46: 4, 0x53: 3, 0x50: 3, 0x4e: 3, 0x52: 3, 0x4c: 3, 0x41: 4, 0x54: 3, 0x56: 3, 0x43: 3, 0x57: 3, 0x58: 4, 0x59: 3, 0x5a: 3 };
export function parseInfo(a) {
  let [version, pos] = rstr(a, 0);
  if (pos < 1 || pos > a.length) throw new Error("The unit sent an unreadable INFO");
  const nEng = a[pos], pcount = a[pos + 1], gcount = a[pos + 2], nstep = a[pos + 3], pe0 = a[pos + 4]; pos += 5;
  const engines = [];
  for (let i = 0; i < nEng; i++) { const [s, n] = rstr(a, pos); engines.push(s); pos = n; }
  const ntrk = a[pos]; pos += 2;
  const tags = {};
  while (pos < a.length && TAG_LEN[a[pos]]) { tags[a[pos]] = a.slice(pos + 2, pos + TAG_LEN[a[pos]]); pos += TAG_LEN[a[pos]]; }
  return { version, nEng, pcount, gcount, nstep, pe0, engines, ntrk, tags, caps: tags[0x42]?.[0] ?? null, menuCount: tags[0x4e]?.[0] ?? null, learn: !!tags[0x43],
           patchCap: tags[0x58] ? tags[0x58][0] | tags[0x58][1] << 7 : null,
           // patchCap: the patch store's capacity (tag 58 01 lo hi, DUELL 0.2.15; null: a unit without it); patchRaw: tag 5A 01 caps bit 0 (DUELL 0.2.19) = PATCH_RAW (cmd 84)
           patchRaw: !!((tags[0x5a]?.[0] ?? 0) & 1),
           // nx: tag 59 01 NX (cloud plan job I): the unit has protocol scope 2 (GET / SET / DESC / CHANGED and TRACK_PARAM's scope byte) for its NX extension slots; null: a unit without it
           nx: tags[0x59] ? tags[0x59][0] : null };
}

export function parseTrackQuery(a, ntrk = 4) {      // TRACK reply: selected, NTRK, per track: engine, preset, level v14, mute, armed
  const out = { selected: a[0], tracks: [] };
  for (let t = 0; t < Math.min(a[1], ntrk); t++) { const o = 2 + t * 6; out.tracks.push({ eng: a[o], preset: a[o + 1], level: rv14(a, o + 2), mute: a[o + 4], armed: a[o + 5] }); }
  return out;
}
export function parseTrackDump(a, pcount) {         // track, engine, preset, P_COUNT x v14
  const p = []; for (let i = 0; i < pcount; i++) p.push(rv14(a, 3 + 2 * i));
  return { track: a[0], eng: a[1], preset: a[2], p };
}
export function parseDump(a, pcount, gcount) {      // engine, preset, P_COUNT x v14, G_COUNT x v14 (selected track and globals)
  const p = [], g = [];
  for (let i = 0; i < pcount; i++) p.push(rv14(a, 2 + 2 * i));
  for (let i = 0; i < gcount; i++) g.push(rv14(a, 2 + 2 * pcount + 2 * i));
  return { eng: a[0], preset: a[1], p, g };
}
export function parseNames(a) {                     // engine, count, names..., two page titles
  const names = []; let pos = 2;
  for (let i = 0; i < a[1]; i++) { const [s, n] = rstr(a, pos); names.push(s); pos = n; }
  return { eng: a[0], names };
}
// UP_LIST: start, count, total, per slot (1 = it holds a sound, engine, name: every slot of DUELL 0.2.17 does, its saved record or else its factory sound),
// then one category byte per slot, then (0.2.17) one SAVED byte per slot: 1 = a saved record (RESTORE applies), 0 = the factory sound. A unit that
// sends no SAVED bytes (before 0.2.17) is read with saved = used.
export function parseUpList(a) {
  const start = a[0], count = a[1], total = a[2], slots = []; let pos = 3;
  for (let i = 0; i < count; i++) { const used = a[pos], eng = a[pos + 1]; const [name, n] = rstr(a, pos + 2); pos = n; slots.push({ slot: start + i, used: !!used, eng, name }); }
  slots.forEach((s, i) => { s.cat = a[pos + i] ?? 0; });
  const hasSaved = a.length >= pos + 2 * count;
  slots.forEach((s, i) => { s.saved = hasSaved ? !!a[pos + count + i] : s.used; });
  return { start, count, total, slots };
}
// UP_GET: slot, used, engine, name, P_COUNT x v14, 16 x (note, flags), kind (0), category, SAVED. A factory slot: its sound's values, no pattern.
export function parseUpGet(a, pcount) {
  const [name, p0] = rstr(a, 3); let pos = p0;
  const values = []; for (let i = 0; i < pcount; i++, pos += 2) values.push(rv14(a, pos));
  const notes = []; for (let i = 0; i < 16; i++, pos += 2) notes.push([a[pos], a[pos + 1]]);
  pos++;                                           // kind
  return { slot: a[0], used: !!a[1], eng: a[2], name, values, notes, cat: a[pos] ?? 0, saved: a[pos + 1] == null ? !!a[1] : !!a[pos + 1] };
}
// UP_PUT arguments for a sound: slot, engine, name, values, 16 steps, kind 0 + 16 zero bytes (the unit's grid slot), category
export const upPutArgs = (slot, eng, name, values, cat = 0, notes = null) =>
  [slot, eng, ...cstr(name), ...values.flatMap(v14), ...Array.from({ length: 16 }, (_, i) => notes?.[i] ?? [0, 0]).flat(), 0, ...new Array(16).fill(0), cat];
export function parseMenu(a) {                      // index, id, kind, value, min, max, name, names|unit, tab, tabname
  if (a[1] === 127) return null;
  const o = { index: a[0], id: a[1], kind: a[2], value: rv14(a, 3), min: rv14(a, 5), max: rv14(a, 7) };
  let pos = 9; [o.name, pos] = rstr(a, pos);
  if (o.kind === 0) { o.names = []; for (let i = 0; i <= o.max - o.min; i++) { const [s, n] = rstr(a, pos); o.names.push(s); pos = n; } }
  else if (o.kind === 1 || o.kind === 2) { [o.unit, pos] = rstr(a, pos); }   // 1: a number; 2 (DUELL 0.2.22): a number in tenths of its unit (TUNE: 4400 = 440.0 Hz)
  if (pos < a.length) { o.tab = a[pos]; [o.tabName] = rstr(a, pos + 1); } else { o.tab = 0; o.tabName = ""; }
  return o;
}
export function parseLearn(a) {                     // [rc,] 16, then 16 x (used, CC, track, id)
  const o = a.length > 65 ? 1 : 0, n = a[o], out = [];
  for (let i = 0; i < n; i++) { const q = o + 1 + 4 * i; if (a[q]) out.push({ cc: a[q + 1], track: a[q + 2], id: a[q + 3] }); }
  return out;
}
export function parseStep(a, o = 0) {               // index, n, note0..3, time, flags, vel, hits(3), chance, ratchet, nudge+8   (offset o skips a track byte)
  return { index: a[o], n: a[o + 1], notes: a.slice(o + 2, o + 6), time: a[o + 6], flags: a[o + 7], vel: a[o + 8], hits: a.slice(o + 9, o + 12), chance: a[o + 12] ?? 100, ratchet: a[o + 13] ?? 1, nudge: a[o + 14] ?? 8 };
}
export const stepBytes = (s) => [s.n, ...s.notes, s.time, s.flags, s.vel, ...s.hits, s.chance, s.ratchet, s.nudge];

export const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
export const noteName = (n) => `${NOTE_NAMES[n % 12]}${Math.floor(n / 12) - 1}`;

export const RC_TEXT = {
  UP: { 1: "The unit refused the request (bad slot or bad name).", 2: "The unit could not write: it could not stop playback in time, or the flash failed. Stop the playback on the unit and try again." },
};
// scope 2 (cloud plan job I, EDITOR_PROTOCOL.md "Scope 2"): an extension slot of a part. TRACK_PARAM reaches any part: [part, slot, scope 2] reads, [part, slot, v14, 2] writes; the reply is part, slot, v14, scope
export const XSCOPE = 2;
export const xReadArgs = (part, k) => [part, k, XSCOPE];
export const xWriteArgs = (part, k, v) => [part, k, ...v14(v), XSCOPE];
export const parseXReply = (a) => ({ part: a[0], k: a[1], v: rv14(a, 2), scope: a[4] });
