#include "mechanics/units.h"

#include <string.h>

#include "utest.h"

/* Kinds for units_from_map(): none is known, but for kind 600, which
 * turns 19 steps a tick. */
static UnitKind kinds[UNIT_KIND_IDS];

static const UnitKind *some_kinds(void)
{
    kinds[600].id = 600;
    kinds[600].rotation_speed = 19;
    kinds[600].health = 80;
    kinds[600].mana = 30;
    kinds[600].sight = 7;
    return kinds;
}

/* A map of SIDE squares each way whose ground is open to all. */
#define SIDE 64
static uint8_t open_cost[SIDE * SIDE];
static uint8_t open_closed[SIDE * SIDE];
static uint8_t open_height[SIDE * SIDE];

static Terrain open_ground(void)
{
    Terrain terrain = {SIDE, SIDE, open_cost, open_closed, open_height};
    memset(open_closed, 0, sizeof open_closed);
    return terrain;
}

/* A unit of a type, to tell one from another by. */
static Unit unit_of(uint16_t type)
{
    Unit unit;
    memset(&unit, 0, sizeof unit);
    unit.type = type;
    return unit;
}

/* Adds a unit of a type and returns its id. */
static UnitId add(Units *units, uint16_t type)
{
    Unit unit = unit_of(type);
    UnitId id = {9999, 9999};
    units_add(units, &unit, &id);
    return id;
}

UTEST(units, a_unit_that_is_added_can_be_found_by_its_id)
{
    Units units = {0};
    UnitId first = add(&units, 10);
    UnitId second = add(&units, 20);
    ASSERT_TRUE(units_get(&units, first) != NULL);
    ASSERT_TRUE(units_get(&units, second) != NULL);
    EXPECT_EQ(10u, units_get(&units, first)->type);
    EXPECT_EQ(20u, units_get(&units, second)->type);
    EXPECT_EQ(0u, first.slot);
    EXPECT_EQ(1u, second.slot);
    EXPECT_EQ(2u, units.living);
    units_free(&units);
}

UTEST(units, a_unit_that_is_removed_is_gone)
{
    Units units = {0};
    UnitId first = add(&units, 10);
    UnitId second = add(&units, 20);
    units_remove(&units, first);
    EXPECT_TRUE(units_get(&units, first) == NULL);
    EXPECT_EQ(20u, units_get(&units, second)->type);
    EXPECT_EQ(1u, units.living);
    /* Again does nothing. */
    units_remove(&units, first);
    EXPECT_EQ(1u, units.living);
    units_free(&units);
}

UTEST(units, the_next_unit_takes_a_slot_that_was_left)
{
    Units units = {0};
    add(&units, 10);
    UnitId second = add(&units, 20);
    add(&units, 30);
    units_remove(&units, second);
    UnitId fourth = add(&units, 40);
    EXPECT_EQ(1u, fourth.slot);
    EXPECT_EQ(3u, units.count);
    EXPECT_EQ(3u, units.living);
    EXPECT_EQ(40u, units_get(&units, fourth)->type);
    units_free(&units);
}

UTEST(units, an_old_id_does_not_find_the_unit_that_took_its_slot)
{
    Units units = {0};
    UnitId old = add(&units, 10);
    units_remove(&units, old);
    UnitId new = add(&units, 20);
    ASSERT_EQ(old.slot, new.slot);
    EXPECT_NE(old.age, new.age);
    EXPECT_TRUE(units_get(&units, old) == NULL);
    EXPECT_EQ(20u, units_get(&units, new)->type);
    /* Nor does removing by the old id touch the new unit. */
    units_remove(&units, old);
    EXPECT_EQ(20u, units_get(&units, new)->type);
    units_free(&units);
}

UTEST(units, an_id_that_never_was_finds_nothing)
{
    Units units = {0};
    UnitId none = {0, 0};
    EXPECT_TRUE(units_get(&units, none) == NULL);
    UnitId first = add(&units, 10);
    UnitId past = {5, 0};
    UnitId other_age = {first.slot, (uint16_t)(first.age + 1)};
    EXPECT_TRUE(units_get(&units, past) == NULL);
    EXPECT_TRUE(units_get(&units, other_age) == NULL);
    units_free(&units);
}

UTEST(units, going_through_the_slots_skips_the_free_ones)
{
    Units units = {0};
    add(&units, 10);
    UnitId second = add(&units, 20);
    add(&units, 30);
    units_remove(&units, second);
    UnitId id;
    ASSERT_TRUE(units_at(&units, 0, &id) != NULL);
    EXPECT_EQ(10u, units_at(&units, 0, NULL)->type);
    EXPECT_EQ(0u, id.slot);
    EXPECT_TRUE(units_at(&units, 1, NULL) == NULL);
    EXPECT_EQ(30u, units_at(&units, 2, &id)->type);
    EXPECT_EQ(units_get(&units, id), units_at(&units, 2, NULL));
    EXPECT_TRUE(units_at(&units, 3, NULL) == NULL);
    units_free(&units);
}

