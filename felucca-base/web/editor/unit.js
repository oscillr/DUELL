// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the unit as the editor sees it. Reads both parts and the globals, writes parameters (coalesced, one request at a time through
// the link), follows the unit by WATCH pushes, loads and stores patches, loads and saves multis, reads and sets MENU settings.
// No DOM: editor/test_editor.mjs runs it against the simulated unit.
import { CMD, v14, rv14, cstr, parseInfo, parseTrackQuery, parseTrackDump, parseDump, parseNames, parseUpList, parseMenu, parseLearn, parseStep, stepBytes } from "./proto.js";
import { L, ID, P_E0, descOf, clampTo, inertReason } from "./layout.js";
import { BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc } from "../fm1backup.js";

export const UP_SLOTS = 128;   // the patch slots, ids 0..127 (DUELL 0.2.17: one list; a slot is its saved record or else the factory sound at that place of the list)
export class UnitError extends Error { constructor(msg, rc) { super(msg); this.rc = rc; } }

export class Unit {
  constructor(link) {
    this.link = link; this.info = null; this.compat = null; this.parts = []; this.g = []; this.selected = 0; this.ntrk = 2;
    this.listeners = new Map(); this.sending = new Map(); this.latest = new Map(); this.menu = null; this.slots = null; this.names = new Map();
    link.onpush = (cmd, a) => this.push(cmd, a);
  }
  on(type, fn) { (this.listeners.get(type) || this.listeners.set(type, []).get(type)).push(fn); return this; }
  off(type, fn) { const l = this.listeners.get(type); if (l) { const i = l.indexOf(fn); if (i >= 0) l.splice(i, 1); } return this; }
  emit(type, d) { for (const fn of this.listeners.get(type) || []) fn(d); }
  req(cmd, args = [], timeout) { return this.link.request([cmd, args], { timeout }); }
  get writable() { return !!this.info && this.compat === null; }   // compat: null = fine, else the reason the unit is read-only

  // ---- connect
  async connect() {
    this.info = parseInfo(await this.req(CMD.INFO, [], 2500));
    const i = this.info;
    if (i.pcount !== L.P_COUNT || i.gcount !== L.G_COUNT || i.pe0 !== L.P_E0 || i.nEng !== L.NENGINES) {
      this.compat = `This unit reports ${i.pcount} parameters (engine parameters from ${i.pe0}); this editor knows ${L.P_COUNT} (from ${L.P_E0}). Nothing is written.`;
    } else if (i.engines.some((n, k) => n !== L.ENG[k].name)) {
      this.compat = "This unit's engine list differs from the one this editor was built for. Nothing is written.";
    } else this.compat = null;
    this.ntrk = Math.min(i.ntrk || 2, 4);
    await this.req(CMD.WATCH, [3]);
    this.link.keepAlive?.(1000);
    await this.readAll();
    this.emit("connected", this);
  }
  async readAll() {
    const t = parseTrackQuery(await this.req(CMD.TRACK), this.ntrk);
    this.selected = Math.min(t.selected, 1);
    this.parts = [];
    for (let k = 0; k < 2; k++) {
      const d = parseTrackDump(await this.req(CMD.TRACK_DUMP, [k]), L.P_COUNT);
      this.parts.push({ eng: d.eng, preset: d.preset, p: d.p, name: null });
    }
    this.g = parseDump(await this.req(CMD.DUMP), L.P_COUNT, L.G_COUNT).g;
    this.emit("reload", { part: -1 });
  }
  async readPart(k) {
    const d = parseTrackDump(await this.req(CMD.TRACK_DUMP, [k]), L.P_COUNT);
    this.parts[k] = { eng: d.eng, preset: d.preset, p: d.p, name: this.parts[k]?.name ?? null };
    this.emit("reload", { part: k });
  }
  async readGlobals() { this.g = parseDump(await this.req(CMD.DUMP), L.P_COUNT, L.G_COUNT).g; this.emit("globals", {}); }
  disconnect() { this.link.close(); }

