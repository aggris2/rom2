#include "mechanics/walk.h"

#include <string.h>

#include "utest.h"

/* A map of SIDE squares each way: flat land of cost 8, open but for
 * its edge of one square, with an empty list of units on it. */
#define SIDE 48

typedef struct MakeWalk {
    Terrain terrain;
    uint8_t cost[SIDE * SIDE];
    uint8_t closed[SIDE * SIDE];
    uint8_t raised[SIDE * SIDE];
    Units units;
    Fields fields;
    Random random;
} MakeWalk;

static bool make_walk(MakeWalk *m)
{
    memset(m, 0, sizeof *m);
    m->terrain.width = SIDE;
    m->terrain.height = SIDE;
    m->terrain.cost = m->cost;
    m->terrain.closed = m->closed;
    m->terrain.raised = m->raised;
    memset(m->cost, 8, sizeof m->cost);
    for (int i = 0; i < SIDE; i++) {
        m->closed[i] = TERRAIN_NO_ONE;
        m->closed[(SIDE - 1) * SIDE + i] = TERRAIN_NO_ONE;
        m->closed[i * SIDE] = TERRAIN_NO_ONE;
        m->closed[i * SIDE + SIDE - 1] = TERRAIN_NO_ONE;
    }
    random_start(&m->random, 7);
    return units_make(SIDE, SIDE, &m->units);
}

/* Puts a walker of speed 18 that turns 16 a tick in the middle of the
 * square (x, y), facing up the map. */
static UnitId put(MakeWalk *m, int x, int y)
{
    Unit unit;
    UnitId id = {9999, 9999};
    memset(&unit, 0, sizeof unit);
    unit.x = x * 256 + 128;
    unit.y = y * 256 + 128;
    unit.size = 1;
    unit.mover = UNIT_WALKS;
    unit.speed = 18;
    unit.turn_speed = 16;
    units_add(&m->units, &unit, &id);
    return id;
}

static void tick(MakeWalk *m, int ticks)
{
    for (int i = 0; i < ticks; i++) {
        units_tick(&m->units, &m->terrain, &m->fields, &m->random);
    }
}

/* The square a unit is on: its x plus 100 times its y. */
static int32_t square_of(const MakeWalk *m, UnitId id)
{
    const Unit *unit = units_get(&m->units, id);
    return unit->x / 256 + 100 * (unit->y / 256);
}

/* The square a unit is stepping to, or is on if it stands: its x
 * plus 100 times its y. */
static int32_t heading_of(const MakeWalk *m, UnitId id)
{
    const Unit *unit = units_get(&m->units, id);
    return unit->reaching ? unit->to_x + 100 * unit->to_y
        : square_of(m, id);
}

/* The slot of the ground unit that holds a square, or -1. */
static int held_by(const MakeWalk *m, int x, int y)
{
    UnitId id = {9999, 9999};
    return units_standing(&m->units, false, x, y, &id) ? id.slot : -1;
}

