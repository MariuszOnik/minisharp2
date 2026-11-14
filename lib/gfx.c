/* MiniSharp - Gfx, Color, Screen, Input, Btn, Time, App (see gfx.h). */
#include "gfx.h"
#include "libvalue.h"
#include "lib.h"
#include "image.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const GfxBackend* be;
static int screen;
static float alpha = 1.0f;
static uint32_t held, prevHeld;
static int touching, prevTouching, touchX, touchY;
static float delta = 1.0f / 60;
static int frame;

/* pictures: the number MiniSharp sees is an index here (1, 2, ...; 0 = none), loading the same file again gives it again */
#define MAX_IMAGES 256
typedef struct { int id, w, h; char path[600]; } Img;
static Img images[MAX_IMAGES];
static int nimages;
static char baseDir[300];

void lib_gfx_set_dir(const char* dir) { snprintf(baseDir, sizeof baseDir, "%s", dir ? dir : ""); }

static void forget_images(void) {
    if (nimages && be && be->imageFreeAll) be->imageFreeAll();
    nimages = 0;
}

void lib_set_gfx(const GfxBackend* b) {
    if (b != be) forget_images();               /* the pictures belong to the backend that made them */
    be = b; screen = 0;
    if (be && be->select) be->select(0);
}
const GfxBackend* lib_gfx(void) { return be; }

void lib_gfx_reset(void) {
    held = prevHeld = 0; touching = prevTouching = 0; touchX = touchY = 0;
    delta = 1.0f / 60; frame = 0; alpha = 1.0f; screen = 0;
    forget_images();
    if (be && be->select) be->select(0);
}

void lib_input_frame(uint32_t h, int t, int x, int y) {
    prevHeld = held; prevTouching = touching;
    held = h; touching = t;
    if (t) { touchX = x; touchY = y; }
}

void lib_time_frame(float d) { delta = d; frame++; }

/* ---------- Gfx ---------- */
static uint32_t argb(int rgb) {
    float a = alpha < 0 ? 0 : alpha > 1 ? 1 : alpha;
    return ((uint32_t)(a * 255.0f + 0.5f) << 24) | ((uint32_t)rgb & 0xFFFFFF);
}

static Value n_screen(VM* vm, Value* a) {
    int s = a[0].as.i;
    if (!be) return vnone();
    if (s < 0 || s >= be->screens) { ms_vm_fail(vm, "Gfx.Screen(%d): this console has screens 0..%d", s, be->screens - 1); return vnone(); }
    screen = s;
    if (be->select) be->select(s);
    return vnone();
}
static Value n_clear(VM* vm, Value* a) { (void)vm; if (be && be->clear) be->clear(argb(a[0].as.i)); return vnone(); }
static Value n_rect(VM* vm, Value* a) {
    (void)vm;
    if (be && be->rect) be->rect(a[0].as.f, a[1].as.f, a[2].as.f, a[3].as.f, argb(a[4].as.i));
    return vnone();
}
/* a frame of a rectangle: four thin rectangles inside x, y, w, h */
static Value n_rectline(VM* vm, Value* a) {
    (void)vm;
    if (!be || !be->rect) return vnone();
    float x = a[0].as.f, y = a[1].as.f, w = a[2].as.f, h = a[3].as.f, t = a[5].as.f;
    uint32_t c = argb(a[4].as.i);
    if (t <= 0 || w <= 0 || h <= 0) return vnone();
    if (2 * t >= w || 2 * t >= h) { be->rect(x, y, w, h, c); return vnone(); }
    be->rect(x, y, w, t, c);
    be->rect(x, y + h - t, w, t, c);
    be->rect(x, y + t, t, h - 2 * t, c);
    be->rect(x + w - t, y + t, t, h - 2 * t, c);
    return vnone();
}
static Value n_rectline1(VM* vm, Value* a) {
    Value b[6]; memcpy(b, a, sizeof(Value) * 5); b[5] = vfloat(1); return n_rectline(vm, b);
}
static Value n_circle(VM* vm, Value* a) {
    (void)vm;
    if (be && be->circle && a[2].as.f > 0) be->circle(a[0].as.f, a[1].as.f, a[2].as.f, argb(a[3].as.i));
    return vnone();
}
static Value n_line(VM* vm, Value* a) {
    (void)vm;
    if (be && be->line) be->line(a[0].as.f, a[1].as.f, a[2].as.f, a[3].as.f, a[5].as.f, argb(a[4].as.i));
    return vnone();
}
static Value n_line1(VM* vm, Value* a) {
    Value b[6]; memcpy(b, a, sizeof(Value) * 5); b[5] = vfloat(1); return n_line(vm, b);
}
static Value n_pixel(VM* vm, Value* a) {
    (void)vm;
    if (be && be->rect) be->rect(floorf(a[0].as.f), floorf(a[1].as.f), 1, 1, argb(a[2].as.i));
    return vnone();
}
#define TEXT_SIZE 14.0f                 /* Gfx.Text without a size */
static Value n_text5(VM* vm, Value* a) {
    (void)vm;
    if (be && be->text && a[2].t == V_OBJ) be->text(a[0].as.f, a[1].as.f, vstr(a[2]), a[4].as.f, argb(a[3].as.i));
    return vnone();
}
static Value n_text(VM* vm, Value* a) {
    Value b[5]; memcpy(b, a, sizeof(Value) * 4); b[4] = vfloat(TEXT_SIZE); return n_text5(vm, b);
}
static Value n_textwidth2(VM* vm, Value* a) {
    (void)vm;
    return vfloat(be && be->textWidth && a[0].t == V_OBJ ? be->textWidth(vstr(a[0]), a[1].as.f) : 0);
}
static Value n_textwidth(VM* vm, Value* a) { Value b[2] = { a[0], vfloat(TEXT_SIZE) }; return n_textwidth2(vm, b); }
static Value n_alpha(VM* vm, Value* a) { (void)vm; alpha = a[0].as.f; return vnone(); }
static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static Value n_rgb(VM* vm, Value* a) {
    (void)vm;
    return vint((clamp255(a[0].as.i) << 16) | (clamp255(a[1].as.i) << 8) | clamp255(a[2].as.i));
}
/* between two colours: t = 0 the first, 1 the second (fades, health bars) */
static Value n_lerp(VM* vm, Value* a) {
    (void)vm;
    float t = a[2].as.f < 0 ? 0 : a[2].as.f > 1 ? 1 : a[2].as.f;
    int c1 = a[0].as.i, c2 = a[1].as.i, r = 0;
    for (int sh = 0; sh <= 16; sh += 8) {
        int x = (c1 >> sh) & 255, y = (c2 >> sh) & 255;
        r |= (int)(x + (y - x) * t + 0.5f) << sh;
    }
    return vint(r);
}
static Value n_width(VM* vm, Value* a) { (void)vm; (void)a; return vint(be ? be->w[screen] : 0); }
static Value n_height(VM* vm, Value* a) { (void)vm; (void)a; return vint(be ? be->h[screen] : 0); }

