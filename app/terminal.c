/* MiniSharp - terminal (see terminal.h). */
#include "terminal.h"
#include "keyboard.h"
#include "repl.h"
#include "lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY 32
#define PENDING_CAP 8192
#define REPEAT_WAIT 400                 /* held B / Left / Right: repeat after this many ms, then every REPEAT_EVERY */
#define REPEAT_EVERY 70

enum { K_OUT, K_INPUT, K_ERROR };       /* kind of a scrollback line: colour */
static const uint32_t KIND_RGB[] = { 0xD8E0E8, 0x7FB2FF, 0xFF7070 };

typedef struct { char text[TERM_COLS * 4 + 1]; unsigned short len, cols; unsigned char kind; } TLine;

struct Terminal {
    Repl* repl;
    TLine lines[TERM_SCROLLBACK];
    int first, n;                       /* ring of lines: the oldest is lines[first] */
    int open;                           /* the last line has no '\n' yet (Console.Write) */
    int kind;                           /* kind of the text printed now */
    int view;                           /* lines scrolled back from the end (L + Up/Down) */
    char line[TERM_LINE_CAP];
    int len, cur, scroll;               /* scroll: first character shown in the input box */
    char hist[HISTORY][TERM_LINE_CAP], draft[TERM_LINE_CAP];
    int nhist, histPos;
    char pending[PENDING_CAP];          /* earlier lines of an input with an open '{' or '(' */
    int plen;
    char sug[TERM_NSUGGEST][REPL_WORD];
    int nsug, sugStart;
    uint32_t heldSince[3], lastRepeat[3];   /* B, Left, Right */
    const char* stopHint;               /* e.g. "START+SELECT: stop", NULL = no Stop button */
};

static Terminal* g_active;              /* receives Console output */
static uint32_t g_frameMs;              /* for the blinking cursor: the time of the last term_update */
static void console_out(const char* s) { if (g_active) term_print(g_active, s); }

/* ---------- scrollback ---------- */
static TLine* new_line(Terminal* t) {
    if (t->n == TERM_SCROLLBACK) { t->first = (t->first + 1) % TERM_SCROLLBACK; t->n--; }
    TLine* l = &t->lines[(t->first + t->n) % TERM_SCROLLBACK];
    t->n++;
    l->len = l->cols = 0; l->text[0] = 0; l->kind = (unsigned char)t->kind;
    t->open = 1;
    return l;
}

static TLine* last_line(Terminal* t) { return &t->lines[(t->first + t->n - 1) % TERM_SCROLLBACK]; }

void term_print(Terminal* t, const char* s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\n') {
            if (!t->open) new_line(t);          /* an empty line */
            t->open = 0;
            continue;
        }
        if (c == '\r') continue;
        int cont = (c & 0xC0) == 0x80;          /* UTF-8 continuation byte: belongs to the character before */
        TLine* l = t->open ? last_line(t) : new_line(t);
        if (!cont && l->cols == TERM_COLS) l = new_line(t);   /* wrap */
        if (l->len + 1 >= (int)sizeof l->text) continue;
        l->text[l->len++] = (char)c;
        l->text[l->len] = 0;
        if (!cont) l->cols++;
    }
}

static void end_output_line(Terminal* t) { if (t->open) term_print(t, "\n"); }

/* ---------- the input line ---------- */
static void refresh_suggestions(Terminal* t) {
    t->nsug = repl_complete(t->repl, t->line, t->cur, &t->sugStart, t->sug, TERM_NSUGGEST);
    if (t->nsug < 0) t->nsug = 0;
}

static void insert(Terminal* t, const char* s) {
    int n = (int)strlen(s);
    if (t->len + n >= TERM_LINE_CAP) return;
    memmove(t->line + t->cur + n, t->line + t->cur, t->len - t->cur + 1);
    memcpy(t->line + t->cur, s, n);
    t->len += n; t->cur += n;
}

static void backspace(Terminal* t) {
    if (t->cur == 0) return;
    memmove(t->line + t->cur - 1, t->line + t->cur, t->len - t->cur + 1);
    t->len--; t->cur--;
}

static void set_line(Terminal* t, const char* s) {
    snprintf(t->line, TERM_LINE_CAP, "%s", s);
    t->len = t->cur = (int)strlen(t->line);
    t->scroll = 0;
}

