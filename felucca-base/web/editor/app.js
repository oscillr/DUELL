// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the page. A fixed full-window layout of three columns: PART 1 | PART 2 | GLOBAL (GLOBAL always on the right), each with its own tab strip.
// Below 1500 px the GLOBAL column is a view of its own (PARTS / GLOBAL). T0229 (owner): the page never scrolls: it fits 1920 x 1080, 1440 x 900 and 1280 x 720,
// below 1260 x 600 inside the browser the whole page is scaled down (fitPage); no notes on the page (tooltips only), no pictures, a control that does not apply is not drawn.
// The pages are made from layout.json (the firmware's own tables), the values come from the unit (GET / SET / WATCH) or from the simulated unit.
import { h, clear, ask } from "./ui_util.js";
import { Cell } from "./controls.js";
import { xdescOf, X, twinWhy, twinX, twinComb, twinTypeOffered, twinFlow } from "./layout.js";
import { L, ID, P_E0, valueText, PART_TABS, GLOBAL_TABS, partTabs, SEQ_PARAMS, descOf, gdescOf, ginert, dmdAfterMode, inertReason, dstWhy, dstOffered, dlyMode, pageByTitle, pageVisible, pageControls, matrixSlots, engineLabel, pickable, unusedLabel } from "./layout.js";
import { Unit } from "./unit.js";
import { Link } from "./link.js";
import { themeVars, isDark, paletteName } from "./theme.js";
import { CMD, parseMenu } from "./proto.js";
import { buildPatches, buildMultis, buildPatterns, buildSettings, buildMidi, buildWavetables } from "./panels.js";
import { handleFile } from "./patches_ui.js";
import { dropZone, guardPage, hasFiles, hasSlot, slotOf, filesOf } from "./dropzone.js";
import { PatternStore } from "./roll.js";
import { createRollView } from "./rollview.js";
import { sysexGranted, watchUnit, HINT } from "./autoconnect.js";
import { longDesc, tipOf, whyText, TAB, ENGINE_TITLES, ENGINE_CARD, CATEGORY_NAMES, MATRIX_GROUPS, VALUE_NAMES, INSERT_LONG } from "./labels.js";
import { voiceWhy, modulesWhy } from "./layout.js";
import { UndoStack } from "./undo.js";
import { Bypass, PART_SECTIONS, BUS_SECTIONS, MASTER_SECTIONS } from "./bypass.js";
import { createFm6Editor } from "./fm6edit.js";
import * as WTM from "./wavetable.js";

const params = new URLSearchParams(location.search), MOCK = params.get("mock");
const $ = (id) => document.getElementById(id);
const app = { unit: null, tabs: ["engine", "engine"], gtab: "global", view: "parts", cells: [], sig: [], cols: [], mock: null, midi: null, refreshers: new Set(), target: 0, pal: null, stores: [], rollViews: [], overlay: null,
  undo: new UndoStack(), fm6: [null, null], opsOpen: [false, false], bypass: null };
window.__app = app;

// ---- messages
let msgTimer;
export function say(text, kind = "") {
  const e = $("msg"); e.textContent = text; e.className = "msg " + kind; clearTimeout(msgTimer);
  if (kind === "good") msgTimer = setTimeout(() => { if (e.textContent === text) e.textContent = ""; }, 6000);
}
app.say = say;
app.soundName = (p) => soundName(p);
app.fail = (e) => say(e.message || String(e), "warn");
// a write that reports its own errors
app.run = async (label, fn) => { try { say(label + " ..."); const r = await fn(); say(label + ": done.", "good"); return r; } catch (e) { app.fail(e); return undefined; } };

