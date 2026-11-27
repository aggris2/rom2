#include "import/sprite16.h"

#include <string.h>

#include "base/buffer.h"
#include "utest.h"

/* The codes of a .16 frame. */
#define PIXELS(n) ((uint8_t)(n))
#define SKIP_ROWS(n) ((uint8_t)(0x40 | (n)))
#define SKIP_PIXELS(n) ((uint8_t)(0x80 | (n)))

/* Builds a frame of `width` by `height` with the `size` bytes of `codes`
 * and gives a reader over it. */
static Reader make_frame(ByteBuffer *b, uint32_t width, uint32_t height,
    const uint8_t *codes, uint32_t size)
{
    b->size = 0;
    buffer_u32_le(b, width);
    buffer_u32_le(b, height);
    buffer_u32_le(b, size);
    buffer_append(b, codes, size);
    return reader_init(b->data, b->size);
}

/* A frame that has not been written to. */
static Frame16 untouched(void)
{
    Frame16 f = {77, 88, (uint8_t *)1};
    return f;
}

static bool is_untouched(const Frame16 *f)
{
    return f->width == 77 && f->height == 88 && f->alpha == (uint8_t *)1;
}

UTEST(sprite16, a_byte_is_two_pixels_the_lower_half_first)
{
    /* 4 by 1: the bytes 0x8f and 0xb4 are the pixels 15, 8, 4, 11. */
    ByteBuffer b = {0};
    const uint8_t codes[] = {PIXELS(2), 0x8f, 0xb4};
    Reader r = make_frame(&b, 4, 1, codes, sizeof codes);
    Frame16 f;

    ASSERT_TRUE(sprite16_frame_read(&r, &f));

    ASSERT_EQ(4u, f.width);
    ASSERT_EQ(1u, f.height);
    ASSERT_EQ(15, f.alpha[0]);
    ASSERT_EQ(8, f.alpha[1]);
    ASSERT_EQ(4, f.alpha[2]);
    ASSERT_EQ(11, f.alpha[3]);
    ASSERT_EQ(0u, (unsigned)reader_remaining(&r));
    frame16_free(&f);
    ASSERT_TRUE(f.alpha == NULL);
    buffer_free(&b);
}

UTEST(sprite16, the_last_byte_of_a_run_with_an_empty_upper_half_is_one_pixel)
{
    /* 3 by 1: 0xfb is 11 and 15, and 0x0d is 13 alone. */
    ByteBuffer b = {0};
    const uint8_t codes[] = {PIXELS(2), 0xfb, 0x0d};
    Reader r = make_frame(&b, 3, 1, codes, sizeof codes);
    Frame16 f;

    ASSERT_TRUE(sprite16_frame_read(&r, &f));

    ASSERT_EQ(11, f.alpha[0]);
    ASSERT_EQ(15, f.alpha[1]);
    ASSERT_EQ(13, f.alpha[2]);
    frame16_free(&f);
    buffer_free(&b);
}

UTEST(sprite16, an_empty_upper_half_inside_a_run_is_a_pixel)
{
    /* 4 by 1: only the last byte of a run may hold one pixel. 0x0f in
     * the first place is 15 and then a pixel of 0. */
    ByteBuffer b = {0};
    const uint8_t codes[] = {PIXELS(2), 0x0f, 0x21};
    Reader r = make_frame(&b, 4, 1, codes, sizeof codes);
    Frame16 f;

    ASSERT_TRUE(sprite16_frame_read(&r, &f));

    ASSERT_EQ(15, f.alpha[0]);
    ASSERT_EQ(0, f.alpha[1]);
    ASSERT_EQ(1, f.alpha[2]);
    ASSERT_EQ(2, f.alpha[3]);
    frame16_free(&f);
    buffer_free(&b);
}

UTEST(sprite16, skips_leave_pixels_that_are_not_there)
{
    /* 4 by 3, as a letter is written: a row skipped whole, then one
     * pixel skipped, two there, one skipped, and a last row skipped. */
    ByteBuffer b = {0};
    const uint8_t codes[] = {
        SKIP_ROWS(1), SKIP_PIXELS(1), PIXELS(1), 0x8f, SKIP_PIXELS(1),
        SKIP_ROWS(1)
    };
    Reader r = make_frame(&b, 4, 3, codes, sizeof codes);
    Frame16 f;
    const uint8_t expected[12] = {0, 0, 0, 0, 0, 15, 8, 0, 0, 0, 0, 0};

    ASSERT_TRUE(sprite16_frame_read(&r, &f));

    ASSERT_EQ(0, memcmp(f.alpha, expected, sizeof expected));
    frame16_free(&f);
    buffer_free(&b);
}

UTEST(sprite16, a_skip_of_pixels_runs_on_into_the_next_row)
{
    /* 2 by 2: three skipped, then one pixel in the last place. */
    ByteBuffer b = {0};
    const uint8_t codes[] = {SKIP_PIXELS(3), PIXELS(1), 0x09};
    Reader r = make_frame(&b, 2, 2, codes, sizeof codes);
    Frame16 f;

    ASSERT_TRUE(sprite16_frame_read(&r, &f));

    ASSERT_EQ(0, f.alpha[2]);
    ASSERT_EQ(9, f.alpha[3]);
    frame16_free(&f);
    buffer_free(&b);
}