  // ---- following the unit
  push(cmd, a) {
    if (cmd === CMD.CHANGED) {                       // scope, id, v14 (the selected track or a global)
      const v = rv14(a, 2);
      if (a[0] === 1) { this.g[a[1]] = v; this.emit("global", { id: a[1], v }); }
      else if (this.parts[this.selected]) this.set(this.selected, a[1], v, false);
    } else if (cmd === CMD.TRACK_CHANGED) {          // track, id, v14
      if (a[0] < 2) this.set(a[0], a[1], rv14(a, 2), false);
    } else if (cmd === CMD.STEP_CHANGED) {            // index, track
      this.emit("step", { index: a[0], part: a[1] ?? this.selected });
    } else if (cmd === CMD.RELOAD) {                 // engine, preset, track
      const t = a[2] ?? this.selected;
      if (t < 2) { this.selected = t; this.readPart(t).catch((e) => this.emit("error", e)); this.readGlobals().catch(() => {}); }
    }
  }
  set(part, id, v, byUs) {
    const p = this.parts[part]; if (!p) return;
    p.p[id] = v; this.emit("param", { part, id, v, byUs });
  }

  // ---- parameter writes: the latest value of a parameter goes out when the one before is answered (a dragged slider sends what it needs)
  writeParam(part, id, v) {
    this.guard();
    const d = descOf(this.parts[part], id); v = clampTo(d, Math.round(v));
    this.set(part, id, v, true);
    this.queueWrite(`p${part}:${id}`, v, async (val) => {
      const a = await this.req(CMD.TRACK_PARAM, [part, id, ...v14(val)]);
      const got = rv14(a, 2); if (got !== val) this.set(part, id, got, true);
    });
  }
  // an edit with the two parts linked (the editor's BOTH): the same step on the other part; an engine parameter only while both play the same
  // engine; never on a control that is inert there; an enum is copied only where both descriptors are the same control
  writeParamLinked(part, id, v) {
    const old = this.parts[part].p[id];
    this.writeParam(part, id, v);
    const q = 1 - part, a = this.parts[part], b = this.parts[q];
    if (id >= P_E0 && a.eng !== b.eng) return;
    if (inertReason(b, id)) return;
    const da = descOf(a, id), db = descOf(b, id), nv = a.p[id];
    this.writeParam(q, id, db.names ? (da.label === db.label ? nv : b.p[id]) : b.p[id] + (nv - old));
  }
  writeGlobal(id, v) {
    this.guard();
    v = clampTo(L.GP[id], Math.round(v)); this.g[id] = v; this.emit("global", { id, v, byUs: true });
    this.queueWrite(`g:${id}`, v, async (val) => {
      const a = await this.req(CMD.SET, [1, id, ...v14(val)]);
      const got = rv14(a, 2); if (got !== val) { this.g[id] = got; this.emit("global", { id, v: got, byUs: true }); }
    });
  }
  queueWrite(key, v, send) {
    this.latest.set(key, v);
    if (this.sending.get(key)) return;
    this.sending.set(key, true);
    (async () => {
      try {
        while (this.latest.has(key)) { const val = this.latest.get(key); this.latest.delete(key); await send(val); }
      } catch (e) { this.latest.delete(key); this.emit("error", e); }
      finally { this.sending.set(key, false); }
    })();
  }
  async idle() { while ([...this.sending.values()].some(Boolean)) await new Promise((r) => setTimeout(r, 5)); }
  guard() { if (!this.writable) throw new UnitError(this.compat || "Not connected"); }

  // ---- the selected part of the unit (what ALGORITHM does on the device)
  async select(part) {
    this.guard();
    if (this.selected === part) return;
    await this.idle();
    const a = await this.req(CMD.TRACK, [part]); this.selected = Math.min(parseTrackQuery(a, this.ntrk).selected, 1);
    this.emit("selected", { part: this.selected });
  }
  async setEngine(part, eng) {
    await this.select(part); await this.idle();
    await this.req(CMD.SET, [1, ID.G_ENGSEL, ...v14(eng)]);
    await this.readPart(part);
  }