// ---- cells
let building = "head";                                    // the column whose tab is being built (fill): the refreshers a card registers belong to it
function reg(cell) { app.cells.push(cell); return cell; }
// 0.2.25: the editor's long labels, value names and tooltips (labels.js; the unit keeps its short ones), and the VOICE hide rule (layout.js voiceWhy)
const keyOf = (p, id) => (id >= P_E0 ? `e:${engineLabel(app.unit.parts[p].eng)}` : id);
function partCell(p, id, opt = {}) {
  const u = () => app.unit;
  return reg(new Cell({
    desc: () => longDesc(descOf(u().parts[p], id), keyOf(p, id), opt.label),
    value: () => u().parts[p].p[id],
    set: (v) => edit(p, id, v),
    inert: () => inertReason(u().parts[p], id) ?? voiceWhy(u().parts[p], id) ?? modulesWhy(u().menu, id, u().parts[p]),
    tip: (d) => tipOf(keyOf(p, id), d), whyText,
    ...(id === P_E0 ? waveTableOpts(p) : {}),
    ...(opt.src || {}),
  }, opt));
}
// an extension slot (protocol scope 2) as a control: the unit has 15 stored values without a parameter id (firmware ext.h); the part holds them in part.x. twin: the TWIN FILTER's hide rule (layout.js twinWhy)
function xCell(p, k, opt = {}) {
  const u = () => app.unit, key = `x${k}`;
  return reg(new Cell({
    desc: () => longDesc(xdescOf(u().parts[p], k), key, opt.label),
    value: () => u().parts[p].x?.[k] ?? L.XTP[k].def,
    set: (v) => editX(p, k, v),
    inert: () => (!u().hasX ? "NOT IN THIS FIRMWARE" : opt.twin ? twinWhy(u().parts[p], twinX(k)) : null),
    tip: (d) => tipOf(key, d), whyText,
  }, opt));
}
function editX(p, k, v) {
  const u = app.unit; if (!u) return;
  const was = u.parts[p].x?.[k];
  guardWrite(() => u.writeX(p, k, v));
  app.undo.record(p, `x${k}`, was, u.parts[p].x?.[k]);
}
// WAVE's TABLE list: an empty USER table says so and cannot be picked (0.2.25, owner: USER 2 picked, the unit said "user table is empty" and the sound kept on)
function waveTableOpts(p) {
  const isWave = () => engineLabel(app.unit.parts[p].eng) === "WAVE", user = (v) => { const d = descOf(app.unit.parts[p], P_E0); return v - (d.max - 2); };
  return {
    optLabel: (v) => { const d = longDesc(descOf(app.unit.parts[p], P_E0), keyOf(p, P_E0)), n = d.names?.[v - d.min] ?? String(v); if (!isWave()) return n; const k = user(v), t = app.unit.wt?.[k]; return k >= 0 && t && !t.used ? `${n} (EMPTY)` : n; },
    optDisabled: (v) => isWave() && user(v) >= 0 && app.unit.wt?.[user(v)]?.used === false,
  };
}
// The delay MODE as the unit does it (params.c dly_mode_changed): MODE into RANDOM / OCT UP with CHANCE (the DEPTH id) at 0 starts CHANCE at L.DMD.chanceDef; back to DIGITAL / PING-PONG an untouched
// CHANCE of that kind goes back to 0 (the modulation DEPTH, as before the visit). Remembered for this page only.
let dmdAuto = false;
function writeGlobalMode(id, v) {
  const u = app.unit, was = u.g[ID.G_DMODE];
  const done = u.writeGlobal(id, v);
  if (id === ID.G_DMDEP) dmdAuto = false;
  if (id !== ID.G_DMODE) return done;
  const r = dmdAfterMode(u.g, was, u.g[ID.G_DMODE], dmdAuto);
  dmdAuto = r.auto;
  if (r.depth != null) u.writeGlobal(ID.G_DMDEP, r.depth);
  return done;
}
function globalCell(id) {
  return reg(new Cell({ desc: () => longDesc(gdescOf(app.unit.g, id), `g${id}`), value: () => app.unit.g[id], set: (v) => { app.undo.record("g", id, app.unit.g[id], v); guardWrite(() => writeGlobalMode(id, v)); },
    inert: () => ginert(app.unit.g, id) ?? modulesWhy(app.unit.menu, id), tip: (d) => tipOf(`g${id}`, d), whyText }));
}
// a MENU setting as a card (the MASTER stages, the SIDECHAIN setup): d the layout's descriptor, key its labels.js key; toShown / toStored convert (THRESH)
function menuCell(id, d, key, opt = {}) {
  const item = () => app.unit?.menu?.find((m) => m.id === id), to = opt.toShown || ((v) => v), from = opt.toStored || ((v) => v);
  return reg(new Cell({ desc: () => longDesc(d, key, opt.label), value: () => to(item()?.value ?? opt.def ?? d.def),
    set: (v) => guardWrite(() => {                      // (coalesced like a parameter: a drag sends what it needs, the last value always; unit.idle() waits for it)
      const it = item(), was = it?.value, nv = from(v);
      if (it) it.value = nv;
      app.undo.record("m", id, was, nv);
      app.unit.queueWrite(`m:${id}`, nv, async (val) => { await app.unit.setMenu(id, val); app.refresh(); });
      app.refresh();
      return app.unit.idle();
    }),
    inert: () => (!item() ? "NOT IN THIS FIRMWARE" : opt.inert ? opt.inert() : null), tip: (dd) => tipOf(key, dd), whyText }, opt.cell || {}));
}
function guardWrite(fn) { try { return fn(); } catch (e) { app.fail(e); } }
// an edit of part p
function edit(p, id, v) {
  const u = app.unit; if (!u) return;
  const was = u.parts[p].p[id];
  guardWrite(() => u.writeParam(p, id, v));
  app.undo.record(p, id, was, u.parts[p].p[id]);
}
// UNDO (0.2.25): the old values go back to the unit (writes, not loads)
async function undoWrite(where, id, v) {
  const u = app.unit; if (!u) return;
  if ((where === 0 || where === 1) && id === "eng") { await u.setEngine(where, v); return; }   // (T0227: the engine change's undo step)
  if ((where === 0 || where === 1) && typeof id === "string") u.writeX(where, +id.slice(1), v);   // ("x6": an extension slot)
  else if (where === 0 || where === 1) u.writeParam(where, id, v);
  else if (where === "g") u.writeGlobal(id, v);
  else if (where === "m") await u.setMenu(id, v);
  else if (where === "f0" || where === "f1") app.fm6[+where[1]]?.setByte(id, v);
}
async function doUndo(where = null) {
  try {
    const e = await app.undo.undo(undoWrite, where);
    say(e ? `Undone: ${e.label ?? describeUndo(e.items[0])}.` : "Nothing to undo.", e ? "good" : "warn"); app.refresh();
  } catch (err) { app.fail(err); }
}
function describeUndo(x) {
  if ((x.where === 0 || x.where === 1) && typeof x.id === "string") return `PART ${x.where + 1} ${longDesc(xdescOf(app.unit.parts[x.where], +x.id.slice(1)), x.id)?.label ?? x.id}`;
  if (x.where === 0 || x.where === 1) return `PART ${x.where + 1} ${longDesc(descOf(app.unit.parts[x.where], x.id), keyOf(x.where, x.id))?.label ?? x.id}`;
  if (x.where === "g") return longDesc(gdescOf(app.unit.g, x.id), `g${x.id}`)?.label ?? `global ${x.id}`;
  if (x.where === "m") return app.unit.menu?.find((m) => m.id === x.id)?.name ?? "a setting";
  return `PART ${+String(x.where).slice(1) + 1} FM6 voice`;
}
app.doUndo = doUndo;
document.addEventListener("keydown", (ev) => {   // Ctrl / Cmd + Z: the newest edit (the piano roll and text fields keep their own)
  if (!(ev.ctrlKey || ev.metaKey) || ev.altKey || ev.shiftKey || ev.key.toLowerCase() !== "z" || ev.defaultPrevented) return;
  if (ev.target.closest?.(".roll") || /^(INPUT|TEXTAREA)$/.test(ev.target.tagName) && !/^(range|checkbox|radio|button)$/.test(ev.target.type)) return;
  if (!app.unit || app.overlay) return;
  ev.preventDefault(); doUndo();
});
// a card (T0229, owner): its title a tinted tab on the card's top line (editor.css .card > h3 .ttl), the card's tools at the right of that line (BYPASS, the reset ↺),
// then its controls. No subtitle and no note: what the card does and why a control is not drawn now is the title's tooltip (opt.tip, then the reasons of the
// hidden controls). Every card is the full width of its area (editor.css). opt.reset: false for none; opt.bypass: a BYPASS key
function card(title, cells, opt = {}) {
  const tools = h("span", { class: "cardtools" });
  if (opt.bypass) tools.append(bypassButton(opt.bypass));
  if (opt.reset !== false && cells.length) tools.append(h("button", { type: "button", class: "ghost small rst", text: "↺", "aria-label": `${title}: defaults`, title: `Defaults: every control of ${title} back to its default value (UNDO brings them back)`, onclick: () => resetCells(title, cells) }));
  const ttl = h("span", { class: "ttl", text: title });
  const grid = h("div", { class: "grid" }, cells.map((c) => c.el));
  const el = h("section", { class: "card" + (opt.cls ? " " + opt.cls : "") }, h("h3", {}, ttl, tools), grid);
  const tip = () => {                                     // the title's tooltip: what the card is, and what is not drawn now and why
    const gone = cells.filter((c) => c.disabled && c.why).map((c) => `${c.desc?.label}: ${c.why}`);
    const t = [opt.tip, gone.length ? `Not shown now: ${gone.join("; ")}.` : ""].filter(Boolean).join("\n");
    if (ttl.title !== t) ttl.title = t;
  };
  tip.col = building; app.refreshers.add(tip);
  if (opt.hideEmpty && cells.length) {                    // a card whose controls are all hidden is not drawn either (TWIN MOD and TWIN CHAIN while TYPE is OFF)
    const gone = () => { el.hidden = cells.every((c) => c.disabled); };
    gone.col = building; app.refreshers.add(gone);
  }
  el.grid = grid;
  return el;
}
// BYPASS (0.2.25, bypass.js): one button per effect section; it remembers the values it changed and puts them back. It always reads BYPASS (DRY: DRY), lit while on
function bypassOf() {
  const u = app.unit; if (!u) return null;
  if (!app.bypass || app.bypass.unit !== u) {
    const get = (w, id) => (w === "m" ? u.menu?.find((m) => m.id === id)?.value : u.parts[w]?.p[id]);
    const set = async (w, id, v) => { if (w === "m") await u.setMenu(id, v); else u.writeParam(w, id, v); };
    app.bypass = Object.assign(new Bypass(get, set), { unit: u });
  }
  return app.bypass;
}
async function toggleBypass(key, label) {
  const b = bypassOf(); if (!b) return;
  if (key.startsWith("m:") || key === "dry") { if (!app.unit.menu) await app.unit.readMenu().catch(() => {}); }
  try {
    const was = b.state(key).bypassed, items = await b.toggle(key);
    app.undo.group(`${was ? "restore" : "bypass"} ${label}`, items);
    say(was ? `${label}: back as it was.` : `${label}: bypassed.`, "good");
  } catch (e) { app.fail(e); }
  app.refresh();
}
function bypassButton(key, label) {
  const name = label || (key.split(":")[1] ? (PART_SECTIONS[key.split(":")[1]] || BUS_SECTIONS[key.split(":")[1]] || MASTER_SECTIONS[key.split(":")[1]])?.label : "everything") || key;
  const b = h("button", { type: "button", class: "ghost small byp", "aria-pressed": "false", text: key === "dry" ? "DRY" : "BYPASS", onclick: () => toggleBypass(key, name) });
  const upd = () => {
    const st = bypassOf()?.state(key); if (!st) return;
    b.setAttribute("aria-pressed", String(st.bypassed)); b.classList.toggle("on", st.bypassed);
    b.disabled = st.off && !st.bypassed && key !== "dry";
    b.title = key === "dry" ? (st.bypassed ? "DRY is on: every effect of both parts and the master stages is off. Press DRY again to bring them back." : "DRY: every effect of both parts and the master stages off at once (remembered: press again to bring them back).")
      : st.bypassed ? `${name} is bypassed by the editor. Press to bring its values back.` : st.off ? `${name} is off already (nothing to bypass).` : `Takes ${name} out of the sound and remembers its values; press again to bring them back.`;
  };
  upd.col = /^p[01]:/.test(key) ? +key[1] : key === "dry" ? "head" : 2; app.refreshers.add(upd); upd();
  return b;
}
// RESET of a card (0.2.25): every shown control of the card to its default, one undo step
async function resetCells(title, cells) {
  const todo = cells.filter((c) => !c.disabled && c.desc && c.src.value() !== c.src.desc().def);
  if (!todo.length) { say(`${title}: everything is at its default already.`); return; }
  const top = app.undo.list.length;
  app.undo.noMerge = true;
  try { for (const c of todo) await c.src.set(c.src.desc().def); } finally { app.undo.noMerge = false; }
  app.undo.group(`${title} back to defaults`, app.undo.list.splice(top).flatMap((e) => e.items));
  say(`${title}: back to the default values (UNDO brings them back).`, "good"); app.refresh();
}
const cellsOf = (p, ids, labels = {}) => ids.filter((i) => i >= 0).map((id) => partCell(p, id, labels[id] ? { label: labels[id] } : {}));

