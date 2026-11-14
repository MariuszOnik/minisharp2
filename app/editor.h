/* MiniSharp - code editor of the native app: a plain text editor (app/textbuf.c) that asks the language service
 * (lang/service.h) for colours, suggestions and call signatures. The code is on screen 0, the tools, suggestions
 * and the touch keyboard on the touch screen (or all on one screen). Draws and reads input only through app/ui.h;
 * tested on the PC (tests/unit/test_editor.c). Running a program is the host's job (EditorHost.run).
 * Buttons: D-pad cursor, L + D-pad line start / end / page, B delete, Y undo, R first suggestion,
 * A run, SELECT save, START leave (twice when there are unsaved changes), X selection mode: the D-pad grows the
 * selection and the tools become Copy / Cut / Paste / All / Done (X again: done). Tools: Save, Run, Undo, Redo, Help.
 * A project's files are tabs: L + X next, L + Y previous; Run saves every changed tab. */
#ifndef MS_EDITOR_H
#define MS_EDITOR_H

#include "ui.h"
#include "textbuf.h"

typedef struct {
    /* runs the saved file; 1 = ok, 0 = error with the message in err ("compile error: line 3: ...") */
    int (*run)(const char* path, char* err, int errcap);
    /* the Help tool: shows Help (app/help.h) and returns when the user leaves it; NULL = no Help */
    void (*help)(void);
} EditorHost;

typedef struct Editor Editor;

Editor* ed_new(const EditorHost* host);
void ed_free(Editor* ed);
int ed_open(Editor* ed, const char* path);      /* 0 = the file does not exist yet: an empty one with that name */
int ed_save(Editor* ed);                        /* 1 = written */
/* the project the open file belongs to (its folder, with uses.txt), NULL = a single file. Its other files are read
 * now so that suggestions and signatures know their classes; returns how many were read. Call after ed_open. */
int ed_set_project(Editor* ed, const char* dir);
int ed_project_files(const Editor* ed);
/* the project's files are open in tabs (ed_set_project); L + X / L + Y switch to the next / previous one */
int ed_tabs(const Editor* ed);
int ed_current_tab(const Editor* ed);
const char* ed_tab_name(const Editor* ed, int i);    /* "Ship.cs" */
int ed_switch_tab(Editor* ed, int i);                /* 0 = no such tab */
/* one frame of input; 0 = leave the editor */
int ed_update(Editor* ed, const UiInput* in);
void ed_draw(Editor* ed);

/* for tests and the host */
TextBuf* ed_buf(Editor* ed);
const char* ed_status(const Editor* ed);
int ed_top_line(const Editor* ed);
int ed_error_line(const Editor* ed);            /* 1-based line of the last error, 0 = none */
void ed_key(Editor* ed, const char* k);         /* text from the keyboard, KBD_BACKSPACE or KBD_ENTER */
int ed_visible_lines(const Editor* ed);         /* code lines on the screen (they depend on the font size) */

/* the code's size and colours: A- / A+ / Col on the touch screen, L + A / L + B. ed_set_config reads the settings
 * file (lines "font 13", "theme name") and the schemes folder (themes/<name>.txt with lines like "keyword 569CD6";
 * keys: text keyword type string number comment background linenumber linenumberactive currentline selection
 * cursor errorline titlebar); every change is written back to the settings file. */
void ed_set_config(Editor* ed, const char* settingsPath, const char* themesDir);
void ed_set_font_size(Editor* ed, int px);      /* 9..24 */
int ed_font_size(const Editor* ed);
int ed_load_theme(Editor* ed, const char* path); /* 0 = cannot read; keys the file does not have keep the default */
int ed_next_theme(Editor* ed);                  /* the next file of the themes folder by name */
const char* ed_theme_name(const Editor* ed);    /* "" = the built-in default */
int ed_sel_mode(const Editor* ed);              /* 1 = the selection mode (X) is on */
const char* ed_clipboard(void);                 /* the editor's clipboard, shared by all files ("" = empty) */
void ed_set_clipboard(const char* s);           /* e.g. an example copied in Help */

#endif
