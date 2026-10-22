/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca menu (HOME held): its rows in tabs (1.0.5: DISPLAY CONTROL AUDIO SYSTEM; 1.2: MIDI before SYSTEM; SELECT
 * walks the rows across the tabs, SPEC 1.1a; no scrolling), the rows in menu_items.c (the settings, shared with the editor's
 * MENU_DESC / MENU_SET; 1.2: TUNE and MIDI IN, the project's). No BACK row: HOME closes it (ui_input.c). The two-valued
 * rows are bits of ui_prefs (MENU_FLAGS). In ABOUT KNOB 1 scrolls through all credits and SELECT jumps between its sections (0.2.9;
 * OCT- back to the tabs).
 * ui.menu: 1 the tabs, 2 ABOUT (information and credits: the firmware, the USB link and the CPU readings, which INFO showed before
 * DUELL 0.2.2), 3 TEST (DUELL 0.2.2: the CPU test screen, MENU > SYSTEM > TEST; it was GLO's TEST page, then the INFO item). */
/* ------------------------------------------------------------ menu --- */
/* (the rows, their values and names, usb_serial_apply: menu_items.c) */


/* DUELL CPU LOAD METER (MENU > SYSTEM > TEST: the full card, graph_test below; ABOUT keeps a line). The audio ISR (audio.c) adds each I2S half's
 * render time to cpu_sum / cpu_n, keeps the longest in cpu_pk and counts the halves that missed their time (cpu_miss); this
 * reads them once a second, only while TEST or ABOUT is shown (cpu_meter_poll from their signature), so nothing runs for it
 * otherwise. AVERAGE = the sum / (halves x the half's time) of the last second; PEAK = the longest half of the last second,
 * held (the highest of the last CPU_HOLD_MS, then it falls to the latest); MISSED = halves over their time or overtaken
 * by the DMA since power-on. Colours (fixed, flat): below 80 % the theme, 80 % and up amber (NEAR LIMIT), 95 % and up or a
 * miss in the last 5 s red (the word says it too: MONO has no colours). Emulator: FELUCCA_METER_EST, a voice-count
 * ESTIMATE (audio.c), shown as such */
#define CPU_WIN_MS 1000u
#define CPU_HOLD_MS 4000u
#define CPU_RECENT_MS 5000u
#define CPU_WARN 80u
#define CPU_HOT 95u
static struct {
    uint16_t avg, peak, hold;                          /* % (peak and hold up to 999) */
    uint32_t miss, miss_t, t, hold_t, n0, sum0;        /* the misses seen, when the last one was seen, window start, ... */
    uint32_t base;                                     /* TEST's RESET: the misses counted from here (ABOUT: since power-on) */
    uint8_t seen;                                      /* a miss was ever seen (miss_t valid) */
} cpum;
static void cpu_meter_open(void)                       /* TEST / ABOUT opened: a fresh window (the IIR load until it is over) */
{
    cpum.n0 = cpu_n;
    cpum.sum0 = cpu_sum;
    cpu_pk = 0;
    cpum.t = fm1_ms;
    cpum.avg = cpum.peak = cpum.hold = (uint16_t)(song.cpu_q8 * 100u / 256u);
    cpum.hold_t = fm1_ms;
    cpum.miss = cpu_miss;
    cpum.seen = 0;
}
static void cpu_meter_poll(void)                       /* once a window, and only when asked */
{
    uint32_t n, sum, pk;
    if (fm1_ms - cpum.t < CPU_WIN_MS)
        return;
    do {
        n = cpu_n;
        sum = cpu_sum;
    } while (n != cpu_n);
    pk = cpu_pk;
    cpu_pk = 0;
    cpum.t = fm1_ms;
    if (n != cpum.n0)
        cpum.avg = (uint16_t)clamp((int32_t)((uint64_t)(sum - cpum.sum0) * 100u / ((uint64_t)(n - cpum.n0) * CPU_BUDGET_US)), 0, 999);
    cpum.n0 = n;
    cpum.sum0 = sum;
    cpum.peak = (uint16_t)clamp((int32_t)((uint64_t)pk * 100u / CPU_BUDGET_US), 0, 999);
    if (cpum.peak >= cpum.hold || fm1_ms - cpum.hold_t >= CPU_HOLD_MS) {
        cpum.hold = cpum.peak;
        cpum.hold_t = fm1_ms;
    }
    if (cpu_miss != cpum.miss) {
        cpum.miss = cpu_miss;
        cpum.miss_t = fm1_ms;
        cpum.seen = 1;
    }
}
static uint32_t cpu_recent(void) { return cpum.seen && fm1_ms - cpum.miss_t < CPU_RECENT_MS; }
static uint32_t cpu_level(void)                        /* 0 fine, 1 near the limit, 2 at it or blocks missed */
{
    return cpu_recent() || cpum.avg >= CPU_HOT ? 2u : cpum.avg >= CPU_WARN ? 1u : 0u;
}
static const char *cpu_word(void)
{
    return cpu_recent() ? "BLOCKS MISSED JUST NOW" : cpum.avg >= CPU_HOT ? "AT THE LIMIT" : cpum.avg >= CPU_WARN ? "NEAR THE LIMIT" : "OK";
}
static uint16_t cpu_col(uint32_t lvl, uint16_t bg)     /* flat colours; MONO: the text colour */
{
    if (lvl == 0u)                                     /* (High Contrast's theme colour is red: no warning colour then) */
        return UI_PALETTES[ux.pal % NPALETTES].hc ? T_TEXT : vt(T_THEME, bg);
    if (ux.mono || ux.bw)
        return T_TEXT;
    return lvl == 2u ? T_REC : ux.light ? 0xB2C0u : 0xFD20u;
}
static uint32_t cpu_sig(void)                          /* what a redraw depends on */
{
    return cpum.avg * 7u + cpum.hold * 31u + cpum.miss * 211u + cpum.base * 4099u + cpu_level() * 1009u + 1u;
}
static void cpu_pct(char *d, uint32_t v)               /* "37 %" */
{
    fmt_int(d, (int32_t)v);
    str_cpy(d + str_len(d), " %", 3);
}

/* https://hugelton.github.io/Felucca, QR version 3, correction M, generated by CoreImage.
 * One row per word, leftmost module in bit 0; the constant matrix needs no encoder or RAM. */
static const uint32_t ABOUT_QR[29] = {
    0x1FCD247Fu, 0x1059C541u, 0x1745765Du, 0x1758E85Du, 0x174E575Du, 0x104C3A41u,
    0x1FD5557Fu, 0x000B7000u, 0x091BCA55u, 0x1270DC8Au, 0x1DB4375Au, 0x083E9FA1u,
    0x1A63124Eu, 0x12513981u, 0x1AE2524Du, 0x0A2E78A0u, 0x1A7F4FC3u, 0x167154B0u,
    0x19A228D5u, 0x0A561C2Au, 0x01F10B4Du, 0x1D1B3F00u, 0x1B5A467Fu, 0x1B13F641u,
    0x11F24D5Du, 0x1D1B625Du, 0x1389315Du, 0x08DFD641u, 0x1813977Fu,
};

static void draw_about_qr(int32_t x, int32_t y)
{
    uint32_t row, col;
    cv_rect(x, y, 74, 74, UI_QR_LIGHT);             /* fixed light / dark, independent of the palette */
    for (row = 0; row < 29u; row++)
        for (col = 0; col < 29u; col++)
            if ((ABOUT_QR[row] >> col) & 1u)
                cv_rect(x + 8 + (int32_t)col * 2, y + 8 + (int32_t)row * 2, 2, 2, UI_QR_DARK);
}

/* All notices are in flash. Wrap at the font's actual width, without dropping URL
 * characters; the same layout computes the scroll limit and draws clipped bands.
 * Mixed case here (long text); S throughout, the title in L. */
static uint32_t about_t0;                              /* ABOUT opened (fm1_ms): its animation's clock */
static uint32_t about_step(void)                       /* the animation's step (ANIM OFF: the first frame, still); 0 off the first view */
{
    return ui.menu == 2 && !ui.menu_scroll && !(ui_prefs & PREF_ANIM_OFF) ? (fm1_ms - about_t0) / SPL_STEP_MS : 0u;
}
#define MENU_DOC_H 196                                /* the document: screen rows 24..220 */
enum { MC_TEXT, MC_TITLE, MC_URL };
typedef struct { const char *text; uint8_t kind; } menu_credit_t;
static const menu_credit_t MENU_CREDITS[] = {
    {"H\xFCgelton Instruments", MC_TITLE},  /* Felucca's own: above; these are ours too */
    {"Leo Kuroshita", MC_TEXT},
    {"CrispyZebra (PHASE waveforms): GPL-3.0", MC_TEXT},
    {"Fukiai icon font: MIT", MC_TEXT},
    {"https://github.com/hugelton", MC_URL},
    {"", MC_TEXT},
    {"KLATTSCH", MC_TITLE},
    {"Tony Gies / MIT", MC_TEXT},
    {"https://github.com/tgies/klattsch", MC_URL},
    {"", MC_TEXT},
    {"DAISYSP", MC_TITLE},
    {"Electrosmith, Emilie Gillet", MC_TEXT},
    {"MIT", MC_TEXT},
    {"https://github.com/electro-smith/DaisySP", MC_URL},
    {"", MC_TEXT},
    {"DEXED + MSFA", MC_TITLE},
    {"Pascal Gauthier, Google Inc.", MC_TEXT},
    {"msfa: Apache-2.0, Dexed: GPL-3.0", MC_TEXT},
    {"https://github.com/asb2m10/dexed", MC_URL},
    {"", MC_TEXT},
    {"RINGS", MC_TITLE},
    {"Emilie Gillet / MIT", MC_TEXT},
    {"https://github.com/pichenettes/eurorack", MC_URL},
    {"", MC_TEXT},
    {"CHOMPI CLUB", MC_TITLE},
    {"The PLATE reverb and the delay modes RAND and OCT+ are based on the CHOMPI firmware (MIT)", MC_TEXT},
    {"https://github.com/CHOMPI-Club/CHOMPI", MC_URL},
    {"", MC_TEXT},
    {"INTER TIGHT", MC_TITLE},
    {"The Inter Project Authors", MC_TEXT},
    {"SIL Open Font License 1.1", MC_TEXT},
    {"https://github.com/rsms/inter-tight", MC_URL},
    {"", MC_TEXT},
    {"VOICE", MC_TITLE},
    {"Dennis H. Klatt (1980)", MC_TEXT},
    {"https://doi.org/10.1121/1.383940", MC_URL},
    {"Hillenbrand, Getty, Clark and Wheeler (1995)", MC_TEXT},
    {"https://doi.org/10.1121/1.411872", MC_URL},
    {"", MC_TEXT},
    {"WAVE USER TABLES", MC_TITLE},                /* DUELL 0.2.14: the editor's WAV converter (web/editor/wavetable.js); ideas, no code */
    {"Cooley and Tukey (1965): the FFT", MC_TEXT},
    {"https://doi.org/10.1090/S0025-5718-1965-0178586-1", MC_URL},
    {"Xfer Serum: wavetable WAV files", MC_TEXT},
    {"of 2048 sample frames (Vital, Surge)", MC_TEXT},
    {"", MC_TEXT},
    {"FM VOICE", MC_TITLE},                        /* DUELL 0.2.17: FM BASS 2 (FM6 factory patch F9), a DX7 voice import */
    {"One FM voice: aquanodemusic (Aquanode Soundscapes), CC0 1.0 Universal", MC_TEXT},
    {"https://github.com/aquanodemusic/aquanode-soundscapes", MC_URL},
#if DUELL_VELO
    {"", MC_TEXT},
    {"VELO", MC_TITLE},                            /* KEY VELOCITY and aftertouch (sense.c): the method of Velo, built on Felucca; in builds with KEY SENSE, where the method is */
    {"Charles Vestal: key velocity and aftertouch from the keys (method)", MC_TEXT},
    {"GPL-3.0-only", MC_TEXT},
    {"https://github.com/charlesvestal/fm1-velocity", MC_URL},
#endif
};

static int32_t menu_text(const char *s, int32_t y, uint16_t col, int draw)
{
    while (*s) {
        char line[64];
        uint32_t n = 0, space = 0, take, prev = 0;
        int32_t pen = 0;                                 /* 1/16 px, as text_w adds it up */
        while (s[n] && n < sizeof line - 1u) {
            uint32_t c = (uint8_t)s[n];
            line[n] = s[n];
            line[n + 1u] = 0;
            pen += (prev ? kern(&AF_S, prev, c) : 0) + AF_S.g[glyph(&AF_S, c)].adv;
            prev = c;
            if ((pen + 8) >> 4 > 226)
                break;
            if (s[n] == ' ')
                space = n;
            n++;
        }
        take = s[n] && space ? space : n;
        if (!take) take = 1;                         /* progress even for an unusually wide glyph */
        line[take] = 0;
        if (draw && y + AF_S.h > 0 && y < MENU_DOC_H)
            cv_text(6, y, &AF_S, line, col);
        y += 18;
        s += take;
        while (*s == ' ') s++;
    }
    return y;
}

/* ABOUT's USB line (what INFO's USB row showed: the old SYSTEM page's MIDI card; OFF no USB, WAIT no bus yet, BUS frames seen, ENUM being set
 * up, MIDI in use). The CPU line is the meter above */
static uint32_t info_usb(void)                         /* 0 OFF, 1 WAIT, 2 BUS, 3 ENUM, 4 MIDI */
{
    return !usb.up ? 0u : usb.config ? 4u : usb.setups ? 3u : usb.sof_seen ? 2u : 1u;
}
static int32_t about_sec[2];                           /* ABOUT's CREDITS and LICENSES headings, from the document's top (menu_document) */
static int32_t menu_document(int32_t y, int draw)
{
    uint32_t i;
    int32_t y0 = y;
    if (draw) {
        char b[16];
        uint16_t col = cpu_col(cpu_level(), T_BG);
        cv_text(6, y + 2, &AF_L, "DUELL", vt(T_THEME, T_BG));
        cv_text(6, y + 36, &AF_S, "MultiMorphing Bitimbral Synthesizer", T_TEXT);
        cv_text(6, y + 54, &AF_M, "by OSCILL/R", T_TEXT);
        cv_text(6, y + 78, &AF_M, FELUCCA_VERSION, vt(T_THEME, T_BG));
        cv_text_r(232, y + 81, &AF_S, __DATE__, T_MID, T_BG);
        cv_text(6, y + 100, &AF_S, "CPU", T_MID);                 /* the meter: the 1 s average, the held peak, the misses */
        cpu_pct(b, cpum.avg);
        cv_text(34, y + 100, &AF_S, b, col);
        cv_text(84, y + 100, &AF_S, "PEAK", T_MID);
        cpu_pct(b, cpum.hold);
        cv_text(118, y + 100, &AF_S, b, T_TEXT);
        if (cpum.miss) {
            fmt_int(b, (int32_t)(cpum.miss > 9999u ? 9999u : cpum.miss));
            str_cpy(b + str_len(b), " MISSED", 8);
            cv_text_r(232, y + 100, &AF_S, b, cpu_recent() ? cpu_col(2u, T_BG) : T_MID, T_BG);
        }
        cv_text(6, y + 118, &AF_S, "USB", T_MID);                /* the link (INFO's USB row): OFF, WAIT, BUS, ENUM or MIDI */
        cv_text(34, y + 118, &AF_S, (const char *const[5]){"OFF", "WAIT", "BUS", "ENUM", "MIDI"}[info_usb()], vt(T_THEME, T_BG));
        if (ovl_lite)                                   /* 0.2.25: the overload guard is engaged (fx.c ovl_step: RANDOM / OCT UP as the plain delay) */
            cv_text_r(232, y + 118, &AF_S, "LIGHT FX", cpu_col(1u, T_BG), T_BG);
#if DUELL_RAMPLACE
        {   /* two-core stage 1a: part 2's voice code in RAM (ramplace.c): OK and its size, or FAIL and the range that differs from the
             * flash image (checked once a second here, about_sig); BOOT COPY n: the start needed n more copies */
            cv_text(6, y + 136, &AF_S, "RAM CODE", T_MID);
            if (!rp_state) {
                fmt_int(b, (int32_t)((ramplace_bytes() + 512u) / 1024u));
                str_cpy(b + str_len(b), " KB", 4);
                cv_text(78, y + 136, &AF_S, "OK", vt(T_THEME, T_BG));
                cv_text(104, y + 136, &AF_S, b, T_TEXT);
            } else
                cv_text(78, y + 136, &AF_S, rp_state == 1u ? "FAIL X" : rp_state == 2u ? "FAIL Z" : "FAIL X Z", cpu_col(2u, T_BG));
            if (rp_boot_copies) {
                str_cpy(b, "BOOT COPY ", 11);
                fmt_int(b + str_len(b), rp_boot_copies);
                cv_text_r(232, y + 136, &AF_S, b, cpu_col(1u, T_BG), T_BG);
            }
            y += 18;                                    /* (the lines below one row down; with draw 0 too: below) */
        }
#endif
        cv_text(6, y + 142, &AF_S, "based on Felucca (GPL-3.0)", T_TEXT);
        cv_text(6, y + 160, &AF_S, "(C) 2026 Leo Kuroshita", T_TEXT);
        cv_text(6, y + 178, &AF_S, "H\xFCgelton Instruments", T_TEXT);
        cv_text(6, y + 202, &AF_S, "hugelton.github.io", T_MID);
        cv_text(6, y + 220, &AF_S, "/Felucca", T_MID);
        cv_text(6, y + 244, &AF_S, "hugelton.com", T_MID);
        draw_about_qr(158, y + 186);
    }
#if DUELL_RAMPLACE
    if (!draw)
        y += 18;                                        /* (the RAM CODE line's row, as drawn) */
#endif
    about_sec[0] = y + 280 - y0;
    y = menu_text("CREDITS", y + 280, T_THEME, draw) + 8;
    for (i = 0; i < NELEM(MENU_CREDITS); i++) {
        const menu_credit_t *c = &MENU_CREDITS[i];
        const char *text = c->text;
        /* Display host/path once; the full source URL stays in the flash table. */
        if (c->kind == MC_URL && str_len(text) >= 8u && !memcmp(text, "https://", 8))
            text += 8;
        if (!text[0]) y += 8;
        else y = menu_text(text, y, c->kind == MC_TITLE ? T_THEME : c->kind == MC_URL ? T_MID : T_TEXT, draw);
    }
    about_sec[1] = y + 12 - y0;
    y = menu_text("LICENSES + SOURCE", y + 12, T_THEME, draw);
    y = menu_text("Fonts and icons: their licences travel with the source and the release files.", y, T_TEXT, draw);
    y = menu_text("NO WARRANTY", y, T_TEXT, draw);
    y = menu_text("Full notices in source:", y + 8, T_TEXT, draw);
    y = menu_text("github.com/hugelton/Felucca", y, T_MID, draw);
    return y + 4;
}
static int32_t menu_scroll_max(void)
{
    int32_t h = menu_document(0, 0);
    return h > MENU_DOC_H ? h - MENU_DOC_H : 0;
}
/* SELECT in ABOUT (0.2.9): the next (s > 0) or the previous section's start: the animation (0), the information (1: the document's top),
 * CREDITS, LICENSES + SOURCE (each at most the end of the scroll); stops at the first and the last, no wrap */
static int32_t about_section(int32_t cur, int32_t s)
{
    int32_t max = menu_scroll_max(), t[4], k, n = 4;
    t[0] = 0, t[1] = 1, t[2] = about_sec[0] < max ? about_sec[0] : max, t[3] = about_sec[1] < max ? about_sec[1] : max;
    if (s > 0) {
        for (k = 0; k < n; k++)
            if (t[k] > cur)
                return t[k];
        return cur;
    }
    for (k = n - 1; k >= 0; k--)
        if (t[k] < cur)
            return t[k];
    return cur;
}

static void menu_dots(void);
/* the menu's header (0..24): its icon and title, the tabs' dots; no keycap (the one top bar of every page) */
static void menu_head(void)
{
    cv_begin(240, H_HEAD, T_BG);
    GFX_HOOK_ALIGN(0, HEAD_MY + AF_M_CAP_Y, 0, HEAD_MY + AF_M_CAP_Y + AF_M_CAP_H, AL_V, "header icon on its title's line");
    cv_icon_mid(8, H_HEAD / 2, 16, ui.menu == 2 ? ICON_X_INFO : ICON_X_COG, T_THEME, T_BG);   /* (as draw_head's) */
    GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
    {   /* the title in the top middle: centred on x 120 between the cog and the dots */
        const char *t = ui.menu == 2 ? "ABOUT / CREDITS" : "MENU";
        int32_t x = 120 - HALF_UP(text_w(&AF_M, t));
        GFX_HOOK_ALIGN(0, 0, 0, H_HEAD, AL_V, "header text on its middle");
        cv_text(x < 30 ? 30 : x, HEAD_MY, &AF_M, t, T_TEXT);
    }
    if (ui.menu != 2)                                 /* the tabs: their dots at the right (ABOUT: none; no way-back keycap: SPEC 1.1a) */
        menu_dots();
    cv_blit(0, Y_HEAD);
}

/* The page (1.0.5): the tabs across the top, the rows of the one shown below, the key hints at the bottom.
 *   header   0..24  the cog and MENU, the REC mark, the tabs as dots at the right (the one shown a THEME
 *                   pill)
 *   tabs    29..53  a 24 px cushion per tab: its icon alone (SURF, MID); the one shown wider, with its name (ACCENT,
 *                   INK; S), MT_GAP between any two. With ANIM ON the cushions grow and shrink into place
 *                   (mt_follow), the gaps kept
 *   rows    59..    one SURF panel, a row a label and its value, 1 px rules
 *                   between them; the selected row THEME with INK, as the browser's. A value THEME (M) at the right,
 *                   ON / OFF as a switch, COLOR its palette's five colours, BUTTON MAP, TEST, GUIDE, ABOUT and ERASE DATA a
 *                   chevron (OCT+ opens them); the labels in M too (a list's rows). 0.2.14: rows of one or two units
 *                   (a two-line label), at most MPAGE_UNITS a page; a tab with more has pages ("CONTROL 1/2" in its pill)
 *   keys   224      SELECT ROW and, on a row with a value, KNOB 1 VALUE (SPEC 1.1a: no OCT / HOME keycap; OCT+ still opens
 *                   BUTTON MAP / TEST / ABOUT, OCT- goes back, HOME closes, unadvertised)
 * Drawn in two bands (the canvas holds 124 rows) split in a gap between two rows; while the tabs slide only their
 * strip (24..58) is drawn again */
static const uint16_t MTAB_ICON[MTAB_COUNT] = {ICON_X_EYE, ICON_X_KNOB, ICON_X_SPEAKER, ICON_X_COG};
#define MT_Y 29                                        /* the tab bar */
#define MT_H 24
#define MT_IW 26                                       /* a cushion with its icon alone (the icon centred in it; 1.2, five
                                                        * tabs: 26, up to 1.1.5 32) */
#define MT_NX 24                                       /* the name's pen in the one shown; 10 px after it */
#define MT_X0 6
#define MT_X1 234
#define MT_GAP 5                                       /* between two tabs, always (the bar centred in MT_X0..MT_X1; 1.2, five
                                                        * tabs: 5, up to 1.1.5 8) */