// ---- the tabs of one part (T0229: grouped by function, not by the unit's pages: one card per engine, the matrix one list, the delay one card)
function pageCard(p, title) {
  const part = app.unit.parts[p], pg = pageByTitle(title);
  if (!pg || !pageVisible(pg, part.eng)) return null;
  return { pg, ids: pageControls(pg, part).map((c) => c.id) };
}
// the ids of several firmware pages (ids already shown in this tab, `seen`, are left out)
function pageIds(p, titles, seen) {
  const ids = [];
  for (const t of titles) { const c = pageCard(p, t); if (c) for (const id of c.ids) if (!seen.has(id)) { seen.add(id); ids.push(id); } }
  return ids;
}
function pagesCard(p, name, titles, seen, opt = {}) {
  const ids = pageIds(p, titles, seen);
  return ids.length ? card(name, cellsOf(p, ids, opt.labels), opt) : null;
}
const addRef = (col, f) => { f.col = col; app.refreshers.add(f); return f; };
// EQUALIZER (0.2.25 layout round, owner): LOW FREQUENCY, LOW GAIN, MID FREQUENCY, MID GAIN, MID Q, HIGH FREQUENCY, HIGH GAIN; ONE BYPASS (the three gains to 0 dB, remembered);
// while bypassed the values are not drawn. T0229: no curve (no pictures in the editor)
function eqCard(p, seen) {
  const c = pagesCard(p, "EQUALIZER", ["EQ 1/2", "EQ 2/2"], seen, { bypass: `p${p}:eq`, tip: "Three bands: LOW and HIGH shelves, a MID bell." });
  if (!c) return null;
  addRef(p, () => { c.grid.hidden = !!bypassOf()?.state(`p${p}:eq`).bypassed; });
  return c;
}
const OWN_FILTERS = [];   // cloud job J: ANALOG, TRIO and NOISE have no filter of their own any more (no VCF / TRIO FILTER / NOISE FILT page)
// CYCLE ENV (cloud job F, B4 of SUB_AND_CYCLE_DESIGN: "in the envelopes group"): since the MOD tab was split (owner, 2026-10-21) the card sits on the ENVELOPE tab, after FILTER ENVELOPE.
// CYCLE_TAB names the tab that holds it (test_editor_0225.mjs and test_layout.mjs follow it).
export const CYCLE_TAB = "envelope";
app.cycleTab = CYCLE_TAB;
function cycleCard(p) {
  return card("CYCLE ENV", [X.X_CYMODE, X.X_CYSYNC, X.X_CYRISE, X.X_CYFALL, X.X_CYCURVE].map((k) => xCell(p, k)),
    { hideEmpty: true, cls: "five", tip: "A rise and fall that can repeat while the key is held (a source in the MATRIX tab: CYCLE). With SYNC on, RISE and FALL are note values of the tempo." });
}
// the second filter's three cards (cloud job I, FILTER_PAGES_PROPOSAL 3 and 5.2). The controls never say TWIN or FILTER again (the card's title does; naming rule R1), FILTER ENV excepted.
//   TYPE OFF: TWIN FILTER shows TYPE alone, TWIN MOD and TWIN CHAIN are not drawn. COMB: LINK and KEY TRACK are not drawn, CUTOFF / OFFSET reads TUNE and RESONANCE reads FEEDBACK (layout.js twDesc),
//   and COMB is not in the TYPE list on WAVE and PHYS. LINK relabels CUTOFF 2 (OFFSET while linked) and hides nothing.
// TWIN MOD and CUTOFF MOD (task 9, owner 11b): the depths of the sources to a filter's cutoff. The FILTER tab shows them all; the ENVELOPE tab shows the envelope's and the LFO tab the LFOs' (the same
// stored values, the same titles and control order; a change on one tab is on the other when it is opened; the hide rules are the cells' own, so both copies hide together)
function twinModCard(p, keys = null) {
  const all = [X.X_ENV2, X.X_KTRK2, X.X_LFO1, X.X_LFO2];
  return card("TWIN MOD", all.filter((k) => !keys || keys.includes(k)).map((k) => xCell(p, k, { twin: true })),
    { hideEmpty: true, tip: "How far the filter envelope, the played key and the LFOs move the second filter's cutoff (on COMB its tuning)." });
}
function cutModCard(p, only = null) {
  const e = L.ENG[app.unit.parts[p].eng], own2 = e?.ufilt === 2, pg = pageCard(p, "MULTI FILTER 2/2"); if (!pg) return null;
  const order = own2 ? [ID.P_E2FLT, ID.P_LD_FLT, ID.P_L2FLT] : [ID.P_E2FLT, ID.P_KTRK, ID.P_LD_FLT, ID.P_L2FLT];
  const ids = order.filter((id) => pg.ids.includes(id) && (!only || only.includes(id)));
  return ids.length ? card("CUTOFF MOD", cellsOf(p, ids), { tip: own2 ? `How far the filter envelope and the LFOs move ${e.name}'s own filter.` : "How far the filter envelope, the played key and the LFOs move the cutoff." }) : null;
}
function twinCards(p) {
  const part = () => app.unit.parts[p];
  const type = partCell(p, ID.P_TWTYPE, { src: { optHidden: (v) => !twinTypeOffered(part(), v) } });
  const filter = card("TWIN FILTER", [type, xCell(p, X.X_LINK, { twin: true }), partCell(p, ID.P_CUT2), partCell(p, ID.P_RES2)],
    { tip: "The second filter, after MULTI FILTER. TYPE OFF leaves it out. LINK ON: its cutoff follows MULTI FILTER's and OFFSET is the distance between them. COMB is a row of peaks tuned to the note." });
  const mod = twinModCard(p);
  const chain = card("TWIN CHAIN", [xCell(p, X.X_ROUTE, { twin: true }), partCell(p, ID.P_DRV2), partCell(p, ID.P_BLEND)],
    { hideEmpty: true, tip: "How the two filters are connected. DRIVE overloads the sound softly (OFF is not run). BLEND mixes the two filters: the centre is both." });
  const flow = h("div", { class: "flow", "aria-label": "Signal flow" });
  chain.append(flow);
  addRef(p, () => { flow.textContent = twinFlow(part()); });
  return [filter, mod, chain];
}
function buildPartTab(p, tab) {
  const u = app.unit, part = u.parts[p], e = L.ENG[part.eng], out = h("div", { class: "tabbody" + (tab === "matrix" ? " matrix" : "") });
  const add = (el) => el && out.append(el), seen = new Set();
  const own2 = e?.ufilt === 2;                            // ANALOG, TRIO, NOISE: the depths move the engine's own filter; the multimode filter follows it
  if (tab === "engine") {
    out.append(app.cols[p].engLine);                      // (the ENGINE selector: first, above the engine's own card)
    if (e && e.name !== "-") {   // T0227 (owner): all eight of the engine's own controls in ONE card, in the unit's order (FM6: SLOT first; the ids are the stored E slots)
      const ids = [0, 1, 2, 3, 4, 5, 6, 7].map((j) => P_E0 + (e.uiord ? e.uiord[j] : j));
      if (e.name === "WAVE") { const st = ids.indexOf(P_E0 + 6); ids.splice(st, 1); ids.splice(ids.indexOf(P_E0 + 1) + 1, 0, P_E0 + 6); }   // STEPS (E7) beside POS (cloud job F, C3 of SUB_AND_CYCLE_DESIGN: the editor groups by function)
      ids.forEach((id) => seen.add(id));
      const own = OWN_FILTERS.find((t) => pageCard(p, t));
      if (own) ids.push(...pageIds(p, [own], seen));      // (ANALOG's FILTER TYPE: its own filter's, in the engine's card; T0229, the list 4.3)
      const t = e.titles[0];
      add(card(ENGINE_CARD[e.name] ?? ENGINE_TITLES[t] ?? t ?? "ENGINE", cellsOf(p, ids)));
    }
    if (e?.name === "WAVE") readTables();                 // (the TABLE list marks an empty USER table)
    seen.add(ID.P_TRIM);                                  // (0.2.25 layout round: PATCH LEVEL is in the column's PATCH block, not on the VOICE card)
    // SUB OSC (cloud job F, A4 of SUB_AND_CYCLE_DESIGN): the sub oscillator's four values, next to the engine's card (extension slots; WAVE's own SUB control stays until job J)
    add(card("SUB OSC", [X.X_SUBWAVE, X.X_SUBOCT, X.X_SUBLVL].map((k) => xCell(p, k)), { hideEmpty: true, tip: "A wave one or two octaves below the played note, added to the engine's sound. LEVEL 0 switches it off." }));
    {   // VOICE: GLIDE before GLIDE TYPE (a control that makes another one appear comes first: nothing moves under a dragged slider)
      const ids = pageIds(p, ["VOICE 1/2", "VOICE 2/2"], seen), gi = ids.indexOf(ID.P_GLIDE), ti = ids.indexOf(ID.P_GLMODE);
      if (gi > ti && ti >= 0) { ids.splice(gi, 1); ids.splice(ti, 0, ID.P_GLIDE); }
      if (ids.length) add(card("VOICE", cellsOf(p, ids), { tip: "How the part plays its notes." }));
    }
    if (e?.name === "FM6") {                              // owner A7: the FM6 operator editor, collapsed until the user presses OPERATORS; open it scrolls the tab
      const open = !!app.opsOpen[p];
      out.append(h("div", { class: "opsbar" }, h("button", { type: "button", class: "ghost small opsbtn" + (open ? " on" : ""), text: "OPERATORS", "aria-expanded": String(open),
        onclick: () => { app.opsOpen[p] = !open; fill(p); refreshAll(); } })));
      if (open) {
        out.classList.add("opsopen");
        const ed = app.fm6[p] = createFm6Editor(app, p, { say, onEdit: (i, was, v) => app.undo.record(`f${p}`, i, was, v) });
        addRef(p, ed.refresh); add(ed.el);
      }
    }
  } else if (tab === "filter") {
    // FILTER (cloud job I, FILTER_PAGES_PROPOSAL 3): top to bottom the way the sound flows: filter 1 (MULTI FILTER, CUTOFF MOD), then filter 2 (TWIN FILTER, TWIN MOD, TWIN CHAIN). The cards of the engine tab's
    // old filter section moved here as they were. CUTOFF MOD (owner, 0b): FILTER ENV, KEY TRACK, LFO 1, LFO 2: the depths to the cutoff, here only (the ENVELOPE and LFO tabs show the same values again, task 9).
    // On ANALOG, TRIO, NOISE the three depths move the engine's own filter (the card on the ENGINE tab): this card comes first, KEY TRACK stays the multimode filter's
    const cmIds = own2 ? [ID.P_E2FLT, ID.P_LD_FLT, ID.P_L2FLT] : [ID.P_E2FLT, ID.P_KTRK, ID.P_LD_FLT, ID.P_L2FLT];
    const cutMod = () => cutModCard(p);
    if (own2) add(cutMod());
    cmIds.forEach((id) => seen.add(id));
    add(pagesCard(p, "MULTI FILTER", ["MULTI FILTER 1/2", "MULTI FILTER 2/2"], seen, { tip: own2 ? `The multimode filter, in series after ${e.name}'s own filter.` : "The part's filter." }));
    if (!own2) add(cutMod());
    twinCards(p).forEach(add);
  } else if (tab === "envelope") {
    [ID.P_ED_FLT, ID.P_E2FLT, ID.P_LD_FLT, ID.P_L2FLT].forEach((id) => seen.add(id));   // (the cutoff depths: CUTOFF MOD on the FILTER tab; the amp envelope's is not offered: owner)
    add(pagesCard(p, "AMP ENVELOPE", ["AMP ENV", "AMP DEST"], seen, { tip: "The volume over a note; its depths move the pitch and the shape." }));
    add(pagesCard(p, "FILTER ENVELOPE", ["ENV 2", "ENV 2 DEST"], seen, { tip: "The second envelope: its cutoff depth is FILTER ENV on the FILTER tab's CUTOFF MOD card; a matrix source too." }));
    if (CYCLE_TAB === "envelope") add(cycleCard(p));
    add(cutModCard(p, [ID.P_E2FLT])); add(twinModCard(p, [X.X_ENV2]));   // (task 9: the same depths as on the FILTER tab)
  } else if (tab === "lfo") {
    [ID.P_ED_FLT, ID.P_E2FLT, ID.P_LD_FLT, ID.P_L2FLT].forEach((id) => seen.add(id));
    add(pagesCard(p, "LFO 1", ["LFO 1", "LFO 1 TRIG", "LFO 1 DEST"], seen));
    add(pagesCard(p, "LFO 2", ["LFO 2", "LFO 2 TRIG", "LFO 2 DEST"], seen));
    add(cutModCard(p, [ID.P_LD_FLT, ID.P_L2FLT])); add(twinModCard(p, [X.X_LFO1, X.X_LFO2]));   // (task 9)
  } else if (tab === "matrix") {
    add(matrixList(p));
  } else if (tab === "fx") {
    const fxSat = pageCard(p, "FX");
    if (fxSat) {
      seen.add(fxSat.ids[0]);
      add(card("SATURATION", cellsOf(p, [fxSat.ids[0]]), { bypass: `p${p}:sat`, tip: "Warm distortion of this part, before its effects." }));
    }
    add(pagesCard(p, "SENDS", ["FX"], seen, { bypass: `p${p}:sends`, tip: "How much of this part goes to the shared CHORUS, DELAY and REVERB (GLOBAL tab)." }));
    add(pagesCard(p, "INSERT", ["INSERT 1/2", "INSERT 2/2"], seen, { bypass: `p${p}:insert`, tip: "This part's own effect." }));
    add(eqCard(p, seen));
    add(pagesCard(p, "DUCKING", ["DUCK 1/2", "DUCK 2/2"], seen, { bypass: `p${p}:duck`, tip: "Turns this part down on a beat, with the other part, or with a MIDI note." }));
  } else if (tab === "arpseq") {
    add(pagesCard(p, "ARPEGGIATOR", ["ARP 1/3", "ARP 2/3", "ARP 3/3"], seen));
    add(pagesCard(p, "CHORD", ["CHORD"], seen, { tip: "One key plays a chord." }));
    add(card("PATTERN", cellsOf(p, [ID.P_SQNT]), { tip: "The pattern itself (its LENGTH, DIVISION, SWING and GATE too) is on the SEQ tab." }));
  } else if (tab === "roll") {
    out.classList.add("rolltab");
    out.append(buildRollTab(p));
  }
  return out;
}
async function readTables(force = false) {
  const u = app.unit; if (!u || (!force && u.wt && Date.now() - (u.wtAt ?? 0) < 10000) || u.wtReading || !((u.info.caps ?? 0) & WTM.WT.CAPS_BIT)) return;
  u.wtReading = true;
  try { const l = await WTM.listTables((r, o) => u.link.request(r, o)); u.wt = l.tables.map((t) => ({ used: t.used, name: t.name })); u.wtAt = Date.now(); app.refresh(); } catch { /* the next time */ }
  finally { u.wtReading = false; }
}
app.readTables = readTables;

