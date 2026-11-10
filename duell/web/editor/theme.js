// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the unit's colour setting (MENU > COLOR, id 0 = the index into the firmware's UI_PALETTES) as CSS variables.
// The palette table is L.PALETTES, written by tests/editordump.c from build/gen/ui_palettes.h, so the colours cannot drift from the unit.
// No DOM here (the tests import it). part: 0 / 1 = a part's column (its own background, panel and accent), 2 = the global column and the page (0.2.24: its accent is the palette's GLOBAL colour g, as the unit's global pages).
import { L } from "./layout.js";

const rgb = (h) => [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16));
const hex = (c) => "#" + c.map((v) => Math.round(Math.min(255, Math.max(0, v))).toString(16).padStart(2, "0")).join("");
export const mix = (a, b, pct) => { const x = rgb(a), y = rgb(b); return hex(x.map((v, i) => v + (y[i] - v) * pct / 100)); };
export const lum = (h) => { const c = rgb(h).map((v) => { v /= 255; return v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }); return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; };
export const inkOn = (h) => (lum(h) > 0.4 ? "#000000" : "#ffffff");

export const palette = (i) => L.PALETTES.LIST[i] || L.PALETTES.LIST[L.PALETTES.DEFAULT];
export const paletteName = (i) => palette(i).name;

// the variables of one column. bg = what the cards and fields inside it use (the firmware's elevated surface), panel = the column itself
export function themeVars(i, part) {
  const p = palette(i), both = (a) => mix(a[0], a[1], 50), pick = (a) => (part === 2 ? both(a) : a[part]);
  const page = p.bgl, panel = pick(p.surf), card = pick(p.raise), line = pick(p.line), text = p.text;
  const base = part === 2 ? page : card;
  const acc = part === 2 ? (p.g || text) : p.r[part * 2];   // 0.2.24: the global column in the unit's GLOBAL colour (a third colour, apart from both parts')
  const dark = lum(page) < 0.4;
  return {
    "--bg": base, "--fg": text, "--dim": mix(base, text, L.PALETTES.DIM_PCT), "--line": mix(base, line, 55), "--panel": part === 2 ? mix(page, panel, 100) : panel,
    "--acc": acc, "--accfg": inkOn(acc), "--page": page,
    "--warn": dark ? "#ff6b81" : "#b00020", "--ok": dark ? "#62d98a" : "#0a6b2d",
  };
}
export const isDark = (i) => lum(palette(i).bgl) < 0.4;

// DUELL 1.1 (owner items 47, 48): the editor's look follows the device design: only the ACTIVE part's colour plus greys. part 0 / 1: a part's column, 2: the global column and the page.
// The part colours are the palette's (VIOLET: part 1 #de38ff, part 2 #ff1c8c; the demo uses the sampled #ff1c8b). The inactive part keeps its own colour with a weaker tint (item 99). A light palette keeps the old themeVars.
export const GREY = "#737073", GREY_TEXT = "#cfcccf";
export function look11Vars(i, part, active, partColours = null) {
  const p = palette(i), col = partColours ?? [p.r[0], p.r[2]];
  if (lum(p.bgl) >= 0.4) return themeVars(i, part);
  if (part === 2) {   // DUELL 1.1 (owner, round 2): the global column and the global pages (GLOBAL, PATCHES, MULTIS, PATTERNS, MIDI, MIDI MAP, WAVETABLES, SETTINGS, the OCTAVE view) take NO part colour: white and greys on black, as the unit's global pages (PAGES_V2_glo-a.png)
    const acc = "#cfcccf", card = "#171719";
    return { "--bg": card, "--fg": p.text, "--dim": "#a09da0", "--line": "#35343a", "--panel": "#0b0b0d", "--acc": acc, "--accfg": "#000000", "--page": "#08080a", "--warn": "#ff6b81", "--ok": "#62d98a", "--base": "#101012" };
  }
  const on = part === active, acc = col[part];   // owner item 99: the inactive part keeps its own colour (only a weaker tint), never grey
  const base = "#101012", card = mix("#18181b", acc, on ? 9 : 6), panel = mix("#0d0d0f", acc, on ? 5 : 3);
  return {
    "--bg": card, "--fg": p.text, "--dim": "#a09da0", "--line": mix(card, acc, on ? 34 : 24), "--panel": part === 2 ? "#0b0b0d" : panel,
    "--acc": acc, "--accfg": inkOn(acc), "--page": "#08080a", "--warn": "#ff6b81", "--ok": "#62d98a", "--base": base,
  };
}
