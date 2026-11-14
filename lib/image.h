/* MiniSharp - decoding a picture file into RGBA pixels (lib/image.c, stb_image). */
#ifndef MS_IMAGE_H
#define MS_IMAGE_H

/* RGBA, 4 bytes per pixel, rows from the top; NULL = cannot read or decode (*why says why); free with image_free_pixels */
unsigned char* image_load_file(const char* path, int* w, int* h, const char** why);
void image_free_pixels(unsigned char* px);

#endif