// ---- the piano roll (roll.js: model, undo, live writes; rollview.js: the picture). One PatternStore per part, shared by the tab and the full-width overlay.
function getStore(p) {
  if (!app.stores[p] || app.stores[p].unit !== app.unit) app.stores[p] = new PatternStore(app.unit, p);
  return app.stores[p];
}
// a cell of the roll's own toolbar: the view refreshes it (not app.cells); inertFn: the roll's hide rule for the controls that do not apply in the arp's SEQ mode
const rollCell = (p) => (id, inertFn) => new Cell({
  desc: () => longDesc(descOf(app.unit.parts[p], id), id), value: () => app.unit.parts[p].p[id], set: (v) => edit(p, id, v),   // (T0229: LENGTH, DIVISION, SWING, GATE: the editor's names, R6)
  inert: () => (inertFn ? inertFn() : inertReason(app.unit.parts[p], id)), tip: (d) => tipOf(id, d), whyText,
});
function rollView(p, extra) {
  return createRollView(app, p, { store: getStore(p), cell: rollCell(p), say, ...extra });
}
function buildRollTab(p) {
  app.rollViews[p]?.destroy(); if (app.rollViews[p]) app.refreshers.delete(app.rollViews[p].refresh);
  const v = rollView(p, { onExpand: openOverlay });
  app.rollViews[p] = v; app.refreshers.add(v.refresh = v.refresh.bind(v));
  return v.el;
}
function dropRollViews() {
  app.rollViews.forEach((v) => { if (v) { v.destroy(); app.refreshers.delete(v.refresh); } });
  app.rollViews = [];
}
// the full-width roll: PART 1 | PART 2 | BOTH (stacked), over the three columns (they cannot be used while it is open)
const KNOWN = ["--bg", "--fg", "--dim", "--line", "--panel", "--acc", "--accfg", "--page", "--warn", "--ok"];
function themeInto(el, p) { const src = app.cols[p]?.col.style; if (src) for (const k of KNOWN) { const v = src.getPropertyValue(k); if (v) el.style.setProperty(k, v); } }
function placeOverlay() {
  const ov = app.overlay; if (!ov) return;
  const r = $("main").getBoundingClientRect();
  Object.assign(ov.el.style, { left: r.left + "px", top: r.top + "px", width: r.width + "px", height: r.height + "px" });
}
function openOverlay(p = 0, mode) {
  if (!app.unit) return;
  closeOverlay(true);
  const views = [], el = h("div", { class: "rolloverlay", role: "region", "aria-label": "Piano roll, full width" });
  const ov = app.overlay = { el, views, mode: mode ?? String(p) };
  const body = h("div", { class: "ovbody" }), modeBtns = {};
  const build = (m) => {
    ov.mode = m;
    views.splice(0).forEach((v) => { v.destroy(); app.refreshers.delete(v.refresh); });
    clear(body);
    const parts = m === "both" ? [0, 1] : [+m];
    body.dataset.both = String(parts.length > 1);
    for (const q of parts) {
      const v = rollView(q, { wide: true, compact: parts.length > 1, onClose: () => closeOverlay() });
      app.refreshers.add(v.refresh = v.refresh.bind(v)); views.push(v);
      const pane = h("section", { class: "ovpane", "aria-label": `Part ${q + 1}` }, h("div", { class: "ovtitle" }, h("b", { text: `PART ${q + 1}` }), h("span", { class: "small", text: `${engineLabel(app.unit.parts[q].eng)}  ${patchLabel(q)}` })), v.el);
      themeInto(pane, q); body.append(pane);
    }
    views.forEach((v) => v.attach());
    for (const [k, b] of Object.entries(modeBtns)) b.setAttribute("aria-pressed", String(k === m));
    views[0]?.focus();
  };
  for (const [k, t] of [["0", "PART 1"], ["1", "PART 2"], ["both", "BOTH"]]) modeBtns[k] = h("button", { type: "button", class: "ghost small", text: t, onclick: () => build(k) });
  el.append(h("div", { class: "ovhead" }, h("b", { text: "SEQ" }), h("span", { class: "tg", role: "group", "aria-label": "Part" }, Object.values(modeBtns))), body);
  document.body.append(el); placeOverlay(); $("main").inert = true;
  build(ov.mode);
  refreshAll();
}
function closeOverlay(quiet) {
  const ov = app.overlay; if (!ov) return;
  ov.views.forEach((v) => { v.destroy(); app.refreshers.delete(v.refresh); });
  ov.el.remove(); app.overlay = null; $("main").inert = false;
  if (!quiet) {
    refreshAll();
    app.cols.slice(0, 2).forEach((c) => c?.col.querySelector(".roll .rgrid")?.focus());
  }
}
app.edit = edit; app.openOverlay = openOverlay; app.closeOverlay = closeOverlay;
document.addEventListener("keydown", (ev) => { if (app.overlay && ev.key === "Escape" && !ev.defaultPrevented) { ev.preventDefault(); closeOverlay(); } });
window.addEventListener("resize", placeOverlay);
// ---- the modulation matrix (T0229, owner): ONE list, slots 1 to 8 always (the editor does not mirror the unit's packing or its two pages); a head line names the
// three columns, the rows carry no labels. The drop-downs have group headings and the readable names of MATRIX_LIST_PREVIEW.md (labels.js MATRIX_GROUPS, VALUE_NAMES)
function matrixDstName(part, v) {
  const short = L.TP[ID.P_M1SRC + 1].names[v] ?? String(v), M = L.MD;
  if (v >= M.E1 && v < M.E1 + 8) { const d = descOf(part, P_E0 + v - M.E1); return unusedLabel(d) ? "-" : longDesc(d, `e:${engineLabel(part.eng)}`).label; }
  if (v >= M.INSA && v < M.INSA + 3) { const d = descOf(part, ID.P_IA + v - M.INSA); return part.p[ID.P_ITYPE] ? `INSERT ${INSERT_LONG[d.label] ?? d.label}` : VALUE_NAMES.dst[short]; }
  if (v === M.BUS + 3 && [2, 3].includes(dlyMode(app.unit?.g))) return "DELAY CHANCE";
  if (M.CUT2 != null && twinComb(part) && v === M.CUT2) return "TWIN FILTER TUNE";          // (the unit's DEST line on COMB: mod.c mod_dst_line)
  if (M.CUT2 != null && twinComb(part) && v === M.RES2) return "TWIN FILTER FEEDBACK";
  return VALUE_NAMES.dst[short] ?? short;
}
app.matrixDstName = matrixDstName;
function matrixList(p) {
  const u = app.unit, part = () => u.parts[p];
  const rows = [];
  for (const title of ["MATRIX 1-4", "MATRIX 5-8"]) for (const s of matrixSlots(pageByTitle(title, "LFO"))) {
    const cs = reg(new Cell({
      desc: () => longDesc(descOf(part(), s.src), s.src), value: () => part().p[s.src], set: (v) => edit(p, s.src, v), inert: () => null,
      tip: (d) => tipOf(s.src, d), groups: MATRIX_GROUPS.src,
    }, { bare: true }));
    const cd = reg(new Cell({
      desc: () => longDesc(descOf(part(), s.dst), s.dst), value: () => part().p[s.dst], set: (v) => edit(p, s.dst, v),
      inert: () => dstWhy(part(), part().p[s.dst], u.g), whyText, optLabel: (v) => matrixDstName(part(), v), optHidden: (v) => !dstOffered(part(), v, u.g), groups: MATRIX_GROUPS.dst,
      tip: (d) => tipOf(s.dst, d),
    }, { soft: true, bare: true }));
    const ca = partCell(p, s.amt, { bare: true });
    rows.push(h("div", { class: "mrow", dataset: { slot: s.n } }, h("span", { class: "mn", text: String(s.n) }), cs.el, cd.el, ca.el));
  }
  return h("section", { class: "card matrix", "aria-label": `Part ${p + 1} modulation matrix` },
    h("div", { class: "mrow mhead", "aria-hidden": "true" }, h("span", { class: "mn" }), h("span", { text: "SOURCE" }), h("span", { text: "DESTINATION" }), h("span", { text: "AMOUNT" })), rows);
}

