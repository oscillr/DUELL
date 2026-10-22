// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the tabs that are not a grid of parameters: PATCHES, MULTIS, SETTINGS (MENU), MIDI (routing and learn map), the SEQ steps, WAVETABLES.
import { h, clear, ask, askText } from "./ui_util.js";
import { L, ID, P_E0, descOf, engineLabel, CATEGORIES } from "./layout.js";
import * as WTM from "./wavetable.js";
import { dropZone, hasFiles, filesOf, readBytes, saveText } from "./dropzone.js";
import * as S from "./stored.js";
import { MENU_LONG, longDesc } from "./labels.js";

const pad = (n, w = 2) => String(n).padStart(w, "0");
const partSel = (app, label = "Part") => {
  const s = h("select", { "aria-label": label }, h("option", { value: 0, text: "PART 1" }), h("option", { value: 1, text: "PART 2" }));
  s.value = app.target; s.addEventListener("change", () => { app.target = +s.value; });
  return s;
};

// ---------------------------------------------------------------- PATCHES (the patch librarian: patches_ui.js)
export { buildPatches } from "./patches_ui.js";

// ---------------------------------------------------------------- MULTIS (0.2.25: the words say where things go; files; no text over the rows)
// Words (owner, T0229): Save to unit / Recall / Load from unit use the unit's own memory (its slots M01..M16); Download file / Import file use files on this computer. No "here".
export function buildMultis(app, wrap) {
  const u = app.unit, box = h("div", { class: "multis", role: "list" }), msg = h("p", { class: "small", role: "status" });
  const fileIn = h("input", { type: "file", accept: ".json,application/json", hidden: true, "aria-label": "Multi file", onchange: async (e) => { const f = e.target.files[0]; e.target.value = ""; if (f) await importFile(f); } });
  const bar = h("div", { class: "row" },
    h("button", { class: "ghost small", text: "Load from unit", title: "Reads which multis you saved and their names from the unit (stops playback on the unit)", onclick: readNames }),
    h("button", { class: "ghost small", text: "Download all", title: "One file on this computer with every multi you saved on the unit", onclick: () => download(null) }),
    h("button", { class: "ghost small", text: "Import file...", title: "A multi file from this computer is written into a slot of the unit you choose", onclick: () => fileIn.click() }), fileIn);
  wrap.append(h("div", { class: "multiswrap" }, bar, msg, box));
  let used = u.multis || null, recs = null;
  const factory = new Map(L.MULTIS);
  const nameOf = (s) => u.multiNames?.[s] ?? (recs?.[s] ? S.multiName(recs[s]) : null) ?? (factory.has(s) ? factory.get(s) : null);
  async function refreshUsed() { try { used = await u.multiUsed(); draw(); } catch (e) { app.fail(e); } }
  async function readNames() { try { app.say("Reading the stored multis ..."); const r = await S.readMultis(u); recs = r.records; await u.readMultiObjects(); draw(); app.say("Names read.", "good"); } catch (e) { app.fail(e); } }
  function draw() {
    clear(box);
    for (let s = 0; s < 16; s++) {
      const stored = recs ? !!recs[s] : u.multiNames ? u.multiNames[s] != null : null, name = nameOf(s);
      const label = used == null ? "…" : name ?? (used[s] ? "(used)" : "(empty)");
      // (editor2, owner item 7: no separate user and factory kinds: no kind words, no bold-versus-plain)
      box.append(h("div", { class: "mrow2", role: "listitem", title: `${S.M_LABEL(s)} ${label}` }, h("b", { text: S.M_LABEL(s) }), h("span", { class: "n", text: label }),
        h("span", { class: "acts" },
          h("button", { class: "ghost small", text: "Recall", title: `Recall ${S.M_LABEL(s)} on the unit (replaces both parts, the effects and the arps)`, disabled: used?.[s] === false, onclick: () => doLoad(s, label) }),
          h("button", { class: "ghost small", text: used?.[s] && name ? `Replace ${S.M_LABEL(s)} ${name}` : "Save to unit", title: `Save what the unit plays now into the unit's slot ${S.M_LABEL(s)} (its own memory; no file on this computer)`, onclick: () => doSave(s, label) }),
          h("button", { class: "ghost small", text: "Download file", title: stored === false ? "A factory multi lives in the firmware: Recall it and save it to the unit once, then it can be downloaded" : `${S.M_LABEL(s)} as a file on this computer`, disabled: stored === false, onclick: () => download(s) }),
          h("button", { class: "ghost small", text: "Rename...", disabled: used?.[s] === false && !factory.has(s), title: "Rename this multi on the unit (a factory multi is saved to the unit under the new name)", onclick: () => doRename(s, label, stored) }))));
    }
  }
  async function doLoad(s, label) {
    if (!(await ask(`Recall ${S.M_LABEL(s)}`, [`Recall "${label}" on the unit: both parts and the effects are replaced by the multi's. Edits not stored are lost.`], "Recall"))) return;
    await app.run(`Recalling ${S.M_LABEL(s)}`, () => u.loadMulti(s));
    app.undo?.forget(); app.refresh();
  }
  async function doSave(s, label) {
    const lines = [`What the unit plays now (both parts, the effects, tempo, mode) is saved into the unit's slot ${S.M_LABEL(s)}. The unit stops playback while it writes (up to two seconds).`];
    if (used?.[s]) lines.unshift(`${S.M_LABEL(s)} holds "${label}". It will be OVERWRITTEN.`);
    if (!(await ask(`Save to unit: ${S.M_LABEL(s)}`, lines, used?.[s] ? "Overwrite" : "Save", !!used?.[s]))) return;
    await app.run(`Saving ${S.M_LABEL(s)}`, () => u.saveMulti(s)); recs = null; draw();
  }
  // RENAME (owner, 0b): every multi, a factory one too: a factory multi (never saved) is recalled, saved to the unit and then renamed, after one short question
  async function doRename(s, label, stored) {
    const fresh = stored !== true && !u.multiNames?.[s];
    const nn = await askText(`Rename ${S.M_LABEL(s)}`, fresh ? [`"${label}" is a factory multi: it is recalled (it replaces what the unit plays now) and saved to the unit under the new name.`] : [], u.multiNames?.[s] || label, "Rename");
    if (!nn) return;
    await app.run("Renaming", async () => {
      if (fresh) { await u.loadMulti(s); app.undo?.forget(); await u.saveMulti(s); }
      await u.renameMulti(s, nn);
    });
    recs = null; app.refresh(); draw();
  }
  async function download(slot) {
    try {
      app.say("Reading the stored multis ...");
      const r = await S.readMultis(u); recs = r.records;
      const list = (slot == null ? [...Array(16).keys()] : [slot]).filter((s) => recs[s]).map((s) => ({ slot: s, bytes: recs[s] }));
      if (!list.length) { app.say(slot == null ? "No multi is saved on the unit (the factory multis live in the firmware: Recall one and save it to the unit to keep a copy)." : `${S.M_LABEL(slot)} is not saved on the unit: Recall it and save it to the unit once, then download it.`, "warn"); draw(); return; }
      const name = slot == null ? `duell-multis-${list.length}.duell-multis.json` : `duell-multi-${S.M_LABEL(slot)}-${S.multiName(recs[slot]).replace(/[^A-Za-z0-9._-]+/g, "_") || "multi"}.duell-multis.json`;
      saveText(app, name, JSON.stringify(S.multiFile(list, u.info.version), null, 1) + "\n");
      app.say(`Downloaded ${name} to this computer (${list.length} multi${list.length > 1 ? "s" : ""}).`, "good"); draw();
    } catch (e) { app.fail(e); }
  }
  async function importFile(f) {
    const r = S.parseMultiFile(new TextDecoder().decode(await readBytes(f)));
    if (!r.ok) { app.say(`${f.name}: ${r.error}`, "warn"); return; }
    const ok = r.multis.filter((m) => m.bytes && !m.problem);
    if (!ok.length) { app.say(`${f.name}: ${r.multis[0]?.problem ? `it cannot go into this unit: ${r.multis[0].problem}` : "no multi in it."}`, "warn"); return; }
    const pick = await new Promise((resolve) => {
      const dlg = h("dialog", { class: "ask", "aria-label": "Import a multi" });
      const which = h("select", { "aria-label": "Multi from the file" }, ok.map((m, i) => h("option", { value: i, text: `${m.slot != null ? S.M_LABEL(m.slot) + "  " : ""}${m.name}` })));
      const slot = h("select", { "aria-label": "Unit slot" }, Array.from({ length: 16 }, (_, s) => h("option", { value: s, text: `${S.M_LABEL(s)}  ${nameOf(s) ?? ""}` })));
      const sync = () => { const m = ok[+which.value]; if (m.slot != null) slot.value = m.slot; };
      which.addEventListener("change", sync); sync();
      const done = (v) => { dlg.close(); dlg.remove(); resolve(v); };
      dlg.append(h("h2", { text: "Import a multi into the unit" }),
        h("p", { text: `From the file ${f.name}${r.firmware ? ` (made with ${r.firmware.replace(/^FELUCCA /, "")})` : ""}. The multi is written into the slot you choose, in the unit's own memory. What that slot holds now is replaced (a factory multi comes back if you erase it on the unit later). The unit stops playback while it writes.` }),
        r.multis.some((m) => m.problem) ? h("p", { class: "warn small", text: `Left out: ${r.multis.filter((m) => m.problem).map((m) => `${m.name} (${m.problem})`).join("; ")}.` }) : null,
        h("label", { class: "row" }, h("span", { text: "Multi" }), which), h("label", { class: "row" }, h("span", { text: "Into the unit's slot" }), slot),
        h("div", { class: "row end" }, h("button", { class: "ghost", text: "Cancel", onclick: () => done(null) }), h("button", { class: "danger", text: "Write into the slot", onclick: () => done({ m: ok[+which.value], slot: +slot.value }) })));
      dlg.addEventListener("cancel", (e) => { e.preventDefault(); done(null); }); document.body.append(dlg); dlg.showModal();
    });
    if (!pick) return;
    await app.run(`Writing "${pick.m.name}" into ${S.M_LABEL(pick.slot)}`, async () => { await S.importMulti(u, pick.slot, pick.m.bytes); await u.readMultiObjects(); recs = null; used = await u.multiUsed(); });
    draw();
  }
  dropZone(wrap, { onDrop: (e) => { const f = filesOf(e)[0]; if (f) importFile(f); } });
  draw(); refreshUsed();
}

