/* Unit test: the editor's text buffer (app/textbuf.c) – lines, moving over UTF-8, selection, copy, typing with
 * indentation, undo / redo in steps, the "modified" flag. In the expected texts "|" marks the cursor.
 * Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "textbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

/* the text with "|" at the cursor */
static void expect(TextBuf* tb, const char* want, const char* what) {
    char got[2048];
    int c = tb_cursor(tb);
    snprintf(got, sizeof got, "%.*s|%s", c, tb_text(tb), tb_text(tb) + c);
    if (strcmp(got, want)) { fails++; printf("FAIL: %s\n  got  [%s]\n  want [%s]\n", what, got, want); }
}

/* a buffer from a text with "|" as the cursor */
static TextBuf* make(const char* marked) {
    char t[2048];
    const char* bar = strchr(marked, '|');
    int c = (int)(bar - marked);
    snprintf(t, sizeof t, "%.*s%s", c, marked, bar + 1);
    TextBuf* tb = tb_new(t);
    tb_set_cursor(tb, c, 0);
    return tb;
}

static void type(TextBuf* tb, const char* s) {
    char c[2] = { 0, 0 };
    for (; *s; s++) {
        if (*s == '\n') tb_newline(tb);
        else { c[0] = *s; tb_insert(tb, c); }
    }
}

static void test_lines(void) {
    TextBuf* tb = tb_new("ab\r\ncd\n\nżółw");
    check(!strcmp(tb_text(tb), "ab\ncd\n\nżółw"), "\\r\\n becomes \\n");
    check(tb_lines(tb) == 4, "4 lines");
    check(tb_line_start(tb, 1) == 3 && tb_line_len(tb, 1) == 2, "line 1");
    check(tb_line_len(tb, 2) == 0, "empty line");
    check(tb_line_of(tb, 0) == 0 && tb_line_of(tb, 2) == 0 && tb_line_of(tb, 3) == 1 && tb_line_of(tb, 6) == 2, "line_of");
    int z = tb_line_start(tb, 3);
    check(tb_col_of(tb, z + 3) == 2, "columns count characters, not bytes (ż, ó are 2 bytes)");
    check(tb_pos_at(tb, 3, 2) == z + 4, "pos_at in a line with Polish letters");
    check(tb_pos_at(tb, 0, 99) == 2, "pos_at clamps to the line end");
    tb_free(tb);
    tb = tb_new("\xEF\xBB\xBF" "class A { }");
    check(!strcmp(tb_text(tb), "class A { }"), "BOM removed");
    tb_free(tb);
    tb = tb_new(NULL);
    check(tb_len(tb) == 0 && tb_lines(tb) == 1, "empty buffer");
    tb_free(tb);
}

static void test_moving(void) {
    TextBuf* tb = make("|żó x\n  abc def\nq");
    tb_move(tb, TB_RIGHT, 0);
    expect(tb, "ż|ó x\n  abc def\nq", "Right over a 2-byte letter");
    tb_move(tb, TB_RIGHT, 0);
    tb_move(tb, TB_LEFT, 0);
    expect(tb, "ż|ó x\n  abc def\nq", "Left back");
    tb_move(tb, TB_END, 0);
    expect(tb, "żó x|\n  abc def\nq", "End");
    tb_move(tb, TB_DOWN, 0);
    expect(tb, "żó x\n  ab|c def\nq", "Down keeps the column (4 characters)");
    tb_move(tb, TB_DOWN, 0);
    expect(tb, "żó x\n  abc def\nq|", "Down to a short line: its end");
    tb_move(tb, TB_UP, 0);
    expect(tb, "żó x\n  ab|c def\nq", "Up: the remembered column comes back");
    tb_move(tb, TB_HOME, 0);
    expect(tb, "żó x\n  |abc def\nq", "Home: first non-space");
    tb_move(tb, TB_HOME, 0);
    expect(tb, "żó x\n|  abc def\nq", "Home again: line start");
    tb_move(tb, TB_WORD_RIGHT, 0);
    expect(tb, "żó x\n  |abc def\nq", "word right over spaces");
    tb_move(tb, TB_WORD_RIGHT, 0);
    expect(tb, "żó x\n  abc |def\nq", "word right");
    tb_move(tb, TB_WORD_LEFT, 0);
    expect(tb, "żó x\n  |abc def\nq", "word left");
    tb_move(tb, TB_BOTTOM, 0);
    expect(tb, "żó x\n  abc def\nq|", "bottom");
    tb_move(tb, TB_TOP, 0);
    expect(tb, "|żó x\n  abc def\nq", "top");
    tb_move(tb, TB_UP, 0);
    expect(tb, "|żó x\n  abc def\nq", "Up on the first line: start");
    tb_move_lines(tb, 10, 0);
    expect(tb, "żó x\n  abc def\nq|", "page down past the end");
    tb_set_cursor(tb, 1, 0);                          /* inside "ż": moved to its start */
    check(tb_cursor(tb) == 0, "the cursor never stops inside a character");
    tb_free(tb);
}

