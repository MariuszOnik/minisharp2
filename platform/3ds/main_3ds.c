/* MiniSharp - minimal 3DS host (stage 2 step 0): a text console from libctru, no graphics yet.
 *   A      run sdmc:/minisharp/main.cs (Main, then Update/Draw every frame until START)
 *   X      run the test files from sdmc:/minisharp/tests/ (the .cs files there and in errors/, the folders in projects/)
 *          and write each result to sdmc:/minisharp/tests/out/<name>.txt (stdout and errors together, like 2>&1 on PC);
 *          tools/3ds_tests.sh compares them with tests/expected/ on the PC
 *   B      programs and projects on the card (app/files.c), then the code editor (app/editor.c; A in it runs the
 *          program - or the whole project for a file in a project folder - and jumps to the error line)
 *   Y      terminal (app/terminal.c, touch keyboard, app/ui.h on citro2d; START = back)
 *   START  quit
 * START+SELECT held = the Stop button: a running loop (also Main with while (true)) stops with "stopped by the user".
 * If sdmc:/minisharp/tests/auto exists, the tests run at start, the file is removed and the app quits (emulator runs).
 * Console output goes to the top screen and to sdmc:/minisharp/log.txt; the menu is on the bottom screen.
 * The full platform (citro2d, Gfx, Input, menu) comes in stage 3 and builds on this file. */
#include <3ds.h>
#include "ms.h"
#include "lib.h"
#include "project.h"
#include "terminal.h"
#include "editor.h"
#include "files.h"
#include "help.h"
#include "conscreen.h"
#include "ui_3ds.h"
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

/* the Stop button (ms_set_interrupt): the VM asks during long loops, when nobody else reads the buttons */
static int stop_pressed(void) {
    hidScanInput();
    u32 h = hidKeysHeld();
    return (h & KEY_START) && (h & KEY_SELECT);
}

/* a program run by the user shows its Console in this screen (app/conscreen.c, citro2d); NULL = the text console
 * (the tests, X in the menu) */
static ConScreen* g_cs;
static u64 g_lastDraw;
static int g_inGameFrame;                      /* between ui3ds_frame_begin and _end of a game's frame (Draw) */
static int g_game;                             /* the program has Draw: the screens are the game's, not the console's */

static void cs_frame(void) {
    ui3ds_frame_begin(0x000000, 0x0C0C0C);
    cs_draw(g_cs);
    ui3ds_frame_end();
    g_lastDraw = osGetTime();
}