UTEST(walk, the_eight_ways_go_round_from_up_the_map)
{
    const int across[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    const int down[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (uint8_t way = 0; way < WALK_WAYS; way++) {
        EXPECT_EQ(across[way], walk_across(way));
        EXPECT_EQ(down[way], walk_down(way));
        EXPECT_EQ(way * 32u, walk_facing(way));
    }
}

UTEST(walk, the_way_to_a_square_is_the_one_that_points_most_nearly_there)
{
    uint8_t way = 99;
    EXPECT_TRUE(walk_way_to(10, 10, 10, 3, &way));
    EXPECT_EQ(0u, way);
    EXPECT_TRUE(walk_way_to(10, 10, 14, 6, &way));
    EXPECT_EQ(1u, way);
    EXPECT_TRUE(walk_way_to(10, 10, 11, 10, &way));
    EXPECT_EQ(2u, way);
    EXPECT_TRUE(walk_way_to(10, 10, 12, 13, &way));
    EXPECT_EQ(3u, way);
    EXPECT_TRUE(walk_way_to(10, 10, 9, 20, &way));
    EXPECT_EQ(4u, way);
    EXPECT_TRUE(walk_way_to(10, 10, 5, 13, &way));
    EXPECT_EQ(5u, way);
    EXPECT_TRUE(walk_way_to(10, 10, 0, 9, &way));
    EXPECT_EQ(6u, way);
    EXPECT_TRUE(walk_way_to(10, 10, 9, 9, &way));
    EXPECT_EQ(7u, way);
    /* Twice as far one way as the other is still to a corner. */
    EXPECT_TRUE(walk_way_to(10, 10, 14, 8, &way));
    EXPECT_EQ(1u, way);
    EXPECT_TRUE(walk_way_to(10, 10, 15, 8, &way));
    EXPECT_EQ(2u, way);
    way = 99;
    EXPECT_FALSE(walk_way_to(10, 10, 10, 10, &way));
    EXPECT_EQ(99u, way);
}

/* The stride of a walker of a speed going a way from (10, 10). */
static unsigned stride_of(const MakeWalk *m, uint8_t speed, uint8_t way)
{
    Unit unit;
    memset(&unit, 0, sizeof unit);
    unit.mover = UNIT_WALKS;
    unit.speed = speed;
    return walk_stride(&m->terrain, &unit, 10, 10, way);
}

UTEST(walk, on_flat_land_a_walker_slides_its_speed_in_a_tick)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    EXPECT_EQ(18u, stride_of(&m, 18, 2));
    EXPECT_EQ(9u, stride_of(&m, 9, 0));
    /* To a corner, 707 thousandths of it, cut down. */
    EXPECT_EQ(12u, stride_of(&m, 18, 1));
    EXPECT_EQ(6u, stride_of(&m, 9, 5));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, downhill_is_faster_and_uphill_slower_by_up_to_half)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    m.raised[10 * SIDE + 10] = 60;
    /* 16 lower: a quarter more. 16 higher: a quarter less. */
    m.raised[10 * SIDE + 11] = 44;
    m.raised[10 * SIDE + 9] = 76;
    EXPECT_EQ(22u, stride_of(&m, 18, 2));
    EXPECT_EQ(13u, stride_of(&m, 18, 6));
    /* No more counts than 32 either way. */
    m.raised[9 * SIDE + 10] = 0;
    m.raised[11 * SIDE + 10] = 127;
    EXPECT_EQ(27u, stride_of(&m, 18, 0));
    EXPECT_EQ(9u, stride_of(&m, 18, 4));
    m.raised[9 * SIDE + 10] = 28;
    EXPECT_EQ(27u, stride_of(&m, 18, 0));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, the_cost_of_the_two_squares_slows_or_speeds_a_walker)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    /* Onto sand, 14: the mean with land is 11. Along a road, 6. */
    m.cost[10 * SIDE + 11] = 14;
    EXPECT_EQ(18u * 8u / 11u, stride_of(&m, 18, 2));
    m.cost[10 * SIDE + 10] = 6;
    m.cost[9 * SIDE + 10] = 6;
    EXPECT_EQ(24u, stride_of(&m, 18, 0));
    /* A mean cut down to a whole number: 6 and 9 are 7. */
    m.cost[11 * SIDE + 10] = 9;
    EXPECT_EQ(18u * 8u / 7u, stride_of(&m, 18, 4));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_step_lasts_the_whole_ticks_its_stride_takes)
{
    EXPECT_EQ(15u, walk_ticks(18));
    EXPECT_EQ(16u, walk_ticks(16));
    EXPECT_EQ(18u, walk_ticks(15));
    EXPECT_EQ(5u, walk_ticks(63));
    EXPECT_EQ(256u, walk_ticks(1));
    EXPECT_EQ(256u, walk_ticks(0));
}

