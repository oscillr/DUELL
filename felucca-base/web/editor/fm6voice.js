// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the FM6 engine's voice format (the generic 6-operator voice) and the DX7 sysex files that carry it. A port of the FM6 object of the older
// web/editor.html (eng_fm6.c fm6_pack / fm6_unpack are the reference), including wip/dx7fix's rule that only the corrupt voice of a bank is left out.
// voice: 155 bytes, 6 x 21 operator bytes (the sixth operator first, as the format has them), then 19 voice bytes and a 10-character name.
// packed: the 128-byte record of a 32-voice bank (every byte 7-bit). SysEx files: a single voice (F0 43 0n 00 01 1B, 155 bytes, checksum, F7: 163 bytes) or 32 packed
// voices (F0 43 0n 09 20 00, 4096 bytes, checksum, F7: 4104 bytes). No DOM, no I/O.
export const FM6 = (() => {
  /* per operator byte: name, highest value; then the voice bytes */
  const OP = [["R1", 99], ["R2", 99], ["R3", 99], ["R4", 99], ["L1", 99], ["L2", 99], ["L3", 99], ["L4", 99],
    ["BP", 99], ["LD", 99], ["RD", 99], ["LC", 3], ["RC", 3], ["RS", 7], ["AMS", 3], ["KVS", 7], ["OL", 99],
    ["MODE", 1], ["FC", 31], ["FF", 99], ["DET", 14]];
  const VOICE = [["PR1", 99], ["PR2", 99], ["PR3", 99], ["PR4", 99], ["PL1", 99], ["PL2", 99], ["PL3", 99], ["PL4", 99],
    ["ALG", 31], ["FB", 7], ["OKS", 1], ["LFS", 99], ["LFD", 99], ["LPMD", 99], ["LAMD", 99], ["LKS", 1], ["LFW", 5],
    ["LPMS", 7], ["TRNSP", 48]];
  const OPI = Object.fromEntries(OP.map(([n], i) => [n, i])), VI = Object.fromEntries(VOICE.map(([n], i) => [n, 126 + i]));
  const NAME = 145, SIZE = 155, PACKED = 128;
  const max = (i) => (i < 126 ? OP[i % 21][1] : i < NAME ? VOICE[i - 126][1] : 126);
  /* byte i of operator n (1..6) */
  const at = (n, field) => (6 - n) * 21 + OPI[field];
  function sanitize(v) {
    const o = Uint8Array.from(v.slice(0, SIZE));
    for (let i = 0; i < SIZE; i++) o[i] = i >= NAME ? (o[i] < 32 || o[i] > 126 ? 32 : o[i]) : Math.min(o[i], max(i));
    return o;
  }
  function unpack(b) {                            /* eng_fm6.c fm6_unpack */
    const v = new Uint8Array(SIZE);
    for (let k = 0; k < 6; k++) {
      const o = k * 17, d = k * 21;
      for (let i = 0; i < 11; i++) v[d + i] = b[o + i] & 127;
      v[d + 11] = b[o + 11] & 3; v[d + 12] = b[o + 11] >> 2 & 3; v[d + 13] = b[o + 12] & 7; v[d + 20] = b[o + 12] >> 3 & 15;
      v[d + 14] = b[o + 13] & 3; v[d + 15] = b[o + 13] >> 2 & 7; v[d + 16] = b[o + 14] & 127; v[d + 17] = b[o + 15] & 1;
      v[d + 18] = b[o + 15] >> 1 & 31; v[d + 19] = b[o + 16] & 127;
    }
    for (let i = 0; i < 9; i++) v[126 + i] = b[102 + i] & 127;
    v[134] &= 31; v[135] = b[111] & 7; v[136] = b[111] >> 3 & 1;
    for (let i = 0; i < 4; i++) v[137 + i] = b[112 + i] & 127;
    v[141] = b[116] & 1; v[142] = b[116] >> 1 & 7; v[143] = b[116] >> 4 & 7; v[144] = b[117] & 127;
    for (let i = 0; i < 10; i++) v[NAME + i] = b[118 + i] & 127;
    return sanitize(v);
  }
  function pack(v) {                              /* eng_fm6.c fm6_pack */
    const b = new Array(PACKED).fill(0);
    for (let k = 0; k < 6; k++) {
      const o = k * 17, d = k * 21;
      for (let i = 0; i < 11; i++) b[o + i] = v[d + i] & 127;
      b[o + 11] = (v[d + 11] & 3) | (v[d + 12] & 3) << 2; b[o + 12] = (v[d + 13] & 7) | (v[d + 20] & 15) << 3;
      b[o + 13] = (v[d + 14] & 3) | (v[d + 15] & 7) << 2; b[o + 14] = v[d + 16] & 127;
      b[o + 15] = (v[d + 17] & 1) | (v[d + 18] & 31) << 1; b[o + 16] = v[d + 19] & 127;
    }
    for (let i = 0; i < 9; i++) b[102 + i] = v[126 + i] & 127;
    b[110] &= 31; b[111] = (v[135] & 7) | (v[136] & 1) << 3;
    for (let i = 0; i < 4; i++) b[112 + i] = v[137 + i] & 127;
    b[116] = (v[141] & 1) | (v[142] & 7) << 1 | (v[143] & 7) << 4; b[117] = v[144] & 127;
    for (let i = 0; i < 10; i++) b[118 + i] = v[NAME + i] & 127;
    return b;
  }
  const name = (v) => String.fromCharCode(...v.slice(NAME, NAME + 10)).replace(/\s+$/, "");
  function setName(v, s) {
    const t = String(s).toUpperCase().replace(/[^\x20-\x7E]/g, " ").slice(0, 10).padEnd(10, " ");
    for (let i = 0; i < 10; i++) v[NAME + i] = t.charCodeAt(i);
    return v;
  }
  const checksum = (a) => (128 - (a.reduce((s, x) => s + x, 0) & 127)) & 127;
  /* every voice in a file's bytes, tolerant of what real files have:
     - SysEx F0 43 0n 00 xx xx (one voice, 155 bytes) and F0 43 0n 09 xx xx (32 packed voices, 4096 bytes): the byte
       count is not checked (some tools write 09 10 00, i.e. 0x1000 as two plain bytes); the payload is taken right
       after the 6-byte header whatever follows it: a checksum or not, F7 or not, a stray byte, the next F0;
       a message cut short (EOF, or a byte >= 0x80 inside it) keeps its whole voices (the name may be cut: blanks);
     - several messages in one file, junk before / between / after them;
     - the format's other blocks (manufacturer 43h: supplements, performances, 4-operator voices, universal "LM  ..." blocks) and other
       makers' messages are skipped and counted;
     - raw data without SysEx: 4096 x n bytes (banks), 155 (one voice) or 128 bytes (one packed voice).
     -> {voices: [{name, v}], badSum: messages with a wrong checksum (still read), short: messages cut short,
         skipped: other messages, corrupt: [{index (1..32), name}] voices of a complete bank left out because
         their 128 bytes hold a byte >= 0x80 (the other voices are kept), kinds: what the skipped ones were ("fm4" 4-operator voices, "other43" the format's other
         blocks, "universal", "maker:XX" another manufacturer id), sysex: F0 seen} */
  function parseSysex(bytes) {
    const d = Uint8Array.from(bytes), voices = [], kinds = [], corrupt = [];
    let badSum = 0, short = 0, skipped = 0;
    const add = (v) => voices.push({ v, name: name(v) });
    const kind = (k) => { skipped++; if (!kinds.includes(k)) kinds.push(k); };
    const sysex = d.includes(0xF0);
    if (d[0] !== 0xF0 && !sysex) {
      if (d.length && d.length % 4096 === 0) {
        for (let o = 0; o < d.length; o += 128) add(unpack(d.subarray(o, o + 128)));
      } else if (d.length === SIZE) add(sanitize(d));
      else if (d.length === PACKED) add(unpack(d));
      return { voices, badSum, short, skipped, kinds, sysex, corrupt };
    }
    for (let i = 0; i < d.length; i++) {
      if (d[i] !== 0xF0) continue;
      const id = d[i + 1], st = d[i + 2], f = d[i + 3];
      if (id === 0x43 && st !== undefined && (st & 0xF0) === 0 && (f === 0 || f === 9) && i + 6 <= d.length) {
        const want = f === 0 ? SIZE : 4096, s = i + 6;
        let e = s;
        while (e < d.length && e < s + want && d[e] < 0x80) e++;
        /* a bank whose 4096 bytes are all there and closed (checksum, F7 or the end of the file) but with a byte >= 0x80 inside has a corrupt
         * voice, not a short message: the records sit at fixed 128-byte offsets, so only the voices that hold such a byte are left out */
        const w = s + want, closed = f === 9 && e < w && w <= d.length
          && (w === d.length || d[w] === 0xF7 || (d[w] < 0x80 && (w + 1 >= d.length || d[w + 1] === 0xF7)));
        if (closed) {
          const data = d.subarray(s, w);
          if (d[w] < 0x80 && w < d.length && checksum(data) !== d[w]) badSum++;
          for (let o = 0; o < want; o += 128) {
            const rec = data.subarray(o, o + 128);
            if (rec.some((x) => x >= 0x80)) {
              const nm = String.fromCharCode(...Array.from(rec.subarray(118, 128), (x) => ((x & 0x7F) < 32 || (x & 0x7F) > 126 ? 32 : x & 0x7F))).trim();
              corrupt.push({ index: o / 128 + 1, name: nm });
            } else add(unpack(rec));
          }
          i = w - 1;
          continue;
        }
        const data = d.subarray(s, e);
        if (data.length < want) short++;
        else if (e < d.length && d[e] < 0x80 && checksum(data) !== d[e]) badSum++;
        if (f === 0) {
          if (data.length >= NAME) { const v = new Uint8Array(SIZE); v.set(data); add(sanitize(v)); }
        } else {
          for (let o = 0; o + 118 <= data.length; o += 128) {    /* whole voices, and one cut in its name (at 118) */
            const pk = new Uint8Array(128); pk.set(data.subarray(o, o + 128)); add(unpack(pk));
          }
        }
        i = e - 1;                      /* on to the next F0 (the checksum, F7, junk are passed over) */
        continue;
      }
      if (id === 0x43) kind(f === 3 || f === 4 ? "fm4" : "other43");
      else if (id === 0x7E || id === 0x7F) kind("universal");
      else if (id !== undefined && id < 0x80) kind("maker:" + (id === 0 ? [0, d[i + 2], d[i + 3]] : [id])
        .map((x) => (x ?? 0).toString(16).toUpperCase().padStart(2, "0")).join(" "));
    }
    return { voices, badSum, short, skipped, kinds, sysex, corrupt };
  }
  function singleSysex(v, ch = 0) {
    const data = Array.from(sanitize(v));
    return Uint8Array.from([0xF0, 0x43, ch & 15, 0, 0x01, 0x1B, ...data, checksum(data), 0xF7]);
  }
  function bankSysex(voices, ch = 0) {            /* up to 32 voices (155 bytes each), the rest the init voice */
    const data = [];
    for (let k = 0; k < 32; k++) data.push(...(voices[k] ? pack(voices[k]) : INIT_PK));
    return Uint8Array.from([0xF0, 0x43, ch & 15, 9, 0x20, 0, ...data, checksum(data), 0xF7]);
  }
  const INIT_PK = [99,99,99,99,99,99,99,0,39,0,0,0,56,0,0,2,0,99,99,99,99,99,99,99,0,39,0,0,0,56,0,0,2,0,99,99,99,99,99,99,99,0,39,0,0,0,56,0,0,2,0,99,99,99,99,99,99,99,0,39,0,0,0,56,0,0,2,0,99,99,99,99,99,99,99,0,39,0,0,0,56,0,0,2,0,99,99,99,99,99,99,99,0,39,0,0,0,56,0,99,2,0,99,99,99,99,50,50,50,50,0,8,35,0,0,0,49,24,73,78,73,84,32,86,79,73,67,69];
  return { OP, VOICE, OPI, VI, NAME, SIZE, PACKED, max, at, sanitize, unpack, pack, name, setName, checksum, parseSysex, singleSysex, INIT_PK };
})();
