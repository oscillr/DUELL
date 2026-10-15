#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DX7 compatibility analysis of the FM6 engine (tests/dx7_compat.c renders, tests/dx7_ref.c is the float reference).
   python3 tests/dx7_analyze.py SYXDIR RENDER_ROOT REF_ROOT JS_TSV OUT_JSON
Own parser of the 32-voice DX7 bulk format; checks the file (header, length, checksum, F7, 7-bit data), every parameter's
range, the engine's unpack (C) and the editor's (JS) against it, counts DX7 features, checks the renders, compares with
the reference.  Read-only on everything but OUT_JSON."""
import sys, os, glob, json, math, struct, hashlib
import numpy as np

SYX, REN, REF, JS, OUT = sys.argv[1:6]
FS = 44100
OPMAX = [99]*11 + [3, 3, 7, 3, 7, 99, 1, 31, 99, 14]          # R1-4 L1-4 BP LD RD LC RC RS AMS KVS OL MODE FC FF DET
OPNAME = "R1 R2 R3 R4 L1 L2 L3 L4 BP LD RD LC RC RS AMS KVS OL MODE FC FF DET".split()
VNAME = "PR1 PR2 PR3 PR4 PL1 PL2 PL3 PL4 ALG FB OKS LFS LFD LPMD LAMD LKS LFW LPMS TRNSP".split()
VMAX = [99]*8 + [31, 7, 1, 99, 99, 99, 99, 1, 5, 7, 48]
CURVES = ["-LIN", "-EXP", "+EXP", "+LIN"]
WAVES = ["TRI", "SAW-", "SAW+", "SQR", "SIN", "S&H"]

def unpack_raw(b):
    """packed 128 bytes -> (155-byte list unclamped from the bit fields, [(field, raw value, max)] out of range, bit7 count)"""
    v = [0]*155; bad = []
    hi = sum(1 for x in b if x >= 0x80)
    for k in range(6):
        o = b[17*k:17*k+17]; d = 21*k
        # the order of the 155-byte layout: R1-4 L1-4 BP LD RD | LC RC RS AMS KVS OL MODE FC FF DET (7-bit bytes masked as the device does)
        order = [x & 127 for x in o[:11]] + [o[11] & 3, o[11] >> 2 & 3, o[12] & 7, o[13] & 3, o[13] >> 2 & 7, o[14] & 127, o[15] & 1, o[15] >> 1 & 31, o[16] & 127, o[12] >> 3 & 15]
        for i, x in enumerate(order):
            v[d+i] = x
            if x > OPMAX[i]: bad.append((f"OP{6-k}.{OPNAME[i]}", x, OPMAX[i]))
    vb = [b[102+i] & 127 for i in range(8)] + [b[110] & 31, b[111] & 7, b[111] >> 3 & 1, b[112] & 127, b[113] & 127, b[114] & 127, b[115] & 127,
          b[116] & 1, b[116] >> 1 & 7, b[116] >> 4 & 7, b[117] & 127]
    for i, x in enumerate(vb):
        v[126+i] = x
        if x > VMAX[i]: bad.append((VNAME[i], x, VMAX[i]))
    for i in range(10):
        c = b[118+i] & 127; v[145+i] = c
        if c < 32 or c > 126: bad.append(("NAME", c, 126))
    return v, bad, hi

def clamp(v):
    o = list(v)
    for i in range(145):
        mx = OPMAX[i % 21] if i < 126 else VMAX[i-126]
        o[i] = min(o[i], mx)
    for i in range(145, 155):
        o[i] = 32 if o[i] < 32 or o[i] > 126 else o[i]
    return o

def name_of(v): return "".join(chr(c) for c in v[145:155]).rstrip()

def read_wav(p):
    d = open(p, "rb").read()
    return np.frombuffer(d[44:], dtype="<i2").astype(np.float64)

# ------------------------------------------------------------------ files
banks = []
jsmap = {}
cur = None
for ln in open(JS).read().splitlines():
    if ln.startswith("#"):
        cur = ln[1:].split("\t")[0]; jsmap[os.path.basename(cur)] = {}; cur = os.path.basename(cur)
    else:
        k, a, b = ln.split("\t"); jsmap[cur][int(k)] = (bytes.fromhex(a), bytes.fromhex(b))
rows = []
fileinfo = []
for path in sorted(glob.glob(os.path.join(SYX, "*.syx"))):
    base = os.path.basename(path)
    key = base[:-4].replace(" ", "_")
    d = open(path, "rb").read()
    hdr_ok = d[:6] == bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00]) or (d[:2] == b"\xf0\x43" and d[3:6] == bytes([9, 0x20, 0]))
    cs = (128 - (sum(d[6:4102]) & 127)) & 127
    info = dict(file=base, bank=key, size=len(d), header_ok=bool(hdr_ok), checksum_ok=cs == d[4102], end_f7=d[-1] == 0xF7,
                bit7_bytes=sum(1 for x in d[6:4102] if x >= 0x80))
    fileinfo.append(info)
    cu = {}
    for ln in open(os.path.join(REN, key, "unpacked.tsv")).read().splitlines():
        k, h = ln.split("\t"); cu[int(k)] = bytes.fromhex(h)
    rt = {}
    for ln in open(os.path.join(REN, key, "render.tsv")).read().splitlines():
        f = ln.split("\t"); rt[int(f[0])] = dict(name=f[1], note=int(f[2]), vel_in=int(f[3]), vel_seen=int(f[4]), peak32=int(f[5]), over=int(f[6]), active=int(f[7]))
    for k in range(1, 33):
        pk = d[6+128*(k-1):6+128*k]
        raw, bad, hi = unpack_raw(pk)
        v = clamp(raw)
        r = dict(bank=key, idx=k, name=name_of(v), packed=pk.hex(), v=v, bad=bad, hi=hi, rt=rt[k])
        r["c_match"] = bytes(v) == cu[k]
        jv = jsmap[base].get(k)
        r["js_present"] = jv is not None
        r["js_match"] = (jv is not None and bytes(v) == jv[0] and jv[0] == jv[1])
        rows.append(r)

# ------------------------------------------------------------------ features
def ops_of(v):   # op1..op6 as dicts
    res = []
    for n in range(1, 7):
        d = 21*(6-n)
        res.append({OPNAME[i]: v[d+i] for i in range(21)} | {"n": n})
    return res
ALG_CARRIERS = None
def features(v):
    ops = ops_of(v)
    V = {VNAME[i]: v[126+i] for i in range(19)}
    aud = [o for o in ops if o["OL"] > 0]
    f = {}
    f["peg"] = any(V[f"PL{i}"] != 50 for i in range(1, 5))
    lp = V["LPMS"] > 0 and V["LPMD"] > 0
    la = V["LAMD"] > 0 and any(o["AMS"] > 0 for o in aud)
    f["lfo_pitch"] = lp; f["lfo_amp"] = la
    act = lp or la
    f["lfo_any"] = act
    f["lfo_delay"] = act and V["LFD"] > 0
    f["lfo_sync"] = act and V["LKS"] == 1
    f["lfo_wave"] = WAVES[V["LFW"]] if act else None
    f["lfo_sh"] = act and V["LFW"] == 5
    f["lfo_nontri"] = act and V["LFW"] != 0
    ks = set()
    for o in aud:
        if o["LD"] > 0: ks.add(("L", CURVES[o["LC"]]))
        if o["RD"] > 0: ks.add(("R", CURVES[o["RC"]]))
    f["keyscale"] = bool(ks); f["keyscale_curves"] = sorted({c for _, c in ks})
    f["rate_scaling"] = any(o["RS"] > 0 for o in aud)
    f["fixed_freq"] = any(o["MODE"] == 1 for o in aud)
    f["oks_off"] = V["OKS"] == 0
    f["vel_sens"] = any(o["KVS"] > 0 for o in aud)
    f["ams"] = any(o["AMS"] > 0 for o in aud)
    f["transpose"] = V["TRNSP"] != 24
    f["detune"] = any(o["DET"] != 7 for o in aud)
    f["feedback"] = V["FB"] > 0
    f["alg"] = V["ALG"] + 1
    f["fb"] = V["FB"]
    f["ol_zero_ops"] = sum(1 for o in ops if o["OL"] == 0)
    return f
for r in rows:
    r["f"] = features(r["v"])

# ------------------------------------------------------------------ renders: sanity + reference comparison
def frames_rms(x, n=882):
    m = len(x)//n
    return np.sqrt((x[:m*n].reshape(m, n)**2).mean(axis=1))
def db(x): return 20*np.log10(np.maximum(x, 1e-9)/32768.0)
EDGES = [60, 120, 240, 480, 960, 1920, 3840, 7680, 15360, 22050]
def band_levels(x, c):
    seg = x[c-2048:c+2048]*np.hanning(4096)
    sp = np.abs(np.fft.rfft(seg))**2
    fr = np.fft.rfftfreq(4096, 1.0/FS)
    out = []
    for a, b in zip(EDGES[:-1], EDGES[1:]):
        e = sp[(fr >= a) & (fr < b)].sum()
        out.append(10*np.log10(max(e/ (np.hanning(4096)**2).sum() / 4096*2, 1e-18)) - 20*math.log10(32768.0)*1)  # approx dBFS of band power
    return np.array(out)
def peak_freq(x, c):
    seg = x[c-4096:c+4096]*np.hanning(8192)
    sp = np.abs(np.fft.rfft(seg)); fr = np.fft.rfftfreq(8192, 1.0/FS)
    i = int(np.argmax(sp[1:])+1)
    if 1 <= i < len(sp)-1:   # parabolic
        a, b, c2 = np.log(sp[i-1]+1e-9), np.log(sp[i]+1e-9), np.log(sp[i+1]+1e-9)
        dlt = 0.5*(a-c2)/(a-2*b+c2) if (a-2*b+c2) != 0 else 0
        return float((i+dlt)*FS/8192)
    return float(fr[i])

sigs = {}
for r in rows:
    nm = "".join(ch if ch.isalnum() else "_" for ch in r["name"]) or "VOICE"
    fw = read_wav(os.path.join(REN, r["bank"], f"{r['idx']:02d}-{nm}.wav"))
    ref = read_wav(os.path.join(REF, r["bank"], f"{r['idx']:02d}-{nm}.wav"))
    r["wav"] = f"{r['bank']}/{r['idx']:02d}-{nm}.wav"
    s = {}
    s["peak"] = float(np.abs(fw).max()); s["rms"] = float(np.sqrt((fw**2).mean()))
    s["dc"] = float(fw[FS//5:].mean())
    s["nan"] = bool(not np.isfinite(fw).all())
    s["silent"] = bool(s["peak"] < 100)
    s["clip"] = bool(r["rt"]["over"] > 0 or s["peak"] >= 32767)
    s["peak32"] = r["rt"]["peak32"]
    fr = frames_rms(fw); s["tail_db"] = float(db(fr[-3:].mean()) - db(fr.max())) if fr.max() > 0 else None
    s["alive_end"] = r["rt"]["active"]
    seg = fw[FS//10:FS*3//2]; sp = np.abs(np.fft.rfft(seg*np.hanning(len(seg))))**2; ff = np.fft.rfftfreq(len(seg), 1.0/FS)
    s["hf_frac"] = float(sp[ff > 8000].sum()/max(sp.sum(), 1e-9)); s["centroid"] = float((sp*ff).sum()/max(sp.sum(), 1e-9))
    s["dc_ratio"] = abs(s["dc"])/max(s["rms"], 1e-9)
    r["s"] = s
    # fingerprint
    c = int(0.6*FS)
    fp = band_levels(fw, c)
    sigs[(r["bank"], r["idx"])] = (hashlib.md5(fw.astype("<i2").tobytes()).hexdigest(), fp, db(frames_rms(fw, 4410)))
    # reference
    f = r["f"]
    covered = not (f["peg"] or f["lfo_any"])      # (AMS with AMD 0 does nothing on a DX7: covered)
    r["ref_covered"] = bool(covered and not r["bad"] and r["hi"] == 0)
    fr_a, fr_b = frames_rms(fw, 882), frames_rms(ref, 882)
    da, dbb = db(fr_a), db(fr_b)
    use = (np.maximum(da, dbb) > -50)
    # per 20 ms frame: the smallest difference to the other side's frame i-1, i, i+1 (a stage edge falling a frame apart is not a deviation), at most 40 dB
    sh = np.stack([np.abs(da - np.roll(dbb, k)) for k in (-1, 0, 1)]).min(axis=0)
    env = np.minimum(sh, 40.0)[use] if use.any() else np.array([0.0])
    wins = [int(0.15*FS), int(0.6*FS), int(1.2*FS), int(1.7*FS)]
    bd = []
    for c in wins:
        a, b = band_levels(fw, c), band_levels(ref, c)
        a, b = np.maximum(a, -80.0), np.maximum(b, -80.0)      # (floor: below -80 dBFS is rounding noise)
        m = np.maximum(a, b) > -62
        bd.append(float(np.abs(a-b)[m].max()) if m.any() else 0.0)
    pf_fw, pf_ref = peak_freq(fw, int(0.8*FS)), peak_freq(ref, int(0.8*FS))
    ok_p = fr_a[int(0.8*FS)//882] > 30 and fr_b[int(0.8*FS)//882] > 30
    r["cmp"] = dict(env_max=float(env.max()), env_mean=float(env.mean()), band_max=max(bd), band_each=bd,
                    level_db=float(db(np.sqrt((fw[FS//10:FS*3//2]**2).mean())) - db(np.sqrt((ref[FS//10:FS*3//2]**2).mean()))) if ref[FS//10:FS*3//2].std() > 0 else None,
                    cents=float(1200*math.log2(pf_fw/pf_ref)) if ok_p and pf_ref > 0 else None, ref_rms=float(np.sqrt((ref**2).mean())))

# duplicates / near-identical
keys = list(sigs)
dup_groups = {}
for k in keys:
    dup_groups.setdefault(sigs[k][0], []).append(k)
rec = {(r["bank"], r["idx"]): r for r in rows}
dups = []
for h, g in dup_groups.items():
    if len(g) > 1:
        same_rec = len({rec[k]["packed"] for k in g}) == 1
        dups.append(dict(keys=g, same_record=same_rec, silent=rec[g[0]]["s"]["silent"]))
near = []
for i in range(len(keys)):
    for j in range(i+1, len(keys)):
        a, b = keys[i], keys[j]
        if sigs[a][0] == sigs[b][0]: continue
        if rec[a]["s"]["silent"] or rec[b]["s"]["silent"]: continue
        fa, fb_ = sigs[a][1], sigs[b][1]
        ea, eb = sigs[a][2], sigs[b][2]
        n = min(len(ea), len(eb))
        if np.abs(fa-fb_).max() < 0.5 and np.abs(ea[:n]-eb[:n]).max() < 0.5:
            near.append((a, b, rec[a]["packed"] == rec[b]["packed"]))

json.dump(dict(files=fileinfo, rows=[{k: v for k, v in r.items()} for r in rows], dups=dups, near=near), open(OUT, "w"), default=str)
print("wrote", OUT, len(rows), "voices")
