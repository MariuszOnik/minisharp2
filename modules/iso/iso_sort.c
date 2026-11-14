/* MiniSharp2 - module iso: depth sorting of isometric boxes.
 *
 * The fast sorting of IsoScreenSolver / IsoSortingSolver from unity-iso-tools (MIT, (C) 2015-2023 Matvey Cherevko,
 * https://github.com/BlackMATov/unity-iso-tools), as ported to C# in IsoSortowanieSzybkie and to C in the old
 * MiniSharp (src/iso.c, branch toee-3d):
 *   1. a pair is tested only when the screen rectangles of the two objects overlap (objects are kept in a grid of
 *      buckets by their screen rectangle; the original uses a quadtree),
 *   2. the dependencies between objects that do not move are found once and kept (the original: the Dirty flag),
 *   3. only visible objects are sorted (curVisibles).
 * Then every visible object is placed after the objects in front of it are (RecursivePlaceIsoObject), here with an
 * explicit stack instead of recursion. The order of every loop is the same as in the C# version, so both give the
 * same drawing order.
 *
 * Changes in MiniSharp2: the state is in IsoSort instead of globals (several worlds at once), bad ids are ignored. */
#include "iso_sort.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

typedef struct { int* v; int n, cap; } IntVec;

static int iv_push(IntVec* a, int x) {
    if (a->n == a->cap) {
        int nc = a->cap ? a->cap * 2 : 4;
        int* p = (int*)realloc(a->v, sizeof(int) * (size_t)nc);
        if (!p) return 0;
        a->v = p; a->cap = nc;
    }
    a->v[a->n++] = x;
    return 1;
}
static void iv_free(IntVec* a) { free(a->v); a->v = NULL; a->n = a->cap = 0; }

typedef struct {
    float x, y, z, sx, sy, sz;                  /* minimal corner and size, in tiles */
    float r0x, r0y, r1x, r1y;                   /* rectangle on the screen */
    uint8_t alive, moving;
    IntVec sdep;                                /* objects in front of this one that do not move (found once) */
    IntVec ddep;                                /* ... found this frame, with a moving object (valid when dynFrame == frame) */
    int visible, placed, mark, dynFrame;        /* frame numbers / marks of the last time */
} Obj;

/* buckets in the order they were made (like the keys of a .NET Dictionary), found through an open-addressing index */
typedef struct { int32_t key; IntVec items; } Bucket;
typedef struct { Bucket* b; int nb, capb; int* index; int icap; } Grid;

struct IsoSort {
    Obj* objs; int nobj, capobj;
    IntVec freeIds;
    Grid gstat, gmov;
    IntVec vis, order;
    struct { int o, pos; }* stk; int capstk;
    int frame, markNo, staticReady;
    int stPairs, stDeps, stStatic, stVisible;
    float m11, m12, m21, m22, tileH, cellSize, det;
};

/* ---------- grid ---------- */
static uint32_t hkey(int32_t k) { return (uint32_t)k * 2654435761u; }

static int grid_find(const Grid* g, int32_t key) {
    if (!g->icap) return -1;
    for (uint32_t i = hkey(key) & (uint32_t)(g->icap - 1);; i = (i + 1) & (uint32_t)(g->icap - 1)) {
        int e = g->index[i];
        if (e < 0) return -1;
        if (g->b[e].key == key) return e;
    }
}

static int grid_rehash(Grid* g, int icap) {
    int* ni = (int*)malloc(sizeof(int) * (size_t)icap);
    if (!ni) return 0;
    for (int i = 0; i < icap; i++) ni[i] = -1;
    for (int e = 0; e < g->nb; e++) {
        uint32_t i = hkey(g->b[e].key) & (uint32_t)(icap - 1);
        while (ni[i] >= 0) i = (i + 1) & (uint32_t)(icap - 1);
        ni[i] = e;
    }
    free(g->index);
    g->index = ni; g->icap = icap;
    return 1;
}

