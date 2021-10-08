#ifndef _WRITE_PPM_H
#define _WRITE_PPM_H

#include <cairo.h>

cairo_status_t cairo_surface_write_to_ppm_stream(cairo_surface_t *sfc, cairo_write_func_t write_func, void *closure);

#endif
