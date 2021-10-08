#ifndef _WRITE_JPEG_H
#define _WRITE_JPEG_H

#include <cairo.h>

cairo_status_t cairo_surface_write_to_jpeg_stream(cairo_surface_t *sfc, cairo_write_func_t write_func, void *closure, int quality);

#endif
