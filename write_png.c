#include <cairo.h>
#include <png.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "write_png.h"

static void pack_row32(uint8_t *restrict row_out, const uint32_t *restrict row_in,
		size_t width, bool fully_opaque) {
	for (size_t x = 0; x < width; x++) {
		uint8_t b = (row_in[x] >>  0) & 0xff;
		uint8_t g = (row_in[x] >>  8) & 0xff;
		uint8_t r = (row_in[x] >> 16) & 0xff;
		uint8_t a = (row_in[x] >> 24) & 0xff;

		// Unpremultiply pixels, if necessary. In practice, few images
		// made by grim will have many pixels with fractional alpha
		if (!fully_opaque && (a != 0 && a != 255)) {
			uint32_t inv = (0xff << 16) / a;
			uint32_t sr = r * inv;
			r = sr > (0xff << 16) ? 0xff : (sr >> 16);
			uint32_t sg = g * inv;
			g = sg > (0xff << 16) ? 0xff : (sg >> 16);
			uint32_t sb = b * inv;
			b = sb > (0xff << 16) ? 0xff : (sb >> 16);
		}

		*row_out++ = r;
		*row_out++ = g;
		*row_out++ = b;
		if (!fully_opaque) {
			*row_out++ = a;
		}
	}
}

cairo_status_t write_to_png_stream(cairo_surface_t *image, FILE *stream,
		int comp_level) {
	cairo_format_t format = cairo_image_surface_get_format(image);
	if (format != CAIRO_FORMAT_RGB24 && format != CAIRO_FORMAT_ARGB32) {
		return CAIRO_STATUS_INVALID_FORMAT;
	}

	int width = cairo_image_surface_get_width(image);
	int height = cairo_image_surface_get_height(image);
	int stride = cairo_image_surface_get_stride(image);
	const unsigned char *data = cairo_image_surface_get_data(image);

	bool fully_opaque = true;
	if (format == CAIRO_FORMAT_ARGB32) {
		for (int y = 0; y < height; y++) {
			const uint32_t *row = (const uint32_t *)(data + y * stride);
			for (int x = 0; x < width; x++) {
				if ((row[x] >> 24) != 0xff) {
					fully_opaque = false;
				}
			}
		}
	}
	int color_type = fully_opaque ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA;
	int bit_depth = 8;

	uint8_t *tmp_row = calloc(width, 4);
	if (!tmp_row) {
		return CAIRO_STATUS_NO_MEMORY;
	}

	cairo_status_t status = CAIRO_STATUS_SUCCESS;
	png_struct *png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL);
	png_info *info = NULL;
	if (!png) {
		status = CAIRO_STATUS_NO_MEMORY;
		goto cleanup;
	}
	info = png_create_info_struct(png);
	if (!info) {
		status = CAIRO_STATUS_NO_MEMORY;
		goto cleanup;
	}

#ifdef PNG_SETJMP_SUPPORTED
	if (setjmp(png_jmpbuf(png))) {
		status = CAIRO_STATUS_PNG_ERROR;
		goto cleanup;
	}
#endif

	png_init_io(png, stream);

	png_set_IHDR(png, info, width, height, bit_depth, color_type,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png, info);

	// If the level is zero (no compression), filtering will be unnecessary
	png_set_compression_level(png, comp_level);
	if (comp_level == 0) {
		png_set_filter(png, 0, PNG_NO_FILTERS);
	} else {
		png_set_filter(png, 0, PNG_ALL_FILTERS);
	}

	for (int y = 0; y < height; y++) {
		const uint32_t *row = (const uint32_t *)(data + y * stride);
		pack_row32(tmp_row, row, width, fully_opaque);
		png_write_row(png, tmp_row);
	}

	png_write_end(png, NULL);

cleanup:
	if (info) {
		png_destroy_info_struct(png, &info);
	}
	if (png) {
		png_destroy_write_struct(&png, NULL);
	}
	free(tmp_row);
	return status;
}
