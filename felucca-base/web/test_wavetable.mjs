// SPDX-License-Identifier: GPL-3.0-only
// The WAV -> user wavetable converter (editor/wavetable.js) and its protocol client against the simulated unit (editor/mockunit.js).
// Run: node web/test_wavetable.mjs     (host_tests.sh wavetable; also a CLI check: node web/test_wavetable.mjs file.wav [frameSize] prints the conversion of a file)
import { readFileSync } from "node:fs";
import { WT, AMP_LUT, byteToQ16, byteToAmp, ampToByte, parseWav, writeWav, fft, harmonics, guessFrameSize, convert, convertWav, lostNote, spectraOf, renderCycle,
         cleanName, buildObject, checkObject, parseObject, listTables, readTable, writeTable, eraseTable, parseList, LOST_NOTE } from "./editor/wavetable.js";
import { MockUnit } from "./editor/mockunit.js";
import { bkCrc } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(120)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const throwsMsg = (f, re) => { try { f(); return false; } catch (e) { return re.test(e.message); } };
const athrows = async (f, re) => { try { await f(); return false; } catch (e) { return re.test(e.message); } };

// ---- the file given on the command line: print what the converter makes of it
if (process.argv[2]) {
  const r = convertWav(readFileSync(process.argv[2]), process.argv[3] ? { frameSize: +process.argv[3] } : {});
  console.log(`${process.argv[2]}: ${r.wav.samples.length} samples, ${r.wav.sampleRate} Hz, frame size ${r.frameSize}, ${r.sourceFrames} frames`);
  console.log(lostNote(r).join("\n"));
  for (let f = 0; f < WT.FRAMES; f++) console.log(String(f).padStart(2), [...r.data.subarray(f * 64, f * 64 + 16)].map((b) => String(b).padStart(3)).join(" "), "...");
  process.exit(0);
}

// additive synthesis of one frame: partials [[k, amplitude], ...] -> S samples
const frame = (S, partials, phase = () => 0) => {
  const out = new Float64Array(S);
  for (const [k, a] of partials) for (let i = 0; i < S; i++) out[i] += a * Math.sin(2 * Math.PI * k * i / S + phase(k));
  return out;
};
const expectByte = (r) => (r > 0 ? Math.max(0, Math.round(255 + 16 * Math.log2(r))) : 0);
const maxDiff = (a, b, from = 0, to = a.length) => { let m = 0; for (let i = from; i < to; i++) m = Math.max(m, Math.abs(a[i] - b[i])); return m; };

// ---- the byte scale: the firmware's own table, and the maths
{
  // the firmware (eng_wave.c WV_AMP) and this module must hold the same 16 numbers
  const src = readFileSync(new URL("../firmware/src/eng_wave.c", import.meta.url), "utf8");
  const m = /WV_AMP\[16\] = \{([^}]*)\}/.exec(src), fw = m && m[1].split(",").map((x) => +x.trim());
  ok(fw && fw.length === 16 && fw.every((v, i) => v === AMP_LUT[i]) && AMP_LUT.every((v, i) => v === Math.round(65536 * 2 ** (-i / 16))), "the amplitude table equals eng_wave.c's WV_AMP and round(65536 * 2^(-r / 16))");
  ok(byteToQ16(255) === 65536 && byteToQ16(0) === 0 && byteToQ16(239) === 32768 && byteToQ16(223) === 16384 && byteToQ16(1) === 1, "decode: 255 = 1.0, 239 = 1/2, 223 = 1/4, 0 = off");
  let mono = true, worst = 0;
  for (let b = 1; b < 256; b++) { mono &&= byteToQ16(b) >= byteToQ16(b - 1); if (b >= 160) worst = Math.max(worst, Math.abs(20 * Math.log10(byteToQ16(b) / (65536 * 2 ** (-(255 - b) / 16))))); }
  ok(mono && worst < 0.01, "decode is monotonic and, down to -36 dB (b >= 160), within 0.01 dB of 2^(-(255 - b) / 16); under it the Q16 truncation shows (the engine's levels are 10 bit: -60 dB)");
  let rt = true; for (let b = 1; b < 256; b++) { const ex = 2 ** (-(255 - b) / 16); rt &&= ampToByte(ex) === (20 * Math.log10(ex) < -66 ? 0 : b); }
  ok(rt, "encode(2^(-(255 - b) / 16)) = b for every byte above the -66 dB floor (under it: 0)");
  ok(ampToByte(1) === 255 && ampToByte(0.5) === 239 && ampToByte(1 / 64) === 159 && ampToByte(0.0004) === 0 && ampToByte(0) === 0 && ampToByte(0.0006, -80) > 0, "encode: 1 = 255, 1/2 = 239, 1/64 = 159 (a saw's 64th), under the floor 0, a lower floor keeps it");
  // the quantiser's worst error over the whole range: half a step, 0.188 dB
  let e = 0; for (let i = 0; i < 20000; i++) { const r = 10 ** (-(i / 20000) * 66 / 20), b = ampToByte(r); e = Math.max(e, Math.abs(20 * Math.log10(2 ** (-(255 - b) / 16) / r))); }
  ok(e < 0.2, `encode: the level error is at most half a step (0.188 dB): worst ${e.toFixed(3)} dB`);
}

