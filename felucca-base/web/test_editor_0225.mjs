// SPDX-License-Identifier: GPL-3.0-only
// DUELL 0.2.25, the editor round: the label table, the VOICE hide rule, UNDO, BYPASS, INIT, both parts after a load on the unit, the write throttle and
// the cutoff slew, FM6 voices (read / write, the algorithm graph), the multis and pattern slots through the backup objects (files, round trips), the
// patch CATEGORY. Against the simulated unit (editor/mockunit.js).
//   node web/test_editor_0225.mjs      (use the emsdk node)
import { L, ID, P_E0, isBipolar, bipolarZero, descOf, voiceWhy, modulesWhy, PART_TABS, GLOBAL_TABS, pageByTitle, dstOffered, dstWhy, shapeInert, inertReason } from "./editor/layout.js";
import { PART, GLOBAL, longDesc, tipOf, whyText, WHY, VALUE_NAMES, CATEGORY_NAMES, engineLong } from "./editor/labels.js";
import { MockUnit, MockLink, defaultPart } from "./editor/mockunit.js";
import { Unit } from "./editor/unit.js";
import { UndoStack } from "./editor/undo.js";
import { Bypass, itemsOf } from "./editor/bypass.js";
import * as S from "./editor/stored.js";
import * as LIB from "./editor/librarian.js";
import { algGraph, opFreq } from "./editor/fm6edit.js";
import { FM6 } from "./editor/fm6voice.js";
import { CMD } from "./editor/proto.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(112)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const E = (n) => L.ENG.findIndex((e) => e.name === n);
async function connect(opt = {}) { const m = new MockUnit(opt), u = new Unit(new MockLink(m)); await u.connect(); return { m, u }; }
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// ---- 0.2.25 matrix round: the DEST list the unit offers (layout.json MXH from mod.c), FILTER MODE, the SHAPE depth cards
{
  const M = L.MD, mk = (name, wave, key = 0) => { const p = new Array(L.P_COUNT).fill(0); p[P_E0 + key] = wave; p[ID.P_FREEA] = L.TP[ID.P_FREEA].max; return { eng: E(name), p }; };
  const saw = mk("ANALOG", 0), pwm = mk("ANALOG", 4), nois = mk("TRIO", 8), lofiTri = mk("LOFI", 1, 1), wheel = mk("WHEEL", 0);
  ok(M.FMODE === 46 && L.TP[ID.P_M1SRC + 1].names[M.FMODE] === "FMODE" && VALUE_NAMES.dst.FMODE === "MULTI FILTER MODE", "matrix: MULTI FILTER MODE is destination 46 (appended), shown MULTI FILTER MODE");
  ok(![28, 29, 30].some((d) => dstOffered(saw, d)) && !dstOffered(saw, M.E1) && dstOffered(saw, M.E1 + 1) && dstOffered(saw, M.FMODE) && dstOffered(saw, M.CUT ?? 2),
     "matrix: the EQ gains and ANALOG WAVE are not offered; ANALOG DTN, FILTER MODE and CUT are");
  ok(!dstOffered(saw, M.SHP) && !dstOffered(saw, M.E2SHP) && dstOffered(pwm, M.SHP) && dstWhy(saw, M.SHP) === "WAVE", "matrix: SHAPE is offered with ANALOG PWM, not with SAW (WAVE)");
  ok(!dstOffered(nois, M.E1 + 1) && !dstOffered(nois, M.E1 + 2) && !dstOffered(nois, M.E1 + 3) && !dstOffered(nois, M.E1 + 7) && !dstOffered(nois, M.E1 + 5) && !dstOffered(nois, M.E1 + 4),
     "matrix: TRIO NOIS: INT2 INT3 DTN PW not offered (no oscillator takes them), nor the unused CUT slot (its own filter is gone, cloud job J); MODE removed");
  ok(!dstOffered(lofiTri, M.E1 + 2) && !dstOffered(lofiTri, M.E1) && !dstOffered(lofiTri, M.E1 + 1) && !dstOffered(lofiTri, M.E1 + 6) && dstOffered(lofiTri, M.E1 + 3),
     "matrix: LOFI TRI: DUTY not offered; BITS WAVE ARP removed; CRSH offered");
  ok(dstOffered(wheel, M.E1) && dstOffered(wheel, M.E1 + 7), "matrix: WHEEL's REG and ROTR stay offered");
  ok(dstWhy(saw, M.RES) === null && dstOffered(saw, M.RES), "matrix (T0229, owner): FILTER RES on ANALOG with the multimode filter open: shown normally, never dimmed or hidden by the cutoff");
  { const fm6 = defaultPart(L.ENG.findIndex((e) => e.name === "FM6")); ok(M.AATK === 47 && dstOffered(saw, M.AATK) && dstOffered(saw, M.AATK + 3) && !dstOffered(fm6, M.AATK) && dstWhy(fm6, M.AATK + 2) === "OWN ENV" && !dstOffered(saw, M.BUS + 5),
    "matrix (T0229): the AMP ENV times are offered (not on FM6: its operators' own envelopes); REVERB SIZE is not offered any more"); }
  ok(shapeInert(saw) && !shapeInert(pwm) && inertReason(saw, ID.P_LD_SHP) === "WAVE" && inertReason(pwm, ID.P_LD_SHP) === null && inertReason(saw, ID.P_E2SHP) === "WAVE",
     "matrix: the SHAPE depth cards hide with ANALOG SAW (WAVE), show with PWM");
}