/* replaces the word being typed with a completion */
static void complete(Terminal* t, const char* word) {
    memmove(t->line + t->sugStart, t->line + t->cur, t->len - t->cur + 1);
    t->len -= t->cur - t->sugStart;
    t->cur = t->sugStart;
    insert(t, word);
}

static void history_add(Terminal* t, const char* s) {
    if (!*s) return;
    if (t->nhist && !strcmp(t->hist[(t->nhist - 1) % HISTORY], s)) return;
    snprintf(t->hist[t->nhist % HISTORY], TERM_LINE_CAP, "%s", s);
    t->nhist++;
}

static void history_move(Terminal* t, int dir) {
    int oldest = t->nhist > HISTORY ? t->nhist - HISTORY : 0;
    int pos = t->histPos + dir;
    if (pos < oldest || pos > t->nhist) return;
    if (t->histPos == t->nhist) snprintf(t->draft, TERM_LINE_CAP, "%s", t->line);    /* leaving the new line */
    t->histPos = pos;
    set_line(t, pos == t->nhist ? t->draft : t->hist[pos % HISTORY]);
}

static void show_result(Terminal* t, int ok, const char* err) {
    end_output_line(t);
    if (ok) return;
    t->kind = K_ERROR;
    term_print(t, "error: ");
    term_print(t, err);
    term_print(t, "\n");
    t->kind = K_OUT;
}

/* Enter: echo the line, then run it (or wait for more lines while a '{' or '(' is open); while the last input
 * waits in Console.ReadLine the line is its answer, shown after the question like in a console */
static void submit(Terminal* t) {
    char echo[TERM_LINE_CAP + 8], err[300];
    if (repl_waiting(t->repl)) {
        char answer[TERM_LINE_CAP];
        snprintf(answer, sizeof answer, "%s", t->line);
        t->kind = K_INPUT;
        term_print(t, answer);
        term_print(t, "\n");
        t->kind = K_OUT;
        t->view = 0;
        set_line(t, "");
        int ok = repl_answer(t->repl, answer, err, sizeof err);
        if (ok == REPL_WAIT) { refresh_suggestions(t); return; }
        show_result(t, ok, err);
        refresh_suggestions(t);
        return;
    }
    end_output_line(t);
    t->kind = K_INPUT;
    snprintf(echo, sizeof echo, "%s %s\n", t->plen ? "." : ">", t->line);
    term_print(t, echo);
    t->kind = K_OUT;
    t->view = 0;
    history_add(t, t->line);
    t->histPos = t->nhist;
    if (!t->plen && !t->len) { refresh_suggestions(t); return; }

    if (t->plen + t->len + 2 >= PENDING_CAP) {
        t->plen = 0;
        t->kind = K_ERROR; term_print(t, "error: input too long\n"); t->kind = K_OUT;
        set_line(t, ""); refresh_suggestions(t);
        return;
    }
    if (t->plen) t->pending[t->plen++] = '\n';
    memcpy(t->pending + t->plen, t->line, t->len + 1);
    t->plen += t->len;
    set_line(t, "");
    if (repl_incomplete(t->pending)) { refresh_suggestions(t); return; }

    t->plen = 0;
    int ok = repl_eval(t->repl, t->pending, err, sizeof err);
    if (ok != REPL_WAIT) show_result(t, ok, err);    /* waiting: the question stays open for the answer */
    refresh_suggestions(t);
}

void term_key(Terminal* t, const char* k) {
    if (!k) return;
    if (!strcmp(k, KBD_BACKSPACE)) backspace(t);
    else if (!strcmp(k, KBD_ENTER)) { submit(t); return; }
    else insert(t, k);
    refresh_suggestions(t);
}

/* ---------- layout ---------- */
/* The output gets screen 0; the input panel (hint, input line, suggestions, keyboard) the touch screen.
 * With one screen the panel takes its bottom 240 pixels. */
#define PANEL_H 240
#define OUT_TOP 18
#define OUT_LINE_H 15
#define OUT_FONT 13
#define INPUT_FONT 15

typedef struct { int outScreen, panelScreen; float outW, outH, panelY, panelW; } Layout;

