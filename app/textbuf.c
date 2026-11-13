/* MiniSharp - text buffer of the code editor (see textbuf.h). One contiguous array (the files are small);
 * every change goes through replace(), which also records it for undo. */
#include "textbuf.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define UNDO_MAX 1000

enum { K_OTHER, K_TYPE, K_BACK, K_DEL };       /* kinds of edits that join with the one before */

typedef struct {
    int pos;                    /* where: 'del' was there before, 'ins' is there after */
    char* del;
    char* ins;
    int curBefore, ancBefore, curAfter, ancAfter;
    int kind;
} Edit;

struct TextBuf {
    char* s; int len, cap;
    int* ls; int nl, lcap, linesOk;     /* line starts, rebuilt after a change */
    int cur, anchor;                    /* anchor == cur: no selection */
    int goal;                           /* column kept by Up/Down; -1 = take the cursor's */
    Edit* u; int nu, ucap, top;         /* u[0..top) done, u[top..nu) can be redone */
    int savedTop;                       /* top when saved; -1 = that state cannot come back */
    int join;                           /* the next typing may join the last edit */
};

static int is_id(int c) { return isalnum(c) || c == '_' || c >= 0x80; }
static int is_cont(const TextBuf* tb, int p) { return p > 0 && p < tb->len && ((unsigned char)tb->s[p] & 0xC0) == 0x80; }

static char* dupn(const char* s, int n) {
    char* d = (char*)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n); d[n] = 0;
    return d;
}

static int reserve(TextBuf* tb, int need) {
    if (need + 1 <= tb->cap) return 1;
    int cap = tb->cap ? tb->cap : 256;
    while (cap < need + 1) cap *= 2;
    char* s = (char*)realloc(tb->s, cap);
    if (!s) return 0;
    tb->s = s; tb->cap = cap;
    return 1;
}

/* ---------- lines ---------- */
static void lines(const TextBuf* ctb) {
    TextBuf* tb = (TextBuf*)ctb;
    if (tb->linesOk) return;
    int n = 1;
    for (int i = 0; i < tb->len; i++) n += tb->s[i] == '\n';
    if (n > tb->lcap) {
        int* l = (int*)realloc(tb->ls, n * 2 * sizeof(int));
        if (!l) { tb->nl = 0; return; }
        tb->ls = l; tb->lcap = n * 2;
    }
    tb->ls[0] = 0; tb->nl = 1;
    for (int i = 0; i < tb->len; i++) if (tb->s[i] == '\n') tb->ls[tb->nl++] = i + 1;
    tb->linesOk = 1;
}

int tb_lines(const TextBuf* tb) { lines(tb); return tb->nl ? tb->nl : 1; }

int tb_line_start(const TextBuf* tb, int line) {
    lines(tb);
    if (line < 0) return 0;
    if (line >= tb->nl) return tb->len;
    return tb->ls[line];
}

int tb_line_len(const TextBuf* tb, int line) {
    lines(tb);
    if (line < 0 || line >= tb->nl) return 0;
    int end = line + 1 < tb->nl ? tb->ls[line + 1] - 1 : tb->len;
    return end - tb->ls[line];
}

int tb_line_of(const TextBuf* tb, int pos) {
    lines(tb);
    int lo = 0, hi = tb->nl - 1;
    while (lo < hi) {                   /* the last line starting at or before pos */
        int mid = (lo + hi + 1) / 2;
        if (tb->ls[mid] <= pos) lo = mid; else hi = mid - 1;
    }
    return lo;
}

int tb_col_of(const TextBuf* tb, int pos) {
    int col = 0;
    for (int p = tb_line_start(tb, tb_line_of(tb, pos)); p < pos; p++)
        if (((unsigned char)tb->s[p] & 0xC0) != 0x80) col++;
    return col;
}

