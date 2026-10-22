#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Cloud job H: compare two outputs of pages_fp (the base, the work). Prints the screens that differ, by kind, and exits 1 when something other than
the footer's page counter changed on an EDIT or ENV page (family 2 / 3 numbers are printed by the caller's key: `fam:title`). Usage: pages_fp_diff.py BASE.txt NEW.txt [--allow-foot FAM,FAM]"""
import sys

def load(path):
    d = {}
    for ln in open(path, encoding='utf-8'):
        ln = ln.rstrip('\n')
        if not ln:
            continue
        eng, key, h = ln.split('|')
        d[(eng, key)] = h.split()
    return d

base, new = load(sys.argv[1]), load(sys.argv[2])
if '--head-only' in sys.argv:    # BASE = the work built with the A / B letter off (-DDUELL_TEST_NO_AB), NEW = the work as it is: only the header may differ (the letter, wave 5)
    diff = [k for k in sorted(set(base) | set(new)) if base.get(k) != new.get(k)]
    bad = [k for k in diff if k not in base or k not in new or base[k][1:] != new[k][1:]]
    print('the A / B letter (job G) changes the header of %d screens, nothing else of them (cards, graph, footer equal)' % (len(diff) - len(bad)))
    for k in bad:
        print('   !', k[0], k[1], 'differs in more than the header')
    sys.exit(1 if bad else 0)
allow = set()
if '--allow-foot' in sys.argv:
    allow = set(sys.argv[sys.argv.index('--allow-foot') + 1].split(','))
foot_only, other, added, removed = [], [], [], []
for k in sorted(set(base) | set(new)):
    if k not in base:
        added.append(k)
    elif k not in new:
        removed.append(k)
    elif base[k] != new[k]:
        b, n = base[k], new[k]
        if b[:3] == n[:3] and b[3] != n[3] and k[1].split(':')[0] in allow:
            foot_only.append(k)
        else:
            other.append((k, [i for i in range(4) if b[i] != n[i]]))
print('unchanged:', sum(1 for k in base if k in new and base[k] == new[k]), 'of', len(base))
print('changed in the footer only (the page counter):', len(foot_only))
for fam in sorted(allow):
    titles = sorted({k[1] for k in foot_only if k[1].startswith(fam + ':')})
    print('   family %s: %d pages (%s)' % (fam, len(titles), ', '.join(t.split(':', 1)[1] for t in titles)))
print('added screens (the new pages, on their engines):', len(added))
for t in sorted({k[1] for k in added}):
    print('   +', t, '(%d engines)' % sum(1 for k in added if k[1] == t))
print('removed screens:', len(removed))
for k in removed:
    print('   -', k)
# cloud job J (owner-approved page changes; WAVE_J.md section 7): the screens listed in tests/pages_expected_J.txt (one 'ENGINE TITLE' a line; '-' first: a removed screen) may change or go; anything else still fails
import os
exp = set()
ef = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pages_expected_J.txt')
if os.path.exists(ef):
    exp = set(l.rstrip('\n') for l in open(ef) if l.strip() and not l.startswith('#'))
known = [(k, p) for (k, p) in other if '%s %s' % (k[0], k[1]) in exp]
other = [(k, p) for (k, p) in other if '%s %s' % (k[0], k[1]) not in exp]
removed = [k for k in removed if '-%s %s' % (k[0], k[1]) not in exp]
print('changed by job J, listed in pages_expected_J.txt:', len(known))
print('changed in more than the footer:', len(other))
for (k, parts) in other:
    print('   !', k[0], k[1], [['head', 'cards', 'graph', 'foot'][i] for i in parts])
sys.exit(1 if other or removed else 0)
