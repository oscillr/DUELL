// SPDX-License-Identifier: GPL-3.0-only
// The patch record's range table (web/editor/patchrecord.js rangeOf, read from layout.json) against the unit's own: web/fixtures/patch_ranges.txt, written by tests/rangedump.c
// from the firmware's param_desc_of. The ranges are part of the stored format (each value is kept in the bits of its range), so every engine and every wide id must agree.
//   node web/test_ranges.mjs
import { readFileSync } from "node:fs";
import { rangeOf, widBits } from "./editor/patchrecord.js";
import { L } from "./editor/layout.js";

let bad = 0, n = 0;
const lines = readFileSync(new URL("./fixtures/patch_ranges.txt", import.meta.url), "utf8").split("\n").filter(Boolean);
if (lines.length !== L.NENGINES) { console.log(`the file has ${lines.length} engines, the editor ${L.NENGINES}`); bad++; }
for (const ln of lines) {
  const [e, rest] = ln.split(":"), cells = rest.split(";").map((c) => c.split(",").map(Number));
  if (cells.length !== 156) { console.log(`engine ${e}: ${cells.length} values, not 156`); bad++; continue; }
  cells.forEach(([min, max], w) => {
    const r = rangeOf(+e, w); n++;
    if (r.min !== min || r.max !== max) { bad++; if (bad < 20) console.log(`engine ${e} wide id ${w}: the unit ${min}..${max}, the editor ${r.min}..${r.max}`); }
    if (widBits(+e, w) !== (max === min ? 0 : Math.ceil(Math.log2(max - min + 1)))) { bad++; console.log(`engine ${e} wide id ${w}: bits`); }
  });
}
console.log(`range table: ${n} ranges compared with the unit's ${bad ? "FAIL" : "ok"}`);
process.exit(bad ? 1 : 0);
