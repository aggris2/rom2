/* One thing to draw: a part of a sheet, at a place on the screen.
 *
 * A frame is a list of these, in the order they are to be drawn: a
 * later one covers an earlier one. The list is what the view hands to
 * the renderer, and all the two share.
 *
 * What stands on the map is drawn row after row from the top, and in a
 * row from the right, so that what is nearer the bottom of the screen
 * covers what is behind it. Each sprite says where it stands in that
 * order, and sprites_sort() puts a list in it.
 */
#ifndef ROM2_VIEW_SPRITE_H
#define ROM2_VIEW_SPRITE_H

#include <stddef.h>
#include <stdint.h>

/* Places and sizes are in pixels. */
typedef struct Sprite {
    uint16_t sheet;         /* the number the renderer gave the sheet */
    uint16_t source_x;      /* the part of the sheet to take */
    uint16_t source_y;
    uint16_t width;
    uint16_t height;
    int16_t x;              /* where its top left corner goes */
    int16_t y;
    uint16_t light;         /* how bright: see FRAME_LIGHT_WHOLE */
    uint16_t palette;       /* which of its sheet's palettes, from 0 */
    uint32_t order;         /* its turn: see sprite_order() */
} Sprite;

/* The turn of what lies flat on the ground, such as a bridge: before
 * everything that stands, so that it never covers a unit on it. */
#define SPRITE_ORDER_FLAT 0u

/* The turns of what is not on the map but over it: the bars of the
 * units, writing, and over that the pointer. They come after every
 * turn sprite_order() gives. */
#define SPRITE_ORDER_BARS (UINT32_MAX - 2u)
#define SPRITE_ORDER_TEXT (UINT32_MAX - 1u)
#define SPRITE_ORDER_POINTER UINT32_MAX

/* The turn of what stands on the square (`column`, `row`) of a map:
 * less for what is drawn sooner. */
uint32_t sprite_order(int32_t column, int32_t row);

/* Puts sprites in the order of their turns. Those with the same turn
 * stay as they were to each other. `spare` has room for `count`
 * sprites, and what it holds afterwards means nothing. */
void sprites_sort(Sprite *sprites, size_t count, Sprite *spare);

#endif