/* the bucket for key, made if needed; -1 = out of memory */
static int grid_get(Grid* g, int32_t key) {
    int e = grid_find(g, key);
    if (e >= 0) return e;
    if ((g->nb + 1) * 2 > g->icap && !grid_rehash(g, g->icap ? g->icap * 2 : 64)) return -1;
    if (g->nb == g->capb) {
        int nc = g->capb ? g->capb * 2 : 32;
        Bucket* nbk = (Bucket*)realloc(g->b, sizeof(Bucket) * (size_t)nc);
        if (!nbk) return -1;
        memset(nbk + g->capb, 0, sizeof(Bucket) * (size_t)(nc - g->capb));
        g->b = nbk; g->capb = nc;
    }
    e = g->nb++;
    g->b[e].key = key; g->b[e].items.n = 0;     /* a reused bucket keeps its memory */
    uint32_t i = hkey(key) & (uint32_t)(g->icap - 1);
    while (g->index[i] >= 0) i = (i + 1) & (uint32_t)(g->icap - 1);
    g->index[i] = e;
    return e;
}

static void grid_empty(Grid* g) {               /* no buckets at all (Dictionary.Clear) */
    g->nb = 0;
    for (int i = 0; i < g->icap; i++) g->index[i] = -1;
}
static void grid_empty_items(Grid* g) { for (int e = 0; e < g->nb; e++) g->b[e].items.n = 0; }
static void grid_free(Grid* g) {
    for (int e = 0; e < g->capb; e++) iv_free(&g->b[e].items);
    free(g->b); free(g->index);
    memset(g, 0, sizeof *g);
}

static int cellOf(const IsoSort* w, float v) { return (int)floorf(v / w->cellSize); }
static int32_t keyOf(int x, int y) { return (int32_t)((x + 2048) * 4096 + (y + 2048)); }

static int grid_insert(IsoSort* w, Grid* g, int id) {
    Obj* o = &w->objs[id];
    int x0 = cellOf(w, o->r0x), x1 = cellOf(w, o->r1x), y0 = cellOf(w, o->r0y), y1 = cellOf(w, o->r1y);
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++) {
            int e = grid_get(g, keyOf(x, y));
            if (e < 0 || !iv_push(&g->b[e].items, id)) return 0;
        }
    return 1;
}

/* ---------- geometry ---------- */
void iso_setup(IsoSort* w, float tileSize, float ratio, float angle, float tileHeight) {
    float kat = (90 - angle) * 3.14159265f / 180;
    float c = (float)cos((double)kat), s = (float)sin((double)kat);
    float skala = tileSize * (float)sqrt(2.0);
    w->m11 = c * skala; w->m12 = -s * skala;
    w->m21 = s * skala * ratio; w->m22 = c * skala * ratio;
    w->det = w->m11 * w->m22 - w->m12 * w->m21;
    w->tileH = tileHeight;
    w->cellSize = tileSize * 2;
    w->staticReady = 0;
    for (int i = 0; i < w->nobj; i++) w->objs[i].dynFrame = -1;
}

void iso_to_screen(const IsoSort* w, float x, float y, float z, float* sx, float* sy) {
    *sx = w->m11 * x + w->m12 * y;
    *sy = -(w->m21 * x + w->m22 * y + z * w->tileH);
}

void iso_from_screen(const IsoSort* w, float sx, float sy, float z, float* x, float* y) {
    float py = -sy - z * w->tileH;
    *x = (w->m22 * sx - w->m12 * py) / w->det;
    *y = (-w->m21 * sx + w->m11 * py) / w->det;
}

static void rect(const IsoSort* w, Obj* o) {
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    for (int i = 0; i < 8; i++) {
        float x = o->x + (float)(i & 1) * o->sx;
        float y = o->y + (float)((i >> 1) & 1) * o->sy;
        float z = o->z + (float)((i >> 2) & 1) * o->sz;
        float px = w->m11 * x + w->m12 * y, py = -(w->m21 * x + w->m22 * y + z * w->tileH);
        if (px < minX) minX = px;
        if (px > maxX) maxX = px;
        if (py < minY) minY = py;
        if (py > maxY) maxY = py;
    }
    o->r0x = minX; o->r0y = minY; o->r1x = maxX; o->r1y = maxY;
}