UTEST(walk, a_unit_that_walks_on_takes_its_next_step_without_a_tick_between)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    ASSERT_TRUE(walk_send(&m.units, &m.fields, &m.terrain, id, 10, 6));
    const Unit *unit = units_get(&m.units, id);
    /* Four squares straight up, 15 ticks each: in every one of the 60
     * ticks it is a tick further along a step. */
    for (int t = 0; t < 60; t++) {
        tick(&m, 1);
        EXPECT_EQ(15u, unit->ticks);
        EXPECT_EQ((unsigned)(14 - t % 15), unit->left);
        /* On the new square from half-way, 7 ticks into each. */
        EXPECT_EQ(10 + 100 * (t % 15 < 7 ? 10 - t / 15 : 9 - t / 15),
            square_of(&m, id));
        EXPECT_EQ(10 + 100 * (9 - t / 15), heading_of(&m, id));
        EXPECT_EQ((unsigned)(t + 1), unit->walked);
    }
    tick(&m, 1);
    EXPECT_EQ(0u, unit->ticks);
    EXPECT_FALSE(unit->going);
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_stride_is_never_less_than_1_or_more_than_63)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    EXPECT_EQ(63u, stride_of(&m, 255, 2));
    EXPECT_EQ(44u, stride_of(&m, 255, 3));
    m.cost[10 * SIDE + 10] = 250;
    m.cost[10 * SIDE + 11] = 250;
    m.cost[9 * SIDE + 11] = 250;
    EXPECT_EQ(1u, stride_of(&m, 1, 2));
    EXPECT_EQ(1u, stride_of(&m, 1, 1));
    /* A unit that says no speed goes as one of no known kind. */
    m.cost[10 * SIDE + 10] = 8;
    m.cost[10 * SIDE + 11] = 8;
    EXPECT_EQ((unsigned)UNITS_SPEED_UNKNOWN, stride_of(&m, 0, 2));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, what_flies_or_hovers_goes_at_its_own_speed_over_anything)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    m.cost[10 * SIDE + 11] = 14;
    m.raised[10 * SIDE + 11] = 100;
    Unit unit;
    memset(&unit, 0, sizeof unit);
    unit.speed = 18;
    unit.mover = UNIT_FLIES;
    EXPECT_EQ(18u, walk_stride(&m.terrain, &unit, 10, 10, 2));
    EXPECT_EQ(12u, walk_stride(&m.terrain, &unit, 10, 10, 3));
    unit.mover = UNIT_HOVERS;
    EXPECT_EQ(18u, walk_stride(&m.terrain, &unit, 10, 10, 2));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_unit_told_to_step_holds_the_square_at_once_and_slides)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    ASSERT_TRUE(walk_tell(&m.units, id, 0));
    tick(&m, 1);
    const Unit *unit = units_get(&m.units, id);
    /* From the first tick it holds the new square as well as the one
     * it is on. */
    EXPECT_TRUE(unit->reaching);
    EXPECT_EQ(10 + 100 * 10, square_of(&m, id));
    EXPECT_EQ(10, unit->from_x);
    EXPECT_EQ(10, unit->from_y);
    EXPECT_EQ(10, unit->to_x);
    EXPECT_EQ(9, unit->to_y);
    EXPECT_EQ(0, held_by(&m, 10, 9));
    EXPECT_EQ(0, held_by(&m, 10, 10));
    EXPECT_TRUE(units_check(&m.units));
    /* 256 by 18 is 15 ticks, and the first is this one. */
    EXPECT_EQ(15u, unit->ticks);
    EXPECT_EQ(14u, unit->left);
    /* Half-way, with 7 of the 15 left, it is on the new square and
     * the old one is free. */
    tick(&m, 6);
    EXPECT_EQ(8u, unit->left);
    EXPECT_EQ(10 + 100 * 10, square_of(&m, id));
    tick(&m, 1);
    EXPECT_EQ(7u, unit->left);
    EXPECT_FALSE(unit->reaching);
    EXPECT_EQ(10 + 100 * 9, square_of(&m, id));
    EXPECT_EQ(0, held_by(&m, 10, 9));
    EXPECT_EQ(-1, held_by(&m, 10, 10));
    EXPECT_TRUE(units_check(&m.units));
    tick(&m, 6);
    EXPECT_EQ(1u, unit->left);
    EXPECT_EQ(14u, unit->walked);
    /* In the tick it arrives it is still the walker of that step. */
    tick(&m, 1);
    EXPECT_EQ(0u, unit->left);
    EXPECT_EQ(15u, unit->ticks);
    /* Then it stands: nothing more was told. */
    tick(&m, 1);
    EXPECT_EQ(10 + 100 * 9, square_of(&m, id));
    EXPECT_EQ(0u, unit->left);
    EXPECT_EQ(0u, unit->ticks);
    EXPECT_EQ(0u, unit->walked);
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_unit_turns_to_face_its_way_before_it_steps)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    /* To the right is a quarter turn: 64 steps at 16 a tick. */
    ASSERT_TRUE(walk_tell(&m.units, id, 2));
    tick(&m, 3);
    EXPECT_EQ(48u, units_get(&m.units, id)->facing);
    EXPECT_EQ(10 + 100 * 10, square_of(&m, id));
    tick(&m, 1);
    EXPECT_EQ(64u, units_get(&m.units, id)->facing);
    EXPECT_EQ(10 + 100 * 10, square_of(&m, id));
    tick(&m, 1);
    EXPECT_EQ(11 + 100 * 10, heading_of(&m, id));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_step_onto_closed_ground_is_dropped)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    m.closed[9 * SIDE + 10] = TERRAIN_NO_WALK;
    ASSERT_TRUE(walk_tell(&m.units, id, 0));
    tick(&m, 2);
    EXPECT_EQ(10 + 100 * 10, square_of(&m, id));
    EXPECT_FALSE(units_get(&m.units, id)->told);
    EXPECT_EQ(0u, units_get(&m.units, id)->left);
    EXPECT_TRUE(units_check(&m.units));
    /* What hovers crosses it. */
    units_get(&m.units, id)->mover = UNIT_HOVERS;
    ASSERT_TRUE(walk_tell(&m.units, id, 0));
    tick(&m, 1);
    EXPECT_EQ(10 + 100 * 9, heading_of(&m, id));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_step_to_a_corner_goes_past_a_closed_square_beside_it)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    /* Up and to the right, with a wall straight up and one to the
     * right: as in the original it goes between them. */
    m.closed[9 * SIDE + 10] = TERRAIN_NO_WALK;
    m.closed[10 * SIDE + 11] = TERRAIN_NO_WALK;
    units_get(&m.units, id)->facing = 32;
    ASSERT_TRUE(walk_tell(&m.units, id, 1));
    tick(&m, 1);
    EXPECT_EQ(11 + 100 * 9, heading_of(&m, id));
    EXPECT_TRUE(units_check(&m.units));
    /* But not onto the wall itself. */
    tick(&m, 30);
    units_get(&m.units, id)->facing = 96;
    ASSERT_TRUE(walk_tell(&m.units, id, 3));
    m.closed[10 * SIDE + 12] = TERRAIN_NO_WALK;
    tick(&m, 1);
    EXPECT_EQ(11 + 100 * 9, square_of(&m, id));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_step_onto_a_square_a_unit_stands_on_is_dropped)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    UnitId other = put(&m, 10, 9);
    ASSERT_TRUE(walk_tell(&m.units, id, 0));
    tick(&m, 2);
    EXPECT_EQ(10 + 100 * 10, square_of(&m, id));
    EXPECT_EQ(10 + 100 * 9, square_of(&m, other));
    EXPECT_FALSE(units_get(&m.units, id)->told);
    EXPECT_TRUE(units_check(&m.units));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, two_units_told_onto_one_square_the_first_in_the_list_gets_it)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId left = put(&m, 9, 10);
    UnitId right = put(&m, 11, 10);
    units_get(&m.units, left)->facing = 64;
    units_get(&m.units, right)->facing = 192;
    ASSERT_TRUE(walk_tell(&m.units, left, 2));
    ASSERT_TRUE(walk_tell(&m.units, right, 6));
    tick(&m, 1);
    EXPECT_EQ(10 + 100 * 10, heading_of(&m, left));
    EXPECT_EQ(11 + 100 * 10, heading_of(&m, right));
    EXPECT_FALSE(units_get(&m.units, right)->reaching);
    EXPECT_EQ(0, held_by(&m, 10, 10));
    EXPECT_TRUE(units_check(&m.units));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_square_is_free_for_the_next_when_its_unit_is_half_way_off)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId ahead = put(&m, 10, 9);
    UnitId behind = put(&m, 10, 10);
    ASSERT_TRUE(walk_tell(&m.units, ahead, 0));
    /* While the one ahead is on its square the other cannot step
     * there: it is still its place, and where it would be hit. */
    tick(&m, 1);
    ASSERT_TRUE(walk_tell(&m.units, behind, 0));
    tick(&m, 1);
    EXPECT_FALSE(units_get(&m.units, behind)->reaching);
    EXPECT_EQ(0, held_by(&m, 10, 9));
    /* Half-way through the step ahead the square is let go, and the
     * one behind takes it in its next tick. */
    tick(&m, 6);
    EXPECT_EQ(10 + 100 * 8, square_of(&m, ahead));
    EXPECT_EQ(-1, held_by(&m, 10, 9));
    ASSERT_TRUE(walk_tell(&m.units, behind, 0));
    tick(&m, 1);
    EXPECT_EQ(10 + 100 * 9, heading_of(&m, behind));
    EXPECT_EQ(1, held_by(&m, 10, 9));
    EXPECT_TRUE(units_check(&m.units));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_slide_goes_on_to_its_end_and_a_new_step_waits_for_it)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    ASSERT_TRUE(walk_tell(&m.units, id, 0));
    tick(&m, 5);
    ASSERT_TRUE(walk_tell(&m.units, id, 0));
    tick(&m, 9);
    EXPECT_EQ(10 + 100 * 9, square_of(&m, id));
    EXPECT_TRUE(units_get(&m.units, id)->told);
    tick(&m, 1);
    EXPECT_EQ(0u, units_get(&m.units, id)->left);
    /* The next tick it is off again, and its walk has not stopped. */
    tick(&m, 1);
    EXPECT_EQ(10 + 100 * 8, heading_of(&m, id));
    EXPECT_EQ(16u, units_get(&m.units, id)->walked);
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_unit_that_is_gone_or_a_way_there_is_not_is_not_told)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    EXPECT_FALSE(walk_tell(&m.units, id, 8));
    EXPECT_FALSE(units_get(&m.units, id)->told);
    units_remove(&m.units, id);
    EXPECT_FALSE(walk_tell(&m.units, id, 0));
    EXPECT_FALSE(walk_tick(&m.units, id, &m.terrain, &m.fields));
    units_free(&m.units);
    fields_free(&m.fields);
}

