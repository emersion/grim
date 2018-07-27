#ifndef _CAIRO_JPEG_H
#define _CAIRO_JPEG_H

#include <cairo.h>

cairo_status_t cairo_image_surface_write_to_jpeg_mem(cairo_surface_t *sfc, unsigned char **data, size_t *len, int quality);
cairo_status_t cairo_surface_write_to_jpeg_stream(cairo_surface_t *sfc, cairo_write_func_t write_func, void *closure, int quality);
cairo_status_t cairo_surface_write_to_jpeg(cairo_surface_t *sfc, const char *filename, int quality);

#endif