#define MT_ONE 64                                      /* mt.pos per tab */
#define MP_Y 59                                        /* the rows' panel */
#define MP_PAD 3
#define MR_X 8                                         /* a row */
#define MR_W 224
#define MR_LX 16                                       /* its label */
#define MR_VR 224                                      /* its value's right edge */
#define MK_Y 224                                       /* the key hints: the footer's row 2 band (its keycaps centred on its names) */
static uint32_t menu_tab(void) { return MI_TAB[ui.menu_sel % MI_COUNT]; }
/* DUELL 0.2.14: the rows in units (menu_items.c menu_units, menu_place): a unit is 21 px and a 1 px rule (1.2's 7-row look), a two-line
 * row two units (43 px); MPAGE_UNITS (7) units a page, the panel then ends 4 px lower than a 6-row tab did and 6 px above the key
 * hints. The page shown is the selected row's (SELECT walks every row, so it turns the pages too) */
#define MU_H 21                                        /* a unit's height */
#define MU_P 22                                        /* .. and its pitch (the rule between two rows is the gap row) */
static int32_t mu_y(uint32_t u) { return MP_Y + MP_PAD + (int32_t)u * MU_P; }
static uint32_t menu_page(void) { return menu_place(ui.menu_sel % MI_COUNT, 0); }   /* the page shown (0 ..) */
static void mtab_label(uint32_t t, char *b)            /* the tab's pill: its name, "CONTROL 1/2" when it has pages (b: 16) */
{
    uint32_t n = mtab_pages(t), k;
    str_cpy(b, MTAB_NAME[t % MTAB_COUNT], 16);
    if (n > 1u) {                                      /* (a tab shown: its page; another: 1, its pill is an icon then) */
        k = str_len(b);
        b[k] = ' ';
        b[k + 1u] = (char)('0' + (t == menu_tab() ? menu_page() + 1u : 1u));
        b[k + 2u] = '/';
        b[k + 3u] = (char)('0' + n);
        b[k + 4u] = 0;
    }
}

