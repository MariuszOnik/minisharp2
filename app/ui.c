/* MiniSharp - ui.h: calls go to the backend set by the platform (see ui.h). */
#include "ui.h"
#include <stddef.h>

static const UiBackend* g_ui;

void ui_set_backend(const UiBackend* b) { g_ui = b; }
const UiBackend* ui_backend(void) { return g_ui; }

void ui_screen(int screen) { if (g_ui && g_ui->select) g_ui->select(screen); }

void ui_rect(float x, float y, float w, float h, uint32_t rgb) {
    if (g_ui && g_ui->rect && w > 0 && h > 0) g_ui->rect(x, y, w, h, rgb);
}

void ui_text(float x, float y, const char* s, float size, uint32_t rgb) {
    if (g_ui && g_ui->text && s && *s) g_ui->text(x, y, s, size, rgb);
}

float ui_text_width(const char* s, float size) {
    return g_ui && g_ui->textWidth && s ? g_ui->textWidth(s, size) : 0;
}

void ui_clip(int x, int y, int w, int h) { if (g_ui && g_ui->clip) g_ui->clip(x, y, w, h); }
void ui_unclip(void) { ui_clip(0, 0, 0, 0); }
