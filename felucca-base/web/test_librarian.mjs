// SPDX-License-Identifier: GPL-3.0-only
// The patch librarian (editor/patchrecord.js, patchfile.js, librarian.js, dx7import.js, the wavetable file) against the simulated unit (editor/mockunit.js):
// the stored record built / parsed, PATCH_RAW with the unit's rules, the "duell-patches" file round trips, export / import plans with a safety copy and a verify,
// copy / swap / move, and synthetic DX7 banks (web/test_dx7_synth.mjs, generated in the test) converted to FM6 patches. With DX7_FIXTURE=file it also writes the fixture the firmware
// test tests/dx7_import_test.c reads (host_tests.sh dx7import).
//   node web/test_librarian.mjs      (use the emsdk node: the Homebrew one is broken here)
import { readFileSync, readdirSync, writeFileSync } from "node:fs";
import { synthBank, synthSingle, synthVoice } from "./test_dx7_synth.mjs";
import { L, ID, P_E0 } from "./editor/layout.js";
import { MockUnit, MockLink, defaultPart } from "./editor/mockunit.js";
import { Unit } from "./editor/unit.js";
import { CMD } from "./editor/proto.js";
import { buildRecord, parseRecord, describeRecord, packPatch, unpackPatch, storeDefaults, engineIndex, PTF, parseXBlock, recordProblem, XB_DEFS, XB_MAX, NX, WORST } from "./editor/patchrecord.js";
import { itemFromRecord, itemApprox, buildFile, stringifyFile, parseFile, sniff, toBase64, fromBase64 } from "./editor/patchfile.js";
import * as LIB from "./editor/librarian.js";
import { FM6 } from "./editor/fm6voice.js";
import { parseDx7File, voiceToPatch, cleanVoiceName, guessCategory, neutralValues, SOURCE_LABEL } from "./editor/dx7import.js";
import { WT, ampToByte, tableToFile, parseTableFile, TABLE_FORMAT } from "./editor/wavetable.js";
import { bkCrc } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(118)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const rejects = async (f, re) => { try { await f(); return false; } catch (e) { return re ? re.test(e.message) : true; } };
const FM6E = engineIndex("FM6");
async function connect(opt = {}) { const m = new MockUnit(opt), u = new Unit(new MockLink(m)); await u.connect(); await u.listSlots(); return { m, u }; }
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);

