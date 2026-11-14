/* MiniSharp - Help of the native app: the chapters of the language documentation (sdk/docs/NN-name.md, the small
 * Markdown subset described in sdk/docs/README.md) as a list and a scrolling reader. The chapter text is on the
 * bigger screen, the list of chapters and the buttons on the touch screen (or all on one screen).
 * Draws and reads input only through app/ui.h; tested on the PC (tests/unit/test_help.c).
 * List: D-pad choose, A / tap twice open, B or START leave. Reader: D-pad Up/Down scroll, L/R page, Left/Right move
 * long code lines sideways, B back to the list, START leave; touch: Back, Prev, Next, Copy (the example on screen). */
#ifndef MS_HELP_H
#define MS_HELP_H

#include "ui.h"

/* kinds of the laid-out lines */
enum { HL_BLANK, HL_TITLE, HL_HEADING, HL_TEXT, HL_BULLET, HL_CODE, HL_RESULT, HL_NOTE };

typedef struct Help Help;

Help* help_new(void);
void help_free(Help* h);
/* the chapters NN-*.md of a folder, in the order of their names; returns how many (0 = none or no folder) */
int help_load_dir(Help* h, const char* dir);
/* the chapters of several folders together, in the order of their file names; a file name found in more than one
 * folder is taken from the first of them (e.g. the card before the copy built into the app). Returns how many. */
int help_load_dirs(Help* h, const char* const* dirs, int ndirs);
/* one chapter from text (copied): the title is its first "# " line */
int help_add(Help* h, const char* text);

/* one frame of input; 0 = leave Help */
int help_update(Help* h, const UiInput* in);
void help_draw(Help* h);

/* for tests and the host */
int help_chapters(const Help* h);
const char* help_title(const Help* h, int i);
int help_open(Help* h, int i);                  /* lays the chapter out for the current screen; 0 = no such chapter */
int help_reading(const Help* h);                /* -1 = the list, otherwise the open chapter */
int help_lines(const Help* h);                  /* laid-out lines of the open chapter */
const char* help_line(const Help* h, int i, int* kind);
int help_top(const Help* h);                    /* first line on screen */

/* the Copy button of the reader: the first example with a line on screen goes to copy() (the editor's clipboard) */
void help_set_copy(Help* h, void (*copy)(const char* text));
char* help_example_on_screen(const Help* h);    /* malloc'ed, NULL = none */
const char* help_status(const Help* h);

#endif
