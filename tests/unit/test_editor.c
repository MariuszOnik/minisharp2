/* Unit test: the code editor (app/editor.c) – open / save, typing through the keyboard path, buttons, run with a
 * jump to the error line, suggestions and the call signature, scrolling, leaving with unsaved changes, drawing.
 * Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "editor.h"
#include "keyboard.h"
#include "lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

/* --- recording backend: two screens like the 3DS --- */
static char drawn[16384];
static int curScreen, textsOn[2];
static uint32_t colorOfWord;            /* colour in which "class" was drawn */
static void b_select(int s) { curScreen = s; }
static void b_rect(float x, float y, float w, float h, uint32_t rgb) { (void)x; (void)y; (void)w; (void)h; (void)rgb; }
static void b_text(float x, float y, const char* s, float size, uint32_t rgb) {
    (void)x; (void)y; (void)size;
    textsOn[curScreen]++;
    if (!strcmp(s, "class")) colorOfWord = rgb;
    strncat(drawn, s, sizeof drawn - strlen(drawn) - 2);
    strcat(drawn, "|");
}
static float b_width(const char* s, float size) { return strlen(s) * size / 2; }
static const UiBackend REC = { 2, 1, { 400, 320 }, { 240, 240 }, b_select, b_rect, b_text, b_width, NULL };

static uint32_t now = 1000;
static int runs;
static const char* runAnswer;           /* NULL = ok, else the error message */

static int fake_run(const char* path, char* err, int cap) {
    (void)path;
    runs++;
    if (!runAnswer) return 1;
    snprintf(err, cap, "%s", runAnswer);
    return 0;
}

static int frame(Editor* ed, uint32_t down) {
    UiInput in = { 0 };
    in.down = in.held = down; in.ms = now += 20;
    int r = ed_update(ed, &in);
    in.down = in.held = 0; in.ms = now += 20;
    if (r) ed_update(ed, &in);
    return r;
}

static void type(Editor* ed, const char* s) {
    char c[2] = { 0, 0 };
    for (; *s; s++) {
        if (*s == '\n') ed_key(ed, KBD_ENTER);
        else { c[0] = *s; ed_key(ed, c); }
    }
}

static char* read_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    static char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

#define PATH "build/tmp/test_editor.cs"

