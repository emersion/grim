#ifndef _BOX_H
#define _BOX_H

#include <stdbool.h>
#include <stdint.h>

struct grim_box {
	int32_t x, y;
	int32_t width, height;
};

bool parse_box(struct grim_box *box, const char *str);

#endif