// ---- FFT and harmonics
{
  const n = 256, re = Float64Array.from({ length: n }, (_, i) => Math.sin(2 * Math.PI * 3 * i / n) + 0.5 * Math.cos(2 * Math.PI * 7 * i / n)), im = new Float64Array(n), re0 = re.slice();
  fft(re, im);
  let dr = 0, di = 0;                                                    // against the plain DFT
  for (const k of [0, 1, 3, 7, 100, 129, 253]) {
    let sr = 0, si = 0; for (let t = 0; t < n; t++) { sr += re0[t] * Math.cos(2 * Math.PI * k * t / n); si -= re0[t] * Math.sin(2 * Math.PI * k * t / n); }
    dr = Math.max(dr, Math.abs(sr - re[k])); di = Math.max(di, Math.abs(si - im[k]));
  }
  ok(dr < 1e-9 && di < 1e-9, "FFT (256) equals the direct DFT at 7 bins to 1e-9");
  ok(throwsMsg(() => fft(new Float64Array(12), new Float64Array(12)), /power of two/), "FFT refuses a length that is not a power of two");
  const h = harmonics(frame(256, [[1, 1], [2, 0.5], [64, 0.25]]));
  ok(Math.abs(h[0] - 1) < 1e-9 && Math.abs(h[1] - 0.5) < 1e-9 && Math.abs(h[63] - 0.25) < 1e-9 && h[2] < 1e-9, "harmonics: peak amplitudes of 1, 2 and 64 come out as 1, 0.5, 0.25 (a sine's phase does not matter)");
  const hp = harmonics(frame(256, [[1, 1], [5, 0.3]], (k) => k * 1.234));
  ok(Math.abs(hp[0] - 1) < 1e-9 && Math.abs(hp[4] - 0.3) < 1e-9, "harmonics: an arbitrary phase gives the same magnitudes (the phases are dropped)");
  const h600 = harmonics(frame(600, [[1, 1], [3, 0.4], [64, 0.2]]));    // a direct DFT: AKWF's 600 sample cycles
  ok(Math.abs(h600[0] - 1) < 1e-9 && Math.abs(h600[2] - 0.4) < 1e-9 && Math.abs(h600[63] - 0.2) < 1e-9 && h600[1] < 1e-9, "harmonics: a 600 sample cycle (not a power of two: direct DFT) is exact too");
  const hs = harmonics(frame(128, [[1, 1], [40, 1], [63, 1]]));
  ok(hs.slice(64).length === 0 && Math.abs(hs[62] - 1) < 1e-9 && hs[63] === 0, "harmonics: a 128 sample frame holds harmonics up to 63, the 64th is 0 (its Nyquist)");
}

