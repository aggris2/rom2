#include "import/structure_sheet.h"

#include <string.h>

#include "make_sprite.h"
#include "utest.h"

/* Adds a tile of 32 by 32 filled with one value. */
static void add_tile(MakeSprite *m, uint8_t value)
{
    static uint8_t pixels[32 * 32];
    memset(pixels, value, sizeof pixels);
    make_sprite_frame(m, 32, 32, pixels);
}

/* The pixel at `x`, `y` of the picture in cell `column`. */
static uint8_t at(const Sheet *s, uint32_t column, uint32_t x, uint32_t y)
{
    return s->pixels[(size_t)y * sheet_width(s)
        + (size_t)column * s->cell_width + x];
}

/* A layout of 2 by 2 with 3 pictures in which tiles 1 and 3 change. */
static StructureLayout make_layout(bool ruin)
{
    StructureLayout l;
    memset(&l, 0, sizeof l);
    l.width = 2;
    l.height = 2;
    l.phases = 3;
    l.animated[1] = true;
    l.animated[3] = true;
    l.animated_count = 2;
    l.has_ruin = ruin;
    return l;
}

UTEST(structure_sheet, build_puts_the_tiles_together_into_pictures)
{
    /* 12 tiles, each filled with its number from 1 up. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 12; i++) {
        add_tile(&m, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout = make_layout(true);
    StructureSheet s;

    ASSERT_TRUE(structure_sheet_build(&layout, &sprite, NULL, &s));

    ASSERT_EQ(64u, s.sheet.cell_width);
    ASSERT_EQ(64u, s.sheet.cell_height);
    ASSERT_EQ(3u, s.sheet.columns);
    ASSERT_EQ(1u, s.sheet.rows);
    /* The first picture: tiles 1 2 / 3 4. */
    ASSERT_EQ(1, at(&s.sheet, 0, 0, 0));
    ASSERT_EQ(1, at(&s.sheet, 0, 31, 31));
    ASSERT_EQ(2, at(&s.sheet, 0, 32, 0));
    ASSERT_EQ(3, at(&s.sheet, 0, 0, 32));
    ASSERT_EQ(4, at(&s.sheet, 0, 63, 63));
    /* The second: the right tiles are new, 5 and 6. */
    ASSERT_EQ(1, at(&s.sheet, 1, 0, 0));
    ASSERT_EQ(5, at(&s.sheet, 1, 32, 0));
    ASSERT_EQ(3, at(&s.sheet, 1, 0, 32));
    ASSERT_EQ(6, at(&s.sheet, 1, 32, 32));
    /* The third: 7 and 8. */
    ASSERT_EQ(7, at(&s.sheet, 2, 63, 31));
    ASSERT_EQ(8, at(&s.sheet, 2, 63, 32));
    ASSERT_EQ(3, at(&s.sheet, 2, 31, 63));
    /* The ruin: tiles 9 10 / 11 12. */
    ASSERT_EQ(1u, s.ruin.columns);
    ASSERT_EQ(64u, s.ruin.cell_width);
    ASSERT_EQ(9, at(&s.ruin, 0, 0, 0));
    ASSERT_EQ(10, at(&s.ruin, 0, 32, 31));
    ASSERT_EQ(11, at(&s.ruin, 0, 31, 32));
    ASSERT_EQ(12, at(&s.ruin, 0, 63, 63));
    ASSERT_TRUE(s.edges.pixels == NULL);
    ASSERT_TRUE(s.ruin_edges.pixels == NULL);
    structure_sheet_free(&s);
    ASSERT_TRUE(s.sheet.pixels == NULL);
    ASSERT_TRUE(s.ruin.pixels == NULL);
}

