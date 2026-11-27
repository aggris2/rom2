#include "view/objects.h"

#include <string.h>

#include "utest.h"

/* A map of 10 by 10 squares, flat and empty until a test puts objects
 * on it, and two kinds of object: kind 3 is a rock of 32 by 32 with its
 * anchor in its middle, and kind 5 a tree of 64 by 96 with its anchor
 * near its foot. */
typedef struct MakeObjects {
    Map map;
    uint8_t objects[100];
    uint8_t heights[100];
    ObjectLook looks[OBJECT_KINDS];
    Sprite sprites[400];
} MakeObjects;

static void make_objects(MakeObjects *m)
{
    memset(m, 0, sizeof *m);
    m->map.info.width = 10;
    m->map.info.height = 10;
    m->map.objects = m->objects;
    m->map.heights = m->heights;
    m->looks[3].known = true;
    m->looks[3].sheet = 30;
    m->looks[3].cell_width = 32;
    m->looks[3].cell_height = 32;
    m->looks[3].anchor_x = 16;
    m->looks[3].anchor_y = 16;
    m->looks[5].known = true;
    m->looks[5].sheet = 50;
    m->looks[5].cell_width = 64;
    m->looks[5].cell_height = 96;
    m->looks[5].anchor_x = 32;
    m->looks[5].anchor_y = 80;
}

/* Puts an object of kind `kind` on the square (x, y). */
static void put(MakeObjects *m, int x, int y, int kind)
{
    m->objects[y * 10 + x] = (uint8_t)(kind + 1);
}

/* The sprites for a camera, or 9999 if more were written than
 * objects_most() allows. */
static size_t sprites_for(MakeObjects *m, int32_t left, int32_t top,
                          int32_t width, int32_t height)
{
    Camera camera = {left, top, width, height};
    size_t count = objects_sprites(&m->map, m->looks, &camera, m->sprites);
    return count <= objects_most(&camera, m->looks)
        && count <= sizeof m->sprites / sizeof m->sprites[0] ? count : 9999;
}

UTEST(objects, an_object_stands_with_its_anchor_on_the_middle_of_its_square)
{
    MakeObjects m;
    make_objects(&m);
    put(&m, 4, 5, 5);
    ASSERT_EQ(1u, sprites_for(&m, 100, 100, 200, 200));
    const Sprite *s = &m.sprites[0];
    EXPECT_EQ(50u, s->sheet);
    EXPECT_EQ(0u, s->source_x);
    EXPECT_EQ(0u, s->source_y);
    EXPECT_EQ(64u, s->width);
    EXPECT_EQ(96u, s->height);
    /* The square's middle is (144, 176) on the map. */
    EXPECT_EQ(144 - 32 - 100, s->x);
    EXPECT_EQ(176 - 80 - 100, s->y);
    /* By day, 13 sixteenths of twice the picture's colours. */
    EXPECT_EQ(52u, s->light);
    EXPECT_EQ(52, LIGHT_STANDING);
}

UTEST(objects, an_object_is_raised_by_the_ground_it_stands_on)
{
    MakeObjects m;
    make_objects(&m);
    put(&m, 4, 5, 3);
    /* The four corners of its square, all at 40. */
    m.heights[5 * 10 + 4] = 40;
    m.heights[5 * 10 + 5] = 40;
    m.heights[6 * 10 + 4] = 40;
    m.heights[6 * 10 + 5] = 40;
    ASSERT_EQ(1u, sprites_for(&m, 100, 100, 200, 200));
    EXPECT_EQ(144 - 16 - 100, m.sprites[0].x);
    EXPECT_EQ(176 - 16 - 100 - 40, m.sprites[0].y);
}

UTEST(objects, come_row_after_row_from_the_top_and_from_the_right)
{
    MakeObjects m;
    make_objects(&m);
    put(&m, 3, 6, 3);
    put(&m, 5, 4, 3);
    put(&m, 3, 4, 3);
    put(&m, 6, 6, 3);
    put(&m, 4, 5, 3);
    ASSERT_EQ(5u, sprites_for(&m, 64, 64, 256, 256));
    /* Row 4 from the right, then row 5, then row 6 from the right. */
    const int x[5] = {5, 3, 4, 6, 3};
    const int y[5] = {4, 4, 5, 6, 6};
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(x[i] * 32 + 16 - 16 - 64, m.sprites[i].x);
        EXPECT_EQ(y[i] * 32 + 16 - 16 - 64, m.sprites[i].y);
    }
}

