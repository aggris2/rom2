/* Building a .256 sprite file by hand, for tests.
 *
 * Tests never read a file of the original game, so a test that needs a
 * sprite writes one: frames one after the other and a trailer, without a
 * palette.
 */
#ifndef ROM2_TESTS_IMPORT_MAKE_SPRITE_H
#define ROM2_TESTS_IMPORT_MAKE_SPRITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "import/sprite256.h"

/* A sprite file built by hand, without a palette: frames one after the
 * other and a trailer. */
typedef struct MakeSprite {
    uint8_t bytes[8192];
    size_t size;
    uint32_t frames;
} MakeSprite;

static inline void put_u32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)(value & 0xff);
    at[1] = (uint8_t)((value >> 8) & 0xff);
    at[2] = (uint8_t)((value >> 16) & 0xff);
    at[3] = (uint8_t)((value >> 24) & 0xff);
}

static inline void make_sprite_start(MakeSprite *m)
{
    memset(m, 0, sizeof *m);
}

/* Adds a frame of `width` by `height` pixels. Each row is written as one
 * run of pixels, so `width` is at most 63. */
static inline void make_sprite_frame(MakeSprite *m, uint32_t width,
    uint32_t height, const uint8_t *pixels)
{
    uint8_t *header = m->bytes + m->size;
    m->size += 12;
    size_t codes_start = m->size;
    for (uint32_t y = 0; y < height; y++) {
        m->bytes[m->size++] = (uint8_t)width;
        memcpy(m->bytes + m->size, pixels + (size_t)y * width, width);
        m->size += width;
    }
    put_u32(header, width);
    put_u32(header + 4, height);
    put_u32(header + 8, (uint32_t)(m->size - codes_start));
    m->frames++;
}

/* Adds a frame filled with one value. It has room for 64 pixels, so
 * `width` times `height` is at most that. */
static inline void make_sprite_solid(MakeSprite *m, uint32_t width,
    uint32_t height, uint8_t value)
{
    uint8_t pixels[64];
    memset(pixels, value, sizeof pixels);
    make_sprite_frame(m, width, height, pixels);
}

static inline bool make_sprite_finish(MakeSprite *m, Sprite256 *out)
{
    put_u32(m->bytes + m->size, m->frames);
    m->size += 4;
    return sprite256_open(m->bytes, m->size, out);
}

#endif
