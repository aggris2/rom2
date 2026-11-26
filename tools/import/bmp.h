/* Reading the original game's .bmp pictures.
 *
 * The terrain is kept as BMP pictures of 8 bits for each pixel: a table
 * of up to 256 colours and, for each pixel, an index into it. The rows
 * are stored from the bottom of the picture up, each filled out to a
 * multiple of 4 bytes.
 *
 * This reads that kind and no other: not compressed, 8 bits, bottom up.
 * Every 8-bit BMP of the original game is of this kind.
 */
#ifndef ROM2_IMPORT_BMP_H
#define ROM2_IMPORT_BMP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/rgb.h"

/* The largest width or height a picture may have. */
#define BMP_MAX_SIDE 16384u

/* A decoded picture: `width * height` pixels, row after row from the
 * top, in a block from malloc() that the picture owns. Each pixel is an
 * index into the palette. Colours the file's table does not have are
 * black. */
typedef struct Bmp8 {
    uint32_t width;
    uint32_t height;
    uint8_t *pixels;
    Rgb palette[256];
} Bmp8;

/* Reads an 8-bit BMP held in memory. Give the picture back with
 * bmp8_free().
 *
 * Returns false, and leaves `*out` alone, if the bytes are not a BMP of
 * the kind described above, the picture is larger than BMP_MAX_SIDE or
 * has no size, its colour table or its pixels are cut short, or memory
 * runs out. */
bool bmp8_read(const uint8_t *data, size_t size, Bmp8 *out);

/* Frees the pixels of a picture and clears it. */
void bmp8_free(Bmp8 *bmp);

#endif
