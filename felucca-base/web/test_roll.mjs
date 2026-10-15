// SPDX-License-Identifier: GPL-3.0-only
// The piano roll's model (web/editor/roll.js): the step rules (the unit's note length = a NOTE step and its TIE steps), the edit functions, undo / redo, copy / paste,
// the live write queue against the simulated unit (STEP_GET / STEP_SET / TRACK_STEP with the unit's refusals), the hide-rule state, the layout entry.
//   node web/test_roll.mjs      (use the emsdk node: the Homebrew one is broken here)
import { readFile } from "node:fs/promises";
import * as R from "./editor/roll.js";
import { MockUnit, MockLink } from "./editor/mockunit.js";
import { Unit } from "./editor/unit.js";
import { CMD, parseStep, noteName } from "./editor/proto.js";
import { L, ID, PART_TABS, uncoveredPages } from "./editor/layout.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(112)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const LEN = 16;
const fresh = () => Array.from({ length: 64 }, (_, i) => R.blank(i));
const run = (steps, map) => R.apply(steps, map);
const note = (steps, i, pitch, opt) => run(steps, R.addNote(steps, LEN, i, pitch, opt));
const summary = (steps, len = LEN) => R.bars(steps, len).map((b) => `${b.start}:${b.pitch}x${b.len}`).join(" ");
async function connect() { const m = new MockUnit(), u = new Unit(new MockLink(m)); await u.connect(); return { m, u }; }

