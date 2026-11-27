#include "view/ground.h"

#include <string.h>

#include "utest.h"

/* A map of 6 by 5 squares, built in memory. The square at (x, y) names
 * the tile in column x + 1 and row y of the sheet, so that a sprite
 * says which square it came from. */
typedef struct MakeGround {
    Map map;
    uint16_t ground[30];
    Sprite sprites[64];
} MakeGround;

static void make_ground(MakeGround *m)
{
    memset(m, 0, sizeof *m);
    m->map.info.width = 6;
    m->map.info.height = 5;
    m->map.ground = m->ground;
    for (uint32_t y = 0; y < 5; y++) {
        for (uint32_t x = 0; x < 6; x++) {
            m->ground[y * 6 + x] = (uint16_t)(((x + 1) << 4) | y);
        }
    }
}

/* A sheet of 16 columns and 8 rows of tiles. */
#define SHEET_WIDTH (16 * 32)
#define SHEET_HEIGHT (8 * 32)

static size_t sprites_for(MakeGround *m, int32_t left, int32_t top,
                          int32_t width, int32_t height)
{
    Camera camera = {left, top, width, height};
    size_t count = ground_sprites(&m->map, &camera, 7, SHEET_WIDTH,
        SHEET_HEIGHT, m->sprites);
    /* Never more than it says it may write. */
    return count <= ground_most(&camera) ? count : 9999;
}

UTEST(ground, a_camera_on_the_squares_gives_a_sprite_for_each)
{
    MakeGround m;
    make_ground(&m);
    /* Squares 1 and 2 across, 2 and 3 down. */
    ASSERT_EQ(4u, sprites_for(&m, 32, 64, 64, 64));
    const Sprite *first = &m.sprites[0];
    EXPECT_EQ(7u, first->sheet);
    EXPECT_EQ(2u * 32, first->source_x);
    EXPECT_EQ(2u * 32, first->source_y);
    EXPECT_EQ(32u, first->width);
    EXPECT_EQ(32u, first->height);
    EXPECT_EQ(0, first->x);
    EXPECT_EQ(0, first->y);
    /* Row after row from the top, each from the left. */
    EXPECT_EQ(3u * 32, m.sprites[1].source_x);
    EXPECT_EQ(2u * 32, m.sprites[1].source_y);
    EXPECT_EQ(32, m.sprites[1].x);
    EXPECT_EQ(0, m.sprites[1].y);
    EXPECT_EQ(2u * 32, m.sprites[2].source_x);
    EXPECT_EQ(3u * 32, m.sprites[2].source_y);
    EXPECT_EQ(0, m.sprites[2].x);
    EXPECT_EQ(32, m.sprites[2].y);
    EXPECT_EQ(32, m.sprites[3].x);
    EXPECT_EQ(32, m.sprites[3].y);
}

UTEST(ground, a_camera_between_squares_shows_a_part_of_one_more)
{
    MakeGround m;
    make_ground(&m);
    /* From pixel 33 to 96 across: squares 1, 2 and the first pixel of
     * 3. From 70 to 101 down: squares 2 and 3. */
    ASSERT_EQ(6u, sprites_for(&m, 33, 70, 64, 32));
    EXPECT_EQ(-1, m.sprites[0].x);
    EXPECT_EQ(-6, m.sprites[0].y);
    EXPECT_EQ(31, m.sprites[1].x);
    EXPECT_EQ(63, m.sprites[2].x);
    EXPECT_EQ(26, m.sprites[3].y);
    /* One pixel less wide, and square 3 is not seen. */
    EXPECT_EQ(4u, sprites_for(&m, 33, 70, 63, 32));
}

