#include "view/pointer.h"

#include "utest.h"
#include "view/frame.h"

/* Which way a screen of 800 by 600 scrolls with the mouse at a place:
 * across times 10 plus down. */
static int scroll_at(int32_t x, int32_t y)
{
    int32_t across = 9;
    int32_t down = 9;
    pointer_scroll(x, y, 800, 600, &across, &down);
    return across * 10 + down;
}

UTEST(pointer, the_map_does_not_scroll_with_the_mouse_inside_the_screen)
{
    EXPECT_EQ(0, scroll_at(400, 300));
    EXPECT_EQ(0, scroll_at(1, 1));
    EXPECT_EQ(0, scroll_at(798, 598));
}

UTEST(pointer, the_map_scrolls_with_the_mouse_on_the_edge_of_the_screen)
{
    EXPECT_EQ(-10, scroll_at(0, 300));
    EXPECT_EQ(10, scroll_at(799, 300));
    EXPECT_EQ(-1, scroll_at(400, 0));
    EXPECT_EQ(1, scroll_at(400, 599));
    /* And past it. */
    EXPECT_EQ(-10, scroll_at(-5, 300));
    EXPECT_EQ(10, scroll_at(900, 300));
}

UTEST(pointer, in_a_corner_the_map_scrolls_both_ways)
{
    EXPECT_EQ(-11, scroll_at(0, 0));
    EXPECT_EQ(9, scroll_at(799, 0));
    EXPECT_EQ(-9, scroll_at(0, 599));
    EXPECT_EQ(11, scroll_at(799, 599));
}

UTEST(pointer, an_arrow_points_the_way_the_map_scrolls)
{
    EXPECT_EQ(POINTER_UP, (int)pointer_kind(0, -1));
    EXPECT_EQ(POINTER_UP_RIGHT, (int)pointer_kind(1, -1));
    EXPECT_EQ(POINTER_RIGHT, (int)pointer_kind(1, 0));
    EXPECT_EQ(POINTER_DOWN_RIGHT, (int)pointer_kind(1, 1));
    EXPECT_EQ(POINTER_DOWN, (int)pointer_kind(0, 1));
    EXPECT_EQ(POINTER_DOWN_LEFT, (int)pointer_kind(-1, 1));
    EXPECT_EQ(POINTER_LEFT, (int)pointer_kind(-1, 0));
    EXPECT_EQ(POINTER_UP_LEFT, (int)pointer_kind(-1, -1));
    EXPECT_EQ(POINTER_PLAIN, (int)pointer_kind(0, 0));
}

UTEST(pointer, the_arrows_are_numbered_from_up_round_by_the_right)
{
    /* As the original numbers their pictures, arrow0 to arrow7. */
    EXPECT_EQ(0, (int)POINTER_UP);
    EXPECT_EQ(2, (int)POINTER_RIGHT);
    EXPECT_EQ(4, (int)POINTER_DOWN);
    EXPECT_EQ(6, (int)POINTER_LEFT);
    EXPECT_EQ(7, (int)POINTER_UP_LEFT);
    EXPECT_EQ(9, (int)POINTER_KINDS);
}

UTEST(pointer, its_tip_is_where_the_mouse_is)
{
    PointerLook look = {true, 77, 32, 32, 4, 5};
    Sprite sprite = pointer_sprite(&look, 100, 200);
    EXPECT_EQ(77u, sprite.sheet);
    EXPECT_EQ(96, sprite.x);
    EXPECT_EQ(195, sprite.y);
    EXPECT_EQ(32u, sprite.width);
    EXPECT_EQ(32u, sprite.height);
    EXPECT_EQ(0u, sprite.source_x);
    EXPECT_EQ(0u, sprite.source_y);
    /* As its picture, and over everything. */
    EXPECT_EQ((unsigned)FRAME_LIGHT_WHOLE, sprite.light);
    EXPECT_EQ(UINT32_MAX, sprite.order);
    /* The arrow that points right reaches left of the mouse. */
    PointerLook right = {true, 78, 32, 32, 26, 16};
    Sprite arrow = pointer_sprite(&right, 799, 300);
    EXPECT_EQ(773, arrow.x);
    EXPECT_EQ(284, arrow.y);
}
