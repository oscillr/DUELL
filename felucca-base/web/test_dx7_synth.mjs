// SPDX-License-Identifier: GPL-3.0-only
// SYNTHETIC DX7 test data, generated here (no real banks are shipped): deterministic voices of our own making (a few algorithms, simple ratios, plain
// envelopes), as a 4104 byte VMEM bank (F0 43 0n 09 20 00, 32 packed voices of 128 bytes, checksum, F7) and as a single voice (VCED, 155 bytes; the
// message is F0 43 0n 00 01 1B + 155 + checksum + F7). Used by test_librarian.mjs, test_librarian_ui.mjs and (as hex) tests/fm6_test.c.
import { FM6 } from "./editor/fm6voice.js";

const KINDS = ["BASS", "PAD", "E.PIANO", "BELL", "LEAD", "KICK", "STRINGS", "ORGAN", "SUB", "BRASS", "MARIMBA", "WHISTLE"];
const ALGS = [0, 4, 7, 13, 21, 31, 17, 22, 4, 31, 13, 0];           // 0-based algorithm numbers (1, 5, 8, 14, 22, 32, 18, 23)
const COARSE = [1, 1, 2, 3, 4, 1, 2, 5, 1, 3, 6, 2];                // frequency ratios (FC; 0 would be 0.5)

// voice k (0-based, any k): 155 bytes in the VCED order (operator 6 first, then the voice bytes, then the 10 character name)
export function synthVoice(k, o = {}) {
  const v = new Uint8Array(FM6.SIZE), kind = k % KINDS.length;
  for (let n = 1; n <= 6; n++) {                                     // operator n, byte i = FM6.at(n, field)
    const set = (f, x) => { v[FM6.at(n, f)] = x; };
    const lead = n === 1, slow = kind === 1 || kind === 6;
    set("R1", lead ? (slow ? 45 : 95) : 80 - 4 * n); set("R2", 60 + 3 * n); set("R3", slow ? 30 : 45); set("R4", slow ? 35 : 55 - 2 * n);
    set("L1", 99); set("L2", lead ? 85 : 70 - 5 * n); set("L3", lead ? 75 : 40 - 3 * n); set("L4", 0);
    set("BP", 39); set("LD", 0); set("RD", 0); set("LC", 0); set("RC", 0); set("RS", n % 3); set("KVS", 2 + (n % 3));
    set("AMS", o.ams && lead ? o.ams : 0);
    set("OL", lead ? 92 : Math.max(30, 80 - 7 * n - (k % 5) * 3));
    set("MODE", 0); set("FC", n === 1 ? 1 : COARSE[(k + n) % COARSE.length]); set("FF", n === 3 ? 20 + (k % 4) * 10 : 0); set("DET", 7 + ((k + n) % 5) - 2);
  }
  const g = (f, x) => { v[FM6.VI[f]] = x; };
  g("PR1", 99); g("PR2", 99); g("PR3", 99); g("PR4", 99); g("PL1", 50); g("PL2", 50); g("PL3", 50); g("PL4", 50);
  g("ALG", ALGS[k % ALGS.length]); g("FB", k % 8); g("OKS", 1); g("LFS", 30 + (k % 7) * 5); g("LFD", 0); g("LPMD", o.lfo ? 12 : 0);
  g("LAMD", o.lamd ?? 0); g("LKS", 1); g("LFW", k % 6); g("LPMS", o.lfo ? 3 : 0); g("TRNSP", 24);
  const nn = String(k + 1).padStart(2, "0");
  FM6.setName(v, `${KINDS[kind]} ${nn}`.slice(0, 10));
  return v;
}
// 32 voices (the voices of the bank "A" are 0..31, "B" 32..63); corrupt: voice places (0-based) whose record gets bytes >= 128 (a damaged voice)
export function synthBank(set = 0, corrupt = [], ch = 0) {
  const voices = Array.from({ length: 32 }, (_, k) => synthVoice(set * 32 + k, { ams: (k % 4) === 3 ? 2 : 0, lamd: (k % 4) === 3 ? 40 : 0, lfo: k % 5 === 4 }));
  const data = voices.flatMap((x) => FM6.pack(x));
  const bytes = Uint8Array.from([0xF0, 0x43, ch & 15, 9, 0x20, 0, ...data, FM6.checksum(data), 0xF7]);
  for (const c of corrupt) for (const j of [3, 20, 44, 77, 100]) bytes[6 + c * 128 + j] |= 0x80;      // the checksum then no longer matches: the importer notes it
  return { bytes, voices };
}
// the same, but as a single voice message (163 bytes); raw155: the bare 155 bytes
export const synthSingle = (k = 4) => ({ bytes: FM6.singleSysex(synthVoice(k)), raw155: FM6.sanitize(synthVoice(k)), voice: synthVoice(k) });
