#include "view/structures.h"

#include <string.h>

#include "utest.h"
#include "view/light.h"

/* A map of 10 by 10 squares, flat, with room for a few structures, and
 * two kinds: kind 12 is a tower on 2 by 2 squares whose picture is 3
 * squares high, and kind 8 a well on one square and no higher. */
typedef struct MakeStructures {
    Map map;
    uint8_t heights[100];
    MapStructure structures[4];
    StructureLook looks[STRUCTURE_KINDS];
    uint64_t tick;
    Sprite sprites[16];
} MakeStructures;

static void make_structures(MakeStructures *m)
{
    memset(m, 0, sizeof *m);
    m->map.info.width = 10;
    m->map.info.height = 10;
    m->map.heights = m->heights;
    m->map.structures = m->structures;
    m->looks[12].known = true;
    m->looks[12].sheet = 120;
    m->looks[12].width = 2;
    m->looks[12].depth = 2;
    m->looks[12].height = 3;
    m->looks[8].known = true;
    m->looks[8].sheet = 80;
    m->looks[8].width = 1;
    m->looks[8].depth = 1;
    m->looks[8].height = 1;
}

/* Puts a structure of kind `kind` with its base's top left square at
 * (x, y). */
static MapStructure *put(MakeStructures *m, int x, int y, uint32_t kind)
{
    MapStructure *structure = &m->structures[m->map.structure_count++];
    structure->x = x;
    structure->y = y;
    structure->type = kind;
    return structure;
}

/* The sprites for a camera, or 9999 if more were written than
 * structures_most() allows. */
static size_t sprites_for(MakeStructures *m, int32_t left, int32_t top,
                          int32_t width, int32_t height)
{
    Camera camera = {left, top, width, height};
    size_t count = structures_sprites(&m->map, m->looks, m->tick, &camera,
        m->sprites);
    return count <= structures_most(&m->map, m->looks) ? count : 9999;
}

UTEST(structures, a_structure_is_a_strip_for_each_row_of_its_base)
{
    MakeStructures m;
    make_structures(&m);
    put(&m, 4, 5, 12);
    ASSERT_EQ(2u, sprites_for(&m, 100, 100, 200, 200));
    /* The back row, with the square of picture that rises above it. */
    const Sprite *back = &m.sprites[0];
    EXPECT_EQ(120u, back->sheet);
    EXPECT_EQ(0u, back->source_x);
    EXPECT_EQ(0u, back->source_y);
    EXPECT_EQ(64u, back->width);
    EXPECT_EQ(64u, back->height);
    EXPECT_EQ(4 * 32 - 100, back->x);
    EXPECT_EQ(4 * 32 - 100, back->y);
    /* The front row: the lowest square of the picture. */
    const Sprite *front = &m.sprites[1];
    EXPECT_EQ(120u, front->sheet);
    EXPECT_EQ(0u, front->source_x);
    EXPECT_EQ(64u, front->source_y);
    EXPECT_EQ(64u, front->width);
    EXPECT_EQ(32u, front->height);
    EXPECT_EQ(4 * 32 - 100, front->x);
    EXPECT_EQ(6 * 32 - 100, front->y);
    /* By day, as bright as a tree. */
    EXPECT_EQ((unsigned)LIGHT_STANDING, back->light);
    EXPECT_EQ((unsigned)LIGHT_STANDING, front->light);
}

UTEST(structures, a_structure_no_higher_than_its_base_is_its_squares)
{
    MakeStructures m;
    make_structures(&m);
    put(&m, 2, 3, 8);
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(0u, m.sprites[0].source_y);
    EXPECT_EQ(32u, m.sprites[0].width);
    EXPECT_EQ(32u, m.sprites[0].height);
    EXPECT_EQ(2 * 32, m.sprites[0].x);
    EXPECT_EQ(3 * 32, m.sprites[0].y);
}

UTEST(structures, a_strip_takes_its_turn_between_its_neighbours_in_the_row)
{
    MakeStructures m;
    make_structures(&m);
    put(&m, 4, 5, 12);
    ASSERT_EQ(2u, sprites_for(&m, 0, 0, 320, 320));
    /* The tower's base is squares 4 and 5 of rows 5 and 6. */
    EXPECT_LT(sprite_order(6, 5), m.sprites[0].order);
    EXPECT_LT(m.sprites[0].order, sprite_order(3, 5));
    EXPECT_LT(sprite_order(6, 6), m.sprites[1].order);
    EXPECT_LT(m.sprites[1].order, sprite_order(3, 6));
    /* What stands in the row above the base comes before all of the
     * tower, whose top rises over it. */
    EXPECT_LT(sprite_order(0, 4), m.sprites[0].order);
}

