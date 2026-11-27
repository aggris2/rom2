/* The ground of a map as things to draw: a sprite for each square the
 * camera sees.
 *
 * This is the ground laid flat. Its heights and the light on it are
 * not shown yet.
 */
#ifndef ROM2_VIEW_GROUND_H
#define ROM2_VIEW_GROUND_H

#include <stddef.h>
#include <stdint.h>

#include "data/map.h"
#include "view/camera.h"
#include "view/sprite.h"

/* The most sprites ground_sprites() writes for a camera: room for this
 * many is enough. */
size_t ground_most(const Camera *camera);

/* Writes a sprite for each square of the map that the camera sees,
 * row after row from the top, and returns how many it wrote.
 *
 * `sheet` is the number of the sheet of tiles, which is `sheet_width`
 * by `sheet_height` pixels. A square names its tile by a column and a
 * row of that sheet: one that names a tile the sheet does not have is
 * left out. */
size_t ground_sprites(const Map *map, const Camera *camera, uint16_t sheet,
                      uint32_t sheet_width, uint32_t sheet_height,
                      Sprite *out);

#endif
