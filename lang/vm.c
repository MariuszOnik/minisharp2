/* MiniSharp - stack virtual machine with a mark & sweep garbage collector. */
#include "vm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

/* ================= host hooks ================= */
static int clock_millis(void) { return (int)((long long)clock() * 1000 / CLOCKS_PER_SEC); }
static int (*hostMillis)(void) = clock_millis;
void ms_set_clock(int (*millis)(void)) { hostMillis = millis ? millis : clock_millis; }
int ms_millis(void) { return hostMillis(); }
static int (*hostInterrupt)(void);
void ms_set_interrupt(int (*check)(void)) { hostInterrupt = check; }

static void (*resetHooks[MS_MAX_RESET_HOOKS])(void);
static int nresetHooks;
void ms_on_vm_new(void (*fn)(void)) {
    for (int i = 0; i < nresetHooks; i++) if (resetHooks[i] == fn) return;
    if (fn && nresetHooks < MS_MAX_RESET_HOOKS) resetHooks[nresetHooks++] = fn;
}

int ms_entry(const Program* p, int which) {
    switch (which) {
        case MS_ENTRY_STATIC_INIT: return p->staticInit;
        case MS_ENTRY_MAIN: return p->entryMain;
    }
    return -1;
}
int ms_hook(const Program* p, const char* name) {
    for (int i = 0; name && i < p->nhooks; i++) if (!strcmp(p->hookNames[i], name)) return p->hookFuncs[i];
    return -1;
}
const char* ms_vm_error(VM* vm) { return vm->err; }
void ms_vm_fail(VM* vm, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(vm->err, sizeof vm->err, fmt, ap);
    va_end(ap);
}
int ms_vm_quit_requested(VM* vm) { return vm->quit; }
void ms_vm_request_quit(VM* vm) { vm->quit = 1; }
void ms_vm_set_deadline(VM* vm, long long deadline) { vm->deadline = deadline; }
void ms_vm_push_root(VM* vm, Value v) { vm->stack[vm->sp++] = v; }   /* calls keep 64 free slots above sp */
void ms_vm_pop_root(VM* vm) { vm->sp--; }

/* rarely run helpers stay out of the interpreter loop, so the loop stays small in the CPU's instruction cache */
#if defined(__GNUC__)
#define MS_COLD __attribute__((noinline))
#else
#define MS_COLD
#endif

/* ================= heap ================= */
static size_t obj_size(Obj* o) {
    switch (o->type) {
        case O_STRING: return sizeof(ObjString) + ((ObjString*)o)->len + 1;
        case O_ARRAY: return sizeof(ObjArray) + sizeof(Value) * ((ObjArray*)o)->len;
        case O_LIST: return sizeof(ObjList) + sizeof(Value) * ((ObjList*)o)->cap;
        case O_CORO: return sizeof(ObjCoro) + sizeof(Value) * ((ObjCoro*)o)->nlocals;
        case O_DICT: return sizeof(ObjDict) + sizeof(DictEntry) * ((ObjDict*)o)->cap + sizeof(int32_t) * ((ObjDict*)o)->icap;
        case O_CELL: return sizeof(ObjCell);
        case O_CLOSURE: return sizeof(ObjClosure) + sizeof(Value) * ((ObjClosure*)o)->nup;
        default: return sizeof(ObjInstance) + sizeof(Value) * ((ObjInstance*)o)->nfields;
    }
}

static void mark_value(VM* vm, Value v) {
    if (v.t != V_OBJ || !v.as.o) return;
    Obj* o = v.as.o;
    if (o->marked || o->pinned) return;
    o->marked = 1;
    if (o->type == O_STRING) return;
    if (vm->ngray == vm->capgray) {
        int nc = vm->capgray ? vm->capgray * 2 : 256;
        Obj** g = (Obj**)realloc(vm->gray, sizeof(Obj*) * nc);
        if (!g) return;     /* worst case: children stay unmarked; avoided by reserving below */
        vm->gray = g; vm->capgray = nc;
    }
    vm->gray[vm->ngray++] = o;
}

/* the size of the object's own memory (obj_size also counts a list's or dictionary's buffers) */
static size_t alloc_size(Obj* o) {
    return o->type == O_LIST ? sizeof(ObjList) : o->type == O_DICT ? sizeof(ObjDict) : obj_size(o);
}

static void free_obj(VM* vm, Obj* o, size_t size) {
    if (o->type == O_LIST) free(((ObjList*)o)->items);
    if (o->type == O_DICT) { free(((ObjDict*)o)->entries); free(((ObjDict*)o)->index); }
    if (size <= MS_POOL_MAX) {                  /* back to the list of its size class */
        int k = (int)((size + 7) >> 3);
        *(void**)o = vm->pool[k]; vm->pool[k] = o;
    } else free(o);
}

/* memory for a small object: from the list of its size class, else cut from the current block */
static void* pool_alloc(VM* vm, size_t size) {
    int k = (int)((size + 7) >> 3);
    void* p = vm->pool[k];
    if (p) { vm->pool[k] = *(void**)p; return p; }
    size_t n = (size_t)k << 3;
    if (vm->bumpLeft < n) {
        char* b = (char*)malloc(16384);
        if (!b) return NULL;
        *(void**)b = vm->blocks; vm->blocks = b;            /* blocks are freed with the VM */
        vm->bump = b + 8; vm->bumpLeft = 16384 - 8;
    }
    p = vm->bump; vm->bump += n; vm->bumpLeft -= n;
    return p;
}

static MS_COLD void collect(VM* vm) {
    for (int i = 0; i < vm->sp; i++) mark_value(vm, vm->stack[i]);
    for (int i = 0; i < vm->p->nstatics; i++) mark_value(vm, vm->statics[i]);
    for (int i = 0; i < vm->ncoros; i++) { Value v; v.t = V_OBJ; v.as.o = (Obj*)vm->coros[i]; mark_value(vm, v); }
    for (int i = 0; i < vm->fp; i++)
        if (vm->frames[i].clo) { Value v; v.t = V_OBJ; v.as.o = vm->frames[i].clo; mark_value(vm, v); }
    while (vm->ngray > 0) {
        Obj* o = vm->gray[--vm->ngray];
        if (o->type == O_ARRAY) {
            ObjArray* a = (ObjArray*)o;
            for (int i = 0; i < a->len; i++) mark_value(vm, a->items[i]);
        } else if (o->type == O_LIST) {
            ObjList* l = (ObjList*)o;
            for (int i = 0; i < l->len; i++) mark_value(vm, l->items[i]);
        } else if (o->type == O_DICT) {
            ObjDict* d = (ObjDict*)o;
            for (int i = 0; i < d->used; i++)
                if (d->entries[i].live) { mark_value(vm, d->entries[i].key); mark_value(vm, d->entries[i].val); }
        } else if (o->type == O_CELL) {
            mark_value(vm, ((ObjCell*)o)->v);
        } else if (o->type == O_CLOSURE) {
            ObjClosure* cl = (ObjClosure*)o;
            mark_value(vm, cl->self);
            for (int i = 0; i < cl->nup; i++) mark_value(vm, cl->ups[i]);
        } else if (o->type == O_CORO) {
            ObjCoro* co = (ObjCoro*)o;
            for (int i = 0; i < co->nlocals; i++) mark_value(vm, co->locals[i]);
            if (co->waitFor) { Value w; w.t = V_OBJ; w.as.o = co->waitFor; mark_value(vm, w); }
            if (co->owner) { Value w; w.t = V_OBJ; w.as.o = co->owner; mark_value(vm, w); }
        } else {
            ObjInstance* in = (ObjInstance*)o;
            for (int i = 0; i < in->nfields; i++) mark_value(vm, in->fields[i]);
        }
    }
    Obj** link = &vm->objects;
    while (*link) {
        Obj* o = *link;
        if (o->marked) { o->marked = 0; link = &o->next; }
        else { *link = o->next; vm->bytes -= obj_size(o); free_obj(vm, o, alloc_size(o)); }
    }
    vm->nextGC = vm->bytes * 2;
    if (vm->nextGC < 1024 * 1024) vm->nextGC = 1024 * 1024;
}

static MS_COLD Obj* alloc_obj(VM* vm, size_t size, int type) {
    if (vm->bytes + size > vm->nextGC) collect(vm);
    Obj* o = (Obj*)(size <= MS_POOL_MAX ? pool_alloc(vm, size) : malloc(size));
    if (!o) { collect(vm); o = (Obj*)(size <= MS_POOL_MAX ? pool_alloc(vm, size) : malloc(size)); if (!o) return NULL; }
    o->type = (uint8_t)type; o->marked = 0; o->pinned = 0;
    o->next = vm->objects; vm->objects = o;
    vm->bytes += size;
    return o;
}

static MS_COLD ObjString* new_string(VM* vm, int len) {
    ObjString* s = (ObjString*)alloc_obj(vm, sizeof(ObjString) + len + 1, O_STRING);
    if (s) { s->len = len; s->chars[len] = 0; }
    return s;
}

Value vm_make_string(VM* vm, const char* str, int len) {
    Value v; v.t = V_NULL; v.as.o = NULL;
    ObjString* s = new_string(vm, len);
    if (!s) return v;
    memcpy(s->chars, str, len);
    v.t = V_OBJ; v.as.o = (Obj*)s;
    return v;
}

Value vm_make_array(VM* vm, int len) {
    Value v; v.t = V_NULL; v.as.o = NULL;
    ObjArray* a = (ObjArray*)alloc_obj(vm, sizeof(ObjArray) + sizeof(Value) * len, O_ARRAY);
    if (!a) return v;
    a->len = len;
    for (int i = 0; i < len; i++) { a->items[i].t = V_NULL; a->items[i].as.o = NULL; }
    v.t = V_OBJ; v.as.o = (Obj*)a;
    return v;
}

/* makes room for one more item; 0 = out of memory */
static int list_reserve(VM* vm, ObjList* l) {
    if (l->len < l->cap) return 1;
    int nc = l->cap ? l->cap * 2 : 8;
    Value* items = (Value*)realloc(l->items, sizeof(Value) * nc);
    if (!items) return 0;
    vm->bytes += sizeof(Value) * (size_t)(nc - l->cap);
    l->items = items; l->cap = nc;
    return 1;
}