UTEST(units, the_list_grows_as_units_are_added)
{
    Units units = {0};
    UnitId ids[500];
    for (int i = 0; i < 500; i++) {
        ids[i] = add(&units, (uint16_t)i);
    }
    EXPECT_EQ(500u, units.living);
    for (int i = 0; i < 500; i++) {
        ASSERT_TRUE(units_get(&units, ids[i]) != NULL);
        EXPECT_EQ((unsigned)i, units_get(&units, ids[i])->type);
    }
    units_free(&units);
    EXPECT_EQ(0u, units.count);
    EXPECT_TRUE(units.slots == NULL);
}

UTEST(units, a_map_gives_the_units_a_game_starts_with)
{
    MapUnit records[3];
    memset(records, 0, sizeof records);
    records[0].type = 3;
    records[0].kind = 600;
    records[0].player = 2;
    records[0].x = 4 * 256 + 128;
    records[0].y = 5 * 256 + 64;
    records[0].angle = 10;
    records[0].group = 12;
    records[1].type = 74;
    records[1].angle = 15;
    records[2].type = 1;
    Map map;
    Terrain ground = open_ground();
    memset(&map, 0, sizeof map);
    map.info.width = SIDE;
    map.info.height = SIDE;
    map.units = records;
    map.unit_count = 3;

    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &ground, &units));
    ASSERT_EQ(3u, units.living);
    /* In the map's order, the first in slot 0. */
    const Unit *first = units_at(&units, 0, NULL);
    EXPECT_EQ(3u, first->type);
    EXPECT_EQ(600, first->kind);
    EXPECT_EQ(2, first->player);
    /* In the group its record names, and none for one that names
     * none. */
    EXPECT_EQ(12, first->group);
    EXPECT_EQ(0, units_at(&units, 1, NULL)->group);
    EXPECT_EQ(4 * 256 + 128, first->x);
    EXPECT_EQ(5 * 256 + 64, first->y);
    /* A map counts a turn in 16 steps, the game in 256. */
    EXPECT_EQ(160u, first->facing);
    EXPECT_EQ(160u, first->facing_wanted);
    EXPECT_EQ(240u, units_at(&units, 1, NULL)->facing);
    EXPECT_EQ(0u, units_at(&units, 2, NULL)->facing);
    /* A unit turns as fast as its kind, or as one of no known kind. */
    /* And has all the health and the mana of its kind. */
    EXPECT_EQ(80, first->health);
    EXPECT_EQ(80, first->health_most);
    /* And sees as far as its kind. */
    EXPECT_EQ(7u, first->sight);
    EXPECT_EQ(30, first->mana);
    EXPECT_EQ(30, first->mana_most);
    EXPECT_EQ(0, units_at(&units, 1, NULL)->health_most);
    EXPECT_EQ(19u, first->turn_speed);
    EXPECT_EQ((unsigned)UNITS_TURN_SPEED_UNKNOWN,
        units_at(&units, 1, NULL)->turn_speed);
    units_free(&units);
}

/* A map with nothing but these squares where a player enters. */
static Map map_with_drops(MapDrop *drops, uint32_t count)
{
    Map map;
    memset(&map, 0, sizeof map);
    map.info.width = SIDE;
    map.info.height = SIDE;
    map.drops = drops;
    map.drop_count = count;
    return map;
}

/* A unit of a size and a way of moving in the middle of the square
 * (x, y). */
static Unit unit_on(int x, int y, uint8_t size, uint8_t mover)
{
    Unit unit = unit_of(10);
    unit.x = x * 256 + 128;
    unit.y = y * 256 + 128;
    unit.size = size;
    unit.mover = mover;
    return unit;
}

/* The slot of the unit on a square, or -1 for nobody. */
static int standing_at(const Units *units, bool in_air, int x, int y)
{
    UnitId id = {9999, 9999};
    return units_standing(units, in_air, x, y, &id) ? id.slot : -1;
}

