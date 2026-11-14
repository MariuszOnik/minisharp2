/* MiniSharp - public API: values and objects (for built-in functions), compiling, running.
 * Hosts, the library, modules and applications use only this header; bytecode, Program and VM are in vm.h. */
#ifndef MS_H
#define MS_H

#include <stdint.h>
#include <stddef.h>

typedef struct Program Program;         /* a compiled program (opaque outside lang/) */
typedef struct VM VM;                   /* a running program (opaque outside lang/) */

/* ---------- runtime values ---------- */
typedef struct Obj Obj;
enum { V_NULL, V_INT, V_FLOAT, V_BOOL, V_OBJ, V_CHAR };
typedef struct {
    uint8_t t;
    union { int32_t i; float f; Obj* o; } as;
} Value;

enum { O_STRING, O_ARRAY, O_INSTANCE, O_LIST, O_CORO, O_DICT, O_CELL, O_CLOSURE };
struct Obj { uint8_t type, marked, pinned; Obj* next; };
/* UTF-8 text. len = bytes; ulen = characters (code points), -1 until first needed (strings never change);
 * cacheI / cacheB: the last character index looked up and its byte offset, so a loop over s[i] is not quadratic */
typedef struct { Obj h; int32_t len, ulen, cacheI, cacheB; char chars[]; } ObjString;
/* rows / cols: the dimensions of a rectangular array T[,] (items row after row); 0 for T[] */
typedef struct { Obj h; int32_t len, rows, cols; Value items[]; } ObjArray;
typedef struct { Obj h; int32_t cls; int32_t nfields; Value fields[]; } ObjInstance;
/* version: changed by every Add, Remove, Insert, Clear, Sort, l[i] = x...; foreach and ForEach stop with an error
 * when it changes under them (as in C#) */
typedef struct { Obj h; int32_t len, cap, version; Value* items; } ObjList;
/* Dictionary, laid out like .NET's so foreach gives the same order: entries are kept in the order
 * they were added, and a removed entry's place goes to a free list that the next Add reuses.
 * 'index' is an open-addressing hash table of entry numbers (-1 empty, -2 removed). */
typedef struct { Value key, val; uint32_t hash; int32_t live; } DictEntry;   /* free entry: hash = next free */
typedef struct {
    Obj h;
    int32_t count, used, cap, icap, ntomb, freeList, freeCount;
    int32_t version;                    /* grows when a new key is added: foreach stops with an error (Remove, Clear and
                                           overwriting a value are allowed while looping, as in .NET Core 3.0+) */
    DictEntry* entries; int32_t* index;
} ObjDict;

/* A local variable that a lambda uses lives in a cell, so the method and the lambda share it. */
typedef struct { Obj h; Value v; } ObjCell;
/* A delegate (Func / Action): a lambda with its captured cells, or a method with its object.
 * Calling it puts 'self' in slot 0 (CL_SELF) or drops that slot (CL_STATIC, a static method). */
enum { CL_SELF, CL_STATIC, CL_MULTI /* a += b: ups = the delegates to call in order */ };
typedef struct { Obj h; int32_t func; uint8_t kind; int32_t nup; Value self; Value ups[]; } ObjClosure;

/* A coroutine (IEnumerator). 'yield' is only allowed as a statement in the iterator method itself,
 * so its whole state is the method's locals plus the resume position. */
typedef struct {
    Obj h;
    int32_t func, ip, nlocals;
    uint8_t started, done, running, waitKind;   /* waitKind: Y_* */
    int32_t waitUntil, lastFrame;               /* ms for WaitForSeconds; frame of the last yield */
    Obj* waitFor;                               /* coroutine we wait for (Y_CORO) */
    Obj* owner;                                 /* started by the owner's 'yield return': stops with it */
    Value locals[];
} ObjCoro;

