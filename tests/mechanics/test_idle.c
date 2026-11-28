#include "mechanics/idle.h"

#include <string.h>

#include "utest.h"

/* A unit that stands and faces a way, with a turning speed. */
static Unit standing(uint8_t way, uint8_t speed)
{
    Unit unit;
    memset(&unit, 0, sizeof unit);
    unit.facing = way;
    unit.facing_wanted = way;
    unit.turn_speed = speed;
    return unit;
}

UTEST(idle, a_standing_unit_seldom_looks_around)
{
    /* Over many ticks of standing units: 205 in 32,768 is 1 in 160. */
    Random random;
    random_start(&random, 7);
    int looked = 0;
    for (int i = 0; i < 160000; i++) {
        Unit unit = standing((uint8_t)i, 16);
        idle_tick(&unit, &random);
        looked += unit.facing_wanted != (uint8_t)i;
    }
    EXPECT_GT(looked, 850);
    EXPECT_LT(looked, 1150);
}

UTEST(idle, a_unit_that_looks_around_wants_a_way_well_off_the_one_it_faces)
{
    Random random;
    random_start(&random, 11);
    int looked = 0;
    for (int i = 0; i < 100000; i++) {
        Unit unit = standing((uint8_t)i, 16);
        idle_tick(&unit, &random);
        if (unit.facing_wanted == (uint8_t)i) {
            /* It stands as it stood. */
            EXPECT_EQ((uint8_t)i, unit.facing);
            continue;
        }
        looked++;
        uint8_t off = (uint8_t)(unit.facing_wanted - (uint8_t)i);
        EXPECT_GE(off, IDLE_TURN_LEAST);
        EXPECT_LT(off, IDLE_TURN_LEAST + IDLE_TURN_SPREAD);
        /* It has begun to turn there in the same tick. */
        EXPECT_NE((uint8_t)i, unit.facing);
    }
    EXPECT_GT(looked, 400);
}

UTEST(idle, a_unit_turns_to_the_way_it_wants_tick_by_tick)
{
    Random random;
    random_start(&random, 1);
    Unit unit = standing(0, 16);
    unit.facing_wanted = 64;
    Random before = random;
    for (int tick = 0; tick < 4; tick++) {
        idle_tick(&unit, &random);
        EXPECT_EQ((tick + 1) * 16, unit.facing);
    }
    EXPECT_FALSE(unit.turning);
    /* The dice are not asked while it turns. */
    EXPECT_EQ(before.state, random.state);
}

UTEST(idle, the_same_dice_make_the_same_unit_do_the_same)
{
    Random a;
    Random b;
    random_start(&a, 2026);
    random_start(&b, 2026);
    Unit first = standing(10, 19);
    Unit second = standing(10, 19);
    for (int tick = 0; tick < 5000; tick++) {
        idle_tick(&first, &a);
        idle_tick(&second, &b);
        ASSERT_EQ(first.facing, second.facing);
    }
    EXPECT_EQ(a.state, b.state);
}

UTEST(idle, a_unit_left_alone_mostly_stands_and_sometimes_turns)
{
    /* 16,000 ticks are 1,000 seconds at the speed a game starts on:
     * about a hundred looks, each a few ticks of turning. */
    Random random;
    random_start(&random, 3);
    Unit unit = standing(0, 16);
    int turned = 0;
    uint8_t last = unit.facing;
    for (int tick = 0; tick < 16000; tick++) {
        idle_tick(&unit, &random);
        turned += unit.facing != last;
        last = unit.facing;
    }
    EXPECT_GT(turned, 200);
    EXPECT_LT(turned, 1600);
}
