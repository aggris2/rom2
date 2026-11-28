/* The dice of the game: numbers that look random and are not.
 *
 * Whatever the game leaves to chance, it asks these for. They follow
 * from where they were started and nothing else, so a game that starts
 * the same and is told the same plays out the same, on any machine and
 * any day. The game never asks the machine's own for anything.
 */
#ifndef ROM2_MECHANICS_RANDOM_H
#define ROM2_MECHANICS_RANDOM_H

#include <stdint.h>

/* Where the dice are in their run. It is part of the state of a
 * game. */
typedef struct Random {
    uint32_t state;
} Random;

/* Starts the dice from `seed`. Every seed is good, and each gives its
 * own run. */
void random_start(Random *random, uint32_t seed);

/* The next number of the run, any of the 32-bit numbers. */
uint32_t random_next(Random *random);

/* The next number of the run as one from 0 to `limit` - 1, each about
 * as likely as the other. With a `limit` of 0 it is 0. */
uint32_t random_below(Random *random, uint32_t limit);

#endif
