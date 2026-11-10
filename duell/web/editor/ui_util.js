// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: small DOM helpers. Text from a unit or a file is only ever set with textContent (never innerHTML).
export function h(tag, attrs, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v == null || v === false) continue;
    if (k === "class") e.className = v;
    else if (k === "text") e.textContent = v;
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else if (k === "dataset") Object.assign(e.dataset, v);
    else if (v === true) e.setAttribute(k, "");
    else e.setAttribute(k, v);
  }
  for (const c of kids.flat()) if (c != null && c !== false) e.append(c.nodeType ? c : document.createTextNode(String(c)));
  return e;
}
export const clear = (e) => { e.replaceChildren(); return e; };

// a modal question; resolves true on OK. Nothing is written to the unit without one for anything that changes what it stores.
export function ask(title, lines, okLabel = "OK", danger = false) {
  return new Promise((resolve) => {
    const dlg = h("dialog", { class: "ask", "aria-label": title });
    const done = (v) => { dlg.close(); dlg.remove(); resolve(v); };
    dlg.append(h("h2", { text: title }), ...[].concat(lines).map((l) => h("p", { text: l })),
      h("div", { class: "row end" }, h("button", { class: "ghost", onclick: () => done(false), text: "Cancel" }), h("button", { class: danger ? "danger" : "", onclick: () => done(true), text: okLabel, autofocus: true })));
    dlg.addEventListener("cancel", (e) => { e.preventDefault(); done(false); });
    document.body.append(dlg); dlg.showModal();
  });
}
// a modal with a text field (names)
export function askText(title, lines, value, okLabel = "OK", max = 12) {
  return new Promise((resolve) => {
    const dlg = h("dialog", { class: "ask", "aria-label": title });
    const input = h("input", { type: "text", maxlength: max, value, "aria-label": "Name", spellcheck: "false" });
    const done = (v) => { dlg.close(); dlg.remove(); resolve(v); };
    const ok = () => { const v = input.value.replace(/[^ -~]/g, "").trim(); if (v) done(v); };
    input.addEventListener("keydown", (e) => { if (e.key === "Enter") ok(); });
    dlg.append(h("h2", { text: title }), ...[].concat(lines).map((l) => h("p", { text: l })), input,
      h("div", { class: "row end" }, h("button", { class: "ghost", onclick: () => done(null), text: "Cancel" }), h("button", { onclick: ok, text: okLabel })));
    dlg.addEventListener("cancel", (e) => { e.preventDefault(); done(null); });
    document.body.append(dlg); dlg.showModal(); input.select();
  });
}
export function notice(title, lines) {
  return new Promise((resolve) => {
    const dlg = h("dialog", { class: "ask wide", "aria-label": title });
    const done = () => { dlg.close(); dlg.remove(); resolve(); };
    dlg.append(h("h2", { text: title }), ...lines, h("div", { class: "row end" }, h("button", { onclick: done, text: "Close", autofocus: true })));
    dlg.addEventListener("cancel", (e) => { e.preventDefault(); done(); });
    document.body.append(dlg); dlg.showModal();
  });
}
