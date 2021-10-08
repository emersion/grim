#ifndef _WRITE_PPM_H
#define _WRITE_PPM_H

#include <cairo.h>
#include <stdio.h>

int cairo_surface_write_to_ppm_stream(cairo_surface_t *sfc, FILE *stream);

#endif
