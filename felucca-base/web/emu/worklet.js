// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Felucca in an AudioWorklet: felucca.wasm is the whole FM-1 firmware against a simulated FM-1
// (web/emu/felucca_web.c). Each render quantum asks it for 128 frames, which runs the device's clock forward;
// between quanta the page's input goes in and, about 30 times a second (4 when the page is hidden), the screen,
// the lights and any changed flash go out. After X0X's web/emu/worklet.js (charlesvestal/fm1-x0x, GPL-3.0).

// The flash the page keeps: storage.c's areas (projects, user presets and their FM6 patches, the user sample
// slots 0x97000..0xDFFFF, DUELL's multis (0x97000..0x9EFFF, 0xCE000..0xD5FFF) and pattern slots (0xD6000..0xDBFFF) among them; the autosave 0xE5000..0xE6FFF (1.2)
// and FM6's voice bank 0xE7000 (1.4.1); the settings and the second FM6 copy 0xFC000..0xFFFFF), only the sectors that
// hold something (an erased sector is all 0xFF). {offset: bytes}
export const STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE8000], [0xFC000, 0x100000]];
export function storeSectors(nor) {
  const out = {};
  for (const [a, b] of STORE_AREAS)
    for (let off = a; off < b; off += 4096) {
      const s = nor.subarray(off, off + 4096);
      if (s.some((v) => v !== 0xFF)) out[off] = s.slice();
    }
  return out;
}
export function restoreSectors(nor, sectors) {
  for (const [k, v] of Object.entries(sectors || {})) {
    const off = +k, b = v instanceof Uint8Array ? v : new Uint8Array(v);
    if (STORE_AREAS.some(([a, e]) => off >= a && off + 4096 <= e && off % 4096 === 0) && b.length === 4096)
      nor.set(b, off);
  }
}

// The saved-data file (the page's Download / Load buttons): "DUELLFLASH1", u16 range count, per range u32 address + u32
// length (little-endian, runs of neighbouring sectors joined), then the ranges' bytes in the same order.
const FILE_MAGIC = "DUELLFLASH1";
export function packFlash(sectors) {
  const offs = Object.keys(sectors).map(Number).sort((a, b) => a - b), ranges = [];
  for (const off of offs) {
    const last = ranges[ranges.length - 1];
    if (last && last.addr + last.parts.length * 4096 === off) last.parts.push(sectors[off]);
    else ranges.push({ addr: off, parts: [sectors[off]] });
  }
  const head = FILE_MAGIC.length + 2 + 8 * ranges.length;
  const out = new Uint8Array(head + offs.length * 4096), dv = new DataView(out.buffer);
  for (let i = 0; i < FILE_MAGIC.length; i++) out[i] = FILE_MAGIC.charCodeAt(i);
  dv.setUint16(FILE_MAGIC.length, ranges.length, true);
  let at = head;
  ranges.forEach((r, i) => {
    dv.setUint32(FILE_MAGIC.length + 2 + 8 * i, r.addr, true);
    dv.setUint32(FILE_MAGIC.length + 6 + 8 * i, r.parts.length * 4096, true);
    for (const p of r.parts) { out.set(p, at); at += 4096; }
  });
  return out;
}
export function unpackFlash(bytes) {                  // -> {offset: 4096 bytes}; throws on anything not a whole, valid file
  const u8 = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes), m = FILE_MAGIC.length;
  if (u8.length < m + 2 || String.fromCharCode(...u8.subarray(0, m)) !== FILE_MAGIC) throw new Error("not a DUELL saved-data file");
  const dv = new DataView(u8.buffer, u8.byteOffset, u8.length), n = dv.getUint16(m, true);
  let at = m + 2 + 8 * n;
  if (at > u8.length) throw new Error("the file is cut short");
  const out = {};
  for (let i = 0; i < n; i++) {
    const addr = dv.getUint32(m + 2 + 8 * i, true), len = dv.getUint32(m + 6 + 8 * i, true);
    if (addr % 4096 || len % 4096 || !len || at + len > u8.length) throw new Error("the file is damaged");
    if (!STORE_AREAS.some(([a, e]) => addr >= a && addr + len <= e)) throw new Error("the file holds addresses this emulator does not keep");
    for (let o = 0; o < len; o += 4096, at += 4096) out[addr + o] = u8.slice(at, at + 4096);
  }
  if (at !== u8.length) throw new Error("the file is damaged (extra bytes)");
  return out;
}

export const HOLD_MS = 24;