// ---- 8: the label table
{
  const onPages = new Set(); for (const pg of L.PAGES) if (pg.scope === "TRACK") for (const id of pg.ids) if (id >= 0 && id < P_E0) onPages.add(id);
  const missing = [...onPages].filter((id) => !PART[id] || !PART[id][0] || !PART[id][1]);
  ok(missing.length === 0, `labels: every part control on a page has a long label and a tooltip (${missing.join(",") || "none missing"})`);
  const tabPages = new Set(GLOBAL_TABS.flatMap((t) => t.pages)), gOn = new Set(); for (const pg of L.PAGES) if (pg.scope === "GLOBAL" && tabPages.has(pg.title) && pg.title !== "MULTIS") for (const id of pg.ids) if (id >= 0 && id < L.G_COUNT) gOn.add(id);
  const gm = [...gOn].filter((id) => !GLOBAL[id]); ok(gm.length === 0, `labels: every global control on a page has a long label (${gm.join(",") || "none missing"})`);
  const want = { 42: "PRIORITY", 37: "MODE", 41: "GLIDE TYPE", 38: "GLIDE", 95: "SPREAD", 44: "UNISON DETUNE", 102: "TEMPO SYNC", 73: "ATTACK", 74: "DECAY", 75: "SUSTAIN", 76: "RELEASE", 109: "PATCH LEVEL", 6: "PITCH DEPTH", 7: "SHAPE DEPTH",
    114: "FILTER ENV", 27: "KEY TRACK", 14: "LFO 1", 117: "LFO 2", 29: "LENGTH", 30: "DIVISION", 31: "SWING", 32: "GATE", 81: "TYPE", 96: "TYPE", 105: "CURVE", 112: "RATE", 16: "AMP DEPTH" };
  ok(Object.entries(want).every(([id, t]) => PART[id][0] === t), "labels (T0229, rules R1..R6): inside a card the short name, the card's title carries the context (VOICE: MODE PRIORITY GLIDE TYPE SPREAD; CUTOFF MOD: FILTER ENV KEY TRACK LFO 1 LFO 2; PATTERN: LENGTH DIVISION SWING GATE; CHORD / INSERT: TYPE; DUCKING: RATE CURVE)");
  const vce = longDesc(L.TP[37], 37), prio = longDesc(L.TP[42], 42);
  ok(vce.names.join() === "POLY,MONO,LEGATO,UNISON" && prio.names.join() === "LAST,LOWEST,HIGHEST" && vce.short === "VCE", "labels: value names spelled out (POLY MONO LEGATO UNISON; LAST LOWEST HIGHEST; id 43 is the TWIN FILTER's TYPE now, job I names it), the unit's short label kept");
  ok(longDesc(L.TP[37], 37) === vce && L.TP[37].label === "VCE", "labels: the same descriptor object for the same control (a drag is never rebuilt) and the firmware's table untouched");
  let lenOk = true; for (const [k, m] of Object.entries(VALUE_NAMES)) { const d = /^\d+$/.test(k) ? L.TP[+k] : null; if (d?.names) lenOk = lenOk && longDesc(d, +k).names.length === d.names.length; }
  ok(lenOk, "labels: a long value list keeps the firmware list's length and order");
  ok(engineLong("FM6", "SLOT") === "SOURCE" && engineLong("ANALOG", "DTN") === "DETUNE" && engineLong("WAVE", "POS") === "POSITION" && engineLong("WAVE", "TABLE") === "TABLE" && engineLong("FM6", "MLVL") === "MOD LEVEL" && /factory voice/.test(tipOf("e:FM6", { short: "SLOT" })), "labels: engine controls by the engine's short label (WAVETABLE: TABLE, POSITION; MACROS: SOURCE, MOD LEVEL; with what F / OWN / B mean)");
  ok(CATEGORY_NAMES.length === 9 && CATEGORY_NAMES[6] === "PERCUSSION", "labels: the nine categories in the unit's order (6 is PERC on the unit, not DRUM)");
  ok(L.TP[48].names.join() === "12dB,24dB", "layout: SLOPE's value names fixed (the dump wrote them from index 0: '-' for 12 dB)");
  ok(whyText("UNUSED") === "" && whyText("INSERT OFF").length > 5 && Object.values(WHY).every((t) => t.length <= 60), "labels: every hide reason is a short plain line (none for an unused engine slot)");
}

// ---- 18: the VOICE hide rule (voice.c / fx.c, docs/VOICE_CONTROLS.md)
{
  const q = (vm, gld = 0) => { const p = defaultPart(0); p.p[ID.P_VOICE] = vm; p.p[ID.P_GLIDE] = gld; return p; };
  const row = (vm, gld) => [42, 44, ID.P_GLMODE, 95, 38, 109].map((id) => (voiceWhy(q(vm, gld), id) ? "-" : "x")).join("");
  ok(row(0, 0) === "---xxx" && row(0, 20) === "--xxxx", `voice rule POLY: PRIORITY hidden, DETUNE hidden, GLIDE MODE with GLIDE only, SPREAD shown (${row(0, 0)} ${row(0, 20)}); id 43 is the TWIN FILTER's TYPE now: no voice rule`);
  ok(row(1, 0) === "x---xx" && row(2, 30) === "x-x-xx" && row(3, 30) === "xxxxxx" && [0, 1, 2, 3].every((vm) => !voiceWhy(q(vm, 30), 43)), `voice rule MONO / LEGATO / UNISON: PRIORITY shown, DETUNE in UNISON only, SPREAD in UNISON only (${row(1, 0)} ${row(2, 30)} ${row(3, 30)}); the TWIN TYPE (id 43) is never hidden by the voice mode`);
  ok(Object.keys(WHY).filter((k) => /^VOICE|GLIDE/.test(k)).length === 6, "voice rule: each reason has its line");
}

