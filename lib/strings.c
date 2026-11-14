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
/* Substring / IndexOf count characters (code points), not bytes: "żółw".Substring(1, 2) is "ół" */
static Value n_substring2(VM* vm, Value* a) {
    ObjString* s = sobj(a[0]); int st = a[1].as.i, n = a[2].as.i, len = ms_str_length(s);
    if (st < 0 || n < 0 || st > len || n > len - st) {
        ms_vm_fail(vm, "Substring(%d, %d) is outside the string (length %d)", st, n, len); return vnone();
    }
    int b = ms_str_offset(s, st), e = ms_str_offset(s, st + n);
    return make_or_fail(vm, s->chars + b, e - b);
}
static Value n_substring1(VM* vm, Value* a) {
    ObjString* s = sobj(a[0]); int st = a[1].as.i, len = ms_str_length(s);
    if (st < 0 || st > len) { ms_vm_fail(vm, "Substring(%d) is outside the string (length %d)", st, len); return vnone(); }
    int b = ms_str_offset(s, st);
    return make_or_fail(vm, s->chars + b, s->len - b);
}
/* a byte position found by find_sub as a character index */
static int char_index(ObjString* s, int byte) { return byte < 0 ? -1 : ms_utf8_count(s->chars, byte); }
static Value n_indexof(VM* vm, Value* a) {
    (void)vm; ObjString* s = sobj(a[0]);
    return vint(char_index(s, find_sub(s, vstr(a[1]), (int)strlen(vstr(a[1])), 0)));
}
static Value n_indexofc(VM* vm, Value* a) {
    (void)vm; ObjString* s = sobj(a[0]);
    char ch[4]; int n = ms_utf8_encode(a[1].as.i, ch);
    return vint(char_index(s, find_sub(s, ch, n, 0)));
}
static Value n_contains(VM* vm, Value* a) { (void)vm; return vbool(find_sub(sobj(a[0]), vstr(a[1]), (int)strlen(vstr(a[1])), 0) >= 0); }
static Value n_startswith(VM* vm, Value* a) {
    (void)vm; ObjString* s = sobj(a[0]); int n = (int)strlen(vstr(a[1]));
    return vbool(n <= s->len && !memcmp(s->chars, vstr(a[1]), n));
}
static Value n_endswith(VM* vm, Value* a) {
    (void)vm; ObjString* s = sobj(a[0]); int n = (int)strlen(vstr(a[1]));
    return vbool(n <= s->len && !memcmp(s->chars + s->len - n, vstr(a[1]), n));
}
/* ToUpper / ToLower letter by letter, also Polish ones (ą -> Ą); the result is never longer than the string */
static Value change_case(VM* vm, Value* a, int upper) {
    ObjString* s = sobj(a[0]);
    char* buf = (char*)malloc((size_t)s->len + 4);
    if (!buf) { ms_vm_fail(vm, "out of memory"); return vnone(); }
    int o = 0, adv;
    for (int i = 0; i < s->len; i += adv) {
        int c = ms_utf8_decode(s->chars + i, s->len - i, &adv);
        int d = upper ? ms_char_upper(c) : ms_char_lower(c);
        if (d == c) { memcpy(buf + o, s->chars + i, adv); o += adv; }     /* unchanged (also a bad byte) */
        else o += ms_utf8_encode(d, buf + o);
    }
    Value r = make_or_fail(vm, buf, o);
    free(buf);
    return r;
}
static Value n_toupper(VM* vm, Value* a) { return change_case(vm, a, 1); }
static Value n_tolower(VM* vm, Value* a) { return change_case(vm, a, 0); }
static Value n_trim(VM* vm, Value* a) {
    ObjString* s = sobj(a[0]); int b = 0, e = s->len, adv;
    while (b < e && ms_char_is_space(ms_utf8_decode(s->chars + b, e - b, &adv))) b += adv;
    while (e > b) {                     /* back to the start of the last character */
        int k = e - 1;
        while (k > b && ((unsigned char)s->chars[k] & 0xC0) == 0x80) k--;
        if (!ms_char_is_space(ms_utf8_decode(s->chars + k, e - k, &adv))) break;
        e = k;
    }
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
    ObjString* s = sobj(a[0]);
    char sep[4]; int sl = ms_utf8_encode(a[1].as.i, sep);     /* the separator may be a Polish letter: 1..4 bytes */
    int n = 1;
    for (int i = 0; i + sl <= s->len; ) { if (!memcmp(s->chars + i, sep, sl)) { n++; i += sl; } else i++; }
    Value arr = vm_make_array(vm, n);
    if (arr.t == V_NULL) { ms_vm_fail(vm, "out of memory"); return arr; }
    ms_vm_push_root(vm, arr);           /* keep the array alive while the parts are allocated */
    int k = 0, st = 0, i = 0;
    while (k < n) {
        int at = i;
        while (at + sl <= s->len && memcmp(s->chars + at, sep, sl)) at++;
        if (at + sl > s->len) at = s->len;                     /* the last part */
        Value part = make_or_fail(vm, s->chars + st, at - st);
        if (part.t == V_NULL) break;
        ((ObjArray*)arr.as.o)->items[k++] = part;
        st = i = at + sl;
    }
    ms_vm_pop_root(vm);
    return arr;
}
static Value n_isnullorempty(VM* vm, Value* a) { (void)vm; return vbool(a[0].t != V_OBJ || sobj(a[0])->len == 0); }

/* string.Join is compiled to a loop (call_join in lang/comp_call.c); this entry only makes it known to the hints,
 * the signature and the graph's catalogue, and is never called */
static Value n_join(VM* vm, Value* a) { (void)a; ms_vm_fail(vm, "internal error: string.Join"); return vnone(); }

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
    {"string", "Join", 2, {S, T_LIST}, S, n_join},
};
void lib_register_strings(void) { MS_REGISTER(strings); }
