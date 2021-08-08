#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "output-layout.h"
#include "render.h"

static bool format_has_alpha(uint32_t format) {
	switch (format) {
	case WL_SHM_FORMAT_ABGR8888:
	case WL_SHM_FORMAT_ARGB8888:
	case WL_SHM_FORMAT_ARGB2101010:
	case WL_SHM_FORMAT_ABGR2101010:
		return true;
	case WL_SHM_FORMAT_XRGB8888:
	case WL_SHM_FORMAT_XBGR8888:
	case WL_SHM_FORMAT_XRGB2101010:
	case WL_SHM_FORMAT_XBGR2101010:
		return false;
	default:
		assert(false);
	}
}

static bool format_is_wide(uint32_t format) {
	switch (format) {
	case WL_SHM_FORMAT_XRGB2101010:
	case WL_SHM_FORMAT_XBGR2101010:
	case WL_SHM_FORMAT_ARGB2101010:
	case WL_SHM_FORMAT_ABGR2101010:
		return true;
	default:
		return false;
	}
}

static cairo_surface_t *convert_buffer(struct grim_buffer *buffer) {
	if (buffer->format == WL_SHM_FORMAT_ABGR8888 ||
		buffer->format == WL_SHM_FORMAT_XBGR8888) {
		// ABGR -> ARGB
		uint8_t *data = buffer->data;
		for (int i = 0; i < buffer->height; ++i) {
			for (int j = 0; j < buffer->width; ++j) {
				uint32_t *px = (uint32_t *)(data + i * buffer->stride + j * 4);
				uint8_t a = (*px >> 24) & 0xFF;
				uint8_t b = (*px >> 16) & 0xFF;
				uint8_t g = (*px >> 8) & 0xFF;
				uint8_t r = *px & 0xFF;
				*px = (a << 24) | (r << 16) | (g << 8) | b;
			}
		}
		if (buffer->format == WL_SHM_FORMAT_ABGR8888) {
			buffer->format = WL_SHM_FORMAT_ARGB8888;
		} else {
			buffer->format = WL_SHM_FORMAT_XRGB8888;
		}
	}

	// Formats are little-endian
	switch (buffer->format) {
	case WL_SHM_FORMAT_ARGB8888:
	case WL_SHM_FORMAT_XRGB8888:;
		// Natively supported by Cairo
		cairo_format_t format = format_has_alpha(buffer->format) ?
			CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
		return cairo_image_surface_create_for_data(
			buffer->data, format, buffer->width, buffer->height,
			buffer->stride);
	case WL_SHM_FORMAT_ARGB2101010:
	case WL_SHM_FORMAT_XRGB2101010:
	case WL_SHM_FORMAT_ABGR2101010:
	case WL_SHM_FORMAT_XBGR2101010:;
		bool has_alpha = format_has_alpha(buffer->format);
		cairo_format_t float_format =
			has_alpha ? CAIRO_FORMAT_RGBA128F : CAIRO_FORMAT_RGB96F;
		int bytes_per_pixel = has_alpha ? 16 : 12;
		cairo_surface_t *surface = cairo_image_surface_create(
			float_format, buffer->width, buffer->height);

		int stride = cairo_image_surface_get_stride(surface);
		uint8_t *data = cairo_image_surface_get_data(surface);
		uint8_t *buffer_data = buffer->data;
		for (int y = 0; y < buffer->height; y++) {
			for (int x = 0; x < buffer->width; x++) {
				uint32_t *px = (uint32_t *)(buffer_data + y * buffer->stride + x * 4);
				float *rgba = (float *)(data + y * stride + x * bytes_per_pixel);

				switch (buffer->format) {
				case WL_SHM_FORMAT_ARGB2101010:
					rgba[3] = ((*px >> 30) & 0x3) / 3.f;
					rgba[0] = ((*px >> 20) & 0x3ff) / 1023.f;
					rgba[1] = ((*px >> 10) & 0x3ff) / 1023.f;
					rgba[2] = ((*px >> 0) & 0x3ff) / 1023.f;
					break;
				case WL_SHM_FORMAT_XRGB2101010:
					rgba[0] = ((*px >> 20) & 0x3ff) / 1023.f;
					rgba[1] = ((*px >> 10) & 0x3ff) / 1023.f;
					rgba[2] = ((*px >> 0) & 0x3ff) / 1023.f;
					break;
				case WL_SHM_FORMAT_ABGR2101010:
					rgba[3] = ((*px >> 30) & 0x3) / 3.f;
					rgba[2] = ((*px >> 20) & 0x3ff) / 1023.f;
					rgba[1] = ((*px >> 10) & 0x3ff) / 1023.f;
					rgba[0] = ((*px >> 0) & 0x3ff) / 1023.f;
					break;
				case WL_SHM_FORMAT_XBGR2101010:
					rgba[2] = ((*px >> 20) & 0x3ff) / 1023.f;
					rgba[1] = ((*px >> 10) & 0x3ff) / 1023.f;
					rgba[0] = ((*px >> 0) & 0x3ff) / 1023.f;
					break;
				default:
					assert(false);
				}
			}
		}
		cairo_surface_mark_dirty(surface);
		return surface;
	default:
		fprintf(stderr, "unsupported format %d = 0x%08x\n",
			buffer->format, buffer->format);
		return NULL;
	}
}

