/* MiniSharp - app/ui.h on the 3DS with citro2d and the system font (see ui_3ds.h). */
#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include "ui_3ds.h"

static C3D_RenderTarget* g_target[2];
static C2D_TextBuf g_buf, g_measure;   /* g_buf: texts of this frame; g_measure: only for widths */
static int g_cur;
static float g_fontPx = 30;             /* line height of the system font at scale 1 */

static u32 col(uint32_t rgb) { return C2D_Color32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255); }

static void s_select(int s) {
    if (s < 0 || s > 1) return;
    g_cur = s;
    C2D_SceneBegin(g_target[s]);
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

static void s_rect(float x, float y, float w, float h, uint32_t rgb) { C2D_DrawRectSolid(x, y, 0.5f, w, h, col(rgb)); }

static void s_text(float x, float y, const char* s, float size, uint32_t rgb) {
    C2D_Text t;
    C2D_TextParse(&t, g_buf, s);
    C2D_TextOptimize(&t);
    float sc = size / g_fontPx;
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, sc, sc, col(rgb));
}

static float s_width(const char* s, float size) {
    C2D_Text t;
    float w = 0, sc = size / g_fontPx;
    C2D_TextBufClear(g_measure);
    C2D_TextParse(&t, g_measure, s);
    C2D_TextGetDimensions(&t, sc, sc, &w, NULL);
    return w;
}

/* the framebuffers are rotated: the scissor box is in the 240-wide portrait coordinates */
static void s_clip(int x, int y, int w, int h) {
    C2D_Flush();
    if (w <= 0 || h <= 0) { C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0); return; }
    int sw = g_cur ? 320 : 400;
    int l = 240 - (y + h), t = sw - (x + w), r = 240 - y, b = sw - x;
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r < 0) r = 0;
    if (b < 0) b = 0;
    C3D_SetScissor(GPU_SCISSOR_NORMAL, l, t, r, b);
}

static const UiBackend BACKEND = { 2, 1, { 400, 320 }, { 240, 240 }, s_select, s_rect, s_text, s_width, s_clip };

int ui3ds_init(void) {
    /* the text console switched the screens to RGB565 without double buffering; citro2d wants them as at start */
    gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return 0;
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) { C3D_Fini(); return 0; }
    C2D_Prepare();
    g_target[0] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_target[1] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_buf = C2D_TextBufNew(8192);
    g_measure = C2D_TextBufNew(256);
    CFNT_s* font = fontGetSystemFont();
    if (font) {
        FINF_s* info = fontGetInfo(font);
        if (info && info->lineFeed) g_fontPx = info->lineFeed;
    }
    ui_set_backend(&BACKEND);
    return 1;
}

void ui3ds_exit(void) {
    ui_set_backend(NULL);
    C2D_TextBufDelete(g_buf);
    C2D_TextBufDelete(g_measure);
    C3D_RenderTargetDelete(g_target[0]);
    C3D_RenderTargetDelete(g_target[1]);
    C2D_Fini();
    C3D_Fini();
}

void ui3ds_frame_begin(uint32_t topBg, uint32_t bottomBg) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(g_buf);
    C2D_TargetClear(g_target[0], col(topBg));
    C2D_TargetClear(g_target[1], col(bottomBg));
    s_select(0);
}

void ui3ds_frame_end(void) { C3D_FrameEnd(0); }

static uint32_t map_keys(u32 k) {
    static const struct { u32 from; uint32_t to; } MAP[] = {
        { KEY_A, UI_BTN_A }, { KEY_B, UI_BTN_B }, { KEY_X, UI_BTN_X }, { KEY_Y, UI_BTN_Y },
        { KEY_L, UI_BTN_L }, { KEY_R, UI_BTN_R }, { KEY_START, UI_BTN_START }, { KEY_SELECT, UI_BTN_SELECT },
        { KEY_UP, UI_BTN_UP }, { KEY_DOWN, UI_BTN_DOWN }, { KEY_LEFT, UI_BTN_LEFT }, { KEY_RIGHT, UI_BTN_RIGHT },
    };
    uint32_t r = 0;
    for (size_t i = 0; i < sizeof MAP / sizeof MAP[0]; i++)
        if (k & MAP[i].from) r |= MAP[i].to;
    return r;
}

void ui3ds_input(UiInput* in) {
    u32 down = hidKeysDown(), held = hidKeysHeld();
    touchPosition tp;
    hidTouchRead(&tp);
    memset(in, 0, sizeof *in);
    in->down = map_keys(down);
    in->held = map_keys(held);
    in->touching = (held & KEY_TOUCH) != 0;
    in->touchDown = (down & KEY_TOUCH) != 0;
    in->x = tp.px;
    in->y = tp.py;
    in->ms = (uint32_t)osGetTime();
}