UTEST(units, a_unit_holds_its_square_and_no_other_takes_it)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit first = unit_on(10, 12, 1, UNIT_WALKS);
    UnitId id;
    ASSERT_TRUE(units_add(&units, &first, &id));
    EXPECT_EQ(0, standing_at(&units, false, 10, 12));
    EXPECT_EQ(-1, standing_at(&units, false, 11, 12));
    EXPECT_EQ(-1, standing_at(&units, true, 10, 12));
    /* Neither one that walks nor one that hovers: both are on the
     * ground. */
    Unit second = unit_on(10, 12, 1, UNIT_WALKS);
    EXPECT_FALSE(units_add(&units, &second, NULL));
    second.mover = UNIT_HOVERS;
    EXPECT_FALSE(units_add(&units, &second, NULL));
    EXPECT_EQ(1u, units.living);
    EXPECT_TRUE(units_check(&units));
    /* When the first is gone, the square is free again. */
    units_remove(&units, id);
    EXPECT_EQ(-1, standing_at(&units, false, 10, 12));
    EXPECT_TRUE(units_add(&units, &second, NULL));
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, one_that_flies_is_over_the_ones_on_the_ground)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit walker = unit_on(10, 12, 1, UNIT_WALKS);
    Unit flyer = unit_on(10, 12, 1, UNIT_FLIES);
    ASSERT_TRUE(units_add(&units, &walker, NULL));
    ASSERT_TRUE(units_add(&units, &flyer, NULL));
    EXPECT_EQ(0, standing_at(&units, false, 10, 12));
    EXPECT_EQ(1, standing_at(&units, true, 10, 12));
    /* But not a second in the air. */
    EXPECT_FALSE(units_add(&units, &flyer, NULL));
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_large_unit_holds_every_square_it_covers)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit dragon = unit_on(10, 12, 3, UNIT_WALKS);
    UnitId id;
    ASSERT_TRUE(units_add(&units, &dragon, &id));
    for (int y = 12; y < 15; y++) {
        for (int x = 10; x < 13; x++) {
            EXPECT_EQ(0, standing_at(&units, false, x, y));
        }
    }
    EXPECT_EQ(-1, standing_at(&units, false, 13, 12));
    EXPECT_EQ(-1, standing_at(&units, false, 9, 12));
    EXPECT_EQ(-1, standing_at(&units, false, 10, 15));
    /* One whose squares would lie over any of those is refused. */
    Unit other = unit_on(12, 14, 1, UNIT_WALKS);
    EXPECT_FALSE(units_add(&units, &other, NULL));
    other = unit_on(9, 11, 2, UNIT_WALKS);
    EXPECT_FALSE(units_add(&units, &other, NULL));
    other = unit_on(8, 11, 2, UNIT_WALKS);
    EXPECT_TRUE(units_add(&units, &other, NULL));
    EXPECT_TRUE(units_check(&units));
    units_remove(&units, id);
    EXPECT_EQ(-1, standing_at(&units, false, 12, 14));
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_unit_cannot_stand_off_the_map)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit unit = unit_on(SIDE, 5, 1, UNIT_WALKS);
    EXPECT_FALSE(units_add(&units, &unit, NULL));
    unit = unit_on(5, SIDE, 1, UNIT_FLIES);
    EXPECT_FALSE(units_add(&units, &unit, NULL));
    unit = unit_on(5, 5, 1, UNIT_WALKS);
    unit.x = -1;
    EXPECT_FALSE(units_add(&units, &unit, NULL));
    /* Nor a large one half off it. */
    unit = unit_on(SIDE - 2, 5, 3, UNIT_WALKS);
    EXPECT_FALSE(units_add(&units, &unit, NULL));
    unit = unit_on(SIDE - 3, SIDE - 3, 3, UNIT_WALKS);
    EXPECT_TRUE(units_add(&units, &unit, NULL));
    EXPECT_EQ(-1, standing_at(&units, false, SIDE, 5));
    EXPECT_EQ(-1, standing_at(&units, false, -1, 5));
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, the_check_finds_a_unit_and_its_squares_that_disagree)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit unit = unit_on(10, 12, 2, UNIT_WALKS);
    UnitId id;
    ASSERT_TRUE(units_add(&units, &unit, &id));
    EXPECT_TRUE(units_check(&units));
    /* A unit moved without its squares. */
    units_get(&units, id)->x += 256;
    EXPECT_FALSE(units_check(&units));
    units_get(&units, id)->x -= 256;
    EXPECT_TRUE(units_check(&units));
    /* A square held by nobody that is there. */
    units.on_ground[30 * SIDE + 30] = 1;
    EXPECT_FALSE(units_check(&units));
    units.on_ground[30 * SIDE + 30] = 0;
    /* A square of the unit that says another. */
    units.on_ground[13 * SIDE + 11] = 7;
    EXPECT_FALSE(units_check(&units));
    units.on_ground[13 * SIDE + 11] = 1;
    /* The other layer. */
    units.in_air[2 * SIDE + 2] = 1;
    EXPECT_FALSE(units_check(&units));
    units.in_air[2 * SIDE + 2] = 0;
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_step_holds_the_new_squares_and_the_old_until_it_arrives)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit unit = unit_on(10, 12, 1, UNIT_WALKS);
    UnitId id;
    ASSERT_TRUE(units_add(&units, &unit, &id));
    ASSERT_TRUE(units_step(&units, id, 11, 12));
    const Unit *now = units_get(&units, id);
    /* It is still where it was, and holds both. */
    EXPECT_TRUE(now->reaching);
    EXPECT_EQ(10 * 256 + 128, now->x);
    EXPECT_EQ(10, now->from_x);
    EXPECT_EQ(11, now->to_x);
    EXPECT_EQ(12, now->to_y);
    EXPECT_EQ(0, standing_at(&units, false, 10, 12));
    EXPECT_EQ(0, standing_at(&units, false, 11, 12));
    EXPECT_TRUE(units_check(&units));
    /* Nobody else gets either, and it takes no second step. */
    Unit other = unit_on(11, 12, 1, UNIT_WALKS);
    EXPECT_FALSE(units_add(&units, &other, NULL));
    other = unit_on(10, 12, 1, UNIT_HOVERS);
    EXPECT_FALSE(units_add(&units, &other, NULL));
    EXPECT_FALSE(units_step(&units, id, 12, 12));
    /* Arrived, it is on the new one and the old is free. */
    units_arrive(&units, id);
    EXPECT_FALSE(now->reaching);
    EXPECT_EQ(11 * 256 + 128, now->x);
    EXPECT_EQ(12 * 256 + 128, now->y);
    EXPECT_EQ(-1, standing_at(&units, false, 10, 12));
    EXPECT_EQ(0, standing_at(&units, false, 11, 12));
    EXPECT_TRUE(units_check(&units));
    /* To arrive again changes nothing. */
    units_arrive(&units, id);
    EXPECT_EQ(11 * 256 + 128, now->x);
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_step_onto_a_held_square_or_off_the_map_is_refused)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit unit = unit_on(10, 12, 1, UNIT_WALKS);
    UnitId id;
    UnitId other;
    ASSERT_TRUE(units_add(&units, &unit, &id));
    unit = unit_on(12, 12, 1, UNIT_WALKS);
    ASSERT_TRUE(units_add(&units, &unit, &other));
    /* The other is reaching for the square between them. */
    ASSERT_TRUE(units_step(&units, other, 11, 12));
    EXPECT_FALSE(units_can_step(&units, id, 11, 12));
    EXPECT_FALSE(units_step(&units, id, 11, 12));
    EXPECT_FALSE(units_get(&units, id)->reaching);
    EXPECT_FALSE(units_step(&units, id, -1, 12));
    EXPECT_FALSE(units_step(&units, id, 10, SIDE));
    /* One in the air goes over them. */
    unit = unit_on(10, 13, 1, UNIT_FLIES);
    UnitId flyer;
    ASSERT_TRUE(units_add(&units, &unit, &flyer));
    EXPECT_TRUE(units_step(&units, flyer, 11, 12));
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_large_unit_that_steps_holds_all_of_both_places)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit dragon = unit_on(10, 12, 3, UNIT_WALKS);
    UnitId id;
    ASSERT_TRUE(units_add(&units, &dragon, &id));
    /* One square right and down: the two places share four squares,
     * and together are fourteen. */
    ASSERT_TRUE(units_step(&units, id, 11, 13));
    int held = 0;
    for (int y = 11; y < 17; y++) {
        for (int x = 9; x < 15; x++) {
            held += standing_at(&units, false, x, y) == 0 ? 1 : 0;
        }
    }
    EXPECT_EQ(14, held);
    EXPECT_TRUE(units_check(&units));
    units_arrive(&units, id);
    held = 0;
    for (int y = 11; y < 17; y++) {
        for (int x = 9; x < 15; x++) {
            held += standing_at(&units, false, x, y) == 0 ? 1 : 0;
        }
    }
    EXPECT_EQ(9, held);
    EXPECT_EQ(-1, standing_at(&units, false, 10, 12));
    EXPECT_EQ(0, standing_at(&units, false, 13, 15));
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_unit_taken_away_in_a_step_frees_both_places)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit unit = unit_on(10, 12, 2, UNIT_WALKS);
    UnitId id;
    ASSERT_TRUE(units_add(&units, &unit, &id));
    ASSERT_TRUE(units_step(&units, id, 12, 12));
    units_remove(&units, id);
    for (int x = 10; x < 14; x++) {
        EXPECT_EQ(-1, standing_at(&units, false, x, 12));
        EXPECT_EQ(-1, standing_at(&units, false, x, 13));
    }
    EXPECT_TRUE(units_check(&units));
    /* The next unit in the slot does not start out reaching. */
    unit = unit_on(20, 20, 1, UNIT_WALKS);
    unit.reaching = true;
    ASSERT_TRUE(units_add(&units, &unit, &id));
    EXPECT_FALSE(units_get(&units, id)->reaching);
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, the_check_finds_a_reaching_unit_that_does_not_hold_its_next)
{
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit unit = unit_on(10, 12, 1, UNIT_WALKS);
    UnitId id;
    ASSERT_TRUE(units_add(&units, &unit, &id));
    ASSERT_TRUE(units_step(&units, id, 11, 12));
    units.on_ground[12 * SIDE + 11] = 0;
    EXPECT_FALSE(units_check(&units));
    units.on_ground[12 * SIDE + 11] = 1;
    EXPECT_TRUE(units_check(&units));
    /* Or one that says it is not reaching and still holds it. */
    units_get(&units, id)->reaching = false;
    EXPECT_FALSE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_list_on_no_map_takes_any_unit_and_checks_as_false)
{
    Units units = {0};
    Unit unit = unit_on(10, 12, 1, UNIT_WALKS);
    EXPECT_TRUE(units_add(&units, &unit, NULL));
    EXPECT_TRUE(units_add(&units, &unit, NULL));
    EXPECT_EQ(-1, standing_at(&units, false, 10, 12));
    EXPECT_FALSE(units_check(&units));
    units_free(&units);
}

