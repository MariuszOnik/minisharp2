/* MiniSharp - the PC test host's screens in memory (see swgfx.c). */
#ifndef MS_SWGFX_H
#define MS_SWGFX_H

#include "gfx.h"

const GfxBackend* swgfx_init(void);     /* two black screens: 400x240 and 320x240; NULL = no memory */
void swgfx_free(void);
void swgfx_begin_frame(void);           /* both screens black again */
uint32_t swgfx_pixel(int screen, int x, int y);     /* 0xRRGGBB */
uint32_t swgfx_hash(void);              /* FNV-1a of both screens: a test compares the picture by this number */
int swgfx_save_ppm(const char* path);   /* 400 x 480: the top screen above the bottom one */

#endif
