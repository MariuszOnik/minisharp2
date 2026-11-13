/* MiniSharp - language service (see service.h). */
#include "service.h"
#include "ms.h"
#include "vm.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MsService {
    CodeIndex ci;
    char** texts;
    int n;
};

static int is_id0(int c) { return isalpha(c) || c == '_'; }
static int is_id(int c) { return isalnum(c) || c == '_'; }

/* ---------- words ---------- */
/* keywords offered as suggestions (the ones a beginner types; colours know more, see COLOR_KEYWORDS) */
static const char* KEYWORDS[] = {
    "var", "int", "float", "bool", "char", "string", "new", "null", "true", "false",
    "if", "else", "for", "foreach", "in", "while", "do", "switch", "break", "continue", "return", "ref", "out",
    "List", "Dictionary",
};

static const char* COLOR_KEYWORDS[] = {
    "abstract", "as", "base", "break", "case", "class", "const", "continue", "default", "do", "else", "enum", "false",
    "for", "foreach", "get", "if", "in", "interface", "internal", "is", "namespace", "new", "null", "out", "override",
    "private", "protected", "public", "readonly", "ref", "return", "sealed", "set", "static", "struct", "switch",
    "this", "true", "using", "value", "virtual", "while", "yield",
};

static const char* TYPE_WORDS[] = {
    "bool", "char", "float", "int", "object", "string", "var", "void", "List", "Dictionary", "KeyValuePair",
    "Func", "Action", "Predicate", "Comparison", "IEnumerator",
};

static int in_words(const char** list, int n, const char* s, int len) {
    for (int i = 0; i < n; i++) if ((int)strlen(list[i]) == len && !memcmp(list[i], s, len)) return 1;
    return 0;
}
#define IN(list, s, len) in_words(list, (int)(sizeof list / sizeof list[0]), s, len)

static int builtin_class(const char* s, int n) {
    for (int i = 0; i < ms_nnatives; i++) if ((int)strlen(ms_natives[i].cls) == n && !memcmp(ms_natives[i].cls, s, n)) return 1;
    for (int i = 0; i < ms_nconsts; i++) if ((int)strlen(ms_consts[i].cls) == n && !memcmp(ms_consts[i].cls, s, n)) return 1;
    return 0;
}

static int user_class(const MsService* svc, const char* s, int n) {
    if (!svc) return 0;
    for (int i = 0; i < svc->ci.nclasses; i++)
        if ((int)strlen(svc->ci.cls[i].name) == n && !memcmp(svc->ci.cls[i].name, s, n)) return 1;
    return 0;
}

/* ---------- colours ---------- */
int svc_colors(const MsService* svc, const char* s, int len, unsigned char* k, int state) {
    int i = 0;
    while (i < len) {
        if (state == 1) {                               /* inside a comment slash-star */
            while (i < len && !(s[i] == '*' && i + 1 < len && s[i + 1] == '/')) k[i++] = SVC_COMMENT;
            if (i < len) { k[i++] = SVC_COMMENT; k[i++] = SVC_COMMENT; state = 0; }
            continue;
        }
        unsigned char c = (unsigned char)s[i];
        if (c == '/' && i + 1 < len && s[i + 1] == '/') { while (i < len) k[i++] = SVC_COMMENT; break; }
        if (c == '/' && i + 1 < len && s[i + 1] == '*') { k[i++] = SVC_COMMENT; k[i++] = SVC_COMMENT; state = 1; continue; }
        if (c == '"' || c == '\'' || ((c == '$' || c == '@') && i + 1 < len && s[i + 1] == '"')) {
            int q = c == '\'' ? '\'' : '"', verbatim = c == '@';
            if (c == '$' || c == '@') k[i++] = SVC_STRING;
            k[i++] = SVC_STRING;
            while (i < len && s[i] != q) {
                if (s[i] == '\\' && !verbatim && i + 1 < len) k[i++] = SVC_STRING;
                k[i++] = SVC_STRING;
            }
            if (i < len) k[i++] = SVC_STRING;
            continue;
        }
        if (isdigit(c) || (c == '.' && i + 1 < len && isdigit((unsigned char)s[i + 1]))) {
            int prevId = i > 0 && is_id((unsigned char)s[i - 1]);
            unsigned char kind = prevId ? SVC_PLAIN : SVC_NUMBER;     /* the 2 in "x2" is part of a name */
            while (i < len && (is_id((unsigned char)s[i]) || s[i] == '.')) k[i++] = kind;
            continue;
        }
        if (is_id0(c)) {
            int a = i;
            while (i < len && is_id((unsigned char)s[i])) i++;
            int n = i - a;
            unsigned char kind = SVC_PLAIN;
            if (IN(TYPE_WORDS, s + a, n) || builtin_class(s + a, n) || user_class(svc, s + a, n)) kind = SVC_TYPE;
            else if (IN(COLOR_KEYWORDS, s + a, n)) kind = SVC_KEYWORD;
            memset(k + a, kind, n);
            continue;
        }
        k[i++] = SVC_PLAIN;
    }
    return state;
}

