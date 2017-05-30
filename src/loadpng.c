/*
 * Copyright 2017 DiUS Computing Pty Ltd. All rights reserved.
 *
 * Released under GPLv3, see LICENSE for details.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */
#include "loadpng.h"
#include <stdlib.h>
#include <assert.h>
#include <png.h>

static_assert(sizeof(png_byte) == sizeof( ((ql_raster_image_t *)0)->data[0]), "Code relies on png_byte being compatible with ql_raster_image_t data ");

ql_raster_image_t *loadpng(const char *path)
{
  ql_raster_image_t *ret = NULL;

  if (!path)
    goto out;

  FILE *f = fopen(path, "rb");
  if (!f)
    goto out;

  uint8_t header[8];
  if (fread(header, 1, sizeof(header), f) != 8)
    goto close_out;

  if (!png_check_sig(header, sizeof(header)))
    goto close_out;

  png_infop info_ptr = NULL, end_ptr = NULL;
  png_structp png_ptr =
    png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr)
    goto close_out;

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    goto destroy_read_out;

  end_ptr = png_create_info_struct(png_ptr);
  if (!end_ptr)
    goto destroy_read_out;

  if (setjmp(png_jmpbuf(png_ptr)))
    goto destroy_read_out;

  png_init_io(png_ptr, f);
  png_set_sig_bytes(png_ptr, sizeof(header));

  png_read_info(png_ptr, info_ptr);

  png_set_strip_alpha(png_ptr);
  png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1); // force into grayscale
  png_set_expand_gray_1_2_4_to_8(png_ptr); // get us a known output format

  png_read_update_info(png_ptr, info_ptr);

  png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
  png_uint_32 height = png_get_image_height(png_ptr, info_ptr);

  png_bytepp row_ptrs = calloc(height, sizeof(png_bytep));
  if (!row_ptrs)
    goto destroy_read_out;

  const unsigned row_bytes = width * sizeof(png_byte);
  ql_raster_image_t *img =
    calloc(1, sizeof(ql_raster_image_t) + height * row_bytes);
  if (!img)
    goto free_image_out;

  if (setjmp(png_jmpbuf(png_ptr)))
    goto free_image_out;

  for (png_uint_32 i = 0; i < height; ++i)
    row_ptrs[i] = (png_bytep)(img->data + (i * row_bytes));

  png_read_image(png_ptr, row_ptrs);
  png_read_end(png_ptr, end_ptr);

  img->height = height;
  img->width = width;

  ret = img;
  img = NULL; // don't free it, we're returning it now

free_image_out:
  free(img);
  free(row_ptrs);
destroy_read_out:
  png_destroy_read_struct(
    &png_ptr, info_ptr ? &info_ptr : NULL, end_ptr ? &end_ptr : NULL);
close_out:
  fclose(f);
out:
  return ret;
}
