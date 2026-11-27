#include "import/sprite16a.h"

#include <string.h>

#include "base/buffer.h"
#include "expect.h"
#include "utest.h"

/* A code: kind 0 for pixels, 1 to skip rows, 2 to skip pixels. */
static void put_code(ByteBuffer *b, uint32_t kind, uint32_t count)
{
    put_u16(b, (kind << 14) | count);
}

/* A pixel: its colour and how solid it is, 0 to 15. */
static void put_pixel(ByteBuffer *b, uint32_t colour, uint32_t alpha)
{
    put_u16(b, (alpha << 9) | (colour << 1));
}

/* Puts the frame's three numbers in front of the codes in `codes`. */
static void make_frame(ByteBuffer *out, uint32_t width, uint32_t height,
    const ByteBuffer *codes)
{
    buffer_u32_le(out, width);
    buffer_u32_le(out, height);
    buffer_u32_le(out, (uint32_t)codes->size);
    buffer_append(out, codes->data, codes->size);
}

UTEST(sprite16a, frame_read_gives_colour_and_solidness_of_each_pixel)
{
    /* 3 by 2: a run of three pixels, then one skipped, then two. */
    ByteBuffer codes = {0};
    ByteBuffer file = {0};
    put_code(&codes, 0, 3);
    put_pixel(&codes, 10, 15);
    put_pixel(&codes, 200, 1);
    put_pixel(&codes, 255, 8);
    put_code(&codes, 2, 1);
    put_code(&codes, 0, 2);
    put_pixel(&codes, 0, 15);
    put_pixel(&codes, 7, 3);
    make_frame(&file, 3, 2, &codes);
    Reader r = reader_init(file.data, file.size);
    Frame16a f;

    ASSERT_TRUE(sprite16a_frame_read(&r, &f));

    ASSERT_EQ(3u, f.width);
    ASSERT_EQ(2u, f.height);
    const uint8_t pixels[6] = {10, 200, 255, 0, 0, 7};
    const uint8_t alpha[6] = {15, 1, 8, 0, 15, 3};
    ASSERT_EQ(0, memcmp(pixels, f.pixels, 6));
    ASSERT_EQ(0, memcmp(alpha, f.alpha, 6));
    ASSERT_EQ(0u, reader_remaining(&r));
    frame16a_free(&f);
    ASSERT_TRUE(f.pixels == NULL);
    ASSERT_TRUE(f.alpha == NULL);
    buffer_free(&codes);
    buffer_free(&file);
}

UTEST(sprite16a, frame_read_skips_whole_rows)
{
    /* 2 by 3: a row skipped, a row of pixels, a row skipped. */
    ByteBuffer codes = {0};
    ByteBuffer file = {0};
    put_code(&codes, 1, 1);
    put_code(&codes, 0, 2);
    put_pixel(&codes, 5, 15);
    put_pixel(&codes, 6, 15);
    put_code(&codes, 1, 1);
    make_frame(&file, 2, 3, &codes);
    Reader r = reader_init(file.data, file.size);
    Frame16a f;

    ASSERT_TRUE(sprite16a_frame_read(&r, &f));

    const uint8_t pixels[6] = {0, 0, 5, 6, 0, 0};
    const uint8_t alpha[6] = {0, 0, 15, 15, 0, 0};
    ASSERT_EQ(0, memcmp(pixels, f.pixels, 6));
    ASSERT_EQ(0, memcmp(alpha, f.alpha, 6));
    frame16a_free(&f);
    buffer_free(&codes);
    buffer_free(&file);
}

UTEST(sprite16a, frame_read_takes_counts_larger_than_255)
{
    /* The count is 14 bits. 600 by 1: skip 300, one pixel, skip 299. */
    ByteBuffer codes = {0};
    ByteBuffer file = {0};
    put_code(&codes, 2, 300);
    put_code(&codes, 0, 1);
    put_pixel(&codes, 9, 15);
    put_code(&codes, 2, 299);
    make_frame(&file, 600, 1, &codes);
    Reader r = reader_init(file.data, file.size);
    Frame16a f;

    ASSERT_TRUE(sprite16a_frame_read(&r, &f));

    ASSERT_EQ(0, f.alpha[299]);
    ASSERT_EQ(9, f.pixels[300]);
    ASSERT_EQ(15, f.alpha[300]);
    ASSERT_EQ(0, f.alpha[301]);
    frame16a_free(&f);
    buffer_free(&codes);
    buffer_free(&file);
}

UTEST(sprite16a, frame_read_leaves_the_reader_at_the_next_frame)
{
    ByteBuffer codes = {0};
    ByteBuffer file = {0};
    put_code(&codes, 0, 1);
    put_pixel(&codes, 1, 15);
    make_frame(&file, 1, 1, &codes);
    codes.size = 0;
    put_code(&codes, 0, 1);
    put_pixel(&codes, 2, 14);
    make_frame(&file, 1, 1, &codes);
    Reader r = reader_init(file.data, file.size);
    Frame16a f;

    ASSERT_TRUE(sprite16a_frame_read(&r, &f));
    ASSERT_EQ(1, f.pixels[0]);
    frame16a_free(&f);
    ASSERT_TRUE(sprite16a_frame_read(&r, &f));
    ASSERT_EQ(2, f.pixels[0]);
    ASSERT_EQ(14, f.alpha[0]);
    frame16a_free(&f);
    ASSERT_EQ(0u, reader_remaining(&r));
    buffer_free(&codes);
    buffer_free(&file);
}

