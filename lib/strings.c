/* MiniSharp - string methods (".string": a[0] is the string itself, the compiler checked it for null)
 * and string.IsNullOrEmpty, from natives.c of 931ad08. */
#include "libvalue.h"
#include "lib.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static ObjString* sobj(Value v) { return (ObjString*)v.as.o; }
static Value make_or_fail(VM* vm, const char* s, int len) {
    Value v = vm_make_string(vm, s, len);
    if (v.t == V_NULL) ms_vm_fail(vm, "out of memory");
    return v;
}
static int find_sub(ObjString* h, const char* n, int nl, int from) {
    for (int i = from; i + nl <= h->len; i++) if (!memcmp(h->chars + i, n, nl)) return i;
    return -1;
}
static Value n_substring2(VM* vm, Value* a) {
    ObjString* s = sobj(a[0]); int st = a[1].as.i, n = a[2].as.i;
    if (st < 0 || n < 0 || st + n > s->len) {
        ms_vm_fail(vm, "Substring(%d, %d) is outside the string (length %d)", st, n, (int)s->len); return vnone();
    }
    return make_or_fail(vm, s->chars + st, n);
}
static Value n_substring1(VM* vm, Value* a) {
    ObjString* s = sobj(a[0]); int st = a[1].as.i;
    if (st < 0 || st > s->len) { ms_vm_fail(vm, "Substring(%d) is outside the string (length %d)", st, (int)s->len); return vnone(); }
    return make_or_fail(vm, s->chars + st, s->len - st);
}
static Value n_indexof(VM* vm, Value* a) { (void)vm; return vint(find_sub(sobj(a[0]), vstr(a[1]), (int)strlen(vstr(a[1])), 0)); }
static Value n_indexofc(VM* vm, Value* a) { (void)vm; char ch = (char)a[1].as.i; return vint(find_sub(sobj(a[0]), &ch, 1, 0)); }
static Value n_contains(VM* vm, Value* a) { (void)vm; return vbool(find_sub(sobj(a[0]), vstr(a[1]), (int)strlen(vstr(a[1])), 0) >= 0); }
static Value n_startswith(VM* vm, Value* a) {
    (void)vm; ObjString* s = sobj(a[0]); int n = (int)strlen(vstr(a[1]));
    return vbool(n <= s->len && !memcmp(s->chars, vstr(a[1]), n));
}
static Value n_endswith(VM* vm, Value* a) {
    (void)vm; ObjString* s = sobj(a[0]); int n = (int)strlen(vstr(a[1]));
    return vbool(n <= s->len && !memcmp(s->chars + s->len - n, vstr(a[1]), n));
}
static Value change_case(VM* vm, Value* a, int upper) {
    ObjString* s = sobj(a[0]);
    Value r = make_or_fail(vm, s->chars, s->len);
    if (r.t == V_NULL) return r;
    ObjString* o = (ObjString*)r.as.o;
    for (int i = 0; i < o->len; i++) o->chars[i] = (char)(upper ? toupper((unsigned char)o->chars[i]) : tolower((unsigned char)o->chars[i]));
    return r;
}
static Value n_toupper(VM* vm, Value* a) { return change_case(vm, a, 1); }
static Value n_tolower(VM* vm, Value* a) { return change_case(vm, a, 0); }
static Value n_trim(VM* vm, Value* a) {
    ObjString* s = sobj(a[0]); int b = 0, e = s->len;
    while (b < e && isspace((unsigned char)s->chars[b])) b++;
    while (e > b && isspace((unsigned char)s->chars[e - 1])) e--;
    return make_or_fail(vm, s->chars + b, e - b);
}
static Value n_replace(VM* vm, Value* a) {
    ObjString* s = sobj(a[0]); const char* from = vstr(a[1]); const char* to = vstr(a[2]);
    int fl = (int)strlen(from), tl = (int)strlen(to);
    if (fl == 0) { ms_vm_fail(vm, "Replace: the text to find cannot be empty"); return vnone(); }
    int count = 0;
    for (int i = find_sub(s, from, fl, 0); i >= 0; i = find_sub(s, from, fl, i + fl)) count++;
    int len = s->len + count * (tl - fl);
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) { ms_vm_fail(vm, "out of memory"); return vnone(); }
    int o = 0, i = 0;
    for (int at = find_sub(s, from, fl, 0); at >= 0; at = find_sub(s, from, fl, i)) {
        memcpy(buf + o, s->chars + i, at - i); o += at - i;
        memcpy(buf + o, to, tl); o += tl;
        i = at + fl;
    }
    memcpy(buf + o, s->chars + i, s->len - i);
    Value r = make_or_fail(vm, buf, len);
    free(buf);
    return r;
}
static Value n_split(VM* vm, Value* a) {
    ObjString* s = sobj(a[0]); char sep = (char)a[1].as.i;
    int n = 1;
    for (int i = 0; i < s->len; i++) if (s->chars[i] == sep) n++;
    Value arr = vm_make_array(vm, n);
    if (arr.t == V_NULL) { ms_vm_fail(vm, "out of memory"); return arr; }
    ms_vm_push_root(vm, arr);           /* keep the array alive while the parts are allocated */
    int k = 0, st = 0;
    for (int i = 0; i <= s->len; i++) {
        if (i < s->len && s->chars[i] != sep) continue;
        Value part = make_or_fail(vm, s->chars + st, i - st);
        if (part.t == V_NULL) break;
        ((ObjArray*)arr.as.o)->items[k++] = part;
        st = i + 1;
    }
    ms_vm_pop_root(vm);
    return arr;
}
static Value n_isnullorempty(VM* vm, Value* a) { (void)vm; return vbool(a[0].t != V_OBJ || sobj(a[0])->len == 0); }

static const NativeDef strings[] = {
    {".string", "Substring", 3, {S, I, I}, S, n_substring2},
    {".string", "Substring", 2, {S, I}, S, n_substring1},
    {".string", "IndexOf", 2, {S, S}, I, n_indexof},
    {".string", "IndexOf", 2, {S, CH}, I, n_indexofc},
    {".string", "Contains", 2, {S, S}, B, n_contains},
    {".string", "StartsWith", 2, {S, S}, B, n_startswith},
    {".string", "EndsWith", 2, {S, S}, B, n_endswith},
    {".string", "ToUpper", 1, {S}, S, n_toupper},
    {".string", "ToLower", 1, {S}, S, n_tolower},
    {".string", "Trim", 1, {S}, S, n_trim},
    {".string", "Replace", 3, {S, S, S}, S, n_replace},
    {".string", "Split", 2, {S, CH}, SA, n_split},
    {"string", "IsNullOrEmpty", 1, {S}, B, n_isnullorempty},
};
void lib_register_strings(void) { MS_REGISTER(strings); }
