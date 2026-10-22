// SPDX-License-Identifier: GPL-3.0-only
// DUELL 0.2.24 (owner: the ENV 2 / LFO 1 / LFO 2 -> CUTOFF depth sliders of MULTI FILTER 2/2 could not be dragged). Headless Chrome drives /editor/?mock=1 and DRAGS EVERY
// slider of the editor with real mouse events: every tab of part 1 and of the global column, every visible <input type=range> (the relabelled depth cards included). A drag
// must land near its target (a control rebuilt mid-drag loses the pointer after the first move) and the simulated unit must receive exactly that parameter. Also: a bipolar
// slider (PAN, a depth, WIDTH) draws its fill from its zero point, a plain one from the left. Skipped (exit 0) when no Chrome is installed (CHROME=/path overrides).
//   node web/test_sliders.mjs      (use the emsdk node)
import http from "node:http";
import { readFile } from "node:fs/promises";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { launch, CHROME } from "./cdp.mjs";

if (!CHROME) { console.log("no Chrome found: slider test skipped"); process.exit(0); }
let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(112)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const ROOT = fileURLToPath(new URL(".", import.meta.url));
const MIME = { ".js": "text/javascript", ".mjs": "text/javascript", ".json": "application/json", ".html": "text/html", ".css": "text/css", ".png": "image/png", ".svg": "image/svg+xml", ".ttf": "font/ttf", ".wav": "audio/wav" };
const server = http.createServer(async (req, res) => {
  try { let p = decodeURIComponent(new URL(req.url, "http://x").pathname); if (p.endsWith("/")) p += "index.html"; const f = normalize(join(ROOT, p)); if (!f.startsWith(ROOT)) throw new Error("outside");
    const b = await readFile(f); res.writeHead(200, { "content-type": MIME[extname(f)] || "application/octet-stream" }); res.end(b); } catch { res.writeHead(404); res.end("not found"); }
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const c = await launch();
const E = (x) => c.eval(x);
const idle = () => E("(async () => { await window.__app.unit.idle(); return true; })()");
const snap = () => E("JSON.stringify({ p: window.__app.mock.parts.map((q) => q.p), x: window.__app.mock.parts.map((q) => q.x), g: window.__app.mock.g, m: window.__app.mock.menu.map((x) => x[6]) })").then(JSON.parse);
const changed = (a, b) => {
  const out = [];
  for (let k = 0; k < 2; k++) a.p[k].forEach((v, i) => { if (v !== b.p[k][i]) out.push(`part ${k + 1} id ${i}`); });
  for (let k = 0; k < 2; k++) a.x[k].forEach((v, i) => { if (v !== b.x[k][i]) out.push(`part ${k + 1} extension slot ${i}`); });   // (cloud job I: SUB OSC, CYCLE ENV and the TWIN cards' slots)
  a.g.forEach((v, i) => { if (v !== b.g[i]) out.push(`global ${i}`); });
  a.m.forEach((v, i) => { if (v !== b.m[i]) out.push(`menu ${i}`); });
  return out;
};
// the visible sliders of column col (0 part 1, 2 the global one): their label, value, range and the thumb's track in page coordinates
const sliders = (col) => E(`JSON.stringify((() => {
  const column = document.querySelectorAll('.col')[${col}], out = [];
  for (const el of column.querySelectorAll('.cell input[type=range]')) {
    const cell = el.closest('.cell');
    if (!el.offsetParent || cell.classList.contains('gone') || el.disabled) continue;
    const r = el.getBoundingClientRect(), lab = cell.querySelector('.lab')?.textContent || '?', card = cell.closest('section')?.querySelector('h3')?.firstChild?.textContent?.trim() || '';
    const fill = cell.querySelector('.fill'), bip = !!cell.querySelector('.slider.bipolar');
    out.push({ lab, card, min: +el.min, max: +el.max, v: +el.value, x0: r.left + 6, x1: r.right - 6, y: r.top + r.height / 2, bip, fill: fill ? { left: fill.style.left, width: fill.style.width } : null });
  }
  return out;
})())`).then(JSON.parse);
const tabsOf = (col) => E(`JSON.stringify([...document.querySelectorAll('.col')[${col}].querySelectorAll('.tab')].map((b) => b.textContent))`).then(JSON.parse);
const clickTab = (col, name) => E(`(() => { const b = [...document.querySelectorAll('.col')[${col}].querySelectorAll('.tab')].find((x) => x.textContent === ${JSON.stringify(name)}); b.click(); return !!b; })()`);

await c.open(`http://127.0.0.1:${server.address().port}/editor/?mock=1`, 1920, 1080);
await c.until("!!window.__app && !!window.__app.unit && !!window.__app.mock", 15000);
await idle();
// part 1 on PHASE: an engine with the multimode filter (ufilt 1), so MULTI FILTER 2/2 shows the ENV 2 / LFO 1 / LFO 2 depth sliders (on ANALOG the routes drive its own filter: hidden there, as on the unit)
await E("(async () => { const u = window.__app.unit; const list = u.info.engines || []; const e = list.indexOf('PHASE'); if (e >= 0 && u.parts[0].eng !== e) await u.setEngine(0, e); return true; })()");
await idle(); await c.sleep(100);
await E("(async () => { window.__app.unit.writeParam(0, 43, 1); await window.__app.unit.idle(); return true; })()");   // (cloud job I: the second filter on (LP), so TWIN FILTER, TWIN MOD and TWIN CHAIN are drawn and their sliders dragged)
await idle(); await c.sleep(100);
let total = 0, dragged = 0, bipolarSeen = 0, plainSeen = 0;
const broken = [];
for (const col of [0, 2]) {
  const tabs = (await tabsOf(col)).filter((t) => !["SEQ", "PATCHES", "MULTIS", "PATTERNS", "WAVETABLES"].includes(t));   // (no parameter sliders there)
  for (const tab of tabs) {
    await clickTab(col, tab); await idle(); await c.sleep(60);
    const list = await sliders(col);
    for (let si = 0; si < list.length; si++) {
      // 0.2.25: a column can be taller than the window (the GLOBAL tab with the MASTER cards): the slider is scrolled into view first, then measured again
      await E(`(() => { const all = [...document.querySelectorAll('.col')[${col}].querySelectorAll('.cell input[type=range]')].filter((el) => el.offsetParent && !el.closest('.cell').classList.contains('gone') && !el.disabled); all[${si}]?.scrollIntoView({ block: 'center' }); return true; })()`);
      await c.sleep(30);
      const s = (await sliders(col))[si] ?? list[si];
      if (s.max <= s.min) continue;
      total++;
      const span = s.max - s.min, width = s.x1 - s.x0;
      const tx = (v) => s.x0 + (v - s.min) / span * width;
      const target = s.v - s.min < span / 2 ? s.min + Math.round(span * 0.85) : s.min + Math.round(span * 0.15);   // the far side of the range: a long drag
      const before = await snap();
      await c.drag(tx(s.v), s.y, tx(target), s.y);
      await idle(); await c.sleep(40);
      const after = await snap(), diff = changed(before, after);
      const now = (await sliders(col)).filter((q) => q.lab === s.lab).sort((a, b) => Math.abs(a.x0 - s.x0) + Math.abs(a.y - s.y) - Math.abs(b.x0 - s.x0) - Math.abs(b.y - s.y))[0];   /* (by label, the nearest: a card's heading may appear after a late read, a body may be drawn again) */
      const landed = now && Math.abs(now.v - target) <= Math.max(2, span * 0.08);
      if (landed && diff.length >= 1) dragged++;
      else broken.push(`${tab} / ${s.card} / ${s.lab}: value ${s.v} -> ${now ? now.v : "?"} (target ${target}), unit changed [${diff.join(", ")}]`);
      // the fill: from the left (plain) or from the zero point (bipolar), as drawn after the drag
      if (now && now.fill) {
        const left = parseFloat(now.fill.left), w = parseFloat(now.fill.width), zero = s.min < 0 && s.max > 0 ? 0 : null;
        if (now.bip !== (s.min < 0 && s.max > 0)) broken.push(`${tab} / ${s.card} / ${s.lab}: range ${s.min}..${s.max} is drawn ${now.bip ? "from the centre" : "from the left"} (the centre fill is only for a range that goes below zero)`);
        if (now.bip) {
          bipolarSeen++;
          const z = zero ?? 0, lo = Math.min(now.v, z), hi = Math.max(now.v, z);
          if (Math.abs(left - (lo - s.min) / span * 100) > 1.5 || Math.abs(w - (hi - lo) / span * 100) > 1.5) broken.push(`${tab} / ${s.card} / ${s.lab}: bipolar fill left ${left} width ${w} for value ${now.v} (zero ${z})`);
        } else {
          plainSeen++;
          if (Math.abs(left) > 0.5 || Math.abs(w - (now.v - s.min) / span * 100) > 1.5) broken.push(`${tab} / ${s.card} / ${s.lab}: fill left ${left} width ${w} for value ${now.v}`);
        }
      }
      // back to where it was (so the hide rules of the other cards stay as the mock starts)
      if (col === 0) {
        const i = before.p[0].findIndex((v, k) => v !== after.p[0][k]);
        if (i >= 0) await E(`(async () => { window.__app.unit.writeParam(0, ${i}, ${before.p[0][i]}); return true; })()`).catch(() => {});
        const xi = before.x[0].findIndex((v, k) => v !== after.x[0][k]);
        if (xi >= 0) await E(`(async () => { window.__app.unit.writeX(0, ${xi}, ${before.x[0][xi]}); return true; })()`).catch(() => {});
      }
      await idle();
    }
  }
}
for (const b of broken) console.log("  broken: " + b);
ok(total > 60, `sliders: ${total} sliders found over the tabs of part 1 and the global column`);
ok(broken.length === 0, `sliders: every slider can be dragged with the mouse and reaches its target; the simulated unit receives the parameter (${dragged} of ${total} fine)`);
ok(bipolarSeen >= 8 && plainSeen >= 20, `sliders: the fill bar follows the value: ${bipolarSeen} bipolar ones from their zero point, ${plainSeen} plain ones from the left`);
{   // the depth cards of MULTI FILTER 2/2 (the owner's broken ones) are among the dragged, under their own labels (cloud job I: on the FILTER tab)
  await clickTab(0, "FILTER"); await idle(); await c.sleep(100);
  const list = await sliders(0), labs = list.map((s) => s.lab);
  const want = ["FILTER ENV", "KEY TRACK", "LFO 1", "LFO 2"], cm = list.filter((s) => s.card === "CUTOFF MOD").map((s) => s.lab);   // (T0229: the CUTOFF MOD card, owner 0b)
  ok(want.join() === cm.join(), `sliders: CUTOFF MOD (part 1 on PHASE) shows FILTER ENV, KEY TRACK, LFO 1, LFO 2, all dragged above (${cm.join(", ")})`);
}
{   // T0227 (owner): the engine drop-down changes the engine at once (no dialog), one undo step puts it back; the ENGINE tab has ONE card of the engine's own controls
  await clickTab(0, "ENGINE"); await idle(); await c.sleep(100);
  const r = JSON.parse(await E(`(async () => { const app = window.__app, u = app.unit, sel = window.__app.cols[0].engSel, list = u.info.engines || [];
    const was = u.parts[0].eng, to = list.indexOf('WAVE');
    sel.value = String(to); sel.dispatchEvent(new Event('change'));
    for (let i = 0; i < 100 && u.parts[0].eng !== to; i++) await new Promise((r) => setTimeout(r, 20));
    const dialog = !!document.querySelector('dialog[open]');
    await new Promise((r) => setTimeout(r, 200));
    const cards = [...document.querySelectorAll('.col')[0].querySelectorAll('.colbody section h3')].map((h) => h.firstChild?.textContent?.trim() || '').filter((t) => /^WAVE/.test(t));
    const changed = u.parts[0].eng === to;
    await app.doUndo(0);
    return JSON.stringify({ dialog, changed, cards, back: u.parts[0].eng === was });
  })()`));
  ok(r.changed && !r.dialog, `editor: the engine drop-down changes the engine at once, no dialog (${JSON.stringify(r)})`);
  ok(r.cards.length === 1 && r.cards[0] === "WAVETABLE", `editor: ONE card of the engine's own controls, titled WAVETABLE once (no engine prefix, no NOW PLAYING card) (${r.cards.join(" / ")})`);
  ok(r.back, "editor: one undo puts the engine back");
}
const errs = c.errors().filter((e) => !/favicon|"source":"network"/.test(JSON.stringify(e)));   /* (a missing favicon is no page error) */
for (const e of errs) console.log("  page error: " + String(JSON.stringify(e.params?.exceptionDetails?.exception?.description || e.params?.args?.map((a) => a.value) || e.params || e)).slice(0, 300));
ok(errs.length === 0, `sliders: no page errors (${errs.length})`);
await c.close();
server.close();
console.log(fails ? `\nSLIDER TESTS FAILED (${fails})` : "\nslider tests passed");
process.exit(fails ? 1 : 0);
