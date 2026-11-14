/* Unit test: the game loop's rhythm (lib_frame_steps in lib/gfx.c) – Update 60 times a second whatever the display
 * does: one step per frame at 60 Hz even with a timer's jitter, two at 30 Hz, none or one at 120 Hz, at most four
 * when the device is too slow (and then the lost time is dropped), Gfx.FrameRate. Exit code 0 = all passed. */
#include "gfx.h"
#include <stdio.h>

static int fails;
static void check(int cond, const char* what) { if (!cond) { fails++; printf("FAIL: %s\n", what); } }

/* how many steps in 'frames' displayed frames 'ms' apart (with a +-1 ms jitter when asked) */
static int run(double ms, int frames, int jitter) {
    lib_gfx_reset();
    double t = 1000;
    int total = lib_frame_steps(t), worst = 0;
    for (int i = 1; i < frames; i++) {
        t += ms + (jitter ? (i % 2 ? 1 : -1) : 0);
        int n = lib_frame_steps(t);
        total += n;
        if (ms < 17 && ms > 16 && n != 1) worst++;
    }
    if (ms < 17 && ms > 16) check(worst == 0, "60 Hz: exactly one step every frame");
    return total;
}

int main(void) {
    check(run(1000.0 / 60, 600, 0) == 600, "60 Hz, 10 s: 600 steps");
    check(run(1000.0 / 60, 600, 1) == 600, "60 Hz with a 1 ms jitter: still one step each");
    check(run(1000.0 / 30, 300, 0) == 599, "30 Hz: two steps per frame (1 + 299 * 2)");
    int at120 = run(1000.0 / 120, 1200, 0);
    check(at120 >= 599 && at120 <= 600, "120 Hz, 10 s: about 600 steps (one every second frame)");
    lib_gfx_reset();
    lib_frame_steps(0);
    check(lib_frame_steps(1000) == 4, "a 1-second stall: four steps, not sixty");
    check(lib_frame_steps(1000 + 1000.0 / 60) == 1, "and then the normal rhythm, the lost time dropped");
    check(lib_draw_rate() == 60, "Draw 60 times a second by default");
    if (!fails) printf("framesteps: ok\n");
    return fails ? 1 : 0;
}
