// node test_installer_state.mjs : the installer's state -> UI mapping (no DOM, no device)
import { uiState, ACTIONS, BANNERS, TEXT, ERASE, eraseOutcome, SMART, detectUnit, eraseDecision } from "./installer_state.js";
import { readFileSync } from "node:fs";
let failed = 0;
const ok = (c, m) => { if (!c) { failed++; console.log("FAIL", m); } };
const base = { busy: null, ready: true, hasImage: true, checksOk: true, hasStock: true };

function installerState() {
  // idle, everything prepared: all three enabled, no banner, no bar, no unload prompt
  let u = uiState(base);
  ok(ACTIONS.every((a) => u.buttons[a].enabled), "idle: all enabled");
  ok(!u.banner && !u.confirmUnload && !u.inputsLocked, "idle: no banner/lock");
  ok(ACTIONS.every((a) => !u.bars[a]), "idle: no bar");

  // idle gating (same rules as before)
  ok(!uiState({ ...base, checksOk: false }).buttons.install.enabled, "install needs the checklist");
  ok(!uiState({ ...base, hasImage: false }).buttons.install.enabled, "install needs a package");
  ok(!uiState({ ...base, hasStock: false }).buttons.return.enabled, "return needs the official file");
  ok(uiState({ ...base, hasImage: false, checksOk: false, hasStock: false }).buttons.backup.enabled, "backup needs nothing else");
  ok(ACTIONS.every((a) => !uiState({ ...base, ready: false }).buttons[a].enabled), "no WebMIDI: all off");

  // running: exactly one bar (its own), all buttons off, banner for that action, notes on the others
  const words = { backup: /^BACKING UP:/, install: /^INSTALLING DUELL:/, return: /^RETURNING TO THE OFFICIAL FIRMWARE:/ };
  for (const a of ACTIONS) {
    u = uiState({ ...base, busy: a });
    ok(ACTIONS.every((x) => !u.buttons[x].enabled), `${a}: all buttons off`);
    ok(ACTIONS.filter((x) => u.bars[x]).join() === a, `${a}: only its own bar`);
    ok(u.banner && words[a].test(u.banner.text) && u.banner.kind === a, `${a}: banner`);
    ok(/do not unplug or close this page until it says DONE/.test(u.banner.text), `${a}: banner warns`);
    ok(u.confirmUnload && u.inputsLocked, `${a}: unload prompt and inputs locked`);
    for (const x of ACTIONS) if (x !== a) ok(/^Locked:/.test(u.buttons[x].note), `${a}: ${x} has a lock note`);
    // even with nothing missing the other buttons stay off; and with things missing nothing becomes enabled
    ok(ACTIONS.every((x) => !uiState({ ...base, busy: a, hasImage: false }).buttons[x].enabled), `${a}: still all off`);
  }
  ok(/Locked: the return to the official firmware/.test(uiState({ ...base, busy: "return" }).buttons.install.note), "install note names the return");

  // wording
  ok(TEXT.writing.return === "Writing official firmware. Do not unplug" && TEXT.writing.install === "Writing DUELL. Do not unplug", "writing texts");
  ok(TEXT.done.return === "Done. The official firmware is back. Switch the unit off and on.", "return done text");
  ok(BANNERS.return.text.startsWith("RETURNING TO THE OFFICIAL FIRMWARE: do not unplug or close this page until it says DONE"), "return banner text");
}

