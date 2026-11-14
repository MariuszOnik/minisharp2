/* MiniSharp - the PC test host's screens: lib/gfx.h drawn in memory, two screens like the 3DS (400x240 on top,
 * 320x240 below), so a test can check a picture by its hash or save it as a PPM file. Text is drawn as a box per
 * character (the real font is the platform's business; tests only need something stable). */
#include "swgfx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t* fb[GFX_MAX_SCREENS];   /* 0x00RRGGBB */
static int cur;

static const int W[2] = { 400, 320 }, H[2] = { 240, 240 };

static void blend(int x, int y, uint32_t argb) {
    if (x < 0 || y < 0 || x >= W[cur] || y >= H[cur]) return;
    uint32_t* p = &fb[cur][y * W[cur] + x];
    unsigned a = argb >> 24;
    if (a == 255) { *p = argb & 0xFFFFFF; return; }
    if (a == 0) return;
    uint32_t d = *p, r = 0;
    for (int sh = 0; sh <= 16; sh += 8) {
        unsigned s = (argb >> sh) & 255, t = (d >> sh) & 255;
        r |= ((s * a + t * (255 - a) + 127) / 255) << sh;
    }
    *p = r;
}

static void s_select(int s) { if (s >= 0 && s < 2) cur = s; }
static void s_clear(uint32_t argb) { for (int y = 0; y < H[cur]; y++) for (int x = 0; x < W[cur]; x++) blend(x, y, argb); }

/* pixels whose centres are inside [x, x + w) x [y, y + h) */
static void s_rect(float x, float y, float w, float h, uint32_t argb) {
    int x0 = (int)ceilf(x - 0.5f), x1 = (int)ceilf(x + w - 0.5f), y0 = (int)ceilf(y - 0.5f), y1 = (int)ceilf(y + h - 0.5f);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W[cur]) x1 = W[cur];
    if (y1 > H[cur]) y1 = H[cur];
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) blend(px, py, argb);
}

static void s_circle(float cx, float cy, float r, uint32_t argb) {
    int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r), y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
    for (int py = y0; py <= y1; py++)
        for (int px = x0; px <= x1; px++) {
            float dx = px + 0.5f - cx, dy = py + 0.5f - cy;
            if (dx * dx + dy * dy <= r * r) blend(px, py, argb);
        }
}

static void s_line(float x1, float y1, float x2, float y2, float width, uint32_t argb) {
    float dx = x2 - x1, dy = y2 - y1;
    int steps = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)));
    if (steps < 1) steps = 1;
    if (width < 1) width = 1;
    int lastX = 1 << 30, lastY = 1 << 30;
    for (int i = 0; i <= steps; i++) {
        float x = x1 + dx * i / steps, y = y1 + dy * i / steps;
        int px = (int)floorf(x - width / 2 + 0.5f), py = (int)floorf(y - width / 2 + 0.5f);
        if (px == lastX && py == lastY) continue;     /* each square once: transparent lines stay even */
        lastX = px; lastY = py;
        s_rect((float)px, (float)py, width, width, argb);
    }
}

static float s_width(const char* s, float size) {
    int n = 0;
    for (; *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) n++;     /* characters, not UTF-8 bytes */
    return n * size * 0.5f;
}

static void s_text(float x, float y, const char* s, float size, uint32_t argb) {
    float cw = size * 0.5f;
    for (; *s; s++) {
        if (((unsigned char)*s & 0xC0) == 0x80) continue;
        if (*s != ' ') s_rect(x + cw * 0.15f, y + size * 0.2f, cw * 0.7f, size * 0.6f, argb);
        x += cw;
    }
}

/* ---------- pictures: kept as RGBA, drawn by mapping every screen pixel back into the picture (nearest pixel) ---------- */
#define MAX_PICS 256
typedef struct { unsigned char* px; int w, h; } Pic;
static Pic pics[MAX_PICS];
static int npics;

