// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the patch LIBRARIAN's work on the unit, without a page: read a slot whole (PATCH_RAW), write a slot whole, copy / swap / move slots, export a
// selection or the whole bank to items (patchfile.js), plan an import (targets, what each write replaces), run it with a safety copy, progress and a verify.
// A unit without PATCH_RAW (INFO tag 5A 01) can still export what UP_GET shows and import it as approximate copies (UP_PUT); the page says so.
// The unit's rules: every write stops the sequencer first (rc 3 if it would not stop), the flash write is power-cut safe (the old record or the new, whole).
// No DOM. test_librarian.mjs runs it against the simulated unit.
import { CMD, parseUpGet, upPutArgs } from "./proto.js";
import { L, ID } from "./layout.js";
import { bkU32, bkR32, bkPack, bkUnpack, bkCrc } from "../fm1backup.js";
import { itemFromRecord, itemApprox, buildFile, stringifyFile } from "./patchfile.js";
import { UnitError, UP_SLOTS } from "./unit.js";
import { CATEGORIES, parseRecord } from "./patchrecord.js";

export const PIECE = 112;                                        // data bytes of a PATCH_RAW piece (16 pack7 groups)
const RC = { 1: "The unit refused the request (a bad slot, or a record it cannot read).", 2: "The unit found the record incomplete or damaged (checksum).",
  3: "The unit is playing and could not stop. Stop playback on the unit and try again.", 4: "The unit could not write its flash memory. The slot is as it was.",
  5: "The transfer on the unit expired. Try again." };
const rcError = (rc, what = "") => new UnitError(`${what}${RC[rc] || `The unit answered error ${rc}.`}`, rc);

export const hasRaw = (u) => !!u.info?.patchRaw;

// ---- one slot, whole
export async function rawInfo(u, slot) {                         // -> { saved, len, crc }
  const a = await u.req(CMD.PATCH_RAW, [0, slot], 3000);
  if (a[2]) throw rcError(a[2]);
  return { saved: !!a[3], len: a[4] | a[5] << 7, crc: bkR32(a, 6) };
}
export async function rawRead(u, slot) {                         // -> null (the slot holds its factory sound) or the record's bytes (CRC checked)
  const info = await rawInfo(u, slot);
  if (!info.saved) return null;
  const out = new Uint8Array(info.len);
  for (let off = 0; off < info.len; off += PIECE) {
    const n = Math.min(PIECE, info.len - off), a = await u.req(CMD.PATCH_RAW, [1, slot, off & 127, off >> 7, n], 3000);
    if (a[2]) throw rcError(a[2]);
    if (a[1] !== slot || (a[3] | a[4] << 7) !== off || a[5] !== n) throw new UnitError("The unit answered out of order.");
    out.set(bkUnpack(a.slice(6), n), off);
  }
  if (bkCrc(out) !== info.crc) throw new UnitError("The unit changed the slot while it was read. Try again.");
  return out;
}
export async function rawWrite(u, slot, bytes, onProgress = () => {}) {   // begin, pieces, commit; the transfer is aborted on an error
  u.guard();
  const put = async (args, t = 3000) => { const a = await u.req(CMD.PATCH_RAW, args, t); if (a[2]) throw rcError(a[2]); };
  await put([2, slot, bytes.length & 127, bytes.length >> 7, ...bkU32(bkCrc(bytes))], 6000);
  try {
    for (let off = 0; off < bytes.length; off += PIECE) {
      await put([3, slot, off & 127, off >> 7, ...bkPack(bytes.subarray(off, off + PIECE))]);
      onProgress(Math.min(bytes.length, off + PIECE), bytes.length);
    }
    await put([4, slot], 8000);
  } catch (e) { await u.req(CMD.PATCH_RAW, [5, slot]).catch(() => {}); throw e; }
}

// ---- reading slots into items. A saved slot is its record (lossless); a slot that holds its factory sound is an approximate copy of its values (UP_GET)
export async function upGet(u, slot) { return parseUpGet(await u.req(CMD.UP_GET, [slot], 3000), L.P_COUNT); }
export async function readItem(u, slot, listed) {
  const li = listed ?? u.slots?.[slot];
  const number = slot + 1;
  if (hasRaw(u)) {
    const rec = await rawRead(u, slot);
    if (rec) return itemFromRecord(rec, { number });
  } else if (li?.saved) {                                        // a unit without PATCH_RAW: what UP_GET shows (the page says it is a copy of the values)
    const g = await upGet(u, slot);
    return itemApprox({ number, name: g.name, engine: g.eng, cat: g.cat, values: g.values });
  }
  const g = await upGet(u, slot);
  return itemApprox({ number, name: g.name, engine: g.eng, cat: g.cat, values: g.values });
}
export async function readItems(u, slots, onProgress = () => {}) {
  if (!u.slots) await u.listSlots();
  const out = [];
  for (const [i, s] of slots.entries()) { out.push(await readItem(u, s)); onProgress(i + 1, slots.length); }
  return out;
}
export const savedSlots = (u) => (u.slots || []).filter((s) => s.saved).map((s) => s.slot);
export function fileText(u, items, source) { return stringifyFile(buildFile(items, { firmware: u.info?.version ?? "", source })); }

