/* MiniSharp compiler - lambdas, captured variables and method groups. */
#include "compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ================= lambdas ================= */

/* Before a method is compiled: which names do its lambdas use from outside themselves? Locals with
 * those names are boxed (kept in cells), so the method and the lambdas share them. It goes by name,
 * so it may box a variable that did not need it; that only costs a little speed. */
static void nl_add(C* c, NameList* L, const char* n, int l) {
    for (int i = 0; i < L->n; i++) if (nameeq(L->v[i].n, L->v[i].l, n, l)) return;
    L->v = (Name*)arena_grow(&c->A, L->v, L->n, &L->cap, sizeof(Name));
    L->v[L->n].n = n; L->v[L->n].l = l; L->n++;
}
static int nl_has(NameList* L, const char* n, int l) {
    for (int i = 0; i < L->n; i++) if (nameeq(L->v[i].n, L->v[i].l, n, l)) return 1;
    return 0;
}
static void scan_stmt(C* c, Stmt* s, NameList* uses, NameList* decls);
static void scan_expr(C* c, Expr* e, NameList* uses, NameList* decls) {
    if (!e) return;
    if (e->k == E_LAMBDA) {
        NameList u = { 0 }, d = { 0 };
        if (e->body) scan_stmt(c, e->body, &u, &d); else scan_expr(c, e->a, &u, &d);
        for (int i = 0; i < u.n; i++) {
            int own = nl_has(&d, u.v[i].n, u.v[i].l);
            for (int q = 0; q < e->nparams && !own; q++) own = nameeq(e->params[q].name, e->params[q].nlen, u.v[i].n, u.v[i].l);
            if (!own) nl_add(c, &c->caps, u.v[i].n, u.v[i].l);
            nl_add(c, uses, u.v[i].n, u.v[i].l);
        }
        return;
    }
    if (e->k == E_NAME) nl_add(c, uses, e->name, e->nlen);
    if ((e->k == E_OUT && e->prefix) || (e->k == E_IS && e->name)) nl_add(c, decls, e->name, e->nlen);
    scan_expr(c, e->a, uses, decls); scan_expr(c, e->b, uses, decls); scan_expr(c, e->c, uses, decls);
    for (int i = 0; i < e->nargs; i++) scan_expr(c, e->args[i], uses, decls);
}
static void scan_stmt(C* c, Stmt* s, NameList* uses, NameList* decls) {
    if (!s) return;
    if (s->k == S_VAR) {
        for (; s; s = s->els) { nl_add(c, decls, s->name, s->nlen); scan_expr(c, s->e, uses, decls); }
        return;
    }
    if (s->k == S_FOREACH) nl_add(c, decls, s->name, s->nlen);
    scan_expr(c, s->e, uses, decls); scan_expr(c, s->step, uses, decls);
    for (int i = 0; i < s->nlabels; i++) scan_expr(c, s->labels[i], uses, decls);
    for (int i = 0; i < s->n; i++) scan_stmt(c, s->list[i], uses, decls);
    scan_stmt(c, s->body, uses, decls); scan_stmt(c, s->els, uses, decls); scan_stmt(c, s->init, uses, decls);
}
void scan_captures(C* c, Stmt* body, Expr* extra) {
    NameList u = { 0 }, d = { 0 };
    c->caps.n = 0;
    scan_stmt(c, body, &u, &d);
    scan_expr(c, extra, &u, &d);
}

static void emit_closure(C* c, int kind, int idx, int nup) { emit_op(c, OP_CLOSURE); emit1(c, kind); emit1(c, idx); emit1(c, nup); }

/* the overload of method n that fits delegate type ft (or the first one, whose mismatch is then reported) */
static CMethod* group_method(C* c, int cls, const char* n, int l, Type* ft) {
    CMethod* cand[64];
    int k = method_cands(c, cls, n, l, cand, 0, 64);
    for (int i = 0; i < k; i++) {
        int ok = cand[i]->np == ft->np && cand[i]->ret == ft->elem;
        for (int j = 0; ok && j < ft->np; j++) ok = cand[i]->params[j] == ft->params[j];
        if (ok) return cand[i];
    }
    return k ? cand[0] : NULL;
}

