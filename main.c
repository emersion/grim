#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>

#include "grim.h"
#include "buffer.h"

static void orbital_screenshot_handle_done(void *data,
		struct orbital_screenshot *screenshot) {
	struct grim_output *output = data;
	++output->state->n_done;
}

static const struct orbital_screenshot_listener orbital_screenshot_listener = {
	.done = orbital_screenshot_handle_done,
};


static void xdg_output_handle_logical_position(void *data,
		struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y) {
	struct grim_output *output = data;

	output->logical.x = x;
	output->logical.y = y;
}

static void xdg_output_handle_logical_size(void *data,
		struct zxdg_output_v1 *xdg_output, int32_t width, int32_t height) {
	struct grim_output *output = data;

	output->logical.width = width;
	output->logical.height = height;
}

static void xdg_output_handle_done(void *data,
		struct zxdg_output_v1 *xdg_output) {
	// No-op
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = xdg_output_handle_logical_position,
	.logical_size = xdg_output_handle_logical_size,
	.done = xdg_output_handle_done,
};


static void output_handle_geometry(void *data, struct wl_output *wl_output,
		int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
		int32_t subpixel, const char *make, const char *model,
		int32_t transform) {
	struct grim_output *output = data;

	output->x = x;
	output->y = y;
	output->transform = transform;
}

static void output_handle_mode(void *data, struct wl_output *wl_output,
		uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	struct grim_output *output = data;

	if ((flags & WL_OUTPUT_MODE_CURRENT) != 0) {
		output->width = width;
		output->height = height;
	}
}

static void output_handle_done(void *data, struct wl_output *wl_output) {
	// No-op
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
		int32_t factor) {
	struct grim_output *output = data;
	output->scale = factor;
}

static const struct wl_output_listener output_listener = {
	.geometry = output_handle_geometry,
	.mode = output_handle_mode,
	.done = output_handle_done,
	.scale = output_handle_scale,
};