// ---- 3: the MODULES switches hide what they switch off, with the reason
{
  const { m, u } = await connect(); await u.readMenu();
  const part = u.parts[0];
  ok(modulesWhy(u.menu, 33, part) === null && modulesWhy(u.menu, 18, part) === null, "MODULES all ON: nothing hidden");
  await u.setMenu(31, 0); await u.setMenu(28, 0); await u.setMenu(30, 0);
  ok(modulesWhy(u.menu, 33, part) === "MODULES PART FX" && modulesWhy(u.menu, 96, part) === "MODULES PART FX" && modulesWhy(u.menu, 18, part) === "MODULES ARP" && modulesWhy(u.menu, 34, part) === "MODULES SENDS" && modulesWhy(u.menu, 7) === "MODULES SENDS" && modulesWhy(u.menu, 0) === null && /SETTINGS > MODULES/.test(whyText("MODULES ARP")),
    "MODULES PART FX / ARP / SENDS OFF: SATURATION, INSERT, the arpeggiator, the sends and the shared effects hidden with the reason; TEMPO not");
}

// ---- 12: UNDO
{
  const st = new UndoStack(); let t = 1000;
  st.record(0, 45, 82, 70, t); st.record(0, 45, 70, 50, t + 100); st.record(0, 46, 0, 10, t + 200); st.record(1, 45, 82, 60, t + 5000);
  ok(st.list.length === 3 && st.list[0].items[0].oldV === 82 && st.list[0].items[0].newV === 50, "undo: a drag of one control is one entry (old 82, new 50)");
  const writes = []; const w = async (where, id, v) => writes.push(`${where}:${id}=${v}`);
  await st.undo(w, 0); ok(writes.join() === "0:46=0" && st.list.length === 2, "undo: the last edit of PART 1 goes back (RESONANCE 0)");
  await st.undo(w); ok(writes.at(-1) === "1:45=82", "undo: Ctrl+Z takes the newest of all (PART 2 CUTOFF)");
  st.group("EQ back to defaults", [{ where: 0, id: 8, oldV: 5, newV: 0 }, { where: 0, id: 88, oldV: -3, newV: 0 }]);
  await st.undo(w); ok(writes.slice(-2).join() === "0:88=-3,0:8=5", "undo: a RESET is one step, its values go back in reverse order");
  st.record(0, 45, 50, 60, t + 9000); st.record(0, 45, 60, 50, t + 9100); ok(st.list.length === 1, "undo: a control dragged back to where it was leaves nothing new to undo");
  st.forget(0); ok(!st.canUndo(0), "undo: a load of the part forgets its entries");
  const { m, u } = await connect(); const s2 = new UndoStack();
  const was = u.parts[0].p[45]; u.writeParam(0, 45, 30); s2.record(0, 45, was, 30); await u.idle();
  await s2.undo(async (wh, id, v) => u.writeParam(wh, id, v)); await u.idle();
  ok(m.parts[0].p[45] === was, "undo against the unit: the old CUTOFF is sent back");
}

