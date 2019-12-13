#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <cairo.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "buffer.h"
#include "grim.h"
#include "output-layout.h"
#include "render.h"
#include "cairo_ppm.h"
#ifdef HAVE_JPEG
#include "cairo_jpg.h"
#endif

static void screencopy_frame_handle_buffer(void *data,
		struct zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width,
		uint32_t height, uint32_t stride) {
	struct grim_output *output = data;

	output->buffer =
		create_buffer(output->state->shm, format, width, height, stride);
	if (output->buffer == NULL) {
		fprintf(stderr, "failed to create buffer\n");
		exit(EXIT_FAILURE);
	}

	zwlr_screencopy_frame_v1_copy(frame, output->buffer->wl_buffer);
}

static void screencopy_frame_handle_flags(void *data,
		struct zwlr_screencopy_frame_v1 *frame, uint32_t flags) {
	struct grim_output *output = data;
	output->screencopy_frame_flags = flags;
}

static void screencopy_frame_handle_ready(void *data,
		struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi,
		uint32_t tv_sec_lo, uint32_t tv_nsec) {
	struct grim_output *output = data;
	++output->state->n_done;
}

static void screencopy_frame_handle_failed(void *data,
		struct zwlr_screencopy_frame_v1 *frame) {
	struct grim_output *output = data;
	fprintf(stderr, "failed to copy output %s\n", output->name);
	exit(EXIT_FAILURE);
}

static const struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener = {
	.buffer = screencopy_frame_handle_buffer,
	.flags = screencopy_frame_handle_flags,
	.ready = screencopy_frame_handle_ready,
	.failed = screencopy_frame_handle_failed,
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
	} else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
		state->screencopy_manager = wl_registry_bind(registry, name,
			&zwlr_screencopy_manager_v1_interface, 1);
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

static cairo_status_t write_func(void *data, const unsigned char *buf,
		unsigned int len) {
	FILE *f = data;

	size_t written = fwrite(buf, sizeof(unsigned char), len, f);
	if (written < len) {
		return CAIRO_STATUS_WRITE_ERROR;
	}

	return CAIRO_STATUS_SUCCESS;
}

bool default_filename(char *filename, size_t n, int filetype) {
	time_t time_epoch = time(NULL);
	struct tm *time = localtime(&time_epoch);
	if (time == NULL) {
		perror("localtime");
		return false;
	}

	char *format_str;
	const char *ext = NULL;
	switch (filetype) {
	case GRIM_FILETYPE_PNG:
		ext = "png";
		break;
	case GRIM_FILETYPE_PPM:
		ext = "ppm";
		break;
	case GRIM_FILETYPE_JPEG:
#if HAVE_JPEG
		ext = "jpeg";
		break;
#else
		assert(false);
#endif
	}
	assert(ext != NULL);
	char tmpstr[32];
	sprintf(tmpstr, "%%Y%%m%%d_%%Hh%%Mm%%Ss_grim.%s", ext);
	format_str = tmpstr;
	if (strftime(filename, n, format_str, time) == 0) {
		fprintf(stderr, "failed to format datetime with strftime(3)\n");
		return false;
	}
	return true;
}

static bool path_exists(const char *path) {
	return path && access(path, R_OK) != -1;
}

char *get_output_dir(void) {
	static const char *output_dirs[] = {
		"GRIM_DEFAULT_DIR",
		"XDG_PICTURES_DIR",
	};

	for (size_t i = 0; i < sizeof(output_dirs) / sizeof(char *); ++i) {
		char *path = getenv(output_dirs[i]);
		if (path_exists(path)) {
			return path;
		}
	}

	return ".";
}

void filepath(char *output_path, const char *filename) {
	char *output_dir = get_output_dir();
	sprintf(output_path, "%s/%s", output_dir, filename);
}

