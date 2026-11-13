/* MiniSharp - what an editor knows about the project's code without compiling it: the classes, their fields
 * and methods, and the type of a name from its declaration. It reads the text, not the AST, so it works on code
 * that does not compile yet (the usual state while typing). Used for suggestions after "ship." and for the
 * signature hint of the user's own methods; the language service (service.h) builds on it. */
#ifndef MS_CODEINDEX_H
#define MS_CODEINDEX_H

#define CI_WORD 32                      /* longest suggested name + 1 (the same as REPL_WORD of the terminal) */

enum { CI_PUBLIC, CI_PRIVATE, CI_PROTECTED };   /* no keyword = CI_PUBLIC, like the compiler's mild access rules */

typedef struct {
    char cls[32], name[32];
    char type[48];                      /* field type or method return type, e.g. "List<Bullet>" */
    char sig[100];                      /* "void Hit(int dmg)" or "float x" */
    char isMethod, access;              /* access: CI_* */
} CIMember;

typedef struct {
    char name[32];                      /* short name: Troll */
    char bases[64];                     /* "Enemy IHealable": what it derives from, space separated */
    char ns[48];                        /* namespace: "World.Monsters" ("" = global) */
    char isEnum;                        /* its body is a list of names */
} CIClass;

typedef struct { int cls, text, start, end; } CIRange;   /* where a class body is: text, from '{'+1 to '}' */

typedef struct {
    CIMember* m; int n, cap;
    CIClass* cls; int nclasses, capclasses;
    CIRange* ranges; int nranges, capranges;
    char nss[32][48]; int nns;          /* namespaces declared in the files, with the ones around them (A for A.B) */
    const char** texts; int ntexts;     /* the files the index was built from (not copied: keep them while it is used) */
} CodeIndex;

/* ci must be zeroed before the first build; building again reuses its memory */
void ci_build(CodeIndex* ci, const char** texts, int ntexts);
void ci_free(CodeIndex* ci);

/* type of the expression that ends just before texts[t][dot] (the '.'), e.g. "ship", "Game.ship",
 * "bullets[i]", "this". 1 = found, the type in 'type' ("Ship", "List<Bullet>", "string", "int[]",
 * "namespace A" for a namespace). Full names of classes are shortened: A.Item -> Item */
int ci_type_before(const CodeIndex* ci, int t, int dot, char* type, int cap);

/* the member names of a type, separated by spaces: user classes, List, Dictionary, string, arrays, built-in
 * classes. 'viewer' = the class the code is in (NULL = outside any class): private and protected members
 * are listed only where the compiler lets them be used. */
void ci_member_names(const CodeIndex* ci, const char* type, const char* viewer, char* out, int cap);

/* members after "expr." ending at 'cursor' in text t, filtered by what is typed after the dot (an empty prefix
 * lists them all, case is ignored). Sets *start to where the typed word begins. -1: the cursor is not after
 * a dot, or the type is unknown */
int ci_complete(const CodeIndex* ci, int t, int cursor, int* start, char out[][CI_WORD], int max);

/* "void Hit(int dmg)" for type.name (also from a base class), or NULL */
const char* ci_signature(const CodeIndex* ci, const char* type, const char* name);

/* name of the innermost class the position is in, or NULL */
const char* ci_class_at(const CodeIndex* ci, int t, int pos);

#endif
