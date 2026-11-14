/* MiniSharp - the file list (see files.h). Moved in parts from the old source3ds/main_3ds.c (scan_files: projects
 * first, then single files) and source3ds/editor.c (files_screen, new_file: a name, then a file with an empty class). */
#include "files.h"
#include "keyboard.h"
#include "project.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0777)
#endif

#define MAX_ENTRIES 128
#define ROW_H 24
#define TOOLS_H 22
#define PANEL_H 240
#define INFO_H 60                       /* the hints above the list when there is one screen */
#define NAME_MAX_LEN 32
#define REPEAT_WAIT 400
#define REPEAT_EVERY 80

typedef struct { char name[68]; int isDir; int ncs; } Entry;

enum { NAMING_NONE, NAMING_FILE, NAMING_PROJECT };

struct Files {
    char root[256], dir[400], chosen[480];
    Entry e[MAX_ENTRIES];
    int n, sel, top;
    int naming;
    char armed[68];                     /* SELECT pressed once on this entry: the second press deletes it */
    char name[NAME_MAX_LEN + 1];
    char status[200];
    uint32_t statusRgb;
    uint32_t ms, upSince, upLast, downSince, downLast;
};

/* ---------- layout ---------- */
typedef struct { int infoScreen, listScreen; float infoW, infoH, listY, listW, listH; } Layout;

static Layout layout(void) {
    Layout L = { 0, 1, 400, 240, 0, 320, 240 };
    const UiBackend* b = ui_backend();
    if (!b) return L;
    if (b->screens >= 2) {
        L.listScreen = b->touchScreen >= 0 ? b->touchScreen : 1;
        L.infoScreen = !L.listScreen;
        L.infoW = (float)b->w[L.infoScreen]; L.infoH = (float)b->h[L.infoScreen];
        L.listW = (float)b->w[L.listScreen]; L.listH = (float)b->h[L.listScreen];
        L.listY = 0;
    } else {
        L.infoScreen = L.listScreen = 0;
        L.infoW = L.listW = (float)b->w[0];
        L.infoH = INFO_H;
        L.listY = INFO_H;
        L.listH = (float)b->h[0] - INFO_H;
    }
    return L;
}

static int visible_rows(const Layout* L) {
    int r = (int)((L->listH - TOOLS_H - 2) / ROW_H);
    return r < 1 ? 1 : r;
}

/* ---------- helpers ---------- */
static void set_status(Files* fs, uint32_t rgb, const char* s) {
    snprintf(fs->status, sizeof fs->status, "%s", s);
    fs->statusRgb = rgb;
}

static int in_root(const Files* fs) { return !strcmp(fs->dir, fs->root); }

static int ends_with(const char* s, const char* end) {
    size_t a = strlen(s), b = strlen(end);
    return a >= b && !strcmp(s + a - b, end);
}

static int is_dir(const char* path) {
    struct stat st;
    return !stat(path, &st) && S_ISDIR(st.st_mode);
}

static int exists(const char* path) {
    struct stat st;
    return !stat(path, &st);
}

/* projects first, then .cs files, then the rest (uses.txt); by name inside each group */
static int rank(const Entry* e) { return e->isDir ? 0 : ends_with(e->name, ".cs") ? 1 : 2; }
static int cmp_entry(const void* a, const void* b) {
    const Entry *x = (const Entry*)a, *y = (const Entry*)b;
    int r = rank(x) - rank(y);
    return r ? r : strcmp(x->name, y->name);
}

void files_scan(Files* fs) {
    fs->n = 0;
    DIR* d = opendir(fs->dir);
    if (d) {
        struct dirent* de;
        while ((de = readdir(d)) && fs->n < MAX_ENTRIES) {
            const char* nm = de->d_name;
            size_t len = strlen(nm);
            if (nm[0] == '.' || len + 2 > sizeof fs->e[0].name) continue;
            char path[480];
            snprintf(path, sizeof path, "%s/%s", fs->dir, nm);
            Entry* e = &fs->e[fs->n];
            if (is_dir(path)) {
                if (!in_root(fs) || !strcmp(nm, "tests")) continue;     /* projects are one level deep */
                e->isDir = 1;
                e->ncs = project_count_cs(path);
                snprintf(e->name, sizeof e->name, "%s/", nm);
            } else {
                if (!ends_with(nm, ".cs") && !ends_with(nm, ".txt")) continue;
                if (in_root(fs) && !ends_with(nm, ".cs")) continue;     /* log.txt of the app */
                e->isDir = 0; e->ncs = 0;
                snprintf(e->name, sizeof e->name, "%s", nm);
            }
            fs->n++;
        }
        closedir(d);
    }
    qsort(fs->e, fs->n, sizeof fs->e[0], cmp_entry);
    if (fs->sel >= fs->n) fs->sel = fs->n ? fs->n - 1 : 0;
    if (fs->sel < 0) fs->sel = 0;
}

