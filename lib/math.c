/* MiniSharp - Math (like .NET) and Mathf (like Unity), from natives.c of 931ad08. */
#include "libvalue.h"
#include "lib.h"
#include <math.h>

static Value n_sin(VM* vm, Value* a) { (void)vm; return vfloat(sinf(a[0].as.f)); }
static Value n_cos(VM* vm, Value* a) { (void)vm; return vfloat(cosf(a[0].as.f)); }
static Value n_tan(VM* vm, Value* a) { (void)vm; return vfloat(tanf(a[0].as.f)); }
static Value n_atan2(VM* vm, Value* a) { (void)vm; return vfloat(atan2f(a[0].as.f, a[1].as.f)); }
static Value n_sqrt(VM* vm, Value* a) { (void)vm; return vfloat(sqrtf(a[0].as.f)); }
static Value n_pow(VM* vm, Value* a) { (void)vm; return vfloat(powf(a[0].as.f, a[1].as.f)); }
static Value n_floor(VM* vm, Value* a) { (void)vm; return vfloat(floorf(a[0].as.f)); }
static Value n_ceil(VM* vm, Value* a) { (void)vm; return vfloat(ceilf(a[0].as.f)); }
static Value n_round(VM* vm, Value* a) { (void)vm; return vfloat(rintf(a[0].as.f)); }   /* 2.5 -> 2, like .NET and Unity */
static Value n_asin(VM* vm, Value* a) { (void)vm; return vfloat(asinf(a[0].as.f)); }
static Value n_acos(VM* vm, Value* a) { (void)vm; return vfloat(acosf(a[0].as.f)); }
static Value n_atan(VM* vm, Value* a) { (void)vm; return vfloat(atanf(a[0].as.f)); }
static Value n_exp(VM* vm, Value* a) { (void)vm; return vfloat(expf(a[0].as.f)); }
static Value n_log(VM* vm, Value* a) { (void)vm; return vfloat(logf(a[0].as.f)); }
static Value n_log10(VM* vm, Value* a) { (void)vm; return vfloat(log10f(a[0].as.f)); }
static Value n_truncate(VM* vm, Value* a) { (void)vm; return vfloat(truncf(a[0].as.f)); }
static Value n_signi(VM* vm, Value* a) { (void)vm; return vint(a[0].as.i > 0 ? 1 : a[0].as.i < 0 ? -1 : 0); }
static Value n_signf(VM* vm, Value* a) { (void)vm; return vint(a[0].as.f > 0 ? 1 : a[0].as.f < 0 ? -1 : 0); }
static Value n_absf(VM* vm, Value* a) { (void)vm; return vfloat(fabsf(a[0].as.f)); }
static Value n_absi(VM* vm, Value* a) { (void)vm; int32_t v = a[0].as.i; return vint(v < 0 ? (int32_t)(0u - (uint32_t)v) : v); }
static Value n_minf(VM* vm, Value* a) { (void)vm; return vfloat(a[0].as.f < a[1].as.f ? a[0].as.f : a[1].as.f); }
static Value n_maxf(VM* vm, Value* a) { (void)vm; return vfloat(a[0].as.f > a[1].as.f ? a[0].as.f : a[1].as.f); }
static Value n_mini(VM* vm, Value* a) { (void)vm; return vint(a[0].as.i < a[1].as.i ? a[0].as.i : a[1].as.i); }
static Value n_maxi(VM* vm, Value* a) { (void)vm; return vint(a[0].as.i > a[1].as.i ? a[0].as.i : a[1].as.i); }
static Value n_clampf(VM* vm, Value* a) {
    (void)vm; float v = a[0].as.f;
    if (v < a[1].as.f) v = a[1].as.f;
    if (v > a[2].as.f) v = a[2].as.f;
    return vfloat(v);
}
static Value n_clampi(VM* vm, Value* a) {
    (void)vm; int32_t v = a[0].as.i;
    if (v < a[1].as.i) v = a[1].as.i;
    if (v > a[2].as.i) v = a[2].as.i;
    return vint(v);
}

/* ---- Mathf (Unity style) ---- */
static float clamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }
/* float -> int like the (int) cast in the VM (OP_F2I) and the 3DS CPU: out of range = int.MaxValue/MinValue, NaN = 0.
 * A plain C cast is undefined there and gives different results on x86 (PC) and ARM (3DS, Switch). */