  // ---- patch slots (DUELL 0.2.17: ONE list of 128 slots; slot i is its saved record, or else the factory sound at browse position i, #NN = i + 1)
  async factoryNames(eng) {
    if (this.names.has(eng)) return this.names.get(eng);
    const n = parseNames(await this.req(CMD.NAMES, [eng])).names; this.names.set(eng, n); return n;
  }
  async loadFactory(part, eng, idx) {                 // a factory sound by engine and index (the EDIT pages' sound list; the slots do not need it)
    await this.select(part); await this.idle();
    await this.req(CMD.PRESET, [eng, idx]);
    await this.readPart(part);
  }
  // the 128 slots: [{slot, eng, name, cat, saved}] (saved: it holds a saved record; false: it holds its factory sound)
  async listSlots(onProgress) {
    const all = [];
    for (let s = 0; s < UP_SLOTS; s += 16) {
      const r = parseUpList(await this.req(CMD.UP_LIST, [s, 16]));
      all.push(...r.slots); onProgress?.(s + 16, UP_SLOTS);
    }
    this.slots = all.slice(0, UP_SLOTS); this.emit("patches", this.slots); return this.slots;
  }
  async loadSlot(part, slot) {                        // the slot's record, else its factory sound, into the part
    await this.select(part); await this.idle();
    const a = await this.req(CMD.UP_LOAD, [slot]);
    if (a[1] !== 0) throw new UnitError("That slot is not valid.", a[1]);
    await this.readPart(part);
  }
  // Saves the part's CURRENT sound over a slot (UP_STORE: the unit stores it itself, so End B and the arp block are kept; UP_PUT would drop them). What
  // the slot held is gone (a saved record, or nothing but its factory sound, which RESTORE brings back).
  async saveSlot(part, slot, name) {
    await this.select(part); await this.idle();
    const a = await this.req(CMD.UP_STORE, [slot, ...cstr(name)], 6000);
    if (a[1] === 1) throw new UnitError("The unit refused the name or the slot.", 1);
    if (a[1] === 2) throw new UnitError("The unit could not write: it could not stop playback in time, or the flash failed. Stop playback on the unit and try again.", 2);
    if (a[1] !== 0) throw new UnitError(`The unit answered error ${a[1]}.`, a[1]);
    await this.listSlots();
  }
  // RESTORE: the slot's saved record is removed and its factory sound is back (UP_ERASE; a slot that holds its factory sound already: rc 0, nothing written)
  async restoreSlot(slot) {
    this.guard();
    const a = await this.req(CMD.UP_ERASE, [slot], 6000);
    if (a[1] !== 0) throw new UnitError("The unit could not restore the slot (playback could not be stopped, or a flash error).", a[1]);
    await this.listSlots();
  }
  // RESTORE ALL: every slot that holds a saved record, one after the other (the unit does a power cut safe write for each)
  async restoreAll(onProgress) {
    this.guard();
    const todo = (this.slots || await this.listSlots()).filter((s) => s.saved).map((s) => s.slot);
    for (const [i, s] of todo.entries()) {
      const a = await this.req(CMD.UP_ERASE, [s], 6000);
      if (a[1] !== 0) { await this.listSlots(); throw new UnitError(`The unit could not restore slot ${s + 1} (playback could not be stopped, or a flash error). The slots before it are restored; run it again.`, a[1]); }
      onProgress?.(i + 1, todo.length);
    }
    await this.listSlots();
    return todo.length;
  }

  // ---- multis
  async multiUsed() {
    const used = [];
    for (let s = 0; s < 16; s++) { const a = await this.req(CMD.PROJECT, [2, s]); used.push(!!a[2]); }
    this.multis = used; this.emit("multis", used); return used;
  }
  async loadMulti(slot) {
    this.guard(); await this.idle();
    await this.req(CMD.PROJECT, [0, slot], 4000);
    await this.readAll();
  }
  async saveMulti(slot) {
    this.guard(); await this.idle();
    let a;
    try { a = await this.req(CMD.PROJECT, [1, slot], 8000); }
    catch { throw new UnitError("The unit did not confirm the save. It refuses to save while playing; stop playback on the unit and check the multi there."); }
    if (!a[2]) throw new UnitError("The unit did not store the multi.");
    await this.multiUsed();
  }
  // the multi objects (BACKUP ids 13..20; a LIST stops playback on the unit): returns [{slot, name} | null] x16 (stored records only)
  async readMultiObjects() {
    const l = await this.req(BACKUP_CMD.LIST, [], 4000);
    if (l[0] !== 1 || l[1] !== 0) throw new UnitError(l[1] === 3 ? "The unit is playing. Stop playback on the unit first." : `The unit refused the list (error ${l[1]}).`, l[1]);
    const n = l[2], sizes = new Map();
    for (let i = 0; i < n; i++) sizes.set(l[3 + i * 11], bkR32(l, 3 + i * 11 + 1));
    const objs = [];
    for (let o = 0; o < 8; o++) {
      const size = sizes.get(13 + o) ?? 0, b = new Uint8Array(size);
      for (let off = 0; off < size; off += 256) {
        const c = Math.min(256, size - off), a = await this.req(BACKUP_CMD.GET, [13 + o, ...bkU32(off), c & 127, c >> 7], 2500);
        if (a[1]) throw new UnitError(`The unit refused to read the multis (error ${a[1]}).`, a[1]);
        b.set(bkUnpack(a.slice(9), c), off);
      }
      objs.push(b);
    }
    this.multiObjs = objs;
    const names = Array(16).fill(null);
    objs.forEach((b, o) => { if (b.length >= 8) for (let h = 0, at = 8; h < 2; at += b[4 + 2 * h] | b[5 + 2 * h] << 8, h++) { const len = b[4 + 2 * h] | b[5 + 2 * h] << 8; if (len) names[2 * o + h] = text(b, at + 8, 12); } });
    this.multiNames = names; this.emit("multinames", names); return names;
  }
  // rename a STORED multi: the object is read back, the name bytes changed and the object written (the partner multi's record is copied unchanged)
  async renameMulti(slot, name) {
    this.guard(); await this.idle();
    if (!/^[ -~]{1,12}$/.test(name)) throw new UnitError("A name is 1 to 12 plain characters.");
    await this.readMultiObjects();
    const o = slot >> 1, b = this.multiObjs[o].slice(), h = slot & 1;
    const len = [b[4] | b[5] << 8, b[6] | b[7] << 8];
    if (b.length < 8 || !len[h]) throw new UnitError("This multi is not stored on the unit (a factory or empty slot): save it first, then rename it.");
    const at = 8 + (h ? len[0] : 0) + 8;
    for (let i = 0; i < 12; i++) b[at + i] = i < name.length ? name.charCodeAt(i) : 0;
    const put = async (args) => { const a = await this.req(BACKUP_CMD.PUT, args, 8000); if (a[2]) throw new UnitError(`The unit refused the write (error ${a[2]}).`, a[2]); };
    const id = 13 + o;
    await put([0, id, ...bkU32(b.length), ...bkU32(bkCrc(b))]);
    try {
      for (let off = 0; off < b.length; off += 256) await put([1, id, ...bkU32(off), ...bkPack(b.subarray(off, off + 256))]);
      await put([2, id]);
    } catch (e) { await this.req(BACKUP_CMD.PUT, [3, id]).catch(() => {}); throw e; }
    await this.readMultiObjects();
  }

