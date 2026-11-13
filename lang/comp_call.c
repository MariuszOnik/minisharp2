/* MiniSharp compiler - calls: methods, overloads, built-in functions, List / Dictionary methods, delegates. */
#include "compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ---------- calls ---------- */
void args_for(C* c, Expr* e, Type** params, int np, const char* what) { args_for_ro(c, e, params, np, what, 0); }
void args_for_ro(C* c, Expr* e, Type** params, int np, const char* what, uint32_t ro) {
    if (e->nargs != np) cfail(c, e->line, "%s takes %d argument%s, but %d were given", what, np, np == 1 ? "" : "s", e->nargs);
    for (int i = 0; i < np; i++) {
        char buf[120]; snprintf(buf, sizeof buf, "argument %d of %s", i + 1, what);
        if (params[i]->k == T_FUNC || (params[i]->k == T_CLASS && e->args[i]->k == E_COND)) { expr_to(c, e->args[i], params[i], e->args[i]->line, buf); c->line = e->line; continue; }
        Type* t = compile_expr(c, e->args[i]);
        c->line = e->line;
        if (params[i]->k == T_ANY && (type_isobj(t) || t->k == T_ENUM)) { emit_tostr(c, t, 0, e->line); t = &tString; }   /* may call an override of ToString */
        coerce(c, t, params[i], e->args[i]->line, buf);
        if (!(i < 32 && (ro >> i & 1))) copy_struct(c, e->args[i], params[i]);
    }
}

/* arguments of a call to method or constructor m; the ones left out get the default values of their parameters */
static int nreq(CMethod* m) { return m->defs ? m->nreq : m->np; }
void args_for_m(C* c, Expr* e, CMethod* m, const char* what, uint32_t ro) {
    int req = nreq(m);
    if (req == m->np || e->nargs > m->np) { args_for_ro(c, e, m->params, m->np, what, ro); return; }
    if (e->nargs < req)
        cfail(c, e->line, "%s takes %d to %d arguments, but %d %s given", what, req, m->np, e->nargs, e->nargs == 1 ? "was" : "were");
    args_for_ro(c, e, m->params, e->nargs, what, ro);
    for (int i = e->nargs; i < m->np; i++) {
        Expr* d = m->defs[i];
        Type* pt = m->params[i];
        if (pt->k == T_ENUM) emit_op1(c, OP_ICONST, d->ival);
        else if (d->k == E_INT && pt->k == T_FLOAT) emit_fconst(c, (float)d->ival);
        else expr_to(c, d, pt, e->line, "default value");
    }
    c->line = e->line;
}

/* ---------- overloads ---------- */
int same_params(CMethod* a, CMethod* b) {
    if (a->np != b->np) return 0;
    for (int i = 0; i < a->np; i++) if (a->params[i] != b->params[i]) return 0;
    return 1;
}

/* every method named n that an object of class cls has: its own first, then its bases' (an override or a method with
   the same parameters in a derived class hides the base one) */
int method_cands(C* c, int cls, const char* n, int l, CMethod** out, int k, int max) {
    for (int ci = cls; ci >= 0; ci = c->classes[ci].base) {
        CClass* kk = &c->classes[ci];
        for (int i = 0; i < kk->nm; i++) {
            CMethod* m = &kk->methods[i];
            if (!nameeq(m->name, m->nlen, n, l)) continue;
            int hidden = 0;
            for (int j = 0; j < k && !hidden; j++) hidden = same_params(out[j], m);
            if (!hidden && k < max) out[k++] = m;
        }
    }
    if (c->classes[cls].isInterface)
        for (int i = 0; i < c->classes[cls].nifaces; i++) k = method_cands(c, c->classes[cls].ifaces[i], n, l, out, k, max);
    return k;
}

/* keeps the methods of cand[0..k) that the code being compiled may call (C# picks an overload only among those);
   when none is left, the error is about the first one */