/* ---------- pictures ---------- */
static Value n_loadimage(VM* vm, Value* a) {
    const char* name = vstr(a[0]);
    if (!*name) { ms_vm_fail(vm, "Gfx.LoadImage: no file name"); return vint(0); }
    char path[600];
    if (name[0] == '/' || strchr(name, ':') || !baseDir[0]) snprintf(path, sizeof path, "%s", name);
    else snprintf(path, sizeof path, "%s/%s", baseDir, name);
    for (int i = 0; i < nimages; i++) if (!strcmp(images[i].path, path)) return vint(i + 1);
    if (!be || !be->imageCreate) return vint(0);            /* no screen (tests of text programs): nothing to draw on */
    if (nimages == MAX_IMAGES) { ms_vm_fail(vm, "Gfx.LoadImage: at most %d pictures", MAX_IMAGES); return vint(0); }
    int w, h;
    const char* why = "";
    unsigned char* px = image_load_file(path, &w, &h, &why);
    if (!px) { ms_vm_fail(vm, "Gfx.LoadImage: cannot read '%s' (%s)", path, why ? why : "?"); return vint(0); }
    int id = be->imageCreate(px, w, h);
    image_free_pixels(px);
    if (id < 0) { ms_vm_fail(vm, "Gfx.LoadImage: '%s' is too big for the screen's memory (%d x %d)", path, w, h); return vint(0); }
    Img* im = &images[nimages++];
    im->id = id; im->w = w; im->h = h;
    snprintf(im->path, sizeof im->path, "%s", path);
    return vint(nimages);
}

static Img* image_of(VM* vm, Value v, const char* what) {
    int i = v.as.i;
    if (i == 0 && !be) return NULL;                         /* LoadImage without a screen gave 0: drawing does nothing */
    if (i < 1 || i > nimages) { ms_vm_fail(vm, "%s: %d is not a picture from Gfx.LoadImage", what, i); return NULL; }
    return &images[i - 1];
}

