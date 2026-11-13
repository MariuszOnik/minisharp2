/* MiniSharp - inside of the compiler: compiler state and the functions its files share.
 * Only the comp_*.c files and compiler.c include it. */
#ifndef MS_COMPILER_H
#define MS_COMPILER_H

#include "vm.h"
#include "ast.h"
#include <setjmp.h>

typedef struct Type Type;
struct Type {
    int k; int cls;
    Type* elem;                     /* T[] / List<T>: element; Dictionary and KeyValuePair: value type */
    Type* key;                      /* Dictionary and KeyValuePair: key type */
    Type* arr; Type* list;          /* cached T[] and List<T> */
    Type* kvp; Type* next;          /* Dictionary: its KeyValuePair type; next Dictionary type in C.dicts */
    uint8_t* isa;                   /* class / interface: isa[k] = 1 if it can be used as class k (itself, bases, interfaces) */
    Type** params; int np;          /* Func / Action: parameter types; elem = return type (void for Action) */
};

extern Type tVoid, tInt, tFloat, tBool, tString,
            tNull, tAny, tChar, tCoro;

/* enum member: isStatic, slot unused, value = its number */
typedef struct { const char* name; int nlen; Type* type; int isStatic; int slot; FieldDecl* decl; int32_t value; } CField;
/* sel >= 0: virtual, abstract, override or interface method, called through the class's vtable[sel];
 * func < 0: abstract (no body). Constructors are CMethods too (in CClass.ctors). */
/* roParams: bit i set = struct parameter i is never changed by the method, so callers pass it without a copy */
typedef struct { const char* name; int nlen; int isStatic, isIter; Type* ret; Type** params; int np; int func, sel, owner; MethodDecl* decl; uint32_t roParams; } CMethod;
/* property: its get_X / set_X methods (NULL if it has none); backing: the hidden field of { get; set; } */
typedef struct { const char* name; int nlen; Type* type; int isStatic, owner; CMethod *get, *set; CField* backing; PropDecl* decl; } CProp;
typedef struct {
    const char* name; int nlen; Type* type;
    CField* fields; int nf; int ninst;          /* own fields; ninst counts the inherited ones too */
    CMethod* methods; int nm;                   /* own methods */
    CMethod* ctors; int nctors;                 /* constructors (one without parameters is made when there is none) */
    CProp* props; int nprops;                   /* own properties */
    ClassDecl* decl;
    int base;                                   /* base class or -1 */
    int* ifaces; int nifaces;                   /* interfaces listed after ':' (for an interface: the ones it extends) */
    int isInterface, isAbstract, isEnum, isStruct, state;   /* state: hierarchy walk (0 new, 1 visiting, 2 done) */
    int* vtable;                                /* [nsel]: function, -1 not there, -2 abstract */
} CClass;

/* depth: 0 for the method, 1 inside a lambda, 2 in a lambda inside that...
 * boxed: a lambda uses it, so the slot holds a cell (ObjCell) with the value */
typedef struct { const char* name; int nlen; Type* type; int slot; int depth, boxed; } Local;

/* the lambda being compiled: the cells it captures from the functions around it */
typedef struct LamCtx {
    int depth;
    struct { int li, from; } ups[64];   /* li: index in C.locals; from: upvalue of the parent lambda, or -1 = parent's local */
    int nup;
    struct LamCtx* parent;
} LamCtx;
typedef struct { const char* n; int l; } Name;
typedef struct { Name* v; int n, cap; } NameList;

typedef struct Loop {
    int isSwitch;                   /* 'break' leaves a switch too, 'continue' skips past it */
    int breaks[128], nb;
    int conts[128], nc;
    int contTarget;                 /* >= 0: continue jumps straight back here */
    struct Loop* prev;
} Loop;

typedef struct {
    ParseCtx pc; Arena A; jmp_buf jb;
    Program* P;
    CClass* classes; int nclasses;
    int cur, curStatic, isCtor, isIter;
    int scope;                      /* class whose namespace and 'using' decide what a class name means (-1: global) */
    Type* retType;
    Func* fn; int line;
    Local locals[256]; int nlocals; int nextSlot;
    Loop* loop;
    int lastOp, lastLabel;
    int prevOp, prevOp2;            /* the two instructions before lastOp (for superinstructions), -1 unknown */
    Type* dicts;                    /* every Dictionary<K,V> type made so far */
    int* order;                     /* classes with every base before the classes that derive from it */
    CMethod** sels; int nsel, capsel;   /* the method that introduced each virtual selector */
    int toStringSel;                /* selector of 'override string ToString()', or -1 */
    Type* funcs;                    /* every Func / Action type made so far */
    int fdepth; LamCtx* lam;        /* lambda nesting while compiling a method */
    NameList caps;                  /* names that lambdas in this method use from outside themselves */
    int lastBoxed;                  /* the local declare_local just made is boxed */
    int assignUnused;               /* the E_ASSIGN being compiled is a statement on its own */
    Type* stackType;                /* type of the E_STACK value (a?.b) */
    /* REPL: variables declared at the top of Repl.Main's body become static fields */
    Stmt* replBody;
    Local replVars[64]; int nreplVars;          /* slot = static slot */
    char* replDecls; int replDeclsCap;          /* "static int x; " for each of them */
    int replPrint;                              /* native Console.WriteLine(x) */
} C;