UTEST(sprite16, read_moves_to_the_next_frame)
{
    ByteBuffer b = {0};
    const uint8_t codes[] = {PIXELS(1), 0x21};
    Frame16 f;
    for (int i = 0; i < 2; i++) {
        buffer_u32_le(&b, 2);
        buffer_u32_le(&b, 1);
        buffer_u32_le(&b, sizeof codes);
        buffer_append(&b, codes, sizeof codes);
    }
    b.data[b.size - 1] = 0x43;
    Reader r = reader_init(b.data, b.size);

    ASSERT_TRUE(sprite16_frame_read(&r, &f));
    ASSERT_EQ(1, f.alpha[0]);
    frame16_free(&f);
    ASSERT_TRUE(sprite16_frame_read(&r, &f));
    ASSERT_EQ(3, f.alpha[0]);
    ASSERT_EQ(4, f.alpha[1]);
    frame16_free(&f);
    ASSERT_EQ(0u, (unsigned)reader_remaining(&r));
    buffer_free(&b);
}

UTEST(sprite16, a_frame_without_pixels_is_read)
{
    ByteBuffer b = {0};
    Reader r = make_frame(&b, 0, 0, NULL, 0);
    Frame16 f = untouched();

    ASSERT_TRUE(sprite16_frame_read(&r, &f));

    ASSERT_EQ(0u, f.width);
    ASSERT_TRUE(f.alpha != NULL);
    frame16_free(&f);
    buffer_free(&b);
}

UTEST(sprite16, codes_that_do_not_fill_the_frame_exactly_fail)
{
    ByteBuffer b = {0};
    Frame16 f = untouched();
    /* 4 by 1 with three pixels, and with five. */
    const uint8_t few[] = {PIXELS(2), 0x11, 0x01};
    const uint8_t many[] = {PIXELS(3), 0x11, 0x11, 0x01};
    /* A skip of rows, and one of pixels, past the end. */
    const uint8_t rows[] = {SKIP_ROWS(2)};
    const uint8_t skip[] = {SKIP_PIXELS(5)};
    /* The last pixel there is room for, and then a byte of two. */
    const uint8_t two[] = {SKIP_PIXELS(3), PIXELS(1), 0x11};

    Reader r = make_frame(&b, 4, 1, few, sizeof few);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    r = make_frame(&b, 4, 1, many, sizeof many);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    r = make_frame(&b, 4, 1, rows, sizeof rows);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    r = make_frame(&b, 4, 1, skip, sizeof skip);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    r = make_frame(&b, 4, 1, two, sizeof two);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    ASSERT_TRUE(is_untouched(&f));
    buffer_free(&b);
}

UTEST(sprite16, pixels_after_a_skip_past_the_end_are_not_written)
{
    /* A skip that goes past the end must stop the reading there: the
     * pixels after it would land outside the frame. */
    ByteBuffer b = {0};
    Frame16 f = untouched();
    const uint8_t after_pixels[] = {SKIP_PIXELS(5), PIXELS(1), 0x11};
    const uint8_t after_rows[] = {SKIP_ROWS(2), PIXELS(1), 0x11};

    Reader r = make_frame(&b, 4, 1, after_pixels, sizeof after_pixels);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    r = make_frame(&b, 4, 1, after_rows, sizeof after_rows);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    ASSERT_TRUE(is_untouched(&f));
    buffer_free(&b);
}

UTEST(sprite16, a_code_of_the_fourth_kind_fails)
{
    ByteBuffer b = {0};
    const uint8_t codes[] = {0xc4};
    Reader r = make_frame(&b, 4, 1, codes, sizeof codes);
    Frame16 f = untouched();

    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    ASSERT_TRUE(is_untouched(&f));
    buffer_free(&b);
}

UTEST(sprite16, a_frame_cut_short_fails_at_every_length)
{
    ByteBuffer b = {0};
    const uint8_t codes[] = {PIXELS(2), 0x8f, 0xb4};
    Frame16 f = untouched();
    make_frame(&b, 4, 1, codes, sizeof codes);

    for (size_t size = 0; size < b.size; size++) {
        Reader r = reader_init(b.data, size);
        ASSERT_FALSE(sprite16_frame_read(&r, &f));
    }
    /* A run that says two bytes and has one. */
    const uint8_t run[] = {PIXELS(2), 0x8f};
    Reader r = make_frame(&b, 4, 1, run, sizeof run);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    ASSERT_TRUE(is_untouched(&f));
    buffer_free(&b);
}

UTEST(sprite16, a_frame_larger_than_the_most_fails)
{
    ByteBuffer b = {0};
    const uint8_t rows[] = {SKIP_ROWS(1)};
    Frame16 f = untouched();

    Reader r = make_frame(&b, SPRITE16_MAX_SIDE + 1, 1, rows, 1);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    r = make_frame(&b, 1, SPRITE16_MAX_SIDE + 1, rows, 1);
    ASSERT_FALSE(sprite16_frame_read(&r, &f));
    ASSERT_TRUE(is_untouched(&f));

    /* The most itself is read. */
    r = make_frame(&b, SPRITE16_MAX_SIDE, 1, rows, 1);
    ASSERT_TRUE(sprite16_frame_read(&r, &f));
    frame16_free(&f);
    buffer_free(&b);
}