// ---- the stored record: built, parsed, the same form the unit writes
{
  const v = storeDefaults(0); v[ID.P_TRIM] = 99; v[45] = 60; v[46] = 20;
  const rec = buildRecord({ engine: 0, name: "TEST ONE", cat: 3, values: v });
  const r = parseRecord(rec);
  ok(r.ok && r.name === "TEST ONE" && r.engine === 0 && r.cat === 3 && r.slot === 0 && r.rev === 0 && same(r.values, v) && (rec[4] & PTF.CB), "record: built and parsed back (name, engine, category, every value; slot and revision 0)");
  ok(rec[0] === rec.length && rec.length === 20 + 2 * rec[19] && rec[19] === 3, "record: sparse, three pairs for three values off the default, the length field is the length");
  const many = storeDefaults(0).map((d, i) => (i % 3 === 0 && i > 0 && i < 100 ? Math.min(d + 1, 120) : d));
  const rb = buildRecord({ engine: 0, name: "BITMAP", values: many }), pb = parseRecord(rb);
  ok((rb[4] & PTF.BM) && pb.ok && same(pb.values, many), "record: more than 16 values off the default use the bitmap form and come back the same");
  const fm = Array.from({ length: 128 }, (_, i) => (i * 7 + 3) % 128), vv = storeDefaults(FM6E); vv[P_E0 + 7] = 8;
  const rf = buildRecord({ engine: FM6E, name: "FM ONE", values: vv, patch128: fm }), pf = parseRecord(rf);
  ok(pf.ok && (rf[4] & PTF.FM6) && same([...pf.fm6], fm) && pf.values[P_E0 + 7] === 8, "record: an FM6 sound carries its 128 byte patch (packed 8 to 7) and SLOT OWN");
  ok(same([...unpackPatch(packPatch(Uint8Array.from(fm)))], fm) && packPatch(Uint8Array.from(fm)).length === 112, "record: the patch packing (upf_pk_put / upf_pk_get) round trips, 112 bytes");
  const bad = [(b) => b.slice(0, 10), (b) => { const c = b.slice(); c[4] &= ~PTF.CB; return c; }, (b) => { const c = b.slice(); c[5] = 99; return c; }, (b) => { const c = b.slice(); c[7] = 0; return c; },
    (b) => { const c = b.slice(); c[0] = 3; return c; }, (b) => { const c = b.slice(); c[21] = 250; return c; }];
  ok(bad.every((f) => !parseRecord(f(rec)).ok), "record: a short, old-form, unknown-engine, nameless, wrong-length or out-of-range (0.2.19: all 8 flag bits are used) record is refused");
  ok(storeDefault0(), "record: CUT, SLOPE and the two TRIGs and the DUCK CURVE have the store default 0 (upreset.c up_def), the others the editor's");
  ok((rec[4] & PTF.TR) && !parseRecord(rec).pairs.has(0), "record: a record built now has PTF_TR (the loudness is the TRIM) and never stores the mixer level (id 0)");
  { // a record of 0.2.14 .. 0.2.18 (no PTF_TR): its LEVEL (id 0) is read as the trim
    const old = rec.slice(); old[4] &= ~PTF.TR;
    const i = 20 + 2 * [...Array(old[19]).keys()].findIndex((k) => old[20 + 2 * k] === ID.P_TRIM); old[i] = 0;   // the pair (109, 99) becomes (0, 99): the LEVEL of the old form
    const po = parseRecord(old);
    ok(po.ok && po.values[ID.P_TRIM] === 99 && po.values[ID.P_LEVEL] === 104, "record: a record before PTF_TR reads its LEVEL as the trim (up_rec)");
  }
  const d = describeRecord(r);
  ok(d.engine === "ANALOG" && d.category === "PAD" && Object.keys(d.params).length >= 3 && Object.values(d.params).some((x) => /kHz|Hz/.test(x)), "record: the readable dump names what differs and formats it as the unit does");
}
// ---- the extension block (firmware/src/up_xb.h): the bytes are written out by hand from the spec, not by the code under test
{
  const v = storeDefaults(0); v[ID.P_TRIM] = 99;
  const base = buildRecord({ engine: 0, name: "XB BASE", cat: 2, values: v, mb: [1, 0, 0, 0, 0, 0, 0] });   // fixed part, End B (7), three... pairs
  const X0 = XB_DEFS.map((d) => d - 64);
  const withBlock = (blk, rec = base) => { const o = Uint8Array.from([...rec.slice(0, 20), ...blk, ...rec.slice(20)]); o[4] |= PTF.XB; o[0] = o.length & 255; o[1] = o.length >> 8; return o; };
  const noBlock = (rec) => !(rec[4] & PTF.XB) && same(rec, base);
  ok(XB_DEFS.length === NX && XB_MAX === 32 && WORST === 534 && !("SH" in PTF) && PTF.XB === 16 && XB_DEFS[1] === 65 && XB_DEFS[7] === 65 && XB_DEFS[11] === 74 && XB_DEFS[12] === 134 && XB_DEFS.filter((d) => d === 64).length === 12, "xblock: PTF.XB is 16 (the shadow flag is gone), 16 slots, TWIN LINK and SUB OCTAVE default to stored 65, CYCLE RISE and FALL to 74 and 134 (job B's descriptors), WORST is the unit's 534");
  ok(noBlock(buildRecord({ engine: 0, name: "XB BASE", cat: 2, values: v, mb: [1, 0, 0, 0, 0, 0, 0], ext: X0, edits: [] })) && noBlock(buildRecord({ engine: 0, name: "XB BASE", cat: 2, values: v, mb: [1, 0, 0, 0, 0, 0, 0], ext: null })),
     "xblock: every slot at its default and no edit: no block at all, the flag clear (the record is the one without ext)");
  const pr = parseRecord(base);
  ok(pr.ok && same(pr.xv, X0) && pr.edits.length === 0 && pr.mb && pr.mb[0] === 1, "xblock: a record without a block reads the 16 default slots and no edits (and End B)");
  // (1) one slot off its default: SUB LEVEL (X6, wide id 134) = 10 -> [L 5][XV 1][tag 1][n 2][134][74]
  const e1 = X0.slice(); e1[6] = 10;
  const r1 = buildRecord({ engine: 0, name: "XB BASE", cat: 2, values: v, mb: [1, 0, 0, 0, 0, 0, 0], ext: e1 }), h1 = withBlock([5, 1, 1, 2, 134, 74]);
  const p1 = parseRecord(r1);
  ok(same(r1, h1) && r1.length === base.length + 6 && (r1[4] & PTF.XB) && r1[20] === 5 && p1.ok && p1.xv[6] === 10 && p1.xv.filter((x, k) => x !== X0[k]).length === 1 && p1.edits.length === 0 && p1.mb[0] === 1 && p1.values[ID.P_TRIM] === 99,
     "xblock 1: one slot off its default is [05 01 01 02 86 4A] between the fixed part and End B, and reads back");
  const e1b = X0.slice(); e1b[1] = 0;                  // TWIN LINK OFF is the off-default one (its default is ON)
  ok(same(buildRecord({ engine: 0, name: "XB BASE", cat: 2, values: v, mb: [1, 0, 0, 0, 0, 0, 0], ext: e1b }), withBlock([5, 1, 1, 2, 129, 64])) && parseRecord(withBlock([5, 1, 1, 2, 129, 64])).xv[1] === 0,
     "xblock 1: TWIN LINK OFF (default ON) is the pair (129, 64)");
  // (2) 16 slots off their default and 32 edits: L 101, tag 1 n 32, tag 2 n 64, 102 bytes
  const e2 = X0.map((d, k) => d + 1 + k), wids = [...Array(26).keys(), 128, 129, 130, 131, 140, 255].slice(0, 32), ed2 = wids.map((wid, i) => ({ wid, val: i * 3 - 64 }));
  const blk2 = [101, 1, 1, 32, ...e2.flatMap((x, k) => [128 + k, x + 64]), 2, 64, ...ed2.flatMap((e) => [e.wid, e.val + 64])];
  const r2 = buildRecord({ engine: 0, name: "XB BASE", cat: 2, values: v, mb: [1, 0, 0, 0, 0, 0, 0], ext: e2, edits: ed2 }), p2 = parseRecord(r2);
  ok(blk2.length === 102 && same(r2, withBlock(blk2)) && r2.length === base.length + 102 && p2.ok && same(p2.xv, e2) && p2.edits.length === 32 && p2.edits.every((e, i) => e.wid === ed2[i].wid && e.val === ed2[i].val),
     "xblock 2: 16 slots off their default and 32 edits: block length 102 (L 101, tag 1 n 32, tag 2 n 64), slots in order, edits in the given order, read back");
  ok(parseXBlock(r2, 20, r2.length - 20).length === 102 && parseXBlock(r2, 20, r2.length - 20).ok, "xblock 2: parseXBlock gives the block's length (1 + L)");
  const B = (...body) => [body.length, ...body];       // a block from its body (XV and sections): L first
  // (3) an unknown tag 9 between known tags: skipped by its n
  const blk3 = B(1, 1, 2, 128, 65, 9, 3, 7, 7, 7, 2, 2, 5, 70), r3 = withBlock(blk3), p3 = parseRecord(r3);
  ok(blk3[0] === 14 && p3.ok && p3.xv[0] === 1 && p3.edits.length === 1 && p3.edits[0].wid === 5 && p3.edits[0].val === 6 && parseXBlock(r3, 20, r3.length - 20).unknownTags.join() === "9",
     "xblock 3: an unknown tag 9 between tag 1 and tag 2 is skipped by its n; the known sections still read");
  // (4) a block whose L is one too long: refused
  const r4 = withBlock([6, 1, 1, 2, 134, 74]);
  ok(!parseRecord(r4).ok && !parseXBlock(r4.slice(0, 26), 20, 6).ok && !parseXBlock([6, 1, 1, 2, 134, 74], 0, 6).ok && !parseXBlock([5, 1, 1, 2, 134, 74], 0, 5).ok && parseXBlock([5, 1, 1, 2, 134, 74], 0, 6).ok,
     "xblock 4: a block whose L is one too long (past the record, or into the data behind it) is refused");
  // (5) a repeated tag 2 (and a repeated tag 1)
  ok(!parseRecord(withBlock(B(1, 2, 2, 5, 70, 2, 2, 6, 70))).ok && !parseRecord(withBlock(B(1, 1, 2, 128, 65, 1, 2, 129, 64))).ok && parseRecord(withBlock(B(1, 2, 2, 5, 70))).ok,
     "xblock 5: a repeated tag 2, and a repeated tag 1, are refused");
  // (6) an unknown wide id inside tag 1 (200): the record loads, the id is skipped
  const r6 = withBlock(B(1, 1, 4, 200, 100, 129, 66)), p6 = parseRecord(r6);
  ok(p6.ok && p6.xv[1] === 2 && p6.xv.filter((x, k) => x !== X0[k]).length === 1, "xblock 6: an unknown wide id 200 in tag 1 is skipped (the record loads, the other slot is read)");
  const skip = (id) => parseRecord(withBlock(B(1, 1, 2, id, 70)));
  ok([144, 255, 127, 3].every((id) => skip(id).ok && same(skip(id).xv, X0)) && skip(143).xv[15] === 6, "xblock 6: wide ids 144, 255, 127 and 3 in tag 1 are not slots and are skipped (only tag 2 refuses 127); 143 is the spare slot X15");
  // (7) edits containing wide id 127: refused (read and build); wide id 200 and 0..126 are fine in tag 2
  ok(!parseRecord(withBlock(B(1, 2, 2, 127, 70))).ok && await rejects(async () => buildRecord({ engine: 0, name: "X", values: storeDefaults(0), edits: [{ wid: 127, val: 1 }] }), /127/)
     && parseRecord(withBlock(B(1, 2, 2, 200, 70))).edits[0].wid === 200, "xblock 7: an edit of wide id 127 is refused (parse and build); 200 (a slot a newer firmware has) is kept");
  // the other ways a block is damaged
  const dmg = [[[0], "L 0"], [B(2), "XV 2"], [B(0), "XV 0"], [B(1, 1, 3, 128, 65, 1), "odd n in tag 1"], [B(1, 2, 3, 5, 70, 1), "odd n in tag 2"], [B(1, 1, 2, 128, 192), "tag 1 value over 191"],
    [B(1, 2, 2, 5, 192), "tag 2 value over 191"], [B(1, 2, 2, 5, 70, 2, 0), "a second, empty tag 2"], [B(1, 9, 9, 1), "a section runs past the block"], [B(1, 9), "a section header cut off"],
    [B(1, 2, 4, 5, 70, 5, 71), "an id twice in tag 2"], [B(1, 2, 66, ...Array(33).fill(0).flatMap((_, i) => [i, 70])), "33 edits"], [B(1, 3, 1, 1), "(an unknown tag is fine)"]];
  ok(dmg.slice(0, -1).every(([blk]) => !parseRecord(withBlock(blk)).ok) && parseRecord(withBlock(dmg.at(-1)[0])).ok, `xblock: damaged blocks are refused (${dmg.slice(0, -1).map((d) => d[1]).join(", ")}); an unknown tag alone is not damage`);
  ok(parseRecord(withBlock(B(1, 2, 64, ...Array(32).fill(0).flatMap((_, i) => [i, 70])))).edits.length === 32, "xblock: exactly 32 edits are fine");
  ok(parseRecord(withBlock(B(1))).ok && same(parseRecord(withBlock(B(1))).xv, X0), "xblock: a block with no section (only XV) is a block the firmware's xb_block_ok takes");
  { const short = base.slice(0, 20); short[4] = PTF.XB | PTF.CB | PTF.TR; short[19] = 0; short[0] = 20; short[1] = 0;
    ok(!parseRecord(short).ok && !parseRecord(Uint8Array.from([...short, 0])).ok, "xblock: a record with the flag and no room for the block's length byte is refused, not misread"); }
  // build refusals and clamping
  ok(await rejects(async () => buildRecord({ engine: 0, name: "X", values: storeDefaults(0), edits: Array.from({ length: 33 }, (_, i) => ({ wid: i, val: 0 })) }), /32/) && await rejects(async () => buildRecord({ engine: 0, name: "X", values: storeDefaults(0), edits: [{ wid: 3, val: 0 }, { wid: 3, val: 1 }] }), /twice/)
     && await rejects(async () => buildRecord({ engine: 0, name: "X", values: storeDefaults(0), ext: [0] }), /16/), "xblock: building more than 32 edits, an id twice or a short slot list is refused");
  const rc = parseRecord(buildRecord({ engine: 0, name: "CLAMP", values: storeDefaults(0), ext: X0.map((d, k) => (k === 6 ? 500 : d)), edits: [{ wid: 9, val: -500 }, { wid: 131, val: 127 }] }));
  ok(rc.ok && rc.xv[6] === 127 && rc.edits[0].val === -64 && rc.edits[1].val === 127, "xblock: values are held to -64 .. +127 as xb_pack does");
  ok(recordProblem(r2) === null && recordProblem(r1) === null && recordProblem(r4) !== null, "xblock: recordProblem (the unit's pt_rec_ok) takes the good records and refuses the bad one");
  const d2 = describeRecord(p2);
  ok(Object.keys(d2.ext).length === 16 && d2.ext["X6 SUB LEVEL"] === String(e2[6]) && d2.edits === 32 && d2.carries.some((c) => /32 hand-set/.test(c)) && Object.keys(describeRecord(pr).ext).length === 0 && describeRecord(pr).edits === 0,
     "xblock: describeRecord lists the slots off their default by name and the number of edits");
}
function storeDefault0() { const d = storeDefaults(0); return d[45] === 0 && d[48] === 0 && d[84] === 0 && d[92] === 0 && d[105] === 0 && d[ID.P_LEVEL] === 104 && d[ID.P_TRIM] === L.TP[ID.P_TRIM].def; }

