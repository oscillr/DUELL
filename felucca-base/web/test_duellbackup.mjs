// SPDX-License-Identifier: GPL-3.0-only
// The Backup page's logic (editor/duellbackup.js) against a simulated unit (editor/mockunit.js): INFO, the "duell-backup" file round trip,
// every refusal before a write, the restore order and what is reported, the safety backup, the verify, older firmware and older files.
import { OLD_PATCH_IDS, PATCH_ID, VERSION, GROUPS, DUELL_IDS, RESTORE_ORDER, patchKey, patchSlot, isPatchKey, objectLabel, parseInfo, parseList, parsePage, readArchive, objectsToFile, captureBackup, planRestore, runRestore,
         verifyRestore, describe, checkObject, listUnit, fileName, rawLines, MULTI_VERSION } from "./editor/duellbackup.js";
import { MockUnit, buildPatchRecord, buildLegacyPatchChunk, buildMultiObject, buildPatternObject, MOCK_ENGINES } from "./editor/mockunit.js";
import { buildRecord, parseRecord, storeDefaults, engineIndex, PTF, XB_DEFS } from "./editor/patchrecord.js";
import { CMD } from "./editor/proto.js";
import { buildObject as wtObject, WT } from "./editor/wavetable.js";
import { readFileSync } from "node:fs";
import { bkCrc, bkPack, BACKUP_IDS_FULL, BACKUP_IDS_WT, BACKUP_IDS_P16, bkManifest, bkU32, readBackup, captureBackup as installerCapture, restoreBackup as restoreInst, parseInfo as parseInfoInst } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(100)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const athrows = async (f, re) => { try { await f(); return false; } catch (e) { return re ? re.test(e.message) : true; } };
const throwsMsg = (f, re) => { try { f(); return false; } catch (e) { return re.test(e.message); } };
const all = new Set(GROUPS.map((g) => g.key));
const clone = (x) => JSON.parse(JSON.stringify(x));
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const PATCH_IDS_6 = OLD_PATCH_IDS.slice(0, 6), PATCH_IDS_X = OLD_PATCH_IDS.slice(6);   // the physical chunks of an older firmware (the installer's backup still carries them)
const nothing = new Uint8Array(0);
const patchRows = (u) => [...u.patches.keys()].sort((a, b) => a - b).map((k) => `${k}:${bkCrc(u.patchBytes(k))}`).join();   // the unit's saved patches: slot and CRC of the record
const bytesOf = (a) => new Map([...a.objects].map(([id, o]) => [id, o.bytes]));

// ---- INFO and the list
{
  const u = new MockUnit();
  const info = parseInfo(await u.request([1, []]));
  ok(info.version === "FELUCCA DUELL 0.2.25 bet" && info.engines.length === MOCK_ENGINES.length && info.caps === 63 && info.patchCap === 128, "INFO: version, engine names, backup caps 63 (bit 2: multis and patterns, bit 3: user wavetables, bit 4: 128 patches, bit 5: patch records), patch capacity 128");
  ok(parseInfo(await new MockUnit({ caps: 3 }).request([1, []])).caps === 3, "INFO: a unit without bit 2 reports caps 3");
  const wire = await u.request([65, []]);
  ok(wire[0] === 1 && wire[1] === 0 && wire[2] === 24 && Array.from({ length: 24 }, (_, i) => wire[3 + 11 * i]).join() === "0,1,2,3,4,5,8,13,14,15,16,17,18,19,20,21,22,23,24,25,26,32,33,34", "LIST (caps 63): 24 ids, 0 1 2 3 4 5 8 13..26 32 33 34; the patch chunk ids 6 7 9..12 27..31 35..39 are not listed");
  const list = await listUnit((r, o) => u.request(r, o));
  ok(same([...list.keys()].sort((a, b) => a - b), DUELL_IDS) && list.get(14).size > 8 && list.get(2) === undefined, "LIST: the 16 DUELL objects other than patches (the retired ids 2..5 8 32..34 are left out)");
  ok(list.get(25).size === 1228 && !list.has(24) === false && list.get(24).size === 0, "LIST: the user wavetables 24..26: USR2 holds the 1228 byte object, USR1 and USR3 are empty (length 0)");
  const full = await listUnit((r, o) => u.request(r, o), 63);
  ok(same([...full.keys()].filter(isPatchKey), [1000, 1001, 1005, 1064]) && full.size === 20 && full.get(patchKey(0)).size === u.patchBytes(0).length && full.get(patchKey(0)).crc === bkCrc(u.patchBytes(0)), "LIST + pages: the four saved patches (slots 0 1 5 64) are merged into the same Map under their keys, with the record's size and CRC");
  const pg = await u.request([65, [1]]);
  ok(pg[0] === 1 && pg[1] === 0 && pg[2] === 3 && pg[3] === 0 && pg.length === 3 + 3 * 11 && (await u.request([65, [3]]))[2] === 1 && (await u.request([65, [2]]))[2] === 0, "LIST page 1: 3 patches (slot, size u32, crc u32), page 3 holds slot 64, page 2 none");
  ok((await u.request([65, [0]]))[1] === 1 && (await u.request([65, [5]]))[1] === 1, "LIST page 0 and 5: rc 1");
  const fresh = new MockUnit();
  ok((await fresh.request([65, [1]]))[1] === 5, "LIST page before the plain LIST: rc 5 (stale)");
  await fresh.request([65, []]); fresh.playing = true;
  ok((await fresh.request([65, [1]]))[1] === 3, "LIST page while playing: rc 3");
  const c7 = await listUnit((r, o) => new MockUnit({ caps: 7 }).request(r, o));
  ok(c7.size === 13 && !c7.has(24) && !c7.has(27), "LIST of a unit with caps 7 (0.2.14 before the wavetables): 13 objects, no wavetables");
  const old = await listUnit((r, o) => new MockUnit({ caps: 3 }).request(r, o));
  ok(old.size === 2 && !old.has(13) && !old.has(21), "LIST of a 0.2.13 unit (16 ids, six of them patch chunks an older firmware lists): music and settings only here, no multis, no patterns");
  const o31 = new MockUnit({ caps: 31 }), l31 = await listUnit((r, o) => o31.request(r, o), 31);
  ok(l31.size === 16 && [...l31.keys()].every((k) => !isPatchKey(k)) && (await o31.request([65, [1]]))[1] === 1, "LIST of a unit with caps 31 (no bit 5): the chunk ids it lists are ignored, no patches, a page answers rc 1");
  ok(throwsMsg(() => parseList([1, 3, 0]), /playing/), "LIST while playing (rc 3): says to stop playback");
  ok(throwsMsg(() => parseList([1, 0, 1, 99, ...bkU32(0), ...bkU32(0)]), /newer editor/), "LIST: an unknown object id is refused");
  const pgOk = [1, 0, 1, 40, ...bkU32(50), ...bkU32(7)];
  ok(parsePage(pgOk, 2).get(patchKey(40)).size === 50 && throwsMsg(() => parsePage(pgOk, 1), /page 1/) && throwsMsg(() => parsePage([1, 0, 1, 40, ...bkU32(19), ...bkU32(7)], 2), /size/) && throwsMsg(() => parsePage([1, 0, 1, 40, ...bkU32(535), ...bkU32(7)], 2), /size/)
     && throwsMsg(() => parsePage([1, 0, 2, 40, ...bkU32(50), ...bkU32(7), 40, ...bkU32(50), ...bkU32(7)], 2), /page 2/) && throwsMsg(() => parsePage(pgOk.slice(0, 10), 2), /incomplete/) && throwsMsg(() => parsePage([1, 5, 0], 1), /expired|Start again/),
     "LIST page: a slot off the page, out of order, a size outside 20..534, a short reply and rc 5 are refused");
}

