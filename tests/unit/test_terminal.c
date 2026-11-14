/* Unit test: the terminal of the native app (app/terminal.c) – typing, running, errors, input over several lines,
 * history, suggestions, wrapping and the scrollback, buttons, drawing through a recording ui.h backend.
 * Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "terminal.h"
#include "keyboard.h"
#include "lib.h"
#include "ms.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

/* --- recording backend: two screens like the 3DS --- */
static char drawn[8192];
static int curScreen, textsOnScreen[2];
static void b_select(int s) { curScreen = s; }
static void b_rect(float x, float y, float w, float h, uint32_t rgb) { (void)x; (void)y; (void)w; (void)h; (void)rgb; }
static void b_text(float x, float y, const char* s, float size, uint32_t rgb) {
    (void)x; (void)y; (void)size; (void)rgb;
    textsOnScreen[curScreen]++;
    strncat(drawn, s, sizeof drawn - strlen(drawn) - 2);
    strcat(drawn, "|");
}
static float b_width(const char* s, float size) { return strlen(s) * size / 2; }
static const UiBackend REC = { 2, 1, { 400, 320 }, { 240, 240 }, b_select, b_rect, b_text, b_width, NULL };

/* --- helpers --- */
static uint32_t now = 1000;

static void type(Terminal* t, const char* s) {
    char c[2] = { 0, 0 };
    for (; *s; s++) { c[0] = *s; term_key(t, c); }
}
static void enter(Terminal* t, const char* s) { type(t, s); term_key(t, KBD_ENTER); }

static void press(Terminal* t, uint32_t btn) {
    UiInput in = { 0 };
    in.down = in.held = btn; in.ms = now += 50;
    term_update(t, &in);
    in.down = 0; in.held = 0; in.ms = now += 50;
    term_update(t, &in);
}

static const char* last(Terminal* t, int back) { return term_line(t, term_lines(t) - 1 - back); }

static void test_run(void) {
    Terminal* t = term_new();
    check(t != NULL, "term_new");
    check(term_lines(t) == 4 && strstr(term_line(t, 0), "terminal"), "welcome text");

    enter(t, "1 + 2");
    check(!strcmp(last(t, 1), "> 1 + 2") && !strcmp(last(t, 0), "3"), "expression: echo and value");
    enter(t, "var x = 5");
    enter(t, "x * 10");
    check(!strcmp(last(t, 0), "50"), "variables stay between lines");
    enter(t, "Console.Write(\"a\"); Console.Write(\"b\");");
    check(!strcmp(last(t, 0), "ab"), "Console.Write goes to the terminal");
    enter(t, "Console.Write(\"c\");");
    check(!strcmp(last(t, 1), "> Console.Write(\"c\");") && !strcmp(last(t, 0), "c"),
          "an open output line is closed before the next input");
    enter(t, "y + 1");
    check(!strncmp(last(t, 0), "error: ", 7) && strstr(last(t, 0), "y"), "error line");

    /* an input over several lines */
    enter(t, "for (int i = 0; i < 3; i++) {");
    check(term_continuing(t), "open '{' waits for more");
    enter(t, "Console.Write(i);");
    enter(t, "}");
    check(!term_continuing(t), "closed");
    check(!strcmp(last(t, 0), "012") && !strcmp(last(t, 1), ". }") && !strcmp(last(t, 2), ". Console.Write(i);"),
          "the loop ran once, continuation lines echoed with '.'");

    /* an empty Enter: only the prompt */
    int n = term_lines(t);
    term_key(t, KBD_ENTER);
    check(term_lines(t) == n + 1 && !strcmp(last(t, 0), "> "), "empty line");

    /* while (true) would hang: the Stop button (here: always pressed) */
    term_free(t);
}

static int always(void) { return 1; }

static void test_stop(void) {
    Terminal* t = term_new();
    ms_set_interrupt(always);
    enter(t, "while (true) { }");
    check(strstr(last(t, 0), "stopped by the user") != NULL, "Stop ends an endless loop");
    ms_set_interrupt(NULL);
    enter(t, "2 * 21");
    check(!strcmp(last(t, 0), "42"), "the terminal works after Stop");
    term_free(t);
}

