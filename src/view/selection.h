/* The units a player has selected, and how the mouse selects them: a
 * click takes one and a box drawn with the mouse takes many.
 */
#ifndef ROM2_VIEW_SELECTION_H
#define ROM2_VIEW_SELECTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/map.h"
#include "mechanics/units.h"
#include "view/camera.h"
#include "view/sprite.h"
#include "view/units.h"

/* Who is selected. `chosen` has an entry for each slot a list of
 * units can have: the age of the unit of that slot that is selected
 * plus 1, or 0 for none. A unit that is gone is therefore no longer
 * selected, and neither is the one that takes its slot. Owned. */
typedef struct Selection {
    uint32_t *chosen;
} Selection;

/* Makes a selection of nobody. Returns false if memory runs out. */
bool selection_make(Selection *selection);

/* Gives it back. One that is all zeros is allowed. */
void selection_free(Selection *selection);

/* Lets go of everyone. */
void selection_clear(Selection *selection);

/* Whether a unit is selected. `selection` may be NULL: nobody is. */
bool selection_has(const Selection *selection, UnitId id);

/* Selects a unit, or with `chosen` false lets go of it. */
void selection_put(Selection *selection, UnitId id, bool chosen);

/* How many of the units of a list are selected. */
uint32_t selection_count(const Selection *selection, const Units *units);

/* The way the mouse went with its button held, in pixels of the
 * screen: from where it was pressed to where it is now. */
typedef struct SelectionDrag {
    int32_t from_x;
    int32_t from_y;
    int32_t to_x;
    int32_t to_y;
} SelectionDrag;

/* Whether a drag is a box and not a click: the mouse went further
 * than a 64th of the screen's width across or down, as in the
 * original, which is 10 pixels on a screen 640 wide. */
bool selection_drag_is_box(const SelectionDrag *drag, const Camera *camera);

/* An overlap of this many pixels or fewer is not enough for a box to
 * take a unit, as in the original. */
#define SELECTION_LEAST_OVERLAP 10

/* Selects with a box. It catches a unit if the box and the unit's
 * selection box, where the unit is drawn, share more than
 * SELECTION_LEAST_OVERLAP pixels, and the unit is of `player`: 0
 * stands for any player.
 *
 * Without `toggle` those it catches are selected and all others let
 * go; if it catches none, nothing changes. With `toggle` each one it
 * catches is selected if it was not and let go if it was, and the
 * others stay as they are. `looks` and `between` are as
 * units_sprites() takes them.
 *
 * Returns whether it caught any. */
bool selection_box(Selection *selection, const Map *map, const Units *units,
                   const UnitLook *looks, uint32_t between,
                   const Camera *camera, const SelectionDrag *drag,
                   int32_t player, bool toggle);

/* The sheet a box is drawn from is this many pixels each way, white
 * and solid all over. */
#define SELECTION_SHEET_SIDE 256

/* The most sprites selection_outline() writes for a screen. */
size_t selection_outline_most(const Camera *camera);

/* Writes the outline of a box, a white line a pixel wide along each
 * of its sides, and returns how many sprites it wrote. A side longer
 * than the sheet is made of several. `sheet` is the sheet on the
 * card. The outline lies over the bars and under what is written. */
size_t selection_outline(uint16_t sheet, const SelectionDrag *drag,
                         const Camera *camera, Sprite *out);

#endif
