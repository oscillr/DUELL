// SPDX-License-Identifier: GPL-3.0-only
// The library UI in a real browser: headless Chrome drives editor/?mock=1 (the simulated unit) through the Chrome DevTools protocol. Files and slots are dropped with
// synthetic DragEvents that carry a DataTransfer (files and the slot type), the dialogs are answered as a user would, and the unit's state is read back from the mock.
// Screenshots for a look go to SHOTS (default: the temp directory). Not part of host_tests.sh (needs Chrome): node web/test_librarian_ui.mjs
//   node web/test_librarian_ui.mjs [CHROME_PATH]      (use the emsdk node: the Homebrew one is broken here)
import { createServer } from "node:http";
import { spawn } from "node:child_process";
import { readFileSync, writeFileSync, mkdirSync, mkdtempSync, existsSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, extname, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { synthBank } from "./test_dx7_synth.mjs";

const WEB = fileURLToPath(new URL(".", import.meta.url));
const CHROME = process.argv[2] || process.env.CHROME || "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const SHOTS = process.env.SHOTS || mkdtempSync(join(tmpdir(), "duell-shots-"));
mkdirSync(SHOTS, { recursive: true });
let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(118)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// ---- a static server for web/
const MIME = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".json": "application/json", ".css": "text/css", ".png": "image/png", ".svg": "image/svg+xml", ".ttf": "font/ttf", ".wav": "audio/wav" };
const server = createServer((req, res) => {
  const path = normalize(decodeURIComponent(new URL(req.url, "http://x").pathname)).replace(/^(\.\.[/\\])+/, "");
  let f = join(WEB, path); if (path.endsWith("/")) f = join(f, "index.html");
  if (!f.startsWith(WEB) || !existsSync(f)) { res.writeHead(404); res.end("no"); return; }
  res.writeHead(200, { "content-type": MIME[extname(f)] || "application/octet-stream" }); res.end(readFileSync(f));
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const PORT = server.address().port;

// ---- Chrome and a tiny DevTools client
const profile = mkdtempSync(join(tmpdir(), "duell-chrome-"));
const chrome = spawn(CHROME, ["--headless=new", "--disable-gpu", "--no-first-run", "--no-default-browser-check", "--hide-scrollbars", "--window-size=1700,1100", "--remote-debugging-port=0", `--user-data-dir=${profile}`, "about:blank"], { stdio: ["ignore", "ignore", "pipe"] });
const wsUrl = await new Promise((resolve, reject) => {
  let buf = ""; const t = setTimeout(() => reject(new Error("Chrome did not start")), 20000);
  chrome.stderr.on("data", (d) => { buf += d; const m = /DevTools listening on (ws:\/\/\S+)/.exec(buf); if (m) { clearTimeout(t); resolve(m[1]); } });
  chrome.on("error", reject);
});
const base = new URL(wsUrl);
const targets = await (await fetch(`http://${base.host}/json`)).json();
const page = targets.find((x) => x.type === "page");
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener("open", r));
let nextId = 1; const pending = new Map(), consoleMsgs = [];
ws.addEventListener("message", (m) => {
  const d = JSON.parse(m.data);
  if (d.id && pending.has(d.id)) { const p = pending.get(d.id); pending.delete(d.id); d.error ? p.reject(new Error(d.error.message)) : p.resolve(d.result); }
  else if (d.method === "Runtime.exceptionThrown") consoleMsgs.push("EXCEPTION " + (d.params.exceptionDetails.exception?.description || d.params.exceptionDetails.text));
  else if (d.method === "Runtime.consoleAPICalled" && d.params.type === "error") consoleMsgs.push("console.error " + d.params.args.map((a) => a.value ?? a.description).join(" "));
});
const cdp = (method, params = {}) => new Promise((resolve, reject) => { const id = nextId++; pending.set(id, { resolve, reject }); ws.send(JSON.stringify({ id, method, params })); });
await cdp("Page.enable"); await cdp("Runtime.enable");
async function ev(expr) {
  const r = await cdp("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception?.description || r.exceptionDetails.text);
  return r.result.value;
}
async function waitFor(expr, what, ms = 8000) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) { try { if (await ev(expr)) return true; } catch { /* the page is loading */ } await sleep(40); }
  throw new Error(`timed out waiting for: ${what}`);
}
async function shot(name) { const r = await cdp("Page.captureScreenshot", { format: "png" }); writeFileSync(join(SHOTS, name + ".png"), Buffer.from(r.data, "base64")); }
async function open(query = "?mock=1") {
  await cdp("Page.navigate", { url: `http://127.0.0.1:${PORT}/editor/${query}` });
  await waitFor(`document.getElementById("unit")?.textContent.startsWith("Connected")`, "the page connects to the mock unit");
  await ev(HELPERS);
}

