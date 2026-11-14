/* MiniSharp - Gfx, Color, Screen, Input, Btn, Time, App (see gfx.h). */
#include "gfx.h"
#include "libvalue.h"
#include "lib.h"
#include "image.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const GfxBackend* be;
static int screen;
static float alpha = 1.0f;
static uint32_t held, prevHeld;
static int touching, prevTouching, touchX, touchY;
static float delta = 1.0f / 60;
static int frame;
static int touchScreenIdx;
static double acc, lastMs;
static int stepsStarted;
static int drawRate = 60;

/* Time.Millis: the real clock, independent of frames */
static int clock_ms(void) { return (int)((long long)clock() * 1000 / CLOCKS_PER_SEC); }
static int (*realClock)(void) = clock_ms;
static int realStart;
void lib_set_real_clock(int (*millis)(void)) { realClock = millis ? millis : clock_ms; realStart = realClock(); }
void lib_time_start(void) { realStart = realClock(); }

/* ---------- logical screens on the physical ones ---------- */
typedef struct { int phys; float s, ox, oy; int w, h; } View;  /* logical w x h drawn at ox, oy scaled by s */
static View views[2];
static int nviews = 2, layoutKind = LAYOUT_TWO, layoutW, layoutH;

/* w x h in pw x ph: the biggest whole scale when it is at least 1, centred */
static View fit(int phys, int w, int h, int pw, int ph) {
    View v = { phys, 1, 0, 0, w, h };
    float s = (float)pw / w < (float)ph / h ? (float)pw / w : (float)ph / h;
    if (s >= 1) s = floorf(s);
    v.s = s;
    v.ox = (pw - w * s) / 2; v.oy = (ph - h * s) / 2;
    return v;
}

static void compute_layout(void) {
    if (!be) { nviews = layoutKind == LAYOUT_TWO ? 2 : 1; return; }
    int pw0 = be->w[0], ph0 = be->h[0];
    if (layoutKind == LAYOUT_FILL) { View v = { 0, 1, 0, 0, pw0, ph0 }; views[0] = v; nviews = 1; return; }
    if (layoutKind == LAYOUT_ONE) { views[0] = fit(0, layoutW, layoutH, pw0, ph0); nviews = 1; return; }
    nviews = 2;
    if (be->screens >= 2) {                     /* 3DS, DS: each screen on its own */
        views[0] = fit(0, 400, 240, pw0, ph0);
        views[1] = fit(1, 320, 240, be->w[1], be->h[1]);
        return;
    }
    /* one display: one above the other (400 x 480) or side by side (720 x 240), whichever is bigger */
    View a = fit(0, 400, 480, pw0, ph0), b = fit(0, 720, 240, pw0, ph0);
    if (a.s >= b.s) {
        views[0] = a; views[0].w = 400; views[0].h = 240;
        views[1] = a; views[1].w = 320; views[1].h = 240; views[1].ox += 40 * a.s; views[1].oy += 240 * a.s;
    } else {
        views[0] = b; views[0].w = 400; views[0].h = 240;
        views[1] = b; views[1].w = 320; views[1].h = 240; views[1].ox += 400 * b.s;
    }
}

static View* cur_view(void) { return &views[screen < nviews ? screen : 0]; }

static void select_screen(int s) {
    screen = s;
    if (be && be->select) be->select(cur_view()->phys);
}

/* drawing in logical pixels of the current screen */
static void d_rect(float x, float y, float w, float h, uint32_t c) {
    View* v = cur_view();
    if (be && be->rect) be->rect(v->ox + x * v->s, v->oy + y * v->s, w * v->s, h * v->s, c);
}
static void d_clear(uint32_t c) {
    View* v = cur_view();
    if (!be) return;
    int full = v->ox == 0 && v->oy == 0 && v->w * v->s == be->w[v->phys] && v->h * v->s == be->h[v->phys];
    if (full && be->clear) be->clear(c);
    else if (be->rect) be->rect(v->ox, v->oy, v->w * v->s, v->h * v->s, c);
}

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
    be = b;
    compute_layout();
    select_screen(0);
}
const GfxBackend* lib_gfx(void) { return be; }