// ---- writing an item into a slot
export async function writeItem(u, slot, item, onProgress) {
  u.guard();
  if (item.error) throw new UnitError(item.error);
  if (item.kind === "record") {
    if (!hasRaw(u)) throw new UnitError("This unit's firmware cannot take a patch whole (it needs DUELL 0.2.19). Update it with the installer.");
    await rawWrite(u, slot, item.record, onProgress);
  } else {                                                      // an approximate copy: UP_PUT (End B, the arp block and an FM6 sound's own patch are not part of it)
    const a = await u.req(CMD.UP_PUT, upPutArgs(slot, item.engine, item.name, item.values, item.cat), 8000);
    if (a[1] === 1) throw new UnitError("The unit refused the values or the name.", 1);
    if (a[1]) throw rcError(a[1]);
  }
}
// did the unit keep it? (a record: the same CRC; a copy of values: UP_GET shows the same values and name)
export async function verifyItem(u, slot, item) {
  if (item.kind === "record") { const i = await rawInfo(u, slot); return i.saved && i.len === item.record.length && i.crc === item.crc; }
  const g = await upGet(u, slot);
  return g.saved && g.name === item.name && g.eng === item.engine && g.values.every((v, i) => v === item.values[i]);
}

// ---- plans. A write is { slot, item }; `replaces` is what the slot holds now.
export function describeSlot(u, slot) {
  const s = u.slots?.[slot];
  return { slot, label: `#${slot + 1}`, name: s?.name ?? "", saved: !!s?.saved, engine: s ? L.ENG[s.eng]?.name : "", category: CATEGORIES[s?.cat] || "" };
}
// targets for a list of items: { mode: "same" (the item's own number), "from" (consecutive from `start`, 0-based), "pick" (explicit array) }
export function chooseTargets(u, items, { mode = "from", start = 0, skipSaved = false, picks = [] } = {}) {
  const t = [];
  if (mode === "same") for (const it of items) t.push(it.number != null ? it.number - 1 : null);
  else if (mode === "pick") for (let i = 0; i < items.length; i++) t.push(picks[i] ?? null);
  else {
    let s = start;
    for (let i = 0; i < items.length; i++) {
      while (s < UP_SLOTS && skipSaved && u.slots?.[s]?.saved) s++;
      t.push(s < UP_SLOTS ? s : null); s++;
    }
  }
  return t;
}
// the plan for items[i] -> targets[i] (null: no place); problems: two items to one slot, no place, a record this unit cannot take
export function makePlan(u, items, targets) {
  const writes = [], problems = [], seen = new Map();
  items.forEach((item, i) => {
    const slot = targets[i];
    if (slot == null) { problems.push(`${item.name}: no slot`); return; }
    if (slot < 0 || slot >= UP_SLOTS) { problems.push(`${item.name}: slot ${slot + 1} does not exist`); return; }
    if (seen.has(slot)) { problems.push(`${item.name} and ${seen.get(slot)} both go to #${slot + 1}`); return; }
    seen.set(slot, item.name);
    if (item.kind === "record" && !hasRaw(u)) { problems.push(`${item.name}: this unit's firmware cannot take a patch whole`); return; }
    writes.push({ slot, item, replaces: describeSlot(u, slot) });
  });
  return { writes, problems, savedLost: writes.filter((w) => w.replaces.saved).length };
}
export const confirmLines = (plan, max = 12) => {
  const lines = plan.writes.slice(0, max).map((w) => `${w.replaces.label} ${w.replaces.saved ? `"${w.replaces.name}" (saved)` : `"${w.replaces.name}" (factory sound)`} becomes "${w.item.name}"`);
  if (plan.writes.length > max) lines.push(`... and ${plan.writes.length - max} more slots.`);
  return lines;
};

