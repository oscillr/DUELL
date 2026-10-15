// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor (beta): the page. A fixed full-window layout of three columns: PART 1 | PART 2 | GLOBAL (the toggle can put GLOBAL in the middle), each with its own tab strip.
// LINK on: a tab choice on one part is applied to the other part too, and an edit moves both parts by the same step. Below 1500 px the global column is a view (PARTS / GLOBAL).
// The pages are made from layout.json (the firmware's own tables), the values come from the unit (GET / SET / WATCH) or from the simulated unit.
import { h, clear, ask, askText, notice } from "./ui_util.js";
import { Cell } from "./controls.js";
import { L, ID, P_E0, MASTER_TITLE, PART_TABS, GLOBAL_TABS, NOT_EDITABLE, SEQ_PARAMS, HEAD_PARAMS, descOf, gdesc, gdescOf, ginert, shimShown, dmdAfterMode, inertReason, dstWhy, dstName, pageByTitle, pageVisible, pageControls, matrixSlots, engineLabel, pickable, enumOrder, isEnum, clampTo } from "./layout.js";
import { Unit } from "./unit.js";
import { Link } from "./link.js";
import { themeVars, isDark, paletteName } from "./theme.js";
import { CMD, parseMenu } from "./proto.js";
import { buildPatches, buildMultis, buildSettings, buildMidi, buildSteps, buildWavetables } from "./panels.js";
import { handleFile } from "./patches_ui.js";
import { dropZone, guardPage, hasFiles, hasSlot, slotOf, filesOf } from "./dropzone.js";
import { PatternStore } from "./roll.js";
import { createRollView } from "./rollview.js";
import { sysexGranted, watchUnit, HINT } from "./autoconnect.js";

const params = new URLSearchParams(location.search), MOCK = params.get("mock");
const $ = (id) => document.getElementById(id);
let place = "right";   // where the GLOBAL column sits: "right" (PART 1 | PART 2 | GLOBAL, the default) or "middle" (PART 1 | GLOBAL | PART 2); remembered in localStorage
try { if (localStorage.getItem("duell-editor-global") === "middle") place = "middle"; } catch { /* no storage: the default */ }
const app = { unit: null, tabs: ["engine", "engine"], gtab: "global", view: "parts", linked: false, cells: [], sig: [], cols: [], mock: null, midi: null, refreshers: new Set(), target: 0, pal: null, stores: [], rollViews: [], overlay: null };
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
function reg(cell) { app.cells.push(cell); return cell; }
function partCell(p, id, opt = {}) {
  const u = () => app.unit;
  return reg(new Cell({
    desc: () => descOf(u().parts[p], id),
    value: () => u().parts[p].p[id],
    set: (v) => edit(p, id, v),
    inert: () => inertReason(u().parts[p], id),
  }, opt));
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
  return reg(new Cell({ desc: () => gdescOf(app.unit.g, id), value: () => app.unit.g[id], set: (v) => guardWrite(() => writeGlobalMode(id, v)), inert: () => ginert(app.unit.g, id) }));
}
function guardWrite(fn) { try { return fn(); } catch (e) { app.fail(e); } }
// an edit of part p (and of the other part too when LINK is on)
function edit(p, id, v) {
  const u = app.unit; if (!u) return;
  guardWrite(() => (app.linked ? u.writeParamLinked(p, id, v) : u.writeParam(p, id, v)));
}
const card = (title, sub, cells, cls = "") => h("section", { class: "card " + cls }, h("h3", {}, title, sub ? h("small", { text: " " + sub }) : null), h("div", { class: "grid" }, cells.map((c) => c.el)));
const cellsOf = (p, ids) => ids.filter((i) => i >= 0).map((id) => partCell(p, id));

