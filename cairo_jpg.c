/**
 * @author Bernhard R. Fischer, 4096R/8E24F29D bf@abenteuerland.at
 * @license This code is free software. Do whatever you like to do with it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cairo.h>
#include <jpeglib.h>

#include "cairo_jpg.h"

cairo_status_t cairo_surface_write_to_jpeg_mem(cairo_surface_t *sfc,
		unsigned char **data, size_t *len, int quality) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
	cairo_surface_t *other = NULL;

	if (cairo_surface_get_type(sfc) != CAIRO_SURFACE_TYPE_IMAGE ||
			(cairo_image_surface_get_format(sfc) != CAIRO_FORMAT_ARGB32 &&
			cairo_image_surface_get_format(sfc) != CAIRO_FORMAT_RGB24)) {
		double x1, y1, x2, y2;
		other = sfc;
		cairo_t *ctx = cairo_create(other);
		cairo_clip_extents(ctx, &x1, &y1, &x2, &y2);
		cairo_destroy(ctx);

		sfc = cairo_surface_create_similar_image(other, CAIRO_FORMAT_RGB24,
			x2 - x1, y2 - y1);
		if (cairo_surface_status(sfc) != CAIRO_STATUS_SUCCESS) {
			return CAIRO_STATUS_INVALID_FORMAT;
		}

		ctx = cairo_create(sfc);
		cairo_set_source_surface(ctx, other, 0, 0);
		cairo_paint(ctx);
		cairo_destroy(ctx);
	}

	cairo_surface_flush(sfc);

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	jpeg_mem_dest(&cinfo, data, len);
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

	if (other != NULL)
		cairo_surface_destroy(sfc);

	return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t cj_write(void *closure, const unsigned char *data,
		unsigned int length) {
	if (write((long) closure, data, length) < length) {
		return CAIRO_STATUS_WRITE_ERROR;
	} else {
		return CAIRO_STATUS_SUCCESS;
	}
}

cairo_status_t cairo_surface_write_to_jpeg_stream(cairo_surface_t *sfc,
		cairo_write_func_t write_func, void *closure, int quality) {
	cairo_status_t e;
	unsigned char *data = NULL;
	size_t len = 0;

	e = cairo_surface_write_to_jpeg_mem(sfc, &data, &len, quality);
	if (e != CAIRO_STATUS_SUCCESS) {
		return e;
	}

	e = write_func(closure, data, len);

	free(data);
	return e;

}

cairo_status_t cairo_surface_write_to_jpeg(cairo_surface_t *sfc,
		const char *filename, int quality) {
	cairo_status_t e;
	int outfile;

	outfile = open(filename,
		O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (outfile == -1) {
		return CAIRO_STATUS_DEVICE_ERROR;
	}

	e = cairo_surface_write_to_jpeg_stream(sfc, cj_write,
		(void*) (long) outfile, quality);

	close(outfile);
	return e;
}