// ---- checks of single objects
{
  const mu = buildMultiObject(["A", "B"]), pa = buildPatternObject(0, { 0: { name: "X" } }), pc = buildPatchRecord({ name: "P", engine: 0, cat: 2 });
  ok(checkObject(14, mu) === null && checkObject(21, pa) === null && checkObject(patchKey(3), pc) === null && checkObject(14, new Uint8Array(0)) === null, "objects: good multi, pattern, patch record and empty multi pass");
  const t = (id, b, f) => { const c = b.slice(); f(c); return checkObject(id, c) !== null; };
  ok(t(14, mu, (c) => { c[0] ^= 1; }) && t(14, mu, (c) => { c[4]++; }) && t(14, mu, (c) => { c[8] = 0; }) && t(14, mu, (c) => { c[8] = 16; }) && t(14, mu, (c) => { c[8 + 8] = 7; }) && t(14, mu, (c) => { c[9] = 3; }),
     "objects: a multi with a bad magic, length, version, name character or part count is refused");
  const music = new MockUnit({ seed: false }).objs.get(0);
  ok(checkObject(0, music) === null && t(0, music, (c) => { c[63] = 4; }) && t(0, music, (c) => { c[67] = 4; }) && t(0, music, (c) => { c[67] = 5; }) && t(0, music, (c) => { c[67] = 6; }) && t(0, music, (c) => { c[66] = 122; }) && t(0, music, (c) => { c[0] ^= 1; }) && checkObject(0, music.slice(0, 3000)) !== null
     && /older DUELL version/.test(checkObject(0, music.map((x, i) => i === 63 ? 4 : x))),
     "objects: the music of two parts and revision 7 passes; one of four parts (byte 63), revision 4, 5 or 6 (byte 67), another P_COUNT, a bad magic or size is refused, the old one with a clear reason");
  const at = 8 + 11;   // (the second slot's record would be later; the first slot's record starts at 8: rlen 2, flags, LEN, DIV, SWING, GATE, nlen, name ...)
  ok(t(21, pa, (c) => { c[0] ^= 1; }) && t(21, pa, (c) => { c[8 + 3] = 0; }) && t(21, pa, (c) => { c[8 + 8] = 3; }) && t(21, pa, (c) => { c[8 + 2] = 4; }) &&
     t(21, pa, (c) => { c[6] = 10; }) && t(21, pa, (c) => { c[4]--; }) && t(21, pa, (c) => { c[8 + 6] = 0; }) && checkObject(21, pa.slice(0, 300)) !== null && checkObject(23, pa) !== null && checkObject(21, new Uint8Array(0)) === null,
     "objects: a pattern object with a bad magic, LEN, name, flag bits, slot count, length, GATE or size, or another object's slot count, is refused; length 0 (the defaults again) passes");
  ok(checkObject(21, (() => { const c = pa.slice(); c[3] = 0x32; return c; })()) === "a pattern slot object of an older firmware (16 slots)", "objects: a PTS2 object of the 16-slot firmware is refused with that reason");
  const v = storeDefaults(0); v[45] = 60; v[46] = 20;
  const pv = buildRecord({ engine: 0, name: "VALUES", values: v }), ex = buildRecord({ engine: 0, name: "XBLOCK", values: v, ext: XB_DEFS.map((d, k) => (k === 6 ? 10 : d - 64)), edits: [{ wid: 9, val: 5 }] });
  const k = patchKey(7);
  ok(checkObject(k, ex) === null && checkObject(k, pv) === null && (ex[4] & PTF.XB), "objects: a patch record with values, and one with the extension block and an edit, pass");
  ok(t(k, pc, (c) => { c[2] = 3; }) && t(k, pc, (c) => { c[3] = 1; }) && t(k, pc, (c) => { c[0]++; }) && t(k, pc, (c) => { c[4] &= ~PTF.CB; }) && t(k, pc, (c) => { c[5] = 99; }) && t(k, pc, (c) => { c[7] = 0; }) && t(k, pc, (c) => { c[8] = 7; }) && t(k, pc, (c) => { c[18] = 65; c[10] = 0; })
     && checkObject(k, pc.slice(0, 19)) !== null && checkObject(k, nothing) !== null && checkObject(k, new Uint8Array(535)) !== null,
     "objects: a patch record with a slot or revision byte not 0, a wrong length field, no CB flag, an unknown engine, no name, a control character or a gap in its name, a size under 20, 0 or over 534 is refused");
  ok(t(k, pv, (c) => { c[c.length - 1] = 200; }) && t(k, pv, (c) => { c[20] = 127; }) && t(k, pv, (c) => { c[19]++; }) && t(k, ex, (c) => { c[21] = 2; }) && t(k, ex, (c) => { c[20]++; }) && t(k, ex, (c) => { c[4] &= ~PTF.XB; }),
     "objects: a patch record with a value byte over 191, a parameter id that does not exist, a wrong value count, an extension block of version 2, a block length off by one or the flag cleared is refused");
  ok(OLD_PATCH_IDS.every((id) => checkObject(id, buildLegacyPatchChunk([{ slot: 3, name: "P" }])) !== null) && checkObject(40, pc) !== null, "objects: a physical patch chunk (ids 6 7 9..12 27..31 35..39) and the bare wire id 40 are no objects of a backup");
  ok(checkObject(0, new Uint8Array(3840)) !== null && checkObject(1, new Uint8Array(584)) !== null && checkObject(2, new Uint8Array(0)) !== null, "objects: music / settings without their magic, and a retired id, are refused");
  ok(objectLabel(patchKey(6), pc) === "patch #07 P" && objectLabel(patchKey(127)) === "patch #128" && objectLabel(6) === "a chunk of the old patch store" && patchSlot(patchKey(9)) === 9 && !isPatchKey(40) && !isPatchKey(1128) && isPatchKey(1127), "labels: patch #NN NAME from the slot (1-based, as the Patches page) and the record");
}

// ---- the user wavetables (ids 24..26)
{
  const data = Uint8Array.from({ length: WT.BYTES }, (_, i) => 1 + (i * 7) % 255), w = wtObject("MY TABLE", data);
  const t = (f) => { const c = w.slice(); f(c); return checkObject(25, c) !== null; };
  ok(checkObject(24, w) === null && checkObject(26, new Uint8Array(0)) === null && w.length === 1228, "objects: a good wavetable object and an empty one pass");
  ok(t((c) => { c[0] = 2; }) && t((c) => { c[1] = 18; }) && t((c) => { c[2] = 63; }) && t((c) => { c[3] = 1; }) && t((c) => { c[6] = 7; }) && t((c) => { c.fill(0, 12); }) && checkObject(25, w.slice(0, 1227)) !== null,
     "objects: a wavetable with another version, frame count, harmonic count, flags, a control character in the name, all zero bytes, or a wrong size is refused");
  const d = describe(new Map([[24, new Uint8Array(0)], [25, w], [26, null]].filter(([, b]) => b)));
  ok(d.wavetables.length === 1 && d.wavetables[0].label === "USR2" && d.wavetables[0].name === "MY TABLE" && d.has.wavetables, "describe: USR2 MY TABLE listed, the empty slots not");
  ok(GROUPS.find((g) => g.key === "wavetables").caps === 8, "the wavetables group needs caps bit 3");
}

// ---- capture, file, round trip
const A = new MockUnit();
const info = parseInfo(await A.request([1, []]));
const seen = []; const rqA = (r, o) => A.request(r, o);
const file = await captureBackup(rqA, info, (d, t) => seen.push([d, t]));
const archive = readArchive(JSON.stringify(file));
ok(file.format === "duell-backup" && file.version === 5 && VERSION === 5 && file.firmware === info.version && file.objects.length === 20 && file.patchRecords === true && !Number.isNaN(Date.parse(file.created)), "file: format duell-backup, version 5, firmware, date, 20 objects (16 others and the 4 saved patches)");
ok(file.objects.slice(16).every((o, i) => o.id === 40 && o.slot === [0, 1, 5, 64][i] && o.size >= 20 && o.size <= 534) && file.objects.slice(0, 16).every((o) => o.id !== 40) && archive.hasPatches && archive.oldPatchChunks === 0 && !archive.notes.length, "file: the patches are {id: 40, slot, size, crc, data} objects, sorted by slot, after the other objects");
ok(seen.at(-1)[0] === seen.at(-1)[1] && seen.length > 10, "capture: progress ends at the total");
ok(DUELL_IDS.every((id) => same(archive.objects.get(id).bytes, A.objs.get(id) || new Uint8Array(0))) && [...A.patches.keys()].every((k) => same(archive.objects.get(patchKey(k)).bytes, A.patchBytes(k))) && archive.objects.size === 20,
   "round trip: capture -> JSON -> bytes equal the unit's objects, empty ones as 0, and each patch equals the record the unit gives out");