// ---- WAV reading and writing
{
  const x = Float64Array.from({ length: 1000 }, (_, i) => Math.sin(i / 10) * 0.8);
  const w16 = parseWav(writeWav(x, { rate: 48000 })), wf = parseWav(writeWav(x, { float: true }));
  ok(w16.sampleRate === 48000 && w16.bits === 16 && w16.samples.length === 1000 && maxDiff(w16.samples, x) < 1e-4 && wf.format === "float" && maxDiff(wf.samples, x) < 1e-6, "WAV: 16 bit PCM and 32 bit float round trip");
  // 24 bit, 8 bit, stereo (averaged), extensible, an odd chunk, a clm chunk, a truncated data chunk
  const mk = (bits, ch, f, extra = [], ext = false) => {
    const bps = bits / 8, n = 64, data = new Uint8Array(n * ch * bps), v = new DataView(data.buffer);
    for (let i = 0; i < n; i++) for (let c = 0; c < ch; c++) f(v, (i * ch + c) * bps, i, c);
    const fmt = new Uint8Array(ext ? 40 : 16), fv = new DataView(fmt.buffer);
    fv.setUint16(0, ext ? 0xfffe : 1, true); fv.setUint16(2, ch, true); fv.setUint32(4, 44100, true); fv.setUint32(8, 44100 * ch * bps, true); fv.setUint16(12, ch * bps, true); fv.setUint16(14, bits, true);
    if (ext) { fv.setUint16(16, 22, true); fv.setUint16(18, bits, true); fv.setUint16(24, 1, true); }
    const chunks = [["fmt ", fmt], ...extra, ["data", data]], size = chunks.reduce((s, [, b]) => s + 8 + b.length + (b.length & 1), 4);
    const out = new Uint8Array(8 + size), ov = new DataView(out.buffer); out.set([...("RIFF")].map((c) => c.charCodeAt(0))); ov.setUint32(4, size, true); out.set([...("WAVE")].map((c) => c.charCodeAt(0)), 8);
    let o = 12; for (const [id, b] of chunks) { out.set([...id].map((c) => c.charCodeAt(0)), o); ov.setUint32(o + 4, b.length, true); out.set(b, o + 8); o += 8 + b.length + (b.length & 1); }
    return out;
  };
  const w24 = parseWav(mk(24, 1, (v, o, i) => { const s = Math.round(8388607 * Math.sin(i / 5)); v.setUint8(o, s & 255); v.setUint8(o + 1, s >> 8 & 255); v.setUint8(o + 2, s >> 16 & 255); }));
  ok(w24.bits === 24 && Math.abs(w24.samples[8] - Math.sin(8 / 5)) < 1e-6 && Math.abs(w24.samples[40] - Math.sin(40 / 5)) < 1e-6, "WAV: 24 bit (negative samples too)");
  const w8 = parseWav(mk(8, 1, (v, o, i) => v.setUint8(o, 128 + (i % 2 ? 64 : -64))));
  ok(w8.samples[0] === -0.5 && w8.samples[1] === 0.5, "WAV: 8 bit unsigned");
  const ws = parseWav(mk(16, 2, (v, o, i, c) => v.setInt16(o, c ? 16384 : -16384, true), [], true));
  ok(ws.channels === 2 && ws.samples.every((s) => s === 0), "WAV: stereo is averaged to mono; WAVE_FORMAT_EXTENSIBLE is read");
  const wc = parseWav(mk(16, 1, (v, o) => v.setInt16(o, 1000, true), [["LIST", new Uint8Array(5)], ["clm ", new TextEncoder().encode("<!>2048 20000000 wavetable (www.surge-synth-team.org)")]]));
  ok(wc.clmFrameSize === 2048 && wc.samples.length === 64, "WAV: the 'clm ' chunk of Serum / Vital gives the frame size (an odd-sized chunk before it is skipped over)");
  ok(throwsMsg(() => parseWav(new Uint8Array(100)), /not a WAV/) && throwsMsg(() => parseWav(mk(16, 1, () => {}).slice(0, 20)), /not a WAV/), "WAV: garbage and a header cut short are refused in plain words");
  const bad = mk(16, 1, () => {}); bad[20] = 2;                         // ADPCM
  ok(throwsMsg(() => parseWav(bad), /compressed/), "WAV: a compressed file is refused");
  const tr = mk(16, 1, () => {}); const cut = tr.slice(0, tr.length - 30);
  ok(parseWav(cut).samples.length === 49, "WAV: a data chunk cut short gives the samples that are there");
}

