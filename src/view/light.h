/* How bright the ground of a map is, from its slopes and the sun.
 *
 * The ground is lit where it faces the sun and dark where it faces
 * away. The rule is the original's, as earlier work on it gives it. It
 * is for drawing alone, and no rule of the game goes by it.
 */
#ifndef ROM2_VIEW_LIGHT_H
#define ROM2_VIEW_LIGHT_H

#include <stdint.h>

#include "data/map.h"
#include "view/frame.h"

/* The level of flat ground, and the highest there is. A level is how
 * bright the ground is drawn, in parts of its picture's own colours:
 * FRAME_LIGHT_WHOLE is as the picture, flat ground is half as bright
 * again, and the highest is three times as bright. */
#define LIGHT_FLAT 50u
#define LIGHT_MAX (3u * FRAME_LIGHT_WHOLE)

/* How dark it is, by day: the number the original calls darkness, which
 * grows towards the night. A map has its own, and the time of day
 * changes it: neither is used yet. */
#define LIGHT_DARKNESS 14

/* The level of what stands on the ground: objects, and structures. The
 * original draws these from one of 16 palettes that run from twice the
 * picture's colours down in sixteenths of that, and picks the palette
 * by a quarter of the darkness: by day the fourth of them, 13 parts of
 * 16. That is 52, a little brighter than flat ground.
 *
 * Measured on a screenshot of the original, where pieces of trees are
 * found at exactly this level, and at half of it under the fog. */
#define LIGHT_STANDING \
    ((16 - LIGHT_DARKNESS / 4) * 2 * FRAME_LIGHT_WHOLE / 16)

/* Works out the level at the top left corner of each square, row after
 * row from the top, into `levels`, which has room for the map's width
 * times its height. Each is from 2 to LIGHT_MAX. */
void light_ground(const Map *map, uint8_t *levels);

#endif