/* Sends a unit to a square and lets the game run until it stands and
 * is going nowhere, or `most` ticks have passed. Returns the ticks. */
static int go(MakeWalk *m, UnitId id, int x, int y, int most)
{
    int ticks = 0;
    if (!walk_send(&m->units, &m->fields, &m->terrain, id, x, y)) {
        return -1;
    }
    const Unit *unit = units_get(&m->units, id);
    while (ticks < most && (unit->going || unit->left > 0)) {
        tick(m, 1);
        ticks++;
    }
    return ticks;
}

/* How many fields are in use. */
static int fields_in_use(const MakeWalk *m)
{
    int count = 0;
    for (int i = 0; i < FIELDS_MOST; i++) {
        count += m->fields.users[i] > 0 ? 1 : 0;
    }
    return count;
}

UTEST(walk, a_unit_sent_to_a_square_walks_there_and_stands)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    int ticks = go(&m, id, 16, 13, 2000);
    EXPECT_EQ(16 + 100 * 13, square_of(&m, id));
    /* Three steps to the corner and three straight: no slower than
     * if it stood a tick between them, and not in one jump. */
    EXPECT_GT(ticks, 3 * 21 + 3 * 15 - 6);
    EXPECT_LT(ticks, 3 * 22 + 3 * 15 + 30);
    EXPECT_FALSE(units_get(&m.units, id)->going);
    EXPECT_EQ(0u, units_get(&m.units, id)->walked);
    /* Its field is given back. */
    EXPECT_EQ(0, fields_in_use(&m));
    EXPECT_TRUE(units_check(&m.units));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, it_goes_round_what_is_closed_to_it)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    /* A wall from the top edge down past its row, with a gap far
     * below. */
    for (int y = 1; y < 30; y++) {
        m.closed[y * SIDE + 13] = TERRAIN_NO_WALK;
    }
    go(&m, id, 16, 10, 5000);
    EXPECT_EQ(16 + 100 * 10, square_of(&m, id));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_goal_closed_to_it_becomes_the_nearest_open_square)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    /* A pond of three by three around the goal. */
    for (int y = 19; y <= 21; y++) {
        for (int x = 19; x <= 21; x++) {
            m.closed[y * SIDE + x] = TERRAIN_NO_WALK;
        }
    }
    go(&m, id, 20, 20, 5000);
    /* The first square of the second ring. */
    EXPECT_EQ(18 + 100 * 18, square_of(&m, id));
    /* What hovers goes onto the pond itself. */
    UnitId bee = put(&m, 12, 10);
    units_get(&m.units, bee)->mover = UNIT_HOVERS;
    go(&m, bee, 20, 20, 5000);
    EXPECT_EQ(20 + 100 * 20, square_of(&m, bee));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_unit_with_no_way_there_stands_where_it_is)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    /* Water right across the map. */
    for (int y = 0; y < SIDE; y++) {
        m.closed[y * SIDE + 20] = TERRAIN_NO_WALK;
    }
    EXPECT_LT(go(&m, id, 30, 10, 5000), 3);
    EXPECT_EQ(10 + 100 * 10, square_of(&m, id));
    EXPECT_FALSE(units_get(&m.units, id)->going);
    EXPECT_EQ(0, fields_in_use(&m));
    /* And is not sent at all where no open square is near the
     * goal. */
    memset(m.closed, TERRAIN_NO_ONE, sizeof m.closed);
    EXPECT_EQ(-1, go(&m, id, 30, 30, 10));
    EXPECT_FALSE(units_get(&m.units, id)->going);
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_large_unit_goes_by_its_top_left_square_and_needs_room)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    /* A wall with a gap of one square and, further down, one of
     * three. */
    for (int y = 1; y < SIDE - 1; y++) {
        m.closed[y * SIDE + 15] = TERRAIN_NO_WALK;
    }
    m.closed[10 * SIDE + 15] = 0;
    for (int y = 30; y < 33; y++) {
        m.closed[y * SIDE + 15] = 0;
    }
    UnitId id;
    Unit dragon;
    memset(&dragon, 0, sizeof dragon);
    dragon.x = 10 * 256 + 128;
    dragon.y = 10 * 256 + 128;
    dragon.size = 3;
    dragon.mover = UNIT_WALKS;
    dragon.speed = 18;
    dragon.turn_speed = 16;
    ASSERT_TRUE(units_add(&m.units, &dragon, &id));
    /* Sent to the square (21, 11): its middle goes there, so its
     * first square to (20, 10), by the wide gap. */
    go(&m, id, 21, 11, 20000);
    EXPECT_EQ(20 + 100 * 10, square_of(&m, id));
    UnitId found;
    EXPECT_TRUE(units_standing(&m.units, false, 22, 12, &found));
    EXPECT_TRUE(units_check(&m.units));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, units_sent_to_one_square_share_its_field)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId first = put(&m, 10, 10);
    UnitId second = put(&m, 10, 12);
    ASSERT_TRUE(walk_send(&m.units, &m.fields, &m.terrain, first, 30, 30));
    ASSERT_TRUE(walk_send(&m.units, &m.fields, &m.terrain, second, 30,
        30));
    EXPECT_EQ(1, fields_in_use(&m));
    EXPECT_EQ(units_get(&m.units, first)->field,
        units_get(&m.units, second)->field);
    /* Sent elsewhere, a unit gives its share back. */
    ASSERT_TRUE(walk_send(&m.units, &m.fields, &m.terrain, second, 5, 5));
    EXPECT_EQ(2, fields_in_use(&m));
    walk_stop(&m.units, &m.fields, first);
    EXPECT_EQ(1, fields_in_use(&m));
    walk_stop(&m.units, &m.fields, first);
    EXPECT_EQ(1, fields_in_use(&m));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_unit_steps_round_one_that_stands_in_its_way)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    UnitId id = put(&m, 10, 10);
    put(&m, 12, 10);
    go(&m, id, 16, 10, 2000);
    EXPECT_EQ(16 + 100 * 10, square_of(&m, id));
    EXPECT_TRUE(units_check(&m.units));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, shut_in_by_units_it_waits_and_then_gives_up)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    /* A passage one square wide with a unit standing in it. */
    for (int x = 1; x < SIDE - 1; x++) {
        m.closed[9 * SIDE + x] = TERRAIN_NO_WALK;
        m.closed[11 * SIDE + x] = TERRAIN_NO_WALK;
    }
    UnitId id = put(&m, 10, 10);
    UnitId other = put(&m, 14, 10);
    int ticks = go(&m, id, 20, 10, 5000);
    /* Up to it, and no further. */
    EXPECT_EQ(13 + 100 * 10, square_of(&m, id));
    EXPECT_GT(ticks, WALK_PATIENCE);
    EXPECT_LT(ticks, WALK_PATIENCE + 3 * 16 + 10);
    EXPECT_FALSE(units_get(&m.units, id)->going);
    EXPECT_EQ(0, fields_in_use(&m));
    /* When the other is gone, the way is free. */
    units_remove(&m.units, other);
    go(&m, id, 20, 10, 5000);
    EXPECT_EQ(20 + 100 * 10, square_of(&m, id));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, a_unit_that_waits_goes_on_when_the_one_ahead_moves_off)
{
    static MakeWalk m;
    ASSERT_TRUE(make_walk(&m));
    for (int x = 1; x < SIDE - 1; x++) {
        m.closed[9 * SIDE + x] = TERRAIN_NO_WALK;
        m.closed[11 * SIDE + x] = TERRAIN_NO_WALK;
    }
    UnitId behind = put(&m, 10, 10);
    UnitId ahead = put(&m, 12, 10);
    ASSERT_TRUE(walk_send(&m.units, &m.fields, &m.terrain, behind, 20,
        10));
    tick(&m, 60);
    EXPECT_EQ(11 + 100 * 10, square_of(&m, behind));
    EXPECT_TRUE(units_get(&m.units, behind)->going);
    /* Now the one ahead is sent on, further than the other goes. */
    ASSERT_TRUE(walk_send(&m.units, &m.fields, &m.terrain, ahead, 30, 10));
    tick(&m, 600);
    EXPECT_EQ(30 + 100 * 10, square_of(&m, ahead));
    EXPECT_EQ(20 + 100 * 10, square_of(&m, behind));
    EXPECT_EQ(0, fields_in_use(&m));
    EXPECT_TRUE(units_check(&m.units));
    units_free(&m.units);
    fields_free(&m.fields);
}