// ---------------------------------------------------------------- PATTERNS (0.2.25: the pattern slots P01.., stored.js; editor2: ONE list for the whole device, none empty, nothing protected)
// Recall into part / Save to unit act on the part the editor has selected (app.target: the part whose tab was used last); the part is named in the buttons' tooltips only.
export function buildPatterns(app, wrap) {
  const u = app.unit, box = h("div", { class: "multis pats", role: "list" });
  const fileIn = h("input", { type: "file", accept: ".json,application/json", hidden: true, "aria-label": "Pattern file", onchange: async (e) => { const f = e.target.files[0]; e.target.value = ""; if (f) await importFile(f); } });
  wrap.append(h("div", { class: "multiswrap" },
    h("div", { class: "row" },
      h("button", { class: "ghost small", text: "Load from unit", title: "Reads the pattern slots from the unit (stops playback on the unit)", onclick: load }),
      h("button", { class: "ghost small", text: "Download all", title: "One file on this computer with every pattern slot", onclick: () => download(null) }),
      h("button", { class: "ghost small", text: "Import file...", onclick: () => fileIn.click() }), fileIn),
    box));
  let slots = null, objs = null;
  const part = () => app.target ?? 0;
  async function load() { try { app.say("Reading the pattern slots ..."); const r = await S.readPatterns(u); slots = r.slots; objs = r.objs; draw(); if (slots.length) app.say("Pattern slots read.", "good"); else app.say("This unit does not report its 32 pattern slots (its firmware is older than this editor).", "warn"); } catch (e) { app.fail(e); } }
  function draw() {
    clear(box);
    if (!slots) return;
    for (let s = 0; s < slots.length; s++) {
      const it = slots[s];
      box.append(h("div", { class: "mrow2", role: "listitem", title: it ? `${S.P_LABEL(s)} ${it.name}: ${it.len} steps` : S.P_LABEL(s) }, h("b", { text: S.P_LABEL(s) }), h("span", { class: "n", text: it ? it.name || "(no name)" : "" }),
        h("span", { class: "acts" },
          h("button", { class: "ghost small", text: "Recall into part", disabled: !it, title: `Its steps and settings replace the pattern of PART ${part() + 1} (the sound stays)`, onclick: () => recall(s, part()) }),
          h("button", { class: "ghost small", text: it ? `Replace ${S.P_LABEL(s)} ${it.name}`.trim() : "Save to unit", title: `Save the pattern of PART ${part() + 1} into this slot of the unit`, onclick: () => store(s, part()) }),
          h("button", { class: "ghost small", text: "Download file", disabled: !it, onclick: () => download(s) }),
          h("button", { class: "ghost small", text: "Rename...", disabled: !it, onclick: () => rename(s) }))));
    }
  }
  const rec = (s) => S.patternRecord(objs, s);
  async function recall(s, p) {
    if (!(await ask(`Recall ${S.P_LABEL(s)} into PART ${p + 1}`, [`"${slots[s].name || S.P_LABEL(s)}" replaces PART ${p + 1}'s pattern (64 steps, LENGTH, DIVISION, SWING, GATE, QUANTIZE). The sound stays. Nothing is saved until you save a patch or a multi.`], "Recall"))) return;
    await app.run(`Recalling ${S.P_LABEL(s)}`, () => S.recallPattern(u, p, rec(s), (i, n) => app.say(`Recalling ${S.P_LABEL(s)}: step ${i} of ${n} ...`)));
    app.stores?.[p]?.invalidate(); app.refresh();
  }
  async function store(s, p) {
    const nn = await askText(`Save PART ${p + 1}'s pattern to ${S.P_LABEL(s)}`, [...(slots?.[s] ? [`${S.P_LABEL(s)} holds "${slots[s].name}". It will be OVERWRITTEN.`] : []), "The unit stops playback while it writes."], slots?.[s]?.name || `PATTERN ${s + 1}`, "Save");
    if (!nn) return;
    await app.run(`Saving ${S.P_LABEL(s)}`, async () => { const r = await S.patternOfPart(u, p, nn); await S.storePattern(u, s, r); });
    await load();
  }
  async function rename(s) {
    const nn = await askText(`Rename ${S.P_LABEL(s)}`, ["Only the name changes. The unit stops playback while it writes."], slots[s].name, "Rename");
    if (!nn) return;
    await app.run("Renaming", async () => { const d = S.decodePattern(rec(s)); d.name = nn; await S.storePattern(u, s, S.encodePattern(d)); });
    await load();
  }
  async function download(slot) {
    if (!slots) await load(); if (!slots) return;
    const list = (slot == null ? [...Array(slots.length).keys()] : [slot]).filter((s) => rec(s)).map((s) => ({ slot: s, bytes: rec(s) }));
    if (!list.length) { app.say("No pattern slot holds a pattern.", "warn"); return; }
    const name = slot == null ? `duell-patterns-${list.length}.duell-patterns.json` : `duell-pattern-${S.P_LABEL(slot)}.duell-patterns.json`;
    saveText(app, name, JSON.stringify(S.patternFile(list, u.info.version), null, 1) + "\n");
    app.say(`Downloaded ${name} to this computer.`, "good");
  }
  async function importFile(f) {
    const r = S.parsePatternFile(new TextDecoder().decode(await readBytes(f)));
    if (!r.ok) { app.say(`${f.name}: ${r.error}`, "warn"); return; }
    const ok = r.patterns.filter((x) => x.bytes);
    if (!ok.length) { app.say(`${f.name}: no readable pattern in it.`, "warn"); return; }
    if (!slots) await load();
    const pick = await new Promise((resolve) => {
      const dlg = h("dialog", { class: "ask", "aria-label": "Import a pattern" });
      const which = h("select", { "aria-label": "Pattern from the file" }, ok.map((m, i) => h("option", { value: i, text: `${m.slot != null ? S.P_LABEL(m.slot) + "  " : ""}${m.name}` })));
      const to = h("select", { "aria-label": "Where" }, h("option", { value: "p0", text: "into PART 1 (its pattern now, not stored)" }), h("option", { value: "p1", text: "into PART 2 (its pattern now, not stored)" }),
        Array.from({ length: slots?.length ?? 0 }, (_, s) => h("option", { value: `s${s}`, text: `into the unit's slot ${S.P_LABEL(s)}  ${slots?.[s] ? `(replaces "${slots[s].name}")` : ""}`.trim() })));
      const done = (v) => { dlg.close(); dlg.remove(); resolve(v); };
      dlg.append(h("h2", { text: "Import a pattern" }), h("p", { text: `From the file ${f.name}.` }), h("label", { class: "row" }, h("span", { text: "Pattern" }), which), h("label", { class: "row" }, h("span", { text: "Where to" }), to),
        h("div", { class: "row end" }, h("button", { class: "ghost", text: "Cancel", onclick: () => done(null) }), h("button", { text: "Import", onclick: () => done({ m: ok[+which.value], to: to.value }) })));
      dlg.addEventListener("cancel", (e) => { e.preventDefault(); done(null); }); document.body.append(dlg); dlg.showModal();
    });
    if (!pick) return;
    if (pick.to[0] === "p") { const p = +pick.to[1]; await app.run(`Recalling "${pick.m.name}" into PART ${p + 1}`, () => S.recallPattern(u, p, pick.m.bytes)); app.stores?.[p]?.invalidate(); app.refresh(); }
    else { const s = +pick.to.slice(1); await app.run(`Writing "${pick.m.name}" into ${S.P_LABEL(s)}`, () => S.storePattern(u, s, pick.m.bytes)); await load(); }
  }
  draw(); load();
}

