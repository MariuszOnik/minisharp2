/* Unit test: the AST printer (lang/printer.c). For every .cs file in tests/ and in tests/errors/ (if it parses), every file of
 * tests/projects/ and every ```cs example of sdk/docs: printing the tree and parsing the text again gives the same
 * tree (lines and spans ignored), and printing that tree gives the same text (the style is stable).
 * Plus a few small cases with the exact text expected. Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "ast.h"
#include "printer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static int fails, oks;
static void check(int cond, const char* what) {
    if (cond) oks++; else { fails++; printf("FAIL: %s\n", what); }
}

/* ---------- structural equality ---------- */
static int eq_name(const char* a, int al, const char* b, int bl) { return al == bl && (al == 0 || !memcmp(a, b, (size_t)al)); }
static int eq_stmt(Stmt* a, Stmt* b);

static int eq_type(TypeNode* a, TypeNode* b) {
    if (!a || !b) return a == b;
    if (a->base != b->base || a->dims != b->dims || a->rect != b->rect || a->nargs != b->nargs) return 0;
    if (!eq_name(a->name, a->nlen, b->name, b->nlen)) return 0;
    for (int i = 0; i < a->nargs; i++) if (!eq_type(a->args[i], b->args[i])) return 0;
    return 1;
}

static int eq_params(Param* a, int na, Param* b, int nb) {
    if (na != nb) return 0;
    for (int i = 0; i < na; i++)
        if (!eq_type(a[i].tn, b[i].tn) || !eq_name(a[i].name, a[i].nlen, b[i].name, b[i].nlen)) return 0;
    return 1;
}

static int eq_expr(Expr* a, Expr* b) {
    if (!a || !b) return a == b;
    if (a->k != b->k || a->op != b->op || a->ival != b->ival || memcmp(&a->fval, &b->fval, sizeof a->fval)) return 0;
    if (a->prefix != b->prefix || a->nargs != b->nargs) return 0;
    if (!eq_name(a->name, a->nlen, b->name, b->nlen)) return 0;
    if (!eq_expr(a->a, b->a) || !eq_expr(a->b, b->b) || !eq_expr(a->c, b->c)) return 0;
    for (int i = 0; i < a->nargs; i++) if (!eq_expr(a->args[i], b->args[i])) return 0;
    return eq_type(a->tn, b->tn) && eq_params(a->params, a->nparams, b->params, b->nparams) && eq_stmt(a->body, b->body);
}

static int eq_stmt(Stmt* a, Stmt* b) {
    if (!a || !b) return a == b;
    if (a->k != b->k || a->n != b->n || a->nlabels != b->nlabels || a->isDefault != b->isDefault) return 0;
    if (!eq_name(a->name, a->nlen, b->name, b->nlen) || !eq_type(a->tn, b->tn)) return 0;
    if (!eq_expr(a->e, b->e) || !eq_expr(a->step, b->step)) return 0;
    if (!eq_stmt(a->body, b->body) || !eq_stmt(a->els, b->els) || !eq_stmt(a->init, b->init)) return 0;
    for (int i = 0; i < a->n; i++) if (!eq_stmt(a->list[i], b->list[i])) return 0;
    for (int i = 0; i < a->nlabels; i++) if (!eq_expr(a->labels[i], b->labels[i])) return 0;
    return 1;
}

static int eq_method(MethodDecl* a, MethodDecl* b) {
    return a->isStatic == b->isStatic && a->isCtor == b->isCtor && a->isIter == b->isIter && a->kind == b->kind &&
           a->access == b->access && a->exprBody == b->exprBody && eq_type(a->ret, b->ret) &&
           eq_name(a->name, a->nlen, b->name, b->nlen) && eq_params(a->params, a->nparams, b->params, b->nparams) &&
           eq_stmt(a->body, b->body) && eq_expr(a->baseCall, b->baseCall) && a->thisCall == b->thisCall;
}