void lib_gfx_reset(void) {
    held = prevHeld = 0; touching = prevTouching = 0; touchX = touchY = 0;
    delta = 1.0f / 60; frame = 0; alpha = 1.0f; touchScreenIdx = 0;
    acc = 0; lastMs = 0; stepsStarted = 0; drawRate = 60;
    layoutKind = LAYOUT_TWO;
    compute_layout();
    forget_images();
    select_screen(0);
    lib_time_start();
}

/* the touch in physical pixels -> the logical screen under it; outside every logical screen it does not count */
void lib_input_frame(uint32_t h, int t, int x, int y) {
    prevHeld = held; prevTouching = touching;
    held = h; touching = 0;
    if (!t) return;
    if (!be) { touching = 1; touchX = x; touchY = y; touchScreenIdx = 1; return; }   /* no screens: as given */
    int ts = be->touchScreen;
    for (int i = 0; i < nviews; i++) {
        View* v = &views[i];
        if (v->phys != ts) continue;
        float lx = (x - v->ox) / v->s, ly = (y - v->oy) / v->s;
        if (lx < 0 || ly < 0 || lx >= v->w || ly >= v->h) continue;
        touching = 1; touchX = (int)lx; touchY = (int)ly; touchScreenIdx = i;
        break;
    }
}

int lib_frame_steps(double now) {
    const double step = 1000.0 / 60;
    if (!stepsStarted) { stepsStarted = 1; lastMs = now; acc = 0; return 1; }
    double dt = now - lastMs;
    lastMs = now;
    for (int k = 1; k <= 4; k++)                /* the display's own rhythm (vsync): no 0-2-1 stutter from a timer's jitter */
        if (dt > k * step - 2 && dt < k * step + 2) { dt = k * step; break; }
    acc += dt;
    int n = 0;
    while (acc >= step && n < 4) { acc -= step; n++; }
    if (n == 4 && acc >= step) acc = 0;        /* too slow: let the time go instead of catching up for ever */
    return n;
}

void lib_time_frame(float d) {
    delta = d; frame++;
    alpha = 1.0f;
    select_screen(0);
}

int lib_draw_rate(void) { return drawRate; }

void lib_draw_begin(void) { alpha = 1.0f; select_screen(0); }   /* every frame starts on the first screen, opaque */

/* ---------- Gfx ---------- */
static uint32_t argb(int rgb) {
    float a = alpha < 0 ? 0 : alpha > 1 ? 1 : alpha;
    return ((uint32_t)(a * 255.0f + 0.5f) << 24) | ((uint32_t)rgb & 0xFFFFFF);
}

