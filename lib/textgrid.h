/* MiniSharp - the character grid behind Console (PLAN.md, "Podział Console na warstwy", layer 2): cells with a
 * character and two colours, a cursor, writing with wrapping and scrolling, clearing. No drawing: a platform reads
 * the cells and draws them its own way (3DS: the top screen 50x30 with an 8x8 font; web: a canvas; tests: text).
 * Colours are the 16 ConsoleColor numbers of C# (0 Black ... 15 White); tg_palette gives their RGB.
 * Text is UTF-8, one cell per code point. Tested on the PC (tests/unit/test_textgrid.c). */
#ifndef MS_TEXTGRID_H
#define MS_TEXTGRID_H

#include <stdint.h>

typedef struct { uint32_t cp; uint8_t fg, bg; } TgCell;   /* cp = code point (' ' when empty) */

enum {                                  /* ConsoleColor, in the order of C# */
    TG_BLACK, TG_DARKBLUE, TG_DARKGREEN, TG_DARKCYAN, TG_DARKRED, TG_DARKMAGENTA, TG_DARKYELLOW, TG_GRAY,
    TG_DARKGRAY, TG_BLUE, TG_GREEN, TG_CYAN, TG_RED, TG_MAGENTA, TG_YELLOW, TG_WHITE
};

typedef struct TextGrid TextGrid;

TextGrid* tg_new(int w, int h);         /* gray on black, empty, cursor at 0,0; NULL = bad size or no memory */
void tg_free(TextGrid* g);
int tg_width(const TextGrid* g);
int tg_height(const TextGrid* g);

/* Writes text at the cursor in the current colours. '\n' next line, '\r' line start, '\t' next multiple of 8,
 * '\b' one back. A full line wraps when the next character comes (so 'w' characters and then '\n' make one line,
 * not one and an empty one); writing below the last line scrolls everything up by one. */
void tg_write(TextGrid* g, const char* utf8);
void tg_clear(TextGrid* g);             /* every cell empty in the current background, cursor at 0,0 */

int tg_set_cursor(TextGrid* g, int x, int y);   /* 0 = outside the grid (nothing changes; C# throws) */
int tg_cursor_x(const TextGrid* g);
int tg_cursor_y(const TextGrid* g);
void tg_set_colors(TextGrid* g, int fg, int bg);    /* 0..15; other values are ignored */
int tg_fg(const TextGrid* g);
int tg_bg(const TextGrid* g);
void tg_reset_colors(TextGrid* g);      /* Console.ResetColor: gray on black */

const TgCell* tg_cell(const TextGrid* g, int x, int y);   /* NULL outside */
/* row y as UTF-8 without the spaces at its end (dumps of the grid in tests, copying the output) */
int tg_row_text(const TextGrid* g, int y, char* out, int cap);
/* grows by every change: a platform redraws only when it differs from the last one it drew */
uint32_t tg_version(const TextGrid* g);

uint32_t tg_palette(int color);         /* 0xRRGGBB of a ConsoleColor (the Windows console colours) */

#endif