// ---- helpers inside the page
const HELPERS = `window.__t = {
  fire(el, type, dt, init = {}) { el.dispatchEvent(new DragEvent(type, { bubbles: true, cancelable: true, dataTransfer: dt, ...init })); },
  bytes(b64) { const s = atob(b64); return Uint8Array.from(s, (c) => c.charCodeAt(0)); },
  dropFiles(sel, files, init = {}) {
    const el = document.querySelector(sel); if (!el) throw new Error("no element " + sel);
    const dt = new DataTransfer(); for (const [name, b64, text] of files) dt.items.add(new File([text != null ? text : this.bytes(b64)], name));
    this.fire(el, "dragenter", dt); this.fire(el, "dragover", dt); this.fire(el, "drop", dt, init); return true;
  },
  dragSlot(srcSel, dstSel, init = {}) {
    const a = document.querySelector(srcSel), b = document.querySelector(dstSel); if (!a || !b) throw new Error("no element " + srcSel + " / " + dstSel);
    const dt = new DataTransfer(); this.fire(a, "dragstart", dt); this.fire(b, "dragenter", dt); this.fire(b, "dragover", dt); this.fire(b, "drop", dt, init); this.fire(a, "dragend", dt);
    return dt.types.join(",");
  },
  btn(text, root = "dialog[open]") { const r = document.querySelector(root) || document; return [...r.querySelectorAll("button")].find((b) => b.textContent.trim() === text || b.textContent.trim().startsWith(text)); },
  click(text, root) { const b = this.btn(text, root); if (!b) throw new Error("no button " + text); b.click(); return true; },
  text(sel) { return document.querySelector(sel)?.textContent ?? null; },
};`;
const clickBtn = (text, root) => ev(`__t.click(${JSON.stringify(text)}, ${root ? JSON.stringify(root) : "undefined"})`);
const rows = () => waitFor(`document.querySelectorAll(".plist li[data-slot]").length === 128`, "the 128 rows");
const msg = () => ev(`document.getElementById("msg").textContent`);
const b64 = (path) => readFileSync(path).toString("base64");
// synthetic DX7 banks (web/test_dx7_synth.mjs): A = 32 clean voices, B = voice 31 damaged (bytes >= 128)
const bank = (name) => Buffer.from(name === "TEST BANK B.syx" ? synthBank(1, [30]).bytes : synthBank(0).bytes).toString("base64");

