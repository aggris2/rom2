#include "mechanics/guard.h"

#include <string.h>

#include "mechanics/walk.h"
#include "utest.h"

/* Flat open ground of SIDE squares each way, an empty list of units
 * on it, and two players: the first, whom a person plays, and the
 * second, who is at war with the first. */
#define SIDE 64

typedef struct MakeGuard {
    Terrain terrain;
    uint8_t cost[SIDE * SIDE];
    uint8_t closed[SIDE * SIDE];
    uint8_t raised[SIDE * SIDE];
    Units units;
    Fields fields;
    Relations relations;
    Random random;
    Guard guard;
} MakeGuard;

static bool make_guard(MakeGuard *m)
{
    memset(m, 0, sizeof *m);
    m->terrain.width = SIDE;
    m->terrain.height = SIDE;
    m->terrain.cost = m->cost;
    m->terrain.closed = m->closed;
    m->terrain.raised = m->raised;
    memset(m->cost, 8, sizeof m->cost);
    m->relations.stands[1][0] = RELATION_ENEMY;
    random_start(&m->random, 7);
    if (!units_make(SIDE, SIDE, &m->units)) {
        return false;
    }
    m->units.people = 1;
    return true;
}

static void unmake_guard(MakeGuard *m)
{
    guard_free(&m->guard);
    units_free(&m->units);
    fields_free(&m->fields);
}

/* Puts a walker of a player and a group on the square (x, y), facing
 * up the map, that sees 5 squares. */
static UnitId put(MakeGuard *m, int x, int y, int32_t player, int32_t group)
{
    Unit unit;
    UnitId id = {9999, 9999};
    memset(&unit, 0, sizeof unit);
    unit.x = x * 256 + 128;
    unit.y = y * 256 + 128;
    unit.player = player;
    unit.group = group;
    unit.size = 1;
    unit.mover = UNIT_WALKS;
    unit.speed = 18;
    unit.turn_speed = 16;
    unit.sight = 5;
    units_add(&m->units, &unit, &id);
    return id;
}

static bool bands(MakeGuard *m)
{
    return guard_make(&m->units, &m->terrain, &m->random, &m->guard);
}

static void tick(MakeGuard *m, int ticks)
{
    for (int i = 0; i < ticks; i++) {
        guard_tick(&m->guard, &m->units, &m->terrain, &m->fields,
            &m->relations);
        units_tick(&m->units, &m->terrain, &m->fields, &m->random);
    }
}

/* The square a unit is on: its x plus 100 times its y. */
static int32_t square_of(const MakeGuard *m, UnitId id)
{
    const Unit *unit = units_get(&m->units, id);
    return unit->x / 256 + 100 * (unit->y / 256);
}

UTEST(guard, the_units_of_a_player_and_a_group_are_a_band)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId a = put(&m, 10, 10, 2, 7);
    UnitId b = put(&m, 12, 10, 2, 7);
    UnitId other = put(&m, 30, 30, 2, 8);
    UnitId third = put(&m, 40, 40, 3, 7);
    ASSERT_TRUE(bands(&m));
    EXPECT_EQ(3u, m.guard.count);
    EXPECT_EQ(units_get(&m.units, a)->band, units_get(&m.units, b)->band);
    EXPECT_NE(units_get(&m.units, a)->band,
        units_get(&m.units, other)->band);
    EXPECT_NE(units_get(&m.units, a)->band,
        units_get(&m.units, third)->band);
    EXPECT_NE(0u, units_get(&m.units, a)->band);
    /* Each guards from the square it is on. */
    EXPECT_EQ(12, units_get(&m.units, b)->home_x);
    EXPECT_EQ(10, units_get(&m.units, b)->home_y);
    unmake_guard(&m);
}

UTEST(guard, a_persons_unit_and_one_in_no_group_do_not_guard)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId mine = put(&m, 10, 10, 1, 7);
    UnitId loose = put(&m, 20, 20, 2, 0);
    ASSERT_TRUE(bands(&m));
    EXPECT_EQ(0u, m.guard.count);
    EXPECT_EQ(0u, units_get(&m.units, mine)->band);
    EXPECT_EQ(0u, units_get(&m.units, loose)->band);
    /* And a turn of no bands does nothing. */
    tick(&m, 20);
    EXPECT_EQ(10 + 100 * 10, square_of(&m, mine));
    unmake_guard(&m);
}