/* ---------- static types (used by compiler and native signatures) ---------- */
enum { T_VOID, T_INT, T_FLOAT, T_BOOL, T_STRING, T_NULL, T_CLASS, T_ARRAY, T_ANY, T_CHAR, T_LIST,
       T_STRARR /* string[], only in native signatures */, T_CORO /* IEnumerator / Coroutine */,
       T_DICT, T_KVP /* Dictionary<K,V> and the KeyValuePair<K,V> of its foreach */,
       T_FUNC /* Func<...> / Action<...> / Predicate<T> / Comparison<T> */,
       T_ENUM /* an int at run time */, T_STRUCT /* an ObjInstance that is copied when it is stored */ };

/* ---------- natives ---------- */
typedef Value (*NativeFn)(VM* vm, Value* args);
#define MS_MAXNATIVEARGS 10
typedef struct { const char* cls; const char* name; int np; int params[MS_MAXNATIVEARGS]; int ret; NativeFn fn; } NativeDef;
typedef struct { const char* cls; const char* name; int isFloat; int ivalue; float fvalue; } ConstDef;

/* Each layer and module registers its own tables before the first ms_compile (the compiler sees what is
 * registered at that moment). Tables are appended in order: overloads of the same name are tried in
 * registration order, and indices of programs compiled earlier stay valid. The table is copied; registering
 * the same table again does nothing. 1 = ok, 0 = bad entry (no name or function, too many parameters) or no memory. */
int ms_register(const NativeDef* table, int n);
int ms_register_consts(const ConstDef* table, int n);
#define MS_REGISTER(table) ms_register((table), (int)(sizeof(table) / sizeof((table)[0])))
#define MS_REGISTER_CONSTS(table) ms_register_consts((table), (int)(sizeof(table) / sizeof((table)[0])))
/* Hooks: a layer above the language (a game loop: "Update", "Draw") names the 'static void X()' methods it will
 * call; the compiler then makes one function per name that calls X() of every class that has it, in the order
 * of the classes ([Order(n)] first, then files by name, classes top to bottom). The language itself knows only Main.
 * At most MS_MAX_HOOKS names of up to MS_HOOK_NAME-1 characters; 1 = ok (also when already registered). */
#define MS_MAX_HOOKS 16
#define MS_HOOK_NAME 32
int ms_register_hook(const char* name);
/* Sources: C# classes a layer adds to every program (unless the program declares a class of that name itself),
 * e.g. the graph layer's base class of nodes. The string is kept, not copied. 1 = ok. */
#define MS_MAX_SOURCES 16
int ms_register_source(const char* src);
int ms_native_count(void);                 /* registered functions (for Help, hints, the graph's node catalogue) */
const NativeDef* ms_native_at(int i);      /* NULL outside 0..count-1 */

/* ---------- API ---------- */
Program* ms_compile(const char* src, char* err, int errlen);
void ms_free_program(Program* p);
/* REPL: like ms_compile, but in Repl.Main a variable declared at the top level becomes a static field
 * ("static int x; " is appended to 'decls' so the next input can declare it in the class) and an
 * expression on its own is printed */
Program* ms_compile_repl(const char* src, char* err, int errlen, char* decls, int declsCap);

VM* vm_new(Program* p);
int vm_set_program(VM* vm, Program* p); /* REPL: go on with a newer program whose statics start with the old ones */
void vm_free(VM* vm);
/* 1 = ok, 0 = runtime error or quit, MS_WAIT_INPUT = stopped in Console.ReadLine: the host gets a line from the
 * user (an input field, stdin) and calls vm_resume_input, which goes on and returns the same codes. Until then
 * the program is paused (its stack is kept); the host must not vm_call it again. */
#define MS_WAIT_INPUT 3
int vm_call(VM* vm, int func);
int vm_resume_input(VM* vm, const char* line);     /* line without the newline; NULL = end of input (ReadLine gives null) */
int vm_tick_coroutines(VM* vm);         /* once per frame after Update(); same result codes */
Value vm_make_string(VM* vm, const char* s, int len);
Value vm_make_array(VM* vm, int len);      /* filled with null */
void ms_value_to_text(VM* vm, Value v, char* buf, int buflen);

