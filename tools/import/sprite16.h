/* Reading the frames of the original game's .16 sprite files, which are
 * its fonts in one colour.
 *
 * A .16 file is a .256 file without a palette: sprite256_open() reads
 * it, and sprite256_frames() gives a reader over its frames. A pixel has
 * no colour, only how solid it is, so the game can draw a letter in any
 * colour.
 */
#ifndef ROM2_IMPORT_SPRITE16_H
#define ROM2_IMPORT_SPRITE16_H

#include <stdbool.h>
#include <stdint.h>

#include "base/reader.h"

/* The largest width or height a frame may have. */
#define SPRITE16_MAX_SIDE 4096u

typedef struct Frame16 {
    uint32_t width;
    uint32_t height;
    /* Owned. How solid each pixel is, row after row from the top: 0 for
     * a pixel that is not there, up to 15. */
    uint8_t *alpha;
} Frame16;

/* Reads the frame the reader stands at and moves past it.
 *
 * Returns false, and leaves `*out` alone, if the frame is cut short, is
 * larger than SPRITE16_MAX_SIDE, has a code of a kind that does not
 * exist, has codes that do not fill it exactly, or memory runs out. */
bool sprite16_frame_read(Reader *frames, Frame16 *out);

/* Frees the pixels of a frame and sets it to zero. */
void frame16_free(Frame16 *frame);

#endif