// run a plan: the safety copy first when `safety` is given (called with the text of a file of the saved patches that will be replaced), then each write and its verify.
// -> { done: [slots], failed: { slot, error } | null, mismatched: [slots] }. A failure stops the run (the slots before it are written).
export async function runPlan(u, plan, { safety = null, safetyAt = 4, onProgress = () => {}, verify = true } = {}) {
  u.guard();
  const lost = plan.writes.filter((w) => w.replaces.saved);
  if (safety && plan.writes.length >= safetyAt && lost.length) {
    const items = await readItems(u, lost.map((w) => w.slot));
    await safety(fileText(u, items, "safety copy before an import"), items);
  }
  const res = { done: [], failed: null, mismatched: [] };
  for (const [i, w] of plan.writes.entries()) {
    onProgress({ index: i, total: plan.writes.length, slot: w.slot, name: w.item.name, phase: "write" });
    try {
      await writeItem(u, w.slot, w.item);
      if (verify && !(await verifyItem(u, w.slot, w.item))) res.mismatched.push(w.slot);
      res.done.push(w.slot);
    } catch (e) { res.failed = { slot: w.slot, error: e }; break; }
  }
  await u.listSlots();
  onProgress({ index: plan.writes.length, total: plan.writes.length, phase: "done" });
  return res;
}

// ---- copy, move, swap. What a slot "holds" for these is its saved record; a slot on its factory sound holds nothing of the user's.
//   copy a -> b: b becomes a (a record whole; a factory sound as an approximate copy)
//   swap a <-> b: two saved records change places; a saved record and a factory slot: the record moves (the old place shows its own factory sound again, b's
//   factory sound is not carried over); two factory slots: nothing to swap
// a stored patch's CATEGORY (0.2.25): the record read whole, byte 6 changed, written whole (the unit checks it at commit); a factory slot cannot be given one
export async function setCategory(u, slot, cat) {
  if (!hasRaw(u)) throw new UnitError("Changing a category needs firmware 0.2.19 or newer.");
  if (!(cat >= 1 && cat < CATEGORIES.length)) throw new UnitError("Pick a category.");
  const rec = await rawRead(u, slot);
  if (!rec) throw new UnitError("This slot holds a factory sound: its category is fixed. Store a sound in it first.");
  if (rec[6] === cat) return;
  const b = rec.slice(); b[6] = cat;
  await rawWrite(u, slot, b);
  await u.listSlots();
}
// SAVE TO UNIT from the PATCHES form: the patch in slot `src` (read whole, as the unit holds it) is stored in slot `dst` under `name` and `cat`. Nothing of the
// parts is read or sent: the sound a part plays has no say in it. A saved patch is its record with the name (bytes 7..18) and the category (byte 6) changed
// and written back through PATCH_RAW; a factory sound goes as the copy of its values (UP_PUT, as in a copy). The result is read back.
export async function saveAs(u, src, dst, { name, cat = null } = {}) {
  const nm = String(name ?? "").replace(/[^ -~]/g, "").trim().slice(0, 12);
  if (!nm) throw new UnitError("Give the patch a name (1 to 12 characters).");
  const it = await readItem(u, src);
  if (it.error) throw new UnitError(it.error);
  let item;
  if (it.kind === "record") {
    const b = it.record.slice(); b.fill(0, 7, 19); [...nm].forEach((c, i) => { b[7 + i] = c.charCodeAt(0); });
    if (cat != null && cat >= 0 && cat < CATEGORIES.length) b[6] = cat;
    item = itemFromRecord(b, { number: dst + 1 });
    if (item.error) throw new UnitError(item.error);
  } else item = { ...it, name: nm, cat: cat != null && cat >= 0 && cat < CATEGORIES.length ? cat : it.cat, number: dst + 1 };
  const plan = makePlan(u, [item], [dst]);
  if (plan.problems.length) throw new UnitError(plan.problems[0]);
  const r = await runPlan(u, plan, {});
  if (r.failed) throw r.failed.error;
  if (r.mismatched?.length) throw new UnitError("The unit did not read the patch back the same. Try again.");
  await u.listSlots();
  return item;
}
export async function planCopy(u, a, b) { const it = await readItem(u, a); return makePlan(u, [it], [b]); }
export async function swapSlots(u, a, b) {
  u.guard();
  if (!u.slots) await u.listSlots();
  const sa = u.slots[a].saved, sb = u.slots[b].saved;
  if (!sa && !sb) return { moved: false };
  const ia = sa ? await readItem(u, a) : null, ib = sb ? await readItem(u, b) : null;
  if (ia) await writeItem(u, b, ia);                            // (both saved: the first record is in a second place before the other one is overwritten)
  if (ib) await writeItem(u, a, ib); else await restoreQuiet(u, a);
  if (!ia && ib) await restoreQuiet(u, b);
  if (ia && !(await verifyItem(u, b, ia))) throw new UnitError(`The patch in #${b + 1} did not read back the same.`);
  if (ib && !(await verifyItem(u, a, ib))) throw new UnitError(`The patch in #${a + 1} did not read back the same.`);
  await u.listSlots();
  return { moved: true, both: sa && sb };
}
// move a saved patch: written to b (what b held is gone), read back, and a shows its own factory sound again
export async function moveSlot(u, a, b) {
  u.guard();
  const it = await readItem(u, a);
  if (!u.slots[a].saved) throw new UnitError(`#${a + 1} holds a factory sound; there is nothing to move.`);
  await writeItem(u, b, it);
  if (!(await verifyItem(u, b, it))) throw new UnitError(`The patch in #${b + 1} did not read back the same; #${a + 1} was left as it is.`);
  await restoreQuiet(u, a);
  await u.listSlots();
}
async function restoreQuiet(u, slot) {
  const a = await u.req(CMD.UP_ERASE, [slot], 6000);
  if (a[1]) throw rcError(a[1]);
}
export function swapLines(u, a, b) {
  const x = describeSlot(u, a), y = describeSlot(u, b);
  if (x.saved && y.saved) return [`${x.label} "${x.name}" and ${y.label} "${y.name}" change places.`];
  const [from, to] = x.saved ? [x, y] : [y, x];
  return [`${from.label} "${from.name}" moves to ${to.label}, replacing "${to.name}" (a factory sound).`, `${from.label} shows its own factory sound again ("${from.name}" is no longer there). Restore brings back a factory sound.`];
}

