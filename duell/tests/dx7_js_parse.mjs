// usage: node js_parse.mjs EDITOR_HTML OUTFILE FILE.syx ...   (runs the editor's own FM6.parseSysex)
import { readFileSync, writeFileSync } from "node:fs";
import vm from "node:vm";
const [html_path, outp, ...files] = process.argv.slice(2);
const html = readFileSync(html_path, "utf8");
const proto = html.slice(html.indexOf("/*PROTO-BEGIN*/"), html.indexOf("/*PROTO-END*/"));
const E = vm.runInNewContext(proto + `;({FM6})`, { setTimeout, clearTimeout, setInterval, clearInterval, console });
const lines = [];
for (const f of files) {
  const r = E.FM6.parseSysex(new Uint8Array(readFileSync(f)));
  lines.push(`#${f}\tvoices=${r.voices.length}\tbadSum=${r.badSum}\tshort=${r.short}\tskipped=${r.skipped}`);
  r.voices.forEach((x, k) => {
    const pk = E.FM6.pack(x.v), back = E.FM6.unpack(pk);               // what the editor sends to the unit, and what the device reads back
    lines.push(`${k + 1}\t${Buffer.from(x.v).toString("hex")}\t${Buffer.from(back).toString("hex")}`);
  });
}
writeFileSync(outp, lines.join("\n") + "\n");
