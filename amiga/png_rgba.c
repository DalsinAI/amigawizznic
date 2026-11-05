/* PNG to 8-bit RGBA rows, on libpng (see png_rgba.h).
 * (From the OpenLieroX port.) The AmiCradle Team. GPL 3 or later, like Wizznic. */
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <png.h>
#include "png_rgba.h"

struct mem_in { const unsigned char *p; size_t left; };

/* libpng's warnings (iCCP profiles and the like, common in the game's
 * pictures) would go to the Shell; the pictures load all the same. */
static void quiet(png_structp png, png_const_charp msg)
{
	(void)png;
	(void)msg;
}

static void read_mem(png_structp png, png_bytep out, png_size_t n)
{
	struct mem_in *m = (struct mem_in *)png_get_io_ptr(png);
	if (n > m->left)
		png_error(png, "PNG data ends early");
	memcpy(out, m->p, n);
	m->p += n;
	m->left -= n;
}

unsigned char *amiga_png_decode(const void *data, size_t size, int *w, int *h, int *has_alpha)
{
	png_structp png;
	png_infop info;
	struct mem_in in;
	unsigned char *volatile pixels = NULL;
	png_bytep *volatile rows = NULL;
	png_uint_32 pw, ph, y;
	int depth, ctype, alpha_seen;
	size_t i;

	if (size < 8 || png_sig_cmp((png_const_bytep)data, 0, 8))
		return NULL;
	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, quiet);
	if (!png)
		return NULL;
	info = png_create_info_struct(png);
	if (!info) {
		png_destroy_read_struct(&png, NULL, NULL);
		return NULL;
	}
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, &info, NULL);
		free(rows);
		free(pixels);
		return NULL;
	}
	in.p = (const unsigned char *)data;
	in.left = size;
	png_set_read_fn(png, &in, read_mem);
	png_read_info(png, info);
	png_get_IHDR(png, info, &pw, &ph, &depth, &ctype, NULL, NULL, NULL);
	if (pw == 0 || ph == 0 || pw > 16384 || ph > 16384)
		png_error(png, "bad PNG size");

	/* everything to 8-bit RGBA */
	if (depth == 16)
		png_set_strip_16(png);
	if (ctype == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);
	if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);
	if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);
	png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
	png_set_interlace_handling(png);
	png_read_update_info(png, info);

	pixels = (unsigned char *)malloc((size_t)pw * ph * 4);
	rows = (png_bytep *)malloc(sizeof(png_bytep) * ph);
	if (!pixels || !rows)
		png_error(png, "out of memory");
	for (y = 0; y < ph; y++)
		rows[y] = pixels + (size_t)y * pw * 4;
	png_read_image(png, rows);
	png_read_end(png, NULL);
	png_destroy_read_struct(&png, &info, NULL);
	free(rows);

	alpha_seen = 0;
	for (i = 3; i < (size_t)pw * ph * 4; i += 4)
		if (pixels[i] != 0xFF) {
			alpha_seen = 1;
			break;
		}
	*w = (int)pw;
	*h = (int)ph;
	*has_alpha = alpha_seen;
	return pixels;
}
