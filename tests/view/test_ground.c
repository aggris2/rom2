#include "view/ground.h"

#include <string.h>

#include "utest.h"

/* A map of 6 by 5 squares, built in memory. The square at (x, y) names
 * the tile in column x + 1 and row y of the sheet, so that a corner
 * says which square it came from. The ground is flat and evenly lit
 * until a test changes it. */
typedef struct MakeGround {
    Map map;
    uint16_t ground[30];
    uint8_t heights[30];
    uint8_t light[30];
    GroundCorner corners[4 * 80];
} MakeGround;

static void make_ground(MakeGround *m)
{
    memset(m, 0, sizeof *m);
    m->map.info.width = 6;
    m->map.info.height = 5;
    m->map.ground = m->ground;
    m->map.heights = m->heights;
    memset(m->light, 50, sizeof m->light);
    for (uint32_t y = 0; y < 5; y++) {
        for (uint32_t x = 0; x < 6; x++) {
            m->ground[y * 6 + x] = (uint16_t)(((x + 1) << 4) | y);
        }
    }
}

/* A sheet of 16 columns and 8 rows of tiles. */
#define SHEET_WIDTH (16 * 32)
#define SHEET_HEIGHT (8 * 32)

/* The squares written for a camera, or 9999 if more corners were
 * written than ground_most() allows or not four to a square. */
static size_t squares_for(MakeGround *m, int32_t left, int32_t top,
                          int32_t width, int32_t height)
{
    Camera camera = {left, top, width, height};
    size_t count = ground_corners(&m->map, m->light, &camera, SHEET_WIDTH,
        SHEET_HEIGHT, m->corners);
    return count <= ground_most(&camera) && count % 4 == 0
        && count <= sizeof m->corners / sizeof m->corners[0]
        ? count / 4 : 9999;
}

UTEST(ground, a_flat_square_is_four_corners_over_its_tile)
{
    MakeGround m;
    make_ground(&m);
    /* A camera on the map's last square alone, (5, 4). */
    ASSERT_EQ(1u, squares_for(&m, 5 * 32, 4 * 32, 32, 32));
    const GroundCorner *c = m.corners;
    /* Top left, top right, low left, low right. */
    const int16_t x[4] = {0, 32, 0, 32};
    const int16_t y[4] = {0, 0, 32, 32};
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(x[i], c[i].x);
        EXPECT_EQ(y[i], c[i].y);
        EXPECT_EQ(6 * 32 + x[i], c[i].point_x);
        EXPECT_EQ(4 * 32 + y[i], c[i].point_y);
        EXPECT_EQ(6u * 32, c[i].tile_x);
        EXPECT_EQ(4u * 32, c[i].tile_y);
        EXPECT_EQ(50u, c[i].light);
        EXPECT_EQ(0u, c[i].unused);
    }
}

UTEST(ground, a_corner_is_raised_by_its_height_and_has_its_light)
{
    MakeGround m;
    make_ground(&m);
    /* The four corners of square (1, 1), each with its own. */
    m.heights[1 * 6 + 1] = 10;
    m.heights[1 * 6 + 2] = 20;
    m.heights[2 * 6 + 1] = 30;
    m.heights[2 * 6 + 2] = 40;
    m.light[1 * 6 + 1] = 11;
    m.light[1 * 6 + 2] = 22;
    m.light[2 * 6 + 1] = 33;
    m.light[2 * 6 + 2] = 44;
    /* The square itself, and the three rows below it. */
    ASSERT_EQ(4u, squares_for(&m, 32, 32, 32, 1));
    const GroundCorner *c = m.corners;
    EXPECT_EQ(0, c[0].x);
    EXPECT_EQ(-10, c[0].y);
    EXPECT_EQ(11u, c[0].light);
    EXPECT_EQ(32, c[1].x);
    EXPECT_EQ(-20, c[1].y);
    EXPECT_EQ(22u, c[1].light);
    EXPECT_EQ(0, c[2].x);
    EXPECT_EQ(32 - 30, c[2].y);
    EXPECT_EQ(33u, c[2].light);
    EXPECT_EQ(32, c[3].x);
    EXPECT_EQ(32 - 40, c[3].y);
    EXPECT_EQ(44u, c[3].light);
    /* The points of the tile do not move with the height. */
    EXPECT_EQ(2u * 32, c[0].point_x);
    EXPECT_EQ(1u * 32, c[0].point_y);
    EXPECT_EQ(3u * 32, c[3].point_x);
    EXPECT_EQ(2u * 32, c[3].point_y);
}

