#ifndef _WRITE_PNG_H
#define _WRITE_PNG_H

#include <pixman.h>
#include <stdio.h>

int write_to_png_stream(pixman_image_t *image, FILE *stream, int comp_level);

#endif
