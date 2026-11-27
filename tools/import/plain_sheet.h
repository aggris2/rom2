/* Laying the frames of a sprite file out as a sheet, in the order of the
 * file, for the sprites no registry describes: the cursors, the moving
 * parts of the interface, and the fonts, whose frames are their glyphs.
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

/* The three kinds of sprite file. They differ in what a pixel holds. */
typedef enum PlainSheetKind {
    PLAIN_SHEET_256, /* an index; solid, 255, unless the index is 0 */
    PLAIN_SHEET_16A, /* an index and its own solidness */
    PLAIN_SHEET_16   /* solidness alone: every index is 0 */
} PlainSheetKind;

/* Builds the sheet of a sprite. `columns` is how many cells a row has,
 * or 0 for as many as fit in PLAIN_SHEET_WIDTH. A file's 15 levels of
 * solidness are spread over 17 to 255.
 *
 * Returns false, and leaves `*out` alone, if the sprite has no frames, a
 * frame cannot be read, the sheet would be larger than a PNG can be, or
 * memory runs out. */
bool plain_sheet_build(const Sprite256 *sprite, PlainSheetKind kind,
    uint32_t columns, PlainSheet *out);

/* Frees the sheet and sets it to zero. */
void plain_sheet_free(PlainSheet *sheet);

/* The size of the whole sheet in pixels. */
uint32_t plain_sheet_width(const PlainSheet *sheet);
uint32_t plain_sheet_height(const PlainSheet *sheet);

#endif
