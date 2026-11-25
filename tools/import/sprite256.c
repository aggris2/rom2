#include "import/sprite256.h"

#include "base/reader.h"

#define SPRITE256_TRAILER_SIZE 4u
#define SPRITE256_PALETTE_SIZE 1024u
#define SPRITE256_FRAME_HEADER_SIZE 12u
#define SPRITE256_COUNT_MASK 0x7fffffffu
#define SPRITE256_PALETTE_FLAG 0x80000000u

bool sprite256_open(const uint8_t *data, size_t size, Sprite256 *out)
{
    if (data == NULL || size < SPRITE256_TRAILER_SIZE) {
        return false;
    }

    /* The trailer is the last 4 bytes. */
    size_t body_size = size - SPRITE256_TRAILER_SIZE;
    Reader r = reader_init(data + body_size, SPRITE256_TRAILER_SIZE);
    uint32_t trailer = reader_u32(&r);
    if (!reader_ok(&r)) {
        return false;
    }
    uint32_t frame_count = trailer & SPRITE256_COUNT_MASK;
    bool has_palette = (trailer & SPRITE256_PALETTE_FLAG) != 0;

    /* What is left for the frames once the palette is taken off. */
    size_t frames_size = body_size;
    if (has_palette) {
        if (frames_size < SPRITE256_PALETTE_SIZE) {
            return false;
        }
        frames_size -= SPRITE256_PALETTE_SIZE;
    }

    /* Every frame starts with three numbers, so there cannot be more
     * frames than that leaves room for. */
    if (frame_count > frames_size / SPRITE256_FRAME_HEADER_SIZE) {
        return false;
    }

    out->data = data;
    out->size = size;
    out->frame_count = frame_count;
    out->has_palette = has_palette;
    return true;
}
