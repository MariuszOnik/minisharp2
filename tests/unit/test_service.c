/* Unit test: the language service (lang/service.c) – colours of lines (with a comment over several lines),
 * suggestions at the cursor in a project, the signature of the call the cursor is in, suggestions in one line.
 * The cursor is where the marker "|" stands in the test text (the marker is removed).
 * Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "service.h"
#include "lib.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

/* colours of a line as letters: . plain, k keyword, t type, s string, n number, c comment */
static void expect(const char* got, const char* want, const char* what) {
    if (strcmp(got, want)) { fails++; printf("FAIL: %s\n  got  %s\n  want %s\n", what, got, want); }
}

static int colors(const MsService* svc, const char* line, char* out, int state) {
    unsigned char k[256];
    int len = (int)strlen(line);
    state = svc_colors(svc, line, len, k, state);
    for (int i = 0; i < len; i++) out[i] = ".ktsnc"[k[i]];
    out[len] = 0;
    return state;
}

static void test_colors(void) {
    char c[256];
    colors(NULL, "int x = 42; // hi", c, 0);
    expect(c, "ttt.....nn..ccccc", "type, number, comment");
    colors(NULL, "if (s == \"a\\\"b\") return;", c, 0);
    expect(c, "kk.......ssssss..kkkkkk.", "keywords and a string with an escaped quote");
    colors(NULL, "Console.Write('x', 1.5f, x2);", c, 0);
    expect(c, "ttttttt.......sss..nnnn......", "built-in class, char, float, digit in a name");
    int st = colors(NULL, "a = 1; /* start", c, 0);
    check(st == 1, "comment opens"); expect(c, "....n..cccccccc", "comment opens");
    st = colors(NULL, "still */ b", c, st);
    check(st == 0, "comment closes"); expect(c, "cccccccc..", "comment closes on the next line");
    colors(NULL, "var t = $\"{a} b\";", c, 0);
    expect(c, "ttt.....ssssssss.", "interpolated string");

    MsService* svc = svc_new();
    const char* texts[] = { "class Ship { }" };
    svc_set_texts(svc, texts, 1);
    colors(svc, "Ship s = new Ship();", c, 0);
    expect(c, "tttt.....kkk.tttt...", "the project's class is a type");
    svc_free(svc);
}

/* sets the project to one file with "|" as the cursor; returns the cursor */
static int open1(MsService* svc, const char* marked) {
    static char text[2048];
    const char* bar = strchr(marked, '|');
    int cur = (int)(bar - marked);
    snprintf(text, sizeof text, "%.*s%s", cur, marked, bar + 1);
    const char* texts[] = { text };
    svc_set_texts(svc, texts, 1);
    return cur;
}

static const char* SHIP =
    "class Ship {\n"
    "    public int hp;\n"
    "    private int fuel;\n"
    "    public void Hit(int dmg, bool crit) { }\n"
    "    void Fly(float speed) { }\n"
    "}\n";

static void test_complete(void) {
    MsService* svc = svc_new();
    char out[8][SVC_WORD], text[2048];
    int start;

    snprintf(text, sizeof text, "%sclass Game { static void Main() { Ship ship = new Ship(); ship.|} }", SHIP);
    int cur = open1(svc, text);
    int n = svc_complete(svc, 0, cur, &start, out, 8);
    check(n == 3 && start == cur, "after 'ship.': hp, Hit and Fly (no keyword = public)");
    if (n != 3) for (int i = 0; i < n; i++) printf("  %s\n", out[i]);
    int hasHit = 0, hasFuel = 0;
    for (int i = 0; i < n; i++) { hasHit |= !strcmp(out[i], "Hit"); hasFuel |= !strcmp(out[i], "fuel"); }
    check(hasHit && !hasFuel, "Hit yes, private fuel no");

    snprintf(text, sizeof text, "%sclass Game { static void Main() { int counter = 1; cou| } }", SHIP);
    cur = open1(svc, text);
    n = svc_complete(svc, 0, cur, &start, out, 8);
    check(n >= 1 && !strcmp(out[0], "counter") && start == cur - 3, "a name from the file first");

    snprintf(text, sizeof text, "%sclass Game { static void Main() { Sh| } }", SHIP);
    cur = open1(svc, text);
    n = svc_complete(svc, 0, cur, &start, out, 8);
    check(n >= 1 && !strcmp(out[0], "Ship"), "the project's class");

    cur = open1(svc, "class Game { static void Main() { Conso| } }");
    n = svc_complete(svc, 0, cur, &start, out, 8);
    check(n == 1 && !strcmp(out[0], "Console"), "a built-in class");

    cur = open1(svc, "class Game { static void Main() { Console.Wr| } }");
    n = svc_complete(svc, 0, cur, &start, out, 8);
    check(n == 2 && start == cur - 2, "members of a built-in class: Write, WriteLine");

    cur = open1(svc, "class Game { static void Main() { fore| } }");
    n = svc_complete(svc, 0, cur, &start, out, 8);
    check(n == 1 && !strcmp(out[0], "foreach"), "a keyword");

    cur = open1(svc, "class Game { static void Main() { string s = \"helloWorld\"; hel| } }");
    n = svc_complete(svc, 0, cur, &start, out, 8);
    check(n == 0, "no words from strings");

    cur = open1(svc, "class Game { static void Main() { int a = 1|; } }");
    n = svc_complete(svc, 0, cur, &start, out, 8);
    check(n == 0, "nothing after a number");

    check(svc_complete(svc, 5, 0, &start, out, 8) == 0, "unknown file");
    svc_free(svc);
}

