#include "view/ground.h"

#include <string.h>

#include "utest.h"

/* A map of 6 by 5 squares, built in memory. The square at (x, y) names
 * the tile in column x + 1 and row y of the sheet, so that a square
 * written says which square of the map it is. The ground is flat and evenly lit
 * until a test changes it. */
typedef struct MakeGround {
    Map map;
    uint16_t ground[30];
    uint8_t heights[30];
    uint8_t light[30];
    const uint8_t *marks;   /* NULL for none */
    GroundSquare squares[80];
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

/* The squares written for a camera, or 9999 if more were written than
 * ground_most() allows. */
static size_t squares_for(MakeGround *m, int32_t left, int32_t top,
                          int32_t width, int32_t height)
{
    Camera camera = {left, top, width, height};
    size_t count = ground_squares(&m->map, m->light, m->marks, 0, &camera,
        SHEET_WIDTH, SHEET_HEIGHT, m->squares);
    return count <= ground_most(&camera)
        && count <= sizeof m->squares / sizeof m->squares[0]
        ? count : 9999;
}

UTEST(ground, a_flat_square_is_as_its_tile)
{
    MakeGround m;
    make_ground(&m);
    /* A camera on the map's last square alone, (5, 4). */
    ASSERT_EQ(1u, squares_for(&m, 5 * 32, 4 * 32, 32, 32));
    const GroundSquare *s = m.squares;
    EXPECT_EQ(0, s->x);
    /* Top left, top right, low left, low right. */
    const int16_t y[4] = {0, 0, 32, 32};
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(y[i], s->y[i]);
        EXPECT_EQ(50u, s->light[i]);
    }
    EXPECT_EQ(6u * 32, s->tile_x);
    EXPECT_EQ(4u * 32, s->tile_y);
    EXPECT_EQ(0u, s->mark);
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
    const GroundSquare *s = m.squares;
    EXPECT_EQ(0, s->x);
    EXPECT_EQ(-10, s->y[0]);
    EXPECT_EQ(11u, s->light[0]);
    EXPECT_EQ(-20, s->y[1]);
    EXPECT_EQ(22u, s->light[1]);
    EXPECT_EQ(32 - 30, s->y[2]);
    EXPECT_EQ(33u, s->light[2]);
    EXPECT_EQ(32 - 40, s->y[3]);
    EXPECT_EQ(44u, s->light[3]);
    /* The tile does not move with the height. */
    EXPECT_EQ(2u * 32, s->tile_x);
    EXPECT_EQ(1u * 32, s->tile_y);
}

UTEST(ground, squares_that_meet_share_the_place_of_their_corner)
{
    MakeGround m;
    make_ground(&m);
    m.heights[2 * 6 + 2] = 17;
    m.light[2 * 6 + 2] = 61;
    /* Squares (1, 1), (2, 1), (1, 2) and (2, 2) meet at corner (2, 2). */
    /* Those four, and the two rows below them. */
    ASSERT_EQ(4u + 2u * 2u, squares_for(&m, 32, 32, 64, 64));
    /* The low right of the first, the low left of the second, the top
     * right of the third and the top left of the fourth. */
    const int corner[4] = {3, 2, 1, 0};
    const int16_t x[4] = {0, 32, 0, 32};
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(x[i], m.squares[i].x);
        EXPECT_EQ(32 - 17, m.squares[i].y[corner[i]]);
        EXPECT_EQ(61u, m.squares[i].light[corner[i]]);
    }
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
        EXPECT_EQ(77u, m.squares[0].light[i]);
    }
    EXPECT_EQ(-9, m.squares[0].y[0]);
    EXPECT_EQ(-9, m.squares[0].y[1]);
    EXPECT_EQ(32 - 9, m.squares[0].y[2]);
    EXPECT_EQ(32 - 9, m.squares[0].y[3]);
}

