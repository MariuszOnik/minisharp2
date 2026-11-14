/* MiniSharp - minimal PC host for the tests: compile and run one .cs file or a project (a folder).
 *   minisharp file.cs               run Main(), then 60 frames of Update()/coroutines/Draw() if there is Update
 *   minisharp folder                the same for a project: all .cs files of the folder + uses.txt / 'using' libraries
 *   minisharp file.cs --frames N    run N frames
 *   minisharp file.cs --check       only compile, report errors
 *   minisharp file.cs --dis         print the bytecode
 *   minisharp file.cs --echo        Console.ReadLine reads stdin; --echo also prints each line read (a console session)
 *   minisharp file.cs --grid 50x30  the console is a character grid (Clear, SetCursorPosition, colours work); at the end
 *                                   its rows are printed instead of the output stream
 *   minisharp --repl                terminal: run inputs from stdin (a line with an open '{' or '(' goes on in the next)
 *   minisharp --print file.cs       the file printed again by the AST printer ("Format code"; comments are dropped)
 *   minisharp --ci folder           editor suggestions (codeindex) at every marker comment slash-star-?-star-slash
 *                                   in the project's files: "File.cs:LINE: type -> first 8 names"
 * Ctrl+C is the Stop button: a running loop stops with "stopped by the user" (the second Ctrl+C kills the host).
 *   minisharp file.cs --press A@10 --press Left@5-20    buttons held in these frames (0 = the first Update)
 *   minisharp file.cs --touch 3@100,50                  the touch screen touched at 100,50 in frame 3 (3-6@... = a range)
 *   minisharp file.cs --hash                            at the end: "screens: 1a2b3c4d" (a hash of both screens)
 *   minisharp file.cs --ppm out.ppm                     at the end: both screens as a picture (400 x 480)
 * The screens are in memory (platform/pc/swgfx.c): 400x240 on top and 320x240 below, black at the start of a frame.
 * Exit code: 0 ok, 1 compile error, 2 bad arguments / cannot read, 3 runtime error. */
#include "ms.h"
#include "lib.h"
#include "project.h"
#include "repl.h"
#include "printer.h"
#include "codeindex.h"
#include "textgrid.h"
#include "gfx.h"
#include "swgfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* the game's clock: 60 frames per second like the 3DS, so timings (WaitForSeconds) match the console */
static double g_ms;
static int frame_millis(void) { return (int)g_ms; }

/* Stop button (ms_set_interrupt) */
static volatile sig_atomic_t g_stop;
static void on_ctrl_c(int sig) { g_stop = 1; signal(sig, SIG_DFL); }
static int stop_pressed(void) { return g_stop; }

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* s = (char*)malloc(n + 1);
    if (!s || fread(s, 1, n, f) != (size_t)n) { fclose(f); free(s); return NULL; }
    s[n] = 0; fclose(f);
    return s;
}

/* one line of stdin without the newline (any length); NULL at the end of input. The caller frees it. */
static char* read_line(void) {
    size_t n = 0, cap = 256;
    char* s = (char*)malloc(cap);
    int c = 0;
    if (!s) return NULL;
    while ((c = getchar()) != EOF && c != '\n') {
        if (n + 1 >= cap) { char* t = (char*)realloc(s, cap *= 2); if (!t) { free(s); return NULL; } s = t; }
        s[n++] = (char)c;
    }
    if (c == EOF && n == 0) { free(s); return NULL; }
    if (n && s[n - 1] == '\r') n--;
    s[n] = 0;
    return s;
}

/* --press / --touch: what is held in which frames */
typedef struct { int from, to; uint32_t btn; int x, y; } Press;
static Press g_press[64];
static int g_npress;

static uint32_t btn_named(const char* s, int n) {
    static const struct { const char* name; uint32_t b; } B[] = {
        {"A", BTN_A}, {"B", BTN_B}, {"X", BTN_X}, {"Y", BTN_Y}, {"L", BTN_L}, {"R", BTN_R}, {"Start", BTN_START},
        {"Select", BTN_SELECT}, {"Up", BTN_UP}, {"Down", BTN_DOWN}, {"Left", BTN_LEFT}, {"Right", BTN_RIGHT},
    };
    for (size_t i = 0; i < sizeof B / sizeof B[0]; i++) if ((int)strlen(B[i].name) == n && !strncmp(B[i].name, s, n)) return B[i].b;
    return 0;
}