static void out_write(const char* s) {
    if (g_cs) {                                 /* the text is in the grid already (lib/console.c): show it now and then */
        if (g_out) fputs(s, g_out);
        if (!g_inGameFrame && !g_game && osGetTime() - g_lastDraw > 50) cs_frame();
        return;
    }
    if (g_screen) { consoleSelect(&top); fputs(s, stdout); }
    if (g_out) fputs(s, g_out);
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
static char g_lastErr[320];             /* the last error of run() ("" = none), for the editor */

/* an error of run(): to the screen / log and to g_lastErr */
static void run_fail(const char* fmt, const char* a) {
    snprintf(g_lastErr, sizeof g_lastErr, fmt, a);
    out_write(g_lastErr);
    out_write("\n");
}

/* answers for Console.ReadLine while running the tests (tests/input/<name>.txt); NULL = a program of the user */
static FILE* g_in;

/* vm_call; Console.ReadLine reads g_in (tests) or asks on the console screen (the keyboard on the touch screen) */
static int call(VM* vm, int func) {
    int r = vm_call(vm, func);
    while (r == MS_WAIT_INPUT) {
        if (g_cs) {
            int inGame = g_inGameFrame;             /* ReadLine during a game's frame: that frame ends first */
            if (inGame) { ui3ds_frame_end(); g_inGameFrame = 0; }
            cs_ask(g_cs);
            int ev = CS_NONE;
            while (ev != CS_LINE && ev != CS_EOF) {
                if (!aptMainLoop()) { ev = CS_EOF; break; }
                hidScanInput();
                UiInput in;
                ui3ds_input(&in);
                ev = cs_update(g_cs, &in);
                cs_frame();
            }
            if (inGame) { ui3ds_frame_begin(0x000000, 0x000000); g_inGameFrame = 1; }
            r = vm_resume_input(vm, ev == CS_LINE ? cs_line(g_cs) : NULL);
            continue;
        }
        if (!g_in && !g_screen) { r = vm_resume_input(vm, NULL); continue; }    /* a test without answers: like stdin from /dev/null */
        if (!g_in) {
            ms_vm_fail(vm,"Console.ReadLine: typing an answer works only in the terminal (Y in the menu) for now");
            return 0;
        }
        char line[1024];
        if (fgets(line, sizeof line, g_in)) { line[strcspn(line, "\r\n")] = 0; r = vm_resume_input(vm, line); }
        else r = vm_resume_input(vm, NULL);
    }
    return r;
}

static void run(const char* path, int isProject, int frames) {
    char err[300];
    g_lastErr[0] = 0;
    Project proj;
    memset(&proj, 0, sizeof proj);
    char* src;
    if (isProject) {
        if (!project_load(path, &proj, err, sizeof err)) { run_fail("%s", err); return; }
        src = proj.src; proj.src = NULL;
    } else {
        src = read_file(path);
        if (!src) { run_fail("cannot read %s", path); return; }
    }
    Program* p = ms_compile(src, err, sizeof err);
    free(src);
    if (!p) {
        project_map_error(&proj, err, sizeof err);
        run_fail("compile error: %s", err);
        project_free(&proj);
        return;
    }
    int si = ms_entry(p, MS_ENTRY_STATIC_INIT), mi = ms_entry(p, MS_ENTRY_MAIN);
    int up = ms_hook(p, "Update"), dr = ms_hook(p, "Draw");
    if (mi < 0 && up < 0 && dr < 0) {
        run_fail("%s", "compile error: line 1: no entry point: add 'static void Main()', 'Update()' or 'Draw()'");
        ms_free_program(p); project_free(&proj);
        return;
    }
    g_ms = 0;
    if (g_cs) {
        lib_set_gfx(ui3ds_gfx());
        lib_gfx_reset();
        char imgDir[400];                           /* Gfx.LoadImage("x.png"): in the project, or next to the .cs file */
        snprintf(imgDir, sizeof imgDir, "%s", path);
        char* slash = isProject ? NULL : strrchr(imgDir, '/');
        if (slash) *slash = 0;
        lib_gfx_set_dir(imgDir);
    }
    VM* vm = vm_new(p);
    int ok = si < 0 || call(vm, si);
    if (ok && mi >= 0) ok = call(vm, mi);
    if (ok && (up >= 0 || dr >= 0)) {           /* the frames: Update and / or Draw */
        /* the game loop (PLAN.md, decisions 2026-11-14): Update 60 times a second, as many steps as the real time
         * asks (lib_frame_steps); Draw once per shown frame, Gfx.FrameRate of them a second (C3D_FrameRate) */
        g_game = g_cs && frames < 0 && dr >= 0;
        for (int f = 0; (frames < 0 || f < frames) && ok; f++) {
            int steps = 1;
            UiInput in;
            memset(&in, 0, sizeof in);
            if (frames < 0) {
                if (!aptMainLoop()) break;
                hidScanInput();
                if (g_cs) {
                    ui3ds_input(&in);
                    if ((in.held & UI_BTN_START) && (in.held & UI_BTN_SELECT)) break;   /* START alone is Btn.Start */
                    steps = lib_frame_steps((double)osGetTime());
                } else {
                    if (hidKeysDown() & KEY_START) break;
                    gspWaitForVBlank();
                }
            }
            for (int k = 0; k < steps && ok; k++) {
                if (g_cs) lib_input_frame(in.held, in.touching, in.x, in.y);
                g_ms += 1000.0 / 60;
                ok = up < 0 || call(vm, up);
                if (ok) ok = vm_tick_coroutines(vm);
                lib_time_frame(1.0f / 60);
            }
            if (!ok) break;
            if (g_game) {
                ui3ds_set_frame_rate((float)lib_draw_rate());
                ui3ds_frame_begin(0x000000, 0x000000);
                g_inGameFrame = 1;
                lib_draw_begin();
                ok = call(vm, dr);
                if (g_inGameFrame) ui3ds_frame_end();
                g_inGameFrame = 0;
            } else if (dr >= 0) {                   /* the tests (X): Draw without screens */
                lib_draw_begin();
                ok = call(vm, dr);
            } else if (g_cs && frames < 0) cs_frame();   /* a text program with Update: the console screen */
        }
        if (g_game) ui3ds_set_frame_rate(60);
        g_game = 0;
    }
    lib_set_gfx(NULL);
    if (!ok && !ms_vm_quit_requested(vm)) {
        snprintf(err, sizeof err, "%s", ms_vm_error(vm));
        project_map_error(&proj, err, sizeof err);
        run_fail("runtime error: %s", err);
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
        char inp[300];
        snprintf(inp, sizeof inp, TESTS_DIR "/input/%s.txt", name);
        g_in = projects ? NULL : fopen(inp, "r");
        run(path, projects, test_frames(name));
        if (g_out) { fclose(g_out); g_out = NULL; }
        if (g_in) { fclose(g_in); g_in = NULL; }
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

/* the example projects built into the .3dsx (romfs:/examples, from sdk/examples) are copied to the card once:
 * a folder that is there already (maybe changed by the user) is left alone */
static void copy_file(const char* from, const char* to) {
    FILE* in = fopen(from, "rb");
    if (!in) return;
    FILE* out = fopen(to, "wb");
    if (out) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
        fclose(out);
    }
    fclose(in);
}

static void install_examples(void) {
    DIR* d = opendir("romfs:/examples");
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char src[300], dst[300];
        snprintf(src, sizeof src, "romfs:/examples/%s", e->d_name);
        snprintf(dst, sizeof dst, APP_DIR "/%s", e->d_name);
        struct stat st;
        if (stat(src, &st) || !S_ISDIR(st.st_mode) || !stat(dst, &st)) continue;
        mkdir(dst, 0777);
        DIR* f = opendir(src);
        if (!f) continue;
        struct dirent* g;
        while ((g = readdir(f))) {
            if (g->d_name[0] == '.') continue;
            char a[600], b[600];
            snprintf(a, sizeof a, "%s/%s", src, g->d_name);
            snprintf(b, sizeof b, "%s/%s", dst, g->d_name);
            copy_file(a, b);
        }
        closedir(f);
    }
    closedir(d);
}

static void menu(void) {
    consoleSelect(&bottom); consoleClear();
    printf("MiniSharp2 (" BUILD_ID ")\n\n");
    printf("A      run " MAIN_CS "\n");
    printf("X      run tests from " TESTS_DIR "\n");
    printf("B      editor: programs and projects\n");
    printf("Y      terminal\n");
    printf("R      Help: the language\n");
    printf("START  quit\n");
}

/* Y: the terminal (app/terminal.c): lines typed on the touch keyboard run at once, the output scrolls on the top
 * screen. The screens go to citro2d for the time of the terminal and back to the text console after it. */
static void terminal(void) {
    Terminal* t = NULL;
    if (!ui3ds_init() || !(t = term_new())) {
        consoleInit(GFX_TOP, &top); consoleInit(GFX_BOTTOM, &bottom); menu();
        printf("\ncannot open the terminal\n");
        return;
    }
    term_set_stop_hint(t, "START+SELECT: stop a loop");
    while (aptMainLoop()) {
        hidScanInput();
        UiInput in;
        ui3ds_input(&in);
        if (!term_update(t, &in)) break;
        ui3ds_frame_begin(0x101418, 0x0C0C0C);
        term_draw(t);
        ui3ds_frame_end();
    }
    term_free(t);
    lib_set_output(out_write);
    ui3ds_exit();
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    menu();
}

/* runs a program by the user on the console screen (the screens are in citro2d already): its Console is a 40 x 20
 * grid on the top screen (the system font is proportional: bigger cells than 50 x 30 stay readable), ReadLine asks
 * on the touch screen; at the end the result stays until A / B. 1 = no error (else the error in g_lastErr). */
static int run_on_screen(const char* path, int isProject) {
    g_cs = cs_new(40, 20);
    if (!g_cs) { snprintf(g_lastErr, sizeof g_lastErr, "out of memory"); return 0; }
    cs_set_hint(g_cs, "START+SELECT: stop the program");
    lib_console_set_grid(cs_grid(g_cs));
    g_out = fopen(LOG_PATH, "w");
    cs_frame();
    run(path, isProject, -1);
    if (g_out) { fclose(g_out); g_out = NULL; }
    cs_finish(g_cs, g_lastErr[0] == 0, g_lastErr);
    while (aptMainLoop()) {
        hidScanInput();
        UiInput in;
        ui3ds_input(&in);
        if (cs_update(g_cs, &in) == CS_LEAVE) break;
        cs_frame();
    }
    lib_console_set_grid(NULL);
    cs_free(g_cs);
    g_cs = NULL;
    return g_lastErr[0] == 0;
}

/* the editor's Run: the program runs on the console screen, then back to the editor; the error, if any, goes back
 * to the editor for the jump */
static int editor_run(const char* path, char* err, int errcap) {
    char proj[400];
    int isProject = files_project_of(APP_DIR, path, proj, sizeof proj);
    int ok = run_on_screen(isProject ? proj : path, isProject);
    snprintf(err, errcap, "%s", g_lastErr);
    return ok;
}

static void editor_help(void);

/* the code editor (app/editor.c) on one file, until START */
static void edit_file(const char* path) {
    EditorHost host = { editor_run, editor_help };
    Editor* ed = ed_new(&host);
    if (!ed) return;
    ed_open(ed, path);
    char proj[400];
    if (files_project_of(APP_DIR, path, proj, sizeof proj)) ed_set_project(ed, proj);
    while (aptMainLoop()) {
        hidScanInput();
        UiInput in;
        ui3ds_input(&in);
        if (!ed_update(ed, &in)) break;
        ui3ds_frame_begin(0x1E1E1E, 0x0C0C0C);
        ed_draw(ed);
        ui3ds_frame_end();
    }
    ed_free(ed);
}

/* R: Help (app/help.c) – the chapters of sdk/docs built into the .3dsx (romfs:/docs) together with the ones in
 * sdmc:/minisharp/docs (e.g. newer chapters sent over FTP): a chapter on the card replaces the built-in file of the
 * same name, other files on the card are added. */
/* the Help loop on screens already in citro2d (from the menu and from the editor's Help tool); 0 = no memory */
static int help_loop(void) {
    Help* h = help_new();
    if (!h) return 0;
    const char* dirs[] = { APP_DIR "/docs", "romfs:/docs" };   /* the card adds chapters and replaces same-named ones */
    help_load_dirs(h, dirs, 2);
    help_set_copy(h, ed_set_clipboard);                       /* Copy: the example goes to the editor's clipboard */
    while (aptMainLoop()) {
        hidScanInput();
        UiInput in;
        ui3ds_input(&in);
        if (!help_update(h, &in)) break;
        ui3ds_frame_begin(0x101418, 0x0C0C0C);
        help_draw(h);
        ui3ds_frame_end();
    }
    help_free(h);
    return 1;
}

static void editor_help(void) { help_loop(); }

static void help(void) {
    int inited = ui3ds_init();
    if (!inited || !help_loop()) {
        if (inited) ui3ds_exit();
        consoleInit(GFX_TOP, &top); consoleInit(GFX_BOTTOM, &bottom); menu();
        printf("\ncannot open Help\n");
        return;
    }
    ui3ds_exit();
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    menu();
}

/* B: the programs and projects on the card (app/files.c); a chosen file opens in the editor, START there
 * comes back to the list */
static void editor(void) {
    Files* fs = NULL;
    if (!ui3ds_init() || !(fs = files_new(APP_DIR))) {
        consoleInit(GFX_TOP, &top); consoleInit(GFX_BOTTOM, &bottom); menu();
        printf("\ncannot open the editor\n");
        return;
    }
    while (aptMainLoop()) {
        hidScanInput();
        UiInput in;
        ui3ds_input(&in);
        int r = files_update(fs, &in);
        if (r == FILES_LEAVE) break;
        if (r == FILES_OPEN) { edit_file(files_chosen(fs)); files_scan(fs); continue; }
        ui3ds_frame_begin(0x101418, 0x0C0C0C);
        files_draw(fs);
        ui3ds_frame_end();
    }
    files_free(fs);
    ui3ds_exit();
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    menu();
}

int main(void) {
    gfxInitDefault();
    romfsInit();                                    /* sdk/ built into the .3dsx: the Help chapters */
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    mkdir(APP_DIR, 0777);
    ensure_main();
    install_examples();

    ms_set_clock(frame_millis);
    ms_set_interrupt(stop_pressed);
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
        romfsExit();
        gfxExit();
        return 0;
    }

    while (aptMainLoop()) {
        hidScanInput();
        u32 k = hidKeysDown();
        if (k & KEY_START) break;
        if (k & KEY_A) {                            /* main.cs on the console screen */
            if (ui3ds_init()) {
                run_on_screen(MAIN_CS, 0);
                ui3ds_exit();
            }
            consoleInit(GFX_TOP, &top);
            consoleInit(GFX_BOTTOM, &bottom);
            menu();
            printf("\n%s (output also in log.txt)\n", g_lastErr[0] ? "error" : "done");
        }
        if (k & KEY_X) run_tests();
        if (k & KEY_Y) terminal();
        if (k & KEY_B) editor();
        if (k & KEY_R) help();
        gspWaitForVBlank();
    }
    romfsExit();
    gfxExit();
    return 0;
}
