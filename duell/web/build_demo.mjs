// SPDX-License-Identifier: GPL-3.0-only
// DUELL 1.1 editor: builds ONE self-contained file (all JS, CSS, fonts and the logo inline) that opens by double click from the disk (Chrome does not load ES modules from file://).
//   node build_demo.mjs [out.html]        (default out: ../../../../DUELL/working docs/ui_redesign/DUELL_EDITOR_DEMO.html)
// Needs esbuild (npm i esbuild somewhere; set ESBUILD=/path/to/node_modules/esbuild/lib/main.js when it is not installed next to this file).
// The file starts in DEMO MODE (the simulated unit; no unit, no MIDI, no network). Add #nodemo to the address to use the real editor path (a real unit needs Chrome with MIDI and a secure page: use the served editor).
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const esbuild = await import(process.env.ESBUILD ? pathToFileURL(process.env.ESBUILD).href : "esbuild");
const out = resolve(process.argv[2] ?? join(here, "../../../../DUELL/working docs/ui_redesign/DUELL_EDITOR_DEMO.html"));
const ed = (f) => join(here, "editor", f);

const js = await esbuild.build({ entryPoints: [ed("app.js")], bundle: true, write: false, format: "iife", target: "chrome110", loader: { ".json": "json" }, logLevel: "warning", legalComments: "none" });
const css1 = readFileSync(ed("editor.css"), "utf8");
const css2 = await esbuild.build({ entryPoints: [ed("look11.css")], bundle: true, write: false, loader: { ".ttf": "dataurl" }, logLevel: "warning" });
let html = readFileSync(ed("index.html"), "utf8");
const logo = "data:image/png;base64," + readFileSync(ed("img/editor-header.png")).toString("base64");
html = html.replace(/<link rel="stylesheet"[^>]*>\s*/g, "")
  .replace(/<script type="module"[^>]*><\/script>/, "")
  .replace('src="./img/editor-header.png"', () => `src="${logo}"`)
  .replace(/<a id="backuplink"[^>]*>[^<]*<\/a>/, "")
  .replace("</head>", () => `<style>\n${css1}\n${css2.outputFiles[0].text}\n</style>\n</head>`)
  .replace("</body>", () => `<script>window.__DUELL_DEMO_FILE = true;</script>\n<script>\n${js.outputFiles[0].text.replace(/<\/script/gi, "<\\/script")}\n</script>\n</body>`);
html = html.replace("<head>", "<head>\n<!-- Barlow Semi Condensed: Copyright 2017 The Barlow Project Authors (https://github.com/jpt/barlow), SIL Open Font License 1.1 (editor/fonts/OFL.txt). -->");
writeFileSync(out, html);
console.log(`${out}: ${(html.length / 1024).toFixed(0)} KB`);
