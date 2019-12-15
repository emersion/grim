#ifndef _CAIRO_PNG_H
#define _CAIRO_PNG_H

#include <cairo.h>

cairo_status_t cairo_surface_write_to_png_mem(cairo_surface_t *sfc, unsigned char **data, unsigned long *len);
cairo_status_t cairo_surface_write_to_png_stream(cairo_surface_t *sfc, cairo_write_func_t write_func, void *closure);
cairo_status_t cairo_surface_write_to_png(cairo_surface_t *sfc, const char *filename);

#endif