static Layout layout(void) {
    Layout L = { 0, 0, 400, 240, 0, 320 };
    const UiBackend* b = ui_backend();
    if (!b) return L;
    if (b->screens >= 2) {
        L.panelScreen = b->touchScreen >= 0 ? b->touchScreen : 1;
        L.outScreen = !L.panelScreen;
        L.outW = (float)b->w[L.outScreen]; L.outH = (float)b->h[L.outScreen];
        L.panelW = (float)b->w[L.panelScreen];
        L.panelY = (float)b->h[L.panelScreen] - PANEL_H;
    } else {
        L.outW = L.panelW = (float)b->w[0];
        L.outH = (float)b->h[0] - PANEL_H;
        L.panelY = L.outH;
    }
    if (L.panelY < 0) L.panelY = 0;
    return L;
}

#define INPUT_X 18                      /* text starts after the "> " prompt */
static float input_w(const Layout* L) { return L->panelW - INPUT_X - 6; }

/* width of line[from..to) on screen */
static float width_of(const Terminal* t, int from, int to) {
    char buf[TERM_LINE_CAP];
    memcpy(buf, t->line + from, to - from); buf[to - from] = 0;
    return ui_text_width(buf, INPUT_FONT);
}

static int repeat(Terminal* t, const UiInput* in, int i, uint32_t btn) {
    if (in->down & btn) { t->heldSince[i] = t->lastRepeat[i] = in->ms; return 1; }
    if (!(in->held & btn)) return 0;
    if (in->ms - t->heldSince[i] < REPEAT_WAIT || in->ms - t->lastRepeat[i] < REPEAT_EVERY) return 0;
    t->lastRepeat[i] = in->ms;
    return 1;
}

int term_update(Terminal* t, const UiInput* in) {
    g_frameMs = in->ms;
    if (in->down & UI_BTN_START) return 0;
    Layout L = layout();
    kbd_set_area(0, L.panelY + 80, L.panelW, PANEL_H - 80);
    const char* k = kbd_update(in);
    if (k) term_key(t, k);
    if (in->touchDown) {
        float px = (float)in->x, py = (float)in->y - L.panelY;
        if (py >= 44 && py < 77) {
            int i = (int)(px / (L.panelW / TERM_NSUGGEST));
            if (i >= 0 && i < t->nsug) { complete(t, t->sug[i]); refresh_suggestions(t); }
        } else if (py >= 18 && py < 42) {           /* tap in the line moves the cursor */
            t->cur = t->scroll;
            while (t->cur < t->len && INPUT_X + width_of(t, t->scroll, t->cur + 1) < px + 3) t->cur++;
            refresh_suggestions(t);
        }
    }
    int moved = 0;
    if (in->down & UI_BTN_A) submit(t);
    if (repeat(t, in, 0, UI_BTN_B)) { backspace(t); moved = 1; }
    if (repeat(t, in, 1, UI_BTN_LEFT) && t->cur > 0) { t->cur--; moved = 1; }
    if (repeat(t, in, 2, UI_BTN_RIGHT) && t->cur < t->len) { t->cur++; moved = 1; }
    if (in->held & UI_BTN_L) {                      /* L + Up/Down: look at older output */
        if ((in->down & UI_BTN_UP) && t->view < t->n - 1) t->view++;
        if ((in->down & UI_BTN_DOWN) && t->view > 0) t->view--;
    } else {
        if (in->down & UI_BTN_UP) { history_move(t, -1); moved = 1; }
        if (in->down & UI_BTN_DOWN) { history_move(t, 1); moved = 1; }
    }
    if ((in->down & UI_BTN_R) && t->nsug) { complete(t, t->sug[0]); moved = 1; }
    if (in->down & UI_BTN_Y) { t->n = t->first = t->open = t->view = 0; }
    if (moved) refresh_suggestions(t);
    return 1;
}

/* ---------- drawing ---------- */
static void draw_output(Terminal* t, const Layout* L) {
    ui_screen(L->outScreen);
    ui_rect(0, 0, L->outW, 16, 0x1C2230);
    ui_text(6, 2, "Terminal", 12, 0x7FB2FF);
    ui_text(80, 2, "START: menu  Y: clear  L+Up/Down: scroll", 12, 0x8090A0);
    if (t->stopHint) ui_text(L->outW - ui_text_width(t->stopHint, 11) - 6, L->outH - 14, t->stopHint, 11, 0x606A78);
    int visible = (int)((L->outH - OUT_TOP) / OUT_LINE_H);
    int last = t->n - 1 - t->view;
    int from = last - visible + 1;
    if (from < 0) from = 0;
    ui_clip(0, OUT_TOP, (int)L->outW, (int)(L->outH - OUT_TOP));
    for (int i = from; i <= last; i++) {
        const TLine* l = &t->lines[(t->first + i) % TERM_SCROLLBACK];
        ui_text(6, OUT_TOP + (i - from) * OUT_LINE_H, l->text, OUT_FONT, KIND_RGB[l->kind]);
    }
    ui_unclip();
    if (t->view) ui_text(6, L->outH - 14, "(older output, L+Down)", 11, 0x8090A0);
}

