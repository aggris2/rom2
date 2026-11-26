#include "import/plain_sheet.h"

#include <string.h>

#include "base/buffer.h"
#include "make_sprite.h"
#include "utest.h"

/* Where the pixel at `x`, `y` of the cell of frame `frame` is in the
 * sheet's two blocks. */
static size_t at(const PlainSheet *s, uint32_t frame, uint32_t x,
    uint32_t y)
{
    size_t px = (size_t)(frame % s->columns) * s->cell_width + x;
    size_t py = (size_t)(frame / s->columns) * s->cell_height + y;
    return py * plain_sheet_width(s) + px;
}

/* ---- a .16a file built by hand ---- */

static void put_u16(ByteBuffer *b, uint32_t value)
{
    buffer_push(b, (uint8_t)(value & 0xff));
    buffer_push(b, (uint8_t)((value >> 8) & 0xff));
}

/* Adds a frame of 2 by 1: two pixels, each a colour and a solidness of
 * 0 to 15. */
static void add_16a_frame(ByteBuffer *b, uint32_t colour0, uint32_t alpha0,
    uint32_t colour1, uint32_t alpha1)
{
    buffer_u32_le(b, 2);
    buffer_u32_le(b, 1);
    buffer_u32_le(b, 6);
    put_u16(b, 2);
    put_u16(b, (alpha0 << 9) | (colour0 << 1));
    put_u16(b, (alpha1 << 9) | (colour1 << 1));
}

/* Ends the file with its trailer and opens it. */
static bool finish_16a(ByteBuffer *b, uint32_t frames, Sprite256 *out)
{
    buffer_u32_le(b, frames);
    return sprite256_open(b->data, b->size, out);
}

UTEST(plain_sheet, build_256_lays_the_frames_in_a_row_in_file_order)
{
    MakeSprite m;
    Sprite256 sprite;
    PlainSheet s;
    make_sprite_start(&m);
    make_sprite_solid(&m, 3, 2, 5);
    make_sprite_solid(&m, 3, 2, 6);
    make_sprite_solid(&m, 3, 2, 7);
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));

    ASSERT_TRUE(plain_sheet_build_256(&sprite, &s));

    ASSERT_EQ(3u, s.frames);
    ASSERT_EQ(3u, s.cell_width);
    ASSERT_EQ(2u, s.cell_height);
    ASSERT_EQ(3u, s.columns);
    ASSERT_EQ(1u, s.rows);
    ASSERT_EQ(9u, plain_sheet_width(&s));
    ASSERT_EQ(2u, plain_sheet_height(&s));
    ASSERT_TRUE(s.one_size);
    ASSERT_EQ(5, s.pixels[at(&s, 0, 0, 0)]);
    ASSERT_EQ(5, s.pixels[at(&s, 0, 2, 1)]);
    ASSERT_EQ(6, s.pixels[at(&s, 1, 0, 0)]);
    ASSERT_EQ(7, s.pixels[at(&s, 2, 2, 1)]);
    ASSERT_EQ(3, s.widths[2]);
    ASSERT_EQ(2, s.heights[2]);
    plain_sheet_free(&s);
    ASSERT_TRUE(s.pixels == NULL);
}

UTEST(plain_sheet, build_256_makes_index_0_empty_and_the_rest_solid)
{
    MakeSprite m;
    Sprite256 sprite;
    PlainSheet s;
    const uint8_t pixels[4] = {0, 9, 9, 0};
    make_sprite_start(&m);
    make_sprite_frame(&m, 2, 2, pixels);
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));

    ASSERT_TRUE(plain_sheet_build_256(&sprite, &s));

    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 1)]);
    ASSERT_EQ(0, s.solid[at(&s, 0, 1, 1)]);
    ASSERT_EQ(9, s.pixels[at(&s, 0, 1, 0)]);
    plain_sheet_free(&s);
}

