// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the PATCHES tab, the patch LIBRARIAN. The 128 slots as a compact list that fits the global column and a wide grid (an overlay over the page, for dragging
// between slots); patch files and DX7 sysex files dropped on the tab (or chosen with a button) open a preview with the target slots; a slot dragged onto another is copied
// (the list) or swapped (the wide grid); a file or a slot dropped on a part loads there without saving (app.js). Every write is listed before it is made; the work is in
// librarian.js. Rules of the owner's UI: no counts of free places (every slot is always writable), a control that does not apply is not drawn, one text style.
import { h, clear, ask, askText } from "./ui_util.js";
import { L, engineLabel } from "./layout.js";
import { CATEGORY_NAMES as CATEGORIES } from "./labels.js";
import { UP_SLOTS } from "./unit.js";
import * as LIB from "./librarian.js";
import { parseFile, sniff, fileName, itemFromRecord } from "./patchfile.js";
import { parseDx7File, voiceToPatch, SOURCE_LABEL } from "./dx7import.js";
import { dropZone, hasFiles, hasSlot, slotOf, filesOf, readBytes, saveText, SLOT_TYPE } from "./dropzone.js";

const label = (slot) => `#${slot + 1}`;
const pad3 = (n) => String(n + 1).padStart(3, " ");
const partSel = (app, name = "Part") => {
  const s = h("select", { "aria-label": name }, h("option", { value: 0, text: "PART 1" }), h("option", { value: 1, text: "PART 2" }));
  s.value = app.target; s.addEventListener("change", () => { app.target = +s.value; });
  return s;
};

// ---------------------------------------------------------------- a dialog that asks for a slot number and shows what would change
function askSlot(app, { title, intro, value, okLabel, describe }) {
  return new Promise((resolve) => {
    const dlg = h("dialog", { class: "ask", "aria-label": title });
    const input = h("input", { type: "number", min: 1, max: UP_SLOTS, value: String(value + 1), "aria-label": "Target slot number" });
    const list = h("ul", { class: "effects", "aria-live": "polite" });
    const ok = h("button", { text: okLabel });
    const done = (v) => { dlg.close(); dlg.remove(); resolve(v); };
    const upd = () => {
      const n = +input.value - 1, d = Number.isInteger(n) && n >= 0 && n < UP_SLOTS ? describe(n) : { lines: ["Pick a slot from 1 to 128."], bad: true };
      clear(list).append(...d.lines.map((t) => h("li", { text: t })));
      ok.disabled = !!d.bad; ok.classList.toggle("danger", !!d.danger);
    };
    input.addEventListener("input", upd);
    input.addEventListener("keydown", (e) => { if (e.key === "Enter" && !ok.disabled) done(+input.value - 1); });
    ok.addEventListener("click", () => done(+input.value - 1));
    dlg.append(h("h2", { text: title }), ...[].concat(intro).map((t) => h("p", { text: t })), h("label", { class: "row" }, h("span", { text: "Target slot #" }), input), list,
      h("div", { class: "row end" }, h("button", { class: "ghost", text: "Cancel", onclick: () => done(null) }), ok));
    dlg.addEventListener("cancel", (e) => { e.preventDefault(); done(null); });
    document.body.append(dlg); upd(); dlg.showModal(); input.select();
  });
}

