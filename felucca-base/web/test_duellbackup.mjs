// SPDX-License-Identifier: GPL-3.0-only
// The Backup page's logic (editor/duellbackup.js) against a simulated unit (editor/mockunit.js): INFO, the "duell-backup" file round trip,
// every refusal before a write, the restore order and what is reported, the safety backup, the verify, older firmware and older files.
import { PATCH_IDS_6, PATCH_IDS_X, GROUPS, DUELL_IDS, RESTORE_ORDER, parseInfo, parseList, readArchive, objectsToFile, captureBackup, planRestore, runRestore,
         verifyRestore, describe, checkObject, listUnit, fileName } from "./editor/duellbackup.js";
import { MockUnit, buildPatchChunk, buildMultiObject, buildPatternObject, MOCK_ENGINES } from "./editor/mockunit.js";
import { buildObject as wtObject, WT } from "./editor/wavetable.js";
import { readFileSync } from "node:fs";
import { bkCrc, BACKUP_IDS_FULL, BACKUP_IDS_WT, BACKUP_IDS_P16, bkManifest, bkU32, readBackup, captureBackup as installerCapture, restoreBackup as restoreInst, parseInfo as parseInfoInst } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(100)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const athrows = async (f, re) => { try { await f(); return false; } catch (e) { return re ? re.test(e.message) : true; } };
const throwsMsg = (f, re) => { try { f(); return false; } catch (e) { return re.test(e.message); } };
const all = new Set(GROUPS.map((g) => g.key));
const clone = (x) => JSON.parse(JSON.stringify(x));
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);

// ---- INFO and the list
{
  const u = new MockUnit();
  const info = parseInfo(await u.request([1, []]));
  ok(info.version === "FELUCCA DUELL 0.2.20 bet" && info.engines.length === MOCK_ENGINES.length && info.caps === 31 && info.patchCap === 128, "INFO: version, engine names, backup caps 31 (bit 2: multis and patterns, bit 3: user wavetables, bit 4: 16 patch chunks) and the patch capacity 128");
  ok(parseInfo(await new MockUnit({ caps: 3 }).request([1, []])).caps === 3, "INFO: a unit without bit 2 reports caps 3");
  const list = await listUnit((r, o) => u.request(r, o));
  ok(same([...list.keys()].sort((a, b) => a - b), DUELL_IDS) && list.get(14).size > 8 && list.get(2) === undefined, "LIST: the 32 DUELL objects (the retired ids 2..5 8 32..34 are left out)");
  ok(list.get(25).size === 1228 && !list.has(24) === false && list.get(24).size === 0, "LIST: the user wavetables 24..26: USR2 holds the 1228 byte object, USR1 and USR3 are empty (length 0)");
  const c7 = await listUnit((r, o) => new MockUnit({ caps: 7 }).request(r, o));
  ok(c7.size === 19 && !c7.has(24) && !c7.has(27), "LIST of a unit with caps 7 (0.2.14 before the wavetables): 19 objects, no wavetables");
  const old = await listUnit((r, o) => new MockUnit({ caps: 3 }).request(r, o));
  ok(old.size === 8 && !old.has(13) && !old.has(21), "LIST of a 0.2.13 unit (16 ids): no multis, no patterns");
  ok(throwsMsg(() => parseList([1, 3, 0]), /playing/), "LIST while playing (rc 3): says to stop playback");
  ok(throwsMsg(() => parseList([1, 0, 1, 99, ...bkU32(0), ...bkU32(0)]), /newer editor/), "LIST: an unknown object id is refused");
}

