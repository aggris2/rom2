/* What a unit sees: the squares around it that the ground does not
 * hide, by the rule of the original game.
 *
 * Sight goes out from the eye square by square, and each square it
 * crosses uses some of it up: a square's side, more on a slant, and
 * as much more as the square lies higher than the eye. Ground that
 * lies lower gives some back. So a hill hides what is behind it, and
 * from a hill a unit sees further than its sight on flat ground.
 */
#ifndef ROM2_MECHANICS_SIGHT_H
#define ROM2_MECHANICS_SIGHT_H

#include <stdint.h>

#include "mechanics/terrain.h"

/* Nothing is seen further off than this many squares, across or
 * down, however it lies. */
#define SIGHT_FURTHEST 19

/* What a square's side uses up of sight, and so what a square of
 * sight is worth: the original's ScanShift of 7, as a number. The
 * heights of a map are in the same parts, so ground this much higher
 * than the eye costs as much as a square more. */
#define SIGHT_SQUARE 128

/* Marks the squares seen from the square (x, y) with a sight of
 * `sight` squares: sets the entry of each to 1 in `seen`, which has
 * one for each square of the table, row after row, and leaves the
 * others as they are, so that what several see can be marked into
 * one. The eye's own square is always seen. Does nothing for an eye
 * that is not on the table.
 *
 * Sight starts as `sight` times SIGHT_SQUARE and half of one more.
 * The squares are gone through ring by ring outwards. Each takes
 * what is left on the square before it on the way from the eye:
 * the next one towards the eye across, down or both, whichever lies
 * nearest the straight line. From that it takes its own cost, which
 * is SIGHT_SQUARE times its distance from the eye and divided by the
 * rings it is out, cut to a whole number, and its height less the
 * eye's. It is seen if 1 or more is left. A square off the table is
 * not seen and nothing is seen through it. */
void sight_mark(const Terrain *terrain, int32_t x, int32_t y,
                uint32_t sight, uint8_t *seen);

#endif
