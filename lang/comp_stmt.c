/* MiniSharp compiler - statements. */
#include "compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ================= statements ================= */


static void cond_expr(C* c, Expr* e, const char* what) {
    Type* t = compile_expr(c, e);
    need_bool(c, t, e->line, what);
}

void expr_stmt(C* c, Expr* e) {
    if (e->k == E_INCDEC) e->prefix = 1;       /* value unused: x++ behaves like ++x */
    if (e->k != E_ASSIGN && e->k != E_INCDEC && e->k != E_CALL && e->k != E_NEW)
        cfail(c, e->line, "only assignments, calls, ++/-- and 'new' can be used as a statement");
    c->assignUnused = e->k == E_ASSIGN && e->op == TK_ASSIGN;
    Type* t = compile_expr(c, e);
    c->assignUnused = 0;
    if (t->k != T_VOID) emit_pop(c);
}

/* REPL: 'var x = 5;' at the top of the input makes a static field that the next inputs still see.
 * Declaring it again with the same type just assigns it. */
static void repl_var(C* c, Stmt* s) {
    for (; s; s = s->els) {
        c->line = s->line;
        CField* f = find_field(c, c->cur, s->name, s->nlen);
        Local* rv = NULL;
        for (int i = 0; i < c->nreplVars; i++)
            if (nameeq(c->replVars[i].name, c->replVars[i].nlen, s->name, s->nlen)) rv = &c->replVars[i];
        Type* old = f ? f->type : rv ? rv->type : NULL;
        Type* t;
        if (s->tn) {
            t = resolve_typenode(c, s->tn, 0);
            if (old && old != t) cfail(c, s->line, "'%.*s' is already a %s", s->nlen, s->name, type_name(c, old));
            if (s->e) expr_to(c, s->e, t, s->line, "variable initializer");
            else push_default(c, t);
        } else {
            t = compile_expr(c, s->e);
            if (t->k == T_VOID) cfail(c, s->line, "cannot assign a void value to a variable");
            if (t->k == T_NULL) cfail(c, s->line, "cannot use 'var' with null; write the type");
            copy_struct(c, s->e, t);
            if (old) { coerce(c, t, old, s->line, "assignment"); t = old; }
        }
        c->line = s->line;
        int slot;
        if (f) slot = f->slot;
        else if (rv) slot = rv->slot;
        else {
            if (c->nreplVars >= 64) cfail(c, s->line, "too many variables in one line");
            int used = (int)strlen(c->replDecls);
            int n = snprintf(c->replDecls + used, c->replDeclsCap - used, "static %s %.*s; ", type_name(c, t), s->nlen, s->name);
            if (n >= c->replDeclsCap - used) cfail(c, s->line, "too many variables (restart the terminal)");
            Program* P = c->P;
            Value* d = (Value*)realloc(P->staticDefaults, sizeof(Value) * (P->nstatics + 1));
            if (!d) cfail(c, s->line, "out of memory");
            P->staticDefaults = d;
            slot = P->nstatics++;
            d[slot] = default_value(t);
            Local* lo = &c->replVars[c->nreplVars++];
            lo->name = s->name; lo->nlen = s->nlen; lo->type = t; lo->slot = slot;
        }
        emit_op1(c, OP_SETSTATIC, slot);
        emit_pop(c);
    }
}

/* REPL: an expression on its own is printed, like in C# Interactive */
static void repl_expr(C* c, Expr* e) {
    if (e->k == E_ASSIGN || e->k == E_INCDEC) { expr_stmt(c, e); return; }
    Type* t = compile_expr(c, e);
    if (type_isobj(t) || t->k == T_ENUM) emit_tostr(c, t, 0, e->line);
    if (t->k != T_VOID) emit_op2(c, OP_CALLN, c->replPrint, 1);
}

static void push_loop(C* c, Loop* l, int contTarget) { memset(l, 0, sizeof *l); l->contTarget = contTarget; l->prev = c->loop; c->loop = l; }
static void pop_loop(C* c) {
    Loop* l = c->loop;
    for (int i = 0; i < l->nb; i++) patch_jump(c, l->breaks[i]);
    c->loop = l->prev;
}

