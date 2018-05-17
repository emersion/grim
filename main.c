#define _XOPEN_SOURCE 500
#include <cairo.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "grim.h"
#include "buffer.h"

static void get_output_layout_extents(struct grim_state *state, int32_t *x,
		int32_t *y, int32_t *width, int32_t *height) {
	int32_t x1 = INT_MAX, y1 = INT_MAX;
	int32_t x2 = INT_MIN, y2 = INT_MIN;

	struct grim_output *output;
	wl_list_for_each(output, &state->outputs, link) {
		if (output->logical_geometry.x < x1) {
			x1 = output->logical_geometry.x;
		}
		if (output->logical_geometry.y < y1) {
			y1 = output->logical_geometry.y;
		}
		if (output->logical_geometry.x + output->logical_geometry.width > x2) {
			x2 = output->logical_geometry.x + output->logical_geometry.width;
		}
		if (output->logical_geometry.y + output->logical_geometry.height > y2) {
			y2 = output->logical_geometry.y + output->logical_geometry.height;
		}
	}

	*x = x1;
	*y = y1;
	*width = x2 - x1;
	*height = y2 - y1;
}

static void apply_output_transform(enum wl_output_transform transform,
		int32_t *width, int32_t *height) {
	if (transform & WL_OUTPUT_TRANSFORM_90) {
		int32_t tmp = *width;
		*width = *height;
		*height = tmp;
	}
}

static double get_output_rotation(enum wl_output_transform transform) {
	switch (transform & ~WL_OUTPUT_TRANSFORM_FLIPPED) {
	case WL_OUTPUT_TRANSFORM_90:
		return M_PI / 2;
	case WL_OUTPUT_TRANSFORM_180:
		return M_PI;
	case WL_OUTPUT_TRANSFORM_270:
		return 3 * M_PI / 2;
	}
	return 0;
}

static int get_output_flipped(enum wl_output_transform transform) {
	return transform & WL_OUTPUT_TRANSFORM_FLIPPED ? -1 : 1;
}


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

	output->logical_geometry.x = x;
	output->logical_geometry.y = y;
}

static void xdg_output_handle_logical_size(void *data,
		struct zxdg_output_v1 *xdg_output, int32_t width, int32_t height) {
	struct grim_output *output = data;

	output->logical_geometry.width = width;
	output->logical_geometry.height = height;
}

static void xdg_output_handle_done(void *data,
		struct zxdg_output_v1 *xdg_output) {
	struct grim_output *output = data;

	// Guess the output scale from the logical size
	int32_t width = output->geometry.width;
	int32_t height = output->geometry.height;
	apply_output_transform(output->transform, &width, &height);
	output->logical_scale = (double)width / output->logical_geometry.width;
}

static void xdg_output_handle_name(void *data,
		struct zxdg_output_v1 *xdg_output, const char *name) {
	struct grim_output *output = data;
	output->name = strdup(name);
}

static void xdg_output_handle_description(void *data,
		struct zxdg_output_v1 *xdg_output, const char *name) {
	// No-op
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = xdg_output_handle_logical_position,
	.logical_size = xdg_output_handle_logical_size,
	.done = xdg_output_handle_done,
	.name = xdg_output_handle_name,
	.description = xdg_output_handle_description,
};


static void output_handle_geometry(void *data, struct wl_output *wl_output,
		int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
		int32_t subpixel, const char *make, const char *model,
		int32_t transform) {
	struct grim_output *output = data;

	output->geometry.x = x;
	output->geometry.y = y;
	output->transform = transform;
}

