#include "import/font_sheet.h"

#include <string.h>

#include "base/buffer.h"
#include "utest.h"

/* Where the pixel at `x`, `y` of the cell of glyph `glyph` is. */
static size_t at(const FontSheet *s, uint32_t glyph, uint32_t x, uint32_t y)
{
    size_t px = (size_t)(glyph % FONT_SHEET_COLUMNS) * s->cell_width + x;
    size_t py = (size_t)(glyph / FONT_SHEET_COLUMNS) * s->cell_height + y;
    return py * font_sheet_width(s) + px;
}

static void put_u16(ByteBuffer *b, uint32_t value)
{
    buffer_push(b, (uint8_t)(value & 0xff));
    buffer_push(b, (uint8_t)((value >> 8) & 0xff));
}

/* Adds a .16 glyph of `width` by 1 whose pixels are all `alpha`, 1 to
 * 15. `width` is even and at most 126. */
static void add_16_glyph(ByteBuffer *b, uint32_t width, uint32_t alpha)
{
    buffer_u32_le(b, width);
    buffer_u32_le(b, 1);
    buffer_u32_le(b, 1 + width / 2);
    buffer_push(b, (uint8_t)(width / 2));
    for (uint32_t i = 0; i < width / 2; i++) {
        buffer_push(b, (uint8_t)(alpha | (alpha << 4)));
    }
}

/* Adds a .16 glyph of 2 by `height` with nothing in it. */
static void add_empty_16_glyph(ByteBuffer *b, uint32_t height)
{
    buffer_u32_le(b, 2);
    buffer_u32_le(b, height);
    buffer_u32_le(b, 1);
    buffer_push(b, (uint8_t)(0x40 | height));
}

/* Adds a .16a glyph of 2 by 1: two pixels, each a colour and a
 * solidness of 0 to 15. */
static void add_16a_glyph(ByteBuffer *b, uint32_t colour0, uint32_t alpha0,
    uint32_t colour1, uint32_t alpha1)
{
    buffer_u32_le(b, 2);
    buffer_u32_le(b, 1);
    buffer_u32_le(b, 6);
    put_u16(b, 2);
    put_u16(b, (alpha0 << 9) | (colour0 << 1));
    put_u16(b, (alpha1 << 9) | (colour1 << 1));
}

/* Ends the file with its count and opens it. */
static bool finish(ByteBuffer *b, uint32_t glyphs, Sprite256 *out)
{
    buffer_u32_le(b, glyphs);
    return sprite256_open(b->data, b->size, out);
}

UTEST(font_sheet, build_16_lays_the_glyphs_sixteen_across)
{
    /* 18 glyphs: 16 in the first row and 2 in the second. Glyph i has
     * the solidness (i % 15) + 1. */
    ByteBuffer b = {0};
    Sprite256 sprite;
    FontSheet s;
    for (uint32_t i = 0; i < 18; i++) {
        add_16_glyph(&b, 2, i % 15 + 1);
    }
    ASSERT_TRUE(finish(&b, 18, &sprite));

    ASSERT_TRUE(font_sheet_build_16(&sprite, &s));

    ASSERT_EQ(18u, s.glyphs);
    ASSERT_EQ(2u, s.cell_width);
    ASSERT_EQ(1u, s.cell_height);
    ASSERT_EQ(2u, s.rows);
    ASSERT_EQ(32u, font_sheet_width(&s));
    ASSERT_EQ(2u, font_sheet_height(&s));
    ASSERT_TRUE(s.pixels == NULL);
    /* 1 becomes 17, 15 becomes 255. */
    ASSERT_EQ(17, s.solid[at(&s, 0, 0, 0)]);
    ASSERT_EQ(17, s.solid[at(&s, 0, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 14, 1, 0)]);
    ASSERT_EQ(17, s.solid[at(&s, 15, 0, 0)]);
    /* Glyph 16 starts the second row, and the cells after 17 are
     * empty. */
    ASSERT_EQ(34, s.solid[32]);
    ASSERT_EQ(51, s.solid[at(&s, 17, 1, 0)]);
    ASSERT_EQ(0, s.solid[32 + 4]);
    font_sheet_free(&s);
    ASSERT_TRUE(s.solid == NULL);
    buffer_free(&b);
}

