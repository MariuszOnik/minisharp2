/* MiniSharp - type checker and bytecode generator.
 * Walks the AST once (after collecting class signatures) and emits bytecode for a stack VM.
 * Split into compiler.c (classes, methods, the entry points) and comp_*.c; shared state in compiler.h. */
#include "compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ================= classes and methods ================= */
static void begin_func(C* c, int func, int cls, const char* mname, int mlen, int isStatic) {
    c->fn = &c->P->funcs[func];
    snprintf(c->fn->name, sizeof c->fn->name, "%.*s.%.*s", c->classes[cls].nlen, c->classes[cls].name, mlen, mname);
    c->cur = cls; c->scope = cls; c->curStatic = isStatic;
    c->nlocals = 0; c->nextSlot = isStatic ? 0 : 1;
    c->fdepth = 0; c->lam = NULL; c->caps.n = 0;
    c->fn->nlocals = c->nextSlot;
    c->loop = NULL; c->lastOp = c->prevOp = c->prevOp2 = -1; c->lastLabel = -1;
}

static void compile_method(C* c, int cls, CMethod* m) {
    MethodDecl* d = m->decl;
    begin_func(c, m->func, cls, m->name, m->nlen, m->isStatic);
    c->fn->nparams = m->np + (m->isStatic ? 0 : 1);
    c->isCtor = 0; c->retType = m->ret; c->isIter = m->isIter;
    scan_captures(c, d->body, NULL);
    for (int i = 0; i < m->np; i++) declare_local(c, d->params[i].name, d->params[i].nlen, m->params[i], d->line);
    box_params(c);
    compile_stmt(c, d->body);
    c->line = d->line;
    if (m->ret->k == T_VOID || m->isIter) emit_op(c, OP_RET); else emit_op(c, OP_NORET);
    c->isIter = 0;
}

static void compile_ctor(C* c, int cls, CMethod* ct) {
    CClass* k = &c->classes[cls];
    MethodDecl* d = ct->decl;
    begin_func(c, ct->func, cls, ".ctor", 5, 0);
    c->fn->nparams = ct->np + 1;
    c->isCtor = 1; c->retType = &tVoid;
    scan_captures(c, d ? d->body : NULL, d ? d->baseCall : NULL);
    if (d) for (int i = 0; i < d->nparams; i++) declare_local(c, d->params[i].name, d->params[i].nlen, ct->params[i], d->line);
    box_params(c);
    Expr* bc = d ? d->baseCall : NULL;
    char what[80];
    if (bc && d->thisCall) {
        /* ': this(...)': that constructor runs the field initializers and the base constructor */
        snprintf(what, sizeof what, "constructor of '%.*s'", k->nlen, k->name);
        CMethod* cand[64]; int n = 0;
        for (int i = 0; i < k->nctors && n < 64; i++) cand[n++] = &k->ctors[i];
        CMethod* t = pick_overload(c, cand, n, bc, what);
        if (t == ct) cfail(c, bc->line, "this constructor calls itself with ': this(...)'");
        c->line = bc->line;
        emit_op1(c, OP_LOAD, 0);
        c->curStatic = 1;                   /* like C#: 'this' is not ready yet in this(...) arguments */
        args_for(c, bc, t->params, t->np, what);
        c->curStatic = 0;
        emit_op2(c, OP_CALL, t->func, t->np + 1);
        emit_op(c, OP_POP);
    } else {
        /* same order as C#: this class's field initializers, then the base constructor, then the body */
        for (int i = 0; i < k->nf; i++) {
            CField* f = &k->fields[i];
            if (f->isStatic || !f->decl->init) continue;
            c->line = f->decl->line;
            emit_op1(c, OP_LOAD, 0);
            expr_to(c, f->decl->init, f->type, f->decl->line, "field initializer");
            emit_op1(c, OP_SETFIELD, f->slot);
            emit_op(c, OP_POP);
        }
        if (k->base >= 0) {
            CClass* b = &c->classes[k->base];
            c->line = bc ? bc->line : d ? d->line : k->decl->line;
            snprintf(what, sizeof what, "constructor of '%.*s'", b->nlen, b->name);
            CMethod* t = NULL;
            if (bc) {
                CMethod* cand[64]; int n = 0;
                for (int i = 0; i < b->nctors && n < 64; i++) cand[n++] = &b->ctors[i];
                t = pick_overload(c, cand, n, bc, what);
            } else {
                for (int i = 0; i < b->nctors && !t; i++) if (b->ctors[i].np == 0) t = &b->ctors[i];
                if (!t) cfail(c, c->line, "the constructor of '%.*s' takes %d argument%s; call it with ': base(...)' after the constructor of '%.*s'",
                              b->nlen, b->name, b->ctors[0].np, b->ctors[0].np == 1 ? "" : "s", k->nlen, k->name);
            }
            emit_op1(c, OP_LOAD, 0);
            if (bc) {
                c->curStatic = 1;               /* like C#: 'this' is not ready yet in base(...) arguments */
                args_for(c, bc, t->params, t->np, what);
                c->curStatic = 0;
            }
            emit_op2(c, OP_CALL, t->func, t->np + 1);
            emit_op(c, OP_POP);
        } else if (bc) cfail(c, bc->line, "'%.*s' has no base class, so it cannot call base(...)", k->nlen, k->name);
    }
    if (d) compile_stmt(c, d->body);
    c->line = d ? d->line : k->decl->line;
    emit_op1(c, OP_LOAD, 0);
    emit_op(c, OP_RETV);
}

