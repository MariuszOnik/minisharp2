/* Unit test: registration of built-in function tables (ms_register) – no platform host needed.
 * Prints "ok: N" or "FAIL: ..." lines; exit code 0 = all passed. */
#include "ms.h"
#include <stdio.h>
#include <string.h>

static char out[1024];
static int fails, oks;

static void check(int cond, const char* what) {
    if (cond) oks++; else { fails++; printf("FAIL: %s\n", what); }
}

static Value t_write(VM* vm, Value* a) {
    char buf[128]; ms_value_to_text(vm, a[0], buf, sizeof buf);
    strncat(out, buf, sizeof out - strlen(out) - 1);
    strncat(out, "\n", sizeof out - strlen(out) - 1);
    return (Value){ .t = V_NULL };
}
static Value t_first(VM* vm, Value* a)  { (void)vm; (void)a; return (Value){ .t = V_INT, .as.i = 1 }; }
static Value t_second(VM* vm, Value* a) { (void)vm; (void)a; return (Value){ .t = V_INT, .as.i = 2 }; }
static Value t_twice(VM* vm, Value* a)  { (void)vm; return (Value){ .t = V_INT, .as.i = a[0].as.i * 2 }; }

static const NativeDef tableA[] = {
    { "Out", "Write", 1, { T_ANY }, T_VOID, t_write },
    { "Pick", "Which", 1, { T_ANY }, T_INT, t_first },
};
/* second table: same overload as tableA (registered later, so never chosen) and one new function */
static const NativeDef tableB[] = {
    { "Pick", "Which", 1, { T_ANY }, T_INT, t_second },
    { "Num", "Twice", 1, { T_INT }, T_INT, t_twice },
};
static const NativeDef badTable[] = { { "Bad", "NoFn", 0, { 0 }, T_VOID, NULL } };
static const ConstDef consts[] = { { "Num", "Answer", 0, 42, 0 }, { "Num", "Half", 1, 0, 0.5f } };

/* compile and run Main; returns the compile error ("" if none), output in 'out' */
static const char* run(const char* src) {
    static char err[256];
    err[0] = 0; out[0] = 0;
    Program* p = ms_compile(src, err, sizeof err);
    if (!p) return err;
    VM* vm = vm_new(p);
    int si = ms_entry(p, MS_ENTRY_STATIC_INIT), mi = ms_entry(p, MS_ENTRY_MAIN);
    if ((si >= 0 && !vm_call(vm, si)) || (mi >= 0 && !vm_call(vm, mi))) snprintf(err, sizeof err, "runtime: %s", ms_vm_error(vm));
    vm_free(vm); ms_free_program(p);
    return err;
}

int main(void) {
    check(ms_native_count() == 0, "nothing registered at start");
    check(strstr(run("class P { static void Main() { Out.Write(1); } }"), "Out") != NULL,
          "unregistered class is a compile error");

    check(MS_REGISTER(tableA), "register table A");
    check(MS_REGISTER(tableA), "registering table A again is ok");
    check(ms_native_count() == 2, "table A counted once");
    check(MS_REGISTER(tableB), "register table B");
    check(MS_REGISTER_CONSTS(consts), "register constants");
    check(ms_native_count() == 4, "A + B = 4 functions");
    check(ms_native_at(3) && !strcmp(ms_native_at(3)->name, "Twice"), "indices follow registration order");
    check(ms_native_at(4) == NULL && ms_native_at(-1) == NULL, "ms_native_at out of range");
    check(!MS_REGISTER(badTable) && ms_native_count() == 4, "entry without a function is refused");

    const char* e = run("class P { static void Main() {\n"
                        "  Out.Write(\"hi\"); Out.Write(Pick.Which(0)); Out.Write(Num.Twice(21));\n"
                        "  Out.Write(Num.Answer); Out.Write(Num.Half);\n} }");
    check(!*e, e);
    check(!strcmp(out, "hi\n1\n42\n42\n0.5\n"), "program output (first registered overload wins)");
    if (strcmp(out, "hi\n1\n42\n42\n0.5\n")) printf("  got: %s\n", out);

    /* a program compiled before more tables are registered keeps working */
    static const NativeDef late[] = { { "Late", "One", 0, { 0 }, T_INT, t_first } };
    char err[256] = "";
    Program* p = ms_compile("class P { static void Main() { Out.Write(Num.Twice(5)); } }", err, sizeof err);
    check(p != NULL, err);
    check(MS_REGISTER(late) && ms_native_count() == 5, "register a table after compiling");
    if (p) {
        out[0] = 0;
        VM* vm = vm_new(p);
        check(vm_call(vm, ms_entry(p, MS_ENTRY_MAIN)) && !strcmp(out, "10\n"), "earlier program still calls the right function");
        vm_free(vm); ms_free_program(p);
    }

    printf("%s: %d passed, %d failed\n", fails ? "FAIL" : "ok", oks, fails);
    return fails != 0;
}