const d = describe(bytesOf(archive), archive.engines);
ok(d.patches.map((p) => `${p.slot}:${p.name}:${p.engineName}:${p.category}`).join() === "0:WARM PAD:ANALOG:PAD,1:SUB BASS:ANALOG:BASS,5:BELL:FM6:KEYS,64:WAVE LEAD:WAVE:LEAD", "preview: patch names, engines and categories by slot");
ok(d.multis.map((m) => `${m.label}:${m.name}`).join() === "M03:TESTONE,M04:TESTTWO,M12:LIVE SET", "preview: multi names (M03 M04 in object 14, M12 in object 18)");
ok(d.patterns.length === 32 && d.patterns[2].label === "P03" && d.patterns[2].name === "PAT THREE" && d.patterns[2].length === 12 && d.patterns[31].label === "P32" && d.patterns[31].name === "SEVEN EIGHT" && d.patterns[31].length === 14 && d.patterns[0].name === "MORODER OCT", "preview: 32 pattern slots by name and length (P03 PAT THREE 12, P32 SEVEN EIGHT 14)");
ok(await athrows(() => captureBackup((r, o) => { const x = new MockUnit(); x.playing = true; return x.request(r, o); }, info), /playing/), "capture while the unit plays: refused with a plain message");
{
  const u = new MockUnit(); let n = 0;
  const f2 = await captureBackup(async (r, o) => { const a = await u.request(r, o); if (r[0] === 66 && ++n === 3) u.objs.get(0)[3000] ^= 1; return a; }, info);
  const a2 = readArchive(JSON.stringify(f2));
  ok(f2.problems?.length === 1 && f2.problems[0].id === 0 && /changed/.test(f2.problems[0].problem) && a2.objects.get(0).raw && a2.objects.size === 20 && rawLines(a2.problems)[0].startsWith("Object 0 (the current music) could not be checked"),
     "capture (0.2.25): an object that changes during the read is kept as raw data with a warning line; the backup goes on");
}
{   // more than 32 patches: the four pages, every patch read in pieces of 256 (a record of 534 bytes needs three)
  const U = new MockUnit({ seed: false }), want = [];
  for (let slot = 0; slot < 128; slot += 3) {            // 43 patches over all four pages; a few with the extension block and a long FM6 record
    const eng = slot % 5 === 0 ? 14 : 0, more = slot % 9 === 0 ? { ext: XB_DEFS.map((d, k) => (k === slot % 15 ? d - 64 + 3 : d - 64)), edits: Array.from({ length: 32 }, (_, i) => ({ wid: i + 1, val: i - 10 })) } : {};
    U.setRecord(slot, buildPatchRecord({ name: `PATCH ${slot}`, engine: eng, cat: slot % 9, values: storeDefaults(eng).map((v, i) => (i === 50 + slot % 20 ? v + 1 : v)), ...more })); want.push(slot);
  }
  U.rebuild();
  const rq = (r, o) => U.request(r, o), pagesSeen = [], gets = [];
  const f = await captureBackup(async (r, o) => { if (r[0] === 65 && r[1].length) pagesSeen.push(r[1][0]); if (r[0] === 66 && r[1][0] === 40) gets.push(r[1].length); return rq(r, o); }, info);
  const a = readArchive(JSON.stringify(f)), keys = [...a.objects.keys()].filter(isPatchKey);
  ok(want.length === 43 && same(pagesSeen, [1, 2, 3, 4]) && same(keys, want.map(patchKey)) && want.every((k) => same(a.objects.get(patchKey(k)).bytes, U.patchBytes(k))) && gets.every((n) => n === 9) && gets.length >= 43,
     "capture: 43 patches over the four LIST pages (1..4), each read with GET 40 (9 byte requests with the slot), every record equals the unit's");
  {   // a record of over 256 bytes (an FM6 sound with its patch, the extension block with 32 edits and 100 values off their default) is read in two pieces
    const fm = engineIndex("FM6"), vv = storeDefaults(fm).map((v, i) => (i >= 20 && i < 120 ? (v >= 100 ? v - 1 : v + 1) : v));
    U.setRecord(126, buildPatchRecord({ name: "BIGGEST", engine: fm, values: vv, patch128: Array.from({ length: 128 }, (_, i) => i), mb: [1, 2, 3, 4, 5, 6, 7], ext: XB_DEFS.map((d) => d - 64 + 1), edits: Array.from({ length: 32 }, (_, i) => ({ wid: i, val: i })) }));
    const g = []; const f2 = await captureBackup(async (r, o) => { if (r[0] === 66 && r[1][8] === 126) g.push(r[1][6] | r[1][7] << 7); return rq(r, o); }, info), a2 = readArchive(JSON.stringify(f2));
    ok(a2.objects.get(patchKey(126)).size > 256 && g.length === 2 && g[0] === 256 && same(a2.objects.get(patchKey(126)).bytes, U.patchBytes(126)) && parseRecord(a2.objects.get(patchKey(126)).bytes).ok, `capture: a ${a2.objects.get(patchKey(126)).size} byte record is read in two pieces (256 and the rest) and equals the unit's`);
    U.patches.delete(126);
  }
  const seen = []; await captureBackup(rq, info, (d, t) => seen.push([d, t]));
  const total = [...(await listUnit(rq, 63)).values()].reduce((n, o) => n + o.size, 0);
  ok(seen.at(-1)[0] === total && seen.at(-1)[1] === total, "capture: progress counts the bytes of the patches too and ends at the total");
}
{   // a patch that changes while it is read: kept raw with the problem noted, the rest saved
  const U = new MockUnit(); let n = 0;
  const f = await captureBackup(async (r, o) => {
    if (r[0] === 66 && r[1][0] === 40 && r[1][8] === 5 && ++n === 1) { const b = U.patchBytes(5); b[6] = (b[6] + 1) % 9; U.setRecord(5, b); }   // (the category changed on the unit's panel after the LIST)
    return U.request(r, o);
  }, info);
  const a = readArchive(JSON.stringify(f)), p5 = a.objects.get(patchKey(5));
  ok(f.problems?.length === 1 && f.problems[0].id === 40 && f.problems[0].slot === 5 && /changed/.test(f.problems[0].problem) && p5.raw && a.problems[0].id === patchKey(5) && !a.objects.get(patchKey(0)).raw
     && rawLines(a.problems, a.objects)[0].startsWith("Object 1005 (patch #06 ") && a.objects.size === 20,
     "capture: a patch that changes on the unit while it is read is kept as raw data with a warning line naming it; the other patches and objects are saved");
  const V = new MockUnit({ seed: false });
  const pl = planRestore(a, await listUnit((r, o) => V.request(r, o), 63), 63, new Set(["patches"])), pi = planRestore(a, await listUnit((r, o) => V.request(r, o), 63), 63, new Set(["patches"]), { includeRaw: true });
  ok(!pl.steps.some((s) => s.slot === 5) && pl.excluded.some((x) => x.id === patchKey(5) && /could not be checked/.test(x.reason)) && pi.steps.some((s) => s.slot === 5) && pl.steps.length === 3, "restore plan: the raw patch is skipped (and said so) unless the user asks for it");
}
{   // a unit without caps bit 5 gives no patches, and the result says so
  const U = new MockUnit({ caps: 31 }), inf = parseInfo(await U.request([1, []]));
  const f = await captureBackup((r, o) => U.request(r, o), inf), a = readArchive(JSON.stringify(f));
  ok(f.version === 5 && f.patchRecords === false && !f.objects.some((o) => o.id === 40) && a.hasPatches === false && /cannot back up patches one at a time/.test(f.notes?.[0] ?? "") && a.notes.length === 1 && a.objects.size === 16,
     "capture: a unit without caps bit 5 (an older firmware) gives no patches; the file says so (a note, patchRecords false) and holds the rest");
}
{   // 0.2.25, the owner's report: "The backup failed. Nothing was saved. Object 13: damaged multi." (firmware 0.2.23 with an editor that refused version 6)
  const { readFileSync } = await import("node:fs");
  const recs = ["M01", "M02"].map((n) => readFileSync(new URL(`../tools/factory_multis/${n}.bin`, import.meta.url)));
  const ob = new Uint8Array(8 + recs[0].length + recs[1].length); ob.set([0x4D, 0x55, 0x4C, 0x31, recs[0].length & 255, recs[0].length >> 8, recs[1].length & 255, recs[1].length >> 8]); ob.set(recs[0], 8); ob.set(recs[1], 8 + recs[0].length);
  ok(recs[0][0] === MULTI_VERSION && MULTI_VERSION === 7 && checkObject(13, ob) === null, "objects: a multi object as firmware 0.2.22 .. 0.2.24 writes it (version 7 now, 127 values: the factory records M01, M02) passes");
  const oldCheck = (b) => { const at = 8; return b[at] > 2 ? "damaged multi" : null; };   // the 0.2.23 editor's rule (MUL_VER 2, b[at] > MUL_VER refused)
  ok(oldCheck(ob) === "damaged multi", "the 0.2.23 editor's multi check refused every version 6 record: that was the owner's 'Object 13: damaged multi'");
  const u = new MockUnit(); u.objs.set(13, ob);
  const g = await captureBackup((r, o) => u.request(r, o), info), ga = readArchive(JSON.stringify(g));
  ok(!g.problems && same(ga.objects.get(13).bytes, ob) && describe(bytesOf(ga)).multis.slice(0, 2).map((m) => m.name).join() === "REESE SWAMP,NIGHT ORGAN", "capture: a unit holding 0.2.23 / 0.2.24 multis in object 13 backs up without a warning, names listed");
  const bad = ob.slice(); bad[8 + 8] = 7;                               // a control character in M01's name: the editor cannot check it
  u.objs.set(13, bad);
  const gb = await captureBackup((r, o) => u.request(r, o), info), gba = readArchive(JSON.stringify(gb));
  ok(gb.problems?.length === 1 && gb.problems[0].id === 13 && gba.objects.size === 20 && same(gba.objects.get(13).bytes, bad) && same(gba.objects.get(patchKey(0)).bytes, u.patchBytes(0)) && /multis M01 and M02/.test(rawLines(gba.problems)[0]),
     "capture: a damaged multi object does not stop the backup: everything else is saved, object 13 as raw data with a warning naming M01 and M02");
  const T = new MockUnit({ seed: false }), lt = await listUnit((r, o) => T.request(r, o));
  const pr = planRestore(gba, lt, 63, all), pi = planRestore(gba, lt, 63, all, { includeRaw: true });
  ok(!pr.steps.some((s) => s.id === 13) && pr.steps.some((s) => s.id === 14) && pr.excluded.some((x) => x.id === 13 && /could not be checked/.test(x.reason)) && pi.steps.some((s) => s.id === 13),
     "restore plan: the raw object is skipped (and said so) unless the user asks for it; the other multis restore");
  const v5 = ob.slice(); v5[8] = 5; const f5 = objectsToFile([...ga.objects.values()].map((o) => ({ id: o.id, bytes: o.id === 13 ? v5 : o.bytes })), info);
  const p5 = planRestore(readArchive(JSON.stringify(f5)), lt, 63, new Set(["multis"]));
  ok(!p5.steps.some((s) => s.id === 13) && p5.excluded.some((x) => x.id === 13 && /version 5/.test(x.reason)), "restore plan: a multi record of another version (5) is left out with the reason (the unit would refuse it)");
}

