// SPDX-License-Identifier: GPL-3.0-only
// DUELL 1.1 editor: the PARAMETER LOCKS tab (owner, 2026-10-25, HANDOFF_V2 item 69; the sequencer's page of the unit, SEQ > PARAMETER LOCKS, firmware ui_events.c). The selected part's records, one row each:
// LOCK (the value sounds on its step only) or AUTO (it holds until the next event), the step, the destination in the matrix's words, the value (an amount in percent for PITCH, TIMBRE, VOICE AMP, the bus
// values and SPACE; the destination's own value for the others). A row's step and value change like every other value of the editor (slider, or click and type); DELETE removes it; the last row picks a step
// and a destination and adds a LOCK at the sound's own value (the unit's "+ ADD LOCK"). The records are the unit's (MOTION, ops 3..11 through unit.js); nothing is stored by the editor.
import { h, clear } from "./ui_util.js";
import { Cell } from "./controls.js";
import { ID, L } from "./layout.js";
import { MATRIX_GROUPS } from "./labels.js";
import { dstOfTarget, targetOfDst, orderOfTarget, offeredDsts, dstName, dstNameInGroup, valueDesc, ownValue, stepDesc } from "./locks.js";

const POLL_MS = 2500;
export function createLocksView(app, p, { say }) {
  const u = () => app.unit, part = () => u().parts[p], g = () => u().g;
  const S = { rows: [], max: 128, loaded: false, running: false, pending: new Set(), holding: false, sig: "" };
  const add = { step: 1, dst: 0 };
  let cells = [], addCells = [], dead = false;
  const lenOf = () => Math.max(1, Math.min(64, part().p[ID.P_SLEN] | 0 || 16));
  const sigOf = (rows) => rows.map((r) => `${r.step}.${r.tid}.${r.lock ? 1 : 0}.${r.v}`).join();
  const sorted = (rows) => rows.slice().sort((a, b) => a.step - b.step || orderOfTarget(a.tid) - orderOfTarget(b.tid) || a.tid - b.tid);
  const idle = async () => { while (S.running) await new Promise((r) => setTimeout(r, 15)); };
  const fail = (e) => { say(e.message || String(e), "warn"); };

  // ---- the add row's controls: STEP (1 .. LEN) and the destination picker (the matrix's groups; only what the unit offers now)
  const dstDesc = { label: "DESTINATION", fmt: 8, min: 0, max: 56, def: 0, fk: -1, unit: "", names: Array.from({ length: 57 }, (_, i) => String(i)), order: null };
  const offered = () => offeredDsts(part(), g(), u().info.lockWide);
  const addStep = new Cell({ desc: () => stepDesc(lenOf()), value: () => Math.min(add.step, lenOf()), set: (v) => { add.step = v; refresh(); }, inert: () => null }, { bare: true });
  const addDst = new Cell({
    desc: () => ({ ...dstDesc, order: offered() }), value: () => add.dst, set: (v) => { add.dst = v; refresh(); }, inert: () => null,
    optLabel: (v) => dstNameInGroup(part(), v, g()), groups: MATRIX_GROUPS.dst,
  }, { bare: true });
  addDst.src.desc = (() => { let last = "", d = null; return () => { const o = offered(), k = o.join(); if (k !== last) { last = k; d = { ...dstDesc, order: o }; } return d; }; })();
  addCells = [addStep, addDst];

  // ---- the list
  const list = h("div", { class: "llist", role: "list", "aria-label": `Part ${p + 1} parameter locks` });
  const none = h("p", { class: "lnone", text: "No rows yet. Pick a step and a destination below, then press + ADD LOCK." });
  const km = (n) => h("span", { class: "km", text: String(n) });
  const head = h("div", { class: "lrow lhead", "aria-hidden": "true" }, h("span", {}), h("span", {}, km(2), "STEP"), h("span", {}, km(3), "DESTINATION"), h("span", {}, km(4), "AMOUNT"), h("span", {}));
  const addBtn = h("button", { type: "button", class: "ladd", text: "+ ADD LOCK", onclick: () => addRow() });
  const addRowEl = h("div", { class: "lrow ladding" }, h("span", {}), addStep.el, h("div", { class: "ldst" }, addDst.el), addBtn);
  const el = h("section", { class: "card lockscard", "aria-label": `Part ${p + 1} parameter locks` },
    h("h3", {}, h("span", { class: "ttl", text: "PARAMETER LOCKS", title: "The steps of this part's pattern that set a value. A LOCK sounds on its step only; an AUTO event holds its value until the next one. The destinations are the matrix's. The unit holds the records: the same list is on the unit under SEQ > PARAMETER LOCKS." })),
    head, list, none, addRowEl);

  function rowEl(r) {
    const key = `${r.step}:${r.tid}`;
    const stepC = new Cell({ desc: () => stepDesc(Math.max(lenOf(), r.step + 1)), value: () => r.step + 1, set: (v) => { r.newStep = v - 1; stepC.refresh(); queueMove(r); }, inert: () => null }, { bare: true });
    const valC = new Cell({ desc: () => valueDesc(part(), r.tid), value: () => r.v, set: (v) => { r.v = v; S.pending.add(r); pump(); valC.refresh(); }, inert: () => null }, { bare: true, soft: true });
    // the DESTINATION is changeable (owner, 2026-10-25, item 85): the row is put on the new destination at the same step with that destination's own value, the old one deleted
    const dstC = new Cell({
      desc: () => ({ ...dstDesc, order: orderWith(r) }), value: () => dstOfTarget(r.tid), set: (v) => { r.newDst = v; queueDst(r); }, inert: () => null,
      optLabel: (v) => dstNameInGroup(part(), v, g()), groups: MATRIX_GROUPS.dst,
    }, { bare: true });
    dstC.src.desc = (() => { let last = "", d = null; return () => { const o = orderWith(r), k = o.join(); if (k !== last) { last = k; d = { ...dstDesc, order: o }; } return d; }; })();
    const name = h("div", { class: "ldst" }, dstC.el);
    const upd = () => { dstC.refresh(); };
    cells.push({ refresh: () => { upd(); stepC.refresh(); valC.refresh(); kind.textContent = r.lock ? "LOCK" : "AUTO"; kind.className = "lkind" + (r.lock ? " lock" : ""); } });
    // the KIND switches like OCT + on the unit: LOCK (this step only) <-> AUTO (holds until the next event)
    const kind = h("button", { type: "button", class: "lkind" + (r.lock ? " lock" : ""), text: r.lock ? "LOCK" : "AUTO", title: "LOCK: the value sounds on this step only. AUTO: the value holds until the next event. Press to switch.", onclick: () => switchKind(r) });
    const del = h("button", { type: "button", class: "ghost small ldel", text: "DELETE", "aria-label": `Delete ${r.lock ? "LOCK" : "AUTO"} step ${r.step + 1}`, onclick: () => removeRow(r) });
    const e = h("div", { class: "lrow", role: "listitem", dataset: { key } },
      kind, stepC.el, name, valC.el, del);
    e.addEventListener("pointerdown", () => { S.holding = true; const up = () => { S.holding = false; window.removeEventListener("pointerup", up); if (r.newStep != null) commitMove(r); }; window.addEventListener("pointerup", up); });
    r.el = e; r.cells = [stepC, valC];
    stepC.refresh(); valC.refresh(); upd();
    return e;
  }
  // the destinations a row can take: what the unit offers now, and its own
  const orderWith = (r) => { const o = offered(), cur = dstOfTarget(r.tid); return o.includes(cur) ? o : [cur, ...o]; };
  function queueDst(r) { clearTimeout(r.dt); r.dt = setTimeout(() => commitDst(r), 150); }
  async function commitDst(r) {
    const to = r.newDst; r.newDst = null;
    const tid = to == null ? 0 : targetOfDst(to);
    if (!tid || tid === r.tid || !S.rows.includes(r)) { refresh(); return; }
    if (S.rows.some((o) => o !== r && o.step === r.step && o.tid === tid)) { say(`STEP ${String(r.step + 1).padStart(2, "0")} already holds ${dstName(part(), to, g())}.`, "warn"); refresh(); return; }
    S.moving = true;
    try {
      await idle(); S.pending.delete(r);
      await u().deleteLock(p, r.step, r.tid);
      try { await u().putLock(p, r.step, tid, ownValue(part(), tid), r.lock); }
      catch (e) { await u().putLock(p, r.step, r.tid, r.v, r.lock).catch(() => {}); throw e; }
    } catch (e) { fail(e); }
    S.moving = false;
    await load(true); render();
  }
  async function switchKind(r) {
    if (!S.rows.includes(r) || !u().writable) return;
    S.moving = true;
    try { await idle(); S.pending.delete(r); await u().putLock(p, r.step, r.tid, r.v, !r.lock); r.lock = !r.lock; } catch (e) { fail(e); }
    S.moving = false;
    await load(true); render();
  }
  function render() {
    cells = []; clear(list);
    S.rows = sorted(S.rows);
    for (const r of S.rows) list.append(rowEl(r));
    S.sig = sigOf(S.rows);
    refresh();
  }
  function refresh() {
    if (dead) return;
    const o = offered();
    if (!o.includes(add.dst)) add.dst = o[0] ?? 0;
    for (const c of cells) c.refresh();
    for (const c of addCells) c.refresh();
    none.hidden = !S.loaded || S.rows.length > 0;
    addBtn.disabled = !add.dst || !u().writable;
  }
  // ---- reading
  async function load(force = false) {
    if (dead || !u()?.hasLocks) return;
    if (!force && (S.running || S.pending.size || S.holding || S.moving || list.querySelector(".valin"))) return;
    try {
      const r = await u().readLocks(p);
      if (S.running || S.pending.size || S.holding || S.moving) return;
      S.max = r.max; S.loaded = true;
      const rows = sorted(r.rows);
      if (sigOf(rows) !== S.sig || !S.loaded) { S.rows = rows; render(); } else { rows.forEach((n, i) => { S.rows[i].v = n.v; }); refresh(); }
    } catch (e) { if (!S.warned) { S.warned = true; fail(e); } }
  }
  // ---- writing: a value goes out at the pace of the link, always the latest (a slider sends many)
  async function pump() {
    if (S.running) return;
    S.running = true;
    try {
      while (S.pending.size) {
        const r = S.pending.values().next().value; S.pending.delete(r);
        if (!S.rows.includes(r)) continue;
        await u().putLock(p, r.step, r.tid, r.v, r.lock);
      }
    } catch (e) { S.pending.clear(); fail(e); S.running = false; await load(true); return; }
    S.running = false;
  }
  // a step moves: the record is put on the new step, the old one deleted (the unit has no move). A step that already holds this destination is refused.
  function queueMove(r) { clearTimeout(r.mt); r.mt = setTimeout(() => { if (!S.holding) commitMove(r); }, 450); }
  async function commitMove(r) {
    clearTimeout(r.mt);
    const to = r.newStep; r.newStep = null;
    if (to == null || to === r.step || !S.rows.includes(r)) { refresh(); return; }
    if (S.rows.some((o) => o !== r && o.step === to && o.tid === r.tid)) { say(`STEP ${String(to + 1).padStart(2, "0")} already holds ${dstName(part(), dstOfTarget(r.tid), g())}.`, "warn"); refresh(); return; }
    S.moving = true;
    try {
      await idle();
      await u().deleteLock(p, r.step, r.tid);
      try { await u().putLock(p, to, r.tid, r.v, r.lock); r.step = to; }
      catch (e) { await u().putLock(p, r.step, r.tid, r.v, r.lock).catch(() => {}); throw e; }
    } catch (e) { fail(e); }
    S.moving = false;
    await load(true); render();
  }
  async function removeRow(r) {
    S.moving = true;
    try { await idle(); S.pending.delete(r); await u().deleteLock(p, r.step, r.tid); S.rows = S.rows.filter((o) => o !== r); } catch (e) { fail(e); }
    S.moving = false;
    await load(true); render();
  }
  async function addRow() {
    const step = Math.min(add.step, lenOf()) - 1, dst = add.dst, tid = targetOfDst(dst);
    if (!tid) return;
    if (S.rows.some((r) => r.step === step && r.tid === tid)) { say(`STEP ${String(step + 1).padStart(2, "0")} already holds ${dstName(part(), dst, g())}.`, "warn"); return; }
    S.moving = true;
    let added = false;
    try { await idle(); await u().putLock(p, step, tid, ownValue(part(), tid), true); added = true; } catch (e) { fail(e); }
    S.moving = false;
    await load(true); render();
    if (added) list.querySelector(`[data-key="${step}:${tid}"]`)?.scrollIntoView({ block: "nearest" });
  }
  // ---- the poll: the unit does not push records; read again while the tab is shown (EDITOR_PROTOCOL.md, MOTION), and after a load
  const timer = setInterval(() => { if (!el.isConnected) { dead = true; clearInterval(timer); u()?.off("reload", onReload); return; } load(); }, POLL_MS);
  const onReload = (d) => { if (!dead && (d.part === p || d.part === -1)) load(true); };
  u().on("reload", onReload);
  render(); load(true);
  return { el, refresh };
}
