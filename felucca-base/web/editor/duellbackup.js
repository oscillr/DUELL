// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor, Backup page: the "duell-backup" file, its checks and decoders, and the capture / restore / verify steps.
// No DOM here (editor/index.html applies it; test_duellbackup.mjs runs it against a simulated unit). The transport is the
// editor protocol's BACKUP_LIST / BACKUP_GET / BACKUP_PUT (EDITOR_PROTOCOL.md "v7: full backup"); requests name objects, never addresses.
//
// The objects (firmware editor_backup.c; ids are the unit's, kept as they are):
//   0         the music of both parts (FUN11, 3840 B; restoring it replaces what plays, RAM only)
//   1         the settings record (palette, LEDs, HOLD, button map, favourites, MENU, MIDI learn)
//   6 7 9..12 the first six chunks of the patch store; 27..31 35..39 the other ten (DUELL 0.2.15, INFO 42 01 caps bit 4: 16 chunks, 128 patches; ids 32..34 lie between them)
//   13..20    the multis, two to an object, M01-02 .. M15-16 (magic MUL1; DUELL 0.2.14, INFO 42 01 caps bit 2)
//   21..23    the pattern slots, six to an object, P1-6 P7-12 P13-16 (magic PTS2; DUELL 0.2.14, caps bit 2)
//   24..26    the user wavetables USR1..USR3 (1228 bytes: version 1, 19, 64, 0, the 8 character name, 19 x 64 harmonic bytes; length 0 = EMPTY;
//             wavetable.js; DUELL 0.2.14, caps bit 3). A file with them is "version 3"; an editor that reads up to version 2 says so and stops
// A file with the ten more chunks is "version 4" (always all 16 patch ids); a version 3 file has the first six and restores onto a unit with 16 chunks by emptying the other ten.
// Ids 2..5, 8 and 32..34 are retired (always empty: Felucca's project slots and FM6 bank, the user sample slots) and are not in the file.
import { BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc, readBackup } from "../fm1backup.js";
import { checkObject as wtCheck, parseObject as wtParse } from "./wavetable.js";

export const FORMAT = "duell-backup";
export const VERSION = 4;                      // 4 adds the ten more patch chunks (ids 27..31, 35..39); 3 adds the user wavetables (ids 24..26); 2 is read as it was; 1 is the installer's "felucca-backup" file of a DUELL unit (read here, never written)
export const CAPS_MULTI_PATTERN = 4;           // INFO 42 01 caps, bit 2
export const CAPS_WAVETABLES = 8;              // bit 3: the user wavetables (ids 24..26, cmds 81..83)
export const CAPS_PATCH16 = 16;                // bit 4: the patch store has 16 chunks (ids 27..31, 35..39), room for 128 patches
export const PATCH_IDS_6 = [6, 7, 9, 10, 11, 12];
export const PATCH_IDS_X = [27, 28, 29, 30, 31, 35, 36, 37, 38, 39];

