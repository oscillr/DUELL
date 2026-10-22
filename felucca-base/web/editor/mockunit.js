// SPDX-License-Identifier: GPL-3.0-only
// A simulated DUELL unit for the Backup page: INFO, BACKUP_LIST / GET / PUT with the unit's rules (editor_backup.c: arguments first, the
// transport stopped or rc 3, staging, CRC and content checked at commit, an empty object for length 0). Used by test_duellbackup.mjs and by
// editor/?mock=1 (try the page without a unit). It is NOT the firmware: the C tests (tests/backup_test.c) cover that.
import { BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc } from "../fm1backup.js";
import { PS as PATS, buildPatObject, nslotOf } from "./patslot.js";
import { checkObject, DUELL_IDS, decodeMultis, OLD_PATCH_IDS, CAPS_PATCH_RECORDS, PATCH_ID, PATCH_PAGE, PATCH_PAGES, PATCH_MAX } from "./duellbackup.js";
import { WT, buildObject, parseObject, cleanName, ampToByte } from "./wavetable.js";
import { CMD, v14, rv14, cstr, rstr } from "./proto.js";
import { L, ID, X, NX, descOf, xdescOf, xDefaults, clampTo, P_E0 } from "./layout.js";
import { buildRecord, parseRecord, recordProblem, storeDefaults, engineIndex, FIX as REC_FIX, WORST as REC_WORST } from "./patchrecord.js";
import { FM6 } from "./fm6voice.js";

const put16 = (b, o, v) => { b[o] = v & 255; b[o + 1] = v >> 8 & 255; };
const putName = (b, o, s) => { for (let i = 0; i < 12; i++) b[o + i] = i < s.length ? s.charCodeAt(i) : 0; };
export const MOCK_ENGINES = L.ENG.map((e) => e.name);   // the firmware's own list (layout.json)
const FM6_E = engineIndex("FM6"), INIT128 = Uint8Array.from(FM6.INIT_PK);
const PIECE = 112;

// builders for the objects the unit stores (the same layouts as the firmware: upreset.c, multi.c, patslot.c)
export function buildPatchRecord({ name, engine = 0, cat = 0, values = null, ...more }) {   // a patch record for tests (patchrecord.js buildRecord; the sparse 20 byte stub when only a name and engine are given); slot and revision 0
  return buildRecord({ engine, name, cat, values: values ?? storeDefaults(engine), ...more });
}
// the physical chunk of the patch store of an OLDER firmware (DUELL 0.2.14 .. 0.2.25, no caps bit 5): the unit of such a caps value still has them, for the installer's backup (fm1backup.js)
export function buildLegacyPatchChunk(records) {      // [{slot, rev, name, engine, cat}] (a 20 byte stub each) or [{bytes}] (a whole record, its slot and revision set by the caller)
  const b = new Uint8Array(4 + records.reduce((n, r) => n + (r.bytes ? r.bytes.length : 20), 0));
  b[0] = 0x50; b[1] = 0x54; b[2] = 2;   // the chunk version 2 (DUELL 0.2.22, upreset.c PT_VER)
  let o = 4;
  for (const r of records) {
    if (r.bytes) { b.set(r.bytes, o); o += r.bytes.length; continue; }
    put16(b, o, 20); b[o + 2] = r.slot; b[o + 3] = r.rev ?? 1; b[o + 5] = r.engine ?? 0; b[o + 6] = r.cat ?? 0; putName(b, o + 7, r.name);
    o += 20;
  }
  return b;
}
const legacyChunkOk = (b) => !b.length || (b.length >= 4 && b[0] === 0x50 && b[1] === 0x54 && b[2] === 2);
export function buildMultiObject(names) {              // [nameA | null, nameB | null]
  const recs = names.map((n) => {
    if (n == null) return new Uint8Array(0);
    const r = new Uint8Array(40); r[0] = 7; r[1] = 127; putName(r, 8, n); r[20] = 7; return r;
  });
  const b = new Uint8Array(8 + recs[0].length + recs[1].length);
  b[0] = 0x4D; b[1] = 0x55; b[2] = 0x4C; b[3] = 0x31; put16(b, 4, recs[0].length); put16(b, 6, recs[1].length);
  b.set(recs[0], 8); b.set(recs[1], 8 + recs[0].length);
  return b;
}
// The unit's 32 default patterns (engines.c PATTERNS[]: name, LEN, DIV; PATTERNS_PROPOSAL.md) as the objects 21..23 hold them (PTS3, patslot.js). The steps here are simple stand-ins (the
// unit's own notes are in the firmware); the format, the names and the 11 + 11 + 10 split are the unit's.
const DEFAULT_PATTERNS = [["MORODER OCT", 16, 2], ["ACID LINE", 16, 2], ["OFFBEAT", 16, 2], ["TECHNO RUMBL", 16, 2], ["TRANCE ROLL", 16, 2], ["8TH PULSE", 16, 2], ["GLASS LOOP", 16, 2], ["BERLIN RUN", 16, 2],
  ["DETROIT STAB", 16, 2], ["FUNK 16TH", 16, 2], ["DUB HALFTIME", 16, 2], ["SKANK", 16, 2], ["GALLOP", 16, 2], ["PENTA RIFF", 16, 2], ["BOOGIE BASS", 16, 2], ["DOTTED HOOK", 16, 2], ["SUB DROP", 16, 2],
  ["TUMBAO", 16, 2], ["MOTORIK", 16, 2], ["WALK LINE", 8, 1], ["2-STEP SUB", 16, 2], ["GHOST FUNK", 16, 2], ["TRIPLET SWAY", 12, 4], ["TRANCE GATE", 16, 2], ["RAVE STABS", 16, 2], ["EP COMP", 16, 2],
  ["NEON HOOK", 16, 2], ["NIGHT CALL", 32, 2], ["PAD BREATH", 16, 2], ["DX OSTINATO", 16, 2], ["DX BELLS", 16, 2], ["SEVEN EIGHT", 14, 2]];