// ---- 17: BYPASS (existing parameters only)
{
  const { m, u } = await connect(); await u.readMenu();
  const get = (w, id) => (w === "m" ? u.menu.find((x) => x.id === id)?.value : u.parts[w].p[id]);
  const set = async (w, id, v) => { if (w === "m") await u.setMenu(id, v); else u.writeParam(w, id, v); };
  const b = new Bypass(get, set);
  u.writeParam(0, 33, 40); u.writeParam(0, 96, 3); u.writeParam(0, 34, 50); u.writeParam(1, 34, 20); await u.idle();
  let it = await b.toggle("p0:sat"); await u.idle();
  ok(m.parts[0].p[33] === 0 && b.state("p0:sat").bypassed && it.length === 1 && it[0].oldV === 40, "bypass SATURATION: SAT 0, remembered 40");
  await b.toggle("p0:sat"); await u.idle(); ok(m.parts[0].p[33] === 40 && !b.state("p0:sat").bypassed, "bypass SATURATION again: 40 is back");
  await b.toggle("p0:insert"); await u.idle(); ok(m.parts[0].p[96] === 0, "bypass INSERT: TYPE OFF");
  u.writeParam(0, 96, 5); await u.idle(); ok(!b.state("p0:insert").bypassed && !b.mem.has("p0:insert"), "bypass: a value turned by hand ends the bypass (the hand-set value wins)");
  await b.toggle("bus:chorus"); await u.idle(); ok(m.parts[0].p[34] === 0 && m.parts[1].p[34] === 0, "bypass CHORUS: both parts' chorus sends at 0");
  await b.toggle("bus:chorus"); await u.idle(); ok(m.parts[0].p[34] === 50 && m.parts[1].p[34] === 20, "bypass CHORUS again: both sends back");
  await u.setMenu(35, 3); await u.setMenu(42, 130);
  u.writeParam(0, 8, 5); u.writeParam(0, 88, -4); u.writeParam(0, 87, 6); await u.idle();
  it = await b.toggle("p0:eq"); await u.idle();
  ok(m.parts[0].p[8] === 0 && m.parts[0].p[88] === 0 && m.parts[0].p[103] === 0 && m.parts[0].p[87] === 6 && b.state("p0:eq").bypassed && it.length === 2,
    "bypass EQUALIZER (0.2.25 layout round, one BYPASS, no switch): the three gains to 0 dB (LOW 5, MID -4 remembered), the frequencies kept");
  await b.toggle("p0:eq"); await u.idle(); ok(m.parts[0].p[8] === 5 && m.parts[0].p[88] === -4 && !b.state("p0:eq").bypassed, "bypass EQUALIZER again: the gains are back");
  await b.toggle("dry"); await u.idle();
  const dryOk = [0, 1].every((p) => m.parts[p].p[33] === 0 && m.parts[p].p[96] === 0 && m.parts[p].p[8] === 0 && m.parts[p].p[88] === 0 && m.parts[p].p[103] === 0 && m.parts[p].p[111] === 3 && m.parts[p].p[34] === 0 && m.parts[p].p[35] === 0 && m.parts[p].p[36] === 0) &&
    m.menu.find((x) => x[0] === 35)[6] === 0 && m.menu.find((x) => x[0] === 42)[6] === 100 && m.menu.find((x) => x[0] === 33)[6] === 0;
  ok(dryOk && b.state("dry").bypassed, "DRY: every part section of both parts and the master stages off (RATIO OFF, WIDTH 100 %, BASS MONO OFF)");
  await b.toggle("dry"); await u.idle();
  ok(m.parts[0].p[96] === 5 && m.parts[0].p[34] === 50 && m.parts[1].p[34] === 20 && m.parts[0].p[33] === 40 && m.parts[0].p[8] === 5 && m.parts[0].p[88] === -4 && m.menu.find((x) => x[0] === 35)[6] === 3 && m.menu.find((x) => x[0] === 42)[6] === 130, "DRY again: every value back");
  ok(itemsOf("p1:eq").map((x) => `${x.id}:${x.off}`).join() === "8:0,88:0,103:0" && itemsOf("p1:duck")[0].off === 3 && itemsOf("m:stereo").map((x) => `${x.id}:${x.off}`).join() === "33:0,42:100" && itemsOf("m:comp")[0].off === 0,
    "bypass: EQ is its three gains at 0 dB, DUCK SOURCE OFF is 3, STEREO MASTER is BASS MONO OFF and WIDTH 100 % (as it is), COMPRESSOR RATIO OFF");
}

// ---- 20: INIT, 11 / E: both parts after a load on the unit
{
  const { m, u } = await connect();
  u.writeParam(0, 45, 20); u.writeParam(0, 1, 90); u.writeParam(0, 0, 80); await u.idle(); u.parts[0].name = "MY SOUND";
  await u.initSound(0);
  ok(m.parts[0].p[45] === L.TP[45].def && m.parts[0].p[1] === L.TP[1].def && m.parts[0].p[0] === 80 && m.parts[0].p[ID.P_KTRK] === L.TP[ID.P_KTRK].def && u.parts[0].name === null && m.log.includes(`cmd ${CMD.PRESET}`),
    "INIT: PRESET (engine, 0) + KEY TRACKING default, as the unit's INIT SOUND; the mixer level stays");
  const before = u.parts[1].p.slice();
  m.deviceLoadMulti(5); await sleep(50); await u.reading; await sleep(20);
  ok(u.parts[0].eng === m.parts[0].eng && u.parts[1].eng === m.parts[1].eng && u.parts[1].p.every((v, i) => v === m.parts[1].p[i]) && u.parts[1].p.some((v, i) => v !== before[i]),
    "a multi recalled ON THE UNIT: the unit pushes RELOAD for its selected part only; the editor reads BOTH parts");
  ok(u.parts[1].fromUnit && u.parts[0].fromUnit, "the parts loaded on the unit say so (the unit does not report which stored slot they came from)");
  m.selected = 1; m.parts[1] = defaultPart(E("WAVE")); await sleep(2700);
  ok(u.selected === 1 && u.parts[1].eng === E("WAVE"), "the light poll (TRACK every 2.5 s) finds a change the unit did not push");
  u.disconnect();
}

// ---- 14: the write throttle and the cutoff slew
{
  const { m, u } = await connect(); const sent = [];
  const orig = m.request.bind(m); m.request = async (r, o) => { if (r[0] === CMD.TRACK_PARAM && r[1].length > 2 && r[1][1] === 45) sent.push([Date.now(), r[1][2] | r[1][3] << 7]); return orig(r, o); };
  u.parts[0].p[45] = 82; m.parts[0].p[45] = 82;
  for (let v = 81; v >= 10; v--) u.writeParam(0, 45, v);    // a fast drag: 72 moves at once
  await u.idle();
  const vals = sent.map((x) => x[1] - 8192), steps = vals.map((v, i) => Math.abs(v - (i ? vals[i - 1] : 82)));
  const gaps = sent.slice(1).map((x, i) => x[0] - sent[i][0]);
  ok(vals.at(-1) === 10 && m.parts[0].p[45] === 10, `throttle: the final CUTOFF always reaches the unit (${vals.length} writes for 72 moves)`);
  ok(Math.max(...steps) <= 4 && vals.length < 72, `slew: the multimode CUTOFF walks in steps of at most 4 dial steps (largest ${Math.max(...steps)})`);
  ok(gaps.every((g) => g >= 15), `throttle: about 50 writes a second at most (smallest gap ${Math.min(...gaps)} ms)`);
  sent.length = 0; u.writeParam(0, 1, 100); u.writeParam(0, 1, 5); await u.idle(); ok(m.parts[0].p[1] === 5, "throttle: a parameter without a slew (ATTACK) jumps to its value");
}

