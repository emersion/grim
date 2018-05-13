#ifndef _GRIM_H
#define _GRIM_H

#include <wayland-client.h>

#include "orbital_screenshooter-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

struct grim_state {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_shm *shm;
	struct zxdg_output_manager_v1 *xdg_output_manager;
	struct orbital_screenshooter *orbital_screenshooter;
	struct wl_list outputs;

	size_t n_done;
};

struct grim_buffer;

struct grim_output {
	struct grim_state *state;
	struct wl_output *wl_output;
	struct zxdg_output_v1 *xdg_output;
	struct wl_list link;

	int32_t x, y;
	int32_t width, height;
	enum wl_output_transform transform;
	int32_t scale;

	struct {
		int32_t x, y;
		int32_t width, height;

		double scale; // guessed from the logical size
	} logical;

	struct grim_buffer *buffer;
	struct orbital_screenshot *orbital_screenshot;
};

#endif
