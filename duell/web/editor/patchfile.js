// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the "duell-patches" file and the "item" the librarian works with. No DOM, no I/O (the page reads and saves the text).
//
// A file is JSON: { format: "duell-patches", version: 1, firmware, created, source, patches: [ ... ] }. Each patch:
//   number     the slot it came from, as the unit shows it (#NN, 1..128), or absent
//   name, engine (its name, "FM6"), category (its name)
//   saved      true: the patch is a saved record of the slot; false: the slot held a factory sound (a copy of its values only: "approximate")
//   record     base64 of the unit's STORED RECORD, lossless (PATCH_RAW; patchrecord.js), with its CRC-32 in crc32 (8 hex digits)
//   values     (approximate patches only) the P_COUNT parameter values, a numbers array
//   params     the readable dump: what differs from the engine's defaults, named as the editor's pages name it. For reading and diffing; never read back
// The record is authoritative. An item (below) is what the page holds for a patch from a unit, a file or a DX7 sysex file.
import { L } from "./layout.js";
import { bkCrc } from "../fm1backup.js";
import { parseRecord, describeRecord, CATEGORIES, engineIndex } from "./patchrecord.js";

export const FORMAT = "duell-patches", FILE_VERSION = 1;
export const hex8 = (n) => (n >>> 0).toString(16).padStart(8, "0");
export function toBase64(bytes) { let s = ""; for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode(...bytes.subarray(i, i + 0x8000)); return btoa(s); }
export function fromBase64(text) {
  if (typeof text !== "string" || !/^[A-Za-z0-9+/]*={0,2}$/.test(text.replace(/\s+/g, ""))) throw new Error("not base64");
  const s = atob(text.replace(/\s+/g, "")), out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i);
  return out;
}

// ---- items
// { number (1..128 | null), name, engine (index), engineName, cat (index), catName, kind: "record" | "approx", record (Uint8Array) | null, crc, values (approx) | null,
//   source, params, warnings: [], error: null | text }
export function itemFromRecord(record, { number = null, source = "unit", warnings = [] } = {}) {
  const r = parseRecord(record);
  if (!r.ok) return { number, name: "?", engine: -1, engineName: "?", cat: 0, catName: "-", kind: "record", record: null, crc: 0, values: null, source, params: {}, warnings, error: r.error };
  return { number, name: r.name, engine: r.engine, engineName: L.ENG[r.engine].name, cat: r.cat, catName: CATEGORIES[r.cat] || "-", kind: "record", record: Uint8Array.from(record),
    crc: bkCrc(record), values: null, source, params: describeRecord(r).params, warnings, error: null, carries: describeRecord(r).carries };
}
export function itemApprox({ number = null, name, engine, cat = 0, values, source = "unit" }) {
  const e = L.ENG[engine];
  return { number, name, engine, engineName: e?.name ?? "?", cat, catName: CATEGORIES[cat] || "-", kind: "approx", record: null, crc: 0, values: values.slice(), source,
    params: {}, warnings: ["A factory sound: a copy of its values (its arp block and pattern are not copied)."], error: e ? null : "unknown engine" };
}

// ---- writing a file
function fileEntry(it) {
  const e = { number: it.number ?? undefined, name: it.name, engine: it.engineName, category: it.catName, saved: it.kind === "record" };
  if (it.kind === "record") { e.crc32 = hex8(it.crc); e.record = toBase64(it.record); }
  else { e.approximate = true; e.values = it.values; }
  e.params = it.params;
  return e;
}
export function buildFile(items, { firmware = "", source = "" } = {}) {
  return { format: FORMAT, version: FILE_VERSION, app: "DUELL editor", firmware, created: new Date().toISOString().slice(0, 19) + "Z", source, patches: items.map(fileEntry) };
}
export const stringifyFile = (f) => JSON.stringify(f, null, 1) + "\n";
export function fileName(items, kind = "") {
  const stamp = new Date().toISOString().slice(0, 10);
  if (items.length === 1) return `${items[0].name.replace(/[^A-Za-z0-9 _.-]/g, "").trim().replace(/ +/g, "-") || "patch"}.duell-patches.json`;
  return `duell-${kind || "patches"}-${items.length}-${stamp}.duell-patches.json`;
}

