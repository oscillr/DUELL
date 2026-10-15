// SPDX-License-Identifier: GPL-3.0-only
// The sound editor (web/editor/): layout from the firmware tables, descriptors that follow the sound, the honesty rule, and every unit operation
// against the simulated unit (editor/mockunit.js): live GET / SET with WATCH both ways, patches, multis, MENU, MIDI learn, steps, refusals.
//   node web/test_editor.mjs      (use the emsdk node: the Homebrew one is broken here)
import { L, ID, P_E0, descOf, valueText, inertReason, gdescOf, ginert, shimShown, dmdAfterMode, dstWhy, dstName, uncoveredPages, PART_TABS, GLOBAL_TABS, pageByTitle, matrixSlots, pageControls, enumOrder } from "./editor/layout.js";
import { readFile } from "node:fs/promises";
import { MockUnit, MockLink, defaultPart, factoryBrowse } from "./editor/mockunit.js";
import { Unit, UnitError } from "./editor/unit.js";
import { parseInfo } from "./editor/proto.js";
import { themeVars, palette, isDark, lum, mix } from "./editor/theme.js";
import { sysexGranted, watchUnit, HINT } from "./editor/autoconnect.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(104)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const rejects = async (f, re) => { try { await f(); return false; } catch (e) { return re ? re.test(e.message) : true; } };
const E = (n) => L.ENG.findIndex((e) => e.name === n);
async function connect(opt = {}) { const m = new MockUnit(opt), u = new Unit(new MockLink(m)); await u.connect(); return { m, u }; }

