/* MiniSharp - code editor (see editor.h). Moved in parts from the old source3ds/editor.c: the screen layout, the
 * colours, the buttons and run-then-jump-to-the-error; the buffer is app/textbuf.c and the language knowledge
 * is lang/service.c now. One file is edited; the other files of its project (ed_set_project) are read from the
 * card so that suggestions and signatures know their classes (tabs come next). */
#include "editor.h"
#include "keyboard.h"
#include "service.h"
#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODE_Y 18                       /* code starts below the title bar */
#define LINE_H 15
#define FONT 13
#define GUTTER 34                       /* line numbers */
#define PANEL_H 240
#define NSUG 4
#define REPEAT_WAIT 400
#define REPEAT_EVERY 60

static const uint32_t KIND_RGB[] = { 0xD4D4D4, 0x569CD6, 0x4EC9B0, 0xCE9178, 0xB5CEA8, 0x6A9955 };   /* SVC_* */

enum { TOOL_SAVE, TOOL_RUN, TOOL_UNDO, TOOL_REDO, TOOL_HELP, NTOOLS };
static const char* TOOL_NAMES[NTOOLS] = { "Save", "Run", "Undo", "Redo", "Help" };
/* in the selection mode (X) the same buttons are the clipboard */
enum { SEL_COPY, SEL_CUT, SEL_PASTE, SEL_ALL, SEL_DONE };
static const char* SEL_NAMES[NTOOLS] = { "Copy", "Cut", "Paste", "All", "Done" };

/* one clipboard for all files the editor opens (copy in one file, paste in another) */
static char* clipboard;

struct Editor {
    EditorHost host;
    TextBuf* tb;
    MsService* svc;
    char path[256];
    int top;                            /* first line on screen */
    float scrollX;
    int errLine;
    char status[300];
    uint32_t statusRgb;
    int leaveArmed;                     /* START pressed once with unsaved changes */
    char sug[NSUG][SVC_WORD];
    int nsug, sugStart;
    char sig[128];
    int sigArg;
    int svcLen; unsigned svcSum;        /* the text the service has (to give it a new one only after changes) */
    char** others; int nothers;         /* the other files of the project (read-only, for the service) */
    uint32_t ms;
    uint32_t heldSince[4], lastRepeat[4];   /* Up, Down, Left, Right, then B */
    uint32_t bSince, bLast;
    int selMode;                        /* X: the D-pad grows the selection, the tools are Copy / Cut / Paste / All */
};

/* ---------- layout ---------- */
typedef struct { int codeScreen, panelScreen; float codeW, codeH, panelY, panelW; } Layout;

static Layout layout(void) {
    Layout L = { 0, 1, 400, 240, 0, 320 };
    const UiBackend* b = ui_backend();
    if (!b) return L;
    if (b->screens >= 2) {
        L.panelScreen = b->touchScreen >= 0 ? b->touchScreen : 1;
        L.codeScreen = !L.panelScreen;
        L.codeW = (float)b->w[L.codeScreen]; L.codeH = (float)b->h[L.codeScreen];
        L.panelW = (float)b->w[L.panelScreen];
        L.panelY = (float)b->h[L.panelScreen] - PANEL_H;
    } else {
        L.codeScreen = L.panelScreen = 0;
        L.codeW = L.panelW = (float)b->w[0];
        L.codeH = (float)b->h[0] - PANEL_H;
        L.panelY = L.codeH;
    }
    if (L.panelY < 0) L.panelY = 0;
    return L;
}

int ed_visible_lines(void) { Layout L = layout(); return (int)((L.codeH - CODE_Y) / LINE_H); }

/* ---------- status ---------- */
static void set_status(Editor* ed, uint32_t rgb, const char* s) {
    snprintf(ed->status, sizeof ed->status, "%s", s);
    ed->statusRgb = rgb;
}

/* ---------- life ---------- */
Editor* ed_new(const EditorHost* host) {
    Editor* ed = (Editor*)calloc(1, sizeof(Editor));
    if (!ed) return NULL;
    if (host) ed->host = *host;
    ed->tb = tb_new(NULL);
    ed->svc = svc_new();
    if (!ed->tb || !ed->svc) { ed_free(ed); return NULL; }
    ed->svcLen = -1;
    kbd_reset();
    kbd_set_enter_label("Enter");
    return ed;
}

static void free_others(Editor* ed) {
    for (int i = 0; i < ed->nothers; i++) free(ed->others[i]);
    free(ed->others);
    ed->others = NULL; ed->nothers = 0;
    ed->svcLen = -1;
}

