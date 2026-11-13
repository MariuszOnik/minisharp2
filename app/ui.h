/* MiniSharp - the drawing and input interface of the native app (terminal, later the editor, menu and graph editor).
 * app/ never calls libctru or citro2d: a platform (platform/3ds, later e.g. platform/switch) fills a UiBackend with
 * its functions and calls ui_set_backend once. Unit tests on the PC set their own backend that records the calls.
 * Coordinates are pixels of the selected screen, (0, 0) = top left; colours are 0xRRGGBB.
 * Input is not polled here: the platform reads it once per frame into a UiInput and passes it to whoever needs it. */
#ifndef MS_UI_H
#define MS_UI_H

#include <stdint.h>

#define UI_MAX_SCREENS 2

/* buttons in UiInput.down / held (a platform maps its own: 3DS A/B/X/Y..., a PC keyboard in tests) */
enum {
    UI_BTN_A = 1 << 0, UI_BTN_B = 1 << 1, UI_BTN_X = 1 << 2, UI_BTN_Y = 1 << 3,
    UI_BTN_L = 1 << 4, UI_BTN_R = 1 << 5, UI_BTN_START = 1 << 6, UI_BTN_SELECT = 1 << 7,
    UI_BTN_UP = 1 << 8, UI_BTN_DOWN = 1 << 9, UI_BTN_LEFT = 1 << 10, UI_BTN_RIGHT = 1 << 11,
};

typedef struct {
    uint32_t down, held;        /* buttons pressed this frame / held now */
    int touching;               /* finger (stylus, mouse button) on the touch screen */
    int touchDown;              /* ... and it touched this frame */
    int x, y;                   /* where, in pixels of the touch screen */
    uint32_t ms;                /* time in milliseconds (any start), for key repeat */
} UiInput;

typedef struct {
    int screens;                        /* 1 or 2; the layout is a parameter, not "top and bottom" */
    int touchScreen;                    /* which screen gets the touch (-1 = none) */
    int w[UI_MAX_SCREENS], h[UI_MAX_SCREENS];
    void (*select)(int screen);         /* the following calls draw on this screen */
    void (*rect)(float x, float y, float w, float h, uint32_t rgb);
    /* text in one line; size = line height in pixels, y = top of the line */
    void (*text)(float x, float y, const char* s, float size, uint32_t rgb);
    float (*textWidth)(const char* s, float size);
    void (*clip)(int x, int y, int w, int h);   /* draw only inside; w <= 0 turns it off */
} UiBackend;

void ui_set_backend(const UiBackend* b);
const UiBackend* ui_backend(void);

/* shortcuts to the backend (do nothing / return 0 without one) */
void ui_screen(int screen);
void ui_rect(float x, float y, float w, float h, uint32_t rgb);
void ui_text(float x, float y, const char* s, float size, uint32_t rgb);
float ui_text_width(const char* s, float size);
void ui_clip(int x, int y, int w, int h);
void ui_unclip(void);

#endif