// ---------------------------------------------------------------- the import preview: a list of patches (a file's, or a DX7 bank's voices) and where each goes
// o: { title, source, items: [item | {damaged, name}], notes: [text], problems: [text], start (0-based slot or null), numbers (the items carry slot numbers) }
export async function importDialog(app, o) {
  const u = app.unit;
  if (!u.slots) await u.listSlots();
  return new Promise((resolve) => {
    const dlg = h("dialog", { class: "ask wide imp", "aria-label": o.title });
    const rows = o.items.map((it, i) => ({ it, i, on: !it.damaged && !it.error, manual: null, slotInput: null }));
    const withNumbers = o.numbers && rows.some((r) => r.it.number != null);
    const st = { mode: withNumbers ? "same" : "from", start: o.start ?? 0, skip: true, safety: true, running: false };
    const body = h("div", { class: "impbody" }), foot = h("div", { class: "row end" });
    dlg.append(h("h2", { text: o.title }), body, foot);
    const close = (v) => { dlg.close(); dlg.remove(); resolve(v); };
    dlg.addEventListener("cancel", (e) => { e.preventDefault(); if (!st.running) close(null); });

    const chosen = () => rows.filter((r) => r.on);
    function targets() {                                        // [slot | null] for the chosen rows; a slot typed for a row stays, the others keep their places
      const ch = chosen(), auto = LIB.chooseTargets(u, ch.map((r) => r.it), { mode: st.mode === "same" ? "same" : "from", start: st.start, skipSaved: st.skip });
      return ch.map((r, k) => (r.manual != null ? r.manual : auto[k]));
    }
    function plan() { const ch = chosen(), t = targets(); return { ch, t, plan: LIB.makePlan(u, ch.map((r) => r.it), t) }; }

    function draw() {
      clear(body);
      const { ch, t, plan: pl } = plan();
      body.append(h("p", { class: "small", text: o.source }));
      for (const n of o.notes || []) body.append(h("p", { class: "small", text: n }));
      for (const n of o.problems || []) body.append(h("p", { class: "warn small", text: n }));
      const approx = rows.filter((r) => r.on && r.it.kind === "approx").length;
      if (approx) body.append(h("p", { class: "small", text: `${approx} of these ${approx > 1 ? "are" : "is"} a copy of a factory sound's values (its arp block and pattern are not part of it).` }));
      // where they go
      const modeSel = h("select", { "aria-label": "Where the patches go", onchange: (e) => { st.mode = e.target.value; rows.forEach((r) => { r.manual = null; }); draw(); } },
        withNumbers ? h("option", { value: "same", text: "The same slot numbers as in the file" }) : null, h("option", { value: "from", text: "One after the other, from the slot ..." }));
      modeSel.value = st.mode;
      const start = h("input", { type: "number", min: 1, max: UP_SLOTS, value: String(st.start + 1), "aria-label": "First slot", hidden: st.mode !== "from",
        oninput: (e) => { const n = +e.target.value - 1; if (n >= 0 && n < UP_SLOTS) { st.start = n; rows.forEach((r) => { r.manual = null; }); update(); } } });
      const skip = h("label", { class: "chk", hidden: st.mode !== "from" }, h("input", { type: "checkbox", checked: st.skip, onchange: (e) => { st.skip = e.target.checked; rows.forEach((r) => { r.manual = null; }); draw(); } }), " skip slots that hold a saved patch");
      body.append(h("div", { class: "row" }, modeSel, start, skip));
      // the list
      const all = h("input", { type: "checkbox", "aria-label": "All patches", checked: rows.filter((r) => !r.it.damaged && !r.it.error).every((r) => r.on), onchange: (e) => { rows.forEach((r) => { if (!r.it.damaged && !r.it.error) r.on = e.target.checked; }); draw(); } });
      const tbl = h("div", { class: "imptable", role: "table" }, h("div", { class: "imphead", role: "row" }, all, h("span", { text: "Patch" }), h("span", { text: "Engine" }), h("span", { text: "Category" }), h("span", { text: "Slot" }), h("span", { text: "Replaces" })));
      let k = 0;
      for (const r of rows) {
        const it = r.it, idx = r.on ? k++ : -1, slot = idx >= 0 ? t[idx] : null, now = slot != null ? LIB.describeSlot(u, slot) : null;
        const chk = h("input", { type: "checkbox", checked: r.on, disabled: !!(it.damaged || it.error), "aria-label": `Import ${it.name}`, onchange: (e) => { r.on = e.target.checked; draw(); } });
        const sl = h("input", { type: "number", min: 1, max: UP_SLOTS, value: slot != null ? String(slot + 1) : "", disabled: !r.on, "aria-label": `Slot for ${it.name}`,
          onchange: (e) => { const n = +e.target.value - 1; r.manual = n >= 0 && n < UP_SLOTS ? n : null; draw(); } });
        const rep = !r.on ? "" : slot == null ? "no slot" : `${now.label} ${now.saved ? `"${now.name}" (saved)` : `"${now.name}" (factory)`}`;
        tbl.append(h("div", { class: "improw" + (r.on ? "" : " off"), role: "row" }, chk,
          h("span", { class: "n", text: it.damaged ? `${it.name || "?"} (damaged in the file)` : it.error ? `${it.name} (${it.error})` : it.name, title: it.number != null ? `from slot ${label(it.number - 1)}` : "" }),
          h("span", { text: it.engineName || "" }), h("span", { text: it.catName ? it.catName + (it.catGuessed ? " (guess)" : "") : "" }), sl, h("span", { class: "rep" + (rep && now?.saved ? " lose" : ""), text: rep })));
      }
      body.append(tbl);
      const lost = pl.savedLost;
      body.append(h("p", { class: "sum", "aria-live": "polite", text: ch.length ? `${pl.writes.length} patch${pl.writes.length === 1 ? "" : "es"} will be written.${lost ? ` ${lost} saved patch${lost === 1 ? "" : "es"} will be replaced.` : ""}` : "Nothing is selected." }));
      for (const p of pl.problems) body.append(h("p", { class: "warn small", text: p }));
      if (pl.writes.length >= 4 && lost) {
        body.append(h("label", { class: "chk" }, h("input", { type: "checkbox", checked: st.safety, onchange: (e) => { st.safety = e.target.checked; } }),
          ` Save a safety copy of the ${lost} saved patch${lost === 1 ? "" : "es"} that will be replaced (a file is downloaded before anything is written)`));
      }
      body.append(h("p", { class: "small", text: "The unit stops playback to write (about a second a patch). Each patch is read back after it is written." }));
      clear(foot).append(h("button", { class: "ghost", text: "Cancel", onclick: () => close(null) }),
        h("button", { class: lost ? "danger" : "", id: "impgo", disabled: !pl.writes.length || pl.problems.length > 0, text: !pl.writes.length ? "Import" : lost ? `Replace ${lost} and import ${pl.writes.length}` : `Import ${pl.writes.length}`, onclick: () => run(plan().plan) }));
    }
    function update() {                                         // only the numbers (typing a start slot must not lose the focus)
      const { t } = plan(); let k = 0;
      const rowsEl = body.querySelectorAll(".improw");
      rows.forEach((r, i) => {
        const el = rowsEl[i]; if (!el) return;
        const slot = r.on ? t[k++] : null, now = slot != null ? LIB.describeSlot(u, slot) : null;
        const inp = el.querySelector("input[type=number]"); if (inp) inp.value = slot != null ? String(slot + 1) : "";
        const rep = el.querySelector(".rep"); rep.textContent = !r.on ? "" : slot == null ? "no slot" : `${now.label} ${now.saved ? `"${now.name}" (saved)` : `"${now.name}" (factory)`}`;
        rep.classList.toggle("lose", !!(now && now.saved));
      });
      const { plan: pl } = plan(), lost = pl.savedLost, go = foot.querySelector("#impgo");
      body.querySelector(".sum").textContent = chosen().length ? `${pl.writes.length} patch${pl.writes.length === 1 ? "" : "es"} will be written.${lost ? ` ${lost} saved patch${lost === 1 ? "" : "es"} will be replaced.` : ""}` : "Nothing is selected.";
      go.disabled = !pl.writes.length || pl.problems.length > 0; go.className = lost ? "danger" : "";
      go.textContent = !pl.writes.length ? "Import" : lost ? `Replace ${lost} and import ${pl.writes.length}` : `Import ${pl.writes.length}`;
    }
    async function run(pl) {
      st.running = true; clear(foot);
      const bar = h("progress", { max: pl.writes.length, value: 0, "aria-label": "Progress" }), line = h("p", { class: "small", role: "status", "aria-live": "polite", text: "Starting ..." });
      clear(body).append(h("p", { text: "Writing to the unit. Do not unplug it." }), bar, line);
      let res;
      try {
        res = await LIB.runPlan(u, pl, { safety: st.safety ? async (text, items) => { const name = `duell-safety-copy-${items.length}-patches.duell-patches.json`; saveText(app, name, text); line.textContent = `A safety copy was saved as ${name}.`; } : null,
          onProgress: (p) => { if (p.phase === "write") { bar.value = p.index; line.textContent = `${p.index + 1} of ${p.total}: ${label(p.slot)} ${p.name}`; } else bar.value = bar.max; } });
      } catch (e) { res = { done: [], failed: { error: e }, mismatched: [] }; }
      st.running = false;
      const lines = [];
      lines.push(res.failed ? `${res.done.length} of ${pl.writes.length} patches were written. Stopped at ${res.failed.slot != null ? label(res.failed.slot) : "the start"}: ${res.failed.error.message}` : `${res.done.length} patch${res.done.length === 1 ? "" : "es"} written and read back.`);
      if (res.mismatched.length) lines.push(`These slots did not read back the same: ${res.mismatched.map(label).join(", ")}. Try writing them again.`);
      clear(body).append(...lines.map((t, i) => h("p", { class: i === 0 && !res.failed && !res.mismatched.length ? "good" : "warn", text: t })));
      clear(foot).append(h("button", { text: "Close", onclick: () => close(res), autofocus: true }));
      app.say(lines[0], res.failed || res.mismatched.length ? "warn" : "good");
      app.patchesChanged?.();
    }
    document.body.append(dlg); draw(); dlg.showModal();
  });
}

