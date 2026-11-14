/* Unit test: the console screen (app/conscreen.c) – a real program runs with its Console in the screen's grid,
 * Console.ReadLine asks through the screen (typing on the keyboard path, Enter, the echo in the grid, backspace,
 * START = end of input), the finished screen with an error in red, drawing on two screens and on one.
 * Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "conscreen.h"
#include "keyboard.h"
#include "lib.h"
#include "ms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

static char drawn[32768];
static int curScreen, textsOn[2], redTexts;
static void b_select(int s) { curScreen = s; }
static void b_rect(float x, float y, float w, float h, uint32_t rgb) { (void)x; (void)y; (void)w; (void)h; (void)rgb; }
static void b_text(float x, float y, const char* s, float size, uint32_t rgb) {
    (void)x; (void)y; (void)size;
    textsOn[curScreen]++;
    if (rgb == 0xFF0000) redTexts++;
    strncat(drawn, s, sizeof drawn - strlen(drawn) - 2);
    strcat(drawn, "|");
}
static float b_width(const char* s, float size) { return strlen(s) * size / 2; }
static const UiBackend REC = { 2, 1, { 400, 320 }, { 240, 240 }, b_select, b_rect, b_text, b_width, NULL };
static const UiBackend ONE = { 1, 0, { 640, 0 }, { 480, 0 }, b_select, b_rect, b_text, b_width, NULL };

static void no_output(const char* s) { (void)s; }

static const char* row(ConScreen* cs, int y) {
    static char buf[256];
    tg_row_text(cs_grid(cs), y, buf, sizeof buf);
    return buf;
}

static uint32_t now = 1000;
static int frame(ConScreen* cs, uint32_t down) {
    UiInput in = { 0 };
    in.down = in.held = down; in.ms = now += 20;
    return cs_update(cs, &in);
}

/* compiles and runs src with the screen as its console, answering ReadLine with the given lines (NULL = START) */
static int run(ConScreen* cs, const char* src, const char* const* answers, int nans, char* err, int errcap) {
    Program* p = ms_compile(src, err, errcap);
    if (!p) return -1;
    VM* vm = vm_new(p);
    int r = vm_call(vm, ms_entry(p, MS_ENTRY_MAIN)), a = 0;
    while (r == MS_WAIT_INPUT) {
        cs_ask(cs);
        check(cs_state(cs) == CS_ASKING, "asking while the VM waits");
        const char* ans = a < nans ? answers[a++] : NULL;
        int ev;
        if (ans) {
            char c[2] = { 0, 0 };
            for (const char* s = ans; *s; s++) { c[0] = *s; cs_key(cs, c); }
            ev = frame(cs, UI_BTN_A);
        } else ev = frame(cs, UI_BTN_START);
        r = vm_resume_input(vm, ev == CS_LINE ? cs_line(cs) : NULL);
    }
    if (!r) snprintf(err, errcap, "runtime error: %s", ms_vm_error(vm));
    vm_free(vm);
    ms_free_program(p);
    return r;
}

int main(void) {
    lib_register_all();
    ui_set_backend(&REC);
    ConScreen* cs = cs_new(40, 20);
    check(cs && tg_width(cs_grid(cs)) == 40 && cs_state(cs) == CS_RUNNING, "cs_new");
    lib_console_set_grid(cs_grid(cs));
    lib_set_output(no_output);

    /* a program that asks twice */
    const char* src =
        "class P { static void Main() {\n"
        "  Console.ForegroundColor = ConsoleColor.Yellow;\n"
        "  Console.Write(\"Imię? \");\n"
        "  string a = Console.ReadLine();\n"
        "  Console.ResetColor();\n"
        "  Console.Write(\"Wiek? \");\n"
        "  int w = int.Parse(Console.ReadLine());\n"
        "  Console.WriteLine($\"{a} ma {w + 1} lat za rok\");\n"
        "  string k = Console.ReadLine();\n"
        "  Console.WriteLine(k == null ? \"koniec wejścia\" : k);\n"
        "} }\n";
    const char* answers[] = { "Ola", "12" };
    char err[300] = "";
    int r = run(cs, src, answers, 2, err, sizeof err);
    check(r == 1, "the program ran");
    check(!strcmp(row(cs, 0), "Imię? Ola") && !strcmp(row(cs, 1), "Wiek? 12"), "the answers are echoed in the grid after the questions");
    check(!strcmp(row(cs, 2), "Ola ma 13 lat za rok") && !strcmp(row(cs, 4), "koniec wejścia"), "the output, and START gives null");
    check(tg_cell(cs_grid(cs), 0, 0)->fg == TG_YELLOW && tg_cell(cs_grid(cs), 0, 1)->fg == TG_GRAY, "the colours of the cells");

    /* typing: backspace removes a whole Polish letter, B too; the line is drawn while typing */
    cs_ask(cs);
    cs_key(cs, "a"); cs_key(cs, "ż"); cs_key(cs, KBD_BACKSPACE);
    frame(cs, UI_BTN_B);
    cs_key(cs, "x"); cs_key(cs, "y");
    drawn[0] = 0; textsOn[0] = textsOn[1] = 0;
    cs_draw(cs);
    check(strstr(drawn, "xy_|") || strstr(drawn, "xy |"), "the typed line on the touch screen");
    check(strstr(drawn, "|xy|") != NULL && textsOn[0] > 0 && strstr(drawn, "Enter") != NULL, "and at the cursor in the grid, with the keyboard");
    check(frame(cs, UI_BTN_A) == CS_LINE && !strcmp(cs_line(cs), "xy") && cs_state(cs) == CS_RUNNING, "A gives the line");
    check(frame(cs, UI_BTN_A) == CS_NONE, "running: A does nothing");

    /* the end with an error */
    cs_finish(cs, 0, "runtime error: line 3 (P.Main): division by zero");
    check(cs_state(cs) == CS_FINISHED, "finished");
    drawn[0] = 0; redTexts = 0;
    cs_draw(cs);
    check(redTexts > 0 && strstr(drawn, "division") && strstr(drawn, "A / B: back"), "the error in red in the grid and in the panel");
    check(frame(cs, 0) == CS_NONE && frame(cs, UI_BTN_B) == CS_LEAVE, "B leaves the finished screen");

    /* a compile error is the host's business; Clear and the cursor still work in the screen's grid */
    r = run(cs, "class P { static void Main() { Console.Clear(); Console.SetCursorPosition(5, 2); Console.Write(\"@\"); } }",
            NULL, 0, err, sizeof err);
    check(r == 1 && !strcmp(row(cs, 2), "     @") && !strcmp(row(cs, 0), ""), "Clear and SetCursorPosition in the screen's grid");
    cs_finish(cs, 1, NULL);
    drawn[0] = 0;
    cs_draw(cs);
    check(strstr(drawn, "done") != NULL, "finished without an error");

    /* one screen */
    ui_set_backend(&ONE);
    cs_ask(cs);
    drawn[0] = 0; textsOn[0] = 0;
    cs_draw(cs);
    check(textsOn[0] > 0 && strstr(drawn, "@|"), "one screen: the grid and the panel");
    frame(cs, UI_BTN_START);
    check(cs_state(cs) == CS_RUNNING, "START ends the input");

    lib_console_set_grid(NULL);
    lib_set_output(NULL);
    cs_free(cs);
    ui_set_backend(NULL);
    if (!fails) printf("conscreen: ok\n");
    return fails ? 1 : 0;
}
