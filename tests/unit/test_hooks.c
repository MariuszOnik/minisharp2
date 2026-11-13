/* Unit test: hooks (ms_register_hook / ms_hook) and sources of the layers (ms_register_source) – no platform host.
 * The language knows only Main; a layer above names the other methods it calls and the classes it adds.
 * Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "ms.h"
#include <stdio.h>
#include <string.h>

static char out[1024];
static int fails;

static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

static Value t_say(VM* vm, Value* a) {
    char buf[128]; ms_value_to_text(vm, a[0], buf, sizeof buf);
    strncat(out, buf, sizeof out - strlen(out) - 1);
    strncat(out, " ", sizeof out - strlen(out) - 1);
    return (Value){ .t = V_NULL };
}
static const NativeDef said[] = { { "Out", "Say", 1, { T_ANY }, T_VOID, t_say } };

static const char* TWO_CLASSES =
    "[Order(5)] class A { static void Tick() { Out.Say(\"A\"); } }\n"
    "class B { static void Tick() { Out.Say(\"B\"); } static void Main() { Out.Say(\"main\"); } }\n"
    "class C { static void Tick(int x) { } int Tock() { return 1; } }\n";     /* parameters / not static: not a hook */

/* run function f (static init first); returns 1 = ok */
static int run(Program* p, int f) {
    VM* vm = vm_new(p);
    int si = ms_entry(p, MS_ENTRY_STATIC_INIT);
    int ok = (si < 0 || vm_call(vm, si)) && (f < 0 || vm_call(vm, f));
    vm_free(vm);
    return ok;
}

int main(void) {
    char err[256];
    check(MS_REGISTER(said), "register Out.Say");

    /* before any hook is registered, Tick is just a method: no hook, and the program compiles */
    Program* p = ms_compile(TWO_CLASSES, err, sizeof err);
    check(p != NULL, "compiles without hooks");
    if (p) { check(ms_hook(p, "Tick") < 0, "unregistered name is not a hook"); ms_free_program(p); }

    check(ms_register_hook("Tick"), "register hook Tick");
    check(ms_register_hook("Tick"), "registering Tick again is ok");
    check(!ms_register_hook("Main"), "Main is the language's, not a hook");
    check(!ms_register_hook(""), "empty name refused");

    p = ms_compile(TWO_CLASSES, err, sizeof err);
    check(p != NULL, "compiles with hook Tick");
    if (p) {
        int t = ms_hook(p, "Tick");
        check(t >= 0, "Tick found");
        out[0] = 0;
        check(run(p, t) && !strcmp(out, "B A "), "Tick of every class, [Order(5)] last");
        out[0] = 0;
        check(run(p, ms_entry(p, MS_ENTRY_MAIN)) && !strcmp(out, "main "), "Main still works");
        check(ms_hook(p, "Tock") < 0, "unknown hook is -1");
        ms_free_program(p);
    }

    /* no Main at all: the language does not complain, the host decides */
    p = ms_compile("class X { static void Tick() { } }", err, sizeof err);
    check(p != NULL && ms_entry(p, MS_ENTRY_MAIN) < 0, "program without Main compiles");
    ms_free_program(p);

    /* sources: classes a layer adds to every program, unless the program has its own class of that name */
    static const char NODE[] = "abstract class Node { public virtual string Run() { return \"base\"; } }\n"
                               "class Helper { public static int Seven() { return 7; } }";
    check(ms_register_source(NODE), "register a source");
    check(ms_register_source(NODE), "registering it again is ok");
    p = ms_compile("class Mine : Node { public override string Run() { return \"mine\"; } }\n"
                   "class P { static void Main() { Node n = new Mine(); Out.Say(n.Run()); Out.Say(Helper.Seven()); } }",
                   err, sizeof err);
    check(p != NULL, "program derives from a class of a source");
    if (p) { out[0] = 0; check(run(p, ms_entry(p, MS_ENTRY_MAIN)) && !strcmp(out, "mine 7 "), "source classes work"); ms_free_program(p); }
    else printf("  (%s)\n", err);
    p = ms_compile("class Node { public int Cost = 3; }\n"
                   "class P { static void Main() { Out.Say(new Node().Cost); } }", err, sizeof err);
    check(p != NULL, "own class Node replaces the source's");
    if (p) { out[0] = 0; check(run(p, ms_entry(p, MS_ENTRY_MAIN)) && !strcmp(out, "3 "), "own Node is used"); ms_free_program(p); }
    else printf("  (%s)\n", err);
    p = ms_compile("class P { static void Main() { int x = \"a\"; } }", err, sizeof err);
    check(p == NULL && strstr(err, "line 1:") != NULL, "error lines are the program's, not the source's");
    ms_free_program(p);
    return fails != 0;
}
