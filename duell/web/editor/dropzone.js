// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: drag and drop and file helpers for the library (PATCHES, WAVETABLES, the part columns). Text from a file is only ever set with textContent.
// A drop zone counts enter / leave (a child element fires its own), marks itself with the class "dropping" while a file (or a slot) is over it, and hands the
// drop to its handler. The page also stops the browser from opening a file that is dropped anywhere else.
export const SLOT_TYPE = "application/x-duell-slot";

export const dragTypes = (e) => [...(e.dataTransfer?.types || [])];
export const hasFiles = (e) => dragTypes(e).includes("Files");
export const hasSlot = (e) => dragTypes(e).includes(SLOT_TYPE);
export const slotOf = (e) => { const v = e.dataTransfer?.getData(SLOT_TYPE); return v === "" || v == null ? null : +v; };
export const filesOf = (e) => [...(e.dataTransfer?.files || [])];
export async function readBytes(file) { return new Uint8Array(await file.arrayBuffer()); }

// el: the zone. accept(e): is this drag one the zone takes (default: files). onDrop(e): the handler (the zone already called preventDefault). Returns a function that removes it.
export function dropZone(el, { accept = hasFiles, onDrop, onState = () => {} }) {
  let depth = 0;
  const on = (e) => { depth++; if (accept(e)) { el.classList.add("dropping"); onState(true); } };
  const over = (e) => { if (accept(e)) { e.preventDefault(); e.dataTransfer.dropEffect = hasSlot(e) && !hasFiles(e) ? "copy" : "copy"; } };
  const off = () => { depth = Math.max(0, depth - 1); if (!depth) { el.classList.remove("dropping"); onState(false); } };
  const reset = () => { depth = 0; el.classList.remove("dropping"); onState(false); };   // (capture phase: a zone further in takes the drop and stops it, but every zone around it is over)
  const drop = (e) => { if (!accept(e)) return; e.preventDefault(); e.stopPropagation(); onDrop(e); };
  el.addEventListener("dragenter", on); el.addEventListener("dragover", over); el.addEventListener("dragleave", off); el.addEventListener("drop", reset, true); el.addEventListener("drop", drop);
  el.addEventListener("dragend", reset, true);
  return () => { el.removeEventListener("dragenter", on); el.removeEventListener("dragover", over); el.removeEventListener("dragleave", off); el.removeEventListener("drop", reset, true); el.removeEventListener("drop", drop); el.removeEventListener("dragend", reset, true); };
}
// the page: a file dropped where nothing takes it must not make the browser leave the editor
export function guardPage(say) {
  const stop = (e) => { if (hasFiles(e)) e.preventDefault(); };
  window.addEventListener("dragover", stop);
  window.addEventListener("drop", (e) => { if (hasFiles(e) && !e.defaultPrevented) { e.preventDefault(); say?.("Drop a patch file, a DX7 .syx file or a WAV on the PATCHES or WAVETABLES tab, or on a part to load it there.", "warn"); } });
  // while a file is dragged over the page the columns say where it can go
  let depth = 0;
  window.addEventListener("dragenter", (e) => { if (hasFiles(e) && depth++ === 0) document.body.classList.add("dragfiles"); });
  const leave = () => { depth = Math.max(0, depth - 1); if (!depth) document.body.classList.remove("dragfiles"); };
  window.addEventListener("dragleave", leave);
  window.addEventListener("drop", () => { depth = 0; document.body.classList.remove("dragfiles"); }, true);   // (the zones reset themselves in the capture phase)
  window.addEventListener("dragend", () => { depth = 0; document.body.classList.remove("dragfiles"); });
}

// a file the user keeps: the page offers it as a download. app.downloads keeps the names and texts (the tests read them)
export function saveText(app, name, text, type = "application/json") {
  (app.downloads ||= []).push({ name, text });
  try {
    const url = URL.createObjectURL(new Blob([text], { type })), a = document.createElement("a");
    a.href = url; a.download = name; a.style.display = "none"; document.body.append(a); a.click();
    setTimeout(() => { a.remove(); URL.revokeObjectURL(url); }, 2000);
  } catch { /* no download in this environment */ }
}