UTEST(units, units_of_a_map_that_share_a_square_are_moved_apart)
{
    MapUnit records[3];
    memset(records, 0, sizeof records);
    for (int i = 0; i < 3; i++) {
        records[i].x = 20 * 256 + 128;
        records[i].y = 20 * 256 + 128;
    }
    Map map;
    Terrain ground = open_ground();
    memset(&map, 0, sizeof map);
    map.info.width = SIDE;
    map.info.height = SIDE;
    map.units = records;
    map.unit_count = 3;
    /* The square to the top left is water. */
    open_closed[19 * SIDE + 19] = TERRAIN_NO_WALK;
    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &ground, &units));
    ASSERT_EQ(3u, units.living);
    EXPECT_EQ(0, standing_at(&units, false, 20, 20));
    EXPECT_EQ(1, standing_at(&units, false, 20, 19));
    EXPECT_EQ(2, standing_at(&units, false, 21, 19));
    /* In the middle of its new square. */
    EXPECT_EQ(20 * 256 + 128, units_at(&units, 1, NULL)->x);
    EXPECT_EQ(19 * 256 + 128, units_at(&units, 1, NULL)->y);
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_unit_of_a_map_stands_where_its_record_says_whatever_is_there)
{
    MapUnit records[2];
    memset(records, 0, sizeof records);
    records[0].x = 20 * 256 + 128;
    records[0].y = 20 * 256 + 128;
    /* The second is off the map, and is left out. */
    records[1].x = SIDE * 256 + 128;
    records[1].y = 20 * 256 + 128;
    Map map;
    Terrain ground = open_ground();
    memset(&map, 0, sizeof map);
    map.info.width = SIDE;
    map.info.height = SIDE;
    map.units = records;
    map.unit_count = 2;
    memset(open_closed, TERRAIN_NO_ONE, sizeof open_closed);
    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &ground, &units));
    ASSERT_EQ(1u, units.living);
    EXPECT_EQ(0, standing_at(&units, false, 20, 20));
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_hero_enters_in_the_middle_of_the_square_for_it)
{
    MapDrop drops[1] = {{11, 41}};
    Map map = map_with_drops(drops, 1);
    Terrain ground = open_ground();
    kinds[UNITS_HERO_KIND].rotation_speed = 18;
    kinds[UNITS_HERO_KIND].health = 20;
    kinds[UNITS_HERO_KIND].size = 1;
    kinds[UNITS_HERO_KIND].mover = UNIT_WALKS;
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    UnitId id = {9999, 9999};
    UnitsHero who = {1, 3, UNIT_DRESS_HERO_ARMOUR};
    Random random;
    random_start(&random, 1);
    ASSERT_TRUE(units_enter(&units, &map, some_kinds(), &ground, &who,
        &random, &id));
    ASSERT_EQ(1u, units.living);
    const Unit *hero = units_get(&units, id);
    ASSERT_TRUE(hero != NULL);
    EXPECT_EQ(3u, hero->type);
    EXPECT_EQ((unsigned)UNIT_DRESS_HERO_ARMOUR, hero->dress);
    EXPECT_EQ(UNITS_HERO_KIND, hero->kind);
    EXPECT_EQ(1, hero->player);
    EXPECT_EQ(11 * 256 + 128, hero->x);
    EXPECT_EQ(41 * 256 + 128, hero->y);
    EXPECT_EQ(0u, hero->facing);
    EXPECT_EQ(0u, hero->facing_wanted);
    EXPECT_FALSE(hero->turning);
    EXPECT_EQ(18u, hero->turn_speed);
    EXPECT_EQ(20, hero->health);
    EXPECT_EQ(20, hero->health_most);
    /* He stands there from now on. */
    UnitId found = {9999, 9999};
    EXPECT_TRUE(units_standing(&units, false, 11, 41, &found));
    EXPECT_EQ(id.slot, found.slot);
    EXPECT_TRUE(units_check(&units));
    memset(&kinds[UNITS_HERO_KIND], 0, sizeof kinds[0]);
    units_free(&units);
}

