/* The squares of a map that are closed to one way of moving, shown: a
 * mark on each, for the ground to be drawn with. It is for looking at
 * what the game takes the ground to be, and no part of how the game
 * looks.
 */
#ifndef ROM2_VIEW_CLOSED_H
#define ROM2_VIEW_CLOSED_H

#include <stdint.h>

#include "mechanics/terrain.h"

/* How strongly a closed square is marked, in 255ths: a third. */
#define CLOSED_MARK 85

/* Writes the mark of every square into `marks`, which has room for the
 * table's width times its height: CLOSED_MARK for one closed to `whom`,
 * which is TERRAIN_NO_WALK, TERRAIN_NO_HOVER or TERRAIN_NO_FLY, and 0
 * for the others. ground_squares() takes it as it is. */
void closed_marks(const Terrain *terrain, uint8_t whom, uint8_t *marks);

#endif
