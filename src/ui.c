#include "ui.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct Rect { int x, y, w, h; } Rect;

enum {
    DRAG_NONE,
    DRAG_PITCH,
    DRAG_VOLUME,
    DRAG_EQ,
    DRAG_CROSSFADER,
    DRAG_GAIN,
    DRAG_NUDGE
};

#define BROWSER_MAX 256
#define BROWSER_NAME_MAX 256

typedef struct BrowserEntry {
    char name[BROWSER_NAME_MAX];
    bool dir;
} BrowserEntry;

static BrowserEntry browser_entries[BROWSER_MAX];
static int browser_count;
static char browser_cwd[1024] = ".";

static void color(SDL_Renderer *r, Uint8 v) { SDL_SetRenderDrawColor(r, v, v, v, 255); }
static void color_rgb(SDL_Renderer *r, Uint8 red, Uint8 green, Uint8 blue) { SDL_SetRenderDrawColor(r, red, green, blue, 255); }
static void fill(SDL_Renderer *r, Rect q, Uint8 v) { SDL_Rect sr = { q.x, q.y, q.w, q.h }; color(r, v); SDL_RenderFillRect(r, &sr); }
static void fill_rgb(SDL_Renderer *r, Rect q, Uint8 red, Uint8 green, Uint8 blue) { SDL_Rect sr = { q.x, q.y, q.w, q.h }; color_rgb(r, red, green, blue); SDL_RenderFillRect(r, &sr); }
static void stroke(SDL_Renderer *r, Rect q, Uint8 v) { SDL_Rect sr = { q.x, q.y, q.w, q.h }; color(r, v); SDL_RenderDrawRect(r, &sr); }
static void stroke_rgb(SDL_Renderer *r, Rect q, Uint8 red, Uint8 green, Uint8 blue) { SDL_Rect sr = { q.x, q.y, q.w, q.h }; color_rgb(r, red, green, blue); SDL_RenderDrawRect(r, &sr); }
static void line(SDL_Renderer *r, int x1, int y1, int x2, int y2, Uint8 v) { color(r, v); SDL_RenderDrawLine(r, x1, y1, x2, y2); }

static bool contains(Rect q, int x, int y) { return x >= q.x && y >= q.y && x < q.x + q.w && y < q.y + q.h; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static bool module_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    char ext[16];
    snprintf(ext, sizeof(ext), "%s", dot + 1);
    for (char *p = ext; *p; p++) *p = (char)tolower((unsigned char)*p);
    return strcmp(ext, "mod") == 0 || strcmp(ext, "xm") == 0 || strcmp(ext, "med") == 0 || strcmp(ext, "it") == 0 || strcmp(ext, "s3m") == 0 || strcmp(ext, "mptm") == 0 || strcmp(ext, "okt") == 0;
}

static int entry_cmp(const void *a, const void *b) {
    const BrowserEntry *ea = a;
    const BrowserEntry *eb = b;
    if (ea->dir != eb->dir) return ea->dir ? -1 : 1;
    return strcasecmp(ea->name, eb->name);
}

static void browser_refresh(void) {
    DIR *dir = opendir(browser_cwd);
    browser_count = 0;
    if (!dir) return;
    if (strcmp(browser_cwd, "/") != 0) {
        snprintf(browser_entries[browser_count].name, BROWSER_NAME_MAX, "..");
        browser_entries[browser_count].dir = true;
        browser_count++;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) && browser_count < BROWSER_MAX) {
        if (ent->d_name[0] == '.') continue;
        char path[1400];
        snprintf(path, sizeof(path), "%s/%s", browser_cwd, ent->d_name);
        bool is_dir = ent->d_type == DT_DIR;
        if (ent->d_type == DT_UNKNOWN) {
            DIR *probe = opendir(path);
            if (probe) { is_dir = true; closedir(probe); }
        }
        if (!is_dir && !module_ext(ent->d_name)) continue;
        snprintf(browser_entries[browser_count].name, BROWSER_NAME_MAX, "%s", ent->d_name);
        browser_entries[browser_count].dir = is_dir;
        browser_count++;
    }
    closedir(dir);
    qsort(browser_entries, (size_t)browser_count, sizeof(browser_entries[0]), entry_cmp);
}

static void browser_enter(const char *name) {
    if (strcmp(name, "..") == 0) {
        char *slash = strrchr(browser_cwd, '/');
        if (slash && slash != browser_cwd) *slash = '\0';
        else snprintf(browser_cwd, sizeof(browser_cwd), "/");
    } else {
        char next[1024];
        if (snprintf(next, sizeof(next), "%s/%s", browser_cwd, name) >= (int)sizeof(next)) return;
        snprintf(browser_cwd, sizeof(browser_cwd), "%s", next);
    }
    browser_refresh();
}

static void load_path(FtAudio *audio, FtDeck *deck, const char *path) {
    char err[256];
    audio_lock(audio);
    bool ok = deck_load(deck, path, audio->spec.freq ? audio->spec.freq : 48000, err, sizeof(err));
    audio_unlock(audio);
    if (!ok) fprintf(stderr, "load failed: %s: %s\n", path, err);
}