// ---------------------------------------------------------------- SETTINGS (MENU)
export function buildSettings(app, wrap) {
  const u = app.unit, box = h("div", { class: "menu tabbody" });
  wrap.append(box);
  if (u.info.menuCount == null) { box.append(h("p", { class: "warn", text: "This firmware does not describe its MENU to an editor." })); return; }
  async function load() {
    box.replaceChildren(h("p", { class: "small", text: "Reading the settings ..." }));
    try { draw(await u.readMenu()); } catch (e) { app.fail(e); }
  }
  function draw(items) {
    clear(box);
    const tabs = new Map();
    for (const m of items) { if (m.id >= 33 && m.id <= 43) continue;   // BASS MONO (0.2.19, id 33), COMP (0.2.21, ids 34..37), SIDECHAIN (0.2.22, ids 38..41), WIDTH (0.2.23, id 42; id 43, DRIVE, is gone) are the GLOBAL tab's MASTER and SIDECHAIN cards, not settings
      if (!tabs.has(m.tab)) tabs.set(m.tab, { name: m.tabName, items: [] }); tabs.get(m.tab).items.push(m); }
    for (const [, t] of [...tabs].sort((a, b) => a[0] - b[0])) {
      const card = h("section", { class: "card" }, h("h3", {}, h("span", { class: "ttl", text: t.name || "SETTINGS", title: "The unit's own MENU settings (names, ranges and tabs come from the unit). A change is applied at once; the unit writes it to its memory when playback is stopped." })));
      const g = h("div", { class: "grid" });
      for (const m of t.items) g.append(menuControl(m));
      card.append(g); box.append(card);
    }
  }
  function menuControl(m) {
    const val = h("span", { class: "val" }), unit = h("span", { class: "unit" });
    const show = () => { val.textContent = m.kind === 0 ? m.names[m.value - m.min] ?? m.value : m.kind === 2 ? (m.value / 10).toFixed(1) : String(m.value); unit.textContent = m.kind !== 0 ? m.unit : ""; };   // kind 2 (0.2.22): tenths of the unit, one decimal
    const apply = async (v) => {
      if (m.id === 11 && !(await ask("USB SERIAL", ["Changing USB SERIAL makes the unit disconnect from USB and come back (USB-MIDI and USB audio go away for about a second). This page loses the connection: press Connect again afterwards."], "Change", true))) { input.value = m.value; return; }
      try { m.value = await u.setMenu(m.id, v); show(); if (m.id === 11) app.say("The unit re-enumerates: press Connect again in a moment.", "warn"); } catch (e) { app.fail(e); input.value = m.value; }
    };
    const input = m.kind === 0
      ? h("select", { "aria-label": m.name, onchange: (e) => apply(+e.target.value) }, m.names.map((n, i) => h("option", { value: m.min + i, text: n })))
      : h("input", { type: "range", min: m.min, max: m.max, step: 1, "aria-label": m.name, oninput: (e) => { m.value = +e.target.value; show(); }, onchange: (e) => apply(+e.target.value) });
    input.value = m.value; show();
    return h("div", { class: "cell", title: m.name !== (MENU_LONG[m.name] ?? m.name) ? `On the unit: ${m.name}` : "" }, h("div", { class: "top" }, h("span", { class: "lab", text: MENU_LONG[m.name] ?? m.name }), h("span", { class: "right", hidden: m.kind === 0 }, val, unit)), input);
  }
  load();
}