// ---- reading a file: { ok, error, items, problems: [text], firmware, source }
export function parseFile(text) {
  let j;
  try { j = JSON.parse(text); } catch { return { ok: false, error: "This is not a JSON file." }; }
  if (!j || j.format !== FORMAT) return { ok: false, error: j?.format === "duell-wavetable" ? "This is a wavetable file, not a patch file." : "This is not a DUELL patch file." };
  if (!Number.isInteger(j.version) || j.version > FILE_VERSION) return { ok: false, error: `This patch file is version ${j.version}; this editor reads version ${FILE_VERSION}.` };
  if (!Array.isArray(j.patches)) return { ok: false, error: "The file has no list of patches." };
  const items = [], problems = [];
  j.patches.forEach((p, i) => {
    const where = `Patch ${i + 1}${typeof p?.name === "string" ? ` (${p.name})` : ""}`;
    const number = Number.isInteger(p?.number) && p.number >= 1 && p.number <= 128 ? p.number : null;
    const bad = (why) => { problems.push(`${where}: ${why}`); };
    if (!p || typeof p !== "object") return bad("not a patch.");
    if (typeof p.record === "string") {
      let bytes;
      try { bytes = fromBase64(p.record); } catch { return bad("the record is damaged (not base64)."); }
      if (p.crc32 != null && hex8(bkCrc(bytes)) !== String(p.crc32).toLowerCase().padStart(8, "0")) return bad("the record fails its checksum, so it was damaged or edited.");
      const it = itemFromRecord(bytes, { number, source: "file" });
      if (it.error) return bad(it.error);
      if (typeof p.engine === "string" && p.engine !== it.engineName) return bad(`the file says engine ${p.engine} but the record is ${it.engineName}.`);
      items.push(it);
    } else if (p.approximate && Array.isArray(p.values)) {
      const eng = L.ENG.findIndex((e) => e.name === p.engine);
      if (eng < 0 || typeof p.name !== "string" || !/^[ -~]{1,12}$/.test(p.name)) return bad("the engine or the name is not usable.");
      if (p.values.length !== L.P_COUNT || p.values.some((v) => !Number.isInteger(v) || v < -64 || v > 127)) return bad("the values are not this unit's parameters.");
      items.push(itemApprox({ number, name: p.name, engine: eng, cat: Math.max(0, CATEGORIES.indexOf(p.category)), values: p.values, source: "file" }));
    } else bad("it has neither a record nor values.");
  });
  return { ok: true, items, problems, firmware: String(j.firmware ?? ""), source: String(j.source ?? "") };
}

// a dropped file's kind from its bytes: "patches" (JSON), "wavetable" (JSON), "dx7" (sysex / raw voices), "wav", or "unknown"
export function sniff(bytes, name = "") {
  const t = bytes.length > 4 && bytes[0] === 0x52 && bytes[1] === 0x49 && bytes[2] === 0x46 && bytes[3] === 0x46;
  if (t) return "wav";
  if (bytes.length >= 6 && bytes[0] === 0xf0) return "dx7";
  if (bytes[0] === 0x7b || bytes[0] === 0x20 || bytes[0] === 0x0a || bytes[0] === 0xef) {
    const head = new TextDecoder().decode(bytes.subarray(0, 400));
    if (head.includes('"duell-wavetable"')) return "wavetable";
    if (head.includes('"duell-patches"')) return "patches";
    return /\.json$/i.test(name) ? "patches" : "unknown";
  }
  if ((/\.syx$/i.test(name) && bytes.length % 4096 === 0) || bytes.length === 155 || bytes.length === 4096) return "dx7";
  return "unknown";
}
