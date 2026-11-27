/* The clock of the game: it turns time that has passed into ticks.
 *
 * Everything in the game happens in ticks, steps of one length. How
 * many pass in a second is the speed of the game, which the player may
 * change. The clock does not look at the time itself: it is told how
 * much has passed, so that it runs the same in a test as in the game.
 */
#ifndef ROM2_MECHANICS_CLOCK_H
#define ROM2_MECHANICS_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

/* The speeds of the game, 0 the slowest. They are the original's, and
 * so is the one a game starts on. */
#define CLOCK_SPEEDS 9
#define CLOCK_SPEED_AT_START 4

/* The most ticks that pass at once. After a long halt, such as a
 * window that was dragged, the game goes on from where it is and does
 * not run through all it missed. */
#define CLOCK_MOST_TICKS 8

typedef struct Clock {
    uint32_t speed;     /* 0 to CLOCK_SPEEDS - 1 */
    bool paused;
    uint64_t tick;      /* how many have passed since the start */
    /* The time that has passed and is not yet a tick, in seconds: less
     * than the length of one tick. */
    double owed;
} Clock;

/* How many ticks pass in a second at a speed, or 0 if there is no such
 * speed. */
uint32_t clock_ticks_a_second(uint32_t speed);

/* Sets a clock to its start: no tick passed, the speed a game starts
 * on, running. */
void clock_start(Clock *clock);

/* Changes the speed. Returns false, and changes nothing, if there is
 * no such speed. The time not yet a tick is kept as the same part of
 * a tick. */
bool clock_set_speed(Clock *clock, uint32_t speed);

/* Tells the clock that `seconds` have passed and returns how many
 * ticks pass with them: at most CLOCK_MOST_TICKS, and none while the
 * clock is paused or if `seconds` is not above 0. */
uint32_t clock_advance(Clock *clock, double seconds);

/* How far the clock is from the tick that passed last to the next one:
 * from 0, just passed, to less than 1. What is drawn glides between
 * two ticks by this. */
double clock_between(const Clock *clock);

#endif