// ---------------------------------------------------------------- what a dropped / chosen file is, and what happens to it
// opts.part (0 / 1): load into that part live, without saving. opts.start: the first slot for an import.
export async function handleFile(app, file, opts = {}) {
  const u = app.unit;
  if (!u) { app.say("Connect the FM-1 first.", "warn"); return; }
  let bytes; try { bytes = await readBytes(file); } catch { app.say(`Could not read ${file.name}.`, "warn"); return; }
  const kind = sniff(bytes, file.name);
  if (kind === "wav" || kind === "wavetable") {
    if (opts.part != null) { app.say("A wavetable is loaded on the WAVETABLES tab; drop it there.", "warn"); return; }
    app.pendingTable = { file, bytes, kind }; app.setGlobalTab("tables", true); return;
  }
  if (!LIB.hasRaw(u)) { app.say("This unit's firmware cannot take whole patches (it needs DUELL 0.2.19). Update it with the installer, then drop the file again.", "warn"); return; }
  let items = [], notes = [], problems = [], title, source, numbers = false;
  if (kind === "patches") {
    const r = parseFile(new TextDecoder().decode(bytes));
    if (!r.ok) { app.say(`${file.name}: ${r.error}`, "warn"); return; }
    items = r.items; problems = r.problems; numbers = true;
    title = items.length === 1 ? "Import a patch" : `Import ${items.length} patches`; source = `From the file ${file.name}${r.firmware ? ` (made with ${r.firmware.replace(/^FELUCCA /, "")})` : ""}.`;
    if (!items.length) { app.say(`${file.name}: no patch in it could be read. ${problems[0] || ""}`, "warn"); return; }
  } else if (kind === "dx7") {
    const p = parseDx7File(bytes);
    if (p.kind === "none") { app.say(`${file.name}: no DX7 voice found in it.`, "warn"); return; }
    items = p.voices.map((v) => {
      if (v.damaged) return { damaged: true, name: v.name };
      const c = voiceToPatch(v.voice, v.index), it = itemFromRecord(c.record, { number: null, source: SOURCE_LABEL });
      it.catGuessed = true; return it;
    });
    notes = [...p.notes, "Each voice becomes an FM6 patch that carries the voice. The effects and the MULTI FILTER start neutral (filter 6.8 kHz, reverb 7 %, EQ flat); the category is a guess from the name."];
    title = p.kind === "single" ? "Import a DX7 voice" : `Import DX7 voices (${p.voices.length})`; source = `${SOURCE_LABEL}: ${file.name}.`;
  } else { app.say(`${file.name}: not a DUELL patch file, a DX7 sysex file or a WAV.`, "warn"); return; }
  if (opts.part != null) return loadInto(app, opts.part, items, title, source, notes, problems);
  await importDialog(app, { title, source, items, notes, problems, start: opts.start ?? app.patchStart ?? 0, numbers });
}
// a patch (one of a file's or a bank's) into a part
async function loadInto(app, part, items, title, source, notes, problems) {
  const u = app.unit, ok = items.filter((i) => !i.damaged && !i.error);
  if (!ok.length) { app.say(`${problems[0] || "Nothing in the file can be loaded."}`, "warn"); return; }
  let item = ok[0];
  if (ok.length > 1) {                                           // pick one of several
    const picked = await new Promise((resolve) => {
      const dlg = h("dialog", { class: "ask wide", "aria-label": `Load into PART ${part + 1}` });
      const sel = h("select", { size: Math.min(12, ok.length), "aria-label": "Patch to load" }, ok.map((it, i) => h("option", { value: i, text: `${it.number != null ? label(it.number - 1) + "  " : ""}${it.name}  ·  ${it.engineName}` })));
      sel.value = "0";
      const done = (v) => { dlg.close(); dlg.remove(); resolve(v); };
      dlg.append(h("h2", { text: `Load into PART ${part + 1}` }), h("p", { class: "small", text: `${source} Pick one patch. It is loaded into the part to hear it; nothing is stored in any slot.` }), sel,
        h("div", { class: "row end" }, h("button", { class: "ghost", text: "Cancel", onclick: () => done(null) }), h("button", { text: "Load", onclick: () => done(ok[+sel.value]) })));
      dlg.addEventListener("cancel", (e) => { e.preventDefault(); done(null); });
      document.body.append(dlg); dlg.showModal();
    });
    if (!picked) return; item = picked;
  } else if (!(await ask(`Load into PART ${part + 1}`, [`"${item.name}" (${item.engineName}) replaces PART ${part + 1}'s current sound. It is not saved in any slot.`], "Load"))) return;
  try {
    app.say(`Loading ${item.name} into PART ${part + 1} ...`);
    const r = await LIB.loadItemIntoPart(u, part, item);
    app.refresh();
    app.say(`${item.name} is loaded in PART ${part + 1} (not saved).${r.notes.length ? ` Not applied: ${r.notes.join(", ")}.` : ""}`, "good");
  } catch (e) { app.fail(e); }
}

