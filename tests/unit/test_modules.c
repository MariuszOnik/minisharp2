/* Unit test: modules (ms_register_module) – parts written in C that are built in but turned on by 'using Name;'.
 * Until then their classes do not exist for the program; with 'using' (in any file) the whole program sees them,
 * together with the module's C# classes. Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "ms.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

static char out[512];
static Value t_say(VM* vm, Value* a) {
    char buf[128]; ms_value_to_text(vm, a[0], buf, sizeof buf);
    strncat(out, buf, sizeof out - strlen(out) - 1);
    strncat(out, " ", sizeof out - strlen(out) - 1);
    return (Value){ .t = V_NULL };
}
static Value t_two(VM* vm, Value* a) { (void)vm; (void)a; Value v; v.t = V_INT; v.as.i = 2; return v; }
static Value t_lamp(VM* vm, Value* a) { (void)vm; (void)a; Value v; v.t = V_INT; v.as.i = 40; return v; }

static const NativeDef core[] = { { "Out", "Say", 1, { T_ANY }, T_VOID, t_say } };
static const NativeDef demoNat[] = { { "Demo", "Two", 0, { 0 }, T_INT, t_two } };
static const ConstDef demoConst[] = { { "Demo", "Max", 0, 99, 0 } };
static const MsModule demo = { "Demo", demoNat, 1, demoConst, 1,
    "abstract class DemoBehaviour { public virtual int Id() { return 1; } public int Twice() { return Id() * 2; } }\n" };
static const NativeDef lightNat[] = { { "Light", "Power", 0, { 0 }, T_INT, t_lamp } };
static const MsModule light = { "Light", lightNat, 1, NULL, 0, NULL };
static const MsModule badName = { "Demo", NULL, 0, NULL, 0, NULL };

/* compile and run Main; returns 1 = ok; *msg = the compile error */
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
    char err[256];
    check(MS_REGISTER(core), "register Out.Say");
    int before = ms_native_count();
    check(ms_register_module(&demo), "register module Demo");
    check(ms_register_module(&demo), "registering it again is ok");
    check(!ms_register_module(&badName), "a second module named Demo is refused");
    check(ms_register_module(&light), "register module Light");
    check(ms_module_count() == 2 && ms_module_at(0) == &demo && ms_module_at(1) == &light && !ms_module_at(2), "module list");
    check(ms_native_count() == before + 2, "module functions are in the table (for Help and the graph)");

    /* without 'using': the module's classes do not exist, the error says what to add */
    check(!run("class P { static void Main() { Out.Say(Demo.Two()); } }", err, sizeof err) &&
          strstr(err, "using Demo;"), "Demo.Two() without using: hint");
    check(!run("class P { static void Main() { Out.Say(Demo.Max); } }", err, sizeof err), "Demo.Max without using");
    check(!run("class D : DemoBehaviour { } class P { static void Main() { } }", err, sizeof err) &&
          strstr(err, "DemoBehaviour"), "module C# class without using");
    /* the names are free: a program may have its own class Demo */
    check(run("class Demo { public static int Two() { return 22; } } class P { static void Main() { Out.Say(Demo.Two()); } }", err, sizeof err) &&
          !strcmp(out, "22 "), "own class Demo without using");

    /* with 'using Demo;' */
    const char* withUsing =
        "using Demo;\n"
        "class Lampa : DemoBehaviour { public override int Id() { return 7; } }\n"
        "class P { static void Main() { Out.Say(Demo.Two()); Out.Say(Demo.Max); Out.Say(new Lampa().Twice()); } }\n";
    check(run(withUsing, err, sizeof err) && !strcmp(out, "2 99 14 "), "using Demo: functions, constants, C# classes");
    if (fails) printf("  (%s / %s)\n", err, out);

    /* 'using' in one file of a project turns the module on for all of it */
    const char* oneFile =
        "namespace Gra { using Demo; class A { public static int X() { return Demo.Two(); } } }\n"
        "namespace Gra { class P { static void Main() { Out.Say(A.X() + Demo.Two()); } } }\n";
    check(run(oneFile, err, sizeof err) && !strcmp(out, "4 "), "using in one place is enough");
    if (fails) printf("  (%s / %s)\n", err, out);

    /* only the modules named are on */
    check(!run("using Demo; class P { static void Main() { Out.Say(Light.Power()); } }", err, sizeof err) &&
          strstr(err, "using Light;"), "Light stays off with using Demo");
    check(run("using Demo; using Light; class P { static void Main() { Out.Say(Light.Power() + Demo.Two()); } }", err, sizeof err) &&
          !strcmp(out, "42 "), "two modules on");
    /* an alias does not turn a module on */
    check(!run("using L = Light; class P { static void Main() { Out.Say(Light.Power()); } }", err, sizeof err), "alias is not 'using Light;'");

    /* the next program starts with every module off again */
    check(!run("class P { static void Main() { Out.Say(Demo.Two()); } }", err, sizeof err), "modules off for the next program");

    if (!fails) printf("test_modules: ok\n");
    return fails ? 1 : 0;
}
