/* MiniSharp compiler - types, bytecode emission, name lookups and locals. */
#include "compiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

Type tVoid = {.k = T_VOID}, tInt = {.k = T_INT}, tFloat = {.k = T_FLOAT}, tBool = {.k = T_BOOL}, tString = {.k = T_STRING},
     tNull = {.k = T_NULL}, tAny = {.k = T_ANY}, tChar = {.k = T_CHAR}, tCoro = {.k = T_CORO};

void cfail(C* c, int line, const char* fmt, ...) {
    char msg[220];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    snprintf(c->pc.err, c->pc.errlen, "line %d: %s", line, msg);
    longjmp(c->jb, 1);
}

int nameeq(const char* a, int al, const char* b, int bl) { return al == bl && !memcmp(a, b, al); }
int namecs(const char* a, int al, const char* s) { return nameeq(a, al, s, (int)strlen(s)); }

Type* array_of(C* c, Type* t) {
    if (!t->arr) {
        Type* a = (Type*)arena_alloc(&c->A, sizeof(Type));
        a->k = T_ARRAY; a->elem = t;
        t->arr = a;
    }
    return t->arr;
}
Type* array2_of(C* c, Type* t) {                   /* T[,] */
    if (!t->arr2) {
        Type* a = (Type*)arena_alloc(&c->A, sizeof(Type));
        a->k = T_ARRAY; a->elem = t; a->rank = 2;
        t->arr2 = a;
    }
    return t->arr2;
}
static Type* dict_of(C* c, Type* k, Type* v) {
    for (Type* d = c->dicts; d; d = d->next) if (d->key == k && d->elem == v) return d;
    Type* d = (Type*)arena_alloc(&c->A, sizeof(Type));
    d->k = T_DICT; d->key = k; d->elem = v; d->next = c->dicts; c->dicts = d;
    Type* p = (Type*)arena_alloc(&c->A, sizeof(Type));
    p->k = T_KVP; p->key = k; p->elem = v;
    d->kvp = p;
    return d;
}
Type* func_of(C* c, Type** ps, int np, Type* ret) {
    for (Type* f = c->funcs; f; f = f->next)
        if (f->np == np && f->elem == ret && (!np || !memcmp(f->params, ps, sizeof(Type*) * np))) return f;
    Type* f = (Type*)arena_alloc(&c->A, sizeof(Type));
    f->k = T_FUNC; f->elem = ret; f->np = np;
    f->params = (Type**)arena_alloc(&c->A, sizeof(Type*) * (np ? np : 1));
    if (np) memcpy(f->params, ps, sizeof(Type*) * np);
    f->next = c->funcs; c->funcs = f;
    return f;
}
Type* list_of(C* c, Type* t) {
    if (!t->list) {
        Type* l = (Type*)arena_alloc(&c->A, sizeof(Type));
        l->k = T_LIST; l->elem = t;
        t->list = l;
    }
    return t->list;
}

const char* type_name(C* c, Type* t) {
    static char bufs[6][96]; static int bi = 0;
    char* b = bufs[bi = (bi + 1) % 6];
    switch (t->k) {
        case T_VOID: return "void"; case T_INT: return "int"; case T_FLOAT: return "float";
        case T_BOOL: return "bool"; case T_STRING: return "string"; case T_NULL: return "null";
        case T_ANY: return "any"; case T_CHAR: return "char"; case T_CORO: return "IEnumerator";
        case T_LIST: snprintf(b, 96, "List<%s>", type_name(c, t->elem)); return b;
        case T_DICT: snprintf(b, 96, "Dictionary<%s, %s>", type_name(c, t->key), type_name(c, t->elem)); return b;
        case T_KVP: snprintf(b, 96, "KeyValuePair<%s, %s>", type_name(c, t->key), type_name(c, t->elem)); return b;
        case T_CLASS: case T_ENUM: case T_STRUCT: snprintf(b, 96, "%.*s", c->classes[t->cls].nlen, c->classes[t->cls].name); return b;
        case T_ARRAY: snprintf(b, 96, t->rank == 2 ? "%s[,]" : "%s[]", type_name(c, t->elem)); return b;
        case T_FUNC: {
            int isAction = t->elem->k == T_VOID, n = snprintf(b, 96, isAction ? "Action" : "Func");
            for (int i = 0; i < t->np + (isAction ? 0 : 1) && n < 90; i++)
                n += snprintf(b + n, 96 - n, "%s%s", i ? ", " : "<", type_name(c, i < t->np ? t->params[i] : t->elem));
            if (t->np || !isAction) snprintf(b + n, 96 - n, ">");
            return b;
        }
    }
    return "?";
}

