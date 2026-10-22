// SPDX-License-Identifier: GPL-3.0-only
// DUELL installer: the pure state -> UI mapping (no DOM). The page (install/index.html) applies the result;
// test_installer_state.mjs checks it with node.
//
// state = { busy: null | "backup" | "install" | "return", ready, hasImage, hasStock }
//   ready     WebMIDI is available
//   hasImage  a DUELL package was accepted
//   hasStock  the official V15 file was chosen and verified
export const ACTIONS = ["backup", "install", "return"];

export const BANNERS = {
  backup:  { kind: "backup",  text: "BACKING UP: do not unplug or close this page until it says DONE. Nothing is written to the FM-1." },
  install: { kind: "install", text: "INSTALLING DUELL: do not unplug or close this page until it says DONE." },
  return:  { kind: "return",  text: "RETURNING TO THE OFFICIAL FIRMWARE: do not unplug or close this page until it says DONE." },
};
const NAMES = { backup: "the backup", install: "the DUELL install", return: "the return to the official firmware" };

// The optional "Erase my saved data after installing" box in the INSTALL card (off by default). Pure texts and outcomes; the page
// does the talking (install/index.html eraseAfterInstall -> fm1backup.js eraseAllData).
export const ERASE = {
  label: "Erase my saved data after installing (start from scratch)",
  help: "Erases your settings (theme, scale, MIDI channels, favourites), the autosave, your patches, your multis and your patterns. " +
        "The factory sounds, multis and patterns come back. This cannot be undone: back up first. " +
        "Left off, your saved data stays and the unit comes back as you left it.",
  status: "Erasing your data ...",
  done: "Done. Your saved data was erased. The unit starts as new.",
  lockNote: "Locked: the install is running.",
};
// rc: the unit's answer to ERASE_ALL (0 erased); err: why there was none (the unit did not appear, no answer, the connection broke).
// Only rc 0 is "erased"; everything else says plainly that the install worked and the data is still there.
export function eraseOutcome(rc, err = null) {
  if (rc === 0) return { erased: true, bad: false, text: ERASE.done };
  const why = rc === 3 ? "The unit was playing." : rc === 1 ? "The unit refused the request." : rc === 4 ? "The unit could not write its memory." :
    err ? "The unit could not be reached." : "The unit did not confirm.";
  return { erased: false, bad: true,
    text: `The install itself succeeded, but your saved data was NOT erased (${why}) and is still on the unit. ` +
          "To start from scratch, use MENU > SYSTEM > ERASE DATA on the unit." };
}

// What the unit runs decides the erase box's default. The unit's answer to the editor protocol's INFO (fm1backup.js, cmd 1) starts with
// "FELUCCA <version string>"; DUELL's version string is "DUELL 0.2.4 beta", Felucca's is a bare number, the official firmware does not answer.
export const SMART = {
  duell:   "Updating DUELL: your patches and settings are kept.",
  clean:   "Clean install recommended: the unit does not run DUELL, so old data from another firmware is cleared.",
  hint:    "Connect the FM-1 to check.",
  keepOwn: "You chose to keep the data: anything an older firmware left on the unit stays.",
  eraseOwn: "You chose to erase: your patches and settings will be erased after the install.",
  detected: { duell: (v) => `Detected: DUELL ${v} running`, other: "Detected: not DUELL (or no answer)", none: "Not checked yet. Connect the FM-1 and press \"Check the unit\"." },
  checking: "Checking the unit ...",
};
// info: the version string from INFO (a string), or null/undefined/anything else when the unit gave no usable answer.
// Returns { kind: "duell", version } or { kind: "other" }. (Not having asked at all is `null` for the caller, never this.)
export function detectUnit(info) {
  const m = typeof info === "string" ? /^(?:FELUCCA )?DUELL (\d+\.\d+(?:\.\d+)?)(?!\S)/.exec(info.trim()) : null;
  return m ? { kind: "duell", version: m[1] } : { kind: "other" };
}
// The erase box: detected = detectUnit(...) or null (not asked / could not talk yet); touched = the user clicked the box;
// current = the box as it stands. Once touched the box is never changed. Returns what the page shows.
export function eraseDecision({ detected = null, touched = false, current = false } = {}) {
  const isDuell = !!detected && detected.kind === "duell";
  const checked = touched ? !!current : !isDuell;
  const line = !detected ? SMART.detected.none : isDuell ? SMART.detected.duell(detected.version) : SMART.detected.other;
  let help = isDuell ? SMART.duell : SMART.clean;
  if (touched && checked && isDuell) help = SMART.eraseOwn;
  else if (touched && !checked && !isDuell) help = SMART.keepOwn;
  else if (!detected && !touched) help += " " + SMART.hint;
  return { checked, line, help };
}

export function uiState(s) {
  const busy = s.busy || null;
  const wants = {
    backup: s.ready,
    install: s.ready && s.hasImage,
    return: s.ready && s.hasStock,
  };
  const buttons = {};
  for (const a of ACTIONS) {
    let note = "";
    if (busy && busy !== a) note = `Locked: ${NAMES[busy]} is running. Wait until it says DONE.`;
    else if (busy === a) note = "Running. Do not unplug.";
    buttons[a] = { enabled: !busy && !!wants[a], note };
  }
  return {
    banner: busy ? BANNERS[busy] : null,
    buttons,
    bars: { backup: busy === "backup", install: busy === "install", return: busy === "return" },   // a bar is shown only in its own card, only while it runs
    inputsLocked: !!busy,                  // file pickers and checkboxes
    eraseBox: { disabled: !!busy, note: busy ? ERASE.lockNote : "" },   // the INSTALL card's erase box: locked while anything runs
    confirmUnload: !!busy,                 // beforeunload protection
  };
}

// texts per card
export const SWITCH_OFF_ON = "Switch the unit off and on once.";
export const TEXT = {
  writing: { install: "Writing DUELL. Do not unplug", return: "Writing official firmware. Do not unplug" },
  done: {
    install: "Done. Switch the unit off and on once.",   // (owner 2026-10-21, item 31: the boot guard step; the one line after a successful install)
    return: "Done. The official firmware is back. Switch the unit off and on.",
  },
};