/* ---------- suggestions in one line ---------- */
static int has(char out[][SVC_WORD], int n, const char* s) {
    for (int i = 0; i < n; i++) if (!strcmp(out[i], s)) return 1;
    return 0;
}

/* adds s if it starts with the prefix (any letter case: "con" finds Console) and is longer */
static int offer(char out[][SVC_WORD], int n, int max, const char* s, int slen, const char* pre, int plen) {
    if (n >= max || slen <= plen || slen >= SVC_WORD) return n;
    for (int i = 0; i < plen; i++) if (tolower((unsigned char)s[i]) != tolower((unsigned char)pre[i])) return n;
    char w[SVC_WORD];
    memcpy(w, s, slen); w[slen] = 0;
    if (has(out, n, w)) return n;
    strcpy(out[n], w);
    return n + 1;
}

static int offer_words(char out[][SVC_WORD], int n, int max, const char* words, const char* pre, int plen) {
    for (const char* s = words; s && *s; ) {
        while (*s == ' ') s++;
        const char* e = s;
        while (*e && *e != ' ') e++;
        if (e > s) n = offer(out, n, max, s, (int)(e - s), pre, plen);
        s = e;
    }
    return n;
}

/* built-in classes and keywords after the caller's words */
static int offer_global(char out[][SVC_WORD], int n, int max, const char* pre, int plen) {
    for (int i = 0; i < ms_nnatives; i++)
        if (ms_natives[i].cls[0] != '.' && ms_natives[i].cls[0] != '#') n = offer(out, n, max, ms_natives[i].cls, (int)strlen(ms_natives[i].cls), pre, plen);
    for (int i = 0; i < ms_nconsts; i++) n = offer(out, n, max, ms_consts[i].cls, (int)strlen(ms_consts[i].cls), pre, plen);
    for (size_t i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++)
        n = offer(out, n, max, KEYWORDS[i], (int)strlen(KEYWORDS[i]), pre, plen);
    return n;
}