int accessible_cands(C* c, CMethod** cand, int k, int line) {
    int n = 0;
    for (int i = 0; i < k; i++) if (can_access(c, cand[i]->owner, method_access(cand[i]))) cand[n++] = cand[i];
    if (k && !n) {
        CMethod* m = cand[0];
        CClass* o = &c->classes[m->owner];
        if (m->decl && m->decl->isCtor) {
            if (method_access(m) == AC_PRIVATE) cfail(c, line, "the constructor of '%.*s' is private; only code inside class %.*s can use 'new %.*s(...)'",
                                                      o->nlen, o->name, o->nlen, o->name, o->nlen, o->name);
            cfail(c, line, "the constructor of '%.*s' is protected; only %.*s and classes derived from it can use it", o->nlen, o->name, o->nlen, o->name);
        }
        check_access(c, m->owner, method_access(m), m->name, m->nlen, line);
    }
    return n;
}

/* is e the name of a method used as a value (a delegate)? */
static int is_method_group(C* c, Expr* e) {
    if (e->k == E_NAME) return resolve_name(c, e->name, e->nlen).k == R_METHOD;
    if (e->k != E_MEMBER || e->prefix || e->a->k == E_BASE) return 0;
    int cls = -1;
    fold_ns(c, e);
    if (e->a->k == E_NAME) {
        Res r = resolve_name(c, e->a->name, e->a->nlen);
        if (r.k == R_BUILTIN) return 0;
        if (r.k == R_CLASS) cls = r.idx;
    }
    if (cls < 0) { Type* t = peek_type(c, e->a); if (!type_isobj(t)) return 0; cls = t->cls; }
    return !find_field(c, cls, e->name, e->nlen) && !find_prop(c, cls, e->name, e->nlen) && find_method(c, cls, e->name, e->nlen);
}

/* Picks one of n methods with the same name for the arguments of call e: by the number of arguments, then the
   one whose parameter types match best. With only one left it is taken without looking at the arguments, so
   its own error messages stay as clear as for a method without overloads. */
CMethod* pick_overload(C* c, CMethod** cand, int n, Expr* e, const char* what) {
    if (n == 1) return cand[0];
    CMethod* m[64]; int nc = 0;
    for (int i = 0; i < n; i++) if (nreq(cand[i]) <= e->nargs && e->nargs <= cand[i]->np && nc < 64) m[nc++] = cand[i];
    if (nc == 1) return m[0];
    const char* vof = strncmp(what, "constructor", 11) ? "version of " : "";
    if (nc == 0) cfail(c, e->line, "no %s%s takes %d argument%s", vof, what, e->nargs, e->nargs == 1 ? "" : "s");
    if (e->nargs > 32) cfail(c, e->line, "too many arguments");
    Type* at[32]; int special[32];            /* special: 1 lambda, 2 method name */
    for (int i = 0; i < e->nargs; i++) {
        Expr* a = e->args[i];
        special[i] = a->k == E_LAMBDA ? 1 : is_method_group(c, a) ? 2 : 0;
        at[i] = special[i] ? NULL : peek_type(c, a);
    }
    int best = -1, bestScore = -1;      /* equal score: the one with fewer default values used wins (as in C#) */
    for (int k = 0; k < nc; k++) {
        int ok = 1, score = 0;
        for (int i = 0; i < e->nargs && ok; i++) {
            Type* pt = m[k]->params[i];
            if (special[i] == 1) ok = pt->k == T_FUNC && pt->np == e->args[i]->nparams;
            else if (special[i] == 2) ok = pt->k == T_FUNC;
            else if (at[i] == pt) score++;
            else ok = assignable(at[i], pt);
        }
        if (ok && (score > bestScore || (score == bestScore && m[k]->np < m[best]->np))) { best = k; bestScore = score; }
    }
    if (best < 0) {
        char list[160]; int len = 0; list[0] = 0;
        for (int i = 0; i < e->nargs && len < 140; i++)
            len += snprintf(list + len, sizeof list - len, "%s%s", i ? ", " : "", special[i] == 1 ? "lambda" : special[i] == 2 ? "method" : type_name(c, at[i]));
        cfail(c, e->line, "no %s%s takes (%s)", vof, what, list);
    }
    return m[best];
}

