#include "import/name.h"

#include <string.h>

#include "utest.h"

/* ---- name_copy ---- */

UTEST(name, copy_stops_at_the_first_zero)
{
    /* What follows the zero is leftover and not part of the name. */
    const uint8_t field[8] = {'p', 'i', 'n', 'e', '1', 0, 'x', 'y'};
    char name[16];
    memset(name, 'z', sizeof name);

    name_copy(name, sizeof name, field, sizeof field);

    ASSERT_STREQ("pine1", name);
}

UTEST(name, copy_takes_the_whole_field_when_it_has_no_zero)
{
    const uint8_t field[4] = {'t', 'r', 'e', 'e'};
    char name[16];
    memset(name, 'z', sizeof name);

    name_copy(name, sizeof name, field, sizeof field);

    ASSERT_STREQ("tree", name);
}

UTEST(name, copy_fits_a_full_field_into_a_buffer_one_larger)
{
    /* 16 characters, and 17 bytes to put them in: the case the archive
     * and registry readers rely on. */
    const uint8_t field[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    char name[17];
    memset(name, 'z', sizeof name);

    name_copy(name, sizeof name, field, sizeof field);

    ASSERT_STREQ("0123456789abcdef", name);
}

UTEST(name, copy_cuts_a_name_that_is_too_long_for_the_buffer)
{
    /* Room for 3 characters and the closing zero. The byte after the
     * buffer must not be touched. */
    const uint8_t field[8] = {'p', 'i', 'n', 'e', '1', 0, 0, 0};
    char name[5] = {'z', 'z', 'z', 'z', 'G'};

    name_copy(name, 4, field, sizeof field);

    ASSERT_STREQ("pin", name);
    ASSERT_EQ('G', name[4]);
}

UTEST(name, copy_of_an_empty_field_gives_an_empty_name)
{
    const uint8_t field[4] = {0, 'a', 'b', 'c'};
    char name[8];
    memset(name, 'z', sizeof name);

    name_copy(name, sizeof name, field, sizeof field);
    ASSERT_STREQ("", name);

    memset(name, 'z', sizeof name);
    name_copy(name, sizeof name, field, 0);
    ASSERT_STREQ("", name);
}

UTEST(name, copy_into_a_buffer_of_no_size_writes_nothing)
{
    const uint8_t field[4] = {'t', 'r', 'e', 'e'};
    char name[2] = {'G', 'G'};

    name_copy(name, 0, field, sizeof field);

    ASSERT_EQ('G', name[0]);
    ASSERT_EQ('G', name[1]);
}

/* ---- name_folder ---- */

UTEST(name, folder_is_the_id_in_three_digits_and_the_name)
{
    char out[NAME_FOLDER_SIZE];
    ASSERT_TRUE(name_folder(3, "Human Swordsman", out));
    ASSERT_STREQ("003_human_swordsman", out);
    ASSERT_TRUE(name_folder(0, "Pine 1", out));
    ASSERT_STREQ("000_pine_1", out);
    ASSERT_TRUE(name_folder(999, "x", out));
    ASSERT_STREQ("999_x", out);
}

UTEST(name, folder_turns_runs_of_other_characters_into_one_underscore)
{
    char out[NAME_FOLDER_SIZE];
    ASSERT_TRUE(name_folder(66, " Ogre  (Lord) 2-handed! ", out));
    ASSERT_STREQ("066_ogre_lord_2_handed", out);
    ASSERT_TRUE(name_folder(7, "", out));
    ASSERT_STREQ("007", out);
    ASSERT_TRUE(name_folder(7, "!!!", out));
    ASSERT_STREQ("007", out);
}

UTEST(name, folder_cuts_a_name_that_is_too_long)
{
    char text[200];
    char out[NAME_FOLDER_SIZE + 1];
    memset(text, 'a', sizeof text - 1);
    text[sizeof text - 1] = '\0';
    memset(out, 'x', sizeof out);

    ASSERT_TRUE(name_folder(5, text, out));

    ASSERT_EQ((size_t)NAME_FOLDER_SIZE - 1, strlen(out));
    ASSERT_EQ('x', out[NAME_FOLDER_SIZE]);
}

UTEST(name, folder_refuses_an_id_outside_0_to_999)
{
    char out[NAME_FOLDER_SIZE] = "untouched";
    ASSERT_FALSE(name_folder(-1, "a", out));
    ASSERT_FALSE(name_folder(1000, "a", out));
    ASSERT_STREQ("untouched", out);
}