static const char usage[] =
	"Usage: grim [options...] [output-file]\n"
	"\n"
	"  -h              Show help message and quit.\n"
	"  -s <factor>     Set the output image scale factor. Defaults to the\n"
	"                  greatest output scale factor.\n"
	"  -g <geometry>   Set the region to capture.\n"
	"  -t png|ppm|jpeg Set the output filetype. Defaults to png.\n"
	"  -q <quality>    Set the JPEG filetype quality 0-100. Defaults to 80.\n"
	"  -o <output>     Set the output name to capture.\n"
	"  -c              Include cursors in the screenshot.\n";

int main(int argc, char *argv[]) {
	double scale = 1.0;
	bool use_greatest_scale = true;
	struct grim_box *geometry = NULL;
	char *geometry_output = NULL;
	enum grim_filetype output_filetype = GRIM_FILETYPE_PNG;
	int jpeg_quality = 80;
	bool with_cursor = false;
	int opt;
	while ((opt = getopt(argc, argv, "hs:g:t:q:o:c")) != -1) {
		switch (opt) {
		case 'h':
			printf("%s", usage);
			return EXIT_SUCCESS;
		case 's':
			use_greatest_scale = false;
			scale = strtod(optarg, NULL);
			break;
		case 'g':;
			char *geometry_str = NULL;
			if (strcmp(optarg, "-") == 0) {
				size_t n = 0;
				ssize_t nread = getline(&geometry_str, &n, stdin);
				if (nread < 0) {
					free(geometry_str);
					fprintf(stderr, "failed to read a line from stdin\n");
					return EXIT_FAILURE;
				}

				if (nread > 0 && geometry_str[nread - 1] == '\n') {
					geometry_str[nread - 1] = '\0';
				}
			} else {
				geometry_str = strdup(optarg);
			}

			free(geometry);
			geometry = calloc(1, sizeof(struct grim_box));
			if (!parse_box(geometry, geometry_str)) {
				fprintf(stderr, "invalid geometry\n");
				return EXIT_FAILURE;
			}

			free(geometry_str);
			break;
		case 't':
			if (strcmp(optarg, "png") == 0) {
				output_filetype = GRIM_FILETYPE_PNG;
			} else if (strcmp(optarg, "ppm") == 0) {
				output_filetype = GRIM_FILETYPE_PPM;
			} else if (strcmp(optarg, "jpeg") == 0) {
#ifdef HAVE_JPEG
				output_filetype = GRIM_FILETYPE_JPEG;
#else
				fprintf(stderr, "jpeg support disabled\n");
				return EXIT_FAILURE;
#endif
			} else {
				fprintf(stderr, "invalid filetype\n");
				return EXIT_FAILURE;
			}
			break;
		case 'q':
			if (output_filetype != GRIM_FILETYPE_JPEG) {
				fprintf(stderr, "quality is used only for jpeg files\n");
				return EXIT_FAILURE;
			} else {
				char *endptr = NULL;
				errno = 0;
				jpeg_quality = strtol(optarg, &endptr, 10);
				if (*endptr != '\0' || errno) {
					fprintf(stderr, "quality must be a integer\n");
					return EXIT_FAILURE;
				}
				if (jpeg_quality < 0 || jpeg_quality > 100) {
					fprintf(stderr, "quality valid values are between 0-100\n");
					return EXIT_FAILURE;
				}
			}
			break;
		case 'o':
			free(geometry_output);
			geometry_output = strdup(optarg);
			break;
		case 'c':
			with_cursor = true;
			break;
		default:
			return EXIT_FAILURE;
		}
	}

	char output_filepath[PATH_MAX];
	char *output_filename;
	char tmp[64];
	if (optind >= argc) {
		if (default_filename(tmp, sizeof(tmp), output_filetype) != true) {
			fprintf(stderr, "failed to generate default filename\n");
			return EXIT_FAILURE;
		}
		output_filename = tmp;
		filepath(output_filepath, output_filename);
	} else {
		output_filename = argv[optind];
		strcpy(output_filepath, output_filename);
	}

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
			guess_output_logical_geometry(output);
		}
	}

	if (state.screencopy_manager == NULL) {
		fprintf(stderr, "compositor doesn't support wlr-screencopy-unstable-v1\n");
		return EXIT_FAILURE;
	}

	if (geometry_output != NULL) {
		struct grim_output *output;
		wl_list_for_each(output, &state.outputs, link) {
			if (output->name != NULL &&
					strcmp(output->name, geometry_output) == 0) {
				geometry = calloc(1, sizeof(struct grim_box));
				memcpy(geometry, &output->logical_geometry,
					sizeof(struct grim_box));
			}
		}

		if (geometry == NULL) {
			fprintf(stderr, "unknown output '%s'", geometry_output);
			return EXIT_FAILURE;
		}
	}

	size_t n_pending = 0;
	struct grim_output *output;
	wl_list_for_each(output, &state.outputs, link) {
		if (geometry != NULL &&
				!intersect_box(geometry, &output->logical_geometry)) {
			continue;
		}
		if (use_greatest_scale && output->logical_scale > scale) {
			scale = output->logical_scale;
		}

		output->screencopy_frame = zwlr_screencopy_manager_v1_capture_output(
			state.screencopy_manager, with_cursor, output->wl_output);
		zwlr_screencopy_frame_v1_add_listener(output->screencopy_frame,
			&screencopy_frame_listener, output);

		++n_pending;
	}

	if (n_pending == 0) {
		fprintf(stderr, "screenshot region is empty\n");
		return EXIT_FAILURE;
	}

	bool done = false;
	while (!done && wl_display_dispatch(state.display) != -1) {
		done = (state.n_done == n_pending);
	}
	if (!done) {
		fprintf(stderr, "failed to screenshoot all outputs\n");
		return EXIT_FAILURE;
	}

	if (geometry == NULL) {
		geometry = calloc(1, sizeof(struct grim_box));
		get_output_layout_extents(&state, geometry);
	}

	cairo_surface_t *surface = render(&state, geometry, scale);
	if (surface == NULL) {
		return EXIT_FAILURE;
	}

	cairo_status_t status;
	if (strcmp(output_filename, "-") == 0) {
		switch (output_filetype) {
		case GRIM_FILETYPE_PPM:
			status = cairo_surface_write_to_ppm_stream(surface, write_func, stdout);
			break;
		case GRIM_FILETYPE_PNG:
			status = cairo_surface_write_to_png_stream(surface, write_func, stdout);
			break;
		case GRIM_FILETYPE_JPEG:
#if HAVE_JPEG
			status = cairo_surface_write_to_jpeg_stream(surface, write_func,
				stdout, jpeg_quality);
			break;
#else
			assert(false);
#endif
		}
	} else {
		switch (output_filetype) {
		case GRIM_FILETYPE_PPM:
			status = cairo_surface_write_to_ppm(surface, output_filepath);
			break;
		case GRIM_FILETYPE_PNG:
			status = cairo_surface_write_to_png(surface, output_filepath);
			break;
		case GRIM_FILETYPE_JPEG:
#if HAVE_JPEG
			status = cairo_surface_write_to_jpeg(
				surface, output_filepath, jpeg_quality);
			break;
#else
			assert(false);
#endif
		}
	}
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to write output file\n");
		return EXIT_FAILURE;
	}

	cairo_surface_destroy(surface);

	struct grim_output *output_tmp;
	wl_list_for_each_safe(output, output_tmp, &state.outputs, link) {
		wl_list_remove(&output->link);
		free(output->name);
		if (output->screencopy_frame != NULL) {
			zwlr_screencopy_frame_v1_destroy(output->screencopy_frame);
		}
		destroy_buffer(output->buffer);
		if (output->xdg_output != NULL) {
			zxdg_output_v1_destroy(output->xdg_output);
		}
		wl_output_release(output->wl_output);
		free(output);
	}
	zwlr_screencopy_manager_v1_destroy(state.screencopy_manager);
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
