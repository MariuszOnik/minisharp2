/* MiniSharp - the console screen of the native app: what a running program writes (the Console character grid,
 * lib/textgrid.h) on the bigger screen, and on the touch screen the state, the line being typed for
 * Console.ReadLine and the touch keyboard. It does not run the program: the host calls vm_call / vm_resume_input
 * and tells the screen what is going on (cs_ask when the VM waits for a line, cs_finish at the end).
 * Draws and reads input only through app/ui.h; tested on the PC (tests/unit/test_conscreen.c).
 * Asking: the keyboard types, Enter (or A) gives the line; START ends the input (ReadLine gives null).
 * Finished: A or B leaves. */
#ifndef MS_CONSCREEN_H
#define MS_CONSCREEN_H

#include "ui.h"
#include "textgrid.h"

enum { CS_RUNNING, CS_ASKING, CS_FINISHED };
enum { CS_NONE, CS_LINE /* cs_line() is ready */, CS_EOF /* the input ended */, CS_LEAVE };

typedef struct ConScreen ConScreen;

ConScreen* cs_new(int cols, int rows);  /* with its own grid of that size */
void cs_free(ConScreen* cs);
TextGrid* cs_grid(ConScreen* cs);       /* give it to lib_console_set_grid */

void cs_ask(ConScreen* cs);             /* the VM waits for Console.ReadLine: show the input line and the keyboard */
/* the program ended: ok = 1, or the error message (shown in red in the grid and in the status) */
void cs_finish(ConScreen* cs, int ok, const char* err);
int cs_state(const ConScreen* cs);
void cs_set_hint(ConScreen* cs, const char* s); /* the status line while running, e.g. "START+SELECT: stop" */

/* one frame of input: CS_LINE when a typed line is ready (it is also written into the grid, like a console
 * echoes it), CS_EOF when the user ended the input, CS_LEAVE when the finished screen is left */
int cs_update(ConScreen* cs, const UiInput* in);
const char* cs_line(const ConScreen* cs);
void cs_key(ConScreen* cs, const char* k);      /* text from the keyboard, KBD_BACKSPACE or KBD_ENTER */
void cs_draw(ConScreen* cs);

#endif
