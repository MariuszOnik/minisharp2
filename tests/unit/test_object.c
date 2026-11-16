/* Unit test: the 'object' kind of a native's parameter (T_OBJECT). A host function that takes a script's own
 * object gets the reference itself (to compare, keep, read its fields), while 'any' (T_ANY) still turns an object
 * into its ToString text. Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "ms.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

static char out[512];
static Obj* kept;
static Value t_say(VM* vm, Value* a) {
    char buf[128]; ms_value_to_text(vm, a[0], buf, sizeof buf);
    strncat(out, buf, sizeof out - strlen(out) - 1);
    strncat(out, " ", sizeof out - strlen(out) - 1);
    return (Value){ .t = V_NULL };
}
static Value vbool(int b) { Value v; v.t = V_BOOL; v.as.i = b != 0; return v; }
/* Host.Same(a, b): the same reference (both null too) */
static Value t_same(VM* vm, Value* a) {
    (void)vm;
    if (a[0].t == V_NULL || a[1].t == V_NULL) return vbool(a[0].t == a[1].t);
    return vbool(a[0].t == V_OBJ && a[1].t == V_OBJ && a[0].as.o == a[1].as.o);
}
/* Host.Keep(a) / Host.IsKept(a): a reference held by the host between calls */
static Value t_keep(VM* vm, Value* a) { (void)vm; kept = a[0].t == V_OBJ ? a[0].as.o : NULL; return (Value){ .t = V_NULL }; }
static Value t_isKept(VM* vm, Value* a) { (void)vm; return vbool(a[0].t == V_OBJ && a[0].as.o == kept); }
/* Host.Kind(a): what arrived: "obj", "null", or another tag (a T_ANY would have made it a string object) */
static Value t_isObj(VM* vm, Value* a) { (void)vm; return vbool(a[0].t == V_OBJ); }

static const NativeDef nat[] = {
    { "Out", "Say", 1, { T_ANY }, T_VOID, t_say },
    { "Host", "Same", 2, { T_OBJECT, T_OBJECT }, T_BOOL, t_same },
    { "Host", "Keep", 1, { T_OBJECT }, T_VOID, t_keep },
    { "Host", "IsKept", 1, { T_OBJECT }, T_BOOL, t_isKept },
    { "Host", "IsObj", 1, { T_OBJECT }, T_BOOL, t_isObj },
};

static int run(const char* src, char* msg, int cap) {
    out[0] = 0;
    Program* p = ms_compile(src, msg, cap);
    if (!p) return 0;
    VM* vm = vm_new(p);
    int si = ms_entry(p, MS_ENTRY_STATIC_INIT);
    int ok = (si < 0 || vm_call(vm, si) == 1) && vm_call(vm, ms_entry(p, MS_ENTRY_MAIN)) == 1;
    if (!ok) snprintf(msg, cap, "%s", ms_vm_error(vm));
    vm_free(vm);
    ms_free_program(p);
    return ok;
}

int main(void) {
    char err[256] = "";
    check(MS_REGISTER(nat), "register");

    const char* src =
        "class Npc { public int Id; public Npc(int i) { Id = i; } public override string ToString() { return \"Npc\" + Id; } }\n"
        "class P { static void Main() {\n"
        "    Npc a = new Npc(1), b = new Npc(2), c = a;\n"
        "    Out.Say(a);                                  // any: the text\n"
        "    Out.Say(Host.Same(a, c)); Out.Say(Host.Same(a, b)); Out.Say(Host.Same(null, null)); Out.Say(Host.Same(a, null));\n"
        "    Host.Keep(b); Out.Say(Host.IsKept(b)); Out.Say(Host.IsKept(a));\n"
        "    Out.Say(Host.IsObj(a)); Out.Say(Host.IsObj(new int[2])); Out.Say(Host.IsObj(\"x\"));\n"
        "} }\n";
    check(run(src, err, sizeof err) && !strcmp(out, "Npc1 True False True False True False True True True "),
          "object parameters get the reference");
    if (fails) printf("  (%s / %s)\n", err, out);

    /* values that are not references do not fit 'object' */
    check(!run("class P { static void Main() { Out.Say(Host.IsObj(5)); } }", err, sizeof err) && strstr(err, "object"),
          "an int does not fit 'object'");
    check(!run("class P { static void Main() { Out.Say(Host.IsObj(1.5f)); } }", err, sizeof err), "a float does not fit 'object'");

    if (!fails) printf("test_object: ok\n");
    return fails ? 1 : 0;
}
