// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: one parameter control (a "cell"): label, value with unit, a slider (numbers), a select or buttons (enums). It re-derives
// everything from the unit state when refresh() runs: the descriptor (labels and ranges follow the engine, the INSERT type, ENV SYNC), the
// value text (formatted by the firmware, layout.json), and the hide rule (owner: a control that does not apply to the current selection is not drawn;
// T0229: it takes no room either, the other controls of its card move up; its reason is in the tooltip of its card's title, app.js card()).
// A control is rebuilt only when its DESCRIPTOR object changes (another engine, INSERT type, ENV SYNC): the source must hand out a stable object for a stable control, or a drag dies
// mid-way (0.2.24: the relabelled depth cards did that; app.js partCell memoises).
import { h } from "./ui_util.js";
import { valueText, isEnum, enumOrder, isBipolar, bipolarZero } from "./layout.js";

export class Cell {
  // src: { desc(): descriptor, value(): number, set(v), inert(): reason|null (null: it applies; a reason: it is not drawn), tip?(d): plain tooltip, whyText?(code): the reason in words,
  //   optLabel?(v), optHidden?(v), optDisabled?(v), groups?: [[heading, [values]]] (a select with group headings, T0229: the matrix lists) }
  // opt: { soft: always drawn (a matrix destination), bare: no label (the matrix rows: the column heads say it), label: an override }
  constructor(src, opt = {}) {
    this.src = src; this.desc = null; this.opt = opt;
    this.el = h("div", { class: "cell" + (opt.bare ? " bare" : "") });
  }
  build(d) {
    this.desc = d; this.extra = null;
    const label = h("span", { class: "lab", text: this.opt.bare ? "" : d.label, ondblclick: () => !this.disabled && this.src.set(d.def) });
    this.labEl = label;
    this.valEl = h("span", { class: "val" }); this.unitEl = h("span", { class: "unit" });
    const right = h("span", { class: "right" }, this.valEl, this.unitEl);
    const top = h("div", { class: "top" }, label, right);
    this.input = null; this.btns = null; this.groupEls = null;
    let ctl;
    // a few short names are buttons; names that would not fit side by side are a select, so no text is ever clipped
    if (isEnum(d) && !this.src.groups && d.names.length <= 3 && enumOrder(d).reduce((n, v) => n + d.names[v - d.min].length, 0) <= 12) {
      this.btns = enumOrder(d).map((v) => h("button", { class: "seg", type: "button", text: d.names[v - d.min], onclick: () => this.src.set(v) }));
      ctl = h("div", { class: "segs", role: "group", "aria-label": d.label }, this.btns);
      this.btnVals = enumOrder(d);
    } else if (isEnum(d)) {
      const opt = (v) => h("option", { value: v, text: this.src.optLabel ? this.src.optLabel(v) : d.names[v - d.min] });
      const vals = enumOrder(d);
      if (this.src.groups) {                              // T0229: group headings (optgroup); a value of the knob in no group goes at the end
        const seen = new Set(), kids = [];
        this.groupEls = [];
        for (const [head, vs] of this.src.groups) {
          const os = vs.filter((v) => vals.includes(v)).map((v) => { seen.add(v); return opt(v); });
          if (!head) kids.push(...os);
          else { const g = h("optgroup", { label: head }, os); this.groupEls.push(g); kids.push(g); }
        }
        kids.push(...vals.filter((v) => !seen.has(v)).map(opt));
        this.input = h("select", { "aria-label": d.label, onchange: (e) => this.src.set(+e.target.value) }, kids);
      } else this.input = h("select", { "aria-label": d.label, onchange: (e) => this.src.set(+e.target.value) }, vals.map(opt));
      ctl = this.input;
    } else {                                              // a number: a slider with its own fill bar under it (0.2.24: a bipolar one fills from the centre, empty at its zero, like the unit's gauge)
      this.input = h("input", { type: "range", min: d.min, max: d.max, step: 1, "aria-label": d.label, oninput: (e) => this.src.set(+e.target.value) });
      this.fill = h("div", { class: "fill" });
      this.bipolar = isBipolar(d);
      ctl = h("div", { class: "slider" + (this.bipolar ? " bipolar" : "") }, h("div", { class: "track" }, this.fill), this.input);
    }
    this.isEnum = isEnum(d);                              // an enum shows its value in its own control
    this.el.replaceChildren(top, ctl);
  }
  refresh() {
    const d = this.src.desc();
    if (d !== this.desc) this.build(d);
    const v = this.src.value(), t = valueText(d, v), why = this.src.inert();
    const sel = this.input?.tagName === "SELECT" ? this.input : null;
    if (this.src.optLabel && sel) {                     // names that follow the sound (the matrix destinations E1..E8, INS A..C)
      t.val = this.src.optLabel(v);
      for (const o of sel.options) o.textContent = this.src.optLabel(+o.value);
    }
    if (this.src.optDisabled && sel) for (const o of sel.options) o.disabled = +o.value !== v && !!this.src.optDisabled(+o.value);   // (0.2.25: an empty USER wavetable cannot be picked)
    if (this.src.optHidden && sel) for (const o of sel.options) { const hd = +o.value !== v && !!this.src.optHidden(+o.value); o.hidden = hd; o.disabled = hd || o.disabled; }   // (0.2.25: a matrix destination the part does not offer: not in the list; the slot's own value stays)
    if (this.groupEls) for (const g of this.groupEls) g.hidden = ![...g.children].some((o) => !o.hidden);   // (T0229: a heading with nothing under it is not shown)
    this.valEl.textContent = this.isEnum ? "" : t.val; this.unitEl.textContent = this.isEnum ? "" : t.unit;
    if (this.input && document.activeElement !== this.input) this.input.value = v;
    if (this.fill) {                                    // the fill: from the left end (a plain number), or from the zero point either way (bipolar)
      const span = d.max - d.min || 1, z = this.bipolar ? bipolarZero(d) : d.min, a = Math.min(v, z), b = Math.max(v, z);
      this.fill.style.left = `${(a - d.min) / span * 100}%`;
      this.fill.style.width = `${(b - a) / span * 100}%`;
    }
    if (sel && !enumOrder(d).includes(v)) {             // a value the knob does not offer (an alias): show it as it is
      if (!this.extra) { this.extra = h("option", { value: v, text: t.val }); sel.append(this.extra); }
      this.extra.value = v; this.extra.textContent = t.val; sel.value = v;
    }
    this.btns?.forEach((b, i) => { b.classList.toggle("on", this.btnVals[i] === v); b.setAttribute("aria-pressed", this.btnVals[i] === v); });
    const soft = !!this.opt.soft;                       // always drawn, even when the route does nothing now (a matrix destination is the selector that changes it)
    this.disabled = !!why && !soft;                     // not drawn: nothing to click, no room taken (editor.css .cell.gone)
    this.el.classList.toggle("gone", this.disabled);
    this.el.hidden = this.disabled;
    this.why = this.disabled ? (this.src.whyText ? this.src.whyText(why) : why) : "";   // (the card's title tooltip lists it, app.js)
    const tip = this.src.tip?.(d) || "";
    this.el.title = this.disabled ? "" : why ? `${d.label}: this destination does nothing now (${this.src.whyText ? this.src.whyText(why) : why})` : `${d.label}${tip ? ": " + tip : ""}\nDouble-click the name: back to the default value.`;
    if (this.input) this.input.disabled = this.disabled;
    this.btns?.forEach((b) => { b.disabled = this.disabled; });
  }
}