UTEST(ground, squares_that_meet_share_the_place_of_their_corner)
{
    MakeGround m;
    make_ground(&m);
    m.heights[2 * 6 + 2] = 17;
    /* Squares (1, 1), (2, 1), (1, 2) and (2, 2) meet at corner (2, 2). */
    /* Those four, and the two rows below them. */
    ASSERT_EQ(4u + 2u * 2u, squares_for(&m, 32, 32, 64, 64));
    const GroundCorner *low_right_of_first = &m.corners[0 * 4 + 3];
    const GroundCorner *low_left_of_second = &m.corners[1 * 4 + 2];
    const GroundCorner *top_right_of_third = &m.corners[2 * 4 + 1];
    const GroundCorner *top_left_of_fourth = &m.corners[3 * 4 + 0];
    EXPECT_EQ(32, low_right_of_first->x);
    EXPECT_EQ(32 - 17, low_right_of_first->y);
    EXPECT_EQ(low_right_of_first->x, low_left_of_second->x);
    EXPECT_EQ(low_right_of_first->y, low_left_of_second->y);
    EXPECT_EQ(low_right_of_first->x, top_right_of_third->x);
    EXPECT_EQ(low_right_of_first->y, top_right_of_third->y);
    EXPECT_EQ(low_right_of_first->x, top_left_of_fourth->x);
    EXPECT_EQ(low_right_of_first->y, top_left_of_fourth->y);
}

UTEST(ground, a_corner_past_the_edge_takes_the_one_beside_it)
{
    MakeGround m;
    make_ground(&m);
    /* The last square: its right and low corners are past the map. */
    m.heights[4 * 6 + 5] = 9;
    m.light[4 * 6 + 5] = 77;
    ASSERT_EQ(1u, squares_for(&m, 5 * 32, 4 * 32, 32, 32));
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(77u, m.corners[i].light);
    }
    EXPECT_EQ(-9, m.corners[0].y);
    EXPECT_EQ(-9, m.corners[1].y);
    EXPECT_EQ(32 - 9, m.corners[2].y);
    EXPECT_EQ(32 - 9, m.corners[3].y);
}

UTEST(ground, squares_are_written_row_after_row_from_the_top)
{
    MakeGround m;
    make_ground(&m);
    /* Squares 1 and 2 across, rows 0 and 1 seen, and the rows below
     * them that high ground could reach up from: all five. */
    ASSERT_EQ(2u * 5u, squares_for(&m, 32, 0, 64, 64));
    EXPECT_EQ(2u * 32, m.corners[0 * 4].tile_x);
    EXPECT_EQ(0u * 32, m.corners[0 * 4].tile_y);
    EXPECT_EQ(3u * 32, m.corners[1 * 4].tile_x);
    EXPECT_EQ(0u * 32, m.corners[1 * 4].tile_y);
    EXPECT_EQ(2u * 32, m.corners[2 * 4].tile_x);
    EXPECT_EQ(1u * 32, m.corners[2 * 4].tile_y);
    EXPECT_EQ(3u * 32, m.corners[9 * 4].tile_x);
    EXPECT_EQ(4u * 32, m.corners[9 * 4].tile_y);
}

