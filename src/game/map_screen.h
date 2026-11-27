/* Being in a map: the one place of the game there is so far.
 *
 * It shows the ground of a map, with its hills and the light on them,
 * and what stands on it. The arrow keys or W, A, S and D move the
 * camera over it, G shows and hides a grid of its squares, and P
 * pauses the game and lets it go on.
 */
#ifndef ROM2_GAME_MAP_SCREEN_H
#define ROM2_GAME_MAP_SCREEN_H

#include <stdbool.h>

#include <SDL3/SDL.h>

#include "game/renderer.h"

typedef struct MapScreen MapScreen;

/* Reads a map and what it takes to show it. `content` is the folder of
 * content. `name` is the number of a mission in it, or the path of a
 * map's file, which ends in ".alm". Returns NULL, with a message
 * printed, if anything of it cannot be read or put on the card. */
MapScreen *map_screen_open(Renderer *renderer, const char *content,
                           const char *name);

/* One frame: lets `seconds` of the game pass, moves the camera by what
 * the keys ask for in that time, and draws. Returns false, with a
 * message printed, if drawing fails. */
bool map_screen_frame(MapScreen *screen, float seconds);

/* A key was pressed down. */
void map_screen_key(MapScreen *screen, SDL_Keycode key);

/* Gives everything back. NULL is allowed. */
void map_screen_close(MapScreen *screen);

#endif
