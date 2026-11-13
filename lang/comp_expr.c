/* MiniSharp compiler - expressions: conversions, operators, l-values and the main expression switch. */
#include "compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ================= expressions ================= */

void coerce(C* c, Type* from, Type* to, int line, const char* what) {
    if (!assignable(from, to)) {
        if (from->k == T_VOID) cfail(c, line, "%s: this expression has no value (void)", what);
        if (from->k == T_FLOAT && to->k == T_INT) cfail(c, line, "%s: cannot convert float to int automatically, use (int)", what);
        if (from->k == T_ENUM && type_isnum(to)) cfail(c, line, "%s: cannot convert %s to %s automatically, use (%s)", what, type_name(c, from), type_name(c, to), type_name(c, to));
        if (type_isint(from) && to->k == T_ENUM) cfail(c, line, "%s: cannot convert %s to %s automatically, use (%s)", what, type_name(c, from), type_name(c, to), type_name(c, to));
        if (from->k == T_CLASS && to->k == T_CLASS && to->isa[from->cls])
            cfail(c, line, "%s: not every %s is a %s; use a cast (%s)x, or 'x as %s' / 'x is %s y'", what,
                  type_name(c, from), type_name(c, to), type_name(c, to), type_name(c, to), type_name(c, to));
        cfail(c, line, "%s: cannot convert %s to %s", what, type_name(c, from), type_name(c, to));
    }
    if ((from->k == T_INT || from->k == T_CHAR) && to->k == T_FLOAT) emit_op1(c, OP_I2F, 0);
    else if (from->k == T_CHAR && to->k == T_INT) emit_op1(c, OP_C2I, 0);
}

Type* peek_type(C* c, Expr* e) {
    Func* f = c->fn;
    int len = f->len, ll = c->lastLabel, ns = c->nextSlot, nl = c->nlocals, line = c->line, nf = c->P->nfuncs;
    Type* t = compile_expr(c, e);
    f->len = len; c->lastOp = c->prevOp = c->prevOp2 = -1; c->lastLabel = ll; c->nextSlot = ns; c->nlocals = nl; c->line = line;
    for (int i = nf; i < c->P->nfuncs; i++) {
        Func* g = &c->P->funcs[i];
        free(g->code); free(g->lines);
        memset(g, 0, sizeof *g);
    }
    c->P->nfuncs = nf;
    return t;
}

void emit_tostr(C* c, Type* t, int depth, int line) {
    if (t->k == T_VOID) cfail(c, line, "cannot use a void value in a string");
    int kind = t->k == T_INT ? 0 : t->k == T_FLOAT ? 1 : t->k == T_BOOL ? 2 : t->k == T_ENUM ? MS_KIND_ENUM + t->cls : 3;
    emit_op2(c, OP_TOSTR, kind, depth);
}

static const char* opname(int tok) {
    switch (tok) {
        case TK_PLUS: return "+"; case TK_MINUS: return "-"; case TK_STAR: return "*";
        case TK_SLASH: return "/"; case TK_PERCENT: return "%"; case TK_LT: return "<";
        case TK_LE: return "<="; case TK_GT: return ">"; case TK_GE: return ">=";
        case TK_EQ: return "=="; case TK_NE: return "!=";
        case TK_AMP: return "&"; case TK_PIPE: return "|"; case TK_CARET: return "^";
        case TK_SHL: return "<<"; case TK_SHR: return ">>";
    }
    return "?";
}

/* both operands already on the stack */
static Type* arith(C* c, int tok, Type* lt, Type* rt, int line) {
    if (tok == TK_PLUS && (lt->k == T_STRING || rt->k == T_STRING)) {
        if (lt->k != T_STRING) emit_tostr(c, lt, 1, line);
        if (rt->k != T_STRING) emit_tostr(c, rt, 0, line);
        emit_op(c, OP_CONCAT);
        return &tString;
    }
    /* enums: e + 1 and e - 1 stay the enum, e1 - e2 is an int (as in C#) */
    if (lt->k == T_ENUM || rt->k == T_ENUM) {
        if (lt->k == T_ENUM && rt->k == T_ENUM && lt == rt && tok == TK_MINUS) { emit_op(c, OP_ISUB); return &tInt; }
        if (lt->k == T_ENUM && type_isint(rt) && (tok == TK_PLUS || tok == TK_MINUS)) {
            if (rt->k == T_CHAR) emit_op1(c, OP_C2I, 0);
            emit_op(c, tok == TK_PLUS ? OP_IADD : OP_ISUB); return lt;
        }
        if (rt->k == T_ENUM && type_isint(lt) && tok == TK_PLUS) { if (lt->k == T_CHAR) emit_op1(c, OP_C2I, 1); emit_op(c, OP_IADD); return rt; }
        cfail(c, line, "operator '%s' cannot be used with %s and %s (use (int) to get the number of an enum value)", opname(tok), type_name(c, lt), type_name(c, rt));
    }
    if (!type_isnum(lt) || !type_isnum(rt))
        cfail(c, line, "operator '%s' cannot be used with %s and %s", opname(tok), type_name(c, lt), type_name(c, rt));
    int f = lt->k == T_FLOAT || rt->k == T_FLOAT;
    if (f) { if (lt->k != T_FLOAT) emit_op1(c, OP_I2F, 1); if (rt->k != T_FLOAT) emit_op1(c, OP_I2F, 0); }
    else if (lt->k == T_CHAR) emit_op1(c, OP_C2I, 1);       /* 'a' + 1 is an int */
    switch (tok) {
        case TK_PLUS: emit_op(c, f ? OP_FADD : OP_IADD); break;
        case TK_MINUS: emit_op(c, f ? OP_FSUB : OP_ISUB); break;
        case TK_STAR: emit_op(c, f ? OP_FMUL : OP_IMUL); break;
        case TK_SLASH: emit_op(c, f ? OP_FDIV : OP_IDIV); break;
        case TK_PERCENT: emit_op(c, f ? OP_FMOD : OP_IMOD); break;
    }
    return f ? &tFloat : &tInt;
}

/* & | ^ on ints or bools, << >> on ints; both operands already on the stack */
static Type* bitwise(C* c, int tok, Type* lt, Type* rt, int line) {
    if (lt->k == T_BOOL && rt->k == T_BOOL && tok != TK_SHL && tok != TK_SHR) {
        emit_op(c, tok == TK_AMP ? OP_IAND : tok == TK_PIPE ? OP_IOR : OP_IXOR);
        return &tBool;
    }
    if (lt->k == T_ENUM && lt == rt && tok != TK_SHL && tok != TK_SHR) {     /* flags: Dir.Up | Dir.Left */
        emit_op(c, tok == TK_AMP ? OP_IAND : tok == TK_PIPE ? OP_IOR : OP_IXOR);
        return lt;
    }
    if (!type_isint(lt) || !type_isint(rt))
        cfail(c, line, "operator '%s' cannot be used with %s and %s", opname(tok), type_name(c, lt), type_name(c, rt));
    if (lt->k == T_CHAR) emit_op1(c, OP_C2I, 1);
    switch (tok) {
        case TK_AMP: emit_op(c, OP_IAND); break;
        case TK_PIPE: emit_op(c, OP_IOR); break;
        case TK_CARET: emit_op(c, OP_IXOR); break;
        case TK_SHL: emit_op(c, OP_ISHL); break;
        case TK_SHR: emit_op(c, OP_ISHR); break;
    }
    return &tInt;
}