export function mockPatternRecord({ name, len = 16, div = 2, changed = 0 }) {      // one slot as the editor's 596-byte record
  const r = new Uint8Array(PATS.REC); r.set([PATS.USED, PATS.VER, len, div, 0, 64, 1, changed]); putName(r, 8, name);
  for (let k = 0; k < 64; k++) { const t = 20 + k * 9; if (k < len && k % 4 === 0) { r[t] = 48 + (k % 12); r[t + 4] = 1; r[t + 5] = 100; } else r[t + 4] = 2 << 3; }
  return r;
}
export function buildPatternObject(o, over = {}) {      // object o (0..2): the defaults of its slots, `over`: slot index in the object -> {name, len, changed}
  return buildPatObject(o, Array.from({ length: nslotOf(o) }, (_, i) => { const d = DEFAULT_PATTERNS[o * PATS.PER + i]; return mockPatternRecord({ name: d[0], len: d[1], div: d[2], ...(over[i] || {}) }); }));
}
const filler = (n, seed, magic) => { const b = Uint8Array.from({ length: n }, (_, i) => (i * 31 + seed * 7 + (i >> 4)) & 255); b.set(magic, 0); return b; };

const MENU = [   // MENU_DESC items, DUELL 0.2.14 (EDITOR_PROTOCOL.md "MENU settings"): [id, name, kind, names|unit, min, max, value, tab]
  [0, "COLOR", 0, L.PALETTES.LIST.map((p) => p.name), 0, L.PALETTES.LIST.length - 1, L.PALETTES.DEFAULT, 0],   // the firmware's palettes (layout.json)
  [3, "ANIMATION", 0, ["ON", "OFF", "IDLE"], 0, 2, 0, 0], [4, "LEDS", 0, ["OFF", "LOW", "MEDIUM", "MAX"], 0, 3, 2, 0],
  [5, "BUTTON HOLD", 0, ["0.3 s", "0.4 s", "0.5 s", "0.6 s"], 0, 3, 1, 1], [6, "KNOB ACCEL", 0, ["OFF", "ON"], 0, 1, 1, 1],
  [11, "USB SERIAL", 0, ["ON", "OFF"], 0, 1, 1, 3], [12, "CLICK", 0, ["OFF", "REC", "ON"], 0, 2, 1, 2],
  [13, "CLICK LEVEL", 0, ["LOW", "MID", "HIGH"], 0, 2, 1, 2], [14, "COUNT-IN", 0, ["OFF", "1 BAR", "2 BARS"], 0, 2, 0, 2],
  [15, "RESTORE LAST", 0, ["ON", "OFF"], 0, 1, 0, 3], [16, "SCALE LEDS", 0, ["AUTO", "OFF"], 0, 1, 0, 0],
  [17, "SCREEN OFF", 0, ["NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"], 0, 4, 0, 0], [19, "STEP PREVIEW", 0, ["OFF", "ON"], 0, 1, 0, 1],
  [20, "STEP KEYS", 0, ["WRITE", "ADD"], 0, 1, 0, 1], [22, "TUNE", 2, "Hz", 4153, 4662, 4400, 2],   // 0.2.22: kind 2, A4 in 0.1 Hz (415.3 .. 466.2 Hz)
  [25, "HOME OPENS", 0, ["MORPH", "MODE"], 0, 1, 0, 1], [26, "CPU METER", 0, ["ON", "OFF"], 0, 1, 0, 0],
  [27, "MULTI LOAD", 0, ["KEEPS MODE", "SETS MODE"], 0, 1, 0, 1],
  [28, "ARP", 0, ["OFF", "ON"], 0, 1, 1, 4], [29, "SEQ", 0, ["OFF", "ON"], 0, 1, 1, 4],      // GLO > MODULES (ids 28..31, tab 4 "MODULES": no MENU tab on the unit)
  [30, "SENDS", 0, ["OFF", "ON"], 0, 1, 1, 4], [31, "PART FX", 0, ["OFF", "ON"], 0, 1, 1, 4],
  [32, "PERF LEAVE", 0, ["FADE", "KEEP"], 0, 1, 0, 1],                                       // CONTROL: PERFORM's SPACE and KILL on leaving / a sound load
  [33, "BASS MONO", 0, ["OFF", "40 Hz", "50 Hz", "63 Hz", "80 Hz", "100 Hz", "125 Hz", "160 Hz", "200 Hz", "250 Hz", "315 Hz", "400 Hz"], 0, 11, 0, 5],   // MASTER (DUELL 0.2.19, tab 5 "MASTER": the unit's GLO > BASS MONO (MASTER) page, no MENU tab): the master mono below the corner; the unit does not run it in DUEL
  [34, "THRESH", 1, "dB", -40, 0, -17, 5], [35, "RATIO", 0, ["OFF", "1.5:1", "2:1", "3:1", "4:1", "6:1", "8:1", "12:1", "INF"], 0, 8, 0, 5],   // COMP (DUELL 0.2.21, ids 34..37, tab 5 "MASTER": GLO > COMP): the master compressor, a device setting
  [36, "ATTACK", 1, "", 0, 120, 80, 5], [37, "RELEASE", 1, "", 0, 80, 40, 5],   // 0.2.24: ATTACK and RELEASE are numbers, the index of 40 steps a decade (editor_menu.c: the editor formats them from layout.json's COMP)
  [38, "P1 CH", 1, "", 1, 16, 10, 6], [39, "P1 NOTE", 1, "", 0, 127, 36, 6], [40, "P2 CH", 1, "", 1, 16, 10, 6], [41, "P2 NOTE", 1, "", 0, 127, 36, 6],   // SIDECHAIN (DUELL 0.2.22, ids 38..41, tab 6 "SIDECHAIN": GLO > SIDECHAIN): the ducker's MIDI channel and note per part, a device setting (no patch, multi or project holds it)
  [42, "WIDTH", 1, "%", 0, 200, 100, 5],   // WIDTH (DUELL 0.2.23, id 42, tab 5 "MASTER": GLO > WIDTH): a device setting (0.2.25: id 43, DRIVE, the saturator, is removed); the unit does not run them in DUEL
];
const TABS = ["DISPLAY", "CONTROL", "AUDIO", "SYSTEM", "MODULES", "MASTER", "SIDECHAIN"];
const KEEP = new Set([0, 39, 40]);                       // a sound load keeps the MIXER LEVEL (0.2.19: the sound's loudness is P_TRIM, which it sets), PAN, MUTE (and the arp / sequencer ids 17..32)
const keepsOnLoad = (id) => KEEP.has(id) || (id >= 17 && id <= 32);