// ---- frame size
{
  const mkw = (n, clm = null) => ({ samples: new Float64Array(n), clmFrameSize: clm });
  ok(guessFrameSize(mkw(2048 * 256)) === 2048 && guessFrameSize(mkw(4096 * 8, 4096)) === 4096 && guessFrameSize(mkw(2048)) === 2048 && guessFrameSize(mkw(1024)) === 1024 && guessFrameSize(mkw(600)) === null && guessFrameSize(mkw(3000, 2048)) === null,
     "frame size: from the clm chunk, else 2048 for whole multiples, else a power of two 256..4096 as one cycle, else unknown (ask)");
  ok(throwsMsg(() => convert(new Float64Array(5000), { frameSize: 100 }), /at least 130/) && throwsMsg(() => convert(new Float64Array(100), { frameSize: 256 }), /shorter than one frame/) &&
     throwsMsg(() => convertWav(writeWav(new Float64Array(600))), /not known/), "frame size: too small, a file shorter than one frame, and an unknown size are refused with a plain message");
}

// ---- known waves: sine, saw, square, a saw -> square sweep
const S = 2048;
{
  const sine = convert(frame(S, [[1, 0.9]]), { frameSize: S });
  ok(sine.data[0] === 255 && sine.data.subarray(1, 64).every((b) => b === 0) && sine.sourceFrames === 1 && sine.lost.singleFrame, "sine, one frame: harmonic 1 = 255, the other 63 = 0, all 19 frames alike");
  ok([...Array(19).keys()].every((f) => sine.data.subarray(f * 64, f * 64 + 64).every((b, k) => b === sine.data[k])), "..and every one of the 19 frames is the same");
  const saw = convert(frame(S, Array.from({ length: 1000 }, (_, i) => [i + 1, 0.5 / (i + 1)])), { frameSize: S });
  let e = 0, n = 0; for (let k = 1; k <= 64; k++) { e = Math.max(e, Math.abs(saw.data[k - 1] - expectByte(1 / k))); n++; }
  ok(e <= 1 && saw.data[0] === 255 && saw.data[1] === 239 && saw.data[63] === 159, `saw (1/k to 1000): bytes = 255 + 16 log2(1/k) within 1 (worst ${e}); harmonic 2 = 239, 64 = 159`);
  // the energy above harmonic 64: sum 1/k^2 (65..1000) / sum (1..1000) = 0.0094, -20.3 dB
  let tot = 0, hi = 0; for (let k = 1; k <= 1000; k++) { tot += 1 / (k * k); if (k > 64) hi += 1 / (k * k); }
  ok(Math.abs(saw.lost.aboveDbWorst - 10 * Math.log10(hi / tot)) < 0.2 && saw.lost.sourceHarmonics === 1023, `lost: a saw loses ${saw.lost.aboveDbWorst.toFixed(1)} dB of its energy above harmonic 64 (analytic ${(10 * Math.log10(hi / tot)).toFixed(1)}), file holds 1023 harmonics`);
  const sq = convert(frame(S, Array.from({ length: 500 }, (_, i) => [2 * i + 1, 0.5 / (2 * i + 1)])), { frameSize: S });
  let eo = 0, ev = 0; for (let k = 1; k <= 64; k++) { if (k % 2) eo = Math.max(eo, Math.abs(sq.data[k - 1] - expectByte(1 / k))); else ev = Math.max(ev, sq.data[k - 1]); }
  ok(eo <= 1 && ev === 0 && sq.data[2] === expectByte(1 / 3), `square: odd harmonics 255 + 16 log2(1/k) within ${eo}, the even ones all 0`);
  // phase does not matter: the same saw with every harmonic at a random phase
  let seed = 7; const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff * 6.28;
  const ph = Array.from({ length: 1001 }, rnd), sawp = convert(frame(S, Array.from({ length: 1000 }, (_, i) => [i + 1, 0.5 / (i + 1)]), (k) => ph[k]), { frameSize: S });
  ok(maxDiff(sawp.data, saw.data, 0, 64) === 0, "phases dropped: a saw with random harmonic phases converts to exactly the same bytes");
  // the sweep: 19 frames, saw -> square (the even harmonics x (1 - f / 18)), picked 1:1 from a file of 19 frames
  const wave = new Float64Array(19 * S);
  for (let f = 0; f < 19; f++) wave.set(frame(S, Array.from({ length: 500 }, (_, i) => [i + 1, 0.5 / (i + 1) * ((i + 1) % 2 ? 1 : 1 - f / 18)])), f * S);
  const sw = convert(wave, { frameSize: S });
  let es = 0, bad = 0;
  for (let f = 0; f < 19; f++) for (let k = 1; k <= 64; k++) {
    const r = (1 / k) * (k % 2 ? 1 : 1 - f / 18), want = 20 * Math.log10(r) < -66 ? 0 : expectByte(r), got = sw.data[f * 64 + k - 1];
    if (want === 0) bad += got > 3 ? 1 : 0; else es = Math.max(es, Math.abs(got - want));
  }
  ok(es <= 1 && bad === 0 && sw.sourceFrames === 19 && !sw.lost.interpolated, `saw -> square sweep (19 frames of 2048): all 1216 bytes within ${es} of the analytic value; the last frame has no even harmonics`);
  ok(sw.data.slice(18 * 64, 19 * 64).every((b, i) => (i % 2 ? b === 0 : b > 0)), "..frame 18 is a square (even harmonics 0), frame 0 a saw");
}