// ---- reading files: every refusal comes before any write
{
  const bad = (f) => { const c = clone(file); f(c); return c; };
  ok(throwsMsg(() => readArchive("{ nope"), /JSON/), "file: not JSON");
  ok(throwsMsg(() => readArchive({ format: "something", objects: [] }), /not a DUELL/), "file: another format");
  ok(throwsMsg(() => readArchive(bad((c) => { c.version = 6; })), /newer editor/), "file: a newer version is refused with a plain message");
  ok(throwsMsg(() => readArchive(bad((c) => { c.version = 1; })), /unknown version/), "file: version 1 of the duell-backup format does not exist");
  ok(throwsMsg(() => readArchive(bad((c) => { const o = c.objects.find((x) => x.id === 14); o.crc = (o.crc ^ 1) >>> 0; })), /checksum/), "file: a damaged object (CRC) is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { const o = c.objects.find((x) => x.id === 14); o.data = o.data.slice(0, -4); })), /damaged/), "file: a truncated object is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects.push(c.objects[2]); })), /twice/), "file: an object listed twice is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects[2].id = 99; })), /Unknown object/), "file: an unknown id is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects = c.objects.filter((o) => o.id !== 17); })), /incomplete/), "file: a group with a missing object (multis without id 17) is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects = c.objects.filter((o) => o.id !== 0); })), /no current music/), "file: a file without the current music is refused");
  const evil = clone(file); const m = evil.objects.find((o) => o.id === 14), raw = Uint8Array.from(atob(m.data), (c) => c.charCodeAt(0)); raw[0] ^= 1;
  m.data = btoa(String.fromCharCode(...raw)); m.crc = bkCrc(raw);
  ok(throwsMsg(() => readArchive(evil), /Object 14: not a multi object/), "file: a good CRC over a bad multi object is still refused (content check)");
  const retired = clone(file); retired.objects.push({ id: 2, size: 4, crc: 1, data: "AAAAAA==" });
  ok(throwsMsg(() => readArchive(retired), /retired|checksum/), "file: data in a retired object is refused");
  const fel = { format: "felucca-backup", version: 1, firmware: "FELUCCA 1.6.1", created: "x", objects: [] };
  ok(throwsMsg(() => readArchive(fel), /Felucca backup/), "file: a Felucca backup is refused (other id meanings)");
}

