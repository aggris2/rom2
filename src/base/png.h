/* Writing and reading PNG pictures.
 *
 * ROM2 keeps its sprites as 8-bit indexed PNGs: each pixel is a number
 * from 0 to 255, and the file carries a palette of 256 colours. The pixel
 * numbers are exactly the ones in the original game's files, so nothing is
 * lost and a sprite can still be recoloured by swapping its palette.
 *
 * The pixel data is packed with zlib, which is lossless: a reader gets
 * back exactly the pixel numbers that were written.
 */
#ifndef ROM2_BASE_PNG_H
#define ROM2_BASE_PNG_H

#include <stdbool.h>
#include <stdint.h>

#include "base/buffer.h"
#include "base/rgb.h"

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

/* As png_write_indexed(), but with no transparent index: index 0 is a
 * colour like the others. ROM2 uses this for the ground, which has no
 * holes. */
bool png_write_indexed_solid(ByteBuffer *out, uint32_t width,
    uint32_t height, const uint8_t *pixels, const Rgb palette[256]);

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

/* What the bytes of a picture that was read are. */
typedef enum PngKind {
    PNG_INDEXED, /* one byte for a pixel: an index into the palette */
    PNG_GREY,    /* one byte for a pixel: a shade, 0 black to 255 white */
    PNG_RGB      /* three bytes for a pixel: red, green, blue */
} PngKind;

/* A picture read from a PNG file. */
typedef struct Png {
    uint32_t width;
    uint32_t height;
    PngKind kind;
    /* Owned. The pixels row after row from the top: `width * height`
     * bytes, or three times that for PNG_RGB. */
    uint8_t *pixels;
    /* For PNG_INDEXED: the colour of each index, and how solid it is,
     * from 0 for not there to 255. An index the file gives no colour is
     * black, and one it says nothing of is solid. */
    Rgb palette[256];
    uint8_t alpha[256];
} Png;

/* Reads a PNG file held in memory.
 *
 * It reads what ROM2 writes and what a picture editor makes of it when
 * it saves one again: 8 bits for each number, indexed, grey or full
 * colour, with any of PNG's five ways of storing a row. A picture with
 * its own solidness for each pixel, one of another depth and an
 * interlaced one are refused.
 *
 * Returns false, and leaves `*out` alone, if the bytes are not such a
 * PNG file, are cut short or damaged, the picture has no size or is
 * larger than PNG_MAX_SIDE, or memory runs out. */
bool png_read(const uint8_t *data, size_t size, Png *out);

/* Frees the pixels of a picture and sets it to zero. */
void png_free(Png *png);

#endif