static const char **glyph(char c) {
    static const char *blank[7] = { "00000", "00000", "00000", "00000", "00000", "00000", "00000" };
    static const char *qmark[7] = { "11110", "00001", "00001", "00110", "00100", "00000", "00100" };
    static const char *g0[7] = { "01110", "10001", "10011", "10101", "11001", "10001", "01110" };
    static const char *g1[7] = { "00100", "01100", "00100", "00100", "00100", "00100", "01110" };
    static const char *g2[7] = { "01110", "10001", "00001", "00010", "00100", "01000", "11111" };
    static const char *g3[7] = { "11110", "00001", "00001", "01110", "00001", "00001", "11110" };
    static const char *g4[7] = { "00010", "00110", "01010", "10010", "11111", "00010", "00010" };
    static const char *g5[7] = { "11111", "10000", "10000", "11110", "00001", "00001", "11110" };
    static const char *g6[7] = { "01110", "10000", "10000", "11110", "10001", "10001", "01110" };
    static const char *g7[7] = { "11111", "00001", "00010", "00100", "01000", "01000", "01000" };
    static const char *g8[7] = { "01110", "10001", "10001", "01110", "10001", "10001", "01110" };
    static const char *g9[7] = { "01110", "10001", "10001", "01111", "00001", "00001", "01110" };
    static const char *ga[7] = { "01110", "10001", "10001", "11111", "10001", "10001", "10001" };
    static const char *gb[7] = { "11110", "10001", "10001", "11110", "10001", "10001", "11110" };
    static const char *gc[7] = { "01111", "10000", "10000", "10000", "10000", "10000", "01111" };
    static const char *gd[7] = { "11110", "10001", "10001", "10001", "10001", "10001", "11110" };
    static const char *ge[7] = { "11111", "10000", "10000", "11110", "10000", "10000", "11111" };
    static const char *gf[7] = { "11111", "10000", "10000", "11110", "10000", "10000", "10000" };
    static const char *gg[7] = { "01111", "10000", "10000", "10011", "10001", "10001", "01111" };
    static const char *gh[7] = { "10001", "10001", "10001", "11111", "10001", "10001", "10001" };
    static const char *gi[7] = { "11111", "00100", "00100", "00100", "00100", "00100", "11111" };
    static const char *gj[7] = { "00001", "00001", "00001", "00001", "10001", "10001", "01110" };
    static const char *gk[7] = { "10001", "10010", "10100", "11000", "10100", "10010", "10001" };
    static const char *gl[7] = { "10000", "10000", "10000", "10000", "10000", "10000", "11111" };
    static const char *gm[7] = { "10001", "11011", "10101", "10101", "10001", "10001", "10001" };
    static const char *gn[7] = { "10001", "11001", "10101", "10011", "10001", "10001", "10001" };
    static const char *go[7] = { "01110", "10001", "10001", "10001", "10001", "10001", "01110" };
    static const char *gp[7] = { "11110", "10001", "10001", "11110", "10000", "10000", "10000" };
    static const char *gr[7] = { "11110", "10001", "10001", "11110", "10100", "10010", "10001" };
    static const char *gs[7] = { "01111", "10000", "10000", "01110", "00001", "00001", "11110" };
    static const char *gt[7] = { "11111", "00100", "00100", "00100", "00100", "00100", "00100" };
    static const char *gu[7] = { "10001", "10001", "10001", "10001", "10001", "10001", "01110" };
    static const char *gv[7] = { "10001", "10001", "10001", "10001", "10001", "01010", "00100" };
    static const char *gw[7] = { "10001", "10001", "10001", "10101", "10101", "10101", "01010" };
    static const char *gx[7] = { "10001", "10001", "01010", "00100", "01010", "10001", "10001" };
    static const char *gy[7] = { "10001", "10001", "01010", "00100", "00100", "00100", "00100" };
    static const char *dash[7] = { "00000", "00000", "00000", "11111", "00000", "00000", "00000" };
    static const char *dot[7] = { "00000", "00000", "00000", "00000", "00000", "01100", "01100" };
    static const char *colon[7] = { "00000", "01100", "01100", "00000", "01100", "01100", "00000" };
    static const char *plus[7] = { "00000", "00100", "00100", "11111", "00100", "00100", "00000" };
    static const char *slash[7] = { "00001", "00010", "00010", "00100", "01000", "01000", "10000" };
    static const char *pct[7] = { "11001", "11010", "00010", "00100", "01000", "01011", "10011" };
    static const char *under[7] = { "00000", "00000", "00000", "00000", "00000", "00000", "11111" };
    static const char *apos[7] = { "01100", "01100", "00100", "01000", "00000", "00000", "00000" };
    static const char *paren_l[7] = { "00010", "00100", "01000", "01000", "01000", "00100", "00010" };
    static const char *paren_r[7] = { "01000", "00100", "00010", "00010", "00010", "00100", "01000" };
    static const char *bang[7] = { "00100", "00100", "00100", "00100", "00100", "00000", "00100" };
    static const char *comma[7] = { "00000", "00000", "00000", "00000", "01100", "01100", "01000" };
    c = (char)toupper((unsigned char)c);
    switch (c) {
    case ' ': return blank; case '?': return qmark; case '0': return g0; case '1': return g1; case '2': return g2; case '3': return g3; case '4': return g4; case '5': return g5; case '6': return g6; case '7': return g7; case '8': return g8; case '9': return g9;
    case 'A': return ga; case 'B': return gb; case 'C': return gc; case 'D': return gd; case 'E': return ge; case 'F': return gf; case 'G': return gg; case 'H': return gh; case 'I': return gi; case 'J': return gj; case 'K': return gk; case 'L': return gl; case 'M': return gm; case 'N': return gn; case 'O': return go; case 'P': return gp; case 'Q': return go; case 'R': return gr; case 'S': return gs; case 'T': return gt; case 'U': return gu; case 'V': return gv; case 'W': return gw; case 'X': return gx; case 'Y': return gy; case 'Z': return g2;
    case '-': return dash; case '.': return dot; case ':': return colon; case '+': return plus; case '/': return slash; case '%': return pct; case '_': return under; case '\'': return apos; case '(': return paren_l; case ')': return paren_r; case '!': return bang; case ',': return comma;
    default: return blank;
    }
}

static void text(SDL_Renderer *r, int x, int y, const char *s, int scale, Uint8 v) {
    color(r, v);
    for (; *s; s++) {
        const char **g = glyph(*s);
        for (int yy = 0; yy < 7; yy++) {
            for (int xx = 0; xx < 5; xx++) {
                if (g[yy][xx] == '1') {
                    SDL_Rect p = { x + xx * scale, y + yy * scale, scale, scale };
                    SDL_RenderFillRect(r, &p);
                }
            }
        }
        x += 6 * scale;
    }
}

static void textf(SDL_Renderer *r, int x, int y, int scale, Uint8 v, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    text(r, x, y, buf, scale, v);
}

static void button(SDL_Renderer *r, Rect q, const char *label, bool active) {
    fill(r, q, active ? 34 : 10);
    stroke(r, q, active ? 235 : 100);
    text(r, q.x + 8, q.y + (q.h - 14) / 2, label, 2, active ? 240 : 155);
}

static void slider(SDL_Renderer *r, Rect q, float value, bool bipolar, const char *label, const char *value_label) {
    text(r, q.x - 4, q.y - 20, label, 2, 120);
    stroke(r, q, 75);
    int cx = q.x + q.w / 2;
    line(r, cx, q.y + 6, cx, q.y + q.h - 6, 80);
    int handle_y;
    if (bipolar) {
        int mid = q.y + q.h / 2;
        line(r, q.x + 4, mid, q.x + q.w - 4, mid, 120);
        value = clampf(value, -1.0f, 1.0f);
        handle_y = mid - (int)(value * (float)(q.h / 2 - 9));
    } else {
        value = clampf(value, 0.0f, 1.0f);
        handle_y = q.y + q.h - 10 - (int)(value * (float)(q.h - 20));
    }
    Rect cap = { q.x + 4, handle_y - 8, q.w - 8, 16 };
    fill_rgb(r, cap, 62, 150, 152);
    stroke_rgb(r, cap, 150, 232, 226);
    if (value_label) text(r, q.x - 14, q.y + q.h + 8, value_label, 1, 150);
}