static Value n_screen(VM* vm, Value* a) {
    int s = a[0].as.i;
    if (s < 0 || s >= nviews) {
        if (nviews == 1) ms_vm_fail(vm, "Gfx.Screen(%d): the layout has one screen (Gfx.Layout(Layout.TwoScreens) gives two)", s);
        else ms_vm_fail(vm, "Gfx.Screen(%d): the screens are Screen.Top (0) and Screen.Bottom (1)", s);
        return vnone();
    }
    select_screen(s);
    return vnone();
}
static Value set_layout(VM* vm, int kind, int w, int h) {
    if (kind == LAYOUT_ONE && (w < 16 || h < 16 || w > 4096 || h > 4096)) {
        ms_vm_fail(vm, "Gfx.Layout(Layout.OneScreen, %d, %d): the size must be 16..4096", w, h);
        return vnone();
    }
    layoutKind = kind; layoutW = w; layoutH = h;
    compute_layout();
    select_screen(0);
    return vnone();
}
static Value n_layout(VM* vm, Value* a) {
    int k = a[0].as.i;
    if (k == LAYOUT_ONE) { ms_vm_fail(vm, "Gfx.Layout(Layout.OneScreen, width, height): give the size of the screen"); return vnone(); }
    if (k != LAYOUT_TWO && k != LAYOUT_FILL) { ms_vm_fail(vm, "Gfx.Layout(%d): use Layout.TwoScreens, Layout.OneScreen or Layout.Fill", k); return vnone(); }
    return set_layout(vm, k, 0, 0);
}
static Value n_layout3(VM* vm, Value* a) {
    if (a[0].as.i != LAYOUT_ONE) { ms_vm_fail(vm, "Gfx.Layout: a width and height go only with Layout.OneScreen"); return vnone(); }
    return set_layout(vm, LAYOUT_ONE, a[1].as.i, a[2].as.i);
}
static Value n_framerate(VM* vm, Value* a) {
    int r = a[0].as.i;
    if (r != 60 && r != 30 && r != 20 && r != 15) { ms_vm_fail(vm, "Gfx.FrameRate(%d): use 60, 30, 20 or 15 (Update stays at 60)", r); return vnone(); }
    drawRate = r;
    return vnone();
}
static Value n_clear(VM* vm, Value* a) { (void)vm; d_clear(argb(a[0].as.i)); return vnone(); }
static Value n_rect(VM* vm, Value* a) {
    (void)vm;
    d_rect(a[0].as.f, a[1].as.f, a[2].as.f, a[3].as.f, argb(a[4].as.i));
    return vnone();
}
/* a frame of a rectangle: four thin rectangles inside x, y, w, h */
static Value n_rectline(VM* vm, Value* a) {
    (void)vm;
    if (!be || !be->rect) return vnone();
    float x = a[0].as.f, y = a[1].as.f, w = a[2].as.f, h = a[3].as.f, t = a[5].as.f;
    uint32_t c = argb(a[4].as.i);
    if (t <= 0 || w <= 0 || h <= 0) return vnone();
    if (2 * t >= w || 2 * t >= h) { d_rect(x, y, w, h, c); return vnone(); }
    d_rect(x, y, w, t, c);
    d_rect(x, y + h - t, w, t, c);
    d_rect(x, y + t, t, h - 2 * t, c);
    d_rect(x + w - t, y + t, t, h - 2 * t, c);
    return vnone();
}
static Value n_rectline1(VM* vm, Value* a) {
    Value b[6]; memcpy(b, a, sizeof(Value) * 5); b[5] = vfloat(1); return n_rectline(vm, b);
}
static Value n_circle(VM* vm, Value* a) {
    (void)vm;
    View* v = cur_view();
    if (be && be->circle && a[2].as.f > 0) be->circle(v->ox + a[0].as.f * v->s, v->oy + a[1].as.f * v->s, a[2].as.f * v->s, argb(a[3].as.i));
    return vnone();
}
static Value n_line(VM* vm, Value* a) {
    (void)vm;
    View* v = cur_view();
    if (be && be->line) be->line(v->ox + a[0].as.f * v->s, v->oy + a[1].as.f * v->s, v->ox + a[2].as.f * v->s, v->oy + a[3].as.f * v->s,
                                 a[5].as.f * v->s, argb(a[4].as.i));
    return vnone();
}
static Value n_line1(VM* vm, Value* a) {
    Value b[6]; memcpy(b, a, sizeof(Value) * 5); b[5] = vfloat(1); return n_line(vm, b);
}
static Value n_pixel(VM* vm, Value* a) {
    (void)vm;
    d_rect(floorf(a[0].as.f), floorf(a[1].as.f), 1, 1, argb(a[2].as.i));
    return vnone();
}
#define TEXT_SIZE 14.0f                 /* Gfx.Text without a size */
static Value n_text5(VM* vm, Value* a) {
    (void)vm;
    View* v = cur_view();
    if (be && be->text && a[2].t == V_OBJ) be->text(v->ox + a[0].as.f * v->s, v->oy + a[1].as.f * v->s, vstr(a[2]), a[4].as.f * v->s, argb(a[3].as.i));
    return vnone();
}
static Value n_text(VM* vm, Value* a) {
    Value b[5]; memcpy(b, a, sizeof(Value) * 4); b[4] = vfloat(TEXT_SIZE); return n_text5(vm, b);
}
static Value n_textwidth2(VM* vm, Value* a) {
    (void)vm;
    View* v = cur_view();
    return vfloat(be && be->textWidth && a[0].t == V_OBJ ? be->textWidth(vstr(a[0]), a[1].as.f * v->s) / v->s : 0);
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
static Value n_width(VM* vm, Value* a) { (void)vm; (void)a; return vint(be ? cur_view()->w : 0); }
static Value n_height(VM* vm, Value* a) { (void)vm; (void)a; return vint(be ? cur_view()->h : 0); }

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

/* a part of a picture into the logical rectangle x, y, w, h (negative w / h mirror it) */
static void d_image(Img* im, float sx, float sy, float sw, float sh, float x, float y, float w, float h, float angle) {
    View* v = cur_view();
    if (be && be->image) be->image(im->id, sx, sy, sw, sh, v->ox + x * v->s, v->oy + y * v->s, w * v->s, h * v->s, angle, argb(0xFFFFFF));
}
/* a negative scale mirrors left-right only: the height takes the scale without its sign (else the picture would
 * also turn upside down) */
static void draw_part(Img* im, float sx, float sy, float sw, float sh, float x, float y, float sc, float angle) {
    d_image(im, sx, sy, sw, sh, x, y, sw * sc, sh * fabsf(sc), angle);
}
/* Gfx.Image(img, x, y [, scale]): the whole picture with its top left corner at x, y; a negative scale mirrors it */
static Value n_image4(VM* vm, Value* a) {
    Img* im = image_of(vm, a[0], "Gfx.Image");
    if (im) {
        float sc = a[3].as.f;
        float x = a[1].as.f, y = a[2].as.f;
        /* mirrored (sc < 0): the backends draw a negative width from x to the right, so x stays the left edge */
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
        float sc = a[4].as.f, w = im->w * sc, h = im->h * fabsf(sc);   /* mirrored left-right only */
        float aw = w < 0 ? -w : w, ah = h < 0 ? -h : h;
        d_image(im, 0, 0, (float)im->w, (float)im->h, a[1].as.f - aw / 2, a[2].as.f - ah / 2, w, h, a[3].as.f * 3.14159265f / 180.0f);
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
static Value n_touchscreen(VM* vm, Value* a) { (void)vm; (void)a; return vint(touchScreenIdx); }

/* ---------- Time, App ---------- */
static Value n_millis(VM* vm, Value* a) { (void)vm; (void)a; return vint(realClock() - realStart); }
static Value n_delta(VM* vm, Value* a) { (void)vm; (void)a; return vfloat(delta); }
static Value n_frame(VM* vm, Value* a) { (void)vm; (void)a; return vint(frame); }
static Value n_quit(VM* vm, Value* a) { (void)a; ms_vm_request_quit(vm); return vnone(); }

static const NativeDef gfx[] = {
    {"Gfx", "Screen", 1, {I}, VD, n_screen},
    {"Gfx", "Layout", 1, {I}, VD, n_layout},
    {"Gfx", "FrameRate", 1, {I}, VD, n_framerate},
    {"Gfx", "Layout", 3, {I, I, I}, VD, n_layout3},
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
    {"Input", "TouchScreen", 0, {0}, I, n_touchscreen},
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
    {"Layout", "TwoScreens", 0, LAYOUT_TWO, 0}, {"Layout", "OneScreen", 0, LAYOUT_ONE, 0}, {"Layout", "Fill", 0, LAYOUT_FILL, 0},
    {"Btn", "A", 0, BTN_A, 0}, {"Btn", "B", 0, BTN_B, 0}, {"Btn", "X", 0, BTN_X, 0}, {"Btn", "Y", 0, BTN_Y, 0},
    {"Btn", "L", 0, BTN_L, 0}, {"Btn", "R", 0, BTN_R, 0}, {"Btn", "Start", 0, BTN_START, 0},
    {"Btn", "Select", 0, BTN_SELECT, 0}, {"Btn", "Up", 0, BTN_UP, 0}, {"Btn", "Down", 0, BTN_DOWN, 0},
    {"Btn", "Left", 0, BTN_LEFT, 0}, {"Btn", "Right", 0, BTN_RIGHT, 0},
};

void lib_register_gfx(void) { MS_REGISTER(gfx); MS_REGISTER_CONSTS(gfx_consts); }
