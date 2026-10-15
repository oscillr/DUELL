// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the piano roll of one part (the PIANO ROLL tab, and the full-width overlay that opens from it). The model, the edit rules and the write queue are
// in roll.js (PatternStore); this file is the picture and the pointer / keyboard handling.
//   x = the steps of the pattern (LEN wide; bar lines every 4 steps, stronger every 16), y = pitch (C1..C7 and wider when the pattern needs it), a keyboard on the left.
//   Mouse: click an empty cell = a note (drag right while pressed: longer), drag a note = move, drag its right edge = longer / shorter, right-click or double-click = delete.
//   Keyboard (the roll has the focus): arrows move the selected note (Shift+Left / Right: length, Shift+Up / Down: octave) or the cursor, Enter adds a note at the cursor,
//   Delete removes, A accent, S slide, + / - velocity, R ratchet, Escape deselects, Ctrl+Z undo, Ctrl+Shift+Z / Ctrl+Y redo.
// There is no playhead: the editor protocol has no command that reports the sequencer's position.
import { h, clear } from "./ui_util.js";
import { noteName, CMD, parseMenu } from "./proto.js";
import { ID } from "./layout.js";
import * as R from "./roll.js";

const KW = 56, EDGE = 7, LANES = [["vel", "VEL", "Velocity of each step"], ["chance", "CHANCE", "Chance of each step, in %"], ["ratchet", "RATCH", "Ratchet of each step, 1 to 4 hits"]];
const SCALE_KEY = "duell-roll-scale";
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

