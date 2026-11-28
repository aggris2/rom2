/* Marks on the squares of a map, for the ground to be drawn with: a
 * tint over a square that says what the game takes it to be. They
 * are for looking at the game's own tables, and no part of how the
 * game looks.
 */
#ifndef ROM2_VIEW_MARKS_H
#define ROM2_VIEW_MARKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mechanics/terrain.h"
#include "mechanics/units.h"

/* Writes the mark of every square into `marks`, which has room for the
 * table's width times its height: FRAME_MARK_CLOSED for one closed to
 * `whom`, which is TERRAIN_NO_WALK, TERRAIN_NO_HOVER or
 * TERRAIN_NO_FLY, and FRAME_MARK_NONE for the others.
 * ground_squares() takes it as it is. */
void marks_closed(const Terrain *terrain, uint8_t whom, uint8_t *marks);

/* Marks the squares a unit holds with FRAME_MARK_HELD, those it is
 * on and those it is reaching for, or with `held` false gives them
 * back the marks they have in `under`, which is a list as `marks`
 * is. Does nothing for a unit that is gone or a
 * list of units on no map. */
void marks_held(const Units *units, UnitId id, bool held,
                const uint8_t *under, uint8_t *marks);

/* Marks the squares that are seen with FRAME_MARK_SEEN: those of the
 * first `squares` whose entry in `seen` is not 0, as
 * mechanics/sight.h fills it. One that has a mark already keeps
 * it. */
void marks_seen(const uint8_t *seen, size_t squares, uint8_t *marks);

#endif
