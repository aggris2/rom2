/* Laying the frames of a sprite file out as a sheet, in the order of the
 * file, for the sprites no registry describes: the cursors and the
 * moving parts of the interface.
 *
 * The sheet is a grid of cells of one size, the size of the largest
 * frame. The frames fill it row by row from the top left, and each lies
 * in the top left corner of its cell.
 */
#ifndef ROM2_IMPORT_PLAIN_SHEET_H
#define ROM2_IMPORT_PLAIN_SHEET_H

#include <stdbool.h>
#include <stdint.h>

#include "import/sprite256.h"

/* A row of the sheet is as many cells as fit in this many pixels, and
 * one cell if the frames are wider. */
#define PLAIN_SHEET_WIDTH 4096u

typedef struct PlainSheet {
    uint32_t frames;
    uint32_t cell_width;
    uint32_t cell_height;
    uint32_t columns;
    uint32_t rows;
    /* Owned. Each is (columns * cell_width) wide, row after row: the
     * colour of every pixel as an index into the palette, and how solid
     * it is, from 0 for a pixel that is not there to 255. */
    uint8_t *pixels;
    uint8_t *solid;
    /* Owned. The size of each frame. */
    int32_t *widths;
    int32_t *heights;
    /* Whether every frame is as large as a cell. */
    bool one_size;
} PlainSheet;

/* Builds the sheet of a .256 sprite. A pixel is solid, 255, unless its
 * index is 0.
 *
 * Returns false, and leaves `*out` alone, if the sprite has no frames, a
 * frame cannot be read, the sheet would be higher than a PNG can be, or
 * memory runs out. */
bool plain_sheet_build_256(const Sprite256 *sprite, PlainSheet *out);

/* The same for a .16a sprite, whose pixels carry their own solidness:
 * the file's 15 levels are spread over 17 to 255. */
bool plain_sheet_build_16a(const Sprite256 *sprite, PlainSheet *out);

/* Frees the sheet and sets it to zero. */
void plain_sheet_free(PlainSheet *sheet);

/* The size of the whole sheet in pixels. */
uint32_t plain_sheet_width(const PlainSheet *sheet);
uint32_t plain_sheet_height(const PlainSheet *sheet);

#endif
