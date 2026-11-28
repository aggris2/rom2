/* The units of a game as things to draw: a sprite for each one the
 * camera can see, standing where it is.
 */
#ifndef ROM2_VIEW_UNITS_H
#define ROM2_VIEW_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/map.h"
#include "data/unit_animation.h"
#include "data/unit_kind.h"
#include "mechanics/units.h"
#include "view/camera.h"
#include "view/sprite.h"

/* The types of unit there can be: a map names a type by its ID. */
#define UNIT_TYPES 256

/* The ways a unit's pictures show it facing as it stands: a full turn
 * in this many steps, where the game counts UNIT_TURN. The first row
 * of a unit's sheet has a frame for each. */
#define UNIT_FACINGS 16

/* The looks there can be: one for each type in each dress. That of a
 * type in a dress is at unit_look_at(). */
#define UNIT_LOOKS (UNIT_TYPES * UNIT_DRESSES)

/* How one type of unit looks in one dress: its sheet on the card, and
 * how a frame is cut out of it. One whose picture could not be loaded
 * is not `known`, and is not drawn. */
typedef struct UnitLook {
    bool known;
    uint16_t sheet;
    uint16_t cell_width;
    uint16_t cell_height;
    int16_t anchor_x;       /* the point of a cell that stands on the map */
    int16_t anchor_y;
    /* Its sheet has a palette for each colour a player can have: a
     * human's cloth shows whose it is. If not, it has one for each
     * colouring of its kinds. */
    bool by_player;
    /* What it does as it stands, if it does not stand still: a bat
     * beats its wings. Of no length for a unit that stands still. */
    UnitAnimation idle;
} UnitLook;

/* Where in a list of UNIT_LOOKS the look of a type in a dress is.
 * `type` is less than UNIT_TYPES and `dress` than UNIT_DRESSES. */
size_t unit_look_at(uint16_t type, uint8_t dress);

/* The most sprites units_sprites() writes for a list of units: room
 * for this many is enough. */
size_t units_most(const Units *units);

/* Writes a sprite for each unit the camera can see and returns how
 * many it wrote. `map` is the map they are on, for its ground and its
 * players. `looks` has UNIT_LOOKS entries: a unit is drawn in the look
 * of its type and its dress. `tick` is how many ticks of the game have
 * passed.
 * `kinds` has UNIT_KIND_IDS entries, the kind with each ID, all zeros
 * for one that is not known.
 *
 * A unit stands with its anchor on its place, raised by the height of
 * the ground at that point, shows the standing frame of the way
 * it faces, and is as bright as LIGHT_STANDING.
 *
 * A unit's facing is cut down to the UNIT_FACINGS its pictures have.
 * The frames of the first row do not start with the way the game
 * counts from: the frame of a facing is the one half a turn further
 * on, so a unit that faces the first way shows the ninth frame.
 *
 * A unit with an idle animation does not show a standing frame. It
 * shows the frame of that animation for the tick, from the row of the
 * way it faces: an animation has UNIT_ANIMATION_FACINGS of them, and
 * their rows come after the row of the standing frames, starting half
 * a turn on as those do. All such units move as one.
 *
 * A unit whose sheet goes by the player is drawn in the palette that
 * its player's colour numbers, and any other in the palette of its
 * kind's face. One whose player the map does not have, or whose kind
 * is not known, is drawn in its sheet's first.
 *
 * The sprites are not in the order they are to be drawn, but each has
 * its turn: see sprites_sort(). */
size_t units_sprites(const Map *map, const Units *units,
                     const UnitLook *looks, const UnitKind *kinds,
                     uint64_t tick, const Camera *camera, Sprite *out);

#endif
