// SPDX-License-Identifier: GPL-3.0-only
// Auto-connect (0.2.20): a browser asks for MIDI with SysEx on a click, once. When the page already has it, the editor connects by itself at load and
// when the unit is plugged in later. No protocol, no request: the Connect button's own code does the connecting (the pages pass it as go).
export async function sysexGranted(nav = globalThis.navigator) {
  try { return (await nav.permissions.query({ name: "midi", sysex: true })).state === "granted"; } catch { return false; }   // no Permissions API / MIDI name: not granted
}
export const HINT = "Press Connect once: the browser asks to allow MIDI with SysEx. After that this page connects by itself.";
const isUnit = (p) => /felucca|duell/i.test(p?.name || "");
// access.onstatechange: a DUELL / FM-1 port that appears while wanted() is true calls go() (once, 600 ms after the last such event: input and output come apart);
// the handler it replaces (the page's "lost" one) still runs first
export function watchUnit(access, { wanted, go, delay = 600, timer = setTimeout, clear = clearTimeout }) {
  const before = access.onstatechange;
  let tm = null;
  access.onstatechange = (e) => {
    before?.(e);
    if (e?.port?.state !== "connected" || !isUnit(e.port) || !wanted()) return;
    clear(tm);
    tm = timer(() => { if (wanted()) go(); }, delay);
  };
}
