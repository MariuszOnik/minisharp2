/* MiniSharp - projects. The .cs files of a folder (and of the library folders in its uses.txt or found
 * by 'using': using Dice; adds ../dice/lib or ../lib/Dice) are joined
 * into one source (classes may be in any file) with byte 0x1C between files, so 'using' and 'namespace X;'
 * stay in their file, and line numbers in errors are mapped back. */
#include "project.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_FILES 96
#define MAX_TOTAL (4 * 1024 * 1024)

static int is_cs(const char* name) {
    size_t n = strlen(name);
    return n > 3 && n < sizeof(((ProjFile*)0)->name) && !strcmp(name + n - 3, ".cs");
}

static int cmpname(const void* a, const void* b) { return strcmp(((const ProjFile*)a)->name, ((const ProjFile*)b)->name); }

int project_count_cs(const char* dir) {
    DIR* d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d))) if (is_cs(e->d_name)) n++;
    closedir(d);
    return n;
}

/* "a/b/../c/./d" -> "a/c/d"; keeps a leading "/" or "sdmc:" and the leading ".." of a relative path
 * (the 3DS card does not understand ".." itself) */
static void normalize(const char* in, char* out, int cap) {
    char buf[512], *seg[64];
    int n = 0, abs = in[0] == '/' || in[0] == '\\';
    snprintf(buf, sizeof buf, "%s", in);
    for (char* c = buf; *c; c++) if (*c == '\\') *c = '/';
    for (char* t = strtok(buf, "/"); t && n < 64; t = strtok(NULL, "/")) {
        if (!strcmp(t, ".")) continue;
        if (!strcmp(t, "..") && n > 0 && strcmp(seg[n - 1], "..") && !strchr(seg[n - 1], ':')) { n--; continue; }
        seg[n++] = t;
    }
    int len = snprintf(out, cap, "%s", abs ? "/" : "");
    for (int i = 0; i < n && len < cap; i++)
        len += snprintf(out + len, cap - len, "%s%s", seg[i], i + 1 < n ? "/" : "");   /* "sdmc:" + "/" + ... */
    if (len == 0) snprintf(out, cap, ".");
}

int project_uses(const char* dir, char out[][320], int max) {
    char path[512];
    snprintf(path, sizeof path, "%s/uses.txt", dir);
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    char self[320], line[300];
    normalize(dir, self, sizeof self);
    int n = 0;
    while (n < max && fgets(line, sizeof line, f)) {
        char* s = line;
        while (*s == ' ' || *s == '\t' || (unsigned char)*s == 0xEF || (unsigned char)*s == 0xBB || (unsigned char)*s == 0xBF) s++;   /* spaces, UTF-8 BOM */
        char* e = s + strlen(s);
        while (e > s && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t' || e[-1] == '/' || e[-1] == '\\')) *--e = 0;
        if (!*s || *s == '#' || (s[0] == '/' && s[1] == '/')) continue;      /* empty line or comment */
        char full[640];
        if (*s == '/' || strchr(s, ':')) snprintf(full, sizeof full, "%s", s);
        else snprintf(full, sizeof full, "%s/%s", dir, s);
        normalize(full, out[n], 320);
        int dup = !strcmp(out[n], self);
        for (int i = 0; i < n && !dup; i++) dup = !strcmp(out[i], out[n]);
        if (!dup) n++;
    }
    fclose(f);
    return n;
}

/* adds the .cs files of one folder, sorted by name; lib = the folder's name to put before them ("" = the project) */
static int add_folder(Project* pr, const char* dir, const char* lib, char* err, int errlen) {
    DIR* d = opendir(dir);
    if (!d) { snprintf(err, errlen, *lib ? "uses.txt: cannot open folder %s" : "cannot open folder %s", dir); return 0; }
    int first = pr->nfiles;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (!is_cs(e->d_name)) continue;
        if (pr->nfiles == MAX_FILES) { closedir(d); snprintf(err, errlen, "more than %d .cs files in the project", MAX_FILES); return 0; }
        ProjFile* pf = &pr->files[pr->nfiles++];
        if (*lib) snprintf(pf->name, sizeof pf->name, "%.30s/%.32s", lib, e->d_name);
        else snprintf(pf->name, sizeof pf->name, "%s", e->d_name);
        snprintf(pf->path, sizeof pf->path, "%s/%s", dir, e->d_name);
        pf->lib = *lib != 0;
    }
    closedir(d);
    qsort(pr->files + first, pr->nfiles - first, sizeof(ProjFile), cmpname);
    return 1;
}

