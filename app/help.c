/* MiniSharp - Help (see help.h): reads the documentation chapters and lays them out in lines that fit the screen.
 * The format is the one of sdk/docs/README.md: "# " title, "## " section, paragraphs, "- " lists, "> " notes for
 * advanced readers, fenced blocks cs / wynik / wejscie / text; **bold** and `code` marks are dropped from the text. */
#include "help.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHAPTERS 64
#define FONT 13
#define LINE_H 16
#define TEXT_Y 20                       /* the chapter text starts below the title bar */
#define MARGIN 6
#define ROW_H 24
#define TOOLS_H 22
#define REPEAT_WAIT 350
#define REPEAT_EVERY 50

typedef struct { char* s; int kind; } Line;

struct Help {
    char* text[MAX_CHAPTERS];
    char title[MAX_CHAPTERS][64];
    int n;
    int sel, listTop;                   /* the list */
    int open;                           /* -1 = the list */
    Line* lines; int nlines, cap;
    int top;
    float scrollX;
    float width;                        /* the width the open chapter was laid out for */
    uint32_t ms, since[4], last[4];
};

/* ---------- layout of the screens ---------- */
typedef struct { int textScreen, listScreen; float textW, textH, listY, listW, listH; } Layout;

static Layout layout(void) {
    Layout L = { 0, 1, 400, 240, 0, 320, 240 };
    const UiBackend* b = ui_backend();
    if (!b) return L;
    if (b->screens >= 2) {
        L.listScreen = b->touchScreen >= 0 ? b->touchScreen : 1;
        L.textScreen = !L.listScreen;
        L.textW = (float)b->w[L.textScreen]; L.textH = (float)b->h[L.textScreen];
        L.listW = (float)b->w[L.listScreen]; L.listH = (float)b->h[L.listScreen];
        L.listY = 0;
    } else {                            /* one screen: the buttons in a row at the bottom, the text above */
        L.textScreen = L.listScreen = 0;
        L.textW = L.listW = (float)b->w[0];
        L.textH = (float)b->h[0] - TOOLS_H;
        L.listY = L.textH;
        L.listH = TOOLS_H;
    }
    return L;
}

static int text_rows(const Layout* L) { int r = (int)((L->textH - TEXT_Y) / LINE_H); return r < 1 ? 1 : r; }
static int list_rows(void) {
    const UiBackend* b = ui_backend();
    float h = b && b->screens >= 2 ? (float)b->h[b->touchScreen >= 0 ? b->touchScreen : 1] : b ? (float)b->h[0] : 240;
    int r = (int)((h - 20) / ROW_H);
    return r < 1 ? 1 : r;
}

/* ---------- life ---------- */
Help* help_new(void) {
    Help* h = (Help*)calloc(1, sizeof(Help));
    if (h) h->open = -1;
    return h;
}

static void clear_lines(Help* h) {
    for (int i = 0; i < h->nlines; i++) free(h->lines[i].s);
    h->nlines = 0;
}

void help_free(Help* h) {
    if (!h) return;
    clear_lines(h);
    free(h->lines);
    for (int i = 0; i < h->n; i++) free(h->text[i]);
    free(h);
}

int help_add(Help* h, const char* text) {
    if (h->n >= MAX_CHAPTERS || !text) return 0;
    size_t len = strlen(text);
    char* t = (char*)malloc(len + 1);
    if (!t) return 0;
    memcpy(t, text, len + 1);
    h->text[h->n] = t;
    const char* p = !strncmp(t, "# ", 2) ? t + 2 : strstr(t, "\n# ");
    if (p && p != t + 2) p += 3;
    int n = 0;
    if (p) while (p[n] && p[n] != '\n' && p[n] != '\r' && n < 63) n++;
    if (n) { memcpy(h->title[h->n], p, n); h->title[h->n][n] = 0; }
    else snprintf(h->title[h->n], sizeof h->title[0], "Chapter %d", h->n + 1);
    h->n++;
    return 1;
}

static int is_chapter(const char* name) {
    size_t n = strlen(name);
    return name[0] >= '0' && name[0] <= '9' && n > 3 && !strcmp(name + n - 3, ".md");
}