// ---- the tabs of one part
function pageCard(p, title) {
  const part = app.unit.parts[p], pg = pageByTitle(title);
  if (!pg || !pageVisible(pg, part.eng)) return null;
  return { pg, ids: pageControls(pg, part).map((c) => c.id) };
}
// one card from several firmware pages (ids already shown in this tab are left out); null when nothing is left
function pagesCard(p, name, titles, seen, sub = "") {
  const ids = [];
  for (const t of titles) { const c = pageCard(p, t); if (c) for (const id of c.ids) if (!seen.has(id)) { seen.add(id); ids.push(id); } }
  return ids.length ? card(name, sub, cellsOf(p, ids)) : null;
}
const OWN_FILTERS = ["VCF", "TRIO FILTER", "NOISE FILT"];
function buildPartTab(p, tab) {
  const u = app.unit, part = u.parts[p], e = L.ENG[part.eng], out = h("div", { class: "tabbody" + (tab === "matrix" ? " matrix" : "") });
  const add = (el) => el && out.append(el), seen = new Set();
  if (tab === "engine") {
    const own = OWN_FILTERS.find((t) => pageCard(p, t));
    const ownIds = new Set(own ? pageCard(p, own).ids : []);
    if (!e || e.name === "-") add(h("p", { class: "note", text: "This part plays a retired engine: it is silent and keeps its values. Pick an engine above." }));
    else for (const k of [0, 4]) {
      const ids = [0, 1, 2, 3].map((j) => P_E0 + (e.uiord ? e.uiord[k + j] : k + j));   // the cards in the unit's order (FM6: SLOT first; the ids are the stored E slots)
      ids.forEach((id) => seen.add(id));
      add(card(e.titles[k / 4] || "EDIT " + (k / 4 + 1), ids.some((id) => ownIds.has(id)) ? `includes the engine's own filter (${own})` : "", cellsOf(p, ids)));
    }
    add(pagesCard(p, "VOICE", ["VOICE 1/2", "VOICE 2/2"], seen));
    add(pagesCard(p, "FILTER", ["MULTI FILTER"], seen, e?.ufilt === 2 ? "multimode filter, in series after the engine's own" : "multimode filter (this engine has none of its own)"));
    if (own) add(pagesCard(p, own, [own], seen, "this engine's own filter"));
    if (e?.name === "FM6") add(h("p", { class: "note", text: "FM6's operator pages (OP TUNE, OP ENV, OP MODE) are not editable yet; the eight macros above are." }));
  } else if (tab === "mod") {
    add(pagesCard(p, "AMP ENV", ["AMP ENV", "AMP DEST"], seen));
    add(pagesCard(p, "ENV 2", ["ENV 2", "ENV 2 DEST"], seen));
    add(pagesCard(p, "LFO 1", ["LFO 1", "LFO 1 TRIG", "LFO 1 DEST"], seen));
    add(pagesCard(p, "LFO 2", ["LFO 2", "LFO 2 TRIG", "LFO 2 DEST"], seen));
  } else if (tab === "matrix") {
    for (const title of ["MATRIX 1-4", "MATRIX 5-8"]) add(matrixCard(p, pageByTitle(title, "LFO")));
  } else if (tab === "fx") {
    add(pagesCard(p, "SAT AND SENDS", ["FX"], seen, "DST CHO DLY REV: the sends feed the buses in the GLOBAL column"));
    add(pagesCard(p, "INSERT", ["INSERT 1/2", "INSERT 2/2"], seen));
    add(pagesCard(p, "EQ", ["EQ 1/2", "EQ 2/2"], seen));
    add(pagesCard(p, "DUCK", ["DUCK 1/2", "DUCK 2/2"], seen));
  } else if (tab === "arpseq") {
    add(pagesCard(p, "ARP", ["ARP 1/3", "ARP 2/3", "ARP 3/3"], seen));
    add(pagesCard(p, "CHORD", ["CHORD"], seen));
    add(card("SEQUENCER", "of this part", cellsOf(p, SEQ_PARAMS)));
    add(buildSteps(app, p));
  } else if (tab === "roll") {
    out.classList.add("rolltab");
    out.append(buildRollTab(p));
  }
  return out;
}

