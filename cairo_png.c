#include <assert.h>
#include <cairo.h>
#include <fcntl.h>
#include <limits.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cairo_png.h"

cairo_status_t cairo_surface_write_to_png_mem(cairo_surface_t *sfc,
                                              unsigned char **data,
                                              unsigned long *len) {
  int png_color_type;
  int bpc;
#ifdef PNG_pHYs_SUPPORTED
  double dpix, dpiy;
#endif

  int height = cairo_image_surface_get_height(sfc);
  int width = cairo_image_surface_get_width(sfc);
  cairo_format_t cformat = cairo_image_surface_get_format(sfc);

  /* PNG complains about "Image width or height is zero in IHDR" */
  if (width == 0 || height == 0) {
    return CAIRO_STATUS_WRITE_ERROR;
  }

  png_structp png =
    png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png == NULL) {
    return CAIRO_STATUS_NO_MEMORY;
  }

  png_infop info = png_create_info_struct(png);
  if (info == NULL) {
    png_destroy_write_struct(&png, &info);
    return CAIRO_STATUS_NO_MEMORY;
  }

#ifdef PNG_SETJMP_SUPPORTED
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    return 2;
  }
#endif

  //png_set_write_fn(png, &state, my_png_write_data, png_simple_output_flush_fn);

  FILE *f = fopen("/tmp/meow.png", "wb");
  if (f == NULL) {
    fprintf(stderr, "failed to open output file\n");
    exit(EXIT_FAILURE);
  }
  png_init_io(png, f);

  switch (cformat) {
  case CAIRO_FORMAT_ARGB32:
    bpc = 8;
    png_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
    break;
  case CAIRO_FORMAT_INVALID:
  default:
    png_destroy_write_struct(&png, &info);
    return CAIRO_STATUS_INVALID_FORMAT;
  }

  png_set_IHDR(png, info, width, height, bpc, png_color_type,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

#ifdef PNG_pHYs_SUPPORTED
  cairo_surface_get_fallback_resolution(sfc, &dpix, &dpiy);
  png_set_pHYs(png, info, dpix * 1000 / 25.4, dpiy * 1000 / 25.4,
               PNG_RESOLUTION_METER);
#endif

  png_write_info(png, info);

  for (size_t i = 0; i < (size_t)height; ++i) {
    png_bytep row = (png_byte *)cairo_image_surface_get_data(sfc) + i * cairo_image_surface_get_stride(sfc);
    png_write_row(png, row);
  }

  png_write_end(png, info);

  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t cj_write(void *closure, const unsigned char *data,
                               unsigned int length) {
  if (write((long)closure, data, length) < (ssize_t)length) {
    return CAIRO_STATUS_WRITE_ERROR;
  } else {
    return CAIRO_STATUS_SUCCESS;
  }
}

cairo_status_t cairo_surface_write_to_png_stream(cairo_surface_t *sfc,
                                                 cairo_write_func_t write_func,
                                                 void *closure) {
  cairo_status_t e;
  unsigned char *data = NULL;
  unsigned long len = 0;

  e = cairo_surface_write_to_png_mem(sfc, &data, &len);
  if (e == CAIRO_STATUS_SUCCESS) {
    assert(sizeof(unsigned long) <= sizeof(size_t) ||
           !(len >> (sizeof(size_t) * CHAR_BIT)));
    e = write_func(closure, data, len);
    free(data);
  }

  return e;
}

cairo_status_t cairo_surface_write_to_png(cairo_surface_t *sfc,
                                          const char *filename) {
  cairo_status_t e;
  int outfile;

  outfile =
      open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  if (outfile == -1) {
    return CAIRO_STATUS_DEVICE_ERROR;
  }

  e = cairo_surface_write_to_png_stream(sfc, cj_write, (void *)(long)outfile);

  close(outfile);
  return e;
}
