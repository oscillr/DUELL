// SPDX-License-Identifier: GPL-3.0-only
// A tiny Chrome DevTools Protocol client for the editor's browser test (no npm packages: the emsdk node has fetch and WebSocket). Headless Chrome on a throw-away profile.
//   const c = await launch(); await c.open(url, 1920, 1080); await c.eval("document.title"); await c.shot(path); await c.close();
import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { existsSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

export const CHROME = process.env.CHROME || ["/Applications/Google Chrome.app/Contents/MacOS/Google Chrome", "/usr/bin/google-chrome", "/usr/bin/chromium", "/usr/bin/chromium-browser"].find(existsSync);

export async function launch() {
  if (!CHROME) throw new Error("no Chrome found (set CHROME=/path/to/chrome)");
  const dir = await mkdtemp(join(tmpdir(), "duell-chrome-"));
  const proc = spawn(CHROME, ["--headless=new", "--remote-debugging-port=0", `--user-data-dir=${dir}`, "--no-first-run", "--no-default-browser-check", "--disable-gpu", "--hide-scrollbars=false", "about:blank"], { stdio: "ignore" });
  let port = 0;
  for (let i = 0; i < 100 && !port; i++) {
    await new Promise((r) => setTimeout(r, 100));
    try { port = +(await readFile(join(dir, "DevToolsActivePort"), "utf8")).split("\n")[0]; } catch { /* not yet */ }
  }
  if (!port) { proc.kill(); throw new Error("Chrome did not start"); }
  const tabs = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
  const ws = new WebSocket(tabs.find((t) => t.type === "page").webSocketDebuggerUrl);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  let id = 0; const wait = new Map(), events = [];
  ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && wait.has(d.id)) { const [res, rej] = wait.get(d.id); wait.delete(d.id); d.error ? rej(new Error(d.error.message)) : res(d.result); } else if (d.method) events.push(d); };
  const send = (method, params = {}) => new Promise((res, rej) => { const i = ++id; wait.set(i, [res, rej]); ws.send(JSON.stringify({ id: i, method, params })); });
  await send("Page.enable"); await send("Runtime.enable"); await send("Log.enable");
  const c = {
    send, events, errors: () => events.filter((e) => e.method === "Runtime.exceptionThrown" || (e.method === "Runtime.consoleAPICalled" && e.params.type === "error") || (e.method === "Log.entryAdded" && e.params.entry.level === "error")).map((e) => JSON.stringify(e.params).slice(0, 400)),
    async open(url, w, h) {
      await send("Emulation.setDeviceMetricsOverride", { width: w, height: h, deviceScaleFactor: 1, mobile: false });
      for (let tries = 0; ; tries++) {          // (a first load now and then stalls: navigate again)
        await send("Page.navigate", { url });
        try { await c.until(`location.href === ${JSON.stringify(url)} && document.readyState === 'complete' && !!window.__app`, 8000); break; } catch (e) { if (tries >= 2) throw e; }
      }
    },
    async size(w, h) { await send("Emulation.setDeviceMetricsOverride", { width: w, height: h, deviceScaleFactor: 1, mobile: false }); await c.sleep(150); },
    async eval(expr) {
      const r = await send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
      if (r.exceptionDetails) throw new Error(`page error: ${r.exceptionDetails.exception?.description || r.exceptionDetails.text}`);
      return r.result.value;
    },
    sleep: (ms) => new Promise((r) => setTimeout(r, ms)),
    async until(expr, ms = 15000) { const t = Date.now(); for (;;) { if (await c.eval(expr).catch(() => false)) return true; if (Date.now() - t > ms) throw new Error(`timeout waiting for: ${expr}`); await c.sleep(50); } },
    async shot(path) { const r = await send("Page.captureScreenshot", { format: "png" }); writeFileSync(path, Buffer.from(r.data, "base64")); },
    // mouse in page coordinates
    async mouse(type, x, y, o = {}) { await send("Input.dispatchMouseEvent", { type, x, y, button: o.button ?? "left", buttons: o.buttons ?? (type === "mouseReleased" || type === "mouseMoved" ? 0 : 1), clickCount: o.clickCount ?? 1, modifiers: o.modifiers ?? 0 }); },
    async click(x, y, o = {}) { await c.mouse("mousePressed", x, y, o); await c.mouse("mouseReleased", x, y, o); },
    async drag(x0, y0, x1, y1, o = {}) {
      await c.mouse("mouseMoved", x0, y0); await c.mouse("mousePressed", x0, y0, o);
      const n = 6; for (let i = 1; i <= n; i++) await c.mouse("mouseMoved", x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, { buttons: 1 });
      await c.mouse("mouseReleased", x1, y1, o);
    },
    // a key: name as KeyboardEvent.key; mods: 1 alt, 2 ctrl, 4 meta, 8 shift
    async key(k, mods = 0) {
      const codes = { ArrowLeft: 37, ArrowRight: 39, ArrowUp: 38, ArrowDown: 40, Delete: 46, Backspace: 8, Enter: 13, Escape: 27, Home: 36, End: 35 };
      const vk = codes[k] ?? k.toUpperCase().charCodeAt(0), text = (mods & 6) ? undefined : k.length === 1 ? k : k === "Enter" ? "\r" : undefined;
      await send("Input.dispatchKeyEvent", { type: text ? "keyDown" : "rawKeyDown", key: k, code: k.length === 1 ? "Key" + k.toUpperCase() : k, windowsVirtualKeyCode: vk, modifiers: mods, text });
      await send("Input.dispatchKeyEvent", { type: "keyUp", key: k, code: k.length === 1 ? "Key" + k.toUpperCase() : k, windowsVirtualKeyCode: vk, modifiers: mods });
    },
    async close() { try { ws.close(); } catch { /* closed */ } proc.kill(); await rm(dir, { recursive: true, force: true }).catch(() => {}); },
  };
  return c;
}
