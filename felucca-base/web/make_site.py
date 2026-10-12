#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Copyright (C) 2026 OSCILL/R (DUELL)
"""Make the DUELL site (GitHub Pages) as a static folder:

  index.html                  landing page (web/landing.html)
  emu/                        the browser emulator (the output of web/emu/build.sh: build/emu), or a
                              "not attached yet" page
  install/index.html          the installer page (web/install/index.html)
  install/duell-VER-beta.fwsc the package, when there is one, also as duell-latest.fwsc, with duell-version.json
                              {"version": "VER"} (VER is read from the file name; + LICENSE, LICENSING.md, LICENSES/: the
                              package holds JieLi SDK files under Apache-2.0, see LICENSING.md)
  fm1pkg.js fm1ota.js fm1backup.js installer_state.js   imported by the installer page as "../fm1*.js"
  .nojekyll

  web/make_site.py [--pkg FILE.fwsc] [--emu DIR] [OUT_DIR]

  OUT_DIR  default: site/ at the repository root
  --pkg    default: the newest build/duell-<VERSION>-beta.fwsc if there is one (any version in the name). Without a package the installer shows
           "Build not yet attached" (and still lets a tester choose a file).
  --emu    default: build/emu if it exists.
           Without it, /emu/ is a short page that says the emulator is not attached yet.

The package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files).
Its identity (FM-1_9xx) is read from the package.
"""
import argparse
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent          # felucca-base/web
BASE = HERE.parent                              # felucca-base
ROOT = BASE.parent                              # repository root
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


PKG_NAME = re.compile(r"duell-(\d+(?:\.\d+)+)(?:-beta)?\.fwsc")


def version_of(name):
    """the version in a package file name (duell-0.2.1-beta.fwsc -> (0, 2, 1)); None for other names"""
    m = PKG_NAME.fullmatch(name)
    return tuple(int(x) for x in m[1].split(".")) if m else None


def newest_package(folder):
    found = [p for p in folder.glob("duell-*.fwsc") if version_of(p.name)]
    return max(found, key=lambda p: version_of(p.name), default=None)


def check_package(pkg):
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca/DUELL package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:          # never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages "
                         "(a package patched from an official one carries vendor files)")
    return raw, product


def main():
    ap = argparse.ArgumentParser(description="Make the DUELL site folder")
    ap.add_argument("out", nargs="?", default=str(ROOT / "site"))
    ap.add_argument("--pkg")
    ap.add_argument("--emu")
    a = ap.parse_args()
    out = Path(a.out).resolve()
    pkg = Path(a.pkg) if a.pkg else (newest_package(BASE / "build") or BASE / "build" / "duell-none.fwsc")
    emu = Path(a.emu) if a.emu else (BASE / "build" / "emu")
    if a.pkg and not pkg.is_file():
        raise SystemExit(f"--pkg {pkg}: no such file")
    if a.emu and not (emu / "index.html").is_file():
        raise SystemExit(f"--emu {emu}: no index.html in it")
    if out in (ROOT, BASE, HERE) or out == Path(out.anchor):
        raise SystemExit(f"refusing to use {out} as the output folder")
    if out.exists():
        shutil.rmtree(out)
    inst = out / "install"
    inst.mkdir(parents=True)
    (out / ".nojekyll").write_text("", encoding="utf-8")
    shutil.copy(HERE / "landing.html", out / "index.html")
    for js in ("fm1pkg.js", "fm1ota.js", "fm1backup.js", "installer_state.js"):
        shutil.copy(HERE / js, out / js)

    # installer page (+ the package, when there is one)
    html = (HERE / "install" / "index.html").read_text(encoding="utf-8")
    note = "no package: the installer says \"Build not yet attached\""
    if pkg.is_file():
        raw, product = check_package(pkg)
        ver = version_of(pkg.name)
        if not ver:
            raise SystemExit(f"{pkg.name}: the file name must be duell-<version>-beta.fwsc (the version is read from it)")
        vtxt = ".".join(map(str, ver))
        shutil.copy(pkg, inst / pkg.name)
        if pkg.name != "duell-latest.fwsc":
            shutil.copy(pkg, inst / "duell-latest.fwsc")      # the page tries this name first
        (inst / "duell-version.json").write_text(json.dumps({"version": vtxt}) + "\n", encoding="utf-8")
        note = f"{pkg.name} ({len(raw)} B, {product}), version {vtxt}, also duell-latest.fwsc + duell-version.json"
    (inst / "index.html").write_text(html, encoding="utf-8")
    lic = BASE / "LICENSES"        # the package holds JieLi SDK files (Apache-2.0): the licence texts travel with it
    (inst / "LICENSES").mkdir()
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, inst / "LICENSES" / n)
    (inst / "LICENSES" / "index.html").write_text(     # Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>DUELL licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a> · '
          '<a href="https://github.com/oscillr/DUELL">Source code</a></p>\n', encoding="utf-8")
    for doc, src in (("LICENSE", ROOT / "LICENSE"), ("LICENSING.md", BASE / "LICENSING.md")):
        shutil.copy(src, inst / doc)

    # emulator
    if (emu / "index.html").is_file():
        shutil.copytree(emu, out / "emu", ignore=shutil.ignore_patterns("native.f32", "native_check*", ".DS_Store"))
        enote = f"from {emu}"
    else:
        (out / "emu").mkdir()
        (out / "emu" / "index.html").write_text(
            '<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">'
            "<title>DUELL emulator</title>"
            '<body style="font:16px/1.6 -apple-system,Helvetica,Arial,sans-serif;max-width:600px;margin:48px auto;padding:0 16px">'
            "<h1>DUELL</h1><p>MultiMorphing Bitimbral Synthesizer<br>by OSCILL/R</p>"
            "<p><b>The browser emulator is not attached yet.</b> It appears here when a release carries it.</p>"
            '<p><a href="../">Back</a> · <a href="https://github.com/oscillr/DUELL/releases">Releases</a> · '
            '<a href="https://github.com/oscillr/DUELL#try-it">Build it yourself</a></p></body>\n', encoding="utf-8")
        enote = "no emulator: placeholder page"
    print(f"site: {out}\n  installer: {note}\n  emu: {enote}")


if __name__ == "__main__":
    sys.exit(main())
