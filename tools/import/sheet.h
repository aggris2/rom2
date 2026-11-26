/* A sprite sheet: a grid of cells of one size, each holding one frame.
 *
 * ROM2 keeps all the frames of a thing in one picture laid out this way,
 * so that drawing a frame is "take the cell at this column and row".
 * Pixels are palette indices, and 0 is transparent.
 */
#ifndef ROM2_IMPORT_SHEET_H
#define ROM2_IMPORT_SHEET_H

#include <stdbool.h>
#include <stdint.h>

/* The largest width or height of a whole sheet, in pixels. */
#define SHEET_MAX_SIDE 16384u

typedef struct Sheet {
    uint32_t cell_width;
    uint32_t cell_height;
    uint32_t columns;
    uint32_t rows;
    uint8_t *pixels; /* owned; (columns * cell_width) wide, row after row */
} Sheet;

/* Makes an empty, fully transparent sheet. Give it back with
 * sheet_free().
 *
 * Returns false, and leaves `*out` alone, if a size is 0, the sheet would
 * be wider or higher than SHEET_MAX_SIDE, or memory runs out. */
bool sheet_create(Sheet *out, uint32_t cell_width, uint32_t cell_height,
    uint32_t columns, uint32_t rows);

/* Frees a sheet's pixels and clears it. Safe on a cleared sheet. */
void sheet_free(Sheet *sheet);

/* The size of the whole sheet in pixels. */
uint32_t sheet_width(const Sheet *sheet);
uint32_t sheet_height(const Sheet *sheet);

/* Copies a frame of `width` by `height` pixels into the cell at `column`
 * and `row`, with its top left corner `x` pixels from the left and `y`
 * pixels from the top of the cell. With `mirror`, the frame is put in
 * flipped left to right. Pixels that are 0 in the frame leave what is in
 * the cell as it is.
 *
 * Returns false, and changes nothing, if there is no such cell or the
 * frame does not lie fully inside it. */
bool sheet_put(Sheet *sheet, uint32_t column, uint32_t row,
    const uint8_t *pixels, uint32_t width, uint32_t height, uint32_t x,
    uint32_t y, bool mirror);

#endif
