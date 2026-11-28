#include "mechanics/random.h"

#include "utest.h"

UTEST(random, the_same_seed_gives_the_same_run)
{
    Random a;
    Random b;
    random_start(&a, 12345);
    random_start(&b, 12345);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(random_next(&a), random_next(&b));
    }
}

UTEST(random, the_run_is_the_same_on_every_machine)
{
    /* The first numbers from the seed 1, worked out apart from the
     * game from the stirring of the seed and the three shifts: a
     * change to the dice changes every game. */
    Random random;
    random_start(&random, 1);
    EXPECT_EQ(524866043u, random_next(&random));
    EXPECT_EQ(2877414208u, random_next(&random));
    EXPECT_EQ(2380002740u, random_next(&random));
}

UTEST(random, the_first_throws_of_seeds_side_by_side_are_spread)
{
    int counts[6] = {0};
    for (uint32_t seed = 1; seed <= 600; seed++) {
        Random random;
        random_start(&random, seed);
        counts[random_below(&random, 6)]++;
    }
    for (int face = 0; face < 6; face++) {
        EXPECT_GT(counts[face], 70);
        EXPECT_LT(counts[face], 130);
    }
}

UTEST(random, another_seed_gives_another_run)
{
    Random a;
    Random b;
    random_start(&a, 1);
    random_start(&b, 2);
    int same = 0;
    for (int i = 0; i < 100; i++) {
        same += random_next(&a) == random_next(&b);
    }
    EXPECT_EQ(0, same);
}

UTEST(random, a_seed_of_none_still_gives_a_run)
{
    Random random;
    random_start(&random, 0);
    uint32_t first = random_next(&random);
    EXPECT_NE(0u, first);
    EXPECT_NE(first, random_next(&random));
}

UTEST(random, a_number_below_a_limit_is_below_it)
{
    Random random;
    random_start(&random, 99);
    for (int i = 0; i < 1000; i++) {
        EXPECT_LT(random_below(&random, 6), 6u);
    }
    EXPECT_EQ(0u, random_below(&random, 1));
    EXPECT_EQ(0u, random_below(&random, 0));
}

UTEST(random, every_number_below_a_limit_comes_up_about_as_often)
{
    Random random;
    random_start(&random, 2026);
    int counts[6] = {0};
    for (int i = 0; i < 6000; i++) {
        counts[random_below(&random, 6)]++;
    }
    for (int face = 0; face < 6; face++) {
        EXPECT_GT(counts[face], 850);
        EXPECT_LT(counts[face], 1150);
    }
}
