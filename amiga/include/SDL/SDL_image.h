/* Wizznic for AmigaOS: the one SDL_image 1.2 call it makes, IMG_Load,
 * on libpng (amiga/sdl_image_lite.c). The OpenGPU kit's SDL 1.2
 * (sdl12-compat) comes without an SDL_image 1.2.
 * The AmiCradle Team. GPL 3 or later, like Wizznic. */
#ifndef AMIGA_SDL_IMAGE_LITE_H
#define AMIGA_SDL_IMAGE_LITE_H
#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
SDL_Surface *IMG_Load(const char *file);
#define IMG_GetError SDL_GetError
#ifdef __cplusplus
}
#endif
#endif
