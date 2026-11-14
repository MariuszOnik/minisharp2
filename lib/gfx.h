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
} GfxBackend;

/* NULL = no screen: drawing does nothing, Gfx.Width/Height give 0 */
void lib_set_gfx(const GfxBackend* b);
const GfxBackend* lib_gfx(void);

/* the buttons (BTN_* bits, the same as Btn.* in MiniSharp) and the touch of this frame: called by the host once per
 * frame before Update; Pressed / Released come from the difference to the frame before */
enum {
    BTN_A = 1 << 0, BTN_B = 1 << 1, BTN_X = 1 << 2, BTN_Y = 1 << 3, BTN_L = 1 << 4, BTN_R = 1 << 5,
    BTN_START = 1 << 6, BTN_SELECT = 1 << 7, BTN_UP = 1 << 8, BTN_DOWN = 1 << 9, BTN_LEFT = 1 << 10, BTN_RIGHT = 1 << 11,
};
void lib_input_frame(uint32_t held, int touching, int x, int y);
/* Time.Delta (seconds since the last frame) and Time.Frame: the host calls it once per frame */
void lib_time_frame(float delta);
/* a new program: no buttons, frame 0, opaque drawing, the top screen */
void lib_gfx_reset(void);

void lib_register_gfx(void);        /* Gfx, Color, Screen, Input, Btn, Time, App (lib_register_all calls it) */

#endif