/* the tab bar's place: the tab shown at pos tab * MT_ONE; with ANIM ON it follows in halving steps, a frame each
 * (a frame missed, as when the menu opens: it snaps, as ui_graph.c pr_follow) */
static struct { int16_t pos; uint32_t frame; } mt;
static void mt_follow(void)
{
    int32_t want = (int32_t)menu_tab() * MT_ONE, d = want - mt.pos;
    if (ui.frame != mt.frame + 1u || (ui_prefs & PREF_ANIM_OFF) || (d < 3 && d > -3))
        mt.pos = (int16_t)want;
    else
        mt.pos = (int16_t)(mt.pos + d / 2);
    mt.frame = ui.frame;
}
/* the tabs at the place mt.pos, MT_GAP apart, centred: each one's share of the shown look (0 .. MT_ONE), x, width */
static void mt_layout(int32_t *a, int32_t *x, int32_t *w)
{
    uint32_t k;
    int32_t tot = 0, cum = 0;
    for (k = 0; k < MTAB_COUNT; k++) {
        int32_t d = mt.pos - (int32_t)k * MT_ONE;
        a[k] = MT_ONE - (d < 0 ? -d : d);
        if (a[k] < 0) a[k] = 0;
        char b[16];
        mtab_label(k, b);
        w[k] = MT_IW + (MT_NX - MT_IW + text_w(&AF_S, b) + 10) * a[k] / MT_ONE;
        tot += w[k];
    }
    cum = MT_X0 + HALF_UP(MT_X1 - MT_X0 - tot - (int32_t)(MTAB_COUNT - 1u) * MT_GAP);   /* (the bar centred) */
    for (k = 0; k < MTAB_COUNT; k++) {                 /* the same gap between every two, sliding too */
        x[k] = cum;
        cum += w[k] + MT_GAP;
    }
}
static void menu_tabs(void)                            /* (in screen rows: cv_oy set by the caller) */
{
    int32_t a[MTAB_COUNT], x[MTAB_COUNT], w[MTAB_COUNT];
    uint32_t k;
    mt_layout(a, x, w);
    for (k = 0; k < MTAB_COUNT; k++) {
        int on = k == (uint32_t)((mt.pos + MT_ONE / 2) / MT_ONE);   /* (half way, the shown look moves over) */
        uint16_t bg = on ? T_ACCENT : T_SURF;
        cv_rrect(x[k], MT_Y, w[k], MT_H, -8, bg, T_BG);
        GFX_HOOK_ALIGN(x[k], MT_Y, x[k] + MT_IW, MT_Y + MT_H, AL_HV, "menu tab icon centred in its cushion");
        cv_icon_in(x[k], MT_Y, MT_IW, MT_H, 16, MTAB_ICON[k], on ? T_INK : T_MID, bg);
        if (a[k] == MT_ONE) {                          /* (settled: its name; 0.2.14: and its page, "CONTROL 1/2") */
            char b[16];
            mtab_label(k, b);
            GFX_HOOK_ALIGN(0, MT_Y, 0, MT_Y + MT_H, AL_V, "menu tab name centred up/down");
            cv_text_on(x[k] + MT_NX, MT_Y + CAP_IN(S, MT_H), &AF_S, b,
                       T_INK, T_ACCENT);
        }
    }
}
/* the header's tab dots at the right, the shown tab's a THEME pill */
static void menu_dots(void)
{
    uint32_t k, t = menu_tab();
    int32_t x = 232 - (12 + (int32_t)(MTAB_COUNT - 1u) * 8);
    for (k = 0; k < MTAB_COUNT; k++) {
        cv_rrect(x, H_HEAD / 2 - 2, k == t ? 12 : 4, 4, -2, k == t ? T_THEME : T_DIM, T_BG);
        x += (k == t ? 12 : 4) + 4;
    }
}

