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
    /* The part of its picture a click takes it by, from its anchor.
     * One with a box of no size cannot be picked. */
    SpriteBox selection;
    /* How many squares its type covers each way: its place is the
     * first of them, the top left, and it is drawn over the middle of
     * them all. 0 counts as 1. And how high over the ground it is
     * drawn, in pixels: what flies is. */
    uint8_t squares;
    uint8_t high;
    /* How it walks: the frames of a step, which go round for as long
     * as it walks. Of no length for a unit without them. */
    UnitAnimation move;
    /* What it does as it stands, if it does not stand still: a bat
     * beats its wings. Of no length for a unit that stands still. */
    UnitAnimation idle;
} UnitLook;

/* Where in a list of UNIT_LOOKS the look of a type in a dress is.
 * `type` is less than UNIT_TYPES and `dress` than UNIT_DRESSES. */
size_t unit_look_at(uint16_t type, uint8_t dress);

/* How high over the ground a unit of the air is drawn, in pixels, as
 * the original draws one. A type of the air is one whose definition
 * states a `z` that is not 0: the number itself is not this height. */
#define UNIT_AIR_HIGH 16

/* How far the clock is from the last tick to the next when a frame is
 * drawn, in these parts: what walks is drawn that far on. */
#define UNIT_BETWEEN 256

/* Where a unit is on the screen: `*x` and `*y` get the point its
 * anchor is drawn on, raised by the ground under that point, and the
 * look it is drawn in is returned. A unit that stands is drawn on its
 * place: for one of several squares that is the middle of them, half
 * a square further right and down for each square more, and the
 * ground under that point is what raises it. One that flies is drawn
 * as high over it as its look says. One that walks is drawn on the
 * way from the square its step is from to the one it is to. What a
 * tick does of the step is drawn while the next tick is awaited,
 * `between` parts of UNIT_BETWEEN of it by now: so it glides, at one
 * speed from square to square, and is on its place as the tick after
 * its last comes.
 * Returns NULL, and leaves the two alone, for a unit that is not
 * drawn: one of a type or a dress without a look, or off the map. */
const UnitLook *unit_on_screen(const Map *map, const Unit *unit,
                               const UnitLook *looks, uint32_t between,
                               const Camera *camera, int32_t *x,
                               int32_t *y);

/* Finds the unit under a point of the screen: one whose selection box,
 * where it is drawn, holds the point. Of several it is the one that
 * stands lowest on the screen, which is in front of the others, and
 * of those at one height the one in the later slot.
 *
 * Returns false, and leaves `*out` alone, if there is none. */
bool units_pick(const Map *map, const Units *units, const UnitLook *looks,
                uint32_t between, const Camera *camera, int32_t x,
                int32_t y, UnitId *out);

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
 * it faces, and is as bright as LIGHT_STANDING. The one `hovered`
 * names, the unit the mouse is on, is as bright as LIGHT_HOVERED:
 * NULL for none.
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
 * A unit that walks shows the frames of its walk from the same rows,
 * the one for each tick it has walked, and is drawn where
 * unit_on_screen() says for `between`. It keeps the turn of the
 * square it is on: for a unit of several squares, of the last of
 * them, the low right one, so that it is drawn after what stands
 * beside it further up.
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
                     const UnitId *hovered, uint64_t tick,
                     uint32_t between, const Camera *camera, Sprite *out);

#endif
