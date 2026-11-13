/* MiniSharp - inside of the language: bytecode, Program and VM.
 * Only lang/ (compiler, VM, disassembler, REPL) and, for now, the built-in functions in lib/ include it. */
#ifndef MS_VM_H
#define MS_VM_H

#include "ms.h"

/* ---------- bytecode ---------- */
enum {
    OP_ICONST, OP_FCONST, OP_SCONST, OP_NULL, OP_TRUE, OP_FALSE,
    OP_POP, OP_DUP, OP_DUP2,
    OP_LOAD, OP_STORE, OP_STOREP,
    OP_GETSTATIC, OP_SETSTATIC, OP_GETFIELD, OP_SETFIELD, OP_GETELEM, OP_SETELEM,
    OP_IADD, OP_ISUB, OP_IMUL, OP_IDIV, OP_IMOD, OP_INEG,
    OP_FADD, OP_FSUB, OP_FMUL, OP_FDIV, OP_FMOD, OP_FNEG,
    OP_I2F, OP_F2I, OP_TOSTR, OP_CONCAT,
    OP_ILT, OP_ILE, OP_IGT, OP_IGE, OP_IEQ, OP_INE,
    OP_FLT, OP_FLE, OP_FGT, OP_FGE, OP_FEQ, OP_FNE,
    OP_SEQ, OP_SNE, OP_REQ, OP_RNE, OP_NOT,
    OP_JMP, OP_JZ, OP_JFALSE_KEEP, OP_JTRUE_KEEP,
    OP_CALL, OP_CALLN, OP_NULLCHK, OP_RET, OP_RETV,
    OP_NEW, OP_NEWARR, OP_ARRLEN, OP_STRLEN, OP_NORET,
    OP_IAND, OP_IOR, OP_IXOR, OP_ISHL, OP_ISHR, OP_INOT, OP_I2C, OP_C2I,
    OP_LIST,                            /* sub-op (L_*) follows */
    OP_CORO_NEW,                        /* func, argc: calling an iterator method makes a coroutine */
    OP_YIELD,                           /* Y_* kind */
    OP_CORO,                            /* C_* sub-op */
    OP_DICT,                            /* D_* sub-op, | default-value kind << 8 for D_TRYGET */
    OP_CALLV,                           /* selector, argc (with 'this'): virtual / interface call through the object's vtable */
    OP_ISINST,                          /* class, mode: 0 'is' (bool), 1 'as' (object or null), 2 cast (fails if not one) */
    OP_BOX, OP_GETCELL, OP_SETCELL,     /* value -> new cell; cell -> value; cell value -> value */
    OP_UPVAL,                           /* k: push captured cell k of the running lambda */
    OP_CLOSURE,                         /* kind (0 lambda / method, 1 static method, 2 virtual: idx = selector), idx, nup:
                                           self, cells... -> delegate */
    OP_CALLF,                           /* argc, hasResult: delegate, args... -> call it */
    OP_DELEG,                           /* 0 a += b, 1 a -= b */
    OP_JNULL, OP_JNULL_POP, OP_JNNULL,  /* offset: a?.b (null stays / is popped), a ?? b (non-null stays, null is popped) */
    OP_LISTF,                           /* LF_* | default-value kind << 8: list, delegate -> result */
    OP_COPY,                            /* struct -> a copy of it (struct fields copied too) */
    /* superinstructions: made by the compiler from frequent sequences */
    OP_JNLT, OP_JNLE, OP_JNGT, OP_JNGE, OP_JNEQ, OP_JNNE,   /* offset: int compare + JZ (jump when the comparison is false) */
    OP_IADDK,                           /* k: ICONST k + IADD */
    OP_INCL,                            /* slot, k: LOAD slot + IADDK k + STOREP slot (i++, i += 2) */
    OP_GETFIELDL                        /* slot, field: LOAD slot + GETFIELD field (this.x, obj.x) */
};
/* Default-value kinds (OP_NEWARR, default(T) in OP_DICT / OP_LISTF): 0 null, 1 int, 2 float, 3 bool, 4 char,
   MS_KIND_STRUCT + class: a new zeroed struct. OP_TOSTR kind MS_KIND_ENUM + class: the name of an enum value. */
#define MS_KIND_STRUCT 16
#define MS_KIND_ENUM 16
enum { LF_FIND, LF_FINDLAST, LF_FINDALL, LF_FINDINDEX, LF_FINDLASTINDEX, LF_EXISTS, LF_TRUEFORALL, LF_REMOVEALL, LF_FOREACH, LF_SORT };
enum { D_NEW, D_ADD, D_REMOVE, D_CONTAINS, D_CONTAINSVAL, D_CLEAR, D_TRYGET, D_KEYS, D_VALUES, D_NEXT, D_KEYAT, D_VALAT };
enum { Y_NEXT, Y_SECONDS, Y_CORO };
enum { C_START, C_STOP, C_STOPALL };
enum { L_NEW, L_ADD, L_INSERT, L_REMOVEAT, L_REMOVE, L_CLEAR, L_INDEXOF, L_ADDRANGE, L_SORT, L_REVERSE };

typedef struct {
    int32_t* code; int* lines; int len, cap;
    int nparams, nlocals;
    uint8_t isIter;                     /* calling it makes a coroutine */
    char name[64];
} Func;

typedef struct {
    char name[32]; int nfields; Value* defaults;
    int base;                           /* base class or -1 */
    int* vtable;                        /* [Program.nsel]: function for each virtual selector, -1 none */
    uint8_t* isa;                       /* [nclasses]: 1 if an object of this class is also that class / interface */
    int toString;                       /* 'override string ToString()' or -1 */
    uint8_t isStruct, isFlags;
    int nstructf; int32_t* structfIdx; int32_t* structfCls;    /* instance fields that hold a struct: made with the object */
    int nenum; int32_t* enumVals; int32_t* enumStr;             /* enum: member values and their names (indices of strings) */
} ClassRT;

struct Program {
    Func* funcs; int nfuncs, capfuncs;
    ObjString** strings; int nstrings, capstrings;
    ClassRT* classes; int nclasses;
    int nsel;                           /* virtual selectors (length of every vtable) */
    Value* staticDefaults; int nstatics;
    int staticInit, entryMain, entryUpdate, entryDraw;
};

/* ---------- VM ---------- */
#define MS_STACK  8192
#define MS_FRAMES 256

typedef struct { Func* f; int ip; int base; Obj* clo; } Frame;    /* clo: the delegate being run, for OP_UPVAL */

struct VM {
    Program* p;
    Value* stack; int sp;
    Frame frames[MS_FRAMES]; int fp;
    Value* statics;
    Obj* objects; size_t bytes, nextGC;
    Obj** gray; int ngray, capgray;
    char err[256];
    int quit;
    ObjCoro** coros; int ncoros, capcoros;     /* started coroutines (GC roots) */
    ObjCoro* curCoro;                           /* the one running right now */
    int frameNo;                                /* counts vm_tick_coroutines calls */
    long long deadline; unsigned loops;         /* REPL: a loop still running at time() > deadline stops (0 = never) */
    /* small objects (up to MS_POOL_MAX bytes) come from 16 KB blocks; a freed one goes to the list of its size */
    void* pool[17]; void* blocks; char* bump; size_t bumpLeft;
};
#define MS_POOL_MAX 128


/* Built-in functions and constants the compiler can see: everything registered so far (register.c). */
extern const NativeDef* ms_natives;
extern int ms_nnatives;
extern const ConstDef* ms_consts;
extern int ms_nconsts;

#endif