/* a two-valued row whose values are ON and OFF: a switch (a 26 x 14 cushion, its knob at the right when ON) */
static void menu_switch(int32_t xr, int32_t y, int32_t h, int on, int sel)
{
    int32_t x = xr - 26, ty = y + HALF_UP(h - 14);
    uint16_t under = sel ? T_THEME : T_SURF;
    uint16_t track = sel ? (on ? T_INK : ux_mix(T_THEME, T_INK, 35)) : on ? T_THEME : T_DIM;
    uint16_t knob = sel ? (on ? T_THEME : T_INK) : on ? T_INK : T_TEXT;
    cv_rrect(x, ty, 26, 14, -7, track, under);
    cv_rrect(on ? x + 14 : x + 2, ty + 2, 10, 10, -5, knob, track);
}
/* a row (menu_items.c row `row`) of the page shown: its units' place, the label at the left (a two-line label: its first line in M,
 * the second small (S, MID), the pair centred in the row), the value at the right centred on the whole row */
static void menu_row(uint32_t row)
{
    uint32_t u;
    int32_t y, h, vy;
    int sel = row == ui.menu_sel;
    uint16_t bg = sel ? T_THEME : T_SURF, fg = sel ? T_INK : T_TEXT, val = sel ? T_INK : T_THEME;
    const char *v = menu_valued(row) ? menu_vname(row, menu_get(row)) : "", *l = menu_label(row);
    char l1[24];
    uint32_t n = 0;
    menu_place(row, &u);
    y = mu_y(u);
    h = (int32_t)menu_units(row) * MU_P - 1;
    vy = y + CAP_IN(M, h);                             /* the label and its value: M (a list's rows) */
    if (sel)
        cv_rrect(MR_X, y, MR_W, h, 6, T_THEME, T_SURF);
    else if (u && row - 1u != ui.menu_sel)             /* a rule from the row above (none beside the selected one, none at a page's top) */
        cv_rect(MR_X + 6, y - 1, MR_W - 12, 1, T_LINE);
    while (l[n] && l[n] != '\n' && n + 1u < sizeof l1) {
        l1[n] = l[n];
        n++;
    }
    l1[n] = 0;
    if (l[n] == '\n') {                                /* two lines: M 11 px caps, 6 px, S 9 px caps, centred in h */
        int32_t top = y + HALF_UP(h - (AF_M_CAP_H + 6 + AF_S_CAP_H));
        GFX_HOOK_ALIGN(0, top, 0, top + AF_M_CAP_H, AL_V, "menu row name's first line");
        cv_text_on(MR_LX, top - AF_M_CAP_Y, &AF_M, l1, fg, bg);
        top += AF_M_CAP_H + 6;
        GFX_HOOK_ALIGN(0, top, 0, top + AF_S_CAP_H, AL_V, "menu row name's second line");
        cv_text_on(MR_LX, top - AF_S_CAP_Y, &AF_S, l + n + 1, sel ? T_INK : T_MID, bg);
    } else {
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "menu row name centred up/down");
        cv_text_on(MR_LX, vy, &AF_M, l1, fg, bg);
    }
    if (!menu_valued(row)) {                           /* BUTTON MAP, TEST, GUIDE, ABOUT, ERASE DATA: OCT+ opens them */
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "menu row chevron centred up/down");
        cv_icon_in(MR_VR - 12, y, 0, h, 12, ICON_X_RIGHT, sel ? T_INK : T_MID, bg);
        return;
    }
    if (row == MI_HOLD || row == MI_TUNE) {            /* "0.4" s, "440.0" Hz: the value and its unit */
        char b[8] = "0.4";
        b[2] = (char)('0' + HOLD_MS[settings_hold % 4u] / 100u);
        GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "menu row value centred up/down");
        GFX_HOOK_ALIGN(0, 0, 0, vy + AF_M.asc, AL_B, "menu row unit on its value's baseline");
        cv_text_r(cv_text_r(MR_VR, vy + AF_M.asc - AF_S.asc, &AF_S, row == MI_TUNE ? "Hz" : "s", sel ? T_INK : T_MID, bg) - 3,
                  vy, &AF_M, row == MI_TUNE ? v : b, val, bg);
        return;
    }
    if (menu_n(row) == 2u && (str_eq(v, "ON") || str_eq(v, "OFF"))) {
        menu_switch(MR_VR, y, h, str_eq(v, "ON"), sel);
        return;
    }
    GFX_HOOK_ALIGN(0, y, 0, y + h, AL_V, "menu row value centred up/down");
    if (row != MI_COLOR) {
        cv_text_r(MR_VR, vy, &AF_M, v, val, bg);
        return;
    }
    {   /* COLOR: the palette's name and its five colours (BG SURF TEXT, part 1's and part 2's colour), live */
        uint32_t c;
        const uint16_t tok[5] = {T_BG, T_SURF, T_TEXT, T_PTHEME(0), T_PTHEME(1)};   /* BG SURF TEXT and each part's colour */
        int32_t x0 = MR_VR - 74, sy = y + HALF_UP(h - 12);
        cv_text_r(x0 - 6, vy, &AF_M, v, val, bg);
        for (c = 0; c < 5u; c++) {
            cv_rrect(x0 + (int32_t)c * 15, sy, 12, 12, -3, T_RAISE, bg);       /* a rim for the dark ones */
            cv_rrect(x0 + 1 + (int32_t)c * 15, sy + 1, 10, 10, -2, tok[c], T_RAISE);
        }
    }
}
static uint32_t about_sig(void)
{
    cpu_meter_poll();
#if DUELL_RAMPLACE
    {   /* the RAM code compared with its flash image once a second while ABOUT is up (ramplace.c; about 1 ms) */
        if (!rp_about_t || fm1_ms - rp_about_t >= 1000u) {
            rp_about_t = fm1_ms | 1u;
            rp_state = (uint8_t)ramplace_check();
        }
    }
    return cpu_sig() + info_usb() * 131u + ovl_lite * 977u + rp_state * 7919u;
#endif
    return cpu_sig() + info_usb() * 131u + ovl_lite * 977u;
}
#if DUELL_TEST_MODE                                    /* the CPU test screen is a developer build (DUELL_TEST_MODE); the release leaves it out down to #endif (TEST screen) */
/* MENU > SYSTEM > TEST (the CPU test screen, beta; ui.menu 3, drawn as the page TEST_PAGE, params.c): what is loaded and how hard the audio works. The multi (slot + name), per part the ENGINE
 * and the PATCH (the stored patch's name or the factory sound's), then the CPU meter: the 1 s AVERAGE with a bar (a mark at 80 %,
 * a tick at the held peak), PEAK HOLD, MISSED blocks, and a word for the state (OK, NEAR THE LIMIT, AT THE LIMIT, BLOCKS MISSED
 * JUST NOW; the same flat colours as before: cpu_col). Drawn on the graph panel (cv_oy 0, panel rows). The meter reads the
 * audio's counters only while this page is up: a gap of more than 0.5 s between two looks (another page, the screen dark) opens a
 * fresh window. K4 RESET: cpu_meter_open and the missed count from zero (test_reset); ABOUT keeps its power-on count */
