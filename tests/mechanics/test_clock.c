#include "mechanics/clock.h"

#include <math.h>

#include "utest.h"

UTEST(clock, the_speeds_are_the_originals)
{
    const uint32_t ticks[CLOCK_SPEEDS] = {8, 10, 12, 14, 16, 20, 24, 28, 32};
    for (uint32_t speed = 0; speed < CLOCK_SPEEDS; speed++) {
        EXPECT_EQ(ticks[speed], clock_ticks_a_second(speed));
    }
    EXPECT_EQ(0u, clock_ticks_a_second(CLOCK_SPEEDS));
}

UTEST(clock, starts_at_no_tick_on_the_speed_a_game_starts_on)
{
    Clock clock;
    clock_start(&clock);
    EXPECT_EQ(0u, (unsigned)clock.tick);
    EXPECT_EQ(4u, clock.speed);
    EXPECT_FALSE(clock.paused);
    EXPECT_EQ(16u, clock_ticks_a_second(clock.speed));
}

UTEST(clock, a_second_is_as_many_ticks_as_the_speed_says)
{
    Clock clock;
    clock_start(&clock);
    /* 16 in a second, in 16 frames of a 16th of a second. */
    uint32_t ticks = 0;
    for (int frame = 0; frame < 16; frame++) {
        ticks += clock_advance(&clock, 1.0 / 16.0);
    }
    EXPECT_EQ(16u, ticks);
    EXPECT_EQ(16u, (unsigned)clock.tick);
}

UTEST(clock, short_frames_add_up_to_a_tick)
{
    Clock clock;
    clock_start(&clock);
    /* A monitor of 165 frames a second: 1650 frames are 10 seconds,
     * and 160 ticks. No frame has more than one. */
    uint32_t ticks = 0;
    for (int frame = 0; frame < 1650; frame++) {
        uint32_t now = clock_advance(&clock, 1.0 / 165.0);
        EXPECT_LE(now, 1u);
        ticks += now;
    }
    /* The last may fall just short for the rounding of the sum. */
    EXPECT_GE(ticks, 159u);
    EXPECT_LE(ticks, 160u);
}

UTEST(clock, a_long_frame_has_several_ticks)
{
    Clock clock;
    clock_start(&clock);
    EXPECT_EQ(4u, clock_advance(&clock, 0.25));
    EXPECT_EQ(4u, (unsigned)clock.tick);
}

UTEST(clock, a_halt_passes_no_more_than_the_most_ticks)
{
    Clock clock;
    clock_start(&clock);
    EXPECT_EQ((unsigned)CLOCK_MOST_TICKS, clock_advance(&clock, 30.0));
    EXPECT_EQ((unsigned)CLOCK_MOST_TICKS, (unsigned)clock.tick);
    /* And nothing of the halt is left over. */
    EXPECT_EQ(0u, clock_advance(&clock, 1.0 / 32.0));
    EXPECT_EQ(1u, clock_advance(&clock, 1.0 / 32.0));
}

UTEST(clock, a_paused_clock_stands_still)
{
    Clock clock;
    clock_start(&clock);
    EXPECT_EQ(0u, clock_advance(&clock, 1.0 / 32.0));
    clock.paused = true;
    EXPECT_EQ(0u, clock_advance(&clock, 5.0));
    EXPECT_EQ(0u, (unsigned)clock.tick);
    /* It goes on from where it stopped: half a tick was owed. */
    clock.paused = false;
    EXPECT_EQ(1u, clock_advance(&clock, 1.0 / 32.0));
}

UTEST(clock, time_that_is_none_or_less_passes_no_tick)
{
    Clock clock;
    clock_start(&clock);
    EXPECT_EQ(0u, clock_advance(&clock, 0.0));
    EXPECT_EQ(0u, clock_advance(&clock, -3.0));
    /* Nor does a time that is not a number. */
    EXPECT_EQ(0u, clock_advance(&clock, (double)NAN));
    EXPECT_EQ(0u, (unsigned)clock.tick);
    EXPECT_EQ(1u, clock_advance(&clock, 1.0 / 16.0));
}

UTEST(clock, between_two_ticks_goes_from_0_towards_1)
{
    Clock clock;
    clock_start(&clock);
    EXPECT_NEAR(0.0, clock_between(&clock), 1e-9);
    clock_advance(&clock, 1.0 / 64.0);
    EXPECT_NEAR(0.25, clock_between(&clock), 1e-9);
    clock_advance(&clock, 1.0 / 32.0);
    EXPECT_NEAR(0.75, clock_between(&clock), 1e-9);
    EXPECT_EQ(1u, clock_advance(&clock, 1.0 / 32.0));
    EXPECT_NEAR(0.25, clock_between(&clock), 1e-9);
    EXPECT_LT(clock_between(&clock), 1.0);
}

UTEST(clock, a_faster_speed_has_more_ticks_in_the_same_time)
{
    Clock clock;
    clock_start(&clock);
    ASSERT_TRUE(clock_set_speed(&clock, 8));
    EXPECT_EQ(8u, clock_advance(&clock, 0.25));
    ASSERT_TRUE(clock_set_speed(&clock, 0));
    EXPECT_EQ(2u, clock_advance(&clock, 0.25));
}

UTEST(clock, changing_the_speed_keeps_the_part_of_a_tick_that_passed)
{
    Clock clock;
    clock_start(&clock);
    clock_advance(&clock, 1.0 / 32.0);
    EXPECT_NEAR(0.5, clock_between(&clock), 1e-9);
    ASSERT_TRUE(clock_set_speed(&clock, 8));
    EXPECT_NEAR(0.5, clock_between(&clock), 1e-9);
    /* Half a tick at 32 a second is a 64th of a second. */
    EXPECT_EQ(1u, clock_advance(&clock, 1.0 / 64.0));
}

UTEST(clock, refuses_a_speed_there_is_not)
{
    Clock clock;
    clock_start(&clock);
    EXPECT_FALSE(clock_set_speed(&clock, CLOCK_SPEEDS));
    EXPECT_EQ(4u, clock.speed);
}