/* the method named n of class cls that call e means, or NULL if there is no method with that name */
static CMethod* find_call(C* c, int cls, const char* n, int l, Expr* e) {
    CMethod* cand[64];
    int k = method_cands(c, cls, n, l, cand, 0, 64);
    if (!k) return NULL;
    k = accessible_cands(c, cand, k, e->line);
    char what[100];
    snprintf(what, sizeof what, "'%.*s.%.*s'", c->classes[cls].nlen, c->classes[cls].name, l, n);
    return pick_overload(c, cand, k, e, what);
}

/* virt: call through the vtable of the object (virtual and interface methods); otherwise this exact method */
static Type* call_method(C* c, Expr* e, CMethod* m, int hasThis, int virt) {
    char what[100];
    CClass* owner = &c->classes[m->owner];
    snprintf(what, sizeof what, "'%.*s.%.*s'", owner->nlen, owner->name, m->nlen, m->name);
    args_for_m(c, e, m, what, virt && m->sel >= 0 ? 0 : m->roParams);   /* an override may change it */
    c->line = e->line;
    if (virt && m->sel >= 0) emit_op2(c, OP_CALLV, m->sel, m->np + 1);     /* the VM looks up the function (or coroutine) */
    else {
        if (m->func < 0) cfail(c, e->line, "'%.*s' is abstract and has no body to call", m->nlen, m->name);
        emit_op2(c, m->isIter ? OP_CORO_NEW : OP_CALL, m->func, m->np + (hasThis ? 1 : 0));
    }
    return m->ret;
}

/* Built-in method from the native table. With 'self' the receiver is already on the stack and
 * the table lists it as the first parameter (class names starting with '.', e.g. ".string"). */
static Type* call_native(C* c, Expr* e, const char* cls, int clen, int self) {
    Expr* f = e->a;
    /* overloads: same name, pick by argument count, then exact types, then convertible types */
    Type* at[MS_MAXNATIVEARGS];
    if (e->nargs > MS_MAXNATIVEARGS) cfail(c, e->line, "too many arguments for '%.*s'", f->nlen, f->name);
    for (int i = 0; i < e->nargs; i++) at[i] = peek_type(c, e->args[i]);
    int ni = -1, any = -1, sameCount = -1;
    for (int pass = 0; pass < 2 && ni < 0; pass++)
        for (int i = 0; i < ms_nnatives; i++) {
            const NativeDef* d = &ms_natives[i];
            if (!namecs(cls, clen, d->cls) || !namecs(f->name, f->nlen, d->name)) continue;
            if (any < 0) any = i;
            if (d->np == e->nargs + self && sameCount < 0) sameCount = i;
            if (d->np != e->nargs + self) continue;
            int ok = 1;
            for (int q = 0; q < e->nargs && ok; q++) {
                Type* pt = code_type(c, d->params[q + self]);
                ok = pass == 0 ? (at[q] == pt || (pt->k == T_ANY && at[q]->k != T_VOID) || (at[q]->k == T_NULL && pt->k == T_STRING))
                               : assignable(at[q], pt);
            }
            if (ok) { ni = i; break; }
        }
    const char* shown = self ? cls + 1 : cls; int slen = self ? clen - 1 : clen;
    if (any < 0) cfail(c, e->line, "'%.*s' has no method '%.*s'", slen, shown, f->nlen, f->name);
    if (ni < 0) ni = sameCount >= 0 ? sameCount : any;   /* report the mismatch against the closest overload */
    const NativeDef* nd = &ms_natives[ni];
    Type* params[MS_MAXNATIVEARGS];
    for (int i = self; i < nd->np; i++) params[i - self] = code_type(c, nd->params[i]);
    char what[80]; snprintf(what, sizeof what, "'%.*s.%s'", slen, shown, nd->name);
    args_for(c, e, params, nd->np - self, what);
    c->line = e->line;
    emit_op2(c, OP_CALLN, ni, nd->np);
    return code_type(c, nd->ret);
}