UTEST(ground, rows_below_the_screen_that_can_reach_into_it_are_written)
{
    MakeGround m;
    make_ground(&m);
    /* A screen one pixel high in row 0: rows 1 to 4 lie below it, and
     * ground 127 high reaches up from all four. */
    EXPECT_EQ(5u, squares_for(&m, 0, 0, 32, 1));
    /* A map with more rows than that below the screen is cut there. */
    EXPECT_EQ(4u * 5u, squares_for(&m, 32, 0, 100, 1));
}

UTEST(ground, a_camera_between_squares_shows_a_part_of_one_more)
{
    MakeGround m;
    make_ground(&m);
    /* From pixel 33 to 96 across: squares 1, 2 and the first pixel of
     * square 3. */
    ASSERT_EQ(3u * 5u, squares_for(&m, 33, 0, 64, 32));
    EXPECT_EQ(-1, m.corners[0].x);
    EXPECT_EQ(31, m.corners[1].x);
    EXPECT_EQ(63, m.corners[2 * 4].x);
    /* One pixel less wide, and square 3 is not seen. */
    EXPECT_EQ(2u * 5u, squares_for(&m, 33, 0, 63, 32));
    /* The camera's top moves every corner up the screen. */
    ASSERT_EQ(3u, squares_for(&m, 0, 4 * 32 + 6, 96, 20));
    EXPECT_EQ(-6, m.corners[0].y);
    EXPECT_EQ(26, m.corners[2].y);
}

UTEST(ground, squares_outside_the_map_give_nothing)
{
    MakeGround m;
    make_ground(&m);
    /* Half off the top left corner: column 0 alone, every row. */
    ASSERT_EQ(5u, squares_for(&m, -40, -40, 64, 64));
    EXPECT_EQ(40, m.corners[0].x);
    EXPECT_EQ(40, m.corners[0].y);
    EXPECT_EQ(1u * 32, m.corners[0].tile_x);
    /* Wholly off the map, on each side. */
    EXPECT_EQ(0u, squares_for(&m, -64, 0, 64, 64));
    EXPECT_EQ(0u, squares_for(&m, 6 * 32, 0, 64, 64));
    EXPECT_EQ(0u, squares_for(&m, 0, 5 * 32, 64, 64));
    /* Above the map, the rows the screen could be reached from still
     * count: the screen ends 1 pixel above row 0. */
    EXPECT_EQ(2u * 4u, squares_for(&m, 0, -65, 64, 64));
    /* One pixel onto it from the left. */
    EXPECT_EQ(5u, squares_for(&m, -63, 0, 64, 64));
}

UTEST(ground, a_tile_the_sheet_does_not_have_is_left_out)
{
    MakeGround m;
    make_ground(&m);
    m.ground[4 * 6 + 1] = (16 << 4) | 0;    /* one column too far */
    m.ground[4 * 6 + 2] = (1 << 4) | 8;     /* one row too far */
    m.ground[4 * 6 + 3] = (15 << 4) | 7;    /* the last there is */
    ASSERT_EQ(1u, squares_for(&m, 32, 4 * 32, 96, 32));
    EXPECT_EQ(15u * 32, m.corners[0].tile_x);
    EXPECT_EQ(7u * 32, m.corners[0].tile_y);
    EXPECT_EQ(64, m.corners[0].x);
}

UTEST(ground, the_bits_above_the_tile_are_not_part_of_it)
{
    /* Bit 13 says a square cannot be walked on, and nothing about how
     * it looks. */
    MakeGround m;
    make_ground(&m);
    m.ground[4 * 6 + 1] |= 0x2000;
    ASSERT_EQ(1u, squares_for(&m, 32, 4 * 32, 32, 32));
    EXPECT_EQ(2u * 32, m.corners[0].tile_x);
    EXPECT_EQ(4u * 32, m.corners[0].tile_y);
}