static void compile_static_init(C* c, int func) {
    begin_func(c, func, 0, ".cctor", 6, 1);
    for (int ci = 0; ci < c->nclasses; ci++) {
        CClass* k = &c->classes[ci];
        if (k->isEnum) continue;
        c->cur = ci; c->scope = ci; c->curStatic = 1; c->isCtor = 0; c->retType = &tVoid;
        c->caps.n = 0;
        for (int i = 0; i < k->nf; i++) {
            CField* f = &k->fields[i];
            if (!f->isStatic) continue;
            c->line = f->decl->line;
            if (f->decl->init) expr_to(c, f->decl->init, f->type, f->decl->line, "field initializer");
            else if (f->type->k == T_STRUCT) emit_op1(c, OP_NEW, f->type->cls);      /* a zeroed struct */
            else continue;
            emit_op1(c, OP_SETSTATIC, f->slot);
            emit_op(c, OP_POP);
        }
    }
    emit_op(c, OP_RET);
}

/* ---------- class hierarchy ---------- */
static void add_sel(C* c, CMethod* m) {
    c->sels = (CMethod**)arena_grow(&c->A, c->sels, c->nsel, &c->capsel, sizeof(CMethod*));
    m->sel = c->nsel;
    c->sels[c->nsel++] = m;
}

static int same_sig(CMethod* a, CMethod* b) { return a->ret == b->ret && same_params(a, b); }

/* the method of class cls (or its bases) with the name and parameters of m, or NULL */
static CMethod* find_same(C* c, int cls, CMethod* m) {
    CMethod* cand[64];
    int k = method_cands(c, cls, m->name, m->nlen, cand, 0, 64);
    for (int i = 0; i < k; i++) if (same_params(cand[i], m)) return cand[i];
    return NULL;
}

/* puts every class after its base class and interfaces; finds cycles */
static void visit(C* c, int i, int* n) {
    CClass* k = &c->classes[i];
    if (k->state == 2) return;
    if (k->state == 1) cfail(c, k->decl->line, "'%.*s' derives from itself (a cycle of base classes)", k->nlen, k->name);
    k->state = 1;
    if (k->base >= 0) visit(c, k->base, n);
    for (int j = 0; j < k->nifaces; j++) visit(c, k->ifaces[j], n);
    k->state = 2;
    c->order[(*n)++] = i;
}

/* the value of an enum member: a whole number made of literals, earlier members and members of other enums */
static int32_t const_int(C* c, Expr* e, int ecls) {
    switch (e->k) {
        case E_INT: case E_CHAR: return e->ival;
        case E_CAST: if (!e->tn) return const_int(c, e->a, ecls); break;
        case E_UNARY: {
            int32_t v = const_int(c, e->a, ecls);
            if (e->op == TK_MINUS) return (int32_t)(0u - (uint32_t)v);
            if (e->op == TK_TILDE) return ~v;
            break;
        }
        case E_BINARY: {
            uint32_t a = (uint32_t)const_int(c, e->a, ecls), b = (uint32_t)const_int(c, e->b, ecls);
            switch (e->op) {
                case TK_PLUS: return (int32_t)(a + b);
                case TK_MINUS: return (int32_t)(a - b);
                case TK_STAR: return (int32_t)(a * b);
                case TK_SLASH: case TK_PERCENT:
                    if (!b) cfail(c, e->line, "division by zero");
                    return e->op == TK_SLASH ? (int32_t)a / (int32_t)b : (int32_t)a % (int32_t)b;
                case TK_AMP: return (int32_t)(a & b);
                case TK_PIPE: return (int32_t)(a | b);
                case TK_CARET: return (int32_t)(a ^ b);
                case TK_SHL: return (int32_t)(a << (b & 31));
                case TK_SHR: return (int32_t)a >> (b & 31);
            }
            break;
        }
        case E_NAME: {
            CClass* k = &c->classes[ecls];
            for (int i = 0; i < k->nf; i++) if (nameeq(k->fields[i].name, k->fields[i].nlen, e->name, e->nlen)) return k->fields[i].value;
            cfail(c, e->line, "'%.*s' is not an earlier member of enum '%.*s'", e->nlen, e->name, k->nlen, k->name);
        }
        case E_MEMBER:
            {
                int ci = enum_of(c, e->a);
                if (ci >= 0) {
                    CClass* k = &c->classes[ci];
                    for (int i = 0; i < k->nf; i++) if (nameeq(k->fields[i].name, k->fields[i].nlen, e->name, e->nlen)) return k->fields[i].value;
                    cfail(c, e->line, "enum '%.*s' has no member '%.*s' (yet: it must come earlier in the program)", k->nlen, k->name, e->nlen, e->name);
                }
            }
            break;
    }
    cfail(c, e->line, "an enum value must be a whole number, e.g. A = 4 or B = A | C");
}

