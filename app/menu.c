/* MiniSharp - the main menu of the native app (see menu.h). */
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_LEN 400
#define COLS 3
#define ROWS 2
#define GAP 4
#define TILE_H 62
#define RECENT_Y (GAP + ROWS * (TILE_H + GAP) + 18)     /* below the tiles and the "Recent" label */
#define RECENT_H 26

typedef struct { const char* label; const char* key; const char* about; int action; uint32_t rgb; } Tile;

static const Tile TILES[MENU_TILES] = {
    { "Programs", "B", "Programs and projects on the card: open one in the editor, run it, make a new one.",
      MENU_PROGRAMS, 0x2F6B3A },
    { "Run main.cs", "", "Runs main.cs from the app folder on the console screen.", MENU_RUN_MAIN, 0x2D4A80 },
    { "Terminal", "Y", "Type a line of MiniSharp and it runs at once (the REPL).", MENU_TERMINAL, 0x34405A },
    { "Help", "R", "The chapters about the language, with examples to copy into the editor.", MENU_HELP, 0x5A4A2A },
    { "Tests", "X", "Runs the test files from the tests folder (sent from the PC); results go to tests/out.",
      MENU_TESTS, 0x3A3A4A },
    { "Quit", "START", "Back to the Homebrew Launcher.", MENU_QUIT, 0x5A2A2A },
};

struct Menu {
    char root[PATH_LEN], recentFile[PATH_LEN];
    char build[120], status[200];
    int statusError;
    char recent[MENU_RECENT_MAX][PATH_LEN];
    int nrecent;
    int sel;
    char chosen[PATH_LEN];
};

/* ---------- the recent files ---------- */
static int file_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void load_recent(Menu* m) {
    m->nrecent = 0;
    FILE* f = m->recentFile[0] ? fopen(m->recentFile, "r") : NULL;
    if (!f) return;
    char line[PATH_LEN];
    while (m->nrecent < MENU_RECENT_MAX && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] && file_exists(line)) snprintf(m->recent[m->nrecent++], PATH_LEN, "%s", line);   /* gone = skip */
    }
    fclose(f);
}

static void save_recent(const Menu* m) {
    FILE* f = m->recentFile[0] ? fopen(m->recentFile, "w") : NULL;
    if (!f) return;
    for (int i = 0; i < m->nrecent; i++) fprintf(f, "%s\n", m->recent[i]);
    fclose(f);
}

void menu_add_recent(Menu* m, const char* path) {
    if (!m || !path || !path[0]) return;
    char p[PATH_LEN];
    snprintf(p, sizeof p, "%s", path);
    int at = m->nrecent < MENU_RECENT_MAX ? m->nrecent : MENU_RECENT_MAX - 1;   /* the oldest falls off */
    for (int i = 0; i < m->nrecent; i++) if (!strcmp(m->recent[i], p)) { at = i; break; }
    if (at == m->nrecent) m->nrecent++;
    for (int i = at; i > 0; i--) memcpy(m->recent[i], m->recent[i - 1], PATH_LEN);
    snprintf(m->recent[0], PATH_LEN, "%s", p);
    save_recent(m);
}

/* ---------- life ---------- */
Menu* menu_new(const char* root, const char* recentFile) {
    Menu* m = calloc(1, sizeof *m);
    if (!m) return NULL;
    snprintf(m->root, sizeof m->root, "%s", root ? root : "");
    snprintf(m->recentFile, sizeof m->recentFile, "%s", recentFile ? recentFile : "");
    load_recent(m);
    return m;
}

void menu_free(Menu* m) { free(m); }

void menu_set_build(Menu* m, const char* build) { if (m) snprintf(m->build, sizeof m->build, "%s", build ? build : ""); }

void menu_set_status(Menu* m, const char* text, int error) {
    if (!m) return;
    snprintf(m->status, sizeof m->status, "%s", text ? text : "");
    m->statusError = error;
}

const char* menu_chosen(const Menu* m) { return m->chosen; }
int menu_selected(const Menu* m) { return m->sel; }
int menu_recent_count(const Menu* m) { return m->nrecent; }
const char* menu_recent(const Menu* m, int i) { return i >= 0 && i < m->nrecent ? m->recent[i] : ""; }
const char* menu_status(const Menu* m) { return m->status; }

/* ---------- layout: the tiles and the recent rows on the touch screen ---------- */
typedef struct { int info, touch; float infoW, infoH, w, h, y0; } Layout;