UTEST(ground, squares_are_written_row_after_row_from_the_top)
{
    MakeGround m;
    make_ground(&m);
    /* Squares 1 and 2 across, rows 0 and 1 seen, and the rows below
     * them that high ground could reach up from: all five. */
    ASSERT_EQ(2u * 5u, squares_for(&m, 32, 0, 64, 64));
    EXPECT_EQ(2u * 32, m.squares[0].tile_x);
    EXPECT_EQ(0u * 32, m.squares[0].tile_y);
    EXPECT_EQ(3u * 32, m.squares[1].tile_x);
    EXPECT_EQ(0u * 32, m.squares[1].tile_y);
    EXPECT_EQ(2u * 32, m.squares[2].tile_x);
    EXPECT_EQ(1u * 32, m.squares[2].tile_y);
    EXPECT_EQ(3u * 32, m.squares[9].tile_x);
    EXPECT_EQ(4u * 32, m.squares[9].tile_y);
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
    EXPECT_EQ(-1, m.squares[0].x);
    EXPECT_EQ(31, m.squares[1].x);
    EXPECT_EQ(63, m.squares[2].x);
    /* One pixel less wide, and square 3 is not seen. */
    EXPECT_EQ(2u * 5u, squares_for(&m, 33, 0, 63, 32));
    /* The camera's top moves every corner up the screen. */
    ASSERT_EQ(3u, squares_for(&m, 0, 4 * 32 + 6, 96, 20));
    EXPECT_EQ(-6, m.squares[0].y[0]);
    EXPECT_EQ(26, m.squares[0].y[2]);
}

UTEST(ground, squares_outside_the_map_give_nothing)
{
    MakeGround m;
    make_ground(&m);
    /* Half off the top left corner: column 0 alone, every row. */
    ASSERT_EQ(5u, squares_for(&m, -40, -40, 64, 64));
    EXPECT_EQ(40, m.squares[0].x);
    EXPECT_EQ(40, m.squares[0].y[0]);
    EXPECT_EQ(1u * 32, m.squares[0].tile_x);
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
    EXPECT_EQ(15u * 32, m.squares[0].tile_x);
    EXPECT_EQ(7u * 32, m.squares[0].tile_y);
    EXPECT_EQ(64, m.squares[0].x);
}

UTEST(ground, the_bits_above_the_tile_are_not_part_of_it)
{
    /* Bit 13 says a square cannot be walked on, and nothing about how
     * it looks. */
    MakeGround m;
    make_ground(&m);
    m.ground[4 * 6 + 1] |= 0x2000;
    ASSERT_EQ(1u, squares_for(&m, 32, 4 * 32, 32, 32));
    EXPECT_EQ(2u * 32, m.squares[0].tile_x);
    EXPECT_EQ(4u * 32, m.squares[0].tile_y);
}

/* The square under a point of a screen that shows the map from its
 * corner: its column plus 100 times its row, or -1 for none. */
static int32_t square_under(MakeGround *m, int32_t x, int32_t y)
{
    Camera camera = {0, 0, 192, 160};
    int32_t column = 77;
    int32_t row = 77;
    if (!ground_square_under(&m->map, &camera, x, y, &column, &row)) {
        return column == 77 && row == 77 ? -1 : -2;
    }
    return column + 100 * row;
}

UTEST(ground, on_flat_ground_the_square_under_a_point_is_the_one_there)
{
    MakeGround m;
    make_ground(&m);
    EXPECT_EQ(0, square_under(&m, 0, 0));
    EXPECT_EQ(2 + 100 * 1, square_under(&m, 64, 32));
    EXPECT_EQ(2 + 100 * 1, square_under(&m, 95, 63));
    EXPECT_EQ(3 + 100 * 2, square_under(&m, 96, 64));
    EXPECT_EQ(5 + 100 * 4, square_under(&m, 191, 159));
    /* Past the map there is none. */
    EXPECT_EQ(-1, square_under(&m, 192, 10));
    EXPECT_EQ(-1, square_under(&m, -1, 10));
    EXPECT_EQ(-1, square_under(&m, 10, 160));
    EXPECT_EQ(-1, square_under(&m, 10, -1));
    /* And the camera counts. */
    Camera camera = {64, 32, 100, 100};
    int32_t column = 0;
    int32_t row = 0;
    ASSERT_TRUE(ground_square_under(&m.map, &camera, 5, 5, &column, &row));
    EXPECT_EQ(2, column);
    EXPECT_EQ(1, row);
}

UTEST(ground, raised_ground_is_under_a_point_higher_up_the_screen)
{
    MakeGround m;
    make_ground(&m);
    /* The whole map is raised 40: a square is drawn 40 higher. */
    memset(m.heights, 40, sizeof m.heights);
    EXPECT_EQ(2 + 100 * 3, square_under(&m, 70, 3 * 32 - 40));
    EXPECT_EQ(2 + 100 * 2, square_under(&m, 70, 3 * 32 - 41));
    /* The top row is drawn above the screen's first line now, and the
     * last row ends 40 above where it did. */
    EXPECT_EQ(2 + 100 * 1, square_under(&m, 70, 0));
    EXPECT_EQ(-1, square_under(&m, 70, 5 * 32 - 40));
    EXPECT_EQ(2 + 100 * 4, square_under(&m, 70, 5 * 32 - 41));
}