/* ---------- struct parameters that a method never changes ---------- */
static int root_is(Expr* e, const char* n, int l) {
    while (e && (e->k == E_MEMBER || e->k == E_INDEX)) e = e->a;
    return e && e->k == E_NAME && nameeq(e->name, e->nlen, n, l);
}
static int uses_name(Expr* e, const char* n, int l);
static int uses_name_s(Stmt* s, const char* n, int l) {
    if (!s) return 0;
    if (uses_name(s->e, n, l) || uses_name(s->step, n, l)) return 1;
    for (int i = 0; i < s->n; i++) if (uses_name_s(s->list[i], n, l)) return 1;
    for (int i = 0; i < s->nlabels; i++) if (uses_name(s->labels[i], n, l)) return 1;
    return uses_name_s(s->body, n, l) || uses_name_s(s->els, n, l) || uses_name_s(s->init, n, l);
}
static int uses_name(Expr* e, const char* n, int l) {
    if (!e) return 0;
    if (e->k == E_NAME && nameeq(e->name, e->nlen, n, l)) return 1;
    if (uses_name(e->a, n, l) || uses_name(e->b, n, l) || uses_name(e->c, n, l) || uses_name_s(e->body, n, l)) return 1;
    for (int i = 0; i < e->nargs; i++) if (uses_name(e->args[i], n, l)) return 1;
    return 0;
}
/* may the code change the struct in variable n (assign it or its fields, call a method on it, 'out' it, use it in a lambda)? */
static int changes(Expr* e, const char* n, int l);
static int changes_s(Stmt* s, const char* n, int l) {
    if (!s) return 0;
    if ((s->k == S_VAR || s->k == S_FOREACH) && nameeq(s->name, s->nlen, n, l)) return 1;     /* another variable with that name */
    if (changes(s->e, n, l) || changes(s->step, n, l)) return 1;
    for (int i = 0; i < s->n; i++) if (changes_s(s->list[i], n, l)) return 1;
    for (int i = 0; i < s->nlabels; i++) if (changes(s->labels[i], n, l)) return 1;
    if (s->k == S_VAR) for (Stmt* v = s->els; v; v = v->els) if (nameeq(v->name, v->nlen, n, l) || changes(v->e, n, l)) return 1;
    return changes_s(s->body, n, l) || (s->k != S_VAR && changes_s(s->els, n, l)) || changes_s(s->init, n, l);
}
static int changes(Expr* e, const char* n, int l) {
    if (!e) return 0;
    switch (e->k) {
        case E_ASSIGN: case E_INCDEC: if (root_is(e->a, n, l)) return 1; break;
        case E_CALL: if (e->a->k == E_MEMBER && root_is(e->a->a, n, l)) return 1; break;
        case E_OUT: if ((e->a && root_is(e->a, n, l)) || (e->prefix && nameeq(e->name, e->nlen, n, l))) return 1; break;
        case E_IS: if (e->name && nameeq(e->name, e->nlen, n, l)) return 1; break;
        case E_LAMBDA: return uses_name(e->a, n, l) || uses_name_s(e->body, n, l);
    }
    if (changes(e->a, n, l) || changes(e->b, n, l) || changes(e->c, n, l)) return 1;
    for (int i = 0; i < e->nargs; i++) if (changes(e->args[i], n, l)) return 1;
    return 0;
}
static uint32_t ro_params(C* c, MethodDecl* md) {
    uint32_t ro = 0;
    if (!md->body) return 0;
    for (int i = 0; i < md->nparams && i < 32; i++) {
        if (resolve_typenode(c, md->params[i].tn, 0)->k != T_STRUCT) continue;
        const char* n = md->params[i].name; int l = md->params[i].nlen;
        if (!changes_s(md->body, n, l) && !(md->baseCall && changes(md->baseCall, n, l))) ro |= 1u << i;
    }
    return ro;
}

/* does struct s hold (through its struct fields) struct target? */
static int struct_contains(C* c, int s, int target, int depth) {
    if (depth > 64) return 1;
    CClass* k = &c->classes[s];
    for (int i = 0; i < k->nf; i++) {
        Type* t = k->fields[i].type;
        if (k->fields[i].isStatic || t->k != T_STRUCT) continue;
        if (t->cls == target || struct_contains(c, t->cls, target, depth + 1)) return 1;
    }
    return 0;
}