if (typeof registerProcessor === "function") {
  const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();
  class Felucca extends AudioWorkletProcessor {
    constructor() {
      super();
      this.ex = null;
      this.sentDraws = -1;
      this.sentWrites = -1;
      this.sentLeds = "";
      this.lastFrame = 0;
      this.period = 1 / 30;
      this.busy = 0;
      this.frames = 0;
      this.queue = [];                                // button / key changes, each held >= HOLD_MS of device time
      this.nextOk = 0;
      this.port.onmessage = (e) => this.onMessage(e.data);
    }

    async onMessage(m) {
      if (m.type === "load") {
        try {
          const { instance } = await WebAssembly.instantiate(m.wasm, {});
          const ex = instance.exports;
          ex._initialize();
          ex.web_nor_erase();
          restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), m.sectors);
          ex.web_master(m.master ?? 700);
          ex.web_boot();
          this.ex = ex;
          this.sentWrites = ex.web_flash_writes_count();
          this.port.postMessage({ type: "ready", rate: ex.web_sample_rate() });
        } catch (err) {
          this.port.postMessage({ type: "error", message: String(err && err.message || err) });
        }
        return;
      }
      const ex = this.ex;
      if (!ex) return;
      if (m.type === "buttons" || m.type === "keys") this.queue.push(m);
      else if (m.type === "enc") ex.web_enc(m.role, m.n | 0);
      else if (m.type === "master") ex.web_master(m.value | 0);
      else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
      else if (m.type === "visible") this.period = m.on ? 1 / 30 : 1 / 4;
    }

    publish() {
      const ex = this.ex, mem = ex.memory.buffer;
      const msg = { type: "frame" };
      const leds = [ex.web_lit_buttons(), ex.web_lit_keys(), ex.web_dim_buttons(), ex.web_dim_keys(), ex.web_dim_level(),
                    ex.web_breath_buttons(), ex.web_breath_keys(), ex.web_mid_keys ? ex.web_mid_keys() : 0, ex.web_hi_keys ? ex.web_hi_keys() : 0,
                    ex.web_mid_buttons ? ex.web_mid_buttons() : 0, ex.web_scl_keys ? ex.web_scl_keys() : 0,
                    ex.web_root_keys ? ex.web_root_keys() : 0, ex.web_kout_keys ? ex.web_kout_keys() : 0,
                    ex.web_top_keys ? ex.web_top_keys() : 0, ex.web_key_levels ? ex.web_key_levels() >>> 0 : 0,
                    ex.web_key_low ? ex.web_key_low() : 0, ex.web_pulse_ms ? ex.web_pulse_ms() : 0];
      const an = ex.web_anim_levels ? ex.web_anim_levels() : 0;   // the power-on sweep: each LED's level
      const anim = an ? Array.from(new Uint8Array(mem, an, 41)) : null;
      const key = leds.join(",") + (anim ? ";" + anim.join(",") : "");
      if (key !== this.sentLeds) {
        this.sentLeds = key;
        msg.leds = leds;
        msg.anim = anim;
      }
      if (this.frames >= 44100) {                     // the share of real time spent running the device
        msg.load = this.busy / (this.frames / 44.1);
        this.busy = this.frames = 0;
      }
      const transfer = [];
      const draws = ex.web_screen_draws();
      if (draws !== this.sentDraws) {
        this.sentDraws = draws;
        msg.fb = new Uint16Array(mem, ex.web_screen(), 240 * 240).slice();
        transfer.push(msg.fb.buffer);
      }
      const writes = ex.web_flash_writes_count();
      if (writes !== this.sentWrites) {               // a save is done within one main-loop pass: whole here
        this.sentWrites = writes;
        msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
      }
      if (ex.web_reboot_requested && ex.web_reboot_requested()) {   // ERASE DATA: the device restarts; the page reloads (index.html)
        msg.reboot = true;
        msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
      }
      if (msg.leds || msg.fb || msg.sectors || msg.reboot || msg.load !== undefined) this.port.postMessage(msg, transfer);
    }

    // A click's down and up can arrive within one render quantum, but the FM-1's contacts never close for less than
    // its debounce, and the UI reads them once a frame: each change of the buttons or the keys stays HOLD_MS of the
    // device's time before the next one goes in (a tap: down for a frame and more, then up).
    input() {
      const ex = this.ex, now = ex.web_now_ms();
      while (this.queue.length && now - this.nextOk >= 0) {
        const m = this.queue.shift();
        if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
        else ex.web_keys(m.mask >>> 0);
        this.nextOk = now + HOLD_MS;
      }
    }

    process(inputs, outputs) {
      const ex = this.ex;
      if (!ex) return true;
      if (this.queue.length) this.input();
      const out = outputs[0], n = out[0].length;
      const t0 = clock();
      ex.web_render(n);
      this.busy += clock() - t0;
      this.frames += n;
      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
      if (currentTime - this.lastFrame >= this.period) {
        this.lastFrame = currentTime;
        this.publish();
      }
      return true;
    }
  }
  registerProcessor("felucca", Felucca);
}