static int is_bitwise(int tok) { return tok == TK_AMP || tok == TK_PIPE || tok == TK_CARET || tok == TK_SHL || tok == TK_SHR; }

Type* compare_op(C* c, int tok, Type* lt, Type* rt, int line) {
    if ((type_isnum(lt) && type_isnum(rt)) || (lt->k == T_ENUM && lt == rt)) {
        int f = lt->k == T_FLOAT || rt->k == T_FLOAT;
        if (f) { if (lt->k != T_FLOAT) emit_op1(c, OP_I2F, 1); if (rt->k != T_FLOAT) emit_op1(c, OP_I2F, 0); }
        switch (tok) {
            case TK_LT: emit_op(c, f ? OP_FLT : OP_ILT); break;
            case TK_LE: emit_op(c, f ? OP_FLE : OP_ILE); break;
            case TK_GT: emit_op(c, f ? OP_FGT : OP_IGT); break;
            case TK_GE: emit_op(c, f ? OP_FGE : OP_IGE); break;
            case TK_EQ: emit_op(c, f ? OP_FEQ : OP_IEQ); break;
            case TK_NE: emit_op(c, f ? OP_FNE : OP_INE); break;
        }
        return &tBool;
    }
    if (tok != TK_EQ && tok != TK_NE)
        cfail(c, line, "operator '%s' cannot be used with %s and %s", opname(tok), type_name(c, lt), type_name(c, rt));
    if (lt->k == T_BOOL && rt->k == T_BOOL) { emit_op(c, tok == TK_EQ ? OP_IEQ : OP_INE); return &tBool; }
    if ((lt->k == T_STRING || rt->k == T_STRING) && type_isref(lt) && type_isref(rt) &&
        (lt->k == T_STRING || lt->k == T_NULL) && (rt->k == T_STRING || rt->k == T_NULL)) {
        emit_op(c, tok == TK_EQ ? OP_SEQ : OP_SNE); return &tBool;
    }
    if (type_isref(lt) && type_isref(rt) && (lt == rt || lt->k == T_NULL || rt->k == T_NULL || (lt->k == T_CLASS && rt->k == T_CLASS))) {
        emit_op(c, tok == TK_EQ ? OP_REQ : OP_RNE); return &tBool;
    }
    cfail(c, line, "cannot compare %s with %s", type_name(c, lt), type_name(c, rt));
}

void need_bool(C* c, Type* t, int line, const char* what) {
    if (t->k != T_BOOL) cfail(c, line, "%s must be bool, not %s", what, type_name(c, t));
}

/* const and static readonly fields with a literal value: the value is put in the code instead of reading the field */
int is_const_field(CField* f) {
    if (!f || !f->isStatic || !f->decl || !f->decl->readOnly || !f->decl->init) return 0;
    int k = f->decl->init->k;
    return k == E_INT || k == E_FLOAT || k == E_BOOL || k == E_CHAR || k == E_STRING;
}
static void emit_const_field(C* c, CField* f) {
    Expr* v = f->decl->init;
    if (v->k == E_INT && f->type->k == T_FLOAT) emit_fconst(c, (float)v->ival);
    else expr_to(c, v, f->type, v->line, "constant");
}
static void no_const_write(C* c, CField* f, int line) {
    if (f && f->isStatic && f->decl && f->decl->readOnly)
        cfail(c, line, "'%.*s' is const or readonly, so it cannot be changed", f->nlen, f->name);
}

/* ---------- l-values ---------- */
enum { LV_LOCAL, LV_STATIC, LV_FIELD, LV_ELEM, LV_CELL, LV_PROP };
typedef struct { int kind, idx; Type* type; CProp* prop; int virt; } LV;

/* calls the getter of a property; the object (if it is not static) is on the stack. virt 0: base.X */
void emit_get(C* c, CProp* pp, int virt, int line) {
    CMethod* g = pp->get;
    check_access(c, pp->owner, pp->decl->access, pp->name, pp->nlen, line);
    if (!g) cfail(c, line, "property '%.*s' has no 'get', so it can only be assigned", pp->nlen, pp->name);
    if (pp->decl->getAccess && !can_access(c, pp->owner, pp->decl->getAccess))
        cfail(c, line, "'get' of property '%.*s' is %s, so it cannot be read here", pp->nlen, pp->name, pp->decl->getAccess == AC_PRIVATE ? "private" : "protected");
    if (pp->backing && g->sel < 0 && g->owner == pp->owner) {      /* { get; set; } that nothing overrides: read the field */
        if (pp->isStatic) emit_op1(c, OP_GETSTATIC, pp->backing->slot);
        else emit_op1(c, OP_GETFIELD, pp->backing->slot);
        return;
    }
    if (virt && g->sel >= 0) emit_op2(c, OP_CALLV, g->sel, 1);
    else {
        if (g->func < 0) cfail(c, line, "property '%.*s' is abstract here", pp->nlen, pp->name);
        emit_op2(c, OP_CALL, g->func, g->isStatic ? 0 : 1);
    }
}

/* a property as the target of an assignment. In a constructor, a property { get; } of this class sets its hidden field. */
static LV prop_lvalue(C* c, CProp* pp, int virt, int viaThis, int line) {
    LV lv = { LV_PROP, 0, pp->type, pp, virt };
    check_access(c, pp->owner, pp->decl->access, pp->name, pp->nlen, line);
    if (pp->set && pp->decl->setAccess && !can_access(c, pp->owner, pp->decl->setAccess))
        cfail(c, line, "'set' of property '%.*s' is %s, so it cannot be assigned here (only read)", pp->nlen, pp->name,
              pp->decl->setAccess == AC_PRIVATE ? "private" : "protected");
    if (!pp->set) {
        if (pp->backing && viaThis && pp->owner == c->cur && c->isCtor && !pp->isStatic) {
            lv.kind = pp->isStatic ? LV_STATIC : LV_FIELD; lv.idx = pp->backing->slot; lv.prop = NULL;
            return lv;
        }
        cfail(c, line, "property '%.*s' has no 'set', so it cannot be assigned%s", pp->nlen, pp->name,
              pp->backing ? " (except in a constructor of its class)" : "");
    }
    return lv;
}

