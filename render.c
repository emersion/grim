#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pixman.h>

#include "buffer.h"
#include "output-layout.h"
#include "render.h"

static pixman_format_code_t get_pixman_format(enum wl_shm_format wl_fmt) {
	switch (wl_fmt) {
	case WL_SHM_FORMAT_ARGB8888:
		return PIXMAN_a8r8g8b8;
	case WL_SHM_FORMAT_XRGB8888:
		return PIXMAN_x8r8g8b8;
	case WL_SHM_FORMAT_ABGR8888:
		return PIXMAN_a8b8g8r8;
	case WL_SHM_FORMAT_XBGR8888:
		return PIXMAN_x8b8g8r8;
	case WL_SHM_FORMAT_BGRA8888:
		return PIXMAN_b8g8r8a8;
	case WL_SHM_FORMAT_BGRX8888:
		return PIXMAN_b8g8r8x8;
	case WL_SHM_FORMAT_RGBA8888:
		return PIXMAN_r8g8b8a8;
	case WL_SHM_FORMAT_RGBX8888:
		return PIXMAN_r8g8b8x8;
	case WL_SHM_FORMAT_ARGB2101010:
		return PIXMAN_a2r10g10b10;
	case WL_SHM_FORMAT_ABGR2101010:
		return PIXMAN_a2b10g10r10;
	case WL_SHM_FORMAT_XRGB2101010:
		return PIXMAN_x2r10g10b10;
	case WL_SHM_FORMAT_XBGR2101010:
		return PIXMAN_x2b10g10r10;
	default:
		return 0;
	}
}

static void compute_composite_region(const struct pixman_f_transform *out2com,
		int output_width, int output_height, struct grim_box *dest,
		bool *grid_aligned) {
	struct pixman_transform o2c_fixedpt;
	pixman_transform_from_pixman_f_transform(&o2c_fixedpt, out2com);

	pixman_fixed_t w = pixman_int_to_fixed(output_width);
	pixman_fixed_t h = pixman_int_to_fixed(output_height);
	struct pixman_vector corners[4] = {
		{{0, 0, pixman_fixed_1}},
		{{w, 0, pixman_fixed_1}},
		{{0, h, pixman_fixed_1}},
		{{w, h, pixman_fixed_1}},
	};

	pixman_fixed_t x_min = INT32_MAX, x_max = INT32_MIN,
		y_min = INT32_MAX, y_max = INT32_MIN;
	for (int i = 0; i < 4; i++) {
		pixman_transform_point(&o2c_fixedpt, &corners[i]);
		x_min = corners[i].vector[0] < x_min ? corners[i].vector[0] : x_min;
		x_max = corners[i].vector[0] > x_max ? corners[i].vector[0] : x_max;
		y_min = corners[i].vector[1] < y_min ? corners[i].vector[1] : y_min;
		y_max = corners[i].vector[1] > y_max ? corners[i].vector[1] : y_max;
	}

	*grid_aligned = pixman_fixed_frac(x_min) == 0 &&
		pixman_fixed_frac(x_max) == 0 &&
		pixman_fixed_frac(y_min) == 0 &&
		pixman_fixed_frac(y_max) == 0;

	int32_t x1 = pixman_fixed_to_int(pixman_fixed_floor(x_min));
	int32_t x2 = pixman_fixed_to_int(pixman_fixed_ceil(x_max));
	int32_t y1 = pixman_fixed_to_int(pixman_fixed_floor(y_min));
	int32_t y2 = pixman_fixed_to_int(pixman_fixed_ceil(y_max));
	*dest = (struct grim_box) {
		.x = x1,
		.y = y1,
		.width = x2 - x1,
		.height = y2 - y1
	};
}

cairo_surface_t *render(struct grim_state *state, struct grim_box *geometry,
		double scale) {
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
		geometry->width * scale, geometry->height * scale);
	pixman_image_t *common_image = pixman_image_create_bits(PIXMAN_a8r8g8b8,
		cairo_image_surface_get_width(surface),
		cairo_image_surface_get_height(surface),
		(uint32_t *)cairo_image_surface_get_data(surface),
		cairo_image_surface_get_stride(surface));

	struct grim_output *output;
	wl_list_for_each(output, &state->outputs, link) {
		struct grim_buffer *buffer = output->buffer;
		if (buffer == NULL) {
			continue;
		}

		pixman_format_code_t pixman_fmt = get_pixman_format(buffer->format);
		if (!pixman_fmt) {
			fprintf(stderr, "unsupported format %d = 0x%08x\n",
				buffer->format, buffer->format);
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

		pixman_image_t *output_image = pixman_image_create_bits(
			pixman_fmt, buffer->width, buffer->height,
			buffer->data, buffer->stride);
		if (!output_image) {
			fprintf(stderr, "Failed to create image\n");
			return NULL;
		}
		pixman_image_set_filter(output_image, PIXMAN_FILTER_BEST, NULL, 0);

		// The transformation `out2com` will send a pixel in the output_image
		// to one in the common_image
		struct pixman_f_transform out2com;
		pixman_f_transform_init_identity(&out2com);
		pixman_f_transform_translate(&out2com, NULL,
			-(double)output->geometry.width / 2,
			-(double)output->geometry.height / 2);
		pixman_f_transform_scale(&out2com, NULL,
			(double)output_width / raw_output_width,
			(double)output_height * output_flipped_y / raw_output_height);
		pixman_f_transform_rotate(&out2com, NULL,
			round(cos(get_output_rotation(output->transform))),
			round(sin(get_output_rotation(output->transform))));
		pixman_f_transform_scale(&out2com, NULL, output_flipped_x, 1);
		pixman_f_transform_translate(&out2com, NULL,
			(double)output_width / 2,
			(double)output_height / 2);
		pixman_f_transform_translate(&out2com, NULL, output_x, output_y);
		pixman_f_transform_scale(&out2com, NULL, scale, scale);

		struct grim_box composite_dest;
		bool grid_aligned;
		compute_composite_region(&out2com, buffer->width,
			buffer->height, &composite_dest, &grid_aligned);

		pixman_f_transform_translate(&out2com, NULL,
			-composite_dest.x, -composite_dest.y);

		struct pixman_f_transform com2out;
		pixman_f_transform_invert(&com2out, &out2com);
		struct pixman_transform c2o_fixedpt;
		pixman_transform_from_pixman_f_transform(&c2o_fixedpt, &com2out);
		pixman_image_set_transform(output_image, &c2o_fixedpt);

		bool overlapping = false;
		struct grim_output *other_output;
		wl_list_for_each(other_output, &state->outputs, link) {
			if (output != other_output && intersect_box(&output->logical_geometry,
					&other_output->logical_geometry)) {
				overlapping = true;
			}
		}
		/* OP_SRC copies the image instead of blending it, and is much
		 * faster, but this a) is incorrect in the weird case where
		 * logical outputs overlap and are partially transparent b)
		 * can draw the edge between two outputs incorrectly if that
		 * edge is not exactly grid aligned in the common image */
		pixman_op_t op = (grid_aligned && !overlapping) ? PIXMAN_OP_SRC : PIXMAN_OP_OVER;
		pixman_image_composite32(op, output_image, NULL, common_image,
			0, 0, 0, 0, composite_dest.x, composite_dest.y,
			composite_dest.width, composite_dest.height);

		pixman_image_unref(output_image);
	}

	pixman_image_unref(common_image);
	cairo_surface_mark_dirty(surface);
	return surface;
}
