/* SPDX-License-Identifier: GPL-3.0-only */
/* tests/mixsplit_test.c's reference: fx.c's mix_part and mix_block as they were BEFORE the MIXSPLIT refactor (commit 16cccc4, the snapshot of the
 * 0.2.25 test line), copied verbatim, only the two names changed (ref_mix_part, ref_mix_block; ref_mix_block calls ref_mix_part). They call the
 * firmware's own helpers (events_block, track_render, mix_spread, ...), which the refactor did not touch. Do not edit: this is the
 * "old combined path" the staged one is compared with. Included by mixsplit_test.c after fx.c is part of the build.
 *
 * Job 2 (partstate) changed where the helpers keep their scratch and take their inputs, so the text below was adapted in the places marked
 * "(partstate ...)" and in these mechanical ways only: part_buf, pfx_dry and spread_on are the part's pscr[pp] fields; mod is MODW(t); mod_gon is
 * mod_gon_any(); eq_live is the pair of t->eqlv; the helpers that now take the block's snapshot (pfx_snap, pfx_mode, pfx_mix, bal_lvl, the mute functions) get ref_bs, filled by
 * blk_snap before the parts; mix_spread and space_tap add into an accumulator that is added into the shared arrays at once, which is what the old
 * += did; split_stash (no longer in the firmware) lives here. The arithmetic and the order are the old ones: tests/partindep_test.c pins this file
 * to the hashes of the commit before the change (OLD_HASH).
 *
 * join0233: the text below also carries wip/editor0232's (T0229) changes to mix_part, in the same verbatim way (dist_follow's new signature, SAT's blend ramp, the matrix's mix ramp
 * with its per-part state), so that this stays the combined path of the joined line. OLD_HASH still pins the job-1 sound for the scenes those changes do not touch.
 *
 * hunt1 (release_drafts/CLICK_HUNT1.md): the SPREAD branch calls fx.c's spread_run and spread_post (SAT's blend ramp and the gains in a line on a SPREAD part), the plain and the silent
 * branches write what the firmware's do (sp_go / sp_ok 0): the comparison with mix_block stays one of the ORDER of the stages, not of the sound of a moving gain. */

/* one synth part into the dry mix and the sends; a part with no voice sounding costs
 * the LFO tick and a cleared buffer only (after the DIST tail has run out) */