/* the chapter files of the folders: one entry per file name, from the first folder that has it */
typedef struct { char name[64]; int dir; } Found;
static int cmp_found(const void* a, const void* b) { return strcmp(((const Found*)a)->name, ((const Found*)b)->name); }

int help_load_dirs(Help* h, const char* const* dirs, int ndirs) {
    static Found found[MAX_CHAPTERS];
    int n = 0;
    for (int di = 0; di < ndirs; di++) {
        DIR* d = opendir(dirs[di]);
        if (!d) continue;
        struct dirent* e;
        while ((e = readdir(d))) {
            if (!is_chapter(e->d_name) || strlen(e->d_name) >= sizeof found[0].name) continue;
            int dup = 0;
            for (int i = 0; i < n && !dup; i++) dup = !strcmp(found[i].name, e->d_name);
            if (dup || n >= MAX_CHAPTERS) continue;          /* an earlier folder has it already: that one wins */
            snprintf(found[n].name, sizeof found[n].name, "%s", e->d_name);
            found[n++].dir = di;
        }
        closedir(d);
    }
    qsort(found, n, sizeof found[0], cmp_found);
    int added = 0;
    for (int i = 0; i < n; i++) {
        char path[512];
        if (snprintf(path, sizeof path, "%s/%s", dirs[found[i].dir], found[i].name) >= (int)sizeof path) continue;
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
        char* s = len >= 0 ? (char*)malloc(len + 1) : NULL;
        if (s && fread(s, 1, len, f) == (size_t)len) { s[len] = 0; added += help_add(h, s); }
        free(s);
        fclose(f);
    }
    return added;
}

int help_load_dir(Help* h, const char* dir) { return help_load_dirs(h, &dir, 1); }

int help_chapters(const Help* h) { return h->n; }
const char* help_title(const Help* h, int i) { return i >= 0 && i < h->n ? h->title[i] : ""; }
int help_reading(const Help* h) { return h->open; }
int help_lines(const Help* h) { return h->nlines; }
int help_top(const Help* h) { return h->top; }
const char* help_line(const Help* h, int i, int* kind) {
    if (i < 0 || i >= h->nlines) { if (kind) *kind = HL_BLANK; return ""; }
    if (kind) *kind = h->lines[i].kind;
    return h->lines[i].s;
}

/* ---------- laying a chapter out ---------- */
static void add_line(Help* h, const char* s, int len, int kind) {
    if (h->nlines == h->cap) {
        int nc = h->cap ? h->cap * 2 : 128;
        Line* l = (Line*)realloc(h->lines, sizeof(Line) * nc);
        if (!l) return;
        h->lines = l; h->cap = nc;
    }
    char* c = (char*)malloc(len + 1);
    if (!c) return;
    memcpy(c, s, len); c[len] = 0;
    h->lines[h->nlines].s = c;
    h->lines[h->nlines].kind = kind;
    h->nlines++;
}

/* drops the ** and ` marks of the inline format */
static void plain(const char* s, int len, char* out, int cap) {
    int o = 0;
    for (int i = 0; i < len && o < cap - 1; i++) {
        if (s[i] == '`') continue;
        if (s[i] == '*' && i + 1 < len && s[i + 1] == '*') { i++; continue; }
        out[o++] = s[i];
    }
    out[o] = 0;
}

/* a paragraph in lines no wider than w; 'indent' pixels for the lines after the first (list items) */
static void wrap(Help* h, const char* text, int kind, float w, const char* firstPrefix) {
    char line[600], word[300];
    float indent = firstPrefix ? ui_text_width(firstPrefix, FONT) : 0;
    snprintf(line, sizeof line, "%s", firstPrefix ? firstPrefix : "");
    int hasWord = 0;
    const char* p = text;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        int n = 0;
        while (p[n] && p[n] != ' ' && n < (int)sizeof word - 1) n++;
        memcpy(word, p, n); word[n] = 0;
        p += n;
        char tryLine[900];
        snprintf(tryLine, sizeof tryLine, "%s%s%s", line, hasWord ? " " : "", word);
        if (hasWord && ui_text_width(tryLine, FONT) > w) {
            add_line(h, line, (int)strlen(line), kind);
            /* continuation lines of a list item start a little to the right, under its text */
            snprintf(line, sizeof line, "%s%s", indent > 0 ? "  " : "", word);
        } else {
            size_t tl = strlen(tryLine);
            if (tl >= sizeof line) tl = sizeof line - 1;     /* a very long line is cut, not overflowed */
            memcpy(line, tryLine, tl); line[tl] = 0;
        }
        hasWord = 1;
    }
    if (hasWord || firstPrefix) add_line(h, line, (int)strlen(line), kind);
}

