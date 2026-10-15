// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the tabs that are not a grid of parameters: PATCHES, MULTIS, SETTINGS (MENU), MIDI (routing and learn map), the SEQ steps, WAVETABLES.
import { h, clear, ask, askText } from "./ui_util.js";
import { L, ID, P_E0, descOf, engineLabel, CATEGORIES } from "./layout.js";
import { noteName, parseStep, CMD } from "./proto.js";
import * as WTM from "./wavetable.js";
import { dropZone, hasFiles, filesOf, readBytes, saveText } from "./dropzone.js";

const pad = (n, w = 2) => String(n).padStart(w, "0");
const partSel = (app, label = "Part") => {
  const s = h("select", { "aria-label": label }, h("option", { value: 0, text: "PART 1" }), h("option", { value: 1, text: "PART 2" }));
  s.value = app.target; s.addEventListener("change", () => { app.target = +s.value; });
  return s;
};

// ---------------------------------------------------------------- PATCHES (the patch librarian: patches_ui.js)
export { buildPatches } from "./patches_ui.js";

// ---------------------------------------------------------------- MULTIS
export function buildMultis(app, wrap) {
  const u = app.unit, box = h("div", { class: "multis" });
  const bar = h("div", { class: "row" },
    h("button", { class: "ghost", text: "Read names from the unit", title: "Lists the unit's stored objects: this stops playback on the unit", onclick: readNames }),
    h("span", { class: "small", text: "Reading the names stops playback on the unit (it is a backup list). Used / free is read without stopping." }));
  wrap.append(bar, box, h("p", { class: "note", text: "Load replaces both parts, the effects and the arps with the multi's. Save writes the current music of both parts into the slot (flash, playback is stopped). A multi keeps the name its slot has; rename a stored multi here. A factory multi (M01..M10) can be renamed after it has been saved once. The output MODE, BALANCE, SPLIT and MORPH state are stored by the unit and cannot be seen here." }));
  let used = u.multis || null;
  async function refreshUsed() { try { used = await u.multiUsed(); draw(); } catch (e) { app.fail(e); } }
  async function readNames() { try { app.say("Reading multi names ..."); await u.readMultiObjects(); draw(); app.say("Names read.", "good"); } catch (e) { app.fail(e); } }
  const factory = new Map(L.MULTIS);
  function draw() {
    clear(box);
    for (let s = 0; s < 16; s++) {
      const stored = u.multiNames?.[s];
      const name = stored ?? (factory.has(s) ? factory.get(s) : null);
      const label = used == null ? "…" : name ? name : used[s] ? "(used)" : "(empty)";
      const kind = stored != null ? "yours" : factory.has(s) ? (u.multiNames ? "factory" : "factory, unless you saved over it") : used?.[s] ? "" : "";
      box.append(h("div", { class: "mrow2" }, h("b", { text: `M${pad(s + 1)}` }), h("span", { class: "n", text: label }), h("span", { class: "small", text: kind }),
        h("button", { class: "ghost", text: "Load", disabled: used?.[s] === false, onclick: () => doLoad(s, label) }),
        h("button", { class: "ghost", text: "Save here...", onclick: () => doSave(s, label) }),
        h("button", { class: "ghost", text: "Rename...", disabled: !u.multiNames, title: u.multiNames ? "" : "Read the names first", onclick: () => doRename(s, label) })));
    }
  }
  async function doLoad(s, label) {
    if (!(await ask(`Load M${pad(s + 1)}`, [`Load "${label}": both parts and the effects are replaced by the multi's. Unsaved edits are lost.`], "Load"))) return;
    await app.run(`Loading M${pad(s + 1)}`, () => u.loadMulti(s));
    app.refresh();
  }
  async function doSave(s, label) {
    const lines = [`The current music of both parts is stored in M${pad(s + 1)}.`, "The unit stops playback while it writes (up to two seconds)."];
    if (used?.[s]) lines.unshift(`M${pad(s + 1)} holds "${label}". It will be OVERWRITTEN.`);
    if (!(await ask(`Save to M${pad(s + 1)}`, lines, used?.[s] ? "Overwrite" : "Save", !!used?.[s]))) return;
    await app.run(`Saving M${pad(s + 1)}`, () => u.saveMulti(s)); draw();
  }
  async function doRename(s, label) {
    const nn = await askText(`Rename M${pad(s + 1)}`, ["The unit stops playback while it writes. Only this multi's name changes; the other multi in the same stored block is copied unchanged."], u.multiNames?.[s] || label, "Rename");
    if (!nn) return;
    await app.run("Renaming", () => u.renameMulti(s, nn)); draw();
  }
  draw(); refreshUsed();
}