// ---------------------------------------------------------------- the tab
export function buildPatches(app, wrap) {
  const u = app.unit, lib = LIB.hasRaw(u);
  const st = { items: [], sel: new Set(), cur: null, anchor: null, q: "", eng: "all", cat: "all", loading: true, wide: null, wideMode: "swap" };
  const rowEls = new Map();
  const list = h("ul", { class: "plist", role: "listbox", "aria-multiselectable": "true", "aria-label": "Patch slots" });
  const detail = h("div", { class: "pdetail" });
  const search = h("input", { type: "search", placeholder: "Search by name", "aria-label": "Search patches", oninput: (e) => { st.q = e.target.value.toUpperCase(); drawList(); paintWide(); } });
  const eng = h("select", { "aria-label": "Engine", onchange: (e) => { st.eng = e.target.value; drawList(); paintWide(); } }, h("option", { value: "all", text: "All engines" }),
    L.ORDER.map((e) => h("option", { value: e, text: engineLabel(e) })));
  const catSel = h("select", { "aria-label": "Category", onchange: (e) => { st.cat = e.target.value; drawList(); paintWide(); } }, h("option", { value: "all", text: "All categories" }),
    CATEGORIES.map((c, i) => (i ? h("option", { value: i, text: c }) : null)));
  const fileIn = h("input", { type: "file", accept: ".json,.syx,application/json", multiple: true, hidden: true, "aria-label": "Patch file", onchange: async (e) => { for (const f of e.target.files) await handleFile(app, f, { start: st.cur ?? 0 }); e.target.value = ""; } });
  const bar1 = h("div", { class: "row" }, search, catSel, eng, h("button", { class: "ghost small", text: "Wide view", title: "All 128 slots on the full width of the page, for dragging a patch from one slot to another", onclick: openWide }));
  const bar2 = h("div", { class: "row" },
    lib ? [h("button", { class: "ghost small", text: "Import file...", title: "A DUELL patch file from this computer (you can also drop it here)", onclick: () => fileIn.click() }), fileIn,
      h("button", { class: "ghost small", text: "Import .syx...", title: "A DX7 sysex file (one voice or a bank of 32): each voice becomes an FM6 patch (the six-operator engine); you choose the slots before anything is written", onclick: () => fileIn.click() }),
      h("button", { class: "ghost small", id: "expsel", hidden: true, text: "Download selection", onclick: () => exportSlots([...st.sel].sort((a, b) => a - b), "selection") }),
      h("button", { class: "ghost small", text: "Download all...", title: "Every patch you saved on the unit, in one file on this computer", onclick: exportBank })] : null,
    h("button", { class: "ghost small", text: "Load from unit", title: "Reads the list of slots from the unit again", onclick: load }),
    h("button", { class: "ghost small danger", id: "restoreall", text: "Restore all...", title: "Every slot gets its factory sound back", onclick: doRestoreAll }));
  const zone = h("div", { class: "patches" },
    h("div", { class: "pleft" }, bar1, bar2, lib ? null : h("p", { class: "warn small", text: "Downloading and importing whole patches need firmware 0.2.19 (this unit has an older one; update it with the installer)." }), list),
    h("div", { class: "pright" }, detail));
  wrap.append(zone);
  list.title = "Drop a patch file or a DX7 .syx file anywhere here. Drag a patch onto another slot to copy it (Shift: swap). Drop it on a part to hear it there (nothing is saved).";

  async function load() {
    st.loading = true; drawList();
    try {
      st.items = (await u.listSlots()).map((s) => ({ slot: s.slot, eng: s.eng, name: s.name, cat: s.cat, saved: s.saved }));
      st.loading = false; drawList(); paintWide(); drawDetail();
    } catch (e) { st.loading = false; app.fail(e); drawList(); }
  }
  app.patchesChanged = () => { st.items = (u.slots || []).map((s) => ({ slot: s.slot, eng: s.eng, name: s.name, cat: s.cat, saved: s.saved })); drawList(); paintWide(); drawDetail(); };
  const matches = (it) => (st.eng === "all" || +st.eng === it.eng) && (st.cat === "all" || +st.cat === it.cat) && (!st.q || it.name.toUpperCase().includes(st.q));

  // ---- selection
  function select(slot, e = {}) {
    if (e.shiftKey && st.anchor != null) { const a = Math.min(st.anchor, slot), b = Math.max(st.anchor, slot); st.sel = new Set([...Array(b - a + 1).keys()].map((i) => a + i)); }
    else if (e.ctrlKey || e.metaKey) { if (st.sel.has(slot)) st.sel.delete(slot); else st.sel.add(slot); st.anchor = slot; }
    else if (st.sel.size === 1 && st.sel.has(slot) && !e.keep) { st.sel = new Set(); st.anchor = slot; }   // a second click on the one selected row deselects it
    else { st.sel = new Set([slot]); st.anchor = slot; }
    st.cur = slot; app.patchStart = slot; paintSel(); drawDetail();
  }
  function paintSel() {
    for (const [slot, li] of rowEls) {
      const on = st.sel.has(slot);
      li.classList.toggle("sel", on); li.setAttribute("aria-selected", String(on));
      const cb = li.querySelector("input[type=checkbox]"); if (cb) cb.checked = on;
    }
    paintWide();
    const ex = bar2.querySelector("#expsel"); if (ex) ex.hidden = !st.sel.size;
  }

  // ---- the list
  function drawList() {
    const keep = list.scrollTop;
    clear(list); rowEls.clear();
    const ra = bar2.querySelector("#restoreall"); if (ra) { ra.disabled = st.loading || !st.items.some((it) => it.saved); ra.title = ra.disabled ? "Every slot holds its factory sound already" : "Every slot gets its factory sound back"; }
    if (st.loading) { list.append(h("li", { class: "note", text: "Reading the patches from the unit ..." })); return; }
    const rows = st.items.filter(matches);
    for (const it of rows) {
      const on = st.sel.has(it.slot);
      const li = h("li", { role: "option", tabindex: it.slot === (st.cur ?? rows[0]?.slot) ? 0 : -1, "aria-selected": String(on), class: (on ? "sel" : "") + (it.saved ? " saved" : ""), draggable: "true", dataset: { slot: it.slot },
        title: `${label(it.slot)} ${it.name}, ${engineLabel(it.eng)}${CATEGORIES[it.cat] ? ", " + CATEGORIES[it.cat] : ""}` },
        h("input", { type: "checkbox", tabindex: -1, checked: on, "aria-label": `Select ${label(it.slot)} ${it.name}` }),
        h("span", { class: "k", text: label(it.slot) }), h("span", { class: "n", text: it.name }), h("span", { class: "e", text: engineLabel(it.eng) }), h("span", { class: "c", text: CATEGORIES[it.cat] || "" }));
      rowEls.set(it.slot, li); list.append(li);
    }
    if (!rows.length) list.append(h("li", { class: "note", text: "No slot matches." }));
    list.scrollTop = keep;
  }
  list.addEventListener("click", (e) => {
    const li = e.target.closest("li[data-slot]"); if (!li) return;
    const slot = +li.dataset.slot;
    if (e.target.matches("input[type=checkbox]")) { e.preventDefault(); select(slot, { ctrlKey: true }); return; }
    select(slot, e);
  });
  list.addEventListener("keydown", (e) => {
    const li = e.target.closest("li[data-slot]"); if (!li) return;
    const slot = +li.dataset.slot, visible = [...rowEls.keys()];
    if (e.key === "Enter" || e.key === " ") { e.preventDefault(); select(slot, e); return; }
    const i = visible.indexOf(slot), j = e.key === "ArrowDown" ? i + 1 : e.key === "ArrowUp" ? i - 1 : e.key === "Home" ? 0 : e.key === "End" ? visible.length - 1 : e.key === "PageDown" ? i + 8 : e.key === "PageUp" ? i - 8 : -1;
    if (j < 0) return;
    e.preventDefault();
    const to = visible[Math.max(0, Math.min(visible.length - 1, j))], el = rowEls.get(to);
    for (const l of rowEls.values()) l.tabIndex = -1;
    el.tabIndex = 0; el.focus(); el.scrollIntoView({ block: "nearest" });
    if (e.shiftKey) select(to, { shiftKey: true });
  });

  // ---- drag and drop: slots (copy; Shift: swap) and files (anywhere on the tab; on a slot: the first slot of the import)
  const slotDrag = (root) => {
    root.addEventListener("dragstart", (e) => {
      const el = e.target.closest?.("[data-slot]"); if (!el) return;
      e.dataTransfer.setData(SLOT_TYPE, el.dataset.slot); e.dataTransfer.setData("text/plain", `${label(+el.dataset.slot)} ${st.items[+el.dataset.slot]?.name ?? ""}`); e.dataTransfer.effectAllowed = "copyMove";
      el.classList.add("dragging");
    });
    root.addEventListener("dragend", (e) => { e.target.closest?.("[data-slot]")?.classList.remove("dragging"); root.querySelectorAll(".dropok").forEach((x) => x.classList.remove("dropok")); });
    root.addEventListener("dragover", (e) => {
      const el = e.target.closest?.("[data-slot]");
      if (hasSlot(e) && el) { e.preventDefault(); e.dataTransfer.dropEffect = "copy"; root.querySelectorAll(".dropok").forEach((x) => x !== el && x.classList.remove("dropok")); el.classList.add("dropok"); }
    });
    root.addEventListener("dragleave", (e) => { e.target.closest?.("[data-slot]")?.classList.remove("dropok"); });
    root.addEventListener("drop", (e) => {
      const el = e.target.closest?.("[data-slot]"); root.querySelectorAll(".dropok").forEach((x) => x.classList.remove("dropok"));
      if (el && hasSlot(e)) { e.preventDefault(); e.stopPropagation(); const src = slotOf(e); if (src != null) dropSlot(src, +el.dataset.slot, root === list ? (e.shiftKey ? "swap" : "copy") : (e.altKey ? "copy" : st.wideMode)); }
      else if (hasFiles(e)) { e.preventDefault(); e.stopPropagation(); dropFiles(filesOf(e), el ? +el.dataset.slot : (st.cur ?? 0)); }
    });
  };
  slotDrag(list);
  dropZone(zone, { accept: (e) => hasFiles(e), onDrop: (e) => dropFiles(filesOf(e), st.cur ?? 0) });
  async function dropFiles(files, start) { for (const f of files) await handleFile(app, f, { start }); }
  async function dropSlot(src, dst, mode) {
    if (src === dst) return;
    if (!lib) { app.say("Copying a patch needs firmware 0.2.19 on the unit.", "warn"); return; }
    const a = LIB.describeSlot(u, src), b = LIB.describeSlot(u, dst);
    try {
      if (mode === "swap") {
        if (!a.saved && !b.saved) { app.say(`${a.label} and ${b.label} both hold factory sounds; there is nothing to swap.`); return; }
        const lines = LIB.swapLines(u, src, dst);
        if (!(await ask(a.saved && b.saved ? "Swap two patches" : "Move a patch", lines, a.saved && b.saved ? "Swap" : "Move", !(a.saved && b.saved) && (a.saved ? b.saved : a.saved)))) return;
        app.say("Swapping ..."); await LIB.swapSlots(u, src, dst); st.sel = new Set([dst]); st.cur = dst; load(); app.say(`${a.label} and ${b.label} are done.`, "good");
      } else {
        const plan = await LIB.planCopy(u, src, dst);
        if (plan.problems.length) { app.say(plan.problems[0], "warn"); return; }
        const lines = LIB.confirmLines(plan); if (!a.saved) lines.push("It is a factory sound: only its values are copied (its arp block and pattern are not).");
        if (!(await ask(`Copy ${a.label} to ${b.label}`, [`"${a.name}" is copied to ${b.label}; ${a.label} stays as it is.`, ...lines], b.saved ? "Overwrite" : "Copy", b.saved))) return;
        app.say("Copying ..."); const r = await LIB.runPlan(u, plan, {});
        if (r.failed) throw r.failed.error;
        st.sel = new Set([dst]); st.cur = dst; await load(); app.say(`Copied ${a.label} to ${b.label}.`, "good");
      }
    } catch (e) { app.fail(e); load(); }
  }

  // ---- actions on the selection
  async function exportSlots(slots, kind) {
    if (!slots.length) return;
    try {
      app.say(`Reading ${slots.length} patch${slots.length === 1 ? "" : "es"} ...`);
      const items = await LIB.readItems(u, slots, (i, n) => app.say(`Reading ${i} of ${n} ...`));
      const name = fileName(items, kind); saveText(app, name, LIB.fileText(u, items, "DUELL editor"));
      const approx = items.filter((i) => i.kind === "approx").length;
      app.say(`Saved ${name} (${items.length} patch${items.length === 1 ? "" : "es"}).${approx ? ` ${approx} factory sound${approx === 1 ? " is" : "s are"} in it as a copy of the values.` : ""}`, "good");
    } catch (e) { app.fail(e); }
  }
  async function exportBank() {
    if (!st.items.length) await load();
    const saved = st.items.filter((i) => i.saved).map((i) => i.slot);
    const dlg = h("dialog", { class: "ask", "aria-label": "Download all saved patches" });
    const inc = h("input", { type: "checkbox" });
    const go = await new Promise((resolve) => {
      const done = (v) => { dlg.close(); dlg.remove(); resolve(v); };
      dlg.append(h("h2", { text: "Download all saved patches" }), h("p", { text: "One file on this computer with every patch you saved on the unit, each with its slot number. Drop it on a unit's PATCHES tab to put them back in the same slots." }),
        h("label", { class: "chk" }, inc, " Also include the factory sounds (a copy of their values, 128 patches in all)"),
        h("div", { class: "row end" }, h("button", { class: "ghost", text: "Cancel", onclick: () => done(false) }), h("button", { text: "Download", onclick: () => done(true) })));
      dlg.addEventListener("cancel", (e) => { e.preventDefault(); done(false); });
      document.body.append(dlg); dlg.showModal();
    });
    if (!go) return;
    const slots = inc.checked ? [...Array(UP_SLOTS).keys()] : saved;
    if (!slots.length) { app.say("No patch is saved on the unit; the bank holds only factory sounds. Tick the box to export those too."); return; }
    await exportSlots(slots, "bank");
  }
  async function copyTo(slots) {
    const first = slots[0];
    const to = await askSlot(app, { title: slots.length === 1 ? `Copy ${label(first)} to ...` : `Copy ${slots.length} patches to ...`, value: Math.min(UP_SLOTS - 1, (slots.at(-1) ?? 0) + 1), okLabel: "Copy",
      intro: slots.length === 1 ? `"${LIB.describeSlot(u, first).name}" is copied; the original stays.` : `The ${slots.length} patches are copied one after the other from the slot you pick; the originals stay.`,
      describe: (n) => {
        if (n + slots.length > UP_SLOTS) return { lines: [`Not enough slots after #${n + 1}.`], bad: true };
        const lines = slots.slice(0, 8).map((s, i) => `${label(s)} "${LIB.describeSlot(u, s).name}" -> ${label(n + i)}, replacing "${LIB.describeSlot(u, n + i).name}" (${LIB.describeSlot(u, n + i).saved ? "saved" : "factory sound"})`);
        if (slots.length > 8) lines.push(`... and ${slots.length - 8} more.`);
        return { lines, danger: slots.some((s, i) => LIB.describeSlot(u, n + i).saved) };
      } });
    if (to == null) return;
    try {
      const items = await LIB.readItems(u, slots), plan = LIB.makePlan(u, items, slots.map((s, i) => to + i));
      if (plan.problems.length) { app.say(plan.problems[0], "warn"); return; }
      app.say("Copying ..."); const r = await LIB.runPlan(u, plan, { safety: async (text) => saveText(app, "duell-safety-copy.duell-patches.json", text), onProgress: (p) => p.phase === "write" && app.say(`Copying ${p.index + 1} of ${p.total} ...`) });
      if (r.failed) throw r.failed.error;
      st.sel = new Set(slots.map((s, i) => to + i)); st.cur = to; await load(); app.say(`Copied to ${label(to)}${slots.length > 1 ? " and the slots after it" : ""}.`, "good");
    } catch (e) { app.fail(e); load(); }
  }
  async function moveTo(slot) {
    const a = LIB.describeSlot(u, slot);
    const to = await askSlot(app, { title: `Move ${a.label} to ...`, value: Math.min(UP_SLOTS - 1, slot + 1), okLabel: "Move", intro: `"${a.name}" moves to the slot you pick. ${a.label} shows its own factory sound again.`,
      describe: (n) => { const b = LIB.describeSlot(u, n); return n === slot ? { lines: ["That is the slot it is in."], bad: true } : { lines: [`${b.label} "${b.name}" (${b.saved ? "saved" : "factory sound"}) is replaced.`], danger: b.saved }; } });
    if (to == null) return;
    const b = LIB.describeSlot(u, to);
    try {
      let doSwap = false;
      if (b.saved) {
        doSwap = await ask("Swap instead?", [`${b.label} holds "${b.name}". Swap puts it in ${a.label}; Replace loses it.`], "Swap");
        if (!doSwap && !(await ask(`Replace ${b.label}?`, [`"${b.name}" is lost (it is not a factory sound).`], "Replace", true))) return;
      }
      app.say("Moving ...");
      if (doSwap) await LIB.swapSlots(u, slot, to); else await LIB.moveSlot(u, slot, to);
      st.sel = new Set([to]); st.cur = to; await load(); app.say(`Moved to ${label(to)}.`, "good");
    } catch (e) { app.fail(e); load(); }
  }

  // ---- the detail pane (existing actions: load, save, rename, restore)
  const suggest = (p) => (app.soundName(p) || "").slice(0, 12);
  function drawDetail() {
    clear(detail);
    const sel = [...st.sel].sort((a, b) => a - b), it = sel.length === 1 ? st.items[sel[0]] : null;
    if (sel.length > 1) {
      detail.append(h("h3", { text: `${sel.length} slots selected` }),
        h("div", { class: "row" }, lib ? [h("button", { text: "Download selection", onclick: () => exportSlots(sel, "selection") }), h("button", { class: "ghost", text: "Copy to...", onclick: () => copyTo(sel) })] : null,
          h("button", { class: "ghost", text: "Clear selection", onclick: () => { st.sel = new Set(); paintSel(); drawDetail(); } })));
      return;
    }
    const part = partSel(app, "Target part");
    if (!it) return;
    detail.append(h("h3", { text: `${label(it.slot)} ${it.name}` }));
    detail.append(h("p", { class: "small", text: `${engineLabel(it.eng)}${CATEGORIES[it.cat] ? " · " + CATEGORIES[it.cat] : ""}` }));
    detail.append(h("div", { class: "row" }, h("span", { text: "RECALL INTO" }), part, h("button", { text: "Recall", title: "The slot's sound replaces the part's sound on the unit (nothing is stored)", onclick: () => doLoad(it, +part.value) })));
    // CATEGORY (0.2.25): a stored patch's category is a byte of its record: read whole (PATCH_RAW), the byte changed, written whole
    const catPick = h("select", { "aria-label": "Category of this patch", disabled: !it.saved || !lib, title: !it.saved ? "A factory sound's category is fixed. Save a sound to this slot to choose its category." : !lib ? "Needs firmware 0.2.19 or newer" : "",
      onchange: (e) => setCategory(it, +e.target.value, e.target) }, CATEGORIES.map((c, i) => h("option", { value: i, text: c })));
    catPick.value = it.cat;
    detail.append(h("div", { class: "row" }, h("span", { text: "CATEGORY" }), catPick));
    // SAVE TO UNIT (the bug fix of 0.2.30): the form belongs to the patch that was clicked. Its name and category are that patch's own, NO slot is chosen for you (the button
    // stays off until you pick one), and what is stored is that patch (read whole from its slot), never the sound a part plays.
    const nm = h("input", { type: "text", maxlength: 12, value: it.name, "aria-label": "Patch name", spellcheck: "false" });
    const sc = h("select", { "aria-label": "Category for the stored patch" }, CATEGORIES.map((c, i) => h("option", { value: i, text: c })));
    sc.value = String(it.cat);
    const target = h("select", { "aria-label": "Slot to save into" }, h("option", { value: "", text: "choose a slot ..." }),
      st.items.map((x) => h("option", { value: x.slot, text: `${label(x.slot)} ${x.name}` })));
    target.value = "";
    const go = h("button", { class: "small", text: "Save to unit", disabled: true, onclick: () => doSave(it, +target.value, nm.value, +sc.value) });
    const upd = () => {
      const t = target.value === "" ? null : st.items[+target.value];
      go.disabled = !t || !lib || !nm.value.trim();
      go.textContent = t ? `Replace ${label(t.slot)} ${t.name}` : "Save to unit";
      go.title = !lib ? "Needs firmware 0.2.19 or newer" : t ? `${label(it.slot)} ${it.name} is stored in ${label(t.slot)} under the name and category above (a file is not made; no part is changed)` : "Choose the slot first";
    };
    target.addEventListener("change", upd); nm.addEventListener("input", upd); upd();
    detail.append(h("div", { class: "row" }, h("span", { text: "SAVE AS" }), nm, sc), h("div", { class: "row" }, h("span", { text: "INTO" }), target, go));
    detail.append(h("div", { class: "row" }, h("button", { class: "ghost", text: "Rename...", onclick: () => doRename(it, +part.value) }),
      it.saved ? h("button", { class: "ghost danger", text: "Restore factory sound...", title: "Take the stored sound out of this slot: the factory sound comes back", onclick: () => doRestore(it) }) : null));
    if (lib) detail.append(h("div", { class: "row" }, h("button", { class: "ghost", text: "Download file", title: it.saved ? "A file on this computer with the patch whole" : "A file on this computer with a copy of this factory sound's values", onclick: () => exportSlots([it.slot], "patch") }),
      h("button", { class: "ghost", text: "Copy to...", onclick: () => copyTo([it.slot]) }), it.saved ? h("button", { class: "ghost", text: "Move to...", onclick: () => moveTo(it.slot) }) : null));
  }
  async function doLoad(it, p) {
    try { app.say(`Recalling ${it.name} into PART ${p + 1} ...`); await u.loadSlot(p, it.slot); u.parts[p].name = it.name; app.undo?.forget(p); app.refresh(); app.say(`${it.name} is recalled in PART ${p + 1}. It replaced that part's sound.`, "good"); } catch (e) { app.fail(e); }
  }
  async function setCategory(it, cat, sel) {
    try {
      app.say("Changing the category ...");
      await LIB.setCategory(u, it.slot, cat);
      await load(); app.say(`${label(it.slot)} ${it.name} is in ${CATEGORIES[cat]} now.`, "good");
    } catch (e) { app.fail(e); sel.value = it.cat; }
  }
  async function doSave(it, dst, name, cat) {
    name = name.replace(/[^ -~]/g, "").trim();
    if (!name) { app.say("Give the patch a name (1 to 12 characters).", "warn"); return; }
    try {
      app.say("Saving ..."); await LIB.saveAs(u, it.slot, dst, { name, cat });
      for (const pt of u.parts) if (pt && pt.slot === dst) pt.name = name;
      await load(); app.say(`Saved "${name}" to the unit's slot ${label(dst)}.`, "good");
    } catch (e) { app.fail(e); }
  }
  async function doRename(it, p) {
    // RENAME (owner, 0b): every patch, a factory one too: it is loaded into the part and saved to the unit under the new name (one short question)
    const nn = await askText("Rename patch", [it.saved ? `"${it.name}" is loaded into PART ${p + 1} and saved again under the new name (PART ${p + 1}'s unsaved edits are lost).` : `"${it.name}" is a factory sound: it is loaded into PART ${p + 1} and saved to the unit under the new name (PART ${p + 1}'s unsaved edits are lost).`], it.name, "Rename");
    if (!nn) return;
    try { app.say("Renaming ..."); await u.loadSlot(p, it.slot); await u.saveSlot(p, it.slot, nn); u.parts[p].name = nn; await load(); app.say(`Renamed to "${nn}".`, "good"); } catch (e) { app.fail(e); }
  }
  async function doRestore(it) {
    if (!(await ask("Restore patch", [`The saved sound "${it.name}" in ${label(it.slot)} is removed from the unit and the slot's factory sound comes back. Playback is stopped to write.`], "Restore", true))) return;
    try { app.say("Restoring ..."); await u.restoreSlot(it.slot); await load(); app.say(`Restored ${label(it.slot)}.`, "good"); } catch (e) { app.fail(e); }
  }
  async function doRestoreAll() {
    if (!(await ask("Restore all patches", ["Every slot gets its factory sound back. Every saved sound on the unit is lost (export the bank first if you want to keep them). Playback is stopped to write."], "Restore all", true))) return;
    try { app.say("Restoring ..."); await u.restoreAll((i, n) => app.say(`Restoring ... ${i} of ${n}`)); await load(); app.say("Every slot holds its factory sound.", "good"); } catch (e) { app.fail(e); await load(); }
  }

  // ---- the wide view: all 128 slots on the full width, an overlay under the header. Drag: swap (Alt: copy)
  function openWide() {
    if (st.wide) { st.wide.querySelector(".tile[tabindex='0']")?.focus(); return; }
    const top = Math.round(document.getElementById("top")?.getBoundingClientRect().bottom ?? 0);
    const grid = h("div", { class: "pgrid", role: "listbox", "aria-multiselectable": "true", "aria-label": "Patch slots, wide view" });
    for (let s = 0; s < UP_SLOTS; s++) grid.append(h("div", { class: "tile", role: "option", tabindex: s === (st.cur ?? 0) ? 0 : -1, draggable: "true", dataset: { slot: s }, "aria-selected": "false" },
      h("span", { class: "k", text: label(s) }), h("span", { class: "n" }), h("span", { class: "e" })));
    const mode = h("span", { class: "segs", role: "group", "aria-label": "What dragging does" },
      ["swap", "copy"].map((m) => h("button", { class: "seg ghost" + (st.wideMode === m ? " on" : ""), type: "button", dataset: { mode: m }, text: m === "swap" ? "Drag swaps" : "Drag copies", "aria-pressed": String(st.wideMode === m),
        onclick: () => { st.wideMode = m; paintWide(); } })));
    const ov = h("div", { class: "pwide", role: "dialog", "aria-label": "Patch slots, wide view", style: `top:${top + 4}px` },
      h("div", { class: "row" }, h("b", { text: "PATCH SLOTS" }), mode, h("span", { class: "grow" }),
        lib ? h("button", { class: "ghost small", text: "Download selection", onclick: () => exportSlots([...st.sel].sort((a, b) => a - b), "selection") }) : null,
        h("button", { class: "ghost small", text: "Close", onclick: closeWide })), grid);
    st.wide = ov; document.body.append(ov); document.body.classList.add("widepatches");
    ov.addEventListener("keydown", (e) => {
      if (e.key === "Escape") { e.preventDefault(); closeWide(); return; }
      const t = e.target.closest?.(".tile"); if (!t) return;
      const s = +t.dataset.slot, step = { ArrowRight: 1, ArrowLeft: -1, ArrowDown: columns(), ArrowUp: -columns(), Home: -s, End: UP_SLOTS - 1 - s }[e.key];
      if (e.key === "Enter" || e.key === " ") { e.preventDefault(); select(s, e); return; }
      if (step == null) return;
      e.preventDefault();
      const to = Math.max(0, Math.min(UP_SLOTS - 1, s + step)), el = grid.children[to];
      for (const c of grid.children) c.tabIndex = -1;
      el.tabIndex = 0; el.focus(); if (e.shiftKey) select(to, { shiftKey: true });
    });
    grid.addEventListener("click", (e) => { const t = e.target.closest(".tile"); if (t) select(+t.dataset.slot, e); });
    slotDrag(grid);
    dropZone(ov, { accept: (e) => hasFiles(e), onDrop: (e) => dropFiles(filesOf(e), st.cur ?? 0) });
    paintWide(); grid.children[st.cur ?? 0].focus();
  }
  const columns = () => { const g = st.wide?.querySelector(".pgrid"); return g ? getComputedStyle(g).gridTemplateColumns.split(" ").length : 16; };
  function closeWide() { st.wide?.remove(); st.wide = null; document.body.classList.remove("widepatches"); rowEls.get(st.cur ?? -1)?.focus(); }
  function paintWide() {
    if (!st.wide) return;
    const grid = st.wide.querySelector(".pgrid"), by = new Map(st.items.map((i) => [i.slot, i]));
    for (const t of grid.children) {
      const it = by.get(+t.dataset.slot); if (!it) continue;
      t.classList.toggle("saved", it.saved); t.classList.toggle("sel", st.sel.has(it.slot)); t.classList.toggle("miss", !matches(it)); t.setAttribute("aria-selected", String(st.sel.has(it.slot)));
      t.children[1].textContent = it.name; t.children[2].textContent = engineLabel(it.eng);
      t.title = `${label(it.slot)} ${it.name}, ${engineLabel(it.eng)}${CATEGORIES[it.cat] ? ", " + CATEGORIES[it.cat] : ""}`;
    }
    for (const b of st.wide.querySelectorAll(".segs button")) { b.classList.toggle("on", b.dataset.mode === st.wideMode); b.setAttribute("aria-pressed", String(b.dataset.mode === st.wideMode)); }
    st.wide.querySelector(".pgrid").title = st.wideMode === "swap" ? "Drag a patch onto another slot to swap them (hold Alt to copy). Drop a file here to import." : "Drag a patch onto another slot to copy it. Drop a file here to import.";
  }
  load();
}