int type_isint(Type* t) { return t->k == T_INT || t->k == T_CHAR; }
int type_isnum(Type* t) { return type_isint(t) || t->k == T_FLOAT; }
int type_isobj(Type* t) { return t->k == T_CLASS || t->k == T_STRUCT; }     /* has fields, methods and properties */
int type_isref(Type* t) { return t->k == T_STRING || t->k == T_CLASS || t->k == T_ARRAY || t->k == T_LIST || t->k == T_DICT || t->k == T_CORO || t->k == T_NULL || t->k == T_FUNC; }

int assignable(Type* from, Type* to) {
    if (from == to) return 1;
    if (to->k == T_ANY) return from->k != T_VOID;
    if (from->k == T_INT && to->k == T_FLOAT) return 1;
    if (from->k == T_CHAR && (to->k == T_INT || to->k == T_FLOAT)) return 1;
    if (from->k == T_NULL && (to->k == T_STRING || to->k == T_CLASS || to->k == T_ARRAY || to->k == T_LIST || to->k == T_DICT || to->k == T_CORO || to->k == T_FUNC)) return 1;
    if (from->k == T_CLASS && to->k == T_CLASS) return from->isa[to->cls];     /* derived -> base, class -> interface */
    return 0;
}

Type* code_type(C* c, int k) {
    switch (k) {
        case T_INT: return &tInt; case T_FLOAT: return &tFloat; case T_BOOL: return &tBool;
        case T_STRING: return &tString; case T_ANY: return &tAny; case T_CHAR: return &tChar;
        case T_STRARR: return array_of(c, &tString);
        default: return &tVoid;
    }
}

/* a struct's default (a zeroed object) cannot be a constant: fields get it from OP_NEW (structf), statics from the static init */
Value default_value(Type* t) {
    Value v; v.t = V_NULL; v.as.i = 0;
    if (t->k == T_INT || t->k == T_ENUM) v.t = V_INT;
    else if (t->k == T_FLOAT) { v.t = V_FLOAT; v.as.f = 0; }
    else if (t->k == T_BOOL) v.t = V_BOOL;
    else if (t->k == T_CHAR) v.t = V_CHAR;
    return v;
}
int default_kind(Type* t) {
    if (t->k == T_STRUCT) return MS_KIND_STRUCT + t->cls;
    return t->k == T_INT || t->k == T_ENUM ? 1 : t->k == T_FLOAT ? 2 : t->k == T_BOOL ? 3 : t->k == T_CHAR ? 4 : 0;
}

/* ================= emission ================= */
void emit1(C* c, int32_t v) {
    Func* f = c->fn;
    if (f->len == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 64;
        f->code = (int32_t*)realloc(f->code, sizeof(int32_t) * f->cap);
        f->lines = (int*)realloc(f->lines, sizeof(int) * f->cap);
        if (!f->code || !f->lines) cfail(c, c->line, "out of memory");
    }
    f->code[f->len] = v; f->lines[f->len] = c->line; f->len++;
}
/* Superinstructions: a few frequent sequences become one instruction as they are emitted. Two instructions are
   joined only when no jump lands between them (lastLabel < len), so jumps into the middle cannot happen. */