/* list.Add(x) etc.; the list is already on the stack (the VM checks it for null) */
static Type* call_list(C* c, Expr* e, Type* lt) {
    Expr* f = e->a;
    Type* el = lt->elem;
    Type* pInsert[2] = { &tInt, el };
    char what[80]; snprintf(what, sizeof what, "'%s.%.*s'", type_name(c, lt), f->nlen, f->name);
    #define IS(n) namecs(f->name, f->nlen, n)
    if (IS("Add")) { args_for(c, e, &el, 1, what); emit_op1(c, OP_LIST, L_ADD); return &tVoid; }
    if (IS("Insert")) { args_for(c, e, pInsert, 2, what); emit_op1(c, OP_LIST, L_INSERT); return &tVoid; }
    if (IS("RemoveAt")) { args_for(c, e, pInsert, 1, what); emit_op1(c, OP_LIST, L_REMOVEAT); return &tVoid; }
    if (IS("Remove")) { args_for(c, e, &el, 1, what); emit_op1(c, OP_LIST, L_REMOVE); return &tBool; }
    if (IS("Clear")) { args_for(c, e, NULL, 0, what); emit_op1(c, OP_LIST, L_CLEAR); return &tVoid; }
    if (IS("AddRange")) {
        if (e->nargs != 1) cfail(c, e->line, "%s takes 1 argument", what);
        Type* st = compile_expr(c, e->args[0]);
        if (!((st->k == T_LIST || st->k == T_ARRAY) && st->elem == el))
            cfail(c, e->line, "%s needs a %s or %s[], not %s", what, type_name(c, lt), type_name(c, el), type_name(c, st));
        c->line = e->line;
        emit_op1(c, OP_LIST, L_ADDRANGE);
        return &tVoid;
    }
    if (IS("IndexOf")) { args_for(c, e, &el, 1, what); emit_op1(c, OP_LIST, L_INDEXOF); return &tInt; }
    /* with a delegate: Find(x => ...), Sort((a, b) => ...) */
    Type* pred = func_of(c, &el, 1, &tBool);
    Type* act = func_of(c, &el, 1, &tVoid);
    Type* two[2] = { el, el };
    Type* cmp = func_of(c, two, 2, &tInt);
    struct { const char* name; int sub; Type** param; Type* ret; } lf[] = {
        { "Find", LF_FIND, &pred, el }, { "FindLast", LF_FINDLAST, &pred, el }, { "FindAll", LF_FINDALL, &pred, lt },
        { "FindIndex", LF_FINDINDEX, &pred, &tInt }, { "FindLastIndex", LF_FINDLASTINDEX, &pred, &tInt },
        { "Exists", LF_EXISTS, &pred, &tBool }, { "TrueForAll", LF_TRUEFORALL, &pred, &tBool },
        { "RemoveAll", LF_REMOVEALL, &pred, &tInt }, { "ForEach", LF_FOREACH, &act, &tVoid },
    };
    for (size_t i = 0; i < sizeof lf / sizeof lf[0]; i++)
        if (IS(lf[i].name)) {
            args_for(c, e, lf[i].param, 1, what);
            emit_op1(c, OP_LISTF, lf[i].sub | default_kind(el) << 8);
            if (lf[i].ret == el && el->k == T_STRUCT) emit_op(c, OP_COPY);     /* a method's result is a new value */
            return lf[i].ret;
        }
    if (IS("Sort")) {
        if (e->nargs == 0) {
            if (!type_isnum(el) && el->k != T_STRING && el->k != T_ENUM)
                cfail(c, e->line, "%s can only sort numbers, chars and strings by itself; give it a comparison: Sort((a, b) => ...)", what);
            emit_op1(c, OP_LIST, L_SORT);
            return &tVoid;
        }
        args_for(c, e, &cmp, 1, what);
        emit_op1(c, OP_LISTF, LF_SORT);
        return &tVoid;
    }
    if (IS("Reverse")) { args_for(c, e, NULL, 0, what); emit_op1(c, OP_LIST, L_REVERSE); return &tVoid; }
    if (IS("Contains")) {
        args_for(c, e, &el, 1, what);
        emit_op1(c, OP_LIST, L_INDEXOF); emit_op1(c, OP_ICONST, 0); emit_op(c, OP_IGE);
        return &tBool;
    }
    #undef IS
    cfail(c, e->line, "List has no method '%.*s' (there is Add, AddRange, Insert, Remove, RemoveAt, RemoveAll, Clear, Contains, IndexOf, "
          "Find, FindLast, FindAll, FindIndex, FindLastIndex, Exists, TrueForAll, ForEach, Sort, Reverse, Count)", f->nlen, f->name);
}