// ---- layout: every firmware page is accounted for
{
  ok(uncoveredPages().length === 0, `layout: every page of PAGES[] is on a tab, a duplicate or in the not-editable list (${uncoveredPages().join(",") || "none missing"})`);
  const titles = new Set(L.PAGES.map((p) => p.title));
  ok([...PART_TABS, ...GLOBAL_TABS].every((t) => t.pages.every((p) => titles.has(p))), "layout: every tab page exists in the firmware's PAGES[]");
  ok(L.P_COUNT === 127 && L.P_E0 === 119 && L.G_COUNT === 27 && L.ENG.length === 15 && L.ORDER.length === 10, "layout: 127 parameters, engine parameters from 119, 27 globals, 15 engines, 10 pickable");
  ok(L.ENG[E("WAVE")].edit[0].names.length >= 26, "layout: WAVE's TABLE lists its table names (26 and more) from the firmware");
  ok(pageByTitle("MULTI FILTER").engines.length === 10 && pageByTitle("VCF").engines.join() === String(E("ANALOG")) && pageByTitle("TRIO FILTER").engines.join() === String(E("TRIO")) && pageByTitle("NOISE FILT").engines.join() === String(E("NOISE")), "layout: the MULTI FILTER page is on the ufilt engines, VCF / TRIO FILTER / NOISE FILT on their own engine");
  ok(L.PAGES.filter((p) => p.title === "OP TUNE").every((p) => p.engines.join() === String(E("FM6"))), "layout: the FM6 operator pages belong to FM6 alone");
}
// ---- descriptors that follow the sound
{
  const part = defaultPart(E("ANALOG"));
  const dynAnalog = L.ENG[E("ANALOG")].dyn;
  ok(dynAnalog.length > 0 && dynAnalog.every(([a, b, k, d]) => d.label), "descriptors: ANALOG's mode-dependent labels are exported (the desc hook)");
  const [a, , k, d] = dynAnalog[0], q = defaultPart(E("ANALOG")); q.p[P_E0] = a;
  ok(descOf(q, P_E0 + k).label === d.label, `descriptors: ANALOG ${L.ENG[E("ANALOG")].edit[0].names[a]} relabels E${k + 1} as ${d.label}`);
  const phys = defaultPart(E("PHYS")); const labels = new Set(); for (let m = 0; m <= L.ENG[E("PHYS")].edit[0].max; m++) { phys.p[P_E0] = m; for (let k = 1; k < 8; k++) labels.add(k + descOf(phys, P_E0 + k).label); }
  ok(labels.size > 7, "descriptors: PHYS's labels follow its MODEL");
  const p = defaultPart(0); p.p[ID.P_ITYPE] = 5;
  ok(descOf(p, ID.P_IA).label === "BITS" && descOf(p, ID.P_IA + 1).label === "RATE" && valueText(descOf(p, ID.P_IA), 99).val === "9", "descriptors: INSERT type CRUSH names A B C as BITS RATE LPF, with its own value names");
  p.p[ID.P_ITYPE] = 7; ok(descOf(p, ID.P_IA).label === "RATE" && descOf(p, ID.P_IA + 1).label === "DEPTH", "descriptors: a swept INSERT type: RATE DEPTH FDBK");
  p.p[ID.P_ESYNC] = 1; ok(valueText(descOf(p, ID.P_ATK), 127).val === "4BAR" && valueText(descOf(p, ID.P_E2REL), 0).val === "0", "descriptors: ENV SYNC shows ATK DEC REL (and ENV 2's) as note values");
  p.p[ID.P_ESYNC] = 0; ok(valueText(descOf(p, ID.P_ATK), 10).unit === "ms", "descriptors: ENV SYNC off: times in ms");
  ok(descOf(defaultPart(E("TRIO")), ID.P_ATGT).names.includes("OSC1") && descOf(defaultPart(E("FM6")), ID.P_ATGT).names.includes("OP1") && descOf(defaultPart(E("ANALOG")), ID.P_ATGT).names.join() === "ALL,A,B", "descriptors: ARP TARGET names follow the engine (OSC / OP / A B)");
  const lv = valueText(L.TP[0], L.TP[0].def); ok(lv.val === "0.0" && lv.unit === "dB", `value text: the MIXER LEVEL default (unity) as the unit shows it (${lv.val} ${lv.unit})`);
  const tr = valueText(L.TP[ID.P_TRIM], L.TP[ID.P_TRIM].def); ok(tr.val === "-4.0" && tr.unit === "dB" && L.TP[ID.P_TRIM].label === "TRIM" && L.TP[0].label === "LVL" && L.TP[0].def === 112 && L.TP[ID.P_TRIM].def === 104, `value text: the PATCH TRIM default as the unit shows it (${tr.val} ${tr.unit})`);
  ok(valueText(L.TP[ID.P_FREEA], 82).unit.toLowerCase().includes("khz") && valueText(L.TP[ID.P_FREEA], 0).val.startsWith("30"), "value text: the multimode CUT shows its cutoff, 30 Hz .. kHz, as the firmware formats it");
  ok(enumOrder(L.TP[ID.P_LSYNC])[0] === 0 && enumOrder(L.TP[ID.P_LSYNC]).length === 18 && enumOrder(L.TP[ID.P_SDIV]).length === 17, "value order: enums list in the order the unit's knob steps them (SYNC, DIV)");
  const dst = L.TP[ID.P_M1SRC + 1]; ok(enumOrder(dst).length === L.MD.N && enumOrder(dst)[3] === 38, "value order: the matrix DST list in the unit's grouped order (RES after CUT)");
}
// ---- the hide rule (a control that does not apply is not shown; inertReason / ginert return why, null: it applies)
{
  const mk = (n) => defaultPart(E(n));
  ok(inertReason(mk("ANALOG"), ID.P_IA) === "INSERT OFF" && inertReason({ ...mk("ANALOG"), p: Object.assign(mk("ANALOG").p.slice(), { [ID.P_ITYPE]: 1 }) }, ID.P_IA) === null, "hide rule: INSERT A B C MIX are hidden while TYPE is OFF, live when it is on");
  ok(inertReason(mk("ANALOG"), ID.P_ITYPE) === null, "hide rule: INSERT TYPE itself is never hidden");
  const q = mk("FM6"); ok(inertReason(q, ID.P_ATK) === "OWN ENV" && inertReason(mk("ANALOG"), ID.P_ATK) === null, "hide rule: FM6's own envelopes hide AMP ENV; other engines keep it");
  {
    const g = []; g[ID.G_DMODE] = 0;
    ok(gdescOf(g, ID.G_DMDEP).label === "MDEPTH" && ginert(g, ID.G_DMRATE) === null && ginert(g, ID.G_PINGSPREAD) === "DIGI", "hide rule: delay MODE DIGITAL: DEPTH is DEPTH, RATE live, SPREAD hidden (the plain delay has no side)");
    g[ID.G_DMODE] = 1;
    ok(gdescOf(g, ID.G_DMDEP).label === "MDEPTH" && ginert(g, ID.G_DMRATE) === null && ginert(g, ID.G_PINGSPREAD) === null, "hide rule: delay MODE PING-PONG: nothing hidden (SPREAD, the modulation)");
    g[ID.G_DMODE] = 2;
    ok(gdescOf(g, ID.G_DMDEP).label === "CHANCE" && gdescOf(g, ID.G_DMDEP).max === 100 && ginert(g, ID.G_DMRATE) === "RAND" && ginert(g, ID.G_PINGSPREAD) === null, "hide rule: delay MODE RANDOM: DEPTH is CHANCE, RATE hidden, SPREAD live");
    g[ID.G_DMODE] = 3;
    ok(ginert(g, ID.G_DMRATE) === "OCT+" && gdescOf(g, ID.G_DMODE).label === "MODE" && gdescOf(g, ID.G_DMODE).names.join() === "DIGITAL,PING-PONG,RANDOM,OCT UP", "hide rule: delay MODE OCT UP; MODE lists DIGITAL, PING-PONG, RANDOM, OCT UP");
    g[ID.G_DMODE] = 9; ok(gdescOf(g, ID.G_DMDEP).label === "MDEPTH" && ginert(g, ID.G_PINGSPREAD) === "DIGI", "hide rule: a stored mode past OCT UP plays as DIGITAL");
    {   // DUELL 0.2.19: SHIM's PITCH (id 26) and MOD (id 17) are the unit's page SHIMMER (REVERB page 2): hidden unless TYPE is SHIM, the page after REVERB
      const sh = pageByTitle("SHIMMER"), rv = pageByTitle("REVERB");
      ok(sh && sh.fam === "FX" && sh.scope === "GLOBAL" && sh.ids.join() === [ID.G_SHPITCH, ID.G_SHMOD, -1, -1].join() && ID.G_SHPITCH === 26 && ID.G_SHMOD === 17 && L.PAGES.indexOf(sh) === L.PAGES.indexOf(rv) + 1 &&
         rv.ids.join() === [ID.G_RTYPE, 8, 9, 21].join(), "layout: SHIMMER follows REVERB (PITCH id 26, MOD id 17); REVERB is still TYPE SIZE DAMP LOCUT");
      const d0 = L.GP[ID.G_SHPITCH], d1 = L.GP[ID.G_SHMOD];
      ok(d0.label === "PITCH" && d0.names.join() === "UP,DOWN,BOTH" && d0.def === 0 && d1.label === "MOD" && d1.min === 0 && d1.max === 100 && d1.def === 0 && L.G_COUNT === 27, "layout: PITCH UP / DOWN / BOTH (default UP), MOD 0..100 (default 0), G_COUNT still 27");
      const q = []; q[ID.G_RTYPE] = 3;
      ok(ginert(q, ID.G_SHPITCH) === null && ginert(q, ID.G_SHMOD) === null && shimShown(q), "hide rule: TYPE SHIM shows PITCH and MOD");
      let all = true; for (const t of [0, 1, 2, 4, 5]) { q[ID.G_RTYPE] = t; all = all && ginert(q, ID.G_SHPITCH) === L.GP[ID.G_RTYPE].names[t] && ginert(q, ID.G_SHMOD) === L.GP[ID.G_RTYPE].names[t] && !shimShown(q); }
      ok(all, "hide rule: any other TYPE hides PITCH and MOD (the model's name is the reason), as the unit leaves the page out of its cycle");
    }
    ok(!pageByTitle("DELAY MODE") && pageByTitle("DELAY 1/2").ids.join() === [ID.G_DMODE, 4, 5, 6].join() && pageByTitle("DELAY 2/2").ids[0] === ID.G_PINGSPREAD && pageByTitle("DELAY 2/2").ids[3] === 7, "layout: the delay has two pages, MODE first on DELAY 1/2 and MIX last on DELAY 2/2 (no DELAY MODE page)");
    {   // the CHANCE default (the unit's params.c dly_mode_changed): MODE into RANDOM / OCT UP with DEPTH 0 starts CHANCE at 40; back to a plain mode it returns to 0 unless the player set it
      const h = []; h[ID.G_DMDEP] = 0;
      let r1 = dmdAfterMode(h, 1, 2, false);
      ok(L.DMD.chanceDef === 40 && r1.depth === 40 && r1.auto && dmdAfterMode(h, 0, 1, false).depth === null, "default: MODE PING-PONG -> RANDOM with DEPTH 0 sets CHANCE 40 (DIGITAL -> PING-PONG sets nothing)");
      h[ID.G_DMDEP] = 40; ok(dmdAfterMode(h, 2, 3, true).depth === null && dmdAfterMode(h, 2, 3, true).auto, "default: RANDOM -> OCT UP keeps it");
      ok(dmdAfterMode(h, 3, 0, true).depth === 0 && dmdAfterMode(h, 3, 1, true).depth === 0 && dmdAfterMode(h, 3, 0, false).depth === null, "default: back to a plain mode: 0 again only when the editor set it");
      h[ID.G_DMDEP] = 25; ok(dmdAfterMode(h, 0, 2, false).depth === null, "default: a DEPTH that is not 0 is left alone");
    }
  }
  {   // 0.2.18, the page-order audit: the editor follows the firmware table (layout.json); FM6 shows SLOT first (uiord) with the stored ids unchanged
    const T = (t) => pageByTitle(t).ids;
    ok(L.ENG[E("FM6")].uiord.join() === "7,0,1,2,3,4,5,6" && L.ENG[E("ANALOG")].uiord.join() === "0,1,2,3,4,5,6,7", "page order: FM6 EDIT 1 is SLOT ALG FB MLVL, EDIT 2 MRAT MEG VMOD DTUN (uiord); the other engines in order");
    const lab = (t) => T(t).filter((i) => i >= 0 && i < 255).map((i) => L.TP[i].label).join(" ");
    ok(lab("MULTI FILTER") === "CUT RES MODE SLOPE", "page order: MULTI FILTER is CUT RES MODE SLOPE (0.2.20, owner)");
    ok(lab("ARP 1/3") === "MODE HOLD ORD OCT" && lab("ARP 2/3") === "RATE GATE SWG PROB" && lab("DUCK 2/2") === "CURVE ATK RLS", "page order: ARP 1/3 MODE HOLD ORD OCT, ARP 2/3 RATE GATE SWG PROB, DUCK 2/2 CURVE ATK RLS");
    ok(lab("VOICE 1/2") === "VCE PRIO ALLOC DTUNE" && lab("VOICE 2/2") === "GLMOD GLD SPRD TRIM", "page order: VOICE 1/2 VCE PRIO ALLOC DTUNE, VOICE 2/2 GLMOD GLD SPRD TRIM (0.2.19: the patch trim; the mixer level is the part header's LVL)");
  }
  const r = mk("ANALOG"); r.p[ID.P_LSYNC] = 3; ok(inertReason(r, ID.P_LRATE) === "SYNC ON" && inertReason(r, ID.P_L2RATE) === null, "hide rule: a synced LFO's RATE is hidden");
  const unused = L.ENG.findIndex((e, i) => i !== 1 && i !== 4 && i !== 8 && i !== 10 && i !== 13 && e.edit.some((d) => d.label === "-"));
  if (unused >= 0) { const k = L.ENG[unused].edit.findIndex((d) => d.label === "-"); ok(inertReason(mk(L.ENG[unused].name), P_E0 + k) === "UNUSED", `hide rule: an engine slot labelled "-" is UNUSED (${L.ENG[unused].name} E${k + 1})`); }
  ok(dstWhy(mk("WAVE"), L.MD.E1) === "REBUILD" && dstWhy(mk("FM6"), L.MD.E1 + 7) === "LOADS", "matrix: WAVE's TABLE is a REBUILD and FM6's SLOT LOADS a patch: both do nothing as a destination");
  ok(dstWhy(mk("ANALOG"), L.MD.RES) === null && dstWhy(mk("WAVE"), L.MD.RES) === null && dstWhy(defaultPart(10), L.MD.RES) === "ENGINE", "matrix: RES is live on the filter engines, 'ENGINE' where there is no filter (a retired engine)");
  ok(dstWhy(mk("ANALOG"), L.MD.INSA) === "INS OFF" && dstWhy(mk("ANALOG"), 2) === null, "matrix: INS A..C and IMIX do nothing while the INSERT is off");
  const w = mk("ANALOG"); w.p[ID.P_ITYPE] = 5; ok(dstName(w, L.MD.INSA) === "BITS" && dstName(mk("WAVE"), L.MD.E1) === L.ENG[E("WAVE")].edit[0].label, "matrix: INS A is named by the INSERT type, E1..E8 by the engine's labels");
  ok(matrixSlots(pageByTitle("MATRIX 5-8", "LFO")).map((s) => s.src).join() === "61,64,67,70" && matrixSlots(pageByTitle("MATRIX 1-4", "LFO")).map((s) => s.n).join() === "1,2,3,4", "matrix: slots 1..4 at ids 49.., 5..8 at ids 61..");
  const duck = pageByTitle("DUCK 1/2"), dp = mk("ANALOG");
  const idOf = (src) => { dp.p[ID.P_DKSRC] = src; return pageControls(duck, dp)[1].id; };
  ok(idOf(0) === ID.P_DKRATE && idOf(1) === ID.P_DKTHR && idOf(5) === ID.P_DKNOTE, "page: DUCK's second knob is RATE for PUMP, THR for PART, NOTE for a MIDI channel");
}
// ---- connect, read, live edit with WATCH
{
  const { m, u } = await connect();
  ok(u.writable && u.parts.length === 2 && u.parts[0].eng === 0 && u.parts[1].eng === E("FM6") && u.g.length === 27 && u.info.menuCount === 25, "connect: INFO checked against the firmware tables, both parts and the globals read");
  const seen = []; u.on("param", (d) => seen.push(d));
  u.writeParam(1, ID.P_FREEA, 40); await u.idle();
  ok(m.parts[1].p[ID.P_FREEA] === 40 && u.parts[1].p[ID.P_FREEA] === 40, "edit: a slider moves part 2's CUT on the unit without selecting it (TRACK_PARAM)");
  ok(m.selected === 0, "edit: the unit's selected part did not change");
  m.log.length = 0; for (let v = 0; v < 50; v++) u.writeParam(0, (ID.P_FREEA + 1), v); await u.idle();
  ok(m.parts[0].p[(ID.P_FREEA + 1)] === 49 && m.log.filter((x) => x === "cmd 31").length < 50, `edit: a dragged slider sends the latest value, not every step (${m.log.filter((x) => x === "cmd 31").length} requests for 50 moves)`);
  u.writeParam(0, (ID.P_FREEA + 1), 9999); await u.idle(); ok(u.parts[0].p[(ID.P_FREEA + 1)] === 127, "edit: out of range is clamped to the descriptor's maximum");
  m.deviceTurn(0, ID.P_FREEA, 12);
  ok(u.parts[0].p[ID.P_FREEA] === 12 && seen.at(-1).byUs === false, "WATCH: a knob turned on the unit (the selected part) updates the page");
  m.deviceTurn(1, ID.P_LEVEL, 50); ok(u.parts[1].p[ID.P_LEVEL] === 50, "WATCH: LEVEL of the other part follows too (TRACK_CHANGED)");
  m.deviceGlobal(L.GP.findIndex((d) => d.label === "TEMPO"), 150); ok(u.g[0] === 150, "WATCH: a global changed on the unit (TEMPO) updates the page");
  m.deviceSelect(1); await new Promise((r) => setTimeout(r, 20)); ok(u.selected === 1, "WATCH: ALGORITHM on the unit moves the selection marker (RELOAD)");
  u.writeGlobal(0, 99); await u.idle(); ok(m.g[0] === 99, "edit: a global is written with SET scope 1");
  await u.setEngine(0, E("PHYS")); ok(u.parts[0].eng === E("PHYS") && m.parts[0].eng === E("PHYS") && m.selected === 0, "engine: setting an engine selects the part, sets G_ENGSEL and re-reads the part");
  // linked
  u.parts[0].p[ID.P_FREEA] = 30; m.parts[0].p[ID.P_FREEA] = 30; u.parts[1].p[ID.P_FREEA] = 50; m.parts[1].p[ID.P_FREEA] = 50;
  u.writeParamLinked(0, ID.P_FREEA, 40); await u.idle();
  ok(m.parts[0].p[ID.P_FREEA] === 40 && m.parts[1].p[ID.P_FREEA] === 60, "LINK: the same step on both parts (+10 each, each from its own value)");
  u.writeParamLinked(0, P_E0 + 1, u.parts[0].p[P_E0 + 1] + 1); await u.idle();
  ok(m.parts[1].p[P_E0 + 1] === defaultPart(E("FM6")).p[P_E0 + 1], "LINK: engine parameters are not linked while the parts play different engines");
  u.writeParamLinked(1, ID.P_ITYPE, 2); await u.idle(); ok(m.parts[0].p[ID.P_ITYPE] === 2, "LINK: an enum is copied (INSERT TYPE)");
  u.writeParamLinked(1, ID.P_IA, 10); await u.idle(); ok(m.parts[1].p[ID.P_IA] === 10 && m.parts[0].p[ID.P_IA] === 10, "LINK: INSERT A moves on both once the INSERT is on (it was faded before)");
}
// ---- an unknown firmware is read-only
{
  const m = new MockUnit(); const real = m.info.bind(m);
  m.info = () => { const a = real(); a[a.indexOf(127) >= 0 ? a.indexOf(127) : 0] = 100; return a; };
  const u = new Unit(new MockLink(m)); await u.connect();
  ok(!u.writable && /Nothing is written/.test(u.compat), "safety: a unit with another parameter count is read-only, with the reason");
  ok(await rejects(async () => u.writeParam(0, 5, 1), /Nothing is written/), "safety: writes to such a unit are refused before anything is sent");
}
// ---- patches: ONE list of 128 slots (DUELL 0.2.17): a slot is its saved record, else the factory sound at that browse position
{
  const { m, u } = await connect();
  const list = await u.listSlots();
  ok(list.length === 128 && list.every((s) => s.used !== false) && list.filter((s) => s.saved).map((s) => s.name).join() === "WARM PAD,SUB BASS,BELL,WAVE LEAD",
     "patches: 128 slots listed, every one holds a sound; four are saved (the others hold their factory sounds)");
  {   // the factory fallback: slot i is the factory sound at browse position i, the order of docs/PATCH_NUMBERS.md (category, engine, name)
    const doc = (await readFile(new URL("../../docs/PATCH_NUMBERS.md", import.meta.url), "utf8")).split("\n")
      .map((l) => /^\| (\d+) \| ([^|]+?)( \(NEW\))? \|/.exec(l)).filter(Boolean).map((r) => [+r[1], r[2]]);
    const fb = factoryBrowse();
    ok(doc.length === 128 && fb.length === 128 && doc.every(([n, name], i) => n === i + 1 && fb[i].name === name), "patches: the browse order (category, engine, name) is the numbering of docs/PATCH_NUMBERS.md: 128 sounds, #1 .. #128");
    ok(list.every((s, i) => s.saved || (s.name === fb[i].name && s.eng === fb[i].eng && s.cat === fb[i].cat)), "patches: every slot that is not saved lists its browse position's factory sound (name, engine, category)");
  }
  ok((await u.factoryNames(0)).join() === L.ENG[0].sounds.map((s) => s[0]).join(), "patches: the factory sounds of an engine come from NAMES");
  await u.loadSlot(1, 40); { const f = factoryBrowse()[40]; ok(u.parts[1].eng === f.eng && u.parts[1].preset === f.idx && m.selected === 1, "patches: loading a slot that holds its factory sound loads that sound into the chosen part"); }
  u.parts[1].p[ID.P_LEVEL] = 33; m.parts[1].p[ID.P_LEVEL] = 33; await u.saveSlot(1, 9, "MY SOUND");
  ok(u.slots[9].name === "MY SOUND" && u.slots[9].saved && m.patches.get(9).p[ID.P_FREEA] === m.parts[1].p[ID.P_FREEA], "patches: UP_STORE saves the part's own sound over a slot that held a factory sound");
  m.parts[0].p[ID.P_LEVEL] = 77; m.parts[0].p[ID.P_PAN] = 12; await u.loadSlot(0, 9); ok(m.parts[0].p[ID.P_LEVEL] === 77 && m.parts[0].p[ID.P_PAN] === 12, "patches: a load keeps the part's LEVEL (the mixer level: a sound load never changes it), PAN and MUTE");
  { // 0.2.19: the trim is the patch's: a load sets it, a save stores it (and not the mixer level)
    m.parts[1].p[ID.P_TRIM] = 91; m.parts[1].p[ID.P_LEVEL] = 33; await u.saveSlot(1, 10, "TRIMMED");
    ok(m.patches.get(10).p[ID.P_TRIM] === 91, "patches: UP_STORE keeps the part's TRIM in the patch");
    m.parts[0].p[ID.P_LEVEL] = 58; m.parts[0].p[ID.P_TRIM] = 100; await u.loadSlot(0, 10);
    ok(m.parts[0].p[ID.P_TRIM] === 91 && m.parts[0].p[ID.P_LEVEL] === 58, "patches: a load sets the TRIM (91) and keeps the mixer LEVEL (58)");
    await u.restoreSlot(10);
  }
  await u.saveSlot(1, 9, "RENAMED 1"); ok(u.slots[9].name === "RENAMED 1" && u.slots.filter((s) => s.saved).length === 5, "patches: saving over a saved slot replaces it (no new slot)");
  await u.restoreSlot(9); ok(!u.slots[9].saved && u.slots[9].name === factoryBrowse()[9].name, "patches: RESTORE: the saved sound is gone, the slot's factory sound is back");
  m.playing = true; ok(await rejects(() => u.saveSlot(0, 20, "X"), /stop playback/i), "patches: saving while the unit plays says to stop playback (rc 2), nothing written");
  ok(!m.patches.has(20), "patches: ... and nothing was saved");
  ok(await rejects(() => u.restoreSlot(5), /could not restore/) && m.patches.has(5), "patches: RESTORE of a saved slot while the unit plays is refused (rc 2), the record stays");
  await u.restoreSlot(20); ok(!u.slots[20].saved, "patches: RESTORE of a slot that holds its factory sound is rc 0 and writes nothing (even playing)");
  m.playing = false;
  ok(u.info.patchCap === 128 && u.info.caps === 31, "patches: the unit reports 128 slots (INFO tag 58 01) and caps 31");
  for (let s = 0; s < 128; s++) if (!u.slots[s].saved) await u.saveSlot(0, s, "F" + s);
  ok(u.slots.filter((x) => x.saved).length === 128, "patches: all 128 slots can be saved, one after the other (there is no full)");
  await u.saveSlot(0, 127, "LAST"); ok(u.slots[127].name === "LAST" && u.slots.every((x) => x.saved), "patches: with all 128 saved a slot is still written (over what it holds)");
  m.playing = true; ok(await rejects(() => u.restoreAll(), /could not restore/), "patches: RESTORE ALL while the unit plays is refused"); m.playing = false;
  const n = await u.restoreAll(); const fb = factoryBrowse();
  ok(n === 128 && u.slots.every((x, i) => !x.saved && x.name === fb[i].name) && m.patches.size === 0, "patches: RESTORE ALL: every saved slot restored, the 128 slots hold their factory sounds");
  ok(await u.restoreAll() === 0, "patches: RESTORE ALL with nothing saved: nothing to do");
  { const old = new MockUnit({ savedFlags: false }); const uo = new Unit(new MockLink(old)); await uo.connect(); const l = await uo.listSlots(); ok(l.length === 128 && l.every((s) => s.saved === s.used), "patches: a unit that sends no SAVED bytes (before 0.2.17) is read with saved = used"); }
  { const old = new MockUnit({ caps: 3 }); const uo = new Unit(new MockLink(old)); await uo.connect(); ok(uo.info.patchCap === null && (await uo.listSlots()).length === 128, "patches: a unit without the capacity tag still lists its 128 slots"); }
}
// ---- multis
{
  const { m, u } = await connect();
  const used = await u.multiUsed(); ok(used.length === 16 && used[0] && used[2] && !used[10] && used[11], "multis: used flags of M01..M16 (PROJECT query)");
  const names = await u.readMultiObjects(); ok(names[2] === "TESTONE" && names[3] === "TESTTWO" && names[11] === "LIVE SET" && names[0] === null, "multis: stored names read from the multi objects");
  await u.renameMulti(2, "NEW NAME"); ok(u.multiNames[2] === "NEW NAME" && u.multiNames[3] === "TESTTWO" && m.multiStored[2] === "NEW NAME", "multis: rename writes the object; the partner multi in it is unchanged");
  ok(await rejects(() => u.renameMulti(0, "FACTORY"), /not stored/), "multis: a factory (not stored) multi cannot be renamed: it says to save it first");
  ok(await rejects(() => u.renameMulti(2, "bad\u0001name"), /plain characters/) && await rejects(() => u.renameMulti(2, ""), /plain characters/) && await rejects(() => u.renameMulti(2, "THIRTEEN CHARS"), /plain characters/), "multis: names are 1..12 plain characters");
  await u.saveMulti(12); ok(m.multiStored[12] === "MULTI 13" && u.multis[12], "multis: save stores the slot (PROJECT save) and refreshes used");
  await u.loadMulti(3); ok(true, "multis: load re-reads both parts");
  m.playing = true; ok(await rejects(() => u.saveMulti(13), /stop playback/), "multis: save while playing: no reply from the unit becomes a plain 'stop playback' message");
  ok(await rejects(() => u.readMultiObjects(), /Stop playback/), "multis: reading names while playing is refused with a plain message");
}
// ---- MENU and MIDI learn
{
  const { m, u } = await connect();
  const items = await u.readMenu(); ok(items.length === 25 && items.some((i) => i.name === "COLOR" && i.names.length === L.PALETTES.LIST.length && i.names.join() === L.PALETTES.LIST.map((p) => p.name).join()) && items.find((i) => i.name === "TUNE").unit === "ct", "menu: 26 items with names, ranges, units and tabs as the unit describes them");
  {   // DUELL 0.2.14: GLO > MODULES (ids 28..31, tab 4 "MODULES") and PERF LEAVE (id 32, CONTROL) are offered like any MENU setting
    const mods = items.filter((i) => i.id >= 28 && i.id <= 31), pl = items.find((i) => i.id === 32);
    ok(mods.length === 4 && mods.map((i) => i.name).join() === "ARP,SEQ,SENDS,PART FX" && mods.every((i) => i.tabName === "MODULES" && i.tab === 4 && i.names.join() === "OFF,ON" && i.value === 1)
       && pl && pl.name === "PERF LEAVE" && pl.tabName === "CONTROL" && pl.names.join() === "FADE,KEEP", "menu: the MODULES switches (28..31, tab MODULES, ON) and PERF LEAVE (32, CONTROL, FADE / KEEP)");
    const bmn = items.find((i) => i.id === 33);
    ok(bmn && bmn.name === "BASS MONO" && bmn.tabName === "MASTER" && bmn.value === 0 && bmn.names.length === 12 && bmn.names[0] === "OFF" && bmn.names[5] === "100 Hz" && bmn.names[11] === "400 Hz" && await u.setMenu(33, 5) === 5 && await u.setMenu(33, 99) === 11 && await u.setMenu(33, 0) === 0, "menu: BASS MONO (33, tab MASTER, OFF + 40 .. 400 Hz) is offered and set");
    ok(await u.setMenu(30, 0) === 0 && m.menu.find((x) => x[0] === 30)[6] === 0 && await u.setMenu(32, 1) === 1, "menu: SENDS OFF and PERF LEAVE KEEP are set");
    const rv = L.PAGES.find((p) => p.title === "REVERB"), lc = L.GP[rv.ids[3]];
    ok(rv.ids.length === 4 && lc.label === "LOCUT" && lc.def === 61 && u.g.length === L.G_COUNT, "layout: REVERB K4 is LOCUT (default 61), so the BUSES tab's REVERB card carries it");
    ok(L.GP.find((d) => d.label === "TYPE" && /PLATE/.test((d.names || []).join())) || L.GP.some((d) => (d.names || []).includes("PLATE")), "layout: REVERB TYPE offers PLATE");
    const tn = L.TP[ID.P_ITYPE].names; ok(tn.length === 11 && tn[9] === "TAPE" && tn[10] === "VINYL" && L.INS[9].map((d) => d.label).join() === "WOW,SAT,TONE" && L.INS[10].map((d) => d.label).join() === "DUST,NOISE,WARP", "layout: INSERT TYPE 9 TAPE (WOW SAT TONE) and 10 VINYL (DUST NOISE WARP)");
  }
  const top = L.PALETTES.LIST.length - 1; ok(await u.setMenu(0, 99) === top && m.menu.find((x) => x[0] === 0)[6] === top, "menu: a value out of range comes back clamped by the unit");
  await u.readLearn(); ok(Array.isArray(u.learn) && u.learn.length === 0, "learn: the map is read (empty)");
  await u.learnSet(74, 1, ID.P_FREEA); ok(u.learn.length === 1 && u.learn[0].cc === 74 && u.learn[0].track === 1, "learn: assign CC74 to part 2's CUT");
  ok(await rejects(() => u.learnSet(1, 0, 5), /never learned/), "learn: a CC the unit never learns is refused with a plain message");
  for (let cc = 2; cc < 17; cc++) await u.learnSet(cc + 40, 0, 5 + cc); ok(await rejects(() => u.learnSet(100, 0, 3), /full/), "learn: the 17th entry says the map is full");
  await u.learnClear(74); ok(u.learn.every((e) => e.cc !== 74), "learn: clear one"); await u.learnClear(null); ok(u.learn.length === 0, "learn: clear all");
}
// ---- steps
{
  const { m, u } = await connect();
  const s = await u.readSteps(1, 16); ok(s.length === 16 && s[0].n === 1 && s[0].notes[0] === 48 && s[1].time === 2, "steps: 16 steps read from part 2 without selecting it");
  const w = await u.writeStep(1, 1, { ...s[1], time: 0, n: 1, notes: [60, 0, 0, 0] }); ok(w.notes[0] === 60 && m.steps[1][1].time === 0, "steps: a step written to part 2 (TRACK_STEP)");
}