static void test_signature(void) {
    MsService* svc = svc_new();
    char sig[128], text[2048];
    int arg;

    snprintf(text, sizeof text, "%sclass Game { static void Main() { Ship ship = new Ship(); ship.Hit(3, |", SHIP);
    int cur = open1(svc, text);
    check(svc_signature(svc, 0, cur, sig, sizeof sig, &arg) && !strcmp(sig, "void Hit(int dmg, bool crit)") && arg == 1,
          "own method, second argument");

    snprintf(text, sizeof text, "%sclass Game { static void Main() { Ship ship = new Ship(); ship.Hit(Math.Max(1, 2), |", SHIP);
    cur = open1(svc, text);
    check(svc_signature(svc, 0, cur, sig, sizeof sig, &arg) && arg == 1 && strstr(sig, "Hit"), "nested call closed");

    snprintf(text, sizeof text, "%sclass Game { static void Main() { Ship ship = new Ship(); ship.Hit(Math.Max(1, |", SHIP);
    cur = open1(svc, text);
    check(svc_signature(svc, 0, cur, sig, sizeof sig, &arg) && arg == 1 && strstr(sig, "Max("), "inner call: Math.Max");

    snprintf(text, sizeof text, "%sclass Game { static void Main() { Ship ship = new Ship(); ship.Hit(\"a, (b\", |", SHIP);
    cur = open1(svc, text);
    check(svc_signature(svc, 0, cur, sig, sizeof sig, &arg) && arg == 1, "commas and brackets in a string do not count");

    snprintf(text, sizeof text, "%s", "class Ship { void Fly(float speed) { } void Go() { Fly(|");
    cur = open1(svc, text);
    check(svc_signature(svc, 0, cur, sig, sizeof sig, &arg) && !strcmp(sig, "void Fly(float speed)") && arg == 0,
          "method of the same class");

    cur = open1(svc, "class Game { static void Main() { Console.WriteLine(|");
    check(svc_signature(svc, 0, cur, sig, sizeof sig, &arg) && !strncmp(sig, "void WriteLine(", 15) && strstr(sig, "(+"),
          "built-in with overloads");

    cur = open1(svc, "class Game { static void Main() { string s = \"x\"; s.Substring(1, |");
    check(svc_signature(svc, 0, cur, sig, sizeof sig, &arg) && arg == 1 && strstr(sig, "Substring(int, int)"),
          "string method: without the string itself");

    cur = open1(svc, "class Game { static void Main() { if (a > 1|");
    check(!svc_signature(svc, 0, cur, sig, sizeof sig, &arg), "if ( is not a call");
    cur = open1(svc, "class Game { static void Main() { Nope(1, |");
    check(!svc_signature(svc, 0, cur, sig, sizeof sig, &arg), "unknown method");
    cur = open1(svc, "class Game { static void Main() { Console.WriteLine(1); |");
    check(!svc_signature(svc, 0, cur, sig, sizeof sig, &arg), "after the call");
    svc_free(svc);
}

static void test_line(void) {
    char out[8][SVC_WORD];
    int start;
    int n = svc_complete_line("x = Ma", 6, &start, out, 8, "max");
    check(n >= 2 && !strcmp(out[0], "max") && !strcmp(out[1], "Math") && start == 4, "caller's words, then classes");
    n = svc_complete_line("Math.Sq", 7, &start, out, 8, "");
    check(n == 1 && !strcmp(out[0], "Sqrt"), "built-in members");
}

int main(void) {
    lib_register_all();
    test_colors();
    test_complete();
    test_signature();
    test_line();
    if (!fails) printf("service: ok\n");
    return fails ? 1 : 0;
}
