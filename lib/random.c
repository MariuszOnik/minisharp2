/* MiniSharp - Random (xorshift32), from natives.c of 931ad08. Without Random.Seed the start comes from the clock. */
#include "libvalue.h"
#include "lib.h"

static uint32_t rng_state = 0;
static uint32_t rng(void) {
    if (!rng_state) rng_state = (uint32_t)ms_millis() * 2654435761u | 1u;
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng_state = x;
}
static Value n_rnext(VM* vm, Value* a) {
    if (a[0].as.i <= 0) { ms_vm_fail(vm, "Random.Next: max must be greater than 0"); return vnone(); }
    return vint((int32_t)(rng() % (uint32_t)a[0].as.i));
}
static Value n_rrange(VM* vm, Value* a) {
    int32_t lo = a[0].as.i, hi = a[1].as.i;
    if (hi <= lo) { ms_vm_fail(vm, "Random.Range: max must be greater than min"); return vnone(); }
    return vint(lo + (int32_t)(rng() % (uint32_t)(hi - lo)));
}
static Value n_rfloat(VM* vm, Value* a) { (void)vm; (void)a; return vfloat((rng() >> 8) / 16777216.0f); }
static Value n_rseed(VM* vm, Value* a) { (void)vm; rng_state = (uint32_t)a[0].as.i | 1u; return vnone(); }

static const NativeDef random[] = {
    {"Random", "Next", 1, {I}, I, n_rnext},
    {"Random", "Range", 2, {I, I}, I, n_rrange},
    {"Random", "Float", 0, {0}, F, n_rfloat},
    {"Random", "Seed", 1, {I}, VD, n_rseed},
};
void lib_register_random(void) { MS_REGISTER(random); }