UTEST(objects, a_kind_that_is_not_known_is_not_drawn)
{
    MakeObjects m;
    make_objects(&m);
    put(&m, 4, 5, 7);
    put(&m, 5, 5, 254);
    put(&m, 6, 5, 0);
    put(&m, 3, 5, 3);
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(30u, m.sprites[0].sheet);
}

UTEST(objects, an_object_beside_the_screen_that_reaches_in_is_drawn)
{
    MakeObjects m;
    make_objects(&m);
    /* A tree on square (4, 5): on the map it covers x 112 to 175 and y
     * 96 to 191. */
    put(&m, 4, 5, 5);
    /* The screen is one pixel wide and high, on each edge of it. */
    EXPECT_EQ(1u, sprites_for(&m, 112, 96, 1, 1));
    EXPECT_EQ(1u, sprites_for(&m, 175, 96, 1, 1));
    EXPECT_EQ(1u, sprites_for(&m, 112, 191, 1, 1));
    EXPECT_EQ(1u, sprites_for(&m, 175, 191, 1, 1));
    /* And one pixel off each of them. */
    EXPECT_EQ(0u, sprites_for(&m, 111, 96, 1, 1));
    EXPECT_EQ(0u, sprites_for(&m, 176, 96, 1, 1));
    EXPECT_EQ(0u, sprites_for(&m, 112, 95, 1, 1));
    EXPECT_EQ(0u, sprites_for(&m, 112, 192, 1, 1));
}

UTEST(objects, an_object_on_high_ground_below_the_screen_reaches_into_it)
{
    MakeObjects m;
    make_objects(&m);
    put(&m, 4, 9, 3);
    for (int i = 0; i < 100; i++) {
        m.heights[i] = 127;
    }
    /* The rock's square is 288 to 319 down the map, and it is drawn
     * 127 higher: from 161 to 192. */
    EXPECT_EQ(1u, sprites_for(&m, 128, 161, 32, 1));
    EXPECT_EQ(1u, sprites_for(&m, 128, 192, 32, 1));
    EXPECT_EQ(0u, sprites_for(&m, 128, 160, 32, 1));
    EXPECT_EQ(0u, sprites_for(&m, 128, 193, 32, 1));
}

UTEST(objects, objects_at_the_edges_of_the_map_are_drawn)
{
    MakeObjects m;
    make_objects(&m);
    put(&m, 0, 0, 3);
    put(&m, 9, 0, 3);
    put(&m, 0, 9, 3);
    put(&m, 9, 9, 3);
    EXPECT_EQ(4u, sprites_for(&m, -100, -100, 600, 600));
    EXPECT_EQ(1u, sprites_for(&m, -100, -100, 110, 110));
}

UTEST(objects, a_camera_with_no_size_sees_nothing)
{
    MakeObjects m;
    make_objects(&m);
    put(&m, 4, 5, 3);
    EXPECT_EQ(0u, sprites_for(&m, 100, 100, 0, 200));
    EXPECT_EQ(0u, sprites_for(&m, 100, 100, 200, 0));
    Camera none = {0, 0, 0, 100};
    EXPECT_EQ(0u, objects_most(&none, m.looks));
}

UTEST(objects, most_is_enough_when_every_square_has_an_object)
{
    MakeObjects m;
    make_objects(&m);
    for (int i = 0; i < 100; i++) {
        m.objects[i] = (uint8_t)(i % 2 == 0 ? 3 + 1 : 5 + 1);
    }
    for (int32_t left = -80; left < 400; left += 37) {
        for (int32_t top = -80; top < 400; top += 41) {
            EXPECT_NE(9999u, sprites_for(&m, left, top, 100, 70));
        }
    }
    EXPECT_EQ(100u, sprites_for(&m, -200, -200, 800, 800));
}
