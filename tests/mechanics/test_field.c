#include "mechanics/field.h"

#include <stdlib.h>
#include <string.h>

#include "utest.h"

/* A map of 12 by 10 squares of land that costs 8, all open. */
#define WIDE 12
#define TALL 10

typedef struct MakeField {
    Terrain terrain;
    uint8_t cost[WIDE * TALL];
    uint8_t closed[WIDE * TALL];
    uint8_t raised[WIDE * TALL];
    Fields fields;
    Field field;
} MakeField;

static void make_field(MakeField *m)
{
    memset(m, 0, sizeof *m);
    m->terrain.width = WIDE;
    m->terrain.height = TALL;
    m->terrain.cost = m->cost;
    m->terrain.closed = m->closed;
    m->terrain.raised = m->raised;
    memset(m->cost, 8, sizeof m->cost);
}

static void close_at(MakeField *m, int x, int y, uint8_t to)
{
    m->closed[y * WIDE + x] = to;
}

/* Spreads from a goal for what walks. */
static bool spread(MakeField *m, int x, int y)
{
    return field_spread(&m->fields, &m->field, &m->terrain,
        TERRAIN_NO_WALK, 1, x, y);
}

static void done(MakeField *m)
{
    free(m->field.far);
    fields_free(&m->fields);
}

UTEST(field, a_square_is_as_far_as_the_steps_to_the_goal_cost)
{
    static MakeField m;
    make_field(&m);
    ASSERT_TRUE(spread(&m, 5, 5));
    EXPECT_EQ(0u, field_far(&m.field, 5, 5));
    /* A straight step onto land of 8 costs 16, one to a corner 24. */
    EXPECT_EQ(16u, field_far(&m.field, 6, 5));
    EXPECT_EQ(16u, field_far(&m.field, 5, 4));
    EXPECT_EQ(24u, field_far(&m.field, 6, 6));
    EXPECT_EQ(24u, field_far(&m.field, 4, 4));
    EXPECT_EQ(3u * 16u, field_far(&m.field, 8, 5));
    /* Two to the corner and one straight. */
    EXPECT_EQ(2u * 24u + 16u, field_far(&m.field, 8, 3));
    EXPECT_EQ(5u * 24u, field_far(&m.field, 0, 0));
    done(&m);
}

UTEST(field, a_step_costs_by_the_square_stepped_onto)
{
    static MakeField m;
    make_field(&m);
    /* A road of 4 to the right of the goal, and sand of 14 to its
     * left. */
    m.cost[5 * WIDE + 6] = 4;
    m.cost[5 * WIDE + 7] = 4;
    m.cost[5 * WIDE + 4] = 14;
    ASSERT_TRUE(spread(&m, 5, 5));
    /* From the road onto the goal: the goal's 8. Then onto road. */
    EXPECT_EQ(16u, field_far(&m.field, 6, 5));
    EXPECT_EQ(16u + 8u, field_far(&m.field, 7, 5));
    EXPECT_EQ(16u + 8u + 8u, field_far(&m.field, 8, 5));
    /* From the sand onto the goal costs as the goal does, and from
     * beyond onto the sand as sand. */
    EXPECT_EQ(16u, field_far(&m.field, 4, 5));
    EXPECT_EQ(16u + 28u, field_far(&m.field, 3, 5));
    done(&m);
}

UTEST(field, the_way_round_a_wall_is_what_counts)
{
    static MakeField m;
    make_field(&m);
    /* A wall to the right of the goal, three squares tall. */
    close_at(&m, 6, 4, TERRAIN_NO_WALK);
    close_at(&m, 6, 5, TERRAIN_NO_WALK);
    close_at(&m, 6, 6, TERRAIN_NO_WALK);
    ASSERT_TRUE(spread(&m, 5, 5));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 6, 5));
    /* Behind it: up, round the top by its two corners, and down. */
    EXPECT_EQ(16u + 24u + 24u + 16u, field_far(&m.field, 7, 5));
    done(&m);
}

