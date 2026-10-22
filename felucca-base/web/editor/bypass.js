// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: BYPASS toggles (0.2.25, owner: "bypass for everything"). Editor-only actions on EXISTING parameters: a toggle sets a section's
// switch (or its level) to the value that takes the section out of the sound and REMEMBERS the values it had; the second press puts them back.
// DRY sets every section of both parts and the master stages at once and remembers all of it. Nothing new is sent: SET / TRACK_PARAM / MENU_SET only.
// A section counts as bypassed while every one of its values is at its "off" value; turning a bypassed control by hand ends the bypass
// (its memory is dropped: the hand-set value wins). No DOM, no I/O: get(where, id) / set(where, id, v) are handed in (where: 0 / 1 a part, "m" a MENU setting).
// What has no existing switch (so no toggle here): the chorus and reverb buses themselves (only the sends feed them, so the toggles of the
// CHORUS / DELAY / REVERB cards zero both parts' sends to that bus), the multimode filter (its bypass is CUT fully open with MODE LOW-PASS: a sound setting,
// not an effect), the arpeggiator / chord (not effects). The unit's own MODULES switches (SENDS, PART FX) bypass more at once but are device settings.
export const PART_SECTIONS = {
  sat: { label: "SATURATION", items: [[33, 0]] },
  insert: { label: "INSERT", items: [[96, 0]] },
  eq: { label: "EQUALIZER", items: [[8, 0], [88, 0], [103, 0]] },   // (0.2.25 layout round: no EQ switch; the three gains LOW MID HIGH to 0 dB: a flat EQ is not run)
  duck: { label: "DUCKING", items: [[111, 3]] },          // (DUCK SOURCE 3 = OFF)
  sends: { label: "SENDS", items: [[34, 0], [35, 0], [36, 0]] },
};
export const BUS_SECTIONS = {
  chorus: { label: "CHORUS", send: 34 }, delay: { label: "DELAY", send: 35 }, reverb: { label: "REVERB", send: 36 },
};
export const MASTER_SECTIONS = {                          // MENU setting ids (editor_menu.c): BASS MONO OFF, WIDTH 100 % = as it is; RATIO OFF (the stored 1:1)
  stereo: { label: "STEREO MASTER", items: [[33, 0], [42, 100]] }, comp: { label: "COMPRESSOR", items: [[35, 0]] },   // (0.2.25 layout round: one card each, in the order of the sound)
};
// the (where, id, off) list of a key: "p0:sat", "bus:delay", "m:comp", "dry"
export function itemsOf(key) {
  if (key === "dry") return [...[0, 1].flatMap((p) => Object.keys(PART_SECTIONS).flatMap((k) => itemsOf(`p${p}:${k}`))), ...Object.keys(MASTER_SECTIONS).flatMap((k) => itemsOf(`m:${k}`))];
  const [a, b] = key.split(":");
  if (a === "p0" || a === "p1") return (PART_SECTIONS[b]?.items ?? []).map(([id, off]) => ({ where: +a[1], id, off }));
  if (a === "bus") return BUS_SECTIONS[b] ? [0, 1].map((p) => ({ where: p, id: BUS_SECTIONS[b].send, off: 0 })) : [];
  if (a === "m") return (MASTER_SECTIONS[b]?.items ?? []).map(([id, off]) => ({ where: "m", id, off }));
  return [];
}
export class Bypass {
  constructor(get, set) { this.get = get; this.set = set; this.mem = new Map(); }
  // bypassed now: every value at its off value and a memory to go back to (or, without a memory, simply off: "off" with nothing to restore)
  state(key) {
    const it = itemsOf(key), off = it.length > 0 && it.every((x) => this.get(x.where, x.id) === x.off);
    if (!off && this.mem.has(key)) this.mem.delete(key);   // a value was turned by hand: that is the user's now
    return { bypassed: off && this.mem.has(key), off, canRestore: this.mem.has(key) };
  }
  // returns the undo items [{where, id, oldV, newV}] of what was written
  async toggle(key) {
    const st = this.state(key), it = itemsOf(key), out = [];
    if (st.bypassed) {
      const saved = this.mem.get(key); this.mem.delete(key);
      for (const s of saved) { const now = this.get(s.where, s.id); if (now !== s.v) { await this.set(s.where, s.id, s.v); out.push({ where: s.where, id: s.id, oldV: now, newV: s.v }); } }
      if (key === "dry") for (const k of [...this.mem.keys()]) this.state(k);   // (a section's own memory stays only while it is still off)
      return out;
    }
    this.mem.set(key, it.map((x) => ({ where: x.where, id: x.id, v: this.get(x.where, x.id) })));
    for (const x of it) { const now = this.get(x.where, x.id); if (now !== x.off) { await this.set(x.where, x.id, x.off); out.push({ where: x.where, id: x.id, oldV: now, newV: x.off }); } }
    return out;
  }
}
