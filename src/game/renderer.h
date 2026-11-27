/* Drawing. The game says what to draw as a list of plain records, and
 * this module is the only one that talks to the graphics card.
 *
 * A sheet is a picture with a palette, put on the card once. A frame
 * is the ground and the sprites on it, handed over all at once: see
 * view/frame.h.
 */
#ifndef ROM2_GAME_RENDERER_H
#define ROM2_GAME_RENDERER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#include "base/png.h"
#include "view/frame.h"

typedef struct Renderer Renderer;

/* Opens the graphics card for a window. Returns NULL, with SDL's error
 * set, if it cannot. */
Renderer *renderer_open(SDL_Window *window);

/* Closes it, with every sheet. NULL is allowed. */
void renderer_close(Renderer *renderer);

/* Puts a picture with a palette on the card and gives the number to
 * name it by in a frame. Returns false, with SDL's error set and
 * `*out` left alone, if the picture has no palette, there are 65,536
 * sheets already, or the card refuses it. */
bool renderer_add_sheet(Renderer *renderer, const Png *png, uint16_t *out);

/* The size of what is drawn into, in pixels: the window's, as it is
 * now. */
void renderer_size(const Renderer *renderer, int *width, int *height);

/* Draws one frame on black: the ground, then the sprites in the order
 * given. A sprite that names a sheet there is not is left out, and so
 * is the ground if its sheet is not there. Returns false, with SDL's
 * error set, if the card fails. */
bool renderer_draw(Renderer *renderer, const Frame *frame);

#endif