// ---- a Serum-like file: 256 frames of 2048, a clm chunk, 16 bit
{
  const N = 256, wave = new Float64Array(N * S);
  const partial = (k, m) => 0.4 / k * (k % 2 ? 1 : 1 - m) * (k < 30 + 400 * m ? 1 : 0.3);    // frame m = i / 255: even harmonics fade out, the top opens up
  for (let i = 0; i < N; i++) wave.set(frame(S, Array.from({ length: 300 }, (_, j) => [j + 1, partial(j + 1, i / 255)])), i * S);
  const bytes = writeWav(wave, { clm: "<!>2048 20000000 wavetable (www.surge-synth-team.org)" });
  const r = convertWav(bytes);
  ok(r.frameSize === 2048 && r.sourceFrames === 256 && r.lost.interpolated && r.wav.clmFrameSize === 2048, "Serum-like 2048 x 256: the frame size comes from the clm chunk, 256 source frames, in-between ones interpolated");
  const want = (m, k) => 1 / k * (k % 2 ? 1 : 1 - m) * (k < 30 + 400 * m ? 1 : 0.3);
  const check = (f, m) => { let e = 0, z = 0; const top = Math.max(...Array.from({ length: 64 }, (_, k) => want(m, k + 1)));
    for (let k = 1; k <= 64; k++) { const w = want(m, k) / top; if (20 * Math.log10(w || 1e-9) >= -60) e = Math.max(e, Math.abs(r.data[f * 64 + k - 1] - expectByte(w))); else z++; }
    return e; };
  const e0 = check(0, 0), e18 = check(18, 1);
  ok(e0 <= 1 && e18 <= 1, `..frame 0 is source frame 0 and frame 18 is source frame 255 (bytes within ${e0} and ${e18} of the analytic ones)`);
  // an in-between frame (f = 9 -> source position 127.5): the mix of the two neighbours' magnitudes
  const m9 = 127.5 / 255, a = Array.from({ length: 64 }, (_, k) => (want(127 / 255, k + 1) + want(128 / 255, k + 1)) / 2), top = Math.max(...a);
  let e9 = 0; for (let k = 1; k <= 64; k++) if (20 * Math.log10(a[k - 1] / top) > -50) e9 = Math.max(e9, Math.abs(r.data[9 * 64 + k - 1] - expectByte(a[k - 1] / top)));
  ok(e9 <= 1, `..frame 9 (source position 127.5) is the mix of the neighbouring frames' magnitudes (within ${e9} byte)`);
  const nof = convertWav(writeWav(wave));
  ok(nof.frameSize === 2048 && same(nof.data, r.data), "without the clm chunk: a whole number of 2048 frames is taken as 2048 frames, the same bytes");
  const hand = convertWav(writeWav(wave), { frameSize: 1024 });
  ok(hand.sourceFrames === 512 && hand.frameSize === 1024, "a user-chosen frame size (1024) overrides the guess: 512 frames");
  const note = lostNote(r);
  ok(note[0] === LOST_NOTE && note.some((l) => /256 frames of 2048 samples became 19/.test(l)) && note.some((l) => /Harmonics 65 to 1023 are dropped/.test(l)) && note.some((l) => /DC/.test(l)), "the 'what is lost' note: the fixed sentence, 256 -> 19 frames, harmonics 65..1023, level steps and DC");
  console.log("   " + note.join("\n   "));
}
function same(a, b) { return a.length === b.length && a.every((v, i) => v === b[i]); }