/* is e a property (x, obj.X, Class.X)? */
static int is_prop_access(C* c, Expr* e) {
    if (e->k == E_NAME) return resolve_name(c, e->name, e->nlen).k == R_PROP;
    if (e->k != E_MEMBER || e->prefix) return 0;
    fold_ns(c, e);
    if (e->a->k == E_NAME) {
        Res r = resolve_name(c, e->a->name, e->a->nlen);
        if (r.k == R_CLASS) return !find_field(c, r.idx, e->name, e->nlen) && find_prop(c, r.idx, e->name, e->nlen);
        if (r.k == R_BUILTIN) return 0;
    }
    Type* t = peek_type(c, e->a);
    return type_isobj(t) && !find_field(c, t->cls, e->name, e->nlen) && find_prop(c, t->cls, e->name, e->nlen);
}
/* a struct value that is a copy, not a variable: changing its fields would be lost (C# does not allow it) */
static int is_temp_struct(C* c, Expr* e) {
    if (e->k == E_INDEX) return peek_type(c, e->a)->k == T_LIST;
    return is_prop_access(c, e);
}

/* the index of t[i] (array, List, string) or t[i, j] (T[,]: OP_IDX2 makes one index of the two); the collection is
   already on the stack, after this the index too, ready for GETELEM / SETELEM */
static void index_args(C* c, Expr* e, Type* t) {
    int rect = t->k == T_ARRAY && t->rank == 2;
    if (rect && !e->c) cfail(c, e->line, "%s needs two indexes: [row, column]", type_name(c, t));
    if (!rect && e->c) cfail(c, e->line, "%s takes one index, not two (only T[,] arrays take [row, column])", type_name(c, t));
    Type* it = compile_expr(c, e->b);
    if (!type_isint(it)) cfail(c, e->line, "index must be int, not %s", type_name(c, it));
    if (rect) {
        Type* jt = compile_expr(c, e->c);
        if (!type_isint(jt)) cfail(c, e->line, "index must be int, not %s", type_name(c, jt));
        c->line = e->line;
        emit_op(c, OP_IDX2);
    }
    c->line = e->line;
}

static LV lvalue(C* c, Expr* e) {
    LV lv = { 0, 0, NULL, NULL, 0 };
    c->line = e->line;
    if (e->k == E_NAME) {
        check_writable_local(c, e->name, e->nlen, e->line);
        Res r = resolve_name(c, e->name, e->nlen);
        switch (r.k) {
            case R_PROP:
                if (!r.prop->isStatic) {
                    if (c->curStatic) cfail(c, e->line, "cannot use instance property '%.*s' in a static method", e->nlen, e->name);
                    emit_op1(c, OP_LOAD, 0);
                }
                return prop_lvalue(c, r.prop, 1, 1, e->line);
            case R_LOCAL: lv.kind = LV_LOCAL; lv.idx = r.idx; lv.type = r.type; return lv;
            case R_CELL: case R_UPVAL:
                emit_op1(c, r.k == R_CELL ? OP_LOAD : OP_UPVAL, r.idx);
                lv.kind = LV_CELL; lv.type = r.type; return lv;
            case R_STATIC: no_const_write(c, r.field, e->line); lv.kind = LV_STATIC; lv.idx = r.idx; lv.type = r.type; return lv;
            case R_FIELD:
                if (c->curStatic) cfail(c, e->line, "cannot use instance field '%.*s' in a static method", e->nlen, e->name);
                emit_op1(c, OP_LOAD, 0);
                lv.kind = LV_FIELD; lv.idx = r.idx; lv.type = r.type; return lv;
            case R_NONE: cfail(c, e->line, "unknown name '%.*s'%s", e->nlen, e->name, class_hint(c, e->name, e->nlen));
            default: cfail(c, e->line, "cannot assign to '%.*s'", e->nlen, e->name);
        }
    }
    if (e->k == E_MEMBER) {
        fold_ns(c, e);
        if (e->a->k == E_NAME) {
            Res r = resolve_name(c, e->a->name, e->a->nlen);
            if (r.k == R_CLASS) {
                if (c->classes[r.idx].isEnum) cfail(c, e->line, "enum members cannot be changed");
                CField* f = use_field(c, find_field(c, r.idx, e->name, e->nlen), e->line);
                CProp* pp = f ? NULL : find_prop(c, r.idx, e->name, e->nlen);
                if (pp) {
                    if (!pp->isStatic) cfail(c, e->line, "'%.*s' is not static; use an object", e->nlen, e->name);
                    return prop_lvalue(c, pp, 1, r.idx == c->cur, e->line);
                }
                if (!f) cfail(c, e->line, "class '%.*s' has no field '%.*s'", e->a->nlen, e->a->name, e->nlen, e->name);
                if (!f->isStatic) cfail(c, e->line, "'%.*s' is not static; use an object", e->nlen, e->name);
                no_const_write(c, f, e->line);
                lv.kind = LV_STATIC; lv.idx = f->slot; lv.type = f->type; return lv;
            }
            if (r.k == R_BUILTIN) cfail(c, e->line, "'%.*s.%.*s' is read-only", e->a->nlen, e->a->name, e->nlen, e->name);
        }
        Type* t = compile_expr(c, e->a);
        c->line = e->line;
        if (t->k == T_STRUCT && (e->a->k == E_CALL || is_temp_struct(c, e->a)))
            cfail(c, e->line, "cannot change '%.*s' of a copy: this %s is a value given back by a method, property or List; "
                  "put it in a variable, change it there and store it back", e->nlen, e->name, type_name(c, t));
        if (type_isobj(t)) {
            CField* f = use_field(c, find_field(c, t->cls, e->name, e->nlen), e->line);
            CProp* pp = f ? NULL : find_prop(c, t->cls, e->name, e->nlen);
            if (pp) {
                if (pp->isStatic) cfail(c, e->line, "'%.*s' is static; use %s.%.*s", e->nlen, e->name, type_name(c, t), e->nlen, e->name);
                return prop_lvalue(c, pp, e->a->k != E_BASE, e->a->k == E_THIS, e->line);
            }
            if (!f) cfail(c, e->line, "%s has no field '%.*s'", type_name(c, t), e->nlen, e->name);
            if (f->isStatic) cfail(c, e->line, "'%.*s' is static; use %s.%.*s", e->nlen, e->name, type_name(c, t), e->nlen, e->name);
            lv.kind = LV_FIELD; lv.idx = f->slot; lv.type = f->type; return lv;
        }
        cfail(c, e->line, "cannot assign to '%.*s' of %s", e->nlen, e->name, type_name(c, t));
    }
    if (e->k == E_INDEX) {
        Type* t = compile_expr(c, e->a);
        if (t->k == T_STRING) cfail(c, e->line, "strings cannot be changed; build a new string instead");
        if (t->k == T_DICT) {
            if (e->c) cfail(c, e->line, "a dictionary takes one key, not two");
            expr_to(c, e->b, t->key, e->line, "dictionary key");
            c->line = e->line;
            lv.kind = LV_ELEM; lv.type = t->elem; return lv;
        }
        if (t->k != T_ARRAY && t->k != T_LIST) cfail(c, e->line, "only arrays and lists can be indexed, not %s", type_name(c, t));
        index_args(c, e, t);
        lv.kind = LV_ELEM; lv.type = t->elem; return lv;
    }
    cfail(c, e->line, "the left side must be a variable, field or array element");
}
static void lv_dup(C* c, LV* lv) {
    if (lv->kind == LV_FIELD || lv->kind == LV_CELL || (lv->kind == LV_PROP && !lv->prop->isStatic)) emit_op(c, OP_DUP);
    else if (lv->kind == LV_ELEM) emit_op(c, OP_DUP2);
}
static void lv_load(C* c, LV* lv) {
    switch (lv->kind) {
        case LV_PROP: emit_get(c, lv->prop, lv->virt, c->line); break;
        case LV_LOCAL: emit_op1(c, OP_LOAD, lv->idx); break;
        case LV_STATIC: emit_op1(c, OP_GETSTATIC, lv->idx); break;
        case LV_FIELD: emit_op1(c, OP_GETFIELD, lv->idx); break;
        case LV_ELEM: emit_op(c, OP_GETELEM); break;
        case LV_CELL: emit_op(c, OP_GETCELL); break;
    }
}
/* stores the value on top of the stack and leaves it there (a property's setter returns nothing: the value is kept in a
   temporary local), or with 'unused' (an assignment statement) leaves nothing and returns 0 */
