// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: user wavetables. A WAV file (single-cycle frames) -> the 19 x 64 harmonic bytes a user table of the WAVE engine stores,
// the preview of what the unit will make of them, the honest note of what is lost, and the unit's commands for reading and writing a slot
// (EDITOR_PROTOCOL.md "User wavetables (81-83)"). No DOM here (wavetable_demo.html and the editor page apply it; test_wavetable.mjs runs it).
//
// What the unit stores (firmware eng_wave.c, wtab.c): per slot 19 frames x 64 harmonics, ONE BYTE each, and an 8 character name. The engine
// builds a user table like a factory recipe: every harmonic is a sine, the same phase in every frame, so a mix of two frames is a mix of their
// spectra; each frame's harmonic levels are scaled so its strongest is 1, then the engine gives every frame the same loudness (WV_RMS).
//
// The byte is a LOG amplitude, relative to the strongest harmonic of its frame (which is 255), 16 steps to an octave (0.376 dB a step):
//   byte 0 = off;  byte b >= 1:  amplitude = 2^(-(255 - b) / 16)  (Q16 in the unit: LUT[(255 - b) & 15] >> ((255 - b) >> 4))
// Why log: the engine's recipes are linear levels like 1 / k; 8 bits of linear amplitude would give harmonic 64 of a saw (1/64) only 4 steps,
// and the ear judges level in dB. The scale reaches -95 dB; the engine itself cannot use anything under -66 dB (its levels are 10 bit).
//
// Algorithms: the FFT is the plain iterative radix-2 Cooley-Tukey FFT (J. W. Cooley and J. W. Tukey, 1965), written for this file; frames
// whose length is not a power of two get a direct DFT of the 64 harmonics (exact for one whole cycle). Linear interpolation between frames
// is done on the harmonic magnitudes. No code of other projects is used (NOTICE.md, LICENSING.md).
import { bkU32, bkR32, bkPack, bkUnpack, bkCrc } from "../fm1backup.js";

export const WT = {
  FRAMES: 19, HARMONICS: 64, BYTES: 19 * 64, NAME: 8, SLOTS: 3, HDR: 12, OBJ: 12 + 19 * 64, VERSION: 1,
  FIRST_TABLE: 26,                 // TABLE value of USR1 (the 26 factory tables come first)
  CMD: { LIST: 81, GET: 82, PUT: 83 }, BACKUP_IDS: [24, 25, 26], CAPS_BIT: 8,   // INFO 42 01 caps bit 3
  CHUNK: 112,                      // bytes of table data a GET / PUT piece carries (16 pack7 groups)
};
export const LOST_NOTE = "User tables are reduced to 19 frames and 64 harmonics. Phases are not kept: every harmonic is a sine, the same in all frames. " +
  "Each frame is scaled to the same loudness, and the level of every harmonic is stored in 0.4 dB steps.";

// ---------------------------------------------------------------------------------------------------- the amplitude byte
export const AMP_LUT = [65536, 62757, 60097, 57549, 55109, 52773, 50535, 48393, 46341, 44376, 42495, 40693, 38968, 37316, 35734, 34219];   // round(65536 * 2^(-r / 16))
export const byteToQ16 = (b) => (b ? AMP_LUT[(255 - b) & 15] >> ((255 - b) >> 4) : 0);   // the unit's decode, bit for bit
export const byteToAmp = (b) => byteToQ16(b) / 65536;
export const FLOOR_DB = -66;                                                  // under this (relative to the frame's strongest) a harmonic is 0
export function ampToByte(ratio, floorDb = FLOOR_DB) {                        // ratio: amplitude / the frame's strongest (0..1]
  if (!(ratio > 0) || 20 * Math.log10(ratio) < floorDb) return 0;
  return Math.max(1, Math.min(255, Math.round(255 + 16 * Math.log2(ratio))));
}