static void draw_part(Img* im, float sx, float sy, float sw, float sh, float x, float y, float sc, float angle) {
    if (be && be->image) be->image(im->id, sx, sy, sw, sh, x, y, sw * sc, sh * sc, angle, argb(0xFFFFFF));
}
/* Gfx.Image(img, x, y [, scale]): the whole picture with its top left corner at x, y; a negative scale mirrors it */
static Value n_image4(VM* vm, Value* a) {
    Img* im = image_of(vm, a[0], "Gfx.Image");
    if (im) {
        float sc = a[3].as.f;
        float x = a[1].as.f, y = a[2].as.f;
        if (sc < 0) x -= im->w * sc;                        /* mirrored: it stays in the same place */
        draw_part(im, 0, 0, (float)im->w, (float)im->h, x, y, sc, 0);
    }
    return vnone();
}
static Value n_image(VM* vm, Value* a) { Value b[4] = { a[0], a[1], a[2], vfloat(1) }; return n_image4(vm, b); }
/* Gfx.ImagePart(img, sx, sy, sw, sh, x, y [, scale]): one frame of a sprite sheet */
static Value n_imagepart8(VM* vm, Value* a) {
    Img* im = image_of(vm, a[0], "Gfx.ImagePart");
    if (im) {
        float sc = a[7].as.f, x = a[5].as.f;
        if (sc < 0) x -= a[3].as.f * sc;
        draw_part(im, a[1].as.f, a[2].as.f, a[3].as.f, a[4].as.f, x, a[6].as.f, sc, 0);
    }
    return vnone();
}
static Value n_imagepart(VM* vm, Value* a) {
    Value b[8]; memcpy(b, a, sizeof(Value) * 7); b[7] = vfloat(1); return n_imagepart8(vm, b);
}
/* Gfx.ImageRot(img, x, y, angle, scale): centred at x, y, turned by angle degrees (clockwise) */
static Value n_imagerot(VM* vm, Value* a) {
    Img* im = image_of(vm, a[0], "Gfx.ImageRot");
    if (im) {
        float sc = a[4].as.f, w = im->w * sc, h = im->h * sc;
        float aw = w < 0 ? -w : w, ah = h < 0 ? -h : h;
        if (be && be->image)
            be->image(im->id, 0, 0, (float)im->w, (float)im->h, a[1].as.f - aw / 2, a[2].as.f - ah / 2, w, h,
                      a[3].as.f * 3.14159265f / 180.0f, argb(0xFFFFFF));
    }
    return vnone();
}
static Value n_imagewidth(VM* vm, Value* a) { Img* im = image_of(vm, a[0], "Gfx.ImageWidth"); return vint(im ? im->w : 0); }
static Value n_imageheight(VM* vm, Value* a) { Img* im = image_of(vm, a[0], "Gfx.ImageHeight"); return vint(im ? im->h : 0); }

/* ---------- Input ---------- */
static Value n_held(VM* vm, Value* a) { (void)vm; return vbool((held & (uint32_t)a[0].as.i) != 0); }
static Value n_pressed(VM* vm, Value* a) { (void)vm; uint32_t b = (uint32_t)a[0].as.i; return vbool((held & b) && !(prevHeld & b)); }
static Value n_released(VM* vm, Value* a) { (void)vm; uint32_t b = (uint32_t)a[0].as.i; return vbool(!(held & b) && (prevHeld & b)); }
static Value n_touching(VM* vm, Value* a) { (void)vm; (void)a; return vbool(touching); }
static Value n_touched(VM* vm, Value* a) { (void)vm; (void)a; return vbool(touching && !prevTouching); }
static Value n_touchx(VM* vm, Value* a) { (void)vm; (void)a; return vint(touchX); }
static Value n_touchy(VM* vm, Value* a) { (void)vm; (void)a; return vint(touchY); }

/* ---------- Time, App ---------- */
static Value n_millis(VM* vm, Value* a) { (void)vm; (void)a; return vint(ms_millis()); }
static Value n_delta(VM* vm, Value* a) { (void)vm; (void)a; return vfloat(delta); }
static Value n_frame(VM* vm, Value* a) { (void)vm; (void)a; return vint(frame); }
static Value n_quit(VM* vm, Value* a) { (void)a; ms_vm_request_quit(vm); return vnone(); }

