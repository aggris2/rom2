#include "view/light.h"

#include <string.h>

#include "utest.h"

/* A map of 5 by 5 squares with the sun where most maps have it, a
 * quarter of a half turn. Its heights are set by each test. */
typedef struct MakeLight {
    Map map;
    uint8_t heights[25];
    uint8_t levels[25];
} MakeLight;

static void make_light(MakeLight *m, float sun_angle)
{
    memset(m, 0, sizeof *m);
    m->map.info.width = 5;
    m->map.info.height = 5;
    m->map.info.sun_angle = sun_angle;
    m->map.heights = m->heights;
}

#define QUARTER 0.78539816f

UTEST(light, flat_ground_has_the_flat_level_at_any_height)
{
    MakeLight m;
    make_light(&m, QUARTER);
    memset(m.heights, 40, sizeof m.heights);
    light_ground(&m.map, m.levels);
    for (int i = 0; i < 25; i++) {
        EXPECT_EQ(LIGHT_FLAT, m.levels[i]);
    }
}

UTEST(light, ground_that_falls_towards_the_bottom_is_brighter)
{
    /* Each row is 16 lower than the one above it. */
    MakeLight m;
    make_light(&m, QUARTER);
    for (int i = 0; i < 25; i++) {
        m.heights[i] = (uint8_t)(100 - 16 * (i / 5));
    }
    light_ground(&m.map, m.levels);
    EXPECT_EQ(59u, m.levels[2 * 5 + 2]);
}

UTEST(light, ground_that_rises_towards_the_bottom_is_darker)
{
    MakeLight m;
    make_light(&m, QUARTER);
    for (int i = 0; i < 25; i++) {
        m.heights[i] = (uint8_t)(16 * (i / 5));
    }
    light_ground(&m.map, m.levels);
    EXPECT_EQ(40u, m.levels[2 * 5 + 2]);
}

UTEST(light, the_side_the_sun_is_on_decides_which_corner_is_looked_at)
{
    /* One raised corner, a square right and down of the middle. With
     * the sun on one side the middle does not look at it and stays
     * flat. With the sun on the other it does, and is brighter. */
    MakeLight one_side;
    MakeLight other_side;
    make_light(&one_side, QUARTER);
    make_light(&other_side, -QUARTER);
    one_side.heights[3 * 5 + 3] = 60;
    other_side.heights[3 * 5 + 3] = 60;
    light_ground(&one_side.map, one_side.levels);
    light_ground(&other_side.map, other_side.levels);
    EXPECT_EQ(LIGHT_FLAT, one_side.levels[2 * 5 + 2]);
    EXPECT_EQ(58u, other_side.levels[2 * 5 + 2]);
}

UTEST(light, a_cliff_stays_within_the_levels_there_are)
{
    MakeLight m;
    make_light(&m, QUARTER);
    for (int i = 0; i < 25; i++) {
        m.heights[i] = (i / 5) % 2 == 0 ? 127 : 0;
    }
    light_ground(&m.map, m.levels);
    for (int i = 0; i < 25; i++) {
        EXPECT_GE(m.levels[i], 2u);
        EXPECT_LE(m.levels[i], LIGHT_MAX);
    }
}

UTEST(light, a_corner_at_the_edge_takes_the_nearest_height_inside)
{
    /* One raised corner in the middle: the corners at the edge of the
     * map, which look outside it, stay flat. */
    MakeLight m;
    make_light(&m, QUARTER);
    m.heights[2 * 5 + 2] = 60;
    light_ground(&m.map, m.levels);
    EXPECT_EQ(LIGHT_FLAT, m.levels[0]);
    EXPECT_EQ(LIGHT_FLAT, m.levels[4]);
    EXPECT_EQ(LIGHT_FLAT, m.levels[20]);
    EXPECT_EQ(LIGHT_FLAT, m.levels[24]);
    EXPECT_NE(LIGHT_FLAT, m.levels[2 * 5 + 2]);
}