// ---- B: FM6 voices
{
  const { m, u } = await connect();
  const pk = await u.readFm6(1); const v = FM6.unpack(pk);
  ok(pk.length === 128 && FM6.name(v) === "INIT VOICE", "FM6: the part's own voice is read whole (FM6_GET target 0)");
  v[FM6.at(1, "OL")] = 77; v[FM6.VI.ALG] = 4; u.writeFm6(1, FM6.pack(v)); await u.idle();
  const back = FM6.unpack(m.parts[1].fm6);
  ok(back[FM6.at(1, "OL")] === 77 && back[FM6.VI.ALG] === 4 && u.parts[1].p[P_E0 + 7] === 8, "FM6: an edit goes back whole (FM6_PUT) and the part's VOICE SOURCE becomes OWN");
  const a1 = algGraph(0), a32 = algGraph(31), a5 = algGraph(4);
  ok(a1.carriers.join() === "1,3" && a1.edges.map((e) => e.join(">")).join() === "6>5,5>4,4>3,2>1" && a1.feedback === 6, "FM6 algorithm 1 from the firmware's table: carriers 1 and 3, 6>5>4>3, 2>1, feedback on 6");
  ok(a32.carriers.length === 6 && a32.edges.length === 0 && a5.carriers.join() === "1,3,5", "FM6 algorithm 32: six carriers; algorithm 5: three pairs");
  ok(opFreq(0, 1, 0) === "x 1.00" && opFreq(0, 0, 0) === "x 0.50" && opFreq(1, 2, 0) === "100 Hz", "FM6: the operator frequency as the unit plays it (ratio 0 = 0.5, fixed decades)");
}

// ---- 1: multis to a file and back (backup objects 13..20)
{
  const { m, u } = await connect();
  const r = await S.readMultis(u);
  ok(S.multiName(r.records[2]) === "TESTONE" && r.records[0] === null, "multis: the stored records are read (M03 TESTONE); a factory slot holds none");
  const file = JSON.parse(JSON.stringify(S.multiFile([{ slot: 2, bytes: r.records[2] }, { slot: 11, bytes: r.records[11] }], u.info.version)));
  const back = S.parseMultiFile(file);
  ok(back.ok && back.multis.length === 2 && back.multis[0].slot === 2 && back.multis[0].name === "TESTONE" && !back.multis[0].problem, "multi file: duell-multis, records whole, slot and name kept");
  await S.importMulti(u, 6, back.multis[1].bytes);
  const r2 = await S.readMultis(u);
  ok(r2.records[6] && r2.records[6].every((x, i) => x === r.records[11][i]) && m.multiStored[6] === "LIVE SET" && r2.records[7] === null && S.multiName(r2.records[2]) === "TESTONE", "multi import: M12's record written into M07 (the partner M08 and the others unchanged)");
  const bad = back.multis[0].bytes.slice(); bad[0] = 5;
  ok(/version 5/.test(S.multiProblem(bad)) && await (async () => { try { await S.importMulti(u, 3, bad); return false; } catch (e) { return /version 5/.test(e.message); } })(), "multi import: a record of another firmware's version is refused before anything is written");
  ok(!S.parseMultiFile({ format: "duell-multis", version: 1, multis: [{ slot: 1, data: file.multis[0].data, size: 3, crc: 1 }] }).multis[0].bytes, "multi file: a damaged entry is left out (checksum)");
}