UTEST(plain_sheet, a_cell_is_as_large_as_the_largest_frame)
{
    /* 2 by 3 and 4 by 1 give cells of 4 by 3. Each frame lies in the
     * top left corner of its cell, and the rest of the cell is empty. */
    MakeSprite m;
    Sprite256 sprite;
    PlainSheet s;
    make_sprite_start(&m);
    make_sprite_solid(&m, 2, 3, 5);
    make_sprite_solid(&m, 4, 1, 6);
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));

    ASSERT_TRUE(plain_sheet_build_256(&sprite, &s));

    ASSERT_EQ(4u, s.cell_width);
    ASSERT_EQ(3u, s.cell_height);
    ASSERT_FALSE(s.one_size);
    ASSERT_EQ(2, s.widths[0]);
    ASSERT_EQ(3, s.heights[0]);
    ASSERT_EQ(4, s.widths[1]);
    ASSERT_EQ(1, s.heights[1]);
    ASSERT_EQ(5, s.pixels[at(&s, 0, 1, 2)]);
    ASSERT_EQ(0, s.pixels[at(&s, 0, 2, 0)]);
    ASSERT_EQ(6, s.pixels[at(&s, 1, 3, 0)]);
    ASSERT_EQ(0, s.pixels[at(&s, 1, 0, 1)]);
    ASSERT_EQ(0, s.solid[at(&s, 1, 0, 1)]);
    plain_sheet_free(&s);
}

UTEST(plain_sheet, the_largest_frame_may_come_last)
{
    MakeSprite m;
    Sprite256 sprite;
    PlainSheet s;
    make_sprite_start(&m);
    make_sprite_solid(&m, 1, 1, 5);
    make_sprite_solid(&m, 3, 4, 6);
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));

    ASSERT_TRUE(plain_sheet_build_256(&sprite, &s));

    ASSERT_EQ(3u, s.cell_width);
    ASSERT_EQ(4u, s.cell_height);
    ASSERT_EQ(6, s.pixels[at(&s, 1, 2, 3)]);
    plain_sheet_free(&s);
}

UTEST(plain_sheet, frames_that_do_not_fit_one_row_go_on_to_the_next)
{
    /* Frames 63 wide: 65 of them fit in 4096 pixels, so 67 frames take
     * two rows, with 2 in the second. */
    static MakeSprite m;
    Sprite256 sprite;
    PlainSheet s;
    make_sprite_start(&m);
    for (uint32_t i = 0; i < 67; i++) {
        make_sprite_solid(&m, 63, 1, (uint8_t)(i + 1));
    }
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));

    ASSERT_TRUE(plain_sheet_build_256(&sprite, &s));

    ASSERT_EQ(65u, s.columns);
    ASSERT_EQ(2u, s.rows);
    ASSERT_EQ(65u * 63u, plain_sheet_width(&s));
    ASSERT_EQ(2u, plain_sheet_height(&s));
    ASSERT_EQ(65, s.pixels[at(&s, 64, 62, 0)]);
    /* Frame 65 is the first of the second row. */
    ASSERT_EQ(66, s.pixels[(size_t)plain_sheet_width(&s)]);
    ASSERT_EQ(67, s.pixels[at(&s, 66, 0, 0)]);
    /* The cells after the last frame are empty. */
    ASSERT_EQ(0, s.pixels[(size_t)plain_sheet_width(&s) + 2 * 63]);
    plain_sheet_free(&s);
}

UTEST(plain_sheet, build_16a_gives_colour_and_solidness)
{
    ByteBuffer b = {0};
    Sprite256 sprite;
    PlainSheet s;
    add_16a_frame(&b, 7, 15, 0, 3);
    add_16a_frame(&b, 9, 0, 200, 1);
    ASSERT_TRUE(finish_16a(&b, 2, &sprite));

    ASSERT_TRUE(plain_sheet_build_16a(&sprite, &s));

    ASSERT_EQ(2u, s.frames);
    ASSERT_EQ(2u, s.cell_width);
    ASSERT_EQ(1u, s.cell_height);
    ASSERT_EQ(7, s.pixels[at(&s, 0, 0, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 0)]);
    /* Index 0 is a colour like the others here: solid as the file says. */
    ASSERT_EQ(0, s.pixels[at(&s, 0, 1, 0)]);
    ASSERT_EQ(51, s.solid[at(&s, 0, 1, 0)]);
    ASSERT_EQ(9, s.pixels[at(&s, 1, 0, 0)]);
    ASSERT_EQ(0, s.solid[at(&s, 1, 0, 0)]);
    ASSERT_EQ(200, s.pixels[at(&s, 1, 1, 0)]);
    ASSERT_EQ(17, s.solid[at(&s, 1, 1, 0)]);
    plain_sheet_free(&s);
    buffer_free(&b);
}