export const GROUPS = [
  { key: "patches", label: "Patches", ids: [...PATCH_IDS_6, ...PATCH_IDS_X], caps: 0 },   // (caps 0: a file with the first six restores onto any unit; planRestore checks the ten more)
  { key: "multis", label: "Multis", ids: [13, 14, 15, 16, 17, 18, 19, 20], caps: CAPS_MULTI_PATTERN },
  { key: "patterns", label: "Pattern slots", ids: [21, 22, 23], caps: CAPS_MULTI_PATTERN },
  { key: "wavetables", label: "User wavetables", ids: [24, 25, 26], caps: CAPS_WAVETABLES },
  { key: "settings", label: "Settings", ids: [1], caps: 0 },
  { key: "music", label: "Current music (both parts)", ids: [0], caps: 0 },
];
export const DUELL_IDS = [0, 1, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, ...PATCH_IDS_X];
export const RESTORE_ORDER = [6, 7, 9, 10, 11, 12, ...PATCH_IDS_X, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 1, 0];   // live music last
const KNOWN_IDS = [...Array(40).keys()];                                                      // what a unit lists
const MAX_OBJ = 3840;
const MUL_HDR = 8, MUL_REC_MAX = (MAX_OBJ - MUL_HDR) / 2, MUL_VER = 2, MUL_NAME = 8, MUL_G_AT = 20, P_COUNT = 127;
const PAT_REC = 596, PAT_NREC = 6, PAT_SIZE = 8 + PAT_NREC * PAT_REC, PAT_USED = 0xA7, PAT_VER = 2, PAT_NAME = 12, PAT_SLOTS = 16;
const CATEGORIES = ["-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "PERC", "FX", "OTHER"];

const u16 = (b, o) => b[o] | b[o + 1] << 8;
const u32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
const printable = (b, from, n) => { for (let i = 0; i < n; i++) if (b[from + i] && (b[from + i] < 32 || b[from + i] > 126)) return false; return true; };
const text = (b, from, n) => { let s = ""; for (let i = 0; i < n && b[from + i]; i++) s += String.fromCharCode(b[from + i]); return s; };
export const groupOf = (id) => GROUPS.find((g) => g.ids.includes(id)) || null;

// ---- object checks: the shapes the unit insists on at commit (the unit checks again; a file that fails here is never sent)
function patSlotOk(b, off, index) {                    // a pattern slot record; index: the slot number P1.. - 1
  if (b[off] === 0) return true;                       // unused
  if (b[off] !== PAT_USED || b[off + 1] !== PAT_VER || index >= PAT_SLOTS) return false;
  const len = b[off + 2], div = b[off + 3], swing = b[off + 4], gate = b[off + 5], qnt = b[off + 6];
  if (len < 1 || len > 64 || div > 16 || swing > 100 || gate < 1 || gate > 127 || qnt > 1 || !printable(b, off + 8, PAT_NAME)) return false;
  for (let s = 0; s < 64; s++) {
    const t = off + 8 + PAT_NAME + s * 9, n = b[t + 4] & 7, time = b[t + 4] >> 3 & 3, prob = b[t + 8] & 127;
    if (b[t + 4] & 128 || n > 4 || time > 2 || prob > 101 || b[t + 7] & ~b[t + 6] & 255) return false;
  }
  return true;
}
// null when the object is acceptable for id, else a short reason
export function checkObject(id, b) {
  const n = b.length;
  if (n > MAX_OBJ) return "too large";
  if (id === 0) return n === MAX_OBJ && u32(b, 0) === 0x46554E42 ? null : "not the current music of a DUELL unit";
  if (id === 1) return n >= 64 && u32(b, 0) === 0x50455234 ? null : "not a settings record";
  if (groupOf(id)?.key === "patches") {
    if (!n) return null;
    if (n < 4 || u16(b, 0) !== 0x5450 || b[2] !== 1) return "not a patch store chunk";
    for (let off = 4; off < n;) {
      const rl = u16(b, off);
      if (rl < 20 || off + rl > n || b[off + 2] > 127 || !printable(b, off + 7, 12)) return "damaged patch record";
      off += rl;
    }
    return null;
  }
  if (groupOf(id)?.key === "multis") {
    if (!n) return null;
    const l = [u16(b, 4), u16(b, 6)];
    if (n < MUL_HDR || u32(b, 0) !== 0x314C554D || l[0] > MUL_REC_MAX || l[1] > MUL_REC_MAX || MUL_HDR + l[0] + l[1] !== n) return "not a multi object";
    for (let h = 0, at = MUL_HDR; h < 2; at += l[h++]) {
      if (!l[h]) continue;
      if (l[h] < MUL_G_AT || !b[at] || b[at] > MUL_VER || b[at + 1] < 8 || b[at + 1] > P_COUNT || !printable(b, at + MUL_NAME, 12)) return "damaged multi";
    }
    return null;
  }
  if (groupOf(id)?.key === "patterns") {
    if (!n) return null;
    if (n !== PAT_SIZE || u32(b, 0) !== 0x32535450 || u16(b, 4) !== PAT_REC || u16(b, 6) !== PAT_NREC) return "not a pattern slot object";
    for (let i = 0; i < PAT_NREC; i++) if (!patSlotOk(b, 8 + i * PAT_REC, (id - 21) * PAT_NREC + i)) return "damaged pattern slot";
    return null;
  }
  if (groupOf(id)?.key === "wavetables") {
    if (!n) return null;
    const why = wtCheck(b);
    return why ? `not a user wavetable (${why})` : null;
  }
  return "not an object of a DUELL backup";
}

// ---- decoders for the preview (what is in an object), pure
// patch store chunks -> [{slot, name, engine, category}] by slot (a record that moved between chunks exists twice: the newer revision wins)
export function decodePatches(chunks) {
  const best = new Map();
  for (const b of chunks) {
    if (!b || b.length < 4) continue;
    for (let off = 4; off + 20 <= b.length;) {
      const rl = u16(b, off);
      if (rl < 20) break;
      const slot = b[off + 2], rev = b[off + 3], old = best.get(slot);
      if (!old || ((rev - old.rev) & 255) < 128 && rev !== old.rev)
        best.set(slot, { slot, rev, name: text(b, off + 7, 12), engine: b[off + 5], category: CATEGORIES[b[off + 6]] || "-" });
      off += rl;
    }
  }
  return [...best.values()].sort((a, c) => a.slot - c.slot);
}
// multi objects (index = id - 13) -> [{label: "M01", name}] for the records that are stored (an empty record shows the factory multi, or nothing)
export function decodeMultis(objects) {
  const out = [];
  objects.forEach((b, o) => {
    if (!b || b.length < MUL_HDR) return;
    const l = [u16(b, 4), u16(b, 6)];
    for (let h = 0, at = MUL_HDR; h < 2; at += l[h++])
      if (l[h]) out.push({ label: "M" + String(2 * o + h + 1).padStart(2, "0"), name: text(b, at + MUL_NAME, 12) });
  });
  return out;
}
// pattern objects (index = id - 21) -> [{label: "P01", name, length}]
export function decodePatterns(objects) {
  const out = [];
  objects.forEach((b, o) => {
    if (!b || b.length !== PAT_SIZE) return;
    for (let i = 0; i < PAT_NREC; i++) {
      const off = 8 + i * PAT_REC, k = o * PAT_NREC + i;
      if (b[off] === PAT_USED && k < PAT_SLOTS) out.push({ label: "P" + String(k + 1).padStart(2, "0"), name: text(b, off + 8, PAT_NAME), length: b[off + 2] });
    }
  });
  return out;
}
// the user wavetables (index = id - 24) -> [{slot: 1.., label: "USR1", name}] for the slots that hold a table
export function decodeWavetables(objects) {
  const out = [];
  objects.forEach((b, o) => { if (b && b.length) out.push({ slot: o, label: "USR" + (o + 1), name: wtParse(b).name }); });
  return out;
}
// the whole content of an archive (objects: Map id -> bytes) for the page
export function describe(objects, engines = []) {
  const get = (ids) => ids.map((id) => objects.get(id) || null);
  return {
    patches: decodePatches(get(GROUPS[0].ids)).map((p) => ({ ...p, engineName: engines[p.engine] || `ENGINE ${p.engine}` })),
    multis: decodeMultis(get(GROUPS[1].ids)),
    patterns: decodePatterns(get(GROUPS[2].ids)),
    wavetables: decodeWavetables(get(GROUPS[3].ids)),
    has: Object.fromEntries(GROUPS.map((g) => [g.key, g.ids.some((id) => objects.has(id) && objects.get(id).length > 0)])),
  };
}

// ---- INFO (cmd 1): version, engine names, and the capability tags. caps: the `42 01 caps` tag (null: a unit without it)
const TAG_LEN = { 0x55: 3, 0x4d: 4, 0x42: 3, 0x46: 4, 0x53: 3, 0x50: 3, 0x4e: 3, 0x52: 3, 0x4c: 3, 0x41: 4, 0x54: 3, 0x56: 3, 0x43: 3, 0x58: 4, 0x5a: 3 };
export function parseInfo(a) {
  let pos = a.indexOf(0);
  if (pos < 1) throw new Error("The unit sent an unreadable INFO");
  const version = String.fromCharCode(...a.slice(0, pos)); pos++;
  const nEng = a[pos]; pos += 5;                       // engines, P_COUNT, G_COUNT, NSTEP, P_E0
  const engines = [];
  for (let i = 0; i < nEng; i++) {
    const end = a.indexOf(0, pos);
    if (end < 0) throw new Error("The unit sent an unreadable INFO");
    engines.push(String.fromCharCode(...a.slice(pos, end))); pos = end + 1;
  }
  pos += 2;                                            // NTRK, song rows
  let caps = null, patchCap = null;
  while (pos < a.length && TAG_LEN[a[pos]]) {
    if (a[pos] === 0x42 && a[pos + 1] === 1) caps = a[pos + 2];
    if (a[pos] === 0x58 && a[pos + 1] === 1) patchCap = a[pos + 2] | a[pos + 3] << 7;   // the patch capacity (DUELL 0.2.15)
    pos += TAG_LEN[a[pos]];
  }
  return { version, engines, caps, patchCap };
}

// ---- the file
const base64 = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode(b); return btoa(s); };
const B64 = /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/;
export function objectsToFile(objects, info) {         // objects: [{id, bytes}]
  return {
    format: FORMAT, version: objects.some((o) => PATCH_IDS_X.includes(o.id)) ? VERSION : 3, firmware: info.version, created: new Date().toISOString(), engines: info.engines,
    objects: objects.map(({ id, bytes }) => ({ id, size: bytes.length, crc: bytes.length ? bkCrc(bytes) : 0, data: base64(bytes) })),
  };
}
// Parse and check a whole file before anything is sent. Returns { version, source, firmware, created, engines, objects: Map id -> {id, size, crc, bytes} }.
export function readArchive(file) {
  if (typeof file === "string") { try { file = JSON.parse(file); } catch { throw new Error("This is not a backup file (it is not valid JSON)."); } }
  if (!file || typeof file !== "object" || !Array.isArray(file.objects)) throw new Error("This is not a DUELL backup file.");
  let source, items, version;
  if (file.format === FORMAT) {
    if (!Number.isInteger(file.version) || file.version < 2) throw new Error("This backup has an unknown version.");
    if (file.version > VERSION) throw new Error(`This backup is version ${file.version}; this editor reads up to version ${VERSION}. Use a newer editor.`);
    version = file.version; source = FORMAT; items = file.objects;
    if (version < 4 && items.some((o) => o && PATCH_IDS_X.includes(o.id))) throw new Error("This backup holds patch chunks a version 3 file does not have: it is damaged.");
  } else if (file.format === "felucca-backup" && file.version === 1) {
    if (!/DUELL/i.test(String(file.firmware || ""))) throw new Error("This is a Felucca backup, not a DUELL one: its objects mean other things. It was not read.");
    items = readBackup(file).objects;                  // the installer's file: its own checks (ids, sizes, CRC); the retired objects are dropped below
    version = 1; source = "felucca-backup v1";
  } else throw new Error("This is not a DUELL backup file.");
  const objects = new Map();
  for (const o of items) {
    if (!o || !Number.isInteger(o.id)) throw new Error("A backup object has no id.");
    if (!KNOWN_IDS.includes(o.id)) throw new Error(`Unknown object id ${o.id} in this backup.`);
    if (!DUELL_IDS.includes(o.id)) {                   // a retired object: must be empty, then it is left out
      if (o.size) throw new Error(`Object ${o.id} is retired in DUELL and holds data: this is not a DUELL backup.`);
      continue;
    }
    if (objects.has(o.id)) throw new Error(`Object ${o.id} is in the file twice.`);
    let bytes = o.bytes;
    if (!bytes) {
      if (!Number.isInteger(o.size) || o.size < 0 || o.size > MAX_OBJ || !Number.isInteger(o.crc) || o.crc < 0 || o.crc > 0xffffffff ||
          typeof o.data !== "string" || o.data.length !== 4 * Math.ceil(o.size / 3) || !B64.test(o.data)) throw new Error(`Object ${o.id} is damaged.`);
      bytes = Uint8Array.from(atob(o.data), (c) => c.charCodeAt(0));
      if (bytes.length !== o.size || (o.size ? bkCrc(bytes) : 0) !== o.crc) throw new Error(`Object ${o.id} failed its checksum: the file is damaged.`);
    }
    const why = checkObject(o.id, bytes);
    if (why) throw new Error(`Object ${o.id}: ${why}.`);
    objects.set(o.id, { id: o.id, size: bytes.length, crc: bytes.length ? bkCrc(bytes) : 0, bytes });
  }
  for (const g of GROUPS) {                            // a group is whole or absent (v1 files lack the multis and patterns, and may lack patch chunks 10..12)
    const n = g.ids.filter((id) => objects.has(id)).length;
    const whole = g.key === "patches" ? (version >= 4 ? g.ids.length : PATCH_IDS_6.length) : g.ids.length;   // (a version 3 file has six patch chunks)
    if (n && n !== whole && !(version === 1 && g.key === "patches")) throw new Error(`The ${g.label.toLowerCase()} in this file are incomplete.`);
  }
  if (!objects.get(0)?.size || !objects.get(1)?.size) throw new Error("This backup has no current music or no settings: it is not complete.");
  return { version, source, firmware: String(file.firmware || ""), created: String(file.created || ""),
           engines: Array.isArray(file.engines) ? file.engines.map(String) : [], objects };
}

