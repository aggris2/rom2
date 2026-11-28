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

/* The most ticks one round of a sheet's animation may last. */
#define SPRITE_SHEET_MAX_ROUND 128

/* The animation of a sheet, which goes round and round: the frame it
 * shows in each tick of one round. A sheet that does not move has a
 * round of no length, and shows its first frame. */
typedef struct SpriteRound {
    uint16_t length;                            /* in ticks */
    uint8_t frame[SPRITE_SHEET_MAX_ROUND];
} SpriteRound;

/* The frame a round shows at a turn: the number of ticks that have
 * passed, from whenever. */
uint32_t sprite_round_frame(const SpriteRound *round, uint64_t turn);

/* Reads a round from the two lists of a table, `frames` and `ticks`:
 * which frame to show, and for how many ticks. A table with neither
 * list gives a round of no length.
 *
 * Returns false, and leaves `*out` alone, if the table has one list
 * and not the other, they differ in length or are empty, a frame is
 * not from 0 to `columns` - 1 or not below 256, a frame is shown for
 * less than a tick, or the round lasts more than
 * SPRITE_SHEET_MAX_ROUND ticks. */
bool sprite_round_read(const TomlTable *table, int32_t columns,
                       SpriteRound *out);

/* A box of a cell, from the cell's anchor: `right` and `bottom` are
 * the first pixels past it. */
typedef struct SpriteBox {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} SpriteBox;

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
    uint16_t columns;                   /* frames in a row of the sheet */
    SpriteRound round;
    /* The part of a frame a click takes the thing by, and over which
     * its health is shown. All zeros for a sheet that names none. */
    SpriteBox selection;
} SpriteSheet;

/* Reads the [sprite] table of a definition, given the table of its
 * top.
 *
 * Returns false, and leaves `*out` alone, if there is no such table,
 * it lacks the sheet or the cell, the name of the sheet, of its edges
 * or of its palettes is empty, too long or not a plain file name, a
 * cell has no size or one over 4096, or the anchor is further than
 * 4096 from the cell's corner, or its `selection` is not four numbers,
 * left, top, right and bottom, each within 4096 of the anchor and the
 * first two not past the last two. A sheet need not have edges,
 * palettes, an anchor or a selection, and one that does not say how
 * many columns it has has one.
 *
 * A sheet that moves says so with the two lists of a round, `frames`
 * and `ticks`, and is refused too if sprite_round_read() refuses
 * them. */
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
