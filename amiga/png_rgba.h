/* PNG to 8-bit RGBA rows, on libpng, for the SDL_image shim.
 * (From the OpenLieroX port.) The AmiCradle Team. GPL 3 or later, like Wizznic. */
#ifndef AMIGA_PNG_RGBA_H
#define AMIGA_PNG_RGBA_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Decodes a whole PNG held in memory. Returns malloc()ed RGBA (w*h*4
 * bytes, R first), or NULL; *has_alpha says whether any pixel is not
 * opaque (tRNS or an alpha channel). */
unsigned char *amiga_png_decode(const void *data, size_t size, int *w, int *h, int *has_alpha);
#ifdef __cplusplus
}
#endif
#endif