static void meter(SDL_Renderer *r, Rect q, float l, float rr) {
    stroke(r, q, 70);
    for (int i = 0; i < 14; i++) {
        float threshold = (float)(i + 1) / 14.0f;
        float boosted = clampf((l > rr ? l : rr) * 2.6f, 0.0f, 1.0f);
        Uint8 v = (boosted > threshold) ? (i > 11 ? 245 : 185) : 35;
        fill(r, (Rect){ q.x + 4, q.y + q.h - 7 - i * 9, q.w - 8, 5 }, v);
    }
}

static void knob(SDL_Renderer *r, Rect q, float value, const char *label, const char *value_label) {
    value = clampf(value, -1.0f, 1.0f);
    int cx = q.x + q.w / 2;
    int cy = q.y + q.h / 2;
    int radius = q.w < q.h ? q.w / 2 - 4 : q.h / 2 - 4;
    text(r, q.x, q.y - 16, label, 1, 125);
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            int d = x * x + y * y;
            if (d <= radius * radius && d >= (radius - 2) * (radius - 2)) {
                color(r, 95);
                SDL_RenderDrawPoint(r, cx + x, cy + y);
            }
        }
    }
    float angle = (-90.0f + value * 135.0f) * 3.14159265f / 180.0f;
    int tx = cx + (int)(cosf(angle) * (float)(radius - 4));
    int ty = cy + (int)(sinf(angle) * (float)(radius - 4));
    line(r, cx, cy, tx, ty, 230);
    fill(r, (Rect){ cx - 2, cy - 2, 4, 4 }, 180);
    if (value_label) text(r, q.x - 4, q.y + q.h + 4, value_label, 1, 130);
}

static float eq_knob_value(float db) {
    return db < 0.0f ? db / 26.0f : db / 6.0f;
}

static void app_layout(const FtUi *ui, Rect decks[2], Rect *mix) {
    int margin = ui->width < 1000 ? 10 : 22;
    int top = ui->height < 560 ? 58 : 76;
    int bottom = 24;
    if (ui->width < 840) {
        int usable_h = ui->height - top - bottom - margin * 2;
        if (usable_h < 360) usable_h = 360;
        int deck_h = usable_h * 2 / 5;
        int mix_h = usable_h - deck_h * 2;
        if (mix_h < 210) mix_h = 210;
        decks[0] = (Rect){ margin, top, ui->width - margin * 2, deck_h };
        *mix = (Rect){ margin, decks[0].y + decks[0].h + margin, ui->width - margin * 2, mix_h };
        decks[1] = (Rect){ margin, mix->y + mix->h + margin, ui->width - margin * 2, deck_h };
        return;
    }
    int mix_w = ui->width < 1100 ? 260 : 330;
    int deck_w = (ui->width - margin * 4 - mix_w) / 2;
    decks[0] = (Rect){ margin, top, deck_w, ui->height - top - bottom };
    *mix = (Rect){ margin * 2 + deck_w, top, mix_w, ui->height - top - bottom };
    decks[1] = (Rect){ margin * 3 + deck_w + mix_w, top, deck_w, ui->height - top - bottom };
}

static void deck_layout(Rect q, Rect *pitch, Rect *browse, Rect *play, Rect *cue, Rect *setcue, Rect *sync, Rect *loop, Rect *loopdec, Rect *loopinc, Rect *nudge_back, Rect *nudge_fwd, Rect *nudge_clear, Rect *progress) {
    int pad = q.w < 420 ? 12 : 24;
    int pitch_h = q.h < 500 ? 150 : 220;
    *pitch = (Rect){ q.x + q.w - pad - 42, q.y + 128, 42, pitch_h };
    if (q.w < 520) {
        int gap = 6;
        int bw = (q.w - pad * 2 - gap * 2) / 3;
        int y2 = q.y + q.h - 58;
        int y1 = y2 - 40;
        *browse = (Rect){ q.x + pad, y1, bw, 32 };
        *play = (Rect){ browse->x + bw + gap, y1, bw, 32 };
        *cue = (Rect){ play->x + bw + gap, y1, bw, 32 };
        *setcue = (Rect){ q.x + pad, y2, bw, 32 };
        *sync = (Rect){ setcue->x + bw + gap, y2, bw, 32 };
        *loop = (Rect){ sync->x + bw + gap, y2, bw, 32 };
    } else {
        *browse = (Rect){ q.x + pad, q.y + q.h - 70, 86, 38 };
        *play = (Rect){ q.x + pad + 94, q.y + q.h - 70, 74, 38 };
        *cue = (Rect){ q.x + pad + 176, q.y + q.h - 70, 62, 38 };
        *setcue = (Rect){ q.x + pad + 246, q.y + q.h - 70, 62, 38 };
        *sync = (Rect){ q.x + pad + 316, q.y + q.h - 70, 62, 38 };
        *loop = (Rect){ q.x + pad + 386, q.y + q.h - 70, 62, 38 };
    }
    if (q.w < 420) {
        *loopdec = (Rect){ q.x + pad, q.y + q.h - 156, 42, 30 };
        *loopinc = (Rect){ q.x + pad + 50, q.y + q.h - 156, 42, 30 };
        *nudge_back = (Rect){ q.x + pad + 108, q.y + q.h - 156, 48, 30 };
        *nudge_fwd = (Rect){ q.x + pad + 162, q.y + q.h - 156, 48, 30 };
        *nudge_clear = (Rect){ q.x + pad + 216, q.y + q.h - 156, 54, 30 };
    } else {
        *loopdec = (Rect){ q.x + pad, q.y + q.h - 156, 44, 30 };
        *loopinc = (Rect){ q.x + pad + 104, q.y + q.h - 156, 44, 30 };
        *nudge_back = (Rect){ q.x + pad + 170, q.y + q.h - 156, 54, 30 };
        *nudge_fwd = (Rect){ q.x + pad + 230, q.y + q.h - 156, 54, 30 };
        *nudge_clear = (Rect){ q.x + pad + 290, q.y + q.h - 156, 62, 30 };
    }
    *progress = (Rect){ q.x + pad, q.y + q.h - 118, q.w - pad * 2, 20 };
}

