// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the unit's colour setting (MENU > COLOR, id 0 = the index into the firmware's UI_PALETTES) as CSS variables.
// The palette table is L.PALETTES, written by tests/editordump.c from build/gen/ui_palettes.h, so the colours cannot drift from the unit.
// No DOM here (the tests import it). part: 0 / 1 = a part's column (its own background, panel and accent), 2 = the global column and the page.
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
  const acc = part === 2 ? text : p.r[part * 2];
  const dark = lum(page) < 0.4;
  return {
    "--bg": base, "--fg": text, "--dim": mix(base, text, L.PALETTES.DIM_PCT), "--line": mix(base, line, 55), "--panel": part === 2 ? mix(page, panel, 100) : panel,
    "--acc": acc, "--accfg": inkOn(acc), "--page": page,
    "--warn": dark ? "#ff6b81" : "#b00020", "--ok": dark ? "#62d98a" : "#0a6b2d",
  };
}
export const isDark = (i) => lum(palette(i).bgl) < 0.4;
