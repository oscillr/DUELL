// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor, Backup page: the DOM side (duellbackup.js holds the logic and the tests). Text from a file or a unit is only ever set with
// textContent. Nothing here sends a frame except the INFO and BACKUP_* requests; no update frames, no addresses.
import { BackupConnection } from "../fm1backup.js";
import { detectUnit } from "../installer_state.js";
import { sysexGranted, watchUnit, HINT } from "./autoconnect.js";
import { GROUPS, CAPS_MULTI_PATTERN, parseInfo, readArchive, captureBackup, listUnit, planRestore, runRestore, verifyRestore, describe, fileName } from "./duellbackup.js";

const $ = (id) => document.getElementById(id);
const params = new URLSearchParams(location.search);
const MOCK = params.get("mock");
let link = null, request = null, info = null, unitList = null, archive = null, busy = false, midi = null, watched = null, ports = [], auto = false;   // auto: connect by itself when the unit appears (MIDI already allowed)
const sel = new Set(GROUPS.map((g) => g.key)), touched = new Set();   // the groups to restore; the ones the user ticked or unticked himself

const say = (id, t, cls = "") => { const e = $(id); e.textContent = t; e.className = e.className.replace(/\b(warn|good)\b/g, "").trim() + (cls ? " " + cls : ""); };
const bar = (id, v) => { const b = $(id); b.hidden = v == null; if (v != null) b.value = v; };
const isDuell = () => !!info && detectUnit(info.version).kind === "duell";
const hasCaps = () => !!info && ((info.caps ?? 0) & CAPS_MULTI_PATTERN) === CAPS_MULTI_PATTERN;
const unitName = () => info.version.replace(/^FELUCCA /, "");

function refresh() {
  $("connect").disabled = busy || !!link && isDuell();
  $("backup").disabled = busy || !link || !isDuell();
  const canRestore = !!archive && !!link && isDuell() && [...sel].length > 0 && !busy;
  $("restore").disabled = !canRestore;
  $("file").disabled = busy;
  $("settingsnote").hidden = !archive || !sel.has("settings");
  document.querySelectorAll("#group-rows input").forEach((c) => { c.disabled = busy || c.dataset.off === "1"; });
}
window.addEventListener("beforeunload", (e) => { if (busy) { e.preventDefault(); e.returnValue = "Working with the unit. Do not close the page."; } });

if (!MOCK && !navigator.requestMIDIAccess) {
  say("browser", "This browser cannot talk to the FM-1. Open this page in Chrome or Edge on a computer.", "warn");
  $("connect").disabled = true;
} else if (!MOCK) say("browser", "Browser OK (WebMIDI available).");
if (MOCK) { $("mockbar").hidden = false; say("browser", "Simulated unit (?mock): " + (MOCK === "old" ? "firmware 0.2.13" : MOCK === "playing" ? "a unit that is playing" : "firmware 0.2.14") + "."); }

// ---- connect
function drop(why) {
  link?.close?.();
  link = null; request = null; info = null; unitList = null;
  say("unit", why, "warn"); say("unitnote", ""); renderGroups(); refresh();
}
async function connect() {
  if (busy || link) return;
  busy = true; auto = true; refresh(); say("unit", "Looking for the FM-1 ..");
  try {
    if (MOCK) {
      const { MockUnit } = await import("./mockunit.js");
      const unit = new MockUnit({ caps: MOCK === "old" ? 3 : 31, playing: MOCK === "playing", version: MOCK === "old" ? "FELUCCA DUELL 0.2.13 bet" : undefined });
      request = (r, o) => unit.request(r, o);
    } else {
      try { midi = await navigator.requestMIDIAccess({ sysex: true }); } catch { say("unit", "MIDI access with SysEx was refused. Allow it and press Connect again.", "warn"); return; }
      const named = (p) => /felucca|duell/i.test(p.name || "") && p.state !== "disconnected";
      const input = [...midi.inputs.values()].find(named), output = [...midi.outputs.values()].find(named);
      if (watched !== midi) {   // (the unit may come later: watched from here on)
        watched = midi;
        midi.onstatechange = (e) => { if (link && e.port.state === "disconnected" && ports.includes(e.port)) { if (!busy) drop("The FM-1 was disconnected."); else say("unit", "The FM-1 was disconnected.", "warn"); } };
        watchUnit(midi, { wanted: () => auto && !link && !busy, go: connect });
      }
      if (!input || !output) { say("unit", "No FM-1 found. Connect it by USB, close other programs that use it (an editor, the installer) and press Connect again.", "warn"); return; }
      link = new BackupConnection(input, output);
      request = (r, o) => link.request(r, o);
      ports = [input, output];
    }
    info = parseInfo(await request([1, []], { timeout: 2000 }));
    if (!isDuell()) { const v = info.version; info = null; link?.close?.(); link = null; request = null; say("unit", `This unit does not run DUELL (it reports "${v}"). Nothing was changed.`, "warn"); return; }
    link = link || {};    // (mock: a placeholder so the buttons know a unit is there)
    say("unit", `Connected: ${unitName()}`, "good");
    say("unitnote", hasCaps()
      ? "This firmware backs up and restores patches, multis, pattern slots, settings and the current music."
      : "This firmware (DUELL 0.2.13 or older) cannot back up or restore multis and pattern slots. A backup from it holds patches, settings and the current music. Update the firmware with the installer for a complete backup.");
    if (!hasCaps()) $("unitnote").className = "small warn";
    else $("unitnote").className = "small";
  } catch (e) {
    link?.close?.();
    link = null; request = null; info = null;
    say("unit", `The unit did not answer (${e.message}). Check the USB cable and that no other page uses the FM-1.`, "warn");
  } finally { busy = false; renderGroups(); refresh(); }
}
$("connect").addEventListener("click", connect);
if (!MOCK && navigator.requestMIDIAccess) sysexGranted().then((ok) => { if (ok) connect(); else say("unit", HINT); });   // MIDI already allowed: connect by itself