// ---- the global column
function sidechainCard() {
  // SIDECHAIN (0.2.22), GLO > SIDECHAIN: the ducker's MIDI setup, MENU settings 38..41 (a device setting: no patch, multi or project holds it): each part's channel and note that start
  // its DUCK when the part's DUCK SOURCE is MIDI NOTE. T0229 (the list (?) 4.11, option A): a small grid, rows PART 1 / PART 2, columns CHANNEL / NOTE: no word twice
  if (!app.unit.menu && app.unit.info.menuCount != null) app.unit.readMenu().then(() => app.refresh()).catch(() => {});
  const cells = [38, 39, 40, 41].map((id, k) => menuCell(id, L.SIDECH[k & 1], `m:${(k & 1) ? "NOTE" : "CH"}`, { label: (k & 1) ? "NOTE" : "CHANNEL" }));
  const c = card("SIDECHAIN", [], { reset: false, tip: "The unit's own MIDI setup of the ducking (not in patches or multis): the channel and the note that start a part's ducking when its DUCK SOURCE is MIDI NOTE." });
  c.grid.replaceWith(h("div", { class: "scgrid" }, h("b", { text: "PART 1" }), cells[0].el, cells[1].el, h("b", { text: "PART 2" }), cells[2].el, cells[3].el));
  return c;
}
const DUEL_TIP = "Not used when the unit's MODE is DUEL (each part on its own output): the editor cannot read the MODE, check it on the unit. A setting of the unit, in no patch or multi.";
function buildGlobalTab(wrap) {
  const gcard = (title, ids, opt) => card(title, ids.filter((i) => i >= 0).map((id) => globalCell(id)), opt);
  const tab = app.gtab;
  if (tab === "global") {
    const pgs = (t) => pageByTitle(t).ids;
    if (!app.unit.menu && app.unit.info.menuCount != null) app.unit.readMenu().then(() => app.refresh()).catch(() => {});
    // the MASTER stages (0.2.25 layout round, owner): STEREO MASTER (BASS MONO, WIDTH) and COMPRESSOR (RATIO first: OFF its lowest value, then THRESHOLD, ATTACK, RELEASE), in the order
    // the sound passes them, one BYPASS each. They are the unit's MENU settings: BASS MONO 33, WIDTH 42, COMP 34..37 (THRESH in dB, the layout's card counts 0 .. 40). T0229: no pictures
    const item = (id) => app.unit?.menu?.find((m) => m.id === id);
    const ratioOff = () => (!item(35)?.value ? "RATIO OFF" : null);
    const stereo = card("STEREO MASTER", [menuCell(33, L.MASTER, "m:BMONO"), menuCell(42, L.MSTG[0], "m:WIDTH")], { bypass: "m:stereo", tip: "Below BASS MONO the low end in mono, above it the stereo picture as wide as WIDTH. " + DUEL_TIP });
    const comp = card("COMPRESSOR", [
      menuCell(35, L.COMP[1], "m:RATIO"),
      menuCell(34, L.COMP[0], "m:THRESH", { toShown: (v) => v + 40, toStored: (v) => v - 40, def: L.COMP[0].def - 40, inert: ratioOff }),
      menuCell(36, L.COMP[2], "m:ATTACK", { inert: ratioOff }), menuCell(37, L.COMP[3], "m:RELEASE", { inert: ratioOff })], { bypass: "m:comp", tip: "Evens out the level of the whole mix (the makeup is automatic). " + DUEL_TIP });
    // REVERB (owner, 0b): the shimmer's PITCH and DRIFT inside the reverb card, drawn only with TYPE SHIM (layout.js ginert)
    wrap.append(h("div", { class: "tabbody" },
      gcard("CLOCK", [2, 0, 1], { tip: "Where the tempo comes from, the tempo and the swing of the whole instrument." }),   // (the clock source first, as the unit's TEMPO page)
      gcard("CHORUS", pgs("CHORUS"), { bypass: "bus:chorus", tip: "Shared by both parts: each part's SENDS feed it. BYPASS sets both parts' chorus sends to 0 and remembers them." }),
      gcard("DELAY", [...pgs("DELAY 1/2"), ...pgs("DELAY 2/2")], { bypass: "bus:delay", tip: "Shared by both parts. In RANDOM and OCTAVE UP the modulation depth is the CHANCE of an event and MOD RATE is not used." }),
      gcard("REVERB", [...pgs("REVERB"), ...pgs("SHIMMER")], { bypass: "bus:reverb", tip: "Shared by both parts. PITCH and DRIFT belong to TYPE SHIMMER." }),
      stereo, comp));
  } else if (tab === "midi") buildMidi(app, wrap, globalCell, card, sidechainCard());
  else if (tab === "patches") buildPatches(app, wrap);
  else if (tab === "multis") buildMultis(app, wrap);
  else if (tab === "patterns") buildPatterns(app, wrap);
  else if (tab === "tables") buildWavetables(app, wrap);
  else if (tab === "settings") buildSettings(app, wrap, card);
}

