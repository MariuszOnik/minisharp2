/* MiniSharp - Console: Write and WriteLine (from natives.c of 931ad08); Clear, SetCursorPosition, ResetColor on the
 * character grid (lib/textgrid.c) when the host gives one. */
#include "ms.h"
#include "lib.h"
#include "textgrid.h"
#include <stdio.h>

static void std_write(const char* s) { fputs(s, stdout); fflush(stdout); }
static void (*out_fn)(const char* s) = std_write;
static TextGrid* grid;
void lib_set_output(void (*write)(const char* s)) { out_fn = write ? write : std_write; }
void lib_console_set_grid(TextGrid* g) { grid = g; }
TextGrid* lib_console_grid(void) { return grid; }

/* the text goes to the host's output (log, stdout) and, when there is one, into the grid */
static void out(const char* s) {
    out_fn(s);
    if (grid) tg_write(grid, s);
}

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

/* without a grid (output to a file or stdout) these do nothing */
static Value n_clear(VM* vm, Value* a) { (void)vm; (void)a; if (grid) tg_clear(grid); return vnone(); }
static Value n_resetcolor(VM* vm, Value* a) { (void)vm; (void)a; if (grid) tg_reset_colors(grid); return vnone(); }
static Value n_setcursor(VM* vm, Value* a) {
    if (!grid) return vnone();
    if (!tg_set_cursor(grid, a[0].as.i, a[1].as.i))
        ms_vm_fail(vm, "SetCursorPosition(%d, %d) is outside the console (%d x %d: x 0..%d, y 0..%d)", a[0].as.i, a[1].as.i,
                   tg_width(grid), tg_height(grid), tg_width(grid) - 1, tg_height(grid) - 1);
    return vnone();
}

static const NativeDef console[] = {
    { "Console", "WriteLine", 1, { T_ANY }, T_VOID, n_writeline },
    { "Console", "WriteLine", 0, { 0 }, T_VOID, n_writeline0 },
    { "Console", "Write", 1, { T_ANY }, T_VOID, n_write },
    { "Console", "ReadLine", 0, { 0 }, T_STRING, n_readline },
    { "Console", "Clear", 0, { 0 }, T_VOID, n_clear },
    { "Console", "SetCursorPosition", 2, { T_INT, T_INT }, T_VOID, n_setcursor },
    { "Console", "ResetColor", 0, { 0 }, T_VOID, n_resetcolor },
};
void lib_register_console(void) { MS_REGISTER(console); }

void lib_register_all(void) {
    lib_register_console();
    lib_register_math();
    lib_register_random();
    lib_register_strings();
    lib_register_convert();
}
