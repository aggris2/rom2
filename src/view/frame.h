/* A frame: everything the view asks the renderer to draw, once.
 *
 * The ground comes first, and the sprites lie on it in the order they
 * are given.
 */
#ifndef ROM2_VIEW_FRAME_H
#define ROM2_VIEW_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "view/sprite.h"

/* A level of light is so many of these parts of a tile's own colours:
 * at this level the ground is as its tile, and above it brighter. */
#define FRAME_LIGHT_WHOLE 32

/* A square of ground has four corners: top left, top right, low left,
 * low right, in that order. */
#define FRAME_SQUARE_CORNERS 4

/* A square of ground. Each of its corners is raised by the height of
 * the ground there and lit by the light on it: on a slope a square is
 * not a rectangle, and a Sprite cannot say it. The left corners are
 * straight above each other, and the right ones a square's side to
 * the right of them.
 *
 * The graphics card is handed these records as they are, so the order
 * and the sizes of the fields are what ground.vert takes. */
typedef struct GroundSquare {
    int16_t y[FRAME_SQUARE_CORNERS];    /* of each corner on the screen,
                                           in pixels */
    int16_t x;              /* of its left corners on the screen */
    uint16_t mark;          /* how strongly it is marked, in 255ths:
                               see FRAME_MARK */
    uint16_t tile_x;        /* the top left corner of its tile in the */
    uint16_t tile_y;        /* sheet */
    uint8_t light[FRAME_SQUARE_CORNERS];    /* how bright each corner
                                               is: see FRAME_LIGHT_WHOLE */
} GroundSquare;

/* A marked square is drawn towards this colour, as far as its mark
 * says: at 255 it would be all of it. A mark is for looking at what
 * the game takes a square to be, and no part of how the game looks. */
#define FRAME_MARK_RED 255
#define FRAME_MARK_GREEN 40
#define FRAME_MARK_BLUE 40

/* How strongly the lines of the grid are drawn over the ground, in
 * 255ths: at 255 they would be white. */
#define FRAME_GRID_STRENGTH 40

typedef struct Frame {
    /* Whether the grid is drawn: a line along the top and the left
     * edge of every square of ground, bent as the ground is. It is for
     * looking at how a map lies, and no part of the game. */
    bool ground_grid;
    uint16_t ground_sheet;          /* the sheet of ground tiles */
    const GroundSquare *ground;
    size_t ground_count;
    const Sprite *sprites;
    size_t sprite_count;
} Frame;

#endif