static int s_image_create(const unsigned char* rgba, int w, int h) {
    if (npics == MAX_PICS || w <= 0 || h <= 0 || w > 4096 || h > 4096) return -1;
    unsigned char* c = (unsigned char*)malloc((size_t)w * h * 4);
    if (!c) return -1;
    memcpy(c, rgba, (size_t)w * h * 4);
    pics[npics].px = c; pics[npics].w = w; pics[npics].h = h;
    return npics++;
}

static void s_image_free_all(void) {
    for (int i = 0; i < npics; i++) free(pics[i].px);
    npics = 0;
}

static void s_image(int id, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh, float angle, uint32_t argb) {
    if (id < 0 || id >= npics || dw == 0 || dh == 0 || sw <= 0 || sh <= 0) return;
    Pic* p = &pics[id];
    float cx = dx + fabsf(dw) / 2, cy = dy + fabsf(dh) / 2;
    float ca = cosf(angle), sa = sinf(angle);
    float rad = sqrtf(dw * dw + dh * dh) / 2 + 1;
    int x0 = (int)floorf(cx - rad), x1 = (int)ceilf(cx + rad), y0 = (int)floorf(cy - rad), y1 = (int)ceilf(cy + rad);
    unsigned ta = argb >> 24;
    for (int py = y0; py <= y1; py++)
        for (int px = x0; px <= x1; px++) {
            float ox = px + 0.5f - cx, oy = py + 0.5f - cy;
            float u = ox * ca + oy * sa, v = -ox * sa + oy * ca;   /* turned back by the angle */
            float fu = u / dw + 0.5f, fv = v / dh + 0.5f;          /* 0..1 across the rectangle (mirrored when dw < 0) */
            if (fu < 0 || fu >= 1 || fv < 0 || fv >= 1) continue;
            int ix = (int)(sx + fu * sw), iy = (int)(sy + fv * sh);
            if (ix < 0 || iy < 0 || ix >= p->w || iy >= p->h) continue;
            const unsigned char* c = p->px + ((size_t)iy * p->w + ix) * 4;
            unsigned a = c[3] * ta / 255;
            if (a) blend(px, py, (a << 24) | ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | c[2]);
        }
}

static const GfxBackend BACKEND = { 2, { 400, 320 }, { 240, 240 }, s_select, s_clear, s_rect, s_circle, s_line, s_text, s_width,
                                     s_image_create, s_image, s_image_free_all, 1 };

const GfxBackend* swgfx_init(void) {
    for (int i = 0; i < 2; i++) {
        free(fb[i]);
        fb[i] = (uint32_t*)calloc((size_t)W[i] * H[i], sizeof(uint32_t));
        if (!fb[i]) return NULL;
    }
    cur = 0;
    return &BACKEND;
}

void swgfx_free(void) {
    s_image_free_all();
    for (int i = 0; i < 2; i++) { free(fb[i]); fb[i] = NULL; }
}

void swgfx_begin_frame(void) {      /* like the 3DS: every frame starts black */
    for (int i = 0; i < 2; i++) if (fb[i]) memset(fb[i], 0, sizeof(uint32_t) * (size_t)W[i] * H[i]);
}

uint32_t swgfx_pixel(int screen, int x, int y) {
    if (screen < 0 || screen > 1 || !fb[screen] || x < 0 || y < 0 || x >= W[screen] || y >= H[screen]) return 0;
    return fb[screen][y * W[screen] + x];
}

uint32_t swgfx_hash(void) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 2; i++)
        for (int k = 0; fb[i] && k < W[i] * H[i]; k++)
            for (int sh = 0; sh <= 16; sh += 8) h = (h ^ ((fb[i][k] >> sh) & 255)) * 16777619u;
    return h;
}

/* both screens one under the other, 400 x 480; the bottom one in the middle, like on the console */
int swgfx_save_ppm(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "P6\n400 480\n255\n");
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 400; x++) {
            uint32_t c = y < 240 ? swgfx_pixel(0, x, y) : (x >= 40 && x < 360 ? swgfx_pixel(1, x - 40, y - 240) : 0x202020);
            unsigned char px[3] = { (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c };
            fwrite(px, 1, 3, f);
        }
    return fclose(f) == 0;
}