static void test_editing(void) {
    Terminal* t = term_new();
    type(t, "abc");
    term_key(t, KBD_BACKSPACE);
    check(!strcmp(term_input(t), "ab") && term_cursor(t) == 2, "backspace");
    press(t, UI_BTN_LEFT);
    type(t, "X");
    check(!strcmp(term_input(t), "aXb"), "Left moves the cursor, typing inserts");
    press(t, UI_BTN_B);
    check(!strcmp(term_input(t), "ab"), "B deletes");

    /* B held repeats */
    UiInput in = { 0 };
    press(t, UI_BTN_RIGHT);                           /* the cursor was before 'b' */
    type(t, "cdefgh");
    in.down = in.held = UI_BTN_B; in.ms = now += 10;
    term_update(t, &in);
    in.down = 0;
    for (int i = 0; i < 50; i++) { in.ms = now += 10; term_update(t, &in); }   /* 500 ms: 1 + 2 repeats */
    in.held = 0; term_update(t, &in);
    check(!strcmp(term_input(t), "abcde"), "B held repeats");
    press(t, UI_BTN_A);                               /* runs "abcde": an error, but the line is in the history */
    check(!strcmp(term_input(t), ""), "A runs and clears the line");

    enter(t, "var n = 1");
    enter(t, "n + 1");
    press(t, UI_BTN_UP);
    check(!strcmp(term_input(t), "n + 1"), "Up: last line");
    press(t, UI_BTN_UP);
    check(!strcmp(term_input(t), "var n = 1"), "Up again: the one before");
    press(t, UI_BTN_DOWN);
    press(t, UI_BTN_DOWN);
    check(!strcmp(term_input(t), ""), "Down back to the new line");
    type(t, "dra");
    press(t, UI_BTN_UP);
    press(t, UI_BTN_DOWN);
    check(!strcmp(term_input(t), "dra"), "the unfinished line comes back");
    while (*term_input(t)) term_key(t, KBD_BACKSPACE);

    /* suggestions: R takes the first one */
    const char* sug[TERM_NSUGGEST];
    type(t, "Conso");
    int ns = term_suggestions(t, sug);
    check(ns >= 1 && !strcmp(sug[0], "Console"), "suggestion for 'Conso'");
    press(t, UI_BTN_R);
    check(!strcmp(term_input(t), "Console"), "R completes");
    type(t, ".WriteL");
    ns = term_suggestions(t, sug);
    check(ns >= 1 && !strcmp(sug[0], "WriteLine"), "member suggestion after the dot");

    /* Y clears the screen */
    press(t, UI_BTN_Y);
    check(term_lines(t) == 0, "Y clears");
    UiInput st = { 0 };
    st.down = UI_BTN_START;
    check(term_update(t, &st) == 0, "START leaves");
    term_free(t);
}

static void test_wrap(void) {
    Terminal* t = term_new();
    char longs[200];
    memset(longs, 'x', 130); longs[130] = 0;
    int n0 = term_lines(t);
    term_print(t, longs);
    term_print(t, "\n");
    check(term_lines(t) == n0 + 3, "130 characters = 3 lines of 56");
    check(strlen(last(t, 0)) == 130 - 2 * TERM_COLS, "the rest on the last line");
    /* Polish letters are 2 bytes in UTF-8 but one character */
    char pl[400] = "";
    for (int i = 0; i < TERM_COLS; i++) strcat(pl, "\xC4\x99");     /* "ę" */
    n0 = term_lines(t);
    term_print(t, pl);
    term_print(t, "\n");
    check(term_lines(t) == n0 + 1, "56 Polish letters fit on one line");
    term_print(t, "\n\n");
    check(!strcmp(last(t, 0), "") && !strcmp(last(t, 1), ""), "empty lines");
    for (int i = 0; i < TERM_SCROLLBACK + 50; i++) { char b[32]; snprintf(b, sizeof b, "line %d\n", i); term_print(t, b); }
    check(term_lines(t) == TERM_SCROLLBACK, "scrollback is limited");
    check(!strcmp(last(t, 0), "line 249"), "the newest line is kept");
    term_free(t);
}

static void test_draw(void) {
    ui_set_backend(&REC);
    Terminal* t = term_new();
    enter(t, "6 * 7");
    type(t, "Mat");
    drawn[0] = 0; textsOnScreen[0] = textsOnScreen[1] = 0;
    term_draw(t);
    check(strstr(drawn, "|42|") != NULL, "output drawn");
    check(strstr(drawn, "|Mat|") != NULL, "input line drawn");
    check(strstr(drawn, "|Math|") != NULL, "suggestion drawn");
    check(strstr(drawn, "|Run|") != NULL, "keyboard drawn with Run");
    check(textsOnScreen[0] > 0 && textsOnScreen[1] > 0, "both screens used");

    /* tapping a suggestion completes the word */
    UiInput in = { 0 };
    in.touching = in.touchDown = 1; in.x = 10; in.y = 60; in.ms = now += 50;
    term_update(t, &in);
    check(!strcmp(term_input(t), "Math"), "tap on the suggestion");

    /* one screen (e.g. a phone or the Switch): the panel goes to the bottom of it */
    static const UiBackend ONE = { 1, 0, { 640, 0 }, { 480, 0 }, b_select, b_rect, b_text, b_width, NULL };
    ui_set_backend(&ONE);
    drawn[0] = 0; textsOnScreen[0] = textsOnScreen[1] = 0;
    term_draw(t);
    check(textsOnScreen[1] == 0 && strstr(drawn, "|42|") && strstr(drawn, "|Run|"), "one screen layout");
    in.ms = now += 50; in.touching = in.touchDown = 0;
    term_update(t, &in);
    float x, y;
    check(kbd_key_center("q", &x, &y) && y > 480 - 160 && x < 640 / 10, "keyboard at the bottom, full width");
    ui_set_backend(NULL);
    term_free(t);
}

int main(void) {
    lib_register_all();
    test_run();
    test_stop();
    test_editing();
    test_wrap();
    test_draw();
    if (!fails) printf("terminal: ok\n");
    return fails ? 1 : 0;
}
