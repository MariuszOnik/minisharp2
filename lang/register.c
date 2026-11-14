/* MiniSharp - registration of built-in functions and constants.
 * Every layer and module registers its own table; the compiler sees everything registered before ms_compile.
 * Tables are appended, never removed or reordered, so a native's index (stored in the bytecode by OP_CALLN)
 * stays valid for programs compiled earlier. Overloads are picked in registration order. */
#include "vm.h"
#include <stdlib.h>
#include <string.h>

const NativeDef* ms_natives = NULL;
int ms_nnatives = 0;
const ConstDef* ms_consts = NULL;
int ms_nconsts = 0;

static NativeDef* natives; static int natCap;
static ConstDef* consts; static int constCap;
/* tables already registered (registering one again does nothing) */
static const void** tables; static int ntables, tablesCap;

static int seen(const void* table) {
    for (int i = 0; i < ntables; i++) if (tables[i] == table) return 1;
    return 0;
}

static int remember(const void* table) {
    if (ntables == tablesCap) {
        int cap = tablesCap ? tablesCap * 2 : 16;
        const void** t = realloc(tables, cap * sizeof *t);
        if (!t) return 0;
        tables = t; tablesCap = cap;
    }
    tables[ntables++] = table;
    return 1;
}

static int grow(void** arr, int* cap, int need, size_t elem) {
    if (need <= *cap) return 1;
    int c = *cap ? *cap : 64;
    while (c < need) c *= 2;
    void* a = realloc(*arr, (size_t)c * elem);
    if (!a) return 0;
    *arr = a; *cap = c;
    return 1;
}

/* module of each native / constant: -1 = always visible (language, library, platform), else a module number */
const int8_t* ms_native_module = NULL;
const int8_t* ms_const_module = NULL;
static int8_t* natMod; static int natModCap;
static int8_t* constMod; static int constModCap;
uint32_t ms_modules_on = 0;

static int reg_natives(const NativeDef* table, int n, int mod) {
    if (!table || n < 0) return 0;
    if (seen(table)) return 1;
    for (int i = 0; i < n; i++) {
        const NativeDef* d = &table[i];
        if (!d->cls || !d->name || !d->fn || d->np < 0 || d->np > MS_MAXNATIVEARGS) return 0;
    }
    if (!grow((void**)&natives, &natCap, ms_nnatives + n, sizeof *natives) ||
        !grow((void**)&natMod, &natModCap, ms_nnatives + n, sizeof *natMod) || !remember(table)) return 0;
    memcpy(natives + ms_nnatives, table, (size_t)n * sizeof *natives);
    memset(natMod + ms_nnatives, mod, (size_t)n);
    ms_natives = natives; ms_native_module = natMod; ms_nnatives += n;
    return 1;
}

static int reg_consts(const ConstDef* table, int n, int mod) {
    if (!table || n < 0) return 0;
    if (seen(table)) return 1;
    for (int i = 0; i < n; i++) if (!table[i].cls || !table[i].name) return 0;
    if (!grow((void**)&consts, &constCap, ms_nconsts + n, sizeof *consts) ||
        !grow((void**)&constMod, &constModCap, ms_nconsts + n, sizeof *constMod) || !remember(table)) return 0;
    memcpy(consts + ms_nconsts, table, (size_t)n * sizeof *consts);
    memset(constMod + ms_nconsts, mod, (size_t)n);
    ms_consts = consts; ms_const_module = constMod; ms_nconsts += n;
    return 1;
}

int ms_register(const NativeDef* table, int n) { return reg_natives(table, n, -1); }
int ms_register_consts(const ConstDef* table, int n) { return reg_consts(table, n, -1); }

/* ---------- modules: built into the build, turned on by 'using Name;' in the program ---------- */
const MsModule* ms_modules[MS_MAX_MODULES];
int ms_nmodules = 0;

int ms_register_module(const MsModule* m) {
    if (!m || !m->name || !*m->name) return 0;
    for (int i = 0; i < ms_nmodules; i++) {
        if (ms_modules[i] == m) return 1;
        if (!strcmp(ms_modules[i]->name, m->name)) return 0;     /* two modules with one name */
    }
    if (ms_nmodules == MS_MAX_MODULES) return 0;
    int k = ms_nmodules;
    if (m->nnatives && !reg_natives(m->natives, m->nnatives, k)) return 0;
    if (m->nconsts && !reg_consts(m->consts, m->nconsts, k)) return 0;
    ms_modules[ms_nmodules++] = m;
    return 1;
}

int ms_module_count(void) { return ms_nmodules; }
const MsModule* ms_module_at(int i) { return i >= 0 && i < ms_nmodules ? ms_modules[i] : NULL; }

/* the module of a native class name that is off right now, or NULL (for 'add using X;' hints) */
const char* ms_module_of_class(const char* n, int l) {
    for (int i = 0; i < ms_nnatives; i++)
        if (natMod[i] >= 0 && !ms_native_visible(i) && (int)strlen(natives[i].cls) == l && !memcmp(natives[i].cls, n, l))
            return ms_modules[natMod[i]]->name;
    for (int i = 0; i < ms_nconsts; i++)
        if (constMod[i] >= 0 && !ms_const_visible(i) && (int)strlen(consts[i].cls) == l && !memcmp(consts[i].cls, n, l))
            return ms_modules[constMod[i]]->name;
    return NULL;
}

int ms_native_count(void) { return ms_nnatives; }
const NativeDef* ms_native_at(int i) { return i >= 0 && i < ms_nnatives ? &ms_natives[i] : NULL; }

/* ---------- hooks and sources registered by the layers above the language ---------- */
/* Hooks: names of 'static void X()' methods that the host calls in every class (a game loop registers
 * "Update" and "Draw"); the compiler makes one function per name that calls them all (ms_hook). */
char ms_hook_names[MS_MAX_HOOKS][MS_HOOK_NAME];
int ms_nhooks = 0;

int ms_register_hook(const char* name) {
    if (!name || !*name || strlen(name) >= MS_HOOK_NAME || !strcmp(name, "Main")) return 0;
    for (int i = 0; i < ms_nhooks; i++) if (!strcmp(ms_hook_names[i], name)) return 1;
    if (ms_nhooks == MS_MAX_HOOKS) return 0;
    strcpy(ms_hook_names[ms_nhooks++], name);
    return 1;
}

/* Sources: C# text whose classes every program gets unless it declares a class of the same name itself
 * (e.g. the graph layer's 'abstract class Node'). The text is not copied: pass a string that lives on. */
const char* ms_sources[MS_MAX_SOURCES];
int ms_nsources = 0;

int ms_register_source(const char* src) {
    if (!src) return 0;
    for (int i = 0; i < ms_nsources; i++) if (ms_sources[i] == src) return 1;
    if (ms_nsources == MS_MAX_SOURCES) return 0;
    ms_sources[ms_nsources++] = src;
    return 1;
}
