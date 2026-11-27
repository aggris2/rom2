#include "import/sprite16.h"

#include <stdlib.h>
#include <string.h>

/* A code is one byte: its kind in the top two bits, a count in the
 * other six. */
#define KIND_PIXELS 0u
#define KIND_SKIP_ROWS 1u
#define KIND_SKIP_PIXELS 2u

/* Decodes the codes of one frame into its block, which starts out all
 * zero. Returns false if the codes are cut short, are of a kind that
 * does not exist, or do not fill the frame exactly. */
static bool decode_codes(Reader *codes, uint8_t *alpha, uint32_t width,
    size_t total)
{
    size_t at = 0;
    while (reader_remaining(codes) > 0) {
        uint8_t code = reader_u8(codes);
        uint32_t kind = code >> 6;
        size_t count = code & 0x3fu;
        if (kind == KIND_SKIP_ROWS) {
            /* Not `count * width > total - at`: the left side could be
             * too large for the number. */
            if (width != 0 && count > (total - at) / width) {
                return false;
            }
            at += count * width;
        } else if (kind == KIND_SKIP_PIXELS) {
            if (count > total - at) {
                return false;
            }
            at += count;
        } else if (kind == KIND_PIXELS) {
            /* `count` bytes follow, each two pixels: the lower half of
             * the byte, then the upper. A run of an odd number of
             * pixels has nothing for the upper half of its last byte,
             * and leaves it 0. No run ends in a pixel that is not
             * there, so a 0 in that place is never a pixel. */
            const uint8_t *bytes = reader_bytes(codes, count);
            if (bytes == NULL) {
                return false;
            }
            for (size_t i = 0; i < count; i++) {
                uint8_t low = bytes[i] & 0x0fu;
                uint8_t high = bytes[i] >> 4;
                bool one = i == count - 1 && high == 0;
                if ((one ? 1u : 2u) > total - at) {
                    return false;
                }
                alpha[at++] = low;
                if (!one) {
                    alpha[at++] = high;
                }
            }
        } else {
            return false;
        }
    }
    return at == total;
}

bool sprite16_frame_read(Reader *frames, Frame16 *out)
{
    uint32_t width = reader_u32(frames);
    uint32_t height = reader_u32(frames);
    uint32_t data_size = reader_u32(frames);
    Reader codes = reader_sub(frames, data_size);
    if (!reader_ok(frames)
        || width > SPRITE16_MAX_SIDE
        || height > SPRITE16_MAX_SIDE) {
        return false;
    }

    /* calloc() gives a block that is all zeros: every pixel starts out
     * not there. A frame with no pixels still gets a block to free. */
    size_t total = (size_t)width * height;
    uint8_t *alpha = calloc(total > 0 ? total : 1, 1);
    if (alpha == NULL || !decode_codes(&codes, alpha, width, total)) {
        free(alpha);
        return false;
    }

    out->width = width;
    out->height = height;
    out->alpha = alpha;
    return true;
}

void frame16_free(Frame16 *frame)
{
    free(frame->alpha);
    memset(frame, 0, sizeof *frame);
}
