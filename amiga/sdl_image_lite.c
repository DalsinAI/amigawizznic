/* IMG_Load for Wizznic on AmigaOS (see include/SDL/SDL_image.h).
 * PNGs become 32-bit surfaces, with SDL_SRCALPHA when a pixel is not
 * opaque; anything else goes to SDL_LoadBMP.
 * Wizznic's pictures are all PNG. (From the OpenLieroX port.)
 * The AmiCradle Team. GPL 3 or later, like Wizznic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL_image.h"
#include "png_rgba.h"

static unsigned char *read_file(const char *name, size_t *size)
{
	FILE *f = fopen(name, "rb");
	unsigned char *buf = NULL;
	long len;

	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0) {
		buf = (unsigned char *)malloc((size_t)len);
		if (buf && fread(buf, 1, (size_t)len, f) != (size_t)len) {
			free(buf);
			buf = NULL;
		}
		*size = (size_t)len;
	}
	fclose(f);
	return buf;
}

SDL_Surface *IMG_Load(const char *file)
{
	size_t size = 0;
	unsigned char *data, *rgba;
	SDL_Surface *s;
	int w, h, alpha, y;

	if (!file) {
		SDL_SetError("IMG_Load: no file name");
		return NULL;
	}
	data = read_file(file, &size);
	if (!data) {
		SDL_SetError("IMG_Load: cannot read %s", file);
		printf("W: IMG_Load: cannot read %s (%lu bytes)\n", file, (unsigned long)size);
		return NULL;
	}
	if (size < 8 || memcmp(data, "\211PNG\r\n\032\n", 8) != 0) {
		free(data);
		return SDL_LoadBMP(file);
	}
	rgba = amiga_png_decode(data, size, &w, &h, &alpha);
	free(data);
	if (!rgba) {
		SDL_SetError("IMG_Load: bad PNG %s", file);
		printf("W: IMG_Load: cannot decode %s (%lu bytes)\n", file, (unsigned long)size);
		return NULL;
	}
	/* bytes R, G, B, A in memory, as big-endian 32-bit pixels */
	s = SDL_CreateRGBSurface(SDL_SWSURFACE | (alpha ? SDL_SRCALPHA : 0), w, h, 32,
	                         0xFF000000u, 0x00FF0000u, 0x0000FF00u, alpha ? 0x000000FFu : 0);
	if (s) {
		for (y = 0; y < h; y++)
			memcpy((Uint8 *)s->pixels + (size_t)y * s->pitch, rgba + (size_t)y * w * 4, (size_t)w * 4);
	}
	if (!s)
		printf("W: IMG_Load: no %dx%d surface for %s: %s\n", w, h, file, SDL_GetError());
	free(rgba);
	return s;
}