// ---- PATCH_RAW against the simulated unit: the unit's rules
{
  const { m, u } = await connect();
  ok(u.info.patchRaw === true && LIB.hasRaw(u), "PATCH_RAW: the unit says so in INFO (tag 5A 01)");
  ok((await LIB.rawRead(u, 7)) === null, "PATCH_RAW: a slot that holds its factory sound has no record");
  const rec0 = await LIB.rawRead(u, 0);
  ok(rec0 && parseRecord(rec0).ok && parseRecord(rec0).name === "WARM PAD" && rec0[2] === 0 && rec0[3] === 0, "PATCH_RAW: a saved slot reads whole (name, slot and revision bytes zero)");
  await LIB.rawWrite(u, 30, rec0);
  const back = await LIB.rawRead(u, 30);
  ok(same(back, rec0) && m.patches.get(30).name === "WARM PAD", "PATCH_RAW: written into another slot, read back the same bytes");
  await u.listSlots();
  ok(u.slots[30].saved && u.slots[30].name === "WARM PAD" && u.slots[30].eng === 0, "PATCH_RAW: UP_LIST shows the new slot saved with the record's name and engine");
  const bytes = rec0.slice(); bytes.fill(0, 7, 19); bytes.set(Buffer.from("RENAMED"), 7);
  await LIB.rawWrite(u, 30, bytes); ok(parseRecord(await LIB.rawRead(u, 30)).name === "RENAMED", "PATCH_RAW: a rename through the bytes");
  m.playing = true;
  ok(await rejects(() => LIB.rawWrite(u, 31, rec0), /playing/), "PATCH_RAW: refused while the unit plays (rc 3)");
  ok(!m.patches.has(31) && (await LIB.rawRead(u, 0)) !== null, "PATCH_RAW: ..nothing was written, reading still works while it plays");
  m.playing = false;
  ok(await rejects(() => LIB.rawWrite(u, 128 - 1, new Uint8Array(30)), /refused/), "PATCH_RAW: a record the store would not read is refused at the commit (rc 1)");
  ok(!m.patches.has(127) && m.log.filter((x) => x.startsWith("raw write")).length === 2, "PATCH_RAW: ..and nothing was written");
  // transfer rules: a wrong CRC, out of order, no begin, abort
  const raw = (a) => u.req(CMD.PATCH_RAW, a);
  const begin = (slot, len, c) => raw([2, slot, len & 127, len >> 7, ...c]);
  const b5 = (n) => Array.from({ length: 5 }, (_, i) => (n >>> (7 * i)) & (i === 4 ? 15 : 127));
  ok((await begin(5, rec0.length, b5(bkCrc(rec0))))[2] === 0 && (await raw([4, 5]))[2] === 2, "PATCH_RAW: a commit before all bytes arrived: rc 2");
  ok((await raw([3, 5, 0, 0, 0]))[2] === 5, "PATCH_RAW: a piece after a failed commit is stale (rc 5)");
  ok((await begin(5, 100, b5(1)))[2] === 0 && (await raw([3, 5, 1, 0, 0, 0]))[2] === 1 && (await raw([5, 5]))[2] === 0 && (await raw([3, 5, 0, 0, 0]))[2] === 5, "PATCH_RAW: out of order is rc 1, abort ends the transfer");
  ok((await begin(128, 100, b5(0)))[2] === 1 && (await begin(0, 19, b5(0)))[2] === 1 && (await begin(0, 535, b5(0)))[2] === 1 && (await begin(0, 534, b5(0)))[2] === 0 && (await raw([5, 0]))[2] === 0 && (await raw([9, 0]))[2] === 1, "PATCH_RAW: slot 128, a length under 20 or over the biggest record (PT_WORST 534; 534 itself is accepted), an unknown op: rc 1");
}