export function defaultPart(eng, old) {
  const p = [];
  for (let i = 0; i < L.P_COUNT; i++) p.push(i < P_E0 ? L.TP[i].def : L.ENG[eng].edit[i - P_E0].def);
  if (old) for (let i = 0; i < P_E0; i++) if (keepsOnLoad(i)) p[i] = old.p[i];
  return { eng, preset: 0, p, x: xDefaults() };   // (x: the extension slots, all at their defaults after a sound load: params.c ext_defaults)
}
// the range the unit clamps a TRACK_PARAM / SET to: the stored one (editor.c ed_tdesc: CUTOFF 2 and RES 2 keep the range of all their meanings, the page card's is narrower)
const storedDesc = (q, id) => (id === ID.P_CUT2 || id === ID.P_RES2 ? L.TP[id] : descOf(q, id));

// DUELL 0.2.17: slot i holds its saved record or else the factory sound at browse position i. The browse order is the firmware's (layout.json BROWSE, from
// sounds.c DS_LIST: category, then engine in the order of L.ORDER, then name; the numbers of docs/PATCH_NUMBERS.md are position + 1)
export function factoryBrowse() {
  return L.BROWSE.map(([eng, idx]) => ({ eng, idx, name: L.ENG[eng].sounds[idx][0], cat: L.ENG[eng].sounds[idx][1] }));
}
const BROWSE = factoryBrowse();

