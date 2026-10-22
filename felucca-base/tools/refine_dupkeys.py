#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Patch refinement pass: a Python dict literal with the same key twice keeps the LAST one silently. Lists every sound of tools/duell_sounds.py
whose "p" dict names a key twice (a new "MOD3" next to an old one, a second "TWTYPE" ..) and every MOD slot that the SUBTLE block at the end of the
file would also take (it asserts and stops the import). Exit 1 when anything is found.  tools/refine_dupkeys.py"""
import ast, pathlib, sys
src = pathlib.Path(__file__).resolve().parent / "duell_sounds.py"
tree = ast.parse(src.read_text())
bad = 0
for node in ast.walk(tree):
    if isinstance(node, ast.Call) and getattr(node.func, "id", "") == "dict":
        kw = {k.arg: k.value for k in node.keywords if k.arg}
        if "name" in kw and isinstance(kw.get("p"), ast.Dict):
            seen = {}
            for k in kw["p"].keys:
                if isinstance(k, ast.Constant):
                    if k.value in seen:
                        print(f"duplicate key {k.value!r} in {kw['name'].value} (lines {seen[k.value]} and {k.lineno})"); bad = 1
                    seen[k.value] = k.lineno
print("refine_dupkeys:", "DUPLICATES FOUND" if bad else "no duplicate keys")
sys.exit(bad)
