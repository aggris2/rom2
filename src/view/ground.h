/* The ground of a map as things to draw: the squares the camera sees,
 * each raised by the heights of the map and lit by the light on it.
 */
#ifndef ROM2_VIEW_GROUND_H
#define ROM2_VIEW_GROUND_H

#include <stddef.h>
#include <stdint.h>

#include "data/map.h"
#include "view/camera.h"
#include "view/frame.h"

/* The most squares ground_squares() writes for a camera: room for this
 * many is enough. */
size_t ground_most(const Camera *camera);

/* Writes each square of the map that the camera can see, row after row
 * from the top, and returns how many it wrote.
 *
 * `light` is the level of light at the top left corner of each square,
 * as light_ground() gives it. The sheet of tiles is `sheet_width` by
 * `sheet_height` pixels. A square names its tile by a column and a row
 * of that sheet: one that names a tile the sheet does not have is left
 * out.
 *
 * A corner on the right or the bottom edge of the map has no height
 * and no light of its own, and takes those of the corner beside it.
 * Ground is raised up the screen, so rows below the screen that may
 * reach up into it are written too. */
size_t ground_squares(const Map *map, const uint8_t *light,
                      const Camera *camera, uint32_t sheet_width,
                      uint32_t sheet_height, GroundSquare *out);

/* The height of the ground at a point of the map, given in pixels:
 * how far a unit or a structure that stands there is raised.
 *
 * It lies between the heights at the four corners of the point's
 * square, and is worked out as the original does it, in whole numbers:
 * first along the top and along the low edge of the square, then from
 * the one to the other, each step cut towards zero. A corner outside
 * the map counts as the nearest inside it. */
int32_t ground_height_at(const Map *map, int32_t x, int32_t y);

/* How far an object that stands on the square (`column`, `row`) is
 * raised: the mean of the heights at the square's four corners, cut
 * down to a whole number. The original does not raise a tree quite as
 * it does a unit. */
int32_t ground_square_height(const Map *map, int32_t column, int32_t row);

#endif