UTEST(ground, of_two_squares_drawn_over_a_point_the_one_in_front_is_under_it)
{
    MakeGround m;
    make_ground(&m);
    /* Row 3 stands up 60 at both its edges: it is drawn over the
     * place of row 1 and 2. */
    for (int x = 0; x < 6; x++) {
        m.heights[3 * 6 + x] = 60;
        m.heights[4 * 6 + x] = 60;
    }
    EXPECT_EQ(2 + 100 * 3, square_under(&m, 70, 3 * 32 - 50));
    EXPECT_EQ(2 + 100 * 3, square_under(&m, 70, 3 * 32 - 60));
    EXPECT_EQ(2 + 100 * 1, square_under(&m, 70, 3 * 32 - 61));
}

UTEST(ground, a_square_takes_its_mark_if_marks_are_given)
{
    MakeGround m;
    make_ground(&m);
    uint8_t marks[30] = {0};
    marks[1 * 6 + 2] = FRAME_MARK_HELD;
    m.marks = marks;
    /* The squares (2, 1) and (3, 1) come first, then those of the
     * rows below that could reach up. */
    ASSERT_EQ(8u, squares_for(&m, 2 * 32, 1 * 32, 64, 1));
    EXPECT_EQ((unsigned)FRAME_MARK_HELD, m.squares[0].mark);
    for (int i = 1; i < 8; i++) {
        EXPECT_EQ(0u, m.squares[i].mark);
    }
    m.marks = NULL;
    ASSERT_EQ(8u, squares_for(&m, 2 * 32, 1 * 32, 64, 1));
    EXPECT_EQ(0u, m.squares[0].mark);
}

/* The picture the square (x, y) shows at a tick, on a sheet as wide
 * as the real one, when the map names picture `named` for it. */
static unsigned picture_shown(int x, int y, unsigned named, uint64_t tick)
{
    MakeGround m;
    make_ground(&m);
    m.ground[y * 6 + x] = (uint16_t)(named << 4);
    Camera camera = {x * 32, y * 32, 32, 1};
    size_t count = ground_squares(&m.map, m.light, NULL, tick, &camera,
        52 * 32, SHEET_HEIGHT, m.squares);
    return count == 0 ? 9999 : m.squares[0].tile_x / 32u;
}

UTEST(ground, water_shows_the_next_of_its_four_frames_every_few_ticks)
{
    /* Picture 33 is the second picture of the first family of water,
     * and stays the second picture of each family in turn. The square
     * (0, 0) is no frame ahead. */
    const unsigned shown[5] = {33, 37, 41, 45, 33};
    for (unsigned step = 0; step < 5; step++) {
        uint64_t tick = step * GROUND_WATER_TICKS;
        EXPECT_EQ(shown[step], picture_shown(0, 0, 33, tick));
        EXPECT_EQ(shown[step], picture_shown(0, 0, 33, tick + 3));
    }
}

UTEST(ground, the_family_a_map_names_is_where_the_water_starts)
{
    EXPECT_EQ(32u, picture_shown(0, 0, 32, 0));
    EXPECT_EQ(36u, picture_shown(0, 0, 36, 0));
    EXPECT_EQ(47u, picture_shown(0, 0, 47, 0));
    /* The last family goes round to the first. */
    EXPECT_EQ(35u, picture_shown(0, 0, 47, GROUND_WATER_TICKS));
}

UTEST(ground, each_square_of_water_is_ahead_by_its_place)
{
    /* The square (x, y) is (x + 1) * y frames ahead: 2 on (1, 1), 6 on
     * (2, 2), which is 2 in a round of 4, and none in the top row. */
    EXPECT_EQ(32u, picture_shown(3, 0, 32, 0));
    EXPECT_EQ(40u, picture_shown(1, 1, 32, 0));
    EXPECT_EQ(40u, picture_shown(2, 2, 32, 0));
    EXPECT_EQ(36u, picture_shown(0, 1, 32, 0));
    EXPECT_EQ(44u, picture_shown(0, 3, 32, 0));
}