// ---- the unit
const NOTES = { 1: "The unit refused the request", 2: "The unit found the data invalid", 3: "The unit is playing. Stop playback on the unit and try again",
                4: "The unit could not write its flash memory", 5: "The backup session on the unit expired. Start again" };
const rcCheck = (rc) => { if (rc) throw Object.assign(new Error(NOTES[rc] || `The unit answered error ${rc}`), { rc }); };
export const CHUNK = 256;
// LIST -> Map id -> {size, crc}; only the ids a DUELL backup holds are kept
export function parseList(a) {
  if (a[0] !== 1) throw new Error("Unexpected answer from the unit");
  rcCheck(a[1]);
  const n = a[2];
  if (a.length !== 3 + n * 11) throw new Error("The unit sent an incomplete list");
  const m = new Map();
  for (let i = 0; i < n; i++) {
    const p = 3 + i * 11, id = a[p];
    if (!KNOWN_IDS.includes(id)) throw new Error(`The unit lists an object (${id}) this editor does not know. Use a newer editor.`);
    const size = bkR32(a, p + 1), crc = bkR32(a, p + 6);
    if (size > MAX_OBJ || (!size && crc)) throw new Error("The unit lists an object that is too large");
    if (DUELL_IDS.includes(id)) m.set(id, { size, crc });
  }
  return m;
}
export async function listUnit(request) { return parseList(await request([BACKUP_CMD.LIST, []], { timeout: 4000, retries: 0 })); }