static int lv_store_(C* c, LV* lv, int unused) {
    if (lv->kind == LV_PROP) {
        CMethod* s = lv->prop->set;
        if (lv->prop->backing && s->sel < 0 && s->owner == lv->prop->owner) {   /* { get; set; }: write the field */
            emit_op1(c, lv->prop->isStatic ? OP_SETSTATIC : OP_SETFIELD, lv->prop->backing->slot);
            if (unused) { emit_pop(c); return 0; }
            return 1;
        }
        int argc = s->isStatic ? 1 : 2, tmp = -1;
        if (!unused) { tmp = alloc_temp(c); emit_op1(c, OP_STORE, tmp); }
        if (lv->virt && s->sel >= 0) emit_op2(c, OP_CALLV, s->sel, argc);
        else {
            if (s->func < 0) cfail(c, c->line, "property '%.*s' is abstract here", lv->prop->nlen, lv->prop->name);
            emit_op2(c, OP_CALL, s->func, argc);
        }
        if (unused) return 0;
        emit_op1(c, OP_LOAD, tmp);
        c->nextSlot--;
        return 1;
    }
    switch (lv->kind) {
        case LV_LOCAL: emit_op1(c, OP_STORE, lv->idx); break;
        case LV_STATIC: emit_op1(c, OP_SETSTATIC, lv->idx); break;
        case LV_FIELD: emit_op1(c, OP_SETFIELD, lv->idx); break;
        case LV_ELEM: emit_op(c, OP_SETELEM); break;
        case LV_CELL: emit_op(c, OP_SETCELL); break;
    }
    return 1;
}
static void lv_store(C* c, LV* lv) { lv_store_(c, lv, 0); }

/* A struct read from a variable, field, element or property is copied before it is stored somewhere else, so the two
   places do not share one object. 'new S()', a method's result (methods return copies) and ?: (its branches copy)
   are new values already. */
void copy_struct(C* c, Expr* e, Type* t) {
    if (t->k == T_STRUCT && e->k != E_NEW && e->k != E_CALL && e->k != E_COND) emit_op(c, OP_COPY);
}