static void mixer_layout(Rect q, Rect eq[2][3], Rect vol[2], Rect gain[2], Rect master[3], Rect *cf) {
    int strip_w = 86;
    int x0 = q.x + 24;
    int x1 = q.x + q.w - 24 - strip_w;
    int cf_y = q.y + q.h - 64;
    int vol_y = q.y + 390;
    int vol_h = cf_y - 24 - vol_y;
    if (vol_h < 90) vol_h = 90;
    if (vol_h > 160) vol_h = 160;
    for (int d = 0; d < 2; d++) {
        int x = d == 0 ? x0 : x1;
        gain[d] = (Rect){ x + 21, q.y + 88, 44, 44 };
        eq[d][0] = (Rect){ x + 21, q.y + 170, 44, 44 };
        eq[d][1] = (Rect){ x + 21, q.y + 235, 44, 44 };
        eq[d][2] = (Rect){ x + 21, q.y + 300, 44, 44 };
        vol[d] = (Rect){ x + 23, vol_y, 40, vol_h };
    }
    master[0] = (Rect){ q.x + q.w / 2 - 34, q.y + 104, 24, 128 };
    master[1] = (Rect){ q.x + q.w / 2 + 10, q.y + 104, 24, 128 };
    master[2] = (Rect){ q.x + q.w / 2 - 58, q.y + 252, 116, 34 };
    *cf = (Rect){ q.x + 30, cf_y, q.w - 60, 28 };
}

static void draw_pattern_blocks(SDL_Renderer *r, const FtDeck *deck, Rect q) {
    stroke(r, q, 65);
    int orders = deck_num_orders(deck);
    int cur = deck_current_order(deck);
    if (!deck->loaded || orders <= 0) {
        text(r, q.x + 12, q.y + 18, "NO MODULE", 3, 55);
        return;
    }
    int max_blocks = q.w / 18;
    if (max_blocks < 1) max_blocks = 1;
    for (int i = 0; i < max_blocks && i < orders; i++) {
        int order = (orders <= max_blocks) ? i : (cur - max_blocks / 2 + i);
        if (order < 0 || order >= orders) continue;
        int h = 22 + ((order * 17) % 46);
        Uint8 v = order == cur ? 235 : (Uint8)(65 + (order * 11) % 70);
        Rect b = { q.x + 8 + i * 18, q.y + q.h - 8 - h, 12, h };
        fill(r, b, v);
    }
    textf(r, q.x + 12, q.y + 12, 2, 135, "PATTERN STRUCTURE  %d ORDERS", orders);
}

static void draw_order_progress(SDL_Renderer *r, const FtDeck *deck, Rect q) {
    stroke(r, q, 85);
    if (!deck->loaded) return;
    int orders = deck_num_orders(deck);
    if (orders <= 0) return;
    int current = deck_current_order(deck);
    for (int i = 0; i < orders; i++) {
        int x0 = q.x + 2 + (i * (q.w - 4)) / orders;
        int x1 = q.x + 2 + ((i + 1) * (q.w - 4)) / orders;
        Uint8 v = i == current ? 230 : (i < current ? 120 : 45);
        fill(r, (Rect){ x0, q.y + 2, (x1 > x0 ? x1 - x0 : 1), q.h - 4 }, v);
        if (orders <= 80) line(r, x0, q.y + 1, x0, q.y + q.h - 2, 18);
    }
}

static void draw_pattern_table(SDL_Renderer *r, const FtDeck *deck, Rect q) {
    stroke(r, q, 65);
    if (!deck->loaded) {
        text(r, q.x + 12, q.y + 18, "NO MODULE", 3, 55);
        return;
    }
    int channels = deck_num_channels(deck);
    if (channels <= 0) channels = 4;
    int max_visible_channels = (q.w - 44) / 42;
    if (max_visible_channels < 1) max_visible_channels = 1;
    if (channels > 12) channels = 12;
    if (channels > max_visible_channels) channels = max_visible_channels;
    int cur_row = deck_current_row(deck);
    int rows = deck_current_pattern_rows(deck);
    int scope_h = 34;
    int visible_rows = (q.h - scope_h - 34) / 14;
    if (visible_rows < 5) visible_rows = 5;
    int first_row = cur_row - visible_rows / 2;
    if (first_row < 0) first_row = 0;
    if (first_row + visible_rows > rows) first_row = rows - visible_rows;
    if (first_row < 0) first_row = 0;
    int rownum_w = 34;
    int col_w = (q.w - rownum_w - 10) / channels;
    if (col_w < 42) col_w = 42;

    textf(r, q.x + 8, q.y + 6, 1, 125, "PATTERN %02d  %dCH SHOWN", deck_current_pattern(deck), channels);
    for (int ch = 0; ch < channels; ch++) {
        int x = q.x + rownum_w + ch * col_w;
        float vu = deck_channel_vu(deck, ch);
        if (deck_channel_muted(deck, ch)) {
            fill(r, (Rect){ x, q.y + 18, col_w - 2, q.h - 20 }, 6);
        } else if (vu > 0.01f) {
            int strip_h = q.h - 20;
            int vu_h = (int)((float)strip_h * clampf(vu * 1.9f, 0.0f, 1.0f));
            fill(r, (Rect){ x, q.y + 18 + strip_h - vu_h, col_w - 2, vu_h }, (Uint8)(18 + vu * 42.0f));
        }
        Rect sq = { x + 2, q.y + 20, col_w - 4, scope_h - 4 };
        stroke(r, sq, deck_channel_muted(deck, ch) ? 115 : 55);
        textf(r, x + 2, q.y + 6, 1, deck_channel_muted(deck, ch) ? 80 : 105, "CH%02d", ch + 1);
        int prev_x = sq.x;
        int prev_y = sq.y + sq.h / 2;
        for (int i = 1; i < FT1210_SCOPE_SAMPLES; i++) {
            int idx = (deck->channel_scope_pos + i) % FT1210_SCOPE_SAMPLES;
            int px = sq.x + (i * sq.w) / (FT1210_SCOPE_SAMPLES - 1);
            int py = sq.y + sq.h / 2 - (int)(deck->channel_scope[ch][idx] * (float)(sq.h / 2 - 2));
            line(r, prev_x, prev_y, px, py, deck_channel_muted(deck, ch) ? 65 : 190);
            prev_x = px;
            prev_y = py;
        }
    }
    for (int i = 0; i < visible_rows; i++) {
        int row = first_row + i;
        int y = q.y + scope_h + 28 + i * 14;
        bool active = row == cur_row;
        textf(r, q.x + 6, y, 1, active ? 240 : 90, "%02X", row);
        for (int ch = 0; ch < channels; ch++) {
            char cell[32];
            int x = q.x + rownum_w + ch * col_w;
            deck_format_pattern_cell(deck, row, ch, cell, sizeof(cell));
            text(r, x + 2, y, cell, 1, active ? 245 : (deck_channel_muted(deck, ch) ? 45 : (deck_channel_vu(deck, ch) > 0.01f ? 155 : 75)));
        }
    }
}