// ---- a column (0.2.25 layout round, owner; T0229): PART n and Undo; the MIXER block (LEVEL, PAN, MUTE, SOLO: the player's, not the patch's); the PATCH block
// (PATCH | #83 TINE ROOM, CATEGORY at the right; ENGINE, PATCH LEVEL, Browse..., Init...); then the tab strip and the body of the tab
function btnCell(...kids) { return h("div", { class: "cell btncell" }, h("div", { class: "top" }, h("span", { class: "lab", text: " " })), h("div", { class: "prow" }, kids)); }
function muteCell(p) {
  const b = h("button", { type: "button", class: "mute", "aria-pressed": "false", text: "MUTE", onclick: () => edit(p, ID.P_MUTE, app.unit.parts[p].p[ID.P_MUTE] ? 0 : 1) });
  const upd = () => {
    const m = !!app.unit?.parts[p]?.p[ID.P_MUTE];
    b.classList.toggle("on", m); b.setAttribute("aria-pressed", String(m));
    b.title = m ? `PART ${p + 1} is muted. Press MUTE again and it sounds.` : `Press MUTE: PART ${p + 1} is silent until you press it again. Not part of the patch.`;
  };
  upd.col = "head"; app.refreshers.add(upd); upd();
  return b;
}
// SOLO (owner, 0b): one click: this part sounds, the other part is muted (the MUTE buttons show it). Editor only, no stored state of its own; one undo step.
// Owner item 18: a second click on SOLO switches it off: the mutes are put back as they were before the SOLO (a part the user muted by hand stays muted, nothing else stays muted)
let soloMemo = null;   // { p: the part soloed, mute: [part 1, part 2] as before the first SOLO }
const isSolo = (q, p) => !q[p].p[ID.P_MUTE] && !!q[1 - p].p[ID.P_MUTE];
function soloCell(p) {
  const b = h("button", { type: "button", class: "mute solo", "aria-pressed": "false", text: "SOLO", onclick: () => {
    const u = app.unit; if (!u) return;
    const q = u.parts, top = app.undo.list.length, was = soloMemo && isSolo(q, soloMemo.p) ? soloMemo : null;   // (a SOLO still on, of this part or of the other)
    app.undo.noMerge = true;
    try {
      if (was && was.p === p) { for (const k of [0, 1]) edit(k, ID.P_MUTE, was.mute[k]); soloMemo = null; }
      else { soloMemo = { p, mute: was ? was.mute : [q[0].p[ID.P_MUTE] ? 1 : 0, q[1].p[ID.P_MUTE] ? 1 : 0] }; edit(p, ID.P_MUTE, 0); edit(1 - p, ID.P_MUTE, 1); }
    } finally { app.undo.noMerge = false; }
    app.undo.group(`SOLO PART ${p + 1}`, app.undo.list.splice(top).flatMap((e) => e.items));
    app.refresh();
  } });
  const upd = () => {
    const q = app.unit?.parts; if (!q?.[1]) return;
    const on = soloMemo?.p === p && isSolo(q, p);   // (lit only for the SOLO this button made: a part muted by hand is not a SOLO)
    b.classList.toggle("on", on); b.setAttribute("aria-pressed", String(on));
    b.title = `SOLO: PART ${p + 1} sounds and PART ${2 - p} is muted (one click; the MUTE buttons show it).`;
  };
  upd.col = "head"; app.refreshers.add(upd); upd();
  return b;
}
function buildColumn(p) {
  const u = app.unit, part = u.parts[p], col = h("section", { class: "col part", "aria-label": `Part ${p + 1}`, dataset: { part: p } });
  const engSel = h("select", { "aria-label": `Part ${p + 1} engine`, onchange: (ev) => changeEngine(p, +ev.target.value, ev.target) },
    pickable(part.eng).map((e) => h("option", { value: e, text: engineLabel(e) === "-" ? "(retired engine)" : engineLabel(e) })));
  engSel.value = part.eng;
  const engLine = h("div", { class: "engline" }, h("label", { class: "lab", text: "ENGINE" }), engSel);   // (owner 2026-10-21, items 24 / 32: the ENGINE tab's first element, on a full-width line of its own; the same select, the same code path)
  const sel = h("span", { class: "seldot", title: "This is the part the unit has selected (ALGORITHM)" });
  const snd = h("span", { class: "pname" }), cat = h("span", { class: "pcat" });
  const undoBtn = h("button", { class: "ghost small", text: "Undo", title: `Undo the last edit of PART ${p + 1} (Ctrl+Z or Cmd+Z: the last edit of any part)`, onclick: () => doUndo(p) });
  const prev = building; building = "head";
  const mixer = h("section", { class: "block mixblock", "aria-label": `Part ${p + 1} mixer` }, h("h3", {}, h("span", { class: "ttl", text: "MIXER", title: "The player's level, pan and mute: not part of the patch (a multi stores them)." })),
    h("div", { class: "hgrid" }, partCell(p, ID.P_LEVEL).el, partCell(p, ID.P_PAN).el, btnCell(muteCell(p), soloCell(p))));
  const patch = h("section", { class: "block patchblock", "aria-label": `Part ${p + 1} patch` }, h("h3", {}, h("span", { class: "ttl", text: "PATCH" }), h("span", { class: "sep", text: "|" }), snd, h("span", { class: "grow" }), cat),
    h("div", { class: "hgrid" },
      btnCell(
        h("button", { class: "ghost small", text: "Browse...", title: "Browse the unit's 128 patch slots and recall one into this part (the PATCHES tab of the GLOBAL column)", onclick: () => { app.target = p; setGlobalTab("patches", true); } }),
        h("button", { class: "ghost small", text: "Init...", title: "Init patch: the engine's basic sound, every common setting at its default (on the unit: SAVE > TOOLS > INIT SOUND)", onclick: () => initPatch(p) })),
      partCell(p, ID.P_TRIM).el));
  building = prev;
  const head = h("div", { class: "colhead" }, h("div", { class: "rowhead" }, h("b", { text: `PART ${p + 1}` }), sel, mixer, h("span", { class: "grow-u" }), undoBtn), patch);   // (T0229: PART n, the MIXER line and Undo share one row)
  const updUndo = () => { undoBtn.disabled = !app.undo.canUndo(p); }; updUndo.col = "head"; app.refreshers.add(updUndo);
  const strip = h("nav", { class: "tabs", role: "tablist", "aria-label": `Part ${p + 1} tabs` });
  const body = h("div", { class: "colbody" });
  col.append(head, strip, body);
  app.cols[p] = { col, body, strip, engSel, engLine, sel, snd, cat };
  // a patch file, a DX7 .syx or a slot dropped on the part: it is loaded into this part, live, and not saved anywhere
  dropZone(col, { accept: (e) => hasFiles(e) || hasSlot(e), onDrop: async (e) => {
    if (hasFiles(e)) { for (const f of filesOf(e)) await handleFile(app, f, { part: p }); return; }
    const slot = slotOf(e); if (slot == null) return;
    try { app.say(`Loading slot #${slot + 1} into PART ${p + 1} ...`); await u.loadSlot(p, slot); u.parts[p].name = u.slots?.[slot]?.name ?? null; app.refresh(); app.say(`#${slot + 1} ${u.slots?.[slot]?.name ?? ""} is loaded in PART ${p + 1} (not saved).`, "good"); } catch (err) { app.fail(err); }
  } });
  return col;
}
// the GLOBAL column (T0229): no head of its own; its tab strip is its first line, DRY at the strip's right end; CLOCK is the first card of the GLOBAL tab
function buildGlobalColumn() {
  const col = h("section", { class: "col glob", "aria-label": "Global" });
  const prev = building; building = "head";
  const dry = bypassButton("dry", "DRY");
  building = prev;
  const strip = h("nav", { class: "tabs", role: "tablist", "aria-label": "Global tabs" });
  const body = h("div", { class: "colbody" });
  col.append(strip, body);
  app.cols[2] = { col, body, strip, dry };
  // a patch file, a DX7 .syx or a WAV dropped anywhere on the GLOBAL column goes to the library (the PATCHES and WAVETABLES tabs take it themselves)
  dropZone(col, { accept: hasFiles, onDrop: async (e) => { for (const f of filesOf(e)) await handleFile(app, f, { start: app.patchStart ?? 0 }); } });
  return col;
}
async function initPatch(p) {
  const u = app.unit; if (!u) return;
  if (!(await ask(`Init patch: PART ${p + 1}`, [`PART ${p + 1} gets ${engineLabel(u.parts[p].eng)}'s basic sound: every sound setting at its default. The mixer, the arpeggiator and the pattern stay. Nothing is saved; this is not undoable.`], "Init patch"))) return;
  await app.run(`Init PART ${p + 1}`, async () => { await u.initSound(p); app.undo.forget(p); });
  app.refresh();
}
// "#12 FOLD BASS": the slot's number when the editor knows it, then the name; a sound recalled on the unit (the protocol does not report the slot): "#--  ON THE UNIT"
function patchLabel(p) {
  const part = app.unit.parts[p], k = part.slot;
  if (k != null) return `#${k + 1} ${soundName(p)}`;
  return part.fromUnit && !part.name ? "#--  ON THE UNIT" : soundName(p);
}
function soundName(p) {
  const part = app.unit.parts[p];
  if (part.name) return part.name;
  if (part.fromUnit) return "ON THE UNIT";
  const s = L.ENG[part.eng]?.sounds[part.preset];
  return s ? s[0] : (L.ENG[part.eng]?.name === "-" ? "" : "INIT");
}
async function changeEngine(p, eng, selectEl) {
  const u = app.unit, was = u.parts[p].eng;
  if (eng === was) return;
  // 0.2.24 (owner): an engine change keeps the sound's common settings; only the engine's own eight change. T0227: no dialog; one undo step puts the engine back
  const nm = soundName(p), slot = u.parts[p].slot;
  try {
    await u.setEngine(p, eng);
    if (!u.parts[p].name) u.parts[p].name = nm;
    if (u.parts[p].slot == null) u.parts[p].slot = slot;
    app.undo.group(`PART ${p + 1} engine ${engineLabel(was)} -> ${engineLabel(eng)}`, [{ where: p, id: "eng", oldV: was, newV: eng }]);
    app.refresh();
  } catch (e) { app.fail(e); selectEl.value = u.parts[p].eng; }
}