UTEST(units, the_dice_pick_the_square_a_hero_enters_on)
{
    MapDrop drops[3] = {{12, 12}, {25, 26}, {38, 38}};
    Map map = map_with_drops(drops, 3);
    Terrain ground = open_ground();
    UnitsHero who = {1, 3, UNIT_DRESS_HERO_LIGHT};
    bool entered[3] = {false, false, false};
    for (uint32_t seed = 1; seed <= 40; seed++) {
        Random random;
        random_start(&random, seed);
        /* What the same dice would say. */
        Random same = random;
        uint32_t picked = random_below(&same, 3);
        Units units;
        ASSERT_TRUE(units_make(SIDE, SIDE, &units));
        UnitId id;
        ASSERT_TRUE(units_enter(&units, &map, some_kinds(), &ground, &who,
            &random, &id));
        EXPECT_EQ(drops[picked].x * 256 + 128, units_get(&units, id)->x);
        EXPECT_EQ(drops[picked].y * 256 + 128, units_get(&units, id)->y);
        /* One throw, and no more. */
        EXPECT_EQ(random_next(&same), random_next(&random));
        entered[picked] = true;
        units_free(&units);
    }
    EXPECT_TRUE(entered[0] && entered[1] && entered[2]);
}

/* Lets a hero enter a map with the one square (20, 20) to enter on,
 * with `units` already there, and gives his square: its x plus 100
 * times its y, or -1 if he did not enter. */
