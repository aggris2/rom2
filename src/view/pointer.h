/* The pointer, the cursor the mouse moves: which of its pictures it
 * shows, where, and which way the map scrolls under it.
 *
 * On the edge of the screen the pointer scrolls the map that way, and
 * shows an arrow that points there. Anywhere else it is the plain
 * one.
 */
#ifndef ROM2_VIEW_POINTER_H
#define ROM2_VIEW_POINTER_H

#include <stdbool.h>
#include <stdint.h>

#include "view/sprite.h"

/* The pictures a pointer has: the eight arrows of scrolling, from the
 * one that points up round by the right, and the plain one. */
typedef enum PointerKind {
    POINTER_UP,
    POINTER_UP_RIGHT,
    POINTER_RIGHT,
    POINTER_DOWN_RIGHT,
    POINTER_DOWN,
    POINTER_DOWN_LEFT,
    POINTER_LEFT,
    POINTER_UP_LEFT,
    POINTER_PLAIN,
    POINTER_KINDS
} PointerKind;

/* How one of them looks: its sheet on the card, the size of a frame,
 * and the pixel of a frame that points. One whose picture could not be
 * loaded is not `known`, and is not drawn. */
typedef struct PointerLook {
    bool known;
    uint16_t sheet;
    uint16_t width;
    uint16_t height;
    uint16_t tip_x;
    uint16_t tip_y;
} PointerLook;

/* Which way the map scrolls with the mouse at (`x`, `y`) of a screen
 * `width` by `height` pixels: -1, 0 or 1 across into `*across`, and
 * down into `*down`. It scrolls while the mouse is on the outermost
 * pixels of the screen, or past them. */
void pointer_scroll(int32_t x, int32_t y, int32_t width, int32_t height,
                    int32_t *across, int32_t *down);

/* The picture for scrolling a way: the arrow that points there, or the
 * plain one for no way at all. */
PointerKind pointer_kind(int32_t across, int32_t down);

/* The sprite of a pointer with its tip at (`x`, `y`) of the screen. It
 * is as its picture, neither lit nor dark, and has the last turn of
 * all, so that it lies over everything. */
Sprite pointer_sprite(const PointerLook *look, int32_t x, int32_t y);

#endif
