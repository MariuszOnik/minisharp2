/* MiniSharp - decoding pictures for Gfx.LoadImage: PNG (and JPG, BMP, GIF, TGA) through stb_image, which is in the
 * public domain (lib/third_party/stb_image.h, by Sean Barrett). The result is RGBA, 4 bytes per pixel, rows from top. */
#include "image.h"
#include <stdlib.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wshift-negative-value"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_THREAD_LOCALS
#include "third_party/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

unsigned char* image_load_file(const char* path, int* w, int* h, const char** why) {
    int n;
    unsigned char* px = stbi_load(path, w, h, &n, 4);
    if (!px && why) *why = stbi_failure_reason();
    return px;
}

void image_free_pixels(unsigned char* px) { stbi_image_free(px); }