static void test_selection(void) {
    TextBuf* tb = make("one |two three");
    tb_move(tb, TB_WORD_RIGHT, 1);
    check(tb_has_selection(tb), "Shift + word right selects");
    char* c = tb_copy(tb);
    check(!strcmp(c, "two "), "copy");
    free(c);
    tb_insert(tb, "2 ");
    expect(tb, "one 2 |three", "typing replaces the selection");
    tb_move(tb, TB_LEFT, 1);
    tb_move(tb, TB_LEFT, 1);
    tb_move(tb, TB_RIGHT, 0);
    expect(tb, "one 2 |three", "Right without Shift goes to the selection's end");
    tb_select_all(tb);
    c = tb_copy(tb);
    check(!strcmp(c, "one 2 three"), "select all");
    free(c);
    tb_set_cursor(tb, 8, 0);
    tb_select_word(tb);
    int a, b;
    tb_selection(tb, &a, &b);
    check(a == 6 && b == 11, "select the word under the cursor");
    tb_backspace(tb);
    expect(tb, "one 2 |", "backspace deletes the selection");
    tb_free(tb);
}

static void test_typing(void) {
    TextBuf* tb = make("|");
    type(tb, "class A {\nvoid F() {\nx = 1;\n}\n}");
    expect(tb, "class A {\n    void F() {\n        x = 1;\n    }\n}|", "auto indentation and '}' one level left");

    tb_free(tb);
    tb = make("if (a) {|}");
    tb_newline(tb);
    expect(tb, "if (a) {\n    |\n}", "Enter between { and }: the } on its own line");

    tb_free(tb);
    tb = make("a|b");
    tb_delete(tb);
    expect(tb, "a|", "Delete");
    tb_backspace(tb);
    expect(tb, "|", "Backspace");
    tb_backspace(tb);
    tb_delete(tb);
    expect(tb, "|", "nothing to delete at the edges");
    tb_insert(tb, "ż");
    tb_backspace(tb);
    check(tb_len(tb) == 0, "Backspace removes a whole 2-byte letter");
    tb_free(tb);

    tb = make("|a\nb\n  c");
    tb_move(tb, TB_BOTTOM, 1);
    tb_indent(tb, 1);
    check(!strcmp(tb_text(tb), "    a\n    b\n      c"), "indent the selected lines");
    tb_indent(tb, -1);
    tb_indent(tb, -1);
    check(!strcmp(tb_text(tb), "a\nb\nc"), "unindent (only spaces, at most 4 a time)");
    tb_undo(tb);
    check(!strcmp(tb_text(tb), "a\nb\n  c"), "indent is one undo step");
    tb_set_cursor(tb, 1, 0);
    tb_indent(tb, 1);
    expect(tb, "    a|\nb\n  c", "indent one line: the cursor stays on its character");
    tb_free(tb);
}

static void test_undo(void) {
    TextBuf* tb = make("|");
    check(!tb_modified(tb) && !tb_undo(tb), "fresh: not modified, nothing to undo");
    type(tb, "hello");
    type(tb, " world");
    check(tb_modified(tb), "modified");
    tb_undo(tb);
    expect(tb, "|", "typing in a row is one step");
    tb_redo(tb);
    expect(tb, "hello world|", "redo");
    check(!tb_redo(tb), "nothing more to redo");

    tb_move(tb, TB_HOME, 0);
    tb_move(tb, TB_END, 0);                           /* a jump: the next typing is a new step */
    type(tb, "!");
    tb_newline(tb);
    type(tb, "x");
    tb_undo(tb);
    expect(tb, "hello world!\n|", "undo the typing after Enter");
    tb_undo(tb);
    expect(tb, "hello world!|", "undo Enter");
    tb_undo(tb);
    expect(tb, "hello world|", "undo the '!' typed after a jump");

    tb_backspace(tb); tb_backspace(tb); tb_backspace(tb);
    expect(tb, "hello wo|", "three backspaces");
    tb_undo(tb);
    expect(tb, "hello world|", "backspaces in a row are one step");

    tb_set_saved(tb);
    check(!tb_modified(tb), "saved");
    type(tb, "s");
    check(tb_modified(tb), "typed after saving");
    tb_undo(tb);
    check(!tb_modified(tb), "undo back to the saved state");
    tb_undo(tb);
    type(tb, "Z");                                    /* the saved state is in the dropped redo branch */
    check(tb_modified(tb), "a new edit after undo past the save: modified for good");

    tb_select_all(tb);
    tb_insert(tb, "new");
    tb_undo(tb);
    int a, b;
    tb_selection(tb, &a, &b);
    check(a == 0 && b == tb_len(tb), "undo brings the selection back");

    tb_set_text(tb, "x");
    check(!tb_undo(tb) && !tb_modified(tb), "set_text clears undo");
    tb_free(tb);
}

int main(void) {
    test_lines();
    test_moving();
    test_selection();
    test_typing();
    test_undo();
    if (!fails) printf("textbuf: ok\n");
    return fails ? 1 : 0;
}
