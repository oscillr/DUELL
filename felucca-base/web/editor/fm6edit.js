// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the FM6 VOICE editor (0.2.25, owner: "FM6 ... I cannot edit FM6 patches in the editor at all"). An FM6 part plays its own 6-operator
// voice (the generic 6-operator format: fm6voice.js); the eight controls of the ENGINE cards are MACROS on top of it. The voice itself travels whole:
// FM6_GET / FM6_PUT target 0 (EDITOR_PROTOCOL.md "FM6 patches"): read it, change a value here, the whole voice goes back (at most ~50 a second: unit.js
// queueWrite) and is heard at once; the unit then plays it as the part's OWN voice (VOICE SOURCE = OWN). A patch stored afterwards keeps it (UP_STORE).
// What the unit's own operator pages edit (OP TUNE, OP ENV, OP MODE) is the same bytes: the editor reads the voice again every few seconds while shown.
// T0229 (owner): no pictures in the editor; each operator's title says CARRIER or MOD (from the firmware's routing table, fm6_core.c FM6_ALG).
import { h, clear } from "./ui_util.js";
import { Cell } from "./controls.js";
import { FM6 } from "./fm6voice.js";
import { P_E0 } from "./layout.js";
import { readBytes } from "./dropzone.js";

// fm6_core.c FM6_ALG: per operator (the sixth first): bits 0..1 the bus it writes (0 = the output), bit 2 adds to it, bits 4..5 the bus it reads, bit 6 / 7 feedback in / out
export const FM6_ALG = [
  [0xc1, 0x11, 0x11, 0x14, 0x01, 0x14], [0x01, 0x11, 0x11, 0x14, 0xc1, 0x14], [0xc1, 0x11, 0x14, 0x01, 0x11, 0x14],
  [0xc1, 0x11, 0x94, 0x01, 0x11, 0x14], [0xc1, 0x14, 0x01, 0x14, 0x01, 0x14], [0xc1, 0x94, 0x01, 0x14, 0x01, 0x14],
  [0xc1, 0x11, 0x05, 0x14, 0x01, 0x14], [0x01, 0x11, 0xc5, 0x14, 0x01, 0x14], [0x01, 0x11, 0x05, 0x14, 0xc1, 0x14],
  [0x01, 0x05, 0x14, 0xc1, 0x11, 0x14], [0xc1, 0x05, 0x14, 0x01, 0x11, 0x14], [0x01, 0x05, 0x05, 0x14, 0xc1, 0x14],
  [0xc1, 0x05, 0x05, 0x14, 0x01, 0x14], [0xc1, 0x05, 0x11, 0x14, 0x01, 0x14], [0x01, 0x05, 0x11, 0x14, 0xc1, 0x14],
  [0xc1, 0x11, 0x02, 0x25, 0x05, 0x14], [0x01, 0x11, 0x02, 0x25, 0xc5, 0x14], [0x01, 0x11, 0x11, 0xc5, 0x05, 0x14],
  [0xc1, 0x14, 0x14, 0x01, 0x11, 0x14], [0x01, 0x05, 0x14, 0xc1, 0x14, 0x14], [0x01, 0x14, 0x14, 0xc1, 0x14, 0x14],
  [0xc1, 0x14, 0x14, 0x14, 0x01, 0x14], [0xc1, 0x14, 0x14, 0x01, 0x14, 0x04], [0xc1, 0x14, 0x14, 0x14, 0x04, 0x04],
  [0xc1, 0x14, 0x14, 0x04, 0x04, 0x04], [0xc1, 0x05, 0x14, 0x01, 0x14, 0x04], [0x01, 0x05, 0x14, 0xc1, 0x14, 0x04],
  [0x04, 0xc1, 0x11, 0x14, 0x01, 0x14], [0xc1, 0x14, 0x01, 0x14, 0x04, 0x04], [0x04, 0xc1, 0x11, 0x14, 0x04, 0x04],
  [0xc1, 0x14, 0x04, 0x04, 0x04, 0x04], [0xc4, 0x04, 0x04, 0x04, 0x04, 0x04]];
