// SPDX-License-Identifier: GPL-3.0-only
// A simulated DUELL unit for the Backup page: INFO, BACKUP_LIST / GET / PUT with the unit's rules (editor_backup.c: arguments first, the
// transport stopped or rc 3, staging, CRC and content checked at commit, an empty object for length 0). Used by test_duellbackup.mjs and by
// editor/?mock=1 (try the page without a unit). It is NOT the firmware: the C tests (tests/backup_test.c) cover that.
import { BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc } from "../fm1backup.js";
import { checkObject, DUELL_IDS, decodePatches, decodeMultis, PATCH_IDS_6, PATCH_IDS_X } from "./duellbackup.js";
import { WT, buildObject, parseObject, cleanName, ampToByte } from "./wavetable.js";
import { CMD, v14, rv14, cstr, rstr } from "./proto.js";
import { L, ID, descOf, clampTo, P_E0 } from "./layout.js";
import { buildRecord, parseRecord, engineIndex, FIX as REC_FIX } from "./patchrecord.js";
import { FM6 } from "./fm6voice.js";

const put16 = (b, o, v) => { b[o] = v & 255; b[o + 1] = v >> 8 & 255; };
const putName = (b, o, s) => { for (let i = 0; i < 12; i++) b[o + i] = i < s.length ? s.charCodeAt(i) : 0; };
export const MOCK_ENGINES = L.ENG.map((e) => e.name);   // the firmware's own list (layout.json)
const FM6_E = engineIndex("FM6"), INIT128 = Uint8Array.from(FM6.INIT_PK);
const PIECE = 112;

// builders for the objects the unit stores (the same layouts as the firmware: upreset.c, multi.c, patslot.c)
export function buildPatchChunk(records) {             // [{slot, rev, name, engine, cat}] (a 20 byte stub each) or [{bytes}] (a whole record, its slot and revision set by the caller)
  const b = new Uint8Array(4 + records.reduce((n, r) => n + (r.bytes ? r.bytes.length : 20), 0));
  b[0] = 0x50; b[1] = 0x54; b[2] = 1;
  let o = 4;
  for (const r of records) {
    if (r.bytes) { b.set(r.bytes, o); o += r.bytes.length; continue; }
    put16(b, o, 20); b[o + 2] = r.slot; b[o + 3] = r.rev ?? 1; b[o + 5] = r.engine ?? 0; b[o + 6] = r.cat ?? 0; putName(b, o + 7, r.name);
    o += 20;
  }
  return b;
}
export function buildMultiObject(names) {              // [nameA | null, nameB | null]
  const recs = names.map((n) => {
    if (n == null) return new Uint8Array(0);
    const r = new Uint8Array(40); r[0] = 2; r[1] = 119; putName(r, 8, n); r[20] = 7; return r;
  });
  const b = new Uint8Array(8 + recs[0].length + recs[1].length);
  b[0] = 0x4D; b[1] = 0x55; b[2] = 0x4C; b[3] = 0x31; put16(b, 4, recs[0].length); put16(b, 6, recs[1].length);
  b.set(recs[0], 8); b.set(recs[1], 8 + recs[0].length);
  return b;
}
export function buildPatternObject(slots) {            // six entries: null or {name, len}
  const b = new Uint8Array(3584);
  b[0] = 0x50; b[1] = 0x54; b[2] = 0x53; b[3] = 0x32; put16(b, 4, 596); put16(b, 6, 6);
  slots.forEach((s, i) => {
    if (!s) return;
    const o = 8 + i * 596;
    b[o] = 0xA7; b[o + 1] = 2; b[o + 2] = s.len ?? 16; b[o + 3] = 2; b[o + 4] = 0; b[o + 5] = 64; b[o + 6] = 1; putName(b, o + 8, s.name);
    for (let k = 0; k < 64; k++) { const t = o + 20 + k * 9; if (k % 4 === 0) { b[t] = 48 + (k % 12); b[t + 4] = 1; b[t + 5] = 100; } else b[t + 4] = 2 << 3; }
  });
  return b;
}
const filler = (n, seed, magic) => { const b = Uint8Array.from({ length: n }, (_, i) => (i * 31 + seed * 7 + (i >> 4)) & 255); b.set(magic, 0); return b; };

