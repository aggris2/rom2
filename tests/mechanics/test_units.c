#include "mechanics/units.h"

#include <string.h>

#include "utest.h"

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
    ASSERT_TRUE(units_from_map(&map, &units));
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
    units_free(&units);
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
    ASSERT_TRUE(units_from_map(&map, &units));
    EXPECT_EQ(0u, units_at(&units, 0, NULL)->facing);
    EXPECT_EQ(0u, units_at(&units, 1, NULL)->facing);
    units_free(&units);
}

UTEST(units, a_map_without_units_gives_an_empty_list)
{
    Map map;
    memset(&map, 0, sizeof map);
    Units units = {0};
    ASSERT_TRUE(units_from_map(&map, &units));
    EXPECT_EQ(0u, units.living);
    EXPECT_TRUE(units_at(&units, 0, NULL) == NULL);
    units_free(&units);
}
