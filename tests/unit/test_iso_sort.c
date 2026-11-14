/* Unit test: module iso, depth sorting (modules/iso/iso_sort.c).
 * A city like the stress test IsometricLib/Gra.cs: floor tiles on the streets, buildings of different heights, people
 * moving on the streets. Every frame the fast order is checked against all pairs: when box a is behind box b and their
 * screen rectangles overlap, a must be drawn before b. Prints "FAIL: ..." lines; exit code 0 = all passed. */
#include "iso_sort.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }
static int near(float a, float b) { return fabsf(a - b) < 0.01f; }

static unsigned seed = 7;
static int rnd(int n) { seed = seed * 1103515245u + 12345u; return (int)((seed >> 16) % (unsigned)n); }

#define MAXO 4096
static float box[MAXO][6];      /* minimal corner, size */
static int alive[MAXO];
static int pos[MAXO];

static void screen_rect(IsoSort* w, const float* b, float r[4]) {
    r[0] = r[1] = 1e30f; r[2] = r[3] = -1e30f;
    for (int i = 0; i < 8; i++) {
        float sx, sy;
        iso_to_screen(w, b[0] + (float)(i & 1) * b[3], b[1] + (float)((i >> 1) & 1) * b[4], b[2] + (float)((i >> 2) & 1) * b[5], &sx, &sy);
        if (sx < r[0]) r[0] = sx;
        if (sy < r[1]) r[1] = sy;
        if (sx > r[2]) r[2] = sx;
        if (sy > r[3]) r[3] = sy;
    }
}
static int rects_overlap(const float* a, const float* b) { return a[2] > b[0] && a[0] < b[2] && a[3] > b[1] && a[1] < b[3]; }

/* sorts the view and checks the order against all pairs; returns the number of wrong pairs (-1 = sort failed) */
static int sort_and_check(IsoSort* w, float v0x, float v0y, float v1x, float v1y, long* brutePairs) {
    int n = iso_sort(w, v0x, v0y, v1x, v1y);
    if (n < 0) return -1;
    for (int i = 0; i < MAXO; i++) pos[i] = -1;
    for (int i = 0; i < n; i++) pos[iso_at(w, i)] = i;
    static float rc[MAXO][4];
    int expected = 0;
    for (int i = 0; i < MAXO; i++) {
        if (!alive[i]) continue;
        screen_rect(w, box[i], rc[i]);
        float view[4] = { v0x, v0y, v1x, v1y };
        int vis = rects_overlap(rc[i], view);
        expected += vis;
        if (vis != (pos[i] >= 0)) { fails++; printf("FAIL: object %d visible %d but sorted %d\n", i, vis, pos[i] >= 0); return -1; }
    }
    if (expected != n) return -1;
    int wrong = 0;
    *brutePairs = 0;
    for (int a = 0; a < MAXO; a++) {
        if (pos[a] < 0) continue;
        for (int b = a + 1; b < MAXO; b++) {
            if (pos[b] < 0 || !rects_overlap(rc[a], rc[b])) continue;
            (*brutePairs)++;
            int ab = iso_behind(box[a], box[b]), ba = iso_behind(box[b], box[a]);
            if (ab && !ba && pos[a] > pos[b]) wrong++;
            if (ba && !ab && pos[b] > pos[a]) wrong++;
        }
    }
    return wrong;
}

static int street(int x, int y) { return x % 3 == 0 || y % 3 == 0; }

static int add(IsoSort* w, float x, float y, float z, float sx, float sy, float sz, int moving) {
    int id = iso_add(w, x, y, z, sx, sy, sz, moving);
    if (id < 0 || id >= MAXO) { fails++; printf("FAIL: iso_add gave %d\n", id); return -1; }
    float b[6] = { x, y, z, sx, sy, sz };
    memcpy(box[id], b, sizeof b);
    alive[id] = 1;
    return id;
}