const MENU = [   // MENU_DESC items, DUELL 0.2.14 (EDITOR_PROTOCOL.md "MENU settings"): [id, name, kind, names|unit, min, max, value, tab]
  [0, "COLOR", 0, L.PALETTES.LIST.map((p) => p.name), 0, L.PALETTES.LIST.length - 1, L.PALETTES.DEFAULT, 0],   // the firmware's palettes (layout.json)
  [3, "ANIMATION", 0, ["ON", "OFF", "IDLE"], 0, 2, 0, 0], [4, "LEDS", 0, ["OFF", "LOW", "MEDIUM", "MAX"], 0, 3, 2, 0],
  [5, "BUTTON HOLD", 0, ["0.3 s", "0.4 s", "0.5 s", "0.6 s"], 0, 3, 1, 1], [6, "KNOB ACCEL", 0, ["OFF", "ON"], 0, 1, 1, 1],
  [10, "USB LEVEL", 0, ["MASTER", "FIXED"], 0, 1, 0, 2],
  [11, "USB SERIAL", 0, ["ON", "OFF"], 0, 1, 1, 3], [12, "CLICK", 0, ["OFF", "REC", "ON"], 0, 2, 1, 2],
  [13, "CLICK LEVEL", 0, ["LOW", "MID", "HIGH"], 0, 2, 1, 2], [14, "COUNT-IN", 0, ["OFF", "1 BAR", "2 BARS"], 0, 2, 0, 2],
  [15, "RESTORE LAST", 0, ["ON", "OFF"], 0, 1, 0, 3], [16, "SCALE LEDS", 0, ["AUTO", "OFF"], 0, 1, 0, 0],
  [17, "SCREEN OFF", 0, ["NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"], 0, 4, 0, 0], [19, "STEP PREVIEW", 0, ["OFF", "ON"], 0, 1, 0, 1],
  [20, "STEP KEYS", 0, ["WRITE", "ADD"], 0, 1, 0, 1], [22, "TUNE", 1, "ct", -50, 50, 0, 2],
  [25, "HOME OPENS", 0, ["MORPH", "MODE", "MIXER"], 0, 2, 0, 1], [26, "CPU METER", 0, ["ON", "OFF"], 0, 1, 0, 0],
  [27, "MULTI LOAD", 0, ["KEEPS MODE", "SETS MODE"], 0, 1, 0, 1],
  [28, "ARP", 0, ["OFF", "ON"], 0, 1, 1, 4], [29, "SEQ", 0, ["OFF", "ON"], 0, 1, 1, 4],      // GLO > MODULES (ids 28..31, tab 4 "MODULES": no MENU tab on the unit)
  [30, "SENDS", 0, ["OFF", "ON"], 0, 1, 1, 4], [31, "PART FX", 0, ["OFF", "ON"], 0, 1, 1, 4],
  [32, "PERF LEAVE", 0, ["FADE", "KEEP"], 0, 1, 0, 1],                                       // CONTROL: PERFORM's SPACE and KILL on leaving / a sound load
  [33, "BASS MONO", 0, ["OFF", "40 Hz", "50 Hz", "63 Hz", "80 Hz", "100 Hz", "125 Hz", "160 Hz", "200 Hz", "250 Hz", "315 Hz", "400 Hz"], 0, 11, 0, 5],   // MASTER (DUELL 0.2.19, tab 5 "MASTER": the unit's FX > MASTER page, no MENU tab): the master mono below the corner; the unit does not run it in DUEL
];
const TABS = ["DISPLAY", "CONTROL", "AUDIO", "SYSTEM", "MODULES", "MASTER"];
const KEEP = new Set([0, 39, 40]);                       // a sound load keeps the MIXER LEVEL (0.2.19: the sound's loudness is P_TRIM, which it sets), PAN, MUTE (and the arp / sequencer ids 17..32)
const keepsOnLoad = (id) => KEEP.has(id) || (id >= 17 && id <= 32);

export function defaultPart(eng, old) {
  const p = [];
  for (let i = 0; i < L.P_COUNT; i++) p.push(i < P_E0 ? L.TP[i].def : L.ENG[eng].edit[i - P_E0].def);
  if (old) for (let i = 0; i < P_E0; i++) if (keepsOnLoad(i)) p[i] = old.p[i];
  return { eng, preset: 0, p };
}

// DUELL 0.2.17: slot i holds its saved record or else the factory sound at browse position i. The browse order is the firmware's (layout.json BROWSE, from
// sounds.c DS_LIST: category, then engine in the order of L.ORDER, then name; the numbers of docs/PATCH_NUMBERS.md are position + 1)
export function factoryBrowse() {
  return L.BROWSE.map(([eng, idx]) => ({ eng, idx, name: L.ENG[eng].sounds[idx][0], cat: L.ENG[eng].sounds[idx][1] }));
}
const BROWSE = factoryBrowse();