/* "A@10", "Left@5-20" (a button) or "3@100,50", "3-6@100,50" (the touch); 0 = wrong */
static int add_press(const char* s, int touch) {
    if (g_npress == 64) return 0;
    Press* p = &g_press[g_npress];
    memset(p, 0, sizeof *p);
    const char* at = strchr(s, '@');
    if (!at) return 0;
    if (touch) {
        if (sscanf(s, "%d-%d@", &p->from, &p->to) != 2) { if (sscanf(s, "%d@", &p->from) != 1) return 0; p->to = p->from; }
        if (sscanf(at + 1, "%d,%d", &p->x, &p->y) != 2) return 0;
        p->btn = 0;
    } else {
        if (!(p->btn = btn_named(s, (int)(at - s)))) return 0;
        if (sscanf(at + 1, "%d-%d", &p->from, &p->to) != 2) { if (sscanf(at + 1, "%d", &p->from) != 1) return 0; p->to = p->from; }
    }
    g_npress++;
    return 1;
}

static void input_for_frame(int f) {
    uint32_t held = 0;
    int touching = 0, x = 0, y = 0;
    for (int i = 0; i < g_npress; i++) {
        if (f < g_press[i].from || f > g_press[i].to) continue;
        if (g_press[i].btn) held |= g_press[i].btn;
        else { touching = 1; x = g_press[i].x; y = g_press[i].y; }
    }
    lib_input_frame(held, touching, x, y);
}

static int g_echo;
static TextGrid* grid;                  /* --grid WxH */
static void no_output(const char* s) { (void)s; }                      /* --echo: print each line read by Console.ReadLine */

/* vm_call; Console.ReadLine gets its lines from stdin until the call ends */
static int call(VM* vm, int func) {
    fflush(stdout);
    int r = vm_call(vm, func);
    while (r == MS_WAIT_INPUT) {
        fflush(stdout);
        char* line = read_line();
        if (g_echo && line) printf("%s\n", line);      /* like a console shows what the user typed */
        r = vm_resume_input(vm, line);
        free(line);
    }
    return r;
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
        if (repl_waiting(r)) {                  /* the answer to Console.ReadLine, shown after the question */
            printf("%s\n", line);
            if (!repl_answer(r, line, err, sizeof err)) printf("error: %s\n", err);
            fflush(stdout);
            continue;
        }
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
        if (g_stop) { g_stop = 0; signal(SIGINT, on_ctrl_c); }     /* Ctrl+C stopped this input, not the terminal */
        fflush(stdout);
    }
    if (tlen) printf("error: the input ended with an open '{' or '('\n");
    repl_free(r);
    return 0;
}

