#include "import/icon_strip.h"

#include <string.h>

#include "base/buffer.h"
#include "utest.h"

/* A .16a file built by hand: a palette in which colour n is
 * (n + shade, n, 0), one frame of 2 by 1, and the trailer. */
static void put_u16(ByteBuffer *b, uint32_t value)
{
    buffer_push(b, (uint8_t)(value & 0xff));
    buffer_push(b, (uint8_t)((value >> 8) & 0xff));
}

static bool make_icon(ByteBuffer *b, uint8_t shade, uint32_t colour0,
    uint32_t alpha0, uint32_t colour1, uint32_t alpha1, Sprite256 *out)
{
    b->size = 0;
    for (int i = 0; i < 256; i++) {
        buffer_push(b, 0);                       /* blue */
        buffer_push(b, (uint8_t)i);              /* green */
        buffer_push(b, (uint8_t)(i + shade));    /* red */
        buffer_push(b, 0);
    }
    buffer_u32_le(b, 2);
    buffer_u32_le(b, 1);
    buffer_u32_le(b, 6);
    put_u16(b, 2);
    put_u16(b, (alpha0 << 9) | (colour0 << 1));
    put_u16(b, (alpha1 << 9) | (colour1 << 1));
    buffer_u32_le(b, 0x80000001u);
    return sprite256_open(b->data, b->size, out);
}

UTEST(icon_strip, create_makes_an_empty_strip)
{
    IconStrip s;
    ASSERT_TRUE(icon_strip_create(&s, 4, 3, 2));
    ASSERT_EQ(4u, s.cell_width);
    ASSERT_EQ(3u, s.cell_height);
    ASSERT_EQ(2u, s.columns);
    for (size_t i = 0; i < 4 * 3 * 2; i++) {
        ASSERT_EQ(0, s.pixels[i]);
        ASSERT_EQ(0, s.solid[i]);
    }
    icon_strip_free(&s);
    ASSERT_TRUE(s.pixels == NULL);
    ASSERT_TRUE(s.solid == NULL);
    ASSERT_TRUE(s.palettes == NULL);
}

UTEST(icon_strip, create_refuses_a_size_of_none_or_over_the_limit)
{
    IconStrip s;
    memset(&s, 0, sizeof s);
    s.columns = 12345;
    ASSERT_FALSE(icon_strip_create(&s, 0, 3, 2));
    ASSERT_FALSE(icon_strip_create(&s, 4, 0, 2));
    ASSERT_FALSE(icon_strip_create(&s, 4, 3, 0));
    ASSERT_FALSE(icon_strip_create(&s, ICON_STRIP_MAX_SIDE + 1, 3, 2));
    ASSERT_FALSE(icon_strip_create(&s, 4, ICON_STRIP_MAX_SIDE + 1, 2));
    ASSERT_FALSE(icon_strip_create(&s, 4, 3, ICON_STRIP_MAX_CELLS + 1));
    ASSERT_EQ(12345u, s.columns);
}

UTEST(icon_strip, put_places_each_icon_in_its_cell_with_its_palette)
{
    ByteBuffer a = {0};
    ByteBuffer b = {0};
    Sprite256 first;
    Sprite256 second;
    ASSERT_TRUE(make_icon(&a, 10, 5, 15, 6, 1, &first));
    ASSERT_TRUE(make_icon(&b, 20, 0, 15, 7, 8, &second));
    IconStrip s;
    ASSERT_TRUE(icon_strip_create(&s, 4, 3, 2));

    ASSERT_TRUE(icon_strip_put(&s, 0, &first));
    ASSERT_TRUE(icon_strip_put(&s, 1, &second));

    /* A frame of 2 by 1 is the middle of a cell of 4 by 3: at 1, 1. The
     * strip is 8 wide, so row 1 starts at 8. */
    ASSERT_EQ(0, s.solid[8 + 0]);
    ASSERT_EQ(5, s.pixels[8 + 1]);
    ASSERT_EQ(255, s.solid[8 + 1]);
    ASSERT_EQ(6, s.pixels[8 + 2]);
    ASSERT_EQ(17, s.solid[8 + 2]);
    ASSERT_EQ(0, s.solid[8 + 3]);
    /* The second cell. Its colour 0 is a pixel that is there. */
    ASSERT_EQ(0, s.pixels[8 + 5]);
    ASSERT_EQ(255, s.solid[8 + 5]);
    ASSERT_EQ(7, s.pixels[8 + 6]);
    ASSERT_EQ(136, s.solid[8 + 6]);
    /* Nothing in the rows above and below. */
    ASSERT_EQ(0, s.solid[1]);
    ASSERT_EQ(0, s.solid[16 + 1]);
    /* Each cell has its own palette. */
    ASSERT_EQ(15, s.palettes[5].r);
    ASSERT_EQ(5, s.palettes[5].g);
    ASSERT_EQ(0, s.palettes[5].b);
    ASSERT_EQ(25, s.palettes[256 + 5].r);
    ASSERT_EQ(5, s.palettes[256 + 5].g);
    icon_strip_free(&s);
    buffer_free(&a);
    buffer_free(&b);
}

UTEST(icon_strip, put_refuses_what_does_not_fit)
{
    ByteBuffer a = {0};
    Sprite256 icon;
    ASSERT_TRUE(make_icon(&a, 0, 5, 15, 6, 15, &icon));
    IconStrip s;
    ASSERT_TRUE(icon_strip_create(&s, 2, 1, 1));

    /* A cell of exactly the frame's size is fine. No such cell is not. */
    ASSERT_TRUE(icon_strip_put(&s, 0, &icon));
    ASSERT_FALSE(icon_strip_put(&s, 1, &icon));
    icon_strip_free(&s);

    /* A cell narrower than the frame. */
    ASSERT_TRUE(icon_strip_create(&s, 1, 1, 1));
    ASSERT_FALSE(icon_strip_put(&s, 0, &icon));
    ASSERT_EQ(0, s.solid[0]);
    icon_strip_free(&s);
    buffer_free(&a);
}

UTEST(icon_strip, put_refuses_a_file_without_a_palette_or_a_frame)
{
    /* Only a trailer: no palette, no frames. */
    const uint8_t empty[4] = {0, 0, 0, 0};
    Sprite256 icon;
    ASSERT_TRUE(sprite256_open(empty, sizeof empty, &icon));
    IconStrip s;
    ASSERT_TRUE(icon_strip_create(&s, 4, 4, 1));

    ASSERT_FALSE(icon_strip_put(&s, 0, &icon));

    /* A palette and a trailer that says one frame, with no frame. */
    static uint8_t bytes[1028];
    memset(bytes, 0, sizeof bytes);
    bytes[1024] = 0;
    bytes[1027] = 0x80;
    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &icon));
    ASSERT_FALSE(icon_strip_put(&s, 0, &icon));
    icon_strip_free(&s);
}
