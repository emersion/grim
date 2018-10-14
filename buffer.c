#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "buffer.h"

static int create_pool_file(size_t size, char **name) {
	static const char template[] = "grim-XXXXXX";
	const char *path = getenv("XDG_RUNTIME_DIR");
	if (path == NULL) {
		return -1;
	}

	int ts = (path[strlen(path) - 1] == '/');

	*name = malloc(
		strlen(template) +
		strlen(path) +
		(ts ? 0 : 1) + 1);
	sprintf(*name, "%s%s%s", path, ts ? "" : "/", template);

	int fd = mkstemp(*name);
	if (fd < 0) {
		return -1;
	}

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

struct grim_buffer *create_buffer(struct wl_shm *shm, enum wl_shm_format wl_fmt,
		int32_t width, int32_t height, int32_t stride) {
	size_t size = stride * height;

	char *name;
	int fd = create_pool_file(size, &name);
	if (fd == -1) {
		return NULL;
	}

	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		free(name);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	struct wl_buffer *wl_buffer =
		wl_shm_pool_create_buffer(pool, 0, width, height, stride, wl_fmt);
	wl_shm_pool_destroy(pool);

	close(fd);
	fd = -1;
	unlink(name);
	free(name);

	struct grim_buffer *buffer = calloc(1, sizeof(struct grim_buffer));
	buffer->wl_buffer = wl_buffer;
	buffer->data = data;
	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;
	buffer->size = size;
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