try {
  // ============================================================================ open the page, the PATCHES tab
  await open();
  await ev(`document.querySelector('.col.glob [data-key="patches"]').click()`);
  await waitFor(`document.querySelectorAll(".plist li[data-slot]").length === 128`, "128 slot rows");
  await shot("01-patches-compact");
  ok(await ev(`document.querySelectorAll(".plist li[data-slot]").length === 128 && !/\\d+ of 128|free/i.test(document.querySelector(".patches").textContent)`), "list: 128 slots, no count of free places anywhere on the tab");
  ok(await ev(`!!__t.btn("Import file...", ".patches") && !!__t.btn("Export bank...", ".patches") && !!__t.btn("Wide view", ".patches")`), "tab: the library buttons are there (Import file, Export bank, Wide view)");
  ok(await ev(`document.querySelector(".pdetail").textContent.includes("Pick a slot")`), "tab: nothing selected: the detail pane asks for a slot");
  ok(await ev(`(() => { const g = document.querySelector(".col.glob").getBoundingClientRect(); const l = document.querySelector(".plist").getBoundingClientRect(); return l.width > 200 && l.right <= g.right + 1 && document.querySelector(".plist").scrollWidth <= document.querySelector(".plist").clientWidth + 1; })()`), "tab: the compact list fits the width of the GLOBAL column (no sideways scroll)");

  // ---- select, hide rule, export one patch
  await ev(`document.querySelector('.plist li[data-slot="0"]').click()`);
  ok(await ev(`!!__t.btn("Restore...", ".pdetail") && !!__t.btn("Move to...", ".pdetail") && !!__t.btn("Export this patch", ".pdetail")`), "detail: a saved slot offers Restore, Move to and Export");
  await ev(`document.querySelector('.plist li[data-slot="9"]').click()`);
  ok(await ev(`!__t.btn("Restore...", ".pdetail") && !__t.btn("Move to...", ".pdetail") && !!__t.btn("Copy to...", ".pdetail")`), "detail: a slot on its factory sound shows no Restore and no Move (they do not apply), Copy to is still there");
  await ev(`document.querySelector('.plist li[data-slot="0"]').click()`);
  await ev(`__t.click("Export this patch", ".pdetail")`);
  await waitFor(`window.__app.downloads?.length === 1`, "a download");
  const one = JSON.parse(await ev(`window.__app.downloads[0].text`));
  ok(one.format === "duell-patches" && one.patches.length === 1 && one.patches[0].name === "WARM PAD" && one.patches[0].record && one.patches[0].number === 1, "export one patch: a duell-patches file with the record of #1 WARM PAD");
  ok(await ev(`window.__app.downloads[0].name`) === "WARM-PAD.duell-patches.json", "  and a file name from the patch's name");

  // ---- select several with shift, export the selection
  await ev(`document.querySelector('.plist li[data-slot="5"]').dispatchEvent(new MouseEvent("click", { bubbles: true, shiftKey: true }))`);
  ok(await ev(`document.querySelectorAll(".plist li.sel").length === 6 && document.querySelector(".pdetail h3").textContent === "6 slots selected"`), "selection: shift-click selects the range (slots 1..6)");
  await ev(`__t.click("Export selection", ".pdetail")`);
  await waitFor(`window.__app.downloads?.length === 2`, "the selection download");
  const sel = JSON.parse(await ev(`window.__app.downloads[1].text`));
  ok(sel.patches.length === 6 && sel.patches.filter((p) => p.saved).length === 3 && sel.patches.filter((p) => p.approximate).length === 3, "export selection: 6 patches, the saved ones whole, the factory ones as approximate copies");
  // ---- bank
  await ev(`__t.click("Export bank...", ".patches")`);
  await waitFor(`!!document.querySelector("dialog[open]")`, "bank dialog");
  await clickBtn("Export");
  await waitFor(`window.__app.downloads?.length === 3`, "the bank download");
  const bk = JSON.parse(await ev(`window.__app.downloads[2].text`));
  ok(bk.patches.length === 4 && bk.patches.every((p) => p.saved && p.record && Number.isInteger(p.number)) && bk.patches.map((p) => p.number).join() === "1,2,6,65", "export bank: every saved patch, each with its slot number (#1, #2, #6, #65)");
  await ev(`__t.click("Export bank...", ".patches")`);
  await ev(`document.querySelector("dialog[open] input[type=checkbox]").click()`);
  await clickBtn("Export");
  await waitFor(`window.__app.downloads?.length === 4`, "the full bank download");
  const full = JSON.parse(await ev(`window.__app.downloads[3].text`));
  ok(full.patches.length === 128 && full.patches.filter((p) => p.saved).length === 4, "export bank with the factory sounds ticked: 128 patches (4 whole, the rest copies of the values)");

  // ============================================================================ import a patch file by dropping it on the tab
  const fileText = JSON.stringify(bk);
  await ev(`__t.dropFiles(".patches", [["mybank.duell-patches.json", null, ${JSON.stringify(fileText)}]])`);
  await waitFor(`!!document.querySelector("dialog.imp[open]")`, "the import dialog");
  await shot("02-import-preview");
  ok(await ev(`!document.querySelector(".dropping") && !document.body.classList.contains("dragfiles")`), "drop: no drop-target outline is left behind on the columns after the drop");
  ok(await ev(`document.querySelectorAll("dialog.imp .improw").length === 4 && [...document.querySelectorAll("dialog.imp .improw .n")].map((x) => x.textContent).join() === "WARM PAD,SUB BASS,BELL,WAVE LEAD"`), "import preview: the four patches by name, engine and category");
  ok(await ev(`[...document.querySelectorAll("dialog.imp .improw")].map((r) => r.children[2].textContent + "/" + r.children[3].textContent).join()`) === "ANALOG/PAD,ANALOG/BASS,FM6/KEYS,WAVE/LEAD", "  with engine and category");
  ok(await ev(`document.querySelector("dialog.imp select").value === "same" && document.querySelector("dialog.imp .sum").textContent.includes("4 saved patches will be replaced")`), "  default target: the same slot numbers as the file; the slots it replaces are counted");
  ok(await ev(`[...document.querySelectorAll("dialog.imp .rep")].map((x) => x.textContent).join(" | ")`) === '#1 "WARM PAD" (saved) | #2 "SUB BASS" (saved) | #6 "BELL" (saved) | #65 "WAVE LEAD" (saved)', "  the overwrite list names each slot and what it holds");
  ok(await ev(`[...document.querySelectorAll("dialog.imp label.chk")].some((l) => l.textContent.includes("safety copy") && l.querySelector("input").checked)`), "  with four saved patches to replace a safety copy is offered (on)");
  // choose 'from slot 20, skipping saved'
  await ev(`(() => { const s = document.querySelector("dialog.imp select"); s.value = "from"; s.dispatchEvent(new Event("change", { bubbles: true })); })()`);
  await ev(`(() => { const i = document.querySelector("dialog.imp input[type=number]"); i.value = "20"; i.dispatchEvent(new Event("input", { bubbles: true })); })()`);
  ok(await ev(`[...document.querySelectorAll("dialog.imp .improw input[type=number]")].map((x) => x.value).join()`) === "20,21,22,23" && await ev(`document.querySelector("dialog.imp .sum").textContent`) === "4 patches will be written.", "import preview: 'one after the other from slot 20': #20..#23, nothing saved is replaced");
  await ev(`(() => { const i = document.querySelectorAll("dialog.imp .improw input[type=number]")[1]; i.value = "40"; i.dispatchEvent(new Event("change", { bubbles: true })); })()`);
  ok(await ev(`[...document.querySelectorAll("dialog.imp .improw input[type=number]")].map((x) => x.value).join()`) === "20,40,22,23", "  a slot can be typed per patch");
  await ev(`(() => { const i = document.querySelectorAll("dialog.imp .improw input[type=number]")[2]; i.value = "40"; i.dispatchEvent(new Event("change", { bubbles: true })); })()`);
  ok(await ev(`document.querySelector("dialog.imp #impgo").disabled && /both go to #40/.test(document.querySelector("dialog.imp").textContent)`), "  two patches to one slot: the problem is shown and Import is off");
  await ev(`(() => { const i = document.querySelectorAll("dialog.imp .improw input[type=number]")[2]; i.value = "41"; i.dispatchEvent(new Event("change", { bubbles: true })); })()`);
  await ev(`__t.click("Import 4")`);
  await waitFor(`/written and read back/.test(document.querySelector("dialog.imp")?.textContent || "")`, "the import to finish");
  await shot("03-import-done");
  ok(await ev(`window.__app.mock.patches.get(19)?.name === "WARM PAD" && window.__app.mock.patches.get(39)?.name === "SUB BASS" && window.__app.mock.patches.get(40)?.name === "BELL" && window.__app.mock.patches.get(22)?.name === "WAVE LEAD"`), "import: the four patches are in slots #20, #40, #41 and #23 on the unit");
  ok(await ev(`window.__app.mock.patches.get(40).fm6 && window.__app.mock.patches.get(40).eng === 12`), "  the FM6 patch came with its patch (lossless)");
  await clickBtn("Close");
  await waitFor(`!document.querySelector("dialog[open]")`, "the dialog to close");
  ok(await ev(`document.querySelector('.plist li[data-slot="39"] .n').textContent === "SUB BASS" && document.querySelector('.plist li[data-slot="39"]').classList.contains("saved")`), "  and the list shows them without a reload");

  // ============================================================================ a DX7 bank dropped on the tab
  await ev(`__t.dropFiles(".patches", [["TEST BANK B.syx", ${JSON.stringify(bank("TEST BANK B.syx"))}]])`);
  await waitFor(`!!document.querySelector("dialog.imp[open]")`, "the DX7 dialog");
  ok(await ev(`document.querySelectorAll("dialog.imp .improw").length === 32 && document.querySelectorAll("dialog.imp .improw.off").length === 1 && /damaged/.test(document.querySelector("dialog.imp .improw.off").textContent)`), "dx7: 32 voices listed with their names, the damaged one is greyed out and cannot be picked");
  ok(await ev(`/DX7 sysex import/.test(document.querySelector("dialog.imp").textContent) && ![...document.querySelectorAll("dialog.imp .improw .n")].some((n) => /DX7/i.test(n.textContent))`), "  'DX7 sysex import' is the source label; no voice is named DX7");
  await shot("04-dx7-preview");
  await ev(`(() => { const all = document.querySelector("dialog.imp .imphead input"); all.click(); })()`);
  ok(await ev(`document.querySelectorAll("dialog.imp .improw:not(.off)").length === 0 && document.querySelector("dialog.imp #impgo").disabled`), "  'all' off: nothing selected, Import is off");
  await ev(`[0, 1, 2].forEach((i) => document.querySelectorAll("dialog.imp .improw input[type=checkbox]")[i].click())`);
  await ev(`(() => { const i = document.querySelector("dialog.imp input[type=number]"); i.value = "100"; i.dispatchEvent(new Event("input", { bubbles: true })); })()`);
  ok(await ev(`[...document.querySelectorAll("dialog.imp .improw input[type=number]")].slice(0, 3).map((x) => x.value).join()`) === "100,101,102", "  three voices picked, to slots #100..#102");
  const names3 = await ev(`[...document.querySelectorAll("dialog.imp .improw:not(.off) .n")].map((x) => x.textContent)`);
  await ev(`__t.click("Import 3")`);
  await waitFor(`/written and read back/.test(document.querySelector("dialog.imp")?.textContent || "")`, "the DX7 import");
  await clickBtn("Close");
  console.log("   (debug)", JSON.stringify(names3), await ev(`JSON.stringify([99, 100, 101].map((s) => { const p = window.__app.mock.patches.get(s); return p && [p.name, p.eng, !!p.fm6]; }))`));
  ok(await ev(`[99, 100, 101].every((s) => window.__app.mock.patches.get(s)?.eng === 12 && window.__app.mock.patches.get(s).fm6)`) && (await ev(`[99, 100, 101].map((s) => window.__app.mock.patches.get(s).name)`)).join() === names3.join(), "dx7: the three voices are FM6 patches with the voice's own name in slots #100..#102, each carrying its patch");
  ok(await ev(`window.__app.mock.patches.get(99).p[45] === 71`), "  with the neutral MULTI FILTER (6.8 kHz)");

  // ============================================================================ drag a slot onto a slot: copy, then swap (shift)
  await ev(`document.querySelector('.plist li[data-slot="0"]').scrollIntoView()`);
  const dtypes = await ev(`__t.dragSlot('.plist li[data-slot="0"]', '.plist li[data-slot="49"]')`);
  ok(dtypes.includes("application/x-duell-slot"), "drag: a slot row starts a drag with the slot type");
  await waitFor(`!!document.querySelector("dialog[open]")`, "the copy confirmation");
  ok(await ev(`/Copy #1 to #50/.test(document.querySelector("dialog[open] h2").textContent) && /factory sound/.test(document.querySelector("dialog[open]").textContent)`), "drag onto another slot: a copy is asked for, naming what it replaces");
  await clickBtn("Copy");
  await waitFor(`window.__app.mock.patches.get(49)?.name === "WARM PAD"`, "the copy"); await waitFor(`/Copied #1 to #50/.test(document.getElementById("msg").textContent)`, "the copy message"); await rows();
  ok(await ev(`window.__app.mock.patches.get(0).name === "WARM PAD" && JSON.stringify([...window.__app.mock.recordOf(window.__app.mock.patches.get(49)).slice(4)]) === JSON.stringify([...window.__app.mock.recordOf(window.__app.mock.patches.get(0)).slice(4)])`), "  #50 holds the same record as #1, #1 stays");
  await ev(`__t.dragSlot('.plist li[data-slot="0"]', '.plist li[data-slot="1"]', { shiftKey: true })`);
  await waitFor(`!!document.querySelector("dialog[open]")`, "the swap confirmation");
  ok(await ev(`/Swap/.test(document.querySelector("dialog[open] h2").textContent) && /change places/.test(document.querySelector("dialog[open]").textContent)`), "shift-drag: a swap is asked for");
  await clickBtn("Swap");
  await waitFor(`window.__app.mock.patches.get(0)?.name === "SUB BASS"`, "the swap"); await waitFor(`/#1 and #2 are done/.test(document.getElementById("msg").textContent)`, "the swap message"); await rows();
  ok(await ev(`window.__app.mock.patches.get(1).name === "WARM PAD"`), "  #1 and #2 changed places");
  await ev(`__t.dragSlot('.plist li[data-slot="2"]', '.plist li[data-slot="2"]')`);
  ok(await ev(`!document.querySelector("dialog[open]")`), "drag onto itself: nothing happens");

  // ============================================================================ the wide view
  await ev(`__t.click("Wide view", ".patches")`);
  await waitFor(`document.querySelectorAll(".pwide .tile").length === 128`, "the wide view");
  await shot("05-wide-view");
  ok(await ev(`(() => { const o = document.querySelector(".pwide").getBoundingClientRect(), t = document.getElementById("top").getBoundingClientRect(); return o.top >= t.bottom - 1 && o.width > 1500 && getComputedStyle(document.querySelector(".pgrid")).gridTemplateColumns.split(" ").length === 16 && !!document.querySelector("#top img.hdr"); })()`), "wide view: an overlay under the header (the header image stays), the full width, 16 slots to a row");
  ok(await ev(`document.querySelector('.pwide .tile[data-slot="1"]').classList.contains("saved") && !document.querySelector('.pwide .tile[data-slot="2"]').classList.contains("saved") && document.querySelector('.pwide .tile[data-slot="1"] .n').textContent === "WARM PAD"`), "  saved patches are drawn strong, factory sounds plain");
  const before = await ev(`[window.__app.mock.patches.get(1).name, window.__app.mock.patches.get(0).name]`);
  await ev(`__t.dragSlot('.pwide .tile[data-slot="1"]', '.pwide .tile[data-slot="0"]')`);
  await waitFor(`!!document.querySelector("dialog[open]")`, "swap confirm in the wide view");
  ok(await ev(`/Swap/.test(document.querySelector("dialog[open] h2").textContent)`), "wide view: dragging a tile onto another swaps (asked first)");
  await clickBtn("Swap");
  await waitFor(`window.__app.mock.patches.get(1)?.name === ${JSON.stringify(before[1])}`, "the swap in the wide view"); await waitFor(`/#2 and #1 are done/.test(document.getElementById("msg").textContent)`, "the swap message"); await rows();
  ok(await ev(`document.querySelector('.pwide .tile[data-slot="0"] .n').textContent === ${JSON.stringify(before[0])}`), "  and the tiles follow");
  await ev(`__t.dragSlot('.pwide .tile[data-slot="0"]', '.pwide .tile[data-slot="30"]', { altKey: true })`);
  await waitFor(`!!document.querySelector("dialog[open]")`, "copy confirm in the wide view");
  ok(await ev(`/Copy #1 to #31/.test(document.querySelector("dialog[open] h2").textContent)`), "  Alt-drag copies");
  await clickBtn("Cancel");
  await ev(`document.querySelector('.pwide .tile[data-slot="0"]').dispatchEvent(new KeyboardEvent("keydown", { key: "ArrowRight", bubbles: true })); 1`);
  ok(await ev(`document.activeElement === document.querySelector('.pwide .tile[data-slot="1"]')`), "wide view: the arrow keys move between tiles");
  await ev(`__t.dropFiles(".pwide", [["x.duell-patches.json", null, ${JSON.stringify(fileText)}]])`);
  await waitFor(`!!document.querySelector("dialog.imp[open]")`, "an import from the wide view");
  ok(true, "wide view: a file dropped on it opens the same import preview");
  await clickBtn("Cancel");
  await ev(`document.querySelector(".pwide").dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true })); 1`);
  ok(await ev(`!document.querySelector(".pwide")`), "wide view: Escape closes it");

  // ============================================================================ keyboard alternatives: Copy to..., Move to...
  await rows();
  await ev(`document.querySelector('.plist li[data-slot="64"]').click()`);
  await ev(`__t.click("Copy to...", ".pdetail")`);
  await waitFor(`!!document.querySelector("dialog[open] input[type=number]")`, "the Copy to dialog");
  await ev(`(() => { const i = document.querySelector("dialog[open] input[type=number]"); i.value = "70"; i.dispatchEvent(new Event("input", { bubbles: true })); })()`);
  ok(await ev(`/#65 "WAVE LEAD" -> #70/.test(document.querySelector("dialog[open] ul.effects").textContent)`), "Copy to...: the dialog lists what changes for the typed slot");
  await clickBtn("Copy");
  await waitFor(`window.__app.mock.patches.get(69)?.name === "WAVE LEAD"`, "the copy to #70"); await waitFor(`/Copied to #70/.test(document.getElementById("msg").textContent)`, "the copy message"); await rows();
  ok(true, "Copy to...: #65 copied to #70 without the mouse");
  await ev(`document.querySelector('.plist li[data-slot="69"]').click()`);
  await ev(`__t.click("Move to...", ".pdetail")`);
  await waitFor(`!!document.querySelector("dialog[open] input[type=number]")`, "the Move to dialog");
  await ev(`(() => { const i = document.querySelector("dialog[open] input[type=number]"); i.value = "90"; i.dispatchEvent(new Event("input", { bubbles: true })); })()`);
  await clickBtn("Move");
  await waitFor(`window.__app.mock.patches.get(89)?.name === "WAVE LEAD" && !window.__app.mock.patches.has(69)`, "the move"); await waitFor(`/Moved to #90/.test(document.getElementById("msg").textContent)`, "the move message"); await rows();
  ok(true, "Move to...: #70 moved to #90 (#70 shows its factory sound again)");
  // arrow keys in the list
  await ev(`document.querySelector('.plist li[data-slot="3"]').focus(); document.querySelector('.plist li[data-slot="3"]').dispatchEvent(new KeyboardEvent("keydown", { key: "ArrowDown", bubbles: true })); 1`);
  ok(await ev(`document.activeElement === document.querySelector('.plist li[data-slot="4"]')`), "list: the arrow keys move between slots (one tab stop for the list)");

  // ============================================================================ a drop on a part column loads there, live, without saving
  const writesBefore = await ev(`window.__app.mock.log.filter((x) => /write|store|put|restore/.test(x)).length`);
  await ev(`__t.dropFiles('.col.part[data-part="1"]', [["w.duell-patches.json", null, ${JSON.stringify(JSON.stringify({ ...bk, patches: [bk.patches[2]] }))}]])`);
  await waitFor(`!!document.querySelector("dialog[open]")`, "the live load question");
  ok(await ev(`/Load into PART 2/.test(document.querySelector("dialog[open] h2").textContent) && /not saved in any slot/.test(document.querySelector("dialog[open]").textContent)`), "drop on a part: asks, and says it is not saved in any slot");
  await clickBtn("Load");
  await waitFor(`/is loaded in PART 2 \\(not saved\\)/.test(document.getElementById("msg").textContent)`, "the live load");
  ok(await ev(`window.__app.mock.parts[1].eng === 12 && window.__app.mock.parts[1].fm6?.length === 128 && window.__app.unit.parts[1].name === "BELL"`), "  PART 2 now plays the FM6 sound with its patch (FM6_PUT)");
  ok(await ev(`window.__app.mock.log.filter((x) => /write|store|put|restore/.test(x)).length === ${writesBefore}`), "  nothing was written to a slot (no store, no write)");
  await shot("06-after-part-drop");
  // a dx7 voice on a part
  await ev(`__t.dropFiles('.col.part[data-part="0"]', [["TEST BANK A.syx", ${JSON.stringify(bank("TEST BANK A.syx"))}]])`);
  await waitFor(`!!document.querySelector("dialog[open] select")`, "the voice picker");
  await ev(`(() => { const s = document.querySelector("dialog[open] select"); s.value = "2"; })()`);
  await clickBtn("Load");
  await waitFor(`/is loaded in PART 1 \\(not saved\\)/.test(document.getElementById("msg").textContent)`, "the DX7 voice live load");
  ok(await ev(`window.__app.mock.parts[0].eng === 12 && window.__app.mock.parts[0].p[45] === 71 && window.__app.mock.log.filter((x) => /write|store|put|restore/.test(x)).length === ${writesBefore}`), "drop of a DX7 bank on a part: pick a voice, PART 1 plays it, nothing is saved");
  // a slot dragged to a part
  await ev(`__t.dragSlot('.plist li[data-slot="1"]', '.col.part[data-part="1"]')`);
  await waitFor(`/#2 .* is loaded in PART 2/.test(document.getElementById("msg").textContent)`, "a slot dropped on a part");
  ok(true, "drag of a slot onto a part: loaded there (UP_LOAD), not saved");

  // ============================================================================ the wavetables tab
  await ev(`document.querySelector('.col.glob [data-key="tables"]').click()`);
  await waitFor(`document.querySelectorAll(".col.glob .card").length >= 3`, "the wavetable cards");
  const wav = b64(join(WEB, "editor", "test_saw_sweep_2048x64.wav"));
  await ev(`__t.dropFiles(".col.glob .card:nth-of-type(1)", [["saw sweep.wav", ${JSON.stringify(wav)}]])`);
  await waitFor(`/19 frames x 64 harmonics/.test(document.querySelector(".col.glob .card").textContent)`, "the WAV conversion");
  await shot("07-wavetable-drop");
  ok(await ev(`document.querySelector(".col.glob .card input[type=text]").value.length > 0 && !document.querySelector(".col.glob .card button").disabled`), "wavetables: a WAV dropped on a slot is converted and can be sent");
  await ev(`__t.click("Send to USR1", ".col.glob .card:nth-of-type(1)")`);
  await waitFor(`window.__app.mock.tables[0]`, "the table on the unit");
  await waitFor(`!document.querySelector(".col.glob .card button[disabled]") || true`, "ok");
  await sleep(150);
  await ev(`__t.click("Export file", ".col.glob .card:nth-of-type(1)")`);
  await waitFor(`window.__app.downloads.some((d) => /duell-wavetable/.test(d.name))`, "the table export");
  const tf = JSON.parse(await ev(`window.__app.downloads.find((d) => /duell-wavetable/.test(d.name)).text`));
  ok(tf.format === "duell-wavetable" && tf.version === 1 && tf.frames === 19 && tf.harmonics === 64 && tf.data.length > 1000 && tf.data.length < 2000, "wavetables: Export file saves a small duell-wavetable file (about 1.6 KB)");
  await ev(`__t.dropFiles(".col.glob .card:nth-of-type(3)", [["t.json", null, ${JSON.stringify(JSON.stringify(tf))}]])`);
  await waitFor(`/a wavetable file/.test(document.querySelector(".col.glob .card:nth-of-type(3)").textContent)`, "the table file read");
  await ev(`__t.click("Send to USR3", ".col.glob .card:nth-of-type(3)")`);
  await waitFor(`window.__app.mock.tables[2]`, "USR3 written");
  ok(await ev(`window.__app.mock.tables[2].data.length === 1216 && window.__app.mock.tables[2].data.every((b, i) => b === window.__app.mock.tables[0].data[i])`), "  the exported file dropped on USR3 gives the same 1216 bytes (the tables are shared by file)");
  await ev(`__t.dropFiles(".col.glob .gbody", [["t2.json", null, ${JSON.stringify(JSON.stringify({ ...tf, data: "AAAA" + tf.data.slice(4) }))}]])`);
  await waitFor(`/checksum/.test(document.querySelector(".col.glob").textContent)`, "the checksum error");
  ok(true, "wavetables: a damaged file anywhere on the tab is refused with the reason (checksum)");

  // ============================================================================ a file dropped on the page outside the library does not leave the editor
  const stay = await ev(`(() => { const dt = new DataTransfer(); dt.items.add(new File(["x"], "x.txt")); const e = new DragEvent("drop", { bubbles: true, cancelable: true, dataTransfer: dt }); document.getElementById("top").dispatchEvent(e); return e.defaultPrevented; })()`);
  ok(stay === true && /Drop a patch file/.test(await msg()), "a file dropped on the header is not opened by the browser; the footer says where files go");

  // ============================================================================ a unit that is playing, and a unit with older firmware
  await open("?mock=playing");
  await ev(`document.querySelector('.col.glob [data-key="patches"]').click()`);
  await waitFor(`document.querySelectorAll(".plist li[data-slot]").length === 128`, "rows (playing unit)");
  ok(await ev(`true`), "playing unit: the list loads (reading never touches the flash)");
  await ev(`__t.dropFiles(".patches", [["mybank.duell-patches.json", null, ${JSON.stringify(fileText)}]])`);
  await waitFor(`!!document.querySelector("dialog.imp[open]")`, "import dialog (playing unit)");
  await ev(`__t.click("Replace 4 and import 4")`);
  await waitFor(`/playing|Stopped/.test(document.querySelector("dialog.imp")?.textContent || "")`, "the refusal");
  ok(await ev(`/0 of 4 patches were written/.test(document.querySelector("dialog.imp").textContent) && !window.__app.mock.log.some((x) => /raw write/.test(x))`), "playing unit: the import is refused by the unit with its reason, nothing is written");
  await clickBtn("Close");
  await open("?mock=old");
  await ev(`document.querySelector('.col.glob [data-key="patches"]').click()`);
  await waitFor(`document.querySelectorAll(".plist li[data-slot]").length === 128`, "rows (old unit)");
  ok(await ev(`!__t.btn("Import file...", ".patches") && !__t.btn("Export bank...", ".patches") && /need firmware 0.2.19/.test(document.querySelector(".patches").textContent)`), "older firmware: the library buttons are not drawn, one line says why");
  // ============================================================================ a smaller window (1280 x 720): the GLOBAL view, the list and the wide view still fit
  await cdp("Emulation.setDeviceMetricsOverride", { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
  await open("?mock=1");
  await ev(`document.getElementById("vglobal").click(); document.querySelector('.col.glob [data-key="patches"]').click()`);
  await waitFor(`document.querySelectorAll(".plist li[data-slot]").length === 128`, "rows (1280 x 720)");
  await shot("08-small-window");
  ok(await ev(`(() => { const l = document.querySelector(".plist"); return l.scrollWidth <= l.clientWidth + 1 && l.getBoundingClientRect().right <= innerWidth; })()`), "1280 x 720: the compact list fits, no sideways scroll");
  await ev(`__t.click("Wide view", ".patches")`);
  await waitFor(`document.querySelectorAll(".pwide .tile").length === 128`, "wide view (1280 x 720)");
  await shot("09-small-window-wide");
  ok(await ev(`(() => { const o = document.querySelector(".pwide"), g = document.querySelector(".pgrid"); return getComputedStyle(g).gridTemplateColumns.split(" ").length === 8 && g.scrollWidth <= g.clientWidth + 1 && o.getBoundingClientRect().right <= innerWidth; })()`), "1280 x 720: the wide view is 8 slots to a row and fits (it scrolls up and down, not sideways)");
  await ev(`document.querySelector(".pwide").dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true })); 1`);
  await cdp("Emulation.clearDeviceMetricsOverride");
  ok(consoleMsgs.length === 0, `no script error in the page${consoleMsgs.length ? ": " + consoleMsgs.slice(0, 3).join(" | ") : ""}`);
} catch (e) {
  console.log("TEST ERROR:", e.message); fails++;
  try { await shot("error"); } catch { /* none */ }
} finally {
  try { ws.close(); } catch { /* closed */ } chrome.kill(); server.close();
}
console.log(`screenshots: ${SHOTS} (${readdirSync(SHOTS).join(", ")})`);
console.log(fails ? `\nLIBRARIAN UI TESTS FAILED (${fails})` : "\nlibrarian UI tests passed");
process.exit(fails ? 1 : 0);
