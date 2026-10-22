// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the unit as the editor sees it. Reads both parts and the globals, writes parameters (coalesced, one request at a time through
// the link), follows the unit by WATCH pushes, loads and stores patches, loads and saves multis, reads and sets MENU settings.
// No DOM: editor/test_editor.mjs runs it against the simulated unit.
import { CMD, v14, rv14, cstr, parseInfo, parseTrackQuery, parseTrackDump, parseDump, parseNames, parseUpList, parseMenu, parseLearn, parseStep, stepBytes, xReadArgs, xWriteArgs, parseXReply } from "./proto.js";
import { L, ID, P_E0, NX, X, descOf, xdescOf, xDefaults, clampTo } from "./layout.js";
import { BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc } from "../fm1backup.js";

export const UP_SLOTS = 128;   // the patch slots, ids 0..127 (DUELL 0.2.17: one list; a slot is its saved record or else the factory sound at that place of the list)
export class UnitError extends Error { constructor(msg, rc) { super(msg); this.rc = rc; } }
// the filter cutoffs (fmt 23 FILT: the multimode filter's CUT, 0..82; fmt 5 CUTOFF: an engine's own) move in steps of at most this many per write (see queueWrite)
const slewOf = (d) => (d && !d.names && (d.fmt === 23 || d.fmt === 5) ? Math.max(2, Math.round((d.max - d.min) / 24)) : 0);

