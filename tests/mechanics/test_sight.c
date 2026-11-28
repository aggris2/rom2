#include "mechanics/sight.h"

#include <string.h>

#include "utest.h"

/* Ground of 41 by 41 squares, flat until a test raises some, and what
 * is seen on it. */
#define SIDE 41
#define MIDDLE 20

typedef struct MakeSight {
    Terrain terrain;
    uint8_t raised[SIDE * SIDE];
    uint8_t seen[SIDE * SIDE + 1];
} MakeSight;

static void make_sight(MakeSight *m)
{
    memset(m, 0, sizeof *m);
    memset(m->raised, 50, sizeof m->raised);
    m->terrain.width = SIDE;
    m->terrain.height = SIDE;
    m->terrain.raised = m->raised;
}

/* Whether the square `across` and `down` from the middle is seen. */
static bool seen_at(const MakeSight *m, int across, int down)
{
    return m->seen[(MIDDLE + down) * SIDE + MIDDLE + across] == 1;
}

static void raise(MakeSight *m, int across, int down, int height)
{
    m->raised[(MIDDLE + down) * SIDE + MIDDLE + across] = (uint8_t)height;
}

static int count(const MakeSight *m)
{
    int seen = 0;
    for (int i = 0; i < SIDE * SIDE; i++) {
        seen += m->seen[i] == 1 ? 1 : 0;
    }
    return seen;
}

UTEST(sight, on_flat_ground_a_sight_of_two_sees_two_squares_off)
{
    MakeSight m;
    make_sight(&m);
    sight_mark(&m.terrain, MIDDLE, MIDDLE, 2, m.seen);
    /* Five by five without the corners: a slant is longer. */
    EXPECT_EQ(21, count(&m));
    EXPECT_TRUE(seen_at(&m, 0, 0));
    EXPECT_TRUE(seen_at(&m, 2, 0));
    EXPECT_TRUE(seen_at(&m, 0, -2));
    EXPECT_TRUE(seen_at(&m, -2, 1));
    EXPECT_TRUE(seen_at(&m, 1, 1));
    EXPECT_FALSE(seen_at(&m, 2, 2));
    EXPECT_FALSE(seen_at(&m, -2, -2));
    EXPECT_FALSE(seen_at(&m, 3, 0));
}

UTEST(sight, what_is_seen_is_the_same_to_the_left_and_right_and_up_and_down)
{
    MakeSight m;
    make_sight(&m);
    sight_mark(&m.terrain, MIDDLE, MIDDLE, 7, m.seen);
    for (int down = -10; down <= 10; down++) {
        for (int across = -10; across <= 10; across++) {
            bool here = seen_at(&m, across, down);
            EXPECT_EQ(here, seen_at(&m, -across, down));
            EXPECT_EQ(here, seen_at(&m, across, -down));
        }
    }
    EXPECT_TRUE(seen_at(&m, 7, 0));
    EXPECT_FALSE(seen_at(&m, 8, 0));
}

UTEST(sight, no_sight_at_all_sees_the_eyes_own_square_alone)
{
    MakeSight m;
    make_sight(&m);
    sight_mark(&m.terrain, MIDDLE, MIDDLE, 0, m.seen);
    EXPECT_EQ(1, count(&m));
    EXPECT_TRUE(seen_at(&m, 0, 0));
}

UTEST(sight, higher_ground_uses_sight_up_and_hides_what_is_behind)
{
    MakeSight m;
    make_sight(&m);
    /* A wall two squares off, a square's worth higher than the eye:
     * 320 less 128 leaves 192 beside the eye, and the wall takes 128
     * and 128 more. */
    raise(&m, 2, 0, 50 + SIGHT_SQUARE);
    sight_mark(&m.terrain, MIDDLE, MIDDLE, 4, m.seen);
    EXPECT_TRUE(seen_at(&m, 1, 0));
    EXPECT_TRUE(seen_at(&m, 2, 0));
    EXPECT_TRUE(seen_at(&m, 3, 0));
    EXPECT_FALSE(seen_at(&m, 4, 0));
    /* The other way nothing is in the way. */
    EXPECT_TRUE(seen_at(&m, -4, 0));
}

UTEST(sight, a_rise_too_high_is_not_seen_itself)
{
    MakeSight m;
    make_sight(&m);
    raise(&m, 1, 0, 255);
    sight_mark(&m.terrain, MIDDLE, MIDDLE, 1, m.seen);
    /* 192 less 128 less 205. */
    EXPECT_FALSE(seen_at(&m, 1, 0));
    EXPECT_TRUE(seen_at(&m, -1, 0));
}

UTEST(sight, lower_ground_gives_sight_back)
{
    MakeSight m;
    make_sight(&m);
    /* A dip of 40 three squares off: 64 is left two off, and the
     * square in the dip costs 128 less 40. */
    sight_mark(&m.terrain, MIDDLE, MIDDLE, 2, m.seen);
    EXPECT_FALSE(seen_at(&m, 3, 0));
    raise(&m, 3, 0, 10);
    memset(m.seen, 0, sizeof m.seen);
    sight_mark(&m.terrain, MIDDLE, MIDDLE, 2, m.seen);
    EXPECT_FALSE(seen_at(&m, 3, 0));
    raise(&m, 3, 0, 0);
    raise(&m, 2, 0, 30);
    memset(m.seen, 0, sizeof m.seen);
    sight_mark(&m.terrain, MIDDLE, MIDDLE, 2, m.seen);
    /* 84 two off, then 128 less 50 from it. */
    EXPECT_TRUE(seen_at(&m, 3, 0));
}

UTEST(sight, nothing_is_seen_further_off_than_the_furthest)
{
    MakeSight m;
    make_sight(&m);
    sight_mark(&m.terrain, 0, MIDDLE, 200, m.seen);
    EXPECT_EQ(1u, m.seen[MIDDLE * SIDE + SIGHT_FURTHEST]);
    EXPECT_EQ(0u, m.seen[MIDDLE * SIDE + SIGHT_FURTHEST + 1]);
}

UTEST(sight, the_edge_of_the_ground_is_where_sight_ends)
{
    MakeSight m;
    make_sight(&m);
    m.seen[SIDE * SIDE] = 9;
    sight_mark(&m.terrain, 0, 0, 5, m.seen);
    EXPECT_EQ(1u, m.seen[0]);
    EXPECT_EQ(1u, m.seen[5]);
    EXPECT_EQ(1u, m.seen[5 * SIDE]);
    EXPECT_EQ(0u, m.seen[6]);
    sight_mark(&m.terrain, SIDE - 1, SIDE - 1, 5, m.seen);
    EXPECT_EQ(1u, m.seen[SIDE * SIDE - 1]);
    EXPECT_EQ(9u, m.seen[SIDE * SIDE]);
    /* An eye off the ground sees nothing. */
    memset(m.seen, 0, SIDE * SIDE);
    sight_mark(&m.terrain, -1, 3, 5, m.seen);
    sight_mark(&m.terrain, 3, SIDE, 5, m.seen);
    EXPECT_EQ(0, count(&m));
}

UTEST(sight, what_is_marked_already_stays_marked)
{
    MakeSight m;
    make_sight(&m);
    sight_mark(&m.terrain, 5, 5, 2, m.seen);
    sight_mark(&m.terrain, 30, 30, 2, m.seen);
    EXPECT_EQ(42, count(&m));
}
