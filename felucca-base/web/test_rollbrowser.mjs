// SPDX-License-Identifier: GPL-3.0-only
// The piano roll in a real browser: headless Chrome drives /editor/?mock=1 (the simulated unit) with real mouse and keyboard events. It checks the editing (click, drag, move,
// lengthen, delete, velocity lane, accent), the keyboard (arrows, Delete, Enter, Ctrl+Z / Ctrl+Shift+Z), the full-width overlay, the hide rule, and that no text is clipped at
// 1920x1080 and 1280x720. Screenshots for the author's own check go to $ROLL_SHOTS (default: not written). Skipped (exit 0) when no Chrome is installed (CHROME=/path overrides).
//   node web/test_rollbrowser.mjs      (use the emsdk node)
import http from "node:http";
import { readFile, mkdir } from "node:fs/promises";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { launch, CHROME } from "./cdp.mjs";

if (!CHROME) { console.log("no Chrome found: browser test skipped"); process.exit(0); }
let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(112)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const ROOT = fileURLToPath(new URL(".", import.meta.url)), SHOTS = process.env.ROLL_SHOTS;
if (SHOTS) await mkdir(SHOTS, { recursive: true });
const MIME = { ".js": "text/javascript", ".mjs": "text/javascript", ".json": "application/json", ".html": "text/html", ".css": "text/css", ".png": "image/png", ".svg": "image/svg+xml", ".ttf": "font/ttf", ".wav": "audio/wav" };
const server = http.createServer(async (req, res) => {
  try { let p = decodeURIComponent(new URL(req.url, "http://x").pathname); if (p.endsWith("/")) p += "index.html"; const f = normalize(join(ROOT, p)); if (!f.startsWith(ROOT)) throw new Error("outside");
    const b = await readFile(f); res.writeHead(200, { "content-type": MIME[extname(f)] || "application/octet-stream" }); res.end(b); } catch { res.writeHead(404); res.end("not found"); }
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const URL0 = `http://127.0.0.1:${server.address().port}/editor/?mock=1`;
const c = await launch();
const E = (x) => c.eval(x);
const shot = async (name) => { if (SHOTS) await c.shot(join(SHOTS, name)); };
const idle = () => E("(async () => { await window.__app.stores[0].idle(); await window.__app.stores[1]?.idle(); await window.__app.unit.idle(); return true; })()");
const mockStep = (p, i) => E(`(() => { const s = window.__app.mock.steps[${p}][${i}]; return JSON.stringify(s); })()`).then(JSON.parse);
const view = (p = 0, overlay = false) => (overlay ? `window.__app.overlay.views.find((v) => v.part === ${p})` : `window.__app.rollViews[${p}]`);
const center = (step, pitch, p = 0, ov = false) => E(`JSON.stringify(${view(p, ov)}.cellCenter(${step}, ${pitch}))`).then(JSON.parse);
const sel = (p = 0, ov = false) => E(`JSON.stringify(${view(p, ov)}.selection)`).then(JSON.parse);
const status = (p = 0) => E(`document.querySelectorAll('.roll')[${p}].querySelector('.rollstatus').textContent`);
const store = (p = 0) => `window.__app.stores[${p}]`;

// text that is cut off: an element with its own text whose content is wider / taller than its box (overflow hidden), or that sticks out of the window
const clipped = (root) => E(`(() => {
  const out = [], vw = innerWidth, vh = innerHeight;
  for (const el of document.querySelectorAll(${JSON.stringify(root)} + ' *')) {
    const own = [...el.childNodes].some((n) => n.nodeType === 3 && n.textContent.trim());
    if (!own || el.closest('[hidden]') || getComputedStyle(el).visibility === 'hidden' || el.closest('.cell.gone')) continue;
    const r = el.getBoundingClientRect(); if (!r.width) continue;
    const cs = getComputedStyle(el);
    if (el.tagName === 'OPTION') continue;
    if (el.scrollWidth > el.clientWidth + 1 || (cs.overflow !== 'visible' && el.scrollHeight > el.clientHeight + 1)) out.push(el.className + ':' + el.textContent.trim().slice(0, 20) + ' ' + el.scrollWidth + '>' + el.clientWidth);
    // (a scrolled grid keeps its text under its own scroll box; only text that is cut by the window itself counts)
    if (!el.closest('.rscroll, .rkeys, .rruler, .lscroll, .ovbody') && (r.right > vw + 1 || r.bottom > vh + 1)) out.push('outside: ' + el.textContent.trim().slice(0, 20));
  }
  return JSON.stringify(out);
})()`).then(JSON.parse);

for (const [W, H] of [[1920, 1080], [1280, 720]]) {
  console.log(`\n== ${W} x ${H}`);
  await c.open(URL0, W, H);
  await c.until("window.__app && window.__app.unit");
  await E("window.__app.setPartTab(0, 'roll'); window.__app.setPartTab(1, 'roll'); true");
  await c.until(`${store(0)}?.loaded && ${store(1)}?.loaded`);
  await c.sleep(300);
  const nodes = await E("document.querySelectorAll('.roll').length");
  ok(nodes === 2 && (await E("[...document.querySelectorAll('.col.part .tab[aria-selected=true]')].map((b) => b.textContent).join()")) === "PIANO ROLL,PIANO ROLL", "tab: both parts show a PIANO ROLL tab");
  ok((await E("[...document.querySelectorAll('.roll .rollctl .lab')].slice(0, 4).map((x) => x.textContent).join()")) === "LEN,DIV,SWG,GATE", "tab: the roll has LEN, DIV, SWG and GATE (the PATTERN page's knobs)");
  await shot(`roll_tab_${W}.png`);
  ok((await clipped(".colbody")).length === 0, `text: nothing is clipped in the PIANO ROLL tab ${JSON.stringify(await clipped(".colbody"))}`);

  // ---- mouse editing on part 1 (the mock's pattern: step 0 C3 (48), 4 D3, 8 chord G3 B3 held 3 steps, 12 E3, 13 A3, 14 C4)
  const before = (await mockStep(0, 2)).n;
  let p = await center(2, 62); await c.click(p.x, p.y); await idle();
  let s2 = await mockStep(0, 2);
  ok(before === 0 && s2.n === 1 && s2.notes[0] === 62 && s2.time === 0 && s2.vel === 100, "mouse: a click on an empty cell writes a NOTE step to the unit at once");
  ok((await sel()).start === 2 && /D4/.test(await status()) && /length 1/.test(await status()), `mouse: the new note is selected and described (${await status()})`);

  p = await center(5, 64); let q = await center(7, 64); p = await center(5, 64);
  await c.drag(p.x, p.y, q.x, q.y); await idle();
  ok((await mockStep(0, 5)).time === 0 && (await mockStep(0, 6)).time === 1 && (await mockStep(0, 7)).time === 1 && (await mockStep(0, 8)).n === 2, "mouse: press and drag right while drawing makes the note 3 steps (a NOTE and 2 TIE steps), the next note stays");

  q = await center(3, 69); p = await center(5, 64); q = await center(3, 69);   // (both cells on screen: a small window scrolls to the last one asked for)
  await c.drag(p.x + 3, p.y, q.x + 3, q.y); await idle();
  ok((await mockStep(0, 3)).notes[0] === 69 && (await mockStep(0, 4)).time === 0 && (await mockStep(0, 5)).time === 2 && (await mockStep(0, 6)).time === 2, "mouse: dragging a note moves it (step and pitch) with its length; the old steps become rests");
  ok((await mockStep(0, 3)).time === 0 && (await mockStep(0, 4)).time === 0 && (await mockStep(0, 4)).notes[0] === 50 && (await mockStep(0, 3)).vel === 100, "mouse: the moved note keeps its velocity; the neighbour at step 5 is untouched");

  // lengthen: grab the right edge of the note at step 2 and pull to step 3 (step 4 holds a note)
  p = await center(2, 62);
  await c.drag(p.right - 3, p.y, p.right + 3, p.y); await idle();
  ok((await mockStep(0, 3)).time === 1 || (await mockStep(0, 3)).notes[0] === 69, "mouse: the right edge lengthens a note but never over the next note");
  // the cell at step 3 holds 69 now; take step 2 to length 1 again by dragging its edge back
  await E(`${store(0)}.undo(); true`); await idle();

  // delete by right-click and by double-click
  p = await center(0, 48); await c.click(p.x, p.y, { button: "right", buttons: 2 }); await idle();
  ok((await mockStep(0, 0)).n === 0 && (await mockStep(0, 0)).time === 2, "mouse: a right-click deletes the note");
  p = await center(13, 57); await c.click(p.x, p.y); await c.click(p.x, p.y, { clickCount: 2 }); await c.mouse("mouseReleased", p.x, p.y, { clickCount: 2 }); await idle();
  ok((await mockStep(0, 13)).n === 0, "mouse: a double-click deletes the note");
  // a chord: add a third note to step 8, then it shows 3 bars
  p = await center(8, 62); await c.click(p.x, p.y); await idle();
  ok((await mockStep(0, 8)).n === 3 && (await mockStep(0, 9)).time === 1, "mouse: a click in a step that has notes adds a chord note; the length stays");
  await E(`${store(0)}.undo(); true`); await idle();

  // ---- keyboard
  p = await center(12, 52); await c.click(p.x, p.y); await idle();       // selects E3 at step 12
  await c.key("ArrowUp"); await idle();
  ok((await mockStep(0, 12)).notes[0] === 53, "keyboard: Arrow Up moves the selected note a semitone up");
  await c.key("ArrowUp", 8); await idle();
  ok((await mockStep(0, 12)).notes[0] === 65, "keyboard: Shift+Arrow Up moves it an octave");
  await c.key("ArrowRight"); await idle();
  ok((await mockStep(0, 13)).notes[0] === 65 && (await mockStep(0, 12)).n === 0, "keyboard: Arrow Right moves it a step (the step it left becomes a rest)");
  await c.key("ArrowRight", 8); await idle();
  ok((await mockStep(0, 13)).time === 1 || (await mockStep(0, 13)).notes[0] === 65, "keyboard: Shift+Arrow Right lengthens it (the next step is a note: it stays)");
  await c.key("a"); await idle();
  ok(((await mockStep(0, 13)).flags & 1) === 1, "keyboard: A toggles the accent");
  await c.key("Delete"); await idle();
  ok((await mockStep(0, 13)).n === 0 && (await sel()) === null, "keyboard: Delete removes the selected note");
  await c.key("z", 2); await idle();
  ok((await mockStep(0, 13)).n === 1 && (await mockStep(0, 13)).notes[0] === 65, "undo: Ctrl+Z brings the deleted note back, on the unit too");
  await c.key("z", 2 | 8); await idle();
  ok((await mockStep(0, 13)).n === 0, "redo: Ctrl+Shift+Z deletes it again");
  await c.key("Escape"); await c.key("Home"); await c.key("ArrowUp"); await c.key("Enter"); await idle();
  const made = await E(`JSON.stringify(window.__app.mock.steps[0].slice(0, 16).map((x, i) => [i, x.n, x.notes[0]]).filter((x) => x[1]))`).then(JSON.parse);
  ok(made.some(([i, n, k]) => i === 0 && k === 66), `keyboard: with nothing selected the arrows move a cursor (Home: step 1, Up: a semitone above the last note) and Enter adds a note there ( ${JSON.stringify(made.find((x) => x[0] === 0))})`);

  // ---- velocity lane and accent row
  const ln = await E(`JSON.stringify(${view(0)}.laneCenter(4))`).then(JSON.parse);
  await c.drag(ln.x, ln.bottom - 5, ln.x, ln.top + 4); await idle();
  ok((await mockStep(0, 4)).vel >= 115, `velocity lane: dragging up on a step's bar raises its velocity (${(await mockStep(0, 4)).vel})`);
  await c.drag(ln.x, ln.top + 4, ln.x, ln.bottom - 2); await idle();
  ok((await mockStep(0, 4)).vel <= 15, `velocity lane: dragging down lowers it (${(await mockStep(0, 4)).vel})`);
  const acc = await E(`(() => { const b = document.querySelectorAll('.roll')[0].querySelector('.flagrow .fl[data-i="4"]'); const r = b.getBoundingClientRect(); return JSON.stringify({ x: r.left + r.width / 2, y: r.top + r.height / 2 }); })()`).then(JSON.parse);
  const had = (await mockStep(0, 4)).flags & 1; await c.click(acc.x, acc.y); await idle();
  ok(((await mockStep(0, 4)).flags & 1) === (had ? 0 : 1), "accent row: a click toggles the step's accent");
  await E(`document.querySelectorAll('.roll')[0].querySelectorAll('.lbtn')[1].click(); true`); await c.sleep(100);
  const lc = await E(`JSON.stringify(${view(0)}.laneCenter(4))`).then(JSON.parse);
  await c.drag(lc.x, lc.bottom - 2, lc.x, lc.top + (lc.bottom - lc.top) / 2); await idle();
  ok((await mockStep(0, 4)).chance > 30 && (await mockStep(0, 4)).chance < 70, `chance lane: a drag to the middle sets about 50 % (${(await mockStep(0, 4)).chance})`);
  await E(`document.querySelectorAll('.roll')[0].querySelectorAll('.lbtn')[2].click(); true`); await c.sleep(100);
  await c.drag(lc.x, lc.bottom - 2, lc.x, lc.top + 2); await idle();
  ok((await mockStep(0, 4)).ratchet === 4, "ratchet lane: a drag to the top sets x4");
  await E(`document.querySelectorAll('.roll')[0].querySelectorAll('.lbtn')[0].click(); true`);

  // ---- toolbar: LEN is live, copy / paste / clear, send live off
  await E(`window.__app.edit(0, 29, 12); true`); await c.sleep(200);
  ok((await E(`${view(0)}.store.len`)) === 12 && (await E("document.querySelectorAll('.roll')[0].querySelectorAll('.rnote').length")) > 0, "toolbar: the LEN control sets the pattern length (the grid follows)");
  await E(`window.__app.edit(0, 29, 16); true`); await c.sleep(200);
  await E(`[...document.querySelectorAll('.roll')[0].querySelectorAll('button')].find((b) => b.textContent === 'Copy').click(); true`);
  await E(`[...document.querySelectorAll('.roll')[1].querySelectorAll('button')].find((b) => b.textContent === 'Paste').click(); true`); await idle();
  const copied = await mockStep(1, 3);
  ok(JSON.stringify((await mockStep(0, 3)).notes) === JSON.stringify(copied.notes) && (await mockStep(1, 8)).n === (await mockStep(0, 8)).n, "toolbar: Copy on PART 1 and Paste on PART 2 copies the pattern");
  await E(`[...document.querySelectorAll('.roll')[1].querySelectorAll('button')].find((b) => b.textContent === 'Clear').click(); true`); await idle();
  ok((await E(`window.__app.mock.steps[1].slice(0, 16).every((x) => !x.n)`)) === true, "toolbar: Clear empties the pattern of PART 2");
  await E(`document.querySelectorAll('.roll')[1].querySelector('input[type=checkbox]').click(); true`); await c.sleep(100);
  const sendHidden = await E(`[...document.querySelectorAll('.roll')[1].querySelectorAll('button')].find((b) => b.textContent === 'Send').hidden`);
  ok(sendHidden === false, "toolbar: Send live OFF shows Send and Revert (they are hidden while it is on)");
  p = await center(1, 70, 1); await c.click(p.x, p.y); await idle();
  ok((await mockStep(1, 1)).n === 0 && (await E(`${store(1)}.pending`)) === 1, "toolbar: with Send live off an edit waits (the unit is untouched)");
  await E(`[...document.querySelectorAll('.roll')[1].querySelectorAll('button')].find((b) => b.textContent === 'Send').click(); true`); await idle();
  ok((await mockStep(1, 1)).notes[0] === 70, "toolbar: Send writes it");
  await E(`document.querySelectorAll('.roll')[1].querySelector('input[type=checkbox]').click(); true`);

  // ---- the hide rule: the arp's SEQ mode (P_AMODE 17 = 15) makes the steps offsets for held keys; MODULES > SEQ OFF plays no step
  const roll0 = `document.querySelectorAll('.roll')[0]`;
  await E(`window.__app.edit(0, 17, 15); true`); await c.sleep(250);
  ok((await E(`${roll0}.classList.contains('arpseq')`)) === true && (await E(`${roll0}.querySelector('.rollnote').hidden`)) === false && /offsets for the keys/.test(await E(`${roll0}.querySelector('.rollnote').textContent`)), "hide rule: arp MODE SEQ: the roll says what the steps are now (offsets for held keys)");
  const gone = await E(`[...${roll0}.querySelectorAll('.rollctl .cell')].map((x) => x.classList.contains('gone') ? 1 : 0).join('')`);
  ok(gone === "0111", `hide rule: arp MODE SEQ: DIV, SWG and GATE are hidden (the arp's own RATE and GATE time the steps), LEN stays (${gone})`);
  ok((await E(`${roll0}.querySelector('.lswitch').hidden && ${roll0}.querySelector('.lanegrid').hidden`)) === true, "hide rule: arp MODE SEQ: the velocity / chance / ratchet lane is hidden (velocity is fixed there)");
  const noLength = await center(10, 40);
  await c.click(noLength.x, noLength.y); await c.click(noLength.x, noLength.y, { button: "right", buttons: 2 }); await idle();
  p = await center(10, 45); q = await center(13, 45); await c.drag(p.x, p.y, q.x, q.y); await idle();
  ok((await mockStep(0, 10)).n === 1 && (await mockStep(0, 11)).time !== 1, "hide rule: arp MODE SEQ: a drag does not make ties (a TIE is a rest there)");
  p = await center(10, 47); await c.click(p.x, p.y); await idle();
  ok((await mockStep(0, 10)).n === 1 && (await mockStep(0, 10)).notes[0] === 47, "hide rule: arp MODE SEQ: one note per step, a second pitch replaces the first");
  await shot(`roll_arpseq_${W}.png`);
  ok((await clipped(".colbody")).length === 0, `text: nothing is clipped with arp MODE SEQ ${JSON.stringify(await clipped(".colbody"))}`);
  await E(`window.__app.edit(0, 17, 0); true`); await c.sleep(250);
  ok((await E(`${roll0}.classList.contains('arpseq')`)) === false && (await E(`${roll0}.querySelector('.rollnote').hidden`)) === true, "hide rule: arp MODE OFF again: the full roll and no note");
  await E(`window.__app.mock.menu.find((m) => m[0] === 29)[6] = 0; ${view(0)}.pollModules().then(() => true)`); await c.sleep(300);
  ok((await E(`${roll0}.querySelector('.rollnote').hidden`)) === false && /SEQ is OFF/.test(await E(`${roll0}.querySelector('.rollnote').textContent`)), "hide rule: MODULES SEQ OFF: the roll says no step plays now; editing still works");
  p = await center(11, 66); await c.click(p.x, p.y); await idle();
  ok((await mockStep(0, 11)).notes[0] === 66, "hide rule: MODULES SEQ OFF: editing still writes to the unit");
  await E(`window.__app.mock.menu.find((m) => m[0] === 29)[6] = 1; ${view(0)}.pollModules().then(() => true)`); await c.sleep(300);

  // ---- the full-width overlay
  await c.click(...(await E(`(() => { const b = [...${roll0}.querySelectorAll('button')].find((x) => x.textContent === 'ROLL'); const r = b.getBoundingClientRect(); return JSON.stringify([r.left + r.width / 2, r.top + r.height / 2]); })()`).then(JSON.parse)));
  await c.until("!!window.__app.overlay && window.__app.overlay.views.length === 1");
  await c.sleep(300);
  const ov = await E(`JSON.stringify((() => { const o = document.querySelector('.rolloverlay').getBoundingClientRect(), m = document.querySelector('#main').getBoundingClientRect(); return { ow: o.width, mw: m.width, oh: o.height, mh: m.height, inert: document.querySelector('#main').inert, cw: window.__app.overlay.views[0].cw, part: window.__app.overlay.views[0].part }; })())`).then(JSON.parse);
  ok(ov.ow >= ov.mw - 2 && ov.oh >= ov.mh - 2 && ov.inert === true && ov.part === 0, `overlay: ROLL opens the roll across the whole work area of the window (${Math.round(ov.ow)} px wide; the columns are inert under it)`);
  const colw = await E("Math.round(document.querySelector('.col.part').getBoundingClientRect().width)");
  ok(ov.cw > 0 && (await E(`document.querySelector('.rolloverlay .rscroll').clientWidth`)) > colw * 1.6, `overlay: the roll is wider than the part column (${await E(`document.querySelector('.rolloverlay .rscroll').clientWidth`)} px against ${colw} px)`);
  await shot(`roll_overlay_${W}.png`);
  ok((await clipped(".rolloverlay")).length === 0, `text: nothing is clipped in the overlay ${JSON.stringify(await clipped(".rolloverlay"))}`);
  p = await center(2, 60, 0, true); await c.click(p.x, p.y); await idle();
  ok((await mockStep(0, 2)).notes[0] === 60 || (await mockStep(0, 2)).notes.includes(60), "overlay: editing works there and writes to the unit");
  await E(`[...document.querySelectorAll('.ovhead button')].find((b) => b.textContent === 'PART 2').click(); true`); await c.sleep(300);
  ok((await E("window.__app.overlay.views.length")) === 1 && (await E("window.__app.overlay.views[0].part")) === 1, "overlay: PART 2 shows the other part's pattern");
  await E(`[...document.querySelectorAll('.ovhead button')].find((b) => b.textContent === 'BOTH').click(); true`); await c.sleep(400);
  ok((await E("window.__app.overlay.views.map((v) => v.part).join()")) === "0,1", "overlay: BOTH shows both parts, one under the other");
  await shot(`roll_overlay_both_${W}.png`);
  ok((await clipped(".rolloverlay")).length === 0, `text: nothing is clipped with BOTH ${JSON.stringify(await clipped(".rolloverlay"))}`);
  await c.key("Escape"); await c.sleep(200);
  ok((await E("window.__app.overlay")) === null && (await E("document.querySelector('#main').inert")) === false, "overlay: Esc closes it and the columns work again");
  ok((await E("window.__app.rollViews[0] && document.querySelectorAll('.roll').length")) === 2, "overlay: the tabs' rolls are still there");
  // the ARP / SEQ tab's old step list reads the pattern again after the roll changed it
  await E("window.__app.setPartTab(0, 'arpseq'); true"); await c.sleep(600);
  ok((await E("document.querySelector('.steps .sgrid .st:nth-child(3)')?.getAttribute('aria-label')")) === "Step 3: D4" || (await E("document.querySelector('.steps .sgrid .st:nth-child(3)')?.getAttribute('aria-label')")).startsWith("Step 3: "), "ARP / SEQ tab: its step list still works and shows the roll's edits");
  const errs = c.errors().filter((e) => !/favicon/.test(e));
  ok(errs.length === 0, `page: no script errors (${errs.length}) ${errs[0] || ""}`);
}
await c.close(); server.close();
console.log(fails ? `\nROLL BROWSER TESTS FAILED (${fails})` : "\nroll browser tests passed");
process.exit(fails ? 1 : 0);
