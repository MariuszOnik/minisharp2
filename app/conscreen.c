/* MiniSharp - the console screen (see conscreen.h). */
#include "conscreen.h"
#include "keyboard.h"
#include "ms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX_LEN 256
#define PANEL_H 240

struct ConScreen {
    TextGrid* g;
    int state, event;
    char line[LINE_MAX_LEN + 1];
    char ready[LINE_MAX_LEN + 1];
    char status[200];
    uint32_t statusRgb;
    char hint[100];
    uint32_t ms;
};

/* ---------- layout ---------- */
typedef struct { int gridScreen, panelScreen; float gx, gy, gw, gh, panelY, panelW; } Layout;

static Layout layout(void) {
    Layout L = { 0, 1, 0, 0, 400, 240, 0, 320 };
    const UiBackend* b = ui_backend();
    if (!b) return L;
    if (b->screens >= 2) {
        L.panelScreen = b->touchScreen >= 0 ? b->touchScreen : 1;
        L.gridScreen = !L.panelScreen;
        L.gw = (float)b->w[L.gridScreen]; L.gh = (float)b->h[L.gridScreen];
        L.panelW = (float)b->w[L.panelScreen];
        L.panelY = (float)b->h[L.panelScreen] - PANEL_H;
        if (L.panelY < 0) L.panelY = 0;
    } else {                            /* one screen: the grid above, the panel below */
        L.gridScreen = L.panelScreen = 0;
        L.gw = L.panelW = (float)b->w[0];
        L.gh = (float)b->h[0] - PANEL_H;
        if (L.gh < 60) L.gh = (float)b->h[0] / 2;
        L.panelY = L.gh;
    }
    return L;
}

/* ---------- life ---------- */
ConScreen* cs_new(int cols, int rows) {
    ConScreen* cs = (ConScreen*)calloc(1, sizeof(ConScreen));
    if (!cs) return NULL;
    cs->g = tg_new(cols, rows);
    if (!cs->g) { free(cs); return NULL; }
    cs->state = CS_RUNNING;
    snprintf(cs->hint, sizeof cs->hint, "running...");
    return cs;
}

void cs_free(ConScreen* cs) {
    if (!cs) return;
    tg_free(cs->g);
    free(cs);
}

TextGrid* cs_grid(ConScreen* cs) { return cs->g; }
int cs_state(const ConScreen* cs) { return cs->state; }
const char* cs_line(const ConScreen* cs) { return cs->ready; }
void cs_set_hint(ConScreen* cs, const char* s) { snprintf(cs->hint, sizeof cs->hint, "%s", s ? s : ""); }

void cs_ask(ConScreen* cs) {
    cs->state = CS_ASKING;
    cs->line[0] = 0;
    kbd_reset();
    kbd_set_enter_label("Enter");
}

void cs_finish(ConScreen* cs, int ok, const char* err) {
    cs->state = CS_FINISHED;
    if (ok) {
        snprintf(cs->status, sizeof cs->status, "done - A / B: back");
        cs->statusRgb = 0x80E080;
        return;
    }
    snprintf(cs->status, sizeof cs->status, "%s", err ? err : "error");
    cs->statusRgb = 0xFF7070;
    int fg = tg_fg(cs->g), bg = tg_bg(cs->g);       /* the error in red at the end of the output */
    if (tg_cursor_x(cs->g) > 0) tg_write(cs->g, "\n");
    tg_set_colors(cs->g, TG_RED, TG_BLACK);
    tg_write(cs->g, err ? err : "error");
    tg_write(cs->g, "\n");
    tg_set_colors(cs->g, fg, bg);
}

/* ---------- input ---------- */
void cs_key(ConScreen* cs, const char* k) {
    if (!k || cs->state != CS_ASKING) return;
    size_t n = strlen(cs->line);
    if (!strcmp(k, KBD_BACKSPACE)) {
        while (n > 0 && ((unsigned char)cs->line[n - 1] & 0xC0) == 0x80) n--;   /* a whole UTF-8 character */
        if (n > 0) n--;
        cs->line[n] = 0;
    } else if (!strcmp(k, KBD_ENTER)) {
        snprintf(cs->ready, sizeof cs->ready, "%s", cs->line);
        tg_write(cs->g, cs->line);                  /* the answer stays on screen, as in a console */
        tg_write(cs->g, "\n");
        cs->line[0] = 0;
        cs->state = CS_RUNNING;
        cs->event = CS_LINE;
    } else if (n + strlen(k) <= LINE_MAX_LEN) strcat(cs->line, k);
}