UTEST(font_sheet, a_cell_is_as_large_as_the_largest_glyph)
{
    /* 2 by 1, then 2 by 3 empty, then 6 by 1: cells of 6 by 3, each
     * glyph in the top left corner of its own. */
    ByteBuffer b = {0};
    Sprite256 sprite;
    FontSheet s;
    add_16_glyph(&b, 2, 5);
    add_empty_16_glyph(&b, 3);
    add_16_glyph(&b, 6, 9);
    ASSERT_TRUE(finish(&b, 3, &sprite));

    ASSERT_TRUE(font_sheet_build_16(&sprite, &s));

    ASSERT_EQ(6u, s.cell_width);
    ASSERT_EQ(3u, s.cell_height);
    ASSERT_EQ(1u, s.rows);
    ASSERT_EQ(85, s.solid[at(&s, 0, 1, 0)]);
    ASSERT_EQ(0, s.solid[at(&s, 0, 2, 0)]);
    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 1)]);
    ASSERT_EQ(153, s.solid[at(&s, 2, 5, 0)]);
    ASSERT_EQ(0, s.solid[at(&s, 2, 5, 1)]);
    font_sheet_free(&s);
    buffer_free(&b);
}

UTEST(font_sheet, build_16a_gives_colour_and_solidness)
{
    ByteBuffer b = {0};
    Sprite256 sprite;
    FontSheet s;
    add_16a_glyph(&b, 7, 15, 0, 3);
    add_16a_glyph(&b, 200, 1, 9, 0);
    ASSERT_TRUE(finish(&b, 2, &sprite));

    ASSERT_TRUE(font_sheet_build_16a(&sprite, &s));

    ASSERT_TRUE(s.pixels != NULL);
    ASSERT_EQ(7, s.pixels[at(&s, 0, 0, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 0)]);
    ASSERT_EQ(0, s.pixels[at(&s, 0, 1, 0)]);
    ASSERT_EQ(51, s.solid[at(&s, 0, 1, 0)]);
    ASSERT_EQ(200, s.pixels[at(&s, 1, 0, 0)]);
    ASSERT_EQ(17, s.solid[at(&s, 1, 0, 0)]);
    ASSERT_EQ(9, s.pixels[at(&s, 1, 1, 0)]);
    ASSERT_EQ(0, s.solid[at(&s, 1, 1, 0)]);
    font_sheet_free(&s);
    buffer_free(&b);
}

UTEST(font_sheet, build_refuses_no_glyphs_a_bad_glyph_and_the_other_kind)
{
    ByteBuffer b = {0};
    Sprite256 sprite;
    FontSheet s;
    memset(&s, 0x5a, sizeof s);
    FontSheet before = s;

    ASSERT_TRUE(finish(&b, 0, &sprite));
    ASSERT_FALSE(font_sheet_build_16(&sprite, &s));
    ASSERT_FALSE(font_sheet_build_16a(&sprite, &s));

    /* A good glyph and then one whose codes stop a pixel short. */
    b.size = 0;
    add_16_glyph(&b, 2, 5);
    buffer_u32_le(&b, 3);
    buffer_u32_le(&b, 1);
    buffer_u32_le(&b, 2);
    buffer_push(&b, 1);
    buffer_push(&b, 0x55);
    ASSERT_TRUE(finish(&b, 2, &sprite));
    ASSERT_FALSE(font_sheet_build_16(&sprite, &s));

    /* A file of one kind read as the other. */
    b.size = 0;
    add_16a_glyph(&b, 7, 15, 0, 3);
    ASSERT_TRUE(finish(&b, 1, &sprite));
    ASSERT_FALSE(font_sheet_build_16(&sprite, &s));

    ASSERT_EQ(0, memcmp(&s, &before, sizeof s));
    buffer_free(&b);
}

UTEST(font_sheet, build_refuses_a_sheet_larger_than_a_png_can_be)
{
    /* 16 glyphs of 1024 across are 16,384 pixels, the most a PNG holds.
     * 1026 across is too much. */
    ByteBuffer b = {0};
    Sprite256 sprite;
    FontSheet s;
    buffer_u32_le(&b, 1024);
    buffer_u32_le(&b, 1);
    buffer_u32_le(&b, 1);
    buffer_push(&b, 0x41);
    ASSERT_TRUE(finish(&b, 1, &sprite));
    ASSERT_TRUE(font_sheet_build_16(&sprite, &s));
    ASSERT_EQ(16384u, font_sheet_width(&s));
    font_sheet_free(&s);

    b.size = 0;
    buffer_u32_le(&b, 1026);
    buffer_u32_le(&b, 1);
    buffer_u32_le(&b, 1);
    buffer_push(&b, 0x41);
    ASSERT_TRUE(finish(&b, 1, &sprite));
    ASSERT_FALSE(font_sheet_build_16(&sprite, &s));
    buffer_free(&b);
}
