/* The units of a map as things to draw: a sprite for each one the
 * camera can see, standing where the map has it.
 */
#ifndef ROM2_VIEW_UNITS_H
#define ROM2_VIEW_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/map.h"
#include "data/unit_kind.h"
#include "view/camera.h"
#include "view/sprite.h"

/* The types of unit there can be: a map names a type by its ID. */
#define UNIT_TYPES 256

/* The ways a unit can face as it stands: a full turn in this many
 * steps. The first row of a unit's sheet has a frame for each. */
#define UNIT_FACINGS 16

/* How one type of unit looks: its sheet on the card, and how a frame
 * is cut out of it. A type whose picture could not be loaded is not
 * `known`, and is not drawn. */
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
} UnitLook;

/* The most sprites units_sprites() writes for a map: room for this
 * many is enough. */
size_t units_most(const Map *map);

/* Writes a sprite for each unit the camera can see and returns how
 * many it wrote. `looks` has UNIT_TYPES entries, one for each ID.
 * `faces` has UNIT_KIND_IDS entries, the face of the kind with each
 * ID as UnitKind has it, and 0 for a kind that is not known.
 *
 * A unit stands with its anchor on its place, raised by the height of
 * the ground at that point, shows the standing frame of the way
 * it faces, and is as bright as LIGHT_STANDING. One that faces a way
 * there is not is not drawn.
 *
 * The frames of the first row do not start with the way a map counts
 * from: the frame of a facing is the one half a turn further on,
 * facing 0 the ninth frame and facing 8 the first.
 *
 * A unit whose sheet goes by the player is drawn in the palette that
 * its player's colour numbers, and any other in the palette of its
 * kind's face. One whose player the map does not have, or whose kind
 * is not known, is drawn in its sheet's first.
 *
 * The sprites are not in the order they are to be drawn, but each has
 * its turn: see sprites_sort(). */
size_t units_sprites(const Map *map, const UnitLook *looks,
                     const uint16_t *faces, const Camera *camera,
                     Sprite *out);

#endif