int svc_complete_line(const char* line, int cursor, int* start, char out[][SVC_WORD], int max, const char* words) {
    int ws = cursor;
    while (ws > 0 && is_id((unsigned char)line[ws - 1])) ws--;
    *start = ws;
    const char* pre = line + ws; int plen = cursor - ws;
    int n = 0;
    if (ws > 0 && line[ws - 1] == '.') {                /* members of a built-in class */
        int os = ws - 1;
        while (os > 0 && is_id((unsigned char)line[os - 1])) os--;
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
    n = offer_words(out, n, max, words, pre, plen);
    return offer_global(out, n, max, pre, plen);
}

/* ---------- a project ---------- */
MsService* svc_new(void) { return (MsService*)calloc(1, sizeof(MsService)); }

static void free_texts(MsService* svc) {
    for (int i = 0; i < svc->n; i++) free(svc->texts[i]);
    free(svc->texts);
    svc->texts = NULL; svc->n = 0;
}

void svc_free(MsService* svc) {
    if (!svc) return;
    ci_free(&svc->ci);
    free_texts(svc);
    free(svc);
}

static char* dup(const char* s) {
    size_t n = strlen(s ? s : "");
    char* d = (char*)malloc(n + 1);
    if (d) memcpy(d, s ? s : "", n + 1);
    return d;
}

static void rebuild(MsService* svc) { ci_build(&svc->ci, (const char**)svc->texts, svc->n); }

int svc_set_texts(MsService* svc, const char** texts, int n) {
    free_texts(svc);
    svc->texts = (char**)calloc(n > 0 ? n : 1, sizeof(char*));
    if (!svc->texts) return 0;
    for (int i = 0; i < n; i++) if (!(svc->texts[i] = dup(texts[i]))) { svc->n = i; rebuild(svc); return 0; }
    svc->n = n;
    rebuild(svc);
    return 1;
}

int svc_set_text(MsService* svc, int file, const char* text) {
    if (file < 0 || file >= svc->n) return 0;
    char* d = dup(text);
    if (!d) return 0;
    free(svc->texts[file]);
    svc->texts[file] = d;
    rebuild(svc);
    return 1;
}

/* skips a string or char literal starting at s[p] (a quote); returns the index after it */
static int skip_literal(const char* s, int p, int end) {
    int q = s[p], verbatim = p > 0 && s[p - 1] == '@';
    for (p++; p < end && s[p] != q && s[p] != '\n'; p++)
        if (s[p] == '\\' && !verbatim) p++;
    return p < end ? p + 1 : end;
}

/* the names in the text (outside strings and comments), separated by spaces, each once, without 'skip'
 * (the word being typed) and without numbers */
static void file_words(const char* s, int skipStart, int skipEnd, char* out, int cap) {
    int n = 0, len = (int)strlen(s);
    out[0] = 0;
    for (int p = 0; p < len; ) {
        char c = s[p];
        if (c == '/' && s[p + 1] == '/') { while (p < len && s[p] != '\n') p++; continue; }
        if (c == '/' && s[p + 1] == '*') { const char* e = strstr(s + p + 2, "*/"); p = e ? (int)(e - s) + 2 : len; continue; }
        if (c == '"' || c == '\'') { p = skip_literal(s, p, len); continue; }
        if (!is_id0((unsigned char)c) || (p > 0 && is_id((unsigned char)s[p - 1]))) { p++; continue; }
        int a = p;
        while (p < len && is_id((unsigned char)s[p])) p++;
        int wl = p - a;
        if (a == skipStart && p == skipEnd) continue;
        if (wl < 2 || wl >= SVC_WORD || IN(COLOR_KEYWORDS, s + a, wl) || IN(TYPE_WORDS, s + a, wl)) continue;
        /* already listed? */
        int dupw = 0;
        for (const char* q = out; *q; ) {
            const char* e = strchr(q, ' ');
            if (!e) break;
            if (e - q == wl && !memcmp(q, s + a, wl)) { dupw = 1; break; }
            q = e + 1;
        }
        if (dupw || n + wl + 2 >= cap) continue;
        memcpy(out + n, s + a, wl);
        n += wl;
        out[n++] = ' ';
        out[n] = 0;
    }
}

int svc_complete(MsService* svc, int file, int cursor, int* start, char out[][SVC_WORD], int max) {
    if (!svc || file < 0 || file >= svc->n) { *start = cursor; return 0; }
    const char* s = svc->texts[file];
    int len = (int)strlen(s);
    if (cursor > len) cursor = len;
    int ws = cursor;
    while (ws > 0 && is_id((unsigned char)s[ws - 1])) ws--;
    *start = ws;
    if (ws > 0 && s[ws - 1] == '.') {
        int n = ci_complete(&svc->ci, file, cursor, start, out, max);
        return n < 0 ? 0 : n;
    }
    int plen = cursor - ws;
    if (plen == 0 || isdigit((unsigned char)s[ws])) return 0;
    int we = cursor;
    while (we < len && is_id((unsigned char)s[we])) we++;
    char* words = (char*)malloc(8192);
    if (!words) return 0;
    file_words(s, ws, we, words, 8192);
    int n = offer_words(out, 0, max, words, s + ws, plen);
    free(words);
    for (int i = 0; i < svc->ci.nclasses; i++)
        n = offer(out, n, max, svc->ci.cls[i].name, (int)strlen(svc->ci.cls[i].name), s + ws, plen);
    return offer_global(out, n, max, s + ws, plen);
}

/* ---------- signature ---------- */
static const char* type_name(int t) {
    switch (t) {
    case T_VOID: return "void"; case T_INT: return "int"; case T_FLOAT: return "float"; case T_BOOL: return "bool";
    case T_STRING: return "string"; case T_CHAR: return "char"; case T_LIST: return "List"; case T_STRARR: return "string[]";
    case T_ARRAY: return "array"; case T_DICT: return "Dictionary"; case T_FUNC: return "Func"; case T_CORO: return "IEnumerator";
    default: return "object";
    }
}

/* "void WriteLine(object)" from the built-in table: the overload with more parameters than 'arg' (the fewest
 * such), and "(+N)" when there are other overloads. self = 1 for string methods (a[0] is the string). */
static int native_sig(const char* cls, const char* name, int arg, char* sig, int cap) {
    int best = -1, count = 0, self = cls[0] == '.';
    for (int i = 0; i < ms_nnatives; i++) {
        const NativeDef* d = &ms_natives[i];
        if (strcmp(d->cls, cls) || strcmp(d->name, name)) continue;
        count++;
        int np = d->np - self;
        if (np > arg && (best < 0 || np < ms_natives[best].np - self)) best = i;
        if (best < 0 && np == 0 && arg == 0) best = i;
    }
    if (!count) return 0;
    if (best < 0)                                      /* more arguments than any overload: show the longest */
        for (int i = 0; i < ms_nnatives; i++)
            if (!strcmp(ms_natives[i].cls, cls) && !strcmp(ms_natives[i].name, name) && (best < 0 || ms_natives[i].np > ms_natives[best].np)) best = i;
    const NativeDef* d = &ms_natives[best];
    int n = snprintf(sig, cap, "%s %s(", type_name(d->ret), name);
    for (int p = self; p < d->np && n < cap; p++) n += snprintf(sig + n, cap - n, "%s%s", p > self ? ", " : "", type_name(d->params[p]));
    if (n < cap) n += snprintf(sig + n, cap - n, ")");
    if (count > 1 && n < cap) snprintf(sig + n, cap - n, " (+%d)", count - 1);
    return 1;
}

int svc_signature(MsService* svc, int file, int cursor, char* sig, int cap, int* arg) {
    *arg = 0;
    if (!svc || file < 0 || file >= svc->n) return 0;
    const char* s = svc->texts[file];
    int len = (int)strlen(s);
    if (cursor > len) cursor = len;
    /* forward to the cursor: a stack of open brackets, commas counted for each '(' */
    enum { DEPTH = 64 };
    int open[DEPTH], commas[DEPTH], sp = 0;
    for (int p = 0; p < cursor; ) {
        char c = s[p];
        if (c == '/' && s[p + 1] == '/') { while (p < cursor && s[p] != '\n') p++; continue; }
        if (c == '/' && s[p + 1] == '*') { const char* e = strstr(s + p + 2, "*/"); p = e ? (int)(e - s) + 2 : cursor; continue; }
        if (c == '"' || c == '\'') { p = skip_literal(s, p, cursor); continue; }
        if (c == '(' || c == '[' || c == '{') { if (sp < DEPTH) { open[sp] = p; commas[sp] = 0; } sp++; }
        else if (c == ')' || c == ']' || c == '}') { if (sp > 0) sp--; }
        else if (c == ',' && sp > 0 && sp <= DEPTH && s[open[sp - 1]] == '(') commas[sp - 1]++;
        p++;
    }
    if (sp <= 0 || sp > DEPTH || s[open[sp - 1]] != '(') return 0;
    int paren = open[sp - 1];
    *arg = commas[sp - 1];
    int ne = paren;
    while (ne > 0 && (s[ne - 1] == ' ' || s[ne - 1] == '\t')) ne--;
    int ns = ne;
    while (ns > 0 && is_id((unsigned char)s[ns - 1])) ns--;
    if (ns == ne || isdigit((unsigned char)s[ns])) return 0;
    char name[SVC_WORD];
    snprintf(name, sizeof name, "%.*s", ne - ns, s + ns);
    if (IN(COLOR_KEYWORDS, name, ne - ns)) return 0;   /* if (, while (, for ( ... */
    const char* found = NULL;
    if (ns > 0 && s[ns - 1] == '.') {
        char type[48];
        int os = ns - 1;
        while (os > 0 && is_id((unsigned char)s[os - 1])) os--;
        if (ci_type_before(&svc->ci, file, ns - 1, type, sizeof type)) {
            found = ci_signature(&svc->ci, type, name);
            if (!found) {
                char cls[48];
                snprintf(cls, sizeof cls, "%s", !strcmp(type, "string") ? ".string" : type);
                return native_sig(cls, name, *arg, sig, cap);
            }
        } else {                                       /* "Console.WriteLine": a built-in class by its name */
            char cls[48];
            snprintf(cls, sizeof cls, "%.*s", ns - 1 - os, s + os);
            return native_sig(cls, name, *arg, sig, cap);
        }
    } else {
        const char* cls = ci_class_at(&svc->ci, file, ns);
        if (cls) found = ci_signature(&svc->ci, cls, name);
    }
    if (!found) return 0;
    snprintf(sig, cap, "%s", found);
    return 1;
}
