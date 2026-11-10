// SPDX-License-Identifier: GPL-3.0-only
// The engine is PHYSICAL wherever the editor shows its name (owner 2026-11-07: never PHYS on a screen). The stored key stays PHYS (layout.json, the unit's
// engine list, patch files): only what a person reads is checked.
//   node web/test_physical_name.mjs [SITE]      SITE: a published package's site/ folder (default: web/, the source)
// Checks:
//   1 labels.js engineShown turns PHYS into PHYSICAL and leaves the other engines as they are
//   2 the librarian's slot list (describeSlot) shows PHYSICAL for a slot that holds a PHYS sound
//   3 the backup page's "Saved patches" list passes the engine name through engineShown
//   4 no text a person reads in the editor's pages and scripts says PHYS on its own (a quoted "PHYS" that is a key, a comparison or a map entry is allowed)
import { readFileSync, readdirSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const site = resolve(process.argv[2] || new URL(".", import.meta.url).pathname);
const ed = join(site, "editor");
let bad = 0, n = 0;
const check = (what, ok, more = "") => { n++; if (!ok) { bad++; console.log(`physical: FAIL ${what}${more ? ": " + more : ""}`); } };

const { engineShown } = await import(pathToFileURL(join(ed, "labels.js")).href);
const { L } = await import(pathToFileURL(join(ed, "layout.js")).href);
const { describeSlot } = await import(pathToFileURL(join(ed, "librarian.js")).href);

// 1
check("engineShown('PHYS') is PHYSICAL", engineShown("PHYS") === "PHYSICAL", engineShown("PHYS"));
for (const e of L.ENG.map((x) => x.name).filter((x) => x && x !== "-" && x !== "PHYS"))
  check(`engineShown('${e}') stays ${e}`, engineShown(e) === e, engineShown(e));
// 2
const phys = L.ENG.findIndex((e) => e.name === "PHYS");
check("the engine list has PHYS (the stored key)", phys >= 0);
const unit = { slots: [{ saved: true, name: "TEST", eng: phys, cat: 0 }] };
check("librarian slot list shows PHYSICAL", describeSlot(unit, 0).engine === "PHYSICAL", describeSlot(unit, 0).engine);
// 3
const bk = readFileSync(join(ed, "backup.js"), "utf8");
check("backup page: Saved patches list uses engineShown", /add\("Saved patches",[^\n]*engineShown\(p\.engineName\)/.test(bk));
// 4: every string literal or HTML text with PHYS not followed by ICAL
const files = [];
const walk = (d) => { for (const f of readdirSync(d)) { const p = join(d, f); if (statSync(p).isDirectory()) walk(p); else if (/\.(js|mjs|html)$/.test(f)) files.push(p); } };
walk(ed);
const allowed = (line) =>
  /PHYS\s*:/.test(line) ||                          // a map key (PHYS: "PHYSICAL")
  /===?\s*["']PHYS["']|["']PHYS["']\s*===?/.test(line) ||   // a comparison with the stored key
  /^\s*(\/\/|\*|\/\*)/.test(line) ||                // a comment
  /name\s*===?\s*["']PHYS["']/.test(line) ||
  /\[[^\]]*["']PHYS["'][^\]]*\]\.includes\(/.test(line);   // a list of stored keys the code compares with
for (const f of files) {
  const lines = readFileSync(f, "utf8").split("\n");
  lines.forEach((line, i) => {
    const code = line.replace(/\/\/.*$/, "");
    if (!/["'`>][^"'`<]*\bPHYS\b(?!ICAL)[^"'`<]*["'`<]/.test(code) || allowed(line)) return;
    check(`no shown PHYS in ${f.slice(site.length + 1)}:${i + 1}`, false, line.trim().slice(0, 140));
  });
}
console.log(`physical: ${n - bad} of ${n} checks pass${bad ? "" : " (PHYSICAL everywhere the editor shows the engine)"}`);
process.exit(bad ? 1 : 0);
