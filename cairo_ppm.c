#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cairo.h>

#include "cairo_ppm.h"

cairo_status_t cairo_surface_write_to_ppm_mem(cairo_surface_t *sfc,
		unsigned char **data, unsigned long *len) {
	// 256 bytes ought to be enough for everyone
	char header[256];

	int width = cairo_image_surface_get_width(sfc);
	int height = cairo_image_surface_get_height(sfc);

	int header_len = snprintf(header, sizeof(header), "P6\n%d %d\n255\n", width, height);
	assert(header_len <= (int)sizeof(header));

	*len = header_len + width * height * 3;
	unsigned char *buffer = malloc(*len);
	*data = buffer;

	// We _do_not_ include the null byte
	memcpy(buffer, header, header_len);
	buffer += header_len;

	cairo_format_t cformat = cairo_image_surface_get_format(sfc);
	assert(cformat == CAIRO_FORMAT_RGB24 || cformat == CAIRO_FORMAT_ARGB32);

	// Both formats are native-endian 32-bit ints
	uint32_t *pixels = (uint32_t *)cairo_image_surface_get_data(sfc);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint32_t p = *pixels++;
			// RGB order
			*buffer++ = (p >> 16) & 0xff;
			*buffer++ = (p >>  8) & 0xff;
			*buffer++ = (p >>  0) & 0xff;
		}
	}

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

cairo_status_t cairo_surface_write_to_ppm_stream(cairo_surface_t *sfc,
		cairo_write_func_t write_func, void *closure) {
	cairo_status_t e;
	unsigned char *data = NULL;
	unsigned long len = 0;

	e = cairo_surface_write_to_ppm_mem(sfc, &data, &len);
	if (e == CAIRO_STATUS_SUCCESS) {
		assert(sizeof(unsigned long) <= sizeof(size_t)
			|| !(len >> (sizeof(size_t) * CHAR_BIT)));
		e = write_func(closure, data, len);
		free(data);
	}

	return e;
}

cairo_status_t cairo_surface_write_to_ppm(cairo_surface_t *sfc,
		const char *filename) {
	cairo_status_t e;
	int outfile;

	outfile = open(filename,
		O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (outfile == -1) {
		return CAIRO_STATUS_DEVICE_ERROR;
	}

	e = cairo_surface_write_to_ppm_stream(sfc, cj_write,
		(void*) (long) outfile);

	close(outfile);
	return e;
}