/* ---------- text: strings are UTF-8, a char is one code point (lang/text.c) ---------- */
int ms_utf8_decode(const char* s, int len, int* adv);  /* code point at s; a bad byte gives itself, *adv = 1 */
int ms_utf8_encode(int cp, char* out);                 /* 1..4 bytes into out (no NUL) */
int ms_utf8_count(const char* s, int len);             /* characters in len bytes */
int ms_str_length(ObjString* s);                       /* characters */
int ms_str_offset(ObjString* s, int i);                /* byte offset of character i (0..length) */
int ms_str_char_at(ObjString* s, int i);               /* code point of character i (0 <= i < length) */
int ms_char_upper(int cp);                             /* Latin (with Polish letters), Greek, Cyrillic */
int ms_char_lower(int cp);
int ms_char_is_letter(int cp);
int ms_char_is_space(int cp);

void ms_disassemble(Program* p);        /* print the bytecode (tools) */

/* Entry points of a compiled program: function index for vm_call, or -1 when the program has none.
 * A program may have no Main (e.g. only Update): whether that is enough is the host's decision. */
enum { MS_ENTRY_STATIC_INIT, MS_ENTRY_MAIN };
int ms_entry(const Program* p, int which);
int ms_hook(const Program* p, const char* name);    /* registered hook (ms_register_hook) or -1 if no class has it */

const char* ms_vm_error(VM* vm);        /* the runtime error after vm_call returned 0 ("" if none) */
void ms_vm_fail(VM* vm, const char* fmt, ...);  /* built-in function: report a runtime error (the call then fails) */
int ms_vm_quit_requested(VM* vm);       /* App.Quit() was called */
void ms_vm_request_quit(VM* vm);
/* Built-in function that needs a line from the user (Console.ReadLine): 1 = the VM stops after this call and
 * vm_call returns MS_WAIT_INPUT; 0 = not possible here (a nested call), the error is set. */
int ms_vm_wait_input(VM* vm);
int ms_vm_waiting(VM* vm);              /* 1 = stopped in Console.ReadLine */
void ms_vm_set_deadline(VM* vm, long long deadline);   /* REPL: a loop still running at time() > deadline stops; 0 = never */
/* Built-in function that allocates several objects: keep the first one alive (GC root on the VM stack) while
 * making the others, then pop it. Pushes and pops must pair up within the call; a few slots are always free. */
void ms_vm_push_root(VM* vm, Value v);
void ms_vm_pop_root(VM* vm);

/* ---------- reflection (lang/reflect.c): classes, fields and methods of a compiled program ----------
 * For layers above the language that build objects from data: a scene loader makes an object of the class named in
 * the scene file and sets its fields by name, an inspector lists the classes derived from a base class and their
 * public fields. Class, field and method numbers are valid for the program they came from. */
enum { MS_CK_CLASS, MS_CK_STRUCT, MS_CK_ENUM, MS_CK_INTERFACE };
enum { MS_ACC_NONE, MS_ACC_PUBLIC, MS_ACC_PRIVATE, MS_ACC_PROTECTED, MS_ACC_INTERNAL };   /* = AC_* of the parser */
typedef struct {
    const char* name;
    int kind, cls;              /* T_*; class / struct / enum (T_CLASS, T_STRUCT, T_ENUM), else -1 */
    int elemKind, elemCls;      /* T[] / T[,] / List<T>: the item type; Dictionary<K,V>: V */
    int keyKind, keyCls;        /* Dictionary<K,V>: K */
    int rank;                   /* 2 for T[,], else 0 */
    int access;                 /* MS_ACC_*; MS_ACC_NONE = nothing written (private in C#) */
    int readOnly;               /* 1 readonly, 2 const */
    int owner;                  /* class that declares it (a base class for an inherited field) */
} MsField;
int ms_class_count(const Program* p);
const char* ms_class_name(const Program* p, int cls);       /* "Drzwi"; NULL outside 0..count-1 */
const char* ms_class_full_name(const Program* p, int cls);  /* "Gra.Drzwi" (namespace.name) */
/* by full name, or by the name alone when one class has it; -1 = none or more than one */
int ms_class_find(const Program* p, const char* name);
int ms_class_kind(const Program* p, int cls);               /* MS_CK_*, -1 bad number */
int ms_class_base(const Program* p, int cls);               /* base class or -1 */
int ms_class_is(const Program* p, int cls, int other);      /* 1: an object of cls is also other (itself, a base, an interface) */
int ms_class_is_abstract(const Program* p, int cls);        /* abstract class or interface */
int ms_class_from_source(const Program* p, int cls);        /* added by a layer (ms_register_source), not written by the user */
int ms_class_can_create(const Program* p, int cls);         /* vm_new_object works: a class (not abstract, static, enum,
                                                               interface) or struct with a constructor without parameters */
