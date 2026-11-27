/* A frame: everything the view asks the renderer to draw, once.
 *
 * The ground comes first, and the sprites lie on it in the order they
 * are given.
 */
#ifndef ROM2_VIEW_FRAME_H
#define ROM2_VIEW_FRAME_H

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
    uint16_t unused;        /* 0: fills the record to 20 bytes */
    uint16_t tile_x;        /* the top left corner of its tile in the */
    uint16_t tile_y;        /* sheet */
    uint8_t light[FRAME_SQUARE_CORNERS];    /* how bright each corner
                                               is: see FRAME_LIGHT_WHOLE */
} GroundSquare;

typedef struct Frame {
    uint16_t ground_sheet;          /* the sheet of ground tiles */
    const GroundSquare *ground;
    size_t ground_count;
    const Sprite *sprites;
    size_t sprite_count;
} Frame;

#endif