static int pattern_table_channel_at(const FtDeck *deck, Rect q, int x, int y) {
    if (!contains(q, x, y) || y < q.y + 18 || y > q.y + 58) return -1;
    int channels = deck_num_channels(deck);
    if (channels <= 0) return -1;
    int max_visible_channels = (q.w - 44) / 42;
    if (max_visible_channels < 1) max_visible_channels = 1;
    if (channels > 12) channels = 12;
    if (channels > max_visible_channels) channels = max_visible_channels;
    int rownum_w = 34;
    int col_w = (q.w - rownum_w - 10) / channels;
    if (col_w < 42) col_w = 42;
    int ch = (x - (q.x + rownum_w)) / col_w;
    return (ch >= 0 && ch < channels) ? ch : -1;
}

static void draw_deck(SDL_Renderer *r, const FtDeck *deck, Rect q, const char *label) {
    Rect pitch, browse, play, cue, setcue, sync, loop, loopdec, loopinc, nudge_back, nudge_fwd, nudge_clear, progress;
    deck_layout(q, &pitch, &browse, &play, &cue, &setcue, &sync, &loop, &loopdec, &loopinc, &nudge_back, &nudge_fwd, &nudge_clear, &progress);
    fill(r, q, 7);
    stroke(r, q, deck->selected ? 235 : 65);
    text(r, q.x + 18, q.y + 18, label, 4, deck->selected ? 240 : 120);
    text(r, q.x + 76, q.y + 18, deck->loaded ? deck->title : "LOAD OR DROP MOD XM MED", 2, deck->loaded ? 175 : 105);
    textf(r, q.x + 76, q.y + 44, 1, 115, "%s", deck->loaded ? deck->format : "OPEN BROWSER BUTTON OR DRAG FILE ON DECK");

    if (deck->loaded) {
        textf(r, q.x + 24, q.y + 76, 4, 210, "%.0f", deck_effective_bpm(deck));
        text(r, q.x + 110, q.y + 86, "BPM", 2, 135);
        textf(r, q.x + 174, q.y + 80, 3, 190, "PAT %02d", deck_current_pattern(deck));
        textf(r, q.x + 304, q.y + 86, 2, 165, "ROW %02d", deck_current_row(deck));
        textf(r, q.x + 410, q.y + 86, 2, 135, "ORD %02d/%02d", deck_current_order(deck) + 1, deck_num_orders(deck));
        textf(r, q.x + 24, q.y + 122, 2, 135, "%dCH  SPD %d  LOOP %s", deck_num_channels(deck), openmpt_module_get_current_speed(deck->module), deck_loop_length_label(deck));
    } else {
        text(r, q.x + 24, q.y + 96, "TRACKER DECK", 3, 70);
        text(r, q.x + 24, q.y + 130, "MODULES ARE ACCEPTED", 1, 80);
    }

    draw_pattern_blocks(r, deck, (Rect){ q.x + 24, q.y + 152, q.w - 120, 82 });
    draw_pattern_table(r, deck, (Rect){ q.x + 24, q.y + 244, q.w - 120, q.h - 420 });
    char pitch_label[32];
    snprintf(pitch_label, sizeof(pitch_label), "%+.1f%%", deck->pitch_percent);
    slider(r, pitch, (float)(deck->pitch_percent / 12.0), true, "PITCH", pitch_label);

    draw_order_progress(r, deck, progress);

    bool narrow = q.w < 520;
    button(r, browse, narrow ? "BRW" : "BROWSE", false);
    button(r, play, deck->playing ? "STOP" : "PLAY", deck->playing);
    button(r, cue, deck->cue_set ? "CUE*" : "CUE", deck->cue_set);
    button(r, setcue, "SET", false);
    button(r, sync, "SYNC", deck->sync_enabled);
    button(r, loop, "LOOP", deck->loop_enabled);
    button(r, loopdec, "L-", false);
    if (!narrow) text(r, loopdec.x + 48, loopdec.y + 8, deck_loop_length_label(deck), 1, 150);
    button(r, loopinc, "L+", false);
    button(r, nudge_back, "N-", deck->nudge_percent < 0.0);
    button(r, nudge_fwd, "N+", deck->nudge_percent > 0.0);
    button(r, nudge_clear, "CLR", deck->nudge_percent != 0.0);
}

static void draw_mixer(SDL_Renderer *r, const FtAudio *audio, const FtDeck decks[2], Rect q) {
    Rect eq[2][3], vol[2], gain[2], master[3], cf;
    mixer_layout(q, eq, vol, gain, master, &cf);
    fill(r, q, 6);
    stroke(r, q, 75);
    text(r, q.x + q.w / 2 - 45, q.y + 18, "MIXER", 3, 170);
    text(r, q.x + 28, q.y + 52, "DECK A", 2, decks[0].selected ? 235 : 120);
    text(r, q.x + q.w - 112, q.y + 52, "DECK B", 2, decks[1].selected ? 235 : 120);
    meter(r, master[0], decks[0].vu_l, decks[0].vu_r);
    meter(r, master[1], decks[1].vu_l, decks[1].vu_r);
    button(r, master[2], decks[0].sync_enabled ? "MASTER B" : (decks[1].sync_enabled ? "MASTER A" : "FREE"), decks[0].sync_enabled || decks[1].sync_enabled);
    for (int d = 0; d < 2; d++) {
        char gain_label[24];
        snprintf(gain_label, sizeof(gain_label), "%+.0fDB", decks[d].gain_db);
        knob(r, gain[d], decks[d].gain_db / 12.0f, "GAIN", gain_label);
        knob(r, eq[d][0], eq_knob_value(decks[d].eq_db[0]), "HI", NULL);
        knob(r, eq[d][1], eq_knob_value(decks[d].eq_db[1]), "MID", NULL);
        knob(r, eq[d][2], eq_knob_value(decks[d].eq_db[2]), "LO", NULL);
        slider(r, vol[d], decks[d].volume / 1.25f, false, "VOL", NULL);
    }
    text(r, cf.x, cf.y - 22, "CROSSFADER", 1, 115);
    stroke(r, cf, 85);
    int pos = cf.x + (int)(((audio->crossfader + 1.0f) * 0.5f) * (float)(cf.w - 18));
    Rect cf_cap = { pos, cf.y - 7, 18, cf.h + 14 };
    fill_rgb(r, cf_cap, 62, 150, 152);
    stroke_rgb(r, cf_cap, 150, 232, 226);
}