/* ---------- life ---------- */
Files* files_new(const char* root) {
    Files* fs = (Files*)calloc(1, sizeof(Files));
    if (!fs) return NULL;
    snprintf(fs->root, sizeof fs->root, "%s", root);
    snprintf(fs->dir, sizeof fs->dir, "%s", root);
    files_scan(fs);
    set_status(fs, 0x8090A0, "");
    return fs;
}

void files_free(Files* fs) { free(fs); }

const char* files_chosen(const Files* fs) { return fs->chosen; }
int files_count(const Files* fs) { return fs->n; }
const char* files_name(const Files* fs, int i) { return i >= 0 && i < fs->n ? fs->e[i].name : ""; }
int files_selected(const Files* fs) { return fs->sel; }
const char* files_dir(const Files* fs) { return fs->dir; }
const char* files_status(const Files* fs) { return fs->status; }
int files_naming(const Files* fs) { return fs->naming != NAMING_NONE; }

int files_project_of(const char* root, const char* path, char* dir, int cap) {
    size_t rl = strlen(root);
    const char* slash = strrchr(path, '/');
    if (!slash || strncmp(path, root, rl) || path[rl] != '/' || slash == path + rl) return 0;
    int n = (int)(slash - path);
    if (n >= cap) return 0;
    memcpy(dir, path, n);
    dir[n] = 0;
    return 1;
}

/* ---------- actions ---------- */
static void enter_dir(Files* fs, const char* name) {
    int n = (int)strlen(name) - 1;                       /* "Game/" -> root/Game */
    snprintf(fs->dir, sizeof fs->dir, "%s/%.*s", fs->root, n > 0 && n < 68 ? n : 0, name);
    fs->sel = fs->top = 0;
    files_scan(fs);
    set_status(fs, 0x8090A0, "");
}

static void go_up(Files* fs) {
    const char* cur = strrchr(fs->dir, '/');
    char was[80];
    snprintf(was, sizeof was, "%s/", cur ? cur + 1 : "");
    snprintf(fs->dir, sizeof fs->dir, "%s", fs->root);
    fs->sel = fs->top = 0;
    files_scan(fs);
    for (int i = 0; i < fs->n; i++) if (!strcmp(fs->e[i].name, was)) fs->sel = i;   /* back on the project we left */
    set_status(fs, 0x8090A0, "");
}

/* 1 = open in the editor */
static int open_selected(Files* fs) {
    if (fs->sel >= fs->n) return 0;
    Entry* e = &fs->e[fs->sel];
    if (e->isDir) { enter_dir(fs, e->name); return 0; }
    snprintf(fs->chosen, sizeof fs->chosen, "%s/%s", fs->dir, e->name);
    return 1;
}

static void start_naming(Files* fs, int what) {
    if (what == NAMING_PROJECT && !in_root(fs)) { set_status(fs, 0xF0D080, "a project cannot hold another project"); return; }
    fs->naming = what;
    fs->name[0] = 0;
    kbd_reset();
    kbd_set_enter_label("Create");
    set_status(fs, 0x8090A0, "");
}

/* a class name: a letter, then letters, digits, _ (ASCII: file names on the card) */
static int valid_name(const char* s) {
    if (!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))) return 0;
    for (; *s; s++)
        if (!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_')) return 0;
    return 1;
}

static int write_text(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");                    /* UTF-8 without BOM */
    if (!f) return 0;
    int ok = fputs(text, f) >= 0;
    return fclose(f) == 0 && ok;
}