void emit_op(C* c, int o) {
    Func* f = c->fn; int L = f->len;
    if ((o == OP_IADD || o == OP_ISUB) && c->lastOp >= 0 && c->lastOp == L - 2 && f->code[L - 2] == OP_ICONST && c->lastLabel < L) {
        f->code[L - 2] = OP_IADDK;                                  /* ICONST k; IADD -> IADDK k */
        if (o == OP_ISUB) f->code[L - 1] = (int32_t)(0u - (uint32_t)f->code[L - 1]);
        return;
    }
    c->prevOp2 = c->prevOp; c->prevOp = c->lastOp; c->lastOp = L;
    emit1(c, o);
}
void emit_op1(C* c, int o, int a) {
    Func* f = c->fn; int L = f->len;
    if (o == OP_GETFIELD && c->lastOp >= 0 && c->lastOp == L - 2 && f->code[L - 2] == OP_LOAD && c->lastLabel < L) {
        f->code[L - 2] = OP_GETFIELDL;                              /* LOAD s; GETFIELD i -> GETFIELDL s i */
        emit1(c, a);
        return;
    }
    emit_op(c, o); emit1(c, a);
}
void emit_op2(C* c, int o, int a, int b) { emit_op(c, o); emit1(c, a); emit1(c, b); }
int emit_jump(C* c, int o) {
    Func* f = c->fn; int L = f->len;
    if (o == OP_JZ && c->lastOp >= 0 && c->lastOp == L - 1 && c->lastLabel < L) {   /* a < b; JZ -> JNLT */
        int k = f->code[L - 1];
        int j = k == OP_ILT ? OP_JNLT : k == OP_ILE ? OP_JNLE : k == OP_IGT ? OP_JNGT : k == OP_IGE ? OP_JNGE :
                k == OP_IEQ ? OP_JNEQ : k == OP_INE ? OP_JNNE : -1;
        if (j >= 0) { f->code[L - 1] = j; emit1(c, 0); return f->len - 1; }
    }
    emit_op(c, o); emit1(c, 0); return c->fn->len - 1;
}
void patch_jump(C* c, int pos) { c->fn->code[pos] = c->fn->len - (pos + 1); c->lastLabel = c->fn->len; }
void emit_jump_back(C* c, int o, int target) { emit_op(c, o); emit1(c, target - (c->fn->len + 1)); }
int code_label(C* c) { c->lastLabel = c->fn->len; return c->fn->len; }

void emit_pop(C* c) {
    Func* f = c->fn;
    if (c->lastOp >= 0 && c->lastOp == f->len - 2 && f->code[c->lastOp] == OP_STORE && c->lastLabel != f->len) {
        int L = f->len, s = f->code[L - 1], P = c->prevOp2;
        /* LOAD s; IADDK k; STORE s; POP -> INCL s k  (i++; i += 2; i--;) */
        if (c->prevOp == L - 4 && f->code[L - 4] == OP_IADDK && P == L - 6 && f->code[P] == OP_LOAD && f->code[P + 1] == s && c->lastLabel <= P) {
            int32_t k = f->code[L - 3];
            f->code[P] = OP_INCL; f->code[P + 1] = s; f->code[P + 2] = k;
            f->len = P + 3;
            c->lastOp = c->prevOp = c->prevOp2 = -1;
            return;
        }
        f->code[c->lastOp] = OP_STOREP;
        c->lastOp = c->prevOp = c->prevOp2 = -1;
        return;
    }
    emit_op(c, OP_POP);
}

void emit_fconst(C* c, float v) { int32_t bits; memcpy(&bits, &v, 4); emit_op1(c, OP_FCONST, bits); }

int add_string(C* c, const char* s, int len) {
    Program* P = c->P;
    for (int i = 0; i < P->nstrings; i++)
        if (P->strings[i]->len == len && !memcmp(P->strings[i]->chars, s, len)) return i;
    if (P->nstrings == P->capstrings) {
        P->capstrings = P->capstrings ? P->capstrings * 2 : 16;
        P->strings = (ObjString**)realloc(P->strings, sizeof(ObjString*) * P->capstrings);
    }
    ObjString* o = (ObjString*)malloc(sizeof(ObjString) + len + 1);
    o->h.type = O_STRING; o->h.marked = 0; o->h.pinned = 1; o->h.next = NULL;
    o->len = len; o->ulen = -1; o->cacheI = o->cacheB = 0; memcpy(o->chars, s, len); o->chars[len] = 0;
    P->strings[P->nstrings] = o;
    return P->nstrings++;
}

void push_default(C* c, Type* t) {
    switch (t->k) {
        case T_INT: case T_ENUM: emit_op1(c, OP_ICONST, 0); break;
        case T_STRUCT: emit_op1(c, OP_NEW, t->cls); break;
        case T_FLOAT: emit_fconst(c, 0.0f); break;
        case T_BOOL: emit_op(c, OP_FALSE); break;
        case T_CHAR: emit_op1(c, OP_ICONST, 0); emit_op(c, OP_I2C); break;
        default: emit_op(c, OP_NULL); break;
    }
}