// ---- the piano roll (roll.js: model, undo, live writes; rollview.js: the picture). One PatternStore per part, shared by the tab and the full-width overlay.
function getStore(p) {
  if (!app.stores[p] || app.stores[p].unit !== app.unit) app.stores[p] = new PatternStore(app.unit, p);
  return app.stores[p];
}
// a cell of the roll's own toolbar: the view refreshes it (not app.cells); inertFn: the roll's hide rule for the controls that do not apply in the arp's SEQ mode
const rollCell = (p) => (id, inertFn) => new Cell({
  desc: () => descOf(app.unit.parts[p], id), value: () => app.unit.parts[p].p[id], set: (v) => edit(p, id, v),
  inert: () => (inertFn ? inertFn() : inertReason(app.unit.parts[p], id)),
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
      const pane = h("section", { class: "ovpane", "aria-label": `Part ${q + 1}` }, h("div", { class: "ovtitle" }, h("b", { text: `PART ${q + 1}` }), h("span", { class: "small", text: `${engineLabel(app.unit.parts[q].eng)}  ${soundName(q)}` })), v.el);
      themeInto(pane, q); body.append(pane);
    }
    views.forEach((v) => v.attach());
    for (const [k, b] of Object.entries(modeBtns)) b.setAttribute("aria-pressed", String(k === m));
    views[0]?.focus();
  };
  for (const [k, t] of [["0", "PART 1"], ["1", "PART 2"], ["both", "BOTH"]]) modeBtns[k] = h("button", { type: "button", class: "ghost small", text: t, onclick: () => build(k) });
  el.append(h("div", { class: "ovhead" }, h("b", { text: "PIANO ROLL" }), h("span", { class: "small", text: "pattern of" }), h("span", { class: "tg", role: "group", "aria-label": "Part" }, Object.values(modeBtns)),
    h("span", { class: "small", text: "Esc closes. Edits go to the unit at once unless Send live is off." })), body);
  document.body.append(el); placeOverlay(); $("main").inert = true;
  build(ov.mode);
  refreshAll();
}
function closeOverlay(quiet) {
  const ov = app.overlay; if (!ov) return;
  ov.views.forEach((v) => { v.destroy(); app.refreshers.delete(v.refresh); });
  ov.el.remove(); app.overlay = null; $("main").inert = false;
  if (!quiet) {
    for (let q = 0; q < 2; q++) if (app.cols[q] && app.tabs[q] === "arpseq") fill(q);   // the STEPS card there reads the pattern again
    refreshAll();
    app.cols.slice(0, 2).forEach((c) => c?.col.querySelector(".roll .rgrid")?.focus());
  }
}
app.edit = edit; app.openOverlay = openOverlay; app.closeOverlay = closeOverlay;
document.addEventListener("keydown", (ev) => { if (app.overlay && ev.key === "Escape" && !ev.defaultPrevented) { ev.preventDefault(); closeOverlay(); } });
window.addEventListener("resize", placeOverlay);
// the modulation matrix: a row per slot
function matrixCard(p, pg) {
  const u = app.unit;
  const rows = matrixSlots(pg).map((s) => {
    const cs = partCell(p, s.src), cd = reg(new Cell({
      desc: () => descOf(u.parts[p], s.dst), value: () => u.parts[p].p[s.dst], set: (v) => edit(p, s.dst, v),
      inert: () => dstWhy(u.parts[p], u.parts[p].p[s.dst]), optLabel: (v) => dstName(u.parts[p], v),
    }, { soft: true })), ca = partCell(p, s.amt);
    return h("div", { class: "mrow" }, h("span", { class: "mn", text: s.n }), cs.el, cd.el, ca.el);
  });
  return h("section", { class: "card" }, h("h3", { text: pg.title }), rows);
}