/* stores the value on top of the stack into an 'out' argument and pops it */
static void store_out(C* c, Expr* o, Type* vt) {
    if (o->k != E_OUT) cfail(c, o->line, "this argument must be 'out var name'");
    if (o->prefix) {
        Type* t = o->tn ? resolve_typenode(c, o->tn, 0) : vt;
        coerce(c, vt, t, o->line, "'out' variable");
        if (t->k == T_STRUCT) emit_op(c, OP_COPY);
        int slot = declare_local(c, o->name, o->nlen, t, o->line);
        if (c->lastBoxed) emit_op(c, OP_BOX);
        emit_op1(c, OP_STORE, slot);
    } else {
        if (o->a->k != E_NAME) cfail(c, o->line, "'out' works with a variable name");
        Res r = resolve_name(c, o->a->name, o->a->nlen);
        if (r.k == R_CELL || r.k == R_UPVAL) cfail(c, o->line, "'out %.*s': a variable used by a lambda cannot be an 'out' argument", o->a->nlen, o->a->name);
        if (r.k != R_LOCAL && r.k != R_STATIC) cfail(c, o->line, "'out' works with a local variable or a static field");
        coerce(c, vt, r.type, o->line, "'out' argument");
        if (vt->k == T_STRUCT) emit_op(c, OP_COPY);
        emit_op1(c, r.k == R_LOCAL ? OP_STORE : OP_SETSTATIC, r.idx);
    }
    emit_pop(c);
}

/* d.Add(k, v) etc.; the dictionary is already on the stack (the VM checks it for null) */
static Type* call_dict(C* c, Expr* e, Type* dt) {
    Expr* f = e->a;
    Type* kv[2] = { dt->key, dt->elem };
    char what[96]; snprintf(what, sizeof what, "'%s.%.*s'", type_name(c, dt), f->nlen, f->name);
    #define IS(n) namecs(f->name, f->nlen, n)
    if (IS("Add")) { args_for(c, e, kv, 2, what); emit_op1(c, OP_DICT, D_ADD); return &tVoid; }
    if (IS("Remove")) { args_for(c, e, kv, 1, what); emit_op1(c, OP_DICT, D_REMOVE); return &tBool; }
    if (IS("ContainsKey")) { args_for(c, e, kv, 1, what); emit_op1(c, OP_DICT, D_CONTAINS); return &tBool; }
    if (IS("ContainsValue")) { args_for(c, e, &kv[1], 1, what); emit_op1(c, OP_DICT, D_CONTAINSVAL); return &tBool; }
    if (IS("Clear")) { args_for(c, e, NULL, 0, what); emit_op1(c, OP_DICT, D_CLEAR); return &tVoid; }
    if (IS("TryGetValue")) {
        if (e->nargs != 2) cfail(c, e->line, "%s takes 2 arguments: key and 'out var value'", what);
        expr_to(c, e->args[0], dt->key, e->args[0]->line, "argument 1 of TryGetValue");
        c->line = e->line;
        emit_op1(c, OP_DICT, D_TRYGET | default_kind(dt->elem) << 8);
        store_out(c, e->args[1], dt->elem);
        return &tBool;
    }
    #undef IS
    cfail(c, e->line, "Dictionary has no method '%.*s' (there is Add, Remove, ContainsKey, ContainsValue, TryGetValue, Clear, Count, Keys, Values)", f->nlen, f->name);
}

