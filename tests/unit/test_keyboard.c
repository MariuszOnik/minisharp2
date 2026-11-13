/* Unit test: the touch keyboard (app/keyboard.c) through a recording ui.h backend – layers, Shift, repeat, hits
 * in a scaled area, what is drawn. Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "keyboard.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

/* --- recording backend --- */
static int nRect, nText, nSelect;
static char texts[2048];
static float lastRectW;
static uint32_t colorOf[64];            /* background of the key drawn before each text (to see the pressed one) */
static char textOf[64][8];
static uint32_t lastRgb;

static void b_select(int s) { (void)s; nSelect++; }
static void b_rect(float x, float y, float w, float h, uint32_t rgb) { (void)x; (void)y; (void)h; nRect++; lastRectW = w; lastRgb = rgb; }
static void b_text(float x, float y, const char* s, float size, uint32_t rgb) {
    (void)x; (void)y; (void)size; (void)rgb;
    if (nText < 64) { colorOf[nText] = lastRgb; snprintf(textOf[nText], 8, "%s", s); }
    nText++;
    strncat(texts, s, sizeof texts - strlen(texts) - 2);
    strcat(texts, "|");
}
static float b_width(const char* s, float size) { return strlen(s) * size / 2; }
static const UiBackend REC = { 2, 1, { 400, 320 }, { 240, 240 }, b_select, b_rect, b_text, b_width, NULL };

static void reset_rec(void) { nRect = nText = nSelect = 0; texts[0] = 0; }

/* --- touch helpers --- */
static uint32_t now;

static const char* touch(float x, float y, int down) {
    UiInput in = { 0, 0, 1, down, (int)x, (int)y, now };
    return kbd_update(&in);
}
static void release(void) { UiInput in = { 0 }; in.ms = now; kbd_update(&in); }

/* taps the key with this word on the current layer; returns what it typed ("" = nothing) */
static const char* tap(const char* word) {
    static char out[16];
    float x, y;
    if (!kbd_key_center(word, &x, &y)) { printf("FAIL: no key '%s' on layer %d\n", word, kbd_layer()); fails++; return ""; }
    const char* r = touch(x, y, 1);
    snprintf(out, sizeof out, "%s", r ? r : "");
    now += 100;
    release();
    now += 100;
    return out;
}

/* types the words (separated by spaces) and returns everything typed */
static const char* type(const char* words) {
    static char out[256];
    char w[16];
    out[0] = 0;
    while (*words) {
        int n = 0;
        while (*words == ' ') words++;
        while (*words && *words != ' ' && n < 15) w[n++] = *words++;
        w[n] = 0;
        if (n) strcat(out, tap(w));
    }
    return out;
}

static void test_typing(void) {
    kbd_reset();
    check(!strcmp(type("h i SPACE 4 2 ."), "hi 42."), "letters, space, digits");
    check(!strcmp(type("BKSP ENTER"), "\b\n"), "Del and Enter");
    check(!strcmp(type("SHIFT A b"), "Ab"), "Shift once: one capital");
    check(kbd_shift() == 0, "Shift once turns off");
    check(!strcmp(type("SHIFT SHIFT A B"), "AB"), "Shift twice: locked");
    check(kbd_shift() == 2 && kbd_layer() == 1, "locked Shift stays");
    check(!strcmp(type("SHIFT c"), "c"), "third Shift: off");
    check(!strcmp(type("SYM += { } && \\ ABC x"), "+={}&&\\x"), "symbols layer and back");
    check(!strcmp(type("SHIFT SYM ( ABC"), "("), "symbols with Shift on");
    check(kbd_layer() == 1, "Shift survives the symbols layer");
    kbd_reset();
    check(kbd_layer() == 0 && kbd_shift() == 0, "reset");
}

