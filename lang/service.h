/* MiniSharp - language service: what an editor asks about the code, so the editor itself stays a plain text editor
 * (docs/ARCHITEKTURA.md, "Edytor kodu a język"). Colours of a line, suggestions at the cursor, the signature of
 * the call the cursor is in. Built on codeindex.c (reads the text, works on code that does not compile yet) and
 * on the registered built-in tables. The same C code serves the 3DS editor and the web page (wasm). */
#ifndef MS_SERVICE_H
#define MS_SERVICE_H

#include "codeindex.h"

#define SVC_WORD CI_WORD

/* ---------- colours ---------- */
enum { SVC_PLAIN, SVC_KEYWORD, SVC_TYPE, SVC_STRING, SVC_NUMBER, SVC_COMMENT };

/* Sets kinds[i] (SVC_*) for every byte of line[0..len). 'state' = the value returned for the line before
 * (0 for the first line): 1 means a comment slash-star is still open. Returns the state after this line.
 * svc may be NULL (then only the built-in classes count as types, not the project's). */
typedef struct MsService MsService;
int svc_colors(const MsService* svc, const char* line, int len, unsigned char* kinds, int state);

/* ---------- a project open in the editor ---------- */
MsService* svc_new(void);
void svc_free(MsService* svc);
/* the texts of the project's files (copied); call again after edits (the index is rebuilt) */
int svc_set_texts(MsService* svc, const char** texts, int n);
int svc_set_text(MsService* svc, int file, const char* text);     /* one file changed */

/* Suggestions for the word that ends at 'cursor' in file 'file': after "expr." the members of its type,
 * otherwise names from the file, the project's classes, built-in classes and keywords. Returns how many
 * (up to max) and sets *start to where the word begins (the suggestion replaces text[*start..cursor]). */
int svc_complete(MsService* svc, int file, int cursor, int* start, char out[][SVC_WORD], int max);

/* The call the cursor is in, e.g. "void Hit(int dmg, bool crit)" for "ship.Hit(3, |". *arg = which argument
 * the cursor is at (0 = first). 0 = not inside a call or the method is unknown. */
int svc_signature(MsService* svc, int file, int cursor, char* sig, int cap, int* arg);

/* ---------- one line without a project (the terminal) ---------- */
/* Suggestions in a single line: after "Console." the members of a built-in class, otherwise 'words' (the caller's
 * names, separated by spaces), built-in classes and keywords. Any letter case matches ("con" finds Console). */
int svc_complete_line(const char* line, int cursor, int* start, char out[][SVC_WORD], int max, const char* words);

#endif