static uint32_t test_t;                                /* the page's last look (fm1_ms; 0: none yet) */
static void test_sync(void)
{
    if (!test_t || fm1_ms - test_t > 500u)
        cpu_meter_open();
    test_t = fm1_ms ? fm1_ms : 1u;
    cpu_meter_poll();
}
static void test_reset(void)                           /* K4: peak hold, missed blocks and the average window from zero */
{
    cpu_meter_open();
    cpum.base = cpu_miss;
    test_t = fm1_ms ? fm1_ms : 1u;
}
static uint8_t test_pick = 0xFFu;                      /* K3's multi (0..15): the last one stepped to or loaded */
static uint32_t test_slot(void)                        /* the multi K3 shows: the pick, else the one the music came from, else the first used */
{
    uint32_t k;
    if (test_pick < MU_SLOTS && project_used(test_pick))
        return test_pick;
    k = project_cur_slot();
    if (k < MU_SLOTS && project_used(k))
        return k;
    for (k = 0; k < MU_SLOTS; k++)
        if (project_used(k))
            return k;
    return MU_SLOTS;                                   /* no multi at all */
}
static uint32_t test_missed(void) { return cpum.miss - cpum.base; }
static void test_multi_name(char *b)                   /* "M05 NAME", or "NO MULTI" when the music did not come from one */
{
    uint32_t k = project_cur_slot();
    if (k >= MU_SLOTS) {
        str_cpy(b, "NO MULTI", 24);
        return;
    }
    mu_label(b, k);
    str_cpy(b + str_len(b), " ", 2);
    project_cur_name(b + str_len(b));
}
static uint32_t test_sig(void)                         /* what the panel shows */
{
    char b[24];
    uint32_t h = 2166136261u, k;
    test_sync();
    h = (h ^ cpu_sig()) * 16777619u;
    h = (h ^ felucca_dbg.erase_max_us / 1000u) * 16777619u;
    h = (h ^ (vcap_shown(0) * 16u + vcap_shown(1))) * 16777619u;
    test_multi_name(b);
    h = str_hash(h, b);
    for (k = 0; k < 2u; k++) {
        sound_name(&trk[k], b);
        h = str_hash(h + trk[k].eng_req * 31u, b);
    }
    return h;
}
static void test_pct(char *d, uint32_t v) { cpu_pct(d, v); }
static void graph_test(void)
{
    char b[24], v[16];
    uint32_t lvl = cpu_level(), plv = cpum.hold >= 100u ? 2u : cpum.hold >= CPU_WARN ? 1u : 0u, k;
    uint16_t col = cpu_col(lvl, T_SURF);
    int32_t fill = (int32_t)(cpum.avg > 100u ? 100u : cpum.avg) * 224 / 100, tick = (int32_t)(cpum.hold > 100u ? 100u : cpum.hold) * 224 / 100, x;
    cv_text_on(10, 3, &AF_S, "MULTI", T_MID, T_SURF);
    test_multi_name(b);
    cv_text_r(230, 3, &AF_S, b, project_cur_slot() < MU_SLOTS ? T_TEXT : T_DIM, T_SURF);
    for (k = 0; k < 2u; k++) {                         /* the parts: engine at the left, the patch at the right */
        int32_t y = 17 + 13 * (int32_t)k;
        v[0] = (char)('1' + k);
        v[1] = 0;
        cv_text_on(10, y, &AF_S, v, song.sel == k ? T_ACCENT : T_DIM, T_SURF);
        cv_text_on(22, y, &AF_S, ENGINES[trk[k].eng_req % NENGINES]->name, T_MID, T_SURF);
        sound_name(&trk[k], b);
        cv_text_r(230, y, &AF_S, b, T_TEXT, T_SURF);
    }
    cv_rect(8, 43, 224, 1, T_RAISE);                   /* (0.2.11: 4 rows up a few px for the ERASE row) */
    cv_text_on(10, 47, &AF_S, "AVERAGE 1 S", T_MID, T_SURF);
    test_pct(v, cpum.avg);
    cv_text_r(230, 44, &AF_M, v, col, T_SURF);
    cv_rect(8, 64, 224, 8, T_RAISE);                   /* the track, the fill, the 80 % mark, the peak tick */
    cv_rect(8, 64, fill, 8, col);
    cv_rect(8 + 224 * (int32_t)CPU_WARN / 100, 72, 1, 3, T_MID);
    cv_rect(8 + (tick > 222 ? 222 : tick), 61, 2, 14, T_TEXT);
    x = cv_text_on(10, 79, &AF_S, "PEAK HOLD", T_MID, T_SURF);
    test_pct(v, cpum.hold);
    cv_text_on(x + 6, 79, &AF_S, v, cpu_col(plv, T_SURF), T_SURF);
    fmt_int(v, (int32_t)(test_missed() > 9999u ? 9999u : test_missed()));
    x = cv_text_r(230, 79, &AF_S, v, cpu_recent() ? cpu_col(2u, T_SURF) : T_TEXT, T_SURF);
    cv_text_r(x - 6, 79, &AF_S, "MISSED", T_MID, T_SURF);
    cv_text_on(10, 93, &AF_S, cpu_word(), lvl ? cpu_col(lvl, T_SURF) : T_MID, T_SURF);
#ifdef FELUCCA_METER_EST
    cv_text_r(230, 93, &AF_S, "EMULATOR ESTIMATE", T_DIM, T_SURF);
#endif
    x = cv_text_on(10, 107, &AF_S, "ERASE", T_MID, T_SURF);   /* DUELL 0.2.11: the longest flash erase since boot (storage_hw.c: the driver's own time) */
    if (felucca_dbg.erase_max_us) {
        fmt_int(v, (int32_t)((felucca_dbg.erase_max_us + 500u) / 1000u));
        str_cpy(v + str_len(v), " MS", 4);
    } else
        str_cpy(v, "--", 3);
    cv_text_on(x + 6, 107, &AF_S, v, T_TEXT, T_SURF);
    v[0] = (char)('0' + vcap_shown(0));          /* DUELL 0.2.11: the VOICE CAP of part 1 / part 2 (voice.c vcap; the engine's own cap included) */
    str_cpy(v + 1, " / ", 4);
    v[4] = (char)('0' + vcap_shown(1));
    v[5] = 0;
    x = cv_text_r(230, 107, &AF_S, v, T_TEXT, T_SURF);
    cv_text_r(x - 6, 107, &AF_S, "VOICES", T_MID, T_SURF);
}
/* The TEST screen (ui.menu 3): the page TEST_PAGE drawn by the page code (header with its name TEST in the top middle, the K1..K4 cards, the
 * panel graph_test, the footer), page_over making it the current page; the LAYER state is put aside while it draws */