// ---------------------------------------------------------------------------------------------------------------- WAV
const tag = (v, o) => String.fromCharCode(v.getUint8(o), v.getUint8(o + 1), v.getUint8(o + 2), v.getUint8(o + 3));
// -> { sampleRate, channels, bits, format, samples (Float64Array, -1..1, channels averaged), clmFrameSize (null or the Serum "clm " chunk's size) }
export function parseWav(buf) {
  const bytes = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (bytes.length < 44 || tag(v, 0) !== "RIFF" || tag(v, 8) !== "WAVE") throw new Error("This is not a WAV file.");
  let fmt = null, data = null, clm = null;
  for (let o = 12; o + 8 <= bytes.length;) {
    const id = tag(v, o); let n = v.getUint32(o + 4, true);
    const at = o + 8;
    if (at + n > bytes.length) n = bytes.length - at;          // a truncated last chunk: take what is there
    if (id === "fmt ") fmt = { tag: v.getUint16(at, true), channels: v.getUint16(at + 2, true), rate: v.getUint32(at + 4, true), bits: v.getUint16(at + 14, true),
                               sub: n >= 26 ? v.getUint16(at + 24, true) : 0 };
    else if (id === "data") data = { at, n };
    else if (id === "clm ") clm = new TextDecoder("latin1").decode(bytes.subarray(at, at + n));
    o = at + n + (n & 1);
  }
  if (!fmt || !data) throw new Error("This WAV file has no audio data.");
  const kind = fmt.tag === 0xfffe ? fmt.sub : fmt.tag;
  if (kind !== 1 && kind !== 3) throw new Error("This WAV file is compressed. Use a plain PCM or float WAV.");
  const ch = fmt.channels, bits = fmt.bits, bps = bits >> 3;
  if (!ch || ![8, 16, 24, 32, 64].includes(bits) || (kind === 3 && bits !== 32 && bits !== 64)) throw new Error(`This WAV file's sample format (${bits} bit) is not supported.`);
  const frames = Math.floor(data.n / (bps * ch)), out = new Float64Array(frames);
  const rd = (o) => {
    if (kind === 3) return bits === 32 ? v.getFloat32(o, true) : v.getFloat64(o, true);
    if (bits === 8) return (v.getUint8(o) - 128) / 128;
    if (bits === 16) return v.getInt16(o, true) / 32768;
    if (bits === 24) return ((v.getUint8(o) | v.getUint8(o + 1) << 8 | v.getInt8(o + 2) << 16)) / 8388608;
    return v.getInt32(o, true) / 2147483648;
  };
  for (let i = 0; i < frames; i++) {
    let s = 0; for (let c = 0; c < ch; c++) s += rd(data.at + (i * ch + c) * bps);
    out[i] = s / ch;
  }
  const m = clm && /<!>\s*(\d+)/.exec(clm);                     // Serum, Vital, Surge: "<!>2048 20000000 wavetable ..."
  return { sampleRate: fmt.rate, channels: ch, bits, format: kind === 3 ? "float" : "pcm", samples: out, clmFrameSize: m ? +m[1] : null };
}
// samples (Float64Array, -1..1) -> a WAV file (Uint8Array). opts: { rate, float (32-bit float instead of 16 bit), clm (text of a "clm " chunk) }
export function writeWav(samples, { rate = 44100, float = false, clm = null } = {}) {
  const bps = float ? 4 : 2, n = samples.length * bps, extra = clm ? 8 + clm.length + (clm.length & 1) : 0;
  const out = new Uint8Array(44 + n + extra), v = new DataView(out.buffer);
  const put = (o, s) => { for (let i = 0; i < 4; i++) out[o + i] = s.charCodeAt(i); };
  put(0, "RIFF"); v.setUint32(4, 36 + n + extra, true); put(8, "WAVE"); put(12, "fmt "); v.setUint32(16, 16, true);
  v.setUint16(20, float ? 3 : 1, true); v.setUint16(22, 1, true); v.setUint32(24, rate, true); v.setUint32(28, rate * bps, true);
  v.setUint16(32, bps, true); v.setUint16(34, bps * 8, true);
  let o = 36;
  if (clm) { put(o, "clm "); v.setUint32(o + 4, clm.length, true); for (let i = 0; i < clm.length; i++) out[o + 8 + i] = clm.charCodeAt(i); o += 8 + clm.length + (clm.length & 1); }
  put(o, "data"); v.setUint32(o + 4, n, true); o += 8;
  for (let i = 0; i < samples.length; i++, o += bps) {
    if (float) v.setFloat32(o, samples[i], true);
    else v.setInt16(o, Math.max(-32768, Math.min(32767, Math.round(samples[i] * 32767))), true);
  }
  return out;
}