/* reads files [from, nfiles) into pr->src after what is there; 0 + err on failure */
static int read_files(Project* pr, int from, size_t* len, size_t* cap, int* line, char* err, int errlen) {
    for (int i = from; i < pr->nfiles; i++) {
        FILE* f = fopen(pr->files[i].path, "rb");
        if (!f) { snprintf(err, errlen, "cannot read %s", pr->files[i].name); return 0; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        if (n < 0 || *len + n + 3 > MAX_TOTAL) { fclose(f); snprintf(err, errlen, "project too big"); return 0; }
        if (*len + n + 3 > *cap) {
            *cap = (*len + n + 3) * 2;
            char* s = (char*)realloc(pr->src, *cap);
            if (!s) { fclose(f); snprintf(err, errlen, "out of memory"); return 0; }
            pr->src = s;
        }
        if (i > 0) pr->src[(*len)++] = 0x1C;  /* file boundary for the parser: 'using' and 'namespace X;' end here */
        size_t got = fread(pr->src + *len, 1, n, f);
        fclose(f);
        pr->files[i].firstLine = *line;
        for (size_t k = 0; k < got; k++) if (pr->src[*len + k] == '\n') (*line)++;
        *len += got;
        if (got == 0 || pr->src[*len - 1] != '\n') { pr->src[(*len)++] = '\n'; (*line)++; }   /* next file on a new line */
        pr->src[*len] = 0;
    }
    return 1;
}

static int is_id(int c) { return isalnum(c) || c == '_'; }

/* the first part of each 'using X.Y;' (into uses) and 'namespace X.Y' (into decl) in src: "Dice" */
static void scan_names(const char* s, char uses[][48], int* nu, char decl[][48], int* nd, int max) {
    int atStmt = 1;                             /* a statement can start here (after ; { } or the start) */
    for (const char* p = s; *p; ) {
        if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
        if (p[0] == '/' && p[1] == '*') { p += 2; while (*p && !(p[0] == '*' && p[1] == '/')) p++; if (*p) p += 2; continue; }
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            while (*p && *p != q && *p != '\n') p += *p == '\\' && p[1] ? 2 : 1;
            if (*p) p++;
            atStmt = 0; continue;
        }
        if (*p == ';' || *p == '{' || *p == '}' || *p == 0x1C) { atStmt = 1; p++; continue; }
        if (isspace((unsigned char)*p)) { p++; continue; }
        if (!isalpha((unsigned char)*p) && *p != '_') { atStmt = 0; p++; continue; }
        const char* w = p;
        while (is_id((unsigned char)*p)) p++;
        int wl = (int)(p - w), isUse = wl == 5 && !memcmp(w, "using", 5), isNs = wl == 9 && !memcmp(w, "namespace", 9);
        if (atStmt && (isUse || isNs)) {
            const char* a = p;
            while (*a == ' ' || *a == '\t') a++;
            const char* b = a;
            while (is_id((unsigned char)*b)) b++;
            const char* after = b;
            while (*after == '.' || is_id((unsigned char)*after)) after++;
            while (*after == ' ' || *after == '\t') after++;
            int ok = b > a && !(isUse && (*after == '=' || (b - a == 6 && !memcmp(a, "static", 6))));
            char (*list)[48] = isUse ? uses : decl;
            int* n = isUse ? nu : nd;
            if (ok && *n < max && b - a < 48) {
                int dup = 0;
                for (int i = 0; i < *n && !dup; i++) dup = (int)strlen(list[i]) == b - a && !memcmp(list[i], a, b - a);
                if (!dup) { memcpy(list[*n], a, b - a); list[*n][b - a] = 0; (*n)++; }
            }
            p = b;
        }
        atStmt = 0;
    }
}

