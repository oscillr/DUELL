// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// fm1backup.js against a simulated device: 7-bit packing, CRC, manifest checks, a full capture,
// validation before the first write, restore order (live music last) and abort on a failed chunk.
import { BACKUP_IDS, BACKUP_IDS_FULL, BACKUP_IDS_WT, BACKUP_IDS_P16, isSampleId, parseInfo, BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc, bkManifest, readBackup, captureBackup, restoreBackup } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(72)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const throws = (f) => { try { f(); return false; } catch { return true; } };
const athrows = async (f) => { try { await f(); return false; } catch { return true; } };

const rnd = (n, seed) => Uint8Array.from({ length: n }, (_, i) => (i * 131 + seed * 17 + (i >> 3)) & 255);
ok(bkR32(bkU32(0xdeadbeef)) === 0xdeadbeef && bkCrc(new TextEncoder().encode("123456789")) === 0xcbf43926, "backup: numbers and CRC-32");
const b = rnd(1000, 1);
ok(bkUnpack(bkPack(b), b.length).every((v, i) => v === b[i]), "backup: 7-bit pack / unpack round trip");
ok(throws(() => bkUnpack([...bkPack(b.subarray(0, 14)), 0], 14)), "backup: trailing bytes refused");

/* a device: objects by id, sample slots, the order of writes */
function device(objs, opt = {}) {
  const d = { objs: new Map(objs), log: [], staged: null };
  d.request = async ([cmd, a]) => {
    if (cmd === BACKUP_CMD.LIST) {
      const out = [1, 0, BACKUP_IDS.length];
      for (const id of BACKUP_IDS) { const v = d.objs.get(id) || new Uint8Array(0); out.push(id, ...bkU32(v.length), ...bkU32(v.length ? bkCrc(v) : 0)); }
      return out;
    }
    if (cmd === BACKUP_CMD.GET) {
      const id = a[0], off = bkR32(a, 1), n = a[6] | a[7] << 7, v = d.objs.get(id);
      if (opt.changeOnGet === id && off === 0) v[0] ^= 1;
      return [id, 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(v.subarray(off, off + n))];
    }
    if (cmd === BACKUP_CMD.PUT) {
      const [op, id] = a;
      if (op === 0 && id > (opt.maxId ?? 9)) { d.log.push(`refused ${id}`); return [op, id, 1]; }   // (an older firmware)
      if (op === 0) { d.staged = { id, size: bkR32(a, 2), crc: bkR32(a, 7), bytes: [] }; return [op, id, 0]; }
      if (op === 1) {
        if (opt.failChunk === id) return [op, id, 2];
        const off = bkR32(a, 2), p = a.slice(7), n = Math.min(256, d.staged.size - off);
        d.staged.bytes.push(...bkUnpack(p, n)); return [op, id, 0];
      }
      if (op === 2) { const s = Uint8Array.from(d.staged.bytes); const rc = bkCrc(s) === d.staged.crc ? 0 : 2; if (!rc) { d.objs.set(id, s); d.log.push(id); } return [op, id, rc]; }
      if (op === 3) { d.log.push(`abort ${id}`); return [op, id, 0]; }
    }
    if (cmd === 11 || cmd === 12 || cmd === 13 || cmd === 14) { if (cmd === 13 || cmd === 14) d.log.push(32 + a[0]); return [a[0], 0]; }
    throw new Error(`unexpected ${cmd}`);
  };
  return d;
}
const objs = [[0, rnd(3388, 2)], [1, rnd(1200, 3)], [2, rnd(3388, 4)], [6, rnd(3080, 5)]];
const dev = device(objs);
const file = await captureBackup(dev.request, "TEST");
ok(file.objects.length === BACKUP_IDS.length && file.objects[0].size === 3388 && file.objects[3].size === 0, "backup: capture lists every object, empty ones as 0");
ok(readBackup(JSON.stringify(file)).objects[2].bytes.every((v, i) => v === objs[2][1][i]), "backup: capture -> file -> bytes round trip");
ok(await athrows(() => captureBackup(device(objs.map(([i, v]) => [i, v.slice()]), { changeOnGet: 2 }).request, "TEST")), "backup: a device that changes during capture fails the capture");