// ---------------------------------------------------------------------------------------------------------------- FFT
// In place, n a power of two (complex re / im). Radix-2 decimation in time.
export function fft(re, im) {
  const n = re.length;
  if (n & (n - 1)) throw new Error("fft: the length must be a power of two");
  for (let i = 1, j = 0; i < n; i++) {                           // bit reversal
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { let t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = -2 * Math.PI / len, half = len >> 1;
    for (let i = 0; i < n; i += len)
      for (let k = 0; k < half; k++) {
        const wr = Math.cos(ang * k), wi = Math.sin(ang * k), a = i + k, b = a + half;
        const xr = re[b] * wr - im[b] * wi, xi = re[b] * wi + im[b] * wr;
        re[b] = re[a] - xr; im[b] = im[a] - xi; re[a] += xr; im[a] += xi;
      }
  }
}
// Peak amplitudes of the harmonics 1..H of ONE whole cycle (frame: any length >= 8). Float64Array(H), index k - 1. A harmonic at or above
// the frame's Nyquist (k >= length / 2) cannot be seen and is 0. A power-of-two frame goes through the FFT, any other length through a direct DFT.
export function harmonics(frame, H = WT.HARMONICS) {
  const S = frame.length, out = new Float64Array(H), top = Math.min(H, Math.ceil(S / 2) - 1);
  if (!(S & (S - 1))) {
    const re = Float64Array.from(frame), im = new Float64Array(S);
    fft(re, im);
    for (let k = 1; k <= top; k++) out[k - 1] = 2 * Math.hypot(re[k], im[k]) / S;
  } else {
    const c = new Float64Array(S), s = new Float64Array(S);
    for (let i = 0; i < S; i++) { c[i] = Math.cos(2 * Math.PI * i / S); s[i] = Math.sin(2 * Math.PI * i / S); }
    for (let k = 1; k <= top; k++) {
      let xr = 0, xi = 0;
      for (let n = 0, idx = 0; n < S; n++, idx = (idx + k) % S) { xr += frame[n] * c[idx]; xi += frame[n] * s[idx]; }
      out[k - 1] = 2 * Math.hypot(xr, xi) / S;
    }
  }
  return out;
}
// power of the frame's alternating part (mean of x^2 minus the mean squared): what Parseval says the harmonics 1.. share
const acPower = (f) => { let m = 0, q = 0; for (const x of f) { m += x; q += x * x; } m /= f.length; return q / f.length - m * m; };

// ----------------------------------------------------------------------------------------------------------- converting
const isPow2 = (n) => n > 0 && !(n & (n - 1));
// The frame size to use: an explicit one; else the file's "clm " chunk (Serum, Vital); else 2048 when the file is a whole number of
// 2048 frames; else, when the whole file is a power of two 256..4096 long, one single-cycle frame. null: it cannot be told (ask the user).
export function guessFrameSize(wav) {
  const n = wav.samples.length;
  if (wav.clmFrameSize && wav.clmFrameSize >= 64 && wav.clmFrameSize <= n && n % wav.clmFrameSize === 0) return wav.clmFrameSize;
  if (n >= 2048 && n % 2048 === 0) return 2048;
  if (isPow2(n) && n >= 256 && n <= 4096) return n;
  return null;
}
// samples -> { data: Uint8Array(1216), frameSize, sourceFrames, spectra: [19][64] linear (the frame's strongest = 1), lost: {...} }
// opts: { frameSize, floorDb }. A frame size that is not a power of two is accepted (one whole cycle in that many samples, e.g. 600 for AKWF
// files). floorDb (default -66): a harmonic this far under its frame's strongest is dropped (raise it for a noisy source: 8-bit files, -54).
export function convert(samples, { frameSize = null, floorDb = FLOOR_DB } = {}) {
  const S = frameSize;
  if (!Number.isInteger(S) || S < 130 || S > 1 << 16) throw new Error("The frame size must be a whole number of samples, at least 130 (a frame must hold 64 harmonics).");
  const N = Math.floor(samples.length / S);
  if (N < 1) throw new Error(`The file is shorter than one frame (${S} samples).`);
  const raw = [], acp = [];
  for (let i = 0; i < N; i++) { const f = samples.subarray(i * S, (i + 1) * S); raw.push(harmonics(f)); acp.push(acPower(f)); }
  // 19 frames evenly over the N: position f (N - 1) / 18, the magnitudes interpolated between the two frames around it
  const mags = [], dropped = [];
  for (let f = 0; f < WT.FRAMES; f++) {
    const pos = N === 1 ? 0 : f * (N - 1) / (WT.FRAMES - 1), i = Math.min(N - 1, Math.floor(pos)), t = pos - i, j = Math.min(N - 1, i + 1);
    mags.push(raw[i].map((x, k) => x * (1 - t) + raw[j][k] * t));
    dropped.push({ power: acp[i] * (1 - t) + acp[j] * t, kept: raw[i].reduce((s, x) => s + x * x / 2, 0) * (1 - t) + raw[j].reduce((s, x) => s + x * x / 2, 0) * t });
  }
  let gmax = 0; for (const m of mags) for (const x of m) gmax = Math.max(gmax, x);
  const data = new Uint8Array(WT.BYTES), spectra = [];
  let worstQ = 0, silent = 0;
  for (let f = 0; f < WT.FRAMES; f++) {
    const m = mags[f], pk = Math.max(...m);
    if (!(pk > gmax * 1e-4)) { silent++; spectra.push(new Float64Array(WT.HARMONICS)); continue; }   // a frame 80 dB under the loudest stays silent
    const lin = new Float64Array(WT.HARMONICS);
    for (let k = 0; k < WT.HARMONICS; k++) {
      const b = ampToByte(m[k] / pk, floorDb);
      data[f * WT.HARMONICS + k] = b;
      lin[k] = byteToAmp(b);
      if (b > 1) worstQ = Math.max(worstQ, Math.abs(20 * Math.log10(byteToAmp(b) / (m[k] / pk))));
    }
    spectra.push(lin);
  }
  // what is lost, in numbers
  const above = dropped.map((d) => (d.power > 1e-12 ? Math.max(0, 1 - d.kept / d.power) : 0));
  const aboveDb = above.map((x) => (x > 1e-9 ? 10 * Math.log10(x) : -Infinity));
  const lost = {
    sourceFrames: N, frameSize: S, usedFrames: WT.FRAMES, interpolated: N > 1 && (N - 1) % (WT.FRAMES - 1) !== 0, singleFrame: N === 1,
    sourceHarmonics: Math.ceil(S / 2) - 1, keptHarmonics: Math.min(WT.HARMONICS, Math.ceil(S / 2) - 1),
    aboveFraction: above, aboveDbWorst: Math.max(...aboveDb), aboveDbMean: 10 * Math.log10(Math.max(1e-12, above.reduce((s, x) => s + x, 0) / above.length)),
    quantErrDb: worstQ, silentFrames: silent,
  };
  return { data, frameSize: S, sourceFrames: N, spectra, lost };
}
// WAV file bytes -> { wav, frameSize, ...convert() }. opts: { frameSize (overrides the guess), floorDb }
export function convertWav(buf, { frameSize = null, floorDb = FLOOR_DB } = {}) {
  const wav = parseWav(buf), S = frameSize || guessFrameSize(wav);
  if (!S) throw new Error(`The frame size of this file is not known (${wav.samples.length} samples). Choose the number of samples in one cycle.`);
  return { wav, ...convert(wav.samples, { frameSize: S, floorDb }) };
}
// The honest note for a conversion: lines of plain text, the fixed sentence first.
export function lostNote(r) {
  const L = r.lost, db = (x) => (x === -Infinity ? "none" : `${x.toFixed(0)} dB`), out = [LOST_NOTE];
  if (L.singleFrame) out.push("The file holds one frame: all 19 frames are the same, POS does not change the sound.");
  else out.push(`${L.sourceFrames} frames of ${L.frameSize} samples became 19 (${L.interpolated ? "the in-between ones mixed from their neighbours" : "every one picked as it is"}).`);
  out.push(L.sourceHarmonics > 64 ? `Harmonics 65 to ${L.sourceHarmonics} are dropped: the worst frame loses energy ${db(L.aboveDbWorst)} under its total, on average ${db(L.aboveDbMean)}. ` +
    "The upper harmonics are the bright edge of a sharp wave; the unit's WAVE engine band-limits to 64 harmonics anyway." : "All harmonics of the file are kept.");
  if (L.silentFrames) out.push(`${L.silentFrames} frame(s) are silent (80 dB under the loudest) and stay silent.`);
  out.push(`The level of each harmonic is rounded to 0.38 dB (worst error ${L.quantErrDb.toFixed(2)} dB). The mean (DC) is dropped.`);
  return out;
}

// ------------------------------------------------------------------------------------------------------------- preview
// bytes -> the 19 spectra the unit will use, [19][64] linear, each frame's strongest 1
export function spectraOf(data) {
  const out = [];
  for (let f = 0; f < WT.FRAMES; f++) out.push(Float64Array.from({ length: WT.HARMONICS }, (_, k) => byteToAmp(data[f * WT.HARMONICS + k])));
  return out;
}
// one cycle of frame f (0..18, fractional: the mix of two frames as the engine's POS makes it) as n samples, sine phase, peak 1
export function renderCycle(data, pos, n = 256) {
  const f = Math.min(WT.FRAMES - 1, Math.max(0, Math.floor(pos))), t = Math.min(1, pos - f), g = Math.min(WT.FRAMES - 1, f + 1);
  const out = new Float64Array(n);
  for (let k = 1; k <= WT.HARMONICS; k++) {
    const a = byteToAmp(data[f * WT.HARMONICS + k - 1]) * (1 - t) + byteToAmp(data[g * WT.HARMONICS + k - 1]) * t;
    if (a) for (let i = 0; i < n; i++) out[i] += a * Math.sin(2 * Math.PI * k * i / n);
  }
  const pk = Math.max(1e-12, ...out.map(Math.abs));
  return out.map((x) => x / pk);
}

// ------------------------------------------------------------------------------------------------------- name, object
export function cleanName(s) { return String(s ?? "").toUpperCase().replace(/[^A-Z0-9 .+#-]/g, "").replace(/ +/g, " ").trim().slice(0, WT.NAME).trim(); }
// the stored object (also the backup object of ids 24..26): version, frames, harmonics, flags, 8 name bytes (0 padded), 1216 bytes
export function buildObject(name, data) {
  if (data.length !== WT.BYTES) throw new Error("A user table is 19 x 64 bytes.");
  const o = new Uint8Array(WT.OBJ);
  o[0] = WT.VERSION; o[1] = WT.FRAMES; o[2] = WT.HARMONICS; o[3] = 0;
  const nm = cleanName(name); for (let i = 0; i < nm.length; i++) o[4 + i] = nm.charCodeAt(i);
  o.set(data, WT.HDR);
  return o;
}
// null if the object is acceptable, else a short reason (what the unit insists on at commit, firmware wt_obj_ok)
export function checkObject(o) {
  if (o.length !== WT.OBJ) return "not a user wavetable";
  if (o[0] !== WT.VERSION || o[1] !== WT.FRAMES || o[2] !== WT.HARMONICS || o[3] !== 0) return "a user wavetable of another version";
  for (let i = 0; i < WT.NAME; i++) if (o[4 + i] && (o[4 + i] < 32 || o[4 + i] > 126)) return "a bad name";
  return o.subarray(WT.HDR).some((b) => b) ? null : "an empty table";
}
export function parseObject(o) {
  const why = checkObject(o); if (why) throw new Error(`Not a user wavetable: ${why}.`);
  let name = ""; for (let i = 0; i < WT.NAME && o[4 + i]; i++) name += String.fromCharCode(o[4 + i]);
  return { name, data: o.slice(WT.HDR) };
}

// -------------------------------------------------------------------------------------------------------- the unit
const NOTES = { 1: "The unit refused the request", 2: "The unit found the table invalid (checksum, name or content)", 3: "The unit is playing. Stop playback on the unit and try again",
                4: "The unit could not write its flash memory", 5: "The transfer on the unit expired. Start again" };
const rc = (c) => { if (c) throw Object.assign(new Error(NOTES[c] || `The unit answered error ${c}`), { rc: c }); };
// LIST -> { slots: 3, frames: 19, harmonics: 64, tables: [{slot, used, name, crc}] }
export function parseList(a) {
  if (a[0] !== WT.SLOTS || a[1] !== WT.FRAMES || a[2] !== WT.HARMONICS) throw new Error("The unit's user tables are not 3 x 19 x 64: use a matching editor.");
  const tables = []; let p = 3;
  for (let s = 0; s < a[0]; s++) {
    const used = a[p++]; let name = "";
    while (a[p]) name += String.fromCharCode(a[p++]);
    p++;
    tables.push({ slot: s, used: !!used, name, crc: bkR32(a, p) }); p += 5;
  }
  if (p !== a.length) throw new Error("The unit sent an unreadable list of tables.");
  return { slots: a[0], frames: a[1], harmonics: a[2], tables };
}
export const listTables = async (request) => parseList(await request([WT.CMD.LIST, []], { timeout: 3000, retries: 1 }));
// -> { name, data } of a used slot (CRC checked against the list)
export async function readTable(request, slot) {
  const t = (await listTables(request)).tables[slot];
  if (!t?.used) throw new Error("That slot is empty.");
  const data = new Uint8Array(WT.BYTES);
  for (let off = 0; off < WT.BYTES; off += WT.CHUNK) {
    const n = Math.min(WT.CHUNK, WT.BYTES - off);
    const a = await request([WT.CMD.GET, [slot, off & 127, off >> 7, n]], { timeout: 1500, retries: 1 });
    rc(a[1]);
    if (a[0] !== slot || (a[2] | a[3] << 7) !== off || a[4] !== n) throw new Error("Unexpected answer from the unit");
    data.set(bkUnpack(a.slice(5), n), off);
  }
  if (bkCrc(data) !== t.crc) throw new Error("The unit changed while it was read. Try again.");
  return { name: t.name, data };
}
// begin (name, CRC), pieces, commit. The unit stops the sequencer itself; rc 3 if it would not stop. Aborts the transfer on an error.
export async function writeTable(request, slot, name, data, onProgress = () => {}) {
  if (data.length !== WT.BYTES) throw new Error("A user table is 19 x 64 bytes.");
  const put = async (args) => { const a = await request([WT.CMD.PUT, args], { timeout: 6000, retries: 0 }); rc(a[2]); };
  const nm = cleanName(name), nb = Array.from({ length: WT.NAME }, (_, i) => (i < nm.length ? nm.charCodeAt(i) : 0));
  await put([0, slot, ...nb, ...bkU32(bkCrc(data))]);
  try {
    for (let off = 0; off < WT.BYTES; off += WT.CHUNK) {
      await put([1, slot, off & 127, off >> 7, ...bkPack(data.subarray(off, off + WT.CHUNK))]);
      onProgress(Math.min(WT.BYTES, off + WT.CHUNK), WT.BYTES);
    }
    await put([2, slot]);
  } catch (e) { await put([3, slot]).catch(() => {}); throw e; }
}
export async function eraseTable(request, slot) {
  const a = await request([WT.CMD.PUT, [4, slot]], { timeout: 6000, retries: 0 });
  rc(a[2]);
}

// ----------------------------------------------------------------------------------------------- the "duell-wavetable" file
// { format: "duell-wavetable", version: 1, name, frames: 19, harmonics: 64, data: base64 of the 1216 bytes, crc32: 8 hex digits, firmware, created, source }
// Small (about 1.7 KB) so tables can be shared. The bytes are exactly what the unit stores; the file says nothing a unit could not take.
export const TABLE_FORMAT = "duell-wavetable";
const b64 = (bytes) => btoa(String.fromCharCode(...bytes));
export function tableToFile(name, data, { firmware = "", source = "" } = {}) {
  if (data.length !== WT.BYTES) throw new Error("A user table is 19 x 64 bytes.");
  return JSON.stringify({ format: TABLE_FORMAT, version: 1, app: "DUELL editor", name: cleanName(name), frames: WT.FRAMES, harmonics: WT.HARMONICS, crc32: (bkCrc(data) >>> 0).toString(16).padStart(8, "0"),
    data: b64(data), firmware, created: new Date().toISOString().slice(0, 19) + "Z", source }, null, 1) + "\n";
}
// -> { ok: true, name, data } or { ok: false, error }
export function parseTableFile(text) {
  let j;
  try { j = JSON.parse(text); } catch { return { ok: false, error: "This is not a JSON file." }; }
  if (!j || j.format !== TABLE_FORMAT) return { ok: false, error: j?.format === "duell-patches" ? "This is a patch file, not a wavetable file." : "This is not a DUELL wavetable file." };
  if (j.version !== 1) return { ok: false, error: `This wavetable file is version ${j.version}; this editor reads version 1.` };
  if (j.frames !== WT.FRAMES || j.harmonics !== WT.HARMONICS) return { ok: false, error: `This table is ${j.frames} x ${j.harmonics}; the unit's tables are ${WT.FRAMES} x ${WT.HARMONICS}.` };
  let data;
  try {
    if (typeof j.data !== "string" || !/^[A-Za-z0-9+/]*={0,2}$/.test(j.data)) throw new Error("x");
    const s = atob(j.data); data = Uint8Array.from(s, (c) => c.charCodeAt(0));
  } catch { return { ok: false, error: "The table data is damaged (not base64)." }; }
  if (data.length !== WT.BYTES) return { ok: false, error: `The table data is ${data.length} bytes, not ${WT.BYTES}.` };
  if (j.crc32 != null && (bkCrc(data) >>> 0).toString(16).padStart(8, "0") !== String(j.crc32).toLowerCase()) return { ok: false, error: "The table fails its checksum, so it was damaged or edited." };
  if (!data.some((b) => b)) return { ok: false, error: "The table is empty (all zero); the unit would refuse it." };
  const name = cleanName(j.name);
  return { ok: true, name: name || "TABLE", data, firmware: String(j.firmware ?? ""), source: String(j.source ?? "") };
}
