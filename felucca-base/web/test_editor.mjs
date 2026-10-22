// SPDX-License-Identifier: GPL-3.0-only
// The sound editor (web/editor/): layout from the firmware tables, descriptors that follow the sound, the honesty rule, and every unit operation
// against the simulated unit (editor/mockunit.js): live GET / SET with WATCH both ways, patches, multis, MENU, MIDI learn, steps, refusals.
//   node web/test_editor.mjs      (use the emsdk node: the Homebrew one is broken here)
import { L, ID, P_E0, descOf, valueText, inertReason, voiceWhy, isEnum, gdescOf, ginert, shimShown, dmdAfterMode, dstWhy, dstName, uncoveredPages, PART_TABS, GLOBAL_TABS, pageByTitle, matrixSlots, pageControls, enumOrder, matrixSecondShown, isBipolar, bipolarZero } from "./editor/layout.js";
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
  ok(pageByTitle("MULTI FILTER 1/2").engines.length === 10 && pageByTitle("MULTI FILTER 2/2").engines.length === 10 && !L.PAGES.some((p) => p.title === "VCF" || p.title === "TRIO FILTER" || p.title === "NOISE FILT"), "layout: the MULTI FILTER pages (1/2, 2/2) are on all ten engines, VCF / TRIO FILTER / NOISE FILT are gone (cloud job J)");
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
      ok(sh && sh.fam === "GLO" && sh.scope === "GLOBAL" && sh.ids.join() === [ID.G_SHPITCH, ID.G_SHMOD, -1, -1].join() && ID.G_SHPITCH === 26 && ID.G_SHMOD === 17 && L.PAGES.indexOf(sh) === L.PAGES.indexOf(rv) + 1 &&
         rv.ids.join() === [ID.G_RTYPE, 8, 9, 21].join(), "layout: SHIMMER follows REVERB (PITCH id 26, MOD id 17); REVERB is still TYPE SIZE DAMP LOCUT");
      const d0 = L.GP[ID.G_SHPITCH], d1 = L.GP[ID.G_SHMOD];
      ok(d0.label === "PITCH" && d0.names.join() === "UP,DOWN" && d0.max === 1 && d0.def === 0 && d1.label === "MOD" && d1.min === 0 && d1.max === 100 && d1.def === 0 && L.G_COUNT === 27, "layout: PITCH UP / DOWN (default UP; BOTH removed in 0.2.25), MOD 0..100 (default 0), G_COUNT still 27");
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
    ok(L.ENG[E("FM6")].uiord.join() === "7,0,1,2,3,4,5,6" && L.ENG[E("ANALOG")].uiord.join() === "0,1,2,3,6,4,5,7", "page order: FM6 EDIT 1 is SLOT ALG FB MLVL, EDIT 2 MRAT MEG VMOD DTUN (uiord); the other engines in order");
    const lab = (t) => T(t).filter((i) => i >= 0 && i < 255).map((i) => L.TP[i].label).join(" ");
    ok(lab("MULTI FILTER 1/2") === "CUT RES MODE SLOPE", "page order: MULTI FILTER 1/2 is CUT RES MODE SLOPE (0.2.20, owner)");
    ok(lab("MULTI FILTER 2/2") === "KTRK FLT FLT FLT" && T("MULTI FILTER 2/2")[1] === ID.P_E2FLT && T("MULTI FILTER 2/2")[2] === ID.P_LD_FLT && T("MULTI FILTER 2/2")[0] === ID.P_KTRK && T("MULTI FILTER 2/2")[3] === ID.P_L2FLT,
       "page order: MULTI FILTER 2/2 (0.2.22) is KTRK, ENV 2's, LFO 1's and LFO 2's cutoff amounts (no AMP ENV card, owner) (the ids of the DEST pages' FLT cards, one stored value each, on both pages; the unit labels them ENV 2 / LFO 1 / LFO 2)");
    ok(ID.P_KTRK === 27 && L.TP[27].label === "KTRK" && L.TP[27].min === -63 && L.TP[27].max === 63 && L.TP[27].def === 32 && L.TP[27].fmt === 27, "KEY TRK: id 27, KTRK -63..+63, default 32 (0.2.22: +51 %), format 27 (0.2.21)");
    ok(valueText(L.TP[27], 63).val === "+100" && valueText(L.TP[27], -63).val === "-100" && valueText(L.TP[27], 0).val === "0" && valueText(L.TP[27], 32).val === "+51", "KEY TRK: shown -100 .. 0 .. +100 % as the firmware formats it");
    {   const ix = (t) => L.PAGES.findIndex((p) => p.title === t), edit = ["EDIT 2", "MULTI FILTER 1/2", "MULTI FILTER 2/2"].map(ix);
      ok(edit.every((i, k) => i >= 0 && (k === 0 || i > edit[k - 1])), "page order: EDIT 2, then MULTI FILTER 1/2, 2/2 (signal order, 0.2.21; the engines' own filters are gone, cloud job J)");
      const tab = PART_TABS.find((t) => t.key === "engine").pages, ft = PART_TABS.find((t) => t.key === "filter").pages;
      ok(!tab.includes("VCF") && !tab.includes("TRIO FILTER") && !tab.includes("NOISE FILT") && !tab.some((t) => t.startsWith("MULTI FILTER")) && ft.join() === "MULTI FILTER 1/2,MULTI FILTER 2/2",
         "layout: the ENGINE tab has no engine filter pages, the multimode pages are on the FILTER tab (cloud job I), in order"); }
    ok(lab("ARP 1/3") === "MODE HOLD ORD OCT" && lab("ARP 2/3") === "RATE GATE SWG PROB" && lab("DUCK 2/2") === "CURVE ATK RLS", "page order: ARP 1/3 MODE HOLD ORD OCT, ARP 2/3 RATE GATE SWG PROB, DUCK 2/2 CURVE ATK RLS");
    ok(/^VCE PRIO DTUNE( -)?$/.test(lab("VOICE 1/2")) && lab("VOICE 2/2") === "GLMOD GLD SPRD TRIM", "page order: VOICE 1/2 VCE PRIO DTUNE (T0227: no ALLOC), VOICE 2/2 GLMOD GLD SPRD TRIM (0.2.19: the patch trim; the mixer level is the part header's LVL)");
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
  ok(idOf(0) === ID.P_DKRATE && idOf(1) === ID.P_DKTHR && (dp.p[ID.P_DKSRC] = 2, pageControls(duck, dp).length === 2 && pageControls(duck, dp).every((c) => c.id !== ID.P_CUT2 && c.id !== ID.P_DKRATE && c.id !== ID.P_DKTHR)),
     "page: DUCK's second knob is RATE for PUMP, THR for PART, none for MIDI (0.2.22: the channel and the note are the device's SIDECHAIN setup)");
  dp.p[ID.P_DKSRC] = 3;
  ok(pageControls(duck, dp).length === 2 && inertReason(dp, ID.P_DKDEP) === "DUCK OFF" && inertReason(dp, ID.P_DKATK) === "DUCK OFF" && inertReason(dp, ID.P_DKRLS) === "DUCK OFF" && inertReason(dp, ID.P_DKCRV) === "DUCK OFF"
     && inertReason(dp, ID.P_DKSRC) === null && (dp.p[ID.P_DKSRC] = 0, inertReason(dp, ID.P_DKDEP) === null),
     "page: DUCK SRC OFF (0.2.24): no second knob, DEPTH ATK RLS CURVE hidden (DUCK OFF), SRC itself shown; PUMP shows them again");
  ok(L.TP[ID.P_DKSRC].names.join() === "PUMP,PART,MIDI,OFF" && L.TP[ID.P_DKSRC].def === 3 && L.TP[ID.P_CUT2].label === "CUTOFF" && pageByTitle("SIDECHAIN").scope === "MIDI" && pageByTitle("SIDECHAIN").global === 1 && pageByTitle("SIDECHAIN").ids.every((i) => i < 0)
     && pageByTitle("SEND FX").global === 0 && pageByTitle("SEND FX").scope === "FXSUM" && !L.PAGES.some((p) => p.title === "GLOBAL FX"), "pages: DUCK SRC is PUMP, PART, MIDI, OFF (0.2.24: OFF the new-sound default; the retired NOTE id, 0.2.24's EQ switch, is free again: 0.2.25 layout round); SIDECHAIN is the device's MIDI setup (global, no parameter ids); SEND FX is the part's (not global)");
}
// ---- connect, read, live edit with WATCH
{
  const { m, u } = await connect();
  ok(u.writable && u.parts.length === 2 && u.parts[0].eng === 0 && u.parts[1].eng === E("FM6") && u.g.length === 27 && u.info.menuCount === 33, "connect: INFO checked against the firmware tables, both parts and the globals read");
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
  ok(typeof u.writeParamLinked === "undefined", "LINK (T0229, owner): removed: the unit has no linked writes any more");
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
  ok(u.info.patchCap === 128 && u.info.caps === 63, "patches: the unit reports 128 slots (INFO tag 58 01) and caps 63");
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
  const used = await u.multiUsed(); ok(used.length === 16 && used.every(Boolean), "multis: used flags of M01..M16 (PROJECT query; 0.2.23: all sixteen hold a factory multi)");
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
  const items = await u.readMenu(); ok(items.length === 33 && items.some((i) => i.name === "COLOR" && i.names.length === L.PALETTES.LIST.length && i.names.join() === L.PALETTES.LIST.map((p) => p.name).join()) && items.find((i) => i.name === "TUNE").unit === "Hz" && items.find((i) => i.name === "TUNE").kind === 2 && items.find((i) => i.name === "TUNE").min === 4153 && items.find((i) => i.name === "TUNE").max === 4662 && items.find((i) => i.name === "TUNE").value === 4400, "menu: 33 items (0.2.25: no USB LEVEL) with names, ranges, units and tabs as the unit describes them");
  {   // DUELL 0.2.22: TUNE (id 22) is a number in tenths (kind 2): A4 in 0.1 Hz, v14 as it is (no coarse / fine pair); the page shows it with one decimal
    const t = items.find((i) => i.id === 22);
    ok(t.kind === 2 && t.value === 4400 && t.tabName === "AUDIO" && await u.setMenu(22, 4320) === 4320 && await u.setMenu(22, 8000) === 4662 && await u.setMenu(22, 0) === 4153 && await u.setMenu(22, 4400) === 4400 && (4320 / 10).toFixed(1) === "432.0",
       "menu: TUNE is kind 2 (tenths of Hz, 4153..4662, default 4400), set and clamped as a plain v14 number");
    const g3 = L.GP[3], f = L.FMTS[g3.fk];
    ok(g3.label === "TUNE" && g3.min === 4153 && g3.max === 4662 && g3.def === 4400 && f[4400 - 4153][0] === "440.0" && f[4400 - 4153][1] === "Hz" && f[0][0] === "415.3" && f[509][0] === "466.2", "layout: the global TUNE shows 440.0 Hz (415.3 .. 466.2)");
  }
  {   // DUELL 0.2.14: GLO > MODULES (ids 28..31, tab 4 "MODULES") and PERF LEAVE (id 32, CONTROL) are offered like any MENU setting
    const mods = items.filter((i) => i.id >= 28 && i.id <= 31), pl = items.find((i) => i.id === 32);
    ok(mods.length === 4 && mods.map((i) => i.name).join() === "ARP,SEQ,SENDS,PART FX" && mods.every((i) => i.tabName === "MODULES" && i.tab === 4 && i.names.join() === "OFF,ON" && i.value === 1)
       && pl && pl.name === "PERF LEAVE" && pl.tabName === "CONTROL" && pl.names.join() === "FADE,KEEP", "menu: the MODULES switches (28..31, tab MODULES, ON) and PERF LEAVE (32, CONTROL, FADE / KEEP)");
    const bmn = items.find((i) => i.id === 33);
    ok(bmn && bmn.name === "BASS MONO" && bmn.tabName === "MASTER" && bmn.value === 0 && bmn.names.length === 12 && bmn.names[0] === "OFF" && bmn.names[5] === "100 Hz" && bmn.names[11] === "400 Hz" && await u.setMenu(33, 5) === 5 && await u.setMenu(33, 99) === 11 && await u.setMenu(33, 0) === 0, "menu: BASS MONO (33, tab MASTER, OFF + 40 .. 400 Hz) is offered and set");
    const sc = [38, 39, 40, 41].map((id) => items.find((i) => i.id === id));   // 0.2.22 SIDECHAIN: P1 CH, P1 NOTE, P2 CH, P2 NOTE (numbers; tab SIDECHAIN; the device's, whatever the DUCK SRC is)
    ok(sc.every((i) => i && i.tabName === "SIDECHAIN" && i.kind === 1) && sc.map((i) => i.name).join() === "P1 CH,P1 NOTE,P2 CH,P2 NOTE" && sc.map((i) => i.value).join() === "10,36,10,36" && sc.map((i) => i.min + "-" + i.max).join() === "1-16,0-127,1-16,0-127"
       && await u.setMenu(38, 5) === 5 && await u.setMenu(39, 0) === 0 && await u.setMenu(40, 99) === 16 && await u.setMenu(41, 127) === 127 && await u.setMenu(38, 10) === 10 && await u.setMenu(39, 36) === 36 && await u.setMenu(40, 10) === 10 && await u.setMenu(41, 36) === 36
       && L.SIDECH.length === 2 && L.SIDECH[0].min === 1 && L.SIDECH[0].max === 16 && L.SIDECH[1].max === 127 && valueText(L.SIDECH[1], 0).val === "ANY" && valueText(L.SIDECH[1], 36).val === "C2" && uncoveredPages().length === 0,
       "menu: the SIDECHAIN setup (38..41, tab SIDECHAIN): channels 1..16, notes ANY .. 127 (36 = C2), set and clamped as the unit does; the page SIDECHAIN is in the GLOBAL tab, no page is uncovered");
    const cmp = [34, 35, 36, 37].map((id) => items.find((i) => i.id === id));   // 0.2.21 COMP: THRESH (a number in dB), RATIO, ATTACK, RELEASE (tab MASTER)
    ok(cmp.every((i) => i && i.tabName === "MASTER") && cmp.map((i) => i.name).join() === "THRESH,RATIO,ATTACK,RELEASE" && cmp[0].kind === 1 && cmp[0].unit === "dB" && cmp[0].min === -40 && cmp[0].max === 0 && cmp[0].value === -17
       && cmp[1].value === 0 && cmp[1].names.join() === "OFF,1.5:1,2:1,3:1,4:1,6:1,8:1,12:1,INF" && cmp[2].kind === 1 && cmp[2].max === 120 && cmp[2].value === 80   // (0.2.24 firmware, editor_menu.c: ATTACK and RELEASE are numbers, the index;
       && cmp[3].kind === 1 && cmp[3].max === 80 && cmp[3].value === 40                                                                                       //  the mock had the 0.2.21 lists until 0.2.25)
       && await u.setMenu(35, 4) === 4 && await u.setMenu(34, -9) === -9 && await u.setMenu(36, 199) === 120 && await u.setMenu(37, 0) === 0 && await u.setMenu(34, -99) === -40 && await u.setMenu(35, 0) === 0, "menu: COMP (34..37, tab MASTER: THRESH -40 .. 0 dB, RATIO, ATTACK, RELEASE) is offered and set, clamped by the unit");
    const ms = [42].map((id) => items.find((i) => i.id === id));   // 0.2.23 WIDTH (a number; tab MASTER; a device setting); 0.2.25: id 43, DRIVE, is removed
    ok(ms.every((i) => i && i.tabName === "MASTER" && i.kind === 1 && i.min === 0) && ms.map((i) => i.name).join() === "WIDTH" && ms[0].unit === "%" && ms[0].max === 200 && ms[0].value === 100 && !items.some((i) => i.id === 43 || i.name === "DRIVE")
       && await u.setMenu(42, 150) === 150 && await u.setMenu(42, 999) === 200 && await u.setMenu(42, 100) === 100, "menu: WIDTH (42, 0 .. 200 %), tab MASTER, offered and set, clamped by the unit; no DRIVE (43): the saturator is removed");
    ok(await u.setMenu(30, 0) === 0 && m.menu.find((x) => x[0] === 30)[6] === 0 && await u.setMenu(32, 1) === 1, "menu: SENDS OFF and PERF LEAVE KEEP are set");
    const rv = L.PAGES.find((p) => p.title === "REVERB"), lc = L.GP[rv.ids[3]];
    ok(rv.ids.length === 4 && lc.label === "LOCUT" && lc.def === 75 && u.g.length === L.G_COUNT, "layout: REVERB K4 is LOCUT (default 75 = 299 Hz), so the BUSES tab's REVERB card carries it");
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

// ---- 0.2.24: an engine change keeps the common settings (the mock as the unit's set_engine_of), the EQ switch, the packed matrix, the bipolar sliders
{
  const { m, u } = await connect();
  const q = m.parts[0], before = q.p.slice();
  for (let i = 0; i < P_E0; i++) { const d = L.TP[i]; if (d.max > d.min && i !== ID.P_MUTE) q.p[i] = d.def < d.max ? d.def + 1 : d.def - 1; }   // a custom patch: every common value off its default
  q.p[ID.P_M1SRC] = 1; q.p[ID.P_M1SRC + 1] = 5; q.p[ID.P_M1SRC + 2] = 20;
  const custom = q.p.slice(0, P_E0);
  await u.readPart(0);
  const phase = L.ENG.findIndex((e) => e.name === "PHASE"), trio = L.ENG.findIndex((e) => e.name === "TRIO");
  await u.setEngine(0, phase);
  ok(m.parts[0].eng === phase && u.parts[0].eng === phase && m.parts[0].p.slice(0, P_E0).join() === custom.join() && u.parts[0].p.slice(0, P_E0).join() === custom.join(),
     "engine change (mock, as the unit since 0.2.24): ANALOG -> PHASE keeps every common value (envelopes, filter, LFOs, matrix, FX, arp, mixer); the editor reads the same");
  ok(m.parts[0].p.slice(P_E0).every((v, k) => v >= L.ENG[phase].edit[k].min && v <= L.ENG[phase].edit[k].max) && m.parts[0].p[P_E0 + 4] === before[P_E0 + 1],
     "  .. the engine's own eight: PHASE's defaults, DTN carried over from ANALOG (the same control)");
  await u.setEngine(0, 0); m.parts[0].p[P_E0 + 1] = 33; await u.readPart(0);   // ANALOG's DTN is slot 1 (its own filter is gone, cloud job J); TRIO's DTN is slot 3
  await u.setEngine(0, trio);
  ok(m.parts[0].p[P_E0 + 3] === 33 && m.parts[0].p.slice(0, P_E0).join() === custom.join(), "  .. ANALOG -> TRIO: DTN carries over, the common values still as set");
  ok(ID.P_EQON === undefined && L.TP[ID.P_CUT2].label === "CUTOFF" && L.TP[ID.P_CUT2].max === 82 && inertReason({ eng: 0, p: Object.assign([], custom, { [ID.P_CUT2]: 1 }) }, ID.P_EQLG) === null,
     "EQ (0.2.25 layout round): no ON / OFF switch (its id 106 is free again); a band card is never hidden for it");
  ok(pageByTitle("EQ 1/2").ids.join() === [ID.P_EQLF, ID.P_EQLG, ID.P_EQMF, ID.P_EQMG].join() && pageByTitle("EQ 2/2").ids.join() === [ID.P_EQMQ, ID.P_EQHF, ID.P_EQHG, -1].join() && pageByTitle("EQ 1/2").graph === "EQ",
     "  .. EQ 1/2: LOW FREQUENCY, LOW GAIN, MID FREQUENCY, MID GAIN; EQ 2/2: MID Q, HIGH FREQUENCY, HIGH GAIN (the editor's card follows: it is built from these pages)");
  ok(!matrixSecondShown({ eng: 0, p: Object.assign([], custom, { [ID.P_M4SRC]: 0 }) }) && matrixSecondShown({ eng: 0, p: Object.assign([], custom, { [ID.P_M4SRC]: 2 }) }),
     "matrix (0.2.24, packed): MATRIX 5-8 is shown only while slot 4 is used");
  ok(isBipolar(L.TP[ID.P_PAN]) && bipolarZero(L.TP[ID.P_PAN]) === 0 && isBipolar(L.TP[ID.P_E2FLT]) && isBipolar(L.TP[ID.P_KTRK]) && !isBipolar(L.MSTG[0]) && bipolarZero(L.MSTG[0]) === 0
     && !isBipolar(L.TP[ID.P_LEVEL]) && !isBipolar(L.TP[ID.P_ATK]) && !isBipolar(L.TP[ID.P_MUTE]) && !isBipolar(L.TP[ID.P_SUS]) && !isBipolar(L.GP[ID.G_BPM]),
     "bipolar sliders (0.2.24, 0.2.25): PAN, the depths and KEY TRK (a negative end) fill from the middle; WIDTH (0 .. 200, default 100), LEVEL, ATK, SUS, TEMPO, a switch fill from the left");
}

// ---- the new layout: tabs per part, the global column, the theme from the unit's palette
{
  const keys = PART_TABS.map((t) => t.key).join(), gk = GLOBAL_TABS.map((t) => t.key).join();
  ok(keys === "engine,filter,envelope,lfo,matrix,fx,arpseq,roll" && PART_TABS.map((t) => t.label).join() === "ENGINE,FILTER,ENVELOPE,LFO,MATRIX,FX,ARP,SEQ", `layout: each part column has the tabs ENGINE FILTER ENVELOPE LFO MATRIX FX ARP SEQ (${keys})`);
  ok(gk === "global,patches,multis,patterns,midi,tables,settings", `layout: the global column has the tabs GLOBAL PATCHES MULTIS PATTERNS MIDI WAVETABLES SETTINGS (${gk})`);
  const part = new Set(PART_TABS.flatMap((t) => t.pages)), glob = new Set(GLOBAL_TABS.flatMap((t) => t.pages));
  ok(["MULTI FILTER 1/2", "MULTI FILTER 2/2", "FX", "INSERT 1/2", "DUCK 2/2", "ARP 1/3", "STEP", "LFO 2 DEST", "MATRIX 5-8"].every((t) => part.has(t)) && ["CHORUS", "DELAY 1/2", "DELAY 2/2", "REVERB", "TEMPO", "MODULES"].every((t) => glob.has(t)), "layout: filter, FX, arp and step pages are in the part tabs, buses and tempo in the global column");
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
// ---- 0.2.25 layout round, T0229: no pictures in the editor (curves.js is gone); the MIXER / PATCH blocks' words, the names
{
  const { existsSync } = await import("node:fs");
  ok(!existsSync(new URL("./editor/curves.js", import.meta.url)), "pictures (T0229, owner): the EQ, COMPRESSOR and STEREO MASTER pictures are gone from the editor (curves.js deleted)");
  const { PART, GLOBAL, MASTER, engineLong } = await import("./editor/labels.js");
  ok(PART[0][0] === "LEVEL" && PART[39][0] === "PAN" && PART[40][0] === "MUTE" && PART[109][0] === "PATCH LEVEL" && PART[22][0] === "CHANCE" && GLOBAL[17][0] === "DRIFT" && MASTER.WIDTH[0] === "WIDTH",
     "names: the MIXER block's LEVEL, PAN, MUTE; PATCH LEVEL; CHANCE (the arp); DRIFT (the shimmer); STEREO MASTER's WIDTH");
  ok(PART[33][0] === "DRIVE" && [34, 35, 36].map((i) => PART[i][0]).join() === "CHORUS,DELAY,REVERB" && [15, 4, 7].map((i) => GLOBAL[i][0]).join() === "MODE,TIME,MIX" && [111, 107, 108, 110].map((i) => PART[i][0]).join() === "SOURCE,DEPTH,ATTACK,RELEASE" && PART[90][0] === "MID Q",
     "names: no word twice on a card (SATURATION: DRIVE; SENDS: CHORUS DELAY REVERB; DELAY: MODE TIME MIX; DUCKING: SOURCE DEPTH ATTACK RELEASE); MID Q");
  ok(engineLong("ANALOG", "SUB") === "SUB" && engineLong("PHASE", "WAVE") === "WAVE 1" && engineLong("PHASE", "WAVE2") === "WAVE 2" && engineLong("PHASE", "DCW") === "SHAPING" && engineLong("PHASE", "ENV") === "SHAPING ENV" && engineLong("ANALOG", "WAVE") === "WAVEFORM",
     "names: SUB (not SUB OSCILLATOR); PHASE: WAVE 1, WAVE 2 (owner: as the unit), SHAPING, SHAPING ENV");
  const { longDesc } = await import("./editor/labels.js");
  const g = []; g[ID.G_DMODE] = 0; const dDigi = longDesc(gdescOf(g, ID.G_DMDEP), "g13"); g[ID.G_DMODE] = 2; const dRand = longDesc(gdescOf(g, ID.G_DMDEP), "g13");
  ok(dDigi.label === "MOD DEPTH" && dRand.label === "CHANCE", "names: the delay's depth card is MOD DEPTH in DIGITAL / PING-PONG and CHANCE in RANDOM / OCT UP, never both in one label");
}
// ---- cloud job I: protocol scope 2 (the extension slots), the TWIN FILTER's descriptors and hide rules, the FILTER tab's names
{
  const { X, NX, XW_BASE, xdescOf, twDesc, twinWhy, twinX, twinTypeOffered, twinFlow, twinComb, dstOffered } = await import("./editor/layout.js");
  const { CMD, v14, rv14, parseInfo, xReadArgs, xWriteArgs, parseXReply } = await import("./editor/proto.js");
  const { XPART, TWIN_LONG, VALUE_NAMES, MATRIX_GROUPS, longValue, PART, longDesc } = await import("./editor/labels.js");
  const names = Object.keys(X);
  ok(NX === 16 && XW_BASE === 128 && L.XTP.length === NX && names.length === 15 && names.every((n, k) => X[n] === k) && L.XTP[NX - 1].label === "-", "scope 2 layout: NX 16 slots (15 used, X15 the spare), the slot numbers by name, wide ids from 128");
  {   // each slot's descriptor, as params.c XTP says (label, format, range, default, names)
    const want = [["ROUTE", 0, 1, 0, "SERIES,PARAL"], ["LINK", 0, 1, 1, "OFF,ON"], ["KTRK2", -63, 63, 0], ["ENV2", -64, 63, 0], ["LFO1", -64, 63, 0], ["LFO2", -64, 63, 0], ["LEVEL", 0, 127, 0], ["OCTAVE", 0, 2, 1, "-2,-1,0"],
      ["WAVE", 0, 3, 0, "SINE,TRI,SQUARE,PULSE"], ["HARM", 0, 127, 0], ["MODE", 0, 1, 0, "LOOP,ONCE"], ["RISE", 0, 127, 10], ["FALL", 0, 127, 70], ["CURVE", -64, 63, 0], ["SYNC", 0, 1, 0, "OFF,ON"]];
    ok(want.every(([l, mn, mx, df, nm], k) => { const d = L.XTP[k]; return d.label === l && d.min === mn && d.max === mx && d.def === df && (nm == null ? !d.names : d.names?.join() === nm); }), "scope 2 layout: the 15 slots' labels, ranges, defaults and value names are the cloud plan's (ROUTE LINK KTRK2 ENV2 LFO1 LFO2 | LEVEL OCTAVE WAVE HARM | MODE RISE FALL CURVE SYNC)");
    ok(L.XSYNC.length === 2 && L.XSYNC[0].label === "RISE" && L.XSYNC[1].label === "FALL" && L.XSYNC.every((d) => d.min === 0 && d.max === 127 && d.fk >= 0 && valueText(d, 0).val !== valueText(L.XTP[X.X_CYRISE], 0).val),
       "scope 2 layout: CYCLE RISE and FALL with SYNC on have the note-value descriptors (XSYNC)");
    const p = defaultPart(0); p.x[X.X_CYSYNC] = 1;
    ok(xdescOf(p, X.X_CYRISE) === L.XSYNC[0] && xdescOf(p, X.X_CYFALL) === L.XSYNC[1] && xdescOf(p, X.X_SUBLVL) === L.XTP[X.X_SUBLVL] && xdescOf(defaultPart(0), X.X_CYRISE) === L.XTP[X.X_CYRISE], "scope 2 layout: xdescOf follows CYCLE SYNC for RISE and FALL only");
  }
  {   // the TWIN FILTER's one-value-two-meanings
    const p = defaultPart(0);
    const lab = (id) => twDesc(p, id).label;
    p.p[ID.P_TWTYPE] = 1; p.x[X.X_LINK] = 1;
    const a = lab(ID.P_CUT2) + "," + lab(ID.P_RES2);
    p.x[X.X_LINK] = 0; const b = lab(ID.P_CUT2) + "," + lab(ID.P_RES2);
    p.p[ID.P_TWTYPE] = L.TW.COMB; p.x[X.X_LINK] = 1; const c = lab(ID.P_CUT2) + "," + lab(ID.P_RES2);
    ok(a === "OFFSET,RES" && b === "CUTOFF,RES" && c === "TUNE,FDBK" && L.TW.COMB === 6 && twinComb(p), `TWIN labels: CUTOFF 2 is OFFSET while LINK is on, CUTOFF with LINK off, TUNE on COMB; RES 2 is RES, FDBK on COMB (${a} | ${b} | ${c})`);
    ok(descOf(p, ID.P_CUT2) === L.TW.TUNE && descOf(p, ID.P_RES2) === L.TW.FDBK && L.TW.OFFSET.min === -36 && L.TW.OFFSET.max === 36 && L.TW.CUTOFF.max === 82 && L.TW.TUNE.min === -24 && L.TW.TUNE.max === 24 && L.TW.FDBK.min === -64 && L.TW.RES.min === 0,
       "TWIN ranges: OFFSET -36..36, CUTOFF 0..82, TUNE -24..24, RES 0..127, FDBK -64..63 (descOf follows)");
    const lg = (q, id) => longDesc(descOf(q, id), id).label;
    const q = defaultPart(0); q.p[ID.P_TWTYPE] = 1;
    const e1 = lg(q, ID.P_CUT2) + "," + lg(q, ID.P_RES2); q.x[X.X_LINK] = 0; const e2 = lg(q, ID.P_CUT2); q.p[ID.P_TWTYPE] = 6; const e3 = lg(q, ID.P_CUT2) + "," + lg(q, ID.P_RES2);
    ok(e1 === "OFFSET,RESONANCE" && e2 === "CUTOFF" && e3 === "TUNE,FEEDBACK", `TWIN names (R2): the editor writes RESONANCE where the unit has RES and FEEDBACK where it has FDBK (${e1} | ${e2} | ${e3})`);
  }
  {   // the hide rules (FILTER_PAGES_PROPOSAL 5.2)
    const q = defaultPart(0), r = (t, link = 1) => { q.p[ID.P_TWTYPE] = t; q.x[X.X_LINK] = link; return [ID.P_TWTYPE, twinX(X.X_LINK), ID.P_CUT2, ID.P_RES2, twinX(X.X_ENV2), twinX(X.X_KTRK2), twinX(X.X_LFO1), twinX(X.X_LFO2), twinX(X.X_ROUTE), ID.P_DRV2, ID.P_BLEND].map((ref) => (twinWhy(q, ref) ? "-" : "x")).join(""); };
    ok(r(0) === "x----------" && r(0, 0) === "x----------", `hide rule: TYPE OFF shows TYPE alone (TWIN MOD and TWIN CHAIN have nothing left: ${r(0)})`);
    ok(r(1) === "xxxxxxxxxxx" && r(2) === "xxxxxxxxxxx" && r(5) === "xxxxxxxxxxx" && r(1, 0) === "xxxxxxxxxxx", "hide rule: LP .. LADDER show everything; LINK changes the label and hides nothing");
    ok(r(6) === "x-xxx-xxxxx" && r(6, 0) === "x-xxx-xxxxx", `hide rule: COMB hides LINK and KEY TRACK, nothing else (${r(6)})`);
    q.p[ID.P_TWTYPE] = 6; ok(twinWhy(q, twinX(X.X_LINK)) === "TWIN COMB" && twinWhy(q, twinX(X.X_KTRK2)) === "TWIN COMB" && inertReason(q, ID.P_CUT2) === null, "hide rule: the reason of a hidden control is TWIN COMB / TWIN OFF (the card's tooltip)");
    q.p[ID.P_TWTYPE] = 0; ok(inertReason(q, ID.P_BLEND) === "TWIN OFF" && inertReason(q, ID.P_DRV2) === "TWIN OFF" && inertReason(q, ID.P_TWTYPE) === null && voiceWhy(q, ID.P_TWTYPE) === null, "hide rule: layout.js inertReason gives it for CUTOFF 2, RES 2, DRIVE, BLEND; the TYPE is never hidden, not even by the voice mode (id 43 was ALLOCATION)");
    const w = defaultPart(E("WAVE")), ph = defaultPart(E("PHYS")), an = defaultPart(E("ANALOG"));
    ok(!twinTypeOffered(w, 6) && !twinTypeOffered(ph, 6) && twinTypeOffered(an, 6) && [0, 1, 2, 3, 4, 5].every((t) => twinTypeOffered(w, t) && twinTypeOffered(ph, t)), "hide rule: COMB is not in the TYPE list on WAVE and PHYS (all other types are), but on the other eight engines");
    ok(twinFlow({ x: { [X.X_ROUTE]: 0 } }) === "ENGINE > MULTI > DRIVE > TWIN > AMP" && twinFlow({ x: { [X.X_ROUTE]: 1 } }) === "ENGINE > DRIVE > MULTI + TWIN > AMP", "flow line: SERIES ENGINE > MULTI > DRIVE > TWIN > AMP; PARALLEL with the drive before both");
  }
  {   // the labels: no control repeats TWIN or FILTER inside a titled card (naming rule R1); FILTER ENV is the one name with the word (the owner wrote it for CUTOFF MOD and TWIN MOD)
    const labs = [...Object.values(XPART).map((x) => x[0]), ...Object.values(TWIN_LONG), ...[25, 26, 43, 101, 106].map((i) => PART[i][0])];
    ok(labs.every((l) => !/TWIN/.test(l) && (!/FILTER/.test(l) || l === "FILTER ENV")), `names (R1): no label of the second filter's, SUB's or CYCLE's controls repeats TWIN or FILTER (${labs.filter((l) => /TWIN|FILTER/.test(l)).join() || "none"})`);
    ok([25, 26, 43, 101, 106].every((i) => PART[i][1].length > 20) && Object.values(XPART).every((x) => x[1].length > 20) && PART[43][0] === "TYPE" && PART[101][0] === "BLEND" && PART[26][0] === "DRIVE" && PART[25][0] === "RESONANCE" && PART[106][0] === "CUTOFF", "names: ids 25, 26, 43, 101, 106 are RESONANCE, DRIVE, TYPE, BLEND, CUTOFF with tooltips (they were ROOT, SCALE, ALLOCATION, FILTER TYPE, none)");
    const t = longDesc(L.TP[ID.P_TWTYPE], ID.P_TWTYPE);
    ok(t.names.join() === "OFF,LOW-PASS,BAND-PASS,HIGH-PASS,NOTCH,LADDER,COMB" && L.TP[ID.P_TWTYPE].names.join() === "OFF,LP,BP,HP,NOTCH,LADDER,COMB", "names: TYPE lists OFF, LOW-PASS, BAND-PASS, HIGH-PASS, NOTCH, LADDER, COMB (the firmware's LP BP HP spelled out, same order)");
    const rt = longDesc(xdescOf(defaultPart(0), X.X_ROUTE), "x0"), sw = longDesc(xdescOf(defaultPart(0), X.X_SUBWAVE), "x8");
    ok(rt.names.join() === "SERIES,PARALLEL" && sw.names.join() === "SINE,TRIANGLE,SQUARE,PULSE" && longDesc(xdescOf(defaultPart(0), X.X_ENV2), "x3").label === "FILTER ENV" && longDesc(xdescOf(defaultPart(0), X.X_KTRK2), "x2").label === "KEY TRACK", "names: ROUTE SERIES / PARALLEL, SUB WAVE spelled out, FILTER ENV and KEY TRACK on TWIN MOD");
    const dr = longDesc(L.TP[ID.P_DRV2], ID.P_DRV2);
    ok(valueText(dr, 0).val === "OFF" && valueText(dr, 1).val !== "OFF" && valueText(longDesc(L.TP[ID.P_BLEND], ID.P_BLEND), 0).val === "BOTH" && valueText(longDesc(L.TP[ID.P_BLEND], ID.P_BLEND), -64).val === "MULTI" && valueText(longDesc(L.TP[ID.P_BLEND], ID.P_BLEND), 63).val === "TWIN" && valueText(longDesc(L.TP[ID.P_BLEND], ID.P_BLEND), 32).val === "TWIN 50%" && valueText(longDesc(L.TP[ID.P_BLEND], ID.P_BLEND), -32).val === "MULTI 50%" && isBipolar(L.TP[ID.P_BLEND]) && !isBipolar(L.TP[ID.P_DRV2]), "controls: DRIVE shows OFF at 0; BLEND is a bipolar slider (the centre is marked)");
    const bip = [L.TW.OFFSET, L.TW.TUNE, L.TW.FDBK, L.XTP[X.X_KTRK2], L.XTP[X.X_ENV2], L.XTP[X.X_LFO1], L.XTP[X.X_LFO2], L.XTP[X.X_CYCURVE], L.TP[ID.P_BLEND]], plain = [L.TW.CUTOFF, L.TW.RES, L.XTP[X.X_SUBLVL], L.XTP[X.X_SUBHARM], L.XTP[X.X_CYRISE], L.TP[ID.P_DRV2]];
    ok(bip.every(isBipolar) && plain.every((d) => !isBipolar(d)), "controls: the truly bipolar ones (OFFSET TUNE FEEDBACK KEY TRACK FILTER ENV LFO 1 LFO 2 CURVE BLEND) are marked at the centre, the one-way ones are not");
    const st = longDesc(L.ENG[E("WAVE")].edit[6], "e:WAVE");
    ok(!isEnum(st) && st.max === 18 && valueText(st, 0).val === "OFF" && valueText(st, 1).val === "2" && valueText(st, 18).val === "19" && L.ENG[E("WAVE")].edit[6].names.length === 19, "WAVE STEPS: a slider of 19 places (OFF, 2 to 19) with its words from the firmware's list");
  }
  {   // the matrix: the new destinations and the source CYCLE by the firmware's name tables
    const dn = L.TP[ID.P_M1SRC + 1].names, sn = L.TP[ID.P_M1SRC].names;
    ok(L.MD.CUT2 === 51 && L.MD.RES2 === 52 && L.MD.DRIVE === 53 && L.MD.BLEND === 54 && L.MD.SUBLVL === 55 && L.MD.N === 56 && L.MD.S_CYCLE === 15 && dn.slice(51).join() === "CUT2,FRES2,FDRV,FBLND,SUBLV" && sn[15] === "CYCLE",
       "matrix: destinations 51..55 CUT2 FRES2 FDRV FBLND SUBLV and the source 15 CYCLE are the firmware's names");
    ok(longValue(ID.P_M1SRC + 1, "CUT2") === "TWIN FILTER CUTOFF" && longValue(ID.P_M1SRC + 1, "FRES2") === "TWIN FILTER RESONANCE" && longValue(ID.P_M1SRC + 1, "FDRV") === "FILTER DRIVE" && longValue(ID.P_M1SRC + 1, "FBLND") === "FILTER BLEND" && longValue(ID.P_M1SRC + 1, "SUBLV") === "SUB LEVEL" && longValue(ID.P_M1SRC, "CYCLE") === "CYCLE",
       "matrix names: TWIN FILTER CUTOFF, TWIN FILTER RESONANCE, FILTER DRIVE, FILTER BLEND, SUB LEVEL; the source CYCLE");
    const flat = (g) => g.flatMap(([, v]) => v);
    ok([51, 52, 53, 54, 55].every((v) => flat(MATRIX_GROUPS.dst).includes(v)) && flat(MATRIX_GROUPS.src).includes(15) && new Set(flat(MATRIX_GROUPS.dst)).size === flat(MATRIX_GROUPS.dst).length, "matrix groups: the new destinations are in the FILTER and ENGINE groups, CYCLE with the envelopes, no value twice");
    const off = defaultPart(0), on = defaultPart(0); on.p[ID.P_TWTYPE] = 1;
    ok([51, 52, 53, 54].every((v) => dstWhy(off, v) === "TWIN OFF" && !dstOffered(off, v) && dstWhy(on, v) === null && dstOffered(on, v)) && dstWhy(off, 55) === null && dstOffered(off, 55), "matrix: the TWIN destinations are offered while TYPE is not OFF (the unit's mod_dst_why); SUB LEVEL always");
  }
  {   // pages that name an extension slot: a page id of 128 + k is slot k, scope 2
    const pg = { ids: [XW_BASE + X.X_LINK, ID.P_CUT2, -1, XW_BASE + 3], scope: "TRACK" };
    const cs = pageControls(pg, defaultPart(0));
    ok(JSON.stringify(cs) === JSON.stringify([{ id: 1, scope: 2 }, { id: ID.P_CUT2, scope: 0 }, { id: 3, scope: 2 }]), "pages: a page id of 128 + k refers to extension slot k by scope 2 and id k");
  }
  // ---- the protocol, against the simulated unit
  {
    const { m, u } = await connect();
    ok(parseInfo(await u.req(CMD.INFO)).nx === 16 && u.hasX && u.info.nx === 16, "scope 2: INFO has the tag 59 01 NX (16)");
    ok(u.parts.every((q) => q.x.length === NX && q.x.every((v, k) => v === L.XTP[k].def)), "scope 2: both parts' slots are read at connect: all at their defaults");
    let a = await u.req(CMD.GET, [2, X.X_LINK]); ok(a.length === 4 && a[0] === 2 && a[1] === 1 && rv14(a, 2) === 1, "scope 2: GET");
    a = await u.req(CMD.SET, [2, X.X_CYRISE, ...v14(55)]); ok(rv14(a, 2) === 55 && m.parts[0].x[X.X_CYRISE] === 55, "scope 2: SET stores it and echoes the value");
    a = await u.req(CMD.SET, [2, X.X_SUBLVL, ...v14(500)]); ok(rv14(a, 2) === 127 && m.parts[0].x[X.X_SUBLVL] === 127, "scope 2: SET clamps to the descriptor (500 -> 127)");
    a = await u.req(CMD.SET, [2, X.X_KTRK2, ...v14(-500)]); ok(rv14(a, 2) === -63, "scope 2: SET clamps below (-500 -> -63)");
    a = await u.req(CMD.SET, [2, NX - 1, ...v14(9)]); ok(rv14(a, 2) === 0, "scope 2: the spare slot X15 keeps its value");
    a = await u.req(CMD.DESC, [2, X.X_SUBOCT]);
    ok(a[0] === 2 && a[1] === X.X_SUBOCT && a[2] === L.XTP[X.X_SUBOCT].fmt && rv14(a, 3) === 0 && rv14(a, 5) === 2 && rv14(a, 7) === 1, "scope 2: DESC gives fmt, min, max, def");
    m.parts[0].x[X.X_CYSYNC] = 1; a = await u.req(CMD.DESC, [2, X.X_CYRISE]); ok(a.length > 12 && a[2] === L.XSYNC[0].fmt, "scope 2: DESC of CYCLE RISE while SYNC is on is the note-value descriptor"); m.parts[0].x[X.X_CYSYNC] = 0;
    ok(await rejects(() => u.req(CMD.GET, [2, NX]), /unexpected command/) && await rejects(() => u.req(CMD.SET, [2, 99, ...v14(1)]), /unexpected command/) && await rejects(() => u.req(CMD.DESC, [3, 0]), /unexpected command/), "scope 2: an id past NX-1 and a scope past 2 get no reply");
    a = await u.req(CMD.TRACK_PARAM, xReadArgs(1, X.X_ENV2)); ok(parseXReply(a).part === 1 && parseXReply(a).k === X.X_ENV2 && parseXReply(a).scope === 2, "scope 2: TRACK_PARAM with a trailing scope byte reads a slot of any part (part 2)");
    a = await u.req(CMD.TRACK_PARAM, xWriteArgs(1, X.X_ENV2, 20)); ok(parseXReply(a).v === 20 && m.parts[1].x[X.X_ENV2] === 20 && m.parts[0].x[X.X_ENV2] === 0, "scope 2: TRACK_PARAM writes the slot of that part only");
    a = await u.req(CMD.TRACK_PARAM, [0, ID.P_PAN]); ok(a.length === 4, "scope 2: TRACK_PARAM without a scope byte is as before");
    ok(await rejects(() => u.req(CMD.TRACK_PARAM, [0, 1, 1]), /unexpected command/) && await rejects(() => u.req(CMD.TRACK_PARAM, [0, NX, 2]), /unexpected command/), "scope 2: TRACK_PARAM with a bad scope byte or slot gets no reply");
    // the editor's side: writes, pushes, undo
    u.writeX(0, X.X_CYFALL, 33); u.writeX(1, X.X_SUBLVL, 44); await u.idle();
    ok(m.parts[0].x[X.X_CYFALL] === 33 && m.parts[1].x[X.X_SUBLVL] === 44 && u.parts[0].x[X.X_CYFALL] === 33, "scope 2: Unit.writeX goes out through TRACK_PARAM for either part");
    u.writeX(0, X.X_SUBLVL, 9999); await u.idle(); ok(u.parts[0].x[X.X_SUBLVL] === 127 && m.parts[0].x[X.X_SUBLVL] === 127, "scope 2: the editor clamps first (9999 -> 127)");
    let ev = null; u.on("param", (d) => { if (d.scope === 2) ev = d; });
    m.watching = true; m.deviceTurnX(0, X.X_CYCURVE, -20);
    ok(u.parts[0].x[X.X_CYCURVE] === -20 && ev && ev.id === X.X_CYCURVE && ev.part === 0 && !ev.byUs, "scope 2: a CHANGED push of the selected part updates the slot (the unit's knob follows)");
    m.deviceTurnX(1, X.X_CYCURVE, 5); ok(u.parts[1].x[X.X_CYCURVE] !== 5, "scope 2: a slot of the part that is not selected is not pushed");
    await u.readPart(1); ok(u.parts[1].x[X.X_CYCURVE] === 5, "scope 2: a part is read again with its slots (readPart)");
    // LINK converts CUTOFF 2
    m.parts[0].p[ID.P_TWTYPE] = 1; m.parts[0].p[ID.P_FREEA] = 30; m.parts[0].p[ID.P_CUT2] = 5; m.parts[0].x[X.X_LINK] = 1; await u.readPart(0);
    u.writeX(0, X.X_LINK, 0); await u.idle();
    ok(m.parts[0].x[X.X_LINK] === 0 && m.parts[0].p[ID.P_CUT2] === 34 && u.parts[0].p[ID.P_CUT2] === 34, "scope 2: LINK off converts CUTOFF 2 from OFFSET +5 semitones to CUTOFF 34 (filter 1's knob 30, 3.8 steps rounded), and the editor reads it back");
    u.writeX(0, X.X_LINK, 1); await u.idle(); ok(u.parts[0].p[ID.P_CUT2] === 5 && m.parts[0].p[ID.P_CUT2] === 5, "scope 2: LINK on converts it back");
    // TYPE
    u.writeParam(0, ID.P_TWTYPE, 6); await u.idle();
    ok(m.parts[0].p[ID.P_TWTYPE] === 6 && u.parts[0].p[ID.P_CUT2] === 0 && u.parts[0].p[ID.P_RES2] === 0, "TWIN TYPE: entering COMB gives CUTOFF 2 (TUNE) and RES 2 (FEEDBACK) their defaults, read back by the editor");
    await u.setEngine(0, E("WAVE")); u.parts[0].p[ID.P_TWTYPE] = 1; m.parts[0].p[ID.P_TWTYPE] = 1; u.writeParam(0, ID.P_TWTYPE, 6); await u.idle();
    ok(m.parts[0].p[ID.P_TWTYPE] === 1 && u.parts[0].p[ID.P_TWTYPE] === 1, "TWIN TYPE: COMB on WAVE is refused by the unit and the editor shows the unit's LP");
    // PRESET / loads reset the slots; a patch keeps them
    await u.setEngine(0, 0); m.parts[0].x[X.X_SUBLVL] = 77; m.parts[0].x[X.X_CYMODE] = 1; await u.saveSlot(0, 20, "SUBBY");
    m.parts[0].x[X.X_SUBLVL] = 3; await u.loadSlot(0, 20); ok(u.parts[0].x[X.X_SUBLVL] === 77 && u.parts[0].x[X.X_CYMODE] === 1, "scope 2: UP_STORE keeps the slots in the patch, UP_LOAD brings them back and the editor reads them");
    await u.loadFactory(0, 0, 0); ok(u.parts[0].x.every((v, k) => v === L.XTP[k].def), "scope 2: a factory sound load puts every slot to its default");
    // an old unit (no tag 59)
    const o = await connect({ scope2: false });
    ok(!o.u.hasX && o.u.info.nx === null && o.u.parts[0].x.every((v, k) => v === L.XTP[k].def), "scope 2: a unit without the tag: no slot is read, the defaults stand");
    ok(await rejects(async () => o.u.writeX(0, X.X_LINK, 0), /extension values/), "scope 2: Unit.writeX refuses on a unit without scope 2");
  }
}
console.log(fails ? `\nEDITOR WEB TESTS FAILED (${fails})` : "\neditor web tests passed");
process.exit(fails ? 1 : 0);