int tb_pos_at(const TextBuf* tb, int line, int col) {
    if (line < 0) return 0;
    if (line >= tb_lines(tb)) return tb->len;
    int p = tb_line_start(tb, line), end = p + tb_line_len(tb, line);
    while (p < end && col > 0) {
        p++;
        while (p < end && ((unsigned char)tb->s[p] & 0xC0) == 0x80) p++;
        col--;
    }
    return p;
}

/* ---------- life ---------- */
static void clear_undo(TextBuf* tb) {
    for (int i = 0; i < tb->nu; i++) { free(tb->u[i].del); free(tb->u[i].ins); }
    tb->nu = tb->top = 0;
    tb->savedTop = 0;
    tb->join = 0;
}

int tb_set_text(TextBuf* tb, const char* text) {
    if (!text) text = "";
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) text += 3;
    int n = (int)strlen(text);
    if (!reserve(tb, n)) return 0;
    int k = 0;
    for (int i = 0; i < n; i++) if (!(text[i] == '\r' && text[i + 1] == '\n')) tb->s[k++] = text[i] == '\r' ? '\n' : text[i];
    tb->s[k] = 0;
    tb->len = k;
    tb->linesOk = 0;
    tb->cur = tb->anchor = 0;
    tb->goal = -1;
    clear_undo(tb);
    return 1;
}

TextBuf* tb_new(const char* text) {
    TextBuf* tb = (TextBuf*)calloc(1, sizeof(TextBuf));
    if (!tb) return NULL;
    if (!tb_set_text(tb, text)) { tb_free(tb); return NULL; }
    return tb;
}

void tb_free(TextBuf* tb) {
    if (!tb) return;
    clear_undo(tb);
    free(tb->u); free(tb->s); free(tb->ls);
    free(tb);
}

const char* tb_text(const TextBuf* tb) { return tb->s ? tb->s : ""; }
int tb_len(const TextBuf* tb) { return tb->len; }

/* ---------- the one place that changes the text ---------- */
static int apply(TextBuf* tb, int pos, int dellen, const char* ins, int inslen) {
    if (!reserve(tb, tb->len - dellen + inslen)) return 0;
    memmove(tb->s + pos + inslen, tb->s + pos + dellen, tb->len - pos - dellen + 1);
    memcpy(tb->s + pos, ins, inslen);
    tb->len += inslen - dellen;
    tb->linesOk = 0;
    return 1;
}

static void push_edit(TextBuf* tb, Edit* e) {
    for (int i = tb->top; i < tb->nu; i++) { free(tb->u[i].del); free(tb->u[i].ins); }   /* no more redo */
    tb->nu = tb->top;
    if (tb->savedTop > tb->top) tb->savedTop = -1;
    if (tb->nu == UNDO_MAX) {                       /* forget the oldest */
        free(tb->u[0].del); free(tb->u[0].ins);
        memmove(tb->u, tb->u + 1, (tb->nu - 1) * sizeof(Edit));
        tb->nu--; tb->top--;
        if (tb->savedTop >= 0) tb->savedTop--;
    }
    if (tb->nu == tb->ucap) {
        int cap = tb->ucap ? tb->ucap * 2 : 64;
        Edit* u = (Edit*)realloc(tb->u, cap * sizeof(Edit));
        if (!u) { free(e->del); free(e->ins); return; }
        tb->u = u; tb->ucap = cap;
    }
    tb->u[tb->nu++] = *e;
    tb->top = tb->nu;
}

static char* concat(const char* a, int an, const char* b, int bn) {
    char* d = (char*)malloc(an + bn + 1);
    if (!d) return NULL;
    memcpy(d, a, an); memcpy(d + an, b, bn); d[an + bn] = 0;
    return d;
}