// ---- the file: round trips
{
  const { u } = await connect();
  const items = await LIB.readItems(u, [0, 1, 5, 7]);
  ok(items[0].kind === "record" && items[1].kind === "record" && items[3].kind === "approx" && items[0].number === 1 && items[0].name === "WARM PAD", "export: a saved slot is its record (lossless), a factory slot an approximate copy of its values");
  const text = stringifyFile(buildFile(items, { firmware: u.info.version, source: "unit" }));
  const f = JSON.parse(text);
  ok(f.format === "duell-patches" && f.version === 1 && f.patches.length === 4 && f.patches[0].record && f.patches[0].crc32 && f.patches[0].params && f.patches[3].approximate === true && f.patches[3].values.length === L.P_COUNT && !f.patches[3].record, "file: format, version, firmware, per patch name / engine / category / base64 record / CRC-32 / readable params; a factory slot has its values");
  ok(f.patches[0].engine === "ANALOG" && f.patches[0].category === "PAD" && f.patches[0].number === 1 && f.firmware === u.info.version, "file: engine and category are names, the number is the #NN of the slot");
  const r = parseFile(text);
  ok(r.ok && r.items.length === 4 && r.problems.length === 0 && same(r.items[0].record, items[0].record) && r.items[3].kind === "approx" && same(r.items[3].values, items[3].values), "file: read back, every record the same bytes, the values of the copy the same");
  const t2 = JSON.parse(text); t2.patches[0].record = t2.patches[0].record.replace(/^./, (c) => (c === "A" ? "B" : "A"));
  const r2 = parseFile(JSON.stringify(t2));
  ok(r2.ok && r2.items.length === 3 && /checksum/.test(r2.problems[0]), "file: a record that fails its CRC-32 is left out with the reason");
  const t3 = JSON.parse(text); t3.patches[1].engine = "WAVE";
  ok(/engine/.test(parseFile(JSON.stringify(t3)).problems[0]), "file: a record whose engine is not the one named is left out");
  ok(!parseFile("{").ok && !parseFile('{"format":"x"}').ok && /wavetable/.test(parseFile('{"format":"duell-wavetable"}').error) && /version 9/.test(parseFile('{"format":"duell-patches","version":9,"patches":[]}').error), "file: not JSON, another format, a wavetable file, a newer version: refused with a reason");
  ok(sniff(new TextEncoder().encode(text)) === "patches" && sniff(Uint8Array.from([0xf0, 0x43, 0, 9, 0x20, 0, 1])) === "dx7" && sniff(new TextEncoder().encode('{"format":"duell-wavetable"}')) === "wavetable" && sniff(Uint8Array.from([0x52, 0x49, 0x46, 0x46, 0])) === "wav", "file: a dropped file is told by its bytes (patches, DX7 sysex, wavetable, WAV)");
  ok(same(fromBase64(toBase64(Uint8Array.from([0, 255, 128, 7]))), [0, 255, 128, 7]), "file: base64 round trip");
}

