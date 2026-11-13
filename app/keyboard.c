/* MiniSharp - touch keyboard (see keyboard.h). Moved from the old source3ds/keyboard.c: the same layers and repeat,
 * but drawn through app/ui.h, with a scalable area instead of fixed 32-pixel keys and the time taken from UiInput. */
#include "keyboard.h"
#include <string.h>

#define ROWS 5
#define UNITS 10                        /* a row is 10 units wide; SPACE takes 3 */
#define MAX_KEYS 16

/* keys separated by spaces; the special ones are written in capitals */
static const char* LAYERS[3][ROWS] = {
    { "1 2 3 4 5 6 7 8 9 0", "q w e r t y u i o p", "a s d f g h j k l ;",
      "SHIFT z x c v b n m . BKSP", "SYM ( ) \" SPACE = , ENTER" },
    { "! ? # $ % ^ & * - +", "Q W E R T Y U I O P", "A S D F G H J K L :",
      "SHIFT Z X C V B N M _ BKSP", "SYM < > ' SPACE / | ENTER" },
    { "+ - * / % = < > ! ?", "( ) [ ] { } & | ^ ~", "\" ' ; : , . _ # @ \\",
      "+= == != <= >= && || ++ -- BKSP", "ABC ( ) \" SPACE = , ENTER" },
};

#define REPEAT_WAIT 450                 /* ms before a held key starts repeating */
#define REPEAT_EVERY 60

enum { SHIFT_OFF, SHIFT_ONCE, SHIFT_LOCK };
static int g_layer, g_shift;            /* g_layer: 0 letters, 2 symbols; capitals come from g_shift */
static int g_pressed = -1;              /* key under the finger: row * MAX_KEYS + index */
static uint32_t g_pressTime, g_lastRepeat;
static const char* g_enterLabel = "Run";
static float g_x = 0, g_y = 80, g_w = 320, g_h = 160;

typedef struct { char word[8]; float x, w; } Key;      /* x, w in units */

static int parse_row(const char* row, Key* keys) {
    int n = 0;
    float x = 0;
    while (*row && n < MAX_KEYS) {
        while (*row == ' ') row++;
        if (!*row) break;
        int len = 0;
        while (row[len] && row[len] != ' ') len++;
        Key* k = &keys[n++];
        int c = len > 7 ? 7 : len;
        memcpy(k->word, row, c); k->word[c] = 0;
        k->w = !strcmp(k->word, "SPACE") ? 3 : 1;
        k->x = x; x += k->w;
        row += len;
    }
    return n;
}

static int cur_layer(void) { return g_layer == 2 ? 2 : g_shift ? 1 : 0; }
int kbd_layer(void) { return cur_layer(); }
int kbd_shift(void) { return g_shift; }

static int is_special(const char* w) { return strlen(w) > 2 && w[0] >= 'A' && w[0] <= 'Z' && w[1] >= 'A' && w[1] <= 'Z'; }

static const char* label(const char* w) {
    if (!strcmp(w, "SHIFT")) return "Shift";
    if (!strcmp(w, "BKSP")) return "Del";
    if (!strcmp(w, "SYM")) return "#+=";
    if (!strcmp(w, "ABC")) return "abc";
    if (!strcmp(w, "SPACE")) return "space";
    if (!strcmp(w, "ENTER")) return g_enterLabel;
    return w;
}

void kbd_set_enter_label(const char* s) { g_enterLabel = s; }
void kbd_set_area(float x, float y, float w, float h) { g_x = x; g_y = y; g_w = w; g_h = h; }
void kbd_reset(void) { g_layer = 0; g_shift = SHIFT_OFF; g_pressed = -1; }

