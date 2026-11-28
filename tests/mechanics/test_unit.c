#include "mechanics/unit.h"

#include <string.h>

#include "utest.h"

/* A unit that faces one way, wants another and turns at a speed. */
static Unit facing(uint8_t now, uint8_t wanted, uint8_t speed)
{
    Unit unit;
    memset(&unit, 0, sizeof unit);
    unit.facing = now;
    unit.facing_wanted = wanted;
    unit.turn_speed = speed;
    return unit;
}

UTEST(unit, turns_by_its_speed_each_tick_until_it_faces_as_it_wants)
{
    Unit unit = facing(0, 100, 16);
    const uint8_t steps[7] = {16, 32, 48, 64, 80, 96, 100};
    for (int tick = 0; tick < 7; tick++) {
        unit_turn(&unit);
        EXPECT_EQ(steps[tick], unit.facing);
        EXPECT_EQ(tick < 6, unit.turning);
    }
    /* And then stands. */
    unit_turn(&unit);
    EXPECT_EQ(100u, unit.facing);
    EXPECT_FALSE(unit.turning);
}

UTEST(unit, turns_the_shorter_way_round)
{
    Unit back = facing(100, 20, 16);
    unit_turn(&back);
    EXPECT_EQ(84u, back.facing);
    /* Over the place where the count starts again, both ways. */
    Unit over = facing(240, 40, 16);
    unit_turn(&over);
    EXPECT_EQ(0u, over.facing);
    Unit under = facing(10, 200, 16);
    unit_turn(&under);
    EXPECT_EQ(250u, under.facing);
}

UTEST(unit, half_a_turn_goes_forward)
{
    Unit unit = facing(0, 128, 16);
    unit_turn(&unit);
    EXPECT_EQ(16u, unit.facing);
}

UTEST(unit, a_small_turn_is_taken_at_once)
{
    Unit unit = facing(50, 82, 4);
    unit_turn(&unit);
    EXPECT_EQ(82u, unit.facing);
    EXPECT_FALSE(unit.turning);
    /* 33 steps are not small. */
    Unit more = facing(50, 83, 4);
    unit_turn(&more);
    EXPECT_EQ(54u, more.facing);
    EXPECT_TRUE(more.turning);
}

UTEST(unit, a_unit_that_is_turning_does_not_jump_the_rest_of_the_way)
{
    /* 40 steps at 4 a tick: after two ticks 32 are left, which a unit
     * that stood would take at once. */
    Unit unit = facing(0, 40, 4);
    unit_turn(&unit);
    unit_turn(&unit);
    unit_turn(&unit);
    EXPECT_EQ(12u, unit.facing);
    EXPECT_TRUE(unit.turning);
}

UTEST(unit, the_last_step_is_no_longer_than_what_is_left)
{
    Unit unit = facing(0, 40, 16);
    unit_turn(&unit);
    unit_turn(&unit);
    EXPECT_EQ(32u, unit.facing);
    unit_turn(&unit);
    EXPECT_EQ(40u, unit.facing);
    EXPECT_FALSE(unit.turning);
}
