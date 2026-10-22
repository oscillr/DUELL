/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 OSCILL/R (DUELL) */
/* The web editor's layout, from the firmware's own tables (through hostsim.c, like descdump.c): the page table PAGES[], the parameter
 * descriptors with every value formatted by param_format (so labels, ranges, value names and units cannot drift), each engine's
 * EDIT descriptors and the mode-dependent ones (engine desc hooks, the INSERT's A B C by TYPE, ENV SYNC, ARP TARGET), the matrix
 * lists in the shown order and their "why" reasons, the factory sounds and multis. web/editor/layout.json is this output; the host
 * test (web/editor/test_layout.mjs) compares the file with a fresh run.
 *   build/host/editordump > web/editor/layout.json */
#include "duell_version.h"
#define main hostsim_main
#include "hostsim.c"
#undef main

static void js_str(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        if (*s == '"' || *s == '\\')
            putchar('\\');
        putchar(*s);
    }
    putchar('"');
}

/* the formatted value tables, one per distinct (fmt, range, names, unit); a descriptor points at its table by index */
#define NSIG 1024
static struct { uint8_t fmt; int16_t min, max; const char *const *names; const char *unit; const param_desc_t *d; } sig[NSIG];
static int nsig;

static int fk_of(const param_desc_t *d)
{
    int i;
    if (d->fmt == F_ENUM || (d->fmt == F_INT && !d->names))
        return -1;
    for (i = 0; i < nsig; i++)
        if (sig[i].fmt == d->fmt && sig[i].min == d->min && sig[i].max == d->max && sig[i].names == d->names && sig[i].unit == d->unit)
            return i;
    sig[nsig].fmt = d->fmt; sig[nsig].min = d->min; sig[nsig].max = d->max; sig[nsig].names = d->names; sig[nsig].unit = d->unit;
    sig[nsig].d = d;
    return nsig++;
}

/* the shown order of an enum's values (what a knob steps), aliases left out */
static void js_order(const param_desc_t *d)
{
    int32_t v = d->def, n, first = 1;
    for (n = 0; n < 128; n++)
        v = param_turn(d, v, -1);
    putchar('[');
    for (n = 0; n < 128; n++) {
        int32_t w = param_turn(d, v, 1);
        printf("%s%d", first ? "" : ",", v);
        first = 0;
        if (w == v)
            break;
        v = w;
    }
    putchar(']');
}

static void js_desc(const param_desc_t *d)
{
    int32_t i;
    printf("{\"label\":");
    js_str(d->label);
    printf(",\"fmt\":%d,\"min\":%d,\"max\":%d,\"def\":%d,\"fk\":%d,\"unit\":", d->fmt, d->min, d->max, d->def, fk_of(d));
    js_str(d->unit ? d->unit : "");
    printf(",\"names\":");
    if (d->fmt == F_ENUM && d->names) {
        putchar('[');
        for (i = 0; i <= d->max - d->min; i++) {
            if (i)
                putchar(',');
            js_str(d->names[i]);
        }
        putchar(']');
        printf(",\"order\":");
        js_order(d);
    } else {
        printf("null,\"order\":null");
    }
    putchar('}');
}

static void js_descs(const param_desc_t *d, uint32_t n)
{
    uint32_t i;
    putchar('[');
    for (i = 0; i < n; i++) {
        if (i)
            printf(",\n  ");
        js_desc(&d[i]);
    }
    putchar(']');
}