cairo_surface_t *render(struct grim_state *state, struct grim_box *geometry,
		double scale) {
	bool wide = false;
	struct grim_output *output;
	wl_list_for_each(output, &state->outputs, link) {
		struct grim_buffer *buffer = output->buffer;
		if (buffer) {
			wide = wide || format_is_wide(buffer->format);
		}
	}

	cairo_surface_t *surface = cairo_image_surface_create(
		wide ? CAIRO_FORMAT_RGBA128F : CAIRO_FORMAT_ARGB32,
		geometry->width * scale, geometry->height * scale);
	cairo_t *cairo = cairo_create(surface);

	// Clear
	cairo_save(cairo);
	cairo_set_source_rgba(cairo, 0, 0, 0, 0);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);
	cairo_restore(cairo);

	wl_list_for_each(output, &state->outputs, link) {
		struct grim_buffer *buffer = output->buffer;
		if (buffer == NULL) {
			continue;
		}

		cairo_surface_t *output_surface = convert_buffer(buffer);
		if (!output_surface) {
			return NULL;
		}

		int32_t output_x = output->logical_geometry.x - geometry->x;
		int32_t output_y = output->logical_geometry.y - geometry->y;
		int32_t output_width = output->logical_geometry.width;
		int32_t output_height = output->logical_geometry.height;

		int32_t raw_output_width = output->geometry.width;
		int32_t raw_output_height = output->geometry.height;
		apply_output_transform(output->transform,
			&raw_output_width, &raw_output_height);

		int output_flipped_x = get_output_flipped(output->transform);
		int output_flipped_y = output->screencopy_frame_flags &
			ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT ? -1 : 1;

		cairo_pattern_t *output_pattern =
			cairo_pattern_create_for_surface(output_surface);

		// All transformations are in pattern-local coordinates
		cairo_matrix_t matrix;
		cairo_matrix_init_identity(&matrix);
		cairo_matrix_translate(&matrix,
			(double)output->geometry.width / 2,
			(double)output->geometry.height / 2);
		cairo_matrix_scale(&matrix,
			(double)raw_output_width / output_width,
			(double)raw_output_height / output_height * output_flipped_y);
		cairo_matrix_rotate(&matrix, -get_output_rotation(output->transform));
		cairo_matrix_scale(&matrix, output_flipped_x, 1);
		cairo_matrix_translate(&matrix,
			-(double)output_width / 2,
			-(double)output_height / 2);
		cairo_matrix_translate(&matrix, -output_x, -output_y);
		cairo_matrix_scale(&matrix, 1 / scale, 1 / scale);
		cairo_pattern_set_matrix(output_pattern, &matrix);

		cairo_pattern_set_filter(output_pattern, CAIRO_FILTER_BEST);

		cairo_set_source(cairo, output_pattern);
		cairo_pattern_destroy(output_pattern);

		cairo_paint(cairo);

		cairo_surface_destroy(output_surface);
	}

	cairo_destroy(cairo);
	return surface;
}