static void output_handle_mode(void *data, struct wl_output *wl_output,
		uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	struct grim_output *output = data;

	if ((flags & WL_OUTPUT_MODE_CURRENT) != 0) {
		output->geometry.width = width;
		output->geometry.height = height;
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
		uint32_t bind_version = (version > 2) ? 2 : version;
		state->xdg_output_manager = wl_registry_bind(registry, name,
			&zxdg_output_manager_v1_interface, bind_version);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct grim_output *output = calloc(1, sizeof(struct grim_output));
		output->state = state;
		output->scale = 1;
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

static cairo_status_t write_func(void *closure, const unsigned char *data,
		unsigned int length) {
	FILE *f = closure;

	size_t written = fwrite(data, sizeof(unsigned char), length, f);
	if (written < length) {
		return CAIRO_STATUS_WRITE_ERROR;
	}

	return CAIRO_STATUS_SUCCESS;
}

static const char usage[] =
	"Usage: grim [options...] <output-file>\n"
	"\n"
	"  -h              Show help message and quit.\n"
	"  -s <factor>     Set the output image scale factor. Defaults to the\n"
	"                  greatest output scale factor.\n"
	"  -g <geometry>   Set the region to capture.\n"
	"  -o <output>     Set the output name to capture.\n";

int main(int argc, char *argv[]) {
	double scale = 1.0;
	bool use_greatest_scale = true;
	struct grim_box *geometry = NULL;
	char *geometry_output = NULL;
	int opt;
	while ((opt = getopt(argc, argv, "hs:g:o:")) != -1) {
		switch (opt) {
		case 'h':
			printf("%s", usage);
			return EXIT_SUCCESS;
		case 's':
			use_greatest_scale = false;
			scale = strtod(optarg, NULL);
			break;
		case 'g':
			geometry = calloc(1, sizeof(struct grim_box));
			if (!parse_box(geometry, optarg)) {
				fprintf(stderr, "invalid geometry\n");
				return EXIT_FAILURE;
			}
			break;
		case 'o':
			geometry_output = strdup(optarg);
			break;
		default:
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "%s", usage);
		return EXIT_FAILURE;
	}
	char *output_filename = argv[optind];

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
			output->logical_geometry.x = output->geometry.x;
			output->logical_geometry.y = output->geometry.y;
			output->logical_geometry.width =
				output->geometry.width / output->scale;
			output->logical_geometry.height =
				output->geometry.height / output->scale;
			apply_output_transform(output->transform,
				&output->logical_geometry.width,
				&output->logical_geometry.height);
			output->logical_scale = output->scale;
		}
	}

	if (state.orbital_screenshooter == NULL) {
		fprintf(stderr, "compositor doesn't support orbital_screenshooter\n");
		return EXIT_FAILURE;
	}

	struct grim_output *output;
	wl_list_for_each(output, &state.outputs, link) {
		if (use_greatest_scale && output->logical_scale > scale) {
			scale = output->logical_scale;
		}
		if (geometry_output != NULL && output->name != NULL &&
				strcmp(output->name, geometry_output) == 0) {
			geometry = calloc(1, sizeof(struct grim_box));
			memcpy(geometry, &output->logical_geometry, sizeof(struct grim_box));
		}
	}

	if (geometry_output != NULL && geometry == NULL) {
		fprintf(stderr, "unknown output '%s'", geometry_output);
		return EXIT_FAILURE;
	}

	wl_list_for_each(output, &state.outputs, link) {
		output->buffer = create_buffer(state.shm, output->geometry.width,
			output->geometry.height);
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
		width * scale, height * scale);
	cairo_t *cairo = cairo_create(surface);

	wl_list_for_each(output, &state.outputs, link) {
		struct grim_buffer *buffer = output->buffer;
		if (buffer->format != WL_SHM_FORMAT_ARGB8888) {
			fprintf(stderr, "unsupported format\n");
			return EXIT_FAILURE;
		}

		int32_t output_x = output->logical_geometry.x - x;
		int32_t output_y = output->logical_geometry.y - y;
		int32_t output_width = output->logical_geometry.width;
		int32_t output_height = output->logical_geometry.height;

		int32_t raw_output_width = output->geometry.width;
		int32_t raw_output_height = output->geometry.height;
		apply_output_transform(output->transform,
			&raw_output_width, &raw_output_height);

		int output_flipped = get_output_flipped(output->transform);

		cairo_surface_t *output_surface = cairo_image_surface_create_for_data(
			buffer->data, CAIRO_FORMAT_ARGB32, buffer->width, buffer->height,
			buffer->stride);
		cairo_pattern_t *output_pattern =
			cairo_pattern_create_for_surface(output_surface);

		// All transformations are in pattern-local coordinates
		cairo_matrix_t matrix;
		cairo_matrix_init_identity(&matrix);
		cairo_matrix_translate(&matrix,
			(double)output->geometry.width / 2,
			(double)output->geometry.height / 2);
		cairo_matrix_rotate(&matrix, get_output_rotation(output->transform));
		cairo_matrix_scale(&matrix,
			(double)raw_output_width / output_width * output_flipped,
			(double)raw_output_height / output_height);
		cairo_matrix_translate(&matrix,
			-(double)output_width / 2,
			-(double)output_height / 2);
		cairo_matrix_translate(&matrix, -output_x, -output_y);
		cairo_matrix_scale(&matrix, 1 / scale, 1 / scale);
		cairo_pattern_set_matrix(output_pattern, &matrix);

		cairo_set_source(cairo, output_pattern);
		cairo_pattern_destroy(output_pattern);

		cairo_paint(cairo);

		cairo_surface_destroy(output_surface);
	}

	cairo_surface_t *subsurface = surface;
	if (geometry != NULL) {
		subsurface = cairo_surface_create_for_rectangle(surface,
			geometry->x * scale, geometry->y * scale,
			geometry->width * scale, geometry->height * scale);
	}

	cairo_status_t status;
	if (strcmp(output_filename, "-") == 0) {
		status =
			cairo_surface_write_to_png_stream(subsurface, write_func, stdout);
	} else {
		status = cairo_surface_write_to_png(subsurface, output_filename);
	}
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to write output file\n");
		return EXIT_FAILURE;
	}

	if (subsurface != surface) {
		cairo_surface_destroy(subsurface);
	}

	cairo_destroy(cairo);
	cairo_surface_destroy(surface);

	struct grim_output *output_tmp;
	wl_list_for_each_safe(output, output_tmp, &state.outputs, link) {
		wl_list_remove(&output->link);
		free(output->name);
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
	free(geometry);
	free(geometry_output);
	return EXIT_SUCCESS;
}