UTEST(ground, squares_outside_the_map_give_nothing)
{
    MakeGround m;
    make_ground(&m);
    /* Half off the top left corner: only square (0, 0) is seen. */
    ASSERT_EQ(1u, sprites_for(&m, -40, -40, 64, 64));
    EXPECT_EQ(1u * 32, m.sprites[0].source_x);
    EXPECT_EQ(0u, m.sprites[0].source_y);
    EXPECT_EQ(40, m.sprites[0].x);
    EXPECT_EQ(40, m.sprites[0].y);
    /* Half off the bottom right corner: only square (5, 4). */
    ASSERT_EQ(1u, sprites_for(&m, 5 * 32 + 8, 4 * 32 + 8, 200, 200));
    EXPECT_EQ(6u * 32, m.sprites[0].source_x);
    EXPECT_EQ(4u * 32, m.sprites[0].source_y);
    EXPECT_EQ(-8, m.sprites[0].x);
    EXPECT_EQ(-8, m.sprites[0].y);
    /* Wholly off the map, on each side. */
    EXPECT_EQ(0u, sprites_for(&m, -64, 0, 64, 64));
    EXPECT_EQ(0u, sprites_for(&m, 0, -64, 64, 64));
    EXPECT_EQ(0u, sprites_for(&m, 6 * 32, 0, 64, 64));
    EXPECT_EQ(0u, sprites_for(&m, 0, 5 * 32, 64, 64));
    /* One pixel onto it from the left and from above. */
    EXPECT_EQ(2u, sprites_for(&m, -63, 0, 64, 64));
    EXPECT_EQ(2u, sprites_for(&m, 0, -63, 64, 64));
}

UTEST(ground, a_camera_over_the_whole_map_gives_every_square)
{
    MakeGround m;
    make_ground(&m);
    EXPECT_EQ(30u, sprites_for(&m, -10, -10, 400, 400));
    EXPECT_EQ(1u * 32, m.sprites[0].source_x);
    EXPECT_EQ(6u * 32, m.sprites[29].source_x);
    EXPECT_EQ(4u * 32, m.sprites[29].source_y);
}

UTEST(ground, a_tile_the_sheet_does_not_have_is_left_out)
{
    MakeGround m;
    make_ground(&m);
    m.ground[2 * 6 + 1] = (16 << 4) | 0;    /* one column too far */
    m.ground[2 * 6 + 2] = (1 << 4) | 8;     /* one row too far */
    m.ground[3 * 6 + 1] = (15 << 4) | 7;    /* the last there is */
    ASSERT_EQ(2u, sprites_for(&m, 32, 64, 64, 64));
    EXPECT_EQ(15u * 32, m.sprites[0].source_x);
    EXPECT_EQ(7u * 32, m.sprites[0].source_y);
    EXPECT_EQ(0, m.sprites[0].x);
    EXPECT_EQ(32, m.sprites[0].y);
}

UTEST(ground, the_bits_above_the_tile_are_not_part_of_it)
{
    /* Bit 13 says a square cannot be walked on, and nothing about how
     * it looks. */
    MakeGround m;
    make_ground(&m);
    m.ground[2 * 6 + 1] |= 0x2000;
    ASSERT_EQ(1u, sprites_for(&m, 32, 64, 32, 32));
    EXPECT_EQ(2u * 32, m.sprites[0].source_x);
    EXPECT_EQ(2u * 32, m.sprites[0].source_y);
}

UTEST(ground, a_camera_with_no_size_sees_nothing)
{
    MakeGround m;
    make_ground(&m);
    EXPECT_EQ(0u, sprites_for(&m, 32, 32, 0, 64));
    EXPECT_EQ(0u, sprites_for(&m, 32, 32, 64, 0));
    EXPECT_EQ(0u, sprites_for(&m, 32, 32, -5, 64));
    Camera none = {0, 0, 0, 64};
    EXPECT_EQ(0u, ground_most(&none));
}

UTEST(ground, most_is_enough_for_any_place_of_the_camera)
{
    MakeGround m;
    make_ground(&m);
    Camera camera = {0, 0, 70, 40};
    EXPECT_EQ(4u * 3u, ground_most(&camera));
    for (int32_t left = -40; left < 240; left += 7) {
        for (int32_t top = -40; top < 200; top += 5) {
            EXPECT_NE(9999u, sprites_for(&m, left, top, 70, 40));
        }
    }
}
