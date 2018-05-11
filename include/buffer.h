#ifndef _BUFFER_H
#define _BUFFER_H

#include <wayland-client.h>

struct grim_buffer {
	struct wl_buffer *wl_buffer;
	void *data;
	int32_t width, height, stride;
	size_t size;
	enum wl_shm_format format;
};

struct grim_buffer *create_buffer(struct wl_shm *shm, int32_t width,
	int32_t height);
void destroy_buffer(struct grim_buffer *buffer);

#endif