// ---- download
function download(name, data) {
  const url = URL.createObjectURL(new Blob([JSON.stringify(data)], { type: "application/json" }));
  const a = Object.assign(document.createElement("a"), { href: url, download: name });
  document.body.appendChild(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 10000);
}
const plural = (n, w, many = w + "s") => `${n} ${n === 1 ? w : many}`;
function fill(el, list, none) {
  el.replaceChildren();
  if (!list.length) { const p = document.createElement("p"); p.className = "small"; p.textContent = none; el.append(p); return; }
  const u = document.createElement("ul"); u.className = "names";
  for (const t of list) { const li = document.createElement("li"); li.textContent = t; u.append(li); }
  el.append(u);
}
function showContents(rootId, objects, engines, hasMulti) {
  const d = describe(objects, engines), root = $(rootId); root.replaceChildren();
  const add = (title, list, none) => {
    const h = document.createElement("p"), holder = document.createElement("div");
    h.textContent = title; h.style.margin = "8px 0 0"; h.style.fontWeight = "700";
    root.append(h, holder); fill(holder, list, none);
  };
  add("Saved patches", d.patches.map((p) => `${p.name || "(no name)"}  ${p.engineName}`), "No saved patches (every slot holds its factory sound).");
  add(hasMulti ? `Multis (${d.multis.length})` : "Multis (not in this file)", d.multis.map((m) => `${m.label}  ${m.name || "(no name)"}`),
      hasMulti ? "No stored multis (the factory multis show)." : "Made by firmware that cannot back up multis.");
  add(hasMulti ? `Pattern slots (${d.patterns.length})` : "Pattern slots (not in this file)", d.patterns.map((p) => `${p.label}  ${p.name || "(no name)"}  ${p.length} steps`),
      hasMulti ? "No stored patterns." : "Made by firmware that cannot back up pattern slots.");
  return d;
}

// ---- back up
async function backup() {
  if (busy || !request) return;
  busy = true; refresh(); say("result-backup", ""); $("saved").hidden = true;
  try {
    say("status-backup", "Saving backup 0%"); bar("bar-backup", 0);
    const file = await captureBackup(request, info, (d, t) => { const pc = d / Math.max(1, t) * 100; say("status-backup", `Saving backup ${Math.round(pc)}%`); bar("bar-backup", pc); });
    const name = fileName("DUELL-backup");
    download(name, file);
    const a = readArchive(file), d = showContents("saved-list", new Map([...a.objects].map(([id, o]) => [id, o.bytes])), a.engines, a.objects.has(13));
    $("saved").hidden = false;
    say("status-backup", "Done.", "good");
    say("result-backup", `Saved ${name} to your downloads: ${plural(d.patches.length, "saved patch", "saved patches")}${a.objects.has(13) ? `, ${plural(d.multis.length, "multi")}, ${plural(d.patterns.length, "pattern slot")}` : " (this firmware cannot back up multis and pattern slots)"}, settings and the current music.`);
  } catch (e) { say("status-backup", "The backup failed. Nothing was saved.", "warn"); say("result-backup", e.message); }
  finally { bar("bar-backup", null); busy = false; refresh(); }
}
$("backup").addEventListener("click", backup);

