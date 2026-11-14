/* MiniSharp - reflection for the layers above the language: the classes, fields and methods of a compiled program.
 * A scene loader makes an object of the class named in a file and sets its fields by name; an inspector lists the
 * classes derived from a base class and their public fields. The compiler keeps the needed tables in ClassRT
 * (rt_class_info, rt_fields, rt_methods in compiler.c); the parts that touch objects of a running program
 * (vm_new_object, vm_call_args, ms_set_field, ms_vm_keep) are in vm.c. */
#include "vm.h"
#include <string.h>

static const ClassRT* cls_at(const Program* p, int cls) {
    return p && cls >= 0 && cls < p->nclasses ? &p->classes[cls] : NULL;
}
static const char* str_at(const Program* p, int i) {
    return i >= 0 && i < p->nstrings ? p->strings[i]->chars : "";
}

int ms_class_count(const Program* p) { return p ? p->nclasses : 0; }

const char* ms_class_name(const Program* p, int cls) {
    const ClassRT* k = cls_at(p, cls);
    return k ? k->name : NULL;
}

const char* ms_class_full_name(const Program* p, int cls) {
    const ClassRT* k = cls_at(p, cls);
    return k ? str_at(p, k->fullName) : NULL;
}

int ms_class_find(const Program* p, const char* name) {
    if (!p || !name) return -1;
    for (int i = 0; i < p->nclasses; i++)
        if (!strcmp(str_at(p, p->classes[i].fullName), name)) return i;
    int found = -1;
    for (int i = 0; i < p->nclasses; i++)
        if (!strcmp(p->classes[i].name, name)) {
            if (found >= 0) return -1;              /* the same name in two namespaces: say which one */
            found = i;
        }
    return found;
}

int ms_class_kind(const Program* p, int cls) {
    const ClassRT* k = cls_at(p, cls);
    return k ? k->kind : -1;
}

int ms_class_base(const Program* p, int cls) {
    const ClassRT* k = cls_at(p, cls);
    return k ? k->base : -1;
}

int ms_class_is(const Program* p, int cls, int other) {
    const ClassRT* k = cls_at(p, cls);
    if (!k || !cls_at(p, other)) return 0;
    if (cls == other) return 1;
    return k->isa ? k->isa[other] != 0 : 0;
}

int ms_class_is_abstract(const Program* p, int cls) {
    const ClassRT* k = cls_at(p, cls);
    return k ? k->isAbstract : 0;
}

int ms_class_from_source(const Program* p, int cls) {
    const ClassRT* k = cls_at(p, cls);
    return k ? k->fromSource : 0;
}

int ms_class_can_create(const Program* p, int cls) {
    const ClassRT* k = cls_at(p, cls);
    if (!k || k->isAbstract || k->isStatic) return 0;
    if (k->kind == MS_CK_STRUCT) return 1;
    return k->kind == MS_CK_CLASS && k->ctor0 >= 0;
}

int ms_field_count(const Program* p, int cls) {
    const ClassRT* k = cls_at(p, cls);
    return k && k->fi ? k->nfields : 0;
}

int ms_field_info(const Program* p, int cls, int i, MsField* out) {
    const ClassRT* k = cls_at(p, cls);
    if (!k || !k->fi || i < 0 || i >= k->nfields || !out) return 0;
    const RtField* f = &k->fi[i];
    out->name = str_at(p, f->name);
    out->kind = f->kind; out->cls = f->cls;
    out->elemKind = f->elemKind; out->elemCls = f->elemCls;
    out->keyKind = f->keyKind; out->keyCls = f->keyCls;
    out->rank = f->rank;
    out->access = f->access; out->readOnly = f->readOnly;
    out->owner = f->owner;
    return 1;
}

int ms_field_find(const Program* p, int cls, const char* name) {
    const ClassRT* k = cls_at(p, cls);
    if (!k || !k->fi || !name) return -1;
    for (int i = 0; i < k->nfields; i++)
        if (!strcmp(str_at(p, k->fi[i].name), name)) return i;
    return -1;
}

int ms_enum_count(const Program* p, int cls) {
    const ClassRT* k = cls_at(p, cls);
    return k && k->kind == MS_CK_ENUM ? k->nenum : 0;
}

const char* ms_enum_name(const Program* p, int cls, int i) {
    if (i < 0 || i >= ms_enum_count(p, cls)) return NULL;
    return str_at(p, p->classes[cls].enumStr[i]);
}

int ms_enum_value(const Program* p, int cls, int i) {
    if (i < 0 || i >= ms_enum_count(p, cls)) return 0;
    return p->classes[cls].enumVals[i];
}

int ms_method_find(const Program* p, int cls, const char* name, int nargs, int* owner) {
    if (!name) return -1;
    /* the class itself first, then its bases: an override in a derived class is found before the base method */
    for (const ClassRT* k = cls_at(p, cls); k; cls = k->base, k = cls_at(p, cls)) {
        for (int j = 0; j < k->nmeth; j++) {
            const RtMethod* m = &k->meth[j];
            if (m->isStatic || m->np != nargs || strcmp(str_at(p, m->name), name)) continue;
            if (m->func < 0) return -1;             /* abstract: nothing to run */
            if (owner) *owner = cls;
            return m->func;
        }
    }
    return -1;
}

int ms_class_of(Value v) {
    if (v.t != V_OBJ || !v.as.o || v.as.o->type != O_INSTANCE) return -1;
    return ((ObjInstance*)v.as.o)->cls;
}

Value ms_get_field(Value obj, int i) {
    Value v; v.t = V_NULL; v.as.o = NULL;
    if (ms_class_of(obj) < 0) return v;
    ObjInstance* in = (ObjInstance*)obj.as.o;
    return i >= 0 && i < in->nfields ? in->fields[i] : v;
}
