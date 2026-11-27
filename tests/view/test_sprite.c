#include "view/sprite.h"

#include <string.h>

#include "utest.h"

UTEST(sprite, a_row_nearer_the_top_comes_before_the_one_below_it)
{
    EXPECT_LT(sprite_order(0, 3), sprite_order(255, 4));
    EXPECT_LT(sprite_order(255, 0), sprite_order(0, 1));
}

UTEST(sprite, in_a_row_what_is_further_right_comes_first)
{
    EXPECT_LT(sprite_order(9, 3), sprite_order(8, 3));
    EXPECT_LT(sprite_order(255, 255), sprite_order(0, 255));
}

/* Sorts sprites that have these turns, and names each by the place it
 * had before in `x`. */
static void sort(const uint32_t *orders, size_t count, Sprite *sprites)
{
    Sprite spare[16];
    memset(sprites, 0, count * sizeof *sprites);
    for (size_t i = 0; i < count; i++) {
        sprites[i].order = orders[i];
        sprites[i].x = (int16_t)i;
    }
    sprites_sort(sprites, count, spare);
}

UTEST(sprite, sorting_puts_sprites_in_the_order_of_their_turns)
{
    const uint32_t orders[7] = {50, 10, 70, 20, 60, 40, 30};
    Sprite sprites[7];
    sort(orders, 7, sprites);
    for (size_t i = 0; i < 7; i++) {
        EXPECT_EQ((i + 1) * 10, sprites[i].order);
    }
    EXPECT_EQ(1, sprites[0].x);
    EXPECT_EQ(2, sprites[6].x);
}

UTEST(sprite, sprites_with_one_turn_stay_as_they_were_to_each_other)
{
    const uint32_t orders[9] = {5, 3, 5, 3, 5, 1, 3, 5, 3};
    Sprite sprites[9];
    sort(orders, 9, sprites);
    const int16_t was[9] = {5, 1, 3, 6, 8, 0, 2, 4, 7};
    for (size_t i = 0; i < 9; i++) {
        EXPECT_EQ(was[i], sprites[i].x);
    }
}

UTEST(sprite, sorting_none_or_one_changes_nothing)
{
    const uint32_t orders[1] = {7};
    Sprite sprites[1];
    sort(orders, 1, sprites);
    EXPECT_EQ(7u, sprites[0].order);
    sprites_sort(sprites, 0, sprites);
    EXPECT_EQ(7u, sprites[0].order);
}

UTEST(sprite, sorting_any_number_of_sprites_leaves_them_in_order)
{
    for (size_t count = 2; count <= 16; count++) {
        uint32_t orders[16];
        Sprite sprites[16];
        for (size_t i = 0; i < count; i++) {
            orders[i] = (uint32_t)((i * 7 + count) % 5);
        }
        sort(orders, count, sprites);
        for (size_t i = 1; i < count; i++) {
            EXPECT_LE(sprites[i - 1].order, sprites[i].order);
            if (sprites[i - 1].order == sprites[i].order) {
                EXPECT_LT(sprites[i - 1].x, sprites[i].x);
            }
        }
    }
}