// ---------------------------------------------------------------- SETTINGS (MENU)
export function buildSettings(app, wrap) {
  const u = app.unit, box = h("div", { class: "menu" });
  wrap.append(h("p", { class: "note", text: "The unit's own MENU settings, as the unit describes them (names, ranges and tabs come from the unit). A change is applied at once; the unit writes it to its memory when playback is stopped, as it does for its own MENU." }), box);
  if (u.info.menuCount == null) { box.append(h("p", { class: "warn", text: "This firmware does not describe its MENU to an editor." })); return; }
  async function load() {
    box.replaceChildren(h("p", { class: "small", text: "Reading the settings ..." }));
    try { draw(await u.readMenu()); } catch (e) { app.fail(e); }
  }
  function draw(items) {
    clear(box);
    const tabs = new Map();
    for (const m of items) { if (m.id === 33) continue;   // BASS MONO (0.2.19) is the GLOBAL tab's MASTER card, not a setting
      if (!tabs.has(m.tab)) tabs.set(m.tab, { name: m.tabName, items: [] }); tabs.get(m.tab).items.push(m); }
    for (const [, t] of [...tabs].sort((a, b) => a[0] - b[0])) {
      const card = h("section", { class: "card" }, h("h3", { text: t.name || "SETTINGS" }));
      const g = h("div", { class: "grid" });
      for (const m of t.items) g.append(menuControl(m));
      card.append(g); box.append(card);
    }
  }
  function menuControl(m) {
    const val = h("span", { class: "val" }), unit = h("span", { class: "unit" });
    const show = () => { val.textContent = m.kind === 0 ? m.names[m.value - m.min] ?? m.value : String(m.value); unit.textContent = m.kind === 1 ? m.unit : ""; };
    const apply = async (v) => {
      if (m.id === 11 && !(await ask("USB SERIAL", ["Changing USB SERIAL makes the unit disconnect from USB and come back (USB-MIDI and USB audio go away for about a second). This page loses the connection: press Connect again afterwards."], "Change", true))) { input.value = m.value; return; }
      try { m.value = await u.setMenu(m.id, v); show(); if (m.id === 11) app.say("The unit re-enumerates: press Connect again in a moment.", "warn"); } catch (e) { app.fail(e); input.value = m.value; }
    };
    const input = m.kind === 0
      ? h("select", { "aria-label": m.name, onchange: (e) => apply(+e.target.value) }, m.names.map((n, i) => h("option", { value: m.min + i, text: n })))
      : h("input", { type: "range", min: m.min, max: m.max, step: 1, "aria-label": m.name, oninput: (e) => { m.value = +e.target.value; show(); }, onchange: (e) => apply(+e.target.value) });
    input.value = m.value; show();
    return h("div", { class: "cell" }, h("div", { class: "top" }, h("span", { class: "lab", text: m.name }), h("span", { class: "right", hidden: m.kind === 0 }, val, unit)), input);
  }
  load();
}