export class Unit {
  static POLL_REST = 4;                                // the poll reads this many of the other MENU rows a turn (pollSettings)
  static POLL_G = 4;                                   // .. and the global dump every this many polls
  static MIN_GAP = 20;                                 // ms between two writes of one parameter (0.2.25)
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
    this.startPoll();
    this.emit("connected", this);
  }
  async readAll() {
    const t = parseTrackQuery(await this.req(CMD.TRACK), this.ntrk);
    this.selected = Math.min(t.selected, 1);
    this.parts = [];
    for (let k = 0; k < 2; k++) {
      const d = parseTrackDump(await this.req(CMD.TRACK_DUMP, [k]), L.P_COUNT);
      this.parts.push({ eng: d.eng, preset: d.preset, p: d.p, x: await this.readX(k), name: null });
    }
    this.g = parseDump(await this.req(CMD.DUMP), L.P_COUNT, L.G_COUNT).g;
    this.emit("reload", { part: -1 });
  }
  async readPart(k) {
    const d = parseTrackDump(await this.req(CMD.TRACK_DUMP, [k]), L.P_COUNT);
    this.parts[k] = { eng: d.eng, preset: d.preset, p: d.p, x: await this.readX(k), name: this.parts[k]?.name ?? null, slot: this.parts[k]?.slot ?? null };   // (slot: the patch slot the editor loaded or stored, 0.2.25 layout round; null: not known)
    this.emit("reload", { part: k });
  }
  // the part's extension slots (protocol scope 2, INFO tag 59): NX values through TRACK_PARAM's scope byte; a unit without the tag has none, the slots are their defaults (the FILTER tab's cards say so)
  async readX(part) {
    const x = xDefaults();
    if (this.info?.nx == null) return x;
    for (let k = 0; k < Math.min(NX, this.info.nx); k++) x[k] = parseXReply(await this.req(CMD.TRACK_PARAM, xReadArgs(part, k))).v;
    return x;
  }
  get hasX() { return this.info?.nx != null; }
  async readGlobals() { this.g = parseDump(await this.req(CMD.DUMP), L.P_COUNT, L.G_COUNT).g; this.emit("globals", {}); }
  disconnect() { clearInterval(this.pollTimer); this.link.close(); }

  // ---- following the unit
  push(cmd, a) {
    if (cmd === CMD.CHANGED) {                       // scope, id, v14 (the selected track, a global, or scope 2: an extension slot of the selected track)
      const v = rv14(a, 2);
      if (a[0] === 1) { this.g[a[1]] = v; this.emit("global", { id: a[1], v }); }
      else if (a[0] === 2) { if (this.parts[this.selected]) this.setX(this.selected, a[1], v, false); }
      else if (this.parts[this.selected]) this.set(this.selected, a[1], v, false);
    } else if (cmd === CMD.TRACK_CHANGED) {          // track, id, v14
      if (a[0] < 2) this.set(a[0], a[1], rv14(a, 2), false);
    } else if (cmd === CMD.STEP_CHANGED) {            // index, track
      this.emit("step", { index: a[0], part: a[1] ?? this.selected });
    } else if (cmd === CMD.RELOAD) {                 // engine, preset, track
      const t = a[2] ?? this.selected;
      // 0.2.25 (owner: after a multi load on the unit only the selected part followed): the unit pushes RELOAD for its selected part only (editor.c
      // ed_poll), but a multi load, a selection change or a sound load may have changed both: both parts and the globals are read again
      if (t < 2) { const was = this.selected; this.selected = t; this.emit("selected", { part: t, was }); this.readBoth(true).catch((e) => this.emit("error", e)); }
    }
  }
  // both parts and the globals again (a load on the unit). byUnit: a part whose sound changed loses the name the editor gave it (the unit does not say which slot it plays)
  async readBoth(byUnit = false) {
    if (this.reading) { this.readAgain = true; return this.reading; }
    this.reading = (async () => {
      do {
        this.readAgain = false;
        for (let k = 0; k < 2; k++) {
          const before = this.parts[k];
          const d = parseTrackDump(await this.req(CMD.TRACK_DUMP, [k]), L.P_COUNT);
          const x = await this.readX(k);
          const changed = !before || before.eng !== d.eng || before.preset !== d.preset || d.p.some((v, i) => i !== ID.P_LEVEL && i !== ID.P_PAN && i !== ID.P_MUTE && v !== before.p[i]) || x.some((v, i) => v !== before.x?.[i]);
          const reloaded = byUnit && changed && before && (before.eng !== d.eng || before.preset !== d.preset || d.p.filter((v, i) => v !== before.p[i]).length + x.filter((v, i) => v !== before.x?.[i]).length > 6);
          this.parts[k] = { eng: d.eng, preset: d.preset, p: d.p, x, name: reloaded ? null : before?.name ?? null, slot: reloaded ? null : before?.slot ?? null, fromUnit: reloaded || (!changed && !!before?.fromUnit) };
          if (changed) this.emit("reload", { part: k });
        }
        this.g = parseDump(await this.req(CMD.DUMP), L.P_COUNT, L.G_COUNT).g; this.emit("globals", {});
      } while (this.readAgain);
    })();
    try { await this.reading; } finally { this.reading = null; }
  }
  // 0.2.25: a light check every few seconds (TRACK query: the selected part, each part's engine and preset): a change the unit did not push is read
  startPoll(ms = 2500) {
    clearInterval(this.pollTimer);
    this.pollTimer = setInterval(async () => {
      if (this.polling || this.reading || [...this.sending.values()].some(Boolean) || (typeof document !== "undefined" && document.hidden)) return;
      this.polling = true;
      try {
        const t = parseTrackQuery(await this.req(CMD.TRACK, [], 1500), this.ntrk);
        const sel = Math.min(t.selected, 1), diff = sel !== this.selected || [0, 1].some((k) => t.tracks[k] && this.parts[k] && (t.tracks[k].eng !== this.parts[k].eng || t.tracks[k].preset !== this.parts[k].preset));
        if (diff) { const was = this.selected; this.selected = sel; this.emit("selected", { part: sel, was }); await this.readBoth(true); }
        if (this.menu) await this.pollSettings();          // the MENU / device settings (WIDTH, BASS MONO, COMP ... the MODULES switches) and now and then the globals
      } catch { /* the next poll */ } finally { this.polling = false; }
    }, ms);
    this.pollTimer.unref?.();                          // (node: the tests end without a disconnect)
  }
  // 0.2.25 (owner: a WIDTH or BASS MONO changed on the unit did not show in the editor): the unit pushes song.g[] changes (CHANGED) but the MENU / device settings (WIDTH, BASS MONO,
  // COMP, SIDECHAIN, the MODULES switches and the MENU rows) only answer MENU_DESC. Every poll reads the ids 28..42 (the panel's own settings: one small request each) and a turn of
  // the others (POLL_REST of the MENU rows), and every POLL_G polls the global dump (a push lost, a value changed by a load). A setting the user is writing now (a write queued or in flight),
  // or any poll while a pointer is held down in the editor (isBusy, set by app.js: a slider being dragged), is left alone: the next poll reads it.
  async pollSettings() {
    if (this.isBusy?.()) return;
    const mine = (key) => this.sending.get(key) || this.latest.has(key);
    const hot = this.menu.filter((m) => m.id >= 28 && m.id <= 42), rest = this.menu.filter((m) => !(m.id >= 28 && m.id <= 42));
    const n = Math.min(Unit.POLL_REST, rest.length), turn = [];
    for (let i = 0; i < n; i++) turn.push(rest[((this.pollRest ?? 0) + i) % rest.length]);
    this.pollRest = rest.length ? ((this.pollRest ?? 0) + n) % rest.length : 0;
    let ch = false;
    for (const m of [...hot, ...turn]) {
      if (mine(`m:${m.id}`)) continue;
      const f = parseMenu(await this.req(CMD.MENU_DESC, [m.index], 1500));
      if (f && f.value !== m.value && !mine(`m:${m.id}`) && !this.isBusy?.()) { m.value = f.value; ch = true; }
    }
    if (ch) this.emit("menu", this.menu);
    if (((this.pollG = (this.pollG ?? 0) + 1) % Unit.POLL_G) === 0) {
      const g = parseDump(await this.req(CMD.DUMP, [], 2500), L.P_COUNT, L.G_COUNT).g;
      for (let id = 0; id < g.length; id++) {
        if (g[id] === this.g[id] || mine(`g:${id}`) || this.isBusy?.()) continue;
        this.g[id] = g[id]; this.emit("global", { id, v: g[id], byUs: false });
      }
    }
  }
  set(part, id, v, byUs) {
    const p = this.parts[part]; if (!p) return;
    p.p[id] = v; this.emit("param", { part, id, v, byUs });
  }
  setX(part, k, v, byUs) {
    const p = this.parts[part]; if (!p) return;
    (p.x ||= xDefaults())[k] = v; this.emit("param", { part, id: k, scope: 2, v, byUs });
  }

  // ---- parameter writes: the latest value of a parameter goes out when the one before is answered (a dragged slider sends what it needs)
  writeParam(part, id, v) {
    this.guard();
    const d = descOf(this.parts[part], id); v = clampTo(d, Math.round(v));
    const old = this.parts[part].p[id];
    this.set(part, id, v, true);
    if (!this.sending.get(`p${part}:${id}`)) (this.sentVal ||= new Map()).set(`p${part}:${id}`, old);   // (the slew starts from the value the unit has now)
    this.queueWrite(`p${part}:${id}`, v, async (val) => {
      const a = await this.req(CMD.TRACK_PARAM, [part, id, ...v14(val)]);
      const got = rv14(a, 2); if (got !== val && !this.latest.has(`p${part}:${id}`)) this.set(part, id, got, true);
      if (id === ID.P_TWTYPE && !this.latest.has(`p${part}:${id}`)) await this.readTwin(part);   // (a TYPE change moves CUTOFF 2 and RES 2 on entering or leaving COMB: the unit pushes only for the selected part)
    }, { slew: slewOf(d) });
  }
  // an extension slot (protocol scope 2); LINK also converts CUTOFF 2 (twin_link_set), so CUTOFF 2 is read back
  writeX(part, k, v) {
    this.guard();
    if (!this.hasX) throw new UnitError("This unit's firmware has no extension values (INFO tag 59 is missing).");
    const d = xdescOf(this.parts[part], k); v = clampTo(d, Math.round(v));
    this.setX(part, k, v, true);
    this.queueWrite(`x${part}:${k}`, v, async (val) => {
      const r = parseXReply(await this.req(CMD.TRACK_PARAM, xWriteArgs(part, k, val)));
      if (r.v !== val && !this.latest.has(`x${part}:${k}`)) this.setX(part, k, r.v, true);
      if (k === X.X_LINK && !this.latest.has(`x${part}:${k}`)) await this.readTwin(part);
    });
  }
  async readTwin(part) {                              // CUTOFF 2 and RES 2 of a part (they change meaning with LINK and TYPE)
    for (const id of [ID.P_CUT2, ID.P_RES2]) { const a = await this.req(CMD.TRACK_PARAM, [part, id]); this.set(part, id, rv14(a, 2), true); }
  }
  writeGlobal(id, v) {
    this.guard();
    v = clampTo(L.GP[id], Math.round(v)); this.g[id] = v; this.emit("global", { id, v, byUs: true });
    this.queueWrite(`g:${id}`, v, async (val) => {
      const a = await this.req(CMD.SET, [1, id, ...v14(val)]);
      const got = rv14(a, 2); if (got !== val) { this.g[id] = got; this.emit("global", { id, v: got, byUs: true }); }
    });
  }
  // 0.2.25 (owner: the cutoff crackled when it was dragged in the editor): at most one write of a parameter every MIN_GAP ms (about 50 a second; the
  // last value always goes out), and a value with a slew (opt.slew: the largest step a write may take, the filter cutoffs) walks to its target in
  // steps of that size instead of jumping: a fast drag reaches the unit as a quick, even sweep
  queueWrite(key, v, send, opt = {}) {
    this.latest.set(key, v);
    if (opt.slew) (this.slew ||= new Map()).set(key, opt.slew);
    if (this.sending.get(key)) return;
    this.sending.set(key, true);
    const sent = (this.sentAt ||= new Map()), last = (this.sentVal ||= new Map());
    (async () => {
      try {
        while (this.latest.has(key)) {
          const gap = Unit.MIN_GAP - (Date.now() - (sent.get(key) ?? 0));
          if (gap > 0) await new Promise((r) => setTimeout(r, gap));
          let val = this.latest.get(key);
          const st = this.slew?.get(key), prev = last.get(key);
          if (st && prev != null && Math.abs(val - prev) > st) val = prev + Math.sign(val - prev) * st;   // (the target stays in latest: the loop goes on)
          else this.latest.delete(key);
          sent.set(key, Date.now()); last.set(key, val);
          await send(val);
        }
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

  // INIT (the unit's SAVE > TOOLS > INIT SOUND, ui.c init_sound): the engine's first sound with every common setting at its default, then KEY TRACKING at
  // its new-sound default. PRESET (engine, 0) is exactly the first half (ui.c apply_preset_to); the steps, the mixer and the arp stay
  async initSound(part) {
    this.guard(); await this.select(part); await this.idle();
    const eng = L.ENG[this.parts[part].eng]?.name === "-" ? 0 : this.parts[part].eng;
    await this.req(CMD.PRESET, [eng, 0]);
    await this.req(CMD.TRACK_PARAM, [part, ID.P_KTRK, ...v14(L.TP[ID.P_KTRK].def)]);
    await this.readPart(part);
    this.parts[part].name = null;
    this.parts[part].slot = null;
  }
  // FM6: the part's own voice, the 128-byte packed record (FM6_GET / FM6_PUT target 0, index = the part). A PUT is heard at once and makes SLOT OWN.
  async readFm6(part) {
    const a = await this.req(CMD.FM6_GET, [0, part], 2000);
    if (a[2]) throw new UnitError("The unit did not give the FM6 voice of this part.", a[2]);
    return a.slice(3, 3 + 128);
  }
  writeFm6(part, packed) {
    this.guard();
    this.queueWrite(`fm6:${part}`, Array.from(packed), async (val) => {
      const a = await this.req(CMD.FM6_PUT, [0, part, ...val.map((x) => x & 127)], 2000);
      if (a[2]) throw new UnitError("The unit refused the FM6 voice.", a[2]);
      const s = await this.req(CMD.TRACK_PARAM, [part, P_E0 + 7]);                   // SLOT is OWN now (or F n when it is a factory voice unchanged)
      this.set(part, P_E0 + 7, rv14(s, 2), false);
    });
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
    this.parts[part].slot = slot;
  }
  // Saves the part's CURRENT sound over a slot (UP_STORE: the unit stores it itself, so End B and the arp block are kept; UP_PUT would drop them). What
  // the slot held is gone (a saved record, or nothing but its factory sound, which RESTORE brings back).
  async saveSlot(part, slot, name) {
    await this.select(part); await this.idle();
    const a = await this.req(CMD.UP_STORE, [slot, ...cstr(name)], 6000);
    if (a[1] === 1) throw new UnitError("The unit refused the name or the slot.", 1);
    if (a[1] === 2) throw new UnitError("The unit could not write: it could not stop playback in time, or the flash failed. Stop playback on the unit and try again.", 2);
    if (a[1] !== 0) throw new UnitError(`The unit answered error ${a[1]}.`, a[1]);
    if (this.parts[part]) this.parts[part].slot = slot;
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