static void collect(C* c, ProgramAST* ast) {
    Program* P = c->P;
    int nc = ast->nclasses;
    c->nclasses = nc;
    c->classes = (CClass*)arena_alloc(&c->A, sizeof(CClass) * (nc ? nc : 1));
    for (int i = 0; i < nc; i++) {
        ClassDecl* d = &ast->classes[i];
        CClass* k = &c->classes[i];
        if (!d->ns.s) d->ns.s = "";              /* classes made by the compiler: global */
        k->name = d->name; k->nlen = d->nlen; k->decl = d;
        k->isInterface = d->isInterface; k->isAbstract = d->isAbstract || d->isInterface;
        k->isEnum = d->isEnum; k->isStruct = d->isStruct;
        k->base = -1;
        k->type = (Type*)arena_alloc(&c->A, sizeof(Type));
        k->type->k = d->isEnum ? T_ENUM : d->isStruct ? T_STRUCT : T_CLASS; k->type->cls = i;
        if (d->nlen >= 32) cfail(c, d->line, "class name too long");
        /* a user class may reuse a built-in name (e.g. 'Game'); the user's class wins */
        for (int j = 0; j < i; j++)
            if (class_in(c, j, d->ns.s, d->ns.l, d->name, d->nlen))
                cfail(c, d->line, "class '%s%s%.*s' is defined twice", d->ns.s, d->ns.l ? "." : "", d->nlen, d->name);
    }

    /* base class and interfaces */
    for (int i = 0; i < nc; i++) {
        c->scope = i;
        ClassDecl* d = &ast->classes[i];
        CClass* k = &c->classes[i];
        k->ifaces = (int*)arena_alloc(&c->A, sizeof(int) * (d->nbases ? d->nbases : 1));
        if (k->isStruct && d->nbases) cfail(c, d->bases[0]->line, "a struct cannot derive from a class or implement an interface here; use a class");
        for (int j = 0; j < d->nbases; j++) {
            TypeNode* b = d->bases[j];
            int bi = find_class(c, b->name, b->nlen);
            if (bi < 0) cfail(c, b->line, "unknown class or interface '%.*s'%s", b->nlen, b->name, class_hint(c, b->name, b->nlen));
            if (bi == i) cfail(c, b->line, "'%.*s' cannot derive from itself", d->nlen, d->name);
            CClass* bk = &c->classes[bi];
            if (bk->isEnum || bk->isStruct) cfail(c, b->line, "cannot derive from %s '%.*s'", bk->isEnum ? "enum" : "struct", b->nlen, b->name);
            if (!bk->isInterface) {
                if (k->isInterface) cfail(c, b->line, "an interface can only extend interfaces, and '%.*s' is a class", b->nlen, b->name);
                if (j > 0) cfail(c, b->line, "a class has only one base class, and it must come first after ':' ('%.*s')", b->nlen, b->name);
                k->base = bi;
            } else {
                for (int q = 0; q < k->nifaces; q++)
                    if (k->ifaces[q] == bi) cfail(c, b->line, "'%.*s' is listed twice", b->nlen, b->name);
                k->ifaces[k->nifaces++] = bi;
            }
        }
    }
    c->order = (int*)arena_alloc(&c->A, sizeof(int) * (nc ? nc : 1));
    int n = 0;
    for (int i = 0; i < nc; i++) visit(c, i, &n);
    for (int oi = 0; oi < nc; oi++) {
        int i = c->order[oi];
        c->scope = i;
        CClass* k = &c->classes[i];
        uint8_t* isa = (uint8_t*)arena_alloc(&c->A, nc ? nc : 1);
        isa[i] = 1;
        if (k->base >= 0) for (int q = 0; q < nc; q++) isa[q] |= c->classes[k->base].type->isa[q];
        for (int j = 0; j < k->nifaces; j++) for (int q = 0; q < nc; q++) isa[q] |= c->classes[k->ifaces[j]].type->isa[q];
        k->type->isa = isa;
    }

    int nfuncs = 1 + 3 + ast->nlambdas;  /* static init, the Main/Update/Draw dispatchers, lambdas */
    for (int i = 0; i < nc; i++) nfuncs += 1 + ast->classes[i].nmethods;
    P->funcs = (Func*)calloc(nfuncs, sizeof(Func));
    P->capfuncs = nfuncs;
    P->classes = (ClassRT*)calloc((size_t)(nc > 0 ? nc : 1), sizeof(ClassRT));
    P->nclasses = nc;

    /* enum values, in the order of the program (a member may use members of enums before it) */
    for (int i = 0; i < nc; i++) {
        ClassDecl* d = &ast->classes[i];
        CClass* k = &c->classes[i];
        if (!k->isEnum) continue;
        c->scope = i;
        k->fields = (CField*)arena_alloc(&c->A, sizeof(CField) * (d->nfields ? d->nfields : 1));
        int32_t next = 0;
        for (int j = 0; j < d->nfields; j++) {
            FieldDecl* fd = &d->fields[j];
            for (int q = 0; q < k->nf; q++)
                if (nameeq(k->fields[q].name, k->fields[q].nlen, fd->name, fd->nlen)) cfail(c, fd->line, "enum member '%.*s' is defined twice", fd->nlen, fd->name);
            int32_t v = fd->init ? const_int(c, fd->init, i) : next;
            CField* f = &k->fields[k->nf++];
            f->name = fd->name; f->nlen = fd->nlen; f->isStatic = 1; f->decl = fd; f->type = k->type; f->value = v; f->slot = -1;
            next = (int32_t)((uint32_t)v + 1u);
        }
    }

    /* fields: a derived class's instance fields come after its base class's */
    int nstatics = 0;
    for (int oi = 0; oi < nc; oi++) {
        int i = c->order[oi];
        c->scope = i;
        ClassDecl* d = &ast->classes[i];
        CClass* k = &c->classes[i];
        if (k->isEnum) continue;
        k->ninst = k->base >= 0 ? c->classes[k->base].ninst : 0;
        k->fields = (CField*)arena_alloc(&c->A, sizeof(CField) * (d->nfields ? d->nfields : 1));
        for (int j = 0; j < d->nfields; j++) {
            FieldDecl* fd = &d->fields[j];
            for (int q = 0; q < k->nf; q++)
                if (nameeq(k->fields[q].name, k->fields[q].nlen, fd->name, fd->nlen)) cfail(c, fd->line, "field '%.*s' is defined twice", fd->nlen, fd->name);
            if (k->base >= 0 && find_field(c, k->base, fd->name, fd->nlen))
                cfail(c, fd->line, "field '%.*s' is already in base class '%.*s'; use a different name", fd->nlen, fd->name,
                      c->classes[k->base].nlen, c->classes[k->base].name);
            CField* f = &k->fields[k->nf++];
            f->name = fd->name; f->nlen = fd->nlen; f->isStatic = fd->isStatic; f->decl = fd;
            f->type = resolve_typenode(c, fd->tn, 0);
            f->slot = fd->isStatic ? nstatics++ : k->ninst++;
        }
    }
    for (int i = 0; i < nc; i++)
        if (c->classes[i].isStruct && struct_contains(c, i, i, 0))
            cfail(c, c->classes[i].decl->line, "struct '%.*s' holds itself (through its fields), so it would never end; use a class",
                  c->classes[i].nlen, c->classes[i].name);
    P->nstatics = nstatics;
    P->staticDefaults = (Value*)calloc(nstatics ? nstatics : 1, sizeof(Value));

    int fi = 0;
    for (int oi = 0; oi < nc; oi++) {
        int i = c->order[oi];
        c->scope = i;
        ClassDecl* d = &ast->classes[i];
        CClass* k = &c->classes[i];
        ClassRT* rt = &P->classes[i];
        snprintf(rt->name, sizeof rt->name, "%.*s", d->nlen, d->name);
        rt->nfields = k->ninst;
        rt->base = k->base;
        rt->isStruct = (uint8_t)k->isStruct; rt->isFlags = (uint8_t)d->isFlags;
        rt->defaults = (Value*)calloc(k->ninst ? k->ninst : 1, sizeof(Value));
        if (k->isEnum) {
            rt->nenum = k->nf;
            rt->enumVals = (int32_t*)malloc(sizeof(int32_t) * (k->nf ? k->nf : 1));
            rt->enumStr = (int32_t*)malloc(sizeof(int32_t) * (k->nf ? k->nf : 1));
            if (!rt->enumVals || !rt->enumStr) cfail(c, d->line, "out of memory");
            for (int j = 0; j < k->nf; j++) { rt->enumVals[j] = k->fields[j].value; rt->enumStr[j] = add_string(c, k->fields[j].name, k->fields[j].nlen); }
            k->methods = (CMethod*)arena_alloc(&c->A, sizeof(CMethod));
            continue;
        }
        if (k->base >= 0) memcpy(rt->defaults, P->classes[k->base].defaults, sizeof(Value) * c->classes[k->base].ninst);
        /* fields holding a struct (also inherited ones) get a zeroed struct when the object is made */
        int nsf = k->base >= 0 ? P->classes[k->base].nstructf : 0;
        for (int j = 0; j < k->nf; j++) if (!k->fields[j].isStatic && k->fields[j].type->k == T_STRUCT) nsf++;
        if (nsf) {
            rt->structfIdx = (int32_t*)malloc(sizeof(int32_t) * nsf);
            rt->structfCls = (int32_t*)malloc(sizeof(int32_t) * nsf);
            if (!rt->structfIdx || !rt->structfCls) cfail(c, d->line, "out of memory");
            if (k->base >= 0) {
                ClassRT* br = &P->classes[k->base];
                for (int j = 0; j < br->nstructf; j++) { rt->structfIdx[rt->nstructf] = br->structfIdx[j]; rt->structfCls[rt->nstructf++] = br->structfCls[j]; }
            }
            for (int j = 0; j < k->nf; j++)
                if (!k->fields[j].isStatic && k->fields[j].type->k == T_STRUCT) {
                    rt->structfIdx[rt->nstructf] = k->fields[j].slot; rt->structfCls[rt->nstructf++] = k->fields[j].type->cls;
                }
        }
        for (int j = 0; j < k->nf; j++) {
            CField* f = &k->fields[j];
            if (f->isStatic) P->staticDefaults[f->slot] = default_value(f->type);
            else rt->defaults[f->slot] = default_value(f->type);
        }
        int nctor = 0;
        for (int j = 0; j < d->nmethods; j++) nctor += d->methods[j].isCtor;
        k->methods = (CMethod*)arena_alloc(&c->A, sizeof(CMethod) * (d->nmethods ? d->nmethods : 1));
        k->ctors = (CMethod*)arena_alloc(&c->A, sizeof(CMethod) * (nctor ? nctor : 1));
        for (int j = 0; j < d->nmethods; j++) {
            MethodDecl* md = &d->methods[j];
            Type** params = (Type**)arena_alloc(&c->A, sizeof(Type*) * (md->nparams ? md->nparams : 1));
            for (int q = 0; q < md->nparams; q++) {
                params[q] = resolve_typenode(c, md->params[q].tn, 0);
                for (int r = 0; r < q; r++)
                    if (nameeq(md->params[r].name, md->params[r].nlen, md->params[q].name, md->params[q].nlen))
                        cfail(c, md->line, "parameter '%.*s' is listed twice", md->params[q].nlen, md->params[q].name);
            }
            if (md->isCtor) {
                if (md->isIter) cfail(c, md->line, "a constructor cannot use 'yield'");
                CMethod* ct = &k->ctors[k->nctors];
                memset(ct, 0, sizeof *ct);
                ct->name = k->name; ct->nlen = k->nlen; ct->ret = &tVoid; ct->params = params; ct->np = md->nparams;
                ct->owner = i; ct->sel = -1; ct->decl = md;
                for (int q = 0; q < k->nctors; q++)
                    if (same_params(&k->ctors[q], ct)) cfail(c, md->line, "two constructors of '%.*s' have the same parameters", k->nlen, k->name);
                ct->func = fi++;
                ct->roParams = ro_params(c, md);
                k->nctors++;
                continue;
            }
            if (find_field(c, i, md->name, md->nlen)) cfail(c, md->line, "'%.*s' is both a field and a method", md->nlen, md->name);
            CMethod* m = &k->methods[k->nm];
            memset(m, 0, sizeof *m);
            m->name = md->name; m->nlen = md->nlen; m->isStatic = md->isStatic; m->decl = md;
            m->params = params; m->np = md->nparams;
            for (int q = 0; q < k->nm; q++)
                if (nameeq(k->methods[q].name, k->methods[q].nlen, md->name, md->nlen) && same_params(&k->methods[q], m))
                    cfail(c, md->line, "method '%.*s' is defined twice with the same parameters", md->nlen, md->name);
            k->nm++;
            m->ret = resolve_typenode(c, md->ret, 0);
            m->isIter = md->isIter;
            if (m->isIter && m->ret->k != T_CORO)
                cfail(c, md->line, "'%.*s' uses 'yield', so it must return IEnumerator", md->nlen, md->name);
            m->owner = i; m->sel = -1;
            m->func = md->body ? fi++ : -1;
            m->roParams = md->body && !md->isIter ? ro_params(c, md) : 0;
            if (m->func >= 0) P->funcs[m->func].isIter = (uint8_t)m->isIter;
            if (k->isStruct && (md->kind == MK_VIRTUAL || md->kind == MK_ABSTRACT))
                cfail(c, md->line, "a struct cannot have virtual or abstract members (only 'override string ToString()')");
            /* virtual methods: the base method with the same name and parameters */
            CMethod* inh = k->base >= 0 ? find_same(c, k->base, m) : NULL;
            int isToString = namecs(md->name, md->nlen, "ToString");
            if (md->kind == MK_OVERRIDE) {
                if (isToString && (m->np || m->ret != &tString)) cfail(c, md->line, "write it as 'public override string ToString()'");
                if (inh) {
                    CClass* ik = &c->classes[inh->owner];
                    if (inh->sel < 0)
                        cfail(c, md->line, "cannot override '%.*s.%.*s' because it is not virtual; write 'public virtual ...' there",
                              ik->nlen, ik->name, md->nlen, md->name);
                    if (!same_sig(inh, m))
                        cfail(c, md->line, "'%.*s' must have the same return type and parameters as in '%.*s'", md->nlen, md->name, ik->nlen, ik->name);
                    m->sel = inh->sel;
                } else if (isToString) {        /* object.ToString() */
                    if (c->toStringSel < 0) { add_sel(c, m); c->toStringSel = m->sel; }
                    else m->sel = c->toStringSel;
                } else cfail(c, md->line, "'%.*s' has nothing to override: no base class of '%.*s' has a virtual method '%.*s'",
                             md->nlen, md->name, k->nlen, k->name, md->nlen, md->name);
            } else if (inh) {
                CClass* ik = &c->classes[inh->owner];
                cfail(c, md->line, "'%.*s' is already in base class '%.*s'; to replace it, make it 'virtual' (or 'abstract') there and 'override' here",
                      md->nlen, md->name, ik->nlen, ik->name);
            } else if (md->kind == MK_VIRTUAL || md->kind == MK_ABSTRACT) add_sel(c, m);
            else if (isToString && !m->isStatic && m->np == 0) cfail(c, md->line, "write it as 'public override string ToString()'");
        }
        if (!k->isInterface && !k->nctors) {    /* no constructor written: one without parameters */
            CMethod* ct = &k->ctors[k->nctors++];
            memset(ct, 0, sizeof *ct);
            ct->name = k->name; ct->nlen = k->nlen; ct->ret = &tVoid; ct->owner = i; ct->sel = -1; ct->func = fi++;
        }
    }
    P->staticInit = fi++;
    P->nfuncs = fi;

    /* properties: their get_X / set_X methods (made by lower_props) and hidden fields */
    for (int oi = 0; oi < nc; oi++) {
        int i = c->order[oi];
        c->scope = i;
        ClassDecl* d = &ast->classes[i];
        CClass* k = &c->classes[i];
        k->props = (CProp*)arena_alloc(&c->A, sizeof(CProp) * (d->nprops ? d->nprops : 1));
        for (int j = 0; j < d->nprops; j++) {
            PropDecl* pd = &d->props[j];
            if (find_field(c, i, pd->name, pd->nlen) || find_method(c, i, pd->name, pd->nlen))
                cfail(c, pd->line, "'%.*s' is used twice (a property and a field or method)", pd->nlen, pd->name);
            for (int q = 0; q < k->nprops; q++)
                if (nameeq(k->props[q].name, k->props[q].nlen, pd->name, pd->nlen)) cfail(c, pd->line, "property '%.*s' is defined twice", pd->nlen, pd->name);
            CProp* base = k->base >= 0 ? find_prop(c, k->base, pd->name, pd->nlen) : NULL;
            CProp* pp = &k->props[k->nprops++];
            memset(pp, 0, sizeof *pp);
            pp->name = pd->name; pp->nlen = pd->nlen; pp->isStatic = pd->isStatic; pp->owner = i; pp->decl = pd;
            pp->type = resolve_typenode(c, pd->tn, 0);
            char nm[80];
            snprintf(nm, sizeof nm, "get_%.*s", pd->nlen, pd->name);
            for (int q = 0; q < k->nm; q++) if (namecs(k->methods[q].name, k->methods[q].nlen, nm)) pp->get = &k->methods[q];
            nm[0] = 's';
            for (int q = 0; q < k->nm; q++) if (namecs(k->methods[q].name, k->methods[q].nlen, nm)) pp->set = &k->methods[q];
            snprintf(nm, sizeof nm, "<%.*s>", pd->nlen, pd->name);
            for (int q = 0; q < k->nf; q++) if (namecs(k->fields[q].name, k->fields[q].nlen, nm)) pp->backing = &k->fields[q];
            if (base) {                         /* an override of only 'get' keeps the base property's 'set' (and the other way round) */
                if (base->type != pp->type) cfail(c, pd->line, "property '%.*s' must have the same type as in the base class", pd->nlen, pd->name);
                if (!pp->get) pp->get = base->get;
                if (!pp->set) pp->set = base->set;
            }
        }
    }

    /* vtables: what each selector calls for an object of this class */
    P->nsel = c->nsel;
    for (int oi = 0; oi < nc; oi++) {
        int i = c->order[oi];
        c->scope = i;
        CClass* k = &c->classes[i];
        int* vt = (int*)arena_alloc(&c->A, sizeof(int) * (c->nsel ? c->nsel : 1));
        for (int s = 0; s < c->nsel; s++) vt[s] = k->base >= 0 ? c->classes[k->base].vtable[s] : -1;
        for (int j = 0; j < k->nm; j++)
            if (k->methods[j].sel >= 0) vt[k->methods[j].sel] = k->methods[j].func >= 0 ? k->methods[j].func : -2;
        k->vtable = vt;
        if (!k->isInterface && !k->isEnum) {
            /* every method of every interface this class implements, directly or through its base class */
            for (int q = 0; q < nc; q++) {
                CClass* ik = &c->classes[q];
                if (!ik->isInterface || !k->type->isa[q]) continue;
                for (int j = 0; j < ik->nm; j++) {
                    CMethod* im = &ik->methods[j];
                    CMethod* impl = find_same(c, i, im);
                    int isAcc = !im->decl->s0 && im->nlen > 4 && im->name[3] == '_';    /* get_X / set_X of a property */
                    if (!impl || impl->isStatic) {
                        if (isAcc) cfail(c, k->decl->line, "class '%.*s' must have the property '%.*s' (with '%.3s') of interface '%.*s'",
                                         k->nlen, k->name, im->nlen - 4, im->name + 4, im->name, ik->nlen, ik->name);
                        cfail(c, k->decl->line, "class '%.*s' must have the method '%.*s' of interface '%.*s'",
                              k->nlen, k->name, im->nlen, im->name, ik->nlen, ik->name);
                    }
                    if (!same_sig(impl, im))
                        cfail(c, impl->decl->line, "'%.*s' must have the same return type and parameters as in interface '%.*s'",
                              isAcc ? im->nlen - 4 : im->nlen, isAcc ? im->name + 4 : im->name, ik->nlen, ik->name);
                    vt[im->sel] = impl->sel >= 0 ? vt[impl->sel] : impl->func;
                }
            }
            if (!k->isAbstract)
                for (int s = 0; s < c->nsel; s++)
                    if (vt[s] == -2) {
                        CMethod* am = c->sels[s];
                        CClass* ak = &c->classes[am->owner];
                        int acc = !am->decl->s0 && am->nlen > 4;
                        cfail(c, k->decl->line, "class '%.*s' must override the abstract %s '%.*s.%.*s' (or be 'abstract class')",
                              k->nlen, k->name, acc ? "property" : "method", ak->nlen, ak->name,
                              acc ? am->nlen - 4 : am->nlen, acc ? am->name + 4 : am->name);
                    }
        }
        ClassRT* rt = &P->classes[i];
        rt->vtable = (int*)malloc(sizeof(int) * (c->nsel ? c->nsel : 1));
        rt->isa = (uint8_t*)malloc(nc ? nc : 1);
        if (!rt->vtable || !rt->isa) cfail(c, 1, "out of memory");
        for (int s = 0; s < c->nsel; s++) rt->vtable[s] = vt[s] >= 0 ? vt[s] : -1;
        memcpy(rt->isa, k->type->isa, nc);
        rt->toString = c->toStringSel >= 0 ? rt->vtable[c->toStringSel] : -1;
    }
}