/* ================= lookups ================= */
/* fields and methods are looked up in the class, then its base classes */
CField* find_field(C* c, int cls, const char* n, int l) {
    for (; cls >= 0; cls = c->classes[cls].base) {
        CClass* k = &c->classes[cls];
        for (int i = 0; i < k->nf; i++) if (nameeq(k->fields[i].name, k->fields[i].nlen, n, l)) return &k->fields[i];
    }
    return NULL;
}
static CMethod* find_own_method(C* c, int cls, const char* n, int l) {
    CClass* k = &c->classes[cls];
    for (int i = 0; i < k->nm; i++) if (nameeq(k->methods[i].name, k->methods[i].nlen, n, l)) return &k->methods[i];
    return NULL;
}
CMethod* find_method(C* c, int cls, const char* n, int l) {
    for (int k = cls; k >= 0; k = c->classes[k].base) {
        CMethod* m = find_own_method(c, k, n, l);
        if (m) return m;
    }
    if (c->classes[cls].isInterface)            /* an interface also has the methods of the interfaces it extends */
        for (int i = 0; i < c->classes[cls].nifaces; i++) {
            CMethod* m = find_method(c, c->classes[cls].ifaces[i], n, l);
            if (m) return m;
        }
    return NULL;
}
/* properties: the class and its bases; for an interface also the interfaces it extends */
CProp* find_prop(C* c, int cls, const char* n, int l) {
    for (int k = cls; k >= 0; k = c->classes[k].base) {
        CClass* kk = &c->classes[k];
        for (int i = 0; i < kk->nprops; i++) if (nameeq(kk->props[i].name, kk->props[i].nlen, n, l)) return &kk->props[i];
    }
    if (c->classes[cls].isInterface)
        for (int i = 0; i < c->classes[cls].nifaces; i++) {
            CProp* p = find_prop(c, c->classes[cls].ifaces[i], n, l);
            if (p) return p;
        }
    return NULL;
}
/* ---- access modifiers (the gentle version: a member without one is open to every class, as before) ---- */
/* may the code being compiled (class c->cur, its lambdas too) use a member of class owner with this access (AC_*)? */
int can_access(C* c, int owner, int access) {
    if (access == AC_PRIVATE) return c->cur == owner;
    if (access == AC_PROTECTED) {
        for (int k = c->cur; k >= 0; k = c->classes[k].base) if (k == owner) return 1;
        return 0;
    }
    return 1;
}
void check_access(C* c, int owner, int access, const char* n, int l, int line) {
    if (can_access(c, owner, access)) return;
    CClass* k = &c->classes[owner];
    if (access == AC_PRIVATE)
        cfail(c, line, "'%.*s' is private in %.*s; only code inside class %.*s can use it", l, n, k->nlen, k->name, k->nlen, k->name);
    cfail(c, line, "'%.*s' is protected in %.*s; only %.*s and classes derived from it can use it", l, n, k->nlen, k->name, k->nlen, k->name);
}
/* f (or NULL) after checking that the code being compiled may use it */
CField* use_field(C* c, CField* f, int line) {
    if (f && f->decl) check_access(c, f->owner, f->decl->access, f->name, f->nlen, line);
    return f;
}
int method_access(CMethod* m) { return m->decl ? m->decl->access : AC_NONE; }

/* ---- namespaces: a class has a short name and the namespace of its declaration (decl->ns, "" = global) ---- */
int class_in(C* c, int i, const char* ns, int nsl, const char* n, int l) {
    CClass* k = &c->classes[i];
    return nameeq(k->name, k->nlen, n, l) && nameeq(k->decl->ns.s, k->decl->ns.l, ns, nsl);
}
/* the namespace that contains ns ("Lib.Monsters" -> "Lib" -> ""): its length */
static int ns_parent(const char* ns, int l) {
    while (l > 0 && ns[l - 1] != '.') l--;
    return l > 0 ? l - 1 : 0;
}
/* the class with the full name ns + "." + n ("Lib" + "Monsters.Troll"); -1 if none */
static int find_qualified(C* c, const char* ns, int nsl, const char* n, int l) {
    char full[256];
    if (nsl + 1 + l >= (int)sizeof full) return -1;
    int fl = 0;
    if (nsl) { memcpy(full, ns, nsl); full[nsl] = '.'; fl = nsl + 1; }
    memcpy(full + fl, n, l); fl += l;
    int dot = fl - 1;
    while (dot >= 0 && full[dot] != '.') dot--;
    if (dot < 0) return -1;
    for (int i = 0; i < c->nclasses; i++) if (class_in(c, i, full, dot, full + dot + 1, fl - dot - 1)) return i;
    return -1;
}
/* the class a name means where c->scope is: the scope's namespace and the ones around it (the global one last),
 * then the namespaces of its 'using' (two of them with that name: ambiguous); -1 if none (maybe a built-in).
 * A name with dots is a full name (Lib.Item), relative to the namespaces around (Monsters.Troll inside Lib),
 * or an enum declared in a class (Player.State). */