static void test_hits(void) {
    kbd_reset();
    now = 1000;
    check(touch(160, 79, 1) == NULL, "above the keyboard: nothing");
    release();
    check(touch(160, 240, 1) == NULL, "below the keyboard: nothing");
    release();
    const char* r = touch(0, 80, 1);                  /* top-left corner = "1" */
    check(r && !strcmp(r, "1"), "corner key");
    release();
    r = touch(319, 239, 1);                           /* bottom-right = Enter */
    check(r && !strcmp(r, KBD_ENTER), "last key");
    release();
    r = touch(4 * 32 + 1, 80 + 4 * 32 + 5, 1);        /* the 3-unit space starts at unit 4 of the last row */
    check(r && !strcmp(r, " "), "wide space key, left edge");
    release();
    r = touch(7 * 32 - 1, 80 + 4 * 32 + 5, 1);
    check(r && !strcmp(r, " "), "wide space key, right edge");
    release();
    /* scaled area (e.g. a bigger screen): the same keys twice as big */
    kbd_set_area(100, 0, 640, 320);
    r = touch(100 + 64 * 2 + 10, 64 + 10, 1);         /* row 1, key 2 = "e" */
    check(r && !strcmp(r, "e"), "scaled area");
    release();
    check(touch(99, 64, 1) == NULL, "left of the scaled area");
    release();
    kbd_set_area(0, 80, 320, 160);
}

static void test_repeat(void) {
    kbd_reset();
    float x, y;
    now = 5000;
    kbd_key_center("BKSP", &x, &y);
    int n = 0;
    if (touch(x, y, 1)) n++;
    for (int t = 0; t < 1000; t += 10) { now += 10; if (touch(x, y, 0)) n++; }
    /* first press, then from 450 ms every 60 ms: 450, 510, ..., 990 = 10 repeats */
    check(n == 11, "Del repeats while held");
    release();

    int shiftChanges = 0, before = kbd_shift();
    kbd_key_center("SHIFT", &x, &y);
    touch(x, y, 1);
    for (int t = 0; t < 1000; t += 10) { now += 10; touch(x, y, 0); if (kbd_shift() != before + 1) shiftChanges++; }
    check(shiftChanges == 0, "Shift does not repeat");
    release();
    kbd_reset();

    /* sliding off the key cancels the repeat, even when coming back */
    float x2, y2;
    kbd_key_center("a", &x, &y);
    kbd_key_center("s", &x2, &y2);
    n = 0;
    if (touch(x, y, 1)) n++;
    now += 100; touch(x2, y2, 0);
    for (int t = 0; t < 1000; t += 10) { now += 10; if (touch(x, y, 0)) n++; }
    check(n == 1, "sliding off cancels the repeat");
    release();
}

static void test_draw(void) {
    kbd_reset();
    ui_set_backend(&REC);
    reset_rec();
    kbd_draw();
    check(nRect == 1 + 48, "background + 48 keys (space = 3 units)");
    check(nText == 48, "48 labels");
    check(strstr(texts, "|Shift|") && strstr(texts, "|Del|") && strstr(texts, "|space|") && strstr(texts, "|Run|"),
          "special labels");
    check(strstr(texts, "|#+=|") != NULL, "symbols key label");

    kbd_set_enter_label("Enter");
    reset_rec();
    kbd_draw();
    check(strstr(texts, "|Enter|") && !strstr(texts, "|Run|"), "Enter label");

    /* the key under the finger is drawn in another colour */
    float x, y;
    kbd_key_center("q", &x, &y);
    touch(x, y, 1);
    reset_rec();
    kbd_draw();
    int pressed = -1, other = -1;
    for (int i = 0; i < nText && i < 64; i++) {
        if (!strcmp(textOf[i], "q")) pressed = (int)colorOf[i];
        if (!strcmp(textOf[i], "w")) other = (int)colorOf[i];
    }
    check(pressed >= 0 && other >= 0 && pressed != other, "pressed key highlighted");
    release();

    kbd_key_center("SHIFT", &x, &y);
    touch(x, y, 1);
    release();
    reset_rec();
    kbd_draw();
    check(strstr(texts, "|Q|") != NULL, "capitals drawn after Shift");
    kbd_set_enter_label("Run");
    ui_set_backend(NULL);
    reset_rec();
    kbd_draw();                                        /* no backend: nothing happens, no crash */
    check(nRect == 0 && nText == 0, "no backend");
}

int main(void) {
    test_typing();
    test_hits();
    test_repeat();
    test_draw();
    if (!fails) printf("keyboard: ok\n");
    return fails ? 1 : 0;
}