/* what pressing the key does; NULL for keys that only switch layers */
static const char* press(const Key* k) {
    static char out[8];
    const char* w = k->word;
    if (!strcmp(w, "SHIFT")) { g_shift = (g_shift + 1) % 3; return NULL; }
    if (!strcmp(w, "SYM")) { g_layer = 2; return NULL; }
    if (!strcmp(w, "ABC")) { g_layer = 0; return NULL; }
    if (!strcmp(w, "BKSP")) return KBD_BACKSPACE;
    if (!strcmp(w, "ENTER")) return KBD_ENTER;
    if (!strcmp(w, "SPACE")) return " ";
    strcpy(out, w);
    if (g_shift == SHIFT_ONCE && g_layer != 2) g_shift = SHIFT_OFF;
    return out;
}

static float unit_w(void) { return g_w / UNITS; }
static float row_h(void) { return g_h / ROWS; }

static int key_at(float px, float py, Key* key) {
    if (px < g_x || py < g_y || px >= g_x + g_w || py >= g_y + g_h) return -1;
    int row = (int)((py - g_y) / row_h());
    if (row < 0 || row >= ROWS) return -1;
    float u = (px - g_x) / unit_w();
    Key keys[MAX_KEYS];
    int n = parse_row(LAYERS[cur_layer()][row], keys);
    for (int i = 0; i < n; i++)
        if (u >= keys[i].x && u < keys[i].x + keys[i].w) { *key = keys[i]; return row * MAX_KEYS + i; }
    return -1;
}

int kbd_key_center(const char* word, float* x, float* y) {
    Key keys[MAX_KEYS];
    for (int row = 0; row < ROWS; row++) {
        int n = parse_row(LAYERS[cur_layer()][row], keys);
        for (int i = 0; i < n; i++)
            if (!strcmp(keys[i].word, word)) {
                *x = g_x + (keys[i].x + keys[i].w / 2) * unit_w();
                *y = g_y + (row + 0.5f) * row_h();
                return 1;
            }
    }
    return 0;
}

const char* kbd_update(const UiInput* in) {
    if (!in->touching) { g_pressed = -1; return NULL; }
    Key k;
    int idx = key_at((float)in->x, (float)in->y, &k);
    if (in->touchDown) {
        g_pressed = idx;
        g_pressTime = g_lastRepeat = in->ms;
        return idx >= 0 ? press(&k) : NULL;
    }
    /* held: repeat letters, Del and space (not the layer keys or Enter) after a short wait; sliding off cancels */
    if (idx != g_pressed) { g_pressed = -1; return NULL; }
    if (idx < 0 || (is_special(k.word) && strcmp(k.word, "BKSP") && strcmp(k.word, "SPACE"))) return NULL;
    if (in->ms - g_pressTime < REPEAT_WAIT || in->ms - g_lastRepeat < REPEAT_EVERY) return NULL;
    g_lastRepeat = in->ms;
    return press(&k);
}

static void draw_key(const Key* k, int row, int pressed) {
    float uw = unit_w(), rh = row_h();
    float x = g_x + k->x * uw, y = g_y + row * rh, w = k->w * uw;
    int special = is_special(k->word);
    uint32_t bg = special ? 0x34405A : 0x2A2E36;
    if (!strcmp(k->word, "ENTER")) bg = 0x2F6B3A;
    if (!strcmp(k->word, "SHIFT") && g_shift) bg = g_shift == SHIFT_LOCK ? 0xC08030 : 0x5078C0;
    if (pressed) bg = 0x6A90D8;
    ui_rect(x + 1, y + 1, w - 2, rh - 2, bg);
    const char* s = label(k->word);
    float size = rh * (special ? 0.42f : 0.56f);
    float tw = ui_text_width(s, size);
    ui_text(x + (w - tw) / 2, y + (rh - size) / 2, s, size, 0xFFFFFF);
}

void kbd_draw(void) {
    int layer = cur_layer();
    Key keys[MAX_KEYS];
    ui_rect(g_x, g_y, g_w, g_h, 0x101216);
    for (int row = 0; row < ROWS; row++) {
        int n = parse_row(LAYERS[layer][row], keys);
        for (int i = 0; i < n; i++) draw_key(&keys[i], row, g_pressed == row * MAX_KEYS + i);
    }
}