static int32_t to_int(float f) {
    if (f != f) return 0;
    if (f >= 2147483647.0f) return 2147483647;
    if (f <= -2147483648.0f) return -2147483647 - 1;
    return (int32_t)f;
}
static Value n_floori(VM* vm, Value* a) { (void)vm; return vint(to_int(floorf(a[0].as.f))); }
static Value n_ceili(VM* vm, Value* a) { (void)vm; return vint(to_int(ceilf(a[0].as.f))); }
static Value n_roundi(VM* vm, Value* a) { (void)vm; return vint(to_int(rintf(a[0].as.f))); }
static Value n_mfsign(VM* vm, Value* a) { (void)vm; return vfloat(a[0].as.f >= 0 ? 1.0f : -1.0f); }   /* Unity: Sign(0) = 1 */
static Value n_clamp01(VM* vm, Value* a) { (void)vm; return vfloat(clamp01(a[0].as.f)); }
static Value n_lerp(VM* vm, Value* a) { (void)vm; return vfloat(a[0].as.f + (a[1].as.f - a[0].as.f) * clamp01(a[2].as.f)); }
static Value n_lerpu(VM* vm, Value* a) { (void)vm; return vfloat(a[0].as.f + (a[1].as.f - a[0].as.f) * a[2].as.f); }
static Value n_invlerp(VM* vm, Value* a) {
    (void)vm; float d = a[1].as.f - a[0].as.f;
    return vfloat(d != 0 ? clamp01((a[2].as.f - a[0].as.f) / d) : 0);
}
static Value n_movetowards(VM* vm, Value* a) {
    (void)vm; float cur = a[0].as.f, target = a[1].as.f, step = a[2].as.f;
    if (fabsf(target - cur) <= step) return vfloat(target);
    return vfloat(cur + (target > cur ? step : -step));
}
static Value n_smoothstep(VM* vm, Value* a) {
    (void)vm; float t = clamp01(a[2].as.f);
    t = -2.0f * t * t * t + 3.0f * t * t;
    return vfloat(a[1].as.f * t + a[0].as.f * (1.0f - t));
}
static Value n_repeat(VM* vm, Value* a) {
    (void)vm; float t = a[0].as.f, len = a[1].as.f;
    float r = t - floorf(t / len) * len;
    return vfloat(r < 0 ? 0 : r > len ? len : r);
}
static Value n_pingpong(VM* vm, Value* a) {
    (void)vm; float len = a[1].as.f, t = a[0].as.f - floorf(a[0].as.f / (len * 2)) * len * 2;
    return vfloat(len - fabsf(t - len));
}
static Value n_approx(VM* vm, Value* a) {
    (void)vm; float x = a[0].as.f, y = a[1].as.f, m = fmaxf(fabsf(x), fabsf(y));
    return vbool(fabsf(y - x) < fmaxf(1e-6f * m, 1.401298e-45f * 8));   /* same rule as Unity */
}
static Value n_deltaangle(VM* vm, Value* a) {
    (void)vm; float d = a[1].as.f - a[0].as.f;
    d = d - floorf(d / 360.0f) * 360.0f;
    return vfloat(d > 180.0f ? d - 360.0f : d);
}