UTEST(ground, ground_that_is_not_water_does_not_move)
{
    for (uint64_t tick = 0; tick < 40; tick += 7) {
        EXPECT_EQ(31u, picture_shown(1, 1, 31, tick));
        EXPECT_EQ(48u, picture_shown(1, 1, 48, tick));
        EXPECT_EQ(0u, picture_shown(1, 1, 0, tick));
    }
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
    EXPECT_EQ(4u * 7u, ground_most(&camera));
    for (int32_t left = -40; left < 240; left += 7) {
        for (int32_t top = -200; top < 200; top += 5) {
            EXPECT_NE(9999u, squares_for(&m, left, top, 70, 40));
        }
    }
}

UTEST(ground, the_height_of_a_square_is_the_mean_of_its_corners_cut_down)
{
    MakeGround m;
    make_ground(&m);
    m.heights[1 * 6 + 1] = 0;
    m.heights[1 * 6 + 2] = 64;
    m.heights[2 * 6 + 1] = 32;
    m.heights[2 * 6 + 2] = 96;
    EXPECT_EQ(48, ground_square_height(&m.map, 1, 1));
    /* The square to its right has two of those corners and two that
     * are down. */
    EXPECT_EQ(40, ground_square_height(&m.map, 2, 1));
    /* A mean that is not whole is cut down: 195 by 4. */
    m.heights[1 * 6 + 1] = 3;
    EXPECT_EQ(48, ground_square_height(&m.map, 1, 1));
}

UTEST(ground, the_height_at_a_point_goes_evenly_between_the_corners)
{
    MakeGround m;
    make_ground(&m);
    /* The square (1, 1): 0 and 64 on top, 32 and 96 below. */
    m.heights[1 * 6 + 1] = 0;
    m.heights[1 * 6 + 2] = 64;
    m.heights[2 * 6 + 1] = 32;
    m.heights[2 * 6 + 2] = 96;
    EXPECT_EQ(0, ground_height_at(&m.map, 32, 32));
    EXPECT_EQ(64, ground_height_at(&m.map, 64, 32));
    EXPECT_EQ(96, ground_height_at(&m.map, 64, 64));
    EXPECT_EQ(32, ground_height_at(&m.map, 48, 32));
    EXPECT_EQ(16, ground_height_at(&m.map, 32, 48));
    EXPECT_EQ(48, ground_height_at(&m.map, 48, 48));
    EXPECT_EQ(16, ground_height_at(&m.map, 40, 32));
    EXPECT_EQ(62 + 31, ground_height_at(&m.map, 63, 63));
}

UTEST(ground, the_height_at_a_point_is_worked_out_as_the_original_does)
{
    /* Squares measured in the original game, each with the height it
     * raises a unit by that stands in the middle. The steps are cut
     * towards zero one after the other, which is not the mean of the
     * four cut down, nor the mean rounded. */
    const int cases[5][5] = {
        {59, 59, 55, 33, 52},   /* the mean is 51 and a half */
        {33, 32, 29, 32, 32},   /* 31 and a half */
        {28, 31, 29, 29, 29},   /* 29 and a quarter */
        {81, 60, 62, 56, 65},   /* 64 and three quarters */
        {39, 35, 33, 36, 36}    /* 35 and three quarters */
    };
    for (int i = 0; i < 5; i++) {
        MakeGround m;
        make_ground(&m);
        m.heights[1 * 6 + 1] = (uint8_t)cases[i][0];
        m.heights[1 * 6 + 2] = (uint8_t)cases[i][1];
        m.heights[2 * 6 + 1] = (uint8_t)cases[i][2];
        m.heights[2 * 6 + 2] = (uint8_t)cases[i][3];
        EXPECT_EQ(cases[i][4], ground_height_at(&m.map, 48, 48));
    }
}

UTEST(ground, the_height_outside_the_map_is_that_of_its_nearest_corner)
{
    MakeGround m;
    make_ground(&m);
    m.heights[0] = 40;
    EXPECT_EQ(40, ground_height_at(&m.map, -500, -500));
    EXPECT_EQ(40, ground_square_height(&m.map, -3, -3));
    m.heights[4 * 6 + 5] = 70;
    EXPECT_EQ(70, ground_height_at(&m.map, 9000, 9000));
    EXPECT_EQ(70, ground_square_height(&m.map, 90, 90));
}