static int32_t enters_at(Units *units, const Terrain *ground)
{
    MapDrop drops[1] = {{20, 20}};
    Map map = map_with_drops(drops, 1);
    UnitsHero who = {1, 3, UNIT_DRESS_HERO_LIGHT};
    Random random;
    random_start(&random, 1);
    UnitId id;
    if (!units_enter(units, &map, some_kinds(), ground, &who, &random,
            &id)) {
        return -1;
    }
    const Unit *hero = units_get(units, id);
    return hero->x / 256 + 100 * (hero->y / 256);
}

UTEST(units, a_hero_takes_the_nearest_free_square_if_his_is_taken)
{
    Terrain ground = open_ground();
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Unit other = unit_of(7);
    /* Anywhere on the square is on it. */
    other.x = 20 * 256 + 250;
    other.y = 20 * 256 + 3;
    ASSERT_TRUE(units_add(&units, &other, NULL));
    /* The ring around it, from its top left. */
    EXPECT_EQ(19 + 100 * 19, enters_at(&units, &ground));
    EXPECT_EQ(20 + 100 * 19, enters_at(&units, &ground));
    EXPECT_EQ(21 + 100 * 19, enters_at(&units, &ground));
    EXPECT_EQ(19 + 100 * 20, enters_at(&units, &ground));
    EXPECT_EQ(21 + 100 * 20, enters_at(&units, &ground));
    EXPECT_EQ(5u, units.living - 1);
    EXPECT_TRUE(units_check(&units));
    units_free(&units);
}

UTEST(units, a_hero_does_not_enter_on_ground_closed_to_him)
{
    Terrain ground = open_ground();
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    /* Water on his square and all around it but for one square, and a
     * wall there, which stops what flies too. */
    for (int y = 19; y <= 21; y++) {
        for (int x = 19; x <= 21; x++) {
            open_closed[y * SIDE + x] = TERRAIN_NO_WALK;
        }
    }
    open_closed[21 * SIDE + 20] = 0;
    EXPECT_EQ(20 + 100 * 21, enters_at(&units, &ground));
    /* The next goes to the ring beyond. */
    EXPECT_EQ(18 + 100 * 18, enters_at(&units, &ground));
    units_free(&units);
}

UTEST(units, no_hero_enters_where_no_square_near_is_free)
{
    Terrain ground = open_ground();
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    memset(open_closed, TERRAIN_NO_ONE, sizeof open_closed);
    /* A free square one too far away. */
    open_closed[20 * SIDE + 20 + UNITS_NEAR + 1] = 0;
    Random random;
    random_start(&random, 1);
    EXPECT_EQ(-1, enters_at(&units, &ground));
    EXPECT_EQ(0u, units.living);
    open_closed[20 * SIDE + 20 + UNITS_NEAR] = 0;
    EXPECT_EQ(20 + UNITS_NEAR + 100 * 20, enters_at(&units, &ground));
    units_free(&units);
}