// ---- a factory table's render, converted back (tests/wavetable_fixture.c: what the unit plays, and the recipe's bytes)
{
  const fx = JSON.parse(readFileSync(new URL("./fixtures/wavetable_factory.json", import.meta.url), "utf8"));
  const unb64 = (s) => Uint8Array.from(Buffer.from(s, "base64"));
  for (const t of fx.tables) {
    const pcm = new Int8Array(unb64(t.pcm).buffer), want = unb64(t.bytes), x = Float64Array.from(pcm, (v) => v / 128);
    const r = convert(x, { frameSize: 256, floorDb: -48 });             // 8 bit samples: a noise floor near -53 dB (narrow pulses) .. -68 dB, so the floor is raised
    let e = 0, n = 0, big = 0, sum = 0, zeros = 0, miss = 0;
    for (let i = 0; i < want.length; i++) {
      if (want[i] >= 255 - 16 * Math.log2(50)) { const d = Math.abs(r.data[i] - want[i]); e = Math.max(e, d); sum += d; n++; }        // harmonics within 34 dB of their frame's strongest
      if (want[i] === 0 && r.data[i] === 0) zeros++;
      if (want[i] >= 255 - 16 * Math.log2(200) && r.data[i] === 0) miss++;     // above -46 dB (the floor is -48): never lost
    }
    const wz = want.filter((b) => b === 0).length;
    ok(e <= 5 && miss === 0 && zeros >= wz * 0.9, `${t.name.padEnd(5)}: render -> converter -> bytes vs the recipe's bytes: ${n} harmonics within 34 dB, worst ${e} (${(e * 0.376).toFixed(2)} dB), mean ${(sum / n).toFixed(2)}; ${zeros} of ${wz} silent ones stay 0`);
    // and the other way: the converter's bytes decoded and rendered are the table again (sine phase, the same peak shape)
    const back = spectraOf(r.data);
    const cyc = renderCycle(r.data, 0, 256), orig = Float64Array.from(pcm.subarray(0, 256), (v) => v), pk = Math.max(...orig.map(Math.abs));
    const wd = maxDiff(cyc.map((v) => v * pk), orig) / pk, hb = harmonics(orig), hc = harmonics(cyc.map((v) => v * pk));
    const sp = maxDiff(hb.map((v) => v / Math.max(...hb)), hc.map((v) => v / Math.max(...hc)));
    if (t.name === "PWM") ok(sp < 0.02 && wd > 0.3, `${t.name.padEnd(5)}: its recipe has negative harmonics (phase flips): frame 0 rendered from the bytes has the same spectrum (worst ${sp.toFixed(3)}) but another shape (${(wd * 100).toFixed(0)} % off): phases are not kept`);
    else ok(wd < 0.05 && sp < 0.02, `${t.name.padEnd(5)}: frame 0 rendered from the bytes = the unit's own frame 0 within 5 % of its peak (worst ${(wd * 100).toFixed(1)} %), spectrum within ${sp.toFixed(3)}`);
    void back;
  }
}

