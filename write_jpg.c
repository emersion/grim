/**
 * @author Bernhard R. Fischer, 4096R/8E24F29D bf@abenteuerland.at
 * @license This code is free software. Do whatever you like to do with it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <cairo.h>
#include <jpeglib.h>

#include "write_jpg.h"

int cairo_surface_write_to_jpeg_stream(cairo_surface_t *sfc,
		FILE *stream, int quality) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	unsigned char *data = NULL;
	unsigned long len = 0;
	jpeg_mem_dest(&cinfo, &data, &len);
	cinfo.image_width = cairo_image_surface_get_width(sfc);
	cinfo.image_height = cairo_image_surface_get_height(sfc);
	if (cairo_image_surface_get_format(sfc) == CAIRO_FORMAT_ARGB32) {
		cinfo.in_color_space = JCS_EXT_BGRA;
	} else {
		cinfo.in_color_space = JCS_EXT_BGRX;
	}
	cinfo.input_components = 4;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = cairo_image_surface_get_data(sfc) + (cinfo.next_scanline
				* cairo_image_surface_get_stride(sfc));
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	size_t written = fwrite(data, 1, len, stream);
	if (written < len) {
		free(data);
		fprintf(stderr, "Failed to write jpg; only %zu of %zu bytes written\n",
			written, len);
		return -1;
	}
	free(data);
	return 0;
}
