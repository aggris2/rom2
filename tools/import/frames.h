/* The frames of a sprite, decoded, and where each sits on a sheet.
 *
 * A frame is stored cut close around its figure. The registry gives the
 * size of the box the figure was drawn in, and a frame is the middle of
 * that box. A sheet shows a window of the box in every cell: the
 * smallest part of the box that holds all the frames put on the sheet.
 *
 * Building a sheet takes two passes over its frames: one to measure the
 * window, and one to put each frame into its cell.
 */
#ifndef ROM2_IMPORT_FRAMES_H
#define ROM2_IMPORT_FRAMES_H

#include <stdbool.h>
#include <stdint.h>

#include "import/sheet.h"
#include "import/sprite256.h"

/* Every frame of a sprite. */
typedef struct Frames {
    Frame256 *items;
    uint32_t count;
} Frames;

/* The part of a box that the cells show: columns `left` up to `right`
 * and rows `top` up to `bottom`. */
typedef struct FrameWindow {
    uint32_t left;
    uint32_t top;
    uint32_t right;
    uint32_t bottom;
} FrameWindow;

/* Decodes all frames of a sprite. Give them back with frames_free().
 *
 * Returns false, and leaves `*out` alone, if a frame cannot be read or
 * memory runs out. */
bool frames_read(const Sprite256 *sprite, Frames *out);

/* Frees the frames and clears the list. */
void frames_free(Frames *frames);

/* A window that holds nothing yet, for a box of the given size. */
FrameWindow frame_window_empty(uint32_t box_width, uint32_t box_height);

/* Makes the window larger, if it has to be, so that it holds frame
 * `index`. Only the left, top and bottom grow: see frame_window_close().
 * A frame of no size changes nothing.
 *
 * Returns false if there is no frame with that number, or the frame is
 * larger than the box. */
bool frames_measure(const Frames *frames, uint32_t index,
    uint32_t box_width, uint32_t box_height, FrameWindow *window);

/* Finishes a window after measuring. Its right edge is set as far from
 * the right of the box as its left edge is from the left. Frames are
 * centred, so that holds every frame, and its mirror image too.
 *
 * A window that holds nothing becomes one pixel in the middle of the
 * box. */
void frame_window_close(FrameWindow *window, uint32_t box_width,
    uint32_t box_height);

/* Puts frame `index` into the cell at `column`, `row` of a sheet whose
 * cells are the window, at the place the frame has in the box. With
 * `mirror` the frame is turned left to right about the middle of the
 * box.
 *
 * Returns false if there is no frame with that number, it is larger
 * than the box, or it does not fit the cell. */
bool frames_put(const Frames *frames, uint32_t index, bool mirror,
    uint32_t box_width, uint32_t box_height, const FrameWindow *window,
    Sheet *sheet, uint32_t column, uint32_t row);

#endif
