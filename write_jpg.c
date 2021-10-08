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
#include <jpeglib.h>

#include "write_jpg.h"

int write_to_jpeg_stream(pixman_image_t *image, FILE *stream, int quality) {
	pixman_format_code_t format = pixman_image_get_format(image);
	assert(format == PIXMAN_a8r8g8b8 || format == PIXMAN_x8r8g8b8);

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	unsigned char *data = NULL;
	unsigned long len = 0;
	jpeg_mem_dest(&cinfo, &data, &len);
	cinfo.image_width = pixman_image_get_width(image);
	cinfo.image_height = pixman_image_get_height(image);
	if (format == PIXMAN_a8r8g8b8) {
		cinfo.in_color_space = JCS_EXT_BGRA;
	} else {
		cinfo.in_color_space = JCS_EXT_BGRX;
	}
	cinfo.input_components = 4;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = (unsigned char *)pixman_image_get_data(image)
			+ (cinfo.next_scanline * pixman_image_get_stride(image));
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