// Read everything the unit lists (multis and patterns only when its firmware has them). Fails if the unit changes meanwhile.
export async function captureBackup(request, info, onProgress = () => {}) {
  const list = await listUnit(request), ids = DUELL_IDS.filter((id) => list.has(id));
  if (!list.has(0) || !list.has(1)) throw new Error("The unit does not list its music and settings: is it running DUELL?");
  const total = ids.reduce((n, id) => n + list.get(id).size, 0); let done = 0; const got = [];
  for (const id of ids) {
    const { size, crc } = list.get(id), bytes = new Uint8Array(size);
    for (let off = 0; off < size; off += CHUNK) {
      const n = Math.min(CHUNK, size - off);
      const a = await request([BACKUP_CMD.GET, [id, ...bkU32(off), n & 127, n >>> 7]], { timeout: 1500, retries: 1 });
      rcCheck(a[1]);
      if (a[0] !== id || bkR32(a, 2) !== off || (a[7] | a[8] << 7) !== n) throw new Error("Unexpected answer from the unit");
      bytes.set(bkUnpack(a.slice(9), n), off);
      done += n; onProgress(done, total);
    }
    if ((size ? bkCrc(bytes) : 0) !== crc) throw new Error("The unit changed while it was read. Try again while it is stopped.");
    got.push({ id, bytes });
  }
  const file = objectsToFile(got, info);
  readArchive(file);                                   // what was read must pass the same checks a restore applies
  return file;
}

