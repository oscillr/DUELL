// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the piano roll's model. No DOM, no I/O of its own (test_roll.mjs imports it).
//   - the step model as the unit has it (firmware core.h step_t, ui.c note_len / note_set_len, the device's own ROLL page): a step holds up to 4 notes, a TIME
//     (NOTE, TIE, REST), accent / slide flags, a velocity, a chance and a ratchet. A NOTE step's length is that step and the TIE steps after it (inside LEN, no wrap).
//     A TIE step carries no notes of its own: the notes of the step before it go on sounding. So a chord has ONE length (all its notes), and a velocity,
//     accent, slide, chance and ratchet belong to the STEP, not to a single note.
//   - pure edit functions: each takes the steps (an array of 64, never changed in place) and returns a Map step index -> new step (only the steps that change)
//   - PatternStore: one part's 64 steps as the editor holds them, the undo / redo history, the write queue to the unit ("send live"), copy / paste.
import { stepBytes } from "./proto.js";
import { ID } from "./layout.js";

export const NSTEP = 64, MAXN = 4, DEF_VEL = 100;
export const NOTE = 0, TIE = 1, REST = 2;
export const F_ACCENT = 1, F_SLIDE = 2;
export const AM_SEQ = 15;                 // P_AMODE's SEQ value: the steps are the arp's offset pattern (seq.c arp_seq), not a pattern that plays by itself

// the firmware's scale masks (seq.c SCALE_MASK; params.c N_SCALE names). SCALE and ROOT themselves cannot be read from the unit (no protocol command): the roll
// lets the player pick them for the HIGHLIGHT only; nothing is sent.
const bits = (...b) => b.reduce((m, x) => m | (1 << x), 0);
export const SCALES = [
  ["CHR", 0xfff], ["MAJ", bits(0, 2, 4, 5, 7, 9, 11)], ["MIN", bits(0, 2, 3, 5, 7, 8, 10)], ["DOR", bits(0, 2, 3, 5, 7, 9, 10)], ["MIX", bits(0, 2, 4, 5, 7, 9, 10)],
  ["PEN", bits(0, 2, 4, 7, 9)], ["MPEN", bits(0, 3, 5, 7, 10)], ["HARM", bits(0, 2, 3, 5, 7, 8, 11)], ["PHRY", bits(0, 1, 3, 5, 7, 8, 10)], ["LYD", bits(0, 2, 4, 6, 7, 9, 11)],
  ["LOC", bits(0, 1, 3, 5, 6, 8, 10)], ["MEL", bits(0, 2, 3, 5, 7, 9, 11)], ["BLUES", bits(0, 3, 5, 6, 7, 10)], ["WHOLE", bits(0, 2, 4, 6, 8, 10)],
  ["DIMHW", bits(0, 1, 3, 4, 6, 7, 9, 10)], ["DIMWH", bits(0, 2, 3, 5, 6, 8, 9, 11)],
];
export const inScale = (pitch, root, scale) => ((SCALES[scale]?.[1] ?? 0xfff) >> (((pitch - root) % 12 + 12) % 12)) & 1;
export const isBlackKey = (pitch) => ((0x54a >> (pitch % 12)) & 1) === 1;   // C#, D#, F#, G#, A# (pitch classes 1 3 6 8 10)