int find_class(C* c, const char* n0, int l0) {
    char buf[256]; const char* n = n0; int l = 0;
    for (int i = 0; i < l0 && l < (int)sizeof buf - 1; i++)           /* "Lib . Item" -> "Lib.Item" */
        if (n0[i] != ' ' && n0[i] != '\t' && n0[i] != '\r' && n0[i] != '\n') buf[l++] = n0[i];
    if (l != l0) n = buf;
    NsName cur = { "", 0, NULL, 0 };
    ClassDecl* sd = c->scope >= 0 && c->scope < c->nclasses ? c->classes[c->scope].decl : NULL;
    if (sd) cur = sd->ns;
    /* 'using G = Lib.Monsters;': G.Troll is Lib.Monsters.Troll, a full name looked up from the global namespace */
    char abuf[256];
    if (sd) {
        int seg = 0;
        while (seg < l && n[seg] != '.') seg++;
        for (int u = 0; u < sd->nusings; u++) {
            NsName* a = &sd->usings[u];
            if (!a->alen || !nameeq(a->alias, a->alen, n, seg)) continue;
            if (a->l + (l - seg) >= (int)sizeof abuf) break;
            memcpy(abuf, a->s, a->l); memcpy(abuf + a->l, n + seg, l - seg);
            n = abuf; l = a->l + (l - seg);
            cur.s = ""; cur.l = 0;
            break;
        }
    }
    if (memchr(n, '.', l)) {
        for (int pl = cur.l;; pl = ns_parent(cur.s, pl)) {
            int ci = find_qualified(c, cur.s, pl, n, l);
            if (ci >= 0) return ci;
            if (!pl) break;
        }
        int dot = l - 1;                                               /* Lib.Player.State: an enum in class Lib.Player */
        while (n[dot] != '.') dot--;
        int outer = find_class(c, n, dot);
        if (outer < 0) return -1;
        NsName ons = c->classes[outer].decl->ns;
        for (int i = 0; i < c->nclasses; i++)
            if (c->classes[i].isEnum && class_in(c, i, ons.s, ons.l, n + dot + 1, l - dot - 1)) return i;
        return -1;
    }
    for (int pl = cur.l;; pl = ns_parent(cur.s, pl)) {
        for (int i = 0; i < c->nclasses; i++) if (class_in(c, i, cur.s, pl, n, l)) return i;
        if (!pl) break;
    }
    int found = -1;
    if (sd)
        for (int u = 0; u < sd->nusings; u++)
            for (int i = 0; i < c->nclasses && !sd->usings[u].alen; i++)    /* aliases do not open a namespace */
                if (i != found && class_in(c, i, sd->usings[u].s, sd->usings[u].l, n, l)) {
                    if (found >= 0)
                        cfail(c, c->line, "'%.*s' is ambiguous: %s.%.*s or %s.%.*s? write the full name",
                              l, n, c->classes[found].decl->ns.s, l, n, sd->usings[u].s, l, n);
                    found = i;
                }
    /* code outside any namespace (older projects, files made from graphs) may use a class of any namespace
     * when the name is unique and not a built-in one: before namespaces existed, every class was visible */
    if (found < 0 && !cur.l && !is_builtin_class(n, l)) {
        int k = 0;
        for (int i = 0; i < c->nclasses; i++) if (nameeq(c->classes[i].name, c->classes[i].nlen, n, l)) { found = i; k++; }
        if (k != 1) found = -1;
    }
    return found;
}
/* A registered source class with the name of a built-in class extends it (lib/console.c: Console.ForegroundColor
 * written in MiniSharp next to the C functions WriteLine...): a member the class does not have is the built-in one. */