// ---- tabs (T0229: one line always)
const tabsOf = (i) => (i === 2 ? GLOBAL_TABS : partTabs(app.unit?.parts[i]?.eng));
const tabKey = (i) => (i === 2 ? app.gtab : app.tabs[i]);
function renderStrip(i) {
  const c = app.cols[i]; if (!c) return;
  c.stripSig = i < 2 ? tabsOf(i).map((t) => t.key).join() : "";
  clear(c.strip).append(...tabsOf(i).map((t) => h("button", { role: "tab", class: "tab", "aria-selected": String(t.key === tabKey(i)), tabindex: t.key === tabKey(i) ? 0 : -1, dataset: { key: t.key }, text: TAB[t.key] ?? t.label,
    title: t.key === "roll" && i < 2 ? rollTabTip(i) : "", onclick: () => (i === 2 ? setGlobalTab(t.key) : setPartTab(i, t.key)) })));
  if (i === 2) c.strip.append(h("span", { class: "grow" }), c.dry);
}
// the SEQ tab's tooltip (the list's note 62): when the pattern plays nothing, or plays as the arp's offsets
function rollTabTip(p) {
  const u = app.unit, q = u?.parts[p]?.p; if (!q) return "";
  const off = (mid) => { const m = u.menu?.find((x) => x.id === mid); return !!m && m.names?.[m.value - m.min] === "OFF"; };
  const t = [];
  if (off(29)) t.push("SEQ is OFF in SETTINGS > MODULES: the unit plays none of these steps (editing still works).");
  if (q[ID.P_AMODE] === 15) t.push(off(28) ? "ARP MODE is SEQ but ARP is OFF in SETTINGS > MODULES: the pattern plays nothing now." : "ARP MODE is SEQ: the steps are offsets for the keys you hold (C4 = no offset), timed by the arp's RATE and GATE.");
  return t.join("\n");
}
document.addEventListener("keydown", (ev) => {
  const strip = ev.target.closest?.(".tabs"); if (!strip) return;
  const tabs = [...strip.querySelectorAll("[role=tab]")], i = tabs.indexOf(document.activeElement);
  if (i < 0) return;
  const j = ev.key === "ArrowRight" ? (i + 1) % tabs.length : ev.key === "ArrowLeft" ? (i + tabs.length - 1) % tabs.length : ev.key === "Home" ? 0 : ev.key === "End" ? tabs.length - 1 : -1;
  if (j >= 0) { ev.preventDefault(); tabs[j].click(); strip.querySelectorAll("[role=tab]")[j].focus(); }
});
export function setPartTab(p, key) {
  if (!app.cols[p]) return;
  app.tabs[p] = key; app.target = p; renderStrip(p); fill(p);
  refreshAll();
}
export function setGlobalTab(key, showView = false) {
  app.gtab = key; renderStrip(2); fill(2);
  if (showView) setView("global");
  refreshAll();
}
app.setPartTab = setPartTab; app.setGlobalTab = setGlobalTab;

function signature(p) { const q = app.unit.parts[p]; if (app.tabs[p] === "roll") return "roll"; return `${q.eng}|${app.tabs[p] === "fx" ? Math.min(q.p[ID.P_DKSRC], 3) : ""}`; }
function fill(i) {
  const c = app.cols[i]; if (!c) return;
  if (i < 2 && !tabsOf(i).some((t) => t.key === app.tabs[i])) { app.tabs[i] = "engine"; renderStrip(i); }   // (a tab an engine change took away)
  app.cells = app.cells.filter((x) => x.partBody !== i);
  for (const f of [...app.refreshers]) if (f.col === i) app.refreshers.delete(f);   // (the refreshers of the tab being replaced)
  if (i < 2 && app.fm6[i]) { app.fm6[i].destroy(); app.fm6[i] = null; }
  const before = app.cells.length, prev = building;
  building = i;
  if (i === 2) { const wrap = h("div", { class: "gbody" }); buildGlobalTab(wrap); c.body.replaceChildren(wrap); }
  else {
    if (app.tabs[i] !== "roll" && app.rollViews[i]) { app.rollViews[i].destroy(); app.refreshers.delete(app.rollViews[i].refresh); app.rollViews[i] = null; }
    c.body.replaceChildren(buildPartTab(i, app.tabs[i])); app.sig[i] = signature(i);
    if (app.tabs[i] === "roll") app.rollViews[i]?.attach();
  }
  building = prev;
  app.cells.slice(before).forEach((x) => { x.partBody = i; });
  c.body.scrollTop = 0;
  c.body.dataset.tab = tabKey(i);
  const t = tabsOf(i).find((x) => x.key === tabKey(i)); document.title = `DUELL editor: ${i === 2 ? "GLOBAL " : "PART " + (i + 1) + " "}${TAB[t?.key] ?? t?.label ?? ""}`;
}

function renderMain() {
  closeOverlay(true); dropRollViews(); app.stores = [];
  const main = clear($("main"));
  app.cells = []; app.cols = []; app.refreshers.clear(); app.sig = [];
  main.classList.toggle("empty", !app.unit);
  if (!app.unit) { applyTheme(null); return; }   // (T0229: before connecting the page is empty; the Connect button and the status line say the rest)
  const c0 = buildColumn(0), c1 = buildColumn(1), cg = buildGlobalColumn();
  main.dataset.view = app.view;
  main.append(c0, c1, cg);
  for (let i = 0; i < 3; i++) { renderStrip(i); fill(i); }
  applyTheme(app.pal);
  refreshAll();
}
function setView(v) { app.view = v; $("main").dataset.view = v; $("vparts").setAttribute("aria-pressed", v === "parts"); $("vglobal").setAttribute("aria-pressed", v === "global"); fitSoon(); }
app.setView = setView;
function refreshAll() {
  const u = app.unit; if (!u) return;
  for (let p = 0; p < 2 && app.cols[p]; p++) {
    const c = app.cols[p];
    if (c.stripSig !== tabsOf(p).map((t) => t.key).join()) renderStrip(p);
    if (app.sig[p] !== signature(p)) fill(p);
    c.engSel.value = u.parts[p].eng;
    if (![...c.engSel.options].some((o) => +o.value === u.parts[p].eng)) { c.engSel.append(h("option", { value: u.parts[p].eng, text: "(retired engine)" })); c.engSel.value = u.parts[p].eng; }
    c.engSel.title = L.ENG[u.parts[p].eng]?.name === "-" ? "This part plays a retired engine: it is silent and keeps its values. Pick an engine." : "";
    c.sel.classList.toggle("on", u.selected === p);
    c.snd.textContent = patchLabel(p);
    c.snd.title = u.parts[p].fromUnit && u.parts[p].slot == null ? "Recalled on the unit: the unit does not tell the editor which slot it plays." : "";
    {   // CATEGORY: the slot's, when the editor knows the slot (it loaded or stored it); the protocol does not say which slot a recall on the unit played
      const k = u.parts[p].slot, s = k != null ? u.slots?.[k] : null;
      c.cat.textContent = s && s.cat != null ? `CATEGORY ${CATEGORY_NAMES[s.cat] ?? "-"}` : "";
      c.cat.title = s ? `The category of slot #${k + 1} (set when saving, on the PATCHES tab).` : "";
    }
    const rt = c.strip.querySelector('[data-key="roll"]'); if (rt) rt.title = rollTabTip(p);
  }
  for (const c of app.cells) c.refresh();
  for (const f of app.refreshers) f();
  const w = u.writable;
  $("main").classList.toggle("readonly", !w);
  if (!w) say(u.compat, "warn");
  fitSoon();
}
let raf = 0;
app.refresh = () => { if (!raf) raf = requestAnimationFrame(() => { raf = 0; refreshAll(); }); };