UTEST(walk, many_units_sent_about_never_share_a_square_and_all_come_to_rest)
{
    static MakeWalk m;
    Random orders;
    ASSERT_TRUE(make_walk(&m));
    random_start(&orders, 5);
    for (int i = 0; i < SIDE * SIDE; i++) {
        uint32_t what = random_below(&orders, 100);
        m.closed[i] |= what < 10 ? TERRAIN_NO_WALK | TERRAIN_NO_HOVER
            : what < 16 ? TERRAIN_NO_WALK : 0;
        m.cost[i] = (uint8_t)(6 + random_below(&orders, 10));
        m.raised[i] = (uint8_t)random_below(&orders, 128);
    }
    static const uint8_t movers[4] = {UNIT_WALKS, UNIT_WALKS, UNIT_HOVERS,
        UNIT_FLIES};
    for (int i = 0; i < 250; i++) {
        Unit unit;
        memset(&unit, 0, sizeof unit);
        unit.x = (int32_t)(1 + random_below(&orders, SIDE - 2)) * 256 + 128;
        unit.y = (int32_t)(1 + random_below(&orders, SIDE - 2)) * 256 + 128;
        unit.mover = movers[random_below(&orders, 4)];
        unit.size = 1;
        unit.speed = (uint8_t)(8 + random_below(&orders, 28));
        unit.turn_speed = (uint8_t)(8 + random_below(&orders, 24));
        unit.player = 2;
        units_add(&m.units, &unit, NULL);
    }
    bool sound = true;
    uint32_t sent = 0;
    for (int t = 0; t < 4000 && sound; t++) {
        /* Now and then a unit is sent to one of a few places. */
        for (int i = 0; i < 3; i++) {
            UnitId id;
            uint32_t slot = random_below(&orders, m.units.count);
            if (units_at(&m.units, slot, &id) != NULL) {
                sent += walk_send(&m.units, &m.fields, &m.terrain, id,
                    (int32_t)(4 + random_below(&orders, 5) * 9),
                    (int32_t)(4 + random_below(&orders, 5) * 9)) ? 1u : 0u;
            }
        }
        tick(&m, 1);
        sound = units_check(&m.units);
    }
    EXPECT_TRUE(sound);
    EXPECT_GT(sent, 5000u);
    /* With no more orders every unit comes to rest: none hangs. */
    tick(&m, 4000);
    EXPECT_TRUE(units_check(&m.units));
    for (uint32_t slot = 0; slot < m.units.count; slot++) {
        const Unit *unit = units_at(&m.units, slot, NULL);
        if (unit != NULL) {
            EXPECT_FALSE(unit->going);
            EXPECT_EQ(0u, unit->left);
        }
    }
    EXPECT_EQ(0, fields_in_use(&m));
    units_free(&m.units);
    fields_free(&m.fields);
}

