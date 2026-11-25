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

#endif