// ---- steps
export const blank = (i) => ({ index: i, n: 0, notes: [0, 0, 0, 0], time: REST, flags: 0, vel: 0, hits: [0, 0, 0], chance: 100, ratchet: 1, nudge: 8 });   // ui.c step_clear
export const clone = (s) => ({ ...s, notes: s.notes.slice(0, 4), hits: s.hits.slice(0, 3) });
export const key = (s) => stepBytes(s).join(",");
export const same = (a, b) => key(a) === key(b);
export const isOn = (s) => s.time === NOTE && s.n > 0;
export const notesOf = (s) => (s.n > 0 ? s.notes.slice(0, Math.min(s.n, MAXN)) : []);
// a NOTE step's length: it and the TIE steps after it, inside len (ui.c note_len)
export function tailOf(steps, len, i) { let n = 1; while (i + n < len && steps[i + n].time === TIE) n++; return n; }
// the NOTE step that owns step i (itself, or the NOTE before the TIE run i is in); -1: none (an orphan TIE, a REST)
export function ownerOf(steps, len, i) {
  if (i < 0 || i >= len) return -1;
  if (isOn(steps[i])) return i;
  if (steps[i].time !== TIE) return -1;
  let j = i; while (j > 0 && steps[j].time === TIE) j--;
  return isOn(steps[j]) ? j : -1;
}
// every note bar: { start, k (index in the step), pitch, len }
export function bars(steps, len) {
  const out = [];
  for (let i = 0; i < len; i++) if (isOn(steps[i])) { const L = tailOf(steps, len, i); notesOf(steps[i]).forEach((pitch, k) => out.push({ start: i, k, pitch, len: L })); }
  return out;
}
export function barAt(steps, len, step, pitch) {
  const o = ownerOf(steps, len, step);
  if (o < 0) return null;
  const k = notesOf(steps[o]).indexOf(pitch);
  return k < 0 ? null : { start: o, k, pitch, len: tailOf(steps, len, o) };
}
// orphan TIEs (no NOTE before them): they play nothing; the unit's own editor leaves them, the roll marks them
export function orphans(steps, len) { const out = []; for (let i = 0; i < len; i++) if (steps[i].time === TIE && ownerOf(steps, len, i) < 0) out.push(i); return out; }

const mapOf = () => new Map();
const put = (m, steps, i, s) => { if (!same(steps[i], s)) m.set(i, s); else m.delete(i); };
export const apply = (steps, ch) => { const o = steps.slice(); for (const [i, s] of ch) o[i] = s; return o; };

