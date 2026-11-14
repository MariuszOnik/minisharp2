/* Unit test: the code editor (app/editor.c) – open / save, typing through the keyboard path, buttons, run with a
 * jump to the error line, suggestions and the call signature, scrolling, leaving with unsaved changes, drawing,
 * suggestions from the other files of a project (ed_set_project).
 * Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "editor.h"
#include "keyboard.h"
#include "lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0777)
#endif

static void write_file(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

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

    /* selection mode (X): D-pad selects, the tools are Copy / Cut / Paste / All; the clipboard is shared */
    Editor* es = ed_new(&host);
    ed_open(es, "build/tmp/test_sel.cs");
    tb_set_text(ed_buf(es), "abc def\nxyz");
    frame(es, UI_BTN_X);
    check(ed_sel_mode(es), "X turns the selection mode on");
    drawn[0] = 0;
    ed_draw(es);
    check(strstr(drawn, "|Copy|") && strstr(drawn, "|Paste|") && !strstr(drawn, "|Run|"), "the tools become Copy / Cut / Paste / All");
    frame(es, UI_BTN_RIGHT); frame(es, UI_BTN_RIGHT); frame(es, UI_BTN_RIGHT);
    check(tb_has_selection(ed_buf(es)), "the D-pad grows the selection");
    UiInput ti = { 0 };
    ti.touching = ti.touchDown = 1; ti.x = 0 * 80 + 10; ti.y = 5; ti.ms = now += 20;     /* Copy */
    ed_update(es, &ti);
    check(!strcmp(ed_clipboard(), "abc") && !ed_sel_mode(es) && !tb_has_selection(ed_buf(es)), "Copy: the clipboard, the mode ends");
    tb_set_cursor(ed_buf(es), tb_len(ed_buf(es)), 0);
    frame(es, UI_BTN_X);
    ti.x = 2 * 80 + 10; ti.ms = now += 20;                                                /* Paste */
    ed_update(es, &ti);
    check(!strcmp(tb_text(ed_buf(es)), "abc def\nxyzabc"), "Paste at the cursor");
    tb_set_cursor(ed_buf(es), 4, 0);
    frame(es, UI_BTN_X);
    frame(es, UI_BTN_RIGHT); frame(es, UI_BTN_RIGHT); frame(es, UI_BTN_RIGHT);
    ti.x = 1 * 80 + 10; ti.ms = now += 20;                                                /* Cut */
    ed_update(es, &ti);
    check(!strcmp(tb_text(ed_buf(es)), "abc \nxyzabc") && !strcmp(ed_clipboard(), "def"), "Cut removes and copies");
    frame(es, UI_BTN_Y);
    check(!strcmp(tb_text(ed_buf(es)), "abc def\nxyzabc"), "Y undoes the cut");
    frame(es, UI_BTN_X);
    ti.x = 3 * 80 + 10; ti.ms = now += 20;                                                /* All */
    ed_update(es, &ti);
    int fa, fb;
    tb_selection(ed_buf(es), &fa, &fb);
    check(fa == 0 && fb == tb_len(ed_buf(es)) && ed_sel_mode(es), "All selects everything");
    frame(es, UI_BTN_X);
    check(!ed_sel_mode(es) && !tb_has_selection(ed_buf(es)), "X again: done, the selection is dropped");
    Editor* es2 = ed_new(&host);
    ed_open(es2, "build/tmp/test_sel2.cs");
    frame(es2, UI_BTN_X);
    ti.x = 2 * 80 + 10; ti.ms = now += 20;
    ed_update(es2, &ti);
    check(!strcmp(tb_text(ed_buf(es2)), "def"), "the clipboard is shared between files");
    ed_free(es2);
    ed_free(es);

    /* a project: suggestions and signatures know the classes of its other files */
    make_dir("build/tmp/edproj");
    write_file("build/tmp/edproj/Ship.cs", "class Ship\n{\n    public int hp;\n    public void Hit(int dmg, bool crit) { hp -= dmg; }\n}\n");
    write_file("build/tmp/edproj/Main.cs", "class Program\n{\n    static void Main()\n    {\n    }\n}\n");
    Editor* ep = ed_new(&host);
    check(ed_open(ep, "build/tmp/edproj/Main.cs"), "open a project file");
    check(ed_set_project(ep, "build/tmp/edproj") == 1 && ed_project_files(ep) == 1, "the other file of the project is read");
    TextBuf* pb = ed_buf(ep);
    tb_set_cursor(pb, tb_line_start(pb, 3) + tb_line_len(pb, 3), 0);
    ed_key(ep, KBD_ENTER);
    type(ep, "Ship s = new Ship();");
    ed_key(ep, KBD_ENTER);
    type(ep, "s.Hi");
    frame(ep, 0);
    frame(ep, UI_BTN_R);
    const char* l5 = tb_text(pb) + tb_line_start(pb, 5);
    check(!strncmp(l5, "        s.Hit", 13), "R completes a method of a class from another file");
    type(ep, "(3, ");
    frame(ep, 0);
    drawn[0] = 0;
    ed_draw(ep);
    check(strstr(drawn, "Hit(int dmg, bool crit)") != NULL && strstr(drawn, "[arg 2]") != NULL,
          "the signature of a method from another file");
    check(ed_set_project(ep, NULL) == 0 && ed_project_files(ep) == 0, "no project: the other files are dropped");
    ed_free(ep);
    remove("build/tmp/edproj/Ship.cs");
    remove("build/tmp/edproj/Main.cs");
    rmdir("build/tmp/edproj");
    ui_set_backend(NULL);
    if (!fails) printf("editor: ok\n");
    return fails ? 1 : 0;
}
