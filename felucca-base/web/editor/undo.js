// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: UNDO of parameter edits (0.2.25, owner). Every edit made in the editor is recorded as (where, id, old value, new value); a drag of
// one control is ONE entry (edits of the same control within MERGE_MS join the entry before). A card's RESET and a BYPASS toggle are one entry of
// several values. UNDO sends the old values back to the unit. Loads, saves and the piano roll (it has its own undo) are not recorded.
// where: 0 / 1 a part, "g" a global, "m" a MENU setting. No DOM, no I/O: the writer is handed in (test_editor.mjs runs it against the mock unit).
export const MERGE_MS = 700, MAX = 100;

export class UndoStack {
  constructor() { this.list = []; this.subs = new Set(); }
  sub(f) { this.subs.add(f); return () => this.subs.delete(f); }
  emit() { for (const f of this.subs) f(this); }
  // one value changed by the user
  record(where, id, oldV, newV, now = Date.now()) {
    if (oldV === newV || oldV == null) return;
    const top = this.list.at(-1);
    if (!this.noMerge && top && top.items.length === 1 && top.items[0].where === where && top.items[0].id === id && now - top.t < MERGE_MS && !top.group) {
      top.items[0].newV = newV; top.t = now;
      if (top.items[0].newV === top.items[0].oldV) this.list.pop();   // dragged back to where it was: nothing to undo
    } else this.push({ items: [{ where, id, oldV, newV }], t: now, label: null });
    this.emit();
  }
  // several values as one step (a RESET of a card, a BYPASS): items [{where, id, oldV, newV}]
  group(label, items, now = Date.now()) {
    const it = items.filter((x) => x.oldV !== x.newV && x.oldV != null);
    if (!it.length) return;
    this.push({ items: it, t: now, label, group: true }); this.emit();
  }
  push(e) { this.list.push(e); if (this.list.length > MAX) this.list.shift(); }
  // the last entry (of part `where` when given: 0 / 1; null: the newest of all)
  last(where = null) { for (let i = this.list.length - 1; i >= 0; i--) if (where == null || this.list[i].items.some((x) => x.where === where)) return i; return -1; }
  canUndo(where = null) { return this.last(where) >= 0; }
  // takes the entry out and writes its old values with write(where, id, value); returns the entry (null: nothing to undo)
  async undo(write, where = null) {
    const i = this.last(where); if (i < 0) return null;
    const [e] = this.list.splice(i, 1);
    for (const x of [...e.items].reverse()) await write(x.where, x.id, x.oldV);
    this.emit();
    return e;
  }
  clear() { this.list = []; this.emit(); }
  // a load replaced a part (or everything: where null): its entries no longer fit
  forget(where = null) { this.list = this.list.filter((e) => where != null && !e.items.some((x) => x.where === where)); this.emit(); }
}