// ---- adding, chords, the 4-note limit
{
  let s = note(fresh(), 3, 60);
  ok(summary(s) === "3:60x1" && s[3].time === R.NOTE && s[3].n === 1 && s[3].vel === R.DEF_VEL && s[3].chance === 100 && s[3].ratchet === 1, "add: a click on an empty step is a NOTE step, velocity 100, chance 100, ratchet 1");
  s = note(s, 3, 64); s = note(s, 3, 67); s = note(s, 3, 71);
  ok(R.notesOf(s[3]).join() === "60,64,67,71" && s[3].n === 4, "add: a step holds up to 4 notes (a chord)");
  const over = R.addNote(s, LEN, 3, 72);
  ok(over.size === 0 && /at most 4/.test(over.msg), "add: a 5th note in a step is refused with a plain message");
  ok(R.addNote(s, LEN, 3, 64).size === 0, "add: the same note twice changes nothing");
  const one = R.apply(note(fresh(), 2, 50), R.addNote(note(fresh(), 2, 50), LEN, 2, 55, { maxNotes: 1 }));
  ok(R.notesOf(one[2]).join() === "55", "add: in the arp's SEQ mode (one note per step) a second pitch replaces the first");
  ok(R.addNote(fresh(), LEN, LEN, 60).size === 0 && R.addNote(fresh(), LEN, 0, 128).size === 0, "add: past LEN or past note 127: nothing");
}
// ---- the tie model: a note's length is its step and the TIE steps after it (ui.c note_len / note_set_len)
{
  let s = note(fresh(), 4, 60);
  s = run(s, R.resize(s, LEN, 4, 4));
  ok(summary(s) === "4:60x4" && s[5].time === R.TIE && s[6].time === R.TIE && s[7].time === R.TIE && s[5].n === 0, "length: 4 steps = the NOTE and 3 TIE steps (a TIE holds no note of its own)");
  s = run(s, R.resize(s, LEN, 4, 2));
  ok(summary(s) === "4:60x2" && s[6].time === R.REST && s[7].time === R.REST && s[6].vel === 0, "length: shorter: the last TIEs become clean RESTs");
  let t = note(s, 8, 62);
  t = run(t, R.resize(t, LEN, 4, 8));
  ok(summary(t) === "4:60x4 8:62x1", "length: growing stops at the next step that holds notes");
  t = run(t, R.resize(t, LEN, 8, 20));
  ok(summary(t) === "4:60x4 8:62x8", "length: growing stops at LEN (no wrap)");
  let c = note(note(fresh(), 0, 60), 0, 67); c = run(c, R.resize(c, LEN, 0, 3));
  ok(summary(c) === "0:60x3 0:67x3", "chord: all notes of a step share the length (a TIE has no notes of its own)");
  ok(R.resize(c, LEN, 5, 3).size === 0 && R.resize(c, LEN, 1, 3).size === 0, "length: only a step that has notes can be lengthened");
  ok(R.ownerOf(c, LEN, 2) === 0 && R.ownerOf(c, LEN, 3) === -1 && R.barAt(c, LEN, 2, 67)?.start === 0 && R.barAt(c, LEN, 2, 61) === null, "bars: a click on a tied step finds its note");
}
// ---- deleting
{
  let s = note(note(fresh(), 2, 60), 2, 64); s = run(s, R.resize(s, LEN, 2, 3));
  const one = run(s, R.removeNote(s, LEN, 2, 60));
  ok(summary(one) === "2:64x3", "delete: one note of a chord goes, the other keeps its length");
  const none = run(one, R.removeNote(one, LEN, 2, 64));
  ok(summary(none) === "" && none[2].time === R.REST && none[3].time === R.REST && none[4].time === R.REST && R.orphans(none, LEN).length === 0, "delete: the last note takes its step and its TIEs with it (no stray ties)");
  ok(R.deleteAt(s, LEN, 4, 64).size > 0 && R.deleteAt(s, LEN, 4, 61).size === 0, "delete: a right-click on the tied part of a bar deletes that note");
}
// ---- add on a tied step cuts the note there
{
  let s = note(fresh(), 0, 60); s = run(s, R.resize(s, LEN, 0, 6));
  const t = note(s, 3, 72);
  ok(summary(t) === "0:60x3 3:72x1" && t[4].time === R.REST && t[5].time === R.REST && R.orphans(t, LEN).length === 0, "add on a TIE step: the first note ends there, the rest of its ties become rests, the new note is one step");
}
// ---- moving
{
  let s = note(fresh(), 2, 60); s = run(s, R.resize(s, LEN, 2, 3)); s[2] = { ...s[2], vel: 90, flags: R.F_ACCENT | R.F_SLIDE, chance: 70, ratchet: 2 };
  const up = run(s, R.moveNote(s, LEN, 2, 60, 2, 65));
  ok(summary(up) === "2:65x3" && up[2].vel === 90, "move: same step, other pitch: only the note changes");
  const mv = run(s, R.moveNote(s, LEN, 2, 60, 6, 62));
  ok(summary(mv) === "6:62x3" && mv[6].vel === 90 && mv[6].flags === 3 && mv[6].chance === 70 && mv[6].ratchet === 2 && mv[2].time === R.REST && mv[3].time === R.REST && R.orphans(mv, LEN).length === 0, "move: another step: it keeps length, velocity, accent, slide, chance, ratchet; the old steps are rests");
  const over = run(s, R.moveNote(s, LEN, 2, 60, 3, 60));
  ok(summary(over) === "3:60x3", "move: one step right over its own tie: the bar moves, no stray steps");
  const edge = run(s, R.moveNote(s, LEN, 2, 60, 15, 60));
  ok(summary(edge) === "15:60x1", "move: at the end of LEN the length is cut to what fits");
  const far = R.moveNote(s, LEN, 2, 60, 99, 60);
  ok(summary(run(s, far)) === "15:60x1", "move: past LEN lands on the last step");
  let chord = note(note(fresh(), 5, 60), 5, 64);
  const split = run(chord, R.moveNote(chord, LEN, 5, 64, 9, 64));
  ok(summary(split) === "5:60x1 9:64x1", "move: one note of a chord leaves the step; the rest stays");
  const join = run(split, R.moveNote(split, LEN, 9, 64, 5, 67));
  ok(summary(join) === "5:60x1 5:67x1", "move: onto a step with notes it joins them");
  const full = note(note(note(note(fresh(), 5, 60), 5, 62), 5, 64), 5, 65);
  const lone = note(full, 6, 40);
  const refused = R.moveNote(lone, LEN, 6, 40, 5, 70);
  ok(refused.size === 0 && /at most 4/.test(refused.msg), "move: onto a full step: refused, nothing changes");
  let tied = note(fresh(), 0, 50); tied = run(tied, R.resize(tied, LEN, 0, 4)); let lone2 = note(tied, 8, 60);
  const cut = run(lone2, R.moveNote(lone2, LEN, 8, 60, 2, 60));
  ok(summary(cut) === "0:50x2 2:60x1" && R.orphans(cut, LEN).length === 0, "move: onto the tied part of another note cuts that note there");
}
// ---- properties and flags
{
  let s = note(fresh(), 1, 60);
  s = run(s, R.setProps(s, LEN, 1, { vel: 500, chance: 0, ratchet: 9 }));
  ok(s[1].vel === 127 && s[1].chance === 1 && s[1].ratchet === 4, "props: velocity 1..127, chance 1..100, ratchet 1..4 are clamped");
  s = run(s, R.toggleFlag(s, LEN, 1, R.F_ACCENT)); s = run(s, R.toggleFlag(s, LEN, 1, R.F_SLIDE));
  ok(s[1].flags === 3 && run(s, R.toggleFlag(s, LEN, 1, R.F_ACCENT))[1].flags === 2, "props: accent and slide toggle on their own bits");
  ok(R.setProps(s, LEN, 5, { vel: 50 }).size === 0 && R.toggleFlag(s, LEN, 0, 1).size === 0, "props: a step with no note has none of them");
}
// ---- clear, paste, the clipboard
{
  let s = note(note(fresh(), 0, 60), 4, 64); s = run(s, R.resize(s, LEN, 4, 3));
  ok(summary(run(s, R.clearPattern(s, LEN))) === "" && R.clearPattern(fresh(), LEN).size === 0, "clear: every step of LEN becomes a rest; an empty pattern has nothing to write");
  const keep = s.slice(); keep[20] = { ...R.blank(20), time: R.NOTE, n: 1, notes: [70, 0, 0, 0], vel: 100 };
  ok(R.apply(keep, R.clearPattern(keep, LEN))[20].n === 1, "clear: steps past LEN are left alone");
  const clip = s.slice(0, 5).map(R.clone), t = fresh();
  const p = run(t, R.pasteClip(t, LEN, clip));
  ok(summary(p) === "0:60x1 4:64x1" && p[4].time === R.NOTE, "paste: the copied steps replace the start of the pattern; a tie cut off by the clip becomes a rest");
}
// ---- the scales are the firmware's
{
  let seq = "", par = "";
  try { seq = await readFile(new URL("../firmware/src/seq.c", import.meta.url), "utf8"); par = await readFile(new URL("../firmware/src/params.c", import.meta.url), "utf8"); } catch { /* the web folder alone */ }
  if (seq) {
    const block = seq.slice(seq.indexOf("SCALE_MASK[] = {"), seq.indexOf("};", seq.indexOf("SCALE_MASK[] = {")));
    const masks = [...block.matchAll(/^\s*(0xFFF|\([^/]*?\))\s*,\s*\/\* ([A-Z]+)/gm)].map((m) => [m[2], eval(m[1].replace(/\(1 << (\d+)\)/g, "(1<<$1)"))]);
    ok(masks.length === 16 && masks.every(([n, v], i) => R.SCALES[i][0] === n && R.SCALES[i][1] === v), `scales: the 16 masks and names are seq.c's SCALE_MASK (${masks.length} read)`);
    const names = par.match(/N_SCALE\[\] = \{([^}]*)\}/)[1].match(/"([A-Z]+)"/g).map((x) => x.slice(1, -1));
    ok(names.join() === R.SCALES.map((x) => x[0]).join(), "scales: the names are params.c's N_SCALE");
  }
  ok(R.inScale(60, 0, 1) && !R.inScale(61, 0, 1) && R.inScale(62, 2, 1) === 1 && R.inScale(61, 0, 0) === 1, "scales: C major lights C and not C#; root D shifts it; CHR lights everything");
  ok([0, 2, 4, 5, 7, 9, 11].every((k) => !R.isBlackKey(60 + k)) && [1, 3, 6, 8, 10].every((k) => R.isBlackKey(60 + k)), "keys: the black keys are C# D# F# G# A#");
}
// ---- fuzz: random edits never leave a state the unit would read differently (no orphan TIE, no TIE with notes, 7-bit bytes, notes sorted into n)
{
  let seed = 7; const rnd = (n) => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed % n; };
  let s = fresh(), bad = "";
  for (let k = 0; k < 4000 && !bad; k++) {
    const i = rnd(LEN), p = 40 + rnd(12), op = rnd(7), b = R.bars(s, LEN);
    let ch;
    if (op === 0) ch = R.addNote(s, LEN, i, p, { maxNotes: rnd(5) === 0 ? 1 : 4 });
    else if (op === 1 && b.length) { const x = b[rnd(b.length)]; ch = R.removeNote(s, LEN, x.start, x.pitch); }
    else if (op === 2 && b.length) { const x = b[rnd(b.length)]; ch = R.moveNote(s, LEN, x.start, x.pitch, rnd(LEN + 2) - 1, 40 + rnd(12)); }
    else if (op === 3 && b.length) { const x = b[rnd(b.length)]; ch = R.resize(s, LEN, x.start, 1 + rnd(8)); }
    else if (op === 4 && b.length) { const x = b[rnd(b.length)]; ch = R.setProps(s, LEN, x.start, { vel: rnd(200), chance: rnd(120), ratchet: rnd(6), flags: rnd(4) }); }
    else if (op === 5 && rnd(8) === 0) ch = R.clearPattern(s, LEN);
    else continue;
    s = run(s, ch);
    for (let j = 0; j < LEN && !bad; j++) {
      const x = s[j];
      if (x.time === R.TIE && (x.n !== 0 || R.ownerOf(s, LEN, j) < 0)) bad = `step ${j}: stray TIE after op ${op}`;
      if (x.time === R.NOTE && x.n > 4) bad = `step ${j}: n ${x.n}`;
      if (R.isOn(x) && new Set(R.notesOf(x)).size !== x.n) bad = `step ${j}: a note twice`;
      if (R.stepBytes?.(x)) { /* not exported */ }
      if ([x.n, ...x.notes, x.time, x.flags, x.vel, x.chance, x.ratchet, x.nudge].some((v) => !(v >= 0 && v <= 127))) bad = `step ${j}: not 7 bit`;
      if (x.chance < 1 || x.chance > 100 || x.ratchet < 1 || x.ratchet > 4 || x.index !== j) bad = `step ${j}: field out of range`;
    }
  }
  ok(!bad, `fuzz: 4000 random edits keep every step valid for the unit${bad ? " (" + bad + ")" : ""}`);
}
// ---- the hide-rule state
{
  const u = { parts: [{ p: Array(L.P_COUNT).fill(0) }, { p: Array(L.P_COUNT).fill(0) }], menu: [{ id: 28, names: ["OFF", "ON"], value: 1, min: 0 }, { id: 29, names: ["OFF", "ON"], value: 1, min: 0 }] };
  ok(JSON.stringify(R.seqState(u, 0)) === '{"arpSeq":false,"arpOff":false,"seqOff":false}', "state: ARP and SEQ ON, arp MODE OFF: the steps play by themselves");
  u.menu[1].value = 0; ok(R.seqState(u, 0).seqOff === true, "state: MODULES SEQ OFF: no step plays (editing still works)");
  u.parts[1].p[ID.P_AMODE] = R.AM_SEQ; u.menu[0].value = 0;
  ok(R.seqState(u, 1).arpSeq && R.seqState(u, 1).arpOff && !R.seqState(u, 0).arpSeq, "state: arp MODE SEQ: the steps are the arp's offsets; MODULES ARP OFF: they play nothing");
  ok(R.seqState({ parts: u.parts }, 1).seqOff === false, "state: before the MODULES are read nothing is called OFF");
}
// ---- the layout: a PIANO ROLL tab per part
{
  ok(PART_TABS.at(-1).key === "roll" && PART_TABS.at(-1).label === "PIANO ROLL" && uncoveredPages().length === 0, "layout: the part columns have a PIANO ROLL tab (the last one) and every firmware page is still covered");
}
// ---- the simulated unit's step commands (the same rules as the unit)
{
  const { m, u } = await connect();
  const a = await u.req(CMD.STEP_GET, [8]);
  ok(a[0] === 8 && a[1] === 2 && a[2] === 55 && a[3] === 59 && a[6] === 0, "mock: STEP_GET reads the selected part's step 8 (a chord of two notes)");
  const w = await u.req(CMD.STEP_SET, [8, 3, 48, 52, 55, 0, 0, 1, 99, 0, 0, 0, 80, 3, 12]);
  ok(parseStep(w).n === 3 && parseStep(w).chance === 80 && parseStep(w).ratchet === 3 && parseStep(w).nudge === 12 && m.steps[0][8].notes[2] === 55, "mock: STEP_SET with every optional byte answers the step as stored");
  const legacy = await u.req(CMD.STEP_SET, [9, 1, 60, 0, 0, 0, 0, 0, 100]);
  ok(parseStep(legacy).chance === 100 && parseStep(legacy).hits.join() === "0,0,0", "mock: the 8-byte legacy step leaves the hits, chance, ratchet and nudge as they were");
  const refuses = async (args) => { try { await u.req(CMD.STEP_SET, args); return false; } catch { return true; } };
  ok(await refuses([1, 5, 0, 0, 0, 0, 0, 0, 100]) && await refuses([1, 1, 60, 0, 0, 0, 0, 0, 100, 0, 0, 0, 101]) && await refuses([1, 1, 60, 0, 0, 0, 0, 0, 100, 0, 0, 0, 100, 5]) && await refuses([1, 1, 60, 0, 0, 0, 0, 0, 100, 0, 0, 0, 100, 1, 16]) && await refuses([1, 1, 200, 0, 0, 0, 0, 0, 100]), "mock: more than 4 notes, chance above 100, ratchet 5, nudge above 15 and a byte above 127 get no reply");
  const t = await u.req(CMD.TRACK_STEP, [1, 8]);
  ok(t[0] === 1 && t[1] === 8 && t[2] === 2, "mock: TRACK_STEP reads part 2 without selecting it");
  let pushed = null; u.on("step", (d) => { pushed = d; }); m.deviceStep(1, 3, { n: 1, notes: [70, 0, 0, 0], time: 0 });
  ok(pushed?.index === 3 && pushed.part === 1, "mock: a step edited on the unit's panel is pushed as STEP_CHANGED with its part");
}
// ---- the store: live writes, gestures, undo / redo, send live off, copy / paste between parts, the unit's own edits
{
  const { m, u } = await connect(), st = new R.PatternStore(u, 0);
  await st.load();
  ok(st.loaded && st.steps.length === 64 && st.len === 16 && summary(st.steps) === "0:48x1 4:50x1 8:55x3 8:59x3 12:52x1 13:57x1 14:60x1", `store: loads all 64 steps and shows LEN (${summary(st.steps)})`);
  const writes = () => m.log.filter((x) => x.startsWith("step 0:")).length;
  st.edit("add note", R.addNote(st.steps, st.len, 1, 62));
  await st.idle();
  ok(m.steps[0][1].time === 0 && m.steps[0][1].notes[0] === 62 && m.steps[0][1].vel === 100 && writes() === 1, "store: send live writes the one changed step at once (STEP: TRACK_STEP)");
  st.begin(); for (let k = 0; k < 5; k++) st.preview(R.resize(st.base, st.len, 1, 1 + k));   // (the note at step 4 stops it at 3 steps)
  await st.idle(); ok(writes() === 1 && R.tailOf(st.steps, 16, 1) === 3, "store: a drag changes the picture only; nothing is written until the gesture ends");
  st.commit("change length"); await st.idle();
  ok(writes() === 1 + 2 && m.steps[0][2].time === 1 && m.steps[0][3].time === 1 && m.steps[0][4].time === 0 && m.steps[0][1].time === 0, `store: the committed gesture writes the 2 tied steps (${writes()} writes in all)`);
  const before = JSON.stringify(m.steps[0]);
  st.undo(); await st.idle();
  ok(summary(st.steps).includes("1:62x1") && m.steps[0][2].time === 2 && m.steps[0][3].time === 2 && m.steps[0][4].time === 0, "undo: the length change is undone in the picture and on the unit");
  st.undo(); await st.idle();
  ok(!summary(st.steps).includes("1:62") && m.steps[0][1].time === 2 && m.steps[0][1].n === 0, "undo: the note is gone again, on the unit too");
  st.redo(); st.redo(); await st.idle();
  ok(JSON.stringify(m.steps[0]) === before && !st.canRedo && st.canUndo, "redo: twice brings back exactly the unit's steps from before");
  st.edit("add note", R.addNote(st.steps, st.len, 0, 80)); st.undo(); st.edit("add note", R.addNote(st.steps, st.len, 3, 80));
  ok(!st.canRedo, "history: a new edit drops the redo steps");
  await st.idle();
  // send live off
  st.setLive(false); const w0 = writes();
  st.edit("clear", R.clearPattern(st.steps, st.len)); st.edit("add", R.addNote(st.steps, st.len, 6, 70));
  await st.idle();
  ok(writes() === w0 && st.pending > 0 && m.steps[0][0].n === 1, `store: send live OFF: ${st.pending} edited steps wait, the unit is untouched`);
  await st.send();
  ok(st.pending === 0 && m.steps[0][0].n === 0 && m.steps[0][6].notes[0] === 70 && writes() > w0, "store: Send writes the waiting steps");
  st.setLive(false); st.edit("add", R.addNote(st.steps, st.len, 7, 71));
  await st.revert();
  ok(st.pending === 0 && m.steps[0][7].n === 0 && st.steps[7].n === 0 && !st.canUndo, "store: Revert throws the waiting edits away and reads the unit again");
  st.setLive(true);
  // copy / paste between parts
  st.edit("add", R.addNote(st.steps, st.len, 2, 61)); await st.idle();
  const n = st.copy(); const st2 = new R.PatternStore(u, 1); await st2.load();
  st2.paste(); await st2.idle();
  ok(n === 16 && m.steps[1][2].notes[0] === 61 && m.steps[1][6].notes[0] === 70 && m.steps[1][0].n === 0, "paste: the pattern copied on PART 1 is pasted over PART 2's (written to the unit)");
  ok(await (async () => { st2.clear(); await st2.idle(); return m.steps[1].slice(0, 16).every((x) => x.n === 0 && x.time === 2); })(), "clear: PART 2's pattern is emptied on the unit");
  st2.undo(); await st2.idle(); ok(m.steps[1][2].notes[0] === 61, "clear: and undo brings it back");
  // the unit's panel edits a step: the store follows (STEP_CHANGED), a waiting local edit is not overwritten
  u.on("step", (d) => { if (d.part === 0) st.external(d.index); });
  m.deviceStep(0, 12, { n: 1, notes: [77, 0, 0, 0] }); await new Promise((r) => setTimeout(r, 30));
  ok(st.steps[12].notes[0] === 77, "sync: a step edited on the unit's panel appears in the roll");
  st.setLive(false); st.edit("add", R.addNote(st.steps, st.len, 13, 90)); m.deviceStep(0, 13, { n: 1, notes: [20, 0, 0, 0] }); await new Promise((r) => setTimeout(r, 30));
  ok(st.steps[13].notes.includes(90), "sync: an edit that is not sent yet is not overwritten by the unit's push");
  st.setLive(true); await st.idle();
  // a refused write: the step stays marked, the error is reported, the next edit sends it again
  let err = null; st.sub((e) => { if (e.type === "error") err = e.error; });
  const real = u.writeStep.bind(u); let fail = true; u.writeStep = async (...a) => { if (fail) { fail = false; throw new Error("no reply"); } return real(...a); };
  st.edit("add", R.addNote(st.steps, st.len, 9, 66)); await st.idle();
  ok(err?.message === "no reply" && st.pending === 1 && m.steps[0][9].n === 0, "store: a write the unit does not answer is reported and stays pending");
  st.edit("add", R.addNote(st.steps, st.len, 10, 67)); await st.idle();
  ok(st.pending === 0 && m.steps[0][9].notes[0] === 66 && m.steps[0][10].notes[0] === 67, "store: the next edit sends the stuck step too");
  // LEN changes show more or fewer steps; steps past LEN are kept and not shown
  u.parts[0].p[ID.P_SLEN] = 8; ok(st.len === 8 && R.bars(st.steps, st.len).every((b) => b.start < 8) && st.steps[12].n === 1, "LEN: a shorter LEN shows fewer steps and keeps the others");
  // a pattern change under the editor clears the history, an identical re-read keeps it
  await st.load(); ok(st.canUndo, "history: reading the same pattern again keeps the undo steps");
  m.steps[0][5].n = 1; m.steps[0][5].notes[0] = 33; m.steps[0][5].time = 0; await st.load(); ok(!st.canUndo, "history: a pattern that changed under us clears the undo steps (they would no longer fit)");
}
// ---- a unit that is read-only (the editor does not know its INFO): the roll edits nothing
{
  const { u } = await connect(), st = new R.PatternStore(u, 0); await st.load();
  u.compat = "This unit reports 200 parameters; this editor knows 127. Nothing is written.";
  let err = null; st.sub((e) => { if (e.type === "error") err = e.error; });
  const done = st.edit("add note", R.addNote(st.steps, st.len, 1, 62));
  ok(done === false && st.steps[1].n === 0 && st.pending === 0 && /Nothing is written/.test(err?.message) && !st.canUndo, "read-only unit: an edit is refused with the reason, the roll keeps the unit's steps");
}
console.log(fails ? `\nROLL TESTS FAILED (${fails})` : "\nroll tests passed");
process.exit(fails ? 1 : 0);
