// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: one parameter control (a "cell"): label, value with unit, a slider (numbers), a select or buttons (enums). It re-derives
// everything from the unit state when refresh() runs: the descriptor (labels and ranges follow the engine, the INSERT type, ENV SYNC), the
// value text (formatted by the firmware, layout.json), and the hide rule (owner, 2026-10-15: a control that does not apply to the current selection is not shown; its place stays empty, like a card the unit does not draw).
import { h } from "./ui_util.js";
import { valueText, isEnum, enumOrder } from "./layout.js";

export class Cell {
  // src: { desc(): descriptor, value(): number, set(v), inert(): reason|null (null: it applies; a reason: it is hidden), labelOf?(): string }
  constructor(src, opt = {}) {
    this.src = src; this.desc = null; this.opt = opt;
    this.el = h("div", { class: "cell" });
  }
  build(d) {
    this.desc = d; this.extra = null;
    const label = h("span", { class: "lab", title: "Double-click: back to the default value", text: d.label, ondblclick: () => !this.disabled && this.src.set(d.def) });
    this.labEl = label;
    this.valEl = h("span", { class: "val" }); this.unitEl = h("span", { class: "unit" });
    const right = h("span", { class: "right" }, this.valEl, this.unitEl);
    const top = h("div", { class: "top" }, label, right);
    this.input = null; this.btns = null;
    let ctl;
    // a few short names are buttons; names that would not fit side by side (DELAY MODE: DIGITAL PING-PONG RANDOM OCT UP) are a select, so no text is ever clipped
    if (isEnum(d) && d.names.length <= 4 && enumOrder(d).reduce((n, v) => n + d.names[v - d.min].length, 0) <= 15) {
      this.btns = enumOrder(d).map((v) => h("button", { class: "seg", type: "button", text: d.names[v - d.min], onclick: () => this.src.set(v) }));
      ctl = h("div", { class: "segs", role: "group", "aria-label": d.label }, this.btns);
      this.btnVals = enumOrder(d);
    } else if (isEnum(d)) {
      this.input = h("select", { "aria-label": d.label, onchange: (e) => this.src.set(+e.target.value) },
        enumOrder(d).map((v) => h("option", { value: v, text: this.src.optLabel ? this.src.optLabel(v) : d.names[v - d.min] })));
      ctl = this.input;
    } else {
      this.input = h("input", { type: "range", min: d.min, max: d.max, step: 1, "aria-label": d.label, oninput: (e) => this.src.set(+e.target.value) });
      ctl = this.input;
    }
    this.isEnum = isEnum(d);                              // an enum shows its value in its own control
    this.el.replaceChildren(top, ctl);
  }
  refresh() {
    const d = this.src.desc();
    if (d !== this.desc) this.build(d);
    const v = this.src.value(), t = valueText(d, v), why = this.src.inert();
    if (this.src.optLabel && this.input?.tagName === "SELECT") {   // names that follow the sound (the matrix destinations E1..E8, INS A..C)
      t.val = this.src.optLabel(v);
      for (const o of this.input.options) o.textContent = this.src.optLabel(+o.value);
    }
    this.valEl.textContent = this.isEnum ? "" : t.val; this.unitEl.textContent = this.isEnum ? "" : t.unit;
    if (this.input && document.activeElement !== this.input) this.input.value = v;
    if (this.input && this.input.tagName === "SELECT" && !enumOrder(d).includes(v)) {   // a value the knob does not offer (an alias): show it as it is
      if (!this.extra) { this.extra = h("option", { value: v, text: t.val }); this.input.append(this.extra); }
      this.extra.value = v; this.extra.textContent = t.val; this.input.value = v;
    }
    this.btns?.forEach((b, i) => { b.classList.toggle("on", this.btnVals[i] === v); b.setAttribute("aria-pressed", this.btnVals[i] === v); });
    const soft = !!this.opt.soft;                       // always shown, even when the route does nothing now (a matrix destination is the selector that changes it)
    this.disabled = !!why && !soft;                     // hidden: not shown, nothing to click
    this.el.classList.toggle("gone", this.disabled);    // the cell keeps its place (visibility), as the unit leaves the card's column empty
    this.el.setAttribute("aria-hidden", this.disabled ? "true" : "false");
    this.el.title = this.disabled ? "" : why ? `${d.label}: this destination does nothing now (${why})` : `${d.label}, ${d.min}..${d.max}`;
    if (this.input) this.input.disabled = this.disabled;
    this.btns?.forEach((b) => { b.disabled = this.disabled; });
  }
}
