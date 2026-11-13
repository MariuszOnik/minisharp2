/* MiniSharp - text buffer of the code editor: the text, the cursor, the selection, undo / redo. No drawing and
 * no knowledge of the language (colours and suggestions come from lang/service.h); tested on the PC
 * (tests/unit/test_textbuf.c). Positions are byte offsets into UTF-8 text; the cursor never stops inside a
 * character. Lines end with '\n' ("\r\n" from a file becomes "\n"). */
#ifndef MS_TEXTBUF_H
#define MS_TEXTBUF_H

typedef struct TextBuf TextBuf;

TextBuf* tb_new(const char* text);              /* NULL text = empty */
void tb_free(TextBuf* tb);
int tb_set_text(TextBuf* tb, const char* text); /* replaces everything, clears undo, cursor at the start */

const char* tb_text(const TextBuf* tb);         /* the whole text, NUL-terminated; valid until the next change */
int tb_len(const TextBuf* tb);
int tb_lines(const TextBuf* tb);                /* at least 1 */
int tb_line_start(const TextBuf* tb, int line); /* byte offset of line (0-based) */
int tb_line_len(const TextBuf* tb, int line);   /* bytes, without the '\n' */
int tb_line_of(const TextBuf* tb, int pos);
int tb_col_of(const TextBuf* tb, int pos);      /* in characters, not bytes */
int tb_pos_at(const TextBuf* tb, int line, int col);   /* clamped to the text and to the line's end */

/* ---------- cursor and selection ---------- */
int tb_cursor(const TextBuf* tb);
/* select = 1: the selection grows from where it started (Shift + arrow); 0: no selection */
void tb_set_cursor(TextBuf* tb, int pos, int select);
enum { TB_LEFT, TB_RIGHT, TB_UP, TB_DOWN, TB_HOME, TB_END, TB_WORD_LEFT, TB_WORD_RIGHT, TB_TOP, TB_BOTTOM };
void tb_move(TextBuf* tb, int how, int select);
void tb_move_lines(TextBuf* tb, int lines, int select);     /* page up / down: -n / +n lines */
int tb_has_selection(const TextBuf* tb);
void tb_selection(const TextBuf* tb, int* from, int* to);   /* from <= to; from == to when nothing is selected */
void tb_select_all(TextBuf* tb);
void tb_select_word(TextBuf* tb);               /* the word under the cursor (double tap) */
char* tb_copy(const TextBuf* tb);               /* the selected text (malloc, free it), "" when none */

/* ---------- editing (each replaces the selection) ---------- */
void tb_insert(TextBuf* tb, const char* s);
void tb_backspace(TextBuf* tb);                 /* the selection, or the character before the cursor */
void tb_delete(TextBuf* tb);                    /* the selection, or the character after the cursor */
/* Enter: '\n' and the indentation of the line; one level more after '{'. A '}' typed at the start of an
 * indented line moves one level left (tb_insert does it). */
void tb_newline(TextBuf* tb);
#define TB_INDENT 4
void tb_indent(TextBuf* tb, int dir);           /* +1 / -1: the selected lines (or the cursor's line) by TB_INDENT */

/* ---------- undo ---------- */
/* typing in a row is one step; a jump of the cursor, Enter, delete after typing etc. start a new one */
int tb_undo(TextBuf* tb);                       /* 0 = nothing to undo */
int tb_redo(TextBuf* tb);
int tb_modified(const TextBuf* tb);             /* changed since tb_set_saved (or since the start) */
void tb_set_saved(TextBuf* tb);

#endif