UTEST(structure_sheet, build_without_a_ruin_has_no_ruin_sheet)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 8; i++) {
        add_tile(&m, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout = make_layout(false);
    StructureSheet s;

    ASSERT_TRUE(structure_sheet_build(&layout, &sprite, NULL, &s));

    ASSERT_EQ(3u, s.sheet.columns);
    ASSERT_TRUE(s.ruin.pixels == NULL);
    structure_sheet_free(&s);
}

UTEST(structure_sheet, build_puts_the_edges_on_sheets_of_their_own)
{
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    for (uint8_t i = 0; i < 12; i++) {
        add_tile(&m, (uint8_t)(i + 1));
        add_tile(&e, (uint8_t)(i + 101));
    }
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    StructureLayout layout = make_layout(true);
    StructureSheet s;

    ASSERT_TRUE(structure_sheet_build(&layout, &sprite, &edges, &s));

    ASSERT_EQ(3u, s.edges.columns);
    ASSERT_EQ(64u, s.edges.cell_width);
    ASSERT_EQ(101, at(&s.edges, 0, 0, 0));
    ASSERT_EQ(105, at(&s.edges, 1, 32, 0));
    ASSERT_EQ(108, at(&s.edges, 2, 32, 32));
    ASSERT_EQ(112, at(&s.ruin_edges, 0, 63, 63));
    /* The figure's own sheets are as without edges. */
    ASSERT_EQ(5, at(&s.sheet, 1, 32, 0));
    ASSERT_EQ(12, at(&s.ruin, 0, 63, 63));
    structure_sheet_free(&s);
    ASSERT_TRUE(s.edges.pixels == NULL);
    ASSERT_TRUE(s.ruin_edges.pixels == NULL);
}

UTEST(structure_sheet, build_of_pieces_gives_one_tile_in_each_cell)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 5; i++) {
        add_tile(&m, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout;
    memset(&layout, 0, sizeof layout);
    layout.width = 1;
    layout.height = 1;
    layout.phases = 1;
    layout.is_pieces = true;
    layout.pieces = 5;
    StructureSheet s;

    ASSERT_TRUE(structure_sheet_build(&layout, &sprite, NULL, &s));

    ASSERT_EQ(5u, s.sheet.columns);
    ASSERT_EQ(32u, s.sheet.cell_width);
    ASSERT_EQ(32u, s.sheet.cell_height);
    for (uint32_t i = 0; i < 5; i++) {
        ASSERT_EQ(i + 1, at(&s.sheet, i, 0, 0));
        ASSERT_EQ(i + 1, at(&s.sheet, i, 31, 31));
    }
    ASSERT_TRUE(s.ruin.pixels == NULL);
    structure_sheet_free(&s);
}

UTEST(structure_sheet, build_refuses_a_file_with_too_few_tiles)
{
    /* The layout with a ruin needs 12 tiles. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 11; i++) {
        add_tile(&m, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout = make_layout(true);
    StructureSheet s;
    memset(&s, 0, sizeof s);

    ASSERT_FALSE(structure_sheet_build(&layout, &sprite, NULL, &s));

    ASSERT_TRUE(s.sheet.pixels == NULL);
    ASSERT_TRUE(s.ruin.pixels == NULL);
}

UTEST(structure_sheet, build_refuses_a_tile_that_is_not_32_by_32)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 7; i++) {
        add_tile(&m, 1);
    }
    make_sprite_solid(&m, 8, 8, 1);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout = make_layout(false);
    StructureSheet s;
    memset(&s, 0, sizeof s);

    ASSERT_FALSE(structure_sheet_build(&layout, &sprite, NULL, &s));
    ASSERT_TRUE(s.sheet.pixels == NULL);
}

UTEST(structure_sheet, build_refuses_edges_with_another_number_of_tiles)
{
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    for (uint8_t i = 0; i < 8; i++) {
        add_tile(&m, 1);
    }
    for (uint8_t i = 0; i < 7; i++) {
        add_tile(&e, 1);
    }
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    StructureLayout layout = make_layout(false);
    StructureSheet s;
    memset(&s, 0, sizeof s);

    ASSERT_FALSE(structure_sheet_build(&layout, &sprite, &edges, &s));
    ASSERT_TRUE(s.sheet.pixels == NULL);
    ASSERT_TRUE(s.edges.pixels == NULL);
}
