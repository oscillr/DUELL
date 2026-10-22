// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 OSCILL/R (DUELL)
// The short test: start the WebAssembly build in Node, hold one key, render, and look at the sound.
//   node web/emu/note_test.mjs [build/emu/felucca.wasm]
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
let peak = 0, frames = 0;
const render = (ms) => {
  for (let n = Math.round(ms * 44.1); n > 0; n -= 128) {
    const m = Math.min(128, n);
    ex.web_render(m);
    const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), m);
    for (const x of l) peak = Math.max(peak, Math.abs(x));
    frames += m;
  }
};
render(2200);                                  // past the splash
const quiet = peak;
ex.web_keys(1 << 7);                           // one key down
render(500);
const ok = quiet === 0 && peak > 0.01 && peak <= 1;
console.log(`note: silent before the key (${quiet}), peak ${peak.toFixed(3)} with it, ${frames} frames: ${ok ? "ok" : "FAIL"}`);
process.exit(ok ? 0 : 1);
