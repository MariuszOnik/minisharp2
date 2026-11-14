/* MiniSharp - 2D graphics, input, time and App for games (PLAN.md, stage 3): the MiniSharp API (Gfx, Color, Screen,
 * Input, Btn, Time, App) is written once here; a platform only fills a GfxBackend (3DS: citro2d, PC tests: a software
 * framebuffer, web: a canvas) and passes the buttons of every frame with lib_input_frame.
 * Colours are 0xRRGGBB ints (Color.Red, Gfx.Rgb(255, 0, 0)); Gfx.Alpha(0..1) makes the next drawing transparent,
 * the backend gets 0xAARRGGBB. Coordinates are float pixels of the selected screen, (0, 0) = top left. */
#ifndef MS_GFX_H
#define MS_GFX_H

#include <stdint.h>

#define GFX_MAX_SCREENS 2

typedef struct {
    int screens;                                    /* 1 or 2 */
    int w[GFX_MAX_SCREENS], h[GFX_MAX_SCREENS];
    void (*select)(int screen);                     /* the following calls draw on this screen */
    void (*clear)(uint32_t argb);                   /* the whole selected screen */
    void (*rect)(float x, float y, float w, float h, uint32_t argb);
    void (*circle)(float x, float y, float r, uint32_t argb);       /* filled, centre x, y */
    void (*line)(float x1, float y1, float x2, float y2, float width, uint32_t argb);
    void (*text)(float x, float y, const char* s, float size, uint32_t argb);   /* size = line height, y = top */
    float (*textWidth)(const char* s, float size);
    /* pictures: imageCreate makes one from RGBA pixels (rows from the top) and gives its number, -1 = cannot;
     * image draws the part sx, sy, sw, sh of it into dx, dy, dw, dh (a negative dw / dh mirrors it), turned by
     * 'angle' radians around the centre of that rectangle; the alpha of argb is its transparency.
     * imageFreeAll forgets every picture (a new program). */
    int (*imageCreate)(const unsigned char* rgba, int w, int h);
    void (*image)(int id, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh, float angle, uint32_t argb);
    void (*imageFreeAll)(void);
    int touchScreen;                                /* the physical screen with the touch (-1 = none) */
} GfxBackend;

/* Gfx.Layout: the logical screens a program draws on (PLAN.md, decisions 2026-11-14). lib/gfx.c places them on the
 * backend's physical screens (scale and offset), so a platform only draws in its own pixels. */
enum { LAYOUT_TWO, LAYOUT_ONE, LAYOUT_FILL };

/* NULL = no screen: drawing does nothing, Gfx.Width/Height give 0 */
void lib_set_gfx(const GfxBackend* b);
const GfxBackend* lib_gfx(void);

/* the buttons (BTN_* bits, the same as Btn.* in MiniSharp) and the touch of this frame: called by the host once per
 * frame before Update; Pressed / Released come from the difference to the frame before */
enum {
    BTN_A = 1 << 0, BTN_B = 1 << 1, BTN_X = 1 << 2, BTN_Y = 1 << 3, BTN_L = 1 << 4, BTN_R = 1 << 5,
    BTN_START = 1 << 6, BTN_SELECT = 1 << 7, BTN_UP = 1 << 8, BTN_DOWN = 1 << 9, BTN_LEFT = 1 << 10, BTN_RIGHT = 1 << 11,
};
void lib_input_frame(uint32_t held, int touching, int x, int y);   /* x, y: pixels of the backend's touchScreen */
/* The game loop (PLAN.md, decisions 2026-11-14): Update runs 60 times a second on every device, Draw once per
 * refresh of the display. Per displayed frame the host asks lib_frame_steps(now in ms) how many Update steps to run
 * (0..4; the first call gives 1), and for each step calls lib_input_frame, Update, the coroutines and
 * lib_time_frame(1/60); then lib_draw_begin and Draw. A host with a fixed frame (PC tests) simply runs one step. */
int lib_frame_steps(double nowMs);
void lib_time_frame(float delta);       /* after an Update step: Time.Frame + 1 */
/* Time.Millis(): the real time in ms since the program started, on every platform and also without frames (a
 * benchmark in Main). The host gives its clock (3DS: osGetTime); default: the C clock(). lib_time_start marks
 * the start (lib_gfx_reset calls it). WaitForSeconds and Time.Delta stay on the game clock (ms_set_clock). */
void lib_set_real_clock(int (*millis)(void));
void lib_time_start(void);
void lib_draw_begin(void);
/* Gfx.FrameRate(n): how many times a second Draw runs (60, 30, 20 or 15); Update stays at 60 */
int lib_draw_rate(void);              /* before Draw: the first logical screen, opaque (Gfx.Screen / Alpha last a frame) */
/* a new program: no buttons, frame 0, opaque drawing, the top screen, no pictures */
void lib_gfx_reset(void);
/* the folder Gfx.LoadImage("hero.png") looks in: the program's folder (a project) or the folder of its .cs file */
void lib_gfx_set_dir(const char* dir);

void lib_register_gfx(void);        /* Gfx, Color, Screen, Input, Btn, Time, App (lib_register_all calls it) */

#endif