int main(void) {
    /* ---------- geometry ---------- */
    IsoSort* w = iso_new();
    check(w != NULL, "iso_new");
    float sx, sy, x, y;
    iso_to_screen(w, 1, 0, 0, &sx, &sy);
    check(near(sx, 32) && near(sy, -16), "x axis: right and up on the screen (tile 32, ratio 0.5, 45 degrees)");
    iso_to_screen(w, 0, 1, 0, &sx, &sy);
    check(near(sx, -32) && near(sy, -16), "y axis: left and up");
    iso_to_screen(w, 0, 0, 1, &sx, &sy);
    check(near(sx, 0) && near(sy, -32), "z axis: up by tileHeight");
    iso_to_screen(w, 3.5f, -2.25f, 0.75f, &sx, &sy);
    iso_from_screen(w, sx, sy, 0.75f, &x, &y);
    check(near(x, 3.5f) && near(y, -2.25f), "screen -> iso at a known height");
    iso_setup(w, 66, 0.5f, 45, 66);             /* Kenney's buildings, as in Gra.cs */
    iso_to_screen(w, 1, 1, 0, &sx, &sy);
    check(near(sx, 0) && near(sy, -66), "tile 66: (1,1) straight up");

    /* ---------- the city ---------- */
    memset(alive, 0, sizeof alive);
    const int N = 24;
    for (int gx = 0; gx < N; gx++)
        for (int gy = 0; gy < N; gy++) {
            if (street(gx, gy)) add(w, (float)gx, (float)gy, -0.5f, 1, 1, 0.5f, 0);
            else add(w, (float)gx, (float)gy, -0.5f, 1, 1, 0.5f + 0.5f * (float)(1 + rnd(4)) + 0.4f, 0);
        }
    int nstatic = iso_count(w);
    check(nstatic == N * N, "static objects added");
    int people[120], np = 0;
    for (int i = 0; i < 120; i++) people[np++] = add(w, 0, 0, 0, 0.3f, 0.3f, 0.5f, 1);

    long brute = 0, worst = 0;
    int totalWrong = 0;
    for (int frame = 0; frame < 20; frame++) {
        /* people on distinct street cells (boxes never intersect) */
        static unsigned char used[64][64];
        memset(used, 0, sizeof used);
        for (int i = 0; i < np; i++) {
            int gx, gy;
            do { gx = rnd(N); gy = rnd(N); } while (!street(gx, gy) || used[gx][gy]);
            used[gx][gy] = 1;
            float px = (float)gx + 0.1f * (float)rnd(7), py = (float)gy + 0.1f * (float)rnd(7);
            iso_move(w, people[i], px, py, 0);
            box[people[i]][0] = px; box[people[i]][1] = py;
        }
        int wrong = sort_and_check(w, -1e9f, -1e9f, 1e9f, 1e9f, &brute);
        if (wrong < 0) { check(0, "sort of the whole city"); break; }
        totalWrong += wrong;
        if (brute > worst) worst = brute;
    }
    check(totalWrong == 0, "drawing order agrees with all pairs (20 frames, 576 static + 120 moving)");
    if (totalWrong) printf("  wrong pairs: %d\n", totalWrong);
    check(iso_stat(w, ISO_STAT_VISIBLE) == N * N + np, "all visible in a big view");
    check(iso_stat(w, ISO_STAT_STATIC_PAIRS) > 0, "static pairs found once");
    long all = (long)(N * N + np) * (N * N + np - 1) / 2;
    check(iso_stat(w, ISO_STAT_PAIRS) < worst && iso_stat(w, ISO_STAT_PAIRS) < all / 100,
          "a frame tests fewer pairs than overlap on the screen, and ~1% of all pairs");
    printf("  pairs per frame: %d (overlapping on the screen: %ld, all: %ld), static pairs found once: %d\n",
           iso_stat(w, ISO_STAT_PAIRS), worst, all, iso_stat(w, ISO_STAT_STATIC_PAIRS));

    /* ---------- a small view: only what touches it ---------- */
    iso_to_screen(w, N / 2.0f, N / 2.0f, 0, &sx, &sy);
    long b2 = 0;
    check(sort_and_check(w, sx - 200, sy - 120, sx + 200, sy + 120, &b2) == 0, "view 400x240: correct order");
    check(iso_stat(w, ISO_STAT_VISIBLE) < N * N / 2 && iso_stat(w, ISO_STAT_VISIBLE) > 10, "view 400x240: some objects visible");

    /* ---------- changes ---------- */
    int victim = 5 * N + 7;                    /* a building */
    iso_remove(w, victim); alive[victim] = 0;
    check(!iso_valid(w, victim) && iso_count(w) == N * N + np - 1, "remove");
    int again = add(w, 5, 7, -0.5f, 1, 1, 3, 0);
    check(again == victim, "the freed id is given again");
    iso_move(w, again, 5, 7, -0.5f);
    iso_resize(w, again, 1, 1, 4); box[again][5] = 4;
    check(sort_and_check(w, -1e9f, -1e9f, 1e9f, 1e9f, &b2) == 0, "order after remove / add / resize of a static object");
    iso_move(w, -1, 0, 0, 0); iso_move(w, 99999, 0, 0, 0); iso_remove(w, 99999); iso_resize(w, -5, 1, 1, 1);
    check(!iso_valid(w, -1) && !iso_valid(w, 99999), "bad ids are ignored");

    /* ---------- two sorters do not share anything ---------- */
    IsoSort* w2 = iso_new();
    check(w2 && iso_count(w2) == 0, "second sorter is empty");
    iso_to_screen(w2, 1, 0, 0, &sx, &sy);
    check(near(sx, 32), "second sorter has its own tile size");
    int a = iso_add(w2, 0, 0, 0, 1, 1, 1, 0), b = iso_add(w2, 1, 0, 0, 1, 1, 1, 1);
    check(iso_sort(w2, -1e9f, -1e9f, 1e9f, 1e9f) == 2 && iso_at(w2, 0) == b && iso_at(w2, 1) == a,
          "two boxes: the one with greater x is farther (drawn first)");
    check(iso_count(w) == N * N + np, "first sorter untouched");
    iso_clear(w);
    check(iso_count(w) == 0 && iso_sort(w, -1e9f, -1e9f, 1e9f, 1e9f) == 0, "clear");
    iso_free(w2);
    iso_free(w);

    if (!fails) printf("test_iso_sort: ok\n");
    return fails ? 1 : 0;
}
