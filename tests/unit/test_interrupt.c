/* Unit test: the Stop button (ms_set_interrupt) – an endless loop stops with "stopped by the user", a loop that ends
 * is not disturbed while the button is not pressed, and without a button nothing is checked.
 * Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "ms.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

static int calls, pressAfter;           /* the button is "pressed" from the pressAfter-th check on */
static int button(void) { return ++calls >= pressAfter; }

/* runs Main; returns 1 = ok, the error in err */
static int run(const char* src, char* err, int errlen) {
    Program* p = ms_compile(src, err, errlen);
    if (!p) return 0;
    VM* vm = vm_new(p);
    int ok = vm_call(vm, ms_entry(p, MS_ENTRY_MAIN));
    if (!ok) snprintf(err, errlen, "%s", ms_vm_error(vm));
    vm_free(vm);
    ms_free_program(p);
    return ok;
}

int main(void) {
    char err[256];
    const char* endless = "class P { static void Main() { int i = 0; while (true) { i++; } } }";
    const char* nested = "class P { static void Spin() { for (;;) { } } static void Main() { Spin(); } }";
    const char* ends = "class P { static void Main() { int s = 0; for (int i = 0; i < 1000000; i++) s += i; } }";
    const char* doLoop = "class P { static void Main() { int i = 0; do { i++; } while (i >= 0 || true); } }";

    ms_set_interrupt(button);
    calls = 0; pressAfter = 3;
    check(!run(endless, err, sizeof err) && strstr(err, "stopped by the user"), "endless while stops");
    check(calls == 3, "checked until pressed");

    calls = 0; pressAfter = 1;
    check(!run(nested, err, sizeof err) && strstr(err, "stopped by the user"), "endless for(;;) in a method stops");
    calls = 0; pressAfter = 1;
    check(!run(doLoop, err, sizeof err) && strstr(err, "stopped by the user"), "endless do-while stops");

    calls = 0; pressAfter = 1000000;
    check(run(ends, err, sizeof err), "a loop that ends runs to the end");
    check(calls > 0 && calls < 100, "the button is checked rarely (every ~65 thousand jumps)");

    ms_set_interrupt(NULL);
    calls = 0; pressAfter = 1;
    check(run(ends, err, sizeof err) && calls == 0, "no button: nothing is checked");

    if (!fails) printf("interrupt: ok\n");
    return fails ? 1 : 0;
}