void ed_free(Editor* ed) {
    if (!ed) return;
    free_others(ed);
    tb_free(ed->tb);
    svc_free(ed->svc);
    free(ed);
}

TextBuf* ed_buf(Editor* ed) { return ed->tb; }
const char* ed_status(const Editor* ed) { return ed->status; }
int ed_top_line(const Editor* ed) { return ed->top; }
int ed_error_line(const Editor* ed) { return ed->errLine; }

static const char* base_name(const char* p) {
    const char* s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* the whole file, 0-terminated (free it); NULL = cannot open (*missing = 1) or read */
static char* read_file(const char* path, int* missing) {
    FILE* f = fopen(path, "rb");
    if (missing) *missing = !f;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* s = n >= 0 ? (char*)malloc(n + 1) : NULL;
    if (!s || fread(s, 1, n, f) != (size_t)n) { fclose(f); free(s); return NULL; }
    s[n] = 0;
    fclose(f);
    return s;
}

int ed_open(Editor* ed, const char* path) {
    snprintf(ed->path, sizeof ed->path, "%s", path);
    ed->top = 0; ed->scrollX = 0; ed->errLine = 0; ed->leaveArmed = 0; ed->svcLen = -1; ed->selMode = 0;
    free_others(ed);
    int missing;
    char* s = read_file(path, &missing);
    if (!s && missing) { tb_set_text(ed->tb, ""); set_status(ed, 0xF0D080, "new file"); return 0; }
    if (!s) { tb_set_text(ed->tb, ""); set_status(ed, 0xFF7070, "cannot read the file"); return 0; }
    tb_set_text(ed->tb, s);
    free(s);
    set_status(ed, 0x8090A0, base_name(path));
    return 1;
}

int ed_set_project(Editor* ed, const char* dir) {
    free_others(ed);
    if (!dir) return 0;
    Project pr;
    char err[200];
    if (!project_load(dir, &pr, err, sizeof err)) return 0;
    ed->others = (char**)calloc(pr.nfiles > 0 ? pr.nfiles : 1, sizeof(char*));
    if (!ed->others) { project_free(&pr); return 0; }
    for (int i = 0; i < pr.nfiles; i++) {
        if (!strcmp(pr.files[i].path, ed->path)) continue;     /* the edited file comes from the buffer */
        char* t = read_file(pr.files[i].path, NULL);
        if (t) ed->others[ed->nothers++] = t;
    }
    project_free(&pr);
    return ed->nothers;
}

int ed_project_files(const Editor* ed) { return ed->nothers; }

int ed_save(Editor* ed) {
    FILE* f = fopen(ed->path, "wb");                   /* written as it is: UTF-8 without BOM, '\n' */
    if (!f || fwrite(tb_text(ed->tb), 1, tb_len(ed->tb), f) != (size_t)tb_len(ed->tb)) {
        if (f) fclose(f);
        set_status(ed, 0xFF7070, "cannot save the file");
        return 0;
    }
    fclose(f);
    tb_set_saved(ed->tb);
    set_status(ed, 0x80E080, "saved");
    return 1;
}

/* ---------- the language service ---------- */
static unsigned checksum(const char* s, int n) {
    unsigned h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}

static void refresh(Editor* ed) {
    const char* t = tb_text(ed->tb);
    int n = tb_len(ed->tb);
    unsigned sum = checksum(t, n);
    if (ed->svcLen < 0) {                               /* new file or project: the buffer is text 0, then the others */
        const char** all = (const char**)malloc(sizeof(char*) * (ed->nothers + 1));
        if (all) {
            all[0] = t;
            for (int i = 0; i < ed->nothers; i++) all[i + 1] = ed->others[i];
            svc_set_texts(ed->svc, all, ed->nothers + 1);
            free(all);
        } else svc_set_texts(ed->svc, &t, 1);
        ed->svcLen = n; ed->svcSum = sum;
    } else if (n != ed->svcLen || sum != ed->svcSum) {  /* the index is rebuilt only after a change */
        svc_set_text(ed->svc, 0, t);
        ed->svcLen = n; ed->svcSum = sum;
    }
    int cur = tb_cursor(ed->tb);
    ed->nsug = svc_complete(ed->svc, 0, cur, &ed->sugStart, ed->sug, NSUG);
    if (!svc_signature(ed->svc, 0, cur, ed->sig, sizeof ed->sig, &ed->sigArg)) ed->sig[0] = 0;
}

static void complete(Editor* ed, int i) {
    if (i < 0 || i >= ed->nsug) return;
    tb_set_cursor(ed->tb, ed->sugStart, 1);            /* select the typed part and type the whole word over it */
    tb_insert(ed->tb, ed->sug[i]);
}

/* ---------- editing ---------- */
static void edited(Editor* ed) { ed->errLine = 0; ed->leaveArmed = 0; }

void ed_key(Editor* ed, const char* k) {
    if (!k) return;
    if (!strcmp(k, KBD_BACKSPACE)) tb_backspace(ed->tb);
    else if (!strcmp(k, KBD_ENTER)) tb_newline(ed->tb);
    else tb_insert(ed->tb, k);
    edited(ed);
}

/* "compile error: line 3: ..." / "runtime error: line 7 (P.Main): ..." -> 3 / 7 */
static int error_line(const char* err) {
    const char* p = strstr(err, "line ");
    return p ? atoi(p + 5) : 0;
}

static void run(Editor* ed) {
    if (!ed->host.run) { set_status(ed, 0xFF7070, "this host cannot run programs"); return; }
    if (!ed_save(ed)) return;
    char err[300] = "";
    if (ed->host.run(ed->path, err, sizeof err)) { set_status(ed, 0x80E080, "ran without errors"); ed->errLine = 0; return; }
    set_status(ed, 0xFF7070, err);
    ed->errLine = error_line(err);
    if (ed->errLine > 0) tb_set_cursor(ed->tb, tb_line_start(ed->tb, ed->errLine - 1), 0);
}

/* ---------- selection and clipboard ---------- */
static void set_sel_mode(Editor* ed, int on) {
    ed->selMode = on;
    if (!on && tb_has_selection(ed->tb)) tb_set_cursor(ed->tb, tb_cursor(ed->tb), 0);
    set_status(ed, 0x8090A0, on ? "select: D-pad, then Copy / Cut / Paste (X: done)" : base_name(ed->path));
}

static void sel_tool(Editor* ed, int t) {
    switch (t) {
    case SEL_COPY: case SEL_CUT: {
        if (!tb_has_selection(ed->tb)) { set_status(ed, 0xF0D080, "nothing selected: move with the D-pad first"); return; }
        char* s = tb_copy(ed->tb);
        if (!s) return;
        free(clipboard);
        clipboard = s;
        if (t == SEL_CUT) { tb_backspace(ed->tb); edited(ed); }
        set_sel_mode(ed, 0);
        set_status(ed, 0x80E080, t == SEL_CUT ? "cut" : "copied");
        break;
    }
    case SEL_PASTE:
        if (!clipboard || !clipboard[0]) { set_status(ed, 0xF0D080, "the clipboard is empty"); return; }
        tb_insert(ed->tb, clipboard);
        edited(ed);
        set_sel_mode(ed, 0);
        set_status(ed, 0x80E080, "pasted");
        break;
    case SEL_ALL: tb_select_all(ed->tb); break;
    case SEL_DONE: set_sel_mode(ed, 0); break;
    }
}

const char* ed_clipboard(void) { return clipboard ? clipboard : ""; }
int ed_sel_mode(const Editor* ed) { return ed->selMode; }

static void tool(Editor* ed, int t) {
    if (ed->selMode) { sel_tool(ed, t); return; }
    switch (t) {
    case TOOL_SAVE: ed_save(ed); break;
    case TOOL_RUN: run(ed); break;
    case TOOL_UNDO: if (tb_undo(ed->tb)) edited(ed); break;
    case TOOL_REDO: if (tb_redo(ed->tb)) edited(ed); break;
    case TOOL_HELP:
        if (ed->host.help) ed->host.help();
        else set_status(ed, 0xF0D080, "no Help in this host");
        break;
    }
}

static int repeat(uint32_t* since, uint32_t* last, const UiInput* in, uint32_t btn) {
    if (in->down & btn) { *since = *last = in->ms; return 1; }
    if (!(in->held & btn)) return 0;
    if (in->ms - *since < REPEAT_WAIT || in->ms - *last < REPEAT_EVERY) return 0;
    *last = in->ms;
    return 1;
}

/* keeps the cursor on screen: vertically by lines, horizontally by pixels */
static void scroll_to_cursor(Editor* ed, const Layout* L) {
    int vis = (int)((L->codeH - CODE_Y) / LINE_H);
    if (vis < 1) vis = 1;
    int line = tb_line_of(ed->tb, tb_cursor(ed->tb));
    if (line < ed->top) ed->top = line;
    if (line >= ed->top + vis) ed->top = line - vis + 1;
    int ls = tb_line_start(ed->tb, line), cur = tb_cursor(ed->tb);
    char buf[512];
    int n = cur - ls < (int)sizeof buf - 1 ? cur - ls : (int)sizeof buf - 1;
    memcpy(buf, tb_text(ed->tb) + ls, n); buf[n] = 0;
    float x = ui_text_width(buf, FONT), w = L->codeW - GUTTER - 8;
    if (x - ed->scrollX > w) ed->scrollX = x - w + 40;
    if (x < ed->scrollX) ed->scrollX = x > 40 ? x - 40 : 0;
}

int ed_update(Editor* ed, const UiInput* in) {
    ed->ms = in->ms;
    Layout L = layout();
    if (in->down & UI_BTN_START) {
        if (!tb_modified(ed->tb) || ed->leaveArmed) return 0;
        ed->leaveArmed = 1;
        set_status(ed, 0xF0D080, "unsaved changes: SELECT saves, START again leaves without saving");
        return 1;
    }
    kbd_set_area(0, L.panelY + 80, L.panelW, PANEL_H - 80);
    const char* k = kbd_update(in);
    if (k) ed_key(ed, k);
    if (in->touchDown) {
        float px = (float)in->x, py = (float)in->y - L.panelY;
        if (py >= 0 && py < 22) tool(ed, (int)(px / (L.panelW / NTOOLS)));
        else if (py >= 46 && py < 77) complete(ed, (int)(px / (L.panelW / NSUG)));
    }
    int sel = (in->held & UI_BTN_L) != 0;               /* L + D-pad: line start / end, page up / down */
    int vis = ed_visible_lines();
    static const uint32_t DIRS[4] = { UI_BTN_UP, UI_BTN_DOWN, UI_BTN_LEFT, UI_BTN_RIGHT };
    for (int i = 0; i < 4; i++) {
        if (!repeat(&ed->heldSince[i], &ed->lastRepeat[i], in, DIRS[i])) continue;
        int grow = ed->selMode;                         /* in the selection mode every move grows the selection */
        if (sel) {
            if (i == 0) tb_move_lines(ed->tb, -(vis - 1), grow);
            if (i == 1) tb_move_lines(ed->tb, vis - 1, grow);
            if (i == 2) tb_move(ed->tb, TB_HOME, grow);
            if (i == 3) tb_move(ed->tb, TB_END, grow);
        } else tb_move(ed->tb, i == 0 ? TB_UP : i == 1 ? TB_DOWN : i == 2 ? TB_LEFT : TB_RIGHT, grow);
    }
    if (repeat(&ed->bSince, &ed->bLast, in, UI_BTN_B)) { tb_backspace(ed->tb); edited(ed); }
    if (in->down & UI_BTN_Y) { if (tb_undo(ed->tb)) edited(ed); }
    if (in->down & UI_BTN_X) set_sel_mode(ed, !ed->selMode);
    if ((in->down & UI_BTN_R) && ed->nsug) { complete(ed, 0); edited(ed); }
    if (in->down & UI_BTN_SELECT) ed_save(ed);
    if (in->down & UI_BTN_A) run(ed);
    refresh(ed);
    scroll_to_cursor(ed, &L);
    return 1;
}

/* ---------- drawing ---------- */
static void draw_code(Editor* ed, const Layout* L) {
    ui_screen(L->codeScreen);
    char title[300];
    snprintf(title, sizeof title, "%s%s", base_name(ed->path), tb_modified(ed->tb) ? " *" : "");
    ui_rect(0, 0, L->codeW, 16, 0x1C2230);
    ui_text(6, 2, title, 12, 0x7FB2FF);
    int line = tb_line_of(ed->tb, tb_cursor(ed->tb)), col = tb_col_of(ed->tb, tb_cursor(ed->tb));
    char pos[40];
    snprintf(pos, sizeof pos, "%d:%d", line + 1, col + 1);
    ui_text(L->codeW - ui_text_width(pos, 12) - 6, 2, pos, 12, 0x8090A0);

    int vis = (int)((L->codeH - CODE_Y) / LINE_H), nl = tb_lines(ed->tb);
    const char* t = tb_text(ed->tb);
    unsigned char kinds[1024];
    int state = 0;
    for (int l = 0; l < ed->top && l < nl; l++) {       /* comments open before the first line on screen */
        int ll = tb_line_len(ed->tb, l);
        if (ll > (int)sizeof kinds) ll = sizeof kinds;
        state = svc_colors(ed->svc, t + tb_line_start(ed->tb, l), ll, kinds, state);
    }
    int sa, sb;
    tb_selection(ed->tb, &sa, &sb);
    for (int i = 0; i < vis && ed->top + i < nl; i++) {
        int l = ed->top + i, ls = tb_line_start(ed->tb, l), ll = tb_line_len(ed->tb, l);
        if (ll > (int)sizeof kinds) ll = sizeof kinds;
        float y = CODE_Y + i * LINE_H;
        if (l + 1 == ed->errLine) ui_rect(0, y, L->codeW, LINE_H, 0x4A1E22);
        else if (l == line) ui_rect(GUTTER, y, L->codeW - GUTTER, LINE_H, 0x1A1E26);
        char num[12];
        snprintf(num, sizeof num, "%d", l + 1);
        ui_text(GUTTER - 6 - ui_text_width(num, 11), y + 2, num, 11, l == line ? 0xC0C8D0 : 0x5A6270);
        state = svc_colors(ed->svc, t + ls, ll, kinds, state);
        ui_clip(GUTTER, (int)y, (int)(L->codeW - GUTTER), LINE_H);
        float x = GUTTER + 4 - ed->scrollX;
        char seg[1025];
        if (sa != sb && sb > ls && sa <= ls + ll) {      /* the selected part of the line */
            int a = sa > ls ? sa - ls : 0, b = sb < ls + ll ? sb - ls : ll;
            memcpy(seg, t + ls, a); seg[a] = 0;
            float xa = ui_text_width(seg, FONT);
            memcpy(seg, t + ls + a, b - a); seg[b - a] = 0;
            float w = ui_text_width(seg, FONT);
            ui_rect(x + xa, y, w + (sb > ls + ll ? 4 : 0), LINE_H, 0x264F78);
        }
        for (int a = 0; a < ll; ) {                       /* runs of one colour */
            int b = a;
            while (b < ll && kinds[b] == kinds[a]) b++;
            memcpy(seg, t + ls + a, b - a); seg[b - a] = 0;
            ui_text(x, y + 1, seg, FONT, KIND_RGB[kinds[a]]);
            x += ui_text_width(seg, FONT);
            a = b;
        }
        if (l == line && (ed->ms / 500) % 2 == 0) {
            int c = tb_cursor(ed->tb) - ls;
            memcpy(seg, t + ls, c); seg[c] = 0;
            ui_rect(GUTTER + 4 - ed->scrollX + ui_text_width(seg, FONT), y + 1, 1.5f, LINE_H - 2, 0xFFFFFF);
        }
        ui_unclip();
    }
}

static void draw_panel(Editor* ed, const Layout* L) {
    ui_screen(L->panelScreen);
    float y = L->panelY, bw = L->panelW / NTOOLS;
    for (int i = 0; i < NTOOLS; i++) {
        const char* name = ed->selMode ? SEL_NAMES[i] : TOOL_NAMES[i];
        uint32_t bg = ed->selMode ? 0x5A4A20 : i == TOOL_RUN ? 0x2F6B3A : 0x34405A;
        ui_rect(i * bw + 1, y + 1, bw - 2, 20, bg);
        float tw = ui_text_width(name, 12);
        ui_text(i * bw + (bw - tw) / 2, y + 4, name, 12, 0xFFFFFF);
    }
    /* the signature of the call at the cursor, otherwise the status */
    ui_clip(0, (int)(y + 23), (int)L->panelW, 22);
    if (ed->sig[0] && !ed->leaveArmed && !ed->errLine) {
        char hint[160];
        snprintf(hint, sizeof hint, "%s  [arg %d]", ed->sig, ed->sigArg + 1);
        ui_text(4, y + 26, hint, 12, 0xE0D890);
    } else ui_text(4, y + 26, ed->status, 12, ed->statusRgb ? ed->statusRgb : 0x8090A0);
    ui_unclip();
    float sw = L->panelW / NSUG;
    for (int i = 0; i < NSUG; i++) {
        ui_rect(i * sw + 1, y + 46, sw - 2, 31, i < ed->nsug ? 0x24324A : 0x15181E);
        if (i >= ed->nsug) continue;
        float size = 14, tw = ui_text_width(ed->sug[i], size);
        if (tw > sw - 6) { size = size * (sw - 6) / tw; tw = sw - 6; }
        ui_text(i * sw + (sw - tw) / 2, y + 46 + (31 - size) / 2, ed->sug[i], size, i == 0 ? 0xFFFFFF : 0xC0D0E8);
    }
    kbd_set_area(0, y + 80, L->panelW, PANEL_H - 80);
    kbd_draw();
}

void ed_draw(Editor* ed) {
    Layout L = layout();
    draw_code(ed, &L);
    draw_panel(ed, &L);
}