int ms_field_count(const Program* p, int cls);              /* instance fields, inherited ones first */
int ms_field_info(const Program* p, int cls, int i, MsField* out);  /* 1 ok, 0 bad numbers */
int ms_field_find(const Program* p, int cls, const char* name);     /* -1 none */
int ms_enum_count(const Program* p, int cls);               /* members of an enum (0 for other classes) */
const char* ms_enum_name(const Program* p, int cls, int i);
int ms_enum_value(const Program* p, int cls, int i);
/* Instance method 'name' with nargs parameters that an object of class cls runs (an override in a derived class
 * wins); -1 = none or abstract. *owner (if not NULL) = class whose method it is: a layer can see whether a script
 * overrides Update or just inherits the empty one of its base class. Overloads differ by the number of parameters. */
int ms_method_find(const Program* p, int cls, const char* name, int nargs, int* owner);

/* A new object of class cls, made as 'new Cls()' does (field initializers, the constructor without parameters).
 * 1 = ok (*out = the object; keep it with ms_vm_keep if the host holds it across calls), 0 = error (ms_vm_error). */
int vm_new_object(VM* vm, int cls, Value* out);
/* Calls function func (e.g. from ms_method_find; an instance method takes the object as args[0]) with argc
 * arguments; *result (if not NULL) = what it returns. Same result codes as vm_call. */
int vm_call_args(VM* vm, int func, const Value* args, int argc, Value* result);
int ms_class_of(Value v);                                   /* class of an object (instance or struct), else -1 */
Value ms_get_field(Value obj, int i);                       /* field i (ms_field_info numbering); null if not there */
/* obj.field i = v, with the type checked as an assignment in C# would (an int goes into a float field; a struct is
 * copied). 1 = ok, 0 = wrong object, field or type (the reason in ms_vm_error). */
int ms_set_field(VM* vm, Value obj, int i, Value v);
/* Values the host holds between calls (objects of a scene): the GC does not free them until released.
 * ms_vm_keep returns a handle (>= 0) or -1 (out of memory). */
int ms_vm_keep(VM* vm, Value v);
Value ms_vm_kept(VM* vm, int handle);
void ms_vm_release(VM* vm, int handle);

/* ---------- host hooks (the language knows no platform) ---------- */
/* Milliseconds since some fixed moment, for WaitForSeconds. Default: the C clock(). */
void ms_set_clock(int (*millis)(void));
int ms_millis(void);
/* The Stop button: while a program runs, the VM calls check() about every 65 thousand backward jumps (loops);
 * a non-zero result stops the program with the error "stopped by the user" (3DS: START+SELECT, PC: Ctrl+C,
 * web: a flag set by the page). NULL = no button (default). */
void ms_set_interrupt(int (*check)(void));
/* Called by vm_new before a program starts: layers and modules reset their state here
 * (e.g. the screen colour, sprites, the target frame rate). Up to MS_MAX_RESET_HOOKS. */
#define MS_MAX_RESET_HOOKS 16
void ms_on_vm_new(void (*fn)(void));

#endif
