/* MiniSharp - int / float / char helpers (Parse, MaxValue, IsDigit…), from natives.c of 931ad08. */
#include "libvalue.h"
#include "lib.h"
#include <stdlib.h>
#include <ctype.h>

static Value n_intparse(VM* vm, Value* a) {
    const char* s = vstr(a[0]); char* end;
    long long v = strtoll(s, &end, 10);     /* not strtol: long is 32 bits on 3DS/Windows, 64 on Switch/Linux */
    while (isspace((unsigned char)*end)) end++;
    if (!*s || *end) { ms_vm_fail(vm, "int.Parse: '%.40s' is not a whole number", s); return vnone(); }
    if (v > 2147483647LL || v < -2147483648LL) { ms_vm_fail(vm, "int.Parse: '%.40s' is outside int", s); return vnone(); }
    return vint((int32_t)v);
}
static Value n_floatparse(VM* vm, Value* a) {
    const char* s = vstr(a[0]); char* end;
    float v = strtof(s, &end);
    while (isspace((unsigned char)*end)) end++;
    if (!*s || *end) { ms_vm_fail(vm, "float.Parse: '%.40s' is not a number", s); return vnone(); }
    return vfloat(v);
}
static Value n_isdigit(VM* vm, Value* a) { (void)vm; return vbool(a[0].as.i < 128 && isdigit(a[0].as.i)); }
static Value n_isletter(VM* vm, Value* a) { (void)vm; return vbool(a[0].as.i < 128 && isalpha(a[0].as.i)); }
static Value n_iswhite(VM* vm, Value* a) { (void)vm; return vbool(a[0].as.i < 128 && isspace(a[0].as.i)); }
static Value n_ctoupper(VM* vm, Value* a) { (void)vm; return vchar(a[0].as.i < 128 ? toupper(a[0].as.i) : a[0].as.i); }
static Value n_ctolower(VM* vm, Value* a) { (void)vm; return vchar(a[0].as.i < 128 ? tolower(a[0].as.i) : a[0].as.i); }

static const NativeDef convert[] = {
    {"int", "Parse", 1, {S}, I, n_intparse},
    {"float", "Parse", 1, {S}, F, n_floatparse},
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
