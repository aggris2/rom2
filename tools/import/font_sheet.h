/* Laying the glyphs of a font of the original out as one picture.
 *
 * A font is a sprite file with a frame for each character, in the order
 * of the characters. The sheet is a grid 16 cells across, a cell as
 * large as the largest glyph, with each glyph in the top left corner of
 * its own: glyph 0 at the top left, glyph 16 under it.
 */
#ifndef ROM2_IMPORT_FONT_SHEET_H
#define ROM2_IMPORT_FONT_SHEET_H

#include <stdbool.h>
#include <stdint.h>

#include "import/sprite256.h"

/* How many glyphs a row of the sheet has. */
#define FONT_SHEET_COLUMNS 16u

typedef struct FontSheet {
    uint32_t glyphs;
    uint32_t cell_width;
    uint32_t cell_height;
    uint32_t rows;
    /* Owned. Each is (16 * cell_width) wide, row after row. `solid` is
     * how solid every pixel is, from 0 for one that is not there to 255.
     * `pixels` is its colour as an index into the palette, and is NULL
     * for a font of one colour, which has no palette. */
    uint8_t *pixels;
    uint8_t *solid;
} FontSheet;

/* Builds the sheet of a font of one colour, a .16 file. The file's 15
 * levels of solidness are spread over 17 to 255.
 *
 * Returns false, and leaves `*out` alone, if the file has no glyphs, a
 * glyph cannot be read, the sheet would be larger than a PNG can be, or
 * memory runs out. */
bool font_sheet_build_16(const Sprite256 *sprite, FontSheet *out);

/* The same for a font in colours, a .16a file. */
bool font_sheet_build_16a(const Sprite256 *sprite, FontSheet *out);

/* Frees the sheet and sets it to zero. */
void font_sheet_free(FontSheet *sheet);

/* The size of the whole sheet in pixels. */
uint32_t font_sheet_width(const FontSheet *sheet);
uint32_t font_sheet_height(const FontSheet *sheet);

#endif
