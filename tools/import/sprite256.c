#include "import/sprite256.h"

#include <stdlib.h>
#include <string.h>

#define SPRITE256_TRAILER_SIZE 4u
#define SPRITE256_PALETTE_SIZE 1024u
#define SPRITE256_FRAME_HEADER_SIZE 12u
#define SPRITE256_COUNT_MASK 0x7fffffffu
#define SPRITE256_PALETTE_FLAG 0x80000000u

/* The kind of a code byte, held in its top two bits. */
#define SPRITE256_KIND_PIXELS 0u      /* the next k bytes are k pixels */
#define SPRITE256_KIND_SKIP_ROWS 1u   /* skip k whole rows */
#define SPRITE256_KIND_SKIP_PIXELS 2u /* skip k pixels */

/* The largest frame in the original game is 316 by 303. Anything far
 * beyond that is a damaged file, not a picture. */
#define SPRITE256_MAX_SIDE 2048u

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

bool sprite256_palette(const Sprite256 *s, Rgb out[256])
{
    if (!s->has_palette) {
        return false;
    }

    /* sprite256_open() checked that the 1024 bytes are there. Each colour
     * is stored as blue, green, red and one unused byte. */
    Reader r = reader_init(s->data, SPRITE256_PALETTE_SIZE);
    for (size_t i = 0; i < 256; i++) {
        out[i].b = reader_u8(&r);
        out[i].g = reader_u8(&r);
        out[i].r = reader_u8(&r);
        reader_skip(&r, 1);
    }
    return true;
}

Reader sprite256_frames(const Sprite256 *s)
{
    size_t start = s->has_palette ? SPRITE256_PALETTE_SIZE : 0;
    size_t end = s->size - SPRITE256_TRAILER_SIZE;
    return reader_init(s->data + start, end - start);
}

/* Decodes the codes of one frame into `pixels`, which holds `total`
 * pixels, all 0, in rows of `width`. Returns false if a code is not a
 * known one, or the codes do not fill the frame exactly. */
static bool decode_codes(Reader *codes, uint8_t *pixels, uint32_t width,
    size_t total)
{
    size_t pos = 0; /* the next pixel to fill */

    while (reader_remaining(codes) > 0) {
        uint8_t code = reader_u8(codes);
        uint8_t kind = code >> 6;     /* the top two bits */
        size_t k = code & 0x3fu;      /* the low six bits */

        /* How many pixels this code covers. */
        size_t count;
        switch (kind) {
        case SPRITE256_KIND_PIXELS:
        case SPRITE256_KIND_SKIP_PIXELS:
            count = k;
            break;
        case SPRITE256_KIND_SKIP_ROWS:
            count = k * width;
            break;
        default:
            return false;
        }
        if (count > total - pos) {
            return false;
        }

        /* Skipped pixels stay 0. Only a run of pixels is copied in. */
        if (kind == SPRITE256_KIND_PIXELS && k > 0) {
            const uint8_t *run = reader_bytes(codes, k);
            if (!reader_ok(codes)) {
                return false;
            }
            memcpy(pixels + pos, run, k);
        }
        pos += count;
    }
    return reader_ok(codes) && pos == total;
}

bool sprite256_frame_read(Reader *frames, Frame256 *out)
{
    uint32_t width = reader_u32(frames);
    uint32_t height = reader_u32(frames);
    uint32_t data_size = reader_u32(frames);
    Reader codes = reader_sub(frames, data_size);
    if (!reader_ok(frames)
        || width > SPRITE256_MAX_SIDE
        || height > SPRITE256_MAX_SIDE) {
        return false;
    }

    /* calloc() gives a block that is all zeros: every pixel starts out
     * transparent. A frame with no pixels still gets a block to free. */
    size_t total = (size_t)width * height;
    uint8_t *pixels = calloc(total > 0 ? total : 1, 1);
    if (pixels == NULL) {
        return false;
    }
    if (!decode_codes(&codes, pixels, width, total)) {
        free(pixels);
        return false;
    }

    out->width = width;
    out->height = height;
    out->pixels = pixels;
    return true;
}

void frame256_free(Frame256 *frame)
{
    free(frame->pixels);
    frame->pixels = NULL;
    frame->width = 0;
    frame->height = 0;
}
