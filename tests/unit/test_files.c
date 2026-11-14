/* Unit test: the file list (app/files.c) – listing the root (projects first, no "tests"), entering a project and
 * back, opening a file, a new file and a new project through the keyboard path, wrong names, taps, drawing,
 * files_project_of. Works in build/tmp/files_root. Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "files.h"
#include "keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0777)
#endif

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

static char drawn[8192];
static int curScreen, textsOn[2];
static void b_select(int s) { curScreen = s; }
static void b_rect(float x, float y, float w, float h, uint32_t rgb) { (void)x; (void)y; (void)w; (void)h; (void)rgb; }
static void b_text(float x, float y, const char* s, float size, uint32_t rgb) {
    (void)x; (void)y; (void)size; (void)rgb;
    textsOn[curScreen]++;
    strncat(drawn, s, sizeof drawn - strlen(drawn) - 2);
    strcat(drawn, "|");
}
static float b_width(const char* s, float size) { return strlen(s) * size / 2; }
static const UiBackend REC = { 2, 1, { 400, 320 }, { 240, 240 }, b_select, b_rect, b_text, b_width, NULL };

static uint32_t now = 1000;
static int frame(Files* fs, uint32_t down) {
    UiInput in = { 0 };
    in.down = in.held = down; in.ms = now += 20;
    int r = files_update(fs, &in);
    in.down = in.held = 0; in.ms = now += 20;
    if (r == FILES_STAY) r = files_update(fs, &in);
    return r;
}
static int tap(Files* fs, int x, int y) {
    UiInput in = { 0 };
    in.touching = in.touchDown = 1; in.x = x; in.y = y; in.ms = now += 20;
    return files_update(fs, &in);
}
static void type(Files* fs, const char* s) {
    char c[2] = { 0, 0 };
    for (; *s; s++) { c[0] = *s; files_key(fs, c); }
}
static void touch_file(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}
static char* read_all(const char* path) {
    static char buf[1024];
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}
static int find(Files* fs, const char* name) {
    for (int i = 0; i < files_count(fs); i++) if (!strcmp(files_name(fs, i), name)) return i;
    return -1;
}

#define ROOT "build/tmp/files_root"

int main(void) {
    ui_set_backend(&REC);
    /* a fresh root: two programs, a project, the tests folder and the log */
    remove(ROOT "/Game/Program.cs"); remove(ROOT "/Game/Player.cs"); remove(ROOT "/Game/uses.txt"); remove(ROOT "/Game/Enemy.cs");
    remove(ROOT "/Nowy/Program.cs"); rmdir(ROOT "/Nowy"); rmdir(ROOT "/Game");
    remove(ROOT "/tests/a.cs"); rmdir(ROOT "/tests");
    remove(ROOT "/main.cs"); remove(ROOT "/beta.cs"); remove(ROOT "/Hello.cs"); remove(ROOT "/log.txt");
    rmdir(ROOT);
    make_dir(ROOT); make_dir(ROOT "/Game"); make_dir(ROOT "/tests");
    touch_file(ROOT "/main.cs", "class P {}\n");
    touch_file(ROOT "/beta.cs", "class B {}\n");
    touch_file(ROOT "/log.txt", "log\n");
    touch_file(ROOT "/tests/a.cs", "class T {}\n");
    touch_file(ROOT "/Game/Program.cs", "class Program {}\n");
    touch_file(ROOT "/Game/Player.cs", "class Player {}\n");
    touch_file(ROOT "/Game/uses.txt", "../lib\n");

    Files* fs = files_new(ROOT);
    check(fs != NULL, "files_new");
    check(files_count(fs) == 3, "root: 3 entries (no tests/, no log.txt)");
    check(!strcmp(files_name(fs, 0), "Game/") && !strcmp(files_name(fs, 1), "beta.cs") && !strcmp(files_name(fs, 2), "main.cs"),
          "projects first, then files by name");

    /* D-pad and A on a file */
    frame(fs, UI_BTN_DOWN); frame(fs, UI_BTN_DOWN); frame(fs, UI_BTN_DOWN);
    check(files_selected(fs) == 2, "Down stops at the last entry");
    check(frame(fs, UI_BTN_A) == FILES_OPEN && !strcmp(files_chosen(fs), ROOT "/main.cs"), "A opens main.cs");

    /* into the project and back */
    frame(fs, UI_BTN_UP); frame(fs, UI_BTN_UP);
    check(frame(fs, UI_BTN_A) == FILES_STAY && !strcmp(files_dir(fs), ROOT "/Game"), "A on a project enters it");
    check(files_count(fs) == 3 && !strcmp(files_name(fs, 0), "Player.cs") && !strcmp(files_name(fs, 2), "uses.txt"),
          "project: .cs files, then uses.txt");
    check(frame(fs, UI_BTN_B) == FILES_STAY && !strcmp(files_dir(fs), ROOT) && files_selected(fs) == 0, "B goes back to the root, on the project");
    check(frame(fs, UI_BTN_B) == FILES_LEAVE, "B in the root leaves");

    /* taps: the first selects, the second opens; the tool row */
    int r = tap(fs, 50, 22 + 2 + 24 + 5);                                    /* second row: beta.cs */
    check(r == FILES_STAY && files_selected(fs) == 1, "a tap selects");
    r = tap(fs, 50, 22 + 2 + 24 + 5);
    check(r == FILES_OPEN && !strcmp(files_chosen(fs), ROOT "/beta.cs"), "a second tap opens");

    /* a new single program: X, a name on the keyboard, Enter */
    frame(fs, UI_BTN_X);
    check(files_naming(fs), "X asks for a name");
    drawn[0] = 0;
    files_draw(fs);
    check(strstr(drawn, "Create|") != NULL && strstr(drawn, "class name") != NULL, "naming: Create button and the prompt");
    type(fs, "9x");
    files_key(fs, KBD_ENTER);
    check(files_naming(fs) && strstr(files_status(fs), "letter"), "a name starting with a digit is refused");
    files_key(fs, KBD_BACKSPACE); files_key(fs, KBD_BACKSPACE);
    type(fs, "main");
    files_key(fs, KBD_ENTER);
    check(files_naming(fs) && strstr(files_status(fs), "already"), "main.cs exists already");
    while (files_naming(fs)) frame(fs, UI_BTN_B);
    check(strstr(files_status(fs), "cancelled") != NULL, "B cancels naming");
    frame(fs, UI_BTN_X);
    type(fs, "Hello");
    UiInput in = { 0 };
    in.down = in.held = UI_BTN_A; in.ms = now += 20;
    check(files_update(fs, &in) == FILES_OPEN && !strcmp(files_chosen(fs), ROOT "/Hello.cs"), "A creates Hello.cs and opens it");
    const char* t = read_all(ROOT "/Hello.cs");
    check(t && strstr(t, "class Hello") && strstr(t, "static void Main()"), "a single program gets Main");
    check(find(fs, "Hello.cs") == files_selected(fs), "the new file is selected");

    /* a new file in a project: an empty class */
    frame(fs, UI_BTN_UP); while (files_selected(fs) > 0) frame(fs, UI_BTN_UP);
    frame(fs, UI_BTN_A);
    frame(fs, UI_BTN_Y);
    check(!files_naming(fs) && strstr(files_status(fs), "another project"), "no project inside a project");
    frame(fs, UI_BTN_X);
    type(fs, "Enemy");
    files_key(fs, KBD_ENTER);
    t = read_all(ROOT "/Game/Enemy.cs");
    check(t && !strcmp(t, "class Enemy\n{\n}\n"), "a project file gets an empty class");
    check(files_count(fs) == 4, "the list is read again");

    /* a new project from the root */
    frame(fs, UI_BTN_B);
    frame(fs, UI_BTN_Y);
    type(fs, "Nowy");
    files_key(fs, KBD_ENTER);
    check(!strcmp(files_chosen(fs), ROOT "/Nowy/Program.cs") && !strcmp(files_dir(fs), ROOT "/Nowy"), "Y creates a project and opens Program.cs");
    t = read_all(ROOT "/Nowy/Program.cs");
    check(t && strstr(t, "class Program") && strstr(t, "Main"), "the project's Program.cs has Main");

    /* drawing: list on the touch screen, hints on the other */
    frame(fs, UI_BTN_B);
    drawn[0] = 0; textsOn[0] = textsOn[1] = 0;
    files_draw(fs);
    check(strstr(drawn, "|Game/|") && strstr(drawn, "|Nowy/|") && strstr(drawn, "New project|"), "list and tools drawn");
    check(textsOn[0] > 0 && textsOn[1] > 0, "both screens used");

    /* which project a file belongs to */
    char dir[300];
    check(files_project_of(ROOT, ROOT "/Game/Player.cs", dir, sizeof dir) && !strcmp(dir, ROOT "/Game"), "project_of: a project file");
    check(!files_project_of(ROOT, ROOT "/main.cs", dir, sizeof dir), "project_of: a single program");
    check(!files_project_of(ROOT, "other/x.cs", dir, sizeof dir), "project_of: outside the root");

    /* deleting: SELECT asks, a second SELECT deletes; another button cancels; a project only when empty */
    int hi = find(fs, "Hello.cs");
    while (files_selected(fs) < hi) frame(fs, UI_BTN_DOWN);
    frame(fs, UI_BTN_SELECT);
    check(strstr(files_status(fs), "SELECT again") && read_all(ROOT "/Hello.cs"), "SELECT asks first");
    frame(fs, UI_BTN_UP);
    check(!strstr(files_status(fs), "SELECT again"), "moving cancels the delete");
    frame(fs, UI_BTN_DOWN);
    frame(fs, UI_BTN_SELECT);
    frame(fs, UI_BTN_SELECT);
    check(!read_all(ROOT "/Hello.cs") && find(fs, "Hello.cs") < 0 && strstr(files_status(fs), "deleted"), "SELECT twice deletes the file");
    while (files_selected(fs) > 0) frame(fs, UI_BTN_UP);
    while (strcmp(files_name(fs, files_selected(fs)), "Nowy/")) frame(fs, UI_BTN_DOWN);
    frame(fs, UI_BTN_SELECT); frame(fs, UI_BTN_SELECT);
    check(find(fs, "Nowy/") >= 0 && strstr(files_status(fs), "not empty"), "a project with files is not deleted");
    frame(fs, UI_BTN_A);
    frame(fs, UI_BTN_SELECT); frame(fs, UI_BTN_SELECT);
    check(files_count(fs) == 0 && !read_all(ROOT "/Nowy/Program.cs"), "the project's last file deleted");
    frame(fs, UI_BTN_B);
    frame(fs, UI_BTN_SELECT); frame(fs, UI_BTN_SELECT);
    check(find(fs, "Nowy/") < 0, "an empty project folder is deleted");
    /* the Delete tool: the same two steps by touch */
    touch_file(ROOT "/Usun.cs", "class U {}\n");
    files_scan(fs);
    while (strcmp(files_name(fs, files_selected(fs)), "Usun.cs")) frame(fs, UI_BTN_DOWN);
    tap(fs, 3 * 80 + 10, 5);
    drawn[0] = 0;
    files_draw(fs);
    check(read_all(ROOT "/Usun.cs") && strstr(drawn, "Delete?|"), "the first tap on Delete asks");
    tap(fs, 3 * 80 + 10, 5);
    check(!read_all(ROOT "/Usun.cs"), "the second tap deletes");

    check(frame(fs, UI_BTN_START) == FILES_LEAVE, "START leaves");
    files_free(fs);
    ui_set_backend(NULL);
    if (!fails) printf("files: ok\n");
    return fails ? 1 : 0;
}
