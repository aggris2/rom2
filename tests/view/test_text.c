#include "view/text.h"

#include <string.h>

#include "utest.h"
#include "view/frame.h"

/* A font of cells of 8 by 10, 16 to a row, from the space on: the
 * space has no width, "1" is 2 wide and the other digits 4, and "W" is
 * 6. */
static FontLook font;

static const FontLook *make_font(void)
{
    memset(&font, 0, sizeof font);
    font.known = true;
    font.sheet = 9;
    font.cell_width = 8;
    font.cell_height = 10;
    font.columns = 16;
    font.first_character = 32;
    font.count = 96;
    for (int digit = 0; digit < 10; digit++) {
        font.widths['0' - 32 + digit] = 4;
    }
    font.widths['1' - 32] = 2;
    font.widths['W' - 32] = 6;
    return &font;
}

UTEST(text, a_glyph_moves_the_writing_on_by_its_width_and_two)
{
    const FontLook *f = make_font();
    /* A glyph covers one pixel more than its width. */
    EXPECT_EQ(5, text_width(f, "0"));
    EXPECT_EQ(6 + 5, text_width(f, "00"));
    EXPECT_EQ(4 + 6 + 7, text_width(f, "10W"));
    EXPECT_EQ(0, text_width(f, ""));
}

UTEST(text, a_space_moves_the_writing_on_by_half_a_cells_height_more)
{
    const FontLook *f = make_font();
    /* Its glyph has no width: 5 and the step of 2. */
    EXPECT_EQ(6 + 7 + 5, text_width(f, "0 0"));
}

UTEST(text, a_character_the_font_has_no_glyph_for_takes_no_room)
{
    const FontLook *f = make_font();
    /* A tab is before the first, and a byte of 200 past the last. */
    EXPECT_EQ(6 + 5, text_width(f, "0\t0"));
    EXPECT_EQ(6 + 5, text_width(f, "0\310" "0"));
}

UTEST(text, each_character_is_the_whole_cell_of_its_glyph)
{
    const FontLook *f = make_font();
    Sprite sprites[8];
    ASSERT_EQ(3u, text_sprites(f, "10W", 100, 50, sprites, 8));
    /* "1" is the 17th glyph: the second row, the second cell. */
    EXPECT_EQ(9u, sprites[0].sheet);
    EXPECT_EQ(1u * 8, sprites[0].source_x);
    EXPECT_EQ(1u * 10, sprites[0].source_y);
    EXPECT_EQ(8u, sprites[0].width);
    EXPECT_EQ(10u, sprites[0].height);
    EXPECT_EQ(100, sprites[0].x);
    EXPECT_EQ(50, sprites[0].y);
    /* "0" follows after the width of "1" and the step. */
    EXPECT_EQ(0u * 8, sprites[1].source_x);
    EXPECT_EQ(1u * 10, sprites[1].source_y);
    EXPECT_EQ(104, sprites[1].x);
    /* "W" is the 56th glyph: the fourth row, the eighth cell. */
    EXPECT_EQ(7u * 8, sprites[2].source_x);
    EXPECT_EQ(3u * 10, sprites[2].source_y);
    EXPECT_EQ(8u, sprites[2].width);
    EXPECT_EQ(110, sprites[2].x);
}

UTEST(text, writing_lies_over_the_map_and_under_the_pointer)
{
    const FontLook *f = make_font();
    Sprite sprites[2];
    ASSERT_EQ(1u, text_sprites(f, "0", 0, 0, sprites, 2));
    EXPECT_EQ((unsigned)FRAME_LIGHT_WHOLE, sprites[0].light);
    EXPECT_GT(sprites[0].order, sprite_order(0, 255));
    EXPECT_LT(sprites[0].order, SPRITE_ORDER_POINTER);
}

UTEST(text, a_space_moves_the_writing_on_and_is_no_sprite)
{
    const FontLook *f = make_font();
    Sprite sprites[4];
    ASSERT_EQ(2u, text_sprites(f, "0 0", 10, 0, sprites, 4));
    EXPECT_EQ(10, sprites[0].x);
    EXPECT_EQ(10 + 6 + 7, sprites[1].x);
}

UTEST(text, no_more_sprites_are_written_than_there_is_room_for)
{
    const FontLook *f = make_font();
    Sprite sprites[2];
    EXPECT_EQ(2u, text_sprites(f, "00000", 0, 0, sprites, 2));
    EXPECT_EQ(0u, text_sprites(f, "00000", 0, 0, sprites, 0));
}

UTEST(text, nothing_is_written_in_a_font_that_is_not_known)
{
    make_font();
    font.known = false;
    Sprite sprites[2];
    EXPECT_EQ(0u, text_sprites(&font, "00", 0, 0, sprites, 2));
}
