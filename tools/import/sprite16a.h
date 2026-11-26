/* Reading the original game's .16a sprite files.
 *
 * A .16a file is laid out as a .256 file is: a palette, the frames, and
 * a trailer with their number. It is opened with sprite256_open() and
 * its palette read with sprite256_palette(). Only the frames differ: a
 * pixel of a .16a frame has, with its colour, one of 16 levels of how
 * solid it is, which gives fire, magic and the pictures of the inventory
 * their soft look. See docs/formats/16a.md.
 */
#ifndef ROM2_IMPORT_SPRITE16A_H
#define ROM2_IMPORT_SPRITE16A_H

#include <stdbool.h>
#include <stdint.h>

#include "base/reader.h"

/* The largest width or height a frame may have. The original game's
 * largest is 640. */
#define SPRITE16A_MAX_SIDE 4096u

/* One decoded frame: `width * height` pixels, row after row from the
 * top, in two blocks from malloc() that the frame owns. */
typedef struct Frame16a {
    uint32_t width;
    uint32_t height;
    /* The colour of each pixel, an index into the palette. */
    uint8_t *pixels;
    /* How solid each pixel is: 0 for a pixel that is not there, up to 15
     * for one that hides what is behind it. */
    uint8_t *alpha;
} Frame16a;

/* Reads and decodes the frame the reader stands at, and moves the reader
 * to the next one. The reader is the one sprite256_frames() gives for
 * the opened file. On success the caller owns the frame's blocks and
 * gives them back with frame16a_free().
 *
 * Returns false, and leaves `*out` alone, if the frame cannot be read,
 * is larger than SPRITE16A_MAX_SIDE, its codes do not fill it exactly,
 * or memory runs out. */
bool sprite16a_frame_read(Reader *frames, Frame16a *out);

/* Frees the blocks of a frame and clears it. Safe on a cleared frame. */
void frame16a_free(Frame16a *frame);

#endif