// ---- the global column
function buildGlobalTab(wrap) {
  const gcard = (title, ids, sub) => card(title, sub, ids.filter((i) => i >= 0).map((id) => globalCell(id)));
  const note = (t) => h("p", { class: "note", text: t });
  const tab = app.gtab;
  if (tab === "global") {
    const pgs = (t) => pageByTitle(t).ids;
    // SHIMMER: the unit's REVERB page 2, in its page cycle only while TYPE is SHIM; here the card is left out the same way
    const shimmer = gcard("SHIMMER", pgs("SHIMMER"), "PITCH of the shimmered return (UP, DOWN, BOTH) and MOD, a slow drift of its pitch; with TYPE SHIM only");
    app.refreshers.add(() => { shimmer.style.display = shimShown(app.unit?.g) ? "" : "none"; });
    // the unit's master-stage page (GLO, 0.2.19; titled BASS MONO while it has the one card), K1 BASS MONO. The unit gives it as MENU setting 33 (tab MASTER: no MENU tab); the firmware's page table holds the card (L.MASTER)
    let bmDesc;   // the unit's short card label is BMONO; the editor has the room for the name
    const bmItem = () => app.unit?.menu?.find((m) => m.id === 33);
    if (!app.unit.menu && app.unit.info.menuCount != null) app.unit.readMenu().then(() => app.refresh()).catch(() => {});
    const bmCell = reg(new Cell({ desc: () => (bmDesc ??= { ...L.MASTER, label: "BASS MONO" }), value: () => bmItem()?.value ?? 0, set: (v) => guardWrite(async () => { await app.unit.setMenu(33, v); app.refresh(); }), inert: () => (bmItem() ? null : "NOT IN THIS FIRMWARE") }));
    const master = card(MASTER_TITLE, "below this frequency the output is mono, above it stereo; the unit does not use it in DUEL mode (part 1 left, part 2 right)", [bmCell]);
    const mix = h("section", { class: "card" }, h("h3", { text: "MIXER" }), [0, 1].map((p) => h("div", { class: "mixrow" }, h("b", { text: `PART ${p + 1}` }), HEAD_PARAMS.map((id) => partCell(p, id).el))));
    wrap.append(h("div", { class: "tabbody" }, gcard("CHORUS", pgs("CHORUS"), "bus"), gcard("DELAY", [...pgs("DELAY 1/2"), ...pgs("DELAY 2/2")], "bus; MODE first, MIX last; RANDOM / OCT UP make MDEPTH the CHANCE of an event"), gcard("REVERB", pgs("REVERB"), "bus"), shimmer, master, mix,
      note("The buses are shared by both parts (one set for the instrument). A part's matrix routes (CHO, DLY, REV, FDBK, RSIZE ...) move them while a note sounds; what you set here is the stored value. CLK, TEMPO, SWG and TUNE are at the top."),
      h("p", { class: "note" }, h("b", { text: "Not editable here yet: " }), "MODE (DUEL / STEREO / LAYER), BALANCE, XFADE, SPLIT, the MORPH fader and its ROLL / UNDO, SCALE / ROOT / OFF-SCALE, the parts' MIDI channels (no editor protocol command).")));
  } else if (tab === "midi") buildMidi(app, wrap, globalCell, card);
  else if (tab === "patches") buildPatches(app, wrap);
  else if (tab === "multis") buildMultis(app, wrap);
  else if (tab === "tables") buildWavetables(app, wrap);
  else if (tab === "settings") buildSettings(app, wrap);
}