/* the starting text of a new file: a whole program, or (in a project that already has files) an empty class */
static void starting_text(char* out, int cap, const char* cls, int withMain) {
    if (withMain)
        snprintf(out, cap, "class %s\n{\n    static void Main()\n    {\n        Console.WriteLine(\"Hello!\");\n    }\n}\n", cls);
    else
        snprintf(out, cap, "class %s\n{\n}\n", cls);
}

/* 1 = created and chosen for the editor */
static int create(Files* fs) {
    if (!valid_name(fs->name)) {
        set_status(fs, 0xFF7070, "start with a letter; then letters A-Z, digits and _");
        return 0;
    }
    char path[480], text[256];
    if (fs->naming == NAMING_PROJECT) {
        char dir[400];
        snprintf(dir, sizeof dir, "%s/%s", fs->root, fs->name);
        if (exists(dir)) { set_status(fs, 0xFF7070, "there is already a project with this name"); return 0; }
        if (make_dir(dir) != 0) { set_status(fs, 0xFF7070, "cannot create the folder"); return 0; }
        snprintf(path, sizeof path, "%s/Program.cs", dir);
        starting_text(text, sizeof text, "Program", 1);
        snprintf(fs->dir, sizeof fs->dir, "%s", dir);
    } else {
        snprintf(path, sizeof path, "%s/%s.cs", fs->dir, fs->name);
        if (exists(path)) { set_status(fs, 0xFF7070, "there is already a file with this name"); return 0; }
        starting_text(text, sizeof text, fs->name, in_root(fs) || project_count_cs(fs->dir) == 0);
    }
    if (!write_text(path, text)) { set_status(fs, 0xFF7070, "cannot write the file"); return 0; }
    fs->naming = NAMING_NONE;
    files_scan(fs);
    const char* base = strrchr(path, '/') + 1;
    for (int i = 0; i < fs->n; i++) if (!strcmp(fs->e[i].name, base)) fs->sel = i;
    snprintf(fs->chosen, sizeof fs->chosen, "%s", path);
    return 1;
}

void files_key(Files* fs, const char* k) {
    if (!k || !fs->naming) return;
    size_t n = strlen(fs->name);
    if (!strcmp(k, KBD_BACKSPACE)) {
        if (n) fs->name[n - 1] = 0;
    } else if (!strcmp(k, KBD_ENTER)) {
        create(fs);
    } else if (n + strlen(k) <= NAME_MAX_LEN) strcat(fs->name, k);
}

/* ---------- deleting ---------- */
/* SELECT on an entry asks, the second SELECT on the same entry deletes it. A project folder is deleted only
 * when it is empty (its files are deleted one by one first: nothing disappears by one wrong press). */
static void delete_selected(Files* fs) {
    if (fs->sel >= fs->n) return;
    Entry* e = &fs->e[fs->sel];
    char msg[200];
    if (strcmp(fs->armed, e->name)) {
        snprintf(fs->armed, sizeof fs->armed, "%s", e->name);
        snprintf(msg, sizeof msg, "SELECT again: delete %s", e->name);
        set_status(fs, 0xFF9060, msg);
        return;
    }
    fs->armed[0] = 0;
    char path[480];
    int n = (int)strlen(e->name);
    if (e->isDir) n--;                                  /* "Game/" -> Game */
    snprintf(path, sizeof path, "%s/%.*s", fs->dir, n, e->name);
    int ok = e->isDir ? rmdir(path) == 0 : remove(path) == 0;
    if (!ok) {
        set_status(fs, 0xFF7070, e->isDir ? "the project is not empty: delete its files first" : "cannot delete the file");
        return;
    }
    snprintf(msg, sizeof msg, "deleted %.*s", n, e->name);
    int sel = fs->sel;
    files_scan(fs);
    fs->sel = sel < fs->n ? sel : (fs->n ? fs->n - 1 : 0);
    set_status(fs, 0x80E080, msg);
}

/* ---------- input ---------- */
static int repeat(uint32_t* since, uint32_t* last, const UiInput* in, uint32_t btn) {
    if (in->down & btn) { *since = *last = in->ms; return 1; }
    if (!(in->held & btn)) return 0;
    if (in->ms - *since < REPEAT_WAIT || in->ms - *last < REPEAT_EVERY) return 0;
    *last = in->ms;
    return 1;
}