UTEST(plain_sheet, build_refuses_a_sprite_without_frames_or_with_a_bad_frame)
{
    ByteBuffer b = {0};
    Sprite256 sprite;
    PlainSheet s;
    memset(&s, 0x5a, sizeof s);
    PlainSheet before = s;

    /* No frames at all. */
    ASSERT_TRUE(finish_16a(&b, 0, &sprite));
    ASSERT_FALSE(plain_sheet_build_16a(&sprite, &s));
    ASSERT_FALSE(plain_sheet_build_256(&sprite, &s));

    /* A good frame, then one that says it is 3 wide and has only two
     * pixels. */
    b.size = 0;
    add_16a_frame(&b, 7, 15, 0, 3);
    buffer_u32_le(&b, 3);
    buffer_u32_le(&b, 1);
    buffer_u32_le(&b, 6);
    put_u16(&b, 2);
    put_u16(&b, 0);
    put_u16(&b, 0);
    ASSERT_TRUE(finish_16a(&b, 2, &sprite));
    ASSERT_FALSE(plain_sheet_build_16a(&sprite, &s));

    ASSERT_EQ(0, memcmp(&s, &before, sizeof s));
    buffer_free(&b);
}

/* The code of a .16a frame that skips whole rows. */
#define SKIP_ROWS 0x4000u

/* Adds an empty .16a frame of `width` by `height`: one code that skips
 * all its rows. */
static void add_empty_16a_frame(ByteBuffer *b, uint32_t width,
    uint32_t height)
{
    buffer_u32_le(b, width);
    buffer_u32_le(b, height);
    buffer_u32_le(b, 2);
    put_u16(b, SKIP_ROWS | height);
}

UTEST(plain_sheet, build_refuses_a_sheet_higher_than_a_png_can_be)
{
    /* Frames of 2049 by 4096 are too wide for two in a row, so each
     * takes a row of its own. Four of them are 16,384 pixels high, the
     * most a PNG holds, and a fifth is too much. */
    ByteBuffer b = {0};
    Sprite256 sprite;
    PlainSheet s;
    for (uint32_t i = 0; i < 4; i++) {
        add_empty_16a_frame(&b, 2049, 4096);
    }
    ASSERT_TRUE(finish_16a(&b, 4, &sprite));
    ASSERT_TRUE(plain_sheet_build_16a(&sprite, &s));
    ASSERT_EQ(1u, s.columns);
    ASSERT_EQ(4u, s.rows);
    ASSERT_EQ(16384u, plain_sheet_height(&s));
    plain_sheet_free(&s);

    /* The trailer goes, a fifth frame comes, and a new trailer. */
    b.size -= 4;
    add_empty_16a_frame(&b, 2049, 4096);
    ASSERT_TRUE(finish_16a(&b, 5, &sprite));
    ASSERT_FALSE(plain_sheet_build_16a(&sprite, &s));
    buffer_free(&b);
}

UTEST(plain_sheet, two_frames_share_a_row_when_both_fit_in_its_width)
{
    /* 2048 wide: two make exactly the 4096 of a row. */
    ByteBuffer b = {0};
    Sprite256 sprite;
    PlainSheet s;
    for (uint32_t i = 0; i < 3; i++) {
        add_empty_16a_frame(&b, 2048, 1);
    }
    ASSERT_TRUE(finish_16a(&b, 3, &sprite));

    ASSERT_TRUE(plain_sheet_build_16a(&sprite, &s));

    ASSERT_EQ(2u, s.columns);
    ASSERT_EQ(2u, s.rows);
    plain_sheet_free(&s);
    buffer_free(&b);
}
