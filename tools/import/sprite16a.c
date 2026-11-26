#include "import/sprite16a.h"

#include <stdlib.h>

/* A code is 16 bits: its kind in the top two, a count in the other 14. */
#define KIND_PIXELS 0u
#define KIND_SKIP_ROWS 1u
#define KIND_SKIP_PIXELS 2u

/* Decodes the codes of one frame into its two blocks, which start out
 * all zero. Returns false if the codes are cut short, are of a kind that
 * does not exist, or do not fill the frame exactly. */
static bool decode_codes(Reader *codes, uint8_t *pixels, uint8_t *alpha,
    uint32_t width, size_t total)
{
    size_t at = 0;
    while (reader_remaining(codes) > 0) {
        uint16_t code = reader_u16(codes);
        uint32_t kind = code >> 14;
        size_t count = code & 0x3fffu;
        if (!reader_ok(codes)) {
            return false;
        }
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
            if (count > total - at) {
                return false;
            }
            for (size_t i = 0; i < count; i++) {
                /* A pixel is 16 bits: bit 0 is not used, the next 8 are
                 * the colour, the 4 after those how solid it is. */
                uint16_t pixel = reader_u16(codes);
                pixels[at + i] = (uint8_t)((pixel >> 1) & 0xffu);
                alpha[at + i] = (uint8_t)((pixel >> 9) & 0x0fu);
            }
            if (!reader_ok(codes)) {
                return false;
            }
            at += count;
        } else {
            return false;
        }
    }
    return at == total;
}

bool sprite16a_frame_read(Reader *frames, Frame16a *out)
{
    uint32_t width = reader_u32(frames);
    uint32_t height = reader_u32(frames);
    uint32_t data_size = reader_u32(frames);
    Reader codes = reader_sub(frames, data_size);
    if (!reader_ok(frames)
        || width > SPRITE16A_MAX_SIDE
        || height > SPRITE16A_MAX_SIDE) {
        return false;
    }

    /* calloc() gives blocks that are all zeros: every pixel starts out
     * not there. A frame with no pixels still gets blocks to free. */
    size_t total = (size_t)width * height;
    uint8_t *pixels = calloc(total > 0 ? total : 1, 1);
    uint8_t *alpha = calloc(total > 0 ? total : 1, 1);
    if (pixels == NULL || alpha == NULL
        || !decode_codes(&codes, pixels, alpha, width, total)) {
        free(pixels);
        free(alpha);
        return false;
    }

    out->width = width;
    out->height = height;
    out->pixels = pixels;
    out->alpha = alpha;
    return true;
}

void frame16a_free(Frame16a *frame)
{
    free(frame->pixels);
    free(frame->alpha);
    frame->pixels = NULL;
    frame->alpha = NULL;
    frame->width = 0;
    frame->height = 0;
}