// ---- import: targets, plan, safety copy, verify, progress, failure
{
  const { m, u } = await connect();
  const src = await LIB.readItems(u, [0, 1, 5, 64]);
  ok(same(LIB.chooseTargets(u, src, { mode: "same" }), [0, 1, 5, 64]), "targets: the same slot numbers as the file");
  ok(same(LIB.chooseTargets(u, src, { mode: "from", start: 10 }), [10, 11, 12, 13]), "targets: consecutive from the slot the user picked");
  ok(same(LIB.chooseTargets(u, src, { mode: "from", start: 0, skipSaved: true }), [2, 3, 4, 6]), "targets: consecutive, skipping the slots that hold a saved patch");
  ok(same(LIB.chooseTargets(u, src, { mode: "from", start: 126 }), [126, 127, null, null]), "targets: running past slot 128 leaves the rest without a place");
  let plan = LIB.makePlan(u, src, [0, 1, 1, null]);
  ok(plan.problems.length === 2 && /both go to #2/.test(plan.problems[0]) && /no slot/.test(plan.problems[1]), "plan: two patches to one slot and a patch without a place are reported");
  plan = LIB.makePlan(u, src, [20, 21, 22, 23]);
  ok(plan.problems.length === 0 && plan.writes.length === 4 && plan.savedLost === 0 && plan.writes[0].replaces.label === "#21" && !plan.writes[0].replaces.saved, "plan: four writes onto factory slots lose nothing saved");
  const plan2 = LIB.makePlan(u, src, [0, 1, 5, 64]);
  ok(plan2.savedLost === 4 && LIB.confirmLines(plan2).length === 4 && /#1 "WARM PAD" \(saved\) becomes "WARM PAD"/.test(LIB.confirmLines(plan2)[0]), "plan: overwriting saved slots is listed one by one (slot, what it holds, what it becomes)");
  // run: safety copy first (4 slots replaced), progress, verify
  let safe = null; const prog = [];
  const res = await LIB.runPlan(u, plan2, { safety: async (text, items) => { safe = { text, items }; }, onProgress: (p) => prog.push(p) });
  ok(res.done.length === 4 && !res.failed && res.mismatched.length === 0, "run: four writes done and verified");
  ok(safe && safe.items.length === 4 && parseFile(safe.text).items.length === 4 && /safety/.test(JSON.parse(safe.text).source), "run: a safety copy of the four saved patches it replaces was handed over before the first write");
  ok(prog.filter((p) => p.phase === "write").length === 4 && prog.at(-1).phase === "done" && prog[0].total === 4, "run: progress is reported per slot and at the end");
  // a mismatch is noticed: the unit keeps something else than was sent
  const mine = await LIB.readItem(u, 0); const alt = { ...mine, crc: mine.crc ^ 1 };
  ok(!(await LIB.verifyItem(u, 0, alt)) && (await LIB.verifyItem(u, 0, mine)), "verify: a record the unit holds differently is noticed (CRC)");
  // no safety copy for fewer than four slots; one for four or more only when something saved is lost
  let called = 0;
  await LIB.runPlan(u, LIB.makePlan(u, src.slice(0, 2), [0, 1]), { safety: async () => { called++; } });
  await LIB.runPlan(u, LIB.makePlan(u, src, [40, 41, 42, 43]), { safety: async () => { called++; } });
  ok(called === 0, "run: no safety copy for fewer than four slots, none when nothing saved is replaced");
  // a failure stops the run; what was written stays
  m.fail = { cmd: 84, op: 4, rc: 4 };
  const res2 = await LIB.runPlan(u, LIB.makePlan(u, src, [50, 51, 52, 53]), {});
  ok(res2.failed?.slot === 50 && res2.done.length === 0 && /flash/.test(res2.failed.error.message), "run: a flash failure at the first slot stops the run with the unit's reason (the slot is as it was)");
  m.fail = null;
  m.playing = true;
  const res3 = await LIB.runPlan(u, LIB.makePlan(u, src, [60, 61, 62, 63]), {});
  ok(res3.failed && /playing/.test(res3.failed.error.message) && !m.patches.has(60), "run: refused while the unit plays; nothing written");
  m.playing = false;
  // an approximate copy (a factory sound) goes through UP_PUT
  const fac = (await LIB.readItems(u, [100]))[0];
  const r4 = await LIB.runPlan(u, LIB.makePlan(u, [fac], [101]), {});
  ok(r4.done.length === 1 && u.slots[101].saved && u.slots[101].name === fac.name && u.slots[101].eng === fac.engine && m.log.includes("put 101"), "copy of a factory sound: written with UP_PUT (an approximate copy), verified by its values");
  // an older unit without PATCH_RAW: records cannot be taken whole
  const old = await connect({ patchRaw: false });
  ok(!LIB.hasRaw(old.u) && LIB.makePlan(old.u, src, [1, 2, 3, 4]).problems.length === 4 && /whole/.test(LIB.makePlan(old.u, src, [1, 2, 3, 4]).problems[0]), "an older firmware: a patch file cannot be imported whole (reported per patch), the editor still works");
  const oi = (await LIB.readItems(old.u, [0, 5]));
  ok(oi.every((x) => x.kind === "approx") && oi[0].name === "WARM PAD", "an older firmware: export gives approximate copies (UP_GET values)");
}

// ---- copy, move, swap
{
  const { m, u } = await connect();
  const A = await LIB.rawRead(u, 0), B = await LIB.rawRead(u, 1);
  ok((await LIB.swapSlots(u, 0, 1)).both && same(await LIB.rawRead(u, 0), B) && same(await LIB.rawRead(u, 1), A), "swap: two saved patches change places, bit for bit");
  await LIB.swapSlots(u, 1, 0);
  const r = await LIB.swapSlots(u, 0, 9);
  ok(r.moved && !r.both && (await LIB.rawRead(u, 0)) === null && same(await LIB.rawRead(u, 9), A) && !u.slots[0].saved && u.slots[9].saved, "swap with a factory slot: the patch moves, its old place shows its own factory sound again");
  const r2 = await LIB.swapSlots(u, 4, 9);
  ok(r2.moved && same(await LIB.rawRead(u, 4), A) && (await LIB.rawRead(u, 9)) === null && u.slots[4].saved && !u.slots[9].saved, "swap: a factory slot first and the saved patch second: the patch moves the other way");
  ok(!(await LIB.swapSlots(u, 30, 31)).moved, "swap: two factory slots: nothing to do");
  ok(LIB.swapLines(u, 4, 0).length === 2 && /moves to/.test(LIB.swapLines(u, 4, 0)[0]) && /change places/.test(LIB.swapLines(u, 1, 4)[0]), "swap: the confirm text says what happens in both cases");
  const copyPlan = await LIB.planCopy(u, 4, 20);
  await LIB.runPlan(u, copyPlan, {});
  ok(same(await LIB.rawRead(u, 20), await LIB.rawRead(u, 4)) && u.slots[4].saved, "copy: the source stays, the target holds the same record");
  void m;
}

// ---- DX7: two synthetic banks (A: 32 clean voices, B: voice 31 damaged) and a single voice; DX7_BANKS=dir also runs every .syx bank found there (optional, real banks stay local)
{
  const banksDir = process.env.DX7_BANKS, extra = banksDir ? readdirSync(banksDir).filter((f) => /\.syx$/i.test(f)).sort() : [];
  const files = ["TEST BANK A.syx", "TEST BANK B.syx", ...extra];
  const bytesOf = (f) => (f === files[0] ? synthBank(0).bytes : f === files[1] ? synthBank(1, [30]).bytes : new Uint8Array(readFileSync(`${banksDir}/${f}`)));
  let voices = 0, damaged = 0, converted = 0, names = 0, structure = 0, patches = 0, noDx = 0, cats = {}; const fixture = [];
  for (const f of files) {
    const p = parseDx7File(bytesOf(f));
    ok(p.kind === "bank" && p.voices.length === 32, `dx7: ${f}: a bank of 32 voice places`);
    if (f === files[1]) ok(p.voices[30].damaged && p.voices.filter((x) => x.damaged).length === 1 && p.voices[31].name === FM6.name(synthVoice(63)) && p.voices[30].voice === null, "dx7: bank B: voice 31 has bytes >= 128: only that voice is damaged, the voice after it is kept");
    for (const v of p.voices) {
      if (v.damaged) { damaged++; continue; }
      voices++;
      const c = voiceToPatch(v.voice, v.index), r = parseRecord(c.record);
      converted += r.ok ? 1 : 0;
      names += /^[ -~]{1,12}$/.test(c.name) && c.name === c.name.trim() ? 1 : 0;
      noDx += /DX\s*7/i.test(c.name) ? 0 : 1;
      structure += r.ok && r.engine === FM6E && same([...r.fm6], [...FM6.pack(FM6.sanitize(v.voice))]) && r.values[P_E0 + 7] === 8 ? 1 : 0;
      patches += r.ok && r.values[45] === 71 && r.values[36] > 0 && L.FMTS[L.TP[36].fk][r.values[36]][0] === "7" && r.values[33] === 0 && r.values[34] === 0 && r.values[35] === 0 && r.values[88] === 0 && r.values[8] === 0 && r.values[103] === 0 ? 1 : 0;
      cats[c.cat] = (cats[c.cat] || 0) + 1;
      fixture.push([f.replace(/\.syx$/, ""), v.index, c.name, Buffer.from(c.patch128).toString("hex"), Buffer.from(c.record).toString("hex"), c.values.join(",")].join("\t"));
    }
  }
  const synthOnly = !extra.length;
  ok(synthOnly ? voices === 63 && damaged === 1 : voices >= 63 && damaged >= 1, `dx7: ${voices} voices parse (synthetic banks: 32 + 31), ${damaged} damaged (bank B voice 31, the corrupt voice the importer leaves out)`);
  ok(converted === voices && names === voices && noDx === voices, "dx7: every voice becomes a well-formed record with a clean name (1..12 plain characters), none called DX7");
  ok(structure === voices, "dx7: each record is an FM6 patch carrying exactly the voice (the packed patch, SLOT OWN)");
  ok(patches === voices, "dx7: the owner's neutral defaults: MULTI FILTER 6.8 kHz, REV 7 %, SAT / CHO / DLY 0, EQ flat");
  ok(Object.keys(cats).length >= 5, `dx7: the name-based category guess spreads over ${Object.keys(cats).length} categories (guessed, the page says so)`);
  ok(cleanVoiceName("  DX7  Piano 1 ") === "PIANO 1" && cleanVoiceName("") === "FM VOICE 01" && cleanVoiceName("E.Piano\u00e9\u0001") === "E.PIANO" && cleanVoiceName("ABCDEFGHIJKLMNOP").length === 12 && SOURCE_LABEL === "DX7 sysex import", "dx7: names are cleaned (printable, upper case, DX7 word dropped, at most 12), the source label is only a label");
  ok(guessCategory("Sub Bass 2") === 1 && guessCategory("Warm Pad") === 3 && guessCategory("E.Piano 1") === 5 && guessCategory("Tubular Bells") === 4 && guessCategory("Zzzz") === 8, "dx7: category guesses (BASS, PAD, KEYS, PLUCK, OTHER)");
  // a single voice (the 163 byte sysex) and the raw 155 bytes
  const bank = parseDx7File(bytesOf(files[0])), sg = synthSingle(4);
  const one = FM6.singleSysex(bank.voices[4].voice), p1 = parseDx7File(one), p2 = parseDx7File(sg.raw155);
  ok(same([...sg.bytes.subarray(0, 6)], [0xF0, 0x43, 0, 0, 1, 0x1B]) && same([...sg.bytes.subarray(6, 161)], [...sg.raw155]) && sg.bytes.length === 163 && sg.bytes[162] === 0xF7 && one.length === 163 && sg.raw155.length === 155, "dx7: the single voice message is F0 43 00 00 01 1B + 155 bytes + checksum + F7 (163)");
  ok(p1.kind === "single" && p1.voices.length === 1 && p1.voices[0].name === bank.voices[4].name && p2.kind === "single", "dx7: a single voice (163 byte sysex or the raw 155 bytes) is one voice");
  ok(parseDx7File(Uint8Array.from([1, 2, 3])).kind === "none", "dx7: a file with no voice is none");
  // the whole path: bank -> file -> import into the mock -> the unit holds FM6 patches that load as the voice
  const { m, u } = await connect();
  const items = bank.voices.map((v) => itemFromRecord(voiceToPatch(v.voice, v.index).record, { number: null, source: SOURCE_LABEL }));
  const plan = LIB.makePlan(u, items, LIB.chooseTargets(u, items, { mode: "from", start: 32 }));
  const res = await LIB.runPlan(u, plan, { verify: true });
  ok(res.done.length === 32 && !res.failed && !res.mismatched.length, "dx7: 32 voices written to slots 33..64 through PATCH_RAW, each verified");
  await u.loadSlot(0, 40);
  ok(u.parts[0].eng === FM6E && same([...m.parts[0].fm6], [...FM6.pack(FM6.sanitize(bank.voices[8].voice))]) && u.parts[0].p[P_E0 + 7] === 8 && u.parts[0].p[45] === 71, "dx7: a slot loaded into a part is an FM6 sound with the voice as its own patch (SLOT OWN) and the neutral filter");
  if (process.env.DX7_FIXTURE) { writeFileSync(process.env.DX7_FIXTURE, fixture.join("\n") + "\n"); console.log(`(fixture: ${fixture.length} voices -> ${process.env.DX7_FIXTURE})`); }
}

// ---- wavetable files
{
  const data = Uint8Array.from({ length: WT.BYTES }, (_, i) => ampToByte(1 / (1 + (i % 64))));
  const text = tableToFile("MYTABLE", data, { firmware: "FELUCCA DUELL 0.2.22 beta", source: "test" });
  const j = JSON.parse(text);
  ok(j.format === TABLE_FORMAT && TABLE_FORMAT === "duell-wavetable" && j.version === 1 && j.frames === 19 && j.harmonics === 64 && j.name === "MYTABLE" && typeof j.data === "string" && /^[0-9a-f]{8}$/.test(j.crc32), "wavetable file: format, version, 19 x 64, name, base64 data, CRC-32");
  const r = parseTableFile(text);
  ok(r.ok && r.name === "MYTABLE" && same(r.data, data), "wavetable file: read back, the 1216 bytes the same");
  j.data = j.data.replace(/^./, (c) => (c === "A" ? "B" : "A"));
  ok(!parseTableFile(JSON.stringify(j)).ok && /checksum/.test(parseTableFile(JSON.stringify(j)).error), "wavetable file: damaged data fails its CRC-32");
  ok(!parseTableFile("{").ok && /patch/.test(parseTableFile('{"format":"duell-patches"}').error) && !parseTableFile(JSON.stringify({ ...JSON.parse(text), frames: 20 })).ok, "wavetable file: not JSON, a patch file, another size: refused");
  const z = tableToFile("ZERO", new Uint8Array(WT.BYTES));
  ok(!parseTableFile(z).ok, "wavetable file: an all-zero table is refused (the unit would)");
}

console.log(fails ? `\nLIBRARIAN TESTS FAILED (${fails})` : "\nlibrarian tests passed");
process.exit(fails ? 1 : 0);