static const char *const FAMN[] = {"HOME", "ENV", "LFO", "FX", "SCL", "EDIT", "GLO", "SAVE", "ARP", "SEQ", "TRK"};
static const char *const SCN[] = {"TRACK", "GLOBAL", "ENGINE", "STEP", "TRK", "MORPH", "MODE", "TEST", "SCALE", "MIDI", "PERF", "MODS", "FXSUM", "GMIX"};   /* (every SC_* of params.c, MODS and FXSUM too: an index past the table read whatever string lay after it) */
static const char *const GRN[] = {"NONE", "ADSR", "LFO", "STEPS", "ARP", "SCALE", "FX", "ROLL", "BROWSE", "SLOTS", "USER", "TRK", "SLCR", "MOD", "PATS",
                                  "TOOLS", "CHORD", "SLICES", "EVENTS", "FMOP", "FMOP2", "FMENV", "INS", "ENGINE", "MORPH", "MORPHSET", "MODE", "GLO",
                                  "MIDI", "MLEARN", "SCHAIN", "TEST", "TEMPO", "PERF", "MODS", "MSTG", "FXSUM", "EQ", "COMP", "FILT", "SUB", "CYCLE"};   /* (every GR_* of params.c: MODS was missing, so MODULES' name was whatever string lay after the table; 0.2.25: FXSUM was missing too, then EQ COMP FILT) */
_Static_assert(sizeof GRN / sizeof GRN[0] == GR_CYCLE + 1u && sizeof SCN / sizeof SCN[0] == SC_GMIX + 1u, "editordump: a name for every GR_* and SC_* (0.2.25)");

/* page_visible (ui.c), by the page's ids: which engines show the page. "all" or a list of engine names */
static void js_vis(const page_t *pg)
{
    uint32_t e, first = 1;
    printf("[");
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        int ok = 1;
        if (pg->graph == GR_FILT && pg->id[0] == P_E4)   /* VCF: ANALOG's own filter (its TYPE card is retired) */
            ok = en == &ENG_ANALOG;
        else if ((pg->id[0] == P_FREEA && pg->id[3] == P_FREED) || (pg->scope == SC_TRACK && pg->id[0] == P_KTRK))
            ok = en->ufilt != 0;
        else if (pg->scope == SC_TRACK && pg->id[0] == P_E4)
            ok = en == &ENG_TRIO;
        else if (pg->scope == SC_TRACK && pg->id[0] == P_E2)
            ok = en == &ENG_NOISE;
        else if (pg->graph >= GR_FMOP && pg->graph <= GR_FMENV)
            ok = e == ENGI_FM6;
        if (ok) {
            printf("%s%u", first ? "" : ",", e);
            first = 0;
        }
    }
    printf("]");
}