/* the paragraph collected so far, wrapped and emptied */
static void flush(Help* h, char* para, int* plen, int kind, float w) {
    if (!*plen) return;
    para[*plen] = 0;
    char clean[4096];
    plain(para, *plen, clean, sizeof clean);
    if (kind == HL_BULLET) wrap(h, clean, kind, w, "- ");
    else wrap(h, clean, kind, w, NULL);
    *plen = 0;
}

static void lay_out(Help* h, const char* t, float w) {
    clear_lines(h);
    char para[4096];
    int plen = 0, pkind = HL_TEXT, inBlock = 0, blockKind = HL_CODE;
    const char* p = t;
    while (*p) {
        const char* e = p;
        while (*e && *e != '\n') e++;
        int len = (int)(e - p);
        if (len && p[len - 1] == '\r') len--;
        if (len >= 3 && !strncmp(p, "```", 3)) {
            flush(h, para, &plen, pkind, w);
            if (inBlock) { inBlock = 0; add_line(h, "", 0, HL_BLANK); }
            else {
                inBlock = 1;
                if (len >= 8 && !strncmp(p + 3, "wynik", 5)) { blockKind = HL_RESULT; add_line(h, "Program wypisze:", 16, HL_TEXT); }
                else if (len >= 10 && !strncmp(p + 3, "wejscie", 7)) { blockKind = HL_RESULT; add_line(h, "Użytkownik wpisuje:", 20, HL_TEXT); }
                else blockKind = HL_CODE;
            }
        } else if (inBlock) {
            add_line(h, p, len, blockKind);
        } else if (len == 0) {
            flush(h, para, &plen, pkind, w);
            if (h->nlines && h->lines[h->nlines - 1].kind != HL_BLANK) add_line(h, "", 0, HL_BLANK);
        } else if (len >= 2 && !strncmp(p, "# ", 2)) {
            flush(h, para, &plen, pkind, w);
            char c[300]; plain(p + 2, len - 2, c, sizeof c);
            add_line(h, c, (int)strlen(c), HL_TITLE);
        } else if (len >= 3 && !strncmp(p, "## ", 3)) {
            flush(h, para, &plen, pkind, w);
            char c[300]; plain(p + 3, len - 3, c, sizeof c);
            if (h->nlines && h->lines[h->nlines - 1].kind != HL_BLANK) add_line(h, "", 0, HL_BLANK);
            add_line(h, c, (int)strlen(c), HL_HEADING);
        } else {
            int kind = HL_TEXT, skip = 0;
            if (len >= 2 && !strncmp(p, "- ", 2)) { kind = HL_BULLET; skip = 2; }
            else if (p[0] == '>') { kind = HL_NOTE; skip = len >= 2 && p[1] == ' ' ? 2 : 1; }
            int cont = kind == HL_TEXT && plen && (pkind == HL_BULLET || pkind == HL_NOTE) && p[0] == ' ';
            if (kind == HL_NOTE && plen && pkind == HL_NOTE) cont = 1;
            if (!cont && (kind != HL_TEXT || pkind != HL_TEXT)) { flush(h, para, &plen, pkind, w); pkind = kind; }
            if (!cont && kind == HL_TEXT) pkind = HL_TEXT;
            if (plen && plen < (int)sizeof para - 2) para[plen++] = ' ';
            for (int i = skip; i < len && plen < (int)sizeof para - 1; i++) para[plen++] = p[i];
        }
        p = *e ? e + 1 : e;
    }
    flush(h, para, &plen, pkind, w);
}

int help_open(Help* h, int i) {
    if (i < 0 || i >= h->n) return 0;
    Layout L = layout();
    h->open = i;
    h->top = 0; h->scrollX = 0;
    h->width = L.textW - 2 * MARGIN;
    lay_out(h, h->text[i], h->width);
    return 1;
}