// ---- the object, the name
{
  const data = Uint8Array.from({ length: WT.BYTES }, (_, i) => (i * 13) & 255), o = buildObject("my table!", data);
  ok(o.length === 1228 && o[0] === 1 && o[1] === 19 && o[2] === 64 && o[3] === 0 && String.fromCharCode(...o.slice(4, 12).filter(Boolean)) === "MY TABLE" && same(o.slice(12), data), "object: version 1, 19, 64, flags 0, the name cleaned to MY TABLE (8 characters, upper case), then the 1216 bytes");
  ok(cleanName("  aéb/c~d  long name") === "ABCD LON" && cleanName(null) === "" && cleanName("x".repeat(20)).length === 8, "name: upper case, only A-Z 0-9 space . + # -, trimmed, 8 characters");
  ok(checkObject(o) === null && parseObject(o).name === "MY TABLE" && checkObject(o.slice(0, 1227)) !== null && checkObject(buildObject("", new Uint8Array(1216))) !== null, "object: checks (size, version, an all-zero table); parse returns name and data");
}

// ---- the protocol client against the simulated unit
{
  const u = new MockUnit({ seed: false }), rq = (r, o) => u.request(r, o);
  const l0 = await listTables(rq);
  ok(l0.slots === 3 && l0.tables.every((t) => !t.used && t.name === "" && t.crc === 0), "client: LIST of a fresh unit: 3 slots, all empty");
  const data = convert(frame(S, Array.from({ length: 300 }, (_, i) => [i + 1, 0.5 / (i + 1)])), { frameSize: S }).data;
  const seen = []; await writeTable(rq, 1, "Test saw", data, (d, t) => seen.push(d / t));
  const l1 = await listTables(rq);
  ok(l1.tables[1].used && l1.tables[1].name === "TEST SAW" && l1.tables[1].crc === bkCrc(data) && !l1.tables[0].used && seen.at(-1) === 1, "client: write USR2 (pieces of 112, progress to 100 %), LIST shows its name and CRC");
  const rb = await readTable(rq, 1);
  ok(same(rb.data, data) && rb.name === "TEST SAW", "client: read it back bit for bit");
  ok(u.objs.get(25).length === 1228 && same(u.objs.get(25), buildObject("TEST SAW", data)), "unit: the backup object 25 is the same bytes");
  u.playing = true;
  ok(await athrows(() => writeTable(rq, 0, "X", data), /playing/) && (await listTables(rq)).tables[0].used === false, "client: the unit is playing: rc 3 at begin, plain message, nothing written");
  ok(await athrows(() => eraseTable(rq, 1), /playing/) && (await listTables(rq)).tables[1].used, "client: erase while playing is refused too");
  u.playing = false;
  u.fail = { cmd: 83, op: 2, rc: 4 };
  ok(await athrows(() => writeTable(rq, 2, "X", data), /flash/) && !(await listTables(rq)).tables[2].used, "client: a flash failure at commit (rc 4) is reported; the slot stays empty");
  u.fail = null;
  u.fail = { cmd: 83, op: 1, rc: 5 };
  const before = u.log.length; await athrows(() => writeTable(rq, 2, "X", data), /expired/);
  ok(u.log.length > before && u.wtStage === null, "client: an error in the middle aborts the transfer on the unit");
  u.fail = null;
  ok(await athrows(() => writeTable(rq, 0, "X", new Uint8Array(1216)), /invalid/), "client: an all-zero table is refused by the unit (rc 2)");
  await eraseTable(rq, 1);
  ok(!(await listTables(rq)).tables[1].used && !u.objs.has(25) && await athrows(() => readTable(rq, 1), /empty/), "client: erase empties the slot");
  ok(throwsMsg(() => parseList([3, 19, 63]), /not 3 x 19 x 64/), "client: a unit with another table shape is refused");
  // the unit's caps: a unit without bit 3 does not take the commands
  const old = new MockUnit({ caps: 7 });
  ok(await athrows(() => listTables((r, o) => old.request(r, o)), /unexpected command/), "client: a unit without caps bit 3 does not know the commands (the editor must check INFO first)");
}

console.log(fails ? `WAVETABLE TESTS FAILED (${fails})` : "wavetable tests passed");
process.exit(fails ? 1 : 0);
