/* Writing PNG pictures.
 *
 * ROM2 keeps its sprites as 8-bit indexed PNGs: each pixel is a number
 * from 0 to 255, and the file carries a palette of 256 colours. The pixel
 * numbers are exactly the ones in the original game's files, so nothing is
 * lost and a sprite can still be recoloured by swapping its palette.
 *
 * The pixel data is packed with zlib, which is lossless: a reader gets
 * back exactly the pixel numbers that were written.
 */
#ifndef ROM2_IMPORT_PNG_H
#define ROM2_IMPORT_PNG_H

#include <stdbool.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/rgb.h"

/* The largest width or height that can be written. */
#define PNG_MAX_SIDE 16384u

/* Adds a complete PNG file to the end of `out`.
 *
 * `pixels` holds `width * height` palette indices, row after row from the
 * top. `palette` gives the colour of each index. Index 0 is written as
 * fully transparent, whatever colour the palette gives it.
 *
 * Returns false, and leaves `out` as it was, if the width or height is 0
 * or more than PNG_MAX_SIDE, or memory runs out. */
bool png_write_indexed(ByteBuffer *out, uint32_t width, uint32_t height,
    const uint8_t *pixels, const Rgb palette[256]);

/* Adds a complete PNG file in full colour to the end of `out`: every
 * pixel has its own colour and none is transparent. ROM2 uses this for
 * pictures of palettes, where a row of 256 pixels is one palette.
 *
 * `pixels` holds `width * height` colours, row after row from the top.
 *
 * Returns false, and leaves `out` as it was, if the width or height is 0
 * or more than PNG_MAX_SIDE, or memory runs out. */
bool png_write_rgb(ByteBuffer *out, uint32_t width, uint32_t height,
    const Rgb *pixels);

/* Adds a complete PNG file in shades of grey to the end of `out`. ROM2
 * uses this for a picture that says how solid each pixel of a sprite
 * is: black for not there, white for solid.
 *
 * `pixels` holds `width * height` bytes, row after row from the top,
 * from 0 for black to 255 for white.
 *
 * Returns false, and leaves `out` as it was, if the width or height is 0
 * or more than PNG_MAX_SIDE, or memory runs out. */
bool png_write_grey(ByteBuffer *out, uint32_t width, uint32_t height,
    const uint8_t *pixels);

#endif
