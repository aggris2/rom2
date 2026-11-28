#include "data/unit_animation.h"

#include <string.h>

#include "utest.h"

/* Reads the idle animation of a definition written as text. */
static bool reads(const char *text, UnitAnimation *out)
{
    TomlTable *top = NULL;
    if (!toml_read((const uint8_t *)text, strlen(text), &top, NULL)) {
        return false;
    }
    bool ok = unit_animation_read(top, "idle", out);
    toml_free(top);
    return ok;
}

/* True if the animation is refused and `out` is left as it was. */
static bool refuses(const char *text)
{
    UnitAnimation animation;
    memset(&animation, 0x5a, sizeof animation);
    UnitAnimation before = animation;
    return !reads(text, &animation)
        && memcmp(&animation, &before, sizeof animation) == 0;
}

UTEST(unit_animation, reads_its_columns_and_the_frame_of_each_tick)
{
    UnitAnimation idle;
    ASSERT_TRUE(reads("id = 70\n[anim.move]\ncolumn = 0\ncolumns = 11\n"
        "frames = [0]\nticks = [2]\n"
        "[anim.idle]\ncolumn = 18\ncolumns = 3\n"
        "frames = [0, 1, 2]\nticks = [2, 1, 1]\n", &idle));
    EXPECT_EQ(18u, idle.column);
    ASSERT_EQ(4u, idle.round.length);
    EXPECT_EQ(0u, sprite_round_frame(&idle.round, 0));
    EXPECT_EQ(0u, sprite_round_frame(&idle.round, 1));
    EXPECT_EQ(1u, sprite_round_frame(&idle.round, 2));
    EXPECT_EQ(2u, sprite_round_frame(&idle.round, 3));
    EXPECT_EQ(0u, sprite_round_frame(&idle.round, 4));
}

UTEST(unit_animation, a_unit_without_it_has_one_of_no_length)
{
    UnitAnimation idle;
    memset(&idle, 0x5a, sizeof idle);
    ASSERT_TRUE(reads("id = 3\n", &idle));
    EXPECT_EQ(0u, idle.round.length);
    EXPECT_EQ(0u, idle.column);
    /* Other animations are not this one. */
    ASSERT_TRUE(reads("[anim.move]\ncolumn = 0\ncolumns = 2\n"
        "frames = [0, 1]\nticks = [2, 2]\n", &idle));
    EXPECT_EQ(0u, idle.round.length);
}

UTEST(unit_animation, refuses_one_that_lacks_a_part)
{
    EXPECT_TRUE(refuses("[anim.idle]\ncolumns = 3\n"
        "frames = [0]\nticks = [2]\n"));
    EXPECT_TRUE(refuses("[anim.idle]\ncolumn = 18\n"
        "frames = [0]\nticks = [2]\n"));
    /* Columns, and nothing to show in them. */
    EXPECT_TRUE(refuses("[anim.idle]\ncolumn = 18\ncolumns = 3\n"));
    EXPECT_TRUE(refuses("[anim.idle]\ncolumn = 18\ncolumns = 3\n"
        "frames = [0]\n"));
}

UTEST(unit_animation, refuses_numbers_that_cannot_be)
{
    EXPECT_TRUE(refuses("[anim.idle]\ncolumn = -1\ncolumns = 3\n"
        "frames = [0]\nticks = [2]\n"));
    EXPECT_TRUE(refuses("[anim.idle]\ncolumn = 18\ncolumns = 0\n"
        "frames = [0]\nticks = [2]\n"));
    /* A frame that is not one of its columns. */
    EXPECT_TRUE(refuses("[anim.idle]\ncolumn = 18\ncolumns = 3\n"
        "frames = [3]\nticks = [2]\n"));
    EXPECT_TRUE(refuses("[anim.idle]\ncolumn = 18\ncolumns = 3\n"
        "frames = [0]\nticks = [0]\n"));
}