void compile_stmt(C* c, Stmt* s) {
    c->line = s->line;
    switch (s->k) {
        case S_BLOCK: {
            int nl = c->nlocals, ns = c->nextSlot;
            for (int i = 0; i < s->n; i++) {
                Stmt* t = s->list[i];
                if (s == c->replBody && t->k == S_VAR) repl_var(c, t);
                else if (s == c->replBody && t->k == S_EXPR) { c->line = t->line; repl_expr(c, t->e); }
                else compile_stmt(c, t);
            }
            c->nlocals = nl; c->nextSlot = ns;
            return;
        }
        case S_VAR: {
          for (; s; s = s->els) {
            c->line = s->line;
            Type* t;
            if (s->tn) {
                t = resolve_typenode(c, s->tn, 0);
                if (s->e) expr_to(c, s->e, t, s->line, "variable initializer");
                else push_default(c, t);
            } else {
                t = compile_expr(c, s->e);
                if (t->k == T_VOID) cfail(c, s->line, "cannot assign a void value to a variable");
                if (t->k == T_NULL) cfail(c, s->line, "cannot use 'var' with null; write the type");
                copy_struct(c, s->e, t);
            }
            c->line = s->line;
            int slot = declare_local(c, s->name, s->nlen, t, s->line);
            init_local(c, slot, c->lastBoxed);
          }
          return;
        }
        case S_EXPR: expr_stmt(c, s->e); return;
        case S_IF: {
            cond_expr(c, s->e, "'if' condition");
            int jz = emit_jump(c, OP_JZ);
            compile_stmt(c, s->body);
            if (s->els) {
                int je = emit_jump(c, OP_JMP);
                patch_jump(c, jz);
                compile_stmt(c, s->els);
                patch_jump(c, je);
            } else patch_jump(c, jz);
            return;
        }
        case S_WHILE: {
            int start = code_label(c);
            cond_expr(c, s->e, "'while' condition");
            int jz = emit_jump(c, OP_JZ);
            Loop l; push_loop(c, &l, start);
            compile_stmt(c, s->body);
            emit_jump_back(c, OP_JMP, start);
            patch_jump(c, jz);
            pop_loop(c);
            return;
        }
        case S_DO: {
            int start = code_label(c);
            Loop l; push_loop(c, &l, -1);
            compile_stmt(c, s->body);
            for (int i = 0; i < l.nc; i++) patch_jump(c, l.conts[i]);
            cond_expr(c, s->e, "'while' condition");
            emit_op(c, OP_NOT);
            emit_jump_back(c, OP_JZ, start);
            pop_loop(c);
            return;
        }
        case S_FOR: {
            int nl = c->nlocals, ns = c->nextSlot;
            if (s->init) {
                if (s->init->k == S_EXPR) expr_stmt(c, s->init->e);
                else compile_stmt(c, s->init);
            }
            int start = code_label(c);
            int jz = -1;
            if (s->e) { cond_expr(c, s->e, "'for' condition"); jz = emit_jump(c, OP_JZ); }
            Loop l; push_loop(c, &l, -1);
            compile_stmt(c, s->body);
            for (int i = 0; i < l.nc; i++) patch_jump(c, l.conts[i]);
            if (s->step) expr_stmt(c, s->step);
            emit_jump_back(c, OP_JMP, start);
            if (jz >= 0) patch_jump(c, jz);
            pop_loop(c);
            c->nlocals = nl; c->nextSlot = ns;
            return;
        }
        case S_YIELDBREAK:
            if (!c->isIter) cfail(c, s->line, "'yield' can only be used in a method that returns IEnumerator");
            emit_op(c, OP_RET);
            return;
        case S_YIELD: {
            if (!c->isIter) cfail(c, s->line, "'yield' can only be used in a method that returns IEnumerator");
            Expr* y = s->e;
            if (y->k == E_NULL || y->k == E_INT) { emit_op1(c, OP_YIELD, Y_NEXT); return; }     /* yield return null / 0: next frame */
            if (y->k == E_NEW && y->tn->base == TK_IDENT) {
                if (namecs(y->tn->name, y->tn->nlen, "WaitForSeconds") || namecs(y->tn->name, y->tn->nlen, "WaitForSecondsRealtime")) {
                    Type* pf = &tFloat;
                    args_for(c, y, &pf, 1, "'WaitForSeconds'");
                    c->line = s->line;
                    emit_op1(c, OP_YIELD, Y_SECONDS);
                    return;
                }
                if (namecs(y->tn->name, y->tn->nlen, "WaitForEndOfFrame")) {
                    args_for(c, y, NULL, 0, "'WaitForEndOfFrame'");
                    emit_op1(c, OP_YIELD, Y_NEXT);
                    return;
                }
            }
            Type* t = compile_expr(c, y);
            c->line = s->line;
            if (t->k != T_CORO) cfail(c, s->line, "'yield return' takes null, new WaitForSeconds(s) or a coroutine, not %s", type_name(c, t));
            emit_op1(c, OP_YIELD, Y_CORO);       /* wait until that coroutine ends (it is started if needed) */
            return;
        }
        case S_RETURN:
            if (c->isIter) cfail(c, s->line, "a coroutine cannot use 'return'; use 'yield break' to stop it");
            if (c->isCtor) {
                if (s->e) cfail(c, s->line, "a constructor cannot return a value");
                emit_op1(c, OP_LOAD, 0); emit_op(c, OP_RETV);
            } else if (c->retType->k == T_VOID) {
                if (s->e) cfail(c, s->line, "a void method cannot return a value");
                emit_op(c, OP_RET);
            } else {
                if (!s->e) cfail(c, s->line, "this method must return a %s", type_name(c, c->retType));
                expr_to(c, s->e, c->retType, s->line, "return value");
                emit_op(c, OP_RETV);
            }
            return;
        case S_BREAK:
            if (!c->loop) cfail(c, s->line, "'break' outside of a loop");
            if (c->loop->nb >= 128) cfail(c, s->line, "too many 'break' statements in one loop");
            c->loop->breaks[c->loop->nb++] = emit_jump(c, OP_JMP);
            return;
        case S_CONTINUE: {
            Loop* l = c->loop;
            while (l && l->isSwitch) l = l->prev;
            if (!l) cfail(c, s->line, "'continue' outside of a loop");
            if (l->contTarget >= 0) emit_jump_back(c, OP_JMP, l->contTarget);
            else {
                if (l->nc >= 128) cfail(c, s->line, "too many 'continue' statements in one loop");
                l->conts[l->nc++] = emit_jump(c, OP_JMP);
            }
            return;
        }
        case S_FOREACH: {
            /* for (int i = 0; i < coll.Length; i++) { T x = coll[i]; body }  over a copy of the reference */
            int nl = c->nlocals, ns = c->nextSlot;
            Type* ct = compile_expr(c, s->e);
            c->line = s->line;
            if (ct->k == T_DICT) {
                /* for (i = next(0); i >= 0; i = next(i + 1)) { kv = entry i } ; kv takes two slots: key, value */
                if (s->tn && resolve_typenode(c, s->tn, 0) != ct->kvp)
                    cfail(c, s->line, "'foreach' over %s gives %s; write 'var'", type_name(c, ct), type_name(c, ct->kvp));
                int coll = alloc_temp(c), idx = alloc_temp(c);
                emit_op1(c, OP_STORE, coll); emit_pop(c);
                emit_op1(c, OP_ICONST, 0); emit_op1(c, OP_STORE, idx); emit_pop(c);
                int kslot = declare_local(c, s->name, s->nlen, ct->kvp, s->line);
                c->locals[c->nlocals - 1].boxed = 0;          /* two slots; a lambda cannot use it */
                alloc_temp(c);                                  /* kslot + 1: the value */
                int start = code_label(c);
                emit_op1(c, OP_LOAD, coll); emit_op1(c, OP_LOAD, idx); emit_op1(c, OP_DICT, D_NEXT);
                emit_op1(c, OP_STORE, idx); emit_op1(c, OP_ICONST, 0); emit_op(c, OP_IGE);
                int jz = emit_jump(c, OP_JZ);
                emit_op1(c, OP_LOAD, coll); emit_op1(c, OP_LOAD, idx); emit_op1(c, OP_DICT, D_KEYAT); emit_op1(c, OP_STORE, kslot); emit_pop(c);
                emit_op1(c, OP_LOAD, coll); emit_op1(c, OP_LOAD, idx); emit_op1(c, OP_DICT, D_VALAT); emit_op1(c, OP_STORE, kslot + 1); emit_pop(c);
                Loop l; push_loop(c, &l, -1);
                compile_stmt(c, s->body);
                for (int i = 0; i < l.nc; i++) patch_jump(c, l.conts[i]);
                c->line = s->line;
                emit_op1(c, OP_LOAD, idx); emit_op1(c, OP_ICONST, 1); emit_op(c, OP_IADD); emit_op1(c, OP_STORE, idx); emit_pop(c);
                emit_jump_back(c, OP_JMP, start);
                patch_jump(c, jz);
                pop_loop(c);
                c->nlocals = nl; c->nextSlot = ns;
                return;
            }
            Type* el = ct->k == T_ARRAY || ct->k == T_LIST ? ct->elem : ct->k == T_STRING ? &tChar : NULL;
            if (!el) cfail(c, s->line, "'foreach' needs an array, a List, a Dictionary or a string, not %s", type_name(c, ct));
            Type* vt = s->tn ? resolve_typenode(c, s->tn, 0) : el;
            if (!assignable(el, vt)) cfail(c, s->line, "'foreach': cannot convert %s to %s", type_name(c, el), type_name(c, vt));
            int coll = alloc_temp(c), idx = alloc_temp(c);
            emit_op1(c, OP_STORE, coll); emit_pop(c);
            emit_op1(c, OP_ICONST, 0); emit_op1(c, OP_STORE, idx); emit_pop(c);
            int var = declare_local(c, s->name, s->nlen, vt, s->line), varBoxed = c->lastBoxed;
            int start = code_label(c);
            emit_op1(c, OP_LOAD, idx); emit_op1(c, OP_LOAD, coll); emit_op(c, OP_ARRLEN); emit_op(c, OP_ILT);
            int jz = emit_jump(c, OP_JZ);
            emit_op1(c, OP_LOAD, coll); emit_op1(c, OP_LOAD, idx); emit_op(c, OP_GETELEM);
            coerce(c, el, vt, s->line, "'foreach' variable");
            if (vt->k == T_STRUCT) emit_op(c, OP_COPY);
            init_local(c, var, varBoxed);                   /* a new variable each time round, as in C# */
            Loop l; push_loop(c, &l, -1);
            compile_stmt(c, s->body);
            for (int i = 0; i < l.nc; i++) patch_jump(c, l.conts[i]);
            c->line = s->line;
            emit_op1(c, OP_LOAD, idx); emit_op1(c, OP_ICONST, 1); emit_op(c, OP_IADD); emit_op1(c, OP_STORE, idx); emit_pop(c);
            emit_jump_back(c, OP_JMP, start);
            patch_jump(c, jz);
            pop_loop(c);
            c->nlocals = nl; c->nextSlot = ns;
            return;
        }
        case S_SWITCH: {
            int ns = c->nextSlot;
            Type* st = compile_expr(c, s->e);
            c->line = s->line;
            if (!type_isint(st) && st->k != T_STRING && st->k != T_BOOL && st->k != T_ENUM)
                cfail(c, s->line, "'switch' works on int, char, string, bool or an enum, not %s", type_name(c, st));
            int tmp = alloc_temp(c);
            emit_op1(c, OP_STORE, tmp); emit_pop(c);
            /* tests first: one jump per label to its section */
            int** jumps = (int**)arena_alloc(&c->A, sizeof(int*) * (s->n ? s->n : 1));
            int def = -1;
            for (int i = 0; i < s->n; i++) {
                Stmt* sec = s->list[i];
                if (sec->isDefault) { if (def >= 0) cfail(c, sec->line, "'switch' has two 'default' labels"); def = i; }
                jumps[i] = (int*)arena_alloc(&c->A, sizeof(int) * (sec->nlabels ? sec->nlabels : 1));
                for (int j = 0; j < sec->nlabels; j++) {
                    c->line = sec->labels[j]->line;
                    emit_op1(c, OP_LOAD, tmp);
                    compare_op(c, TK_EQ, st, compile_expr(c, sec->labels[j]), sec->labels[j]->line);
                    emit_op(c, OP_NOT);
                    jumps[i][j] = emit_jump(c, OP_JZ);
                }
            }
            int jdef = emit_jump(c, OP_JMP);
            Loop l; push_loop(c, &l, -1); l.isSwitch = 1;
            for (int i = 0; i < s->n; i++) {
                Stmt* sec = s->list[i];
                for (int j = 0; j < sec->nlabels; j++) patch_jump(c, jumps[i][j]);
                if (i == def) patch_jump(c, jdef);
                int nl = c->nlocals, ns2 = c->nextSlot;
                for (int k = 0; k < sec->n; k++) compile_stmt(c, sec->list[k]);
                c->nlocals = nl; c->nextSlot = ns2;
                /* no fall-through: the end of a section leaves the switch (C# requires 'break' there anyway) */
                if (l.nb >= 128) cfail(c, sec->line, "too many 'case' sections in one switch");
                l.breaks[l.nb++] = emit_jump(c, OP_JMP);
            }
            if (def < 0) {
                if (l.nb >= 128) cfail(c, s->line, "too many 'case' sections in one switch");
                l.breaks[l.nb++] = jdef;
            }
            pop_loop(c);
            c->nextSlot = ns;
            return;
        }
    }
}