/* Properties become ordinary methods before anything else is collected: 'int Hp { get; set; }' gets a hidden field
   <Hp> and methods 'int get_Hp() { return <Hp>; }', 'void set_Hp(int value) { <Hp> = value; }'; a property with
   bodies gets methods with those bodies. Made-up declarations have no source span (s0 == NULL). */
static void lower_props(C* c, ProgramAST* ast) {
    for (int ci = 0; ci < ast->nclasses; ci++) {
        ClassDecl* cd = &ast->classes[ci];
        if (!cd->nprops) continue;
        FieldDecl* nf = (FieldDecl*)arena_alloc(&c->A, sizeof(FieldDecl) * (size_t)(cd->nfields + cd->nprops));
        MethodDecl* nm = (MethodDecl*)arena_alloc(&c->A, sizeof(MethodDecl) * (size_t)(cd->nmethods + 2 * cd->nprops));
        if (cd->nfields) memcpy(nf, cd->fields, sizeof(FieldDecl) * cd->nfields);
        if (cd->nmethods) memcpy(nm, cd->methods, sizeof(MethodDecl) * cd->nmethods);
        cd->fields = nf; cd->methods = nm;
        for (int j = 0; j < cd->nprops; j++) {
            PropDecl* pd = &cd->props[j];
            int isAuto = pd->kind != MK_ABSTRACT && !pd->getBody && !pd->setBody;
            char* bname = (char*)arena_alloc(&c->A, (size_t)pd->nlen + 3);
            sprintf(bname, "<%.*s>", pd->nlen, pd->name);
            if (isAuto) {
                FieldDecl* f = &cd->fields[cd->nfields++];
                memset(f, 0, sizeof *f);
                f->line = pd->line; f->isStatic = pd->isStatic; f->tn = pd->tn; f->name = bname; f->nlen = pd->nlen + 2; f->init = pd->init;
            }
            for (int acc = 0; acc < 2; acc++) {
                if (acc == 0 ? !pd->hasGet : !pd->hasSet) continue;
                MethodDecl* m = &cd->methods[cd->nmethods++];
                memset(m, 0, sizeof *m);
                m->line = pd->line; m->isStatic = pd->isStatic; m->kind = pd->kind;
                char* mname = (char*)arena_alloc(&c->A, (size_t)pd->nlen + 5);
                sprintf(mname, "%s_%.*s", acc == 0 ? "get" : "set", pd->nlen, pd->name);
                m->name = mname; m->nlen = pd->nlen + 4;
                if (acc == 0) m->ret = pd->tn;
                else {
                    TypeNode* v = (TypeNode*)arena_alloc(&c->A, sizeof(TypeNode));
                    v->base = TK_VOID; v->name = "void"; v->nlen = 4; v->line = pd->line;
                    m->ret = v;
                    m->params = (Param*)arena_alloc(&c->A, sizeof(Param));
                    m->params[0].tn = pd->tn; m->params[0].name = "value"; m->params[0].nlen = 5;
                    m->nparams = 1;
                }
                if (pd->kind == MK_ABSTRACT) continue;              /* no body */
                if (!isAuto) { m->body = acc == 0 ? pd->getBody : pd->setBody; continue; }
                Expr* field = (Expr*)arena_alloc(&c->A, sizeof(Expr));
                field->k = E_NAME; field->line = pd->line; field->name = bname; field->nlen = pd->nlen + 2;
                Stmt* one = (Stmt*)arena_alloc(&c->A, sizeof(Stmt));
                one->line = pd->line;
                if (acc == 0) { one->k = S_RETURN; one->e = field; }
                else {
                    Expr* val = (Expr*)arena_alloc(&c->A, sizeof(Expr));
                    val->k = E_NAME; val->line = pd->line; val->name = "value"; val->nlen = 5;
                    Expr* as = (Expr*)arena_alloc(&c->A, sizeof(Expr));
                    as->k = E_ASSIGN; as->op = TK_ASSIGN; as->line = pd->line; as->a = field; as->b = val;
                    one->k = S_EXPR; one->e = as;
                }
                Stmt* body = (Stmt*)arena_alloc(&c->A, sizeof(Stmt));
                body->k = S_BLOCK; body->line = pd->line;
                body->list = (Stmt**)arena_alloc(&c->A, sizeof(Stmt*));
                body->list[0] = one; body->n = 1;
                m->body = body;
            }
        }
    }
}