void builtin_fallback(C* c, Res* r, const char* cls, int clen, const char* m, int ml) {
    if (r->k != R_CLASS || !is_builtin_class(cls, clen)) return;
    if (find_field(c, r->idx, m, ml) || find_prop(c, r->idx, m, ml) || find_method(c, r->idx, m, ml)) return;
    r->k = R_BUILTIN;
}
/* for 'unknown class X' errors: " (there is Lib.X: add 'using Lib;')" or "" */
const char* class_hint(C* c, const char* n, int l) {
    static char buf[160];
    buf[0] = 0;
    for (int i = 0; i < c->nclasses; i++) {
        CClass* k = &c->classes[i];
        if (k->decl && k->decl->ns.l && nameeq(k->name, k->nlen, n, l)) {
            snprintf(buf, sizeof buf, " (there is %s.%.*s: add 'using %s;' or write the full name)", k->decl->ns.s, l, n, k->decl->ns.s);
            break;
        }
    }
    return buf;
}
int is_builtin_class(const char* n, int l) {
    for (int i = 0; i < ms_nnatives; i++) if (namecs(n, l, ms_natives[i].cls)) return 1;
    for (int i = 0; i < ms_nconsts; i++) if (namecs(n, l, ms_consts[i].cls)) return 1;
    return 0;
}
int find_local(C* c, const char* n, int l) {
    for (int i = c->nlocals - 1; i >= 0; i--) if (nameeq(c->locals[i].name, c->locals[i].nlen, n, l)) return i;
    return -1;
}

/* like C# (CS1656): the variable of a foreach cannot be assigned, incremented or passed with ref / out */
void check_writable_local(C* c, const char* n, int l, int line) {
    int li = find_local(c, n, l);
    if (li >= 0 && c->locals[li].readOnly)
        cfail(c, line, "cannot assign to '%.*s': it is the variable of a 'foreach' loop (to change the elements, use 'for' with an index)", l, n);
}

/* index of the captured local li among the cells of lambda lc (adding it, and to the lambdas around it) */
static int get_upvalue(C* c, LamCtx* lc, int li) {
    for (int k = 0; k < lc->nup; k++) if (lc->ups[k].li == li) return k;
    if (lc->nup >= 64) cfail(c, c->line, "a lambda can use at most 64 variables from outside");
    int from = c->locals[li].depth < lc->depth - 1 ? get_upvalue(c, lc->parent, li) : -1;
    lc->ups[lc->nup].li = li; lc->ups[lc->nup].from = from;
    return lc->nup++;
}

Res resolve_name(C* c, const char* n, int l) {
    Res r = { R_NONE, 0, NULL, NULL, NULL };
    int li = find_local(c, n, l);
    if (li >= 0) {
        Local* lo = &c->locals[li];
        r.type = lo->type;
        if (lo->depth == c->fdepth) { r.k = lo->boxed ? R_CELL : R_LOCAL; r.idx = lo->slot; return r; }
        if (!lo->boxed) cfail(c, c->line, "'%.*s' cannot be used inside a lambda; copy it to a variable first", l, n);
        if (lo->refMode) cfail(c, c->line, "'%s' parameter '%.*s' cannot be used inside a lambda; copy it to a variable first", lo->refMode == PM_REF ? "ref" : "out", l, n);
        r.k = R_UPVAL; r.idx = get_upvalue(c, c->lam, li);
        return r;
    }
    for (int i = 0; i < c->nreplVars; i++)
        if (nameeq(c->replVars[i].name, c->replVars[i].nlen, n, l)) { r.k = R_STATIC; r.idx = c->replVars[i].slot; r.type = c->replVars[i].type; return r; }
    CField* f = use_field(c, find_field(c, c->cur, n, l), c->line);
    if (f) { r.k = f->isStatic ? R_STATIC : R_FIELD; r.idx = f->slot; r.type = f->type; r.field = f; return r; }
    CProp* pp = find_prop(c, c->cur, n, l);
    if (pp) { r.k = R_PROP; r.prop = pp; r.type = pp->type; return r; }
    if (find_method(c, c->cur, n, l)) { r.k = R_METHOD; return r; }
    int ci = find_class(c, n, l);
    if (ci >= 0) { r.k = R_CLASS; r.idx = ci; return r; }
    if (is_builtin_class(n, l)) { r.k = R_BUILTIN; return r; }
    return r;
}

