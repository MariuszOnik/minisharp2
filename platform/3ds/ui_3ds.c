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

/* ---------- lib/gfx.h: the colours come with alpha (0xAARRGGBB) ---------- */
static u32 acol(uint32_t argb) { return C2D_Color32((argb >> 16) & 255, (argb >> 8) & 255, argb & 255, (argb >> 24) & 255); }
static void g_clear(uint32_t argb) { C2D_DrawRectSolid(0, 0, 0.5f, g_cur ? 320 : 400, 240, acol(argb)); }
static void g_rect(float x, float y, float w, float h, uint32_t argb) { C2D_DrawRectSolid(x, y, 0.5f, w, h, acol(argb)); }
static void g_circle(float x, float y, float r, uint32_t argb) { C2D_DrawCircleSolid(x, y, 0.5f, r, acol(argb)); }
static void g_line(float x1, float y1, float x2, float y2, float w, uint32_t argb) {
    u32 c = acol(argb);
    C2D_DrawLine(x1, y1, c, x2, y2, c, w < 1 ? 1 : w, 0.5f);
}
static void g_text(float x, float y, const char* s, float size, uint32_t argb) {
    C2D_Text t;
    C2D_TextParse(&t, g_buf, s);                    /* a full buffer of this frame parses only what fits */
    C2D_TextOptimize(&t);
    float sc = size / g_fontPx;
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, sc, sc, acol(argb));
}
/* pictures: GPU textures (RGBA8, sides a power of two from 8 to 1024) in the 3DS tiled order, rows from the bottom */
#define MAX_PICS 256
typedef struct { C3D_Tex tex; int w, h, tw, th; } Pic;
static Pic g_pics[MAX_PICS];
static int g_npics;

static int pow2(int n) { int p = 8; while (p < n) p <<= 1; return p; }

static int g_image_create(const unsigned char* rgba, int w, int h) {
    if (g_npics == MAX_PICS || w <= 0 || h <= 0 || w > 1024 || h > 1024) return -1;
    Pic* p = &g_pics[g_npics];
    int tw = pow2(w), th = pow2(h);
    if (!C3D_TexInit(&p->tex, (u16)tw, (u16)th, GPU_RGBA8)) return -1;
    u32* dst = (u32*)p->tex.data;
    memset(dst, 0, (size_t)tw * th * 4);
    for (int y = 0; y < h; y++) {
        int fy = th - 1 - y;                        /* the texture's first row is the picture's bottom */
        for (int x = 0; x < w; x++) {
            const unsigned char* c = rgba + ((size_t)y * w + x) * 4;
            u32 tile = (u32)(((fy >> 3) * (tw >> 3) + (x >> 3)) << 6);
            u32 in = (u32)((x & 1) | ((fy & 1) << 1) | ((x & 2) << 1) | ((fy & 2) << 2) | ((x & 4) << 2) | ((fy & 4) << 3));
            dst[tile + in] = ((u32)c[0] << 24) | ((u32)c[1] << 16) | ((u32)c[2] << 8) | c[3];   /* RGBA8: A B G R in memory */
        }
    }
    GSPGPU_FlushDataCache(p->tex.data, (u32)tw * th * 4);
    C3D_TexSetFilter(&p->tex, GPU_NEAREST, GPU_NEAREST);     /* pixel art stays sharp when scaled */
    C3D_TexSetWrap(&p->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    p->w = w; p->h = h; p->tw = tw; p->th = th;
    return g_npics++;
}

static void g_image(int id, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh, float angle, uint32_t argb) {
    if (id < 0 || id >= g_npics || dw == 0 || dh == 0) return;
    Pic* p = &g_pics[id];
    Tex3DS_SubTexture st;
    st.width = (u16)sw; st.height = (u16)sh;
    st.left = sx / p->tw; st.right = (sx + sw) / p->tw;
    st.top = 1.0f - sy / p->th; st.bottom = 1.0f - (sy + sh) / p->th;
    C2D_Image img = { &p->tex, &st };
    float aw = dw < 0 ? -dw : dw, ah = dh < 0 ? -dh : dh;
    C2D_DrawParams pr;
    pr.pos.x = dx + aw / 2; pr.pos.y = dy + ah / 2; pr.pos.w = dw; pr.pos.h = dh;   /* a negative size mirrors */
    pr.center.x = dw / 2; pr.center.y = dh / 2;
    pr.depth = 0.5f; pr.angle = angle;
    unsigned a = argb >> 24;
    if (a >= 255) { C2D_DrawImage(img, &pr, NULL); return; }
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, a / 255.0f);
    C2D_DrawImage(img, &pr, &tint);
}

static void g_image_free_all(void) {
    for (int i = 0; i < g_npics; i++) C3D_TexDelete(&g_pics[i].tex);
    g_npics = 0;
}

static const GfxBackend GFX = { 2, { 400, 320 }, { 240, 240 }, s_select, g_clear, g_rect, g_circle, g_line, g_text, s_width,
                                g_image_create, g_image, g_image_free_all };
const GfxBackend* ui3ds_gfx(void) { return &GFX; }

int ui3ds_init(void) {
    /* the text console switched the screens to RGB565 without double buffering; citro2d wants them as at start */
    gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return 0;
    if (!C2D_Init(8192)) { C3D_Fini(); return 0; }    /* games draw many small things: twice the default */
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
