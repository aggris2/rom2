#include "mechanics/random.h"

/* What a seed of 0 starts from: the run below cannot leave 0. */
#define SEED_FOR_NONE 0x9e3779b9u

void random_start(Random *random, uint32_t seed)
{
    random->state = seed != 0 ? seed : SEED_FOR_NONE;
}

uint32_t random_next(Random *random)
{
    /* Marsaglia's xorshift, the one of 32 bits with the shifts 13, 17
     * and 5: it goes through every number but 0 before it repeats. */
    uint32_t x = random->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    random->state = x;
    return x;
}

uint32_t random_below(Random *random, uint32_t limit)
{
    return (uint32_t)(((uint64_t)random_next(random) * limit) >> 32);
}