// ---- checks of single objects
{
  const mu = buildMultiObject(["A", "B"]), pa = buildPatternObject([{ name: "X" }, null, null, null, null, null]), pc = buildPatchChunk([{ slot: 3, name: "P" }]);
  ok(checkObject(14, mu) === null && checkObject(21, pa) === null && checkObject(6, pc) === null && checkObject(14, new Uint8Array(0)) === null, "objects: good multi, pattern, patch chunk and empty objects pass");
  const t = (id, b, f) => { const c = b.slice(); f(c); return checkObject(id, c) !== null; };
  ok(t(14, mu, (c) => { c[0] ^= 1; }) && t(14, mu, (c) => { c[4]++; }) && t(14, mu, (c) => { c[8] = 9; }) && t(14, mu, (c) => { c[8 + 8] = 7; }) && t(14, mu, (c) => { c[9] = 3; }),
     "objects: a multi with a bad magic, length, version, name character or part count is refused");
  ok(t(21, pa, (c) => { c[0] ^= 1; }) && t(21, pa, (c) => { c[8 + 2] = 0; }) && t(21, pa, (c) => { c[8 + 8] = 3; }) && t(21, pa, (c) => { c[8] = 1; }) &&
     t(21, pa, (c) => { c[8 + 20 + 4] = 128; }) && t(21, pa, (c) => { c[8 + 5] = 0; }) && checkObject(21, pa.slice(0, 3000)) !== null,
     "objects: a pattern with a bad magic, LEN, name, used marker, step byte, GATE or size is refused");
  ok(checkObject(23, (() => { const c = buildPatternObject([null, null, null, null, { name: "Z" }, null]); return c; })()) !== null, "objects: a used pattern slot past P16 is refused");
  ok(t(6, pc, (c) => { c[1] = 0; }) && t(6, pc, (c) => { c[4] = 19; }) && t(6, pc, (c) => { c[6] = 200; }), "objects: a patch chunk with a bad magic, record length or slot is refused");
  ok(checkObject(0, new Uint8Array(3840)) !== null && checkObject(1, new Uint8Array(584)) !== null && checkObject(2, new Uint8Array(0)) !== null, "objects: music / settings without their magic, and a retired id, are refused");
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
ok(file.format === "duell-backup" && file.version === 4 && file.firmware === info.version && file.objects.length === 32 && !Number.isNaN(Date.parse(file.created)), "file: format duell-backup, version 4, firmware, date, 32 objects");
ok(seen.at(-1)[0] === seen.at(-1)[1] && seen.length > 10, "capture: progress ends at the total");
ok(DUELL_IDS.every((id) => same(archive.objects.get(id).bytes, A.objs.get(id) || new Uint8Array(0))), "round trip: capture -> JSON -> bytes equal the unit's objects, empty ones as 0");
const d = describe(new Map([...archive.objects].map(([id, o]) => [id, o.bytes])), archive.engines);
ok(d.patches.map((p) => `${p.slot}:${p.name}:${p.engineName}:${p.category}`).join() === "0:WARM PAD:ANALOG:PAD,1:SUB BASS:ANALOG:BASS,5:BELL:FM6:KEYS,64:WAVE LEAD:WAVE:LEAD", "preview: patch names, engines and categories by slot");
ok(d.multis.map((m) => `${m.label}:${m.name}`).join() === "M03:TESTONE,M04:TESTTWO,M12:LIVE SET", "preview: multi names (M03 M04 in object 14, M12 in object 18)");
ok(d.patterns.map((p) => `${p.label}:${p.name}:${p.length}`).join() === "P03:PAT THREE:12", "preview: pattern slot names and lengths");
ok(await athrows(() => captureBackup((r, o) => { const x = new MockUnit(); x.playing = true; return x.request(r, o); }, info), /playing/), "capture while the unit plays: refused with a plain message");
{
  const u = new MockUnit(); let n = 0;
  ok(await athrows(() => captureBackup(async (r, o) => { const a = await u.request(r, o); if (r[0] === 66 && ++n === 3) u.objs.get(0)[3000] ^= 1; return a; }, info), /changed/), "capture: a unit that changes during the read fails the backup");
}

// ---- reading files: every refusal comes before any write
{
  const bad = (f) => { const c = clone(file); f(c); return c; };
  ok(throwsMsg(() => readArchive("{ nope"), /JSON/), "file: not JSON");
  ok(throwsMsg(() => readArchive({ format: "something", objects: [] }), /not a DUELL/), "file: another format");
  ok(throwsMsg(() => readArchive(bad((c) => { c.version = 5; })), /newer editor/), "file: a newer version is refused with a plain message");
  ok(throwsMsg(() => readArchive(bad((c) => { c.version = 1; })), /unknown version/), "file: version 1 of the duell-backup format does not exist");
  ok(throwsMsg(() => readArchive(bad((c) => { const o = c.objects.find((x) => x.id === 6); o.crc = (o.crc ^ 1) >>> 0; })), /checksum/), "file: a damaged object (CRC) is refused");
  ok(throwsMsg(() => readArchive(bad((c) => { const o = c.objects.find((x) => x.id === 6); o.data = o.data.slice(0, -4); })), /damaged/), "file: a truncated object is refused");
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
const rows = (u) => DUELL_IDS.map((id) => `${id}:${u.objs.has(id) ? bkCrc(u.objs.get(id)) : 0}`).join();
{
  const B = new MockUnit({ seed: false });                       // a unit that lost its data
  const rq = (r, o) => B.request(r, o);
  const list = await listUnit(rq);
  const plan = planRestore(archive, list, 31, all);
  ok(same(plan.steps.map((s) => s.id), RESTORE_ORDER) && plan.excluded.length === 0, "plan: all groups, order patches -> multis -> patterns -> wavetables -> settings -> current music last");
  const prog = [];
  const r = await runRestore(rq, plan, (dn, t) => prog.push(dn / t));
  ok(r.failed === null && r.written.length === 32 && prog.at(-1) === 1, "restore: all 32 objects written, progress reaches 100 %");
  const v = await verifyRestore(rq, plan);
  ok(v.differ.length === 0 && v.same.length === 30 && same(v.notCompared, [1, 0]), "verify: patches, multis, patterns equal the file (size, CRC); settings and music accepted by the unit's check");
  ok(DUELL_IDS.every((id) => B.objs.has(id) === (A.objs.get(id)?.length > 0)) && DUELL_IDS.every((id) => (B.objs.get(id) || new Uint8Array(0)).length === (A.objs.get(id) || new Uint8Array(0)).length), "restore: the unit now holds exactly the backed-up objects");
  const w = B.log.filter((x) => x.startsWith("write ")).map((x) => +x.slice(6));
  ok(w.at(-1) === 0 && w.at(-2) === 1 && w.indexOf(21) < w.indexOf(1) && w.indexOf(14) < w.indexOf(21), "restore: the writes the unit made are in that order");
}
{   // a restore over a unit with other data: the objects the file holds empty become empty (an erased multi stays erased)
  const C = new MockUnit(); C.objs.set(15, buildMultiObject(["OLD ONE", null])); C.objs.set(22, buildPatternObject([{ name: "OLDPAT" }, null, null, null, null, null]));
  const rq = (r, o) => C.request(r, o);
  const plan = planRestore(archive, await listUnit(rq), 31, all);
  const r = await runRestore(rq, plan);
  const v = await verifyRestore(rq, plan);
  ok(r.failed === null && !C.objs.has(15) && !C.objs.has(22) && v.differ.length === 0, "restore: objects empty in the file are emptied on the unit (size 0 PUT), verify equal");
}
{   // group selection and honest exclusions
  const D = new MockUnit({ seed: false });
  const rq = (r, o) => D.request(r, o);
  const list = await listUnit(rq);
  const p1 = planRestore(archive, list, 31, new Set(["patches"]));
  ok(same(p1.steps.map((s) => s.id), [...PATCH_IDS_6, ...PATCH_IDS_X]), "plan: only the chosen group (patches) is written");
  const p2 = planRestore(archive, list, 3, all);
  ok(p2.excluded.map((e) => e.key).join() === "patches,multis,patterns,wavetables" && p2.steps.every((s) => s.id < 13) && /update/.test(p2.excluded[0].reason), "plan: a unit without caps bits 2, 3 and 4: patches (this file holds more than six chunks), multis, patterns and wavetables left out, with the reason (update the firmware)");
  const small = readArchive(file); for (const id of PATCH_IDS_X) small.objects.set(id, { id, size: 0, crc: 0, bytes: new Uint8Array(0) });
  const p2s = planRestore(small, list, 3, new Set(["patches"]));
  ok(p2s.excluded.length === 0 && same(p2s.steps.map((s) => s.id), PATCH_IDS_6), "plan: a file whose ten more chunks are empty restores onto a unit with six chunks (only the first six are written)");
  const old6 = readArchive(JSON.stringify({ ...file, version: 3, objects: file.objects.filter((o) => !PATCH_IDS_X.includes(o.id)) }));
  const p2o = planRestore(old6, list, 31, new Set(["patches"]));
  ok(old6.version === 3 && same(p2o.steps.map((s) => s.id), [...PATCH_IDS_6, ...PATCH_IDS_X]) && p2o.steps.filter((s) => PATCH_IDS_X.includes(s.id)).every((s) => s.size === 0), "plan: a version 3 file (six chunks) onto a unit with sixteen: the ten more chunks are written empty, so no old patch stays there");
  ok(throwsMsg(() => readArchive(JSON.stringify({ ...file, version: 3 })), /version 3 file does not have|damaged/), "file: a version 3 file that holds the ten more chunks is refused");
  const p2b = planRestore(archive, list, 23, all);
  ok(p2b.excluded.map((e) => e.key).join() === "wavetables" && p2b.steps.every((s) => s.id < 24 || s.id > 26) && /update/.test(p2b.excluded[0].reason), "plan: a unit with caps 7 (before the wavetables): only the wavetables left out");
  const p3 = planRestore(archive, list, null, all);
  ok(p3.excluded.length === 4, "plan: a unit that reported no caps is treated as not able");
  const odd = readArchive(file); odd.objects.get(1).size = 600;
  ok(planRestore(odd, list, 31, new Set(["settings"])).excluded[0]?.reason.includes("another DUELL version"), "plan: a settings object of another size than the unit's is left out");
}
{   // errors while writing: reported, the object aborted, nothing after it written, what was written is named
  const E = new MockUnit({ seed: false }); E.fail = { id: 14, op: 1, rc: 4 };
  const rq = (r, o) => E.request(r, o);
  const plan = planRestore(archive, await listUnit(rq), 31, all);
  const r = await runRestore(rq, plan);
  ok(r.failed?.id === 14 && /flash/.test(r.failed.message) && r.written.join() === "6,7,9,10,11,12,27,28,29,30,31,35,36,37,38,39,13" && !E.objs.has(14) && bkCrc(E.objs.get(0)) === bkCrc(new MockUnit({ seed: false }).objs.get(0)) && E.staged === null, "restore: a flash error in object 14 stops there, the sixteen patch chunks and object 13 are named as written, nothing after");
  const F = new MockUnit({ seed: false });
  const rq2 = (r, o) => F.request(r, o);
  const plan2 = planRestore(archive, await listUnit(rq2), 31, all);
  F.playing = true;
  const r2 = await runRestore(rq2, plan2);
  ok(r2.failed?.rc === 3 && /Stop playback/.test(r2.failed.message) && r2.written.length === 0 && F.objs.size === 2, "restore while the unit plays (rc 3): nothing written, the message says stop playback");
  const G = new MockUnit({ seed: false, caps: 3 });                // the page would not offer it; if the unit refuses at begin it is reported, never skipped
  const plan3 = planRestore(archive, await listUnit((r, o) => G.request(r, o)), 7, new Set(["multis"]));
  const r3 = await runRestore((r, o) => G.request(r, o), plan3);
  ok(r3.failed?.id === 13 && r3.failed.rc === 1 && r3.written.length === 0, "restore: an object the unit refuses at begin (rc 1) is reported as failed, not skipped");
  const H = new MockUnit({ seed: false }); H.fail = { id: 21, op: 2, rc: 2 };
  const plan4 = planRestore(archive, await listUnit((r, o) => H.request(r, o)), 31, all);
  const r4 = await runRestore((r, o) => H.request(r, o), plan4);
  ok(r4.failed?.id === 21 && r4.written.includes(20) && !r4.written.includes(1) && !r4.written.includes(0) && bkCrc(H.objs.get(0)) === bkCrc(new MockUnit({ seed: false }).objs.get(0)), "restore: the unit refusing a commit (rc 2) stops before the settings and the music");
}
{   // verify notices a difference
  const I = new MockUnit({ seed: false });
  const rq = (r, o) => I.request(r, o);
  const plan = planRestore(archive, await listUnit(rq), 31, all);
  await runRestore(rq, plan);
  I.objs.get(14)[10] ^= 1;
  ok((await verifyRestore(rq, plan)).differ.join() === "14", "verify: an object that differs from the file is named");
}
{   // the safety backup: a capture of the unit as it is, readable and restorable
  const J = new MockUnit();
  J.objs.set(6, buildPatchChunk([{ slot: 9, name: "MINE ONLY" }])); J.objs.delete(14);   // (a unit with other data than the file)
  const safety = await captureBackup((r, o) => J.request(r, o), info);
  const before = rows(J);
  await runRestore((r, o) => J.request(r, o), planRestore(archive, await listUnit((r, o) => J.request(r, o)), 31, all));
  ok(rows(J) !== before, "safety: the restore changed the unit");
  await runRestore((r, o) => J.request(r, o), planRestore(readArchive(safety), await listUnit((r, o) => J.request(r, o)), 31, all));
  ok(rows(J).split(",").slice(2).join() === before.split(",").slice(2).join(), "safety: restoring the safety backup brings back patches, multis and patterns exactly");
}

// ---- older firmware and older files
{
  const K = new MockUnit({ caps: 3 });                           // 0.2.13: 16 ids, no multis / patterns
  const f13 = await captureBackup((r, o) => K.request(r, o), parseInfo(await K.request([1, []])));
  const a13 = readArchive(f13);
  ok(f13.objects.length === 8 && !a13.objects.has(13) && !describe(new Map([...a13.objects].map(([i, o]) => [i, o.bytes]))).has.multis, "0.2.13 unit: the backup holds patches, settings and music only, and the preview says no multis");
  const L = new MockUnit();
  L.objs.set(6, buildPatchChunk([{ slot: 9, name: "NEWER ONE" }]));
  const m14 = bkCrc(L.objs.get(14)), p21 = bkCrc(L.objs.get(21));
  const r = await runRestore((r2, o) => L.request(r2, o), planRestore(a13, await listUnit((r2, o) => L.request(r2, o)), 31, all));
  ok(r.failed === null && bkCrc(L.objs.get(6)) === bkCrc(K.objs.get(6)) && bkCrc(L.objs.get(14)) === m14 && bkCrc(L.objs.get(21)) === p21,
     "a 0.2.13 file onto a 0.2.14 unit: patches, settings and music restored, multis and patterns left as they are");
}
{   // the installer's file of a DUELL unit (felucca-backup v1, 16 objects) restores the same way
  const M = new MockUnit({ caps: 3 });
  const v1 = await installerCapture((r, o) => M.request(r, o), "FELUCCA DUELL 0.2.13 bet");
  const a1 = readArchive(v1);
  ok(a1.version === 1 && a1.source === "felucca-backup v1" && a1.objects.size === 8, "v1 file (the installer's, 16 objects of a DUELL unit) is read: 8 DUELL objects, retired ones dropped");
  const N = new MockUnit({ seed: false });
  const plan = planRestore(a1, await listUnit((r, o) => N.request(r, o)), 31, all);
  ok(plan.excluded.map((e) => e.key).join() === "multis,patterns,wavetables" && plan.excluded.every((e) => e.reason === "not in this file"), "v1 file: multis, patterns and wavetables are 'not in this file'");
  const r = await runRestore((r2, o) => N.request(r2, o), plan);
  ok(r.failed === null && same(r.written, [...PATCH_IDS_6, ...PATCH_IDS_X, 1, 0]), "v1 file: restored in order (patches: the ten chunks the file lacks written empty, settings, music)");
  const felucca = clone(v1); felucca.firmware = "FELUCCA 1.6.1";
  ok(throwsMsg(() => readArchive(felucca), /Felucca backup/), "v1 file of a Felucca unit (firmware without DUELL) is refused");
}
{   // the installer's own backup flow against every generation of unit (the mock unit answers as the firmware does)
  const html = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  const archiveInfo = new Function("parseInfo", html.match(/function archiveInfo\(a\) \{[\s\S]*?\n\}\n/)[0] + "return archiveInfo;")(parseInfoInst);   // the page's own function
  ok(BACKUP_IDS_FULL.length === 27 && BACKUP_IDS_WT.length === 30 && BACKUP_IDS_P16.length === 40 && same(BACKUP_IDS_P16.filter((id) => !PATCH_IDS_X.includes(id)).slice(26, 29), [26, 32, 33]), "installer: manifests of 27 (0.2.14), 30 (wavetables) and 40 ids (0.2.15)");
  for (const [caps, n, objs] of [[3, 16, 8], [7, 27, 19], [15, 30, 22], [31, 40, 32]]) {
    const U = new MockUnit({ caps }), link = (r, o) => U.request(r, o);
    if (caps & 16) { U.objs.set(29, buildPatchChunk([{ slot: 100, name: "CHUNK 29" }])); U.objs.set(38, buildPatchChunk([{ slot: 121, name: "CHUNK 38" }])); }   // (chunks past the seed)
    const info = archiveInfo(await U.request([1, []]));
    ok(info.supported && info.caps === caps, `installer: archiveInfo accepts a unit with song rows 0 and caps ${caps}`);
    const inst = await installerCapture(link, info.version);
    ok(readBackup(inst).objects.length === n && inst.objects.length === n, `installer caps ${caps}: capture lists ${n} objects and the file reads`);
    const ar = readArchive(inst);
    ok(ar.objects.size === objs && ar.source === "felucca-backup v1", `installer caps ${caps}: the editor's Restore reads the file (${objs} DUELL objects)`);
    if (caps & 16) ok(PATCH_IDS_X.some((id) => id > 34 && ar.objects.get(id).size > 0) && PATCH_IDS_X.some((id) => id < 32 && ar.objects.get(id).size > 0), `installer caps ${caps}: the ten more patch chunks (27..31, 35..39) hold patches, not read as samples`);
    // the installer's own restore onto a fresh unit of the same kind, then the editor's verify
    const T = new MockUnit({ caps, seed: false });
    const log = []; const tlink = async (r, o) => { log.push(r[0]); return T.request(r, o); };
    await restoreInst(tlink, inst);
    ok(!log.some((c) => c >= 11 && c <= 13) && log.filter((c) => c === 14).length === 3, `installer caps ${caps}: its restore touches only the three sample slots 32..34 (erase), never ids 35..39 (patch chunks)`);
    const planned = planRestore(ar, await listUnit((r, o) => T.request(r, o)), caps, all);
    const v = await verifyRestore((r, o) => T.request(r, o), { steps: [...ar.objects.values()].filter((o) => o.id > 1).map((o) => ({ id: o.id, size: o.size, crc: o.crc })) });
    ok(v.differ.length === 0 && v.same.length > 0, `installer caps ${caps}: after the installer's restore every object equals the file (the editor's verify: ${v.same.length} same)`);
    // the editor's Restore of the installer's file
    const E = new MockUnit({ caps, seed: false });
    const run = await runRestore((r, o) => E.request(r, o), planned.steps.length ? planRestore(ar, await listUnit((r, o) => E.request(r, o)), caps, all) : planned);
    const v2 = await verifyRestore((r, o) => E.request(r, o), planRestore(ar, await listUnit((r, o) => E.request(r, o)), caps, all));
    ok(run.failed === null && v2.differ.length === 0, `installer caps ${caps}: the editor's Restore of the installer's file succeeds and verifies`);
  }
  {   // a 40-object file onto older firmware (the installer's restore skips what that unit refuses) and a 16-object file onto a new unit (the ten more chunks are emptied)
    const N = new MockUnit(), file = await installerCapture((r, o) => N.request(r, o), "FELUCCA DUELL 0.2.15 bet");
    const O = new MockUnit({ caps: 3, seed: false });
    await restoreInst((r, o) => O.request(r, o), file);
    ok(bkCrc(O.objs.get(6)) === bkCrc(N.objs.get(6)) && !O.objs.has(27), "installer: a 40-object file onto 0.2.13 firmware restores what it has and skips the rest");
    const U3 = new MockUnit({ caps: 3 }), old = await installerCapture((r, o) => U3.request(r, o), "FELUCCA DUELL 0.2.13 bet");
    const X = new MockUnit(); X.objs.set(28, buildPatchChunk([{ slot: 9, name: "STALE" }]));
    await restoreInst((r, o) => X.request(r, o), old);
    ok(!X.objs.get(28)?.length, "installer: a 16-object file onto a new unit empties the ten more chunks (no record outlives the restore)");
  }
}
ok(/^DUELL-backup-\d{4}-\d\d-\d\d-\d{4}\.json$/.test(fileName("DUELL-backup")), "file name: DUELL-backup-YYYY-MM-DD-HHMM.json");

console.log(fails ? `DUELL BACKUP WEB TESTS FAILED (${fails})` : "duell backup web tests passed");
process.exit(fails ? 1 : 0);