// ---- 4: pattern slots
{
  const { m, u } = await connect();
  let r = await S.readPatterns(u);
  ok(r.slots[2]?.name === "PAT THREE" && r.slots[2].len === 12 && r.slots.length === 32 && r.slots.every((x) => x && x.name) && r.slots[0].name === "MORODER OCT", "patterns: the 32 slots are read from the unit's objects 21..23 (P03 PAT THREE, 12 steps; every slot holds a named pattern)");
  const rec = S.patternRecord(r.objs, 2), d = S.decodePattern(rec), back = S.encodePattern(d);
  ok(back.every((x, i) => x === rec[i]), "patterns: a record decodes and encodes bit for bit (notes, nudge, ratchet bits)");
  const s0 = { index: 0, n: 2, notes: [60, 67, 0, 0], time: 0, flags: 3, vel: 99, hits: [0, 0, 0], chance: 55, ratchet: 3, nudge: 3 }, s1 = S.unpackStep(S.packStep(s0), 0);
  ok(["n", "time", "flags", "vel", "chance", "ratchet", "nudge"].every((k) => s1[k] === s0[k]) && s1.notes.join() === s0.notes.join(), "patterns: a step with chance, ratchet 3 and nudge -5 survives the 9-byte packing (patslot.c)");
  m.steps[0][5] = { ...m.steps[0][5], n: 1, notes: [72, 0, 0, 0], time: 0, vel: 88, chance: 70, ratchet: 2, nudge: 10 };
  u.writeParam(0, ID.P_SLEN, 24); await u.idle();
  const mine = await S.patternOfPart(u, 0, "MY PATTERN"); await S.storePattern(u, 9, mine);
  r = await S.readPatterns(u);
  ok(r.slots[9]?.name === "MY PATTERN" && r.slots[9].len === 24 && r.slots[2]?.name === "PAT THREE", "patterns: PART 1's pattern stored in P10 (its LENGTH 24 too); P03 is untouched");
  await S.recallPattern(u, 1, S.patternRecord(r.objs, 9));
  ok(m.steps[1][5].notes[0] === 72 && m.steps[1][5].chance === 70 && m.steps[1][5].ratchet === 2 && m.steps[1][5].nudge === 10 && m.parts[1].p[ID.P_SLEN] === 24, "patterns: P10 recalled into PART 2 (steps, chance, ratchet, nudge, LENGTH)");
  const f = S.parsePatternFile(JSON.parse(JSON.stringify(S.patternFile([{ slot: 9, bytes: S.patternRecord(r.objs, 9) }]))));
  ok(f.ok && f.patterns[0].name === "MY PATTERN" && f.patterns[0].bytes.length === 596, "pattern file: duell-patterns round trip");
  const was = Array.from(m.objs.get(21));
  ok(await (async () => { try { await S.storePattern(u, 9, null); return false; } catch (e) { return true; } })() && Array.from(m.objs.get(21)).join() === was.join() && (await S.readPatterns(u)).slots[9].name === "MY PATTERN", "patterns: a slot cannot be emptied (nothing is empty; the unit's ERASE gives the default back): nothing written");
}

// ---- 4b: one list of pattern slots (editor2 A8, job F3): the unit reports 32 slots in the objects 21..23 (PTS3, 11 + 11 + 10), each a pattern; the real protocol, no stand-in ids
{
  const { u } = await connect();
  const r0 = await S.readPatterns(u);
  ok(r0.slots.length === 32 && S.PATTERN_SLOTS === 32, "patterns: the unit lists three objects: 32 slots (11 + 11 + 10), every one a pattern");
  const c = await connect({ pat32: true }), r = await S.readPatterns(c.u);
  ok(r.slots.length === 32 && r.slots.every((x) => x && x.name) && r.slots[2].name === "OFFBEAT" && r.slots[31].name === "SEVEN EIGHT" && r.slots[31].len === 14, `patterns: ?mock=pat32 is the unit's plain defaults: ${r.slots.length} slots, P03 OFFBEAT, P32 SEVEN EIGHT (14 steps)`);
  ok([21, 22, 23].every((id, o) => { const b = c.m.objs.get(id); return b.length > 400 && b.length <= 3584 && String.fromCharCode(...b.slice(0, 4)) === "PTS3" && b[6] === (o < 2 ? 11 : 10) && b[7] === 11 && (b[4] | b[5] << 8) === b.length; }), "patterns: the objects are PTS3: used length = size, 11 / 11 / 10 slots, 11 an object (PS_PER from the header)");
  const rec = S.patternRecord(r.objs, 31), d = S.decodePattern(rec);
  await S.storePattern(c.u, 30, S.encodePattern({ ...d, name: "WRITTEN 31" }));
  const r2 = await S.readPatterns(c.u);
  ok(r2.slots[30].name === "WRITTEN 31" && r2.slots[31].name === "SEVEN EIGHT" && r2.slots[0].name === "MORODER OCT" && S.patternRecord(r2.objs, 30)[7] === 1, "patterns: a slot of the third object (P31) is written and read back, its neighbours untouched, saved by the user (flag)");
  // the compact format: every record packs and unpacks to itself; a pattern that fills an object is refused before anything is written
  const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
  const P = await import("./editor/patslot.js");
  ok([0, 1, 2].every((o) => { const x = P.parsePatObject(o, c.m.objs.get(21 + o)); return !x.error && x.recs.length === P.nslotOf(o) && same(P.buildPatObject(o, x.recs), c.m.objs.get(21 + o)) && x.recs.every((q) => !P.recordProblem(q)); }), "patterns: the 32 records of the objects pack and unpack bit for bit, the object bytes come back the same");
  const heavy = (k) => { const q = new Uint8Array(596); q.set([0xA7, 2, 64, 2, 0, 64, 1, 1]); q.set(new TextEncoder().encode("HEAVY " + k), 8); for (let i = 0; i < 64; i++) { const t = 20 + i * 9; q[t] = 40 + i % 30 | ((i * 3) & 1) << 7; q[t + 1] = 50 + i % 20; q[t + 2] = 55; q[t + 4] = 3 | 8 * 0 | (i & 1) << 5; q[t + 5] = 20 + i; q[t + 8] = (i % 3) * 20; } return q; };
  let refused = null; const before = Array.from(c.m.objs.get(21));
  for (let k = 0; k < 11 && !refused; k++) { try { await S.storePattern(c.u, k, heavy(k)); } catch (e) { refused = e.message; } }
  ok(refused && /does not fit/.test(refused) && c.m.objs.get(21).length <= 3584, `patterns: eleven 64-step chord patterns do not fit one object: the editor says so ("${(refused || "").slice(0, 60)}...") and writes nothing more`);
  ok(!(await (async () => { try { return P.parsePatObject(0, new Uint8Array([0x50, 0x54, 0x53, 0x32, 0, 0, 6, 0])).recs; } catch { return null; } })()) && /older firmware/.test(P.parsePatObject(0, Uint8Array.from([0x50, 0x54, 0x53, 0x32, 0, 0, 6, 0])).error), "patterns: an object of the 16-slot firmware (PTS2) is not read: it says why");
  const old = await connect({ caps: 3 });
  ok((await S.readPatterns(old.u).catch(() => ({ slots: [] }))).slots.length === 0, "patterns: a unit that reports no pattern objects shows no rows (and no placeholder)");
}