/* Func<int, int> f = Square;  Action a = enemy.Die;  -> a delegate of that method. 0 = e is not a method name */
static int method_group(C* c, Expr* e, Type* ft, int line) {
    CMethod* m = NULL;
    int self = 0;                               /* 0 static method, 1 this, 2 the object of e->a */
    if (e->k == E_NAME) {
        if (resolve_name(c, e->name, e->nlen).k != R_METHOD) return 0;
        m = group_method(c, c->cur, e->name, e->nlen, ft);
        if (!m->isStatic) {
            if (c->curStatic) cfail(c, line, "cannot use instance method '%.*s' in a static method", e->nlen, e->name);
            self = 1;
        }
    } else if (e->k == E_MEMBER) {
        if (e->a->k == E_BASE) return 0;
        fold_ns(c, e);
        if (e->a->k == E_NAME) {
            Res r = resolve_name(c, e->a->name, e->a->nlen);
            if (r.k == R_CLASS) {
                if (find_field(c, r.idx, e->name, e->nlen) || find_prop(c, r.idx, e->name, e->nlen)) return 0;
                m = group_method(c, r.idx, e->name, e->nlen, ft);
                if (!m) return 0;
                if (!m->isStatic) cfail(c, line, "'%.*s' is not static; use an object: obj.%.*s", e->nlen, e->name, e->nlen, e->name);
            }
            if (r.k == R_BUILTIN) cfail(c, line, "built-in methods like '%.*s.%.*s' cannot be used as a delegate yet; wrap it in a lambda",
                                       e->a->nlen, e->a->name, e->nlen, e->name);
        }
        if (!m) {
            Type* t = peek_type(c, e->a);
            if (!type_isobj(t) || find_field(c, t->cls, e->name, e->nlen) || find_prop(c, t->cls, e->name, e->nlen)) return 0;
            m = group_method(c, t->cls, e->name, e->nlen, ft);
            if (!m) return 0;
            if (m->isStatic) cfail(c, line, "'%.*s' is static; use %s.%.*s", e->nlen, e->name, type_name(c, t), e->nlen, e->name);
            self = 2;
        }
    } else return 0;
    int ok = m->np == ft->np && m->ret == ft->elem;
    for (int i = 0; ok && i < m->np; i++) ok = m->params[i] == ft->params[i];
    if (!ok) cfail(c, line, "method '%.*s' does not fit %s (it needs the same parameter types and return type)", m->nlen, m->name, type_name(c, ft));
    c->line = line;
    if (!self) { emit_op(c, OP_NULL); emit_closure(c, 1, m->func, 0); return 1; }
    if (self == 1) emit_op1(c, OP_LOAD, 0);
    else { compile_expr(c, e->a); c->line = line; emit_op1(c, OP_NULLCHK, 0); }
    if (m->sel >= 0) emit_closure(c, 2, m->sel, 0);
    else emit_closure(c, 0, m->func, 0);
    return 1;
}

/* A lambda becomes a function of its own: slot 0 = the object of the method around it (or null),
 * then the parameters. Variables from outside are reached through the cells the delegate captured. */