// ---- a patch into a PART, live, without saving anywhere (a drop on a part column). The part gets the sound the way the unit loads a slot (ui.c param_kept): every
// parameter but the mixer LEVEL, PAN, MUTE and the arp / keys / sequencer ids (those of a patch that carries its own arp block are set); the sound's loudness is its TRIM; an FM6 sound brings its own patch.
// The 16 steps, End B and the pattern stay with a saved patch (the protocol has no command to set them). -> { notes: [text] }
const KEPT = (i) => i === ID.P_LEVEL || i === ID.P_PAN || i === ID.P_MUTE || (i >= 17 && i <= 32) || i === 81 || i === 82 || i === 94;
const ARPKEY = (i) => (i >= 17 && i <= 24) || i === 81 || i === 82;      // the arp's eight values (P_AMODE .. P_AORDER) and the chord keys (P_CHRD, P_VOIC): a patch always brings its own
export async function loadItemIntoPart(u, part, item) {
  u.guard();
  let values, eng, patch128 = null, ak = null; const notes = [];
  if (item.kind === "record") {
    const r = parseRecord(item.record); if (!r.ok) throw new UnitError(r.error);
    values = r.values; eng = r.engine; patch128 = r.fm6; ak = r.ak;
    if (r.mb) notes.push("its End B"); if (r.pat || ak?.steps) notes.push("its pattern");
  } else { values = item.values; eng = item.engine; }
  await u.select(part); await u.idle();
  if (u.parts[part].eng !== eng) await u.setEngine(part, eng); else await u.readPart(part);
  if (patch128) {
    const a = await u.req(CMD.FM6_PUT, [0, part, ...patch128], 6000);
    if (a[2]) throw new UnitError("The unit did not take the FM6 patch.", a[2]);
  }
  const want = (i) => !KEPT(i) || ARPKEY(i) || ak?.values.has(i);         // (DUELL 1.0: every patch sets the arp and the chord keys, OFF and the defaults too: a record without the block is the defaults)
  for (let pass = 0; pass < 2; pass++) {
    await u.readPart(part);
    const p = u.parts[part].p, diff = [];
    for (let i = 0; i < L.P_COUNT; i++) if (want(i) && p[i] !== values[i]) diff.push(i);
    if (!diff.length) break;
    for (const i of diff) u.writeParam(part, i, values[i]);
    await u.idle();
  }
  await u.readPart(part);
  u.parts[part].name = item.name;
  u.parts[part].slot = null;                                // (a file's sound: in no slot of the unit)
  const left = [...Array(L.P_COUNT).keys()].filter((i) => want(i) && u.parts[part].p[i] !== values[i]).length;
  if (left) notes.push(`${left} value${left > 1 ? "s" : ""} the part would not take`);
  return { notes };
}
