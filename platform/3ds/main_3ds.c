/* MiniSharp - minimal 3DS host (stage 2 step 0): a text console from libctru, no graphics yet.
 *   A      run sdmc:/minisharp/main.cs (Main, then Update/Draw every frame until START)
 *   X      run the test files from sdmc:/minisharp/tests/ (the .cs files there and in errors/, the folders in projects/)
 *          and write each result to sdmc:/minisharp/tests/out/<name>.txt (stdout and errors together, like 2>&1 on PC);
 *          tools/3ds_tests.sh compares them with tests/expected/ on the PC
 *   START  quit
 * If sdmc:/minisharp/tests/auto exists, the tests run at start, the file is removed and the app quits (emulator runs).
 * Console output goes to the top screen and to sdmc:/minisharp/log.txt; the menu is on the bottom screen.
 * The full platform (citro2d, Gfx, Input, menu) comes in stage 3 and builds on this file. */
#include <3ds.h>
#include "ms.h"
#include "lib.h"
#include "project.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* the parser and the compiler recurse; the default 32 KB is too little for nested programs (the old host used 256 KB) */
u32 __stacksize__ = 256 * 1024;

#define APP_DIR   "sdmc:/minisharp"
#define MAIN_CS   APP_DIR "/main.cs"
#define LOG_PATH  APP_DIR "/log.txt"
#define TESTS_DIR APP_DIR "/tests"
#define OUT_DIR   TESTS_DIR "/out"

#ifndef BUILD_ID
#define BUILD_ID "?"
#endif

static PrintConsole top, bottom;
static FILE* g_out;            /* where Console output and errors go besides the screen (log or a test's result file) */
static int g_screen = 1;       /* 0 while running tests: only the file */
static int g_tests;            /* tests run so far (progress line) */

/* the game's clock: 60 frames per second, the same as the PC host, so WaitForSeconds matches */
static double g_ms;
static int frame_millis(void) { return (int)g_ms; }

static void out_write(const char* s) {
    if (g_screen) { consoleSelect(&top); fputs(s, stdout); }
    if (g_out) fputs(s, g_out);
}

static void out_printf(const char* fmt, const char* a) {
    char buf[400];
    snprintf(buf, sizeof buf, fmt, a);
    out_write(buf);
}

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* s = (char*)malloc(n + 1);
    if (!s || fread(s, 1, n, f) != (size_t)n) { fclose(f); free(s); return NULL; }
    s[n] = 0; fclose(f);
    return s;
}

/* frames > 0: a fixed number of frames (tests); frames < 0: until START (a program run by the user) */
static void run(const char* path, int isProject, int frames) {
    char err[300];
    Project proj;
    memset(&proj, 0, sizeof proj);
    char* src;
    if (isProject) {
        if (!project_load(path, &proj, err, sizeof err)) { out_printf("%s\n", err); return; }
        src = proj.src; proj.src = NULL;
    } else {
        src = read_file(path);
        if (!src) { out_printf("cannot read %s\n", path); return; }
    }
    Program* p = ms_compile(src, err, sizeof err);
    free(src);
    if (!p) {
        project_map_error(&proj, err, sizeof err);
        out_printf("compile error: %s\n", err);
        project_free(&proj);
        return;
    }
    int si = ms_entry(p, MS_ENTRY_STATIC_INIT), mi = ms_entry(p, MS_ENTRY_MAIN);
    int up = ms_hook(p, "Update"), dr = ms_hook(p, "Draw");
    if (mi < 0 && up < 0) {
        out_write("compile error: line 1: no entry point: add 'static void Main()' or 'static void Update()'\n");
        ms_free_program(p); project_free(&proj);
        return;
    }
    g_ms = 0;
    VM* vm = vm_new(p);
    int ok = si < 0 || vm_call(vm, si);
    if (ok && mi >= 0) ok = vm_call(vm, mi);
    if (ok && up >= 0) {
        for (int f = 0; (frames < 0 || f < frames) && ok; f++) {
            if (frames < 0) {
                if (!aptMainLoop()) break;
                hidScanInput();
                if (hidKeysDown() & KEY_START) break;
                gspWaitForVBlank();
            }
            g_ms += 1000.0 / 60;
            ok = vm_call(vm, up);
            if (ok) ok = vm_tick_coroutines(vm);
            if (ok && dr >= 0) ok = vm_call(vm, dr);
        }
    }
    if (!ok && !ms_vm_quit_requested(vm)) {
        snprintf(err, sizeof err, "%s", ms_vm_error(vm));
        project_map_error(&proj, err, sizeof err);
        out_printf("runtime error: %s\n", err);
    }
    vm_free(vm);
    ms_free_program(p);
    project_free(&proj);
}

