/* MiniSharp - code editor of the native app: a plain text editor (app/textbuf.c) that asks the language service
 * (lang/service.h) for colours, suggestions and call signatures. The code is on screen 0, the tools, suggestions
 * and the touch keyboard on the touch screen (or all on one screen). Draws and reads input only through app/ui.h;
 * tested on the PC (tests/unit/test_editor.c). Running a program is the host's job (EditorHost.run).
 * Buttons: D-pad cursor, L + D-pad line start / end / page, B delete, Y undo, R first suggestion,
 * A run, SELECT save, START leave (twice when there are unsaved changes). */
#ifndef MS_EDITOR_H
#define MS_EDITOR_H

#include "ui.h"
#include "textbuf.h"

typedef struct {
    /* runs the saved file; 1 = ok, 0 = error with the message in err ("compile error: line 3: ...") */
    int (*run)(const char* path, char* err, int errcap);
} EditorHost;

typedef struct Editor Editor;

Editor* ed_new(const EditorHost* host);
void ed_free(Editor* ed);
int ed_open(Editor* ed, const char* path);      /* 0 = the file does not exist yet: an empty one with that name */
int ed_save(Editor* ed);                        /* 1 = written */
/* one frame of input; 0 = leave the editor */
int ed_update(Editor* ed, const UiInput* in);
void ed_draw(Editor* ed);

/* for tests and the host */
TextBuf* ed_buf(Editor* ed);
const char* ed_status(const Editor* ed);
int ed_top_line(const Editor* ed);
int ed_error_line(const Editor* ed);            /* 1-based line of the last error, 0 = none */
void ed_key(Editor* ed, const char* k);         /* text from the keyboard, KBD_BACKSPACE or KBD_ENTER */
int ed_visible_lines(void);

#endif