// the page wiring: three cards, each progress bar only in its own card, ids used by the script exist
function pageWiring() {
  const h = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  for (const a of ACTIONS) {
    const card = h.match(new RegExp(`<div class="card ${a}" id="card-${a}">([\\s\\S]*?)\\n    </div>\\n  </section>`));
    ok(card, `card ${a} exists`);
    if (!card) continue;
    for (const part of ["bar", "status", "result", "lock"])
      ok(card[1].includes(`id="${part}-${a}"`), `card ${a} has ${part}`);
    ok(!/id="(bar|status|result)-(?!${a}\b)\w+"/.test(card[1].replace(new RegExp(`id="\\w+-${a}"`, "g"), "")), `card ${a} holds no other card's ids`);
    ok(new RegExp(`<progress class="bar" id="bar-${a}"[^>]*\\shidden`).test(card[1]), `bar ${a} starts hidden`);
  }
  ok(/This writes the official M-VAVE firmware back\. DUELL will be removed\. Keep the cable connected until DONE\./.test(h), "return warning text");
  ok(/Go back to the official firmware/.test(h), "return section title");
  ok(!/id="(bar|status)"/.test(h), "no shared bar/status left");
  // the return card is not inside the package-gated blocks
  const gated = [...h.matchAll(/<div class="needpkg" hidden>([\s\S]*?)\n  <\/div>/g)].map((m) => m[1]).join("");
  ok(!gated.includes('id="card-return"') && !gated.includes('id="card-backup"') && gated.includes('id="card-install"'), "only install is package-gated");
}
// the optional "Erase my saved data after installing" box: locked while anything runs, and an outcome that never claims an erase the unit did not report
function eraseBox() {
  ok(!uiState(base).eraseBox.disabled && uiState(base).eraseBox.note === "", "erase box: usable when idle");
  for (const a of ACTIONS) ok(uiState({ ...base, busy: a }).eraseBox.disabled && /^Locked:/.test(uiState({ ...base, busy: a }).eraseBox.note), `erase box: locked while ${a} runs`);
  ok(ERASE.status === "Erasing your data ...", "erase status text");
  ok(ERASE.done === "Done. Your saved data was erased. The unit starts as new.", "erase done text");
  const yes = eraseOutcome(0);
  ok(yes.erased && !yes.bad && yes.text === ERASE.done, "rc 0: erased, the done line");
  const clean = (o) => !o.erased && o.bad && /install itself succeeded/.test(o.text) && /NOT erased/.test(o.text) && /MENU > SYSTEM > ERASE DATA/.test(o.text) && !/Your saved data was erased/.test(o.text);
  for (const [rc, err] of [[1, null], [3, null], [4, null], [99, null], [undefined, null], [null, new Error("timed out")], [null, "notfound"]])
    ok(clean(eraseOutcome(rc, err)), `rc ${rc} / ${err}: install succeeded, data NOT erased, menu item named`);
  ok(/playing/.test(eraseOutcome(3).text) && /could not be reached/.test(eraseOutcome(null, "x").text), "the reason is named");
  for (const w of ["settings", "autosave", "patches", "multis", "patterns", "factory sounds, multis and patterns come back", "cannot be undone", "back up first (step 2)"])
    ok(ERASE.help.includes(w), `help says: ${w}`);
}
function erasePage() {
  const h = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  const card = h.match(/<div class="card install" id="card-install">([\s\S]*?)\n    <\/div>\n  <\/section>/);
  ok(card, "install card");
  if (!card) return;
  const box = card[1].match(/<input type="checkbox" id="erase-after"([^>]*)>/);
  ok(box && !/class="chk"/.test(box[1]), "erase box is in the INSTALL card, not a checklist box (its state comes from eraseDecision)");
  ok(card[1].includes(ERASE.label) && card[1].includes(ERASE.help), "the page shows the label and the description");
  ok(!document_has_chk_erase(h), "the checklist gating does not count the erase box");
  ok(/const wantErase = \$\("erase-after"\)\.checked;/.test(h) && /if \(installed && !wantErase\) result\("install", TEXT\.done\.install\)/.test(h), "the erase runs only when ticked, after the install finished");
  ok(/\$\("erase-after"\)\.disabled = u\.eraseBox\.disabled/.test(h), "the box is locked with the state");
}
const document_has_chk_erase = (h) => /class="chk"[^>]*id="erase-after"|id="erase-after"[^>]*class="chk"/.test(h);
// the smart default: what the unit runs decides the box, the user's own click always wins
function smartDefault() {
  const duell = detectUnit("DUELL 0.2.4 beta");
  ok(duell.kind === "duell" && duell.version === "0.2.4", "DUELL 0.2.4 beta is DUELL");
  ok(detectUnit("FELUCCA DUELL 0.2.4 beta").kind === "duell" && detectUnit("DUELL 0.2.4").version === "0.2.4", "with the FELUCCA prefix, without 'beta'");
  for (const v of [null, undefined, "", "FELUCCA 1.5.2", "1.2", "FELUCCA 1.0", "M-VAVE", "DUELLX 0.2", 42, "DUELL"])
    ok(detectUnit(v).kind === "other", `not DUELL: ${JSON.stringify(v)}`);
  let d = eraseDecision({ detected: duell });
  ok(d.checked === false && d.help === "Updating DUELL: your patches and settings are kept." && d.line === "Detected: DUELL 0.2.4 running", "DUELL answered: unticked, update text, detected line");
  d = eraseDecision({ detected: detectUnit(null) });
  ok(d.checked === true && d.help === "Clean install recommended: the unit does not run DUELL, so old data from another firmware is cleared." && d.line === "Detected: not DUELL (or no answer)", "no answer / other: ticked, clean text, not-DUELL line");
  d = eraseDecision({});
  ok(d.checked === true && d.help.startsWith(SMART.clean) && /Connect the FM-1 to check/.test(d.help), "not checked yet: ticked, clean text and the connect hint");
  ok(!/Detected: (DUELL|not DUELL)/.test(d.line) && /Not checked yet/.test(d.line), "never claims a detection without an answer");
  for (const det of [null, duell, detectUnit(null)]) {
    ok(eraseDecision({ detected: det, touched: true, current: true }).checked === true, "touched ticked stays ticked");
    ok(eraseDecision({ detected: det, touched: true, current: false }).checked === false, "touched unticked stays unticked");
  }
  ok(eraseDecision({ detected: duell, touched: true, current: true }).help === SMART.eraseOwn, "chose to erase on DUELL: says so");
  ok(eraseDecision({ detected: detectUnit(null), touched: true, current: false }).help === SMART.keepOwn, "chose to keep on non-DUELL: says so");
  ok(eraseDecision({ detected: duell, touched: true, current: false }).help === SMART.duell, "touched in line with the detection: the plain text");
  const h = readFileSync(new URL("./install/index.html", import.meta.url), "utf8");
  ok(/id="erase-detect"/.test(h) && /id="erase-smart"/.test(h) && /id="detect"/.test(h), "page has the detected line, the recommendation and the check button");
  ok(/addEventListener\("change", \(\) => \{ touched = true;/.test(h), "a click on the box marks it touched");
  ok(/requestMIDIAccess/.test(h.split("a silent check at load")[1].split("// ---- install")[0]) && /state !== "granted"\) return/.test(h), "the load check asks nothing unless MIDI was already allowed");
}
installerState(); pageWiring(); eraseBox(); erasePage(); smartDefault();
console.log(failed ? `INSTALLER STATE TESTS FAILED (${failed})` : "installer state tests passed");
process.exit(failed ? 1 : 0);