/* ================= helpers ================= */
static void fmt_float(float f, char* buf, int n) {
    if (isnan(f)) { snprintf(buf, n, "NaN"); return; }
    if (isinf(f)) { snprintf(buf, n, f > 0 ? "Infinity" : "-Infinity"); return; }
    snprintf(buf, n, "%.7g", (double)f);
}

void ms_value_to_text(VM* vm, Value v, char* buf, int n) {
    switch (v.t) {
        case V_NULL: buf[0] = 0; return;
        case V_INT: snprintf(buf, n, "%d", (int)v.as.i); return;
        case V_FLOAT: fmt_float(v.as.f, buf, n); return;
        case V_BOOL: snprintf(buf, n, v.as.i ? "True" : "False"); return;
        case V_CHAR: buf[0] = (char)v.as.i; buf[1] = 0; return;
        default: {
            Obj* o = v.as.o;
            if (o->type == O_STRING) snprintf(buf, n, "%s", ((ObjString*)o)->chars);
            else if (o->type == O_ARRAY) snprintf(buf, n, "Array[%d]", (int)((ObjArray*)o)->len);
            else if (o->type == O_LIST) snprintf(buf, n, "List[%d]", (int)((ObjList*)o)->len);
            else if (o->type == O_DICT) snprintf(buf, n, "Dictionary[%d]", (int)((ObjDict*)o)->count);
            else if (o->type == O_CORO) snprintf(buf, n, "Coroutine");
            else if (o->type == O_CLOSURE) snprintf(buf, n, "Delegate");
            else snprintf(buf, n, "%s", vm->p->classes[((ObjInstance*)o)->cls].name);
        }
    }
}

/* ================= VM ================= */
VM* vm_new(Program* p) {
    VM* vm = (VM*)calloc(1, sizeof(VM));
    if (!vm) return NULL;
    vm->p = p;
    for (int i = 0; i < nresetHooks; i++) resetHooks[i]();   /* layers and modules reset their state (ms_on_vm_new) */
    vm->stack = (Value*)calloc(MS_STACK, sizeof(Value));
    vm->statics = (Value*)calloc(p->nstatics ? p->nstatics : 1, sizeof(Value));
    if (!vm->stack || !vm->statics) { vm_free(vm); return NULL; }
    for (int i = 0; i < p->nstatics; i++) vm->statics[i] = p->staticDefaults[i];
    vm->nextGC = 1024 * 1024;
    return vm;
}

int vm_set_program(VM* vm, Program* p) {
    int old = vm->p->nstatics;
    if (p->nstatics > old) {
        Value* s = (Value*)realloc(vm->statics, sizeof(Value) * p->nstatics);
        if (!s) return 0;
        vm->statics = s;
        for (int i = old; i < p->nstatics; i++) s[i] = p->staticDefaults[i];
    }
    vm->p = p;
    return 1;
}

void vm_free(VM* vm) {
    if (!vm) return;
    Obj* o = vm->objects;
    while (o) { Obj* n = o->next; free_obj(vm, o, alloc_size(o)); o = n; }
    for (void* b = vm->blocks; b; ) { void* n = *(void**)b; free(b); b = n; }
    free(vm->gray); free(vm->stack); free(vm->statics); free(vm->coros);
    free(vm);
}

static int runtime_error(VM* vm, Frame* fr, int ip, const char* msg) {
    int line = (ip > 0 && ip <= fr->f->len) ? fr->f->lines[ip - 1] : 0;
    snprintf(vm->err, sizeof vm->err, "line %d (%.63s): %.170s", line, fr->f->name, msg);
    return 0;
}

#define STR(v) ((ObjString*)(v).as.o)
#define ARR(v) ((ObjArray*)(v).as.o)
#define INST(v) ((ObjInstance*)(v).as.o)

static int str_equal(Value a, Value b) {
    if (a.t == V_NULL || b.t == V_NULL) return a.t == b.t;
    ObjString* x = STR(a); ObjString* y = STR(b);
    return x == y || (x->len == y->len && !memcmp(x->chars, y->chars, x->len));
}

/* List.IndexOf/Remove, Dictionary keys: strings by content, structs field by field (like .NET's Equals),
   other objects by reference, numbers by value */
static Program* g_prog;                 /* the running program (for structs) */
static int value_equal(Value a, Value b) {
    if (a.t == V_NULL || b.t == V_NULL) return a.t == b.t;
    if (a.t == V_OBJ && b.t == V_OBJ) {
        if (a.as.o->type == O_STRING && b.as.o->type == O_STRING) return str_equal(a, b);
        if (a.as.o != b.as.o && a.as.o->type == O_INSTANCE && b.as.o->type == O_INSTANCE && INST(a)->cls == INST(b)->cls &&
            g_prog && g_prog->classes[INST(a)->cls].isStruct) {
            for (int i = 0; i < INST(a)->nfields; i++) if (!value_equal(INST(a)->fields[i], INST(b)->fields[i])) return 0;
            return 1;
        }
        return a.as.o == b.as.o;
    }
    if (a.t == V_FLOAT) return b.t == V_FLOAT && a.as.f == b.as.f;
    return a.as.i == b.as.i;
}

static int list_index_of(ObjList* l, Value v) {
    for (int i = 0; i < l->len; i++) if (value_equal(l->items[i], v)) return i;
    return -1;
}

/* ================= Dictionary ================= */
static uint32_t hash_value(Value v) {
    if (v.t == V_OBJ) {
        if (v.as.o->type == O_STRING) {         /* FNV-1a over the characters */
            ObjString* s = STR(v); uint32_t h = 2166136261u;
            for (int i = 0; i < s->len; i++) { h ^= (unsigned char)s->chars[i]; h *= 16777619u; }
            return h;
        }
        if (v.as.o->type == O_INSTANCE && g_prog && g_prog->classes[INST(v)->cls].isStruct) {
            uint32_t h = 17;
            for (int i = 0; i < INST(v)->nfields; i++) h = h * 31u + hash_value(INST(v)->fields[i]);
            return h;
        }
        uintptr_t p = (uintptr_t)v.as.o;
        return (uint32_t)(p >> 3) * 2654435761u;
    }
    if (v.t == V_NULL) return 0;
    uint32_t bits = (uint32_t)v.as.i;           /* also the float's bits */
    return bits * 2654435761u ^ (bits >> 15);
}

static int dict_find(ObjDict* d, Value key, uint32_t h) {
    if (!d->icap) return -1;
    for (uint32_t i = h & (uint32_t)(d->icap - 1);; i = (i + 1) & (uint32_t)(d->icap - 1)) {
        int e = d->index[i];
        if (e == -1) return -1;
        if (e >= 0 && d->entries[e].hash == h && value_equal(d->entries[e].key, key)) return e;
    }
}

/* makes room for one more entry: grows the entry array when it is full (entries never move, so
 * the order is kept) and rebuilds the index when it gets too full of entries or removed marks */
static MS_COLD int dict_reserve(VM* vm, ObjDict* d) {
    int needEntry = d->freeCount == 0 && d->used == d->cap;
    int filled = d->used - d->freeCount + d->ntomb + 1;          /* index slots in use after this Add */
    if (!needEntry && filled * 4 <= d->icap * 3) return 1;
    int cap = d->cap;
    if (needEntry) cap = cap ? cap * 2 : 8;
    int icap = 16;
    while (icap < cap * 2) icap *= 2;
    DictEntry* ne = cap == d->cap ? d->entries : (DictEntry*)realloc(d->entries, sizeof(DictEntry) * (size_t)cap);
    if (!ne) return 0;
    vm->bytes += sizeof(DictEntry) * (size_t)(cap - d->cap);
    d->entries = ne; d->cap = cap;
    int32_t* ni = (int32_t*)malloc(sizeof(int32_t) * (size_t)icap);
    if (!ni) return 0;
    for (int i = 0; i < icap; i++) ni[i] = -1;
    for (int e = 0; e < d->used; e++) {
        if (!ne[e].live) continue;
        uint32_t i = ne[e].hash & (uint32_t)(icap - 1);
        while (ni[i] != -1) i = (i + 1) & (uint32_t)(icap - 1);
        ni[i] = e;
    }
    vm->bytes += sizeof(int32_t) * (size_t)icap;
    vm->bytes -= sizeof(int32_t) * (size_t)d->icap;
    free(d->index);
    d->index = ni; d->icap = icap; d->ntomb = 0;
    return 1;
}

/* 1 = stored, 0 = out of memory, -1 = key exists and onlyNew was set */
static MS_COLD int dict_put(VM* vm, ObjDict* d, Value key, Value val, int onlyNew) {
    uint32_t h = hash_value(key);
    int e = dict_find(d, key, h);
    if (e >= 0) { if (onlyNew) return -1; d->entries[e].val = val; return 1; }
    if (!dict_reserve(vm, d)) return 0;
    uint32_t i = h & (uint32_t)(d->icap - 1);
    while (d->index[i] >= 0) i = (i + 1) & (uint32_t)(d->icap - 1);
    if (d->index[i] == -2) d->ntomb--;
    if (d->freeCount > 0) { e = d->freeList; d->freeList = (int32_t)d->entries[e].hash; d->freeCount--; }   /* like .NET */
    else e = d->used++;
    d->entries[e].key = key; d->entries[e].val = val; d->entries[e].hash = h; d->entries[e].live = 1;
    d->index[i] = e;
    d->count++;
    return 1;
}

static int dict_remove(ObjDict* d, Value key) {
    uint32_t h = hash_value(key);
    if (!d->icap) return 0;
    for (uint32_t i = h & (uint32_t)(d->icap - 1);; i = (i + 1) & (uint32_t)(d->icap - 1)) {
        int e = d->index[i];
        if (e == -1) return 0;
        if (e >= 0 && d->entries[e].hash == h && value_equal(d->entries[e].key, key)) {
            d->index[i] = -2; d->ntomb++;
            d->entries[e].live = 0; d->count--;
            d->entries[e].key.t = V_NULL; d->entries[e].val.t = V_NULL;
            d->entries[e].hash = (uint32_t)d->freeList; d->freeList = e; d->freeCount++;
            return 1;
        }
    }
}

static MS_COLD ObjList* new_list(VM* vm, int n) {
    ObjList* l = (ObjList*)alloc_obj(vm, sizeof(ObjList), O_LIST);
    if (!l) return NULL;
    l->len = 0; l->cap = 0; l->items = NULL;
    if (n > 0) {
        l->items = (Value*)malloc(sizeof(Value) * (size_t)n);
        if (!l->items) return NULL;             /* the empty list is collected later */
        l->cap = n; vm->bytes += sizeof(Value) * (size_t)n;
    }
    return l;
}

