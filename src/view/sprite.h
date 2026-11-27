/* One thing to draw: a part of a sheet, at a place on the screen.
 *
 * A frame is a list of these, in the order they are to be drawn: a
 * later one covers an earlier one. The list is what the view hands to
 * the renderer, and all the two share.
 */
#ifndef ROM2_VIEW_SPRITE_H
#define ROM2_VIEW_SPRITE_H

#include <stdint.h>

/* All of it is in pixels. */
typedef struct Sprite {
    uint16_t sheet;         /* the number the renderer gave the sheet */
    uint16_t source_x;      /* the part of the sheet to take */
    uint16_t source_y;
    uint16_t width;
    uint16_t height;
    int16_t x;              /* where its top left corner goes */
    int16_t y;
} Sprite;

#endif
