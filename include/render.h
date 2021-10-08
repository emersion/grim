#ifndef _RENDER_H
#define _RENDER_H

#include <pixman.h>

#include "grim.h"

pixman_image_t *render(struct grim_state *state, struct grim_box *geometry,
	double scale);

#endif