static part_acc_t ref_acc;
static uint8_t ref_on;
static int32_t ref_dist_rw0;                            /* (join0233: A's dist_rw0 of mix_part, the followed SAT before this block's step) */
/* (join0233: wip/editor0232's mix_ramp verbatim, adding into the shared arrays as the old combined mix did; the state it reads is fx.c's per-part mx_lv / mx_sd) */
static __attribute__((noinline)) int32_t ref_mix_ramp(const int32_t *b, uint32_t n, uint32_t pp, int32_t pk, int32_t gl, int32_t gr, int32_t lvl,
                                                      int32_t c, int32_t d, int32_t r)
{
    int32_t gl0 = pan_gv[pp] ? pan_g[pp][0] : gl, gr0 = pan_gv[pp] ? pan_g[pp][1] : gr, ni = (int32_t)n, i, xmax = 1;
    int32_t glq = gl0 << 16, grq = gr0 << 16, dgl = ((gl - gl0) << 16) / ni, dgr = ((gr - gr0) << 16) / ni;
    int32_t lq = mx_lv[pp] << 8, dl = ((lvl - mx_lv[pp]) << 8) / ni;
    int32_t cq = mx_sd[pp][0] << 8, dq = mx_sd[pp][1] << 8, rq = mx_sd[pp][2] << 8;
    int32_t dc = ((c - mx_sd[pp][0]) << 8) / ni, dd = ((d - mx_sd[pp][1]) << 8) / ni, dr = ((r - mx_sd[pp][2]) << 8) / ni;
    for (i = 0; i < 3; i++)
        xmax = mx_sd[pp][i] > xmax ? mx_sd[pp][i] : xmax;
    xmax = c > xmax ? c : xmax;
    xmax = d > xmax ? d : xmax;
    xmax = r > xmax ? r : xmax;
    xmax = 0x7FFFFFFF / (xmax | 1);
    for (i = 0; i < ni; i++) {
        int32_t x, a, xs, l, sc, sd, sr, gli, gri;
        lq += dl; cq += dc; dq += dd; rq += dr; glq += dgl; grq += dgr;
        if (i == ni - 1) {
            l = lvl, sc = c, sd = d, sr = r, gli = gl, gri = gr;
        } else {
            l = lq >> 8, sc = cq >> 8, sd = dq >> 8, sr = rq >> 8, gli = glq >> 16, gri = grq >> 16;
        }
        x = ((b[i] >> 2) * l) >> 10;
        a = x < 0 ? -x : x;
        xs = clamp(x, -xmax, xmax);
        if (a > pk)
            pk = a;
        if (sc)
            send_c[i] += mulq15(xs, sc);
        if (sd)
            send_d[i] += mulq15(xs, sd);
        if (sr)
            send_r[i] += mulq15(xs, sr);
        mix_l[i] += (x * gli) >> 12;
        mix_r[i] += (x * gri) >> 12;
    }
    return pk;
}
static blockstate_t ref_bs;                             /* (partstate: the helpers below take the block's snapshot; the old code read the variables, which are the same values) */
static void ref_mix_part(track_t *t, uint32_t n)
{
    uint32_t i, fxm, pp = (uint32_t)(t - trk) % NLIVE;
    int32_t *b = pscr[pp].buf;
    int32_t pan_st = t->p[P_PAN], pan_mv, pq;            /* (0.2.24: the STORED pan before the matrix writes the block's; pan_follow) */
    int32_t dist_st = t->p[P_DIST];                      /* (0.2.25: the STORED SAT likewise; dist_follow) */
    mod_begin(t);                                       /* the matrix's per-block values into t->p (mod.c) */
    pscr[pp].spread_on = t->p[P_SPRD] > 0 && (trk_vmode(t) == V_POLY || trk_vmode(t) == V_UNISON) && !dm_split;   /* (SPREAD,
                                                         * mix_spread; SPLIT: none, the part is mono on its side) */
    if (track_render(t, b, n))
        t->tail = t->p[P_ITYPE] ? 64 : 16;              /* blocks of DIST / INSERT state to run out after the last
                                                         * voice (the INSERT's: 46 ms, a flanger's feedback) */
    else if (!t->tail || !(t->p[P_DIST] | t->p[P_ITYPE] | t->p[P_EQLG] | t->p[P_EQMG] | t->p[P_EQHG]) ||
             pfx_x[(uint32_t)(t - trk) % NLIVE] == 32768 || !--t->tail) {   /* (0.2.14 MODULES PART FX OFF: no DIST / INSERT tail to run out) */
        if (trk[0].eqlv | trk[1].eqlv)                  /* (the old eq_live byte: any part) */
            eq_rest(t);                                 /* (the EQ's states clear with the tail) */
        pfx_snap(t, &ref_bs);                                    /* (PART FX: what is asked, at once: nothing sounds) */
        if (!(mods_off & MODS_PFX)) {
            if (t->p[P_DKSRC] == 1 || trk[(uint32_t)(t - trk) ^ 1u].p[P_DKSRC] == 1)
                duck_part(t, 0, n, &ref_bs);                     /* (SRC PART: a silent key, and the part keyed by the other) */
            if (duck_st[(uint32_t)(t - trk) % NTRK].ph)
                duck_run(t, 0, 0, n);                   /* (the DUCK's envelope runs on) */
        }
        if (t->ins_run && !t->p[P_ITYPE])
            ins_rest(t);                                /* TYPE OFF while silent: no fade-out left for the next note */
        if ((pf.mute >> (t - trk)) & 1u)
            perf_mute_rest((uint32_t)(t - trk), ref_bs.mute_tg[pp]);        /* (PERFORM's MUTE: silent, its gain where it goes) */
        pan_follow(pp, pan_st, 1, 0);                   /* (silent: the pan at once) */
        dist_follow(pp, t->p[P_DIST], 1);               /* (.. and the SAT) (join0233: wip/editor0232's) */
        pan_gv[pp] = 0;
        mx_ok[pp] = 0;
        sp_go[pp] = 0, sp_ok[pp] = 0;                   /* (hunt1) */
        mx_was[pp] = mod_mixmv[pp];
        if (MODW(t).on)
            mod_end(t);
        return;
    }
    pan_mv = pan_follow(pp, pan_st, 0, pscr[pp].spread_on);
    pq = pan_x[pp] + ((t->p[P_PAN] - pan_st) << 8);     /* the pan this block mixes at, Q8: the smoothed stored pan plus the matrix's offset as it was */
    {
        int32_t dw0 = dist_x[pp], dlv = dist_live[pp];  /* (join0233: wip/editor0232's SAT blend ramp state) */
        t->p[P_DIST] = (int16_t)dist_follow(pp, t->p[P_DIST], 0);   /* the SAT this block runs at (put back below; mod_end puts the stored back too) */
        ref_dist_rw0 = dlv ? dw0 : dist_x[pp];
    }
    fxm = pfx_mode(t, &ref_bs);                                  /* (0.2.14 MODULES PART FX: PFX_RUN as ever, PFX_FADE, PFX_SKIP) */
    if (fxm != PFX_SKIP && (t->p[P_DKSRC] == 1 || trk[(uint32_t)(t - trk) ^ 1u].p[P_DKSRC] == 1))
        duck_part(t, b, n, &ref_bs);                             /* DUCK SRC PART: this block's level for the other part, its key for this */
    if (pscr[pp].spread_on) {
        mixpart_t rm;                                   /* (partstate: mix_spread takes the stage context and adds into the part's own accumulators; they are added into the shared arrays here, which is what the old += did) */
        memset(&rm, 0, sizeof rm);
        rm.t = t, rm.b = b, rm.n = n, rm.pp = pp, rm.acc = &pacc[pp], rm.scr = &pscr[pp], rm.bs = &ref_bs;
        acc_prep(&rm);
        rm.dist_rw0 = ref_dist_rw0;
        spread_run(&rm, fxm, pq);                       /* (hunt1: fx.c's SPREAD mix: SAT's blend ramp around mix_spread, then the gains in a line (spread_post, which mix_gather calls for the firmware's path) */
        spread_post(pp, n, &ref_bs);
        for (i = 0; i < n; i++) {
            mix_l[i] += rm.acc->l[i], mix_r[i] += rm.acc->r[i];
            send_c[i] += rm.acc->sc[i], send_d[i] += rm.acc->sd[i], send_r[i] += rm.acc->sr[i];
            if (sp_tap && (pacc_on[pp] & 16u))
                sp_fc[i] += rm.acc->fc[i], sp_fd[i] += rm.acc->fd[i], sp_fr[i] += rm.acc->fr[i];
        }
        mx_was[pp] = mod_mixmv[pp];
    } else {
        if (fxm == PFX_FADE)
            memcpy(pscr[pp].dry, b, n * sizeof b[0]);        /* (PART FX changing: the dry block for the crossfade) */
        if (fxm != PFX_SKIP) {
            if (ref_dist_rw0 != dist_x[pp] && (ref_dist_rw0 < 12 * 256 || dist_x[pp] < 12 * 256))
                track_dist_ramp(t, b, n, ref_dist_rw0, dist_x[pp]);
            else
                track_dist(t, b, n);
            if (t->p[P_ITYPE] | t->ins_run)
                track_insert(t, b, n);                  /* the INSERT (1.5): off, nothing */
            if (EQ_ON(t))
                track_eq(t, b, n, 0u);                  /* the EQ (DUELL): all bands at 0 dB, nothing */
        }
        if ((pf.mute >> (t - trk)) & 1u) {              /* PERFORM: MUTE 1 / 2 held (perform.c) */
            if (fxm == PFX_FADE)
                pfx_mute((uint32_t)(t - trk), b, 0, n, ref_bs.mute_tg[pp]);
            else
                perf_mute((uint32_t)(t - trk), b, n, ref_bs.mute_tg[pp]);
        }
        if (fxm != PFX_SKIP && duck_st[(uint32_t)(t - trk) % NTRK].ph)
            duck_run(t, b, 0, n);                       /* the DUCK (DUELL): before the level, pan and the sends */
        if (fxm == PFX_FADE)
            pfx_mix(t, b, 0, n, &ref_bs);
    }
    if (!pscr[pp].spread_on) {                                   /* (the mix's values read after the calls: none is held over
                                                         * them, no spill in the loop: the ISR's cost as before 1.5) */
        int32_t lvl = bal_lvl(t, part_lvl(t), mix_bal);
        int32_t gl = 4096 - (pq > 0 ? pq >> 2 : 0), gr = 4096 + (pq < 0 ? pq >> 2 : 0);   /* (pq Q8: pq >> 2 = pan x 64, the gains of before) */
        int32_t c = t->p[P_CHOR] * 258, d = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak;
        int ramp = 0;
        if (snd_st == SND_OFF)
            c = d = r = 0;                              /* (0.2.14 MODULES SENDS OFF: nothing sent) */
        if (dm_split) {                                 /* SPLIT: part 1 left, the others right, PAN not heard */
            gl = t == trk ? 4096 : 0;
            gr = 4096 - gl;
        } else if (pan_mv) {                            /* (0.2.24: the stored pan on its way: the gains in a straight line from the block before's) */
            ramp = pan_gv[pp] && (pan_g[pp][0] != gl || pan_g[pp][1] != gr);
        }
        int32_t xmax = c > d ? c : d;
        xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);   /* sends: loud chords at a high LEVEL */
        if ((((uint32_t)mod_mixmv[pp] | mx_was[pp]) & 1u) && mx_ok[pp] &&
            (mx_lv[pp] != lvl || mx_sd[pp][0] != c || mx_sd[pp][1] != d || mx_sd[pp][2] != r || (pan_gv[pp] && (pan_g[pp][0] != gl || pan_g[pp][1] != gr)))) {
            pk = ref_mix_ramp(b, n, pp, pk, gl, gr, lvl, c, d, r);
        } else if (ramp) {
            int32_t glq = pan_g[pp][0] << 16, grq = pan_g[pp][1] << 16, dgl = ((gl - pan_g[pp][0]) << 16) / (int32_t)n, dgr = ((gr - pan_g[pp][1]) << 16) / (int32_t)n;
            for (i = 0; i < n; i++) {
                int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x;
                int32_t xs = clamp(x, -xmax, xmax);
                if (a > pk)
                    pk = a;
                if (c)
                    send_c[i] += mulq15(xs, c);
                if (d)
                    send_d[i] += mulq15(xs, d);
                if (r)
                    send_r[i] += mulq15(xs, r);
                glq += dgl;
                grq += dgr;
                mix_l[i] += (x * (glq >> 16)) >> 12;
                mix_r[i] += (x * (grq >> 16)) >> 12;
            }
        } else
        for (i = 0; i < n; i++) {
            int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x;   /* pre-shift: 8 loud voices */
            int32_t xs = clamp(x, -xmax, xmax);         /* sends: mulq15 would overflow */
            if (a > pk)
                pk = a;
            if (c)
                send_c[i] += mulq15(xs, c);
            if (d)
                send_d[i] += mulq15(xs, d);
            if (r)
                send_r[i] += mulq15(xs, r);
            mix_l[i] += (x * gl) >> 12;
            mix_r[i] += (x * gr) >> 12;
        }
        pan_g[pp][0] = gl;
        pan_g[pp][1] = gr;
        pan_gv[pp] = (uint8_t)!dm_split;
        mx_lv[pp] = lvl, mx_sd[pp][0] = c, mx_sd[pp][1] = d, mx_sd[pp][2] = r;
        mx_ok[pp] = 1;
        sp_go[pp] = 0, sp_ok[pp] = 0;                   /* (hunt1: a plain block ends SPREAD's line; the firmware's spread_post sees sp_go 0 in mix_gather) */
        mx_was[pp] = mod_mixmv[pp];
        t->peak = pk;
        if (sp_tap) {
            memset(&ref_acc, 0, sizeof ref_acc);        /* (partstate: space_tap adds into an accumulator, which goes into the shared floors here) */
            space_tap(b, lvl, c, d, r, n, &ref_acc, &ref_on);    /* (SPACE right: the part's send floors) */
            for (i = 0; i < n; i++)
                sp_fc[i] += ref_acc.fc[i], sp_fd[i] += ref_acc.fd[i], sp_fr[i] += ref_acc.fr[i];
        }
    }
    t->p[P_DIST] = (int16_t)dist_st;                    /* (the stored SAT back; mod_end below also does) */
    if (MODW(t).on)
        mod_end(t);                                     /* the stored values back */
}