static void keep_visible(Files* fs, const Layout* L) {
    int vis = visible_rows(L);
    if (fs->sel < fs->top) fs->top = fs->sel;
    if (fs->sel >= fs->top + vis) fs->top = fs->sel - vis + 1;
    if (fs->top < 0) fs->top = 0;
}

/* the tool buttons at the top of the list: Open, New file, New project / Back */
enum { TOOL_OPEN, TOOL_NEWFILE, TOOL_THIRD, NTOOLS };

static int tool(Files* fs, int t) {
    switch (t) {
    case TOOL_OPEN: return open_selected(fs);
    case TOOL_NEWFILE: start_naming(fs, NAMING_FILE); break;
    case TOOL_THIRD: if (in_root(fs)) start_naming(fs, NAMING_PROJECT); else go_up(fs); break;
    }
    return 0;
}

int files_update(Files* fs, const UiInput* in) {
    fs->ms = in->ms;
    Layout L = layout();
    fs->chosen[0] = 0;
    if (fs->naming) {
        if (in->down & (UI_BTN_START | UI_BTN_B)) { fs->naming = NAMING_NONE; set_status(fs, 0x8090A0, "cancelled"); return FILES_STAY; }
        kbd_set_area(0, L.listY + 80, L.listW, PANEL_H - 80);
        const char* k = kbd_update(in);
        if (k) files_key(fs, k);
        if (in->touchDown) {                       /* Create / Cancel above the keyboard */
            float py = (float)in->y - L.listY;
            if (py >= 0 && py < TOOLS_H) {
                if (in->x < L.listW / 2) files_key(fs, KBD_ENTER);
                else { fs->naming = NAMING_NONE; set_status(fs, 0x8090A0, "cancelled"); }
            }
        }
        if ((in->down & UI_BTN_A)) files_key(fs, KBD_ENTER);
        return fs->chosen[0] ? FILES_OPEN : FILES_STAY;
    }
    if (in->down & UI_BTN_START) return FILES_LEAVE;
    if (in->down & UI_BTN_B) {
        if (in_root(fs)) return FILES_LEAVE;
        go_up(fs);
    }
    int vis = visible_rows(&L);
    int wasSel = fs->sel;
    if (in->down & UI_BTN_SELECT) delete_selected(fs);
    else if (in->down && fs->armed[0]) { fs->armed[0] = 0; set_status(fs, 0x8090A0, ""); }   /* any other button cancels */
    if (repeat(&fs->upSince, &fs->upLast, in, UI_BTN_UP) && fs->sel > 0) fs->sel--;
    if (repeat(&fs->downSince, &fs->downLast, in, UI_BTN_DOWN) && fs->sel < fs->n - 1) fs->sel++;
    if (in->down & UI_BTN_L) fs->sel = fs->sel > vis ? fs->sel - vis : 0;
    if (in->down & UI_BTN_R) fs->sel = fs->sel + vis < fs->n ? fs->sel + vis : (fs->n ? fs->n - 1 : 0);
    int open = 0;
    if (in->down & UI_BTN_A) open = open_selected(fs);
    if (in->down & UI_BTN_X) start_naming(fs, NAMING_FILE);
    if (in->down & UI_BTN_Y) start_naming(fs, NAMING_PROJECT);
    if (in->touchDown && !open) {
        float px = (float)in->x, py = (float)in->y - L.listY;
        if (py >= 0 && py < TOOLS_H) open = tool(fs, (int)(px / (L.listW / NTOOLS)));
        else if (py >= TOOLS_H + 2) {
            int i = fs->top + (int)((py - TOOLS_H - 2) / ROW_H);
            if (i < fs->n) {
                if (i == fs->sel) open = open_selected(fs);     /* the second tap opens */
                else fs->sel = i;
            }
        }
    }
    if (fs->sel != wasSel && fs->armed[0]) { fs->armed[0] = 0; set_status(fs, 0x8090A0, ""); }
    keep_visible(fs, &L);
    return open ? FILES_OPEN : FILES_STAY;
}