/* A new object of class ci with its default field values; struct fields get zeroed structs. The caller has done
   SYNC(), so the new object can be kept on the stack while its parts are made. */
static MS_COLD Obj* new_instance(VM* vm, int ci) {
    ClassRT* k = &vm->p->classes[ci];
    ObjInstance* in = (ObjInstance*)alloc_obj(vm, sizeof(ObjInstance) + sizeof(Value) * k->nfields, O_INSTANCE);
    if (!in) return NULL;
    in->cls = ci; in->nfields = k->nfields;
    for (int i = 0; i < k->nfields; i++) in->fields[i] = k->defaults[i];
    if (k->nstructf) {
        if (vm->sp + 64 >= MS_STACK) return NULL;
        vm->stack[vm->sp].t = V_OBJ; vm->stack[vm->sp].as.o = (Obj*)in; vm->sp++;
        for (int j = 0; j < k->nstructf; j++) {
            Obj* f = new_instance(vm, k->structfCls[j]);
            if (!f) { vm->sp--; return NULL; }
            in->fields[k->structfIdx[j]].t = V_OBJ; in->fields[k->structfIdx[j]].as.o = f;
        }
        vm->sp--;
    }
    return (Obj*)in;
}

/* a copy of struct src (its struct fields are copied too); same rules as new_instance */
static MS_COLD Obj* copy_instance(VM* vm, ObjInstance* src) {
    ClassRT* k = &vm->p->classes[src->cls];
    if (vm->sp + 64 >= MS_STACK) return NULL;
    vm->stack[vm->sp].t = V_OBJ; vm->stack[vm->sp].as.o = (Obj*)src; vm->sp++;     /* src may be only in a register */
    ObjInstance* in = (ObjInstance*)alloc_obj(vm, sizeof(ObjInstance) + sizeof(Value) * src->nfields, O_INSTANCE);
    if (!in) { vm->sp--; return NULL; }
    in->cls = src->cls; in->nfields = src->nfields;
    memcpy(in->fields, src->fields, sizeof(Value) * (size_t)src->nfields);
    if (k->nstructf) {
        vm->stack[vm->sp].t = V_OBJ; vm->stack[vm->sp].as.o = (Obj*)in; vm->sp++;
        for (int j = 0; j < k->nstructf; j++) {
            Value* fv = &in->fields[k->structfIdx[j]];
            if (fv->t != V_OBJ) continue;
            Obj* f = copy_instance(vm, (ObjInstance*)fv->as.o);
            if (!f) { vm->sp -= 2; return NULL; }
            fv->as.o = f;
        }
        vm->sp--;
    }
    vm->sp--;
    return (Obj*)in;
}

/* default(T) for a default-value kind (see MS_KIND_STRUCT); 0 = out of memory */
static MS_COLD int make_default(VM* vm, int kind, Value* out) {
    out->as.i = 0;
    if (kind >= MS_KIND_STRUCT) {
        Obj* o = new_instance(vm, kind - MS_KIND_STRUCT);
        if (!o) return 0;
        out->t = V_OBJ; out->as.o = o;
        return 1;
    }
    out->t = kind == 1 ? V_INT : kind == 2 ? V_FLOAT : kind == 3 ? V_BOOL : kind == 4 ? V_CHAR : V_NULL;
    if (kind == 2) out->as.f = 0;
    return 1;
}

/* the text of enum value v: its member's name, "A, B" for a [Flags] combination, or the number */
static MS_COLD void enum_text(Program* P, int ci, int32_t v, char* buf, int n) {
    ClassRT* k = &P->classes[ci];
    for (int i = 0; i < k->nenum; i++)
        if (k->enumVals[i] == v) { snprintf(buf, n, "%s", P->strings[k->enumStr[i]]->chars); return; }
    if (k->isFlags && v) {
        /* like .NET: the biggest values first, then printed from the smallest */
        int used[64], nu = 0; uint32_t rest = (uint32_t)v;
        for (int pass = 0; pass < k->nenum && rest; pass++) {
            int best = -1;
            for (int i = 0; i < k->nenum; i++) {
                uint32_t ev = (uint32_t)k->enumVals[i];
                int taken = 0;
                for (int j = 0; j < nu; j++) taken |= used[j] == i;
                if (!ev || taken || (ev & rest) != ev) continue;
                if (best < 0 || ev > (uint32_t)k->enumVals[best]) best = i;
            }
            if (best < 0 || nu == 64) break;
            used[nu++] = best; rest &= ~(uint32_t)k->enumVals[best];
        }
        if (!rest) {
            int len = 0; buf[0] = 0;
            for (int j = nu - 1; j >= 0 && len < n; j--)
                len += snprintf(buf + len, n - len, "%s%s", j == nu - 1 ? "" : ", ", P->strings[k->enumStr[used[j]]]->chars);
            return;
        }
    }
    snprintf(buf, n, "%d", (int)v);
}

static int vm_run(VM* vm, int startFp);

int vm_call(VM* vm, int func) {
    Program* P = vm->p;
    if (func < 0 || func >= P->nfuncs) return 1;
    int startFp = vm->fp;
    if (vm->fp >= MS_FRAMES) { snprintf(vm->err, sizeof vm->err, "call stack overflow"); return 0; }
    Frame* fr = &vm->frames[vm->fp++];
    fr->f = &P->funcs[func]; fr->ip = 0; fr->base = vm->sp; fr->clo = NULL;
    for (int i = 0; i < fr->f->nlocals; i++) { vm->stack[vm->sp + i].t = V_NULL; vm->stack[vm->sp + i].as.o = NULL; }
    vm->sp += fr->f->nlocals;
    return vm_run(vm, startFp);
}

/* ================= coroutines ================= */
/* StopCoroutine: also stops the nested coroutine it started with 'yield return Other()', like Unity */
static void coro_stop(ObjCoro* co) {
    while (co && !co->done) {
        co->done = 1;
        ObjCoro* child = co->waitKind == Y_CORO ? (ObjCoro*)co->waitFor : NULL;
        co = child && child->owner == (Obj*)co ? child : NULL;
    }
}

/* Runs a coroutine until its next yield or its end. 0 = runtime error or quit. */
static MS_COLD int coro_resume(VM* vm, ObjCoro* co) {
    Func* f = &vm->p->funcs[co->func];
    if (vm->fp >= MS_FRAMES || vm->sp + co->nlocals + 512 >= MS_STACK) { snprintf(vm->err, sizeof vm->err, "stack overflow (coroutine)"); return 0; }
    int startFp = vm->fp;
    Frame* fr = &vm->frames[vm->fp++];
    fr->f = f; fr->ip = co->ip; fr->base = vm->sp; fr->clo = NULL;
    memcpy(vm->stack + vm->sp, co->locals, sizeof(Value) * (size_t)co->nlocals);
    vm->sp += co->nlocals;
    ObjCoro* prev = vm->curCoro;
    vm->curCoro = co; co->running = 1;
    int r = vm_run(vm, startFp);
    vm->curCoro = prev; co->running = 0;
    if (r == 0) return 0;
    if (r == 1) {                       /* reached the end or 'yield break' */
        co->done = 1;
        /* like Unity, a coroutine waiting for this one continues in the same frame */
        for (int i = 0; i < vm->ncoros; i++) {
            ObjCoro* w = vm->coros[i];
            if (w->waitKind == Y_CORO && w->waitFor == (Obj*)co && !w->done && !w->running) {
                w->waitFor = NULL; w->waitKind = Y_NEXT; w->lastFrame = -1;
                if (!coro_resume(vm, w)) return 0;
            }
        }
    }
    return 1;
}

/* StartCoroutine: registers it and runs it at once up to the first yield, like Unity */
static MS_COLD int coro_start(VM* vm, ObjCoro* co) {
    if (co->started) return 1;
    co->started = 1;
    if (vm->ncoros == vm->capcoros) {
        int nc = vm->capcoros ? vm->capcoros * 2 : 16;
        ObjCoro** a = (ObjCoro**)realloc(vm->coros, sizeof(ObjCoro*) * nc);
        if (!a) { snprintf(vm->err, sizeof vm->err, "out of memory"); return 0; }
        vm->coros = a; vm->capcoros = nc;
    }
    vm->coros[vm->ncoros++] = co;
    return coro_resume(vm, co);
}

int vm_tick_coroutines(VM* vm) {
    int now = ms_millis();
    int n = vm->ncoros;                 /* ones started during this tick already ran */
    for (int i = 0; i < n; i++) {
        ObjCoro* co = vm->coros[i];
        if (co->done || co->running || co->lastFrame == vm->frameNo) continue;
        if (co->waitKind == Y_SECONDS && now < co->waitUntil) continue;
        if (co->waitKind == Y_CORO && co->waitFor && !((ObjCoro*)co->waitFor)->done) continue;
        if (!coro_resume(vm, co)) return 0;
    }
    int k = 0;
    for (int i = 0; i < vm->ncoros; i++) if (!vm->coros[i]->done) vm->coros[k++] = vm->coros[i];
    vm->ncoros = k;
    vm->frameNo++;
    return 1;
}

/* Calling an iterator method: the arguments on the stack become a new coroutine. 0 = out of memory */
static MS_COLD int coro_new(VM* vm, int fidx, int argc, Value** spp) {
    Value* sp = *spp;
    int nl = vm->p->funcs[fidx].nlocals;
    ObjCoro* co = (ObjCoro*)alloc_obj(vm, sizeof(ObjCoro) + sizeof(Value) * nl, O_CORO);
    if (!co) return 0;
    co->func = fidx; co->ip = 0; co->nlocals = nl;
    co->started = co->done = co->running = 0; co->waitKind = Y_NEXT;
    co->waitUntil = 0; co->lastFrame = -1; co->waitFor = NULL; co->owner = NULL;
    for (int i = 0; i < nl; i++) {
        if (i < argc) co->locals[i] = sp[i - argc];
        else { co->locals[i].t = V_NULL; co->locals[i].as.o = NULL; }
    }
    sp -= argc;
    sp->t = V_OBJ; sp->as.o = (Obj*)co; sp++;
    *spp = sp;
    return 1;
}

