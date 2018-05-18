#ifndef _RENDER_H
#define _RENDER_H

#include <cairo.h>

#include "grim.h"

cairo_surface_t *render(struct grim_state *state, struct grim_box *geometry,
	double scale);

#endif