static Layout layout(void) {
    Layout L = { 0, 1, 400, 240, 320, 240, 0 };
    const UiBackend* b = ui_backend();
    if (!b) return L;
    if (b->screens >= 2) {
        L.touch = b->touchScreen >= 0 ? b->touchScreen : 1;
        L.info = !L.touch;
        L.infoW = (float)b->w[L.info]; L.infoH = (float)b->h[L.info];
        L.w = (float)b->w[L.touch]; L.h = (float)b->h[L.touch];
    } else {                                        /* one screen: the info on top, the tiles below it */
        L.info = L.touch = 0;
        L.infoW = L.w = (float)b->w[0];
        L.infoH = 70;
        L.y0 = 70;
        L.h = (float)b->h[0] - 70;
    }
    return L;
}

static void tile_rect(const Layout* L, int i, float* x, float* y, float* w, float* h) {
    *w = (L->w - GAP * (COLS + 1)) / COLS;
    *h = TILE_H;
    *x = GAP + (i % COLS) * (*w + GAP);
    *y = L->y0 + GAP + (i / COLS) * (TILE_H + GAP);
}

static int recent_shown(const Menu* m, const Layout* L) {
    int room = (int)((L->y0 + L->h - (L->y0 + RECENT_Y)) / RECENT_H);
    int n = m->nrecent < MENU_RECENT_SHOWN ? m->nrecent : MENU_RECENT_SHOWN;
    return n < room ? n : (room > 0 ? room : 0);
}

static int hit(const Menu* m, const Layout* L, int px, int py) {
    for (int i = 0; i < MENU_TILES; i++) {
        float x, y, w, h;
        tile_rect(L, i, &x, &y, &w, &h);
        if (px >= x && px < x + w && py >= y && py < y + h) return i;
    }
    float ry = L->y0 + RECENT_Y;
    for (int i = 0; i < recent_shown(m, L); i++)
        if (py >= ry + i * RECENT_H && py < ry + (i + 1) * RECENT_H && px >= GAP && px < L->w - GAP) return MENU_TILES + i;
    return -1;
}

/* ---------- input ---------- */
static int start(Menu* m, int i) {
    m->sel = i;
    if (i < MENU_TILES) return TILES[i].action;
    snprintf(m->chosen, sizeof m->chosen, "%s", m->recent[i - MENU_TILES]);
    return MENU_OPEN_RECENT;
}

static void move(Menu* m, const Layout* L, int dx, int dy) {
    int nr = recent_shown(m, L);
    int s = m->sel;
    if (s >= MENU_TILES) {                          /* in the recent rows: up / down, up from the first = the tiles */
        int r = s - MENU_TILES + dy;
        if (r < 0) s = (ROWS - 1) * COLS;
        else if (r < nr) s = MENU_TILES + r;
    } else if (dy) {
        int row = s / COLS + dy;
        if (row >= 0 && row < ROWS) s = row * COLS + s % COLS;
        else if (row >= ROWS && nr) s = MENU_TILES;
    } else {
        int col = s % COLS + dx;
        if (col >= 0 && col < COLS) s = (s / COLS) * COLS + col;
    }
    m->sel = s;
}

int menu_update(Menu* m, const UiInput* in) {
    if (!m || !in) return MENU_STAY;
    Layout L = layout();
    if (m->sel >= MENU_TILES + recent_shown(m, &L)) m->sel = 0;
    if (in->touchDown) {
        int i = hit(m, &L, in->x, in->y);
        if (i >= 0) return start(m, i);
    }
    uint32_t d = in->down;
    if (d & UI_BTN_START) return start(m, 5);
    if (d & UI_BTN_B) return start(m, 0);
    if (d & UI_BTN_Y) return start(m, 2);
    if (d & UI_BTN_R) return start(m, 3);
    if (d & UI_BTN_X) return start(m, 4);
    if (d & UI_BTN_A) return start(m, m->sel);
    if (d & UI_BTN_LEFT) move(m, &L, -1, 0);
    if (d & UI_BTN_RIGHT) move(m, &L, 1, 0);
    if (d & UI_BTN_UP) move(m, &L, 0, -1);
    if (d & UI_BTN_DOWN) move(m, &L, 0, 1);
    return MENU_STAY;
}

/* ---------- drawing ---------- */
static uint32_t lighter(uint32_t c) {
    uint32_t r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    r = r + (255 - r) / 3; g = g + (255 - g) / 3; b = b + (255 - b) / 3;
    return (r << 16) | (g << 8) | b;
}