#include "duell_multis.h"                 /* DF_N, DF_SLOT, DM_AT, DM_REC (multi.c is not in hostsim) */
#define MU_NAME_AT 8u                      /* multi.c: the record's name */
#define PROJ_NAME_LEN 12u
/* the factory multis' names: the record's name at MU_NAME_AT */
/* (the same typedef as src/gfx.c: the generated build/gen/ui_palettes.h is the one table both read) */
typedef struct { const char *name; uint16_t bg[2], bgl, surf[2], text, mid, line[2], raise[2], rec, c[4], r[4], a[4], g, ga; uint8_t hc; } ui_pal_t;   /* (as gfx.c; 0.2.24: g / ga) */
#include "ui_palettes.h"
/* the UI palettes (src/gfx.c UI_PALETTES, RGB565 as "#rrggbb"): the editor takes the unit's COLOR setting (MENU id 0 = the palette index) and maps it to its own colours */
static void js_rgb(uint16_t c)
{
    unsigned r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
    printf("\"#%02x%02x%02x\"", (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
}
static void js_rgbs(const uint16_t *c, int n)
{
    int i;
    putchar('[');
    for (i = 0; i < n; i++) { if (i) putchar(','); js_rgb(c[i]); }
    putchar(']');
}
static void js_palettes(void)
{
    uint32_t i;
    printf("{\"DEFAULT\":%u,\"GREY\":%u,\"BW\":%u,\"DIM_PCT\":%d,\"LIST\":[", (unsigned)UI_DEFAULT_INDEX, (unsigned)UI_GREY_INDEX, (unsigned)UI_BW_INDEX, UI_DIM_PCT);
    for (i = 0; i < UI_NPALETTES; i++) {
        const ui_pal_t *p = &UI_PALETTES[i];
        printf("%s\n {\"name\":", i ? "," : "");
        js_str(p->name);
        printf(",\"bg\":"); js_rgbs(p->bg, 2);
        printf(",\"bgl\":"); js_rgb(p->bgl);
        printf(",\"surf\":"); js_rgbs(p->surf, 2);
        printf(",\"text\":"); js_rgb(p->text);
        printf(",\"mid\":"); js_rgb(p->mid);
        printf(",\"line\":"); js_rgbs(p->line, 2);
        printf(",\"raise\":"); js_rgbs(p->raise, 2);
        printf(",\"rec\":"); js_rgb(p->rec);
        printf(",\"c\":"); js_rgbs(p->c, 4);
        printf(",\"r\":"); js_rgbs(p->r, 4);
        printf(",\"a\":"); js_rgbs(p->a, 4);
        printf(",\"g\":"); js_rgb(p->g);                /* 0.2.24: the GLOBAL colour and its accent (the editor's global column) */
        printf(",\"ga\":"); js_rgb(p->ga);
        printf(",\"hc\":%d}", p->hc);
    }
    printf("]}");
}

static void js_multis(void)
{
    uint32_t i, k;
    printf("[");
    for (i = 0; i < DF_N; i++) {
        char nm[PROJ_NAME_LEN + 1u];
        for (k = 0; k < PROJ_NAME_LEN; k++)
            nm[k] = (char)DM_REC[DM_AT[i] + MU_NAME_AT + k];
        nm[PROJ_NAME_LEN] = 0;
        printf("%s[%u,", i ? "," : "", (unsigned)DF_SLOT[i]);
        js_str(nm);
        printf("]");
    }
    printf("]");
}

int main(void)
{
    uint32_t e, i, k, a, b;
    printf("{\"version\":");
    js_str(DUELL_VERSION_STR);
    printf(",\"P_COUNT\":%d,\"G_COUNT\":%d,\"P_E0\":%d,\"NSTEP\":%d,\"NENGINES\":%d,\"ESYNC_N\":%d,\n", P_COUNT, G_COUNT, P_E0, NSTEP, NENGINES, (int)ESYNC_N);
    printf("\"ID\":{\"P_LEVEL\":%d,\"P_PAN\":%d,\"P_MUTE\":%d,\"P_ESYNC\":%d,\"P_ATK\":%d,\"P_DEC\":%d,\"P_REL\":%d,\"P_E2ATK\":%d,\"P_E2DEC\":%d,\"P_E2REL\":%d,"
           "\"P_ITYPE\":%d,\"P_IA\":%d,\"P_IMIX\":%d,\"P_ATGT\":%d,\"P_DKSRC\":%d,\"P_DKRATE\":%d,\"P_DKTHR\":%d,\"P_CUT2\":%d,\"P_RES2\":%d,\"P_DRV2\":%d,\"P_TWTYPE\":%d,\"P_BLEND\":%d,\"P_DKDEP\":%d,\"P_DKATK\":%d,\"P_DKRLS\":%d,\"P_DKCRV\":%d,\"P_EQLG\":%d,\"P_EQMG\":%d,\"P_EQHG\":%d,\"P_EQLF\":%d,\"P_EQMF\":%d,\"P_EQMQ\":%d,\"P_EQHF\":%d,\"P_M4SRC\":%d,\"P_LSYNC\":%d,\"P_L2SYNC\":%d,"
           "\"P_M1SRC\":%d,\"P_M5SRC\":%d,\"P_FREEA\":%d,\"P_AMODE\":%d,\"P_SLEN\":%d,\"P_SDIV\":%d,\"P_SSWING\":%d,\"P_SGATE\":%d,\"P_SQNT\":%d,"
           "\"P_TRIM\":%d,\"P_LRATE\":%d,\"P_L2RATE\":%d,\"P_VOICE\":%d,\"P_GLIDE\":%d,\"P_GLMODE\":%d,\"P_SUS\":%d,\"P_E2SUS\":%d,\"P_ILAST\":%d,\"P_TRANS\":%d,\"P_KTRK\":%d,\"P_E2FLT\":%d,\"P_LD_FLT\":%d,\"P_ED_FLT\":%d,\"P_L2FLT\":%d,\"P_FREEC\":%d,\"P_ED_SHP\":%d,\"P_LD_SHP\":%d,\"P_E2SHP\":%d,\"P_L2SHP\":%d,"
           "\"G_BPM\":%d,\"G_ENGSEL\":%d,\"G_SLOT\":%d,\"G_ROUTE\":%d,\"G_RTYPE\":%d,\"G_DMODE\":%d,\"G_DMDEP\":%d,\"G_DMRATE\":%d,\"G_PINGSPREAD\":%d,\"G_SHPITCH\":%d,\"G_SHMOD\":%d,\"G_BM\":%d,\"G_CT\":%d,\"G_CR\":%d,\"G_CA\":%d,\"G_CL\":%d,\"G_WD\":%d},\n",
           P_LEVEL, P_PAN, P_MUTE, P_ESYNC, P_ATK, P_DEC, P_REL, P_E2ATK, P_E2DEC, P_E2REL, P_ITYPE, P_IA, P_IMIX, P_ATGT, P_DKSRC, P_DKRATE, P_DKTHR, P_CUT2, P_RES2, P_DRV2, P_TWTYPE, P_BLEND, P_DKDEP, P_DKATK, P_DKRLS, P_DKCRV, P_EQLG, P_EQMG, P_EQHG, P_EQLF, P_EQMF, P_EQMQ, P_EQHF, P_M4SRC,
           P_LSYNC, P_L2SYNC, P_M1SRC, P_M5SRC, P_FREEA, P_AMODE, P_SLEN, P_SDIV, P_SSWING, P_SGATE, P_SQNT, P_TRIM, P_LRATE, P_L2RATE, P_VOICE, P_GLIDE, P_GLMODE, P_SUS, P_E2SUS, P_IC, P_TRANS, P_KTRK, P_E2FLT, P_LD_FLT, P_ED_FLT, P_L2FLT, P_FREEC, P_ED_SHP, P_LD_SHP, P_E2SHP, P_L2SHP, G_BPM, G_ENGSEL, G_SLOT, G_ROUTE, G_RTYPE, G_DMODE, G_DMDEP, G_DMRATE, G_PINGSPREAD, G_SHPITCH, G_SHMOD, G_BM, G_CT, G_CR, G_CA, G_CL, G_WD);
    printf("\"FAM\":[");
    for (i = 0; i < NELEM(FAMN); i++) { if (i) putchar(','); js_str(FAMN[i]); }
    printf("],\n\"PAGES\":[");
    for (i = 0; i < NPAGES; i++) {
        const page_t *pg = &PAGES[i];
        printf("%s\n {\"title\":", i ? "," : "");
        js_str(pg->title);
        printf(",\"fam\":");
        js_str(FAMN[pg->fam]);
        printf(",\"scope\":");
        js_str(SCN[pg->scope]);
        printf(",\"graph\":");
        js_str(GRN[pg->graph]);
        printf(",\"ids\":[");
        for (k = 0; k < 4; k++)
            printf("%s%d", k ? "," : "", pg->id[k] == 0xFFu ? -1 : (int)pg->id[k]);
        printf("],\"engines\":");
        js_vis(pg);
        printf(",\"global\":%d}", page_global(pg));
    }
    printf("],\n\"TP\":");
    js_descs(TP, P_E0);
    printf(",\n\"GP\":");
    js_descs(GP, G_COUNT);
    printf(",\n\"MASTER\":");                    /* 0.2.19: the MASTER page's card, BASS MONO (pseudo id G_BM, past the stored globals: the editor reaches it as MENU setting 33) */
    js_desc(&GP[G_BM]);
    printf(",\n\"COMP\":");                      /* 0.2.21: the COMP page's four cards (pseudo ids G_CT .. G_CL: the editor reaches them as MENU settings 34..37) */
    js_descs(&GP[G_CT], 4);
    printf(",\n\"MSTG\":");                      /* 0.2.23: the WIDTH page's card (pseudo id G_WD: the editor reaches it as MENU setting 42; 0.2.25: the saturator's DRIVE is gone) */
    js_descs(&GP[G_WD], 1);
    {                                                   /* 0.2.22: the SIDECHAIN page's cards (the editor reaches them as MENU settings 38..41: P1 CH, P1 NOTE, P2 CH, P2 NOTE): the channel 1..16 and the note (0 = ANY) */
        static const param_desc_t SC_CH_D = PD("CH", F_INT, 1, 16, SC_CH_DEF + 1);
        printf(",\n\"SIDECH\":[");
        js_desc(&SC_CH_D);
        putchar(',');
        js_desc(&SC_NOTE_D);
        putchar(']');
    }
    printf(",\n\"DMD\":{\"chance\":");                   /* the DELAY MODE: DEPTH is CHANCE in RANDOM / OCT UP (params.c glob_desc); RATE is faded in those two and SPREAD in DIGITAL, with the mode's card name */
    js_desc(&DMD_CHANCE);
    printf(",\"why\":[");
    for (k = 0; k < DMD_N; k++) {
        if (k) putchar(',');
        js_str(N_DMODE[k]);
    }
    printf("],\"full\":[");                               /* the modes in full (the header, the editor's menu) */
    for (k = 0; k < DMD_N; k++) {
        if (k) putchar(',');
        js_str(N_DMODE_FULL[k]);
    }
    printf("],\"chanceDef\":%d}", DMD_CHANCE_DEF);       /* (the CHANCE a switch into RANDOM / OCT UP starts with when it is 0: params.c dly_mode_changed) */
    printf(",\n\"ENG\":[");
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        printf("%s\n {\"name\":", e ? "," : "");
        js_str(en->name);
        printf(",\"ufilt\":%d,\"atgt\":%d,\"ownenv\":%d,\"titles\":[", en->ufilt, en->atgt, en->ownenv);
        js_str(en->page_title[0]);
        putchar(',');
        js_str(en->page_title[1]);
        printf("],\"knob\":[%d,%d,%d,%d],\"uiord\":[", en->knob[0], en->knob[1], en->knob[2], en->knob[3]);
        for (k = 0; k < 8; k++)                         /* the order the EDIT 1 / EDIT 2 cards show the E slots in (engine_t.uiord, 0.2.18: FM6 shows SLOT first) */
            printf("%s%d", k ? "," : "", en->uiord ? en->uiord[k] : k);
        printf("],\"edit\":");
        js_descs(en->edit, 8);
        printf(",\"why\":[");                           /* mod_e_why of E1..E8 */
        for (k = 0; k < 8; k++) {
            const char *w = mod_e_why(e, k);
            printf("%s", k ? "," : "");
            if (w) js_str(w); else printf("null");
        }
        printf("],\"dyn\":[");                          /* the desc hook: [E1 value, E2 value or -1 (any), k, descriptor] */
        if (en->desc) {
            int first = 1;
            for (a = 0; a < 128; a++) {
                track_t t;
                const param_desc_t *ref[8];
                int any = 0;
                if ((int32_t)a < en->edit[0].min || (int32_t)a > en->edit[0].max) continue;
                for (k = 0; k < 8; k++) {
                    int same = 1, has = 0;
                    const param_desc_t *d0 = 0;
                    for (b = 0; b < 128; b++) {
                        const param_desc_t *d;
                        if ((int32_t)b < en->edit[1].min || (int32_t)b > en->edit[1].max) continue;
                        memset(&t, 0, sizeof t);
                        t.p[P_E0] = (int16_t)a; t.p[P_E1] = (int16_t)b;
                        d = en->desc(&t, k);
                        if (!has) { d0 = d; has = 1; }
                        else if (d != d0) same = 0;
                        ref[k] = d;
                        if (!same) break;
                    }
                    if (same && d0) {
                        printf("%s[%u,-1,%u,", first ? "" : ",\n  ", a, k); first = 0; js_desc(d0); printf("]"); any = 1;
                    } else if (!same) {
                        for (b = 0; b < 128; b++) {
                            const param_desc_t *d;
                            if ((int32_t)b < en->edit[1].min || (int32_t)b > en->edit[1].max) continue;
                            memset(&t, 0, sizeof t);
                            t.p[P_E0] = (int16_t)a; t.p[P_E1] = (int16_t)b;
                            d = en->desc(&t, k);
                            if (d) { printf("%s[%u,%u,%u,", first ? "" : ",\n  ", a, b, k); first = 0; js_desc(d); printf("]"); any = 1; }
                        }
                    }
                }
                (void)any; (void)ref;
            }
        }
        printf("]");
        printf(",\"npresets\":%u,\"sounds\":[", (unsigned)ds_count(e));
        for (k = 0; k < ds_count(e); k++) {
            const ds_sound_t *s = ds_get(e, k);
            printf("%s[", k ? "," : "");
            js_str(s->name);
            printf(",%u]", (unsigned)s->cat);
        }
        printf("]}");
    }
    printf("],\n\"ORDER\":[");
    for (k = 0; k < NENG_SHOWN; k++)
        printf("%s%u", k ? "," : "", (unsigned)eng_vis(k));
    printf("],\n\"SLOTS\":%u,\n\"BROWSE\":[", (unsigned)UP_SLOTS);   /* 0.2.17: slot i holds the factory sound at browse position i: [engine, factory index] */
    for (k = 0; k < ds_list_n(); k++) {
        uint32_t fk, fe = ds_list_at(k, &fk);
        printf("%s[%u,%u]", k ? "," : "", (unsigned)fe, (unsigned)fk);
    }
    printf("],\n\"INS\":[");                            /* the INSERT's A B C by TYPE 0..8 */
    for (i = 0; i < NELEM(N_ITYPE); i++) {
        printf("%s[", i ? "," : "");
        for (k = 0; k < 3; k++) {
            if (k) putchar(',');
            js_desc(ins_desc((int32_t)i, k));
        }
        putchar(']');
    }
    printf("],\n\"ESYNC\":");
    js_descs(ESYNC_DESC, 6);
    printf(",\n\"ATG\":");
    js_descs(ATG_DESC, ATG_KINDS);
    printf(",\n\"MD\":{\"E1\":%d,\"INSA\":%d,\"IMIX\":%d,\"RES\":%d,\"PAN\":%d,\"BUS\":%d,\"N\":%d,\"SHP\":%d,\"E2SHP\":%d,\"FMODE\":%d,\"AATK\":%d,"
           "\"CUT2\":%d,\"RES2\":%d,\"DRIVE\":%d,\"BLEND\":%d,\"SUBLVL\":%d,\"S_CYCLE\":%d},\n", MD_E1, MD_INSA, MD_IMIX, MD_RES, MD_PAN, MD_BUS, MD_N, MD_SHP, MD_E2SHP, MD_FMODE, MD_AATK,
           MD_CUT2, MD_RES2, MD_DRIVE, MD_BLEND, MD_SUBLVL, MS_CYCLE);
    {   /* 0.2.25 (mod.c mod_dst_removed, mod_e_inert, mod_shp_inert): the destinations the DEST knob does not offer. rm: removed for every engine (the EQ gains);
         * eng: per engine [index, [its removed list slots as DST values], the slot that picks the wave (-1: none), [[wave value, [DST values the wave leaves without effect]], ..]]
         * (SHP and E2SHP in a wave's list: the SHAPE depth cards hide too) */
        uint32_t e, k;
        int32_t v, d, first = 1;
        printf("\"MXH\":{\"rm\":[");
        for (d = 1; d < MD_N; d++)
            if (!MD_ENGINE(d) && mod_dst_removed(0, d))
                printf("%s%d", first ? "" : ",", d), first = 0;
        printf("],\"eng\":[");
        for (e = 0; e < NENGINES; e++) {
            const engine_t *en = ENGINES[e];
            int32_t key = en == &ENG_LOFI ? 1 : (en == &ENG_ANALOG || en == &ENG_TRIO) ? 0 : -1;
            printf("%s[%u,[", e ? "," : "", (unsigned)e);
            for (k = 0, first = 1; k < 8u; k++)
                if (mod_dst_removed(e, MD_E1 + (int32_t)k))
                    printf("%s%d", first ? "" : ",", MD_E1 + (int32_t)k), first = 0;
            printf("],%d,[", key);
            if (key >= 0)
                for (v = en->edit[key].min; v <= en->edit[key].max; v++) {
                    track_t t;
                    memset(&t, 0, sizeof t);
                    t.eng_req = (uint8_t)e;
                    t.p[P_E0 + key] = (int16_t)v;
                    printf("%s[%d,[", v > en->edit[key].min ? "," : "", v);
                    first = 1;
                    if (mod_shp_inert(&t))
                        printf("%d,%d", MD_SHP, MD_E2SHP), first = 0;
                    for (k = 0; k < 8u; k++)
                        if (mod_e_inert(&t, k))
                            printf("%s%d", first ? "" : ",", MD_E1 + (int32_t)k), first = 0;
                    printf("]]");
                }
            printf("]]");
        }
        printf("]},\n");
    }
    printf("\"MULTIS\":");
    js_multis();
    printf(",\n\"PALETTES\":");
    js_palettes();
    {   /* cloud job I: the extension slots (ext.h, params.c XTP: wide id XW_BASE + k, protocol scope 2). XID: the slot numbers by name; XTP: their descriptors (the stored range; X15 is the spare, label "-");
         * XSYNC: CYCLE RISE and FALL with SYNC on (note values, as ESYNC); TW: the TWIN FILTER's one-value-two-meanings (params.c tw_desc): CUTOFF 2 is OFFSET (LINK on), CUTOFF (LINK off) or TUNE (COMB),
         * RES 2 is RES or FEEDBACK (COMB, label FDBK) */
        static const char *const XN[] = {"X_ROUTE", "X_LINK", "X_KTRK2", "X_ENV2", "X_LFO1", "X_LFO2", "X_SUBLVL", "X_SUBOCT", "X_SUBWAVE", "X_SUBHARM",
                                         "X_CYMODE", "X_CYRISE", "X_CYFALL", "X_CYCURVE", "X_CYSYNC"};
        _Static_assert(sizeof XN / sizeof XN[0] == X_CYSYNC + 1u, "editordump: a name for every extension slot");
        printf(",\n\"NX\":%d,\"XW_BASE\":%d,\"XID\":{", NX, XW_BASE);
        for (k = 0; k < NELEM(XN); k++)
            printf("%s\"%s\":%u", k ? "," : "", XN[k], (unsigned)k);
        printf("},\n\"XTP\":");
        js_descs(XTP, NX);
        printf(",\n\"XSYNC\":");
        js_descs(XTP_SYNC, 2);
        printf(",\n\"TW\":{\"OFFSET\":");
        js_desc(&TWD_OFFSET);
        printf(",\"CUTOFF\":");
        js_desc(&TWD_CUTOFF);
        printf(",\"TUNE\":");
        js_desc(&TWD_TUNE);
        printf(",\"RES\":");
        js_desc(&TWD_RES);
        printf(",\"FDBK\":");
        js_desc(&TWD_FDBK);
        printf(",\"COMB\":%d}", (int)TT_COMB);
    }
    printf(",\n\"FMTS\":[");
    for (i = 0; i < (uint32_t)nsig; i++) {                 /* (after every descriptor: fk_of filled the table) */
        int32_t v;
        printf("%s\n [", i ? "," : "");
        for (v = sig[i].min; v <= sig[i].max; v++) {
            char val[16];
            const char *unit;
            param_format(sig[i].d, v, val, &unit);
            printf("%s[", v > sig[i].min ? "," : "");
            js_str(val);
            putchar(',');
            js_str(unit);
            putchar(']');
        }
        putchar(']');
    }
    printf("]}\n");
    return 0;
}