UTEST(field, a_way_goes_to_a_corner_past_a_closed_square)
{
    static MakeField m;
    make_field(&m);
    close_at(&m, 6, 5, TERRAIN_NO_WALK);
    close_at(&m, 5, 4, TERRAIN_NO_WALK);
    ASSERT_TRUE(spread(&m, 5, 5));
    /* Up and to the right of the goal is one step away, though a
     * wall is on each side of that way: as in the original. */
    EXPECT_EQ(24u, field_far(&m.field, 6, 4));
    EXPECT_EQ(24u, field_far(&m.field, 6, 6));
    EXPECT_EQ(24u, field_far(&m.field, 4, 4));
    done(&m);
}

UTEST(field, a_large_unit_needs_room_for_all_of_it)
{
    static MakeField m;
    make_field(&m);
    /* A wall with a gap one square wide, and one two wide lower. */
    for (int y = 0; y < TALL; y++) {
        close_at(&m, 6, y, TERRAIN_NO_WALK);
    }
    close_at(&m, 6, 2, 0);
    close_at(&m, 6, 6, 0);
    close_at(&m, 6, 7, 0);
    ASSERT_TRUE(field_spread(&m.fields, &m.field, &m.terrain,
        TERRAIN_NO_WALK, 2, 3, 2));
    /* By its top left square: it cannot stand with that in the wall,
     * nor beside the wall on the left, where its right half would be
     * in it. */
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 6, 2));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 5, 2));
    EXPECT_EQ(0u, field_far(&m.field, 3, 2));
    EXPECT_EQ(16u, field_far(&m.field, 4, 2));
    /* Through the wide gap it gets to the other side, and not past
     * the last row or column it fits in. */
    EXPECT_NE(FIELD_FAR, field_far(&m.field, 6, 6));
    EXPECT_NE(FIELD_FAR, field_far(&m.field, 8, 2));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, WIDE - 1, 2));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 8, TALL - 1));
    /* A unit of one square takes the narrow gap, which is nearer. */
    uint32_t large = field_far(&m.field, 8, 2);
    ASSERT_TRUE(spread(&m, 3, 2));
    EXPECT_LT(field_far(&m.field, 8, 2), large);
    done(&m);
}

UTEST(field, a_place_the_goal_cannot_be_reached_from_is_far)
{
    static MakeField m;
    make_field(&m);
    /* Water right across the map. */
    for (int y = 0; y < TALL; y++) {
        close_at(&m, 8, y, TERRAIN_NO_WALK);
    }
    ASSERT_TRUE(spread(&m, 5, 5));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 8, 5));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 10, 5));
    EXPECT_EQ(2u * 16u, field_far(&m.field, 7, 5));
    /* Off the map is far too. */
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, -1, 5));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 5, TALL));
    done(&m);
}

UTEST(field, a_goal_that_is_closed_or_off_the_map_is_far_from_everything)
{
    static MakeField m;
    make_field(&m);
    close_at(&m, 5, 5, TERRAIN_NO_WALK);
    ASSERT_TRUE(spread(&m, 5, 5));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 5, 5));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 6, 5));
    ASSERT_TRUE(spread(&m, WIDE, 5));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, WIDE - 1, 5));
    ASSERT_TRUE(spread(&m, 5, -1));
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 5, 0));
    done(&m);
}

UTEST(field, what_hovers_crosses_water_at_one_price_for_every_square)
{
    static MakeField m;
    make_field(&m);
    for (int y = 0; y < TALL; y++) {
        close_at(&m, 8, y, TERRAIN_NO_WALK);
    }
    close_at(&m, 6, 5, TERRAIN_NO_WALK | TERRAIN_NO_HOVER);
    m.cost[5 * WIDE + 7] = 14;
    ASSERT_TRUE(field_spread(&m.fields, &m.field, &m.terrain,
        TERRAIN_NO_HOVER, 1, 5, 5));
    /* Round the rock by two steps to a corner. */
    EXPECT_EQ(FIELD_FAR, field_far(&m.field, 6, 5));
    EXPECT_EQ(2u * 3u, field_far(&m.field, 7, 5));
    /* From over the water, whatever the ground costs: a corner, a
     * straight step and a corner. */
    EXPECT_EQ(3u + 2u + 3u, field_far(&m.field, 8, 5));
    done(&m);
}

