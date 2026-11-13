/* MiniSharp - minimal PC host for the tests: compile and run one .cs file or a project (a folder).
 *   minisharp file.cs               run Main(), then 60 frames of Update()/coroutines/Draw() if there is Update
 *   minisharp folder                the same for a project: all .cs files of the folder + uses.txt / 'using' libraries
 *   minisharp file.cs --frames N    run N frames
 *   minisharp file.cs --check       only compile, report errors
 *   minisharp file.cs --dis         print the bytecode
 *   minisharp --repl                terminal: run inputs from stdin (a line with an open '{' or '(' goes on in the next)
 *   minisharp --print file.cs       the file printed again by the AST printer ("Format code"; comments are dropped)
 * Graphics, input and --press/--ppm come in stage 3.
 * Exit code: 0 ok, 1 compile error, 2 bad arguments / cannot read, 3 runtime error. */
#include "ms.h"
#include "lib.h"
#include "project.h"
#include "repl.h"
#include "printer.h"
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

/* terminal mode: echoes each line after "> " (or ". " when it continues an open '{'), then runs the input;
 * "#complete text" lists completions instead */
static int repl_main(void) {
    Repl* r = repl_new();
    if (!r) { fprintf(stderr, "out of memory\n"); return 2; }
    char line[1024], err[300], text[8192];
    int tlen = 0;
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\r\n")] = 0;
        printf("%s %s\n", tlen ? "." : ">", line);
        if (!tlen && !strncmp(line, "#complete ", 10)) {
            char out[8][REPL_WORD]; int start;
            const char* t = line + 10;
            int n = repl_complete(r, t, (int)strlen(t), &start, out, 8);
            for (int i = 0; i < n; i++) printf("%s%s", i ? " " : "", out[i]);
            printf("\n");
            continue;
        }
        int l = (int)strlen(line);
        if (tlen + l + 2 > (int)sizeof text) { printf("error: input too long\n"); tlen = 0; continue; }
        if (tlen) text[tlen++] = '\n';
        memcpy(text + tlen, line, l + 1); tlen += l;
        if (repl_incomplete(text)) continue;
        tlen = 0;
        if (!repl_eval(r, text, err, sizeof err)) printf("error: %s\n", err);
        fflush(stdout);
    }
    if (tlen) printf("error: the input ended with an open '{' or '('\n");
    repl_free(r);
    return 0;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY); /* "\n" not "\r\n", so output matches tests/expected */
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    if (argc == 2 && !strcmp(argv[1], "--repl")) {
        ms_set_clock(frame_millis);
        lib_register_all();
        return repl_main();
    }
    if (argc == 3 && !strcmp(argv[1], "--print")) {
        char* src = read_file(argv[2]);
        if (!src) { fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
        char perr[300];
        char* out = ms_format(src, perr, sizeof perr);
        free(src);
        if (!out) { fprintf(stderr, "syntax error: %s\n", perr); return 1; }
        fputs(out, stdout);
        free(out);
        return 0;
    }
    if (argc < 2) { fprintf(stderr, "usage: %s file.cs|folder [--frames N] [--check] [--dis] | --repl\n", argv[0]); return 2; }
    int frames = 60, check = 0, dis = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--check")) check = 1;
        else if (!strcmp(argv[i], "--dis")) dis = 1;
        else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
    }
    char dir[256];
    snprintf(dir, sizeof dir, "%s", argv[1]);
    size_t dl = strlen(dir);
    int isProject = dl < 3 || strcmp(dir + dl - 3, ".cs");     /* a folder: all its .cs files */
    while (isProject && dl > 1 && (dir[dl - 1] == '/' || dir[dl - 1] == '\\')) dir[--dl] = 0;

    ms_set_clock(frame_millis);
    lib_register_all();
    ms_register_hook("Update");             /* the game loop is the host's: the language knows only Main */
    ms_register_hook("Draw");

    char err[300];
    Project proj;                           /* stays empty for one file: project_map_error then does nothing */
    memset(&proj, 0, sizeof proj);
    char* src;
    if (isProject) {
        if (!project_load(dir, &proj, err, sizeof err)) { fprintf(stderr, "%s\n", err); return 2; }
        src = proj.src; proj.src = NULL;
    } else {
        src = read_file(argv[1]);
        if (!src) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    }
    Program* p = ms_compile(src, err, sizeof err);
    free(src);
    if (!p) {
        project_map_error(&proj, err, sizeof err);     /* "line 57: ..." -> "Player.cs line 12: ..." */
        fprintf(stderr, "compile error: %s\n", err);
        project_free(&proj);
        return 1;
    }
    int si = ms_entry(p, MS_ENTRY_STATIC_INIT), mi = ms_entry(p, MS_ENTRY_MAIN);
    int up = ms_hook(p, "Update"), dr = ms_hook(p, "Draw");
    if (mi < 0 && up < 0) {
        fprintf(stderr, "compile error: line 1: no entry point: add 'static void Main()' or 'static void Update()'\n");
        ms_free_program(p); project_free(&proj);
        return 1;
    }
    if (dis) ms_disassemble(p);
    if (check) { ms_free_program(p); project_free(&proj); return 0; }

    VM* vm = vm_new(p);
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
    if (!ok && !ms_vm_quit_requested(vm)) {
        snprintf(err, sizeof err, "%s", ms_vm_error(vm));
        project_map_error(&proj, err, sizeof err);
        fprintf(stderr, "runtime error: %s\n", err);
        rc = 3;
    }
    vm_free(vm);
    ms_free_program(p);
    project_free(&proj);
    return rc;
}