static void draw_test_screen(void)
{
    uint8_t layer = ui.layer;
    ui.layer = 0;
    page_over = &TEST_PAGE;
    if (ui.force)
        draw_frame();
    draw_head();
    draw_columns();
    draw_graph();
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.hot_t)
        ui.hot_t--;
    draw_foot();
    page_over = 0;
    ui.layer = layer;
}
static void edit_param(uint32_t slot, int32_t steps);   /* ui_input.c: the TEST screen's knobs */
#endif                                                 /* (TEST screen) */

/* the rows' two bands meet at `split`: in the gap above a row of the page (or below its last, at unit `end`), each band at most 124 rows */
static uint32_t menu_split(uint32_t first, uint32_t n, uint32_t pg, uint32_t end)
{
    int32_t s = mu_y(end) - 1, y;
    uint32_t k, u;
    for (k = 0; k < n; k++)                            /* (the lowest row top that keeps the upper band within 124) */
        if (menu_place(first + k, &u) == pg && (y = mu_y(u) - 1) <= H_HEAD + 124 && (s > H_HEAD + 124 || y > s))
            s = y;
    return (uint32_t)(s < 240 - 124 ? 240 - 124 : s);
}
static void draw_menu(void)
{
    /* SPEC 1.1a: SELECT picks the row (across the tabs), KNOB 1 the value (an action row has none: SELECT alone;
     * no OCT+ OPEN / CLEAR hint) */
    static const khint_t KEYS[3] = {{KC_SELECT, "ROW"}, {KC_K1, "VALUE"}, {KC_K1, "OPEN"}};
    uint32_t pass, k, first, n, split, sig, pg, end, u;
    int32_t pos0 = mt.pos;
#if DUELL_TEST_MODE
    if (ui.menu == 3) {                                /* TEST: the page code draws it, whole each frame (it checks its own signatures) */
        draw_test_screen();
        return;
    }
#endif
#if DUELL_BENCH
    if (ui.menu == 5) {                                /* BENCH (bench builds): src/bench.c draws it */
        bench_draw();
        return;
    }
#endif
    if (ui.menu == 1)
        mt_follow();
    sig = ui.menu * 7u + ui.menu_sel * 131u + settings.palette * 1009u + settings_hold * 3511u +
          settings_leds * 6151u + ui_prefs * 4099u + song.rec * 65537u + song.sel * 13u +
          ui_rec_prefs * 92821u + ui_scr * 131071u +          /* (#157: SCREEN OFF's value redraws its row) */
          ui_prefs2 * 524287u + ui_idle * 1048573u +      /* (1.2: ANIM IDLE's value redraws its row) */
          (uint32_t)(song.g[G_TUNE] + 64) * 8191u + ui_home_pg * 27449u +   /* (1.2: the project's row; DUELL: HOME OPENS) */
          (ui.menu == 2 ? ui.menu_scroll * 48611u + about_sig() * 40507u + about_step() * 6007u : 0u);
    if (!ui.force && sig == ui.menu_sig) {
        if (ui.menu == 1 && mt.pos != pos0) {          /* only the tabs slid: their strip */
            cv_begin(240, MP_Y - H_HEAD, T_BG);
            cv_oy = -H_HEAD;
            menu_tabs();
            cv_oy = 0;
            cv_blit(0, H_HEAD);
        }
        return;
    }
    ui.menu_sig = sig;
    if (ui.menu == 2 && !ui.menu_scroll) {             /* ABOUT's first view: the splash drawing, endless (the footer hint on it) */
        splash_frame(about_step(), MK_Y);
        return;
    }
    menu_head();
    if (ui.menu == 2) {
        int32_t scroll = ui.menu_scroll, max = menu_scroll_max();
        int32_t thumb = MENU_DOC_H * MENU_DOC_H / (max + MENU_DOC_H);
        if (thumb < 12) thumb = 12;
        for (pass = 0; pass < 2u; pass++) {
            cv_begin(240, pass ? MENU_DOC_H - 124u : 124u, T_BG);
            cv_oy = pass ? -124 : 0;
            cv_scroll = 1;
            menu_document(-scroll, 1);
            cv_scroll = 0;
            cv_rrect(235, 2, 3, MENU_DOC_H - 4, 1, T_LINE, T_BG);           /* the scroll bar */
            cv_rrect(235, 2 + (max ? scroll * (MENU_DOC_H - 4 - thumb) / max : 0), 3, thumb, 1, T_MID, T_LINE);
            cv_oy = 0;
            cv_blit(0, H_HEAD + pass * 124u);
        }
        cv_begin(240, 240 - (H_HEAD + MENU_DOC_H), T_BG);
        cv_key_row(8, 232, MK_Y - (H_HEAD + MENU_DOC_H), ABOUT_KEYS, 3u, 7u, T_BG);
        cv_blit(0, H_HEAD + MENU_DOC_H);
        return;
    }
    first = mtab_first(menu_tab());
    n = mtab_rows(menu_tab());
    pg = menu_page();
    for (k = 0, end = 0; k < n; k++)                   /* (the page's last unit: the panel's end) */
        if (menu_place(first + k, &u) == pg && u + menu_units(first + k) > end)
            end = u + menu_units(first + k);
    split = menu_split(first, n, pg, end);
    for (pass = 0; pass < 2u; pass++) {
        int32_t top_y = pass ? (int32_t)split : H_HEAD;
        cv_begin(240, pass ? 240u - split : split - H_HEAD, T_BG);
        cv_oy = -top_y;                               /* drawn in screen rows */
        if (!pass)
            menu_tabs();
        cv_rrect(4, MP_Y, 232, mu_y(end) - 1 + MP_PAD - MP_Y, 8, T_SURF, T_BG);
        for (k = 0; k < n; k++)
            if (menu_place(first + k, &u) == pg && mu_y(u) + (int32_t)menu_units(first + k) * MU_P - 1 > top_y && mu_y(u) - 1 < top_y + (int32_t)cv_h)
                menu_row(first + k);
        if (pass) {
            if (MI_READONLY(ui.menu_sel))                 /* TEST, ABOUT: SELECT picks the row, KNOB 1 right opens it */
                cv_key_row(8, 232, MK_Y, (const khint_t[2]){KEYS[0], KEYS[2]}, 2u, 3u, T_BG);
            else
                cv_key_row(8, 232, MK_Y, KEYS, menu_valued(ui.menu_sel) ? 2u : 1u, 3u, T_BG);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)top_y);
    }
}
static void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}