// ---- a column: header (PART n, engine, sound, level, pan, mute), a tab strip and the body of the tab
function buildColumn(p) {
  const u = app.unit, part = u.parts[p], col = h("section", { class: "col part", "aria-label": `Part ${p + 1}`, dataset: { part: p } });
  const engSel = h("select", { "aria-label": `Part ${p + 1} engine`, onchange: (ev) => changeEngine(p, +ev.target.value, ev.target) },
    pickable(part.eng).map((e) => h("option", { value: e, text: engineLabel(e) === "-" ? "(retired engine)" : engineLabel(e) })));
  engSel.value = part.eng;
  const sel = h("span", { class: "seldot", title: "This is the part the unit has selected (ALGORITHM)" });
  const snd = h("span", { class: "snd" });
  const head = h("div", { class: "colhead" },
    h("div", { class: "rowhead" }, h("b", { text: `PART ${p + 1}` }), sel, engSel, snd,
      h("button", { class: "ghost small", text: "Sounds...", title: "Browse and load patches into this part (the PATCHES tab of the GLOBAL column)", onclick: () => { app.target = p; setGlobalTab("patches", true); } })),
    h("div", { class: "grid3" }, HEAD_PARAMS.map((id) => partCell(p, id).el)));
  const strip = h("nav", { class: "tabs", role: "tablist", "aria-label": `Part ${p + 1} tabs` });
  const body = h("div", { class: "colbody" });
  col.append(head, strip, body);
  app.cols[p] = { col, body, strip, engSel, sel, snd };
  // a patch file, a DX7 .syx or a slot dropped on the part: it is loaded into this part, live, and not saved anywhere
  dropZone(col, { accept: (e) => hasFiles(e) || hasSlot(e), onDrop: async (e) => {
    if (hasFiles(e)) { for (const f of filesOf(e)) await handleFile(app, f, { part: p }); return; }
    const slot = slotOf(e); if (slot == null) return;
    try { app.say(`Loading slot #${slot + 1} into PART ${p + 1} ...`); await u.loadSlot(p, slot); u.parts[p].name = u.slots?.[slot]?.name ?? null; app.refresh(); app.say(`#${slot + 1} ${u.slots?.[slot]?.name ?? ""} is loaded in PART ${p + 1} (not saved).`, "good"); } catch (err) { app.fail(err); }
  } });
  return col;
}
function buildGlobalColumn() {
  const col = h("section", { class: "col glob", "aria-label": "Global" });
  const head = h("div", { class: "colhead" }, h("div", { class: "rowhead" }, h("b", { text: "GLOBAL" }), h("span", { class: "snd", text: "both parts: buses, clock, library" })),
    h("div", { class: "grid4" }, [2, 0, 1, 3].map((id) => globalCell(id).el)));   // CLK, TEMPO, SWG, TUNE: the clock source first, as the unit's TEMPO page (0.2.18)
  const strip = h("nav", { class: "tabs", role: "tablist", "aria-label": "Global tabs" });
  const body = h("div", { class: "colbody" });
  col.append(head, strip, body);
  app.cols[2] = { col, body, strip };
  // a patch file, a DX7 .syx or a WAV dropped anywhere on the GLOBAL column goes to the library (the PATCHES and WAVETABLES tabs take it themselves)
  dropZone(col, { accept: hasFiles, onDrop: async (e) => { for (const f of filesOf(e)) await handleFile(app, f, { start: app.patchStart ?? 0 }); } });
  return col;
}
function soundName(p) {
  const part = app.unit.parts[p];
  if (part.name) return part.name;
  const s = L.ENG[part.eng]?.sounds[part.preset];
  return s ? s[0] : (L.ENG[part.eng]?.name === "-" ? "" : "INIT");
}
async function changeEngine(p, eng, selectEl) {
  const u = app.unit, was = u.parts[p].eng;
  if (eng === was) return;
  if (!(await ask(`PART ${p + 1}: ${engineLabel(eng)}`, [`Changing the engine replaces the sound of PART ${p + 1} with ${engineLabel(eng)}'s first sound, as on the unit. The sound is not stored anywhere: save it first if you want it back.`], "Change engine"))) { selectEl.value = was; return; }
  try { u.parts[p].name = null; await u.setEngine(p, eng); } catch (e) { app.fail(e); selectEl.value = u.parts[p].eng; }
}

// ---- tabs
const tabsOf = (i) => (i === 2 ? GLOBAL_TABS : PART_TABS);
const tabKey = (i) => (i === 2 ? app.gtab : app.tabs[i]);
function renderStrip(i) {
  const c = app.cols[i]; if (!c) return;
  clear(c.strip).append(...tabsOf(i).map((t) => h("button", { role: "tab", class: "tab", "aria-selected": String(t.key === tabKey(i)), tabindex: t.key === tabKey(i) ? 0 : -1, dataset: { key: t.key }, text: t.label,
    onclick: () => (i === 2 ? setGlobalTab(t.key) : setPartTab(i, t.key)) })));
}
document.addEventListener("keydown", (ev) => {
  const strip = ev.target.closest?.(".tabs"); if (!strip) return;
  const tabs = [...strip.querySelectorAll("[role=tab]")], i = tabs.indexOf(document.activeElement);
  if (i < 0) return;
  const j = ev.key === "ArrowRight" ? (i + 1) % tabs.length : ev.key === "ArrowLeft" ? (i + tabs.length - 1) % tabs.length : ev.key === "Home" ? 0 : ev.key === "End" ? tabs.length - 1 : -1;
  if (j >= 0) { ev.preventDefault(); tabs[j].click(); strip.querySelectorAll("[role=tab]")[j].focus(); }
});
// LINK on: the other part follows the tab
export function setPartTab(p, key) {
  const ps = app.linked ? [0, 1] : [p];
  for (const q of ps) { if (!app.cols[q]) continue; app.tabs[q] = key; renderStrip(q); fill(q); }
  refreshAll();
}
export function setGlobalTab(key, showView = false) {
  app.gtab = key; renderStrip(2); fill(2);
  if (showView) setView("global");
  refreshAll();
}
app.setPartTab = setPartTab; app.setGlobalTab = setGlobalTab;