static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct grim_state *state = data;

	if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
		state->xdg_output_manager = wl_registry_bind(registry, name,
			&zxdg_output_manager_v1_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct grim_output *output = calloc(1, sizeof(struct grim_output));
		output->state = state;
		output->wl_output =  wl_registry_bind(registry, name,
			&wl_output_interface, 3);
		wl_output_add_listener(output->wl_output, &output_listener, output);
		wl_list_insert(&state->outputs, &output->link);
	} else if (strcmp(interface, orbital_screenshooter_interface.name) == 0) {
		state->orbital_screenshooter = wl_registry_bind(registry, name,
			&orbital_screenshooter_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static void get_output_layout_extents(struct grim_state *state, int32_t *x,
		int32_t *y, int32_t *width, int32_t *height) {
	int32_t x1 = INT_MAX, y1 = INT_MAX;
	int32_t x2 = INT_MIN, y2 = INT_MIN;

	struct grim_output *output;
	wl_list_for_each(output, &state->outputs, link) {
		if (output->logical.x < x1) {
			x1 = output->logical.x;
		}
		if (output->logical.y < y1) {
			y1 = output->logical.y;
		}
		if (output->logical.x + output->logical.width > x2) {
			x2 = output->logical.x + output->logical.width;
		}
		if (output->logical.y + output->logical.height > y2) {
			y2 = output->logical.y + output->logical.height;
		}
	}

	*x = x1;
	*y = y1;
	*width = x2 - x1;
	*height = y2 - y1;
}

int main() {
	struct grim_state state = {0};
	wl_list_init(&state.outputs);

	state.display = wl_display_connect(NULL);
	if (state.display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return EXIT_FAILURE;
	}

	state.registry = wl_display_get_registry(state.display);
	wl_registry_add_listener(state.registry, &registry_listener, &state);
	wl_display_dispatch(state.display);
	wl_display_roundtrip(state.display);

	if (state.shm == NULL) {
		fprintf(stderr, "compositor doesn't support wl_shm\n");
		return EXIT_FAILURE;
	}
	if (wl_list_empty(&state.outputs)) {
		fprintf(stderr, "no wl_output\n");
		return EXIT_FAILURE;
	}

	if (state.xdg_output_manager != NULL) {
		struct grim_output *output;
		wl_list_for_each(output, &state.outputs, link) {
			output->xdg_output = zxdg_output_manager_v1_get_xdg_output(
				state.xdg_output_manager, output->wl_output);
			zxdg_output_v1_add_listener(output->xdg_output,
				&xdg_output_listener, output);
		}

		wl_display_dispatch(state.display);
		wl_display_roundtrip(state.display);
	} else {
		fprintf(stderr, "warning: zxdg_output_manager_v1 isn't available, "
			"guessing the output layout\n");

		struct grim_output *output;
		wl_list_for_each(output, &state.outputs, link) {
			// TODO: guess with transform and scale
			output->logical.x = output->x;
			output->logical.y = output->y;
			output->logical.width = output->width;
			output->logical.height = output->height;
		}
	}

	if (state.orbital_screenshooter == NULL) {
		fprintf(stderr, "compositor doesn't support orbital_screenshooter\n");
		return EXIT_FAILURE;
	}

	struct grim_output *output;
	wl_list_for_each(output, &state.outputs, link) {
		output->buffer = create_buffer(state.shm, output->width, output->height);
		if (output->buffer == NULL) {
			fprintf(stderr, "failed to create buffer\n");
			return EXIT_FAILURE;
		}
		output->orbital_screenshot = orbital_screenshooter_shoot(
			state.orbital_screenshooter, output->wl_output,
			output->buffer->wl_buffer);
		orbital_screenshot_add_listener(output->orbital_screenshot,
			&orbital_screenshot_listener, output);
	}

	bool done = false;
	size_t n = wl_list_length(&state.outputs);
	while (!done && wl_display_dispatch(state.display) != -1) {
		done = (n == state.n_done);
	}
	if (!done) {
		fprintf(stderr, "failed to screenshoot all outputs\n");
		return EXIT_FAILURE;
	}

	int32_t x, y, width, height;
	get_output_layout_extents(&state, &x, &y, &width, &height);

	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
		width, height);
	cairo_t *cairo = cairo_create(surface);

	wl_list_for_each(output, &state.outputs, link) {
		struct grim_buffer *buffer = output->buffer;
		if (buffer->format != WL_SHM_FORMAT_ARGB8888) {
			fprintf(stderr, "unsupported format\n");
			return EXIT_FAILURE;
		}

		cairo_surface_t *output_surface = cairo_image_surface_create_for_data(
			buffer->data, CAIRO_FORMAT_ARGB32, buffer->width, buffer->height,
			buffer->stride);
		cairo_pattern_t *output_pattern =
			cairo_pattern_create_for_surface(output_surface);

		cairo_set_source(cairo, output_pattern);
		cairo_pattern_destroy(output_pattern);

		cairo_rectangle(cairo, output->logical.x - x, output->logical.y - y,
			output->logical.width, output->logical.height);
		cairo_fill(cairo);

		cairo_surface_destroy(output_surface);
	}

	cairo_surface_write_to_png(surface, "wayland-screenshot.png");

	cairo_destroy(cairo);
	cairo_surface_destroy(surface);

	struct grim_output *output_tmp;
	wl_list_for_each_safe(output, output_tmp, &state.outputs, link) {
		wl_list_remove(&output->link);
		if (output->orbital_screenshot != NULL) {
			orbital_screenshot_destroy(output->orbital_screenshot);
		}
		destroy_buffer(output->buffer);
		if (output->xdg_output != NULL) {
			zxdg_output_v1_destroy(output->xdg_output);
		}
		wl_output_release(output->wl_output);
		free(output);
	}
	orbital_screenshooter_destroy(state.orbital_screenshooter);
	if (state.xdg_output_manager != NULL) {
		zxdg_output_manager_v1_destroy(state.xdg_output_manager);
	}
	wl_shm_destroy(state.shm);
	wl_registry_destroy(state.registry);
	wl_display_disconnect(state.display);
	return EXIT_SUCCESS;
}