UTEST(guard, a_bands_range_is_its_furthest_reach_and_a_little_more)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    /* Units 10 apart that see 5: the middle is 5 from each, so each
     * reaches 10. */
    UnitId a = put(&m, 10, 10, 2, 7);
    put(&m, 20, 10, 2, 7);
    ASSERT_TRUE(bands(&m));
    unsigned range = m.guard.bands[units_get(&m.units, a)->band - 1].range;
    EXPECT_TRUE(range >= 10u + GUARD_RANGE_MORE - 1);
    EXPECT_TRUE(range <= 10u + GUARD_RANGE_MORE + 1);
    unmake_guard(&m);
}

UTEST(guard, a_band_with_a_short_reach_guards_wide)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    /* One unit that sees 5 reaches 5, less than the least. */
    UnitId a = put(&m, 10, 10, 2, 7);
    ASSERT_TRUE(bands(&m));
    unsigned range = m.guard.bands[units_get(&m.units, a)->band - 1].range;
    EXPECT_TRUE(range >= GUARD_RANGE_WIDE + GUARD_RANGE_MORE - 1u);
    EXPECT_TRUE(range <= GUARD_RANGE_WIDE + GUARD_RANGE_MORE + 1u);
    unmake_guard(&m);
}

UTEST(guard, a_unit_walks_up_to_an_enemy_it_sees_and_stops_beside_it)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    UnitId hero = put(&m, 24, 20, 1, 0);
    ASSERT_TRUE(bands(&m));
    tick(&m, 1);
    EXPECT_TRUE(units_get(&m.units, monster)->going);
    tick(&m, 200);
    EXPECT_EQ(23 + 100 * 20, square_of(&m, monster));
    EXPECT_FALSE(units_get(&m.units, monster)->going);
    /* It faces him: to the right. */
    EXPECT_EQ(64u, units_get(&m.units, monster)->facing);
    /* He was not moved. */
    EXPECT_EQ(24 + 100 * 20, square_of(&m, hero));
    EXPECT_TRUE(units_check(&m.units));
    unmake_guard(&m);
}

UTEST(guard, an_enemy_out_of_sight_is_left_alone)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    put(&m, 27, 20, 1, 0);
    ASSERT_TRUE(bands(&m));
    tick(&m, 40);
    EXPECT_EQ(20 + 100 * 20, square_of(&m, monster));
    EXPECT_FALSE(units_get(&m.units, monster)->going);
    unmake_guard(&m);
}

UTEST(guard, a_hill_hides_an_enemy_from_a_band)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    put(&m, 24, 20, 1, 0);
    /* A ridge between the two. */
    for (int row = 0; row < SIDE; row++) {
        m.raised[row * SIDE + 22] = 127;
        m.raised[row * SIDE + 23] = 127;
    }
    ASSERT_TRUE(bands(&m));
    tick(&m, 40);
    EXPECT_EQ(20 + 100 * 20, square_of(&m, monster));
    unmake_guard(&m);
}

UTEST(guard, one_who_is_no_enemy_is_left_alone)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    put(&m, 24, 20, 1, 0);
    m.relations.stands[1][0] = RELATION_ALLY;
    ASSERT_TRUE(bands(&m));
    tick(&m, 40);
    EXPECT_EQ(20 + 100 * 20, square_of(&m, monster));
    unmake_guard(&m);
}

UTEST(guard, the_whole_band_goes_for_what_one_of_it_sees)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId near = put(&m, 20, 20, 2, 7);
    UnitId far = put(&m, 12, 20, 2, 7);
    put(&m, 24, 20, 1, 0);
    ASSERT_TRUE(bands(&m));
    tick(&m, 1);
    EXPECT_TRUE(units_get(&m.units, near)->going);
    /* He is 12 squares from this one, which sees 5. */
    EXPECT_TRUE(units_get(&m.units, far)->going);
    unmake_guard(&m);
}

