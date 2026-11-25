/* Reading the original game's .256 sprite files.
 *
 * A sprite file is an optional palette of 1024 bytes, then the frames one
 * after another, then a 4-byte trailer at the very end. The trailer holds
 * the number of frames, and its top bit says whether the palette is there.
 * All numbers are unsigned 32-bit, low byte first.
 */
#ifndef ROM2_IMPORT_SPRITE256_H
#define ROM2_IMPORT_SPRITE256_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "base/reader.h"

/* An open sprite file: its bytes and what its trailer says. Does not own
 * the bytes. */
typedef struct Sprite256 {
    const uint8_t *data;
    size_t size;
    uint32_t frame_count;
    bool has_palette;
} Sprite256;

/* Opens a .256 sprite held in memory.
 *
 * Returns false, and leaves `*out` alone, if the data is too short to have
 * a trailer, or too short to hold the palette and the number of frames the
 * trailer claims. */
bool sprite256_open(const uint8_t *data, size_t size, Sprite256 *out);

/* One decoded frame: `width * height` pixels, row after row from the top.
 * Each pixel is an index into the palette, and 0 means transparent. The
 * pixels are a block from malloc() that the frame owns. */
typedef struct Frame256 {
    uint32_t width;
    uint32_t height;
    uint8_t *pixels;
} Frame256;

/* Returns a reader over the frames of an open sprite, standing at the
 * first one. Reading `frame_count` frames from it reads the whole sprite. */
Reader sprite256_frames(const Sprite256 *s);

/* Reads and decodes the frame the reader stands at, and moves the reader
 * to the next one. On success the caller owns `out->pixels` and gives it
 * back with frame256_free().
 *
 * Returns false, and leaves `*out` alone, if the frame cannot be read, is
 * larger than any frame the game has, or its codes do not fill it
 * exactly. */
bool sprite256_frame_read(Reader *frames, Frame256 *out);

/* Frees the pixels of a frame and clears it. Safe on a cleared frame. */
void frame256_free(Frame256 *frame);

#endif