/* f(args): the delegate is already on the stack */
static Type* invoke(C* c, Expr* e, Type* ft) {
    char what[110]; snprintf(what, sizeof what, "the %s", type_name(c, ft));
    args_for(c, e, ft->params, ft->np, what);
    c->line = e->line;
    emit_op2(c, OP_CALLF, ft->np, ft->elem->k != T_VOID);
    return ft->elem;
}

Type* compile_call(C* c, Expr* e) {
    Expr* f = e->a;
    if (f->k == E_MEMBER && f->prefix) {        /* a?.M(): nothing is called when a is null */
        Type* t = compile_expr(c, f->a);
        c->line = e->line;
        if (!type_isref(t) || t->k == T_NULL) cfail(c, e->line, "'?.' needs something that can be null, not %s", type_name(c, t));
        int j = emit_jump(c, OP_JNULL);
        Expr st = *f; st.k = E_STACK; c->stackType = t;
        Expr mf = *f; mf.prefix = 0; mf.a = &st;
        Expr ce = *e; ce.a = &mf;
        Type* rt = compile_call(c, &ce);
        if (rt->k == T_VOID) c->fn->code[j - 1] = OP_JNULL_POP;
        else if (!type_isref(rt)) cfail(c, e->line, "'?.%.*s()' gives %s or null, and there is no %s? here; add a value for null: ?.%.*s() ?\? 0",
                                   f->nlen, f->name, type_name(c, rt), type_name(c, rt), f->nlen, f->name);
        patch_jump(c, j);
        return rt;
    }
    if (f->k == E_NAME) {
        Res rv = resolve_name(c, f->name, f->nlen);
        if (rv.type && rv.type->k == T_FUNC && rv.k != R_METHOD) { compile_expr(c, f); return invoke(c, e, rv.type); }
        CMethod* m = find_call(c, c->cur, f->name, f->nlen, e);
        if (!m && (namecs(f->name, f->nlen, "StartCoroutine") || namecs(f->name, f->nlen, "StopCoroutine"))) {
            int start = namecs(f->name, f->nlen, "StartCoroutine");
            Type* pt = &tCoro;
            args_for(c, e, &pt, 1, start ? "'StartCoroutine'" : "'StopCoroutine'");
            c->line = e->line;
            emit_op1(c, OP_CORO, start ? C_START : C_STOP);
            return start ? &tCoro : &tVoid;
        }
        if (!m && namecs(f->name, f->nlen, "StopAllCoroutines")) {
            args_for(c, e, NULL, 0, "'StopAllCoroutines'");
            emit_op1(c, OP_CORO, C_STOPALL);
            return &tVoid;
        }
        if (!m) {
            Res r = resolve_name(c, f->name, f->nlen);
            if (r.k != R_NONE) cfail(c, e->line, "'%.*s' is not a method", f->nlen, f->name);
            cfail(c, e->line, "unknown method '%.*s'", f->nlen, f->name);
        }
        if (!m->isStatic) {
            if (c->curStatic) cfail(c, e->line, "cannot call instance method '%.*s' from a static method", f->nlen, f->name);
            emit_op1(c, OP_LOAD, 0);
        }
        return call_method(c, e, m, !m->isStatic, 1);
    }
    if (f->k == E_MEMBER) {
        if (f->a->k == E_BASE) {               /* base.Method(): the base class's version, not the override */
            compile_expr(c, f->a);
            int b = c->classes[c->cur].base;
            CMethod* m = find_call(c, b, f->name, f->nlen, e);
            if (!m && namecs(f->name, f->nlen, "ToString") && e->nargs == 0) { emit_op2(c, OP_TOSTR, 4, 0); return &tString; }
            if (!m) cfail(c, e->line, "base class '%.*s' has no method '%.*s'", c->classes[b].nlen, c->classes[b].name, f->nlen, f->name);
            if (m->isStatic) { emit_op(c, OP_POP); return call_method(c, e, m, 0, 0); }
            return call_method(c, e, m, 1, 0);
        }
        fold_ns(c, f);
        if (f->a->k == E_NAME) {
            Res r = resolve_name(c, f->a->name, f->a->nlen);
            if (r.k == R_CLASS) {
                if (c->classes[r.idx].isEnum) cfail(c, e->line, "enum '%.*s' has no methods", f->a->nlen, f->a->name);
                CMethod* m = find_call(c, r.idx, f->name, f->nlen, e);
                CField* df = m ? NULL : use_field(c, find_field(c, r.idx, f->name, f->nlen), e->line);
                CProp* dp = m || df ? NULL : find_prop(c, r.idx, f->name, f->nlen);
                if ((df && df->type->k == T_FUNC) || (dp && dp->type->k == T_FUNC)) { Type* ft = compile_expr(c, f); return invoke(c, e, ft); }      /* Game.onWin() */
                if (!m) cfail(c, e->line, "class '%.*s' has no method '%.*s'", f->a->nlen, f->a->name, f->nlen, f->name);
                if (!m->isStatic) cfail(c, e->line, "'%.*s' is not static; call it on an object", f->nlen, f->name);
                return call_method(c, e, m, 0, 0);
            }
            if (r.k == R_BUILTIN) return call_native(c, e, f->a->name, f->a->nlen, 0);
        }
        Type* t = compile_expr(c, f->a);
        c->line = e->line;
        if (namecs(f->name, f->nlen, "ToString") && e->nargs == 0 && t->k != T_VOID) {
            if (t->k == T_STRING) emit_op1(c, OP_NULLCHK, 0);
            emit_tostr(c, t, 0, e->line);
            return &tString;
        }
        if (t->k == T_STRING) {
            emit_op1(c, OP_NULLCHK, 0);
            return call_native(c, e, ".string", 7, 1);
        }
        if (t->k == T_LIST) return call_list(c, e, t);
        if (t->k == T_DICT) return call_dict(c, e, t);
        if (t->k == T_FUNC && namecs(f->name, f->nlen, "Invoke")) return invoke(c, e, t);
        if (type_isobj(t)) {
            CMethod* m = find_call(c, t->cls, f->name, f->nlen, e);
            CField* df = m ? NULL : use_field(c, find_field(c, t->cls, f->name, f->nlen), e->line);
            CProp* dp = m || df ? NULL : find_prop(c, t->cls, f->name, f->nlen);
            if (df && df->type->k == T_FUNC && !df->isStatic) { emit_op1(c, OP_GETFIELD, df->slot); return invoke(c, e, df->type); }
            if (dp && dp->type->k == T_FUNC && !dp->isStatic) { emit_get(c, dp, f->a->k != E_BASE, e->line); return invoke(c, e, dp->type); }
            if (!m) cfail(c, e->line, "%s has no method '%.*s'", type_name(c, t), f->nlen, f->name);
            if (m->isStatic) cfail(c, e->line, "'%.*s' is static; call it as %s.%.*s()", f->nlen, f->name, type_name(c, t), f->nlen, f->name);
            if (m->sel < 0) emit_op1(c, OP_NULLCHK, 0);     /* OP_CALLV checks it itself */
            return call_method(c, e, m, 1, 1);
        }
        cfail(c, e->line, "%s has no method '%.*s'", type_name(c, t), f->nlen, f->name);
    }
    if (f->k != E_NAME && f->k != E_MEMBER) {                      /* GetAction()(), actions[i](1) */
        Type* t = compile_expr(c, f);
        if (t->k == T_FUNC) return invoke(c, e, t);
    }
    cfail(c, e->line, "this cannot be called like a method");
}
