/* MiniSharp - REPL (terminal). Each input is compiled as the body of
 *     class Repl { <earlier variables as static fields> static void Main() { <input> } }
 * and run on one VM that lives as long as the terminal, so the values of the fields carry over.
 * Moved from the old src/repl.c (931ad08); new: inputs of several lines (repl_incomplete), no platform header. */
#include "ms.h"
#include "vm.h"
#include "repl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#define DECLS_CAP 4096
#define TIME_LIMIT 4                    /* seconds (+ up to one) before a loop is stopped */

struct Repl {
    VM* vm;
    Program** progs; int nprogs, cap;   /* every input run so far: values may point at their string constants */
    char decls[DECLS_CAP];              /* "static int x; static string s; " */
};

static const char* SKELETON = "class Repl { static void Main() { } }";

static int keep(Repl* r, Program* p) {
    if (r->nprogs == r->cap) {
        int cap = r->cap ? r->cap * 2 : 16;
        Program** n = (Program**)realloc(r->progs, sizeof(Program*) * cap);
        if (!n) return 0;
        r->progs = n; r->cap = cap;
    }
    r->progs[r->nprogs++] = p;
    return 1;
}

Repl* repl_new(void) {
    Repl* r = (Repl*)calloc(1, sizeof(Repl));
    if (!r) return NULL;
    char err[200];
    Program* p = ms_compile(SKELETON, err, sizeof err);
    if (!p || !keep(r, p) || !(r->vm = vm_new(p))) { if (p && !r->nprogs) ms_free_program(p); repl_free(r); return NULL; }
    return r;
}

void repl_free(Repl* r) {
    if (!r) return;
    if (r->vm) vm_free(r->vm);
    for (int i = 0; i < r->nprogs; i++) ms_free_program(r->progs[i]);
    free(r->progs);
    free(r);
}

/* "line 1: x" and "line 1 (Repl.Main): x" -> "x": a one-line input needs no line number.
 * For several lines only the method name goes: "line 2 (Repl.Main): x" -> "line 2: x". */
static void strip_line(char* s, int multi) {
    if (strncmp(s, "line ", 5)) return;
    char* p = s + 5;
    while (isdigit((unsigned char)*p)) p++;
    char* q;
    if (!strncmp(p, ": ", 2)) q = p + 2;
    else if (!strncmp(p, " (Repl.Main): ", 14)) q = p + 14;
    else return;
    if (multi) { memmove(p + 2, q, strlen(q) + 1); p[0] = ':'; p[1] = ' '; }
    else memmove(s, q, strlen(q) + 1);
}

static int isword(int ch) { return isalnum(ch) || ch == '_'; }

/* the word that ends just before 'end' (spaces skipped) is 'w' */
static int word_before(const char* start, const char* end, const char* w) {
    while (end > start && isspace((unsigned char)end[-1])) end--;
    int n = (int)strlen(w);
    return end - start >= n && !memcmp(end - n, w, n) && (end - n == start || !isword((unsigned char)end[-n - 1]));
}

int repl_incomplete(const char* s) {
    const char* start = s;
    const char* open = NULL;        /* the '(' that opened the current top-level parentheses */
    int depth = 0, header = 0;      /* header: the text ends with "for (...)", "if (...)"… or "else"/"do" with no body yet */
    while (*s) {
        if (isspace((unsigned char)*s)) { s++; continue; }
        if (s[0] == '/' && s[1] == '/') { while (*s && *s != '\n') s++; continue; }
        if (s[0] == '/' && s[1] == '*') {
            s += 2;
            while (*s && !(s[0] == '*' && s[1] == '/')) s++;
            if (!*s) return 1;                  /* the comment goes on */
            s += 2; continue;
        }
        if (*s == '"' || *s == '\'') {          /* a string or char ends on its line: an unclosed one is the compiler's error */
            char q = *s++;
            while (*s && *s != q && *s != '\n') { if (*s == '\\' && s[1]) s++; s++; }
            if (*s == q) s++;
            header = 0;
            continue;
        }
        header = 0;
        if (*s == '(' && depth == 0) open = s;
        if (*s == '{' || *s == '(' || *s == '[') depth++;
        else if (*s == '}' || *s == ')' || *s == ']') {
            depth--;
            if (*s == ')' && depth == 0 && open)
                header = word_before(start, open, "for") || word_before(start, open, "foreach") ||
                         word_before(start, open, "while") || word_before(start, open, "if");
        } else if (isword((unsigned char)*s) && (s == start || !isword((unsigned char)s[-1]))) {
            const char* e = s;
            while (isword((unsigned char)*e)) e++;
            header = depth == 0 && ((e - s == 4 && !memcmp(s, "else", 4)) || (e - s == 2 && !memcmp(s, "do", 2)));
            s = e; continue;
        }
        s++;
    }
    return depth > 0 || header;
}

