#ifndef _WRITE_PPM_H
#define _WRITE_PPM_H

#include <cairo.h>

cairo_status_t cairo_surface_write_to_ppm_mem(cairo_surface_t *sfc, unsigned char **data, unsigned long *len);
cairo_status_t cairo_surface_write_to_ppm_stream(cairo_surface_t *sfc, cairo_write_func_t write_func, void *closure);
cairo_status_t cairo_surface_write_to_ppm(cairo_surface_t *sfc, const char *filename);

#endif
