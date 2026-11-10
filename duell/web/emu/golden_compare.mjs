// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 OSCILL/R (DUELL)
// The golden hashes of the repo (tests/golden.txt, tests/multi_render_golden.txt) against the WebAssembly build of the same render code
// (golden_wasm.c, multi_wasm.c; see golden_compare.sh). Every render in a fresh instance. A render is "equal" when the 64-bit FNV-1a hash of
// its output samples is the golden one, which is every sample bit for bit.
//   node web/emu/golden_compare.mjs DIR GOLDEN MULTI_GOLDEN [FILTER]
import fs from "fs";
import { WASI } from "node:wasi";
const [dir, goldenPath, multiPath, filter = ""] = process.argv.slice(2);
const hex = (lo, hi) => (BigInt(hi >>> 0) << 32n | BigInt(lo >>> 0)).toString(16).padStart(16, "0");
const read = (p) => Object.fromEntries(fs.readFileSync(p, "utf8").split("\n").filter((l) => l && l[0] !== "#").map((l) => {
  const t = l.split(/\s+/);
  return t.length === 2 ? [t[0], t[1]] : [`${t[0]} ${t[1]}`, t[2]];
}));
let diffs = 0, total = 0;
async function section(file, golden, jobs) {
  const mod = await WebAssembly.compile(fs.readFileSync(`${dir}/${file}.wasm`));
  const fresh = async () => {                   // (these test builds use libc: stdio, getenv; Node's WASI gives them)
    const wasi = new WASI({ version: "preview1", args: [], env: {}, });
    const inst = await WebAssembly.instantiate(mod, { ...wasi.getImportObject(), env: { emscripten_notify_memory_growth() {} } });
    wasi.initialize(inst);
    return inst.exports;
  };
  const cstr = (ex, p) => { const u = new Uint8Array(ex.memory.buffer); let e = p; while (u[e]) e++; return Buffer.from(u.subarray(p, e)).toString(); };
  return { fresh, cstr, golden, jobs };
}
// regress.c
{
  const g = read(goldenPath), s = await section("golden", g);
  const probe = await s.fresh(), n = probe.g_count();
  let ok = 0, bad = 0, missing = 0;
  for (let i = 0; i < n; i++) {
    const name = s.cstr(probe, probe.g_name(i));
    if (filter && !name.includes(filter)) continue;
    const ex = await s.fresh(); ex.g_count();
    ex.g_run(i);
    const h = hex(ex.g_hash_lo(), ex.g_hash_hi()), want = g[name];
    total++;
    if (!want) { missing++; console.log(`golden: ${name}: no golden entry (${h})`); }
    else if (h === want) ok++;
    else { bad++; diffs++; console.log(`golden: ${name}: DIFFERENT ${h} (golden ${want})`); }
  }
  console.log(`golden: tests/golden.txt: ${ok} equal, ${bad} different, ${missing} without entry, of ${ok + bad + missing} renders in the wasm build`);
}
// multi_render_test.c
{
  const g = read(multiPath), s = await section("multi", g);
  const probe = await s.fresh(), n = probe.m_count(), FAD = [0, 63, 127];
  let ok = 0, bad = 0;
  for (let i = 0; i < n; i++) {
    if (probe.m_scene(i)) continue;                 // (the test scenes need files; see multi_wasm.c)
    const name = s.cstr(probe, probe.m_name(i));
    for (let f = 0; f < 3; f++) {
      const key = `${name} f${FAD[f]}`;
      if (filter && !key.includes(filter)) continue;
      const ex = await s.fresh();
      ex.m_run(i, f);
      const h = hex(ex.m_hash_lo(), ex.m_hash_hi()), want = g[key];
      total++;
      if (h === want) ok++;
      else { bad++; diffs++; console.log(`multi: ${key}: DIFFERENT ${h} (golden ${want})`); }
    }
  }
  console.log(`multi: tests/multi_render_golden.txt: ${ok} equal, ${bad} different, of ${ok + bad} renders`);
}
console.log(`golden compare: ${total} renders, ${diffs} different`);
process.exit(diffs ? 1 : 0);