/* ================= delegates ================= */
/* Runs delegate cl(args) to its end, nested in the running VM loop (the caller has done SYNC() and keeps
 * everything it needs on the stack). *out = its result, if it returns one. 0 = runtime error or quit. */
static MS_COLD int call_delegate(VM* vm, ObjClosure* cl, const Value* args, int argc, Value* out) {
    if (cl->kind == CL_MULTI) {
        for (int i = 0; i < cl->nup; i++)
            if (!call_delegate(vm, (ObjClosure*)cl->ups[i].as.o, args, argc, out)) return 0;
        return 1;
    }
    Func* f = &vm->p->funcs[cl->func];
    if (f->isIter) { snprintf(vm->err, sizeof vm->err, "a coroutine cannot be called here; use StartCoroutine"); return 0; }
    if (vm->fp >= MS_FRAMES || vm->sp + argc + f->nlocals + 512 >= MS_STACK) { snprintf(vm->err, sizeof vm->err, "stack overflow (too much recursion?)"); return 0; }
    int base = vm->sp, n = 0;
    Value* st = vm->stack + base;
    if (cl->kind != CL_STATIC) st[n++] = cl->self;
    for (int i = 0; i < argc; i++) st[n++] = args[i];
    for (int i = n; i < f->nlocals; i++) { st[i].t = V_NULL; st[i].as.o = NULL; }
    int top = vm->fp;
    Frame* fr = &vm->frames[vm->fp++];
    fr->f = f; fr->ip = 0; fr->base = base; fr->clo = (Obj*)cl;
    vm->sp = base + f->nlocals;
    if (vm_run(vm, top) != 1) return 0;
    *out = vm->stack[base];
    vm->sp = base;
    return 1;
}

static int deleg_same(ObjClosure* a, ObjClosure* b) {
    if (a->func != b->func || a->kind != b->kind || a->nup != b->nup || a->self.t != b->self.t) return 0;
    if (a->self.t == V_OBJ && a->self.as.o != b->self.as.o) return 0;
    for (int i = 0; i < a->nup; i++) if (a->ups[i].as.o != b->ups[i].as.o) return 0;
    return 1;
}
/* the single delegates in d, in call order */
static int deleg_parts(ObjClosure* d, ObjClosure** out) {
    if (d->kind != CL_MULTI) { out[0] = d; return 1; }
    for (int i = 0; i < d->nup; i++) out[i] = (ObjClosure*)d->ups[i].as.o;
    return d->nup;
}

/* a += b, a -= b (removes the last place where b's delegates are, in order, as in .NET). 0 = out of memory */
static MS_COLD int deleg_op(VM* vm, int remove, Value a, Value b, Value* out) {
    if (a.t != V_OBJ) { if (remove) out->t = V_NULL, out->as.o = NULL; else *out = b; return 1; }
    if (b.t != V_OBJ) { *out = a; return 1; }
    ObjClosure* da = (ObjClosure*)a.as.o; ObjClosure* db = (ObjClosure*)b.as.o;
    int na = da->kind == CL_MULTI ? da->nup : 1, nb = db->kind == CL_MULTI ? db->nup : 1;
    ObjClosure** pa = (ObjClosure**)malloc(sizeof(ObjClosure*) * (size_t)(na + nb));
    if (!pa) return 0;
    deleg_parts(da, pa);
    ObjClosure** pb = pa + na;
    deleg_parts(db, pb);
    int n = na;
    if (!remove) { n = na + nb; }
    else {
        int at = -1;
        for (int i = na - nb; i >= 0 && at < 0; i--) {
            int ok = 1;
            for (int j = 0; j < nb && ok; j++) ok = deleg_same(pa[i + j], pb[j]);
            if (ok) at = i;
        }
        if (at < 0) { free(pa); *out = a; return 1; }
        memmove(pa + at, pa + at + nb, sizeof(ObjClosure*) * (size_t)(na - at - nb));
        n = na - nb;
    }
    if (n == 0) { free(pa); out->t = V_NULL; out->as.o = NULL; return 1; }
    if (n == 1) { out->t = V_OBJ; out->as.o = (Obj*)pa[0]; free(pa); return 1; }
    ObjClosure* m = (ObjClosure*)alloc_obj(vm, sizeof(ObjClosure) + sizeof(Value) * (size_t)n, O_CLOSURE);   /* a and b are on the stack */
    if (!m) { free(pa); return 0; }
    m->func = 0; m->kind = CL_MULTI; m->nup = n; m->self.t = V_NULL; m->self.as.o = NULL;
    for (int i = 0; i < n; i++) { m->ups[i].t = V_OBJ; m->ups[i].as.o = (Obj*)pa[i]; }
    free(pa);
    out->t = V_OBJ; out->as.o = (Obj*)m;
    return 1;
}

/* ================= sorting ================= */
static int text_compare(ObjString* a, ObjString* b) {
    /* close to .NET's default (culture) order for ASCII: letters ignoring case first, then lower case before upper */
    int n = a->len < b->len ? a->len : b->len;
    for (int i = 0; i < n; i++) {
        int x = tolower((unsigned char)a->chars[i]), y = tolower((unsigned char)b->chars[i]);
        if (x != y) return x < y ? -1 : 1;
    }
    if (a->len != b->len) return a->len < b->len ? -1 : 1;
    for (int i = 0; i < n; i++)
        if (a->chars[i] != b->chars[i]) return islower((unsigned char)a->chars[i]) ? -1 : 1;
    return 0;
}
static int value_compare(Value a, Value b) {
    if (a.t == V_FLOAT) return a.as.f < b.as.f ? -1 : a.as.f > b.as.f ? 1 : 0;
    if (a.t == V_INT || a.t == V_CHAR) return a.as.i < b.as.i ? -1 : a.as.i > b.as.i ? 1 : 0;
    if (a.t != V_OBJ) return b.t != V_OBJ ? 0 : -1;         /* null first */
    if (b.t != V_OBJ) return 1;
    return text_compare((ObjString*)a.as.o, (ObjString*)b.as.o);
}
/* stable merge sort of a[0..n) using tmp[0..n); cmp NULL = the natural order. 0 = error in cmp */
static MS_COLD int merge_sort(VM* vm, Value* a, Value* tmp, int n, ObjClosure* cmp) {
    Value* src = a; Value* dst = tmp;
    for (int w = 1; w < n; w *= 2) {
        for (int lo = 0; lo < n; lo += 2 * w) {
            int mid = lo + w < n ? lo + w : n, hi = lo + 2 * w < n ? lo + 2 * w : n;
            int i = lo, j = mid, k = lo;
            while (i < mid && j < hi) {
                int r;
                if (cmp) {
                    Value args[2] = { src[i], src[j] }, res;
                    if (!call_delegate(vm, cmp, args, 2, &res)) return 0;
                    r = res.as.i;
                } else r = value_compare(src[i], src[j]);
                dst[k++] = r <= 0 ? src[i++] : src[j++];
            }
            while (i < mid) dst[k++] = src[i++];
            while (j < hi) dst[k++] = src[j++];
        }
        Value* t = src; src = dst; dst = t;
    }
    if (src != a) memcpy(a, src, sizeof(Value) * (size_t)n);
    return 1;
}

