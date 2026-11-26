/* Reading the original game's .bmp pictures.
 *
 * The original has two kinds. The terrain and a few others are of 8 bits
 * for each pixel: a table of up to 256 colours and, for each pixel, an
 * index into it. The pictures of the interface are of 24 bits: for each
 * pixel its blue, its green and its red. In both the rows are stored
 * from the bottom of the picture up, each filled out to a multiple of 4
 * bytes, and nothing is compressed.
 *
 * This reads those two kinds and no other. Every BMP of the original
 * game is one of them.
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

/* A decoded picture in full colour: `width * height` colours, row after
 * row from the top, in a block from malloc() that the picture owns. */
typedef struct Bmp24 {
    uint32_t width;
    uint32_t height;
    Rgb *pixels;
} Bmp24;

/* Reads a 24-bit BMP held in memory. Give the picture back with
 * bmp24_free().
 *
 * Returns false, and leaves `*out` alone, if the bytes are not a BMP of
 * the kind described above, the picture is larger than BMP_MAX_SIDE or
 * has no size, its pixels are cut short, or memory runs out. */
bool bmp24_read(const uint8_t *data, size_t size, Bmp24 *out);

/* Frees the pixels of a picture and clears it. */
void bmp24_free(Bmp24 *bmp);

/* Says how many bits for each pixel a BMP has, 8 or 24 for those that
 * can be read here, without reading the picture. Returns 0 if the bytes
 * are too short to be a BMP or do not start as one. */
uint32_t bmp_bits(const uint8_t *data, size_t size);

#endif