// opts: { store, cell(id, inertFn) -> Cell, wide, compact, onExpand, onClose, say }
export function createRollView(app, p, opts) {
  const u = app.unit, st = opts.store, wide = !!opts.wide, say = opts.say || app.say;
  const rh = wide ? 16 : 14, laneH = opts.compact || window.innerHeight < 800 ? (wide && !opts.compact ? 64 : 48) : wide ? 84 : 64;
  let sel = null, cur = { step: 0, pitch: 60 }, lane = "vel", zoom = 1, cw = 30, hi = 96, lo = 24, destroyed = false, mode = null, sig = "", scrolled = false;
  let root = 0, scale = 0;
  try { const v = (localStorage.getItem(SCALE_KEY) || "").split(","); root = clamp(+v[0] | 0, 0, 11); scale = clamp(+v[1] | 0, 0, R.SCALES.length - 1); } catch { /* the defaults */ }
  const state = () => R.seqState(u, p);
  const maxN = () => (state().arpSeq ? 1 : R.MAXN);
  const len = () => st.len;
  const steps = () => st.steps;

  // ---- the parts of the picture
  const cells = [];
  const inertSeq = (id) => () => (state().arpSeq ? "SEQ ARP" : null);                       // DIV, SWG, GATE: the arp's own RATE / GATE time the steps in the arp's SEQ mode
  const mk = (id, inert) => { const c = opts.cell(id, inert); cells.push(c); return c; };
  const ctl = h("div", { class: "grid rollctl" }, [mk(ID.P_SLEN), mk(ID.P_SDIV, inertSeq(ID.P_SDIV)), mk(ID.P_SSWING, inertSeq(ID.P_SSWING)), mk(ID.P_SGATE, inertSeq(ID.P_SGATE))].map((c) => c.el));
  const btn = (text, title, fn, cls = "ghost small") => h("button", { type: "button", class: cls, text, title, onclick: fn });
  const bUndo = btn("Undo", "Undo the last edit (Ctrl+Z)", () => doUndo()), bRedo = btn("Redo", "Redo (Ctrl+Shift+Z or Ctrl+Y)", () => doRedo());
  const bCopy = btn("Copy", "Copy the pattern (the first LEN steps); Paste works on either part", () => { const n = st.copy(); say(`Copied ${n} steps of PART ${p + 1}.`, "good"); update(); });
  const bPaste = btn("Paste", "Paste the copied steps over the start of this pattern", () => { if (!st.paste()) say("Nothing copied yet.", "warn"); });
  const bClear = btn("Clear", "Empty the pattern (the first LEN steps); Undo brings it back", () => st.clear());
  const live = h("input", { type: "checkbox", checked: st.live, "aria-label": "Send live", onchange: (e) => { st.setLive(e.target.checked); update(); } });
  const liveLab = h("label", { class: "livebox", title: "On: every edit is written to the unit at once. Off: edits wait until you press Send." }, live, " Send live");
  const bSend = btn("Send", "Write the waiting edits to the unit", () => st.send(), "ghost small");
  const bRevert = btn("Revert", "Throw the waiting edits away and read the pattern from the unit again", () => st.revert(), "ghost small");
  const pend = h("span", { class: "small rollpend" });
  const selRoot = h("select", { "aria-label": "Highlight: root note", title: "Highlight only: the unit's SCALE and ROOT cannot be read by the editor, so choose them here to see them in the roll (nothing is sent)", onchange: () => { root = +selRoot.value; saveScale(); paintRows(); } },
    ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"].map((n, i) => h("option", { value: i, text: n })));
  const selScale = h("select", { "aria-label": "Highlight: scale", title: "Highlight only (see ROOT)", onchange: () => { scale = +selScale.value; saveScale(); paintRows(); } }, R.SCALES.map(([n], i) => h("option", { value: i, text: n })));
  selRoot.value = root; selScale.value = scale;
  const bZoomOut = btn("-", "Zoom out: narrower steps", () => setZoom(zoom / 1.25), "zb"), bZoomIn = btn("+", "Zoom in: wider steps", () => setZoom(zoom * 1.25), "zb");
  const bExpand = opts.onExpand ? btn("ROLL", "Open the piano roll across the whole window (both parts)", () => opts.onExpand(p), "ghost small") : null;
  const bClose = opts.onClose ? btn("Close", "Back to the editor (Esc)", () => opts.onClose(), "ghost small") : null;
  const tools = h("div", { class: "rolltools" }, bExpand, bClose,
    h("span", { class: "tg", role: "group", "aria-label": "Send" }, liveLab, bSend, bRevert, pend),
    h("span", { class: "tg", role: "group", "aria-label": "Edit" }, bUndo, bRedo, bCopy, bPaste, bClear),
    h("span", { class: "tg", role: "group", "aria-label": "Highlight a scale" }, h("span", { class: "small", text: "Scale" }), selRoot, selScale),
    );
  const note = h("p", { class: "note rollnote" });
  const status = h("div", { class: "rollstatus", role: "status", "aria-live": "polite" });

  const ruler = h("div", { class: "rruler" }), rulerIn = h("div", { class: "rulerin" }); ruler.append(rulerIn);
  const keys = h("div", { class: "rkeys", "aria-hidden": "true" }), keysIn = h("div", { class: "keysin" }); keys.append(keysIn);
  const rows = h("div", { class: "rrows" }), vlines = h("div", { class: "vlines" }), nlayer = h("div", { class: "rnotes" }), curEl = h("div", { class: "rcur" }), loadEl = h("div", { class: "rollload", text: "Reading the steps ..." });
  const grid = h("div", { class: "rgrid", tabindex: 0, role: "application", "aria-label": `Piano roll of part ${p + 1}. Arrow keys move the selected note or the cursor, Enter adds a note, Delete removes it.` }, rows, vlines, nlayer, curEl);
  const scroller = h("div", { class: "rscroll" }, grid);
  const laneHead = h("div", { class: "lhead" });
  const laneBtns = LANES.map(([k, t, tip]) => h("button", { type: "button", class: "lbtn", text: t, title: tip, "aria-pressed": String(k === lane), onclick: () => { lane = k; update(); } }));
  const laneSwitch = h("div", { class: "lswitch", style: `height:${laneH}px`, role: "group", "aria-label": "Lane" }, laneBtns);
  const accLab = h("div", { class: "lflag", text: "ACCENT", title: "Accent of each step: velocity 127" }), sldLab = h("div", { class: "lflag", text: "SLIDE", title: "Slide: the step glides into the next one" });
  laneHead.append(laneSwitch, accLab, sldLab);
  const laneGrid = h("div", { class: "lanegrid", style: `height:${laneH}px` }), accRow = h("div", { class: "flagrow" }), sldRow = h("div", { class: "flagrow" });
  const laneIn = h("div", { class: "lanein" }, laneGrid, accRow, sldRow), laneScroll = h("div", { class: "lscroll" }, laneIn);
  const body = h("div", { class: "rollbody" }, h("div", { class: "corner", role: "group", "aria-label": "Zoom" }, bZoomOut, bZoomIn), ruler, keys, h("div", { class: "rwrap" }, scroller, loadEl), laneHead, laneScroll);
  const rootEl = h("div", { class: "roll" + (wide ? " wide" : "") + (opts.compact ? " compact" : ""), dataset: { part: p } }, ctl, tools, note, body, status);

  // ---- geometry
  const pitchY = (pitch) => (hi - pitch) * rh;
  function range() {
    let a = 127, b = 0;
    for (const s of steps()) for (const n of R.notesOf(s)) { a = Math.min(a, n); b = Math.max(b, n); }
    lo = Math.min(24, Math.floor(Math.min(a, 24) / 12) * 12); hi = Math.max(96, b > 96 ? Math.min(127, Math.ceil((b + 1) / 12) * 12 - 1) : 96);
  }
  function at(e) {
    const r = grid.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top;
    return { x, y, step: Math.floor(x / cw), pitch: clamp(hi - Math.floor(y / rh), lo, hi) };
  }
  function layout() {
    if (destroyed || !rootEl.isConnected) return;
    range();
    const avail = Math.max(120, scroller.clientWidth || 600), n = len();
    cw = clamp(Math.floor(Math.max(avail / n, 22) * zoom), 14, 200);
    const W = n * cw, H = (hi - lo + 1) * rh;
    grid.style.width = W + "px"; grid.style.height = H + "px";
    laneIn.style.width = W + "px"; rulerIn.style.width = W + "px";
    keysIn.style.height = H + "px";
    const sbw = scroller.offsetWidth - scroller.clientWidth, sbh = scroller.offsetHeight - scroller.clientHeight;
    ruler.style.marginRight = sbw + "px"; laneScroll.style.marginRight = sbw + "px"; keysIn.style.marginBottom = sbh + "px";
    vlines.style.backgroundSize = `${cw}px 100%, ${4 * cw}px 100%, ${16 * cw}px 100%`;
    buildKeys(); buildRuler(); paintRows(); renderAll();
    if (!scrolled && st.loaded) { scrolled = true; centre(); }
    syncScroll();
  }
  function centre() {
    const ns = []; for (const s of steps().slice(0, len())) for (const n of R.notesOf(s)) ns.push(n);
    ns.sort((a, b) => a - b);
    const mid = ns.length ? ns[ns.length >> 1] : 60;
    scroller.scrollTop = Math.max(0, pitchY(mid) - scroller.clientHeight / 2 + rh / 2);
  }
  function syncScroll() { ruler.scrollLeft = laneScroll.scrollLeft = scroller.scrollLeft; keys.scrollTop = scroller.scrollTop; }
  scroller.addEventListener("scroll", syncScroll);
  function setZoom(z) { zoom = clamp(z, 0.4, 4); layout(); }
  function saveScale() { try { localStorage.setItem(SCALE_KEY, `${root},${scale}`); } catch { /* not remembered */ } }

  // ---- static pieces: keys, ruler, row stripes
  function buildKeys() {
    const f = document.createDocumentFragment();
    for (let n = hi; n >= lo; n--) {
      const black = R.isBlackKey(n), c = n % 12 === 0;
      f.append(h("div", { class: "k " + (black ? "b" : "w") + (c ? " c" : ""), style: `height:${rh}px`, dataset: { p: n }, text: black ? "" : (c || wide) ? noteName(n) : "" }));
    }
    keysIn.replaceChildren(f);
  }
  function buildRuler() {
    const f = document.createDocumentFragment();
    for (let i = 0; i < len(); i += 4) f.append(h("span", { class: "rtick" + (i % 16 === 0 ? " bar" : ""), style: `left:${i * cw}px;width:${4 * cw}px`, text: String(i + 1) }));
    rulerIn.replaceChildren(f);
  }
  function paintRows() {
    const f = document.createDocumentFragment(), chr = scale === 0, st4 = state().arpSeq;
    for (let n = hi; n >= lo; n--) {
      const ins = R.inScale(n, root, scale), pc = ((n - root) % 12 + 12) % 12;
      f.append(h("div", { class: "rrow" + (chr ? (R.isBlackKey(n) ? " blk" : "") : ins ? "" : " out") + (!chr && pc === 0 ? " rt" : "") + (n % 12 === 0 ? " oct" : "") + (st4 && n === 60 ? " zero" : ""), style: `height:${rh}px` }));
    }
    rows.replaceChildren(f);
  }

  // ---- the notes
  function renderNotes() {
    const f = document.createDocumentFragment(), n = len(), s = steps();
    for (const b of R.bars(s, n)) {
      if (b.pitch < lo || b.pitch > hi) continue;
      const step = s[b.start], on = sel && sel.start === b.start && sel.pitch === b.pitch, w = b.len * cw - 1;
      const el = h("div", { class: "rnote" + (on ? " sel" : "") + (step.flags & R.F_ACCENT ? " acc" : "") + (step.flags & R.F_SLIDE && !state().arpSeq ? " sld" : "") + (step.chance < 100 && !state().arpSeq ? " prob" : "") + (b.len > 1 ? " long" : ""),
        style: `left:${b.start * cw}px;top:${pitchY(b.pitch)}px;width:${w}px;height:${rh - 1}px;opacity:${state().arpSeq ? 1 : (0.5 + 0.5 * (step.vel || 96) / 127).toFixed(2)}`, dataset: { start: b.start, pitch: b.pitch } });
      if (w >= 30) el.append(h("span", { class: "nn", text: noteName(b.pitch) }));
      if (step.ratchet > 1 && !state().arpSeq) el.append(h("i", { class: "rat", style: `width:${Math.min(cw, w)}px;background-size:${Math.min(cw, w) / step.ratchet}px 100%` }));
      f.append(el);
    }
    nlayer.replaceChildren(f);
  }
  function renderLane() {
    const f = document.createDocumentFragment(), a = document.createDocumentFragment(), sl = document.createDocumentFragment(), n = len(), s = steps(), sq = state().arpSeq;
    for (let i = 0; i < n; i++) {
      const stp = s[i], on = R.isOn(stp);
      if (on && !sq) {
        const v = lane === "vel" ? stp.vel || 96 : lane === "chance" ? stp.chance : stp.ratchet, mx = lane === "vel" ? 127 : lane === "chance" ? 100 : 4;
        const bh = Math.max(3, Math.round((laneH - 4) * v / mx));
        f.append(h("div", { class: "lbar" + (lane === "vel" && stp.flags & R.F_ACCENT ? " acc" : ""), style: `left:${i * cw + 2}px;width:${cw - 4}px;height:${bh}px` }, cw >= 26 && laneH >= 54 ? h("span", { class: "lv" + (bh > laneH - 18 ? " in" : ""), text: String(v) }) : null));
      }
      a.append(h("button", { type: "button", class: "fl" + (stp.flags & R.F_ACCENT && on ? " on" : "") + (on ? "" : " off"), style: `left:${i * cw}px;width:${cw}px`, disabled: !on, "aria-pressed": String(!!(stp.flags & R.F_ACCENT && on)), "aria-label": `Accent of step ${i + 1}`, title: on ? `Accent of step ${i + 1}` : "", dataset: { i }, onclick: () => st.edit("accent", R.toggleFlag(steps(), len(), i, R.F_ACCENT)) }));
      sl.append(h("button", { type: "button", class: "fl" + (stp.flags & R.F_SLIDE && on ? " on" : "") + (on ? "" : " off"), style: `left:${i * cw}px;width:${cw}px`, disabled: !on, "aria-pressed": String(!!(stp.flags & R.F_SLIDE && on)), "aria-label": `Slide of step ${i + 1}`, title: on ? `Slide of step ${i + 1}` : "", dataset: { i }, onclick: () => st.edit("slide", R.toggleFlag(steps(), len(), i, R.F_SLIDE)) }));
    }
    laneGrid.replaceChildren(f); accRow.replaceChildren(a); sldRow.replaceChildren(sl);
  }
  function renderCursor() {
    const show = !sel && document.activeElement === grid;
    curEl.style.display = show ? "block" : "none";
    curEl.style.left = cur.step * cw + "px"; curEl.style.top = pitchY(cur.pitch) + "px"; curEl.style.width = cw - 1 + "px"; curEl.style.height = rh - 1 + "px";
  }
  function renderAll() { renderNotes(); renderLane(); renderCursor(); describe(); }

  // ---- state shown around the roll (the hide rule: what does not apply is not shown; the notes say why)
  function update() {
    const s = state(), sq = s.arpSeq;
    rootEl.classList.toggle("arpseq", sq);
    for (const c of cells) c.refresh();
    laneBtns.forEach((b, i) => { b.setAttribute("aria-pressed", String(LANES[i][0] === lane)); });
    laneSwitch.hidden = sq; sldRow.hidden = sldLab.hidden = sq; laneGrid.hidden = sq;
    live.checked = st.live; bSend.hidden = bRevert.hidden = st.live;
    bSend.disabled = bRevert.disabled = !st.pending;
    pend.textContent = st.pending ? (st.live ? "sending ..." : `${st.pending} step${st.pending > 1 ? "s" : ""} not sent`) : "";
    bUndo.disabled = !st.canUndo; bRedo.disabled = !st.canRedo; bPaste.disabled = !R.clipboard.steps;
    bZoomOut.disabled = zoom <= 0.4; bZoomIn.disabled = zoom >= 4;
    const msgs = [];
    if (s.seqOff) msgs.push("SEQ is OFF in GLO > MODULES: the unit plays none of these steps. Editing still works.");
    if (sq) msgs.push(s.arpOff ? "ARP MODE is SEQ but ARP is OFF in GLO > MODULES: this pattern plays nothing now." : "ARP MODE is SEQ: the steps are offsets for the keys you hold (C4 = no offset), timed by the arp's RATE and GATE. One note per step; length, slide, velocity, chance and ratchet are not used.");
    note.textContent = msgs.join(" "); note.hidden = !msgs.length;
    loadEl.hidden = st.loaded;
    const g = `${len()}|${sq}|${st.loaded}|${hi}|${lo}`; if (g !== sig) { sig = g; layout(); } else { renderLane(); }
    describe();
  }
  function describe() {
    const b = sel && R.barAt(steps(), len(), sel.start, sel.pitch);
    if (!b) { status.textContent = st.error ? `Not sent: ${st.error.message}` : "Click: add a note. Drag: move it. Drag its right edge: length. Right-click or double-click: delete."; return; }
    const s = steps()[b.start], sq = state().arpSeq, parts = [`Step ${b.start + 1}`, `${noteName(b.pitch)} (${b.pitch})`];
    parts.push(sq ? `offset ${b.pitch - 60 >= 0 ? "+" : ""}${b.pitch - 60}` : `length ${b.len}`);
    if (!sq) parts.push(`velocity ${s.vel || 96}`);
    if (s.flags & R.F_ACCENT) parts.push("accent");
    if (s.flags & R.F_SLIDE && !sq) parts.push("slide");
    if (!sq && s.chance < 100) parts.push(`chance ${s.chance}%`);
    if (!sq && s.ratchet > 1) parts.push(`ratchet x${s.ratchet}`);
    if (!sq && s.n > 1) parts.push(`${s.n} notes in this step (one length, one velocity)`);
    status.textContent = parts.join(", ");
  }
  const flash = (msg) => { if (msg) say(msg, "warn"); };

  // ---- selection and the cursor
  function select(b) { sel = b ? { start: b.start, pitch: b.pitch } : null; if (b) cur = { step: b.start, pitch: b.pitch }; renderNotes(); renderCursor(); describe(); }
  function reveal(step, pitch) {
    const x = step * cw, y = pitchY(pitch), v = scroller;
    if (x < v.scrollLeft) v.scrollLeft = x; else if (x + cw > v.scrollLeft + v.clientWidth) v.scrollLeft = x + cw - v.clientWidth;
    if (y < v.scrollTop) v.scrollTop = y; else if (y + rh > v.scrollTop + v.clientHeight) v.scrollTop = y + rh - v.clientHeight;
  }
  const selBar = () => (sel ? R.barAt(steps(), len(), sel.start, sel.pitch) : null);
  const commitSel = (label, ch, to) => {              // a one-shot edit, then the selection follows the note
    if (ch.msg) flash(ch.msg);
    if (!st.edit(label, ch)) return false;
    if (to) { sel = { start: to.start, pitch: to.pitch }; cur = { step: to.start, pitch: to.pitch }; reveal(to.start, to.pitch); }
    return true;
  };

  // ---- mouse
  let down = null;
  grid.addEventListener("pointerdown", (e) => {
    if (e.button !== 0 || !st.loaded) return;
    grid.focus({ preventScroll: true });
    const a = at(e); if (a.step < 0 || a.step >= len()) return;
    const bar = R.barAt(steps(), len(), a.step, a.pitch), sq = state().arpSeq;
    cur = { step: a.step, pitch: a.pitch };
    if (bar) {
      select(bar);
      const edge = !sq && (bar.start + bar.len) * cw - a.x <= EDGE;
      down = { kind: edge ? "resize" : "move", bar, a, moved: false };
      if (edge) st.begin();
    } else {
      const ch = R.addNote(steps(), len(), a.step, a.pitch, { maxNotes: maxN() });
      if (!ch.size) { flash(ch.msg); select(null); renderCursor(); return; }
      st.begin(); st.preview(ch);
      select({ start: a.step, pitch: a.pitch });
      down = { kind: "draw", a, moved: false };
    }
    grid.setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  grid.addEventListener("pointermove", (e) => {
    const a = at(e);
    if (!down) {
      const bar = st.loaded && a.step >= 0 && a.step < len() ? R.barAt(steps(), len(), a.step, a.pitch) : null;
      grid.style.cursor = bar && !state().arpSeq && (bar.start + bar.len) * cw - a.x <= EDGE ? "ew-resize" : bar ? "grab" : "crosshair";
      return;
    }
    const sq = state().arpSeq;
    if (down.kind === "draw") {
      if (sq) return;
      const base = st.base, add = R.addNote(base, len(), down.a.step, down.a.pitch, { maxNotes: maxN() }), tmp = R.apply(base, add);
      const rz = R.resize(tmp, len(), down.a.step, clamp(a.step, 0, len() - 1) - down.a.step + 1);
      st.preview(new Map([...add, ...rz]));
    } else if (down.kind === "resize") {
      st.preview(R.resize(st.base, len(), down.bar.start, clamp(a.step, 0, len() - 1) - down.bar.start + 1));
    } else if (down.kind === "move") {
      if (!down.moved) { if (Math.hypot(a.x - down.a.x, a.y - down.a.y) < 4) return; down.moved = true; st.begin(); }
      const ds = a.step - down.a.step, dp = a.pitch - down.a.pitch, b = down.bar;
      const to = { start: clamp(b.start + ds, 0, len() - 1), pitch: clamp(b.pitch + dp, 0, 127) };
      const ch = R.moveNote(st.base, len(), b.start, b.pitch, to.start, to.pitch, { maxNotes: maxN() });
      st.preview(ch); if (ch.size) sel = to; else sel = { start: b.start, pitch: b.pitch };
    }
    renderNotes(); describe();
  });
  const finish = (e, cancel) => {
    if (!down) return;
    const d = down; down = null;
    try { grid.releasePointerCapture(e.pointerId); } catch { /* already released */ }
    if (cancel) { st.cancel(); return; }
    if (st.base) { const label = d.kind === "draw" ? "add note" : d.kind === "resize" ? "change length" : "move note"; st.commit(label); }
    if (sel && !R.barAt(steps(), len(), sel.start, sel.pitch)) sel = null;
    renderAll(); update();
  };
  grid.addEventListener("pointerup", (e) => finish(e, false));
  grid.addEventListener("pointercancel", (e) => finish(e, true));
  grid.addEventListener("contextmenu", (e) => {
    e.preventDefault();
    const a = at(e); if (a.step < 0 || a.step >= len()) return;
    const b = R.barAt(steps(), len(), a.step, a.pitch);
    if (b) { commitSel("delete note", R.removeNote(steps(), len(), b.start, b.pitch)); sel = null; renderAll(); }
  });
  grid.addEventListener("dblclick", (e) => {
    const a = at(e), b = a.step >= 0 && a.step < len() ? R.barAt(steps(), len(), a.step, a.pitch) : null;
    if (b) { commitSel("delete note", R.removeNote(steps(), len(), b.start, b.pitch)); sel = null; renderAll(); }
  });
  keys.addEventListener("click", (e) => { const k = e.target.closest?.(".k"); if (k) { cur.pitch = +k.dataset.p; sel = null; renderAll(); grid.focus(); } });

  // ---- the lane: press and drag across the steps that have notes
  let paint = null;
  const laneValue = (e) => {
    const r = laneGrid.getBoundingClientRect(), f = clamp(1 - (e.clientY - r.top) / (laneH - 2), 0, 1);
    return lane === "vel" ? Math.max(1, Math.round(f * 127)) : lane === "chance" ? Math.max(1, Math.round(f * 100)) : clamp(Math.ceil(f * 4 - 0.0001) || 1, 1, 4);
  };
  laneGrid.addEventListener("pointerdown", (e) => {
    if (e.button !== 0 || !st.loaded) return;
    paint = {}; st.begin(); laneGrid.setPointerCapture(e.pointerId); lanePaint(e); e.preventDefault();
  });
  laneGrid.addEventListener("pointermove", (e) => { if (paint) lanePaint(e); });
  const laneEnd = (e) => { if (!paint) return; paint = null; try { laneGrid.releasePointerCapture(e.pointerId); } catch { /* released */ } st.commit(lane === "vel" ? "velocity" : lane === "chance" ? "chance" : "ratchet"); update(); };
  laneGrid.addEventListener("pointerup", laneEnd); laneGrid.addEventListener("pointercancel", laneEnd);
  function lanePaint(e) {
    const r = laneGrid.getBoundingClientRect(), i = Math.floor((e.clientX - r.left) / cw);
    if (i < 0 || i >= len() || !R.isOn(st.base[i])) return;
    const v = laneValue(e), patch = lane === "vel" ? { vel: v } : lane === "chance" ? { chance: v } : { ratchet: v };
    paint[i] = R.setProps(st.base, len(), i, patch).get(i);
    const m = new Map(); for (const [k, s] of Object.entries(paint)) if (s) m.set(+k, s);
    st.preview(m); renderLane(); describe();
  }

  // ---- the keyboard
  const move = (dStep, dPitch) => {
    const b = selBar();
    if (!b) { cur = { step: clamp(cur.step + dStep, 0, len() - 1), pitch: clamp(cur.pitch + dPitch, lo, hi) }; reveal(cur.step, cur.pitch); renderCursor(); return; }
    const to = { start: clamp(b.start + dStep, 0, len() - 1), pitch: clamp(b.pitch + dPitch, 0, 127) };
    commitSel("move note", R.moveNote(steps(), len(), b.start, b.pitch, to.start, to.pitch, { maxNotes: maxN() }), to);
    renderAll();
  };
  function key(e) {
    const k = e.key, mod = e.ctrlKey || e.metaKey;
    if (mod && !e.altKey && (k === "z" || k === "Z" || k === "y" || k === "Y")) { e.preventDefault(); (k.toLowerCase() === "y" || e.shiftKey) ? doRedo() : doUndo(); return true; }
    if (mod || e.altKey) return false;
    const b = selBar(), sq = state().arpSeq;
    switch (k) {
      case "ArrowLeft": case "ArrowRight": {
        const d = k === "ArrowLeft" ? -1 : 1;
        if (e.shiftKey && b && !sq) { commitSel("change length", R.resize(steps(), len(), b.start, b.len + d)); renderAll(); } else move(d, 0);
        break;
      }
      case "ArrowUp": case "ArrowDown": move(0, (k === "ArrowUp" ? 1 : -1) * (e.shiftKey ? 12 : 1)); break;
      case "Home": cur.step = 0; if (b) { sel = null; } reveal(cur.step, cur.pitch); renderAll(); break;
      case "End": cur.step = len() - 1; if (b) { sel = null; } reveal(cur.step, cur.pitch); renderAll(); break;
      case "Enter": case " ": {
        const hit = R.barAt(steps(), len(), cur.step, cur.pitch);
        if (hit) { select(hit); break; }
        const ch = R.addNote(steps(), len(), cur.step, cur.pitch, { maxNotes: maxN() });
        commitSel("add note", ch, { start: cur.step, pitch: cur.pitch }); renderAll(); break;
      }
      case "Delete": case "Backspace":
        if (b) { commitSel("delete note", R.removeNote(steps(), len(), b.start, b.pitch)); sel = null; renderAll(); }
        break;
      case "Escape": if (sel) { sel = null; renderAll(); } else return false; break;
      case "a": case "A": if (b) { commitSel("accent", R.toggleFlag(steps(), len(), b.start, R.F_ACCENT)); renderAll(); } break;
      case "s": case "S": if (b && !sq) { commitSel("slide", R.toggleFlag(steps(), len(), b.start, R.F_SLIDE)); renderAll(); } break;
      case "+": case "=": case "-": case "_": if (b && !sq) { const s = steps()[b.start]; commitSel("velocity", R.setProps(steps(), len(), b.start, { vel: (s.vel || 96) + (k === "+" || k === "=" ? 8 : -8) })); renderAll(); } break;
      case "r": case "R": if (b && !sq) { const s = steps()[b.start]; commitSel("ratchet", R.setProps(steps(), len(), b.start, { ratchet: s.ratchet % 4 + 1 })); renderAll(); } break;
      default: return false;
    }
    e.preventDefault(); e.stopPropagation();
    return true;
  }
  grid.addEventListener("keydown", key);
  rootEl.addEventListener("keydown", (e) => {          // Undo / Redo also with the focus on a button of the roll (not in a text or number field)
    if (e.target === grid || /^(INPUT|TEXTAREA)$/.test(e.target.tagName) && e.target.type !== "checkbox" && e.target.type !== "range") return;
    if ((e.ctrlKey || e.metaKey) && /^[zZyY]$/.test(e.key)) key(e);
  });
  grid.addEventListener("focus", renderCursor); grid.addEventListener("blur", renderCursor);
  rootEl.addEventListener("focusin", () => { app.rollActive = api; });
  function doUndo() { const e = st.undo(); if (e) { fixSel(); say(`Undo: ${e.label}.`); } else say("Nothing to undo.", "warn"); renderAll(); update(); }
  function doRedo() { const e = st.redo(); if (e) { fixSel(); say(`Redo: ${e.label}.`); } else say("Nothing to redo.", "warn"); renderAll(); update(); }
  const fixSel = () => { if (sel && !R.barAt(steps(), len(), sel.start, sel.pitch)) sel = null; };

  // ---- the MODULES switches (SEQ / ARP OFF), read now and then
  let timer = 0, polled = false;
  async function readModules() {
    if (destroyed || document.hidden || u.info.menuCount == null) return;
    try {
      if (!u.menu) await u.readMenu();
      else for (const id of [28, 29]) { const m = u.menu.find((x) => x.id === id); if (!m) continue; const f = parseMenu(await u.req(CMD.MENU_DESC, [m.index], 1500)); if (f) { m.value = f.value; m.names = f.names ?? m.names; } }
      polled = true; if (!destroyed) update();
    } catch { /* the next poll */ }
  }

  // ---- wiring to the store
  const unsub = st.sub((e) => {
    if (destroyed) return;
    if (e.type === "load") { sig = ""; scrolled = false; fixSel(); update(); } else if (e.type === "error") { say(`Not sent: ${e.error?.message ?? e.error}`, "warn"); describe(); }
    else if (e.type === "state") update();
    else if (e.preview) { /* the gesture draws itself */ } else { range(); const g = `${len()}|${state().arpSeq}|${st.loaded}|${hi}|${lo}`; if (g !== sig) { sig = g; layout(); } else renderAll(); update(); }
  });
  const ro = typeof ResizeObserver === "function" ? new ResizeObserver(() => { if (Math.abs((scroller.clientWidth || 0) - lastW) > 1 || Math.abs((scroller.clientHeight || 0) - lastH) > 1) { lastW = scroller.clientWidth; lastH = scroller.clientHeight; layout(); } }) : null;
  let lastW = 0, lastH = 0;
  st.views++;
  const api = {
    el: rootEl, part: p, store: st,
    refresh() { if (!destroyed) update(); },
    attach() { ro?.observe(scroller); if (!timer) timer = setInterval(readModules, 3000); readModules(); if (!st.loaded || st.stale) st.load(); layout(); },
    focus() { grid.focus(); },
    cellCenter(step, pitch) { reveal(step, pitch); const r = grid.getBoundingClientRect(); return { x: r.left + (step + 0.5) * cw, y: r.top + pitchY(pitch) + rh / 2, right: r.left + (step + 1) * cw }; },   // (the browser test)
    laneCenter(step) { const r = laneGrid.getBoundingClientRect(); return { x: r.left + (step + 0.5) * cw, top: r.top, bottom: r.bottom }; },
    pollModules: readModules,
    undo: doUndo, redo: doRedo,
    select: (start, pitch) => select({ start, pitch }),
    get selection() { return sel; }, get cw() { return cw; }, get rh() { return rh; }, get hi() { return hi; },
    destroy() { destroyed = true; unsub(); st.views--; clearInterval(timer); ro?.disconnect(); if (app.rollActive === api) app.rollActive = null; },
  };
  update();
  return api;
}
