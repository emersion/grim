#ifndef _WRITE_PPM_H
#define _WRITE_PPM_H

#include <pixman.h>
#include <stdio.h>

int write_to_ppm_stream(pixman_image_t *image, FILE *stream);

#endif
