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

#include "write_ppm.h"

int cairo_surface_write_to_ppm_stream(cairo_surface_t *sfc,
		FILE *stream) {
	// 256 bytes ought to be enough for everyone
	char header[256];

	int width = cairo_image_surface_get_width(sfc);
	int height = cairo_image_surface_get_height(sfc);

	int header_len = snprintf(header, sizeof(header), "P6\n%d %d\n255\n", width, height);
	assert(header_len <= (int)sizeof(header));

	size_t len = header_len + width * height * 3;
	unsigned char *data = malloc(len);
	unsigned char *buffer = data;

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

	size_t written = fwrite(data, 1, len, stream);
	if (written < len) {
		free(data);
		fprintf(stderr, "Failed to write ppm; only %zu of %zu bytes written\n",
			written, len);
		return -1;
	}
	free(data);
	return 0;
}