function signature(p) { const q = app.unit.parts[p]; if (app.tabs[p] === "roll") return "roll"; return `${q.eng}|${app.tabs[p] === "fx" ? Math.min(q.p[ID.P_DKSRC], 2) : ""}`; }
function fill(i) {
  const c = app.cols[i]; if (!c) return;
  app.cells = app.cells.filter((x) => x.partBody !== i);
  if (i === 2) for (const f of [...app.refreshers]) if (f.col === 2) app.refreshers.delete(f);
  const before = app.cells.length;
  if (i === 2) { const wrap = h("div", { class: "gbody" }); buildGlobalTab(wrap); c.body.replaceChildren(wrap); }
  else {
    if (app.tabs[i] !== "roll" && app.rollViews[i]) { app.rollViews[i].destroy(); app.refreshers.delete(app.rollViews[i].refresh); app.rollViews[i] = null; }
    c.body.replaceChildren(buildPartTab(i, app.tabs[i])); app.sig[i] = signature(i);
    if (app.tabs[i] === "roll") app.rollViews[i]?.attach();
  }
  app.cells.slice(before).forEach((x) => { x.partBody = i; });
  c.body.scrollTop = 0;
  const t = tabsOf(i).find((x) => x.key === tabKey(i)); document.title = `DUELL editor: ${i === 2 ? "GLOBAL " : "PART " + (i + 1) + " "}${t?.label ?? ""}`;
}

function renderMain() {
  closeOverlay(true); dropRollViews(); app.stores = [];
  const main = clear($("main"));
  app.cells = []; app.cols = []; app.refreshers.clear(); app.sig = [];
  main.classList.toggle("empty", !app.unit);
  if (!app.unit) { main.append(h("div", { class: "welcome" }, h("p", { text: "Connect the FM-1 (button at the top right) to read both parts and edit them live." }),
    h("p", { class: "small", text: "Nothing is sent to the unit until you change something. Backup and restore are on their own page." }))); applyTheme(null); return; }
  const c0 = buildColumn(0), c1 = buildColumn(1), cg = buildGlobalColumn();
  main.dataset.place = place; main.dataset.view = app.view;
  main.append(...(place === "right" ? [c0, c1, cg] : [c0, cg, c1]));
  for (let i = 0; i < 3; i++) { renderStrip(i); fill(i); }
  applyTheme(app.pal);
  refreshAll();
}
function placeColumns() {
  const main = $("main"); if (!app.cols[2]) return;
  main.dataset.place = place;
  main.append(...(place === "right" ? [app.cols[0].col, app.cols[1].col, app.cols[2].col] : [app.cols[0].col, app.cols[2].col, app.cols[1].col]));
  $("placeglobal").textContent = `GLOBAL: ${place}`;
}
function setView(v) { app.view = v; $("main").dataset.view = v; $("vparts").setAttribute("aria-pressed", v === "parts"); $("vglobal").setAttribute("aria-pressed", v === "global"); }
function refreshAll() {
  const u = app.unit; if (!u) return;
  for (let p = 0; p < 2 && app.cols[p]; p++) {
    if (app.sig[p] !== signature(p)) fill(p);
    const c = app.cols[p];
    c.engSel.value = u.parts[p].eng;
    if (![...c.engSel.options].some((o) => +o.value === u.parts[p].eng)) { c.engSel.append(h("option", { value: u.parts[p].eng, text: "(retired engine)" })); c.engSel.value = u.parts[p].eng; }
    c.sel.classList.toggle("on", u.selected === p);
    c.snd.textContent = soundName(p);
  }
  for (const c of app.cells) c.refresh();
  for (const f of app.refreshers) f();
  const w = u.writable;
  $("main").classList.toggle("readonly", !w);
  if (!w) say(u.compat, "warn");
}
let raf = 0;
app.refresh = () => { if (!raf) raf = requestAnimationFrame(() => { raf = 0; refreshAll(); }); };