// ---- 6: patch CATEGORY
{
  const { m, u } = await connect(); await u.listSlots();
  await LIB.setCategory(u, 0, 5);
  ok(u.slots[0].cat === 5 && m.patches.get(0).cat === 5, "category: a stored patch's category changed (record read whole, byte 6, written whole)");
  ok(await (async () => { try { await LIB.setCategory(u, 3, 2); return false; } catch (e) { return /factory/.test(e.message); } })(), "category: a factory slot's category is fixed (it says so)");
}

// ---- 21: the unit's own settings follow the panel (WIDTH, BASS MONO, COMP, the MENU rows, the globals)
{
  const { m, u } = await connect(); await u.readMenu();
  let menuEvents = 0, descs = 0; u.on("menu", () => { menuEvents++; });
  const orig = m.request.bind(m); m.request = async (r, o) => { if (r[0] === CMD.MENU_DESC) descs++; return orig(r, o); };
  const val = (id) => u.menu.find((x) => x.id === id).value;
  m.deviceMenu(42, 150); m.deviceMenu(33, 6); m.deviceMenu(35, 4); m.deviceMenu(34, -9); m.deviceMenu(39, 60); m.deviceMenu(29, 0); m.deviceMenu(22, 4320);
  m.deviceMenu(6, 0); m.deviceMenu(26, 1); m.deviceGlobalQuiet(ID.G_BPM, 133);
  const bpm0 = u.g[ID.G_BPM];
  u.startPoll(60);
  await sleep(700);
  ok(val(42) === 150 && val(33) === 6 && val(35) === 4 && val(34) === -9 && val(39) === 60 && val(29) === 0 && menuEvents > 0,
    "poll: WIDTH, BASS MONO, COMP RATIO / THRESH, a SIDECHAIN note and a MODULES switch turned ON THE UNIT reach the editor's settings (and a \"menu\" event)");
  ok(val(22) === 4320 && val(6) === 0 && val(26) === 1, "poll: the other MENU rows (TUNE, KNOB ACCEL, CPU METER) follow within a turn of the rotation");
  ok(bpm0 !== 133 && u.g[ID.G_BPM] === 133, "poll: a global (TEMPO) whose push was lost is read from the dump");
  u.disconnect();
  // not while the user holds a control or has a write in flight
  const c = await connect(); await c.u.readMenu();
  c.m.deviceMenu(42, 77); c.m.deviceMenu(33, 3); c.m.deviceGlobalQuiet(ID.G_BPM, 91);
  c.u.isBusy = () => true; c.u.startPoll(50); await sleep(250);
  ok(c.u.menu.find((x) => x.id === 42).value === 100 && c.u.menu.find((x) => x.id === 33).value === 0 && c.u.g[ID.G_BPM] !== 91, "poll: nothing is read over the controls while a pointer is held (a slider being dragged)");
  c.u.isBusy = () => false; c.u.latest.set("m:42", 120);                // a WIDTH write waiting: that one setting is left alone, the others follow
  await sleep(250);
  ok(c.u.menu.find((x) => x.id === 42).value === 100 && c.u.menu.find((x) => x.id === 33).value === 3, "poll: a setting with a write queued is not overwritten (WIDTH stays), the others follow (BASS MONO)");
  c.u.latest.delete("m:42"); await sleep(250);
  ok(c.u.menu.find((x) => x.id === 42).value === 77 && c.u.g[ID.G_BPM] === 91, "poll: once the write is done the unit's value shows");
  // the link is not flooded: the requests of one poll
  const d0 = descs; c.u.disconnect(); const c2 = await connect(); await c2.u.readMenu(); let n = 0;
  const o2 = c2.m.request.bind(c2.m); c2.m.request = async (r, o) => { n++; return o2(r, o); };
  c2.u.startPoll(100); await sleep(1050); c2.u.disconnect();
  ok(n / 10 <= 25, `poll: about ${Math.round(n / 10)} requests a poll (TRACK, ${c2.u.menu.filter((x) => x.id >= 28 && x.id <= 42).length} panel settings, ${Unit.POLL_REST} of the other rows, the dump every ${Unit.POLL_G}th), not the whole menu`);
}