static int overlap(const Obj* a, const Obj* b) { return a->r1x > b->r0x && a->r0x < b->r1x && a->r1y > b->r0y && a->r0y < b->r1y; }

/* 1:1 with IsoScreenSolver.IsIsoObjectDepends: is a behind b (so b must be drawn after a)? */
static int depends(float ax, float ay, float az, float asx, float asy, float asz,
                   float bx, float by, float bz, float bsx, float bsy, float bsz) {
    float a_max_x = ax + asx, a_max_y = ay + asy, a_max_z = az + asz;
    float b_max_x = bx + bsx, b_max_y = by + bsy, b_max_z = bz + bsz;
    if (!(a_max_x > bx && a_max_y > by && b_max_z > az)) return 0;
    if (!(b_max_x > ax && b_max_y > ay && a_max_z > bz)) return 1;
    float dA_x = a_max_x - bx, dA_y = a_max_y - by, dA_z = b_max_z - az;
    float dB_x = b_max_x - ax, dB_y = b_max_y - ay, dB_z = a_max_z - bz;
    float dD_x = fabsf(dB_x - dA_x), dD_y = fabsf(dB_y - dA_y), dD_z = fabsf(dB_z - dA_z);
    float dP_x = asx + bsx - dD_x, dP_y = asy + bsy - dD_y, dP_z = asz + bsz - dD_z;
    if (dP_x <= dP_y && dP_x <= dP_z) return dA_x > dB_x;
    if (dP_y <= dP_x && dP_y <= dP_z) return dA_y > dB_y;
    return dA_z > dB_z;
}
static int obj_depends(const Obj* a, const Obj* b) {
    return depends(a->x, a->y, a->z, a->sx, a->sy, a->sz, b->x, b->y, b->z, b->sx, b->sy, b->sz);
}
int iso_behind(const float a[6], const float b[6]) {
    return depends(a[0], a[1], a[2], a[3], a[4], a[5], b[0], b[1], b[2], b[3], b[4], b[5]);
}

/* ---------- objects ---------- */
IsoSort* iso_new(void) {
    IsoSort* w = (IsoSort*)calloc(1, sizeof(IsoSort));
    if (w) iso_setup(w, 32, 0.5f, 45, 32);
    return w;
}

void iso_free(IsoSort* w) {
    if (!w) return;
    for (int i = 0; i < w->capobj; i++) { iv_free(&w->objs[i].sdep); iv_free(&w->objs[i].ddep); }
    free(w->objs);
    iv_free(&w->freeIds); iv_free(&w->vis); iv_free(&w->order);
    free(w->stk);
    grid_free(&w->gstat); grid_free(&w->gmov);
    free(w);
}

void iso_clear(IsoSort* w) {
    for (int i = 0; i < w->nobj; i++) { w->objs[i].alive = 0; w->objs[i].sdep.n = w->objs[i].ddep.n = 0; }
    w->nobj = 0; w->freeIds.n = 0; w->vis.n = w->order.n = 0;
    grid_empty(&w->gstat); grid_empty(&w->gmov);
    w->staticReady = 0;
}

