/* MiniSharp - int / float / char helpers (Parse, MaxValue, IsDigit…), from natives.c of 931ad08. */
#include "libvalue.h"
#include "lib.h"
#include <stdlib.h>
#include <ctype.h>

/* 0 ok, 1 not a number, 2 outside int */
static int parse_int(const char* s, int32_t* out) {
    char* end;
    long long v = strtoll(s, &end, 10);     /* not strtol: long is 32 bits on 3DS/Windows, 64 on Switch/Linux */
    while (isspace((unsigned char)*end)) end++;
    if (!*s || *end) return 1;
    if (v > 2147483647LL || v < -2147483648LL) return 2;
    *out = (int32_t)v;
    return 0;
}
static int parse_float(const char* s, float* out) {
    char* end;
    float v = strtof(s, &end);
    while (isspace((unsigned char)*end)) end++;
    if (!*s || *end) return 1;
    *out = v;
    return 0;
}
static Value n_intparse(VM* vm, Value* a) {
    const char* s = vstr(a[0]); int32_t v = 0;
    int r = parse_int(s, &v);
    if (r == 1) { ms_vm_fail(vm, "int.Parse: '%.40s' is not a whole number", s); return vnone(); }
    if (r == 2) { ms_vm_fail(vm, "int.Parse: '%.40s' is outside int", s); return vnone(); }
    return vint(v);
}
static Value n_floatparse(VM* vm, Value* a) {
    const char* s = vstr(a[0]); float v = 0;
    if (parse_float(s, &v)) { ms_vm_fail(vm, "float.Parse: '%.40s' is not a number", s); return vnone(); }
    return vfloat(v);
}
/* int.TryParse(s, out n) / float.TryParse: the compiler calls "#int".CanParse (the bool) and then "#int".TryParse
   (the value, 0 when it is not a number) and stores the value in n. '#' hides them from code and hints. */
static Value n_intcan(VM* vm, Value* a) { (void)vm; int32_t v; return vbool(a[0].t == V_OBJ && !parse_int(vstr(a[0]), &v)); }
static Value n_intval(VM* vm, Value* a) { (void)vm; int32_t v = 0; return vint(a[0].t == V_OBJ && !parse_int(vstr(a[0]), &v) ? v : 0); }
static Value n_floatcan(VM* vm, Value* a) { (void)vm; float v; return vbool(a[0].t == V_OBJ && !parse_float(vstr(a[0]), &v)); }
static Value n_floatval(VM* vm, Value* a) { (void)vm; float v = 0; return vfloat(a[0].t == V_OBJ && !parse_float(vstr(a[0]), &v) ? v : 0); }
static Value n_isdigit(VM* vm, Value* a) { (void)vm; return vbool(a[0].as.i < 128 && isdigit(a[0].as.i)); }
/* letters beyond ASCII too: Polish, other Latin, Greek, Cyrillic (lang/text.c) */
static Value n_isletter(VM* vm, Value* a) { (void)vm; return vbool(ms_char_is_letter(a[0].as.i)); }
static Value n_iswhite(VM* vm, Value* a) { (void)vm; return vbool(ms_char_is_space(a[0].as.i)); }
static Value n_ctoupper(VM* vm, Value* a) { (void)vm; return vchar(ms_char_upper(a[0].as.i)); }
static Value n_ctolower(VM* vm, Value* a) { (void)vm; return vchar(ms_char_lower(a[0].as.i)); }

static const NativeDef convert[] = {
    {"int", "Parse", 1, {S}, I, n_intparse},
    {"float", "Parse", 1, {S}, F, n_floatparse},
    {"#int", "CanParse", 1, {S}, B, n_intcan},
    {"#int", "TryParse", 1, {S}, I, n_intval},
    {"#float", "CanParse", 1, {S}, B, n_floatcan},
    {"#float", "TryParse", 1, {S}, F, n_floatval},
    {"char", "IsDigit", 1, {CH}, B, n_isdigit},
    {"char", "IsLetter", 1, {CH}, B, n_isletter},
    {"char", "IsWhiteSpace", 1, {CH}, B, n_iswhite},
    {"char", "ToUpper", 1, {CH}, CH, n_ctoupper},
    {"char", "ToLower", 1, {CH}, CH, n_ctolower},
};
static const ConstDef convert_consts[] = {
    {"int", "MaxValue", 0, 2147483647, 0}, {"int", "MinValue", 0, (-2147483647 - 1), 0},
};
void lib_register_convert(void) { MS_REGISTER(convert); MS_REGISTER_CONSTS(convert_consts); }