// algorithm a (0..31) as a graph: { carriers: [op numbers 1..6], edges: [[from, to]] (from modulates to), feedback: op number or 0 }
export function algGraph(a) {
  const t = FM6_ALG[a & 31], bus = [[], [], []], edges = [], carriers = [];
  let feedback = 0;
  for (let k = 0; k < 6; k++) {
    const f = t[k], op = 6 - k, out = f & 3, inb = (f >> 4) & 3;
    if (inb) for (const m of bus[inb]) edges.push([m, op]);
    if ((f & 0xc0) === 0xc0) feedback = op;
    if (out === 0) carriers.push(op);
    else bus[out] = f & 4 ? [...bus[out], op] : [op];
  }
  if (!feedback) { const i = t.findIndex((f) => f & 0x40); const o = t.findIndex((f) => f & 0x80); if (i >= 0 && o >= 0) feedback = 6 - i; }
  return { carriers: carriers.sort(), edges, feedback };
}
// an operator's frequency as the unit plays it: RATIO coarse (0 = 0.5) x (1 + fine / 100); FIXED 10 ^ (coarse mod 4) x 10 ^ (fine / 100) Hz
export function opFreq(mode, coarse, fine) {
  if (mode === 1) { const f = 10 ** (coarse & 3) * 10 ** (fine / 100); return `${f < 100 ? f.toFixed(2) : f.toFixed(0)} Hz`; }
  return `x ${((coarse || 0.5) * (1 + fine / 100)).toFixed(2)}`;
}
const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const bpName = (v) => { const n = v + 21; return `${NOTE[n % 12]}${Math.floor(n / 12) - 1}`; };

// the controls: [field, long label, tooltip, min, max, names | null, to-shown (v) -> number | null]
const OPC = [
  ["MODE", "FREQUENCY MODE", "RATIO follows the played note; FIXED stays at one frequency.", ["RATIO", "FIXED"]],
  ["FC", "COARSE", "Frequency ratio (0 is 0.5) or, in FIXED, the decade (1, 10, 100, 1000 Hz)."],
  ["FF", "FINE", "Fine frequency: up to almost double the coarse value."],
  ["DET", "DETUNE", "A small offset against the other operators (-7 .. +7).", null, -7],
  ["OL", "OUTPUT LEVEL", "Loudness of a carrier, or brightness of a modulator (0 is silent)."],
  ["KVS", "VELOCITY SENSITIVITY", "How much the key velocity changes the output level."],
  ["AMS", "AMP MODULATION SENSITIVITY", "How much the LFO's amplitude depth moves this operator."],
  ["RS", "RATE SCALING", "Higher notes make the envelope faster."],
];
const ENVC = [["R1", "RATE 1 (ATTACK)"], ["R2", "RATE 2"], ["R3", "RATE 3"], ["R4", "RATE 4 (RELEASE)"], ["L1", "LEVEL 1"], ["L2", "LEVEL 2"], ["L3", "LEVEL 3 (HELD)"], ["L4", "LEVEL 4 (END)"]];
const KLS = [["BP", "BREAK POINT", "The note where the level scaling starts."], ["LD", "LEFT DEPTH", "Level change below the break point."], ["RD", "RIGHT DEPTH", "Level change above the break point."],
  ["LC", "LEFT CURVE", "Shape of the change below the break point.", ["-LINEAR", "-EXPONENTIAL", "+EXPONENTIAL", "+LINEAR"]], ["RC", "RIGHT CURVE", "Shape of the change above the break point.", ["-LINEAR", "-EXPONENTIAL", "+EXPONENTIAL", "+LINEAR"]]];
const VC = {
  alg: [["FB", "FEEDBACK", "The feedback operator's loop (0 to 7)."], ["OKS", "OSCILLATOR KEY SYNC", "ON: every note starts the operators at the same phase.", ["OFF", "ON"]],
    ["TRNSP", "TRANSPOSE", "The whole voice in semitones (-24 .. +24).", null, -24]],
  pitch: [["PR1", "RATE 1"], ["PR2", "RATE 2"], ["PR3", "RATE 3"], ["PR4", "RATE 4 (RELEASE)"], ["PL1", "LEVEL 1"], ["PL2", "LEVEL 2"], ["PL3", "LEVEL 3 (HELD)"], ["PL4", "LEVEL 4 (END)"]],
  lfo: [["LFW", "WAVEFORM", "Shape of the voice's LFO.", ["TRIANGLE", "SAW DOWN", "SAW UP", "SQUARE", "SINE", "SAMPLE AND HOLD"]], ["LFS", "SPEED", "Speed of the LFO."],
    ["LFD", "DELAY", "Time before the LFO starts after a note."], ["LPMD", "PITCH DEPTH", "Vibrato depth."], ["LAMD", "AMP DEPTH", "Tremolo depth (on the operators with AMP MODULATION SENSITIVITY)."],
    ["LKS", "KEY SYNC", "ON: every note restarts the LFO.", ["OFF", "ON"]], ["LPMS", "PITCH SENSITIVITY", "How much the pitch depth acts (0 to 7)."]],
};

