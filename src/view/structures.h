/* The structures of a map as things to draw: sprites for each one the
 * camera can see.
 *
 * A structure covers several rows of the map, and something may stand
 * behind its front and before its back. So it is not one sprite: it is
 * cut into strips a square high, one for each row of its base, and
 * each strip takes its turn with what else stands in its row. The
 * strip of the back row has with it all of the picture that rises
 * above the base.
 */
#ifndef ROM2_VIEW_STRUCTURES_H
#define ROM2_VIEW_STRUCTURES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/map.h"
#include "data/structure_kind.h"
#include "view/camera.h"
#include "view/sprite.h"

/* How one kind of structure looks: its sheet on the card, and its
 * size in squares, as StructureKind has it. Each frame of the sheet is
 * `width` by `height` squares. A kind whose
 * picture could not be loaded is not `known`, and is not drawn. */
typedef struct StructureLook {
    bool known;
    uint16_t sheet;
    uint16_t width;
    uint16_t depth;
    uint16_t height;
    SpriteRound round;      /* how it moves: its frames are in one row */
    /* A bridge: its sheet is a row of nine pieces a square each, and a
     * map says how many squares it covers. See below. */
    bool pieces;
} StructureLook;

/* The most sprites structures_sprites() writes for a map, given the
 * looks: room for this many is enough. */
size_t structures_most(const Map *map, const StructureLook *looks);

/* Writes the sprites of each structure the camera can see and returns
 * how many it wrote. `looks` has STRUCTURE_KINDS entries, one for each
 * ID. `tick` is how many ticks of the game have passed.
 *
 * A structure stands with the bottom of its picture on the front of
 * its base, and is as bright as LIGHT_STANDING. One that moves, such
 * as a windmill, shows the frame of its round for the tick: all of a
 * kind move as one.
 * All of it is raised by one height, that of the ground under the
 * middle of its base, however the ground lies under the rest.
 *
 * A structure of pieces, a bridge, covers as many squares as its
 * record in the map says, and is a sprite for each of them: the piece
 * for where the square is in the bridge. The nine pieces are, in the
 * order of the sheet, the three of the first row of squares, the three
 * of the rows between and the three of the last, and of each three
 * the first square of a row, those between and the last. A bridge
 * lies flat: it has the turn of what is under everything that stands.
 * One the map gives no size is not drawn.
 *
 * The sprites are not in the order they are to be drawn, but each has
 * its turn: see sprites_sort(). */
size_t structures_sprites(const Map *map, const StructureLook *looks,
                          uint64_t tick, const Camera *camera,
                          Sprite *out);

#endif
