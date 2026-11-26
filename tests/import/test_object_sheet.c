#include "import/object_sheet.h"

#include <string.h>

#include "make_sprite.h"
#include "utest.h"

/* The pixel at `x`, `y` inside cell `column` of a sheet of one row. */
static uint8_t cell_at(const Sheet *s, uint32_t column, uint32_t x,
    uint32_t y)
{
    return s->pixels[(size_t)y * sheet_width(s)
        + (size_t)column * s->cell_width + x];
}

UTEST(object_sheet, build_puts_the_objects_frames_in_one_row)
{
    /* Five frames of 2 by 2, each filled with its number from 1 up. The
     * object is frames 1, 2 and 3. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 5; i++) {
        make_sprite_solid(&m, 2, 2, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 1, 3, 8, 6, 4, 5, &o));

    ASSERT_EQ(3u, o.sheet.columns);
    ASSERT_EQ(1u, o.sheet.rows);
    ASSERT_EQ(2u, o.sheet.cell_width);
    ASSERT_EQ(2u, o.sheet.cell_height);
    /* The frames sit at 3, 2 in the box, and so does the cell. */
    ASSERT_EQ(1, o.anchor_x);
    ASSERT_EQ(3, o.anchor_y);
    ASSERT_EQ(2, cell_at(&o.sheet, 0, 0, 0));
    ASSERT_EQ(3, cell_at(&o.sheet, 1, 1, 1));
    ASSERT_EQ(4, cell_at(&o.sheet, 2, 0, 1));
    ASSERT_TRUE(o.edges.pixels == NULL);
    object_sheet_free(&o);
    ASSERT_TRUE(o.sheet.pixels == NULL);
}

UTEST(object_sheet, build_keeps_frames_of_different_sizes_in_place)
{
    /* A frame of 6 by 2 and one of 2 by 4 in a box of 8 by 8: the cell
     * is 6 wide and 4 high, from 1, 2. */
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 6, 2, 9);
    make_sprite_solid(&m, 2, 4, 5);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 0, 2, 8, 8, 4, 6, &o));

    ASSERT_EQ(6u, o.sheet.cell_width);
    ASSERT_EQ(4u, o.sheet.cell_height);
    ASSERT_EQ(3, o.anchor_x);
    ASSERT_EQ(4, o.anchor_y);
    ASSERT_EQ(0, cell_at(&o.sheet, 0, 0, 0));
    ASSERT_EQ(9, cell_at(&o.sheet, 0, 0, 1));
    ASSERT_EQ(9, cell_at(&o.sheet, 0, 5, 2));
    ASSERT_EQ(0, cell_at(&o.sheet, 1, 1, 0));
    ASSERT_EQ(5, cell_at(&o.sheet, 1, 2, 0));
    ASSERT_EQ(5, cell_at(&o.sheet, 1, 3, 3));
    object_sheet_free(&o);
}

UTEST(object_sheet, build_is_sized_by_the_objects_own_frames_only)
{
    /* The file is shared: frame 0 belongs to another object and is
     * larger than this object's box. Frame 1 is this object's. */
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 8, 6, 1);
    make_sprite_solid(&m, 2, 2, 7);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 1, 1, 4, 4, 2, 2, &o));

    ASSERT_EQ(2u, o.sheet.cell_width);
    ASSERT_EQ(2u, o.sheet.cell_height);
    ASSERT_EQ(7, cell_at(&o.sheet, 0, 0, 0));
    object_sheet_free(&o);

    /* Asking for the large frame with that box is refused. */
    memset(&o, 0, sizeof o);
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 4, 4, 2, 2, &o));
    ASSERT_TRUE(o.sheet.pixels == NULL);
}

UTEST(object_sheet, build_puts_the_edges_on_a_sheet_with_the_same_cells)
{
    /* The object is 2 by 2 and its edge 4 by 4 around it. */
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    make_sprite_solid(&m, 2, 2, 5);
    make_sprite_solid(&e, 4, 4, 9);
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, &edges, 0, 1, 8, 8, 4, 6, &o));

    ASSERT_EQ(4u, o.sheet.cell_width);
    ASSERT_EQ(4u, o.edges.cell_width);
    ASSERT_EQ(4u, o.edges.cell_height);
    ASSERT_EQ(2, o.anchor_x);
    ASSERT_EQ(4, o.anchor_y);
    ASSERT_EQ(0, cell_at(&o.sheet, 0, 0, 0));
    ASSERT_EQ(5, cell_at(&o.sheet, 0, 1, 1));
    ASSERT_EQ(9, cell_at(&o.edges, 0, 0, 0));
    ASSERT_EQ(9, cell_at(&o.edges, 0, 3, 3));
    object_sheet_free(&o);
    ASSERT_TRUE(o.edges.pixels == NULL);
}

UTEST(object_sheet, build_refuses_frames_that_are_not_in_the_file)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 3; i++) {
        make_sprite_solid(&m, 1, 1, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;
    memset(&o, 0, sizeof o);
    o.anchor_x = 12345;

    /* The last frame is fine. One more is not. */
    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 2, 1, 2, 2, 1, 1, &o));
    object_sheet_free(&o);
    o.anchor_x = 12345;
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 2, 2, 2, 2, 1, 1, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 3, 1, 2, 2, 1, 1, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 4, 2, 2, 1, 1, &o));
    /* No frames at all. */
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 0, 2, 2, 1, 1, &o));
    /* A count so large that first + count wraps round. */
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 2, 0xffffffffu, 2, 2, 1,
        1, &o));
    ASSERT_EQ(12345, o.anchor_x);
    ASSERT_TRUE(o.sheet.pixels == NULL);
}

UTEST(object_sheet, build_refuses_a_box_of_no_size_or_beyond_the_limit)
{
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 1, 1, 1);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;
    memset(&o, 0, sizeof o);

    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 0, 2, 0, 0, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 2, 0, 0, 0, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 2049, 2, 0, 0, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 2, 2049, 0, 0, &o));
    ASSERT_TRUE(o.sheet.pixels == NULL);
}

UTEST(object_sheet, build_refuses_edges_with_another_number_of_frames)
{
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    make_sprite_solid(&m, 1, 1, 1);
    make_sprite_solid(&m, 1, 1, 1);
    make_sprite_solid(&e, 1, 1, 1);
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    ObjectSheet o;
    memset(&o, 0, sizeof o);

    ASSERT_FALSE(object_sheet_build(&sprite, &edges, 0, 1, 2, 2, 1, 1, &o));
    ASSERT_TRUE(o.sheet.pixels == NULL);
    ASSERT_TRUE(o.edges.pixels == NULL);
}

UTEST(object_sheet, build_of_an_object_whose_frames_are_empty)
{
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 0, 0, 0);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 0, 1, 8, 8, 4, 4, &o));

    ASSERT_EQ(1u, o.sheet.cell_width);
    ASSERT_EQ(1u, o.sheet.cell_height);
    ASSERT_EQ(0, cell_at(&o.sheet, 0, 0, 0));
    object_sheet_free(&o);
}