/* Runs from the top frame until the frame at startFp returns (1), yields (2) or fails (0). */
static int vm_run(VM* vm, int startFp) {
    Program* P = vm->p;
    g_prog = P;
    Frame* fr = &vm->frames[vm->fp - 1];
    Value* stack = vm->stack;
    Value* sp = stack + vm->sp;
    Value* bp = stack + fr->base;
    int32_t* code = fr->f->code;
    int ip = fr->ip;

#define SYNC() (vm->sp = (int)(sp - stack), fr->ip = ip)
#define FAIL(msg) do { SYNC(); runtime_error(vm, fr, ip, msg); vm->fp = startFp; return 0; } while (0)
#define FAILED() do { vm->fp = startFp; return 0; } while (0)   /* vm->err is already set (or quit) */
#define ARG() (code[ip++])

#if defined(__GNUC__) && !defined(MS_NO_THREADED)
    /* threaded dispatch: every instruction jumps straight to the next one's code (one predictable jump each) */
    static void* const disp[] = {
        &&L_OP_ICONST,
        &&L_OP_FCONST,
        &&L_OP_SCONST,
        &&L_OP_NULL,
        &&L_OP_TRUE,
        &&L_OP_FALSE,
        &&L_OP_POP,
        &&L_OP_DUP,
        &&L_OP_DUP2,
        &&L_OP_LOAD,
        &&L_OP_STORE,
        &&L_OP_STOREP,
        &&L_OP_GETSTATIC,
        &&L_OP_SETSTATIC,
        &&L_OP_GETFIELD,
        &&L_OP_SETFIELD,
        &&L_OP_GETELEM,
        &&L_OP_SETELEM,
        &&L_OP_IADD,
        &&L_OP_ISUB,
        &&L_OP_IMUL,
        &&L_OP_IDIV,
        &&L_OP_IMOD,
        &&L_OP_INEG,
        &&L_OP_FADD,
        &&L_OP_FSUB,
        &&L_OP_FMUL,
        &&L_OP_FDIV,
        &&L_OP_FMOD,
        &&L_OP_FNEG,
        &&L_OP_I2F,
        &&L_OP_F2I,
        &&L_OP_TOSTR,
        &&L_OP_CONCAT,
        &&L_OP_ILT,
        &&L_OP_ILE,
        &&L_OP_IGT,
        &&L_OP_IGE,
        &&L_OP_IEQ,
        &&L_OP_INE,
        &&L_OP_FLT,
        &&L_OP_FLE,
        &&L_OP_FGT,
        &&L_OP_FGE,
        &&L_OP_FEQ,
        &&L_OP_FNE,
        &&L_OP_SEQ,
        &&L_OP_SNE,
        &&L_OP_REQ,
        &&L_OP_RNE,
        &&L_OP_NOT,
        &&L_OP_JMP,
        &&L_OP_JZ,
        &&L_OP_JFALSE_KEEP,
        &&L_OP_JTRUE_KEEP,
        &&L_OP_CALL,
        &&L_OP_CALLN,
        &&L_OP_NULLCHK,
        &&L_OP_RET,
        &&L_OP_RETV,
        &&L_OP_NEW,
        &&L_OP_NEWARR,
        &&L_OP_ARRLEN,
        &&L_OP_STRLEN,
        &&L_OP_NORET,
        &&L_OP_IAND,
        &&L_OP_IOR,
        &&L_OP_IXOR,
        &&L_OP_ISHL,
        &&L_OP_ISHR,
        &&L_OP_INOT,
        &&L_OP_I2C,
        &&L_OP_C2I,
        &&L_OP_LIST,
        &&L_OP_CORO_NEW,
        &&L_OP_YIELD,
        &&L_OP_CORO,
        &&L_OP_DICT,
        &&L_OP_CALLV,
        &&L_OP_ISINST,
        &&L_OP_BOX,
        &&L_OP_GETCELL,
        &&L_OP_SETCELL,
        &&L_OP_UPVAL,
        &&L_OP_CLOSURE,
        &&L_OP_CALLF,
        &&L_OP_DELEG,
        &&L_OP_JNULL,
        &&L_OP_JNULL_POP,
        &&L_OP_JNNULL,
        &&L_OP_LISTF,
        &&L_OP_COPY,
        &&L_OP_JNLT, &&L_OP_JNLE, &&L_OP_JNGT, &&L_OP_JNGE, &&L_OP_JNEQ, &&L_OP_JNNE,
        &&L_OP_IADDK, &&L_OP_INCL, &&L_OP_GETFIELDL
    };
#define CASE(x) case x: L_##x:
#ifdef MS_PROFILE
#define NEXT do { o = code[ip++]; ms_prof_count(o); goto *disp[o]; } while (0)
#else
#define NEXT do { o = code[ip++]; goto *disp[o]; } while (0)
#endif
#else
#define CASE(x) case x:
#define NEXT break
#endif
    int o;
    for (;;) {
        o = code[ip++];
#ifdef MS_PROFILE
        ms_prof_count(o);
#endif
        switch (o) {
            CASE(OP_ICONST) sp->t = V_INT; sp->as.i = ARG(); sp++; NEXT;
            CASE(OP_FCONST) { int32_t b = ARG(); sp->t = V_FLOAT; memcpy(&sp->as.f, &b, 4); sp++; NEXT; }
            CASE(OP_SCONST) sp->t = V_OBJ; sp->as.o = (Obj*)P->strings[ARG()]; sp++; NEXT;
            CASE(OP_NULL) sp->t = V_NULL; sp->as.o = NULL; sp++; NEXT;
            CASE(OP_TRUE) sp->t = V_BOOL; sp->as.i = 1; sp++; NEXT;
            CASE(OP_FALSE) sp->t = V_BOOL; sp->as.i = 0; sp++; NEXT;
            CASE(OP_POP) sp--; NEXT;
            CASE(OP_DUP) sp[0] = sp[-1]; sp++; NEXT;
            CASE(OP_DUP2) sp[0] = sp[-2]; sp[1] = sp[-1]; sp += 2; NEXT;
            CASE(OP_LOAD) *sp++ = bp[ARG()]; NEXT;
            CASE(OP_STORE) bp[ARG()] = sp[-1]; NEXT;
            CASE(OP_STOREP) bp[ARG()] = *--sp; NEXT;
            CASE(OP_GETSTATIC) *sp++ = vm->statics[ARG()]; NEXT;
            CASE(OP_SETSTATIC) vm->statics[ARG()] = sp[-1]; NEXT;
            CASE(OP_GETFIELD) {
                int idx = ARG();
                if (sp[-1].t != V_OBJ) FAIL("null reference (reading a field of null)");
                sp[-1] = INST(sp[-1])->fields[idx];
                NEXT;
            }
            CASE(OP_SETFIELD) {
                int idx = ARG();
                if (sp[-2].t != V_OBJ) FAIL("null reference (setting a field of null)");
                INST(sp[-2])->fields[idx] = sp[-1];
                sp[-2] = sp[-1]; sp--;
                NEXT;
            }
            CASE(OP_GETELEM) {            /* array, List, Dictionary or string (gives a char) */
                if (sp[-2].t != V_OBJ) FAIL("null reference (indexing null)");
                Obj* ob = sp[-2].as.o; int i = sp[-1].as.i;
                if (ob->type == O_DICT) {
                    int e = dict_find((ObjDict*)ob, sp[-1], hash_value(sp[-1]));
                    if (e < 0) {
                        char k[64], m[200]; ms_value_to_text(vm, sp[-1], k, sizeof k);
                        snprintf(m, sizeof m, "key '%s' is not in the dictionary (use ContainsKey or TryGetValue)", k); FAIL(m);
                    }
                    sp[-2] = ((ObjDict*)ob)->entries[e].val; sp--;
                    NEXT;
                }
                int len = ob->type == O_ARRAY ? ((ObjArray*)ob)->len : ob->type == O_LIST ? ((ObjList*)ob)->len : ((ObjString*)ob)->len;
                if (i < 0 || i >= len) {
                    char m[80]; snprintf(m, sizeof m, "index %d is outside the %s (length %d)", i,
                                         ob->type == O_ARRAY ? "array" : ob->type == O_LIST ? "list" : "string", len); FAIL(m);
                }
                if (ob->type == O_ARRAY) sp[-2] = ((ObjArray*)ob)->items[i];
                else if (ob->type == O_LIST) sp[-2] = ((ObjList*)ob)->items[i];
                else { sp[-2].t = V_CHAR; sp[-2].as.i = (unsigned char)((ObjString*)ob)->chars[i]; }
                sp--;
                NEXT;
            }
            CASE(OP_SETELEM) {            /* array, List or Dictionary */
                if (sp[-3].t != V_OBJ) FAIL("null reference (indexing null)");
                Obj* ob = sp[-3].as.o; int i = sp[-2].as.i;
                if (ob->type == O_DICT) {
                    if (!dict_put(vm, (ObjDict*)ob, sp[-2], sp[-1], 0)) FAIL("out of memory");
                    sp[-3] = sp[-1]; sp -= 2;
                    NEXT;
                }
                int len = ob->type == O_ARRAY ? ((ObjArray*)ob)->len : ((ObjList*)ob)->len;
                if (i < 0 || i >= len) {
                    char m[80]; snprintf(m, sizeof m, "index %d is outside the %s (length %d)", i, ob->type == O_ARRAY ? "array" : "list", len); FAIL(m);
                }
                if (ob->type == O_ARRAY) ((ObjArray*)ob)->items[i] = sp[-1]; else ((ObjList*)ob)->items[i] = sp[-1];
                sp[-3] = sp[-1]; sp -= 2;
                NEXT;
            }
            CASE(OP_IADD) sp[-2].as.i = (int32_t)((uint32_t)sp[-2].as.i + (uint32_t)sp[-1].as.i); sp--; NEXT;
            CASE(OP_ISUB) sp[-2].as.i = (int32_t)((uint32_t)sp[-2].as.i - (uint32_t)sp[-1].as.i); sp--; NEXT;
            CASE(OP_IMUL) sp[-2].as.i = (int32_t)((uint32_t)sp[-2].as.i * (uint32_t)sp[-1].as.i); sp--; NEXT;
            CASE(OP_IDIV)
                if (sp[-1].as.i == 0) FAIL("division by zero");
                if (sp[-1].as.i == -1) sp[-2].as.i = (int32_t)(0u - (uint32_t)sp[-2].as.i);
                else sp[-2].as.i /= sp[-1].as.i;
                sp--; NEXT;
            CASE(OP_IMOD)
                if (sp[-1].as.i == 0) FAIL("division by zero");
                if (sp[-1].as.i == -1) sp[-2].as.i = 0; else sp[-2].as.i %= sp[-1].as.i;
                sp--; NEXT;
            CASE(OP_INEG) sp[-1].as.i = (int32_t)(0u - (uint32_t)sp[-1].as.i); NEXT;
            CASE(OP_FADD) sp[-2].as.f += sp[-1].as.f; sp--; NEXT;
            CASE(OP_FSUB) sp[-2].as.f -= sp[-1].as.f; sp--; NEXT;
            CASE(OP_FMUL) sp[-2].as.f *= sp[-1].as.f; sp--; NEXT;
            CASE(OP_FDIV) sp[-2].as.f /= sp[-1].as.f; sp--; NEXT;
            CASE(OP_FMOD) sp[-2].as.f = fmodf(sp[-2].as.f, sp[-1].as.f); sp--; NEXT;
            CASE(OP_FNEG) sp[-1].as.f = -sp[-1].as.f; NEXT;
            CASE(OP_I2F) { Value* v = sp - 1 - ARG(); v->t = V_FLOAT; v->as.f = (float)v->as.i; NEXT; }
            CASE(OP_F2I) {
                float f = sp[-1].as.f; int32_t r;
                if (f != f) r = 0; else if (f >= 2147483647.0f) r = 2147483647; else if (f <= -2147483648.0f) r = (-2147483647 - 1); else r = (int32_t)f;
                sp[-1].t = V_INT; sp[-1].as.i = r; NEXT;
            }
            CASE(OP_TOSTR) {               /* kind 4: an object's own class name, without its ToString override (base.ToString()) */
                int kind = ARG(); int depth = ARG();
                Value* v = sp - 1 - depth;
                char buf[200];
                if (kind >= MS_KIND_ENUM) {
                    enum_text(P, kind - MS_KIND_ENUM, v->as.i, buf, sizeof buf);
                    SYNC();
                    Value s = vm_make_string(vm, buf, (int)strlen(buf));
                    if (s.t == V_NULL) FAIL("out of memory");
                    *v = s;
                    NEXT;
                }
                if (kind == 3 && v->t == V_OBJ && v->as.o->type == O_STRING) NEXT;
                if (kind == 3 && v->t == V_OBJ && v->as.o->type == O_INSTANCE && P->classes[INST(*v)->cls].toString >= 0) {
                    /* override string ToString(): run it here, nested, and put its result in place of the object */
                    int f = P->classes[INST(*v)->cls].toString;
                    *sp = *v;
                    vm->sp = (int)(sp - stack) + 1; fr->ip = ip;
                    if (vm->fp >= MS_FRAMES) FAIL("stack overflow (too much recursion?)");
                    int top = vm->fp;
                    Frame* nf = &vm->frames[vm->fp++];
                    nf->f = &P->funcs[f]; nf->ip = 0; nf->base = vm->sp - 1; nf->clo = NULL;
                    for (int i = 1; i < nf->f->nlocals; i++) { stack[nf->base + i].t = V_NULL; stack[nf->base + i].as.o = NULL; }
                    vm->sp = nf->base + nf->f->nlocals;
                    if (vm->sp + 512 >= MS_STACK) FAIL("stack overflow (too much recursion?)");
                    int r = vm_run(vm, top);
                    if (r != 1) { vm->fp = startFp; return 0; }
                    *v = stack[vm->sp];
                    sp = stack + vm->sp;
                    NEXT;
                }
                ms_value_to_text(vm, *v, buf, sizeof buf);
                SYNC();
                Value s = vm_make_string(vm, buf, (int)strlen(buf));
                if (s.t == V_NULL) FAIL("out of memory");
                *v = s;
                NEXT;
            }
            CASE(OP_CONCAT) {
                ObjString* a = sp[-2].t == V_OBJ ? STR(sp[-2]) : NULL;
                ObjString* b = sp[-1].t == V_OBJ ? STR(sp[-1]) : NULL;
                int la = a ? a->len : 0, lb = b ? b->len : 0;
                SYNC();
                ObjString* r = new_string(vm, la + lb);
                if (!r) FAIL("out of memory");
                if (la) memcpy(r->chars, a->chars, la);
                if (lb) memcpy(r->chars + la, b->chars, lb);
                sp[-2].t = V_OBJ; sp[-2].as.o = (Obj*)r; sp--;
                NEXT;
            }
#define ICMP(OPC, X) CASE(OPC) sp[-2].as.i = (sp[-2].as.i X sp[-1].as.i); sp[-2].t = V_BOOL; sp--; NEXT;
#define FCMP(OPC, X) CASE(OPC) sp[-2].as.i = (sp[-2].as.f X sp[-1].as.f); sp[-2].t = V_BOOL; sp--; NEXT;
            ICMP(OP_ILT, <) ICMP(OP_ILE, <=) ICMP(OP_IGT, >) ICMP(OP_IGE, >=) ICMP(OP_IEQ, ==) ICMP(OP_INE, !=)
            FCMP(OP_FLT, <) FCMP(OP_FLE, <=) FCMP(OP_FGT, >) FCMP(OP_FGE, >=) FCMP(OP_FEQ, ==) FCMP(OP_FNE, !=)
            CASE(OP_SEQ) CASE(OP_SNE) {
                int eq = str_equal(sp[-2], sp[-1]);
                sp[-2].t = V_BOOL; sp[-2].as.i = (o == OP_SEQ) ? eq : !eq; sp--;
                NEXT;
            }
            CASE(OP_REQ) CASE(OP_RNE) {
                int eq = (sp[-2].t == sp[-1].t) && (sp[-2].t == V_NULL || sp[-2].as.o == sp[-1].as.o);
                sp[-2].t = V_BOOL; sp[-2].as.i = (o == OP_REQ) ? eq : !eq; sp--;
                NEXT;
            }
            CASE(OP_NOT) sp[-1].as.i = !sp[-1].as.i; NEXT;
#define LOOP_CHECK() if (off < 0 && !(++vm->loops & 0xFFFF)) {                                     \
                         if (hostInterrupt && hostInterrupt()) FAIL("stopped by the user");         \
                         if (vm->deadline && time(NULL) > vm->deadline)                              \
                             FAIL("stopped: still running after 5 seconds (endless loop?)");        \
                     }
            CASE(OP_JMP) { int off = ARG(); ip += off; LOOP_CHECK(); NEXT; }
            CASE(OP_JZ) { int off = ARG(); if (!(--sp)->as.i) { ip += off; LOOP_CHECK(); } NEXT; }
            CASE(OP_JFALSE_KEEP) { int off = ARG(); if (!sp[-1].as.i) ip += off; else sp--; NEXT; }
            CASE(OP_JTRUE_KEEP) { int off = ARG(); if (sp[-1].as.i) ip += off; else sp--; NEXT; }
            CASE(OP_NULLCHK) { int d = ARG(); if (sp[-1 - d].t != V_OBJ) FAIL("null reference (calling a method on null)"); NEXT; }
            CASE(OP_CALLV) {               /* virtual call: the object (first argument) picks the function */
                int sel = ARG(); int argc = ARG();
                Value self = sp[-argc];
                if (self.t != V_OBJ) FAIL("null reference (calling a method on null)");
                int fidx = P->classes[INST(self)->cls].vtable[sel];
                if (fidx < 0) FAIL("internal error: missing virtual method");
                if (P->funcs[fidx].isIter) { SYNC(); if (!coro_new(vm, fidx, argc, &sp)) FAIL("out of memory"); NEXT; }
                Func* nf = &P->funcs[fidx];
                SYNC();
                if (vm->fp >= MS_FRAMES) FAIL("stack overflow (too much recursion?)");
                int base = (int)(sp - stack) - argc;
                if (base + nf->nlocals + 512 >= MS_STACK) FAIL("stack overflow (too much recursion?)");
                for (int i = argc; i < nf->nlocals; i++) { stack[base + i].t = V_NULL; stack[base + i].as.o = NULL; }
                fr = &vm->frames[vm->fp++];
                fr->f = nf; fr->ip = 0; fr->base = base; fr->clo = NULL;
                sp = stack + base + nf->nlocals;
                bp = stack + base;
                code = nf->code; ip = 0;
                NEXT;
            }
            CASE(OP_BOX) {
                SYNC();
                ObjCell* cell = (ObjCell*)alloc_obj(vm, sizeof(ObjCell), O_CELL);
                if (!cell) FAIL("out of memory");
                cell->v = sp[-1];
                sp[-1].t = V_OBJ; sp[-1].as.o = (Obj*)cell;
                NEXT;
            }
            CASE(OP_GETCELL) sp[-1] = ((ObjCell*)sp[-1].as.o)->v; NEXT;
            CASE(OP_SETCELL) ((ObjCell*)sp[-2].as.o)->v = sp[-1]; sp[-2] = sp[-1]; sp--; NEXT;
            CASE(OP_UPVAL) *sp++ = ((ObjClosure*)fr->clo)->ups[ARG()]; NEXT;
            CASE(OP_CLOSURE) {
                int kind = ARG(); int idx = ARG(); int nup = ARG();
                Value self = sp[-nup - 1];
                if (kind == 2) {            /* virtual method: the object picks the function now */
                    if (self.t != V_OBJ) FAIL("null reference (making a delegate of a method of null)");
                    idx = P->classes[INST(self)->cls].vtable[idx];
                }
                SYNC();
                ObjClosure* cl = (ObjClosure*)alloc_obj(vm, sizeof(ObjClosure) + sizeof(Value) * nup, O_CLOSURE);
                if (!cl) FAIL("out of memory");
                cl->func = idx; cl->kind = kind == 1 ? CL_STATIC : CL_SELF; cl->nup = nup;
                cl->self = sp[-nup - 1];
                for (int i = 0; i < nup; i++) cl->ups[i] = sp[i - nup];
                sp -= nup;
                sp[-1].t = V_OBJ; sp[-1].as.o = (Obj*)cl;
                NEXT;
            }
            CASE(OP_CALLF) {
                int argc = ARG(); int hasResult = ARG();
                Value cv = sp[-argc - 1];
                if (cv.t != V_OBJ) FAIL("null reference (calling a delegate that is null)");
                ObjClosure* cl = (ObjClosure*)cv.as.o;
                if (cl->kind == CL_MULTI) {     /* a += b: each one in turn, the last one's result */
                    Value args[MS_MAXNATIVEARGS + 8], r; r.t = V_NULL; r.as.o = NULL;
                    for (int i = 0; i < argc; i++) args[i] = sp[i - argc];
                    SYNC();
                    if (!call_delegate(vm, cl, args, argc, &r)) FAILED();
                    sp = stack + vm->sp - argc - 1;
                    if (hasResult) *sp++ = r;
                    break;
                }
                int n = argc;
                if (cl->kind == CL_STATIC) { memmove(sp - argc - 1, sp - argc, sizeof(Value) * (size_t)argc); sp--; }
                else { sp[-argc - 1] = cl->self; n = argc + 1; }
                int fidx = cl->func;
                if (P->funcs[fidx].isIter) { SYNC(); if (!coro_new(vm, fidx, n, &sp)) FAIL("out of memory"); break; }
                Func* nf = &P->funcs[fidx];
                SYNC();
                if (vm->fp >= MS_FRAMES) FAIL("stack overflow (too much recursion?)");
                int base = (int)(sp - stack) - n;
                if (base + nf->nlocals + 512 >= MS_STACK) FAIL("stack overflow (too much recursion?)");
                for (int i = n; i < nf->nlocals; i++) { stack[base + i].t = V_NULL; stack[base + i].as.o = NULL; }
                fr = &vm->frames[vm->fp++];
                fr->f = nf; fr->ip = 0; fr->base = base; fr->clo = (Obj*)cl;
                sp = stack + base + nf->nlocals;
                bp = stack + base;
                code = nf->code; ip = 0;
                break;
            }
            CASE(OP_DELEG) {
                int remove = ARG();
                Value r;
                SYNC();
                if (!deleg_op(vm, remove, sp[-2], sp[-1], &r)) FAIL("out of memory");
                sp[-2] = r; sp--;
                NEXT;
            }
            CASE(OP_JNULL) { int off = ARG(); if (sp[-1].t == V_NULL) ip += off; NEXT; }
            CASE(OP_JNULL_POP) { int off = ARG(); if (sp[-1].t == V_NULL) { sp--; ip += off; } NEXT; }
            CASE(OP_JNNULL) { int off = ARG(); if (sp[-1].t != V_NULL) ip += off; else sp--; NEXT; }
            CASE(OP_LISTF) {                /* list, delegate -> result */
                int sub = ARG(), kind = sub >> 8; sub &= 255;
                if (sp[-2].t != V_OBJ) FAIL("null reference (calling a method on a null list)");
                if (sp[-1].t != V_OBJ) FAIL("null reference (the delegate given to the list is null)");
                ObjList* l = (ObjList*)sp[-2].as.o;
                ObjClosure* fn = (ObjClosure*)sp[-1].as.o;
                Value res, arg, r; res.t = V_NULL; res.as.o = NULL;
                SYNC();
                switch (sub) {
                    case LF_FIND: case LF_FINDLAST: case LF_FINDINDEX: case LF_FINDLASTINDEX: {
                        int back = sub == LF_FINDLAST || sub == LF_FINDLASTINDEX, found = -1;
                        for (int k = 0; k < l->len; k++) {
                            int i = back ? l->len - 1 - k : k;
                            arg = l->items[i];
                            if (!call_delegate(vm, fn, &arg, 1, &r)) FAILED();
                            if (r.as.i) { found = i; break; }
                        }
                        if (sub == LF_FINDINDEX || sub == LF_FINDLASTINDEX) { res.t = V_INT; res.as.i = found; }
                        else if (found >= 0) res = l->items[found];
                        else if (!make_default(vm, kind, &res)) FAIL("out of memory");     /* default(T) */
                        break;
                    }
                    case LF_EXISTS: case LF_TRUEFORALL: {
                        int all = sub == LF_TRUEFORALL, v = all;
                        for (int i = 0; i < l->len; i++) {
                            arg = l->items[i];
                            if (!call_delegate(vm, fn, &arg, 1, &r)) FAILED();
                            if (all && !r.as.i) { v = 0; break; }
                            if (!all && r.as.i) { v = 1; break; }
                        }
                        res.t = V_BOOL; res.as.i = v;
                        break;
                    }
                    case LF_FINDALL: {
                        ObjList* nl = (ObjList*)alloc_obj(vm, sizeof(ObjList), O_LIST);
                        if (!nl) FAIL("out of memory");
                        nl->len = 0; nl->cap = 0; nl->items = NULL;
                        stack[vm->sp].t = V_OBJ; stack[vm->sp].as.o = (Obj*)nl; vm->sp++;     /* kept alive while the predicate runs */
                        for (int i = 0; i < l->len; i++) {
                            arg = l->items[i];
                            if (!call_delegate(vm, fn, &arg, 1, &r)) FAILED();
                            if (r.as.i) { if (!list_reserve(vm, nl)) FAIL("out of memory"); nl->items[nl->len++] = arg; }
                        }
                        vm->sp--;
                        res.t = V_OBJ; res.as.o = (Obj*)nl;
                        break;
                    }
                    case LF_REMOVEALL: {
                        int j = 0, n = l->len;
                        for (int i = 0; i < n && i < l->len; i++) {
                            arg = l->items[i];
                            if (!call_delegate(vm, fn, &arg, 1, &r)) FAILED();
                            if (!r.as.i) l->items[j++] = arg;
                        }
                        res.t = V_INT; res.as.i = n - j;
                        l->len = j;
                        break;
                    }
                    case LF_FOREACH:
                        for (int i = 0; i < l->len; i++) {
                            arg = l->items[i];
                            if (!call_delegate(vm, fn, &arg, 1, &r)) FAILED();
                        }
                        break;
                    case LF_SORT: {
                        Value t = vm_make_array(vm, l->len);   /* the buffer is a VM array, so the collector sees it */
                        if (t.t == V_NULL) FAIL("out of memory");
                        stack[vm->sp++] = t;
                        int n = l->len;
                        if (!merge_sort(vm, l->items, ((ObjArray*)t.as.o)->items, n, fn)) FAILED();
                        vm->sp--;
                        break;
                    }
                    default: FAIL("internal error: bad list op");
                }
                sp = stack + vm->sp - 2;
                if (sub != LF_FOREACH && sub != LF_SORT) *sp++ = res;
                NEXT;
            }
            CASE(OP_ISINST) {
                int ci = ARG(); int mode = ARG();
                Value v = sp[-1];
                int ok = v.t == V_OBJ && v.as.o->type == O_INSTANCE && P->classes[INST(v)->cls].isa[ci];
                if (mode == 0) { sp[-1].t = V_BOOL; sp[-1].as.i = ok; NEXT; }
                if (ok || v.t == V_NULL) NEXT;
                if (mode == 1) { sp[-1].t = V_NULL; sp[-1].as.o = NULL; NEXT; }
                char m[160]; snprintf(m, sizeof m, "invalid cast: this %s is not a %s", P->classes[INST(v)->cls].name, P->classes[ci].name);
                FAIL(m);
            }
            CASE(OP_CALL) {
                int fidx = ARG(); int argc = ARG();
                Func* nf = &P->funcs[fidx];
                SYNC();
                if (vm->fp >= MS_FRAMES) FAIL("stack overflow (too much recursion?)");
                int base = (int)(sp - stack) - argc;
                if (base + nf->nlocals + 512 >= MS_STACK) FAIL("stack overflow (too much recursion?)");
                for (int i = argc; i < nf->nlocals; i++) { stack[base + i].t = V_NULL; stack[base + i].as.o = NULL; }
                fr = &vm->frames[vm->fp++];
                fr->f = nf; fr->ip = 0; fr->base = base; fr->clo = NULL;
                sp = stack + base + nf->nlocals;
                bp = stack + base;
                code = nf->code; ip = 0;
                NEXT;
            }
            CASE(OP_CALLN) {
                int ni = ARG(); int argc = ARG();
                SYNC();
                Value* args = sp - argc;
                Value r = ms_natives[ni].fn(vm, args);
                sp -= argc;
                if (vm->err[0]) { char m[256]; snprintf(m, sizeof m, "%s", vm->err); FAIL(m); }
                if (vm->quit) { vm->fp = startFp; vm->sp = (int)(sp - stack); return 0; }
                if (ms_natives[ni].ret != T_VOID) *sp++ = r;
                NEXT;
            }
            CASE(OP_RET) CASE(OP_RETV) {
                Value r; r.t = V_NULL; r.as.o = NULL;
                if (o == OP_RETV) r = sp[-1];
                int base = fr->base;
                vm->fp--;
                sp = stack + base;
                if (o == OP_RETV) *sp++ = r;
                if (vm->fp == startFp) { vm->sp = (int)(sp - stack); if (o == OP_RETV) vm->sp--; return 1; }
                fr = &vm->frames[vm->fp - 1];
                code = fr->f->code; ip = fr->ip; bp = stack + fr->base;
                NEXT;
            }
            CASE(OP_NEW) {
                int ci = ARG();
                SYNC();
                Obj* in = new_instance(vm, ci);
                if (!in) FAIL("out of memory");
                sp->t = V_OBJ; sp->as.o = in; sp++;
                NEXT;
            }
            /* superinstructions (see the compiler's op / op1 / jump / emit_pop) */
#define JCMP(OPC, X) CASE(OPC) { int off = ARG(); sp -= 2; if (!(sp[0].as.i X sp[1].as.i)) { ip += off; LOOP_CHECK(); } NEXT; }
            JCMP(OP_JNLT, <) JCMP(OP_JNLE, <=) JCMP(OP_JNGT, >) JCMP(OP_JNGE, >=) JCMP(OP_JNEQ, ==) JCMP(OP_JNNE, !=)
            CASE(OP_IADDK) { int32_t k = ARG(); sp[-1].as.i = (int32_t)((uint32_t)sp[-1].as.i + (uint32_t)k); NEXT; }
            CASE(OP_INCL) { int s = ARG(); int32_t k = ARG(); bp[s].as.i = (int32_t)((uint32_t)bp[s].as.i + (uint32_t)k); NEXT; }
            CASE(OP_GETFIELDL) {
                Value v = bp[ARG()]; int idx = ARG();
                if (v.t != V_OBJ) FAIL("null reference (reading a field of null)");
                *sp++ = INST(v)->fields[idx];
                NEXT;
            }
            CASE(OP_COPY) {
                if (sp[-1].t != V_OBJ) NEXT;
                SYNC();
                Obj* cp = copy_instance(vm, INST(sp[-1]));
                if (!cp) FAIL("out of memory");
                sp[-1].as.o = cp;
                NEXT;
            }
            CASE(OP_NEWARR) {
                int kind = ARG();
                int n = sp[-1].as.i;
                if (n < 0) FAIL("array size cannot be negative");
                if (n > 4 * 1024 * 1024) FAIL("array too big");
                SYNC();
                ObjArray* a = (ObjArray*)alloc_obj(vm, sizeof(ObjArray) + sizeof(Value) * n, O_ARRAY);
                if (!a) FAIL("out of memory");
                a->len = n;
                Value d; d.as.i = 0;
                d.t = kind == 1 ? V_INT : kind == 2 ? V_FLOAT : kind == 3 ? V_BOOL : kind == 4 ? V_CHAR : V_NULL;
                if (kind == 2) d.as.f = 0.0f;
                for (int i = 0; i < n; i++) a->items[i] = d;
                sp[-1].t = V_OBJ; sp[-1].as.o = (Obj*)a;
                if (kind >= MS_KIND_STRUCT)                     /* new Vector2[n]: n zeroed structs (the array is on the stack) */
                    for (int i = 0; i < n; i++) if (!make_default(vm, kind, &a->items[i])) FAIL("out of memory");
                NEXT;
            }
            CASE(OP_ARRLEN) {             /* array.Length, list.Count or string.Length */
                if (sp[-1].t != V_OBJ) FAIL("null reference (Length/Count of null)");
                Obj* ob = sp[-1].as.o;
                sp[-1].t = V_INT;
                sp[-1].as.i = ob->type == O_ARRAY ? ((ObjArray*)ob)->len : ob->type == O_LIST ? ((ObjList*)ob)->len :
                              ob->type == O_DICT ? ((ObjDict*)ob)->count : ((ObjString*)ob)->len;
                break;
            }
            CASE(OP_STRLEN)
                if (sp[-1].t != V_OBJ) FAIL("null reference (Length of a null string)");
                sp[-1].t = V_INT; sp[-1].as.i = STR(sp[-1])->len; NEXT;
            CASE(OP_NORET) FAIL("method ended without returning a value");
            CASE(OP_IAND) sp[-2].as.i &= sp[-1].as.i; sp--; NEXT;
            CASE(OP_IOR) sp[-2].as.i |= sp[-1].as.i; sp--; NEXT;
            CASE(OP_IXOR) sp[-2].as.i ^= sp[-1].as.i; sp--; NEXT;
            CASE(OP_ISHL) sp[-2].as.i = (int32_t)((uint32_t)sp[-2].as.i << (sp[-1].as.i & 31)); sp--; NEXT;
            CASE(OP_ISHR) sp[-2].as.i >>= (sp[-1].as.i & 31); sp--; NEXT;
            CASE(OP_INOT) sp[-1].as.i = ~sp[-1].as.i; NEXT;
            CASE(OP_I2C) sp[-1].t = V_CHAR; sp[-1].as.i &= 0xFFFF; NEXT;
            CASE(OP_C2I) sp[-1 - ARG()].t = V_INT; NEXT;
            CASE(OP_LIST) {
                int sub = ARG();
                if (sub == L_NEW) {
                    SYNC();
                    ObjList* l = (ObjList*)alloc_obj(vm, sizeof(ObjList), O_LIST);
                    if (!l) FAIL("out of memory");
                    l->len = 0; l->cap = 0; l->items = NULL;
                    sp->t = V_OBJ; sp->as.o = (Obj*)l; sp++;
                    NEXT;
                }
                /* stack: list, args...  (the list is checked for null here) */
                int nargs = sub == L_CLEAR || sub == L_SORT || sub == L_REVERSE ? 0 : sub == L_INSERT ? 2 : 1;
                Value* lv = sp - 1 - nargs;
                if (lv->t != V_OBJ) FAIL("null reference (calling a method on a null list)");
                ObjList* l = (ObjList*)lv->as.o;
                switch (sub) {
                    case L_ADD:
                        if (!list_reserve(vm, l)) FAIL("out of memory");
                        l->items[l->len++] = sp[-1];
                        sp -= 2;
                        break;
                    case L_INSERT: {
                        int i = sp[-2].as.i;
                        if (i < 0 || i > l->len) { char m[80]; snprintf(m, sizeof m, "Insert: index %d is outside the list (length %d)", i, (int)l->len); FAIL(m); }
                        if (!list_reserve(vm, l)) FAIL("out of memory");
                        memmove(l->items + i + 1, l->items + i, sizeof(Value) * (size_t)(l->len - i));
                        l->items[i] = sp[-1]; l->len++;
                        sp -= 3;
                        break;
                    }
                    case L_REMOVEAT: {
                        int i = sp[-1].as.i;
                        if (i < 0 || i >= l->len) { char m[80]; snprintf(m, sizeof m, "RemoveAt: index %d is outside the list (length %d)", i, (int)l->len); FAIL(m); }
                        memmove(l->items + i, l->items + i + 1, sizeof(Value) * (size_t)(l->len - i - 1));
                        l->len--;
                        sp -= 2;
                        break;
                    }
                    case L_REMOVE: {
                        int i = list_index_of(l, sp[-1]);
                        if (i >= 0) { memmove(l->items + i, l->items + i + 1, sizeof(Value) * (size_t)(l->len - i - 1)); l->len--; }
                        sp[-2].t = V_BOOL; sp[-2].as.i = i >= 0; sp--;
                        break;
                    }
                    case L_CLEAR: l->len = 0; sp--; break;
                    case L_REVERSE:
                        for (int i = 0, j = l->len - 1; i < j; i++, j--) { Value t = l->items[i]; l->items[i] = l->items[j]; l->items[j] = t; }
                        sp--;
                        break;
                    case L_SORT: {                /* numbers, chars, strings: nothing is allocated while sorting */
                        Value* tmp = (Value*)malloc(sizeof(Value) * (size_t)(l->len ? l->len : 1));
                        if (!tmp) FAIL("out of memory");
                        merge_sort(vm, l->items, tmp, l->len, NULL);
                        free(tmp);
                        sp--;
                        break;
                    }
                    case L_ADDRANGE: {            /* list, array-or-list: appends a copy of every item */
                        if (sp[-1].t != V_OBJ) FAIL("AddRange(null)");
                        Obj* src = sp[-1].as.o;
                        int n = src->type == O_ARRAY ? ((ObjArray*)src)->len : ((ObjList*)src)->len;
                        Value* items = src->type == O_ARRAY ? ((ObjArray*)src)->items : ((ObjList*)src)->items;
                        for (int i = 0; i < n; i++) {
                            if (!list_reserve(vm, l)) FAIL("out of memory");
                            if (src == (Obj*)l) items = l->items;     /* l.AddRange(l): the buffer may have moved */
                            l->items[l->len++] = items[i];
                        }
                        sp -= 2;
                        break;
                    }
                    case L_INDEXOF: sp[-2].t = V_INT; sp[-2].as.i = list_index_of(l, sp[-1]); sp--; break;
                    default: FAIL("internal error: bad list op");
                }
                NEXT;
            }
            CASE(OP_DICT) {
                int sub = ARG(), kind = sub >> 8;
                sub &= 0xFF;
                if (sub == D_NEW) {
                    SYNC();
                    ObjDict* d = (ObjDict*)alloc_obj(vm, sizeof(ObjDict), O_DICT);
                    if (!d) FAIL("out of memory");
                    d->count = d->used = d->cap = d->icap = d->ntomb = d->freeCount = 0; d->freeList = -1;
                    d->entries = NULL; d->index = NULL;
                    sp->t = V_OBJ; sp->as.o = (Obj*)d; sp++;
                    NEXT;
                }
                int nargs = sub == D_ADD ? 2 : (sub == D_CLEAR || sub == D_KEYS || sub == D_VALUES) ? 0 : 1;
                Value* dv = sp - 1 - nargs;
                if (dv->t != V_OBJ) FAIL("null reference (calling a method on a null dictionary)");
                ObjDict* d = (ObjDict*)dv->as.o;
                switch (sub) {
                    case D_ADD: {
                        int r = dict_put(vm, d, sp[-2], sp[-1], 1);
                        if (r == 0) FAIL("out of memory");
                        if (r < 0) {
                            char k[64], m[200]; ms_value_to_text(vm, sp[-2], k, sizeof k);
                            snprintf(m, sizeof m, "Add: the key '%s' is already in the dictionary (use d[key] = value)", k); FAIL(m);
                        }
                        sp -= 3;
                        break;
                    }
                    case D_REMOVE: { int r = dict_remove(d, sp[-1]); sp[-2].t = V_BOOL; sp[-2].as.i = r; sp--; break; }
                    case D_CONTAINS: { int r = dict_find(d, sp[-1], hash_value(sp[-1])) >= 0; sp[-2].t = V_BOOL; sp[-2].as.i = r; sp--; break; }
                    case D_CONTAINSVAL: {
                        int r = 0;
                        for (int i = 0; i < d->used && !r; i++) r = d->entries[i].live && value_equal(d->entries[i].val, sp[-1]);
                        sp[-2].t = V_BOOL; sp[-2].as.i = r; sp--;
                        break;
                    }
                    case D_CLEAR:
                        for (int i = 0; i < d->icap; i++) d->index[i] = -1;
                        d->count = d->used = d->ntomb = d->freeCount = 0; d->freeList = -1;
                        sp--;
                        break;
                    case D_TRYGET: {            /* leaves: found, value (the value on top) */
                        int e = dict_find(d, sp[-1], hash_value(sp[-1]));
                        sp[-2].t = V_BOOL; sp[-2].as.i = e >= 0;
                        if (e >= 0) sp[-1] = d->entries[e].val;
                        else { SYNC(); if (!make_default(vm, kind, &sp[-1])) FAIL("out of memory"); }
                        break;
                    }
                    case D_KEYS: case D_VALUES: {
                        SYNC();
                        ObjList* l = new_list(vm, d->count);
                        if (!l) FAIL("out of memory");
                        for (int i = 0; i < d->used; i++)
                            if (d->entries[i].live) l->items[l->len++] = sub == D_KEYS ? d->entries[i].key : d->entries[i].val;
                        sp[-1].t = V_OBJ; sp[-1].as.o = (Obj*)l;
                        break;
                    }
                    case D_NEXT: {              /* next live entry at or after i, or -1 */
                        int i = sp[-1].as.i;
                        while (i < d->used && !d->entries[i].live) i++;
                        sp[-2].t = V_INT; sp[-2].as.i = i < d->used ? i : -1; sp--;
                        break;
                    }
                    case D_KEYAT: case D_VALAT: {
                        int i = sp[-1].as.i;
                        if (i < 0 || i >= d->used) FAIL("internal error: bad dictionary position");
                        sp[-2] = sub == D_KEYAT ? d->entries[i].key : d->entries[i].val; sp--;
                        break;
                    }
                    default: FAIL("internal error: bad dictionary op");
                }
                NEXT;
            }
            CASE(OP_CORO_NEW) {
                int fidx = ARG(); int argc = ARG();
                SYNC();
                if (!coro_new(vm, fidx, argc, &sp)) FAIL("out of memory");
                NEXT;
            }
            CASE(OP_YIELD) {
                int kind = ARG();
                ObjCoro* co = vm->curCoro;
                if (!co || vm->fp - 1 != startFp) FAIL("internal error: yield outside a coroutine");
                co->waitKind = (uint8_t)kind; co->waitFor = NULL;
                if (kind == Y_SECONDS) { float s = (--sp)->as.f; co->waitUntil = ms_millis() + (int)(s * 1000.0f); }
                else if (kind == Y_CORO) {
                    Value w = *--sp;
                    if (w.t == V_OBJ) co->waitFor = w.as.o; else co->waitKind = Y_NEXT;
                }
                co->ip = ip;
                co->lastFrame = vm->frameNo;
                memcpy(co->locals, bp, sizeof(Value) * (size_t)co->nlocals);
                vm->fp = startFp;
                vm->sp = fr->base;
                /* 'yield return Other()' starts Other if nobody did */
                if (co->waitFor && !((ObjCoro*)co->waitFor)->started) {
                    ((ObjCoro*)co->waitFor)->owner = (Obj*)co;
                    if (!coro_start(vm, (ObjCoro*)co->waitFor)) return 0;
                }
                return 2;
            }
            CASE(OP_CORO) {
                int sub = ARG();
                if (sub == C_STOPALL) { for (int i = 0; i < vm->ncoros; i++) vm->coros[i]->done = 1; NEXT; }
                Value v = sp[-1];
                if (v.t != V_OBJ) FAIL(sub == C_START ? "StartCoroutine(null)" : "StopCoroutine(null)");
                ObjCoro* co = (ObjCoro*)v.as.o;
                if (sub == C_STOP) { coro_stop(co); sp--; break; }
                SYNC();                       /* C_START: leaves the coroutine on the stack as the result */
                if (!coro_start(vm, co)) FAILED();
                NEXT;
            }
            default:
#if defined(__GNUC__) && !defined(MS_NO_THREADED)
            L_bad: __attribute__((unused));
#endif
                FAIL("internal error: bad opcode");
        }
    }
}