/* A long game of many units told to step at random, on a map with
 * walls and water. Returns a number made of where every unit is at
 * the end, or 0 if at any tick the list failed its check or a slide
 * lasted longer than a step can. `*steps` gets how many were taken. */
static uint32_t soak(uint32_t seed, int units, int ticks, uint32_t *steps)
{
    static MakeWalk m;
    Random orders;
    bool sound = make_walk(&m);
    random_start(&orders, seed);
    for (int i = 0; i < SIDE * SIDE; i++) {
        uint32_t what = random_below(&orders, 100);
        m.closed[i] |= what < 12 ? TERRAIN_NO_WALK | TERRAIN_NO_HOVER
            : what < 20 ? TERRAIN_NO_WALK : 0;
        m.cost[i] = (uint8_t)(6 + random_below(&orders, 10));
        m.raised[i] = (uint8_t)random_below(&orders, 128);
    }
    static const uint8_t movers[4] = {UNIT_WALKS, UNIT_WALKS, UNIT_HOVERS,
        UNIT_FLIES};
    for (int i = 0; i < units; i++) {
        Unit unit;
        memset(&unit, 0, sizeof unit);
        unit.x = (int32_t)(1 + random_below(&orders, SIDE - 4)) * 256 + 128;
        unit.y = (int32_t)(1 + random_below(&orders, SIDE - 4)) * 256 + 128;
        unit.mover = movers[random_below(&orders, 4)];
        unit.size = (uint8_t)(random_below(&orders, 10) == 0 ? 2 : 1);
        unit.speed = (uint8_t)(8 + random_below(&orders, 28));
        unit.turn_speed = (uint8_t)(8 + random_below(&orders, 24));
        unit.player = 2;
        units_add(&m.units, &unit, NULL);
    }
    *steps = 0;
    static uint16_t sliding[UNITS_MOST];
    memset(sliding, 0, sizeof sliding);
    for (int t = 0; t < ticks && sound; t++) {
        for (uint32_t slot = 0; slot < m.units.count; slot++) {
            UnitId id;
            if (units_at(&m.units, slot, &id) != NULL
                && random_below(&orders, 4) == 0) {
                walk_tell(&m.units, id,
                    (uint8_t)random_below(&orders, WALK_WAYS));
            }
        }
        units_tick(&m.units, &m.terrain, &m.fields, &m.random);
        sound = units_check(&m.units);
        for (uint32_t slot = 0; slot < m.units.count; slot++) {
            const Unit *unit = units_at(&m.units, slot, NULL);
            if (unit == NULL) {
                continue;
            }
            *steps += unit->ticks != 0 && unit->left == unit->ticks - 1
                ? 1u : 0u;
            sliding[slot] = unit->left > 0 ? (uint16_t)(sliding[slot] + 1)
                : 0;
            sound = sound && sliding[slot] <= UNIT_STEP;
        }
    }
    uint32_t sum = 2166136261u;
    for (uint32_t slot = 0; slot < m.units.count; slot++) {
        const Unit *unit = units_at(&m.units, slot, NULL);
        if (unit != NULL) {
            sum = (sum ^ (uint32_t)unit->x) * 16777619u;
            sum = (sum ^ (uint32_t)unit->y) * 16777619u;
            sum = (sum ^ unit->left) * 16777619u;
            sum = (sum ^ unit->facing) * 16777619u;
        }
    }
    units_free(&m.units);
    fields_free(&m.fields);
    return sound ? sum | 1u : 0;
}

UTEST(walk, in_a_long_game_no_two_units_ever_stand_on_one_square)
{
    uint32_t steps = 0;
    EXPECT_NE(0u, soak(11, 300, 5000, &steps));
    /* And they did walk. */
    EXPECT_GT(steps, 10000u);
}

UTEST(walk, the_same_game_plays_out_the_same_again)
{
    uint32_t first_steps = 0;
    uint32_t again_steps = 0;
    uint32_t first = soak(23, 200, 2000, &first_steps);
    uint32_t again = soak(23, 200, 2000, &again_steps);
    EXPECT_NE(0u, first);
    EXPECT_EQ(first, again);
    EXPECT_EQ(first_steps, again_steps);
    /* Another game is another. */
    EXPECT_NE(first, soak(24, 200, 2000, &again_steps));
}