static int eq_prop(PropDecl* a, PropDecl* b) {
    return a->isStatic == b->isStatic && a->kind == b->kind && a->hasGet == b->hasGet && a->hasSet == b->hasSet &&
           a->access == b->access && a->getAccess == b->getAccess && a->setAccess == b->setAccess &&
           a->getExpr == b->getExpr && a->setExpr == b->setExpr && a->arrow == b->arrow && eq_type(a->tn, b->tn) &&
           eq_name(a->name, a->nlen, b->name, b->nlen) && eq_stmt(a->getBody, b->getBody) && eq_stmt(a->setBody, b->setBody) &&
           eq_expr(a->init, b->init);
}

static int eq_field(FieldDecl* a, FieldDecl* b) {
    return a->isStatic == b->isStatic && a->attrs == b->attrs && a->readOnly == b->readOnly && a->access == b->access &&
           eq_type(a->tn, b->tn) && eq_name(a->name, a->nlen, b->name, b->nlen) && eq_expr(a->init, b->init);
}

static int eq_class(ClassDecl* a, ClassDecl* b) {
    if (!eq_name(a->name, a->nlen, b->name, b->nlen) || a->isAbstract != b->isAbstract || a->isInterface != b->isInterface) return 0;
    if (a->order != b->order || a->isStatic != b->isStatic) return 0;
    if (a->isEnum != b->isEnum || a->isStruct != b->isStruct || a->isFlags != b->isFlags) return 0;
    if (!eq_name(a->ns.s, a->ns.l, b->ns.s, b->ns.l) || a->nusings != b->nusings) return 0;
    for (int i = 0; i < a->nusings; i++) if (!eq_name(a->usings[i].s, a->usings[i].l, b->usings[i].s, b->usings[i].l)) return 0;
    if (a->nbases != b->nbases || a->nfields != b->nfields || a->nmethods != b->nmethods || a->nprops != b->nprops) return 0;
    for (int i = 0; i < a->nbases; i++) if (!eq_type(a->bases[i], b->bases[i])) return 0;
    for (int i = 0; i < a->nfields; i++) if (!eq_field(&a->fields[i], &b->fields[i])) return 0;
    for (int i = 0; i < a->nmethods; i++) if (!eq_method(&a->methods[i], &b->methods[i])) return 0;
    for (int i = 0; i < a->nprops; i++) if (!eq_prop(&a->props[i], &b->props[i])) return 0;
    return 1;
}

/* ---------- round trip ---------- */
static int files;

/* mustParse = 0: a file that does not parse is skipped (tests/errors/ has syntax errors on purpose) */
static void round_trip(const char* name, const char* src, int mustParse) {
    Arena A = { 0 }, B = { 0 };
    char err[256], msg[512];
    ProgramAST* pr = ms_parse_text(&A, src, err, sizeof err);
    if (!pr) {
        if (mustParse) { snprintf(msg, sizeof msg, "%s: %s", name, err); check(0, msg); }
        arena_free(&A);
        return;
    }
    files++;
    MsBuf p1 = { 0 }, p2 = { 0 };
    ms_print_program(&p1, pr);
    ProgramAST* re = ms_parse_text(&B, p1.p ? p1.p : "", err, sizeof err);
    if (!re) { snprintf(msg, sizeof msg, "%s: printed code does not parse: %s", name, err); check(0, msg); }
    else if (re->nclasses != pr->nclasses) { snprintf(msg, sizeof msg, "%s: printed code has %d classes, not %d", name, re->nclasses, pr->nclasses); check(0, msg); }
    else {
        int same = 1;
        for (int i = 0; i < pr->nclasses; i++)
            if (!eq_class(&re->classes[i], &pr->classes[i])) {
                snprintf(msg, sizeof msg, "%s: printed class '%.*s' parses to a different tree", name, pr->classes[i].nlen, pr->classes[i].name);
                check(0, msg); same = 0;
            }
        if (same) {
            ms_print_program(&p2, re);
            snprintf(msg, sizeof msg, "%s: printing the printed code changes it", name);
            check(p1.p && p2.p && !strcmp(p1.p, p2.p), msg);
        }
    }
    ms_buf_free(&p1); ms_buf_free(&p2);
    arena_free(&A); arena_free(&B);
}

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* s = (char*)malloc((size_t)n + 1);
    if (!s || fread(s, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(s); return NULL; }
    s[n] = 0; fclose(f);
    return s;
}