// ---- restore: choose a file
function groupRows() {   // what each group would do, from the file and (when connected) the unit
  return GROUPS.map((g) => {
    const inFile = archive && (g.key === "patches" ? archive.objects.has(6) : g.ids.every((id) => archive.objects.has(id)));
    let off = null;
    if (!inFile) off = "not in this file";
    else if (info && g.caps && !hasCaps()) off = "this unit's firmware cannot restore them (update it first)";
    return { g, off };
  });
}
function renderGroups() {
  const holder = $("group-rows"); holder.replaceChildren();
  $("groups").hidden = !archive;
  if (!archive) return;
  const d = describe(new Map([...archive.objects].map(([id, o]) => [id, o.bytes])), archive.engines);
  const counts = { patches: plural(d.patches.length, "saved patch", "saved patches"), multis: plural(d.multis.length, "multi"), patterns: plural(d.patterns.length, "pattern slot"), settings: "", music: "" };
  for (const { g, off } of groupRows()) {
    if (off) sel.delete(g.key); else if (!touched.has(g.key)) sel.add(g.key);
    const label = document.createElement("label"), box = document.createElement("input");
    box.type = "checkbox"; box.dataset.key = g.key; box.checked = sel.has(g.key) && !off; box.dataset.off = off ? "1" : "0"; box.disabled = !!off;
    box.addEventListener("change", () => { touched.add(g.key); if (box.checked) sel.add(g.key); else sel.delete(g.key); refresh(); });
    label.append(box, ` ${g.label}${counts[g.key] && !off ? " (" + counts[g.key] + ")" : ""}${off ? ": " + off : ""}`);
    if (off) label.className = "off";
    holder.append(label);
  }
}
$("file").addEventListener("change", async () => {
  archive = null; $("contents").hidden = true; say("result-restore", ""); say("status-restore", ""); sel.clear(); touched.clear();
  const f = $("file").files[0];
  if (f) {
    try {
      if (f.size > 200000) throw new Error("This file is too large to be a DUELL backup.");
      archive = readArchive(await f.text());
      const when = Date.parse(archive.created) ? new Date(archive.created).toLocaleString() : "an unknown date";
      say("filestate", `OK: a backup of ${archive.firmware.replace(/^FELUCCA /, "") || "a DUELL unit"}, made ${when}. It passed every check.`, "good");
      showContents("contents-list", new Map([...archive.objects].map(([id, o]) => [id, o.bytes])), archive.engines, archive.objects.has(13));
      $("contents").hidden = false;
    } catch (e) { say("filestate", `Cannot use this file: ${e.message}`, "warn"); }
  } else say("filestate", "");
  renderGroups(); refresh();
});

// ---- restore: go
async function restore() {
  if (busy || !archive || !request) return;
  busy = true; refresh(); say("result-restore", ""); say("status-restore", "Checking the unit ..");
  let safetyName = null;
  try {
    unitList = await listUnit(request);
    const plan = planRestore(archive, unitList, info.caps, sel);
    if (!plan.steps.length) { say("status-restore", "Nothing to restore.", "warn"); say("result-restore", plan.excluded.map((x) => `${x.label}: ${x.reason}`).join("\n")); return; }
    const kinds = [...new Set(plan.steps.map((s) => s.group))].map((k) => GROUPS.find((g) => g.key === k).label.toLowerCase());
    const text = `Replace on ${unitName()}: ${kinds.join(", ")}.\n\n` +
      (plan.excluded.length ? `Not restored: ${plan.excluded.map((x) => `${x.label.toLowerCase()} (${x.reason})`).join("; ")}.\n\n` : "") +
      "What the unit holds of these kinds is overwritten, empty slots in the file included. Playback on the unit is stopped. " +
      "A safety backup of the unit is saved to your downloads first; the restore does not start without it.\n\nRestore now?";
    if (!window.confirm(text)) { say("status-restore", "Cancelled. Nothing was written.", ""); return; }
    say("status-restore", "Saving a safety backup of the unit first ..");
    const safety = await captureBackup(request, info);
    safetyName = fileName("DUELL-safety-backup");
    download(safetyName, safety);
    say("status-restore", "Restoring 0%"); bar("bar-restore", 0);
    const r = await runRestore(request, plan, (d, t) => { const pc = d / Math.max(1, t) * 100; say("status-restore", `Restoring ${Math.round(pc)}%`); bar("bar-restore", pc); });
    const names = (ids) => ids.length ? ids.join(", ") : "none";
    if (r.failed) {
      say("status-restore", "The restore stopped. Do not power off; restore again.", "warn");
      say("result-restore", `${r.failed.message} (object ${r.failed.id}).\nWritten before it stopped: objects ${names(r.written)}. Not written: object ${r.failed.id} and everything after it.\n` +
        `The unit holds a mix of its old data and the file's. Fix the cause, then restore the same file again. The safety backup ${safetyName} holds what was on the unit before.`);
      return;
    }
    say("status-restore", "Checking what was written ..");
    const v = await verifyRestore(request, plan);
    if (v.differ.length) {
      say("status-restore", "The restore ran but the check found a difference.", "warn");
      say("result-restore", `Objects that do not match the file: ${names(v.differ)}. Restore again; if it stays, keep ${safetyName} and the file and report it.`);
    } else {
      say("status-restore", "Done.", "good");
      say("result-restore", `Restored ${kinds.join(", ")}. ${v.same.length} stored objects were read back and match the file exactly` +
        `${v.notCompared.length ? "; the settings and the current music were accepted by the unit's own check (it may store them in its current form)" : ""}.` +
        `${plan.excluded.length ? `\nNot restored: ${plan.excluded.map((x) => `${x.label.toLowerCase()} (${x.reason})`).join("; ")}.` : ""}\nSafety backup of the unit as it was: ${safetyName}.`);
    }
  } catch (e) {
    say("status-restore", safetyName ? "The restore stopped." : "Nothing was written.", "warn");
    say("result-restore", e.message + (safetyName ? `\nThe safety backup ${safetyName} was saved.` : ""));
  } finally { bar("bar-restore", null); busy = false; refresh(); }
}
$("restore").addEventListener("click", restore);

renderGroups(); refresh();
if (MOCK) connect();
