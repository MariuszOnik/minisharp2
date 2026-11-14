/* MiniSharp2 - module iso: depth sorting of isometric boxes (the sorting of unity-iso-tools, see iso_sort.c).
 *
 * Coordinates: iso x, y on the floor and z up, in tiles; the screen is in "base pixels" (y grows downwards, iso
 * (0,0,0) at (0,0)) – the camera adds its offset and zoom. Every object is a box: its minimal corner and its size.
 * Boxes must not intersect (as in the original). Objects that do not move get their dependencies found once;
 * moving ones are tested every frame against what is near them on the screen.
 *
 * Several sorters may live at once (the map and a preview in the prefab tool): all state is in IsoSort. */
#ifndef MS_ISO_SORT_H
#define MS_ISO_SORT_H

typedef struct IsoSort IsoSort;

IsoSort* iso_new(void);                         /* NULL = out of memory; tile 32, ratio 0.5, angle 45, height 32 */
void iso_free(IsoSort* w);
/* tileSize: width of a tile's side (A in unity-iso-tools); ratio 0.25..1 (B = A * ratio); angle 0..90 (D);
 * tileHeight: pixels of one unit of z (C) */
void iso_setup(IsoSort* w, float tileSize, float ratio, float angle, float tileHeight);
void iso_to_screen(const IsoSort* w, float x, float y, float z, float* sx, float* sy);
void iso_from_screen(const IsoSort* w, float sx, float sy, float z, float* x, float* y);   /* the iso point at height z */

int iso_add(IsoSort* w, float x, float y, float z, float sx, float sy, float sz, int moving);   /* id, -1 out of memory */
int iso_valid(const IsoSort* w, int id);
void iso_move(IsoSort* w, int id, float x, float y, float z);      /* a bad id does nothing */
void iso_resize(IsoSort* w, int id, float sx, float sy, float sz);
void iso_remove(IsoSort* w, int id);           /* the id may be given to the next iso_add */
void iso_clear(IsoSort* w);
int iso_count(const IsoSort* w);               /* objects alive */

/* Sorts the objects whose screen rectangle touches the view (base pixels). Returns how many are visible
 * (then iso_at(i), 0 <= i < that, is the i-th to draw, the farthest first) or -1 = out of memory. */
int iso_sort(IsoSort* w, float minX, float minY, float maxX, float maxY);
int iso_at(const IsoSort* w, int i);
enum { ISO_STAT_PAIRS, ISO_STAT_DEPS, ISO_STAT_STATIC_PAIRS, ISO_STAT_VISIBLE };
int iso_stat(const IsoSort* w, int which);     /* of the last iso_sort: pairs tested, dependencies found, ... */

/* 1 = box a (minimal corner a[0..2], size a[3..5]) is behind box b, so b must be drawn after it
 * (IsoScreenSolver.IsIsoObjectDepends); for tests and tools */
int iso_behind(const float a[6], const float b[6]);

#endif