UTEST(units, no_hero_enters_a_map_without_a_square_for_it)
{
    Map map = map_with_drops(NULL, 0);
    Terrain ground = open_ground();
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    UnitId id = {9999, 9999};
    UnitsHero who = {1, 3, UNIT_DRESS_HERO_LIGHT};
    Random random;
    random_start(&random, 1);
    Random same = random;
    EXPECT_FALSE(units_enter(&units, &map, some_kinds(), &ground, &who,
        &random, &id));
    EXPECT_EQ(0u, units.living);
    EXPECT_EQ(9999u, id.slot);
    /* The dice were not thrown. */
    EXPECT_EQ(random_next(&same), random_next(&random));
    units_free(&units);
}

UTEST(units, a_record_may_say_its_own_health)
{
    MapUnit records[3];
    memset(records, 0, sizeof records);
    for (int i = 0; i < 3; i++) {
        records[i].kind = 600;
    }
    records[0].health = 25;
    records[0].health_max = 100;
    /* The others say none, as most do, or only half of it. */
    records[1].health = -1;
    records[1].health_max = -1;
    records[2].health = 25;
    records[2].health_max = -1;
    Map map;
    Terrain ground = open_ground();
    memset(&map, 0, sizeof map);
    map.info.width = SIDE;
    map.info.height = SIDE;
    map.units = records;
    map.unit_count = 3;
    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &ground, &units));
    EXPECT_EQ(25, units_at(&units, 0, NULL)->health);
    EXPECT_EQ(100, units_at(&units, 0, NULL)->health_most);
    EXPECT_EQ(30, units_at(&units, 0, NULL)->mana);
    EXPECT_EQ(80, units_at(&units, 1, NULL)->health);
    EXPECT_EQ(80, units_at(&units, 1, NULL)->health_most);
    EXPECT_EQ(80, units_at(&units, 2, NULL)->health);
    EXPECT_EQ(80, units_at(&units, 2, NULL)->health_most);
    units_free(&units);
}

UTEST(units, a_tick_is_a_tick_of_every_unit_there_is)
{
    Units units = {0};
    Random random;
    random_start(&random, 1);
    Unit unit = unit_of(10);
    unit.turn_speed = 16;
    unit.facing_wanted = 64;
    UnitId first;
    UnitId second;
    UnitId third;
    units_add(&units, &unit, &first);
    units_add(&units, &unit, &second);
    units_add(&units, &unit, &third);
    units_remove(&units, second);
    units_tick(&units, NULL, NULL, &random);
    EXPECT_EQ(16u, units_get(&units, first)->facing);
    EXPECT_EQ(16u, units_get(&units, third)->facing);
    units_tick(&units, NULL, NULL, &random);
    EXPECT_EQ(32u, units_get(&units, first)->facing);
    units_free(&units);
}

/* Whether a unit of `player` has looked another way after 5000 ticks
 * of a game in which a person plays the players of `people`. */
static bool looks_around(int32_t player, uint32_t people)
{
    Units units = {0};
    units.people = people;
    Random random;
    random_start(&random, 5);
    Unit unit = unit_of(10);
    unit.turn_speed = 16;
    unit.facing = 40;
    unit.facing_wanted = 40;
    unit.player = player;
    UnitId id;
    units_add(&units, &unit, &id);
    for (int tick = 0; tick < 5000; tick++) {
        units_tick(&units, NULL, NULL, &random);
    }
    bool looked = units_get(&units, id)->facing != 40;
    units_free(&units);
    return looked;
}

UTEST(units, only_a_unit_the_computer_plays_looks_around)
{
    /* A person plays the first player and the third. */
    EXPECT_FALSE(looks_around(1, 5u));
    EXPECT_TRUE(looks_around(2, 5u));
    EXPECT_FALSE(looks_around(3, 5u));
    EXPECT_FALSE(looks_around(32, 0x80000000u));
    /* A player the game does not count is the computer's. */
    EXPECT_TRUE(looks_around(0, 0xffffffffu));
    EXPECT_TRUE(looks_around(33, 0xffffffffu));
    EXPECT_TRUE(looks_around(-4, 0xffffffffu));
}