static const NativeDef gfx[] = {
    {"Gfx", "Screen", 1, {I}, VD, n_screen},
    {"Gfx", "Clear", 1, {I}, VD, n_clear},
    {"Gfx", "Rect", 5, {F, F, F, F, I}, VD, n_rect},
    {"Gfx", "RectLine", 5, {F, F, F, F, I}, VD, n_rectline1},
    {"Gfx", "RectLine", 6, {F, F, F, F, I, F}, VD, n_rectline},
    {"Gfx", "Circle", 4, {F, F, F, I}, VD, n_circle},
    {"Gfx", "Line", 5, {F, F, F, F, I}, VD, n_line1},
    {"Gfx", "Line", 6, {F, F, F, F, I, F}, VD, n_line},
    {"Gfx", "Pixel", 3, {F, F, I}, VD, n_pixel},
    {"Gfx", "Text", 4, {F, F, S, I}, VD, n_text},
    {"Gfx", "Text", 5, {F, F, S, I, F}, VD, n_text5},
    {"Gfx", "TextWidth", 1, {S}, F, n_textwidth},
    {"Gfx", "TextWidth", 2, {S, F}, F, n_textwidth2},
    {"Gfx", "Alpha", 1, {F}, VD, n_alpha},
    {"Gfx", "Rgb", 3, {I, I, I}, I, n_rgb},
    {"Gfx", "Lerp", 3, {I, I, F}, I, n_lerp},
    {"Gfx", "LoadImage", 1, {S}, I, n_loadimage},
    {"Gfx", "Image", 3, {I, F, F}, VD, n_image},
    {"Gfx", "Image", 4, {I, F, F, F}, VD, n_image4},
    {"Gfx", "ImagePart", 7, {I, F, F, F, F, F, F}, VD, n_imagepart},
    {"Gfx", "ImagePart", 8, {I, F, F, F, F, F, F, F}, VD, n_imagepart8},
    {"Gfx", "ImageRot", 5, {I, F, F, F, F}, VD, n_imagerot},
    {"Gfx", "ImageWidth", 1, {I}, I, n_imagewidth},
    {"Gfx", "ImageHeight", 1, {I}, I, n_imageheight},
    {"Gfx", "Width", 0, {0}, I, n_width},
    {"Gfx", "Height", 0, {0}, I, n_height},
    {"Input", "Held", 1, {I}, B, n_held},
    {"Input", "Pressed", 1, {I}, B, n_pressed},
    {"Input", "Released", 1, {I}, B, n_released},
    {"Input", "Touching", 0, {0}, B, n_touching},
    {"Input", "Touched", 0, {0}, B, n_touched},
    {"Input", "TouchX", 0, {0}, I, n_touchx},
    {"Input", "TouchY", 0, {0}, I, n_touchy},
    {"Time", "Millis", 0, {0}, I, n_millis},
    {"Time", "Delta", 0, {0}, F, n_delta},
    {"Time", "Frame", 0, {0}, I, n_frame},
    {"App", "Quit", 0, {0}, VD, n_quit},
};

static const ConstDef gfx_consts[] = {
    {"Color", "Black", 0, 0x000000, 0}, {"Color", "White", 0, 0xFFFFFF, 0}, {"Color", "Red", 0, 0xFF0000, 0},
    {"Color", "Green", 0, 0x00C000, 0}, {"Color", "Blue", 0, 0x0040FF, 0}, {"Color", "Yellow", 0, 0xFFFF00, 0},
    {"Color", "Cyan", 0, 0x00FFFF, 0}, {"Color", "Magenta", 0, 0xFF00FF, 0}, {"Color", "Orange", 0, 0xFF8000, 0},
    {"Color", "Purple", 0, 0x8000C0, 0}, {"Color", "Brown", 0, 0x804000, 0}, {"Color", "Pink", 0, 0xFF80C0, 0},
    {"Color", "Gray", 0, 0x808080, 0}, {"Color", "LightGray", 0, 0xC0C0C0, 0}, {"Color", "DarkGray", 0, 0x404040, 0},
    {"Color", "DarkBlue", 0, 0x000080, 0}, {"Color", "DarkGreen", 0, 0x006000, 0}, {"Color", "DarkRed", 0, 0x800000, 0},
    {"Screen", "Top", 0, 0, 0}, {"Screen", "Bottom", 0, 1, 0},
    {"Btn", "A", 0, BTN_A, 0}, {"Btn", "B", 0, BTN_B, 0}, {"Btn", "X", 0, BTN_X, 0}, {"Btn", "Y", 0, BTN_Y, 0},
    {"Btn", "L", 0, BTN_L, 0}, {"Btn", "R", 0, BTN_R, 0}, {"Btn", "Start", 0, BTN_START, 0},
    {"Btn", "Select", 0, BTN_SELECT, 0}, {"Btn", "Up", 0, BTN_UP, 0}, {"Btn", "Down", 0, BTN_DOWN, 0},
    {"Btn", "Left", 0, BTN_LEFT, 0}, {"Btn", "Right", 0, BTN_RIGHT, 0},
};

void lib_register_gfx(void) { MS_REGISTER(gfx); MS_REGISTER_CONSTS(gfx_consts); }
