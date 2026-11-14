/* MiniSharp - the character grid behind Console (see textgrid.h). */
#include "textgrid.h"
#include "ms.h"
#include <stdlib.h>
#include <string.h>

struct TextGrid {
    int w, h, x, y;                     /* x may be w: the line is full, the next character wraps */
    int fg, bg;
    uint32_t version;
    TgCell* cells;
};

TextGrid* tg_new(int w, int h) {
    if (w < 1 || h < 1 || w > 1000 || h > 1000) return NULL;
    TextGrid* g = (TextGrid*)calloc(1, sizeof(TextGrid));
    if (!g) return NULL;
    g->cells = (TgCell*)malloc(sizeof(TgCell) * (size_t)w * (size_t)h);
    if (!g->cells) { free(g); return NULL; }
    g->w = w; g->h = h;
    g->fg = TG_GRAY; g->bg = TG_BLACK;
    tg_clear(g);
    return g;
}

void tg_free(TextGrid* g) {
    if (!g) return;
    free(g->cells);
    free(g);
}

int tg_width(const TextGrid* g) { return g->w; }
int tg_height(const TextGrid* g) { return g->h; }
int tg_cursor_x(const TextGrid* g) { return g->x < g->w ? g->x : g->w - 1; }
int tg_cursor_y(const TextGrid* g) { return g->y; }
int tg_fg(const TextGrid* g) { return g->fg; }
int tg_bg(const TextGrid* g) { return g->bg; }
uint32_t tg_version(const TextGrid* g) { return g->version; }

static void blank(TextGrid* g, TgCell* c, int n) {
    for (int i = 0; i < n; i++) { c[i].cp = ' '; c[i].fg = (uint8_t)g->fg; c[i].bg = (uint8_t)g->bg; }
}

void tg_clear(TextGrid* g) {
    blank(g, g->cells, g->w * g->h);
    g->x = g->y = 0;
    g->version++;
}

int tg_set_cursor(TextGrid* g, int x, int y) {
    if (x < 0 || y < 0 || x >= g->w || y >= g->h) return 0;
    g->x = x; g->y = y;
    g->version++;
    return 1;
}

void tg_set_colors(TextGrid* g, int fg, int bg) {
    if (fg >= 0 && fg < 16) g->fg = fg;
    if (bg >= 0 && bg < 16) g->bg = bg;
}

void tg_reset_colors(TextGrid* g) { g->fg = TG_GRAY; g->bg = TG_BLACK; }

const TgCell* tg_cell(const TextGrid* g, int x, int y) {
    if (x < 0 || y < 0 || x >= g->w || y >= g->h) return NULL;
    return &g->cells[y * g->w + x];
}

/* the cursor goes one line down; below the last line everything moves up and the new line is empty */
static void line_feed(TextGrid* g) {
    g->x = 0;
    if (g->y < g->h - 1) { g->y++; return; }
    memmove(g->cells, g->cells + g->w, sizeof(TgCell) * (size_t)g->w * (size_t)(g->h - 1));
    blank(g, g->cells + g->w * (g->h - 1), g->w);
}

static void put(TextGrid* g, uint32_t cp) {
    if (g->x >= g->w) line_feed(g);             /* the line was full: wrap now */
    TgCell* c = &g->cells[g->y * g->w + g->x];
    c->cp = cp; c->fg = (uint8_t)g->fg; c->bg = (uint8_t)g->bg;
    g->x++;
}

void tg_write(TextGrid* g, const char* s) {
    if (!s || !*s) return;
    int len = (int)strlen(s);
    for (int i = 0; i < len; ) {
        int adv;
        int cp = ms_utf8_decode(s + i, len - i, &adv);
        i += adv;
        switch (cp) {
        case '\n': line_feed(g); break;
        case '\r': g->x = 0; break;
        case '\t': {
            int to = (g->x / 8 + 1) * 8;
            if (to > g->w) to = g->w;
            while (g->x < to) put(g, ' ');
            break;
        }
        case '\b': if (g->x > 0) g->x--; if (g->x >= g->w) g->x = g->w - 1; break;
        default: if (cp >= 32) put(g, (uint32_t)cp); break;
        }
    }
    g->version++;
}

int tg_row_text(const TextGrid* g, int y, char* out, int cap) {
    if (cap <= 0) return 0;
    out[0] = 0;
    if (y < 0 || y >= g->h) return 0;
    const TgCell* row = &g->cells[y * g->w];
    int end = g->w;
    while (end > 0 && row[end - 1].cp == ' ') end--;
    int o = 0;
    for (int x = 0; x < end; x++) {
        char b[4];
        int n = ms_utf8_encode((int)row[x].cp, b);
        if (o + n >= cap) break;
        memcpy(out + o, b, n); o += n;
    }
    out[o] = 0;
    return o;
}

uint32_t tg_palette(int c) {
    static const uint32_t P[16] = {
        0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0xC0C0C0,
        0x808080, 0x0000FF, 0x00FF00, 0x00FFFF, 0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF,
    };
    return c >= 0 && c < 16 ? P[c] : 0;
}