const twRound = (x) => (x < 0 ? -Math.floor(-x + 0.5) : Math.floor(x + 0.5));   // C's (x * m + 2048) / 4096 for the sign-symmetric rounding
export class MockUnit {
  constructor({ caps = 63, playing = false, version = "FELUCCA DUELL 0.2.25 bet", seed = true, savedFlags = true, patchRaw = true, scope2 = true, pat32 = false } = {}) {
    this.pat32 = pat32; this.scope2 = scope2; this.savedFlags = savedFlags; this.patchRaw = patchRaw; this.prStage = null; this.version = version; this.caps = caps; this.playing = playing;
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
    this.objs.set(0, filler(3840, 1, [0x42, 0x4E, 0x55, 0x46])); { const o = this.objs.get(0); o[63] = 2; o[66] = 127; o[67] = 7; }   // (the music: 2 parts, 127 values, revision 7)
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
    if (caps & 4) for (let o = 0; o < PATS.NOBJ; o++) this.objs.set(21 + o, buildPatternObject(o, seed && !pat32 && o === 0 ? { 2: { name: "PAT THREE", len: 12, changed: 1 } } : {}));   // (the unit never lists an empty pattern object: 32 slots, each a pattern)
  }
  // the objects the unit would list for its multis and wavetables (the patches are records: slot -> this.patches, LIST page / GET 40 / PUT 40)
  rebuild() {
    if (!this.records) {                                // an older firmware: the 16 chunks of 8 slots hold whole records; the two chunks of the oldest keep 20 byte stubs
      const real = (this.caps & 16) !== 0;
      const rec = (lo, hi) => [...this.patches].filter(([s]) => s >= lo && s < hi).sort((a, b) => a[0] - b[0]).map(([slot, v]) => (real ? { bytes: this.withPos(slot, v) } : { slot, name: v.name, engine: v.eng, cat: v.cat }));
      const chunks = real ? OLD_PATCH_IDS.map((id, c) => [id, rec(8 * c, 8 * c + 8)]) : [[6, rec(0, 64)], [7, rec(64, 128)]];
      for (const [id, r] of chunks) if (r.length) this.objs.set(id, buildLegacyPatchChunk(r)); else this.objs.delete(id);
    }
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
      v.rec = buildRecord({ engine: v.eng, name: v.name, cat: v.cat, values: vals, ext: v.x ?? null, patch128: v.eng === FM6_E && v.fm6 !== null ? (v.fm6 || INIT128) : null });
    }
    return v.rec;
  }
  withPos(slot, v) { const b = this.recordOf(v).slice(); b[2] = slot; b[3] = v.rev ?? 0; return b; }
  setRecord(slot, bytes) {                              // a record written whole (PATCH_RAW, a restored chunk): the entry follows it
    const r = parseRecord(bytes), old = this.patches.get(slot);
    this.patches.set(slot, { name: r.name, eng: r.engine, cat: r.cat, p: r.values.slice(), x: r.xv.slice(), preset: 0, fm6: r.fm6 ? Uint8Array.from(r.fm6) : null, rec: Uint8Array.from(bytes).fill(0, 2, 4), rev: old ? ((old.rev ?? 0) + 1) & 255 : 0 });
  }
  // a BACKUP_PUT committed object id: take the multis / wavetables from the objects
  fromObjects(id) {
    if (id >= 24 && id <= 26) { const b = this.objs.get(id); this.tables[id - 24] = b && b.length ? parseObject(b) : null; }
    if (id >= 13 && id <= 20) {
      this.multiStored = Array(16).fill(null);
      for (const m of decodeMultis([...Array(8).keys()].map((o) => this.objs.get(13 + o) || null))) this.multiStored[+m.label.slice(1) - 1] = m.name;
    }
  }
  info() {
    const s = (t) => [...t].map((c) => c.charCodeAt(0)).concat(0);
    return [...s(this.version), MOCK_ENGINES.length, L.P_COUNT, L.G_COUNT, L.NSTEP, L.P_E0, ...MOCK_ENGINES.flatMap(s), 4, 0, 0x55, 1, 9, 0x4d, 1, 64, 1, 0x42, 1, this.caps,
      0x46, 1, 8, 0, 0x53, 1, 3, 0x50, 1, 3, 0x4e, 1, this.menu.length, 0x43, 1, 16, ...(this.caps & 16 ? [0x58, 1, this.capacity & 127, this.capacity >> 7] : []), ...(this.patchRaw ? [0x5a, 1, 1] : []), ...(this.scope2 ? [0x59, 1, NX] : [])];
  }
  get capacity() { return this.caps & 16 ? 128 : 48; }                // the patches the unit holds (INFO tag 58 01 with caps bit 4)
  get records() { return (this.caps & CAPS_PATCH_RECORDS) !== 0; }    // caps bit 5: the patches go one at a time as records (editor_backup.c); without it the unit is an older firmware: no pages, GET / PUT of 40 answer rc 1
  has(id) { return [0, 1, 2, 3, 4, 5, 8, 32, 33, 34].includes(id) || (OLD_PATCH_IDS.includes(id) ? !this.records && (id < 27 || (this.caps & 16) !== 0) : id >= 24 ? (this.caps & 8) !== 0 && id <= 26 : id >= 13 && id <= 23 && (this.caps & 4) !== 0); }
  patchBytes(slot) { const v = slot < 128 ? this.patches.get(slot) : null; return v ? this.recordOf(v).slice().fill(0, 2, 4) : null; }   // the saved record as the store gives it out (up_raw_get): slot and revision 0
  push(cmd, args) { if (this.watching && this.onpush) this.onpush(cmd, args); }
  // ---- what the unit's panel does (tests and the page's "turn a knob on the unit" button)
  deviceTurn(part, id, v) {
    this.parts[part].p[id] = v;
    if (part === this.selected) this.push(CMD.CHANGED, [0, id, ...v14(v)]);
    else if (id === 0 || id === 39 || id === 40) this.push(CMD.TRACK_CHANGED, [part, id, ...v14(v)]);
  }
  // a multi load (0.2.25): both parts change (the stored snapshot, else two factory sounds of the slot); the unit pushes RELOAD for its SELECTED part only (editor.c ed_poll)
  loadMultiParts(slot) {
    const snap = this.multiSnap?.[slot];
    this.parts = snap ? snap.map((q) => ({ ...q, p: q.p.slice(), x: (q.x ?? xDefaults()).slice() })) : [0, 1].map((k) => { const f = BROWSE[(slot * 2 + k) % BROWSE.length], q = defaultPart(f.eng); q.preset = f.idx; q.p[ID.P_TRIM] = 90 + slot + k; return q; });
  }
  deviceLoadMulti(slot) { this.loadMultiParts(slot); this.push(CMD.RELOAD, [this.parts[this.selected].eng, this.parts[this.selected].preset, this.selected]); }
  deviceSelect(part) { this.selected = part; this.push(CMD.RELOAD, [this.parts[part].eng, this.parts[part].preset, part]); }
  devicePalette(i) { this.menu.find((m) => m[0] === 0)[6] = i; }   // MENU > COLOR on the panel (the page finds it by polling MENU_DESC)
  deviceMenu(id, v) { this.menu.find((m) => m[0] === id)[6] = v; }   // a MENU / device setting turned on the panel (WIDTH, BASS MONO, COMP ...): the unit pushes nothing, the editor finds it by polling MENU_DESC
  deviceGlobalQuiet(id, v) { this.g[id] = v; }                          // a global changed on the panel whose CHANGED push was lost (the editor's global dump poll finds it)
  deviceGlobal(id, v) { this.g[id] = v; this.push(CMD.CHANGED, [1, id, ...v14(v)]); }
  async request([cmd, a], opt = {}) {
    this.log.push(`cmd ${cmd}`);
    if (cmd >= WT.CMD.LIST && cmd <= WT.CMD.PUT && (this.caps & 8)) return this.wavetable(cmd, a);
    if (cmd <= 100 && cmd !== 65 && cmd !== 66 && cmd !== 67) { const r = this.edit(cmd, a); if (r !== undefined) return r; }
    if (cmd === 1) return this.info();
    if (cmd === BACKUP_CMD.LIST && a.length === 1) {   // a page of the saved patches (caps bit 5); the plain LIST before, else rc 5
      const pg = a[0], rc = !this.records || pg < 1 || pg > PATCH_PAGES ? 1 : !this.listed ? 5 : this.playing ? 3 : 0;
      const slots = rc ? [] : [...this.patches.keys()].filter((k) => k >= PATCH_PAGE * (pg - 1) && k < PATCH_PAGE * pg).sort((x, y) => x - y);
      return [1, rc, slots.length, ...slots.flatMap((k) => { const b = this.patchBytes(k); return [k, ...bkU32(b.length), ...bkU32(bkCrc(b))]; })];
    }
    if (cmd === BACKUP_CMD.LIST) {
      if (a.length) return [1, 1, 0];
      if (this.playing) return [1, 3, 0];
      this.listed = true;
      const ids = this.records ? [0, 1, 2, 3, 4, 5, 8, ...(this.caps & 4 ? [13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23] : []), ...(this.caps & 8 ? [24, 25, 26] : []), 32, 33, 34]   // (24 in all with caps 63)
        : [...Array(13).keys(), ...(this.caps & 4 ? [13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23] : []), ...(this.caps & 8 ? [24, 25, 26] : []), ...(this.caps & 16 ? [27, 28, 29, 30, 31] : []), 32, 33, 34, ...(this.caps & 16 ? [35, 36, 37, 38, 39] : [])];   // an older firmware lists its patch chunks
      return [1, 0, ids.length, ...ids.flatMap((id) => { const v = this.objs.get(id); return [id, ...bkU32(v ? v.length : 0), ...bkU32(v && v.length ? bkCrc(v) : 0)]; })];
    }
    if (cmd === BACKUP_CMD.GET) {
      const patch = a[0] === PATCH_ID, id = a[0], off = bkR32(a, 1), n = a[6] | a[7] << 7;
      const v = patch ? (a.length === 9 && this.records ? this.patchBytes(a[8]) : null) ?? new Uint8Array(0) : this.objs.get(id) || new Uint8Array(0);
      const rc = !this.listed ? 5 : this.playing ? 3 : a.length !== (patch ? 9 : 8) || (patch ? !this.records : !this.has(id)) || !n || n > 256 || off > v.length || n > v.length - off ? 1 : 0;
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
      case CMD.GET:
        if (a[0] > 2) return undefined;
        if (a[0] === 2) return this.scope2 && a[1] < NX ? [2, a[1], ...v14(part().x[a[1]])] : undefined;
        return a[0] ? [1, a[1], ...v14(this.g[a[1]])] : [0, a[1], ...v14(part().p[a[1]])];
      case CMD.SET: return a[0] > 2 ? undefined : this.setScoped(a[0], a[1], rv14(a, 2));
      case CMD.TRACK_PARAM: {
        const t = a[0], id = a[1], sc = a.length & 1 ? a[a.length - 1] : 0;   // a scope byte last (3 or 5 bytes): 0 or 2
        if (t > 3 || (sc !== 0 && sc !== 2) || id >= (sc === 2 ? NX : P) || a.length > 5 || (sc === 2 && !this.scope2)) return undefined;
        const q = this.parts[Math.min(t, 1)];
        if (sc === 2) {
          if (a.length === 5) this.putX(q, id, rv14(a, 2), t === this.selected);
          return [t, id, ...v14(q.x[id]), 2];
        }
        if (a.length >= 4) this.putP(q, id, rv14(a, 2), t === this.selected);
        return [t, id, ...v14(q.p[id]), ...(a.length & 1 ? [sc] : [])];
      }
      case CMD.DESC: {
        if (a[0] > 2 || (a[0] === 2 && !(this.scope2 && a[1] < NX))) return undefined;
        const d = a[0] === 2 ? xdescOf(part(), a[1]) : a[0] ? L.GP[a[1]] : descOf(part(), a[1]);
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
        const q = { ...defaultPart(v.eng, cur), p: v.p.map((x, i) => (kept(i) ? cur.p[i] : x)), x: v.x ? v.x.slice() : xDefaults() };
        q.eng = v.eng;
        if (v.eng === FM6_E) q.fm6 = v.fm6 ? Uint8Array.from(v.fm6) : INIT128.slice();
        this.parts[this.selected] = q; return [a[0], 0];
      }
      case CMD.UP_STORE: {                             // over a slot: it holds the part's sound now (no 'full': every slot can be written)
        if (this.playing || a[0] > 127) return [a[0], 2];
        const [nm] = rstr(a, 1), q = part();
        if (nm.length > 12) return [a[0], 1];
        this.patches.set(a[0], { name: nm || `${L.ENG[q.eng].name} ${String(a[0] + 1).padStart(2, "0")}`, eng: q.eng, cat: 8, eng2: q.eng, p: q.p.map((v, i) => (i === ID.P_LEVEL ? L.TP[i].def : v)), x: q.x.slice(), preset: q.preset, fm6: q.eng === FM6_E ? (q.fm6 ? Uint8Array.from(q.fm6) : INIT128.slice()) : null, rev: (this.patches.get(a[0])?.rev ?? -1) + 1 & 255 });
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
        const used = this.multiStored[a[1]] != null || a[1] < 16 || this.multiUsedExtra.has(a[1]);   // (DUELL 0.2.23: all sixteen slots hold a factory multi)
        if (a[0] === 2) return [2, a[1], used ? 1 : 0];
        if (a[0] === 1) {
          if (this.playing) throw new Error("mock unit: no reply (playing)");
          if (this.multiStored[a[1]] == null) this.multiStored[a[1]] = `MULTI ${a[1] + 1}`;
          this.multiSaved = { part0: this.parts[0], part1: this.parts[1] };
          (this.multiSnap ||= [])[a[1]] = this.parts.map((q) => ({ ...q, p: q.p.slice(), x: q.x.slice(), fm6: q.fm6 ? q.fm6.slice() : q.fm6 }));
          this.rebuild(); return [1, a[1], 1];
        }
        if (a[0] === 0) { this.loadMultiParts(a[1]); return [0, a[1], used ? 1 : 0]; }   // (the editor's own load: it reads everything after it)
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
      if (a.length !== 9 || a[8] > 15 || len < REC_FIX || len > REC_WORST) return ret(1);
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
    if (recordProblem(st.bytes)) return ret(1);       // (pt_rec_ok and up_put_raw: the structure, the extension block, a printable 0-padded name, the values)
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
      if (id === ID.G_ENGSEL) {                        // 0.2.24: as the unit (ui.c set_engine_of): the common values stay, the engine's own eight are the new engine's defaults, or carry over where the label and format match
        const e0 = clampTo(L.GP[id], v), e = L.ENG[e0].name === "-" ? 0 : e0, cur = this.parts[this.selected];
        if (e !== cur.eng) {
          const was = cur.p.slice(P_E0), q = { eng: e, preset: 0, p: cur.p.slice(0, P_E0), x: cur.x, name: cur.name };
          for (let k = 0; k < 8; k++) {
            const d = L.ENG[e].edit[k]; let val = d.def;
            const j = L.ENG[cur.eng].edit.findIndex((o) => o.label && o.label[0] !== "-" && o.label === d.label && o.fmt === d.fmt && !!o.names === !!d.names && (!d.names || (o.min === d.min && o.max === d.max && o.names.join() === d.names.join())));
            if (j >= 0) val = clampTo(d, was[j]);
            q.p.push(val);
          }
          this.parts[this.selected] = q;
        }
        this.g[id] = this.parts[this.selected].eng; return [1, id, ...v14(this.parts[this.selected].eng)];
      }
      this.g[id] = clampTo(L.GP[id], v); return [1, id, ...v14(this.g[id])];
    }
    if (scope === 2) {                                 // an extension slot of the selected part
      if (!this.scope2 || id >= NX) return undefined;
      const q = this.parts[this.selected]; this.putX(q, id, v, false); return [2, id, ...v14(q.x[id])];
    }
    const q = this.parts[this.selected]; this.putP(q, id, v, false); return [0, id, ...v14(q.p[id])];
  }
  // a value written by the editor (TRACK_PARAM, SET), as the unit does it: clamped to the stored range; TWIN TYPE through twin_type_set, LINK through twin_link_set (chain_f2.c). push: the unit
  // pushes CHANGED for what the setter moved besides (CUTOFF 2, RES 2) when the part is the selected one
  putP(q, id, v, push) {
    if (id === ID.P_TWTYPE) { this.twinType(q, clampTo(L.TP[id], v), push); return; }
    const d = storedDesc(q, id); if (d.max > d.min) q.p[id] = clampTo(d, v);
  }
  putX(q, k, v, push) {
    const d = xdescOf(q, k); if (!(d.max > d.min)) return;
    v = clampTo(d, v);
    if (k === X.X_LINK) this.twinLink(q, v, push); else q.x[k] = v;
  }
  twinLink(q, on, push) {                              // twin_link_set: CUTOFF 2 keeps the sound (filter 1's knob converts OFFSET <-> CUTOFF), not on COMB
    const knob = Math.min(Math.max(q.p[ID.P_FREEA], 0), 82), c = q.p[ID.P_CUT2];
    on = on ? 1 : 0;
    if (on !== (q.x[X.X_LINK] ? 1 : 0) && q.p[ID.P_TWTYPE] !== L.TW.COMB) q.p[ID.P_CUT2] = on ? Math.min(36, Math.max(-36, twRound((c - knob) * 5423 / 4096))) : Math.min(82, Math.max(0, knob + twRound(Math.min(36, Math.max(-36, c)) * 3093 / 4096)));   // (1.0: OFFSET is whole semitones, a dial step 1.324 of them; chain_f2.c TW_STEPS_OFF / TW_OFF_STEPS)
    q.x[X.X_LINK] = on;
    if (push && this.parts[this.selected] === q) this.push(CMD.CHANGED, [0, ID.P_CUT2, ...v14(q.p[ID.P_CUT2])]);
  }
  twinType(q, type, push) {                            // twin_type_set: COMB is refused on WAVE and PHYS; entering or leaving COMB resets RES 2 and CUTOFF 2
    if (type === L.TW.COMB && ["WAVE", "PHYS"].includes(L.ENG[q.eng].name)) return;
    if ((q.p[ID.P_TWTYPE] === L.TW.COMB) !== (type === L.TW.COMB)) {
      q.p[ID.P_RES2] = 0; q.p[ID.P_CUT2] = type === L.TW.COMB || q.x[X.X_LINK] ? 0 : 82;
      if (push && this.parts[this.selected] === q) { this.push(CMD.CHANGED, [0, ID.P_RES2, ...v14(0)]); this.push(CMD.CHANGED, [0, ID.P_CUT2, ...v14(q.p[ID.P_CUT2])]); }
    }
    q.p[ID.P_TWTYPE] = type;
  }
  deviceTurnX(part, k, v) {                            // the panel changes an extension slot: the unit pushes CHANGED scope 2 for the selected part
    this.parts[part].x[k] = v;
    if (part === this.selected) this.push(CMD.CHANGED, [2, k, ...v14(v)]);
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
      if (id === PATCH_ID) {                            // one patch record into a slot, 13 bytes: id, size u32, crc u32, slot (editor_backup.c); size 0 removes the slot's record
        const size = bkR32(a, 2), crc = bkR32(a, 7), slot = a[12];
        if (!this.records || a.length !== 13 || a[6] > 15 || a[11] > 15 || slot >= 128 || (size && (size < REC_FIX || size > PATCH_MAX))) return ret(1);
        if (this.playing) return ret(3);
        this.staged = { id, slot, size, crc, bytes: new Uint8Array(size), pos: 0 };
        return ret(0);
      }
      if (!RETIRED_IDS.includes(id) && ((!DUELL_IDS.includes(id) && !OLD_PATCH_IDS.includes(id)) || !this.has(id))) return ret(1);   // (the retired ids 2..5 and 8 are accepted and dropped, as editor_backup.c does for an older archive)
      const size = bkR32(a, 2), crc = bkR32(a, 7);
      const bad = size > 3840 || (id === 0 && size !== 3840) || (id === 1 && size !== this.settingsSize) ||
        (id >= 13 && id <= 20 && size && size < 8) || (id >= 21 && id <= 23 && size && (size < 8 || size > 3584)) || (id >= 24 && id <= 26 && size && size !== WT.OBJ);
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
      if (id === PATCH_ID) {                            // the store's own check (pt_rec_ok, up_put_raw): the slot and revision bytes of the record are not looked at
        if (st.size && recordProblem(st.bytes)) return ret(2);
        if (st.size) { this.setRecord(st.slot, st.bytes); this.log.push(`write patch ${st.slot}`); } else { this.patches.delete(st.slot); this.log.push(`remove patch ${st.slot}`); }
        this.listed = false;
        return ret(0);
      }
      if (OLD_PATCH_IDS.includes(id) ? !legacyChunkOk(st.bytes) : checkObject(id, st.bytes)) return ret(2);      // (the content check)
      this.log.push(`write ${id}`);
      if (st.size) this.objs.set(id, st.bytes); else if (id >= 21 && id <= 23) this.objs.set(id, buildPatternObject(id - 21)); else this.objs.delete(id);   // (length 0 on a pattern object: its slots get their defaults, as the unit does)
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