// ---- 22: the slider fill (0.2.25, owner): from the centre only for a range that goes below zero
{
  const all = [], walk = (x, path) => { if (!x || typeof x !== "object") return; if (typeof x.min === "number" && typeof x.max === "number" && typeof x.def === "number") all.push([path, x]); for (const k of Object.keys(x)) walk(x[k], `${path}.${k}`); };
  walk({ TP: L.TP, GP: L.GP, ENG: L.ENG, INS: L.INS, MASTER: L.MASTER, COMP: L.COMP, MSTG: L.MSTG, SIDECH: L.SIDECH, DMD: L.DMD }, "L");
  const wrong = all.filter(([, d]) => isBipolar(d) !== (!d.names && d.min < 0 && d.max > 0));
  ok(all.length > 300 && wrong.length === 0, `fill: all ${all.length} numeric descriptors of the layout (part, global, engine, INSERT, master, sidechain): the centre fill exactly when min < 0 < max (${wrong.length} wrong)`);
  const by = (lab) => all.filter(([, d]) => d.label === lab).map(([, d]) => d);
  ok(by("PAN").every((d) => isBipolar(d)) && by("TUNE").length > 0 && by("TUNE").filter((d) => d.min < 0).every((d) => isBipolar(d)), "fill: PAN and the signed TUNE / offsets / depths fill from the centre");
  const mixes = all.filter(([, d]) => d.min === 0 && d.def === Math.round(d.max / 2));
  ok(mixes.length >= 10 && mixes.every(([, d]) => !isBipolar(d)) && !isBipolar(L.MSTG[0]) && bipolarZero(L.TP[ID.P_PAN]) === 0, `fill: ${mixes.length} controls of 0 .. N with the default in the middle (oscillator MIX 0..127 def 64, WIDTH 0..200 def 100 ...) fill from the left; the zero point is 0`);
}

// ---- cloud job I: the editor's words for the extension slots and the second filter's ids
{
  const { XPART, XVALUE, TWIN_LONG } = await import("./editor/labels.js");
  const { X, NX } = await import("./editor/layout.js");
  const used = Object.keys(X).length;
  ok(Object.keys(XPART).length === used && L.XTP.slice(0, used).every((d, k) => XPART[k] && XPART[k][0] && XPART[k][1].length > 20), "labels: every used extension slot has a long label and a plain tooltip (15 of 16; X15 is the spare)");
  let lenOk = true; for (const [k, m] of Object.entries(XVALUE)) { const d = L.XTP[+k]; lenOk = lenOk && !!d.names && longDesc(d, `x${k}`).names.length === d.names.length && Object.keys(m).every((n) => d.names.includes(n)); }
  ok(lenOk, "labels: the slots' long value names keep the firmware list's length and order (ROUTE: PARALLEL; SUB WAVE: TRIANGLE)");
  ok(longDesc(L.XTP[X.X_CYRISE], "x11") === longDesc(L.XTP[X.X_CYRISE], "x11") && longDesc(L.XTP[X.X_CYRISE], "x11").label === "RISE" && longDesc(L.XSYNC[0], "x11").label === "RISE" && longDesc(L.XSYNC[0], "x11").fk === L.XSYNC[0].fk, "labels: the same descriptor object for the same slot (a drag is never rebuilt); CYCLE RISE keeps its name with SYNC on");
  ok(whyText("TWIN OFF").length > 5 && whyText("TWIN COMB").length > 5 && ["TWIN OFF", "TWIN COMB"].every((k) => WHY[k].length <= 60), "labels: the hide reasons of the second filter are short plain lines (TWIN OFF, TWIN COMB)");
  ok(Object.keys(TWIN_LONG).join() === "OFFSET,CUTOFF,TUNE,RES,FDBK" && NX === 16, "labels: CUTOFF 2 reads OFFSET, CUTOFF or TUNE and RES 2 reads RESONANCE or FEEDBACK by the unit's short labels");
}

// ---- tabs
ok(GLOBAL_TABS.some((t) => t.key === "patterns" && t.pages.includes("PATTERNS")) && pageByTitle("PATTERNS"), "tabs: PATTERNS has a tab of the GLOBAL column");
ok(PART_TABS.map((t) => t.key).join() === "engine,filter,envelope,lfo,matrix,fx,arpseq,roll" && PART_TABS[1].label === "FILTER" && PART_TABS[1].pages.join() === "MULTI FILTER 1/2,MULTI FILTER 2/2", "tabs: FILTER sits between ENGINE and ENVELOPE and holds the multimode filter's two pages (cloud job I)");
{   // editor2 task 12: the editor's own hand-written text names the filters MULTI FILTER and TWIN FILTER (the firmware-driven names, the flow strip and the matrix destinations are not touched)
  const { readFileSync } = await import("node:fs");
  const own = ["labels.js", "app.js"].map((f) => readFileSync(new URL(`./editor/${f}`, import.meta.url), "utf8")).join("\n").split("\n").filter((l) => !/^\s*\/\//.test(l)).join("\n");
  ok(!/[Ff]ilter 2['s:]|filter 1['s ]|\bFilter 2\b/.test(own.replace(/FILTER 2 (TUNE|FEEDBACK|CUTOFF|RESONANCE)|FILTER 1 (CUTOFF|RESONANCE|MODE)/g, "")), "labels: the editor's own tooltips say MULTI FILTER and TWIN FILTER, not filter 1 / filter 2");
  ok(/TWIN FILTER's cutoff follows MULTI FILTER's/.test(own) && /after MULTI FILTER/.test(own), "labels: LINK and the TWIN FILTER card's tooltip use the two names");
}
console.log(fails ? `\nEDITOR 0.2.25 TESTS FAILED (${fails})` : "\neditor 0.2.25 tests passed");
process.exit(fails ? 1 : 0);