UTEST(units, a_hero_makes_his_player_a_persons)
{
    MapDrop drops[1] = {{3, 3}};
    Map map = map_with_drops(drops, 1);
    Terrain ground = open_ground();
    Units units;
    ASSERT_TRUE(units_make(SIDE, SIDE, &units));
    Random random;
    random_start(&random, 5);
    UnitsHero who = {UNITS_PLAYER_SELF, 3, UNIT_DRESS_HERO_ARMOUR};
    UnitId hero;
    ASSERT_TRUE(units_enter(&units, &map, some_kinds(), &ground, &who,
        &random, &hero));
    EXPECT_EQ(1u, units.people);
    /* One more of his player, as a map may have, and one of another. */
    Unit unit = unit_of(10);
    unit.turn_speed = 16;
    UnitId own;
    UnitId other;
    unit.player = UNITS_PLAYER_SELF;
    unit.x = 5 * 256;
    ASSERT_TRUE(units_add(&units, &unit, &own));
    unit.player = 2;
    unit.x = 6 * 256;
    ASSERT_TRUE(units_add(&units, &unit, &other));
    for (int tick = 0; tick < 5000; tick++) {
        units_tick(&units, NULL, NULL, &random);
    }
    EXPECT_EQ(0u, units_get(&units, hero)->facing);
    EXPECT_EQ(0u, units_get(&units, own)->facing);
    EXPECT_NE(0u, units_get(&units, other)->facing);
    units_free(&units);
}

UTEST(units, no_dice_are_thrown_for_a_unit_of_a_person)
{
    Units units = {0};
    units.people = 1u;
    Random random;
    random_start(&random, 5);
    Random same = random;
    Unit unit = unit_of(10);
    unit.player = 1;
    units_add(&units, &unit, NULL);
    for (int tick = 0; tick < 100; tick++) {
        units_tick(&units, NULL, NULL, &random);
    }
    EXPECT_EQ(random_next(&same), random_next(&random));
    units_free(&units);
}

UTEST(units, a_unit_of_a_person_still_turns_as_told)
{
    Units units = {0};
    units.people = 1u;
    Random random;
    random_start(&random, 5);
    Unit unit = unit_of(10);
    unit.player = 1;
    unit.turn_speed = 16;
    unit.facing_wanted = 64;
    UnitId id;
    units_add(&units, &unit, &id);
    units_tick(&units, NULL, NULL, &random);
    EXPECT_EQ(16u, units_get(&units, id)->facing);
    units_free(&units);
}

UTEST(units, the_same_dice_play_the_same_game)
{
    MapUnit records[20];
    memset(records, 0, sizeof records);
    for (int i = 0; i < 20; i++) {
        records[i].angle = i % 16;
        records[i].kind = i % 2 == 0 ? 600 : 0;
    }
    Map map;
    Terrain ground = open_ground();
    memset(&map, 0, sizeof map);
    map.info.width = SIDE;
    map.info.height = SIDE;
    map.units = records;
    map.unit_count = 20;
    Units a = {0};
    Units b = {0};
    Random dice_a;
    Random dice_b;
    random_start(&dice_a, 42);
    random_start(&dice_b, 42);
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &ground, &a));
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &ground, &b));
    for (int tick = 0; tick < 3000; tick++) {
        units_tick(&a, NULL, NULL, &dice_a);
        units_tick(&b, NULL, NULL, &dice_b);
    }
    int moved = 0;
    for (uint32_t slot = 0; slot < 20; slot++) {
        EXPECT_EQ(units_at(&a, slot, NULL)->facing,
            units_at(&b, slot, NULL)->facing);
        moved += units_at(&a, slot, NULL)->facing
            != (uint8_t)(records[slot].angle * 16);
    }
    EXPECT_GT(moved, 10);
    EXPECT_EQ(dice_a.state, dice_b.state);
    units_free(&a);
    units_free(&b);
}

UTEST(units, a_record_that_faces_a_way_there_is_not_faces_the_first)
{
    MapUnit records[2];
    memset(records, 0, sizeof records);
    records[0].angle = 16;
    records[1].angle = -1;
    Map map;
    Terrain ground = open_ground();
    memset(&map, 0, sizeof map);
    map.info.width = SIDE;
    map.info.height = SIDE;
    map.units = records;
    map.unit_count = 2;
    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &ground, &units));
    EXPECT_EQ(0u, units_at(&units, 0, NULL)->facing);
    EXPECT_EQ(0u, units_at(&units, 1, NULL)->facing);
    units_free(&units);
}

UTEST(units, a_map_without_units_gives_an_empty_list)
{
    Map map;
    Terrain ground = open_ground();
    memset(&map, 0, sizeof map);
    map.info.width = SIDE;
    map.info.height = SIDE;
    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &ground, &units));
    EXPECT_EQ(0u, units.living);
    EXPECT_EQ((unsigned)SIDE, units.width);
    EXPECT_TRUE(units_check(&units));
    EXPECT_TRUE(units_at(&units, 0, NULL) == NULL);
    units_free(&units);
}