// ---- the new layout: tabs per part, the global column, the theme from the unit's palette
{
  const keys = PART_TABS.map((t) => t.key).join(), gk = GLOBAL_TABS.map((t) => t.key).join();
  ok(keys === "engine,mod,matrix,fx,arpseq,roll", `layout: each part column has the tabs ENGINE MOD MATRIX FX ARP / SEQ PIANO ROLL (${keys})`);
  ok(gk === "global,patches,multis,midi,tables,settings", `layout: the global column has the tabs GLOBAL PATCHES MULTIS MIDI WAVETABLES SETTINGS (${gk})`);
  const part = new Set(PART_TABS.flatMap((t) => t.pages)), glob = new Set(GLOBAL_TABS.flatMap((t) => t.pages));
  ok(["MULTI FILTER", "VCF", "FX", "INSERT 1/2", "DUCK 2/2", "ARP 1/3", "STEP", "LFO 2 DEST", "MATRIX 5-8"].every((t) => part.has(t)) && ["CHORUS", "DELAY 1/2", "DELAY 2/2", "REVERB", "TEMPO", "MODULES"].every((t) => glob.has(t)), "layout: filter, FX, arp and step pages are in the part tabs, buses and tempo in the global column");
  ok([...part].every((t) => !glob.has(t)), "layout: no page is in both a part tab and the global column");
  const P = L.PALETTES;
  ok(P.LIST.length === 9 && P.LIST[P.DEFAULT].name === "VIOLET" && P.LIST.every((p) => /^#[0-9a-f]{6}$/.test(p.text) && p.c.length === 4 && p.bg.length === 2), "theme: the firmware's 9 palettes (UI_PALETTES) are in layout.json, VIOLET is the default");
  const vs = [0, 1, 2].map((k) => themeVars(P.DEFAULT, k));
  ok(vs.every((v) => /^#[0-9a-f]{6}$/.test(v["--bg"]) && /^#[0-9a-f]{6}$/.test(v["--acc"])) && vs[0]["--acc"] === P.LIST[0].r[0] && vs[1]["--acc"] === P.LIST[0].r[2], "theme: a part column takes its own accent (A of part 1, A of part 2) from the palette");
  ok(P.LIST.every((p, i) => Math.abs(lum(themeVars(i, 0)["--fg"]) - lum(themeVars(i, 0)["--bg"])) > 0.3), "theme: text and card colours are well apart in every palette");
  ok(isDark(0) && !isDark(P.LIST.findIndex((p) => p.name === "PALE")) && !isDark(P.LIST.findIndex((p) => p.name === "MINT")), "theme: dark palettes give the dark scheme, PALE and MINT the light one");
  const { u, m } = await connect();
  const item = async () => (await u.readMenu()).find((i) => i.id === 0).value;
  ok(await item() === P.DEFAULT, "theme: the mock unit starts on the default palette (MENU id 0)");
  m.devicePalette(3); ok(await item() === 3 && palette(3).name === "COBALT", "theme: a COLOR change on the unit is read back through MENU_DESC");
  let seen = null; u.on("menu", (it) => { seen = it.find((x) => x.id === 0).value; }); await u.setMenu(0, 5); ok(seen === 5, "theme: setting COLOR from the page emits a menu event (the page re-themes at once)");
}
// ---- auto-connect (0.2.20): MIDI with SysEx already allowed -> connect at load and when the unit is plugged in later (permissions mocked)
{
  const nav = (state, boom) => { const o = { last: null, permissions: { query: async (q) => { o.last = q; if (boom) throw new TypeError("no midi"); return { state }; } } }; return o; };
  const n1 = nav("granted"); ok(await sysexGranted(n1) === true && n1.last.name === "midi" && n1.last.sysex === true, "auto-connect: permission 'granted' for midi with sysex: connect by itself (the query asks for sysex)");
  ok(await sysexGranted(nav("prompt")) === false && await sysexGranted(nav("denied")) === false, "auto-connect: 'prompt' or 'denied': no auto-connect (the Connect button stays, a click is needed for the first grant)");
  ok(await sysexGranted(nav("granted", true)) === false && await sysexGranted({}) === false, "auto-connect: no Permissions API, or a browser that does not know the name 'midi': not granted, nothing breaks");
  ok(/Connect/.test(HINT) && /SysEx/.test(HINT), "auto-connect: the plain hint tells to press Connect once");
  let timers = [], went = 0, want = true, before = 0;
  const access = { onstatechange: () => { before++; } };
  watchUnit(access, { wanted: () => want, go: () => { went++; }, delay: 600, timer: (fn, d) => { const t = { fn, d, live: true }; timers.push(t); return t; }, clear: (t) => { if (t) t.live = false; } });
  const fire = () => { for (const t of timers) if (t.live) { t.live = false; t.fn(); } };
  const ev = (name, state) => access.onstatechange({ port: { name, state, type: "output" } });
  ev("FM-1 DUELL", "connected"); ev("FM-1 DUELL", "connected"); fire();
  ok(went === 1 && before === 2, "auto-connect: the unit plugged in later (input and output events) connects once after the delay; the page's own statechange handler still runs");
  ev("Some Other Synth", "connected"); fire(); ev("FM-1 DUELL", "disconnected"); fire();
  ok(went === 1, "auto-connect: another device, or a disconnect, does not connect");
  want = false; ev("Felucca", "connected"); fire();
  ok(went === 1, "auto-connect: not wanted (already connected, or the user pressed Disconnect): no connect");
  want = true; ev("Felucca FM-1", "connected"); want = false; fire();
  ok(went === 1, "auto-connect: no longer wanted when the timer fires: no connect");
}
console.log(fails ? `\nEDITOR WEB TESTS FAILED (${fails})` : "\neditor web tests passed");
process.exit(fails ? 1 : 0);