// ---------------------------------------------------------------- MIDI
export function buildMidi(app, wrap, globalCell, card, sidechain) {
  const u = app.unit;
  const shown = (id, label, p) => longDesc(descOf(p, id), id >= P_E0 ? `e:${engineLabel(p.eng)}` : id)?.label ?? label;   // (T0229: the editor's names, never the unit's short codes)
  wrap.append(h("div", { class: "tabbody" }, card("MIDI IN", [globalCell(ID.G_ROUTE)], { tip: "OWN CHANNEL: part 1 listens on its channel and part 2 on its channel (set on the unit: GLO > MIDI). SELECTED PART: every channel plays the part ALGORITHM selected." }), sidechain));
  const where = new Map(); for (const pg of L.PAGES) pg.ids.forEach((id) => { if (id >= 0 && !where.has(id) && pg.scope === "TRACK") where.set(id, pg.title); });
  const tbl = h("div", { class: "lrows" }), note = h("p", { class: "warn small" });
  const cc = h("input", { type: "number", min: 0, max: 127, value: 74, "aria-label": "CC number" }), part = h("select", { "aria-label": "Part" }, h("option", { value: 0, text: "PART 1" }), h("option", { value: 1, text: "PART 2" }));
  const pid = h("select", { "aria-label": "Parameter" });
  const fillIds = () => {
    clear(pid); const p = u.parts[+part.value];
    for (let id = 0; id < L.P_COUNT; id++) {
      const d = descOf(p, id); if (!d || d.label[0] === "-" || d.max <= d.min || !(where.has(id) || id >= P_E0)) continue;
      pid.append(h("option", { value: id, text: `${where.get(id) ?? engineLabel(p.eng)} · ${shown(id, d.label, p)}` }));
    }
  };
  part.addEventListener("change", fillIds); fillIds();
  wrap.firstElementChild.append(h("section", { class: "card learn" }, h("h3", {}, h("span", { class: "ttl", text: "MIDI LEARN", title: "A learned CC sets one parameter of one part over its whole range, on any channel MIDI IN lets through. Up to 16; the unit keeps the map in its settings. The unit does not say when it learns a CC itself: press Reload." })), note, tbl,
    h("div", { class: "row" }, h("span", { text: "CC" }), cc, part, pid, h("button", { class: "small", text: "Assign", onclick: assign }), h("button", { class: "ghost small danger", text: "Clear all...", onclick: clearAll }), h("button", { class: "ghost small", text: "Reload", onclick: load }))));
  async function load() { try { draw(await u.readLearn()); } catch (e) { app.fail(e); } }
  function draw(map) {
    clear(tbl);
    if (map == null) { note.textContent = "This firmware has no MIDI learn map."; return; }
    note.textContent = "";
    for (const e of map) {
      const p = u.parts[Math.min(e.track, 1)], d = e.track < 2 ? descOf(p, e.id) : null;
      tbl.append(h("div", { class: "lrow" }, h("b", { text: `CC ${e.cc}` }), h("span", { text: `PART ${e.track + 1}` }), h("span", { text: `${where.get(e.id) ?? engineLabel(p.eng)} · ${d ? shown(e.id, d.label, p) : e.id}` }),
        h("button", { class: "ghost small", text: "Clear", onclick: async () => { await app.run("Clearing", () => u.learnClear(e.cc)); draw(u.learn); } })));
    }
  }
  async function assign() { await app.run("Assigning", () => u.learnSet(+cc.value, +part.value, +pid.value)); draw(u.learn); }
  async function clearAll() { if (await ask("Clear MIDI learn", ["Every learned CC is cleared (the unit saves the empty map)."], "Clear all", true)) { await app.run("Clearing", () => u.learnClear(null)); draw(u.learn); } }
  load();
}