/* a long text in lines of at most maxW pixels (cut at spaces) */
static float wrap(float x, float y, float maxW, const char* s, float size, uint32_t rgb) {
    char line[200];
    while (*s) {
        int n = 0, lastSpace = -1;
        while (s[n] && n < (int)sizeof line - 1) {
            line[n] = s[n];
            line[n + 1] = 0;
            if (s[n] == ' ') lastSpace = n;
            if (ui_text_width(line, size) > maxW && lastSpace > 0) { n = lastSpace; break; }
            n++;
        }
        line[n] = 0;
        ui_text(x, y, line, size, rgb);
        y += size + 3;
        s += n;
        while (*s == ' ') s++;
    }
    return y;
}

static const char* short_path(const Menu* m, const char* p) {
    size_t n = strlen(m->root);
    if (n && !strncmp(p, m->root, n) && p[n] == '/') return p + n + 1;
    return p;
}

static void draw_info(const Menu* m, const Layout* L) {
    ui_screen(L->info);
    if (L->info == L->touch) {                      /* one screen: a short header only */
        ui_text(8, 6, "MiniSharp2", 20, 0x7FB2FF);
        if (m->status[0]) ui_text(8, 40, m->status, 12, m->statusError ? 0xFF7070 : 0x90E090);
        return;
    }
    ui_rect(0, 0, L->infoW, 4, 0x2D4A80);
    ui_text(16, 18, "MiniSharp2", 30, 0x7FB2FF);
    if (m->build[0]) ui_text(18, 54, m->build, 11, 0x8090A0);
    const Tile* t = m->sel < MENU_TILES ? &TILES[m->sel] : NULL;
    float y = 84;
    if (t) {
        char head[64];
        snprintf(head, sizeof head, t->key[0] ? "%s  (%s)" : "%s", t->label, t->key);
        ui_text(16, y, head, 16, 0xFFFFFF);
        wrap(16, y + 22, L->infoW - 32, t->about, 12, 0xA0A8B0);
    } else {
        ui_text(16, y, "Open in the editor", 16, 0xFFFFFF);
        wrap(16, y + 22, L->infoW - 32, short_path(m, m->recent[m->sel - MENU_TILES]), 12, 0xF0D080);
    }
    ui_text(16, L->infoH - 40, "Tap a tile, or D-pad + A.  START+SELECT stops a program.", 11, 0x707880);
    if (m->status[0]) {
        ui_rect(0, L->infoH - 22, L->infoW, 22, m->statusError ? 0x4A2020 : 0x1C2A20);
        ui_text(10, L->infoH - 18, m->status, 12, m->statusError ? 0xFF9090 : 0x90E090);
    }
}

static void draw_tiles(const Menu* m, const Layout* L) {
    ui_screen(L->touch);
    for (int i = 0; i < MENU_TILES; i++) {
        float x, y, w, h;
        tile_rect(L, i, &x, &y, &w, &h);
        int on = i == m->sel;
        if (on) ui_rect(x - 2, y - 2, w + 4, h + 4, 0xFFFFFF);
        ui_rect(x, y, w, h, on ? lighter(TILES[i].rgb) : TILES[i].rgb);
        float tw = ui_text_width(TILES[i].label, 14);
        ui_text(x + (w - tw) / 2, y + h / 2 - 12, TILES[i].label, 14, 0xFFFFFF);
        if (TILES[i].key[0]) {
            float kw = ui_text_width(TILES[i].key, 11);
            ui_text(x + (w - kw) / 2, y + h / 2 + 8, TILES[i].key, 11, 0xC8D0D8);
        }
    }
    int nr = recent_shown(m, L);
    float ry = L->y0 + RECENT_Y;
    ui_text(GAP + 2, ry - 15, nr ? "Recent" : "Recent: none yet (files opened in the editor show here)", 11, 0x8090A0);
    for (int i = 0; i < nr; i++) {
        float y = ry + i * RECENT_H;
        int on = m->sel == MENU_TILES + i;
        ui_rect(GAP, y + 1, L->w - 2 * GAP, RECENT_H - 2, on ? 0x2D4A80 : 0x1C2230);
        ui_text(GAP + 8, y + 6, short_path(m, m->recent[i]), 13, on ? 0xFFFFFF : 0xF0D080);
    }
}

void menu_draw(Menu* m) {
    if (!m) return;
    Layout L = layout();
    draw_info(m, &L);
    draw_tiles(m, &L);
}