static void draw_browser(SDL_Renderer *r, const FtUi *ui) {
    if (!ui->browser_open) return;
    Rect q = { ui->width / 2 - 360, 82, 720, ui->height - 132 };
    fill(r, q, 4);
    stroke(r, q, 230);
    textf(r, q.x + 18, q.y + 18, 3, 220, "LOAD TO DECK %c", ui->browser_deck == 0 ? 'A' : 'B');
    text(r, q.x + q.w - 70, q.y + 18, "ESC", 2, 130);
    text(r, q.x + 18, q.y + 52, browser_cwd, 1, 125);
    int y = q.y + 82;
    int row_h = 24;
    for (int i = 0; i < browser_count && y + row_h < q.y + q.h - 18; i++, y += row_h) {
        fill(r, (Rect){ q.x + 16, y, q.w - 32, row_h - 3 }, i % 2 ? 10 : 7);
        text(r, q.x + 26, y + 5, browser_entries[i].dir ? "DIR" : "MOD", 1, browser_entries[i].dir ? 170 : 125);
        text(r, q.x + 72, y + 5, browser_entries[i].name, 1, browser_entries[i].dir ? 220 : 180);
    }
}

static FtDeck *selected_deck(FtDeck decks[2]) { return decks[0].selected ? &decks[0] : &decks[1]; }
static FtDeck *other_deck(FtDeck decks[2]) { return decks[0].selected ? &decks[1] : &decks[0]; }
static void select_deck(FtDeck decks[2], int i) { decks[0].selected = i == 0; decks[1].selected = i == 1; }

static void apply_drag(FtUi *ui, FtAudio *audio, FtDeck decks[2], int x, int y) {
    audio_lock(audio);
    if (ui->drag_kind == DRAG_CROSSFADER) {
        Rect deck_rects[2], mix, eq[2][3], vol[2], gain[2], master[3], cf;
        app_layout(ui, deck_rects, &mix);
        mixer_layout(mix, eq, vol, gain, master, &cf);
        audio->crossfader = clampf(((float)(x - cf.x) / (float)cf.w) * 2.0f - 1.0f, -1.0f, 1.0f);
    } else if (ui->drag_deck >= 0 && ui->drag_deck < 2) {
        FtDeck *deck = &decks[ui->drag_deck];
        if (ui->drag_kind == DRAG_PITCH) {
            Rect deck_rects[2], mix, pitch, browse, play, cue, setcue, sync, loop, loopdec, loopinc, nb, nf, nc, progress;
            app_layout(ui, deck_rects, &mix);
            deck_layout(deck_rects[ui->drag_deck], &pitch, &browse, &play, &cue, &setcue, &sync, &loop, &loopdec, &loopinc, &nb, &nf, &nc, &progress);
            float f = clampf(1.0f - (float)(y - pitch.y) / (float)pitch.h, 0.0f, 1.0f);
            deck_set_pitch(deck, ((double)f * 24.0) - 12.0);
        } else if (ui->drag_kind == DRAG_VOLUME || ui->drag_kind == DRAG_EQ || ui->drag_kind == DRAG_GAIN) {
            Rect deck_rects[2], mix, eq[2][3], vol[2], gain[2], master[3], cf;
            app_layout(ui, deck_rects, &mix);
            mixer_layout(mix, eq, vol, gain, master, &cf);
            if (ui->drag_kind == DRAG_VOLUME) {
                deck->volume = clampf((1.0f - (float)(y - vol[ui->drag_deck].y) / (float)vol[ui->drag_deck].h) * 1.25f, 0.0f, 1.25f);
            } else if (ui->drag_kind == DRAG_GAIN) {
                float f = clampf(1.0f - (float)(y - gain[ui->drag_deck].y) / (float)gain[ui->drag_deck].h, 0.0f, 1.0f);
                deck->gain_db = -12.0f + f * 24.0f;
            } else if (ui->drag_band >= 0 && ui->drag_band < 3) {
                float f = clampf(1.0f - (float)(y - eq[ui->drag_deck][ui->drag_band].y) / (float)eq[ui->drag_deck][ui->drag_band].h, 0.0f, 1.0f);
                deck->eq_db[ui->drag_band] = -26.0f + f * 32.0f;
            }
        }
    }
    audio_unlock(audio);
}

bool ui_open(FtUi *ui, char *err, int err_len) {
    memset(ui, 0, sizeof(*ui));
    ui->width = 1200;
    ui->height = 680;
    ui->running = true;
    ui->drag_deck = -1;
    ui->drag_band = -1;
    if (browser_cwd[0] == '.' && !getcwd(browser_cwd, sizeof(browser_cwd))) {
        snprintf(browser_cwd, sizeof(browser_cwd), ".");
    }
    browser_refresh();
    ui->window = SDL_CreateWindow("FT-1210", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ui->width, ui->height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!ui->window) { if (err && err_len > 0) snprintf(err, (size_t)err_len, "%s", SDL_GetError()); return false; }
    ui->renderer = SDL_CreateRenderer(ui->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ui->renderer) { if (err && err_len > 0) snprintf(err, (size_t)err_len, "%s", SDL_GetError()); SDL_DestroyWindow(ui->window); return false; }
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    SDL_EventState(SDL_DROPTEXT, SDL_ENABLE);
    SDL_EventState(SDL_DROPBEGIN, SDL_ENABLE);
    SDL_EventState(SDL_DROPCOMPLETE, SDL_ENABLE);
    return true;
}

void ui_close(FtUi *ui) {
    if (ui->renderer) SDL_DestroyRenderer(ui->renderer);
    if (ui->window) SDL_DestroyWindow(ui->window);
    memset(ui, 0, sizeof(*ui));
}

int ui_deck_at(const FtUi *ui, int x, int y) {
    Rect deck_rects[2], mix;
    app_layout(ui, deck_rects, &mix);
    if (contains(deck_rects[0], x, y)) return 0;
    if (contains(deck_rects[1], x, y)) return 1;
    return -1;
}

