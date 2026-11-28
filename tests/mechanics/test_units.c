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
    return kinds;
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
    records[1].type = 74;
    records[1].angle = 15;
    records[2].type = 1;
    Map map;
    memset(&map, 0, sizeof map);
    map.units = records;
    map.unit_count = 3;

    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &units));
    ASSERT_EQ(3u, units.living);
    /* In the map's order, the first in slot 0. */
    const Unit *first = units_at(&units, 0, NULL);
    EXPECT_EQ(3u, first->type);
    EXPECT_EQ(600, first->kind);
    EXPECT_EQ(2, first->player);
    EXPECT_EQ(4 * 256 + 128, first->x);
    EXPECT_EQ(5 * 256 + 64, first->y);
    /* A map counts a turn in 16 steps, the game in 256. */
    EXPECT_EQ(160u, first->facing);
    EXPECT_EQ(160u, first->facing_wanted);
    EXPECT_EQ(240u, units_at(&units, 1, NULL)->facing);
    EXPECT_EQ(0u, units_at(&units, 2, NULL)->facing);
    /* A unit turns as fast as its kind, or as one of no known kind. */
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
    map.drops = drops;
    map.drop_count = count;
    return map;
}

UTEST(units, a_hero_enters_in_the_middle_of_the_square_for_it)
{
    MapDrop drops[1] = {{11, 41}};
    Map map = map_with_drops(drops, 1);
    kinds[UNITS_HERO_KIND].rotation_speed = 18;
    Units units = {0};
    UnitId id = {9999, 9999};
    UnitsHero who = {1, 3, UNIT_DRESS_HERO_ARMOUR};
    Random random;
    random_start(&random, 1);
    ASSERT_TRUE(units_enter(&units, &map, some_kinds(), &who, &random,
        &id));
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
    kinds[UNITS_HERO_KIND].rotation_speed = 0;
    units_free(&units);
}

UTEST(units, the_dice_pick_the_square_a_hero_enters_on)
{
    MapDrop drops[3] = {{2, 2}, {5, 6}, {8, 8}};
    Map map = map_with_drops(drops, 3);
    UnitsHero who = {1, 3, UNIT_DRESS_HERO_LIGHT};
    bool entered[3] = {false, false, false};
    for (uint32_t i = 1; i <= 40; i++) {
        /* Seeds far from each other: the first throws from a small
         * seed are all small. */
        Random random;
        random_start(&random, i * 2654435761u);
        /* What the same dice would say. */
        Random same = random;
        uint32_t picked = random_below(&same, 3);
        Units units = {0};
        UnitId id;
        ASSERT_TRUE(units_enter(&units, &map, some_kinds(), &who, &random,
            &id));
        EXPECT_EQ(drops[picked].x * 256 + 128, units_get(&units, id)->x);
        EXPECT_EQ(drops[picked].y * 256 + 128, units_get(&units, id)->y);
        /* One throw, and no more. */
        EXPECT_EQ(random_next(&same), random_next(&random));
        entered[picked] = true;
        units_free(&units);
    }
    EXPECT_TRUE(entered[0] && entered[1] && entered[2]);
}

UTEST(units, a_hero_takes_the_next_square_if_a_unit_stands_on_his)
{
    MapDrop drops[3] = {{2, 2}, {5, 6}, {8, 8}};
    Map map = map_with_drops(drops, 3);
    UnitsHero who = {1, 3, UNIT_DRESS_HERO_LIGHT};
    Random random;
    random_start(&random, 1);
    Random same = random;
    uint32_t picked = random_below(&same, 3);
    Units units = {0};
    Unit other = unit_of(7);
    /* Anywhere on the square is on it. */
    other.x = drops[picked].x * 256 + 250;
    other.y = drops[picked].y * 256 + 3;
    units_add(&units, &other, NULL);
    UnitId id;
    ASSERT_TRUE(units_enter(&units, &map, some_kinds(), &who, &random,
        &id));
    /* The next in the map's order, and round to the first. */
    const MapDrop *next = &drops[(picked + 1) % 3];
    EXPECT_EQ(next->x * 256 + 128, units_get(&units, id)->x);
    EXPECT_EQ(next->y * 256 + 128, units_get(&units, id)->y);
    units_free(&units);
}

UTEST(units, a_hero_takes_the_picked_square_if_a_unit_stands_on_each)
{
    MapDrop drops[2] = {{2, 2}, {5, 6}};
    Map map = map_with_drops(drops, 2);
    UnitsHero who = {1, 3, UNIT_DRESS_HERO_LIGHT};
    Random random;
    random_start(&random, 1);
    Random same = random;
    uint32_t picked = random_below(&same, 2);
    Units units = {0};
    for (int i = 0; i < 2; i++) {
        Unit other = unit_of(7);
        other.x = drops[i].x * 256 + 128;
        other.y = drops[i].y * 256 + 128;
        units_add(&units, &other, NULL);
    }
    UnitId id;
    ASSERT_TRUE(units_enter(&units, &map, some_kinds(), &who, &random,
        &id));
    EXPECT_EQ(drops[picked].x * 256 + 128, units_get(&units, id)->x);
    EXPECT_EQ(3u, units.living);
    units_free(&units);
}

UTEST(units, no_hero_enters_a_map_without_a_square_for_it)
{
    Map map = map_with_drops(NULL, 0);
    Units units = {0};
    UnitId id = {9999, 9999};
    UnitsHero who = {1, 3, UNIT_DRESS_HERO_LIGHT};
    Random random;
    random_start(&random, 1);
    Random same = random;
    EXPECT_FALSE(units_enter(&units, &map, some_kinds(), &who, &random,
        &id));
    EXPECT_EQ(0u, units.living);
    EXPECT_EQ(9999u, id.slot);
    /* The dice were not thrown. */
    EXPECT_EQ(random_next(&same), random_next(&random));
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
    units_tick(&units, &random);
    EXPECT_EQ(16u, units_get(&units, first)->facing);
    EXPECT_EQ(16u, units_get(&units, third)->facing);
    units_tick(&units, &random);
    EXPECT_EQ(32u, units_get(&units, first)->facing);
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
    memset(&map, 0, sizeof map);
    map.units = records;
    map.unit_count = 20;
    Units a = {0};
    Units b = {0};
    Random dice_a;
    Random dice_b;
    random_start(&dice_a, 42);
    random_start(&dice_b, 42);
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &a));
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &b));
    for (int tick = 0; tick < 3000; tick++) {
        units_tick(&a, &dice_a);
        units_tick(&b, &dice_b);
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
    memset(&map, 0, sizeof map);
    map.units = records;
    map.unit_count = 2;
    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &units));
    EXPECT_EQ(0u, units_at(&units, 0, NULL)->facing);
    EXPECT_EQ(0u, units_at(&units, 1, NULL)->facing);
    units_free(&units);
}

UTEST(units, a_map_without_units_gives_an_empty_list)
{
    Map map;
    memset(&map, 0, sizeof map);
    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, some_kinds(), &units));
    EXPECT_EQ(0u, units.living);
    EXPECT_TRUE(units_at(&units, 0, NULL) == NULL);
    units_free(&units);
}
