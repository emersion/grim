#ifndef _WRITE_JPEG_H
#define _WRITE_JPEG_H

#include <cairo.h>
#include <stdio.h>

int cairo_surface_write_to_jpeg_stream(cairo_surface_t *sfc, FILE *stream, int quality);

#endif
