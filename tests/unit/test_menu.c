/* Unit test: the main menu (app/menu.c) – the shortcuts, D-pad + A, taps on tiles and on recent files, the recent list
 * (order, no duplicates, at most 5, saved and read again, missing files skipped), the status line and drawing.
 * Works in build/tmp/menu_root. Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static int press(Menu* m, uint32_t down) {
    UiInput in = { 0 };
    in.down = in.held = down;
    return menu_update(m, &in);
}
static int tap(Menu* m, int x, int y) {
    UiInput in = { 0 };
    in.touching = in.touchDown = 1; in.x = x; in.y = y;
    return menu_update(m, &in);
}
static void touch_file(const char* path) {
    FILE* f = fopen(path, "wb");
    if (f) { fputs("class P {}\n", f); fclose(f); }
}

#define ROOT "build/tmp/menu_root"
#define RECENT ROOT "/recent.txt"

int main(void) {
    ui_set_backend(&REC);
    make_dir("build/tmp"); make_dir(ROOT); make_dir(ROOT "/Game");
    remove(RECENT);
    const char* files[] = { ROOT "/a.cs", ROOT "/b.cs", ROOT "/c.cs", ROOT "/d.cs", ROOT "/e.cs", ROOT "/f.cs",
                            ROOT "/Game/Program.cs" };
    for (int i = 0; i < 7; i++) touch_file(files[i]);

    Menu* m = menu_new(ROOT, RECENT);
    check(m != NULL, "menu_new");
    check(menu_recent_count(m) == 0, "no recent file at start");

    /* shortcuts */
    check(press(m, UI_BTN_B) == MENU_PROGRAMS, "B = Programs");
    check(press(m, UI_BTN_Y) == MENU_TERMINAL, "Y = Terminal");
    check(press(m, UI_BTN_R) == MENU_HELP, "R = Help");
    check(press(m, UI_BTN_X) == MENU_TESTS, "X = Tests");
    check(press(m, UI_BTN_START) == MENU_QUIT, "START = Quit");

    /* D-pad + A: 3 x 2 tiles, no wrap */
    press(m, UI_BTN_B);                         /* selects Programs (0) */
    check(menu_selected(m) == 0, "a shortcut selects its tile");
    check(press(m, UI_BTN_LEFT) == MENU_STAY && menu_selected(m) == 0, "left at the edge stays");
    press(m, UI_BTN_RIGHT);
    check(menu_selected(m) == 1, "right -> Run main.cs");
    check(press(m, UI_BTN_A) == MENU_RUN_MAIN, "A starts the chosen tile");
    press(m, UI_BTN_DOWN);
    check(menu_selected(m) == 4, "down -> Tests");
    press(m, UI_BTN_DOWN);
    check(menu_selected(m) == 4, "down without recent files stays on the tiles");
    press(m, UI_BTN_UP);
    check(menu_selected(m) == 1, "up -> back to the first row");

    /* taps: the tiles are 3 columns of (320 - 16) / 3 px, 62 px high from y = 4 */
    check(tap(m, 20, 20) == MENU_PROGRAMS, "tap on the first tile");
    check(tap(m, 300, 100) == MENU_QUIT, "tap on the last tile");
    check(tap(m, 160, 139) == MENU_STAY, "tap between the tiles and the list does nothing");

    /* recent files: newest first, no duplicates, at most 5, missing ones dropped when read */
    for (int i = 0; i < 6; i++) menu_add_recent(m, files[i]);
    check(menu_recent_count(m) == MENU_RECENT_MAX, "at most 5 recent files");
    check(!strcmp(menu_recent(m, 0), ROOT "/f.cs") && !strcmp(menu_recent(m, 4), ROOT "/b.cs"), "newest first, oldest out");
    menu_add_recent(m, ROOT "/d.cs");
    check(!strcmp(menu_recent(m, 0), ROOT "/d.cs") && !strcmp(menu_recent(m, 1), ROOT "/f.cs") &&
          !strcmp(menu_recent(m, 2), ROOT "/e.cs") && !strcmp(menu_recent(m, 3), ROOT "/c.cs"),
          "opening a recent file again moves it to the top");
    check(menu_recent_count(m) == 5, "no duplicate");
    menu_add_recent(m, ROOT "/Game/Program.cs");
    menu_free(m);

    remove(ROOT "/f.cs");
    m = menu_new(ROOT, RECENT);
    check(menu_recent_count(m) == 4, "read again: the missing f.cs is skipped");
    check(!strcmp(menu_recent(m, 0), ROOT "/Game/Program.cs"), "read again in order");

    /* the recent rows: tap and D-pad */
    check(tap(m, 100, 160) == MENU_OPEN_RECENT && !strcmp(menu_chosen(m), ROOT "/Game/Program.cs"),
          "tap on the first recent row opens it");
    press(m, UI_BTN_B);
    press(m, UI_BTN_DOWN);
    press(m, UI_BTN_DOWN);
    check(menu_selected(m) == MENU_TILES, "down from the tiles -> the first recent file");
    press(m, UI_BTN_DOWN);
    check(press(m, UI_BTN_A) == MENU_OPEN_RECENT && !strcmp(menu_chosen(m), ROOT "/d.cs"), "A on the second recent file");
    press(m, UI_BTN_DOWN);
    press(m, UI_BTN_DOWN);
    check(menu_selected(m) == MENU_TILES + MENU_RECENT_SHOWN - 1, "only 3 recent rows shown");
    press(m, UI_BTN_UP); press(m, UI_BTN_UP); press(m, UI_BTN_UP);
    check(menu_selected(m) == 3, "up from the first recent row -> the lower row of tiles");

    /* drawing: info on the top screen, tiles and short recent paths on the touch screen */
    menu_set_build(m, "build 7 / abc123");
    menu_set_status(m, "12 tests run", 0);
    check(!strcmp(menu_status(m), "12 tests run"), "status kept");
    drawn[0] = 0; textsOn[0] = textsOn[1] = 0;
    menu_draw(m);
    check(strstr(drawn, "MiniSharp2|") && strstr(drawn, "build 7 / abc123|") && strstr(drawn, "12 tests run|"),
          "title, build and status drawn");
    check(strstr(drawn, "Programs|") && strstr(drawn, "Terminal|") && strstr(drawn, "Quit|"), "tiles drawn");
    check(strstr(drawn, "Game/Program.cs|") && !strstr(drawn, ROOT "/Game"), "recent files shown below the root");
    check(textsOn[0] > 0 && textsOn[1] > 0, "both screens used");
    menu_free(m);

    /* no recent file to keep */
    m = menu_new(ROOT, NULL);
    check(menu_recent_count(m) == 0, "no recent file without a file");
    menu_add_recent(m, ROOT "/a.cs");
    check(menu_recent_count(m) == 1, "kept in memory only");
    menu_free(m);

    if (!fails) printf("test_menu: ok\n");
    return fails ? 1 : 0;
}