static int ends_with(const char* s, const char* end) {
    size_t a = strlen(s), b = strlen(end);
    return a >= b && !strcmp(s + a - b, end);
}

/* every *.cs of dir (recursive = one level of subfolders, for tests/projects/<name>/) */
static void each_cs(const char* dir, int mustParse, int recursive) {
    DIR* d = opendir(dir);
    if (!d) { char m[300]; snprintf(m, sizeof m, "cannot open %s", dir); check(0, m); return; }
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char path[512];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (ends_with(e->d_name, ".cs")) {
            char* src = read_file(path);
            if (src) { round_trip(path, src, mustParse); free(src); }
        } else if (recursive) each_cs(path, mustParse, 0);
    }
    closedir(d);
}

/* the ```cs blocks of the docs chapters */
static void each_doc_example(const char* dir) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (!ends_with(e->d_name, ".md")) continue;
        char path[512];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        char* md = read_file(path);
        if (!md) continue;
        int n = 0;
        for (char* p = md; (p = strstr(p, "```cs")); ) {
            char* body = strchr(p, '\n');
            if (!body) break;
            body++;
            char* end = strstr(body, "\n```");
            if (!end) break;
            *end = 0;
            char name[600]; snprintf(name, sizeof name, "%s example %d", path, ++n);
            round_trip(name, body, 1);
            p = end + 1;
        }
        free(md);
    }
    closedir(d);
}

/* small cases with the exact text: the style and the parts the old printer did not know */
static void exact(const char* src, const char* want) {
    char err[256];
    char* got = ms_format(src, err, sizeof err);
    if (!got) { char m[400]; snprintf(m, sizeof m, "format: %s", err); check(0, m); return; }
    if (strcmp(got, want)) printf("FAIL: format gave:\n%s---- instead of:\n%s----\n", got, want), fails++;
    else oks++;
    free(got);
}

int main(void) {
    exact("class P{public int A{get;private set;}private static int n=1;const int Max=5;static readonly int R=2;"
          "protected virtual void F(){}}",
          "class P {\n"
          "    private static int n = 1;\n"
          "    const int Max = 5;\n"
          "    static readonly int R = 2;\n"
          "\n"
          "    public int A { get; private set; }\n"
          "\n"
          "    protected virtual void F() { }\n"
          "}\n");
    exact("using System;using Lib;static class U{internal static int Twice(int x)=>x*2;}"
          "namespace Game{using Extra;class A{} class B{}} namespace Other{enum E{X,Y=3}}",
          "using System;\n"
          "using Lib;\n"
          "\n"
          "static class U {\n"
          "    internal static int Twice(int x) => x * 2;\n"
          "}\n"
          "\n"
          "namespace Game {\n"
          "    using Extra;\n"
          "\n"
          "    class A {\n"
          "    }\n"
          "\n"
          "    class B {\n"
          "    }\n"
          "}\n"
          "\n"
          "namespace Other {\n"
          "    enum E { X, Y = 3 }\n"
          "}\n");
    exact("class P{static void Main(int y){int x=-(-y);if(x>0)x++;else{x--;}}}",
          "class P {\n"
          "    static void Main(int y) {\n"
          "        int x = -(-y);\n"
          "        if (x > 0)\n"
          "            x++;\n"
          "        else {\n"
          "            x--;\n"
          "        }\n"
          "    }\n"
          "}\n");
    { char err[256]; char* r = ms_format("class {", err, sizeof err); check(!r && strstr(err, "line 1"), "syntax error gives NULL and the line"); free(r); }

    each_cs("tests", 1, 0);
    each_cs("tests/errors", 0, 0);
    each_cs("tests/projects", 0, 1);            /* some projects are broken on purpose */
    each_doc_example("sdk/docs");
    check(files > 50, "round trip found the test files (run from the repository root)");
    if (fails) printf("printer: %d failed, %d ok (%d files)\n", fails, oks, files);
    return fails ? 1 : 0;
}