static void compile_lambda(C* c, Expr* e, Type* ft) {
    Program* P = c->P;
    if (e->nparams != ft->np)
        cfail(c, e->line, "this lambda has %d parameter%s, but %s needs %d", e->nparams, e->nparams == 1 ? "" : "s", type_name(c, ft), ft->np);
    if (P->nfuncs >= P->capfuncs) cfail(c, e->line, "internal error: too many lambdas");
    int fi = P->nfuncs++;
    /* the function around it, put back at the end */
    Func* sfn = c->fn; int sCtor = c->isCtor, sIter = c->isIter, sOp = c->lastOp, sLabel = c->lastLabel, sSlot = c->nextSlot, sLine = c->line;
    Type* sRet = c->retType; Loop* sLoop = c->loop;
    int baseLocals = c->nlocals;
    LamCtx lc; lc.depth = c->fdepth + 1; lc.nup = 0; lc.parent = c->lam;
    c->lam = &lc; c->fdepth++;
    c->fn = &P->funcs[fi];
    snprintf(c->fn->name, sizeof c->fn->name, "%.50s.lambda", sfn->name);
    c->fn->nparams = ft->np + 1; c->fn->nlocals = 1;
    c->nextSlot = 1; c->loop = NULL; c->lastOp = c->prevOp = c->prevOp2 = -1; c->lastLabel = -1;
    c->isCtor = 0; c->isIter = 0; c->retType = ft->elem;
    for (int i = 0; i < e->nparams; i++) {
        Param* pa = &e->params[i];
        if (pa->tn && resolve_typenode(c, pa->tn, 0) != ft->params[i])
            cfail(c, e->line, "parameter '%.*s' of this lambda must be %s", pa->nlen, pa->name, type_name(c, ft->params[i]));
        if (find_local(c, pa->name, pa->nlen) >= 0)
            cfail(c, e->line, "a variable named '%.*s' already exists here; give the lambda parameter another name", pa->nlen, pa->name);
        declare_local(c, pa->name, pa->nlen, ft->params[i], e->line);
    }
    box_params(c);
    if (e->body) {
        compile_stmt(c, e->body);
        c->line = e->line;
        emit_op(c, ft->elem->k == T_VOID ? OP_RET : OP_NORET);
    } else if (ft->elem->k == T_VOID) {
        expr_stmt(c, e->a);
        emit_op(c, OP_RET);
    } else {
        expr_to(c, e->a, ft->elem, e->a->line, "the lambda's result");
        emit_op(c, OP_RETV);
    }
    c->nlocals = baseLocals; c->fdepth--; c->lam = lc.parent;
    c->fn = sfn; c->isCtor = sCtor; c->isIter = sIter; c->lastOp = sOp; c->prevOp = c->prevOp2 = -1; c->lastLabel = sLabel; c->nextSlot = sSlot;
    c->retType = sRet; c->loop = sLoop; c->line = sLine;
    /* in the function around it: self, the captured cells, then the delegate */
    if (c->curStatic) emit_op(c, OP_NULL); else emit_op1(c, OP_LOAD, 0);
    for (int k = 0; k < lc.nup; k++) {
        if (lc.ups[k].from >= 0) emit_op1(c, OP_UPVAL, lc.ups[k].from);
        else emit_op1(c, OP_LOAD, c->locals[lc.ups[k].li].slot);
    }
    emit_closure(c, 0, fi, lc.nup);
}

/* an expression converted to type t; lambdas and method names need t to know what they are */
void expr_to(C* c, Expr* e, Type* t, int line, const char* what) {
    if (t->k == T_FUNC && e->k == E_LAMBDA) { compile_lambda(c, e, t); return; }
    if (t->k == T_FUNC && method_group(c, e, t, line)) return;
    if (e->k == E_LAMBDA) cfail(c, line, "%s: a lambda cannot become %s", what, type_name(c, t));
    if (e->k == E_COND && (t->k == T_CLASS || t->k == T_FUNC)) {   /* c ? new Bat() : new Rock() given to a Wrog, as in C# 9 */
        need_bool(c, compile_expr(c, e->a), e->line, "condition of '?:'");
        c->line = e->line;
        int jz = emit_jump(c, OP_JZ);
        expr_to(c, e->b, t, line, what);
        int je = emit_jump(c, OP_JMP);
        patch_jump(c, jz);
        expr_to(c, e->c, t, line, what);
        patch_jump(c, je);
        return;
    }
    coerce(c, compile_expr(c, e), t, line, what);
    copy_struct(c, e, t);
}