export class MockUnit {
  constructor({ caps = 31, playing = false, version = "FELUCCA DUELL 0.2.20 bet", seed = true, savedFlags = true, patchRaw = true } = {}) {
    this.savedFlags = savedFlags; this.patchRaw = patchRaw; this.prStage = null; this.version = version; this.caps = caps; this.playing = playing;
    this.tables = [null, null, null]; this.wtStage = null;    // the user wavetables {name, data} (caps bit 3, cmds 81..83)
    this.objs = new Map(); this.log = []; this.fail = null; this.listed = false; this.staged = null; this.settingsSize = 584;
    this.parts = [defaultPart(0), defaultPart(12)];
    this.parts[1].p[ID.P_PAN] = 20;
    this.g = L.GP.map((d) => d.def); this.selected = 0; this.watching = false; this.onpush = null; this.watchTime = 0;
    this.patches = new Map(); this.multiStored = Array(16).fill(null); this.multiUsedExtra = new Set();
    this.menu = MENU.map((m) => m.slice()); this.learn = []; this.steps = [0, 1].map(() => Array.from({ length: 64 }, (_, i) => ({ index: i, n: i % 4 === 0 ? 1 : 0, notes: [i % 4 === 0 ? 48 + (i % 12) : 0, 0, 0, 0], time: i % 4 === 0 ? 0 : 2, flags: 0, vel: 100, hits: [0, 0, 0], chance: 100, ratchet: 1, nudge: 8 })));
    for (const st of this.steps) {      // a few things to look at: a chord held over two TIEs, a SLIDE, a RATCHET, a CHANCE, an accent
      Object.assign(st[8], { n: 2, notes: [55, 59, 0, 0], time: 0, vel: 90 }); Object.assign(st[9], { n: 0, notes: [0, 0, 0, 0], time: 1 }); Object.assign(st[10], { n: 0, notes: [0, 0, 0, 0], time: 1 });
      Object.assign(st[12], { flags: 2, notes: [52, 0, 0, 0] }); Object.assign(st[13], { n: 1, notes: [57, 0, 0, 0], time: 0, vel: 70 });
      Object.assign(st[4], { notes: [50, 0, 0, 0], flags: 1, vel: 127 }); Object.assign(st[14], { n: 1, notes: [60, 0, 0, 0], time: 0, ratchet: 3, chance: 60, vel: 80 });
    }
    this.objs.set(0, filler(3840, 1, [0x42, 0x4E, 0x55, 0x46]));
    this.objs.set(1, filler(this.settingsSize, 2, [0x34, 0x52, 0x45, 0x50]));
    if (seed) {
      for (const [slot, name, eng, cat] of [[0, "WARM PAD", 0, 3], [1, "SUB BASS", 0, 1], [5, "BELL", 12, 5], [64, "WAVE LEAD", 14, 2]])
        this.patches.set(slot, { name, eng, cat, ...defaultPart(eng) });
      if (caps & 8) this.tables[1] = { name: "SEED SAW", data: Uint8Array.from({ length: WT.BYTES }, (_, i) => ampToByte(1 / (1 + i % WT.HARMONICS))) };
      if (caps & 4) {
        this.multiStored[2] = "TESTONE"; this.multiStored[3] = "TESTTWO"; this.multiStored[11] = "LIVE SET";
      }
    }
    this.rebuild();
    if (seed && (caps & 4)) this.objs.set(21, buildPatternObject([null, null, { name: "PAT THREE", len: 12 }, null, null, null]));
  }
  // the objects the unit would list for its patches and multis
  rebuild() {
    const real = (this.caps & 16) !== 0;                // the 16 chunks of 8 slots hold whole records; the two chunks of an older unit keep 20 byte stubs
    const rec = (lo, hi) => [...this.patches].filter(([s]) => s >= lo && s < hi).sort((a, b) => a[0] - b[0])
      .map(([slot, v]) => (real ? { bytes: this.withPos(slot, v) } : { slot, name: v.name, engine: v.eng, cat: v.cat }));
    const chunks = real ? [...PATCH_IDS_6, ...PATCH_IDS_X].map((id, c) => [id, rec(8 * c, 8 * c + 8)]) : [[6, rec(0, 64)], [7, rec(64, 128)]];   // 16 chunks of 8 slots, or the old two
    for (const [id, r] of chunks) if (r.length) this.objs.set(id, buildPatchChunk(r)); else this.objs.delete(id);
    if (this.caps & 8) this.tables.forEach((t, u) => { if (t) this.objs.set(24 + u, buildObject(t.name, t.data)); else this.objs.delete(24 + u); });
    if (this.caps & 4) for (let o = 0; o < 8; o++) {
      const n = [this.multiStored[2 * o], this.multiStored[2 * o + 1]];
      if (n[0] == null && n[1] == null) this.objs.delete(13 + o); else this.objs.set(13 + o, buildMultiObject(n));
    }
  }
  // the stored record of a saved slot (built from the entry when it has none yet), as the unit holds it: slot and revision set
  recordOf(v) {
    if (!v.rec) {
      let vals = v.p.slice(); for (let i = 0; i < vals.length; i++) vals[i] = Math.max(-64, Math.min(127, vals[i]));
      v.rec = buildRecord({ engine: v.eng, name: v.name, cat: v.cat, values: vals, patch128: v.eng === FM6_E && v.fm6 !== null ? (v.fm6 || INIT128) : null });
    }
    return v.rec;
  }
  withPos(slot, v) { const b = this.recordOf(v).slice(); b[2] = slot; b[3] = v.rev ?? 0; return b; }
  setRecord(slot, bytes) {                              // a record written whole (PATCH_RAW, a restored chunk): the entry follows it
    const r = parseRecord(bytes), old = this.patches.get(slot);
    this.patches.set(slot, { name: r.name, eng: r.engine, cat: r.cat, p: r.values.slice(), preset: 0, fm6: r.fm6 ? Uint8Array.from(r.fm6) : null, rec: Uint8Array.from(bytes).fill(0, 2, 4), rev: old ? ((old.rev ?? 0) + 1) & 255 : 0 });
  }
  // a BACKUP_PUT committed object id: take the patches / multis from the objects
  fromObjects(id) {
    if (PATCH_IDS_6.includes(id) || PATCH_IDS_X.includes(id)) {
      const old = this.patches; this.patches = new Map();
      const best = new Map();                              // the whole records of all chunks (the newer revision of a slot wins)
      for (const i of [...PATCH_IDS_6, ...PATCH_IDS_X]) {
        const b = this.objs.get(i); if (!b || b.length < 4) continue;
        for (let off = 4; off + 20 <= b.length;) {
          const rl = b[off] | b[off + 1] << 8; if (rl < 20) break;
          const slot = b[off + 2], prev = best.get(slot);
          if (!prev || ((b[off + 3] - prev.rev) & 255) < 128 && b[off + 3] !== prev.rev) best.set(slot, { rev: b[off + 3], bytes: b.slice(off, off + rl) });
          off += rl;
        }
      }
      for (const p of decodePatches([...PATCH_IDS_6, ...PATCH_IDS_X].map((i) => this.objs.get(i)))) {
        const whole = best.get(p.slot), parsed = whole && whole.bytes.length > 20 ? parseRecord(whole.bytes) : null;
        if (parsed?.ok) this.patches.set(p.slot, { name: parsed.name, eng: parsed.engine, cat: parsed.cat, p: parsed.values.slice(), preset: 0, fm6: parsed.fm6 ? Uint8Array.from(parsed.fm6) : null, rec: whole.bytes.slice().fill(0, 2, 4), rev: whole.rev });
        else this.patches.set(p.slot, { name: p.name, eng: p.engine, cat: CATS.indexOf(p.category), ...(old.get(p.slot) || defaultPart(p.engine)) });
      }
    }
    if (id >= 24 && id <= 26) { const b = this.objs.get(id); this.tables[id - 24] = b && b.length ? parseObject(b) : null; }
    if (id >= 13 && id <= 20) {
      this.multiStored = Array(16).fill(null);
      for (const m of decodeMultis([...Array(8).keys()].map((o) => this.objs.get(13 + o) || null))) this.multiStored[+m.label.slice(1) - 1] = m.name;
    }
  }
  info() {
    const s = (t) => [...t].map((c) => c.charCodeAt(0)).concat(0);
    return [...s(this.version), MOCK_ENGINES.length, L.P_COUNT, L.G_COUNT, L.NSTEP, L.P_E0, ...MOCK_ENGINES.flatMap(s), 4, 0, 0x55, 1, 9, 0x4d, 1, 64, 1, 0x42, 1, this.caps,
      0x46, 1, 8, 0, 0x53, 1, 3, 0x50, 1, 3, 0x4e, 1, this.menu.length, 0x43, 1, 16, ...(this.caps & 16 ? [0x58, 1, this.capacity & 127, this.capacity >> 7] : []), ...(this.patchRaw ? [0x5a, 1, 1] : [])];
  }
  get capacity() { return this.caps & 16 ? 128 : 48; }                // the patches the unit holds (INFO tag 58 01 with caps bit 4)
  has(id) { return id <= 12 || id === 32 || id === 33 || id === 34 || (PATCH_IDS_X.includes(id) ? (this.caps & 16) !== 0 : id >= 24 ? (this.caps & 8) !== 0 : (this.caps & 4) !== 0); }
  push(cmd, args) { if (this.watching && this.onpush) this.onpush(cmd, args); }
  // ---- what the unit's panel does (tests and the page's "turn a knob on the unit" button)
  deviceTurn(part, id, v) {
    this.parts[part].p[id] = v;
    if (part === this.selected) this.push(CMD.CHANGED, [0, id, ...v14(v)]);
    else if (id === 0 || id === 39 || id === 40) this.push(CMD.TRACK_CHANGED, [part, id, ...v14(v)]);
  }
  deviceSelect(part) { this.selected = part; this.push(CMD.RELOAD, [this.parts[part].eng, this.parts[part].preset, part]); }
  devicePalette(i) { this.menu.find((m) => m[0] === 0)[6] = i; }   // MENU > COLOR on the panel (the page finds it by polling MENU_DESC)
  deviceGlobal(id, v) { this.g[id] = v; this.push(CMD.CHANGED, [1, id, ...v14(v)]); }
  async request([cmd, a], opt = {}) {
    this.log.push(`cmd ${cmd}`);
    if (cmd >= WT.CMD.LIST && cmd <= WT.CMD.PUT && (this.caps & 8)) return this.wavetable(cmd, a);
    if (cmd <= 100 && cmd !== 65 && cmd !== 66 && cmd !== 67) { const r = this.edit(cmd, a); if (r !== undefined) return r; }
    if (cmd === 1) return this.info();
    if (cmd === BACKUP_CMD.LIST) {
      if (this.playing) return [1, 3, 0];
      this.listed = true;
      const ids = [...Array(13).keys(), ...(this.caps & 4 ? [13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23] : []), ...(this.caps & 8 ? [24, 25, 26] : []), ...(this.caps & 16 ? [27, 28, 29, 30, 31] : []), 32, 33, 34, ...(this.caps & 16 ? [35, 36, 37, 38, 39] : [])];
      return [1, 0, ids.length, ...ids.flatMap((id) => { const v = this.objs.get(id); return [id, ...bkU32(v ? v.length : 0), ...bkU32(v && v.length ? bkCrc(v) : 0)]; })];
    }
    if (cmd === BACKUP_CMD.GET) {
      const id = a[0], off = bkR32(a, 1), n = a[6] | a[7] << 7, v = this.objs.get(id) || new Uint8Array(0);
      const rc = !this.listed ? 5 : this.playing ? 3 : !this.has(id) || !n || n > 256 || off > v.length || n > v.length - off ? 1 : 0;
      return [id, rc, ...bkU32(off), rc ? 0 : n & 127, rc ? 0 : n >> 7, ...(rc ? [] : bkPack(v.subarray(off, off + n)))];
    }
    if (cmd === BACKUP_CMD.PUT) return this.put(a);
    if (cmd >= 11 && cmd <= 14) return cmd === 12 ? [a[0], a[1], a[2], a[3], 6] : [a[0], cmd === 14 ? 0 : 6];   // the retired sample slots (editor.c: ERASE rc 0, BEGIN / WRITE / END rc 6)
    throw new Error(`mock unit: unexpected command ${cmd}`);
  }
  // ---- the editor commands (undefined: not one of them)
  edit(cmd, a) {
    const P = L.P_COUNT, G = L.G_COUNT, part = () => this.parts[this.selected];
    const dump = (t) => [...v14(0).slice(0, 0), ...Array.from({ length: P }, (_, i) => v14(this.parts[t].p[i])).flat()];
    switch (cmd) {
      case CMD.PING: return [0];
      case CMD.WATCH: this.watching = !!(a[0] & 1); return [a[0] & 3];
      case CMD.TRACK:
        if (a.length) { if (a[0] > 3) return undefined; this.selected = a[0]; }
        return [this.selected, 4, ...[0, 1, 2, 3].flatMap((t) => { const q = this.parts[t] || this.parts[1]; return [q.eng, q.preset, ...v14(q.p[0]), q.p[40], 0]; })];
      case CMD.TRACK_DUMP: { const t = a[0]; if (t > 3) return undefined; const q = this.parts[Math.min(t, 1)]; return [t, q.eng, q.preset, ...dump(Math.min(t, 1))]; }
      case CMD.DUMP: return [part().eng, part().preset, ...dump(this.selected), ...this.g.flatMap((x) => v14(x))];
      case CMD.GET: return a[0] ? [1, a[1], ...v14(this.g[a[1]])] : [0, a[1], ...v14(part().p[a[1]])];
      case CMD.SET: return this.setScoped(a[0], a[1], rv14(a, 2));
      case CMD.TRACK_PARAM: {
        const t = a[0], id = a[1]; if (t > 3 || id >= P) return undefined;
        const q = this.parts[Math.min(t, 1)];
        if (a.length === 2) return [t, id, ...v14(q.p[id])];
        q.p[id] = clampTo(descOf(q, id), rv14(a, 2)); return [t, id, ...v14(q.p[id])];
      }
      case CMD.DESC: {
        const d = a[0] ? L.GP[a[1]] : descOf(part(), a[1]);
        return [a[0], a[1], d.fmt, ...v14(d.min), ...v14(d.max), ...v14(d.def), ...cstr(d.label), ...cstr(d.unit || ""), ...(d.names ? d.names.flatMap(cstr) : [])];
      }
      case CMD.NAMES: { const e = L.ENG[a[0]]; return [a[0], e.sounds.length, ...e.sounds.flatMap(([n]) => cstr(n)), ...cstr(e.titles[0]), ...cstr(e.titles[1])]; }
      case CMD.PRESET: { const q = defaultPart(a[0], part()); q.preset = a[1]; this.parts[this.selected] = q; return [a[0], a[1]]; }
      case CMD.UP_LIST: {                              // every slot holds a sound; then the categories, then (0.2.17) SAVED per slot
        const start = a[0], count = Math.min(a[1], 16, 128 - start), rows = [], cats = [], saved = [];
        for (let i = 0; i < count; i++) {
          const v = this.patches.get(start + i), f = BROWSE[start + i];
          rows.push([1, v ? v.eng : f.eng, ...cstr(v ? v.name : f.name)]); cats.push(v ? v.cat : f.cat); saved.push(v ? 1 : 0);
        }
        return [start, count, 127, ...rows.flat(), ...cats, ...(this.savedFlags ? saved : [])];   // (savedFlags false: a unit that predates the SAVED bytes)
      }
      case CMD.UP_LOAD: {                              // the slot's record, else its factory sound
        if (a[0] > 127) return [a[0], 1];
        const v = this.patches.get(a[0]);
        if (!v) { const f = BROWSE[a[0]], q = defaultPart(f.eng, part()); q.preset = f.idx; this.parts[this.selected] = q; return [a[0], 0]; }
        // a record: every parameter but the part's own (ui.c param_kept: the mixer LEVEL (0.2.19), PAN, MUTE, the arp and sequencer ids 17..32, CHRD, VOIC, QNT); the sound's loudness is its TRIM
        const cur = part(), kept = (i) => i === 0 || i === 39 || i === 40 || (i >= 17 && i <= 32) || i === 81 || i === 82 || i === 94;
        const q = { ...defaultPart(v.eng, cur), p: v.p.map((x, i) => (kept(i) ? cur.p[i] : x)) };
        q.eng = v.eng;
        if (v.eng === FM6_E) q.fm6 = v.fm6 ? Uint8Array.from(v.fm6) : INIT128.slice();
        this.parts[this.selected] = q; return [a[0], 0];
      }
      case CMD.UP_STORE: {                             // over a slot: it holds the part's sound now (no 'full': every slot can be written)
        if (this.playing || a[0] > 127) return [a[0], 2];
        const [nm] = rstr(a, 1), q = part();
        if (nm.length > 12) return [a[0], 1];
        this.patches.set(a[0], { name: nm || `${L.ENG[q.eng].name} ${String(a[0] + 1).padStart(2, "0")}`, eng: q.eng, cat: 8, eng2: q.eng, p: q.p.map((v, i) => (i === ID.P_LEVEL ? L.TP[i].def : v)), preset: q.preset, fm6: q.eng === FM6_E ? (q.fm6 ? Uint8Array.from(q.fm6) : INIT128.slice()) : null, rev: (this.patches.get(a[0])?.rev ?? -1) + 1 & 255 });
        this.log.push(`store ${a[0]}`); this.rebuild(); return [a[0], 0];
      }
      case CMD.UP_ERASE:                               // RESTORE: the saved record is removed, the factory sound is back (rc 0 and no write when it was never saved)
        if (a[0] > 127) return [a[0], 1];
        if (!this.patches.has(a[0])) return [a[0], 0];
        if (this.playing) return [a[0], 2];
        this.patches.delete(a[0]); this.log.push(`restore ${a[0]}`); this.rebuild(); return [a[0], 0];
      case CMD.UP_GET: {                               // slot -> slot, used, engine, name, P_COUNT x v14, 16 x (note, flags), kind 0, category, SAVED
        if (a.length !== 1 || a[0] > 127) return undefined;
        const v = this.patches.get(a[0]), f = BROWSE[a[0]], e = v || { eng: f.eng, name: f.name, cat: f.cat, p: defaultPart(f.eng).p };
        return [a[0], 1, e.eng, ...cstr(e.name), ...e.p.flatMap(v14), ...new Array(32).fill(0), 0, e.cat, v ? 1 : 0];
      }
      case CMD.UP_PUT: {                               // slot, engine, name, values, 16 steps [, kind 0, 16 x 0, category]: lossy by design (no End B, no arp block, no FM6 patch)
        const n = L.P_COUNT, [nm, at] = rstr(a, 2), end = at + 2 * n + 32, ret = (rc) => [a[0], rc];
        if (a[0] > 127 || a[1] >= L.NENGINES || !/^[ -~]{1,12}$/.test(nm) || (a.length !== end && !((a.length === end + 17 || a.length === end + 18) && a[end] === 0))) return ret(1);
        const vals = Array.from({ length: n }, (_, i) => rv14(a, at + 2 * i));
        if (vals.some((x) => x < -64 || x > 127)) return ret(1);
        if (this.playing) return ret(2);
        this.patches.set(a[0], { name: nm, eng: a[1], cat: a.length === end + 18 ? a[end + 17] : 0, p: vals, preset: 0, fm6: null, rev: (this.patches.get(a[0])?.rev ?? -1) + 1 & 255 });
        this.log.push(`put ${a[0]}`); this.rebuild(); this.listed = false; return ret(0);
      }
      case CMD.PATCH_RAW: return this.patchRaw ? this.rawCmd(a) : undefined;
      case CMD.FM6_GET: case CMD.FM6_PUT: {            // target 0 (a part's own patch) only
        if (a[0] !== 0 || a[1] > 3) return [a[0], a[1], 1];
        const q = this.parts[Math.min(a[1], 1)];
        if (cmd === CMD.FM6_GET) return [a[0], a[1], 0, ...(q.fm6 || INIT128)];
        if (a.length !== 130) return [a[0], a[1], 1];
        q.fm6 = Uint8Array.from(a.slice(2)); q.p[P_E0 + 7] = 8; return [a[0], a[1], 0];
      }
      case CMD.PROJECT: {
        if (a[1] > 15) return undefined;
        const used = this.multiStored[a[1]] != null || a[1] < 10 || this.multiUsedExtra.has(a[1]);
        if (a[0] === 2) return [2, a[1], used ? 1 : 0];
        if (a[0] === 1) {
          if (this.playing) throw new Error("mock unit: no reply (playing)");
          if (this.multiStored[a[1]] == null) this.multiStored[a[1]] = `MULTI ${a[1] + 1}`;
          this.multiSaved = { part0: this.parts[0], part1: this.parts[1] }; this.rebuild(); return [1, a[1], 1];
        }
        if (a[0] === 0) { this.push(CMD.RELOAD, [this.parts[this.selected].eng, 0, this.selected]); return [0, a[1], used ? 1 : 0]; }
        return undefined;
      }
      case CMD.MENU_DESC: {
        const m = this.menu[a[0]]; if (!m) return [a[0], 127];
        return [a[0], m[0], m[2], ...v14(m[6]), ...v14(m[4]), ...v14(m[5]), ...cstr(m[1]), ...(m[2] === 0 ? m[3].flatMap(cstr) : cstr(m[3])), m[7], ...cstr(TABS[m[7]])];
      }
      case CMD.MENU_SET: {
        const m = this.menu.find((x) => x[0] === a[0]); if (!m) return [1, a[0], ...v14(rv14(a, 1))];
        m[6] = Math.min(m[5], Math.max(m[4], rv14(a, 1))); return [0, m[0], ...v14(m[6])];
      }
      case CMD.LEARN_GET: return this.learnReply(false);
      case CMD.LEARN_SET: {
        if (a[0] === 2) this.learn = [];
        else if (a[0] === 1) this.learn = this.learn.filter((x) => x.cc !== a[1]);
        else if (a[0] === 0) {
          if ([0, 1, 6, 11, 32, 38, 64].includes(a[1]) || a[1] > 127) return [1, ...this.learnReply(false)];
          this.learn = this.learn.filter((x) => x.cc !== a[1] && !(x.track === a[2] && x.id === a[3]));
          if (this.learn.length >= 16) return [5, ...this.learnReply(false)];
          this.learn.push({ cc: a[1], track: a[2], id: a[3] });
        }
        return [0, ...this.learnReply(false)];
      }
      case CMD.TRACK_STEP: return a[0] > 3 ? undefined : this.stepCmd(Math.min(a[0], 1), a[0], a.slice(1), 1);
      case CMD.STEP_GET: case CMD.STEP_SET: return this.stepCmd(this.selected, this.selected, a, 0);
      default: return undefined;
    }
  }
  // PATCH_RAW (editor_patchraw.c): the same rules as the unit: INFO / READ never write, BEGIN / DATA / COMMIT are refused while playing (rc 3), CRC and a record the store reads at commit
  rawCmd(a) {
    const op = a[0], slot = a[1], ret = (rc) => [op, slot ?? 127, rc];
    if (a.length < 2) return [a[0] ?? 127, 127, 1];
    if (op === 0 || op === 1) {
      const v = slot < 128 ? this.patches.get(slot) : null, bytes = v ? this.recordOf(v).slice().fill(0, 2, 4) : null;
      if (op === 0) {
        const rc = slot >= 128 || a.length !== 2 ? 1 : 0;
        return [0, slot, rc, bytes ? 1 : 0, ...(bytes ? [bytes.length & 127, bytes.length >> 7] : [0, 0]), ...bkU32(bytes ? bkCrc(bytes) : 0)];
      }
      const off = a[2] | a[3] << 7, n = a[4], rc = a.length !== 5 || slot >= 128 || !bytes || !n || n > PIECE || off > bytes.length || n > bytes.length - off ? 1 : 0;
      return [1, slot, rc, off & 127, off >> 7, rc ? 0 : n, ...(rc ? [] : bkPack(bytes.subarray(off, off + n)))];
    }
    if (op < 2 || op > 5 || slot >= 128) return ret(1);
    if (this.fail && this.fail.cmd === 84 && this.fail.op === op) return ret(this.fail.rc);
    if (op === 2) {
      const len = a[2] | a[3] << 7;
      if (a.length !== 9 || a[8] > 15 || len < REC_FIX || len > 440) return ret(1);
      if (this.playing) return ret(3);
      this.prStage = { slot, len, crc: bkR32(a, 4), bytes: new Uint8Array(len), pos: 0 }; this.staged = null; this.wtStage = null;
      return ret(0);
    }
    const st = this.prStage;
    if (!st || st.slot !== slot) return ret(5);
    if (op === 5) { this.prStage = null; return ret(a.length === 2 ? 0 : 1); }
    if (op === 3) {
      const off = a[2] | a[3] << 7, nb = a.length - 4;
      if (a.length < 6 || off !== st.pos) return ret(1);
      let bytes; try { bytes = bkUnpack(a.slice(4), Math.floor(nb / 8) * 7 + Math.max(0, nb % 8 - 1)); } catch { return ret(1); }
      if (!bytes.length || bytes.length > PIECE || bytes.length > st.len - st.pos) return ret(1);
      if (this.playing) return ret(3);
      st.bytes.set(bytes, st.pos); st.pos += bytes.length;
      return ret(0);
    }
    if (a.length !== 2) return ret(1);                    // 4: commit
    if (this.playing) return ret(3);
    this.prStage = null;
    if (st.pos !== st.len || bkCrc(st.bytes) !== st.crc) return ret(2);
    const r = parseRecord(st.bytes);
    if (!r.ok || !/^[ -~]{1,12}$/.test(r.name) || st.bytes.slice(7, 19).some((x, i, w) => x === 0 && w.slice(i).some((y) => y))) return ret(1);
    this.setRecord(slot, st.bytes);
    this.log.push(`raw write ${slot}`); this.rebuild(); this.listed = false;
    return ret(0);
  }
  // STEP_GET / STEP_SET (the selected part) and TRACK_STEP (any part): [index] or [index, n, note0..3, time, flags, vel [, hits x3 [, chance [, ratchet [, nudge + 8]]]]].
  // The unit's rules (EDITOR_PROTOCOL.md): every data byte 7 bit; a chance above 100, a ratchet outside 1..4, a nudge byte above 15, more than 4 notes or a bad time: no reply
  // (the mock throws). Without the optional bytes a step keeps its own. A STEP_SET answers the step as stored; TRACK_STEP's reply starts with the track byte.
  stepCmd(t, trackByte, a, withTrack) {
    const s = this.steps[t]?.[a[0]]; if (!s || a[0] > 63) return undefined;
    if (a.length > 1) {
      const noReply = () => { throw new Error("mock unit: no reply (a bad step)"); };
      if (a.length < 9 || a.length > 15 || a.some((x) => !(x >= 0 && x <= 127))) noReply();
      const [, n, n0, n1, n2, n3, time, flags, vel] = a, hits = a.length >= 12 ? a.slice(9, 12) : s.hits, chance = a.length >= 13 ? a[12] : s.chance, ratchet = a.length >= 14 ? a[13] : s.ratchet, nudge = a.length >= 15 ? a[14] : s.nudge;
      if (n > 4 || time > 2 || chance > 100 || ratchet < 1 || ratchet > 4 || nudge > 15) noReply();
      Object.assign(s, { n, notes: [n0, n1, n2, n3], time, flags, vel, hits, chance, ratchet, nudge });
      this.log.push(`step ${t}:${s.index}`);
    }
    return [...(withTrack ? [trackByte] : []), s.index, s.n, ...s.notes, s.time, s.flags, s.vel, ...s.hits, s.chance, s.ratchet, s.nudge];
  }
  // the unit's panel edits step i of a part (STEP_CHANGED is pushed with the part; not for the editor's own writes)
  deviceStep(part, i, patch) { Object.assign(this.steps[part][i], patch); this.push(CMD.STEP_CHANGED, [i, part]); }
  learnReply() { const r = [16]; for (let i = 0; i < 16; i++) { const e = this.learn[i]; r.push(...(e ? [1, e.cc, e.track, e.id] : [0, 0, 0, 0])); } return r; }
  setScoped(scope, id, v) {
    if (scope === 1) {
      if (id >= L.G_COUNT) return undefined;
      if (id === ID.G_ENGSEL) {
        const e = clampTo(L.GP[id], v), q = defaultPart(L.ENG[e].name === "-" ? 0 : e, this.parts[this.selected]);
        this.parts[this.selected] = q; this.g[id] = q.eng; return [1, id, ...v14(q.eng)];
      }
      this.g[id] = clampTo(L.GP[id], v); return [1, id, ...v14(this.g[id])];
    }
    const q = this.parts[this.selected]; q.p[id] = clampTo(descOf(q, id), v); return [0, id, ...v14(q.p[id])];
  }
  // the user wavetables (editor_wtab.c): WT_LIST 81, WT_GET 82, WT_PUT 83 with the unit's rules and rc
  wavetable(cmd, a) {
    if (cmd === WT.CMD.LIST) {
      return [WT.SLOTS, WT.FRAMES, WT.HARMONICS, ...this.tables.flatMap((t) => [t ? 1 : 0, ...[...(t ? t.name : "")].map((c) => c.charCodeAt(0)), 0, ...bkU32(t ? bkCrc(t.data) : 0)])];
    }
    if (cmd === WT.CMD.GET) {
      const [slot, lo, hi, n] = a, off = lo | hi << 7, t = this.tables[slot];
      const rc = a.length !== 4 || !t || !n || n > 127 || off > WT.BYTES || n > WT.BYTES - off ? 1 : 0;
      return [slot, rc, lo, hi, rc ? 0 : n, ...(rc ? [] : bkPack(t.data.subarray(off, off + n)))];
    }
    const [op, slot] = a, ret = (rc) => [op, slot, rc];
    if (a.length < 2 || op > 4 || slot >= WT.SLOTS) return ret(1);
    if (this.fail && this.fail.cmd === 83 && this.fail.op === op) return ret(this.fail.rc);
    if (op === 0) {
      if (a.length !== 15 || a[14] > 15 || a.slice(2, 10).some((c) => c && (c < 32 || c > 126))) return ret(1);
      if (this.playing) return ret(3);
      this.wtStage = { slot, name: String.fromCharCode(...a.slice(2, 10).filter(Boolean)), crc: bkR32(a, 10), data: new Uint8Array(WT.BYTES), pos: 0 };
      this.staged = null;
      return ret(0);
    }
    if (op === 4) { if (a.length !== 2) return ret(1); if (this.playing) return ret(3); this.wtStage = null; this.tables[slot] = null; this.rebuild(); this.log.push(`wt erase ${slot}`); return ret(0); }
    const st = this.wtStage;
    if (!st || st.slot !== slot) return ret(5);
    if (op === 3) { this.wtStage = null; return ret(a.length === 2 ? 0 : 1); }
    if (op === 1) {
      const off = a[2] | a[3] << 7;
      if (a.length < 6 || off !== st.pos) return ret(1);
      let bytes; try { bytes = bkUnpack(a.slice(4), Math.floor((a.length - 4) / 8) * 7 + Math.max(0, (a.length - 4) % 8 - 1)); } catch { return ret(1); }
      if (!bytes.length || bytes.length > WT.BYTES - st.pos) return ret(1);
      if (this.playing) return ret(3);
      st.data.set(bytes, st.pos); st.pos += bytes.length;
      return ret(0);
    }
    if (a.length !== 2) return ret(1);
    if (this.playing) return ret(3);
    this.wtStage = null;
    if (st.pos !== WT.BYTES || bkCrc(st.data) !== st.crc || !st.data.some((b) => b)) return ret(2);
    this.tables[slot] = { name: cleanName(st.name), data: st.data };
    this.log.push(`wt write ${slot}`);
    this.rebuild(); this.listed = false;
    return ret(0);
  }
  put(a) {
    const [op, id] = a, ret = (rc) => [op, id, rc];
    if (this.fail && this.fail.id === id && this.fail.op === op) return ret(this.fail.rc);
    if (op === 0) {
      if (!RETIRED_IDS.includes(id) && (!DUELL_IDS.includes(id) || !this.has(id))) return ret(1);   // (the retired ids 2..5 and 8 are accepted and dropped, as editor_backup.c does for an older archive)
      const size = bkR32(a, 2), crc = bkR32(a, 7);
      const bad = size > 3840 || (id === 0 && size !== 3840) || (id === 1 && size !== this.settingsSize) ||
        (id >= 13 && id <= 20 && size && size < 8) || (id >= 21 && id <= 23 && size && size !== 3584) || (id >= 24 && id <= 26 && size && size !== WT.OBJ);
      if (bad) return ret(1);
      if (this.playing) return ret(3);
      this.staged = { id, size, crc, bytes: new Uint8Array(size), pos: 0 };
      return ret(0);
    }
    const st = this.staged;
    if (!st || st.id !== id) return ret(5);
    if (op === 3) { this.staged = null; return ret(0); }
    if (op === 1) {
      const off = bkR32(a, 2);
      if (off !== st.pos) return ret(1);
      const piece = a.slice(7), n = Math.min(256, st.size - st.pos);
      let bytes; try { bytes = bkUnpack(piece, n); } catch { return ret(1); }
      st.bytes.set(bytes, st.pos); st.pos += n;
      return ret(0);
    }
    if (op === 2) {
      this.staged = null;
      if (st.pos !== st.size || (st.size ? bkCrc(st.bytes) : 0) !== st.crc) return ret(2);
      if (RETIRED_IDS.includes(id)) { this.listed = false; return ret(0); }   // (dropped)
      if (checkObject(id, st.bytes)) return ret(2);      // (the content check)
      this.log.push(`write ${id}`);
      if (st.size) this.objs.set(id, st.bytes); else this.objs.delete(id);
      this.fromObjects(id);
      this.listed = false;                              // (a write reuses the staging RAM: the next GET wants a fresh LIST)
      return ret(0);
    }
    return ret(1);
  }
}
const RETIRED_IDS = [2, 3, 4, 5, 8];
const CATS = ["-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "PERC", "FX", "OTHER"];

// A link to a MockUnit with the real link's interface (editor/?mock=1 and the tests): request(), keepAlive(), close(), onpush.
export class MockLink {
  constructor(unit, delay = 0) { this.unit = unit; this.delay = delay; this.onpush = null; this.closed = false; unit.onpush = (c, a) => this.onpush?.(c, a); }
  async request(r, opt) {
    if (this.closed) throw new Error("The connection is closed");
    if (this.delay) await new Promise((res) => setTimeout(res, this.delay));
    return this.unit.request(r, opt);
  }
  keepAlive() {}
  close() { this.closed = true; this.unit.watching = false; }
}