/* Every class may have 'static void Main()', 'static void Update()' and 'static void Draw()' (like the Unity scripts of one scene):
 * the program calls all of them, in the order of the classes in the project (files by name, classes top to bottom).
 * With more than one, a small function made here does the calls, so the hosts keep calling a single entry. */
static int entry_point(C* c, const char* name) {
    int fns[256], ords[256], n = 0;
    for (int i = 0; i < c->nclasses; i++) {
        CMethod* m = NULL;
        for (int j = 0; j < c->classes[i].nm && !m; j++) {
            CMethod* q = &c->classes[i].methods[j];
            if (namecs(q->name, q->nlen, name) && q->isStatic && q->np == 0 && q->ret->k == T_VOID) m = q;
        }
        if (!m) continue;
        if (n == 256) cfail(c, m->decl->line, "too many classes with 'static void %s()' (at most 256)", name);
        /* [Order(n)] on the class: smaller numbers first; equal ones keep the order of the classes (files by name) */
        int ord = c->classes[i].decl ? c->classes[i].decl->order : 0, k = n++;
        while (k > 0 && ords[k - 1] > ord) { fns[k] = fns[k - 1]; ords[k] = ords[k - 1]; k--; }
        fns[k] = m->func; ords[k] = ord;
    }
    if (n == 0) return -1;
    if (n == 1) return fns[0];
    Program* P = c->P;
    if (P->nfuncs >= P->capfuncs) cfail(c, 1, "internal error: no room for the '%s' dispatcher", name);
    int fi = P->nfuncs++;
    Func* f = &P->funcs[fi];
    int len = 3 * n + 1;
    f->code = (int32_t*)malloc(sizeof(int32_t) * len);
    f->lines = (int*)calloc(len, sizeof(int));
    if (!f->code || !f->lines) cfail(c, 1, "out of memory");
    int k = 0;
    for (int j = 0; j < n; j++) { f->code[k++] = OP_CALL; f->code[k++] = fns[j]; f->code[k++] = 0; }
    f->code[k++] = OP_RET;
    f->len = f->cap = len;
    f->nparams = 0; f->nlocals = 0;
    snprintf(f->name, sizeof f->name, "%s (all classes)", name);
    return fi;
}