const bad = JSON.parse(JSON.stringify(file)); bad.objects[2].crc ^= 1;
ok(throws(() => readBackup(bad)), "backup: a damaged file is refused");
const noRun = JSON.parse(JSON.stringify(file)); noRun.objects[0] = { ...noRun.objects[0], size: 0, crc: 0, data: "" };
ok(throws(() => readBackup(noRun)), "backup: a file without the current music is refused");
ok(throws(() => bkManifest([1, 0, 3])), "backup: a short manifest is refused");

const target = device([]);
const damaged = JSON.parse(JSON.stringify(file)); damaged.objects[6].crc ^= 1;
ok(await athrows(() => restoreBackup(target.request, damaged)) && target.log.length === 0, "backup: restore validates every byte before the first write");
await restoreBackup(target.request, file);
const order = target.log.filter((x) => typeof x === "number");
ok(order.at(-1) === 0 && order.at(-2) === 1 && order.indexOf(2) < order.indexOf(1), "backup: restore order: projects, banks, samples, settings, live music last");
ok([0, 1, 2, 6].every((id) => target.objs.get(id).every((v, i) => v === objs.find((o) => o[0] === id)[1][i])), "backup: restored objects equal the source");
{   /* the FM6 patch bank (id 8) and the archives of firmware before it (11 objects, no id 8); DUELL's patch store has six chunks (ids 10..12 too) */
  ok(BACKUP_IDS.length === 16 && file.objects.some((o) => o.id === 8) && file.objects.some((o) => o.id === 9) && file.objects.some((o) => o.id === 12),
    "backup: 16 objects: id 8 (the retired FM6 bank, empty), id 9 and 10..12 (the patch store's chunks)");
  const v3 = JSON.parse(JSON.stringify(file)); v3.objects = v3.objects.filter((o) => o.id < 10 || o.id > 12);
  ok(readBackup(v3).objects.length === 13, "backup: an archive of the patch store's first three chunks (13 objects) still reads");
  const v2 = JSON.parse(JSON.stringify(v3)); v2.objects = v2.objects.filter((o) => o.id !== 9);
  ok(readBackup(v2).objects.length === 12, "backup: an archive of 1.0..1.0.2 (12 objects, the bank as id 8) still reads");
  const old = JSON.parse(JSON.stringify(v2)); old.objects = old.objects.filter((o) => o.id !== 8);
  ok(readBackup(old).objects.length === 11, "backup: an archive of the 11 objects before FM6 still reads");
  const odd = JSON.parse(JSON.stringify(file)); odd.objects = odd.objects.filter((o) => o.id !== 33);
  ok(throws(() => readBackup(odd)), "backup: an archive missing another object is refused");
}
{   /* 1.0.3: an old archive with a bank restores its id 8 (after 6, 7); a new archive onto 1.0.2 skips id 9 */
  const bankBytes = rnd(3472, 8), patches = rnd(3728, 9);
  const v2dev = device([...objs, [8, bankBytes]]);
  const v2file = await captureBackup(async ([cmd, a]) => {   // (a 1.0.2 device: 12 objects)
    const r = await v2dev.request([cmd, a]);
    if (cmd !== BACKUP_CMD.LIST) return r;
    const n = r[2], keep = [];
    for (let i = 0; i < n; i++) if (r[3 + i * 11] < 9 || r[3 + i * 11] > 12) keep.push(...r.slice(3 + i * 11, 14 + i * 11));
    return [1, 0, keep.length / 11, ...keep];
  }, "1.0.2");
  const to103 = device([]);
  await restoreBackup(to103.request, v2file);
  const lg = to103.log.filter((x) => typeof x === "number");
  ok(v2file.objects.length === 12 && lg.indexOf(8) > lg.indexOf(7) && lg.indexOf(7) > lg.indexOf(6) &&
     to103.objs.get(8).every((v, i) => v === bankBytes[i]), "backup: a 1.0.2 archive (id 8, the bank) restores, its bank after the user presets");
  const newFile = await captureBackup(device([...objs, [9, patches]]).request, "1.0.3");
  const to102 = device([], { maxId: 8 });
  await restoreBackup(to102.request, newFile);
  ok(to102.log.includes("refused 9") && !to102.objs.has(9) && to102.log.at(-1) === 0,
    "backup: a 1.0.3 archive onto older firmware: id 9 skipped, the rest restored");
  const to103b = device([]);
  await restoreBackup(to103b.request, newFile);
  ok(to103b.objs.get(9).every((v, i) => v === patches[i]), "backup: a 1.0.3 archive restores id 9");
}
{   /* DUELL 0.2.15: 40 ids; 35..39 are patch chunks, only 32..34 are sample slots; 0.2.14: 27 and 30 ids */
  const empty = (ids) => [1, 0, ids.length, ...ids.flatMap((id) => [id, ...bkU32(0), ...bkU32(0)])];
  ok(bkManifest(empty(BACKUP_IDS_P16)).length === 40 && bkManifest(empty(BACKUP_IDS_WT)).length === 30 && bkManifest(empty(BACKUP_IDS_FULL)).length === 27, "backup: the manifests of 40, 30 and 27 ids are accepted");
  ok(throws(() => bkManifest(empty(BACKUP_IDS_P16).map((v, i) => (i === 3 + 38 * 11 ? 99 : v)))), "backup: a manifest with an id out of order is refused");
  ok([32, 33, 34].every(isSampleId) && [31, 35, 39].every((id) => !isSampleId(id)), "backup: only ids 32..34 are sample slots");
  const chunk = rnd(1500, 11), p16 = device([...objs, [38, chunk], [29, rnd(900, 12)]], { maxId: 39 });
  const list40 = async (req) => { const a = await p16.request(req); if (req[0] !== BACKUP_CMD.LIST) return a;
    const out = []; for (let id = 0; id < 40; id++) { const v = p16.objs.get(id) || new Uint8Array(0); out.push(id, ...bkU32(v.length), ...bkU32(v.length ? bkCrc(v) : 0)); } return [1, 0, 40, ...out]; };
  const f40 = await captureBackup(list40, "FELUCCA DUELL 0.2.15 bet");
  ok(f40.objects.length === 40 && f40.objects[38].id === 38 && f40.objects[38].size === 1500 && readBackup(JSON.stringify(f40)).objects[29].size === 900, "backup: a 40-object capture keeps the patch chunks 29 and 38 (38 is not read as a sample)");
  const to40 = device([], { maxId: 39 }); await restoreBackup(to40.request, f40);
  const lg = to40.log.filter((x) => typeof x === "number");
  ok(to40.objs.get(38).every((v, i) => v === chunk[i]) && lg.indexOf(38) >= 0 && lg.at(-1) === 0, "backup: a 40-object restore writes 38 as an object (not a sample), live music last");
  const to16 = device([], { maxId: 9 }); await restoreBackup(to16.request, f40);
  ok(to16.log.includes("refused 38") && to16.log.includes("refused 12") && !to16.objs.has(38) && to16.log.at(-1) === 0, "backup: a 40-object file onto older firmware: the ids it refuses are skipped, the rest restored");
  const info = parseInfo([...Array.from("FELUCCA DUELL 0.2.15 bet", (c) => c.charCodeAt(0)), 0, 1, 127, 127, 16, 0, 69, 78, 71, 0, 4, 0, 0x55, 1, 9, 0x4d, 1, 64, 1, 0x42, 1, 31, 0x58, 1, 0, 1]);
  ok(info.caps === 31 && info.tags.has(0x55) && info.patchCap === 128 && /DUELL/.test(info.version), "backup: INFO with song rows 0 gives its caps and the patch capacity");
}
const failing = device([], { failChunk: 2 });
ok(await athrows(() => restoreBackup(failing.request, file)) && failing.log.includes("abort 2") && !failing.log.includes(0), "backup: a refused chunk aborts that object and stops before the live music");

console.log(fails ? `BACKUP WEB TESTS FAILED (${fails})` : "backup web tests passed");
process.exit(fails ? 1 : 0);
