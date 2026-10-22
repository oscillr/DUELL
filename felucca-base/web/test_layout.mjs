// SPDX-License-Identifier: GPL-3.0-only
// web/editor/layout.json (tests/editordump.c) against the other, older exporter of the firmware tables (tests/descdump.c -> build/host/desc.json):
// the two are written by different code from the same tables, so a label, range, default or value name that differs between them is a bug in one of them.
//   FELUCCA_DESC=../build/host/desc.json node test_layout.mjs
import { readFileSync, existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { join, dirname } from "node:path";
const HERE = dirname(fileURLToPath(import.meta.url));
const dj = process.env.FELUCCA_DESC || join(HERE, "../build/host/desc.json");
const L = JSON.parse(readFileSync(join(HERE, "editor/layout.json"), "utf8"));
let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(104)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
if (!existsSync(dj)) console.log("layout vs desc.json: no build/host/desc.json (run host_tests.sh editorjson)   skip");
else {
const D = JSON.parse(readFileSync(dj, "utf8"));
const same = (a, b) => a.label === b.label && a.fmt === b.fmt && a.min === b.min && a.max === b.max && a.def === b.def && JSON.stringify(a.names) === JSON.stringify(b.names) && (a.unit || "") === (b.unit || "");
const diff = (xs, ys) => xs.map((x, i) => (same(x, ys[i]) ? null : `${i}:${x.label}`)).filter(Boolean);
ok(L.P_COUNT === D.P_COUNT && L.G_COUNT === D.G_COUNT && L.P_E0 === D.P_E0 && L.NSTEP === D.NSTEP, "counts: P_COUNT, G_COUNT, P_E0, NSTEP equal descdump's");
{ const d = diff(L.TP, D.TP); ok(d.length === 0, `track parameters: all ${D.P_E0} descriptors equal descdump's ${d.length ? "(differ: " + d.join(" ") + ")" : ""}`); }
{ const d = diff(L.GP, D.GP); ok(d.length === 0, `global parameters: all ${D.G_COUNT} descriptors equal descdump's ${d.length ? "(differ: " + d.join(" ") + ")" : ""}`); }
{ const d = diff(L.XTP, D.XTP), x = diff(L.XSYNC, D.XSYNC); ok(L.NX === 16 && D.XTP.length === L.NX && d.length === 0 && x.length === 0, `extension slots: all ${D.XTP.length} descriptors and the 2 note-value ones equal descdump's (cloud job I) ${d.length || x.length ? "(differ: " + d.concat(x).join(" ") + ")" : ""}`); }
ok(L.ENG.length === D.ENG.length && L.ENG.every((e, i) => e.name === D.ENG[i].name && JSON.stringify(e.titles) === JSON.stringify(D.ENG[i].titles) && diff(e.edit, D.ENG[i].edit).length === 0), "engines: names, page titles and the 8 EDIT descriptors of all 15 equal descdump's");
ok(JSON.stringify(L.ORDER.map((e) => L.ENG[e].name)) === JSON.stringify(D.ORDER), "engines: the pickable order equals descdump's ORDER");
// the knob order of every enum equals descdump's SHOWN (param_turn from the left end)
{
  const bad = [];
  for (const [scope, idx, names] of D.SHOWN) {
    const d = scope ? L.GP[idx] : L.TP[idx];
    if (!d.order || JSON.stringify(d.order.map((v) => d.names[v - d.min])) !== JSON.stringify(names)) bad.push(`${scope}:${idx}`);
  }
  ok(bad.length === 0, `enum order: ${D.SHOWN.length} enums step in descdump's SHOWN order ${bad.length ? "(differ: " + bad.join(" ") + ")" : ""}`);
}
// every value text equals the firmware's own formatting for the percentage descriptors descdump prints
{
  const bad = [];
  for (const [scope, idx, v, , text] of D.PCT) {
    const d = scope ? L.GP[idx] : L.TP[idx]; if (d.fk < 0) { bad.push(`${scope}:${idx}`); continue; }
    if (L.FMTS[d.fk][v - d.min][0] !== text) bad.push(`${scope}:${idx}@${v}`);
  }
  ok(bad.length === 0, `value text: ${D.PCT.length} formatted percentages equal descdump's ${bad.length ? "(differ: " + bad.slice(0, 6).join(" ") + ")" : ""}`);
}
// each page id is a real parameter of its scope
{
  const bad = L.PAGES.filter((pg) => pg.ids.some((id) => id >= 0 && pg.scope !== "STEP" && pg.scope !== "TRK" && id >= (pg.scope === "GLOBAL" ? L.G_COUNT + 7 : L.P_COUNT) && !(pg.scope === "TRACK" && id >= L.XW_BASE && id < L.XW_BASE + L.NX) && pg.graph !== "TRK" && pg.graph !== "ROLL")).map((p) => p.title);
  ok(bad.length === 0, `pages: every parameter id of PAGES[] is inside its table ${bad.join(" ")}`);
}
// 0.2.25 layout round (owner): STEREO MASTER (K1 BASS MONO = G_BM, K2 WIDTH = G_WD) where WIDTH and MASTER were, then COMPRESSOR (K1 RATIO, OFF its lowest value, K2 THRESH, K3 ATTACK,
// K4 RELEASE); the pseudo ids past the stored globals (G_BM = G_COUNT, G_CT .. G_CL, G_WD), the cards the editor shows from MENU settings 33, 42 and 34..37
{ const sm = L.PAGES.find((p) => p.title === "STEREO MASTER"), cp = L.PAGES.find((p) => p.title === "COMPRESSOR"), sh = L.PAGES.findIndex((p) => p.title === "SHIMMER"), d = L.MASTER, c = L.COMP, m = L.MSTG;
  const fv = (q, v) => L.FMTS[q.fk][v - q.min].join("|");
  ok(sm && sm.fam === "GLO" && sm.scope === "GLOBAL" && sm.graph === "MSTG" && L.PAGES.indexOf(sm) === sh + 1 && sm.ids.join() === [L.ID.G_BM, L.ID.G_WD, -1, -1].join() && L.ID.G_BM === L.G_COUNT && L.ID.G_WD === L.G_COUNT + 5
     && d.label === "BMONO" && d.min === 0 && d.max === 11 && d.def === 8 && d.fk >= 0 && fv(d, 0) === "OFF|" && fv(d, 1) === "40|Hz" && fv(d, 5) === "100|Hz" && fv(d, 8) === "200|Hz" && fv(d, 11) === "400|Hz"
     && m.length === 1 && m[0].label === "WIDTH" && m[0].max === 200 && m[0].def === 100 && m[0].min === 0 && fv(m[0], 0) === "0|%" && fv(m[0], 100) === "100|%" && fv(m[0], 200) === "200|%"
     && !L.PAGES.some((p) => p.title === "WIDTH" || p.title === "MASTER" || p.title === "COMP" || p.title === "SATURATOR" || p.title === "BASS MONO") && L.ID.G_SD === undefined && L.GP.length === L.G_COUNT,
    "pages: STEREO MASTER (0.2.25 layout round; GLO, GLOBAL, after SHIMMER: K1 BASS MONO OFF / 40 .. 400 Hz, default 200 Hz; K2 WIDTH 0 .. 200 %, default 100), no WIDTH, MASTER or COMP page; GP keeps the stored globals only");
  ok(cp && cp.fam === "GLO" && cp.scope === "GLOBAL" && cp.graph === "COMP" && L.PAGES.indexOf(cp) === L.PAGES.indexOf(sm) + 1 && cp.ids.join() === [L.ID.G_CR, L.ID.G_CT, L.ID.G_CA, L.ID.G_CL].join() && L.ID.G_CT === L.G_COUNT + 1 && L.ID.G_CL === L.G_COUNT + 4
     && c.length === 4 && c.map((q) => q.label).join() === "THRESH,RATIO,ATTACK,RELEASE" && c.map((q) => q.max).join() === "40,8,120,80" && c.map((q) => q.def).join() === "23,0,80,40" && c.every((q) => q.min === 0 && q.fk >= 0)
     && fv(c[0], 0) === "-40|dB" && fv(c[0], 23) === "-17|dB" && fv(c[0], 40) === "0|dB" && [0, 1, 2, 3, 4, 5, 6, 7, 8].map((v) => fv(c[1], v)).join() === "OFF|,1.5:1|,2:1|,3:1|,4:1|,6:1|,8:1|,12:1|,INF|"
     && fv(c[2], 0) === "0.1|ms" && fv(c[2], 40) === "1|ms" && fv(c[2], 80) === "10|ms" && fv(c[2], 120) === "100|ms" && fv(c[3], 0) === "20|ms" && fv(c[3], 20) === "63|ms" && fv(c[3], 40) === "200|ms" && fv(c[3], 68) === "1|s" && fv(c[3], 80) === "2|s",
     "pages: COMPRESSOR (GLO, GLOBAL, right after STEREO MASTER; K1 RATIO OFF (the stored 1:1) 1.5:1 .. INF, K2 THRESH -40 .. 0 dB, K3 ATTACK 0.1 .. 100 ms, K4 RELEASE 20 ms .. 2 s, 40 steps a decade; defaults -17 dB / OFF / 10 ms / 200 ms)"); }
ok(L.FMTS.every((t) => t.every((x) => Array.isArray(x) && typeof x[0] === "string" && typeof x[1] === "string")) && L.FMTS.length > 20, `value text: ${L.FMTS.length} formatted value tables, every entry a [text, unit] pair`);
}
// ---- 0.2.25 (owner: "VCE touches the POLY/MONO switch row, UNI touches PRIO"): in a real browser (headless Chrome, the simulated unit): no two neighbouring controls of a card touch or
// overlap (a gap of 4 px at least), no control reaches out of its own cell, no label touches its control; no long label pushes its value to another line (at most two lines, never cut).
// T0229 (owner: "NO scrolling"): the OVERFLOW test: at the rooms a browser leaves on a 1920 x 1080, 1440 x 900 and 1280 x 720 screen (1920 x 940, 1440 x 780, 1280 x 600 inside the
// browser), every part tab of every engine (with the most controls each card can show: UNISON with GLIDE, INSERT on, DUCK PUMP, EQ, shimmer, PING-PONG, the compressor on) and every
// GLOBAL tab fits its column at its natural size (the column's own 90 % safety switched off): no page scroll bar, no column that would scroll, no tab strip wider than its column, no element
// cut. The exceptions are lists and views that scroll by nature: the PATCHES list of 128, the piano roll's grid, the OPERATORS tab (FM6's voice editor as it is, owner 0e: its full-page editor
// comes later). Below 1260 x 600 the page is scaled, never scrolled. Skipped without Chrome
const ONLY = process.env.LAYOUT_ONLY;   // (a shortcut while working: LAYOUT_ONLY=lists or structure runs only that browser part)
if (!ONLY) await spacing();
if (!ONLY || ONLY === "structure") await structure();
if (!ONLY || ONLY === "lists") await lists();
if (!ONLY || ONLY === "linked") await linked();
console.log(fails ? `\nLAYOUT TESTS FAILED (${fails})` : "\nlayout tests passed");
process.exit(fails ? 1 : 0);

async function spacing() {
  const { launch, CHROME } = await import("./cdp.mjs");
  if (!CHROME) { console.log("spacing / overflow: no Chrome found   skip"); return; }
  const http = await import("node:http"), { readFile } = await import("node:fs/promises"), { extname, normalize } = await import("node:path");
  const MIME = { ".js": "text/javascript", ".json": "application/json", ".html": "text/html", ".css": "text/css", ".png": "image/png", ".svg": "image/svg+xml" };
  const server = http.createServer(async (req, res) => { try { let p = decodeURIComponent(new URL(req.url, "http://x").pathname); if (p.endsWith("/")) p += "index.html"; const f = normalize(join(HERE, p)); if (!f.startsWith(HERE)) throw 0; const b = await readFile(f); res.writeHead(200, { "content-type": MIME[extname(f)] || "application/octet-stream" }); res.end(b); } catch { res.writeHead(404); res.end(); } });
  await new Promise((r) => server.listen(0, "127.0.0.1", r));
  let c = await launch();
  const E = (x) => c.eval(x);
  // the spacing check, in the page: [text] for every pair of visible sibling controls closer than 4 px, and every control that leaves its cell
  const CHECK = `(() => {
    const out = [], vis = (el) => el.offsetParent && getComputedStyle(el).visibility !== 'hidden' && el.getBoundingClientRect().width > 0;
    const name = (el) => (el.querySelector('.lab')?.textContent || el.querySelector('select')?.getAttribute('aria-label') || el.textContent || '').trim().slice(0, 24);
    const where = (el) => (el.closest('section')?.querySelector('h3 .ttl, h3')?.textContent || '').trim().slice(0, 24);
    const pairs = (els) => { for (let i = 0; i < els.length; i++) for (let j = i + 1; j < els.length; j++) {
      const a = els[i].getBoundingClientRect(), b = els[j].getBoundingClientRect();
      const vy = Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top), vx = Math.min(a.right, b.right) - Math.max(a.left, b.left);
      if (vy > 0 && Math.max(b.left - a.right, a.left - b.right) < 4 && vx <= 0 + 4) out.push(where(els[i]) + ': ' + name(els[i]) + ' | ' + name(els[j]) + ' (' + Math.round(Math.max(b.left - a.right, a.left - b.right)) + ' px apart)');
      else if (vx > 0 && vy > -4 && Math.max(b.top - a.bottom, a.top - b.bottom) < 4) out.push(where(els[i]) + ': ' + name(els[i]) + ' over ' + name(els[j]) + ' (' + Math.round(Math.max(b.top - a.bottom, a.top - b.bottom)) + ' px apart)');
    } };
    for (const g of document.querySelectorAll('#main .grid, #main .hgrid, #main .mrow, #main .scgrid, .colhead .rowhead')) {
      if (!vis(g)) continue;
      pairs([...g.children].filter((x) => vis(x) && !x.classList.contains('gone')));
    }
    for (const cell of document.querySelectorAll('#main .cell')) {
      if (!vis(cell) || cell.classList.contains('gone')) continue;
      const r = cell.getBoundingClientRect(), inline = !!cell.closest('.hgrid') || cell.classList.contains('bare');
      for (const k of cell.querySelectorAll('.segs, select, .slider, .seg' + (inline ? '' : ', .top'))) { const q = k.getBoundingClientRect(); if (q.width && (q.right > r.right + 0.5 || q.left < r.left - 0.5)) out.push(where(cell) + ': ' + name(cell) + ' reaches out of its cell'); }
      const top = cell.querySelector('.top'), ctl = cell.querySelector('.segs, select, .slider');
      if (!inline && top && ctl && top.getBoundingClientRect().height) { const t = top.getBoundingClientRect(), q = ctl.getBoundingClientRect(); if (q.top < t.bottom + 2) out.push(where(cell) + ': ' + name(cell) + ' label touches its control (' + Math.round(q.top - t.bottom) + ' px)'); }
      if (inline) { const l = cell.querySelector('.lab'), v = cell.querySelector('.right'); for (const x of [l, v]) if (x && ctl && x.getBoundingClientRect().width) { const a = x.getBoundingClientRect(), q = ctl.getBoundingClientRect(); if (Math.max(q.left - a.right, a.left - q.right) < 4) out.push(where(cell) + ': ' + name(cell) + ' text touches its control'); } }
      const lab = cell.querySelector('.top .lab'), right = cell.querySelector('.top .right');
      if (!inline && lab && right && right.getBoundingClientRect().width > 0) {
        const l = lab.getBoundingClientRect(), v = right.getBoundingClientRect(), lh = parseFloat(getComputedStyle(lab).lineHeight) || 14;
        if (v.top > l.top + lh * 0.6) out.push(where(cell) + ': ' + name(cell) + ' value pushed under its label (' + Math.round(v.top - l.top) + ' px down)');
        if (v.left < l.right - 0.5) out.push(where(cell) + ': ' + name(cell) + ' value over its label');
      }
      if (lab && lab.textContent) {
        const l = lab.getBoundingClientRect(), lh = parseFloat(getComputedStyle(lab).lineHeight) || 14;
        if (l.height > 2 * lh + 1.5) out.push(where(cell) + ': ' + name(cell) + ' label on more than two lines (' + Math.round(l.height / lh) + ')');
        if (lab.scrollWidth > lab.clientWidth + (window.__app.zoom < 1 ? 8 : 1)) out.push(where(cell) + ': ' + name(cell) + ' label cut or a word too long for its cell (page zoom < 1 allows 8 px: the browser lays zoomed text out a little wider, e.g. EQ MID FREQUENCY 64 in 60 at 0.81) (' + lab.scrollWidth + ' in ' + lab.clientWidth + ')');
      }
    }
    return JSON.stringify([...new Set(out)]);
  })()`;
  // the overflow check, in the page: every column body, every element that clips or scrolls (except the lists that scroll by nature), the tab strips, the page itself
  const FIT = `(() => {
    window.__app.fitNow();   // (owner, sizing policy: the editor's own scaling is on: a tab is scaled to 85 % at most; one that still needs more scrolls: reported as SCROLL, allowed below Full HD)
    const out = [], vis = (el) => !!el.offsetParent || el === document.body;
    const de = document.documentElement;
    if (de.scrollHeight > innerHeight + 1 || de.scrollWidth > innerWidth + 1 || document.body.scrollHeight > document.body.clientHeight + 1) out.push('the page scrolls (' + de.scrollWidth + ' x ' + de.scrollHeight + ' in ' + innerWidth + ' x ' + innerHeight + ')');
    for (const [i, col] of window.__app.cols.entries()) {
      if (!col || !vis(col.col)) continue;
      const tab = col.body.dataset.tab, nm = (i < 2 ? 'PART ' + (i + 1) : 'GLOBAL') + ' ' + tab;
      if (col.strip.scrollWidth > col.strip.clientWidth + 1) out.push(nm + ': the tab strip needs ' + col.strip.scrollWidth + ' px, has ' + col.strip.clientWidth);
      if (col.body.querySelector('.opsopen')) continue;
      if (col.body.scrollHeight > col.body.clientHeight + 1) out.push('SCROLL ' + nm + ': needs ' + col.body.scrollHeight + ' px, has ' + col.body.clientHeight + ' (zoom ' + (col.body.firstElementChild?.style.zoom || '1') + ')');
      for (const el of col.col.querySelectorAll('*')) {
        if (!vis(el) || el.closest('.plist, .rscroll, .rruler, .rkeys, .lscroll, .rgrid, .pats')) continue;
        const ov = getComputedStyle(el); if (!/(auto|hidden|scroll)/.test(ov.overflowX + ov.overflowY)) continue;
        if (el.matches('.cell .right, .cell .lab, select, input, .pname, .unit, .msg, .tabs, .colbody') || ov.textOverflow === 'ellipsis') continue;
        if (el.scrollHeight > el.clientHeight + 1 || el.scrollWidth > el.clientWidth + 1) out.push(nm + ': ' + (el.className || el.tagName) + ' cut or scrolling (' + el.scrollWidth + ' x ' + el.scrollHeight + ' in ' + el.clientWidth + ' x ' + el.clientHeight + ')');
      }
    }
    return JSON.stringify(out);
  })()`;
  // room / need of each column: the tab's natural height; a tab that fills its column by design: the piano roll ("fills": its grid scrolls inside), PATCHES ("list": the 128 slots
  // scroll inside), MULTIS / PATTERNS (the rows' natural height), WAVETABLES (the cards')
  const ROOM = `JSON.stringify(window.__app.cols.map((col) => { if (!col || !col.col.offsetParent) return null; const b = col.body, cs = getComputedStyle(b), pad = parseFloat(cs.paddingTop) + parseFloat(cs.paddingBottom), H = (e) => e.getBoundingClientRect().height;
    let t = b.firstElementChild, need;
    if (t?.classList.contains('rolltab')) need = 'fills';
    else if (t?.classList.contains('gbody') && !t.firstElementChild?.classList.contains('tabbody')) {
      const w = t.firstElementChild;
      if (w?.classList.contains('patches') || w?.querySelector('.pats')) need = 'list';   // (the PATTERNS list of 32 scrolls inside, like the 128 patches)
      else if (w?.classList.contains('multiswrap')) { const kids = [...w.children]; need = kids.reduce((n, k) => n + (k.classList.contains('multis') ? Math.max(0, ...[...k.children].map((r) => r.getBoundingClientRect().bottom)) - k.getBoundingClientRect().top + 2 : H(k)), 0) + 4 * (kids.filter((k) => H(k) > 0).length - 1); }
      else need = [...t.children].reduce((n, k) => n + H(k), 0);
    } else { if (t?.classList.contains('gbody')) t = t.firstElementChild; need = H(t); }
    return [b.dataset.tab, Math.round(b.clientHeight - pad), typeof need === 'number' ? Math.ceil(need) : need]; }))`;
  const worst = (p) => `(() => { const u = window.__app.unit; for (const [id, v] of [[37, 3], [38, 20], [96, 1], [111, 0], [91, 0], [83, 0], [102, 0], [17, 1], [43, 1]]) u.writeParam(${p}, id, v); return u.idle().then(() => true); })()`;
  const SIZES = [[1920, 940, "1920 x 1080"], [1440, 780, "1440 x 900"], [1280, 600, "1280 x 720"], [1024, 560, "1024 x 640"]];
  const scrolled = [];
  const table = [];
  for (const [W, H, screen] of SIZES) {
    if (screen !== SIZES[0][2]) { await c.close(); c = await launch(); }   // (a fresh browser for each size: a second navigation now and then stalls headless Chrome)
    await c.open(`http://127.0.0.1:${server.address().port}/editor/?mock=1`, W, H);
    await c.until("window.__app && window.__app.unit && window.__app.mock");
    await c.sleep(500);
    { const lg = JSON.parse(await E(`(() => { const i = document.querySelector('#top .hdr').getBoundingClientRect(), t = document.getElementById('top'), k = t.getBoundingClientRect(); return JSON.stringify([Math.round(i.width), Math.round(i.height), t.scrollWidth <= t.clientWidth + 1, [...t.children].every((e) => e.tagName === "IMG" || !e.offsetParent || e.getBoundingClientRect().height < k.height + 1), Math.round(k.height)]); })()`)), want = null;
      const z = await E(`window.__app.zoom`), lh = lg[1], lw = lg[0];
      ok(lh >= 56 - 1 && (screen !== "1920 x 1080" || lh === 84) && lg[2] && lg[3], `logo ${screen}: ${lw} x ${lh} px as drawn (page scale ${(+z).toFixed(2)}; want 84 at Full HD, never under 56), top bar ${lg[4]} px high, nothing clipped`);
      // GLOBAL can always be opened: whenever the column is hidden (window under 1500 px) a visible GLOBAL button is on the screen, and it shows the column
      const gl = JSON.parse(await E(`(() => { const g = document.querySelector('.col.glob'), b = document.getElementById('vglobal'), r = b.getBoundingClientRect(); const hidden = getComputedStyle(g).display === 'none'; const vis = !!b.offsetParent && r.width > 0 && r.right <= innerWidth + 1 && r.left >= 0 && r.bottom <= innerHeight; return JSON.stringify({ hidden, vis }); })()`));
      ok(!gl.hidden || gl.vis, `GLOBAL ${screen}: the column is ${gl.hidden ? "hidden and the GLOBAL button is visible on the screen" : "shown"}`);
      if (gl.hidden) { await E(`document.getElementById('vglobal').click(); true`); await c.sleep(150); ok(await E(`getComputedStyle(document.querySelector('.col.glob')).display !== 'none'`), `GLOBAL ${screen}: pressing the button shows the GLOBAL column`); await E(`document.getElementById('vparts').click(); true`); } }
    const bad = new Set(), over = new Set(), need = new Map();
    const note = (k, room, n) => { const x = need.get(k); if (!x || typeof n !== 'number' || n > x[1]) need.set(k, [room, n]); };
    await E(`${worst(0)}.then(() => ${worst(1)}).then(() => { const u = window.__app.unit; u.writeGlobal(24, 3); u.writeGlobal(15, 1); return u.idle(); }).then(() => window.__app.unit.readMenu()).then(() => window.__app.unit.setMenu(35, 2)).then(() => true)`);
    await c.sleep(300);
    // every engine one can pick, on both parts; every part tab
    const engs = JSON.parse(await E(`JSON.stringify([...window.__app.cols[0].engSel.options].map((o) => +o.value))`));
    for (const eng of engs) {
      await E(`Promise.all([window.__app.unit.setEngine(0, ${eng}), Promise.resolve()]).then(() => window.__app.unit.setEngine(1, ${eng})).then(() => true)`); await c.sleep(500);
      const en = await E(`window.__app.unit.parts[0].eng === ${eng} ? window.__app.cols[0].engSel.selectedOptions[0].textContent : '?'`);
      for (const tab of ["engine", "filter", "envelope", "lfo", "matrix", "fx", "arpseq", "roll"]) {
        await E(`window.__app.setPartTab(0, '${tab}'); window.__app.setPartTab(1, '${tab}'); true`); await c.sleep(tab === "engine" ? 450 : 200);
        for (const x of JSON.parse(await E(CHECK))) bad.add(`${en} ${tab}: ${x}`);
        for (const x of JSON.parse(await E(FIT))) { over.add(`${en}: ${x}`); if (process.env.LAYOUT_DEBUG) console.log(x, await E(`innerWidth + 'x' + innerHeight + ' sl=' + getComputedStyle(document.documentElement).getPropertyValue('--sl') + ' cells=' + [...window.__app.cols[0].body.querySelectorAll('.card .cell:not(.gone)')].slice(0, 7).map((c) => Math.round(c.getBoundingClientRect().height) + '/' + Math.round(c.querySelector('.top').getBoundingClientRect().height) + '/' + c.querySelector('.right').textContent).join(' ') + ' z=' + document.body.style.zoom`), await E(`[...window.__app.cols[0].body.querySelectorAll('.card')].map((c) => c.querySelector('.ttl')?.textContent + ' ' + Math.round(c.getBoundingClientRect().height) + ' [' + [...c.querySelectorAll('.cell:not(.gone) .lab')].map((l) => l.textContent + (l.getBoundingClientRect().height > 16 ? '(2)' : '')).join(',') + ']').join(' | ')`)); }
        const r = JSON.parse(await E(ROOM)); for (const k of [0, 1]) if (r[k]) note(tab === "roll" ? "SEQ" : tab.toUpperCase(), r[k][1], r[k][2]);
      }
    }
    // the GLOBAL column's tabs (below 1500 px: the GLOBAL view)
    for (const g of ["global", "patches", "multis", "patterns", "midi", "tables", "settings"]) {
      await E(`window.__app.setGlobalTab('${g}', true); true`); await c.sleep(g === "settings" || g === "patches" ? 800 : 400);
      if (g === "patterns") { await E(`[...document.querySelectorAll('.col.glob button')].find((b) => b.textContent === 'Load from unit')?.click(); true`); await c.sleep(1500); }
      if (g === "multis") { await E(`[...document.querySelectorAll('.col.glob button')].find((b) => b.textContent === 'Load from unit')?.click(); true`); await c.sleep(1500); }
      if (g === "patches") { await E(`document.querySelector('.plist li[data-slot]')?.click(); true`); await c.sleep(300); }
      for (const x of JSON.parse(await E(CHECK))) bad.add(`GLOBAL ${g}: ${x}`);
      for (const x of JSON.parse(await E(FIT))) over.add(x);
      const r = JSON.parse(await E(ROOM)); if (r[2]) note(`GLOBAL ${g.toUpperCase()}`, r[2][1], r[2][2]);
    }
    await E(`window.__app.setView('parts'); true`);
    ok(bad.size === 0, `spacing ${screen} (room ${W} x ${H}): no two neighbouring controls closer than 4 px, no control out of its cell, no label on its control; every value on its label's first line, labels at most two lines, nothing cut${bad.size ? "\n    " + [...bad].slice(0, 14).join("\n    ") : ""}`);
    for (const x of [...over]) if (/(^|: )SCROLL /.test(x) && screen !== "1920 x 1080") { over.delete(x); scrolled.push(`${screen}: ${x.replace(/^.*?SCROLL /, "")}`); }
    ok(over.size === 0, `overflow ${screen} (room ${W} x ${H}): every part tab of every engine and every GLOBAL tab fits its column, no tab strip scrolls, no page scroll bar${over.size ? "\n    " + [...over].slice(0, 14).join("\n    ") : ""}`);
    table.push([screen, need]);
  }
  console.log("\ntabs that scroll as the last resort (need more than 85 % scaling gives), below Full HD:\n  " + (scrolled.length ? [...new Set(scrolled)].join("\n  ") : "none"));
  console.log("\nfit (room / need in px, the most any engine needs; scale used = min(1, room/need), at least 0.85):");
  for (const [screen, need] of table) console.log(`  ${screen.padEnd(12)} ${[...need].map(([k, [r, n]]) => `${k} ${r}/${n}${n > r ? " x" + Math.max(0.85, r / n).toFixed(2) : ""}`).join(", ")}`);
  // below the smallest room: the page is drawn smaller, never scrolled
  await c.size(1100, 540); await c.sleep(400);
  const sc = JSON.parse(await E(`JSON.stringify({ z: window.__app.zoom, sh: document.documentElement.scrollHeight, sw: document.documentElement.scrollWidth, ih: innerHeight, iw: innerWidth })`));
  ok(sc.z < 1 && sc.sh <= sc.ih + 1 && sc.sw <= sc.iw + 1, `below the minimum (1100 x 540 inside the browser): the page is scaled (${sc.z.toFixed(2)}) and does not scroll`);
  // the matrix: all eight slots, always, one list (owner: the editor does not mirror the unit's packing)
  await c.size(1920, 940); await c.sleep(300);
  await E(`(() => { const u = window.__app.unit; for (let k = 0; k < 8; k++) u.writeParam(0, 49 + 3 * k, 0); return u.idle().then(() => true); })()`);
  await E(`window.__app.setPartTab(0, 'matrix'); true`); await c.sleep(300);
  const mx = JSON.parse(await E(`JSON.stringify({ rows: window.__app.cols[0].body.querySelectorAll('.mrow:not(.mhead)').length, cards: window.__app.cols[0].body.querySelectorAll('.card').length, titles: [...window.__app.cols[0].body.querySelectorAll('h3')].map((x) => x.textContent) })`));
  ok(mx.rows === 8 && mx.cards === 1 && mx.titles.length === 0, `matrix: one list of 8 slots with every slot empty (no MATRIX 1-4 / 5-8 cards, no title) (${mx.rows} rows, ${mx.cards} card)`);
  // ---- cloud job I: the FILTER tab (FILTER_PAGES_PROPOSAL 3), SUB OSC, CYCLE ENV, STEPS, the tab strip on one line
  {
    await c.size(1280, 600); await c.sleep(300);
    const U = (x) => E(`(async () => { const u = window.__app.unit; ${x}; await u.idle(); window.__app.refresh(); return true; })()`);
    const cardsOf = async (tab, part = 0) => { await E(`window.__app.setPartTab(${part}, '${tab}'); true`); await c.sleep(250); return JSON.parse(await E(`JSON.stringify([...window.__app.cols[${part}].body.querySelectorAll('.card')].filter((x) => !x.hidden && x.offsetParent).map((x) => ({ t: x.querySelector('h3 .ttl')?.textContent ?? '', labs: [...x.querySelectorAll('.cell:not([hidden]) .lab')].map((l) => l.textContent), flow: x.querySelector('.flow')?.textContent ?? null })))`)); };
    const labs = (cs, t) => cs.find((x) => x.t === t)?.labs.join();
    const engIdx = (n) => E(`[...window.__app.cols[0].engSel.options].find((o) => o.textContent === '${n}').value`);
    for (const n of ["ANALOG", "PHASE", "WAVE", "PHYS", "FM6"]) {
      await E(`window.__app.unit.setEngine(0, ${await engIdx(n)}).then(() => true)`); await c.sleep(300);
      await U(`u.writeParam(0, 43, 0)`);
      // the tab strip: one line at 1280 wide (the stop rule: if it did not fit, nothing is shrunk, the widths are reported)
      const st = JSON.parse(await E(`JSON.stringify((() => { const s = window.__app.cols[0].strip, t = [...s.querySelectorAll('[role=tab]')]; return { sw: s.scrollWidth, cw: s.clientWidth, tops: [...new Set(t.map((x) => Math.round(x.getBoundingClientRect().top)))], keys: t.map((x) => x.dataset.key), w: t.map((x) => Math.round(x.getBoundingClientRect().width)), last: Math.round(t.at(-1).getBoundingClientRect().right), right: Math.round(s.getBoundingClientRect().right) }; })())`));
      ok(st.tops.length === 1 && st.sw <= st.cw && st.last <= st.right && st.keys.join() === "engine,filter,envelope,lfo,matrix,fx,arpseq,roll",
        `FILTER tab / ${n} 1280 px: the tab strip is one line, ENGINE FILTER ENVELOPE LFO ... in order (${st.keys.length} tabs, ${st.w.reduce((a, b) => a + b, 0)} px of tabs in a strip of ${st.cw} px, the last ends ${st.right - st.last} px before the edge)`);
      if (n === "FM6") console.log(`  tab strip at 1280 px wide (the part column ${st.cw} px): ${st.keys.map((k, i) => k + " " + st.w[i]).join(", ")}`);
      const eng = await cardsOf("engine");
      ok(!eng.some((x) => x.t === "MULTI FILTER" || x.t === "CUTOFF MOD"), `ENGINE tab / ${n}: no MULTI FILTER and no CUTOFF MOD card any more (${eng.map((x) => x.t).join(" | ")})`);
      const app = await E(`window.__app.cycleTab`);
      ok(labs(eng, "SUB OSC") === "WAVE,OCTAVE,LEVEL" && !eng.some((x) => x.t === "CYCLE ENV") && eng.some((x) => x.t === "VOICE"), `ENGINE tab / ${n}: engine card, SUB OSC (WAVE, OCTAVE, LEVEL), VOICE; CYCLE ENV is on the ${app} tab`);
      { const ev = await cardsOf(app); ok(labs(ev, "CYCLE ENV") === "MODE,SYNC,RISE,FALL,CURVE" && app === "envelope", `${app.toUpperCase()} tab / ${n}: CYCLE ENV (MODE, SYNC, RISE, FALL, CURVE) after FILTER ENVELOPE`); await cardsOf("engine"); }
      if (n === "WAVE") ok(/^TABLE,POSITION,STEPS,/.test(eng[0].labs.join()) && await E(`!!window.__app.cols[0].body.querySelector('.card .cell input[aria-label="STEPS"][type=range]')`), `WAVE card: STEPS is a slider right beside POSITION (${eng[0].labs.join()})`);
      // TYPE OFF: TYPE alone, no TWIN MOD, no TWIN CHAIN
      let f = await cardsOf("filter");
      ok(f.map((x) => x.t).join() === "MULTI FILTER,CUTOFF MOD,TWIN FILTER" && labs(f, "TWIN FILTER") === "TYPE",
         `FILTER tab / ${n}, TYPE OFF: MULTI FILTER, CUTOFF MOD, and TWIN FILTER with the TYPE control only; no TWIN MOD, no TWIN CHAIN (${f.map((x) => x.t + "[" + x.labs.length + "]").join(" ")})`);
      ok(labs(f, "MULTI FILTER").startsWith("CUTOFF,RESONANCE,MODE,SLOPE") && /FILTER ENV,(KEY TRACK,)?LFO 1,LFO 2/.test(labs(f, "CUTOFF MOD")), `FILTER tab / ${n}: MULTI FILTER CUTOFF RESONANCE MODE SLOPE; CUTOFF MOD FILTER ENV (KEY TRACK) LFO 1 LFO 2`);
      // TYPE LP: all the cards, the names, the flow line
      await U(`u.writeParam(0, 43, 1)`); f = await cardsOf("filter");
      ok(labs(f, "TWIN FILTER") === "TYPE,LINK,OFFSET,RESONANCE" && labs(f, "TWIN MOD") === "FILTER ENV,KEY TRACK,LFO 1,LFO 2" && labs(f, "TWIN CHAIN") === "ROUTE,DRIVE,BLEND",
         `FILTER tab / ${n}, TYPE LP: TWIN FILTER (TYPE, LINK, OFFSET, RESONANCE), TWIN MOD (FILTER ENV, KEY TRACK, LFO 1, LFO 2), TWIN CHAIN (ROUTE, DRIVE, BLEND) (${labs(f, "TWIN FILTER")} | ${labs(f, "TWIN MOD")} | ${labs(f, "TWIN CHAIN")})`);
      ok(f.find((x) => x.t === "TWIN CHAIN").flow === "ENGINE > MULTI > DRIVE > TWIN > AMP", "FILTER tab: the flow line of SERIES is ENGINE > MULTI > DRIVE > TWIN > AMP");
      await U(`u.writeX(0, 0, 1)`); f = await cardsOf("filter");
      ok(f.find((x) => x.t === "TWIN CHAIN").flow === "ENGINE > DRIVE > MULTI + TWIN > AMP", "FILTER tab: PARALLEL puts the drive before both filters");
      await U(`u.writeX(0, 0, 0)`);
      await U(`u.writeX(0, 1, 0)`); f = await cardsOf("filter");
      ok(labs(f, "TWIN FILTER") === "TYPE,LINK,CUTOFF,RESONANCE" && labs(f, "TWIN MOD") === "FILTER ENV,KEY TRACK,LFO 1,LFO 2", "FILTER tab: LINK off renames OFFSET to CUTOFF and hides nothing");
      await U(`u.writeX(0, 1, 1)`);
      // the DRIVE says OFF at 0; the centre of the bipolar sliders is marked; the controls
      const ctl = JSON.parse(await E(`JSON.stringify((() => { const q = (t, l) => [...window.__app.cols[0].body.querySelectorAll('.card')].find((x) => x.querySelector('h3 .ttl')?.textContent === t)?.querySelectorAll('.cell'); const by = (t, l) => [...q(t)].find((c) => c.querySelector('.lab')?.textContent === l);
        return { drive: by('TWIN CHAIN', 'DRIVE').querySelector('.val').textContent, blendBi: !!by('TWIN CHAIN', 'BLEND').querySelector('.slider.bipolar'), offBi: !!by('TWIN FILTER', 'OFFSET').querySelector('.slider.bipolar'), ktBi: !!by('TWIN MOD', 'KEY TRACK').querySelector('.slider.bipolar'),
          resBi: !!by('TWIN FILTER', 'RESONANCE').querySelector('.slider.bipolar'), typeSel: by('TWIN FILTER', 'TYPE').querySelector('select')?.tagName, routeSel: by('TWIN CHAIN', 'ROUTE').querySelector('select')?.tagName, link: [...by('TWIN FILTER', 'LINK').querySelectorAll('.seg')].map((b) => b.textContent).join() }; })())`));
      ok(ctl.drive === "OFF" && ctl.blendBi && ctl.offBi && ctl.ktBi && !ctl.resBi && ctl.typeSel === "SELECT" && ctl.routeSel === "SELECT" && ctl.link === "OFF,ON", `FILTER tab / ${n}: DRIVE shows OFF at 0; BLEND, OFFSET, KEY TRACK are marked at the centre, RESONANCE is not; TYPE and ROUTE are drop-downs, LINK a switch (${JSON.stringify(ctl)})`);
      // TYPE list: COMB not on WAVE and PHYS
      const opts = JSON.parse(await E(`JSON.stringify([...[...window.__app.cols[0].body.querySelectorAll('.card')].find((x) => x.querySelector('h3 .ttl')?.textContent === 'TWIN FILTER').querySelectorAll('select option')].filter((o) => !o.hidden && !o.disabled).map((o) => o.textContent))`));
      ok(opts.join() === (n === "WAVE" || n === "PHYS" ? "OFF,LOW-PASS,BAND-PASS,HIGH-PASS,NOTCH,LADDER" : "OFF,LOW-PASS,BAND-PASS,HIGH-PASS,NOTCH,LADDER,COMB"), `TYPE list / ${n}: ${opts.join(" ")} (COMB is not offered on WAVE and PHYS)`);
      if (n !== "WAVE" && n !== "PHYS" && n !== "FM6") {
        await U(`u.writeParam(0, 43, 6)`); f = await cardsOf("filter");
        ok(labs(f, "TWIN FILTER") === "TYPE,TUNE,FEEDBACK" && labs(f, "TWIN MOD") === "FILTER ENV,LFO 1,LFO 2" && labs(f, "TWIN CHAIN") === "ROUTE,DRIVE,BLEND",
           `FILTER tab / ${n}, COMB: LINK and KEY TRACK are not drawn, the cutoff reads TUNE and the resonance FEEDBACK (${labs(f, "TWIN FILTER")} | ${labs(f, "TWIN MOD")})`);
        const tip = await E(`[...window.__app.cols[0].body.querySelectorAll('.card h3 .ttl')].find((x) => x.textContent === 'TWIN FILTER').title`);
        ok(/LINK: the comb follows the played note by itself/.test(tip), "FILTER tab: the card's tooltip says why LINK is not drawn (COMB)");
      }
      // names: no control repeats TWIN or FILTER inside a titled card
      for (const tab of ["engine", "filter", "envelope", "lfo", "matrix"]) {
        const cs = await cardsOf(tab); const bad = cs.flatMap((x) => x.labs.filter((l) => /TWIN/.test(l) || (/FILTER/.test(l) && l !== "FILTER ENV")).map((l) => x.t + ": " + l));
        ok(!bad.length, `names / ${n} ${tab}: no control repeats TWIN or FILTER inside a titled card (${bad.join("; ") || "none"})`);
      }
      await U(`u.writeParam(0, 43, 0)`);
    }
    // the matrix: CYCLE as a source, the new destinations by the firmware's names, offered by the TWIN TYPE
    await E(`window.__app.unit.setEngine(0, ${await engIdx("PHASE")}).then(() => true)`); await c.sleep(300);
    await U(`u.writeParam(0, 43, 1)`); await E(`window.__app.setPartTab(0, 'matrix'); true`); await c.sleep(300);
    const mt = JSON.parse(await E(`JSON.stringify((() => { const r = window.__app.cols[0].body.querySelector('.mrow:not(.mhead)'); const sel = r.querySelectorAll('select'); const txt = (s) => [...s.options].filter((o) => !o.hidden && !o.disabled).map((o) => o.textContent);
      return { src: txt(sel[0]), dst: txt(sel[1]) }; })())`));
    ok(mt.src.includes("CYCLE") && ["TWIN FILTER CUTOFF", "TWIN FILTER RESONANCE", "FILTER DRIVE", "FILTER BLEND", "SUB LEVEL"].every((x) => mt.dst.includes(x)), `MATRIX: the source CYCLE and the destinations TWIN FILTER CUTOFF, TWIN FILTER RESONANCE, FILTER DRIVE, FILTER BLEND, SUB LEVEL are in the lists (TYPE LP)`);
    await U(`u.writeParam(0, 43, 0)`); await c.sleep(200);
    const mt2 = JSON.parse(await E(`JSON.stringify((() => { const sel = window.__app.cols[0].body.querySelector('.mrow:not(.mhead)').querySelectorAll('select'); return [...sel[1].options].filter((o) => !o.hidden && !o.disabled).map((o) => o.textContent); })())`));
    ok(!mt2.some((x) => /^FILTER (2|DRIVE|BLEND)/.test(x)) && mt2.includes("SUB LEVEL"), "MATRIX: with TYPE OFF the TWIN destinations are not offered (SUB LEVEL is)");
    // the unit follows: a slot turned on the unit shows in its card
    await E(`window.__app.setPartTab(0, 'engine'); true`); await c.sleep(200);
    await E(`window.__app.mock.deviceTurnX(0, 6, 90); true`); await c.sleep(300);
    ok((await E(`[...window.__app.cols[0].body.querySelectorAll('.card')].find((x) => x.querySelector('h3 .ttl')?.textContent === 'SUB OSC').querySelector('.cell:last-child .val').textContent`)) !== "0", "SUB OSC: a LEVEL turned on the unit shows in the card (the CHANGED push, scope 2)");
    {   // a control of an extension slot writes through scope 2, UNDO puts the value back, the card's reset button writes the defaults
      const lvl = `[...window.__app.cols[0].body.querySelectorAll('.card')].find((x) => x.querySelector('h3 .ttl')?.textContent === 'SUB OSC')`;
      await E(`window.__app.mock.parts[0].x[6] = 0; window.__app.unit.parts[0].x[6] = 0; window.__app.refresh(); true`); await c.sleep(150);
      await E(`(() => { const i = ${lvl}.querySelector('input[aria-label="LEVEL"]'); i.value = 80; i.dispatchEvent(new Event('input', { bubbles: true })); return true; })()`);
      await E(`window.__app.unit.idle().then(() => true)`);
      ok((await E(`window.__app.mock.parts[0].x[6]`)) === 80, "SUB OSC: moving LEVEL writes extension slot 6 of the unit (scope 2)");
      await E(`window.__app.doUndo(0).then(() => window.__app.unit.idle()).then(() => true)`); await c.sleep(150);
      ok((await E(`window.__app.mock.parts[0].x[6]`)) === 0, "SUB OSC: UNDO puts the old LEVEL back on the unit");
      await E(`window.__app.mock.parts[0].x[7] = 0; window.__app.mock.parts[0].x[9] = 50; window.__app.unit.readPart(0).then(() => true)`); await c.sleep(200);
      await E(`${lvl}.querySelector('.rst').click(); true`); await c.sleep(400);
      await E(`window.__app.unit.idle().then(() => true)`);
      ok((await E(`JSON.stringify(window.__app.mock.parts[0].x.slice(6, 10))`)) === "[0,1,0,50]", "SUB OSC: the card's reset writes the defaults (WAVE SINE, OCTAVE -1, LEVEL 0); the hidden HARMONICS (disabled, id reserved) is not touched");
    }
    ok((await E(`window.__app.say ? true : false`)) && JSON.stringify(await c.errors?.()).indexOf("exception") < 0, "FILTER tab: no script error while the cards were built");
  }
  // the checks themselves: they must catch what they are for
  await E(`window.__app.setPartTab(0, 'engine'); (() => { const st = document.createElement('style'); st.id = 'selftest'; st.textContent = '.grid { gap: 0 !important } .seg { min-width: min-content !important; flex: 1 1 auto !important }'; document.head.append(st); return true; })()`);
  await c.sleep(300);
  ok(JSON.parse(await E(CHECK)).length > 0, "spacing: the check finds controls that touch (self-test with the gaps removed)");
  await E(`(() => { document.getElementById('selftest').textContent = '.cell .top { flex-wrap: wrap !important } .cell .lab { flex: 1 0 100% !important }'; return true; })()`);
  await c.sleep(300);
  ok(JSON.parse(await E(CHECK)).some((x) => / value pushed under its label/.test(x)), "spacing: the check finds a value pushed under its label (self-test with 0.2.25's wrapping top row)");
  await E(`(() => { document.getElementById('selftest').textContent = '.cell .slider { height: 90px !important }'; return true; })()`);
  await c.sleep(300);
  ok(JSON.parse(await E(FIT)).some((x) => / needs \d+ px, has /.test(x)), "overflow: the check finds a tab that would scroll (self-test with tall sliders)");
  await c.close(); server.close();
}

// ---- the editor2 round (owner 2026-10-21): the MIXER under its title line (A4), the PATCH box order (A5), every box full width, the tab strip, the FM6 operator editor (A7).
// Headless Chrome, both ANALOG and FM6, the three sizes; the report lines are printed, the checks fail the run
async function structure() {
  const { launch, CHROME } = await import("./cdp.mjs");
  if (!CHROME) { console.log("structure: no Chrome found   skip"); return; }
  const http = await import("node:http"), { readFile } = await import("node:fs/promises"), { extname, normalize } = await import("node:path");
  const MIME = { ".js": "text/javascript", ".json": "application/json", ".html": "text/html", ".css": "text/css", ".png": "image/png", ".svg": "image/svg+xml" };
  const server = http.createServer(async (req, res) => { try { let p = decodeURIComponent(new URL(req.url, "http://x").pathname); if (p.endsWith("/")) p += "index.html"; const f = normalize(join(HERE, p)); if (!f.startsWith(HERE)) throw 0; const b = await readFile(f); res.writeHead(200, { "content-type": MIME[extname(f)] || "application/octet-stream" }); res.end(b); } catch { res.writeHead(404); res.end(); } });
  await new Promise((r) => server.listen(0, "127.0.0.1", r));
  // in the page: for every titled card / block of the visible columns: no control above the bottom edge of the title box; the box as wide as its area; the control cells as wide as everyone's
  const CHECK = `(() => {
    const out = { under: [], wide: [], cell: [], checked: [] }, vis = (e) => e.offsetParent && e.getBoundingClientRect().width > 0;
    for (const body of document.querySelectorAll('.colbody')) {
      if (!vis(body)) continue;
      const tab = body.dataset.tab, nm = (body.closest('.col.glob') ? 'GLOBAL ' : 'PART ') + tab, widths = [];
      for (const sec of body.querySelectorAll('section.card, section.block')) {
        if (!vis(sec) || sec.closest('.fm6ed')) continue;
        const ttl = sec.querySelector(':scope > h3 .ttl'), r = sec.getBoundingClientRect();
        if (ttl) {
          out.checked.push(nm + ': ' + ttl.textContent);
          const bottom = ttl.getBoundingClientRect().bottom;
          for (const el of sec.querySelectorAll('.cell, .mrow, .grid, .hgrid, .flow, select, input, .seg')) {
            if (!vis(el) || el.closest('h3')) continue;
            if (el.getBoundingClientRect().top < bottom - 0.5) out.under.push(nm + ': ' + ttl.textContent + ' / ' + (el.className || el.tagName) + ' ' + (el.querySelector?.('.lab')?.textContent ?? ''));
          }
        }
        const par = sec.parentElement;
        if (par && par.matches('.tabbody, .gbody')) { const pc = par.getBoundingClientRect(), ps = getComputedStyle(par); const room = pc.width - parseFloat(ps.paddingLeft) - parseFloat(ps.paddingRight); if (Math.abs(r.width - room) > 1.5) out.wide.push(nm + ': ' + (ttl?.textContent ?? sec.className) + ' ' + Math.round(r.width) + ' of ' + Math.round(room)); }
        if (!sec.classList.contains('five') && !sec.classList.contains('matrix')) for (const cell of sec.querySelectorAll(':scope > .grid > .cell')) if (vis(cell)) widths.push(Math.round(cell.getBoundingClientRect().width));
      }
      if (widths.length && Math.max(...widths) - Math.min(...widths) > 2) out.cell.push(nm + ': control cells ' + Math.min(...widths) + ' .. ' + Math.max(...widths) + ' px');
    }
    return JSON.stringify(out);
  })()`;
  const SIZES = [[1920, 940, "1920 x 1080"], [1440, 780, "1440 x 900"], [1280, 600, "1280 x 720"]];
  for (const [W, H, screen] of SIZES) {
    const c = await launch(); const E = (x) => c.eval(x);
    await c.open(`http://127.0.0.1:${server.address().port}/editor/?mock=1`, W, H);
    await c.until("window.__app && window.__app.unit && window.__app.mock"); await c.sleep(500);
    const engIdx = (n) => E(`[...window.__app.cols[0].engSel.options].find((o) => o.textContent === '${n}').value`);
    const under = new Set(), wide = new Set(), cell = new Set(), checked = new Set();
    for (const n of ["ANALOG", "FM6"]) {
      await E(`Promise.all([window.__app.unit.setEngine(0, ${await engIdx(n)}), window.__app.unit.setEngine(1, ${await engIdx(n)})]).then(() => true)`); await c.sleep(500);
      for (const tab of ["engine", "filter", "envelope", "lfo", "matrix", "fx", "arpseq"]) {
        await E(`window.__app.setPartTab(0, '${tab}'); window.__app.setPartTab(1, '${tab}'); true`); await c.sleep(300);
        const r = JSON.parse(await E(CHECK)); r.under.forEach((x) => under.add(n + ' ' + x)); r.wide.forEach((x) => wide.add(n + ' ' + x)); r.cell.forEach((x) => cell.add(n + ' ' + x)); r.checked.forEach((x) => checked.add(x));
      }
      if (n === "FM6") {   // the OPERATORS button: collapsed by default, one press opens every operator control, the tab scrolls to the last one (owner A7)
        await E(`window.__app.setPartTab(0, 'engine'); true`); await c.sleep(300);
        ok(!(await E(`!!document.querySelector('.col.part .fm6ed')`)) && (await E(`document.querySelector('.col.part .opsbtn')?.getAttribute('aria-expanded')`)) === "false", `OPERATORS ${screen}: the FM6 operator editor is collapsed by default (button OPERATORS, closed)`);
        await E(`document.querySelector('.col.part .opsbtn').click(); true`); await c.sleep(900);
        const o = JSON.parse(await E(`JSON.stringify((() => { const ed = document.querySelector('.col.part .fm6ed'), body = ed.closest('.colbody'), bad = [];
          for (const cl of ed.querySelectorAll('.cell')) { const card = cl.closest('.card').getBoundingClientRect(); for (const x of cl.querySelectorAll('.lab, .right')) { const q = x.getBoundingClientRect(); if (x.scrollWidth > x.clientWidth + 1 || q.right > card.right + 0.5 || q.left < card.left - 0.5) bad.push((cl.querySelector('.lab')?.textContent ?? '?') + ' ' + x.className); } }
          const before = body.scrollTop; body.scrollTop = body.scrollHeight; return { cells: ed.querySelectorAll('.cell').length, bad: [...new Set(bad)], sh: body.scrollHeight, ch: body.clientHeight, moved: body.scrollTop > before, ops: ed.querySelectorAll('.opgrid > .card').length, wide: [...ed.querySelectorAll('.opgrid > .card')].every((x) => Math.abs(x.getBoundingClientRect().width - ed.getBoundingClientRect().width) < 2) }; })())`));
        ok(o.cells === 144 && o.ops === 6 && o.wide, `OPERATORS ${screen}: opened, ${o.cells} controls (21 x 6 operators + 3 + 8 + 7), six OPERATOR cards one per line`);
        ok(o.bad.length === 0, `OPERATORS ${screen}: every label and value inside its frame${o.bad.length ? " " + o.bad.slice(0, 6).join("; ") : ""}`);
        ok(o.sh > o.ch && o.moved, `OPERATORS ${screen}: the tab scrolls to the last control (${o.sh} px of content in ${o.ch})`);
        await E(`document.querySelector('.col.part .opsbtn').click(); true`); await c.sleep(300);
        ok(!(await E(`!!document.querySelector('.col.part .fm6ed')`)), `OPERATORS ${screen}: pressed again it closes`);
      }
    }
    // the GLOBAL column
    const exc = new Set();
    for (const g of ["global", "midi", "settings"]) {
      await E(`window.__app.setGlobalTab('${g}', true); true`); await c.sleep(500); const r = JSON.parse(await E(CHECK)); r.under.forEach((x) => under.add(x)); r.checked.forEach((x) => checked.add(x));
      if (W >= 1500) { r.wide.forEach((x) => wide.add(x)); r.cell.forEach((x) => cell.add(x)); } else r.wide.forEach((x) => exc.add(x));   // (below 1500 px the GLOBAL view shows two cards to a line: Q4)
    }
    if (exc.size) console.log(`  full width ${screen}: the GLOBAL view shows two cards to a line, exceptions (${exc.size}): ${[...exc].slice(0, 4).join("; ")} ...`);
    await E(`window.__app.setView('parts'); true`);
    ok(under.size === 0, `title line ${screen}: no control above the bottom edge of its card's title box (${checked.size} titled cards and blocks checked)${under.size ? "\n    " + [...under].slice(0, 10).join("\n    ") : ""}`);
    ok(wide.size === 0, `full width ${screen}: every card and block has the width of its area${wide.size ? "\n    " + [...wide].slice(0, 10).join("\n    ") : ""}`);
    ok(cell.size === 0, `control width ${screen}: every control cell of a tab is as wide as everyone's${cell.size ? "\n    " + [...cell].slice(0, 10).join("\n    ") : ""}`);
    if (screen === SIZES[0][2]) console.log("  cards checked for the title line: " + [...checked].join(" | "));
    // the PATCH box (owner 2026-10-21): Browse... Init..., PATCH LEVEL, no ENGINE; the ENGINE selector is the ENGINE tab's first element on a full-width line of its own, above the engine's card
    const ord = JSON.parse(await E(`JSON.stringify([...document.querySelector('.col.part .patchblock .hgrid').children].map((x) => x.classList.contains('btncell') ? [...x.querySelectorAll('button')].map((b) => b.textContent).join('+') : x.querySelector('.lab')?.textContent))`));
    ok(ord.join() === "Browse...+Init...,PATCH LEVEL", `PATCH box ${screen}: Browse... Init..., PATCH LEVEL, no ENGINE (${ord.join(" | ")})`);
    await E(`window.__app.setPartTab(0, 'engine'); true`); await c.sleep(300);
    const el = JSON.parse(await E(`(() => { const b = window.__app.cols[0].body.querySelector('.tabbody'), l = b.firstElementChild, s = l.querySelector('select'), r = l.getBoundingClientRect(), br = b.getBoundingClientRect(), n = l.nextElementSibling?.getBoundingClientRect();
      return JSON.stringify({ cls: l.className, sel: s === window.__app.cols[0].engSel && s.getAttribute('aria-label') === 'Part 1 engine', w: Math.round(r.width), bw: Math.round(br.width), gap: n ? Math.round(n.top - r.bottom) : -1, sh: Math.round(s.getBoundingClientRect().height), inPatch: !!document.querySelector('.col.part .patchblock select') }); })()`));
    ok(el.cls === "engline" && el.sel && el.w >= el.bw - 2 && el.gap > 0 && !el.inPatch, `ENGINE tab ${screen}: the ENGINE selector is the first element, full width (${el.w} of ${el.bw} px), ${el.gap} px above the next box, ${el.sh} px high`);
    const sel = 'select[aria-label="Part 1 engine"]', other = await E(`[...window.__app.cols[0].engSel.options].map((o) => +o.value).find((v) => v !== +window.__app.cols[0].engSel.value)`);
    await E(`(() => { const s = document.querySelector('${sel}'); s.value = ${other}; s.dispatchEvent(new Event('change')); return true; })()`); await c.sleep(600);
    ok((await E(`window.__app.unit.parts[0].eng`)) === other, `ENGINE tab ${screen}: the ENGINE selector still changes the engine`);
    // the tab strip: all tabs on one line, ENVELOPE written out
    const st = JSON.parse(await E(`JSON.stringify([...document.querySelectorAll('.col.part .tabs')].map((s) => ({ sw: s.scrollWidth, cw: s.clientWidth, words: [...s.querySelectorAll('[role=tab]')].map((t) => t.textContent), w: [...s.querySelectorAll('[role=tab]')].map((t) => Math.round(t.getBoundingClientRect().width)) })))`));
    ok(st.every((x) => x.sw <= x.cw && x.words.join() === "ENGINE,FILTER,ENVELOPE,LFO,MATRIX,FX,ARP,SEQ"), `tab strip ${screen}: ${st[0].words.length} tabs on one line, ENVELOPE in full (strip ${st[0].sw} of ${st[0].cw} px; ${st[0].words.map((t, i) => t + " " + st[0].w[i]).join(", ")})`);
    await c.close();
  }
  server.close();
}

// ---- the lists (editor2 items 7 and 12, A8): PATTERNS is one list (32 rows, what the unit reports), no part chooser, no "empty", no kind words, no Clear;
// the MULTIS and PATTERNS save buttons name what they replace and nothing is written before the dialog is confirmed (BUG_SAVE_FORM.md, adapted: these lists have no selected row and no
// preset slot, each row has its own buttons and a confirmation)
async function lists() {
  const { launch, CHROME } = await import("./cdp.mjs");
  if (!CHROME) { console.log("lists: no Chrome found   skip"); return; }
  const http = await import("node:http"), { readFile } = await import("node:fs/promises"), { extname, normalize } = await import("node:path");
  const MIME = { ".js": "text/javascript", ".json": "application/json", ".html": "text/html", ".css": "text/css", ".png": "image/png", ".svg": "image/svg+xml" };
  const server = http.createServer(async (req, res) => { try { let p = decodeURIComponent(new URL(req.url, "http://x").pathname); if (p.endsWith("/")) p += "index.html"; const f = normalize(join(HERE, p)); if (!f.startsWith(HERE)) throw 0; const b = await readFile(f); res.writeHead(200, { "content-type": MIME[extname(f)] || "application/octet-stream" }); res.end(b); } catch { res.writeHead(404); res.end(); } });
  await new Promise((r) => server.listen(0, "127.0.0.1", r));
  for (const mode of ["pat32", "1"]) {
    const c = await launch(), E = (x) => c.eval(x);
    await c.open(`http://127.0.0.1:${server.address().port}/editor/?mock=${mode}`, 1920, 940);
    await c.until("window.__app && window.__app.unit && window.__app.mock"); await c.sleep(500);
    await E(`window.__app.setGlobalTab('patterns', true); true`); await c.sleep(2500);
    const rows = JSON.parse(await E(`JSON.stringify([...document.querySelectorAll('.col.glob .mrow2')].map((r) => ({ t: r.textContent, ti: r.title + ' ' + [...r.querySelectorAll('button')].map((b) => b.title).join(' '), b: [...r.querySelectorAll('button')].map((b) => b.textContent) })))`));
    const want = 32;
    ok(rows.length === want, `PATTERNS (?mock=${mode}): ${rows.length} rows, one list for the whole device (want ${want})`);
    ok(!(await E(`!!document.querySelector('.col.glob .multiswrap select')`)) && !rows.some((r) => /empty|factory|saved/i.test(r.t + r.ti)) && !rows.some((r) => r.b.includes("Clear...")), `PATTERNS (?mock=${mode}): no part chooser, no "empty", "factory" or "saved" in a row or tooltip, no Clear button`);
    ok(rows.every((r, i) => r.b[1].startsWith(`Replace P${String(i + 1).padStart(2, "0")} `) && r.b[0] === "Recall into part"), `PATTERNS (?mock=${mode}): every slot holds a pattern; its save button reads Replace P01 NAME ... (${rows[6].b[1]})`);
    // nothing is written before the dialog is confirmed; Cancel leaves the unit as it was
    const row = 6, label = `P${String(row + 1).padStart(2, "0")}`;
    const press = (n) => E(`(() => { const r = document.querySelectorAll('.col.glob .mrow2')[${row}]; [...r.querySelectorAll('button')].find((b) => /^(Replace|Save)/.test(b.textContent)).click(); return true; })()`);
    const w0 = await E(`window.__app.mock.log.filter((x) => /^write 2[1-3]/.test(x)).length`);
    await press(); await c.sleep(300);
    const dlg = await E(`document.querySelector('dialog.ask h2')?.textContent`);
    ok(new RegExp(`^Save PART \\d+'s pattern to ${label}$`).test(dlg ?? ""), `PATTERNS (?mock=${mode}): the save button of ${label} opens its own dialog: "${dlg}"`);
    await E(`[...document.querySelectorAll('dialog.ask button')].find((b) => b.textContent === 'Cancel').click(); true`); await c.sleep(500);
    ok((await E(`window.__app.mock.log.filter((x) => /^write 2[1-3]/.test(x)).length`)) === w0 && !(await E(`!!document.querySelector('dialog.ask')`)), `PATTERNS (?mock=${mode}): Cancel writes nothing`);
    await press(); await c.sleep(300);
    await E(`[...document.querySelectorAll('dialog.ask button')].find((b) => b.textContent === 'Save').click(); true`); await c.sleep(2500);
    ok((await E(`window.__app.mock.log.filter((x) => /^write 2[1-3]/.test(x)).length`)) === w0 + 1, `PATTERNS (?mock=${mode}): confirmed, exactly one object is written`);
    // MULTIS: the same
    await E(`window.__app.setGlobalTab('multis', true); true`); await c.sleep(1500);
    await E(`[...document.querySelectorAll('.col.glob button')].find((b) => b.textContent === 'Load from unit')?.click(); true`); await c.sleep(1500);
    const mr = JSON.parse(await E(`JSON.stringify([...document.querySelectorAll('.col.glob .mrow2')].map((r) => ({ t: r.textContent, ti: r.title, b: [...r.querySelectorAll('button')].map((b) => b.textContent) })))`));
    ok(mr.length === 16 && !mr.some((r) => /factory|saved/i.test(r.t + r.ti)), `MULTIS (?mock=${mode}): ${mr.length} rows, no kind words in a row or its tooltip`);
    const j = mr.findIndex((r) => /^Replace M\d\d /.test(r.b[1]));
    ok(j >= 0, `MULTIS (?mock=${mode}): the save button names what it replaces (${mr[j]?.b[1]})`);
    const wm = await E(`window.__app.mock.log.filter((x) => /^write 1[3-9]|^write 20/.test(x)).length`);
    await E(`[...document.querySelectorAll('.col.glob .mrow2')][${j}].querySelectorAll('button')[1].click(); true`); await c.sleep(300);
    ok(/^Save to unit: M\d\d$/.test(await E(`document.querySelector('dialog.ask h2')?.textContent`) ?? ""), `MULTIS (?mock=${mode}): the button opens its dialog`);
    await E(`[...document.querySelectorAll('dialog.ask button')].find((b) => b.textContent === 'Cancel').click(); true`); await c.sleep(500);
    ok((await E(`window.__app.mock.log.filter((x) => /^write 1[3-9]|^write 20/.test(x)).length`)) === wm, `MULTIS (?mock=${mode}): Cancel writes nothing`);
    if (mode === "pat32") {   // SOLO (item 18): a second click puts the mutes back; a hand mute from before stays
      const st = () => E(`(async () => { const { ID } = await import('./layout.js'); return JSON.stringify(window.__app.unit.parts.slice(0, 2).map((q) => q.p[ID.P_MUTE] ? 1 : 0)); })()`);
      const click = async (sel, p) => { await E(`document.querySelectorAll('.col.part')[${p}].querySelector('${sel}').click(); true`); await c.sleep(250); };
      await E(`window.__app.setView('parts'); true`);
      const seq = [];
      for (const p of [0, 1]) {
        seq.push(await st());                                               // [0,0]
        await click(".solo", p); seq.push(await st()); await click(".solo", p); seq.push(await st());
        await click(".mute:not(.solo)", 1 - p); seq.push(await st());       // the other part muted by hand
        await click(".solo", p); seq.push(await st()); await click(".solo", p); seq.push(await st());
        await click(".mute:not(.solo)", 1 - p);                             // back to nothing muted
      }
      const want = [0, 1].flatMap((p) => [[0, 0], p ? [1, 0] : [0, 1], [0, 0], p ? [0, 1] : [1, 0], p ? [0, 1] : [1, 0], p ? [0, 1] : [1, 0]]).map((x) => JSON.stringify(x));
      // (with the part's own hand mute: solo of p mutes 1-p anyway; off puts 1-p back to its hand mute = muted)
      ok(seq[0] === "[0,0]" && seq[1] === (JSON.stringify([0, 1])) && seq[2] === "[0,0]", `SOLO PART 1: on mutes PART 2, a second click puts it back (${seq.slice(0, 3).join(" ")})`);
      ok(seq[3] === "[0,1]" && seq[4] === "[0,1]" && seq[5] === "[0,1]", `SOLO PART 1 with PART 2 muted by hand: off leaves PART 2 muted, PART 1 is not muted (${seq.slice(3, 6).join(" ")})`);
      ok(seq[6] === "[0,0]" && seq[7] === "[1,0]" && seq[8] === "[0,0]", `SOLO PART 2: on mutes PART 1, a second click puts it back (${seq.slice(6, 9).join(" ")})`);
      ok(seq[9] === "[1,0]" && seq[10] === "[1,0]" && seq[11] === "[1,0]", `SOLO PART 2 with PART 1 muted by hand: off leaves PART 1 muted, PART 2 is not muted (${seq.slice(9, 12).join(" ")})`);
    }
    await c.close();
  }
  server.close();
}

// ---- item 11b (task 9): linked controls on both tabs. The cutoff depths of the filter envelope and the two LFOs (CUTOFF MOD; TWIN MOD for the second filter) are on the FILTER tab and on the
// source's own tab (ENVELOPE / LFO): one stored value, so a change made on one tab is what the other shows
async function linked() {
  const { launch, CHROME } = await import("./cdp.mjs");
  if (!CHROME) { console.log("linked: no Chrome found   skip"); return; }
  const http = await import("node:http"), { readFile } = await import("node:fs/promises"), { extname, normalize } = await import("node:path");
  const MIME = { ".js": "text/javascript", ".json": "application/json", ".html": "text/html", ".css": "text/css", ".png": "image/png", ".svg": "image/svg+xml" };
  const server = http.createServer(async (req, res) => { try { let p = decodeURIComponent(new URL(req.url, "http://x").pathname); if (p.endsWith("/")) p += "index.html"; const f = normalize(join(HERE, p)); if (!f.startsWith(HERE)) throw 0; const b = await readFile(f); res.writeHead(200, { "content-type": MIME[extname(f)] || "application/octet-stream" }); res.end(b); } catch { res.writeHead(404); res.end(); } });
  await new Promise((r) => server.listen(0, "127.0.0.1", r));
  const c = await launch(), E = (x) => c.eval(x);
  await c.open(`http://127.0.0.1:${server.address().port}/editor/?mock=1`, 1920, 940);
  await c.until("window.__app && window.__app.unit && window.__app.mock"); await c.sleep(500);
  const engIdx = (n) => E(`[...window.__app.cols[0].engSel.options].find((o) => o.textContent === '${n}').value`);
  // a cell's text on a tab: the card with this title, the control with this label -> its value text, or null when it is not drawn
  const val = async (tab, card, label) => { await E(`window.__app.setPartTab(0, '${tab}'); true`); await c.sleep(250); return E(`(() => { const s = [...window.__app.cols[0].body.querySelectorAll('section.card')].find((x) => !x.hidden && x.querySelector('h3 .ttl')?.textContent === '${card}'); const cl = s && [...s.querySelectorAll('.cell')].find((x) => !x.hidden && !x.classList.contains('gone') && x.querySelector('.lab')?.textContent === '${label}'); return cl ? cl.querySelector('.right')?.textContent ?? '' : null; })()`); };
  // move the cell's slider the way a hand does (input + change)
  const move = async (tab, card, label, to) => { await E(`window.__app.setPartTab(0, '${tab}'); true`); await c.sleep(250); await E(`(() => { const s = [...window.__app.cols[0].body.querySelectorAll('section.card')].find((x) => x.querySelector('h3 .ttl')?.textContent === '${card}'); const cl = [...s.querySelectorAll('.cell')].find((x) => x.querySelector('.lab')?.textContent === '${label}'); const i = cl.querySelector('input[type=range]'); const lo = +i.min, hi = +i.max; i.value = String(Math.round(lo + (hi - lo) * ${to})); i.dispatchEvent(new Event('input', { bubbles: true })); i.dispatchEvent(new Event('change', { bubbles: true })); return true; })()`); await c.sleep(300); };
  const PAIRS = [["CUTOFF MOD", "FILTER ENV", "envelope"], ["CUTOFF MOD", "LFO 1", "lfo"], ["CUTOFF MOD", "LFO 2", "lfo"], ["TWIN MOD", "FILTER ENV", "envelope"], ["TWIN MOD", "LFO 1", "lfo"], ["TWIN MOD", "LFO 2", "lfo"]];
  for (const n of ["ANALOG", "FM6"]) {
    await E(`window.__app.unit.setEngine(0, ${await engIdx(n)}).then(() => window.__app.unit.writeParam(0, 43, 1)).then(() => window.__app.unit.idle()).then(() => true)`); await c.sleep(500);
    for (const [card, label, tab] of PAIRS) {
      const a0 = await val(tab, card, label), f0 = await val("filter", card, label);
      await move(tab, card, label, 0.8);
      const a1 = await val(tab, card, label), f1 = await val("filter", card, label);
      await move("filter", card, label, 0.3);
      const a2 = await val(tab, card, label), f2 = await val("filter", card, label);
      ok(a0 != null && f0 != null && a0 === f0 && a1 === f1 && a1 !== a0 && a2 === f2 && a2 !== a1, `linked / ${n}: ${card} ${label} on the ${tab.toUpperCase()} tab and on the FILTER tab show one value (${a0} > ${a1} > ${a2})`);
    }
    const t = JSON.parse(await E(`(async () => { const r = {}; for (const tab of ['envelope', 'lfo', 'filter']) { window.__app.setPartTab(0, tab); await new Promise((x) => setTimeout(x, 250)); r[tab] = [...window.__app.cols[0].body.querySelectorAll('section.card')].filter((x) => !x.hidden).map((x) => x.querySelector('h3 .ttl')?.textContent); } return JSON.stringify(r); })()`));
    ok(t.envelope.join() === "AMP ENVELOPE,FILTER ENVELOPE,CYCLE ENV,CUTOFF MOD,TWIN MOD" && t.lfo.join() === "LFO 1,LFO 2,CUTOFF MOD,TWIN MOD" && /CUTOFF MOD/.test(t.filter.join()), `linked / ${n}: cards ENVELOPE ${t.envelope.join(" > ")} | LFO ${t.lfo.join(" > ")}`);
  }
  await c.close(); server.close();
}
