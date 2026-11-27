#include "mechanics/clock.h"

static const uint32_t TICKS_A_SECOND[CLOCK_SPEEDS] = {
    8, 10, 12, 14, 16, 20, 24, 28, 32
};

uint32_t clock_ticks_a_second(uint32_t speed)
{
    return speed < CLOCK_SPEEDS ? TICKS_A_SECOND[speed] : 0;
}

void clock_start(Clock *clock)
{
    clock->speed = CLOCK_SPEED_AT_START;
    clock->paused = false;
    clock->tick = 0;
    clock->owed = 0.0;
}

bool clock_set_speed(Clock *clock, uint32_t speed)
{
    if (speed >= CLOCK_SPEEDS) {
        return false;
    }
    clock->owed = clock->owed * TICKS_A_SECOND[clock->speed]
        / TICKS_A_SECOND[speed];
    clock->speed = speed;
    return true;
}

uint32_t clock_advance(Clock *clock, double seconds)
{
    /* The second test is false for a number that is not one, too. */
    if (clock->paused || !(seconds > 0.0)) {
        return 0;
    }
    double a_second = TICKS_A_SECOND[clock->speed];
    double ticks = (clock->owed + seconds) * a_second;
    if (ticks >= CLOCK_MOST_TICKS) {
        clock->owed = 0.0;
        clock->tick += CLOCK_MOST_TICKS;
        return CLOCK_MOST_TICKS;
    }
    uint32_t whole = (uint32_t)ticks;
    clock->owed = (ticks - whole) / a_second;
    clock->tick += whole;
    return whole;
}

double clock_between(const Clock *clock)
{
    double part = clock->owed * TICKS_A_SECOND[clock->speed];
    return part < 0.0 ? 0.0 : part >= 1.0 ? 0.999999 : part;
}