int main(void) {
    lib_register_all();
    ui_set_backend(&REC);
    remove(PATH);
    EditorHost host = { fake_run };
    Editor* ed = ed_new(&host);
    check(ed != NULL, "ed_new");
    check(!ed_open(ed, PATH) && tb_len(ed_buf(ed)) == 0, "a file that does not exist: empty, new");

    type(ed, "class P {\nstatic void Main() {\nConsole.WriteLine(1);\n}\n}");
    check(tb_modified(ed_buf(ed)), "modified after typing");
    frame(ed, UI_BTN_SELECT);
    const char* saved = read_all(PATH);
    check(saved && !strcmp(saved, "class P {\n    static void Main() {\n        Console.WriteLine(1);\n    }\n}"),
          "SELECT saves (indented, without BOM)");
    check(!tb_modified(ed_buf(ed)), "not modified after saving");

    /* open again */
    Editor* ed2 = ed_new(&host);
    check(ed_open(ed2, PATH) && tb_lines(ed_buf(ed2)) == 5, "open an existing file");
    ed_free(ed2);

    /* run: ok, then an error with a line */
    runAnswer = NULL;
    frame(ed, UI_BTN_A);
    check(runs == 1 && strstr(ed_status(ed), "without errors"), "A runs");
    runAnswer = "compile error: line 3: 'Console' has no method 'WriteLin'";
    tb_set_cursor(ed_buf(ed), 0, 0);
    frame(ed, UI_BTN_A);
    check(ed_error_line(ed) == 3 && tb_line_of(ed_buf(ed), tb_cursor(ed_buf(ed))) == 2, "the cursor jumps to the error line");
    check(strstr(ed_status(ed), "WriteLin") != NULL, "the error in the status");
    type(ed, " ");
    check(ed_error_line(ed) == 0, "typing clears the error mark");
    tb_undo(ed_buf(ed));

    /* buttons: D-pad, L + D-pad, B, Y */
    tb_set_cursor(ed_buf(ed), 0, 0);
    frame(ed, UI_BTN_DOWN);
    frame(ed, UI_BTN_RIGHT);
    check(tb_line_of(ed_buf(ed), tb_cursor(ed_buf(ed))) == 1, "Down");
    UiInput in = { 0 };
    in.held = UI_BTN_L; in.down = UI_BTN_RIGHT; in.ms = now += 20;
    ed_update(ed, &in);
    check(tb_cursor(ed_buf(ed)) == tb_line_start(ed_buf(ed), 1) + tb_line_len(ed_buf(ed), 1), "L + Right: line end");
    int before = tb_len(ed_buf(ed));
    frame(ed, UI_BTN_B);
    check(tb_len(ed_buf(ed)) == before - 1, "B deletes");
    frame(ed, UI_BTN_Y);
    check(tb_len(ed_buf(ed)) == before, "Y undoes");

    /* suggestions and the signature come from the language service */
    tb_set_cursor(ed_buf(ed), tb_line_start(ed_buf(ed), 2) + tb_line_len(ed_buf(ed), 2), 0);
    ed_key(ed, KBD_ENTER);
    type(ed, "Conso");
    frame(ed, 0);
    frame(ed, UI_BTN_R);
    type(ed, ".WriteLine(");
    frame(ed, 0);
    char* line3 = (char*)tb_text(ed_buf(ed)) + tb_line_start(ed_buf(ed), 3);
    check(!strncmp(line3, "        Console.WriteLine(", 26), "R completes Conso -> Console");
    drawn[0] = 0;
    ed_draw(ed);
    check(strstr(drawn, "WriteLine(") != NULL && strstr(drawn, "[arg 1]") != NULL, "the signature hint is drawn");

    /* tapping a tool: Undo */
    int len = tb_len(ed_buf(ed));
    in = (UiInput){ 0 };
    in.touching = in.touchDown = 1; in.x = 2 * 80 + 10; in.y = 5; in.ms = now += 20;
    ed_update(ed, &in);
    check(tb_len(ed_buf(ed)) < len, "the Undo tool");

    /* drawing: colours, line numbers, both screens */
    drawn[0] = 0; textsOn[0] = textsOn[1] = 0; colorOfWord = 0;
    ed_draw(ed);
    check(colorOfWord == 0x569CD6, "'class' drawn as a keyword");
    check(strstr(drawn, "|1|") && strstr(drawn, "|5|"), "line numbers");
    check(textsOn[0] > 0 && textsOn[1] > 0 && strstr(drawn, "|Run|"), "code on top, tools and keyboard below");

    /* scrolling: the cursor stays on screen */
    tb_set_text(ed_buf(ed), "");
    for (int i = 0; i < 40; i++) type(ed, "x\n");
    frame(ed, 0);
    int vis = ed_visible_lines();
    check(vis == 14, "14 lines on the 240-pixel screen");
    check(ed_top_line(ed) == 40 - vis + 1, "scrolled down with the cursor");
    tb_set_cursor(ed_buf(ed), 0, 0);
    frame(ed, 0);
    check(ed_top_line(ed) == 0, "scrolled back up");

    /* leaving with unsaved changes needs START twice; without changes once */
    check(frame(ed, UI_BTN_START) == 1 && strstr(ed_status(ed), "unsaved"), "first START warns");
    check(frame(ed, UI_BTN_START) == 0, "second START leaves");
    ed_save(ed);
    check(frame(ed, UI_BTN_START) == 0, "saved: one START leaves");

    ed_free(ed);
    remove(PATH);
    ui_set_backend(NULL);
    if (!fails) printf("editor: ok\n");
    return fails ? 1 : 0;
}