/* replaces text[from..to) with ins, records it, puts the cursor after the inserted text */
static void replace(TextBuf* tb, int from, int to, const char* ins, int kind) {
    int inslen = (int)strlen(ins), dellen = to - from;
    if (!dellen && !inslen) return;
    Edit* last = tb->top > 0 && tb->top == tb->nu && tb->savedTop != tb->top ? &tb->u[tb->top - 1] : NULL;
    int join = tb->join && last && last->kind == kind && kind != K_OTHER;
    if (join && kind == K_TYPE && !dellen && last->pos + (int)strlen(last->ins) == from && !strchr(ins, '\n')) {
        char* n = concat(last->ins, (int)strlen(last->ins), ins, inslen);
        if (!n || !apply(tb, from, 0, ins, inslen)) { free(n); return; }
        free(last->ins); last->ins = n;
        tb->cur = tb->anchor = from + inslen;
        last->curAfter = last->ancAfter = tb->cur;
        tb->goal = -1;
        return;
    }
    if (join && kind == K_BACK && !inslen && to == last->pos) {      /* Backspace again: the deleted text grows left */
        char* n = concat(tb->s + from, dellen, last->del, (int)strlen(last->del));
        if (!n || !apply(tb, from, dellen, "", 0)) { free(n); return; }
        free(last->del); last->del = n; last->pos = from;
        tb->cur = tb->anchor = from;
        last->curAfter = last->ancAfter = from;
        tb->goal = -1;
        return;
    }
    if (join && kind == K_DEL && !inslen && from == last->pos) {     /* Delete again: it grows right */
        char* n = concat(last->del, (int)strlen(last->del), tb->s + from, dellen);
        if (!n || !apply(tb, from, dellen, "", 0)) { free(n); return; }
        free(last->del); last->del = n;
        tb->goal = -1;
        return;
    }
    Edit e = { from, dupn(tb->s + from, dellen), dupn(ins, inslen), tb->cur, tb->anchor, 0, 0, kind };
    if (!e.del || !e.ins || !apply(tb, from, dellen, ins, inslen)) { free(e.del); free(e.ins); return; }
    tb->cur = tb->anchor = from + inslen;
    e.curAfter = e.ancAfter = tb->cur;
    push_edit(tb, &e);
    tb->join = kind != K_OTHER;
    tb->goal = -1;
}

/* ---------- cursor ---------- */
int tb_cursor(const TextBuf* tb) { return tb->cur; }

static int clamp(const TextBuf* tb, int pos) {
    if (pos < 0) pos = 0;
    if (pos > tb->len) pos = tb->len;
    while (is_cont(tb, pos)) pos--;
    return pos;
}

void tb_set_cursor(TextBuf* tb, int pos, int select) {
    tb->cur = clamp(tb, pos);
    if (!select) tb->anchor = tb->cur;
    tb->goal = -1;
    tb->join = 0;
}

static int prev_char(const TextBuf* tb, int p) { if (p > 0) p--; while (is_cont(tb, p)) p--; return p; }
static int next_char(const TextBuf* tb, int p) { if (p < tb->len) p++; while (is_cont(tb, p)) p++; return p; }

void tb_move_lines(TextBuf* tb, int n, int select) {
    int goal = tb->goal >= 0 ? tb->goal : tb_col_of(tb, tb->cur);
    int line = tb_line_of(tb, tb->cur) + n;
    int pos;
    if (line < 0) pos = 0;
    else if (line >= tb_lines(tb)) pos = tb->len;
    else pos = tb_pos_at(tb, line, goal);
    tb_set_cursor(tb, pos, select);
    if (line >= 0 && line < tb_lines(tb)) tb->goal = goal;      /* kept for the next Up/Down */
}