static bool handle_deck_click(FtUi *ui, FtAudio *audio, FtDeck decks[2], int deck_index, Rect q, int x, int y, int clicks) {
    Rect pitch, browse, play, cue, setcue, sync, loop, loopdec, loopinc, nudge_back, nudge_fwd, nudge_clear, progress;
    deck_layout(q, &pitch, &browse, &play, &cue, &setcue, &sync, &loop, &loopdec, &loopinc, &nudge_back, &nudge_fwd, &nudge_clear, &progress);
    if (!contains(q, x, y)) return false;
    select_deck(decks, deck_index);

    audio_lock(audio);
    FtDeck *deck = &decks[deck_index];
    FtDeck *other = &decks[deck_index == 0 ? 1 : 0];
    Rect table = { q.x + 24, q.y + 244, q.w - 120, q.h - 420 };
    int mute_ch = pattern_table_channel_at(deck, table, x, y);
    if (clicks >= 2 && contains(pitch, x, y)) {
        deck_set_pitch(deck, 0.0);
    } else if (mute_ch >= 0) {
        deck_toggle_channel_mute(deck, mute_ch);
    } else if (contains(browse, x, y)) { ui->browser_open = true; ui->browser_deck = deck_index; }
    else if (contains(play, x, y)) deck_play_pause(deck);
    else if (contains(cue, x, y)) deck_cue(deck);
    else if (contains(setcue, x, y)) deck_set_cue_current(deck);
    else if (contains(sync, x, y)) {
        deck->sync_enabled = !deck->sync_enabled;
        if (other->loaded && deck_base_bpm(other) > 1.0) deck->sync_target_bpm = deck_effective_bpm(other);
    } else if (contains(loop, x, y)) deck_toggle_loop(deck);
    else if (contains(loopdec, x, y)) deck_loop_length_prev(deck);
    else if (contains(loopinc, x, y)) deck_loop_length_next(deck);
    else if (contains(nudge_back, x, y)) { deck_nudge(deck, -3.0); ui->drag_kind = DRAG_NUDGE; ui->drag_deck = deck_index; }
    else if (contains(nudge_fwd, x, y)) { deck_nudge(deck, 3.0); ui->drag_kind = DRAG_NUDGE; ui->drag_deck = deck_index; }
    else if (contains(nudge_clear, x, y)) deck_nudge(deck, 0.0);
    else if (contains(progress, x, y)) {
        int orders = deck_num_orders(deck);
        if (orders > 0) deck_seek_order(deck, ((x - progress.x) * orders) / progress.w);
    }
    else if (contains(pitch, x, y)) { ui->drag_kind = DRAG_PITCH; ui->drag_deck = deck_index; }
    audio_unlock(audio);

    if (ui->drag_kind != DRAG_NONE) apply_drag(ui, audio, decks, x, y);
    return true;
}

static bool handle_deck_right_click(FtAudio *audio, FtDeck decks[2], int deck_index, Rect q, int x, int y) {
    Rect pitch, browse, play, cue, setcue, sync, loop, loopdec, loopinc, nudge_back, nudge_fwd, nudge_clear, progress;
    deck_layout(q, &pitch, &browse, &play, &cue, &setcue, &sync, &loop, &loopdec, &loopinc, &nudge_back, &nudge_fwd, &nudge_clear, &progress);
    if (!contains(q, x, y)) return false;
    if (contains(cue, x, y) || contains(setcue, x, y)) {
        audio_lock(audio);
        deck_clear_cue(&decks[deck_index]);
        audio_unlock(audio);
        return true;
    }
    return false;
}

static bool handle_mixer_click(FtUi *ui, FtAudio *audio, FtDeck decks[2], Rect q, int x, int y, int clicks) {
    if (!contains(q, x, y)) return false;
    Rect eq[2][3], vol[2], gain[2], master[3], cf;
    mixer_layout(q, eq, vol, gain, master, &cf);
    audio_lock(audio);
    if (clicks >= 2 && contains(cf, x, y)) {
        audio->crossfader = 0.0f;
    } else if (contains(master[2], x, y)) {
        if (!decks[0].sync_enabled && !decks[1].sync_enabled) {
            decks[1].sync_enabled = true;
            decks[1].sync_target_bpm = deck_effective_bpm(&decks[0]);
        } else if (decks[1].sync_enabled) {
            decks[1].sync_enabled = false;
            decks[0].sync_enabled = true;
            decks[0].sync_target_bpm = deck_effective_bpm(&decks[1]);
        } else {
            decks[0].sync_enabled = false;
            decks[1].sync_enabled = false;
        }
    } else if (contains(cf, x, y)) {
        ui->drag_kind = DRAG_CROSSFADER;
    } else {
        for (int d = 0; d < 2; d++) {
            if (clicks >= 2 && contains(vol[d], x, y)) { decks[d].volume = 1.0f; break; }
            if (clicks >= 2 && contains(gain[d], x, y)) { decks[d].gain_db = 0.0f; break; }
            if (contains(vol[d], x, y)) { ui->drag_kind = DRAG_VOLUME; ui->drag_deck = d; break; }
            if (contains(gain[d], x, y)) { ui->drag_kind = DRAG_GAIN; ui->drag_deck = d; break; }
            for (int i = 0; i < 3; i++) {
                if (clicks >= 2 && contains(eq[d][i], x, y)) { decks[d].eq_db[i] = 0.0f; break; }
                if (contains(eq[d][i], x, y)) { ui->drag_kind = DRAG_EQ; ui->drag_deck = d; ui->drag_band = i; break; }
            }
        }
    }
    audio_unlock(audio);
    if (ui->drag_kind != DRAG_NONE) apply_drag(ui, audio, decks, x, y);
    return true;
}

static bool handle_browser_click(FtUi *ui, FtAudio *audio, FtDeck decks[2], int x, int y) {
    if (!ui->browser_open) return false;
    Rect q = { ui->width / 2 - 360, 82, 720, ui->height - 132 };
    if (!contains(q, x, y)) { ui->browser_open = false; return true; }
    int row_h = 24;
    int idx = (y - (q.y + 82)) / row_h;
    if (idx < 0 || idx >= browser_count) return true;
    if (browser_entries[idx].dir) {
        browser_enter(browser_entries[idx].name);
    } else {
        char path[1400];
        snprintf(path, sizeof(path), "%s/%s", browser_cwd, browser_entries[idx].name);
        load_path(audio, &decks[ui->browser_deck], path);
        select_deck(decks, ui->browser_deck);
        ui->browser_open = false;
    }
    return true;
}