/* Lib.Item.Count(), Lib.Dir.Up: if member access m is reached through a chain of plain names that does not
 * start with a variable, field or class but makes the full name of a class, that part becomes one name */
void fold_ns(C* c, Expr* m) {
    if (!m || m->k != E_MEMBER || m->prefix || !m->a || m->a->k != E_MEMBER || m->a->prefix) return;
    Expr* parts[32]; int np = 0;
    Expr* r = m->a;
    while (r->k == E_MEMBER && !r->prefix && np < 32) { parts[np++] = r; r = r->a; }
    if (r->k != E_NAME || np == 32) return;
    if (resolve_name(c, r->name, r->nlen).k != R_NONE) return;
    char buf[256]; int len = r->nlen;
    if (len >= (int)sizeof buf) return;
    memcpy(buf, r->name, len);
    for (int i = np - 1; i >= 0; i--) {                        /* the shortest part that is a class: Lib.Item */
        if (len + 1 + parts[i]->nlen >= (int)sizeof buf) return;
        buf[len++] = '.'; memcpy(buf + len, parts[i]->name, parts[i]->nlen); len += parts[i]->nlen;
        if (find_class(c, buf, len) >= 0) {
            char* s = (char*)arena_alloc(&c->A, len + 1);
            memcpy(s, buf, len); s[len] = 0;
            Expr* e = parts[i];
            e->k = E_NAME; e->name = s; e->nlen = len; e->a = NULL;
            return;
        }
    }
}

/* ================= locals ================= */
int declare_local(C* c, const char* n, int l, Type* t, int line) {
    if (find_local(c, n, l) >= 0) cfail(c, line, "a variable named '%.*s' already exists here", l, n);
    if (c->nlocals >= 256) cfail(c, line, "too many local variables in one method");
    int slot = c->nextSlot++;
    if (c->nextSlot > c->fn->nlocals) c->fn->nlocals = c->nextSlot;
    Local* lo = &c->locals[c->nlocals++];
    lo->name = n; lo->nlen = l; lo->type = t; lo->slot = slot;
    lo->depth = c->fdepth;
    lo->boxed = 0; lo->refMode = 0; lo->readOnly = 0;
    for (int i = 0; i < c->caps.n; i++) if (nameeq(c->caps.v[i].n, c->caps.v[i].l, n, l)) lo->boxed = 1;
    c->lastBoxed = lo->boxed;
    return slot;
}
/* the value on top of the stack is the first value of a new local (in a fresh cell if a lambda uses it) */
void init_local(C* c, int slot, int boxed) {
    if (boxed) emit_op(c, OP_BOX);
    emit_op1(c, OP_STORE, slot);
    emit_pop(c);
}
/* parameters that lambdas use are moved into cells at the start of the function */
void box_params(C* c) {
    for (int i = 0; i < c->nlocals; i++)
        if (c->locals[i].depth == c->fdepth && c->locals[i].boxed) {
            emit_op1(c, OP_LOAD, c->locals[i].slot);
            init_local(c, c->locals[i].slot, 1);
        }
}
int alloc_temp(C* c) {
    int slot = c->nextSlot++;
    if (c->nextSlot > c->fn->nlocals) c->fn->nlocals = c->nextSlot;
    return slot;
}