// ---- edit functions. opt.maxNotes: 4 (a pattern that plays by itself) or 1 (the SEQ arp: only note 0 of a step is used, seq.c arp_seq_notes)
// A note at (i, pitch). A step with notes takes it as one more (up to 4, a full step says so); a TIE step in the middle of another note cuts that note (its
// remaining tied steps become rests: a TIE has no notes of its own, the unit cannot keep them as a second note); a REST / empty step becomes a NOTE.
export function addNote(steps, len, i, pitch, opt = {}) {
  const out = mapOf(), maxN = opt.maxNotes ?? MAXN;
  if (i < 0 || i >= len || pitch < 0 || pitch > 127) return out;
  const s = steps[i];
  if (isOn(s)) {
    if (notesOf(s).includes(pitch)) return out;
    const c = clone(s);
    if (maxN === 1) { c.notes[0] = pitch; c.n = 1; }
    else if (s.n >= maxN) { out.msg = `A step holds at most ${maxN} notes.`; return out; }
    else c.notes[c.n++] = pitch;
    put(out, steps, i, c); return out;
  }
  if (s.time === TIE) for (let j = i; j < len && steps[j].time === TIE; j++) out.set(j, blank(j));
  out.set(i, { ...blank(i), time: NOTE, n: 1, notes: [pitch, 0, 0, 0], vel: opt.vel ?? DEF_VEL });
  return out;
}
// the note leaves its step; the last note of a step takes the whole step and its tied steps with it
export function removeNote(steps, len, start, pitch) {
  const out = mapOf(), s = steps[start];
  if (!s || !isOn(s)) return out;
  const ns = notesOf(s), k = ns.indexOf(pitch);
  if (k < 0) return out;
  if (ns.length === 1) { const L = tailOf(steps, len, start); for (let j = 0; j < L; j++) out.set(start + j, blank(start + j)); return out; }
  const c = clone(s); ns.splice(k, 1); c.notes = [...ns, 0, 0, 0, 0].slice(0, 4); c.n = ns.length;
  out.set(start, c); return out;
}
export function deleteAt(steps, len, step, pitch) {
  const b = barAt(steps, len, step, pitch);
  return b ? removeNote(steps, len, b.start, pitch) : mapOf();
}
// the note (start, pitch) goes to (toStep, toPitch). Same step: its pitch changes. Another step: it keeps its step's velocity / accent / slide / chance / ratchet and
// its length (as far as there is room); landing on a step that has notes it joins them (and takes that step's length and properties).
export function moveNote(steps, len, start, pitch, toStep, toPitch, opt = {}) {
  const out = mapOf(), maxN = opt.maxNotes ?? MAXN, s = steps[start];
  toStep = Math.max(0, Math.min(len - 1, toStep)); toPitch = Math.max(0, Math.min(127, toPitch));
  if (!s || !isOn(s)) return out;
  const ns = notesOf(s), k = ns.indexOf(pitch);
  if (k < 0 || (toStep === start && toPitch === pitch)) return out;
  if (toStep === start) {
    const c = clone(s); const nn = ns.slice(); nn[k] = toPitch;
    const u = nn.filter((x, j) => nn.indexOf(x) === j);                       // moved onto a note of the step: they merge
    c.notes = [...u, 0, 0, 0, 0].slice(0, 4); c.n = u.length;
    put(out, steps, start, c); return out;
  }
  const work = new Map(), get = (j) => work.get(j) ?? steps[j], L = tailOf(steps, len, start);
  const props = { vel: s.vel || DEF_VEL, flags: s.flags, chance: s.chance, ratchet: s.ratchet, nudge: s.nudge, hits: s.hits.slice() };
  if (ns.length === 1) for (let j = 0; j < L; j++) work.set(start + j, blank(start + j));
  else { const c = clone(s); ns.splice(k, 1); c.notes = [...ns, 0, 0, 0, 0].slice(0, 4); c.n = ns.length; work.set(start, c); }
  const t = get(toStep);
  if (isOn(t)) {
    const c = clone(t);
    if (notesOf(t).includes(toPitch)) { /* merges into the same note */ }
    else if (maxN === 1) { c.notes[0] = toPitch; c.n = 1; }
    else if (t.n >= maxN) { out.msg = `A step holds at most ${maxN} notes.`; return out; }
    else c.notes[c.n++] = toPitch;
    work.set(toStep, c);
  } else {
    if (t.time === TIE) for (let j = toStep; j < len && get(j).time === TIE; j++) work.set(j, blank(j));
    work.set(toStep, { ...blank(toStep), ...props, index: toStep, time: NOTE, n: 1, notes: [toPitch, 0, 0, 0] });
    for (let q = 1; q < L; q++) { const j = toStep + q; if (j >= len || get(j).n > 0 || get(j).time === TIE) break; work.set(j, { ...blank(j), time: TIE }); }
  }
  for (const [j, v] of work) put(out, steps, j, v);
  return out;
}
// a note's length (ui.c note_set_len): longer, the steps after it become TIEs (a REST or an empty step; it stops at the next step that holds notes, and at LEN);
// shorter, its last TIEs become RESTs. Returns the changes; the length it gets is tailOf(apply(...))
export function resize(steps, len, start, want) {
  const out = mapOf();
  if (start < 0 || start >= len || !isOn(steps[start])) return out;
  want = Math.max(1, Math.min(len - start, want));
  let n = tailOf(steps, len, start);
  const get = (j) => out.get(j) ?? steps[j];
  for (; n > want && n > 1; n--) out.set(start + n - 1, blank(start + n - 1));
  for (; n < want && start + n < len && !get(start + n).n; n++) out.set(start + n, { ...blank(start + n), time: TIE });
  for (const [j, v] of [...out]) put(out, steps, j, v);
  return out;
}
// step-level properties of a step that has notes: vel, chance, ratchet, flags (accent, slide)
export function setProps(steps, len, i, patch) {
  const out = mapOf(), s = steps[i];
  if (i < 0 || i >= len || !s || !isOn(s)) return out;
  const c = clone(s);
  if (patch.vel != null) c.vel = Math.max(1, Math.min(127, Math.round(patch.vel)));
  if (patch.chance != null) c.chance = Math.max(1, Math.min(100, Math.round(patch.chance)));
  if (patch.ratchet != null) c.ratchet = Math.max(1, Math.min(4, Math.round(patch.ratchet)));
  if (patch.flags != null) c.flags = patch.flags & 3;
  put(out, steps, i, c); return out;
}
export function toggleFlag(steps, len, i, bit) { const s = steps[i]; return s && isOn(s) ? setProps(steps, len, i, { flags: s.flags ^ bit }) : mapOf(); }
export function clearPattern(steps, len) { const out = mapOf(); for (let i = 0; i < len; i++) if (!same(steps[i], blank(i)) && (steps[i].n || steps[i].time !== REST || steps[i].flags || steps[i].vel)) out.set(i, blank(i)); return out; }
// paste the first min(len, clip) steps; a TIE the paste leaves without a note before it becomes a rest
export function pasteClip(steps, len, clip) {
  const out = mapOf(), n = Math.min(len, clip.length), now = steps.slice();
  for (let i = 0; i < n; i++) now[i] = { ...clone(clip[i]), index: i };
  for (let i = 0; i < len; i++) if (now[i].time === TIE && ownerOf(now, len, i) < 0) now[i] = blank(i);
  for (let i = 0; i < len; i++) put(out, steps, i, now[i]);
  return out;
}
// the clipboard is shared by both parts (copy on one, paste on the other)
export const clipboard = { steps: null };