// ---------------------------------------------------------------- MIDI
export function buildMidi(app, wrap, globalCell, card) {
  const u = app.unit;
  wrap.append(card("MIDI IN", "the project's routing", [globalCell(ID.G_ROUTE)], ""),
    h("p", { class: "note", text: "OWN CH: part 1 listens on its channel and part 2 on its channel (set on the unit: GLO > MIDI). SELECTED: every channel plays the part ALGORITHM selected. The parts' channel numbers are not reachable from here yet." }));
  const where = new Map(); for (const pg of L.PAGES) pg.ids.forEach((id) => { if (id >= 0 && !where.has(id) && pg.scope === "TRACK") where.set(id, pg.title); });
  const tbl = h("div", { class: "learn" }), note = h("p", { class: "small" });
  const cc = h("input", { type: "number", min: 0, max: 127, value: 74, "aria-label": "CC number" }), part = h("select", { "aria-label": "Part" }, h("option", { value: 0, text: "PART 1" }), h("option", { value: 1, text: "PART 2" }));
  const pid = h("select", { "aria-label": "Parameter" });
  const fillIds = () => {
    clear(pid); const p = u.parts[+part.value];
    for (let id = 0; id < L.P_COUNT; id++) {
      const d = descOf(p, id); if (!d || d.label[0] === "-" || d.max <= d.min || !(where.has(id) || id >= P_E0)) continue;
      pid.append(h("option", { value: id, text: `${where.get(id) ?? engineLabel(p.eng)} · ${d.label}` }));
    }
  };
  part.addEventListener("change", fillIds); fillIds();
  wrap.append(h("section", { class: "card" }, h("h3", { text: "MIDI LEARN" }), note, tbl,
    h("div", { class: "row" }, h("span", { text: "CC" }), cc, part, pid, h("button", { text: "Assign", onclick: assign }), h("button", { class: "ghost danger", text: "Clear all...", onclick: clearAll })),
    h("p", { class: "small", text: "A learned CC sets one parameter of one part over its whole range, on any channel the MIDI IN setting lets through. Up to 16. The unit keeps the map in its settings. The unit does not say when it learns a CC itself: press Reload." }),
    h("div", { class: "row" }, h("button", { class: "ghost small", text: "Reload", onclick: load }))));
  async function load() { try { draw(await u.readLearn()); } catch (e) { app.fail(e); } }
  function draw(map) {
    clear(tbl);
    if (map == null) { note.textContent = "This firmware has no MIDI learn map."; return; }
    note.textContent = map.length ? `${map.length} of 16 used.` : "No CC is learned.";
    for (const e of map) {
      const p = u.parts[Math.min(e.track, 1)], d = e.track < 2 ? descOf(p, e.id) : null;
      tbl.append(h("div", { class: "lrow" }, h("b", { text: `CC ${e.cc}` }), h("span", { text: `PART ${e.track + 1}` }), h("span", { text: `${where.get(e.id) ?? engineLabel(p.eng)} · ${d?.label ?? e.id}` }),
        h("button", { class: "ghost small", text: "Clear", onclick: async () => { await app.run("Clearing", () => u.learnClear(e.cc)); draw(u.learn); } })));
    }
  }
  async function assign() { await app.run("Assigning", () => u.learnSet(+cc.value, +part.value, +pid.value)); draw(u.learn); }
  async function clearAll() { if (await ask("Clear MIDI learn", ["Every learned CC is cleared (the unit saves the empty map)."], "Clear all", true)) { await app.run("Clearing", () => u.learnClear(null)); draw(u.learn); } }
  load();
}

// ---------------------------------------------------------------- SEQ steps
export function buildSteps(app, p) {
  const u = app.unit, root = h("section", { class: "card steps" }), grid = h("div", { class: "sgrid" }), ed = h("div", { class: "sedit" });
  root.append(h("h3", {}, "STEPS", h("small", { text: " first steps of the pattern (LEN)" })), grid, ed);
  let steps = [], sel = 0, loading = true;
  const N = () => Math.min(u.parts[p].p[ID.P_SLEN], L.NSTEP);
  async function load() {
    loading = true; draw();
    try { steps = await u.readSteps(p, N()); } catch (e) { app.fail(e); steps = []; }
    loading = false; draw();
  }
  function label(s) { return s.time === 2 ? "·" : s.time === 1 ? "—" : s.n ? noteName(s.notes[0]) : s.hits.some((x) => x) ? "▪" : "·"; }
  function draw() {
    clear(grid); clear(ed);
    if (loading) { grid.append(h("span", { class: "small", text: "Reading the steps ..." })); return; }
    steps.forEach((s, i) => grid.append(h("button", { class: "st" + (i === sel ? " sel" : "") + (s.time === 0 && (s.n || s.hits.some((x) => x)) ? " on" : ""), text: label(s), title: `Step ${i + 1}`, "aria-label": `Step ${i + 1}: ${label(s)}`, onclick: () => { sel = i; draw(); } })));
    const s = steps[sel]; if (!s) return;
    const upd = async (patch) => { const n = { ...s, ...patch }; try { steps[sel] = await u.writeStep(p, sel, n); } catch (e) { app.fail(e); } draw(); };
    const time = h("select", { "aria-label": "Step type", onchange: (e) => upd({ time: +e.target.value, n: +e.target.value === 0 ? Math.max(s.n, 1) : s.n }) }, ["NOTE", "TIE", "REST"].map((n, i) => h("option", { value: i, text: n })));
    time.value = s.time;
    const note = h("input", { type: "number", min: 0, max: 127, value: s.notes[0], "aria-label": "Note number", onchange: (e) => { const v = Math.max(0, Math.min(127, +e.target.value | 0)); const nn = s.notes.slice(); nn[0] = v; upd({ notes: nn, n: Math.max(s.n, 1), time: 0 }); } });
    const vel = h("input", { type: "range", min: 0, max: 127, value: s.vel, "aria-label": "Velocity", onchange: (e) => upd({ vel: +e.target.value }) });
    const acc = h("input", { type: "checkbox", checked: !!(s.flags & 1), "aria-label": "Accent", onchange: (e) => upd({ flags: e.target.checked ? s.flags | 1 : s.flags & ~1 }) });
    const sld = h("input", { type: "checkbox", checked: !!(s.flags & 2), "aria-label": "Slide", onchange: (e) => upd({ flags: e.target.checked ? s.flags | 2 : s.flags & ~2 }) });
    const ch = h("input", { type: "range", min: 0, max: 100, value: s.chance, "aria-label": "Chance", onchange: (e) => upd({ chance: +e.target.value }) });
    const rat = h("select", { "aria-label": "Ratchet", onchange: (e) => upd({ ratchet: +e.target.value }) }, [1, 2, 3, 4].map((n) => h("option", { value: n, text: `x${n}` })));
    rat.value = s.ratchet;
    ed.append(h("b", { text: `Step ${sel + 1}` }), h("label", {}, "type", time), h("label", {}, `note ${s.n ? noteName(s.notes[0]) : ""}`, note), h("label", {}, `vel ${s.vel}`, vel), h("label", {}, "accent", acc), h("label", {}, "slide", sld),
      h("label", {}, `chance ${s.chance}%`, ch), h("label", {}, "ratchet", rat),
      ...(s.n > 1 ? [h("span", { class: "small", text: `${s.n} notes in this step; only the first is edited here.` })] : []));
  }
  (app.stepFor ||= [])[p] = async (d) => {            // one handler per part (a rebuilt tab replaces it)
    if (loading || !steps[d.index]) return;
    try { steps[d.index] = parseStep(await u.req(CMD.TRACK_STEP, [p, d.index]), 1); draw(); } catch { /* the next full read shows it */ }
  };
  app.stepChanged = (d) => app.stepFor[d.part]?.(d);
  load();
  return root;
}