/* SPLIT: part 1's sends (mix_part put them into send_*) to bus A's, send_* cleared for the other parts (bus B) */
static __attribute__((noinline)) void split_stash(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        sa_c[i] = send_c[i];
        sa_d[i] = send_d[i];
        sa_r[i] = send_r[i];
        send_c[i] = send_d[i] = send_r[i] = 0;
    }
    if (sp_tap)                                         /* (SPACE's floors: part 1's to bus A's) */
        for (i = 0; i < n; i++) {
            sp_fc_a[i] = sp_fc[i], sp_fd_a[i] = sp_fd[i], sp_fr_a[i] = sp_fr[i];
            sp_fc[i] = sp_fd[i] = sp_fr[i] = 0;
        }
}

static void ref_mix_block(int32_t *out, uint32_t n)
{
    uint32_t i;
    int perf;
    int32_t mg = MIX_MG(), mg0 = mg;   /* (the device: the default level, MASTER scales the DAC after; host tests: fx.c MIX_MG) */
    for (i = 0; i < n; i++)
        send_c[i] = send_d[i] = send_r[i] = mix_l[i] = mix_r[i] = 0;
    events_block(n);
    if (dm_cur != duell_mode || dm_g != DM_ONE || dm_clr)
        dm_step();                                      /* DUELL step 5: the MODE changing (the dip) */
    if (snd_st != SND_ON || (mods_off & MODS_SENDS) || snd_panic)
        snd_step();                                     /* DUELL 0.2.14 MODULES SENDS: OFF, fading, clearing its lines */
    sp_tap = snd_st != SND_OFF && (perf_space > 0 || sv.spc > 0);   /* (SPACE right of the middle, or gliding there: the parts' dry mix for its floor) */
    if (sp_tap)
        memset(sp_fc, 0, sizeof sp_fc), memset(sp_fd, 0, sizeof sp_fd), memset(sp_fr, 0, sizeof sp_fr);
    if (perf_idle()) {                                  /* PERFORM (perform.c) idle: nothing but its 1/16's clock */
        perf_tick(n);
        perf = 0;
    } else {
        perf = perf_begin(n);
    }
    blk_snap(&ref_bs, sp_tap);
    for (i = 0; i < NLIVE; i++) {                      /* (DUELL: tracks 3..4 do not render) */
        int mo = morph_begin(&trk[i], 1);               /* DUELL MORPH: the fader's values into t->p (morph.c) */
        ref_mix_part(&trk[i], n);
        if (mo)
            morph_end(&trk[i]);                         /* .. and the stored ones back */
        if (!i && dm_split && snd_st != SND_OFF)
            split_stash(n);                             /* (SPLIT: part 1's sends to bus A) */
    }
    if (mod_gon_any())                                        /* 0.2.7: the parts' matrix offsets on the bus values (mod.c mod_bus): DUEL bus A part 1's, */
        mod_bus(dm_split ? 1u : ~0u);                   /* (split_buses: bus B part 2's), STEREO / LAYER the shared buses both parts' added */
    sv.spc0 = sv.spc;                                   /* (the width's ramp starts where the last block ended) */
    if (space_goal() || sv.spc) {                       /* PERFORM's K2 SPACE (above; in the middle and at rest: nothing; SENDS OFF: to the middle) */
        space_step();
        if (snd_st != SND_OFF && (sv.on || sv.sg0 != 32768)) {
            space_sends(send_c, send_d, send_r, sp_fc, sp_fd, sp_fr, n);
            if (dm_split)
                space_sends(sa_c, sa_d, sa_r, sp_fc_a, sp_fd_a, sp_fr_a, n);   /* (DUEL: bus A's, part 1's) */
        }
    } else if (sv.on || sv.sg != 32768) {
        sv.on = 0;                                      /* (back in the middle: the stored values again, no gain left) */
        sv.sg = sv.sg0 = 32768;
        sv.ac = sv.ad = sv.ar = 0;
    }
    if (snd_st == SND_FADE) {                           /* (MODULES SENDS changing: the dry mix kept, snd_fade below) */
        memcpy(snd_dl, mix_l, n * sizeof mix_l[0]);
        memcpy(snd_dr, mix_r, n * sizeof mix_r[0]);
    }
    if (dm_hold || snd_st == SND_OFF) {                 /* (the dip's silence: the lines being cleared; MODULES SENDS OFF: no bus) */
        for (i = 0; i < n; i++)
            wet[i] = 0;
    } else if (dm_split) {
        split_buses(n);
    } else {
        fx_buses(send_c, send_d, send_r, wet, n);
        if (hl.side)
            rev_side_mix(mix_l, mix_r, n);              /* HALL: stereo */
        if (dly_side)
            dly_side_mix(mix_l, mix_r, n);              /* PING-PONG: the repeats left and right */
    }
    if (snd_st == SND_FADE)
        snd_fade(n);                                    /* (MODULES SENDS: the buses' share of the mix toward OFF or ON) */
    if (mod_gw)
        mod_bus_end();                                  /* (the stored bus values back: the main loop never sees a modulated one) */
    if (meter_on) {
        meter_on--;
        if (dm_split)
            meter_scan(n);                              /* (GLO's page shows: SPLIT's two buses) */
    }
    if (scope_mix)
        scope_take_mix(n);
    if (perf) {
        perf_master(n);                                 /* (then the dip and master_out as ever, the level done) */
        mg = PERF_MG;
    }
    if (!dm_split && sw_on())
        space_width(n);                                 /* SPACE's stereo width (STEREO, LAYER; never DUEL: its sides are the two parts) */
    if (dm_g != DM_ONE || dm_dir)
        dm_ramp(n);
    if (!dm_split && (wd_sel != 100u || wdn.k))
        width_stage(n);                                 /* DUELL 0.2.23 WIDTH, the master stereo width (STEREO, LAYER; not in DUEL; 100 %: this test only) */
    if (!dm_split && (bm_sel || bm.a))
        bass_mono(n);                                   /* DUELL 0.2.19 BASS MONO (STEREO, LAYER; not in DUEL; OFF: this test only) */
    if (!dm_split && ((cmp_cfg & 0xF00u) || cmp.a))
        comp_stage(n, perf ? cmp_poff(mg0) : 0);        /* DUELL 0.2.21 COMP, the master compressor (STEREO, LAYER; not in DUEL; RATIO 1:1: this test only; PERFORM's stage has applied MASTER: poff) */
    if (dm_split) {
        split_master(out, n, mg);
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t l = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        int32_t r = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
        master_out(&l, &r);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
}