/* what a name resolves to (resolve_name) */
enum { R_NONE, R_LOCAL, R_FIELD, R_STATIC, R_CLASS, R_BUILTIN, R_METHOD, R_CELL /* boxed local */, R_UPVAL /* captured by this lambda */,
       R_PROP };
typedef struct { int k; int idx; Type* type; CProp* prop; CField* field; } Res;

void cfail(C* c, int line, const char* fmt, ...) __attribute__((noreturn));

/* types, emission, lookups, locals (comp_types.c) */
int nameeq(const char* a, int al, const char* b, int bl);
int namecs(const char* a, int al, const char* s);
Type* array_of(C* c, Type* t);
Type* func_of(C* c, Type** ps, int np, Type* ret);
Type* list_of(C* c, Type* t);
const char* type_name(C* c, Type* t);
int type_isint(Type* t);
int type_isnum(Type* t);
int type_isobj(Type* t);
int type_isref(Type* t);
int assignable(Type* from, Type* to);
Type* code_type(C* c, int k);
Value default_value(Type* t);
int default_kind(Type* t);
void emit1(C* c, int32_t v);
void emit_op(C* c, int o);
void emit_op1(C* c, int o, int a);
void emit_op2(C* c, int o, int a, int b);
int emit_jump(C* c, int o);
void patch_jump(C* c, int pos);
void emit_jump_back(C* c, int o, int target);
int code_label(C* c);
void emit_pop(C* c);
void emit_fconst(C* c, float v);
int add_string(C* c, const char* s, int len);
void push_default(C* c, Type* t);
CField* find_field(C* c, int cls, const char* n, int l);
CMethod* find_method(C* c, int cls, const char* n, int l);
CProp* find_prop(C* c, int cls, const char* n, int l);
int class_in(C* c, int i, const char* ns, int nsl, const char* n, int l);
int find_class(C* c, const char* n0, int l0);
const char* class_hint(C* c, const char* n, int l);
int is_builtin_class(const char* n, int l);
int find_local(C* c, const char* n, int l);
Res resolve_name(C* c, const char* n, int l);
void fold_ns(C* c, Expr* m);
int declare_local(C* c, const char* n, int l, Type* t, int line);
void init_local(C* c, int slot, int boxed);
void box_params(C* c);
int alloc_temp(C* c);
Type* resolve_typenode(C* c, TypeNode* tn, int extraDims);
int enum_of(C* c, Expr* e);

/* expressions (comp_expr.c) */
void coerce(C* c, Type* from, Type* to, int line, const char* what);
Type* peek_type(C* c, Expr* e);
void emit_tostr(C* c, Type* t, int depth, int line);
Type* compare_op(C* c, int tok, Type* lt, Type* rt, int line);
void need_bool(C* c, Type* t, int line, const char* what);
void emit_get(C* c, CProp* pp, int virt, int line);
void copy_struct(C* c, Expr* e, Type* t);
Type* compile_expr(C* c, Expr* e);

/* calls and overloads (comp_call.c) */
void args_for(C* c, Expr* e, Type** params, int np, const char* what);
void args_for_ro(C* c, Expr* e, Type** params, int np, const char* what, uint32_t ro);
int same_params(CMethod* a, CMethod* b);
int method_cands(C* c, int cls, const char* n, int l, CMethod** out, int k, int max);
CMethod* pick_overload(C* c, CMethod** cand, int n, Expr* e, const char* what);
Type* compile_call(C* c, Expr* e);

/* lambdas (comp_lambda.c) */
void scan_captures(C* c, Stmt* body, Expr* extra);
void expr_to(C* c, Expr* e, Type* t, int line, const char* what);

/* statements (comp_stmt.c) */
void expr_stmt(C* c, Expr* e);
void compile_stmt(C* c, Stmt* s);

#endif
