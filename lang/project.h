/* MiniSharp - a project is a folder: every .cs file in it is compiled together, plus the .cs files of the
 * shared library folders listed in its uses.txt (one path per line, relative to the project, e.g. ../shared/lib). */
#ifndef MS_PROJECT_H
#define MS_PROJECT_H

typedef struct {
    char name[64];                      /* shown in errors: "Game.cs", library files "lib/Inventory.cs" */
    char path[320];                     /* where it is read from */
    int firstLine;
    int lib;                            /* 1 = from a uses.txt folder (the editor shows it read-only) */
} ProjFile;

typedef struct {
    char* src;                          /* the files one after another */
    ProjFile* files; int nfiles;        /* project files sorted by name, then each library folder's; firstLine = their line 1 in src */
} Project;

/* 1 = ok; 0 = err says why (no .cs files, cannot read, too big) */
int project_load(const char* dir, Project* pr, char* err, int errlen);
void project_free(Project* pr);
int project_count_cs(const char* dir);  /* how many .cs files the folder has */

/* the library folders of dir/uses.txt, with ".." resolved; returns how many (0 = no uses.txt).
 * A folder that does not exist is still returned, project_load then reports it. */
int project_uses(const char* dir, char out[][320], int max);

/* "line 57: msg" / "line 57 (A.B): msg" -> "Player.cs line 12: msg" (in place) */
void project_map_error(const Project* pr, char* err, int errlen);

#endif
