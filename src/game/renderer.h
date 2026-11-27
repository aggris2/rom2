/* Drawing. The game says what to draw as a list of plain records, and
 * this module is the only one that talks to the graphics card.
 *
 * A sheet is a picture with a palette, put on the card once. A sprite
 * is a part of a sheet at a place on the screen. A frame is drawn by
 * handing over all its sprites at once, in the order they are to be
 * drawn: a later one covers an earlier one.
 */
#ifndef ROM2_GAME_RENDERER_H
#define ROM2_GAME_RENDERER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#include "base/png.h"

typedef struct Renderer Renderer;

/* One thing to draw: a part of a sheet, at a place on the screen. All
 * of it is in pixels. */
typedef struct Sprite {
    uint16_t sheet;         /* from renderer_add_sheet() */
    uint16_t source_x;      /* the part of the sheet to take */
    uint16_t source_y;
    uint16_t width;
    uint16_t height;
    int16_t x;              /* where its top left corner goes */
    int16_t y;
} Sprite;

/* Opens the graphics card for a window. Returns NULL, with SDL's error
 * set, if it cannot. */
Renderer *renderer_open(SDL_Window *window);

/* Closes it, with every sheet. NULL is allowed. */
void renderer_close(Renderer *renderer);

/* Puts a picture with a palette on the card and gives the number to
 * name it by in a Sprite. Returns false, with SDL's error set and
 * `*out` left alone, if the picture has no palette, there are 65,536
 * sheets already, or the card refuses it. */
bool renderer_add_sheet(Renderer *renderer, const Png *png, uint16_t *out);

/* The size of what is drawn into, in pixels: the window's, as it is
 * now. */
void renderer_size(const Renderer *renderer, int *width, int *height);

/* Draws one frame: the sprites in the order given, on black. A sprite
 * that names a sheet there is not is left out. Returns false, with
 * SDL's error set, if the card fails. */
bool renderer_draw(Renderer *renderer, const Sprite *sprites, size_t count);

#endif
