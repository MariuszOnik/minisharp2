/* MiniSharp - terminal (REPL) of the native app: a line typed on the touch keyboard (or buttons) runs with Enter / A,
 * variables stay for the next lines (app/repl.c), Console output scrolls on the output screen.
 * Draws and reads input only through app/ui.h, so the logic is tested on the PC (tests/unit/test_terminal.c).
 * Moved from the old source3ds/terminal.c, without the Gfx canvas (comes with the graphics in stage 3). */
#ifndef MS_TERMINAL_H
#define MS_TERMINAL_H

#include "ui.h"

#define TERM_COLS 56                    /* output lines are wrapped after this many characters */
#define TERM_SCROLLBACK 200             /* lines kept; older ones are dropped */
#define TERM_LINE_CAP 256               /* longest input line */
#define TERM_NSUGGEST 4

typedef struct Terminal Terminal;

/* the terminal also becomes the Console output (lib_set_output) until term_free */
Terminal* term_new(void);
void term_free(Terminal* t);
/* the host has a Stop button: no time limit for loops (repl_set_time_limit 0), the hint names the button */
void term_set_stop_hint(Terminal* t, const char* hint);

void term_print(Terminal* t, const char* s);    /* output text ('\n' ends a line); also what Console writes */
void term_key(Terminal* t, const char* k);      /* text from the keyboard, KBD_BACKSPACE or KBD_ENTER (keyboard.h) */
/* one frame of input: the touch keyboard, taps on the suggestions / the input line, buttons
 * (A run, B delete, Left/Right cursor, Up/Down history, R first suggestion, Y clear). 0 = START: leave. */
int term_update(Terminal* t, const UiInput* in);
void term_draw(Terminal* t);                    /* both screens (or one, see UiBackend.screens) */

/* for tests */
int term_lines(const Terminal* t);
const char* term_line(const Terminal* t, int i);   /* i-th line of the scrollback, 0 = oldest kept */
const char* term_input(const Terminal* t);
int term_cursor(const Terminal* t);
int term_continuing(const Terminal* t);         /* 1 while an open '{' or '(' waits for more lines */
int term_suggestions(const Terminal* t, const char** out);   /* up to TERM_NSUGGEST */

#endif