void tb_move(TextBuf* tb, int how, int select) {
    int p = tb->cur;
    int line = tb_line_of(tb, p), ls = tb_line_start(tb, line), le = ls + tb_line_len(tb, line);
    switch (how) {
    case TB_LEFT:
        if (!select && tb->cur != tb->anchor) { tb_set_cursor(tb, tb->cur < tb->anchor ? tb->cur : tb->anchor, 0); return; }
        p = prev_char(tb, p); break;
    case TB_RIGHT:
        if (!select && tb->cur != tb->anchor) { tb_set_cursor(tb, tb->cur > tb->anchor ? tb->cur : tb->anchor, 0); return; }
        p = next_char(tb, p); break;
    case TB_UP: tb_move_lines(tb, -1, select); return;
    case TB_DOWN: tb_move_lines(tb, 1, select); return;
    case TB_HOME: {                     /* first non-space of the line; pressed there: the line's start */
        int first = ls;
        while (first < le && (tb->s[first] == ' ' || tb->s[first] == '\t')) first++;
        p = p == first ? ls : first;
        break;
    }
    case TB_END: p = le; break;
    case TB_WORD_LEFT:
        while (p > 0 && (tb->s[p - 1] == ' ' || tb->s[p - 1] == '\t')) p--;
        if (p > 0 && is_id((unsigned char)tb->s[p - 1])) while (p > 0 && is_id((unsigned char)tb->s[p - 1])) p--;
        else if (p > 0) p = prev_char(tb, p);
        break;
    case TB_WORD_RIGHT:
        if (p < tb->len && is_id((unsigned char)tb->s[p])) while (p < tb->len && is_id((unsigned char)tb->s[p])) p++;
        else if (p < tb->len) p = next_char(tb, p);
        while (p < tb->len && (tb->s[p] == ' ' || tb->s[p] == '\t')) p++;
        break;
    case TB_TOP: p = 0; break;
    case TB_BOTTOM: p = tb->len; break;
    }
    tb_set_cursor(tb, p, select);
}

int tb_has_selection(const TextBuf* tb) { return tb->cur != tb->anchor; }

void tb_selection(const TextBuf* tb, int* from, int* to) {
    *from = tb->cur < tb->anchor ? tb->cur : tb->anchor;
    *to = tb->cur < tb->anchor ? tb->anchor : tb->cur;
}

void tb_select_all(TextBuf* tb) { tb->anchor = 0; tb->cur = tb->len; tb->goal = -1; tb->join = 0; }

void tb_select_word(TextBuf* tb) {
    int a = tb->cur, b = tb->cur;
    while (a > 0 && is_id((unsigned char)tb->s[a - 1])) a--;
    while (b < tb->len && is_id((unsigned char)tb->s[b])) b++;
    tb->anchor = a; tb->cur = b; tb->goal = -1; tb->join = 0;
}

char* tb_copy(const TextBuf* tb) {
    int a, b;
    tb_selection(tb, &a, &b);
    return dupn(tb->s + a, b - a);
}

/* ---------- editing ---------- */
void tb_insert(TextBuf* tb, const char* s) {
    if (!s || !*s) return;
    int a, b;
    tb_selection(tb, &a, &b);
    if (a == b && !strcmp(s, "}")) {    /* '}' at the start of an indented line: one level left */
        int ls = tb_line_start(tb, tb_line_of(tb, a)), k = 0;
        while (ls + k < a && tb->s[ls + k] == ' ') k++;
        if (ls + k == a && k >= TB_INDENT) { replace(tb, a - TB_INDENT, a, "}", K_OTHER); return; }
    }
    if (strchr(s, '\n') || a != b) { replace(tb, a, b, s, K_OTHER); tb->join = 0; return; }
    replace(tb, a, b, s, K_TYPE);
}

void tb_backspace(TextBuf* tb) {
    int a, b;
    tb_selection(tb, &a, &b);
    if (a != b) { replace(tb, a, b, "", K_OTHER); tb->join = 0; return; }
    if (a == 0) return;
    replace(tb, prev_char(tb, a), a, "", K_BACK);
}

void tb_delete(TextBuf* tb) {
    int a, b;
    tb_selection(tb, &a, &b);
    if (a != b) { replace(tb, a, b, "", K_OTHER); tb->join = 0; return; }
    if (a == tb->len) return;
    int keep = a;
    replace(tb, a, next_char(tb, a), "", K_DEL);
    tb->cur = tb->anchor = keep;
}