  // ---- MENU settings and MIDI LEARN
  async readMenu() {
    const items = [];
    for (let i = 0; i < (this.info.menuCount ?? 0); i++) { const m = parseMenu(await this.req(CMD.MENU_DESC, [i])); if (m) items.push(m); }
    this.menu = items; this.emit("menu", items); return items;
  }
  async setMenu(id, v) {
    this.guard();
    const a = await this.req(CMD.MENU_SET, [id, ...v14(v)], 3000);
    if (a[0] === 1) throw new UnitError("The unit does not know that setting.", 1);
    const m = this.menu?.find((x) => x.id === id); if (m) m.value = rv14(a, 2);
    if (id === 0 && this.menu) this.emit("menu", this.menu);   // COLOR: the page's theme follows
    return rv14(a, 2);
  }
  async readLearn() { this.learn = this.info.learn ? parseLearn(await this.req(CMD.LEARN_GET)) : null; this.emit("learn", this.learn); return this.learn; }
  async learnClear(cc) {
    this.guard(); const a = await this.req(CMD.LEARN_SET, cc == null ? [2] : [1, cc], 4000);
    if (a[0] !== 0 && a[0] !== 3 && a[0] !== 4) throw new UnitError(`The unit refused (error ${a[0]}).`, a[0]);
    this.learn = parseLearn(a); this.emit("learn", this.learn);
  }
  async learnSet(cc, track, id) {
    this.guard(); const a = await this.req(CMD.LEARN_SET, [0, cc, track, id], 4000);
    if (a[0] === 5) throw new UnitError("The learn map is full (16 entries): clear one first.", 5);
    if (a[0] === 1) throw new UnitError("The unit does not accept that CC (some are never learned: bank select, mod wheel, expression, pedal ...).", 1);
    this.learn = parseLearn(a); this.emit("learn", this.learn);
  }

  // ---- steps of a part (TRACK_STEP reaches any part)
  async readSteps(part, n) {
    const out = [];
    for (let i = 0; i < n; i++) out.push(parseStep(await this.req(CMD.TRACK_STEP, [part, i]), 1));
    return out;
  }
  async readStep(part, index) { return parseStep(await this.req(CMD.TRACK_STEP, [part, index]), 1); }
  async writeStep(part, index, s) {
    this.guard();
    const a = await this.req(CMD.TRACK_STEP, [part, index, ...stepBytes(s)]);
    return parseStep(a, 1);
  }
}
const text = (b, from, n) => { let s = ""; for (let i = 0; i < n && b[from + i]; i++) s += String.fromCharCode(b[from + i]); return s; };