static int dir_exists(const char* path) {
    DIR* d = opendir(path);
    if (d) closedir(d);
    return d != NULL;
}

int project_load(const char* dir, Project* pr, char* err, int errlen) {
    memset(pr, 0, sizeof *pr);
    pr->files = (ProjFile*)calloc(MAX_FILES, sizeof(ProjFile));
    if (!pr->files) { snprintf(err, errlen, "out of memory"); return 0; }
    if (!add_folder(pr, dir, "", err, errlen)) { project_free(pr); return 0; }
    if (pr->nfiles == 0) { snprintf(err, errlen, "no .cs files in %s", dir); project_free(pr); return 0; }
    static char folders[32][320];               /* library folders added: uses.txt, then found by 'using' */
    int nfolders = project_uses(dir, folders, 16);
    for (int u = 0; u < nfolders; u++) {
        const char* last = strrchr(folders[u], '/');
        last = last ? last + 1 : folders[u];
        if (!add_folder(pr, folders[u], *last ? last : "lib", err, errlen)) { project_free(pr); return 0; }
    }
    size_t len = 0, cap = 0;
    int line = 1;
    if (!read_files(pr, 0, &len, &cap, &line, err, errlen)) { project_free(pr); return 0; }

    /* 'using Dice;' where no file declares namespace Dice: the library folder ../dice/lib (next to the
     * project, as sdmc:/minisharp/dice/lib) or ../lib/Dice; its files may use further libraries */
    char self[320];
    normalize(dir, self, sizeof self);
    for (int pass = 0; pass < 8; pass++) {
        static char uses[32][48], decl[64][48];
        int nu = 0, nd = 0;
        scan_names(pr->src, uses, &nu, decl, &nd, 32);
        int from = pr->nfiles;
        for (int u = 0; u < nu; u++) {
            int declared = 0;
            for (int d = 0; d < nd && !declared; d++) declared = !strcmp(uses[u], decl[d]);
            if (declared) continue;
            char lower[48], cand[2][640], full[320];
            int k = 0;
            for (; uses[u][k]; k++) lower[k] = (char)tolower((unsigned char)uses[u][k]);
            lower[k] = 0;
            snprintf(cand[0], sizeof cand[0], "%.300s/../%.47s/lib", dir, lower);
            snprintf(cand[1], sizeof cand[1], "%.300s/../lib/%.47s", dir, uses[u]);
            for (int c = 0; c < 2; c++) {
                normalize(cand[c], full, sizeof full);
                if (!strcmp(full, self) || !dir_exists(full)) continue;
                int dup = 0;
                for (int i = 0; i < nfolders && !dup; i++) dup = !strcmp(folders[i], full);
                if (dup || nfolders == 32) break;
                snprintf(folders[nfolders++], sizeof folders[0], "%s", full);
                if (!add_folder(pr, full, lower, err, errlen)) { project_free(pr); return 0; }
                break;
            }
        }
        if (pr->nfiles == from) break;
        for (int i = from; i < pr->nfiles; i++) pr->files[i].lib = 1;
        if (!read_files(pr, from, &len, &cap, &line, err, errlen)) { project_free(pr); return 0; }
    }
    return 1;
}

void project_free(Project* pr) {
    free(pr->src);
    free(pr->files);
    memset(pr, 0, sizeof *pr);
}

void project_map_error(const Project* pr, char* err, int errlen) {
    if (strncmp(err, "line ", 5) || !isdigit((unsigned char)err[5]) || pr->nfiles == 0) return;
    char* rest;
    long line = strtol(err + 5, &rest, 10);
    int fi = 0;
    while (fi + 1 < pr->nfiles && pr->files[fi + 1].firstLine <= line) fi++;
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s line %ld%s", pr->files[fi].name, line - pr->files[fi].firstLine + 1, rest);
    snprintf(err, errlen, "%s", tmp);
}
