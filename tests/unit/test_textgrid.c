/* Unit test: the character grid behind Console (lib/textgrid.c) – writing, wrapping at the line end without an
 * extra empty line, scrolling, '\r' '\t' '\b', colours of the cells, clear, the cursor, Polish letters, the palette.
 * Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "textgrid.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

static const char* row(TextGrid* g, int y) {
    static char buf[256];
    tg_row_text(g, y, buf, sizeof buf);
    return buf;
}

int main(void) {
    check(tg_new(0, 5) == NULL && tg_new(5, -1) == NULL, "a bad size");
    TextGrid* g = tg_new(10, 4);
    check(g && tg_width(g) == 10 && tg_height(g) == 4, "tg_new");
    check(tg_fg(g) == TG_GRAY && tg_bg(g) == TG_BLACK && tg_cursor_x(g) == 0 && tg_cursor_y(g) == 0, "gray on black at 0,0");

    tg_write(g, "Hej\nświecie");
    check(!strcmp(row(g, 0), "Hej") && !strcmp(row(g, 1), "świecie"), "two lines, Polish letters one cell each");
    check(tg_cursor_x(g) == 7 && tg_cursor_y(g) == 1, "the cursor after the text");

    /* wrapping: 10 characters fill the line; '\n' then does not add an empty line */
    tg_clear(g);
    tg_write(g, "0123456789\nx");
    check(!strcmp(row(g, 0), "0123456789") && !strcmp(row(g, 1), "x"), "a full line and '\\n': no empty line");
    tg_clear(g);
    tg_write(g, "0123456789ab");
    check(!strcmp(row(g, 0), "0123456789") && !strcmp(row(g, 1), "ab"), "a long line wraps");

    /* scrolling */
    tg_clear(g);
    tg_write(g, "a\nb\nc\nd\ne");
    check(!strcmp(row(g, 0), "b") && !strcmp(row(g, 3), "e") && tg_cursor_y(g) == 3, "below the last line: scroll up");
    uint32_t v = tg_version(g);
    tg_write(g, "\n");
    check(!strcmp(row(g, 0), "c") && !strcmp(row(g, 3), "") && tg_version(g) != v, "an empty new line, the version grows");

    /* \r \t \b */
    tg_clear(g);
    tg_write(g, "abcdef\rXY");
    check(!strcmp(row(g, 0), "XYcdef"), "'\\r' goes to the line start");
    tg_clear(g);
    tg_write(g, "a\tb");
    check(!strcmp(row(g, 0), "a       b") && tg_cursor_x(g) == 9, "'\\t' to the next multiple of 8");
    tg_clear(g);
    tg_write(g, "ab\bX");
    check(!strcmp(row(g, 0), "aX"), "'\\b' one back");

    /* colours */
    tg_clear(g);
    tg_set_colors(g, TG_YELLOW, TG_DARKBLUE);
    tg_write(g, "Z");
    const TgCell* c = tg_cell(g, 0, 0);
    check(c && c->cp == 'Z' && c->fg == TG_YELLOW && c->bg == TG_DARKBLUE, "a cell keeps its colours");
    check(tg_cell(g, 1, 0)->bg == TG_BLACK, "the rest keeps the old background");
    tg_set_colors(g, 99, -1);
    check(tg_fg(g) == TG_YELLOW && tg_bg(g) == TG_DARKBLUE, "bad colours are ignored");
    tg_clear(g);
    check(tg_cell(g, 9, 3)->bg == TG_DARKBLUE && tg_cell(g, 0, 0)->cp == ' ', "clear fills with the current background");
    tg_reset_colors(g);
    check(tg_fg(g) == TG_GRAY && tg_bg(g) == TG_BLACK, "ResetColor");
    check(tg_cell(g, 10, 0) == NULL && tg_cell(g, 0, 4) == NULL, "no cell outside");

    /* the cursor */
    check(tg_set_cursor(g, 3, 2) && tg_cursor_x(g) == 3 && tg_cursor_y(g) == 2, "SetCursorPosition");
    tg_write(g, "@");
    check(!strcmp(row(g, 2), "   @"), "writing at the cursor");
    check(!tg_set_cursor(g, 10, 0) && !tg_set_cursor(g, 0, -1) && tg_cursor_x(g) == 4, "outside: refused, nothing moves");
    tg_set_cursor(g, 9, 0);
    tg_write(g, "!");
    check(tg_cursor_x(g) == 9 && tg_cursor_y(g) == 0, "after the last column the cursor stays on the line until the next character");

    check(tg_palette(TG_WHITE) == 0xFFFFFF && tg_palette(TG_DARKBLUE) == 0x000080 && tg_palette(16) == 0, "the palette");
    tg_free(g);

    /* the 3DS top screen with an 8x8 font */
    TextGrid* big = tg_new(50, 30);
    for (int i = 0; i < 40; i++) { char line[32]; snprintf(line, sizeof line, "line %d\n", i); tg_write(big, line); }
    check(!strcmp(row(big, 0), "line 11") && !strcmp(row(big, 28), "line 39") && !strcmp(row(big, 29), ""), "50x30: 40 lines scroll");
    tg_free(big);

    if (!fails) printf("textgrid: ok\n");
    return fails ? 1 : 0;
}