// ---------------------------------------------------------------- WAVETABLES (the three user tables of the WAVE engine, caps bit 3)
// A WAV file -> convertWav (wavetable.js) -> the 19 x 64 bytes of a slot, or a "duell-wavetable" file (the bytes as they are). A file is chosen with the button or dropped on a slot
// (on the tab anywhere else: the first empty slot, else USR1). A table on the unit can be saved as a small file to share. Only offered when the unit says it has user tables (INFO caps bit 3).
export function buildWavetables(app, wrap) {
  const u = app.unit, rq = (r, o) => u.link.request(r, o);
  wrap.append(h("p", { class: "note", text: "Three user tables for the WAVE engine: choose TABLE USR1, USR2 or USR3 on a WAVE part. A WAV file with single-cycle frames (for example 2048 samples a frame) is turned into 19 frames of 64 harmonics here in the browser, then sent to the unit. Sending stops the sequencer on the unit and writes its flash memory." }));
  wrap.append(h("p", { class: "note" }, h("b", { text: "What is lost: " }), WTM.LOST_NOTE, " The page shows the result before you send it; the sound on the unit is the sound of those harmonics, not of the file."));
  if (!((u.info.caps ?? 0) & WTM.WT.CAPS_BIT)) { wrap.append(h("p", { class: "warn", text: "This firmware has no user wavetables (they need DUELL 0.2.14 or newer, INFO caps bit 3). Update it with the installer." })); return; }
  const slots = [0, 1, 2].map((s) => slotCard(s));
  wrap.append(h("div", { class: "grid2" }, slots.map((x) => x.el)), h("p", { class: "note", text: "Drop a WAV file or a wavetable file on a slot (or anywhere on this tab) to load it. Export saves a table from the unit as a small file you can share." }));
  let usedNow = [false, false, false];
  async function refresh() {
    try { const l = await WTM.listTables(rq); l.tables.forEach((t) => { usedNow[t.slot] = t.used; slots[t.slot].show(t); }); } catch (e) { app.fail(e); }
  }
  dropZone(wrap, { onDrop: (e) => { const f = filesOf(e)[0]; if (f) slots[Math.max(0, usedNow.indexOf(false)) % 3].take(f); } });
  function slotCard(s) {
    const st = h("span", { class: "small", text: "reading ..." }), info = h("p", { class: "small" }), err = h("p", { class: "warn small" });
    const name = h("input", { type: "text", maxlength: WTM.WT.NAME, size: 9, "aria-label": `USR${s + 1} name`, value: "MYTABLE" });
    const cv = h("canvas", { width: 64, height: 19, style: "width:100%;max-width:384px;height:114px;image-rendering:pixelated;border:1px solid var(--line);background:#000", "aria-label": "Spectra of the 19 frames, 64 harmonics each" });
    const send = h("button", { text: `Send to USR${s + 1}`, disabled: true }), erase = h("button", { class: "ghost", text: "Erase", disabled: true });
    const file = h("input", { type: "file", accept: ".wav,audio/wav,audio/x-wav,.json,application/json", "aria-label": `WAV or wavetable file for USR${s + 1}` });
    const exp = h("button", { class: "ghost", text: "Export file", disabled: true, title: "Save this table as a small file" });
    let result = null, used = false;
    const draw = () => {
      const c = cv.getContext("2d"), img = c.createImageData(64, 19), data = result ? result.data : null;
      for (let i = 0; i < 64 * 19; i++) { const b = data ? data[i] : 0, v = b ? 40 + Math.round((b / 255) * 215) : 0; img.data.set([v, b ? Math.min(255, v * 0.9 + 20) : 0, b ? 60 + (255 - v) / 2 : 0, 255], i * 4); }
      c.putImageData(img, 0, 0);
    };
    draw();
    async function take(f) {
      result = null; send.disabled = true; err.textContent = ""; info.textContent = "";
      if (!f) return draw();
      try {
        const bytes = await readBytes(f);
        if (bytes[0] === 0x7b || /\.json$/i.test(f.name)) {                   // a wavetable file: the bytes as they are
          const t = WTM.parseTableFile(new TextDecoder().decode(bytes));
          if (!t.ok) throw new Error(`${f.name}: ${t.error}`);
          result = { data: t.data }; name.value = t.name; info.textContent = `${f.name}: a wavetable file, 19 frames x 64 harmonics${t.firmware ? ` (made with ${t.firmware.replace(/^FELUCCA /, "")})` : ""}. Nothing is lost on the way.`;
        } else {
          result = WTM.convertWav(bytes);
          const w = result.wav; info.textContent = `${f.name}: ${w.samples.length} samples, ${w.sampleRate} Hz, frame size ${result.frameSize} (${result.sourceFrames} frames) -> 19 frames x 64 harmonics`;
          if (/^[A-Za-z0-9 ._+#-]+$/.test(f.name)) name.value = WTM.cleanName(f.name.replace(/\.[^.]*$/, "")) || name.value;
        }
        send.disabled = false;
      } catch (e) { err.textContent = e.message; result = null; }
      draw();
    }
    file.addEventListener("change", () => take(file.files[0]));
    exp.addEventListener("click", async () => {
      try { const t = await WTM.readTable(rq, s), nm = `${t.name.replace(/[^A-Za-z0-9._-]/g, "") || "table"}.duell-wavetable.json`; saveText(app, nm, WTM.tableToFile(t.name, t.data, { firmware: u.info.version, source: `USR${s + 1}` })); app.say(`Saved ${nm}.`, "good"); }
      catch (e) { err.textContent = e.message; app.fail(e); }
    });
    send.addEventListener("click", async () => {
      if (!result) return;
      if (used && !(await ask(`Replace USR${s + 1}?`, ["The table now in this slot is overwritten. A WAVE sound that uses it will sound different."], "Replace", true))) return;
      send.disabled = true; err.textContent = "";
      try { await WTM.writeTable(rq, s, name.value, result.data, (a, b) => { st.textContent = `sending ${Math.round((100 * a) / b)} %`; }); app.say(`USR${s + 1} written: ${WTM.cleanName(name.value)}.`, "good"); }
      catch (e) { err.textContent = e.message; app.fail(e); }
      send.disabled = !result; await refresh();
    });
    erase.addEventListener("click", async () => {
      if (!(await ask(`Erase USR${s + 1}?`, ["The slot is emptied. A WAVE sound that uses it then plays a plain sine."], "Erase", true))) return;
      try { await WTM.eraseTable(rq, s); app.say(`USR${s + 1} erased.`, "good"); } catch (e) { err.textContent = e.message; app.fail(e); }
      await refresh();
    });
    const el = h("section", { class: "card" }, h("h3", { text: `USR${s + 1}  (TABLE ${WTM.WT.FIRST_TABLE + s})` }), h("div", { class: "row" }, st), h("div", { class: "row" }, file), h("div", { class: "row" }, h("label", {}, "name ", name), send, exp, erase), info, err, cv);
    dropZone(el, { onDrop: (e) => { const f = filesOf(e)[0]; if (f) take(f); } });
    return { el, take, show: (t) => { used = t.used; st.textContent = t.used ? `holds "${t.name}"` : "empty"; erase.disabled = !t.used; exp.disabled = !t.used; } };
  }
  refresh().then(() => { if (app.pendingTable) { const f = app.pendingTable.file; app.pendingTable = null; slots[Math.max(0, usedNow.indexOf(false)) % 3].take(f); } });
}
