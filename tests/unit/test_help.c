/* Unit test: Help (app/help.c) – a chapter laid out in lines (title, sections, wrapped paragraphs, list items,
 * notes, code and result blocks, inline marks dropped), the list and the reader with buttons and taps, scrolling,
 * one screen, and loading the real chapters from sdk/docs. Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "help.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

/* --- recording backend: two screens like the 3DS; every character is size/2 pixels wide --- */
static char drawn[16384];
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
static const UiBackend ONE = { 1, 0, { 640, 0 }, { 360, 0 }, b_select, b_rect, b_text, b_width, NULL };

static uint32_t now = 1000;
static int frame(Help* h, uint32_t down) {
    UiInput in = { 0 };
    in.down = in.held = down; in.ms = now += 20;
    int r = help_update(h, &in);
    in.down = in.held = 0; in.ms = now += 20;
    if (r) r = help_update(h, &in);
    return r;
}
static int tap(Help* h, int x, int y) {
    UiInput in = { 0 };
    in.touching = in.touchDown = 1; in.x = x; in.y = y; in.ms = now += 20;
    return help_update(h, &in);
}
static int find_line(Help* h, const char* s, int kind) {
    for (int i = 0; i < help_lines(h); i++) {
        int k;
        const char* l = help_line(h, i, &k);
        if (!strcmp(l, s) && (kind < 0 || k == kind)) return i;
    }
    return -1;
}

static const char* CH1 =
    "# Pierwszy program\n"
    "\n"
    "To jest **akapit** z `kodem` w tekście, który jest na tyle długi, że na ekranie o szerokości czterystu pikseli\n"
    "musi się zawinąć w kilka linii.\n"
    "\n"
    "## Sekcja\n"
    "\n"
    "- pierwszy punkt listy, też dość długi, żeby sprawdzić zawijanie elementu listy na kolejną linię ekranu\n"
    "- drugi\n"
    "\n"
    "```cs\n"
    "class Program { static void Main() { Console.WriteLine(\"Hej\"); } }\n"
    "```\n"
    "\n"
    "```wynik\n"
    "Hej\n"
    "```\n"
    "\n"
    "> **Dla zaawansowanych:** dopisek\n"
    "> w dwóch liniach.\n";