int iso_add(IsoSort* w, float x, float y, float z, float sx, float sy, float sz, int moving) {
    int id;
    if (w->freeIds.n) id = w->freeIds.v[--w->freeIds.n];
    else {
        if (w->nobj == w->capobj) {
            int nc = w->capobj ? w->capobj * 2 : 256;
            Obj* p = (Obj*)realloc(w->objs, sizeof(Obj) * (size_t)nc);
            if (!p) return -1;
            memset(p + w->capobj, 0, sizeof(Obj) * (size_t)(nc - w->capobj));
            w->objs = p; w->capobj = nc;
        }
        id = w->nobj++;
    }
    Obj* o = &w->objs[id];
    IntVec sd = o->sdep, dd = o->ddep;          /* reuse their memory */
    memset(o, 0, sizeof *o);
    o->sdep = sd; o->ddep = dd; o->sdep.n = o->ddep.n = 0;
    o->x = x; o->y = y; o->z = z; o->sx = sx; o->sy = sy; o->sz = sz;
    o->alive = 1; o->moving = (uint8_t)(moving != 0);
    o->dynFrame = -1;
    rect(w, o);
    if (!o->moving) w->staticReady = 0;
    return id;
}

int iso_valid(const IsoSort* w, int id) { return id >= 0 && id < w->nobj && w->objs[id].alive; }
int iso_count(const IsoSort* w) { return w->nobj - w->freeIds.n; }

void iso_move(IsoSort* w, int id, float x, float y, float z) {
    if (!iso_valid(w, id)) return;
    Obj* o = &w->objs[id];
    o->x = x; o->y = y; o->z = z;
    if (!o->moving) w->staticReady = 0;
}

void iso_resize(IsoSort* w, int id, float sx, float sy, float sz) {
    if (!iso_valid(w, id)) return;
    Obj* o = &w->objs[id];
    o->sx = sx; o->sy = sy; o->sz = sz;
    if (!o->moving) w->staticReady = 0;
}

void iso_remove(IsoSort* w, int id) {
    if (!iso_valid(w, id)) return;
    Obj* o = &w->objs[id];
    o->alive = 0;
    o->sdep.n = o->ddep.n = 0;
    o->visible = o->dynFrame = -1;
    iv_push(&w->freeIds, id);
    if (!o->moving) w->staticReady = 0;         /* other objects keep it among their static dependencies */
}

/* ---------- sorting ---------- */
static int build_static(IsoSort* w) {
    grid_empty(&w->gstat);
    w->stStatic = 0;
    for (int i = 0; i < w->nobj; i++) {
        Obj* o = &w->objs[i];
        o->sdep.n = 0;
        if (!o->alive || o->moving) continue;
        rect(w, o);
        if (!grid_insert(w, &w->gstat, i)) return 0;
    }
    for (int e = 0; e < w->gstat.nb; e++) {
        IntVec* l = &w->gstat.b[e].items;
        for (int i = 0; i < l->n; i++)
            for (int j = i + 1; j < l->n; j++) {
                Obj* a = &w->objs[l->v[i]]; Obj* b = &w->objs[l->v[j]];
                /* a pair can be in several buckets: it is tested in the one where the overlap starts */
                float x = a->r0x > b->r0x ? a->r0x : b->r0x, y = a->r0y > b->r0y ? a->r0y : b->r0y;
                if (grid_find(&w->gstat, keyOf(cellOf(w, x), cellOf(w, y))) != e) continue;
                if (!overlap(a, b)) continue;
                w->stStatic++;
                if (obj_depends(a, b) && !iv_push(&a->sdep, l->v[j])) return 0;
                if (obj_depends(b, a) && !iv_push(&b->sdep, l->v[i])) return 0;
            }
    }
    w->staticReady = 1;
    return 1;
}

static int add_dyn(IsoSort* w, int o, int before) {
    Obj* ob = &w->objs[o];
    if (ob->dynFrame != w->frame) { ob->ddep.n = 0; ob->dynFrame = w->frame; }
    return iv_push(&ob->ddep, before);
}

static int check_bucket(IsoSort* w, int m, const IntVec* l) {
    Obj* mo = &w->objs[m];
    for (int k = 0; k < l->n; k++) {
        int c = l->v[k];
        Obj* co = &w->objs[c];
        if (c == m || co->mark == w->markNo) continue;
        co->mark = w->markNo;
        if (co->moving && co->visible == w->frame && c < m) continue;   /* two visible moving objects: once */
        if (!overlap(mo, co)) continue;
        w->stPairs++;
        if (obj_depends(mo, co)) { if (!add_dyn(w, m, c)) return 0; w->stDeps++; }
        if (obj_depends(co, mo)) { if (!add_dyn(w, c, m)) return 0; w->stDeps++; }
    }
    return 1;
}