// ---- the unit's colour (MENU > COLOR, id 0 = the palette index of the firmware's UI_PALETTES, layout.json)
function applyTheme(i) {
  const root = document.documentElement.style;
  const known = ["--bg", "--fg", "--dim", "--line", "--panel", "--acc", "--accfg", "--page", "--warn", "--ok"];
  const set = (el, vars) => { for (const k of known) el.setProperty(k, vars[k]); };
  const unset = (el) => { for (const k of known) el.removeProperty(k); };
  if (i == null) { unset(root); root.removeProperty("color-scheme"); document.documentElement.dataset.scheme = matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light"; $("palname").textContent = ""; return; }
  set(root, themeVars(i, 2)); root.setProperty("color-scheme", isDark(i) ? "dark" : "light"); document.documentElement.dataset.scheme = isDark(i) ? "dark" : "light";
  for (let k = 0; k < 3; k++) if (app.cols[k]) set(app.cols[k].col.style, themeVars(i, k));
  $("palname").textContent = `colour: ${paletteName(i)}`;
}
function setPalette(i) {
  if (!Number.isInteger(i) || i === app.pal) return;
  app.pal = i; applyTheme(i);
}
let themeTimer = 0;
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
  u.on("menu", (items) => { const c = items?.find((m) => m.id === 0); if (c) setPalette(c.value); })
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
      app.mock = new MockUnit({ caps: MOCK === "old" ? 3 : 31, playing: MOCK === "playing", patchRaw: MOCK !== "old", version: MOCK === "old" ? "FELUCCA DUELL 0.2.13 bet" : undefined });
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
    if (u.compat) say(u.compat, "warn");
    else say("Connected. Turn a knob on the unit and the page follows; move a slider here and the unit follows.", "good");
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
$("link").addEventListener("change", (e) => {
  app.linked = e.target.checked;
  $("linknote").textContent = app.linked ? "LINK: a tab choice goes to both parts and a change on one part moves the other by the same step. Engine pages follow only while both parts play the same engine." : "";
  if (app.linked && app.unit && app.tabs[1] !== app.tabs[0]) { app.tabs[1] = app.tabs[0]; renderStrip(1); fill(1); refreshAll(); }
});
$("placeglobal").addEventListener("click", () => { place = place === "right" ? "middle" : "right"; try { localStorage.setItem("duell-editor-global", place); } catch { /* not remembered */ } placeColumns(); });
$("vparts").addEventListener("click", () => setView("parts")); $("vglobal").addEventListener("click", () => setView("global"));
$("notyet").addEventListener("click", () => notice("Not editable yet", [
  h("p", { text: "What this version edits: both parts' engine, every sound page (engine, voice, filters, envelopes, LFOs, the 8-slot matrix, FX sends, insert, EQ, duck, arp, chord), the sequencer settings and steps, the buses, tempo and tuning, MIDI IN routing and the MIDI learn map, patches (the 128 slots: browse, load, save over a slot, rename, restore, and the library: patch files, DX7 .syx import, drag and drop, copy / move / swap), multis (load, save, rename) and the MENU settings (with the MODULES switches and PERF LEAVE). Backup and restore are on the Backup page." }),
  h("p", { class: "small", text: "What it cannot edit yet, and why:" }),
  h("ul", {}, NOT_EDITABLE.map(([t, why]) => h("li", {}, h("b", { text: t + ": " }), why)),
    h("li", {}, h("b", { text: "Patch category and pattern: " }), "a saved patch keeps the category the unit gives it; its pattern is not edited here (UP_STORE saves the part's own sound over a slot, which is lossless; a patch file is written with PATCH_RAW, also lossless; UP_PUT, which drops End B and the arp block, is used only for the copy of a factory sound). Restore (UP_ERASE) takes a saved sound out of its slot and the factory sound comes back."),
    h("li", {}, h("b", { text: "BOTH on the unit's edit pages: " }), "the unit has BOTH only on MORPH and PERFORM; the LINK switch here is the editor's own version of it (the same step on both parts).")),
]));
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
setView("parts"); $("placeglobal").textContent = `GLOBAL: ${place}`; renderMain();
if (MOCK) connect();
export { app, edit };