// SEQ / ARP state of a part from the MODULES switches and the part's arp MODE: what the unit does with the steps
//   arpSeq: P_AMODE is SEQ: the steps are the arp's offsets on the held keys (note 0 - 60 semitones; a rest / tie / empty step is a rest; accent = 127, else 100); they do
//           not play by themselves, the arp's own RATE / GATE time them, and TIE, SLIDE, chance, ratchet, velocity and notes 2..4 do nothing
//   seqOff: MODULES > SEQ OFF: no step plays (editing still works)   arpOff: MODULES > ARP OFF: with MODE SEQ the pattern does not play either
export function seqState(unit, part) {
  const p = unit.parts[part]?.p ?? [], menu = unit.menu ?? [];
  const off = (id) => { const m = menu.find((x) => x.id === id); return !!m && m.names ? m.names[m.value - m.min] === "OFF" : false; };
  return { arpSeq: p[ID.P_AMODE] === AM_SEQ, arpOff: off(28), seqOff: off(29) };
}

// ---- the pattern store of one part
const rd = (store, e) => { for (const f of [...store.subs]) f(e); };
export class PatternStore {
  constructor(unit, part) {
    this.unit = unit; this.part = part; this.steps = Array.from({ length: NSTEP }, (_, i) => blank(i));
    this.loaded = false; this.stale = false; this.everLoaded = false; this.loading = null; this.live = true; this.dirty = new Set(); this.undoStack = []; this.redoStack = []; this.base = null;
    this.subs = new Set(); this.flushing = null; this.error = null; this.written = 0; this.views = 0;
  }
  get len() { return Math.max(1, Math.min(NSTEP, this.unit.parts[this.part]?.p[ID.P_SLEN] | 0 || 1)); }
  sub(f) { this.subs.add(f); return () => this.subs.delete(f); }
  emit(type, d = {}) { rd(this, { type, ...d }); }
  // ---- reading
  async load() {
    if (this.loading) return this.loading;
    this.loading = (async () => {
      try {
        const steps = await this.unit.readSteps(this.part, NSTEP);
        const changed = this.everLoaded && steps.some((s, i) => !same(s, this.steps[i]));
        if (this.base) return;                                                        // a gesture is running: the next load shows it
        for (const i of this.dirty) steps[i] = this.steps[i];                         // not yet sent edits win
        this.steps = steps; this.loaded = this.everLoaded = true; this.stale = false; this.error = null;
        if (changed) { this.undoStack = []; this.redoStack = []; }                   // the pattern changed under us: the old history no longer fits
        this.emit("load");
      } catch (e) { this.error = e; this.emit("error", { error: e }); }
      finally { this.loading = null; }
    })();
    return this.loading;
  }
  invalidate() { this.stale = true; if (this.views > 0) this.load(); }
  // the unit says step i changed (the device was edited); our own writes are not echoed
  async external(i) {
    if (!this.loaded || this.base || this.dirty.has(i) || i >= NSTEP) return;
    try { const s = await this.unit.readStep(this.part, i); if (!this.dirty.has(i) && !this.base) { this.steps[i] = s; this.emit("sync", { index: i }); } } catch { /* the next full read shows it */ }
  }
  // ---- editing: a gesture is begin / preview / commit; a one-shot edit is apply()
  begin() { if (!this.base) this.base = this.steps.slice(); }
  preview(ch) { if (!this.base) this.begin(); this.steps = apply(this.base, ch); this.emit("change", { preview: true }); }
  cancel() { if (!this.base) return; this.steps = this.base; this.base = null; this.emit("change", { cancel: true }); }
  commit(label) {
    const base = this.base; if (!base) return false;
    this.base = null;
    if (!this.unit.writable) {                                                         // a unit this editor does not know (INFO differs): nothing is written, so nothing is edited
      this.steps = base; this.emit("change", { cancel: true }); this.emit("error", { error: new Error(this.unit.compat || "The unit is not connected.") }); return false;
    }
    const ch = []; this.steps.forEach((s, i) => { if (!same(s, base[i])) ch.push({ i, before: base[i], after: s }); });
    if (!ch.length) { this.emit("change", {}); return false; }
    this.undoStack.push({ label, ch }); if (this.undoStack.length > 200) this.undoStack.shift(); this.redoStack = [];
    for (const c of ch) this.dirty.add(c.i);
    this.emit("change", { label }); this.afterEdit();
    return true;
  }
  edit(label, ch) { if (!ch || !ch.size) return false; this.begin(); this.preview(ch); return this.commit(label); }
  afterEdit() { this.emit("state"); if (this.live) this.flush(); }
  get canUndo() { return this.undoStack.length > 0; }
  get canRedo() { return this.redoStack.length > 0; }
  undo() { return this.travel(this.undoStack, this.redoStack, "before", "undo"); }
  redo() { return this.travel(this.redoStack, this.undoStack, "after", "redo"); }
  travel(from, to, side, what) {
    const e = from.pop(); if (!e) return null;
    const o = this.steps.slice(); for (const c of e.ch) { o[c.i] = c[side]; this.dirty.add(c.i); }
    this.steps = o; to.push(e); this.emit("change", { [what]: e.label, indexes: e.ch.map((c) => c.i) }); this.afterEdit();
    return e;
  }
  copy() { clipboard.steps = this.steps.slice(0, this.len).map(clone); return clipboard.steps.length; }
  paste() { return clipboard.steps ? this.edit("paste", pasteClip(this.steps, this.len, clipboard.steps)) : false; }
  clear() { return this.edit("clear", clearPattern(this.steps, this.len)); }
  // ---- writing ("send live": each edit goes out at once; off: they wait for send())
  setLive(on) { this.live = !!on; this.emit("state"); if (this.live && this.dirty.size) this.flush(); }
  get pending() { return this.dirty.size; }
  flush() {
    if (this.flushing) return this.flushing;
    this.flushing = (async () => {
      try {
        while (this.dirty.size) {
          const i = Math.min(...this.dirty), snap = this.steps[i], k = key(snap);
          this.dirty.delete(i);
          try {
            const r = await this.unit.writeStep(this.part, i, snap); this.written++;
            if (this.steps[i] && key(this.steps[i]) === k && !same(r, snap) && !this.base) { this.steps[i] = r; this.emit("sync", { index: i }); }   // the unit's own reading wins
          } catch (e) { this.dirty.add(i); this.error = e; this.emit("error", { error: e }); break; }
        }
      } finally { this.flushing = null; this.emit("state"); }
    })();
    return this.flushing;
  }
  send() { return this.flush(); }
  async idle() { while (this.flushing) await this.flushing; }
  async revert() { this.dirty.clear(); this.undoStack = []; this.redoStack = []; this.loaded = false; this.emit("state"); await this.load(); }
}
