#include "import/bmp.h"

#include <string.h>

#include "base/buffer.h"
#include "utest.h"

static void put_u16(ByteBuffer *b, uint32_t value)
{
    buffer_push(b, (uint8_t)(value & 0xff));
    buffer_push(b, (uint8_t)((value >> 8) & 0xff));
}

/* Builds an 8-bit BMP of `width` by `height` with a table of `colours`
 * colours, where colour n is blue n, green n + 1, red n + 2. The pixel
 * in column x of the row stored as number `stored` is stored * 10 + x. */
static void make_bmp(ByteBuffer *b, uint32_t width, uint32_t height,
    uint32_t colours)
{
    uint32_t stride = (width + 3) / 4 * 4;
    uint32_t pixels_at = 14 + 40 + colours * 4;
    b->size = 0;
    buffer_push(b, 'B');
    buffer_push(b, 'M');
    buffer_u32_le(b, pixels_at + stride * height);
    buffer_u32_le(b, 0);
    buffer_u32_le(b, pixels_at);
    buffer_u32_le(b, 40);
    buffer_u32_le(b, width);
    buffer_u32_le(b, height);
    put_u16(b, 1);
    put_u16(b, 8);
    buffer_u32_le(b, 0);            /* not compressed */
    buffer_u32_le(b, 0);
    buffer_u32_le(b, 0);
    buffer_u32_le(b, 0);
    buffer_u32_le(b, colours == 256 ? 0 : colours);
    buffer_u32_le(b, 0);
    for (uint32_t i = 0; i < colours; i++) {
        buffer_push(b, (uint8_t)i);
        buffer_push(b, (uint8_t)(i + 1));
        buffer_push(b, (uint8_t)(i + 2));
        buffer_push(b, 0);
    }
    for (uint32_t stored = 0; stored < height; stored++) {
        for (uint32_t x = 0; x < stride; x++) {
            buffer_push(b, x < width ? (uint8_t)(stored * 10 + x) : 0xee);
        }
    }
}

UTEST(bmp, read_gives_the_rows_from_the_top)
{
    /* 3 by 2. The file has the bottom row first: 0 1 2, then 10 11 12,
     * each with one byte to fill it out to 4. */
    ByteBuffer b = {0};
    Bmp8 bmp;
    make_bmp(&b, 3, 2, 256);

    ASSERT_TRUE(bmp8_read(b.data, b.size, &bmp));

    ASSERT_EQ(3u, bmp.width);
    ASSERT_EQ(2u, bmp.height);
    const uint8_t expected[6] = {10, 11, 12, 0, 1, 2};
    ASSERT_EQ(0, memcmp(expected, bmp.pixels, 6));
    bmp8_free(&bmp);
    ASSERT_TRUE(bmp.pixels == NULL);
    buffer_free(&b);
}

UTEST(bmp, read_gives_the_colours_red_first)
{
    ByteBuffer b = {0};
    Bmp8 bmp;
    make_bmp(&b, 4, 1, 256);

    ASSERT_TRUE(bmp8_read(b.data, b.size, &bmp));

    ASSERT_EQ(5, bmp.palette[5].b);
    ASSERT_EQ(6, bmp.palette[5].g);
    ASSERT_EQ(7, bmp.palette[5].r);
    ASSERT_EQ(255, bmp.palette[255].b);
    bmp8_free(&bmp);
    buffer_free(&b);
}

UTEST(bmp, read_of_a_table_with_fewer_colours_leaves_the_rest_black)
{
    ByteBuffer b = {0};
    Bmp8 bmp;
    make_bmp(&b, 4, 1, 16);

    ASSERT_TRUE(bmp8_read(b.data, b.size, &bmp));

    ASSERT_EQ(15, bmp.palette[15].b);
    ASSERT_EQ(0, bmp.palette[16].b);
    ASSERT_EQ(0, bmp.palette[16].g);
    ASSERT_EQ(0, bmp.palette[255].r);
    /* The pixels are found after the shorter table. */
    ASSERT_EQ(0, bmp.pixels[0]);
    ASSERT_EQ(3, bmp.pixels[3]);
    bmp8_free(&bmp);
    buffer_free(&b);
}