/* Graph nodes with memory derive from Node. Unless the program has its own class Node (an A* graph node, say),
   one is added to the parsed program; it is parsed on its own, so the program's text and lines stay as they are. */
static void add_node_class(C* c, ProgramAST* ast) {
    for (int i = 0; i < ast->nclasses; i++) if (ast->classes[i].nlen == 4 && !memcmp(ast->classes[i].name, "Node", 4)) return;
    static const char NODE_CLASS[] = "abstract class Node { public virtual void Run() { } }";
    int n;
    Token* t = ms_lex(&c->pc, NODE_CLASS, &n);
    ProgramAST* extra = ms_parse(&c->pc, t, n);
    int cap = ast->nclasses;
    ast->classes = (ClassDecl*)arena_grow(&c->A, ast->classes, ast->nclasses, &cap, sizeof(ClassDecl));
    ast->classes[ast->nclasses++] = extra->classes[0];
}

static Program* compile(const char* src, char* err, int errlen, char* replDecls, int replDeclsCap) {
    C* c = (C*)calloc(1, sizeof(C));
    if (!c) { snprintf(err, errlen, "out of memory"); return NULL; }
    Type* prims[] = { &tVoid, &tInt, &tFloat, &tBool, &tString, &tNull, &tAny, &tChar, &tCoro };
    for (size_t i = 0; i < sizeof prims / sizeof prims[0]; i++) { prims[i]->arr = NULL; prims[i]->list = NULL; }
    c->toStringSel = -1;
    c->scope = -1;
    c->pc.A = &c->A; c->pc.jb = &c->jb; c->pc.err = err; c->pc.errlen = errlen;
    c->P = (Program*)calloc(1, sizeof(Program));
    err[0] = 0;
    if (setjmp(c->jb)) {
        ms_free_program(c->P);
        arena_free(&c->A);
        free(c);
        return NULL;
    }
    int ntok;
    Token* toks = ms_lex(&c->pc, src, &ntok);
    ProgramAST* ast = ms_parse(&c->pc, toks, ntok);
    add_node_class(c, ast);
    lower_props(c, ast);
    collect(c, ast);
    c->scope = -1;
    if (replDecls) {
        int ci = find_class(c, "Repl", 4);
        CMethod* m = ci >= 0 ? find_method(c, ci, "Main", 4) : NULL;
        if (m) c->replBody = m->decl->body;
        c->replDecls = replDecls; c->replDeclsCap = replDeclsCap;
        for (int i = 0; i < ms_nnatives; i++)
            if (!strcmp(ms_natives[i].cls, "Console") && !strcmp(ms_natives[i].name, "WriteLine") && ms_natives[i].np == 1) c->replPrint = i;
    }
    for (int i = 0; i < c->nclasses; i++) {
        if (c->classes[i].isInterface || c->classes[i].isEnum) continue;
        for (int j = 0; j < c->classes[i].nctors; j++) compile_ctor(c, i, &c->classes[i].ctors[j]);
        for (int j = 0; j < c->classes[i].nm; j++)
            if (c->classes[i].methods[j].func >= 0) compile_method(c, i, &c->classes[i].methods[j]);
    }
    compile_static_init(c, c->P->staticInit);
    c->P->entryMain = entry_point(c, "Main");
    c->P->entryUpdate = entry_point(c, "Update");
    c->P->entryDraw = entry_point(c, "Draw");
    if (c->P->entryMain < 0 && c->P->entryUpdate < 0)
        cfail(c, 1, "no entry point: add 'static void Main()' or 'static void Update()'");
    Program* P = c->P;
    arena_free(&c->A);
    free(c);
    return P;
}

Program* ms_compile(const char* src, char* err, int errlen) { return compile(src, err, errlen, NULL, 0); }
Program* ms_compile_repl(const char* src, char* err, int errlen, char* decls, int declsCap) { return compile(src, err, errlen, decls, declsCap); }

void ms_free_program(Program* p) {
    if (!p) return;
    for (int i = 0; i < p->capfuncs; i++) { free(p->funcs[i].code); free(p->funcs[i].lines); }
    free(p->funcs);
    for (int i = 0; i < p->nstrings; i++) free(p->strings[i]);
    free(p->strings);
    for (int i = 0; i < p->nclasses; i++) {
        ClassRT* k = &p->classes[i];
        free(k->defaults); free(k->vtable); free(k->isa);
        free(k->structfIdx); free(k->structfCls); free(k->enumVals); free(k->enumStr);
    }
    free(p->classes);
    free(p->staticDefaults);
    free(p);
}