// ---- restore
const rows = (u) => DUELL_IDS.map((id) => `${id}:${u.objs.has(id) ? bkCrc(u.objs.get(id)) : 0}`).join() + "|" + patchRows(u);
const KEYS = [1000, 1001, 1005, 1064];
{
  const B = new MockUnit({ seed: false });                       // a unit that lost its data
  const rq = (r, o) => B.request(r, o);
  const list = await listUnit(rq, 63);
  const plan = planRestore(archive, list, 63, all);
  ok(same(plan.steps.map((s) => s.key ?? s.id), [...KEYS, ...RESTORE_ORDER]) && plan.excluded.length === 0 && plan.skipped.length === 0 && plan.steps.slice(0, 4).every((s) => s.id === 40 && s.slot === patchSlot(s.key) && s.size >= 20 && s.group === "patches"),
     "plan: all groups, order patches (by slot, id 40 with the slot) -> multis -> patterns -> wavetables -> settings -> current music last");
  const wire = []; const prog = [];
  const r = await runRestore(async (q, o) => { if (q[0] === 67) wire.push(q[1].slice(0, 2).join() + (q[1][0] === 0 && q[1][1] === 40 ? ":" + q[1][12] + ":" + q[1].length : "")); return rq(q, o); }, plan, (dn, t) => prog.push(dn / t));
  ok(r.failed === null && r.written.length === 20 && prog.at(-1) === 1, "restore: all 20 objects written (4 patches, 16 others), progress reaches 100 %");
  ok(wire.filter((w) => w.startsWith("0,40")).join() === "0,40:0:13,0,40:1:13,0,40:5:13,0,40:64:13" && wire.indexOf("2,40") < wire.indexOf("0,13") && wire.filter((w) => w === "2,40").length === 4,
     "restore: each patch is begin (13 bytes: 0, 40, size, crc, slot) / data / commit [2, 40]; the slot byte is sent at begin; patches before the multis");
  const v = await verifyRestore(rq, plan);
  ok(v.differ.length === 0 && v.same.length === 18 && same(v.same.slice(0, 4), KEYS) && same(v.notCompared, [1, 0]), "verify: patches, multis, patterns equal the file (size, CRC, from a fresh list with the pages); settings and music accepted by the unit's check");
  ok(DUELL_IDS.every((id) => B.objs.has(id) === (A.objs.get(id)?.length > 0)) && DUELL_IDS.every((id) => (B.objs.get(id) || nothing).length === (A.objs.get(id) || nothing).length) && patchRows(B) === patchRows(A), "restore: the unit now holds exactly the backed-up objects and patch records");
  const w = B.log.filter((x) => x.startsWith("write ")).map((x) => (x.startsWith("write patch") ? 1000 + +x.slice(12) : +x.slice(6)));
  ok(same(w.slice(0, 4), KEYS) && w.at(-1) === 0 && w.at(-2) === 1 && w.indexOf(21) < w.indexOf(1) && w.indexOf(14) < w.indexOf(21) && w.indexOf(1064) < w.indexOf(13), "restore: the writes the unit made are in that order");
  const again = planRestore(archive, await listUnit(rq, 63), 63, new Set(["patches"]));
  ok(again.steps.length === 0 && again.skipped.join() === KEYS.join(), "plan: patches the unit already holds (same size and CRC) are skipped, not written again");
}
{   // a restore over a unit with other data: the objects the file holds empty become empty (an erased multi stays erased); extra patches on the unit are removed
  const C = new MockUnit(); C.objs.set(15, buildMultiObject(["OLD ONE", null])); C.objs.set(22, buildPatternObject(1, { 0: { name: "OLDPAT" } }));
  C.setRecord(9, buildPatchRecord({ name: "MINE ONLY" })); C.setRecord(100, buildPatchRecord({ name: "ALSO MINE" })); C.setRecord(5, buildPatchRecord({ name: "OTHER BELL", engine: 3 })); C.rebuild();
  const rq = (r, o) => C.request(r, o);
  const plan = planRestore(archive, await listUnit(rq, 63), 63, all);
  const puts = plan.steps.filter((s) => s.key !== undefined);
  ok(same(puts.map((s) => s.slot), [5, 9, 100]) && same(puts.map((s) => s.size > 0), [true, false, false]) && plan.skipped.join() === "1000,1001,1064" && puts.filter((s) => !s.size).every((s) => s.id === 40 && s.crc === 0 && s.bytes.length === 0),
     "plan: one PUT for the patch that differs (slot 5), one PUT of length 0 for each slot the unit has saved and the file has not (9, 100); the identical ones (0, 1, 64) are skipped");
  const r = await runRestore(rq, plan);
  const v = await verifyRestore(rq, plan);
  ok(r.failed === null && !C.objs.has(15) && bkCrc(C.objs.get(22)) === bkCrc(A.objs.get(22)) && v.differ.length === 0 && patchRows(C) === patchRows(A) && !C.patches.has(9) && !C.patches.has(100) && C.log.includes("remove patch 9") && C.log.includes("remove patch 100"),
     "restore: multi objects empty in the file are emptied on the unit (size 0 PUT), the pattern object (never empty) is the file's, patches the file lacks are removed, the unit's saved patches end up exactly the file's, verify equal");
  const none = readArchive(JSON.stringify(objectsToFile([...archive.objects.values()].filter((o) => !isPatchKey(o.id)), info, { patchRecords: true })));
  const pn = planRestore(none, await listUnit(rq, 63), 63, new Set(["patches"]));
  ok(none.hasPatches && pn.steps.length === 4 && pn.steps.every((s) => !s.size), "plan: a file of a unit with no saved patch (patchRecords true) removes every patch of the unit");
}
{   // group selection and honest exclusions
  const D = new MockUnit({ seed: false });
  const rq = (r, o) => D.request(r, o);
  const list = await listUnit(rq, 63);
  const p1 = planRestore(archive, list, 63, new Set(["patches"]));
  ok(same(p1.steps.map((s) => s.key), KEYS), "plan: only the chosen group (patches) is written");
  const p2 = planRestore(archive, list, 3, all);
  ok(p2.excluded.map((e) => e.key).join() === "patches,multis,patterns,wavetables" && p2.steps.every((s) => s.id < 13) && p2.excluded[0].reason === "this unit's firmware cannot restore patches one at a time (update it first)" && /update/.test(p2.excluded[1].reason),
     "plan: a unit without caps bits 2, 3 and 5: patches (\"cannot restore patches one at a time (update it first)\"), multis, patterns and wavetables left out, with the reason");
  const p2c = planRestore(archive, list, 31, all);
  ok(p2c.excluded.map((e) => e.key).join() === "patches" && p2c.steps.every((s) => s.key === undefined) && p2c.steps.length === 16, "plan: a unit with caps 31 (everything but bit 5): only the patches are left out");
  const old6 = JSON.parse(JSON.stringify({ ...file, version: 3, patchRecords: undefined, objects: file.objects.filter((o) => o.id !== 40).concat(PATCH_IDS_6.map((id) => ({ id, size: 0, crc: 0, data: "" }))) }));
  const o6 = readArchive(JSON.stringify(old6));
  ok(o6.version === 3 && !o6.hasPatches && o6.oldPatchChunks === 0 && planRestore(o6, list, 63, new Set(["patches"])).excluded[0].reason === "not in this file", "file: a version 3 file with empty chunks has no patches (not in this file)");
  ok(throwsMsg(() => readArchive(JSON.stringify({ ...old6, objects: [...old6.objects, { id: 27, size: 0, crc: 0, data: "" }] })), /version 3 file does not have|damaged/), "file: a version 3 file that holds the ten more chunks is refused");
  const p2b = planRestore(archive, list, 55, all);
  ok(p2b.excluded.map((e) => e.key).join() === "wavetables" && p2b.steps.every((s) => s.id < 24 || s.id > 26) && /update/.test(p2b.excluded[0].reason), "plan: a unit with caps 55 (no wavetables): only the wavetables left out");
  const p3 = planRestore(archive, list, null, all);
  ok(p3.excluded.length === 4, "plan: a unit that reported no caps is treated as not able");
  const odd = readArchive(file); odd.objects.get(1).size = 600;
  ok(planRestore(odd, list, 63, new Set(["settings"])).excluded[0]?.reason.includes("another DUELL version"), "plan: a settings object of another size than the unit's is left out");
  ok(GROUPS[0].key === "patches" && GROUPS[0].dynamic && GROUPS[0].caps === 32 && GROUPS.slice(1).every((g) => !g.dynamic && g.ids.length > 0), "groups: patches is dynamic (caps bit 5), the others keep their ids");
}
{   // errors while writing: reported, the object aborted, nothing after it written, what was written is named
  const E = new MockUnit({ seed: false }); E.fail = { id: 14, op: 1, rc: 4 };
  const rq = (r, o) => E.request(r, o);
  const plan = planRestore(archive, await listUnit(rq, 63), 63, all);
  const r = await runRestore(rq, plan);
  ok(r.failed?.id === 14 && /flash/.test(r.failed.message) && r.written.join() === "1000,1001,1005,1064,13" && !E.objs.has(14) && bkCrc(E.objs.get(0)) === bkCrc(new MockUnit({ seed: false }).objs.get(0)) && E.staged === null, "restore: a flash error in object 14 stops there, the four patches and object 13 are named as written, nothing after");
  const P = new MockUnit({ seed: false }); P.fail = { id: 40, op: 2, rc: 4 };
  const rp = await runRestore((r2, o) => P.request(r2, o), planRestore(archive, await listUnit((r2, o) => P.request(r2, o), 63), 63, all));
  ok(rp.failed?.id === 1000 && rp.failed.rc === 4 && rp.written.length === 0 && P.patches.size === 0 && P.staged === null && /patch #01/.test(objectLabel(rp.failed.id)), "restore: a flash error at the commit of the first patch stops there (rc 4, the failed patch named by its key), nothing written");
  const F = new MockUnit({ seed: false });
  const rq2 = (r, o) => F.request(r, o);
  const plan2 = planRestore(archive, await listUnit(rq2, 63), 63, all);
  F.playing = true;
  const r2 = await runRestore(rq2, plan2);
  ok(r2.failed?.rc === 3 && /Stop playback/.test(r2.failed.message) && r2.written.length === 0 && F.objs.size === 5 && F.patches.size === 0, "restore while the unit plays (rc 3): nothing written, the message says stop playback");
  const G = new MockUnit({ seed: false, caps: 3 });                // the page would not offer it; if the unit refuses at begin it is reported, never skipped
  const plan3 = planRestore(archive, await listUnit((r, o) => G.request(r, o)), 7, new Set(["multis"]));
  const r3 = await runRestore((r, o) => G.request(r, o), plan3);
  ok(r3.failed?.id === 13 && r3.failed.rc === 1 && r3.written.length === 0, "restore: an object the unit refuses at begin (rc 1) is reported as failed, not skipped");
  const Q = new MockUnit({ seed: false, caps: 31 }), pq = planRestore(archive, await listUnit((r, o) => Q.request(r, o)), 63, new Set(["patches"]));
  const rq3 = await runRestore((r, o) => Q.request(r, o), pq);
  ok(rq3.failed?.id === 1000 && rq3.failed.rc === 1 && Q.patches.size === 0, "restore: a patch PUT sent to a unit without caps bit 5 is refused at begin (rc 1) and reported");
  const H = new MockUnit({ seed: false }); H.fail = { id: 21, op: 2, rc: 2 };
  const plan4 = planRestore(archive, await listUnit((r, o) => H.request(r, o), 63), 63, all);
  const r4 = await runRestore((r, o) => H.request(r, o), plan4);
  ok(r4.failed?.id === 21 && r4.written.includes(20) && !r4.written.includes(1) && !r4.written.includes(0) && bkCrc(H.objs.get(0)) === bkCrc(new MockUnit({ seed: false }).objs.get(0)), "restore: the unit refusing a commit (rc 2) stops before the settings and the music");
}
{   // the unit's own rules for PUT 40, over the wire
  const U = new MockUnit({ seed: false }), rq = (r, o) => U.request(r, o), rec = buildPatchRecord({ name: "WIRE" }), crc = bkCrc(rec);
  const begin = (size, c, slot, tail = []) => rq([67, [0, 40, ...bkU32(size), ...bkU32(c), slot, ...tail]]);
  ok((await begin(19, 0, 3))[2] === 1 && (await begin(535, 0, 3))[2] === 1 && (await begin(20, 0, 128))[2] === 1 && (await begin(20, 0, 3, [0]))[2] === 1 && (await rq([67, [0, 40, ...bkU32(20), ...bkU32(0)]]))[2] === 1 && (await begin(rec.length, crc, 3))[2] === 0,
     "PUT 40 begin: a size 1..19 or over 534, a slot over 127 and a request of 12 or 14 bytes answer rc 1; size 20..534 with a slot is accepted");
  await rq([67, [3, 40]]);
  const piece = async (off, n) => rq([67, [1, 40, ...bkU32(off), ...bkPack(rec.subarray(off, off + n))]]);
  await begin(rec.length, crc ^ 1, 3); await piece(0, rec.length);
  ok((await rq([67, [2, 40]]))[2] === 2 && U.patches.size === 0, "PUT 40: a wrong CRC at commit is rc 2, nothing written");
  const junk = rec.slice(); junk[4] &= ~PTF.CB; await begin(junk.length, bkCrc(junk), 3); await rq([67, [1, 40, ...bkU32(0), ...bkPack(junk)]]);
  ok((await rq([67, [2, 40]]))[2] === 2 && U.patches.size === 0, "PUT 40: a record the store would not read (no CB flag) is rc 2 at commit, nothing written");
  await begin(rec.length, crc, 77); await piece(0, rec.length);
  ok((await rq([67, [2, 40]]))[2] === 0 && U.patches.has(77) && same(U.patchBytes(77), rec) && (await rq([67, [2, 40]]))[2] === 5, "PUT 40: a good record goes into the slot of the begin (no slot in its bytes), a second commit is stale (rc 5)");
  U.playing = true;
  ok((await begin(0, 0, 77))[2] === 3, "PUT 40 while playing: rc 3");
  U.playing = false;
  await begin(0, 0, 77);
  ok((await rq([67, [2, 40]]))[2] === 0 && !U.patches.has(77), "PUT 40 of size 0 removes the slot's record (its factory sound is back)");
  await rq([65, []]);
  const get = (slot, off, n, extra = []) => rq([66, [40, ...bkU32(off), n & 127, n >> 7, slot, ...extra]]);
  U.setRecord(4, rec);
  await rq([65, []]);
  const g1 = await get(4, 0, rec.length);
  ok(g1[0] === 40 && g1[1] === 0 && g1.length === 9 + Math.ceil(rec.length / 7) * 8 - (7 - rec.length % 7) % 7 && (await get(6, 0, 10))[1] === 1 && (await get(200, 0, 10))[1] === 1 && (await get(4, 0, 0))[1] === 1 && (await get(4, 5, rec.length))[1] === 1
     && (await get(4, 0, 10, [0]))[1] === 1 && (await rq([66, [40, ...bkU32(0), 10, 0]]))[1] === 1 && (await get(4, 0, 257))[1] === 1,
     "GET 40: a saved slot answers (the reply has no slot); a slot with no record, a slot over 127, count 0, a count past the end, 10 bytes and 8 bytes requests answer rc 1");
  ok((await rq([66, [6, ...bkU32(0), 10, 0]]))[1] === 1 && (await rq([66, [27, ...bkU32(0), 10, 0]]))[1] === 1 && (await rq([67, [0, 6, ...bkU32(0), ...bkU32(0)]]))[2] === 1 && (await rq([67, [0, 39, ...bkU32(0), ...bkU32(0)]]))[2] === 1,
     "GET and PUT of the old patch chunk ids (6, 27 ...) answer rc 1 on a unit with caps bit 5");
}
{   // verify notices a difference
  const I = new MockUnit({ seed: false });
  const rq = (r, o) => I.request(r, o);
  const plan = planRestore(archive, await listUnit(rq, 63), 63, all);
  await runRestore(rq, plan);
  I.setRecord(5, buildPatchRecord({ name: "SOMEONE ELSE" })); I.rebuild();
  I.objs.get(14)[10] ^= 1;
  ok((await verifyRestore(rq, plan)).differ.join() === "1005,14", "verify: an object and a patch that differ from the file are named");
  const rm = planRestore(readArchive(JSON.stringify(objectsToFile([...archive.objects.values()].filter((o) => !isPatchKey(o.id)), info, { patchRecords: true }))), await listUnit(rq, 63), 63, new Set(["patches"]));
  await runRestore(rq, rm);
  I.setRecord(9, buildPatchRecord({ name: "BACK AGAIN" })); I.rebuild();
  ok((await verifyRestore(rq, rm)).differ.join() === "" && (await verifyRestore(rq, { ...rm, steps: [{ ...rm.steps[0], slot: 9, key: 1009 }] })).differ.join() === "1009", "verify: a removed patch is checked to be gone");
}
{   // the safety backup: a capture of the unit as it is, readable and restorable
  const J = new MockUnit();
  J.setRecord(9, buildPatchRecord({ name: "MINE ONLY" })); J.rebuild(); J.objs.delete(14);   // (a unit with other data than the file)
  const safety = await captureBackup((r, o) => J.request(r, o), info);
  const before = rows(J);
  await runRestore((r, o) => J.request(r, o), planRestore(archive, await listUnit((r, o) => J.request(r, o), 63), 63, all));
  ok(rows(J) !== before && !J.patches.has(9), "safety: the restore changed the unit");
  await runRestore((r, o) => J.request(r, o), planRestore(readArchive(safety), await listUnit((r, o) => J.request(r, o), 63), 63, all));
  ok(rows(J).split(",").slice(2).join() === before.split(",").slice(2).join() && J.patches.has(9), "safety: restoring the safety backup brings back patches, multis and patterns exactly");
}
{   // the whole path of a record with the extension block and 32 edits: build -> PATCH_RAW write -> read back -> backup -> restore onto a fresh unit -> bytes equal
  const { rawWrite, rawRead } = await import("./editor/librarian.js");
  const { Unit } = await import("./editor/unit.js"), { MockLink } = await import("./editor/mockunit.js");
  const ext = XB_DEFS.map((d, k) => d - 64 + 2), edits = Array.from({ length: 32 }, (_, i) => ({ wid: i < 26 ? i * 4 : 128 + (i - 26), val: i * 4 - 60 }));
  const rec = buildRecord({ engine: 0, name: "XB + 32 EDITS", cat: 4, values: storeDefaults(0).map((v, i) => (i === 60 ? v + 2 : v)), mb: [1, 2, 3, 4, 5, 6, 7], ext, edits });
  ok((rec[4] & PTF.XB) && rec[20] === 101 && parseRecord(rec).edits.length === 32, "xblock path: the record has the block (102 bytes: L 101) and 32 edits");
  const src = new MockUnit({ seed: false }), u = new Unit(new MockLink(src)); await u.connect();
  await rawWrite(u, 9, rec);
  const back = await rawRead(u, 9);
  ok(same(back, rec) && same(src.patchBytes(9), rec), "xblock path: PATCH_RAW write on the unit and read back: the bytes are equal");
  const f = await captureBackup((r, o) => src.request(r, o), info), a = readArchive(JSON.stringify(f));
  ok(same(a.objects.get(patchKey(9)).bytes, rec) && a.objects.size === 17 && f.objects.some((o) => o.id === 40 && o.slot === 9 && o.size === rec.length), "xblock path: the backup holds the record bytes under {id: 40, slot: 9}");
  const dst = new MockUnit({ seed: false }), rd = (r, o) => dst.request(r, o);
  const run = await runRestore(rd, planRestore(a, await listUnit(rd, 63), 63, all));
  const v = await verifyRestore(rd, planRestore(a, await listUnit(rd, 63), 63, all));
  ok(run.failed === null && same(dst.patchBytes(9), rec) && v.differ.length === 0 && parseRecord(dst.patchBytes(9)).edits.length === 32, "xblock path: restored onto a fresh unit the bytes are equal, 32 edits and all, verify equal");
  const dst2 = new MockUnit({ seed: false }), u2 = new Unit(new MockLink(dst2)); await u2.connect();
  ok(same(await rawRead(u2, 9) ?? new Uint8Array(0), new Uint8Array(0)), "xblock path: (the fresh unit held nothing there before)");
  const mid = new MockUnit({ seed: false }), rm = (r, o) => mid.request(r, o);
  await runRestore(rm, planRestore(a, await listUnit(rm, 63), 63, all));
  const u3 = new Unit(new MockLink(mid)); await u3.connect();
  ok(same(await rawRead(u3, 9), rec), "xblock path: the restored record reads back through PATCH_RAW too");
  const damaged = rec.slice(); damaged[20]++;
  ok(await athrows(() => rawWrite(u, 10, damaged)) && !src.patches.has(10), "xblock path: PATCH_RAW refuses a record whose block length is off by one at commit (nothing written)");
  ok(checkObject(patchKey(9), damaged) !== null && throwsMsg(() => readArchive(JSON.stringify(objectsToFile([...a.objects.values()].map((o) => (o.id === patchKey(9) ? { id: o.id, bytes: damaged } : o)), info))), /Patch #10/), "xblock path: a record whose block length is off by one is refused when a file is read (Patch #10)");
}

// ---- older firmware and older files
{
  const K = new MockUnit({ caps: 3 });                           // 0.2.13: 16 ids (six of them patch chunks), no multis / patterns
  const f13 = await captureBackup((r, o) => K.request(r, o), parseInfo(await K.request([1, []])));
  const a13 = readArchive(f13);
  ok(f13.objects.length === 2 && f13.version === 5 && !a13.objects.has(13) && !a13.hasPatches && !describe(bytesOf(a13)).has.multis && !describe(bytesOf(a13)).has.patches && /cannot back up patches/.test(a13.notes[0]),
     "0.2.13 unit: the backup holds settings and music only (no patches: it cannot give them one at a time, and the file says so), and the preview says no multis");
  const L = new MockUnit();
  L.setRecord(9, buildPatchRecord({ name: "NEWER ONE" })); L.rebuild();
  const m14 = bkCrc(L.objs.get(14)), p21 = bkCrc(L.objs.get(21)), pr = patchRows(L);
  const plan = planRestore(a13, await listUnit((r2, o) => L.request(r2, o), 63), 63, all);
  const r = await runRestore((r2, o) => L.request(r2, o), plan);
  ok(r.failed === null && plan.excluded.map((e) => e.key).join() === "patches,multis,patterns,wavetables" && plan.excluded.every((e) => e.reason === "not in this file") && same(r.written, [1, 0]) && bkCrc(L.objs.get(14)) === m14 && bkCrc(L.objs.get(21)) === p21 && patchRows(L) === pr,
     "a 0.2.13 file onto a new unit: settings and music restored; patches, multis and patterns left as they are (a file without patch records never empties the unit's patches)");
}
{   // a version 1 .. 4 file with physical patch chunks: read, the chunks kept out, the rest restorable
  const chunks = [buildLegacyPatchChunk([{ slot: 0, name: "OLD ONE" }, { slot: 1, name: "OLD TWO" }]), buildLegacyPatchChunk([{ slot: 9, name: "OLD NINE" }])];
  const mk = (version, ids) => ({ ...clone(file), version, patchRecords: undefined, objects: file.objects.filter((o) => o.id !== 40).concat(ids.map((id, i) => {
    const b = i < 2 ? chunks[i] : nothing; return { id, size: b.length, crc: b.length ? bkCrc(b) : 0, data: btoa(String.fromCharCode(...b)) }; })) });
  for (const [version, ids] of [[4, OLD_PATCH_IDS], [3, PATCH_IDS_6]]) {
    const ar = readArchive(JSON.stringify(mk(version, ids)));
    ok(ar.version === version && !ar.hasPatches && ar.oldPatchChunks === 2 && ![...ar.objects.keys()].some((k) => isPatchKey(k) || OLD_PATCH_IDS.includes(k)) && ar.objects.size === 16 && /older firmware \(physical chunks\) are in this file but cannot be restored/.test(ar.notes[0]) && describe(bytesOf(ar), [], ar.oldPatchChunks).notes.length === 1 && describe(bytesOf(ar)).patches.length === 0,
       `v${version} file with physical patch chunks: read; its patches are kept out; the description says they cannot be restored`);
    const U = new MockUnit({ seed: false }), rq = (r, o) => U.request(r, o);
    const pl = planRestore(ar, await listUnit(rq, 63), 63, all);
    ok(pl.excluded.length === 1 && pl.excluded[0].key === "patches" && /older firmware \(physical chunks\).*cannot be restored/.test(pl.excluded[0].reason) && same(pl.steps.map((s) => s.id), RESTORE_ORDER), `v${version} file: the patches are excluded with the reason, everything else restores in order`);
    const run = await runRestore(rq, pl), v = await verifyRestore(rq, pl);
    ok(run.failed === null && v.differ.length === 0 && U.patches.size === 0 && bkCrc(U.objs.get(14)) === bkCrc(A.objs.get(14)), `v${version} file: the rest restores and verifies, the unit's patches are untouched`);
  }
  ok(throwsMsg(() => readArchive(JSON.stringify({ ...clone(file), objects: [...clone(file).objects, { id: 6, size: 0, crc: 0, data: "" }] })), /version 5.*patch chunks.*damaged/), "file: a version 5 file with a physical patch chunk (id 6) is refused");
  ok(throwsMsg(() => readArchive(JSON.stringify({ ...clone(file), objects: [...clone(file).objects, { id: 39, size: 0, crc: 0, data: "" }] })), /patch chunks/) && throwsMsg(() => readArchive(JSON.stringify({ ...mk(4, OLD_PATCH_IDS), objects: [...mk(4, OLD_PATCH_IDS).objects, file.objects.at(-1)] })), /version 4.*no patch records/), "file: a version 5 file with chunk 39 is refused, and so is a version 4 file with a patch record (id 40)");
}
{   // a version 5 file: every refusal about the patches comes before any write
  const bad = (f) => { const c = clone(file); f(c); return c; }, P = (c, slot = 0) => c.objects.find((o) => o.id === 40 && o.slot === slot);
  const rewrite = (c, f, slot = 0) => { const o = P(c, slot), raw = Uint8Array.from(atob(o.data), (x) => x.charCodeAt(0)); f(raw); o.data = btoa(String.fromCharCode(...raw)); o.crc = bkCrc(raw); o.size = raw.length; };
  ok(readArchive(JSON.stringify(file)).objects.size === 20, "file v5: the good file reads (20 objects)");
  ok(throwsMsg(() => readArchive(bad((c) => { c.objects.push(clone(P(c, 1))); P(c, 1).slot = 1; c.objects.at(-1).slot = 1; })), /Patch #02 is in the file twice/), "file v5: a slot listed twice is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { P(c).slot = 128; })), /valid slot/) && throwsMsg(() => readArchive(bad((c) => { delete P(c).slot; })), /valid slot/) && throwsMsg(() => readArchive(bad((c) => { P(c).slot = "1"; })), /valid slot/), "file v5: a patch without a slot, with a slot over 127 or not a number is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { P(c).crc = (P(c).crc ^ 1) >>> 0; })), /Patch #01 failed its checksum/) && throwsMsg(() => readArchive(bad((c) => { P(c).data = P(c).data.slice(0, -4); })), /Patch #01 is damaged/), "file v5: a patch with a wrong CRC or a truncated one is refused, naming the patch");
  ok(throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[2] = 5; }))), /Patch #01: .*slot and revision/) && throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[3] = 1; }))), /Patch #01: .*slot and revision/), "file v5: a good CRC over a record with a slot or revision byte is refused (the records of a backup have both 0)");
  ok(throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[0]++; }))), /Patch #01: damaged patch record/) && throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[4] &= ~PTF.CB; }))), /damaged patch record/) && throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[7] = 7; }))), /damaged patch record/)
     && throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[5] = 99; }))), /damaged patch record/), "file v5: a record with a wrong length field, no CB flag, a control character in the name or an unknown engine is refused");
  ok(throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { /* too short */ }, 0) || (P(c).size = 0, P(c).data = "", P(c).crc = 0))), /Patch #01/) && throwsMsg(() => readArchive(bad((c) => rewrite(c, () => {}, 0) || (() => { const r = new Uint8Array(535); r[0] = 535 & 255; r[1] = 535 >> 8; P(c).data = btoa(String.fromCharCode(...r)); P(c).size = 535; P(c).crc = bkCrc(r); })())), /Patch #01/),
     "file v5: a patch of size 0 or of 535 bytes is refused (only saved patches, 20 to 534 bytes)");
  const xb = readArchive(JSON.stringify(file));
  ok(throwsMsg(() => readArchive(bad((c) => rewrite(c, (r) => { r[4] |= PTF.XB; r[0] += 0; }))), /Patch #01/), "file v5: the extension flag with no block behind it is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { c.version = 5; c.objects.find((o) => o.id === 40).id = 41; })), /Unknown object id 41/), "file v5: an object id 41 is unknown");
  ok(readArchive(JSON.stringify({ ...clone(file), objects: clone(file).objects.filter((o) => o.id !== 40) })).hasPatches === true && readArchive(JSON.stringify({ ...clone(file), patchRecords: undefined, objects: clone(file).objects.filter((o) => o.id !== 40) })).hasPatches === false, "file v5: patchRecords says a file without patches is the file of a unit with none; without the flag it leaves a unit's patches alone");
  ok(xb.objects.get(1000).size >= 20 && !xb.problems.length, "file v5: (no raw objects in the good file)");
}
{   // the installer's file of a DUELL unit (felucca-backup v1, 16 objects) is read, its patch chunks kept out
  const M = new MockUnit({ caps: 3 });
  const v1 = await installerCapture((r, o) => M.request(r, o), "FELUCCA DUELL 0.2.13 bet");
  const a1 = readArchive(v1);
  ok(a1.version === 1 && a1.source === "felucca-backup v1" && a1.objects.size === 2 && a1.oldPatchChunks === 2 && !a1.hasPatches, "v1 file (the installer's, 16 objects of a DUELL unit) is read: music and settings; the two patch chunks that hold patches are kept out, retired ones dropped");
  const N = new MockUnit({ seed: false });
  const plan = planRestore(a1, await listUnit((r, o) => N.request(r, o), 63), 63, all);
  ok(plan.excluded.map((e) => e.key).join() === "patches,multis,patterns,wavetables" && plan.excluded[0].reason.includes("older firmware") && plan.excluded.slice(1).every((e) => e.reason === "not in this file"), "v1 file: the patches are 'older firmware', multis, patterns and wavetables are 'not in this file'");
  const r = await runRestore((r2, o) => N.request(r2, o), plan);
  ok(r.failed === null && same(r.written, [1, 0]), "v1 file: restored in order (settings, music)");
  const felucca = clone(v1); felucca.firmware = "FELUCCA 1.6.1";
  ok(throwsMsg(() => readArchive(felucca), /Felucca backup/), "v1 file of a Felucca unit (firmware without DUELL) is refused");
}
{   // the installer's own backup flow against every generation of unit (the mock unit answers as the firmware does)
  const html = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  const archiveInfo = new Function("parseInfo", html.match(/function archiveInfo\(a\) \{[\s\S]*?\n\}\n/)[0] + "return archiveInfo;")(parseInfoInst);   // the page's own function
  ok(BACKUP_IDS_FULL.length === 27 && BACKUP_IDS_WT.length === 30 && BACKUP_IDS_P16.length === 40 && same(BACKUP_IDS_P16.filter((id) => !PATCH_IDS_X.includes(id)).slice(26, 29), [26, 32, 33]), "installer: manifests of 27 (0.2.14), 30 (wavetables) and 40 ids (0.2.15)");
  for (const [caps, n, objs] of [[3, 16, 2], [7, 27, 13], [15, 30, 16], [31, 40, 16]]) {
    const U = new MockUnit({ caps }), link = (r, o) => U.request(r, o);
    if (caps & 16) { U.objs.set(29, buildLegacyPatchChunk([{ slot: 100, name: "CHUNK 29" }])); U.objs.set(38, buildLegacyPatchChunk([{ slot: 121, name: "CHUNK 38" }])); }   // (chunks past the seed)
    const info = archiveInfo(await U.request([1, []]));
    ok(info.supported && info.caps === caps, `installer: archiveInfo accepts a unit with song rows 0 and caps ${caps}`);
    const inst = await installerCapture(link, info.version);
    ok(readBackup(inst).objects.length === n && inst.objects.length === n, `installer caps ${caps}: capture lists ${n} objects and the file reads`);
    const ar = readArchive(inst);
    ok(ar.objects.size === objs && ar.source === "felucca-backup v1", `installer caps ${caps}: the editor's Restore reads the file (${objs} DUELL objects)`);
    ok(!ar.hasPatches && [...ar.objects.keys()].every((k) => !OLD_PATCH_IDS.includes(k) && !isPatchKey(k)) && ar.oldPatchChunks >= 2 && ar.notes.length === 1, `installer caps ${caps}: the patch chunks of the file (${ar.oldPatchChunks} hold patches) are kept out of the patches (never restored by the editor)`);
    // the installer's own restore onto a fresh unit of the same kind, then the editor's verify
    const T = new MockUnit({ caps, seed: false });
    const log = []; const tlink = async (r, o) => { log.push(r[0]); return T.request(r, o); };
    await restoreInst(tlink, inst);
    ok(!log.some((c) => c >= 11 && c <= 13) && log.filter((c) => c === 14).length === 3, `installer caps ${caps}: its restore touches only the three sample slots 32..34 (erase), never ids 35..39 (patch chunks)`);
    const planned = planRestore(ar, await listUnit((r, o) => T.request(r, o)), caps, all);
    const v = await verifyRestore((r, o) => T.request(r, o), { steps: [...ar.objects.values()].filter((o) => o.id > 1).map((o) => ({ id: o.id, size: o.size, crc: o.crc })) });
    ok(v.differ.length === 0 && (caps === 3 || v.same.length > 0) && OLD_PATCH_IDS.filter((id) => U.has(id)).every((id) => same(T.objs.get(id) || nothing, U.objs.get(id) || nothing)), `installer caps ${caps}: after the installer's restore every object equals the file, its patch chunks too (the editor's verify: ${v.same.length} same)`);
    // the editor's Restore of the installer's file
    const E = new MockUnit({ caps, seed: false });
    const run = await runRestore((r, o) => E.request(r, o), planned.steps.length ? planRestore(ar, await listUnit((r, o) => E.request(r, o)), caps, all) : planned);
    const v2 = await verifyRestore((r, o) => E.request(r, o), planRestore(ar, await listUnit((r, o) => E.request(r, o)), caps, all));
    ok(run.failed === null && v2.differ.length === 0, `installer caps ${caps}: the editor's Restore of the installer's file succeeds and verifies`);
  }
  {   // a 40-object file onto older firmware (the installer's restore skips what that unit refuses) and a 16-object file onto a new unit (the ten more chunks are emptied)
    const N = new MockUnit({ caps: 31 }), file = await installerCapture((r, o) => N.request(r, o), "FELUCCA DUELL 0.2.15 bet");
    const O = new MockUnit({ caps: 3, seed: false });
    await restoreInst((r, o) => O.request(r, o), file);
    ok(bkCrc(O.objs.get(6)) === bkCrc(N.objs.get(6)) && !O.objs.has(27), "installer: a 40-object file onto 0.2.13 firmware restores what it has and skips the rest");
    const U3 = new MockUnit({ caps: 3 }), old = await installerCapture((r, o) => U3.request(r, o), "FELUCCA DUELL 0.2.13 bet");
    const X = new MockUnit({ caps: 31 }); X.objs.set(28, buildLegacyPatchChunk([{ slot: 9, name: "STALE" }]));
    await restoreInst((r, o) => X.request(r, o), old);
    ok(!X.objs.get(28)?.length, "installer: a 16-object file onto a new unit empties the ten more chunks (no record outlives the restore)");
  }
}
{   // what the installer's generic backup (fm1backup.js, out of scope here) still assumes: the manifests of 16 .. 40 ids with the patch chunks. A unit with caps bit 5 lists 24 ids.
  const N = new MockUnit();
  ok(await athrows(() => installerCapture((r, o) => N.request(r, o), "FELUCCA DUELL 0.2.26 bet"), /manifest/), "installer (still the old chunk model): a unit with caps 63 lists 24 ids, which no installer manifest has: its capture refuses (the Backup page is the way to back it up)");
}
ok(/^DUELL-backup-\d{4}-\d\d-\d\d-\d{4}\.json$/.test(fileName("DUELL-backup")), "file name: DUELL-backup-YYYY-MM-DD-HHMM.json");

console.log(fails ? `DUELL BACKUP WEB TESTS FAILED (${fails})` : "duell backup web tests passed");
process.exit(fails ? 1 : 0);