/* ---------- drawing ---------- */
static void button(float x, float y, float w, float h, const char* s, uint32_t bg) {
    ui_rect(x + 1, y + 1, w - 2, h - 2, bg);
    float tw = ui_text_width(s, 12);
    ui_text(x + (w - tw) / 2, y + (h - 12) / 2, s, 12, 0xFFFFFF);
}

static void draw_info(Files* fs, const Layout* L) {
    ui_screen(L->infoScreen);
    ui_rect(0, 0, L->infoW, 16, 0x1C2230);
    const char* shown = fs->dir + strlen(fs->root);    /* "" in the root, "/Game" in a project */
    char title[300];
    snprintf(title, sizeof title, "%s", in_root(fs) ? "Programs and projects" : shown + 1);
    ui_text(6, 2, title, 12, 0x7FB2FF);
    float y = 22;
    if (fs->naming) {
        ui_text(8, y, fs->naming == NAMING_PROJECT ? "New project - name:" : "New file - class name:", 12, 0xC0C8D0);
        char field[NAME_MAX_LEN + 8];
        snprintf(field, sizeof field, "%s%s", fs->name, (fs->ms / 500) % 2 ? " " : "_");
        ui_text(8, y + 16, field, 16, 0xFFFFFF);
        y += 40;
        if (L->infoH > 100)
            ui_text(8, y, fs->naming == NAMING_PROJECT ? "A folder with Program.cs (Main) inside." :
                                                         "A file Name.cs with an empty class Name.", 12, 0x8090A0);
    } else if (L->infoH > 100) {
        const char* HELP[] = {
            in_root(fs) ? "A program is one .cs file. A project is a folder:" : "All .cs files of this project are compiled",
            in_root(fs) ? "all its .cs files are compiled together." : "together: classes see each other.",
            "",
            "A: open    B: back    X: new file",
            in_root(fs) ? "Y: new project   L/R: page   SELECT: delete" : "L / R: page    SELECT: delete",
        };
        for (int i = 0; i < (int)(sizeof HELP / sizeof HELP[0]); i++) ui_text(8, y + i * 15, HELP[i], 12, 0xA0A8B0);
        y += 5 * 15 + 6;
        if (fs->sel < fs->n && fs->e[fs->sel].isDir) {
            char about[120];
            snprintf(about, sizeof about, "%s  project, %d .cs file%s", fs->e[fs->sel].name, fs->e[fs->sel].ncs,
                     fs->e[fs->sel].ncs == 1 ? "" : "s");
            ui_text(8, y, about, 12, 0xF0D080);
        }
    }
    if (fs->status[0]) ui_text(8, L->infoH - 16, fs->status, 12, fs->statusRgb ? fs->statusRgb : 0x8090A0);
}

static void draw_list(Files* fs, const Layout* L) {
    ui_screen(L->listScreen);
    float y = L->listY, bw = L->listW / NTOOLS;
    if (fs->naming) {
        button(0, y, L->listW / 2, TOOLS_H, "Create", 0x2F6B3A);
        button(L->listW / 2, y, L->listW / 2, TOOLS_H, "Cancel", 0x34405A);
        kbd_set_area(0, y + 80, L->listW, PANEL_H - 80);
        kbd_draw();
        return;
    }
    button(0, y, bw, TOOLS_H, "Open", 0x2F6B3A);
    button(bw, y, bw, TOOLS_H, "New file", 0x34405A);
    button(2 * bw, y, bw, TOOLS_H, in_root(fs) ? "New project" : "Back", 0x34405A);
    int vis = visible_rows(L);
    if (!fs->n) ui_text(8, y + TOOLS_H + 8, in_root(fs) ? "no programs yet: X = new file" : "no files", 12, 0x8090A0);
    for (int r = 0; r < vis && fs->top + r < fs->n; r++) {
        int i = fs->top + r;
        float ry = y + TOOLS_H + 2 + r * ROW_H;
        if (i == fs->sel) ui_rect(0, ry, L->listW, ROW_H, 0x2D4A80);
        uint32_t c = fs->e[i].isDir ? 0xF0D080 : 0xC0C8D0;
        ui_text(10, ry + 5, fs->e[i].name, 13, i == fs->sel ? 0xFFFFFF : c);
    }
}

void files_draw(Files* fs) {
    Layout L = layout();
    draw_info(fs, &L);
    draw_list(fs, &L);
}
