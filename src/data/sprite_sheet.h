/* A sprite sheet: the picture that holds the frames of a unit, an
 * object or a structure, and how it is cut. It is the [sprite] part of
 * a definition.
 *
 * A sheet is a grid of cells of one size, a frame in each. The anchor
 * is the point of a cell that stands on the thing's place on the map:
 * for a unit, between its feet. A structure is placed by its squares
 * and its sheet has no anchor, which is then the cell's top left
 * corner.
 */
#ifndef ROM2_DATA_SPRITE_SHEET_H
#define ROM2_DATA_SPRITE_SHEET_H

#include <stdbool.h>
#include <stdint.h>

#include "base/png.h"
#include "base/toml.h"

/* The room the name of a sheet's file needs, with its closing zero. */
#define SPRITE_SHEET_NAME_SIZE 64

/* How solid a pixel of a sheet is. A pixel of the figure is solid, and
 * one of its soft edge is half see-through, which smooths the outline.
 * That it is half is taken from how the original looks, and was not
 * measured in it. */
#define SPRITE_SHEET_SOLID 255
#define SPRITE_SHEET_EDGE 128

typedef struct SpriteSheet {
    char file[SPRITE_SHEET_NAME_SIZE];  /* beside the definition */
    /* The picture of the soft edges, of the same size, or empty if the
     * sheet has none. */
    char edges[SPRITE_SHEET_NAME_SIZE];
    /* The picture of its palettes, 256 pixels wide with a palette in
     * each row, or empty if the sheet has only the one of its own
     * picture. */
    char palettes[SPRITE_SHEET_NAME_SIZE];
    uint16_t cell_width;
    uint16_t cell_height;
    int16_t anchor_x;                   /* from the cell's top left corner */
    int16_t anchor_y;
} SpriteSheet;

/* Reads the [sprite] table of a definition, given the table of its
 * top.
 *
 * Returns false, and leaves `*out` alone, if there is no such table,
 * it lacks the sheet or the cell, the name of the sheet, of its edges
 * or of its palettes is empty, too long or not a plain file name, a
 * cell has no size or one over 4096, or the anchor is further than
 * 4096 from the cell's corner. A sheet need not have edges, palettes
 * or an anchor. */
bool sprite_sheet_read(const TomlTable *top, SpriteSheet *out);

/* Joins the figure of a sheet and its soft edges into the two numbers
 * a pixel has when it is drawn: which colour of the palette, and how
 * solid it is.
 *
 * A pixel of the figure keeps its colour and is SPRITE_SHEET_SOLID.
 * Where the figure is empty and the edge is not, the pixel takes the
 * edge's colour, which is one of the figure's palette, and is
 * SPRITE_SHEET_EDGE. Where both are empty it is 0 and 0. A pixel is
 * empty if its colour is one the picture's palette makes see-through.
 * `edges` may be NULL for a sheet without soft edges.
 *
 * `colours` and `solid` each have room for the figure's width times
 * its height. Returns false, and leaves them alone, if a picture has no
 * palette or the two are not of one size. */
bool sprite_sheet_join(const Png *figure, const Png *edges,
                       uint8_t *colours, uint8_t *solid);

#endif
