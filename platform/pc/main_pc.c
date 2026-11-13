/* MiniSharp - minimal PC host for the tests (stage 1): compile and run one .cs file.
 *   minisharp file.cs               run Main(), then 60 frames of Update()/coroutines/Draw() if there is Update
 *   minisharp file.cs --frames N    run N frames
 *   minisharp file.cs --check       only compile, report errors
 *   minisharp file.cs --dis         print the bytecode
 * Graphics, input, --press/--ppm, projects (folders) and the terminal come in stages 2–3.
 * Exit code: 0 ok, 1 compile error, 2 bad arguments / cannot read, 3 runtime error. */
#include "ms.h"
#include "lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* the game's clock: 60 frames per second like the 3DS, so timings (WaitForSeconds) match the console */
static double g_ms;
static int frame_millis(void) { return (int)g_ms; }

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* s = (char*)malloc(n + 1);
    if (!s || fread(s, 1, n, f) != (size_t)n) { fclose(f); free(s); return NULL; }
    s[n] = 0; fclose(f);
    return s;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY); /* "\n" not "\r\n", so output matches tests/expected */
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    if (argc < 2) { fprintf(stderr, "usage: %s file.cs [--frames N] [--check] [--dis]\n", argv[0]); return 2; }
    int frames = 60, check = 0, dis = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--check")) check = 1;
        else if (!strcmp(argv[i], "--dis")) dis = 1;
        else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
    }
    size_t len = strlen(argv[1]);
    if (len < 3 || strcmp(argv[1] + len - 3, ".cs")) { fprintf(stderr, "projects (folders) come in stage 2: %s\n", argv[1]); return 2; }

    ms_set_clock(frame_millis);
    lib_register_all();

    char* src = read_file(argv[1]);
    if (!src) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char err[300];
    Program* p = ms_compile(src, err, sizeof err);
    free(src);
    if (!p) { fprintf(stderr, "compile error: %s\n", err); return 1; }
    if (dis) ms_disassemble(p);
    if (check) { ms_free_program(p); return 0; }

    VM* vm = vm_new(p);
    int si = ms_entry(p, MS_ENTRY_STATIC_INIT), mi = ms_entry(p, MS_ENTRY_MAIN);
    int up = ms_entry(p, MS_ENTRY_UPDATE), dr = ms_entry(p, MS_ENTRY_DRAW);
    int ok = si < 0 || vm_call(vm, si);
    if (ok && mi >= 0) ok = vm_call(vm, mi);
    if (ok && up >= 0) {
        for (int f = 0; f < frames && ok; f++) {
            g_ms += 1000.0 / 60;
            ok = vm_call(vm, up);
            if (ok) ok = vm_tick_coroutines(vm);    /* after Update, before Draw, like Unity */
            if (ok && dr >= 0) ok = vm_call(vm, dr);
        }
    }
    int rc = 0;
    if (!ok && !ms_vm_quit_requested(vm)) { fprintf(stderr, "runtime error: %s\n", ms_vm_error(vm)); rc = 3; }
    vm_free(vm);
    ms_free_program(p);
    return rc;
}