int main(void) {
    ui_set_backend(&REC);
    Help* h = help_new();
    check(h && help_chapters(h) == 0, "help_new: no chapters");
    check(help_add(h, CH1) && help_add(h, "# Drugi\n\nTekst.\n") && help_add(h, "bez tytułu\n"), "help_add");
    check(help_chapters(h) == 3 && !strcmp(help_title(h, 0), "Pierwszy program") && !strcmp(help_title(h, 1), "Drugi"),
          "titles from the first '# ' line");
    check(!strcmp(help_title(h, 2), "Chapter 3"), "a chapter without a title gets a number");

    /* layout */
    check(help_open(h, 0) && help_reading(h) == 0, "open a chapter");
    int k;
    check(!strcmp(help_line(h, 0, &k), "Pierwszy program") && k == HL_TITLE, "the title line");
    int sec = find_line(h, "Sekcja", HL_HEADING);
    check(sec > 0 && help_line(h, sec - 1, &k) && k == HL_BLANK, "a section with a blank line before it");
    const char* p1 = help_line(h, 2, &k);
    check(k == HL_TEXT && !strncmp(p1, "To jest akapit z kodem w", 24), "the paragraph without ** and ` marks");
    int textLines = 0;
    for (int i = 0; i < help_lines(h); i++) {
        help_line(h, i, &k);
        if (k == HL_TEXT) textLines++;
        check(strlen(help_line(h, i, NULL)) * 13 / 2 <= 388 || k == HL_CODE, "no wrapped line wider than the screen");
    }
    check(textLines >= 3, "the long paragraph is wrapped");
    int b1 = -1;
    for (int i = 0; i < help_lines(h); i++) { help_line(h, i, &k); if (k == HL_BULLET && b1 < 0) b1 = i; }
    check(b1 > 0 && !strncmp(help_line(h, b1, NULL), "- pierwszy punkt", 16), "a list item starts with '- '");
    help_line(h, b1 + 1, &k);
    check(k == HL_BULLET && !strncmp(help_line(h, b1 + 1, NULL), "  ", 2), "its next line is indented");
    check(find_line(h, "- drugi", HL_BULLET) > b1, "the second item");
    check(find_line(h, "class Program { static void Main() { Console.WriteLine(\"Hej\"); } }", HL_CODE) > 0, "a code line, not wrapped");
    int res = find_line(h, "Hej", HL_RESULT);
    check(res > 0 && find_line(h, "Program wypisze:", HL_TEXT) == res - 1, "a result block with its label");
    check(find_line(h, "Dla zaawansowanych: dopisek w dwóch liniach.", HL_NOTE) > 0, "a note joined from two '> ' lines");

    /* the reader: scrolling, buttons, back */
    drawn[0] = 0; textsOn[0] = textsOn[1] = 0;
    help_draw(h);
    check(strstr(drawn, "Pierwszy program|") && strstr(drawn, "|Back|") && textsOn[0] > 0 && textsOn[1] > 0,
          "the reader: text on top, buttons below");
    frame(h, UI_BTN_DOWN);
    check(help_top(h) == 1, "Down scrolls");
    for (int i = 0; i < 50; i++) frame(h, UI_BTN_DOWN);
    check(help_top(h) == help_lines(h) - 13, "scrolling stops at the end (13 lines on 240 pixels)");
    int endTop = help_top(h);
    frame(h, UI_BTN_L);
    check(help_top(h) == (endTop > 12 ? endTop - 12 : 0), "L: a page up (not above the start)");
    frame(h, UI_BTN_R);
    check(help_top(h) == endTop, "R: a page down (not past the end)");
    tap(h, 2 * 106 + 10, 5);
    check(help_reading(h) == 1 && help_top(h) == 0, "Next opens the next chapter");
    tap(h, 1 * 106 + 10, 5);
    check(help_reading(h) == 0, "Prev");
    frame(h, UI_BTN_B);
    check(help_reading(h) == -1, "B back to the list");

    /* the list */
    drawn[0] = 0;
    help_draw(h);
    check(strstr(drawn, "1. Pierwszy program|") && strstr(drawn, "2. Drugi|"), "the list of chapters");
    frame(h, UI_BTN_DOWN);
    frame(h, UI_BTN_A);
    check(help_reading(h) == 1, "A opens the chosen chapter");
    frame(h, UI_BTN_B);
    tap(h, 50, 20 + 2 * 24 + 5);
    check(help_reading(h) == -1, "the first tap selects");
    tap(h, 50, 20 + 2 * 24 + 5);
    check(help_reading(h) == 2, "the second tap opens");
    check(frame(h, UI_BTN_START) == 0 && help_reading(h) == -1, "START leaves");
    check(frame(h, UI_BTN_B) == 0, "B in the list leaves");

    /* one screen: laid out again for its width */
    ui_set_backend(&ONE);
    help_open(h, 0);
    frame(h, 0);
    int narrowLines = help_lines(h);
    check(narrowLines > 0 && !strcmp(help_line(h, 0, NULL), "Pierwszy program"), "one screen: opens");
    drawn[0] = 0;
    help_draw(h);
    check(strstr(drawn, "|Back|") != NULL, "one screen: the buttons");
    help_free(h);

    /* the real chapters */
    ui_set_backend(&REC);
    Help* d = help_new();
    int n = help_load_dir(d, "sdk/docs");
    check(n >= 10 && !strcmp(help_title(d, 0), "Pierwszy program"), "sdk/docs: the chapters in order");
    for (int i = 0; i < n; i++) {
        help_open(d, i);
        check(help_lines(d) > 20, "every chapter has text");
        drawn[0] = 0;
        help_draw(d);
    }
    check(help_load_dir(d, "no/such/folder") == 0, "a missing folder: 0");
    help_free(d);
    ui_set_backend(NULL);
    if (!fails) printf("help: ok\n");
    return fails ? 1 : 0;
}
