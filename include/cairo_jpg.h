#ifndef CAIRO_JPEG_H
#define CAIRO_JPEG_H

/*! This file contains all prototypes for the Cairo-JPEG functions implemented
 * in cairo_jpg.c. See there for the function documentation.
 *
 * @author Bernhard R. Fischer, 2048R/5C5FFD47 bf@abenteuerland.at
 * @version 2016/01/01 r1922
 * @license This code is free software. Do whatever you like to do with it.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <cairo.h>

#ifdef USE_CAIRO_READ_FUNC_LEN_T
/*! This is the type for the stream read function. Which must be implemented by
 * the user if cairo_image_surface_create_from_jpeg_stream() is used. Please
 * note that this prototype is slightly different from cairo_read_func_t which
 * is used by cairo_image_surface_create_from_png_stream().
 * @param closure This parameter is directly passed through by
 * cairo_image_surface_create_from_jpeg_stream().
 * @param data Pointer to data buffer which will receive the data.
 * @param length Size of the data buffer in bytes.
 * @return This function must return the actually length that was read into the
 * buffer. This may actually be less than length which indicates an EOF. In
 * case of any fatal unrecoverable error on the input stream -1 shall be
 * returned.
 */
typedef ssize_t (*cairo_read_func_len_t) (void *closure, unsigned char *data, unsigned int length);
#endif


cairo_status_t cairo_image_surface_write_to_jpeg_mem(cairo_surface_t *sfc, unsigned char **data, size_t *len, int quality);
cairo_status_t cairo_image_surface_write_to_jpeg_stream(cairo_surface_t *sfc, cairo_write_func_t write_func, void *closure, int quality);
cairo_status_t cairo_image_surface_write_to_jpeg(cairo_surface_t *sfc, const char *filename, int quality);
cairo_surface_t *cairo_image_surface_create_from_jpeg_mem(void *data, size_t len);
#ifdef USE_CAIRO_READ_FUNC_LEN_T
cairo_surface_t *cairo_image_surface_create_from_jpeg_stream(cairo_read_func_len_t read_func, void *closure);
#else
cairo_surface_t *cairo_image_surface_create_from_jpeg_stream(cairo_read_func_t read_func, void *closure);
#endif
cairo_surface_t *cairo_image_surface_create_from_jpeg(const char *filename);

#endif