int repl_eval(Repl* r, const char* text, char* err, int errlen) {
    err[0] = 0;
    int n = (int)strlen(text);
    while (n > 0 && isspace((unsigned char)text[n - 1])) n--;
    if (n == 0) return 1;
    int multi = memchr(text, '\n', n) != NULL;
    int semi = text[n - 1] != ';' && text[n - 1] != '}';     /* "Math.Sqrt(2)" needs no ';' */

    size_t cap = strlen(r->decls) + n + 64;
    char* src = (char*)malloc(cap);
    char* decls = (char*)malloc(DECLS_CAP);
    if (!src || !decls) { free(src); free(decls); snprintf(err, errlen, "out of memory"); return 0; }
    snprintf(src, cap, "class Repl { %sstatic void Main() { %.*s%s\n} }", r->decls, n, text, semi ? ";" : "");
    memcpy(decls, r->decls, DECLS_CAP);
    Program* p = ms_compile_repl(src, err, errlen, decls, DECLS_CAP);
    free(src);
    if (!p) {
        free(decls);
        const char* t = text;
        while (isspace((unsigned char)*t)) t++;
        if (!strncmp(t, "else", 4) && !isword((unsigned char)t[4])) {   /* the 'if' already ran as the previous input */
            snprintf(err, errlen, "'else' has no 'if': in the terminal write '} else {' on one line");
            return 0;
        }
        strip_line(err, multi);
        char* f = strstr(err, "found ';'");          /* the ';' we added, not one the user typed */
        if (semi && f && !f[9]) snprintf(f, errlen - (f - err), "found end of line");
        return 0;
    }
    if (!keep(r, p)) { ms_free_program(p); free(decls); snprintf(err, errlen, "out of memory"); return 0; }
    if (!vm_set_program(r->vm, p)) { free(decls); snprintf(err, errlen, "out of memory"); return 0; }
    memcpy(r->decls, decls, DECLS_CAP);         /* new variables exist from now on, even if the input fails */
    free(decls);

    VM* vm = r->vm;
    vm->sp = 0; vm->fp = 0; vm->quit = 0;       /* a failed input may have left frames and its error behind */
    vm->err[0] = 0;
    vm->deadline = (long long)time(NULL) + TIME_LIMIT;
    int ok = vm_call(vm, p->entryMain);
    vm->deadline = 0;
    if (!ok && !vm->quit) { snprintf(err, errlen, "%s", vm->err); strip_line(err, multi); return 0; }
    return 1;
}

/* ---------- completion ---------- */
static const char* KEYWORDS[] = {
    "var", "int", "float", "bool", "char", "string", "new", "null", "true", "false",
    "if", "else", "for", "foreach", "in", "while", "do", "switch", "break", "continue", "return", "ref", "out",
    "List", "Dictionary",
};


static int has(char out[][REPL_WORD], int n, const char* s) {
    for (int i = 0; i < n; i++) if (!strcmp(out[i], s)) return 1;
    return 0;
}

/* adds s if it starts with the prefix (any letter case: "con" finds Console) and is longer */
static int offer(char out[][REPL_WORD], int n, int max, const char* s, int slen, const char* pre, int plen) {
    if (n >= max || slen <= plen || slen >= REPL_WORD) return n;
    for (int i = 0; i < plen; i++) if (tolower((unsigned char)s[i]) != tolower((unsigned char)pre[i])) return n;
    char w[REPL_WORD];
    memcpy(w, s, slen); w[slen] = 0;
    if (has(out, n, w)) return n;
    strcpy(out[n], w);
    return n + 1;
}

int repl_complete(Repl* r, const char* line, int cursor, int* start, char out[][REPL_WORD], int max) {
    /* the terminal's own variables: "static int x; " -> "x" */
    char words[DECLS_CAP];
    int n = 0;
    for (const char* s = r->decls; (s = strchr(s, ';')); s++) {
        const char* e = s;
        while (e > r->decls && e[-1] != ' ') e--;
        n += snprintf(words + n, sizeof words - n, "%.*s ", (int)(s - e), e);
    }
    words[n] = 0;
    return ms_complete(line, cursor, start, out, max, words);
}

int ms_complete(const char* line, int cursor, int* start, char out[][REPL_WORD], int max, const char* words) {
    int ws = cursor;
    while (ws > 0 && isword((unsigned char)line[ws - 1])) ws--;
    *start = ws;
    const char* pre = line + ws; int plen = cursor - ws;
    int n = 0;
    if (ws > 0 && line[ws - 1] == '.') {                /* members of a built-in class */
        int os = ws - 1;
        while (os > 0 && isword((unsigned char)line[os - 1])) os--;
        int olen = ws - 1 - os;
        if (olen == 0) return 0;
        for (int i = 0; i < ms_nnatives; i++) {
            const NativeDef* d = &ms_natives[i];
            if ((int)strlen(d->cls) == olen && !memcmp(d->cls, line + os, olen))
                n = offer(out, n, max, d->name, (int)strlen(d->name), pre, plen);
        }
        for (int i = 0; i < ms_nconsts; i++) {
            const ConstDef* d = &ms_consts[i];
            if ((int)strlen(d->cls) == olen && !memcmp(d->cls, line + os, olen))
                n = offer(out, n, max, d->name, (int)strlen(d->name), pre, plen);
        }
        return n;
    }
    if (plen == 0 || isdigit((unsigned char)line[ws])) return 0;
    for (const char* s = words; *s; ) {                 /* the caller's names first */
        while (*s == ' ') s++;
        const char* e = s;
        while (*e && *e != ' ') e++;
        if (e > s) n = offer(out, n, max, s, (int)(e - s), pre, plen);
        s = e;
    }
    for (int i = 0; i < ms_nnatives; i++)
        if (ms_natives[i].cls[0] != '.' && ms_natives[i].cls[0] != '#') n = offer(out, n, max, ms_natives[i].cls, (int)strlen(ms_natives[i].cls), pre, plen);
    for (int i = 0; i < ms_nconsts; i++) n = offer(out, n, max, ms_consts[i].cls, (int)strlen(ms_consts[i].cls), pre, plen);
    for (size_t i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++)
        n = offer(out, n, max, KEYWORDS[i], (int)strlen(KEYWORDS[i]), pre, plen);
    return n;
}