/* editor suggestions test: at every marker comment in the project's files, what the editor would offer there */
static int ci_main(const char* dir) {
    Project pr;
    char err[300];
    if (!project_load(dir, &pr, err, sizeof err)) { fprintf(stderr, "%s\n", err); return 2; }
    const char** texts = (const char**)calloc(pr.nfiles, sizeof *texts);
    char** own = (char**)calloc(pr.nfiles, sizeof *own);
    if (!texts || !own) { fprintf(stderr, "out of memory\n"); return 2; }
    for (int i = 0; i < pr.nfiles; i++) {
        texts[i] = own[i] = read_file(pr.files[i].path);
        if (!own[i]) { fprintf(stderr, "cannot read %s\n", pr.files[i].path); return 2; }
    }
    CodeIndex ci = { 0 };
    ci_build(&ci, texts, pr.nfiles);
    for (int t = 0; t < pr.nfiles; t++)
        for (const char* m = texts[t]; (m = strstr(m, "/*?*/")); m += 5) {
            int pos = (int)(m - texts[t]), line = 1;
            for (const char* q = texts[t]; q < m; q++) line += *q == '\n';
            char out[8][CI_WORD], type[48] = "?";
            int start, n = ci_complete(&ci, t, pos, &start, out, 8);
            if (start > 0 && texts[t][start - 1] == '.') ci_type_before(&ci, t, start - 1, type, sizeof type);
            printf("%s:%d: %s ->", pr.files[t].name, line, type);
            for (int i = 0; i < n; i++) printf(" %s", out[i]);
            if (n < 0) printf(" (none)");
            printf("\n");
        }
    ci_free(&ci);
    for (int i = 0; i < pr.nfiles; i++) free(own[i]);
    free(own); free(texts);
    project_free(&pr);
    return 0;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY); /* "\n" not "\r\n", so output matches tests/expected */
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    signal(SIGINT, on_ctrl_c);
    ms_set_interrupt(stop_pressed);
    if (argc == 2 && !strcmp(argv[1], "--repl")) {
        ms_set_clock(frame_millis);
        lib_register_all();
        return repl_main();
    }
    if (argc == 3 && !strcmp(argv[1], "--ci")) {
        lib_register_all();
        return ci_main(argv[2]);
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
    int frames = 60, check = 0, dis = 0, hash = 0;
    const char* ppm = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--check")) check = 1;
        else if (!strcmp(argv[i], "--dis")) dis = 1;
        else if (!strcmp(argv[i], "--echo")) g_echo = 1;
        else if ((!strcmp(argv[i], "--press") || !strcmp(argv[i], "--touch")) && i + 1 < argc) {
            int touch = !strcmp(argv[i], "--touch");
            if (!add_press(argv[++i], touch)) { fprintf(stderr, "bad %s %s (e.g. %s)\n", touch ? "--touch" : "--press", argv[i], touch ? "3@100,50" : "A@10 or Left@5-20"); return 2; }
        }
        else if (!strcmp(argv[i], "--hash")) hash = 1;
        else if (!strcmp(argv[i], "--ppm") && i + 1 < argc) ppm = argv[++i];
        else if (!strcmp(argv[i], "--grid") && i + 1 < argc) {
            int gw = 0, gh = 0;
            if (sscanf(argv[++i], "%dx%d", &gw, &gh) != 2 || !(grid = tg_new(gw, gh))) { fprintf(stderr, "bad --grid %s (e.g. 50x30)\n", argv[i]); return 2; }
        }
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
    if (mi < 0 && up < 0 && dr < 0) {
        fprintf(stderr, "compile error: line 1: no entry point: add 'static void Main()', 'Update()' or 'Draw()'\n");
        ms_free_program(p); project_free(&proj);
        return 1;
    }
    if (dis) ms_disassemble(p);
    if (check) { ms_free_program(p); project_free(&proj); return 0; }

    if (grid) { lib_console_set_grid(grid); lib_set_output(no_output); }
    const GfxBackend* screens = swgfx_init();
    lib_set_gfx(screens);
    lib_gfx_reset();
    {                                               /* Gfx.LoadImage("x.png"): next to the program (or in the project) */
        char imgDir[512];
        snprintf(imgDir, sizeof imgDir, "%s", isProject ? dir : argv[1]);
        char* slash = isProject ? NULL : strrchr(imgDir, '/');
        if (!isProject) { if (slash) *slash = 0; else snprintf(imgDir, sizeof imgDir, "."); }
        lib_gfx_set_dir(imgDir);
    }
    VM* vm = vm_new(p);
    int ok = si < 0 || call(vm, si);
    if (ok && mi >= 0) ok = call(vm, mi);
    if (ok && (up >= 0 || dr >= 0)) {           /* the frames: Update and / or Draw */
        for (int f = 0; f < frames && ok; f++) {
            g_ms += 1000.0 / 60;
            input_for_frame(f);
            swgfx_begin_frame();
            ok = up < 0 || call(vm, up);
            if (ok) ok = vm_tick_coroutines(vm);    /* after Update, before Draw, like Unity */
            if (ok && dr >= 0) ok = call(vm, dr);
            lib_time_frame(1.0f / 60);
        }
    }
    if (hash) printf("screens: %08x\n", swgfx_hash());
    if (ppm && !swgfx_save_ppm(ppm)) fprintf(stderr, "cannot write %s\n", ppm);
    lib_set_gfx(NULL);
    swgfx_free();
    int rc = 0;
    if (grid) {                                     /* the grid's rows, without the empty ones at the bottom */
        int last = tg_height(grid) - 1;
        char row[4096];
        while (last >= 0 && tg_row_text(grid, last, row, sizeof row) == 0) last--;
        for (int y = 0; y <= last; y++) { tg_row_text(grid, y, row, sizeof row); printf("%s\n", row); }
        lib_console_set_grid(NULL); lib_set_output(NULL);
        tg_free(grid);
    }
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