/* RecursivePlaceIsoObject without recursion: the objects in front of o first, then o */
static int place(IsoSort* w, int start) {
    Obj* objs = w->objs;
    int frame = w->frame;
    if (objs[start].placed == frame) return 1;
    int sp = 0;
    objs[start].placed = frame;
    w->stk[sp].o = start; w->stk[sp].pos = 0; sp++;
    while (sp) {
        Obj* ob = &objs[w->stk[sp - 1].o];
        int ns = ob->sdep.n, nd = ob->dynFrame == frame ? ob->ddep.n : 0, next = -1;
        while (w->stk[sp - 1].pos < ns + nd) {
            int p = w->stk[sp - 1].pos++;
            int d = p < ns ? ob->sdep.v[p] : ob->ddep.v[p - ns];
            if (objs[d].visible == frame && objs[d].placed != frame) { next = d; break; }
        }
        if (next >= 0) {
            objs[next].placed = frame;
            w->stk[sp].o = next; w->stk[sp].pos = 0; sp++;
        } else {
            if (!iv_push(&w->order, w->stk[sp - 1].o)) return 0;
            sp--;
        }
    }
    return 1;
}

int iso_sort(IsoSort* w, float minX, float minY, float maxX, float maxY) {
    w->frame++;
    w->stPairs = w->stDeps = 0;
    if (!w->staticReady && !build_static(w)) return -1;
    grid_empty_items(&w->gmov);
    w->vis.n = 0;
    for (int i = 0; i < w->nobj; i++) {
        Obj* o = &w->objs[i];
        if (!o->alive) continue;
        if (o->moving) { rect(w, o); if (!grid_insert(w, &w->gmov, i)) return -1; }
        if (o->r1x > minX && o->r0x < maxX && o->r1y > minY && o->r0y < maxY) {
            o->visible = w->frame;
            if (!iv_push(&w->vis, i)) return -1;
        }
    }
    w->stVisible = w->vis.n;
    for (int v = 0; v < w->vis.n; v++) {
        int m = w->vis.v[v];
        Obj* mo = &w->objs[m];
        if (!mo->moving) continue;
        w->markNo++;
        int x0 = cellOf(w, mo->r0x), x1 = cellOf(w, mo->r1x), y0 = cellOf(w, mo->r0y), y1 = cellOf(w, mo->r1y);
        for (int x = x0; x <= x1; x++)
            for (int y = y0; y <= y1; y++) {
                int32_t k = keyOf(x, y);
                int e = grid_find(&w->gstat, k);
                if (e >= 0 && !check_bucket(w, m, &w->gstat.b[e].items)) return -1;
                e = grid_find(&w->gmov, k);
                if (e >= 0 && !check_bucket(w, m, &w->gmov.b[e].items)) return -1;
            }
    }
    if (w->capstk < w->vis.n + 1) {
        int nc = w->vis.n + 64;
        void* p = realloc(w->stk, sizeof *w->stk * (size_t)nc);
        if (!p) return -1;
        w->stk = p; w->capstk = nc;
    }
    w->order.n = 0;
    for (int v = 0; v < w->vis.n; v++) if (!place(w, w->vis.v[v])) return -1;
    return w->order.n;
}

int iso_at(const IsoSort* w, int i) { return i >= 0 && i < w->order.n ? w->order.v[w->order.n - 1 - i] : -1; }   /* the farthest first */

int iso_stat(const IsoSort* w, int which) {
    switch (which) {
        case ISO_STAT_PAIRS: return w->stPairs;
        case ISO_STAT_DEPS: return w->stDeps;
        case ISO_STAT_STATIC_PAIRS: return w->stStatic;
        default: return w->stVisible;
    }
}
