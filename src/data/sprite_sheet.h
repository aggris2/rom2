/* A sprite sheet: the picture that holds the frames of a unit, an
 * object or a structure, and how it is cut. It is the [sprite] part of
 * a definition.
 *
 * A sheet is a grid of cells of one size, a frame in each. The anchor
 * is the point of a cell that stands on the thing's place on the map:
 * for a unit, between its feet.
 */
#ifndef ROM2_DATA_SPRITE_SHEET_H
#define ROM2_DATA_SPRITE_SHEET_H

#include <stdbool.h>
#include <stdint.h>

#include "base/toml.h"

/* The room the name of a sheet's file needs, with its closing zero. */
#define SPRITE_SHEET_NAME_SIZE 64

typedef struct SpriteSheet {
    char file[SPRITE_SHEET_NAME_SIZE];  /* beside the definition */
    uint16_t cell_width;
    uint16_t cell_height;
    int16_t anchor_x;                   /* from the cell's top left corner */
    int16_t anchor_y;
} SpriteSheet;

/* Reads the [sprite] table of a definition, given the table of its
 * top.
 *
 * Returns false, and leaves `*out` alone, if there is no such table,
 * it lacks the sheet, the cell or the anchor, the name of the sheet is
 * empty, too long or not a plain file name, a cell has no size or one
 * over 4096, or the anchor is further than 4096 from the cell's
 * corner. */
bool sprite_sheet_read(const TomlTable *top, SpriteSheet *out);

#endif