/* ---------- input ---------- */
static int repeat(Help* h, int k, const UiInput* in, uint32_t btn) {
    if (in->down & btn) { h->since[k] = h->last[k] = in->ms; return 1; }
    if (!(in->held & btn)) return 0;
    if (in->ms - h->since[k] < REPEAT_WAIT || in->ms - h->last[k] < REPEAT_EVERY) return 0;
    h->last[k] = in->ms;
    return 1;
}

static void clamp_top(Help* h, const Layout* L) {
    int maxTop = h->nlines - text_rows(L);
    if (h->top > maxTop) h->top = maxTop;
    if (h->top < 0) h->top = 0;
}

enum { RT_BACK, RT_PREV, RT_NEXT, NRT };
static const char* RT_NAMES[NRT] = { "Back", "Prev", "Next" };

int help_update(Help* h, const UiInput* in) {
    h->ms = in->ms;
    Layout L = layout();
    if (in->down & UI_BTN_START) { h->open = -1; return 0; }
    if (h->open < 0) {                                   /* the list of chapters */
        if (in->down & UI_BTN_B) return 0;
        int vis = list_rows();
        if (repeat(h, 0, in, UI_BTN_UP) && h->sel > 0) h->sel--;
        if (repeat(h, 1, in, UI_BTN_DOWN) && h->sel < h->n - 1) h->sel++;
        if (in->down & UI_BTN_L) h->sel = h->sel > vis ? h->sel - vis : 0;
        if (in->down & UI_BTN_R) h->sel = h->sel + vis < h->n ? h->sel + vis : (h->n ? h->n - 1 : 0);
        if (in->down & UI_BTN_A) help_open(h, h->sel);
        else if (in->touchDown) {
            float py = (float)in->y - 20;
            if (py >= 0) {
                int i = h->listTop + (int)(py / ROW_H);
                if (i < h->n) { if (i == h->sel) help_open(h, i); else h->sel = i; }
            }
        }
        if (h->sel < h->listTop) h->listTop = h->sel;
        if (h->sel >= h->listTop + vis) h->listTop = h->sel - vis + 1;
        return 1;
    }
    /* the reader */
    if (L.textW - 2 * MARGIN != h->width) help_open(h, h->open);   /* the screen changed: lay out again */
    int rows = text_rows(&L);
    if (in->down & UI_BTN_B) { h->open = -1; return 1; }
    if (repeat(h, 0, in, UI_BTN_UP)) h->top--;
    if (repeat(h, 1, in, UI_BTN_DOWN)) h->top++;
    if (repeat(h, 2, in, UI_BTN_LEFT)) h->scrollX = h->scrollX > 40 ? h->scrollX - 40 : 0;
    if (repeat(h, 3, in, UI_BTN_RIGHT)) h->scrollX += 40;
    if (in->down & UI_BTN_L) h->top -= rows - 1;
    if (in->down & UI_BTN_R) h->top += rows - 1;
    if (in->touchDown) {
        float py = (float)in->y - L.listY;
        if (py >= 0 && py < TOOLS_H) {
            int t = (int)(in->x / (L.listW / NRT));
            if (t == RT_BACK) { h->open = -1; return 1; }
            if (t == RT_PREV && h->open > 0) { h->sel = h->open - 1; help_open(h, h->sel); }
            else if (t == RT_NEXT && h->open < h->n - 1) { h->sel = h->open + 1; help_open(h, h->sel); }
        }
    }
    clamp_top(h, &L);
    return 1;
}

/* ---------- drawing ---------- */
static const uint32_t KIND_RGB[] = {
    0, 0xFFFFFF, 0x7FB2FF, 0xD4D4D4, 0xD4D4D4, 0xCE9178, 0xB5CEA8, 0xE0C080,   /* HL_* */
};

static void button(float x, float y, float w, float h, const char* s, uint32_t bg) {
    ui_rect(x + 1, y + 1, w - 2, h - 2, bg);
    float tw = ui_text_width(s, 12);
    ui_text(x + (w - tw) / 2, y + (h - 12) / 2, s, 12, 0xFFFFFF);
}

