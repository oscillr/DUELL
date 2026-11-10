// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: DX7 sysex files (a voice of 155 bytes "VCED", a bank of 32 voices of 4096 bytes "VMEM") -> DUELL patches of the FM6 engine.
// The FM6 engine plays the generic 6-operator voice; a patch of the store carries its own copy of it (record flag FM6, SLOT 8 = OWN), so a converted
// voice is an ordinary FM6 patch. Everything else in the patch is the owner's neutral starting point (a DX7 knows nothing of our sections):
//   the effects and sends dry but a little reverb (REV 7 %), SAT 0, EQ flat, a gentle MULTI FILTER (12 dB low-pass, 6.8 kHz, a trace of resonance: it takes the
//   digital edge off the top, as the factory FM BASS 2 does), POLY and the house LEVEL (a bass voice is MONO), no arp, no keys, no morph block.
// A patch is never called "DX7": the name is the voice's own, cleaned; "DX7 sysex import" is only the label of where it came from. No DOM.
import { L, P_E0, ID } from "./layout.js";
import { FM6 } from "./fm6voice.js";
import { buildRecord, engineIndex, CATEGORIES } from "./patchrecord.js";

export const SOURCE_LABEL = "DX7 sysex import";
export const NEUTRAL = { SLOT_OWN: 8, UF_CUT: 71, UF_RES: 7, UF_MODE: 0, UF_SLOPE: 1, REV_PERCENT: "7", MONO: 1, POLY: 0 };

// the REV send value whose text is "7" (%): the lowest one (layout.json FMTS)
function revSend() {
  const d = L.TP[36], t = L.FMTS[d.fk];
  const v = t.findIndex((x) => x[0] === NEUTRAL.REV_PERCENT && x[1] === "%");
  return v < 0 ? 9 : d.min + v;
}

// "Piano 1 " -> "PIANO 1": printable ASCII, upper case, runs of blanks one, trimmed; a name that says DX7 loses that word; empty: FM VOICE nn
export function cleanVoiceName(raw, n = 0) {
  let s = String(raw ?? "").replace(/[^\x20-\x7e]/g, " ").toUpperCase();
  s = s.replace(/\bDX\s*7\b[-:. ]*/g, " ").replace(/\s+/g, " ").trim().slice(0, 12).trim();
  return s || `FM VOICE ${String(n + 1).padStart(2, "0")}`;
}

// a guess at the category from the voice's name (the owner's list: BASS LEAD PAD PLUCK KEYS DRUM FX OTHER); OTHER when nothing says
const GUESS = [
  ["BASS", /BASS|\bBS\b|SUB\b|WOBBLE/], ["DRUM", /KICK|SNARE|TOM\b|HIHAT|HI-HAT|HAT\b|DRUM|PERC|CLAP|CONGA|BONGO|TABLA/],
  ["FX", /\bFX\b|NOISE|WIND|EXPLO|LASER|ZAP|SPACE|RISER|SWEEP|ALIEN|WHOOSH|STORM|RAIN/], ["PAD", /PAD|STRING|STRNG|CHOIR|VOX|VOICE|SWELL|ATMOS|AMBI|SYNTH ?PAD|WARM|SLOW|FADE/],
  ["PLUCK", /BELL|CHIME|GLASS|MARIMBA|VIBE|KALIMBA|XYLO|GLOCK|HARP|GUITAR|KOTO|BANJO|PLUCK|MALLET|STEEL|SITAR|PIZZ|TUBULAR|DULCIMER|BLIP|CYCLE/],
  ["KEYS", /PIANO|\bE\.?P(IANO)?\b|RHODES|WURLI|CLAV|ORGAN|KEYS|HARPSI|CELESTE|TINE|ORCH|ACCORD/], ["LEAD", /LEAD|SOLO|FLUTE|BRASS|TRUMP|SAX|HORN|OBOE|CLARI|TROMB|REED|WHISTLE|SQUARE|SAW/],
];
export function guessCategory(name) {
  const s = String(name).toUpperCase();
  for (const [c, re] of GUESS) if (re.test(s)) return CATEGORIES.indexOf(c);
  return CATEGORIES.indexOf("OTHER");
}

// the values (P_COUNT) of the neutral FM6 patch. mono: the voice mode (a BASS voice)
export function neutralValues(mono = false) {
  const eng = engineIndex("FM6"), v = Array.from({ length: L.P_COUNT }, (_, i) => (i >= P_E0 ? L.ENG[eng].edit[i - P_E0].def : L.TP[i].def));
  v[P_E0 + 7] = NEUTRAL.SLOT_OWN;                               // SLOT: OWN, the patch the record carries
  v[ID.P_FREEA ?? 45] = NEUTRAL.UF_CUT; v[46] = NEUTRAL.UF_RES; v[47] = NEUTRAL.UF_MODE; v[48] = NEUTRAL.UF_SLOPE;   // MULTI FILTER CUT RES MODE SLOPE
  v[36] = revSend();                                              // REV send; SAT (33), CHO (34), DLY (35) stay 0; the EQ stays flat
  v[ID.P_VOICE] = mono ? NEUTRAL.MONO : NEUTRAL.POLY;
  return v;
}

// one voice (155 bytes, as FM6.parseSysex gives it) -> { name, cat, catGuessed, record (Uint8Array), values, patch128 }
export function voiceToPatch(voice, index = 0) {
  const v = FM6.sanitize(voice), name = cleanVoiceName(FM6.name(v), index), cat = guessCategory(name), bass = CATEGORIES[cat] === "BASS";
  const values = neutralValues(bass), patch128 = Uint8Array.from(FM6.pack(v));
  const record = buildRecord({ engine: engineIndex("FM6"), name, cat, values, patch128 });
  return { name, cat, catGuessed: true, record, values, patch128, voice: v };
}

// a file's bytes -> { kind: "bank" | "single" | "none", voices: [{index (0-based position in the file), name, damaged, voice}], notes: [text] }
export function parseDx7File(bytes) {
  const r = FM6.parseSysex(bytes), notes = [];
  let voices = r.voices.map((x, i) => ({ index: i, name: x.name, damaged: false, voice: x.v }));
  if (r.corrupt.length && voices.length + r.corrupt.length === 32) {            // a bank with damaged voices: keep their places
    voices = []; let k = 0;
    for (let i = 1; i <= 32; i++) {
      const c = r.corrupt.find((x) => x.index === i);
      if (c) voices.push({ index: i - 1, name: c.name, damaged: true, voice: null });
      else { voices.push({ index: i - 1, name: r.voices[k].name, damaged: false, voice: r.voices[k].v }); k++; }
    }
    notes.push(`${r.corrupt.length} voice${r.corrupt.length > 1 ? "s are" : " is"} damaged in the file and left out.`);
  }
  if (r.badSum) notes.push("The file's checksum does not match; the voices were read anyway.");
  if (r.short) notes.push("A message in the file is cut short; its whole voices were read.");
  if (r.skipped) notes.push(`${r.skipped} other message${r.skipped > 1 ? "s" : ""} in the file ${r.skipped > 1 ? "are" : "is"} not DX7 voices and left out.`);
  return { kind: !voices.length ? "none" : voices.length === 1 ? "single" : "bank", voices, notes };
}
// is it a DX7 sysex at all (cheap test for a dropped file)
export const looksLikeDx7 = (bytes) => (bytes.length >= 163 && bytes[0] === 0xf0 && bytes[1] === 0x43) || (bytes.length > 0 && bytes.length % 4096 === 0);
