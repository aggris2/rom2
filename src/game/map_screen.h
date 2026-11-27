/* Being in a map: the one place of the game there is so far.
 *
 * It shows the ground of a map, laid flat, and the arrow keys or W, A,
 * S and D move the camera over it.
 */
#ifndef ROM2_GAME_MAP_SCREEN_H
#define ROM2_GAME_MAP_SCREEN_H

#include <stdbool.h>

#include "game/renderer.h"

typedef struct MapScreen MapScreen;

/* Reads a map and what it takes to show it. `content` is the folder of
 * content. `name` is the number of a mission in it, or the path of a
 * map's file, which ends in ".alm". Returns NULL, with a message
 * printed, if anything of it cannot be read or put on the card. */
MapScreen *map_screen_open(Renderer *renderer, const char *content,
                           const char *name);

/* One frame: moves the camera by what the keys ask for in `seconds`,
 * and draws. Returns false, with a message printed, if drawing fails. */
bool map_screen_frame(MapScreen *screen, float seconds);

/* Gives everything back. NULL is allowed. */
void map_screen_close(MapScreen *screen);

#endif