/* ---------- main expression switch ---------- */
Type* compile_expr(C* c, Expr* e) {
    c->line = e->line;
    switch (e->k) {
        case E_INT: emit_op1(c, OP_ICONST, e->ival); return &tInt;
        case E_FLOAT: emit_fconst(c, e->fval); return &tFloat;
        case E_BOOL: emit_op(c, e->ival ? OP_TRUE : OP_FALSE); return &tBool;
        case E_CHAR: emit_op1(c, OP_ICONST, e->ival); emit_op(c, OP_I2C); return &tChar;
        case E_OUT: cfail(c, e->line, "'%s' can only be used for an argument of a method with a '%s' parameter", e->op ? "ref" : "out", e->op ? "ref" : "out");
        case E_PAIR: cfail(c, e->line, "{ key, value } is only allowed in a Dictionary initializer");
        case E_LAMBDA: cfail(c, e->line, "a lambda needs a known type here, e.g. Func<int, int> f = x => x * 2;");
        case E_STACK: return c->stackType;
        case E_COALESCE: {
            Expr* ca = e->a;
            int condCall = ca->k == E_CALL && ca->a->k == E_MEMBER && ca->a->prefix, condMember = ca->k == E_MEMBER && ca->prefix;
            if (condCall || condMember) {       /* a?.b ?? x, a?.F() ?? x: also for int, float... results */
                Expr* recv = condCall ? ca->a->a : ca->a;
                Type* t = compile_expr(c, recv);
                c->line = e->line;
                if (!type_isref(t) || t->k == T_NULL) cfail(c, e->line, "'?.' needs something that can be null, not %s", type_name(c, t));
                int jnull = emit_jump(c, OP_JNULL_POP);
                Expr st = *recv; st.k = E_STACK; c->stackType = t;
                Expr mf = condCall ? *ca->a : *ca; mf.prefix = 0; mf.a = &st;
                Expr ce; Type* rt;
                if (condCall) { ce = *ca; ce.a = &mf; rt = compile_call(c, &ce); } else rt = compile_expr(c, &mf);
                if (rt->k == T_VOID) cfail(c, e->line, "'?\?' needs a value on its left");
                Type* bt = e->b->k == E_LAMBDA ? rt : peek_type(c, e->b);
                Type* T = assignable(bt, rt) ? rt : assignable(rt, bt) ? bt : NULL;
                if (!T) cfail(c, e->line, "'?\?' needs both sides of the same type (%s vs %s)", type_name(c, rt), type_name(c, bt));
                coerce(c, rt, T, e->line, "'?\?'");
                int jend = emit_jump(c, type_isref(rt) ? OP_JNNULL : OP_JMP);    /* a null result also takes the right side */
                patch_jump(c, jnull);
                expr_to(c, e->b, T, e->line, "right side of '?\?'");
                patch_jump(c, jend);
                return T;
            }
            Type* lt = peek_type(c, e->a);
            Type* rt = e->b->k == E_LAMBDA ? lt : peek_type(c, e->b);
            if (!type_isref(lt)) cfail(c, e->line, "the left side of '?\?' must be something that can be null, not %s", type_name(c, lt));
            Type* t = lt->k == T_NULL ? rt : assignable(rt, lt) ? lt : assignable(lt, rt) ? rt : NULL;
            if (!t) cfail(c, e->line, "'?\?' needs both sides of the same type (%s vs %s)", type_name(c, lt), type_name(c, rt));
            compile_expr(c, e->a);
            c->line = e->line;
            int j = emit_jump(c, OP_JNNULL);
            expr_to(c, e->b, t, e->line, "right side of '?\?'");
            patch_jump(c, j);
            return t;
        }
        case E_SEQ: cfail(c, e->line, "a list with ',' is allowed only in the first and the last part of 'for'");
        case E_NULL: emit_op(c, OP_NULL); return &tNull;
        case E_STRING: emit_op1(c, OP_SCONST, add_string(c, e->name, e->nlen)); return &tString;
        case E_INTERP: {                        /* $"a{x}b" is "a" + x + "b"; it starts with a text piece, so a null string gives "" */
            int n = 0;
            for (int i = 0; i < e->nargs; i++) {
                Expr* part = e->args[i];
                if (n == 0 && !(part->k == E_STRING && part->prefix)) { emit_op1(c, OP_SCONST, add_string(c, "", 0)); n++; }
                Type* t = compile_expr(c, part);
                c->line = e->line;
                if (t->k != T_STRING) emit_tostr(c, t, 0, part->line);
                if (n++) emit_op(c, OP_CONCAT);
            }
            if (!n) emit_op1(c, OP_SCONST, add_string(c, "", 0));
            return &tString;
        }
        case E_THIS:
            if (c->curStatic) cfail(c, e->line, "'this' cannot be used in a static method");
            emit_op1(c, OP_LOAD, 0);
            return c->classes[c->cur].type;
        case E_BASE: {
            if (c->curStatic) cfail(c, e->line, "'base' cannot be used in a static method");
            int b = c->classes[c->cur].base;
            if (b < 0) cfail(c, e->line, "class '%.*s' has no base class", c->classes[c->cur].nlen, c->classes[c->cur].name);
            emit_op1(c, OP_LOAD, 0);
            return c->classes[b].type;
        }
        case E_IS: case E_AS: {
            Type* t = compile_expr(c, e->a);
            c->line = e->line;
            if (e->k == E_IS && !e->tn) {        /* is null / is not null */
                if (!type_isref(t)) cfail(c, e->line, "%s can never be null", type_name(c, t));
                emit_op(c, OP_NULL);
                emit_op(c, e->op ? OP_RNE : OP_REQ);
                return &tBool;
            }
            const char* w = e->k == E_IS ? "is" : "as";
            Type* to = resolve_typenode(c, e->tn, 0);
            if (to->k != T_CLASS) cfail(c, e->line, "'%s' works with classes and interfaces, not %s", w, type_name(c, to));
            if (t->k != T_CLASS && t->k != T_NULL) cfail(c, e->line, "'%s' works with objects of classes, not %s", w, type_name(c, t));
            if (e->k == E_AS) { emit_op2(c, OP_ISINST, to->cls, 1); return to; }
            if (e->name) {                      /* x is Enemy en: en = x as Enemy; en != null */
                emit_op2(c, OP_ISINST, to->cls, 1);
                int slot = declare_local(c, e->name, e->nlen, to, e->line);
                if (c->lastBoxed) { emit_op(c, OP_DUP); init_local(c, slot, 1); }
                else emit_op1(c, OP_STORE, slot);
                emit_op(c, OP_NULL); emit_op(c, OP_RNE);
                return &tBool;
            }
            emit_op2(c, OP_ISINST, to->cls, 0);
            if (e->op) emit_op(c, OP_NOT);
            return &tBool;
        }

        case E_NAME: {
            Res r = resolve_name(c, e->name, e->nlen);
            switch (r.k) {
                case R_LOCAL:
                    if (r.type->k == T_KVP) cfail(c, e->line, "use '%.*s.Key' and '%.*s.Value'", e->nlen, e->name, e->nlen, e->name);
                    emit_op1(c, OP_LOAD, r.idx); return r.type;
                case R_CELL: emit_op1(c, OP_LOAD, r.idx); emit_op(c, OP_GETCELL); return r.type;
                case R_UPVAL: emit_op1(c, OP_UPVAL, r.idx); emit_op(c, OP_GETCELL); return r.type;
                case R_STATIC: if (is_const_field(r.field)) emit_const_field(c, r.field); else emit_op1(c, OP_GETSTATIC, r.idx); return r.type;
                case R_FIELD:
                    if (c->curStatic) cfail(c, e->line, "cannot use instance field '%.*s' in a static method", e->nlen, e->name);
                    emit_op1(c, OP_LOAD, 0); emit_op1(c, OP_GETFIELD, r.idx); return r.type;
                case R_PROP:
                    if (!r.prop->isStatic) {
                        if (c->curStatic) cfail(c, e->line, "cannot use instance property '%.*s' in a static method", e->nlen, e->name);
                        emit_op1(c, OP_LOAD, 0);
                    }
                    emit_get(c, r.prop, 1, e->line);
                    return r.type;
                case R_METHOD: cfail(c, e->line, "'%.*s' is a method; call it with ()", e->nlen, e->name);
                case R_CLASS: case R_BUILTIN: cfail(c, e->line, "'%.*s' is a class, not a value", e->nlen, e->name);
                default: cfail(c, e->line, "unknown name '%.*s'%s", e->nlen, e->name, class_hint(c, e->name, e->nlen));
            }
        }

        case E_MEMBER: {
            if (e->prefix) {                    /* a?.field: null when a is null */
                Type* t = compile_expr(c, e->a);
                c->line = e->line;
                if (!type_isref(t) || t->k == T_NULL) cfail(c, e->line, "'?.' needs something that can be null, not %s", type_name(c, t));
                int j = emit_jump(c, OP_JNULL);
                Expr st = *e; st.k = E_STACK; c->stackType = t;
                Expr m = *e; m.prefix = 0; m.a = &st;
                Type* rt = compile_expr(c, &m);
                if (!type_isref(rt)) cfail(c, e->line, "'?.%.*s' gives %s or null, and there is no %s? here; add a value for null: ?.%.*s ?\? 0",
                                      e->nlen, e->name, type_name(c, rt), type_name(c, rt), e->nlen, e->name);
                patch_jump(c, j);
                return rt;
            }
            int ei = enum_of(c, e->a);
            if (ei >= 0) {                              /* Dir.Up, Player.State.Jump: a constant */
                CClass* k = &c->classes[ei];
                CField* f = find_field(c, ei, e->name, e->nlen);
                if (!f) cfail(c, e->line, "enum '%.*s' has no member '%.*s'", k->nlen, k->name, e->nlen, e->name);
                emit_op1(c, OP_ICONST, f->value);
                return k->type;
            }
            fold_ns(c, e);
            if (e->a->k == E_NAME) {
                Res r = resolve_name(c, e->a->name, e->a->nlen);
                if (r.k == R_LOCAL && r.type->k == T_KVP) {     /* foreach pair: key and value live in two slots */
                    int isKey = namecs(e->name, e->nlen, "Key");
                    if (!isKey && !namecs(e->name, e->nlen, "Value")) cfail(c, e->line, "a KeyValuePair has only 'Key' and 'Value'");
                    emit_op1(c, OP_LOAD, r.idx + (isKey ? 0 : 1));
                    return isKey ? r.type->key : r.type->elem;
                }
                if (r.k == R_CLASS) {
                    CClass* k = &c->classes[r.idx];
                    CField* f = use_field(c, find_field(c, r.idx, e->name, e->nlen), e->line);
                    if (k->isEnum) {                    /* Dir.Up: a constant */
                        if (!f) cfail(c, e->line, "enum '%.*s' has no member '%.*s'", e->a->nlen, e->a->name, e->nlen, e->name);
                        emit_op1(c, OP_ICONST, f->value);
                        return k->type;
                    }
                    CProp* pp = f ? NULL : find_prop(c, r.idx, e->name, e->nlen);
                    if (pp) {
                        if (!pp->isStatic) cfail(c, e->line, "'%.*s' is not static; use an object", e->nlen, e->name);
                        emit_get(c, pp, 1, e->line);
                        return pp->type;
                    }
                    if (!f) cfail(c, e->line, "class '%.*s' has no field '%.*s'", e->a->nlen, e->a->name, e->nlen, e->name);
                    if (!f->isStatic) cfail(c, e->line, "'%.*s' is not static; use an object", e->nlen, e->name);
                    if (is_const_field(f)) emit_const_field(c, f); else emit_op1(c, OP_GETSTATIC, f->slot);
                    return f->type;
                }
                if (r.k == R_BUILTIN) {
                    for (int i = 0; i < ms_nconsts; i++)
                        if (namecs(e->a->name, e->a->nlen, ms_consts[i].cls) && namecs(e->name, e->nlen, ms_consts[i].name)) {
                            if (ms_consts[i].isFloat) { emit_fconst(c, ms_consts[i].fvalue); return &tFloat; }
                            emit_op1(c, OP_ICONST, ms_consts[i].ivalue);
                            return &tInt;
                        }
                    cfail(c, e->line, "'%.*s' has no member '%.*s'", e->a->nlen, e->a->name, e->nlen, e->name);
                }
            }
            Type* t = compile_expr(c, e->a);
            c->line = e->line;
            if ((t->k == T_STRING || t->k == T_ARRAY) && namecs(e->name, e->nlen, "Length")) {
                emit_op(c, t->k == T_STRING ? OP_STRLEN : OP_ARRLEN);
                return &tInt;
            }
            if ((t->k == T_LIST || t->k == T_DICT) && namecs(e->name, e->nlen, "Count")) { emit_op(c, OP_ARRLEN); return &tInt; }
            if (t->k == T_DICT && namecs(e->name, e->nlen, "Keys")) { emit_op1(c, OP_DICT, D_KEYS); return list_of(c, t->key); }
            if (t->k == T_DICT && namecs(e->name, e->nlen, "Values")) { emit_op1(c, OP_DICT, D_VALUES); return list_of(c, t->elem); }
            if (t->k == T_LIST && namecs(e->name, e->nlen, "Length")) cfail(c, e->line, "a List has 'Count', not 'Length'");
            if (t->k == T_ARRAY && namecs(e->name, e->nlen, "Count")) cfail(c, e->line, "an array has 'Length', not 'Count'");
            if (type_isobj(t)) {
                CField* f = use_field(c, find_field(c, t->cls, e->name, e->nlen), e->line);
                CProp* pp = f ? NULL : find_prop(c, t->cls, e->name, e->nlen);
                if (pp) {
                    if (pp->isStatic) cfail(c, e->line, "'%.*s' is static; use %s.%.*s", e->nlen, e->name, type_name(c, t), e->nlen, e->name);
                    emit_get(c, pp, e->a->k != E_BASE, e->line);
                    return pp->type;
                }
                if (!f) {
                    if (find_method(c, t->cls, e->name, e->nlen)) cfail(c, e->line, "'%.*s' is a method; call it with ()", e->nlen, e->name);
                    cfail(c, e->line, "%s has no field '%.*s'", type_name(c, t), e->nlen, e->name);
                }
                if (f->isStatic) cfail(c, e->line, "'%.*s' is static; use %s.%.*s", e->nlen, e->name, type_name(c, t), e->nlen, e->name);
                emit_op1(c, OP_GETFIELD, f->slot);
                return f->type;
            }
            cfail(c, e->line, "%s has no member '%.*s'", type_name(c, t), e->nlen, e->name);
        }

        case E_INDEX: {
            Type* t = compile_expr(c, e->a);
            if (t->k == T_DICT) {
                if (e->c) cfail(c, e->line, "a dictionary takes one key, not two");
                expr_to(c, e->b, t->key, e->line, "dictionary key");
                c->line = e->line;
                emit_op(c, OP_GETELEM);
                return t->elem;
            }
            if (t->k != T_ARRAY && t->k != T_LIST && t->k != T_STRING) cfail(c, e->line, "only arrays, lists, dictionaries and strings can be indexed, not %s", type_name(c, t));
            index_args(c, e, t);
            emit_op(c, OP_GETELEM);
            return t->k == T_STRING ? &tChar : t->elem;
        }

        case E_CALL: return compile_call(c, e);

        case E_NEW: {
            if (e->tn->base != TK_IDENT) cfail(c, e->line, "cannot use 'new' with a built-in type like that");
            if (namecs(e->tn->name, e->tn->nlen, "WaitForSeconds") || namecs(e->tn->name, e->tn->nlen, "WaitForSecondsRealtime") ||
                namecs(e->tn->name, e->tn->nlen, "WaitForEndOfFrame"))
                cfail(c, e->line, "'new %.*s(...)' can only be used in 'yield return'", e->tn->nlen, e->tn->name);
            int ci = find_class(c, e->tn->name, e->tn->nlen);
            if (ci < 0) {
                if (is_builtin_class(e->tn->name, e->tn->nlen)) cfail(c, e->line, "'%.*s' is built-in and cannot be created", e->tn->nlen, e->tn->name);
                cfail(c, e->line, "unknown class '%.*s'%s", e->tn->nlen, e->tn->name, class_hint(c, e->tn->name, e->tn->nlen));
            }
            CClass* k = &c->classes[ci];
            if (k->isEnum) cfail(c, e->line, "'%.*s' is an enum; use one of its members, e.g. %.*s.%.*s", k->nlen, k->name, k->nlen, k->name,
                                 k->decl->nfields ? k->decl->fields[0].nlen : 1, k->decl->nfields ? k->decl->fields[0].name : "X");
            if (k->isInterface) cfail(c, e->line, "'%.*s' is an interface; create an object of a class that implements it", k->nlen, k->name);
            if (k->isAbstract) cfail(c, e->line, "'%.*s' is abstract; create an object of a class derived from it", k->nlen, k->name);
            if (k->decl->isStatic) cfail(c, e->line, "'%.*s' is a static class, so no object of it can be made; use its members directly: %.*s.Name",
                                         k->nlen, k->name, k->nlen, k->name);
            char what[80]; snprintf(what, sizeof what, "constructor of '%.*s'", k->nlen, k->name);
            CMethod* cand[64]; int n = 0, has0 = 0;
            for (int i = 0; i < k->nctors && n < 64; i++) { cand[n++] = &k->ctors[i]; has0 |= k->ctors[i].np == 0; }
            n = accessible_cands(c, cand, n, e->line);
            if (k->isStruct && !e->nargs && !has0) { emit_op1(c, OP_NEW, ci); return k->type; }   /* new V2(): a zeroed struct, as in C# */
            CMethod* ct = pick_overload(c, cand, n, e, what);
            emit_op1(c, OP_NEW, ci);
            args_for_m(c, e, ct, what, ct->roParams);
            c->line = e->line;
            emit_op2(c, OP_CALL, ct->func, ct->np + 1);
            return k->type;
        }

        case E_NEWLIST: {
            Type* lt = resolve_typenode(c, e->tn, 0);
            Type* ct = e->a ? peek_type(c, e->a) : NULL;
            if (ct && type_isint(ct)) { compile_expr(c, e->a); emit_op(c, OP_POP); ct = NULL; }   /* the capacity is only a hint */
            c->line = e->line;
            if (ct) {                           /* new List<T>(otherListOrArray): a copy */
                if (lt->k != T_LIST || !((ct->k == T_LIST || (ct->k == T_ARRAY && ct->rank != 2)) && ct->elem == lt->elem))
                    cfail(c, e->line, "'new %s(...)' takes a capacity (int) or a %s / %s[] to copy, not %s",
                          type_name(c, lt), type_name(c, lt), type_name(c, lt->elem), type_name(c, ct));
                emit_op1(c, OP_LIST, L_NEW);
                emit_op(c, OP_DUP);
                compile_expr(c, e->a);
                c->line = e->line;
                emit_op1(c, OP_LIST, L_ADDRANGE);
            }
            if (lt->k == T_DICT) {
                emit_op1(c, OP_DICT, D_NEW);
                for (int i = 0; i < e->nargs; i++) {
                    Expr* it = e->args[i];
                    if (it->k != E_PAIR) cfail(c, it->line, "dictionary items are written { key, value } or [key] = value");
                    emit_op(c, OP_DUP);
                    expr_to(c, it->a, lt->key, it->line, "dictionary key");
                    expr_to(c, it->b, lt->elem, it->line, "dictionary value");
                    c->line = it->line;
                    if (it->prefix) { emit_op(c, OP_SETELEM); emit_op(c, OP_POP); }   /* [k] = v overwrites */
                    else emit_op1(c, OP_DICT, D_ADD);                             /* {k, v} is Add: duplicates fail */
                }
                return lt;
            }
            if (lt->k != T_LIST) cfail(c, e->line, "'new %s' needs '()'", type_name(c, lt));
            if (!ct) emit_op1(c, OP_LIST, L_NEW);
            for (int i = 0; i < e->nargs; i++) {
                if (e->args[i]->k == E_PAIR) cfail(c, e->args[i]->line, "a List item is a single value, not { a, b }");
                emit_op(c, OP_DUP);
                expr_to(c, e->args[i], lt->elem, e->args[i]->line, "list element");
                emit_op1(c, OP_LIST, L_ADD);
            }
            return lt;
        }

        case E_NEWARR: {
            Type* elem = resolve_typenode(c, e->tn, 0);
            if (elem->k == T_VOID) cfail(c, e->line, "cannot make an array of void");
            if (e->op == 2) {                       /* T[,]: new T[rows, cols] or { { .. }, { .. } } (items row after row) */
                if (!e->prefix) {
                    Type* rt = compile_expr(c, e->a);
                    if (rt->k != T_INT) cfail(c, e->line, "array size must be int, not %s", type_name(c, rt));
                    Type* ct = compile_expr(c, e->c);
                    if (ct->k != T_INT) cfail(c, e->line, "array size must be int, not %s", type_name(c, ct));
                    c->line = e->line;
                    emit_op1(c, OP_NEWARR2, default_kind(elem));
                } else {
                    emit_op1(c, OP_ICONST, (int)e->fval);
                    emit_op1(c, OP_ICONST, e->ival);
                    emit_op1(c, OP_NEWARR2, default_kind(elem));
                    for (int i = 0; i < e->nargs; i++) {
                        emit_op(c, OP_DUP);
                        emit_op1(c, OP_ICONST, i);
                        expr_to(c, e->args[i], elem, e->args[i]->line, "array element");
                        emit_op(c, OP_SETELEM);
                        emit_op(c, OP_POP);
                    }
                }
                return array2_of(c, elem);
            }
            if (!e->prefix) {
                Type* st = compile_expr(c, e->a);
                if (st->k != T_INT) cfail(c, e->line, "array size must be int, not %s", type_name(c, st));
                c->line = e->line;
                emit_op1(c, OP_NEWARR, default_kind(elem));
            } else {
                emit_op1(c, OP_ICONST, e->nargs);
                emit_op1(c, OP_NEWARR, default_kind(elem));
                for (int i = 0; i < e->nargs; i++) {
                    emit_op(c, OP_DUP);
                    emit_op1(c, OP_ICONST, i);
                    expr_to(c, e->args[i], elem, e->args[i]->line, "array element");
                    emit_op(c, OP_SETELEM);
                    emit_op(c, OP_POP);
                }
            }
            return array_of(c, elem);
        }

        case E_UNARY: {
            Type* t = compile_expr(c, e->a);
            c->line = e->line;
            if (e->op == TK_MINUS) {
                if (type_isint(t)) { if (t->k == T_CHAR) emit_op1(c, OP_C2I, 0); emit_op(c, OP_INEG); return &tInt; }
                if (t->k == T_FLOAT) { emit_op(c, OP_FNEG); return t; }
                cfail(c, e->line, "operator '-' cannot be used with %s", type_name(c, t));
            }
            if (e->op == TK_TILDE) {
                if (t->k == T_ENUM) { emit_op(c, OP_INOT); return t; }
                if (!type_isint(t)) cfail(c, e->line, "operator '~' cannot be used with %s", type_name(c, t));
                if (t->k == T_CHAR) emit_op1(c, OP_C2I, 0);
                emit_op(c, OP_INOT);
                return &tInt;
            }
            need_bool(c, t, e->line, "operand of '!'");
            emit_op(c, OP_NOT);
            return &tBool;
        }

        case E_CAST: {
            Type* t = compile_expr(c, e->a);
            c->line = e->line;
            if (e->tn) {                        /* (Enemy)x */
                Type* to = resolve_typenode(c, e->tn, 0);
                if (to->k == T_ENUM) {          /* (Dir)2 */
                    if (t->k == T_FLOAT) emit_op(c, OP_F2I);
                    else if (t->k == T_CHAR) emit_op1(c, OP_C2I, 0);
                    else if (t->k != T_INT && t->k != T_ENUM) cfail(c, e->line, "cannot convert %s to %s", type_name(c, t), type_name(c, to));
                    return to;
                }
                if (to->k == T_STRUCT) {
                    if (t != to) cfail(c, e->line, "cannot convert %s to %s", type_name(c, t), type_name(c, to));
                    return to;
                }
                if (to->k != T_CLASS) cfail(c, e->line, "cannot cast to %s this way", type_name(c, to));
                if (t->k == T_NULL || assignable(t, to)) return to;          /* up-cast: nothing to check */
                if (t->k != T_CLASS) cfail(c, e->line, "cannot convert %s to %s", type_name(c, t), type_name(c, to));
                if (!to->isa[t->cls] && !c->classes[t->cls].isInterface && !c->classes[to->cls].isInterface)
                    cfail(c, e->line, "cannot convert %s to %s (neither derives from the other)", type_name(c, t), type_name(c, to));
                emit_op2(c, OP_ISINST, to->cls, 2);  /* fails at run time if the object is not one */
                return to;
            }
            if (t->k == T_ENUM) {               /* (int)Dir.Up */
                if (e->op == TK_KINT) return &tInt;
                if (e->op == TK_KCHAR) { emit_op(c, OP_I2C); return &tChar; }
                emit_op1(c, OP_I2F, 0); return &tFloat;
            }
            if (!type_isnum(t)) cfail(c, e->line, "cannot cast %s to a number", type_name(c, t));
            if (e->op == TK_KINT) { if (t->k == T_FLOAT) emit_op(c, OP_F2I); else if (t->k == T_CHAR) emit_op1(c, OP_C2I, 0); return &tInt; }
            if (e->op == TK_KCHAR) {
                if (t->k == T_FLOAT) emit_op(c, OP_F2I);
                if (t->k != T_CHAR) emit_op(c, OP_I2C);
                return &tChar;
            }
            if (t->k != T_FLOAT) emit_op1(c, OP_I2F, 0);
            return &tFloat;
        }

        case E_BINARY: {
            Type* lt = compile_expr(c, e->a);
            Type* rt = compile_expr(c, e->b);
            c->line = e->line;
            switch (e->op) {
                case TK_PLUS: case TK_MINUS: case TK_STAR: case TK_SLASH: case TK_PERCENT:
                    return arith(c, e->op, lt, rt, e->line);
                case TK_AMP: case TK_PIPE: case TK_CARET: case TK_SHL: case TK_SHR:
                    return bitwise(c, e->op, lt, rt, e->line);
                default:
                    return compare_op(c, e->op, lt, rt, e->line);
            }
        }

        case E_LOGIC: {
            Type* lt = compile_expr(c, e->a);
            need_bool(c, lt, e->line, e->op == TK_AND ? "left side of '&&'" : "left side of '||'");
            c->line = e->line;
            int j = emit_jump(c, e->op == TK_AND ? OP_JFALSE_KEEP : OP_JTRUE_KEEP);
            Type* rt = compile_expr(c, e->b);
            need_bool(c, rt, e->line, e->op == TK_AND ? "right side of '&&'" : "right side of '||'");
            patch_jump(c, j);
            return &tBool;
        }

        case E_COND: {
            Type* ct = compile_expr(c, e->a);
            need_bool(c, ct, e->line, "condition of '?:'");
            Type* tb = peek_type(c, e->b);
            Type* tc = peek_type(c, e->c);
            Type* t;
            if (tb == tc) t = tb;
            else if (type_isnum(tb) && type_isnum(tc)) t = (tb->k == T_FLOAT || tc->k == T_FLOAT) ? &tFloat : &tInt;
            else if (tb->k == T_NULL && type_isref(tc)) t = tc;
            else if (tc->k == T_NULL && type_isref(tb)) t = tb;
            else if (tb->k == T_CLASS && assignable(tc, tb)) t = tb;
            else if (tc->k == T_CLASS && assignable(tb, tc)) t = tc;
            else cfail(c, e->line, "both sides of '?:' must have the same type (%s vs %s)", type_name(c, tb), type_name(c, tc));
            if (t->k == T_VOID) cfail(c, e->line, "'?:' branches cannot be void");
            c->line = e->line;
            int jz = emit_jump(c, OP_JZ);
            expr_to(c, e->b, t, e->line, "'?:'");
            int je = emit_jump(c, OP_JMP);
            patch_jump(c, jz);
            expr_to(c, e->c, t, e->line, "'?:'");
            patch_jump(c, je);
            return t;
        }

        case E_ASSIGN: {
            int unused = c->assignUnused;       /* a statement 'X = v;': a property's setter need not keep the value */
            c->assignUnused = 0;
            LV lv = lvalue(c, e->a);
            if (e->op == TK_ASSIGN) {
                expr_to(c, e->b, lv.type, e->line, "assignment");
                c->line = e->line;
                return lv_store_(c, &lv, unused && lv.kind == LV_PROP) ? lv.type : &tVoid;
            }
            if (lv.type->k == T_FUNC && (e->op == TK_PLUSEQ || e->op == TK_MINUSEQ)) {    /* onHit += Handler; */
                lv_dup(c, &lv);
                lv_load(c, &lv);
                expr_to(c, e->b, lv.type, e->line, e->op == TK_PLUSEQ ? "'+='" : "'-='");
                c->line = e->line;
                emit_op1(c, OP_DELEG, e->op == TK_MINUSEQ);
                lv_store(c, &lv);
                return lv.type;
            }
            int tok = e->op == TK_PLUSEQ ? TK_PLUS : e->op == TK_MINUSEQ ? TK_MINUS :
                      e->op == TK_STAREQ ? TK_STAR : e->op == TK_SLASHEQ ? TK_SLASH :
                      e->op == TK_AMPEQ ? TK_AMP : e->op == TK_PIPEEQ ? TK_PIPE : e->op == TK_CARETEQ ? TK_CARET :
                      e->op == TK_SHLEQ ? TK_SHL : e->op == TK_SHREQ ? TK_SHR : TK_PERCENT;
            lv_dup(c, &lv);
            lv_load(c, &lv);
            Type* rt = compile_expr(c, e->b);
            c->line = e->line;
            Type* res = is_bitwise(tok) ? bitwise(c, tok, lv.type, rt, e->line) : arith(c, tok, lv.type, rt, e->line);
            if (lv.type->k == T_CHAR && res->k == T_INT) emit_op(c, OP_I2C);     /* c += 1 stays a char, like C# */
            else coerce(c, res, lv.type, e->line, "compound assignment");
            lv_store(c, &lv);
            return lv.type;
        }

        case E_INCDEC: {
            LV lv = lvalue(c, e->a);
            if (!type_isnum(lv.type) && lv.type->k != T_ENUM) cfail(c, e->line, "'%s' needs an int or float, not %s", e->op == TK_INC ? "++" : "--", type_name(c, lv.type));
            c->line = e->line;
            lv_dup(c, &lv);
            lv_load(c, &lv);
            int tmp = -1;
            if (!e->prefix) { tmp = alloc_temp(c); emit_op1(c, OP_STORE, tmp); }
            if (type_isint(lv.type) || lv.type->k == T_ENUM) { emit_op1(c, OP_ICONST, 1); emit_op(c, e->op == TK_INC ? OP_IADD : OP_ISUB); }   /* a char stays a char */
            else { emit_fconst(c, 1.0f); emit_op(c, e->op == TK_INC ? OP_FADD : OP_FSUB); }
            lv_store(c, &lv);
            if (!e->prefix) { emit_pop(c); emit_op1(c, OP_LOAD, tmp); c->nextSlot--; }
            return lv.type;
        }
    }
    cfail(c, e->line, "internal error: unknown expression");
}