void ui_handle_event(FtUi *ui, FtAudio *audio, FtDeck decks[2], const SDL_Event *event) {
    if (event->type == SDL_QUIT) { ui->running = false; return; }
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) { ui->width = event->window.data1; ui->height = event->window.data2; return; }
    if (event->type == SDL_MOUSEBUTTONUP) {
        if (ui->drag_kind == DRAG_NUDGE && ui->drag_deck >= 0 && ui->drag_deck < 2) {
            audio_lock(audio);
            deck_nudge(&decks[ui->drag_deck], 0.0);
            audio_unlock(audio);
        }
        ui->drag_kind = DRAG_NONE; ui->drag_deck = -1; ui->drag_band = -1; return;
    }
    if (event->type == SDL_MOUSEMOTION && ui->drag_kind != DRAG_NONE) { apply_drag(ui, audio, decks, event->motion.x, event->motion.y); return; }
    if (event->type == SDL_MOUSEWHEEL) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        Rect deck_rects[2], mix, eq[2][3], vol[2], gain[2], master[3], cf;
        app_layout(ui, deck_rects, &mix);
        mixer_layout(mix, eq, vol, gain, master, &cf);
        audio_lock(audio);
        for (int d = 0; d < 2; d++) {
            if (contains(gain[d], mx, my)) decks[d].gain_db = clampf(decks[d].gain_db + event->wheel.y * 0.5f, -12.0f, 12.0f);
            for (int i = 0; i < 3; i++) {
                if (contains(eq[d][i], mx, my)) decks[d].eq_db[i] = clampf(decks[d].eq_db[i] + event->wheel.y * 1.0f, -26.0f, 6.0f);
            }
        }
        audio_unlock(audio);
        return;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT) {
        Rect deck_rects[2], mix;
        app_layout(ui, deck_rects, &mix);
        if (handle_deck_right_click(audio, decks, 0, deck_rects[0], event->button.x, event->button.y)) return;
        if (handle_deck_right_click(audio, decks, 1, deck_rects[1], event->button.x, event->button.y)) return;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (handle_browser_click(ui, audio, decks, event->button.x, event->button.y)) return;
        Rect deck_rects[2], mix;
        app_layout(ui, deck_rects, &mix);
        if (handle_deck_click(ui, audio, decks, 0, deck_rects[0], event->button.x, event->button.y, event->button.clicks)) return;
        if (handle_deck_click(ui, audio, decks, 1, deck_rects[1], event->button.x, event->button.y, event->button.clicks)) return;
        if (handle_mixer_click(ui, audio, decks, mix, event->button.x, event->button.y, event->button.clicks)) return;
    }
    if (event->type == SDL_KEYUP) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_LEFT || key == SDLK_RIGHT) { audio_lock(audio); deck_nudge(selected_deck(decks), 0.0); audio_unlock(audio); }
        return;
    }
    if (event->type != SDL_KEYDOWN || event->key.repeat) return;

    FtDeck *deck = selected_deck(decks);
    FtDeck *other = other_deck(decks);
    SDL_Keycode key = event->key.keysym.sym;
    SDL_Keymod mod = SDL_GetModState();
    audio_lock(audio);
    switch (key) {
    case SDLK_ESCAPE: if (ui->browser_open) ui->browser_open = false; else ui->running = false; break;
    case SDLK_b: ui->browser_open = true; ui->browser_deck = decks[0].selected ? 0 : 1; browser_refresh(); break;
    case SDLK_TAB: select_deck(decks, decks[0].selected ? 1 : 0); break;
    case SDLK_1: select_deck(decks, 0); break;
    case SDLK_2: select_deck(decks, 1); break;
    case SDLK_SPACE: deck_play_pause(deck); break;
    case SDLK_c: deck_cue(deck); break;
    case SDLK_F3: deck_set_cue_current(deck); break;
    case SDLK_F1: deck_jump_cue(deck); break;
    case SDLK_s: deck->sync_enabled = !deck->sync_enabled; if (other->loaded && deck_base_bpm(other) > 1.0) deck->sync_target_bpm = deck_effective_bpm(other); break;
    case SDLK_l: deck_toggle_loop(deck); break;
    case SDLK_F5: deck_toggle_loop(deck); break;
    case SDLK_F6: deck_loop_length_prev(deck); break;
    case SDLK_F7: deck_loop_length_next(deck); break;
    case SDLK_i: deck_set_loop_in(deck); break;
    case SDLK_o: deck_set_loop_out(deck); break;
    case SDLK_n: deck_nudge(deck, 0.0); break;
    case SDLK_LEFT: deck_nudge(deck, -3.0); break;
    case SDLK_RIGHT: deck_nudge(deck, 3.0); break;
    case SDLK_UP: deck_set_pitch(deck, deck->pitch_percent + ((mod & KMOD_SHIFT) ? 0.1 : 1.0)); break;
    case SDLK_DOWN: deck_set_pitch(deck, deck->pitch_percent - ((mod & KMOD_SHIFT) ? 0.1 : 1.0)); break;
    case SDLK_HOME: deck_set_pitch(deck, 0.0); break;
    case SDLK_PAGEUP: deck_seek_relative(deck, 5.0); break;
    case SDLK_PAGEDOWN: deck_seek_relative(deck, -5.0); break;
    case SDLK_q: deck->eq_db[0] = clampf(deck->eq_db[0] + 1.0f, -26.0f, 6.0f); break;
    case SDLK_a: deck->eq_db[0] = clampf(deck->eq_db[0] - 1.0f, -26.0f, 6.0f); break;
    case SDLK_w: deck->eq_db[1] = clampf(deck->eq_db[1] + 1.0f, -26.0f, 6.0f); break;
    case SDLK_z: deck->eq_db[1] = clampf(deck->eq_db[1] - 1.0f, -26.0f, 6.0f); break;
    case SDLK_e: deck->eq_db[2] = clampf(deck->eq_db[2] + 1.0f, -26.0f, 6.0f); break;
    case SDLK_d: deck->eq_db[2] = clampf(deck->eq_db[2] - 1.0f, -26.0f, 6.0f); break;
    case SDLK_EQUALS: deck->volume = clampf(deck->volume + 0.05f, 0.0f, 1.25f); break;
    case SDLK_MINUS: deck->volume = clampf(deck->volume - 0.05f, 0.0f, 1.25f); break;
    case SDLK_LEFTBRACKET: audio->crossfader = clampf(audio->crossfader - 0.1f, -1.0f, 1.0f); break;
    case SDLK_RIGHTBRACKET: audio->crossfader = clampf(audio->crossfader + 0.1f, -1.0f, 1.0f); break;
    default: break;
    }
    audio_unlock(audio);
}

void ui_render(FtUi *ui, const FtAudio *audio, const FtDeck decks[2]) {
    SDL_SetRenderDrawColor(ui->renderer, 2, 2, 2, 255);
    SDL_RenderClear(ui->renderer);
    Rect deck_rects[2], mix;
    app_layout(ui, deck_rects, &mix);
    text(ui->renderer, 22, 22, "FT-1210", 3, 190);
    text(ui->renderer, 22, 50, "INSPIRED BY DJ H0FFMAN'S PT-1210", 1, 105);
    draw_deck(ui->renderer, &decks[0], deck_rects[0], "A");
    draw_deck(ui->renderer, &decks[1], deck_rects[1], "B");
    draw_mixer(ui->renderer, audio, decks, mix);
    draw_browser(ui->renderer, ui);
    SDL_RenderPresent(ui->renderer);
}