static const NativeDef math[] = {
    {"Math", "Sin", 1, {F}, F, n_sin},
    {"Math", "Cos", 1, {F}, F, n_cos},
    {"Math", "Tan", 1, {F}, F, n_tan},
    {"Math", "Atan2", 2, {F, F}, F, n_atan2},
    {"Math", "Sqrt", 1, {F}, F, n_sqrt},
    {"Math", "Pow", 2, {F, F}, F, n_pow},
    {"Math", "Floor", 1, {F}, F, n_floor},
    {"Math", "Ceiling", 1, {F}, F, n_ceil},
    {"Math", "Round", 1, {F}, F, n_round},
    {"Math", "Abs", 1, {I}, I, n_absi},
    {"Math", "Abs", 1, {F}, F, n_absf},
    {"Math", "Min", 2, {I, I}, I, n_mini},
    {"Math", "Min", 2, {F, F}, F, n_minf},
    {"Math", "Max", 2, {I, I}, I, n_maxi},
    {"Math", "Max", 2, {F, F}, F, n_maxf},
    {"Math", "Clamp", 3, {I, I, I}, I, n_clampi},
    {"Math", "Clamp", 3, {F, F, F}, F, n_clampf},
    {"Math", "Asin", 1, {F}, F, n_asin},
    {"Math", "Acos", 1, {F}, F, n_acos},
    {"Math", "Atan", 1, {F}, F, n_atan},
    {"Math", "Exp", 1, {F}, F, n_exp},
    {"Math", "Log", 1, {F}, F, n_log},
    {"Math", "Log10", 1, {F}, F, n_log10},
    {"Math", "Truncate", 1, {F}, F, n_truncate},
    {"Math", "Sign", 1, {I}, I, n_signi},
    {"Math", "Sign", 1, {F}, I, n_signf},
};
static const NativeDef mathf[] = {
    {"Mathf", "Sin", 1, {F}, F, n_sin},
    {"Mathf", "Cos", 1, {F}, F, n_cos},
    {"Mathf", "Tan", 1, {F}, F, n_tan},
    {"Mathf", "Asin", 1, {F}, F, n_asin},
    {"Mathf", "Acos", 1, {F}, F, n_acos},
    {"Mathf", "Atan", 1, {F}, F, n_atan},
    {"Mathf", "Atan2", 2, {F, F}, F, n_atan2},
    {"Mathf", "Sqrt", 1, {F}, F, n_sqrt},
    {"Mathf", "Pow", 2, {F, F}, F, n_pow},
    {"Mathf", "Exp", 1, {F}, F, n_exp},
    {"Mathf", "Log", 1, {F}, F, n_log},
    {"Mathf", "Log10", 1, {F}, F, n_log10},
    {"Mathf", "Floor", 1, {F}, F, n_floor},
    {"Mathf", "Ceil", 1, {F}, F, n_ceil},
    {"Mathf", "Round", 1, {F}, F, n_round},
    {"Mathf", "FloorToInt", 1, {F}, I, n_floori},
    {"Mathf", "CeilToInt", 1, {F}, I, n_ceili},
    {"Mathf", "RoundToInt", 1, {F}, I, n_roundi},
    {"Mathf", "Abs", 1, {I}, I, n_absi},
    {"Mathf", "Abs", 1, {F}, F, n_absf},
    {"Mathf", "Min", 2, {I, I}, I, n_mini},
    {"Mathf", "Min", 2, {F, F}, F, n_minf},
    {"Mathf", "Max", 2, {I, I}, I, n_maxi},
    {"Mathf", "Max", 2, {F, F}, F, n_maxf},
    {"Mathf", "Clamp", 3, {I, I, I}, I, n_clampi},
    {"Mathf", "Clamp", 3, {F, F, F}, F, n_clampf},
    {"Mathf", "Clamp01", 1, {F}, F, n_clamp01},
    {"Mathf", "Sign", 1, {F}, F, n_mfsign},
    {"Mathf", "Lerp", 3, {F, F, F}, F, n_lerp},
    {"Mathf", "LerpUnclamped", 3, {F, F, F}, F, n_lerpu},
    {"Mathf", "InverseLerp", 3, {F, F, F}, F, n_invlerp},
    {"Mathf", "MoveTowards", 3, {F, F, F}, F, n_movetowards},
    {"Mathf", "SmoothStep", 3, {F, F, F}, F, n_smoothstep},
    {"Mathf", "Repeat", 2, {F, F}, F, n_repeat},
    {"Mathf", "PingPong", 2, {F, F}, F, n_pingpong},
    {"Mathf", "Approximately", 2, {F, F}, B, n_approx},
    {"Mathf", "DeltaAngle", 2, {F, F}, F, n_deltaangle},
};
static const ConstDef math_consts[] = {
    {"Math", "PI", 1, 0, 3.14159265f}, {"Math", "E", 1, 0, 2.71828183f},
    {"Mathf", "PI", 1, 0, 3.14159265f}, {"Mathf", "Deg2Rad", 1, 0, 3.14159265f / 180.0f},
    {"Mathf", "Rad2Deg", 1, 0, 180.0f / 3.14159265f}, {"Mathf", "Epsilon", 1, 0, 1.401298e-45f},
};
void lib_register_math(void) { MS_REGISTER(math); MS_REGISTER(mathf); MS_REGISTER_CONSTS(math_consts); }