// ---- the fit (T0229, owner: no scroll bars, sideways or down). Below the smallest room, 1260 x 600 inside the browser, the whole page is drawn smaller (CSS zoom)
// instead of scrolling. As a last safety a column whose tab is a few pixels too tall shrinks itself, down to 85 % at most (owner, sizing policy: Full HD 1920 x 1080 is the target and must fit; a smaller window scales; a tab that still does not fit at 85 % scrolls, the last resort) (the overflow test in test_layout.mjs
// checks every tab at its natural size, without this safety: a tab that needs it fails the build)
const MIN_W = 1260, MIN_H = 600;
function fitPage() {
  const W = document.documentElement.clientWidth || innerWidth, H = innerHeight, z = Math.min(1, W / MIN_W, H / MIN_H), b = document.body.style;
  if (z < 1) { b.zoom = String(z); b.width = `${W / z}px`; b.height = `${H / z}px`; } else { b.zoom = ""; b.width = ""; b.height = ""; }
  app.zoom = z;
  // the logo (owner, sizing policy): 84 px on a Full HD window (940 px of browser room), in proportion to the window height below that, never under 56 px as drawn on the screen (the page zoom is divided out)
  const logo = Math.max(56, Math.min(84, 84 * H / 940));
  document.documentElement.style.setProperty("--logo", `${logo / z}px`);
  app.logoPx = logo;
}
function fitColumns() {
  const off = document.documentElement.classList.contains("nofit");
  for (const c of app.cols) {
    const t = c?.body.firstElementChild; if (!t || !t.classList.contains("tabbody") || t.classList.contains("rolltab")) continue;
    t.style.zoom = "";
    if (off) continue;
    const room = c.body.clientHeight - 2, need = t.scrollHeight + 2 * parseFloat(getComputedStyle(c.body).paddingTop || 0);
    if (need > room + 1 && room > 0) t.style.zoom = String(Math.max(0.85, room / need));
  }
}
let fitRaf = 0;
app.fitNow = () => { fitColumns(); };
function fitSoon() { if (!fitRaf) fitRaf = requestAnimationFrame(() => { fitRaf = 0; fitColumns(); }); }
addEventListener("resize", () => { fitPage(); fitSoon(); });
fitPage();

// ---- the unit's colour (MENU > COLOR, id 0 = the palette index of the firmware's UI_PALETTES, layout.json)
function applyTheme(i) {
  const root = document.documentElement.style;
  const known = ["--bg", "--fg", "--dim", "--line", "--panel", "--acc", "--accfg", "--page", "--warn", "--ok"];
  const set = (el, vars) => { for (const k of known) el.setProperty(k, vars[k]); };
  const unset = (el) => { for (const k of known) el.removeProperty(k); };
  if (i == null) { unset(root); root.removeProperty("color-scheme"); document.documentElement.dataset.scheme = matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light"; $("unit").title = ""; return; }
  set(root, themeVars(i, 2)); root.setProperty("color-scheme", isDark(i) ? "dark" : "light"); document.documentElement.dataset.scheme = isDark(i) ? "dark" : "light";
  for (let k = 0; k < 3; k++) if (app.cols[k]) set(app.cols[k].col.style, themeVars(i, k));
  $("unit").title = `The unit's colour: ${paletteName(i)}`;
}
function setPalette(i) {
  if (!Number.isInteger(i) || i === app.pal) return;
  app.pal = i; applyTheme(i);
}
let themeTimer = 0;
let pointerHeld = false;                                // a pointer button is down in the editor (a slider drag): the unit's poll does not write over controls meanwhile
document.addEventListener("pointerdown", () => { pointerHeld = true; }, true);
for (const ev of ["pointerup", "pointercancel", "blur"]) addEventListener(ev, () => { pointerHeld = false; }, true);
async function pollTheme() {
  const u = app.unit; if (!u || u.info.menuCount == null || document.hidden) return;
  try { const m = parseMenu(await u.req(CMD.MENU_DESC, [0], 1500)); if (m && m.id === 0) setPalette(m.value); } catch { /* the next poll */ }
}
function startTheme(u) {
  stopTheme(); app.pal = null;
  if (u.info.menuCount == null) { setPalette(L.PALETTES.DEFAULT); return; }   // firmware that does not describe its MENU: the default theme
  pollTheme().then(() => { if (app.pal == null) setPalette(L.PALETTES.DEFAULT); });
  themeTimer = setInterval(pollTheme, 2000);
}
function stopTheme() { clearInterval(themeTimer); themeTimer = 0; app.pal = null; }
matchMedia("(prefers-color-scheme: dark)").addEventListener?.("change", () => { if (app.pal == null) applyTheme(null); });

// ---- events from the unit
function wire(u) {
  u.isBusy = () => pointerHeld;                         // (0.2.25: the poll leaves the settings alone while a control is being dragged)
  u.on("menu", (items) => { const c = items?.find((m) => m.id === 0); if (c) setPalette(c.value); app.refresh(); })
    .on("param", () => app.refresh()).on("global", () => app.refresh()).on("reload", () => { app.refresh(); }).on("selected", () => app.refresh())
    .on("error", (e) => app.fail(e)).on("patches", () => { app.refresh(); app.patchesChanged?.(); }).on("step", (d) => { app.stores[d.part]?.external(d.index); app.stepChanged?.(d); });
  u.on("reload", (d) => { for (const st of app.stores) if (st && (d.part < 0 || d.part === st.part)) st.invalidate(); });
}

// ---- connect
function setStatus(text, cls = "") { const e = $("unit"); e.textContent = text; e.className = "unit " + cls; }
let connecting = false, auto = false;   // auto: connect by itself when the unit appears (MIDI already allowed; off after the user's Disconnect)
async function connect() {
  if (connecting || app.unit) return;
  connecting = true; auto = true;
  $("connect").disabled = true; setStatus("Looking for the FM-1 ..");
  try {
    let link;
    if (MOCK) {
      const { MockUnit, MockLink } = await import("./mockunit.js");
      app.mock = new MockUnit({ pat32: MOCK === "pat32", caps: MOCK === "old" ? 3 : 31, playing: MOCK === "playing", patchRaw: MOCK !== "old", version: MOCK === "old" ? "FELUCCA DUELL 0.2.13 bet" : undefined });
      link = new MockLink(app.mock, 2);
    } else {
      let midi;
      try { midi = await navigator.requestMIDIAccess({ sysex: true }); } catch { setStatus("MIDI access with SysEx was refused. Allow it and press Connect again.", "warn"); return; }
      const named = (p) => /felucca|duell/i.test(p.name || "") && p.state !== "disconnected";
      const input = [...midi.inputs.values()].find(named), output = [...midi.outputs.values()].find(named);
      if (app.midi !== midi) {   // (the unit may come later: watched from here on)
        app.midi = midi;
        midi.onstatechange = (e) => { if (e.port.state === "disconnected" && app.ports?.includes(e.port) && app.unit) lost("The FM-1 was disconnected."); };
        watchUnit(midi, { wanted: () => auto && !app.unit && !connecting, go: connect });
      }
      if (!input || !output) { setStatus("No FM-1 found. Connect it by USB, close other programs that use it (the installer, another editor) and press Connect again.", "warn"); return; }
      link = new Link(input, output);
      app.ports = [input, output];
    }
    const u = new Unit(link); wire(u);
    await u.connect();
    app.unit = u;
    setStatus(`Connected: ${u.info.version.replace(/^FELUCCA /, "")}${MOCK ? " (simulated)" : ""}`, "good");
    $("connect").hidden = true; $("disconnect").hidden = false;
    startTheme(u);
    renderMain();
    if (u.info.menuCount != null && !u.menu) u.readMenu().then(() => app.refresh()).catch(() => {});
    if (u.compat) say(u.compat, "warn");
    else say("");
  } catch (e) {
    setStatus(`The unit did not answer (${e.message}). Check the USB cable and that no other page uses the FM-1.`, "warn");
    app.unit?.disconnect?.(); app.unit = null; $("connect").hidden = false; $("disconnect").hidden = true;
  } finally { connecting = false; $("connect").disabled = false; }
}
function lost(why) {
  stopTheme(); app.unit?.disconnect?.(); app.unit = null; setStatus(why, "warn"); $("connect").hidden = false; $("disconnect").hidden = true; renderMain();
}
guardPage(say);
$("connect").addEventListener("click", connect);
$("disconnect").addEventListener("click", () => { auto = false; lost("Disconnected."); });
$("vparts").addEventListener("click", () => setView("parts")); $("vglobal").addEventListener("click", () => setView("global"));
if (MOCK) {
  $("mockbar").hidden = false;
  $("mockturn").addEventListener("click", () => {
    const m = app.mock; if (!m) return;
    const part = m.selected, id = 45 + 0, d = L.TP[id], v = Math.max(d.min, Math.min(d.max, m.parts[part].p[id] > 40 ? 20 : 70));
    m.deviceTurn(part, id, v);
    say(`Simulated knob turn on the unit: PART ${part + 1} CUT = ${v}.`);
  });
  $("mockselect").addEventListener("click", () => { const m = app.mock; if (m) m.deviceSelect(1 - m.selected); });
  $("mockcolor").addEventListener("click", () => { const m = app.mock; if (!m) return; const n = L.PALETTES.LIST.length, i = ((app.pal ?? 0) + 1) % n; m.devicePalette(i); say(`Simulated MENU > COLOR on the unit: ${paletteName(i)} (the page follows within 2 s).`); });
} else if (!navigator.requestMIDIAccess) { setStatus("This browser cannot talk to the FM-1. Open this page in Chrome or Edge on a computer.", "warn"); $("connect").disabled = true; }
if (!MOCK && navigator.requestMIDIAccess) sysexGranted().then((ok) => { if (ok) connect(); else setStatus(HINT); });   // MIDI already allowed: connect by itself
$("backuplink").href = "./backup.html" + location.search;
setView("parts"); renderMain();
if (MOCK) connect();
export { app, edit };
