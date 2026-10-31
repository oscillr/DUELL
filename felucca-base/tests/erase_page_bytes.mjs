// Runs the installer's real erase code (web/fm1backup.js BackupConnection + eraseAllData, as install/index.html eraseAfterInstall
// calls it) against a fake MIDI port; prints the bytes the page sends, as hex.
const base = process.argv[2] || new URL("..", import.meta.url).href.replace(/\/$/, "");
const { BackupConnection, eraseAllData } = await import(base + "/web/fm1backup.js");
const sent = [];
const input = { state: "connected", onmidimessage: null };
const output = { state: "connected", send(b) { sent.push(b); setTimeout(() => input.onmidimessage({ data: Uint8Array.from([240,125,70,76,80,0,247]) }), 5); } };
const link = new BackupConnection(input, output);
const rc = await eraseAllData((r, o) => link.request(r, o));
console.error("page: frames sent", sent.length, "reply rc", rc);
console.log(sent[0].map((x) => x.toString(16).padStart(2, "0")).join(" "));