UTEST(structures, a_structure_is_raised_by_the_ground_under_its_middle)
{
    MakeStructures m;
    make_structures(&m);
    put(&m, 4, 5, 12);
    /* The base is 2 by 2 squares, so its middle is the corner (5, 6).
     * The other corners of the base do not count. */
    m.heights[6 * 10 + 5] = 30;
    m.heights[5 * 10 + 4] = 90;
    m.heights[5 * 10 + 5] = 80;
    m.heights[6 * 10 + 4] = 70;
    m.heights[7 * 10 + 6] = 60;
    ASSERT_EQ(2u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(4 * 32 - 30, m.sprites[0].y);
    EXPECT_EQ(6 * 32 - 30, m.sprites[1].y);
}

UTEST(structures, the_middle_of_an_odd_base_lies_between_corners)
{
    MakeStructures m;
    make_structures(&m);
    /* A base of one square: the height in the middle of the square,
     * as measured on a campfire in the original game. */
    put(&m, 1, 1, 8);
    m.heights[1 * 10 + 1] = 39;
    m.heights[1 * 10 + 2] = 35;
    m.heights[2 * 10 + 1] = 33;
    m.heights[2 * 10 + 2] = 36;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(1 * 32 - 36, m.sprites[0].y);
    /* A base of 3 by 2 squares: its middle is halfway between the
     * corners (6, 6) and (7, 6), as measured on a hut. */
    m.looks[12].width = 3;
    put(&m, 5, 5, 12);
    m.heights[6 * 10 + 6] = 34;
    m.heights[6 * 10 + 7] = 35;
    m.heights[5 * 10 + 6] = 99;
    m.heights[7 * 10 + 6] = 99;
    ASSERT_EQ(3u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(5 * 32 - 32 - 34, m.sprites[1].y);
    EXPECT_EQ(6 * 32 - 34, m.sprites[2].y);
}

UTEST(structures, a_structure_that_moves_shows_the_frame_of_the_tick)
{
    MakeStructures m;
    make_structures(&m);
    /* The tower turns its light: frame 1 for two ticks, then 0. */
    m.looks[12].round.length = 3;
    m.looks[12].round.frame[0] = 1;
    m.looks[12].round.frame[1] = 1;
    m.looks[12].round.frame[2] = 0;
    put(&m, 4, 5, 12);
    put(&m, 1, 1, 12);
    const uint16_t frames[6] = {1, 1, 0, 1, 1, 0};
    for (uint64_t tick = 0; tick < 6; tick++) {
        m.tick = tick;
        ASSERT_EQ(4u, sprites_for(&m, 0, 0, 320, 320));
        /* Every strip of it, and every tower alike: a frame is as
         * wide as the base, 64 pixels. */
        for (int i = 0; i < 4; i++) {
            EXPECT_EQ(frames[tick] * 64, m.sprites[i].source_x);
        }
        EXPECT_EQ(0u, m.sprites[0].source_y);
        EXPECT_EQ(64u, m.sprites[1].source_y);
    }
    /* The well does not move. */
    put(&m, 8, 8, 8);
    ASSERT_EQ(5u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(0u, m.sprites[4].source_x);
}

UTEST(structures, a_strip_off_the_screen_is_left_out)
{
    MakeStructures m;
    make_structures(&m);
    put(&m, 4, 5, 12);
    /* The screen ends where the front row starts. */
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 6 * 32));
    EXPECT_EQ(0u, m.sprites[0].source_y);
    /* It starts there. */
    ASSERT_EQ(1u, sprites_for(&m, 0, 6 * 32, 320, 100));
    EXPECT_EQ(64u, m.sprites[0].source_y);
    /* Beside the tower, on each side, and one pixel onto it. */
    EXPECT_EQ(0u, sprites_for(&m, 6 * 32, 0, 100, 320));
    EXPECT_EQ(0u, sprites_for(&m, 0, 0, 4 * 32, 320));
    EXPECT_EQ(2u, sprites_for(&m, 6 * 32 - 1, 0, 100, 320));
    EXPECT_EQ(2u, sprites_for(&m, 0, 0, 4 * 32 + 1, 320));
    /* Above it and below it. */
    EXPECT_EQ(0u, sprites_for(&m, 0, 0, 320, 4 * 32));
    EXPECT_EQ(0u, sprites_for(&m, 0, 7 * 32, 320, 100));
}

UTEST(structures, a_kind_that_is_not_known_is_not_drawn)
{
    MakeStructures m;
    make_structures(&m);
    put(&m, 1, 1, 13);
    put(&m, 3, 3, 7000);
    put(&m, 5, 5, 8);
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(80u, m.sprites[0].sheet);
    EXPECT_EQ(1u, structures_most(&m.map, m.looks));
}

UTEST(structures, a_bridge_of_variable_size_is_not_drawn)
{
    MakeStructures m;
    make_structures(&m);
    MapStructure *bridge = put(&m, 2, 2, 8);
    bridge->bridge_width = 3;
    bridge->bridge_height = 1;
    EXPECT_EQ(0u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(0u, structures_most(&m.map, m.looks));
}

UTEST(structures, most_is_a_strip_for_each_row_of_each_base)
{
    MakeStructures m;
    make_structures(&m);
    put(&m, 1, 1, 12);
    put(&m, 5, 5, 12);
    put(&m, 8, 8, 8);
    EXPECT_EQ(5u, structures_most(&m.map, m.looks));
    EXPECT_EQ(5u, sprites_for(&m, 0, 0, 320, 320));
}
