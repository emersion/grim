#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "buffer.h"

static void randname(char *buf) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int anonymous_shm_open(void) {
	char name[] = "/grim-XXXXXX";
	int retries = 100;

	do {
		randname(name + strlen(name) - 6);

		--retries;
		// shm_open guarantees that O_CLOEXEC is set
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);

	return -1;
}

static int create_shm_file(off_t size) {
	int fd = anonymous_shm_open();
	if (fd < 0) {
		return fd;
	}

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

struct grim_buffer *create_buffer(struct wl_shm *shm, enum wl_shm_format format,
		int32_t width, int32_t height, int32_t stride) {
	size_t size = stride * height;

	int fd = create_shm_file(size);
	if (fd == -1) {
		return NULL;
	}

	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	struct wl_buffer *wl_buffer =
		wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
	wl_shm_pool_destroy(pool);

	close(fd);

	struct grim_buffer *buffer = calloc(1, sizeof(struct grim_buffer));
	buffer->wl_buffer = wl_buffer;
	buffer->data = data;
	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;
	buffer->size = size;
	buffer->format = format;
	return buffer;
}

void destroy_buffer(struct grim_buffer *buffer) {
	if (buffer == NULL) {
		return;
	}
	munmap(buffer->data, buffer->size);
	wl_buffer_destroy(buffer->wl_buffer);
	free(buffer);
}