UTEST(bmp, read_of_a_width_that_is_a_multiple_of_4_has_no_filling)
{
    ByteBuffer b = {0};
    Bmp8 bmp;
    make_bmp(&b, 4, 3, 256);

    ASSERT_TRUE(bmp8_read(b.data, b.size, &bmp));

    ASSERT_EQ(20, bmp.pixels[0]);
    ASSERT_EQ(23, bmp.pixels[3]);
    ASSERT_EQ(10, bmp.pixels[4]);
    ASSERT_EQ(3, bmp.pixels[11]);
    bmp8_free(&bmp);
    buffer_free(&b);
}

UTEST(bmp, read_refuses_a_file_cut_short_anywhere)
{
    ByteBuffer b = {0};
    Bmp8 bmp;
    memset(&bmp, 0, sizeof bmp);
    bmp.width = 12345;
    make_bmp(&b, 3, 2, 256);

    for (size_t size = 0; size < b.size; size++) {
        ASSERT_FALSE(bmp8_read(b.data, size, &bmp));
    }
    ASSERT_EQ(12345u, bmp.width);
    ASSERT_TRUE(bmp.pixels == NULL);
    buffer_free(&b);
}

UTEST(bmp, read_refuses_what_is_not_this_kind_of_bmp)
{
    ByteBuffer b = {0};
    Bmp8 bmp;
    memset(&bmp, 0, sizeof bmp);

    make_bmp(&b, 3, 2, 256);
    b.data[0] = 'X';
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    make_bmp(&b, 3, 2, 256);
    b.data[28] = 24;                /* 24 bits for each pixel */
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    make_bmp(&b, 3, 2, 256);
    b.data[30] = 1;                 /* compressed */
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    make_bmp(&b, 3, 2, 256);
    b.data[14] = 12;                /* the old, short picture header */
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    make_bmp(&b, 3, 2, 256);
    b.data[47] = 2;                 /* 513 colours */
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    ASSERT_TRUE(bmp.pixels == NULL);
    buffer_free(&b);
}

UTEST(bmp, read_refuses_a_size_of_none_negative_or_too_large)
{
    ByteBuffer b = {0};
    Bmp8 bmp;
    memset(&bmp, 0, sizeof bmp);

    make_bmp(&b, 3, 2, 256);
    memset(b.data + 18, 0, 4);      /* width 0 */
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    make_bmp(&b, 3, 2, 256);
    memset(b.data + 22, 0, 4);      /* height 0 */
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    make_bmp(&b, 3, 2, 256);
    memset(b.data + 22, 0xff, 4);   /* height -1: stored from the top */
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    make_bmp(&b, 3, 2, 256);
    b.data[19] = 0x41;              /* width 16,643 */
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    make_bmp(&b, 3, 2, 256);
    memset(b.data + 22, 0x7f, 4);   /* a height no file could hold */
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    ASSERT_TRUE(bmp.pixels == NULL);
    buffer_free(&b);
}

UTEST(bmp, read_refuses_pixels_that_start_past_the_end)
{
    ByteBuffer b = {0};
    Bmp8 bmp;
    memset(&bmp, 0, sizeof bmp);
    make_bmp(&b, 3, 2, 256);
    /* Where the pixels start: one byte too far for both rows to fit,
     * and far outside the file. */
    b.data[10] = (uint8_t)(b.data[10] + 1);
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    memset(b.data + 10, 0xff, 4);
    ASSERT_FALSE(bmp8_read(b.data, b.size, &bmp));
    ASSERT_TRUE(bmp.pixels == NULL);
    buffer_free(&b);
}