UTEST(guard, a_unit_takes_the_nearest_enemy)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    put(&m, 24, 20, 1, 0);
    put(&m, 20, 17, 1, 0);
    ASSERT_TRUE(bands(&m));
    tick(&m, 200);
    EXPECT_EQ(20 + 100 * 18, square_of(&m, monster));
    unmake_guard(&m);
}

UTEST(guard, of_enemies_equally_near_it_takes_the_one_it_faces)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    put(&m, 20, 23, 1, 0);
    put(&m, 20, 17, 1, 0);
    ASSERT_TRUE(bands(&m));
    /* It faces up the map. */
    tick(&m, 200);
    EXPECT_EQ(20 + 100 * 18, square_of(&m, monster));
    unmake_guard(&m);
}

UTEST(guard, an_enemy_too_far_from_the_bands_middle_is_left_alone)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    put(&m, 24, 20, 1, 0);
    ASSERT_TRUE(bands(&m));
    m.guard.bands[0].range = 3;
    tick(&m, 40);
    EXPECT_EQ(20 + 100 * 20, square_of(&m, monster));
    unmake_guard(&m);
}

UTEST(guard, a_unit_goes_home_when_its_enemy_is_gone)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    UnitId hero = put(&m, 24, 20, 1, 0);
    ASSERT_TRUE(bands(&m));
    tick(&m, 200);
    ASSERT_EQ(23 + 100 * 20, square_of(&m, monster));
    units_remove(&m.units, hero);
    tick(&m, 200);
    EXPECT_EQ(20 + 100 * 20, square_of(&m, monster));
    EXPECT_FALSE(units_get(&m.units, monster)->going);
    EXPECT_TRUE(units_check(&m.units));
    unmake_guard(&m);
}

UTEST(guard, a_band_follows_an_enemy_that_moves)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    UnitId hero = put(&m, 24, 20, 1, 0);
    ASSERT_TRUE(bands(&m));
    tick(&m, 200);
    ASSERT_TRUE(walk_send(&m.units, &m.fields, &m.terrain, hero, 24, 26));
    tick(&m, 400);
    ASSERT_EQ(24 + 100 * 26, square_of(&m, hero));
    const Unit *unit = units_get(&m.units, monster);
    const Unit *man = units_get(&m.units, hero);
    int32_t across = unit->x / 256 - man->x / 256;
    int32_t down = unit->y / 256 - man->y / 256;
    EXPECT_TRUE(across >= -1 && across <= 1 && down >= -1 && down <= 1);
    EXPECT_TRUE(units_check(&m.units));
    unmake_guard(&m);
}

UTEST(guard, the_bands_look_around_once_in_sixteen_ticks)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    ASSERT_TRUE(bands(&m));
    tick(&m, 1);
    /* An enemy that comes now is seen at the next turn. */
    put(&m, 24, 20, 1, 0);
    tick(&m, GUARD_TURN_TICKS - 1);
    EXPECT_FALSE(units_get(&m.units, monster)->going);
    tick(&m, 1);
    EXPECT_TRUE(units_get(&m.units, monster)->going);
    unmake_guard(&m);
}

UTEST(guard, a_large_unit_stops_beside_its_enemy_too)
{
    static MakeGuard m;
    ASSERT_TRUE(make_guard(&m));
    UnitId monster = put(&m, 20, 20, 2, 7);
    units_remove(&m.units, monster);
    Unit big;
    memset(&big, 0, sizeof big);
    big.x = 20 * 256 + 128;
    big.y = 20 * 256 + 128;
    big.player = 2;
    big.group = 7;
    big.size = 2;
    big.mover = UNIT_WALKS;
    big.speed = 18;
    big.turn_speed = 16;
    big.sight = 6;
    ASSERT_TRUE(units_add(&m.units, &big, &monster));
    put(&m, 26, 20, 1, 0);
    ASSERT_TRUE(bands(&m));
    tick(&m, 300);
    /* Its low right square is next to him. */
    const Unit *unit = units_get(&m.units, monster);
    EXPECT_EQ(24, unit->x / 256);
    EXPECT_FALSE(unit->going);
    EXPECT_TRUE(units_check(&m.units));
    unmake_guard(&m);
}
