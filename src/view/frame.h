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

/* A square of ground has four corners: top left, top right, low left,
 * low right, in that order. It is drawn as two triangles, cut from the
 * top right corner to the low left. */
#define FRAME_SQUARE_CORNERS 4

/* A level of light is so many of these parts of a tile's own colours:
 * at this level the ground is as its tile, and above it brighter. */
#define FRAME_LIGHT_WHOLE 32

/* A corner of a square of ground, raised by the height of the ground
 * there and lit by the light on it. On a slope a square is not a
 * rectangle, and a Sprite cannot say it.
 *
 * The graphics card is handed these records as they are, so the order
 * and the sizes of the fields are what ground.vert takes. */
typedef struct GroundCorner {
    int16_t x;              /* where it goes on the screen, in pixels */
    int16_t y;
    uint16_t point_x;       /* the point of the sheet that is there */
    uint16_t point_y;
    uint16_t tile_x;        /* the top left corner of its tile in the */
    uint16_t tile_y;        /* sheet: the same for all four of a square */
    uint16_t light;         /* how bright: see FRAME_LIGHT_WHOLE */
    uint16_t unused;        /* 0: fills the record to 16 bytes */
} GroundCorner;

typedef struct Frame {
    uint16_t ground_sheet;          /* the sheet of ground tiles */
    const GroundCorner *ground;     /* four for each square */
    size_t ground_count;
    const Sprite *sprites;
    size_t sprite_count;
} Frame;

#endif
