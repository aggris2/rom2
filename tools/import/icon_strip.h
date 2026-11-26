/* Decoding the inventory pictures of items.
 *
 * Each variant of a shape has a picture of its own for the inventory, a
 * .16a file of one frame with its own palette. A strip holds one or more
 * of them in cells of one size, in three parts: the colour of each pixel
 * as an index, how solid each pixel is, and the palettes, one for each
 * cell. Cell `n` is drawn with palette `n`.
 *
 * ROM2 writes each picture as a file of its own, from a strip of one
 * cell, because a PNG holds one palette.
 */
#ifndef ROM2_IMPORT_ICON_STRIP_H
#define ROM2_IMPORT_ICON_STRIP_H

#include <stdbool.h>
#include <stdint.h>

#include "import/rgb.h"
#include "import/sprite256.h"

/* The largest cell, and the most cells, a strip may have. The original
 * game's pictures are 80 by 80, and a shape has 14 at most. */
#define ICON_STRIP_MAX_SIDE 1024u
#define ICON_STRIP_MAX_CELLS 128u

typedef struct IconStrip {
    uint32_t cell_width;
    uint32_t cell_height;
    uint32_t columns;
    /* Two blocks of `cell_width * columns` by `cell_height` bytes, row
     * after row from the top: each pixel's colour as an index into its
     * cell's palette, and how solid it is, 0 to 255. */
    uint8_t *pixels;
    uint8_t *solid;
    /* `columns` palettes of 256 colours, one after the other. */
    Rgb *palettes;
} IconStrip;

/* Makes an empty strip of `columns` cells. Give it back with
 * icon_strip_free().
 *
 * Returns false, and leaves `*out` alone, if a size is 0 or over its
 * limit, or memory runs out. */
bool icon_strip_create(IconStrip *out, uint32_t cell_width,
    uint32_t cell_height, uint32_t columns);

/* Puts the first frame of a .16a file into the middle of cell `column`,
 * and its palette into the strip. The file's 15 levels of solidness are
 * spread over 17 to 255.
 *
 * Returns false, and leaves the strip as it was, if there is no such
 * cell, the file has no palette or no frame, or the frame cannot be
 * read or is larger than a cell. */
bool icon_strip_put(IconStrip *strip, uint32_t column,
    const Sprite256 *icon);

/* Frees a strip's blocks and clears it. */
void icon_strip_free(IconStrip *strip);

#endif
