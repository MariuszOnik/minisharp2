/* MiniSharp - Console: Write and WriteLine (from natives.c of 931ad08). */
#include "ms.h"
#include "lib.h"
#include <stdio.h>

static void std_write(const char* s) { fputs(s, stdout); fflush(stdout); }
static void (*out)(const char* s) = std_write;
void lib_set_output(void (*write)(const char* s)) { out = write ? write : std_write; }

static Value vnone(void) { Value v; v.t = V_NULL; v.as.o = NULL; return v; }

static void print_value(VM* vm, Value v) {
    if (v.t == V_OBJ && v.as.o->type == O_STRING) { out(((ObjString*)v.as.o)->chars); return; }
    char buf[96];
    ms_value_to_text(vm, v, buf, sizeof buf);
    out(buf);
}
static Value n_writeline(VM* vm, Value* a) { print_value(vm, a[0]); out("\n"); return vnone(); }
static Value n_writeline0(VM* vm, Value* a) { (void)vm; (void)a; out("\n"); return vnone(); }
static Value n_write(VM* vm, Value* a) { print_value(vm, a[0]); return vnone(); }
/* Console.ReadLine: the VM stops and the host asks the user (vm_call returns MS_WAIT_INPUT); the line it gets
 * is the result, given by vm_resume_input */
static Value n_readline(VM* vm, Value* a) { (void)a; ms_vm_wait_input(vm); return vnone(); }

static const NativeDef console[] = {
    { "Console", "WriteLine", 1, { T_ANY }, T_VOID, n_writeline },
    { "Console", "WriteLine", 0, { 0 }, T_VOID, n_writeline0 },
    { "Console", "Write", 1, { T_ANY }, T_VOID, n_write },
    { "Console", "ReadLine", 0, { 0 }, T_STRING, n_readline },
};
void lib_register_console(void) { MS_REGISTER(console); }

void lib_register_all(void) {
    lib_register_console();
    lib_register_math();
    lib_register_random();
    lib_register_strings();
    lib_register_convert();
}