static void menu_close(void)
{
    if (ui.confirm == CF_ERASE_ALL)                    /* (HOME in the ERASE DATA dialog: cancelled, nothing erased) */
        ui.confirm = 0;
    settings_save();                                   /* palette / panel table, if changed */
    ui.menu = 0;
    ui.force = 1;
    go_main();
}

/* menu (SPEC 1.1a): SELECT is the one way round it: it walks the rows continuously across the tabs (a tab's last row,
 * then the next tab's first; stopping at the first and the last row of the whole menu, no wrap), in ABOUT it jumps between sections
 * (KNOB 1 scrolls there);
 * KNOB 1..4 (any of them) change the selected row's value (menu_items.c menu_step), OCT+ / OCT- step it too (and OCT+
 * opens CALIBRATION / TEST / ABOUT, OCT- nothing there; OCT- in TEST and ABOUT goes back to the tabs) but no hint
 * names them. ALGORITHM and PRESETS do nothing here. HOME closes it, from them too (ui_input.c) */
static void menu_move(int32_t s)                       /* SELECT: the next / previous row, across the tabs, clamped */
{
    int32_t r = (int32_t)ui.menu_sel + clamp(s, -(int32_t)MI_COUNT, (int32_t)MI_COUNT);
    uint32_t t;
    ui.menu_sel = (uint8_t)clamp(r, 0, (int32_t)MI_COUNT - 1);
    t = menu_tab();
    ui.menu_row[t] = (uint8_t)(ui.menu_sel - mtab_first(t));
}
static int32_t accel(uint32_t role, int32_t s, int32_t range);   /* (ui_input.c, KNOB ACCEL) */
static void menu_input(uint32_t oct)                  /* oct: ui_input.c oct_taps */
{
    int32_t s;
    uint32_t k, up = (oct >> 1) & 1u, dn = oct & 1u;
    panel_enc(EN_PRESET);                              /* PRESETS, ALGORITHM: nothing in the menu (SPEC 1.1a) */
    panel_enc(EN_ALGO);
#if DUELL_TEST_MODE
    if (ui.menu == 3) {                                /* TEST: KNOB 1..4 are its own (K1 MORPH both ways: not a way back); OCT- goes back to the
                                                        * tabs, HOME and a page button close the menu; SELECT and OCT+ do nothing here */
        uint8_t layer = ui.layer;
        if (dn) {
            ui.menu = 1, ui.force = 1;
            enc_drop();
            return;
        }
        ui.layer = 0;
        page_over = &TEST_PAGE;
        for (k = 0; k < 4u; k++)
            if ((s = panel_enc(EN_K1 + k)) != 0) {
                ui.hot_col = (uint8_t)k;
                ui.hot_t = 40;
                edit_param(k, s);
            }
        page_over = 0;
        ui.layer = layer;
        enc_drop();
        return;
    }
#endif
    if (ui.menu == 2) {                                /* ABOUT (0.2.9, the owner): KNOB 1 scrolls (right down, left up; bounded), SELECT
                                                        * jumps to the next / previous section (about_section); OCT- back to the tabs,
                                                        * HOME closes; KNOB 2..4, PRESETS, ALGORITHM nothing */
        if (dn) {
            ui.menu = 1, ui.force = 1;
            enc_drop();
            return;
        }
        if ((s = panel_enc(EN_K1)) != 0)
            ui.menu_scroll = (uint16_t)clamp((int32_t)ui.menu_scroll + clamp(s, -128, 128) * 18, 0, menu_scroll_max());
        for (s = clamp(panel_enc(EN_SELECT), -4, 4); s; s -= s > 0 ? 1 : -1)   /* (a section a detent) */
            ui.menu_scroll = (uint16_t)about_section(ui.menu_scroll, s);
        enc_drop();
        return;
    }
    if ((s = panel_enc(EN_SELECT)) != 0)
        menu_move(s);
    for (s = 0, k = 0; k < 4u; k++) {                  /* KNOB 1..4, any of them */
        int32_t e = panel_enc(EN_K1 + k);
        s += ui.menu_sel == MI_TUNE ? accel(EN_K1 + k, e, (int32_t)TUNE_N) : e;   /* (0.2.22: TUNE has 510 steps: with KNOB ACCEL ON a fast turn goes x2 .. x8, ui_input.c accel; the other rows as ever) */
    }
    if (menu_valued(ui.menu_sel)) {
        /* a step (OCT+ the next value, OCT- the previous); COLOR previews the palette */
        s += (int32_t)up - (int32_t)dn;
        if (s) {
            uint32_t was = menu_get(ui.menu_sel);
            menu_put(ui.menu_sel, menu_step(ui.menu_sel, s));
            if (menu_get(ui.menu_sel) != was && !MI_PROJECT(ui.menu_sel)) {   /* DUELL 0.2.2: a changed setting is saved now, not when the */
                if (menu_n(ui.menu_sel) == 2u)                                /* menu is left: a two-value switch at once, any other once the */
                    settings_save();                                          /* knob has rested ~2 s (the AMOUNT knob's debounce, ui_input.c */
                else {                                                        /* morph_settings); while playing the write waits (settings_poll) */
                    ui.mset_dirty = 1;
                    ui.mset_t = fm1_ms;
                }
            }
        }
    } else if (up || (s > 0 && MI_READONLY(ui.menu_sel))) {   /* OCT+ opens BUTTON MAP, ERASE DATA and the three; a knob right too TEST, GUIDE and ABOUT */
        if (ui.menu_sel == MI_PANEL) {
            panel_setup();
            scrn.idle = fm1_ms;                         /* (SCREEN OFF: its own loop had the input) */
            ui.force = 1;
        } else if (ui.menu_sel == MI_ERASE) {           /* DUELL 0.2.2: the dialog over the menu (ui_input.c: OCT+ YES, OCT- NO) */
            ui.confirm = CF_ERASE_ALL;
            ui.confirm_trk = 0;
            ui.force = 1;
#if DUELL_TEST_MODE
        } else if (ui.menu_sel == MI_TEST) {
            ui.menu = 3;
            cpu_meter_open();                           /* (the CPU meter's window begins) */
            ui.force = 1;
#endif
        } else if (ui.menu_sel == MI_GUIDE) {           /* DUELL 0.2.11: the first-start GUIDE's pages, over the menu (ui_guide.c); the bit untouched */
            guide_open(1);
        } else {                                       /* MI_ABOUT */
            ui.menu = 2;
            about_t0 = fm1_ms;
            cpu_meter_open();
            ui.menu_scroll = 0;
            ui.force = 1;
        }
    }
    enc_drop();                                        /* swallow the rest while the menu is up */
}