void tb_newline(TextBuf* tb) {
    int a, b;
    tb_selection(tb, &a, &b);
    int ls = tb_line_start(tb, tb_line_of(tb, a)), ind = 0;
    while (ls + ind < a && tb->s[ls + ind] == ' ') ind++;
    int q = a;
    while (q > ls && tb->s[q - 1] == ' ') q--;
    int open = q > ls && tb->s[q - 1] == '{';
    int closeNext = open && b < tb->len && tb->s[b] == '}';
    char buf[512];
    int inner = ind + (open ? TB_INDENT : 0);
    if (inner > 200) inner = 200;
    int n = 0;
    buf[n++] = '\n';
    memset(buf + n, ' ', inner); n += inner;
    int cursorAt = n;
    if (closeNext) { buf[n++] = '\n'; memset(buf + n, ' ', ind > 200 ? 200 : ind); n += ind > 200 ? 200 : ind; }
    buf[n] = 0;
    replace(tb, a, b, buf, K_OTHER);
    tb->cur = tb->anchor = a + cursorAt;
    if (tb->top > 0) tb->u[tb->top - 1].curAfter = tb->u[tb->top - 1].ancAfter = tb->cur;
    tb->join = 0;
}

void tb_indent(TextBuf* tb, int dir) {
    int a, b;
    tb_selection(tb, &a, &b);
    int l0 = tb_line_of(tb, a), l1 = tb_line_of(tb, b);
    if (l1 > l0 && b == tb_line_start(tb, l1)) l1--;            /* a selection ending at a line start: not that line */
    int from = tb_line_start(tb, l0), to = tb_line_start(tb, l1) + tb_line_len(tb, l1);
    int cap = (to - from) + (l1 - l0 + 1) * TB_INDENT + 1, n = 0;
    char* out = (char*)malloc(cap);
    if (!out) return;
    for (int l = l0; l <= l1; l++) {
        int ls = tb_line_start(tb, l), ll = tb_line_len(tb, l), skip = 0;
        if (dir > 0 && ll) { memset(out + n, ' ', TB_INDENT); n += TB_INDENT; }
        if (dir < 0) while (skip < TB_INDENT && skip < ll && tb->s[ls + skip] == ' ') skip++;
        memcpy(out + n, tb->s + ls + skip, ll - skip); n += ll - skip;
        if (l < l1) out[n++] = '\n';
    }
    out[n] = 0;
    int single = a == b;
    int col = tb_col_of(tb, a), keepLine = l0;
    if (strncmp(out, tb->s + from, to - from) || n != to - from) replace(tb, from, to, out, K_OTHER);
    free(out);
    if (single) {                       /* the cursor stays on its character */
        int c = col + (dir > 0 ? TB_INDENT : -TB_INDENT);
        tb_set_cursor(tb, tb_pos_at(tb, keepLine, c < 0 ? 0 : c), 0);
    } else {                            /* the whole lines stay selected */
        tb->anchor = tb_line_start(tb, l0);
        tb->cur = tb_line_start(tb, l1) + tb_line_len(tb, l1);
    }
    tb->join = 0;
}

/* ---------- undo ---------- */
int tb_undo(TextBuf* tb) {
    if (tb->top == 0) return 0;
    Edit* e = &tb->u[tb->top - 1];
    int inslen = (int)strlen(e->ins), dellen = (int)strlen(e->del);
    if (!apply(tb, e->pos, inslen, e->del, dellen)) return 0;
    tb->top--;
    tb->cur = e->curBefore; tb->anchor = e->ancBefore;
    tb->goal = -1; tb->join = 0;
    return 1;
}

int tb_redo(TextBuf* tb) {
    if (tb->top == tb->nu) return 0;
    Edit* e = &tb->u[tb->top];
    int inslen = (int)strlen(e->ins), dellen = (int)strlen(e->del);
    if (!apply(tb, e->pos, dellen, e->ins, inslen)) return 0;
    tb->top++;
    tb->cur = e->curAfter; tb->anchor = e->ancAfter;
    tb->goal = -1; tb->join = 0;
    return 1;
}

int tb_modified(const TextBuf* tb) { return tb->savedTop != tb->top; }
void tb_set_saved(TextBuf* tb) { tb->savedTop = tb->top; tb->join = 0; }