/* "--frames N" for this test from tests/opcje.txt (copied next to the tests), else 60 like the PC host */
static int test_frames(const char* name) {
    int frames = 60;
    FILE* f = fopen(TESTS_DIR "/opcje.txt", "r");
    if (!f) return frames;
    char line[200];
    size_t nl = strlen(name);
    while (fgets(line, sizeof line, f)) {
        const char* a;
        if (strncmp(line, name, nl) || line[nl] != ' ') continue;
        if ((a = strstr(line, "--frames "))) frames = atoi(a + 9);
    }
    fclose(f);
    return frames;
}

static int ends_with(const char* s, const char* end) {
    size_t a = strlen(s), b = strlen(end);
    return a >= b && !strcmp(s + a - b, end);
}

/* runs every .cs in dir (or every subfolder as a project); results go to OUT_DIR/<prefix><name>.txt */
static int run_dir(const char* dir, const char* prefix, int projects) {
    DIR* d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d))) {
        char path[300], name[128], outp[300];
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st)) continue;
        int isDir = S_ISDIR(st.st_mode);
        if (projects ? !isDir : (isDir || !ends_with(e->d_name, ".cs"))) continue;
        snprintf(name, sizeof name, "%s", e->d_name);
        if (!projects) name[strlen(name) - 3] = 0;
        snprintf(outp, sizeof outp, OUT_DIR "/%s%s.txt", prefix, name);
        consoleSelect(&bottom);
        printf("\x1b[4;1H\x1b[2K test %d: %.30s", ++g_tests, name);   /* one progress line, not a list of names */
        g_out = fopen(outp, "w");
        run(path, projects, test_frames(name));
        if (g_out) { fclose(g_out); g_out = NULL; }
        n++;
    }
    closedir(d);
    return n;
}

static void menu(void);

static void run_tests(void) {
    mkdir(OUT_DIR, 0777);
    g_screen = 0;
    g_tests = 0;
    consoleSelect(&bottom); consoleClear();
    printf("Running the tests from\n" TESTS_DIR "\n(the results are compared on the PC)\n");
    int n = run_dir(TESTS_DIR, "", 0);
    n += run_dir(TESTS_DIR "/errors", "error_", 0);
    n += run_dir(TESTS_DIR "/projects", "", 1);
    g_screen = 1;
    menu();
    if (n) printf("\n%d tests run, results in\n" OUT_DIR "/\nCompare on the PC:\n  sh tools/3ds_tests.sh compare <sd>\n", n);
    else printf("\nno tests in " TESTS_DIR "\n(sh tools/3ds_send.sh tests)\n");
}

/* a first program, so A does something on a fresh card */
static void ensure_main(void) {
    FILE* f = fopen(MAIN_CS, "r");
    if (f) { fclose(f); return; }
    f = fopen(MAIN_CS, "w");
    if (!f) return;
    fputs("class Program\n{\n    static void Main()\n    {\n        Console.WriteLine(\"Hello from MiniSharp2!\");\n"
          "        for (int i = 1; i <= 5; i++)\n            Console.WriteLine(i + \" * \" + i + \" = \" + i * i);\n    }\n}\n", f);
    fclose(f);
}

static void menu(void) {
    consoleSelect(&bottom); consoleClear();
    printf("MiniSharp2 (" BUILD_ID ")\n\n");
    printf("A      run " MAIN_CS "\n");
    printf("X      run tests from " TESTS_DIR "\n");
    printf("START  quit\n");
}

int main(void) {
    gfxInitDefault();
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    mkdir(APP_DIR, 0777);
    ensure_main();

    ms_set_clock(frame_millis);
    lib_register_all();
    lib_set_output(out_write);
    ms_register_hook("Update");
    ms_register_hook("Draw");
    menu();

    FILE* autof = fopen(TESTS_DIR "/auto", "r");
    if (autof) {
        fclose(autof);
        remove(TESTS_DIR "/auto");
        run_tests();
        gfxExit();
        return 0;
    }

    while (aptMainLoop()) {
        hidScanInput();
        u32 k = hidKeysDown();
        if (k & KEY_START) break;
        if (k & KEY_A) {
            consoleSelect(&top); consoleClear();
            consoleSelect(&bottom); printf("\nrunning... (START stops Update)\n");
            g_out = fopen(LOG_PATH, "w");
            run(MAIN_CS, 0, -1);
            if (g_out) { fclose(g_out); g_out = NULL; }
            menu();
            printf("\ndone (output also in log.txt)\n");
        }
        if (k & KEY_X) run_tests();
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
