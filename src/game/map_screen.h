/* Being in a map: the one place of the game there is so far.
 *
 * It shows the ground of a map, with its hills and the light on them,
 * and what stands on it. The mouse on the edge of the screen moves the
 * camera over it. G shows and hides a grid of its squares, P pauses
 * the game and lets it go on, and F shows and hides how many frames a
 * second are drawn.
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

/* A button of the mouse was pressed down, or with `down` false let
 * go, where the mouse is now. `button` is SDL's number for it.
 *
 * The left one, pressed and let go in one place, is a click: on a
 * unit it selects that unit alone, and on the ground it sends the
 * selected units of the player there. While units of the player are
 * selected, a click on an enemy does not select it. Held while the
 * mouse moves it draws a box, which selects the player's units in it
 * when it is let go, and changes nothing if there are none. With
 * Shift a click or a box selects the player's units besides those
 * there are, or lets go of them. The right one lets go of all. */
void map_screen_button(MapScreen *screen, Uint8 button, bool down);

/* Gives everything back. NULL is allowed. */
void map_screen_close(MapScreen *screen);

#endif
