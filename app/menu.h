/* MiniSharp - the main menu of the native app: big tiles on the touch screen (Programs, Run main.cs, Terminal, Help,
 * Tests, Quit) and the recently opened files below them; the title, the build, what the chosen tile does and a status
 * line (e.g. the result of the tests) on the other screen.
 * Input: tap = start a tile or open a recent file; D-pad moves, A starts the chosen one; shortcuts B Programs,
 * Y Terminal, R Help, X Tests, START Quit. The menu only says what was chosen: the host starts it (each program then
 * gets a fresh VM) and comes back. Draws only through app/ui.h; tested on the PC (tests/unit/test_menu.c). */
#ifndef MS_MENU_H
#define MS_MENU_H

#include "ui.h"

enum {
    MENU_STAY, MENU_PROGRAMS, MENU_RUN_MAIN, MENU_TERMINAL, MENU_HELP, MENU_TESTS, MENU_QUIT,
    MENU_OPEN_RECENT        /* menu_chosen(): the file to open in the editor */
};

#define MENU_TILES 6
#define MENU_RECENT_MAX 5   /* kept in the file */
#define MENU_RECENT_SHOWN 3 /* shown below the tiles */

typedef struct Menu Menu;

/* root: the app folder (paths below it are shown short); recentFile: where the recent files are kept (NULL = none) */
Menu* menu_new(const char* root, const char* recentFile);
void menu_free(Menu* m);
void menu_set_build(Menu* m, const char* build);           /* "build 12 / 3acff8a, 2026-11-14" */
void menu_set_status(Menu* m, const char* text, int error); /* the line at the bottom of the info screen */
/* a file was opened in the editor: it goes to the top of the recent list (saved at once) */
void menu_add_recent(Menu* m, const char* path);

int menu_update(Menu* m, const UiInput* in);    /* MENU_STAY or what to start */
void menu_draw(Menu* m);
const char* menu_chosen(const Menu* m);         /* after MENU_OPEN_RECENT */

/* for tests */
int menu_selected(const Menu* m);               /* 0..MENU_TILES-1 a tile, MENU_TILES + i the recent file i */
int menu_recent_count(const Menu* m);
const char* menu_recent(const Menu* m, int i);
const char* menu_status(const Menu* m);

#endif
