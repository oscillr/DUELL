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
if (!existsSync(dj)) { console.log("layout vs desc.json: no build/host/desc.json (run host_tests.sh editorjson)   skip"); process.exit(0); }
const D = JSON.parse(readFileSync(dj, "utf8"));
const same = (a, b) => a.label === b.label && a.fmt === b.fmt && a.min === b.min && a.max === b.max && a.def === b.def && JSON.stringify(a.names) === JSON.stringify(b.names) && (a.unit || "") === (b.unit || "");
const diff = (xs, ys) => xs.map((x, i) => (same(x, ys[i]) ? null : `${i}:${x.label}`)).filter(Boolean);
ok(L.P_COUNT === D.P_COUNT && L.G_COUNT === D.G_COUNT && L.P_E0 === D.P_E0 && L.NSTEP === D.NSTEP, "counts: P_COUNT, G_COUNT, P_E0, NSTEP equal descdump's");
{ const d = diff(L.TP, D.TP); ok(d.length === 0, `track parameters: all ${D.P_E0} descriptors equal descdump's ${d.length ? "(differ: " + d.join(" ") + ")" : ""}`); }
{ const d = diff(L.GP, D.GP); ok(d.length === 0, `global parameters: all ${D.G_COUNT} descriptors equal descdump's ${d.length ? "(differ: " + d.join(" ") + ")" : ""}`); }
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
  const bad = L.PAGES.filter((pg) => pg.ids.some((id) => id >= 0 && pg.scope !== "STEP" && pg.scope !== "TRK" && id >= (pg.scope === "GLOBAL" ? L.G_COUNT + 1 : L.P_COUNT) && pg.graph !== "TRK" && pg.graph !== "ROLL")).map((p) => p.title);
  ok(bad.length === 0, `pages: every parameter id of PAGES[] is inside its table ${bad.join(" ")}`);
}
{ const mp = L.PAGES.find((p) => p.ids[0] === L.ID.G_BM), d = L.MASTER;   // 0.2.19: BASS MONO, the pseudo id past the stored globals (core.h G_BM), the card the editor shows from MENU setting 33
  ok(mp && mp.fam === "GLO" && mp.scope === "GLOBAL" && mp.ids[0] === L.ID.G_BM && L.ID.G_BM === L.G_COUNT && mp.ids.slice(1).every((i) => i < 0) && d.label === "BMONO" && d.min === 0 && d.max === 11 && d.def === 0 && d.fk >= 0 && L.FMTS[d.fk][0][0] === "OFF" && L.FMTS[d.fk][1].join("|") === "40|Hz" && L.FMTS[d.fk][5].join("|") === "100|Hz" && L.FMTS[d.fk][11].join("|") === "400|Hz" && L.GP.length === L.G_COUNT,
    "pages: the master-stage page (title BASS MONO; GLO, GLOBAL, K1 BMONO = id G_COUNT: OFF, then 40 .. 400 Hz) and its descriptor; GP keeps the stored globals only"); }
ok(L.FMTS.every((t) => t.every((x) => Array.isArray(x) && typeof x[0] === "string" && typeof x[1] === "string")) && L.FMTS.length > 20, `value text: ${L.FMTS.length} formatted value tables, every entry a [text, unit] pair`);
console.log(fails ? `\nLAYOUT TESTS FAILED (${fails})` : "\nlayout tests passed");
process.exit(fails ? 1 : 0);