// ---------------------------------------------------------------- WAVETABLES (the three user tables of the WAVE engine, caps bit 3)
// A WAV file -> convertWav (wavetable.js) -> the 19 x 64 bytes of a slot, or a "duell-wavetable" file (the bytes as they are). A file is chosen with the button or dropped on a slot
// (on the tab anywhere else: the first empty slot, else USR1). A table on the unit can be saved as a small file to share. Only offered when the unit says it has user tables (INFO caps bit 3).
export function buildWavetables(app, wrap) {
  const u = app.unit, rq = (r, o) => u.link.request(r, o);
  if (!((u.info.caps ?? 0) & WTM.WT.CAPS_BIT)) { wrap.append(h("p", { class: "warn", text: "This firmware has no user wavetables (they need DUELL 0.2.14 or newer, INFO caps bit 3). Update it with the installer." })); return; }
  const slots = [0, 1, 2].map((s) => slotCard(s));
  wrap.append(h("div", { class: "grid2", title: "Drop a WAV file or a wavetable file on a slot (or anywhere on this tab) to load it." }, slots.map((x) => x.el)));
  let usedNow = [false, false, false];
  async function refresh() {
    try { const l = await WTM.listTables(rq); l.tables.forEach((t) => { usedNow[t.slot] = t.used; slots[t.slot].show(t); }); } catch (e) { app.fail(e); }
  }
  dropZone(wrap, { onDrop: (e) => { const f = filesOf(e)[0]; if (f) slots[Math.max(0, usedNow.indexOf(false)) % 3].take(f); } });
  function slotCard(s) {
    const st = h("span", { class: "small", text: "reading ..." }), info = h("p", { class: "small" }), err = h("p", { class: "warn small" });
    const name = h("input", { type: "text", maxlength: WTM.WT.NAME, size: 9, "aria-label": `USER ${s + 1} name`, value: "MYTABLE" });
    const send = h("button", { class: "small", text: "Save to unit", disabled: true, title: `Writes the table into USER ${s + 1} on the unit (it stops the sequencer and writes its flash memory). What is lost: ${WTM.LOST_NOTE}` }), erase = h("button", { class: "ghost small", text: "Erase", disabled: true });
    const file = h("input", { type: "file", accept: ".wav,audio/wav,audio/x-wav,.json,application/json", "aria-label": `WAV or wavetable file for USER ${s + 1}` });
    const exp = h("button", { class: "ghost small", text: "Download file", disabled: true, title: "This table as a small file on this computer" });
    let result = null, used = false;
    const draw = () => {};                              // (T0229: no spectrum picture in the editor)
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
      try { const t = await WTM.readTable(rq, s), nm = `${t.name.replace(/[^A-Za-z0-9._-]/g, "") || "table"}.duell-wavetable.json`; saveText(app, nm, WTM.tableToFile(t.name, t.data, { firmware: u.info.version, source: `USER ${s + 1}` })); app.say(`Saved ${nm}.`, "good"); }
      catch (e) { err.textContent = e.message; app.fail(e); }
    });
    send.addEventListener("click", async () => {
      if (!result) return;
      if (used && !(await ask(`Replace USER ${s + 1}?`, ["The table now in this slot is overwritten. A WAVE sound that uses it will sound different."], "Replace", true))) return;
      send.disabled = true; err.textContent = "";
      try { await WTM.writeTable(rq, s, name.value, result.data, (a, b) => { st.textContent = `sending ${Math.round((100 * a) / b)} %`; }); app.say(`USER ${s + 1} saved: ${WTM.cleanName(name.value)}.`, "good"); }
      catch (e) { err.textContent = e.message; app.fail(e); }
      send.disabled = !result; await refresh();
    });
    erase.addEventListener("click", async () => {
      if (!(await ask(`Erase USER ${s + 1}?`, ["The slot is emptied. A WAVE sound that uses it then plays a plain sine."], "Erase", true))) return;
      try { await WTM.eraseTable(rq, s); app.say(`USER ${s + 1} erased.`, "good"); } catch (e) { err.textContent = e.message; app.fail(e); }
      await refresh();
    });
    const el = h("section", { class: "card" }, h("h3", {}, h("span", { class: "ttl", text: `USER ${s + 1}`, title: `Table ${WTM.WT.FIRST_TABLE + s} of the WAVE engine's TABLE list` }), st), h("div", { class: "row" }, file), h("div", { class: "row" }, h("label", {}, "NAME ", name), send, exp, erase), info, err);
    dropZone(el, { onDrop: (e) => { const f = filesOf(e)[0]; if (f) take(f); } });
    return { el, take, show: (t) => { used = t.used; st.textContent = t.used ? `holds "${t.name}"` : "empty"; erase.disabled = !t.used; exp.disabled = !t.used; } };
  }
  refresh().then(() => { if (app.pendingTable) { const f = app.pendingTable.file; app.pendingTable = null; slots[Math.max(0, usedNow.indexOf(false)) % 3].take(f); } });
}