static void draw_input(Terminal* t, const Layout* L, uint32_t ms) {
    float y = L->panelY;
    ui_rect(0, y + 18, L->panelW, 24, 0x1C2230);
    ui_text(5, y + 22, t->plen ? "." : ">", INPUT_FONT, 0x7FB2FF);
    float w = input_w(L);
    if (t->cur < t->scroll) t->scroll = t->cur;
    while (t->scroll < t->cur && width_of(t, t->scroll, t->cur) > w - 4) t->scroll++;
    int end = t->scroll;                            /* as much as fits after the scroll position */
    while (end < t->len && width_of(t, t->scroll, end + 1) <= w) end++;
    char buf[TERM_LINE_CAP];
    memcpy(buf, t->line + t->scroll, end - t->scroll); buf[end - t->scroll] = 0;
    ui_text(INPUT_X, y + 22, buf, INPUT_FONT, 0xFFFFFF);
    if ((ms / 500) % 2 == 0) ui_rect(INPUT_X + width_of(t, t->scroll, t->cur), y + 22, 1.5f, 16, 0xFFFFFF);
}

static void draw_suggestions(Terminal* t, const Layout* L) {
    float bw = L->panelW / TERM_NSUGGEST, y = L->panelY + 45;
    for (int i = 0; i < TERM_NSUGGEST; i++) {
        float x = i * bw;
        ui_rect(x + 1, y, bw - 2, 31, i < t->nsug ? 0x24324A : 0x15181E);
        if (i >= t->nsug) continue;
        float size = 15, tw = ui_text_width(t->sug[i], size);
        if (tw > bw - 6) { size = size * (bw - 6) / tw; tw = bw - 6; }
        ui_text(x + (bw - tw) / 2, y + (31 - size) / 2, t->sug[i], size, i == 0 ? 0xFFFFFF : 0xC0D0E8);
    }
}


void term_draw(Terminal* t) {
    Layout L = layout();
    draw_output(t, &L);
    ui_screen(L.panelScreen);
    ui_text(4, L.panelY + 3, "A: run  B: del  Up/Down: history  R: complete", 11, 0x8090A0);
    draw_input(t, &L, g_frameMs);
    draw_suggestions(t, &L);
    kbd_set_area(0, L.panelY + 80, L.panelW, PANEL_H - 80);
    kbd_draw();
}

/* ---------- life ---------- */
Terminal* term_new(void) {
    Terminal* t = (Terminal*)calloc(1, sizeof *t);
    if (!t) return NULL;
    t->repl = repl_new();
    if (!t->repl) { free(t); return NULL; }
    g_active = t;
    lib_set_output(console_out);
    kbd_reset();
    kbd_set_enter_label("Run");
    term_print(t, "MiniSharp terminal. Try:\n  1 + 2\n  var x = 5\n  for (int i = 0; i < x; i++) Console.Write(i);\n");
    refresh_suggestions(t);
    return t;
}

void term_set_stop_hint(Terminal* t, const char* hint) {
    t->stopHint = hint;
    repl_set_time_limit(t->repl, hint ? 0 : 4);
}

void term_free(Terminal* t) {
    if (!t) return;
    if (g_active == t) { g_active = NULL; lib_set_output(NULL); }
    repl_free(t->repl);
    free(t);
}

/* ---------- for tests ---------- */
int term_lines(const Terminal* t) { return t->n; }
const char* term_line(const Terminal* t, int i) {
    return i >= 0 && i < t->n ? t->lines[(t->first + i) % TERM_SCROLLBACK].text : NULL;
}
const char* term_input(const Terminal* t) { return t->line; }
int term_cursor(const Terminal* t) { return t->cur; }
int term_continuing(const Terminal* t) { return t->plen > 0; }
int term_suggestions(const Terminal* t, const char** out) {
    for (int i = 0; i < t->nsug; i++) out[i] = t->sug[i];
    return t->nsug;
}