UTEST(ground, a_camera_with_no_size_sees_nothing)
{
    MakeGround m;
    make_ground(&m);
    EXPECT_EQ(0u, squares_for(&m, 32, 32, 0, 64));
    EXPECT_EQ(0u, squares_for(&m, 32, 32, 64, 0));
    EXPECT_EQ(0u, squares_for(&m, 32, 32, -5, 64));
    Camera none = {0, 0, 0, 64};
    EXPECT_EQ(0u, ground_most(&none));
}

UTEST(ground, most_is_enough_for_any_place_of_the_camera)
{
    MakeGround m;
    make_ground(&m);
    Camera camera = {0, 0, 70, 40};
    /* 4 across, and 3 down with 4 more below. */
    EXPECT_EQ(4u * 7u * 4u, ground_most(&camera));
    for (int32_t left = -40; left < 240; left += 7) {
        for (int32_t top = -200; top < 200; top += 5) {
            EXPECT_NE(9999u, squares_for(&m, left, top, 70, 40));
        }
    }
}

UTEST(ground, the_height_at_a_point_is_that_of_the_triangle_drawn_there)
{
    /* Three corners of square (1, 1) are down and its low right one is
     * up: the square is not flat, and its two triangles differ. */
    MakeGround m;
    make_ground(&m);
    m.heights[2 * 6 + 2] = 64;
    /* The triangle of the top left corner lies flat on the ground. */
    EXPECT_EQ(0, ground_height_at(&m.map, 32 + 8, 32 + 8));
    EXPECT_EQ(0, ground_height_at(&m.map, 32 + 16, 32 + 16));
    EXPECT_EQ(0, ground_height_at(&m.map, 32 + 31, 32 + 1));
    EXPECT_EQ(0, ground_height_at(&m.map, 32 + 1, 32 + 31));
    /* The other rises from the cut to the raised corner. */
    EXPECT_EQ(32, ground_height_at(&m.map, 32 + 24, 32 + 24));
    EXPECT_EQ(16, ground_height_at(&m.map, 32 + 24, 32 + 16));
    EXPECT_EQ(16, ground_height_at(&m.map, 32 + 16, 32 + 24));
    EXPECT_EQ(64, ground_height_at(&m.map, 64, 64));
    /* And the other way: only the top left corner is up. */
    m.heights[2 * 6 + 2] = 0;
    m.heights[1 * 6 + 1] = 64;
    EXPECT_EQ(64, ground_height_at(&m.map, 32, 32));
    EXPECT_EQ(32, ground_height_at(&m.map, 32 + 8, 32 + 8));
    EXPECT_EQ(0, ground_height_at(&m.map, 32 + 16, 32 + 16));
    EXPECT_EQ(0, ground_height_at(&m.map, 32 + 24, 32 + 24));
}

UTEST(ground, the_height_at_a_point_lies_between_its_four_corners)
{
    MakeGround m;
    make_ground(&m);
    m.heights[1 * 6 + 1] = 0;
    m.heights[1 * 6 + 2] = 64;
    m.heights[2 * 6 + 1] = 32;
    m.heights[2 * 6 + 2] = 96;
    EXPECT_EQ(0, ground_height_at(&m.map, 32, 32));
    EXPECT_EQ(32, ground_height_at(&m.map, 48, 32));
    EXPECT_EQ(16, ground_height_at(&m.map, 32, 48));
    EXPECT_EQ(48, ground_height_at(&m.map, 48, 48));
    EXPECT_EQ(64, ground_height_at(&m.map, 64, 32));
    EXPECT_EQ(96, ground_height_at(&m.map, 64, 64));
    /* Outside the map counts as its nearest corner. */
    m.heights[0] = 40;
    EXPECT_EQ(40, ground_height_at(&m.map, -500, -500));
    m.heights[4 * 6 + 5] = 70;
    EXPECT_EQ(70, ground_height_at(&m.map, 9000, 9000));
}