int cs_update(ConScreen* cs, const UiInput* in) {
    cs->ms = in->ms;
    cs->event = CS_NONE;
    Layout L = layout();
    if (cs->state == CS_FINISHED) return (in->down & (UI_BTN_A | UI_BTN_B)) ? CS_LEAVE : CS_NONE;
    if (cs->state != CS_ASKING) return CS_NONE;
    if (in->down & UI_BTN_START) { cs->state = CS_RUNNING; tg_write(cs->g, "\n"); return CS_EOF; }
    kbd_set_area(0, L.panelY + 80, L.panelW, PANEL_H - 80);
    const char* k = kbd_update(in);
    if (k) cs_key(cs, k);
    if (cs->event == CS_NONE && (in->down & UI_BTN_A)) cs_key(cs, KBD_ENTER);
    if (cs->event == CS_NONE && (in->down & UI_BTN_B)) cs_key(cs, KBD_BACKSPACE);
    return cs->event;
}

/* ---------- drawing ---------- */
/* the grid in an area: background runs as rectangles, every character in its own cell (the font is proportional,
 * so a row cannot be one text) */
static void draw_grid(ConScreen* cs, const Layout* L) {
    TextGrid* g = cs->g;
    int cols = tg_width(g), rows = tg_height(g);
    float cw = L->gw / cols, ch = L->gh / rows;
    for (int y = 0; y < rows; y++) {
        float py = L->gy + y * ch;
        for (int x = 0; x < cols; ) {                       /* backgrounds other than black, one rectangle per run */
            int bg = tg_cell(g, x, y)->bg, e = x + 1;
            while (e < cols && tg_cell(g, e, y)->bg == bg) e++;
            if (bg != TG_BLACK) ui_rect(L->gx + x * cw, py, (e - x) * cw, ch, tg_palette(bg));
            x = e;
        }
        for (int x = 0; x < cols; x++) {
            const TgCell* c = tg_cell(g, x, y);
            if (c->cp == ' ') continue;
            char s[5];
            s[ms_utf8_encode((int)c->cp, s)] = 0;
            ui_text(L->gx + x * cw + cw * 0.1f, py, s, ch, tg_palette(c->fg));
        }
    }
    /* the cursor: where the answer is typed, blinking */
    if (cs->state == CS_ASKING || cs->state == CS_RUNNING) {
        int cx = tg_cursor_x(g), cy = tg_cursor_y(g);
        float px = L->gx + cx * cw, py = L->gy + cy * ch;
        if (cs->state == CS_ASKING && cs->line[0]) {        /* the typed text is shown at the cursor, not yet in the grid */
            ui_text(px + cw * 0.1f, py, cs->line, ch, 0xFFFFFF);
            px += ui_text_width(cs->line, ch) + 1;
        }
        if (cs->state == CS_ASKING && (cs->ms / 400) % 2 == 0) ui_rect(px, py + ch - 2, cw, 2, 0xFFFFFF);
    }
}

static void draw_panel(ConScreen* cs, const Layout* L) {
    ui_screen(L->panelScreen);
    float y = L->panelY;
    if (cs->state == CS_ASKING) {
        ui_rect(0, y, L->panelW, 22, 0x1C2230);
        ui_text(6, y + 4, "The program asks: type the answer, Enter", 12, 0x7FB2FF);
        ui_rect(4, y + 28, L->panelW - 8, 26, 0x101820);
        char shown[LINE_MAX_LEN + 4];
        snprintf(shown, sizeof shown, "%s%s", cs->line, (cs->ms / 400) % 2 ? " " : "_");
        ui_text(10, y + 33, shown, 15, 0xFFFFFF);
        ui_text(6, y + 60, "START: end of input (null)", 11, 0x8090A0);
        kbd_set_area(0, y + 80, L->panelW, PANEL_H - 80);
        kbd_draw();
        return;
    }
    ui_rect(0, y, L->panelW, 22, 0x1C2230);
    ui_text(6, y + 4, cs->state == CS_FINISHED ? "Program finished" : "Program running", 12, 0x7FB2FF);
    if (cs->state == CS_FINISHED) {
        /* the status may be long (an error): in lines of the panel's width */
        const char* s = cs->status;
        float ty = y + 32;
        while (*s && ty < y + PANEL_H - 40) {
            char part[200];
            int n = 0, lastSpace = -1;
            while (s[n] && n < (int)sizeof part - 1) {
                part[n] = s[n]; part[n + 1] = 0;
                if (s[n] == ' ') lastSpace = n;
                if (ui_text_width(part, 12) > L->panelW - 16) { if (lastSpace > 0) n = lastSpace; break; }
                n++;
            }
            if (n == 0) n = 1;                              /* one character wider than the panel */
            part[n] = 0;
            ui_text(8, ty, part, 12, cs->statusRgb);
            s += n;
            while (*s == ' ') s++;
            ty += 16;
        }
        ui_text(8, y + PANEL_H - 30, "A / B: back", 12, 0xC0C8D0);
    } else ui_text(8, y + 32, cs->hint, 12, 0xA0A8B0);
}

void cs_draw(ConScreen* cs) {
    Layout L = layout();
    ui_screen(L.gridScreen);
    draw_grid(cs, &L);
    draw_panel(cs, &L);
}