/* ================= reachability ================= */
/* Like C#: a method that returns a value must not reach its closing '}'. can_end tells whether the end of a statement
   can be reached once the statement is; a reachable 'break' / 'continue' is noted in Flow for the loop or switch around it.
   Only the literals true/false count as constant conditions (while (true), for (;;), if (false)...). */
typedef struct { int brk, cont; } Flow;
static int is_true(Expr* e)  { return !e || (e->k == E_BOOL && e->ival); }     /* for (;;): no condition = true */
static int is_false(Expr* e) { return e && e->k == E_BOOL && !e->ival; }

static int can_end(Stmt* s, Flow* f) {
    if (!s) return 1;
    switch (s->k) {
        case S_BLOCK:
            for (int i = 0; i < s->n; i++) if (!can_end(s->list[i], f)) return 0;  /* the rest cannot be reached */
            return 1;
        case S_RETURN: case S_YIELDBREAK: return 0;
        case S_BREAK: f->brk = 1; return 0;
        case S_CONTINUE: f->cont = 1; return 0;
        case S_IF: {
            int t = is_false(s->e) ? 0 : can_end(s->body, f);
            int e = is_true(s->e) ? 0 : can_end(s->els, f);   /* no 'else': its end is reached when the condition is false */
            return t || e;
        }
        case S_WHILE: case S_FOR: {
            if (is_false(s->e)) return 1;
            Flow in = {0, 0};
            can_end(s->body, &in);
            return in.brk || !is_true(s->e);
        }
        case S_DO: {
            Flow in = {0, 0};
            int b = can_end(s->body, &in);
            return in.brk || ((b || in.cont) && !is_true(s->e));
        }
        case S_FOREACH: { Flow in = {0, 0}; can_end(s->body, &in); return 1; }
        case S_SWITCH: {
            Flow in = {0, 0};
            int end = 0, def = 0;
            for (int i = 0; i < s->n; i++) {
                Stmt* sec = s->list[i];
                if (sec->isDefault) def = 1;
                int r = 1;
                for (int k = 0; k < sec->n && r; k++) r = can_end(sec->list[k], &in);
                if (r) end = 1;                 /* the end of a section leaves the switch */
            }
            if (in.cont) f->cont = 1;           /* 'continue' in a switch belongs to the loop around it */
            return end || in.brk || !def;
        }
        default: return 1;                      /* S_VAR, S_EXPR, S_YIELD */
    }
}

int stmt_can_end(Stmt* s) { Flow f = {0, 0}; return can_end(s, &f); }