static void draw_reader(Help* h, const Layout* L) {
    ui_screen(L->textScreen);
    ui_rect(0, 0, L->textW, 16, 0x1C2230);
    ui_text(6, 2, h->title[h->open], 12, 0x7FB2FF);
    char pos[32];
    snprintf(pos, sizeof pos, "%d/%d", h->top + 1, h->nlines);
    ui_text(L->textW - ui_text_width(pos, 12) - 6, 2, pos, 12, 0x8090A0);
    int rows = text_rows(L);
    for (int r = 0; r < rows && h->top + r < h->nlines; r++) {
        Line* l = &h->lines[h->top + r];
        float y = TEXT_Y + r * LINE_H;
        if (l->kind == HL_CODE || l->kind == HL_RESULT) {
            ui_rect(MARGIN - 2, y, L->textW - 2 * MARGIN + 4, LINE_H, l->kind == HL_CODE ? 0x1A1E26 : 0x182018);
            ui_clip(MARGIN, (int)y, (int)(L->textW - 2 * MARGIN), LINE_H);
            ui_text(MARGIN + 2 - h->scrollX, y + 1, l->s, FONT, KIND_RGB[l->kind]);
            ui_unclip();
        } else if (l->kind == HL_NOTE) {
            ui_rect(MARGIN - 2, y, 2, LINE_H, 0xE0C080);
            ui_text(MARGIN + 4, y + 1, l->s, FONT, KIND_RGB[l->kind]);
        } else if (l->kind != HL_BLANK) {
            float size = l->kind == HL_TITLE ? 15 : FONT;
            ui_text(MARGIN, y + 1, l->s, size, KIND_RGB[l->kind]);
        }
    }
    /* the buttons on the touch screen */
    ui_screen(L->listScreen);
    float bw = L->listW / NRT;
    for (int i = 0; i < NRT; i++) {
        int off = (i == RT_PREV && h->open == 0) || (i == RT_NEXT && h->open == h->n - 1);
        button(i * bw, L->listY, bw, TOOLS_H, RT_NAMES[i], off ? 0x22262E : 0x34405A);
    }
    if (L->listScreen != L->textScreen) {
        float y = L->listY + TOOLS_H + 10;
        ui_text(8, y, h->title[h->open], 14, 0xFFFFFF);
        const char* HINTS[] = { "Up / Down: scroll    L / R: page", "Left / Right: long code lines", "B: chapters    START: leave Help" };
        for (int i = 0; i < 3; i++) ui_text(8, y + 26 + i * 16, HINTS[i], 12, 0xA0A8B0);
    }
}

static void draw_list(Help* h, const Layout* L) {
    if (L->listScreen != L->textScreen) {
        ui_screen(L->textScreen);
        ui_rect(0, 0, L->textW, 16, 0x1C2230);
        ui_text(6, 2, "Help: the MiniSharp language", 12, 0x7FB2FF);
        const char* HINTS[] = { "Chapters for beginners, with examples that really run.",
                                "", "A / tap twice: open    D-pad: choose", "L / R: page    B: leave" };
        for (int i = 0; i < 4; i++) ui_text(8, 26 + i * 16, HINTS[i], 12, 0xA0A8B0);
        if (!h->n) ui_text(8, 26 + 5 * 16, "no chapters found (the docs folder is missing)", 12, 0xFF7070);
    }
    ui_screen(L->listScreen);
    float w = L->listW, top = 0;
    ui_rect(0, top, w, 18, 0x1C2230);
    ui_text(6, top + 3, "Chapters", 12, 0x7FB2FF);
    int vis = list_rows();
    for (int r = 0; r < vis && h->listTop + r < h->n; r++) {
        int i = h->listTop + r;
        float y = top + 20 + r * ROW_H;
        if (i == h->sel) ui_rect(0, y, w, ROW_H, 0x264F78);
        char row[96];
        snprintf(row, sizeof row, "%d. %s", i + 1, h->title[i]);
        ui_text(8, y + 5, row, 13, 0xFFFFFF);
    }
}

void help_draw(Help* h) {
    Layout L = layout();
    if (h->open >= 0) draw_reader(h, &L);
    else draw_list(h, &L);
}
