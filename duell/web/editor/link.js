// SPDX-License-Identifier: GPL-3.0-only
// DUELL editor: the SysEx link. One request in flight (the unit drops a frame that arrives while it answers another), a queue for every request
// (keep-alive PING included), push frames (CHANGED 23, RELOAD 24, STEP_CHANGED 26, TRACK_CHANGED 32) handed to onpush at any time.
// The same interface is implemented by MockLink (mockunit.js) so the editor runs without a unit: request([cmd, args], {timeout}) -> reply args.
export const PUSH = new Set([23, 24, 26, 32]);

export class Link {
  constructor(input, output) {
    this.input = input; this.output = output; this.pending = null; this.queue = []; this.closed = false; this.onpush = null; this.onclose = null;
    this.lastSend = 0; this.timer = null;
    input.onmidimessage = (e) => {
      const d = e.data;
      if (d.length < 6 || d[0] !== 240 || d[1] !== 125 || d[2] !== 70 || d[3] !== 76 || d.at(-1) !== 247) return;
      const cmd = d[4], args = Array.from(d.slice(5, -1));
      if (PUSH.has(cmd)) { this.onpush?.(cmd, args); return; }
      if (this.pending?.cmd === cmd) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.resolve(args); this.pump(); }
    };
  }
  request([cmd, args = []], opt = {}) {
    if (this.closed) return Promise.reject(new Error("The connection is closed"));
    return new Promise((resolve, reject) => { this.queue.push({ cmd, args, resolve, reject, timeout: opt.timeout || 1500 }); this.pump(); });
  }
  pump() {
    if (this.pending || !this.queue.length || this.closed) return;
    if (this.input.state === "disconnected" || this.output.state === "disconnected") { this.fail(new Error("The unit was disconnected")); return; }
    const r = this.queue.shift();
    const timer = setTimeout(() => { this.pending = null; r.reject(new Error(`No answer to command ${r.cmd} (timed out)`)); this.pump(); }, r.timeout);
    this.pending = { cmd: r.cmd, timer, resolve: r.resolve, reject: r.reject };
    try { this.output.send([240, 125, 70, 76, r.cmd, ...r.args, 247]); }
    catch (e) { clearTimeout(timer); this.pending = null; r.reject(e); this.pump(); }
  }
  fail(err) {
    for (const r of this.queue.splice(0)) r.reject(err);
    if (this.pending) { clearTimeout(this.pending.timer); this.pending.reject(err); this.pending = null; }
  }
  // the unit stops pushing 3 s after the last request: ping when idle
  keepAlive(ms = 1000) {
    clearInterval(this.timer);
    this.timer = setInterval(() => { if (!this.pending && !this.queue.length) this.request([25, []], { timeout: 1500 }).catch(() => {}); }, ms);
  }
  close() {
    this.closed = true; clearInterval(this.timer); this.input.onmidimessage = null; this.fail(new Error("The connection is closed")); this.onclose?.();
  }
}
