#include <stdio.h>

#include "buffer.h"
#include "output-layout.h"
#include "render.h"

cairo_surface_t *render(struct grim_state *state, struct grim_box *geometry,
		double scale) {
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
		geometry->width * scale, geometry->height * scale);
	cairo_t *cairo = cairo_create(surface);

	// Clear
	cairo_save(cairo);
	cairo_set_source_rgba(cairo, 0, 0, 0, 0);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);
	cairo_restore(cairo);

	struct grim_output *output;
	wl_list_for_each(output, &state->outputs, link) {
		struct grim_buffer *buffer = output->buffer;
		if (buffer == NULL) {
			continue;
		}
		if (buffer->format != WL_SHM_FORMAT_ARGB8888) {
			fprintf(stderr, "unsupported format\n");
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

		cairo_pattern_set_filter(output_pattern, CAIRO_FILTER_BEST);

		cairo_set_source(cairo, output_pattern);
		cairo_pattern_destroy(output_pattern);

		cairo_paint(cairo);

		cairo_surface_destroy(output_surface);
	}

	cairo_destroy(cairo);
	return surface;
}