Type* resolve_typenode(C* c, TypeNode* tn, int extraDims) {
    Type* t;
    switch (tn->base) {
        case TK_KINT: t = &tInt; break;
        case TK_KFLOAT: t = &tFloat; break;
        case TK_KBOOL: t = &tBool; break;
        case TK_KSTRING: t = &tString; break;
        case TK_KCHAR: t = &tChar; break;
        case TK_VOID: t = &tVoid; break;
        default: {
            if (namecs(tn->name, tn->nlen, "Dictionary") || namecs(tn->name, tn->nlen, "KeyValuePair")) {
                if (!tn->arg2 || tn->nargs > 2) cfail(c, tn->line, "write both types: %.*s<string, int>, ...", tn->nlen, tn->name);
                Type* kt = resolve_typenode(c, tn->arg, 0);
                Type* vt = resolve_typenode(c, tn->arg2, 0);
                if (kt->k == T_VOID || vt->k == T_VOID) cfail(c, tn->line, "a Dictionary cannot hold void");
                t = dict_of(c, kt, vt);
                if (tn->name[0] == 'K') t = t->kvp;
                break;
            }
            if (namecs(tn->name, tn->nlen, "List")) {
                if (!tn->arg) cfail(c, tn->line, "write the element type: List<int>, List<string>, ...");
                if (tn->nargs > 1) cfail(c, tn->line, "List takes one type: List<int>");
                Type* el = resolve_typenode(c, tn->arg, 0);
                if (el->k == T_VOID) cfail(c, tn->line, "cannot make a List of void");
                t = list_of(c, el);
                break;
            }
            int isFunc = namecs(tn->name, tn->nlen, "Func"), isAction = namecs(tn->name, tn->nlen, "Action");
            int isPred = namecs(tn->name, tn->nlen, "Predicate"), isCmp = namecs(tn->name, tn->nlen, "Comparison");
            if (isFunc || isAction || isPred || isCmp) {
                Type* ps[16]; int np = 0; Type* ret = &tVoid;
                if (tn->nargs > 16) cfail(c, tn->line, "too many type arguments");
                if (isFunc) {
                    if (!tn->nargs) cfail(c, tn->line, "write the types: Func<int> returns an int, Func<int, bool> takes an int and returns a bool");
                    for (int i = 0; i < tn->nargs - 1; i++) ps[np++] = resolve_typenode(c, tn->args[i], 0);
                    ret = resolve_typenode(c, tn->args[tn->nargs - 1], 0);
                    if (ret->k == T_VOID) cfail(c, tn->line, "a Func must return something; for no result use Action<...>");
                } else if (isAction) {
                    for (int i = 0; i < tn->nargs; i++) ps[np++] = resolve_typenode(c, tn->args[i], 0);
                } else {
                    if (tn->nargs != 1) cfail(c, tn->line, "%.*s takes one type: %.*s<int>", tn->nlen, tn->name, tn->nlen, tn->name);
                    Type* a = resolve_typenode(c, tn->args[0], 0);
                    ps[np++] = a;
                    if (isCmp) { ps[np++] = a; ret = &tInt; } else ret = &tBool;
                }
                for (int i = 0; i < np; i++) if (ps[i]->k == T_VOID) cfail(c, tn->line, "a parameter cannot be void");
                t = func_of(c, ps, np, ret);
                break;
            }
            if (tn->arg) cfail(c, tn->line, "'%.*s' is not generic (only List<T> is)", tn->nlen, tn->name);
            /* IEnumerator (iterator methods) and Coroutine (what StartCoroutine returns) are the same thing here */
            if (namecs(tn->name, tn->nlen, "IEnumerator") || namecs(tn->name, tn->nlen, "Coroutine")) { t = &tCoro; break; }
            int ci = find_class(c, tn->name, tn->nlen);         /* Item, Lib.Item, Player.State (an enum in a class) */
            if (ci < 0) cfail(c, tn->line, "unknown type '%.*s'%s", tn->nlen, tn->name, class_hint(c, tn->name, tn->nlen));
            t = c->classes[ci].type;
        }
    }
    int dims = tn->dims + extraDims;
    if (dims && t->k == T_VOID) cfail(c, tn->line, "cannot make an array of void");
    for (int i = 0; i < dims; i++) t = tn->rect && i == dims - 1 && !extraDims ? array2_of(c, t) : array_of(c, t);
    return t;
}

/* the enum that e names: Dir, or Player.State for an enum declared inside class Player; -1 if e is not one */
int enum_of(C* c, Expr* e) {
    if (e->k == E_NAME) {
        if (find_local(c, e->name, e->nlen) >= 0) return -1;
        int ci = find_class(c, e->name, e->nlen);
        return ci >= 0 && c->classes[ci].isEnum && !find_field(c, c->cur, e->name, e->nlen) ? ci : -1;
    }
    fold_ns(c, e);
    if (e->k == E_MEMBER && !e->prefix && e->a->k == E_NAME && find_local(c, e->a->name, e->a->nlen) < 0) {
        int outer = find_class(c, e->a->name, e->a->nlen), ci = find_class(c, e->name, e->nlen);
        if (outer >= 0 && ci >= 0 && c->classes[ci].isEnum && !find_field(c, outer, e->name, e->nlen) && !find_prop(c, outer, e->name, e->nlen)) return ci;
    }
    return -1;
}
