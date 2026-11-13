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

int ms_register(const NativeDef* table, int n) {
    if (!table || n < 0) return 0;
    if (seen(table)) return 1;
    for (int i = 0; i < n; i++) {
        const NativeDef* d = &table[i];
        if (!d->cls || !d->name || !d->fn || d->np < 0 || d->np > MS_MAXNATIVEARGS) return 0;
    }
    if (!grow((void**)&natives, &natCap, ms_nnatives + n, sizeof *natives) || !remember(table)) return 0;
    memcpy(natives + ms_nnatives, table, (size_t)n * sizeof *natives);
    ms_natives = natives; ms_nnatives += n;
    return 1;
}

int ms_register_consts(const ConstDef* table, int n) {
    if (!table || n < 0) return 0;
    if (seen(table)) return 1;
    for (int i = 0; i < n; i++) if (!table[i].cls || !table[i].name) return 0;
    if (!grow((void**)&consts, &constCap, ms_nconsts + n, sizeof *consts) || !remember(table)) return 0;
    memcpy(consts + ms_nconsts, table, (size_t)n * sizeof *consts);
    ms_consts = consts; ms_nconsts += n;
    return 1;
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