UTEST(field, every_square_that_can_reach_the_goal_has_a_nearer_one_beside_it)
{
    static MakeField m;
    make_field(&m);
    /* Walls and costs by a rule, so that the map is a tangle. */
    for (int i = 0; i < WIDE * TALL; i++) {
        m.cost[i] = (uint8_t)(4 + i * 7 % 13);
        m.closed[i] = i * 5 % 11 == 0 ? TERRAIN_NO_WALK : 0;
    }
    close_at(&m, 5, 5, 0);
    ASSERT_TRUE(spread(&m, 5, 5));
    int reached = 0;
    for (int y = 0; y < TALL; y++) {
        for (int x = 0; x < WIDE; x++) {
            uint32_t far = field_far(&m.field, x, y);
            if (far == FIELD_FAR || far == 0) {
                continue;
            }
            reached++;
            bool nearer = false;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    nearer = nearer
                        || field_far(&m.field, x + dx, y + dy) < far;
                }
            }
            EXPECT_TRUE(nearer);
        }
    }
    EXPECT_GT(reached, 60);
    done(&m);
}

UTEST(fields, units_with_one_goal_share_one_field)
{
    static MakeField m;
    make_field(&m);
    uint16_t first = 99;
    uint16_t second = 99;
    uint16_t other = 99;
    ASSERT_TRUE(fields_take(&m.fields, &m.terrain, TERRAIN_NO_WALK, 1, 5, 5,
        &first));
    ASSERT_TRUE(fields_take(&m.fields, &m.terrain, TERRAIN_NO_WALK, 1, 5, 5,
        &second));
    EXPECT_EQ(first, second);
    EXPECT_EQ(2u, m.fields.users[first]);
    EXPECT_EQ(16u, field_far(fields_get(&m.fields, first), 6, 5));
    /* Another goal, or the same for what hovers, is another field. */
    ASSERT_TRUE(fields_take(&m.fields, &m.terrain, TERRAIN_NO_WALK, 1, 2, 2,
        &other));
    EXPECT_NE(first, other);
    ASSERT_TRUE(fields_take(&m.fields, &m.terrain, TERRAIN_NO_HOVER, 1, 5, 5,
        &second));
    EXPECT_NE(first, second);
    EXPECT_NE(other, second);
    fields_free(&m.fields);
    EXPECT_TRUE(m.fields.next == NULL);
}

UTEST(fields, a_field_nobody_uses_is_made_anew_for_the_next_goal)
{
    static MakeField m;
    make_field(&m);
    uint16_t first = 99;
    uint16_t next = 99;
    ASSERT_TRUE(fields_take(&m.fields, &m.terrain, TERRAIN_NO_WALK, 1, 5, 5,
        &first));
    fields_drop(&m.fields, first);
    EXPECT_TRUE(fields_get(&m.fields, first) == NULL);
    ASSERT_TRUE(fields_take(&m.fields, &m.terrain, TERRAIN_NO_WALK, 1, 2, 2,
        &next));
    EXPECT_EQ(first, next);
    EXPECT_EQ(0u, field_far(fields_get(&m.fields, next), 2, 2));
    /* Dropping what is not in use does nothing. */
    fields_drop(&m.fields, 7);
    fields_drop(&m.fields, 9999);
    EXPECT_EQ(1u, m.fields.users[next]);
    fields_free(&m.fields);
}

UTEST(fields, when_all_are_in_use_no_more_is_given)
{
    static MakeField m;
    make_field(&m);
    uint16_t number = 99;
    for (int i = 0; i < FIELDS_MOST; i++) {
        ASSERT_TRUE(fields_take(&m.fields, &m.terrain, TERRAIN_NO_WALK, 1,
            i % WIDE, i / WIDE, &number));
    }
    number = 99;
    EXPECT_FALSE(fields_take(&m.fields, &m.terrain, TERRAIN_NO_WALK, 1, 11, 9,
        &number));
    EXPECT_EQ(99u, number);
    /* One that is there already can still be shared. */
    EXPECT_TRUE(fields_take(&m.fields, &m.terrain, TERRAIN_NO_WALK, 1, 0, 0,
        &number));
    fields_free(&m.fields);
}
