#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""What core 1 really reaches: the call graph of the compiled object (docs/TWOCORE_REACH_REAL.md).

  ram_reach.py build/felucca_ram.o tc_job_run fm1_c1_main

Follows every relocation (calls, addresses taken in code, pointers in data tables such as the engine descriptors) from the named roots, section by section, and
prints how many functions are reached, in which place they are (hot block, range X, range Z, flash) and how each flash one is reached. A function that hangs only on
the engine table (note_on, desc) is reached by pointer, not called by the job. Device bytes (the object's section sizes). Run it on a DUELL_TWOCORE=1 DUELL_RAMPLACE=1
build (OUT/felucca_ram.o; tools/build.py leaves it there)."""
import sys, struct, collections
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ram_place as rp, hot_fns, ram_fns_x, ram_fns_z
obj=open(sys.argv[1],'rb').read()
sh,base=rp._sections(obj)
names=[rp._cstr(obj,base+s[0]).decode() for s in sh]
size={n:s[5] for n,s in zip(names,sh)}
typ={n:s[1] for n,s in zip(names,sh)}
refs=collections.defaultdict(set)
for src,off,t,to,sym,add in rp.graph(obj):
    if to and to!=src: refs[src].add(to)
    elif not to and sym: refs[src].add('SYM:'+sym)
def sec_of(fn):
    for p in ('.text.','.ramx.','.ramz.',hot_fns.NEW.decode(),'.c1_text'):
        n=p+fn if p!='.c1_text' else p
        if n in size: return n
roots=[sec_of(r) for r in sys.argv[2:]]
seen=set(); st=list(roots)
while st:
    s=st.pop()
    if s in seen or s is None: continue
    seen.add(s)
    for t in refs[s]:
        if not t.startswith('SYM:'): st.append(t)
funcs=[s for s in seen if s.split('.')[1:2] and (s.startswith('.text.') or s.startswith('.ramx.') or s.startswith('.ramz.') or s.startswith(hot_fns.NEW.decode()) or s=='.c1_text')]
def cat(s):
    if s.startswith('.ramx.'): return 'X'
    if s.startswith('.ramz.'): return 'Z'
    if s.startswith(hot_fns.NEW.decode()): return 'hot'
    return 'flash'
tot=collections.Counter(); cnt=collections.Counter()
for s in funcs: tot[cat(s)]+=size[s]; cnt[cat(s)]+=1
print('functions reached',len(funcs),dict(cnt),dict(tot))
dat=[s for s in seen if s not in funcs]
dd=collections.Counter()
for s in dat:
    k=s.split('.')[1] if s.startswith('.') else s
    dd[k]+=size.get(s,0)
print('data sections',dict(dd))
unpl=sorted(((size[s],s) for s in funcs if cat(s)=='flash'),reverse=True)
print('unplaced flash funcs',len(unpl),sum(x for x,_ in unpl))
import json
for x,s in unpl[:60]: print(x,s)
print('--- how each unplaced function is reached (shortest path from tc_job_run)')
par={}
q=collections.deque(roots)
for r in roots: par[r]=None
while q:
    s=q.popleft()
    for t in sorted(refs[s]):
        if t.startswith('SYM:') or t in par: continue
        par[t]=s; q.append(t)
def nm(s): return s.lstrip('.').split('.',1)[-1] if '.' in s.lstrip('.') else s
for x,s in unpl:
    p=[];c=s
    while c: p.append(nm(c)); c=par[c]
    print(x, ' <- '.join(p[:6]))
