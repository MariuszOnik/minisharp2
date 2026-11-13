/* MiniSharp - REPL (terminal): runs one input at a time, keeping variables between inputs.
 * The host reads the text (keyboard, stdin), shows it and passes it here; output goes through Console (lib_set_output). */
#ifndef MS_REPL_H
#define MS_REPL_H

typedef struct Repl Repl;

Repl* repl_new(void);
void repl_free(Repl* r);
/* A loop still running after this many seconds (+ up to one) stops with an error; default 4. 0 = no limit: for hosts
 * with a Stop button (ms_set_interrupt), where a game loop typed in the terminal may run as long as it likes. */
void repl_set_time_limit(Repl* r, int seconds);

/* Runs one input: a line, or several lines (a loop typed over a few lines, see repl_incomplete).
 * 1 = ok, 0 = error (message in err; for a one-line input without the "line N:" prefix). */
int repl_eval(Repl* r, const char* text, char* err, int errlen);

/* 1 when the text so far leaves a '{', '(' or '[' open (outside strings, characters and comments):
 * the host then asks for the next line and joins them before repl_eval, like "for (...) {" + body + "}". */
int repl_incomplete(const char* text);

/* Completions for the word that ends at 'cursor': after "Math." the members of Math, otherwise
 * classes, keywords and the terminal's variables. Returns how many (up to max) and sets *start
 * to where the word begins (the completion replaces line[*start..cursor]). */
#define REPL_WORD 32
int repl_complete(Repl* r, const char* line, int cursor, int* start, char out[][REPL_WORD], int max);
/* (the same for any line with the caller's own names: svc_complete_line in lang/service.h) */

#endif