UTEST(sprite16a, frame_read_of_a_frame_with_no_pixels)
{
    ByteBuffer codes = {0};
    ByteBuffer file = {0};
    make_frame(&file, 0, 0, &codes);
    Reader r = reader_init(file.data, file.size);
    Frame16a f;

    ASSERT_TRUE(sprite16a_frame_read(&r, &f));
    ASSERT_EQ(0u, f.width);
    ASSERT_TRUE(f.pixels != NULL);
    frame16a_free(&f);
    buffer_free(&file);
}

/* Reads a frame of `width` by `height` with the given codes and says
 * whether that worked, having checked that a failure left `f` alone. */
static bool reads(uint32_t width, uint32_t height, const ByteBuffer *codes)
{
    ByteBuffer file = {0};
    make_frame(&file, width, height, codes);
    Reader r = reader_init(file.data, file.size);
    Frame16a f;
    memset(&f, 0, sizeof f);
    f.width = 12345;
    bool ok = sprite16a_frame_read(&r, &f);
    if (ok) {
        frame16a_free(&f);
    } else if (f.width != 12345 || f.pixels != NULL) {
        ok = true; /* a failure that touched the frame: report as wrong */
    }
    buffer_free(&file);
    return ok;
}

UTEST(sprite16a, frame_read_refuses_codes_that_do_not_fill_the_frame)
{
    ByteBuffer codes = {0};
    put_code(&codes, 2, 3);
    ASSERT_FALSE(reads(2, 2, &codes));  /* one pixel short */
    ASSERT_TRUE(reads(3, 1, &codes));
    put_code(&codes, 2, 2);
    ASSERT_FALSE(reads(2, 2, &codes));  /* one too many */
    buffer_free(&codes);
}

UTEST(sprite16a, frame_read_refuses_a_run_or_a_skip_past_the_end)
{
    ByteBuffer codes = {0};
    put_code(&codes, 0, 3);
    put_pixel(&codes, 1, 15);
    put_pixel(&codes, 1, 15);
    put_pixel(&codes, 1, 15);
    ASSERT_FALSE(reads(2, 1, &codes));
    codes.size = 0;
    put_code(&codes, 1, 3);
    ASSERT_FALSE(reads(2, 2, &codes));
    codes.size = 0;
    /* The largest count there is, in a frame of one pixel. */
    put_code(&codes, 1, 0x3fff);
    ASSERT_FALSE(reads(1, 1, &codes));
    codes.size = 0;
    /* Rows skipped past the end, and then a pixel that would land
     * outside the frame. */
    put_code(&codes, 1, 3);
    put_code(&codes, 0, 1);
    put_pixel(&codes, 1, 15);
    ASSERT_FALSE(reads(2, 2, &codes));
    codes.size = 0;
    /* A run that fits the frame but not what is left of it. */
    put_code(&codes, 2, 1);
    put_code(&codes, 0, 2);
    put_pixel(&codes, 1, 15);
    put_pixel(&codes, 1, 15);
    ASSERT_FALSE(reads(2, 1, &codes));
    buffer_free(&codes);
}

UTEST(sprite16a, frame_read_refuses_codes_cut_short)
{
    ByteBuffer codes = {0};
    put_code(&codes, 0, 2);
    put_pixel(&codes, 1, 15);
    ASSERT_FALSE(reads(2, 1, &codes));  /* the second pixel is missing */
    buffer_push(&codes, 0);
    ASSERT_FALSE(reads(2, 1, &codes));  /* half a pixel */
    codes.size = 0;
    buffer_push(&codes, 0);
    ASSERT_FALSE(reads(0, 0, &codes));  /* half a code */
    buffer_free(&codes);
}

UTEST(sprite16a, frame_read_refuses_the_kind_that_does_not_exist)
{
    ByteBuffer codes = {0};
    put_code(&codes, 3, 1);
    ASSERT_FALSE(reads(1, 1, &codes));
    /* Not even with a count of 0 after codes that fill the frame. */
    codes.size = 0;
    put_code(&codes, 2, 1);
    put_code(&codes, 3, 0);
    ASSERT_FALSE(reads(1, 1, &codes));
    buffer_free(&codes);
}

UTEST(sprite16a, frame_read_refuses_a_frame_cut_short_or_too_large)
{
    ByteBuffer codes = {0};
    ByteBuffer file = {0};
    put_code(&codes, 2, 1);
    make_frame(&file, 1, 1, &codes);
    Frame16a f;
    memset(&f, 0, sizeof f);
    for (size_t size = 0; size < file.size; size++) {
        Reader r = reader_init(file.data, size);
        ASSERT_FALSE(sprite16a_frame_read(&r, &f));
    }
    ASSERT_TRUE(f.pixels == NULL);
    buffer_free(&file);

    codes.size = 0;
    ASSERT_FALSE(reads(SPRITE16A_MAX_SIDE + 1, 0, &codes));
    ASSERT_FALSE(reads(0, SPRITE16A_MAX_SIDE + 1, &codes));
    buffer_free(&codes);
}
