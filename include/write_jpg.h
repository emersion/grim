#ifndef _WRITE_JPEG_H
#define _WRITE_JPEG_H

#include <pixman.h>
#include <stdio.h>

int write_to_jpeg_stream(pixman_image_t *image, FILE *stream, int quality);

#endif