// A file whose patches need the 16-chunk store: one of the ten more chunks holds something
export const patchesNeedCaps = (archive) => PATCH_IDS_X.some((id) => (archive.objects.get(id)?.size ?? 0) > 0);

// What a restore would do. sel: Set of group keys. list: listUnit() now. caps: INFO caps (null unknown). Returns
// { steps: [{id, size, crc, bytes, group}], excluded: [{key, label, reason}] }; nothing is sent.
export function planRestore(archive, list, caps, sel) {
  const steps = [], excluded = [];
  for (const g of GROUPS) {
    if (!sel.has(g.key)) continue;
    let reason = null;
    if (g.caps && ((caps ?? 0) & g.caps) !== g.caps) reason = "this unit's firmware cannot restore them (update it first)";
    else if (g.key !== "patches" && !g.ids.every((id) => archive.objects.has(id))) reason = "not in this file";
    else if (g.key === "patches" && !archive.objects.has(6)) reason = "not in this file";
    else if (g.key === "patches" && patchesNeedCaps(archive) && !((caps ?? 0) & CAPS_PATCH16)) reason = "this file holds more patches than this unit's firmware can store (update it first)";
    else if ((g.key === "settings" || g.key === "music") && list.get(g.ids[0])?.size !== archive.objects.get(g.ids[0]).size) reason = "the file was made by another DUELL version (its size differs from the unit's)";
    if (reason) { excluded.push({ key: g.key, label: g.label, reason }); continue; }
    for (const id of g.key === "patches" && !((caps ?? 0) & CAPS_PATCH16) ? PATCH_IDS_6 : g.ids) {
      const o = archive.objects.get(id) || { id, size: 0, crc: 0, bytes: new Uint8Array(0) };   // a patch chunk an old file lacks is written empty
      steps.push({ id, size: o.size, crc: o.crc, bytes: o.bytes, group: g.key });
    }
  }
  steps.sort((a, b) => RESTORE_ORDER.indexOf(a.id) - RESTORE_ORDER.indexOf(b.id));
  return { steps, excluded };
}
// Send the steps, one object at a time (begin, 256-byte pieces, commit: the unit checks the CRC and the content, then writes; the old copy
// stays valid until the new one is complete). Stops at the first error. Never aborts silently: the result says what was written.
export async function runRestore(request, plan, onProgress = () => {}) {
  const written = [], total = plan.steps.reduce((n, s) => n + s.size, 0); let done = 0;
  const put = async (args) => { const a = await request([BACKUP_CMD.PUT, args], { timeout: 6000, retries: 0 }); rcCheck(a[2]); };
  for (const s of plan.steps) {
    try {
      await put([0, s.id, ...bkU32(s.size), ...bkU32(s.crc)]);
    } catch (e) { return { written, failed: { id: s.id, message: e.message, rc: e.rc } }; }
    try {
      for (let off = 0; off < s.size; off += CHUNK) {
        const piece = s.bytes.subarray(off, off + CHUNK);
        await put([1, s.id, ...bkU32(off), ...bkPack(piece)]);
        done += piece.length; onProgress(done, total);
      }
      await put([2, s.id]);
    } catch (e) {
      await put([3, s.id]).catch(() => {});
      return { written, failed: { id: s.id, message: e.message, rc: e.rc } };
    }
    written.push(s.id);
  }
  return { written, failed: null };
}
// After a restore: a fresh LIST. The patch, multi and pattern objects must equal the file's (size and CRC). The music and the settings are
// checked by the unit at commit but may read back in the unit's own normal form (an older record is brought up to date), so they are not compared.
export async function verifyRestore(request, plan) {
  const list = await listUnit(request), same = [], differ = [], notCompared = [];
  for (const s of plan.steps) {
    if (s.id === 0 || s.id === 1) { notCompared.push(s.id); continue; }
    const now = list.get(s.id);
    (now && now.size === s.size && now.crc === s.crc ? same : differ).push(s.id);
  }
  return { same, differ, notCompared };
}
export const fileName = (prefix, d = new Date()) => `${prefix}-${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}-${String(d.getHours()).padStart(2, "0")}${String(d.getMinutes()).padStart(2, "0")}.json`;
