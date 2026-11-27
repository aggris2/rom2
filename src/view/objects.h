/* The objects of a map, its trees and rocks, as things to draw: a
 * sprite for each one the camera can see, in the order they are drawn.
 */
#ifndef ROM2_VIEW_OBJECTS_H
#define ROM2_VIEW_OBJECTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/map.h"
#include "data/sprite_sheet.h"
#include "view/camera.h"
#include "view/frame.h"
#include "view/light.h"

/* The kinds of object a map can name: a square holds a byte, 0 for
 * none and a kind's ID plus 1. */
#define OBJECT_KINDS 255

/* How one kind of object looks: its sheet on the card, and how a frame
 * is cut out of it. A kind whose picture could not be loaded is not
 * `known`, and is not drawn. */
typedef struct ObjectLook {
    bool known;
    uint16_t sheet;
    uint16_t cell_width;
    uint16_t cell_height;
    int16_t anchor_x;       /* the point of a cell that stands on the map */
    int16_t anchor_y;
    SpriteRound round;      /* how it moves: its frames are in one row */
} ObjectLook;

/* The most sprites objects_sprites() writes for a camera, given the
 * looks: room for this many is enough. */
size_t objects_most(const Camera *camera, const ObjectLook *looks);

/* Writes a sprite for each object the camera can see and returns how
 * many it wrote. `looks` has OBJECT_KINDS entries, one for each ID.
 * `tick` is how many ticks of the game have passed.
 *
 * An object stands with its anchor on the middle of its square, raised
 * by the height of its square, and is as bright as LIGHT_STANDING.
 *
 * An object that moves, such as a tree in the wind, shows the frame of
 * its round for the tick. Each is at another point of its round, by
 * the square it stands on, so that a wood does not sway as one tree:
 * the object on the square (x, y) is x + x * y ticks ahead, as in the
 * original.
 *
 * The sprites come in the order they are to be drawn: row after row
 * from the top, and in a row from the right, so that what is nearer
 * the bottom of the screen covers what is behind it. Squares around
 * the screen are looked at too, as far as the largest object reaches:
 * a tree that stands below the screen may show its top in it. */
size_t objects_sprites(const Map *map, const ObjectLook *looks,
                       uint64_t tick, const Camera *camera, Sprite *out);

#endif
