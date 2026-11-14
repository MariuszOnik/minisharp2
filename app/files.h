/* MiniSharp - the file list of the native app: the programs and projects on the card, before the editor.
 * Two levels, like the old app: the root folder (e.g. sdmc:/minisharp) holds single programs (.cs) and projects
 * (folders with .cs files, all compiled together); a project shows its .cs and .txt files (uses.txt).
 * New file / new project ask for a name on the touch keyboard and write a small starting class.
 * The list is on the touch screen (tap = select, tap again = open), the hints and the name on the other one.
 * Buttons: D-pad choose, L/R page, A open, B back (from a project to the root, from the root out), X new file,
 * Y new project (in the root), SELECT twice delete (a project folder only when empty), START out. Draws only through app/ui.h; tested on the PC (tests/unit/test_files.c).
 * The folders "tests", "themes", "docs" and dot-folders of the root are not listed (they belong to the app). */
#ifndef MS_FILES_H
#define MS_FILES_H

#include "ui.h"

enum { FILES_STAY, FILES_OPEN, FILES_LEAVE };

typedef struct Files Files;

Files* files_new(const char* root);
void files_free(Files* fs);
void files_scan(Files* fs);                     /* read the current folder again (after the editor saved files) */
/* one frame of input: FILES_OPEN = open files_chosen() in the editor, FILES_LEAVE = back to the menu */
int files_update(Files* fs, const UiInput* in);
void files_draw(Files* fs);
const char* files_chosen(const Files* fs);      /* the full path of the file to open */

/* for tests and the host */
int files_count(const Files* fs);
const char* files_name(const Files* fs, int i); /* "Game/" for a project, "main.cs" for a file */
int files_selected(const Files* fs);
const char* files_dir(const Files* fs);         /* the folder shown now */
const char* files_status(const Files* fs);
int files_naming(const Files* fs);              /* 1 = asking for a name (keyboard on screen) */
void files_key(Files* fs, const char* k);       /* text from the keyboard, KBD_BACKSPACE or KBD_ENTER */

/* the project a file belongs to: 1 and its folder in dir when the file is in a folder below root, 0 = a single file */
int files_project_of(const char* root, const char* path, char* dir, int cap);

#endif
