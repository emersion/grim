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

#include "write_ppm.h"

int write_to_ppm_stream(pixman_image_t *image, FILE *stream) {
	// 256 bytes ought to be enough for everyone
	char header[256];

	int width = pixman_image_get_width(image);
	int height = pixman_image_get_height(image);

	int header_len = snprintf(header, sizeof(header), "P6\n%d %d\n255\n", width, height);
	assert(header_len <= (int)sizeof(header));

	size_t len = header_len + width * height * 3;
	unsigned char *data = malloc(len);
	unsigned char *buffer = data;

	// We _do_not_ include the null byte
	memcpy(buffer, header, header_len);
	buffer += header_len;

	pixman_format_code_t format = pixman_image_get_format(image);
	assert(format == PIXMAN_a8r8g8b8 || format == PIXMAN_x8r8g8b8);

	// Both formats are native-endian 32-bit ints
	uint32_t *pixels = pixman_image_get_data(image);
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