// the editor of part p's FM6 voice: { el, refresh(), destroy() }. opt: { onEdit(byte, oldV, newV) (undo), say }
export function createFm6Editor(app, p, opt = {}) {
  const u = app.unit, say = opt.say || app.say;
  let voice = null, loading = false, timer = 0, destroyed = false, lastPacked = "", pending = 0;
  const cells = [], root = h("div", { class: "fm6ed" });
  const status = h("p", { class: "note" }), nameIn = h("input", { type: "text", maxlength: 10, size: 11, "aria-label": "FM6 voice name", spellcheck: "false" });
  const byte = (field, op) => (op ? FM6.at(op, field) : FM6.VI[field]);
  const put = (i, v) => {
    if (!voice) return;
    const was = voice[i]; v = Math.max(0, Math.min(FM6.max(i), Math.round(v)));
    if (was === v) return;
    voice[i] = v; send(); opt.onEdit?.(i, was, v); refresh();
  };
  function send() { const pk = FM6.pack(voice); lastPacked = pk.join(); pending = Date.now(); try { u.writeFm6(p, pk); } catch (e) { app.fail(e); } }
  const mkCell = (i, label, tip, names, shift = 0) => {
    const max = FM6.max(i), d = names ? { label, fmt: 8, min: 0, max, def: 0, fk: -1, unit: "", names: names.slice(0, max + 1), order: null } : { label, fmt: 0, min: shift, max: max + shift, def: shift < 0 ? 0 : 0, fk: -1, unit: "", names: null, order: null };
    const c = new Cell({ desc: () => d, value: () => (voice ? voice[i] + shift : d.min), set: (v) => put(i, v - shift), inert: () => (voice ? null : "VOICE NOT READ"), tip: () => tip || "", whyText: () => "the voice is being read from the unit" });
    cells.push(c); return c.el;
  };
  const grid = (els, n = 4) => { const g = h("div", { class: "grid" }, els); if (n !== 4) g.style.gridTemplateColumns = `repeat(${n}, minmax(0, 1fr))`; return g; };   // (four: the stylesheet's .grid, three to a row in the narrow layout: 0.2.25 layout round)
  const card = (title, tip, ...kids) => h("section", { class: "card" }, h("h3", {}, h("span", { class: "ttl", text: title, title: tip || "" })), ...kids);

  // the voice: name, source, actions
  const fileIn = h("input", { type: "file", accept: ".syx,application/octet-stream", hidden: true, "aria-label": "DX7 sysex file", onchange: async (e) => { const f = e.target.files[0]; e.target.value = ""; if (f) await importSyx(f); } });
  const head = card("FM6 VOICE", "The six operators this part plays (the ENGINE tab's MACROS act on top of it).",
    h("div", { class: "row" }, h("label", { class: "row" }, h("span", { text: "NAME" }), nameIn),
      h("button", { class: "ghost small", text: "Read from the unit again", title: "Reads this part's voice from the unit (after an edit on the unit's own operator pages)", onclick: () => load(true) }),
      h("button", { class: "ghost small", text: "Import DX7 .syx voice...", title: "A DX7 sysex file (one voice or a bank of 32): the voice you pick becomes this part's voice", onclick: () => fileIn.click() }), fileIn,
      h("button", { class: "ghost small", text: "Download voice as .syx", title: "This part's voice as a single-voice DX7 sysex file on this computer", onclick: exportSyx })),
    status);
  nameIn.addEventListener("change", () => { if (!voice) return; FM6.setName(voice, nameIn.value); send(); });

  const algSel = h("select", { "aria-label": "Algorithm", onchange: (e) => put(FM6.VI.ALG, +e.target.value) }, Array.from({ length: 32 }, (_, a) => h("option", { value: a, text: `ALGORITHM ${a + 1}` })));
  const algCard = card("ALGORITHM AND FEEDBACK", "How the six operators are connected: a CARRIER is heard, a MOD (modulator) shapes the operator it feeds.",
    h("div", { class: "algrow" }, h("div", {}, h("label", { class: "lab", text: "ALGORITHM" }), algSel)),
    grid(VC.alg.map(([f, l, t, n, s]) => mkCell(byte(f), l, t, n, s)), 3));
  const pitchCard = card("PITCH ENVELOPE", "Moves the pitch of all six operators (50 is no change).", grid(VC.pitch.map(([f, l]) => mkCell(byte(f), l, `Pitch envelope ${l.toLowerCase()}.`))));
  const lfoCard = card("LFO", "The voice's own LFO (vibrato and tremolo).", grid(VC.lfo.map(([f, l, t, n]) => mkCell(byte(f), l, t, n))));
  const opCards = [1, 2, 3, 4, 5, 6].map((op) => {
    const freq = h("span", { class: "small opfreq" }), role = h("span", { class: "state" });
    const el = card(`OPERATOR ${op}`, "",
      grid(OPC.map(([f, l, t, n, s]) => mkCell(byte(f, op), l, t, n, s))),
      h("h4", { class: "sub", text: "ENVELOPE", title: "Rates: higher is faster." }), grid(ENVC.map(([f, l]) => mkCell(byte(f, op), l, `Operator ${op} envelope ${l.toLowerCase()}. Rates: higher is faster.`))),
      h("h4", { class: "sub", text: "KEY SCALING" }), grid(KLS.map(([f, l, t, n]) => mkCell(byte(f, op), l, t, n)), 5));
    el.querySelector("h3").append(role, freq);
    return { op, el, freq, role };
  });
  root.append(head, algCard, h("div", { class: "opgrid" }, opCards.map((c) => c.el)), pitchCard, lfoCard);

  function refresh() {
    if (destroyed) return;
    for (const c of cells) c.refresh();
    if (!voice) return;
    if (document.activeElement !== nameIn) nameIn.value = FM6.name(voice);
    const a = voice[FM6.VI.ALG]; algSel.value = a;
    const g = algGraph(a);
    for (const c of opCards) {
      c.freq.textContent = opFreq(voice[byte("MODE", c.op)], voice[byte("FC", c.op)], voice[byte("FF", c.op)]);
      const car = g.carriers.includes(c.op);
      c.role.textContent = car ? "CARRIER" : "MOD";
      c.role.title = car ? "A carrier: it is heard." : `A modulator of operator ${g.edges.filter(([m]) => m === c.op).map(([, t]) => t).join(", ")}.`;
      c.el.classList.toggle("carrier", car);
    }
  }
  async function load(force = false) {
    if (loading || destroyed) return;
    if (!force && (Date.now() - pending < 1500 || u.sending.get(`fm6:${p}`))) return;   // (our own write is on its way)
    loading = true;
    try {
      const pk = await u.readFm6(p);
      if (pk.join() !== lastPacked || force || !voice) { voice = FM6.unpack(pk); lastPacked = pk.join(); status.textContent = ""; refresh(); }
    } catch (e) { status.textContent = `The voice could not be read: ${e.message}`; }
    finally { loading = false; }
  }
  async function importSyx(f) {
    try {
      const r = FM6.parseSysex(await readBytes(f));
      if (!r.voices.length) { say(`${f.name}: no DX7 voice found in it.`, "warn"); return; }
      const pick = r.voices.length === 1 ? 0 : await new Promise((resolve) => {
        const dlg = h("dialog", { class: "ask", "aria-label": "Pick a voice" }), sel = h("select", { size: Math.min(12, r.voices.length), "aria-label": "Voice" }, r.voices.map((v, i) => h("option", { value: i, text: `${i + 1}  ${v.name}` })));
        sel.value = "0"; const done = (v) => { dlg.close(); dlg.remove(); resolve(v); };
        dlg.append(h("h2", { text: `Import a voice into PART ${p + 1}` }), h("p", { text: `${f.name} holds ${r.voices.length} voices. The one you pick replaces PART ${p + 1}'s FM6 voice (heard at once, not stored on the unit until you store the patch).` }), sel,
          h("div", { class: "row end" }, h("button", { class: "ghost", text: "Cancel", onclick: () => done(null) }), h("button", { text: "Use this voice", onclick: () => done(+sel.value) })));
        dlg.addEventListener("cancel", (e) => { e.preventDefault(); done(null); }); document.body.append(dlg); dlg.showModal();
      });
      if (pick == null) return;
      voice = FM6.sanitize(r.voices[pick].v); send(); refresh();
      say(`"${r.voices[pick].name}" is PART ${p + 1}'s FM6 voice now (not stored on the unit).`, "good");
    } catch (e) { app.fail(e); }
  }
  function exportSyx() {
    if (!voice) return;
    const b = FM6.singleSysex(voice), nm = `${FM6.name(voice).replace(/[^A-Za-z0-9._-]+/g, "_") || "voice"}.syx`;
    (app.downloads ||= []).push({ name: nm, bytes: b });
    try { const url = URL.createObjectURL(new Blob([b], { type: "application/octet-stream" })), a = h("a", { href: url, download: nm }); document.body.append(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 2000); } catch { /* no download here */ }
    say(`Downloaded ${nm} to this computer.`, "good");
  }
  status.textContent = "Reading the voice from the unit ...";
  load(true); timer = setInterval(() => { if (!document.hidden && root.isConnected) load(); }, 2500);
  return {
    el: root, refresh, reload: () => load(true),
    setByte: (i, v) => { if (voice) { voice[i] = v; send(); refresh(); } },          // (undo)
    get voice() { return voice; },
    destroy() { destroyed = true; clearInterval(timer); },
  };
}
